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
 *     Samir Gehani <sgehani@netscape.com>
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

#include "nsIZip.h"
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

static NS_DEFINE_IID(kIZip_IID, NS_IZIP_IID);
static NS_DEFINE_IID(kZip_CID,  NS_ZIP_CID);

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
    NS_INIT_REFCNT();
}


nsJAR::~nsJAR()
{
}


NS_IMETHODIMP
nsJAR::Open(const char *aZipFileName, PRInt32 *_retval)
{
  *_retval = zip.OpenArchive(aZipFileName);
  return NS_OK;
}

NS_IMETHODIMP
nsJAR::Extract(const char *aFilename, const char *aOutname, PRInt32 *_retval)
{
  *_retval = zip.ExtractFile(aFilename, aOutname);
  return NS_OK;
}

NS_IMETHODIMP    
nsJAR::Find(const char *aPattern, nsISimpleEnumerator **_retval)
{
    if (!_retval)
      return NS_ERROR_INVALID_POINTER;
    
    nsZipFind *find = zip.FindInit(aPattern);
    if (!find)
        return NS_ERROR_OUT_OF_MEMORY;

    nsISimpleEnumerator *zipEnum = new nsJAREnumerator(find);
    if (!zipEnum)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF( zipEnum );

    *_retval = zipEnum;
    return NS_OK;
}




//----------------------------------------------
// nsJAREnumerator constructor and destructor
//----------------------------------------------

nsJAREnumerator::nsJAREnumerator(nsZipFind *aFind)
: mFind(aFind),
  mCurr(nsnull),
  mIsCurrStale(PR_TRUE)
{
    mArchive = mFind->GetArchive();
    NS_INIT_REFCNT();
}

nsJAREnumerator::~nsJAREnumerator()
{
    mArchive->FindFree(mFind);
}

NS_IMPL_ISUPPORTS(nsJAREnumerator, nsISimpleEnumerator::GetIID());

//----------------------------------------------
// nsJAREnumerator::HasMoreElements
//----------------------------------------------
NS_IMETHODIMP
nsJAREnumerator::HasMoreElements(PRBool* aResult)
{
    PRInt32 err;

    if (!mFind)
        return NS_ERROR_NOT_INITIALIZED;

    // try to get the next element
    if (mIsCurrStale)
    {
        err = mArchive->FindNext( mFind, &mCurr );
        if (err == ZIP_ERR_FNF)
        {
            *aResult = PR_FALSE;
            return NS_OK;
        }
        if (err != ZIP_OK)
            return NS_ERROR_FAILURE; // no error translation

        mIsCurrStale = PR_FALSE;
    }

    *aResult = PR_TRUE;
    return NS_OK;
}

//----------------------------------------------
// nsJAREnumerator::GetNext
//----------------------------------------------
NS_IMETHODIMP
nsJAREnumerator::GetNext(nsISupports** aResult)
{
    nsresult rv;
    PRBool   bMore;

    // check if the current item is "stale"
    if (mIsCurrStale)
    {
        rv = HasMoreElements( &bMore );
        if (NS_FAILED(rv))
            return rv;
        if (bMore == PR_FALSE)
        {
            *aResult = nsnull;  // null return value indicates no more elements
            return NS_OK;
        }
    }

    // pack into an nsIJARItem
    nsIJARItem* jarItem = new nsJARItem(mCurr);
    if(jarItem)
    {
      jarItem->AddRef();
      *aResult = jarItem;
      mIsCurrStale = PR_TRUE; // we just gave this one away
      return NS_OK;
    }
    else
      return NS_ERROR_OUT_OF_MEMORY;
}




//-------------------------------------------------
// nsJARItem constructors and destructor
//-------------------------------------------------
nsJARItem::nsJARItem()
{
}

nsJARItem::nsJARItem(nsZipItem* aOther)
{
    NS_INIT_ISUPPORTS();
    name = PL_strndup( aOther->name, aOther->namelen );
    namelen = aOther->namelen;

    offset = aOther->offset;
    headerloc = aOther->headerloc;
    compression = aOther->compression;
    size = aOther->size;
    realsize = aOther->realsize;
    crc32 = aOther->crc32;

    next = nsnull;  // unused by a JARItem
}

nsJARItem::~nsJARItem()
{
}

NS_IMPL_ISUPPORTS(nsJARItem, nsIJARItem::GetIID());

//------------------------------------------
// nsJARItem::GetName
//------------------------------------------
NS_IMETHODIMP
nsJARItem::GetName(char * *aName)
{
    char *namedup;

    if ( !aName )
        return NS_ERROR_NULL_POINTER;
    if ( !name )
        return NS_ERROR_FAILURE;

    namedup = PL_strndup( name, namelen );
    if ( !namedup )
        return NS_ERROR_OUT_OF_MEMORY;

    *aName = namedup;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetCompression
//------------------------------------------
NS_IMETHODIMP 
nsJARItem::GetCompression(PRUint16 *aCompression)
{
    if (!aCompression)
        return NS_ERROR_NULL_POINTER;
    if (!compression)
        return NS_ERROR_FAILURE;

    *aCompression = compression;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetSize
//------------------------------------------
NS_IMETHODIMP 
nsJARItem::GetSize(PRUint32 *aSize)
{
    if (!aSize)
        return NS_ERROR_NULL_POINTER;
    if (!size)
        return NS_ERROR_FAILURE;

    *aSize = size;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetRealSize
//------------------------------------------
NS_IMETHODIMP 
nsJARItem::GetRealsize(PRUint32 *aRealsize)
{
    if (!aRealsize)
        return NS_ERROR_NULL_POINTER;
    if (!realsize)
        return NS_ERROR_FAILURE;

    *aRealsize = realsize;
    return NS_OK;
}

//------------------------------------------
// nsJARItem::GetCrc32
//------------------------------------------
NS_IMETHODIMP 
nsJARItem::GetCrc32(PRUint32 *aCrc32)
{
    if (!aCrc32)
        return NS_ERROR_NULL_POINTER;
    if (!crc32)
        return NS_ERROR_FAILURE;

    *aCrc32 = crc32;
    return NS_OK;
}
