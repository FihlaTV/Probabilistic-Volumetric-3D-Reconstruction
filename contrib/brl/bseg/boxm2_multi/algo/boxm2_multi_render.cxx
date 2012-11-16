#include "boxm2_multi_render.h"
#include <vcl_algorithm.h>
#include <boxm2/boxm2_scene.h>
#include <boxm2/boxm2_util.h>
#include <bocl/bocl_manager.h>
#include <boxm2/ocl/boxm2_ocl_util.h>
#include <boxm2/ocl/boxm2_opencl_cache.h>
#include <boxm2/ocl/algo/boxm2_ocl_camera_converter.h>

#include <bocl/bocl_mem.h>
#include <bocl/bocl_device.h>
#include <bocl/bocl_kernel.h>
#include <brdb/brdb_value.h>
#include <brdb/brdb_selection.h>
#include <bprb/bprb_batch_process_manager.h>
#include <bprb/bprb_parameters.h>
#include <bprb/bprb_macros.h>
#include <bprb/bprb_func_process.h>
#include <vil/vil_image_view.h>
#include <vpgl/vpgl_perspective_camera.h>
#include <vul/vul_timer.h>

float boxm2_multi_render::render(boxm2_multi_cache&      cache,
                                 vil_image_view<float>&  img,
                                 vpgl_camera_double_sptr cam)
{
  vcl_cout<<"------------ boxm2_multi_render -----------------------"<<vcl_endl;
  //verify appearance model
  vul_timer rtime; rtime.mark();
  vcl_size_t lthreads[2] = {8,8};
  vcl_string data_type, options;
  int apptypesize;
  if ( !get_scene_appearances(cache.get_scene(), data_type, options, apptypesize) )
    return 0.0f;

  //setup image size
  int ni=img.ni(),
      nj=img.nj();
  unsigned cl_ni=RoundUp(ni,lthreads[0]);
  unsigned cl_nj=RoundUp(nj,lthreads[1]);

  //setup output images
  vil_image_view<float> vis_out(ni,nj);
  vis_out.fill(1.0f);
  img.fill(0.0f);

#if 1 //Using first block method

  //set up image, command queue lists
  vcl_vector<cl_command_queue> queues;
  vcl_vector<bocl_mem_sptr> exp_mems, vis_mems, img_dims, outputs,
                            ray_os, ray_ds, lookups;
  vcl_vector<vcl_vector<boxm2_block_id> > vis_orders;
  vcl_size_t maxBlocks = 0;
  vcl_vector<boxm2_opencl_cache*>& ocl_caches = cache.ocl_caches();
  for (unsigned int i=0; i<ocl_caches.size(); ++i)
  {
    //grab sub scene and it's cache
    boxm2_opencl_cache*     ocl_cache = ocl_caches[i];
    boxm2_scene_sptr        sub_scene = ocl_cache->get_scene();
    bocl_device_sptr        device    = ocl_cache->get_device();

    // create a command queue.
    int status=0;
    cl_command_queue queue = clCreateCommandQueue( device->context(),
                                                   *(device->device_id()),
                                                   CL_QUEUE_PROFILING_ENABLE,
                                                   &status );
    queues.push_back(queue);
    if (status!=0) {
      vcl_cout<<"boxm2_multi_render::render unalbe to create command queue"<<vcl_endl;
      return 0.0f;
    }

    //create image dim buff
    int img_dim_buff[4] = {0, 0, ni, nj};
    bocl_mem_sptr exp_img_dim=new bocl_mem(device->context(), img_dim_buff, sizeof(int)*4, "image dims");
    exp_img_dim->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
    img_dims.push_back(exp_img_dim);

    //create exp image (TODO, make these patches to save mem)
    float* exp_buff = new float[cl_ni*cl_nj];
    vcl_fill(exp_buff, exp_buff+cl_ni*cl_nj, 0.0f);
    bocl_mem_sptr exp_image = ocl_cache->alloc_mem(cl_ni*cl_nj*sizeof(float), exp_buff, "exp image buffer");
    exp_image->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
    exp_mems.push_back(exp_image);

    // visibility image
    float* vis_buff = new float[cl_ni*cl_nj];
    vcl_fill(vis_buff, vis_buff + cl_ni*cl_nj, 1.0f);
    bocl_mem_sptr vis_image = ocl_cache->alloc_mem(cl_ni*cl_nj*sizeof(float), vis_buff, "exp image buffer");
    vis_image->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
    vis_mems.push_back(vis_image);

    // Output Array
    float* output_arr = new float[cl_ni*cl_nj];
    vcl_fill(output_arr, output_arr+cl_ni*cl_nj, 0.0f);
    bocl_mem_sptr  cl_output = ocl_cache->alloc_mem(sizeof(float)*cl_ni*cl_nj, output_arr, "output buffer");
    cl_output->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
    outputs.push_back(cl_output);
    //set generic cam and get visible block order
    cl_float* ray_origins    = new cl_float[4*cl_ni*cl_nj];
    cl_float* ray_directions = new cl_float[4*cl_ni*cl_nj];
    bocl_mem_sptr ray_o_buff = ocl_cache->alloc_mem(cl_ni*cl_nj * sizeof(cl_float4), ray_origins, "ray_origins buffer");
    bocl_mem_sptr ray_d_buff = ocl_cache->alloc_mem(cl_ni*cl_nj * sizeof(cl_float4), ray_directions, "ray_directions buffer");
    boxm2_ocl_camera_converter::compute_ray_image( device, queue, cam, cl_ni, cl_nj, ray_o_buff, ray_d_buff);
    ray_os.push_back(ray_o_buff);
    ray_ds.push_back(ray_d_buff);

    // bit lookup buffer
    cl_uchar lookup_arr[256];
    boxm2_ocl_util::set_bit_lookup(lookup_arr);
    bocl_mem_sptr lookup=new bocl_mem(device->context(), lookup_arr, sizeof(cl_uchar)*256, "bit lookup buffer");
    lookup->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);
    lookups.push_back(lookup);

    //grab visibility order
    vcl_vector<boxm2_block_id> vis_order = sub_scene->get_vis_blocks(cam);
    vis_orders.push_back(vis_order);
    maxBlocks = vcl_max(vis_order.size(), maxBlocks);
  }

  //visibility order
  vcl_vector<boxm2_multi_cache_group*> grp = cache.get_vis_groups(cam);
  float cpu_time = 0.0f, gpu_time = 0.0f;
  vul_timer gpu_timer; gpu_timer.mark();

  //--------------------------------------------------
  //run block-wise ray trace for each block/device
  //--------------------------------------------------
  //go through each scene's blocks in vis order
  for (unsigned int grpId=0; grpId<grp.size(); ++grpId) {
    boxm2_multi_cache_group& group = *grp[grpId];
    vcl_vector<boxm2_block_id>& ids = group.ids();
    vcl_vector<int> indices = group.order_from_cam(cam);
    for (unsigned int idx=0; idx<indices.size(); ++idx) {
      int i = indices[idx];
      boxm2_opencl_cache* ocl_cache = ocl_caches[i];
      boxm2_scene_sptr    sub_scene = ocl_cache->get_scene();
      bocl_device_sptr    device    = ocl_cache->get_device();
      boxm2_block_id id = ids[i];

      //keep track of mems allocated, so you can unref them later
      vis_mems[i]->fill(queues[i], 1.0f, "float");
      exp_mems[i]->zero_gpu_buffer(queues[i]);
      vcl_vector<bocl_kernel*>& kerns = get_kernels(device, options);
      this->render_block(sub_scene, id, ocl_cache, queues[i],
                         ray_os[i], ray_ds[i], exp_mems[i], vis_mems[i], img_dims[i],
                         outputs[i], lookups[i], data_type, kerns[0],
                         lthreads, cl_ni, cl_nj, apptypesize);
    }

    //finish queues before moving on
    for (unsigned int idx=0; idx<indices.size(); ++idx) {
      int i = indices[idx];

    //Figure out image location
#if 1
      vul_timer cpu_timer; cpu_timer.mark();
      double minU=ni, minV=nj,
             maxU=0, maxV=0;
      vgl_box_3d<double>& blkBox = group.bbox(i);
      vcl_vector<vgl_point_3d<double> > verts = blkBox.vertices();
      for (unsigned int vi=0; vi<verts.size(); ++vi) {
        double u, v;
        cam->project(verts[vi].x(), verts[vi].y(), verts[vi].z(), u, v);
        if (u < minU) minU = u;
        if (u > maxU) maxU = u;
        if (v < minV) minV = v;
        if (v > maxV) maxV = v;
      }
      //make sure you clamp the value between
      minU = clamp(minU, 0.0, (double) ni);
      maxU = clamp(maxU, 0.0, (double) ni);
      maxV = clamp(maxV, 0.0, (double) nj);
      minV = clamp(minV, 0.0, (double) nj);
      cpu_time += (float) cpu_timer.all();
#else
      double minU = 0, minV = 0,
             maxU = ni, maxV = nj;
#endif
      clFinish(queues[i]);
      cpu_timer.mark();
      exp_mems[i]->read_to_buffer(queues[i]);
      vis_mems[i]->read_to_buffer(queues[i]);

      //set the vis and exp images
      float* v = (float*) vis_mems[i]->cpu_buffer();
      float* e = (float*) exp_mems[i]->cpu_buffer();
      float* imgbuff = img.top_left_ptr();
      float* visbuff = vis_out.top_left_ptr();
      for (int jj=minV; jj<maxV; ++jj)
        for (int ii=minU; ii<maxU; ++ii) {
          int imIdx = jj*ni + ii;
          imgbuff[imIdx] += e[imIdx] * visbuff[imIdx];
          visbuff[imIdx] *= v[imIdx];
        }

      //record cpu time and finally GPU time
      cpu_time += (float)cpu_timer.all();
    }
  }
  //actual GPU time
  gpu_time = (float) gpu_timer.all();
  gpu_time -= cpu_time;

