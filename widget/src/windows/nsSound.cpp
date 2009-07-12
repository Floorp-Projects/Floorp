/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsCRT.h"

#include "nsNativeCharsetUtils.h"

class nsSoundPlayer: public nsRunnable {
public:
  nsSoundPlayer(nsISound *aSound, const wchar_t* aSoundName) :
    mSound(aSound), mSoundName(aSoundName)
  {
    Init();
  }

  nsSoundPlayer(nsISound *aSound, const nsAString& aSoundName) :
    mSound(aSound), mSoundName(aSoundName)
  {
    Init();
  }

  NS_DECL_NSIRUNNABLE

protected:
  nsString mSoundName;
  nsISound *mSound; // Strong, but this will be released from SoundReleaser.
  nsCOMPtr<nsIThread> mThread;

  void Init()
  {
    NS_GetCurrentThread(getter_AddRefs(mThread));
    NS_ASSERTION(mThread, "failed to get current thread");
    NS_IF_ADDREF(mSound);
  }

  class SoundReleaser: public nsRunnable {
  public:
    SoundReleaser(nsISound* aSound) :
      mSound(aSound)
    {
    }

    NS_DECL_NSIRUNNABLE

  protected:
    nsISound *mSound;
  };
};

NS_IMETHODIMP
nsSoundPlayer::Run()
{
  NS_PRECONDITION(!mSoundName.IsEmpty(), "Sound name should not be empty");
  ::PlaySoundW(mSoundName.get(), NULL, SND_NODEFAULT | SND_ALIAS | SND_ASYNC);
  nsCOMPtr<nsIRunnable> releaser = new SoundReleaser(mSound);
  // Don't release nsISound from here, because here is not an owning thread of
  // the nsSound. nsSound must be released in its owning thread.
  mThread->Dispatch(releaser, NS_DISPATCH_NORMAL);
  return NS_OK;
}

NS_IMETHODIMP
nsSoundPlayer::SoundReleaser::Run()
{
  NS_IF_RELEASE(mSound);
  return NS_OK;
}


#ifndef SND_PURGE
// Not available on Windows CE, and according to MSDN
// doesn't do anything on recent windows either.
#define SND_PURGE 0
#endif

NS_IMPL_ISUPPORTS2(nsSound, nsISound, nsIStreamLoaderObserver)


nsSound::nsSound()
{
  mLastSound = nsnull;
}

nsSound::~nsSound()
{
  PurgeLastSound();
}

void nsSound::PurgeLastSound() {
  if (mPlayerThread) {
    mPlayerThread->Shutdown();
    mPlayerThread = nsnull;
  }
  if (mLastSound) {
    // Halt any currently playing sound.
    ::PlaySound(nsnull, nsnull, SND_PURGE);

    // Now delete the buffer.
    free(mLastSound);
    mLastSound = nsnull;
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
                                        PRUint32 dataLen,
                                        const PRUint8 *data)
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
          nsCAutoString uriSpec;
          uri->GetSpec(uriSpec);
          printf("Failed to load %s\n", uriSpec.get());
        }
      }
    }
#endif
    return aStatus;
  }

  PurgeLastSound();

  if (data && dataLen > 0) {
    DWORD flags = SND_MEMORY | SND_NODEFAULT;
    // We try to make a copy so we can play it async.
    mLastSound = (PRUint8 *) malloc(dataLen);
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
  printf("%s\n", url);
#endif

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), aURL, this);

  return rv;
}


NS_IMETHODIMP nsSound::Init()
{
  // This call halts a sound if it was still playing.
  // We have to use the sound library for something to make sure
  // it is initialized.
  // If we wait until the first sound is played, there will
  // be a time lag as the library gets loaded.
  ::PlaySound(nsnull, nsnull, SND_PURGE);

  return NS_OK;
}


NS_IMETHODIMP nsSound::PlaySystemSound(const nsAString &aSoundAlias)
{
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

  PRUint32 eventId;
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

NS_IMETHODIMP nsSound::PlayEventSound(PRUint32 aEventId)
{
  PurgeLastSound();

  const wchar_t *sound = nsnull;
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
