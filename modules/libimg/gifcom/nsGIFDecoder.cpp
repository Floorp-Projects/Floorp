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

#include "nsIImgDecoder.h"  // include if_struct.h Needs to be included first
#include "nsGIFDecoder.h"
#include "gif.h"

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
// GIF Decoder Definition

class GIFDecoder : public nsIImgDecoder   
{
public:
  GIFDecoder(il_container* aContainer);
  virtual ~GIFDecoder();
 
  NS_DECL_ISUPPORTS

  /* stream */
  NS_IMETHOD ImgDInit();

  NS_IMETHOD ImgDWriteReady();
  NS_IMETHOD ImgDWrite(const unsigned char *buf, int32 len);
  NS_IMETHOD ImgDComplete();
  NS_IMETHOD ImgDAbort();

  NS_IMETHOD_(il_container *) SetContainer(il_container *ic){ilContainer = ic; return ic;}
  NS_IMETHOD_(il_container *) GetContainer() {return ilContainer;}

  
private:
  il_container* ilContainer;
};

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

//////////////////////////////////////////////////////////////////////
// GIF Decoder Factory using nsIGenericFactory

static NS_IMETHODIMP
nsGIFDecoderCreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    GIFDecoder *gifdec = NULL;
    *aResult = NULL;
    il_container* ic = NULL;

    NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

    if (aOuter && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;  

    ic = new il_container();
    if (!ic)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    gifdec = new GIFDecoder(ic);
    if (!gifdec)
    {
        delete ic;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult res = gifdec->QueryInterface(aIID, aResult);
    if (NS_FAILED(res))
    {
        *aResult = NULL;
        delete gifdec;
    }
    delete ic; /* is a place holder */
    return res;
}


//////////////////////////////////////////////////////////////////////
// Module Object Creation Entry points

extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
  if( !aClass.Equals(kGIFDecoderCID))
		return NS_ERROR_FACTORY_NOT_REGISTERED;

  nsCOMPtr<nsIGenericFactory> fact;
  nsresult rv = NS_NewGenericFactory(getter_AddRefs(fact), nsGIFDecoderCreateInstance);
  if (NS_FAILED(rv)) return rv;

  rv = fact->QueryInterface(kIFactoryIID, (void **)aFactory);
  return rv;
}

//////////////////////////////////////////////////////////////////////
// Module Registration Entrypoints

extern "C" NS_EXPORT nsresult 
NSRegisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

     if ((rv = compMgr->RegisterComponent(kGIFDecoderCID, 
       "Netscape GIFDec", 
       "component://netscape/image/decoder&type=image/gif", path, PR_TRUE, PR_TRUE)
				  ) != NS_OK) {
				  return rv;
			  }
 (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;

}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kGIFDecoderCID, path);

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}
 

