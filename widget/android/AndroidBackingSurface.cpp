/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "AndroidBackingSurface.h"
#include "AndroidBridge.h"
#include "AndroidJavaWrappers.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <GLES2/gl2.h>
#include <android/log.h>
#include <dlfcn.h>
#include <stdint.h>

#define LIBHARDWARE_NAME                     "libhardware.so"
#define LIBHARDWARE_HW_GET_MODULE_SYMBOL     "hw_get_module"

#define GRALLOC_MODULE_NAME                  "gralloc"

#define GRALLOC_USAGE_SW_READ_OFTEN          0x03
#define GRALLOC_USAGE_SW_WRITE_OFTEN         0x30

// We can't just #include <EGL/egl.h> because that causes conflicts with types in GLContext.h.

typedef void* EGLClientBuffer;

extern "C" {

EGLImageKHR eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx, int target, EGLClientBuffer buffer,
                              const int *attrib_list);

}

// Private Android system stuff

typedef void* buffer_handle_t;
struct hw_module_methods_t;

struct hw_module_t {
    uint32_t tag;
    uint16_t version_major;
    uint16_t version_minor;
    const char* id;
    const char* name;
    const char* author;
    hw_module_methods_t* methods;
    void* dso;
    uint32_t reserved[32 - 7];
};

struct gralloc_module_t {
    hw_module_t common;

    int (*registerBuffer)(gralloc_module_t const* module, buffer_handle_t handle);
    int (*unregisterBuffer)(gralloc_module_t const* module, buffer_handle_t handle);
    int (*lock)(gralloc_module_t const* module, buffer_handle_t handle, int usage, int l, int t,
                int w, int h, void** vaddr);
    int (*unlock)(gralloc_module_t const* module, buffer_handle_t handle);
    int (*perform)(gralloc_module_t const* module, int operation, ...);
    void* reserved_proc[7];
};

struct android_native_base_t {
    int magic;
    int version;                                    // The size of the native EGL type.
    void* reserved[4];
    void (*incRef)(android_native_base_t* base);
    void (*decRef)(android_native_base_t* base);
};

struct android_native_buffer_t {
    android_native_base_t common;

    int width;
    int height;
    int stride;
    int format;
    int usage;

    void* reserved[2];

    buffer_handle_t handle;

    void* reserved_proc[8];
};

struct ANativeWindow {
    android_native_base_t common;

    const uint32_t flags;
    const int minSwapInterval;
    const int maxSwapInterval;
    const float xdpi;
    const float ydpi;
    intptr_t oem[4];
    int (*setSwapInterval)(ANativeWindow* window, int interval);
    int (*dequeueBuffer)(ANativeWindow* window, android_native_buffer_t** buffer);
    int (*lockBuffer)(ANativeWindow* window, android_native_buffer_t* buffer);
    int (*queueBuffer)(ANativeWindow* window, android_native_buffer_t* buffer);
    int (*query)(ANativeWindow* window, int what, int* value);
    int (*perform)(ANativeWindow* window, int operation, ...);
    int (*cancelBuffer)(ANativeWindow* window, android_native_buffer_t* buffer);
    void* reserved_proc[2];
};

namespace {

// Wrappers for Android private APIs.

class AndroidPrivateFunctions {
public:
    static AndroidPrivateFunctions* Get();

    // libhardware functions
    typedef void (*pfn_hw_get_module)(const char* id, hw_module_t** module);

    pfn_hw_get_module f_hw_get_module;

private:
    AndroidPrivateFunctions();
    static AndroidPrivateFunctions* sInstance;
};

class AndroidGrallocModule {
public:
    static AndroidGrallocModule* Get();

    unsigned char* Lock(android_native_buffer_t* aBuffer, const gfxIntSize& aSize);
    void Unlock(android_native_buffer_t* aBuffer);

private:
    AndroidGrallocModule();

