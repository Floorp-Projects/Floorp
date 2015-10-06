/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Hal.h"
#include "nsIFile.h"
#include "nsString.h"

#include "AndroidBridge.h"
#include "AndroidContentController.h"
#include "AndroidGraphicBuffer.h"

#include <jni.h>
#include <pthread.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

#include "nsAppShell.h"
#include "nsWindow.h"
#include <android/log.h>
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"

#ifdef MOZ_CRASHREPORTER
#include "nsICrashReporter.h"
#include "nsExceptionHandler.h"
#endif

#include "mozilla/unused.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/UniquePtr.h"

#include "mozilla/layers/APZCTreeManager.h"
#include "nsPluginInstanceOwner.h"
#include "AndroidSurfaceTexture.h"
#include "nsMemoryPressure.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla::widget::android;

/* Forward declare all the JNI methods as extern "C" */

extern "C" {
/*
 * Incoming JNI methods
 */

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_registerJavaUiThread(JNIEnv *jenv, jclass jc)
{
    AndroidBridge::RegisterJavaUiThread();
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifyGeckoOfEvent(JNIEnv *jenv, jclass jc, jobject event)
{
    // poke the appshell
    if (nsAppShell::gAppShell)
        nsAppShell::gAppShell->PostEvent(AndroidGeckoEvent::MakeFromJavaObject(jenv, event));
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifyGeckoObservers(JNIEnv *aEnv, jclass,
                                                         jstring aTopic, jstring aData)
{
    if (!NS_IsMainThread()) {
        jni::ThrowException(aEnv,
            "java/lang/IllegalThreadStateException", "Not on Gecko main thread");
        return;
    }

    nsCOMPtr<nsIObserverService> obsServ =
        mozilla::services::GetObserverService();
    if (!obsServ) {
        jni::ThrowException(aEnv,
            "java/lang/IllegalStateException", "No observer service");
        return;
    }

    const nsJNICString topic(aTopic, aEnv);
    const nsJNIString data(aData, aEnv);
    obsServ->NotifyObservers(nullptr, topic.get(), data.get());
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_processNextNativeEvent(JNIEnv *jenv, jclass, jboolean mayWait)
{
    // poke the appshell
    if (nsAppShell::gAppShell)
        nsAppShell::gAppShell->ProcessNextNativeEvent(mayWait != JNI_FALSE);
}

NS_EXPORT jlong JNICALL
Java_org_mozilla_gecko_GeckoAppShell_runUiThreadCallback(JNIEnv* env, jclass)
{
    if (!AndroidBridge::Bridge()) {
        return -1;
    }

    return AndroidBridge::Bridge()->RunDelayedUiThreadTasks();
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_onResume(JNIEnv *jenv, jclass jc)
{
    if (nsAppShell::gAppShell)
        nsAppShell::gAppShell->OnResume();
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_reportJavaCrash(JNIEnv *jenv, jclass, jstring jStackTrace)
{
#ifdef MOZ_CRASHREPORTER
    const nsJNICString stackTrace(jStackTrace, jenv);
    if (NS_WARN_IF(NS_FAILED(CrashReporter::AnnotateCrashReport(
            NS_LITERAL_CSTRING("JavaStackTrace"), stackTrace)))) {
        // Only crash below if crash reporter is initialized and annotation succeeded.
        // Otherwise try other means of reporting the crash in Java.
        return;
    }
#endif // MOZ_CRASHREPORTER
    MOZ_CRASH("Uncaught Java exception");
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_notifyBatteryChange(JNIEnv* jenv, jclass,
                                                         jdouble aLevel,
                                                         jboolean aCharging,
                                                         jdouble aRemainingTime)
{
    class NotifyBatteryChangeRunnable : public nsRunnable {
    public:
      NotifyBatteryChangeRunnable(double aLevel, bool aCharging, double aRemainingTime)
        : mLevel(aLevel)
        , mCharging(aCharging)
        , mRemainingTime(aRemainingTime)
      {}

      NS_IMETHODIMP Run() {
        hal::NotifyBatteryChange(hal::BatteryInformation(mLevel, mCharging, mRemainingTime));
        return NS_OK;
      }

    private:
      double mLevel;
      bool   mCharging;
      double mRemainingTime;
    };

    nsCOMPtr<nsIRunnable> runnable = new NotifyBatteryChangeRunnable(aLevel, aCharging, aRemainingTime);
    NS_DispatchToMainThread(runnable);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_invalidateAndScheduleComposite(JNIEnv*, jclass)
{
    nsWindow::InvalidateAndScheduleComposite();
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_scheduleResumeComposition(JNIEnv*, jclass, jint width, jint height)
{
    nsWindow::ScheduleResumeComposition(width, height);
}

NS_EXPORT float JNICALL
Java_org_mozilla_gecko_GeckoAppShell_computeRenderIntegrity(JNIEnv*, jclass)
{
    return nsWindow::ComputeRenderIntegrity();
}

#define MAX_LOCK_ATTEMPTS 10

static bool LockWindowWithRetry(void* window, unsigned char** bits, int* width, int* height, int* format, int* stride)
{
  int count = 0;

  while (count < MAX_LOCK_ATTEMPTS) {
      if (AndroidBridge::Bridge()->LockWindow(window, bits, width, height, format, stride))
        return true;

      count++;
      usleep(500);
  }

  return false;
}

NS_EXPORT jobject JNICALL
Java_org_mozilla_gecko_GeckoAppShell_getSurfaceBits(JNIEnv* jenv, jclass, jobject surface)
{
    static jclass jSurfaceBitsClass = nullptr;
    static jmethodID jSurfaceBitsCtor = 0;
    static jfieldID jSurfaceBitsWidth, jSurfaceBitsHeight, jSurfaceBitsFormat, jSurfaceBitsBuffer;

    jobject surfaceBits = nullptr;
    unsigned char* bitsCopy = nullptr;
    int dstWidth, dstHeight, dstSize;

    void* window = AndroidBridge::Bridge()->AcquireNativeWindow(jenv, surface);
    if (!window)
        return nullptr;

    unsigned char* bits;
    int srcWidth, srcHeight, format, srcStride;

    // So we lock/unlock once here in order to get whatever is currently the front buffer. It sucks.
    if (!LockWindowWithRetry(window, &bits, &srcWidth, &srcHeight, &format, &srcStride))
        return nullptr;

    AndroidBridge::Bridge()->UnlockWindow(window);

    // This is lock will result in the front buffer, since the last unlock rotated it to the back. Probably.
    if (!LockWindowWithRetry(window, &bits, &srcWidth, &srcHeight, &format, &srcStride))
        return nullptr;

    // These are from android.graphics.PixelFormat
    int bpp;
    switch (format) {
    case 1: // RGBA_8888
        bpp = 4;
        break;
    case 4: // RGB_565
        bpp = 2;
        break;
    default:
        goto cleanup;
    }

    dstWidth = mozilla::RoundUpPow2(srcWidth);
    dstHeight = mozilla::RoundUpPow2(srcHeight);
    dstSize = dstWidth * dstHeight * bpp;

    bitsCopy = (unsigned char*)malloc(dstSize);
    bzero(bitsCopy, dstSize);
    for (int i = 0; i < srcHeight; i++) {
        memcpy(bitsCopy + ((dstHeight - i - 1) * dstWidth * bpp), bits + (i * srcStride * bpp), srcStride * bpp);
    }

    if (!jSurfaceBitsClass) {
        jSurfaceBitsClass = (jclass)jenv->NewGlobalRef(jenv->FindClass("org/mozilla/gecko/SurfaceBits"));
        jSurfaceBitsCtor = jenv->GetMethodID(jSurfaceBitsClass, "<init>", "()V");

        jSurfaceBitsWidth = jenv->GetFieldID(jSurfaceBitsClass, "width", "I");
        jSurfaceBitsHeight = jenv->GetFieldID(jSurfaceBitsClass, "height", "I");
        jSurfaceBitsFormat = jenv->GetFieldID(jSurfaceBitsClass, "format", "I");
        jSurfaceBitsBuffer = jenv->GetFieldID(jSurfaceBitsClass, "buffer", "Ljava/nio/ByteBuffer;");
    }

    surfaceBits = jenv->NewObject(jSurfaceBitsClass, jSurfaceBitsCtor);
    jenv->SetIntField(surfaceBits, jSurfaceBitsWidth, dstWidth);
    jenv->SetIntField(surfaceBits, jSurfaceBitsHeight, dstHeight);
    jenv->SetIntField(surfaceBits, jSurfaceBitsFormat, format);
    jenv->SetObjectField(surfaceBits, jSurfaceBitsBuffer, jenv->NewDirectByteBuffer(bitsCopy, dstSize));

cleanup:
    AndroidBridge::Bridge()->UnlockWindow(window);
    AndroidBridge::Bridge()->ReleaseNativeWindow(window);

    return surfaceBits;
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_addPresentationSurface(JNIEnv* jenv, jclass, jobject surface)
{
    if (surface != NULL) {
        void* window = AndroidBridge::Bridge()->AcquireNativeWindow(jenv, surface);
        if (window) {
            AndroidBridge::Bridge()->SetPresentationWindow(window);
        }
    }
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_removePresentationSurface(JNIEnv* jenv, jclass, jobject surface)
{
    void* window = AndroidBridge::Bridge()->GetPresentationWindow();
    if (window) {
        AndroidBridge::Bridge()->SetPresentationWindow(nullptr);
        AndroidBridge::Bridge()->ReleaseNativeWindow(window);
    }
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_onFullScreenPluginHidden(JNIEnv* jenv, jclass, jobject view)
{
  class ExitFullScreenRunnable : public nsRunnable {
    public:
      ExitFullScreenRunnable(jobject view) : mView(view) {}

      NS_IMETHODIMP Run() {
        JNIEnv* const env = jni::GetGeckoThreadEnv();
        nsPluginInstanceOwner::ExitFullScreen(mView);
        env->DeleteGlobalRef(mView);
        return NS_OK;
      }

    private:
      jobject mView;
  };

  nsCOMPtr<nsIRunnable> runnable = new ExitFullScreenRunnable(jenv->NewGlobalRef(view));
  NS_DispatchToMainThread(runnable);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_onSurfaceTextureFrameAvailable(JNIEnv* jenv, jclass, jobject surfaceTexture, jint id)
{
  mozilla::gl::AndroidSurfaceTexture* st = mozilla::gl::AndroidSurfaceTexture::Find(id);
  if (!st) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoJNI", "Failed to find AndroidSurfaceTexture with id %d", id);
    return;
  }

  st->NotifyFrameAvailable();
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_GeckoAppShell_dispatchMemoryPressure(JNIEnv* jenv, jclass)
{
    NS_DispatchMemoryPressure(MemPressure_New);
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_gfx_NativePanZoomController_abortAnimation(JNIEnv* env, jobject instance)
{
    APZCTreeManager *controller = nsWindow::GetAPZCTreeManager();
    if (controller) {
        // TODO: Pass in correct values for presShellId and viewId.
        controller->CancelAnimation(ScrollableLayerGuid(nsWindow::RootLayerTreeId(), 0, 0));
    }
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_gfx_NativePanZoomController_init(JNIEnv* env, jobject instance)
{
    if (!AndroidBridge::Bridge()) {
        return;
    }

    const auto& newRef = NativePanZoomController::Ref::From(instance);
    NativePanZoomController::LocalRef oldRef =
            AndroidContentController::SetNativePanZoomController(newRef);

    // MOZ_ASSERT(!oldRef, "Registering a new NPZC when we already have one");
}

NS_EXPORT jboolean JNICALL
Java_org_mozilla_gecko_gfx_NativePanZoomController_handleTouchEvent(JNIEnv* env, jobject instance, jobject event)
{
    APZCTreeManager *controller = nsWindow::GetAPZCTreeManager();
    if (!controller) {
        return false;
    }

    AndroidGeckoEvent* wrapper = AndroidGeckoEvent::MakeFromJavaObject(env, event);
    MultiTouchInput input = wrapper->MakeMultiTouchInput(nsWindow::TopWindow());
    delete wrapper;

    if (input.mType < 0 || !nsAppShell::gAppShell) {
        return false;
    }

    ScrollableLayerGuid guid;
    uint64_t blockId;
    nsEventStatus status = controller->ReceiveInputEvent(input, &guid, &blockId);
    if (status != nsEventStatus_eConsumeNoDefault) {
        nsAppShell::gAppShell->PostEvent(AndroidGeckoEvent::MakeApzInputEvent(input, guid, blockId, status));
    }
    return true;
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_gfx_NativePanZoomController_handleMotionEvent(JNIEnv* env, jobject instance, jobject event)
{
    // FIXME implement this
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_gfx_NativePanZoomController_destroy(JNIEnv* env, jobject instance)
{
    if (!AndroidBridge::Bridge()) {
        return;
    }

    NativePanZoomController::LocalRef oldRef =
            AndroidContentController::SetNativePanZoomController(nullptr);

    MOZ_ASSERT(oldRef, "Clearing a non-existent NPZC");
}

NS_EXPORT jboolean JNICALL
Java_org_mozilla_gecko_gfx_NativePanZoomController_getRedrawHint(JNIEnv* env, jobject instance)
{
    // FIXME implement this
    return true;
}

NS_EXPORT void JNICALL
Java_org_mozilla_gecko_gfx_NativePanZoomController_setOverScrollMode(JNIEnv* env, jobject instance, jint overscrollMode)
{
    // FIXME implement this
}

NS_EXPORT jint JNICALL
Java_org_mozilla_gecko_gfx_NativePanZoomController_getOverScrollMode(JNIEnv* env, jobject instance)
{
    // FIXME implement this
    return 0;
}

}
