/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Terry Hayes <thayes@netscape.com>
 *  Javier Delgadillo <javi@netscape.com>
 */

/*
 * Dialog services for PIP.
 */
#include <stdarg.h>
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIPrompt.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDialogParamBlock.h"
#include "nsIComponentManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIServiceManager.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "jsapi.h"
#include "nsIStringBundle.h"
#include "nsIPref.h"
#include "nsIInterfaceRequestor.h"
#include "nsIX509Cert.h"
#include "nsILocaleService.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"

#include "nsNSSDialogs.h"

#define PIPSTRING_BUNDLE_URL "chrome://pippki/locale/pippki.properties"
#define STRING_BUNDLE_URL    "chrome://communicator/locale/security.properties"

#define ENTER_SITE_PREF      "security.warn_entering_secure"
#define LEAVE_SITE_PREF      "security.warn_leaving_secure"
#define MIXEDCONTENT_PREF    "security.warn_viewing_mixed"
#define INSECURE_SUBMIT_PREF "security.warn_submit_insecure"

static NS_DEFINE_CID(kCStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);

/**
 * Common class that provides a standard dialog display function, 
 * and uses the hidden window if one can't be determined from
 * context
 */
class nsNSSDialogHelper
{
public:
  const static char *kDefaultOpenWindowParam;
  static nsresult openDialog(
                  nsIDOMWindowInternal *window,
                  const char *url,
                  nsIDialogParamBlock *params);

  static nsresult openDialogVA(nsIDOMWindowInternal *window,
                               const char *format, ...);
};

const char* nsNSSDialogHelper::kDefaultOpenWindowParam = "centerscreen,chrome,modal,titlebar";

static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

static nsresult getHiddenWindow(nsIDOMWindowInternal **outWindow);
static JSContext* getJSContext(nsIDOMWindowInternal *window);

nsresult 
getHiddenWindow(nsIDOMWindowInternal **outWindow)
{
  nsresult rv;

  NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  if (NS_FAILED(rv)) 
    return rv;

  rv = appShell->GetHiddenDOMWindow( outWindow );
  return rv;
}

JSContext* 
getJSContext(nsIDOMWindowInternal *window)
{
  nsresult rv;

  // Get JS context from parent window.
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface( window, &rv );
  if (!sgo) 
    return nsnull;

  nsCOMPtr<nsIScriptContext> context;
  sgo->GetContext( getter_AddRefs( context ) );
  if (!context) 
    return nsnull;

  return (JSContext*)context->GetNativeContext();
}

nsresult
nsNSSDialogHelper::openDialog(
    nsIDOMWindowInternal *window,
    const char *url,
    nsIDialogParamBlock *params)
{
  nsresult rv;
  nsCOMPtr<nsIDOMWindowInternal> hiddenWindow;

  if (!window) {
    rv = getHiddenWindow(getter_AddRefs(hiddenWindow));
    if (NS_FAILED(rv))
      return rv;

    window = hiddenWindow;
  }

  JSContext *jsContext = getJSContext(window);
  if (!jsContext) return NS_ERROR_FAILURE;

  void *stackPtr;
  jsval *argv = JS_PushArguments( jsContext,
                                  &stackPtr,
                                  "sss%ip",
                                  url,
                                  "_blank",
                                  nsNSSDialogHelper::kDefaultOpenWindowParam,
                                  &NS_GET_IID(nsIDialogParamBlock),
                                  (nsISupports*)params
                                );
  if ( !argv ) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindowInternal> newWindow;
  rv = window->OpenDialog( jsContext, argv, 4, getter_AddRefs(newWindow) );
   
  JS_PopArguments( jsContext, stackPtr );

  return rv;
}

