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
#include "fortsock.h"
#include "fpkmem.h"
#include "fmutex.h"
#include <string.h>
#include <stdlib.h>

#define DEF_ENCRYPT_SIZE 0x8000

static unsigned char Fortezza_mail_Rb[128] = { 
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1,
};

int InitSocket (FortezzaSocket *inSocket, int inSlotID) {
    int        ci_rv;
    CK_RV      mrv;

    if (inSocket == NULL)
        return SOCKET_FAILURE;

    inSocket->isLoggedIn          = PR_FALSE;
    inSocket->personalitiesLoaded = PR_FALSE;
    inSocket->isOpen              = PR_FALSE;
    inSocket->personalityList     = NULL;
    inSocket->keyRegisters        = NULL;
    inSocket->keys                = NULL;
    inSocket->numPersonalities    = 0;
    inSocket->numKeyRegisters     = 0;
    inSocket->hitCount            = 0;

    inSocket->slotID = inSlotID;
    ci_rv = MACI_GetSessionID(&(inSocket->maciSession));
    if (ci_rv != CI_OK)
      return SOCKET_FAILURE;

    ci_rv = MACI_Open (inSocket->maciSession, 0, inSlotID);
    if (ci_rv == CI_OK) { 
        inSocket->isOpen = PR_TRUE;
    }  else {
        MACI_Close (inSocket->maciSession, CI_NULL_FLAG, inSlotID);
    }

    if (FMUTEX_MutexEnabled()) {
        mrv = FMUTEX_Create(&inSocket->registersLock);
	if (mrv != CKR_OK) {
	    inSocket->registersLock = NULL;
	}
    } else {
        inSocket->registersLock = NULL;
    }

    return SOCKET_SUCCESS;
}

int FreeSocket (FortezzaSocket *inSocket) {
    if (inSocket->registersLock) {
        FMUTEX_Destroy(inSocket->registersLock);
    }
    MACI_Close(inSocket->maciSession, CI_NULL_FLAG, inSocket->slotID);
    return SOCKET_SUCCESS;
}

int LoginToSocket (FortezzaSocket *inSocket, int inUserType, CI_PIN inPin) {
    int ci_rv, i;
    CI_STATUS ciStatus;
    CI_CONFIG ciConfig;
    FortezzaKey **oldRegisters, **newRegisters;
    int oldCount;
    HSESSION hs;

    if (inSocket == NULL || inSocket->isLoggedIn)
        return SOCKET_FAILURE;

    hs = inSocket->maciSession;
    ci_rv = MACI_Select (hs, inSocket->slotID);
    if (ci_rv != CI_OK)
        return ci_rv;
    
    ci_rv = MACI_CheckPIN(hs, inUserType, inPin);

    if (ci_rv != CI_OK) {
        return ci_rv;
    }

    ci_rv = MACI_GetStatus(hs, &ciStatus);

    if (ci_rv != CI_OK) {
        if (ci_rv == CI_FAIL) {
	    ci_rv = CI_EXEC_FAIL;
	}
	return ci_rv;
    }

    ci_rv = MACI_GetConfiguration(hs, &ciConfig);
    if (ci_rv != CI_OK) {
      return ci_rv;
    }

    inSocket->isLoggedIn  = PR_TRUE;
    inSocket->hasLoggedIn = PR_TRUE;
    PORT_Memcpy (inSocket->openCardSerial, ciStatus.SerialNumber, 
		 sizeof (CI_SERIAL_NUMBER));
    inSocket->openCardState = ciStatus.CurrentState;
    inSocket->numPersonalities = ciStatus.CertificateCount;
    inSocket->numKeyRegisters  = ciConfig.KeyRegisterCount;
    newRegisters     = 
     (FortezzaKey**)PORT_Alloc (sizeof(FortezzaKey)*ciConfig.KeyRegisterCount);

    FMUTEX_Lock(inSocket->registersLock);
    oldRegisters = inSocket->keyRegisters;
    oldCount = inSocket->numKeyRegisters;
    inSocket->keyRegisters = newRegisters;
    if (oldRegisters) {
	for (i=0; i<oldCount; i++) {
	    if (oldRegisters[i]) {
		oldRegisters[i]->keyRegister = KeyNotLoaded;
	    }
            oldRegisters[i] = NULL;
	}
	PORT_Free(oldRegisters);
    }

    if (inSocket->keyRegisters == NULL) {
        FMUTEX_Unlock(inSocket->registersLock);
        return SOCKET_FAILURE;
    }

    for (i=0; i<ciConfig.KeyRegisterCount; i++) {
        inSocket->keyRegisters[i] = NULL;
    }
    FMUTEX_Unlock(inSocket->registersLock);

    return SOCKET_SUCCESS;
}

