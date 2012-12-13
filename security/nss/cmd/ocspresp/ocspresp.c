/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * ocspresp - self test for OCSP response creation
 */

#include "nspr.h"
#include "secutil.h"
#include "secpkcs7.h"
#include "cert.h"
#include "certdb.h"
#include "nss.h"
#include "pk11func.h"
#include "cryptohi.h"
#include "ocsp.h"

#if defined(XP_UNIX)
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>

secuPWData  pwdata          = { PW_NONE, 0 };

static PRBool
getCaAndSubjectCert(CERTCertDBHandle *certHandle,
                    const char *caNick, const char *eeNick,
                    CERTCertificate **outCA, CERTCertificate **outCert)
{
    *outCA = CERT_FindCertByNickname(certHandle, caNick);
    *outCert = CERT_FindCertByNickname(certHandle, eeNick);
    return *outCA && *outCert;
}

static SECItem *
encode(PRArenaPool *arena, CERTOCSPCertID *cid, CERTCertificate *ca)
{
    SECItem *response;
    PRTime now = PR_Now();
    PRTime nextUpdate;
    CERTOCSPSingleResponse **responses;
    CERTOCSPSingleResponse *sr;

    if (!arena)
        return NULL;

    nextUpdate = now + 10 * PR_USEC_PER_SEC; /* in the future */
    
    sr = CERT_CreateOCSPSingleResponseGood(arena, cid, now, &nextUpdate);

    /* meaning of value 2: one entry + one end marker */
    responses = PORT_ArenaNewArray(arena, CERTOCSPSingleResponse*, 2);
    if (responses == NULL)
        return NULL;
    
    responses[0] = sr;
    responses[1] = NULL;
    
    response = CERT_CreateEncodedOCSPSuccessResponse(
        arena, ca, ocspResponderID_byName, now, responses, &pwdata);

    return response;
}

static SECItem *
encodeRevoked(PRArenaPool *arena, CERTOCSPCertID *cid, CERTCertificate *ca)
{
    SECItem *response;
    PRTime now = PR_Now();
    PRTime revocationTime;
    CERTOCSPSingleResponse **responses;
    CERTOCSPSingleResponse *sr;

    if (!arena)
        return NULL;

    revocationTime = now - 10 * PR_USEC_PER_SEC; /* in the past */

    sr = CERT_CreateOCSPSingleResponseRevoked(arena, cid, now, NULL,
                                              revocationTime, NULL);

    /* meaning of value 2: one entry + one end marker */
    responses = PORT_ArenaNewArray(arena, CERTOCSPSingleResponse*, 2);
    if (responses == NULL)
        return NULL;

    responses[0] = sr;
    responses[1] = NULL;

    response = CERT_CreateEncodedOCSPSuccessResponse(
        arena, ca, ocspResponderID_byName, now, responses, &pwdata);

    return response;
}

int Usage(void)
{
    PRFileDesc *pr_stderr = PR_STDERR;
    PR_fprintf (pr_stderr, "ocspresp runs an internal selftest for OCSP response creation");
    PR_fprintf (pr_stderr, "Usage:");
    PR_fprintf (pr_stderr,
                "\tocspresp <dbdir> <CA-nick> <EE-nick> [-p <pass>] [-f <file>]\n");
    PR_fprintf (pr_stderr,
                "\tdbdir:   Find security databases in \"dbdir\"\n");
    PR_fprintf (pr_stderr,
                "\tCA-nick: nickname of a trusted CA certificate with private key\n");
    PR_fprintf (pr_stderr,
                "\tEE-nick: nickname of a entity cert issued by CA\n");
    PR_fprintf (pr_stderr,
                "\t-p:      a password for db\n");
    PR_fprintf (pr_stderr,
                "\t-f:      a filename containing the password for db\n");
    return -1;
}

