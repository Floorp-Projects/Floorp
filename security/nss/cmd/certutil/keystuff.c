/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    char           randbuf[RAND_BUF_SIZE];
    int            fd,  count;
    int            c;
    int            rv		= 0;
#ifdef XP_UNIX
    cc_t           orig_cc_min;
    cc_t           orig_cc_time;
    tcflag_t       orig_lflag;
    struct termios tio;
#endif
    char meter[] = { 
      "\r|                                                            |" };

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
    FPS "%s", meter);
    FPS "\r|");

    /* turn off echo on stdin & return on 1 char instead of NL */
    fd = fileno(stdin);

#if defined(XP_UNIX)
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
    count = 0;
    while (count < sizeof randbuf) {
#if defined(XP_UNIX)
	c = getc(stdin);
#else
	c = getch();
#endif
	if (c == EOF) {
	    rv = -1;
	    break;
	}
	randbuf[count] = c;
	if (count == 0 || c != randbuf[count-1]) {
	    count++;
	    FPS "*");
	}
    }
    PK11_RandomUpdate(randbuf, sizeof randbuf);
    memset(randbuf, 0, sizeof randbuf);

    FPS "\n\n");
    FPS "Finished.  Press enter to continue: ");
    while ((c = getc(stdin)) != '\n' && c != EOF)
	;
    if (c == EOF) 
	rv = -1;
    FPS "\n");

#undef FPS

#if defined(XP_UNIX)
    /* set back termio the way it was */
    tio.c_lflag = orig_lflag;
    tio.c_cc[VMIN] = orig_cc_min;
    tio.c_cc[VTIME] = orig_cc_time;
    tcsetattr(fd, TCSAFLUSH, &tio);
#endif
    return rv;
}

static const unsigned char P[] = { 0, 
       0x98, 0xef, 0x3a, 0xae, 0x70, 0x98, 0x9b, 0x44, 
       0xdb, 0x35, 0x86, 0xc1, 0xb6, 0xc2, 0x47, 0x7c, 
       0xb4, 0xff, 0x99, 0xe8, 0xae, 0x44, 0xf2, 0xeb, 
       0xc3, 0xbe, 0x23, 0x0f, 0x65, 0xd0, 0x4c, 0x04, 
       0x82, 0x90, 0xa7, 0x9d, 0x4a, 0xc8, 0x93, 0x7f, 
       0x41, 0xdf, 0xf8, 0x80, 0x6b, 0x0b, 0x68, 0x7f, 
       0xaf, 0xe4, 0xa8, 0xb5, 0xb2, 0x99, 0xc3, 0x69, 
       0xfb, 0x3f, 0xe7, 0x1b, 0xd0, 0x0f, 0xa9, 0x7a, 
       0x4a, 0x04, 0xbf, 0x50, 0x9e, 0x22, 0x33, 0xb8, 
       0x89, 0x53, 0x24, 0x10, 0xf9, 0x68, 0x77, 0xad, 
       0xaf, 0x10, 0x68, 0xb8, 0xd3, 0x68, 0x5d, 0xa3, 
       0xc3, 0xeb, 0x72, 0x3b, 0xa0, 0x0b, 0x73, 0x65, 
       0xc5, 0xd1, 0xfa, 0x8c, 0xc0, 0x7d, 0xaa, 0x52, 
       0x29, 0x34, 0x44, 0x01, 0xbf, 0x12, 0x25, 0xfe, 
       0x18, 0x0a, 0xc8, 0x3f, 0xc1, 0x60, 0x48, 0xdb, 
       0xad, 0x93, 0xb6, 0x61, 0x67, 0xd7, 0xa8, 0x2d };
static const unsigned char Q[] = { 0,
       0xb5, 0xb0, 0x84, 0x8b, 0x44, 0x29, 0xf6, 0x33, 
       0x59, 0xa1, 0x3c, 0xbe, 0xd2, 0x7f, 0x35, 0xa1, 
       0x76, 0x27, 0x03, 0x81                         };
