/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator.
 * 
 * The Initial Developer of the Original Code is Intel Corp.
 * Portions created by Intel Corp. are
 * Copyright (C) 1999, 1999 Intel Corp.  All
 * Rights Reserved.
 * 
 * Contributor(s): Yixiong Zou <yixiong.zou@intel.com>
 *                 Carl Wong <carl.wong@intel.com>
 */

#include "nsDiskCacheRecord.h"
#include "nsINetDataDiskCache.h"
#include "nsNetDiskCacheCID.h"
#include "nsDiskCacheRecordChannel.h"
#include "nsFileStream.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIProtocolHandler.h"
#include "nsIIOService.h"
#include "nsIAllocator.h"

#include "plstr.h"
#include "prprf.h"
#include "prmem.h"
#include "prlog.h"
#include "prtypes.h"
#include "netCore.h"

#include "nsDBAccessor.h"

#if !defined(IS_LITTLE_ENDIAN) && !defined(IS_BIG_ENDIAN)
ERROR! Must have a byte order
#endif

#ifdef IS_LITTLE_ENDIAN
#define COPY_INT32(_a,_b)  memcpy(_a, _b, sizeof(int32))
#else
#define COPY_INT32(_a,_b)  /* swap */                   \
    do {                                                \
    ((char *)(_a))[0] = ((char *)(_b))[3];              \
    ((char *)(_a))[1] = ((char *)(_b))[2];              \
    ((char *)(_a))[2] = ((char *)(_b))[1];              \
    ((char *)(_a))[3] = ((char *)(_b))[0];              \
    } while(0)
#endif

nsDiskCacheRecord::nsDiskCacheRecord(nsIDBAccessor* db, nsNetDiskCache* aCache) :
  mKey(0) ,
  mKeyLength(0) ,
  mRecordID(0) ,
  mMetaData(0) ,
  mMetaDataLength(0) ,
  mDB(db) ,
  mInfo(0) ,
  mInfoSize(0) ,
  mNumChannels(0) ,
  mDiskCache(aCache) 
{

  NS_INIT_REFCNT();
  NS_ASSERTION(mDiskCache, "Must have an nsNetDiskCache");
  NS_ADDREF(mDiskCache);
}

// mem alloced. so caller should do free() on key. 
NS_IMETHODIMP
nsDiskCacheRecord::Init(const char* key, PRUint32 length) 
{
  NS_NewFileSpec(getter_AddRefs(mFile));
  if(!mFile)
    return NS_ERROR_OUT_OF_MEMORY ;

  // copy key
  mKeyLength = length ;
  mKey = NS_STATIC_CAST(char*, nsAllocator::Alloc(mKeyLength*sizeof(char))) ;
  if(!mKey) 
    return NS_ERROR_OUT_OF_MEMORY ;

  memcpy(mKey, key, length) ;

  // get RecordID
  // FUR!! Another disk access ?  If called from GetCachedData, ID is already known
  mDB->GetID(key, length, &mRecordID) ;
  // FUR - check for GetID failure

  // setup the file name
  nsCOMPtr<nsIFileSpec> dbFolder ;
  mDiskCache->GetDiskCacheFolder(getter_AddRefs(dbFolder)) ;

  nsresult rv = mFile->FromFileSpec(dbFolder) ;
  if(NS_FAILED(rv))
    return NS_ERROR_FAILURE ;

  // dir is a hash result of mRecordID%32, hope it's enough 
  char filename[9], dirName[3] ;
  
  // FUR!! - should the format string be "%.02x".  How does this work !?
  PR_snprintf(dirName, 3, "%.2x", (((PRUint32)mRecordID) % 32)) ;
  mFile->AppendRelativeUnixPath(dirName) ;

  // FUR!! - should the format string be "%.08x".  How does this work !?
  PR_snprintf(filename, 9, "%.8x", mRecordID) ;
  mFile->AppendRelativeUnixPath(filename) ;

  return NS_OK ;
}

nsDiskCacheRecord::~nsDiskCacheRecord()
{
//  printf(" ~nsDiskCacheRecord()\n") ;
  if(mKey)
    nsAllocator::Free(mKey) ;
  if(mMetaData)
    nsAllocator::Free(mMetaData) ;

  NS_IF_RELEASE(mDiskCache);
}

//
// Implement nsISupports methods
//
NS_IMPL_ISUPPORTS(nsDiskCacheRecord, NS_GET_IID(nsINetDataCacheRecord))

///////////////////////////////////////////////////////////////////////
// nsINetDataCacheRecord methods

// yes, mem alloced on *_retval.
NS_IMETHODIMP
nsDiskCacheRecord::GetKey(PRUint32 *length, char** _retval) 
{
  if(!_retval)
    return NS_ERROR_NULL_POINTER ;

  *length = mKeyLength ;
  *_retval = NS_STATIC_CAST(char*, nsAllocator::Alloc(mKeyLength*sizeof(char))) ;
  if(!*_retval)
    return NS_ERROR_OUT_OF_MEMORY ;

  memcpy(*_retval, mKey, mKeyLength) ;

  return NS_OK ;
}

