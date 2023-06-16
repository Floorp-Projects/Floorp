/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signtool.h"

#include "secoid.h"
#include "cryptohi.h"
#include "certdb.h"

static char *GetSubjectFromUser(unsigned long serial);
static CERTCertificate *GenerateSelfSignedObjectSigningCert(char *nickname,
                                                            CERTCertDBHandle *db, char *subject, unsigned long serial, int keysize,
                                                            char *token);
static SECStatus ChangeTrustAttributes(CERTCertDBHandle *db,
                                       CERTCertificate *cert, char *trusts);
static SECStatus set_cert_type(CERTCertificate *cert, unsigned int type);
static SECItem *sign_cert(CERTCertificate *cert, SECKEYPrivateKey *privk);
static CERTCertificate *install_cert(CERTCertDBHandle *db, SECItem *derCert,
                                     char *nickname);
static SECStatus GenerateKeyPair(PK11SlotInfo *slot, SECKEYPublicKey **pubk,
                                 SECKEYPrivateKey **privk, int keysize);
static CERTCertificateRequest *make_cert_request(char *subject,
                                                 SECKEYPublicKey *pubk);
static CERTCertificate *make_cert(CERTCertificateRequest *req,
                                  unsigned long serial, CERTName *ca_subject);
static void output_ca_cert(CERTCertificate *cert, CERTCertDBHandle *db);

/***********************************************************************
 *
 * G e n e r a t e C e r t
 *
 * Runs the whole process of creating a new cert, getting info from the
 * user, etc.
 */
int
GenerateCert(char *nickname, int keysize, char *token)
{
    CERTCertDBHandle *db;
    CERTCertificate *cert;
    char *subject;
    unsigned long serial;
    char stdinbuf[160];

    /* Print warning about having the browser open */
    PR_fprintf(PR_STDOUT /*always go to console*/,
               "\nWARNING: Performing this operation while the browser is running could cause"
               "\ncorruption of your security databases. If the browser is currently running,"
               "\nyou should exit the browser before continuing this operation. Enter "
               "\n\"y\" to continue, or anything else to abort: ");
    pr_fgets(stdinbuf, 160, PR_STDIN);
    PR_fprintf(PR_STDOUT, "\n");
    if (tolower(stdinbuf[0]) != 'y') {
        PR_fprintf(errorFD, "Operation aborted at user's request.\n");
        errorCount++;
        return -1;
    }

    db = CERT_GetDefaultCertDB();
    if (!db) {
        FatalError("Unable to open certificate database");
    }

    if (PK11_FindCertFromNickname(nickname, &pwdata)) {
        PR_fprintf(errorFD,
                   "ERROR: Certificate with nickname \"%s\" already exists in database. You\n"
                   "must choose a different nickname.\n",
                   nickname);
        errorCount++;
        exit(ERRX);
    }

    LL_L2UI(serial, PR_Now());

    subject = GetSubjectFromUser(serial);
    if (!subject) {
        FatalError("Unable to get subject from user");
    }

    cert = GenerateSelfSignedObjectSigningCert(nickname, db, subject,
                                               serial, keysize, token);

    if (cert) {
        output_ca_cert(cert, db);
        CERT_DestroyCertificate(cert);
    }

    PORT_Free(subject);
    return 0;
}

#undef VERBOSE_PROMPTS

/*********************************************************************8
 * G e t S u b j e c t F r o m U s e r
 *
 * Construct the subject information line for a certificate by querying
 * the user on stdin.
 */
