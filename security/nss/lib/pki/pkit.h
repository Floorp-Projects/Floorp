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
static const char PKIT_CVS_ID[] = "@(#) $RCSfile: pkit.h,v $ $Revision: 1.10 $ $Date: 2002/02/04 22:34:22 $ $Name:  $";
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
#endif /* NSS_3_4_CODE */

#ifndef NSSPKIT_H
#include "nsspkit.h"
#endif /* NSSPKIT_H */

#ifndef NSSDEVT_H
#include "nssdevt.h"
#endif /* NSSDEVT_H */

#ifndef DEVT_H
#include "devt.h"
#endif /* DEVT_H */

PR_BEGIN_EXTERN_C

typedef struct nssDecodedCertStr nssDecodedCert;

typedef struct nssCertificateStoreStr nssCertificateStore;

/* How wide is the scope of this? */
typedef struct nssSMIMEProfileStr nssSMIMEProfile;

/*
 * A note on ephemeral certs
 *
 * The key objects defined here can only be created on tokens, and can only
 * exist on tokens.  Therefore, any instance of a key object must have
 * a corresponding cryptoki instance.  OTOH, certificates created in 
 * crypto contexts need not be stored as session objects on the token.
 * There are good performance reasons for not doing so.  The certificate
 * and trust objects have been defined with a cryptoContext field to
 * allow for ephemeral certs, which may have a single instance in a crypto
 * context along with any number (including zero) of cryptoki instances.
 * Since contexts may not share objects, there can be only one context
 * for each object.
 */

/* The common data from which all objects inherit */
struct nssPKIObjectBaseStr 
{
    /* The arena for all object memory */
    NSSArena *arena;
    /* Thread-safe reference counting */
    PZLock *lock;
    PRInt32 refCount;
    /* List of nssCryptokiInstance's of the object */
    nssList *instanceList;
    nssListIterator *instances;
    /* The object must live in a trust domain */
    NSSTrustDomain *trustDomain;
    /* The object may live in a crypto context */
    NSSCryptoContext *cryptoContext;
};

struct NSSTrustStr 
{
    struct nssPKIObjectBaseStr object;
    NSSCertificate *certificate;
    nssTrustLevel serverAuth;
    nssTrustLevel clientAuth;
    nssTrustLevel emailProtection;
    nssTrustLevel codeSigning;
};

struct nssSMIMEProfileStr
{
    struct nssPKIObjectBaseStr object;
    NSSCertificate *certificate;
    NSSASCII7 *email;
    NSSDER *subject;
    NSSItem *profileTime;
    NSSItem *profileData;
};

struct NSSCertificateStr
{
    struct nssPKIObjectBaseStr object;
    NSSCertificateType type;
    NSSItem id;
    NSSBER encoding;
    NSSDER issuer;
    NSSDER subject;
    NSSDER serial;
    NSSUTF8 *nickname;
    NSSASCII7 *email;
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
    NSSTrustDomain *td;
    NSSToken *token;
    nssSession *session;
    nssCertificateStore *certStore;
};

struct NSSTimeStr {
    PRTime prTime;
};

struct NSSPoliciesStr;

struct NSSAlgorithmAndParametersStr;

struct NSSPKIXCertificateStr;

PR_END_EXTERN_C

#endif /* PKIT_H */
