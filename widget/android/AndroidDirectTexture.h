/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidDirectTexture_h_
#define AndroidDirectTexture_h_

#include "gfxASurface.h"
#include "nsRect.h"
#include "mozilla/Mutex.h"
#include "AndroidGraphicBuffer.h"

namespace mozilla {

/**
 * This is a thread safe wrapper around AndroidGraphicBuffer that handles
 * double buffering. Each call to Bind() flips the buffer when necessary.
 *
 * You need to be careful when destroying an instance of this class. If either
 * buffer is locked by the application of the driver/hardware, bad things will
 * happen. Be sure that the OpenGL texture is no longer on the screen.
 */
class AndroidDirectTexture
{
public:
  AndroidDirectTexture(PRUint32 width, PRUint32 height, PRUint32 usage, gfxASurface::gfxImageFormat format);
  virtual ~AndroidDirectTexture();

  bool Lock(PRUint32 usage, unsigned char **bits);
  bool Lock(PRUint32 usage, const nsIntRect& rect, unsigned char **bits);
  bool Unlock(bool aFlip = true);

  bool Reallocate(PRUint32 aWidth, PRUint32 aHeight);
  bool Reallocate(PRUint32 aWidth, PRUint32 aHeight, gfxASurface::gfxImageFormat aFormat);

  PRUint32 Width() { return mWidth; }
  PRUint32 Height() { return mHeight; }

  bool Bind();

private:
  mozilla::Mutex mLock;
  bool mNeedFlip;

  PRUint32 mWidth;
  PRUint32 mHeight;
  gfxASurface::gfxImageFormat mFormat;

  AndroidGraphicBuffer* mFrontBuffer;
  AndroidGraphicBuffer* mBackBuffer;

  AndroidGraphicBuffer* mPendingReallocBuffer;
  void ReallocPendingBuffer();
};

} /* mozilla */
#endif /* AndroidDirectTexture_h_ */
