/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidDirectTexture_h_
#define AndroidDirectTexture_h_

#include "gfxTypes.h"
#include "mozilla/Mutex.h"
#include "AndroidGraphicBuffer.h"
#include "nsRect.h"

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
  AndroidDirectTexture(uint32_t width, uint32_t height, uint32_t usage, gfxImageFormat format);
  virtual ~AndroidDirectTexture();

  bool Lock(uint32_t usage, unsigned char **bits);
  bool Lock(uint32_t usage, const nsIntRect& rect, unsigned char **bits);
  bool Unlock(bool aFlip = true);

  bool Reallocate(uint32_t aWidth, uint32_t aHeight);
  bool Reallocate(uint32_t aWidth, uint32_t aHeight, gfxImageFormat aFormat);

  uint32_t Width() { return mWidth; }
  uint32_t Height() { return mHeight; }

  bool Bind();

private:
  mozilla::Mutex mLock;
  bool mNeedFlip;

  uint32_t mWidth;
  uint32_t mHeight;
  gfxImageFormat mFormat;

  AndroidGraphicBuffer* mFrontBuffer;
  AndroidGraphicBuffer* mBackBuffer;

  AndroidGraphicBuffer* mPendingReallocBuffer;
  void ReallocPendingBuffer();
};

} /* mozilla */
#endif /* AndroidDirectTexture_h_ */
