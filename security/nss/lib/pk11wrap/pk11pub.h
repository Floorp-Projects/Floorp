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
 *   Douglas Stebila <douglas@stebila.ca>, Sun Microsystems Laboratories
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
#ifndef _PK11PUB_H_
#define _PK11PUB_H_
#include "plarena.h"
#include "seccomon.h"
#include "secoidt.h"
#include "secdert.h"
#include "keyt.h"
#include "certt.h"
#include "pkcs11t.h"
#include "secmodt.h"
#include "seccomon.h"
#include "pkcs7t.h"
#include "cmsreclist.h"

/*
 * Exported PK11 wrap functions.
 */

SEC_BEGIN_PROTOS

/************************************************************
 * Generic Slot Lists Management
 ************************************************************/
void PK11_FreeSlotList(PK11SlotList *list);
PK11SlotListElement * PK11_GetFirstSafe(PK11SlotList *list);
PK11SlotListElement *PK11_GetNextSafe(PK11SlotList *list, 
				PK11SlotListElement *le, PRBool restart);

/************************************************************
 * Generic Slot Management
 ************************************************************/
PK11SlotInfo *PK11_ReferenceSlot(PK11SlotInfo *slot);
void PK11_FreeSlot(PK11SlotInfo *slot);
SECStatus PK11_DestroyObject(PK11SlotInfo *slot,CK_OBJECT_HANDLE object);
SECStatus PK11_DestroyTokenObject(PK11SlotInfo *slot,CK_OBJECT_HANDLE object);
PK11SlotInfo *PK11_GetInternalKeySlot(void);
PK11SlotInfo *PK11_GetInternalSlot(void);
SECStatus PK11_Logout(PK11SlotInfo *slot);
void PK11_LogoutAll(void);


/************************************************************
 *  Slot Password Management
 ************************************************************/
void PK11_SetSlotPWValues(PK11SlotInfo *slot,int askpw, int timeout);
void PK11_GetSlotPWValues(PK11SlotInfo *slot,int *askpw, int *timeout);
SECStatus PK11_CheckSSOPassword(PK11SlotInfo *slot, char *ssopw);
SECStatus PK11_CheckUserPassword(PK11SlotInfo *slot,char *pw);
PRBool PK11_IsLoggedIn(PK11SlotInfo *slot, void *wincx);
SECStatus PK11_InitPin(PK11SlotInfo *slot,char *ssopw, char *pk11_userpwd);
SECStatus PK11_ChangePW(PK11SlotInfo *slot,char *oldpw, char *newpw);
void PK11_SetPasswordFunc(PK11PasswordFunc func);
int PK11_GetMinimumPwdLength(PK11SlotInfo *slot);
SECStatus PK11_ResetToken(PK11SlotInfo *slot, char *sso_pwd);
SECStatus PK11_Authenticate(PK11SlotInfo *slot, PRBool loadCerts, void *wincx);
SECStatus PK11_TokenRefresh(PK11SlotInfo *slot);


/******************************************************************
 *           Slot info functions
 ******************************************************************/
PK11SlotInfo *PK11_FindSlotByName(const char *name);
/******************************************************************
 * PK11_FindSlotsByNames searches for a PK11SlotInfo using one or
 * more criteria : dllName, slotName and tokenName . In addition, if
 * presentOnly is set , only slots with a token inserted will be
 * returned.
 ******************************************************************/
PK11SlotList *PK11_FindSlotsByNames(const char *dllName,
        const char* slotName, const char* tokenName, PRBool presentOnly);
