/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_POINT 7
/* Standard C includes */
#include <stdlib.h>
#include <stdio.h>

/* NSPR includes */
#include <prio.h>
#include <prprf.h>
#include <plarena.h>
#include <prinit.h>
#include <prmem.h>

/* security includes */
#include <pkcs11t.h>
#include <secmodt.h>
#include <pk11func.h>
#include <secmod.h>
#include <secutil.h>
#include <keyt.h>

/* replacer header file */
#include "pk11test.h"

#include "pkcs11.h"

void SEC_Init(void);

PRStatus InitCrypto(char*);
int TestUserManagement();
int TestCrypto();
MechInfo* GetMechInfo(CK_MECHANISM_TYPE type);
int TestEncrypt(CK_MECHANISM_TYPE mech);
int TestSign(CK_MECHANISM_TYPE mech);
int TestDigest(CK_MECHANISM_TYPE mech);
int TestHMAC(CK_MECHANISM_TYPE mech);
int TestSymmetricEncrypt(CK_MECHANISM_TYPE mech);
int TestPKEncrypt(CK_MECHANISM_TYPE mech);


static char* userpw = NULL;
static int secerror=0;
/* PK11SymKey *symkey=NULL;*/
PK11SlotInfo *slot=NULL;

/* Errors */
enum {
	NO_ERROR_AT_ALL=0,
	NO_SUCH_SLOT=1,
	KEY_GEN_FAILED,
	CREATE_CONTEXT_FAILED,
	INTERNAL_RNG_FAILED,
	MECH_NOT_FOUND,
	INPUT_FILE_ERROR,
	KEY_COPY_FAILED,
	CIPHER_OP_FAILED,
	FINALIZE_FAILED,
	RESULTS_DONT_MATCH,
	PARAM_GEN_FAILED,
	PLAINTEXT_DOESNT_MATCH,
	ENCRYPTION_IS_NOOP,
	WRAP_PRIVKEY_FAILED,
	WRAP_SYMKEY_FAILED,
	UNWRAP_SYMKEY_FAILED,
	UNWRAPPED_KEY_DOESNT_MATCH,
	UNWRAP_PRIVKEY_FAILED,
	SIGNATURE_FAILED,
	SIGNATURE_DOESNT_VERIFY,
	AUTHENTICATION_FAILED,
	AUTHENTICATION_SUCCEEDED,
	MODDB_ACCESS
};

static char* errString[] = {
	"No error",
	"No such slot",
	"Failed to generate key",
	"Failed to create a cryptographic context",
	"Failed to generate random bytes",
	"Mechanism was not found",
	"Error in input file",
	"Failed to copy key from internal to external module",
	"Cipher operation failed",
	"Cipher finalization failed",
	"Internal module produced a different result than the target module",
	"Failed to generate cryptographic parameters",
	"Recovered plaintext does not match original plaintext",
	"Ciphertext is the same as plaintext",
	"Unable to wrap private key",
	"Unable to wrap symmetric key",
	"Unable to unwrap symmetric key",
	"Unwrapped key does not match original key",
	"Unable to unwrap private key",
	"Signing operation failed",
	"Incorrect signature: doesn't verify",
	"Failed to authenticate to slot",
	"Authenticated to slot with incorrect password",
	"Unable to access security module database"
};

/***********************************************************************
 *
 * R e a d I n p u t F i l e
 *
 * Read tokenname and module name from the file with the indicated name.
 * Pass in the addresses of pointers.  They will be set to point at
 * dynamically-allocated memory.
 *
 * Returns 0 on success, -1 on error with file.
 */
int
ReadInputFile(char *filename, char**tokenname, char**moddbname, char **userpw)
{
	PRFileDesc* file=NULL;
	char readbuf[1025];
	int numbytes=0;
	char *cp;

	*tokenname = NULL;
	*moddbname = NULL;

	/* Open file */
	file = PR_Open(filename, PR_RDONLY, 0);
	if(!file) {
		return -1;
	}

	/* Read in everything */
	numbytes = PR_Read(file, readbuf, 1024);
	if(numbytes==-1) {
		goto loser;
	}
	readbuf[numbytes] = '\0'; /* make sure we're null-terminated */

	/* Get tokenname */
	cp = strtok(readbuf, "\r\n");
	if(cp == NULL) {
		goto loser;
	}
	*tokenname = PR_Malloc(strlen(cp)+1);
	strcpy(*tokenname, cp);

	/* get moddbname */
	cp = strtok(NULL, "\r\n");
	if(cp == NULL) {
		goto loser;
	}
	*moddbname = PR_Malloc(strlen(cp)+1);
	strcpy(*moddbname, cp);

	/* Get module PIN */
	cp = strtok(NULL, "\r\n");
	if(cp == NULL) {
		goto loser;
	}
	*userpw = PR_Malloc(strlen(cp)+1);
	strcpy(*userpw, cp);

	PR_Close(file);
	return 0;

loser:
	if(file) {
		PR_Close(file);
	}
	if(*tokenname) {
		PR_Free(*tokenname);
		*tokenname = NULL;
	}
	if(*moddbname) {
		PR_Free(*moddbname);
		*moddbname = NULL;
	}
	return -1;
}

static PRBool supplyPassword=PR_TRUE;
char*
PasswordFunc(PK11SlotInfo *slot, PRBool loadcerts, void *wincx)
{
	if(supplyPassword) {
		/*PR_fprintf(PR_STDOUT, "Feeding password: |%s|\n", userpw);*/
		supplyPassword = PR_FALSE;
		return PL_strdup(userpw);
	} else  {
		/*PR_fprintf(PR_STDOUT, "PasswordFunc supplying NULL.\n");*/
		return NULL;
	}
}
	

