#include <vgui/vgui_vil2_image_renderer.txx>
#include <vxl_config.h>
#include <vil2/vil2_rgb.h>

typedef vil2_rgb<vxl_uint_16> Type;

template class vgui_vil2_image_renderer<Type>;
