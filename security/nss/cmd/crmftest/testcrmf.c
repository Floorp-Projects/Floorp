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

#include "crmf.h"
#include "secrng.h"
#include "secpkcs5.h"
#include "pk11func.h"
#include "pkcs11.h"
#include "secmod.h"
#include "secmodi.h"
#include "key.h"
#include "prio.h"
#include "pqggen.h"
#include "cmmf.h"
#include "seccomon.h"
#include "secmod.h"
#include "prlock.h"
#include "secmodi.h"
#include "pkcs11.h"
#include "pk11func.h"
#include "secitem.h"
#include "key.h"
#include "rsa.h"
#include "secpkcs5.h"
#include "secasn1.h"
#include "sechash.h"
#include "cert.h"
#include "secerr.h"
#include <stdio.h>
#include "prprf.h"
#if !defined(XP_UNIX) && !defined(LINUX)
extern int getopt(int, char **, char*);
extern char *optarg;
#endif
#define MAX_KEY_LEN 512

int64 notBefore;
char *personalCert      = NULL;
char *recoveryEncrypter = NULL;
char *caCertName        = NULL;

CERTCertDBHandle *db;
SECKEYKeyDBHandle *keydb;

void 
debug_test(SECItem *src, char *filePath)
{
    PRFileDesc *fileDesc;

    fileDesc = PR_Open (filePath, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 
			0666);
    if (fileDesc == NULL) {
        printf ("Could not cretae file %s.\n", filePath);
	return;
    }
    PR_Write(fileDesc, src->data, src->len);
    
}

SECStatus
get_serial_number(long *dest)
{
   RNGContext *rng;
   SECStatus   rv;

   if (dest == NULL) {
       return SECFailure;
   }
   rng = RNG_CreateContext();
   if (rng == NULL) {
       *dest = 0;
       return SECFailure;
   }
   rv = RNG_GenerateRandomBytes(rng, (void*)dest, sizeof(long));
   RNG_DestroyContext(rng, PR_TRUE);
    return SECSuccess;
}

char *
promptForPassword (PK11SlotInfo *slot, PRBool retry, void *cx)
{
    char passWord[80];
    char *retPass = NULL;
    
    if (retry) {
        printf ("Incorrect password.  Please re-enter the password.\n");
    } 
    printf ("WARNING: Password will be echoed to the screen.\n");
    printf ("Please enter the password for slot \"%s\":", 
	    PK11_GetTokenName(slot));
    scanf ("%s", passWord);
    retPass = PORT_Strdup(passWord);
    return retPass;
}

PK11RSAGenParams *
GetRSAParams(void) 
{
    PK11RSAGenParams *rsaParams;

    rsaParams = PORT_ZNew(PK11RSAGenParams);

    if (rsaParams == NULL)
      return NULL;

    rsaParams->keySizeInBits = MAX_KEY_LEN;
    rsaParams->pe = 0x1001;
    
    return rsaParams;
    
}

SECStatus
SetSlotPassword(PK11SlotInfo *slot)
{
    char userPin[80];

    printf ("Initialization of PIN's for your Database.\n");
    printf ("------------------------------------------\n");
    printf ("Please enter the PIN's for your Database.\n");
    printf ("Warning: ALL PIN'S WILL BE ECHOED TO SCREEN!!!\n");
    printf ("Now enter the PIN for the user: ");
    scanf  ("%s", userPin);
    return PK11_InitPin (slot, NULL, userPin);
}

PQGParams*
GetDSAParams(void)
{
    PQGParams *params = NULL;
    PQGVerify *vfy = NULL;

    SECStatus  rv;

    rv = PQG_ParamGen(0, &params, &vfy);
    if (rv != SECSuccess) {
        return NULL;
    }
    PQG_DestroyVerify(vfy);
    return params;
}

CERTSubjectPublicKeyInfo *
GetSubjectPubKeyInfo(SECKEYPrivateKey **destPrivKey, 
		     SECKEYPublicKey  **destPubKey) {
    CERTSubjectPublicKeyInfo *spki       = NULL;
    SECKEYPrivateKey         *privKey    = NULL;
    SECKEYPublicKey          *pubKey     = NULL;
    PK11SlotInfo             *keySlot    = NULL;
    PK11SlotInfo             *cryptoSlot = NULL;
    PK11RSAGenParams         *rsaParams  = NULL;
    PQGParams                *dsaParams  = NULL;
    
    keySlot = PK11_GetInternalKeySlot();
    PK11_Authenticate(keySlot, PR_FALSE, NULL);
    cryptoSlot = PK11_GetInternalSlot();
    PK11_Authenticate(cryptoSlot, PR_FALSE, NULL);
    PK11_FreeSlot(cryptoSlot);
    rsaParams  = GetRSAParams();
    privKey = PK11_GenerateKeyPair(keySlot, CKM_RSA_PKCS_KEY_PAIR_GEN,
				   (void*)rsaParams, &pubKey, PR_FALSE,
				   PR_FALSE, NULL);
/*    dsaParams = GetDSAParams();
    if (dsaParams == NULL) {
        PK11_FreeSlot(keySlot);
        return NULL;
    }
    privKey = PK11_GenerateKeyPair(keySlot, CKM_DSA_KEY_PAIR_GEN,
				   (void*)dsaParams, &pubKey, PR_FALSE,
				   PR_FALSE, NULL);*/
    PK11_FreeSlot(keySlot);
    if (privKey == NULL || pubKey == NULL) {
        if (pubKey) {
	    SECKEY_DestroyPublicKey(pubKey);
	}
	if (privKey) {
	    SECKEY_DestroyPrivateKey(privKey);
	}
	return NULL;
    }

    spki = SECKEY_CreateSubjectPublicKeyInfo(pubKey);
    *destPrivKey = privKey;
    *destPubKey  = pubKey;
    return spki;
}


SECStatus
InitPKCS11(void)
{
    PK11SlotInfo *cryptoSlot, *keySlot;

    PK11_SetPasswordFunc(promptForPassword);

    cryptoSlot = PK11_GetInternalSlot();
    keySlot    = PK11_GetInternalKeySlot();
    
    if (PK11_NeedUserInit(cryptoSlot) && PK11_NeedLogin(cryptoSlot)) {
        if (SetSlotPassword (cryptoSlot) != SECSuccess) {
	    printf ("Initializing the PIN's failed.\n");
	    return SECFailure;
	}
    }
    
    if (PK11_NeedUserInit(keySlot) && PK11_NeedLogin(keySlot)) {
        if (SetSlotPassword (keySlot) != SECSuccess) {
	    printf ("Initializing the PIN's failed.\n");
	    return SECFailure;
	}
    }

    PK11_FreeSlot(cryptoSlot);
    PK11_FreeSlot(keySlot);
    return SECSuccess;
}


