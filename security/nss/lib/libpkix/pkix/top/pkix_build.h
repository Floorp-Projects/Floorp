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
 * The Original Code is the PKIX-C library.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are
 * Copyright 2004-2007 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems, Inc.
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
/*
 * pkix_build.h
 *
 * Header file for buildChain function
 *
 */

#ifndef _PKIX_BUILD_H
#define _PKIX_BUILD_H
#include "pkix_tools.h"
#include "pkix_pl_ldapt.h"
#include "pkix_ekuchecker.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
        BUILD_SHORTCUTPENDING,
        BUILD_INITIAL,
        BUILD_TRYAIA,
        BUILD_AIAPENDING,
        BUILD_COLLECTINGCERTS,
        BUILD_GATHERPENDING,
        BUILD_CERTVALIDATING,
        BUILD_ABANDONNODE,
        BUILD_CRLPREP,
        BUILD_CRL1,
        BUILD_DATEPREP,
        BUILD_CHECKTRUSTED,
        BUILD_CHECKTRUSTED2,
        BUILD_ADDTOCHAIN,
        BUILD_CRL2PREP,
        BUILD_CRL2,
        BUILD_VALCHAIN,
        BUILD_VALCHAIN2,
        BUILD_EXTENDCHAIN,
        BUILD_GETNEXTCERT
} BuildStatus;

typedef struct BuildConstantsStruct BuildConstants;

/*
 * These fields (the ones that are objects) are not reference-counted
 * in *each* state, but only in the root, the state that has no parent.
 * That saves time in creation and destruction of child states, but is
 * safe enough since they are constants.
 */
struct BuildConstantsStruct {
        PKIX_UInt32 numAnchors;
        PKIX_UInt32 numCertStores;
        PKIX_UInt32 numHintCerts;
        PKIX_UInt32 maxDepth;
        PKIX_UInt32 maxFanout;
        PKIX_UInt32 maxTime;
        PKIX_ProcessingParams *procParams;
        PKIX_PL_Date *testDate;
        PKIX_PL_Date *timeLimit;
        PKIX_PL_Cert *targetCert;
        PKIX_PL_PublicKey *targetPubKey;
        PKIX_List *certStores;
        PKIX_List *anchors;
        PKIX_List *userCheckers;
        PKIX_List *hintCerts;
        PKIX_RevocationChecker *revChecker;
        PKIX_PL_AIAMgr *aiaMgr;
        PKIX_Boolean useAIAForCertFetching;
};

struct PKIX_ForwardBuilderStateStruct{
        BuildStatus status;
        PKIX_Int32 traversedCACerts;
        PKIX_UInt32 certStoreIndex;
        PKIX_UInt32 numCerts;
        PKIX_UInt32 numAias;
        PKIX_UInt32 certIndex;
        PKIX_UInt32 aiaIndex;
        PKIX_UInt32 certCheckedIndex;
        PKIX_UInt32 checkerIndex;
        PKIX_UInt32 hintCertIndex;
        PKIX_UInt32 numFanout;
        PKIX_UInt32 numDepth;
        PKIX_UInt32 reasonCode;
        PKIX_Boolean revCheckDelayed;
        PKIX_Boolean canBeCached;
        PKIX_Boolean useOnlyLocal;
        PKIX_Boolean revChecking;
        PKIX_Boolean usingHintCerts;
        PKIX_Boolean certLoopingDetected;
        PKIX_PL_Date *validityDate;
        PKIX_PL_Cert *prevCert;
        PKIX_PL_Cert *candidateCert;
        PKIX_List *traversedSubjNames;
        PKIX_List *trustChain;
        PKIX_List *aia;
        PKIX_List *candidateCerts;
        PKIX_List *reversedCertChain;
        PKIX_List *checkedCritExtOIDs;
        PKIX_List *checkerChain;
        PKIX_CertSelector *certSel;
        PKIX_VerifyNode *verifyNode;
        void *client; /* messageHandler, such as LDAPClient */
        PKIX_ForwardBuilderState *parentState;
        BuildConstants buildConstants;
};

/* --Private-Functions-------------------------------------------- */

PKIX_Error *
pkix_ForwardBuilderState_RegisterSelf(void *plContext);

PKIX_Error *
PKIX_Build_GetNBIOContext(void *state, void **pNBIOContext, void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_BUILD_H */
