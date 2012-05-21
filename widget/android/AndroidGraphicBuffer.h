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

  AndroidGraphicBuffer(PRUint32 width, PRUint32 height, PRUint32 usage, gfxASurface::gfxImageFormat format);
  virtual ~AndroidGraphicBuffer();

  int Lock(PRUint32 usage, unsigned char **bits);
  int Lock(PRUint32 usage, const nsIntRect& rect, unsigned char **bits);
  int Unlock();
  bool Reallocate(PRUint32 aWidth, PRUint32 aHeight, gfxASurface::gfxImageFormat aFormat);

  PRUint32 Width() { return mWidth; }
  PRUint32 Height() { return mHeight; }

  bool Bind();

  static bool IsBlacklisted();

private:
  PRUint32 mWidth;
  PRUint32 mHeight;
  PRUint32 mUsage;
  gfxASurface::gfxImageFormat mFormat;

  bool EnsureInitialized();
  bool EnsureEGLImage();

  void DestroyBuffer();
  bool EnsureBufferCreated();

  PRUint32 GetAndroidUsage(PRUint32 aUsage);
  PRUint32 GetAndroidFormat(gfxASurface::gfxImageFormat aFormat);

  void *mHandle;
  void *mEGLImage;
};

} /* mozilla */
#endif /* AndroidGraphicBuffer_h_ */