#if 0
  //normalize
  float* imgbuff = img.top_left_ptr();
  float* visbuff = vis_out.top_left_ptr();
  for (unsigned int i=0; i<vis_out.size(); ++i)
    imgbuff[i] += visbuff[i]*.5f;
#endif

  //clean up all ocl buffers
  for (unsigned int i=0; i<ocl_caches.size(); ++i)
  {
    //grab sub scene and it's cache
    boxm2_opencl_cache*     ocl_cache = ocl_caches[i];
    boxm2_scene_sptr        sub_scene = ocl_cache->get_scene();
    bocl_device_sptr        device    = ocl_cache->get_device();

    //release queue
    clReleaseCommandQueue(queues[i]);

    //clear exp images
    float* v = (float*) vis_mems[i]->cpu_buffer();
    float* e = (float*) exp_mems[i]->cpu_buffer();
    delete[] v;
    delete[] e;
    ocl_cache->unref_mem(exp_mems[i].ptr());
    ocl_cache->unref_mem(vis_mems[i].ptr());

    //clear ray mems
    float* ro = (float*) ray_os[i]->cpu_buffer();
    float* rd = (float*) ray_ds[i]->cpu_buffer();
    delete[] ro;
    delete[] rd;
    ocl_cache->unref_mem(ray_os[i].ptr());
    ocl_cache->unref_mem(ray_ds[i].ptr());

    //clear output
    float* out = (float*) outputs[i]->cpu_buffer();
    delete[] out;
    ocl_cache->unref_mem(outputs[i].ptr());
  }

  //--------------------------------
  //report times
  //--------------------------------
  float wall_time = (float) rtime.all();
  vcl_cout<<"\nMulti Render Time: "<< wall_time <<" ms\n"
          <<"  cpu_time: "<<wall_time-gpu_time<<" ms\n"
          <<"  gpu_time: "<<gpu_time<<" ms"<<vcl_endl;
  return (float) gpu_time;