int LogoutFromSocket (FortezzaSocket *inSocket) {
    if (inSocket == NULL)
        return SOCKET_FAILURE;

    inSocket->isLoggedIn  = PR_FALSE;
    inSocket->hasLoggedIn = PR_FALSE;
    if (UnloadPersonalityList(inSocket) != SOCKET_SUCCESS)
        return SOCKET_FAILURE;
    

    return SOCKET_SUCCESS;
}


int FetchPersonalityList(FortezzaSocket *inSocket) {
    int rv;

    if (inSocket == NULL || inSocket->numPersonalities == 0) {
        return SOCKET_FAILURE;
    }

    rv = MACI_Select (inSocket->maciSession, inSocket->slotID);

    inSocket->personalityList = 
        (CI_PERSON*)PORT_Alloc (sizeof(CI_PERSON)*inSocket->numPersonalities);

    if (inSocket->personalityList == NULL) {
        return SOCKET_FAILURE;
    }

    rv = MACI_GetPersonalityList(inSocket->maciSession, 
				 inSocket->numPersonalities, 
				 inSocket->personalityList);

    if (rv != CI_OK) {
        return SOCKET_FAILURE;
    }

    inSocket->personalitiesLoaded = PR_TRUE;
    return SOCKET_SUCCESS;
}

int UnloadPersonalityList(FortezzaSocket *inSocket) {
    if (inSocket == NULL)
        return SOCKET_FAILURE;

    inSocket->personalitiesLoaded = PR_FALSE;
    if (inSocket->personalityList) {
        PORT_Free(inSocket->personalityList);
    }
    inSocket->numPersonalities = 0;
    inSocket->personalityList = NULL;

    return SOCKET_SUCCESS;
}

PRBool SocketIsLoggedIn(CI_STATE status) {
	
    return (PRBool)((status == CI_READY) || (status == CI_STANDBY));
}

PRBool SocketStateUnchanged(FortezzaSocket* inSocket) {
    CI_STATUS ciStatus;
    int       ciRV;

    ciRV = MACI_Select (inSocket->maciSession, inSocket->slotID);
    if (ciRV != CI_OK)
        return PR_FALSE;

    if (inSocket->hasLoggedIn && !inSocket->isLoggedIn) 
        return PR_FALSE;  /* User Logged out from the socket */

    /*
     * Some vendor cards are slow. so if we think we are logged in, 
     * and the card still thinks we're logged in, we must have the same
     * card.
     */
    if (inSocket->isLoggedIn) {
	CI_STATE state;
	ciRV = MACI_GetState(inSocket->maciSession, &state);
	if (ciRV != CI_OK) return PR_FALSE;

	return SocketIsLoggedIn(state);
    }
	
    ciRV = MACI_GetStatus(inSocket->maciSession, &ciStatus);
    if(ciRV != CI_OK) {
        return PR_FALSE;
    }
    if (inSocket->isLoggedIn) {
 	if (PORT_Memcmp(ciStatus.SerialNumber, inSocket->openCardSerial, 
			sizeof (CI_SERIAL_NUMBER)) != 0)
	  return PR_FALSE;  /* Serial Number of card in slot has changed */
	                    /* Probably means there is a new card        */
    }
    
    if (inSocket->isLoggedIn  && !SocketIsLoggedIn(ciStatus.CurrentState))
        return PR_FALSE;  /* State of card changed.         */
                         /* Probably re-inserted same card */ 

    return PR_TRUE;  /* No change in the state of the socket */
}

