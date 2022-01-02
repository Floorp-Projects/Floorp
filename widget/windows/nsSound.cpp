/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "plstr.h"
#include <stdio.h>
#include "nsString.h"
#include <windows.h>

// mmsystem.h is needed to build with WIN32_LEAN_AND_MEAN
#include <mmsystem.h>

#include "HeadlessSound.h"
#include "nsSound.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsContentUtils.h"
#include "nsCRT.h"
#include "nsIObserverService.h"

#include "mozilla/Logging.h"
#include "prtime.h"

#include "nsNativeCharsetUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "gfxPlatform.h"

using mozilla::LogLevel;

static mozilla::LazyLogModule gWin32SoundLog("nsSound");

// Hackaround for bug 1644240
// When we call PlaySound for the first time in the process, winmm.dll creates
// a new thread and starts a message loop in winmm!mciwindow.  After that,
// every call of PlaySound communicates with that thread via Window messages.
// It seems that Warsaw application hooks USER32!GetMessageA, and there is
// a timing window where they free their trampoline region without reverting
// the hook on USER32!GetMessageA, resulting in crash when winmm!mciwindow
// receives a message because it tries to jump to a freed buffer.
// Based on the crash reports, it happened on all versions of Windows x64, and
// the possible condition was wslbdhm64.dll was loaded but wslbscrwh64.dll was
// unloaded.  Therefore we suppress playing a sound under such a condition.
static bool ShouldSuppressPlaySound() {
#if defined(_M_AMD64)
  if (::GetModuleHandle(L"wslbdhm64.dll") &&
      !::GetModuleHandle(L"wslbscrwh64.dll")) {
    return true;
  }
#endif  // defined(_M_AMD64)
  return false;
}

class nsSoundPlayer : public mozilla::Runnable {
 public:
  explicit nsSoundPlayer(const nsAString& aSoundName)
      : mozilla::Runnable("nsSoundPlayer"),
        mSoundName(aSoundName),
        mSoundData(nullptr) {}

  nsSoundPlayer(const uint8_t* aData, size_t aSize)
      : mozilla::Runnable("nsSoundPlayer"), mSoundName(u""_ns) {
    MOZ_ASSERT(aSize > 0, "Size should not be zero");
    MOZ_ASSERT(aData, "Data shoud not be null");

    // We will disptach nsSoundPlayer to playerthread, so keep a data copy
    mSoundData = new uint8_t[aSize];
    memcpy(mSoundData, aData, aSize);
  }

  NS_DECL_NSIRUNNABLE

 protected:
  ~nsSoundPlayer();

  nsString mSoundName;
  uint8_t* mSoundData;
};

NS_IMETHODIMP
nsSoundPlayer::Run() {
  if (ShouldSuppressPlaySound()) {
    return NS_OK;
  }

  MOZ_ASSERT(!mSoundName.IsEmpty() || mSoundData,
             "Sound name or sound data should be specified");
  DWORD flags = SND_NODEFAULT | SND_ASYNC;

  if (mSoundData) {
    flags |= SND_MEMORY;
    ::PlaySoundW(reinterpret_cast<LPCWSTR>(mSoundData), nullptr, flags);
  } else {
    flags |= SND_ALIAS;
    ::PlaySoundW(mSoundName.get(), nullptr, flags);
  }
  return NS_OK;
}

nsSoundPlayer::~nsSoundPlayer() { delete[] mSoundData; }

mozilla::StaticRefPtr<nsISound> nsSound::sInstance;

/* static */
already_AddRefed<nsISound> nsSound::GetInstance() {
  if (!sInstance) {
    if (gfxPlatform::IsHeadless()) {
      sInstance = new mozilla::widget::HeadlessSound();
    } else {
      RefPtr<nsSound> sound = new nsSound();
      nsresult rv = sound->CreatePlayerThread();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return nullptr;
      }
      sInstance = sound.forget();
    }
    ClearOnShutdown(&sInstance);
  }

  RefPtr<nsISound> service = sInstance;
  return service.forget();
}

#ifndef SND_PURGE
// Not available on Windows CE, and according to MSDN
// doesn't do anything on recent windows either.
#  define SND_PURGE 0
#endif

NS_IMPL_ISUPPORTS(nsSound, nsISound, nsIStreamLoaderObserver, nsIObserver)

nsSound::nsSound() : mInited(false) {}

nsSound::~nsSound() {}

void nsSound::PurgeLastSound() {
  // Halt any currently playing sound.
  if (mSoundPlayer) {
    if (mPlayerThread) {
      mPlayerThread->Dispatch(
          NS_NewRunnableFunction("nsSound::PurgeLastSound",
                                 [player = std::move(mSoundPlayer)]() {
                                   // Capture move mSoundPlayer to lambda then
                                   // PlaySoundW(nullptr, nullptr, SND_PURGE)
                                   // will be called before freeing the
                                   // nsSoundPlayer.
                                   if (ShouldSuppressPlaySound()) {
                                     return;
                                   }
                                   ::PlaySoundW(nullptr, nullptr, SND_PURGE);
                                 }),
          NS_DISPATCH_NORMAL);
    }
  }
}