void 
WriteItOut (void *arg, const char *buf, unsigned long len)
{
    PRFileDesc *fileDesc = (PRFileDesc*)arg;

    PR_Write(fileDesc, (void*)buf, len);
}

SECItem
GetRandomBitString(void)
{
#define NUM_BITS     800
#define BITS_IN_BYTE 8
    SECItem bitString;
    int numBytes = NUM_BITS/BITS_IN_BYTE;
    unsigned char *bits = PORT_ZNewArray(unsigned char, numBytes);
    RNGContext *rng;

    rng = RNG_CreateContext();
    RNG_GenerateRandomBytes(rng, (void*)bits, numBytes);
    RNG_DestroyContext(rng, PR_TRUE);
    bitString.data = bits;
    bitString.len = NUM_BITS;
    bitString.type = siBuffer;
    return bitString;
}

CRMFCertExtCreationInfo*
GetExtensions(void)
{
    CRMFCertExtCreationInfo *extInfo;
    CRMFCertExtension *currExt;
    CRMFCertExtension *extension;
    SECItem data;
    PRBool prFalse = PR_FALSE;
    unsigned char keyUsage[4];

    data.len = 4;
    data.data = keyUsage;
    keyUsage[0] = 0x03;
    keyUsage[1] = 0x02;
    keyUsage[2] = 0x07;
    keyUsage[3] = KU_DIGITAL_SIGNATURE;
    extension = CRMF_CreateCertExtension(SEC_OID_X509_KEY_USAGE,prFalse,
					 &data);
    extInfo = PORT_ZNew(CRMFCertExtCreationInfo);
    extInfo->numExtensions = 1;
    extInfo->extensions = PORT_ZNewArray(CRMFCertExtension*, 1);
    extInfo->extensions[0] = extension;
    return extInfo;
}

void
FreeExtInfo(CRMFCertExtCreationInfo *extInfo)
{
    int i;
    
    for (i=0; i<extInfo->numExtensions; i++) {
        CRMF_DestroyCertExtension(extInfo->extensions[i]);
    }
    PORT_Free(extInfo->extensions);
    PORT_Free(extInfo);
}

int
CreateCertRequest (CRMFCertRequest **inCertReq, SECKEYPrivateKey **privKey,
		   SECKEYPublicKey **pubKey)
{
    long serialNumber;
    long version = 3;
    char *issuerStr = PORT_Strdup ("CN=Javi's CA Shack, O=Information Systems");
    char *subjectStr = PORT_Strdup ("CN=Javi's CA Shack ID, O=Engineering, "
				    "C=US");
    CRMFCertRequest *certReq;
    SECAlgorithmID * algID;
    CERTName *issuer, *subject;
    CRMFValidityCreationInfo validity;
    CERTSubjectPublicKeyInfo *spki;
    SECStatus rv;
    SECOidTag tag, tag2;
    SECItem issuerUID, subjectUID;
    CRMFCertExtCreationInfo *extInfo;
    CRMFEncryptedKey  *encKey;
    CERTCertificate *caCert;
    CRMFPKIArchiveOptions *pkiArchOpt;
  
    *inCertReq = NULL;
    certReq = CRMF_CreateCertRequest(0x0ff02345);
    if (certReq == NULL) {
        printf ("Could not initialize a certificate request.\n");
	return 1;
    }
    rv = CRMF_CertRequestSetTemplateField (certReq, crmfVersion, (void*)(&version));
    if (rv != SECSuccess) {
        printf("Could not add the version number to the "
	       "Certificate Request.\n");
	CRMF_DestroyCertRequest(certReq);
	return 2;
    }

    if (get_serial_number(&serialNumber) != SECSuccess) {
        printf ("Could not generate a serial number for cert request.\n");
	CRMF_DestroyCertRequest(certReq);
	return 3;
    }

    rv = CRMF_CertRequestSetTemplateField (certReq, crmfSerialNumber, 
				      (void*)(&serialNumber));
    if (rv != SECSuccess) {
        printf ("Could not add serial number to certificate template\n.");
	CRMF_DestroyCertRequest(certReq);
	return 4;
    }
    
    issuer = CERT_AsciiToName(issuerStr);
    if (issuer == NULL) {
        printf ("Could not create CERTName structure from %s.\n", issuerStr);
	CRMF_DestroyCertRequest(certReq);
	return 5;
    }
    rv = CRMF_CertRequestSetTemplateField (certReq, crmfIssuer, (void*) issuer);
    PORT_Free(issuerStr);
    CERT_DestroyName(issuer);
    if (rv != SECSuccess) {
        printf ("Could not add issuer to cert template\n");
	CRMF_DestroyCertRequest(certReq);
	return 6;
    }

    subject = CERT_AsciiToName(subjectStr);
    if (subject == NULL) {
        printf ("Could not create CERTName structure from %s.\n", subjectStr);
	CRMF_DestroyCertRequest(certReq);
	return 7;
    }
    PORT_Free(subjectStr);
    rv = CRMF_CertRequestSetTemplateField (certReq, crmfSubject, (void*)subject);
    if (rv != SECSuccess) {
        printf ("Could not add subject to cert template\n");
	CRMF_DestroyCertRequest(certReq);
	return 8;
    }
    CERT_DestroyName(subject);

    algID = 
   SEC_PKCS5CreateAlgorithmID (SEC_OID_PKCS5_PBE_WITH_SHA1_AND_DES_CBC,
			       NULL, 1);
    if (algID == NULL) {
        printf ("Couldn't create algorithm ID\n");
	CRMF_DestroyCertRequest(certReq);
	return 9;
    }
    rv = CRMF_CertRequestSetTemplateField(certReq, crmfSigningAlg, (void*)algID);
    SECOID_DestroyAlgorithmID(algID, PR_TRUE);
    if (rv != SECSuccess) {
        printf ("Could not add the signing algorithm to the cert template.\n");
	CRMF_DestroyCertRequest(certReq);
	return 10;
    }

    validity.notBefore = &notBefore;
    validity.notAfter  = NULL;
    notBefore = PR_Now();
    rv = CRMF_CertRequestSetTemplateField(certReq, crmfValidity,(void*)(&validity));
    if (rv != SECSuccess) {
        printf ("Could not add validity to cert template\n");
	CRMF_DestroyCertRequest(certReq);
	return 11;
    }

    spki = GetSubjectPubKeyInfo(privKey, pubKey);
    if (spki == NULL) {
        printf ("Could not create a Subject Public Key Info to add\n");
	CRMF_DestroyCertRequest(certReq);
	return 12;
    }
    rv = CRMF_CertRequestSetTemplateField(certReq, crmfPublicKey, (void*)spki);
    SECKEY_DestroySubjectPublicKeyInfo(spki);
    if (rv != SECSuccess) {
        printf ("Could not add the public key to the template\n");
	CRMF_DestroyCertRequest(certReq);
	return 13;
    }
    
    caCert = 
      CERT_FindCertByNickname(CERT_GetDefaultCertDB(), 
			      caCertName);
    if (caCert == NULL) {
        printf ("Could not find the certificate for %s\n", caCertName);
        CRMF_DestroyCertRequest(certReq);
	return 50;
    }

    issuerUID = GetRandomBitString();
    subjectUID = GetRandomBitString();
    CRMF_CertRequestSetTemplateField(certReq,crmfIssuerUID,  (void*)&issuerUID);
    CRMF_CertRequestSetTemplateField(certReq,crmfSubjectUID, (void*)&subjectUID);
    PORT_Free(issuerUID.data);
    PORT_Free(subjectUID.data);
    extInfo = GetExtensions();
    CRMF_CertRequestSetTemplateField(certReq, crmfExtension, (void*)extInfo);
    FreeExtInfo(extInfo);
    encKey = CRMF_CreateEncryptedKeyWithEncryptedValue(*privKey, caCert);
    CERT_DestroyCertificate(caCert);
    if (encKey == NULL) {
        printf ("Could not create Encrypted Key with Encrypted Value.\n");
	return 14;
    }
    pkiArchOpt = CRMF_CreatePKIArchiveOptions(crmfEncryptedPrivateKey, encKey);
    CRMF_DestroyEncryptedKey(encKey);
    if (pkiArchOpt == NULL) {
        printf ("Could not create PKIArchiveOptions.\n");
	return 15;
    }
    rv  = CRMF_CertRequestSetPKIArchiveOptions(certReq, pkiArchOpt);
    CRMF_DestroyPKIArchiveOptions(pkiArchOpt);
    if (rv != SECSuccess) {
        printf ("Could not add the PKIArchiveControl to Cert Request.\n");
	return 16;
    }
    *inCertReq = certReq;
    return 0;
}

