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
#ifndef __MESSAGES_H__
#define __MESSAGES_H__

#include "newproto.h"

typedef struct SingleNumMessage {
  CMInt32 value;
} SingleNumMessage;

extern CMTMessageTemplate SingleNumMessageTemplate[];

typedef struct SingleStringMessage {
  char *string;
} SingleStringMessage;

extern CMTMessageTemplate SingleStringMessageTemplate[];

typedef struct SingleItemMessage {
  CMTItem item;
} SingleItemMessage;

extern CMTMessageTemplate SingleItemMessageTemplate[];

typedef struct HelloRequest {
  CMInt32 version;
  CMInt32 policy;
  CMBool doesUI;
  char *profile;
  char* profileDir;
} HelloRequest;

extern CMTMessageTemplate HelloRequestTemplate[];

typedef struct HelloReply {
  CMInt32 result;
  CMInt32 sessionID;
  CMInt32 version;
  CMInt32 httpPort;
  CMInt32 policy;
  CMTItem nonce;
  char *stringVersion;
} HelloReply;

extern CMTMessageTemplate HelloReplyTemplate[];

typedef struct SSLDataConnectionRequest {
  CMInt32 flags;
  CMInt32 port;
  char *hostIP;
  char *hostName;
  CMBool forceHandshake;
  CMTItem clientContext;
} SSLDataConnectionRequest;

extern CMTMessageTemplate SSLDataConnectionRequestTemplate[];

typedef struct TLSDataConnectionRequest {
    CMInt32 port;
    char* hostIP;
    char* hostName;
} TLSDataConnectionRequest;

extern CMTMessageTemplate TLSDataConnectionRequestTemplate[];

typedef struct TLSStepUpRequest {
    CMUint32 connID;
    CMTItem clientContext;
} TLSStepUpRequest;

extern CMTMessageTemplate TLSStepUpRequestTemplate[];

typedef struct ProxyStepUpRequest {
    CMUint32 connID;
    CMTItem clientContext;
    char* url;
} ProxyStepUpRequest;

extern CMTMessageTemplate ProxyStepUpRequestTemplate[];

typedef struct PKCS7DataConnectionRequest {
    CMUint32 resID;
    CMTItem clientContext;
} PKCS7DataConnectionRequest;

extern CMTMessageTemplate PKCS7DataConnectionRequestTemplate[];

typedef struct DataConnectionReply {
  CMInt32 result;
  CMInt32 connID;
  CMInt32 port;
} DataConnectionReply;

extern CMTMessageTemplate DataConnectionReplyTemplate[];

typedef struct UIEvent {
  CMInt32 resourceID;
  CMInt32 width;
  CMInt32 height;
  CMBool isModal;
  char *url;
  CMTItem clientContext;
} UIEvent;

extern CMTMessageTemplate UIEventTemplate[];
extern CMTMessageTemplate OldUIEventTemplate[];

typedef struct TaskCompletedEvent {
  CMInt32 resourceID;
  CMInt32 numTasks;
  CMInt32 result;
} TaskCompletedEvent;

extern CMTMessageTemplate TaskCompletedEventTemplate[];

typedef struct VerifyDetachedSigRequest {
  CMInt32 pkcs7ContentID;
  CMInt32 certUsage;
  CMInt32 hashAlgID;
  CMBool keepCert;
  CMTItem hash;
} VerifyDetachedSigRequest;

extern CMTMessageTemplate VerifyDetachedSigRequestTemplate[];

typedef struct CreateSignedRequest {
  CMInt32 scertRID;
  CMInt32 ecertRID;
  CMInt32 dig_alg;
  CMTItem digest;
} CreateSignedRequest;

extern CMTMessageTemplate CreateSignedRequestTemplate[];

typedef struct CreateContentInfoReply {
  CMInt32 ciRID;
  CMInt32 result;
  CMInt32 errorCode;
} CreateContentInfoReply;

extern CMTMessageTemplate CreateContentInfoReplyTemplate[];

typedef struct CreateEncryptedRequest {
  CMInt32 scertRID;
  CMInt32 nrcerts;
  CMInt32 *rcertRIDs;
} CreateEncryptedRequest;

extern CMTMessageTemplate CreateEncryptedRequestTemplate[];