/**********************************************************************
 *
 * m a i n
 *
 */
int
main(int argc, char *argv[])
{
	char *tokenname=NULL;
	char *moddbname=NULL;
	int errcode;

	if(argc < 3) {
		PR_fprintf(PR_STDERR,
"\nPKCS #11 Test Suite Version %d.%d.%d\n\
Usage: pkcs11 testid configfile\n",VERSION_MAJOR,VERSION_MINOR,VERSION_POINT);
		return -1;
	}

	testId = atoi(argv[1]);
	if(ReadInputFile(argv[2], &tokenname, &moddbname, &userpw)) {
		errcode = INPUT_FILE_ERROR;
		goto loser;
	}

	PR_fprintf(PR_STDOUT, "testId=%d\n", testId);
	PR_fprintf(PR_STDOUT, "tokenname=%s\n", tokenname);
	PR_fprintf(PR_STDOUT, "moddbname=%s\n", moddbname);

	PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

 	if( InitCrypto(moddbname) != PR_SUCCESS ) {
		errcode = MODDB_ACCESS;
		goto loser;
	}

	slot = PK11_FindSlotByName(tokenname);
	if(!slot) {
		errcode = NO_SUCH_SLOT;
		goto loser;
	}

	if(!REP_USE_CORRECT_PIN && userpw) {
		/* don't use the pin passed in */
		userpw[0]++;
	}
	PK11_SetPasswordFunc(PasswordFunc);
	if(PK11_NeedLogin(slot)) {
		SECStatus result;
		supplyPassword = PR_TRUE;
		result = PK11_Authenticate(slot, PR_FALSE, NULL);
		/* If we just did an invalid login, login correctly so we don't
		 * cause the token to lock us out */
		if(!REP_USE_CORRECT_PIN) {
			userpw[0]--;
			supplyPassword = PR_TRUE;
			PK11_Authenticate(slot, PR_FALSE, NULL);
		}
		if(REP_USE_CORRECT_PIN && result!=SECSuccess) {
			errcode =  AUTHENTICATION_FAILED;
			goto loser;
		} else if(!REP_USE_CORRECT_PIN && result==SECSuccess) {
			errcode = AUTHENTICATION_SUCCEEDED;
			goto loser;
		}
	}

	errcode = TestCrypto();

loser:
	if(tokenname) {
		PR_Free(tokenname); tokenname = NULL;
	}
	if(moddbname) {
		PR_Free(moddbname); moddbname = NULL;
	}
	if(errcode) {
	    PR_fprintf(PR_STDOUT, "Exiting with error: %s.\n\n", errString[errcode]);
	} else {
		PR_fprintf(PR_STDOUT, "Test was successful\n\n");
	}
	return errcode;
}

/**********************************************************************
 *
 * I n i t C r y p t o
 *
 */
PRStatus
InitCrypto(char *moddbname)
{
	SEC_Init();

	if( PR_Access(moddbname, PR_ACCESS_EXISTS) != PR_SUCCESS) {
		PR_fprintf(PR_STDERR, "Error: %s does not exist.\n", moddbname);
		return PR_FAILURE;
	}
	if( PR_Access(moddbname, PR_ACCESS_READ_OK) != PR_SUCCESS) {
		PR_fprintf(PR_STDERR, "Error: %s is not readable.\n",
			moddbname);
		return PR_FAILURE;
	}

	SECMOD_init(moddbname);
	return PR_SUCCESS;
}

/**********************************************************************
 *
 * T e s t C r y p t o
 *
 */
int
TestCrypto()
{
	MechInfo *mechInfo;
	int errcode;
	unsigned short testcount=0;

	if(!PK11_DoesMechanism(slot, REP_MECHANISM)) {
		return 0;
	}

	mechInfo = GetMechInfo(REP_MECHANISM);
	/*PR_fprintf(PR_STDOUT, "Using mechanism %x.\n", REP_MECHANISM);*/
	if(!mechInfo) {
		PR_fprintf(PR_STDERR, "Unable to find mech %x\n",
			REP_MECHANISM);
		return MECH_NOT_FOUND;
	}

	if(mechInfo->op & ENCRYPT_OP) {
		testcount++;
		errcode = TestEncrypt(REP_MECHANISM);
		if(errcode) return errcode;
	}

	if(mechInfo->op & SIGN_OP) {
		testcount++;
		errcode = TestSign(REP_MECHANISM);
		if(errcode) return errcode;
	}

#if 0
	if(mechInfo->op & DIGEST_OP) {
		testcount++;
		errcode = TestDigest(REP_MECHANISM);
		if(errcode) return errcode;
	}

	if(mechInfo->op & HMAC_OP) {
		testcount++;
		errcode = TestHMAC(REP_MECHANISM);
		if(errcode) return errcode;
	}
#endif

	return 0;
}

/**********************************************************************
 *
 * I s S y m m e t r i c
 *
 */
