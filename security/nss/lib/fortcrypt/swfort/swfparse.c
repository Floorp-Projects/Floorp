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
 * The following program decodes the FORTEZZA Init File, and stores the result
 * into the fortezza directory.
 */
#include "secasn1.h"
#include "swforti.h"
#include "blapi.h"
#include "secoid.h"
#include "secitem.h"
#include "secder.h"


/*
 * templates for parsing the FORTEZZA Init File. These were taken from DER
 * definitions on SWF Initialization File Format Version 1.0 pp1-3.
 */

/* Key info structure... There are up to two of these per slot entry  */
static const SEC_ASN1Template fortKeyInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(fortKeyInformation) },
    { SEC_ASN1_INTEGER,
          offsetof(fortKeyInformation,keyFlags) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(fortKeyInformation,privateKeyWrappedWithKs) },
    { SEC_ASN1_ANY ,
          offsetof(fortKeyInformation, derPublicKey) },
    { SEC_ASN1_OCTET_STRING, offsetof(fortKeyInformation,p) },
    { SEC_ASN1_OCTET_STRING, offsetof(fortKeyInformation,g) },
    { SEC_ASN1_OCTET_STRING, offsetof(fortKeyInformation,q) },
    { 0 }
}; 

/* This is data that has been wrapped by Ks */
static const SEC_ASN1Template fortProtDataTemplate[] = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(fortProtectedData) },
    { SEC_ASN1_INTEGER,
          offsetof(fortProtectedData,length) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(fortProtectedData,dataIV) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(fortProtectedData,dataEncryptedWithKs) },
    { 0 }
};

/* DER to describe each Certificate Slot ... there are an arbitrary number */
static const SEC_ASN1Template fortSlotEntryTemplate[] = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(fortSlotEntry) },
    { SEC_ASN1_BOOLEAN,
          offsetof(fortSlotEntry,trusted) },
    { SEC_ASN1_INTEGER,
          offsetof(fortSlotEntry,certificateIndex) },
    { SEC_ASN1_INLINE,
          offsetof(fortSlotEntry,certificateLabel), fortProtDataTemplate },
    { SEC_ASN1_INLINE,
          offsetof(fortSlotEntry,certificateData), fortProtDataTemplate },
    { SEC_ASN1_POINTER | SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC |  
				SEC_ASN1_CONSTRUCTED | 0,
          offsetof(fortSlotEntry, exchangeKeyInformation),
						 fortKeyInfoTemplate },
    { SEC_ASN1_POINTER | SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 
				SEC_ASN1_CONSTRUCTED | 1,
          offsetof(fortSlotEntry, signatureKeyInformation), 
						 fortKeyInfoTemplate },
    { 0 }
};

/* This data is used to check MemPhrases, and to generate Ks
 * each file has two mem phrases, one for SSO, one for User */
static const SEC_ASN1Template fortProtectedMemPhrase[]  = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(fortProtectedPhrase) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(fortProtectedPhrase,kValueIV) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(fortProtectedPhrase,wrappedKValue) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(fortProtectedPhrase,memPhraseIV) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(fortProtectedPhrase,hashedEncryptedMemPhrase) },
    { 0 }
};

/* This data is used to check the Mem Init Phrases, and to generate Kinit
 * each file has one mem init phrase, which is used only in transport of
 * this file */
static const SEC_ASN1Template fortMemInitPhrase[]  = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(fortProtectedPhrase) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(fortProtectedPhrase,wrappedKValue) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(fortProtectedPhrase,memPhraseIV) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(fortProtectedPhrase,hashedEncryptedMemPhrase) },
    { 0 }
};

static const SEC_ASN1Template fortSlotEntriesTemplate[]  = {
    { SEC_ASN1_SEQUENCE_OF, 0, fortSlotEntryTemplate }
};

/* This is the complete file with all it's data, but has not been signed
 * yet. */
static const SEC_ASN1Template fortSwFortezzaInitFileToSign[]  = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(FORTSWFile) },
    { SEC_ASN1_INTEGER,
          offsetof(FORTSWFile,version) },
    { SEC_ASN1_ANY,
          offsetof(FORTSWFile,derIssuer) },
    { SEC_ASN1_OCTET_STRING,
          offsetof(FORTSWFile,serialID) },
    { SEC_ASN1_INLINE,
          offsetof(FORTSWFile,initMemPhrase), fortMemInitPhrase },
    { SEC_ASN1_INLINE,
          offsetof(FORTSWFile,ssoMemPhrase), fortProtectedMemPhrase },
    { SEC_ASN1_INLINE,
          offsetof(FORTSWFile,userMemPhrase), fortProtectedMemPhrase },
    { SEC_ASN1_INLINE,
          offsetof(FORTSWFile,ssoPinPhrase), fortProtectedMemPhrase },
    { SEC_ASN1_INLINE,
          offsetof(FORTSWFile,userPinPhrase), fortProtectedMemPhrase },
    { SEC_ASN1_OCTET_STRING,
          offsetof(FORTSWFile,wrappedRandomSeed) },
    { SEC_ASN1_SEQUENCE_OF, offsetof(FORTSWFile,slotEntries),
						 fortSlotEntryTemplate },
    /* optional extentions to ignore here... */
    { 0 }
};

