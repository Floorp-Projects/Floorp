#ifndef mozilla_widget_AndroidColors_h
#define mozilla_widget_AndroidColors_h

#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace widget {

static const ColorPattern sAndroidBorderColor(ToDeviceColor(Color(0.75f, 0.75f, 0.75f)));
static const ColorPattern sAndroidCheckColor(ToDeviceColor(Color(0.21f, 0.23f, 0.25f)));

} // namespace widget
} // namespace mozilla

#endif
