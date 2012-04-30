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
 * pkix_signaturechecker.c
 *
 * Functions for signature validation
 *
 */

#include "pkix_signaturechecker.h"

/*
 * FUNCTION: pkix_SignatureCheckerstate_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_SignatureCheckerState_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        pkix_SignatureCheckerState *state = NULL;

        PKIX_ENTER(SIGNATURECHECKERSTATE,
                    "pkix_SignatureCheckerState_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a signature checker state */
        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_SIGNATURECHECKERSTATE_TYPE, plContext),
                    PKIX_OBJECTNOTSIGNATURECHECKERSTATE);

        state = (pkix_SignatureCheckerState *) object;

        state->prevCertCertSign = PKIX_FALSE;

        PKIX_DECREF(state->prevPublicKey);
        PKIX_DECREF(state->prevPublicKeyList);
        PKIX_DECREF(state->keyUsageOID);

cleanup:

        PKIX_RETURN(SIGNATURECHECKERSTATE);
}

/*
 * FUNCTION: pkix_SignatureCheckerState_RegisterSelf
 *
 * DESCRIPTION:
 *  Registers PKIX_SIGNATURECHECKERSTATE_TYPE and its related functions
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
pkix_SignatureCheckerState_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(SIGNATURECHECKERSTATE,
                    "pkix_SignatureCheckerState_RegisterSelf");

        entry.description = "SignatureCheckerState";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(pkix_SignatureCheckerState);
        entry.destructor = pkix_SignatureCheckerState_Destroy;
        entry.equalsFunction = NULL;
        entry.hashcodeFunction = NULL;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_SIGNATURECHECKERSTATE_TYPE] = entry;

        PKIX_RETURN(SIGNATURECHECKERSTATE);
}

/*
 * FUNCTION: pkix_SignatureCheckerState_Create
 *
 * DESCRIPTION:
 *  Allocate and initialize SignatureChecker state data.
 *
 * PARAMETERS
 *  "trustedPubKey"
 *      Address of trusted Anchor Public Key for verifying first Cert in the
 *      chain. Must be non-NULL.
 *  "certsRemaining"
 *      Number of certificates remaining in the chain.
 *  "pCheckerState"
 *      Address where SignatureCheckerState will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 *
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 *
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a SignatureCheckerState Error if the function fails in a
 *  non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_SignatureCheckerState_Create(
    PKIX_PL_PublicKey *trustedPubKey,
    PKIX_UInt32 certsRemaining,
    pkix_SignatureCheckerState **pCheckerState,
    void *plContext)
{
        pkix_SignatureCheckerState *state = NULL;
        PKIX_PL_OID *keyUsageOID = NULL;

        PKIX_ENTER(SIGNATURECHECKERSTATE, "pkix_SignatureCheckerState_Create");
        PKIX_NULLCHECK_TWO(trustedPubKey, pCheckerState);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_SIGNATURECHECKERSTATE_TYPE,
                    sizeof (pkix_SignatureCheckerState),
                    (PKIX_PL_Object **)&state,
                    plContext),
                    PKIX_COULDNOTCREATESIGNATURECHECKERSTATEOBJECT);

        /* Initialize fields */

        state->prevCertCertSign = PKIX_TRUE;
        state->prevPublicKeyList = NULL;
        state->certsRemaining = certsRemaining;

        PKIX_INCREF(trustedPubKey);
        state->prevPublicKey = trustedPubKey;

        PKIX_CHECK(PKIX_PL_OID_Create
                    (PKIX_CERTKEYUSAGE_OID,
                    &keyUsageOID,
                    plContext),
                    PKIX_OIDCREATEFAILED);

        state->keyUsageOID = keyUsageOID;
        keyUsageOID = NULL;

        *pCheckerState = state;
        state = NULL;

cleanup:

        PKIX_DECREF(keyUsageOID);
        PKIX_DECREF(state); 

        PKIX_RETURN(SIGNATURECHECKERSTATE);
}

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_SignatureChecker_Check
 * (see comments for PKIX_CertChainChecker_CheckCallback in pkix_checker.h)
 */
