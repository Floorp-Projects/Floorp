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


#include "nsIImgDecoder.h" // include if_struct.h Needs to be first
#include "prmem.h"
#include "merrors.h"


#include "dllcompat.h"
#include "pngdec.h"
#include "nsPNGDecoder.h"
#include "nscore.h"

/*--- needed for autoregistry ---*/
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"


PR_BEGIN_EXTERN_C
extern int MK_OUT_OF_MEMORY;
PR_END_EXTERN_C


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

static NS_DEFINE_IID(kPNGDecoderIID, NS_PNGDECODER_IID);

NS_IMETHODIMP 
PNGDecoder::QueryInterface(const nsIID& aIID, void** aInstPtr)
{ 
  if (NULL == aInstPtr) {
    return NS_ERROR_NULL_POINTER;
  }

  NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  NS_DEFINE_IID(kIImgDecoderIID, NS_IIMGDECODER_IID);

  if (aIID.Equals(kPNGDecoderIID) ||  
      aIID.Equals(kIImgDecoderIID) ||
      aIID.Equals(kISupportsIID)) {
	  *aInstPtr = (void*) this;
    NS_INIT_REFCNT();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(PNGDecoder)
NS_IMPL_RELEASE(PNGDecoder)

/*----------------------------------------------------*/
// api functions
/*------------------------------------------------------*/
NS_IMETHODIMP
PNGDecoder::ImgDInit()
{
  int ret;

  if( ilContainer != NULL ) {
     ret = il_png_init(ilContainer);
     if(ret != 0)
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
 