int
IsSymmetric(CK_MECHANISM_TYPE mech)
{
	switch(mech) {
	case CKM_RC2_ECB:
	case CKM_RC2_CBC:
	case CKM_RC2_CBC_PAD:
	case CKM_RC4:
	case CKM_RC5_ECB:
	case CKM_RC5_CBC:
	case CKM_RC5_CBC_PAD:
	case CKM_DES_ECB:
	case CKM_DES_CBC:
	case CKM_DES_CBC_PAD:
	case CKM_DES3_ECB:
	case CKM_DES3_CBC:
	case CKM_DES3_CBC_PAD:
	case CKM_CAST_ECB:
	case CKM_CAST_CBC:
	case CKM_CAST_CBC_PAD:
	case CKM_CAST3_ECB:
	case CKM_CAST3_CBC:
	case CKM_CAST3_CBC_PAD:
	case CKM_CAST5_ECB:
	case CKM_CAST5_CBC:
	case CKM_CAST5_CBC_PAD:
	case CKM_IDEA_ECB:
	case CKM_IDEA_CBC:
	case CKM_IDEA_CBC_PAD:
	case CKM_CDMF_ECB:
	case CKM_CDMF_CBC:
	case CKM_CDMF_CBC_PAD:
	case CKM_SKIPJACK_ECB64:
	case CKM_SKIPJACK_CBC64:
	case CKM_SKIPJACK_OFB64:
	case CKM_SKIPJACK_CFB64:
	case CKM_SKIPJACK_CFB32:
	case CKM_SKIPJACK_CFB16:
	case CKM_SKIPJACK_CFB8:
	case CKM_BATON_ECB128:
	case CKM_BATON_ECB96:
	case CKM_BATON_CBC128:
	case CKM_BATON_COUNTER:
	case CKM_BATON_SHUFFLE:
	case CKM_JUNIPER_ECB128:
	case CKM_JUNIPER_CBC128:
	case CKM_JUNIPER_COUNTER:
	case CKM_JUNIPER_SHUFFLE:
		return 1;
	default:
		return 0;
	}
}

/**********************************************************************
 *
 * T e s t E n c r y p t
 *
 */
int
TestEncrypt(CK_MECHANISM_TYPE mech)
{

	/*PR_fprintf(PR_STDOUT, "Inside TestEncrypt\n");*/
	if(!PK11_DoesMechanism(slot, mech)) {
		/* Can't test if the slot doesn't do this mechanism */
		PR_fprintf(PR_STDERR, "Slot doesn't do this mechanism.\n");
		return 0;
	}

	if(IsSymmetric(mech)) {
		/*PR_fprintf(PR_STDOUT, "Is a symmetric algorithm\n");*/
		return TestSymmetricEncrypt(mech);
	} else {
		/*PR_fprintf(PR_STDOUT, "Is not a symmetric algorithm\n");*/
		return TestPKEncrypt(mech);
	}

	return 0;
}

/**********************************************************************
 *
 * G e n e r a t e P K P a r a m s
 *
 */
void*
GeneratePKParams(CK_MECHANISM_TYPE mech)
{

	/* FIPS preprocessor directives for DSA.                        */
	#define FIPS_DSA_TYPE                           siBuffer
	#define FIPS_DSA_DIGEST_LENGTH                  20  /* 160-bits */
	#define FIPS_DSA_SUBPRIME_LENGTH                20  /* 160-bits */
	#define FIPS_DSA_SIGNATURE_LENGTH               40  /* 320-bits */
	#define FIPS_DSA_PRIME_LENGTH                   64  /* 512-bits */
	#define FIPS_DSA_BASE_LENGTH                    64  /* 512-bits */


	CK_MECHANISM_TYPE keygenMech;
	PK11RSAGenParams *rsaparams;
	PQGParams *dsa_pqg;
	unsigned char *dsa_P = (unsigned char *)
                           "\x8d\xf2\xa4\x94\x49\x22\x76\xaa"
                           "\x3d\x25\x75\x9b\xb0\x68\x69\xcb"
                           "\xea\xc0\xd8\x3a\xfb\x8d\x0c\xf7"
                           "\xcb\xb8\x32\x4f\x0d\x78\x82\xe5"
                           "\xd0\x76\x2f\xc5\xb7\x21\x0e\xaf"
                           "\xc2\xe9\xad\xac\x32\xab\x7a\xac"
                           "\x49\x69\x3d\xfb\xf8\x37\x24\xc2"
                           "\xec\x07\x36\xee\x31\xc8\x02\x91";
	unsigned char *dsa_Q = (unsigned char *)
                           "\xc7\x73\x21\x8c\x73\x7e\xc8\xee"
                           "\x99\x3b\x4f\x2d\xed\x30\xf4\x8e"
                           "\xda\xce\x91\x5f";
	unsigned char *dsa_G = (unsigned char *)
                           "\x62\x6d\x02\x78\x39\xea\x0a\x13"
                           "\x41\x31\x63\xa5\x5b\x4c\xb5\x00"
                           "\x29\x9d\x55\x22\x95\x6c\xef\xcb"
                           "\x3b\xff\x10\xf3\x99\xce\x2c\x2e"
                           "\x71\xcb\x9d\xe5\xfa\x24\xba\xbf"
                           "\x58\xe5\xb7\x95\x21\x92\x5c\x9c"
                           "\xc4\x2e\x9f\x6f\x46\x4b\x08\x8c"
                           "\xc5\x72\xaf\x53\xe6\xd7\x88\x02";


	keygenMech = PK11_GetKeyGen(mech);

	switch(keygenMech) {
	case CKM_RSA_PKCS_KEY_PAIR_GEN:
		rsaparams = PR_Malloc(sizeof(PK11RSAGenParams));
		rsaparams->keySizeInBits = REP_PK_KEY_SIZE;
		rsaparams->pe = 65537L;
		return (void*) rsaparams;
	case CKM_ECDSA_KEY_PAIR_GEN:
	case CKM_DSA_KEY_PAIR_GEN:

		/* Allocate PQG memory */
		dsa_pqg = PORT_ZAlloc(sizeof(PQGParams));
		dsa_pqg->prime.data = (unsigned char*)
			PORT_ZAlloc(FIPS_DSA_PRIME_LENGTH);
		dsa_pqg->subPrime.data = (unsigned char*)
			PORT_ZAlloc(FIPS_DSA_SUBPRIME_LENGTH);
		dsa_pqg->base.data = (unsigned char*)
			PORT_ZAlloc(FIPS_DSA_BASE_LENGTH);

		dsa_pqg->prime.type = FIPS_DSA_TYPE;
		PORT_Memcpy(dsa_pqg->prime.data, dsa_P, FIPS_DSA_PRIME_LENGTH);
		dsa_pqg->prime.len = FIPS_DSA_PRIME_LENGTH;

		dsa_pqg->subPrime.type = FIPS_DSA_TYPE;
		PORT_Memcpy( dsa_pqg->subPrime.data, dsa_Q,
			FIPS_DSA_SUBPRIME_LENGTH );
		dsa_pqg->subPrime.len = FIPS_DSA_SUBPRIME_LENGTH;

		dsa_pqg->base.type = FIPS_DSA_TYPE;
		PORT_Memcpy( dsa_pqg->base.data, dsa_G, FIPS_DSA_BASE_LENGTH );
		dsa_pqg->base.len = FIPS_DSA_BASE_LENGTH;

		return (void*) dsa_pqg;

	case CKM_DH_PKCS_KEY_PAIR_GEN:
	case CKM_KEA_KEY_PAIR_GEN:
	default:
		return NULL;
	}
	return NULL;
}

