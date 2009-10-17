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

#include "nsSystemSoundService.h"
#include "nsServiceManagerUtils.h"
#include <windows.h>
#include <mmsystem.h>

#ifndef SND_PURGE
// Not available on Windows CE, and according to MSDN
// doesn't do anything on recent windows either.
#define SND_PURGE 0
#endif

class nsStopSoundPlayer : public nsRunnable {
public:
  nsStopSoundPlayer()
  {
  }

  NS_DECL_NSIRUNNABLE
};

NS_IMETHODIMP
nsStopSoundPlayer::Run()
{
  nsSystemSoundServiceBase::StopSoundPlayer();
  return NS_OK;
}

class nsSystemSoundPlayer : public nsRunnable {
public:
  nsSystemSoundPlayer(PRUint32 aEventID) :
    mEventID(aEventID)
  {
  }

  nsSystemSoundPlayer(const nsAString &aName) :
    mEventID(PR_UINT32_MAX), mName(aName)
  {
  }

  NS_DECL_NSIRUNNABLE

protected:
  PRUint32 mEventID;
  nsString mName;
};

NS_IMETHODIMP
nsSystemSoundPlayer::Run()
{
  const wchar_t *sound = nsnull;
  if (!mName.IsEmpty()) {
    sound = static_cast<const wchar_t*>(mName.get());
  } else if (mEventID != PR_UINT32_MAX) {
    switch (mEventID) {
      case nsISystemSoundService::EVENT_NEW_MAIL_RECEIVED:
        sound = L"MailBeep";
        break;
      case nsISystemSoundService::EVENT_ALERT_DIALOG_OPEN:
        sound = L"SystemExclamation";
        break;
      case nsISystemSoundService::EVENT_CONFIRM_DIALOG_OPEN:
        sound = L"SystemQuestion";
        break;
      case nsISystemSoundService::EVENT_MENU_EXECUTE:
        sound = L"MenuCommand";
        break;
      case nsISystemSoundService::EVENT_MENU_POPUP:
        sound = L"MenuPopup";
        break;
      case nsISystemSoundService::EVENT_MENU_NOT_FOUND:
        // Just beep
        ::MessageBeep(0);
        return NS_OK;
      default:
        // Win32 plays no sounds at NS_SYSSOUND_PROMPT_DIALOG and
        // NS_SYSSOUND_SELECT_DIALOG.
        return NS_OK;
    }
  }
  if (sound) {
    nsCOMPtr<nsStopSoundPlayer> stopper = new nsStopSoundPlayer();
    NS_DispatchToMainThread(stopper, nsIEventTarget::DISPATCH_SYNC);
    ::PlaySoundW(sound, NULL, SND_NODEFAULT | SND_ALIAS | SND_ASYNC);
  }
  return NS_OK;
}

/*****************************************************************************
 *  nsSystemSoundService implementation
 *****************************************************************************/

NS_IMPL_ISUPPORTS1(nsSystemSoundService, nsISystemSoundService)

NS_IMPL_ISYSTEMSOUNDSERVICE_GETINSTANCE(nsSystemSoundService)

nsSystemSoundService::nsSystemSoundService() :
  nsSystemSoundServiceBase()
{
}

nsSystemSoundService::~nsSystemSoundService()
{
  if (mPlayerThread) {
    mPlayerThread->Shutdown();
    mPlayerThread = nsnull;
  }
}

nsresult
nsSystemSoundService::Init()
{
  // This call halts a sound if it was still playing.
  // We have to use the sound library for something to make sure
  // it is initialized.
  // If we wait until the first sound is played, there will
  // be a time lag as the library gets loaded.
  ::PlaySound(nsnull, nsnull, SND_PURGE);

  return NS_OK;
}

NS_IMETHODIMP
nsSystemSoundService::Beep()
{
  nsresult rv = nsSystemSoundServiceBase::Beep();
  NS_ENSURE_SUCCESS(rv, rv);

  ::MessageBeep(0);
  return NS_OK;
}

NS_IMETHODIMP
nsSystemSoundService::PlayAlias(const nsAString &aSoundAlias)
{
  nsresult rv = nsSystemSoundServiceBase::PlayAlias(aSoundAlias);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsSystemSoundPlayer> player = new nsSystemSoundPlayer(aSoundAlias);
  NS_ENSURE_TRUE(player, NS_ERROR_OUT_OF_MEMORY);
  return PostPlayer(player);
}

NS_IMETHODIMP
nsSystemSoundService::PlayEventSound(PRUint32 aEventID)
{
  nsresult rv = nsSystemSoundServiceBase::PlayEventSound(aEventID);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsSystemSoundPlayer> player = new nsSystemSoundPlayer(aEventID);
  NS_ENSURE_TRUE(player, NS_ERROR_OUT_OF_MEMORY);
  return PostPlayer(player);
}

nsresult
nsSystemSoundService::PostPlayer(nsSystemSoundPlayer *aPlayer)
{
  nsresult rv;
  if (mPlayerThread) {
    rv = mPlayerThread->Dispatch(aPlayer, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  rv = NS_NewThread(getter_AddRefs(mPlayerThread), aPlayer);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(mPlayerThread, NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}
