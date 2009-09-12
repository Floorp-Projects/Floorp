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
#include "imgIContainer.h"
#include "imgIContainerObserver.h"
#include "nspr.h"
#include "nsIComponentManager.h"
#include "nsRect.h"
#include "nsComponentManagerUtils.h"

#include "nsIInterfaceRequestorUtils.h"
#include "ImageErrors.h"

NS_IMPL_THREADSAFE_ADDREF(nsIconDecoder)
NS_IMPL_THREADSAFE_RELEASE(nsIconDecoder)

NS_INTERFACE_MAP_BEGIN(nsIconDecoder)
   NS_INTERFACE_MAP_ENTRY(imgIDecoder)
NS_INTERFACE_MAP_END_THREADSAFE


nsIconDecoder::nsIconDecoder() :
  mImage(nsnull),
  mObserver(nsnull),
  mFlags(imgIDecoder::DECODER_FLAG_NONE),
  mWidth(-1),
  mHeight(-1),
  mPixBytesRead(0),
  mPixBytesTotal(0),
  mImageData(nsnull),
  mState(iconStateStart),
  mNotifiedDone(PR_FALSE)
{
  // Nothing to do
}

nsIconDecoder::~nsIconDecoder()
{ }


/** imgIDecoder methods **/

NS_IMETHODIMP nsIconDecoder::Init(imgIContainer *aImage,
                                  imgIDecoderObserver *aObserver,
                                  PRUint32 aFlags)
{

  // Grab parameters
  mImage = aImage;
  mObserver = aObserver;
  mFlags = aFlags;

  // Fire OnStartDecode at init time to support bug 512435
  if (!(mFlags & imgIDecoder::DECODER_FLAG_HEADERONLY) && mObserver)
    mObserver->OnStartDecode(nsnull);

  return NS_OK;
}

NS_IMETHODIMP nsIconDecoder::Close(PRUint32 aFlags)
{
  // If we haven't notified of completion yet for a full/success decode, we
  // didn't finish. Notify in error mode
  if (!(aFlags & CLOSE_FLAG_DONTNOTIFY) &&
      !(mFlags & imgIDecoder::DECODER_FLAG_HEADERONLY) &&
      !mNotifiedDone)
    NotifyDone(/* aSuccess = */ PR_FALSE);

  mImage = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsIconDecoder::Flush()
{
  return NS_OK;
}

static nsresult
WriteIconData(nsIInputStream *aInStream, void *aClosure, const char *aFromSegment,
              PRUint32 aToOffset, PRUint32 aCount, PRUint32 *aWriteCount)
{
  nsresult rv;

  // We always read everything
  *aWriteCount = aCount;

  // We put this here to avoid errors about crossing initialization with case
  // jumps on linux.
  PRUint32 bytesToRead = 0;

  // Grab the parameters
  nsIconDecoder *decoder = static_cast<nsIconDecoder*>(aClosure);

  // Performance isn't critical here, so our update rectangle is 
  // always the full icon
  nsIntRect r(0, 0, decoder->mWidth, decoder->mHeight);

  // Loop until the input data is gone
  while (aCount > 0) {
    switch (decoder->mState) {
      case iconStateStart:

        // Grab the width
        decoder->mWidth = (PRUint8)*aFromSegment;

        // Book Keeping
        aFromSegment++;
        aCount--;
        decoder->mState = iconStateHaveHeight;
        break;

      case iconStateHaveHeight:

        // Grab the Height
        decoder->mHeight = (PRUint8)*aFromSegment;

        // Set up the container and signal
        decoder->mImage->SetSize(decoder->mWidth,
                                 decoder->mHeight);
        if (decoder->mObserver)
          decoder->mObserver->OnStartContainer(nsnull, decoder->mImage);

        // If We're doing a header-only decode, we're done
        if (decoder->mFlags & imgIDecoder::DECODER_FLAG_HEADERONLY) {
          decoder->mState = iconStateFinished;
          break;
        }

        // Add the frame and signal
        rv = decoder->mImage->AppendFrame(0, 0,
                                          decoder->mWidth,
                                          decoder->mHeight,
                                          gfxASurface::ImageFormatARGB32,
                                          &decoder->mImageData, 
                                          &decoder->mPixBytesTotal);
        if (NS_FAILED(rv)) {
          decoder->mState = iconStateError;
          return rv;
        }
        if (decoder->mObserver)
          decoder->mObserver->OnStartFrame(nsnull, 0);

        // Book Keeping
        aFromSegment++;
        aCount--;
        decoder->mState = iconStateReadPixels;
        break;

      case iconStateReadPixels:

        // How many bytes are we reading?
        bytesToRead = PR_MAX(aCount,
                             decoder->mPixBytesTotal - decoder->mPixBytesRead);

        // Copy the bytes
        memcpy(decoder->mImageData + decoder->mPixBytesRead,
               aFromSegment, bytesToRead);

        // Notify
        rv = decoder->mImage->FrameUpdated(0, r);
        if (NS_FAILED(rv)) {
          decoder->mState = iconStateError;
          return rv;
        }
        if (decoder->mObserver)
          decoder->mObserver->OnDataAvailable(nsnull, PR_TRUE, &r);

        // Book Keeping
        aFromSegment += bytesToRead;
        aCount -= bytesToRead;
        decoder->mPixBytesRead += bytesToRead;

        // If we've got all the pixel bytes, we're finished
        if (decoder->mPixBytesRead == decoder->mPixBytesTotal) {
          decoder->NotifyDone(/* aSuccess = */ PR_TRUE);
          decoder->mState = iconStateFinished;
        }
        break;

      case iconStateFinished:

        // Consume all excess data silently
        aCount = 0;

        break;

      case iconStateError:
        return NS_IMAGELIB_ERROR_FAILURE;
        break;
    }
  }

  return NS_OK;
}

void
nsIconDecoder::NotifyDone(PRBool aSuccess)
{
  // We should only call this once
  NS_ABORT_IF_FALSE(!mNotifiedDone, "Calling NotifyDone twice");

  // Notify
  if (mObserver)
    mObserver->OnStopFrame(nsnull, 0);
  if (aSuccess)
    mImage->DecodingComplete();
  if (mObserver) {
    mObserver->OnStopContainer(nsnull, mImage);
    mObserver->OnStopDecode(nsnull, aSuccess ? NS_OK : NS_ERROR_FAILURE,
                            nsnull);
  }

  // Flag that we've notified
  mNotifiedDone = PR_TRUE;
}


NS_IMETHODIMP nsIconDecoder::WriteFrom(nsIInputStream *inStr, PRUint32 count)
{
  // Decode, watching for errors.
  nsresult rv = NS_OK;
  PRUint32 ignored;
  if (mState != iconStateError)
    rv = inStr->ReadSegments(WriteIconData, this, count, &ignored);
  if ((mState == iconStateError) || NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  return NS_OK;
}

