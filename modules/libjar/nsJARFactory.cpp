/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Daniel Veditz <dveditz@netscape.com>
 */
#include <string.h>

#include "nscore.h"
#include "pratom.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "prlog.h"

#include "xp_regexp.h"
#include "nsRepository.h"
#include "nsIComponentManager.h"

#include "nsJAR.h"
#include "nsIJARFactory.h"

/* XPCOM includes */
//#include "nsIComponentManager.h"


/*********************************
 * Begin XPCOM-related functions
 *********************************/

/*-----------------------------------------------------------------
 * XPCOM-related Globals
 *-----------------------------------------------------------------*/
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

static NS_DEFINE_IID(kIJAR_IID, NS_IJAR_IID);
static NS_DEFINE_IID(kJAR_CID,  NS_JAR_CID);

static NS_DEFINE_IID(kIJARFactory_IID, NS_IJARFactory_IID);
static NS_DEFINE_IID(kJARFactory_CID,  NS_JARFactory_CID);

static PRInt32 gInstanceCnt = 0;
static PRInt32 gLockCnt     = 0;


/*---------------------------------------------------------
 * nsJARFactory implementation
 *--------------------------------------------------------*/
static PRInt32 gJARInstanceCnt = 0;
static PRInt32 gJARLock        = 0;

/* constructor */
nsJARFactory::nsJARFactory(void)
{
  mRefCnt=0;
  PR_AtomicIncrement(&gJARInstanceCnt);
}

/* destructor */
nsJARFactory::~nsJARFactory(void)
{
  PR_AtomicDecrement(&gJARInstanceCnt);
}

/* The ubiquitous QueryInterface method */
NS_IMETHODIMP 
nsJARFactory::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aInstancePtr == NULL)
  {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kISupportsIID) )
  {
    *aInstancePtr = (void*) this;
  }
  else if ( aIID.Equals(kIFactoryIID) )
  {
    *aInstancePtr = (void*) this;
  }

  if (aInstancePtr == NULL)
  {
    return NS_ERROR_NO_INTERFACE;
  }

  AddRef();

  return NS_OK;
}

/* The omnipresent AddRef method */
NS_IMETHODIMP
nsJARFactory::AddRef(void)
{
  return ++mRefCnt;
}

/* The ever-popular Release method */
NS_IMETHODIMP
nsJARFactory::Release(void)
{
  if (--mRefCnt ==0)
  {
    delete this;
    return 0; // Don't access mRefCnt after deleting!
  }
  return mRefCnt;
}

/*------------------------------------------------------------------------*/
/* The JARFactory CreateInstance Method
/*------------------------------------------------------------------------*/
NS_IMETHODIMP
nsJARFactory::CreateInstance(nsISupports *aOuter, 
                             REFNSIID aIID, 
                             void **aResult)
{
  if (aResult == NULL)
  {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;
  nsJAR* pJAR = new nsJAR();

  if (pJAR == NULL)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult result =  pJAR->QueryInterface(aIID, aResult);

  if (result != NS_OK)
    delete pJAR;

  return result;
}

NS_IMETHODIMP
nsJARFactory::LockFactory(PRBool aLock)
{
  if (aLock)
    PR_AtomicIncrement(&gJARLock);
  else
    PR_AtomicDecrement(&gJARLock);
  return NS_OK;
}

/*----------------------------------------------------------
 * Exported XPCOM Factory stuff
 *---------------------------------------------------------*/
extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
  if (aFactory == NULL)
  {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = NULL;
  nsISupports *inst;

  if ( aClass.Equals(kJAR_CID) )
  {
    inst = new nsJARFactory();        
  }
  else
  {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (inst == NULL)
  {   
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult res = inst->QueryInterface(kIFactoryIID, (void**) aFactory);

  if (NS_FAILED(res))
  {   
    delete inst;
  }

  return res;
}


/*----------------------------------------------------------
 * XPCOM registry stuff
 *---------------------------------------------------------*/
extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* aServiceMgr, const char *path)
{
  nsresult result;

  printf("*** Libjar is being registered\n");
  nsRepository::RegisterComponent(kJAR_CID, NULL, NULL, path, PR_TRUE, PR_TRUE);

  return NS_OK;
}


extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServiceMgr, const char *path)
{
  nsresult result;
  //nsRepository::UnregisterFactory(kJAR_CID, path);
  return NS_OK;
}

