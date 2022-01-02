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
#include "nsComponentManagerUtils.h"
#include "nsEmbedCID.h"
#include "nsHashPropertyBag.h"
#include "nsIDialogParamBlock.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPK11Token.h"
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

nsNSSDialogs::nsNSSDialogs() = default;

nsNSSDialogs::~nsNSSDialogs() = default;

NS_IMPL_ISUPPORTS(nsNSSDialogs, nsITokenPasswordDialogs, nsICertificateDialogs,
                  nsIClientAuthDialogs, nsITokenDialogs)

nsresult nsNSSDialogs::Init() {
  nsresult rv;

  nsCOMPtr<nsIStringBundleService> service =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = service->CreateBundle(PIPSTRING_BUNDLE_URL,
                             getter_AddRefs(mPIPStringBundle));
  return rv;
}

NS_IMETHODIMP
nsNSSDialogs::SetPassword(nsIInterfaceRequestor* ctx, nsIPK11Token* token,
                          /*out*/ bool* canceled) {
  // |ctx| is allowed to be null.
  NS_ENSURE_ARG(canceled);

  *canceled = false;

  // Get the parent window for the dialog
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(ctx);

  nsCOMPtr<nsIDialogParamBlock> block =
      do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID);
  if (!block) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIMutableArray> objects = nsArrayBase::Create();
  if (!objects) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = objects->AppendElement(token);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = block->SetObjects(objects);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = nsNSSDialogHelper::openDialog(
      parent, "chrome://pippki/content/changepassword.xhtml", block);

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
                                    /*out*/ bool* importConfirmed) {
  // |ctx| is allowed to be null.
  NS_ENSURE_ARG(cert);
  NS_ENSURE_ARG(trust);
  NS_ENSURE_ARG(importConfirmed);

  nsCOMPtr<nsIMutableArray> argArray = nsArrayBase::Create();
  if (!argArray) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = argArray->AppendElement(cert);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIWritablePropertyBag2> retVals = new nsHashPropertyBag();
  rv = argArray->AppendElement(retVals);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the parent window for the dialog
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(ctx);
  rv = nsNSSDialogHelper::openDialog(
      parent, "chrome://pippki/content/downloadcert.xhtml", argArray);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = retVals->GetPropertyAsBool(u"importConfirmed"_ns, importConfirmed);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *trust = nsIX509CertDB::UNTRUSTED;
  if (!*importConfirmed) {
    return NS_OK;
  }

  bool trustForSSL = false;
  rv = retVals->GetPropertyAsBool(u"trustForSSL"_ns, &trustForSSL);
  if (NS_FAILED(rv)) {
    return rv;
  }
  bool trustForEmail = false;
  rv = retVals->GetPropertyAsBool(u"trustForEmail"_ns, &trustForEmail);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *trust |= trustForSSL ? nsIX509CertDB::TRUSTED_SSL : 0;
  *trust |= trustForEmail ? nsIX509CertDB::TRUSTED_EMAIL : 0;

  return NS_OK;
}

