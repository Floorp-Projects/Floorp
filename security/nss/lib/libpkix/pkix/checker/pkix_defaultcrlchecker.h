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
 * pkix_defaultcrlchecker.h
 *
 * Header file for default CRL function
 *
 */

#ifndef _PKIX_DEFAULTCRLCHECKER_H
#define _PKIX_DEFAULTCRLCHECKER_H

#include "pkix_tools.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pkix_DefaultCRLCheckerState pkix_DefaultCRLCheckerState;

struct pkix_DefaultCRLCheckerState {
        PKIX_List *certStores; /* list of CertStore */
        PKIX_PL_Date *testDate;
        PKIX_Boolean certHasValidCrl;
        PKIX_Boolean nistCRLPolicyEnabled;
        PKIX_Boolean prevCertCrlSign;
        PKIX_PL_PublicKey *prevPublicKey; /* Subject PubKey of last cert */
        PKIX_List *prevPublicKeyList; /* of PKIX_PL_PublicKey */
        PKIX_UInt32 reasonCodeMask;
        PKIX_UInt32 certsRemaining;
        PKIX_PL_OID *crlReasonCodeOID;

        PKIX_PL_X500Name *certIssuer;
        PKIX_PL_BigInt *certSerialNumber;
        PKIX_CRLSelector *crlSelector;
        PKIX_UInt32 crlStoreIndex;
        PKIX_UInt32 numCrlStores;
};

PKIX_Error *
pkix_DefaultCRLChecker_Initialize(
        PKIX_List *certStores,
        PKIX_PL_Date *testDate,
        PKIX_PL_PublicKey *trustedPubKey,
        PKIX_UInt32 certsRemaining,
        PKIX_Boolean nistCRLPolicyEnabled,
        PKIX_CertChainChecker **pChecker,
        void *plContext);

PKIX_Error *
pkix_DefaultCRLChecker_Check_Helper(
        PKIX_CertChainChecker *checker,
        PKIX_PL_Cert *cert,
        PKIX_PL_PublicKey *prevPublicKey,
        pkix_DefaultCRLCheckerState *state,
        PKIX_List *unresolvedCriticalExtensions,
        PKIX_Boolean useOnlyLocal,
        void **pNBIOContext,
        void *plContext);

PKIX_Error *
pkix_DefaultCRLChecker_Check_SetSelector(
        PKIX_PL_Cert *cert,
        pkix_DefaultCRLCheckerState *state,
        void *plContext);

PKIX_Error *
pkix_DefaultCRLCheckerState_RegisterSelf(void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_DEFAULTCRLCHECKER_H */
