/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "seccomon.h"
#include "nss.h"
#include "key.h"
#include "cert.h"
#include "pk11func.h"
#include "secmod.h"
#include "cmmf.h"
#include "crmf.h"
#include "base64.h"
#include "secasn1.h"
#include "cryptohi.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define DEFAULT_ALLOC_SIZE 200
#define DEFAULT_CGI_VARS 20

typedef struct CGIVariableStr {
    char *name;
    char *value;
} CGIVariable;

typedef struct CGIVarTableStr {
    CGIVariable **variables;
    int numVars;
    int numAlloc;
} CGIVarTable;

typedef struct CertResponseInfoStr {
    CERTCertificate *cert;
    long certReqID;
} CertResponseInfo;

typedef struct ChallengeCreationInfoStr {
    long random;
    SECKEYPublicKey *pubKey;
} ChallengeCreationInfo;

char *missingVar = NULL;

/*
 * Error values.
 */
typedef enum {
    NO_ERROR = 0,
    NSS_INIT_FAILED,
    AUTH_FAILED,
    REQ_CGI_VAR_NOT_PRESENT,
    CRMF_REQ_NOT_PRESENT,
    BAD_ASCII_FOR_REQ,
    CGI_VAR_MISSING,
    COULD_NOT_FIND_CA,
    COULD_NOT_DECODE_REQS,
    OUT_OF_MEMORY,
    ERROR_RETRIEVING_REQUEST_MSG,
    ERROR_RETRIEVING_CERT_REQUEST,
    ERROR_RETRIEVING_SUBJECT_FROM_REQ,
    ERROR_RETRIEVING_PUBLIC_KEY_FROM_REQ,
    ERROR_CREATING_NEW_CERTIFICATE,
    COULD_NOT_START_EXTENSIONS,
    ERROR_RETRIEVING_EXT_FROM_REQ,
    ERROR_ADDING_EXT_TO_CERT,
    ERROR_ENDING_EXTENSIONS,
    COULD_NOT_FIND_ISSUER_PRIVATE_KEY,
    UNSUPPORTED_SIGN_OPERATION_FOR_ISSUER,
    ERROR_SETTING_SIGN_ALG,
    ERROR_ENCODING_NEW_CERT,
    ERROR_SIGNING_NEW_CERT,
    ERROR_CREATING_CERT_REP_CONTENT,
    ERROR_CREATING_SINGLE_CERT_RESPONSE,
    ERROR_SETTING_CERT_RESPONSES,
    ERROR_CREATING_CA_LIST,
    ERROR_ADDING_ISSUER_TO_CA_LIST,
    ERROR_ENCODING_CERT_REP_CONTENT,
    NO_POP_FOR_REQUEST,
    UNSUPPORTED_POP,
    ERROR_RETRIEVING_POP_SIGN_KEY,
    ERROR_RETRIEVING_ALG_ID_FROM_SIGN_KEY,
    ERROR_RETRIEVING_SIGNATURE_FROM_POP_SIGN_KEY,
    DO_CHALLENGE_RESPONSE,
    ERROR_RETRIEVING_PUB_KEY_FROM_NEW_CERT,
    ERROR_ENCODING_CERT_REQ_FOR_POP,
    ERROR_VERIFYING_SIGNATURE_POP,
    ERROR_RETRIEVING_PUB_KEY_FOR_CHALL,
    ERROR_CREATING_EMPTY_CHAL_CONTENT,
    ERROR_EXTRACTING_GEN_NAME_FROM_ISSUER,
    ERROR_SETTING_CHALLENGE,
    ERROR_ENCODING_CHALL,
    ERROR_CONVERTING_CHALL_TO_BASE64,
    ERROR_CONVERTING_RESP_FROM_CHALL_TO_BIN,
    ERROR_CREATING_KEY_RESP_FROM_DER,
    ERROR_RETRIEVING_CLIENT_RESPONSE_TO_CHALLENGE,
    ERROR_RETURNED_CHALL_NOT_VALUE_EXPECTED,
    ERROR_GETTING_KEY_ENCIPHERMENT,
    ERROR_NO_POP_FOR_PRIVKEY,
    ERROR_UNSUPPORTED_POPOPRIVKEY_TYPE
} ErrorCode;

const char *
CGITableFindValue(CGIVarTable *varTable, const char *key);

void
spitOutHeaders(void)
{
    printf("Content-type: text/html\n\n");
}

void
dumpRequest(CGIVarTable *varTable)
{
    int i;
    CGIVariable *var;

    printf("<table border=1 cellpadding=1 cellspacing=1 width=\"100%%\">\n");
    printf("<tr><td><b><center>Variable Name<center></b></td>"
           "<td><b><center>Value</center></b></td></tr>\n");
    for (i = 0; i < varTable->numVars; i++) {
        var = varTable->variables[i];
        printf("<tr><td><pre>%s</pre></td><td><pre>%s</pre></td></tr>\n",
               var->name, var->value);
    }
    printf("</table>\n");
}