PRBool PK11_IsReadOnly(PK11SlotInfo *slot);
PRBool PK11_IsInternal(PK11SlotInfo *slot);
char * PK11_GetTokenName(PK11SlotInfo *slot);
char * PK11_GetSlotName(PK11SlotInfo *slot);
PRBool PK11_NeedLogin(PK11SlotInfo *slot);
PRBool PK11_IsFriendly(PK11SlotInfo *slot);
PRBool PK11_IsHW(PK11SlotInfo *slot);
PRBool PK11_NeedUserInit(PK11SlotInfo *slot);
PRBool PK11_ProtectedAuthenticationPath(PK11SlotInfo *slot);
int PK11_GetSlotSeries(PK11SlotInfo *slot);
int PK11_GetCurrentWrapIndex(PK11SlotInfo *slot);
unsigned long PK11_GetDefaultFlags(PK11SlotInfo *slot);
CK_SLOT_ID PK11_GetSlotID(PK11SlotInfo *slot);
SECMODModuleID PK11_GetModuleID(PK11SlotInfo *slot);
SECStatus PK11_GetSlotInfo(PK11SlotInfo *slot, CK_SLOT_INFO *info);
SECStatus PK11_GetTokenInfo(PK11SlotInfo *slot, CK_TOKEN_INFO *info);
PRBool PK11_IsDisabled(PK11SlotInfo *slot);
PRBool PK11_HasRootCerts(PK11SlotInfo *slot);
PK11DisableReasons PK11_GetDisabledReason(PK11SlotInfo *slot);
/* Prevents the slot from being used, and set disable reason to user-disable */
/* NOTE: Mechanisms that were ON continue to stay ON */
/*       Therefore, when the slot is enabled, it will remember */
/*       what mechanisms needs to be turned on */
PRBool PK11_UserDisableSlot(PK11SlotInfo *slot);
/* Allow all mechanisms that are ON before UserDisableSlot() */
/* was called to be available again */
PRBool PK11_UserEnableSlot(PK11SlotInfo *slot);
/*
 * wait for a specific slot event.
 * event is a specific event to wait for. Currently only 
 *    PK11TokenChangeOrRemovalEvent and PK11TokenPresentEvents are defined.
 * timeout can be an interval time to wait, PR_INTERVAL_NO_WAIT (meaning only
 * poll once), or PR_INTERVAL_NO_TIMEOUT (meaning block until a change).
 * pollInterval is a suggested pulling interval value. '0' means use the 
 *  default. Future implementations that don't poll may ignore this value.
 * series is the current series for the last slot. This should be the series 
 *  value for the slot the last time you read persistant information from the
 *  slot. For instance, if you publish a cert from the slot, you should obtain
 *  the slot series at that time. Then PK11_WaitForTokenEvent can detect a 
 *  a change in the slot between the time you publish and the time 
 *  PK11_WaitForTokenEvent is called, elliminating potential race conditions.
 *
 * The current status that is returned is:
 *   PK11TokenNotRemovable - always returned for any non-removable token.
 *   PK11TokenPresent - returned when the token is present and we are waiting
 *     on a PK11TokenPresentEvent. Then next event to look for is a 
 *     PK11TokenChangeOrRemovalEvent.
 *   PK11TokenChanged - returned when the old token has been removed and a new
 *     token ad been inserted, and we are waiting for a 
 *     PK11TokenChangeOrRemovalEvent. The next event to look for is another
 *     PK11TokenChangeOrRemovalEvent.
 *   PK11TokenRemoved - returned when the token is not present and we are 
 *     waiting for a PK11TokenChangeOrRemovalEvent. The next event to look for 
 *     is a PK11TokenPresentEvent.
 */
PK11TokenStatus PK11_WaitForTokenEvent(PK11SlotInfo *slot, PK11TokenEvent event,
	PRIntervalTime timeout, PRIntervalTime pollInterval, int series);

PRBool PK11_NeedPWInit(void);
PRBool PK11_TokenExists(CK_MECHANISM_TYPE);
SECStatus PK11_GetModInfo(SECMODModule *mod, CK_INFO *info);
PRBool PK11_IsFIPS(void);
SECMODModule *PK11_GetModule(PK11SlotInfo *slot);

/*********************************************************************
 *            Slot mapping utility functions.
 *********************************************************************/
PRBool PK11_IsPresent(PK11SlotInfo *slot);
PRBool PK11_DoesMechanism(PK11SlotInfo *slot, CK_MECHANISM_TYPE type);
PK11SlotList * PK11_GetAllTokens(CK_MECHANISM_TYPE type,PRBool needRW,
					PRBool loadCerts, void *wincx);
PK11SlotInfo *PK11_GetBestSlotMultiple(CK_MECHANISM_TYPE *type, int count,
							void *wincx);
PK11SlotInfo *PK11_GetBestSlot(CK_MECHANISM_TYPE type, void *wincx);
CK_MECHANISM_TYPE PK11_GetBestWrapMechanism(PK11SlotInfo *slot);
int PK11_GetBestKeyLength(PK11SlotInfo *slot, CK_MECHANISM_TYPE type);

/*********************************************************************
 *       Mechanism Mapping functions
 *********************************************************************/
