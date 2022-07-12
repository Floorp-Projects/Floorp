/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jni_GeckoBundleUtils_h
#define mozilla_jni_GeckoBundleUtils_h

#include "mozilla/java/GeckoBundleWrappers.h"

namespace mozilla {
namespace jni {

#define GECKOBUNDLE_START(name)                   \
  nsTArray<jni::String::LocalRef> _##name##_keys; \
  nsTArray<jni::Object::LocalRef> _##name##_values;

#define GECKOBUNDLE_PUT(name, key, value)                     \
  _##name##_keys.AppendElement(                               \
      jni::StringParam(NS_LITERAL_STRING_FROM_CSTRING(key))); \
  _##name##_values.AppendElement(value);

#define GECKOBUNDLE_FINISH(name)                                            \
  MOZ_ASSERT(_##name##_keys.Length() == _##name##_values.Length());         \
  auto _##name##_jkeys =                                                    \
      jni::ObjectArray::New<jni::String>(_##name##_keys.Length());          \
  auto _##name##_jvalues =                                                  \
      jni::ObjectArray::New<jni::Object>(_##name##_values.Length());        \
  for (size_t i = 0;                                                        \
       i < _##name##_keys.Length() && i < _##name##_values.Length(); i++) { \
    _##name##_jkeys->SetElement(i, _##name##_keys.ElementAt(i));            \
    _##name##_jvalues->SetElement(i, _##name##_values.ElementAt(i));        \
  }                                                                         \
  auto name =                                                               \
      mozilla::java::GeckoBundle::New(_##name##_jkeys, _##name##_jvalues);

}  // namespace jni
}  // namespace mozilla

#endif  // mozilla_jni_GeckoBundleUtils_h