int 
Encode (CRMFCertReqMsg *inCertReq, 
	CRMFCertReqMsg *secondReq, char *configdir)
{   
#define PATH_LEN  150
#define CRMF_FILE "CertReqMessages.der"
    char filePath[PATH_LEN];
    PRFileDesc *fileDesc;
    SECStatus rv;
    int irv = 0;
    CRMFCertReqMsg *msgArr[3];
    CRMFCertReqMsg *newMsg;

    PR_snprintf(filePath, PATH_LEN, "%s/%s", configdir, CRMF_FILE);
    fileDesc = PR_Open (filePath, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 
			0666);
    if (fileDesc == NULL) {
        printf ("Could not open file %s\n", filePath);
	irv = 14;
	goto finish;
    }
/*    rv = CRMF_EncodeCertReqMsg (inCertReq, WriteItOut, (void*)fileDesc);*/
    msgArr[0] = inCertReq;
    msgArr[1] = secondReq;
    msgArr[2] = NULL;
    rv = CRMF_EncodeCertReqMessages(msgArr, WriteItOut, (void*)fileDesc);
    if (rv != SECSuccess) {
        printf ("An error occurred while encoding.\n");
	irv = 15;
	goto finish;
    }
 finish:
    PR_Close(fileDesc);
    return irv;
}

int
AddProofOfPossession(CRMFCertReqMsg *certReqMsg, SECKEYPrivateKey *privKey,
		     SECKEYPublicKey *pubKey, CRMFPOPChoice inPOPChoice)
{

    switch(inPOPChoice){
    case crmfSignature:
      CRMF_CertReqMsgSetSignaturePOP(certReqMsg, privKey, pubKey, NULL, NULL, 
				     NULL);
      break;
    case crmfRAVerified:
      CRMF_CertReqMsgSetRAVerifiedPOP(certReqMsg);
      break;
    case crmfKeyEncipherment:
      CRMF_CertReqMsgSetKeyEnciphermentPOP(certReqMsg,
					   crmfSubsequentMessage, 
					   crmfChallengeResp, NULL);
      break;
    case crmfKeyAgreement:
      {
	SECItem pendejo;
	unsigned char lame[] = { 0xf0, 0x0f, 0xf0, 0x0f, 0xf0 };

	pendejo.data = lame;
	pendejo.len  = 5;
	
	CRMF_CertReqMsgSetKeyAgreementPOP(certReqMsg, crmfThisMessage, 
					  crmfNoSubseqMess, &pendejo);
      }
      break;
    default:
      return 1;
    }
    return 0;
}

#define BUFF_SIZE  150

int
Decode(char *configdir)
{
    char           filePath[PATH_LEN];
    unsigned char  buffer[BUFF_SIZE];
    char          *asn1Buff;
    PRFileDesc    *fileDesc;
    PRInt32          fileLen = 0;
    PRInt32          bytesRead;
    CRMFCertReqMsg  *certReqMsg;
    CRMFCertRequest *certReq;
    CRMFGetValidity validity= {NULL, NULL};
    CRMFCertReqMessages *certReqMsgs;
    int numMsgs, i;
    long lame;

    PR_snprintf(filePath, PATH_LEN, "%s/%s", configdir, CRMF_FILE);
    fileDesc = PR_Open(filePath, PR_RDONLY, 0644);
    while (1) {
        bytesRead = PR_Read(fileDesc, buffer, BUFF_SIZE);
	if (bytesRead <= 0) break;
	fileLen += bytesRead;
    }
    if (bytesRead < 0) {
        printf ("Error while getting the length of the file %s\n", filePath);
	return 200;
    }
    
    PR_Close(fileDesc);
    fileDesc = PR_Open(filePath, PR_RDONLY, 0644);
    asn1Buff = PORT_ZNewArray(char, fileLen);
    bytesRead = PR_Read(fileDesc, asn1Buff, fileLen);
    if (bytesRead != fileLen) {
        printf ("Error while reading in the contents of %s\n", filePath);
	return 201;
    }
    /*certReqMsg = CRMF_CreateCertReqMsgFromDER(asn1Buff, fileLen);
    if (certReqMsg == NULL) {
        printf ("Error while decoding the CertReqMsg\n");
	return 202;
    }
    certReq = CRMF_CertReqMsgGetCertRequest(certReqMsg);
*/
    certReqMsgs = CRMF_CreateCertReqMessagesFromDER(asn1Buff, fileLen);
    if (certReqMsgs == NULL) {
        printf ("Error decoding CertReqMessages.\n");
	return 202;
    }
    numMsgs = CRMF_CertReqMessagesGetNumMessages(certReqMsgs);
    if (numMsgs <= 0) {
        printf ("WARNING: The DER contained %d messages.\n", numMsgs);
    }
    for (i=0; i < numMsgs; i++) {
        certReqMsg = CRMF_CertReqMessagesGetCertReqMsgAtIndex(certReqMsgs, i);
	if (certReqMsg == NULL) {
	    printf ("ERROR: Could not access the message at index %d of %s\n",
		    i, filePath);
	}
	CRMF_CertReqMsgGetID(certReqMsg, &lame);
	certReq = CRMF_CertReqMsgGetCertRequest(certReqMsg);
	CRMF_CertRequestGetCertTemplateValidity(certReq, &validity);
	CRMF_DestroyGetValidity(&validity);
	CRMF_DestroyCertRequest(certReq);
	CRMF_DestroyCertReqMsg(certReqMsg);
    }
    CRMF_DestroyCertReqMessages(certReqMsgs);
    PORT_Free(asn1Buff);
    return 0;
}

