/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>
#include <android/log.h>
#include <GLES2/gl2.h>
#include "AndroidGraphicBuffer.h"
#include "AndroidBridge.h"
#include "mozilla/Preferences.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "AndroidGraphicBuffer" , ## args)

#define EGL_NATIVE_BUFFER_ANDROID 0x3140
#define EGL_IMAGE_PRESERVED_KHR   0x30D2

typedef void *EGLContext;
typedef void *EGLDisplay;
typedef PRUint32 EGLenum;
typedef PRInt32 EGLint;
typedef PRUint32 EGLBoolean;

typedef gfxASurface::gfxImageFormat gfxImageFormat;

#define EGL_TRUE 1
#define EGL_FALSE 0
#define EGL_NONE 0x3038
#define EGL_NO_CONTEXT (EGLContext)0
#define EGL_DEFAULT_DISPLAY  (void*)0

#define ANDROID_LIBUI_PATH "libui.so"
#define ANDROID_GLES_PATH "libGLESv2.so"
#define ANDROID_EGL_PATH "libEGL.so"

// Really I have no idea, but this should be big enough
#define GRAPHIC_BUFFER_SIZE 1024

enum {
    /* buffer is never read in software */
    GRALLOC_USAGE_SW_READ_NEVER   = 0x00000000,
    /* buffer is rarely read in software */
    GRALLOC_USAGE_SW_READ_RARELY  = 0x00000002,
    /* buffer is often read in software */
    GRALLOC_USAGE_SW_READ_OFTEN   = 0x00000003,
    /* mask for the software read values */
    GRALLOC_USAGE_SW_READ_MASK    = 0x0000000F,

    /* buffer is never written in software */
    GRALLOC_USAGE_SW_WRITE_NEVER  = 0x00000000,
    /* buffer is never written in software */
    GRALLOC_USAGE_SW_WRITE_RARELY = 0x00000020,
    /* buffer is never written in software */
    GRALLOC_USAGE_SW_WRITE_OFTEN  = 0x00000030,
    /* mask for the software write values */
    GRALLOC_USAGE_SW_WRITE_MASK   = 0x000000F0,

    /* buffer will be used as an OpenGL ES texture */
    GRALLOC_USAGE_HW_TEXTURE      = 0x00000100,
    /* buffer will be used as an OpenGL ES render target */
    GRALLOC_USAGE_HW_RENDER       = 0x00000200,
    /* buffer will be used by the 2D hardware blitter */
    GRALLOC_USAGE_HW_2D           = 0x00000400,
    /* buffer will be used with the framebuffer device */
    GRALLOC_USAGE_HW_FB           = 0x00001000,
    /* mask for the software usage bit-mask */
    GRALLOC_USAGE_HW_MASK         = 0x00001F00,
};

enum {
    HAL_PIXEL_FORMAT_RGBA_8888          = 1,
    HAL_PIXEL_FORMAT_RGBX_8888          = 2,
    HAL_PIXEL_FORMAT_RGB_888            = 3,
    HAL_PIXEL_FORMAT_RGB_565            = 4,
    HAL_PIXEL_FORMAT_BGRA_8888          = 5,
    HAL_PIXEL_FORMAT_RGBA_5551          = 6,
    HAL_PIXEL_FORMAT_RGBA_4444          = 7,
};

typedef struct ARect {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} ARect;

static bool gTryRealloc = true;

static class GLFunctions
{
public:
  GLFunctions() : mInitialized(false)
  {
  }

  typedef EGLDisplay (* pfnGetDisplay)(void *display_id);
  pfnGetDisplay fGetDisplay;
  typedef EGLint (* pfnEGLGetError)(void);
  pfnEGLGetError fEGLGetError;

  typedef EGLImageKHR (* pfnCreateImageKHR)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
  pfnCreateImageKHR fCreateImageKHR;
  typedef EGLBoolean (* pfnDestroyImageKHR)(EGLDisplay dpy, EGLImageKHR image);
  pfnDestroyImageKHR fDestroyImageKHR;

  typedef void (* pfnImageTargetTexture2DOES)(GLenum target, EGLImageKHR image);
  pfnImageTargetTexture2DOES fImageTargetTexture2DOES;

  typedef void (* pfnBindTexture)(GLenum target, GLuint texture);
  pfnBindTexture fBindTexture;