void
echo_request(CGIVarTable *varTable)
{
    spitOutHeaders();
    printf("<html><head><title>CGI Echo Page</title></head>\n"
           "<body><h1>Got the following request</h1>\n");
    dumpRequest(varTable);
    printf("</body></html>");
}

void
processVariable(CGIVariable *var)
{
    char *plusSign, *percentSign;

    /*First look for all of the '+' and convert them to spaces */
    plusSign = var->value;
    while ((plusSign = strchr(plusSign, '+')) != NULL) {
        *plusSign = ' ';
    }
    percentSign = var->value;
    while ((percentSign = strchr(percentSign, '%')) != NULL) {
        char string[3];
        int value;

        string[0] = percentSign[1];
        string[1] = percentSign[2];
        string[2] = '\0';

        sscanf(string, "%x", &value);
        *percentSign = (char)value;
        memmove(&percentSign[1], &percentSign[3], 1 + strlen(&percentSign[3]));
    }
}

char *
parseNextVariable(CGIVarTable *varTable, char *form_output)
{
    char *ampersand, *equal;
    CGIVariable *var;

    if (varTable->numVars == varTable->numAlloc) {
        CGIVariable **newArr = realloc(varTable->variables,
                                       (varTable->numAlloc + DEFAULT_CGI_VARS) * sizeof(CGIVariable *));
        if (newArr == NULL) {
            return NULL;
        }
        varTable->variables = newArr;
        varTable->numAlloc += DEFAULT_CGI_VARS;
    }
    equal = strchr(form_output, '=');
    if (equal == NULL) {
        return NULL;
    }
    ampersand = strchr(equal, '&');
    if (ampersand == NULL) {
        return NULL;
    }
    equal[0] = '\0';
    if (ampersand != NULL) {
        ampersand[0] = '\0';
    }
    var = malloc(sizeof(CGIVariable));
    var->name = form_output;
    var->value = &equal[1];
    varTable->variables[varTable->numVars] = var;
    varTable->numVars++;
    processVariable(var);
    return (ampersand != NULL) ? &ampersand[1] : NULL;
}

void
ParseInputVariables(CGIVarTable *varTable, char *form_output)
{
    varTable->variables = malloc(sizeof(CGIVariable *) * DEFAULT_CGI_VARS);
    varTable->numVars = 0;
    varTable->numAlloc = DEFAULT_CGI_VARS;
    while (form_output && form_output[0] != '\0') {
        form_output = parseNextVariable(varTable, form_output);
    }
}

const char *
CGITableFindValue(CGIVarTable *varTable, const char *key)
{
    const char *retVal = NULL;
    int i;

    for (i = 0; i < varTable->numVars; i++) {
        if (strcmp(varTable->variables[i]->name, key) == 0) {
            retVal = varTable->variables[i]->value;
            break;
        }
    }
    return retVal;
}

char *
passwordCallback(PK11SlotInfo *slot, PRBool retry, void *arg)
{
    const char *passwd;
    if (retry) {
        return NULL;
    }
    passwd = CGITableFindValue((CGIVarTable *)arg, "dbPassword");
    if (passwd == NULL) {
        return NULL;
    }
    return PORT_Strdup(passwd);
}

ErrorCode
initNSS(CGIVarTable *varTable)
{
    const char *nssDir;
    PK11SlotInfo *keySlot;
    SECStatus rv;

    nssDir = CGITableFindValue(varTable, "NSSDirectory");
    if (nssDir == NULL) {
        missingVar = "NSSDirectory";
        return REQ_CGI_VAR_NOT_PRESENT;
    }
    rv = NSS_Init(nssDir);
    if (rv != SECSuccess) {
        return NSS_INIT_FAILED;
    }
    PK11_SetPasswordFunc(passwordCallback);
    keySlot = PK11_GetInternalKeySlot();
    rv = PK11_Authenticate(keySlot, PR_FALSE, varTable);
    PK11_FreeSlot(keySlot);
    if (rv != SECSuccess) {
        return AUTH_FAILED;
    }
    return NO_ERROR;
}

void
dumpErrorMessage(ErrorCode errNum)
{
    spitOutHeaders();
    printf("<html><head><title>Error</title></head><body><h1>Error processing "
           "data</h1> Received the error %d<p>",
           errNum);
    if (errNum == REQ_CGI_VAR_NOT_PRESENT) {
        printf("The missing variable is %s.", missingVar);
    }
    printf("<i>More useful information here in the future.</i></body></html>");
}

ErrorCode
initOldCertReq(CERTCertificateRequest *oldCertReq,
               CERTName *subject, CERTSubjectPublicKeyInfo *spki)
{
    PLArenaPool *poolp;

    poolp = oldCertReq->arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    SEC_ASN1EncodeInteger(poolp, &oldCertReq->version,
                          SEC_CERTIFICATE_VERSION_3);
    CERT_CopyName(poolp, &oldCertReq->subject, subject);
    SECKEY_CopySubjectPublicKeyInfo(poolp, &oldCertReq->subjectPublicKeyInfo,
                                    spki);
    oldCertReq->attributes = NULL;
    return NO_ERROR;
}