static char *
GetSubjectFromUser(unsigned long serial)
{
    char buf[STDIN_BUF_SIZE];
    char common_name_buf[STDIN_BUF_SIZE];
    char *common_name, *state, *orgunit, *country, *org, *locality;
    char *email, *uid;
    char *subject;
    char *cp;
    int subjectlen = 0;

    common_name = state = orgunit = country = org = locality = email =
        uid = subject = NULL;

    /* Get subject information */
    PR_fprintf(PR_STDOUT,
               "\nEnter certificate information.  All fields are optional. Acceptable\n"
               "characters are numbers, letters, spaces, and apostrophes.\n");

#ifdef VERBOSE_PROMPTS
    PR_fprintf(PR_STDOUT, "\nCOMMON NAME\n"
                          "Enter the full name you want to give your certificate. (Example: Test-Only\n"
                          "Object Signing Certificate)\n"
                          "-->");
#else
    PR_fprintf(PR_STDOUT, "certificate common name: ");
#endif
    if (!fgets(buf, STDIN_BUF_SIZE, stdin)) {
        return NULL;
    }
    cp = chop(buf);
    if (*cp == '\0') {
        snprintf(common_name_buf, sizeof(common_name_buf), "%s (%lu)", DEFAULT_COMMON_NAME,
                 serial);
        cp = common_name_buf;
    }
    common_name = PORT_ZAlloc(strlen(cp) + 6);
    if (!common_name) {
        out_of_memory();
    }
    snprintf(common_name, strlen(cp) + 6, "CN=%s, ", cp);
    subjectlen += strlen(common_name);

#ifdef VERBOSE_PROMPTS
    PR_fprintf(PR_STDOUT, "\nORGANIZATION NAME\n"
                          "Enter the name of your organization. For example, this could be the name\n"
                          "of your company.\n"
                          "-->");
#else
    PR_fprintf(PR_STDOUT, "organization: ");
#endif
    if (!fgets(buf, STDIN_BUF_SIZE, stdin)) {
        return NULL;
    }
    cp = chop(buf);
    if (*cp != '\0') {
        org = PORT_ZAlloc(strlen(cp) + 5);
        if (!org) {
            out_of_memory();
        }
        snprintf(org, strlen(cp) + 5, "O=%s, ", cp);
        subjectlen += strlen(org);
    }

#ifdef VERBOSE_PROMPTS
    PR_fprintf(PR_STDOUT, "\nORGANIZATION UNIT\n"
                          "Enter the name of your organization unit.  For example, this could be the\n"
                          "name of your department.\n"
                          "-->");
#else
    PR_fprintf(PR_STDOUT, "organization unit: ");
#endif
    if (!fgets(buf, STDIN_BUF_SIZE, stdin)) {
        return NULL;
    }
    cp = chop(buf);
    if (*cp != '\0') {
        orgunit = PORT_ZAlloc(strlen(cp) + 6);
        if (!orgunit) {
            out_of_memory();
        }
        snprintf(orgunit, strlen(cp) + 6, "OU=%s, ", cp);
        subjectlen += strlen(orgunit);
    }

#ifdef VERBOSE_PROMPTS
    PR_fprintf(PR_STDOUT, "\nSTATE\n"
                          "Enter the name of your state or province.\n"
                          "-->");
#else
    PR_fprintf(PR_STDOUT, "state or province: ");
#endif
    if (!fgets(buf, STDIN_BUF_SIZE, stdin)) {
        return NULL;
    }
    cp = chop(buf);
    if (*cp != '\0') {
        state = PORT_ZAlloc(strlen(cp) + 6);
        if (!state) {
            out_of_memory();
        }
        snprintf(state, strlen(cp) + 6, "ST=%s, ", cp);
        subjectlen += strlen(state);
    }

#ifdef VERBOSE_PROMPTS
    PR_fprintf(PR_STDOUT, "\nCOUNTRY\n"
                          "Enter the 2-character abbreviation for the name of your country.\n"
                          "-->");
#else
    PR_fprintf(PR_STDOUT, "country (must be exactly 2 characters): ");
#endif
    if (!fgets(buf, STDIN_BUF_SIZE, stdin)) {
        return NULL;
    }
    cp = chop(cp);
    if (strlen(cp) != 2) {
        *cp = '\0'; /* country code must be 2 chars */
    }
    if (*cp != '\0') {
        country = PORT_ZAlloc(strlen(cp) + 5);
        if (!country) {
            out_of_memory();
        }
        snprintf(country, strlen(cp) + 5, "C=%s, ", cp);
        subjectlen += strlen(country);
    }

#ifdef VERBOSE_PROMPTS
    PR_fprintf(PR_STDOUT, "\nUSERNAME\n"
                          "Enter your system username or UID\n"
                          "-->");
#else
    PR_fprintf(PR_STDOUT, "username: ");
#endif
    if (!fgets(buf, STDIN_BUF_SIZE, stdin)) {
        return NULL;
    }
    cp = chop(buf);
    if (*cp != '\0') {
        uid = PORT_ZAlloc(strlen(cp) + 7);
        if (!uid) {
            out_of_memory();
        }
        snprintf(uid, strlen(cp) + 7, "UID=%s, ", cp);
        subjectlen += strlen(uid);
    }

#ifdef VERBOSE_PROMPTS
    PR_fprintf(PR_STDOUT, "\nEMAIL ADDRESS\n"
                          "Enter your email address.\n"
                          "-->");
#else
    PR_fprintf(PR_STDOUT, "email address: ");
#endif
    if (!fgets(buf, STDIN_BUF_SIZE, stdin)) {
        return NULL;
    }
    cp = chop(buf);
    if (*cp != '\0') {
        email = PORT_ZAlloc(strlen(cp) + 5);
        if (!email) {
            out_of_memory();
        }
        snprintf(email, strlen(cp) + 5, "E=%s,", cp);
        subjectlen += strlen(email);
    }

    subjectlen++;

    subject = PORT_ZAlloc(subjectlen);
    if (!subject) {
        out_of_memory();
    }

    snprintf(subject, subjectlen, "%s%s%s%s%s%s%s",
             common_name ? common_name : "",
             org ? org : "",
             orgunit ? orgunit : "",
             state ? state : "",
             country ? country : "",
             uid ? uid : "",
             email ? email : "");
    if ((strlen(subject) > 1) && (subject[strlen(subject) - 1] == ' ')) {
        subject[strlen(subject) - 2] = '\0';
    }

    PORT_Free(common_name);
    PORT_Free(org);
    PORT_Free(orgunit);
    PORT_Free(state);
    PORT_Free(country);
    PORT_Free(uid);
    PORT_Free(email);

    return subject;
}