CK_MECHANISM_TYPE PK11_GetKeyType(CK_MECHANISM_TYPE type,unsigned long len);
CK_MECHANISM_TYPE PK11_GetKeyGen(CK_MECHANISM_TYPE type);
int PK11_GetBlockSize(CK_MECHANISM_TYPE type,SECItem *params);
int PK11_GetIVLength(CK_MECHANISM_TYPE type);
SECItem *PK11_ParamFromIV(CK_MECHANISM_TYPE type,SECItem *iv);
unsigned char *PK11_IVFromParam(CK_MECHANISM_TYPE type,SECItem *param,int *len);
SECItem * PK11_BlockData(SECItem *data,unsigned long size);

/* PKCS #11 to DER mapping functions */
SECItem *PK11_ParamFromAlgid(SECAlgorithmID *algid);
SECItem *PK11_GenerateNewParam(CK_MECHANISM_TYPE, PK11SymKey *);
CK_MECHANISM_TYPE PK11_AlgtagToMechanism(SECOidTag algTag);
SECOidTag PK11_MechanismToAlgtag(CK_MECHANISM_TYPE type);
SECOidTag PK11_FortezzaMapSig(SECOidTag algTag);
SECStatus PK11_ParamToAlgid(SECOidTag algtag, SECItem *param,
                                   PRArenaPool *arena, SECAlgorithmID *algid);
SECStatus PK11_SeedRandom(PK11SlotInfo *,unsigned char *data,int len);
SECStatus PK11_RandomUpdate(void *data, size_t bytes);
SECStatus PK11_GenerateRandom(unsigned char *data,int len);
CK_RV PK11_MapPBEMechanismToCryptoMechanism(CK_MECHANISM_PTR pPBEMechanism,
					    CK_MECHANISM_PTR pCryptoMechanism,
					    SECItem *pbe_pwd, PRBool bad3DES);
CK_MECHANISM_TYPE PK11_GetPadMechanism(CK_MECHANISM_TYPE);

/**********************************************************************
 *                   Symetric, Public, and Private Keys 
 **********************************************************************/
void PK11_FreeSymKey(PK11SymKey *key);
PK11SymKey *PK11_ReferenceSymKey(PK11SymKey *symKey);
PK11SymKey *PK11_ImportSymKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
    PK11Origin origin, CK_ATTRIBUTE_TYPE operation, SECItem *key, void *wincx);
PK11SymKey *PK11_ImportSymKeyWithFlags(PK11SlotInfo *slot, 
    CK_MECHANISM_TYPE type, PK11Origin origin, CK_ATTRIBUTE_TYPE operation, 
    SECItem *key, CK_FLAGS flags, PRBool isPerm, void *wincx);
PK11SymKey *PK11_SymKeyFromHandle(PK11SlotInfo *slot, PK11SymKey *parent,
    PK11Origin origin, CK_MECHANISM_TYPE type, CK_OBJECT_HANDLE keyID, 
    PRBool owner, void *wincx);
PK11SymKey *PK11_GetWrapKey(PK11SlotInfo *slot, int wrap,
			      CK_MECHANISM_TYPE type,int series, void *wincx);
/*
 * This function is not thread-safe.  It can only be called when only
 * one thread has a reference to wrapKey.
 */
void PK11_SetWrapKey(PK11SlotInfo *slot, int wrap, PK11SymKey *wrapKey);
CK_MECHANISM_TYPE PK11_GetMechanism(PK11SymKey *symKey);
CK_OBJECT_HANDLE PK11_ImportPublicKey(PK11SlotInfo *slot, 
				SECKEYPublicKey *pubKey, PRBool isToken);
PK11SymKey *PK11_KeyGen(PK11SlotInfo *slot,CK_MECHANISM_TYPE type,
				SECItem *param,	int keySize,void *wincx);
PK11SymKey *PK11_TokenKeyGen(PK11SlotInfo *slot, CK_MECHANISM_TYPE type,
				SECItem *param, int keySize, SECItem *keyid,
				PRBool isToken, void *wincx);
PK11SymKey * PK11_ListFixedKeysInSlot(PK11SlotInfo *slot, char *nickname,
								void *wincx);
PK11SymKey *PK11_GetNextSymKey(PK11SymKey *symKey);
CK_KEY_TYPE PK11_GetSymKeyType(PK11SymKey *key);

SECStatus PK11_PubWrapSymKey(CK_MECHANISM_TYPE type, SECKEYPublicKey *pubKey,
				PK11SymKey *symKey, SECItem *wrappedKey);
SECStatus PK11_WrapSymKey(CK_MECHANISM_TYPE type, SECItem *params,
	 PK11SymKey *wrappingKey, PK11SymKey *symKey, SECItem *wrappedKey);