    static AndroidGrallocModule* sInstance;
    gralloc_module_t* mModule;
};

AndroidPrivateFunctions* AndroidPrivateFunctions::sInstance = NULL;
AndroidGrallocModule* AndroidGrallocModule::sInstance = NULL;

AndroidPrivateFunctions::AndroidPrivateFunctions() {
    void* libhardware = dlopen("libhardware.so", RTLD_LAZY);
    NS_ABORT_IF_FALSE(libhardware, "Couldn't open libhardware.so!");
    f_hw_get_module = reinterpret_cast<pfn_hw_get_module>
        (dlsym(libhardware, LIBHARDWARE_HW_GET_MODULE_SYMBOL));
}

AndroidPrivateFunctions*
AndroidPrivateFunctions::Get() {
    if (!sInstance) {
        sInstance = new AndroidPrivateFunctions();
    }
    return sInstance;
}

AndroidGrallocModule::AndroidGrallocModule() {
    hw_module_t** modulePtr = reinterpret_cast<hw_module_t**>(&mModule);
    AndroidPrivateFunctions::Get()->f_hw_get_module(GRALLOC_MODULE_NAME, modulePtr);
    NS_ABORT_IF_FALSE(mModule, "Couldn't get the module!");
}

AndroidGrallocModule*
AndroidGrallocModule::Get() {
    if (!sInstance) {
        sInstance = new AndroidGrallocModule();
    }
    return sInstance;
}

unsigned char*
AndroidGrallocModule::Lock(android_native_buffer_t* aBuffer, const gfxIntSize& aSize) {
    void* bits;
    mModule->lock(mModule, aBuffer->handle,
                  GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN,
                  0, 0, aSize.width, aSize.height, &bits);
    return reinterpret_cast<unsigned char*>(bits);
}

void
AndroidGrallocModule::Unlock(android_native_buffer_t* aBuffer) {
    mModule->unlock(mModule, aBuffer->handle);
}

}   // end anonymous namespace

namespace mozilla {

// TODO: Investigate the functions in /usr/include/android/native_window.h -- they look like
// public APIs.

void
CreateAndroidBackingSurface(const gfxIntSize& aSize, nsRefPtr<gfxASurface>& aBackingSurface,
                            EGLSurface& aSurface, EGLClientBuffer& aBuffer) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoAndroidBackingSurface",
                        "### CreateAndroidBackingSurface");

    AndroidBridge::AutoLocalJNIFrame jniFrame(GetJNIForThread());

    AndroidGeckoLayerClient& client = AndroidBridge::Bridge()->GetLayerClient();
    AndroidGeckoGLLayerClient& glClient = static_cast<AndroidGeckoGLLayerClient&>(client);

    AndroidSurfaceView surfaceView;
    glClient.CreateSurfaceViewForBackingSurface(surfaceView, aSize.width, aSize.height);
    ANativeWindow* nativeWindow = surfaceView.GetNativeWindow();

    // Must increment the reference count on the native window to avoid crashes on Mali GPUs
    // (used in the Galaxy S II, et al).
    aSurface = reinterpret_cast<EGLSurface>(nativeWindow);
    nativeWindow->common.incRef(&nativeWindow->common);

    android_native_buffer_t* nativeBuffer;
    nativeWindow->dequeueBuffer(nativeWindow, &nativeBuffer);
    nativeWindow->lockBuffer(nativeWindow, nativeBuffer);

    // Increment the reference count on the buffer too, in order to be on the safe side.
    aBuffer = reinterpret_cast<EGLClientBuffer>(nativeBuffer);
    nativeBuffer->common.incRef(&nativeBuffer->common);

}

already_AddRefed<gfxImageSurface>
LockAndroidBackingBuffer(EGLClientBuffer aBuffer, const gfxIntSize& aSize) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoAndroidBackingSurface",
                        "### LockAndroidBackingBuffer");

    AndroidBridge::AutoLocalJNIFrame jniFrame(GetJNIForThread());

    android_native_buffer_t* nativeBuffer = reinterpret_cast<android_native_buffer_t*>(aBuffer);
    unsigned char* bits = AndroidGrallocModule::Get()->Lock(nativeBuffer, aSize);
    NS_ABORT_IF_FALSE(bits, "Couldn't lock the bits!");

    return new gfxImageSurface(bits, gfxIntSize(aSize.width, aSize.height), aSize.width * 2,
                               gfxASurface::ImageFormatRGB16_565);
}

void
UnlockAndroidBackingBuffer(EGLClientBuffer aBuffer) {
    __android_log_print(ANDROID_LOG_ERROR, "GeckoAndroidBackingSurface",
                        "### UnlockAndroidBackingBuffer");

    AndroidBridge::AutoLocalJNIFrame jniFrame(GetJNIForThread());

    AndroidGrallocModule::Get()->Unlock(reinterpret_cast<android_native_buffer_t*>(aBuffer));
}

}   // end namespace mozilla