PKIX_Error *
pkix_SignatureChecker_Check(
        PKIX_CertChainChecker *checker,
        PKIX_PL_Cert *cert,
        PKIX_List *unresolvedCriticalExtensions,
        void **pNBIOContext,
        void *plContext)
{
        pkix_SignatureCheckerState *state = NULL;
        PKIX_PL_PublicKey *prevPubKey = NULL;
        PKIX_PL_PublicKey *currPubKey = NULL;
        PKIX_PL_PublicKey *newPubKey = NULL;
        PKIX_PL_PublicKey *pKey = NULL;
        PKIX_PL_CertBasicConstraints *basicConstraints = NULL;
        PKIX_Error *checkKeyUsageFail = NULL;
        PKIX_Error *verifyFail = NULL;
        PKIX_Boolean certVerified = PKIX_FALSE;

        PKIX_ENTER(CERTCHAINCHECKER, "pkix_SignatureChecker_Check");
        PKIX_NULLCHECK_THREE(checker, cert, pNBIOContext);

        *pNBIOContext = NULL; /* we never block on pending I/O */

        PKIX_CHECK(PKIX_CertChainChecker_GetCertChainCheckerState
                    (checker, (PKIX_PL_Object **)&state, plContext),
                    PKIX_CERTCHAINCHECKERGETCERTCHAINCHECKERSTATEFAILED);

        (state->certsRemaining)--;

        PKIX_INCREF(state->prevPublicKey);
        prevPubKey = state->prevPublicKey;

        /*
         * Previous Cert doesn't have CertSign bit on for signature
         * verification and it is not a self-issued Cert so there is no
         * old key saved. This is considered error.
         */
        if (state->prevCertCertSign == PKIX_FALSE &&
                state->prevPublicKeyList == NULL) {
                    PKIX_ERROR(PKIX_KEYUSAGEKEYCERTSIGNBITNOTON);
        }

        /* Previous Cert is valid for signature verification, try it first */
        if (state->prevCertCertSign == PKIX_TRUE) {
                verifyFail = PKIX_PL_Cert_VerifySignature
                        (cert, prevPubKey, plContext);
                if (verifyFail == NULL) {
                        certVerified = PKIX_TRUE;
                } else {
                        certVerified = PKIX_FALSE;
                }
        }

#ifdef NIST_TEST_4_5_4_AND_4_5_6

        /*
         * Following codes under this compiler flag is implemented for
         * special cases of NIST tests 4.5.4 and 4.5.6. We are not sure
         * we should handle these two tests as what is implemented so the
         * codes are commented out, and the tests fails (for now).
         * For Cert chain validation, our assumption is all the Certs on
         * the chain are using its previous Cert's public key to decode
         * its current key. But for thses two tests, keys are used not
         * in this precedent order, we can either
         * 1) Use what is implemented here: take in what Cert order NIST
         *    specified and for continuous self-issued Certs, stacking up
         *    their keys and tries all of them in FILO order.
         *    But this method breaks the idea of chain key presdency.
         * 2) Use Build Chain facility: we will specify the valid Certs
         *    order (means key precedency is kept) and count on Build Chain
         *    to get the Certs that can fill for the needed keys. This may have
         *    performance impact.
         * 3) Fetch Certs from CertStore: we will specifiy the valid Certs
         *    order and use CertSelector on SubjectName to get a list of
         *    candidates Certs to fill in for the needed keys.
         * Anyhow, the codes are kept around just in case we want to use
         * solution one...
         */

        /* If failed and previous key is self-issued, try its old key(s) */
        if (certVerified == PKIX_FALSE && state->prevPublicKeyList != NULL) {

                /* Verify from keys on the list */
                PKIX_CHECK(PKIX_List_GetLength
                        (state->prevPublicKeyList, &numKeys, plContext),
                        PKIX_LISTGETLENGTHFAILED);

                for (i = numKeys - 1; i >= 0; i--) {

                        PKIX_CHECK(PKIX_List_GetItem
                                (state->prevPublicKeyList,
                                i,
                                (PKIX_PL_Object **) &pKey,
                                plContext),
                                PKIX_LISTGETITEMFAILED);

                        PKIX_DECREF(verifyFail);
                        verifyFail = PKIX_PL_Cert_VerifySignature
                                (cert, pKey, plContext);

                        if (verifyFail == NULL) {
                                certVerified = PKIX_TRUE;
                                break;
                        } else {
                                certVerified = PKIX_FALSE;
                        }

                        PKIX_DECREF(pKey);
                }
        }
#endif

        if (certVerified == PKIX_FALSE) {
                pkixErrorResult = verifyFail;
                verifyFail = NULL;
                PKIX_ERROR(PKIX_VALIDATIONFAILEDCERTSIGNATURECHECKING);
        }

#ifdef NIST_TEST_4_5_4_AND_4_5_6
        /*
         * Check if Cert is self-issued. If so, the old key(s) is saved, in
         * conjunction to the new key, for verifying CERT validity later.
         */
        PKIX_CHECK(pkix_IsCertSelfIssued(cert, &selfIssued, plContext),
                    PKIX_ISCERTSELFISSUEFAILED);

        /*
         * Check if Cert is self-issued. If so, the public key of the Cert
         * that issues this Cert (old key) can be used together with this
         * current key (new key) for key verification. If there are multiple
         * self-issued certs, keys of those Certs (old keys) can also be used
         * for key verification. Old key(s) is saved in a list (PrevPublickKey-
         * List) and cleared when a Cert is no longer self-issued. PrevPublic-
         * Key keep key of the previous Cert.
         */
        if (selfIssued == PKIX_TRUE) {

            /* Make sure previous Cert is valid for signature verification */
            if (state->prevCertCertSign == PKIX_TRUE) {

                if (state->prevPublicKeyList == NULL) {

                        PKIX_CHECK(PKIX_List_Create
                                (&state->prevPublicKeyList, plContext),
                                PKIX_LISTCREATEFALIED);

                }

                PKIX_CHECK(PKIX_List_AppendItem
                            (state->prevPublicKeyList,
                            (PKIX_PL_Object *) state->prevPublicKey,
                            plContext),
                            PKIX_LISTAPPENDITEMFAILED);
            }

        } else {
            /* Not self-issued Cert any more, clear old key(s) saved */
            PKIX_DECREF(state->prevPublicKeyList);
        }
#endif

        /* Save current key as prevPublicKey */
        PKIX_CHECK(PKIX_PL_Cert_GetSubjectPublicKey
                    (cert, &currPubKey, plContext),
                    PKIX_CERTGETSUBJECTPUBLICKEYFAILED);

        PKIX_CHECK(PKIX_PL_PublicKey_MakeInheritedDSAPublicKey
                    (currPubKey, prevPubKey, &newPubKey, plContext),
                    PKIX_PUBLICKEYMAKEINHERITEDDSAPUBLICKEYFAILED);

        if (newPubKey == NULL){
                PKIX_INCREF(currPubKey);
                newPubKey = currPubKey;
        }

        PKIX_INCREF(newPubKey);
        PKIX_DECREF(state->prevPublicKey);

        state->prevPublicKey = newPubKey;

        /* Save this Cert key usage CertSign bit */
        if (state->certsRemaining != 0) {
                checkKeyUsageFail = PKIX_PL_Cert_VerifyKeyUsage
                        (cert, PKIX_KEY_CERT_SIGN, plContext);

                state->prevCertCertSign = (checkKeyUsageFail == NULL)?
                        PKIX_TRUE:PKIX_FALSE;

                PKIX_DECREF(checkKeyUsageFail);
        }

        /* Remove Key Usage Extension OID from list */
        if (unresolvedCriticalExtensions != NULL) {

                PKIX_CHECK(pkix_List_Remove
                            (unresolvedCriticalExtensions,
                            (PKIX_PL_Object *) state->keyUsageOID,
                            plContext),
                            PKIX_LISTREMOVEFAILED);
        }

        PKIX_CHECK(PKIX_CertChainChecker_SetCertChainCheckerState
                    (checker, (PKIX_PL_Object *)state, plContext),
                    PKIX_CERTCHAINCHECKERSETCERTCHAINCHECKERSTATEFAILED);

cleanup:

        PKIX_DECREF(state);
        PKIX_DECREF(pKey);
        PKIX_DECREF(prevPubKey);
        PKIX_DECREF(currPubKey);
        PKIX_DECREF(newPubKey);
        PKIX_DECREF(basicConstraints);
        PKIX_DECREF(verifyFail);
        PKIX_DECREF(checkKeyUsageFail);

        PKIX_RETURN(CERTCHAINCHECKER);

}

