/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WidgetUtilsGtk_h__
#define WidgetUtilsGtk_h__

#include <stdint.h>

namespace mozilla {
namespace widget {

class WidgetUtilsGTK
{
public:
  /* See WidgetUtils::IsTouchDeviceSupportPresent(). */
  static int32_t IsTouchDeviceSupportPresent();
};

}  // namespace widget

}  // namespace mozilla

#endif // WidgetUtilsGtk_h__
