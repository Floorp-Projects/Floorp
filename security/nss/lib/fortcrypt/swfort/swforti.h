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
 * Software implementation of FORTEZZA Skipjack primatives and helper functions.
 */
#ifndef _SWFORTI_H_
#define _SWFORTI_H_

#ifndef RETURN_TYPE
#define RETURN_TYPE int
#endif

#include "seccomon.h"
#include "swfort.h"
#include "swfortti.h"
#include "maci.h"


SEC_BEGIN_PROTOS
/*
 * Check to see if the index is ok, and that key is appropriately present or
 * absent.
 */
int fort_KeyOK(FORTSWToken *token, int index, PRBool isPresent);

/*
 * clear out a key register
 */
void fort_ClearKey(FORTKeySlot *key);

/*
 * clear out an Ra register
 */
void fort_ClearRaSlot(FORTRaRegisters *ra);

/*
 * provide a helper function to do all the loggin out functions.
 * NOTE: Logging in only happens in MACI_CheckPIN
 */
void fort_Logout(FORTSWToken *token);

/*
 * update the new IV value based on the current cipherText (should be the last 
 * block of the cipher text).
 */
int fort_UpdateIV(unsigned char *cipherText, unsigned int size,unsigned char *IV);


/*
 * verify that we have a card initialized, and if necessary, logged in.
 */
int fort_CardExists(FORTSWToken *token,PRBool needLogin);

/*
 * walk down the cert slot entries, counting them.
 * return that count.
 */
int fort_GetCertCount(FORTSWFile *file);

/*
 * copy an unsigned SECItem to a signed SecItem. (if the high bit is on,
 * pad with a leading 0.
 */
SECStatus fort_CopyUnsigned(PRArenaPool *arena, SECItem *to, const SECItem *from);

/*
 * NOTE: these keys do not have the public values, and cannot be used to
 * extract the public key from the private key. Since we never do this in
 * this code, and this function is private, we're reasonably safe (as long as
 * any of your callees do not try to extract the public value as well).
 * Also -- the token must be logged in before this function is called.
 */
SECKEYLowPrivateKey * fort_GetPrivKey(FORTSWToken *token,KeyType keyType,fortSlotEntry *certEntry);

/*
 * find a particulare certificate entry from the config
 * file.
 */
fortSlotEntry * fort_GetCertEntry(FORTSWFile *file,int index);

/*
 * use the token to termine it's CI_State.
 */
CI_STATE fort_GetState(FORTSWToken *token);

/*
 * find the private ra value for a given public Ra value.
 */
fortRaPrivatePtr fort_LookupPrivR(FORTSWToken *token,CI_RA Ra);

/*
 * go add more noise to the random number generator
 */
void fort_AddNoise(void);

/*
 * Get a random number
 */
int fort_GenerateRandom(unsigned char *buf, int bytes);


/*
 * We're deep in the bottom of MACI and PKCS #11... We need to
 * find our fortezza key file. This function lets us search manual paths to 
 * find our key file.
 */
char *fort_FindFileInPath(char *path, char *fn);


char *fort_LookupFORTEZZAInitFile(void);


FORTSkipjackKeyPtr fort_CalculateKMemPhrase(FORTSWFile *file, 
  fortProtectedPhrase * prot_phrase, char *phrase, FORTSkipjackKeyPtr wrapKey);


PRBool fort_CheckMemPhrase(FORTSWFile *file, 
  fortProtectedPhrase * prot_phrase, char *phrase, FORTSkipjackKeyPtr wrapKey);


/* These function actually implements skipjack CBC64 Decrypt */
int fort_skipjackDecrypt(FORTSkipjackKeyPtr key, unsigned char *iv, 
		unsigned long size, unsigned char *cipherIn, 
						unsigned char *plainOut);

/* These function actually implements skipjack CBC64 Encrypt */
int fort_skipjackEncrypt(FORTSkipjackKeyPtr key, unsigned char *iv, 
		unsigned long size, unsigned char *plainIn, 
						unsigned char *cipherOut);

/*
 * unwrap is used for key generation and mixing
 */
int fort_skipjackUnwrap(FORTSkipjackKeyPtr key,unsigned long len, 
			unsigned char *cipherIn, unsigned char *plainOut);

/*
 * unwrap is used for key generation and mixing
 */
int
fort_skipjackWrap(FORTSkipjackKeyPtr key,unsigned long len, 
			unsigned char *plainIn, unsigned char *cipherOut);

SEC_END_PROTOS

#endif
