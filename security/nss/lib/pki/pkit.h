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

#ifndef PKIT_H
#define PKIT_H

#ifdef DEBUG
static const char PKIT_CVS_ID[] = "@(#) $RCSfile: pkit.h,v $ $Revision: 1.5 $ $Date: 2001/10/19 18:16:44 $ $Name:  $";
#endif /* DEBUG */

/*
 * pkit.h
 *
 * This file contains definitions for the types of the top-level PKI objects.
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

#ifndef BASET_H
#include "baset.h"
#endif /* BASET_H */

#ifdef NSS_3_4_CODE
#include "certt.h"
#include "pkcs11t.h"
#define NSSCKT_H
#include "ckt.h"
#else
#ifndef NSSCKT_H
#include "nssckt.h"
#endif /* NSSCKT_H */
#endif /* NSS_3_4_CODE */

#ifndef NSSPKIT_H
#include "nsspkit.h"
#endif /* NSSPKIT_H */

#ifndef NSSDEVT_H
#include "nssdevt.h"
#endif /* NSSDEVT_H */

PR_BEGIN_EXTERN_C

typedef enum {
    NSSCertificateType_Unknown = 0,
    NSSCertificateType_PKIX = 1
} NSSCertificateType;

typedef struct nssDecodedCertStr nssDecodedCert;

struct NSSTrustStr 
{
    CK_TRUST serverAuth;
    CK_TRUST emailProtection;
    CK_TRUST codeSigning;
};

struct NSSCertificateStr
{
    PRInt32 refCount;
    NSSArena *arena;
    NSSCertificateType type;
    NSSItem id;
    NSSBER encoding;
    NSSDER issuer;
    NSSDER subject;
    NSSDER serial;
    NSSUTF8 *nickname;
    NSSASCII7 *email;
    NSSSlot *slot;
    NSSToken *token;
    NSSTrustDomain *trustDomain;
    NSSCryptoContext *cryptoContext;
    NSSTrust trust;
    CK_OBJECT_HANDLE handle;
    nssDecodedCert *decoding;
};

struct NSSPrivateKeyStr;

struct NSSPublicKeyStr;

struct NSSSymmetricKeyStr;

typedef struct nssTDCertificateCacheStr nssTDCertificateCache;

struct NSSTrustDomainStr {
    PRInt32 refCount;
    NSSArena *arena;
    NSSCallback defaultCallback;
    nssList *tokenList;
    nssListIterator *tokens;
    nssTDCertificateCache *cache;
#ifdef NSS_3_4_CODE
    void *spkDigestInfo;
    CERTStatusConfig *statusConfig;
#endif
};

struct NSSCryptoContextStr
{
    PRInt32 refCount;
    NSSArena *arena;
};

struct NSSTimeStr;

struct NSSPoliciesStr;

struct NSSAlgorithmAndParametersStr;

struct NSSPKIXCertificateStr;

PR_END_EXTERN_C

#endif /* PKIT_H */
