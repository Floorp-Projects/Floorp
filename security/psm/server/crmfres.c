/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
/* Cartman Server specific includes */
#include "serv.h"
#include "crmfres.h"
#include "kgenctxt.h"
#include "p12res.h"
#include "newproto.h"
#include "messages.h"
#include "certres.h"
#include "oldfunc.h"

/* NSS include files */
#include "base64.h"
#include "crmfi.h"
#include "cmmfi.h"
#include "cert.h"
#include "secmod.h"
#include "secasn1.h"

/*
 * These defines are used for setting the keyUsage extensions in 
 * the requests.
 */

#define DIGITAL_SIGNATURE KU_DIGITAL_SIGNATURE
#define NON_REPUDIATION   KU_NON_REPUDIATION
#define KEY_ENCIPHERMENT  KU_KEY_ENCIPHERMENT
#define KEY_AGREEMENT     KU_KEY_AGREEMENT

/* I can use this for both SSMKeyPair and SSMCRMFRequest because they
 * both inherit directly from SSMResource
 *
 * A simple macro for an object that inherits from SSMResource that will
 * convert the object pointer into a SSMResource pointer.
 */
#define SSMRESOURCE(object) (&(object)->super)

/*
 * This structure is used for callbacks passed to the ASN1 encoder.
 */
typedef struct CRMFASN1ArgStr {
    SECItem buffer;
    long    allocatedLen;
} CRMFASN1Arg;

/*
 * Default allocation size for a DER chunk that will be passed to the 
 * ASN1 parser.
 */
#define DEFAULT_ASN1_CHUNK_SIZE 1024

typedef struct CMMFResponseArgStr {
    SECItem *msg;
    SSMControlConnection *connection;
} CMMFResponseArg;

/* Forward decls */
void
SSMCRMFRequest_PrepareBitStringForEncoding (SECItem *bitsmap, SECItem *value);
SSMStatus
SSMCRMFRequest_SetKeyUsageExtension(SSMCRMFRequest *crmfReq,
                                    unsigned char   keyUsage);
SSMStatus
SSMCRMFRequest_SetRSADualUse(SSMCRMFRequest *crmfReq);
SSMStatus
SSMCRMFRequest_SetRSAKeyEx(SSMCRMFRequest *crmfReq);
SSMStatus
SSMCRMFRequest_SetRSASign(SSMCRMFRequest *crmfReq);
SSMStatus
SSMCRMFRequest_SetRSANonRepudiation(SSMCRMFRequest *crmfReq);
SSMStatus
SSMCRMFRequest_SetRSASignNonRepudiation(SSMCRMFRequest *crmfReq);
SSMStatus
SSMCRMFRequest_SetDH(SSMCRMFRequest *crmfReq);
SSMStatus
SSMCRMFRequest_SetDSASign(SSMCRMFRequest *crmfReq);
SSMStatus
SSMCRMFRequest_SetDSANonRepudiation(SSMCRMFRequest *crmfReq);
SSMStatus
SSMCRMFRequest_SetDSASignNonRepudiation(SSMCRMFRequest *crmfReq);
SSMStatus
SSMCRMFRequest_SetKeyGenType(SSMCRMFRequest *crmfReq, SSMKeyGenType keyGenType);
SSMStatus
SSMCRMFRequest_SetDN(SSMCRMFRequest *crmfReq, 
                     char           *data);
SSMStatus
SSMCRMFRequest_SetRegToken(SSMCRMFRequest *crmfReq,
			   char           *value);
SSMStatus
SSMCRMFRequest_SetAuthenticator(SSMCRMFRequest *crmfReq,
				 char           *value);
PRBool
SSMCRMFRequest_HasEscrowAuthority(SSMCRMFRequest *currReq);
SSMStatus
SSMCRMFRequest_SetKeyEnciphermentPOP(CRMFCertReqMsg *certReqMsg,
                                     SSMCRMFRequest *curReq);
SSMStatus
SSMCRMFRequest_SetProofOfPossession(CRMFCertReqMsg *certReqMsg, 
				    SSMCRMFRequest *curReq);
SECKEYPrivateKey*
SSM_FindPrivKeyFromKeyID(SECItem *keyID, SECMODModule *currMod,
                         SSMControlConnection *ctrl);
SECKEYPrivateKey*
SSM_FindPrivKeyFromPubValue(SECItem *publicValue, SECMODModuleList *modList,
                            SSMControlConnection *ctrl);
SSMStatus
SSM_InitCRMFASN1Arg(CRMFASN1Arg *arg);
void
SSM_GenericASN1Callback(void *arg, const char *buf, unsigned long len);