/**************************************************************************
 *
 * G e n e r a t e S e l f S i g n e d O b j e c t S i g n i n g C e r t
 *														   		  *phew*^
 *
 */
static CERTCertificate *
GenerateSelfSignedObjectSigningCert(char *nickname, CERTCertDBHandle *db,
                                    char *subject, unsigned long serial, int keysize, char *token)
{
    CERTCertificate *cert, *temp_cert;
    SECItem *derCert;
    CERTCertificateRequest *req;

    PK11SlotInfo *slot = NULL;
    SECKEYPrivateKey *privk = NULL;
    SECKEYPublicKey *pubk = NULL;

    if (token) {
        slot = PK11_FindSlotByName(token);
    } else {
        slot = PK11_GetInternalKeySlot();
    }

    if (slot == NULL) {
        PR_fprintf(errorFD, "Can't find PKCS11 slot %s\n",
                   token ? token : "");
        errorCount++;
        exit(ERRX);
    }

    if (GenerateKeyPair(slot, &pubk, &privk, keysize) != SECSuccess) {
        FatalError("Error generating keypair.");
    }
    req = make_cert_request(subject, pubk);
    temp_cert = make_cert(req, serial, &req->subject);
    if (set_cert_type(temp_cert,
                      NS_CERT_TYPE_OBJECT_SIGNING |
                          NS_CERT_TYPE_OBJECT_SIGNING_CA) !=
        SECSuccess) {
        FatalError("Unable to set cert type");
    }

    derCert = sign_cert(temp_cert, privk);
    cert = install_cert(db, derCert, nickname);
    if (ChangeTrustAttributes(db, cert, ",,uC") != SECSuccess) {
        FatalError("Unable to change trust on generated certificate");
    }

    /* !!! Free memory ? !!! */
    PK11_FreeSlot(slot);
    SECKEY_DestroyPrivateKey(privk);
    SECKEY_DestroyPublicKey(pubk);
    CERT_DestroyCertificate(temp_cert);
    CERT_DestroyCertificateRequest(req);

    return cert;
}

