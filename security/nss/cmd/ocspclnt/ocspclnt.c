/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test program for client-side OCSP.
 */

#include "secutil.h"
#include "nspr.h"
#include "plgetopt.h"
#include "nss.h"
#include "cert.h"
#include "ocsp.h"
#include "xconst.h" /*
                      * XXX internal header file; needed to get at
                      * cert_DecodeAuthInfoAccessExtension -- would be
                      * nice to not need this, but that would require
                      * better/different APIs.
                      */

#ifndef NO_PP       /*                                               \
                     * Compile with this every once in a while to be \
                     * sure that no dependencies on it get added     \
                     * outside of the pretty-printing routines.      \
                     */
#include "ocspti.h" /* internals for pretty-printing routines *only* */
#endif              /* NO_PP */

#if defined(_WIN32)
#include "fcntl.h"
#include "io.h"
#endif

#define DEFAULT_DB_DIR "~/.netscape"

/* global */
char *program_name;

static void
synopsis(char *progname)
{
    PRFileDesc *pr_stderr;

    pr_stderr = PR_STDERR;
    PR_fprintf(pr_stderr, "Usage:");
    PR_fprintf(pr_stderr,
               "\t%s -p [-d <dir>]\n",
               progname);
    PR_fprintf(pr_stderr,
               "\t%s -P [-d <dir>]\n",
               progname);
    PR_fprintf(pr_stderr,
               "\t%s -r <name> [-a] [-L] [-s <name>] [-d <dir>]\n",
               progname);
    PR_fprintf(pr_stderr,
               "\t%s -R <name> [-a] [-l <location>] [-s <name>] [-d <dir>]\n",
               progname);
    PR_fprintf(pr_stderr,
               "\t%s -S <name> [-a] [-l <location> -t <name>]\n",
               progname);
    PR_fprintf(pr_stderr,
               "\t\t [-s <name>] [-w <time>] [-d <dir>]\n");
    PR_fprintf(pr_stderr,
               "\t%s -V <name> [-a] -u <usage> [-l <location> -t <name>]\n",
               progname);
    PR_fprintf(pr_stderr,
               "\t\t [-s <name>] [-w <time>] [-d <dir>]\n");
}

static void
short_usage(char *progname)
{
    PR_fprintf(PR_STDERR,
               "Type %s -H for more detailed descriptions\n",
               progname);
    synopsis(progname);
}

static void
long_usage(char *progname)
{
    PRFileDesc *pr_stderr;

    pr_stderr = PR_STDERR;
    synopsis(progname);
    PR_fprintf(pr_stderr, "\nCommands (must specify exactly one):\n");
    PR_fprintf(pr_stderr,
               "  %-13s Pretty-print a binary request read from stdin\n",
               "-p");
    PR_fprintf(pr_stderr,
               "  %-13s Pretty-print a binary response read from stdin\n",
               "-P");
    PR_fprintf(pr_stderr,
               "  %-13s Create a request for cert \"nickname\" on stdout\n",
               "-r nickname");
    PR_fprintf(pr_stderr,
               "  %-13s Get response for cert \"nickname\", dump to stdout\n",
               "-R nickname");
    PR_fprintf(pr_stderr,
               "  %-13s Get status for cert \"nickname\"\n",
               "-S nickname");
    PR_fprintf(pr_stderr,
               "  %-13s Fully verify cert \"nickname\", w/ status check\n",
               "-V nickname");
    PR_fprintf(pr_stderr,
               "\n     %-10s also can be the name of the file with DER or\n"
               "  %-13s PEM(use -a option) cert encoding\n",
               "nickname", "");
    PR_fprintf(pr_stderr, "Options:\n");
    PR_fprintf(pr_stderr,
               "  %-13s Decode input cert from PEM format. DER is default\n",
               "-a");
    PR_fprintf(pr_stderr,
               "  %-13s Add the service locator extension to the request\n",
               "-L");
    PR_fprintf(pr_stderr,
               "  %-13s Find security databases in \"dbdir\" (default %s)\n",
               "-d dbdir", DEFAULT_DB_DIR);
    PR_fprintf(pr_stderr,
               "  %-13s Use \"location\" as URL of responder\n",
               "-l location");
    PR_fprintf(pr_stderr,
               "  %-13s Trust cert \"nickname\" as response signer\n",
               "-t nickname");
    PR_fprintf(pr_stderr,
               "  %-13s Sign requests with cert \"nickname\"\n",
               "-s nickname");
    PR_fprintf(pr_stderr,
               "  %-13s Type of certificate usage for verification:\n",
               "-u usage");
    PR_fprintf(pr_stderr,
               "%-17s c   SSL Client\n", "");
    PR_fprintf(pr_stderr,
               "%-17s s   SSL Server\n", "");
    PR_fprintf(pr_stderr,
               "%-17s I   IPsec\n", "");
    PR_fprintf(pr_stderr,
               "%-17s e   Email Recipient\n", "");
    PR_fprintf(pr_stderr,
               "%-17s E   Email Signer\n", "");
    PR_fprintf(pr_stderr,
               "%-17s S   Object Signer\n", "");
    PR_fprintf(pr_stderr,
               "%-17s C   CA\n", "");
    PR_fprintf(pr_stderr,
               "  %-13s Validity time (default current time), one of:\n",
               "-w time");
    PR_fprintf(pr_stderr,
               "%-17s %-25s (GMT)\n", "", "YYMMDDhhmm[ss]Z");
    PR_fprintf(pr_stderr,
               "%-17s %-25s (later than GMT)\n", "", "YYMMDDhhmm[ss]+hhmm");
    PR_fprintf(pr_stderr,
               "%-17s %-25s (earlier than GMT)\n", "", "YYMMDDhhmm[ss]-hhmm");
}

