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
/*
 * implement the MACI calls as Software Fortezza Calls.
 * only do the ones Nescape Needs. This provides a single software slot,
 * with 100 key registers, and 50 backup Ra private registers. Since we only
 * create one session per slot, this implementation only uses one session.
 * One future enhancement may be to try to improve on this for better threading
 * support.
 */

#include "prtypes.h"
#include "prio.h"

#include "swforti.h"
/*#include "keytlow.h"*/
/* #include "dh.h" */
#include "blapi.h"
#include "maci.h"
/* #include "dsa.h" */
/* #include "hasht.h" */
#include "secitem.h"
#include "secrng.h"
/*#include "keylow.h" */
#include "secder.h"

#ifdef XP_UNIX
#include <unistd.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif


/* currently we only support one software token. In the future we can use the
 * session to determin which of many possible tokens we are talking about.
 * all the calls which need tokens take a pointer to the software token as a
 * target.
 */
static FORTSWToken *swtoken = NULL;

#define SOCKET_ID 1


/* can't change the pin on SW fortezza for now */
int
MACI_ChangePIN(HSESSION session, int PINType, CI_PIN CI_FAR pOldPIN,
						 CI_PIN CI_FAR pNewPin)
{
    return CI_INV_STATE;
}


/*
 * Check pin checks the pin, then logs the user in or out depending on if
 * the pin succedes. The General implementation would support both SSO and 
 * User mode our's only needs User mode. Pins are checked by whether or not
 * they can produce our valid Ks for this 'card'.
 */
int
MACI_CheckPIN(HSESSION session, int PINType, CI_PIN CI_FAR pin)
{
    FORTSkipjackKeyPtr Ks;
    FORTSWFile *config_file = NULL;
    FORTSkipjackKey seed;
    unsigned char pinArea[13];
    unsigned char *padPin = NULL;

    /* This SW module can only log in as USER */
    if (PINType != CI_USER_PIN) return CI_INV_TYPE;

    if (swtoken == NULL) return CI_NO_CARD;
    /* we can't check a pin if we haven't been initialized yet */
    if (swtoken->config_file == NULL) return CI_NO_CARD;
    config_file = swtoken->config_file;

    /* Make sure the pin value meets minimum lengths */
    if (PORT_Strlen((char *)pin) < 12) {
	PORT_Memset(pinArea, ' ', sizeof(pinArea));
	PORT_Memcpy(pinArea,pin,PORT_Strlen((char *)pin));
	pinArea[12] = 0;
	padPin = pinArea;
    }

    /* get the Ks by unwrapping it from the memphrase with the pbe generated
     * from the pin */
    Ks = fort_CalculateKMemPhrase(config_file, 
			&config_file->fortezzaPhrase, (char *)pin, NULL);

    if (Ks == 0) {
	Ks = fort_CalculateKMemPhrase(config_file, 
		&config_file->fortezzaPhrase, (char *)padPin, NULL);
	if (Ks == 0) {
	    PORT_Memset(pinArea, 0, sizeof(pinArea));
	    fort_Logout(swtoken);
	    return CI_FAIL;
	}
    }

    /* use Ks and hash to verify that pin is correct */
    if (! fort_CheckMemPhrase(config_file, &config_file->fortezzaPhrase, 
						(char *)pin, Ks) ) {
	if ((padPin == NULL) ||
    	   ! fort_CheckMemPhrase(config_file, &config_file->fortezzaPhrase, 
						(char *)padPin, Ks) )  {
	    PORT_Memset(pinArea, 0, sizeof(pinArea));
	    fort_Logout(swtoken);
	    return CI_FAIL;
	}
    }

    PORT_Memset(pinArea, 0, sizeof(pinArea));


    /* OK, add the random Seed value into the random number generator */
    fort_skipjackUnwrap(Ks,config_file->wrappedRandomSeed.len,
	config_file->wrappedRandomSeed.data,seed);
    RNG_RandomUpdate(seed,sizeof(seed));

    /* it is, go ahead and log in */
    swtoken->login = PR_TRUE;
    /* Ks is always stored in keyReg[0] when we log in */
    PORT_Memcpy(swtoken->keyReg[0].data, Ks, sizeof (FORTSkipjackKey));
    swtoken->keyReg[0].present = PR_TRUE;
    PORT_Memset(Ks, 0, sizeof(FORTSkipjackKey));
    PORT_Free(Ks);


    return CI_OK;
}