void
GetBitsFromFile(char *filePath, SECItem *fileBits)
{
    PRFileDesc *fileDesc;
    int         bytesRead, fileLen=0;
    char        buffer[BUFF_SIZE], *asn1Buf;

    fileDesc = PR_Open(filePath, PR_RDONLY, 0644);
    while (1) {
        bytesRead = PR_Read(fileDesc, buffer, BUFF_SIZE);
	if (bytesRead <= 0) break;
	fileLen += bytesRead;
    }
    if (bytesRead < 0) {
        printf ("Error while getting the length of file %s.\n", filePath);
	goto loser;
    }
    PR_Close(fileDesc);
    
    fileDesc = PR_Open(filePath, PR_RDONLY, 0644);
    asn1Buf = PORT_ZNewArray(char, fileLen);
    if (asn1Buf == NULL) {
        printf ("Out of memory in function GetBitsFromFile\n");
        goto loser;
    }
    bytesRead = PR_Read(fileDesc, asn1Buf, fileLen);
    if (bytesRead != fileLen) {
        printf ("Error while reading the contents of %s\n", filePath);
	goto loser;
    }
    fileBits->data = (unsigned char*)asn1Buf;
    fileBits->len  = fileLen;
    return;
 loser:
    if (asn1Buf) {
        PORT_Free(asn1Buf);
    }
    fileBits->data = NULL;
    fileBits->len  = 0;
} 

int
DecodeCMMFCertRepContent(char *derFile)
{
    int         fileLen=0;
    char       *asn1Buf;
    SECItem     fileBits;
    CMMFCertRepContent *certRepContent;


    GetBitsFromFile(derFile, &fileBits);
    if (fileBits.data == NULL) {
        printf("Could not get bits from file %s\n", derFile);
        return 304;
    }
    asn1Buf = (char*)fileBits.data;
    fileLen = fileBits.len;
    certRepContent = CMMF_CreateCertRepContentFromDER(db, asn1Buf, fileLen);
    if (certRepContent == NULL) {
        printf ("Error while decoding %s\n", derFile);
	return 303;
    }
    CMMF_DestroyCertRepContent(certRepContent);
    PORT_Free(asn1Buf);
    return 0;
}

int
DoCMMFStuff(char *configdir)
{
    CMMFCertResponse   *certResp=NULL, *certResp2=NULL, *certResponses[3];
    CMMFCertRepContent *certRepContent=NULL;
    CERTCertificate    *cert=NULL, *caCert=NULL;
    CERTCertList       *list=NULL;
    PRFileDesc         *fileDesc=NULL;
    char                filePath[PATH_LEN];
    int                 rv = 0;
    long                random;
    CMMFKeyRecRepContent       *repContent=NULL;
    SECKEYPrivateKey           *privKey = NULL;
    SECKEYPublicKey *caPubKey;
    SECStatus        srv;
    SECItem          fileBits;
    
    certResp = CMMF_CreateCertResponse(0xff123);
    CMMF_CertResponseSetPKIStatusInfoStatus(certResp, cmmfGranted);
    cert = CERT_FindCertByNickname(CERT_GetDefaultCertDB(), personalCert);
    if (cert == NULL) {
        printf ("Could not find the certificate for %s\n", personalCert);
        rv = 416;
        goto finish;
    } 
    CMMF_CertResponseSetCertificate(certResp, cert);
    certResp2 = CMMF_CreateCertResponse(0xff122);
    CMMF_CertResponseSetPKIStatusInfoStatus(certResp2, cmmfGranted);
    CMMF_CertResponseSetCertificate(certResp2, cert);
    
    certResponses[0] = certResp;
    certResponses[1] = NULL;
    certResponses[2] = NULL;

    certRepContent = CMMF_CreateCertRepContent();
    CMMF_CertRepContentSetCertResponses(certRepContent, certResponses, 1);

    list = CERT_GetCertChainFromCert(cert, PR_Now(), certUsageEmailSigner);
    CMMF_CertRepContentSetCAPubs(certRepContent, list);

    PR_snprintf(filePath, PATH_LEN, "%s/%s", configdir, "CertRepContent.der");
    fileDesc = PR_Open (filePath, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 
			0666);
    if (fileDesc == NULL) {
        printf ("Could not open file %s\n", filePath);
	rv = 400;
	goto finish;
    }
    
    srv = CMMF_EncodeCertRepContent(certRepContent, WriteItOut, 
				    (void*)fileDesc);
    PORT_Assert (srv == SECSuccess);
    PR_Close(fileDesc);
    rv = DecodeCMMFCertRepContent(filePath);
    if (rv != 0) {
        goto finish;
    }
    random = 0xa4e7;
    caCert = CERT_FindCertByNickname(CERT_GetDefaultCertDB(), 
				     caCertName);
    if (caCert == NULL) {
        printf ("Could not get the certifcate for %s\n", caCertName);
	rv = 411;
	goto finish;
    }
    repContent = CMMF_CreateKeyRecRepContent();
    if (repContent == NULL) {
        printf ("Could not allocate a CMMFKeyRecRepContent structure\n");
	rv = 407;
	goto finish;
    }
    srv = CMMF_KeyRecRepContentSetPKIStatusInfoStatus(repContent, 
						      cmmfGrantedWithMods);
    if (srv != SECSuccess) {
        printf ("Error trying to set PKIStatusInfo for "
		"CMMFKeyRecRepContent.\n");
	rv = 406;
	goto finish;
    }
    srv = CMMF_KeyRecRepContentSetNewSignCert(repContent, cert);
    if (srv != SECSuccess) {
        printf ("Error trying to set the new signing certificate for "
		"key recovery\n");
	rv = 408;
	goto finish;
    }
    srv = CMMF_KeyRecRepContentSetCACerts(repContent, list);
    if (srv != SECSuccess) {
        printf ("Errory trying to add the list of CA certs to the "
		"CMMFKeyRecRepContent structure.\n");
	rv = 409;
	goto finish;
    }
    privKey = PK11_FindKeyByAnyCert(cert, NULL);
    if (privKey == NULL) {
        printf ("Could not get the private key associated with the\n"
		"certificate %s\n", personalCert);
	rv = 410;
	goto finish;
    }
    caPubKey = CERT_ExtractPublicKey(caCert);
    if (caPubKey == NULL) {
        printf ("Could not extract the public from the "
		"certificate for \n%s\n", caCertName);
	rv = 412;
	goto finish;
    }
    CERT_DestroyCertificate(caCert);
    caCert = NULL;
    srv = CMMF_KeyRecRepContentSetCertifiedKeyPair(repContent, cert, privKey,
						   caPubKey);
    SECKEY_DestroyPrivateKey(privKey);
    SECKEY_DestroyPublicKey(caPubKey);
    if (srv != SECSuccess) {
        printf ("Could not set the Certified Key Pair\n");
	rv = 413;
	goto finish;
    }
    PR_snprintf(filePath, PATH_LEN, "%s/%s", configdir, 
		"KeyRecRepContent.der");
    fileDesc = PR_Open (filePath, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 
			0666);
    if (fileDesc == NULL) {
        printf ("Could not open file %s\n", filePath);
	rv = 414;
	goto finish;
    }
    
    srv = CMMF_EncodeKeyRecRepContent(repContent, WriteItOut, 
				      (void*)fileDesc);
    PORT_Assert (srv == SECSuccess);
    PR_Close(fileDesc);
    CMMF_DestroyKeyRecRepContent(repContent);
    GetBitsFromFile(filePath, &fileBits);
    repContent = 
        CMMF_CreateKeyRecRepContentFromDER(db, (const char *) fileBits.data,
					   fileBits.len);
    if (repContent == NULL) {
        printf ("ERROR: CMMF_CreateKeyRecRepContentFromDER failed on file:\n"
		"\t%s\n", filePath);
	rv = 415;
	goto finish;
    }
 finish:
    if (repContent) {
        CMMF_DestroyKeyRecRepContent(repContent);
    }
    if (cert) {
        CERT_DestroyCertificate(cert);
    }
    if (list) {
        CERT_DestroyCertList(list);
    }
    if (certResp) {
        CMMF_DestroyCertResponse(certResp);
    }
    if (certResp2) {
        CMMF_DestroyCertResponse(certResp2);
    }
    if (certRepContent) {
        CMMF_DestroyCertRepContent(certRepContent);
    }
    return rv;
}

