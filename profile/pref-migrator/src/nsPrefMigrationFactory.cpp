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


#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "NSReg.h"
#include "nsCOMPtr.h"

#include "nsPrefMigration.h"

/*-------------------------------------------------------------------------*/
/* Pref Migration Factory routines                                         */
/*-------------------------------------------------------------------------*/
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);  /* Need to remove these defines from this */
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);    /* file they should be global to the      */
                                                        /* prefmigration source files.            */

static PRInt32 gPrefMigrationInstanceCnt = 0;
static PRInt32 gPrefMigrationLock        = 0;

nsPrefMigrationFactory::nsPrefMigrationFactory(void)
{
  mRefCnt=0;
  PR_AtomicIncrement(&gPrefMigrationInstanceCnt);
}

nsPrefMigrationFactory::~nsPrefMigrationFactory(void)
{
  PR_AtomicDecrement(&gPrefMigrationInstanceCnt);
}

NS_IMETHODIMP 
nsPrefMigrationFactory::QueryInterface(REFNSIID aIID, void** aInstancePtr)
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


NS_IMPL_ADDREF(nsPrefMigrationFactory);
NS_IMPL_RELEASE(nsPrefMigrationFactory);
/*------------------------------------------------------------------------*/
/* The PrefMigration CreateInstance Method
/*------------------------------------------------------------------------*/
NS_IMETHODIMP
nsPrefMigrationFactory::CreateInstance(nsISupports *aOuter, 
                                       REFNSIID aIID, 
                                       void **aResult)
{
  if (aResult == NULL)
  {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;
  nsPrefMigration *pPrefMigration = new nsPrefMigration();

  if (pPrefMigration == NULL)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult result =  pPrefMigration->QueryInterface(aIID, aResult);

  if (result != NS_OK)
    delete pPrefMigration;

  return result;
}

NS_IMETHODIMP
nsPrefMigrationFactory::LockFactory(PRBool aLock)
{
  if (aLock)
    PR_AtomicIncrement(&gPrefMigrationLock);
  else
    PR_AtomicDecrement(&gPrefMigrationLock);

  return NS_OK;
}