/* move a key to 'slot' optionally set the key attributes according to either
 * operation or the  flags and making the key permanent at the same time.
 * If the key is moved to the same slot, operation and flags values are 
 * currently ignored */
PK11SymKey *PK11_MoveSymKey(PK11SlotInfo *slot, CK_ATTRIBUTE_TYPE operation, 
			CK_FLAGS flags, PRBool  perm, PK11SymKey *symKey);
/*
 * derive a new key from the base key.
 *  PK11_Derive returns a key which can do exactly one operation, and is
 * ephemeral (session key).
 *  PK11_DeriveWithFlags is the same as PK11_Derive, except you can use
 * CKF_ flags to enable more than one operation.
 *  PK11_DeriveWithFlagsPerm is the same as PK11_DeriveWithFlags except you can
 *  (optionally) make the key permanent (token key).
 */
PK11SymKey *PK11_Derive(PK11SymKey *baseKey, CK_MECHANISM_TYPE mechanism,
   			SECItem *param, CK_MECHANISM_TYPE target, 
		        CK_ATTRIBUTE_TYPE operation, int keySize);
PK11SymKey *PK11_DeriveWithFlags( PK11SymKey *baseKey, 
	CK_MECHANISM_TYPE derive, SECItem *param, CK_MECHANISM_TYPE target, 
	CK_ATTRIBUTE_TYPE operation, int keySize, CK_FLAGS flags);
PK11SymKey * PK11_DeriveWithFlagsPerm( PK11SymKey *baseKey, 
	CK_MECHANISM_TYPE derive, 
	SECItem *param, CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, 
	int keySize, CK_FLAGS flags, PRBool isPerm);

PK11SymKey *PK11_PubDerive( SECKEYPrivateKey *privKey, 
 SECKEYPublicKey *pubKey, PRBool isSender, SECItem *randomA, SECItem *randomB,
 CK_MECHANISM_TYPE derive, CK_MECHANISM_TYPE target,
		 CK_ATTRIBUTE_TYPE operation, int keySize,void *wincx) ;
PK11SymKey *PK11_PubDeriveWithKDF( SECKEYPrivateKey *privKey, 
 SECKEYPublicKey *pubKey, PRBool isSender, SECItem *randomA, SECItem *randomB,
 CK_MECHANISM_TYPE derive, CK_MECHANISM_TYPE target,
		 CK_ATTRIBUTE_TYPE operation, int keySize,
		 CK_ULONG kdf, SECItem *sharedData, void *wincx);

/*
 * unwrap a new key with a symetric key.
 *  PK11_Unwrap returns a key which can do exactly one operation, and is
 * ephemeral (session key).
 *  PK11_UnwrapWithFlags is the same as PK11_Unwrap, except you can use
 * CKF_ flags to enable more than one operation.
 *  PK11_UnwrapWithFlagsPerm is the same as PK11_UnwrapWithFlags except you can
 *  (optionally) make the key permanent (token key).
 */
PK11SymKey *PK11_UnwrapSymKey(PK11SymKey *key, 
	CK_MECHANISM_TYPE wraptype, SECItem *param, SECItem *wrapppedKey,  
	CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, int keySize);
PK11SymKey *PK11_UnwrapSymKeyWithFlags(PK11SymKey *wrappingKey, 
	CK_MECHANISM_TYPE wrapType, SECItem *param, SECItem *wrappedKey, 
	CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, int keySize, 
	CK_FLAGS flags);
PK11SymKey * PK11_UnwrapSymKeyWithFlagsPerm(PK11SymKey *wrappingKey, 
	CK_MECHANISM_TYPE wrapType,
        SECItem *param, SECItem *wrappedKey, 
	CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, 
	 int keySize, CK_FLAGS flags, PRBool isPerm);

/*
 * unwrap a new key with a private key.
 *  PK11_PubUnwrap returns a key which can do exactly one operation, and is
 * ephemeral (session key).
 *  PK11_PubUnwrapWithFlagsPerm is the same as PK11_PubUnwrap except you can 
 * use * CKF_ flags to enable more than one operation, and optionally make 
 * the key permanent (token key).
 */
PK11SymKey *PK11_PubUnwrapSymKey(SECKEYPrivateKey *key, SECItem *wrapppedKey,
	 CK_MECHANISM_TYPE target, CK_ATTRIBUTE_TYPE operation, int keySize);