/*
 * close an open socket. Power_Down flag is set when we want to reset the
 * cards complete state.
 */
int
MACI_Close(HSESSION session, unsigned int flags, int socket)
{
    if (socket != SOCKET_ID) return CI_BAD_CARD;
    if (swtoken == NULL) return CI_BAD_CARD;

    if (flags == CI_POWER_DOWN_FLAG) {
	fort_Logout(swtoken);
    }
    return CI_OK;
}

/*
 * Decrypt keeps track of it's own IV.
 */
int
MACI_Decrypt(HSESSION session, unsigned int size, CI_DATA cipherIn,
				 			CI_DATA plainOut)
{
    int ret;
    unsigned char IV[SKIPJACK_BLOCK_SIZE];

    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    if ((ret = fort_KeyOK(swtoken,swtoken->key,PR_TRUE)) != CI_OK)  return ret;

    /*fort_AddNoise();*/

    /* save the IV, before we potentially trash the new one when we decrypt.
     * (it's permissible to decrypt into the cipher text buffer by passing the
     * same buffers for both cipherIn and plainOut.
     */
    PORT_Memcpy(IV,swtoken->IV, sizeof(IV));
    fort_UpdateIV(cipherIn,size,swtoken->IV);
    return fort_skipjackDecrypt(swtoken->keyReg[swtoken->key].data,
						IV,size,cipherIn,plainOut);
}

/*
 * Clear a key from one of the key registers (indicated by index).
 * return an error if no key exists.
 */
int
MACI_DeleteKey(HSESSION session, int index)
{
    int ret;
    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;

    /* can't delete Ks */
    if (index == 0) return CI_INV_KEY_INDEX;

    if ((ret = fort_KeyOK(swtoken,index,PR_TRUE)) != CI_OK)  return ret;
    fort_ClearKey(&swtoken->keyReg[index]);
    return CI_OK;
}


/*
 * encrypt  some blocks of data and update the IV.
 */ 
int
MACI_Encrypt(HSESSION session, unsigned int size, CI_DATA plainIn,
				 			CI_DATA cipherOut)
{
    int ret;
    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    if ((ret = fort_KeyOK(swtoken,swtoken->key,PR_TRUE)) != CI_OK)  return ret;

    /*fort_AddNoise();*/

    ret = fort_skipjackEncrypt(swtoken->keyReg[swtoken->key].data,
				swtoken->IV,size,plainIn,cipherOut);
    fort_UpdateIV(cipherOut,size,swtoken->IV);

    return ret;

}

/*
 * create a new IV and encode it.
 */

static char *leafbits="THIS IS NOT LEAF";

int
MACI_GenerateIV(HSESSION Session, CI_IV CI_FAR pIV)
{
    int ret;

    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    if ((ret = fort_KeyOK(swtoken,swtoken->key,PR_TRUE)) != CI_OK)  return ret;

    ret = fort_GenerateRandom(swtoken->IV,SKIPJACK_BLOCK_SIZE);
    if (ret != CI_OK) return ret;

    PORT_Memcpy(pIV,leafbits,SKIPJACK_LEAF_SIZE);
    PORT_Memcpy(&pIV[SKIPJACK_LEAF_SIZE],swtoken->IV,SKIPJACK_BLOCK_SIZE);
   
    return CI_OK;
}


/*
 * create a new Key
 */
int
MACI_GenerateMEK(HSESSION session, int index, int reserved)
{
    int ret;
    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    if ((ret = fort_KeyOK(swtoken,index,PR_FALSE)) != CI_OK)  return ret;

    ret = fort_GenerateRandom(swtoken->keyReg[index].data,
					sizeof (swtoken->keyReg[index].data));
    if (ret == CI_OK) swtoken->keyReg[index].present = PR_TRUE;

    return ret;
}

