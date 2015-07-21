/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Maintains a circular buffer of recent messages, and notifies
 * listeners when new messages are logged.
 */

/* Threadsafe. */

#include "nsMemory.h"
#include "nsCOMArray.h"
#include "nsThreadUtils.h"

#include "nsConsoleService.h"
#include "nsConsoleMessage.h"
#include "nsIClassInfoImpl.h"
#include "nsIConsoleListener.h"
#include "nsPrintfCString.h"
#include "nsProxyRelease.h"
#include "nsIScriptError.h"

#include "mozilla/Preferences.h"

#if defined(ANDROID)
#include <android/log.h>
#include "mozilla/dom/ContentChild.h"
#endif
#ifdef XP_WIN
#include <windows.h>
#endif

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
using namespace mozilla::tasktracer;
#endif

using namespace mozilla;

NS_IMPL_ADDREF(nsConsoleService)
NS_IMPL_RELEASE(nsConsoleService)
NS_IMPL_CLASSINFO(nsConsoleService, nullptr,
                  nsIClassInfo::THREADSAFE | nsIClassInfo::SINGLETON,
                  NS_CONSOLESERVICE_CID)
NS_IMPL_QUERY_INTERFACE_CI(nsConsoleService, nsIConsoleService)
NS_IMPL_CI_INTERFACE_GETTER(nsConsoleService, nsIConsoleService)

static bool sLoggingEnabled = true;
static bool sLoggingBuffered = true;
#if defined(ANDROID)
static bool sLoggingLogcat = true;
#endif // defined(ANDROID)


nsConsoleService::nsConsoleService()
  : mMessages(nullptr)
  , mCurrent(0)
  , mFull(false)
  , mDeliveringMessage(false)
  , mLock("nsConsoleService.mLock")
{
  // XXX grab this from a pref!
  // hm, but worry about circularity, bc we want to be able to report
  // prefs errs...
  mBufferSize = 250;
}


NS_IMETHODIMP
nsConsoleService::ClearMessagesForWindowID(const uint64_t innerID)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // Remove the messages related to this window
  for (uint32_t i = 0; i < mBufferSize && mMessages[i]; i++) {
    // Only messages implementing nsIScriptError interface exposes the inner window ID
    nsCOMPtr<nsIScriptError> scriptError = do_QueryInterface(mMessages[i]);
    if (!scriptError) {
      continue;
    }
    uint64_t innerWindowID;
    nsresult rv = scriptError->GetInnerWindowID(&innerWindowID);
    if (NS_FAILED(rv) || innerWindowID != innerID) {
      continue;
    }

    // Free this matching message!
    NS_RELEASE(mMessages[i]);

    uint32_t j = i;
    // Now shift all the following messages
    for (; j < mBufferSize - 1 && mMessages[j + 1]; j++) {
      mMessages[j] = mMessages[j + 1];
    }
    // Nullify the current slot
    mMessages[j] = nullptr;
    mCurrent = j;

    // The array is no longer full
    mFull = false;

    // Ensure the next iteration handles the messages we just shifted down
    i--;
  }

  return NS_OK;
}

nsConsoleService::~nsConsoleService()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  uint32_t i = 0;
  while (i < mBufferSize && mMessages[i]) {
    NS_RELEASE(mMessages[i]);
    i++;
  }

  if (mMessages) {
    free(mMessages);
  }
}

class AddConsolePrefWatchers : public nsRunnable
{
public:
  explicit AddConsolePrefWatchers(nsConsoleService* aConsole) : mConsole(aConsole)
  {
  }

  NS_IMETHOD Run()
  {
    Preferences::AddBoolVarCache(&sLoggingEnabled, "consoleservice.enabled", true);
    Preferences::AddBoolVarCache(&sLoggingBuffered, "consoleservice.buffered", true);
#if defined(ANDROID)
    Preferences::AddBoolVarCache(&sLoggingLogcat, "consoleservice.logcat", true);
#endif // defined(ANDROID)
    if (!sLoggingBuffered) {
      mConsole->Reset();
    }
    return NS_OK;
  }

private:
  nsRefPtr<nsConsoleService> mConsole;
};

nsresult
nsConsoleService::Init()
{
  mMessages = (nsIConsoleMessage**)
    moz_xmalloc(mBufferSize * sizeof(nsIConsoleMessage*));
  if (!mMessages) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Array elements should be 0 initially for circular buffer algorithm.
  memset(mMessages, 0, mBufferSize * sizeof(nsIConsoleMessage*));

  NS_DispatchToMainThread(new AddConsolePrefWatchers(this));

  return NS_OK;
}

