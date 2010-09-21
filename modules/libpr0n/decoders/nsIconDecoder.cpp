/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Bobby Holley <bobbyholley@gmail.com>
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

#include "nsIconDecoder.h"
#include "nsIInputStream.h"
#include "RasterImage.h"
#include "imgIContainerObserver.h"
#include "nspr.h"
#include "nsRect.h"

#include "ImageErrors.h"

namespace mozilla {
namespace imagelib {

nsIconDecoder::nsIconDecoder() :
  mWidth(-1),
  mHeight(-1),
  mPixBytesRead(0),
  mPixBytesTotal(0),
  mImageData(nsnull),
  mState(iconStateStart)
{
  // Nothing to do
}

nsIconDecoder::~nsIconDecoder()
{ }

void
nsIconDecoder::WriteInternal(const char *aBuffer, PRUint32 aCount)
{
  NS_ABORT_IF_FALSE(!HasError(), "Shouldn't call WriteInternal after error!");

  // We put this here to avoid errors about crossing initialization with case
  // jumps on linux.
  PRUint32 bytesToRead = 0;
  nsresult rv;

  // Performance isn't critical here, so our update rectangle is 
  // always the full icon
  nsIntRect r(0, 0, mWidth, mHeight);

  // Loop until the input data is gone
  while (aCount > 0) {
    switch (mState) {
      case iconStateStart:

        // Grab the width
        mWidth = (PRUint8)*aBuffer;

        // Book Keeping
        aBuffer++;
        aCount--;
        mState = iconStateHaveHeight;
        break;

      case iconStateHaveHeight:

        // Grab the Height
        mHeight = (PRUint8)*aBuffer;

        // Post our size to the superclass
        PostSize(mWidth, mHeight);

        // If We're doing a size decode, we're done
        if (IsSizeDecode()) {
          mState = iconStateFinished;
          break;
        }

        // Add the frame and signal
        rv = mImage->AppendFrame(0, 0, mWidth, mHeight,
                                 gfxASurface::ImageFormatARGB32,
                                 &mImageData, &mPixBytesTotal);
        if (NS_FAILED(rv)) {
          PostDecoderError(rv);
          return;
        }

        // Tell the superclass we're starting a frame
        PostFrameStart();

        // Book Keeping
        aBuffer++;
        aCount--;
        mState = iconStateReadPixels;
        break;

      case iconStateReadPixels:

        // How many bytes are we reading?
        bytesToRead = PR_MIN(aCount, mPixBytesTotal - mPixBytesRead);

        // Copy the bytes
        memcpy(mImageData + mPixBytesRead, aBuffer, bytesToRead);

        // Invalidate
        PostInvalidation(r);

        // Book Keeping
        aBuffer += bytesToRead;
        aCount -= bytesToRead;
        mPixBytesRead += bytesToRead;

        // If we've got all the pixel bytes, we're finished
        if (mPixBytesRead == mPixBytesTotal) {
          PostFrameStop();
          PostDecodeDone();
          mState = iconStateFinished;
        }
        break;

      case iconStateFinished:

        // Consume all excess data silently
        aCount = 0;

        break;
    }
  }
}

} // namespace imagelib
} // namespace mozilla