#endif //Using first block method
#if 0 //Using second block method (commented out)

  //set up image lists
  vcl_vector<cl_command_queue> queues;
  vcl_vector<bocl_mem_sptr> exp_mems, vis_mems;

  //for each device/cache, run a render
  vcl_vector<boxm2_opencl_cache*> ocl_caches = cache.get_vis_sub_scenes(cam.ptr());
  for (unsigned int i=0; i<ocl_caches.size(); ++i)
  {
    //grab sub scene and it's cache
    boxm2_opencl_cache*     ocl_cache = ocl_caches[i];
    boxm2_scene_sptr        sub_scene = ocl_cache->get_scene();
    bocl_device_sptr        device    = ocl_cache->get_device();

    // compile the kernel/retrieve cached kernel for this device
    vcl_vector<bocl_kernel*> kerns = get_kernels(device, options);

    // create a command queue.
    int status=0;
    cl_command_queue queue = clCreateCommandQueue( device->context(),
                                                   *(device->device_id()),
                                                   CL_QUEUE_PROFILING_ENABLE,
                                                   &status );
    queues.push_back(queue);
    if (status!=0) {
      vcl_cout<<"boxm2_multi_render::render unalbe to create command queue"<<vcl_endl;
      return 0.0f;
    }

    //create image and workspace size
    unsigned cl_ni=RoundUp(ni,lthreads[0]);
    unsigned cl_nj=RoundUp(nj,lthreads[1]);
    float* buff = new float[cl_ni*cl_nj];
    vcl_fill(buff, buff+cl_ni*cl_nj, 0.0f);
    bocl_mem_sptr exp_image=new bocl_mem(device->context(),buff,cl_ni*cl_nj*sizeof(float),"exp image buffer");
    exp_image->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
    exp_mems.push_back(exp_image);

    //create image dim buff
    int img_dim_buff[4] = {0, 0, ni, nj};
    bocl_mem_sptr exp_img_dim=new bocl_mem(device->context(), img_dim_buff, sizeof(int)*4, "image dims");
    exp_img_dim->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

    // visibility image
    float* vis_buff = new float[cl_ni*cl_nj];
    vcl_fill(vis_buff, vis_buff + cl_ni*cl_nj, 1.0f);
    bocl_mem_sptr vis_image = new bocl_mem(device->context(), vis_buff, cl_ni*cl_nj*sizeof(float), "exp image buffer");
    vis_image->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);
    vis_mems.push_back(vis_image);

    // run expected image function
    vcl_cout<<"  rendering with device: "<<device<<vcl_endl;
    this->render_scene( sub_scene,
                        device,
                        ocl_cache,
                        queue,
                        cam,
                        exp_image,
                        vis_image,
                        exp_img_dim,
                        data_type,
                        kerns[0],
                        lthreads,
                        cl_ni,
                        cl_nj,
                        apptypesize );
  }

  //finish execution along each queue
  for (unsigned int i=0; i<queues.size(); ++i)
    clFinish(queues[i]);

  //read all images in
  vcl_vector<float*> exp_imgs, vis_imgs;
  for (unsigned int i=0; i<exp_mems.size(); ++i) {
    // read out expected image
    exp_mems[i]->read_to_buffer(queues[i]);
    vis_mems[i]->read_to_buffer(queues[i]);

    //populate vil_image_views for combination
    exp_imgs.push_back( (float*) exp_mems[i]->cpu_buffer());
    vis_imgs.push_back( (float*) vis_mems[i]->cpu_buffer());
  }

  //------------------------------------------------------------------------
  //combine images
  float pre_time = (float) rtime.all(); rtime.mark();
  vil_image_view<float> vis_out(ni,nj);
  vis_out.fill(1.0f);
  img.fill(0.0f);
  for (unsigned int idx=0; idx<exp_imgs.size(); ++idx) {
    float* v = vis_imgs[idx];
    float* e = exp_imgs[idx];
    int c=0;
    for (int j=0; j<nj; ++j) {
      for (int i=0; i<ni; ++i) {
        img(i,j)     += e[c] * vis_out(i,j);
        vis_out(i,j) *= v[c];
        ++c;
      }
    }

    //delete image buffers
    delete[] v;
    delete[] e;
  }
  float combine_time = (float) rtime.all();
  vcl_cout<<"\nMulti Render Time: "<<combine_time + pre_time<<" ms\n"
          <<"  pre_time (gpu+overhead): "<<pre_time<<" ms\n"
          <<"  combine time (end):      "<<combine_time<<" ms"<<vcl_endl;
  return (float) combine_time+pre_time;
