
#include <testlib/testlib_test.h>

#include <boxm/boxm_scene.h>
#include <boxm/boxm_cell_vis_graph_iterator.h>
#include "test_utils.h"
#include <vpl/vpl.h>

MAIN( test_cell_vis_graph )
{
  START ("CREATE SCENE");
  short nlevels=5;

  // create scene
  bgeo_lvcs lvcs(33.33,44.44,10.0, bgeo_lvcs::wgs84, bgeo_lvcs::DEG, bgeo_lvcs::METERS);
  vgl_point_3d<double> origin(10,10,20);
  vgl_vector_3d<double> block_dim(10,10,10);
  vgl_vector_3d<double> world_dim(10,10,10);
  boxm_scene<tree_type> scene(lvcs, origin, block_dim, world_dim);
  scene.set_paths("./boxm_scene", "block");

  vgl_box_3d<double> world;
  world.add(origin);
  world.add(vgl_point_3d<double>(origin.x()+world_dim.x(), origin.y()+world_dim.y(), origin.z()+world_dim.z()));

  tree_type * tree=new tree_type();
  vpgl_camera_double_sptr camera = generate_camera_top(world);

  boxm_block_iterator<boct_tree<short,vgl_point_3d<double> > > iter(&scene);
  
  while(!iter.end())
  {
    scene.load_block(iter.index().x(),iter.index().y(),iter.index().z());
    boxm_block<boct_tree<short,vgl_point_3d<double> > > * block=scene.get_active_block();
    boct_tree<short,vgl_point_3d<double> > * tree=new boct_tree<short,vgl_point_3d<double> >(3,2);
    block->init_tree(tree);
	boxm_cell_vis_graph_iterator<short,vgl_point_3d<double> > cell_iterator(camera,tree,IMAGE_U, IMAGE_V);
	double cnt=30;
	while (cell_iterator.next()){
		vcl_cout<<"Frontier\n";
		vcl_vector<boct_tree_cell<short,vgl_point_3d<double>> *> vis_cells=cell_iterator.frontier();
		for(unsigned i=0;i<vis_cells.size();i++)
		{
			vgl_box_3d<double> box=tree->cell_bounding_box(vis_cells[i]);
			TEST("Returns correct frontier",cnt,box.max_z());

			

		}
		cnt-=5;
	}

	scene.write_active_block();
    iter++;
  }
  vpl_rmdir("./boxm_scene");
  vpl_unlink("./scene.bin");

  SUMMARY();
}
