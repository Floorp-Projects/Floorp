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
 *  Ian McGreer <mcgreer@netscape.com>
 *  Javier Delgadillo <javi@netscape.com>
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
 */

#ifndef _NS_NSSCERTIFICATE_H_
#define _NS_NSSCERTIFICATE_H_

#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"

#include "prtypes.h"
#include "cert.h"
#include "secitem.h"

/* Certificate */
class nsNSSCertificate : public nsIX509Cert 
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIX509CERT

  nsNSSCertificate(char *certDER, int derLen);
  nsNSSCertificate(CERTCertificate *cert);
  /* from a request? */
  virtual ~nsNSSCertificate();
  CERTCertificate *GetCert();

private:
  CERTCertificate *mCert;

  PRBool verifyFailed(PRUint32 *_verified);
};

class nsNSSCertificateDB : public nsIX509CertDB
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIX509CERTDB

  nsNSSCertificateDB(); 
  virtual ~nsNSSCertificateDB();

private:

  void getCertNames(CERTCertList *certList,
                    PRUint32      type, 
                    nsString&     nameList);

  PRUint32 getCertType(CERTCertificate *cert);

};

#endif /* _NS_NSSCERTIFICATE_H_ */
