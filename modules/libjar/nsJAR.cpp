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

#include "nsZipIIDs.h"
#include "nsJAR.h"

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

static NS_DEFINE_IID(kIZip_IID, NS_IZip_IID);
static NS_DEFINE_IID(kZip_CID,  NS_Zip_CID);

/*---------------------------------------------
 *  nsJAR::QueryInterface implementation
 *--------------------------------------------*/
NS_IMETHODIMP 
nsJAR::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL)
  {
    return NS_ERROR_NULL_POINTER;
  }
  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kIZip_IID) )
  {
    *aInstancePtr = (void*)(nsIZip*)this;
    AddRef();
    return NS_OK;
  }
  else if ( aIID.Equals(kIJAR_IID) )
  {
   *aInstancePtr = (void*)(nsIJAR*)this;
    AddRef();
    return NS_OK;
  }
  else if ( aIID.Equals(kISupportsIID) )
  {
   *aInstancePtr = (void*)(nsISupports*)this;
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

/* AddRef macro */
NS_IMPL_ADDREF(nsJAR)

/* Release macro */
NS_IMPL_RELEASE(nsJAR)


/*----------------------------------------------------------
 * After all that stuff we finally get to what nsJAR is 
 * all about
 *---------------------------------------------------------*/
nsJAR::nsJAR()
{
}


nsJAR::~nsJAR()
{
}


NS_IMETHODIMP
nsJAR::Open(const char* zipFileName, PRInt32 *aResult)
{
  *aResult = zip.OpenArchive(zipFileName);
  return NS_OK;
}

NS_IMETHODIMP
nsJAR::Extract(const char * aFilename, const char * aOutname, PRInt32 *aResult)
{
  *aResult = zip.ExtractFile(aFilename, aOutname);
  return NS_OK;
}

NS_IMETHODIMP
nsJAR::Find( const char * aPattern, nsZipFind *aResult)
{
  aResult = zip.FindInit(aPattern);
  return NS_OK;
}

