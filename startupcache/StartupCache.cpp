/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Startup Cache.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Benedict Hsieh <bhsieh@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "prio.h"
#include "prtypes.h"
#include "pldhash.h"
#include "mozilla/scache/StartupCache.h"

#include "nsAutoPtr.h"
#include "nsClassHashtable.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIClassInfo.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIOutputStream.h"
#include "nsIStartupCache.h"
#include "nsIStorageStream.h"
#include "nsIStreamBufferAccess.h"
#include "nsIStringStream.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "nsIZipWriter.h"
#include "nsIZipReader.h"
#include "nsWeakReference.h"
#include "nsZipArchive.h"

#ifdef IS_BIG_ENDIAN
#define SC_ENDIAN "big"
#else
#define SC_ENDIAN "little"
#endif

#if PR_BYTES_PER_WORD == 4
#define SC_WORDSIZE "4"
#else
#define SC_WORDSIZE "8"
#endif

namespace mozilla {
namespace scache {

static const char sStartupCacheName[] = "startupCache." SC_WORDSIZE "." SC_ENDIAN;
static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);

StartupCache*
StartupCache::GetSingleton() 
{
  if (!gStartupCache)
    StartupCache::InitSingleton();

  return StartupCache::gStartupCache;
}

void
StartupCache::DeleteSingleton()
{
  delete StartupCache::gStartupCache;
}

nsresult
StartupCache::InitSingleton() 
{
  nsresult rv;
  StartupCache::gStartupCache = new StartupCache();

  rv = StartupCache::gStartupCache->Init();
  if (NS_FAILED(rv)) {
    delete StartupCache::gStartupCache;
    StartupCache::gStartupCache = nsnull;
  }
  return rv;
}

StartupCache* StartupCache::gStartupCache;
PRBool StartupCache::gShutdownInitiated;

StartupCache::StartupCache() 
  : mArchive(NULL), mStartupWriteInitiated(PR_FALSE) { }

StartupCache::~StartupCache() 
{
  if (mTimer) {
    mTimer->Cancel();
  }

  // Generally, the in-memory table should be empty here,
  // but in special cases (like Talos Ts tests) we
  // could shut down before we write.
  // This mechanism will change when IO is moved off-thread
  // (bug 586859) or when Talos first-run is changed to allow
  // our timer to work (bug 591471).
  WriteToDisk();
  gStartupCache = nsnull;
}

nsresult
StartupCache::Init() 
{
  nsresult rv;
  mTable.Init();
#ifdef DEBUG
  mWriteObjectMap.Init();
#endif

  mZipW = do_CreateInstance("@mozilla.org/zipwriter;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIFile> file;
  rv = NS_GetSpecialDirectory("ProfLDS",
                              getter_AddRefs(file));
  if (NS_FAILED(rv)) {
    // return silently, this will fail in mochitests's xpcshell process.
    return rv;
  }

  rv = file->AppendNative(NS_LITERAL_CSTRING("startupCache"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Try to create the directory if it's not there yet
  rv = file->Create(nsIFile::DIRECTORY_TYPE, 0777);
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_ALREADY_EXISTS)
    return rv;

  rv = file->AppendNative(NS_LITERAL_CSTRING(sStartupCacheName));
  NS_ENSURE_SUCCESS(rv, rv);
  
  mFile = do_QueryInterface(file);
  NS_ENSURE_TRUE(mFile, NS_ERROR_UNEXPECTED);

  mObserverService = do_GetService("@mozilla.org/observer-service;1");
  
  if (!mObserverService) {
    NS_WARNING("Could not get observerService.");
    return NS_ERROR_UNEXPECTED;
  }
  
  mListener = new StartupCacheListener();  
  rv = mObserverService->AddObserver(mListener, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                     PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mObserverService->AddObserver(mListener, "startupcache-invalidate",
                                     PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = LoadArchive();
  
  // Sometimes we don't have a cache yet, that's ok.
  // If it's corrupted, just remove it and start over.
  if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND) {
    NS_WARNING("Failed to load startupcache file correctly, removing!");
    InvalidateCache();
  }

  mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  // Wait for 10 seconds, then write out the cache.
  rv = mTimer->InitWithFuncCallback(StartupCache::WriteTimeout, this, 600000,
                                    nsITimer::TYPE_ONE_SHOT);

  return rv;
}

nsresult
StartupCache::LoadArchive() 
{
  PRBool exists;
  mArchive = NULL;
  nsresult rv = mFile->Exists(&exists);
  if (NS_FAILED(rv) || !exists)
    return NS_ERROR_FILE_NOT_FOUND;
  
  mArchive = new nsZipArchive();
  return mArchive->OpenArchive(mFile);
}

// NOTE: this will not find a new entry until it has been written to disk!
// Consumer should take ownership of the resulting buffer.
nsresult
StartupCache::GetBuffer(const char* id, char** outbuf, PRUint32* length) 
{
  char* data = NULL;
  PRUint32 len;

  if (!mStartupWriteInitiated) {
    CacheEntry* entry; 
    nsDependentCString idStr(id);
    mTable.Get(idStr, &entry);
    if (entry) {
      data = entry->data;
      len = entry->size;
    }
  }

  if (!data && mArchive) {
    nsZipItem* zipItem = mArchive->GetItem(id);
    if (zipItem) {
      const PRUint8* itemData = mArchive->GetData(zipItem);
      if (!itemData || !mArchive->CheckCRC(zipItem, itemData)) {
        NS_WARNING("StartupCache file corrupted!");
        InvalidateCache();
        return NS_ERROR_FILE_CORRUPTED;
      }

      len = zipItem->Size();
      data = (char*) itemData;
    }
  }

  if (data) {
    *outbuf = new char[len];
    memcpy(*outbuf, data, len);
    *length = len;
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

// Makes a copy of the buffer, client retains ownership of inbuf.
nsresult
StartupCache::PutBuffer(const char* id, const char* inbuf, PRUint32 len) 
{
  nsresult rv;

  if (StartupCache::gShutdownInitiated) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoArrayPtr<char> data(new char[len]);
  memcpy(data, inbuf, len);

  nsDependentCString idStr(id);
  if (!mStartupWriteInitiated) {
    // Cache it for now, we'll write all together later.
    CacheEntry* entry; 

#ifdef DEBUG
    mTable.Get(idStr, &entry);
    NS_ASSERTION(entry == nsnull, "Existing entry in StartupCache.");

    if (mArchive) {
      nsZipItem* zipItem = mArchive->GetItem(id);
      NS_ASSERTION(zipItem == nsnull, "Existing entry in disk StartupCache.");
    }
#endif

    entry = new CacheEntry(data.forget(), len);
    mTable.Put(idStr, entry);
    return NS_OK;
  }
  
  rv = mZipW->Open(mFile, PR_RDWR | PR_CREATE_FILE);
  NS_ENSURE_SUCCESS(rv, rv);  

  // XXX We need to think about whether to write this out every time,
  // or somehow detect a good time to write.  We need to finish writing
  // before shutdown though, and writing also requires a reload of the
  // reader's archive, which probably can't handle having the underlying
  // file change underneath it. Potentially could reload on the next
  // read request, if this is a problem. See Bug 586859.
#ifdef DEBUG
  PRBool hasEntry;
  rv = mZipW->HasEntry(idStr, &hasEntry);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(hasEntry == PR_FALSE, "Existing entry in disk StartupCache.");
#endif

  nsCOMPtr<nsIStringInputStream> stream
    = do_CreateInstance("@mozilla.org/io/string-input-stream;1",
                        &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->AdoptData(data, len);
  NS_ENSURE_SUCCESS(rv, rv);
  data.forget();
  
  rv = mZipW->AddEntryStream(idStr, 0, 0, stream, false);
  NS_ENSURE_SUCCESS(rv, rv);

  // Close the archive so Windows doesn't choke.
  mArchive = NULL;
  rv = mZipW->Close();
  NS_ENSURE_SUCCESS(rv, rv);

  // our reader's view of the archive is outdated now, reload it.
  return LoadArchive();
}

struct CacheWriteHolder
{
  nsCOMPtr<nsIZipWriter> writer;
  nsCOMPtr<nsIStringInputStream> stream;
};

PLDHashOperator
CacheCloseHelper(const nsACString& key, nsAutoPtr<CacheEntry>& data, 
                 void* closure) 
{
  nsresult rv;
 
  CacheWriteHolder* holder = (CacheWriteHolder*) closure;  
  nsIStringInputStream* stream = holder->stream;
  nsIZipWriter* writer = holder->writer;

  stream->ShareData(data->data, data->size);

#ifdef DEBUG
  PRBool hasEntry;
  rv = writer->HasEntry(key, &hasEntry);
  NS_ASSERTION(NS_SUCCEEDED(rv) && hasEntry == PR_FALSE, 
               "Existing entry in disk StartupCache.");
#endif
  rv = writer->AddEntryStream(key, 0, 0, stream, false);
  
  if (NS_FAILED(rv)) {
    NS_WARNING("cache entry deleted but not written to disk.");
  }
  return PL_DHASH_REMOVE;
}

void
StartupCache::WriteToDisk() 
{
  nsresult rv;
  mStartupWriteInitiated = PR_TRUE;

  if (mTable.Count() == 0)
    return;

  rv = mZipW->Open(mFile, PR_RDWR | PR_CREATE_FILE);
  if (NS_FAILED(rv)) {
    NS_WARNING("could not open zipfile for write");
    return;
  } 

  nsCOMPtr<nsIStringInputStream> stream 
    = do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("Couldn't create string input stream.");
    return;
  }

  CacheWriteHolder holder;
  holder.stream = stream;
  holder.writer = mZipW;

  mTable.Enumerate(CacheCloseHelper, &holder);

  // Close the archive so Windows doesn't choke.
  mArchive = NULL;
  mZipW->Close();
      
  // our reader's view of the archive is outdated now, reload it.
  LoadArchive();
  
  return;
}

void
StartupCache::InvalidateCache() 
{
  mTable.Clear();
  mArchive = NULL;

  // This is usually closed, but it's possible to get into
  // an inconsistent state.
  mZipW->Close();
  mFile->Remove(false);
  LoadArchive();
}

void
StartupCache::WriteTimeout(nsITimer *aTimer, void *aClosure)
{
  StartupCache* sc = (StartupCache*) aClosure;
  sc->WriteToDisk();
}

// We don't want to refcount StartupCache, so we'll just
// hold a ref to this and pass it to observerService instead.
NS_IMPL_THREADSAFE_ISUPPORTS1(StartupCacheListener, nsIObserver)

nsresult
StartupCacheListener::Observe(nsISupports *subject, const char* topic, const PRUnichar* data)
{
  nsresult rv = NS_OK;
  if (strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    StartupCache::gShutdownInitiated = PR_TRUE;
  } else if (strcmp(topic, "startupcache-invalidate") == 0) {
    StartupCache* sc = StartupCache::GetSingleton();
    if (sc)
      sc->InvalidateCache();
  }
  return rv;
} 

nsresult
StartupCache::GetDebugObjectOutputStream(nsIObjectOutputStream* aStream,
                                         nsIObjectOutputStream** aOutStream) 
{
  NS_ENSURE_ARG_POINTER(aStream);
#ifdef DEBUG
  StartupCacheDebugOutputStream* stream
    = new StartupCacheDebugOutputStream(aStream, &mWriteObjectMap);
  NS_ADDREF(*aOutStream = stream);
#else
  NS_ADDREF(*aOutStream = aStream);
#endif
  
  return NS_OK;
}

// StartupCacheDebugOutputStream implementation
#ifdef DEBUG
NS_IMPL_ISUPPORTS3(StartupCacheDebugOutputStream, nsIObjectOutputStream, 
                   nsIBinaryOutputStream, nsIOutputStream)

PRBool
StartupCacheDebugOutputStream::CheckReferences(nsISupports* aObject)
{
  nsresult rv;
  
  nsCOMPtr<nsIClassInfo> classInfo = do_QueryInterface(aObject);
  if (!classInfo) {
    NS_ERROR("aObject must implement nsIClassInfo");
    return PR_FALSE;
  }
  
  PRUint32 flags;
  rv = classInfo->GetFlags(&flags);
  NS_ENSURE_SUCCESS(rv, rv);
  if (flags & nsIClassInfo::SINGLETON)
    return PR_TRUE;
  
  nsISupportsHashKey* key = mObjectMap->GetEntry(aObject);
  if (key) {
    NS_ERROR("non-singleton aObject is referenced multiple times in this" 
                  "serialization, we don't support that.");
    return PR_FALSE;
  }

  mObjectMap->PutEntry(aObject);
  return PR_TRUE;
}

// nsIObjectOutputStream implementation
nsresult
StartupCacheDebugOutputStream::WriteObject(nsISupports* aObject, PRBool aIsStrongRef)
{
  nsresult rv;
  
  nsCOMPtr<nsISupports> rootObject(do_QueryInterface(aObject));
  
  NS_ASSERTION(rootObject.get() == aObject,
               "bad call to WriteObject -- call WriteCompoundObject!");
  PRBool check = CheckReferences(aObject);
  NS_ENSURE_TRUE(check, NS_ERROR_FAILURE);
  return mBinaryStream->WriteObject(aObject, aIsStrongRef);
}

nsresult
StartupCacheDebugOutputStream::WriteSingleRefObject(nsISupports* aObject)
{
  nsresult rv;
  nsCOMPtr<nsISupports> rootObject(do_QueryInterface(aObject));
  
  NS_ASSERTION(rootObject.get() == aObject,
               "bad call to WriteSingleRefObject -- call WriteCompoundObject!");
  PRBool check = CheckReferences(aObject);
  NS_ENSURE_TRUE(check, NS_ERROR_FAILURE);
  return mBinaryStream->WriteSingleRefObject(aObject);
}

nsresult
StartupCacheDebugOutputStream::WriteCompoundObject(nsISupports* aObject,
                                                const nsIID& aIID,
                                                PRBool aIsStrongRef)
{
  nsresult rv;
  nsCOMPtr<nsISupports> rootObject(do_QueryInterface(aObject));
  
  nsCOMPtr<nsISupports> roundtrip;
  rootObject->QueryInterface(aIID, getter_AddRefs(roundtrip));
  NS_ASSERTION(roundtrip.get() == aObject,
               "bad aggregation or multiple inheritance detected by call to "
               "WriteCompoundObject!");

  PRBool check = CheckReferences(aObject);
  NS_ENSURE_TRUE(check, NS_ERROR_FAILURE);
  return mBinaryStream->WriteCompoundObject(aObject, aIID, aIsStrongRef);
}

nsresult
StartupCacheDebugOutputStream::WriteID(nsID const& aID) 
{
  return mBinaryStream->WriteID(aID);
}

char*
StartupCacheDebugOutputStream::GetBuffer(PRUint32 aLength, PRUint32 aAlignMask)
{
  return mBinaryStream->GetBuffer(aLength, aAlignMask);
}

void
StartupCacheDebugOutputStream::PutBuffer(char* aBuffer, PRUint32 aLength)
{
  mBinaryStream->PutBuffer(aBuffer, aLength);
}
#endif //DEBUG

StartupCacheWrapper* StartupCacheWrapper::gStartupCacheWrapper = nsnull;

NS_IMPL_THREADSAFE_ISUPPORTS1(StartupCacheWrapper, nsIStartupCache)

StartupCacheWrapper* StartupCacheWrapper::GetSingleton() 
{
  if (!gStartupCacheWrapper)
    gStartupCacheWrapper = new StartupCacheWrapper();

  NS_ADDREF(gStartupCacheWrapper);
  return gStartupCacheWrapper;
}

nsresult 
StartupCacheWrapper::GetBuffer(const char* id, char** outbuf, PRUint32* length) 
{
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return sc->GetBuffer(id, outbuf, length);
}

nsresult
StartupCacheWrapper::PutBuffer(const char* id, char* inbuf, PRUint32 length) 
{
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return sc->PutBuffer(id, inbuf, length);
}

nsresult
StartupCacheWrapper::InvalidateCache() 
{
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  sc->InvalidateCache();
  return NS_OK;
}

nsresult 
StartupCacheWrapper::GetDebugObjectOutputStream(nsIObjectOutputStream* stream,
                                                nsIObjectOutputStream** outStream) 
{
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return sc->GetDebugObjectOutputStream(stream, outStream);
}

nsresult
StartupCacheWrapper::StartupWriteComplete(PRBool *complete)
{  
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  *complete = sc->mStartupWriteInitiated && sc->mTable.Count() == 0;
  return NS_OK;
}

nsresult
StartupCacheWrapper::ResetStartupWriteTimer()
{
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  sc->mStartupWriteInitiated = PR_FALSE;
  
  // Init with a shorter timer, for testing convenience.
  sc->mTimer->Cancel();
  sc->mTimer->InitWithFuncCallback(StartupCache::WriteTimeout, sc, 10000,
                                   nsITimer::TYPE_ONE_SHOT);
  return NS_OK;
}

nsresult
StartupCacheWrapper::GetObserver(nsIObserver** obv) {
  StartupCache* sc = StartupCache::GetSingleton();
  if (!sc) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  NS_ADDREF(*obv = sc->mListener);
  return NS_OK;
}

} // namespace scache
} // namespace mozilla
