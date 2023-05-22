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

#include "nsCOMArray.h"
#include "nsThreadUtils.h"

#include "nsConsoleService.h"
#include "nsConsoleMessage.h"
#include "nsIClassInfoImpl.h"
#include "nsIConsoleListener.h"
#include "nsIObserverService.h"
#include "nsPrintfCString.h"
#include "nsProxyRelease.h"
#include "nsIScriptError.h"
#include "nsISupportsPrimitives.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/BrowserParent.h"

#include "mozilla/SchedulerGroup.h"
#include "mozilla/Services.h"

#if defined(ANDROID)
#  include <android/log.h>
#  include "mozilla/dom/ContentChild.h"
#  include "mozilla/StaticPrefs_consoleservice.h"
#endif
#ifdef XP_WIN
#  include <windows.h>
#endif

using namespace mozilla;

NS_IMPL_ADDREF(nsConsoleService)
NS_IMPL_RELEASE(nsConsoleService)
NS_IMPL_CLASSINFO(nsConsoleService, nullptr,
                  nsIClassInfo::THREADSAFE | nsIClassInfo::SINGLETON,
                  NS_CONSOLESERVICE_CID)
NS_IMPL_QUERY_INTERFACE_CI(nsConsoleService, nsIConsoleService, nsIObserver)
NS_IMPL_CI_INTERFACE_GETTER(nsConsoleService, nsIConsoleService, nsIObserver)

static const bool gLoggingEnabled = true;
static const bool gLoggingBuffered = true;
#ifdef XP_WIN
static bool gLoggingToDebugger = true;
#endif  // XP_WIN

nsConsoleService::MessageElement::~MessageElement() = default;

nsConsoleService::nsConsoleService()
    : mCurrentSize(0),
      // XXX grab this from a pref!
      // hm, but worry about circularity, bc we want to be able to report
      // prefs errs...
      mMaximumSize(250),
      mDeliveringMessage(false),
      mLock("nsConsoleService.mLock") {
#ifdef XP_WIN
  // This environment variable controls whether the console service
  // should be prevented from putting output to the attached debugger.
  // It only affects the Windows platform.
  //
  // To disable OutputDebugString, set:
  //   MOZ_CONSOLESERVICE_DISABLE_DEBUGGER_OUTPUT=1
  //
  const char* disableDebugLoggingVar =
      getenv("MOZ_CONSOLESERVICE_DISABLE_DEBUGGER_OUTPUT");
  gLoggingToDebugger =
      !disableDebugLoggingVar || (disableDebugLoggingVar[0] == '0');
#endif  // XP_WIN
}

void nsConsoleService::ClearMessagesForWindowID(const uint64_t innerID) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mLock);

  for (MessageElement* e = mMessages.getFirst(); e != nullptr;) {
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

void nsConsoleService::ClearMessages() {
  // NB: A lock is not required here as it's only called from |Reset| which
  //     locks for us and from the dtor.
  while (!mMessages.isEmpty()) {
    MessageElement* e = mMessages.popFirst();
    delete e;
  }
  mCurrentSize = 0;
}

nsConsoleService::~nsConsoleService() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  ClearMessages();
}

class AddConsolePrefWatchers : public Runnable {
 public:
  explicit AddConsolePrefWatchers(nsConsoleService* aConsole)
      : mozilla::Runnable("AddConsolePrefWatchers"), mConsole(aConsole) {}

  NS_IMETHOD Run() override {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    MOZ_ASSERT(obs);
    obs->AddObserver(mConsole, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
    obs->AddObserver(mConsole, "inner-window-destroyed", false);

    if (!gLoggingBuffered) {
      mConsole->Reset();
    }
    return NS_OK;
  }

 private:
  RefPtr<nsConsoleService> mConsole;
};

nsresult nsConsoleService::Init() {
  NS_DispatchToMainThread(new AddConsolePrefWatchers(this));

  return NS_OK;
}

nsresult nsConsoleService::MaybeForwardScriptError(nsIConsoleMessage* aMessage,
                                                   bool* sent) {
  *sent = false;

  nsCOMPtr<nsIScriptError> scriptError = do_QueryInterface(aMessage);
  if (!scriptError) {
    // Not an nsIScriptError
    return NS_OK;
  }

  uint64_t windowID;
  nsresult rv;
  rv = scriptError->GetInnerWindowID(&windowID);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!windowID) {
    // Does not set window id
    return NS_OK;
  }

