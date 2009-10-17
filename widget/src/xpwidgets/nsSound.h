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

#ifndef __nsSound_h__
#define __nsSound_h__

#include "nsISound.h"
#include "nsISystemSoundService.h"
#include "nsCOMPtr.h"
#include "nsISoundPlayer.h"
#include "nsString.h"
#include "nsIFileURL.h"

class nsITimer;

class nsSound : public nsISound
{
public:
  nsSound();
  virtual ~nsSound();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISOUND

protected:
  nsresult Stop();
};

// Playing system sound methods of nsSystemSoundServiceBase return this code if
// the platform doesn't support the method.  So, they never return
// NS_ERROR_NOT_IMPLEMENTED.
#define NS_SUCCESS_NOT_SUPPORTED \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 1)

// The implementation class of nsISystemSoundService should be singleton.
// These macros declares/implements the GetInstance method.
#define NS_DECL_ISYSTEMSOUNDSERVICE_GETINSTANCE(_InstanceClass) \
  static _InstanceClass* GetInstance();

#define NS_IMPL_ISYSTEMSOUNDSERVICE_GETINSTANCE(_InstanceClass)                \
_InstanceClass*                                                                \
_InstanceClass::GetInstance()                                                  \
{                                                                              \
  if (sInstance) {                                                             \
    NS_ADDREF(sInstance);                                                      \
    return static_cast<_InstanceClass*>(sInstance);                            \
  }                                                                            \
                                                                               \
  sInstance = new _InstanceClass();                                            \
  NS_ENSURE_TRUE(sInstance, nsnull);                                           \
  NS_ADDREF(sInstance);                                                        \
  return static_cast<_InstanceClass*>(sInstance);                              \
}

// Base class of each platform-specific nsSystemSoundService.
class nsSystemSoundServiceBase : public nsISystemSoundService
{
public:
  nsSystemSoundServiceBase();
  virtual ~nsSystemSoundServiceBase();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISYSTEMSOUNDSERVICE

  static void InitService();
  static already_AddRefed<nsIFileURL> GetFileURL(const nsAString &aFilePath);
  static already_AddRefed<nsISystemSoundService> GetSystemSoundService();
  static already_AddRefed<nsISoundPlayer> GetSoundPlayer();
  static nsresult PlayFile(const nsAString &aFilePath);
  static nsresult Play(nsIURL *aURL);
  // Stops playing all in-progress sounds that are played via nsISoundPlayer.
  // This method should be called by the inherited class before it plays a
  // system sound by PlayAlias() and PlayEventSound().
  static void StopSoundPlayer();

protected:
  static nsISystemSoundService* sInstance;
  static PRBool sIsInitialized;

  virtual nsresult Init();
  virtual void OnShutdown();

private:
  static void ExecuteInitService(nsITimer* aTimer, void* aClosure);
};

#endif /* __nsSound_h__ */
