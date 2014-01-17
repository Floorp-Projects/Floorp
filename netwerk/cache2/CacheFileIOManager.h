/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileIOManager__h__
#define CacheFileIOManager__h__

#include "CacheIOThread.h"
#include "CacheEntriesEnumerator.h"
#include "nsIEventTarget.h"
#include "nsCOMPtr.h"
#include "mozilla/SHA1.h"
#include "nsTArray.h"
#include "nsString.h"
#include "pldhash.h"
#include "prclist.h"
#include "prio.h"

class nsIFile;

namespace mozilla {
namespace net {

class CacheFileHandle : public nsISupports
                      , public PRCList
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  CacheFileHandle(const SHA1Sum::Hash *aHash, bool aPriority);
  bool IsDoomed() { return mIsDoomed; }
  const SHA1Sum::Hash *Hash() { return mHash; }
  int64_t FileSize() { return mFileSize; }
  bool IsPriority() { return mPriority; }
  bool FileExists() { return mFileExists; }
  bool IsClosed() { return mClosed; }
  nsCString & Key() { return mKey; }

private:
  friend class CacheFileIOManager;
  friend class CacheFileHandles;
  friend class ReleaseNSPRHandleEvent;

  virtual ~CacheFileHandle();

  const SHA1Sum::Hash *mHash;
  bool                 mIsDoomed;
  bool                 mRemovingHandle;
  bool                 mPriority;
  bool                 mClosed;
  bool                 mInvalid;
  bool                 mFileExists; // This means that the file should exists,
                                    // but it can be still deleted by OS/user
                                    // and then a subsequent OpenNSPRFileDesc()
                                    // will fail.
  nsCOMPtr<nsIFile>    mFile;
  int64_t              mFileSize;
  PRFileDesc          *mFD;  // if null then the file doesn't exists on the disk
  nsCString            mKey;
};

class CacheFileHandles {
public:
  CacheFileHandles();
  ~CacheFileHandles();

  nsresult Init();
  void     Shutdown();

  nsresult GetHandle(const SHA1Sum::Hash *aHash, bool aReturnDoomed, CacheFileHandle **_retval);
  nsresult NewHandle(const SHA1Sum::Hash *aHash, bool aPriority, CacheFileHandle **_retval);
  void     RemoveHandle(CacheFileHandle *aHandlle);
  void     GetAllHandles(nsTArray<nsRefPtr<CacheFileHandle> > *_retval);
  uint32_t HandleCount();

private:
  static PLDHashNumber HashKey(PLDHashTable *table, const void *key);
  static bool          MatchEntry(PLDHashTable *table,
                                  const PLDHashEntryHdr *entry,
                                  const void *key);
  static void          MoveEntry(PLDHashTable *table,
                                 const PLDHashEntryHdr *from,
                                 PLDHashEntryHdr *to);
  static void          ClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry);

  static const PLDHashTableOps mOps;
  PLDHashTable                 mTable;
  bool                         mInitialized;
};

////////////////////////////////////////////////////////////////////////////////

class OpenFileEvent;
class CloseFileEvent;
class ReadEvent;
class WriteEvent;

#define CACHEFILEIOLISTENER_IID \
{ /* dcaf2ddc-17cf-4242-bca1-8c86936375a5 */       \
  0xdcaf2ddc,                                      \
  0x17cf,                                          \
  0x4242,                                          \
  {0xbc, 0xa1, 0x8c, 0x86, 0x93, 0x63, 0x75, 0xa5} \
}

