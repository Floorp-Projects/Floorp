/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http:/www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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

NS_IMPL_ISUPPORTS(PNGDecoder, NS_GET_IID(nsIImgDecoder))

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
 