/* The complete, signed init file */
static const SEC_ASN1Template fortSwFortezzaInitFile[]  = {
    { SEC_ASN1_SEQUENCE,
          0, NULL, sizeof(FORTSignedSWFile) },
    { SEC_ASN1_SAVE,
          offsetof(FORTSignedSWFile,signatureWrap.data) },
    { SEC_ASN1_INLINE,
          offsetof(FORTSignedSWFile,file),
          fortSwFortezzaInitFileToSign },
    { SEC_ASN1_INLINE,
          offsetof(FORTSignedSWFile,signatureWrap.signatureAlgorithm),
          SECOID_AlgorithmIDTemplate },
    { SEC_ASN1_BIT_STRING,
          offsetof(FORTSignedSWFile,signatureWrap.signature) },
    { 0 }
};

FORTSkipjackKeyPtr
fort_CalculateKMemPhrase(FORTSWFile *file, 
  fortProtectedPhrase * prot_phrase, char *phrase, FORTSkipjackKeyPtr wrapKey)
{
    unsigned char *data = NULL;
    unsigned char hashout[SHA1_LENGTH];
    int data_len = prot_phrase->wrappedKValue.len;
    int ret;
    unsigned int len;
    unsigned int version;
    unsigned char enc_version[2];
    FORTSkipjackKeyPtr Kout = NULL;
    FORTSkipjackKey Kfek;
    SHA1Context *sha;

    data = (unsigned char *) PORT_ZAlloc(data_len);
    if (data == NULL) goto fail;
    
    PORT_Memcpy(data,prot_phrase->wrappedKValue.data,data_len);

    /* if it's a real protected mem phrase, it's been wrapped by kinit, which
     * was passed to us. */
    if (wrapKey) {
	fort_skipjackDecrypt(wrapKey,
		&prot_phrase->kValueIV.data[SKIPJACK_LEAF_SIZE],data_len,
								data,data);
	data_len = sizeof(CI_KEY);
    }

    /* now calculate the PBE key for fortezza */
    sha = SHA1_NewContext();
    if (sha == NULL) goto fail;
    SHA1_Begin(sha);
    version = DER_GetUInteger(&file->version);
    enc_version[0] = (version >> 8) & 0xff;
    enc_version[1] = version & 0xff;
    SHA1_Update(sha,enc_version,sizeof(enc_version));
    SHA1_Update(sha,file->derIssuer.data, file->derIssuer.len);
    SHA1_Update(sha,file->serialID.data, file->serialID.len);
    SHA1_Update(sha,(unsigned char *)phrase,strlen(phrase));
    SHA1_End(sha,hashout,&len,SHA1_LENGTH);
    SHA1_DestroyContext(sha, PR_TRUE);
    PORT_Memcpy(Kfek,hashout,sizeof(FORTSkipjackKey));

    /* now use that key to unwrap */
    Kout = (FORTSkipjackKeyPtr) PORT_Alloc(sizeof(FORTSkipjackKey));
    ret = fort_skipjackUnwrap(Kfek,data_len,data,Kout);
    if (ret != CI_OK) {
	PORT_Free(Kout);
	Kout = NULL;
    }

fail:
    PORT_Memset(&Kfek, 0, sizeof(FORTSkipjackKey));
    if (data) PORT_ZFree(data,data_len);
    return Kout;
}


