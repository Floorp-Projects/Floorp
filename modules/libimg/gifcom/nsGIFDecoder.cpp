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

/*
 *   nsGIFDecoder.cpp --- interface to gif decoder
 */

#include "nsGIFDecoder.h"

/*--- needed for autoregistry ---*/
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIGenericFactory.h"

PR_BEGIN_EXTERN_C
extern int MK_OUT_OF_MEMORY;
PR_END_EXTERN_C

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGIFDecoderCID, NS_GIFDECODER_CID);
static NS_DEFINE_IID(kIImgDecoderIID, NS_IIMGDECODER_IID);


//////////////////////////////////////////////////////////////////////
// GIF Decoder Implementation

NS_IMPL_ISUPPORTS(GIFDecoder, kIImgDecoderIID)

GIFDecoder::GIFDecoder(il_container* aContainer)
{
  NS_INIT_REFCNT();
  ilContainer = aContainer;
};

GIFDecoder::~GIFDecoder(void)
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
};

NS_IMETHODIMP
GIFDecoder::ImgDInit()
{
   if(ilContainer != NULL) {
     return(il_gif_init(ilContainer));
  }
  else {
    return nsnull;
  }
}


NS_IMETHODIMP 
GIFDecoder::ImgDWriteReady()
{
  if(ilContainer != NULL) {
     return(il_gif_write_ready(ilContainer));
  }
  return 0;
}

NS_IMETHODIMP
GIFDecoder::ImgDWrite(const unsigned char *buf, int32 len)
{
  if( ilContainer != NULL ) {
     return(il_gif_write(ilContainer, buf,len));
  }
  return 0;
}

NS_IMETHODIMP 
GIFDecoder::ImgDComplete()
{
  if( ilContainer != NULL ) {
     il_gif_complete(ilContainer);
  }
  return 0;
}

NS_IMETHODIMP 
GIFDecoder::ImgDAbort()
{
  if( ilContainer != NULL ) {
    il_gif_abort(ilContainer);
  }
  return 0;
}
 

