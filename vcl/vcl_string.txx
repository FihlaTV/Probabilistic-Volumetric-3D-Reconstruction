#include "vcl_string.h"

#if !VCL_USE_NATIVE_STL
# include "emulation/vcl_string.txx"
#elif defined(VCL_EGCS)
# include "egcs/vcl_string.txx"
#elif defined(VCL_GCC_295) && !defined(GNU_LIBSTDCXX_V3)
# include "gcc-295/vcl_string.txx"
#elif defined(GNU_LIBSTDCXX_V3)
# include "gcc-libstdcxx-v3/vcl_string.txx"
#elif defined(VCL_SUNPRO_CC)
# include "iso/vcl_string.txx"
#elif defined(VCL_SGI_CC)
# include "sgi/vcl_string.txx"
#elif defined(VCL_WIN32)
# include "win32/vcl_string.txx"
#else
# include "iso/vcl_string.txx"
#endif
