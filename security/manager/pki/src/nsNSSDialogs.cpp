/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Dialog services for PIP.
 */
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIDOMWindow.h"
#include "nsIDialogParamBlock.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsPromiseFlatString.h"

#include "nsNSSDialogs.h"
#include "nsPKIParamBlock.h"
#include "nsIKeygenThread.h"
#include "nsIProtectedAuthThread.h"
#include "nsNSSDialogHelper.h"
#include "nsIWindowWatcher.h"
#include "nsIX509CertValidity.h"
#include "nsICRLInfo.h"

#define PIPSTRING_BUNDLE_URL "chrome://pippki/locale/pippki.properties"

/* ==== */

nsNSSDialogs::nsNSSDialogs()
{
}

nsNSSDialogs::~nsNSSDialogs()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS8(nsNSSDialogs, nsITokenPasswordDialogs,
                                            nsICertificateDialogs,
                                            nsIClientAuthDialogs,
                                            nsICertPickDialogs,
                                            nsITokenDialogs,
                                            nsIDOMCryptoDialogs,
                                            nsIGeneratingKeypairInfoDialogs,
                                            nsISSLCertErrorDialog)

nsresult
nsNSSDialogs::Init()
{
  nsresult rv;

  nsCOMPtr<nsIStringBundleService> service =
           do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = service->CreateBundle(PIPSTRING_BUNDLE_URL,
                             getter_AddRefs(mPIPStringBundle));
  return rv;
}

nsresult
nsNSSDialogs::SetPassword(nsIInterfaceRequestor *ctx,
                          const PRUnichar *tokenName, bool* _canceled)
{
  nsresult rv;

  *_canceled = false;

  // Get the parent window for the dialog
  nsCOMPtr<nsIDOMWindow> parent = do_GetInterface(ctx);

  nsCOMPtr<nsIDialogParamBlock> block =
           do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
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

  *_canceled = (status == 0)?true:false;

  return rv;
}

nsresult
nsNSSDialogs::GetPassword(nsIInterfaceRequestor *ctx,
                          const PRUnichar *tokenName, 
                          PRUnichar **_password,
                          bool* _canceled)
{
  nsresult rv;
  *_canceled = false;
  // Get the parent window for the dialog
  nsCOMPtr<nsIDOMWindow> parent = do_GetInterface(ctx);
  nsCOMPtr<nsIDialogParamBlock> block = 
           do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  if (!block) return NS_ERROR_FAILURE;
  // Set the token name in the window
  rv = block->SetString(1, tokenName);
  if (NS_FAILED(rv)) return rv;
  // open up the window
  rv = nsNSSDialogHelper::openDialog(parent,
                                     "chrome://pippki/content/getpassword.xul",
                                     block);
  if (NS_FAILED(rv)) return rv;
  // see if user canceled
  PRInt32 status;
  rv = block->GetInt(1, &status);
  if (NS_FAILED(rv)) return rv;
  *_canceled = (status == 0) ? true : false;
  if (!*_canceled) {
    // retrieve the password
    rv = block->GetString(2, _password);
  }
  return rv;
}