PK11SymKey * PK11_PubUnwrapSymKeyWithFlagsPerm(SECKEYPrivateKey *wrappingKey, 
	  SECItem *wrappedKey, CK_MECHANISM_TYPE target, 
	  CK_ATTRIBUTE_TYPE operation, int keySize,
	  CK_FLAGS flags, PRBool isPerm);
PK11SymKey *PK11_FindFixedKey(PK11SlotInfo *slot, CK_MECHANISM_TYPE type, 
						SECItem *keyID, void *wincx);
SECStatus PK11_DeleteTokenPrivateKey(SECKEYPrivateKey *privKey,PRBool force);
SECStatus PK11_DeleteTokenPublicKey(SECKEYPublicKey *pubKey);
SECStatus PK11_DeleteTokenSymKey(PK11SymKey *symKey);
SECStatus PK11_DeleteTokenCertAndKey(CERTCertificate *cert,void *wincx);
SECKEYPrivateKey * PK11_LoadPrivKey(PK11SlotInfo *slot,
		SECKEYPrivateKey *privKey, SECKEYPublicKey *pubKey, 
					PRBool token, PRBool sensitive);
char * PK11_GetSymKeyNickname(PK11SymKey *symKey);
char * PK11_GetPrivateKeyNickname(SECKEYPrivateKey *privKey);
char * PK11_GetPublicKeyNickname(SECKEYPublicKey *pubKey);
SECStatus PK11_SetSymKeyNickname(PK11SymKey *symKey, const char *nickname);
SECStatus PK11_SetPrivateKeyNickname(SECKEYPrivateKey *privKey, 
							const char *nickname);
SECStatus PK11_SetPublicKeyNickname(SECKEYPublicKey *pubKey, 
							const char *nickname);

/* size to hold key in bytes */
unsigned int PK11_GetKeyLength(PK11SymKey *key);
/* size of actual secret parts of key in bits */
/* algid is because RC4 strength is determined by the effective bits as well
 * as the key bits */
unsigned int PK11_GetKeyStrength(PK11SymKey *key,SECAlgorithmID *algid);
SECStatus PK11_ExtractKeyValue(PK11SymKey *symKey);
SECItem * PK11_GetKeyData(PK11SymKey *symKey);
PK11SlotInfo * PK11_GetSlotFromKey(PK11SymKey *symKey);
void *PK11_GetWindow(PK11SymKey *symKey);
SECKEYPrivateKey *PK11_GenerateKeyPair(PK11SlotInfo *slot,
   CK_MECHANISM_TYPE type, void *param, SECKEYPublicKey **pubk,
		 	    PRBool isPerm, PRBool isSensitive, void *wincx);
SECKEYPrivateKey * PK11_FindPrivateKeyFromCert(PK11SlotInfo *slot,
				 	CERTCertificate *cert, void *wincx);
SECKEYPrivateKey * PK11_FindKeyByAnyCert(CERTCertificate *cert, void *wincx);
SECKEYPrivateKey * PK11_FindKeyByKeyID(PK11SlotInfo *slot, SECItem *keyID,
				       void *wincx);
int PK11_GetPrivateModulusLen(SECKEYPrivateKey *key); 

/* note: despite the name, this function takes a private key. */
SECStatus PK11_PubDecryptRaw(SECKEYPrivateKey *key, unsigned char *data,
   unsigned *outLen, unsigned int maxLen, unsigned char *enc, unsigned encLen);
#define PK11_PrivDecryptRaw PK11_PubDecryptRaw
/* The encrypt function that complements the above decrypt function. */
SECStatus PK11_PubEncryptRaw(SECKEYPublicKey *key, unsigned char *enc,
                unsigned char *data, unsigned dataLen, void *wincx);

SECStatus PK11_PrivDecryptPKCS1(SECKEYPrivateKey *key, unsigned char *data,
   unsigned *outLen, unsigned int maxLen, unsigned char *enc, unsigned encLen);
/* The encrypt function that complements the above decrypt function. */
SECStatus PK11_PubEncryptPKCS1(SECKEYPublicKey *key, unsigned char *enc,
                unsigned char *data, unsigned dataLen, void *wincx);

SECStatus PK11_ImportPrivateKeyInfo(PK11SlotInfo *slot, 
		SECKEYPrivateKeyInfo *pki, SECItem *nickname,
		SECItem *publicValue, PRBool isPerm, PRBool isPrivate,
		unsigned int usage, void *wincx);