ErrorCode
addExtensions(CERTCertificate *newCert, CRMFCertRequest *certReq)
{
    int numExtensions, i;
    void *extHandle;
    ErrorCode rv = NO_ERROR;
    CRMFCertExtension *ext;
    SECStatus srv;

    numExtensions = CRMF_CertRequestGetNumberOfExtensions(certReq);
    if (numExtensions == 0) {
        /* No extensions to add */
        return NO_ERROR;
    }
    extHandle = CERT_StartCertExtensions(newCert);
    if (extHandle == NULL) {
        rv = COULD_NOT_START_EXTENSIONS;
        goto loser;
    }
    for (i = 0; i < numExtensions; i++) {
        ext = CRMF_CertRequestGetExtensionAtIndex(certReq, i);
        if (ext == NULL) {
            rv = ERROR_RETRIEVING_EXT_FROM_REQ;
        }
        srv = CERT_AddExtension(extHandle, CRMF_CertExtensionGetOidTag(ext),
                                CRMF_CertExtensionGetValue(ext),
                                CRMF_CertExtensionGetIsCritical(ext), PR_FALSE);
        if (srv != SECSuccess) {
            rv = ERROR_ADDING_EXT_TO_CERT;
        }
    }
    srv = CERT_FinishExtensions(extHandle);
    if (srv != SECSuccess) {
        rv = ERROR_ENDING_EXTENSIONS;
        goto loser;
    }
    return NO_ERROR;
loser:
    return rv;
}

void
writeOutItem(const char *filePath, SECItem *der)
{
    PRFileDesc *outfile;

    outfile = PR_Open(filePath,
                      PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                      0666);
    PR_Write(outfile, der->data, der->len);
    PR_Close(outfile);
}

ErrorCode
createNewCert(CERTCertificate **issuedCert, CERTCertificateRequest *oldCertReq,
              CRMFCertReqMsg *currReq, CRMFCertRequest *certReq,
              CERTCertificate *issuerCert, CGIVarTable *varTable)
{
    CERTCertificate *newCert = NULL;
    CERTValidity *validity;
    PRExplodedTime printableTime;
    PRTime now, after;
    ErrorCode rv = NO_ERROR;
    SECKEYPrivateKey *issuerPrivKey;
    SECItem derCert = { 0 };
    SECOidTag signTag;
    SECStatus srv;
    long version;

    now = PR_Now();
    PR_ExplodeTime(now, PR_GMTParameters, &printableTime);
    printableTime.tm_month += 9;
    after = PR_ImplodeTime(&printableTime);
    validity = CERT_CreateValidity(now, after);
    newCert = *issuedCert =
        CERT_CreateCertificate(rand(), &(issuerCert->subject), validity,
                               oldCertReq);
    if (newCert == NULL) {
        rv = ERROR_CREATING_NEW_CERTIFICATE;
        goto loser;
    }
    rv = addExtensions(newCert, certReq);
    if (rv != NO_ERROR) {
        goto loser;
    }
    issuerPrivKey = PK11_FindKeyByAnyCert(issuerCert, varTable);
    if (issuerPrivKey == NULL) {
        rv = COULD_NOT_FIND_ISSUER_PRIVATE_KEY;
    }
    signTag = SEC_GetSignatureAlgorithmOidTag(issuerPrivatekey->keytype,
                                              SEC_OID_UNKNOWN);
    if (signTag == SEC_OID_UNKNOWN) {
        rv = UNSUPPORTED_SIGN_OPERATION_FOR_ISSUER;
        goto loser;
    }
    srv = SECOID_SetAlgorithmID(newCert->arena, &newCert->signature,
                                signTag, 0);
    if (srv != SECSuccess) {
        rv = ERROR_SETTING_SIGN_ALG;
        goto loser;
    }
    srv = CRMF_CertRequestGetCertTemplateVersion(certReq, &version);
    if (srv != SECSuccess) {
        /* No version included in the request */
        *(newCert->version.data) = SEC_CERTIFICATE_VERSION_3;
    } else {
        SECITEM_FreeItem(&newCert->version, PR_FALSE);
        SEC_ASN1EncodeInteger(newCert->arena, &newCert->version, version);
    }
    SEC_ASN1EncodeItem(newCert->arena, &derCert, newCert,
                       CERT_CertificateTemplate);
    if (derCert.data == NULL) {
        rv = ERROR_ENCODING_NEW_CERT;
        goto loser;
    }
    srv = SEC_DerSignData(newCert->arena, &(newCert->derCert), derCert.data,
                          derCert.len, issuerPrivKey, signTag);
    if (srv != SECSuccess) {
        rv = ERROR_SIGNING_NEW_CERT;
        goto loser;
    }
#ifdef WRITE_OUT_RESPONSE
    writeOutItem("newcert.der", &newCert->derCert);
#endif
    return NO_ERROR;
loser:
    *issuedCert = NULL;
    if (newCert) {
        CERT_DestroyCertificate(newCert);
    }
    return rv;
}

