/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Conversions.h"
#include "JavaBuiltins.h"

#include "mozilla/ipc/GeckoChildProcessHost.h"

namespace mozilla {
namespace jni {

template <class T>
jfieldID GetValueFieldID(JNIEnv* aEnv, const char* aType) {
  const jfieldID id = aEnv->GetFieldID(
      typename T::Context(aEnv, nullptr).ClassRef(), "value", aType);
  aEnv->ExceptionClear();
  return id;
}

// Cached locations of the primitive types within their standard boxed objects
// to skip doing that lookup on every get.
static jfieldID gBooleanValueField;
static jfieldID gIntValueField;
static jfieldID gDoubleValueField;

void InitConversionStatics() {
  MOZ_ASSERT(NS_IsMainThread());
  JNIEnv* const env = jni::GetGeckoThreadEnv();
  gBooleanValueField = GetValueFieldID<java::sdk::Boolean>(env, "Z");
  gIntValueField = GetValueFieldID<java::sdk::Integer>(env, "I");
  gDoubleValueField = GetValueFieldID<java::sdk::Double>(env, "D");
}

template <>
bool Java2Native(mozilla::jni::Object::Param aData, JNIEnv* aEnv) {
  MOZ_ASSERT(aData.IsInstanceOf<jni::Boolean>());

  bool result = false;
  if (gBooleanValueField) {
    if (!aEnv) {
      aEnv = jni::GetEnvForThread();
    }
    result =
        aEnv->GetBooleanField(aData.Get(), gBooleanValueField) != JNI_FALSE;
    MOZ_CATCH_JNI_EXCEPTION(aEnv);
  } else {
    result = java::sdk::Boolean::Ref::From(aData)->BooleanValue();
  }

  return result;
}

template <>
int Java2Native(mozilla::jni::Object::Param aData, JNIEnv* aEnv) {
  MOZ_ASSERT(aData.IsInstanceOf<jni::Integer>());

  int result = 0;
  if (gIntValueField) {
    if (!aEnv) {
      aEnv = jni::GetEnvForThread();
    }
    result = aEnv->GetIntField(aData.Get(), gIntValueField);
    MOZ_CATCH_JNI_EXCEPTION(aEnv);
  } else {
    result = java::sdk::Number::Ref::From(aData)->IntValue();
  }

  return result;
}

template <>
double Java2Native(mozilla::jni::Object::Param aData, JNIEnv* aEnv) {
  MOZ_ASSERT(aData.IsInstanceOf<jni::Double>());

  double result = 0;
  if (gDoubleValueField) {
    if (!aEnv) {
      aEnv = jni::GetEnvForThread();
    }
    result = aEnv->GetDoubleField(aData.Get(), gDoubleValueField);
    MOZ_CATCH_JNI_EXCEPTION(aEnv);
  } else {
    result = java::sdk::Number::Ref::From(aData)->DoubleValue();
  }

  return result;
}

template <>
ipc::LaunchError Java2Native(mozilla::jni::Object::Param aData, JNIEnv* aEnv) {
  // Bug 1819311: there is not much we can really catch due to how Android
  // services are started, so for now we just expose it this way.
  return ipc::LaunchError("Java2Native");
}

template <>
nsString Java2Native(mozilla::jni::Object::Param aData, JNIEnv* aEnv) {
  nsString result;
  if (aData != NULL && aData.IsInstanceOf<jni::String>()) {
    result = jni::String::Ref::From(aData)->ToString();
  }
  return result;
}

template <>
nsresult Java2Native(mozilla::jni::Object::Param aData, JNIEnv* aEnv) {
  MOZ_ASSERT(aData.IsInstanceOf<jni::Throwable>());
  return NS_ERROR_FAILURE;
}

}  // namespace jni
}  // namespace mozilla