static CK_MECHANISM_TYPE
mapWrapKeyType(KeyType keyType)
{
    switch (keyType) {
    case rsaKey:
        return CKM_RSA_PKCS;
    default:
        break;
    }
    return CKM_INVALID_MECHANISM;
} 

#define KNOWN_MESSAGE_LENGTH 20 /*160 bits*/

int
DoKeyRecovery(char *configdir, SECKEYPrivateKey *privKey)
{
    SECKEYPublicKey *pubKey;
    PK11SlotInfo    *slot;
    CK_OBJECT_HANDLE id;
    CK_MECHANISM     mech = { CKM_INVALID_MECHANISM, NULL, 0};
    unsigned char *known_message = (unsigned char*)"Known Crypto Message";
    unsigned char  plaintext[KNOWN_MESSAGE_LENGTH];
    char filePath[PATH_LEN];
    CK_RV            crv;
    unsigned char   *ciphertext;
    CK_ULONG         max_bytes_encrypted, bytes_encrypted;
    unsigned char   *text_compared;
    CK_ULONG         bytes_compared, bytes_decrypted;
    SECKEYPrivateKey *unwrappedPrivKey, *caPrivKey;
    CMMFKeyRecRepContent *keyRecRep;
    SECStatus rv;
    CERTCertificate *caCert, *myCert;
    SECKEYPublicKey *caPubKey;
    PRFileDesc *fileDesc;
    SECItem fileBits, nickname;
    CMMFCertifiedKeyPair *certKeyPair;

    /*caCert = CERT_FindCertByNickname(CERT_GetDefaultCertDB(), 
				     caCertName);*/
    myCert = CERT_FindCertByNickname(CERT_GetDefaultCertDB(), personalCert);
    if (myCert == NULL) {
        printf ("Could not find the certificate for %s\n", personalCert);
        return 700;
    }
    caCert = CERT_FindCertByNickname(CERT_GetDefaultCertDB(), 
				     recoveryEncrypter);
    if (caCert == NULL) {
        printf ("Could not find the certificate for %s\n", recoveryEncrypter);
        return 701;
    }
    caPubKey = CERT_ExtractPublicKey(caCert);
    pubKey = SECKEY_ConvertToPublicKey(privKey);
    max_bytes_encrypted = PK11_GetPrivateModulusLen(privKey);
    slot = PK11_GetBestSlot(mapWrapKeyType(privKey->keyType), NULL);
    id = PK11_ImportPublicKey(slot, pubKey, PR_FALSE);
    switch(privKey->keyType) {
    case rsaKey:
        mech.mechanism = CKM_RSA_PKCS;
	break;
    case dsaKey:
        mech.mechanism = CKM_DSA;
	break;
    case dhKey:
        mech.mechanism = CKM_DH_PKCS_DERIVE;
	break;
    default:
        printf ("Bad Key type in key recovery.\n");
	return 512;

    }
    PK11_EnterSlotMonitor(slot);
    crv = PK11_GETTAB(slot)->C_EncryptInit(slot->session, &mech, id);
    if (crv != CKR_OK) {
        PK11_ExitSlotMonitor(slot);
	PK11_FreeSlot(slot);
	printf ("C_EncryptInit failed in KeyRecovery\n");
	return 500;
    }
    ciphertext = PORT_NewArray(unsigned char, max_bytes_encrypted);
    if (ciphertext == NULL) {
        PK11_ExitSlotMonitor(slot);
	PK11_FreeSlot(slot);
	printf ("Could not allocate memory for ciphertext.\n");
	return 501;
    }
    bytes_encrypted = max_bytes_encrypted;
    crv = PK11_GETTAB(slot)->C_Encrypt(slot->session, 
				       known_message,
				       KNOWN_MESSAGE_LENGTH,
				       ciphertext,
				       &bytes_encrypted);
    PK11_ExitSlotMonitor(slot);
    PK11_FreeSlot(slot);
    if (crv != CKR_OK) {
       PORT_Free(ciphertext);
       return 502;
    }
    /* Always use the smaller of these two values . . . */
    bytes_compared = ( bytes_encrypted > KNOWN_MESSAGE_LENGTH )
                      ? KNOWN_MESSAGE_LENGTH
                      : bytes_encrypted;
 
    /* If there was a failure, the plaintext */
    /* goes at the end, therefore . . .      */
    text_compared = ( bytes_encrypted > KNOWN_MESSAGE_LENGTH )
                    ? (ciphertext + bytes_encrypted -
		       KNOWN_MESSAGE_LENGTH )
                     : ciphertext;  

    keyRecRep = CMMF_CreateKeyRecRepContent();
    if (keyRecRep == NULL) {
        PORT_Free(ciphertext);
	PK11_FreeSlot(slot);
	CMMF_DestroyKeyRecRepContent(keyRecRep);
	printf ("Could not allocate a CMMFKeyRecRepContent structre.\n");
	return 503;
    }
    rv = CMMF_KeyRecRepContentSetPKIStatusInfoStatus(keyRecRep,
						     cmmfGranted);
    if (rv != SECSuccess) {
        PORT_Free(ciphertext);
	PK11_FreeSlot(slot);
	CMMF_DestroyKeyRecRepContent(keyRecRep);
	printf ("Could not set the status for the KeyRecRepContent\n");
	return 504;
    }
    /* The myCert here should correspond to the certificate corresponding
     * to the private key, but for this test any certificate will do.
     */
    rv = CMMF_KeyRecRepContentSetCertifiedKeyPair(keyRecRep, myCert,
						  privKey, caPubKey);
    if (rv != SECSuccess) {
        PORT_Free(ciphertext);
	PK11_FreeSlot(slot);
	CMMF_DestroyKeyRecRepContent(keyRecRep);
	printf ("Could not set the Certified Key Pair\n");
	return 505;
    }
    PR_snprintf(filePath, PATH_LEN, "%s/%s", configdir, 
		"KeyRecRepContent.der");
    fileDesc = PR_Open (filePath, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 
			0666);
    if (fileDesc == NULL) {
        PORT_Free(ciphertext);
	PK11_FreeSlot(slot);
	CMMF_DestroyKeyRecRepContent(keyRecRep);
        printf ("Could not open file %s\n", filePath);
	return 506;
    }
    rv = CMMF_EncodeKeyRecRepContent(keyRecRep, WriteItOut, fileDesc);
    CMMF_DestroyKeyRecRepContent(keyRecRep);
    PR_Close(fileDesc);

    if (rv != SECSuccess) {
        PORT_Free(ciphertext);
	PK11_FreeSlot(slot);
	printf ("Error while encoding CMMFKeyRecRepContent\n");
	return 507;
    }
    GetBitsFromFile(filePath, &fileBits);
    if (fileBits.data == NULL) {
        PORT_Free(ciphertext);
	PK11_FreeSlot(slot);
        printf ("Could not get the bits from file %s\n", filePath);
	return 508;
    }
    keyRecRep = 
        CMMF_CreateKeyRecRepContentFromDER(db,(const char*)fileBits.data,
					   fileBits.len);
    if (keyRecRep == NULL) {
        printf ("Could not decode the KeyRecRepContent in file %s\n", 
		filePath);
	PORT_Free(ciphertext);
	PK11_FreeSlot(slot);
	return 509;
    }
    caPrivKey = PK11_FindKeyByAnyCert(caCert, NULL);
    if (CMMF_KeyRecRepContentGetPKIStatusInfoStatus(keyRecRep) != 
	                                                      cmmfGranted) {
        PORT_Free(ciphertext);
	PK11_FreeSlot(slot);
	CMMF_DestroyKeyRecRepContent(keyRecRep);
	printf ("A bad status came back with the "
		"KeyRecRepContent structure\n");
	return 510;
    }
#define NICKNAME "Key Recovery Test Key"
    nickname.data = (unsigned char*)NICKNAME;
    nickname.len  = PORT_Strlen(NICKNAME);
    certKeyPair = CMMF_KeyRecRepContentGetCertKeyAtIndex(keyRecRep, 0);
    CMMF_DestroyKeyRecRepContent(keyRecRep);
    rv = CMMF_CertifiedKeyPairUnwrapPrivKey(certKeyPair,
					    caPrivKey,
					    &nickname,
					    PK11_GetInternalKeySlot(),
					    db,
					    &unwrappedPrivKey, NULL);
    CMMF_DestroyCertifiedKeyPair(certKeyPair);
    if (rv != SECSuccess) {
        printf ("Unwrapping the private key failed.\n");
	return 511;
    }
    /*Now let's try to decrypt the ciphertext with the "recovered" key*/
    PK11_EnterSlotMonitor(slot);
    crv = 
      PK11_GETTAB(slot)->C_DecryptInit(unwrappedPrivKey->pkcs11Slot->session,
				       &mech,
				       unwrappedPrivKey->pkcs11ID);
    if (crv != CKR_OK) {
        PK11_ExitSlotMonitor(slot);
	PORT_Free(ciphertext);
	PK11_FreeSlot(slot);
	printf ("Decrypting with the recovered key failed.\n");
	return 513;
    }
    bytes_decrypted = KNOWN_MESSAGE_LENGTH;
    crv = PK11_GETTAB(slot)->C_Decrypt(unwrappedPrivKey->pkcs11Slot->session,
				       ciphertext, 
				       bytes_encrypted, plaintext,
				       &bytes_decrypted);
    SECKEY_DestroyPrivateKey(unwrappedPrivKey);
    PK11_ExitSlotMonitor(slot);
    PORT_Free(ciphertext);
    if (crv != CKR_OK) {
        PK11_FreeSlot(slot);
	printf ("Decrypting the ciphertext with recovered key failed.\n");
	return 514;
    }
    if ((bytes_decrypted != KNOWN_MESSAGE_LENGTH) || 
	(PORT_Memcmp(plaintext, known_message, KNOWN_MESSAGE_LENGTH) != 0)) {
        PK11_FreeSlot(slot);
	printf ("The recovered plaintext does not equal the known message:\n"
		"\tKnown message:       %s\n"
		"\tRecovered plaintext: %s\n", known_message, plaintext);
	return 515;
    }
    return 0;
}

