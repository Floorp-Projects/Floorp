/*
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 * $Id: nsNSSCertificate.cpp,v 1.1 2001/02/16 22:17:44 javi%netscape.com Exp $
 */

#include "nsNSSCertificate.h"
#include "nsString.h"

/* nsNSSCertificate */

NS_IMPL_ISUPPORTS1(nsNSSCertificate, nsIX509Cert)

nsNSSCertificate::nsNSSCertificate(const char *certDER, int derLen)
{
  NS_INIT_ISUPPORTS();

  mCert = CERT_DecodeCertFromPackage(certDER, derLen);
}

nsNSSCertificate::nsNSSCertificate(CERTCertificate *cert)
{
  NS_INIT_ISUPPORTS();

  if (cert) 
    mCert = CERT_DupCertificate(cert);
}

nsNSSCertificate::~nsNSSCertificate()
{
  if (mCert)
    CERT_DestroyCertificate(mCert);
}

NS_IMETHODIMP
nsNSSCertificate::GetCommonName(PRUnichar **aCommonName)
{
  NS_ENSURE_ARG(aCommonName);
  *aCommonName = nsnull;
  if (mCert) {
    char *commonName = CERT_GetCommonName(&mCert->subject);
    if (commonName) {
      nsAutoString cn = NS_ConvertASCIItoUCS2(commonName);
      *aCommonName = cn.ToNewUnicode();
    }
  }
  return NS_OK;
}

