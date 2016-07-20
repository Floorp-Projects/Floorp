/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheFileIOManager__h__
#define CacheFileIOManager__h__

#include "CacheIOThread.h"
#include "CacheStorageService.h"
#include "nsIEventTarget.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "mozilla/Atomics.h"
#include "mozilla/SHA1.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "prio.h"

//#define DEBUG_HANDLES 1

class nsIFile;
class nsITimer;
class nsIDirectoryEnumerator;
class nsILoadContextInfo;

namespace mozilla {
namespace net {

class CacheFile;
class CacheFileIOListener;

#ifdef DEBUG_HANDLES
class CacheFileHandlesEntry;
#endif

#define ENTRIES_DIR "entries"
#define DOOMED_DIR  "doomed"
#define TRASH_DIR   "trash"


class CacheFileHandle : public nsISupports
{
public:
  enum class PinningStatus : uint32_t {
    UNKNOWN,
    NON_PINNED,
    PINNED
  };

  NS_DECL_THREADSAFE_ISUPPORTS
  bool DispatchRelease();

  CacheFileHandle(const SHA1Sum::Hash *aHash, bool aPriority, PinningStatus aPinning);
  CacheFileHandle(const nsACString &aKey, bool aPriority, PinningStatus aPinning);
  void Log();
  bool IsDoomed() const { return mIsDoomed; }
  const SHA1Sum::Hash *Hash() const { return mHash; }
  int64_t FileSize() const { return mFileSize; }
  uint32_t FileSizeInK() const;
  bool IsPriority() const { return mPriority; }
  bool FileExists() const { return mFileExists; }
  bool IsClosed() const { return mClosed; }
  bool IsSpecialFile() const { return mSpecialFile; }
  nsCString & Key() { return mKey; }

  // Returns false when this handle has been doomed based on the pinning state update.
  bool SetPinned(bool aPinned);

  // Memory reporting
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

private:
  friend class CacheFileIOManager;
  friend class CacheFileHandles;
  friend class ReleaseNSPRHandleEvent;

  virtual ~CacheFileHandle();

  const SHA1Sum::Hash *mHash;
  mozilla::Atomic<bool, ReleaseAcquire> mIsDoomed;
  mozilla::Atomic<bool, ReleaseAcquire> mClosed;

  // mPriority and mSpecialFile are plain "bool", not "bool:1", so as to
  // avoid bitfield races with the byte containing mInvalid et al.  See
  // bug 1278502.
  bool const           mPriority;
  bool const           mSpecialFile;

  // These bit flags are all accessed only on the IO thread
  bool                 mInvalid : 1;
  bool                 mFileExists : 1; // This means that the file should exists,
                                        // but it can be still deleted by OS/user
                                        // and then a subsequent OpenNSPRFileDesc()
                                        // will fail.

  // Both initially false.  Can be raised to true only when this handle is to be doomed
  // during the period when the pinning status is unknown.  After the pinning status
  // determination we check these flags and possibly doom.
  // These flags are only accessed on the IO thread.
  bool                 mDoomWhenFoundPinned : 1;
  bool                 mDoomWhenFoundNonPinned : 1;
  // For existing files this is always pre-set to UNKNOWN.  The status is udpated accordingly
  // after the matadata has been parsed.
  // For new files the flag is set according to which storage kind is opening
  // the cache entry and remains so for the handle's lifetime.
  // The status can only change from UNKNOWN (if set so initially) to one of PINNED or NON_PINNED
  // and it stays unchanged afterwards.
  // This status is only accessed on the IO thread.
  PinningStatus        mPinning;

  nsCOMPtr<nsIFile>    mFile;
  int64_t              mFileSize;
  PRFileDesc          *mFD;  // if null then the file doesn't exists on the disk
  nsCString            mKey;
};

class CacheFileHandles {
public:
  CacheFileHandles();
  ~CacheFileHandles();

  nsresult GetHandle(const SHA1Sum::Hash *aHash, CacheFileHandle **_retval);
  nsresult NewHandle(const SHA1Sum::Hash *aHash, bool aPriority,
                     CacheFileHandle::PinningStatus aPinning, CacheFileHandle **_retval);
  void     RemoveHandle(CacheFileHandle *aHandlle);
  void     GetAllHandles(nsTArray<RefPtr<CacheFileHandle> > *_retval);
  void     GetActiveHandles(nsTArray<RefPtr<CacheFileHandle> > *_retval);
  void     ClearAll();
  uint32_t HandleCount();

#ifdef DEBUG_HANDLES
  void     Log(CacheFileHandlesEntry *entry);
#endif