NS_IMETHODIMP
nsNSSDialogs::ChooseCertificate(const nsACString& hostname, int32_t port,
                                const nsACString& organization,
                                const nsACString& issuerOrg, nsIArray* certList,
                                /*out*/ uint32_t* selectedIndex,
                                /*out*/ bool* rememberClientAuthCertificate,
                                /*out*/ bool* certificateChosen) {
  NS_ENSURE_ARG_POINTER(certList);
  NS_ENSURE_ARG_POINTER(selectedIndex);
  NS_ENSURE_ARG_POINTER(rememberClientAuthCertificate);
  NS_ENSURE_ARG_POINTER(certificateChosen);

  *certificateChosen = false;
  *rememberClientAuthCertificate = false;

  nsCOMPtr<nsIMutableArray> argArray = nsArrayBase::Create();
  if (!argArray) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWritableVariant> hostnameVariant = new nsVariant();
  nsresult rv = hostnameVariant->SetAsAUTF8String(hostname);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = argArray->AppendElement(hostnameVariant);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIWritableVariant> organizationVariant = new nsVariant();
  rv = organizationVariant->SetAsAUTF8String(organization);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = argArray->AppendElement(organizationVariant);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIWritableVariant> issuerOrgVariant = new nsVariant();
  rv = issuerOrgVariant->SetAsAUTF8String(issuerOrg);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = argArray->AppendElement(issuerOrgVariant);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIWritableVariant> portVariant = new nsVariant();
  rv = portVariant->SetAsInt32(port);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = argArray->AppendElement(portVariant);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = argArray->AppendElement(certList);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIWritablePropertyBag2> retVals = new nsHashPropertyBag();
  rv = argArray->AppendElement(retVals);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = nsNSSDialogHelper::openDialog(
      nullptr, "chrome://pippki/content/clientauthask.xhtml", argArray);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = retVals->GetPropertyAsBool(u"rememberSelection"_ns,
                                  rememberClientAuthCertificate);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = retVals->GetPropertyAsBool(u"certChosen"_ns, certificateChosen);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (*certificateChosen) {
    rv = retVals->GetPropertyAsUint32(u"selectedIndex"_ns, selectedIndex);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSDialogs::SetPKCS12FilePassword(nsIInterfaceRequestor* ctx,
                                    /*out*/ nsAString& password,
                                    /*out*/ bool* confirmedPassword) {
  // |ctx| is allowed to be null.
  NS_ENSURE_ARG(confirmedPassword);

  // Get the parent window for the dialog
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(ctx);
  nsCOMPtr<nsIWritablePropertyBag2> retVals = new nsHashPropertyBag();
  nsresult rv = nsNSSDialogHelper::openDialog(
      parent, "chrome://pippki/content/setp12password.xhtml", retVals);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = retVals->GetPropertyAsBool(u"confirmedPassword"_ns, confirmedPassword);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!*confirmedPassword) {
    return NS_OK;
  }

  return retVals->GetPropertyAsAString(u"password"_ns, password);
}

NS_IMETHODIMP
nsNSSDialogs::GetPKCS12FilePassword(nsIInterfaceRequestor* ctx,
                                    nsAString& _password, bool* _retval) {
  *_retval = false;

  nsCOMPtr<nsIPromptService> promptSvc(
      do_GetService(NS_PROMPTSERVICE_CONTRACTID));
  if (!promptSvc) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString msg;
  nsresult rv =
      mPIPStringBundle->GetStringFromName("getPKCS12FilePasswordMessage", msg);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the parent window for the dialog
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(ctx);
  char16_t* pwTemp = nullptr;
  rv = promptSvc->PromptPassword(parent, nullptr, msg.get(), &pwTemp, _retval);
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
nsNSSDialogs::DisplayProtectedAuth(nsIInterfaceRequestor* aCtx,
                                   nsIProtectedAuthThread* runnable) {
  // We cannot use nsNSSDialogHelper here. We cannot allow close widget
  // in the window because protected authentication is interruptible
  // from user interface and changing nsNSSDialogHelper's static variable
  // would not be thread-safe

  nsresult rv = NS_ERROR_FAILURE;

  // Get the parent window for the dialog
  nsCOMPtr<mozIDOMWindowProxy> parent = do_GetInterface(aCtx);

  nsCOMPtr<nsIWindowWatcher> windowWatcher =
      do_GetService("@mozilla.org/embedcomp/window-watcher;1", &rv);
  if (NS_FAILED(rv)) return rv;

  if (!parent) {
    windowWatcher->GetActiveWindow(getter_AddRefs(parent));
  }

  nsCOMPtr<mozIDOMWindowProxy> newWindow;
  rv = windowWatcher->OpenWindow(
      parent, "chrome://pippki/content/protectedAuth.xhtml"_ns, "_blank"_ns,
      "centerscreen,chrome,modal,titlebar,close=no"_ns, runnable,
      getter_AddRefs(newWindow));

  return rv;
}