NS_IMETHODIMP 
nsNSSDialogs::CrlImportStatusDialog(nsIInterfaceRequestor *ctx, nsICRLInfo *crl)
{
  nsresult rv;

  nsCOMPtr<nsIPKIParamBlock> block =
           do_CreateInstance(NS_PKIPARAMBLOCK_CONTRACTID,&rv);
  if (NS_FAILED(rv))
    return rv;
  
  rv = block->SetISupportAtIndex(1, crl);
  if (NS_FAILED(rv))
    return rv;

  rv = nsNSSDialogHelper::openDialog(nullptr,
                             "chrome://pippki/content/crlImportDialog.xul",
                             block,
                             false);
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSDialogs::ConfirmDownloadCACert(nsIInterfaceRequestor *ctx, 
                                    nsIX509Cert *cert,
                                    PRUint32 *_trust,
                                    bool *_retval)
{
  nsresult rv;

  *_retval = true;

  // Get the parent window for the dialog
  nsCOMPtr<nsIDOMWindow> parent = do_GetInterface(ctx);

  nsCOMPtr<nsIPKIParamBlock> block =
           do_CreateInstance(NS_PKIPARAMBLOCK_CONTRACTID);
  if (!block)
    return NS_ERROR_FAILURE;

  rv = block->SetISupportAtIndex(1, cert);
  if (NS_FAILED(rv))
    return rv;

  rv = nsNSSDialogHelper::openDialog(parent, 
                                     "chrome://pippki/content/downloadcert.xul",
                                     block);
  if (NS_FAILED(rv)) return rv;

  PRInt32 status;
  PRInt32 ssl, email, objsign;

  nsCOMPtr<nsIDialogParamBlock> dlgParamBlock = do_QueryInterface(block);
  
  rv = dlgParamBlock->GetInt(1, &status);
  if (NS_FAILED(rv)) return rv;
  rv = dlgParamBlock->GetInt(2, &ssl);
  if (NS_FAILED(rv)) return rv;
  rv = dlgParamBlock->GetInt(3, &email);
  if (NS_FAILED(rv)) return rv;
  rv = dlgParamBlock->GetInt(4, &objsign);
  if (NS_FAILED(rv)) return rv;
 
  *_trust = nsIX509CertDB::UNTRUSTED;
  *_trust |= (ssl) ? nsIX509CertDB::TRUSTED_SSL : 0;
  *_trust |= (email) ? nsIX509CertDB::TRUSTED_EMAIL : 0;
  *_trust |= (objsign) ? nsIX509CertDB::TRUSTED_OBJSIGN : 0;

  *_retval = (status == 0)?false:true;

  return rv;
}


NS_IMETHODIMP 
nsNSSDialogs::NotifyCACertExists(nsIInterfaceRequestor *ctx)
{
  nsresult rv;

  // Get the parent window for the dialog
  nsCOMPtr<nsIDOMWindow> parent = do_GetInterface(ctx);

  nsCOMPtr<nsIDialogParamBlock> block =
           do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  if (!block) return NS_ERROR_FAILURE;

  
  rv = nsNSSDialogHelper::openDialog(parent, 
                                     "chrome://pippki/content/cacertexists.xul",
                                     block);

  return rv;
}


NS_IMETHODIMP
nsNSSDialogs::ChooseCertificate(nsIInterfaceRequestor *ctx, const PRUnichar *cn, const PRUnichar *organization, const PRUnichar *issuer, const PRUnichar **certNickList, const PRUnichar **certDetailsList, PRUint32 count, PRInt32 *selectedIndex, bool *canceled) 
{
  nsresult rv;
  PRUint32 i;

  *canceled = false;

  // Get the parent window for the dialog
  nsCOMPtr<nsIDOMWindow> parent = do_GetInterface(ctx);

  nsCOMPtr<nsIDialogParamBlock> block =
           do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  if (!block) return NS_ERROR_FAILURE;

  block->SetNumberStrings(4+count*2);

  rv = block->SetString(0, cn);
  if (NS_FAILED(rv)) return rv;

  rv = block->SetString(1, organization);
  if (NS_FAILED(rv)) return rv;

  rv = block->SetString(2, issuer);
  if (NS_FAILED(rv)) return rv;

  for (i = 0; i < count; i++) {
    rv = block->SetString(i+3, certNickList[i]);
    if (NS_FAILED(rv)) return rv;
  }

  for (i = 0; i < count; i++) {
    rv = block->SetString(i+count+3, certDetailsList[i]);
    if (NS_FAILED(rv)) return rv;
  }

  rv = block->SetInt(0, count);
  if (NS_FAILED(rv)) return rv;

  rv = nsNSSDialogHelper::openDialog(nullptr,
                                "chrome://pippki/content/clientauthask.xul",
                                block);
  if (NS_FAILED(rv)) return rv;

  PRInt32 status;
  rv = block->GetInt(0, &status);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIClientAuthUserDecision> extraResult = do_QueryInterface(ctx);
  if (extraResult) {
    PRInt32 rememberSelection;
    rv = block->GetInt(2, &rememberSelection);
    if (NS_SUCCEEDED(rv)) {
      extraResult->SetRememberClientAuthCertificate(rememberSelection!=0);
    }
  }

  *canceled = (status == 0)?true:false;
  if (!*canceled) {
    // retrieve the nickname
    rv = block->GetInt(1, selectedIndex);
  }
  return rv;
}


NS_IMETHODIMP
nsNSSDialogs::PickCertificate(nsIInterfaceRequestor *ctx, 
                              const PRUnichar **certNickList, 
                              const PRUnichar **certDetailsList, 
                              PRUint32 count, 
                              PRInt32 *selectedIndex, 
                              bool *canceled) 
{
  nsresult rv;
  PRUint32 i;

  *canceled = false;

  // Get the parent window for the dialog
  nsCOMPtr<nsIDOMWindow> parent = do_GetInterface(ctx);

  nsCOMPtr<nsIDialogParamBlock> block =
           do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  if (!block) return NS_ERROR_FAILURE;

  block->SetNumberStrings(1+count*2);

  for (i = 0; i < count; i++) {
    rv = block->SetString(i, certNickList[i]);
    if (NS_FAILED(rv)) return rv;
  }

  for (i = 0; i < count; i++) {
    rv = block->SetString(i+count, certDetailsList[i]);
    if (NS_FAILED(rv)) return rv;
  }

  rv = block->SetInt(0, count);
  if (NS_FAILED(rv)) return rv;

  rv = block->SetInt(1, *selectedIndex);
  if (NS_FAILED(rv)) return rv;

  rv = nsNSSDialogHelper::openDialog(nullptr,
                                "chrome://pippki/content/certpicker.xul",
                                block);
  if (NS_FAILED(rv)) return rv;

  PRInt32 status;

  rv = block->GetInt(0, &status);
  if (NS_FAILED(rv)) return rv;

  *canceled = (status == 0)?true:false;
  if (!*canceled) {
    rv = block->GetInt(1, selectedIndex);
  }
  return rv;
}


NS_IMETHODIMP 
nsNSSDialogs::SetPKCS12FilePassword(nsIInterfaceRequestor *ctx, 
                                    nsAString &_password,
                                    bool *_retval)
{
  nsresult rv;
  *_retval = true;
  // Get the parent window for the dialog
  nsCOMPtr<nsIDOMWindow> parent = do_GetInterface(ctx);
  nsCOMPtr<nsIDialogParamBlock> block =
           do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  if (!block) return NS_ERROR_FAILURE;
  // open up the window
  rv = nsNSSDialogHelper::openDialog(parent,
                                  "chrome://pippki/content/setp12password.xul",
                                  block);
  if (NS_FAILED(rv)) return rv;
  // see if user canceled
  PRInt32 status;
  rv = block->GetInt(1, &status);
  if (NS_FAILED(rv)) return rv;
  *_retval = (status == 0) ? false : true;
  if (*_retval) {
    // retrieve the password
    PRUnichar *pw;
    rv = block->GetString(2, &pw);
    if (NS_SUCCEEDED(rv)) {
      _password = pw;
      nsMemory::Free(pw);
    }
  }
  return rv;
}

NS_IMETHODIMP 
nsNSSDialogs::GetPKCS12FilePassword(nsIInterfaceRequestor *ctx, 
                                    nsAString &_password,
                                    bool *_retval)
{
  nsresult rv;
  *_retval = true;
  // Get the parent window for the dialog
  nsCOMPtr<nsIDOMWindow> parent = do_GetInterface(ctx);
  nsCOMPtr<nsIDialogParamBlock> block =
           do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  if (!block) return NS_ERROR_FAILURE;
  // open up the window
  rv = nsNSSDialogHelper::openDialog(parent,
                                  "chrome://pippki/content/getp12password.xul",
                                  block);
  if (NS_FAILED(rv)) return rv;
  // see if user canceled
  PRInt32 status;
  rv = block->GetInt(1, &status);
  if (NS_FAILED(rv)) return rv;
  *_retval = (status == 0) ? false : true;
  if (*_retval) {
    // retrieve the password
    PRUnichar *pw;
    rv = block->GetString(2, &pw);
    if (NS_SUCCEEDED(rv)) {
      _password = pw;
      nsMemory::Free(pw);
    }
  }
  return rv;
}

/* void viewCert (in nsIX509Cert cert); */
NS_IMETHODIMP 
nsNSSDialogs::ViewCert(nsIInterfaceRequestor *ctx, 
                       nsIX509Cert *cert)
{
  nsresult rv;

  nsCOMPtr<nsIPKIParamBlock> block =
           do_CreateInstance(NS_PKIPARAMBLOCK_CONTRACTID);
  if (!block)
    return NS_ERROR_FAILURE;

  rv = block->SetISupportAtIndex(1, cert);
  if (NS_FAILED(rv))
    return rv;

  // Get the parent window for the dialog
  nsCOMPtr<nsIDOMWindow> parent = do_GetInterface(ctx);

  rv = nsNSSDialogHelper::openDialog(parent,
                                     "chrome://pippki/content/certViewer.xul",
                                     block,
                                     false);
  return rv;
}

NS_IMETHODIMP
nsNSSDialogs::DisplayGeneratingKeypairInfo(nsIInterfaceRequestor *aCtx, nsIKeygenThread *runnable) 
{
  nsresult rv;

  // Get the parent window for the dialog
  nsCOMPtr<nsIDOMWindow> parent = do_GetInterface(aCtx);

  rv = nsNSSDialogHelper::openDialog(parent,
                                     "chrome://pippki/content/createCertInfo.xul",
                                     runnable);
  return rv;
}

NS_IMETHODIMP
nsNSSDialogs::ChooseToken(nsIInterfaceRequestor *aCtx, const PRUnichar **aTokenList, PRUint32 aCount, PRUnichar **aTokenChosen, bool *aCanceled) {
  nsresult rv;
  PRUint32 i;

  *aCanceled = false;

  // Get the parent window for the dialog
  nsCOMPtr<nsIDOMWindow> parent = do_GetInterface(aCtx);

  nsCOMPtr<nsIDialogParamBlock> block =
           do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  if (!block) return NS_ERROR_FAILURE;

  block->SetNumberStrings(aCount);

  for (i = 0; i < aCount; i++) {
    rv = block->SetString(i, aTokenList[i]);
    if (NS_FAILED(rv)) return rv;
  }

  rv = block->SetInt(0, aCount);
  if (NS_FAILED(rv)) return rv;

  rv = nsNSSDialogHelper::openDialog(nullptr,
                                "chrome://pippki/content/choosetoken.xul",
                                block);
  if (NS_FAILED(rv)) return rv;

  PRInt32 status;

  rv = block->GetInt(0, &status);
  if (NS_FAILED(rv)) return rv;

  *aCanceled = (status == 0)?true:false;
  if (!*aCanceled) {
    // retrieve the nickname
    rv = block->GetString(0, aTokenChosen);
  }
  return rv;
}

/* boolean ConfirmKeyEscrow (in nsIX509Cert escrowAuthority); */
NS_IMETHODIMP 
nsNSSDialogs::ConfirmKeyEscrow(nsIX509Cert *escrowAuthority, bool *_retval)
                                     
{
  *_retval = false;

  nsresult rv;

  nsCOMPtr<nsIPKIParamBlock> block =
           do_CreateInstance(NS_PKIPARAMBLOCK_CONTRACTID);
  if (!block)
    return NS_ERROR_FAILURE;

  rv = block->SetISupportAtIndex(1, escrowAuthority);
  if (NS_FAILED(rv))
    return rv;

  rv = nsNSSDialogHelper::openDialog(nullptr,
                                     "chrome://pippki/content/escrowWarn.xul",
                                     block);

  if (NS_FAILED(rv))
    return rv;

  PRInt32 status=0;
  nsCOMPtr<nsIDialogParamBlock> dlgParamBlock = do_QueryInterface(block);
  rv = dlgParamBlock->GetInt(1, &status);
 
  if (status) {
    *_retval = true;
  } 
  return rv;
}

NS_IMETHODIMP
nsNSSDialogs::DisplayProtectedAuth(nsIInterfaceRequestor *aCtx, nsIProtectedAuthThread *runnable)
{
    // We cannot use nsNSSDialogHelper here. We cannot allow close widget
    // in the window because protected authentication is interruptible
    // from user interface and changing nsNSSDialogHelper's static variable
    // would not be thread-safe
    
    nsresult rv = NS_ERROR_FAILURE;
    
    // Get the parent window for the dialog
    nsCOMPtr<nsIDOMWindow> parent = do_GetInterface(aCtx);
    
    nsCOMPtr<nsIWindowWatcher> windowWatcher = 
        do_GetService("@mozilla.org/embedcomp/window-watcher;1", &rv);
    if (NS_FAILED(rv))
        return rv;
    
    if (!parent) {
        windowWatcher->GetActiveWindow(getter_AddRefs(parent));
    }
    
    nsCOMPtr<nsIDOMWindow> newWindow;
    rv = windowWatcher->OpenWindow(parent,
        "chrome://pippki/content/protectedAuth.xul",
        "_blank",
        "centerscreen,chrome,modal,titlebar,close=no",
        runnable,
        getter_AddRefs(newWindow));
    
    return rv;
}

NS_IMETHODIMP
nsNSSDialogs::ShowCertError(nsIInterfaceRequestor *ctx, 
                            nsISSLStatus *status, 
                            nsIX509Cert *cert, 
                            const nsAString & textErrorMessage, 
                            const nsAString & htmlErrorMessage, 
                            const nsACString & hostName, 
                            PRUint32 portNumber)
{
  nsCOMPtr<nsIPKIParamBlock> block =
           do_CreateInstance(NS_PKIPARAMBLOCK_CONTRACTID);
  if (!block)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIDialogParamBlock> dialogBlock = do_QueryInterface(block);

  nsresult rv;
  rv = dialogBlock->SetInt(1, portNumber);
  if (NS_FAILED(rv))
    return rv; 

  rv = dialogBlock->SetString(1, NS_ConvertUTF8toUTF16(hostName).get());
  if (NS_FAILED(rv))
    return rv;
  
  rv = dialogBlock->SetString(2, PromiseFlatString(textErrorMessage).get());
  if (NS_FAILED(rv))
    return rv;
  
  rv = block->SetISupportAtIndex(1, cert);
  if (NS_FAILED(rv))
    return rv;

  rv = nsNSSDialogHelper::openDialog(nullptr, 
                                     "chrome://pippki/content/certerror.xul",
                                     block);
  return rv;
}
