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
 * pkix_pl_crldp.c
 *
 * Crl DP Object Functions
 *
 */

#include "pkix_pl_crldp.h"

static PKIX_Error *
pkix_pl_CrlDp_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
    pkix_pl_CrlDp *crldp = NULL;
    
    PKIX_ENTER(CRLCHECKER, "pkix_CrlDp_Destroy");
    PKIX_NULLCHECK_ONE(object);
    
    /* Check that this object is a default CRL checker state */
    PKIX_CHECK(
        pkix_CheckType(object, PKIX_CRLDP_TYPE, plContext),
        PKIX_OBJECTNOTCRLCHECKER);
    
    crldp = (pkix_pl_CrlDp *)object;
    if (crldp->distPointType == relativeDistinguishedName) {
        CERT_DestroyName(crldp->name.issuerName);
        crldp->name.issuerName = NULL;
    }
    crldp->nssdp = NULL;
cleanup:
    PKIX_RETURN(CRLCHECKER);
}

/*
 * FUNCTION: pkix_pl_CrlDp_RegisterSelf
 *
 * DESCRIPTION:
 *  Registers PKIX_CRLDP_TYPE and its related functions
 *  with systemClasses[]
 *
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_CrlDp_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry* entry = &systemClasses[PKIX_CRLDP_TYPE];

        PKIX_ENTER(CRLCHECKER, "pkix_CrlDp_RegisterSelf");

        entry->description = "CrlDistPoint";
        entry->typeObjectSize = sizeof(pkix_pl_CrlDp);
        entry->destructor = pkix_pl_CrlDp_Destroy;
        entry->duplicateFunction = pkix_duplicateImmutable;

        PKIX_RETURN(CRLCHECKER);
}



PKIX_Error *
pkix_pl_CrlDp_Create(
    const CRLDistributionPoint *dp,
    const CERTName *certIssuerName,
    pkix_pl_CrlDp **pPkixDP,
    void *plContext)
{
    PRArenaPool *rdnArena = NULL;
    CERTName *issuerNameCopy = NULL;
    pkix_pl_CrlDp *dpl = NULL;

    /* Need to save the following info to update crl cache:
     * - reasons if partitioned(but can not return revocation check
     *   success if not all crl are downloaded)
     * - issuer name if different from issuer of the cert
     * - url to upload a crl if needed.
     * */
    PKIX_ENTER(CRLDP, "pkix_pl_CrlDp_Create");
    PKIX_NULLCHECK_ONE(dp);

    PKIX_CHECK(
        PKIX_PL_Object_Alloc(PKIX_CRLDP_TYPE,
                             sizeof (pkix_pl_CrlDp),
                             (PKIX_PL_Object **)&dpl,
                             plContext),
        PKIX_COULDNOTCREATEOBJECT);

    dpl->nssdp = dp;
    dpl->isPartitionedByReasonCode = PKIX_FALSE;
    if (dp->reasons.data) {
        dpl->isPartitionedByReasonCode = PKIX_TRUE;
    }
    if (dp->distPointType == generalName) {
        dpl->distPointType = generalName;
        dpl->name.fullName = dp->distPoint.fullName;
    } else {
        SECStatus rv;
        const CERTName *issuerName = NULL;
        const CERTRDN *relName = &dp->distPoint.relativeName;

        if (dp->crlIssuer) {
            if (dp->crlIssuer->l.next) {
                /* Violate RFC 5280: in this case crlIssuer
                 * should have only one name and should be
                 * a distinguish name. */
                PKIX_ERROR(PKIX_NOTCONFORMINGCRLDP);
            }
            issuerName = &dp->crlIssuer->name.directoryName;
        } else {
            issuerName = certIssuerName;
        }
        rdnArena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
        if (!rdnArena) {
            PKIX_ERROR(PKIX_PORTARENAALLOCFAILED);
        }
        issuerNameCopy = (CERTName *)PORT_ArenaZNew(rdnArena, CERTName*);
        if (!issuerNameCopy) {
            PKIX_ERROR(PKIX_ALLOCERROR);
        }
        rv = CERT_CopyName(rdnArena, issuerNameCopy, (CERTName*)issuerName);
        if (rv == SECFailure) {
            PKIX_ERROR(PKIX_ALLOCERROR);
        }
        rv = CERT_AddRDN(issuerNameCopy, (CERTRDN*)relName);
        if (rv == SECFailure) {
            PKIX_ERROR(PKIX_ALLOCERROR);
        }
        dpl->distPointType = relativeDistinguishedName;
        dpl->name.issuerName = issuerNameCopy;
        rdnArena = NULL;
    }
    *pPkixDP = dpl;
    dpl = NULL;

cleanup:
    if (rdnArena) {
        PORT_FreeArena(rdnArena, PR_FALSE);
    }
    PKIX_DECREF(dpl);

    PKIX_RETURN(CRLDP);
}
