// This is vxl/vbl/io/vbl_io_array_1d.txx

//:
// \file
// \brief  binary IO functions for vbl_array_1d<T>
// \author K.Y.McGaul

#include <vsl/vsl_binary_io.h>
#include <vbl/vbl_array_1d.h>


//====================================================================
//: Binary save self to stream.
template<class T>
void vsl_b_write(vsl_b_ostream & os, const vbl_array_1d<T> & p)
{
  const short io_version_no = 1;
  vsl_b_write(os, io_version_no);

  int array_size = p.size();
  vsl_b_write(os, array_size);   
  int array_capacity = p.capacity(); 
  vsl_b_write(os, array_capacity);
  for (unsigned i=0; i < array_size; ++i)
    vsl_b_write(os, p[i]);
}

//====================================================================
//: Binary load self from stream.
template<class T>
void vsl_b_read(vsl_b_istream &is, vbl_array_1d<T> & p)
{
  short ver;
  int array_size;
  int array_capacity;
  T val;
  vsl_b_read(is, ver);
  switch(ver)
  {
  case 1:
    vsl_b_read(is, array_size);
    vsl_b_read(is, array_capacity);
    p.reserve(array_capacity);
    for (unsigned i=0; i<array_size; ++i)
    {
      vsl_b_read(is, val);
      p.push_back(val);
    }
    break;

  default:

    vcl_cerr << "vbl_array_1d::b_read() Unknown version number "<< 
                                                   ver << vcl_endl;
    abort();
  }
}


//===========================================================================
//: Output a human readable summary to the stream
template<class T>
void vsl_print_summary(vcl_ostream & os,const vbl_array_1d<T> & p)
{
  os<<"Length: "<<p.size()<<vcl_endl;
  for ( int i =0; i < p.size() && i < 5; i++ )
  {
    os << " " << i << ": "; 
    vsl_print_summary(os, p[i]);
    os << vcl_endl;
  }
  if (p.size() > 5) 
    os << " ..." << vcl_endl;
}

#define VBL_IO_ARRAY_1D_INSTANTIATE(T) \
template void vsl_print_summary(vcl_ostream &, const vbl_array_1d<T> &); \
template void vsl_b_read(vsl_b_istream &, vbl_array_1d<T> &); \
template void vsl_b_write(vsl_b_ostream &, const vbl_array_1d<T> &); \
;