#if defined(WIN32)
/* We're going to write binary data to stdout, or read binary from stdin.
 * We must put stdout or stdin into O_BINARY mode or else
   outgoing \n's will become \r\n's, and incoming \r\n's will become \n's.
*/
static SECStatus
make_file_binary(FILE *binfile)
{
    int smrv = _setmode(_fileno(binfile), _O_BINARY);
    if (smrv == -1) {
        fprintf(stderr, "%s: Cannot change stdout to binary mode.\n",
                program_name);
    }
    return smrv;
}
#define MAKE_FILE_BINARY make_file_binary
#else
#define MAKE_FILE_BINARY(file)
#endif

/*
 * XXX This is a generic function that would probably make a good
 * replacement for SECU_DER_Read (which is not at all specific to DER,
 * despite its name), but that requires fixing all of the tools...
 * Still, it should be done, whenenver I/somebody has the time.
 * (Also, consider whether this actually belongs in the security
 * library itself, not just in the command library.)
 *
 * This function takes an open file (a PRFileDesc *) and reads the
 * entire file into a SECItem.  (Obviously, the file is intended to
 * be small enough that such a thing is advisable.)  Both the SECItem
 * and the buffer it points to are allocated from the heap; the caller
 * is expected to free them.  ("SECITEM_FreeItem(item, PR_TRUE)")
 */
static SECItem *
read_file_into_item(PRFileDesc *in_file, SECItemType si_type)
{
    PRStatus prv;
    SECItem *item;
    PRFileInfo file_info;
    PRInt32 bytes_read;

    prv = PR_GetOpenFileInfo(in_file, &file_info);
    if (prv != PR_SUCCESS)
        return NULL;

    if (file_info.size == 0) {
        /* XXX Need a better error; just grabbed this one for expediency. */
        PORT_SetError(SEC_ERROR_INPUT_LEN);
        return NULL;
    }

    if (file_info.size > 0xffff) { /* I think this is too big. */
        PORT_SetError(SEC_ERROR_NO_MEMORY);
        return NULL;
    }

    item = PORT_Alloc(sizeof(SECItem));
    if (item == NULL)
        return NULL;

    item->type = si_type;
    item->len = (unsigned int)file_info.size;
    item->data = PORT_Alloc((size_t)item->len);
    if (item->data == NULL)
        goto loser;

    bytes_read = PR_Read(in_file, item->data, (PRInt32)item->len);
    if (bytes_read < 0) {
        /* Something went wrong; error is already set for us. */
        goto loser;
    } else if (bytes_read == 0) {
        /* Something went wrong; we read nothing.  But no system/nspr error. */
        /* XXX Need to set an error here. */
        goto loser;
    } else if (item->len != (unsigned int)bytes_read) {
        /* Something went wrong; we read less (or more!?) than we expected. */
        /* XXX Need to set an error here. */
        goto loser;
    }

    return item;

loser:
    SECITEM_FreeItem(item, PR_TRUE);
    return NULL;
}

/*
 * Create a DER-encoded OCSP request (for the certificate whose nickname
 * is "name") and dump it out.
 */
