/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "libi18n.h"
#include "nsIServiceManager.h"
#include "nsIWalletService.h"
static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);

PUBLIC int
SI_SaveSignonData(char * filename) {
  return 0;
}

PUBLIC Bool
SI_RemoveUser(char *URLName, char *userName, Bool save) {
  return FALSE;
}

PUBLIC void
SI_RemoveAllSignonData() {
}

PUBLIC PRBool
SI_PromptUsernameAndPassword
    (MWContext *context, char *prompt,
    char **username, char **password, char *URLName)
{
  nsIWalletService *walletservice;
  nsresult res;
  PRBool status;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->
      SI_PromptUsernameAndPassword(prompt, username, password, URLName, status);
    NS_RELEASE(walletservice);
  }
  return status;
}

PUBLIC char *
SI_PromptPassword
    (MWContext *context, char *prompt, char *URLName, Bool pickFirstUser)
{
  nsIWalletService *walletservice;
  nsresult res;
  char * password;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->SI_PromptPassword(prompt, &password, URLName, pickFirstUser);
    NS_RELEASE(walletservice);
  }
  return password;
}

PUBLIC char *
SI_Prompt (MWContext *context, char *prompt,
    char* defaultUsername, char *URLName)
{
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->SI_Prompt(prompt, &defaultUsername, URLName);
    NS_RELEASE(walletservice);
  }
  return defaultUsername;
}

PUBLIC void
SI_AnonymizeSignons()
{
}

PUBLIC void
SI_UnanonymizeSignons()
{
}

PUBLIC void
SI_DisplaySignonInfoAsHTML(MWContext *context)
{
}

#ifdef xxx
PUBLIC void
SI_StartOfForm() {
}

PUBLIC int
SI_LoadSignonData(char * filename) {
  return 0;
}

PUBLIC void
SI_RememberSignonData
       (char* URLName, char** name_array, char** value_array, char** type_array, PRInt32 value_cnt)
{
}

PUBLIC void
SI_RestoreSignonData
    (char* URLName, char* name, char** value)
{
}

PUBLIC void
SI_RememberSignonDataFromBrowser
    (char* URLName, char* username, char* password)
{
}

#endif