PRBool
fort_CheckMemPhrase(FORTSWFile *file, 
  fortProtectedPhrase * prot_phrase, char *phrase, FORTSkipjackKeyPtr wrapKey) 
{
    unsigned char *data = NULL;
    unsigned char hashout[SHA1_LENGTH];
    int data_len = prot_phrase->hashedEncryptedMemPhrase.len;
    unsigned int len;
    SHA1Context *sha;
    PRBool pinOK = PR_FALSE;
    unsigned char cw[4];
    int i;


    /* first, decrypt the hashed/Encrypted Memphrase */
    data = (unsigned char *) PORT_ZAlloc(data_len);
    if (data == NULL) goto failed;

    PORT_Memcpy(data,prot_phrase->hashedEncryptedMemPhrase.data,data_len);
    fort_skipjackDecrypt(wrapKey,
		&prot_phrase->memPhraseIV.data[SKIPJACK_LEAF_SIZE],data_len,
								data,data);

    /* now build the hash for comparisons */
    sha = SHA1_NewContext();
    if (sha == NULL) goto failed;
    SHA1_Begin(sha);
    SHA1_Update(sha,(unsigned char *)phrase,strlen(phrase));
    SHA1_End(sha,hashout,&len,SHA1_LENGTH);
    SHA1_DestroyContext(sha, PR_TRUE);

    /* hashes don't match... must not be the right pass mem */
    if (PORT_Memcmp(data,hashout,len) != 0) goto failed;

    /* now calcuate the checkword and compare it */
    cw[0] = cw[1] = cw[2] = cw[3] = 0;
    for (i=0; i <5 ; i++) {
	cw[0] = cw[0] ^ hashout[i*4];
	cw[1] = cw[1] ^ hashout[i*4+1];
	cw[2] = cw[2] ^ hashout[i*4+2];
	cw[3] = cw[3] ^ hashout[i*4+3];
    }

    /* checkword doesn't match, must not be the right pass mem */
    if (PORT_Memcmp(data+len,cw,4) != 0) goto failed;

    /* pased all our test, its OK */
    pinOK = PR_TRUE;

failed:
    PORT_Free(data);

    return pinOK;
}

/*
 * walk through the list of memphrases. This function allows us to use a
 * for loop to walk down them.
 */
fortProtectedPhrase *
fort_getNextPhrase( FORTSWFile *file, fortProtectedPhrase *last)
{
    if (last == &file->userMemPhrase) {
	return &file->userPinPhrase;
    }
    /* we can add more test here if we want to support SSO mode someday. */

    return NULL;
}

/*
 * decode the DER file data into our nice data structures, including turning
 * cert indexes into integers.
 */
FORTSignedSWFile *
FORT_GetSWFile(SECItem *initBits)
{
    FORTSignedSWFile *sw_init_file;
    PRArenaPool *arena = NULL;
    SECStatus rv;
    int i, count;

    /* get the local arena... be sure to free this at the end */

    /* get the local arena... be sure to free this at the end */
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) goto fail;

    sw_init_file = (FORTSignedSWFile *)
		PORT_ArenaZAlloc(arena,sizeof(FORTSignedSWFile));
    if (sw_init_file == NULL) goto fail;

    /* ANS1 decode the complete init file */
    rv = SEC_ASN1DecodeItem(arena,sw_init_file,fortSwFortezzaInitFile,initBits);
    if (rv != SECSuccess) {
	goto fail;
    }

    /* count the certs */
    count = 0;
    while (sw_init_file->file.slotEntries[count]) count++;

    for (i=0; i < count; i++) {
	/* update the cert Index Pointers */
	sw_init_file->file.slotEntries[i]->certIndex =
		DER_GetInteger(&sw_init_file->
				file.slotEntries[i]->certificateIndex );
    }

    /* now start checking the mem phrases and pins, as well as calculating the
     * file's 'K' values. First we start with K(init). */
    sw_init_file->file.arena = arena; 

    return sw_init_file;
    /* OK now that we've read in the init file, and now have Kinit, Ks, and the
     * appropriate Pin Phrase, we need to build our database file. */
   
fail: 
    if (arena) PORT_FreeArena(arena,PR_TRUE);
    return NULL;
}

/*
 * Check the init memphrases and the user mem phrases. Remove all the init
 * memphrase wrappings. Save the Kinit and Ks values for use.
 */
SECStatus
FORT_CheckInitPhrase(FORTSignedSWFile *sw_init_file, char *initMemPhrase)
{
    SECStatus rv = SECFailure;

    sw_init_file->Kinit = fort_CalculateKMemPhrase(&sw_init_file->file,
		 &sw_init_file->file.initMemPhrase, initMemPhrase, NULL);
    if (sw_init_file->Kinit == NULL)  goto fail;

    /* now check the init Mem phrase */
    if (!fort_CheckMemPhrase(&sw_init_file->file,
			&sw_init_file->file.initMemPhrase, 
				initMemPhrase, sw_init_file->Kinit)) {
	goto fail;
    }
    rv = SECSuccess;

fail:
    return rv;
}

    /* now check user user mem phrase and calculate Ks */