void
formatCMMFResponse(char *nickname, char *base64Response)
{
    char *currLine, *nextLine;

    printf("var retVal = crypto.importUserCertificates(\"%s\",\n", nickname);
    currLine = base64Response;
    while (1) {
        nextLine = strchr(currLine, '\n');
        if (nextLine == NULL) {
            /* print out the last line here. */
            printf("\"%s\",\n", currLine);
            break;
        }
        nextLine[0] = '\0';
        printf("\"%s\\n\"+\n", currLine);
        currLine = nextLine + 1;
    }
    printf("true);\n"
           "if(retVal == '') {\n"
           "\tdocument.write(\"<h1>New Certificate Successfully Imported.</h1>\");\n"
           "} else {\n"
           "\tdocument.write(\"<h2>Unable to import New Certificate</h2>\");\n"
           "\tdocument.write(\"crypto.importUserCertificates returned <b>\");\n"
           "\tdocument.write(retVal);\n"
           "\tdocument.write(\"</b>\");\n"
           "}\n");
}

void
spitOutCMMFResponse(char *nickname, char *base64Response)
{
    spitOutHeaders();
    printf("<html>\n<head>\n<title>CMMF Resonse Page</title>\n</head>\n\n"
           "<body><h1>CMMF Response Page</h1>\n"
           "<script language=\"JavaScript\">\n"
           "<!--\n");
    formatCMMFResponse(nickname, base64Response);
    printf("// -->\n"
           "</script>\n</body>\n</html>");
}

char *
getNickname(CERTCertificate *cert)
{
    char *nickname;

    if (cert->nickname != NULL) {
        return cert->nickname;
    }
    nickname = CERT_GetCommonName(&cert->subject);
    if (nickname != NULL) {
        return nickname;
    }
    return CERT_NameToAscii(&cert->subject);
}

ErrorCode
createCMMFResponse(CertResponseInfo *issuedCerts, int numCerts,
                   CERTCertificate *issuerCert, char **base64der)
{
    CMMFCertRepContent *certRepContent = NULL;
    ErrorCode rv = NO_ERROR;
    CMMFCertResponse **responses, *currResponse;
    CERTCertList *caList;
    int i;
    SECStatus srv;
    PLArenaPool *poolp;
    SECItem *der;

    certRepContent = CMMF_CreateCertRepContent();
    if (certRepContent == NULL) {
        rv = ERROR_CREATING_CERT_REP_CONTENT;
        goto loser;
    }
    responses = PORT_NewArray(CMMFCertResponse *, numCerts);
    if (responses == NULL) {
        rv = OUT_OF_MEMORY;
        goto loser;
    }
    for (i = 0; i < numCerts; i++) {
        responses[i] = currResponse =
            CMMF_CreateCertResponse(issuedCerts[i].certReqID);
        if (currResponse == NULL) {
            rv = ERROR_CREATING_SINGLE_CERT_RESPONSE;
            goto loser;
        }
        CMMF_CertResponseSetPKIStatusInfoStatus(currResponse, cmmfGranted);
        CMMF_CertResponseSetCertificate(currResponse, issuedCerts[i].cert);
    }
    srv = CMMF_CertRepContentSetCertResponses(certRepContent, responses,
                                              numCerts);
    if (srv != SECSuccess) {
        rv = ERROR_SETTING_CERT_RESPONSES;
        goto loser;
    }
    caList = CERT_NewCertList();
    if (caList == NULL) {
        rv = ERROR_CREATING_CA_LIST;
        goto loser;
    }
    srv = CERT_AddCertToListTail(caList, issuerCert);
    if (srv != SECSuccess) {
        rv = ERROR_ADDING_ISSUER_TO_CA_LIST;
        goto loser;
    }
    srv = CMMF_CertRepContentSetCAPubs(certRepContent, caList);
    CERT_DestroyCertList(caList);
    poolp = PORT_NewArena(1024);
    der = SEC_ASN1EncodeItem(poolp, NULL, certRepContent,
                             CMMFCertRepContentTemplate);
    if (der == NULL) {
        rv = ERROR_ENCODING_CERT_REP_CONTENT;
        goto loser;
    }
#ifdef WRITE_OUT_RESPONSE
    writeOutItem("CertRepContent.der", der);
#endif
    *base64der = BTOA_DataToAscii(der->data, der->len);
    return NO_ERROR;
loser:
    return rv;
}

ErrorCode
issueCerts(CertResponseInfo *issuedCerts, int numCerts,
           CERTCertificate *issuerCert)
{
    ErrorCode rv;
    char *base64Response;

    rv = createCMMFResponse(issuedCerts, numCerts, issuerCert, &base64Response);
    if (rv != NO_ERROR) {
        goto loser;
    }
    spitOutCMMFResponse(getNickname(issuedCerts[0].cert), base64Response);
    return NO_ERROR;
loser:
    return rv;
}

