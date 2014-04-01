/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeJSContainer.h"
#include "AndroidBridge.h"

using namespace mozilla;
using namespace mozilla::widget;

namespace mozilla {
namespace widget {

class NativeJSContainer
{
public:
    static void InitJNI(JNIEnv* env) {
        if (jNativeJSContainer) {
            return;
        }
        jNativeJSContainer = AndroidBridge::GetClassGlobalRef(
            env, "org/mozilla/gecko/util/NativeJSContainer");
        MOZ_ASSERT(jNativeJSContainer);
        jContainerNativeObject = AndroidBridge::GetFieldID(
            env, jNativeJSContainer, "mNativeObject", "J");
        MOZ_ASSERT(jContainerNativeObject);
        jContainerConstructor = AndroidBridge::GetMethodID(
            env, jNativeJSContainer, "<init>", "(J)V");
        MOZ_ASSERT(jContainerConstructor);
    }

    static jobject CreateInstance(JNIEnv* env, JSContext* cx,
                                  JS::HandleObject object) {
        return CreateInstance(env, new NativeJSContainer(cx, object));
    }

    static NativeJSContainer* FromInstance(JNIEnv* env, jobject instance) {
        MOZ_ASSERT(instance);

        const jlong fieldValue =
            env->GetLongField(instance, jContainerNativeObject);
        NativeJSContainer* const nativeObject =
            reinterpret_cast<NativeJSContainer* const>(
            static_cast<uintptr_t>(fieldValue));
        if (!nativeObject) {
            AndroidBridge::ThrowException(env,
                "java/lang/NullPointerException",
                "Uninitialized NativeJSContainer");
        }
        return nativeObject;
    }

    static void DisposeInstance(JNIEnv* env, jobject instance) {
        NativeJSContainer* const container = FromInstance(env, instance);
        if (container) {
            env->SetLongField(instance, jContainerNativeObject,
                static_cast<jlong>(reinterpret_cast<uintptr_t>(nullptr)));
            delete container;
        }
    }

private:
    static jclass jNativeJSContainer;
    static jfieldID jContainerNativeObject;
    static jmethodID jContainerConstructor;

    static jobject CreateInstance(JNIEnv* env,
                                  NativeJSContainer* nativeObject) {
        InitJNI(env);
        const jobject newObject =
            env->NewObject(jNativeJSContainer, jContainerConstructor,
                           static_cast<jlong>(
                           reinterpret_cast<uintptr_t>(nativeObject)));
        AndroidBridge::HandleUncaughtException(env);
        MOZ_ASSERT(newObject);
        return newObject;
    }

    JSContext* const mContext;
    // Root JS object
    const JS::Heap<JSObject*> mJSObject;

    // Create a new container containing the given object
    NativeJSContainer(JSContext* cx, JS::HandleObject object)
            : mContext(cx)
            , mJSObject(object)
    {
    }
};

jclass NativeJSContainer::jNativeJSContainer = 0;
jfieldID NativeJSContainer::jContainerNativeObject = 0;
jmethodID NativeJSContainer::jContainerConstructor = 0;

jobject
CreateNativeJSContainer(JNIEnv* env, JSContext* cx, JS::HandleObject object)
{
    return NativeJSContainer::CreateInstance(env, cx, object);
}

} // namespace widget
} // namespace mozilla

extern "C" {

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_util_NativeJSContainer_dispose(JNIEnv* env, jobject instance)
{
    MOZ_ASSERT(env);
    NativeJSContainer::DisposeInstance(env, instance);
}

} // extern "C"
