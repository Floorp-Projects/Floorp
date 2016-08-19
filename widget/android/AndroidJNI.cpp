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

#include "mozilla/unused.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/UniquePtr.h"

#include "mozilla/layers/APZCTreeManager.h"
#include "nsPluginInstanceOwner.h"
#include "AndroidSurfaceTexture.h"

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
Java_org_mozilla_gecko_GeckoAppShell_invalidateAndScheduleComposite(JNIEnv*, jclass)
{
    nsWindow::InvalidateAndScheduleComposite();
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
Java_org_mozilla_gecko_GeckoAppShell_onSurfaceTextureFrameAvailable(JNIEnv* jenv, jclass, jobject surfaceTexture, jint id)
{
  mozilla::gl::AndroidSurfaceTexture* st = mozilla::gl::AndroidSurfaceTexture::Find(id);
  if (!st) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoJNI", "Failed to find AndroidSurfaceTexture with id %d", id);
    return;
  }

  st->NotifyFrameAvailable();
}

}
