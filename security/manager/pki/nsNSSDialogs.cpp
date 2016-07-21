/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Dialog services for PIP.
 */
#include "mozIDOMWindow.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "nsArray.h"
#include "nsDateTimeFormatCID.h"
#include "nsEmbedCID.h"
#include "nsIComponentManager.h"
#include "nsIDateTimeFormat.h"
#include "nsIDialogParamBlock.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIKeygenThread.h"
#include "nsIPromptService.h"
#include "nsIProtectedAuthThread.h"
#include "nsIServiceManager.h"
#include "nsIWindowWatcher.h"
#include "nsIX509CertDB.h"
#include "nsIX509Cert.h"
#include "nsIX509CertValidity.h"
#include "nsNSSDialogHelper.h"
#include "nsNSSDialogs.h"
#include "nsPromiseFlatString.h"
#include "nsReadableUtils.h"
#include "nsString.h"

#define PIPSTRING_BUNDLE_URL "chrome://pippki/locale/pippki.properties"

/* ==== */

nsNSSDialogs::nsNSSDialogs()
{
}

nsNSSDialogs::~nsNSSDialogs()
{
}

NS_IMPL_ISUPPORTS(nsNSSDialogs, nsITokenPasswordDialogs,
                  nsICertificateDialogs,
                  nsIClientAuthDialogs,
                  nsICertPickDialogs,
                  nsITokenDialogs,
                  nsIGeneratingKeypairInfoDialogs)

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
                          const char16_t *tokenName, bool* _canceled)
{
  nsresult rv;

  *_canceled = false;

  // Get the parent window for the dialog
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(ctx);

  nsCOMPtr<nsIDialogParamBlock> block =
           do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  if (!block) return NS_ERROR_FAILURE;

  rv = block->SetString(1, tokenName);
  if (NS_FAILED(rv)) return rv;

  rv = nsNSSDialogHelper::openDialog(parent,
                                "chrome://pippki/content/changepassword.xul",
                                block);

  if (NS_FAILED(rv)) return rv;

  int32_t status;

  rv = block->GetInt(1, &status);
  if (NS_FAILED(rv)) return rv;

  *_canceled = (status == 0)?true:false;

  return rv;
}

NS_IMETHODIMP 
nsNSSDialogs::ConfirmDownloadCACert(nsIInterfaceRequestor *ctx, 
                                    nsIX509Cert *cert,
                                    uint32_t *_trust,
                                    bool *_retval)
{
  nsresult rv;

  nsCOMPtr<nsIMutableArray> dlgArray = nsArrayBase::Create();
  if (!dlgArray) {
    return NS_ERROR_FAILURE;
  }
  rv = dlgArray->AppendElement(cert, false);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIDialogParamBlock> dlgParamBlock(
    do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID));
  if (!dlgParamBlock) {
    return NS_ERROR_FAILURE;
  }
  rv = dlgParamBlock->SetObjects(dlgArray);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the parent window for the dialog
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(ctx);
  rv = nsNSSDialogHelper::openDialog(parent, 
                                     "chrome://pippki/content/downloadcert.xul",
                                     dlgParamBlock);
  if (NS_FAILED(rv)) {
    return rv;
  }

  int32_t status;
  int32_t ssl, email, objsign;

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

  *_retval = (status != 0);

  return rv;
}