int
DoChallengeResponse(char *configdir, SECKEYPrivateKey *privKey,
		    SECKEYPublicKey *pubKey)
{
    CMMFPOPODecKeyChallContent *chalContent = NULL;
    CMMFPOPODecKeyRespContent  *respContent = NULL;
    CERTCertificate            *myCert      = NULL;
    CERTGeneralName            *myGenName   = NULL;
    PRArenaPool                *poolp       = NULL;
    SECItem                     DecKeyChallBits;
    long                       *randomNums;
    int                         numChallengesFound=0;
    int                         numChallengesSet = 1,i;
    long                        retrieved;
    char                        filePath[PATH_LEN];
    RNGContext                 *rng;
    SECStatus                   rv;
    PRFileDesc                 *fileDesc;
    SECItem                    *publicValue, *keyID;
    SECKEYPrivateKey           *foundPrivKey;

    chalContent = CMMF_CreatePOPODecKeyChallContent();
    myCert = CERT_FindCertByNickname(db, personalCert);
    if (myCert == NULL) {
        printf ("Could not find the certificate for %s\n", personalCert);
        return 900;
    }
    poolp = PORT_NewArena(1024);
    if (poolp == NULL) {
        printf("Could no allocate a new arena in DoChallengeResponse\n");
	return 901;
    }
    myGenName = CERT_GetCertificateNames(myCert, poolp);
    if (myGenName == NULL) {
        printf ("Could not get the general names for %s certificate\n", 
		personalCert);
	return 902;
    }
    randomNums = PORT_ArenaNewArray(poolp,long, numChallengesSet);
    rng = RNG_CreateContext();
    RNG_GenerateRandomBytes(rng, randomNums, numChallengesSet*sizeof(long));
    for (i=0; i<numChallengesSet; i++) {
	rv = CMMF_POPODecKeyChallContentSetNextChallenge(chalContent,
							 randomNums[i],
							 myGenName,
							 pubKey,
							 NULL);
    }
    RNG_DestroyContext(rng, PR_TRUE);
    if (rv != SECSuccess) {
        printf ("Could not set the challenge in DoChallengeResponse\n");
	return 903;
    }
    PR_snprintf(filePath, PATH_LEN, "%s/POPODecKeyChallContent.der", 
		configdir);
    fileDesc = PR_Open(filePath, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
		       0666);
    if (fileDesc == NULL) {
        printf ("Could not open file %s\n", filePath);
	return 904;
    }
    rv = CMMF_EncodePOPODecKeyChallContent(chalContent,WriteItOut, 
					   (void*)fileDesc);
    PR_Close(fileDesc);
    CMMF_DestroyPOPODecKeyChallContent(chalContent);
    if (rv != SECSuccess) {
        printf ("Could not encode the POPODecKeyChallContent.\n");
	return 905;
    }
    GetBitsFromFile(filePath, &DecKeyChallBits);
    chalContent = 
      CMMF_CreatePOPODecKeyChallContentFromDER
      ((const char*)DecKeyChallBits.data, DecKeyChallBits.len);
    PORT_Free(DecKeyChallBits.data);
    if (chalContent == NULL) {
        printf ("Could not create the POPODecKeyChallContent from DER\n");
	return 906;
    }
    numChallengesFound = 
      CMMF_POPODecKeyChallContentGetNumChallenges(chalContent);
    if (numChallengesFound != numChallengesSet) {
        printf ("Number of Challenges Found (%d) does not equal the number "
		"set (%d)\n", numChallengesFound, numChallengesSet);
	return 907;
    }
    for (i=0; i<numChallengesSet; i++) {
        publicValue = CMMF_POPODecKeyChallContentGetPublicValue(chalContent, i);
	if (publicValue == NULL) {
	  printf("Could not get the public value for challenge at index %d\n",
		 i);
	  return 908;
	}
	keyID = PK11_MakeIDFromPubKey(publicValue);
	if (keyID == NULL) {
	    printf ("Could not make the keyID from the public value\n");
	    return 909;
	}
	foundPrivKey = PK11_FindKeyByKeyID(privKey->pkcs11Slot,keyID, NULL);
	if (foundPrivKey == NULL) {
	    printf ("Could not find the private key corresponding to the public"
		    " value.\n");
	    return 910;
	}
	rv = CMMF_POPODecKeyChallContDecryptChallenge(chalContent, i, 
						      foundPrivKey);
	if (rv != SECSuccess) {
	    printf ("Could not decrypt the challenge at index %d\n", i);
	    return 911;
	}
	rv = CMMF_POPODecKeyChallContentGetRandomNumber(chalContent, i, 
							&retrieved);
	if (rv != SECSuccess) {
	    printf ("Could not get the random number from the challenge at "
		    "index %d\n", i);
	    return 912;
	}
	if (retrieved != randomNums[i]) {
	    printf ("Retrieved the number (%d), expected (%d)\n", retrieved,
		    randomNums[i]);
	    return 913;
	}
    }
    CMMF_DestroyPOPODecKeyChallContent(chalContent);
    PR_snprintf(filePath, PATH_LEN, "%s/POPODecKeyRespContent.der", 
		configdir);
    fileDesc = PR_Open(filePath, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
		       0666);
    if (fileDesc == NULL) {
        printf ("Could not open file %s\n", filePath);
	return 914;
    }
    rv = CMMF_EncodePOPODecKeyRespContent(randomNums, numChallengesSet,
					  WriteItOut, fileDesc);
    PR_Close(fileDesc);
    if (rv != 0) {
        printf ("Could not encode the POPODecKeyRespContent\n");
	return 915;
    }
    GetBitsFromFile(filePath, &DecKeyChallBits);
    respContent = 
     CMMF_CreatePOPODecKeyRespContentFromDER((const char*)DecKeyChallBits.data,
					     DecKeyChallBits.len);
    if (respContent == NULL) {
        printf ("Could not decode the contents of the file %s\n", filePath);
	return 916;
    }
    numChallengesFound = 
         CMMF_POPODecKeyRespContentGetNumResponses(respContent);
    if (numChallengesFound != numChallengesSet) {
        printf ("Number of responses found (%d) does not match the number "
		"of challenges set (%d)\n",
		numChallengesFound, numChallengesSet);
	return 917;
    }
    for (i=0; i<numChallengesSet; i++) {
        rv = CMMF_POPODecKeyRespContentGetResponse(respContent, i, &retrieved);
	if (rv != SECSuccess) {
	    printf ("Could not retrieve the response at index %d\n", i);
	    return 918;
	}
	if (retrieved != randomNums[i]) {
	    printf ("Retrieved the number (%ld), expected (%ld)\n", retrieved,
		    randomNums[i]);
	    return 919;
	}
							  
    }
    CMMF_DestroyPOPODecKeyRespContent(respContent);
    return 0;
}

