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

#include "stddef.h"
#include "messages.h"

CMTMessageTemplate SingleNumMessageTemplate[] =
{
  { CMT_DT_INT, offsetof(SingleNumMessage, value) },
  { CMT_DT_END }
};

CMTMessageTemplate SingleStringMessageTemplate[] =
{
  { CMT_DT_STRING, offsetof(SingleStringMessage, string) },
  { CMT_DT_END }
};

CMTMessageTemplate SingleItemMessageTemplate[] =
{
  { CMT_DT_ITEM, offsetof(SingleItemMessage, item) },
  { CMT_DT_END }
};

CMTMessageTemplate HelloRequestTemplate[] =
{
  { CMT_DT_INT, offsetof(HelloRequest, version) },
  { CMT_DT_INT, offsetof(HelloRequest, policy) },
  { CMT_DT_BOOL, offsetof(HelloRequest, doesUI) },
  { CMT_DT_STRING, offsetof(HelloRequest, profile) },
  { CMT_DT_STRING, offsetof(HelloRequest, profileDir) },
  { CMT_DT_END }
};

CMTMessageTemplate HelloReplyTemplate[] =
{
  { CMT_DT_INT, offsetof(HelloReply, result) },
  { CMT_DT_INT, offsetof(HelloReply, sessionID) },
  { CMT_DT_INT, offsetof(HelloReply, version) },
  { CMT_DT_STRING, offsetof(HelloReply, stringVersion) },
  { CMT_DT_INT, offsetof(HelloReply, httpPort) },
  { CMT_DT_INT, offsetof(HelloReply, policy) },
  { CMT_DT_ITEM, offsetof(HelloReply, nonce) },
  { CMT_DT_END }
};

CMTMessageTemplate SSLDataConnectionRequestTemplate[] =
{
  { CMT_DT_INT, offsetof(SSLDataConnectionRequest, flags) },
  { CMT_DT_INT, offsetof(SSLDataConnectionRequest, port) },
  { CMT_DT_STRING, offsetof(SSLDataConnectionRequest, hostIP) },
  { CMT_DT_STRING, offsetof(SSLDataConnectionRequest, hostName) },
  { CMT_DT_BOOL, offsetof(SSLDataConnectionRequest, forceHandshake) },
  { CMT_DT_ITEM, offsetof(SSLDataConnectionRequest, clientContext) },
  { CMT_DT_END }
};

CMTMessageTemplate TLSDataConnectionRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(TLSDataConnectionRequest, port) },
    { CMT_DT_STRING, offsetof(TLSDataConnectionRequest, hostIP) },
    { CMT_DT_STRING, offsetof(TLSDataConnectionRequest, hostName) },
    { CMT_DT_END }
};

CMTMessageTemplate TLSStepUpRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(TLSStepUpRequest, connID) },
    { CMT_DT_ITEM, offsetof(TLSStepUpRequest, clientContext) },
    { CMT_DT_END }
};

CMTMessageTemplate ProxyStepUpRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(ProxyStepUpRequest, connID) },
    { CMT_DT_ITEM, offsetof(ProxyStepUpRequest, clientContext) },
    { CMT_DT_STRING, offsetof(ProxyStepUpRequest, url) },
    { CMT_DT_END }
};

CMTMessageTemplate PKCS7DataConnectionRequestTemplate[] =
{
  { CMT_DT_INT, offsetof(PKCS7DataConnectionRequest, resID) },
  { CMT_DT_ITEM, offsetof(PKCS7DataConnectionRequest, clientContext) },
  { CMT_DT_END }
};

CMTMessageTemplate DataConnectionReplyTemplate[] =
{
  { CMT_DT_INT, offsetof(DataConnectionReply, result) },
  { CMT_DT_INT, offsetof(DataConnectionReply, connID) },
  { CMT_DT_INT, offsetof(DataConnectionReply, port) },
  { CMT_DT_END }
};

