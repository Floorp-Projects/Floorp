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

#ifndef __NSNSSCERTIFICATEDB_H__
#define __NSNSSCERTIFICATEDB_H__

#include "nsIX509CertDB.h"
#include "nsIX509CertDB2.h"
#include "nsNSSCertHeader.h"

class nsIArray;

class nsNSSCertificateDB : public nsIX509CertDB, public nsIX509CertDB2
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIX509CERTDB
  NS_DECL_NSIX509CERTDB2

  nsNSSCertificateDB(); 
  virtual ~nsNSSCertificateDB();

  // Use this function to generate a default nickname for a user
  // certificate that is to be imported onto a token.
  static char *
  default_nickname(CERTCertificate *cert, nsIInterfaceRequestor* ctx);

  static nsresult 
  ImportValidCACerts(int numCACerts, SECItem *CACerts, nsIInterfaceRequestor *ctx);

private:

  static nsresult
  ImportValidCACertsInList(CERTCertList *certList, nsIInterfaceRequestor *ctx);

  void getCertNames(CERTCertList *certList,
                    PRUint32      type, 
                    PRUint32     *_count,
                    PRUnichar  ***_certNameList);

  CERTDERCerts *getCertsFromPackage(PRArenaPool *arena, PRUint8 *data, 
                                    PRUint32 length);
  nsresult handleCACertDownload(nsIArray *x509Certs, 
                                nsIInterfaceRequestor *ctx);
};

#define NS_X509CERTDB_CID { /* fb0bbc5c-452e-4783-b32c-80124693d871 */ \
    0xfb0bbc5c,                                                        \
    0x452e,                                                            \
    0x4783,                                                            \
    {0xb3, 0x2c, 0x80, 0x12, 0x46, 0x93, 0xd8, 0x71}                   \
  }

#endif
