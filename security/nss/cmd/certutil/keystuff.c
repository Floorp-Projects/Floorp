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
 *   Dr Vipul Gupta <vipul.gupta@sun.com>, Sun Microsystems Laboratories
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
#include <string.h>
#include "secutil.h"

#if defined(XP_UNIX)
#include <unistd.h>
#include <sys/time.h>
#include <termios.h>
#endif

#if defined(XP_WIN) || defined (XP_PC)
#include <time.h>
#include <conio.h>
#endif

#if defined(__sun) && !defined(SVR4)
extern int fclose(FILE*);
extern int fprintf(FILE *, char *, ...);
extern int isatty(int);
extern char *sys_errlist[];
#define strerror(errno) sys_errlist[errno]
#endif

#include "nspr.h"
#include "prtypes.h"
#include "prtime.h"
#include "prlong.h"

#include "pk11func.h"

#define NUM_KEYSTROKES 120
#define RAND_BUF_SIZE 60

#define ERROR_BREAK rv = SECFailure;break;

const SEC_ASN1Template SECKEY_PQGParamsTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SECKEYPQGParams) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPQGParams,prime) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPQGParams,subPrime) },
    { SEC_ASN1_INTEGER, offsetof(SECKEYPQGParams,base) },
    { 0, }
};

/* returns 0 for success, -1 for failure (EOF encountered) */
static int
UpdateRNG(void)
{
    char *         randbuf;
    int            fd, i, count;
    int            c;
    int            rv		= 0;
#ifdef XP_UNIX
    cc_t           orig_cc_min;
    cc_t           orig_cc_time;
    tcflag_t       orig_lflag;
    struct termios tio;
#endif

#define FPS fprintf(stderr, 
    FPS "\n");
    FPS "A random seed must be generated that will be used in the\n");
    FPS "creation of your key.  One of the easiest ways to create a\n");
    FPS "random seed is to use the timing of keystrokes on a keyboard.\n");
    FPS "\n");
    FPS "To begin, type keys on the keyboard until this progress meter\n");
    FPS "is full.  DO NOT USE THE AUTOREPEAT FUNCTION ON YOUR KEYBOARD!\n");
    FPS "\n");
    FPS "\n");
    FPS "Continue typing until the progress meter is full:\n\n");
    FPS "|                                                            |\r");

    /* turn off echo on stdin & return on 1 char instead of NL */
    fd = fileno(stdin);

#if defined(XP_UNIX) && !defined(VMS)
    tcgetattr(fd, &tio);
    orig_lflag = tio.c_lflag;
    orig_cc_min = tio.c_cc[VMIN];
    orig_cc_time = tio.c_cc[VTIME];
    tio.c_lflag &= ~ECHO;
    tio.c_lflag &= ~ICANON;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
    tcsetattr(fd, TCSAFLUSH, &tio);
#endif

    /* Get random noise from keyboard strokes */
    randbuf = (char *) PORT_Alloc(RAND_BUF_SIZE);
    count = 0;
    while (count < NUM_KEYSTROKES+1) {
#ifdef VMS
	c = GENERIC_GETCHAR_NOECHO();
#elif XP_UNIX
	c = getc(stdin);
#else
	c = getch();
#endif
	if (c == EOF) {
	    rv = -1;
	    break;
	}
	PK11_RandomUpdate(randbuf, sizeof(randbuf));
	if (c != randbuf[0]) {
	    randbuf[0] = c;
	    FPS "\r|");
	    for (i=0; i<count/(NUM_KEYSTROKES/RAND_BUF_SIZE); i++) {
		FPS "*");
	    }
	    if (count%(NUM_KEYSTROKES/RAND_BUF_SIZE) == 1)
		FPS "/");
	    count++;
	}
    }
    free(randbuf); 

    FPS "\n\n");
    FPS "Finished.  Press enter to continue: ");
#if defined(VMS)
    while((c = GENERIC_GETCHAR_NO_ECHO()) != '\r' && c != EOF)
	;
#else
    while ((c = getc(stdin)) != '\n' && c != EOF)
	;
#endif
    if (c == EOF) 
	rv = -1;
    FPS "\n");

#undef FPS

#if defined(XP_UNIX) && !defined(VMS)
    /* set back termio the way it was */
    tio.c_lflag = orig_lflag;
    tio.c_cc[VMIN] = orig_cc_min;
    tio.c_cc[VTIME] = orig_cc_time;
    tcsetattr(fd, TCSAFLUSH, &tio);
