
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
 * Portions created by the Initial Developer are Copyright (C) 2003
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
/* $Id: shvfy.c,v 1.10 2004/04/27 23:04:36 gerv%gerv.net Exp $ */

#include "shsign.h"
#include "prlink.h"
#include "prio.h"
#include "blapi.h"
#include "seccomon.h"
#include "stdio.h"
#include "prmem.h"

/* #define DEBUG_SHVERIFY 1 */

static char *
mkCheckFileName(const char *libName)
{
    int ln_len = PORT_Strlen(libName);
    char *output = PORT_Alloc(ln_len+sizeof(SGN_SUFFIX));
    int index = ln_len + 1 - sizeof("."SHLIB_SUFFIX);

    if ((index > 0) &&
        (PORT_Strncmp(&libName[index],
                        "."SHLIB_SUFFIX,sizeof("."SHLIB_SUFFIX)) == 0)) {
        ln_len = index;
    }
    PORT_Memcpy(output,libName,ln_len);
    PORT_Memcpy(&output[ln_len],SGN_SUFFIX,sizeof(SGN_SUFFIX));
    return output;
}

static int
decodeInt(unsigned char *buf)
{
    return (buf[3]) | (buf[2] << 8) | (buf[1] << 16) | (buf[0] << 24);
}

static SECStatus
readItem(PRFileDesc *fd, SECItem *item)
{
    unsigned char buf[4];
    int bytesRead;


    bytesRead = PR_Read(fd, buf, 4);
    if (bytesRead != 4) {
	return SECFailure;
    }
    item->len = decodeInt(buf);

    item->data = PORT_Alloc(item->len);
    if (item->data == NULL) {
	item->len = 0;
	return SECFailure;
    }
    bytesRead = PR_Read(fd, item->data, item->len);
    if (bytesRead != item->len) {
	PORT_Free(item->data);
	item->data = NULL;
	item->len = 0;
	return SECFailure;
    }
    return SECSuccess;
}