/*
 * can we regenerate this key on the fly?
 */
static PRBool
FortezzaIsRegenerating(FortezzaKey *key) {
    /* TEK's are the only type of key that can't be regenerated */
    if (key->keyType != TEK) return PR_TRUE;
    /* Client TEK's can never be regenerated */
    if (key->keyData.tek.flags == CI_INITIATOR_FLAG) return PR_FALSE; 
    /* Only Server TEK's that use the Mail protocol can be regenerated */
    return ((PRBool)(memcmp(key->keyData.tek.Rb,Fortezza_mail_Rb,
			sizeof(key->keyData.tek.Rb)) == 0));
}

int GetBestKeyRegister(FortezzaSocket *inSocket) {
    int i, candidate = -1, candidate2 = 1;
    CK_ULONG minHitCount = 0xffffffff;
    CK_ULONG minRegHitCount = 0xffffffff;  
    FortezzaKey **registers;

    registers = inSocket->keyRegisters;
    for (i=1; i< inSocket->numKeyRegisters; i++) {
        if (registers[i] == NULL)
	    return i;
    }

    for (i=1; i < inSocket->numKeyRegisters; i++) {
        if (registers[i]->hitCount < minHitCount) {
	    minHitCount = registers[i]->hitCount;
	    candidate2 = i;
	}

	if (FortezzaIsRegenerating(registers[i]) &&
	    (registers[i]->hitCount < minRegHitCount)) {
	    minRegHitCount = registers[i]->hitCount;
	    candidate = i;
	}
    }

    if (candidate == -1)
        candidate = candidate2;

    return candidate;
}

int  SetFortezzaKeyHandle (FortezzaKey *inKey, CK_OBJECT_HANDLE inHandle) {
    inKey->keyHandle = inHandle;
    return SOCKET_SUCCESS;
}

void
RemoveKey (FortezzaKey *inKey) {
    if (inKey != NULL && inKey->keySocket->keyRegisters != NULL) {
	if (inKey->keyRegister != KeyNotLoaded) {
	    FortezzaKey **registers = inKey->keySocket->keyRegisters;
	    registers[inKey->keyRegister] = NULL;
	    MACI_DeleteKey(inKey->keySocket->maciSession, inKey->keyRegister);
	}
	
	PORT_Free(inKey);
    }
}

