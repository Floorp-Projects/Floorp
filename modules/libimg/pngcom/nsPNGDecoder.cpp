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
class PNGDecoder : public nsIImgDecoder   
{
public:
  PNGDecoder(il_container* aContainer);
  virtual ~PNGDecoder();
 
  NS_DECL_ISUPPORTS

  /* stream */
  NS_IMETHOD ImgDInit();

  NS_IMETHOD ImgDWriteReady(PRUint32 *request_size);
  NS_IMETHOD ImgDWrite(const unsigned char *buf, int32 len);
  NS_IMETHOD ImgDComplete();
  NS_IMETHOD ImgDAbort();

  NS_IMETHOD_(il_container *) SetContainer(il_container *ic){ilContainer = ic; return ic;}
  NS_IMETHOD_(il_container *) GetContainer() {return ilContainer;}

  
private:
  il_container* ilContainer;
};
/*-------------------------------------------------*/

PNGDecoder::PNGDecoder(il_container* aContainer)
{
  NS_INIT_REFCNT();
  ilContainer = aContainer;
};


PNGDecoder::~PNGDecoder(void)
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
};


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

/*-----------class----------------*/
class nsPNGDecFactory : public nsIFactory 
{
public:
  NS_DECL_ISUPPORTS

  nsPNGDecFactory(const nsCID &aClass);
  virtual ~nsPNGDecFactory();

  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            REFNSIID aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);
 
protected:

private:
	nsCID mClassID;
	il_container *ilContainer;
};

/*-----------------------------------------*/

nsPNGDecFactory* gFactory = NULL;

NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsPNGDecFactory, kIFactoryIID)


nsPNGDecFactory::nsPNGDecFactory(const nsCID &aClass)
{
	NS_INIT_REFCNT();
	mClassID = aClass;
}

nsPNGDecFactory::~nsPNGDecFactory(void)
{
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}
/*----------------------------------for autoregistration ----*/
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

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

     if ((rv = compMgr->RegisterComponent(kPNGDecoderCID, 
       "Netscape PNGDec", 
       "component://netscape/image/decoder&type=image/png", path, PR_TRUE, PR_TRUE)
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

  rv = compMgr->UnregisterComponent(kPNGDecoderCID, path);

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

/*--------------------*/
NS_IMETHODIMP
nsPNGDecFactory::CreateInstance(nsISupports *aOuter,
                             const nsIID &aIID,
                             void **ppv)
{
   PNGDecoder *pngdec = NULL;
   *ppv  = NULL;
   il_container* ic = NULL;

NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

   if (aOuter && !aIID.Equals(kISupportsIID))
       return NS_NOINTERFACE;  
   ic = new il_container();

   pngdec = new PNGDecoder(ic);
   nsresult res = pngdec->QueryInterface(aIID,(void**)ppv);
       //interface is other than nsISupports and pngdecoder
       
    if (NS_FAILED(res)) {
    *ppv = NULL;
    delete pngdec;
  }
  delete ic; /* is a place holder */

  return res;
}
 
NS_METHOD
nsPNGDecFactory::LockFactory(PRBool aLock)
{
    return NS_OK;
}

extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
  if( !aClass.Equals(kPNGDecoderCID))
		return NS_ERROR_FACTORY_NOT_REGISTERED;
  if( gFactory == NULL){
	  gFactory = new nsPNGDecFactory(aClass);
	  if( gFactory == NULL)
		return NS_ERROR_OUT_OF_MEMORY;
	  gFactory->AddRef(); //for global
  }
  gFactory->AddRef();   //for client
  *aFactory = gFactory;
  return NS_OK;
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
     if(ret != 0)
         return NS_ERROR_FAILURE;
  }
  return NS_OK;
}


NS_IMETHODIMP 
PNGDecoder::ImgDWriteReady(PRUint32 *request_size)
{
  /* dummy return needed */
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
 

