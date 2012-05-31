/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsSecurityWarningDialogs, nsISecurityWarningDialogs)

#define STRING_BUNDLE_URL    "chrome://pipnss/locale/security.properties"

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

  nsCOMPtr<nsIStringBundleService> service =
           do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = service->CreateBundle(STRING_BUNDLE_URL,
                             getter_AddRefs(mStringBundle));
  return rv;
}

NS_IMETHODIMP 
nsSecurityWarningDialogs::ConfirmEnteringSecure(nsIInterfaceRequestor *ctx, bool *_retval)
{
  nsresult rv;

  rv = AlertDialog(ctx, ENTER_SITE_PREF, 
                   NS_LITERAL_STRING("EnterSecureMessage").get(),
                   NS_LITERAL_STRING("EnterSecureShowAgain").get(),
                   false);

  *_retval = true;
  return rv;
}

NS_IMETHODIMP 
nsSecurityWarningDialogs::ConfirmEnteringWeak(nsIInterfaceRequestor *ctx, bool *_retval)
{
  nsresult rv;

  rv = AlertDialog(ctx, WEAK_SITE_PREF,
                   NS_LITERAL_STRING("WeakSecureMessage").get(),
                   NS_LITERAL_STRING("WeakSecureShowAgain").get(),
                   false);

  *_retval = true;
  return rv;
}

NS_IMETHODIMP 
nsSecurityWarningDialogs::ConfirmLeavingSecure(nsIInterfaceRequestor *ctx, bool *_retval)
{
  nsresult rv;

  rv = AlertDialog(ctx, LEAVE_SITE_PREF, 
                   NS_LITERAL_STRING("LeaveSecureMessage").get(),
                   NS_LITERAL_STRING("LeaveSecureShowAgain").get(),
                   false);

  *_retval = true;
  return rv;
}


NS_IMETHODIMP 
nsSecurityWarningDialogs::ConfirmMixedMode(nsIInterfaceRequestor *ctx, bool *_retval)
{
  nsresult rv;

  rv = AlertDialog(ctx, MIXEDCONTENT_PREF, 
                   NS_LITERAL_STRING("MixedContentMessage").get(),
                   NS_LITERAL_STRING("MixedContentShowAgain").get(),
                   true);

  *_retval = true;
  return rv;
}

class nsAsyncAlert : public nsRunnable
{
public:
  nsAsyncAlert(nsIPrompt*       aPrompt,
               const char*      aPrefName,
               const PRUnichar* aDialogMessageName,
               const PRUnichar* aShowAgainName,
               nsIPrefBranch*   aPrefBranch,
               nsIStringBundle* aStringBundle)
  : mPrompt(aPrompt), mPrefName(aPrefName),
    mDialogMessageName(aDialogMessageName),
    mShowAgainName(aShowAgainName), mPrefBranch(aPrefBranch),
    mStringBundle(aStringBundle) {}
  NS_IMETHOD Run();

protected:
  nsCOMPtr<nsIPrompt>       mPrompt;
  nsCString                 mPrefName;
  nsString                  mDialogMessageName;
  nsString                  mShowAgainName;
  nsCOMPtr<nsIPrefBranch>   mPrefBranch;
  nsCOMPtr<nsIStringBundle> mStringBundle;
};

NS_IMETHODIMP
nsAsyncAlert::Run()
{
  nsresult rv;

  // Get user's preference for this alert
  bool prefValue;
  rv = mPrefBranch->GetBoolPref(mPrefName.get(), &prefValue);
  if (NS_FAILED(rv)) prefValue = true;

  // Stop if alert is not requested
  if (!prefValue) return NS_OK;

  // Check for a show-once pref for this dialog.
  // If the show-once pref is set to true:
  //   - The default value of the "show every time" checkbox is unchecked
  //   - If the user checks the checkbox, we clear the show-once pref.

  nsCAutoString showOncePref(mPrefName);
  showOncePref += ".show_once";

  bool showOnce = false;
  mPrefBranch->GetBoolPref(showOncePref.get(), &showOnce);

  if (showOnce)
    prefValue = false;

  // Get messages strings from localization file
  nsXPIDLString windowTitle, message, dontShowAgain;

  mStringBundle->GetStringFromName(NS_LITERAL_STRING("Title").get(),
                                   getter_Copies(windowTitle));
  mStringBundle->GetStringFromName(mDialogMessageName.get(),
                                   getter_Copies(message));
  mStringBundle->GetStringFromName(mShowAgainName.get(),
                                   getter_Copies(dontShowAgain));
  if (!windowTitle || !message || !dontShowAgain) return NS_ERROR_FAILURE;

  rv = mPrompt->AlertCheck(windowTitle, message, dontShowAgain, &prefValue);
  if (NS_FAILED(rv)) return rv;
      
  if (!prefValue) {
    mPrefBranch->SetBoolPref(mPrefName.get(), false);
  } else if (showOnce) {
    mPrefBranch->SetBoolPref(showOncePref.get(), false);
  }

  return rv;
}


nsresult
nsSecurityWarningDialogs::AlertDialog(nsIInterfaceRequestor* aCtx,
                                      const char* aPrefName,
                                      const PRUnichar* aDialogMessageName,
                                      const PRUnichar* aShowAgainName,
                                      bool aAsync)
{
  // Get Prompt to use
  nsCOMPtr<nsIPrompt> prompt = do_GetInterface(aCtx);
  if (!prompt) return NS_ERROR_FAILURE;

  nsRefPtr<nsAsyncAlert> alert = new nsAsyncAlert(prompt,
                                                  aPrefName,
                                                  aDialogMessageName,
                                                  aShowAgainName,
                                                  mPrefBranch,
                                                  mStringBundle);
  NS_ENSURE_TRUE(alert, NS_ERROR_OUT_OF_MEMORY);
  return aAsync ? NS_DispatchToCurrentThread(alert) : alert->Run();
}



NS_IMETHODIMP 
nsSecurityWarningDialogs::ConfirmPostToInsecure(nsIInterfaceRequestor *ctx, bool* _result)
{
  nsresult rv;

  rv = ConfirmDialog(ctx, INSECURE_SUBMIT_PREF,
                     NS_LITERAL_STRING("PostToInsecureFromInsecureMessage").get(),
                     NS_LITERAL_STRING("PostToInsecureFromInsecureShowAgain").get(),
                     _result);

  return rv;
}

NS_IMETHODIMP 
nsSecurityWarningDialogs::ConfirmPostToInsecureFromSecure(nsIInterfaceRequestor *ctx, bool* _result)
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
                            bool* _result)
{
  nsresult rv;

  // Get user's preference for this alert
  // prefName, showAgainName are null if there is no preference for this dialog
  bool prefValue = true;
  
  if (prefName != nsnull) {
    rv = mPrefBranch->GetBoolPref(prefName, &prefValue);
    if (NS_FAILED(rv)) prefValue = true;
  }
  
  // Stop if confirm is not requested
  if (!prefValue) {
    *_result = true;
    return NS_OK;
  }
  
  // See AlertDialog() for a description of how showOnce works.
  nsCAutoString showOncePref(prefName);
  showOncePref += ".show_once";

  bool showOnce = false;
  mPrefBranch->GetBoolPref(showOncePref.get(), &showOnce);

  if (showOnce)
    prefValue = false;

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
    mPrefBranch->SetBoolPref(prefName, false);
  } else if (prefValue && showOnce) {
    mPrefBranch->SetBoolPref(showOncePref.get(), false);
  }

  return rv;
}

