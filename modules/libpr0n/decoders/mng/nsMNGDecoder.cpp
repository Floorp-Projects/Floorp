/*
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
 * The Initial Developer of the Original Code is Tim Rowley.
 * Portions created by Tim Rowley are 
 * Copyright (C) 2001 Tim Rowley.  Rights Reserved.
 * 
 * Contributor(s): 
 *   Tim Rowley <tor@cs.brown.edu>
 */

#include "nsMNGDecoder.h"
#include "nsIInputStream.h"
#include "nsIServiceManager.h"
#include "imgContainerMNG.h"

//////////////////////////////////////////////////////////////////////////////
// MNG Decoder Implementation

NS_IMPL_ISUPPORTS1(nsMNGDecoder, imgIDecoder);

nsMNGDecoder::nsMNGDecoder()
{
  NS_INIT_ISUPPORTS();
}

nsMNGDecoder::~nsMNGDecoder()
{
}

//////////////////////////////////////////////////////////////////////////////
//* imgIDecoder methods **/
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// void init (in imgILoad aLoad); */
NS_IMETHODIMP
nsMNGDecoder::Init(imgILoad *aLoad)
{
  if(!aLoad) return NS_ERROR_FAILURE;
  
  mObserver = 
    do_QueryInterface(aLoad);  // we're holding 2 strong refs to the request
  if(!mObserver) return NS_ERROR_FAILURE;

  mImageContainer = 
    do_CreateInstance("@mozilla.org/image/container;1?type=image/x-mng");
  if(!mImageContainer) return NS_ERROR_OUT_OF_MEMORY;
  aLoad->SetImage(mImageContainer);
  
  /* do MNG init stuff */
  NS_REINTERPRET_CAST(imgContainerMNG*, mImageContainer.get())->InitMNG(this);

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
// void close (); 
NS_IMETHODIMP
nsMNGDecoder::Close()
{
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
/* void flush (); */
NS_IMETHODIMP
nsMNGDecoder::Flush()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//////////////////////////////////////////////////////////////////////////////
// unsigned long writeFrom (in nsIInputStream inStr, in unsigned long count);
NS_IMETHODIMP
nsMNGDecoder::WriteFrom(nsIInputStream *inStr, 
			PRUint32 count,
			PRUint32 *_retval)
{
  NS_REINTERPRET_CAST(imgContainerMNG*, mImageContainer.get())->WriteMNG(inStr, count, _retval);
  return NS_OK;
}

