import optparse
from xml.etree.ElementTree import ElementTree
import os, sys
import boxm2_batch
import math

class dbvalue:
    def __init__(self, index, type):
        self.id = index    # unsigned integer
        self.type = type   # string



def writeSceneFromBox(data_path, resolution, min_pt, max_pt, ntrees_x=64,ntrees_y=64,ntrees_z=64, max_num_lvls=4, appearance_model1 = "boxm2_mog3_grey", appearance_model2 = "boxm2_num_obs", appearance_model3 = "boxm2_sum_log_msg_pos", p_init=0.001,max_data_size=1500.0):
    """A function that takes the minimum and maximum points of a bounding box for the scene in real world coordinates
    and partitions the space into the appropriate number of boxes given a user specified number of trees.

    min_pt              : A python list specifying the 3d position of the minimum corner of the box.
                             This is interpreted by boxm2 as the 3d origin of the scene.
    max-pt              : A python list specifying the 3d position of the maximum corner of the box.
    ntrees              : Number of trees in all dimensions (the python interface only supports symmetric trees thus far)
    max_num_lvls            : Maximum number of levels in the trees (a tree will have pow(2,max_num_lvls-1) possible cells)
    appearance_model     : A string indicating desired appearance model
    occupancy_model      : A string indicating desired occupancy model
    max_data_size        : Maximum Sizer of a block in megabytes. Determined by GPU memory. Recomment 650MB for 1GB card and 1.1GB for 1.5GM card
    maximum_data_size    : Maximum memory allowable for a superblock. The max_data_size is determined by GPU memory size.
                             For a 1 GB card, a maximum data size of 650MB is recommended.
                             For a 1.5GB card, a 1.1G maximum data size of 1.1GB is recommended.
    p_init               : Initial occupancy probability."""


    boxm2_batch.register_processes();
    boxm2_batch.register_datatypes();

    tree_size=resolution*pow(2,max_num_lvls-1);

    block_size_x=ntrees_x*tree_size;
    block_size_y=ntrees_y*tree_size;
    block_size_z=ntrees_z*tree_size;

    xsize=max_pt[0]-min_pt[0];
    ysize=max_pt[1]-min_pt[1];
    zsize=max_pt[2]-min_pt[2];

    print "zsize: %f" % zsize
    print "block size: %f" % block_size_z

    nblocks_x=int(round(xsize/block_size_x));
    nblocks_y=int(round(ysize/block_size_y));
    nblocks_z=int(round(zsize/block_size_z));
    print "nblocks_z : %f" % nblocks_z

    if(nblocks_x<=0):nblocks_x=1;
    if(nblocks_y<=0):nblocks_y=1;
    if(nblocks_z<=0):nblocks_z=1;

    print '\t Number of blocks in the x dimension: %d' % nblocks_x
    print '\t Number of blocks in the y dimension: %d' % nblocks_y
    print '\t Number of blocks in the z dimension: %d' % nblocks_z


    print("\t CREATING THE BOXM2_SCENE_SPTR")
    boxm2_batch.init_process("boxm2CreateSceneProcess");
    boxm2_batch.set_input_string(0,data_path);
    boxm2_batch.set_input_string(1,appearance_model1);
    boxm2_batch.set_input_string(2,appearance_model2);
    boxm2_batch.set_input_string(3,appearance_model3);
    boxm2_batch.set_input_float(4,min_pt[0]);
    boxm2_batch.set_input_float(5,min_pt[1]);
    boxm2_batch.set_input_float(6,min_pt[2]);
    boxm2_batch.run_process();
    (id,type)=boxm2_batch.commit_output(0);
    scene=dbvalue(id,type);


    for k in range(nblocks_z):
        for j in range(nblocks_y):
            for i in range(nblocks_x):
                block_origin_x=min_pt[0]+i*block_size_x;
                block_origin_y=min_pt[1]+j*block_size_y;
                block_origin_z=min_pt[2]+k*block_size_z;
                print '\t \t Creating block with id (%d,%d,%d) at origin (%s,%s,%s)' % (i,j,k,block_origin_x, block_origin_y, block_origin_z)
                boxm2_batch.init_process("boxm2AddBlockProcess");
                boxm2_batch.set_input_from_db(0,scene);
                boxm2_batch.set_input_int(1,i);
                boxm2_batch.set_input_int(2,j);
                boxm2_batch.set_input_int(3,k);
                boxm2_batch.set_input_unsigned(4,ntrees_x);
                boxm2_batch.set_input_unsigned(5,ntrees_y);
                boxm2_batch.set_input_unsigned(6,ntrees_z);
                boxm2_batch.set_input_unsigned(7,max_num_lvls);
                boxm2_batch.set_input_float(8,block_origin_x);
                boxm2_batch.set_input_float(9,block_origin_y);
                boxm2_batch.set_input_float(10,block_origin_z);
                boxm2_batch.set_input_float(11,tree_size);
                boxm2_batch.set_input_float(12,max_data_size);
                boxm2_batch.set_input_float(13,p_init);
                boxm2_batch.set_input_unsigned(14,1);
                boxm2_batch.run_process();



    boxm2_batch.init_process("boxm2WriteSceneXMLProcess")
    boxm2_batch.set_input_from_db(0,scene);
    boxm2_batch.set_input_string(1,"scene");
    boxm2_batch.run_process();

def boxm2CreateScene(scene_info,boxm2_dir, app1='boxm2_mog3_grey', app2='boxm2_num_obs', app3='boxm2_sum_log_msg_pos'):

    if not os.path.isdir(boxm2_dir + '/'):
        os.mkdir(boxm2_dir + '/');

    if not os.path.isdir(boxm2_dir + '/'):
        print "Invalid scene xml"
        sys.exit(-1);

    print 'Parsing: '
    print scene_info
    print boxm2_dir

    #parse xml file
    tree = ElementTree();
    tree.parse(scene_info);

    #find scene dimensions
    bbox_elm = tree.getroot().find('bbox');

    if bbox_elm is None:
        print "Invalid scene info file: No bbox"
        sys.exit(-1);


    minx = float(bbox_elm.get('minx'));
    miny = float(bbox_elm.get('miny'));
    minz = float(bbox_elm.get('minz'));
    maxx = float(bbox_elm.get('maxx'));
    maxy = float(bbox_elm.get('maxy'));
    maxz = float(bbox_elm.get('maxz'));

    #find scene resolution

    res_elm = tree.getroot().find('min_octree_cell_length');

    if res_elm is None:
        print "Invalid info file: No min_octree_cell_length"
        sys.exit(-1);

    resolution = float(res_elm.get('val'));

    print ("Resolution: " + str(resolution));


    res_elm = tree.getroot().find('prior_probability');

    if res_elm is None:
        print "Invalid info file: No prior_probability"
        sys.exit(-1);

    prior_probability = float(res_elm.get('val'));

    #PARAMETERS
    ntrees_x=32 #was 32
    ntrees_y=32 #was 32
    ntrees_z=32 #was 32
    max_num_lvls=4
    min_pt = [minx, miny, minz]
    max_pt = [maxx, maxy, maxz]

    writeSceneFromBox(boxm2_dir,resolution,min_pt,max_pt,ntrees_x,ntrees_y,ntrees_z,max_num_lvls,app1,app2,app3,prior_probability);