  RefPtr<mozilla::dom::WindowGlobalParent> windowGlobalParent =
      mozilla::dom::WindowGlobalParent::GetByInnerWindowId(windowID);
  if (!windowGlobalParent) {
    // Could not find parent window by id
    return NS_OK;
  }

  RefPtr<mozilla::dom::BrowserParent> browserParent =
      windowGlobalParent->GetBrowserParent();
  if (!browserParent) {
    return NS_OK;
  }

  mozilla::dom::ContentParent* contentParent = browserParent->Manager();
  if (!contentParent) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString msg, sourceName, sourceLine;
  nsCString category;
  uint32_t lineNum, colNum, flags;
  uint64_t innerWindowId;
  bool fromPrivateWindow, fromChromeContext;

  rv = scriptError->GetErrorMessage(msg);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = scriptError->GetSourceName(sourceName);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = scriptError->GetSourceLine(sourceLine);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = scriptError->GetCategory(getter_Copies(category));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = scriptError->GetLineNumber(&lineNum);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = scriptError->GetColumnNumber(&colNum);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = scriptError->GetFlags(&flags);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = scriptError->GetIsFromPrivateWindow(&fromPrivateWindow);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = scriptError->GetIsFromChromeContext(&fromChromeContext);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = scriptError->GetInnerWindowID(&innerWindowId);
  NS_ENSURE_SUCCESS(rv, rv);

  *sent = contentParent->SendScriptError(
      msg, sourceName, sourceLine, lineNum, colNum, flags, category,
      fromPrivateWindow, innerWindowId, fromChromeContext);
  return NS_OK;
}

namespace {

class LogMessageRunnable : public Runnable {
 public:
  LogMessageRunnable(nsIConsoleMessage* aMessage, nsConsoleService* aService)
      : mozilla::Runnable("LogMessageRunnable"),
        mMessage(aMessage),
        mService(aService) {}

  NS_DECL_NSIRUNNABLE

 private:
  nsCOMPtr<nsIConsoleMessage> mMessage;
  RefPtr<nsConsoleService> mService;
};

NS_IMETHODIMP
LogMessageRunnable::Run() {
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

}  // namespace

// nsIConsoleService methods
NS_IMETHODIMP
nsConsoleService::LogMessage(nsIConsoleMessage* aMessage) {
  return LogMessageWithMode(aMessage, nsIConsoleService::OutputToLog);
}

// This can be called off the main thread.
nsresult nsConsoleService::LogMessageWithMode(
    nsIConsoleMessage* aMessage, nsIConsoleService::OutputMode aOutputMode) {
  if (!aMessage) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!gLoggingEnabled) {
    return NS_OK;
  }

  if (NS_IsMainThread() && mDeliveringMessage) {
    nsCString msg;
    aMessage->ToString(msg);
    NS_WARNING(
        nsPrintfCString(
            "Reentrancy error: some client attempted to display a message to "
            "the console while in a console listener. The following message "
            "was discarded: \"%s\"",
            msg.get())
            .get());
    return NS_ERROR_FAILURE;
  }