static SECStatus
create_request(FILE *out_file, CERTCertDBHandle *handle, CERTCertificate *cert,
               PRBool add_service_locator, PRBool add_acceptable_responses)
{
    CERTCertList *certs = NULL;
    CERTCertificate *myCert = NULL;
    CERTOCSPRequest *request = NULL;
    PRTime now = PR_Now();
    SECItem *encoding = NULL;
    SECStatus rv = SECFailure;

    if (handle == NULL || cert == NULL)
        return rv;

    myCert = CERT_DupCertificate(cert);
    if (myCert == NULL)
        goto loser;

    /*
     * We need to create a list of one.
     */
    certs = CERT_NewCertList();
    if (certs == NULL)
        goto loser;

    if (CERT_AddCertToListTail(certs, myCert) != SECSuccess)
        goto loser;

    /*
     * Now that cert is included in the list, we need to be careful
     * that we do not try to destroy it twice.  This will prevent that.
     */
    myCert = NULL;

    request = CERT_CreateOCSPRequest(certs, now, add_service_locator, NULL);
    if (request == NULL)
        goto loser;

    if (add_acceptable_responses) {
        rv = CERT_AddOCSPAcceptableResponses(request,
                                             SEC_OID_PKIX_OCSP_BASIC_RESPONSE);
        if (rv != SECSuccess)
            goto loser;
    }

    encoding = CERT_EncodeOCSPRequest(NULL, request, NULL);
    if (encoding == NULL)
        goto loser;

    MAKE_FILE_BINARY(out_file);
    if (fwrite(encoding->data, encoding->len, 1, out_file) != 1)
        goto loser;

    rv = SECSuccess;

loser:
    if (encoding != NULL)
        SECITEM_FreeItem(encoding, PR_TRUE);
    if (request != NULL)
        CERT_DestroyOCSPRequest(request);
    if (certs != NULL)
        CERT_DestroyCertList(certs);
    if (myCert != NULL)
        CERT_DestroyCertificate(myCert);

    return rv;
}

/*
 * Create a DER-encoded OCSP request (for the certificate whose nickname is
 * "cert_name"), then get and dump a corresponding response.  The responder
 * location is either specified explicitly (as "responder_url") or found
 * via the AuthorityInfoAccess URL in the cert.
 */
static SECStatus
dump_response(FILE *out_file, CERTCertDBHandle *handle, CERTCertificate *cert,
              const char *responder_url)
{
    CERTCertList *certs = NULL;
    CERTCertificate *myCert = NULL;
    char *loc = NULL;
    PRTime now = PR_Now();
    SECItem *response = NULL;
    SECStatus rv = SECFailure;
    PRBool includeServiceLocator;

    if (handle == NULL || cert == NULL)
        return rv;

    myCert = CERT_DupCertificate(cert);
    if (myCert == NULL)
        goto loser;

    if (responder_url != NULL) {
        loc = (char *)responder_url;
        includeServiceLocator = PR_TRUE;
    } else {
        loc = CERT_GetOCSPAuthorityInfoAccessLocation(cert);
        if (loc == NULL)
            goto loser;
        includeServiceLocator = PR_FALSE;
    }

    /*
     * We need to create a list of one.
     */
    certs = CERT_NewCertList();
    if (certs == NULL)
        goto loser;

    if (CERT_AddCertToListTail(certs, myCert) != SECSuccess)
        goto loser;

    /*
     * Now that cert is included in the list, we need to be careful
     * that we do not try to destroy it twice.  This will prevent that.
     */
    myCert = NULL;

    response = CERT_GetEncodedOCSPResponse(NULL, certs, loc, now,
                                           includeServiceLocator,
                                           NULL, NULL, NULL);
    if (response == NULL)
        goto loser;

    MAKE_FILE_BINARY(out_file);
    if (fwrite(response->data, response->len, 1, out_file) != 1)
        goto loser;

    rv = SECSuccess;

loser:
    if (response != NULL)
        SECITEM_FreeItem(response, PR_TRUE);
    if (certs != NULL)
        CERT_DestroyCertList(certs);
    if (myCert != NULL)
        CERT_DestroyCertificate(myCert);
    if (loc != NULL && loc != responder_url)
        PORT_Free(loc);

    return rv;
}

/*
 * Get the status for the specified certificate (whose nickname is "cert_name").
 * Directly use the OCSP function rather than doing a full verification.
 */
static SECStatus
get_cert_status(FILE *out_file, CERTCertDBHandle *handle,
                CERTCertificate *cert, const char *cert_name,
                PRTime verify_time)
{
    SECStatus rv = SECFailure;

    if (handle == NULL || cert == NULL)
        goto loser;

    rv = CERT_CheckOCSPStatus(handle, cert, verify_time, NULL);

    fprintf(out_file, "Check of certificate \"%s\" ", cert_name);
    if (rv == SECSuccess) {
        fprintf(out_file, "succeeded.\n");
    } else {
        const char *error_string = SECU_Strerror(PORT_GetError());
        fprintf(out_file, "failed.  Reason:\n");
        if (error_string != NULL && PORT_Strlen(error_string) > 0)
            fprintf(out_file, "%s\n", error_string);
        else
            fprintf(out_file, "Unknown\n");
    }

    rv = SECSuccess;

loser:

    return rv;
}

