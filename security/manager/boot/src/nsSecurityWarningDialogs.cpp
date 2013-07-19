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

#include "mozilla/Telemetry.h"
#include "nsISecurityUITelemetry.h"

NS_IMPL_ISUPPORTS1(nsSecurityWarningDialogs, nsISecurityWarningDialogs)

#define STRING_BUNDLE_URL    "chrome://pipnss/locale/security.properties"

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
nsSecurityWarningDialogs::ConfirmPostToInsecureFromSecure(nsIInterfaceRequestor *ctx, bool* _result)
{
  nsresult rv;

  // The Telemetry clickthrough constant is 1 more than the constant for the dialog.
  rv = ConfirmDialog(ctx, nullptr, // No preference for this one - it's too important
                     NS_LITERAL_STRING("PostToInsecureFromSecureMessage").get(),
                     nullptr,
                     nsISecurityUITelemetry::WARNING_CONFIRM_POST_TO_INSECURE_FROM_SECURE,
                     _result);

  return rv;
}

nsresult
nsSecurityWarningDialogs::ConfirmDialog(nsIInterfaceRequestor *ctx, const char *prefName,
                            const PRUnichar *messageName, 
                            const PRUnichar *showAgainName, 
                            const uint32_t aBucket,
                            bool* _result)
{
  nsresult rv;

  // Get user's preference for this alert
  // prefName, showAgainName are null if there is no preference for this dialog
  bool prefValue = true;
  
  if (prefName) {
    rv = mPrefBranch->GetBoolPref(prefName, &prefValue);
    if (NS_FAILED(rv)) prefValue = true;
  }
  
  // Stop if confirm is not requested
  if (!prefValue) {
    *_result = true;
    return NS_OK;
  }
  
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::Telemetry::Accumulate(mozilla::Telemetry::SECURITY_UI, aBucket);
  // See AlertDialog() for a description of how showOnce works.
  nsAutoCString showOncePref(prefName);
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
  if (showAgainName) {
    mStringBundle->GetStringFromName(showAgainName,
                                     getter_Copies(alertMe));
  }
  mStringBundle->GetStringFromName(NS_LITERAL_STRING("Continue").get(),
                                   getter_Copies(cont));
  // alertMe is allowed to be null
  if (!windowTitle || !message || !cont) return NS_ERROR_FAILURE;
      
  // Replace # characters with newlines to lay out the dialog.
  PRUnichar* msgchars = message.BeginWriting();
  
  uint32_t i = 0;
  for (i = 0; msgchars[i] != '\0'; i++) {
    if (msgchars[i] == '#') {
      msgchars[i] = '\n';
    }
  }  

  int32_t buttonPressed;

  rv  = prompt->ConfirmEx(windowTitle, 
                          message, 
                          (nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_0) +
                          (nsIPrompt::BUTTON_TITLE_CANCEL * nsIPrompt::BUTTON_POS_1),
                          cont,
                          nullptr,
                          nullptr,
                          alertMe, 
                          &prefValue, 
                          &buttonPressed);

  if (NS_FAILED(rv)) return rv;

  *_result = (buttonPressed != 1);
  if (*_result) {
  // For confirmation dialogs, the clickthrough constant is 1 more
  // than the constant for the dialog.
  mozilla::Telemetry::Accumulate(mozilla::Telemetry::SECURITY_UI, aBucket + 1);
  }

  if (!prefValue && prefName) {
    mPrefBranch->SetBoolPref(prefName, false);
  } else if (prefValue && showOnce) {
    mPrefBranch->SetBoolPref(showOncePref.get(), false);
  }

  return rv;
}

