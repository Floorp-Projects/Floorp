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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Terry Hayes <thayes@netscape.com>
 *   Javier Delgadillo <javi@netscape.com>
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

#include "nsSecurityWarningDialogs.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsIPrompt.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsSecurityWarningDialogs, nsISecurityWarningDialogs)

static NS_DEFINE_CID(kCStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);

#define STRING_BUNDLE_URL    "chrome://communicator/locale/security.properties"

#define ENTER_SITE_PREF      "security.warn_entering_secure"
#define WEAK_SITE_PREF       "security.warn_entering_weak"
#define LEAVE_SITE_PREF      "security.warn_leaving_secure"
#define MIXEDCONTENT_PREF    "security.warn_viewing_mixed"
#define INSECURE_SUBMIT_PREF "security.warn_submit_insecure"

nsSecurityWarningDialogs::nsSecurityWarningDialogs()
{
}

nsSecurityWarningDialogs::~nsSecurityWarningDialogs()
{
}

nsresult
nsSecurityWarningDialogs::Init()
{
  nsresult rv;

  mPrefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStringBundleService> service = do_GetService(kCStringBundleServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = service->CreateBundle(STRING_BUNDLE_URL,
                             getter_AddRefs(mStringBundle));
  return rv;
}

NS_IMETHODIMP 
nsSecurityWarningDialogs::ConfirmEnteringSecure(nsIInterfaceRequestor *ctx, PRBool *_retval)
{
  nsresult rv;

  rv = AlertDialog(ctx, ENTER_SITE_PREF, 
                   NS_LITERAL_STRING("EnterSecureMessage").get(),
                   NS_LITERAL_STRING("EnterSecureShowAgain").get());

  *_retval = PR_TRUE;
  return rv;
}

NS_IMETHODIMP 
nsSecurityWarningDialogs::ConfirmEnteringWeak(nsIInterfaceRequestor *ctx, PRBool *_retval)
{
  nsresult rv;

  rv = AlertDialog(ctx, WEAK_SITE_PREF,
                   NS_LITERAL_STRING("WeakSecureMessage").get(),
                   NS_LITERAL_STRING("WeakSecureShowAgain").get());

  *_retval = PR_TRUE;
  return rv;
}

NS_IMETHODIMP 
nsSecurityWarningDialogs::ConfirmLeavingSecure(nsIInterfaceRequestor *ctx, PRBool *_retval)
{
  nsresult rv;

  rv = AlertDialog(ctx, LEAVE_SITE_PREF, 
                   NS_LITERAL_STRING("LeaveSecureMessage").get(),
                   NS_LITERAL_STRING("LeaveSecureShowAgain").get());

  *_retval = PR_TRUE;
  return rv;
}


NS_IMETHODIMP 
nsSecurityWarningDialogs::ConfirmMixedMode(nsIInterfaceRequestor *ctx, PRBool *_retval)
{
  nsresult rv;

  rv = AlertDialog(ctx, MIXEDCONTENT_PREF, 
                   NS_LITERAL_STRING("MixedContentMessage").get(),
                   NS_LITERAL_STRING("MixedContentShowAgain").get());

  *_retval = PR_TRUE;
  return rv;
}


nsresult
nsSecurityWarningDialogs::AlertDialog(nsIInterfaceRequestor *ctx, const char *prefName,
                          const PRUnichar *dialogMessageName,
                          const PRUnichar *showAgainName)
{
  nsresult rv;

  // Get user's preference for this alert
  PRBool prefValue;
  rv = mPrefBranch->GetBoolPref(prefName, &prefValue);
  if (NS_FAILED(rv)) prefValue = PR_TRUE;

  // Stop if alert is not requested
  if (!prefValue) return NS_OK;

  // Check for a show-once pref for this dialog.
  // If the show-once pref is set to true:
  //   - The default value of the "show every time" checkbox is unchecked
  //   - If the user checks the checkbox, we clear the show-once pref.

  nsCAutoString showOncePref(prefName);
  showOncePref += ".show_once";

  PRBool showOnce = PR_FALSE;
  mPrefBranch->GetBoolPref(showOncePref.get(), &showOnce);

  if (showOnce)
    prefValue = PR_FALSE;

  // Get Prompt to use
  nsCOMPtr<nsIPrompt> prompt = do_GetInterface(ctx);
  if (!prompt) return NS_ERROR_FAILURE;

  // Get messages strings from localization file
  nsXPIDLString windowTitle, message, dontShowAgain;

  mStringBundle->GetStringFromName(NS_LITERAL_STRING("Title").get(),
                                   getter_Copies(windowTitle));
  mStringBundle->GetStringFromName(dialogMessageName,
                                   getter_Copies(message));
  mStringBundle->GetStringFromName(showAgainName,
                                   getter_Copies(dontShowAgain));
  if (!windowTitle || !message || !dontShowAgain) return NS_ERROR_FAILURE;

  rv = prompt->AlertCheck(windowTitle, message, dontShowAgain, &prefValue);
  if (NS_FAILED(rv)) return rv;
      
  if (!prefValue) {
    mPrefBranch->SetBoolPref(prefName, PR_FALSE);
  } else if (showOnce) {
    mPrefBranch->SetBoolPref(showOncePref.get(), PR_FALSE);
  }

  return rv;
}

NS_IMETHODIMP 
nsSecurityWarningDialogs::ConfirmPostToInsecure(nsIInterfaceRequestor *ctx, PRBool* _result)
{
  nsresult rv;

  rv = ConfirmDialog(ctx, INSECURE_SUBMIT_PREF,
                     NS_LITERAL_STRING("PostToInsecureFromInsecureMessage").get(),
                     NS_LITERAL_STRING("PostToInsecureFromInsecureShowAgain").get(),
                     _result);

  return rv;
}

NS_IMETHODIMP 
nsSecurityWarningDialogs::ConfirmPostToInsecureFromSecure(nsIInterfaceRequestor *ctx, PRBool* _result)
{
  nsresult rv;

  rv = ConfirmDialog(ctx, nsnull, // No preference for this one - it's too important
                     NS_LITERAL_STRING("PostToInsecureFromSecureMessage").get(),
                     nsnull, 
                     _result);

  return rv;
}

nsresult
nsSecurityWarningDialogs::ConfirmDialog(nsIInterfaceRequestor *ctx, const char *prefName,
                            const PRUnichar *messageName, 
                            const PRUnichar *showAgainName, 
                            PRBool* _result)
{
  nsresult rv;

  // Get user's preference for this alert
  // prefName, showAgainName are null if there is no preference for this dialog
  PRBool prefValue = PR_TRUE;
  
  if (prefName != nsnull) {
    rv = mPrefBranch->GetBoolPref(prefName, &prefValue);
    if (NS_FAILED(rv)) prefValue = PR_TRUE;
  }
  
  // Stop if confirm is not requested
  if (!prefValue) {
    *_result = PR_TRUE;
    return NS_OK;
  }
  
  // See AlertDialog() for a description of how showOnce works.
  nsCAutoString showOncePref(prefName);
  showOncePref += ".show_once";

  PRBool showOnce = PR_FALSE;
  mPrefBranch->GetBoolPref(showOncePref.get(), &showOnce);

  if (showOnce)
    prefValue = PR_FALSE;

  // Get Prompt to use
  nsCOMPtr<nsIPrompt> prompt = do_GetInterface(ctx);
  if (!prompt) return NS_ERROR_FAILURE;

  // Get messages strings from localization file
  nsXPIDLString windowTitle, message, alertMe, cont;

  mStringBundle->GetStringFromName(NS_LITERAL_STRING("Title").get(),
                                   getter_Copies(windowTitle));
  mStringBundle->GetStringFromName(messageName,
                                   getter_Copies(message));
  if (showAgainName != nsnull) {
    mStringBundle->GetStringFromName(showAgainName,
                                     getter_Copies(alertMe));
  }
  mStringBundle->GetStringFromName(NS_LITERAL_STRING("Continue").get(),
                                   getter_Copies(cont));
  // alertMe is allowed to be null
  if (!windowTitle || !message || !cont) return NS_ERROR_FAILURE;
      
  // Replace # characters with newlines to lay out the dialog.
  PRUnichar* msgchars = message.BeginWriting();
  
  PRUint32 i = 0;
  for (i = 0; msgchars[i] != '\0'; i++) {
    if (msgchars[i] == '#') {
      msgchars[i] = '\n';
    }
  }  

  PRInt32 buttonPressed;

  rv  = prompt->ConfirmEx(windowTitle, 
                          message, 
                          (nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_0) +
                          (nsIPrompt::BUTTON_TITLE_CANCEL * nsIPrompt::BUTTON_POS_1),
                          cont,
                          nsnull,
                          nsnull,
                          alertMe, 
                          &prefValue, 
                          &buttonPressed);

  if (NS_FAILED(rv)) return rv;

  *_result = (buttonPressed != 1);

  if (!prefValue && prefName != nsnull) {
    mPrefBranch->SetBoolPref(prefName, PR_FALSE);
  } else if (prefValue && showOnce) {
    mPrefBranch->SetBoolPref(showOncePref.get(), PR_FALSE);
  }

  return rv;
}

