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

/*
 *   nsGIFDecoder.cpp --- interface to gif decoder
 */

#include "nsGIFDecoder.h"
#include "nsCOMPtr.h"

//////////////////////////////////////////////////////////////////////
// GIF Decoder Implementation

NS_IMPL_ISUPPORTS1(GIFDecoder, nsIImgDecoder);

GIFDecoder::GIFDecoder(il_container* aContainer)
{
  NS_INIT_REFCNT();
  ilContainer = aContainer;
}

GIFDecoder::~GIFDecoder(void)
{
}


NS_METHOD
GIFDecoder::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  il_container *ic = new il_container();
  if (!ic) return NS_ERROR_OUT_OF_MEMORY;

  GIFDecoder *decoder = new GIFDecoder(ic);
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


NS_IMETHODIMP
GIFDecoder::ImgDInit()
{
   PRBool ret=PR_FALSE;

   if(ilContainer != NULL) {
     ret=il_gif_init(ilContainer);
   } 
   if(ret)
       return NS_OK;
   else
   return NS_ERROR_FAILURE;  
}


NS_IMETHODIMP 
GIFDecoder::ImgDWriteReady(PRUint32 *max_read)
{

  if(ilContainer != NULL) {
     *max_read = il_gif_write_ready(ilContainer);
  }
  return NS_OK;
}

NS_IMETHODIMP
GIFDecoder::ImgDWrite(const unsigned char *buf, int32 len)
{
  int ret = 0;

  if( ilContainer != NULL ) {
     ret = il_gif_write(ilContainer, buf,len);
     if(ret != 0)
         return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP 
GIFDecoder::ImgDComplete()
{
  if( ilContainer != NULL ) {
     il_gif_complete(ilContainer);
  }
  return NS_OK;
}

NS_IMETHODIMP 
GIFDecoder::ImgDAbort()
{
  if( ilContainer != NULL ) {
    il_gif_abort(ilContainer);
  }
  return NS_OK;
}
 

