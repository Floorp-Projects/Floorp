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
#include <stdio.h>

#include "prio.h"
#include "swforti.h"
#include "maci.h"
#include "secder.h"
#include "blapi.h"

void
printkey(char *s, unsigned char *block) {
 int i;
 printf("%s \n  0x",s);
 for(i=0; i < 10; i++) printf("%02x",block[i]);
 printf("\n");
}

void
printblock(char *s, unsigned char *block) {
 int i;
 printf("%s \n  0x",s);
 for(i=0; i < 8; i++) printf("%02x",block[i]);
 printf("\n  0x");
 for(i=8; i < 16; i++) printf("%02x",block[i]);
 printf("\n");
}


static char *leafbits="THIS IS NOT LEAF";

static void
encryptCertEntry(fortProtectedData *pdata,FORTSkipjackKeyPtr Ks,
						unsigned char *data,int len)
{
    unsigned char *dataout;
    int enc_len;
	/* XXX Make length */
    pdata->dataIV.data = PORT_ZAlloc(24);
    pdata->dataIV.len = 24;
    PORT_Memcpy(pdata->dataIV.data,leafbits,SKIPJACK_LEAF_SIZE);
    fort_GenerateRandom(&pdata->dataIV.data[SKIPJACK_LEAF_SIZE],
						SKIPJACK_BLOCK_SIZE);
    enc_len = (len + (SKIPJACK_BLOCK_SIZE-1)) & ~(SKIPJACK_BLOCK_SIZE-1);
    dataout = pdata->dataEncryptedWithKs.data = PORT_ZAlloc(enc_len);
    pdata->dataEncryptedWithKs.len = enc_len;
    fort_skipjackEncrypt(Ks,&pdata->dataIV.data[SKIPJACK_LEAF_SIZE],
					enc_len, data,dataout);
    if (len > 255) {
	pdata->length.data = PORT_ZAlloc(2);
	pdata->length.data[0] = (len >> 8) & 0xff;
	pdata->length.data[1] = len & 0xff;
	pdata->length.len = 2;
    } else {
	pdata->length.data = PORT_ZAlloc(1);
	pdata->length.data[0] = len & 0xff;
	pdata->length.len = 1;
    }

}
    
unsigned char issuer[30] = { 0 };

void
makeCertSlot(fortSlotEntry *entry,int index,char *label,SECItem *cert,
 FORTSkipjackKeyPtr Ks, unsigned char *xKEA, unsigned char *xDSA, 
 unsigned char *pubKey, int pubKeyLen, unsigned char *p, unsigned char *q, 
							unsigned char *g)
{
    unsigned char *key; /* private key */

    entry->trusted.data = PORT_Alloc(1);
    *entry->trusted.data = index == 0 ? 1 : 0;
    entry->trusted.len = 1;
    entry->certificateIndex.data = PORT_Alloc(1);
    *entry->certificateIndex.data = index;
    entry->certificateIndex.len = 1;
    entry->certIndex = index;
    encryptCertEntry(&entry->certificateLabel,Ks,
		(unsigned char *)label, strlen(label));
    encryptCertEntry(&entry->certificateData,Ks, cert->data, cert->len);
    if (xKEA) {
	entry->exchangeKeyInformation = PORT_ZNew(fortKeyInformation);
	entry->exchangeKeyInformation->keyFlags.data = PORT_ZAlloc(1);
	entry->exchangeKeyInformation->keyFlags.data[0] = 1;
	entry->exchangeKeyInformation->keyFlags.len = 1;
	key = PORT_Alloc(24);
	fort_skipjackWrap(Ks,24,xKEA,key);
	entry->exchangeKeyInformation->privateKeyWrappedWithKs.data = key;
	entry->exchangeKeyInformation->privateKeyWrappedWithKs.len = 24;
	entry->exchangeKeyInformation->derPublicKey.data = pubKey;
	entry->exchangeKeyInformation->derPublicKey.len = pubKeyLen;
	entry->exchangeKeyInformation->p.data = p;
	entry->exchangeKeyInformation->p.len = 128;
	entry->exchangeKeyInformation->q.data = q;
	entry->exchangeKeyInformation->q.len = 20;
	entry->exchangeKeyInformation->g.data = g;
	entry->exchangeKeyInformation->g.len = 128;

	entry->signatureKeyInformation = PORT_ZNew(fortKeyInformation);
	entry->signatureKeyInformation->keyFlags.data = PORT_ZAlloc(1);
	entry->signatureKeyInformation->keyFlags.data[0] = 1;
	entry->signatureKeyInformation->keyFlags.len = 1;
	key = PORT_Alloc(24);
	fort_skipjackWrap(Ks,24,xDSA,key);
	entry->signatureKeyInformation->privateKeyWrappedWithKs.data = key;
	entry->signatureKeyInformation->privateKeyWrappedWithKs.len = 24;
	entry->signatureKeyInformation->derPublicKey.data = pubKey;
	entry->signatureKeyInformation->derPublicKey.len = pubKeyLen;
	entry->signatureKeyInformation->p.data = p;
	entry->signatureKeyInformation->p.len = 128;
	entry->signatureKeyInformation->q.data = q;
	entry->signatureKeyInformation->q.len = 20;
	entry->signatureKeyInformation->g.data = g;
	entry->signatureKeyInformation->g.len = 128;
    } else {
	entry->exchangeKeyInformation = NULL;
        entry->signatureKeyInformation = NULL;
    }
    
    return;
}