PRBool
BLAPI_SHVerify(const char *name, PRFuncPtr addr)
{
    /* find our shared library name */
    char *shName = PR_GetLibraryFilePathname(name, addr);
    char *checkName = NULL;
    PRFileDesc *checkFD = NULL;
    PRFileDesc *shFD = NULL;
    SHA1Context *hashcx = NULL;
    SECItem signature = { 0, NULL, 0 };
    SECItem hash;
    int bytesRead, offset;
    SECStatus rv;
    DSAPublicKey key;
    int count;

    PRBool result = PR_FALSE; /* if anything goes wrong,
			       * the signature does not verify */
    unsigned char buf[512];
    unsigned char hashBuf[SHA1_LENGTH];

    PORT_Memset(&key,0,sizeof(key));
    hash.data = hashBuf;
    hash.len = sizeof(hashBuf);

    if (!shName) {
	goto loser;
    }

    /* figure out the name of our check file */
    checkName = mkCheckFileName(shName);
    if (!checkName) {
	goto loser;
    }

    /* open the check File */
    checkFD = PR_Open(checkName, PR_RDONLY, 0);
    if (checkFD == NULL) {
#ifdef DEBUG_SHVERIFY
        fprintf(stderr, "Failed to open the check file %s: (%d, %d)\n",
                checkName, (int)PR_GetError(), (int)PR_GetOSError());
#endif /* DEBUG_SHVERIFY */
	goto loser;
    }

    /* read and Verify the headerthe header */
    bytesRead = PR_Read(checkFD, buf, 12);
    if (bytesRead != 12) {
	goto loser;
    }
    if ((buf[0] != NSS_SIGN_CHK_MAGIC1) || (buf[1] != NSS_SIGN_CHK_MAGIC2)) {
	goto loser;
    }
    if ((buf[2] != NSS_SIGN_CHK_MAJOR_VERSION) || 
				(buf[3] < NSS_SIGN_CHK_MINOR_VERSION)) {
	goto loser;
    }
#ifdef notdef
    if (decodeInt(&buf[8]) != CKK_DSA) {
	goto loser;
    }
#endif

    /* seek past any future header extensions */
    offset = decodeInt(&buf[4]);
    PR_Seek(checkFD, offset, PR_SEEK_SET);

    /* read the key */
    rv = readItem(checkFD,&key.params.prime);
    if (rv != SECSuccess) {
	goto loser;
    }
    rv = readItem(checkFD,&key.params.subPrime);
    if (rv != SECSuccess) {
	goto loser;
    }
    rv = readItem(checkFD,&key.params.base);
    if (rv != SECSuccess) {
	goto loser;
    }
    rv = readItem(checkFD,&key.publicValue);
    if (rv != SECSuccess) {
	goto loser;
    }
    /* read the siganture */
    rv = readItem(checkFD,&signature);
    if (rv != SECSuccess) {
	goto loser;
    }

    /* done with the check file */
    PR_Close(checkFD);
    checkFD = NULL;

    /* open our library file */
    shFD = PR_Open(shName, PR_RDONLY, 0);
    if (shFD == NULL) {
#ifdef DEBUG_SHVERIFY
        fprintf(stderr, "Failed to open the library file %s: (%d, %d)\n",
                shName, (int)PR_GetError(), (int)PR_GetOSError());
#endif /* DEBUG_SHVERIFY */
	goto loser;
    }

    /* hash our library file with SHA1 */
    hashcx = SHA1_NewContext();
    if (hashcx == NULL) {
	goto loser;
    }
    SHA1_Begin(hashcx);

    count = 0;
    while ((bytesRead = PR_Read(shFD, buf, sizeof(buf))) > 0) {
	SHA1_Update(hashcx, buf, bytesRead);
	count += bytesRead;
    }
    PR_Close(shFD);
    shFD = NULL;

    SHA1_End(hashcx, hash.data, &hash.len, hash.len);


    /* verify the hash against the check file */
    if (DSA_VerifyDigest(&key, &signature, &hash) == SECSuccess) {
	result = PR_TRUE;
    }
#ifdef DEBUG_SHVERIFY
  {
        int i,j;
        fprintf(stderr,"File %s: %d bytes\n",shName, count);
        fprintf(stderr,"  hash: %d bytes\n", hash.len);
#define STEP 10
        for (i=0; i < hash.len; i += STEP) {
           fprintf(stderr,"   ");
           for (j=0; j < STEP && (i+j) < hash.len; j++) {
                fprintf(stderr," %02x", hash.data[i+j]);
           }
           fprintf(stderr,"\n");
        }
        fprintf(stderr,"  signature: %d bytes\n", signature.len);
        for (i=0; i < signature.len; i += STEP) {
           fprintf(stderr,"   ");
           for (j=0; j < STEP && (i+j) < signature.len; j++) {
                fprintf(stderr," %02x", signature.data[i+j]);
           }
           fprintf(stderr,"\n");
        }
	fprintf(stderr,"Verified : %s\n",result?"TRUE": "FALSE");
    }
#endif /* DEBUG_SHVERIFY */


loser:
    if (shName != NULL) {
	PR_Free(shName);
    }
    if (checkName != NULL) {
	PORT_Free(checkName);
    }
    if (checkFD != NULL) {
	PR_Close(checkFD);
    }
    if (shFD != NULL) {
	PR_Close(shFD);
    }
    if (hashcx != NULL) {
	SHA1_DestroyContext(hashcx,PR_TRUE);
    }
    if (signature.data != NULL) {
	PORT_Free(signature.data);
    }
    if (key.params.prime.data != NULL) {
	PORT_Free(key.params.prime.data);
    }
    if (key.params.subPrime.data != NULL) {
	PORT_Free(key.params.subPrime.data);
    }
    if (key.params.base.data != NULL) {
	PORT_Free(key.params.base.data);
    }
    if (key.publicValue.data != NULL) {
	PORT_Free(key.publicValue.data);
    }

    return result;
}

PRBool
BLAPI_VerifySelf(const char *name)
{
    /* to separate shlib to verify if name is NULL */
    if (name == NULL) {
	return PR_TRUE;
    }
    return BLAPI_SHVerify(name, (PRFuncPtr) decodeInt);
}
