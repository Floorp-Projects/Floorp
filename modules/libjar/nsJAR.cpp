/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Daniel Veditz <dveditz@netscape.com>
 *     Samir Gehani <sgehani@netscape.com>
 *     Mitch Stoltz <mstoltz@netsape.com>
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
#include "nsJARInputStream.h"
//#include "nsIFile.h"

#ifndef __gen_nsIFile_h__
#define NS_ERROR_FILE_UNRECONGNIZED_PATH        NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 1)
#define NS_ERROR_FILE_UNRESOLVABLE_SYMLINK      NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 2)
#define NS_ERROR_FILE_EXECUTION_FAILED          NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 3)
#define NS_ERROR_FILE_UNKNOWN_TYPE              NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 4)
#define NS_ERROR_FILE_DESTINATION_NOT_DIR       NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 5)
#define NS_ERROR_FILE_TARGET_DOES_NOT_EXIST     NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 6)
#define NS_ERROR_FILE_COPY_OR_MOVE_FAILED       NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 7)
#define NS_ERROR_FILE_ALREADY_EXISTS            NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 8)
#define NS_ERROR_FILE_INVALID_PATH              NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 9)
#define NS_ERROR_FILE_DISK_FULL                 NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 10)
#define NS_ERROR_FILE_CORRUPTED                 NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES, 11)
#endif

static nsresult
ziperr2nsresult(PRInt32 ziperr)
{
  switch (ziperr) {
    case ZIP_OK:                return NS_OK;
    case ZIP_ERR_MEMORY:        return NS_ERROR_OUT_OF_MEMORY;
    case ZIP_ERR_DISK:          return NS_ERROR_FILE_DISK_FULL;
    case ZIP_ERR_CORRUPT:       return NS_ERROR_FILE_CORRUPTED;
    case ZIP_ERR_PARAM:         return NS_ERROR_ILLEGAL_VALUE;
    case ZIP_ERR_FNF:           return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
    case ZIP_ERR_UNSUPPORTED:   return NS_ERROR_NOT_IMPLEMENTED;
    default:                    return NS_ERROR_FAILURE;
  }
}

#if 0
static PRInt32
nsresult2ziperr(nsresult rv)
{
  switch (rv) {
    case NS_OK:                                 return ZIP_OK;
    case NS_ERROR_OUT_OF_MEMORY:                return ZIP_ERR_MEMORY;
    case NS_ERROR_FILE_DISK_FULL:               return ZIP_ERR_DISK;
    case NS_ERROR_FILE_CORRUPTED:               return ZIP_ERR_CORRUPT;
    case NS_ERROR_ILLEGAL_VALUE:                return ZIP_ERR_PARAM;
    case NS_ERROR_FILE_TARGET_DOES_NOT_EXIST:   return ZIP_ERR_FNF;
    case NS_ERROR_NOT_IMPLEMENTED:              return ZIP_ERR_UNSUPPORTED;
    default:                                    return ZIP_ERR_GENERAL;
  }    
}
#endif /* 0 */

nsJAR::nsJAR()
{
  NS_INIT_REFCNT();
}

nsJAR::~nsJAR()
{
}

NS_IMPL_ISUPPORTS1(nsJAR, nsIZipReader);

NS_IMETHODIMP
nsJAR::Init(nsFileSpec& zipFile)
{
  mZipFile = zipFile;
  return NS_OK;
}

NS_IMETHODIMP
nsJAR::Open()
{
  PRInt32 err = mZip.OpenArchive( nsNSPRPath(mZipFile) );
  return ziperr2nsresult(err);
}

NS_IMETHODIMP
nsJAR::Close()
{
  PRInt32 err = mZip.CloseArchive();
  return ziperr2nsresult(err);
}

NS_IMETHODIMP
nsJAR::Extract(const char *zipEntry, nsFileSpec& outFile)
{
  PRInt32 err = mZip.ExtractFile(zipEntry, nsNSPRPath(outFile));
  return ziperr2nsresult(err);
}

NS_IMETHODIMP    
nsJAR::GetEntry(const char *zipEntry, nsIZipEntry* *result)
{
  nsZipItem item;
  PRInt32 err = mZip.GetItem(zipEntry, &item);

  nsJARItem* jarItem = new nsJARItem();
  if (jarItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  err = jarItem->Init(&item);
  if (err != ZIP_OK) {
    delete jarItem;
    return ziperr2nsresult(err);
  }
  
  NS_ADDREF(jarItem);
  *result = jarItem;
  return NS_OK;
}

NS_IMETHODIMP    
nsJAR::FindEntries(const char *aPattern, nsISimpleEnumerator **_retval)
{
  if (!_retval)
    return NS_ERROR_INVALID_POINTER;
    
  nsZipFind *find = mZip.FindInit(aPattern);
  if (!find)
    return NS_ERROR_OUT_OF_MEMORY;

  nsISimpleEnumerator *zipEnum = new nsJAREnumerator(find);
  if (!zipEnum)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF( zipEnum );

  *_retval = zipEnum;
  return NS_OK;
}

NS_IMETHODIMP
nsJAR::GetInputStream(const char *aFilename, nsIInputStream **_retval)
{
  nsresult rv;
  nsJARInputStream* is = nsnull;
  rv = nsJARInputStream::Create(nsnull, NS_GET_IID(nsIInputStream), (void**)&is);
  if (!is) return NS_ERROR_FAILURE;

  rv = is->Init(&mZip, aFilename);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  *_retval = (nsIInputStream*)is;
  NS_ADDREF(*_retval);
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
    nsJARItem* jarItem = new nsJARItem();
    if(jarItem)
    {
      PRInt32 err = jarItem->Init(mCurr);
      if (err != ZIP_OK) {
        delete jarItem;
        return err;
      }
      NS_ADDREF(jarItem);
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
    NS_INIT_ISUPPORTS();
}

nsJARItem::~nsJARItem()
{
}

NS_IMPL_ISUPPORTS1(nsJARItem, nsIZipEntry);

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
nsJARItem::GetRealSize(PRUint32 *aRealsize)
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
nsJARItem::GetCRC32(PRUint32 *aCrc32)
{
    if (!aCrc32)
        return NS_ERROR_NULL_POINTER;
    if (!crc32)
        return NS_ERROR_FAILURE;

    *aCrc32 = crc32;
    return NS_OK;
}
