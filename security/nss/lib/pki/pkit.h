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

#ifndef PKIT_H
#define PKIT_H

#ifdef DEBUG
static const char PKIT_CVS_ID[] = "@(#) $RCSfile: pkit.h,v $ $Revision: 1.16 $ $Date: 2004/07/29 23:38:14 $ $Name:  $";
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

#ifndef nssrwlkt_h__
#include "nssrwlkt.h"
#endif /* nssrwlkt_h__ */

PR_BEGIN_EXTERN_C

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

/* nssPKIObject
 *
 * This is the base object class, common to all PKI objects defined in
 * nsspkit.h
 */
struct nssPKIObjectStr 
{
    /* The arena for all object memory */
    NSSArena *arena;
    /* Atomically incremented/decremented reference counting */
    PRInt32 refCount;
    /* lock protects the array of nssCryptokiInstance's of the object */
    PZLock *lock;
    /* XXX with LRU cache, this cannot be guaranteed up-to-date.  It cannot
     * be compared against the update level of the trust domain, since it is
     * also affected by import/export.  Where is this array needed?
     */
    nssCryptokiObject **instances;
    PRUint32 numInstances;
    /* The object must live in a trust domain */
    NSSTrustDomain *trustDomain;
    /* The object may live in a crypto context */
    NSSCryptoContext *cryptoContext;
    /* XXX added so temp certs can have nickname, think more ... */
    NSSUTF8 *tempName;
};

typedef struct nssDecodedCertStr nssDecodedCert;

typedef struct nssCertificateStoreStr nssCertificateStore;

/* How wide is the scope of this? */
typedef struct nssSMIMEProfileStr nssSMIMEProfile;

typedef struct nssPKIObjectStr nssPKIObject;

struct NSSTrustStr 
{
    nssPKIObject object;
    NSSCertificate *certificate;
    nssTrustLevel serverAuth;
    nssTrustLevel clientAuth;
    nssTrustLevel emailProtection;
    nssTrustLevel codeSigning;
    PRBool stepUpApproved;
};

struct nssSMIMEProfileStr
{
    nssPKIObject object;
    NSSCertificate *certificate;
    NSSASCII7 *email;
    NSSDER *subject;
    NSSItem *profileTime;
    NSSItem *profileData;
};

struct NSSCertificateStr
{
    nssPKIObject object;
    NSSCertificateType type;
    NSSItem id;
    NSSBER encoding;
    NSSDER issuer;
    NSSDER subject;
    NSSDER serial;
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
    NSSCallback *defaultCallback;
    nssList *tokenList;
    nssListIterator *tokens;
    nssTDCertificateCache *cache;
    NSSRWLock *tokensLock;
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

struct NSSCRLStr {
  nssPKIObject object;
  NSSDER encoding;
  NSSUTF8 *url;
  PRBool isKRL;
};

typedef struct NSSCRLStr NSSCRL;

struct NSSPoliciesStr;

struct NSSAlgorithmAndParametersStr;

struct NSSPKIXCertificateStr;

PR_END_EXTERN_C

#endif /* PKIT_H */