nsresult
nsNSSDialogHelper::openDialogVA(nsIDOMWindowInternal *window,
                                const char *format,  ...)
{
  va_list ap;
  nsresult rv;
  nsCOMPtr<nsIDOMWindowInternal> hiddenWindow;

  va_start(ap, format);
  if (!window) {
    rv = getHiddenWindow(getter_AddRefs(hiddenWindow));
    if (NS_FAILED(rv))
      return rv;

    window = hiddenWindow;
  }

  JSContext *jsContext = getJSContext(window);
  if (!jsContext) return NS_ERROR_FAILURE;

  void *stackPtr;
  jsval *argv = JS_PushArgumentsVA(jsContext, &stackPtr, format, ap);

  if ( !argv ) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindowInternal> newWindow;
  rv = window->OpenDialog( jsContext, argv, 4, getter_AddRefs(newWindow) );
   
  JS_PopArguments( jsContext, stackPtr );

  return rv;
}

/* ==== */
static NS_DEFINE_CID(kDialogParamBlockCID, NS_DialogParamBlock_CID);

nsNSSDialogs::nsNSSDialogs()
{
  NS_INIT_ISUPPORTS();
}

nsNSSDialogs::~nsNSSDialogs()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS5(nsNSSDialogs, nsINSSDialogs, 
                                            nsITokenPasswordDialogs,
                                            nsISecurityWarningDialogs,
                                            nsIBadCertListener,
                                            nsICertificateDialogs)