NS_IMETHODIMP
nsNSSDialogs::ChooseCertificate(nsIInterfaceRequestor* ctx,
                                const nsAString& cnAndPort,
                                const nsAString& organization,
                                const nsAString& issuerOrg,
                                nsIArray* certList,
                        /*out*/ uint32_t* selectedIndex,
                        /*out*/ bool* certificateChosen)
{
  NS_ENSURE_ARG_POINTER(ctx);
  NS_ENSURE_ARG_POINTER(certList);
  NS_ENSURE_ARG_POINTER(selectedIndex);
  NS_ENSURE_ARG_POINTER(certificateChosen);

  *certificateChosen = false;

  nsCOMPtr<nsIDialogParamBlock> block =
    do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  if (!block) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIMutableArray> paramBlockArray = nsArrayBase::Create();
  if (!paramBlockArray) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = paramBlockArray->AppendElement(certList, false);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = block->SetObjects(paramBlockArray);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = block->SetNumberStrings(3);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = block->SetString(0, PromiseFlatString(cnAndPort).get());
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = block->SetString(1, PromiseFlatString(organization).get());
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = block->SetString(2, PromiseFlatString(issuerOrg).get());
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = nsNSSDialogHelper::openDialog(nullptr,
                                     "chrome://pippki/content/clientauthask.xul",
                                     block);
  if (NS_FAILED(rv)) {
    return rv;
  }

  int32_t status;
  rv = block->GetInt(0, &status);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIClientAuthUserDecision> extraResult = do_QueryInterface(ctx);
  if (extraResult) {
    int32_t rememberSelection;
    rv = block->GetInt(2, &rememberSelection);
    if (NS_SUCCEEDED(rv)) {
      extraResult->SetRememberClientAuthCertificate(rememberSelection!=0);
    }
  }

  *certificateChosen = (status != 0);
  if (*certificateChosen) {
    int32_t index = 0;
    rv = block->GetInt(1, &index);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (index < 0) {
      MOZ_ASSERT_UNREACHABLE("Selected index should never be negative");
      return NS_ERROR_FAILURE;
    }

    *selectedIndex = mozilla::AssertedCast<uint32_t>(index);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNSSDialogs::PickCertificate(nsIInterfaceRequestor *ctx, 
                              const char16_t **certNickList, 
                              const char16_t **certDetailsList, 
                              uint32_t count, 
                              int32_t *selectedIndex, 
                              bool *canceled) 
{
  nsresult rv;
  uint32_t i;

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

  int32_t status;

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
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(ctx);
  nsCOMPtr<nsIDialogParamBlock> block =
           do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  if (!block) return NS_ERROR_FAILURE;
  // open up the window
  rv = nsNSSDialogHelper::openDialog(parent,
                                  "chrome://pippki/content/setp12password.xul",
                                  block);
  if (NS_FAILED(rv)) return rv;
  // see if user canceled
  int32_t status;
  rv = block->GetInt(1, &status);
  if (NS_FAILED(rv)) return rv;
  *_retval = (status == 0) ? false : true;
  if (*_retval) {
    // retrieve the password
    char16_t *pw;
    rv = block->GetString(2, &pw);
    if (NS_SUCCEEDED(rv)) {
      _password = pw;
      free(pw);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsNSSDialogs::GetPKCS12FilePassword(nsIInterfaceRequestor* ctx,
                                    nsAString& _password,
                                    bool* _retval)
{
  *_retval = false;

  nsCOMPtr<nsIPromptService> promptSvc(
    do_GetService(NS_PROMPTSERVICE_CONTRACTID));
  if (!promptSvc) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString msg;
  nsresult rv = mPIPStringBundle->GetStringFromName(
    u"getPKCS12FilePasswordMessage", getter_Copies(msg));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the parent window for the dialog
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(ctx);
  bool ignored = false;
  char16_t* pwTemp = nullptr;
  rv = promptSvc->PromptPassword(parent, nullptr, msg.get(), &pwTemp, nullptr,
                                 &ignored, _retval);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (*_retval) {
    _password.Assign(pwTemp);
    free(pwTemp);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsNSSDialogs::ViewCert(nsIInterfaceRequestor* ctx, nsIX509Cert* cert)
{
  nsCOMPtr<nsIMutableArray> dlgArray = nsArrayBase::Create();
  if (!dlgArray) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = dlgArray->AppendElement(cert, false);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIDialogParamBlock> dlgParamBlock(
    do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID));
  if (!dlgParamBlock) {
    return NS_ERROR_FAILURE;
  }
  rv = dlgParamBlock->SetObjects(dlgArray);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the parent window for the dialog
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(ctx);
  return nsNSSDialogHelper::openDialog(parent,
                                       "chrome://pippki/content/certViewer.xul",
                                       dlgParamBlock,
                                       false);
}

NS_IMETHODIMP
nsNSSDialogs::DisplayGeneratingKeypairInfo(nsIInterfaceRequestor *aCtx, nsIKeygenThread *runnable) 
{
  nsresult rv;

  // Get the parent window for the dialog
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(aCtx);

  rv = nsNSSDialogHelper::openDialog(parent,
                                     "chrome://pippki/content/createCertInfo.xul",
                                     runnable);
  return rv;
}

NS_IMETHODIMP
nsNSSDialogs::ChooseToken(nsIInterfaceRequestor *aCtx, const char16_t **aTokenList, uint32_t aCount, char16_t **aTokenChosen, bool *aCanceled) {
  nsresult rv;
  uint32_t i;

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

  int32_t status;

  rv = block->GetInt(0, &status);
  if (NS_FAILED(rv)) return rv;

  *aCanceled = (status == 0)?true:false;
  if (!*aCanceled) {
    // retrieve the nickname
    rv = block->GetString(0, aTokenChosen);
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
    nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(aCtx);
    
    nsCOMPtr<nsIWindowWatcher> windowWatcher = 
        do_GetService("@mozilla.org/embedcomp/window-watcher;1", &rv);
    if (NS_FAILED(rv))
        return rv;
    
    if (!parent) {
        windowWatcher->GetActiveWindow(getter_AddRefs(parent));
    }
    
    nsCOMPtr<mozIDOMWindowProxy> newWindow;
    rv = windowWatcher->OpenWindow(parent,
        "chrome://pippki/content/protectedAuth.xul",
        "_blank",
        "centerscreen,chrome,modal,titlebar,close=no",
        runnable,
        getter_AddRefs(newWindow));
    
    return rv;
}
