/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Password Manager.
 *
 * The Initial Developer of the Original Code is
 * Brian Ryner.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#include "nsSingleSignonPrompt.h"
#include "nsPasswordManager.h"
#include "nsIServiceManager.h"

NS_IMPL_ISUPPORTS2(nsSingleSignonPrompt,
                   nsIAuthPromptWrapper,
                   nsIAuthPrompt)

// nsIAuthPrompt
NS_IMETHODIMP
nsSingleSignonPrompt::Prompt(const PRUnichar* aDialogTitle,
                             const PRUnichar* aText,
                             const PRUnichar* aPasswordRealm,
                             PRUint32 aSavePassword,
                             const PRUnichar* aDefaultText,
                             PRUnichar** aResult,
                             PRBool* aConfirm)
{
  nsAutoString checkMsg;
  nsString emptyString;
  PRBool checkValue = PR_FALSE;
  PRBool *checkPtr = nsnull;
  PRUnichar* value = nsnull;
  nsCOMPtr<nsIPasswordManagerInternal> mgrInternal;

  if (nsPasswordManager::SingleSignonEnabled() && aPasswordRealm) {
    if (aSavePassword == SAVE_PASSWORD_PERMANENTLY) {
      nsPasswordManager::GetLocalizedString(NS_LITERAL_STRING("rememberValue"),
                                            checkMsg);
      checkPtr = &checkValue;
    }

    mgrInternal = do_GetService(NS_PASSWORDMANAGER_CONTRACTID);
    nsCAutoString outHost;
    nsAutoString outUser, outPassword;

    mgrInternal->FindPasswordEntry(NS_ConvertUCS2toUTF8(aPasswordRealm),
                                   emptyString,
                                   emptyString,
                                   outHost,
                                   outUser,
                                   outPassword);

    if (!outUser.IsEmpty()) {
      value = ToNewUnicode(outUser);
      checkValue = PR_TRUE;
    }
  }

  if (!value && aDefaultText)
    value = ToNewUnicode(nsDependentString(aDefaultText));

  mPrompt->Prompt(aDialogTitle,
                  aText,
                  &value,
                  checkMsg.get(),
                  checkPtr,
                  aConfirm);

  if (*aConfirm) {
    if (checkValue && value[0] != '\0') {
      // The user requested that we save the value
      // TODO: support SAVE_PASSWORD_FOR_SESSION

      nsCOMPtr<nsIPasswordManager> manager = do_QueryInterface(mgrInternal);

      manager->AddUser(NS_ConvertUCS2toUTF8(aPasswordRealm),
                       nsDependentString(value),
                       emptyString);
    }

    *aResult = value;
  } else {
    if (value)
      nsMemory::Free(value);
    *aResult = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSingleSignonPrompt::PromptUsernameAndPassword(const PRUnichar* aDialogTitle,
                                                const PRUnichar* aText,
                                                const PRUnichar* aPasswordRealm,
                                                PRUint32 aSavePassword,
                                                PRUnichar** aUser,
                                                PRUnichar** aPassword,
                                                PRBool* aConfirm)
{
  nsAutoString checkMsg;
  nsString emptyString;
  PRBool checkValue = PR_FALSE;
  PRBool *checkPtr = nsnull;
  PRUnichar *user = nsnull, *password = nsnull;
  nsCOMPtr<nsIPasswordManagerInternal> mgrInternal;

  if (nsPasswordManager::SingleSignonEnabled() && aPasswordRealm) {
    if (aSavePassword == SAVE_PASSWORD_PERMANENTLY) {
      nsPasswordManager::GetLocalizedString(NS_LITERAL_STRING("rememberPassword"),
                                            checkMsg);
      checkPtr = &checkValue;
    }

    mgrInternal = do_GetService(NS_PASSWORDMANAGER_CONTRACTID);
    nsCAutoString outHost;
    nsAutoString outUser, outPassword;

    mgrInternal->FindPasswordEntry(NS_ConvertUCS2toUTF8(aPasswordRealm),
                                   emptyString,
                                   emptyString,
                                   outHost,
                                   outUser,
                                   outPassword);

    user = ToNewUnicode(outUser);
    password = ToNewUnicode(outPassword);
    if (!outUser.IsEmpty() || !outPassword.IsEmpty())
      checkValue = PR_TRUE;
  }

  mPrompt->PromptUsernameAndPassword(aDialogTitle,
                                     aText,
                                     &user,
                                     &password,
                                     checkMsg.get(),
                                     checkPtr,
                                     aConfirm);

  if (*aConfirm) {
    if (checkValue && (user[0] != '\0' || password[0] != '\0')) {
      // The user requested that we save the values
      // TODO: support SAVE_PASSWORD_FOR_SESSION

      nsCOMPtr<nsIPasswordManager> manager = do_QueryInterface(mgrInternal);

      manager->AddUser(NS_ConvertUCS2toUTF8(aPasswordRealm),
                       nsDependentString(user),
                       nsDependentString(password));
    }

    *aUser = user;
    *aPassword = password;

  } else {
    if (user)
      nsMemory::Free(user);
    if (password)
      nsMemory::Free(password);

    *aUser = *aPassword = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSingleSignonPrompt::PromptPassword(const PRUnichar* aDialogTitle,
                                     const PRUnichar* aText,
                                     const PRUnichar* aPasswordRealm,
                                     PRUint32 aSavePassword,
                                     PRUnichar** aPassword,
                                     PRBool* aConfirm)
{
  nsAutoString checkMsg;
  nsString emptyString;
  PRBool checkValue = PR_FALSE;
  PRBool *checkPtr = nsnull;
  PRUnichar* password = nsnull;
  nsCOMPtr<nsIPasswordManagerInternal> mgrInternal;

  if (nsPasswordManager::SingleSignonEnabled() && aPasswordRealm) {
    if (aSavePassword == SAVE_PASSWORD_PERMANENTLY) {
      nsPasswordManager::GetLocalizedString(NS_LITERAL_STRING("rememberPassword"),
                                            checkMsg);
      checkPtr = &checkValue;
    }

    mgrInternal = do_GetService(NS_PASSWORDMANAGER_CONTRACTID);
    nsCAutoString outHost;
    nsAutoString outUser, outPassword;

    mgrInternal->FindPasswordEntry(NS_ConvertUCS2toUTF8(aPasswordRealm),
                                   emptyString,
                                   emptyString,
                                   outHost,
                                   outUser,
                                   outPassword);

    password = ToNewUnicode(outPassword);
    if (!outPassword.IsEmpty())
      checkValue = PR_TRUE;
  }

  mPrompt->PromptPassword(aDialogTitle,
                          aText,
                          &password,
                          checkMsg.get(),
                          checkPtr,
                          aConfirm);

  if (*aConfirm) {
    if (checkValue && password[0] != '\0') {
      // The user requested that we save the password
      // TODO: support SAVE_PASSWORD_FOR_SESSION

      nsCOMPtr<nsIPasswordManager> manager = do_QueryInterface(mgrInternal);

      manager->AddUser(NS_ConvertUCS2toUTF8(aPasswordRealm),
                       emptyString,
                       nsDependentString(password));
    }

    *aPassword = password;

  } else {
    if (password)
      nsMemory::Free(password);
    *aPassword = nsnull;
  }

  return NS_OK;
}

// nsIAuthPromptWrapper

NS_IMETHODIMP
nsSingleSignonPrompt::SetPromptDialogs(nsIPrompt* aDialogs)
{
  mPrompt = aDialogs;
  return NS_OK;
}
