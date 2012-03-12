/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   James Willcox <jwillcox@mozilla.com>
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

#include <unistd.h>
#include <errno.h>
#include "AndroidDirectTexture.h"

typedef gfxASurface::gfxImageFormat gfxImageFormat;

namespace mozilla {

AndroidDirectTexture::AndroidDirectTexture(PRUint32 width, PRUint32 height, PRUint32 usage,
                                           gfxImageFormat format) :
    mLock("AndroidDirectTexture.mLock")
  , mNeedFlip(false)
  , mWidth(width)
  , mHeight(height)
  , mFormat(format)
  , mPendingReallocBuffer(NULL)
{
  mFrontBuffer = new AndroidGraphicBuffer(width, height, usage, format);
  mBackBuffer = new AndroidGraphicBuffer(width, height, usage, format);
}

AndroidDirectTexture::~AndroidDirectTexture()
{
  if (mFrontBuffer) {
    delete mFrontBuffer;
    mFrontBuffer = NULL;
  }

  if (mBackBuffer) {
    delete mBackBuffer;
    mBackBuffer = NULL;
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
    mPendingReallocBuffer = NULL;
  }
}

bool
AndroidDirectTexture::Lock(PRUint32 aUsage, unsigned char **bits)
{
  nsIntRect rect(0, 0, mWidth, mHeight);
  return Lock(aUsage, rect, bits);
}

bool
AndroidDirectTexture::Lock(PRUint32 aUsage, const nsIntRect& aRect, unsigned char **bits)
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
AndroidDirectTexture::Reallocate(PRUint32 aWidth, PRUint32 aHeight) {
  return Reallocate(aWidth, aHeight, mFormat);
}

bool
AndroidDirectTexture::Reallocate(PRUint32 aWidth, PRUint32 aHeight, gfxASurface::gfxImageFormat aFormat)
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
AndroidDirectTexture::Bind(GLenum target)
{
  MutexAutoLock lock(mLock);

  if (mNeedFlip) {
    AndroidGraphicBuffer* tmp = mBackBuffer;
    mBackBuffer = mFrontBuffer;
    mFrontBuffer = tmp;
    mNeedFlip = false;
  }

  return mFrontBuffer->Bind(target);
}

} /* mozilla */