/*
 * Verify the specified certificate (whose nickname is "cert_name").
 * OCSP is already turned on, so we just need to call the standard
 * certificate verification API and let it do all the work.
 */
static SECStatus
verify_cert(FILE *out_file, CERTCertDBHandle *handle, CERTCertificate *cert,
            const char *cert_name, SECCertUsage cert_usage, PRTime verify_time)
{
    SECStatus rv = SECFailure;

    if (handle == NULL || cert == NULL)
        return rv;

    rv = CERT_VerifyCert(handle, cert, PR_TRUE, cert_usage, verify_time,
                         NULL, NULL);

    fprintf(out_file, "Verification of certificate \"%s\" ", cert_name);
    if (rv == SECSuccess) {
        fprintf(out_file, "succeeded.\n");
    } else {
        const char *error_string = SECU_Strerror(PORT_GetError());
        fprintf(out_file, "failed.  Reason:\n");
        if (error_string != NULL && PORT_Strlen(error_string) > 0)
            fprintf(out_file, "%s\n", error_string);
        else
            fprintf(out_file, "Unknown\n");
    }

    rv = SECSuccess;

    return rv;
}

CERTCertificate *
find_certificate(CERTCertDBHandle *handle, const char *name, PRBool ascii)
{
    CERTCertificate *cert = NULL;
    SECItem der;
    PRFileDesc *certFile;

    if (handle == NULL || name == NULL)
        return NULL;

    if (ascii == PR_FALSE) {
        /* by default need to check if there is cert nick is given */
        cert = CERT_FindCertByNicknameOrEmailAddr(handle, (char *)name);
        if (cert != NULL)
            return cert;
    }

    certFile = PR_Open(name, PR_RDONLY, 0);
    if (certFile == NULL) {
        return NULL;
    }

    if (SECU_ReadDERFromFile(&der, certFile, ascii, PR_FALSE) == SECSuccess) {
        cert = CERT_DecodeCertFromPackage((char *)der.data, der.len);
        SECITEM_FreeItem(&der, PR_FALSE);
    }
    PR_Close(certFile);

    return cert;
}

#ifdef NO_PP

static SECStatus
print_request(FILE *out_file, SECItem *data)
{
    fprintf(out_file, "Cannot pretty-print request compiled with NO_PP.\n");
    return SECSuccess;
}

static SECStatus
print_response(FILE *out_file, SECItem *data, CERTCertDBHandle *handle)
{
    fprintf(out_file, "Cannot pretty-print response compiled with NO_PP.\n");
    return SECSuccess;
}

#else /* NO_PP */

static void
print_ocsp_version(FILE *out_file, SECItem *version, int level)
{
    if (version->len > 0) {
        SECU_PrintInteger(out_file, version, "Version", level);
    } else {
        SECU_Indent(out_file, level);
        fprintf(out_file, "Version: DEFAULT\n");
    }
}

static void
print_ocsp_cert_id(FILE *out_file, CERTOCSPCertID *cert_id, int level)
{
    SECU_Indent(out_file, level);
    fprintf(out_file, "Cert ID:\n");
    level++;

    SECU_PrintAlgorithmID(out_file, &(cert_id->hashAlgorithm),
                          "Hash Algorithm", level);
    SECU_PrintAsHex(out_file, &(cert_id->issuerNameHash),
                    "Issuer Name Hash", level);
    SECU_PrintAsHex(out_file, &(cert_id->issuerKeyHash),
                    "Issuer Key Hash", level);
    SECU_PrintInteger(out_file, &(cert_id->serialNumber),
                      "Serial Number", level);
    /* XXX lookup the cert; if found, print something nice (nickname?) */
}

static void
print_raw_certificates(FILE *out_file, SECItem **raw_certs, int level)
{
    SECItem *raw_cert;
    int i = 0;
    char cert_label[50];

    SECU_Indent(out_file, level);

    if (raw_certs == NULL) {
        fprintf(out_file, "No Certificates.\n");
        return;
    }

    fprintf(out_file, "Certificate List:\n");
    while ((raw_cert = raw_certs[i++]) != NULL) {
        sprintf(cert_label, "Certificate (%d)", i);
        (void)SECU_PrintSignedData(out_file, raw_cert, cert_label, level + 1,
                                   (SECU_PPFunc)SECU_PrintCertificate);
    }
}

static void
print_ocsp_extensions(FILE *out_file, CERTCertExtension **extensions,
                      char *msg, int level)
{
    if (extensions) {
        SECU_PrintExtensions(out_file, extensions, msg, level);
    } else {
        SECU_Indent(out_file, level);
        fprintf(out_file, "No %s\n", msg);
    }
}

static void
print_single_request(FILE *out_file, ocspSingleRequest *single, int level)
{
    print_ocsp_cert_id(out_file, single->reqCert, level);
    print_ocsp_extensions(out_file, single->singleRequestExtensions,
                          "Single Request Extensions", level);
}