namespace {

class LogMessageRunnable : public nsRunnable
{
public:
  LogMessageRunnable(nsIConsoleMessage* aMessage, nsConsoleService* aService)
    : mMessage(aMessage)
    , mService(aService)
  { }

  NS_DECL_NSIRUNNABLE

private:
  nsCOMPtr<nsIConsoleMessage> mMessage;
  nsRefPtr<nsConsoleService> mService;
};

NS_IMETHODIMP
LogMessageRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Snapshot of listeners so that we don't reenter this hash during
  // enumeration.
  nsCOMArray<nsIConsoleListener> listeners;
  mService->CollectCurrentListeners(listeners);

  mService->SetIsDelivering();

  for (int32_t i = 0; i < listeners.Count(); ++i) {
    listeners[i]->Observe(mMessage);
  }

  mService->SetDoneDelivering();

  return NS_OK;
}

} // namespace

// nsIConsoleService methods
NS_IMETHODIMP
nsConsoleService::LogMessage(nsIConsoleMessage* aMessage)
{
  return LogMessageWithMode(aMessage, OutputToLog);
}

// This can be called off the main thread.
nsresult
nsConsoleService::LogMessageWithMode(nsIConsoleMessage* aMessage,
                                     nsConsoleService::OutputMode aOutputMode)
{
  if (!aMessage) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!sLoggingEnabled) {
    return NS_OK;
  }

  if (NS_IsMainThread() && mDeliveringMessage) {
    nsCString msg;
    aMessage->ToString(msg);
    NS_WARNING(nsPrintfCString("Reentrancy error: some client attempted "
      "to display a message to the console while in a console listener. "
      "The following message was discarded: \"%s\"", msg.get()).get());
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<LogMessageRunnable> r;
  nsIConsoleMessage* retiredMessage;

  if (sLoggingBuffered) {
    NS_ADDREF(aMessage); // early, in case it's same as replaced below.
  }

  /*
   * Lock while updating buffer, and while taking snapshot of
   * listeners array.
   */
  {
    MutexAutoLock lock(mLock);

#if defined(ANDROID)
    if (sLoggingLogcat && aOutputMode == OutputToLog) {
      nsCString msg;
      aMessage->ToString(msg);

      /** Attempt to use the process name as the log tag. */
      mozilla::dom::ContentChild* child =
          mozilla::dom::ContentChild::GetSingleton();
      nsCString appName;
      if (child) {
        child->GetProcessName(appName);
      } else {
        appName = "GeckoConsole";
      }

      uint32_t logLevel = 0;
      aMessage->GetLogLevel(&logLevel);

      android_LogPriority logPriority = ANDROID_LOG_INFO;
      switch (logLevel) {
        case nsIConsoleMessage::debug:
          logPriority = ANDROID_LOG_DEBUG;
          break;
        case nsIConsoleMessage::info:
          logPriority = ANDROID_LOG_INFO;
          break;
        case nsIConsoleMessage::warn:
          logPriority = ANDROID_LOG_WARN;
          break;
        case nsIConsoleMessage::error:
          logPriority = ANDROID_LOG_ERROR;
          break;
      }

      __android_log_print(logPriority, appName.get(), "%s", msg.get());
    }
#endif
#ifdef XP_WIN
    if (IsDebuggerPresent()) {
      nsString msg;
      aMessage->GetMessageMoz(getter_Copies(msg));
      msg.Append('\n');
      OutputDebugStringW(msg.get());
    }
#endif
#ifdef MOZ_TASK_TRACER
    {
      nsCString msg;
      aMessage->ToString(msg);
      int prefixPos = msg.Find(GetJSLabelPrefix());
      if (prefixPos >= 0) {
        nsDependentCSubstring submsg(msg, prefixPos);
        AddLabel("%s", submsg.BeginReading());
      }
    }
#endif

    /*
     * If there's already a message in the slot we're about to replace,
     * we've wrapped around, and we need to release the old message.  We
     * save a pointer to it, so we can release below outside the lock.
     */
    retiredMessage = mMessages[mCurrent];

    if (sLoggingBuffered) {
      mMessages[mCurrent++] = aMessage;
      if (mCurrent == mBufferSize) {
        mCurrent = 0; // wrap around.
        mFull = true;
      }
    }

    if (mListeners.Count() > 0) {
      r = new LogMessageRunnable(aMessage, this);
    }
  }

  if (retiredMessage) {
    // Release |retiredMessage| on the main thread in case it is an instance of
    // a mainthread-only class like nsScriptErrorWithStack and we're off the
    // main thread.
    NS_ReleaseOnMainThread(retiredMessage);
  }

  if (r) {
    // avoid failing in XPCShell tests
    nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
    if (mainThread) {
      NS_DispatchToMainThread(r.forget());
    }
  }

  return NS_OK;
}