/*
 * build a new Ra/ra pair for a KEA exchange.
 */
int
MACI_GenerateRa(HSESSION session, CI_RA CI_FAR pRa)
{
    int ret;
    int counter;
    int RaLen,raLen;
    DSAPrivateKey *privKey = NULL;
    PQGParams params;
    SECStatus rv;
    int crv = CI_EXEC_FAIL;
    fortSlotEntry *certEntry = NULL;
    unsigned char *unsignedRa = NULL;
    unsigned char *unsignedra = NULL;
    fortKeyInformation *key_info = NULL;
	

    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    /* make sure the personality is set */
    if (swtoken->certIndex == 0) return CI_INV_STATE;

    /* pick next Ra circular buffer */
    counter = swtoken->nextRa;
    swtoken->nextRa++;
    if (swtoken->nextRa >= MAX_RA_SLOTS) swtoken->nextRa = 0;

    /* now get the params for diffie -helman key gen */
    certEntry = fort_GetCertEntry(swtoken->config_file,swtoken->certIndex);
    if (certEntry == NULL) return CI_INV_CERT_INDEX;
    if (certEntry->exchangeKeyInformation) {
	key_info = certEntry->exchangeKeyInformation;
    } else {
	key_info = certEntry->signatureKeyInformation;
    }
    if (key_info == NULL) return CI_NO_X;
   
    /* Generate Diffie Helman key Pair -- but we use DSA key gen to do it */
    rv = SECITEM_CopyItem(NULL,&params.prime,&key_info->p);
    if (rv != SECSuccess) return CI_EXEC_FAIL;
    rv = SECITEM_CopyItem(NULL,&params.subPrime,&key_info->q);
    if (rv != SECSuccess) return CI_EXEC_FAIL;
    rv = SECITEM_CopyItem(NULL,&params.base,&key_info->g);
    if (rv != SECSuccess) return CI_EXEC_FAIL;

    /* KEA uses DSA like key generation with short DSA keys that have to
     * maintain a relationship to q */
    rv = DSA_NewKey(&params, &privKey);
    SECITEM_FreeItem(&params.prime,PR_FALSE);
    SECITEM_FreeItem(&params.subPrime,PR_FALSE);
    SECITEM_FreeItem(&params.base,PR_FALSE);
    if (rv != SECSuccess) return CI_EXEC_FAIL;
    
    /* save private key, public key, and param in Ra Circular buffer */
    unsignedRa = privKey->publicValue.data;
    RaLen      = privKey->publicValue.len;
    while ((unsignedRa[0] == 0) && (RaLen > CI_RA_SIZE)) {
	unsignedRa++;
	RaLen--;
    }
    if (RaLen > CI_RA_SIZE) goto loser;

    unsignedra = privKey->privateValue.data;
    raLen      = privKey->privateValue.len;
    while ((unsignedra[0] == 0) && (raLen > sizeof(fortRaPrivate))) {
	unsignedra++;
	raLen--;
    }

    if (raLen > sizeof(fortRaPrivate)) goto loser;

    PORT_Memset(swtoken->RaValues[counter].private, 0, sizeof(fortRaPrivate));
    PORT_Memcpy(
	&swtoken->RaValues[counter].private[sizeof(fortRaPrivate) - raLen],
							unsignedra, raLen);
    PORT_Memset(pRa, 0, CI_RA_SIZE);
    PORT_Memcpy(&pRa[CI_RA_SIZE-RaLen], unsignedRa, RaLen);
    PORT_Memcpy(swtoken->RaValues[counter].public, pRa, CI_RA_SIZE);
    crv = CI_OK;

loser:
    PORT_FreeArena(privKey->params.arena, PR_TRUE);

    return crv;
}


/*
 * return some random data.
 */
int
MACI_GenerateRandom(HSESSION session, CI_RANDOM CI_FAR random)
{
    int ret;
    if ((ret = fort_CardExists(swtoken,PR_FALSE)) != CI_OK) return ret;
    return fort_GenerateRandom(random,sizeof (CI_RANDOM));
}


