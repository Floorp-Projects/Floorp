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
#include "nsINSSDialogs.h"
#include "nsReadableUtils.h"

#include "cert.h"

/* strings for marking invalid user cert nicknames */
#define NICKNAME_EXPIRED_STRING " (expired)"
#define NICKNAME_NOT_YET_VALID_STRING " (not yet valid)"


NS_IMPL_ISUPPORTS1(nsCertPicker, nsIUserCertPicker)

nsCertPicker::nsCertPicker()
{
  NS_INIT_ISUPPORTS();
}

nsCertPicker::~nsCertPicker()
{
}

/* nsIX509Cert pick (in nsIInterfaceRequestor ctx, in wstring title, in wstring infoPrompt, in PRInt32 certUsage, in boolean allowInvalid, in boolean allowDuplicateNicknames, out boolean canceled); */
NS_IMETHODIMP nsCertPicker::PickByUsage(nsIInterfaceRequestor *ctx, const PRUnichar *title, const PRUnichar *infoPrompt, PRInt32 certUsage, PRBool allowInvalid, PRBool allowDuplicateNicknames, PRBool *canceled, nsIX509Cert **_retval)
{
  PRInt32 i = 0;
  PRInt32 selectedIndex = -1;
  PRUnichar **certNicknameList = nsnull;
  PRUnichar **certDetailsList = nsnull;
  CERTCertListNode* node = nsnull;
  CERTCertificate* cert = nsnull;
  nsresult rv;

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
  
  if (!certList) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  
  rv = NS_OK;
  
  CERTCertNicknames *nicknames = 
    CERT_NicknameStringsFromCertList(certList,
                                     NICKNAME_EXPIRED_STRING,
                                     NICKNAME_NOT_YET_VALID_STRING);

  if (!nicknames) {
    rv = NS_ERROR_NOT_AVAILABLE;
  }
  else {
    certNicknameList = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * nicknames->numnicknames);
    certDetailsList = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * nicknames->numnicknames);

    for (i = 0, node = CERT_LIST_HEAD(certList);
         !CERT_LIST_END(node, certList);
         ++i, node = CERT_LIST_NEXT(node)
        )
    {
      nsNSSCertificate *tempCert = new nsNSSCertificate(node->cert);
      if (tempCert) {

        // XXX we really should be using an nsCOMPtr instead of manually add-refing,
        // but nsNSSCertificate does not have a default constructor.

        NS_ADDREF(tempCert);

        nsAutoString i_nickname(NS_ConvertUTF8toUCS2(nicknames->nicknames[i]));
        nsAutoString nickWithSerial;
        nsAutoString details;
        
        if (NS_SUCCEEDED(tempCert->FormatUIStrings(i_nickname, nickWithSerial, details))) {
          certNicknameList[i] = ToNewUnicode(nickWithSerial);
          certDetailsList[i] = ToNewUnicode(details);
        }

        NS_RELEASE(tempCert);
      }
    }

    nsICertPickDialogs *dialogs = nsnull;
    rv = getNSSDialogs((void**)&dialogs, NS_GET_IID(nsICertPickDialogs));

    if (NS_SUCCEEDED(rv)) {
      /* Throw up the cert picker dialog and get back the index of the selected cert */
      rv = dialogs->PickCertificate(ctx, title, infoPrompt,
        (const PRUnichar**)certNicknameList, (const PRUnichar**)certDetailsList,
        nicknames->numnicknames, &selectedIndex, canceled);

      for (i = 0; i < nicknames->numnicknames; ++i) {
        nsMemory::Free(certNicknameList[i]);
        nsMemory::Free(certDetailsList[i]);
      }
      nsMemory::Free(certNicknameList);
      nsMemory::Free(certDetailsList);

      NS_RELEASE(dialogs);
    }

    if (NS_SUCCEEDED(rv) && !*canceled) {
      for (i = 0, node = CERT_LIST_HEAD(certList);
           !CERT_LIST_END(node, certList);
           ++i, node = CERT_LIST_NEXT(node)) {

        if (i == selectedIndex) {
          nsNSSCertificate *cert = new nsNSSCertificate(node->cert);
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

    CERT_FreeNicknames(nicknames);
  }

  if (certList) {
    CERT_DestroyCertList(certList);
  }
  return NS_OK;
}