/**********************************************************************
 *
 * F r e e P K P a r a m s
 *
 */
void
FreePKParams(void* p, CK_MECHANISM_TYPE mech)
{

	switch(PK11_GetKeyGen(mech)) {
	case CKM_RSA_PKCS_KEY_PAIR_GEN:
		PR_Free( (PK11RSAGenParams*)p);
		break;
	case CKM_ECDSA_KEY_PAIR_GEN:
	case CKM_DSA_KEY_PAIR_GEN:
		PR_Free( (PQGParams*)p);
		break;
	}
}


/**********************************************************************
 *
 * T e s t P K E n c r y p t
 *
 */
int
TestPKEncrypt(CK_MECHANISM_TYPE mech)
{
	PK11SlotInfo *internal;
	SECStatus status;
	int errcode;
	SECItem *kgparams;
	SECKEYPublicKey *pubk=NULL;
	SECKEYPrivateKey *refPrivk=NULL, *testPrivk=NULL;
	PK11SymKey *refSymKey=NULL, *testSymKey=NULL, *recoveredSymKey=NULL;
	SECItem refWrappedKey, testWrappedKey;
	SECItem *refSymkeyData=NULL, *testSymkeyData=NULL;
	SECItem wrapParams;
	int testSymKeySize;
    CK_ATTRIBUTE_TYPE usages[] = { CKA_UNWRAP };
    int usageCount = 1;

	wrapParams.data = "aaaaaaaa";
	wrapParams.len = 8;

	refWrappedKey.len = 1024;
	refWrappedKey.data = PR_Malloc(1024);
	testWrappedKey.len = 1024;
	testWrappedKey.data = PR_Malloc(1024);

	internal = PK11_GetInternalSlot();

	/* Generate keygen parameter */
	kgparams = GeneratePKParams(mech);
	if(!kgparams) {
		errcode = PARAM_GEN_FAILED;
		goto loser;
	}

    /*
     * Generate the keypair, either on the target module or on the internal
     * module.
     */
	if(REP_KEYGEN_ON_TARGET) {
		refPrivk = PK11_GenerateKeyPair(slot, PK11_GetKeyGen(mech),
			kgparams, &pubk,
			(slot==internal) ? PR_FALSE : PR_TRUE /*isPerm*/,
			PR_FALSE /*isSensitive*/,
			NULL/*wincx*/);
	} else {
		refPrivk = PK11_GenerateKeyPair(internal, PK11_GetKeyGen(mech),
			kgparams, &pubk,
			PR_FALSE/*isPerm*/, PR_FALSE /*isSensitive*/,
			NULL/*wincx*/);
	}
	if(!refPrivk) {
		secerror = PORT_GetError();
		errcode = KEY_GEN_FAILED;
		goto loser;
	}

	/*
	 * Generate symmetric key, either on the target module or on the internal
     * module.
	 */
	if(REP_KEYGEN_ON_TARGET) {
		refSymKey = PK11_KeyGen(slot, CKM_DES_CBC_PAD, NULL,
			REP_SYMKEY_SIZE, NULL);
	} else {
		refSymKey = PK11_KeyGen(internal, CKM_DES_CBC_PAD, NULL,
			REP_SYMKEY_SIZE, NULL);
	}
	if(!refSymKey) {
		secerror = PORT_GetError();
		errcode = KEY_GEN_FAILED;
		goto loser;
	}

	/*
     * If we generated the keys on the internal module, we have to
     * transfer them from the internal module to the target module, unless
     * the target module is the internal module.
	 */
	if( (slot != internal) && !REP_KEYGEN_ON_TARGET) {
		SECItem empty;
		SECItem label;
		empty.len=0;
		empty.data=NULL;
		label.data = "foobar";
		label.len = 6;

		/* Copy the symmetric key to the target token*/
		testSymKey = pk11_CopyToSlot(slot,
					     CKM_DES_CBC_PAD,
					     CKA_UNWRAP,
					     refSymKey);
		if(testSymKey==NULL) {
			secerror = PORT_GetError();
			errcode = KEY_COPY_FAILED;
			goto loser;
		}

		/* Copy the private key to the target token */
		status = PK11_WrapPrivKey(internal,
					  refSymKey,
					  refPrivk,
					  CKM_DES_CBC_PAD,
					  &wrapParams,
					  &refWrappedKey,
					  NULL /*wincx*/);
		if(status != SECSuccess) {
			secerror = PORT_GetError();
			errcode = WRAP_PRIVKEY_FAILED;
			goto loser;
		}

		testPrivk = PK11_UnwrapPrivKey(slot,
						 testSymKey,
						 CKM_DES_CBC_PAD,
						 &wrapParams,
						 &refWrappedKey,
						 &label /*label*/,
						 &empty /*ID Value*/,
						 PR_TRUE /*perm*/,
						 PR_TRUE /*sensitive*/,
                         PK11_GetKeyType(mech, 0),
                         usages, usageCount,
						 NULL /*wincx*/);
		if(testPrivk==NULL) {
			secerror = PORT_GetError();
			errcode = UNWRAP_PRIVKEY_FAILED;
			goto loser;
		}
	} else {
		testPrivk=refPrivk; refPrivk = NULL;
		testSymKey=refSymKey; refSymKey = NULL;
	}

	/* Wrap the symmetric key with the public key */
    /* !!! Which mech do we use here, the symmetric or the PK? */
	status = PK11_PubWrapSymKey(mech, pubk, testSymKey, &testWrappedKey);
	if(status != SECSuccess) {
		secerror = PORT_GetError();
		errcode = WRAP_SYMKEY_FAILED;
		goto loser;
	}
	testSymKeySize = PK11_GetKeyLength(testSymKey);

	/*
	 * Unless we are testing the internal slot, do the same wrap operation
	 * on the internal slot and compare with the wrap done on the module
	 * under test.  If we did the keygen on the target module, we don't
     * have the keys on the internal module so we can't compare.
	 */
	if( (slot != internal) && !REP_KEYGEN_ON_TARGET) {
		status = PK11_PubWrapSymKey(mech, pubk, refSymKey,
				&refWrappedKey);
		if(status != SECSuccess) {
			secerror = PORT_GetError();
			errcode = WRAP_SYMKEY_FAILED;
			goto loser;
		}

		if( (testWrappedKey.len != refWrappedKey.len) ||
		    memcmp(testWrappedKey.data, refWrappedKey.data, 
			testWrappedKey.len) ) {
			/* Wrapped Keys don't match */
			/* !!! There's random data in these encryptions, so they'll never
             * match. */
			/*errcode = RESULTS_DONT_MATCH;*/
			/*goto loser;*/
		}
	}

	/* Get the data of the symmetric key */
    /* Extracting the key value may not work, depending on the token.  If
     * it doesn't work, we won't be able to do the comparison later */
    PK11_ExtractKeyValue(testSymKey);
	testSymkeyData = PK11_GetKeyData(testSymKey);
    if(testSymkeyData->data == NULL) {
        /* couldn't extract key data */
        testSymkeyData = NULL;
    } else {
	    testSymkeyData = SECITEM_DupItem(testSymkeyData);
    }

	/* Destroy the symmetric key everywhere */
	if(refSymKey) {
		PK11_FreeSymKey(refSymKey); refSymKey = NULL;
	}
	if(testSymKey) {
		PK11_FreeSymKey(testSymKey); testSymKey = NULL;
	}

	/*
	 * Unwrap the key and make sure we get the same thing back. Can only
     * do this if we were able to get the key data from the test token.
	 */
    if(testSymkeyData != NULL) {
	    refSymKey = PK11_PubUnwrapSymKey(testPrivk, &testWrappedKey,
			    CKM_DES_CBC_PAD, CKA_WRAP, testSymKeySize);
	    if(refSymKey==NULL) {
		    secerror = PORT_GetError();
		    errcode = UNWRAP_SYMKEY_FAILED;
		    goto loser;
	    }
        /* We should always be able to get the key data from the internal
         * module */
		PK11_ExtractKeyValue(refSymKey);
	    refSymkeyData = PK11_GetKeyData(refSymKey);
        PR_ASSERT(refSymkeyData!=NULL && refSymkeyData->data!=NULL);
        PR_ASSERT(testSymkeyData!=NULL && testSymkeyData->data!=NULL);
	    if(SECITEM_CompareItem(refSymkeyData, testSymkeyData) != SECEqual) {
		    errcode = UNWRAPPED_KEY_DOESNT_MATCH;
		    goto loser;
	    }
    }

#ifdef DEBUG
	PR_fprintf(PR_STDOUT, "Successfully finished TestPKEncrypt!\n");
#endif

	errcode = 0;

loser:
	if(refPrivk) {
		SECKEY_DestroyPrivateKey(refPrivk);
	}
	SECITEM_FreeItem(&refWrappedKey, PR_FALSE);
	SECITEM_FreeItem(&testWrappedKey, PR_FALSE);
	if(refSymkeyData) {
		/* do nothing, it's a copy */
	}
	if(testSymkeyData) {
		SECITEM_FreeItem(testSymkeyData, PR_TRUE);
	}
	if(pubk) {
		SECKEY_DestroyPublicKey(pubk);
	}
	if(testPrivk) {
		SECKEY_DestroyPrivateKey(testPrivk);
	}
	if(refSymKey) {
		PK11_FreeSymKey(refSymKey);
	} 
	if(testSymKey) {
		PK11_FreeSymKey(testSymKey);
	}
	if(recoveredSymKey) {
		PK11_FreeSymKey(recoveredSymKey);
	}
	return errcode;
	

}

