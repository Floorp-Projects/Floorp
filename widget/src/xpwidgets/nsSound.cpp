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
 * Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#include "nsCOMPtr.h"
#include "nsSound.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsITimer.h"


/*****************************************************************************
 *  nsSound implementation
 *****************************************************************************/

NS_IMPL_ISUPPORTS1(nsSound, nsISound)

nsSound::nsSound()
{
}

nsSound::~nsSound()
{
}

nsresult
nsSound::Stop()
{
  nsSystemSoundServiceBase::StopSoundPlayer();
  return NS_OK;
}

NS_IMETHODIMP
nsSound::Beep()
{
  nsCOMPtr<nsISystemSoundService> sysSound =
    nsSystemSoundServiceBase::GetSystemSoundService();
  NS_ENSURE_TRUE(sysSound, NS_ERROR_FAILURE);
  return sysSound->Beep();
}

NS_IMETHODIMP
nsSound::Init()
{
  // Nothing to do
  return NS_OK;
}

NS_IMETHODIMP
nsSound::PlayEventSound(PRUint32 aEventID)
{
  // Stop all sounds before we play a system sound.
  nsresult rv = Stop();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISystemSoundService> sysSound =
    nsSystemSoundServiceBase::GetSystemSoundService();
  NS_ENSURE_TRUE(sysSound, NS_ERROR_FAILURE);
  return sysSound->PlayEventSound(aEventID);
}

NS_IMETHODIMP
nsSound::Play(nsIURL *aURL)
{
  NS_ENSURE_ARG_POINTER(aURL);
  nsresult rv = nsSystemSoundServiceBase::Play(aURL);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
nsSound::PlaySystemSound(const nsAString &aSoundAlias)
{
  if (aSoundAlias.IsEmpty())
    return NS_OK;

  // Stop all sounds before we play a system sound.
  nsresult rv = Stop();
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(!NS_IsMozAliasSound(aSoundAlias),
    "nsISound::playSystemSound is called with \"_moz_\" events, they are obsolete, use nsISystemSoundService::playEventSound instead");
  if (aSoundAlias.Equals(NS_SYSSOUND_MAIL_BEEP)) {
    rv = PlayEventSound(EVENT_NEW_MAIL_RECEIVED);
  } else if (aSoundAlias.Equals(NS_SYSSOUND_CONFIRM_DIALOG)) {
    rv = PlayEventSound(EVENT_CONFIRM_DIALOG_OPEN);
  } else if (aSoundAlias.Equals(NS_SYSSOUND_ALERT_DIALOG)) {
    rv = PlayEventSound(EVENT_ALERT_DIALOG_OPEN);
  } else if (aSoundAlias.Equals(NS_SYSSOUND_PROMPT_DIALOG)) {
    rv = PlayEventSound(EVENT_PROMPT_DIALOG_OPEN);
  } else if (aSoundAlias.Equals(NS_SYSSOUND_SELECT_DIALOG)) {
    rv = PlayEventSound(EVENT_SELECT_DIALOG_OPEN);
  } else if (aSoundAlias.Equals(NS_SYSSOUND_MENU_EXECUTE)) {
    rv = PlayEventSound(EVENT_MENU_EXECUTE);
  } else if (aSoundAlias.Equals(NS_SYSSOUND_MENU_POPUP)) {
    rv = PlayEventSound(EVENT_MENU_POPUP);
  } else {
    // First, assume that the given string is an alias sound name.
    nsCOMPtr<nsISystemSoundService> sysSound =
      nsSystemSoundServiceBase::GetSystemSoundService();
    NS_ENSURE_TRUE(sysSound, NS_ERROR_FAILURE);
    rv = sysSound->PlayAlias(aSoundAlias);
    NS_ENSURE_SUCCESS(rv, rv);
    if (rv != NS_SUCCESS_NOT_SUPPORTED) {
      // The current platform supports the alias sound name, we shouldn't assume
      // that the given string is a file path.
      return NS_OK;
    }

    // If this platform doesn't support the alias system sound names, we should
    // assume that the given string is a file path.
    nsCOMPtr<nsIFileURL> fileURL =
      nsSystemSoundServiceBase::GetFileURL(aSoundAlias);
    if (!fileURL) {
      return NS_OK;
    }
    rv = Play(fileURL);
  }

  NS_ENSURE_SUCCESS(rv, rv);
  return rv;
}

/*****************************************************************************
 *  nsSystemSoundServiceBase implementation
 *****************************************************************************/

nsISystemSoundService* nsSystemSoundServiceBase::sInstance = nsnull;
PRBool nsSystemSoundServiceBase::sIsInitialized = PR_FALSE;

NS_IMPL_ISUPPORTS1(nsSystemSoundServiceBase, nsISystemSoundService)

nsSystemSoundServiceBase::nsSystemSoundServiceBase()
{
}

nsSystemSoundServiceBase::~nsSystemSoundServiceBase()
{
  if (sInstance == this) {
    OnShutdown();
    sInstance = nsnull;
    sIsInitialized = PR_FALSE;
  }
}

nsresult
nsSystemSoundServiceBase::Init()
{
  return NS_OK;
}

void
nsSystemSoundServiceBase::OnShutdown()
{
  // Nothing to do.
}

/* static */ void
nsSystemSoundServiceBase::InitService()
{
  nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (!timer) {
    return; // OOM
  }
  NS_ADDREF(timer); // will be released in ExecuteInitService
  nsresult rv =
    timer->InitWithFuncCallback(ExecuteInitService, nsnull, 0,
                                nsITimer::TYPE_ONE_SHOT);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "nsITimer::InitWithFuncCallback failed");
}