CMTMessageTemplate UIEventTemplate[] =
{
    { CMT_DT_INT, offsetof(UIEvent, resourceID) },
    { CMT_DT_INT, offsetof(UIEvent, width) },
    { CMT_DT_INT, offsetof(UIEvent, height) },
    { CMT_DT_BOOL, offsetof(UIEvent, isModal) },
    { CMT_DT_STRING, offsetof(UIEvent, url) },
    { CMT_DT_ITEM, offsetof(UIEvent, clientContext) },
    { CMT_DT_END }
};

/*
 * The old UI Event was missing the modal indication.
 * As a transition aid, we use the old template if the
 * "modern" version doesn't work.  Model is true in that case
 */
CMTMessageTemplate OldUIEventTemplate[] =
{
    { CMT_DT_INT, offsetof(UIEvent, resourceID) },
    { CMT_DT_INT, offsetof(UIEvent, width) },
    { CMT_DT_INT, offsetof(UIEvent, height) },
    { CMT_DT_STRING, offsetof(UIEvent, url) },
    { CMT_DT_ITEM, offsetof(UIEvent, clientContext) },
    { CMT_DT_END }
};

CMTMessageTemplate TaskCompletedEventTemplate[] =
{
    { CMT_DT_INT, offsetof(TaskCompletedEvent, resourceID) },
    { CMT_DT_INT, offsetof(TaskCompletedEvent, numTasks) },
    { CMT_DT_INT, offsetof(TaskCompletedEvent, result) },
    { CMT_DT_END }
};

CMTMessageTemplate VerifyDetachedSigRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(VerifyDetachedSigRequest, pkcs7ContentID) },
    { CMT_DT_INT, offsetof(VerifyDetachedSigRequest, certUsage) },
    { CMT_DT_INT, offsetof(VerifyDetachedSigRequest, hashAlgID) },
    { CMT_DT_BOOL, offsetof(VerifyDetachedSigRequest, keepCert) },
    { CMT_DT_ITEM, offsetof(VerifyDetachedSigRequest, hash) },
    { CMT_DT_END }
};

CMTMessageTemplate CreateSignedRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(CreateSignedRequest, scertRID) },
    { CMT_DT_INT, offsetof(CreateSignedRequest, ecertRID) },
    { CMT_DT_INT, offsetof(CreateSignedRequest, dig_alg) },
    { CMT_DT_ITEM, offsetof(CreateSignedRequest, digest) },
    { CMT_DT_END }
};

CMTMessageTemplate CreateContentInfoReplyTemplate[] =
{
    { CMT_DT_INT, offsetof(CreateContentInfoReply, ciRID) },
    { CMT_DT_INT, offsetof(CreateContentInfoReply, result) },
    { CMT_DT_INT, offsetof(CreateContentInfoReply, errorCode) },
    { CMT_DT_END }
};

CMTMessageTemplate CreateEncryptedRequestTemplate[] =
{
  { CMT_DT_INT, offsetof(CreateEncryptedRequest, scertRID) },
  { CMT_DT_LIST, offsetof(CreateEncryptedRequest, nrcerts) },
  { CMT_DT_INT, offsetof(CreateEncryptedRequest, rcertRIDs) },
  { CMT_DT_END }
};

CMTMessageTemplate CreateResourceRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(CreateResourceRequest, type) },
    { CMT_DT_ITEM, offsetof(CreateResourceRequest, params) },
    { CMT_DT_END }
};

CMTMessageTemplate CreateResourceReplyTemplate[] =
{
    { CMT_DT_INT, offsetof(CreateResourceReply, result) },
    { CMT_DT_INT, offsetof(CreateResourceReply, resID) },
    { CMT_DT_END }
};

CMTMessageTemplate GetAttribRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(GetAttribRequest, resID) },
    { CMT_DT_INT, offsetof(GetAttribRequest, fieldID) },
    { CMT_DT_END }
};

