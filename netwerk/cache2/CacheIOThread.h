/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheIOThread__h__
#define CacheIOThread__h__

#include "nsIThreadInternal.h"
#include "nsISupportsImpl.h"
#include "prthread.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "mozilla/Monitor.h"

class nsIRunnable;

namespace mozilla {
namespace net {

class CacheIOThread : public nsIThreadObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITHREADOBSERVER

  CacheIOThread();
  virtual ~CacheIOThread();

  enum ELevel {
    IMMEDIATE,
    DOOM_PRIORITY,
    OPEN_PRIORITY,
    READ_PRIORITY,
    DOOM,
    OPEN,
    READ,
    OPEN_TRUNCATE,
    WRITE,
    CLOSE,
    EVICT,
    LAST_LEVEL
  };

  nsresult Init();
  nsresult Dispatch(nsIRunnable* aRunnable, uint32_t aLevel);
  bool IsCurrentThread();
  nsresult Shutdown();
  already_AddRefed<nsIEventTarget> Target();

private:
  static void ThreadFunc(void* aClosure);
  void ThreadFunc();
  void LoopOneLevel(uint32_t aLevel);
  bool EventsPending(uint32_t aLastLevel = LAST_LEVEL);

  mozilla::Monitor mMonitor;
  PRThread* mThread;
  nsCOMPtr<nsIThread> mXPCOMThread;
  uint32_t mLowestLevelWaiting;
  nsTArray<nsRefPtr<nsIRunnable> > mEventQueue[LAST_LEVEL];

  bool mHasXPCOMEvents;
  bool mShutdown;
};

} // net
} // mozilla

#endif
