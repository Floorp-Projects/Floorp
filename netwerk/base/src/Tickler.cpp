/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComponentManagerUtils.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"
#include "prnetdb.h"
#include "Tickler.h"

#ifdef MOZ_USE_WIFI_TICKLER

#include "AndroidBridge.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS1(Tickler, nsISupportsWeakReference)

Tickler::Tickler()
    : mLock("Tickler::mLock")
    , mActive(false)
    , mCanceled(false)
    , mEnabled(false)
    , mDelay(16)
    , mDuration(TimeDuration::FromMilliseconds(400))
    , mFD(nullptr)
{
  MOZ_ASSERT(NS_IsMainThread());
}

Tickler::~Tickler()
{
  // non main thread uses of the tickler should hold weak
  // references to it if they must hold a reference at all
  MOZ_ASSERT(NS_IsMainThread());

  if (mThread)
    mThread->Shutdown();
  if (mTimer)
    mTimer->Cancel();
  if (mFD)
    PR_Close(mFD);
}

nsresult
Tickler::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mTimer);
  MOZ_ASSERT(!mActive);
  MOZ_ASSERT(!mThread);
  MOZ_ASSERT(!mFD);

  AndroidBridge::Bridge()->EnableNetworkNotifications();

  mFD = PR_OpenUDPSocket(PR_AF_INET);
  if (!mFD)
    return NS_ERROR_FAILURE;

  // make sure new socket has a ttl of 1
  // failure is not fatal.
  PRSocketOptionData opt;
  opt.option = PR_SockOpt_IpTimeToLive;
  opt.value.ip_ttl = 1;
  PR_SetSocketOption(mFD, &opt);

  nsresult rv = NS_NewNamedThread("wifi tickler",
                                  getter_AddRefs(mThread));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsITimer> tmpTimer(do_CreateInstance(NS_TIMER_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;

  rv = tmpTimer->SetTarget(mThread);
  if (NS_FAILED(rv))
    return rv;

  mTimer.swap(tmpTimer);

  mAddr.inet.family = PR_AF_INET;
  mAddr.inet.port = PR_htons (4886);
  mAddr.inet.ip = 0;

  return NS_OK;
}

void Tickler::Tickle()
{
  MutexAutoLock lock(mLock);
  MOZ_ASSERT(mThread);
  mLastTickle = TimeStamp::Now();
  if (!mActive)
    MaybeStartTickler();
}

void Tickler::PostCheckTickler()
{
  mLock.AssertCurrentThreadOwns();
  mThread->Dispatch(NS_NewRunnableMethod(this, &Tickler::CheckTickler),
                    NS_DISPATCH_NORMAL);
  return;
}

void Tickler::MaybeStartTicklerUnlocked()
{
  MutexAutoLock lock(mLock);
  MaybeStartTickler();
}

void Tickler::MaybeStartTickler()
{
  mLock.AssertCurrentThreadOwns();
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(
      NS_NewRunnableMethod(this, &Tickler::MaybeStartTicklerUnlocked),
      NS_DISPATCH_NORMAL);
    return;
  }

  if (!mPrefs)
    mPrefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (mPrefs) {
    int32_t val;
    bool boolVal;

    if (NS_SUCCEEDED(mPrefs->GetBoolPref("network.tickle-wifi.enabled", &boolVal)))
      mEnabled = boolVal;

    if (NS_SUCCEEDED(mPrefs->GetIntPref("network.tickle-wifi.duration", &val))) {
      if (val < 1)
        val = 1;
      if (val > 100000)
        val = 100000;
      mDuration = TimeDuration::FromMilliseconds(val);
    }

    if (NS_SUCCEEDED(mPrefs->GetIntPref("network.tickle-wifi.delay", &val))) {
      if (val < 1)
        val = 1;
      if (val > 1000)
        val = 1000;
      mDelay = static_cast<uint32_t>(val);
    }
  }

  PostCheckTickler();
}

void Tickler::CheckTickler()
{
  MutexAutoLock lock(mLock);
  MOZ_ASSERT(mThread == NS_GetCurrentThread());

  bool shouldRun = (!mCanceled) &&
    ((TimeStamp::Now() - mLastTickle) <= mDuration);

  if ((shouldRun && mActive) || (!shouldRun && !mActive))
    return; // no change in state

  if (mActive)
    StopTickler();
  else
    StartTickler();
}

void Tickler::Cancel()
{
  MutexAutoLock lock(mLock);
  MOZ_ASSERT(NS_IsMainThread());
  mCanceled = true;
  if (mThread)
    PostCheckTickler();
}

void Tickler::StopTickler()
{
  mLock.AssertCurrentThreadOwns();
  MOZ_ASSERT(mThread == NS_GetCurrentThread());
  MOZ_ASSERT(mTimer);
  MOZ_ASSERT(mActive);

  mTimer->Cancel();
  mActive = false;
}

class TicklerTimer MOZ_FINAL : public nsITimerCallback
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  TicklerTimer(Tickler *aTickler)
  {
    mTickler = do_GetWeakReference(aTickler);
  }

  ~TicklerTimer() {};

private:
  nsWeakPtr mTickler;
};

void Tickler::StartTickler()
{
  mLock.AssertCurrentThreadOwns();
  MOZ_ASSERT(mThread == NS_GetCurrentThread());
  MOZ_ASSERT(!mActive);
  MOZ_ASSERT(mTimer);

  if (NS_SUCCEEDED(mTimer->InitWithCallback(new TicklerTimer(this),
                                            mEnabled ? mDelay : 1000,
                                            nsITimer::TYPE_REPEATING_SLACK)))
    mActive = true;
}

// argument should be in network byte order
void Tickler::SetIPV4Address(uint32_t address)
{
  mAddr.inet.ip = address;
}

// argument should be in network byte order
void Tickler::SetIPV4Port(uint16_t port)
{
  mAddr.inet.port = port;
}

NS_IMPL_ISUPPORTS1(TicklerTimer, nsITimerCallback)

NS_IMETHODIMP TicklerTimer::Notify(nsITimer *timer)
{
  nsRefPtr<Tickler> tickler = do_QueryReferent(mTickler);
  if (!tickler)
    return NS_ERROR_FAILURE;
  MutexAutoLock lock(tickler->mLock);

  if (!tickler->mFD) {
    tickler->StopTickler();
    return NS_ERROR_FAILURE;
  }

  if (tickler->mCanceled ||
      ((TimeStamp::Now() - tickler->mLastTickle) > tickler->mDuration)) {
    tickler->StopTickler();
    return NS_OK;
  }

  if (!tickler->mEnabled)
    return NS_OK;

  PR_SendTo(tickler->mFD, "", 0, 0, &tickler->mAddr, 0);
  return NS_OK;
}

} // namespace mozilla::net
} // namespace mozilla

#else // not defined MOZ_USE_WIFI_TICKLER

namespace mozilla {
namespace net {
NS_IMPL_ISUPPORTS0(Tickler)
} // namespace mozilla::net
} // namespace mozilla

#endif // defined MOZ_USE_WIFI_TICKLER