static CI_RA Remail = {
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
};

/*
 * build a new Token exchange key using KEA.
 */
int
MACI_GenerateTEK(HSESSION hSession, int flags, int target, 
     CI_RA CI_FAR Ra, CI_RA CI_FAR Rb, unsigned int YSize, CI_Y CI_FAR pY )
{
    FORTEZZAPrivateKey *key 		= NULL;
    fortSlotEntry *      certEntry;
    unsigned char *      w		= NULL;
    SECItem              *q;
    SECStatus            rv;
    int                  ret,i;
    PRBool               email 		= PR_TRUE;
    SECItem              R;		/* public */
    SECItem              Y;		/* public */
    SECItem              r;		/* private */
    SECItem              x;		/* private */
    SECItem              wItem;		/* derived secret */
    fortRaPrivatePtr     ra;
    FORTSkipjackKey      cover_key;

    unsigned char pad[10] = { 0x72, 0xf1, 0xa8, 0x7e, 0x92,
			      0x82, 0x41, 0x98, 0xab, 0x0b };

    /* verify that everything is ok with the token, keys and certs */
    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    /* make sure the personality is set */
    if (swtoken->certIndex == 0) return CI_INV_STATE;
    if ((ret = fort_KeyOK(swtoken,target,PR_FALSE)) != CI_OK)  return ret;

    /* get the cert from the entry, then look up the key from that cert */
    certEntry = fort_GetCertEntry(swtoken->config_file,swtoken->certIndex);
    if (certEntry == NULL) return CI_INV_CERT_INDEX;
    key = fort_GetPrivKey(swtoken,fortezzaDHKey,certEntry);
    if (key == NULL) return CI_NO_X; 

    if (certEntry->exchangeKeyInformation) {
	q = &certEntry->exchangeKeyInformation->q;
    } else {
	q = &certEntry->signatureKeyInformation->q;
    }

    email = (PORT_Memcmp(Rb,Remail,sizeof(Rb)) == 0) ? PR_TRUE: PR_FALSE;


    /* load the common elements */
    Y.data = pY;
    Y.len = YSize;
    x.data = key->u.dh.privateValue.data;
    x.len = key->u.dh.privateValue.len;

    /* now initialize the rest of the values */
    if (flags == CI_INITIATOR_FLAG) {
	if (email) { 
	    R.data = Y.data;
	    R.len = Y.len;
	} else {
	    R.data = Rb;
	    R.len = sizeof(CI_RA);
	}
        ra = fort_LookupPrivR(swtoken,Ra);
	if (ra == NULL) {
	    ret = CI_EXEC_FAIL;
	    goto loser;
	}
	r.data = ra;
	r.len = sizeof(fortRaPrivate);
    } else {
	R.data = Ra;
	R.len = sizeof(CI_RA);
	if (email) {
	    r.data = x.data;
	    r.len = x.len;
	} else {
            ra = fort_LookupPrivR(swtoken,Rb);
	    if (ra == NULL) {
		ret = CI_EXEC_FAIL;
		goto loser;
	    }
	    r.data = ra;
	    r.len = sizeof(fortRaPrivate);
	}
    }


    if (!KEA_Verify(&Y,&key->u.dh.prime,q)) {
	ret = CI_EXEC_FAIL;
	goto loser;
    }
    if (!KEA_Verify(&R,&key->u.dh.prime,q)) {
	ret = CI_EXEC_FAIL;
	goto loser;
    }

    /* calculate the base key */
    rv = KEA_Derive(&key->u.dh.prime, &Y, &R, &r, &x, &wItem);
    if (rv != SECSuccess) {
	ret = CI_EXEC_FAIL;
	goto loser;
    }

    w = wItem.data;
    /* use the skipjack wrapping function to 'mix' the key up */
    for (i=0; i < sizeof(FORTSkipjackKey); i++) 
    	cover_key[i] = pad[i] ^ w[i];

    ret = fort_skipjackWrap(cover_key,sizeof(FORTSkipjackKey),
		&w[sizeof(FORTSkipjackKey)],swtoken->keyReg[target].data);
    if (ret != CI_OK) goto loser;

    swtoken->keyReg[target].present = PR_TRUE;

    ret = CI_OK;
loser:
    if (w) PORT_Free(w);
    if (key) fort_DestroyPrivateKey(key);
    
    return ret;
}