  // Memory reporting
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  class HandleHashKey : public PLDHashEntryHdr
  {
  public:
    typedef const SHA1Sum::Hash& KeyType;
    typedef const SHA1Sum::Hash* KeyTypePointer;

    explicit HandleHashKey(KeyTypePointer aKey)
    {
      MOZ_COUNT_CTOR(HandleHashKey);
      mHash = MakeUnique<uint8_t[]>(SHA1Sum::kHashSize);
      memcpy(mHash.get(), aKey, sizeof(SHA1Sum::Hash));
    }
    HandleHashKey(const HandleHashKey& aOther)
    {
      NS_NOTREACHED("HandleHashKey copy constructor is forbidden!");
    }
    ~HandleHashKey()
    {
      MOZ_COUNT_DTOR(HandleHashKey);
    }

    bool KeyEquals(KeyTypePointer aKey) const
    {
      return memcmp(mHash.get(), aKey, sizeof(SHA1Sum::Hash)) == 0;
    }
    static KeyTypePointer KeyToPointer(KeyType aKey)
    {
      return &aKey;
    }
    static PLDHashNumber HashKey(KeyTypePointer aKey)
    {
      return (reinterpret_cast<const uint32_t *>(aKey))[0];
    }

    void AddHandle(CacheFileHandle* aHandle);
    void RemoveHandle(CacheFileHandle* aHandle);
    already_AddRefed<CacheFileHandle> GetNewestHandle();
    void GetHandles(nsTArray<RefPtr<CacheFileHandle> > &aResult);

    SHA1Sum::Hash *Hash() const
    {
      return reinterpret_cast<SHA1Sum::Hash*>(mHash.get());
    }
    bool IsEmpty() const { return mHandles.Length() == 0; }

    enum { ALLOW_MEMMOVE = true };

#ifdef DEBUG
    void AssertHandlesState();
#endif

    // Memory reporting
    size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  private:
    // We can't make this UniquePtr<SHA1Sum::Hash>, because you can't have
    // UniquePtrs with known bounds.  So we settle for this representation
    // and using appropriate casts when we need to access it as a
    // SHA1Sum::Hash.
    UniquePtr<uint8_t[]> mHash;
    // Use weak pointers since the hash table access is on a single thread
    // only and CacheFileHandle removes itself from this table in its dtor
    // that may only be called on the same thread as we work with the hashtable
    // since we dispatch its Release() to this thread.
    nsTArray<CacheFileHandle*> mHandles;
  };

private:
  nsTHashtable<HandleHashKey> mTable;
};

////////////////////////////////////////////////////////////////////////////////

class OpenFileEvent;
class ReadEvent;
class WriteEvent;
class MetadataWriteScheduleEvent;
class CacheFileContextEvictor;

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
  NS_IMETHOD OnFileRenamed(CacheFileHandle *aHandle, nsresult aResult) = 0;

  virtual bool IsKilled() { return false; }
};

NS_DEFINE_STATIC_IID_ACCESSOR(CacheFileIOListener, CACHEFILEIOLISTENER_IID)


class CacheFileIOManager : public nsITimerCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  enum {
    OPEN         =  0U,
    CREATE       =  1U,
    CREATE_NEW   =  2U,
    PRIORITY     =  4U,
    SPECIAL_FILE =  8U,
    PINNED       = 16U
  };

  CacheFileIOManager();

  static nsresult Init();
  static nsresult Shutdown();
  static nsresult OnProfile();
  static already_AddRefed<nsIEventTarget> IOTarget();
  static already_AddRefed<CacheIOThread> IOThread();
  static bool IsOnIOThread();
  static bool IsOnIOThreadOrCeased();
  static bool IsShutdown();

  // Make aFile's WriteMetadataIfNeeded be called automatically after
  // a short interval.
  static nsresult ScheduleMetadataWrite(CacheFile * aFile);
  // Remove aFile from the scheduling registry array.
  // WriteMetadataIfNeeded will not be automatically called.
  static nsresult UnscheduleMetadataWrite(CacheFile * aFile);
  // Shuts the scheduling off and flushes all pending metadata writes.
  static nsresult ShutdownMetadataWriteScheduling();