typedef struct CreateResourceRequest {
  CMInt32 type;
  CMTItem params;
} CreateResourceRequest;

extern CMTMessageTemplate CreateResourceRequestTemplate[];

typedef struct CreateResourceReply {
  CMInt32 result;
  CMInt32 resID;
} CreateResourceReply;

extern CMTMessageTemplate CreateResourceReplyTemplate[];

typedef struct GetAttribRequest {
  CMInt32 resID;
  CMInt32 fieldID;
} GetAttribRequest;

extern CMTMessageTemplate GetAttribRequestTemplate[];

typedef struct GetAttribReply {
  CMInt32 result;
  SSMAttributeValue value;
} GetAttribReply;

extern CMTMessageTemplate GetAttribReplyTemplate[];

typedef struct SetAttribRequest {
  CMInt32 resID;
  CMInt32 fieldID;
  SSMAttributeValue value;
} SetAttribRequest;

extern CMTMessageTemplate SetAttribRequestTemplate[];

typedef struct PickleResourceReply {
    CMInt32 result;
    CMTItem blob;
} PickleResourceReply;

extern CMTMessageTemplate PickleResourceReplyTemplate[];

typedef struct UnpickleResourceRequest {
  CMInt32 resourceType;
  CMTItem resourceData;
} UnpickleResourceRequest;

extern CMTMessageTemplate UnpickleResourceRequestTemplate[];

typedef struct UnpickleResourceReply {
    CMInt32 result;
    CMInt32 resID;
} UnpickleResourceReply;

extern CMTMessageTemplate UnpickleResourceReplyTemplate[];

typedef struct PickleSecurityStatusReply {
    CMInt32 result;
    CMInt32 securityLevel;
    CMTItem blob;
} PickleSecurityStatusReply;

extern CMTMessageTemplate PickleSecurityStatusReplyTemplate[];

typedef struct DupResourceReply {
    CMInt32 result;
    CMUint32 resID;
} DupResourceReply;

extern CMTMessageTemplate DupResourceReplyTemplate[];

typedef struct DestroyResourceRequest {
  CMInt32 resID;
  CMInt32 resType;
} DestroyResourceRequest;

extern CMTMessageTemplate DestroyResourceRequestTemplate[];

typedef struct VerifyCertRequest {
  CMInt32 resID;
  CMInt32 certUsage;
} VerifyCertRequest;

extern CMTMessageTemplate VerifyCertRequestTemplate[];

typedef struct AddTempCertToDBRequest {
  CMInt32 resID;
  char *nickname;
  CMInt32 sslFlags;
  CMInt32 emailFlags;
  CMInt32 objSignFlags;
} AddTempCertToDBRequest;

extern CMTMessageTemplate AddTempCertToDBRequestTemplate[];

typedef struct MatchUserCertRequest {
  CMInt32 certType;
  CMInt32 numCANames;
  char **caNames;
} MatchUserCertRequest;

extern CMTMessageTemplate MatchUserCertRequestTemplate[];

typedef struct MatchUserCertReply {
  CMInt32 numCerts;
  CMInt32 *certs;
} MatchUserCertReply;

extern CMTMessageTemplate MatchUserCertReplyTemplate[];

typedef struct EncodeCRMFReqRequest {
    CMInt32 numRequests;
    CMInt32 * reqIDs;
} EncodeCRMFReqRequest;

extern CMTMessageTemplate EncodeCRMFReqRequestTemplate[];

typedef struct CMMFCertResponseRequest {
    char *nickname;
    char *base64Der;
    CMBool doBackup;
    CMTItem clientContext;
} CMMFCertResponseRequest;

extern CMTMessageTemplate CMMFCertResponseRequestTemplate[];

typedef struct PasswordRequest {
    CMInt32 tokenKey;
    char *prompt;
    CMTItem clientContext;
} PasswordRequest;

extern CMTMessageTemplate PasswordRequestTemplate[];

typedef struct PasswordReply {
    CMInt32 result;
    CMInt32 tokenID;
    char * passwd;
} PasswordReply;

extern CMTMessageTemplate PasswordReplyTemplate[];

typedef struct KeyPairGenRequest {
    CMInt32 keyGenCtxtID;
    CMInt32 genMechanism;
    CMInt32 keySize;
    CMTItem params;
} KeyPairGenRequest;