static const unsigned char G[] = { 
       0x04, 0x0e, 0x83, 0x69, 0xf1, 0xcd, 0x7d, 0xe5, 
       0x0c, 0x78, 0x93, 0xd6, 0x49, 0x6f, 0x00, 0x04, 
       0x4e, 0x0e, 0x6c, 0x37, 0xaa, 0x38, 0x22, 0x47, 
       0xd2, 0x58, 0xec, 0x83, 0x12, 0x95, 0xf9, 0x9c, 
       0xf1, 0xf4, 0x27, 0xff, 0xd7, 0x99, 0x57, 0x35, 
       0xc6, 0x64, 0x4c, 0xc0, 0x47, 0x12, 0x31, 0x50, 
       0x82, 0x3c, 0x2a, 0x07, 0x03, 0x01, 0xef, 0x30, 
       0x09, 0x89, 0x82, 0x41, 0x76, 0x71, 0xda, 0x9e, 
       0x57, 0x8b, 0x76, 0x38, 0x37, 0x5f, 0xa5, 0xcd, 
       0x32, 0x84, 0x45, 0x8d, 0x4c, 0x17, 0x54, 0x2b, 
       0x5d, 0xc2, 0x6b, 0xba, 0x3e, 0xa0, 0x7b, 0x95, 
       0xd7, 0x00, 0x42, 0xf7, 0x08, 0xb8, 0x83, 0x87, 
       0x60, 0xe1, 0xe5, 0xf4, 0x1a, 0x54, 0xc2, 0x20, 
       0xda, 0x38, 0x3a, 0xd1, 0xb6, 0x10, 0xf4, 0xcb, 
       0x35, 0xda, 0x97, 0x92, 0x87, 0xd6, 0xa5, 0x37, 
       0x62, 0xb4, 0x93, 0x4a, 0x15, 0x21, 0xa5, 0x10 };

/*  h:
 *      4a:76:30:89:eb:e1:81:7c:99:0b:39:7f:95:4a:65:72:
 *      c6:b4:05:92:48:6c:3c:b2:7e:e7:39:f3:92:7d:c1:3f:
 *      bf:e1:fd:b3:4a:46:3e:ce:29:80:e3:d6:f4:59:c6:92:
 *      16:2b:0e:d7:d6:bb:ef:94:36:31:c2:66:46:c5:4a:77:
 *      aa:95:84:ef:99:7e:e3:9c:d9:a0:32:42:09:b6:4e:d0:
 *      b3:c8:5e:06:df:a1:ac:4d:2d:f9:08:c2:cb:4b:a4:42:
 *      db:8a:5b:de:25:6e:2b:5b:ca:00:75:2c:57:00:18:aa:
 *      68:59:a1:94:03:07:94:78:38:bc:f8:7c:1e:1c:a3:2e
 *  SEED:
 *      b5:44:66:c9:0f:f1:ca:1c:95:45:ce:90:74:89:14:f2:
 *      13:3e:23:5a:b0:6a:bf:86:ad:cb:a0:7d:ce:3b:c8:16:
 *      7f:2d:a2:1a:cb:33:7d:c1:e7:d7:07:aa:1b:a2:d7:89:
 *      f5:a4:db:f7:8b:50:00:cd:b4:7d:25:81:3f:f8:a8:dd:
 *      6c:46:e5:77:b5:60:7e:75:79:b8:99:57:c1:c4:f3:f7:
 *      17:ca:43:00:b8:33:b6:06:8f:4d:91:ed:23:a5:66:1b:
 *      ef:14:d7:bc:21:2b:82:d8:ab:fa:fd:a7:c3:4d:bf:52:
 *      af:8e:57:59:61:1a:4e:65:c6:90:d6:a6:ff:0b:15:b1
 *  g:       1024
 *  counter: 1003
 */

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
    PLArenaPool *arena;
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

#ifndef NSS_DISABLE_ECC
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
#endif /* NSS_DISABLE_ECC */

SECKEYPrivateKey *
CERTUTIL_GeneratePrivateKey(KeyType keytype, PK11SlotInfo *slot, int size,
			    int publicExponent, const char *noise, 
			    SECKEYPublicKey **pubkeyp, const char *pqgFile,
			    PK11AttrFlags attrFlags, CK_FLAGS opFlagsOn,
			    CK_FLAGS opFlagsOff, secuPWData *pwdata)
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
#ifndef NSS_DISABLE_ECC
    case ecKey:
	mechanism = CKM_EC_KEY_PAIR_GEN;
	/* For EC keys, PQGFile determines EC parameters */
	if ((params = (void *) getECParams(pqgFile)) == NULL)
	    return NULL;
	break;
#endif /* NSS_DISABLE_ECC */
    default:
	return NULL;
    }

    fprintf(stderr, "\n\n");
    fprintf(stderr, "Generating key.  This may take a few moments...\n\n");

    privKey = PK11_GenerateKeyPairWithOpFlags(slot, mechanism, params, pubkeyp,
				attrFlags, opFlagsOn, opFlagsOn|opFlagsOff,
				pwdata /*wincx*/);
    /* free up the params */
    switch (keytype) {
    case dsaKey: if (dsaparams) CERTUTIL_DestroyParamsPQG(dsaparams); 
	                                                      break;
#ifndef NSS_DISABLE_ECC
    case ecKey: SECITEM_FreeItem((SECItem *)params, PR_TRUE); break;
#endif
    default: /* nothing to free */                            break;
    }
    return privKey;
}