/*
 * return the bytes of a certificate.
 */
int
MACI_GetCertificate(HSESSION hSession, int certIndex,
						 CI_CERTIFICATE CI_FAR cert)
{
    int len;
    int ret;
    fortSlotEntry *certEntry = NULL;

    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;

    certEntry = fort_GetCertEntry(swtoken->config_file,certIndex);
    if (certEntry == NULL) return CI_INV_CERT_INDEX;

    len = certEntry->certificateData.dataEncryptedWithKs.len;
    PORT_Memset(cert,0,sizeof(CI_CERTIFICATE));
    PORT_Memcpy(cert, certEntry->certificateData.dataEncryptedWithKs.data,len);

    /* Ks is always stored in keyReg[0] when we log in */
    return fort_skipjackDecrypt(swtoken->keyReg[0].data,
	&certEntry->certificateData.dataIV.data[SKIPJACK_LEAF_SIZE],
						len,cert,cert);
}


/*
 * return out sofware configuration bytes. Those field not used by the PKCS #11
 * module may not be filled in exactly.
 */
#define NETSCAPE "Netscape Communications Corp    "
#define PRODUCT  "Netscape Software FORTEZZA Lib  "
#define SOFTWARE "Software FORTEZZA Implementation"

int
MACI_GetConfiguration(HSESSION hSession, CI_CONFIG_PTR config)
{
    config->LibraryVersion = 0x0100;
    config->ManufacturerVersion = 0x0100;
    PORT_Memcpy(config->ManufacturerName,NETSCAPE,sizeof(NETSCAPE));
    PORT_Memcpy(config->ProductName,PRODUCT,sizeof(PRODUCT));
    PORT_Memcpy(config->ProcessorType,SOFTWARE,sizeof(SOFTWARE));
    config->UserRAMSize = 0;
    config->LargestBlockSize = 0x10000;
    config->KeyRegisterCount = KEY_REGISTERS;
    config->CertificateCount = 
		swtoken ? fort_GetCertCount(swtoken->config_file): 0;
    config->CryptoCardFlag = 0;
    config->ICDVersion = 0;
    config->ManufacturerSWVer = 0x0100;
    config->DriverVersion = 0x0100;
    return CI_OK;
}

/*
 * return a list of all the personalities (up to the value 'EntryCount')
 */
int
MACI_GetPersonalityList(HSESSION hSession, int EntryCount, 
						CI_PERSON CI_FAR personList[])
{
    int count;
    int i,ret;
    FORTSWFile *config_file = NULL;
    unsigned char tmp[32];

    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    config_file = swtoken->config_file;

    /* search for the index */
    count= fort_GetCertCount(config_file);

    /* don't return more than the user asked for */
    if (count > EntryCount) count = EntryCount;
    for (i=0; i < count ;i ++) {
	int len, dataLen;
	personList[i].CertificateIndex =
				config_file->slotEntries[i]->certIndex;
	len = config_file->slotEntries[i]->certificateLabel.
						dataEncryptedWithKs.len;
	if (len > sizeof(tmp)) len = sizeof(tmp);
        PORT_Memset(personList[i].CertLabel, ' ',
					sizeof(personList[i].CertLabel));
        PORT_Memcpy(tmp, 
	     config_file->slotEntries[i]->
			certificateLabel.dataEncryptedWithKs.data,
								len);
	/* Ks is always stored in keyReg[0] when we log in */
	ret = fort_skipjackDecrypt(swtoken->keyReg[0].data,
	 &config_file->slotEntries[i]->
			certificateLabel.dataIV.data[SKIPJACK_LEAF_SIZE],len,
			tmp,tmp);
	if (ret != CI_OK) return ret;
	dataLen = DER_GetInteger(&config_file->slotEntries[i]->
			certificateLabel.length);
	if (dataLen > sizeof(tmp)) dataLen = sizeof(tmp);
        PORT_Memcpy(personList[i].CertLabel, tmp, dataLen);
        personList[i].CertLabel[32] = 0;
        personList[i].CertLabel[33] = 0;
        personList[i].CertLabel[34] = 0;
        personList[i].CertLabel[35] = 0;
    }
    return CI_OK;
}