void
nsConsoleService::CollectCurrentListeners(
  nsCOMArray<nsIConsoleListener>& aListeners)
{
  MutexAutoLock lock(mLock);
  for (auto iter = mListeners.Iter(); !iter.Done(); iter.Next()) {
    nsIConsoleListener* value = iter.UserData();
    aListeners.AppendObject(value);
  }
}

NS_IMETHODIMP
nsConsoleService::LogStringMessage(const char16_t* aMessage)
{
  if (!sLoggingEnabled) {
    return NS_OK;
  }

  nsRefPtr<nsConsoleMessage> msg(new nsConsoleMessage(aMessage));
  return this->LogMessage(msg);
}

NS_IMETHODIMP
nsConsoleService::GetMessageArray(uint32_t* aCount,
                                  nsIConsoleMessage*** aMessages)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  nsIConsoleMessage** messageArray;

  /*
   * Lock the whole method, as we don't want anyone mucking with mCurrent or
   * mFull while we're copying out the buffer.
   */
  MutexAutoLock lock(mLock);

  if (mCurrent == 0 && !mFull) {
    /*
     * Make a 1-length output array so that nobody gets confused,
     * and return a count of 0.  This should result in a 0-length
     * array object when called from script.
     */
    messageArray = (nsIConsoleMessage**)
      moz_xmalloc(sizeof(nsIConsoleMessage*));
    *messageArray = nullptr;
    *aMessages = messageArray;
    *aCount = 0;

    return NS_OK;
  }

  uint32_t resultSize = mFull ? mBufferSize : mCurrent;
  messageArray =
    (nsIConsoleMessage**)moz_xmalloc((sizeof(nsIConsoleMessage*))
                                         * resultSize);

  if (!messageArray) {
    *aMessages = nullptr;
    *aCount = 0;
    return NS_ERROR_FAILURE;
  }

  uint32_t i;
  if (mFull) {
    for (i = 0; i < mBufferSize; i++) {
      // if full, fill the buffer starting from mCurrent (which'll be
      // oldest) wrapping around the buffer to the most recent.
      messageArray[i] = mMessages[(mCurrent + i) % mBufferSize];
      NS_ADDREF(messageArray[i]);
    }
  } else {
    for (i = 0; i < mCurrent; i++) {
      messageArray[i] = mMessages[i];
      NS_ADDREF(messageArray[i]);
    }
  }
  *aCount = resultSize;
  *aMessages = messageArray;

  return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::RegisterListener(nsIConsoleListener* aListener)
{
  if (!NS_IsMainThread()) {
    NS_ERROR("nsConsoleService::RegisterListener is main thread only.");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsCOMPtr<nsISupports> canonical = do_QueryInterface(aListener);

  MutexAutoLock lock(mLock);
  if (mListeners.GetWeak(canonical)) {
    // Reregistering a listener isn't good
    return NS_ERROR_FAILURE;
  }
  mListeners.Put(canonical, aListener);
  return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::UnregisterListener(nsIConsoleListener* aListener)
{
  if (!NS_IsMainThread()) {
    NS_ERROR("nsConsoleService::UnregisterListener is main thread only.");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsCOMPtr<nsISupports> canonical = do_QueryInterface(aListener);

  MutexAutoLock lock(mLock);

  if (!mListeners.GetWeak(canonical)) {
    // Unregistering a listener that was never registered?
    return NS_ERROR_FAILURE;
  }
  mListeners.Remove(canonical);
  return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::Reset()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  /*
   * Make sure nobody trips into the buffer while it's being reset
   */
  MutexAutoLock lock(mLock);

  mCurrent = 0;
  mFull = false;

  /*
   * Free all messages stored so far (cf. destructor)
   */
  for (uint32_t i = 0; i < mBufferSize && mMessages[i]; i++) {
    NS_RELEASE(mMessages[i]);
  }

  return NS_OK;
}