  typedef GLenum (* pfnGLGetError)();
  pfnGLGetError fGLGetError;

  typedef void (*pfnGraphicBufferCtor)(void*, PRUint32 w, PRUint32 h, PRUint32 format, PRUint32 usage);
  pfnGraphicBufferCtor fGraphicBufferCtor;

  typedef void (*pfnGraphicBufferDtor)(void*);
  pfnGraphicBufferDtor fGraphicBufferDtor;

  typedef int (*pfnGraphicBufferLock)(void*, PRUint32 usage, unsigned char **addr);
  pfnGraphicBufferLock fGraphicBufferLock;

  typedef int (*pfnGraphicBufferLockRect)(void*, PRUint32 usage, const ARect&, unsigned char **addr);
  pfnGraphicBufferLockRect fGraphicBufferLockRect;

  typedef int (*pfnGraphicBufferUnlock)(void*);
  pfnGraphicBufferUnlock fGraphicBufferUnlock;

  typedef void* (*pfnGraphicBufferGetNativeBuffer)(void*);
  pfnGraphicBufferGetNativeBuffer fGraphicBufferGetNativeBuffer;

  typedef int (*pfnGraphicBufferReallocate)(void*, PRUint32 w, PRUint32 h, PRUint32 format);
  pfnGraphicBufferReallocate fGraphicBufferReallocate;

  bool EnsureInitialized()
  {
    if (mInitialized) {
      return true;
    }

    void *handle = dlopen(ANDROID_EGL_PATH, RTLD_LAZY);
    if (!handle) {
      LOG("Couldn't load EGL library");
      return false;
    }

    fGetDisplay = (pfnGetDisplay)dlsym(handle, "eglGetDisplay");
    fEGLGetError = (pfnEGLGetError)dlsym(handle, "eglGetError");
    fCreateImageKHR = (pfnCreateImageKHR)dlsym(handle, "eglCreateImageKHR");
    fDestroyImageKHR = (pfnDestroyImageKHR)dlsym(handle, "eglDestroyImageKHR");

    if (!fGetDisplay || !fEGLGetError || !fCreateImageKHR || !fDestroyImageKHR) {
      LOG("Failed to find some EGL functions");
      return false;
    }

    handle = dlopen(ANDROID_GLES_PATH, RTLD_LAZY);
    if (!handle) {
      LOG("Couldn't load GL library");
      return false;
    }

    fImageTargetTexture2DOES = (pfnImageTargetTexture2DOES)dlsym(handle, "glEGLImageTargetTexture2DOES");
    fBindTexture = (pfnBindTexture)dlsym(handle, "glBindTexture");
    fGLGetError = (pfnGLGetError)dlsym(handle, "glGetError");

    if (!fImageTargetTexture2DOES || !fBindTexture || !fGLGetError) {
      LOG("Failed to find some GL functions");
      return false;
    }

    handle = dlopen(ANDROID_LIBUI_PATH, RTLD_LAZY);
    if (!handle) {
      LOG("Couldn't load libui.so");
      return false;
    }

    fGraphicBufferCtor = (pfnGraphicBufferCtor)dlsym(handle, "_ZN7android13GraphicBufferC1Ejjij");
    fGraphicBufferDtor = (pfnGraphicBufferDtor)dlsym(handle, "_ZN7android13GraphicBufferD1Ev");
    fGraphicBufferLock = (pfnGraphicBufferLock)dlsym(handle, "_ZN7android13GraphicBuffer4lockEjPPv");
    fGraphicBufferLockRect = (pfnGraphicBufferLockRect)dlsym(handle, "_ZN7android13GraphicBuffer4lockEjRKNS_4RectEPPv");
    fGraphicBufferUnlock = (pfnGraphicBufferUnlock)dlsym(handle, "_ZN7android13GraphicBuffer6unlockEv");
    fGraphicBufferGetNativeBuffer = (pfnGraphicBufferGetNativeBuffer)dlsym(handle, "_ZNK7android13GraphicBuffer15getNativeBufferEv");
    fGraphicBufferReallocate = (pfnGraphicBufferReallocate)dlsym(handle, "_ZN7android13GraphicBuffer10reallocateEjjij");

    if (!fGraphicBufferCtor || !fGraphicBufferDtor || !fGraphicBufferLock ||
        !fGraphicBufferUnlock || !fGraphicBufferGetNativeBuffer) {
      LOG("Failed to lookup some GraphicBuffer functions");
      return false;
    }

    mInitialized = true;
    return true;
  }

private:
  bool mInitialized;

} sGLFunctions;

