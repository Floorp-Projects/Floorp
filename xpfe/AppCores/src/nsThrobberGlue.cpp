/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

//
// nsThrobberGlue
// Mike Pinkerton, Netscape Communications
//
// See header file for details
//


#include "nsThrobberGlue.h"
#include "nsIFactory.h"
#include "pratom.h"
#include "nsAppCores.h"

// IID's
static NS_DEFINE_IID(kIThrobberIID, NS_ITHROBBER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);


nsThrobberGlue :: nsThrobberGlue()
{
  NS_INIT_REFCNT();
}

//----------------------------------------------------------------------
nsThrobberGlue :: ~nsThrobberGlue()
{
}


NS_IMPL_ADDREF(nsThrobberGlue)
NS_IMPL_RELEASE(nsThrobberGlue)


//----------------------------------------------------------------------
nsresult
nsThrobberGlue :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult rv = NS_NOINTERFACE;

  if (NULL == aInstancePtr) {
    rv = NS_ERROR_NULL_POINTER;
  }
  else if (aIID.Equals(kIThrobberIID)) {
    *aInstancePtr = (void*)(nsIThrobber*)this;
    NS_ADDREF_THIS();
    rv = NS_OK;
  }
  else if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    rv = NS_OK;
  }

  return rv;
}


NS_IMETHODIMP
nsThrobberGlue::Init(nsIWidget* , const nsRect& )
{
  return NS_OK;
}

NS_IMETHODIMP
nsThrobberGlue::Init(nsIWidget* , const nsRect& , const nsString& , PRInt32 )
{
#if 0
  mWidth     = aBounds.width;
  mHeight    = aBounds.height;
  mNumImages = aNumImages;

  // Create widget
  nsresult rv = nsRepository::CreateInstance(kChildCID, nsnull, kIWidgetIID, (void**)&mWidget);
  if (NS_OK != rv) {
    return rv;
  }
  mWidget->Create(aParent, aBounds, HandleThrobberEvent, NULL);
  return LoadThrobberImages(aFileNameMask, aNumImages);
#endif
  
  return NS_OK;
}

NS_IMETHODIMP
nsThrobberGlue::MoveTo(PRInt32 aX, PRInt32 aY)
{
#if 0
  NS_PRECONDITION(nsnull != mWidget, "no widget");
  mWidget->Resize(aX, aY, mWidth, mHeight, PR_TRUE);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsThrobberGlue::Show()
{
#if 0
  mWidget->Show(PR_TRUE);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsThrobberGlue::Hide()
{
#if 0
  mWidget->Show(PR_FALSE);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsThrobberGlue::Start()
{
#if 0
  mRunning = PR_TRUE;
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsThrobberGlue::Stop()
{
#if 0
  mRunning = PR_FALSE;
  mIndex = 0;
  mWidget->Invalidate(PR_FALSE);
#endif

  return NS_OK;
}

#ifdef USE_THROBBER_GLUE_FACTORY

//----------------------------------------------------------------------
// Factory code for creating nsThrobberGlue's
// *** Do we really need one of these? This should be mostly internal
// *** to the browser appCore stuff and not really exposed to js.

class nsThrobberGlueFactory : public nsIFactory
{
public:
  nsThrobberGlueFactory();
  virtual ~nsThrobberGlueFactory();

  // nsISupports methods
  NS_IMETHOD QueryInterface(const nsIID &aIID, void **aResult);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

private:
  nsrefcnt  mRefCnt;
};

nsThrobberGlueFactory::nsThrobberGlueFactory()
{
  mRefCnt = 0;
  IncInstanceCount();
}

nsThrobberGlueFactory::~nsThrobberGlueFactory()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
  DecInstanceCount();
}

nsresult
nsThrobberGlueFactory::QueryInterface(const nsIID &aIID, void **aResult)
{
  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aResult = NULL;

  if (aIID.Equals(kISupportsIID)) {
    *aResult = (void *)(nsISupports*)this;
  } else if (aIID.Equals(kIFactoryIID)) {
    *aResult = (void *)(nsIFactory*)this;
  }

  if (*aResult == NULL) {
    return NS_ERROR_NO_INTERFACE;
  }

  NS_ADDREF_THIS();  // Increase reference count for caller
  return NS_OK;
}

nsrefcnt
nsThrobberGlueFactory::AddRef()
{
  return ++mRefCnt;
}

nsrefcnt
nsThrobberGlueFactory::Release()
{
  if (--mRefCnt == 0) {
    delete this;
    return 0; // Don't access mRefCnt after deleting!
  }
  return mRefCnt;
}

nsresult
nsThrobberGlueFactory::CreateInstance(nsISupports *aOuter,
                                  const nsIID &aIID,
                                  void **aResult)
{
    if (aResult == NULL)
        return NS_ERROR_NULL_POINTER;

    *aResult = NULL;

    /* do I have to use iSupports? */
    nsThrobberGlue *inst = new nsThrobberGlue();

    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult result =  inst->QueryInterface(aIID, aResult);

    if (result != NS_OK)
        delete inst;

    return result;
}

nsresult
nsThrobberGlueFactory::LockFactory(PRBool aLock)
{
  if (aLock)
      IncLockCount();
  else
      DecLockCount();

  return NS_OK;
}

#endif // USE_THROBBER_GLUE_FACTORY