CMTMessageTemplate GetAttribReplyTemplate[] =
{
  { CMT_DT_INT, offsetof(GetAttribReply, result) },
  { CMT_DT_CHOICE, offsetof(GetAttribReply, value.type) },
  { CMT_DT_RID, offsetof(GetAttribReply, value.u.rid), 0, SSM_RID_ATTRIBUTE },
  { CMT_DT_INT, offsetof(GetAttribReply, value.u.numeric), 0,
    SSM_NUMERIC_ATTRIBUTE },
  { CMT_DT_ITEM, offsetof(GetAttribReply, value.u.string), 0,
    SSM_STRING_ATTRIBUTE},
  { CMT_DT_END_CHOICE },
  { CMT_DT_END }
};

CMTMessageTemplate SetAttribRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(SetAttribRequest, resID) },
    { CMT_DT_INT, offsetof(SetAttribRequest, fieldID) },
    { CMT_DT_CHOICE, offsetof(SetAttribRequest, value.type) },
    { CMT_DT_RID, offsetof(SetAttribRequest, value.u.rid), 0, SSM_RID_ATTRIBUTE },
    { CMT_DT_INT, offsetof(SetAttribRequest, value.u.numeric), 0,
                            SSM_NUMERIC_ATTRIBUTE },
    { CMT_DT_ITEM, offsetof(SetAttribRequest, value.u.string), 0,
                            SSM_STRING_ATTRIBUTE},
    { CMT_DT_END_CHOICE },
    { CMT_DT_END }
};

CMTMessageTemplate PickleResourceReplyTemplate[] =
{
    { CMT_DT_INT, offsetof(PickleResourceReply, result) },
    { CMT_DT_ITEM, offsetof(PickleResourceReply, blob) },
    { CMT_DT_END }
};

CMTMessageTemplate UnpickleResourceRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(UnpickleResourceRequest, resourceType) },
    { CMT_DT_ITEM, offsetof(UnpickleResourceRequest, resourceData) },
    { CMT_DT_END }
};

CMTMessageTemplate UnpickleResourceReplyTemplate[] =
{
    { CMT_DT_INT, offsetof(UnpickleResourceReply, result) },
    { CMT_DT_INT, offsetof(UnpickleResourceReply, resID) },
    { CMT_DT_END }
};

CMTMessageTemplate PickleSecurityStatusReplyTemplate[] =
{
    { CMT_DT_INT, offsetof(PickleSecurityStatusReply, result) },
    { CMT_DT_INT, offsetof(PickleSecurityStatusReply, securityLevel) },
    { CMT_DT_ITEM, offsetof(PickleSecurityStatusReply, blob) },
    { CMT_DT_END }
};

CMTMessageTemplate DupResourceReplyTemplate[] =
{
    { CMT_DT_INT, offsetof(DupResourceReply, result) },
    { CMT_DT_RID, offsetof(DupResourceReply, resID), 0, SSM_RID_ATTRIBUTE },
    { CMT_DT_END }
};

CMTMessageTemplate DestroyResourceRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(DestroyResourceRequest, resID) },
    { CMT_DT_INT, offsetof(DestroyResourceRequest, resType) },
    { CMT_DT_END }
};

CMTMessageTemplate VerifyCertRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(VerifyCertRequest, resID) },
    { CMT_DT_INT, offsetof(VerifyCertRequest, certUsage) },
    { CMT_DT_END }
};

CMTMessageTemplate AddTempCertToDBRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(AddTempCertToDBRequest, resID) },
    { CMT_DT_STRING, offsetof(AddTempCertToDBRequest, nickname) },
    { CMT_DT_INT, offsetof(AddTempCertToDBRequest, sslFlags) },
    { CMT_DT_INT, offsetof(AddTempCertToDBRequest, emailFlags) },
    { CMT_DT_INT, offsetof(AddTempCertToDBRequest, objSignFlags) },
    { CMT_DT_END }
};

