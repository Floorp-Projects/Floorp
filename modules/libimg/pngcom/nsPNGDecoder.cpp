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
 *   nsPNGDecoder.cpp --- interface to png decoder
 */


#include "nsPNGDecoder.h"
#include "pngdec.h"

/*-----------class----------------*/
/*-------------------------------------------------*/

PNGDecoder::PNGDecoder(il_container* aContainer)
{
  NS_INIT_REFCNT();
  ilContainer = aContainer;
};


PNGDecoder::~PNGDecoder(void)
{
};


NS_IMPL_ISUPPORTS1(PNGDecoder, nsIImgDecoder)

NS_METHOD
PNGDecoder::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  il_container *ic = new il_container();
  if (!ic) return NS_ERROR_OUT_OF_MEMORY;

  PNGDecoder *decoder = new PNGDecoder(ic);
  if (!decoder) {
    delete ic;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(decoder);
  rv = decoder->QueryInterface(aIID, aResult);
  NS_RELEASE(decoder);


  /* why are we creating and destroying this object for no reason? */
  delete ic; /* is a place holder */

  return rv;
}


/*----------------------------------------------------*/
// api functions
/*------------------------------------------------------*/
NS_IMETHODIMP
PNGDecoder::ImgDInit()
{
  int ret;

  if( ilContainer != NULL ) {
     ret = il_png_init(ilContainer);
     if(!ret)
         return NS_ERROR_FAILURE;
  }
  return NS_OK;
}


NS_IMETHODIMP 
PNGDecoder::ImgDWriteReady(PRUint32 *max_read)
{
  /* dummy return needed */
  *max_read = 2048; 
  return NS_OK;
}

NS_IMETHODIMP
PNGDecoder::ImgDWrite(const unsigned char *buf, int32 len)
{
  int ret;

  if( ilContainer != NULL ) {
     ret = il_png_write(ilContainer, buf,len);
     if(ret != 0)
         return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP 
PNGDecoder::ImgDComplete()
{
  int ret;

  if( ilContainer != NULL ) {
     ret = il_png_complete(ilContainer);
     if(ret != 0)
         return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP 
PNGDecoder::ImgDAbort()
{
  int ret;

  if( ilContainer != NULL ) {
     ret = il_png_abort(ilContainer);
     if(ret != 0)
         return NS_ERROR_FAILURE;
  }
  return NS_OK;
}
 

