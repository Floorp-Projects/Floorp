/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jni_Conversions_h__
#define mozilla_jni_Conversions_h__

#include "mozilla/jni/Refs.h"

namespace mozilla {
namespace jni {

template <typename ArgType>
ArgType Java2Native(mozilla::jni::Object::Param, JNIEnv* aEnv = nullptr);

void InitConversionStatics();

}  // namespace jni
}  // namespace mozilla

#endif  // mozilla_jni_Conversions_h__