CMTMessageTemplate MatchUserCertRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(MatchUserCertRequest, certType) },
    { CMT_DT_LIST, offsetof(MatchUserCertRequest, numCANames) },
    { CMT_DT_STRING, offsetof(MatchUserCertRequest, caNames) },
    { CMT_DT_END }
};

CMTMessageTemplate MatchUserCertReplyTemplate[] =
{
    { CMT_DT_LIST, offsetof(MatchUserCertReply, numCerts) },
    { CMT_DT_INT, offsetof(MatchUserCertReply, certs) },
    { CMT_DT_END }
};

CMTMessageTemplate EncodeCRMFReqRequestTemplate[] =
{
    { CMT_DT_LIST, offsetof(EncodeCRMFReqRequest, numRequests) },
    { CMT_DT_INT, offsetof(EncodeCRMFReqRequest, reqIDs) },
    { CMT_DT_END }
};

CMTMessageTemplate CMMFCertResponseRequestTemplate[] =
{
    { CMT_DT_STRING, offsetof(CMMFCertResponseRequest, nickname) },
    { CMT_DT_STRING, offsetof(CMMFCertResponseRequest, base64Der) },
    { CMT_DT_INT,    offsetof(CMMFCertResponseRequest, doBackup) },
    { CMT_DT_ITEM,   offsetof(CMMFCertResponseRequest, clientContext) },
    { CMT_DT_END }
};

CMTMessageTemplate PasswordRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(PasswordRequest, tokenKey) },
    { CMT_DT_STRING, offsetof(PasswordRequest, prompt) },
    { CMT_DT_ITEM, offsetof(PasswordRequest, clientContext) },
    { CMT_DT_END }
};

CMTMessageTemplate PasswordReplyTemplate[] =
{
    { CMT_DT_INT, offsetof(PasswordReply, result) },
    { CMT_DT_INT, offsetof(PasswordReply, tokenID) },
    { CMT_DT_STRING, offsetof(PasswordReply, passwd) },
    { CMT_DT_END }
};

CMTMessageTemplate KeyPairGenRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(KeyPairGenRequest, keyGenCtxtID) },
    { CMT_DT_INT, offsetof(KeyPairGenRequest, genMechanism) },
    { CMT_DT_INT, offsetof(KeyPairGenRequest, keySize) },
    { CMT_DT_ITEM, offsetof(KeyPairGenRequest, params) },
    { CMT_DT_END }
};

CMTMessageTemplate DecodeAndCreateTempCertRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(DecodeAndCreateTempCertRequest, type) },
    { CMT_DT_ITEM, offsetof(DecodeAndCreateTempCertRequest, cert) },
    { CMT_DT_END }
};

CMTMessageTemplate GenKeyOldStyleRequestTemplate[] =
{
    { CMT_DT_STRING, offsetof(GenKeyOldStyleRequest, choiceString) },
    { CMT_DT_STRING, offsetof(GenKeyOldStyleRequest, challenge) },
    { CMT_DT_STRING, offsetof(GenKeyOldStyleRequest, typeString) },
    { CMT_DT_STRING, offsetof(GenKeyOldStyleRequest, pqgString) },
    { CMT_DT_END }
};

CMTMessageTemplate GenKeyOldStyleTokenRequestTemplate[] = 
{
    { CMT_DT_INT,  offsetof(GenKeyOldStyleTokenRequest, rid)       },
    { CMT_DT_LIST,  offsetof(GenKeyOldStyleTokenRequest, numtokens) },
    { CMT_DT_STRING,offsetof(GenKeyOldStyleTokenRequest, tokenNames)},
    { CMT_DT_END }
};

CMTMessageTemplate GenKeyOldStyleTokenReplyTemplate[] = 
{
    { CMT_DT_INT, offsetof(GenKeyOldStyleTokenReply, rid) },
    { CMT_DT_BOOL, offsetof(GenKeyOldStyleTokenReply, cancel) },
    { CMT_DT_STRING, offsetof(GenKeyOldStyleTokenReply, tokenName) },
    { CMT_DT_END }
};

