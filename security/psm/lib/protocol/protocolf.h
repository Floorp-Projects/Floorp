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
#ifndef __PROTOCOLF_H__
#define __PROTOCOLF_H__
/*************************************************************************
 * For each type of message, parse and pack function is provided. 
 *
 * Parse functions accept a ptr to the "blob" of data received from the 
 * network and fill in fields of the message, numbers in host-order, strings
 * as C-style NULL-terminated strings. Return SSMPRStatus. 
 *
 * Pack functions take all the info to construct a message and fill in a 
 * ptr to the "blob" of data to be sent. Return length of the data blob, or 
 * a zero in case of an error
 *
 * All functions set NSPR errors when necessary.
 ************************************************************************/
#include "protocol.h"
#include "cert.h"

SSMPRStatus SSM_ParseHelloRequest(void * helloRequest, 
				  SSMPRUint32 * version, 
				  PRBool * doesUI,
				  PRInt32 * policyType,
				  SSMPRUint32 * profileLen,
				  char ** profile);
SSMPRInt32 SSM_PackHelloReply(void ** helloReply, SSMPRInt32 result, 
			      SSMPRUint32 sessionID, SSMPRUint32 version, 
			      SSMPRUint32 httpPort, SSMPRUint32 nonceLen, 
			      char * nonce, SSMPolicyType policy);

/* Parse data connections requests */
SSMPRStatus SSM_ParseSSLDataConnectionRequest(void *sslRequest, 
					      SSMPRUint32 * flags, 
					      SSMPRUint32 * port,
					      SSMPRUint32 * hostIPLen,
					      char ** hostIP, 
					      SSMPRUint32 * hostNameLen,
					      char ** hostName);
SSMPRStatus SSM_ParseHashStreamRequest(void * hashStreamRequest,
                                       SSMPRUint32 * type);
SSMPRStatus SSM_ParseP7EncodeConnectionRequest(void *request,
					       SSMPRUint32 *ciRID);
/* Messages to initiate PKCS7 data connection */
/* PKCS7DecodeRequest message has no data */ 

/* Single data connection reply */
SSMPRInt32 SSM_PackDataConnectionReply(void ** sslReply, 
				       SSMPRInt32 result, 
				       SSMPRUint32 connID, 
				       SSMPRUint32 port);

SSMPRStatus SSM_ParseSSLSocketStatusRequest(void * statusRequest, 
					    SSMPRUint32 * connID);
SSMPRInt32 SSM_PackSSLSocketStatusReply(void ** statusReply, 
					SSMPRInt32 result, 
					SSMPRUint32 resourceID);

/* 
 * UI event is an asynchroneous message sent from PSM server to the client
 * NOTE: (context) is the actual context pointer, it is NOT a ptr-to-ptr.
 * The value of (context) is copied into the packet.
 */
SSMPRInt32 SSM_PackUIEvent(void ** eventRequest, SSMPRUint32 resourceID, 
			   SSMPRUint32 width, SSMPRUint32 height,
			   SSMPRUint32 urlLen, char * url);

SSMPRInt32 SSM_PackTaskCompletedEvent(void **event, SSMPRUint32 resourceID,
                                     SSMPRUint32 numTasks, SSMPRUint32 result);

/* Verify raw signature */

SSMPRStatus SSM_ParseVerifyRawSigRequest(void * verifyRawSigRequest, 
					 SSMPRUint32 * algorithmID,
					 SSMPRUint32 * paramsLen, 
					 unsigned char ** params,
					 SSMPRUint32 * pubKeyLen,
					 unsigned char ** pubKey,
					 SSMPRUint32 * hashLen,
					 unsigned char ** hash,
					 SSMPRUint32 * signatureLen,
					 unsigned char ** signature);
SSMPRInt32 SSM_PackVerifyRawSigReply(void ** verifyRawSigReply, 
				     SSMPRInt32 result);

/* Verify detached signature */
SSMPRStatus SSM_ParseVerifyDetachedSigRequest(void * request, 
											  SSMPRInt32 * pkcs7ContentID,
											  SSMPRInt32 * certUsage,
											  SSMPRInt32 * hashAlgID,
											  SSMPRUint32 * keepCert,
											  SSMPRUint32 * digestLen,
											  unsigned char ** hash);

SSMPRInt32 SSM_PackVerifyDetachedSigReply(void ** verifyDetachedSigReply, 
										  SSMPRInt32 result);

/* PKCS#7 functions */
SSMPRStatus SSM_ParseCreateSignedRequest(void *request,
                                         SSMPRInt32 *scertRID,
                                         SSMPRInt32 *ecertRID,
                                         SSMPRUint32 *dig_alg,
                                         SECItem **digest);

SSMPRInt32 SSM_PackCreateSignedReply(void **reply, SSMPRInt32 ciRID,
                                     SSMPRUint32 result);

SSMPRStatus SSM_ParseCreateEncryptedRequest(void *request,
					    SSMPRInt32 *scertRID,
                                            SSMPRInt32 *nrcerts,
					    SSMPRInt32 **rcertRIDs);