#endif
    return rv;
}


static const unsigned char P[] = 
                           { 0x00, 0x8d, 0xf2, 0xa4, 0x94, 0x49, 0x22, 0x76,
			     0xaa, 0x3d, 0x25, 0x75, 0x9b, 0xb0, 0x68, 0x69,
			     0xcb, 0xea, 0xc0, 0xd8, 0x3a, 0xfb, 0x8d, 0x0c,
			     0xf7, 0xcb, 0xb8, 0x32, 0x4f, 0x0d, 0x78, 0x82,
			     0xe5, 0xd0, 0x76, 0x2f, 0xc5, 0xb7, 0x21, 0x0e,
			     0xaf, 0xc2, 0xe9, 0xad, 0xac, 0x32, 0xab, 0x7a,
			     0xac, 0x49, 0x69, 0x3d, 0xfb, 0xf8, 0x37, 0x24,
			     0xc2, 0xec, 0x07, 0x36, 0xee, 0x31, 0xc8, 0x02,
			     0x91 };
static const unsigned char Q[] = 
                           { 0x00, 0xc7, 0x73, 0x21, 0x8c, 0x73, 0x7e, 0xc8,
			     0xee, 0x99, 0x3b, 0x4f, 0x2d, 0xed, 0x30, 0xf4,
			     0x8e, 0xda, 0xce, 0x91, 0x5f };
static const unsigned char G[] = 
                           { 0x00, 0x62, 0x6d, 0x02, 0x78, 0x39, 0xea, 0x0a,
			     0x13, 0x41, 0x31, 0x63, 0xa5, 0x5b, 0x4c, 0xb5,
			     0x00, 0x29, 0x9d, 0x55, 0x22, 0x95, 0x6c, 0xef,
			     0xcb, 0x3b, 0xff, 0x10, 0xf3, 0x99, 0xce, 0x2c,
			     0x2e, 0x71, 0xcb, 0x9d, 0xe5, 0xfa, 0x24, 0xba,
			     0xbf, 0x58, 0xe5, 0xb7, 0x95, 0x21, 0x92, 0x5c,
			     0x9c, 0xc4, 0x2e, 0x9f, 0x6f, 0x46, 0x4b, 0x08,
			     0x8c, 0xc5, 0x72, 0xaf, 0x53, 0xe6, 0xd7, 0x88,
			     0x02 };

static const SECKEYPQGParams default_pqg_params = {
    NULL,
    { 0, (unsigned char *)P, sizeof(P) },
    { 0, (unsigned char *)Q, sizeof(Q) },
    { 0, (unsigned char *)G, sizeof(G) }
};

static SECKEYPQGParams *
decode_pqg_params(const char *str)
{
    char *buf;
    unsigned int len;
    PRArenaPool *arena;
    SECKEYPQGParams *params;
    SECStatus status;
 
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
        return NULL;
 
    params = PORT_ArenaZAlloc(arena, sizeof(SECKEYPQGParams));
    if (params == NULL)
        goto loser;
    params->arena = arena;
 
    buf = (char *)ATOB_AsciiToData(str, &len);
    if ((buf == NULL) || (len == 0))
        goto loser;
 
    status = SEC_ASN1Decode(arena, params, SECKEY_PQGParamsTemplate, buf, len);
    if (status != SECSuccess)
        goto loser;
 
    return params;
 
loser:
    if (arena != NULL)
        PORT_FreeArena(arena, PR_FALSE);
    return NULL;
}

void  
CERTUTIL_DestroyParamsPQG(SECKEYPQGParams *params)
{
    if (params->arena) {
        PORT_FreeArena(params->arena, PR_FALSE);
    }
}

static int
pqg_prime_bits(const SECKEYPQGParams *params)
{
    int primeBits = 0;
 
    if (params != NULL) {
	int i;
	for (i = 0; params->prime.data[i] == 0; i++) {
	    /* empty */;
	}
	primeBits = (params->prime.len - i) * 8;
    }
 
    return primeBits;
}

