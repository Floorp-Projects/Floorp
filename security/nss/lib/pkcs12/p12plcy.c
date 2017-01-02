/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "p12plcy.h"
#include "secoid.h"
#include "secport.h"
#include "secpkcs5.h" 

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

