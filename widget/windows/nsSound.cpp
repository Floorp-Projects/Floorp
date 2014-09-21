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

#include "nsSound.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsCRT.h"

#include "prlog.h"
#include "prtime.h"
#include "prprf.h"
#include "prmem.h"

#include "nsNativeCharsetUtils.h"
#include "nsThreadUtils.h"

#ifdef PR_LOGGING
PRLogModuleInfo* gWin32SoundLog = nullptr;
#endif

class nsSoundPlayer: public nsRunnable {
public:
  nsSoundPlayer(nsSound *aSound, const wchar_t* aSoundName) :
    mSoundName(aSoundName), mSound(aSound)
  {
    Init();
  }

  nsSoundPlayer(nsSound *aSound, const nsAString& aSoundName) :
    mSoundName(aSoundName), mSound(aSound)
  {
    Init();
  }

  NS_DECL_NSIRUNNABLE

protected:
  nsString mSoundName;
  nsSound *mSound; // Strong, but this will be released from SoundReleaser.
  nsCOMPtr<nsIThread> mThread;

  void Init()
  {
    NS_GetCurrentThread(getter_AddRefs(mThread));
    NS_ASSERTION(mThread, "failed to get current thread");
    NS_IF_ADDREF(mSound);
  }

  class SoundReleaser: public nsRunnable {
  public:
    SoundReleaser(nsSound* aSound) :
      mSound(aSound)
    {
    }

    NS_DECL_NSIRUNNABLE

  protected:
    nsSound *mSound;
  };
};

NS_IMETHODIMP
nsSoundPlayer::Run()
{
  PR_SetCurrentThreadName("Play Sound");

  NS_PRECONDITION(!mSoundName.IsEmpty(), "Sound name should not be empty");
  ::PlaySoundW(mSoundName.get(), nullptr,
               SND_NODEFAULT | SND_ALIAS | SND_ASYNC);
  nsCOMPtr<nsIRunnable> releaser = new SoundReleaser(mSound);
  // Don't release nsSound from here, because here is not an owning thread of
  // the nsSound. nsSound must be released in its owning thread.
  mThread->Dispatch(releaser, NS_DISPATCH_NORMAL);
  return NS_OK;
}

NS_IMETHODIMP
nsSoundPlayer::SoundReleaser::Run()
{
  mSound->ShutdownOldPlayerThread();
  NS_IF_RELEASE(mSound);
  return NS_OK;
}


#ifndef SND_PURGE
// Not available on Windows CE, and according to MSDN
// doesn't do anything on recent windows either.
#define SND_PURGE 0
#endif

NS_IMPL_ISUPPORTS(nsSound, nsISound, nsIStreamLoaderObserver)


nsSound::nsSound()
{
#ifdef PR_LOGGING
    if (!gWin32SoundLog) {
      gWin32SoundLog = PR_NewLogModule("nsSound");
    }
#endif

    mLastSound = nullptr;
}

nsSound::~nsSound()
{
  NS_ASSERTION(!mPlayerThread, "player thread is not null but should be");
  PurgeLastSound();
}

void nsSound::ShutdownOldPlayerThread()
{
  if (mPlayerThread) {
    mPlayerThread->Shutdown();
    mPlayerThread = nullptr;
  }
}

void nsSound::PurgeLastSound() 
{
  if (mLastSound) {
    // Halt any currently playing sound.
    ::PlaySound(nullptr, nullptr, SND_PURGE);

    // Now delete the buffer.
    free(mLastSound);
    mLastSound = nullptr;
  }
}

NS_IMETHODIMP nsSound::Beep()
{
  ::MessageBeep(0);

  return NS_OK;
}

