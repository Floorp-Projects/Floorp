/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Kai Engert <kaie@netscape.com>
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

#include "nsCertPicker.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsNSSComponent.h"
#include "nsNSSCertificate.h"
#include "nsReadableUtils.h"
#include "nsNSSCleaner.h"
#include "nsICertPickDialogs.h"
#include "nsNSSShutDown.h"
#include "nsNSSCertHelper.h"

NSSCleanupAutoPtrClass(CERTCertNicknames, CERT_FreeNicknames)
NSSCleanupAutoPtrClass(CERTCertList, CERT_DestroyCertList)

#include "cert.h"

NS_IMPL_ISUPPORTS1(nsCertPicker, nsIUserCertPicker)

nsCertPicker::nsCertPicker()
{
}

nsCertPicker::~nsCertPicker()
{
}

NS_IMETHODIMP nsCertPicker::PickByUsage(nsIInterfaceRequestor *ctx, 
                                        const PRUnichar *selectedNickname, 
                                        PRInt32 certUsage, 
                                        bool allowInvalid, 
                                        bool allowDuplicateNicknames, 
                                        bool *canceled, 
                                        nsIX509Cert **_retval)
{
  nsNSSShutDownPreventionLock locker;
  PRInt32 selectedIndex = -1;
  bool selectionFound = false;
  PRUnichar **certNicknameList = nsnull;
  PRUnichar **certDetailsList = nsnull;
  CERTCertListNode* node = nsnull;
  nsresult rv = NS_OK;

  {
    // Iterate over all certs. This assures that user is logged in to all hardware tokens.
    CERTCertList *allcerts = nsnull;
    nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
    allcerts = PK11_ListCerts(PK11CertListUnique, ctx);
    CERT_DestroyCertList(allcerts);
  }

  /* find all user certs that are valid and for SSL */
  /* note that we are allowing expired certs in this list */

  CERTCertList *certList = 
    CERT_FindUserCertsByUsage(CERT_GetDefaultCertDB(), 
                              (SECCertUsage)certUsage,
                              !allowDuplicateNicknames,
                              !allowInvalid,
                              ctx);
  CERTCertListCleaner clc(certList);

  if (!certList) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  CERTCertNicknames *nicknames = getNSSCertNicknamesFromCertList(certList);

  CERTCertNicknamesCleaner cnc(nicknames);

  if (!nicknames) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  certNicknameList = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * nicknames->numnicknames);
  certDetailsList = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * nicknames->numnicknames);

  if (!certNicknameList || !certDetailsList) {
    nsMemory::Free(certNicknameList);
    nsMemory::Free(certDetailsList);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 CertsToUse;

  for (CertsToUse = 0, node = CERT_LIST_HEAD(certList);
       !CERT_LIST_END(node, certList) && CertsToUse < nicknames->numnicknames;
       node = CERT_LIST_NEXT(node)
      )
  {
    nsNSSCertificate *tempCert = nsNSSCertificate::Create(node->cert);

    if (tempCert) {

      // XXX we really should be using an nsCOMPtr instead of manually add-refing,
      // but nsNSSCertificate does not have a default constructor.

      NS_ADDREF(tempCert);

      nsAutoString i_nickname(NS_ConvertUTF8toUTF16(nicknames->nicknames[CertsToUse]));
      nsAutoString nickWithSerial;
      nsAutoString details;

      if (!selectionFound) {
        if (i_nickname == nsDependentString(selectedNickname)) {
          selectedIndex = CertsToUse;
          selectionFound = PR_TRUE;
        }
      }

      if (NS_SUCCEEDED(tempCert->FormatUIStrings(i_nickname, nickWithSerial, details))) {
        certNicknameList[CertsToUse] = ToNewUnicode(nickWithSerial);
        certDetailsList[CertsToUse] = ToNewUnicode(details);
      }
      else {
        certNicknameList[CertsToUse] = nsnull;
        certDetailsList[CertsToUse] = nsnull;
      }

      NS_RELEASE(tempCert);

      ++CertsToUse;
    }
  }

  if (CertsToUse) {
    nsICertPickDialogs *dialogs = nsnull;
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
          (const PRUnichar**)certNicknameList, (const PRUnichar**)certDetailsList,
          CertsToUse, &selectedIndex, canceled);
      }

      NS_RELEASE(dialogs);
    }
  }

  PRInt32 i;
  for (i = 0; i < CertsToUse; ++i) {
    nsMemory::Free(certNicknameList[i]);
    nsMemory::Free(certDetailsList[i]);
  }
  nsMemory::Free(certNicknameList);
  nsMemory::Free(certDetailsList);
  
  if (!CertsToUse) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (NS_SUCCEEDED(rv) && !*canceled) {
    for (i = 0, node = CERT_LIST_HEAD(certList);
         !CERT_LIST_END(node, certList);
         ++i, node = CERT_LIST_NEXT(node)) {

      if (i == selectedIndex) {
        nsNSSCertificate *cert = nsNSSCertificate::Create(node->cert);
        if (!cert) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }

        nsIX509Cert *x509 = 0;
        nsresult rv = cert->QueryInterface(NS_GET_IID(nsIX509Cert), (void**)&x509);
        if (NS_FAILED(rv)) {
          break;
        }

        NS_ADDREF(x509);
        *_retval = x509;
        NS_RELEASE(cert);
        break;
      }
    }
  }

  return rv;
}