ErrorCode
verifySignature(CGIVarTable *varTable, CRMFCertReqMsg *currReq,
                CRMFCertRequest *certReq, CERTCertificate *newCert)
{
    SECStatus srv;
    ErrorCode rv = NO_ERROR;
    CRMFPOPOSigningKey *signKey = NULL;
    SECAlgorithmID *algID = NULL;
    SECItem *signature = NULL;
    SECKEYPublicKey *pubKey = NULL;
    SECItem *reqDER = NULL;

    srv = CRMF_CertReqMsgGetPOPOSigningKey(currReq, &signKey);
    if (srv != SECSuccess || signKey == NULL) {
        rv = ERROR_RETRIEVING_POP_SIGN_KEY;
        goto loser;
    }
    algID = CRMF_POPOSigningKeyGetAlgID(signKey);
    if (algID == NULL) {
        rv = ERROR_RETRIEVING_ALG_ID_FROM_SIGN_KEY;
        goto loser;
    }
    signature = CRMF_POPOSigningKeyGetSignature(signKey);
    if (signature == NULL) {
        rv = ERROR_RETRIEVING_SIGNATURE_FROM_POP_SIGN_KEY;
        goto loser;
    }
    /* Make the length the number of bytes instead of bits */
    signature->len = (signature->len + 7) / 8;
    pubKey = CERT_ExtractPublicKey(newCert);
    if (pubKey == NULL) {
        rv = ERROR_RETRIEVING_PUB_KEY_FROM_NEW_CERT;
        goto loser;
    }
    reqDER = SEC_ASN1EncodeItem(NULL, NULL, certReq, CRMFCertRequestTemplate);
    if (reqDER == NULL) {
        rv = ERROR_ENCODING_CERT_REQ_FOR_POP;
        goto loser;
    }
    srv = VFY_VerifyDataWithAlgorithmID(reqDER->data, reqDER->len, pubKey,
                                        signature, &algID->algorithm, NULL, varTable);
    if (srv != SECSuccess) {
        rv = ERROR_VERIFYING_SIGNATURE_POP;
        goto loser;
    }
/* Fall thru in successfull case. */
loser:
    if (pubKey != NULL) {
        SECKEY_DestroyPublicKey(pubKey);
    }
    if (reqDER != NULL) {
        SECITEM_FreeItem(reqDER, PR_TRUE);
    }
    if (signature != NULL) {
        SECITEM_FreeItem(signature, PR_TRUE);
    }
    if (algID != NULL) {
        SECOID_DestroyAlgorithmID(algID, PR_TRUE);
    }
    if (signKey != NULL) {
        CRMF_DestroyPOPOSigningKey(signKey);
    }
    return rv;
}

ErrorCode
doChallengeResponse(CGIVarTable *varTable, CRMFCertReqMsg *currReq,
                    CRMFCertRequest *certReq, CERTCertificate *newCert,
                    ChallengeCreationInfo *challs, int *numChall)
{
    CRMFPOPOPrivKey *privKey = NULL;
    CRMFPOPOPrivKeyChoice privKeyChoice;
    SECStatus srv;
    ErrorCode rv = NO_ERROR;

    srv = CRMF_CertReqMsgGetPOPKeyEncipherment(currReq, &privKey);
    if (srv != SECSuccess || privKey == NULL) {
        rv = ERROR_GETTING_KEY_ENCIPHERMENT;
        goto loser;
    }
    privKeyChoice = CRMF_POPOPrivKeyGetChoice(privKey);
    CRMF_DestroyPOPOPrivKey(privKey);
    switch (privKeyChoice) {
        case crmfSubsequentMessage:
            challs = &challs[*numChall];
            challs->random = rand();
            challs->pubKey = CERT_ExtractPublicKey(newCert);
            if (challs->pubKey == NULL) {
                rv =
                    ERROR_RETRIEVING_PUB_KEY_FOR_CHALL;
                goto loser;
            }
            (*numChall)++;
            rv = DO_CHALLENGE_RESPONSE;
            break;
        case crmfThisMessage:
            /* There'd better be a PKIArchiveControl in this message */
            if (!CRMF_CertRequestIsControlPresent(certReq,
                                                  crmfPKIArchiveOptionsControl)) {
                rv =
                    ERROR_NO_POP_FOR_PRIVKEY;
                goto loser;
            }
            break;
        default:
            rv = ERROR_UNSUPPORTED_POPOPRIVKEY_TYPE;
            goto loser;
    }
loser:
    return rv;
}