/*
 * FUNCTION: pkix_SignatureChecker_Initialize
 * DESCRIPTION:
 *
 *  Creates a new CertChainChecker and stores it at "pChecker", where it will
 *  be used by pkix_SignatureChecker_Check to check that the public key in
 *  the checker's state is able to successfully validate the certificate's
 *  signature. The PublicKey pointed to by "trustedPubKey" is used to
 *  initialize the checker's state.
 *
 * PARAMETERS:
 *  "trustedPubKey"
 *      Address of PublicKey representing the trusted public key used to
 *      initialize the state of this checker. Must be non-NULL.
 *  "certsRemaining"
 *      Number of certificates remaining in the chain.
 *  "pChecker"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertChainChecker Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_SignatureChecker_Initialize(
        PKIX_PL_PublicKey *trustedPubKey,
        PKIX_UInt32 certsRemaining,
        PKIX_CertChainChecker **pChecker,
        void *plContext)
{
        pkix_SignatureCheckerState* state = NULL;
        PKIX_ENTER(CERTCHAINCHECKER, "PKIX_SignatureChecker_Initialize");
        PKIX_NULLCHECK_TWO(pChecker, trustedPubKey);

        PKIX_CHECK(pkix_SignatureCheckerState_Create
                    (trustedPubKey, certsRemaining, &state, plContext),
                    PKIX_SIGNATURECHECKERSTATECREATEFAILED);

        PKIX_CHECK(PKIX_CertChainChecker_Create
                    (pkix_SignatureChecker_Check,
                    PKIX_FALSE,
                    PKIX_FALSE,
                    NULL,
                    (PKIX_PL_Object *) state,
                    pChecker,
                    plContext),
                    PKIX_CERTCHAINCHECKERCREATEFAILED);

cleanup:

        PKIX_DECREF(state);

        PKIX_RETURN(CERTCHAINCHECKER);

}