SECStatus PK11_ImportPrivateKeyInfoAndReturnKey(PK11SlotInfo *slot, 
		SECKEYPrivateKeyInfo *pki, SECItem *nickname,
		SECItem *publicValue, PRBool isPerm, PRBool isPrivate,
		unsigned int usage, SECKEYPrivateKey** privk, void *wincx);
SECStatus PK11_ImportDERPrivateKeyInfo(PK11SlotInfo *slot, 
		SECItem *derPKI, SECItem *nickname,
		SECItem *publicValue, PRBool isPerm, PRBool isPrivate,
		unsigned int usage, void *wincx);
SECStatus PK11_ImportDERPrivateKeyInfoAndReturnKey(PK11SlotInfo *slot, 
		SECItem *derPKI, SECItem *nickname,
		SECItem *publicValue, PRBool isPerm, PRBool isPrivate,
		unsigned int usage, SECKEYPrivateKey** privk, void *wincx);
SECStatus PK11_ImportEncryptedPrivateKeyInfo(PK11SlotInfo *slot, 
		SECKEYEncryptedPrivateKeyInfo *epki, SECItem *pwitem, 
		SECItem *nickname, SECItem *publicValue, PRBool isPerm,
		PRBool isPrivate, KeyType type, 
		unsigned int usage, void *wincx);
SECKEYPrivateKeyInfo *PK11_ExportPrivateKeyInfo(
		CERTCertificate *cert, void *wincx);
SECKEYEncryptedPrivateKeyInfo *PK11_ExportEncryptedPrivKeyInfo(
		PK11SlotInfo *slot, SECOidTag algTag, SECItem *pwitem,
		SECKEYPrivateKey *pk, int iteration, void *wincx);
SECKEYEncryptedPrivateKeyInfo *PK11_ExportEncryptedPrivateKeyInfo(
		PK11SlotInfo *slot, SECOidTag algTag, SECItem *pwitem,
		CERTCertificate *cert, int iteration, void *wincx);
SECKEYPrivateKey *PK11_FindKeyByDERCert(PK11SlotInfo *slot, 
					CERTCertificate *cert, void *wincx);
SECKEYPublicKey *PK11_MakeKEAPubKey(unsigned char *data, int length);
SECStatus PK11_DigestKey(PK11Context *context, PK11SymKey *key);
PRBool PK11_VerifyKeyOK(PK11SymKey *key);
SECKEYPrivateKey *PK11_UnwrapPrivKey(PK11SlotInfo *slot, 
		PK11SymKey *wrappingKey, CK_MECHANISM_TYPE wrapType,
		SECItem *param, SECItem *wrappedKey, SECItem *label, 
		SECItem *publicValue, PRBool token, PRBool sensitive,
		CK_KEY_TYPE keyType, CK_ATTRIBUTE_TYPE *usage, int usageCount,
		void *wincx);
SECStatus PK11_WrapPrivKey(PK11SlotInfo *slot, PK11SymKey *wrappingKey,
			   SECKEYPrivateKey *privKey, CK_MECHANISM_TYPE wrapType,
			   SECItem *param, SECItem *wrappedKey, void *wincx);
SECItem* PK11_DEREncodePublicKey(SECKEYPublicKey *pubk);
PK11SymKey* PK11_CopySymKeyForSigning(PK11SymKey *originalKey,
	CK_MECHANISM_TYPE mech);
SECKEYPrivateKeyList* PK11_ListPrivKeysInSlot(PK11SlotInfo *slot,
						 char *nickname, void *wincx);
SECKEYPublicKeyList* PK11_ListPublicKeysInSlot(PK11SlotInfo *slot,
							char *nickname);
SECKEYPQGParams *PK11_GetPQGParamsFromPrivateKey(SECKEYPrivateKey *privKey);
/* depricated */
SECKEYPrivateKeyList* PK11_ListPrivateKeysInSlot(PK11SlotInfo *slot);

PK11SymKey *PK11_ConvertSessionSymKeyToTokenSymKey(PK11SymKey *symk,
	void *wincx);
SECKEYPrivateKey *PK11_ConvertSessionPrivKeyToTokenPrivKey(
	SECKEYPrivateKey *privk, void* wincx);

/**********************************************************************
 *                   Certs
 **********************************************************************/
SECItem *PK11_MakeIDFromPubKey(SECItem *pubKeyData);
SECStatus PK11_TraverseSlotCerts(
     SECStatus(* callback)(CERTCertificate*,SECItem *,void *),
                                                void *arg, void *wincx);