NS_IMETHODIMP nsSound::OnStreamComplete(nsIStreamLoader *aLoader,
                                        nsISupports *context,
                                        nsresult aStatus,
                                        uint32_t dataLen,
                                        const uint8_t *data)
{
  // print a load error on bad status
  if (NS_FAILED(aStatus)) {
#ifdef DEBUG
    if (aLoader) {
      nsCOMPtr<nsIRequest> request;
      nsCOMPtr<nsIChannel> channel;
      aLoader->GetRequest(getter_AddRefs(request));
      if (request)
          channel = do_QueryInterface(request);
      if (channel) {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        if (uri) {
          nsAutoCString uriSpec;
          uri->GetSpec(uriSpec);
          PR_LOG(gWin32SoundLog, PR_LOG_ALWAYS,
                 ("Failed to load %s\n", uriSpec.get()));
        }
      }
    }
#endif
    return aStatus;
  }

  ShutdownOldPlayerThread();
  PurgeLastSound();

  if (data && dataLen > 0) {
    DWORD flags = SND_MEMORY | SND_NODEFAULT;
    // We try to make a copy so we can play it async.
    mLastSound = (uint8_t *) malloc(dataLen);
    if (mLastSound) {
      memcpy(mLastSound, data, dataLen);
      data = mLastSound;
      flags |= SND_ASYNC;
    }
    ::PlaySoundW(reinterpret_cast<LPCWSTR>(data), 0, flags);
  }

  return NS_OK;
}

NS_IMETHODIMP nsSound::Play(nsIURL *aURL)
{
  nsresult rv;

#ifdef DEBUG_SOUND
  char *url;
  aURL->GetSpec(&url);
  PR_LOG(gWin32SoundLog, PR_LOG_ALWAYS,
         ("%s\n", url));
#endif

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader),
                          aURL,
                          this, // aObserver
                          nsContentUtils::GetSystemPrincipal(),
                          nsILoadInfo::SEC_NORMAL,
                          nsIContentPolicy::TYPE_OTHER);
  return rv;
}


NS_IMETHODIMP nsSound::Init()
{
  // This call halts a sound if it was still playing.
  // We have to use the sound library for something to make sure
  // it is initialized.
  // If we wait until the first sound is played, there will
  // be a time lag as the library gets loaded.
  ::PlaySound(nullptr, nullptr, SND_PURGE);

  return NS_OK;
}


NS_IMETHODIMP nsSound::PlaySystemSound(const nsAString &aSoundAlias)
{
  ShutdownOldPlayerThread();
  PurgeLastSound();

  if (!NS_IsMozAliasSound(aSoundAlias)) {
    if (aSoundAlias.IsEmpty())
      return NS_OK;
    nsCOMPtr<nsIRunnable> player = new nsSoundPlayer(this, aSoundAlias);
    NS_ENSURE_TRUE(player, NS_ERROR_OUT_OF_MEMORY);
    nsresult rv = NS_NewThread(getter_AddRefs(mPlayerThread), player);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  NS_WARNING("nsISound::playSystemSound is called with \"_moz_\" events, they are obsolete, use nsISound::playEventSound instead");

  uint32_t eventId;
  if (aSoundAlias.Equals(NS_SYSSOUND_MAIL_BEEP))
    eventId = EVENT_NEW_MAIL_RECEIVED;
  else if (aSoundAlias.Equals(NS_SYSSOUND_CONFIRM_DIALOG))
    eventId = EVENT_CONFIRM_DIALOG_OPEN;
  else if (aSoundAlias.Equals(NS_SYSSOUND_ALERT_DIALOG))
    eventId = EVENT_ALERT_DIALOG_OPEN;
  else if (aSoundAlias.Equals(NS_SYSSOUND_MENU_EXECUTE))
    eventId = EVENT_MENU_EXECUTE;
  else if (aSoundAlias.Equals(NS_SYSSOUND_MENU_POPUP))
    eventId = EVENT_MENU_POPUP;
  else
    return NS_OK;

  return PlayEventSound(eventId);
}

NS_IMETHODIMP nsSound::PlayEventSound(uint32_t aEventId)
{
  ShutdownOldPlayerThread();
  PurgeLastSound();

  const wchar_t *sound = nullptr;
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

  nsCOMPtr<nsIRunnable> player = new nsSoundPlayer(this, sound);
  NS_ENSURE_TRUE(player, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = NS_NewThread(getter_AddRefs(mPlayerThread), player);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}