/*
 * get a new session ID. This function is only to make the interface happy,
 * the PKCS #11 module only uses one session per token.
 */
int
MACI_GetSessionID(HSESSION *session)
{
    *session = 1;
    return CI_OK;
}

/*
 * return the current card state.
 */
int
MACI_GetState(HSESSION hSession, CI_STATE_PTR state)
{
    int ret;
    if ((ret = fort_CardExists(swtoken,PR_FALSE)) != CI_OK) return ret;
    *state = fort_GetState(swtoken);
    return CI_OK;
}

/*
 * return the status. NOTE that KeyRegisterFlags and CertificateFlags are never
 * really used by the PKCS #11 module, so they are not implemented.
 */
int
MACI_GetStatus(HSESSION hSession, CI_STATUS_PTR status)
{
    int ret;
    FORTSWFile *config_file = NULL;

    if ((ret = fort_CardExists(swtoken,PR_FALSE)) != CI_OK) return ret;
    config_file = swtoken->config_file;
    status->CurrentSocket = 1;
    status->LockState = swtoken->lock;
    PORT_Memcpy(status->SerialNumber,
		config_file->serialID.data, config_file->serialID.len);
    status->CurrentState = fort_GetState(swtoken);
    status->DecryptionMode = CI_CBC64_MODE;
    status->EncryptionMode = CI_CBC64_MODE;
    status->CurrentPersonality = swtoken->certIndex;
    status->KeyRegisterCount = KEY_REGISTERS;
    /* our code doesn't use KeyRegisters, which is good, because there's not
     * enough of them .... */
    PORT_Memset(status->KeyRegisterFlags,0,sizeof(status->KeyRegisterFlags));
    status->CertificateCount = fort_GetCertCount(config_file);
    PORT_Memset(status->CertificateFlags,0,sizeof(status->CertificateFlags));
    PORT_Memset(status->Flags,0,sizeof(status->Flags));

    return CI_OK;
}

/*
 * add the time call because the PKCS #11 module calls it, but always pretend
 * the clock is bad, so it never uses the returned time.
 */
int
MACI_GetTime(HSESSION hSession, CI_TIME CI_FAR time)
{
   return CI_BAD_CLOCK;
}


/* This function is copied from NSPR so that the PKCS #11 module can be
 * independent of NSPR */
PRInt32 local_getFileInfo(const char *fn, PRFileInfo *info);

/*
 * initialize the SW module, and return the number of slots we support (1).
 */
int
MACI_Initialize(int CI_FAR *count)
{
    char *filename = NULL;
    SECItem file;
    FORTSignedSWFile *decode_file = NULL;
    PRFileInfo info;
    /*PRFileDesc *fd = NULL;*/
    int fd = -1;
    PRStatus err;
    int ret = CI_OK;
    int fcount;

    file.data = NULL;
    file.len = 0;

    *count = 1;

    /* allocate swtoken structure */
    swtoken = PORT_ZNew(FORTSWToken);
    if (swtoken == NULL) return CI_OUT_OF_MEMORY;

    filename = (char *)fort_LookupFORTEZZAInitFile();
    if (filename == NULL) {
	ret = CI_BAD_READ;
	goto failed;
    }
    
    fd = open(filename,O_RDONLY|O_BINARY,0);
    if (fd < 0) {
	ret = CI_BAD_READ;
	goto failed;
    }

    err = local_getFileInfo(filename,&info);
    if ((err != 0) || (info.size == 0)) {
	ret = CI_BAD_READ;
	goto failed;
    }

    file.data = PORT_ZAlloc(info.size);
    if (file.data == NULL) {
	ret = CI_OUT_OF_MEMORY;
	goto failed;
    }

    fcount = read(fd,file.data,info.size);
    close(fd);  fd = -1;
    if (fcount != (int)info.size) {
	ret = CI_BAD_READ;
	goto failed;
    }

    file.len = fcount;

    decode_file = FORT_GetSWFile(&file);
    if (decode_file == NULL) {
	ret = CI_BAD_READ;
	goto failed;
    }
    swtoken->config_file = &decode_file->file;

    RNG_SystemInfoForRNG();
    RNG_FileForRNG(filename);


failed:
    if (filename) PORT_Free(filename);
    if (fd != -1) close(fd);
    if (file.data) PORT_Free(file.data);
    if (ret != CI_OK) {
	if (decode_file) FORT_DestroySignedSWFile(decode_file);
	if (swtoken) PORT_Free(swtoken);
	swtoken = NULL;
    }

    return CI_OK;
}