  static nsresult OpenFile(const nsACString &aKey,
                           uint32_t aFlags, CacheFileIOListener *aCallback);
  static nsresult Read(CacheFileHandle *aHandle, int64_t aOffset,
                       char *aBuf, int32_t aCount,
                       CacheFileIOListener *aCallback);
  static nsresult Write(CacheFileHandle *aHandle, int64_t aOffset,
                        const char *aBuf, int32_t aCount, bool aValidate,
                        bool aTruncate, CacheFileIOListener *aCallback);
  // PinningDoomRestriction:
  // NO_RESTRICTION
  //    no restriction is checked, the file is simply always doomed
  // DOOM_WHEN_(NON)_PINNED, we branch based on the pinning status of the handle:
  //   UNKNOWN: the handle is marked to be doomed when later found (non)pinned
  //   PINNED/NON_PINNED: doom only when the restriction matches the pin status
  //      and the handle has not yet been required to doom during the UNKNOWN
  //      period
  enum PinningDoomRestriction {
    NO_RESTRICTION,
    DOOM_WHEN_NON_PINNED,
    DOOM_WHEN_PINNED
  };
  static nsresult DoomFile(CacheFileHandle *aHandle,
                           CacheFileIOListener *aCallback);
  static nsresult DoomFileByKey(const nsACString &aKey,
                                CacheFileIOListener *aCallback);
  static nsresult ReleaseNSPRHandle(CacheFileHandle *aHandle);
  static nsresult TruncateSeekSetEOF(CacheFileHandle *aHandle,
                                     int64_t aTruncatePos, int64_t aEOFPos,
                                     CacheFileIOListener *aCallback);
  static nsresult RenameFile(CacheFileHandle *aHandle,
                             const nsACString &aNewName,
                             CacheFileIOListener *aCallback);
  static nsresult EvictIfOverLimit();
  static nsresult EvictAll();
  static nsresult EvictByContext(nsILoadContextInfo *aLoadContextInfo,
                                 bool aPinning);

  static nsresult InitIndexEntry(CacheFileHandle *aHandle,
                                 uint32_t         aAppId,
                                 bool             aAnonymous,
                                 bool             aInIsolatedMozBrowser,
                                 bool             aPinning);
  static nsresult UpdateIndexEntry(CacheFileHandle *aHandle,
                                   const uint32_t  *aFrecency,
                                   const uint32_t  *aExpirationTime);

  static nsresult UpdateIndexEntry();

  enum EEnumerateMode {
    ENTRIES,
    DOOMED
  };

  static void GetCacheDirectory(nsIFile** result);
#if defined(MOZ_WIDGET_ANDROID)
  static void GetProfilelessCacheDirectory(nsIFile** result);
#endif

  // Calls synchronously OnEntryInfo for an entry with the given hash.
  // Tries to find an existing entry in the service hashtables first, if not
  // found, loads synchronously from disk file.
  // Callable on the IO thread only.
  static nsresult GetEntryInfo(const SHA1Sum::Hash *aHash,
                               CacheStorageService::EntryInfoCallback *aCallback);

