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
 * pkix_pl_pk11certstore.c
 *
 * PKCS11CertStore Function Definitions
 *
 */

#include "pkix_pl_pk11certstore.h"

/* --Private-Pk11CertStore-Functions---------------------------------- */

/*
 * FUNCTION: pkix_pl_Pk11CertStore_CheckTrust
 * DESCRIPTION:
 * This function checks the trust status of this "cert" that was retrieved
 * from the CertStore "store" and returns its trust status at "pTrusted".
 *
 * PARAMETERS:
 * "store"
 *      Address of the CertStore. Must be non-NULL.
 * "cert"
 *      Address of the Cert. Must be non-NULL.
 * "pTrusted"
 *      Address of PKIX_Boolean where the "cert" trust status is returned.
 *      Must be non-NULL.
 * "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Pk11CertStore_CheckTrust(
        PKIX_CertStore *store,
        PKIX_PL_Cert *cert,
        PKIX_Boolean *pTrusted,
        void *plContext)
{
        SECStatus rv = SECFailure;
        PKIX_Boolean trusted = PKIX_FALSE;
        SECCertUsage certUsage = 0;
        SECCertificateUsage certificateUsage;
        unsigned int requiredFlags;
        SECTrustType trustType;
        CERTCertTrust trust;

        PKIX_ENTER(CERTSTORE, "pkix_pl_Pk11CertStore_CheckTrust");
        PKIX_NULLCHECK_THREE(store, cert, pTrusted);
        PKIX_NULLCHECK_ONE(cert->nssCert);

        certificateUsage = ((PKIX_PL_NssContext*)plContext)->certificateUsage;

        /* ensure we obtained a single usage bit only */
        PORT_Assert(!(certificateUsage & (certificateUsage - 1)));

        /* convert SECertificateUsage (bit mask) to SECCertUsage (enum) */
        while (0 != (certificateUsage = certificateUsage >> 1)) { certUsage++; }

        rv = CERT_TrustFlagsForCACertUsage(certUsage, &requiredFlags, &trustType);
        if (rv == SECSuccess) {
                rv = CERT_GetCertTrust(cert->nssCert, &trust);
        }

        if (rv == SECSuccess) {
            unsigned int certFlags;

            if (certUsage != certUsageAnyCA &&
                certUsage != certUsageStatusResponder) {
                CERTCertificate *nssCert = cert->nssCert;
                
                if (certUsage == certUsageVerifyCA) {
                    if (nssCert->nsCertType & NS_CERT_TYPE_EMAIL_CA) {
                        trustType = trustEmail;
                    } else if (nssCert->nsCertType & NS_CERT_TYPE_SSL_CA) {
                        trustType = trustSSL;
                    } else {
                        trustType = trustObjectSigning;
                    }
                }
                
                certFlags = SEC_GET_TRUST_FLAGS((&trust), trustType);
                if ((certFlags & requiredFlags) == requiredFlags) {
                    trusted = PKIX_TRUE;
                }
            } else {
                for (trustType = trustSSL; trustType < trustTypeNone;
                     trustType++) {
                    certFlags =
                        SEC_GET_TRUST_FLAGS((&trust), trustType);
                    if ((certFlags & requiredFlags) == requiredFlags) {
                        trusted = PKIX_TRUE;
                        break;
                    }
                }
            }
        }

        *pTrusted = trusted;

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_Pk11CertStore_CertQuery
 * DESCRIPTION:
 *
 *  This function obtains from the database the Certs specified by the
 *  ComCertSelParams pointed to by "params" and stores the resulting
 *  List at "pSelected". If no matching Certs are found, a NULL pointer
 *  will be stored.
 *
 *  This function uses a "smart" database query if the Subject has been set
 *  in ComCertSelParams. Otherwise, it uses a very inefficient call to
 *  retrieve all Certs in the database (and run them through the selector).
 *
 * PARAMETERS:
 * "params"
 *      Address of the ComCertSelParams. Must be non-NULL.
 * "pSelected"
 *      Address at which List will be stored. Must be non-NULL.
 * "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Pk11CertStore_CertQuery(
        PKIX_ComCertSelParams *params,
        PKIX_List **pSelected,
        void *plContext)
{
        PRBool validOnly = PR_FALSE;
        PRTime prtime = 0;
        PKIX_PL_X500Name *subjectName = NULL;
        PKIX_PL_Date *certValid = NULL;
        PKIX_List *certList = NULL;
        PKIX_PL_Cert *cert = NULL;
        CERTCertList *pk11CertList = NULL;
        CERTCertListNode *node = NULL;
        CERTCertificate *nssCert = NULL;
        CERTCertDBHandle *dbHandle = NULL;

        PRArenaPool *arena = NULL;
        SECItem *nameItem = NULL;
        void *wincx = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_Pk11CertStore_CertQuery");
        PKIX_NULLCHECK_TWO(params, pSelected);

        /* avoid multiple calls to retrieve a constant */
        PKIX_PL_NSSCALLRV(CERTSTORE, dbHandle, CERT_GetDefaultCertDB, ());

        /*
         * Any of the ComCertSelParams may be obtained and used to constrain
         * the database query, to allow the use of a "smart" query. See
         * pkix_certsel.h for a list of the PKIX_ComCertSelParams_Get*
         * calls available. No corresponding "smart" queries exist at present,
         * except for CERT_CreateSubjectCertList based on Subject. When others
         * are added, corresponding code should be added to
         * pkix_pl_Pk11CertStore_CertQuery to use them when appropriate
         * selector parameters have been set.
         */

        PKIX_CHECK(PKIX_ComCertSelParams_GetSubject
                (params, &subjectName, plContext),
                PKIX_COMCERTSELPARAMSGETSUBJECTFAILED);

        PKIX_CHECK(PKIX_ComCertSelParams_GetCertificateValid
                (params, &certValid, plContext),
                PKIX_COMCERTSELPARAMSGETCERTIFICATEVALIDFAILED);

        /* If caller specified a Date, convert it to PRTime */
        if (certValid) {
                PKIX_CHECK(pkix_pl_Date_GetPRTime
                        (certValid, &prtime, plContext),
                        PKIX_DATEGETPRTIMEFAILED);
                validOnly = PR_TRUE;
        }

        /*
         * If we have the subject name for the desired subject,
         * ask the database for Certs with that subject. Otherwise
         * ask the database for all Certs.
         */
        if (subjectName) {
                arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
                if (arena) {

                        PKIX_CHECK(pkix_pl_X500Name_GetDERName
                                (subjectName, arena, &nameItem, plContext),
                                PKIX_X500NAMEGETSECNAMEFAILED);

                        if (nameItem) {

                            PKIX_PL_NSSCALLRV
                                (CERTSTORE,
                                pk11CertList,
                                CERT_CreateSubjectCertList,
                                (NULL, dbHandle, nameItem, prtime, validOnly));
                        }
                        PKIX_PL_NSSCALL
                                (CERTSTORE, PORT_FreeArena, (arena, PR_FALSE));
                        arena = NULL;
                }

        } else {

                PKIX_CHECK(pkix_pl_NssContext_GetWincx
                        ((PKIX_PL_NssContext *)plContext, &wincx),
                        PKIX_NSSCONTEXTGETWINCXFAILED);

                PKIX_PL_NSSCALLRV
                        (CERTSTORE,
                        pk11CertList,
                        PK11_ListCerts,
                        (PK11CertListAll, wincx));
        }

        if (pk11CertList) {

                PKIX_CHECK(PKIX_List_Create(&certList, plContext),
                        PKIX_LISTCREATEFAILED);

                for (node = CERT_LIST_HEAD(pk11CertList);
                    !(CERT_LIST_END(node, pk11CertList));
                    node = CERT_LIST_NEXT(node)) {

                        PKIX_PL_NSSCALLRV
                                (CERTSTORE,
                                nssCert,
                                CERT_NewTempCertificate,
                                        (dbHandle,
                                        &(node->cert->derCert),
                                        NULL, /* nickname */
                                        PR_FALSE,
                                        PR_TRUE)); /* copyDER */

                        if (!nssCert) {
                                continue; /* just skip bad certs */
                        }

                        PKIX_CHECK_ONLY_FATAL(pkix_pl_Cert_CreateWithNSSCert
                                (nssCert, &cert, plContext),
                                PKIX_CERTCREATEWITHNSSCERTFAILED);

                        if (PKIX_ERROR_RECEIVED) {
                                CERT_DestroyCertificate(nssCert);
                                nssCert = NULL;
                                continue; /* just skip bad certs */
                        }

                        PKIX_CHECK_ONLY_FATAL(PKIX_List_AppendItem
                                (certList, (PKIX_PL_Object *)cert, plContext),
                                PKIX_LISTAPPENDITEMFAILED);

                        PKIX_DECREF(cert);

                }

                /* Don't throw away the list if one cert was bad! */
                pkixTempErrorReceived = PKIX_FALSE;
        }

        *pSelected = certList;
        certList = NULL;

cleanup:
        
        if (pk11CertList) {
            CERT_DestroyCertList(pk11CertList);
        }
        if (arena) {
            PORT_FreeArena(arena, PR_FALSE);
        }

        PKIX_DECREF(subjectName);
        PKIX_DECREF(certValid);
        PKIX_DECREF(cert);
        PKIX_DECREF(certList);

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_Pk11CertStore_CrlQuery
 * DESCRIPTION:
 *
 *  This function obtains from the database the CRLs specified by the
 *  ComCRLSelParams pointed to by "params" and stores the List at "pSelected".
 *  If no Crls are found matching the criteria a NULL pointer is stored.
 *
 *  This function uses a "smart" database query if IssuerNames has been set
 *  in ComCertSelParams. Otherwise, it would have to use a very inefficient
 *  call to retrieve all Crls in the database (and run them through the
 *  selector). In addition to being inefficient, this usage would cause a
 *  memory leak because we have no mechanism at present for releasing a List
 *  of Crls that occupy the same arena. Therefore this function returns a
 *  CertStore Error if the selector has not been provided with parameters
 *  that allow for a "smart" query. (Currently, only the Issuer Name meets
 *  this requirement.)
 *
 * PARAMETERS:
 * "params"
 *      Address of the ComCRLSelParams. Must be non-NULL.
 * "pSelected"
 *      Address at which List will be stored. Must be non-NULL.
 * "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Pk11CertStore_CrlQuery(
        PKIX_ComCRLSelParams *params,
        PKIX_List **pSelected,
        void *plContext)
{
        PKIX_UInt32 nameIx = 0;
        PKIX_UInt32 numNames = 0;
        PKIX_List *issuerNames = NULL;
        PKIX_PL_X500Name *issuer = NULL;
        PRArenaPool *arena = NULL;
        SECItem *nameItem = NULL;
        SECStatus rv = SECFailure;
        PKIX_List *crlList = NULL;
        PKIX_PL_CRL *crl = NULL;
        CRLDPCache* dpcache = NULL;
        CERTSignedCrl** crls = NULL;
        PRBool writeLocked = PR_FALSE;
        PRUint16 status = 0;
        void *wincx = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_Pk11CertStore_CrlQuery");
        PKIX_NULLCHECK_TWO(params, pSelected);

        PKIX_CHECK(pkix_pl_NssContext_GetWincx
                ((PKIX_PL_NssContext *)plContext, &wincx),
                PKIX_NSSCONTEXTGETWINCXFAILED);

        /*
         * If we have <info> for <a smart query>,
         * ask the database for Crls meeting those constraints.
         */
        PKIX_CHECK(PKIX_ComCRLSelParams_GetIssuerNames
                (params, &issuerNames, plContext),
                PKIX_COMCRLSELPARAMSGETISSUERNAMESFAILED);

        /*
         * The specification for PKIX_ComCRLSelParams_GetIssuerNames in
         * pkix_crlsel.h says that if the IssuerNames is not set we get a null
         * pointer. If the user set IssuerNames to an empty List he has
         * provided a criterion impossible to meet ("must match at least one
         * of the names in the List").
         */
        if (issuerNames) {

            PKIX_CHECK(PKIX_List_Create(&crlList, plContext),
                        PKIX_LISTCREATEFAILED);

            PKIX_CHECK(PKIX_List_GetLength
                        (issuerNames, &numNames, plContext),
                        PKIX_LISTGETLENGTHFAILED);
            arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
            if (arena) {

                for (nameIx = 0; nameIx < numNames; nameIx++) {
                    PKIX_CHECK(PKIX_List_GetItem
                        (issuerNames,
                        nameIx,
                        (PKIX_PL_Object **)&issuer,
                        plContext),
                        PKIX_LISTGETITEMFAILED);
                    PKIX_CHECK(pkix_pl_X500Name_GetDERName
                        (issuer, arena, &nameItem, plContext),
                        PKIX_X500NAMEGETSECNAMEFAILED);
                    if (nameItem) {
                        /*
                         * Successive calls append CRLs to
                         * the end of the list. If failure,
                         * no CRLs were appended.
                         */
                        rv = AcquireDPCache(NULL, nameItem, NULL, 0,
                                            wincx, &dpcache, &writeLocked);
                        if (rv == SECFailure) {
                            PKIX_ERROR(PKIX_FETCHINGCACHEDCRLFAILED);
                        }

                        PKIX_PL_NSSCALLRV
                            (CERTSTORE, rv, DPCache_GetAllCRLs,
                            (dpcache, arena, &crls, &status));

                        if ((status & (~CRL_CACHE_INVALID_CRLS)) != 0) {
                            PKIX_ERROR(PKIX_FETCHINGCACHEDCRLFAILED);
                        }

                        while (crls != NULL && *crls != NULL) {

                            PKIX_CHECK_ONLY_FATAL
                                (pkix_pl_CRL_CreateWithSignedCRL
                                (*crls, &crl, plContext),
                                PKIX_CRLCREATEWITHSIGNEDCRLFAILED);

                            if (PKIX_ERROR_RECEIVED) {
                                PKIX_DECREF(crl);
                                crls++;
                                continue; /* just skip bad certs */
                            }

                            PKIX_CHECK_ONLY_FATAL(PKIX_List_AppendItem
                                (crlList, (PKIX_PL_Object *)crl, plContext),
                                PKIX_LISTAPPENDITEMFAILED);

                            PKIX_DECREF(crl);
                            crls++;
                        }

                        /* Don't throw away the list if one cert was bad! */
                        pkixTempErrorReceived = PKIX_FALSE;
                        
                    }
                    PKIX_DECREF(issuer);
                }

            }
        } else {
                PKIX_ERROR(PKIX_INSUFFICIENTCRITERIAFORCRLQUERY);
        }

        *pSelected = crlList;
        crlList = NULL;

cleanup:

        PKIX_DECREF(crlList);

        ReleaseDPCache(dpcache, writeLocked);

        if (arena) {
            PORT_FreeArena(arena, PR_FALSE);
        }

        PKIX_DECREF(issuerNames);
        PKIX_DECREF(issuer);
        PKIX_DECREF(crl);

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_Pk11CertStore_ImportCrl
 * DESCRIPTION:
 *
 * PARAMETERS:
 * "params"
 *      Address of the ComCRLSelParams. Must be non-NULL.
 * "pSelected"
 *      Address at which List will be stored. Must be non-NULL.
 * "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Pk11CertStore_ImportCrl(
        PKIX_CertStore *store,
        PKIX_List *crlList,
        void *plContext)
{
    int crlIndex = 0;
    PKIX_PL_CRL *crl = NULL;
    CERTCertDBHandle *certHandle = CERT_GetDefaultCertDB();
    PKIX_UInt32 listLength;

    PKIX_ENTER(CERTSTORE, "pkix_pl_Pk11CertStore_ImportCrl");
    PKIX_NULLCHECK_ONE(store);
    
    if (!crlList) {
        goto cleanup;
    }
    PKIX_CHECK(
        PKIX_List_GetLength(crlList, &listLength, plContext),
        PKIX_LISTGETLENGTHFAILED);
    for (;crlIndex < listLength;crlIndex++) {
        PKIX_CHECK(
            PKIX_List_GetItem(crlList, crlIndex, (PKIX_PL_Object**)&crl,
                              plContext),
            PKIX_LISTGETITEMFAILED);
        if (!crl->nssSignedCrl || !crl->nssSignedCrl->derCrl) {
            PKIX_ERROR(PKIX_NULLARGUMENT);
        }
        CERT_CacheCRL(certHandle, crl->nssSignedCrl->derCrl);
        PKIX_DECREF(crl);
    }

cleanup:
    PKIX_DECREF(crl);

    PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_Pk11CertStore_CheckCrl
 * DESCRIPTION:
 *
 * PARAMETERS:
 * "params"
 *      Address of the ComCRLSelParams. Must be non-NULL.
 * "pSelected"
 *      Address at which List will be stored. Must be non-NULL.
 * "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_Pk11CertStore_CheckRevByCrl(
        PKIX_CertStore *store,
        PKIX_PL_Cert *pkixCert,
        PKIX_PL_Cert *pkixIssuer,
        PKIX_PL_Date *date,
        PKIX_Boolean delayCrlSigCheck,
        PKIX_UInt32  *pReasonCode,
        PKIX_RevocationStatus *pStatus,
        void *plContext)
{
    CERTCRLEntryReasonCode revReason = crlEntryReasonUnspecified;
    PKIX_RevocationStatus status = PKIX_RevStatus_NoInfo;
    PRTime time = 0;
    void *wincx = NULL;
    PRBool lockedwrite = PR_FALSE;
    SECStatus rv = SECSuccess;
    CRLDPCache* dpcache = NULL;
    CERTCertificate *cert, *issuer;
    CERTCrlEntry* entry = NULL;

    PKIX_ENTER(CERTSTORE, "pkix_pl_Pk11CertStore_CheckRevByCrl");
    PKIX_NULLCHECK_FOUR(store, pkixCert, pkixIssuer, date);

    PKIX_CHECK(
        pkix_pl_Date_GetPRTime(date, &time, plContext),
        PKIX_DATEGETPRTIMEFAILED);

    PKIX_CHECK(
        pkix_pl_NssContext_GetWincx((PKIX_PL_NssContext*)plContext,
                                    &wincx),
        PKIX_NSSCONTEXTGETWINCXFAILED);

    cert = pkixCert->nssCert;
    issuer = pkixIssuer->nssCert;

    if (SECSuccess != CERT_CheckCertValidTimes(issuer, time, PR_FALSE))
    {
        /* we won't be able to check the CRL's signature if the issuer cert
           is expired as of the time we are verifying. This may cause a valid
           CRL to be cached as bad. short-circuit to avoid this case. */
        PORT_SetError(SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE);
        PKIX_ERROR(PKIX_CRLISSUECERTEXPIRED);
    }

    rv = AcquireDPCache(issuer, &issuer->derSubject, NULL,
                        /* AcquireDPCache will not validate the signature
                         * on the crl if time is not specified. */
                        delayCrlSigCheck ? 0: time,
                        wincx, &dpcache, &lockedwrite);
    if (rv == SECFailure) {
        PKIX_ERROR(PKIX_CERTCHECKCRLFAILED);
    }
    if ((delayCrlSigCheck && dpcache->invalid) ||
        /* obtained cache is invalid due to delayed signature check */
        !dpcache->ncrls) {
        goto cleanup;
    }
    /* now look up the certificate SN in the DP cache's CRL */
    rv = DPCache_Lookup(dpcache, &cert->serialNumber, &entry);
    if (rv == SECFailure) {
        PKIX_ERROR(PKIX_CERTCHECKCRLFAILED);
    }
    if (entry) {
        /* check the time if we have one */
        if (entry->revocationDate.data && entry->revocationDate.len) {
            PRTime revocationDate = 0;
            
            if (SECSuccess == DER_DecodeTimeChoice(&revocationDate,
                                                   &entry->revocationDate)) {
                /* we got a good revocation date, only consider the
                   certificate revoked if the time we are inquiring about
                   is past the revocation date */
                if (time >= revocationDate) {
                    rv = SECFailure;
                }
            } else {
                /* invalid revocation date, consider the certificate
                   permanently revoked */
                rv = SECFailure;
            }
        } else {
            /* no revocation date, certificate is permanently revoked */
            rv = SECFailure;
        }
        if (SECFailure == rv) {
            /* Find real revocation reason */
            CERT_FindCRLEntryReasonExten(entry, &revReason);
            status = PKIX_RevStatus_Revoked;
            PORT_SetError(SEC_ERROR_REVOKED_CERTIFICATE);
        }
    } else {
        status = PKIX_RevStatus_Success;
    }

cleanup:
    *pStatus = status;
    *pReasonCode = revReason;
    if (dpcache) {
        ReleaseDPCache(dpcache, lockedwrite);
    }
    PKIX_RETURN(CERTSTORE);    
}


