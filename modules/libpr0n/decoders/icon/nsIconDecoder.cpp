/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *
 */

#include "nsIconDecoder.h"
#include "nsIInputStream.h"
#include "imgIContainer.h"
#include "imgIContainerObserver.h"
#include "imgILoad.h"
#include "nspr.h"
#include "nsIComponentManager.h"
#include "nsRect.h"

NS_IMPL_THREADSAFE_ADDREF(nsIconDecoder);
NS_IMPL_THREADSAFE_RELEASE(nsIconDecoder);

NS_INTERFACE_MAP_BEGIN(nsIconDecoder)
   NS_INTERFACE_MAP_ENTRY(imgIDecoder)
NS_INTERFACE_MAP_END_THREADSAFE

nsIconDecoder::nsIconDecoder()
{
  NS_INIT_ISUPPORTS();
}

nsIconDecoder::~nsIconDecoder()
{ }


/** imgIDecoder methods **/

NS_IMETHODIMP nsIconDecoder::Init(imgILoad *aLoad)
{
  mObserver = do_QueryInterface(aLoad);  // we're holding 2 strong refs to the request.

  mImage = do_CreateInstance("@mozilla.org/image/container;1");
  if (!mImage) return NS_ERROR_OUT_OF_MEMORY;

  aLoad->SetImage(mImage);                                                   

  mFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
  if (!mFrame) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP nsIconDecoder::Close()
{
  if (mObserver) 
  {
    mObserver->OnStopFrame(nsnull, nsnull, mFrame);
    mObserver->OnStopContainer(nsnull, nsnull, mImage);
    mObserver->OnStopDecode(nsnull, nsnull, NS_OK, nsnull);
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsIconDecoder::Flush()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsIconDecoder::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
  nsresult rv;

  char *buf = (char *)PR_Malloc(count);
  if (!buf) return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
 
  // read the data from the input stram...
  PRUint32 readLen;
  rv = inStr->Read(buf, count, &readLen);

  char *data = buf;

  if (NS_FAILED(rv)) return rv;

  // since WriteFrom is only called once, go ahead and fire the on start notifications..

  mObserver->OnStartDecode(nsnull, nsnull);
  PRUint32 i = 0;
  // Read size
  PRInt32 w, h;
  w = data[0];
  h = data[1];

  data += 2;

  readLen -= i + 2;

  mImage->Init(w, h, mObserver);
  if (mObserver)
    mObserver->OnStartContainer(nsnull, nsnull, mImage);

  mFrame->Init(0, 0, w, h, gfxIFormats::RGB_A1);
  mImage->AppendFrame(mFrame);
  if (mObserver)
    mObserver->OnStartFrame(nsnull, nsnull, mFrame);
  
  PRUint32 bpr, abpr;
  nscoord width, height;
  mFrame->GetImageBytesPerRow(&bpr);
  mFrame->GetAlphaBytesPerRow(&abpr);
  mFrame->GetWidth(&width);
  mFrame->GetHeight(&height);
  
  i = 0;
  PRInt32 rownum = 0;  // XXX this better not have a decimal
  
  PRInt32 wroteLen = 0;

  do 
  {
    PRUint8 *line = (PRUint8*)data + i*bpr;
    mFrame->SetImageData(line, bpr, (rownum++)*bpr);

    nsRect r(0, rownum, width, 1);
    mObserver->OnDataAvailable(nsnull, nsnull, mFrame, &r);

    wroteLen += bpr ;
    i++;
  } while(rownum < height);


 // now we want to send in the alpha data...
 for (rownum = 0; rownum < height; rownum ++)
 {
  PRUint8 * line = (PRUint8*) data + abpr * rownum + height*bpr;
  mFrame->SetAlphaData(line, abpr, (rownum)*abpr);   
 } 

  PR_FREEIF(buf);

  return NS_OK;
}

