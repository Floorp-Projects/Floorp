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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian McGreer <mcgreer@netscape.com>
 *   Javier Delgadillo <javi@netscape.com>
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

#include "nsOCSPResponder.h"

#include "nsCOMPtr.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsComponentManagerUtils.h"
#include "nsReadableUtils.h"

#include "certdb.h"

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsOCSPResponder, nsIOCSPResponder)

nsOCSPResponder::nsOCSPResponder()
{
  /* member initializers and constructor code */
}

nsOCSPResponder::nsOCSPResponder(const PRUnichar * aCA, const PRUnichar * aURL)
{
  mCA.Assign(aCA);
  mURL.Assign(aURL);
}

nsOCSPResponder::~nsOCSPResponder()
{
  /* destructor code */
}

/* readonly attribute */
NS_IMETHODIMP nsOCSPResponder::GetResponseSigner(PRUnichar** aCA)
{
  NS_ENSURE_ARG(aCA);
  *aCA = ToNewUnicode(mCA);
  return NS_OK;
}

/* readonly attribute */
NS_IMETHODIMP nsOCSPResponder::GetServiceURL(PRUnichar** aURL)
{
  NS_ENSURE_ARG(aURL);
  *aURL = ToNewUnicode(mURL);
  return NS_OK;
}

bool nsOCSPResponder::IncludeCert(CERTCertificate *aCert)
{
  CERTCertTrust *trust;
  char *nickname;

  trust = aCert->trust;
  nickname = aCert->nickname;

  PR_ASSERT(trust != nsnull);

  // Check that trust is non-null //
  if (trust == nsnull) {
    return PR_FALSE;
  }

  if ( ( ( trust->sslFlags & CERTDB_INVISIBLE_CA ) ||
         (trust->emailFlags & CERTDB_INVISIBLE_CA ) ||
         (trust->objectSigningFlags & CERTDB_INVISIBLE_CA ) ) ||
       nickname == NULL) {
      return PR_FALSE;
  }
  if ((trust->sslFlags & CERTDB_VALID_CA) ||
      (trust->emailFlags & CERTDB_VALID_CA) ||
      (trust->objectSigningFlags & CERTDB_VALID_CA)) {
      return PR_TRUE;
  }
  return PR_FALSE;
}

// CmpByCAName
//
// Compare two responders their token name.  Returns -1, 0, 1 as
// in strcmp.  No token name (null) is treated as >.
PRInt32 nsOCSPResponder::CmpCAName(nsIOCSPResponder *a, nsIOCSPResponder *b)
{
  PRInt32 cmp1;
  nsXPIDLString aTok, bTok;
  a->GetResponseSigner(getter_Copies(aTok));
  b->GetResponseSigner(getter_Copies(bTok));
  if (aTok != nsnull && bTok != nsnull) {
    cmp1 = Compare(aTok, bTok);
  } else {
    cmp1 = (aTok == nsnull) ? 1 : -1;
  }
  return cmp1;
}

// ocsp_compare_entries
//
// Compare two responders.  Returns -1, 0, 1 as
// in strcmp.  Entries with urls come before those without urls.
PRInt32 nsOCSPResponder::CompareEntries(nsIOCSPResponder *a, nsIOCSPResponder *b)
{
  nsXPIDLString aURL, bURL;
  nsAutoString aURLAuto, bURLAuto;

  a->GetServiceURL(getter_Copies(aURL));
  aURLAuto.Assign(aURL);
  b->GetServiceURL(getter_Copies(bURL));
  bURLAuto.Assign(bURL);

  if (!aURLAuto.IsEmpty()) {
    if (!bURLAuto.IsEmpty()) {
      return nsOCSPResponder::CmpCAName(a, b);
    } else {
      return -1;
    }
  } else {
    if (!bURLAuto.IsEmpty()) {
      return 1;
    } else {
      return nsOCSPResponder::CmpCAName(a, b);
    }
  }
}