FortezzaKey *NewFortezzaKey(FortezzaSocket  *inSocket, 
			    FortezzaKeyType  inKeyType,
			    CreateTEKInfo   *TEKinfo,
			    int              inKeyRegister) {
    FortezzaKey  *newKey, *oldKey;
    FortezzaKey **registers;
    HSESSION      hs = inSocket->maciSession;
    int ciRV;

    newKey = (FortezzaKey*)PORT_Alloc (sizeof(FortezzaKey));
    if (newKey == NULL) {
        return NULL;
    }

    newKey->keyHandle    = 0;
    newKey->keyRegister  = KeyNotLoaded;
    newKey->keyType      = inKeyType;
    newKey->keySocket    = inSocket;
    newKey->hitCount     = 0;
    newKey->id           = TEKinfo ? TEKinfo->personality : 0;

    if (inKeyType != Ks && inSocket->keyRegisters) {
	registers = inSocket->keyRegisters;
	oldKey    = registers[inKeyRegister];
	if (oldKey != NULL) {
	    oldKey->keyRegister = KeyNotLoaded;
	}

	registers[inKeyRegister] = newKey;
	newKey->hitCount = inSocket->hitCount++;

	MACI_DeleteKey (hs, inKeyRegister);
    }
    newKey->keyRegister = inKeyRegister;

    MACI_Lock(hs, CI_BLOCK_LOCK_FLAG);
    switch (inKeyType) {
    case MEK:
        ciRV = MACI_GenerateMEK (hs, inKeyRegister, 0);
	if (ciRV != CI_OK) {
	    RemoveKey(newKey);
	    MACI_Unlock(hs);
	    return NULL;
        }
	MACI_WrapKey(hs, 0, inKeyRegister, newKey->keyData.mek);
	break;
    case TEK:
        PORT_Memcpy (newKey->keyData.tek.Rb, TEKinfo->Rb, TEKinfo->randomLen);
	PORT_Memcpy (newKey->keyData.tek.Ra, TEKinfo->Ra, TEKinfo->randomLen);
	PORT_Memcpy (newKey->keyData.tek.pY, TEKinfo->pY, TEKinfo->YSize);
	newKey->keyData.tek.ySize = TEKinfo->YSize;
	newKey->keyData.tek.randomLen = TEKinfo->randomLen;
	newKey->keyData.tek.registerIndex = TEKinfo->personality;
	newKey->keyData.tek.flags = TEKinfo->flag;

	ciRV = MACI_SetPersonality(hs,TEKinfo->personality);
	if (ciRV != CI_OK) {
	    RemoveKey(newKey);
	    MACI_Unlock(hs);
	    return NULL;
	}
	ciRV = MACI_GenerateTEK(hs, TEKinfo->flag, inKeyRegister,
			 newKey->keyData.tek.Ra, TEKinfo->Rb, 
			 TEKinfo->YSize, TEKinfo->pY);
	if (ciRV != CI_OK) {
	    RemoveKey(newKey);
	    MACI_Unlock(hs);
	    return NULL;
	}

	
        break;
    case Ks:
	break;
    default:
        RemoveKey(newKey);
	MACI_Unlock(hs);
	return NULL;
    }
    MACI_Unlock(hs);
    return newKey;
}

FortezzaKey *NewUnwrappedKey(int inKeyRegister, int id,
			     FortezzaSocket *inSocket) {
    FortezzaKey *newKey;

    newKey = (FortezzaKey*)PORT_Alloc (sizeof(FortezzaKey));
    if (newKey == NULL) {
        return NULL;
    }
 
    newKey->keyRegister = inKeyRegister;
    newKey->keyType     = UNWRAP;
    newKey->keySocket   = inSocket;
    newKey->id          = id;
    newKey->hitCount    = inSocket->hitCount++; 
    MACI_WrapKey(inSocket->maciSession,0 , inKeyRegister, newKey->keyData.mek);
    inSocket->keyRegisters[inKeyRegister] = newKey;

    return newKey;
}

int LoadKeyIntoRegister (FortezzaKey *inKey) {
    int              registerIndex = GetBestKeyRegister(inKey->keySocket);
    FortezzaSocket  *socket        = inKey->keySocket;
    FortezzaKey    **registers     = socket->keyRegisters;
    HSESSION         hs            = socket->maciSession;
    FortezzaTEK     *tek           = &inKey->keyData.tek;
    FortezzaKey     *oldKey;
    int              rv = CI_FAIL;

    if (inKey->keyRegister != KeyNotLoaded) {
        return inKey->keyRegister;
    }

    oldKey = registers[registerIndex];

    MACI_Select(hs, socket->slotID);
    if (oldKey) {
        oldKey->keyRegister = KeyNotLoaded;
    }
    MACI_DeleteKey (hs, registerIndex);

    switch (inKey->keyType) {
    case TEK:
        if (!FortezzaIsRegenerating(inKey)) {
	  return KeyNotLoaded;
	}
        if (MACI_SetPersonality(hs, tek->registerIndex) == CI_OK) {
	    rv = MACI_GenerateTEK (hs, tek->flags, registerIndex, 
				   tek->Ra, tek->Rb, tek->ySize, 
				   tek->pY);   
	} 
	if (rv != CI_OK)
	    return KeyNotLoaded;
	break;
    case MEK:
    case UNWRAP:
        rv = MACI_UnwrapKey (hs, 0, registerIndex, inKey->keyData.mek);
	if (rv != CI_OK) 
	    return KeyNotLoaded;
	break;
    default:
        return KeyNotLoaded;
    }
    inKey->keyRegister = registerIndex;
    registers[registerIndex] = inKey;
 
    return registerIndex;
}

