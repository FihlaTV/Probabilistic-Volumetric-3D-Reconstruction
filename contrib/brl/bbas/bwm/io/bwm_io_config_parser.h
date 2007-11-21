#ifndef bwm_io_config_parser_h_
#define bwm_io_config_parser_h_

#include "bwm_io_structs.h"

#include <expatpp/expatpplib.h>
#include <vcl_iostream.h>
#include <vcl_string.h>
#include <vcl_cstdio.h>
#include <vcl_cassert.h>
#include <vcl_vector.h>
#include <vcl_utility.h>

#include <vsol/vsol_point_2d.h>
#include <vsol/vsol_point_3d.h>

class bwm_io_config_parser : public expatpp {
public:
  
  bwm_io_config_parser(void);
  ~bwm_io_config_parser(void){};
  vcl_vector<bwm_io_tab_config* > tableau_config() { return tableaus_; }
  vcl_vector<vcl_vector<vcl_pair<vcl_string, vsol_point_2d> > > correspondences() { return corresp_; }
  vcl_string corresp_mode() {return corr_mode_; }
  vcl_vector<vsol_point_3d> corresp_world_pts() {return corresp_world_pts_; }

private:
  virtual void startElement(const XML_Char* name, const XML_Char** atts);
  virtual void endElement(const XML_Char* name);
  virtual void charData(const XML_Char* s, int len);

  void handleAtts(const XML_Char** atts);
  void cdataHandler(vcl_string name, vcl_string data);
  void WriteIndent();
  void init_params();

  //Data
  int mDepth;
  vcl_string cdata;
  vcl_string last_tag;

  vcl_vector<bwm_io_tab_config* > tableaus_;
  vcl_string name_;
  bool status_;
  vcl_string image_path_;
  vcl_string camera_path_;
  vcl_string camera_type_;
  vcl_string proj2d_type_;
  vcl_string coin3d_name_;
  vcl_string object_path_;
  vcl_string object_type_;

  // correspondence related parameters
  vcl_string corr_mode_;
  vcl_string corr_cam_tab_;
  double X_, Y_, Z_;
  vcl_vector<vcl_vector<vcl_pair<vcl_string, vsol_point_2d> > > corresp_;
  vcl_vector<vsol_point_3d> corresp_world_pts_;
  vcl_vector<vcl_pair<vcl_string, vsol_point_2d> > corresp_elm_;

  void trim_string(vcl_string& s);
};

#endif