ErrorCode
doProofOfPossession(CGIVarTable *varTable, CRMFCertReqMsg *currReq,
                    CRMFCertRequest *certReq, CERTCertificate *newCert,
                    ChallengeCreationInfo *challs, int *numChall)
{
    CRMFPOPChoice popChoice;
    ErrorCode rv = NO_ERROR;

    popChoice = CRMF_CertReqMsgGetPOPType(currReq);
    if (popChoice == crmfNoPOPChoice) {
        rv = NO_POP_FOR_REQUEST;
        goto loser;
    }
    switch (popChoice) {
        case crmfSignature:
            rv = verifySignature(varTable, currReq, certReq, newCert);
            break;
        case crmfKeyEncipherment:
            rv = doChallengeResponse(varTable, currReq, certReq, newCert,
                                     challs, numChall);
            break;
        case crmfRAVerified:
        case crmfKeyAgreement:
        default:
            rv = UNSUPPORTED_POP;
            goto loser;
    }
loser:
    return rv;
}

void
convertB64ToJS(char *base64)
{
    int i;

    for (i = 0; base64[i] != '\0'; i++) {
        if (base64[i] == '\n') {
            printf("\\n");
        } else {
            printf("%c", base64[i]);
        }
    }
}

void
formatChallenge(char *chall64, char *certRepContentDER,
                ChallengeCreationInfo *challInfo, int numChalls)
{
    printf("function respondToChallenge() {\n"
           "  var chalForm = document.chalForm;\n\n"
           "  chalForm.CertRepContent.value = '");
    convertB64ToJS(certRepContentDER);
    printf("';\n"
           "  chalForm.ChallResponse.value = crypto.popChallengeResponse('");
    convertB64ToJS(chall64);
    printf("');\n"
           "  chalForm.submit();\n"
           "}\n");
}

void
spitOutChallenge(char *chall64, char *certRepContentDER,
                 ChallengeCreationInfo *challInfo, int numChalls,
                 char *nickname)
{
    int i;

    spitOutHeaders();
    printf("<html>\n"
           "<head>\n"
           "<title>Challenge Page</title>\n"
           "<script language=\"JavaScript\">\n"
           "<!--\n");
    /* The JavaScript function actually gets defined within
   * this function call
   */
    formatChallenge(chall64, certRepContentDER, challInfo, numChalls);
    printf("// -->\n"
           "</script>\n"
           "</head>\n"
           "<body onLoad='respondToChallenge()'>\n"
           "<h1>Cartman is now responding to the Challenge "
           "presented by the CGI</h1>\n"
           "<form action='crmfcgi' method='post' name='chalForm'>\n"
           "<input type='hidden' name=CertRepContent value=''>\n"
           "<input type='hidden' name=ChallResponse value=''>\n");
    for (i = 0; i < numChalls; i++) {
        printf("<input type='hidden' name='chal%d' value='%d'>\n",
               i + 1, challInfo[i].random);
    }
    printf("<input type='hidden' name='nickname' value='%s'>\n", nickname);
    printf("</form>\n</body>\n</html>");
}

ErrorCode
issueChallenge(CertResponseInfo *issuedCerts, int numCerts,
               ChallengeCreationInfo *challInfo, int numChalls,
               CERTCertificate *issuer, CGIVarTable *varTable)
{
    ErrorCode rv = NO_ERROR;
    CMMFPOPODecKeyChallContent *chalContent = NULL;
    int i;
    SECStatus srv;
    PLArenaPool *poolp;
    CERTGeneralName *genName;
    SECItem *challDER = NULL;
    char *chall64, *certRepContentDER;

    rv = createCMMFResponse(issuedCerts, numCerts, issuer,
                            &certRepContentDER);
    if (rv != NO_ERROR) {
        goto loser;
    }
    chalContent = CMMF_CreatePOPODecKeyChallContent();
    if (chalContent == NULL) {
        rv = ERROR_CREATING_EMPTY_CHAL_CONTENT;
        goto loser;
    }
    poolp = PORT_NewArena(1024);
    if (poolp == NULL) {
        rv = OUT_OF_MEMORY;
        goto loser;
    }
    genName = CERT_GetCertificateNames(issuer, poolp);
    if (genName == NULL) {
        rv = ERROR_EXTRACTING_GEN_NAME_FROM_ISSUER;
        goto loser;
    }
    for (i = 0; i < numChalls; i++) {
        srv = CMMF_POPODecKeyChallContentSetNextChallenge(chalContent,
                                                          challInfo[i].random,
                                                          genName,
                                                          challInfo[i].pubKey,
                                                          varTable);
        SECKEY_DestroyPublicKey(challInfo[i].pubKey);
        if (srv != SECSuccess) {
            rv = ERROR_SETTING_CHALLENGE;
            goto loser;
        }
    }
    challDER = SEC_ASN1EncodeItem(NULL, NULL, chalContent,
                                  CMMFPOPODecKeyChallContentTemplate);
    if (challDER == NULL) {
        rv = ERROR_ENCODING_CHALL;
        goto loser;
    }
    chall64 = BTOA_DataToAscii(challDER->data, challDER->len);
    SECITEM_FreeItem(challDER, PR_TRUE);
    if (chall64 == NULL) {
        rv = ERROR_CONVERTING_CHALL_TO_BASE64;
        goto loser;
    }
    spitOutChallenge(chall64, certRepContentDER, challInfo, numChalls,
                     getNickname(issuedCerts[0].cert));
loser:
    return rv;
}