/**************************************************************************
 *
 * C h a n g e T r u s t A t t r i b u t e s
 */
static SECStatus
ChangeTrustAttributes(CERTCertDBHandle *db, CERTCertificate *cert, char *trusts)
{

    CERTCertTrust *trust;

    if (!db || !cert || !trusts) {
        PR_fprintf(errorFD, "ChangeTrustAttributes got incomplete arguments.\n");
        errorCount++;
        return SECFailure;
    }

    trust = (CERTCertTrust *)PORT_ZAlloc(sizeof(CERTCertTrust));
    if (!trust) {
        PR_fprintf(errorFD, "ChangeTrustAttributes unable to allocate "
                            "CERTCertTrust\n");
        errorCount++;
        return SECFailure;
    }

    if (CERT_DecodeTrustString(trust, trusts)) {
        return SECFailure;
    }

    if (CERT_ChangeCertTrust(db, cert, trust)) {
        PR_fprintf(errorFD, "unable to modify trust attributes for cert %s\n",
                   cert->nickname ? cert->nickname : "");
        errorCount++;
        return SECFailure;
    }

    PORT_Free(trust);
    return SECSuccess;
}

/*************************************************************************
 *
 * s e t _ c e r t _ t y p e
 */
static SECStatus
set_cert_type(CERTCertificate *cert, unsigned int type)
{
    void *context;
    SECStatus status = SECSuccess;
    SECItem certType;
    char ctype;

    context = CERT_StartCertExtensions(cert);

    certType.type = siBuffer;
    certType.data = (unsigned char *)&ctype;
    certType.len = 1;
    ctype = (unsigned char)type;
    if (CERT_EncodeAndAddBitStrExtension(context, SEC_OID_NS_CERT_EXT_CERT_TYPE,
                                         &certType, PR_TRUE /*critical*/) !=
        SECSuccess) {
        status = SECFailure;
    }

    if (CERT_FinishExtensions(context) != SECSuccess) {
        status = SECFailure;
    }

    return status;
}

/********************************************************************
 *
 * s i g n _ c e r t
 */
static SECItem *
sign_cert(CERTCertificate *cert, SECKEYPrivateKey *privk)
{
    SECStatus rv;

    SECItem der2;
    SECItem *result2;

    SECOidTag alg = SEC_OID_UNKNOWN;

    alg = SEC_GetSignatureAlgorithmOidTag(privk->keyType, SEC_OID_UNKNOWN);
    if (alg == SEC_OID_UNKNOWN) {
        FatalError("Unknown key type");
    }

    rv = SECOID_SetAlgorithmID(cert->arena, &cert->signature, alg, 0);

    if (rv != SECSuccess) {
        PR_fprintf(errorFD, "%s: unable to set signature alg id\n",
                   PROGRAM_NAME);
        errorCount++;
        exit(ERRX);
    }

    der2.len = 0;
    der2.data = NULL;

    (void)SEC_ASN1EncodeItem(cert->arena, &der2, cert, SEC_ASN1_GET(CERT_CertificateTemplate));

    if (rv != SECSuccess) {
        PR_fprintf(errorFD, "%s: error encoding cert\n", PROGRAM_NAME);
        errorCount++;
        exit(ERRX);
    }

    result2 = (SECItem *)PORT_ArenaZAlloc(cert->arena, sizeof(SECItem));
    if (result2 == NULL)
        out_of_memory();

    rv = SEC_DerSignData(cert->arena, result2, der2.data, der2.len, privk, alg);

    if (rv != SECSuccess) {
        PR_fprintf(errorFD, "can't sign encoded certificate data\n");
        errorCount++;
        exit(ERRX);
    } else if (verbosity >= 0) {
        PR_fprintf(outputFD, "certificate has been signed\n");
    }

    cert->derCert = *result2;

    return result2;
}

