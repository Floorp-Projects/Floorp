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
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
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
 */

#ifndef PKITM_H
#define PKITM_H

#ifdef DEBUG
static const char PKITM_CVS_ID[] = "@(#) $RCSfile: pkitm.h,v $ $Revision: 1.9 $ $Date: 2002/04/18 17:30:04 $ $Name:  $";
#endif /* DEBUG */

/*
 * pkitm.h
 *
 * This file contains PKI-module specific types.
 */

#ifndef BASET_H
#include "baset.h"
#endif /* BASET_H */

#ifndef PKIT_H
#include "pkit.h"
#endif /* PKIT_H */

PR_BEGIN_EXTERN_C

/*
 * nssDecodedCert
 *
 * This is an interface to allow the PKI module access to certificate
 * information that can only be found by decoding.  The interface is
 * generic, allowing each certificate type its own way of providing
 * the information
 */
struct nssDecodedCertStr {
    NSSCertificateType type;
    void *data;
    /* returns the unique identifier for the cert */
    NSSItem *  (*getIdentifier)(nssDecodedCert *dc);
    /* returns the unique identifier for this cert's issuer */
    NSSItem *  (*getIssuerIdentifier)(nssDecodedCert *dc);
    /* is id the identifier for this cert? */
    PRBool     (*matchIdentifier)(nssDecodedCert *dc, NSSItem *id);
    /* returns the cert usage */
    NSSUsage * (*getUsage)(nssDecodedCert *dc);
    /* is time within the validity period of the cert? */
    PRBool     (*isValidAtTime)(nssDecodedCert *dc, NSSTime *time);
    /* is the validity period of this cert newer than cmpdc? */
    PRBool     (*isNewerThan)(nssDecodedCert *dc, nssDecodedCert *cmpdc);
    /* does the usage for this cert match the requested usage? */
    PRBool     (*matchUsage)(nssDecodedCert *dc, NSSUsage *usage);
    /* extract the email address */
    NSSASCII7 *(*getEmailAddress)(nssDecodedCert *dc);
    /* extract the DER-encoded serial number */
    PRStatus   (*getDERSerialNumber)(nssDecodedCert *dc,
                                     NSSDER *derSerial, NSSArena *arena);
};

struct NSSUsageStr {
    PRBool anyUsage;
#ifdef NSS_3_4_CODE
    SECCertUsage nss3usage;
    PRBool nss3lookingForCA;
#endif
};

typedef struct nssBestCertificateCBStr nssBestCertificateCB;

struct nssBestCertificateCBStr {
    NSSCertificate *cert;
    NSSTime *time;
    NSSTime sTime; /* to avoid allocating when unnecessary */
    NSSUsage *usage;
    NSSPolicies *policies;
};

typedef struct nssPKIObjectCollectionStr nssPKIObjectCollection;

typedef struct
{
  union {
    PRStatus (*  cert)(NSSCertificate *c, void *arg);
    PRStatus (*   crl)(NSSCRL       *crl, void *arg);
    PRStatus (* pvkey)(NSSPrivateKey *vk, void *arg);
    PRStatus (* pbkey)(NSSPublicKey *bk, void *arg);
  } func;
  void *arg;
} nssPKIObjectCallback;

PR_END_EXTERN_C

#endif /* PKITM_H */