namespace mozilla {

static void clearGLError()
{
  while (glGetError() != GL_NO_ERROR);
}

static bool ensureNoGLError(const char* name)
{
  bool result = true;
  GLuint error;

  while ((error = glGetError()) != GL_NO_ERROR) {
    LOG("GL error [%s]: %40x\n", name, error);
    result = false;
  }

  return result;
}

AndroidGraphicBuffer::AndroidGraphicBuffer(PRUint32 width, PRUint32 height, PRUint32 usage,
                                           gfxImageFormat format) :
    mWidth(width)
  , mHeight(height)
  , mUsage(usage)
  , mFormat(format)
  , mHandle(0)
  , mEGLImage(0)
{
}

AndroidGraphicBuffer::~AndroidGraphicBuffer()
{
  DestroyBuffer();
}

void
AndroidGraphicBuffer::DestroyBuffer()
{
  /**
   * XXX: eglDestroyImageKHR crashes sometimes due to refcount badness (I think)
   *
   * If you look at egl.cpp (https://github.com/android/platform_frameworks_base/blob/master/opengl/libagl/egl.cpp#L2002)
   * you can see that eglCreateImageKHR just refs the native buffer, and eglDestroyImageKHR
   * just unrefs it. Somehow the ref count gets messed up and things are already destroyed
   * by the time eglDestroyImageKHR gets called. For now, at least, just not calling
   * eglDestroyImageKHR should be fine since we do free the GraphicBuffer below.
   *
   * Bug 712716
   */
#if 0
  if (mEGLImage) {
    if (sGLFunctions.EnsureInitialized()) {
      sGLFunctions.fDestroyImageKHR(sGLFunctions.fGetDisplay(EGL_DEFAULT_DISPLAY), mEGLImage);
      mEGLImage = NULL;
    }
  }
#endif
  mEGLImage = NULL;

  if (mHandle) {
    if (sGLFunctions.EnsureInitialized()) {
      sGLFunctions.fGraphicBufferDtor(mHandle);
    }
    free(mHandle);
    mHandle = NULL;
  }

}

bool
AndroidGraphicBuffer::EnsureBufferCreated()
{
  if (!mHandle) {
    mHandle = malloc(GRAPHIC_BUFFER_SIZE);
    sGLFunctions.fGraphicBufferCtor(mHandle, mWidth, mHeight, GetAndroidFormat(mFormat), GetAndroidUsage(mUsage));
  }

  return true;
}

bool
AndroidGraphicBuffer::EnsureInitialized()
{
  if (!sGLFunctions.EnsureInitialized()) {
    return false;
  }

  EnsureBufferCreated();
  return true;
}

int
AndroidGraphicBuffer::Lock(PRUint32 aUsage, unsigned char **bits)
{
  if (!EnsureInitialized())
    return true;

  return sGLFunctions.fGraphicBufferLock(mHandle, GetAndroidUsage(aUsage), bits);
}

int
AndroidGraphicBuffer::Lock(PRUint32 aUsage, const nsIntRect& aRect, unsigned char **bits)
{
  if (!EnsureInitialized())
    return false;

  ARect rect;
  rect.left = aRect.x;
  rect.top = aRect.y;
  rect.right = aRect.x + aRect.width;
  rect.bottom = aRect.y + aRect.height;

  return sGLFunctions.fGraphicBufferLockRect(mHandle, GetAndroidUsage(aUsage), rect, bits);
}

int
AndroidGraphicBuffer::Unlock()
{
  if (!EnsureInitialized())
    return false;

  return sGLFunctions.fGraphicBufferUnlock(mHandle);
}

bool
AndroidGraphicBuffer::Reallocate(PRUint32 aWidth, PRUint32 aHeight, gfxImageFormat aFormat)
{
  if (!EnsureInitialized())
    return false;

  mWidth = aWidth;
  mHeight = aHeight;
  mFormat = aFormat;

  // Sometimes GraphicBuffer::reallocate just doesn't work. In those cases we'll just allocate a brand
  // new buffer. If reallocate fails once, never try it again.
  if (!gTryRealloc || sGLFunctions.fGraphicBufferReallocate(mHandle, aWidth, aHeight, GetAndroidFormat(aFormat)) != 0) {
    DestroyBuffer();
    EnsureBufferCreated();

    gTryRealloc = false;
  }

  return true;
}

PRUint32
AndroidGraphicBuffer::GetAndroidUsage(PRUint32 aUsage)
{
  PRUint32 flags = 0;

  if (aUsage & UsageSoftwareRead) {
    flags |= GRALLOC_USAGE_SW_READ_OFTEN;
  }

  if (aUsage & UsageSoftwareWrite) {
    flags |= GRALLOC_USAGE_SW_WRITE_OFTEN;
  }

  if (aUsage & UsageTexture) {
    flags |= GRALLOC_USAGE_HW_TEXTURE;
  }

  if (aUsage & UsageTarget) {
    flags |= GRALLOC_USAGE_HW_RENDER;
  }

  if (aUsage & Usage2D) {
    flags |= GRALLOC_USAGE_HW_2D;
  }

  return flags;
}

PRUint32
AndroidGraphicBuffer::GetAndroidFormat(gfxImageFormat aFormat)
{
  switch (aFormat) {
    case gfxImageFormat::ImageFormatRGB24:
      return HAL_PIXEL_FORMAT_RGBX_8888;
    case gfxImageFormat::ImageFormatRGB16_565:
      return HAL_PIXEL_FORMAT_RGB_565;
    default:
      return 0;
  }
}

bool
AndroidGraphicBuffer::EnsureEGLImage()
{
  if (mEGLImage)
    return true;


  if (!EnsureInitialized())
    return false;

  EGLint eglImgAttrs[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE };
  void* nativeBuffer = sGLFunctions.fGraphicBufferGetNativeBuffer(mHandle);

  mEGLImage = sGLFunctions.fCreateImageKHR(sGLFunctions.fGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, (EGLClientBuffer)nativeBuffer, eglImgAttrs);
  return mEGLImage != NULL;
}

bool
AndroidGraphicBuffer::Bind()
{
  if (!EnsureInitialized())
    return false;

  if (!EnsureEGLImage()) {
    LOG("No valid EGLImage!");
    return false;
  }

  clearGLError();
  sGLFunctions.fImageTargetTexture2DOES(GL_TEXTURE_2D, mEGLImage);
  return ensureNoGLError("glEGLImageTargetTexture2DOES");
}

static const char* const sAllowedBoards[] = {
  "venus2",     // Motorola Droid Pro
  "tuna",       // Galaxy Nexus
  "omap4sdp",   // Amazon Kindle Fire
  "droid2",     // Motorola Droid 2
  "targa",      // Motorola Droid Bionic
  "spyder",     // Motorola Razr
  "shadow",     // Motorola Droid X
  "SGH-I897",   // Samsung Galaxy S
  "GT-I9100",   // Samsung Galaxy SII
  "sgh-i997",   // Samsung Infuse 4G
  "herring",    // Samsung Nexus S
  "sgh-t839",   // Samsung Sidekick 4G
  NULL
};

bool
AndroidGraphicBuffer::IsBlacklisted()
{
  nsAutoString board;
  if (!AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "BOARD", board))
    return true;

  const char* boardUtf8 = NS_ConvertUTF16toUTF8(board).get();

  if (Preferences::GetBool("direct-texture.force.enabled", false)) {
    LOG("allowing board '%s' due to prefs override", boardUtf8);
    return false;
  }

  if (Preferences::GetBool("direct-texture.force.disabled", false)) {
    LOG("disallowing board '%s' due to prefs override", boardUtf8);
    return true;
  }

  // FIXME: (Bug 722605) use something better than a linear search
  for (int i = 0; sAllowedBoards[i]; i++) {
    if (board.Find(sAllowedBoards[i]) >= 0) {
      LOG("allowing board '%s' based on '%s'\n", boardUtf8, sAllowedBoards[i]);
      return false;
    }
  }

  LOG("disallowing board: %s\n", boardUtf8);
  return true;
}

} /* mozilla */
