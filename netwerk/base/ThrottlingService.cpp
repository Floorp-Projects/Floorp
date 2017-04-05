/* vim: set ts=2 sts=2 et sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


 // Things to think about
 //  * do we need to be multithreaded, or is mt-only ok?

#include "ThrottlingService.h"

#include "nsIHttpChannel.h"
#include "nsIObserverService.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"

#include "mozilla/net/NeckoChild.h"

namespace mozilla {
namespace net{

static const char kEnabledPref[] = "network.throttle.enable";
static const bool kDefaultEnabled = true;
static const char kSuspendPeriodPref[] = "network.throttle.suspend-for";
static const uint32_t kDefaultSuspendPeriod = 2000;
static const char kResumePeriodPref[] = "network.throttle.resume-for";
static const uint32_t kDefaultResumePeriod = 2000;

NS_IMPL_ISUPPORTS(ThrottlingService, nsIThrottlingService, nsIObserver, nsITimerCallback)

ThrottlingService::ThrottlingService()
  :mEnabled(kDefaultEnabled)
  ,mInitCalled(false)
  ,mSuspended(false)
  ,mPressureCount(0)
  ,mSuspendPeriod(kDefaultSuspendPeriod)
  ,mResumePeriod(kDefaultResumePeriod)
  ,mIteratingHash(false)
{
}

ThrottlingService::~ThrottlingService()
{
  Shutdown();
}


nsresult
ThrottlingService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInitCalled);

  mInitCalled = true;

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  NS_ENSURE_SUCCESS(rv, rv);

  mEnabled = Preferences::GetBool(kEnabledPref, kDefaultEnabled);
  rv = Preferences::AddStrongObserver(this, kEnabledPref);
  NS_ENSURE_SUCCESS(rv, rv);
  Preferences::AddUintVarCache(&mSuspendPeriod, kSuspendPeriodPref, kDefaultSuspendPeriod);
  Preferences::AddUintVarCache(&mResumePeriod, kResumePeriodPref, kDefaultResumePeriod);

  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);

  return NS_OK;
}

void
ThrottlingService::Shutdown()
{
  if (!mInitCalled) {
    return;
  }

  if (mTimer) {
    mTimer->Cancel();
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }

  Preferences::RemoveObserver(this, kEnabledPref);

  MaybeResumeAll();
  mChannelHash.Clear();
}

nsresult
ThrottlingService::Create(nsISupports *outer, const nsIID& iid, void **result)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (outer != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }

  RefPtr<ThrottlingService> svc = new ThrottlingService();
  if (!IsNeckoChild()) {
    // We only need to do any work on the parent, so only bother initializing
    // there. Child-side, we'll just error out since we only deal with parent
    // channels.)
    nsresult rv = svc->Init();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return svc->QueryInterface(iid, result);
}

// nsIThrottlingService

nsresult
ThrottlingService::AddChannel(nsIHttpChannel *channel)
{
  MOZ_ASSERT(NS_IsMainThread());

  // We don't check mEnabled, because we always want to put channels in the hash
  // to avoid potential inconsistencies in the case where the user changes the
  // enabled pref at run-time.

  if (IsNeckoChild()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  uint64_t key;
  nsresult rv = channel->GetChannelId(&key);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mChannelHash.Get(key, nullptr)) {
    // We already have this channel under our control, not adding it again.
    MOZ_ASSERT(false, "Trying to throttle an already-throttled channel");
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (!mIteratingHash) {
    // This should be the common case, and as such is easy to handle
    mChannelHash.Put(key, channel);

    if (mSuspended) {
      channel->Suspend();
    }
  } else {
    // This gets tricky - we've somehow re-entrantly gotten here through the
    // hash iteration in one of MaybeSuspendAll or MaybeResumeAll. Keep track
    // of the fact that this add came in now, and once we're done iterating, we
    // can add this into the hash. This avoids unexpectedly modifying the hash
    // while it's being iterated over, which could lead to inconsistencies.
    mChannelsToAddRemove.AppendElement(channel);
    mChannelIsAdd.AppendElement(true);
  }

  return NS_OK;
}

nsresult
ThrottlingService::RemoveChannel(nsIHttpChannel *channel)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Just like above, don't worry about mEnabled to avoid inconsistencies when
  // the pref changes at run-time

  if (IsNeckoChild()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  uint64_t key;
  nsresult rv = channel->GetChannelId(&key);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mChannelHash.Get(key, nullptr)) {
    // TODO - warn?
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (!mIteratingHash) {
    // This should be the common case, and easy to handle.
    mChannelHash.Remove(key);

    if (mSuspended) {
      // This channel is no longer under our control for suspend/resume, but
      // we've suspended it. Time to let it go.
      channel->Resume();
    }
  } else {
    // This gets tricky - we've somehow re-entrantly gotten here through the
    // hash iteration in one of MaybeSuspendAll or MaybeResumeAll. Keep track
    // of the fact that this add came in now, and once we're done iterating, we
    // can add this into the hash. This avoids unexpectedly modifying the hash
    // while it's being iterated over, which could lead to inconsistencies.
    mChannelsToAddRemove.AppendElement(channel);
    mChannelIsAdd.AppendElement(false);
  }

  return NS_OK;
}

nsresult
ThrottlingService::IncreasePressure()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Just like add/removing channels, we don't check mEnabled here in order to
  // avoid inconsistencies that could occur if the pref is flipped at runtime

  if (IsNeckoChild()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mPressureCount++ == 0) {
    MOZ_ASSERT(!mSuspended, "Suspended with 0 pressure?");
    MaybeSuspendAll();
    if (mSuspended) {
      // MaybeSuspendAll() may not actually suspend things, and we only want to
      // bother setting a timer to resume if we actually suspended.
      mTimer->InitWithCallback(this, mSuspendPeriod, nsITimer::TYPE_ONE_SHOT);
    }
  }

  return NS_OK;
}

nsresult
ThrottlingService::DecreasePressure()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Just like add/removing channels, we don't check mEnabled here in order to
  // avoid inconsistencies that could occur if the pref is flipped at runtime

  if (IsNeckoChild()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_ASSERT(mPressureCount > 0, "Unbalanced throttle pressure");

  if (--mPressureCount == 0) {
    MaybeResumeAll();
    mTimer->Cancel();
  }

  return NS_OK;
}

// nsIObserver

nsresult
ThrottlingService::Observe(nsISupports *subject, const char *topic,
                           const char16_t *data_unicode)
{
  MOZ_ASSERT(!IsNeckoChild());
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, topic)) {
    Shutdown();
  } else if (!strcmp("nsPref:changed", topic)) {
    mEnabled = Preferences::GetBool(kEnabledPref, mEnabled);
    if (mEnabled && mPressureCount) {
      // We weren't enabled, but we are now, AND we're under pressure. Go ahead
      // and suspend things.
      MaybeSuspendAll();
      if (mSuspended) {
        mTimer->InitWithCallback(this, mSuspendPeriod, nsITimer::TYPE_ONE_SHOT);
      }
    } else if (!mEnabled) {
      // We were enabled, but we aren't any longer. Make sure we aren't
      // suspending channels and that we don't have any timer that wants to
      // change things unexpectedly.
      mTimer->Cancel();
      MaybeResumeAll();
    }
  }

  return NS_OK;
}

// nsITimerCallback

nsresult
ThrottlingService::Notify(nsITimer *timer)
{
  MOZ_ASSERT(!IsNeckoChild());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(timer == mTimer);

  if (mSuspended) {
    MaybeResumeAll();
    // Always try to resume if we were suspended, but only time-limit the
    // resumption if we're under pressure and we're enabled. If either of those
    // conditions is false, it doesn't make any sense to set a timer to suspend
    // things when we don't want to be suspended anyway.
    if (mPressureCount && mEnabled) {
      mTimer->InitWithCallback(this, mResumePeriod, nsITimer::TYPE_ONE_SHOT);
    }
  } else if (mPressureCount) {
    MaybeSuspendAll();
    if (mSuspended) {
      // MaybeSuspendAll() may not actually suspend, and it only makes sense to
      // set a timer to resume if we actually suspended the channels.
      mTimer->InitWithCallback(this, mSuspendPeriod, nsITimer::TYPE_ONE_SHOT);
    }
  }

  return NS_OK;
}

// Internal methods

void
ThrottlingService::MaybeSuspendAll()
{
  MOZ_ASSERT(!IsNeckoChild());
  MOZ_ASSERT(NS_IsMainThread());

  if (!mEnabled) {
    // We don't actually suspend when disabled, even though it's possible we get
    // called in that state in order to avoid inconsistencies in the hash and
    // the count if the pref changes at runtime.
    return;
  }

  if (mSuspended) {
    // Already suspended, nothing to do!
    return;
  }
  mSuspended = true;

  IterateHash([](ChannelHash::Iterator &iter) -> void {
    const nsCOMPtr<nsIHttpChannel> channel = iter.UserData();
    channel->Suspend();
  });
}

void
ThrottlingService::MaybeResumeAll()
{
  MOZ_ASSERT(!IsNeckoChild());
  MOZ_ASSERT(NS_IsMainThread());

  if (!mSuspended) {
    // Already resumed, nothing to do!
    return;
  }
  mSuspended = false;

  IterateHash([](ChannelHash::Iterator &iter) -> void {
    const nsCOMPtr<nsIHttpChannel> channel = iter.UserData();
    channel->Resume();
  });
}

void
ThrottlingService::IterateHash(void (* callback)(ChannelHash::Iterator &iter))
{
  MOZ_ASSERT(!mIteratingHash);
  mIteratingHash = true;
  for (ChannelHash::Iterator iter = mChannelHash.ConstIter(); !iter.Done(); iter.Next()) {
    callback(iter);
  }
  mIteratingHash = false;
  HandleExtraAddRemove();
}

void
ThrottlingService::HandleExtraAddRemove()
{
  MOZ_ASSERT(!mIteratingHash);
  MOZ_ASSERT(mChannelsToAddRemove.Length() == mChannelIsAdd.Length());

  nsCOMArray<nsIHttpChannel> channelsToAddRemove;
  channelsToAddRemove.SwapElements(mChannelsToAddRemove);

  nsTArray<bool> channelIsAdd;
  channelIsAdd.SwapElements(mChannelIsAdd);

  for (size_t i = 0; i < channelsToAddRemove.Length(); ++i) {
    if (channelIsAdd[i]) {
      AddChannel(channelsToAddRemove[i]);
    } else {
      RemoveChannel(channelsToAddRemove[i]);
    }
  }

  channelsToAddRemove.Clear();
  channelIsAdd.Clear();
}

// The publicly available way to throttle things

Throttler::Throttler()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (IsNeckoChild()) {
    if (gNeckoChild) {
      // The child object may have already gone away, so we need to guard
      // guard against deref'ing a nullptr here. If that's what happened, then
      // our pageload won't be continuing anyway, so what we do is pretty much
      // irrelevant.
      gNeckoChild->SendIncreaseThrottlePressure();
    }
  } else {
    mThrottlingService = do_GetService("@mozilla.org/network/throttling-service;1");
    mThrottlingService->IncreasePressure();
  }
}

Throttler::~Throttler()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (IsNeckoChild()) {
    if (gNeckoChild) {
      // The child object may have already gone away, so we need to guard
      // guard against deref'ing a nullptr here. If that's what happened, then
      // NeckoParent::ActorDestroy will take care of releasing the pressure we
      // created.
      gNeckoChild->SendDecreaseThrottlePressure();
    }
  } else {
    MOZ_RELEASE_ASSERT(mThrottlingService);
    mThrottlingService->DecreasePressure();
    mThrottlingService = nullptr;
  }
}

}
}