char *
certdb_name_cb(void *arg, int dbVersion)
{
    char *configdir = (char *)arg;
    char *dbver;

    switch (dbVersion) {
    case 7:
       dbver = "7";
       break;
    case 6:
       dbver = "6";
       break;
    case 5:
        dbver = "5";
	break;
    case 4:
    default:
        dbver = "";
	break;
    }
    return PR_smprintf("%s/cert%s.db", configdir, dbver);
}

SECStatus
OpenCertDB(char *configdir)
{
    CERTCertDBHandle *certdb;
    SECStatus         status = SECFailure;

    certdb = PORT_ZNew(CERTCertDBHandle);
    if (certdb == NULL) {
        goto loser;
    }
    status = CERT_OpenCertDB(certdb, PR_TRUE, certdb_name_cb, configdir);
    if (status == SECSuccess) {
        CERT_SetDefaultCertDB(certdb);
	db = certdb;
    } else {
        PORT_Free(certdb);
    }
 loser:
    return status;
}

char *
keydb_name_cb(void *arg, int dbVersion)
{
    char *configdir = (char*) arg;
    char *dbver;

    switch(dbVersion){
    case 3:
        dbver = "3";
	break;
    case 2:
    default:
        dbver = "";
	break;
    }
    return PR_smprintf("%s/key%s.db", configdir, dbver);
}