CERTCertificate * PK11_FindCertFromNickname(char *nickname, void *wincx);
CERTCertList * PK11_FindCertsFromNickname(char *nickname, void *wincx);
CERTCertificate *PK11_GetCertFromPrivateKey(SECKEYPrivateKey *privKey);
SECStatus PK11_ImportCert(PK11SlotInfo *slot, CERTCertificate *cert,
                CK_OBJECT_HANDLE key, char *nickname, PRBool includeTrust);
SECStatus PK11_ImportDERCert(PK11SlotInfo *slot, SECItem *derCert,
                CK_OBJECT_HANDLE key, char *nickname, PRBool includeTrust);
PK11SlotInfo *PK11_ImportCertForKey(CERTCertificate *cert, char *nickname,
								void *wincx);
PK11SlotInfo *PK11_ImportDERCertForKey(SECItem *derCert, char *nickname,
								void *wincx);
PK11SlotInfo *PK11_KeyForCertExists(CERTCertificate *cert,
					CK_OBJECT_HANDLE *keyPtr, void *wincx);
PK11SlotInfo *PK11_KeyForDERCertExists(SECItem *derCert,
					CK_OBJECT_HANDLE *keyPtr, void *wincx);
CERTCertificate * PK11_FindCertByIssuerAndSN(PK11SlotInfo **slot,
					CERTIssuerAndSN *sn, void *wincx);
CERTCertificate * PK11_FindCertAndKeyByRecipientList(PK11SlotInfo **slot,
	SEC_PKCS7RecipientInfo **array, SEC_PKCS7RecipientInfo **rip,
				SECKEYPrivateKey**privKey, void *wincx);
int PK11_FindCertAndKeyByRecipientListNew(NSSCMSRecipient **recipientlist,
				void *wincx);
SECStatus PK11_TraverseCertsForSubjectInSlot(CERTCertificate *cert,
	PK11SlotInfo *slot, SECStatus(*callback)(CERTCertificate *, void *),
	void *arg);
CERTCertificate *PK11_FindCertFromDERCert(PK11SlotInfo *slot, 
					  CERTCertificate *cert, void *wincx);
SECStatus PK11_ImportCertForKeyToSlot(PK11SlotInfo *slot, CERTCertificate *cert,
					char *nickname, PRBool addUsage,
					void *wincx);
CERTCertificate *PK11_FindBestKEAMatch(CERTCertificate *serverCert,void *wincx);
PRBool PK11_FortezzaHasKEA(CERTCertificate *cert);
CK_OBJECT_HANDLE PK11_FindCertInSlot(PK11SlotInfo *slot, CERTCertificate *cert,
				     void *wincx);
SECStatus PK11_TraverseCertsForNicknameInSlot(SECItem *nickname,
	PK11SlotInfo *slot, SECStatus(*callback)(CERTCertificate *, void *),
	void *arg);
CERTCertList * PK11_ListCerts(PK11CertListType type, void *pwarg);
CERTCertList * PK11_ListCertsInSlot(PK11SlotInfo *slot);
CERTSignedCrl* PK11_ImportCRL(PK11SlotInfo * slot, SECItem *derCRL, char *url,
    int type, void *wincx, PRInt32 importOptions, PRArenaPool* arena, PRInt32 decodeOptions);

/**********************************************************************
 *                   Sign/Verify 
 **********************************************************************/
int PK11_SignatureLen(SECKEYPrivateKey *key);
PK11SlotInfo * PK11_GetSlotFromPrivateKey(SECKEYPrivateKey *key);
SECStatus PK11_Sign(SECKEYPrivateKey *key, SECItem *sig, SECItem *hash);
SECStatus PK11_VerifyRecover(SECKEYPublicKey *key, SECItem *sig,
						 SECItem *dsig, void * wincx);
SECStatus PK11_Verify(SECKEYPublicKey *key, SECItem *sig, 
						SECItem *hash, void *wincx);



/**********************************************************************
 *                   Crypto Contexts
 **********************************************************************/
void PK11_DestroyContext(PK11Context *context, PRBool freeit);
PK11Context *PK11_CreateContextBySymKey(CK_MECHANISM_TYPE type,
	CK_ATTRIBUTE_TYPE operation, PK11SymKey *symKey, SECItem *param);