class CacheFileIOListener : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(CACHEFILEIOLISTENER_IID)

  NS_IMETHOD OnFileOpened(CacheFileHandle *aHandle, nsresult aResult) = 0;
  NS_IMETHOD OnDataWritten(CacheFileHandle *aHandle, const char *aBuf,
                           nsresult aResult) = 0;
  NS_IMETHOD OnDataRead(CacheFileHandle *aHandle, char *aBuf,
                        nsresult aResult) = 0;
  NS_IMETHOD OnFileDoomed(CacheFileHandle *aHandle, nsresult aResult) = 0;
  NS_IMETHOD OnEOFSet(CacheFileHandle *aHandle, nsresult aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(CacheFileIOListener, CACHEFILEIOLISTENER_IID)


class CacheFileIOManager : public nsISupports
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  enum {
    OPEN       = 0U,
    CREATE     = 1U,
    CREATE_NEW = 2U,
    PRIORITY   = 4U,
    NOHASH     = 8U
  };

  CacheFileIOManager();

  static nsresult Init();
  static nsresult Shutdown();
  static nsresult OnProfile();
  static already_AddRefed<nsIEventTarget> IOTarget();
  static already_AddRefed<CacheIOThread> IOThread();

  static nsresult OpenFile(const nsACString &aKey,
                           uint32_t aFlags,
                           CacheFileIOListener *aCallback);
  static nsresult Read(CacheFileHandle *aHandle, int64_t aOffset,
                       char *aBuf, int32_t aCount,
                       CacheFileIOListener *aCallback);
  static nsresult Write(CacheFileHandle *aHandle, int64_t aOffset,
                        const char *aBuf, int32_t aCount, bool aValidate,
                        CacheFileIOListener *aCallback);
  static nsresult DoomFile(CacheFileHandle *aHandle,
                           CacheFileIOListener *aCallback);
  static nsresult DoomFileByKey(const nsACString &aKey,
                                CacheFileIOListener *aCallback);
  static nsresult ReleaseNSPRHandle(CacheFileHandle *aHandle);
  static nsresult TruncateSeekSetEOF(CacheFileHandle *aHandle,
                                     int64_t aTruncatePos, int64_t aEOFPos,
                                     CacheFileIOListener *aCallback);

  enum EEnumerateMode {
    ENTRIES,
    DOOMED
  };

  static nsresult EnumerateEntryFiles(EEnumerateMode aMode,
                                      CacheEntriesEnumerator** aEnumerator);

private:
  friend class CacheFileHandle;
  friend class CacheFileChunk;
  friend class CacheFile;
  friend class ShutdownEvent;
  friend class OpenFileEvent;
  friend class CloseHandleEvent;
  friend class ReadEvent;
  friend class WriteEvent;
  friend class DoomFileEvent;
  friend class DoomFileByKeyEvent;
  friend class ReleaseNSPRHandleEvent;
  friend class TruncateSeekSetEOFEvent;

  virtual ~CacheFileIOManager();

  static nsresult CloseHandle(CacheFileHandle *aHandle);

  nsresult InitInternal();
  nsresult ShutdownInternal();

  nsresult OpenFileInternal(const SHA1Sum::Hash *aHash,
                            uint32_t aFlags,
                            CacheFileHandle **_retval);
  nsresult CloseHandleInternal(CacheFileHandle *aHandle);
  nsresult ReadInternal(CacheFileHandle *aHandle, int64_t aOffset,
                        char *aBuf, int32_t aCount);
  nsresult WriteInternal(CacheFileHandle *aHandle, int64_t aOffset,
                         const char *aBuf, int32_t aCount, bool aValidate);
  nsresult DoomFileInternal(CacheFileHandle *aHandle);
  nsresult DoomFileByKeyInternal(const SHA1Sum::Hash *aHash);
  nsresult ReleaseNSPRHandleInternal(CacheFileHandle *aHandle);
  nsresult TruncateSeekSetEOFInternal(CacheFileHandle *aHandle,
                                      int64_t aTruncatePos, int64_t aEOFPos);

  nsresult CreateFile(CacheFileHandle *aHandle);
  static void GetHashStr(const SHA1Sum::Hash *aHash, nsACString &_retval);
  nsresult GetFile(const SHA1Sum::Hash *aHash, nsIFile **_retval);
  nsresult GetDoomedFile(nsIFile **_retval);
  nsresult CheckAndCreateDir(nsIFile *aFile, const char *aDir);
  nsresult CreateCacheTree();
  nsresult OpenNSPRHandle(CacheFileHandle *aHandle, bool aCreate = false);
  void     NSPRHandleUsed(CacheFileHandle *aHandle);


  static CacheFileIOManager  *gInstance;
  bool                        mShuttingDown;
  nsRefPtr<CacheIOThread>     mIOThread;
  nsCOMPtr<nsIFile>           mCacheDirectory;
  bool                        mTreeCreated;
  CacheFileHandles            mHandles;
  nsTArray<CacheFileHandle *> mHandlesByLastUsed;
};

} // net
} // mozilla

#endif