SECStatus
OpenKeyDB(char *configdir)
{
    SECKEYKeyDBHandle *keydb;

    keydb = SECKEY_OpenKeyDB(PR_FALSE, keydb_name_cb, configdir);
    if (keydb == NULL) {
        return SECFailure;
    }
    SECKEY_SetDefaultKeyDB(keydb);
    return SECSuccess;
}

SECStatus
OpenSecModDB(char *configdir)
{
    char *secmodname = PR_smprintf("%d/secmod.db", configdir);
    if (secmodname == NULL) {
        return SECFailure;
    }
    SECMOD_init(secmodname);
    return SECSuccess;
}

void
CloseHCL(void)
{
    CERTCertDBHandle  *certHandle;
    SECKEYKeyDBHandle *keyHandle;
    
    certHandle = CERT_GetDefaultCertDB();
    if (certHandle) {
        CERT_ClosePermCertDB(certHandle);
    }
    keyHandle = SECKEY_GetDefaultKeyDB();
    if (keyHandle) {
        SECKEY_CloseKeyDB(keyHandle);
    }
}

SECStatus
InitHCL(char *configdir)
{
    SECStatus status;
    SECStatus rv = SECFailure;

    RNG_RNGInit();
    RNG_SystemInfoForRNG();

    status = OpenCertDB(configdir);
    if (status != SECSuccess) {
        goto loser;
    }
    
    status = OpenKeyDB(configdir);
    if (status != SECSuccess) {
        goto loser;
    }
    
    status = OpenSecModDB(configdir);
    if (status != SECSuccess) {
       goto loser;
    }
    
    rv = SECSuccess;

 loser:
    if (rv != SECSuccess) {
        CloseHCL();
    }
    return rv;
}
void
Usage (void)
{
    printf ("Usage:\n"
	    "\tcrmftest -d [Database Directory] -p [Personal Cert]\n"
	    "\t         -e [Encrypter] -s [CA Certificate]\n\n"
	    "Database Directory\n"
	    "\tThis is the directory where the key3.db, cert7.db, and\n"
	    "\tsecmod.db files are located.  This is also the directory\n"
	    "\twhere the program will place CRMF/CMMF der files\n"
	    "Personal Cert\n"
	    "\tThis is the certificate that already exists in the cert\n"
	    "\tdatabase to use while encoding the response.  The private\n"
	    "\tkey associated with the certificate must also exist in the\n"
	    "\tkey database.\n"
	    "Encrypter\n"
	    "\tThis is the certificate to use when encrypting the the \n"
	    "\tkey recovery response.  The private key for this cert\n"
	    "\tmust also be present in the key database.\n"
	    "CA Certificate\n"
	    "\tThis is the nickname of the certificate to use as the\n"
	    "\tCA when doing all of the encoding.\n");
}

int
main(int argc, char **argv) 
{
    CRMFCertRequest *certReq, *certReq2;
    CRMFCertReqMsg *certReqMsg;
    CRMFCertReqMsg *secondMsg;
    char *configdir;
    int irv;
    SECStatus rv;
    SECKEYPrivateKey *privKey;
    SECKEYPublicKey  *pubKey;
    int o;
    PRBool hclInit = PR_FALSE, pArg = PR_FALSE, eArg = PR_FALSE, 
           sArg = PR_FALSE;

    printf ("\ncrmftest v1.0\n");
    while (-1 != (o = getopt(argc, argv, "d:p:e:s:"))) {
        switch(o) {
	case 'd':
	    configdir = PORT_Strdup(optarg);
	    rv = InitHCL(configdir);
	    if (rv != SECSuccess) {
	        printf ("InitHCL failed\n");
		return 101;
	    }	      
	    hclInit = PR_TRUE;
	    break;
	case 'p':
	    personalCert = PORT_Strdup(optarg);
	    if (personalCert == NULL) {
	        return 603;
	    }
	    pArg = PR_TRUE;
	    break;
	case 'e':
	    recoveryEncrypter = PORT_Strdup(optarg);
	    if (recoveryEncrypter == NULL) {
	        return 602;
	    }
	    eArg = PR_TRUE;
	    break;
	case 's':
	    caCertName = PORT_Strdup(optarg);
	    if (caCertName == NULL) {
	        return 604;
	    }
	    sArg = PR_TRUE;
	    break;
	default:
	  Usage();
	  return 601;
	}
    }
    if (!hclInit || !pArg || !eArg || !sArg) {
        Usage();
	return 600;
    }

    InitPKCS11();
    
    irv = CreateCertRequest(&certReq, &privKey, &pubKey);
    if (irv != 0 || certReq == NULL) {
        goto loser;
    }

    certReqMsg = CRMF_CreateCertReqMsg();
    secondMsg  = CRMF_CreateCertReqMsg();
    CRMF_CertReqMsgSetCertRequest(certReqMsg, certReq);
    CRMF_CertReqMsgSetCertRequest(secondMsg,  certReq);

    irv = AddProofOfPossession(certReqMsg, privKey, pubKey, crmfSignature);
    irv = AddProofOfPossession(secondMsg, privKey, pubKey, crmfKeyAgreement);
    irv = Encode (certReqMsg, secondMsg, configdir);
    if (irv != 0) {
        goto loser;
    }
    
    rv = CRMF_DestroyCertRequest (certReq);
    if (rv != SECSuccess) {
        printf ("Error when destroy certificate request.\n");
	irv = 100;
	goto loser;
    }

    rv = CRMF_DestroyCertReqMsg(certReqMsg);
    CRMF_DestroyCertReqMsg(secondMsg);

    irv = Decode (configdir);
    if (irv != 0) {
        printf("Error while decoding\n");
	goto loser;
    }

    if ((irv = DoCMMFStuff(configdir)) != 0) {
        printf ("CMMF tests failed.\n");
	goto loser;
    }

    if ((irv = DoKeyRecovery(configdir, privKey)) != 0) {
        printf ("Error doing key recovery\n");
	goto loser;
    }

    if ((irv = DoChallengeResponse(configdir, privKey, pubKey)) != 0) {
        printf ("Error doing challenge-response\n");
	goto loser;
    }
    printf ("Exiting successfully!!!\n\n");
    irv = 0;

 loser:
    CloseHCL();
    PORT_Free(configdir);
    return irv;
}
