/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCertPicker.h"
#include "pkix/pkixtypes.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsNSSComponent.h"
#include "nsNSSCertificate.h"
#include "nsReadableUtils.h"
#include "nsICertPickDialogs.h"
#include "nsNSSShutDown.h"
#include "nsNSSCertHelper.h"
#include "ScopedNSSTypes.h"

#include "cert.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsCertPicker, nsIUserCertPicker)

nsCertPicker::nsCertPicker()
{
}

nsCertPicker::~nsCertPicker()
{
}

NS_IMETHODIMP nsCertPicker::PickByUsage(nsIInterfaceRequestor *ctx, 
                                        const char16_t *selectedNickname, 
                                        int32_t certUsage, 
                                        bool allowInvalid, 
                                        bool allowDuplicateNicknames, 
                                        const nsAString &emailAddress,
                                        bool *canceled, 
                                        nsIX509Cert **_retval)
{
  nsNSSShutDownPreventionLock locker;
  int32_t selectedIndex = -1;
  bool selectionFound = false;
  char16_t **certNicknameList = nullptr;
  char16_t **certDetailsList = nullptr;
  CERTCertListNode* node = nullptr;
  nsresult rv = NS_OK;

  {
    // Iterate over all certs. This assures that user is logged in to all hardware tokens.
    nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
    ScopedCERTCertList allcerts(PK11_ListCerts(PK11CertListUnique, ctx));
  }

  /* find all user certs that are valid for the specified usage */
  /* note that we are allowing expired certs in this list */

  ScopedCERTCertList certList(
    CERT_FindUserCertsByUsage(CERT_GetDefaultCertDB(), 
                              (SECCertUsage)certUsage,
                              !allowDuplicateNicknames,
                              !allowInvalid,
                              ctx));
  if (!certList) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  /* if a (non-empty) emailAddress argument is supplied to PickByUsage, */
  /* remove non-matching certificates from the candidate list */

  if (!emailAddress.IsEmpty()) {
    node = CERT_LIST_HEAD(certList);
    while (!CERT_LIST_END(node, certList)) {
      /* if the cert has at least one e-mail address, check if suitable */
      if (CERT_GetFirstEmailAddress(node->cert)) {
        RefPtr<nsNSSCertificate> tempCert(nsNSSCertificate::Create(node->cert));
        bool match = false;
        rv = tempCert->ContainsEmailAddress(emailAddress, &match);
        if (NS_FAILED(rv)) {
          return rv;
        }
        if (!match) {
          /* doesn't contain the specified address, so remove from the list */
          CERTCertListNode* freenode = node;
          node = CERT_LIST_NEXT(node);
          CERT_RemoveCertListNode(freenode);
          continue;
        }
      }
      node = CERT_LIST_NEXT(node);
    }
  }

  ScopedCERTCertNicknames nicknames(getNSSCertNicknamesFromCertList(certList.get()));
  if (!nicknames) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  certNicknameList = (char16_t **)moz_xmalloc(sizeof(char16_t *) * nicknames->numnicknames);
  certDetailsList = (char16_t **)moz_xmalloc(sizeof(char16_t *) * nicknames->numnicknames);

  if (!certNicknameList || !certDetailsList) {
    free(certNicknameList);
    free(certDetailsList);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  int32_t CertsToUse;

  for (CertsToUse = 0, node = CERT_LIST_HEAD(certList.get());
       !CERT_LIST_END(node, certList.get()) &&
         CertsToUse < nicknames->numnicknames;
       node = CERT_LIST_NEXT(node)
      )
  {
    RefPtr<nsNSSCertificate> tempCert(nsNSSCertificate::Create(node->cert));

    if (tempCert) {

      nsAutoString i_nickname(NS_ConvertUTF8toUTF16(nicknames->nicknames[CertsToUse]));
      nsAutoString nickWithSerial;
      nsAutoString details;

      if (!selectionFound) {
        /* for the case when selectedNickname refers to a bare nickname */
        if (i_nickname == nsDependentString(selectedNickname)) {
          selectedIndex = CertsToUse;
          selectionFound = true;
        }
      }

      if (NS_SUCCEEDED(tempCert->FormatUIStrings(i_nickname, nickWithSerial, details))) {
        certNicknameList[CertsToUse] = ToNewUnicode(nickWithSerial);
        certDetailsList[CertsToUse] = ToNewUnicode(details);
        if (!selectionFound) {
          /* for the case when selectedNickname refers to nickname + serial */
          if (nickWithSerial == nsDependentString(selectedNickname)) {
            selectedIndex = CertsToUse;
            selectionFound = true;
          }
        }
      }
      else {
        certNicknameList[CertsToUse] = nullptr;
        certDetailsList[CertsToUse] = nullptr;
      }

      ++CertsToUse;
    }
  }

  if (CertsToUse) {
    nsICertPickDialogs *dialogs = nullptr;
    rv = getNSSDialogs((void**)&dialogs, 
      NS_GET_IID(nsICertPickDialogs), 
      NS_CERTPICKDIALOGS_CONTRACTID);

    if (NS_SUCCEEDED(rv)) {
      nsPSMUITracker tracker;
      if (tracker.isUIForbidden()) {
        rv = NS_ERROR_NOT_AVAILABLE;
      }
      else {
        /* Throw up the cert picker dialog and get back the index of the selected cert */
        rv = dialogs->PickCertificate(ctx,
          (const char16_t**)certNicknameList, (const char16_t**)certDetailsList,
          CertsToUse, &selectedIndex, canceled);
      }

      NS_RELEASE(dialogs);
    }
  }

  int32_t i;
  for (i = 0; i < CertsToUse; ++i) {
    free(certNicknameList[i]);
    free(certDetailsList[i]);
  }
  free(certNicknameList);
  free(certDetailsList);
  
  if (!CertsToUse) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (NS_SUCCEEDED(rv) && !*canceled) {
    for (i = 0, node = CERT_LIST_HEAD(certList);
         !CERT_LIST_END(node, certList);
         ++i, node = CERT_LIST_NEXT(node)) {

      if (i == selectedIndex) {
        nsRefPtr<nsNSSCertificate> cert = nsNSSCertificate::Create(node->cert);
        if (!cert) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }

        cert.forget(_retval);
        break;
      }
    }
  }

  return rv;
}