SSMPRInt32 SSM_PackCreateEncryptedReply(void **reply, SSMPRInt32 ciRID,
					SSMPRUint32 result);

/* Resource functions */
SSMPRStatus SSM_ParseCreateResourceRequest(void *request,
                                           SSMPRUint32 *type,
                                           unsigned char **params,
                                           SSMPRUint32 *paramLen);

SSMPRStatus SSM_PackCreateResourceReply(void **reply, SSMPRStatus rv,
                                        SSMPRUint32 resID);

SSMPRStatus SSM_ParseGetAttribRequest(void * getAttribRequest, 
				      SSMPRUint32 * resourceID, 
				      SSMPRUint32 * fieldID);

void SSM_DestroyAttrValue(SSMAttributeValue *value, PRBool freeit);

SSMPRInt32 SSM_PackGetAttribReply(void **getAttribReply,
				  SSMPRInt32 result,
				  SSMAttributeValue *value);
SSMPRStatus SSM_ParseSetAttribRequest(SECItem *msg,
				      SSMPRInt32 *resourceID,
				      SSMPRInt32 *fieldID,
				      SSMAttributeValue *value);
/* Currently, there is no need for a pack version.  There is nothing to send
 * back except for the notice that the operation was successful.
 */
				  
				       
				       
/* Pickle and unpickle resources. */
SSMPRStatus SSM_ParsePickleResourceRequest(void * pickleResourceRequest, 
					  SSMPRUint32 * resourceID);
SSMPRInt32 SSM_PackPickleResourceReply(void ** pickleResourceReply, 
				       SSMPRInt32 result,
				       SSMPRUint32 resourceLen,
				       void * resource);
SSMPRStatus SSM_ParseUnpickleResourceRequest(void * unpickleResourceRequest,
					     SSMPRUint32 blobSize,
					     SSMPRUint32 * resourceType,
					     SSMPRUint32 * resourceLen,
					     void ** resource);
SSMPRInt32 SSM_PackUnpickleResourceReply(void ** unpickleResourceReply, 
					   SSMPRInt32 result, 
					   SSMPRUint32 resourceID);

/* Destroy resource */
SSMPRStatus SSM_ParseDestroyResourceRequest(void * destroyResourceRequest, 
					    SSMPRUint32 * resourceID, 
					    SSMPRUint32 * resourceType);
SSMPRInt32 SSM_PackDestroyResourceReply(void ** destroyResourceReply, 
                                        SSMPRInt32 result);

/* Duplicate resource */
SSMPRStatus SSM_ParseDuplicateResourceRequest(void * request, 
					      SSMPRUint32 * resourceID);
SSMPRInt32 SSM_PackDuplicateResourceReply(void ** reply, SSMPRInt32 result,
					  SSMPRUint32 resID);

/* Cert actions */
typedef struct MatchUserCertRequestData {
    PRUint32 certType;
    PRInt32 numCANames;
    char ** caNames;
} MatchUserCertRequestData;

typedef struct SSMCertList {
    PRCList certs;
    PRInt32 count;
} SSMCertList;

typedef struct SSMCertListElement {
    PRCList links;
    PRUint32 certResID;
} SSMCertListElement;

#define SSM_CERT_LIST_ELEMENT_PTR(_q) (SSMCertListElement*)(_q);

SSMPRStatus SSM_ParseVerifyCertRequest(void * verifyCertRequest, 
				       SSMPRUint32 * resourceID, 
				       SSMPRInt32 * certUsage);
SSMPRInt32 SSM_PackVerifyCertReply(void ** verifyCertReply, 
			           SSMPRInt32 result);

SSMPRStatus SSM_ParseImportCertRequest(void * importCertRequest, 
				       SSMPRUint32 * blobLen, 
				       void ** certBlob);
SSMPRInt32 SSM_PackImportCertReply(void ** importCertReply, SSMPRInt32 result, 
				   SSMPRUint32 resourceID);
PRStatus SSM_ParseFindCertByNicknameRequest(void *request, char ** nickname);
PRInt32 SSM_PackFindCertByNicknameReply(void ** reply, PRUint32 resourceID);
PRStatus SSM_ParseFindCertByKeyRequest(void *request, SECItem ** key);
PRInt32 SSM_PackFindCertByKeyReply(void ** reply, PRUint32 resourceID);
PRStatus SSM_ParseFindCertByEmailAddrRequest(void *request, char ** emailAddr);
PRInt32 SSM_PackFindCertByEmailAddrReply(void ** reply, PRUint32 resourceID);
PRStatus SSM_ParseAddTempCertToDBRequest(void *request, PRUint32 *resourceID, char ** nickname, PRInt32 *ssl, PRInt32 *email, PRInt32 *objectSigning);
PRInt32 SSM_PackAddTempCertToDBReply(void ** reply);
PRStatus SSM_ParseMatchUserCertRequest(void *request, MatchUserCertRequestData** data);
PRInt32 SSM_PackMatchUserCertReply(void **reply, SSMCertList * certList);

SSMPRInt32 SSM_PackErrorMessage(void ** errorReply, SSMPRInt32 result);


