/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeJSContainer.h"
#include "AndroidBridge.h"
#include "prthread.h"

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

    static jobject CloneInstance(JNIEnv* env, jobject instance) {
        NativeJSContainer* const container = FromInstance(env, instance);
        if (!container || !container->EnsureObject(env)) {
            return nullptr;
        }
        JSContext* const cx = container->mThreadContext;
        JS::RootedObject object(cx, container->mJSObject);
        MOZ_ASSERT(object);

        JSAutoStructuredCloneBuffer buffer;
        if (!buffer.write(cx, JS::RootedValue(cx, JS::ObjectValue(*object)))) {
            AndroidBridge::ThrowException(env,
                "java/lang/UnsupportedOperationException",
                "Cannot serialize object");
            return nullptr;
        }
        return CreateInstance(env, new NativeJSContainer(cx, Move(buffer)));
    }

    // Make sure we have a JSObject and deserialize if necessary/possible
    bool EnsureObject(JNIEnv* env) {
        if (mJSObject) {
            if (PR_GetCurrentThread() != mThread) {
                AndroidBridge::ThrowException(env,
                    "java/lang/IllegalThreadStateException",
                    "Using NativeJSObject off its thread");
                return false;
            }
            return true;
        }
        if (!SwitchContextToCurrentThread()) {
            AndroidBridge::ThrowException(env,
                "java/lang/IllegalThreadStateException",
                "Not available for this thread");
            return false;
        }

        JS::RootedValue value(mThreadContext);
        MOZ_ASSERT(mBuffer.data());
        MOZ_ALWAYS_TRUE(mBuffer.read(mThreadContext, &value));
        if (value.isObject()) {
            mJSObject = &value.toObject();
        }
        if (!mJSObject) {
            AndroidBridge::ThrowException(env,
                "java/lang/IllegalStateException", "Cannot deserialize data");
            return false;
        }
        mBuffer.clear();
        return true;
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

    // Thread that the object is valid on
    PRThread* mThread;
    // Context that the object is valid in
    JSContext* mThreadContext;
    // Deserialized object, or nullptr if object is in serialized form
    JS::Heap<JSObject*> mJSObject;
    // Serialized object, or empty if object is in deserialized form
    JSAutoStructuredCloneBuffer mBuffer;

    // Create a new container containing the given deserialized object
    NativeJSContainer(JSContext* cx, JS::HandleObject object)
            : mThread(PR_GetCurrentThread())
            , mThreadContext(cx)
            , mJSObject(object)
    {
    }

    // Create a new container containing the given serialized object
    NativeJSContainer(JSContext* cx, JSAutoStructuredCloneBuffer&& buffer)
            : mThread(PR_GetCurrentThread())
            , mThreadContext(cx)
            , mBuffer(Forward<JSAutoStructuredCloneBuffer>(buffer))
    {
    }

    bool SwitchContextToCurrentThread() {
        PRThread* const currentThread = PR_GetCurrentThread();
        if (currentThread == mThread) {
            return true;
        }
        return false;
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

NS_EXPORT jobject JNICALL
Java_org_mozilla_gecko_util_NativeJSContainer_clone(JNIEnv* env, jobject instance)
{
    MOZ_ASSERT(env);
    return NativeJSContainer::CloneInstance(env, instance);
}

} // extern "C"