/*********************************************************************
 *
 * i n s t a l l _ c e r t
 *
 * Installs the cert in the permanent database.
 */
static CERTCertificate *
install_cert(CERTCertDBHandle *db, SECItem *derCert, char *nickname)
{
    CERTCertificate *newcert;
    PK11SlotInfo *newSlot;

    newSlot = PK11_ImportDERCertForKey(derCert, nickname, &pwdata);
    if (newSlot == NULL) {
        PR_fprintf(errorFD, "Unable to install certificate\n");
        errorCount++;
        exit(ERRX);
    }

    newcert = PK11_FindCertFromDERCertItem(newSlot, derCert, &pwdata);
    PK11_FreeSlot(newSlot);
    if (newcert == NULL) {
        PR_fprintf(errorFD, "%s: can't find new certificate\n",
                   PROGRAM_NAME);
        errorCount++;
        exit(ERRX);
    }

    if (verbosity >= 0) {
        PR_fprintf(outputFD, "certificate \"%s\" added to database\n",
                   nickname);
    }

    return newcert;
}

/******************************************************************
 *
 * G e n e r a t e K e y P a i r
 */
static SECStatus
GenerateKeyPair(PK11SlotInfo *slot, SECKEYPublicKey **pubk,
                SECKEYPrivateKey **privk, int keysize)
{

    PK11RSAGenParams rsaParams;

    if (keysize == -1) {
        rsaParams.keySizeInBits = DEFAULT_RSA_KEY_SIZE;
    } else {
        rsaParams.keySizeInBits = keysize;
    }
    rsaParams.pe = 0x10001;

    if (PK11_Authenticate(slot, PR_FALSE /*loadCerts*/, &pwdata) !=
        SECSuccess) {
        SECU_PrintError(progName, "failure authenticating to key database.\n");
        exit(ERRX);
    }

    *privk = PK11_GenerateKeyPair(slot, CKM_RSA_PKCS_KEY_PAIR_GEN, &rsaParams,

                                  pubk, PR_TRUE /*isPerm*/, PR_TRUE /*isSensitive*/, &pwdata);

    if (*privk != NULL && *pubk != NULL) {
        if (verbosity >= 0) {
            PR_fprintf(outputFD, "generated public/private key pair\n");
        }
    } else {
        SECU_PrintError(progName, "failure generating key pair\n");
        exit(ERRX);
    }

    return SECSuccess;
}

/******************************************************************
 *
 * m a k e _ c e r t _ r e q u e s t
 */
static CERTCertificateRequest *
make_cert_request(char *subject, SECKEYPublicKey *pubk)
{
    CERTName *subj;
    CERTSubjectPublicKeyInfo *spki;

    CERTCertificateRequest *req;

    /* Create info about public key */
    spki = SECKEY_CreateSubjectPublicKeyInfo(pubk);
    if (!spki) {
        SECU_PrintError(progName, "unable to create subject public key");
        exit(ERRX);
    }

    subj = CERT_AsciiToName(subject);
    if (subj == NULL) {
        FatalError("Invalid data in certificate description");
    }

    /* Generate certificate request */
    req = CERT_CreateCertificateRequest(subj, spki, 0);
    if (!req) {
        SECU_PrintError(progName, "unable to make certificate request");
        exit(ERRX);
    }

    SECKEY_DestroySubjectPublicKeyInfo(spki);
    CERT_DestroyName(subj);

    if (verbosity >= 0) {
        PR_fprintf(outputFD, "certificate request generated\n");
    }

    return req;
}

