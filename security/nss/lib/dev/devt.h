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

#ifndef DEVT_H
#define DEVT_H

#ifdef DEBUG
static const char DEVT_CVS_ID[] = "@(#) $RCSfile: devt.h,v $ $Revision: 1.21 $ $Date: 2004/05/17 20:08:37 $ $Name:  $";
#endif /* DEBUG */

/*
 * devt.h
 *
 * This file contains definitions for the low-level cryptoki devices.
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

#ifndef NSSPKIT_H
#include "nsspkit.h"
#endif /* NSSPKIT_H */

#ifndef NSSDEVT_H
#include "nssdevt.h"
#endif /* NSSDEVT_H */

#ifndef NSSCKT_H
#include "nssckt.h"
#endif /* NSSCKT_H */

#ifndef BASET_H
#include "baset.h"
#endif /* BASET_H */

#ifdef NSS_3_4_CODE
#include "secmodt.h"
#endif /* NSS_3_4_CODE */

PR_BEGIN_EXTERN_C

typedef struct nssSessionStr nssSession;

/* XXX until NSSTokenStr is moved */
struct nssDeviceBaseStr
{
  NSSArena *arena;
  PZLock *lock;
  PRInt32 refCount;
  NSSUTF8 *name;
  PRUint32 flags;
};

typedef struct nssTokenObjectCacheStr nssTokenObjectCache;

/* XXX until devobject.c goes away */
struct NSSTokenStr
{
    struct nssDeviceBaseStr base;
    NSSSlot *slot;  /* Parent (or peer, if you will) */
    CK_FLAGS ckFlags; /* from CK_TOKEN_INFO.flags */
    PRUint32 flags;
    void *epv;
    nssSession *defaultSession;
    NSSTrustDomain *trustDomain;
    PRIntervalTime lastTime;
    nssTokenObjectCache *cache;
#ifdef NSS_3_4_CODE
    PK11SlotInfo *pk11slot;
#endif
};

typedef enum {
  nssSlotAskPasswordTimes_FirstTime = 0,
  nssSlotAskPasswordTimes_EveryTime = 1,
  nssSlotAskPasswordTimes_Timeout = 2
} 
nssSlotAskPasswordTimes;

struct nssSlotAuthInfoStr
{
  PRTime lastLogin;
  nssSlotAskPasswordTimes askTimes;
  PRIntervalTime askPasswordTimeout;
};

struct NSSSlotStr
{
  struct nssDeviceBaseStr base;
  NSSModule *module; /* Parent */
  NSSToken *token;  /* Peer */
  CK_SLOT_ID slotID;
  CK_FLAGS ckFlags; /* from CK_SLOT_INFO.flags */
  struct nssSlotAuthInfoStr authInfo;
  PRIntervalTime lastTokenPing;
  PZLock *lock;
#ifdef NSS_3_4_CODE
  void *epv;
  PK11SlotInfo *pk11slot;
#endif
};

struct nssSessionStr
{
  PZLock *lock;
  CK_SESSION_HANDLE handle;
  NSSSlot *slot;
  PRBool isRW;
  PRBool ownLock;
};

typedef enum {
    NSSCertificateType_Unknown = 0,
    NSSCertificateType_PKIX = 1
} NSSCertificateType;

typedef enum {
    nssTrustLevel_Unknown = 0,
    nssTrustLevel_NotTrusted = 1,
    nssTrustLevel_Trusted = 2,
    nssTrustLevel_TrustedDelegator = 3,
    nssTrustLevel_Valid = 4,
    nssTrustLevel_ValidDelegator = 5
} nssTrustLevel;

typedef struct nssCryptokiInstanceStr nssCryptokiInstance;

struct nssCryptokiInstanceStr
{
    CK_OBJECT_HANDLE handle;
    NSSToken *token;
    PRBool isTokenObject;
    NSSUTF8 *label;
};

typedef struct nssCryptokiInstanceStr nssCryptokiObject;

typedef struct nssTokenCertSearchStr nssTokenCertSearch;

typedef enum {
    nssTokenSearchType_AllObjects = 0,
    nssTokenSearchType_SessionOnly = 1,
    nssTokenSearchType_TokenOnly = 2,
    nssTokenSearchType_TokenForced = 3
} nssTokenSearchType;

struct nssTokenCertSearchStr
{
    nssTokenSearchType searchType;
    PRStatus (* callback)(NSSCertificate *c, void *arg);
    void *cbarg;
    nssList *cached;
    /* TODO: add a cache query callback if the list would be large 
     *       (traversal) 
     */
};

struct nssSlotListStr;
typedef struct nssSlotListStr nssSlotList;

struct NSSAlgorithmAndParametersStr
{
    CK_MECHANISM mechanism;
};

PR_END_EXTERN_C

#endif /* DEVT_H */