  // Memory reporting
  static size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);
  static size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

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
  friend class RenameFileEvent;
  friend class CacheIndex;
  friend class MetadataWriteScheduleEvent;
  friend class CacheFileContextEvictor;

  virtual ~CacheFileIOManager();

  nsresult InitInternal();
  nsresult ShutdownInternal();

  nsresult OpenFileInternal(const SHA1Sum::Hash *aHash,
                            const nsACString &aKey,
                            uint32_t aFlags,
                            CacheFileHandle **_retval);
  nsresult OpenSpecialFileInternal(const nsACString &aKey,
                                   uint32_t aFlags,
                                   CacheFileHandle **_retval);
  nsresult CloseHandleInternal(CacheFileHandle *aHandle);
  nsresult ReadInternal(CacheFileHandle *aHandle, int64_t aOffset,
                        char *aBuf, int32_t aCount);
  nsresult WriteInternal(CacheFileHandle *aHandle, int64_t aOffset,
                         const char *aBuf, int32_t aCount, bool aValidate,
                         bool aTruncate);
  nsresult DoomFileInternal(CacheFileHandle *aHandle,
                            PinningDoomRestriction aPinningStatusRestriction = NO_RESTRICTION);
  nsresult DoomFileByKeyInternal(const SHA1Sum::Hash *aHash);
  nsresult MaybeReleaseNSPRHandleInternal(CacheFileHandle *aHandle,
                                     bool aIgnoreShutdownLag = false);
  nsresult TruncateSeekSetEOFInternal(CacheFileHandle *aHandle,
                                      int64_t aTruncatePos, int64_t aEOFPos);
  nsresult RenameFileInternal(CacheFileHandle *aHandle,
                              const nsACString &aNewName);
  nsresult EvictIfOverLimitInternal();
  nsresult OverLimitEvictionInternal();
  nsresult EvictAllInternal();
  nsresult EvictByContextInternal(nsILoadContextInfo *aLoadContextInfo,
                                  bool aPinning);

  nsresult TrashDirectory(nsIFile *aFile);
  static void OnTrashTimer(nsITimer *aTimer, void *aClosure);
  nsresult StartRemovingTrash();
  nsresult RemoveTrashInternal();
  nsresult FindTrashDirToRemove();

  nsresult CreateFile(CacheFileHandle *aHandle);
  static void HashToStr(const SHA1Sum::Hash *aHash, nsACString &_retval);
  static nsresult StrToHash(const nsACString &aHash, SHA1Sum::Hash *_retval);
  nsresult GetFile(const SHA1Sum::Hash *aHash, nsIFile **_retval);
  nsresult GetSpecialFile(const nsACString &aKey, nsIFile **_retval);
  nsresult GetDoomedFile(nsIFile **_retval);
  nsresult IsEmptyDirectory(nsIFile *aFile, bool *_retval);
  nsresult CheckAndCreateDir(nsIFile *aFile, const char *aDir,
                             bool aEnsureEmptyDir);
  nsresult CreateCacheTree();
  nsresult OpenNSPRHandle(CacheFileHandle *aHandle, bool aCreate = false);
  void     NSPRHandleUsed(CacheFileHandle *aHandle);

  // Removing all cache files during shutdown
  nsresult SyncRemoveDir(nsIFile *aFile, const char *aDir);
  void     SyncRemoveAllCacheFiles();

  nsresult ScheduleMetadataWriteInternal(CacheFile * aFile);
  nsresult UnscheduleMetadataWriteInternal(CacheFile * aFile);
  nsresult ShutdownMetadataWriteSchedulingInternal();

  static nsresult CacheIndexStateChanged();
  nsresult CacheIndexStateChangedInternal();

  // Smart size calculation. UpdateSmartCacheSize() must be called on IO thread.
  // It is called in EvictIfOverLimitInternal() just before we decide whether to
  // start overlimit eviction or not and also in OverLimitEvictionInternal()
  // before we start an eviction loop.
  nsresult UpdateSmartCacheSize(int64_t aFreeSpace);

  // Memory reporting (private part)
  size_t SizeOfExcludingThisInternal(mozilla::MallocSizeOf mallocSizeOf) const;

  static StaticRefPtr<CacheFileIOManager> gInstance;

  TimeStamp                            mStartTime;
  // Set true on the IO thread, CLOSE level as part of the internal shutdown
  // procedure.
  bool                                 mShuttingDown;
  RefPtr<CacheIOThread>                mIOThread;
  nsCOMPtr<nsIFile>                    mCacheDirectory;
#if defined(MOZ_WIDGET_ANDROID)
  // On Android we add the active profile directory name between the path
  // and the 'cache2' leaf name.  However, to delete any leftover data from
  // times before we were doing it, we still need to access the directory
  // w/o the profile name in the path.  Here it is stored.
  nsCOMPtr<nsIFile>                    mCacheProfilelessDirectory;
#endif
  bool                                 mTreeCreated;
  CacheFileHandles                     mHandles;
  nsTArray<CacheFileHandle *>          mHandlesByLastUsed;
  nsTArray<CacheFileHandle *>          mSpecialHandles;
  nsTArray<RefPtr<CacheFile> >         mScheduledMetadataWrites;
  nsCOMPtr<nsITimer>                   mMetadataWritesTimer;
  bool                                 mOverLimitEvicting;
  bool                                 mRemovingTrashDirs;
  nsCOMPtr<nsITimer>                   mTrashTimer;
  nsCOMPtr<nsIFile>                    mTrashDir;
  nsCOMPtr<nsIDirectoryEnumerator>     mTrashDirEnumerator;
  nsTArray<nsCString>                  mFailedTrashDirs;
  RefPtr<CacheFileContextEvictor>      mContextEvictor;
  TimeStamp                            mLastSmartSizeTime;
};

} // namespace net
} // namespace mozilla

#endif