NS_IMETHODIMP nsSound::Beep() {
  ::MessageBeep(0);

  return NS_OK;
}

NS_IMETHODIMP nsSound::OnStreamComplete(nsIStreamLoader* aLoader,
                                        nsISupports* context, nsresult aStatus,
                                        uint32_t dataLen, const uint8_t* data) {
  MOZ_ASSERT(mPlayerThread, "player thread should not be null ");
  // print a load error on bad status
  if (NS_FAILED(aStatus)) {
#ifdef DEBUG
    if (aLoader) {
      nsCOMPtr<nsIRequest> request;
      nsCOMPtr<nsIChannel> channel;
      aLoader->GetRequest(getter_AddRefs(request));
      if (request) channel = do_QueryInterface(request);
      if (channel) {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        if (uri) {
          nsAutoCString uriSpec;
          uri->GetSpec(uriSpec);
          MOZ_LOG(gWin32SoundLog, LogLevel::Info,
                  ("Failed to load %s\n", uriSpec.get()));
        }
      }
    }
#endif
    return aStatus;
  }

  PurgeLastSound();

  if (data && dataLen > 0) {
    MOZ_ASSERT(!mSoundPlayer, "mSoundPlayer should be null");
    mSoundPlayer = new nsSoundPlayer(data, dataLen);
    MOZ_ASSERT(mSoundPlayer, "Could not create player");

    nsresult rv = mPlayerThread->Dispatch(mSoundPlayer, NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsSound::Play(nsIURL* aURL) {
  nsresult rv;

#ifdef DEBUG_SOUND
  char* url;
  aURL->GetSpec(&url);
  MOZ_LOG(gWin32SoundLog, LogLevel::Info, ("%s\n", url));
#endif

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(
      getter_AddRefs(loader), aURL,
      this,  // aObserver
      nsContentUtils::GetSystemPrincipal(),
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      nsIContentPolicy::TYPE_OTHER);
  return rv;
}

nsresult nsSound::CreatePlayerThread() {
  if (mPlayerThread) {
    return NS_OK;
  }
  if (NS_WARN_IF(NS_FAILED(NS_NewNamedThread("PlayEventSound",
                                             getter_AddRefs(mPlayerThread))))) {
    return NS_ERROR_FAILURE;
  }

  // Add an observer for shutdown event to release the thread at that time
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (!observerService) {
    return NS_ERROR_FAILURE;
  }

  observerService->AddObserver(this, "xpcom-shutdown-threads", false);
  return NS_OK;
}

NS_IMETHODIMP
nsSound::Observe(nsISupports* aSubject, const char* aTopic,
                 const char16_t* aData) {
  if (!strcmp(aTopic, "xpcom-shutdown-threads")) {
    PurgeLastSound();

    if (mPlayerThread) {
      mPlayerThread->Shutdown();
      mPlayerThread = nullptr;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsSound::Init() {
  if (mInited) {
    return NS_OK;
  }

  MOZ_ASSERT(mPlayerThread, "player thread should not be null ");
  // This call halts a sound if it was still playing.
  // We have to use the sound library for something to make sure
  // it is initialized.
  // If we wait until the first sound is played, there will
  // be a time lag as the library gets loaded.
  // This should be done in player thread otherwise it will block main thread
  // at the first time loading sound library.
  mPlayerThread->Dispatch(
      NS_NewRunnableFunction("nsSound::Init",
                             []() {
                               if (ShouldSuppressPlaySound()) {
                                 return;
                               }
                               ::PlaySoundW(nullptr, nullptr, SND_PURGE);
                             }),
      NS_DISPATCH_NORMAL);

  mInited = true;

  return NS_OK;
}

NS_IMETHODIMP nsSound::PlayEventSound(uint32_t aEventId) {
  MOZ_ASSERT(mPlayerThread, "player thread should not be null ");
  PurgeLastSound();

  const wchar_t* sound = nullptr;
  switch (aEventId) {
    case EVENT_NEW_MAIL_RECEIVED:
      sound = L"MailBeep";
      break;
    case EVENT_ALERT_DIALOG_OPEN:
      sound = L"SystemExclamation";
      break;
    case EVENT_CONFIRM_DIALOG_OPEN:
      sound = L"SystemQuestion";
      break;
    case EVENT_MENU_EXECUTE:
      sound = L"MenuCommand";
      break;
    case EVENT_MENU_POPUP:
      sound = L"MenuPopup";
      break;
    case EVENT_EDITOR_MAX_LEN:
      sound = L".Default";
      break;
    default:
      // Win32 plays no sounds at NS_SYSSOUND_PROMPT_DIALOG and
      // NS_SYSSOUND_SELECT_DIALOG.
      return NS_OK;
  }
  NS_ASSERTION(sound, "sound is null");
  MOZ_ASSERT(!mSoundPlayer, "mSoundPlayer should be null");
  mSoundPlayer = new nsSoundPlayer(nsDependentString(sound));
  MOZ_ASSERT(mSoundPlayer, "Could not create player");
  nsresult rv = mPlayerThread->Dispatch(mSoundPlayer, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}