static char *
getPQGString(const char *filename)
{
    unsigned char *buf      = NULL;
    PRFileDesc    *src;
    PRInt32        numBytes;
    PRStatus       prStatus;
    PRFileInfo     info;

    src   = PR_Open(filename, PR_RDONLY, 0);
    if (!src) {
    	fprintf(stderr, "Failed to open PQG file %s\n", filename);
	return NULL;
    }

    prStatus = PR_GetOpenFileInfo(src, &info);

    if (prStatus == PR_SUCCESS) {
	buf = (unsigned char*)PORT_Alloc(info.size + 1);
    }
    if (!buf) {
	PR_Close(src);
    	fprintf(stderr, "Failed to read PQG file %s\n", filename);
	return NULL;
    }

    numBytes = PR_Read(src, buf, info.size);
    PR_Close(src);
    if (numBytes != info.size) {
	PORT_Free(buf);
    	fprintf(stderr, "Failed to read PQG file %s\n", filename);
	PORT_SetError(SEC_ERROR_IO);
	return NULL;
    }

    if (buf[numBytes-1] == '\n') 
    	numBytes--;
    if (buf[numBytes-1] == '\r') 
    	numBytes--;
    buf[numBytes] = 0;
    
    return (char *)buf;
}

static SECKEYPQGParams*
getpqgfromfile(int keyBits, const char *pqgFile)
{
    char *end, *str, *pqgString;
    SECKEYPQGParams* params = NULL;

    str = pqgString = getPQGString(pqgFile);
    if (!str) 
	return NULL;

    do {
	end = PORT_Strchr(str, ',');
	if (end)
	    *end = '\0';
	params = decode_pqg_params(str);
	if (params) {
	    int primeBits = pqg_prime_bits(params);
	    if (keyBits == primeBits)
		break;
	    CERTUTIL_DestroyParamsPQG(params);
	    params = NULL;
	}
	if (end)
	    str = end + 1;
    } while (end);

    PORT_Free(pqgString);
    return params;
}

static SECStatus 
CERTUTIL_FileForRNG(const char *noise)
{
    char buf[2048];
    PRFileDesc *fd;
    PRInt32 count;

    fd = PR_Open(noise,PR_RDONLY,0);
    if (!fd) {
	fprintf(stderr, "failed to open noise file.");
	return SECFailure;
    }

    do {
	count = PR_Read(fd,buf,sizeof(buf));
	if (count > 0) {
	    PK11_RandomUpdate(buf,count);
	}
    } while (count > 0);

    PR_Close(fd);
    return SECSuccess;
}

#ifdef NSS_ENABLE_ECC
typedef struct curveNameTagPairStr {
    char *curveName;
    SECOidTag curveOidTag;
} CurveNameTagPair;

#define DEFAULT_CURVE_OID_TAG  SEC_OID_SECG_EC_SECP192R1
/* #define DEFAULT_CURVE_OID_TAG  SEC_OID_SECG_EC_SECP160R1 */