/*
 * load an IV from an external source. We technically should check it with the
 * key we received.
 */
int
MACI_LoadIV(HSESSION session, CI_IV CI_FAR iv)
{
    int ret;

    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    PORT_Memcpy(swtoken->IV,&iv[SKIPJACK_LEAF_SIZE],SKIPJACK_BLOCK_SIZE);
    return CI_OK;
}

/* implement token lock (should call PR_Monitor here) */
int
MACI_Lock(HSESSION session, int flags)
{
    int ret;
    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    swtoken->lock = 1;

    return CI_OK;
}

/* open a token. For software there isn't much to do that hasn't already been
 * done by initialize. */
int
MACI_Open(HSESSION session, unsigned int flags, int socket)
{
    if (socket != SOCKET_ID) return CI_NO_CARD;
    if (swtoken == NULL) return CI_NO_CARD;
    return CI_OK;
}

/*
 * Reset logs out the token...
 */
int
MACI_Reset(HSESSION session)
{
    if (swtoken) fort_Logout(swtoken);
    return CI_OK;
}

/*
 * restore and encrypt/decrypt state. NOTE: there is no error checking in this
 * or the save function.
 */
int
MACI_Restore(HSESSION session, int type, CI_SAVE_DATA CI_FAR data)
{
    int ret;
    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    PORT_Memcpy(swtoken->IV,data, sizeof (swtoken->IV));
    return CI_OK;
}

/*
 * save and encrypt/decrypt state. NOTE: there is no error checking in this
 * or the restore function.
 */
int
MACI_Save(HSESSION session, int type,CI_SAVE_DATA CI_FAR data)
{
    int ret;
    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    PORT_Memcpy(data,swtoken->IV, sizeof (swtoken->IV));
    return CI_OK;
}

/*
 * picks a token to operate against. In our case there can be only one.
 */
int
MACI_Select(HSESSION session, int socket)
{
    if (socket == SOCKET_ID) return CKR_OK;
    return CI_NO_CARD;
}

/*
 * set a register as the key to use for encrypt/decrypt operations.
 */
int
MACI_SetKey(HSESSION session, int index)
{
    int ret;
    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    if ((ret = fort_KeyOK(swtoken,index,PR_TRUE)) != CI_OK)  return ret;

    swtoken->key = index;
    return CI_OK;
}

/*
 * only CBC64 is supported. Keep setmode for compatibility */
int
MACI_SetMode(HSESSION session, int type, int mode)
{
    if (mode != CI_CBC64_MODE) return CI_INV_MODE;
    return CI_OK;
}

/* set the personality to use for sign/verify */
int
MACI_SetPersonality(HSESSION session, int cert)
{
    int ret;
    fortSlotEntry *certEntry = NULL;

    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;

    certEntry = fort_GetCertEntry(swtoken->config_file,cert);
    if ((certEntry == NULL) ||
	   ((certEntry->exchangeKeyInformation == NULL) &&
			(certEntry->signatureKeyInformation == NULL)) )
						 return CI_INV_CERT_INDEX;
    swtoken->certIndex = cert;
    return CI_OK;
}