CMTMessageTemplate GenKeyOldStylePasswordRequestTemplate[] = 
{
    { CMT_DT_INT, offsetof(GenKeyOldStylePasswordRequest, rid) },
    { CMT_DT_STRING, offsetof(GenKeyOldStylePasswordRequest, tokenName) },
    { CMT_DT_BOOL, offsetof(GenKeyOldStylePasswordRequest, internal) },
    { CMT_DT_INT, offsetof(GenKeyOldStylePasswordRequest, minpwdlen) },
    { CMT_DT_INT, offsetof(GenKeyOldStylePasswordRequest, maxpwdlen) }, 
    { CMT_DT_END }
};

CMTMessageTemplate GenKeyOldStylePasswordReplyTemplate[] = 
{
    { CMT_DT_INT, offsetof(GenKeyOldStylePasswordReply, rid) },
    { CMT_DT_BOOL, offsetof(GenKeyOldStylePasswordReply, cancel) },
    { CMT_DT_STRING, offsetof(GenKeyOldStylePasswordReply, password) },
    { CMT_DT_END }
};


CMTMessageTemplate GetKeyChoiceListRequestTemplate[] =
{
    { CMT_DT_STRING, offsetof(GetKeyChoiceListRequest, type) },
    { CMT_DT_STRING, offsetof(GetKeyChoiceListRequest, pqgString) },
    { CMT_DT_END }
};

CMTMessageTemplate GetKeyChoiceListReplyTemplate[] =
{
    { CMT_DT_LIST, offsetof(GetKeyChoiceListReply, nchoices) },
    { CMT_DT_STRING, offsetof(GetKeyChoiceListReply, choices) },
    { CMT_DT_END }
};

CMTMessageTemplate AddNewSecurityModuleRequestTemplate[] =
{
    { CMT_DT_STRING, offsetof(AddNewSecurityModuleRequest, moduleName) },
    { CMT_DT_STRING, offsetof(AddNewSecurityModuleRequest, libraryPath) },
    { CMT_DT_INT, offsetof(AddNewSecurityModuleRequest, pubMechFlags) },
    { CMT_DT_INT, offsetof(AddNewSecurityModuleRequest, pubCipherFlags) },
    { CMT_DT_END }
};

CMTMessageTemplate FilePathRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(FilePathRequest, resID) },
    { CMT_DT_STRING, offsetof(FilePathRequest, prompt) },
    { CMT_DT_BOOL, offsetof(FilePathRequest, getExistingFile) },
    { CMT_DT_STRING, offsetof(FilePathRequest, fileRegEx) },
    { CMT_DT_END }
};

CMTMessageTemplate FilePathReplyTemplate[] =
{
    { CMT_DT_INT, offsetof(FilePathReply, resID) },
    { CMT_DT_STRING, offsetof(FilePathReply, filePath) },
    { CMT_DT_END }
};

CMTMessageTemplate PasswordPromptReplyTemplate[] =
{
    { CMT_DT_INT, offsetof(PasswordPromptReply, resID) },
    { CMT_DT_STRING, offsetof(PasswordPromptReply, promptReply) },
    { CMT_DT_END }
};

CMTMessageTemplate SignTextRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(SignTextRequest, resID) },
    { CMT_DT_STRING, offsetof(SignTextRequest, stringToSign) },
    { CMT_DT_STRING, offsetof(SignTextRequest, hostName) },
    { CMT_DT_STRING, offsetof(SignTextRequest, caOption) },
    { CMT_DT_LIST, offsetof(SignTextRequest, numCAs) },
    { CMT_DT_STRING, offsetof(SignTextRequest, caNames) },
    { CMT_DT_END }
};