/**********************************************************************
 *
 * T e s t S y m m e t r i c E n c r y p t
 *
 */
int
TestSymmetricEncrypt(CK_MECHANISM_TYPE mech)
{
	PK11Context *refcontext=NULL, *testcontext=NULL;
	PK11SlotInfo *internal;
	SECStatus status;
	PK11SymKey* intkey=NULL, *extkey=NULL;
	int errcode;
	unsigned char *ptext=NULL;
	int maxclen = REP_PLAINTEXT_LEN + 128;
	unsigned char *refctext=NULL, *testctext=NULL;
	int refclen, testclen;
	unsigned char *recovered=NULL;
	int reclen;
	SECItem iv, *param=NULL;

	internal = PK11_GetInternalSlot();

	ptext = PR_Malloc(REP_PLAINTEXT_LEN);
	refctext = PR_Malloc(maxclen);
	testctext = PR_Malloc(maxclen);
	recovered = PR_Malloc(maxclen);

    /* Generate random plaintext */
	status = RNG_GenerateGlobalRandomBytes(ptext, REP_PLAINTEXT_LEN);
	if(status != SECSuccess) {
		errcode = INTERNAL_RNG_FAILED;
		goto loser;
	}

	/* Generate mechanism parameter */
	iv.len = 8;
	iv.data = "aaaaaaaa"; /* !!! does this need to be random? Maybe a
                            * replacer variable ? */
	param = PK11_ParamFromIV(mech, &iv);
	if(!param) {
		errcode = PARAM_GEN_FAILED;
		goto loser;
	}

	/*
     * Generate the key, either on the target module or the internal module.
	 */
	if(REP_KEYGEN_ON_TARGET) {
		intkey = PK11_KeyGen(slot, mech, NULL, REP_SYMKEY_SIZE,
			NULL);
	} else {
		intkey = PK11_KeyGen(internal, mech, NULL, REP_SYMKEY_SIZE,
			NULL);
	}
	if(!intkey) {
		secerror = PORT_GetError();
		errcode = KEY_GEN_FAILED;
		goto loser;
	}

	if( (slot != internal) && !REP_KEYGEN_ON_TARGET) {
		/* Copy the key to the target token if it isn't there already */
		extkey = pk11_CopyToSlot(slot, mech, CKA_ENCRYPT, intkey);
		if(!extkey) {
			secerror = PORT_GetError();
			errcode = KEY_COPY_FAILED;
			goto loser;
		}
	} else {
		extkey = intkey;
		intkey = NULL;
	}

	/* Create an encryption context */
	testcontext = PK11_CreateContextBySymKey(mech, CKA_ENCRYPT, extkey,
						param);
	if(!testcontext) {
		secerror = PORT_GetError();
		errcode = CREATE_CONTEXT_FAILED;
		goto loser;
	}

	/* Do the encryption */
	status = PK11_CipherOp(testcontext, testctext, &testclen,
				maxclen, ptext, REP_PLAINTEXT_LEN);
	if(status != SECSuccess) {
		secerror = PORT_GetError();
		errcode = CIPHER_OP_FAILED;
		goto loser;
	}
	status = PK11_Finalize(testcontext);
	if(status != SECSuccess) {
		secerror = PORT_GetError();
		errcode = FINALIZE_FAILED;
		goto loser;
	}

	/* Free the encryption context */
	PK11_DestroyContext(testcontext, PR_TRUE /*freeit*/);
	testcontext = NULL;

	/* Make sure the encryption did something */
	if(!memcmp(ptext, testctext,
		REP_PLAINTEXT_LEN > testclen ? testclen : REP_PLAINTEXT_LEN)) {
		errcode = ENCRYPTION_IS_NOOP;
		goto loser;
	}

    /*
	 * Now do everything on the internal module and compare the results. 
     * If the key was generated on the target module, it doesn't exist on
     * the internal module so we can't compare.
     */
	if( (slot != internal) && !REP_KEYGEN_ON_TARGET) {
		/* Create encryption context */
		refcontext = PK11_CreateContextBySymKey(mech, CKA_ENCRYPT,
			intkey, param);
		if(!refcontext) {
			secerror = PORT_GetError();
			errcode = CREATE_CONTEXT_FAILED;
			goto loser;
		}

		/* Perform the encryption */
		status = PK11_CipherOp(refcontext, refctext, &refclen,
				maxclen, ptext, REP_PLAINTEXT_LEN);
		if(status != SECSuccess) {
			secerror = PORT_GetError();
			errcode = CIPHER_OP_FAILED;
			goto loser;
		}
		status = PK11_Finalize(refcontext);
		if(status != SECSuccess) {
			secerror = PORT_GetError();
			errcode = FINALIZE_FAILED;
			goto loser;
		}

		/* Free the context */
		PK11_DestroyContext(refcontext, PR_TRUE /*freeit*/);
		refcontext = NULL;


		/* Compare the ciphertext from the target module and the
		 * internal module
		 */
		if( (testclen != refclen) ||
		  (memcmp(testctext, refctext, testclen)) ) {
			errcode = RESULTS_DONT_MATCH;
			goto loser;
		}
	}

	/*
	 * Decrypt the ciphertext and make sure we get back the original
	 * ptext
	 */

	/* Create the decryption context */
	testcontext = PK11_CreateContextBySymKey(mech, CKA_DECRYPT, extkey,
				param);
	if(!testcontext) {
		secerror = PORT_GetError();
		errcode = CREATE_CONTEXT_FAILED;
		goto loser;
	}

	/* Do the decryption */
	status = PK11_CipherOp(testcontext, recovered, &reclen,
				maxclen, testctext, testclen);
	if(status != SECSuccess) {
		secerror = PORT_GetError();
		errcode = CIPHER_OP_FAILED;
		goto loser;
	}
	status = PK11_Finalize(testcontext);
	if(status != SECSuccess) {
		secerror = PORT_GetError();
		errcode = FINALIZE_FAILED;
		goto loser;
	}


	/* Free the encryption context */
	PK11_DestroyContext(testcontext, PR_TRUE /*freeit*/);
	testcontext = NULL;


	/* Compare the recovered text to the plaintext */
	if( (reclen != REP_PLAINTEXT_LEN) || 
		(memcmp(recovered, ptext, reclen)) ) {
			errcode = PLAINTEXT_DOESNT_MATCH;
			goto loser;
	}
	

#ifdef DEBUG
	PR_fprintf(PR_STDOUT, "Successfully finished TestSymmetricEncrypt!\n");
#endif

	errcode = 0;

loser:
	if(ptext) {
		PR_Free(ptext); ptext = NULL;
	}
	if(refctext) {
		PR_Free(refctext); refctext = NULL;
	}
	if(testctext) {
		PR_Free(testctext); testctext = NULL;
	}
	if(intkey) {
		PK11_FreeSymKey(intkey); intkey = NULL;
	}
	if(extkey) {
		PK11_FreeSymKey(extkey); extkey = NULL;
	}
	if(testcontext) {
		PK11_DestroyContext(testcontext, PR_TRUE /*freeit*/);
	}
	if(refcontext) {
		PK11_DestroyContext(refcontext, PR_TRUE /*freeit*/);
	}
	if(param) {
		SECITEM_FreeItem(param, PR_TRUE);
		param = NULL;
	}
	if(recovered) {
		PR_Free(recovered); recovered = NULL;
	}
	return errcode;
}

