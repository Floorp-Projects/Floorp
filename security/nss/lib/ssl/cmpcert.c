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
/* $Id: cmpcert.c,v 1.6 2008/02/01 22:09:09 julien.pierre.boogz%sun.com Exp $ */

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

/*
 * Look to see if any of the signers in the cert chain for "cert" are found
 * in the list of caNames.  
 * Returns SECSuccess if so, SECFailure if not.
 */
SECStatus
NSS_CmpCertChainWCANames(CERTCertificate *cert, CERTDistNames *caNames)
{
  SECItem *         caname;
  CERTCertificate * curcert;
  CERTCertificate * oldcert;
  PRInt32           contentlen;
  int               j;
  int               headerlen;
  int               depth;
  SECStatus         rv;
  SECItem           issuerName;
  SECItem           compatIssuerName;

  if (!cert || !caNames || !caNames->nnames || !caNames->names ||
      !caNames->names->data)
    return SECFailure;
  depth=0;
  curcert = CERT_DupCertificate(cert);
  
  while( curcert ) {
    issuerName = curcert->derIssuer;
    
    /* compute an alternate issuer name for compatibility with 2.0
     * enterprise server, which send the CA names without
     * the outer layer of DER header
     */
    rv = DER_Lengths(&issuerName, &headerlen, (PRUint32 *)&contentlen);
    if ( rv == SECSuccess ) {
      compatIssuerName.data = &issuerName.data[headerlen];
      compatIssuerName.len = issuerName.len - headerlen;
    } else {
      compatIssuerName.data = NULL;
      compatIssuerName.len = 0;
    }
    
    for (j = 0; j < caNames->nnames; j++) {
      caname = &caNames->names[j];
      if (SECITEM_CompareItem(&issuerName, caname) == SECEqual) {
	rv = SECSuccess;
	CERT_DestroyCertificate(curcert);
	goto done;
      } else if (SECITEM_CompareItem(&compatIssuerName, caname) == SECEqual) {
	rv = SECSuccess;
	CERT_DestroyCertificate(curcert);
	goto done;
      }
    }
    if ( ( depth <= 20 ) &&
	 ( SECITEM_CompareItem(&curcert->derIssuer, &curcert->derSubject)
	   != SECEqual ) ) {
      oldcert = curcert;
      curcert = CERT_FindCertByName(curcert->dbhandle,
				    &curcert->derIssuer);
      CERT_DestroyCertificate(oldcert);
      depth++;
    } else {
      CERT_DestroyCertificate(curcert);
      curcert = NULL;
    }
  }
  rv = SECFailure;
  
done:
  return rv;
}