SSMStatus
SSM_CreateNewCRMFRequest(SECItem *msg, SSMControlConnection *connection,
                         SSMResourceID *destID)
{
  SSMStatus     prv;
  SSMResource *keyPairRes=NULL, *crmfReqRes=NULL;
  SSMCRMFRequestArg arg;
  SingleNumMessage request;

  if (CMT_DecodeMessage(SingleNumMessageTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
      goto loser;
  }

  prv = SSMControlConnection_GetResource(connection, (SSMResourceID)request.value, 
                                         &keyPairRes);
  if (prv != PR_SUCCESS || keyPairRes == NULL) {
    goto loser;
  }
  if (!SSM_IsAKindOf(keyPairRes, SSM_RESTYPE_KEY_PAIR)) {
    goto loser;
  }
  arg.keyPair    = (SSMKeyPair*)keyPairRes;
  arg.connection = connection;
  prv = SSM_CreateResource(SSM_RESTYPE_CRMF_REQUEST, (void*)&arg, connection,
                           destID, &crmfReqRes);

  if (prv != PR_SUCCESS || crmfReqRes == NULL) {
    goto loser;
  }
  SSM_FreeResource(keyPairRes);
  crmfReqRes->m_clientCount++;
  return PR_SUCCESS;
 loser:
  if (keyPairRes != NULL) {
    SSM_FreeResource(keyPairRes);
  }
  if (crmfReqRes != NULL) {
    SSM_FreeResource(crmfReqRes);
  }
  return PR_FAILURE;
}


SSMStatus
SSMCRMFRequest_Create(void *arg, SSMControlConnection * connection, 
                      SSMResource **res)
{
  SSMCRMFRequest    *crmfReq;
  SSMStatus           rv;
  SSMCRMFRequestArg *createArg = (SSMCRMFRequestArg*)arg;

  crmfReq = SSM_ZNEW(SSMCRMFRequest);
  if (crmfReq == NULL) {
    goto loser;
  }
  rv = SSMCRMFRequest_Init(crmfReq, connection, SSM_RESTYPE_CRMF_REQUEST, 
                           createArg->keyPair, createArg->connection);
  
  if (rv != PR_SUCCESS) {
    goto loser;
  }
  *res = SSMRESOURCE(crmfReq);
  return PR_SUCCESS;
  
 loser:
  if (crmfReq != NULL) {
    SSM_FreeResource(SSMRESOURCE(crmfReq));
  }
  *res = NULL;
  return PR_FAILURE;
}

SSMStatus
SSMCRMFRequest_Init(SSMCRMFRequest       *inCRMFReq,
		    SSMControlConnection * conn,
                    SSMResourceType       type,
                    SSMKeyPair           *inKeyPair,
                    SSMControlConnection *inConnection)
{
  CRMFCertRequest          *certReq = NULL;
  CERTSubjectPublicKeyInfo *spki    = NULL;
  SSMStatus                  rv;
  SECStatus                 srv;
  long                      version = SEC_CERTIFICATE_VERSION_3 ; 
                                         /* Version passed in is the value 
                                          * to be encoded for the version
                                          * of the certificate.
                                          */

  if (inKeyPair == NULL || inConnection == NULL) {
      goto loser;
  }
  rv = SSMResource_Init(conn, SSMRESOURCE(inCRMFReq), type);
  if (rv != PR_SUCCESS) {
    goto loser;
  }
  /* This reference will be inherited by the SSMCRMFRequest
   * structure being initialized.
   */
  SSM_GetResourceReference(SSMRESOURCE(inKeyPair));
  if (inKeyPair->m_PubKey  == NULL || 
      inKeyPair->m_PrivKey == NULL) {
    goto loser;
  }
  inCRMFReq->m_KeyPair    = inKeyPair;
  inCRMFReq->m_KeyGenType = invalidKeyGen;
  inCRMFReq->m_Connection = inConnection;
  /* How do I remember that request number? */
  certReq = CRMF_CreateCertRequest(rand());
  if (certReq == NULL) {
    goto loser;
  }
  srv = CRMF_CertRequestSetTemplateField(certReq, crmfVersion, &version);
  if (srv != SECSuccess) {
    goto loser;
  }
  spki = SECKEY_CreateSubjectPublicKeyInfo(inKeyPair->m_PubKey);
  if(spki == NULL) {
    goto loser;
  }
  srv = CRMF_CertRequestSetTemplateField(certReq, crmfPublicKey, spki);
  SECKEY_DestroySubjectPublicKeyInfo(spki);
  if (srv != SECSuccess) {
    goto loser;
  }
  inCRMFReq->m_CRMFRequest = certReq;
  if (inKeyPair->m_KeyGenContext->m_eaCert != NULL &&
      inKeyPair->m_KeyGenType == rsaEnc) {
      rv = SSMCRMFRequest_SetEscrowAuthority(inCRMFReq,
                                        inKeyPair->m_KeyGenContext->m_eaCert);
      if (rv != PR_SUCCESS) {
        goto loser;
      }
  }
  return PR_SUCCESS;
 loser:
  /* We only obtain a reference to the key pair in this function, so 
   * that's the only reference we need to free in here.
   */
  if (inKeyPair != NULL) {
    SSM_FreeResource(SSMRESOURCE(inKeyPair));
  }
  if (certReq != NULL) {
    CRMF_DestroyCertRequest(certReq);
  }
  return PR_FAILURE;
}

SSMStatus
SSMCRMFRequest_Destroy(SSMResource *inRequest, PRBool doFree)
{
  SSMCRMFRequest *crmfReq = (SSMCRMFRequest*)inRequest;

  if (crmfReq->m_KeyPair != NULL) {
    SSM_FreeResource(SSMRESOURCE(crmfReq->m_KeyPair));
    crmfReq->m_KeyPair = NULL;
  }
  if (crmfReq->m_CRMFRequest != NULL) {
    CRMF_DestroyCertRequest(crmfReq->m_CRMFRequest);
    crmfReq->m_CRMFRequest = NULL;
  }
  SSMResource_Destroy(SSMRESOURCE(crmfReq), PR_FALSE);
  if (doFree) {
    PR_Free(crmfReq);
  }
  return PR_SUCCESS;
}

void
SSMCRMFRequest_PrepareBitStringForEncoding (SECItem *bitsmap, SECItem *value)
{
  unsigned char onebyte;
  unsigned int i, len = 0;

  /* to prevent warning on some platform at compile time */
  onebyte = '\0';
  /* Get the position of the right-most turn-on bit */
  for (i = 0; i < (value->len ) * 8; ++i) {
      if (i % 8 == 0)
          onebyte = value->data[i/8];
      if (onebyte & 0x80)
          len = i;
      onebyte <<= 1;

  }
  bitsmap->data = value->data;
  /* Add one here since we work with base 1 */
  bitsmap->len = len + 1;
}

/*
 * Below is the set of functions that set the keyUsage extension in the
 * request.  Each different type of key generation has a different
 * value that gets set for keyUsage.  So each one defines the 
 * value and just calls SSMCRMFRequest_SetKeyUsageExtension.
 */

SSMStatus
SSMCRMFRequest_SetKeyUsageExtension(SSMCRMFRequest *crmfReq,
                                    unsigned char   keyUsage)
{
  SECItem                 *encodedExt= NULL;
  SECItem                  keyUsageValue = { (SECItemType) 0, NULL, 0 };
  SECItem                  bitsmap = { (SECItemType) 0, NULL, 0 };
  SECStatus                srv;
  CRMFCertExtension       *ext = NULL;
  CRMFCertExtCreationInfo  extAddParams;
  SEC_ASN1Template         bitStrTemplate = {SEC_ASN1_BIT_STRING, 0, NULL,
                                             sizeof(SECItem)};

  keyUsageValue.data = &keyUsage;
  keyUsageValue.len  = 1;
  SSMCRMFRequest_PrepareBitStringForEncoding(&bitsmap, &keyUsageValue);

  encodedExt = SEC_ASN1EncodeItem(NULL, NULL, &bitsmap,&bitStrTemplate);
  if (encodedExt == NULL) {
      goto loser;
  }
  ext = CRMF_CreateCertExtension(SEC_OID_X509_KEY_USAGE, PR_TRUE, encodedExt);
  if (ext == NULL) {
      goto loser;
  }
  extAddParams.numExtensions = 1;
  extAddParams.extensions = &ext;
  srv = CRMF_CertRequestSetTemplateField(crmfReq->m_CRMFRequest, crmfExtension,
                                         &extAddParams);
  CRMF_DestroyCertExtension(ext);
  if (srv != SECSuccess) {
      goto loser;
  }
  return PR_SUCCESS;
 loser:
  if (encodedExt != NULL) {
      SECITEM_FreeItem(encodedExt, PR_TRUE);
  }
  return PR_FAILURE;
}

SSMStatus
SSMCRMFRequest_SetRSADualUse(SSMCRMFRequest *crmfReq)
{
  unsigned char keyUsage =   DIGITAL_SIGNATURE
                           | NON_REPUDIATION
                           | KEY_ENCIPHERMENT;

  crmfReq->m_KeyGenType = rsaDualUse;
  return SSMCRMFRequest_SetKeyUsageExtension(crmfReq, keyUsage);
}

SSMStatus
SSMCRMFRequest_SetRSAKeyEx(SSMCRMFRequest *crmfReq)
{
  unsigned char keyUsage = KEY_ENCIPHERMENT;
			     
  crmfReq->m_KeyGenType = rsaEnc;
  return SSMCRMFRequest_SetKeyUsageExtension(crmfReq, keyUsage);
}

SSMStatus
SSMCRMFRequest_SetRSASign(SSMCRMFRequest *crmfReq)
{
  unsigned char keyUsage = DIGITAL_SIGNATURE;
                         

  crmfReq->m_KeyGenType = rsaSign;
  return SSMCRMFRequest_SetKeyUsageExtension(crmfReq, keyUsage);
}

SSMStatus
SSMCRMFRequest_SetRSANonRepudiation(SSMCRMFRequest *crmfReq)
{
  unsigned char keyUsage = NON_REPUDIATION;

  crmfReq->m_KeyGenType = rsaNonrepudiation;
  return SSMCRMFRequest_SetKeyUsageExtension(crmfReq, keyUsage);
}

SSMStatus
SSMCRMFRequest_SetRSASignNonRepudiation(SSMCRMFRequest *crmfReq)
{
  unsigned char keyUsage = DIGITAL_SIGNATURE |
                           NON_REPUDIATION;

  crmfReq->m_KeyGenType = rsaSignNonrepudiation;
  return SSMCRMFRequest_SetKeyUsageExtension(crmfReq, keyUsage);
}

SSMStatus
SSMCRMFRequest_SetDH(SSMCRMFRequest *crmfReq)
{
  unsigned char keyUsage = KEY_AGREEMENT;
  
  crmfReq->m_KeyGenType = dhEx;
  return SSMCRMFRequest_SetKeyUsageExtension(crmfReq, keyUsage);
}

SSMStatus
SSMCRMFRequest_SetDSASign(SSMCRMFRequest *crmfReq)
{
  unsigned char keyUsage = DIGITAL_SIGNATURE;

  crmfReq->m_KeyGenType = dsaSign;
  return SSMCRMFRequest_SetKeyUsageExtension(crmfReq, keyUsage);
}

SSMStatus
SSMCRMFRequest_SetDSANonRepudiation(SSMCRMFRequest *crmfReq)
{
  unsigned char keyUsage = NON_REPUDIATION;

  crmfReq->m_KeyGenType = dsaNonrepudiation;
  return SSMCRMFRequest_SetKeyUsageExtension(crmfReq, keyUsage);
}

SSMStatus
SSMCRMFRequest_SetDSASignNonRepudiation(SSMCRMFRequest *crmfReq)
{
  unsigned char keyUsage = DIGITAL_SIGNATURE |
                           NON_REPUDIATION;

  crmfReq->m_KeyGenType = dsaSignNonrepudiation;
  return SSMCRMFRequest_SetKeyUsageExtension(crmfReq, keyUsage);
}

SSMStatus
SSMCRMFRequest_SetKeyGenType(SSMCRMFRequest *crmfReq, SSMKeyGenType keyGenType)
{
  SSMStatus rv;

  if (crmfReq->m_KeyGenType != invalidKeyGen) {
    return PR_FAILURE;
  }
  switch (keyGenType) {
  case rsaDualUse:
    rv = SSMCRMFRequest_SetRSADualUse(crmfReq);
    break;
  case rsaEnc:
    rv = SSMCRMFRequest_SetRSAKeyEx(crmfReq);
    break;
  case rsaSign:
    rv = SSMCRMFRequest_SetRSASign(crmfReq);
    break;
  case rsaNonrepudiation:
    rv = SSMCRMFRequest_SetRSANonRepudiation(crmfReq);
    break;
  case rsaSignNonrepudiation:
    rv = SSMCRMFRequest_SetRSASignNonRepudiation(crmfReq);
    break;
  case dhEx:
    rv = SSMCRMFRequest_SetDH(crmfReq);
    break;
  case dsaSign:
    rv = SSMCRMFRequest_SetDSASign(crmfReq);
    break;
  case dsaNonrepudiation:
    rv = SSMCRMFRequest_SetDSANonRepudiation(crmfReq);
    break;
  case dsaSignNonrepudiation:
    rv = SSMCRMFRequest_SetDSASignNonRepudiation(crmfReq);
    break;
  default:
    SSM_DEBUG("Unknown KeyGenType %d\n", keyGenType);
    rv = PR_FAILURE;
    break;
  }
  return rv;
}

/*
 * This function takes an ASCII string in RFC1485 format and converts it
 * to a CERTName, then sets that names as the subjectName for the request
 * passed in.
 */
SSMStatus
SSMCRMFRequest_SetDN(SSMCRMFRequest *crmfReq, 
                     char           *data)
{
  SECStatus    srv;
  CERTName    *subjectName;

  if (data == NULL || CRMF_CertRequestIsFieldPresent(crmfReq->m_CRMFRequest,
                                                     crmfSubject)) {
    return PR_FAILURE;
  }
  subjectName = CERT_AsciiToName(data);
  if (subjectName == NULL) {
    return PR_FAILURE;
  }
  srv = CRMF_CertRequestSetTemplateField (crmfReq->m_CRMFRequest, crmfSubject, 
                                          (void*)subjectName);
  CERT_DestroyName(subjectName);
  return (srv == SECSuccess) ? PR_SUCCESS : PR_FAILURE;
}

/*
 * This function will set the Registration Token Control in 
 * the certificate request passed in.  We only allow one registration
 * token control per request in Cartman.
 */
SSMStatus
SSMCRMFRequest_SetRegToken(SSMCRMFRequest *crmfReq,
			   char           *value)
{
  SECItem   src, *derEncoded;
  SECStatus rv;

  if (CRMF_CertRequestIsControlPresent(crmfReq->m_CRMFRequest, 
                                       crmfRegTokenControl)) {
    return PR_FAILURE;
  }
  src.data = (unsigned char*)value;
  src.len  = strlen(value)+1;
  derEncoded = SEC_ASN1EncodeItem(NULL, NULL, &src, SEC_UTF8StringTemplate);
  if (derEncoded == NULL) {
    return PR_FAILURE;
  }
  rv = CRMF_CertRequestSetRegTokenControl(crmfReq->m_CRMFRequest, derEncoded);
  SECITEM_FreeItem(derEncoded, PR_TRUE);
  if (rv != SECSuccess) {
    return PR_FAILURE;
  }
  return PR_SUCCESS;
}

/*
 * This function sets the authenticator control in the certificate request.
 * The string passed in is an ASCII string.  The CRMF libraries encode that
 * string as a UTF8 string and then include it in the request.  Cartman will
 * only allow one Authenticator control per request.
 */
SSMStatus
SSMCRMFRequest_SetAuthenticator(SSMCRMFRequest *crmfReq,
				 char           *value)
{
  SECItem   src, *derEncoded;
  SECStatus rv;

  if (CRMF_CertRequestIsControlPresent(crmfReq->m_CRMFRequest,
                                       crmfAuthenticatorControl)) {
    return PR_FAILURE;
  }
  src.data = (unsigned char*)value;
  src.len  = strlen(value)+1;
  derEncoded = SEC_ASN1EncodeItem(NULL, NULL, &src, SEC_UTF8StringTemplate);
  if (derEncoded == NULL) {
    return PR_FAILURE;
  }
  rv = CRMF_CertRequestSetAuthenticatorControl(crmfReq->m_CRMFRequest, 
                                               derEncoded);
  SECITEM_FreeItem(derEncoded, PR_TRUE);
  if (rv != SECSuccess) {
    return PR_FAILURE;
  }
  return PR_SUCCESS;
}

SSMStatus
SSMCRMFRequest_SetEscrowAuthority(SSMCRMFRequest  *crmfReq,
                                  CERTCertificate *eaCert)
{
  CRMFEncryptedKey      *encrKey   = NULL;
  CRMFPKIArchiveOptions *archOpt   = NULL;
  SECStatus              rv;

  PR_ASSERT(eaCert!= NULL);
  if (eaCert == NULL || 
      CRMF_CertRequestIsControlPresent(crmfReq->m_CRMFRequest, 
                                       crmfPKIArchiveOptionsControl)) {
    return PR_FAILURE;
  }

  /*
   * CRMF_CreatePKIArchiveOptions checks to see if the value passed in,
   * in this case encrKey, is NULL.  So passing in the value without 
   * checking the contents first is OK in this case.  Really.
   */
  encrKey = 
    CRMF_CreateEncryptedKeyWithEncryptedValue(crmfReq->m_KeyPair->m_PrivKey,
                                              eaCert);
  archOpt = CRMF_CreatePKIArchiveOptions(crmfEncryptedPrivateKey, 
                                         (void*)encrKey);
  if (archOpt == NULL) {
    goto loser;
  }
  rv = CRMF_CertRequestSetPKIArchiveOptions(crmfReq->m_CRMFRequest, archOpt);
  if (rv != SECSuccess) {
    goto loser;
  }
  CRMF_DestroyEncryptedKey(encrKey);
  CRMF_DestroyPKIArchiveOptions(archOpt);
  return PR_SUCCESS;
 loser:
  /* Free up all the memory here. */
  if (encrKey != NULL) {
    CRMF_DestroyEncryptedKey(encrKey);
  }
  if (archOpt != NULL) {
    CRMF_DestroyPKIArchiveOptions(archOpt);
  }
  return PR_FAILURE;
}

SSMStatus
SSMCRMFRequest_SetAttr(SSMResource         *res,
                       SSMAttributeID      attrID,
                       SSMAttributeValue   *value)
{
  SSMCRMFRequest *crmfReq = (SSMCRMFRequest*)res;
  SSMStatus        rv;

  SSM_DEBUG("Calling SSMCRMFRequest_SetAttr\n");
  switch (attrID) {
  case SSM_FID_CRMFREQ_KEY_TYPE:
    SSM_DEBUG("Setting SSMKeyGenType to %d\n", value->u.numeric);
    if (value->type != SSM_NUMERIC_ATTRIBUTE) {
      rv = PR_FAILURE;
      break;
    }
    rv = SSMCRMFRequest_SetKeyGenType(crmfReq, (SSMKeyGenType) value->u.numeric);
    break;
  case SSM_FID_CRMFREQ_DN:
    SSM_DEBUG("Setting the DN to %s\n", value->u.string.data);
    if (value->type != SSM_STRING_ATTRIBUTE) {
      rv = PR_FAILURE;
      break;
    }
    rv = SSMCRMFRequest_SetDN(crmfReq, (char *) value->u.string.data);
    break;
  case SSM_FID_CRMFREQ_REGTOKEN:
    SSM_DEBUG("Setting the regToken to %s\n", value->u.string.data);
    if (value->type != SSM_STRING_ATTRIBUTE) {
      rv = PR_FAILURE;
      break;
    }
    rv = SSMCRMFRequest_SetRegToken(crmfReq, (char *) value->u.string.data);
    break;
  case SSM_FID_CRMFREQ_AUTHENTICATOR:
    SSM_DEBUG("Setting the authenticator to %s\n", value->u.string.data);
    rv = SSMCRMFRequest_SetAuthenticator(crmfReq, (char *) value->u.string.data);
    break;
  default:
    SSM_DEBUG("Got unknown CRMF Set Attribute request %d\n", attrID);
    rv = PR_FAILURE;
    break;
  }
  return rv;
}

/*
 * This function checks to see if a CRMF request has the PKIArchiveOptions
 * control already set.
 */
PRBool
SSMCRMFRequest_HasEscrowAuthority(SSMCRMFRequest *currReq)
{
  return CRMF_CertRequestIsControlPresent(currReq->m_CRMFRequest,
                                          crmfPKIArchiveOptionsControl);
}

/*
 * This function will set the Proof Of Possession (POP) for a request
 * associated with a key pair intended to do Key Encipherment.  Currently
 * this means encryption only keys.
 */
SSMStatus
SSMCRMFRequest_SetKeyEnciphermentPOP(CRMFCertReqMsg *certReqMsg,
                                     SSMCRMFRequest *curReq)
{
  SECItem       bitString;
  unsigned char der[2];
  SECStatus     rv;

  if (SSMCRMFRequest_HasEscrowAuthority(curReq)) { 
    /* For proof of possession on escrowed keys, we use the
     * this Message option of POPOPrivKey and include a zero
     * length bit string in the POP field.  This is OK because the encrypted
     * private key already exists as part of the PKIArchiveOptions
     * Control and that for all intents and purposes proves that
     * we do own the private key.
     */
    der[0] = 0x03; /*We've got a bit string          */
    der[1] = 0x00; /*We've got a 0 length bit string */
    bitString.data = der;
    bitString.len  = 2;
    rv = CRMF_CertReqMsgSetKeyEnciphermentPOP(certReqMsg, crmfThisMessage,
                                              crmfNoSubseqMess, &bitString);
  } else {
    /* If the encryption key is not being escrowed, then we set the 
     * Proof Of Possession to be a Challenge Response mechanism.
     */
    rv = CRMF_CertReqMsgSetKeyEnciphermentPOP(certReqMsg, 
                                              crmfSubsequentMessage,
                                              crmfChallengeResp, NULL); 
  }
  return (rv == SECSuccess) ? PR_SUCCESS : PR_FAILURE; 
}

/*
 * This function sets the Proof Of Possession(POP) for a requests.
 * All requests except for those associated with encryption only keys
 * sign the request as their form of POP.  Encryption only keys require
 * extra logic in determining the type of POP to use, that is why encryption
 * only keys have an extra layer for setting the POP instead of calling into
 * the CRMF libraries directly from within this function.
 */
SSMStatus
SSMCRMFRequest_SetProofOfPossession(CRMFCertReqMsg *certReqMsg, 
				    SSMCRMFRequest *curReq)
{
  SSMStatus  rv;
  SECStatus srv;

  switch (curReq->m_KeyGenType) {
  case rsaSign:
  case rsaDualUse:
  case rsaNonrepudiation:
  case rsaSignNonrepudiation:
  case dsaSign:
  case dsaNonrepudiation:
  case dsaSignNonrepudiation:
    srv = CRMF_CertReqMsgSetSignaturePOP(certReqMsg, 
                                         curReq->m_KeyPair->m_PrivKey, 
                                         curReq->m_KeyPair->m_PubKey, NULL,
                                         NULL, NULL);
    rv = (srv == SECSuccess) ? PR_SUCCESS : PR_FAILURE;
    break;
  case rsaEnc:
    rv = SSMCRMFRequest_SetKeyEnciphermentPOP(certReqMsg, curReq);
    break;
  case dhEx:
    /* This case may be supported in the future, but for now, we just fall 
     * though to the default case and return an error for diffie-hellman keys.
     */
  default:
    rv = PR_FAILURE;
    break;
  }
  return rv;
}

SSMStatus
SSM_EncodeCRMFRequests(SSMControlConnection * ctrl,SECItem *msg, 
                       char **destDER, SSMPRUint32 *destLen)
{
  PRInt32               i;
  SSMStatus              rv;
  SECStatus             srv;
  CRMFCertReqMsg      **certReqMsgs = NULL;
  SSMCRMFRequest       *curReq;
  SSMResource          *res;
  SECItem              *encodedReq;
  CRMFCertReqMessages   messages;
  EncodeCRMFReqRequest request;

  if (CMT_DecodeMessage(EncodeCRMFReqRequestTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
      goto loser;
  }

  certReqMsgs = SSM_ZNEW_ARRAY(CRMFCertReqMsg*, (request.numRequests+1));
  for (i=0; i<request.numRequests; i++) {
    rv = SSMControlConnection_GetResource(ctrl, (SSMResourceID)request.reqIDs[i],
                                          &res);
    if (rv != PR_SUCCESS) {
      goto loser;
    }
    if (!SSM_IsAKindOf(res, SSM_RESTYPE_CRMF_REQUEST)) {
      rv = PR_FAILURE;
      goto loser;
    }
    curReq = (SSMCRMFRequest*)res;
    certReqMsgs[i] = CRMF_CreateCertReqMsg();
    srv = CRMF_CertReqMsgSetCertRequest(certReqMsgs[i], curReq->m_CRMFRequest);
    if (srv != SECSuccess) {
      rv = PR_FAILURE;
      goto loser;
    }
    rv = SSMCRMFRequest_SetProofOfPossession(certReqMsgs[i], curReq);
    if (rv != PR_SUCCESS) {
      goto loser;
    }
    
  }
  /* Now we're ready to encode */
  messages.messages = certReqMsgs;
  encodedReq = SEC_ASN1EncodeItem(NULL, NULL, &messages,
                                  CRMFCertReqMessagesTemplate);
  if (encodedReq == NULL) {
    rv = PR_FAILURE;
    goto loser;
  }
  *destDER = BTOA_DataToAscii(encodedReq->data, encodedReq->len);
  *destLen = (*destDER == NULL) ? 0 : strlen(*destDER) + 1;
  SECITEM_FreeItem(encodedReq, PR_TRUE);
  rv = (*destDER == NULL) ? PR_FAILURE : PR_SUCCESS;
 loser:
  /* Do some massive memory clean-up in here. */
  if (certReqMsgs != NULL) {
    for (i=0; certReqMsgs[i] != NULL; i++){
      CRMF_DestroyCertReqMsg(certReqMsgs[i]);
    }
    PR_Free(certReqMsgs);
  }
  return rv;
}

void
ssm_processcmmfresponse_thread(void *arg)
{
  CMMFResponseArg *inArg = (CMMFResponseArg*)arg;
  SECItem *msg = inArg->msg;
  SSMControlConnection *connection = inArg->connection;
  SECStatus    srv;
  SSMStatus    rv;
  char        *nickname=NULL;
  int          numResponses, i;
  SECItem      cmmfDer = {siBuffer, NULL, 0};
  PRArenaPool *ourPool  = NULL;
  SECItem     *derCerts = NULL;
  SSMPKCS12CreateArg p12Create;
  SSMResourceID      p12RID;
  PRBool             freeLocalNickname = PR_FALSE;

  SSMPKCS12Context   *p12Cxt         = NULL;
  CERTCertList       *caPubs         = NULL;
  CERTCertificate    *currCert       = NULL;
  CERTCertificate    *dbCert         = NULL;
  CMMFCertRepContent *certRepContent = NULL;
  CMMFCertResponse   *currResponse   = NULL;
  PK11SlotInfo       *slot           = NULL;
  CERTCertificate   **certArr        = NULL;
  CMMFPKIStatus       reqStatus;
  PRBool              certAlreadyExisted = PR_FALSE;
  CMMFCertResponseRequest request;

  if (CMT_DecodeMessage(CMMFCertResponseRequestTemplate, &request, 
                        (CMTItem*)msg) != CMTSuccess) {
      goto loser;
  }

  srv = ATOB_ConvertAsciiToItem(&cmmfDer, request.base64Der);
  if (srv != SECSuccess || cmmfDer.data == NULL) {
      goto loser;
  }
  certRepContent = CMMF_CreateCertRepContentFromDER(connection->m_certdb,
                                                    (const char*)cmmfDer.data,
                                                    cmmfDer.len);
  if (certRepContent == NULL) {
      /* This could be a key recovery response, but fail for now. */
      goto loser;
  }
  numResponses = CMMF_CertRepContentGetNumResponses(certRepContent);
  if (request.doBackup) {
      certArr = SSM_ZNEW_ARRAY(CERTCertificate*, numResponses);
      if (certArr == NULL) {
          /* Let's not do back up in this case. */
          request.doBackup = (CMBool) PR_FALSE;
      }
  }
  for (i=0; i<numResponses; i++) {
      currResponse = CMMF_CertRepContentGetResponseAtIndex(certRepContent, i);
      if (currResponse == NULL) {
          goto loser;
      }
      /* Need to make sure the ID corresponds to a request Cartman actually
       * made.  Figure out how you're gonna do this.
       */
      reqStatus = CMMF_CertResponseGetPKIStatusInfoStatus(currResponse);
      if (!(reqStatus == cmmfGranted || reqStatus == cmmfGrantedWithMods)) {
          /* The CA didn't give us the cert as we requested. */
          goto loser;
      }
      currCert = CMMF_CertResponseGetCertificate(currResponse, 
						 connection->m_certdb);
      if (currCert == NULL) {
          goto loser;
      }
      if (!certificate_conflict(connection, &currCert->derCert,
                               connection->m_certdb, !certAlreadyExisted)) {
          /* This certificate already exists in the permanent
           * database.  Let's not add it and continue on 
           * processing responses.
           */
          if (request.doBackup) {
              certArr[i] = currCert;
          } else {
              CERT_DestroyCertificate(currCert);
          }
          CMMF_DestroyCertResponse(currResponse);
          certAlreadyExisted = PR_TRUE;
          continue;
      }
      
      if (currCert->subjectList && currCert->subjectList->entry && 
          currCert->subjectList->entry->nickname) {
          nickname = currCert->subjectList->entry->nickname;
      } else if (request.nickname == NULL || request.nickname[0] == '\0') {
          nickname = default_nickname(currCert, connection);
          freeLocalNickname = PR_TRUE; 
      } else {
          nickname = request.nickname;
      }
      slot = PK11_ImportCertForKey(currCert, nickname, connection);
      if (freeLocalNickname) {
          PR_Free(nickname);
          freeLocalNickname = PR_FALSE;
      }
      if (slot == NULL) {
          goto loser;
      }
      SSM_UseAsDefaultEmailIfNoneSet(connection, currCert, PR_TRUE);
      if (request.doBackup) {
          certArr[i] = currCert;
      } else {
          CERT_DestroyCertificate(currCert);
      }
      PK11_FreeSlot(slot);
      CMMF_DestroyCertResponse(currResponse);
  }
  caPubs = CMMF_CertRepContentGetCAPubs(certRepContent);
  if (caPubs != NULL) {
      CERTCertListNode *node;

      /* We got some CA certs to install in the database */
      ourPool = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
      if (ourPool == NULL) {
          goto loser;
      }
      derCerts = PORT_ArenaNewArray(ourPool, SECItem, 
                                    SSM_CertListCount(caPubs));
      if (derCerts == NULL) {
          goto loser;
      }
      for (node = CERT_LIST_HEAD(caPubs), i=0; 
           !CERT_LIST_END(node, caPubs);
           node = CERT_LIST_NEXT(node), i++) {
          srv = SECITEM_CopyItem(ourPool, &derCerts[i], &node->cert->derCert);
          if (srv != SECSuccess) {
              goto loser;
          }
      }
      srv = CERT_ImportCAChain(derCerts, SSM_CertListCount(caPubs),
                               certUsageUserCertImport);
      if (srv != SECSuccess) {
          goto loser;
      }
      PORT_FreeArena(ourPool, PR_FALSE);
  }
  CMMF_DestroyCertRepContent(certRepContent);
  if (request.doBackup) {
      p12Create.isExportContext = PR_TRUE;
      rv = SSM_CreateResource(SSM_RESTYPE_PKCS12_CONTEXT, 
                              (void*)&p12Create,
                              connection, &p12RID,
                              (SSMResource**)(&p12Cxt));
      p12Cxt->arg = SSM_ZNEW(SSMPKCS12BackupThreadArg);
      if (p12Cxt->arg == NULL) {
          goto loser;
      }
      p12Cxt->arg->certs = certArr;
      p12Cxt->arg->numCerts = numResponses;
      p12Cxt->super.m_clientContext = request.clientContext;
      p12Cxt->m_thread = SSM_CreateThread(&p12Cxt->super,
                                  SSMPKCS12Context_BackupMultipleCertsThread);
                                          

  }
  PR_Free(request.base64Der);
  PR_Free(request.nickname);
  PR_FREEIF(msg->data);
  msg->data = NULL;
  msg->len = 0;
  msg->type = (SECItemType)(SSM_REPLY_OK_MESSAGE | SSM_CRMF_ACTION | 
                            SSM_PROCESS_CMMF_RESP);
  if (ssmcontrolconnection_send_message_to_client(connection, msg) 
      != SSM_SUCCESS){
      goto loser;
  }
  SSM_FreeResource(&connection->super.super);
  SECITEM_FreeItem(msg, PR_TRUE);
  return;
 loser:
  if (connection != NULL) {
      SSM_FreeResource(&connection->super.super);
  }
  if (ourPool != NULL) {
      PORT_FreeArena(ourPool, PR_FALSE);
  }
  if (certRepContent != NULL) {
      CMMF_DestroyCertRepContent(certRepContent);
  }
  if (request.base64Der != NULL) {
      PR_Free(request.base64Der);
  }
  if (request.nickname != NULL) {
      PR_Free(request.nickname);
  }
  rv = ssmcontrolconnection_encode_err_reply(msg, SSM_FAILURE);
  PR_ASSERT(rv == SSM_SUCCESS);
  rv = ssmcontrolconnection_send_message_to_client(connection, msg);
  PR_ASSERT(rv == SSM_SUCCESS);
}

SSMStatus 
SSM_ProcessCMMFCertResponse(SECItem *msg, SSMControlConnection *connection)
{
    CMMFResponseArg *arg=NULL;

    arg = SSM_NEW(CMMFResponseArg);
    if (arg == NULL) {
        goto loser;
    }
    arg->msg = SECITEM_DupItem(msg);
    arg->connection = connection;
    SSM_GetResourceReference(&connection->super.super);
    if (SSM_CreateAndRegisterThread(PR_USER_THREAD, 
                                    ssm_processcmmfresponse_thread,
                                    (void *)arg, PR_PRIORITY_NORMAL, 
                                    PR_LOCAL_THREAD,
                                    PR_UNJOINABLE_THREAD, 0) == NULL) {
        SSM_FreeResource(&connection->super.super);
        goto loser;
    }
    return SSM_ERR_DEFER_RESPONSE;    
 loser:
    PR_FREEIF(arg);
    return SSM_FAILURE;
}

SECKEYPrivateKey*
SSM_FindPrivKeyFromKeyID(SECItem *keyID, SECMODModule *currMod,
                         SSMControlConnection *ctrl)
{
    PK11SlotInfo     *currSlot;
    SECKEYPrivateKey *privKey = NULL;
    int               i;

    for (i=0; i<currMod->slotCount; i++) {
        currSlot = currMod->slots[i];
        privKey = PK11_FindKeyByKeyID(currSlot, keyID, (void*)ctrl);
        if (privKey != NULL) {
            break;
        }
    }
    return privKey;
}

SECKEYPrivateKey*
SSM_FindPrivKeyFromPubValue(SECItem *publicValue, SECMODModuleList *modList,
                            SSMControlConnection *ctrl)
{
    SECItem          *keyID = NULL;
    SECMODModuleList *currMod;
    SECKEYPrivateKey *privKey = NULL;

    keyID = PK11_MakeIDFromPubKey(publicValue);
    if (keyID == NULL) {
        return NULL;
    }
    /* Iterate through the slots looking for the slot where the private 
     * key lives.
     */
    currMod = modList;
    while (currMod != NULL && currMod->module != NULL) {
        privKey = SSM_FindPrivKeyFromKeyID(keyID, currMod->module, ctrl);
        if (privKey != NULL) {
            break;
        }
        currMod = currMod->next;
    }
    SECITEM_FreeItem(keyID, PR_TRUE);
    return privKey;
}

SSMStatus
SSM_InitCRMFASN1Arg(CRMFASN1Arg *arg)
{
    arg->buffer.data = SSM_NEW_ARRAY(unsigned char, DEFAULT_ASN1_CHUNK_SIZE);
    if (arg->buffer.data == NULL) {
        return PR_FAILURE;
    }
    arg->buffer.len   = 0;
    arg->allocatedLen = DEFAULT_ASN1_CHUNK_SIZE;
    return PR_FAILURE;
}

void
SSM_GenericASN1Callback(void *arg, const char *buf, unsigned long len)
{
    CRMFASN1Arg   *encoderArg = (CRMFASN1Arg*)arg;
    unsigned char *cursor;
    
    if ((encoderArg->buffer.len + len) > encoderArg->allocatedLen) {
        int newSize = encoderArg->buffer.len + DEFAULT_ASN1_CHUNK_SIZE;
        void *dummy = PR_Realloc(encoderArg->buffer.data, newSize);
        if (dummy == NULL) {
            PR_ASSERT(0);
            return;
        }
        encoderArg->buffer.data  = (unsigned char *) dummy;
        encoderArg->allocatedLen = newSize;
    }
    cursor = &(encoderArg->buffer.data[encoderArg->buffer.len]);
    memcpy(cursor, buf, len);
    encoderArg->buffer.len += len;
}

SSMStatus
SSM_RespondToPOPChallenge(SECItem                *msg, 
                          SSMControlConnection   *ctrl,
                          char                  **challengeResponse, 
                          PRUint32               *responseLen)
{
    SSMStatus   rv;
    SECStatus  srv;
    SECItem    challengeDER = {siBuffer, NULL, 0}, *publicValue=NULL;
    int        i, numChallenges;
    long      *decryptedChallenges=NULL;

    CMMFPOPODecKeyChallContent *challContent = NULL;
    SECMODModuleList           *modList = NULL;
    SECKEYPrivateKey           *privKey = NULL;
    CRMFASN1Arg                 asn1Arg = {{(SECItemType) 0, NULL, 0}, 0};
    SingleStringMessage request;

    if (CMT_DecodeMessage(SingleStringMessageTemplate, &request, (CMTItem*)msg) != CMTSuccess) {
        goto loser;
    }

    srv = ATOB_ConvertAsciiToItem(&challengeDER, request.string);
    if (srv != SECSuccess) {
        goto loser;
    }
    challContent = CMMF_CreatePOPODecKeyChallContentFromDER((char *) challengeDER.data,
                                                            (unsigned int) challengeDER.len);
    if (challContent == NULL) {
        goto loser;
    }
    numChallenges = CMMF_POPODecKeyChallContentGetNumChallenges(challContent);
    if (numChallenges <= 0) {
        /* There weren't any challenges in the challenge string. */
        goto loser;
    }
    modList = SECMOD_GetDefaultModuleList();
    if (modList == NULL) {
        goto loser;
    }
    decryptedChallenges = SSM_ZNEW_ARRAY(long, (numChallenges+1));
    if (decryptedChallenges == NULL) {
        goto loser;
    }
    for (i=0; i<numChallenges; i++) {
        publicValue = 
            CMMF_POPODecKeyChallContentGetPublicValue(challContent, i);
        if (publicValue == NULL) {
            goto loser;
        }
        privKey = SSM_FindPrivKeyFromPubValue(publicValue, modList, ctrl);
        if (privKey == NULL) {
            goto loser;
        }
        SECITEM_FreeItem(publicValue, PR_TRUE);
        publicValue = NULL;
        srv = CMMF_POPODecKeyChallContDecryptChallenge(challContent, i,
                                                       privKey);
        if (srv != SECSuccess) {
            goto loser;
        }

        srv = CMMF_POPODecKeyChallContentGetRandomNumber(challContent, i,
                                                     &decryptedChallenges[i]);
        if (srv != SECSuccess) {
            goto loser;
        }
    }
    modList = NULL;
    CMMF_DestroyPOPODecKeyChallContent(challContent);
    challContent = NULL;
    rv = SSM_InitCRMFASN1Arg(&asn1Arg);
    srv = CMMF_EncodePOPODecKeyRespContent(decryptedChallenges, numChallenges,
                                           SSM_GenericASN1Callback, &asn1Arg);
    if (srv != SECSuccess) {
        goto loser;
    }
    *challengeResponse = BTOA_DataToAscii(asn1Arg.buffer.data, 
                                          asn1Arg.buffer.len);
    PR_Free(asn1Arg.buffer.data);
    *responseLen = (*challengeResponse == NULL) ? 0 : 
                                                  strlen(*challengeResponse)+1;
    PR_Free(decryptedChallenges);
    PR_Free(request.string);
    return PR_SUCCESS;
 loser:
    if (asn1Arg.buffer.data != NULL) {
        PR_Free(asn1Arg.buffer.data);
    }
    if (decryptedChallenges != NULL) {
        PR_Free(decryptedChallenges);
    }
    if (request.string != NULL) {
        PR_Free(request.string);
    }
    if (challContent != NULL) {
        CMMF_DestroyPOPODecKeyChallContent(challContent);
    }
    if (publicValue != NULL) {
        SECITEM_FreeItem(publicValue, PR_TRUE);
    }
    if (modList != NULL) {
        SECMOD_DestroyModuleList(modList);
    }
    return PR_FAILURE;
}

void SSM_CRMFEncodeThread(void *arg)
{
   SSMCRMFThreadArg     *encArg = (SSMCRMFThreadArg*)arg; 
   SSMControlConnection *ctrl   = encArg->ctrl;
   SECItem              *msg    = encArg->msg;
   char                 *encodedReq = NULL;
   PRUint32              derLen;
   SingleItemMessage reply;
   SSMStatus rv;

   SSM_RegisterThread("CRMF Encode", NULL);
   SSM_DEBUG("Encoding CRMF request(s)\n");
   rv = SSM_EncodeCRMFRequests(ctrl, msg, &encodedReq, &derLen);
   if (rv != PR_SUCCESS) {
       goto loser;
   }
   msg->data = NULL;
   msg->len  = 0;
   msg->type = (SECItemType) (SSM_REPLY_OK_MESSAGE | 
                              SSM_CRMF_ACTION      |
                              SSM_DER_ENCODE_REQ);
   reply.item.len = derLen;
   reply.item.data = (unsigned char *) encodedReq;
   if (CMT_EncodeMessage(SingleItemMessageTemplate, 
                         (CMTItem*)msg, &reply) != CMTSuccess) {
       goto loser;
   }
   SSM_DEBUG("Created the following request: \n");
   SSM_DEBUG("\n%s\n",encodedReq);
   PR_Free(encodedReq);
   SSM_DEBUG("queueing reply: type %lx, len %ld.\n", msg->type, msg->len);
   SSM_SendQMessage(ctrl->m_controlOutQ,
                    SSM_PRIORITY_NORMAL,
                    msg->type, msg->len,
                    (char *)msg->data, PR_TRUE);
   goto done;
 loser:
  {
    SingleNumMessage reply;

    msg->type = (SECItemType) SSM_REPLY_ERR_MESSAGE;
    reply.value = rv;
    CMT_EncodeMessage(SingleNumMessageTemplate, (CMTItem*)msg, &reply);
  }
  SSM_DEBUG("queueing reply: type %lx, len %ld.\n", msg->type, msg->len);
  SSM_SendQMessage(ctrl->m_controlOutQ,
                   SSM_PRIORITY_NORMAL,
                   msg->type, msg->len,
                   (char *)msg->data, PR_TRUE);
 done:
  SSM_FreeResource(&ctrl->super.super);
  SECITEM_FreeItem(msg, PR_TRUE);
  PR_Free(arg);
}