/* DSA sign some data */
int
MACI_Sign(HSESSION session, CI_HASHVALUE CI_FAR hash, CI_SIGNATURE CI_FAR sig)
{
    FORTEZZAPrivateKey *key 		= NULL;
    fortSlotEntry *      certEntry	= NULL;
    int                  ret 		= CI_OK;
    SECStatus            rv;
    SECItem              signItem;
    SECItem              hashItem;
    unsigned char        random[DSA_SUBPRIME_LEN];

    /* standard checks */
    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    /* make sure the personality is set */
    if (swtoken->certIndex == 0) return CI_INV_STATE;

    /* get the current personality */
    certEntry = fort_GetCertEntry(swtoken->config_file,swtoken->certIndex);
    if (certEntry == NULL) return CI_INV_CERT_INDEX;

    /* extract the private key from the personality */
    ret = CI_OK;
    key = fort_GetPrivKey(swtoken,fortezzaDSAKey,certEntry);
    if (key == NULL) {
	ret = CI_NO_X;
	goto loser;
    }

    /* create a random value for the signature */
    ret = fort_GenerateRandom(random, sizeof(random));
    if (ret != CI_OK) goto loser;

    /* Sign with that private key */
    signItem.data = sig;
    signItem.len  = DSA_SIGNATURE_LEN;

    hashItem.data = hash;
    hashItem.len  = SHA1_LENGTH;

    rv = DSA_SignDigestWithSeed(&key->u.dsa, &signItem, &hashItem, random);
    if (rv != SECSuccess) {
	ret = CI_EXEC_FAIL;
    }

    /* clean up */
loser:
    if (key != NULL) fort_DestroyPrivateKey(key);

    return ret;
}

/*
 * clean up after ourselves.
 */
int
MACI_Terminate(HSESSION session)
{
    if (swtoken == NULL) return CI_OUT_OF_MEMORY;

    /* clear all the keys */
    fort_Logout(swtoken);
    
    FORT_DestroySWFile(swtoken->config_file);
    PORT_Free(swtoken);
    swtoken = NULL;
    return CI_OK;
}



/* implement token unlock (should call PR_Monitor here) */
int
MACI_Unlock(HSESSION session)
{
    int ret;
    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    swtoken->lock = 0;
    return CI_OK;
}

/*
 * unwrap a key into our software token. NOTE: this function does not
 * verify that the wrapping key is Ks or a TEK. This is because our higher
 * level software doesn't try to wrap MEKs with MEKs. If this API was exposed
 * generically, then we would have to worry about things like this.
 */
int
MACI_UnwrapKey(HSESSION session, int wrapKey, int target, 
						CI_KEY CI_FAR keyData)
{
    int ret = CI_OK;

    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    if ((ret = fort_KeyOK(swtoken,target,PR_FALSE)) != CI_OK)  return ret;
    if ((ret = fort_KeyOK(swtoken,wrapKey,PR_TRUE)) != CI_OK)  return ret;
    ret = fort_skipjackUnwrap(swtoken->keyReg[wrapKey].data,
	sizeof(CI_KEY), keyData, swtoken->keyReg[target].data);
    if (ret != CI_OK) goto loser;

    swtoken->keyReg[target].present = PR_TRUE;

loser:
    return ret;
}

/*
 * Wrap a key out of our software token. NOTE: this function does not
 * verify that the wrapping key is Ks or a TEK, or that the source key is
 * a MEK. This is because our higher level software doesn't try to wrap MEKs 
 * with MEKs, or wrap out TEKS and Ks. If this API was exposed
 * generically, then we would have to worry about things like this.
 */
int
MACI_WrapKey(HSESSION session, int wrapKey, int source, CI_KEY CI_FAR keyData)
{
    int ret;

    if ((ret = fort_CardExists(swtoken,PR_TRUE)) != CI_OK) return ret;
    if ((ret = fort_KeyOK(swtoken,source,PR_TRUE)) != CI_OK)  return ret;
    if ((ret = fort_KeyOK(swtoken,wrapKey,PR_TRUE)) != CI_OK)  return ret;
    ret = fort_skipjackWrap(swtoken->keyReg[wrapKey].data,
	sizeof(CI_KEY), swtoken->keyReg[source].data,keyData);

    return ret;
}

