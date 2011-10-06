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

#ifndef StartupCache_h_
#define StartupCache_h_

#include "prio.h"
#include "prtypes.h"

#include "nsClassHashtable.h"
#include "nsIZipWriter.h"
#include "nsIZipReader.h"
#include "nsComponentManagerUtils.h"
#include "nsZipArchive.h"
#include "nsIStartupCache.h"
#include "nsIStorageStream.h"
#include "nsITimer.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIOutputStream.h"
#include "nsIFile.h"

/**
 * The StartupCache is a persistent cache of simple key-value pairs,
 * where the keys are null-terminated c-strings and the values are 
 * arbitrary data, passed as a (char*, size) tuple. 
 *
 * Clients should use the GetSingleton() static method to access the cache. It 
 * will be available from the end of XPCOM init (NS_InitXPCOM3 in nsXPComInit.cpp), 
 * until XPCOM shutdown begins. The GetSingleton() method will return null if the cache
 * is unavailable. The cache is only provided for libxul builds --
 * it will fail to link in non-libxul builds. The XPCOM interface is provided
 * only to allow compiled-code tests; clients should avoid using it.
 *
 * The API provided is very simple: GetBuffer() returns a buffer that was previously
 * stored in the cache (if any), and PutBuffer() inserts a buffer into the cache.
 * GetBuffer returns a new buffer, and the caller must take ownership of it.
 * PutBuffer will assert if the client attempts to insert a buffer with the same name as
 * an existing entry. The cache makes a copy of the passed-in buffer, so client
 * retains ownership.
 *
 * InvalidateCache() may be called if a client suspects data corruption 
 * or wishes to invalidate for any other reason. This will remove all existing cache data.
 * Finally, getDebugObjectOutputStream() allows debug code to wrap an objectstream
 * with a debug objectstream, to check for multiply-referenced objects. These will
 * generally fail to deserialize correctly, unless they are stateless singletons or the 
 * client maintains their own object data map for deserialization.
 *
 * Writes before the final-ui-startup notification are placed in an intermediate
 * cache in memory, then written out to disk at a later time, to get writes off the
 * startup path. In any case, clients should not rely on being able to GetBuffer()
 * data that is written to the cache, since it may not have been written to disk or
 * another client may have invalidated the cache. In other words, it should be used as
 * a cache only, and not a reliable persistent store.
 *
 * Some utility functions are provided in StartupCacheUtils. These functions wrap the
 * buffers into object streams, which may be useful for serializing objects. Note
 * the above caution about multiply-referenced objects, though -- the streams are just
 * as 'dumb' as the underlying buffers about multiply-referenced objects. They just
 * provide some convenience in writing out data.
 */

namespace mozilla {
namespace scache {

struct CacheEntry 
{
  nsAutoArrayPtr<char> data;
  PRUint32 size;

  CacheEntry() : data(nsnull), size(0) { }

  // Takes possession of buf
  CacheEntry(char* buf, PRUint32 len) : data(buf), size(len) { }

  ~CacheEntry()
  {
  }
};

// We don't want to refcount StartupCache, and ObserverService wants to
// refcount its listeners, so we'll let it refcount this instead.
class StartupCacheListener : public nsIObserver
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

class StartupCache
{

friend class StartupCacheListener;
friend class StartupCacheWrapper;
                                
public:

  // StartupCache methods. See above comments for a more detailed description.

  // Returns a buffer that was previously stored, caller takes ownership. 
  nsresult GetBuffer(const char* id, char** outbuf, PRUint32* length);

  // Stores a buffer. Caller keeps ownership, we make a copy.
  nsresult PutBuffer(const char* id, const char* inbuf, PRUint32 length);

  // Removes the cache file.
  void InvalidateCache();

  // In DEBUG builds, returns a stream that will attempt to check for
  // and disallow multiple writes of the same object.
  nsresult GetDebugObjectOutputStream(nsIObjectOutputStream* aStream,
                                      nsIObjectOutputStream** outStream);

  static StartupCache* GetSingleton();
  static void DeleteSingleton();

private:
  StartupCache();
  ~StartupCache();

  nsresult LoadArchive();
  nsresult Init();
  void WriteToDisk();
  nsresult ResetStartupWriteTimer();
  void WaitOnWriteThread();

  static nsresult InitSingleton();
  static void WriteTimeout(nsITimer *aTimer, void *aClosure);
  static void ThreadedWrite(void *aClosure);

  nsClassHashtable<nsCStringHashKey, CacheEntry> mTable;
  nsAutoPtr<nsZipArchive> mArchive;
  nsCOMPtr<nsILocalFile> mFile;
  
  nsCOMPtr<nsIObserverService> mObserverService;
  nsRefPtr<StartupCacheListener> mListener;
  nsCOMPtr<nsITimer> mTimer;

  bool mStartupWriteInitiated;

  static StartupCache *gStartupCache;
  static bool gShutdownInitiated;
  PRThread *mWriteThread;
#ifdef DEBUG
  nsTHashtable<nsISupportsHashKey> mWriteObjectMap;
#endif
};

// This debug outputstream attempts to detect if clients are writing multiple
// references to the same object. We only support that if that object
// is a singleton.
#ifdef DEBUG
class StartupCacheDebugOutputStream
  : public nsIObjectOutputStream
{  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBJECTOUTPUTSTREAM

  StartupCacheDebugOutputStream (nsIObjectOutputStream* binaryStream,
                                   nsTHashtable<nsISupportsHashKey>* objectMap)
  : mBinaryStream(binaryStream), mObjectMap(objectMap) { }
  
  NS_FORWARD_SAFE_NSIBINARYOUTPUTSTREAM(mBinaryStream)
  NS_FORWARD_SAFE_NSIOUTPUTSTREAM(mBinaryStream)
  
  bool CheckReferences(nsISupports* aObject);
  
  nsCOMPtr<nsIObjectOutputStream> mBinaryStream;
  nsTHashtable<nsISupportsHashKey> *mObjectMap;
};
#endif // DEBUG

// XPCOM wrapper interface provided for tests only.
#define NS_STARTUPCACHE_CID \
      {0xae4505a9, 0x87ab, 0x477c, \
      {0xb5, 0x77, 0xf9, 0x23, 0x57, 0xed, 0xa8, 0x84}}
// contract id: "@mozilla.org/startupcache/cache;1"

class StartupCacheWrapper 
  : public nsIStartupCache
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTARTUPCACHE

  static StartupCacheWrapper* GetSingleton();
  static StartupCacheWrapper *gStartupCacheWrapper;
};

} // namespace scache
} // namespace mozilla
#endif //StartupCache_h_