ErrorCode
processRequest(CGIVarTable *varTable)
{
    CERTCertDBHandle *certdb;
    SECKEYKeyDBHandle *keydb;
    CRMFCertReqMessages *certReqs = NULL;
    const char *crmfReq;
    const char *caNickname;
    CERTCertificate *caCert = NULL;
    CertResponseInfo *issuedCerts = NULL;
    CERTSubjectPublicKeyInfo spki = { 0 };
    ErrorCode rv = NO_ERROR;
    PRBool doChallengeResponse = PR_FALSE;
    SECItem der = { 0 };
    SECStatus srv;
    CERTCertificateRequest oldCertReq = { 0 };
    CRMFCertReqMsg **reqMsgs = NULL, *currReq = NULL;
    CRMFCertRequest **reqs = NULL, *certReq = NULL;
    CERTName subject = { 0 };
    int numReqs, i;
    ChallengeCreationInfo *challInfo = NULL;
    int numChalls = 0;

    certdb = CERT_GetDefaultCertDB();
    keydb = SECKEY_GetDefaultKeyDB();
    crmfReq = CGITableFindValue(varTable, "CRMFRequest");
    if (crmfReq == NULL) {
        rv = CGI_VAR_MISSING;
        missingVar = "CRMFRequest";
        goto loser;
    }
    caNickname = CGITableFindValue(varTable, "CANickname");
    if (caNickname == NULL) {
        rv = CGI_VAR_MISSING;
        missingVar = "CANickname";
        goto loser;
    }
    caCert = CERT_FindCertByNickname(certdb, caNickname);
    if (caCert == NULL) {
        rv = COULD_NOT_FIND_CA;
        goto loser;
    }
    srv = ATOB_ConvertAsciiToItem(&der, crmfReq);
    if (srv != SECSuccess) {
        rv = BAD_ASCII_FOR_REQ;
        goto loser;
    }
    certReqs = CRMF_CreateCertReqMessagesFromDER(der.data, der.len);
    SECITEM_FreeItem(&der, PR_FALSE);
    if (certReqs == NULL) {
        rv = COULD_NOT_DECODE_REQS;
        goto loser;
    }
    numReqs = CRMF_CertReqMessagesGetNumMessages(certReqs);
    issuedCerts = PORT_ZNewArray(CertResponseInfo, numReqs);
    challInfo = PORT_ZNewArray(ChallengeCreationInfo, numReqs);
    if (issuedCerts == NULL || challInfo == NULL) {
        rv = OUT_OF_MEMORY;
        goto loser;
    }
    reqMsgs = PORT_ZNewArray(CRMFCertReqMsg *, numReqs);
    reqs = PORT_ZNewArray(CRMFCertRequest *, numReqs);
    if (reqMsgs == NULL || reqs == NULL) {
        rv = OUT_OF_MEMORY;
        goto loser;
    }
    for (i = 0; i < numReqs; i++) {
        currReq = reqMsgs[i] =
            CRMF_CertReqMessagesGetCertReqMsgAtIndex(certReqs, i);
        if (currReq == NULL) {
            rv = ERROR_RETRIEVING_REQUEST_MSG;
            goto loser;
        }
        certReq = reqs[i] = CRMF_CertReqMsgGetCertRequest(currReq);
        if (certReq == NULL) {
            rv = ERROR_RETRIEVING_CERT_REQUEST;
            goto loser;
        }
        srv = CRMF_CertRequestGetCertTemplateSubject(certReq, &subject);
        if (srv != SECSuccess) {
            rv = ERROR_RETRIEVING_SUBJECT_FROM_REQ;
            goto loser;
        }
        srv = CRMF_CertRequestGetCertTemplatePublicKey(certReq, &spki);
        if (srv != SECSuccess) {
            rv = ERROR_RETRIEVING_PUBLIC_KEY_FROM_REQ;
            goto loser;
        }
        rv = initOldCertReq(&oldCertReq, &subject, &spki);
        if (rv != NO_ERROR) {
            goto loser;
        }
        rv = createNewCert(&issuedCerts[i].cert, &oldCertReq, currReq, certReq,
                           caCert, varTable);
        if (rv != NO_ERROR) {
            goto loser;
        }
        rv = doProofOfPossession(varTable, currReq, certReq, issuedCerts[i].cert,
                                 challInfo, &numChalls);
        if (rv != NO_ERROR) {
            if (rv == DO_CHALLENGE_RESPONSE) {
                doChallengeResponse = PR_TRUE;
            } else {
                goto loser;
            }
        }
        CRMF_CertReqMsgGetID(currReq, &issuedCerts[i].certReqID);
        CRMF_DestroyCertReqMsg(currReq);
        CRMF_DestroyCertRequest(certReq);
    }
    if (doChallengeResponse) {
        rv = issueChallenge(issuedCerts, numReqs, challInfo, numChalls, caCert,
                            varTable);
    } else {
        rv = issueCerts(issuedCerts, numReqs, caCert);
    }
loser:
    if (certReqs != NULL) {
        CRMF_DestroyCertReqMessages(certReqs);
    }
    return rv;
}