/*
 * Decode the DER/BER-encoded item "data" as an OCSP request
 * and pretty-print the subfields.
 */
static SECStatus
print_request(FILE *out_file, SECItem *data)
{
    CERTOCSPRequest *request;
    ocspTBSRequest *tbsRequest;
    int level = 0;

    PORT_Assert(out_file != NULL);
    PORT_Assert(data != NULL);
    if (out_file == NULL || data == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    request = CERT_DecodeOCSPRequest(data);
    if (request == NULL || request->tbsRequest == NULL)
        return SECFailure;

    tbsRequest = request->tbsRequest;

    fprintf(out_file, "TBS Request:\n");
    level++;

    print_ocsp_version(out_file, &(tbsRequest->version), level);

    /*
     * XXX Probably should be an interface to get the signer name
     * without looking inside the tbsRequest at all.
     */
    if (tbsRequest->requestorName != NULL) {
        SECU_Indent(out_file, level);
        fprintf(out_file, "XXX print the requestorName\n");
    } else {
        SECU_Indent(out_file, level);
        fprintf(out_file, "No Requestor Name.\n");
    }

    if (tbsRequest->requestList != NULL) {
        int i;

        for (i = 0; tbsRequest->requestList[i] != NULL; i++) {
            SECU_Indent(out_file, level);
            fprintf(out_file, "Request %d:\n", i);
            print_single_request(out_file, tbsRequest->requestList[i],
                                 level + 1);
        }
    } else {
        fprintf(out_file, "Request list is empty.\n");
    }

    print_ocsp_extensions(out_file, tbsRequest->requestExtensions,
                          "Request Extensions", level);

    if (request->optionalSignature != NULL) {
        ocspSignature *whole_sig;
        SECItem rawsig;

        fprintf(out_file, "Signature:\n");

        whole_sig = request->optionalSignature;
        SECU_PrintAlgorithmID(out_file, &(whole_sig->signatureAlgorithm),
                              "Signature Algorithm", level);

        rawsig = whole_sig->signature;
        DER_ConvertBitString(&rawsig);
        SECU_PrintAsHex(out_file, &rawsig, "Signature", level);

        print_raw_certificates(out_file, whole_sig->derCerts, level);

        fprintf(out_file, "XXX verify the sig and print result\n");
    } else {
        fprintf(out_file, "No Signature\n");
    }

    CERT_DestroyOCSPRequest(request);
    return SECSuccess;
}

static void
print_revoked_info(FILE *out_file, ocspRevokedInfo *revoked_info, int level)
{
    SECU_PrintGeneralizedTime(out_file, &(revoked_info->revocationTime),
                              "Revocation Time", level);

    if (revoked_info->revocationReason != NULL) {
        SECU_PrintAsHex(out_file, revoked_info->revocationReason,
                        "Revocation Reason", level);
    } else {
        SECU_Indent(out_file, level);
        fprintf(out_file, "No Revocation Reason.\n");
    }
}

static void
print_cert_status(FILE *out_file, ocspCertStatus *status, int level)
{
    SECU_Indent(out_file, level);
    fprintf(out_file, "Status: ");

    switch (status->certStatusType) {
        case ocspCertStatus_good:
            fprintf(out_file, "Cert is good.\n");
            break;
        case ocspCertStatus_revoked:
            fprintf(out_file, "Cert has been revoked.\n");
            print_revoked_info(out_file, status->certStatusInfo.revokedInfo,
                               level + 1);
            break;
        case ocspCertStatus_unknown:
            fprintf(out_file, "Cert is unknown to responder.\n");
            break;
        default:
            fprintf(out_file, "Unrecognized status.\n");
            break;
    }
}

static void
print_single_response(FILE *out_file, CERTOCSPSingleResponse *single,
                      int level)
{
    print_ocsp_cert_id(out_file, single->certID, level);

    print_cert_status(out_file, single->certStatus, level);

    SECU_PrintGeneralizedTime(out_file, &(single->thisUpdate),
                              "This Update", level);

    if (single->nextUpdate != NULL) {
        SECU_PrintGeneralizedTime(out_file, single->nextUpdate,
                                  "Next Update", level);
    } else {
        SECU_Indent(out_file, level);
        fprintf(out_file, "No Next Update\n");
    }

    print_ocsp_extensions(out_file, single->singleExtensions,
                          "Single Response Extensions", level);
}

static void
print_responder_id(FILE *out_file, ocspResponderID *responderID, int level)
{
    SECU_Indent(out_file, level);
    fprintf(out_file, "Responder ID ");

    switch (responderID->responderIDType) {
        case ocspResponderID_byName:
            fprintf(out_file, "(byName):\n");
            SECU_PrintName(out_file, &(responderID->responderIDValue.name),
                           "Name", level + 1);
            break;
        case ocspResponderID_byKey:
            fprintf(out_file, "(byKey):\n");
            SECU_PrintAsHex(out_file, &(responderID->responderIDValue.keyHash),
                            "Key Hash", level + 1);
            break;
        default:
            fprintf(out_file, "Unrecognized Responder ID Type\n");
            break;
    }
}

static void
print_response_data(FILE *out_file, ocspResponseData *responseData, int level)
{
    SECU_Indent(out_file, level);
    fprintf(out_file, "Response Data:\n");
    level++;

    print_ocsp_version(out_file, &(responseData->version), level);

    print_responder_id(out_file, responseData->responderID, level);

    SECU_PrintGeneralizedTime(out_file, &(responseData->producedAt),
                              "Produced At", level);

    if (responseData->responses != NULL) {
        int i;

        for (i = 0; responseData->responses[i] != NULL; i++) {
            SECU_Indent(out_file, level);
            fprintf(out_file, "Response %d:\n", i);
            print_single_response(out_file, responseData->responses[i],
                                  level + 1);
        }
    } else {
        fprintf(out_file, "Response list is empty.\n");
    }

    print_ocsp_extensions(out_file, responseData->responseExtensions,
                          "Response Extensions", level);
}

static void
print_basic_response(FILE *out_file, ocspBasicOCSPResponse *basic, int level)
{
    SECItem rawsig;

    SECU_Indent(out_file, level);
    fprintf(out_file, "Basic OCSP Response:\n");
    level++;

    print_response_data(out_file, basic->tbsResponseData, level);

    SECU_PrintAlgorithmID(out_file,
                          &(basic->responseSignature.signatureAlgorithm),
                          "Signature Algorithm", level);

    rawsig = basic->responseSignature.signature;
    DER_ConvertBitString(&rawsig);
    SECU_PrintAsHex(out_file, &rawsig, "Signature", level);

    print_raw_certificates(out_file, basic->responseSignature.derCerts, level);
}

/*
 * Note this must match (exactly) the enumeration ocspResponseStatus.
 */
static char *responseStatusNames[] = {
    "successful (Response has valid confirmations)",
    "malformedRequest (Illegal confirmation request)",
    "internalError (Internal error in issuer)",
    "tryLater (Try again later)",
    "unused ((4) is not used)",
    "sigRequired (Must sign the request)",
    "unauthorized (Request unauthorized)"
};

/*
 * Decode the DER/BER-encoded item "data" as an OCSP response
 * and pretty-print the subfields.
 */
static SECStatus
print_response(FILE *out_file, SECItem *data, CERTCertDBHandle *handle)
{
    CERTOCSPResponse *response;
    int level = 0;

    PORT_Assert(out_file != NULL);
    PORT_Assert(data != NULL);
    if (out_file == NULL || data == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    response = CERT_DecodeOCSPResponse(data);
    if (response == NULL)
        return SECFailure;

    if (response->statusValue >= ocspResponse_min &&
        response->statusValue <= ocspResponse_max) {
        fprintf(out_file, "Response Status: %s\n",
                responseStatusNames[response->statusValue]);
    } else {
        fprintf(out_file,
                "Response Status: other (Status value %d out of defined range)\n",
                (int)response->statusValue);
    }

    if (response->statusValue == ocspResponse_successful) {
        ocspResponseBytes *responseBytes = response->responseBytes;
        SECStatus sigStatus;
        CERTCertificate *signerCert = NULL;

        PORT_Assert(responseBytes != NULL);

        level++;
        fprintf(out_file, "Response Bytes:\n");
        SECU_PrintObjectID(out_file, &(responseBytes->responseType),
                           "Response Type", level);
        switch (response->responseBytes->responseTypeTag) {
            case SEC_OID_PKIX_OCSP_BASIC_RESPONSE:
                print_basic_response(out_file,
                                     responseBytes->decodedResponse.basic,
                                     level);
                break;
            default:
                SECU_Indent(out_file, level);
                fprintf(out_file, "Unknown response syntax\n");
                break;
        }

        sigStatus = CERT_VerifyOCSPResponseSignature(response, handle,
                                                     NULL, &signerCert, NULL);
        SECU_Indent(out_file, level);
        fprintf(out_file, "Signature verification ");
        if (sigStatus != SECSuccess) {
            fprintf(out_file, "failed: %s\n", SECU_Strerror(PORT_GetError()));
        } else {
            fprintf(out_file, "succeeded.\n");
            if (signerCert != NULL) {
                SECU_PrintName(out_file, &signerCert->subject, "Signer",
                               level);
                CERT_DestroyCertificate(signerCert);
            } else {
                SECU_Indent(out_file, level);
                fprintf(out_file, "No signer cert returned?\n");
            }
        }
    } else {
        SECU_Indent(out_file, level);
        fprintf(out_file, "Unsuccessful response, no more information.\n");
    }

    CERT_DestroyOCSPResponse(response);
    return SECSuccess;
}

#endif /* NO_PP */

static SECStatus
cert_usage_from_char(const char *cert_usage_str, SECCertUsage *cert_usage)
{
    PORT_Assert(cert_usage_str != NULL);
    PORT_Assert(cert_usage != NULL);

    if (PORT_Strlen(cert_usage_str) != 1)
        return SECFailure;

    switch (*cert_usage_str) {
        case 'c':
            *cert_usage = certUsageSSLClient;
            break;
        case 's':
            *cert_usage = certUsageSSLServer;
            break;
        case 'I':
            *cert_usage = certUsageIPsec;
            break;
        case 'e':
            *cert_usage = certUsageEmailRecipient;
            break;
        case 'E':
            *cert_usage = certUsageEmailSigner;
            break;
        case 'S':
            *cert_usage = certUsageObjectSigner;
            break;
        case 'C':
            *cert_usage = certUsageVerifyCA;
            break;
        default:
            return SECFailure;
    }

    return SECSuccess;
}

int
main(int argc, char **argv)
{
    int retval;
    PRFileDesc *in_file;
    FILE *out_file; /* not PRFileDesc until SECU accepts it */
    int crequest, dresponse;
    int prequest, presponse;
    int ccert, vcert;
    const char *db_dir, *date_str, *cert_usage_str, *name;
    const char *responder_name, *responder_url, *signer_name;
    PRBool add_acceptable_responses, add_service_locator;
    SECItem *data = NULL;
    PLOptState *optstate;
    SECStatus rv;
    CERTCertDBHandle *handle = NULL;
    SECCertUsage cert_usage = certUsageSSLClient;
    PRTime verify_time;
    CERTCertificate *cert = NULL;
    PRBool ascii = PR_FALSE;

    retval = -1; /* what we return/exit with on error */

    program_name = PL_strrchr(argv[0], '/');
    program_name = program_name ? (program_name + 1) : argv[0];

    in_file = PR_STDIN;
    out_file = stdout;

    crequest = 0;
    dresponse = 0;
    prequest = 0;
    presponse = 0;
    ccert = 0;
    vcert = 0;

    db_dir = NULL;
    date_str = NULL;
    cert_usage_str = NULL;
    name = NULL;
    responder_name = NULL;
    responder_url = NULL;
    signer_name = NULL;

    add_acceptable_responses = PR_FALSE;
    add_service_locator = PR_FALSE;

    optstate = PL_CreateOptState(argc, argv, "AHLPR:S:V:d:l:pr:s:t:u:w:");
    if (optstate == NULL) {
        SECU_PrintError(program_name, "PL_CreateOptState failed");
        return retval;
    }

    while (PL_GetNextOpt(optstate) == PL_OPT_OK) {
        switch (optstate->option) {
            case '?':
                short_usage(program_name);
                return retval;

            case 'A':
                add_acceptable_responses = PR_TRUE;
                break;

            case 'H':
                long_usage(program_name);
                return retval;

            case 'L':
                add_service_locator = PR_TRUE;
                break;

            case 'P':
                presponse = 1;
                break;

            case 'R':
                dresponse = 1;
                name = optstate->value;
                break;

            case 'S':
                ccert = 1;
                name = optstate->value;
                break;

            case 'V':
                vcert = 1;
                name = optstate->value;
                break;

            case 'a':
                ascii = PR_TRUE;
                break;

            case 'd':
                db_dir = optstate->value;
                break;

            case 'l':
                responder_url = optstate->value;
                break;

            case 'p':
                prequest = 1;
                break;

            case 'r':
                crequest = 1;
                name = optstate->value;
                break;

            case 's':
                signer_name = optstate->value;
                break;

            case 't':
                responder_name = optstate->value;
                break;

            case 'u':
                cert_usage_str = optstate->value;
                break;

            case 'w':
                date_str = optstate->value;
                break;
        }
    }

    PL_DestroyOptState(optstate);

    if ((crequest + dresponse + prequest + presponse + ccert + vcert) != 1) {
        PR_fprintf(PR_STDERR, "%s: must specify exactly one command\n\n",
                   program_name);
        short_usage(program_name);
        return retval;
    }

    if (vcert) {
        if (cert_usage_str == NULL) {
            PR_fprintf(PR_STDERR, "%s: verification requires cert usage\n\n",
                       program_name);
            short_usage(program_name);
            return retval;
        }

        rv = cert_usage_from_char(cert_usage_str, &cert_usage);
        if (rv != SECSuccess) {
            PR_fprintf(PR_STDERR, "%s: invalid cert usage (\"%s\")\n\n",
                       program_name, cert_usage_str);
            long_usage(program_name);
            return retval;
        }
    }

    if (ccert + vcert) {
        if (responder_url != NULL || responder_name != NULL) {
            /*
            * To do a full status check, both the URL and the cert name
            * of the responder must be specified if either one is.
            */
            if (responder_url == NULL || responder_name == NULL) {
                if (responder_url == NULL)
                    PR_fprintf(PR_STDERR,
                               "%s: must also specify responder location\n\n",
                               program_name);
                else
                    PR_fprintf(PR_STDERR,
                               "%s: must also specify responder name\n\n",
                               program_name);
                short_usage(program_name);
                return retval;
            }
        }

        if (date_str != NULL) {
            rv = DER_AsciiToTime(&verify_time, (char *)date_str);
            if (rv != SECSuccess) {
                SECU_PrintError(program_name, "error converting time string");
                PR_fprintf(PR_STDERR, "\n");
                long_usage(program_name);
                return retval;
            }
        } else {
            verify_time = PR_Now();
        }
    }

    retval = -2; /* errors change from usage to runtime */

    /*
     * Initialize the NSPR and Security libraries.
     */
    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
    db_dir = SECU_ConfigDirectory(db_dir);
    rv = NSS_Init(db_dir);
    if (rv != SECSuccess) {
        SECU_PrintError(program_name, "NSS_Init failed");
        goto prdone;
    }
    SECU_RegisterDynamicOids();

    if (prequest + presponse) {
        MAKE_FILE_BINARY(stdin);
        data = read_file_into_item(in_file, siBuffer);
        if (data == NULL) {
            SECU_PrintError(program_name, "problem reading input");
            goto nssdone;
        }
    }

    if (crequest + dresponse + presponse + ccert + vcert) {
        handle = CERT_GetDefaultCertDB();
        if (handle == NULL) {
            SECU_PrintError(program_name, "problem getting certdb handle");
            goto nssdone;
        }

        /*
        * It would be fine to do the enable for all of these commands,
        * but this way we check that everything but an overall verify
        * can be done without it.  That is, that the individual pieces
        * work on their own.
        */
        if (vcert) {
            rv = CERT_EnableOCSPChecking(handle);
            if (rv != SECSuccess) {
                SECU_PrintError(program_name, "error enabling OCSP checking");
                goto nssdone;
            }
        }

        if ((ccert + vcert) && (responder_name != NULL)) {
            rv = CERT_SetOCSPDefaultResponder(handle, responder_url,
                                              responder_name);
            if (rv != SECSuccess) {
                SECU_PrintError(program_name,
                                "error setting default responder");
                goto nssdone;
            }

            rv = CERT_EnableOCSPDefaultResponder(handle);
            if (rv != SECSuccess) {
                SECU_PrintError(program_name,
                                "error enabling default responder");
                goto nssdone;
            }
        }
    }

#define NOTYET(opt)                                         \
    {                                                       \
        PR_fprintf(PR_STDERR, "%s not yet working\n", opt); \
        exit(-1);                                           \
    }

    if (name) {
        cert = find_certificate(handle, name, ascii);
    }

    if (crequest) {
        if (signer_name != NULL) {
            NOTYET("-s");
        }
        rv = create_request(out_file, handle, cert, add_service_locator,
                            add_acceptable_responses);
    } else if (dresponse) {
        if (signer_name != NULL) {
            NOTYET("-s");
        }
        rv = dump_response(out_file, handle, cert, responder_url);
    } else if (prequest) {
        rv = print_request(out_file, data);
    } else if (presponse) {
        rv = print_response(out_file, data, handle);
    } else if (ccert) {
        if (signer_name != NULL) {
            NOTYET("-s");
        }
        rv = get_cert_status(out_file, handle, cert, name, verify_time);
    } else if (vcert) {
        if (signer_name != NULL) {
            NOTYET("-s");
        }
        rv = verify_cert(out_file, handle, cert, name, cert_usage, verify_time);
    }

    if (rv != SECSuccess)
        SECU_PrintError(program_name, "error performing requested operation");
    else
        retval = 0;

nssdone:
    if (cert) {
        CERT_DestroyCertificate(cert);
    }

    if (data != NULL) {
        SECITEM_FreeItem(data, PR_TRUE);
    }

    if (handle != NULL) {
        CERT_DisableOCSPDefaultResponder(handle);
        CERT_DisableOCSPChecking(handle);
    }

    if (NSS_Shutdown() != SECSuccess) {
        retval = 1;
    }

prdone:
    PR_Cleanup();
    return retval;
}