int InitCryptoOperation (FortezzaContext *inContext, 
			 CryptoType inCryptoOperation) {
    inContext->cryptoOperation = inCryptoOperation;
    return SOCKET_SUCCESS;
}

int EndCryptoOperation (FortezzaContext *inContext, 
			CryptoType inCryptoOperation) {
    if (inCryptoOperation != inContext->cryptoOperation) {
      return SOCKET_FAILURE;
    }
    inContext->cryptoOperation = None;
    return SOCKET_SUCCESS;
}

CryptoType GetCryptoOperation (FortezzaContext *inContext) {
    return inContext->cryptoOperation;
}

void InitContext(FortezzaContext *inContext, FortezzaSocket *inSocket,
		 CK_OBJECT_HANDLE hKey) {
    inContext->fortezzaKey     = NULL;
    inContext->fortezzaSocket  = inSocket;
    inContext->session         = NULL;
    inContext->mechanism       = NO_MECHANISM;
    inContext->userRamSize     = 0;
    inContext->cryptoOperation = None;
    inContext->hKey            = hKey;
}

extern PRBool fort11_FortezzaIsUserCert(unsigned char *label);

static int
GetValidPersonality (FortezzaSocket *inSocket) {
    int index = -1; /* return an invalid personalidyt if one isn't found */
    int i;
    PRBool unLoadList = PR_FALSE;
    int numPersonalities = 0;

    if (!inSocket->personalitiesLoaded) {
        numPersonalities = inSocket->numPersonalities;
        FetchPersonalityList (inSocket);
	unLoadList = PR_TRUE;
    }

    for (i=0; i<inSocket->numPersonalities; i++) {
        if (fort11_FortezzaIsUserCert(inSocket->personalityList[i].CertLabel)) {
	    index = inSocket->personalityList[i].CertificateIndex;
	    break;
	}
    }

    if (unLoadList) {
        UnloadPersonalityList(inSocket);
	/* UnloadPersonality sets numPersonalities to zero,
	 * so we set it back to what it was when this function
	 * was called.
	 */
	inSocket->numPersonalities = numPersonalities;
    }
    return index;
}

int RestoreState (FortezzaContext *inContext, CryptoType inType) {
    FortezzaKey    *key    = inContext->fortezzaKey;
    FortezzaSocket *socket = inContext->fortezzaSocket;
    HSESSION        hs     = socket->maciSession; 
    CI_IV           bogus_iv;
    int             rv, cryptoType = -1;
    int             personality = inContext->fortezzaKey->id;

    if (key == NULL)
        return SOCKET_FAILURE;

    if (personality == 0) {
        personality = GetValidPersonality (socket);
    }
    rv = MACI_SetPersonality(hs, personality);
    if (rv != CI_OK) {
        return SOCKET_FAILURE;
    }
    /*
     * The cards need to have some state bits set because
     * save and restore don't necessarily save all the state.
     * Instead of fixing the cards, they decided to change the
     * protocol :(.
     */
    switch (inType) {
    case Encrypt:
        rv = MACI_SetKey(hs, key->keyRegister);
	if (rv != CI_OK)
	    break;
	rv = MACI_GenerateIV (hs, bogus_iv);
	cryptoType = CI_ENCRYPT_EXT_TYPE;
	break;
    case Decrypt:
	rv = MACI_SetKey(hs, key->keyRegister);
        rv = MACI_LoadIV (hs, inContext->cardIV);
	cryptoType = CI_DECRYPT_EXT_TYPE;
	break;
    default:
      rv = CI_INV_POINTER;
      break;
    }

    if (rv != CI_OK) {
        return SOCKET_FAILURE;
    }
    /*PORT_Assert(cryptoType != -1); */

    rv = MACI_Restore(hs, cryptoType, inContext->cardState);
    if (rv != CI_OK) {
        return SOCKET_FAILURE;
    }

    return SOCKET_SUCCESS;
}

