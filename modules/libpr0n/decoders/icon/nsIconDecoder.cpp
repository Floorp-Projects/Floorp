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
#include "imgILoad.h"
#include "nspr.h"
#include "nsIComponentManager.h"
#include "nsRect.h"
#include "nsComponentManagerUtils.h"

#include "nsIInterfaceRequestorUtils.h"

NS_IMPL_THREADSAFE_ADDREF(nsIconDecoder)
NS_IMPL_THREADSAFE_RELEASE(nsIconDecoder)

NS_INTERFACE_MAP_BEGIN(nsIconDecoder)
   NS_INTERFACE_MAP_ENTRY(imgIDecoder)
NS_INTERFACE_MAP_END_THREADSAFE

nsIconDecoder::nsIconDecoder()
{
}

nsIconDecoder::~nsIconDecoder()
{ }


/** imgIDecoder methods **/

NS_IMETHODIMP nsIconDecoder::Init(imgILoad *aLoad)
{
  mObserver = do_QueryInterface(aLoad);  // we're holding 2 strong refs to the request.

  mImage = do_CreateInstance("@mozilla.org/image/container;2");
  if (!mImage) return NS_ERROR_OUT_OF_MEMORY;

  aLoad->SetImage(mImage);                                                   

  return NS_OK;
}

NS_IMETHODIMP nsIconDecoder::Close()
{
  mImage->DecodingComplete();

  if (mObserver) 
  {
    mObserver->OnStopFrame(nsnull, 0);
    mObserver->OnStopContainer(nsnull, mImage);
    mObserver->OnStopDecode(nsnull, NS_OK, nsnull);
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsIconDecoder::Flush()
{
  return NS_OK;
}

NS_IMETHODIMP nsIconDecoder::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
  // read the header from the input stram...
  PRUint32 readLen;
  PRUint8 header[2];
  nsresult rv = inStr->Read((char*)header, 2, &readLen);
  NS_ENSURE_TRUE(readLen == 2, NS_ERROR_UNEXPECTED); // w, h
  count -= 2;

  PRInt32 w = header[0];
  PRInt32 h = header[1];
  NS_ENSURE_TRUE(w > 0 && h > 0, NS_ERROR_UNEXPECTED);

  if (mObserver)
    mObserver->OnStartDecode(nsnull);
  mImage->Init(w, h, mObserver);
  if (mObserver)
    mObserver->OnStartContainer(nsnull, mImage);

  PRUint32 imageLen;
  PRUint8 *imageData;

  rv = mImage->AppendFrame(0, 0, w, h, gfxASurface::ImageFormatARGB32, &imageData, &imageLen);
  if (NS_FAILED(rv))
    return rv;

  if (mObserver)
    mObserver->OnStartFrame(nsnull, 0);

  // Ensure that there enough in the inputStream
  NS_ENSURE_TRUE(count >= imageLen, NS_ERROR_UNEXPECTED);

  // Read the image data direct into the frame data
  rv = inStr->Read((char*)imageData, imageLen, &readLen);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(readLen == imageLen, NS_ERROR_UNEXPECTED);

  // Notify the image...
  nsIntRect r(0, 0, w, h);
  rv = mImage->FrameUpdated(0, r);
  if (NS_FAILED(rv))
    return rv;

  mObserver->OnDataAvailable(nsnull, PR_TRUE, &r);

  return NS_OK;
}