nsresult
nsNSSDialogs::Init()
{
  nsresult rv;

  mPref = do_GetService(kPrefCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStringBundleService> service = do_GetService(kCStringBundleServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = service->CreateBundle(STRING_BUNDLE_URL, nsnull,
                             getter_AddRefs(mStringBundle));
  if (NS_FAILED(rv)) return rv;

  rv = service->CreateBundle(PIPSTRING_BUNDLE_URL, nsnull,
                             getter_AddRefs(mPIPStringBundle));
  return rv;
}

nsresult
nsNSSDialogs::SetPassword(nsIInterfaceRequestor *ctx,
                          const PRUnichar *tokenName, PRBool* _canceled)
{
  nsresult rv;

  *_canceled = PR_FALSE;

  // Get the parent window for the dialog
  nsCOMPtr<nsIDOMWindowInternal> parent = do_GetInterface(ctx);

  nsCOMPtr<nsIDialogParamBlock> block = do_CreateInstance(kDialogParamBlockCID);
  if (!block) return NS_ERROR_FAILURE;

  // void ChangePassword(in wstring tokenName, out int status);
  rv = block->SetString(1, tokenName);
  if (NS_FAILED(rv)) return rv;

  rv = nsNSSDialogHelper::openDialog(parent,
                                "chrome://pippki/content/changepassword.xul",
                                block);

  if (NS_FAILED(rv)) return rv;

  PRInt32 status;

  rv = block->GetInt(1, &status);
  if (NS_FAILED(rv)) return rv;

  *_canceled = (status == 0)?PR_TRUE:PR_FALSE;

  return rv;
}

/* boolean unknownIssuer (in nsITransportSecurityInfo socketInfo,
                          in nsIX509Cert cert, out addType); */
NS_IMETHODIMP
nsNSSDialogs::UnknownIssuer(nsITransportSecurityInfo *socketInfo,
                            nsIX509Cert *cert, PRInt16 *outAddType,
                            PRBool *_retval)
{
  nsresult rv;
  PRInt32 addType;
  
  *_retval = PR_FALSE;

  nsCOMPtr<nsIDialogParamBlock> block = do_CreateInstance(kDialogParamBlockCID);

  if (!block)
    return NS_ERROR_FAILURE;

  nsXPIDLString commonName;
  rv = cert->GetCommonName(getter_Copies(commonName));
  if (NS_FAILED(rv))
    return rv;
  rv = block->SetString(1, commonName);
  if (NS_FAILED(rv))
    return rv;

  rv = nsNSSDialogHelper::openDialog(nsnull, 
                                     "chrome://pippki/content/newserver.xul",
                                     block);

  if (NS_FAILED(rv))
    return rv;

  PRInt32 status;

  rv = block->GetInt(1, &status);
  if (NS_FAILED(rv))
    return rv; 

  if (status == 0) {
    *_retval = PR_FALSE;
  } else {
    // The user wants to continue, let's figure out
    // what to do with this cert. 
    rv = block->GetInt(2, &addType);
    switch (addType) {
      case 0:
        *outAddType = ADD_TRUSTED_PERMANENTLY;
        *_retval    = PR_TRUE;
        break;
      case 1:
        *outAddType = ADD_TRUSTED_FOR_SESSION;
        *_retval    = PR_TRUE;
        break;
      default:
        *outAddType = UNINIT_ADD_FLAG;
        *_retval    = PR_FALSE;
        break;
    } 
  }

  return NS_OK; 
}

/* boolean mismatchDomain (in nsITransportSecurityInfo socketInfo, 
                           in wstring targetURL, 
                           in nsIX509Cert cert); */

NS_IMETHODIMP 
nsNSSDialogs::MismatchDomain(nsITransportSecurityInfo *socketInfo, 
                             const PRUnichar *targetURL, 
                             nsIX509Cert *cert, PRBool *_retval) 
{
  nsresult rv;

  *_retval = PR_FALSE;

  nsCOMPtr<nsIDialogParamBlock> block = do_CreateInstance(kDialogParamBlockCID);

  if (!block)
    return NS_ERROR_FAILURE;

  nsXPIDLString commonName;
  rv = cert->GetCommonName(getter_Copies(commonName));
  if (NS_FAILED(rv))
    return rv;
  rv = block->SetString(1, targetURL);
  if (NS_FAILED(rv))
    return rv;

  block->SetString(2, commonName);
  if (NS_FAILED(rv))
    return rv;

  rv = nsNSSDialogHelper::openDialog(nsnull,
                                 "chrome://pippki/content/domainMismatch.xul",
                                 block);
  if (NS_FAILED(rv))
    return rv;

  PRInt32 status;

  rv = block->GetInt(1, &status);
  if (NS_FAILED(rv))
    return rv;

  *_retval = (status) ? PR_TRUE : PR_FALSE;

  return NS_OK;  
}

/* boolean certExpired (in nsITransportSecurityInfo socketInfo, 
                        in nsIX509Cert cert); */
NS_IMETHODIMP 
nsNSSDialogs::CertExpired(nsITransportSecurityInfo *socketInfo, 
                          nsIX509Cert *cert, PRBool *_retval)
{
  nsresult rv;
  PRTime now = PR_Now();
  PRTime notAfter, notBefore, timeToUse;
  nsCOMPtr<nsIX509CertValidity> validity;
  const char *key;

  *_retval = PR_FALSE;

  nsCOMPtr<nsIDialogParamBlock> block = do_CreateInstance(kDialogParamBlockCID);

  if (!block)
    return NS_ERROR_FAILURE; 
  rv = cert->GetValidity(getter_AddRefs(validity));
  if (NS_FAILED(rv))
    return rv;

  rv = validity->GetNotAfter(&notAfter);
  if (NS_FAILED(rv))
    return rv;

  rv = validity->GetNotBefore(&notBefore);
  if (NS_FAILED(rv))
    return rv;

  if (LL_CMP(now, >, notAfter)) {
    key       = "serverCertExpiredMsg1"; 
    timeToUse = notAfter; 
  } else {
    key = "serverCertNotYetValedMsg1";
    timeToUse = notBefore;
  }

  nsXPIDLString message1;
  PRUnichar *commonName=nsnull;
  nsString formattedDate;

  rv = cert->GetCommonName(&commonName);

  nsIDateTimeFormat *aDateTimeFormat;
  rv = nsComponentManager::CreateInstance(kDateTimeFormatCID, NULL,
                                          NS_GET_IID(nsIDateTimeFormat), 
                                          (void **) &aDateTimeFormat);

  aDateTimeFormat->FormatPRTime(nsnull, kDateFormatShort, 
                                kTimeFormatNoSeconds, timeToUse, 
                                formattedDate);
  PRUnichar *formattedDatePR = formattedDate.ToNewUnicode();
  const PRUnichar *formatStrings[2] = { commonName, formattedDatePR }; 
  nsString keyString = NS_ConvertASCIItoUCS2(key);
  mPIPStringBundle->FormatStringFromName(keyString.GetUnicode(), formatStrings, 
                                         2, getter_Copies(message1));
  
  Recycle(commonName);
  Recycle(formattedDatePR);

  rv = block->SetString(1,message1); 

  if (NS_FAILED(rv))
    return rv;

  rv = nsNSSDialogHelper::openDialog(nsnull,
                             "chrome://pippki/content/serverCertExpired.xul",
                             block);

  PRInt32 status;
  rv = block->GetInt(1, &status);
  if (NS_FAILED(rv))
    return rv; 

  *_retval = (status) ? PR_TRUE : PR_FALSE;
  
  return NS_OK;
}

nsresult
nsNSSDialogs::AlertEnteringSecure(nsIInterfaceRequestor *ctx)
{
  nsresult rv;

  rv = AlertDialog(ctx, ENTER_SITE_PREF, NS_LITERAL_STRING("EnterSiteMessage"));

  return rv;
}


nsresult
nsNSSDialogs::AlertLeavingSecure(nsIInterfaceRequestor *ctx)
{
  nsresult rv;

  rv = AlertDialog(ctx, LEAVE_SITE_PREF, NS_LITERAL_STRING("LeaveSiteMessage"));

  return rv;
}


nsresult
nsNSSDialogs::AlertMixedMode(nsIInterfaceRequestor *ctx)
{
  nsresult rv;

  rv = AlertDialog(ctx, MIXEDCONTENT_PREF, NS_LITERAL_STRING("MixedContentMessage"));

  return rv;
}


nsresult
nsNSSDialogs::AlertDialog(nsIInterfaceRequestor *ctx, const char *prefName,
                          const PRUnichar *dialogMessageName)
{
  nsresult rv;

  // Get user's preference for this alert
  PRBool prefValue;
  rv = mPref->GetBoolPref(prefName, &prefValue);
  if (NS_FAILED(rv)) prefValue = PR_TRUE;

  // Stop if alert is not requested
  if (!prefValue) return NS_OK;

  // Get Prompt to use
  nsCOMPtr<nsIPrompt> prompt = do_GetInterface(ctx);
  if (!prompt) return NS_ERROR_FAILURE;

  // Get messages strings from localization file
  nsXPIDLString windowTitle, message, dontShowAgain;

  mStringBundle->GetStringFromName(NS_LITERAL_STRING("Title"),
                                   getter_Copies(windowTitle));
  mStringBundle->GetStringFromName(dialogMessageName,
                                   getter_Copies(message));
  mStringBundle->GetStringFromName(NS_LITERAL_STRING("DontShowAgain"),
                                   getter_Copies(dontShowAgain));
  if (!windowTitle || !message || !dontShowAgain) return NS_ERROR_FAILURE;
      
  rv = prompt->AlertCheck(windowTitle, message, dontShowAgain, &prefValue);
  if (NS_FAILED(rv)) return rv;
      
  if (!prefValue) {
    mPref->SetBoolPref(prefName, PR_FALSE);
  }

  return rv;
}

nsresult
nsNSSDialogs::ConfirmPostToInsecure(nsIInterfaceRequestor *ctx, PRBool* _result)
{
  nsresult rv;

  rv = ConfirmDialog(ctx, INSECURE_SUBMIT_PREF,
                     NS_LITERAL_STRING("PostToInsecureFromInsecure"), _result);

  return rv;
}

nsresult
nsNSSDialogs::ConfirmPostToInsecureFromSecure(nsIInterfaceRequestor *ctx, PRBool* _result)
{
  nsresult rv;

  rv = ConfirmDialog(ctx, INSECURE_SUBMIT_PREF,
                     NS_LITERAL_STRING("PostToInsecure"), _result);

  return rv;
}

nsresult
nsNSSDialogs::ConfirmDialog(nsIInterfaceRequestor *ctx, const char *prefName,
                            const PRUnichar *messageName, PRBool* _result)
{
  nsresult rv;

  // Get user's preference for this alert
  PRBool prefValue;
  rv = mPref->GetBoolPref(prefName, &prefValue);
  if (NS_FAILED(rv)) prefValue = PR_TRUE;

  // Stop if confirm is not requested
  if (!prefValue) {
    *_result = PR_TRUE;
    return NS_OK;
  }

  // Get Prompt to use
  nsCOMPtr<nsIPrompt> prompt = do_GetInterface(ctx);
  if (!prompt) return NS_ERROR_FAILURE;

  // Get messages strings from localization file
  nsXPIDLString windowTitle, message, dontShowAgain;

  mStringBundle->GetStringFromName(NS_LITERAL_STRING("Title"),
                                   getter_Copies(windowTitle));
  mStringBundle->GetStringFromName(messageName,
                                   getter_Copies(message));
  mStringBundle->GetStringFromName(NS_LITERAL_STRING("DontShowAgain"),
                                   getter_Copies(dontShowAgain));
  if (!windowTitle || !message || !dontShowAgain) return NS_ERROR_FAILURE;
      
  rv = prompt->ConfirmCheck(windowTitle, message, dontShowAgain, &prefValue, _result);
  if (NS_FAILED(rv)) return rv;
      
  if (!prefValue) {
    mPref->SetBoolPref(prefName, PR_FALSE);
  }

  return rv;
}

/* void downloadCACert (in nsIInterfaceRequestor ctx,
                        in nsIX509Cert cert,
                        out trust,
                        out canceled); */
NS_IMETHODIMP 
nsNSSDialogs::DownloadCACert(nsIInterfaceRequestor *ctx, 
                             nsIX509Cert *cert,
                             PRUint32 *_trust,
                             PRBool *_canceled)
{
  nsresult rv;

  *_canceled = PR_FALSE;

  // Get the parent window for the dialog
  nsCOMPtr<nsIDOMWindowInternal> parent = do_GetInterface(ctx);

  nsCOMPtr<nsIDialogParamBlock> block = do_CreateInstance(kDialogParamBlockCID);
  if (!block) return NS_ERROR_FAILURE;

  nsXPIDLString commonName;
  rv = cert->GetCommonName(getter_Copies(commonName));
  if (NS_FAILED(rv))
    return rv;
  rv = block->SetString(1, commonName);
  if (NS_FAILED(rv)) return rv;

  rv = nsNSSDialogHelper::openDialog(parent, 
                                   "chrome://pippki/content/downloadcert.xul",
                                     block);
  if (NS_FAILED(rv)) return rv;

  PRInt32 status;
  PRInt32 ssl, email, objsign;

  rv = block->GetInt(1, &status);
  if (NS_FAILED(rv)) return rv;
  rv = block->GetInt(2, &ssl);
  if (NS_FAILED(rv)) return rv;
  rv = block->GetInt(3, &email);
  if (NS_FAILED(rv)) return rv;
  rv = block->GetInt(4, &objsign);
  if (NS_FAILED(rv)) return rv;
 
  *_trust = nsIX509Cert::UNTRUSTED;
  *_trust |= (ssl) ? nsIX509Cert::TRUSTED_SSL : 0;
  *_trust |= (email) ? nsIX509Cert::TRUSTED_EMAIL : 0;
  *_trust |= (objsign) ? nsIX509Cert::TRUSTED_OBJSIGN : 0;

  *_canceled = (status == 0)?PR_TRUE:PR_FALSE;

  return rv;
}

