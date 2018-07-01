/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nsDumpUtils_h
#define mozilla_nsDumpUtils_h

#include "nsIObserver.h"
#include "base/message_loop.h"
#include "nsXULAppAPI.h"
#include "nsThreadUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include "nsTArray.h"

#ifdef LOG
#undef LOG
#endif

#ifdef ANDROID
#include "android/log.h"
#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "Gecko:DumpUtils", ## __VA_ARGS__)
#else
#define LOG(...)
#endif

#ifdef XP_UNIX // {

/**
 * Abstract base class for something which watches an fd and takes action when
 * we can read from it without blocking.
 */
class FdWatcher
  : public MessageLoopForIO::Watcher
  , public nsIObserver
{
protected:
  MessageLoopForIO::FileDescriptorWatcher mReadWatcher;
  int mFd;

  virtual ~FdWatcher()
  {
    // StopWatching should have run.
    MOZ_ASSERT(mFd == -1);
  }

public:
  FdWatcher()
    : mFd(-1)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  /**
   * Open the fd to watch.  If we encounter an error, return -1.
   */
  virtual int OpenFd() = 0;

  /**
   * Called when you can read() from the fd without blocking.  Note that this
   * function is also called when you're at eof (read() returns 0 in this case).
   */
  virtual void OnFileCanReadWithoutBlocking(int aFd) override = 0;
  virtual void OnFileCanWriteWithoutBlocking(int aFd) override {};

  NS_DECL_THREADSAFE_ISUPPORTS

  /**
   * Initialize this object.  This should be called right after the object is
   * constructed.  (This would go in the constructor, except we interact with
   * XPCOM, which we can't do from a constructor because our refcount is 0 at
   * that point.)
   */
  void Init();

  // Implementations may call this function multiple times if they ensure that

  virtual void StartWatching();

  // Since implementations can call StartWatching() multiple times, they can of
  // course call StopWatching() multiple times.
  virtual void StopWatching();

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!strcmp(aTopic, "xpcom-shutdown"));

    XRE_GetIOMessageLoop()->PostTask(mozilla::NewRunnableMethod(
      "FdWatcher::StopWatching", this, &FdWatcher::StopWatching));

    return NS_OK;
  }
};

typedef void (*FifoCallback)(const nsCString& aInputStr);
struct FifoInfo
{
  nsCString mCommand;
  FifoCallback mCallback;
};
typedef nsTArray<FifoInfo> FifoInfoArray;

class FifoWatcher : public FdWatcher
{
public:
  /**
   * The name of the preference used to enable/disable the FifoWatcher.
   */
  // The length of this array must match the size of the string constant in
  // the definition in nsDumpUtils.cpp. A mismatch will result in a compile-time
  // error.
  static const char kPrefName[38];

  static FifoWatcher* GetSingleton();

  static bool MaybeCreate();

  void RegisterCallback(const nsCString& aCommand, FifoCallback aCallback);

  virtual ~FifoWatcher();

  virtual int OpenFd() override;

  virtual void OnFileCanReadWithoutBlocking(int aFd) override;

private:
  nsAutoCString mDirPath;

  static mozilla::StaticRefPtr<FifoWatcher> sSingleton;

  explicit FifoWatcher(nsCString aPath)
    : mDirPath(aPath)
    , mFifoInfoLock("FifoWatcher.mFifoInfoLock")
  {
  }

  mozilla::Mutex mFifoInfoLock; // protects mFifoInfo
  FifoInfoArray mFifoInfo;
};

typedef void (*PipeCallback)(const uint8_t aRecvSig);
struct SignalInfo
{
  uint8_t mSignal;
  PipeCallback mCallback;
};
typedef nsTArray<SignalInfo> SignalInfoArray;

class SignalPipeWatcher : public FdWatcher
{
public:
  static SignalPipeWatcher* GetSingleton();

  void RegisterCallback(uint8_t aSignal, PipeCallback aCallback);

  void RegisterSignalHandler(uint8_t aSignal = 0);

  virtual ~SignalPipeWatcher();

  virtual int OpenFd() override;

  virtual void StopWatching() override;

  virtual void OnFileCanReadWithoutBlocking(int aFd) override;

private:
  static mozilla::StaticRefPtr<SignalPipeWatcher> sSingleton;

  SignalPipeWatcher()
    : mSignalInfoLock("SignalPipeWatcher.mSignalInfoLock")
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  mozilla::Mutex mSignalInfoLock; // protects mSignalInfo
  SignalInfoArray mSignalInfo;
};

#endif // XP_UNIX }

class nsDumpUtils
{
public:

  enum Mode {
    CREATE,
    CREATE_UNIQUE
  };

  /**
   * This function creates a new unique file based on |aFilename| in a
   * world-readable temp directory. This is the system temp directory
   * or, in the case of Android, the downloads directory. If |aFile| is
   * non-null, it is assumed to point to a folder, and that folder is used
   * instead.
   */
  static nsresult OpenTempFile(const nsACString& aFilename,
                               nsIFile** aFile,
                               const nsACString& aFoldername = EmptyCString(),
                               Mode aMode = CREATE_UNIQUE);
};

#endif
