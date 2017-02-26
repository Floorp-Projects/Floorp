/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Dialog services for PIP.
 */

#include "nsNSSDialogs.h"

#include "mozIDOMWindow.h"
#include "nsArray.h"
#include "nsEmbedCID.h"
#include "nsHashPropertyBag.h"
#include "nsIDialogParamBlock.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIKeygenThread.h"
#include "nsIPromptService.h"
#include "nsIProtectedAuthThread.h"
#include "nsIWindowWatcher.h"
#include "nsIX509CertDB.h"
#include "nsIX509Cert.h"
#include "nsNSSDialogHelper.h"
#include "nsPromiseFlatString.h"
#include "nsString.h"
#include "nsVariant.h"

#define PIPSTRING_BUNDLE_URL "chrome://pippki/locale/pippki.properties"

nsNSSDialogs::nsNSSDialogs()
{
}

nsNSSDialogs::~nsNSSDialogs()
{
}

NS_IMPL_ISUPPORTS(nsNSSDialogs, nsITokenPasswordDialogs,
                  nsICertificateDialogs,
                  nsIClientAuthDialogs,
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

NS_IMETHODIMP
nsNSSDialogs::SetPassword(nsIInterfaceRequestor* ctx,
                          const nsAString& tokenName,
                  /*out*/ bool* canceled)
{
  // |ctx| is allowed to be null.
  NS_ENSURE_ARG(canceled);

  *canceled = false;

  // Get the parent window for the dialog
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(ctx);

  nsCOMPtr<nsIDialogParamBlock> block =
           do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  if (!block) return NS_ERROR_FAILURE;

  nsresult rv = block->SetString(1, PromiseFlatString(tokenName).get());
  if (NS_FAILED(rv)) return rv;

  rv = nsNSSDialogHelper::openDialog(parent,
                                "chrome://pippki/content/changepassword.xul",
                                block);

  if (NS_FAILED(rv)) return rv;

  int32_t status;

  rv = block->GetInt(1, &status);
  if (NS_FAILED(rv)) return rv;

  *canceled = (status == 0);

  return rv;
}

NS_IMETHODIMP
nsNSSDialogs::ConfirmDownloadCACert(nsIInterfaceRequestor* ctx,
                                    nsIX509Cert* cert,
                            /*out*/ uint32_t* trust,
                            /*out*/ bool* importConfirmed)
{
  // |ctx| is allowed to be null.
  NS_ENSURE_ARG(cert);
  NS_ENSURE_ARG(trust);
  NS_ENSURE_ARG(importConfirmed);

  nsCOMPtr<nsIMutableArray> argArray = nsArrayBase::Create();
  if (!argArray) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = argArray->AppendElement(cert, false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIWritablePropertyBag2> retVals = new nsHashPropertyBag();
  rv = argArray->AppendElement(retVals, false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the parent window for the dialog
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(ctx);
  rv = nsNSSDialogHelper::openDialog(parent,
                                     "chrome://pippki/content/downloadcert.xul",
                                     argArray);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = retVals->GetPropertyAsBool(NS_LITERAL_STRING("importConfirmed"),
                                  importConfirmed);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *trust = nsIX509CertDB::UNTRUSTED;
  if (!*importConfirmed) {
    return NS_OK;
  }

  bool trustForSSL = false;
  rv = retVals->GetPropertyAsBool(NS_LITERAL_STRING("trustForSSL"),
                                  &trustForSSL);
  if (NS_FAILED(rv)) {
    return rv;
  }
  bool trustForEmail = false;
  rv = retVals->GetPropertyAsBool(NS_LITERAL_STRING("trustForEmail"),
                                  &trustForEmail);
  if (NS_FAILED(rv)) {
    return rv;
  }
  bool trustForObjSign = false;
  rv = retVals->GetPropertyAsBool(NS_LITERAL_STRING("trustForObjSign"),
                                  &trustForObjSign);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *trust |= trustForSSL ? nsIX509CertDB::TRUSTED_SSL : 0;
  *trust |= trustForEmail ? nsIX509CertDB::TRUSTED_EMAIL : 0;
  *trust |= trustForObjSign ? nsIX509CertDB::TRUSTED_OBJSIGN : 0;

  return NS_OK;
}

NS_IMETHODIMP
nsNSSDialogs::ChooseCertificate(nsIInterfaceRequestor* ctx,
                                const nsACString& hostname,
                                int32_t port,
                                const nsACString& organization,
                                const nsACString& issuerOrg,
                                nsIArray* certList,
                        /*out*/ uint32_t* selectedIndex,
                        /*out*/ bool* certificateChosen)
{
  NS_ENSURE_ARG_POINTER(ctx);
  NS_ENSURE_ARG_POINTER(certList);
  NS_ENSURE_ARG_POINTER(selectedIndex);
  NS_ENSURE_ARG_POINTER(certificateChosen);

  *certificateChosen = false;

  nsCOMPtr<nsIMutableArray> argArray = nsArrayBase::Create();
  if (!argArray) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWritableVariant> hostnameVariant = new nsVariant();
  nsresult rv = hostnameVariant->SetAsAUTF8String(hostname);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = argArray->AppendElement(hostnameVariant, false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIWritableVariant> organizationVariant = new nsVariant();
  rv = organizationVariant->SetAsAUTF8String(organization);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = argArray->AppendElement(organizationVariant, false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIWritableVariant> issuerOrgVariant = new nsVariant();
  rv = issuerOrgVariant->SetAsAUTF8String(issuerOrg);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = argArray->AppendElement(issuerOrgVariant, false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIWritableVariant> portVariant = new nsVariant();
  rv = portVariant->SetAsInt32(port);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = argArray->AppendElement(portVariant, false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = argArray->AppendElement(certList, false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIWritablePropertyBag2> retVals = new nsHashPropertyBag();
  rv = argArray->AppendElement(retVals, false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = nsNSSDialogHelper::openDialog(nullptr,
                                     "chrome://pippki/content/clientauthask.xul",
                                     argArray);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIClientAuthUserDecision> extraResult = do_QueryInterface(ctx);
  if (extraResult) {
    bool rememberSelection = false;
    rv = retVals->GetPropertyAsBool(NS_LITERAL_STRING("rememberSelection"),
                                    &rememberSelection);
    if (NS_SUCCEEDED(rv)) {
      extraResult->SetRememberClientAuthCertificate(rememberSelection);
    }
  }

  rv = retVals->GetPropertyAsBool(NS_LITERAL_STRING("certChosen"),
                                  certificateChosen);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (*certificateChosen) {
    rv = retVals->GetPropertyAsUint32(NS_LITERAL_STRING("selectedIndex"),
                                      selectedIndex);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSDialogs::SetPKCS12FilePassword(nsIInterfaceRequestor* ctx,
                            /*out*/ nsAString& password,
                            /*out*/ bool* confirmedPassword)
{
  // |ctx| is allowed to be null.
  NS_ENSURE_ARG(confirmedPassword);

  // Get the parent window for the dialog
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(ctx);
  nsCOMPtr<nsIWritablePropertyBag2> retVals = new nsHashPropertyBag();
  nsresult rv =
    nsNSSDialogHelper::openDialog(parent,
                                  "chrome://pippki/content/setp12password.xul",
                                  retVals);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = retVals->GetPropertyAsBool(NS_LITERAL_STRING("confirmedPassword"),
                                  confirmedPassword);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!*confirmedPassword) {
    return NS_OK;
  }

  return retVals->GetPropertyAsAString(NS_LITERAL_STRING("password"), password);
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
  // |ctx| is allowed to be null.
  NS_ENSURE_ARG(cert);

  // Get the parent window for the dialog
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(ctx);
  return nsNSSDialogHelper::openDialog(parent,
                                       "chrome://pippki/content/certViewer.xul",
                                       cert,
                                       false /*modal*/);
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
nsNSSDialogs::ChooseToken(nsIInterfaceRequestor* /*aCtx*/,
                          const char16_t** aTokenList,
                          uint32_t aCount,
                  /*out*/ nsAString& aTokenChosen,
                  /*out*/ bool* aCanceled)
{
  NS_ENSURE_ARG(aTokenList);
  NS_ENSURE_ARG(aCanceled);

  *aCanceled = false;

  nsCOMPtr<nsIDialogParamBlock> block =
           do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  if (!block) return NS_ERROR_FAILURE;

  block->SetNumberStrings(aCount);

  nsresult rv;
  for (uint32_t i = 0; i < aCount; i++) {
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

  *aCanceled = (status == 0);
  if (!*aCanceled) {
    // retrieve the nickname
    rv = block->GetString(0, getter_Copies(aTokenChosen));
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
