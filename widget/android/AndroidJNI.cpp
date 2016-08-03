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
    nsAppShell::PostEvent(AndroidGeckoEvent::MakeFromJavaObject(jenv, event));
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
    class NotifyBatteryChangeRunnable : public Runnable {
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

NS_EXPORT float JNICALL
Java_org_mozilla_gecko_GeckoAppShell_computeRenderIntegrity(JNIEnv*, jclass)
{
    return nsWindow::ComputeRenderIntegrity();
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
  class ExitFullScreenRunnable : public Runnable {
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

}