SECStatus
FORT_CheckUserPhrase(FORTSignedSWFile *sw_init_file, char *userMemPhrase)
{
    SECStatus rv = SECFailure;
    char tmp_data[13];
    char *padMemPhrase = NULL;
    fortProtectedPhrase *phrase_store;

    if (strlen(userMemPhrase) < 12) {
	PORT_Memset(tmp_data, ' ', sizeof(tmp_data));
	PORT_Memcpy(tmp_data,userMemPhrase,strlen(userMemPhrase));
	tmp_data[12] = 0;
	padMemPhrase = tmp_data;
    }

    for (phrase_store = &sw_init_file->file.userMemPhrase; phrase_store;
     phrase_store = fort_getNextPhrase(&sw_init_file->file,phrase_store)) {
	sw_init_file->Ks = fort_CalculateKMemPhrase(&sw_init_file->file,
		 phrase_store, userMemPhrase, sw_init_file->Kinit);
 
	if ((sw_init_file->Ks == NULL) && (padMemPhrase != NULL)) {
		sw_init_file->Ks = fort_CalculateKMemPhrase(&sw_init_file->file,
		     phrase_store, padMemPhrase, sw_init_file->Kinit);
		userMemPhrase = padMemPhrase;
	}
	if (sw_init_file->Ks == NULL) {
	    continue;
	}

	/* now check the User Mem phrase */
	if (fort_CheckMemPhrase(&sw_init_file->file, phrase_store, 
				userMemPhrase, sw_init_file->Ks)) {
	    break;
	}
	PORT_Free(sw_init_file->Ks);
	sw_init_file->Ks = NULL;
    }


    if (phrase_store == NULL) goto fail;

    /* strip the Kinit wrapping */
    fort_skipjackDecrypt(sw_init_file->Kinit,
		&phrase_store->kValueIV.data[SKIPJACK_LEAF_SIZE],
	phrase_store->wrappedKValue.len, phrase_store->wrappedKValue.data,
	phrase_store->wrappedKValue.data);
    phrase_store->wrappedKValue.len = 12;

    PORT_Memset(phrase_store->kValueIV.data,0,phrase_store->kValueIV.len);

    sw_init_file->file.initMemPhrase = *phrase_store;
    sw_init_file->file.ssoMemPhrase = *phrase_store;
    sw_init_file->file.ssoPinPhrase = *phrase_store;
    sw_init_file->file.userMemPhrase = *phrase_store;
    sw_init_file->file.userPinPhrase = *phrase_store;


    rv = SECSuccess;
   
fail: 
    /* don't keep the pin around */
    PORT_Memset(tmp_data, 0, sizeof(tmp_data));
    return rv;
}

void
FORT_DestroySWFile(FORTSWFile *file)
{
    PORT_FreeArena(file->arena,PR_FALSE);
}

void
FORT_DestroySignedSWFile(FORTSignedSWFile *swfile)
{
    FORT_DestroySWFile(&swfile->file);
}


SECItem *
FORT_GetDERCert(FORTSignedSWFile *swfile,int index)
{
    SECItem *newItem = NULL;
    unsigned char *cert = NULL;
    int len,ret;
    fortSlotEntry *certEntry = NULL;
   

    newItem = PORT_ZNew(SECItem);
    if (newItem == NULL) return NULL;

    certEntry = fort_GetCertEntry(&swfile->file,index);
    if (certEntry == NULL) {
	PORT_Free(newItem);
	return NULL;
    }

    newItem->len = len = certEntry->certificateData.dataEncryptedWithKs.len;
    newItem->data = cert = PORT_ZAlloc(len);
    if (cert == NULL) {
	PORT_Free(newItem);
	return NULL;
    }
    newItem->len = DER_GetUInteger(&certEntry->certificateData.length);
    

    PORT_Memcpy(cert, certEntry->certificateData.dataEncryptedWithKs.data,len);

    /* Ks is always stored in keyReg[0] when we log in */
    ret = fort_skipjackDecrypt(swfile->Ks,
	&certEntry->certificateData.dataIV.data[SKIPJACK_LEAF_SIZE],
						len,cert,cert);
    if (ret != CI_OK) {
	SECITEM_FreeItem(newItem,PR_TRUE);
	return NULL;
    }
    return newItem;
}

/*
 * decode the DER file data into our nice data structures, including turning
 * cert indexes into integers.
 */
SECItem *
FORT_PutSWFile(FORTSignedSWFile *sw_init_file)
{
    SECItem *outBits, *tmpBits;

    outBits = PORT_ZNew(SECItem);
    if (outBits == NULL) goto fail;

    /* ANS1 encode the complete init file */
    tmpBits = SEC_ASN1EncodeItem(NULL,outBits,sw_init_file,fortSwFortezzaInitFile);
    if (tmpBits == NULL) {
	goto fail;
    }

    return outBits;
   
fail: 
    if (outBits) SECITEM_FreeItem(outBits,PR_TRUE);
    return NULL;
}