PK11Context *PK11_CreateDigestContext(SECOidTag hashAlg);
PK11Context *PK11_CloneContext(PK11Context *old);
SECStatus PK11_DigestBegin(PK11Context *cx);
SECStatus PK11_HashBuf(SECOidTag hashAlg, unsigned char *out, unsigned char *in,
					int32 len);
SECStatus PK11_DigestOp(PK11Context *context, const unsigned char *in, 
                        unsigned len);
SECStatus PK11_CipherOp(PK11Context *context, unsigned char * out, int *outlen, 
				int maxout, unsigned char *in, int inlen);
SECStatus PK11_Finalize(PK11Context *context);
SECStatus PK11_DigestFinal(PK11Context *context, unsigned char *data, 
				unsigned int *outLen, unsigned int length);
SECStatus PK11_SaveContext(PK11Context *cx,unsigned char *save,
						int *len, int saveLength);

/* Save the context's state, with possible allocation.
 * The caller may supply an already allocated buffer in preAllocBuf,
 * with length pabLen.  If the buffer is large enough for the context's
 * state, it will receive the state.
 * If the buffer is not large enough (or NULL), then a new buffer will
 * be allocated with PORT_Alloc.
 * In either case, the state will be returned as a buffer, and the length
 * of the state will be given in *stateLen.
 */
unsigned char *
PK11_SaveContextAlloc(PK11Context *cx,
                      unsigned char *preAllocBuf, unsigned int pabLen,
                      unsigned int *stateLen);

SECStatus PK11_RestoreContext(PK11Context *cx,unsigned char *save,int len);
SECStatus PK11_GenerateFortezzaIV(PK11SymKey *symKey,unsigned char *iv,int len);
void PK11_SetFortezzaHack(PK11SymKey *symKey) ;


/**********************************************************************
 *                   PBE functions 
 **********************************************************************/

/* This function creates PBE parameters from the given inputs.  The result
 * can be used to create a password integrity key for PKCS#12, by sending
 * the return value to PK11_KeyGen along with the appropriate mechanism.
 */
SECItem * 
PK11_CreatePBEParams(SECItem *salt, SECItem *pwd, unsigned int iterations);

/* free params created above (can be called after keygen is done */
void PK11_DestroyPBEParams(SECItem *params);

SECAlgorithmID *
PK11_CreatePBEAlgorithmID(SECOidTag algorithm, int iteration, SECItem *salt);
PK11SymKey *
PK11_PBEKeyGen(PK11SlotInfo *slot, SECAlgorithmID *algid,  SECItem *pwitem,
	       PRBool faulty3DES, void *wincx);
PK11SymKey *
PK11_RawPBEKeyGen(PK11SlotInfo *slot, CK_MECHANISM_TYPE type, SECItem *params,
		SECItem *pwitem, PRBool faulty3DES, void *wincx);
SECItem *
PK11_GetPBEIV(SECAlgorithmID *algid, SECItem *pwitem);

/**********************************************************************
 * Functions to manage secmod flags
 **********************************************************************/
PK11DefaultArrayEntry * PK11_GetDefaultArray(int *);
SECStatus PK11_UpdateSlotAttribute(PK11SlotInfo *, PK11DefaultArrayEntry *,
							PRBool );

/**********************************************************************
 * Functions to look at PKCS #11 dependent data
 **********************************************************************/
PK11GenericObject *PK11_FindGenericObjects(PK11SlotInfo *slot, 
						CK_OBJECT_CLASS objClass);
PK11GenericObject *PK11_GetNextGenericObject(PK11GenericObject *object);
PK11GenericObject *PK11_GetPrevtGenericObject(PK11GenericObject *object);
SECStatus PK11_UnlinkGenericObject(PK11GenericObject *object);
SECStatus PK11_LinkGenericObject(PK11GenericObject *list,
				 PK11GenericObject *object);
SECStatus PK11_DestroyGenericObjects(PK11GenericObject *object);
SECStatus PK11_DestroyGenericObject(PK11GenericObject *object);
SECStatus PK11_ReadRawAttribute(PK11ObjectType type, void *object, 
				CK_ATTRIBUTE_TYPE attr, SECItem *item);


/**********************************************************************
 * New fucntions which are already depricated....
 **********************************************************************/
SECItem *
PK11_GetLowLevelKeyIDForCert(PK11SlotInfo *slot,
					CERTCertificate *cert, void *pwarg);
SECItem *
PK11_GetLowLevelKeyIDForPrivateKey(SECKEYPrivateKey *key);

PRBool SECMOD_HasRootCerts(void);

SEC_END_PROTOS

#endif