static CurveNameTagPair nameTagPair[] =
{ 
  { "sect163k1", SEC_OID_SECG_EC_SECT163K1},
  { "nistk163", SEC_OID_SECG_EC_SECT163K1},
  { "sect163r1", SEC_OID_SECG_EC_SECT163R1},
  { "sect163r2", SEC_OID_SECG_EC_SECT163R2},
  { "nistb163", SEC_OID_SECG_EC_SECT163R2},
  { "sect193r1", SEC_OID_SECG_EC_SECT193R1},
  { "sect193r2", SEC_OID_SECG_EC_SECT193R2},
  { "sect233k1", SEC_OID_SECG_EC_SECT233K1},
  { "nistk233", SEC_OID_SECG_EC_SECT233K1},
  { "sect233r1", SEC_OID_SECG_EC_SECT233R1},
  { "nistb233", SEC_OID_SECG_EC_SECT233R1},
  { "sect239k1", SEC_OID_SECG_EC_SECT239K1},
  { "sect283k1", SEC_OID_SECG_EC_SECT283K1},
  { "nistk283", SEC_OID_SECG_EC_SECT283K1},
  { "sect283r1", SEC_OID_SECG_EC_SECT283R1},
  { "nistb283", SEC_OID_SECG_EC_SECT283R1},
  { "sect409k1", SEC_OID_SECG_EC_SECT409K1},
  { "nistk409", SEC_OID_SECG_EC_SECT409K1},
  { "sect409r1", SEC_OID_SECG_EC_SECT409R1},
  { "nistb409", SEC_OID_SECG_EC_SECT409R1},
  { "sect571k1", SEC_OID_SECG_EC_SECT571K1},
  { "nistk571", SEC_OID_SECG_EC_SECT571K1},
  { "sect571r1", SEC_OID_SECG_EC_SECT571R1},
  { "nistb571", SEC_OID_SECG_EC_SECT571R1},
  { "secp160k1", SEC_OID_SECG_EC_SECP160K1},
  { "secp160r1", SEC_OID_SECG_EC_SECP160R1},
  { "secp160r2", SEC_OID_SECG_EC_SECP160R2},
  { "secp192k1", SEC_OID_SECG_EC_SECP192K1},
  { "secp192r1", SEC_OID_SECG_EC_SECP192R1},
  { "nistp192", SEC_OID_SECG_EC_SECP192R1},
  { "secp224k1", SEC_OID_SECG_EC_SECP224K1},
  { "secp224r1", SEC_OID_SECG_EC_SECP224R1},
  { "nistp224", SEC_OID_SECG_EC_SECP224R1},
  { "secp256k1", SEC_OID_SECG_EC_SECP256K1},
  { "secp256r1", SEC_OID_SECG_EC_SECP256R1},
  { "nistp256", SEC_OID_SECG_EC_SECP256R1},
  { "secp384r1", SEC_OID_SECG_EC_SECP384R1},
  { "nistp384", SEC_OID_SECG_EC_SECP384R1},
  { "secp521r1", SEC_OID_SECG_EC_SECP521R1},
  { "nistp521", SEC_OID_SECG_EC_SECP521R1},

  { "prime192v1", SEC_OID_ANSIX962_EC_PRIME192V1 },
  { "prime192v2", SEC_OID_ANSIX962_EC_PRIME192V2 },
  { "prime192v3", SEC_OID_ANSIX962_EC_PRIME192V3 },
  { "prime239v1", SEC_OID_ANSIX962_EC_PRIME239V1 },
  { "prime239v2", SEC_OID_ANSIX962_EC_PRIME239V2 },
  { "prime239v3", SEC_OID_ANSIX962_EC_PRIME239V3 },

  { "c2pnb163v1", SEC_OID_ANSIX962_EC_C2PNB163V1 },
  { "c2pnb163v2", SEC_OID_ANSIX962_EC_C2PNB163V2 },
  { "c2pnb163v3", SEC_OID_ANSIX962_EC_C2PNB163V3 },
  { "c2pnb176v1", SEC_OID_ANSIX962_EC_C2PNB176V1 },
  { "c2tnb191v1", SEC_OID_ANSIX962_EC_C2TNB191V1 },
  { "c2tnb191v2", SEC_OID_ANSIX962_EC_C2TNB191V2 },
  { "c2tnb191v3", SEC_OID_ANSIX962_EC_C2TNB191V3 },
  { "c2onb191v4", SEC_OID_ANSIX962_EC_C2ONB191V4 },
  { "c2onb191v5", SEC_OID_ANSIX962_EC_C2ONB191V5 },
  { "c2pnb208w1", SEC_OID_ANSIX962_EC_C2PNB208W1 },
  { "c2tnb239v1", SEC_OID_ANSIX962_EC_C2TNB239V1 },
  { "c2tnb239v2", SEC_OID_ANSIX962_EC_C2TNB239V2 },
  { "c2tnb239v3", SEC_OID_ANSIX962_EC_C2TNB239V3 },
  { "c2onb239v4", SEC_OID_ANSIX962_EC_C2ONB239V4 },
  { "c2onb239v5", SEC_OID_ANSIX962_EC_C2ONB239V5 },
  { "c2pnb272w1", SEC_OID_ANSIX962_EC_C2PNB272W1 },
  { "c2pnb304w1", SEC_OID_ANSIX962_EC_C2PNB304W1 },
  { "c2tnb359v1", SEC_OID_ANSIX962_EC_C2TNB359V1 },
  { "c2pnb368w1", SEC_OID_ANSIX962_EC_C2PNB368W1 },
  { "c2tnb431r1", SEC_OID_ANSIX962_EC_C2TNB431R1 },

  { "secp112r1", SEC_OID_SECG_EC_SECP112R1},
  { "secp112r2", SEC_OID_SECG_EC_SECP112R2},
  { "secp128r1", SEC_OID_SECG_EC_SECP128R1},
  { "secp128r2", SEC_OID_SECG_EC_SECP128R2},

  { "sect113r1", SEC_OID_SECG_EC_SECT113R1},
  { "sect113r2", SEC_OID_SECG_EC_SECT113R2},
  { "sect131r1", SEC_OID_SECG_EC_SECT131R1},
  { "sect131r2", SEC_OID_SECG_EC_SECT131R2},
};