extern CMTMessageTemplate KeyPairGenRequestTemplate[];

typedef struct DecodeAndCreateTempCertRequest {
    CMInt32 type;
    CMTItem cert;
} DecodeAndCreateTempCertRequest;

extern CMTMessageTemplate DecodeAndCreateTempCertRequestTemplate[];

typedef struct GenKeyOldStyleRequest {
    char *choiceString;
    char *challenge;
    char *typeString;
    char *pqgString;
} GenKeyOldStyleRequest;

extern CMTMessageTemplate GenKeyOldStyleRequestTemplate[];

typedef struct GenKeyOldStyleTokenRequest {
    CMInt32 rid;
    CMInt32 numtokens;
    char ** tokenNames;
} GenKeyOldStyleTokenRequest;

extern CMTMessageTemplate GenKeyOldStyleTokenRequestTemplate[];

typedef struct GenKeyOldStyleTokenReply {
    CMInt32 rid;
    CMBool cancel;
    char * tokenName;
} GenKeyOldStyleTokenReply;

extern CMTMessageTemplate GenKeyOldStyleTokenReplyTemplate[];

typedef struct GenKeyOldStylePasswordRequest {
    CMInt32 rid;
    char * tokenName;
    CMBool internal;
    CMInt32 minpwdlen;
    CMInt32 maxpwdlen;
} GenKeyOldStylePasswordRequest;

extern CMTMessageTemplate GenKeyOldStylePasswordRequestTemplate[];

typedef struct GenKeyOldStylePasswordReply {
    CMInt32 rid;
    CMBool cancel;
    char * password;
} GenKeyOldStylePasswordReply;

extern CMTMessageTemplate GenKeyOldStylePasswordReplyTemplate[];

typedef struct GetKeyChoiceListRequest {
    char *type;
    char *pqgString;
} GetKeyChoiceListRequest;

extern CMTMessageTemplate GetKeyChoiceListRequestTemplate[];

typedef struct GetKeyChoiceListReply {
    CMInt32 nchoices;
    char **choices;
} GetKeyChoiceListReply;

extern CMTMessageTemplate GetKeyChoiceListReplyTemplate[];

typedef struct AddNewSecurityModuleRequest {
    char *moduleName;
    char *libraryPath;
    CMInt32 pubMechFlags;
    CMInt32 pubCipherFlags;
} AddNewSecurityModuleRequest;

extern CMTMessageTemplate AddNewSecurityModuleRequestTemplate[];

typedef struct FilePathRequest {
    CMInt32 resID;
    char *prompt;
    CMBool getExistingFile;
    char *fileRegEx;
} FilePathRequest;

extern CMTMessageTemplate FilePathRequestTemplate[];

typedef struct FilePathReply {
    CMInt32 resID;
    char *filePath;
} FilePathReply;

extern CMTMessageTemplate FilePathReplyTemplate[];

typedef struct PasswordPromptReply {
    CMInt32 resID;
    char *promptReply;
} PasswordPromptReply;

extern CMTMessageTemplate PasswordPromptReplyTemplate[];

typedef struct SignTextRequest {
    CMInt32 resID;
    char *stringToSign;
    char *hostName;
    char *caOption;
    CMInt32 numCAs;
    char** caNames;
} SignTextRequest;

extern CMTMessageTemplate SignTextRequestTemplate[];

typedef struct GetLocalizedTextReply {
    CMInt32 whichString;
    char *localizedString;
} GetLocalizedTextReply;

extern CMTMessageTemplate GetLocalizedTextReplyTemplate[];

typedef struct ImportCertReply {
    CMInt32 result;
    CMInt32 resID;
} ImportCertReply;

extern CMTMessageTemplate ImportCertReplyTemplate[];

typedef struct PromptRequest {
    CMInt32 resID;
    char *prompt;
    CMTItem clientContext;
} PromptRequest;

extern CMTMessageTemplate PromptRequestTemplate[];

typedef struct PromptReply {
    CMInt32 resID;
    CMBool cancel;
    char *promptReply;
} PromptReply;

extern CMTMessageTemplate PromptReplyTemplate[];