NS_IMETHODIMP
nsDiskCacheRecord::GetRecordID(PRInt32* aRecordID)
{
  *aRecordID = mRecordID ;
  return NS_OK ;
}

// yes, mem alloced on *_retval. 
NS_IMETHODIMP
nsDiskCacheRecord::GetMetaData(PRUint32 *length, char **_retval) 
{
  if(!_retval)
    return NS_ERROR_NULL_POINTER ;

  // always null the return value first. 
  *_retval = nsnull ;

  *length = mMetaDataLength ;

  if(mMetaDataLength) {
    *_retval = NS_STATIC_CAST(char*, nsAllocator::Alloc(mMetaDataLength*sizeof(char))) ;
    if(!*_retval)
      return NS_ERROR_OUT_OF_MEMORY ;

    memcpy(*_retval, mMetaData, mMetaDataLength) ;
  }

  return NS_OK ;
}

NS_IMETHODIMP
nsDiskCacheRecord::SetMetaData(PRUint32 length, const char* data)
{
  // set the mMetaData
  mMetaDataLength = length ;
  if(mMetaData)
    nsAllocator::Free(mMetaData) ;
  mMetaData = NS_STATIC_CAST(char*, nsAllocator::Alloc(mMetaDataLength*sizeof(char))) ;
  if(!mMetaData) {
    return NS_ERROR_OUT_OF_MEMORY ;
  }
  memcpy(mMetaData, data, length) ;

  // Generate mInfo
  nsresult rv = GenInfo() ;
  if(NS_FAILED(rv))
    return rv ;

  // write through into mDB
  rv = mDB->Put(mRecordID, mInfo, mInfoSize) ;

  // FUR - mInfo leaking ?
  return rv ;
}

NS_IMETHODIMP
nsDiskCacheRecord::GetStoredContentLength(PRUint32 *aStoredContentLength)
{
  return mFile->GetFileSize(aStoredContentLength) ;
}

// untill nsIFileSpec::Truncate() is in, we have to do all this ugly stuff
NS_IMETHODIMP
nsDiskCacheRecord::SetStoredContentLength(PRUint32 aStoredContentLength)
{
  PRUint32 len = 0 ;
  nsresult rv = mFile->GetFileSize(&len) ;
  if(NS_FAILED(rv))
    return rv ;

  if(len < aStoredContentLength)
  {
    NS_ERROR("Error: can not set filesize to something bigger than itself.\n") ;
    return NS_ERROR_FAILURE ;
  }
  else 
    return mFile->Truncate(aStoredContentLength) ;
}

NS_IMETHODIMP
nsDiskCacheRecord::Delete(void)
{
  if(mNumChannels)
    return NS_ERROR_NOT_AVAILABLE ;

  PRUint32 len ;
  mFile->GetFileSize(&len) ;

  nsFileSpec cache_file ;
  nsresult rv = mFile->GetFileSpec(&cache_file) ;

  if(NS_FAILED(rv)) 
    return NS_ERROR_FAILURE ;

  cache_file.Delete(PR_TRUE) ;

  // updata the storage size
  mDiskCache->m_StorageInUse -= len ;

  rv = mDB->Del(mRecordID, mKey, mKeyLength) ;
  if(NS_FAILED(rv)) 
    return NS_ERROR_FAILURE ;
  else
    return NS_OK ;
}

NS_IMETHODIMP
nsDiskCacheRecord::GetFilename(nsIFileSpec * *aFilename) 
{
  if(!aFilename)
    return NS_ERROR_NULL_POINTER ;

  *aFilename = mFile ;
  NS_ADDREF(*aFilename) ;

  return NS_OK ;
}

NS_IMETHODIMP
nsDiskCacheRecord::NewChannel(nsILoadGroup *loadGroup, nsIChannel **_retval) 
{
  nsDiskCacheRecordChannel* channel = new nsDiskCacheRecordChannel(this, loadGroup) ;
  if(!channel) 
    return NS_ERROR_OUT_OF_MEMORY ;

  nsresult rv = channel->Init() ;
  if(NS_FAILED(rv))
    return rv ;

  NS_ADDREF(channel) ;
  *_retval = NS_STATIC_CAST(nsIChannel*, channel) ;
  return NS_OK ;
}

//////////////////////////////////////////////////////////////////////////
// nsDiskCacheRecord methods