#endif //Using second block method (commented out)
}

float boxm2_multi_render::render_scene( boxm2_scene_sptr scene,
                                        bocl_device_sptr device,
                                        boxm2_opencl_cache* opencl_cache,
                                        cl_command_queue & queue,
                                        vpgl_camera_double_sptr & cam,
                                        bocl_mem_sptr & exp_image,
                                        bocl_mem_sptr & vis_image,
                                        bocl_mem_sptr & exp_img_dim,
                                        vcl_string data_type,
                                        bocl_kernel* kernel,
                                        vcl_size_t * lthreads,
                                        unsigned cl_ni,
                                        unsigned cl_nj,
                                        int apptypesize )
{
    float transfer_time=0.0f;
    float gpu_time=0.0f;

    //camera check
    if (cam->type_name()!= "vpgl_perspective_camera" &&
        cam->type_name()!= "vpgl_generic_camera" ) {
      vcl_cout<<"Cannot render with camera of type "<<cam->type_name()<<vcl_endl;
      return 0.0f;
    }

    //set generic cam and get visible block order
    cl_float* ray_origins    = new cl_float[4*cl_ni*cl_nj];
    cl_float* ray_directions = new cl_float[4*cl_ni*cl_nj];
    bocl_mem_sptr ray_o_buff = new bocl_mem(device->context(), ray_origins   ,  cl_ni*cl_nj * sizeof(cl_float4), "ray_origins buffer");
    bocl_mem_sptr ray_d_buff = new bocl_mem(device->context(), ray_directions,  cl_ni*cl_nj * sizeof(cl_float4), "ray_directions buffer");
    boxm2_ocl_camera_converter::compute_ray_image( device, queue, cam, cl_ni, cl_nj, ray_o_buff, ray_d_buff);

    // Output Array
    float output_arr[100];
    for (int i=0; i<100; ++i) output_arr[i] = 0.0f;
    bocl_mem_sptr  cl_output=new bocl_mem(device->context(), output_arr, sizeof(float)*100, "output buffer");
    cl_output->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR);

    // bit lookup buffer
    cl_uchar lookup_arr[256];
    boxm2_ocl_util::set_bit_lookup(lookup_arr);
    bocl_mem_sptr lookup=new bocl_mem(device->context(), lookup_arr, sizeof(cl_uchar)*256, "bit lookup buffer");
    lookup->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);

    // set arguments
    vcl_vector<boxm2_block_id> vis_order = scene->get_vis_blocks(cam);
    vcl_vector<boxm2_block_id>::iterator id;
    for (id = vis_order.begin(); id != vis_order.end(); ++id)
    {
        vcl_cout<<(*id);
        render_block(scene, *id, opencl_cache, queue,  ray_o_buff, ray_d_buff,
                     exp_image, vis_image, exp_img_dim, cl_output, lookup, data_type, kernel,
                     lthreads, cl_ni, cl_nj, apptypesize);
    }

    //clean up cam
    delete[] ray_origins;
    delete[] ray_directions;
    return gpu_time + transfer_time;
}