CMTMessageTemplate GetLocalizedTextReplyTemplate[] =
{
    { CMT_DT_INT, offsetof(GetLocalizedTextReply, whichString) },
    { CMT_DT_STRING, offsetof(GetLocalizedTextReply, localizedString) },
    { CMT_DT_END }
};

CMTMessageTemplate ImportCertReplyTemplate[] =
{
    { CMT_DT_INT, offsetof(ImportCertReply, result) },
    { CMT_DT_INT, offsetof(ImportCertReply, resID) },
    { CMT_DT_END }
};

CMTMessageTemplate PromptRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(PromptRequest, resID) },
    { CMT_DT_STRING, offsetof(PromptRequest, prompt) },
    { CMT_DT_ITEM, offsetof(PromptRequest, clientContext) },
    { CMT_DT_END }
};

CMTMessageTemplate PromptReplyTemplate[] =
{
    { CMT_DT_INT, offsetof(PromptReply, resID) },
    { CMT_DT_BOOL, offsetof(PromptReply, cancel) },
    { CMT_DT_STRING, offsetof(PromptReply, promptReply) },
    { CMT_DT_END }
};

CMTMessageTemplate RedirectCompareRequestTemplate[] =
{
    { CMT_DT_ITEM, offsetof(RedirectCompareRequest, socketStatus1Data) },
    { CMT_DT_ITEM, offsetof(RedirectCompareRequest, socketStatus2Data) },
    { CMT_DT_END }
};

CMTMessageTemplate DecodeAndAddCRLRequestTemplate[] = 
{
    { CMT_DT_ITEM,   offsetof(DecodeAndAddCRLRequest, derCrl) },
    { CMT_DT_INT,    offsetof(DecodeAndAddCRLRequest, type)   },
    { CMT_DT_STRING, offsetof(DecodeAndAddCRLRequest, url)    },
    { CMT_DT_END }
};

CMTMessageTemplate SecurityAdvisorRequestTemplate[] =
{
    { CMT_DT_INT,   offsetof(SecurityAdvisorRequest, infoContext) },
    { CMT_DT_INT,   offsetof(SecurityAdvisorRequest, resID) },
    { CMT_DT_STRING, offsetof(SecurityAdvisorRequest, hostname) },
    { CMT_DT_STRING, offsetof(SecurityAdvisorRequest, senderAddr) },
    { CMT_DT_INT,   offsetof(SecurityAdvisorRequest, encryptedP7CInfo) },
    { CMT_DT_INT,   offsetof(SecurityAdvisorRequest, signedP7CInfo) },
    { CMT_DT_INT,   offsetof(SecurityAdvisorRequest, decodeError) },
    { CMT_DT_INT,   offsetof(SecurityAdvisorRequest, verifyError) },
    { CMT_DT_BOOL,   offsetof(SecurityAdvisorRequest, encryptthis) },
    { CMT_DT_BOOL,   offsetof(SecurityAdvisorRequest, signthis) },
    { CMT_DT_LIST,	offsetof(SecurityAdvisorRequest, numRecipients) },
    { CMT_DT_STRING, offsetof(SecurityAdvisorRequest, recipients) },
    { CMT_DT_END }
};

CMTMessageTemplate SCAddTempCertToPermDBRequestTemplate[] =
{
    { CMT_DT_ITEM, offsetof(SCAddTempCertToPermDBRequest, certKey) },
    { CMT_DT_STRING, offsetof(SCAddTempCertToPermDBRequest, trustStr) },
    { CMT_DT_STRING, offsetof(SCAddTempCertToPermDBRequest, nickname) },
    { CMT_DT_END }
};

CMTMessageTemplate SCDeletePermCertsRequestTemplate[] =
{
    { CMT_DT_ITEM, offsetof(SCDeletePermCertsRequest, certKey) },
    { CMT_DT_BOOL, offsetof(SCDeletePermCertsRequest, deleteAll) },
    { CMT_DT_END }
};