/* static */ void
nsSystemSoundServiceBase::ExecuteInitService(nsITimer* aTimer, void* aClosure)
{
  NS_IF_RELEASE(aTimer);

  // The instance should be created this time.
  nsCOMPtr<nsISystemSoundService> sysSound =
    do_GetService("@mozilla.org/systemsoundservice;1");
  if (!sysSound) {
    return;
  }
  nsresult rv = static_cast<nsSystemSoundServiceBase*>(sysSound.get())->Init();
  sIsInitialized = NS_SUCCEEDED(rv);
}

/* static */ already_AddRefed<nsIFileURL>
nsSystemSoundServiceBase::GetFileURL(const nsAString &aFilePath)
{
  if (aFilePath.IsEmpty()) {
    return nsnull;
  }

  nsresult rv;
  nsCOMPtr<nsILocalFile> file;
  rv = NS_NewLocalFile(aFilePath, PR_TRUE, getter_AddRefs(file));
  if (rv == NS_ERROR_FILE_UNRECOGNIZED_PATH) {
    return nsnull;
  }
  NS_ENSURE_SUCCESS(rv, nsnull);
  NS_ENSURE_TRUE(file, nsnull);

  PRBool isExists;
  PRBool isFile;
  rv = file->Exists(&isExists);
  NS_ENSURE_SUCCESS(rv, nsnull);
  rv = file->IsFile(&isFile);
  NS_ENSURE_SUCCESS(rv, nsnull);
  if (!isExists || !isFile) {
    return nsnull;
  }

  nsCOMPtr<nsIURI> fileURI;
  rv = NS_NewFileURI(getter_AddRefs(fileURI), file);
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(fileURI, &rv);
  NS_ENSURE_SUCCESS(rv, nsnull);

  // The file URL must not be null here because the given string is a native
  // file path of an existing file.
  NS_ENSURE_TRUE(fileURI, nsnull);

  return fileURL.forget();
}

/* static */ already_AddRefed<nsISystemSoundService>
nsSystemSoundServiceBase::GetSystemSoundService()
{
  NS_ENSURE_TRUE(sInstance, nsnull);
  NS_ADDREF(sInstance);
  return sInstance;
}

/* static */ already_AddRefed<nsISoundPlayer>
nsSystemSoundServiceBase::GetSoundPlayer()
{
  nsCOMPtr<nsISoundPlayer> player =
    do_GetService("@mozilla.org/content/media/soundplayer;1");
  NS_ENSURE_TRUE(player, nsnull);
  return player.forget();
}

/* static */ nsresult
nsSystemSoundServiceBase::PlayFile(const nsAString &aFilePath)
{
  nsCOMPtr<nsIFileURL> fileURL = GetFileURL(aFilePath);
  if (!fileURL) {
    return NS_OK;
  }

  nsCOMPtr<nsISoundPlayer> player = GetSoundPlayer();
  NS_ENSURE_TRUE(player, NS_ERROR_FAILURE);

  return player->Play(fileURL);
}

/* static */ nsresult
nsSystemSoundServiceBase::Play(nsIURL *aURL)
{
  nsCOMPtr<nsISoundPlayer> player = GetSoundPlayer();
  NS_ENSURE_TRUE(player, NS_ERROR_FAILURE);

  // We don't need to stop the previous sound before we play a new sound.
  return player->Play(aURL);
}

/* static */ void
nsSystemSoundServiceBase::StopSoundPlayer()
{
  nsCOMPtr<nsISoundPlayer> player = GetSoundPlayer();
  if (player) {
    player->Stop();
  }
}

NS_IMETHODIMP
nsSystemSoundServiceBase::Beep()
{
  NS_ENSURE_TRUE(sIsInitialized, NS_ERROR_NOT_INITIALIZED);
  return NS_SUCCESS_NOT_SUPPORTED;
}

NS_IMETHODIMP
nsSystemSoundServiceBase::PlayAlias(const nsAString &aSoundAlias)
{
  NS_ENSURE_TRUE(sIsInitialized, NS_ERROR_NOT_INITIALIZED);
  return NS_SUCCESS_NOT_SUPPORTED;
}

NS_IMETHODIMP
nsSystemSoundServiceBase::PlayEventSound(PRUint32 aEventID)
{
  NS_ENSURE_TRUE(sIsInitialized, NS_ERROR_NOT_INITIALIZED);
  return NS_SUCCESS_NOT_SUPPORTED;
}