float boxm2_multi_render::render_block( boxm2_scene_sptr& scene,
                                        boxm2_block_id id,
                                        boxm2_opencl_cache* opencl_cache,
                                        cl_command_queue& queue,
                                        bocl_mem_sptr & ray_o_buff,
                                        bocl_mem_sptr & ray_d_buff,
                                        bocl_mem_sptr & exp_image,
                                        bocl_mem_sptr & vis_image,
                                        bocl_mem_sptr & exp_img_dim,
                                        bocl_mem_sptr & cl_output,
                                        bocl_mem_sptr & lookup,
                                        vcl_string data_type,
                                        bocl_kernel* kern,
                                        vcl_size_t* lthreads,
                                        unsigned cl_ni,
                                        unsigned cl_nj,
                                        int apptypesize)
{
    //choose correct render kernel
    boxm2_block_metadata mdata = scene->get_block_metadata(id);
    vcl_size_t gThreads[] = {cl_ni,cl_nj};

    //write the image values to the buffer
    vul_timer transfer;
    bocl_mem* blk       = opencl_cache->get_block(id);
    bocl_mem* blk_info  = opencl_cache->loaded_block_info();
    bocl_mem* alpha     = opencl_cache->get_data<BOXM2_ALPHA>(id);
    int alphaTypeSize   = (int)boxm2_data_info::datasize(boxm2_data_traits<BOXM2_ALPHA>::prefix());
    // data type string may contain an identifier so determine the buffer size
    bocl_mem* mog       = opencl_cache->get_data(id,data_type,alpha->num_bytes()/alphaTypeSize*apptypesize,true);
    float transfer_time = (float) transfer.all();

    ////3. SET args
    kern->set_arg( blk_info );
    kern->set_arg( blk );
    kern->set_arg( alpha );
    kern->set_arg( mog );
    kern->set_arg( ray_o_buff.ptr() );
    kern->set_arg( ray_d_buff.ptr() );
    kern->set_arg( exp_image.ptr() );
    kern->set_arg( exp_img_dim.ptr());
    kern->set_arg( cl_output.ptr() );
    kern->set_arg( lookup.ptr() );
    kern->set_arg( vis_image.ptr() );

    //local tree , cumsum buffer, imindex buffer
    kern->set_local_arg( lthreads[0]*lthreads[1]*sizeof(cl_uchar16) );
    kern->set_local_arg( lthreads[0]*lthreads[1]*10*sizeof(cl_uchar) );
    kern->set_local_arg( lthreads[0]*lthreads[1]*sizeof(cl_int) );

    //execute kernel
    kern->execute(queue, 2, lthreads, gThreads);

    //clear render kernel args so it can reset em on next execution
    kern->clear_args();
    return transfer_time;
}


