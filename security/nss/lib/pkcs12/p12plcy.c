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


#include "p12plcy.h"
#include "secoid.h"
#include "secport.h"
#include "secpkcs5.h" /* LOTS of PKCS5 calls below.  XXX EVIL. */

#define PKCS12_NULL  0x0000

typedef struct pkcs12SuiteMapStr {
    SECOidTag		algTag;
    unsigned int	keyLengthBits;	/* in bits */
    unsigned long	suite;
    PRBool 		allowed;
    PRBool		preferred;
} pkcs12SuiteMap;

static pkcs12SuiteMap pkcs12SuiteMaps[] = {
    { SEC_OID_RC4,		40,	PKCS12_RC4_40,		PR_FALSE,	PR_FALSE},
    { SEC_OID_RC4,	       128,	PKCS12_RC4_128,		PR_FALSE,	PR_FALSE},
    { SEC_OID_RC2_CBC,		40,	PKCS12_RC2_CBC_40,	PR_FALSE,	PR_TRUE},
    { SEC_OID_RC2_CBC,	       128,	PKCS12_RC2_CBC_128,	PR_FALSE,	PR_FALSE},
    { SEC_OID_DES_CBC,		64,	PKCS12_DES_56,		PR_FALSE,	PR_FALSE},
    { SEC_OID_DES_EDE3_CBC,    192,	PKCS12_DES_EDE3_168,	PR_FALSE,	PR_FALSE},
    { SEC_OID_UNKNOWN,		 0,	PKCS12_NULL,		PR_FALSE,	PR_FALSE},
    { SEC_OID_UNKNOWN,		 0,	0L,			PR_FALSE,	PR_FALSE}
};

/* determine if algid is an algorithm which is allowed */
PRBool 
SEC_PKCS12DecryptionAllowed(SECAlgorithmID *algid)
{
    unsigned int keyLengthBits;
    SECOidTag algId;
    int i;
   
    algId = SEC_PKCS5GetCryptoAlgorithm(algid);
    if(algId == SEC_OID_UNKNOWN) {
	return PR_FALSE;
    }
    
    keyLengthBits = (unsigned int)(SEC_PKCS5GetKeyLength(algid) * 8);

    i = 0;
    while(pkcs12SuiteMaps[i].algTag != SEC_OID_UNKNOWN) {
	if((pkcs12SuiteMaps[i].algTag == algId) && 
	   (pkcs12SuiteMaps[i].keyLengthBits == keyLengthBits)) {

	    return pkcs12SuiteMaps[i].allowed;
	}
	i++;
    }

    return PR_FALSE;
}

/* is any encryption allowed? */
PRBool
SEC_PKCS12IsEncryptionAllowed(void)
{
    int i;

    i = 0;
    while(pkcs12SuiteMaps[i].algTag != SEC_OID_UNKNOWN) {
	if(pkcs12SuiteMaps[i].allowed == PR_TRUE) {
	    return PR_TRUE;
	} 
	i++;
    }

    return PR_FALSE;
}

/* get the preferred algorithm.
 */
SECOidTag
SEC_PKCS12GetPreferredEncryptionAlgorithm(void)
{
    int i;

    i = 0;
    while(pkcs12SuiteMaps[i].algTag != SEC_OID_UNKNOWN) {
	if((pkcs12SuiteMaps[i].preferred == PR_TRUE) && 
	   (pkcs12SuiteMaps[i].allowed == PR_TRUE)) {
	    return SEC_PKCS5GetPBEAlgorithm(pkcs12SuiteMaps[i].algTag,
	    				    pkcs12SuiteMaps[i].keyLengthBits);
	}
	i++;
    }

    return SEC_OID_UNKNOWN;
}

/* return the strongest algorithm allowed */
SECOidTag
SEC_PKCS12GetStrongestAllowedAlgorithm(void)
{
    int i, keyLengthBits = 0;
    SECOidTag algorithm = SEC_OID_UNKNOWN;

    i = 0;
    while(pkcs12SuiteMaps[i].algTag != SEC_OID_UNKNOWN) {
	if((pkcs12SuiteMaps[i].allowed == PR_TRUE) && 
	   (pkcs12SuiteMaps[i].keyLengthBits > (unsigned int)keyLengthBits) &&
	   (pkcs12SuiteMaps[i].algTag != SEC_OID_RC4)) {
	    algorithm = pkcs12SuiteMaps[i].algTag;
	    keyLengthBits = pkcs12SuiteMaps[i].keyLengthBits;
	}
	i++;
    }

    if(algorithm == SEC_OID_UNKNOWN) {
	return SEC_OID_UNKNOWN;
    }

    return SEC_PKCS5GetPBEAlgorithm(algorithm, keyLengthBits);
}

SECStatus
SEC_PKCS12EnableCipher(long which, int on) 
{
    int i;

    i = 0;
    while(pkcs12SuiteMaps[i].suite != 0L) {
	if(pkcs12SuiteMaps[i].suite == (unsigned long)which) {
	    if(on) {
		pkcs12SuiteMaps[i].allowed = PR_TRUE;
	    } else {
		pkcs12SuiteMaps[i].allowed = PR_FALSE;
	    }
	    return SECSuccess;
	}
	i++;
    }

    return SECFailure;
}

SECStatus
SEC_PKCS12SetPreferredCipher(long which, int on)
{
    int i;
    PRBool turnedOff = PR_FALSE;
    PRBool turnedOn = PR_FALSE;

    i = 0;
    while(pkcs12SuiteMaps[i].suite != 0L) {
	if(pkcs12SuiteMaps[i].preferred == PR_TRUE) {
	    pkcs12SuiteMaps[i].preferred = PR_FALSE;
	    turnedOff = PR_TRUE;
	}
	if(pkcs12SuiteMaps[i].suite == (unsigned long)which) {
	    pkcs12SuiteMaps[i].preferred = PR_TRUE;
	    turnedOn = PR_TRUE;
	}
	i++;
    }

    if((turnedOn) && (turnedOff)) {
	return SECSuccess;
    }

    return SECFailure;
}

