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
 *   nsJPGDecoder.cpp --- interface to gif decoder
 */


#include "if_struct.h"
#include "jpeg.h"
#include "prmem.h"
#include "merrors.h"


#include "dllcompat.h"
#include "nsJPGDecoder.h"
#include "nsImgDecCID.h"
#include "nsIImgDecoder.h"  /* interface class */
#include "nsImgDecoder.h" /* factory */
#include "nscore.h"

/*--- needed for autoregistry ---*/

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

PR_BEGIN_EXTERN_C
extern int MK_OUT_OF_MEMORY;
PR_END_EXTERN_C


/*-----------class----------------*/
class JPGDecoder : public nsIImgDecoder   
{
public:
  JPGDecoder(il_container* aContainer);
  ~JPGDecoder();
 
  NS_DECL_ISUPPORTS

  /* stream */
  NS_IMETHOD ImgDInit();

  NS_IMETHOD ImgDWriteReady();
  NS_IMETHOD ImgDWrite(const unsigned char *buf, int32 len);
  NS_IMETHOD ImgDComplete();
  NS_IMETHOD ImgDAbort();

  il_container *SetContainer(il_container *ic){mContainer = ic; return ic;}
  il_container *GetContainer() {return mContainer;}

  
private:
  il_container* mContainer;
};
/*-------------------------------------------------*/

JPGDecoder::JPGDecoder(il_container* aContainer)
{
  NS_INIT_REFCNT();
  mContainer = aContainer;
};


JPGDecoder::~JPGDecoder(void)
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
};


NS_IMETHODIMP 
JPGDecoder::QueryInterface(const nsIID& aIID, void** aInstPtr)
{ 
  if (NULL == aInstPtr) {
    return NS_ERROR_NULL_POINTER;
  }
   
  if (aIID.Equals(kJPGDecoderIID) ||     
      aIID.Equals(kImgDecoderIID) ||
      aIID.Equals(kISupportsIID)) {
	  *aInstPtr = (void*) this;
    NS_INIT_REFCNT();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


/*----------------------------------for autoregistration ----*/
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);


extern "C" NS_EXPORT nsresult 
NSRegisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;
/**/
  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;
  if ((rv = nsRepository::RegisterComponent(kJPGDecoderCID, "Netscape JPGDec", "component://netscape/image/decoder&type=image/jpeg", path, PR_TRUE, PR_TRUE)
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

  rv = compMgr->UnregisterFactory(kJPGDecoderCID, path);

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

/*--------------------*/

NS_IMETHODIMP 
JPGDecoder::AddRef()
{
  NS_INIT_REFCNT();
  return NS_OK;
}

NS_IMETHODIMP 
JPGDecoder::Release()
{
  return NS_OK;
}

/*-----------class----------------*/
class nsJPGDecFactory : public nsIFactory 
{
public:
  NS_DECL_ISUPPORTS

  nsJPGDecFactory(const nsCID &aClass);
  ~nsJPGDecFactory();

  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            REFNSIID aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);
 
protected:

private:
	nsCID mClassID;
	il_container *mContainer;
};

/*-----------------------------------------*/

nsJPGDecFactory* gFactory = NULL;
NS_IMPL_ISUPPORTS(nsJPGDecFactory, kIFactoryIID);


nsJPGDecFactory::nsJPGDecFactory(const nsCID &aClass)
{
	NS_INIT_REFCNT();
	mClassID = aClass;
}

nsJPGDecFactory::~nsJPGDecFactory(void)
{
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}


NS_IMETHODIMP
nsJPGDecFactory::CreateInstance(nsISupports *aOuter,
                             const nsIID &aIID,
                             void **ppv)
{
   JPGDecoder *jpgdec = NULL;
   *ppv  = NULL;
   il_container* ic = NULL;
   
   if (aOuter && !aIID.Equals(kISupportsIID))
       return NS_NOINTERFACE;  
   ic = new il_container();

   jpgdec = new JPGDecoder(ic);
   nsresult res = jpgdec->QueryInterface(aIID,(void**)ppv);
       //interface is other than nsISupports and gifdecoder
       
    if (NS_FAILED(res)) {
    *ppv = NULL;
    delete jpgdec;
  }

  return res;
}

 
NS_METHOD
nsJPGDecFactory::LockFactory(PRBool aLock)
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
  if( !aClass.Equals(kJPGDecoderCID))
		return NS_ERROR_FACTORY_NOT_REGISTERED;
  if( gFactory == NULL){
	  gFactory = new nsJPGDecFactory(aClass);
	  if( gFactory == NULL)
		return NS_ERROR_OUT_OF_MEMORY;
	  gFactory->AddRef(); //for global
  }
  gFactory->AddRef();   //for client
  *aFactory = gFactory;
  return NS_OK;
}

/*------------------------------------------------------*/
/* api functions
 */
/*------------------------------------------------------*/
NS_IMETHODIMP
JPGDecoder::ImgDInit()
{
   if(mContainer != NULL) {
     return(il_jpeg_init(mContainer));
  }
  else {
    return nsnull;
  }
}


NS_IMETHODIMP 
JPGDecoder::ImgDWriteReady()
{ /*  IL_StreamWriteReady needs a return 1 */
  return 1;
}

NS_IMETHODIMP
JPGDecoder::ImgDWrite(const unsigned char *buf, int32 len)
{
  if( mContainer != NULL ) {
     return(il_jpeg_write(mContainer, buf,len));
  }
  return 0;
}

NS_IMETHODIMP 
JPGDecoder::ImgDComplete()
{
  if( mContainer != NULL ) {
     il_jpeg_complete(mContainer);
  }
  return 0;
}

NS_IMETHODIMP 
JPGDecoder::ImgDAbort()
{
  if( mContainer != NULL ) {
    il_jpeg_abort(mContainer);
  }
  return 0;
}
 