/* PKCS11 actions */
SSMPRStatus SSM_ParseKeyPairGenRequest(void *keyPairGenRequest, 
				       SSMPRInt32 requestLen,
				       SSMPRUint32 *keyPairCtxtID,
				       SSMPRUint32 *genMechanism,
				       SSMPRUint32 *keySize,
				       unsigned char **params, 
				       SSMPRUint32 *paramLen);

SSMPRInt32 SSM_PackKeyPairGenResponse(void ** keyPairGenResponse, 
				      SSMPRUint32 keyPairId);

PRStatus
SSM_ParseFinishKeyGenRequest(void    *finishKeyGenRequest,
                             PRInt32  requestLen,
                             PRInt32 *keyGenContext);

/* CMMF/CRMF Actions */
SSMPRStatus SSM_ParseCreateCRMFReqRequest(void        *crmfReqRequest,
					  SSMPRInt32   requestLen,
					  SSMPRUint32 *keyPairId);

SSMPRInt32 SSM_PackCreateCRMFReqReply(void        **crmfReqReply,
				      SSMPRUint32   crmfReqId);

SSMPRStatus SSM_ParseEncodeCRMFReqRequest(void         *encodeReq,
					  SSMPRInt32    requestLen,
					  SSMPRUint32 **crmfReqId,
					  SSMPRInt32   *numRequests);

SSMPRInt32 SSM_PackEncodeCRMFReqReply(void        **encodeReply,
				      char         *crmfDER,
				      SSMPRUint32   derLen);

SSMPRStatus SSM_ParseCMMFCertResponse(void        *encodedRes,
				      SSMPRInt32   encodeLen,
				      char       **nickname,
				      char       **base64Der,
				      PRBool      *doBackup);

PRStatus SSM_ParsePOPChallengeRequest(void     *challenge,
				      PRInt32   len,
				      char    **responseString);
PRInt32 SSM_PackPOPChallengeResponse(void   **response,
				     char    *responseString,
				     PRInt32  responseStringLen);

PRInt32 SSM_PackPasswdRequest(void ** passwdRequest, PRInt32 tokenID,
                              char * prompt, PRInt32 promptLen);
PRStatus SSM_ParsePasswordReply(void * passwdReply, PRInt32 * result, 
                                PRInt32 * tokenID,
                                char ** passwd, PRInt32 * passwdLen);

/* Sign Text Actions */
typedef struct {
    char *stringToSign;
    char *hostName;
    char *caOption;
    PRInt32 numCAs;
    char **caNames;
} signTextRequestData;

PRStatus SSM_ParseSignTextRequest(void* signTextRequest, PRInt32 len, PRUint32* resID, signTextRequestData ** data);

PRStatus SSM_ParseGetLocalizedTextRequest(void               *data,
                                          SSMLocalizedString *whichString);  

PRInt32 SSM_PackGetLocalizedTextResponse(void               **data,
					 SSMLocalizedString   whichString,
					 char                *retString);

PRStatus SSM_ParseAddNewSecurityModuleRequest(void          *data, 
					      char         **moduleName,
					      char         **libraryPath, 
					      unsigned long *pubMechFlags,
					      unsigned long *pubCipherFlags);

PRInt32 SSM_PackAddNewModuleResponse(void **data, PRInt32 rv);

PRStatus SSM_ParseDeleteSecurityModuleRequest(void *data, char **moduleName);

PRInt32 SSM_PackDeleteModuleResponse(void **data, PRInt32 moduleType);

PRInt32 SSM_PackFilePathRequest(void **data, PRInt32 resID, char *prompt,
				PRBool shouldFileExist, char *fileSuffix);

PRStatus SSM_ParseFilePathReply(void *message, char **filePath,
				PRInt32 *rid);

PRInt32 SSM_PackPromptRequestEvent(void **data, PRInt32 resID, char *prompt);
PRStatus SSM_ParsePasswordPromptReply(void *data, PRInt32 *resID, 
				      char **reply);

/* messages for importing certs *the traditional way* */
PRInt32 SSM_PackDecodeCertReply(void ** data, PRInt32 certID);
PRStatus SSM_ParseDecodeCertRequest(void * data, PRInt32 * len, 
					char ** buffer);
PRStatus SSM_ParseGetKeyChoiceListRequest(void * data, PRUint32 dataLen, 
					  char ** type, PRUint32 *typeLen, 
					  char ** pqgString, PRUint32 *pqgLen);
PRInt32 SSM_PackGetKeyChoiceListReply(void **data, char ** list);

PRStatus SSM_ParseGenKeyOldStyleRequest(void * data, PRUint32 datalen, 
					char ** choiceString, 
					char ** challenge, 
					char ** typeString, 
					char ** pqgString);
PRInt32 SSM_PackGenKeyOldStyleReply(void ** data, char * keydata);

PRStatus SSM_ParseDecodeAndCreateTempCertRequest(void * data, 
			char ** certbuf, PRUint32 * certlen, int * certClass);

#endif /*PROTOCOLF_H_*/