typedef struct RedirectCompareReqeust {
    CMTItem socketStatus1Data;
    CMTItem socketStatus2Data;
} RedirectCompareRequest;

extern CMTMessageTemplate RedirectCompareRequestTemplate[];

typedef struct DecodeAndAddCRLRequest {
    CMTItem  derCrl;
    CMUint32 type;
    char *url;
} DecodeAndAddCRLRequest;

extern CMTMessageTemplate DecodeAndAddCRLRequestTemplate[];

typedef struct SecurityAdvisorRequest {
    CMInt32 infoContext;
    CMInt32 resID;
    char * hostname;
	char * senderAddr;
    CMUint32 encryptedP7CInfo;
    CMUint32 signedP7CInfo;
    CMInt32 decodeError;
    CMInt32 verifyError;
	CMBool encryptthis;
	CMBool signthis;
	CMInt32 numRecipients;
	char ** recipients;
} SecurityAdvisorRequest;

extern CMTMessageTemplate SecurityAdvisorRequestTemplate[];

/* "SecurityConfig" javascript related message templates */
typedef struct SCAddTempCertToPermDBRequest {
    CMTItem certKey;
    char* trustStr;
    char* nickname;
} SCAddTempCertToPermDBRequest;

extern CMTMessageTemplate SCAddTempCertToPermDBRequestTemplate[];

typedef struct SCDeletePermCertsRequest {
    CMTItem certKey;
    CMBool deleteAll;
} SCDeletePermCertsRequest;

extern CMTMessageTemplate SCDeletePermCertsRequestTemplate[];

typedef struct TimeMessage {
    CMInt32 year;
    CMInt32 month;
    CMInt32 day;
    CMInt32 hour;
    CMInt32 minute;
    CMInt32 second;
} TimeMessage;

extern CMTMessageTemplate TimeMessageTemplate[];

typedef struct CertEnumElement {
    char* name;
    CMTItem certKey;
} CertEnumElement;

typedef struct SCCertIndexEnumReply {
      int length;
      CertEnumElement* list;
} SCCertIndexEnumReply;

extern CMTMessageTemplate SCCertIndexEnumReplyTemplate[];

/* Test message */
typedef struct TestListElement {
	char * name;
	char * value;
} TestListElement;

typedef struct TestList {
	char *listName;
	int numElements;
	TestListElement *elements;
} TestList;

extern CMTMessageTemplate TestListTemplate[];

/* Preference-related structs */
typedef struct SetPrefElement {
    char* key;
    char* value;
    CMInt32 type;
} SetPrefElement;

typedef struct SetPrefListMessage {
    int length;
    SetPrefElement* list;
} SetPrefListMessage;

extern CMTMessageTemplate SetPrefListMessageTemplate[];

typedef struct GetPrefElement {
    char* key;
    CMInt32 type;
} GetPrefElement;

typedef struct GetPrefListRequest {
    int length;
    GetPrefElement* list;
} GetPrefListRequest;

extern CMTMessageTemplate GetPrefListRequestTemplate[];

typedef struct GetCertExtension {
    CMUint32 resID;
    CMUint32 extension;
} GetCertExtension;

extern CMTMessageTemplate GetCertExtensionTemplate[];

typedef struct HTMLCertInfoRequest {
    CMUint32 certID;
    CMUint32 showImages;
    CMUint32 showIssuer;
} HTMLCertInfoRequest;

extern CMTMessageTemplate HTMLCertInfoRequestTemplate[];

typedef struct EncryptRequestMessage
{
  CMTItem keyid;  /* May have length 0 for default */
  CMTItem data;
  CMTItem ctx;  /* serialized void* ptr */
} EncryptRequestMessage;

extern CMTMessageTemplate EncryptRequestTemplate[];

typedef struct SingleItemMessage EncryptReplyMessage;
#define EncryptReplyTemplate SingleItemMessageTemplate

typedef struct DecryptRequestMessage
{
  CMTItem data;
  CMTItem ctx;  /* serialized void* ptr */
} DecryptRequestMessage;
extern CMTMessageTemplate DecryptRequestTemplate[];

typedef struct SingleItemMessage DecryptReplyMessage;
#define DecryptReplyTemplate SingleItemMessageTemplate

#endif /* __MESSAGES_H__ */