  if (XRE_IsParentProcess() && NS_IsMainThread()) {
    // If mMessage is a scriptError with an innerWindowId set,
    // forward it to the matching ContentParent
    // This enables logging from parent to content process
    bool sent;
    nsresult rv = MaybeForwardScriptError(aMessage, &sent);
    NS_ENSURE_SUCCESS(rv, rv);
    if (sent) {
      return NS_OK;
    }
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
    if (StaticPrefs::consoleservice_logcat() && aOutputMode == OutputToLog) {
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
    if (gLoggingToDebugger && IsDebuggerPresent()) {
      nsString msg;
      aMessage->GetMessageMoz(msg);
      msg.Append('\n');
      OutputDebugStringW(msg.get());
    }
#endif

    if (gLoggingBuffered) {
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
    NS_ReleaseOnMainThread("nsConsoleService::retiredMessage",
                           retiredMessage.forget());
  }

  if (r) {
    // avoid failing in XPCShell tests
    nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
    if (mainThread) {
      SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
    }
  }

  return NS_OK;
}

void nsConsoleService::CollectCurrentListeners(
    nsCOMArray<nsIConsoleListener>& aListeners) {
  MutexAutoLock lock(mLock);
  // XXX When MakeBackInserter(nsCOMArray<T>&) is added, we can do:
  // AppendToArray(aListeners, mListeners.Values());
  for (const auto& listener : mListeners.Values()) {
    aListeners.AppendObject(listener);
  }
}

NS_IMETHODIMP
nsConsoleService::LogStringMessage(const char16_t* aMessage) {
  if (!gLoggingEnabled) {
    return NS_OK;
  }

  RefPtr<nsConsoleMessage> msg(new nsConsoleMessage(
      aMessage ? nsDependentString(aMessage) : EmptyString()));
  return LogMessage(msg);
}

NS_IMETHODIMP
nsConsoleService::GetMessageArray(
    nsTArray<RefPtr<nsIConsoleMessage>>& aMessages) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MutexAutoLock lock(mLock);

  if (mMessages.isEmpty()) {
    return NS_OK;
  }

  MOZ_ASSERT(mCurrentSize <= mMaximumSize);
  aMessages.SetCapacity(mCurrentSize);

  for (MessageElement* e = mMessages.getFirst(); e != nullptr;
       e = e->getNext()) {
    aMessages.AppendElement(e->Get());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::RegisterListener(nsIConsoleListener* aListener) {
  if (!NS_IsMainThread()) {
    NS_ERROR("nsConsoleService::RegisterListener is main thread only.");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsCOMPtr<nsISupports> canonical = do_QueryInterface(aListener);
  MOZ_ASSERT(canonical);

  MutexAutoLock lock(mLock);
  return mListeners.WithEntryHandle(canonical, [&](auto&& entry) {
    if (entry) {
      // Reregistering a listener isn't good
      return NS_ERROR_FAILURE;
    }
    entry.Insert(aListener);
    return NS_OK;
  });
}

NS_IMETHODIMP
nsConsoleService::UnregisterListener(nsIConsoleListener* aListener) {
  if (!NS_IsMainThread()) {
    NS_ERROR("nsConsoleService::UnregisterListener is main thread only.");
    return NS_ERROR_NOT_SAME_THREAD;
  }

  nsCOMPtr<nsISupports> canonical = do_QueryInterface(aListener);

  MutexAutoLock lock(mLock);

  return mListeners.Remove(canonical)
             ? NS_OK
             // Unregistering a listener that was never registered?
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsConsoleService::Reset() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  /*
   * Make sure nobody trips into the buffer while it's being reset
   */
  MutexAutoLock lock(mLock);

  ClearMessages();
  return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::ResetWindow(uint64_t windowInnerId) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  ClearMessagesForWindowID(windowInnerId);
  return NS_OK;
}

NS_IMETHODIMP
nsConsoleService::Observe(nsISupports* aSubject, const char* aTopic,
                          const char16_t* aData) {
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    // Dump all our messages, in case any are cycle collected.
    Reset();
    // We could remove ourselves from the observer service, but it is about to
    // drop all observers anyways, so why bother.
  } else if (!strcmp(aTopic, "inner-window-destroyed")) {
    nsCOMPtr<nsISupportsPRUint64> supportsInt = do_QueryInterface(aSubject);
    MOZ_ASSERT(supportsInt);

    uint64_t windowId;
    MOZ_ALWAYS_SUCCEEDS(supportsInt->GetData(&windowId));

    ClearMessagesForWindowID(windowId);
  } else {
    MOZ_CRASH();
  }
  return NS_OK;
}