static SECKEYECParams * 
getECParams(const char *curve)
{
    SECKEYECParams *ecparams;
    SECOidData *oidData = NULL;
    SECOidTag curveOidTag = SEC_OID_UNKNOWN; /* default */
    int i, numCurves;

    if (curve != NULL) {
        numCurves = sizeof(nameTagPair)/sizeof(CurveNameTagPair);
	for (i = 0; ((i < numCurves) && (curveOidTag == SEC_OID_UNKNOWN)); 
	     i++) {
	    if (PL_strcmp(curve, nameTagPair[i].curveName) == 0)
	        curveOidTag = nameTagPair[i].curveOidTag;
	}
    }

    /* Return NULL if curve name is not recognized */
    if ((curveOidTag == SEC_OID_UNKNOWN) || 
	(oidData = SECOID_FindOIDByTag(curveOidTag)) == NULL) {
        fprintf(stderr, "Unrecognized elliptic curve %s\n", curve);
	return NULL;
    }

    ecparams = SECITEM_AllocItem(NULL, NULL, (2 + oidData->oid.len));

    /* 
     * ecparams->data needs to contain the ASN encoding of an object ID (OID)
     * representing the named curve. The actual OID is in 
     * oidData->oid.data so we simply prepend 0x06 and OID length
     */
    ecparams->data[0] = SEC_ASN1_OBJECT_ID;
    ecparams->data[1] = oidData->oid.len;
    memcpy(ecparams->data + 2, oidData->oid.data, oidData->oid.len);

    return ecparams;
}
#endif /* NSS_ENABLE_ECC */

SECKEYPrivateKey *
CERTUTIL_GeneratePrivateKey(KeyType keytype, PK11SlotInfo *slot, int size,
			    int publicExponent, const char *noise, 
			    SECKEYPublicKey **pubkeyp, const char *pqgFile,
                            secuPWData *pwdata)
{
    CK_MECHANISM_TYPE  mechanism;
    SECOidTag          algtag;
    PK11RSAGenParams   rsaparams;
    SECKEYPQGParams  * dsaparams = NULL;
    void             * params;
    SECKEYPrivateKey * privKey = NULL;

    if (slot == NULL)
	return NULL;

    if (PK11_Authenticate(slot, PR_TRUE, pwdata) != SECSuccess)
	return NULL;

    /*
     * Do some random-number initialization.
     */

    if (noise) {
    	SECStatus rv = CERTUTIL_FileForRNG(noise);
	if (rv != SECSuccess) {
	    PORT_SetError(PR_END_OF_FILE_ERROR); /* XXX */
	    return NULL;
    	}
    } else {
	int rv = UpdateRNG();
	if (rv) {
	    PORT_SetError(PR_END_OF_FILE_ERROR);
	    return NULL;
	}
    }

    switch (keytype) {
    case rsaKey:
	rsaparams.keySizeInBits = size;
	rsaparams.pe = publicExponent;
	mechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;
	algtag = SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION;
	params = &rsaparams;
	break;
    case dsaKey:
	mechanism = CKM_DSA_KEY_PAIR_GEN;
	algtag = SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST;
	if (pqgFile) {
	    dsaparams = getpqgfromfile(size, pqgFile);
	    if (dsaparams == NULL)
	    	return NULL;
	    params = dsaparams;
	} else {
	    /* cast away const, and don't set dsaparams */
	    params = (void *)&default_pqg_params;
	}
	break;
#ifdef NSS_ENABLE_ECC
    case ecKey:
	mechanism = CKM_EC_KEY_PAIR_GEN;
	/* For EC keys, PQGFile determines EC parameters */
	if ((params = (void *) getECParams(pqgFile)) == NULL)
	    return NULL;
	break;
#endif /* NSS_ENABLE_ECC */
    default:
	return NULL;
    }

    fprintf(stderr, "\n\n");
    fprintf(stderr, "Generating key.  This may take a few moments...\n\n");

    privKey = PK11_GenerateKeyPair(slot, mechanism, params, pubkeyp,
				PR_TRUE /*isPerm*/, PR_TRUE /*isSensitive*/, 
				pwdata /*wincx*/);
    /* free up the params */
    switch (keytype) {
    case rsaKey: /* nothing to free */                        break;
    case dsaKey: if (dsaparams) CERTUTIL_DestroyParamsPQG(dsaparams); 
	                                                      break;
#ifdef NSS_ENABLE_ECC
    case ecKey: SECITEM_FreeItem((SECItem *)params, PR_TRUE); break;
#endif
    }
    return privKey;
}
