/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* -*- Mode: C; tab-width: 4 -*-
 *   nsJPGDecoder.cpp --- interface to gif decoder
 */


#include "nsCOMPtr.h"
#include "nsJPGDecoder.h"
#include "jpeg.h"

/*-----------class----------------*/
/*-------------------------------------------------*/

JPGDecoder::JPGDecoder(il_container* aContainer)
{
  NS_INIT_REFCNT();
  ilContainer = aContainer;
};


JPGDecoder::~JPGDecoder(void)
{
};

NS_IMPL_ISUPPORTS(JPGDecoder, NS_GET_IID(nsIImgDecoder))

/*------------------------------------------------------*/
/* api functions
 */
/*------------------------------------------------------*/
NS_IMETHODIMP
JPGDecoder::ImgDInit()
{
  int ret;

  if( ilContainer != NULL ) {
     ret = il_jpeg_init(ilContainer);
     if(ret != 1)
         return NS_ERROR_FAILURE;
  }
  return NS_OK;
}


NS_IMETHODIMP 
JPGDecoder::ImgDWriteReady(PRUint32 *max_read)
{
  /* dummy return needed */
	*max_read = 2048; 
  return NS_OK; 
}

NS_IMETHODIMP
JPGDecoder::ImgDWrite(const unsigned char *buf, int32 len)
{
  int ret;

  if( ilContainer != NULL ) {
     ret = il_jpeg_write(ilContainer, buf,len);
     if(ret != 0)
         return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP 
JPGDecoder::ImgDComplete()
{
  if( ilContainer != NULL ) {
     il_jpeg_complete(ilContainer);
  }
  return NS_OK;
}

NS_IMETHODIMP 
JPGDecoder::ImgDAbort()
{
  if( ilContainer != NULL ) {
     il_jpeg_abort(ilContainer);
  }
  return NS_OK;

}
 