/**********************************************************************
 *
 * T e s t S i g n
 *
 */
int
TestSign(CK_MECHANISM_TYPE mech)
{
	PK11SlotInfo *internal;
	SECStatus status;
	int errcode;
	SECItem *kgparams;
	SECKEYPublicKey *pubk=NULL;
	SECKEYPrivateKey *refPrivk=NULL, *testPrivk=NULL;
	PK11SymKey *refSymKey=NULL, *testSymKey=NULL, *recoveredSymKey=NULL;
	SECItem refWrappedKey, testWrappedKey;
	SECItem ptext, refSignature, testSignature;
	SECItem wrapParam;
    CK_ATTRIBUTE_TYPE usages[] = { CKA_SIGN };
    int usageCount = 1;

	refWrappedKey.len = 1024;
	refWrappedKey.data = PR_Malloc(1024);
	testWrappedKey.len = 1024;
	testWrappedKey.data = PR_Malloc(1024);
	refSignature.len = 1024;
	refSignature.data = PR_Malloc(1024);
	testSignature.len = 1024;
	testSignature.data = PR_Malloc(1024);
	wrapParam.data = "aaaaaaaa";
	wrapParam.len = 8;

	internal = PK11_GetInternalSlot();

	/* Generate random ptext */
	ptext.data = PR_Malloc(20);
	ptext.len = 20;
	status = RNG_GenerateGlobalRandomBytes(ptext.data, 8);
	if(status != SECSuccess) {
		errcode = INTERNAL_RNG_FAILED;
		goto loser;
	}

	/* Generate keygen parameter */
	kgparams = GeneratePKParams(mech);
	if(!kgparams) {
		errcode = PARAM_GEN_FAILED;
		goto loser;
	}

	/*
     * Generate the keypair, on the target module or the internal module.
	 */
	if(REP_KEYGEN_ON_TARGET) {
		refPrivk = PK11_GenerateKeyPair(slot, PK11_GetKeyGen(mech),
			kgparams, &pubk, (slot==internal) ? PR_FALSE :
			PR_TRUE /*isPerm*/,
			PR_FALSE /*isSensitive*/, NULL/*wincx*/);
	} else {
		refPrivk = PK11_GenerateKeyPair(internal, PK11_GetKeyGen(mech),
			kgparams, &pubk, PR_FALSE /*isPerm*/,
			PR_FALSE /*isSensitive*/, NULL/*wincx*/);
	}
	if(!refPrivk) {
		secerror = PORT_GetError();
		errcode = KEY_GEN_FAILED;
		goto loser;
	}

	/*
	 * Generate symmetric key, on the target module or the internal module.
	 */
	if(REP_KEYGEN_ON_TARGET) {
		refSymKey = PK11_KeyGen(slot, CKM_DES_CBC_PAD, NULL,
			REP_SYMKEY_SIZE, NULL);
	} else {
		refSymKey = PK11_KeyGen(internal, CKM_DES_CBC_PAD, NULL,
			REP_SYMKEY_SIZE, NULL);
	}
	if(!refSymKey) {
		secerror = PORT_GetError();
		errcode = KEY_GEN_FAILED;
		goto loser;
	}

	/*
     * If the key was generated on the internal module, copy it to the
     * target module, unless the target module is the internal module.
	 */
	if( (slot != internal) && !REP_KEYGEN_ON_TARGET) {
		SECItem empty;
		SECItem label;
		SECItem *pubValue;
		empty.len=0;
		empty.data=NULL;
		label.len=6;
		label.data = "foobar";

		/* Copy the symmetric key to the target token*/
		testSymKey = pk11_CopyToSlot(slot,
					     CKM_DES_CBC_PAD,
					     CKA_WRAP,
					     refSymKey);
		if(testSymKey==NULL) {
			secerror = PORT_GetError();
			errcode = KEY_COPY_FAILED;
			goto loser;
		}

		/* Copy the private key to the target token */
		status = PK11_WrapPrivKey(internal,
					  refSymKey,
					  refPrivk,
					  CKM_DES_CBC_PAD,
					  &wrapParam,
					  &refWrappedKey,
					  NULL /*wincx*/);
		if(status != SECSuccess) {
			secerror = PORT_GetError();
			errcode = WRAP_PRIVKEY_FAILED;
			goto loser;
		}

		switch(pubk->keyType) {
		case dsaKey:
			pubValue = SECITEM_DupItem(&pubk->u.dsa.publicValue);
			break;
		case rsaKey:
			pubValue = SECITEM_DupItem(&pubk->u.rsa.modulus);
			break;
		default:
			pubValue = NULL;
		}
		testPrivk = PK11_UnwrapPrivKey(slot,
						 testSymKey,
						 CKM_DES_CBC_PAD,
						 &wrapParam,
						 &refWrappedKey,
						 &label /*label*/,
						 pubValue /*ID Value*/,
						 PR_TRUE /*perm*/,
						 PR_TRUE /*sensitive*/,
                         PK11_GetKeyType(mech, 0),
                         usages, usageCount,
						 NULL /*wincx*/);
		if(pubValue) {
			SECITEM_FreeItem(pubValue, PR_TRUE);
			pubValue = NULL;
		}
		if(testPrivk==NULL) {
			secerror = PORT_GetError();
			errcode = UNWRAP_PRIVKEY_FAILED;
			goto loser;
		}
	} else {
		testPrivk=refPrivk; refPrivk = NULL;
		testSymKey=refSymKey; refSymKey = NULL;
	}

	/* Sign the data with the private key */
	status = PK11_Sign(testPrivk, &testSignature, &ptext);
	if(status != SECSuccess) {
		secerror = PORT_GetError();
		errcode = SIGNATURE_FAILED;
		goto loser;
	}

	/*
	 * Unless we are testing the internal slot, do the same wrap operation
	 * on the internal slot and compare with the signature done on the
	 * module under test
	 * Also, DSA signatures contain random data, so comparing them
	 * is useless (I suppose if they are the same something is wrong!).
	 */
	if( (slot != internal) && !REP_KEYGEN_ON_TARGET
		&& mech != CKM_DSA && mech != CKM_DSA_SHA1) {
		status = PK11_Sign(refPrivk, &refSignature, &ptext);
		if(status != SECSuccess) {
			secerror = PORT_GetError();
			errcode = SIGNATURE_FAILED;
			goto loser;
		}

		if( SECITEM_CompareItem(&refSignature, &testSignature)
			!=  SECEqual) {
			errcode = RESULTS_DONT_MATCH;
			goto loser;
		}
	}


	/*
	 * Verify the signature.
	 */
	status = PK11_Verify(pubk, &testSignature, &ptext, NULL /*wincx*/);
	if(status != SECSuccess) {
		secerror = PORT_GetError();
		errcode = SIGNATURE_DOESNT_VERIFY;
		goto loser;
	}


#ifdef DEBUG
	PR_fprintf(PR_STDOUT, "Successfully finished TestSign!\n");
#endif

	errcode = 0;

loser:
	SECITEM_FreeItem(&refWrappedKey, PR_FALSE);
	SECITEM_FreeItem(&testWrappedKey, PR_FALSE);
	SECITEM_FreeItem(&ptext, PR_FALSE);
	SECITEM_FreeItem(&refSignature, PR_FALSE);
	SECITEM_FreeItem(&testSignature, PR_FALSE);
	if(refPrivk) {
		SECKEY_DestroyPrivateKey(refPrivk);
	}
	if(pubk) {
		SECKEY_DestroyPublicKey(pubk);
	}
	if(testPrivk) {
		SECKEY_DestroyPrivateKey(testPrivk);
	}
	if(refSymKey) {
		PK11_FreeSymKey(refSymKey);
	} 
	if(testSymKey) {
		PK11_FreeSymKey(testSymKey);
	}
	if(recoveredSymKey) {
		PK11_FreeSymKey(recoveredSymKey);
	}
	return errcode;
	

}

/**********************************************************************
 *
 * T e s t D i g e s t
 *
 */
int
TestDigest(CK_MECHANISM_TYPE mech)
{
	return 0;
}

/**********************************************************************
 *
 * T e s t H M A C
 *
 */
int
TestHMAC(CK_MECHANISM_TYPE mech)
{
	return 0;
}

/**********************************************************************
 *
 * G e t M e c h I n f o
 *
 */
MechInfo*
GetMechInfo(CK_MECHANISM_TYPE type)
{
	/* mechInfo array is sorted by type, so we can do a binary search
	   l is the left-most possible matching index
	   r is the rightmost possible matching index
	   mid is approximately the middle point between l and r */
	int l, r, mid;

	l = 0; r = numMechs-1;

	while(l <= r) {
		mid = (l+r)/2;
		if(mechInfo[mid].type == type) {
			return &(mechInfo[mid]);
		} else if(mechInfo[mid].type < type) {
			l = mid+1;
		} else {
			r = mid-1;
		}
	}

	/* If l > r, the pointers have crossed without finding the element. */
	return NULL;
}
