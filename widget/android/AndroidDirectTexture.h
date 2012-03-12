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

  bool Bind(GLenum target);

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