// file name is represented by a url string. I hope this would be more
// generic
nsresult
nsDiskCacheRecord::GenInfo() 
{
  if(mInfo)
    nsAllocator::Free(mInfo) ;

  char* file_url=nsnull ;
  PRUint32 name_len ;
  mFile->GetURLString(&file_url) ;
  name_len = PL_strlen(file_url)+1 ;

  mInfoSize  = sizeof(PRUint32) ;    // checksum for mInfoSize
  mInfoSize += sizeof(PRInt32) ;     // RecordID
  mInfoSize += sizeof(PRUint32) ;    // key length
  mInfoSize += mKeyLength ;          // key 
  mInfoSize += sizeof(PRUint32) ;    // metadata length
  mInfoSize += mMetaDataLength ;     // metadata 
  mInfoSize += sizeof(PRUint32) ;    // filename length
  mInfoSize += name_len ;            // filename 

  void* newInfo = nsAllocator::Alloc(mInfoSize*sizeof(char)) ;
  if(!newInfo) {
    return NS_ERROR_OUT_OF_MEMORY ;
  }

  // copy the checksum mInfoSize
  char* cur_ptr = NS_STATIC_CAST(char*, newInfo) ;
  COPY_INT32(cur_ptr, &mInfoSize) ;
  cur_ptr += sizeof(PRUint32) ;

  // copy RecordID
  COPY_INT32(cur_ptr, &mRecordID) ;
  cur_ptr += sizeof(PRInt32) ;

  // copy key length
  COPY_INT32(cur_ptr, &mKeyLength) ;
  cur_ptr += sizeof(PRUint32) ;

  // copy key
  memcpy(cur_ptr, mKey, mKeyLength) ;
  cur_ptr += mKeyLength ;

  // copy metadata length
  COPY_INT32(cur_ptr, &mMetaDataLength) ;
  cur_ptr += sizeof(PRUint32) ;

  // copy metadata
  memcpy(cur_ptr, mMetaData, mMetaDataLength) ;
  cur_ptr += mMetaDataLength ;

  // copy file name length 
  COPY_INT32(cur_ptr, &name_len) ;
  cur_ptr += sizeof(PRUint32) ;

  // copy file name
  memcpy(cur_ptr, file_url, name_len) ;
  cur_ptr += name_len ;

  PR_ASSERT(cur_ptr == NS_STATIC_CAST(char*, newInfo) + mInfoSize);
  mInfo = newInfo ;

  return NS_OK ;
}

/*
 * This Method suppose to get all the info from the db record
 * and set them to accroding members. the original values
 * will all be overwritten. only minimal error checking is performed.
 */
NS_IMETHODIMP
nsDiskCacheRecord::RetrieveInfo(void* aInfo, PRUint32 aInfoLength) 
{
  // reset everything
  if(mInfo) {
    nsAllocator::Free(mInfo) ;
    mInfo = nsnull ;
  }

  if(mKey) {
    nsAllocator::Free(mKey) ;
    mKey = nsnull ;
  }
  if(mMetaData) {
    nsAllocator::Free(mMetaData) ;
    mMetaData = nsnull ;
  }

  char * cur_ptr = NS_STATIC_CAST(char*, aInfo) ;

  char* file_url ;
  PRUint32 name_len ;

  // set mInfoSize 
  COPY_INT32(&mInfoSize, cur_ptr) ;
  cur_ptr += sizeof(PRUint32) ;

  // check this at least
  if(mInfoSize != aInfoLength) 
    return NS_ERROR_FAILURE ;

  // set mRecordID 
  COPY_INT32(&mRecordID, cur_ptr) ;
  cur_ptr += sizeof(PRInt32) ;

  // set mKeyLength
  COPY_INT32(&mKeyLength, cur_ptr) ;
  cur_ptr += sizeof(PRUint32) ;

  // set mKey
  mKey = NS_STATIC_CAST(char*, nsAllocator::Alloc(mKeyLength*sizeof(char))) ;
  if(!mKey) 
    return NS_ERROR_OUT_OF_MEMORY ;
  
  memcpy(mKey, cur_ptr, mKeyLength) ;
  cur_ptr += mKeyLength ;

  PRInt32 id ;
  mDB->GetID(mKey, mKeyLength, &id) ;
  NS_ASSERTION(id==mRecordID, "\t ++++++ bad record, somethings wrong\n") ;

  // set mMetaDataLength
  COPY_INT32(&mMetaDataLength, cur_ptr) ;
  cur_ptr += sizeof(PRUint32) ;

  // set mMetaData
  mMetaData = NS_STATIC_CAST(char*, nsAllocator::Alloc(mMetaDataLength*sizeof(char))) ;
  if(!mMetaData) 
    return NS_ERROR_OUT_OF_MEMORY ;
  
  memcpy(mMetaData, cur_ptr, mMetaDataLength) ;
  cur_ptr += mMetaDataLength ;

  // get mFile name length
  COPY_INT32(&name_len, cur_ptr) ;
  cur_ptr += sizeof(PRUint32) ;

  // get mFile native name
  file_url = NS_STATIC_CAST(char*, nsAllocator::Alloc(name_len*sizeof(char))) ;
  if(!file_url) 
    return NS_ERROR_OUT_OF_MEMORY ;
  
  memcpy(file_url, cur_ptr, name_len) ;
  cur_ptr += name_len ;

  PR_ASSERT(cur_ptr == NS_STATIC_CAST(char*, aInfo) + mInfoSize);

  // create mFile if Init() isn't called 
  if(!mFile) {
    NS_NewFileSpec(getter_AddRefs(mFile));
    if(!mFile)
      return NS_ERROR_OUT_OF_MEMORY ;
  }

  // setup mFile
  mFile->SetURLString(file_url) ;

  return NS_OK ;
}

