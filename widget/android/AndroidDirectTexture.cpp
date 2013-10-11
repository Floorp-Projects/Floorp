/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <unistd.h>
#include <errno.h>
#include "AndroidDirectTexture.h"
#include "nsRect.h"

namespace mozilla {

AndroidDirectTexture::AndroidDirectTexture(uint32_t width, uint32_t height, uint32_t usage,
                                           gfxImageFormat format) :
    mLock("AndroidDirectTexture.mLock")
  , mNeedFlip(false)
  , mWidth(width)
  , mHeight(height)
  , mFormat(format)
  , mPendingReallocBuffer(nullptr)
{
  mFrontBuffer = new AndroidGraphicBuffer(width, height, usage, format);
  mBackBuffer = new AndroidGraphicBuffer(width, height, usage, format);
}

AndroidDirectTexture::~AndroidDirectTexture()
{
  if (mFrontBuffer) {
    delete mFrontBuffer;
    mFrontBuffer = nullptr;
  }

  if (mBackBuffer) {
    delete mBackBuffer;
    mBackBuffer = nullptr;
  }
}

void
AndroidDirectTexture::ReallocPendingBuffer()
{
  // We may have reallocated while the current back buffer was being
  // used as the front buffer. If we have such a reallocation pending
  // and the current back buffer is the target buffer, do it now.
  //
  // It is assumed that mLock is already acquired
  if (mPendingReallocBuffer == mBackBuffer) {
    mBackBuffer->Reallocate(mWidth, mHeight, mFormat);
    mPendingReallocBuffer = nullptr;
  }
}

bool
AndroidDirectTexture::Lock(uint32_t aUsage, unsigned char **bits)
{
  nsIntRect rect(0, 0, mWidth, mHeight);
  return Lock(aUsage, rect, bits);
}

bool
AndroidDirectTexture::Lock(uint32_t aUsage, const nsIntRect& aRect, unsigned char **bits)
{
  mLock.Lock();
  ReallocPendingBuffer();

  int result;
  while ((result = mBackBuffer->Lock(aUsage, aRect, bits)) == -EBUSY) {
    usleep(1000); // 1ms
  }

  return result == 0;
}

bool
AndroidDirectTexture::Unlock(bool aFlip)
{
  if (aFlip) {
    mNeedFlip = true;
  }

  bool result = mBackBuffer->Unlock();
  mLock.Unlock();

  return result;
}

bool
AndroidDirectTexture::Reallocate(uint32_t aWidth, uint32_t aHeight) {
  return Reallocate(aWidth, aHeight, mFormat);
}

bool
AndroidDirectTexture::Reallocate(uint32_t aWidth, uint32_t aHeight, gfxImageFormat aFormat)
{
  MutexAutoLock lock(mLock);

  // We only reallocate the current back buffer. The front buffer is likely
  // in use, so we'll reallocate it on the first Lock() after it is rotated
  // to the back.
  bool result = mBackBuffer->Reallocate(aWidth, aHeight, aFormat);
  if (result) {
    mPendingReallocBuffer = mFrontBuffer;

    mWidth = aWidth;
    mHeight = aHeight;
  }

  return result;
}

bool
AndroidDirectTexture::Bind()
{
  MutexAutoLock lock(mLock);

  if (mNeedFlip) {
    AndroidGraphicBuffer* tmp = mBackBuffer;
    mBackBuffer = mFrontBuffer;
    mFrontBuffer = tmp;
    mNeedFlip = false;
  }

  return mFrontBuffer->Bind();
}

} /* mozilla */
