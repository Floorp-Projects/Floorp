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
 *   nsGIFDecoder.cpp --- interface to gif decoder
 */


#include "if_struct.h"
#include "prmem.h"
#include "merrors.h"


#include "dllcompat.h"
#include "gif.h"
#include "nsGIFDecoder.h"
#include "nsImgDecCID.h"
#include "nsIImgDecoder.h"  /* interface class */
#include "nsImgDecoder.h"   /* factory */
#include "nscore.h"

/*--- needed for autoregistry ---*/
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"


PR_BEGIN_EXTERN_C
extern int MK_OUT_OF_MEMORY;
PR_END_EXTERN_C


/*-----------class----------------*/
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

  il_container *SetContainer(il_container *ic){ilContainer = ic; return ic;}
  il_container *GetContainer() {return ilContainer;}

  
private:
  il_container* ilContainer;
};
/*-------------------------------------------------*/

GIFDecoder::GIFDecoder(il_container* aContainer)
{
  NS_INIT_REFCNT();
  ilContainer = aContainer;
};


GIFDecoder::~GIFDecoder(void)
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
};


NS_IMPL_ADDREF(GIFDecoder)
NS_IMPL_RELEASE(GIFDecoder)

NS_IMETHODIMP 
GIFDecoder::QueryInterface(const nsIID& aIID, void** aInstPtr)
{ 
  if (NULL == aInstPtr) {
    return NS_ERROR_NULL_POINTER;
  }

NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(kGIFDecoderIID) ||
      aIID.Equals(kImgDecoderIID) ||
      aIID.Equals(kISupportsIID)) {
	  *aInstPtr = (void*) this;
    NS_INIT_REFCNT();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

/*-----------class----------------*/
class nsGIFDecFactory : public nsIFactory 
{
public:
  NS_DECL_ISUPPORTS

  nsGIFDecFactory(const nsCID &aClass);
  virtual ~nsGIFDecFactory();

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

nsGIFDecFactory* gFactory = NULL;

NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsGIFDecFactory, kIFactoryIID)

nsGIFDecFactory::nsGIFDecFactory(const nsCID &aClass)
{
	NS_INIT_REFCNT();
	mClassID = aClass;
}

nsGIFDecFactory::~nsGIFDecFactory(void)
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

     if ((rv = nsRepository::RegisterComponent(kGIFDecoderCID, 
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

/*--------------------*/
NS_IMETHODIMP
nsGIFDecFactory::CreateInstance(nsISupports *aOuter,
                             const nsIID &aIID,
                             void **ppv)
{
   GIFDecoder *gifdec = NULL;
   *ppv  = NULL;
   il_container* ic = NULL;

NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

   if (aOuter && !aIID.Equals(kISupportsIID))
       return NS_NOINTERFACE;  
   ic = new il_container();

   gifdec = new GIFDecoder(ic);
   nsresult res = gifdec->QueryInterface(aIID,(void**)ppv);
       //interface is other than nsISupports and gifdecoder
       
    if (NS_FAILED(res)) {
    *ppv = NULL;
    delete gifdec;
  }
  delete ic; /* is a place holder */

  return res;
}
 
NS_METHOD
nsGIFDecFactory::LockFactory(PRBool aLock)
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
  if( !aClass.Equals(kGIFDecoderCID))
		return NS_ERROR_FACTORY_NOT_REGISTERED;
  if( gFactory == NULL){
	  gFactory = new nsGIFDecFactory(aClass);
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
 