/*
 * FUNCTION: pkix_pl_Pk11CertStore_GetCert
 *  (see description of PKIX_CertStore_CertCallback in pkix_certstore.h)
 */
PKIX_Error *
pkix_pl_Pk11CertStore_GetCert(
        PKIX_CertStore *store,
        PKIX_CertSelector *selector,
        PKIX_VerifyNode *parentVerifyNode,
        void **pNBIOContext,
        PKIX_List **pCertList,
        void *plContext)
{
        PKIX_UInt32 i = 0;
        PKIX_UInt32 numFound = 0;
        PKIX_PL_Cert *candidate = NULL;
        PKIX_List *selected = NULL;
        PKIX_List *filtered = NULL;
        PKIX_CertSelector_MatchCallback selectorCallback = NULL;
        PKIX_CertStore_CheckTrustCallback trustCallback = NULL;
        PKIX_ComCertSelParams *params = NULL;
        PKIX_Boolean cacheFlag = PKIX_FALSE;
        PKIX_VerifyNode *verifyNode = NULL;
        PKIX_Error *selectorError = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_Pk11CertStore_GetCert");
        PKIX_NULLCHECK_FOUR(store, selector, pNBIOContext, pCertList);

        *pNBIOContext = NULL;   /* We don't use non-blocking I/O */

        PKIX_CHECK(PKIX_CertSelector_GetMatchCallback
                (selector, &selectorCallback, plContext),
                PKIX_CERTSELECTORGETMATCHCALLBACKFAILED);

        PKIX_CHECK(PKIX_CertSelector_GetCommonCertSelectorParams
                (selector, &params, plContext),
                PKIX_CERTSELECTORGETCOMCERTSELPARAMSFAILED);

        PKIX_CHECK(pkix_pl_Pk11CertStore_CertQuery
                (params, &selected, plContext),
                PKIX_PK11CERTSTORECERTQUERYFAILED);

        if (selected) {
                PKIX_CHECK(PKIX_List_GetLength(selected, &numFound, plContext),
                        PKIX_LISTGETLENGTHFAILED);
        }

        PKIX_CHECK(PKIX_CertStore_GetCertStoreCacheFlag
                (store, &cacheFlag, plContext),
                PKIX_CERTSTOREGETCERTSTORECACHEFLAGFAILED);

        PKIX_CHECK(PKIX_CertStore_GetTrustCallback
                (store, &trustCallback, plContext),
                PKIX_CERTSTOREGETTRUSTCALLBACKFAILED);

        PKIX_CHECK(PKIX_List_Create(&filtered, plContext),
                PKIX_LISTCREATEFAILED);

        for (i = 0; i < numFound; i++) {
                PKIX_CHECK_ONLY_FATAL(PKIX_List_GetItem
                        (selected,
                        i,
                        (PKIX_PL_Object **)&candidate,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                if (PKIX_ERROR_RECEIVED) {
                        continue; /* just skip bad certs */
                }

                selectorError =
                    selectorCallback(selector, candidate, plContext);
                if (!selectorError) {
                        PKIX_CHECK(PKIX_PL_Cert_SetCacheFlag
                                (candidate, cacheFlag, plContext),
                                PKIX_CERTSETCACHEFLAGFAILED);

                        if (trustCallback) {
                                PKIX_CHECK(PKIX_PL_Cert_SetTrustCertStore
                                    (candidate, store, plContext),
                                    PKIX_CERTSETTRUSTCERTSTOREFAILED);
                        }

                        PKIX_CHECK_ONLY_FATAL(PKIX_List_AppendItem
                                (filtered,
                                (PKIX_PL_Object *)candidate,
                                plContext),
                                PKIX_LISTAPPENDITEMFAILED);
                } else if (parentVerifyNode) {
                    PKIX_CHECK_FATAL(
                        pkix_VerifyNode_Create(candidate, 0, selectorError,
                                               &verifyNode, plContext),
                        PKIX_VERIFYNODECREATEFAILED);
                    PKIX_CHECK_FATAL(
                        pkix_VerifyNode_AddToTree(parentVerifyNode,
                                                  verifyNode,
                                                  plContext),
                        PKIX_VERIFYNODEADDTOTREEFAILED);
                    PKIX_DECREF(verifyNode);
                }
                PKIX_DECREF(selectorError);
                PKIX_DECREF(candidate);
        }

        /* Don't throw away the list if one cert was bad! */
        pkixTempErrorReceived = PKIX_FALSE;

        *pCertList = filtered;
        filtered = NULL;

cleanup:
fatal:
        PKIX_DECREF(filtered);
        PKIX_DECREF(candidate);
        PKIX_DECREF(selected);
        PKIX_DECREF(params);
        PKIX_DECREF(verifyNode);
        PKIX_DECREF(selectorError);

        PKIX_RETURN(CERTSTORE);
}

/*
 * FUNCTION: pkix_pl_Pk11CertStore_GetCRL
 *  (see description of PKIX_CertStore_CRLCallback in pkix_certstore.h)
 */
PKIX_Error *
pkix_pl_Pk11CertStore_GetCRL(
        PKIX_CertStore *store,
        PKIX_CRLSelector *selector,
        void **pNBIOContext,
        PKIX_List **pCrlList,
        void *plContext)
{
        PKIX_UInt32 i = 0;
        PKIX_UInt32 numFound = 0;
        PKIX_Boolean match = PKIX_FALSE;
        PKIX_PL_CRL *candidate = NULL;
        PKIX_List *selected = NULL;
        PKIX_List *filtered = NULL;
        PKIX_CRLSelector_MatchCallback callback = NULL;
        PKIX_ComCRLSelParams *params = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_Pk11CertStore_GetCRL");
        PKIX_NULLCHECK_FOUR(store, selector, pNBIOContext, pCrlList);

        *pNBIOContext = NULL;   /* We don't use non-blocking I/O */

        PKIX_CHECK(PKIX_CRLSelector_GetMatchCallback
                (selector, &callback, plContext),
                PKIX_CRLSELECTORGETMATCHCALLBACKFAILED);

        PKIX_CHECK(PKIX_CRLSelector_GetCommonCRLSelectorParams
                (selector, &params, plContext),
                PKIX_CRLSELECTORGETCOMCERTSELPARAMSFAILED);

        PKIX_CHECK(pkix_pl_Pk11CertStore_CrlQuery
                (params, &selected, plContext),
                PKIX_PK11CERTSTORECRLQUERYFAILED);

        if (selected) {
                PKIX_CHECK(PKIX_List_GetLength(selected, &numFound, plContext),
                        PKIX_LISTGETLENGTHFAILED);
        }

        PKIX_CHECK(PKIX_List_Create(&filtered, plContext),
                PKIX_LISTCREATEFAILED);

        for (i = 0; i < numFound; i++) {
                PKIX_CHECK_ONLY_FATAL(PKIX_List_GetItem
                        (selected,
                        i,
                        (PKIX_PL_Object **)&candidate,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                if (PKIX_ERROR_RECEIVED) {
                        continue; /* just skip bad CRLs */
                }

                PKIX_CHECK_ONLY_FATAL(callback
                        (selector, candidate, &match, plContext),
                        PKIX_CRLSELECTORFAILED);

                if (!(PKIX_ERROR_RECEIVED) && match) {
                        PKIX_CHECK_ONLY_FATAL(PKIX_List_AppendItem
                                (filtered,
                                (PKIX_PL_Object *)candidate,
                                plContext),
                                PKIX_LISTAPPENDITEMFAILED);
                }

                PKIX_DECREF(candidate);
        }

        /* Don't throw away the list if one CRL was bad! */
        pkixTempErrorReceived = PKIX_FALSE;

        *pCrlList = filtered;
        filtered = NULL;

cleanup:

        PKIX_DECREF(filtered);
        PKIX_DECREF(candidate);
        PKIX_DECREF(selected);
        PKIX_DECREF(params);

        PKIX_RETURN(CERTSTORE);
}

/* --Public-Pk11CertStore-Functions----------------------------------- */

/*
 * FUNCTION: PKIX_PL_Pk11CertStore_Create
 * (see comments in pkix_samples_modules.h)
 */
PKIX_Error *
PKIX_PL_Pk11CertStore_Create(
        PKIX_CertStore **pCertStore,
        void *plContext)
{
        PKIX_CertStore *certStore = NULL;

        PKIX_ENTER(CERTSTORE, "PKIX_PL_Pk11CertStore_Create");
        PKIX_NULLCHECK_ONE(pCertStore);

        PKIX_CHECK(PKIX_CertStore_Create
                (pkix_pl_Pk11CertStore_GetCert,
                pkix_pl_Pk11CertStore_GetCRL,
                NULL, /* getCertContinue */
                NULL, /* getCrlContinue */
                pkix_pl_Pk11CertStore_CheckTrust,
                pkix_pl_Pk11CertStore_ImportCrl,
                pkix_pl_Pk11CertStore_CheckRevByCrl,
                NULL,
                PKIX_TRUE, /* cache flag */
                PKIX_TRUE, /* local - no network I/O */
                &certStore,
                plContext),
                PKIX_CERTSTORECREATEFAILED);

        *pCertStore = certStore;

cleanup:

        PKIX_RETURN(CERTSTORE);
}
