// This is vxl/vbl/io/vbl_io_array_2d.txx

//:
// \file
// \brief  binary IO functions for vbl_array_2d<T>
// \author K.Y.McGaul

#include <vsl/vsl_binary_io.h>
#include <vbl/vbl_array_2d.h>


//=======================================================================
//: Binary save self to stream.
template<class T>
void vsl_b_write(vsl_b_ostream & os, const vbl_array_2d<T> &p)
{
  const short io_version_no = 1;
  vsl_b_write(os, io_version_no);

  int array_rows = p.rows();
  int array_cols = p.cols();
  vsl_b_write(os, array_rows);
  vsl_b_write(os, array_cols);
  for (unsigned i=0; i<array_rows; i++)
  {
    for (unsigned j=0; j<array_cols; j++)
      vsl_b_write(os, p(i,j));
  }
}

//=======================================================================
//: Binary load self from stream.
template<class T>
void vsl_b_read(vsl_b_istream &is, vbl_array_2d<T> &p)
{
  short ver;
  int array_rows, array_cols;
  vsl_b_read(is, ver);
  switch(ver)
  {
  case 1:
    vsl_b_read(is, array_rows);
    vsl_b_read(is, array_cols);
    p.resize(array_rows, array_cols);
    for (unsigned i=0; i<array_rows; i++)
    {
      for (unsigned j=0; j<array_cols; j++)
        vsl_b_read(is, p(i,j));
    }
    break;

  default:

    vcl_cerr << "vbl_array_2d::b_read() Unknown version number "<< 
                                                   ver << vcl_endl;
    abort();
  }
}

//=======================================================================
//: Output a human readable summary to the stream
template<class T>
void vsl_print_summary(vcl_ostream & os,const vbl_array_2d<T> & p)
{
  os << "Rows: " << p.rows() << vcl_endl;
  os << "Columns: " << p.cols() << vcl_endl;
  for (unsigned i =0; i<p.rows() && i<5; i++)
  {
    for (unsigned j=0; j<p.cols() && j<5; j++)
    {
      os << " ";
      vsl_print_summary(os, p(i,j)); 
    }
    if (p.cols() > 5)
      os << "...";
    os << vcl_endl;
  }
  if (p.rows() > 5) 
    os << " ..." << vcl_endl;
}

#define VBL_IO_ARRAY_2D_INSTANTIATE(T) \
template void vsl_print_summary(vcl_ostream &, const vbl_array_2d<T> &); \
template void vsl_b_read(vsl_b_istream &, vbl_array_2d<T> &); \
template void vsl_b_write(vsl_b_ostream &, const vbl_array_2d<T> &); \
;
