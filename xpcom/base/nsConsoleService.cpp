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
#include "nsISupportsPrimitives.h"

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
NS_IMPL_QUERY_INTERFACE_CI(nsConsoleService, nsIConsoleService, nsIObserver)
NS_IMPL_CI_INTERFACE_GETTER(nsConsoleService, nsIConsoleService, nsIObserver)

static bool sLoggingEnabled = true;
static bool sLoggingBuffered = true;
#if defined(ANDROID)
static bool sLoggingLogcat = true;
#endif // defined(ANDROID)

nsConsoleService::MessageElement::~MessageElement()
{
}

nsConsoleService::nsConsoleService()
  : mCurrentSize(0)
  , mDeliveringMessage(false)
  , mLock("nsConsoleService.mLock")
{
  // XXX grab this from a pref!
  // hm, but worry about circularity, bc we want to be able to report
  // prefs errs...
  mMaximumSize = 250;
}


void
nsConsoleService::ClearMessagesForWindowID(const uint64_t innerID)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  for (MessageElement* e = mMessages.getFirst(); e != nullptr; ) {
    // Only messages implementing nsIScriptError interface expose the
    // inner window ID.
    nsCOMPtr<nsIScriptError> scriptError = do_QueryInterface(e->Get());
    if (!scriptError) {
      e = e->getNext();
      continue;
    }
    uint64_t innerWindowID;
    nsresult rv = scriptError->GetInnerWindowID(&innerWindowID);
    if (NS_FAILED(rv) || innerWindowID != innerID) {
      e = e->getNext();
      continue;
    }

    MessageElement* next = e->getNext();
    e->remove();
    delete e;
    mCurrentSize--;
    MOZ_ASSERT(mCurrentSize < mMaximumSize);

    e = next;
  }
}

void
nsConsoleService::ClearMessages()
{
  while (!mMessages.isEmpty()) {
    MessageElement* e = mMessages.popFirst();
    delete e;
  }
  mCurrentSize = 0;
}

nsConsoleService::~nsConsoleService()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  ClearMessages();
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

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    MOZ_ASSERT(obs);
    obs->AddObserver(mConsole, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
    obs->AddObserver(mConsole, "inner-window-destroyed", false);

    if (!sLoggingBuffered) {
      mConsole->Reset();
    }
    return NS_OK;
  }

private:
  RefPtr<nsConsoleService> mConsole;
};

nsresult
nsConsoleService::Init()
{
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
  RefPtr<nsConsoleService> mService;
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

  RefPtr<LogMessageRunnable> r;
  nsCOMPtr<nsIConsoleMessage> retiredMessage;

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

    if (sLoggingBuffered) {
      MessageElement* e = new MessageElement(aMessage);
      mMessages.insertBack(e);
      if (mCurrentSize != mMaximumSize) {
        mCurrentSize++;
      } else {
        MessageElement* p = mMessages.popFirst();
        MOZ_ASSERT(p);
        p->swapMessage(retiredMessage);
        delete p;
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

  RefPtr<nsConsoleMessage> msg(new nsConsoleMessage(aMessage));
  return this->LogMessage(msg);
}

NS_IMETHODIMP
nsConsoleService::GetMessageArray(uint32_t* aCount,
                                  nsIConsoleMessage*** aMessages)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mLock);

  if (mMessages.isEmpty()) {
    /*
     * Make a 1-length output array so that nobody gets confused,
     * and return a count of 0.  This should result in a 0-length
     * array object when called from script.
     */
    nsIConsoleMessage** messageArray = (nsIConsoleMessage**)
      moz_xmalloc(sizeof(nsIConsoleMessage*));
    *messageArray = nullptr;
    *aMessages = messageArray;
    *aCount = 0;

    return NS_OK;
  }

  MOZ_ASSERT(mCurrentSize <= mMaximumSize);
  nsIConsoleMessage** messageArray =
    static_cast<nsIConsoleMessage**>(moz_xmalloc(sizeof(nsIConsoleMessage*)
                                                 * mCurrentSize));

  uint32_t i = 0;
  for (MessageElement* e = mMessages.getFirst(); e != nullptr; e = e->getNext()) {
    nsCOMPtr<nsIConsoleMessage> m = e->Get();
    m.forget(&messageArray[i]);
    i++;
  }

  MOZ_ASSERT(i == mCurrentSize);

  *aCount = i;
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

  ClearMessages();
  return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::Observe(nsISupports* aSubject, const char* aTopic,
                          const char16_t* aData)
{
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    // Dump all our messages, in case any are cycle collected.
    Reset();
    // We could remove ourselves from the observer service, but it is about to
    // drop all observers anyways, so why bother.
  } else if (!strcmp(aTopic, "inner-window-destroyed")) {
    nsCOMPtr<nsISupportsPRUint64> supportsInt = do_QueryInterface(aSubject);
    MOZ_ASSERT(supportsInt);

    uint64_t windowId;
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(supportsInt->GetData(&windowId)));

    ClearMessagesForWindowID(windowId);
  } else {
    MOZ_CRASH();
  }
  return NS_OK;
}