void
makeProtectedPhrase(FORTSWFile *file, fortProtectedPhrase *prot_phrase,
  	     FORTSkipjackKeyPtr Ks, FORTSkipjackKeyPtr Kinit, char *phrase)
{
    SHA1Context *sha;
    unsigned char hashout[SHA1_LENGTH];
    FORTSkipjackKey Kfek;
    unsigned int len;
    unsigned char cw[4];
    unsigned char enc_version[2];
    unsigned char *data = NULL;
    int keySize;
    int i,version;
    char tmp_data[13];

    if (strlen(phrase) < 12) {
        PORT_Memset(tmp_data, ' ', sizeof(tmp_data));
        PORT_Memcpy(tmp_data,phrase,strlen(phrase));
        tmp_data[12] = 0;
        phrase = tmp_data;
    }

    /* now calculate the PBE key for fortezza */
    sha = SHA1_NewContext();
    SHA1_Begin(sha);
    version = DER_GetUInteger(&file->version);
    enc_version[0] = (version >> 8) & 0xff;
    enc_version[1] = version  & 0xff;
    SHA1_Update(sha,enc_version,sizeof(enc_version));
    SHA1_Update(sha,file->derIssuer.data, file->derIssuer.len);
    SHA1_Update(sha,file->serialID.data, file->serialID.len);
    SHA1_Update(sha,(unsigned char *)phrase,strlen(phrase));
    SHA1_End(sha,hashout,&len,SHA1_LENGTH);
    PORT_Memcpy(Kfek,hashout,sizeof(FORTSkipjackKey));

    keySize = sizeof(CI_KEY);
    if (Kinit) keySize = SKIPJACK_BLOCK_SIZE*2;
    data = PORT_ZAlloc(keySize);
    prot_phrase->wrappedKValue.data = data;
    prot_phrase->wrappedKValue.len = keySize;
    fort_skipjackWrap(Kfek,sizeof(CI_KEY),Ks,data);

    /* first, decrypt the hashed/Encrypted Memphrase */
    data = (unsigned char *) PORT_ZAlloc(SHA1_LENGTH+sizeof(cw));

    /* now build the hash for comparisons */
    SHA1_Begin(sha);
    SHA1_Update(sha,(unsigned char *)phrase,strlen(phrase));
    SHA1_End(sha,hashout,&len,SHA1_LENGTH);
    SHA1_DestroyContext(sha,PR_TRUE);


    /* now calcuate the checkword and compare it */
    cw[0] = cw[1] = cw[2] = cw[3] = 0;
    for (i=0; i <5 ; i++) {
	cw[0] = cw[0] ^ hashout[i*4];
	cw[1] = cw[1] ^ hashout[i*4+1];
	cw[2] = cw[2] ^ hashout[i*4+2];
	cw[3] = cw[3] ^ hashout[i*4+3];
    }

    PORT_Memcpy(data,hashout,len);
    PORT_Memcpy(data+len,cw,sizeof(cw));

    prot_phrase->memPhraseIV.data = PORT_ZAlloc(24);
    prot_phrase->memPhraseIV.len = 24;
    PORT_Memcpy(prot_phrase->memPhraseIV.data,leafbits,SKIPJACK_LEAF_SIZE);
    fort_GenerateRandom(&prot_phrase->memPhraseIV.data[SKIPJACK_LEAF_SIZE],
						SKIPJACK_BLOCK_SIZE);
    prot_phrase->kValueIV.data = PORT_ZAlloc(24);
    prot_phrase->kValueIV.len = 24;
    PORT_Memcpy(prot_phrase->kValueIV.data,leafbits,SKIPJACK_LEAF_SIZE);
    fort_GenerateRandom(&prot_phrase->kValueIV.data[SKIPJACK_LEAF_SIZE],
						SKIPJACK_BLOCK_SIZE);
    fort_skipjackEncrypt(Ks,&prot_phrase->memPhraseIV.data[SKIPJACK_LEAF_SIZE],
					len+sizeof(cw), data,data);

    prot_phrase->hashedEncryptedMemPhrase.data = data;
    prot_phrase->hashedEncryptedMemPhrase.len = len+sizeof(cw);

    if (Kinit) {
        fort_skipjackEncrypt(Kinit,
		&prot_phrase->kValueIV.data[SKIPJACK_LEAF_SIZE],
                prot_phrase->wrappedKValue.len,
                prot_phrase->wrappedKValue.data,
                prot_phrase->wrappedKValue.data );
    }

    return;
}


void
fill_in(SECItem *item,unsigned char *data, int len)
{
    item->data = PORT_Alloc(len);
    PORT_Memcpy(item->data,data,len);
    item->len = len;
}