int SaveState (FortezzaContext *inContext, CI_IV inIV, 
	       PK11Session *inSession, FortezzaKey *inKey,
	       int inCryptoType, CK_MECHANISM_TYPE inMechanism){
    int             ciRV;
    FortezzaSocket *socket = inContext->fortezzaSocket;
    HSESSION        hs     = socket->maciSession;
    CI_CONFIG       ciConfig;

    ciRV = MACI_Select (hs, socket->slotID);
    if (ciRV != CI_OK) {
        return SOCKET_FAILURE;
    }
    inContext->session     = inSession;
    inContext->fortezzaKey = inKey;
    inContext->mechanism   = inMechanism;
    PORT_Memcpy (inContext->cardIV, inIV, sizeof (CI_IV));
    ciRV = MACI_Save(hs, inCryptoType, inContext->cardState);
    if (ciRV != CI_OK) {
        return SOCKET_FAILURE;
    }
    ciRV = MACI_GetConfiguration (hs, &ciConfig);
    if (ciRV == CI_OK) {
      inContext->userRamSize = ciConfig.LargestBlockSize;
    }

    if (inContext->userRamSize == 0) inContext->userRamSize = 0x4000;
    
    return SOCKET_SUCCESS;
}

int SocketSaveState (FortezzaContext *inContext, int inCryptoType) {
    int ciRV;

    ciRV = MACI_Save (inContext->fortezzaSocket->maciSession, inCryptoType, 
		      inContext->cardState);
    if (ciRV != CI_OK) {
        return SOCKET_FAILURE;
    }
    return SOCKET_SUCCESS;
}

int DecryptData (FortezzaContext *inContext, 
		 CK_BYTE_PTR inData,
		 CK_ULONG inDataLen, 
		 CK_BYTE_PTR inDest, 
		 CK_ULONG inDestLen) {
    FortezzaSocket *socket = inContext->fortezzaSocket;
    FortezzaKey    *key    = inContext->fortezzaKey;
    HSESSION        hs     = socket->maciSession;
    CK_ULONG        defaultEncryptSize;
    CK_ULONG        left = inDataLen;
    CK_BYTE_PTR    loopin, loopout;
    int             rv = CI_OK;    
    
    MACI_Select (hs, socket->slotID);

    defaultEncryptSize = (inContext->userRamSize > DEF_ENCRYPT_SIZE) 
                          ? DEF_ENCRYPT_SIZE : inContext->userRamSize;

    if (key->keyRegister == KeyNotLoaded) {
        rv = LoadKeyIntoRegister(key);
	if (rv == KeyNotLoaded) {
	    return SOCKET_FAILURE;
	}
    }

    key->hitCount = socket->hitCount++;
    loopin  = inData; 
    loopout = inDest;
    left    = inDataLen;
    rv = CI_OK;

    MACI_Lock(hs, CI_BLOCK_LOCK_FLAG);
    RestoreState (inContext, Decrypt);

    while ((left > 0) && (rv  == CI_OK)) {
        CK_ULONG current = (left > defaultEncryptSize) 
	                         ? defaultEncryptSize : left;
	rv = MACI_Decrypt(hs, current, loopin, loopout);
	loopin  += current;
	loopout += current;
	left    -= current;
    }

    MACI_Unlock(hs);

    if (rv != CI_OK) {
        return SOCKET_FAILURE;
    }


    rv = SocketSaveState (inContext, CI_DECRYPT_EXT_TYPE);
    if (rv != SOCKET_SUCCESS) {
      return rv;
    }


    return SOCKET_SUCCESS;
}