int
main(int argc, char **argv)
{
    SECStatus rv;
    int retval = -1;
    CERTCertDBHandle *certHandle = NULL;
    CERTCertificate *caCert = NULL, *cert = NULL;
    CERTOCSPCertID *cid = NULL;
    PRArenaPool *arena = NULL;
    PRTime now = PR_Now();
    
    SECItem *encoded = NULL;
    CERTOCSPResponse *decoded = NULL;
    SECStatus statusDecoded;

    SECItem *encodedRev = NULL;
    CERTOCSPResponse *decodedRev = NULL;
    SECStatus statusDecodedRev;
    
    SECItem *encodedFail = NULL;
    CERTOCSPResponse *decodedFail = NULL;
    SECStatus statusDecodedFail;

    CERTCertificate *obtainedSignerCert = NULL;

    if (argc != 4 && argc != 6) {
        return Usage();
    }

    if (argc == 6) {
        if (!strcmp(argv[4], "-p")) {
            pwdata.source = PW_PLAINTEXT;
            pwdata.data = PORT_Strdup(argv[5]);
        }
        else if (!strcmp(argv[4], "-f")) {
            pwdata.source = PW_FROMFILE;
            pwdata.data = PORT_Strdup(argv[5]);
        }
        else
            return Usage();
    }

    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    /*rv = NSS_Init(SECU_ConfigDirectory(NULL));*/
    rv = NSS_Init(argv[1]);
    if (rv != SECSuccess) {
	SECU_PrintPRandOSError(argv[0]);
	goto loser;
    }

    PK11_SetPasswordFunc(SECU_GetModulePassword);

    certHandle = CERT_GetDefaultCertDB();
    if (!certHandle)
	goto loser;

    if (!getCaAndSubjectCert(certHandle, argv[2], argv[3], &caCert, &cert))
        goto loser;

    cid = CERT_CreateOCSPCertID(cert, now);

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    encoded = encode(arena, cid, caCert);
    PORT_Assert(encoded);
    decoded = CERT_DecodeOCSPResponse(encoded);
    statusDecoded = CERT_GetOCSPResponseStatus(decoded);
    PORT_Assert(statusDecoded == SECSuccess);

    statusDecoded = CERT_VerifyOCSPResponseSignature(decoded, certHandle, &pwdata,
                                                &obtainedSignerCert, caCert);
    PORT_Assert(statusDecoded == SECSuccess);
    statusDecoded = CERT_GetOCSPStatusForCertID(certHandle, decoded, cid,
                                                obtainedSignerCert, now);
    PORT_Assert(statusDecoded == SECSuccess);
    CERT_DestroyCertificate(obtainedSignerCert);

    encodedRev = encodeRevoked(arena, cid, caCert);
    PORT_Assert(encodedRev);
    decodedRev = CERT_DecodeOCSPResponse(encodedRev);
    statusDecodedRev = CERT_GetOCSPResponseStatus(decodedRev);
    PORT_Assert(statusDecodedRev == SECSuccess);

    statusDecodedRev = CERT_VerifyOCSPResponseSignature(decodedRev, certHandle, &pwdata,
                                                        &obtainedSignerCert, caCert);
    PORT_Assert(statusDecodedRev == SECSuccess);
    statusDecodedRev = CERT_GetOCSPStatusForCertID(certHandle, decodedRev, cid,
                                                   obtainedSignerCert, now);
    PORT_Assert(statusDecodedRev == SECFailure);
    PORT_Assert(PORT_GetError() == SEC_ERROR_REVOKED_CERTIFICATE);
    CERT_DestroyCertificate(obtainedSignerCert);
    
    encodedFail = CERT_CreateEncodedOCSPErrorResponse(
        arena, SEC_ERROR_OCSP_TRY_SERVER_LATER);
    PORT_Assert(encodedFail);
    decodedFail = CERT_DecodeOCSPResponse(encodedFail);
    statusDecodedFail = CERT_GetOCSPResponseStatus(decodedFail);
    PORT_Assert(statusDecodedFail == SECFailure);
    PORT_Assert(PORT_GetError() == SEC_ERROR_OCSP_TRY_SERVER_LATER);

    retval = 0;
loser:
    if (retval != 0)
        SECU_PrintError(argv[0], "tests failed");
    
    if (cid)
        CERT_DestroyOCSPCertID(cid);
    if (cert)
        CERT_DestroyCertificate(cert);
    if (caCert)
        CERT_DestroyCertificate(caCert);
    if (arena)
        PORT_FreeArena(arena, PR_FALSE);
    if (decoded)
        CERT_DestroyOCSPResponse(decoded);
    if (decodedRev)
        CERT_DestroyOCSPResponse(decodedRev);
    if (decodedFail)
        CERT_DestroyOCSPResponse(decodedFail);
    if (pwdata.data) {
        PORT_Free(pwdata.data);
    }
    
    if (NSS_Shutdown() != SECSuccess) {
        SECU_PrintError(argv[0], "NSS shutdown:");
        if (retval == 0)
            retval = -2;
    }

    return retval;
}
