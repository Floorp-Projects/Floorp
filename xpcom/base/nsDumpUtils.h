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

using namespace mozilla;

#if defined(XP_LINUX) || defined(__FreeBSD__) // {

/**
 * Abstract base class for something which watches an fd and takes action when
 * we can read from it without blocking.
 */
class FdWatcher : public MessageLoopForIO::Watcher
                , public nsIObserver
{
protected:
  MessageLoopForIO::FileDescriptorWatcher mReadWatcher;
  int mFd;

public:
  FdWatcher()
    : mFd(-1)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual ~FdWatcher()
  {
    // StopWatching should have run.
    MOZ_ASSERT(mFd == -1);
  }

  /**
   * Open the fd to watch.  If we encounter an error, return -1.
   */
  virtual int OpenFd() = 0;

  /**
   * Called when you can read() from the fd without blocking.  Note that this
   * function is also called when you're at eof (read() returns 0 in this case).
   */
  virtual void OnFileCanReadWithoutBlocking(int aFd) = 0;
  virtual void OnFileCanWriteWithoutBlocking(int aFd) {};

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
                     const char16_t* aData)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!strcmp(aTopic, "xpcom-shutdown"));

    XRE_GetIOMessageLoop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &FdWatcher::StopWatching));

    return NS_OK;
  }
};

typedef void (* FifoCallback)(const nsCString& inputStr);
struct FifoInfo {
  nsCString mCommand;
  FifoCallback mCallback;
};
typedef nsTArray<FifoInfo> FifoInfoArray;

class FifoWatcher : public FdWatcher
{
public:
  static FifoWatcher* GetSingleton();

  static bool MaybeCreate();

  void RegisterCallback(const nsCString& aCommand, FifoCallback aCallback);

  virtual ~FifoWatcher();

  virtual int OpenFd();

  virtual void OnFileCanReadWithoutBlocking(int aFd);

private:
  nsAutoCString mDirPath;

  static StaticRefPtr<FifoWatcher> sSingleton;

  FifoWatcher(nsCString aPath)
    : mDirPath(aPath)
  {}

  FifoInfoArray mFifoInfo;
};

typedef void (* PipeCallback)(const uint8_t recvSig);
struct SignalInfo {
  uint8_t mSignal;
  PipeCallback mCallback;
};
typedef nsTArray<SignalInfo> SignalInfoArray;

class SignalPipeWatcher : public FdWatcher
{
public:
  static SignalPipeWatcher* GetSingleton();

  void RegisterCallback(const uint8_t aSignal, PipeCallback aCallback);

  void RegisterSignalHandler(const uint8_t aSignal = 0);

  virtual ~SignalPipeWatcher();

  virtual int OpenFd();

  virtual void StopWatching();

  virtual void OnFileCanReadWithoutBlocking(int aFd);

private:
  static StaticRefPtr<SignalPipeWatcher> sSingleton;

  SignalPipeWatcher()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  SignalInfoArray mSignalInfo;
};

#endif // XP_LINUX }


class nsDumpUtils
{
public:
  /**
   * This function creates a new unique file based on |aFilename| in a
   * world-readable temp directory. This is the system temp directory
   * or, in the case of Android, the downloads directory. If |aFile| is
   * non-null, it is assumed to point to a folder, and that folder is used
   * instead.
   */
  static nsresult OpenTempFile(const nsACString& aFilename,
                        nsIFile** aFile,
                        const nsACString& aFoldername = EmptyCString());
};

#endif
