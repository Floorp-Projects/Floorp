/*
 * NSS utility functions
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include <stdio.h>
#include <string.h>
#include "prerror.h"
#include "secitem.h"
#include "prnetdb.h"
#include "cert.h"
#include "nspr.h"
#include "secder.h"
#include "key.h"
#include "nss.h"
#include "ssl.h"
#include "pk11func.h"	/* for PK11_ function calls */

/*
 * This callback used by SSL to pull client sertificate upon
 * server request
 */
SECStatus 
NSS_GetClientAuthData(void *                       arg, 
                      PRFileDesc *                 socket, 
		      struct CERTDistNamesStr *    caNames, 
		      struct CERTCertificateStr ** pRetCert, 
		      struct SECKEYPrivateKeyStr **pRetKey)
{
  CERTCertificate *  cert = NULL;
  SECKEYPrivateKey * privkey = NULL;
  char *             chosenNickName = (char *)arg;    /* CONST */
  void *             proto_win  = NULL;
  SECStatus          rv         = SECFailure;
  
  proto_win = SSL_RevealPinArg(socket);
  
  if (chosenNickName) {
    cert = CERT_FindUserCertByUsage(CERT_GetDefaultCertDB(),
                                    chosenNickName, certUsageSSLClient,
                                    PR_FALSE, proto_win);	
    if ( cert ) {
      privkey = PK11_FindKeyByAnyCert(cert, proto_win);
      if ( privkey ) {
	rv = SECSuccess;
      } else {
	CERT_DestroyCertificate(cert);
      }
    }
  } else { /* no name given, automatically find the right cert. */
    CERTCertNicknames * names;
    int                 i;
      
    names = CERT_GetCertNicknames(CERT_GetDefaultCertDB(),
				  SEC_CERT_NICKNAMES_USER, proto_win);
    if (names != NULL) {
      for (i = 0; i < names->numnicknames; i++) {
	cert = CERT_FindUserCertByUsage(CERT_GetDefaultCertDB(),
                            names->nicknames[i], certUsageSSLClient,
                            PR_FALSE, proto_win);	
	if ( !cert )
	  continue;
	/* Only check unexpired certs */
	if (CERT_CheckCertValidTimes(cert, PR_Now(), PR_TRUE) != 
	    secCertTimeValid ) {
	  CERT_DestroyCertificate(cert);
	  continue;
	}
	rv = NSS_CmpCertChainWCANames(cert, caNames);
	if ( rv == SECSuccess ) {
	  privkey = PK11_FindKeyByAnyCert(cert, proto_win);
	  if ( privkey )
	    break;
	}
	rv = SECFailure;
	CERT_DestroyCertificate(cert);
      } 
      CERT_FreeNicknames(names);
    }
  }
  if (rv == SECSuccess) {
    *pRetCert = cert;
    *pRetKey  = privkey;
  }
  return rv;
}