int EncryptData (FortezzaContext *inContext, 
		 CK_BYTE_PTR inData,
		 CK_ULONG inDataLen, 
		 CK_BYTE_PTR inDest, 
		 CK_ULONG inDestLen) {
    FortezzaSocket *socket = inContext->fortezzaSocket;
    FortezzaKey    *key    = inContext->fortezzaKey;
    HSESSION        hs     = socket->maciSession;
    CK_ULONG        defaultEncryptSize;
    CK_ULONG        left = inDataLen;
    CK_BYTE_PTR    loopin, loopout;
    int             rv = CI_OK;
    
    MACI_Select (hs, socket->slotID);

    defaultEncryptSize = (inContext->userRamSize > DEF_ENCRYPT_SIZE) 
                          ? DEF_ENCRYPT_SIZE : inContext->userRamSize;
    if (key->keyRegister == KeyNotLoaded) {
        rv = LoadKeyIntoRegister(key);
	if (rv == KeyNotLoaded) {
	    return rv;
	}
    }

    key->hitCount = socket->hitCount++;
    loopin  = inData;
    loopout = inDest;

    RestoreState (inContext,Encrypt);

    rv = CI_OK;
    while ((left > 0) && (rv == CI_OK)) {
      CK_ULONG current = (left > defaultEncryptSize) ? defaultEncryptSize : 
	                                               left;
      rv = MACI_Encrypt(hs, current, loopin, loopout);
      loopin  += current;
      loopout += current; 
      left    -= current;
    }

    if (rv != CI_OK) {
        return SOCKET_FAILURE;
    }

    rv = SocketSaveState (inContext, CI_ENCRYPT_EXT_TYPE);
    if (rv != SOCKET_SUCCESS) {
      return rv;
    }

    return SOCKET_SUCCESS;
}

int WrapKey (FortezzaKey *wrappingKey, FortezzaKey *srcKey,
	     CK_BYTE_PTR pDest, CK_ULONG ulDestLen) {
    int ciRV;
    HSESSION hs = wrappingKey->keySocket->maciSession;

    if (wrappingKey->keyRegister == KeyNotLoaded) {
      if (LoadKeyIntoRegister(wrappingKey) == KeyNotLoaded) {
	  return SOCKET_FAILURE;
      }
    }

    if (srcKey->id == 0) srcKey->id = wrappingKey->id;

    ciRV = MACI_WrapKey (hs, wrappingKey->keyRegister, 
			 srcKey->keyRegister, pDest);
    if (ciRV != CI_OK) {
        return SOCKET_FAILURE;
    }

    return SOCKET_SUCCESS;
}

int UnwrapKey (CK_BYTE_PTR inWrappedKey, FortezzaKey *inUnwrapKey) {
    int newIndex;
    int ciRV;
    FortezzaSocket *socket = inUnwrapKey->keySocket;
    HSESSION        hs     = socket->maciSession;
    FortezzaKey    *oldKey;

    if (inUnwrapKey->keyRegister == KeyNotLoaded) {
        if (LoadKeyIntoRegister(inUnwrapKey) == KeyNotLoaded) {
	    return KeyNotLoaded;
	}
    }

    ciRV = MACI_Select(hs, socket->slotID);
    if (ciRV != CI_OK) {
        return KeyNotLoaded;
    }

    newIndex = GetBestKeyRegister(inUnwrapKey->keySocket);
    oldKey = socket->keyRegisters[newIndex];

    MACI_Select(hs, socket->slotID);
    if (oldKey) {
        oldKey->keyRegister = KeyNotLoaded;
        socket->keyRegisters[newIndex] = NULL;
    }
    MACI_DeleteKey (hs, newIndex);
    ciRV = MACI_UnwrapKey(hs,inUnwrapKey->keyRegister, newIndex, inWrappedKey);
    if (ciRV != CI_OK) {
        inUnwrapKey->keyRegister = KeyNotLoaded;
	socket->keyRegisters[newIndex] = NULL;
        return KeyNotLoaded;
    }
    
    return newIndex;
}