//multi_render compile
vcl_vector<bocl_kernel*>&
boxm2_multi_render::get_kernels(bocl_device_sptr device, vcl_string opts)
{
  // check to see if this device has compiled kernels already
  vcl_string identifier = device->device_identifier()+opts;
  if (kernels_.find(identifier) != kernels_.end())
    return kernels_[identifier];

  //if not found, then compile and cache
  vcl_cout<<"===========Compiling multi render kernels===========\n"
          <<"  for device: "<<device->device_identifier()<<vcl_endl;

  //gather all render sources... seems like a lot for rendering...
  vcl_vector<vcl_string> src_paths;
  vcl_string source_dir = boxm2_ocl_util::ocl_src_root();
  src_paths.push_back(source_dir + "scene_info.cl");
  src_paths.push_back(source_dir + "pixel_conversion.cl");
  src_paths.push_back(source_dir + "bit/bit_tree_library_functions.cl");
  src_paths.push_back(source_dir + "backproject.cl");
  src_paths.push_back(source_dir + "statistics_library_functions.cl");
  src_paths.push_back(source_dir + "expected_functor.cl");
  src_paths.push_back(source_dir + "ray_bundle_library_opt.cl");
  src_paths.push_back(source_dir + "bit/render_bit_scene.cl");
  src_paths.push_back(source_dir + "bit/cast_ray_bit.cl");

  //set kernel options
  //#define STEP_CELL step_cell_render(mixture_array, alpha_array, data_ptr, d, &vis, &expected_int);
  vcl_string options = opts + " -D RENDER ";
  options += " -D DETERMINISTIC ";
  options += " -D STEP_CELL=step_cell_render(aux_args.mog,aux_args.alpha,data_ptr,d*linfo->block_len,vis,aux_args.expint)";

  //have kernel construct itself using the context and device
  bocl_kernel * ray_trace_kernel=new bocl_kernel();
  ray_trace_kernel->create_kernel( &device->context(),
                                   device->device_id(),
                                   src_paths,
                                   "render_bit_scene",   //kernel name
                                   options,              //options
                                   "boxm2 opencl render_bit_scene"); //kernel identifier (for error checking)

  //create normalize image kernel
  vcl_vector<vcl_string> norm_src_paths;
  norm_src_paths.push_back(source_dir + "pixel_conversion.cl");
  norm_src_paths.push_back(source_dir + "bit/normalize_kernels.cl");
  bocl_kernel * normalize_render_kernel=new bocl_kernel();
  normalize_render_kernel->create_kernel( &device->context(),
                                          device->device_id(),
                                          norm_src_paths,
                                          "normalize_render_kernel",   //kernel name
                                          options,              //options
                                          "normalize render kernel"); //kernel identifier (for error checking)

  //store list
  vcl_vector<bocl_kernel*> kerns(2);
  kerns[0] = ray_trace_kernel;
  kerns[1] = normalize_render_kernel;

  //cache in map and return
  this->kernels_[identifier] = kerns;
  return kernels_[identifier];
}


//pick out data type
bool boxm2_multi_render::get_scene_appearances(boxm2_scene_sptr    scene,
                                               vcl_string&         data_type,
                                               vcl_string&         options,
                                               int&                apptypesize)
{
  bool foundDataType = false;
  vcl_vector<vcl_string> apps = scene->appearances();
  apptypesize = 0;
  for (unsigned int i=0; i<apps.size(); ++i) {
    if ( apps[i] == boxm2_data_traits<BOXM2_MOG3_GREY>::prefix() )
    {
      data_type = apps[i];
      foundDataType = true;
      options=" -D MOG_TYPE_8 ";
      apptypesize = boxm2_data_traits<BOXM2_MOG3_GREY>::datasize();
    }
    else if ( apps[i] == boxm2_data_traits<BOXM2_MOG3_GREY_16>::prefix() )
    {
      data_type = apps[i];
      foundDataType = true;
      options=" -D MOG_TYPE_16 ";
      apptypesize = boxm2_data_traits<BOXM2_MOG3_GREY_16>::datasize();
    }
  }
  if (!foundDataType) {
    vcl_cout<<"BOXM2_OCL_RENDER_PROCESS ERROR: scene doesn't have BOXM2_MOG3_GREY or BOXM2_MOG3_GREY_16 data type"<<vcl_endl;
    return false;
  }
  return true;
}