/******************************************************************
 *
 * m a k e _ c e r t
 */
static CERTCertificate *
make_cert(CERTCertificateRequest *req, unsigned long serial,
          CERTName *ca_subject)
{
    CERTCertificate *cert;

    CERTValidity *validity = NULL;

    PRTime now, after;
    PRExplodedTime printableTime;

    now = PR_Now();
    PR_ExplodeTime(now, PR_GMTParameters, &printableTime);

    printableTime.tm_month += 3;
    after = PR_ImplodeTime(&printableTime);

    validity = CERT_CreateValidity(now, after);

    if (validity == NULL) {
        PR_fprintf(errorFD, "%s: error creating certificate validity\n",
                   PROGRAM_NAME);
        errorCount++;
        exit(ERRX);
    }

    cert = CERT_CreateCertificate(serial, ca_subject, validity, req);
    CERT_DestroyValidity(validity);

    if (cert == NULL) {
        /* should probably be more precise here */
        PR_fprintf(errorFD, "%s: error while generating certificate\n",
                   PROGRAM_NAME);
        errorCount++;
        exit(ERRX);
    }

    return cert;
}

/*************************************************************************
 *
 * o u t p u t _ c a _ c e r t
 */
static void
output_ca_cert(CERTCertificate *cert, CERTCertDBHandle *db)
{
    FILE *out;

    SECItem *encodedCertChain;
    SEC_PKCS7ContentInfo *certChain;
    char *filename, *certData;

    /* the raw */

    filename = PORT_ZAlloc(strlen(DEFAULT_X509_BASENAME) + 8);
    if (!filename)
        out_of_memory();

    snprintf(filename, strlen(DEFAULT_X509_BASENAME) + 8, "%s.raw", DEFAULT_X509_BASENAME);
    if ((out = fopen(filename, "wb")) == NULL) {
        PR_fprintf(errorFD, "%s: Can't open %s output file\n", PROGRAM_NAME,
                   filename);
        errorCount++;
        exit(ERRX);
    }

    certChain = SEC_PKCS7CreateCertsOnly(cert, PR_TRUE, db);
    encodedCertChain =
        SEC_PKCS7EncodeItem(NULL, NULL, certChain, NULL, NULL, NULL);
    SEC_PKCS7DestroyContentInfo(certChain);

    if (encodedCertChain) {
        fprintf(out, "Content-type: application/x-x509-ca-cert\n\n");
        fwrite(encodedCertChain->data, 1, encodedCertChain->len,
               out);
        SECITEM_FreeItem(encodedCertChain, PR_TRUE);
    } else {
        PR_fprintf(errorFD, "%s: Can't DER encode this certificate\n",
                   PROGRAM_NAME);
        errorCount++;
        exit(ERRX);
    }

    fclose(out);

    /* and the cooked */

    snprintf(filename, strlen(DEFAULT_X509_BASENAME) + 8, "%s.cacert", DEFAULT_X509_BASENAME);
    if ((out = fopen(filename, "wb")) == NULL) {
        PR_fprintf(errorFD, "%s: Can't open %s output file\n", PROGRAM_NAME,
                   filename);
        errorCount++;
        return;
    }

    certData = BTOA_DataToAscii(cert->derCert.data, cert->derCert.len);
    fprintf(out, "%s\n%s\n%s\n", NS_CERT_HEADER, certData, NS_CERT_TRAILER);
    PORT_Free(certData);

    PORT_Free(filename);
    fclose(out);

    if (verbosity >= 0) {
        PR_fprintf(outputFD, "Exported certificate to %s.raw and %s.cacert.\n",
                   DEFAULT_X509_BASENAME, DEFAULT_X509_BASENAME);
    }
}
