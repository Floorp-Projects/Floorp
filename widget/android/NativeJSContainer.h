/* -*- Mode: c++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NativeJSObject_h__
#define NativeJSObject_h__

#include "GeneratedJNIWrappers.h"
#include "jsapi.h"

namespace mozilla {
namespace widget {

java::NativeJSContainer::LocalRef
CreateNativeJSContainer(JSContext* cx, JS::HandleObject object);

} // namespace widget
} // namespace mozilla

#endif // NativeJSObject_h__