CMTMessageTemplate TimeMessageTemplate[] =
{
    { CMT_DT_INT, offsetof(TimeMessage, year) },
    { CMT_DT_INT, offsetof(TimeMessage, month) },
    { CMT_DT_INT, offsetof(TimeMessage, day) },
    { CMT_DT_INT, offsetof(TimeMessage, hour) },
    { CMT_DT_INT, offsetof(TimeMessage, minute) },
    { CMT_DT_INT, offsetof(TimeMessage, second) },
    { CMT_DT_END }
};

CMTMessageTemplate SCCertIndexEnumReplyTemplate[] =
{
    { CMT_DT_INT, offsetof(SCCertIndexEnumReply, length) },
    { CMT_DT_STRUCT_PTR, offsetof(SCCertIndexEnumReply, list) },
    { CMT_DT_STRING, offsetof(CertEnumElement, name) },
    { CMT_DT_ITEM, offsetof(CertEnumElement, certKey) },
    { CMT_DT_END_STRUCT_LIST },
    { CMT_DT_END }
};

/* Test template */

CMTMessageTemplate TestListTemplate[] = 
{
	{ CMT_DT_STRING, offsetof(TestList, listName) },
	{ CMT_DT_STRUCT_LIST, offsetof(TestList, numElements) },
	{ CMT_DT_STRUCT_PTR, offsetof(TestList, elements) },
	{ CMT_DT_STRING, offsetof(TestListElement, name) },
	{ CMT_DT_STRING, offsetof(TestListElement, value) },
	{ CMT_DT_END_STRUCT_LIST},
	{ CMT_DT_END}
};

CMTMessageTemplate SetPrefListMessageTemplate[] =
{
    { CMT_DT_STRUCT_LIST, offsetof(SetPrefListMessage, length) },
    { CMT_DT_STRUCT_PTR, offsetof(SetPrefListMessage, list) },
    { CMT_DT_STRING, offsetof(SetPrefElement, key) },
    { CMT_DT_STRING, offsetof(SetPrefElement, value) },
    { CMT_DT_INT, offsetof(SetPrefElement, type) },
    { CMT_DT_END_STRUCT_LIST },
    { CMT_DT_END }
};

CMTMessageTemplate GetPrefListRequestTemplate[] =
{
    { CMT_DT_STRUCT_LIST, offsetof(GetPrefListRequest, length) },
    { CMT_DT_STRUCT_PTR, offsetof(GetPrefListRequest, list) },
    { CMT_DT_STRING, offsetof(GetPrefElement, key) },
    { CMT_DT_INT, offsetof(GetPrefElement, type) },
    { CMT_DT_END_STRUCT_LIST },
    { CMT_DT_END }
};

CMTMessageTemplate GetCertExtensionTemplate[] =
{
    { CMT_DT_INT, offsetof(GetCertExtension, resID) },
    { CMT_DT_INT, offsetof(GetCertExtension, extension) },
    { CMT_DT_END }
};

CMTMessageTemplate HTMLCertInfoRequestTemplate[] =
{
    { CMT_DT_INT, offsetof(HTMLCertInfoRequest, certID) },
    { CMT_DT_INT, offsetof(HTMLCertInfoRequest, showImages) },
    { CMT_DT_INT, offsetof(HTMLCertInfoRequest, showIssuer) },
    { CMT_DT_END }
};

CMTMessageTemplate EncryptRequestTemplate[] =
{
  { CMT_DT_ITEM, offsetof(EncryptRequestMessage, keyid) },
  { CMT_DT_ITEM, offsetof(EncryptRequestMessage, data) },
  { CMT_DT_ITEM, offsetof(EncryptRequestMessage, ctx) },
  { CMT_DT_END }
};

CMTMessageTemplate DecryptRequestTemplate[] =
{
  { CMT_DT_ITEM, offsetof(DecryptRequestMessage, data) },
  { CMT_DT_ITEM, offsetof(DecryptRequestMessage, ctx) },
  { CMT_DT_END }
};