ErrorCode
processChallengeResponse(CGIVarTable *varTable, const char *certRepContent)
{
    SECItem binDER = { 0 };
    SECStatus srv;
    ErrorCode rv = NO_ERROR;
    const char *clientResponse;
    const char *formChalValue;
    const char *nickname;
    CMMFPOPODecKeyRespContent *respContent = NULL;
    int numResponses, i;
    long curResponse, expectedResponse;
    char cgiChalVar[10];
#ifdef WRITE_OUT_RESPONSE
    SECItem certRepBinDER = { 0 };

    ATOB_ConvertAsciiToItem(&certRepBinDER, certRepContent);
    writeOutItem("challCertRepContent.der", &certRepBinDER);
    PORT_Free(certRepBinDER.data);
#endif
    clientResponse = CGITableFindValue(varTable, "ChallResponse");
    if (clientResponse == NULL) {
        rv = REQ_CGI_VAR_NOT_PRESENT;
        missingVar = "ChallResponse";
        goto loser;
    }
    srv = ATOB_ConvertAsciiToItem(&binDER, clientResponse);
    if (srv != SECSuccess) {
        rv = ERROR_CONVERTING_RESP_FROM_CHALL_TO_BIN;
        goto loser;
    }
    respContent = CMMF_CreatePOPODecKeyRespContentFromDER(binDER.data,
                                                          binDER.len);
    SECITEM_FreeItem(&binDER, PR_FALSE);
    binDER.data = NULL;
    if (respContent == NULL) {
        rv = ERROR_CREATING_KEY_RESP_FROM_DER;
        goto loser;
    }
    numResponses = CMMF_POPODecKeyRespContentGetNumResponses(respContent);
    for (i = 0; i < numResponses; i++) {
        srv = CMMF_POPODecKeyRespContentGetResponse(respContent, i, &curResponse);
        if (srv != SECSuccess) {
            rv = ERROR_RETRIEVING_CLIENT_RESPONSE_TO_CHALLENGE;
            goto loser;
        }
        sprintf(cgiChalVar, "chal%d", i + 1);
        formChalValue = CGITableFindValue(varTable, cgiChalVar);
        if (formChalValue == NULL) {
            rv = REQ_CGI_VAR_NOT_PRESENT;
            missingVar = strdup(cgiChalVar);
            goto loser;
        }
        sscanf(formChalValue, "%ld", &expectedResponse);
        if (expectedResponse != curResponse) {
            rv = ERROR_RETURNED_CHALL_NOT_VALUE_EXPECTED;
            goto loser;
        }
    }
    nickname = CGITableFindValue(varTable, "nickname");
    if (nickname == NULL) {
        rv = REQ_CGI_VAR_NOT_PRESENT;
        missingVar = "nickname";
        goto loser;
    }
    spitOutCMMFResponse(nickname, certRepContent);
loser:
    if (respContent != NULL) {
        CMMF_DestroyPOPODecKeyRespContent(respContent);
    }
    return rv;
}

int
main()
{
    char *form_output = NULL;
    int form_output_len, form_output_used;
    CGIVarTable varTable = { 0 };
    ErrorCode errNum = 0;
    char *certRepContent;

#ifdef ATTACH_CGI
    /* Put an ifinite loop in here so I can attach to
     * the process after the process is spun off
     */
    {
        int stupid = 1;
        while (stupid)
            ;
    }
#endif

    form_output_used = 0;
    srand(time(NULL));
    while (feof(stdin) == 0) {
        if (form_output == NULL) {
            form_output = PORT_NewArray(char, DEFAULT_ALLOC_SIZE + 1);
            form_output_len = DEFAULT_ALLOC_SIZE;
        } else if ((form_output_used + DEFAULT_ALLOC_SIZE) >= form_output_len) {
            form_output_len += DEFAULT_ALLOC_SIZE;
            form_output = PORT_Realloc(form_output, form_output_len + 1);
        }
        form_output_used += fread(&form_output[form_output_used], sizeof(char),
                                  DEFAULT_ALLOC_SIZE, stdin);
    }
    ParseInputVariables(&varTable, form_output);
    certRepContent = CGITableFindValue(&varTable, "CertRepContent");
    if (certRepContent == NULL) {
        errNum = initNSS(&varTable);
        if (errNum != 0) {
            goto loser;
        }
        errNum = processRequest(&varTable);
    } else {
        errNum = processChallengeResponse(&varTable, certRepContent);
    }
    if (errNum != NO_ERROR) {
        goto loser;
    }
    goto done;
loser:
    dumpErrorMessage(errNum);
done:
    free(form_output);
    return 0;
}
