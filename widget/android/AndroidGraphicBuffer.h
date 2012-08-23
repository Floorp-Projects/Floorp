/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidGraphicBuffer_h_
#define AndroidGraphicBuffer_h_

#include "gfxASurface.h"
#include "nsRect.h"

typedef void* EGLImageKHR;
typedef void* EGLClientBuffer;

namespace mozilla {

/**
 * This class allows access to Android's direct texturing mechanism. Locking
 * the buffer gives you a pointer you can read/write to directly. It is fully
 * threadsafe, but you probably really want to use the AndroidDirectTexture
 * class which will handle double buffering.
 *
 * In order to use the buffer in OpenGL, just call Bind() and it will attach
 * to whatever texture is bound to GL_TEXTURE_2D.
 */
class AndroidGraphicBuffer
{
public:
  enum {
    UsageSoftwareRead = 1,
    UsageSoftwareWrite = 1 << 1,
    UsageTexture = 1 << 2,
    UsageTarget = 1 << 3,
    Usage2D = 1 << 4
  };

  AndroidGraphicBuffer(uint32_t width, uint32_t height, uint32_t usage, gfxASurface::gfxImageFormat format);
  virtual ~AndroidGraphicBuffer();

  int Lock(uint32_t usage, unsigned char **bits);
  int Lock(uint32_t usage, const nsIntRect& rect, unsigned char **bits);
  int Unlock();
  bool Reallocate(uint32_t aWidth, uint32_t aHeight, gfxASurface::gfxImageFormat aFormat);

  uint32_t Width() { return mWidth; }
  uint32_t Height() { return mHeight; }

  bool Bind();

  static bool IsBlacklisted();

private:
  uint32_t mWidth;
  uint32_t mHeight;
  uint32_t mUsage;
  gfxASurface::gfxImageFormat mFormat;

  bool EnsureInitialized();
  bool EnsureEGLImage();

  void DestroyBuffer();
  bool EnsureBufferCreated();

  uint32_t GetAndroidUsage(uint32_t aUsage);
  uint32_t GetAndroidFormat(gfxASurface::gfxImageFormat aFormat);

  void *mHandle;
  void *mEGLImage;
};

} /* mozilla */
#endif /* AndroidGraphicBuffer_h_ */
