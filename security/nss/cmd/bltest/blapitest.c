/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>

#include "blapi.h"
#include "secrng.h"
#include "prmem.h"
#include "prprf.h"
#include "prtime.h"
#include "prsystem.h"
#include "plstr.h"
#include "nssb64.h"
#include "basicutil.h"
#include "plgetopt.h"
#include "softoken.h"
#include "nspr.h"
#include "secport.h"
#include "secoid.h"
#include "nssutil.h"

#ifdef NSS_ENABLE_ECC
#include "ecl-curve.h"
SECStatus EC_DecodeParams(const SECItem *encodedParams, 
	ECParams **ecparams);
SECStatus EC_CopyParams(PLArenaPool *arena, ECParams *dstParams,
	      const ECParams *srcParams);
#endif

/* Temporary - add debugging output on windows for RSA to track QA failure */
#ifdef _WIN32
#define TRACK_BLTEST_BUG
    char __bltDBG[] = "BLTEST DEBUG";
#endif

char *progName;
char *testdir = NULL;

#define BLTEST_DEFAULT_CHUNKSIZE 4096

#define WORDSIZE sizeof(unsigned long)

#define CHECKERROR(rv, ln) \
    if (rv) { \
	PRErrorCode prerror = PR_GetError(); \
	PR_fprintf(PR_STDERR, "%s: ERR %d (%s) at line %d.\n", progName, \
	prerror, PORT_ErrorToString(prerror), ln); \
	exit(-1); \
    }

/* Macros for performance timing. */
#define TIMESTART() \
    time1 = PR_IntervalNow();

#define TIMEFINISH(time, reps) \
    time2 = (PRIntervalTime)(PR_IntervalNow() - time1); \
    time1 = PR_IntervalToMilliseconds(time2); \
    time = ((double)(time1))/reps;

#define TIMEMARK(seconds) \
    time1 = PR_SecondsToInterval(seconds); \
    { \
        PRInt64 tmp, L100; \
        LL_I2L(L100, 100); \
        if (time2 == 0) { \
            time2 = 1; \
        } \
        LL_DIV(tmp, time1, time2); \
        if (tmp < 10) { \
            if (tmp == 0) { \
                opsBetweenChecks = 1; \
            } else { \
                LL_L2I(opsBetweenChecks, tmp); \
            } \
        } else { \
            opsBetweenChecks = 10; \
        } \
    } \
    time2 = time1; \
    time1 = PR_IntervalNow();

#define TIMETOFINISH() \
    PR_IntervalNow() - time1 >= time2

static void Usage()
{
#define PRINTUSAGE(subject, option, predicate) \
    fprintf(stderr, "%10s %s\t%s\n", subject, option, predicate);
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "[-DEHSVR]", "List available cipher modes"); /* XXX */
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-E -m mode ", "Encrypt a buffer");
    PRINTUSAGE("",	"", "[-i plaintext] [-o ciphertext] [-k key] [-v iv]");
    PRINTUSAGE("",	"", "[-b bufsize] [-g keysize] [-e exp] [-r rounds]");
    PRINTUSAGE("",	"", "[-w wordsize] [-p repetitions | -5 time_interval]");
    PRINTUSAGE("",	"", "[-4 th_num]");
    PRINTUSAGE("",	"-m", "cipher mode to use");
    PRINTUSAGE("",	"-i", "file which contains input buffer");
    PRINTUSAGE("",	"-o", "file for output buffer");
    PRINTUSAGE("",	"-k", "file which contains key");
    PRINTUSAGE("",	"-v", "file which contains initialization vector");
    PRINTUSAGE("",	"-b", "size of input buffer");
    PRINTUSAGE("",	"-g", "key size (in bytes)");
    PRINTUSAGE("",	"-p", "do performance test");
    PRINTUSAGE("",	"-4", "run test in multithread mode. th_num number of parallel threads");
    PRINTUSAGE("",	"-5", "run test for specified time interval(in seconds)");
    PRINTUSAGE("",	"--aad", "File with contains additional auth data");
    PRINTUSAGE("(rsa)", "-e", "rsa public exponent");
    PRINTUSAGE("(rc5)", "-r", "number of rounds");
    PRINTUSAGE("(rc5)", "-w", "wordsize (32 or 64)");
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-D -m mode", "Decrypt a buffer");
    PRINTUSAGE("",	"", "[-i plaintext] [-o ciphertext] [-k key] [-v iv]");
    PRINTUSAGE("",	"", "[-p repetitions | -5 time_interval] [-4 th_num]");
    PRINTUSAGE("",	"-m", "cipher mode to use");
    PRINTUSAGE("",	"-i", "file which contains input buffer");
    PRINTUSAGE("",	"-o", "file for output buffer");
    PRINTUSAGE("",	"-k", "file which contains key");
    PRINTUSAGE("",	"-v", "file which contains initialization vector");
    PRINTUSAGE("",	"-p", "do performance test");
    PRINTUSAGE("",	"-4", "run test in multithread mode. th_num number of parallel threads");
    PRINTUSAGE("",	"-5", "run test for specified time interval(in seconds)");
    PRINTUSAGE("",	"--aad", "File with contains additional auth data");
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-H -m mode", "Hash a buffer");
    PRINTUSAGE("",	"", "[-i plaintext] [-o hash]");
    PRINTUSAGE("",	"", "[-b bufsize]");
    PRINTUSAGE("",	"", "[-p repetitions | -5 time_interval] [-4 th_num]");
    PRINTUSAGE("",	"-m", "cipher mode to use");
    PRINTUSAGE("",	"-i", "file which contains input buffer");
    PRINTUSAGE("",	"-o", "file for hash");
    PRINTUSAGE("",	"-b", "size of input buffer");
    PRINTUSAGE("",	"-p", "do performance test");
    PRINTUSAGE("",	"-4", "run test in multithread mode. th_num number of parallel threads");
    PRINTUSAGE("",	"-5", "run test for specified time interval(in seconds)");
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-S -m mode", "Sign a buffer");
    PRINTUSAGE("",	"", "[-i plaintext] [-o signature] [-k key]");
    PRINTUSAGE("",	"", "[-b bufsize]");
#ifdef NSS_ENABLE_ECC
    PRINTUSAGE("",	"", "[-n curvename]");
#endif
    PRINTUSAGE("",	"", "[-p repetitions | -5 time_interval] [-4 th_num]");
    PRINTUSAGE("",	"-m", "cipher mode to use");
    PRINTUSAGE("",	"-i", "file which contains input buffer");
    PRINTUSAGE("",	"-o", "file for signature");
    PRINTUSAGE("",	"-k", "file which contains key");
#ifdef NSS_ENABLE_ECC
    PRINTUSAGE("",	"-n", "name of curve for EC key generation; one of:");
    PRINTUSAGE("",  "",   "  sect163k1, nistk163, sect163r1, sect163r2,");
    PRINTUSAGE("",  "",   "  nistb163, sect193r1, sect193r2, sect233k1, nistk233,");
    PRINTUSAGE("",  "",   "  sect233r1, nistb233, sect239k1, sect283k1, nistk283,");
    PRINTUSAGE("",  "",   "  sect283r1, nistb283, sect409k1, nistk409, sect409r1,");
    PRINTUSAGE("",  "",   "  nistb409, sect571k1, nistk571, sect571r1, nistb571,");
    PRINTUSAGE("",  "",   "  secp160k1, secp160r1, secp160r2, secp192k1, secp192r1,");
    PRINTUSAGE("",  "",   "  nistp192, secp224k1, secp224r1, nistp224, secp256k1,");
    PRINTUSAGE("",  "",   "  secp256r1, nistp256, secp384r1, nistp384, secp521r1,");
    PRINTUSAGE("",  "",   "  nistp521, prime192v1, prime192v2, prime192v3,");
    PRINTUSAGE("",  "",   "  prime239v1, prime239v2, prime239v3, c2pnb163v1,");
    PRINTUSAGE("",  "",   "  c2pnb163v2, c2pnb163v3, c2pnb176v1, c2tnb191v1,");
    PRINTUSAGE("",  "",   "  c2tnb191v2, c2tnb191v3, c2onb191v4, c2onb191v5,");
    PRINTUSAGE("",  "",   "  c2pnb208w1, c2tnb239v1, c2tnb239v2, c2tnb239v3,");
    PRINTUSAGE("",  "",   "  c2onb239v4, c2onb239v5, c2pnb272w1, c2pnb304w1,");
    PRINTUSAGE("",  "",   "  c2tnb359w1, c2pnb368w1, c2tnb431r1, secp112r1,");
    PRINTUSAGE("",  "",   "  secp112r2, secp128r1, secp128r2, sect113r1, sect113r2,");
    PRINTUSAGE("",  "",   "  sect131r1, sect131r2");
#endif
    PRINTUSAGE("",	"-p", "do performance test");
    PRINTUSAGE("",	"-4", "run test in multithread mode. th_num number of parallel threads");
    PRINTUSAGE("",	"-5", "run test for specified time interval(in seconds)");
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-V -m mode", "Verify a signed buffer");
    PRINTUSAGE("",	"", "[-i plaintext] [-s signature] [-k key]");
    PRINTUSAGE("",	"", "[-p repetitions | -5 time_interval] [-4 th_num]");
    PRINTUSAGE("",	"-m", "cipher mode to use");
    PRINTUSAGE("",	"-i", "file which contains input buffer");
    PRINTUSAGE("",	"-s", "file which contains signature of input buffer");
    PRINTUSAGE("",	"-k", "file which contains key");
    PRINTUSAGE("",	"-p", "do performance test");
    PRINTUSAGE("",	"-4", "run test in multithread mode. th_num number of parallel threads");
    PRINTUSAGE("",	"-5", "run test for specified time interval(in seconds)");
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-N -m mode -b bufsize", 
                                            "Create a nonce plaintext and key");
    PRINTUSAGE("",      "", "[-g keysize] [-u cxreps]");
    PRINTUSAGE("",	"-g", "key size (in bytes)");
    PRINTUSAGE("",      "-u", "number of repetitions of context creation");
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-R [-g keysize] [-e exp]", 
                                            "Test the RSA populate key function");
    PRINTUSAGE("",      "", "[-r repetitions]");
    PRINTUSAGE("",	"-g", "key size (in bytes)");
    PRINTUSAGE("", 	"-e", "rsa public exponent");
    PRINTUSAGE("", 	"-r", "repetitions of the test");
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-F", "Run the FIPS self-test");
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-T [-m mode1,mode2...]", "Run the BLAPI self-test");
    fprintf(stderr, "\n");
    exit(1);
}

/*  Helper functions for ascii<-->binary conversion/reading/writing */

/* XXX argh */
struct item_with_arena {
    SECItem	*item;
    PLArenaPool *arena;
};

static PRInt32
get_binary(void *arg, const unsigned char *ibuf, PRInt32 size)
{
    struct item_with_arena *it = arg;
    SECItem *binary = it->item;
    SECItem *tmp;
    int index;
    if (binary->data == NULL) {
	tmp = SECITEM_AllocItem(it->arena, NULL, size);
	binary->data = tmp->data;
	binary->len = tmp->len;
	index = 0;
    } else {
	SECITEM_ReallocItem(NULL, binary, binary->len, binary->len + size);
	index = binary->len;
    }
    PORT_Memcpy(&binary->data[index], ibuf, size);
    return binary->len;
}

static SECStatus
atob(SECItem *ascii, SECItem *binary, PLArenaPool *arena)
{
    SECStatus status;
    NSSBase64Decoder *cx;
    struct item_with_arena it;
    int len;
    binary->data = NULL;
    binary->len = 0;
    it.item = binary;
    it.arena = arena;
    len = (strncmp((const char *)&ascii->data[ascii->len-2],"\r\n",2)) ?
           ascii->len : ascii->len-2;
    cx = NSSBase64Decoder_Create(get_binary, &it);
    status = NSSBase64Decoder_Update(cx, (const char *)ascii->data, len);
    status = NSSBase64Decoder_Destroy(cx, PR_FALSE);
    return status;
}

static PRInt32
output_ascii(void *arg, const char *obuf, PRInt32 size)
{
    PRFileDesc *outfile = arg;
    PRInt32 nb = PR_Write(outfile, obuf, size);
    if (nb != size) {
	PORT_SetError(SEC_ERROR_IO);
	return -1;
    }
    return nb;
}

static SECStatus
btoa_file(SECItem *binary, PRFileDesc *outfile)
{
    SECStatus status;
    NSSBase64Encoder *cx;
    if (binary->len == 0)
	return SECSuccess;
    cx = NSSBase64Encoder_Create(output_ascii, outfile);
    status = NSSBase64Encoder_Update(cx, binary->data, binary->len);
    status = NSSBase64Encoder_Destroy(cx, PR_FALSE);
    status = PR_Write(outfile, "\r\n", 2);
    return status;
}

SECStatus
hex_from_2char(unsigned char *c2, unsigned char *byteval)
{
    int i;
    unsigned char offset;
    *byteval = 0;
    for (i=0; i<2; i++) {
	if (c2[i] >= '0' && c2[i] <= '9') {
	    offset = c2[i] - '0';
	    *byteval |= offset << 4*(1-i);
	} else if (c2[i] >= 'a' && c2[i] <= 'f') {
	    offset = c2[i] - 'a';
	    *byteval |= (offset + 10) << 4*(1-i);
	} else if (c2[i] >= 'A' && c2[i] <= 'F') {
	    offset = c2[i] - 'A';
	    *byteval |= (offset + 10) << 4*(1-i);
	} else {
	    return SECFailure;
	}
    }
    return SECSuccess;
}

SECStatus
char2_from_hex(unsigned char byteval, char *c2)
{
    int i;
    unsigned char offset;
    for (i=0; i<2; i++) {
	offset = (byteval >> 4*(1-i)) & 0x0f;
	if (offset < 10) {
	    c2[i] = '0' + offset;
	} else {
	    c2[i] = 'A' + offset - 10;
	}
    }
    return SECSuccess;
}

void
serialize_key(SECItem *it, int ni, PRFileDesc *file)
{
    unsigned char len[4];
    int i;
    SECStatus status;
    NSSBase64Encoder *cx;
    cx = NSSBase64Encoder_Create(output_ascii, file);
    for (i=0; i<ni; i++, it++) {
	len[0] = (it->len >> 24) & 0xff;
	len[1] = (it->len >> 16) & 0xff;
	len[2] = (it->len >>  8) & 0xff;
	len[3] = (it->len	 & 0xff);
	status = NSSBase64Encoder_Update(cx, len, 4);
	status = NSSBase64Encoder_Update(cx, it->data, it->len);
    }
    status = NSSBase64Encoder_Destroy(cx, PR_FALSE);
    status = PR_Write(file, "\r\n", 2);
}

void
key_from_filedata(PLArenaPool *arena, SECItem *it, int ns, int ni, SECItem *filedata)
{
    int fpos = 0;
    int i, len;
    unsigned char *buf = filedata->data;
    for (i=0; i<ni; i++) {
	len  = (buf[fpos++] & 0xff) << 24;
	len |= (buf[fpos++] & 0xff) << 16;
	len |= (buf[fpos++] & 0xff) <<  8;
	len |= (buf[fpos++] & 0xff);
	if (ns <= i) {
	    if (len > 0) {
		it->len = len;
		it->data = PORT_ArenaAlloc(arena, it->len);
		PORT_Memcpy(it->data, &buf[fpos], it->len);
	    } else {
		it->len = 0;
		it->data = NULL;
	    }
	    it++;
	}
	fpos += len;
    }
}

static RSAPrivateKey *
rsakey_from_filedata(SECItem *filedata)
{
    RSAPrivateKey *key;
    PLArenaPool *arena;
    arena = PORT_NewArena(BLTEST_DEFAULT_CHUNKSIZE);
    key = (RSAPrivateKey *)PORT_ArenaZAlloc(arena, sizeof(RSAPrivateKey));
    key->arena = arena;
    key_from_filedata(arena, &key->version, 0, 9, filedata);
    return key;
}

static PQGParams *
pqg_from_filedata(SECItem *filedata)
{
    PQGParams *pqg;
    PLArenaPool *arena;
    arena = PORT_NewArena(BLTEST_DEFAULT_CHUNKSIZE);
    pqg = (PQGParams *)PORT_ArenaZAlloc(arena, sizeof(PQGParams));
    pqg->arena = arena;
    key_from_filedata(arena, &pqg->prime, 0, 3, filedata);
    return pqg;
}

static DSAPrivateKey *
dsakey_from_filedata(SECItem *filedata)
{
    DSAPrivateKey *key;
    PLArenaPool *arena;
    arena = PORT_NewArena(BLTEST_DEFAULT_CHUNKSIZE);
    key = (DSAPrivateKey *)PORT_ArenaZAlloc(arena, sizeof(DSAPrivateKey));
    key->params.arena = arena;
    key_from_filedata(arena, &key->params.prime, 0, 5, filedata);
    return key;
}

#ifdef NSS_ENABLE_ECC
static ECPrivateKey *
eckey_from_filedata(SECItem *filedata)
{
    ECPrivateKey *key;
    PLArenaPool *arena;
    SECStatus rv;
    ECParams *tmpECParams = NULL;
    arena = PORT_NewArena(BLTEST_DEFAULT_CHUNKSIZE);
    key = (ECPrivateKey *)PORT_ArenaZAlloc(arena, sizeof(ECPrivateKey));
    /* read and convert params */
    key->ecParams.arena = arena;
    key_from_filedata(arena, &key->ecParams.DEREncoding, 0, 1, filedata);
    rv = SECOID_Init();
    CHECKERROR(rv, __LINE__);
    rv = EC_DecodeParams(&key->ecParams.DEREncoding, &tmpECParams);
    CHECKERROR(rv, __LINE__);
    rv = EC_CopyParams(key->ecParams.arena, &key->ecParams, tmpECParams);
    CHECKERROR(rv, __LINE__);
    rv = SECOID_Shutdown();
    CHECKERROR(rv, __LINE__);
    PORT_FreeArena(tmpECParams->arena, PR_TRUE);
    /* read key */
    key_from_filedata(arena, &key->publicValue, 1, 3, filedata);
    return key;
}

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

static SECItem * 
getECParams(const char *curve)
{
    SECItem *ecparams;
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

static void
dump_pqg(PQGParams *pqg)
{
    SECU_PrintInteger(stdout, &pqg->prime, "PRIME:", 0);
    SECU_PrintInteger(stdout, &pqg->subPrime, "SUBPRIME:", 0);
    SECU_PrintInteger(stdout, &pqg->base, "BASE:", 0);
}

static void
dump_dsakey(DSAPrivateKey *key)
{
    dump_pqg(&key->params);
    SECU_PrintInteger(stdout, &key->publicValue, "PUBLIC VALUE:", 0);
    SECU_PrintInteger(stdout, &key->privateValue, "PRIVATE VALUE:", 0);
}

#ifdef NSS_ENABLE_ECC
static void
dump_ecp(ECParams *ecp)
{
    /* TODO other fields */
    SECU_PrintInteger(stdout, &ecp->base, "BASE POINT:", 0);
}

static void
dump_eckey(ECPrivateKey *key)
{
    dump_ecp(&key->ecParams);
    SECU_PrintInteger(stdout, &key->publicValue, "PUBLIC VALUE:", 0);
    SECU_PrintInteger(stdout, &key->privateValue, "PRIVATE VALUE:", 0);
}
#endif

static void
dump_rsakey(RSAPrivateKey *key)
{
    SECU_PrintInteger(stdout, &key->version, "VERSION:", 0);
    SECU_PrintInteger(stdout, &key->modulus, "MODULUS:", 0);
    SECU_PrintInteger(stdout, &key->publicExponent, "PUBLIC EXP:", 0);
    SECU_PrintInteger(stdout, &key->privateExponent, "PRIVATE EXP:", 0);
    SECU_PrintInteger(stdout, &key->prime1, "CRT PRIME 1:", 0);
    SECU_PrintInteger(stdout, &key->prime2, "CRT PRIME 2:", 0);
    SECU_PrintInteger(stdout, &key->exponent1, "CRT EXP 1:", 0);
    SECU_PrintInteger(stdout, &key->exponent2, "CRT EXP 2:", 0);
    SECU_PrintInteger(stdout, &key->coefficient, "CRT COEFFICIENT:", 0);
}

typedef enum {
    bltestBase64Encoded,       /* Base64 encoded ASCII */
    bltestBinary,	       /* straight binary */
    bltestHexSpaceDelim,       /* 0x12 0x34 0xab 0xCD ... */
    bltestHexStream 	       /* 1234abCD ... */
} bltestIOMode;

typedef struct
{
    SECItem	   buf;
    SECItem	   pBuf;
    bltestIOMode   mode;
    PRFileDesc*	   file;
} bltestIO;

typedef SECStatus (* bltestSymmCipherFn)(void *cx,
					 unsigned char *output,
					 unsigned int *outputLen,
					 unsigned int maxOutputLen,
					 const unsigned char *input,
					 unsigned int inputLen);

typedef SECStatus (* bltestPubKeyCipherFn)(void *key,
					   SECItem *output,
					   const SECItem *input);

typedef SECStatus (* bltestHashCipherFn)(unsigned char *dest,
					 const unsigned char *src,
					 PRUint32 src_length);

typedef enum {
    bltestINVALID = -1,
    bltestDES_ECB,	  /* Symmetric Key Ciphers */
    bltestDES_CBC,	  /* .			   */
    bltestDES_EDE_ECB,	  /* .			   */
    bltestDES_EDE_CBC,	  /* .			   */
    bltestRC2_ECB,	  /* .			   */
    bltestRC2_CBC,	  /* .			   */
    bltestRC4,		  /* .			   */
#ifdef NSS_SOFTOKEN_DOES_RC5
    bltestRC5_ECB,	  /* .			   */
    bltestRC5_CBC,	  /* .			   */
#endif
    bltestAES_ECB,        /* .                     */
    bltestAES_CBC,        /* .                     */
    bltestAES_CTS,        /* .                     */
    bltestAES_CTR,        /* .                     */
    bltestAES_GCM,        /* .                     */
    bltestCAMELLIA_ECB,   /* .                     */
    bltestCAMELLIA_CBC,   /* .                     */
    bltestSEED_ECB,       /* SEED algorithm	   */
    bltestSEED_CBC,       /* SEED algorithm	   */
    bltestRSA,		  /* Public Key Ciphers	   */
#ifdef NSS_ENABLE_ECC
    bltestECDSA,	  /* . (Public Key Sig.)   */
#endif
    bltestDSA,		  /* .                     */
    bltestMD2,		  /* Hash algorithms	   */
    bltestMD5,		  /* .			   */
    bltestSHA1,           /* .			   */
    bltestSHA224,         /* .			   */
    bltestSHA256,         /* .			   */
    bltestSHA384,         /* .			   */
    bltestSHA512,         /* .			   */
    NUMMODES
} bltestCipherMode;

static char *mode_strings[] =
{
    "des_ecb",
    "des_cbc",
    "des3_ecb",
    "des3_cbc",
    "rc2_ecb",
    "rc2_cbc",
    "rc4",
#ifdef NSS_SOFTOKEN_DOES_RC5
    "rc5_ecb",
    "rc5_cbc",
#endif
    "aes_ecb",
    "aes_cbc",
    "aes_cts",
    "aes_ctr",
    "aes_gcm",
    "camellia_ecb",
    "camellia_cbc",
    "seed_ecb",
    "seed_cbc",
    "rsa",
#ifdef NSS_ENABLE_ECC
    "ecdsa",
#endif
    /*"pqg",*/
    "dsa",
    "md2",
    "md5",
    "sha1",
    "sha224",
    "sha256",
    "sha384",
    "sha512",
};

typedef struct
{
    bltestIO key;
    bltestIO iv;
} bltestSymmKeyParams;

typedef struct
{
    bltestSymmKeyParams sk; /* must be first */
    bltestIO aad;
} bltestAuthSymmKeyParams;

typedef struct
{
    bltestIO key;
    bltestIO iv;
    int	     rounds;
    int	     wordsize;
} bltestRC5Params;

typedef struct
{
    bltestIO key;
    int	     keysizeInBits;
    RSAPrivateKey *rsakey;
} bltestRSAParams;

typedef struct
{
    bltestIO   key;
    bltestIO   pqgdata;
    unsigned int keysize;
    bltestIO   keyseed;
    bltestIO   sigseed;
    bltestIO   sig; /* if doing verify, have additional input */
    PQGParams *pqg;
    DSAPrivateKey *dsakey;
} bltestDSAParams;

#ifdef NSS_ENABLE_ECC
typedef struct
{
    bltestIO   key;
    char      *curveName;
    bltestIO   sigseed;
    bltestIO   sig; /* if doing verify, have additional input */
    ECPrivateKey *eckey;
} bltestECDSAParams;
#endif

typedef struct
{
    bltestIO   key; /* unused */
    PRBool     restart;
} bltestHashParams;

typedef union
{
    bltestIO		key;
    bltestSymmKeyParams sk;
    bltestAuthSymmKeyParams ask;
    bltestRC5Params	rc5;
    bltestRSAParams	rsa;
    bltestDSAParams	dsa;
#ifdef NSS_ENABLE_ECC
    bltestECDSAParams	ecdsa;
#endif
    bltestHashParams	hash;
} bltestParams;

typedef struct bltestCipherInfoStr bltestCipherInfo;

struct  bltestCipherInfoStr {
    PLArenaPool *arena;
    /* link to next in multithreaded test */
    bltestCipherInfo *next;
    PRThread         *cipherThread;

    /* MonteCarlo test flag*/
    PRBool mCarlo;
    /* cipher context */
    void *cx;
    /* I/O streams */
    bltestIO input;
    bltestIO output;
    /* Cipher-specific parameters */
    bltestParams params;
    /* Cipher mode */
    bltestCipherMode  mode;
    /* Cipher function (encrypt/decrypt/sign/verify/hash) */
    union {
	bltestSymmCipherFn   symmkeyCipher;
	bltestPubKeyCipherFn pubkeyCipher;
	bltestHashCipherFn   hashCipher;
    } cipher;
    /* performance testing */
    int   repetitionsToPerfom;
    int   seconds;
    int	  repetitions;
    int   cxreps;
    double cxtime;
    double optime;
};

PRBool
is_symmkeyCipher(bltestCipherMode mode)
{
    /* change as needed! */
    if (mode >= bltestDES_ECB && mode <= bltestSEED_CBC)
	return PR_TRUE;
    return PR_FALSE;
}

PRBool
is_authCipher(bltestCipherMode mode)
{
    /* change as needed! */
    if (mode == bltestAES_GCM)
	return PR_TRUE;
    return PR_FALSE;
}


PRBool
is_singleShotCipher(bltestCipherMode mode)
{
    /* change as needed! */
    if (mode == bltestAES_GCM)
	return PR_TRUE;
    if (mode == bltestAES_CTS)
	return PR_TRUE;
    return PR_FALSE;
}

PRBool
is_pubkeyCipher(bltestCipherMode mode)
{
    /* change as needed! */
    if (mode >= bltestRSA && mode <= bltestDSA)
	return PR_TRUE;
    return PR_FALSE;
}

PRBool
is_hashCipher(bltestCipherMode mode)
{
    /* change as needed! */
    if (mode >= bltestMD2 && mode <= bltestSHA512)
	return PR_TRUE;
    return PR_FALSE;
}

PRBool
is_sigCipher(bltestCipherMode mode)
{
    /* change as needed! */
#ifdef NSS_ENABLE_ECC
    if (mode >= bltestECDSA && mode <= bltestDSA)
#else
    if (mode >= bltestDSA && mode <= bltestDSA)
#endif
	return PR_TRUE;
    return PR_FALSE;
}

PRBool
cipher_requires_IV(bltestCipherMode mode)
{
    /* change as needed! */
    if (mode == bltestDES_CBC || mode == bltestDES_EDE_CBC ||
	mode == bltestRC2_CBC || 
#ifdef NSS_SOFTOKEN_DOES_RC5
	mode == bltestRC5_CBC ||
#endif
	mode == bltestAES_CBC || mode == bltestAES_CTS || 
	mode == bltestAES_CTR || mode == bltestAES_GCM ||
	mode == bltestCAMELLIA_CBC || mode == bltestSEED_CBC)
	return PR_TRUE;
    return PR_FALSE;
}

SECStatus finishIO(bltestIO *output, PRFileDesc *file);

SECStatus
setupIO(PLArenaPool *arena, bltestIO *input, PRFileDesc *file,
	char *str, int numBytes)
{
    SECStatus rv = SECSuccess;
    SECItem fileData;
    SECItem *in;
    unsigned char *tok;
    unsigned int i, j;

    if (file && (numBytes == 0 || file == PR_STDIN)) {
	/* grabbing data from a file */
	rv = SECU_FileToItem(&fileData, file);
	if (rv != SECSuccess) {
	    PR_Close(file);
	    return SECFailure;
	}
	in = &fileData;
    } else if (str) {
	/* grabbing data from command line */
	fileData.data = (unsigned char *)str;
	fileData.len = PL_strlen(str);
	in = &fileData;
    } else if (file) {
	/* create nonce */
	SECITEM_AllocItem(arena, &input->buf, numBytes);
	RNG_GenerateGlobalRandomBytes(input->buf.data, numBytes);
	return finishIO(input, file);
    } else {
	return SECFailure;
    }

    switch (input->mode) {
    case bltestBase64Encoded:
	if (in->len == 0) {
	    input->buf.data = NULL;
	    input->buf.len = 0;
	    break;
	}
	rv = atob(in, &input->buf, arena);
	break;
    case bltestBinary:
	if (in->len == 0) {
	    input->buf.data = NULL;
	    input->buf.len = 0;
	    break;
	}
	if (in->data[in->len-1] == '\n') --in->len;
	if (in->data[in->len-1] == '\r') --in->len;
	SECITEM_CopyItem(arena, &input->buf, in);
	break;
    case bltestHexSpaceDelim:
	SECITEM_AllocItem(arena, &input->buf, in->len/5);
	for (i=0, j=0; i<in->len; i+=5, j++) {
	    tok = &in->data[i];
	    if (tok[0] != '0' || tok[1] != 'x' || tok[4] != ' ')
		/* bad hex token */
		break;

	    rv = hex_from_2char(&tok[2], input->buf.data + j);
	    if (rv)
		break;
	}
	break;
    case bltestHexStream:
	SECITEM_AllocItem(arena, &input->buf, in->len/2);
	for (i=0, j=0; i<in->len; i+=2, j++) {
	    tok = &in->data[i];
	    rv = hex_from_2char(tok, input->buf.data + j);
	    if (rv)
		break;
	}
	break;
    }

    if (file)
	SECITEM_FreeItem(&fileData, PR_FALSE);
    return rv;
}

SECStatus
finishIO(bltestIO *output, PRFileDesc *file)
{
    SECStatus rv = SECSuccess;
    PRInt32 nb;
    unsigned char byteval;
    SECItem *it;
    char hexstr[5];
    unsigned int i;
    if (output->pBuf.len > 0) {
	it = &output->pBuf;
    } else {
	it = &output->buf;
    }
    switch (output->mode) {
    case bltestBase64Encoded:
	rv = btoa_file(it, file);
	break;
    case bltestBinary:
	nb = PR_Write(file, it->data, it->len);
	rv = (nb == (PRInt32)it->len) ? SECSuccess : SECFailure;
	break;
    case bltestHexSpaceDelim:
	hexstr[0] = '0';
	hexstr[1] = 'x';
	hexstr[4] = ' ';
	for (i=0; i<it->len; i++) {
	    byteval = it->data[i];
	    rv = char2_from_hex(byteval, hexstr + 2);
	    nb = PR_Write(file, hexstr, 5);
	    if (rv)
		break;
	}
	PR_Write(file, "\n", 1);
	break;
    case bltestHexStream:
	for (i=0; i<it->len; i++) {
	    byteval = it->data[i];
	    rv = char2_from_hex(byteval, hexstr);
	    if (rv)
		break;
	    nb = PR_Write(file, hexstr, 2);
	}
	PR_Write(file, "\n", 1);
	break;
    }
    return rv;
}

void
bltestCopyIO(PLArenaPool *arena, bltestIO *dest, bltestIO *src)
{
    SECITEM_CopyItem(arena, &dest->buf, &src->buf);
    if (src->pBuf.len > 0) {
	dest->pBuf.len = src->pBuf.len;
	dest->pBuf.data = dest->buf.data + (src->pBuf.data - src->buf.data);
    }
    dest->mode = src->mode;
    dest->file = src->file;
}

void
misalignBuffer(PLArenaPool *arena, bltestIO *io, int off)
{
    ptrdiff_t offset = (ptrdiff_t)io->buf.data % WORDSIZE;
    int length = io->buf.len;
    if (offset != off) {
	SECITEM_ReallocItemV2(arena, &io->buf, length + 2*WORDSIZE);
	/* offset may have changed? */
	offset = (ptrdiff_t)io->buf.data % WORDSIZE;
	if (offset != off) {
	    memmove(io->buf.data + off, io->buf.data, length);
	    io->pBuf.data = io->buf.data + off;
	    io->pBuf.len = length;
	} else {
	    io->pBuf.data = io->buf.data;
	    io->pBuf.len = length;
	}
    } else {
	io->pBuf.data = io->buf.data;
	io->pBuf.len = length;
    }
}

SECStatus
des_Encrypt(void *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    return DES_Encrypt((DESContext *)cx, output, outputLen, maxOutputLen,
                       input, inputLen);
}

SECStatus
des_Decrypt(void *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    return DES_Decrypt((DESContext *)cx, output, outputLen, maxOutputLen,
                       input, inputLen);
}

SECStatus
rc2_Encrypt(void *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    return RC2_Encrypt((RC2Context *)cx, output, outputLen, maxOutputLen,
                       input, inputLen);
}

SECStatus
rc2_Decrypt(void *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    return RC2_Decrypt((RC2Context *)cx, output, outputLen, maxOutputLen,
                       input, inputLen);
}

SECStatus
rc4_Encrypt(void *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    return RC4_Encrypt((RC4Context *)cx, output, outputLen, maxOutputLen,
                       input, inputLen);
}

SECStatus
rc4_Decrypt(void *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    return RC4_Decrypt((RC4Context *)cx, output, outputLen, maxOutputLen,
                       input, inputLen);
}

SECStatus
aes_Encrypt(void *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    return AES_Encrypt((AESContext *)cx, output, outputLen, maxOutputLen,
                       input, inputLen);
}

SECStatus
aes_Decrypt(void *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    return AES_Decrypt((AESContext *)cx, output, outputLen, maxOutputLen,
                       input, inputLen);
}

SECStatus
camellia_Encrypt(void *cx, unsigned char *output, unsigned int *outputLen,
		 unsigned int maxOutputLen, const unsigned char *input,
		 unsigned int inputLen)
{
    return Camellia_Encrypt((CamelliaContext *)cx, output, outputLen,
			    maxOutputLen,
			    input, inputLen);
}

SECStatus
camellia_Decrypt(void *cx, unsigned char *output, unsigned int *outputLen,
		 unsigned int maxOutputLen, const unsigned char *input,
		 unsigned int inputLen)
{
    return Camellia_Decrypt((CamelliaContext *)cx, output, outputLen,
			    maxOutputLen,
			    input, inputLen);
}

SECStatus
seed_Encrypt(void *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    return SEED_Encrypt((SEEDContext *)cx, output, outputLen, maxOutputLen,
                       input, inputLen);
}

SECStatus
seed_Decrypt(void *cx, unsigned char *output, unsigned int *outputLen,
            unsigned int maxOutputLen, const unsigned char *input,
            unsigned int inputLen)
{
    return SEED_Decrypt((SEEDContext *)cx, output, outputLen, maxOutputLen,
                       input, inputLen);
}

SECStatus
rsa_PublicKeyOp(void *key, SECItem *output, const SECItem *input)
{
    return RSA_PublicKeyOp((RSAPublicKey *)key, output->data, input->data);
}

SECStatus
rsa_PrivateKeyOp(void *key, SECItem *output, const SECItem *input)
{
    return RSA_PrivateKeyOp((RSAPrivateKey *)key, output->data, input->data);
}

SECStatus
dsa_signDigest(void *key, SECItem *output, const SECItem *input)
{
    return DSA_SignDigest((DSAPrivateKey *)key, output, input);
}

SECStatus
dsa_verifyDigest(void *key, SECItem *output, const SECItem *input)
{
    return DSA_VerifyDigest((DSAPublicKey *)key, output, input);
}

#ifdef NSS_ENABLE_ECC
SECStatus
ecdsa_signDigest(void *key, SECItem *output, const SECItem *input)
{
    return ECDSA_SignDigest((ECPrivateKey *)key, output, input);
}

SECStatus
ecdsa_verifyDigest(void *key, SECItem *output, const SECItem *input)
{
    return ECDSA_VerifyDigest((ECPublicKey *)key, output, input);
}
#endif

SECStatus
bltest_des_init(bltestCipherInfo *cipherInfo, PRBool encrypt)
{
    PRIntervalTime time1, time2;
    bltestSymmKeyParams *desp = &cipherInfo->params.sk;
    int minorMode;
    int i;
    switch (cipherInfo->mode) {
    case bltestDES_ECB:	    minorMode = NSS_DES;	  break;
    case bltestDES_CBC:	    minorMode = NSS_DES_CBC;	  break;
    case bltestDES_EDE_ECB: minorMode = NSS_DES_EDE3;	  break;
    case bltestDES_EDE_CBC: minorMode = NSS_DES_EDE3_CBC; break;
    default:
	return SECFailure;
    }
    cipherInfo->cx = (void*)DES_CreateContext(desp->key.buf.data,
					      desp->iv.buf.data,
					      minorMode, encrypt);
    if (cipherInfo->cxreps > 0) {
	DESContext **dummycx;
	dummycx = PORT_Alloc(cipherInfo->cxreps * sizeof(DESContext *));
	TIMESTART();
	for (i=0; i<cipherInfo->cxreps; i++) {
	    dummycx[i] = (void*)DES_CreateContext(desp->key.buf.data,
					          desp->iv.buf.data,
					          minorMode, encrypt);
	}
	TIMEFINISH(cipherInfo->cxtime, 1.0);
	for (i=0; i<cipherInfo->cxreps; i++) {
	    DES_DestroyContext(dummycx[i], PR_TRUE);
	}
	PORT_Free(dummycx);
    }
    if (encrypt)
	cipherInfo->cipher.symmkeyCipher = des_Encrypt;
    else
	cipherInfo->cipher.symmkeyCipher = des_Decrypt;
    return SECSuccess;
}

SECStatus
bltest_rc2_init(bltestCipherInfo *cipherInfo, PRBool encrypt)
{
    PRIntervalTime time1, time2;
    bltestSymmKeyParams *rc2p = &cipherInfo->params.sk;
    int minorMode;
    int i;
    switch (cipherInfo->mode) {
    case bltestRC2_ECB: minorMode = NSS_RC2;	 break;
    case bltestRC2_CBC: minorMode = NSS_RC2_CBC; break;
    default:
	return SECFailure;
    }
    cipherInfo->cx = (void*)RC2_CreateContext(rc2p->key.buf.data,
					      rc2p->key.buf.len,
					      rc2p->iv.buf.data,
					      minorMode,
					      rc2p->key.buf.len);
    if (cipherInfo->cxreps > 0) {
	RC2Context **dummycx;
	dummycx = PORT_Alloc(cipherInfo->cxreps * sizeof(RC2Context *));
	TIMESTART();
	for (i=0; i<cipherInfo->cxreps; i++) {
	    dummycx[i] = (void*)RC2_CreateContext(rc2p->key.buf.data,
	                                          rc2p->key.buf.len,
	                                          rc2p->iv.buf.data,
	                                          minorMode,
	                                          rc2p->key.buf.len);
	}
	TIMEFINISH(cipherInfo->cxtime, 1.0);
	for (i=0; i<cipherInfo->cxreps; i++) {
	    RC2_DestroyContext(dummycx[i], PR_TRUE);
	}
	PORT_Free(dummycx);
    }
    if (encrypt)
	cipherInfo->cipher.symmkeyCipher = rc2_Encrypt;
    else
	cipherInfo->cipher.symmkeyCipher = rc2_Decrypt;
    return SECSuccess;
}

SECStatus
bltest_rc4_init(bltestCipherInfo *cipherInfo, PRBool encrypt)
{
    PRIntervalTime time1, time2;
    int i;
    bltestSymmKeyParams *rc4p = &cipherInfo->params.sk;
    cipherInfo->cx = (void*)RC4_CreateContext(rc4p->key.buf.data,
					      rc4p->key.buf.len);
    if (cipherInfo->cxreps > 0) {
	RC4Context **dummycx;
	dummycx = PORT_Alloc(cipherInfo->cxreps * sizeof(RC4Context *));
	TIMESTART();
	for (i=0; i<cipherInfo->cxreps; i++) {
	    dummycx[i] = (void*)RC4_CreateContext(rc4p->key.buf.data,
	                                          rc4p->key.buf.len);
	}
	TIMEFINISH(cipherInfo->cxtime, 1.0);
	for (i=0; i<cipherInfo->cxreps; i++) {
	    RC4_DestroyContext(dummycx[i], PR_TRUE);
	}
	PORT_Free(dummycx);
    }
    if (encrypt)
	cipherInfo->cipher.symmkeyCipher = rc4_Encrypt;
    else
	cipherInfo->cipher.symmkeyCipher = rc4_Decrypt;
    return SECSuccess;
}

SECStatus
bltest_rc5_init(bltestCipherInfo *cipherInfo, PRBool encrypt)
{
#ifdef NSS_SOFTOKEN_DOES_RC5
    PRIntervalTime time1, time2;
    bltestRC5Params *rc5p = &cipherInfo->params.rc5;
    int minorMode;
    switch (cipherInfo->mode) {
    case bltestRC5_ECB: minorMode = NSS_RC5;	 break;
    case bltestRC5_CBC: minorMode = NSS_RC5_CBC; break;
    default:
	return SECFailure;
    }
    TIMESTART();
    cipherInfo->cx = (void*)RC5_CreateContext(&rc5p->key.buf,
					      rc5p->rounds, rc5p->wordsize,
					      rc5p->iv.buf.data, minorMode);
    TIMEFINISH(cipherInfo->cxtime, 1.0);
    if (encrypt)
	cipherInfo->cipher.symmkeyCipher = RC5_Encrypt;
    else
	cipherInfo->cipher.symmkeyCipher = RC5_Decrypt;
    return SECSuccess;
#else
    return SECFailure;
#endif
}

SECStatus
bltest_aes_init(bltestCipherInfo *cipherInfo, PRBool encrypt)
{
    bltestSymmKeyParams *aesp = &cipherInfo->params.sk;
    bltestAuthSymmKeyParams *gcmp = &cipherInfo->params.ask;
    int minorMode;
    int i;
    int keylen   = aesp->key.buf.len;
    int blocklen = AES_BLOCK_SIZE; 
    PRIntervalTime time1, time2;
    unsigned char *params;
    int len;
    CK_AES_CTR_PARAMS ctrParams;
    CK_GCM_PARAMS gcmParams;

    params = aesp->iv.buf.data;
    switch (cipherInfo->mode) {
    case bltestAES_ECB:	    minorMode = NSS_AES;	  break;
    case bltestAES_CBC:	    minorMode = NSS_AES_CBC;	  break;
    case bltestAES_CTS:	    minorMode = NSS_AES_CTS;	  break;
    case bltestAES_CTR:	    
	minorMode = NSS_AES_CTR;
	ctrParams.ulCounterBits = 32;
	len = PR_MIN(aesp->iv.buf.len, blocklen);
	PORT_Memset(ctrParams.cb, 0, blocklen);
	PORT_Memcpy(ctrParams.cb, aesp->iv.buf.data, len);
	params = (unsigned char *)&ctrParams;
	break;
    case bltestAES_GCM:
	minorMode = NSS_AES_GCM;
	gcmParams.pIv = gcmp->sk.iv.buf.data;
	gcmParams.ulIvLen = gcmp->sk.iv.buf.len;
	gcmParams.pAAD = gcmp->aad.buf.data;
	gcmParams.ulAADLen = gcmp->aad.buf.len;
	gcmParams.ulTagBits = blocklen*8;
	params = (unsigned char *)&gcmParams;
	break;
    default:
	return SECFailure;
    }
    cipherInfo->cx = (void*)AES_CreateContext(aesp->key.buf.data,
					      params,
					      minorMode, encrypt, 
                                              keylen, blocklen);
    if (cipherInfo->cxreps > 0) {
	AESContext **dummycx;
	dummycx = PORT_Alloc(cipherInfo->cxreps * sizeof(AESContext *));
	TIMESTART();
	for (i=0; i<cipherInfo->cxreps; i++) {
	    dummycx[i] = (void*)AES_CreateContext(aesp->key.buf.data,
					          params,
					          minorMode, encrypt,
	                                          keylen, blocklen);
	}
	TIMEFINISH(cipherInfo->cxtime, 1.0);
	for (i=0; i<cipherInfo->cxreps; i++) {
	    AES_DestroyContext(dummycx[i], PR_TRUE);
	}
	PORT_Free(dummycx);
    }
    if (encrypt)
	cipherInfo->cipher.symmkeyCipher = aes_Encrypt;
    else
	cipherInfo->cipher.symmkeyCipher = aes_Decrypt;
    return SECSuccess;
}

SECStatus
bltest_camellia_init(bltestCipherInfo *cipherInfo, PRBool encrypt)
{
    bltestSymmKeyParams *camelliap = &cipherInfo->params.sk;
    int minorMode;
    int i;
    int keylen   = camelliap->key.buf.len;
    PRIntervalTime time1, time2;
    
    switch (cipherInfo->mode) {
    case bltestCAMELLIA_ECB:	    minorMode = NSS_CAMELLIA;	  break;
    case bltestCAMELLIA_CBC:	    minorMode = NSS_CAMELLIA_CBC;  break;
    default:
	return SECFailure;
    }
    cipherInfo->cx = (void*)Camellia_CreateContext(camelliap->key.buf.data,
						   camelliap->iv.buf.data,
						   minorMode, encrypt, 
						   keylen);
    if (cipherInfo->cxreps > 0) {
	CamelliaContext **dummycx;
	dummycx = PORT_Alloc(cipherInfo->cxreps * sizeof(CamelliaContext *));
	TIMESTART();
	for (i=0; i<cipherInfo->cxreps; i++) {
	    dummycx[i] = (void*)Camellia_CreateContext(camelliap->key.buf.data,
						       camelliap->iv.buf.data,
						       minorMode, encrypt,
						       keylen);
	}
	TIMEFINISH(cipherInfo->cxtime, 1.0);
	for (i=0; i<cipherInfo->cxreps; i++) {
	    Camellia_DestroyContext(dummycx[i], PR_TRUE);
	}
	PORT_Free(dummycx);
    }
    if (encrypt)
	cipherInfo->cipher.symmkeyCipher = camellia_Encrypt;
    else
	cipherInfo->cipher.symmkeyCipher = camellia_Decrypt;
    return SECSuccess;
}

SECStatus
bltest_seed_init(bltestCipherInfo *cipherInfo, PRBool encrypt)
{
    PRIntervalTime time1, time2;
    bltestSymmKeyParams *seedp = &cipherInfo->params.sk;
    int minorMode;
    int i;

    switch (cipherInfo->mode) {
    case bltestSEED_ECB:	minorMode = NSS_SEED;		break;
    case bltestSEED_CBC:	minorMode = NSS_SEED_CBC;	break;
    default:
	return SECFailure;
    }
    cipherInfo->cx = (void*)SEED_CreateContext(seedp->key.buf.data,
					      seedp->iv.buf.data,
					      minorMode, encrypt);
    if (cipherInfo->cxreps > 0) {
	SEEDContext **dummycx;
	dummycx = PORT_Alloc(cipherInfo->cxreps * sizeof(SEEDContext *));
	TIMESTART();
	for (i=0; i<cipherInfo->cxreps; i++) {
	    dummycx[i] = (void*)SEED_CreateContext(seedp->key.buf.data,
					          seedp->iv.buf.data,
					          minorMode, encrypt);
	}
	TIMEFINISH(cipherInfo->cxtime, 1.0);
	for (i=0; i<cipherInfo->cxreps; i++) {
	    SEED_DestroyContext(dummycx[i], PR_TRUE);
	}
	PORT_Free(dummycx);
    }
    if (encrypt)
	cipherInfo->cipher.symmkeyCipher = seed_Encrypt;
    else
	cipherInfo->cipher.symmkeyCipher = seed_Decrypt;
	
	return SECSuccess;
}

SECStatus
bltest_rsa_init(bltestCipherInfo *cipherInfo, PRBool encrypt)
{
    int i;
    RSAPrivateKey **dummyKey;
    PRIntervalTime time1, time2;
    bltestRSAParams *rsap = &cipherInfo->params.rsa;
    /* RSA key gen was done during parameter setup */
    cipherInfo->cx = cipherInfo->params.rsa.rsakey;
    /* For performance testing */
    if (cipherInfo->cxreps > 0) {
	/* Create space for n private key objects */
	dummyKey = (RSAPrivateKey **)PORT_Alloc(cipherInfo->cxreps *
						sizeof(RSAPrivateKey *));
	/* Time n keygens, storing in the array */
	TIMESTART();
	for (i=0; i<cipherInfo->cxreps; i++)
	    dummyKey[i] = RSA_NewKey(rsap->keysizeInBits, 
	                             &rsap->rsakey->publicExponent);
	TIMEFINISH(cipherInfo->cxtime, cipherInfo->cxreps);
	/* Free the n key objects */
	for (i=0; i<cipherInfo->cxreps; i++)
	    PORT_FreeArena(dummyKey[i]->arena, PR_TRUE);
	PORT_Free(dummyKey);
    }
    if (encrypt) {
	/* Have to convert private key to public key.  Memory
	 * is freed with private key's arena  */
	RSAPublicKey *pubkey;
	RSAPrivateKey *key = (RSAPrivateKey *)cipherInfo->cx;
	pubkey = (RSAPublicKey *)PORT_ArenaAlloc(key->arena,
						 sizeof(RSAPublicKey));
	pubkey->modulus.len = key->modulus.len;
	pubkey->modulus.data = key->modulus.data;
	pubkey->publicExponent.len = key->publicExponent.len;
	pubkey->publicExponent.data = key->publicExponent.data;
	cipherInfo->cx = (void *)pubkey;
	cipherInfo->cipher.pubkeyCipher = rsa_PublicKeyOp;
    } else {
	cipherInfo->cipher.pubkeyCipher = rsa_PrivateKeyOp;
    }
    return SECSuccess;
}

SECStatus
blapi_pqg_param_gen(unsigned int keysize, PQGParams **pqg, PQGVerify **vfy)
{
    if (keysize < 1024) {
	int j = PQG_PBITS_TO_INDEX(keysize);
	return PQG_ParamGen(j, pqg, vfy);
    }
    return PQG_ParamGenV2(keysize, 0, 0, pqg, vfy);
}

SECStatus
bltest_pqg_init(bltestDSAParams *dsap)
{
    SECStatus rv, res;
    PQGVerify *vfy = NULL;
    rv = blapi_pqg_param_gen(dsap->keysize, &dsap->pqg, &vfy);
    CHECKERROR(rv, __LINE__);
    rv = PQG_VerifyParams(dsap->pqg, vfy, &res);
    CHECKERROR(res, __LINE__);
    CHECKERROR(rv, __LINE__);
    return rv;
}

SECStatus
bltest_dsa_init(bltestCipherInfo *cipherInfo, PRBool encrypt)
{
    int i;
    DSAPrivateKey **dummyKey;
    PQGParams *dummypqg;
    PRIntervalTime time1, time2;
    bltestDSAParams *dsap = &cipherInfo->params.dsa;
    PQGVerify *ignore = NULL;
    /* DSA key gen was done during parameter setup */
    cipherInfo->cx = cipherInfo->params.dsa.dsakey;
    /* For performance testing */
    if (cipherInfo->cxreps > 0) {
	/* Create space for n private key objects */
	dummyKey = (DSAPrivateKey **)PORT_ZAlloc(cipherInfo->cxreps *
	                                         sizeof(DSAPrivateKey *));
	/* Time n keygens, storing in the array */
	TIMESTART();
	for (i=0; i<cipherInfo->cxreps; i++) {
	    dummypqg = NULL;
	    blapi_pqg_param_gen(dsap->keysize, &dummypqg, &ignore);
	    DSA_NewKey(dummypqg, &dummyKey[i]);
	}
	TIMEFINISH(cipherInfo->cxtime, cipherInfo->cxreps);
	/* Free the n key objects */
	for (i=0; i<cipherInfo->cxreps; i++)
	    PORT_FreeArena(dummyKey[i]->params.arena, PR_TRUE);
	PORT_Free(dummyKey);
    }
    if (!dsap->pqg && dsap->pqgdata.buf.len > 0) {
	dsap->pqg = pqg_from_filedata(&dsap->pqgdata.buf);
    }
    if (!cipherInfo->cx && dsap->key.buf.len > 0) {
	cipherInfo->cx = dsakey_from_filedata(&dsap->key.buf);
    }
    if (encrypt) {
	cipherInfo->cipher.pubkeyCipher = dsa_signDigest;
    } else {
	/* Have to convert private key to public key.  Memory
	 * is freed with private key's arena  */
	DSAPublicKey *pubkey;
	DSAPrivateKey *key = (DSAPrivateKey *)cipherInfo->cx;
	pubkey = (DSAPublicKey *)PORT_ArenaZAlloc(key->params.arena,
						  sizeof(DSAPublicKey));
	pubkey->params.prime.len = key->params.prime.len;
	pubkey->params.prime.data = key->params.prime.data;
	pubkey->params.subPrime.len = key->params.subPrime.len;
	pubkey->params.subPrime.data = key->params.subPrime.data;
	pubkey->params.base.len = key->params.base.len;
	pubkey->params.base.data = key->params.base.data;
	pubkey->publicValue.len = key->publicValue.len;
	pubkey->publicValue.data = key->publicValue.data;
	cipherInfo->cipher.pubkeyCipher = dsa_verifyDigest;
    }
    return SECSuccess;
}

#ifdef NSS_ENABLE_ECC
SECStatus
bltest_ecdsa_init(bltestCipherInfo *cipherInfo, PRBool encrypt)
{
    int i;
    ECPrivateKey **dummyKey;
    PRIntervalTime time1, time2;
    bltestECDSAParams *ecdsap = &cipherInfo->params.ecdsa;
    /* ECDSA key gen was done during parameter setup */
    cipherInfo->cx = cipherInfo->params.ecdsa.eckey;
    /* For performance testing */
    if (cipherInfo->cxreps > 0) {
	/* Create space for n private key objects */
	dummyKey = (ECPrivateKey **)PORT_ZAlloc(cipherInfo->cxreps *
	                                         sizeof(ECPrivateKey *));
	/* Time n keygens, storing in the array */
	TIMESTART();
	for (i=0; i<cipherInfo->cxreps; i++) {
	    EC_NewKey(&ecdsap->eckey->ecParams, &dummyKey[i]);
	}
	TIMEFINISH(cipherInfo->cxtime, cipherInfo->cxreps);
	/* Free the n key objects */
	for (i=0; i<cipherInfo->cxreps; i++)
	    PORT_FreeArena(dummyKey[i]->ecParams.arena, PR_TRUE);
	PORT_Free(dummyKey);
    }
    if (!cipherInfo->cx && ecdsap->key.buf.len > 0) {
	cipherInfo->cx = eckey_from_filedata(&ecdsap->key.buf);
    }
    if (encrypt) {
	cipherInfo->cipher.pubkeyCipher = ecdsa_signDigest;
    } else {
	/* Have to convert private key to public key.  Memory
	 * is freed with private key's arena  */
	ECPublicKey *pubkey;
	ECPrivateKey *key = (ECPrivateKey *)cipherInfo->cx;
	pubkey = (ECPublicKey *)PORT_ArenaZAlloc(key->ecParams.arena,
						  sizeof(ECPublicKey));
	pubkey->ecParams.type = key->ecParams.type;
	pubkey->ecParams.fieldID.size = key->ecParams.fieldID.size;
	pubkey->ecParams.fieldID.type = key->ecParams.fieldID.type;
	pubkey->ecParams.fieldID.u.prime.len = key->ecParams.fieldID.u.prime.len;
	pubkey->ecParams.fieldID.u.prime.data = key->ecParams.fieldID.u.prime.data;
	pubkey->ecParams.fieldID.k1 = key->ecParams.fieldID.k1;
	pubkey->ecParams.fieldID.k2 = key->ecParams.fieldID.k2;
	pubkey->ecParams.fieldID.k3 = key->ecParams.fieldID.k3;
	pubkey->ecParams.curve.a.len = key->ecParams.curve.a.len;
	pubkey->ecParams.curve.a.data = key->ecParams.curve.a.data;
	pubkey->ecParams.curve.b.len = key->ecParams.curve.b.len;
	pubkey->ecParams.curve.b.data = key->ecParams.curve.b.data;
	pubkey->ecParams.curve.seed.len = key->ecParams.curve.seed.len;
	pubkey->ecParams.curve.seed.data = key->ecParams.curve.seed.data;
	pubkey->ecParams.base.len = key->ecParams.base.len;
	pubkey->ecParams.base.data = key->ecParams.base.data;
	pubkey->ecParams.order.len = key->ecParams.order.len;
	pubkey->ecParams.order.data = key->ecParams.order.data;
	pubkey->ecParams.cofactor = key->ecParams.cofactor;
	pubkey->ecParams.DEREncoding.len = key->ecParams.DEREncoding.len;
	pubkey->ecParams.DEREncoding.data = key->ecParams.DEREncoding.data;
	pubkey->ecParams.name= key->ecParams.name;
	pubkey->publicValue.len = key->publicValue.len;
	pubkey->publicValue.data = key->publicValue.data;
	cipherInfo->cipher.pubkeyCipher = ecdsa_verifyDigest;
    }
    return SECSuccess;
}
#endif

/* XXX unfortunately, this is not defined in blapi.h */
SECStatus
md2_HashBuf(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    unsigned int len;
    MD2Context *cx = MD2_NewContext();
    if (cx == NULL) return SECFailure;
    MD2_Begin(cx);
    MD2_Update(cx, src, src_length);
    MD2_End(cx, dest, &len, MD2_LENGTH);
    MD2_DestroyContext(cx, PR_TRUE);
    return SECSuccess;
}

SECStatus
md2_restart(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    MD2Context *cx, *cx_cpy;
    unsigned char *cxbytes;
    unsigned int len;
    unsigned int i, quarter;
    SECStatus rv = SECSuccess;
    cx = MD2_NewContext();
    MD2_Begin(cx);
    /* divide message by 4, restarting 3 times */
    quarter = (src_length + 3)/ 4;
    for (i=0; i < 4 && src_length > 0; i++) {
	MD2_Update(cx, src + i*quarter, PR_MIN(quarter, src_length));
	len = MD2_FlattenSize(cx);
	cxbytes = PORT_Alloc(len);
	MD2_Flatten(cx, cxbytes);
	cx_cpy = MD2_Resurrect(cxbytes, NULL);
	if (!cx_cpy) {
	    PR_fprintf(PR_STDERR, "%s: MD2_Resurrect failed!\n", progName);
	    goto finish;
	}
	rv = PORT_Memcmp(cx, cx_cpy, len);
	if (rv) {
	    MD2_DestroyContext(cx_cpy, PR_TRUE);
	    PR_fprintf(PR_STDERR, "%s: MD2_restart failed!\n", progName);
	    goto finish;
	}
	MD2_DestroyContext(cx_cpy, PR_TRUE);
	PORT_Free(cxbytes);
	src_length -= quarter;
    }
    MD2_End(cx, dest, &len, MD2_LENGTH);
finish:
    MD2_DestroyContext(cx, PR_TRUE);
    return rv;
}

SECStatus
md5_restart(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    SECStatus rv = SECSuccess;
    MD5Context *cx, *cx_cpy;
    unsigned char *cxbytes;
    unsigned int len;
    unsigned int i, quarter;
    cx = MD5_NewContext();
    MD5_Begin(cx);
    /* divide message by 4, restarting 3 times */
    quarter = (src_length + 3)/ 4;
    for (i=0; i < 4 && src_length > 0; i++) {
	MD5_Update(cx, src + i*quarter, PR_MIN(quarter, src_length));
	len = MD5_FlattenSize(cx);
	cxbytes = PORT_Alloc(len);
	MD5_Flatten(cx, cxbytes);
	cx_cpy = MD5_Resurrect(cxbytes, NULL);
	if (!cx_cpy) {
	    PR_fprintf(PR_STDERR, "%s: MD5_Resurrect failed!\n", progName);
	    rv = SECFailure;
	    goto finish;
	}
	rv = PORT_Memcmp(cx, cx_cpy, len);
	if (rv) {
	    MD5_DestroyContext(cx_cpy, PR_TRUE);
	    PR_fprintf(PR_STDERR, "%s: MD5_restart failed!\n", progName);
	    goto finish;
	}
	MD5_DestroyContext(cx_cpy, PR_TRUE);
	PORT_Free(cxbytes);
	src_length -= quarter;
    }
    MD5_End(cx, dest, &len, MD5_LENGTH);
finish:
    MD5_DestroyContext(cx, PR_TRUE);
    return rv;
}

SECStatus
sha1_restart(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    SECStatus rv = SECSuccess;
    SHA1Context *cx, *cx_cpy;
    unsigned char *cxbytes;
    unsigned int len;
    unsigned int i, quarter;
    cx = SHA1_NewContext();
    SHA1_Begin(cx);
    /* divide message by 4, restarting 3 times */
    quarter = (src_length + 3)/ 4;
    for (i=0; i < 4 && src_length > 0; i++) {
	SHA1_Update(cx, src + i*quarter, PR_MIN(quarter, src_length));
	len = SHA1_FlattenSize(cx);
	cxbytes = PORT_Alloc(len);
	SHA1_Flatten(cx, cxbytes);
	cx_cpy = SHA1_Resurrect(cxbytes, NULL);
	if (!cx_cpy) {
	    PR_fprintf(PR_STDERR, "%s: SHA1_Resurrect failed!\n", progName);
	    rv = SECFailure;
	    goto finish;
	}
	rv = PORT_Memcmp(cx, cx_cpy, len);
	if (rv) {
	    SHA1_DestroyContext(cx_cpy, PR_TRUE);
	    PR_fprintf(PR_STDERR, "%s: SHA1_restart failed!\n", progName);
	    goto finish;
	}
	SHA1_DestroyContext(cx_cpy, PR_TRUE);
	PORT_Free(cxbytes);
	src_length -= quarter;
    }
    SHA1_End(cx, dest, &len, MD5_LENGTH);
finish:
    SHA1_DestroyContext(cx, PR_TRUE);
    return rv;
}

SECStatus
SHA224_restart(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    SECStatus rv = SECSuccess;
    SHA224Context *cx, *cx_cpy;
    unsigned char *cxbytes;
    unsigned int len;
    unsigned int i, quarter;
    cx = SHA224_NewContext();
    SHA224_Begin(cx);
    /* divide message by 4, restarting 3 times */
    quarter = (src_length + 3) / 4;
    for (i=0; i < 4 && src_length > 0; i++) {
	SHA224_Update(cx, src + i*quarter, PR_MIN(quarter, src_length));
	len = SHA224_FlattenSize(cx);
	cxbytes = PORT_Alloc(len);
	SHA224_Flatten(cx, cxbytes);
	cx_cpy = SHA224_Resurrect(cxbytes, NULL);
	if (!cx_cpy) {
	    PR_fprintf(PR_STDERR, "%s: SHA224_Resurrect failed!\n", progName);
	    rv = SECFailure;
	    goto finish;
	}
	rv = PORT_Memcmp(cx, cx_cpy, len);
	if (rv) {
	    SHA224_DestroyContext(cx_cpy, PR_TRUE);
	    PR_fprintf(PR_STDERR, "%s: SHA224_restart failed!\n", progName);
	    goto finish;
	}
	
	SHA224_DestroyContext(cx_cpy, PR_TRUE);
	PORT_Free(cxbytes);
	src_length -= quarter;
    }
    SHA224_End(cx, dest, &len, MD5_LENGTH);
finish:
    SHA224_DestroyContext(cx, PR_TRUE);
    return rv;
}

SECStatus
SHA256_restart(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    SECStatus rv = SECSuccess;
    SHA256Context *cx, *cx_cpy;
    unsigned char *cxbytes;
    unsigned int len;
    unsigned int i, quarter;
    cx = SHA256_NewContext();
    SHA256_Begin(cx);
    /* divide message by 4, restarting 3 times */
    quarter = (src_length + 3)/ 4;
    for (i=0; i < 4 && src_length > 0; i++) {
	SHA256_Update(cx, src + i*quarter, PR_MIN(quarter, src_length));
	len = SHA256_FlattenSize(cx);
	cxbytes = PORT_Alloc(len);
	SHA256_Flatten(cx, cxbytes);
	cx_cpy = SHA256_Resurrect(cxbytes, NULL);
	if (!cx_cpy) {
	    PR_fprintf(PR_STDERR, "%s: SHA256_Resurrect failed!\n", progName);
	    rv = SECFailure;
	    goto finish;
	}
	rv = PORT_Memcmp(cx, cx_cpy, len);
	if (rv) {
	    SHA256_DestroyContext(cx_cpy, PR_TRUE);
	    PR_fprintf(PR_STDERR, "%s: SHA256_restart failed!\n", progName);
	    goto finish;
	}
	SHA256_DestroyContext(cx_cpy, PR_TRUE);
	PORT_Free(cxbytes);
	src_length -= quarter;
    }
    SHA256_End(cx, dest, &len, MD5_LENGTH);
finish:
    SHA256_DestroyContext(cx, PR_TRUE);
    return rv;
}

SECStatus
SHA384_restart(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    SECStatus rv = SECSuccess;
    SHA384Context *cx, *cx_cpy;
    unsigned char *cxbytes;
    unsigned int len;
    unsigned int i, quarter;
    cx = SHA384_NewContext();
    SHA384_Begin(cx);
    /* divide message by 4, restarting 3 times */
    quarter = (src_length + 3)/ 4;
    for (i=0; i < 4 && src_length > 0; i++) {
	SHA384_Update(cx, src + i*quarter, PR_MIN(quarter, src_length));
	len = SHA384_FlattenSize(cx);
	cxbytes = PORT_Alloc(len);
	SHA384_Flatten(cx, cxbytes);
	cx_cpy = SHA384_Resurrect(cxbytes, NULL);
	if (!cx_cpy) {
	    PR_fprintf(PR_STDERR, "%s: SHA384_Resurrect failed!\n", progName);
	    rv = SECFailure;
	    goto finish;
	}
	rv = PORT_Memcmp(cx, cx_cpy, len);
	if (rv) {
	    SHA384_DestroyContext(cx_cpy, PR_TRUE);
	    PR_fprintf(PR_STDERR, "%s: SHA384_restart failed!\n", progName);
	    goto finish;
	}
	SHA384_DestroyContext(cx_cpy, PR_TRUE);
	PORT_Free(cxbytes);
	src_length -= quarter;
    }
    SHA384_End(cx, dest, &len, MD5_LENGTH);
finish:
    SHA384_DestroyContext(cx, PR_TRUE);
    return rv;
}

SECStatus
SHA512_restart(unsigned char *dest, const unsigned char *src, PRUint32 src_length)
{
    SECStatus rv = SECSuccess;
    SHA512Context *cx, *cx_cpy;
    unsigned char *cxbytes;
    unsigned int len;
    unsigned int i, quarter;
    cx = SHA512_NewContext();
    SHA512_Begin(cx);
    /* divide message by 4, restarting 3 times */
    quarter = (src_length + 3)/ 4;
    for (i=0; i < 4 && src_length > 0; i++) {
	SHA512_Update(cx, src + i*quarter, PR_MIN(quarter, src_length));
	len = SHA512_FlattenSize(cx);
	cxbytes = PORT_Alloc(len);
	SHA512_Flatten(cx, cxbytes);
	cx_cpy = SHA512_Resurrect(cxbytes, NULL);
	if (!cx_cpy) {
	    PR_fprintf(PR_STDERR, "%s: SHA512_Resurrect failed!\n", progName);
	    rv = SECFailure;
	    goto finish;
	}
	rv = PORT_Memcmp(cx, cx_cpy, len);
	if (rv) {
	    SHA512_DestroyContext(cx_cpy, PR_TRUE);
	    PR_fprintf(PR_STDERR, "%s: SHA512_restart failed!\n", progName);
	    goto finish;
	}
	SHA512_DestroyContext(cx_cpy, PR_TRUE);
	PORT_Free(cxbytes);
	src_length -= quarter;
    }
    SHA512_End(cx, dest, &len, MD5_LENGTH);
finish:
    SHA512_DestroyContext(cx, PR_TRUE);
    return rv;
}

SECStatus
pubkeyInitKey(bltestCipherInfo *cipherInfo, PRFileDesc *file,
#ifdef NSS_ENABLE_ECC
	      int keysize, int exponent, char *curveName)
#else
	      int keysize, int exponent)
#endif
{
    int i;
    SECStatus rv = SECSuccess;
    bltestRSAParams *rsap;
    bltestDSAParams *dsap;
#ifdef NSS_ENABLE_ECC
    bltestECDSAParams *ecdsap;
    SECItem *tmpECParamsDER;
    ECParams *tmpECParams = NULL;
    SECItem ecSerialize[3];
#endif
    switch (cipherInfo->mode) {
    case bltestRSA:
	rsap = &cipherInfo->params.rsa;
	if (keysize > 0) {
	    SECItem expitem = { 0, 0, 0 };
	    SECITEM_AllocItem(cipherInfo->arena, &expitem, sizeof(int));
	    for (i = 1; i <= sizeof(int); i++)
		expitem.data[i-1] = exponent >> (8*(sizeof(int) - i));
	    rsap->rsakey = RSA_NewKey(keysize * 8, &expitem);
	    serialize_key(&rsap->rsakey->version, 9, file);
	    rsap->keysizeInBits = keysize * 8;
	} else {
	    setupIO(cipherInfo->arena, &cipherInfo->params.key, file, NULL, 0);
	    rsap->rsakey = rsakey_from_filedata(&cipherInfo->params.key.buf);
	    rsap->keysizeInBits = rsap->rsakey->modulus.len * 8;
	}
	break;
    case bltestDSA:
	dsap = &cipherInfo->params.dsa;
	if (keysize > 0) {
	    dsap->keysize = keysize*8;
	    if (!dsap->pqg)
		bltest_pqg_init(dsap);
	    rv = DSA_NewKey(dsap->pqg, &dsap->dsakey);
	    CHECKERROR(rv, __LINE__);
	    serialize_key(&dsap->dsakey->params.prime, 5, file);
	} else {
	    setupIO(cipherInfo->arena, &cipherInfo->params.key, file, NULL, 0);
	    dsap->dsakey = dsakey_from_filedata(&cipherInfo->params.key.buf);
	    dsap->keysize = dsap->dsakey->params.prime.len*8;
	}
	break;
#ifdef NSS_ENABLE_ECC
    case bltestECDSA:
	ecdsap = &cipherInfo->params.ecdsa;
	if (curveName != NULL) {
	    tmpECParamsDER = getECParams(curveName);
	    rv = SECOID_Init();
	    CHECKERROR(rv, __LINE__);
	    rv = EC_DecodeParams(tmpECParamsDER, &tmpECParams) == SECFailure;
	    CHECKERROR(rv, __LINE__);
	    rv = EC_NewKey(tmpECParams, &ecdsap->eckey);
	    CHECKERROR(rv, __LINE__);
	    ecSerialize[0].type = tmpECParamsDER->type;
	    ecSerialize[0].data = tmpECParamsDER->data;
	    ecSerialize[0].len  = tmpECParamsDER->len;
	    ecSerialize[1].type = ecdsap->eckey->publicValue.type;
	    ecSerialize[1].data = ecdsap->eckey->publicValue.data;
	    ecSerialize[1].len  = ecdsap->eckey->publicValue.len;
	    ecSerialize[2].type = ecdsap->eckey->privateValue.type;
	    ecSerialize[2].data = ecdsap->eckey->privateValue.data;
	    ecSerialize[2].len  = ecdsap->eckey->privateValue.len;
	    serialize_key(&(ecSerialize[0]), 3, file);
	    SECITEM_FreeItem(tmpECParamsDER, PR_TRUE);
	    PORT_FreeArena(tmpECParams->arena, PR_TRUE);
	    rv = SECOID_Shutdown();
	    CHECKERROR(rv, __LINE__);
	} else {
	    setupIO(cipherInfo->arena, &cipherInfo->params.key, file, NULL, 0);
	    ecdsap->eckey = eckey_from_filedata(&cipherInfo->params.key.buf);
	}
	break;
#endif
    default:
	return SECFailure;
    }
    return SECSuccess;
}

SECStatus
cipherInit(bltestCipherInfo *cipherInfo, PRBool encrypt)
{
    PRBool restart;
    int outlen;
    switch (cipherInfo->mode) {
    case bltestDES_ECB:
    case bltestDES_CBC:
    case bltestDES_EDE_ECB:
    case bltestDES_EDE_CBC:
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  cipherInfo->input.pBuf.len);
	return bltest_des_init(cipherInfo, encrypt);
	break;
    case bltestRC2_ECB:
    case bltestRC2_CBC:
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  cipherInfo->input.pBuf.len);
	return bltest_rc2_init(cipherInfo, encrypt);
	break;
    case bltestRC4:
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  cipherInfo->input.pBuf.len);
	return bltest_rc4_init(cipherInfo, encrypt);
	break;
#ifdef NSS_SOFTOKEN_DOES_RC5
    case bltestRC5_ECB:
    case bltestRC5_CBC:
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  cipherInfo->input.pBuf.len);
#endif
	return bltest_rc5_init(cipherInfo, encrypt);
	break;
    case bltestAES_ECB:
    case bltestAES_CBC:
    case bltestAES_CTS:
    case bltestAES_CTR:
    case bltestAES_GCM:
	outlen = cipherInfo->input.pBuf.len;
	if (cipherInfo->mode == bltestAES_GCM && encrypt) {
	    outlen += 16;
	}
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf, outlen);
	return bltest_aes_init(cipherInfo, encrypt);
	break;
    case bltestCAMELLIA_ECB:
    case bltestCAMELLIA_CBC:
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  cipherInfo->input.pBuf.len);
	return bltest_camellia_init(cipherInfo, encrypt);
	break;
    case bltestSEED_ECB:
    case bltestSEED_CBC:
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  cipherInfo->input.pBuf.len);
	return bltest_seed_init(cipherInfo, encrypt);
	break;
    case bltestRSA:
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  cipherInfo->input.pBuf.len);
	return bltest_rsa_init(cipherInfo, encrypt);
	break;
    case bltestDSA:
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  DSA_MAX_SIGNATURE_LEN);
	return bltest_dsa_init(cipherInfo, encrypt);
	break;
#ifdef NSS_ENABLE_ECC
    case bltestECDSA:
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  2 * MAX_ECKEY_LEN);
	return bltest_ecdsa_init(cipherInfo, encrypt);
	break;
#endif
    case bltestMD2:
	restart = cipherInfo->params.hash.restart;
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  MD2_LENGTH);
	cipherInfo->cipher.hashCipher = (restart) ? md2_restart : md2_HashBuf;
	return SECSuccess;
	break;
    case bltestMD5:
	restart = cipherInfo->params.hash.restart;
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  MD5_LENGTH);
	cipherInfo->cipher.hashCipher = (restart) ? md5_restart : MD5_HashBuf;
	return SECSuccess;
	break;
    case bltestSHA1:
	restart = cipherInfo->params.hash.restart;
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  SHA1_LENGTH);
	cipherInfo->cipher.hashCipher = (restart) ? sha1_restart : SHA1_HashBuf;
	return SECSuccess;
	break;
    case bltestSHA224:
	restart = cipherInfo->params.hash.restart;
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  SHA224_LENGTH);
	cipherInfo->cipher.hashCipher = (restart) ? SHA224_restart 
	                                          : SHA224_HashBuf;
	return SECSuccess;
	break;
    case bltestSHA256:
	restart = cipherInfo->params.hash.restart;
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  SHA256_LENGTH);
	cipherInfo->cipher.hashCipher = (restart) ? SHA256_restart 
	                                          : SHA256_HashBuf;
	return SECSuccess;
	break;
    case bltestSHA384:
	restart = cipherInfo->params.hash.restart;
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  SHA384_LENGTH);
	cipherInfo->cipher.hashCipher = (restart) ? SHA384_restart 
	                                          : SHA384_HashBuf;
	return SECSuccess;
	break;
    case bltestSHA512:
	restart = cipherInfo->params.hash.restart;
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  SHA512_LENGTH);
	cipherInfo->cipher.hashCipher = (restart) ? SHA512_restart 
	                                          : SHA512_HashBuf;
	return SECSuccess;
	break;
    default:
	return SECFailure;
    }
    return SECSuccess;
}

SECStatus
dsaOp(bltestCipherInfo *cipherInfo)
{
    PRIntervalTime time1, time2;
    SECStatus rv = SECSuccess;
    int i;
    int maxLen = cipherInfo->output.pBuf.len;
    SECItem dummyOut = { 0, 0, 0 };
    SECITEM_AllocItem(NULL, &dummyOut, maxLen);
    if (cipherInfo->cipher.pubkeyCipher == dsa_signDigest) {
	if (cipherInfo->params.dsa.sigseed.buf.len > 0) {
            bltestDSAParams *dsa = &cipherInfo->params.dsa;
            DSAPrivateKey *key = (DSAPrivateKey *)cipherInfo->cx;

            TIMESTART();
            rv = DSA_SignDigestWithSeed(key,
                                        &cipherInfo->output.pBuf,
                                        &cipherInfo->input.pBuf,
                                        dsa->sigseed.buf.data);
            TIMEFINISH(cipherInfo->optime, 1.0);
            CHECKERROR(rv, __LINE__);
            cipherInfo->repetitions = 0;
            if (cipherInfo->repetitionsToPerfom != 0) {
                TIMESTART();
                for (i=0; i<cipherInfo->repetitionsToPerfom;
                     i++, cipherInfo->repetitions++) {
                    rv = DSA_SignDigestWithSeed(key, &dummyOut,
                                                &cipherInfo->input.pBuf,
                                                dsa->sigseed.buf.data);
                    CHECKERROR(rv, __LINE__);
                }
            } else {
                int opsBetweenChecks = 0;
                TIMEMARK(cipherInfo->seconds);
                while (! (TIMETOFINISH())) {
                    int j = 0;
                    for (;j < opsBetweenChecks;j++) {
                        rv = DSA_SignDigestWithSeed(key, &dummyOut,
                                                    &cipherInfo->input.pBuf,
                                                    dsa->sigseed.buf.data);
                        CHECKERROR(rv, __LINE__);
                    }
                    cipherInfo->repetitions += j;
                }
            }
            TIMEFINISH(cipherInfo->optime, 1.0);
        } else {
            TIMESTART();
            rv = DSA_SignDigest((DSAPrivateKey *)cipherInfo->cx,
                                &cipherInfo->output.pBuf,
                                &cipherInfo->input.pBuf);
            TIMEFINISH(cipherInfo->optime, 1.0);
            CHECKERROR(rv, __LINE__);
            cipherInfo->repetitions = 0;
            if (cipherInfo->repetitionsToPerfom != 0) {
                TIMESTART();
                for (i=0; i<cipherInfo->repetitionsToPerfom;
                     i++, cipherInfo->repetitions++) {
                    rv = DSA_SignDigest((DSAPrivateKey *)cipherInfo->cx,
                                        &dummyOut,
                                        &cipherInfo->input.pBuf);
                    CHECKERROR(rv, __LINE__);
                }
            } else {
                int opsBetweenChecks = 0;
                TIMEMARK(cipherInfo->seconds);
                while (! (TIMETOFINISH())) {
                    int j = 0;
                    for (;j < opsBetweenChecks;j++) {
                        rv = DSA_SignDigest((DSAPrivateKey *)cipherInfo->cx,
                                            &dummyOut,
                                            &cipherInfo->input.pBuf);
                        CHECKERROR(rv, __LINE__);
                    }
                    cipherInfo->repetitions += j;
                }
            }
            TIMEFINISH(cipherInfo->optime, 1.0);
        }
	cipherInfo->output.buf.len = cipherInfo->output.pBuf.len;
        bltestCopyIO(cipherInfo->arena, &cipherInfo->params.dsa.sig, 
                     &cipherInfo->output);
    } else {
        TIMESTART();
        rv = DSA_VerifyDigest((DSAPublicKey *)cipherInfo->cx,
                              &cipherInfo->params.dsa.sig.buf,
                              &cipherInfo->input.pBuf);
        TIMEFINISH(cipherInfo->optime, 1.0);
        CHECKERROR(rv, __LINE__);
        cipherInfo->repetitions = 0;
        if (cipherInfo->repetitionsToPerfom != 0) {
            TIMESTART();
            for (i=0; i<cipherInfo->repetitionsToPerfom;
                 i++, cipherInfo->repetitions++) {
                rv = DSA_VerifyDigest((DSAPublicKey *)cipherInfo->cx,
                                      &cipherInfo->params.dsa.sig.buf,
                                      &cipherInfo->input.pBuf);
                CHECKERROR(rv, __LINE__);
            }
        } else {
            int opsBetweenChecks = 0;
            TIMEMARK(cipherInfo->seconds);
            while (! (TIMETOFINISH())) {
                int j = 0;
                for (;j < opsBetweenChecks;j++) {
                    rv = DSA_VerifyDigest((DSAPublicKey *)cipherInfo->cx,
                                          &cipherInfo->params.dsa.sig.buf,
                                          &cipherInfo->input.pBuf);
                    CHECKERROR(rv, __LINE__);
                }
                cipherInfo->repetitions += j;
            }
        }
        TIMEFINISH(cipherInfo->optime, 1.0);
    }
    SECITEM_FreeItem(&dummyOut, PR_FALSE);
    return rv;
}

#ifdef NSS_ENABLE_ECC
SECStatus
ecdsaOp(bltestCipherInfo *cipherInfo)
{
    PRIntervalTime time1, time2;
    SECStatus rv = SECSuccess;
    int i;
    int maxLen = cipherInfo->output.pBuf.len;
    SECItem dummyOut = { 0, 0, 0 };
    SECITEM_AllocItem(NULL, &dummyOut, maxLen);
    if (cipherInfo->cipher.pubkeyCipher == ecdsa_signDigest) {
        if (cipherInfo->params.ecdsa.sigseed.buf.len > 0) {
            ECPrivateKey *key = (ECPrivateKey *)cipherInfo->cx;
            bltestECDSAParams *ecdsa = &cipherInfo->params.ecdsa;

            TIMESTART();
            rv = ECDSA_SignDigestWithSeed(key,
                                          &cipherInfo->output.pBuf,
                                          &cipherInfo->input.pBuf,
                                          ecdsa->sigseed.buf.data,
                                          ecdsa->sigseed.buf.len);
            TIMEFINISH(cipherInfo->optime, 1.0);
            CHECKERROR(rv, __LINE__);
            cipherInfo->repetitions = 0;
            if (cipherInfo->repetitionsToPerfom != 0) {
                TIMESTART();
                for (i=0; i<cipherInfo->repetitionsToPerfom;
                     i++, cipherInfo->repetitions++) {
                    rv = ECDSA_SignDigestWithSeed(key, &dummyOut,
                                                  &cipherInfo->input.pBuf,
                                                  ecdsa->sigseed.buf.data,
                                                  ecdsa->sigseed.buf.len);
                    CHECKERROR(rv, __LINE__);
                }
            } else {
                int opsBetweenChecks = 0;
                TIMEMARK(cipherInfo->seconds);
                while (! (TIMETOFINISH())) {
                    int j = 0;
                    for (;j < opsBetweenChecks;j++) {
                        rv = ECDSA_SignDigestWithSeed(key, &dummyOut,
                                                      &cipherInfo->input.pBuf,
                                                      ecdsa->sigseed.buf.data,
                                                      ecdsa->sigseed.buf.len);
                        CHECKERROR(rv, __LINE__);
                    }
                    cipherInfo->repetitions += j;
                }
            }
            TIMEFINISH(cipherInfo->optime, 1.0);
        } else {
            TIMESTART();
            rv = ECDSA_SignDigest((ECPrivateKey *)cipherInfo->cx,
                                  &cipherInfo->output.pBuf,
                                  &cipherInfo->input.pBuf);
            TIMEFINISH(cipherInfo->optime, 1.0);
            CHECKERROR(rv, __LINE__);
            cipherInfo->repetitions = 0;
            if (cipherInfo->repetitionsToPerfom != 0) {
                TIMESTART();
                for (i=0; i<cipherInfo->repetitionsToPerfom;
                     i++, cipherInfo->repetitions++) {
                    rv = ECDSA_SignDigest((ECPrivateKey *)cipherInfo->cx,
                                          &dummyOut,
                                          &cipherInfo->input.pBuf);
                    CHECKERROR(rv, __LINE__);
                }
            } else {
                int opsBetweenChecks = 0;
                TIMEMARK(cipherInfo->seconds);
                while (! (TIMETOFINISH())) {
                    int j = 0;
                    for (;j < opsBetweenChecks;j++) {
                        rv = ECDSA_SignDigest((ECPrivateKey *)cipherInfo->cx,
                                              &dummyOut,
                                              &cipherInfo->input.pBuf);
                        CHECKERROR(rv, __LINE__);
                    }
                    cipherInfo->repetitions += j;
                }
            }
            TIMEFINISH(cipherInfo->optime, 1.0);
        }
        bltestCopyIO(cipherInfo->arena, &cipherInfo->params.ecdsa.sig, 
                     &cipherInfo->output);
    } else {
        TIMESTART();
        rv = ECDSA_VerifyDigest((ECPublicKey *)cipherInfo->cx,
                                &cipherInfo->params.ecdsa.sig.buf,
                                &cipherInfo->input.pBuf);
        TIMEFINISH(cipherInfo->optime, 1.0);
        CHECKERROR(rv, __LINE__);
        cipherInfo->repetitions = 0;
        if (cipherInfo->repetitionsToPerfom != 0) {
            TIMESTART();
            for (i=0; i<cipherInfo->repetitionsToPerfom;
                 i++, cipherInfo->repetitions++) {
                rv = ECDSA_VerifyDigest((ECPublicKey *)cipherInfo->cx,
                                        &cipherInfo->params.ecdsa.sig.buf,
                                        &cipherInfo->input.pBuf);
                CHECKERROR(rv, __LINE__);
            }
        } else {
            int opsBetweenChecks = 0;
            TIMEMARK(cipherInfo->seconds);
            while (! (TIMETOFINISH())) {
                int j = 0;
                for (;j < opsBetweenChecks;j++) {
                    rv = ECDSA_VerifyDigest((ECPublicKey *)cipherInfo->cx,
                                            &cipherInfo->params.ecdsa.sig.buf,
                                            &cipherInfo->input.pBuf);
                    CHECKERROR(rv, __LINE__);
                }
                cipherInfo->repetitions += j;
            }
        }
        TIMEFINISH(cipherInfo->optime, 1.0);
    }
    SECITEM_FreeItem(&dummyOut, PR_FALSE);
    return rv;
}
#endif

SECStatus
cipherDoOp(bltestCipherInfo *cipherInfo)
{
    PRIntervalTime time1, time2;
    SECStatus rv = SECSuccess;
    int i;
    unsigned int len;
    unsigned int maxLen = cipherInfo->output.pBuf.len;
    unsigned char *dummyOut;
    if (cipherInfo->mode == bltestDSA)
	return dsaOp(cipherInfo);
#ifdef NSS_ENABLE_ECC
    else if (cipherInfo->mode == bltestECDSA)
	return ecdsaOp(cipherInfo);
#endif
    dummyOut = PORT_Alloc(maxLen);
    if (is_symmkeyCipher(cipherInfo->mode)) {
        const unsigned char *input = cipherInfo->input.pBuf.data;
        unsigned int inputLen = is_singleShotCipher(cipherInfo->mode) ?
                 cipherInfo->input.pBuf.len :
                 PR_MIN(cipherInfo->input.pBuf.len, 16);
        unsigned char *output = cipherInfo->output.pBuf.data;
        unsigned int outputLen = maxLen;
        unsigned int totalOutputLen = 0;
        TIMESTART();
        rv = (*cipherInfo->cipher.symmkeyCipher)(cipherInfo->cx,
                                                 output, &len, outputLen,
                                                 input, inputLen);
        CHECKERROR(rv, __LINE__);
        totalOutputLen += len;
        if (cipherInfo->input.pBuf.len > inputLen) {
            input += inputLen;
            inputLen = cipherInfo->input.pBuf.len - inputLen;
            output += len;
            outputLen -= len;
            rv = (*cipherInfo->cipher.symmkeyCipher)(cipherInfo->cx,
                                                     output, &len, outputLen,
                                                     input, inputLen);
            CHECKERROR(rv, __LINE__);
	    totalOutputLen += len;
        }
	cipherInfo->output.pBuf.len = totalOutputLen;
        TIMEFINISH(cipherInfo->optime, 1.0);
        cipherInfo->repetitions = 0;
        if (cipherInfo->repetitionsToPerfom != 0) {
            TIMESTART();
            for (i=0; i<cipherInfo->repetitionsToPerfom; i++,
                     cipherInfo->repetitions++) {
                (*cipherInfo->cipher.symmkeyCipher)(cipherInfo->cx, dummyOut,
                                                    &len, maxLen,
                                                    cipherInfo->input.pBuf.data,
                                                    cipherInfo->input.pBuf.len);
                
                CHECKERROR(rv, __LINE__);
            }
        } else {
            int opsBetweenChecks = 0;
            TIMEMARK(cipherInfo->seconds);
            while (! (TIMETOFINISH())) {
                int j = 0;
                for (;j < opsBetweenChecks;j++) {
                    (*cipherInfo->cipher.symmkeyCipher)(
                        cipherInfo->cx, dummyOut, &len, maxLen,
                        cipherInfo->input.pBuf.data,
                        cipherInfo->input.pBuf.len);
                }
                cipherInfo->repetitions += j;
            }
        }
        TIMEFINISH(cipherInfo->optime, 1.0);
    } else if (is_pubkeyCipher(cipherInfo->mode)) {
        TIMESTART();
        rv = (*cipherInfo->cipher.pubkeyCipher)(cipherInfo->cx,
                                                &cipherInfo->output.pBuf,
                                                &cipherInfo->input.pBuf);
        TIMEFINISH(cipherInfo->optime, 1.0);
        CHECKERROR(rv, __LINE__);
        cipherInfo->repetitions = 0;
        if (cipherInfo->repetitionsToPerfom != 0) {
            TIMESTART();
            for (i=0; i<cipherInfo->repetitionsToPerfom;
                 i++, cipherInfo->repetitions++) {
                SECItem dummy;
                dummy.data = dummyOut;
                dummy.len = maxLen;
                (*cipherInfo->cipher.pubkeyCipher)(cipherInfo->cx, &dummy, 
                                                   &cipherInfo->input.pBuf);
                CHECKERROR(rv, __LINE__);
            }
        } else {
            int opsBetweenChecks = 0;
            TIMEMARK(cipherInfo->seconds);
            while (! (TIMETOFINISH())) {
                int j = 0;
                for (;j < opsBetweenChecks;j++) {
                    SECItem dummy;
                    dummy.data = dummyOut;
                    dummy.len = maxLen;
                    (*cipherInfo->cipher.pubkeyCipher)(cipherInfo->cx, &dummy,
                                                       &cipherInfo->input.pBuf);
                    CHECKERROR(rv, __LINE__);
                }
                cipherInfo->repetitions += j;
            }
        }
        TIMEFINISH(cipherInfo->optime, 1.0);
    } else if (is_hashCipher(cipherInfo->mode)) {
        TIMESTART();
        rv = (*cipherInfo->cipher.hashCipher)(cipherInfo->output.pBuf.data,
                                              cipherInfo->input.pBuf.data,
                                              cipherInfo->input.pBuf.len);
        TIMEFINISH(cipherInfo->optime, 1.0);
        CHECKERROR(rv, __LINE__);
        cipherInfo->repetitions = 0;
        if (cipherInfo->repetitionsToPerfom != 0) {
            TIMESTART();
            for (i=0; i<cipherInfo->repetitionsToPerfom;
                 i++, cipherInfo->repetitions++) {
                (*cipherInfo->cipher.hashCipher)(dummyOut,
                                                 cipherInfo->input.pBuf.data,
                                                 cipherInfo->input.pBuf.len);
                CHECKERROR(rv, __LINE__);
            }
        } else {
            int opsBetweenChecks = 0;
            TIMEMARK(cipherInfo->seconds);
            while (! (TIMETOFINISH())) {
                int j = 0;
                for (;j < opsBetweenChecks;j++) {
                    bltestIO *input = &cipherInfo->input;
                    (*cipherInfo->cipher.hashCipher)(dummyOut,
                                                     input->pBuf.data,
                                                     input->pBuf.len);
                    CHECKERROR(rv, __LINE__);
                }
                cipherInfo->repetitions += j;
            }
        }
        TIMEFINISH(cipherInfo->optime, 1.0);
    }
    PORT_Free(dummyOut);
    return rv;
}

SECStatus
cipherFinish(bltestCipherInfo *cipherInfo)
{
    SECStatus rv = SECSuccess;

    switch (cipherInfo->mode) {
    case bltestDES_ECB:
    case bltestDES_CBC:
    case bltestDES_EDE_ECB:
    case bltestDES_EDE_CBC:
	DES_DestroyContext((DESContext *)cipherInfo->cx, PR_TRUE);
	break;
    case bltestAES_GCM:
    case bltestAES_ECB:
    case bltestAES_CBC:
    case bltestAES_CTS:
    case bltestAES_CTR:
	AES_DestroyContext((AESContext *)cipherInfo->cx, PR_TRUE);
	break;
    case bltestCAMELLIA_ECB:
    case bltestCAMELLIA_CBC:
	Camellia_DestroyContext((CamelliaContext *)cipherInfo->cx, PR_TRUE);
	break;
    case bltestSEED_ECB:
    case bltestSEED_CBC:
	SEED_DestroyContext((SEEDContext *)cipherInfo->cx, PR_TRUE);
	break;
    case bltestRC2_ECB:
    case bltestRC2_CBC:
	RC2_DestroyContext((RC2Context *)cipherInfo->cx, PR_TRUE);
	break;
    case bltestRC4:
	RC4_DestroyContext((RC4Context *)cipherInfo->cx, PR_TRUE);
	break;
#ifdef NSS_SOFTOKEN_DOES_RC5
    case bltestRC5_ECB:
    case bltestRC5_CBC:
	RC5_DestroyContext((RC5Context *)cipherInfo->cx, PR_TRUE);
	break;
#endif
    case bltestRSA: /* keys are alloc'ed within cipherInfo's arena, */
    case bltestDSA: /* will be freed with it. */
#ifdef NSS_ENABLE_ECC
    case bltestECDSA:
#endif
    case bltestMD2: /* hash contexts are ephemeral */
    case bltestMD5:
    case bltestSHA1:
    case bltestSHA224:
    case bltestSHA256:
    case bltestSHA384:
    case bltestSHA512:
	return SECSuccess;
	break;
    default:
	return SECFailure;
    }
    return rv;
}

void
print_exponent(SECItem *exp)
{
    int i;
    int e = 0;
    if (exp->len <= 4) {
	for (i=exp->len; i >=0; --i) e |= exp->data[exp->len-i] << 8*(i-1);
	fprintf(stdout, "%12d", e);
    } else {
	e = 8*exp->len;
	fprintf(stdout, "~2**%-8d", e);
    }
}

static void
splitToReportUnit(PRInt64 res, int *resArr, int *del, int size)
{
    PRInt64 remaining = res, tmp = 0;
    PRInt64 Ldel;
    int i = -1;

    while (remaining > 0 && ++i < size) {
        LL_I2L(Ldel, del[i]);
        LL_MOD(tmp, remaining, Ldel);
        LL_L2I(resArr[i], tmp);
        LL_DIV(remaining, remaining, Ldel);
    }
}

static char*
getHighUnitBytes(PRInt64 res)
{
    int spl[] = {0, 0, 0, 0};
    int del[] = {1024, 1024, 1024, 1024};
    char *marks[] = {"b", "Kb", "Mb", "Gb"};
    int i = 3;

    splitToReportUnit(res, spl, del, 4);

    for (;i>0;i--) {
        if (spl[i] != 0) {
            break;
        }
    }

    return PR_smprintf("%d%s", spl[i], marks[i]);
}


static void
printPR_smpString(const char *sformat, char *reportStr,
                  const char *nformat, PRInt64 rNum)
{
    if (reportStr) {
        fprintf(stdout, sformat, reportStr);
        PR_smprintf_free(reportStr);
    } else {
        int prnRes;
        LL_L2I(prnRes, rNum);
        fprintf(stdout, nformat, rNum);
    }
}

static char*
getHighUnitOps(PRInt64 res)
{
    int spl[] = {0, 0, 0, 0};
    int del[] = {1000, 1000, 1000, 1000};
    char *marks[] = {"", "T", "M", "B"};
    int i = 3;

    splitToReportUnit(res, spl, del, 4);

    for (;i>0;i--) {
        if (spl[i] != 0) {
            break;
        }
    }

    return PR_smprintf("%d%s", spl[i], marks[i]);
}

void
dump_performance_info(bltestCipherInfo *infoList, double totalTimeInt,
                      PRBool encrypt, PRBool cxonly)
{
    bltestCipherInfo *info = infoList;
    
    PRInt64 totalIn = 0;
    PRBool td = PR_TRUE;

    int   repetitions = 0;
    int   cxreps = 0;
    double cxtime = 0;
    double optime = 0;
    while (info != NULL) {
        repetitions += info->repetitions;
        cxreps += info->cxreps;
        cxtime += info->cxtime;
        optime += info->optime;
        totalIn += (PRInt64) info->input.buf.len * (PRInt64) info->repetitions;
        
        info = info->next;
    }
    info = infoList;

    fprintf(stdout, "#%9s", "mode");
    fprintf(stdout, "%12s", "in");
print_td:
    switch (info->mode) {
      case bltestDES_ECB:
      case bltestDES_CBC:
      case bltestDES_EDE_ECB:
      case bltestDES_EDE_CBC:
      case bltestAES_ECB:
      case bltestAES_CBC:
      case bltestAES_CTS:
      case bltestAES_CTR:
      case bltestAES_GCM:
      case bltestCAMELLIA_ECB:
      case bltestCAMELLIA_CBC:
      case bltestSEED_ECB:
      case bltestSEED_CBC:
      case bltestRC2_ECB:
      case bltestRC2_CBC:
      case bltestRC4:
          if (td)
              fprintf(stdout, "%8s", "symmkey");
          else
              fprintf(stdout, "%8d", 8*info->params.sk.key.buf.len);
          break;
#ifdef NSS_SOFTOKEN_DOES_RC5
      case bltestRC5_ECB:
      case bltestRC5_CBC:
          if (info->params.sk.key.buf.len > 0)
              printf("symmetric key(bytes)=%d,", info->params.sk.key.buf.len);
          if (info->rounds > 0)
              printf("rounds=%d,", info->params.rc5.rounds);
          if (info->wordsize > 0)
              printf("wordsize(bytes)=%d,", info->params.rc5.wordsize);
          break;
#endif
      case bltestRSA:
          if (td) {
              fprintf(stdout, "%8s", "rsa_mod");
              fprintf(stdout, "%12s", "rsa_pe");
          } else {
              fprintf(stdout, "%8d", info->params.rsa.keysizeInBits);
              print_exponent(&info->params.rsa.rsakey->publicExponent);
          }
          break;
      case bltestDSA:
          if (td)
              fprintf(stdout, "%8s", "pqg_mod");
          else
              fprintf(stdout, "%8d", info->params.dsa.keysize);
          break;
#ifdef NSS_ENABLE_ECC
      case bltestECDSA:
          if (td)
              fprintf(stdout, "%12s", "ec_curve");
          else {
	      ECCurveName curveName = info->params.ecdsa.eckey->ecParams.name;
              fprintf(stdout, "%12s",
                      ecCurve_map[curveName]? ecCurve_map[curveName]->text:
					      "Unsupported curve");
	  }
          break;
#endif
      case bltestMD2:
      case bltestMD5:
      case bltestSHA1:
      case bltestSHA256:
      case bltestSHA384:
      case bltestSHA512:
      default:
          break;
    }
    if (!td) {
        PRInt64 totalThroughPut;

        printPR_smpString("%8s", getHighUnitOps(repetitions),
                          "%8d", repetitions);

        printPR_smpString("%8s", getHighUnitOps(cxreps), "%8d", cxreps);

        fprintf(stdout, "%12.3f", cxtime);
        fprintf(stdout, "%12.3f", optime);
        fprintf(stdout, "%12.03f", totalTimeInt / 1000);

        totalThroughPut = (PRInt64)(totalIn / totalTimeInt * 1000);
        printPR_smpString("%12s", getHighUnitBytes(totalThroughPut),
                          "%12d", totalThroughPut);

        fprintf(stdout, "\n");
        return;
    }
    
    fprintf(stdout, "%8s", "opreps");
    fprintf(stdout, "%8s", "cxreps");
    fprintf(stdout, "%12s", "context");
    fprintf(stdout, "%12s", "op");
    fprintf(stdout, "%12s", "time(sec)");
    fprintf(stdout, "%12s", "thrgput");
    fprintf(stdout, "\n");
    fprintf(stdout, "%8s", mode_strings[info->mode]);
    fprintf(stdout, "_%c", (cxonly) ? 'c' : (encrypt) ? 'e' : 'd');
    printPR_smpString("%12s", getHighUnitBytes(totalIn), "%12d", totalIn);
    
    td = !td;
    goto print_td;
}

void
printmodes()
{
    bltestCipherMode mode;
    int nummodes = sizeof(mode_strings) / sizeof(char *);
    fprintf(stderr, "%s: Available modes (specify with -m):\n", progName);
    for (mode=0; mode<nummodes; mode++)
	fprintf(stderr, "%s\n", mode_strings[mode]);
}

bltestCipherMode
get_mode(const char *modestring)
{
    bltestCipherMode mode;
    int nummodes = sizeof(mode_strings) / sizeof(char *);
    for (mode=0; mode<nummodes; mode++)
	if (PL_strcmp(modestring, mode_strings[mode]) == 0)
	    return mode;
    fprintf(stderr, "%s: invalid mode: %s\n", progName, modestring);
    return bltestINVALID;
}

void
load_file_data(PLArenaPool *arena, bltestIO *data,
	       char *fn, bltestIOMode ioMode)
{
    PRFileDesc *file;
    data->mode = ioMode;
    data->file = NULL; /* don't use -- not saving anything */
    data->pBuf.data = NULL;
    data->pBuf.len = 0;
    file = PR_Open(fn, PR_RDONLY, 00660);
    if (file)
	setupIO(arena, data, file, NULL, 0);
}

void
get_params(PLArenaPool *arena, bltestParams *params,
	   bltestCipherMode mode, int j)
{
    char filename[256];
    char *modestr = mode_strings[mode];
#ifdef NSS_SOFTOKEN_DOES_RC5
    FILE *file;
    char *mark, *param, *val;
    int index = 0;
#endif
    switch (mode) {
    case bltestAES_GCM:
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "aad", j);
	load_file_data(arena, &params->ask.aad, filename, bltestBinary);
    case bltestDES_CBC:
    case bltestDES_EDE_CBC:
    case bltestRC2_CBC:
    case bltestAES_CBC:
    case bltestAES_CTS:
    case bltestAES_CTR:
    case bltestCAMELLIA_CBC:
    case bltestSEED_CBC: 
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "iv", j);
	load_file_data(arena, &params->sk.iv, filename, bltestBinary);
    case bltestDES_ECB:
    case bltestDES_EDE_ECB:
    case bltestRC2_ECB:
    case bltestRC4:
    case bltestAES_ECB:
    case bltestCAMELLIA_ECB:
    case bltestSEED_ECB:
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "key", j);
	load_file_data(arena, &params->sk.key, filename, bltestBinary);
	break;
#ifdef NSS_SOFTOKEN_DOES_RC5
    case bltestRC5_ECB:
    case bltestRC5_CBC:
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "iv", j);
	load_file_data(arena, &params->sk.iv, filename, bltestBinary);
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "key", j);
	load_file_data(arena, &params->sk.key, filename, bltestBinary);
	    sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr,
			      "params", j);
	file = fopen(filename, "r");
	if (!file) return;
	param = malloc(100);
	len = fread(param, 1, 100, file);
	while (index < len) {
	    mark = PL_strchr(param, '=');
	    *mark = '\0';
	    val = mark + 1;
	    mark = PL_strchr(val, '\n');
	    *mark = '\0';
	    if (PL_strcmp(param, "rounds") == 0) {
		params->rc5.rounds = atoi(val);
	    } else if (PL_strcmp(param, "wordsize") == 0) {
		params->rc5.wordsize = atoi(val);
	    }
	    index += PL_strlen(param) + PL_strlen(val) + 2;
	    param = mark + 1;
	}
	break;
#endif
    case bltestRSA:
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "key", j);
	load_file_data(arena, &params->rsa.key, filename, bltestBase64Encoded);
	params->rsa.rsakey = rsakey_from_filedata(&params->key.buf);
	break;
    case bltestDSA:
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "key", j);
	load_file_data(arena, &params->dsa.key, filename, bltestBase64Encoded);
	params->dsa.dsakey = dsakey_from_filedata(&params->key.buf);
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "pqg", j);
	load_file_data(arena, &params->dsa.pqgdata, filename,
		       bltestBase64Encoded);
	params->dsa.pqg = pqg_from_filedata(&params->dsa.pqgdata.buf);
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "keyseed", j);
	load_file_data(arena, &params->dsa.keyseed, filename, 
	               bltestBase64Encoded);
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "sigseed", j);
	load_file_data(arena, &params->dsa.sigseed, filename, 
	               bltestBase64Encoded);
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "ciphertext",j);
	load_file_data(arena, &params->dsa.sig, filename, bltestBase64Encoded);
	break;
#ifdef NSS_ENABLE_ECC
    case bltestECDSA:
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "key", j);
	load_file_data(arena, &params->ecdsa.key, filename, bltestBase64Encoded);
	params->ecdsa.eckey = eckey_from_filedata(&params->key.buf);
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "sigseed", j);
	load_file_data(arena, &params->ecdsa.sigseed, filename, 
	               bltestBase64Encoded);
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "ciphertext",j);
	load_file_data(arena, &params->ecdsa.sig, filename, bltestBase64Encoded);
	break;
#endif
    case bltestMD2:
    case bltestMD5:
    case bltestSHA1:
    case bltestSHA224:
    case bltestSHA256:
    case bltestSHA384:
    case bltestSHA512:
	/*params->hash.restart = PR_TRUE;*/
	params->hash.restart = PR_FALSE;
	break;
    default:
	break;
    }
}

SECStatus
verify_self_test(bltestIO *result, bltestIO *cmp, bltestCipherMode mode,
		 PRBool forward, SECStatus sigstatus)
{
    int res;
    char *modestr = mode_strings[mode];
    res = SECITEM_CompareItem(&result->pBuf, &cmp->buf);
    if (is_sigCipher(mode)) {
	if (forward) {
	    if (res == 0) {
		printf("Signature self-test for %s passed.\n", modestr);
	    } else {
		printf("Signature self-test for %s failed!\n", modestr);
	    }
	} else {
	    if (sigstatus == SECSuccess) {
		printf("Verification self-test for %s passed.\n", modestr);
	    } else {
		printf("Verification self-test for %s failed!\n", modestr);
	    }
	}
	return sigstatus;
    } else if (is_hashCipher(mode)) {
	if (res == 0) {
	    printf("Hash self-test for %s passed.\n", modestr);
	} else {
	    printf("Hash self-test for %s failed!\n", modestr);
	}
    } else {
	if (forward) {
	    if (res == 0) {
		printf("Encryption self-test for %s passed.\n", modestr);
	    } else {
		printf("Encryption self-test for %s failed!\n", modestr);
	    }
	} else {
	    if (res == 0) {
		printf("Decryption self-test for %s passed.\n", modestr);
	    } else {
		printf("Decryption self-test for %s failed!\n", modestr);
	    }
	}
    }
    return (res != 0);
}

static SECStatus
blapi_selftest(bltestCipherMode *modes, int numModes, int inoff, int outoff,
               PRBool encrypt, PRBool decrypt)
{
    bltestCipherInfo cipherInfo;
    bltestIO pt, ct;
    bltestCipherMode mode;
    bltestParams *params;
    int i, j, nummodes, numtests;
    char *modestr;
    char filename[256];
    PRFileDesc *file;
    PLArenaPool *arena;
    SECItem item;
    PRBool finished;
    SECStatus rv = SECSuccess, srv;

    PORT_Memset(&cipherInfo, 0, sizeof(cipherInfo));
    arena = PORT_NewArena(BLTEST_DEFAULT_CHUNKSIZE);
    cipherInfo.arena = arena;

    finished = PR_FALSE;
    nummodes = (numModes == 0) ? NUMMODES : numModes;
    for (i=0; i < nummodes && !finished; i++) {
	if (numModes > 0)
	    mode = modes[i];
	else
	    mode = i;
	if (mode == bltestINVALID) {
	    fprintf(stderr, "%s: Skipping invalid mode.\n",progName);
	    continue;
	}
	modestr = mode_strings[mode];
	cipherInfo.mode = mode;
	params = &cipherInfo.params;
#ifdef TRACK_BLTEST_BUG
	if (mode == bltestRSA) {
	    fprintf(stderr, "[%s] Self-Testing RSA\n", __bltDBG);
	}
#endif
	/* get the number of tests in the directory */
	sprintf(filename, "%s/tests/%s/%s", testdir, modestr, "numtests");
	file = PR_Open(filename, PR_RDONLY, 00660);
	if (!file) {
	    fprintf(stderr, "%s: File %s does not exist.\n", progName,filename);
	    return SECFailure;
	}
	rv = SECU_FileToItem(&item, file);
#ifdef TRACK_BLTEST_BUG
	if (mode == bltestRSA) {
	    fprintf(stderr, "[%s] Loaded data from %s\n", __bltDBG, filename);
	}
#endif
	PR_Close(file);
	/* loop over the tests in the directory */
	numtests = 0;
	for (j=0; j<item.len; j++) {
	    if (!isdigit(item.data[j])) {
		break;
	    }
	    numtests *= 10;
	    numtests += (int) (item.data[j] - '0');
	}
	for (j=0; j<numtests; j++) {
#ifdef TRACK_BLTEST_BUG
	    if (mode == bltestRSA) {
		fprintf(stderr, "[%s] Executing self-test #%d\n", __bltDBG, j);
	    }
#endif
	    sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr,
			      "plaintext", j);
	    load_file_data(arena, &pt, filename, 
#ifdef NSS_ENABLE_ECC
	                   ((mode == bltestDSA) || (mode == bltestECDSA))
#else
	                   (mode == bltestDSA)
#endif
	                   ? bltestBase64Encoded : bltestBinary);
	    sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr,
			      "ciphertext", j);
	    load_file_data(arena, &ct, filename, bltestBase64Encoded);

#ifdef TRACK_BLTEST_BUG
	    if (mode == bltestRSA) {
		fprintf(stderr, "[%s] Loaded data for  self-test #%d\n", __bltDBG, j);
	    }
#endif
	    get_params(arena, params, mode, j);
#ifdef TRACK_BLTEST_BUG
	    if (mode == bltestRSA) {
		fprintf(stderr, "[%s] Got parameters for #%d\n", __bltDBG, j);
	    }
#endif
	    /* Forward Operation (Encrypt/Sign/Hash)
	    ** Align the input buffer (plaintext) according to request
	    ** then perform operation and compare to ciphertext
	    */
	    /* XXX for now */
	    rv = SECSuccess;
	    if (encrypt) {
		bltestCopyIO(arena, &cipherInfo.input, &pt);
		misalignBuffer(arena, &cipherInfo.input, inoff);
		memset(&cipherInfo.output.buf, 0, sizeof cipherInfo.output.buf);
		rv |= cipherInit(&cipherInfo, PR_TRUE);
		misalignBuffer(arena, &cipherInfo.output, outoff);
#ifdef TRACK_BLTEST_BUG
		if (mode == bltestRSA) {
		    fprintf(stderr, "[%s] Inited cipher context and buffers for #%d\n", __bltDBG, j);
		}
#endif
		rv |= cipherDoOp(&cipherInfo);
#ifdef TRACK_BLTEST_BUG
		if (mode == bltestRSA) {
		    fprintf(stderr, "[%s] Performed encrypt for #%d\n", __bltDBG, j);
		}
#endif
		rv |= cipherFinish(&cipherInfo);
#ifdef TRACK_BLTEST_BUG
		if (mode == bltestRSA) {
		    fprintf(stderr, "[%s] Finished encrypt for #%d\n", __bltDBG, j);
		}
#endif
		rv |= verify_self_test(&cipherInfo.output, 
		                       &ct, mode, PR_TRUE, 0);
#ifdef TRACK_BLTEST_BUG
		if (mode == bltestRSA) {
		    fprintf(stderr, "[%s] Verified self-test for #%d\n", __bltDBG, j);
		}
#endif
		/* If testing hash, only one op to test */
		if (is_hashCipher(mode))
		    continue;
		/*if (rv) return rv;*/
	    }
	    if (!decrypt)
		continue;
	    /* XXX for now */
	    rv = SECSuccess;
	    /* Reverse Operation (Decrypt/Verify)
	    ** Align the input buffer (ciphertext) according to request
	    ** then perform operation and compare to plaintext
	    */
#ifdef NSS_ENABLE_ECC
	    if ((mode != bltestDSA) && (mode != bltestECDSA))
#else
	    if (mode != bltestDSA)
#endif
		bltestCopyIO(arena, &cipherInfo.input, &ct);
	    else
		bltestCopyIO(arena, &cipherInfo.input, &pt);
	    misalignBuffer(arena, &cipherInfo.input, inoff);
	    memset(&cipherInfo.output.buf, 0, sizeof cipherInfo.output.buf);
	    rv |= cipherInit(&cipherInfo, PR_FALSE);
	    misalignBuffer(arena, &cipherInfo.output, outoff);
#ifdef TRACK_BLTEST_BUG
	    if (mode == bltestRSA) {
		fprintf(stderr, "[%s] Inited cipher context and buffers for #%d\n", __bltDBG, j);
	    }
#endif
	    srv = SECSuccess;
	    srv |= cipherDoOp(&cipherInfo);
#ifdef TRACK_BLTEST_BUG
	    if (mode == bltestRSA) {
		fprintf(stderr, "[%s] Performed decrypt for #%d\n", __bltDBG, j);
	    }
#endif
	    rv |= cipherFinish(&cipherInfo);
#ifdef TRACK_BLTEST_BUG
	    if (mode == bltestRSA) {
		fprintf(stderr, "[%s] Finished decrypt for #%d\n", __bltDBG, j);
	    }
#endif
	    rv |= verify_self_test(&cipherInfo.output, 
	                           &pt, mode, PR_FALSE, srv);
#ifdef TRACK_BLTEST_BUG
	    if (mode == bltestRSA) {
		fprintf(stderr, "[%s] Verified self-test for #%d\n", __bltDBG, j);
	    }
#endif
	    /*if (rv) return rv;*/
	}
    }
    return rv;
}

SECStatus
dump_file(bltestCipherMode mode, char *filename)
{
    bltestIO keydata;
    PLArenaPool *arena = NULL;
    arena = PORT_NewArena(BLTEST_DEFAULT_CHUNKSIZE);
    if (mode == bltestRSA) {
	RSAPrivateKey *key;
	load_file_data(arena, &keydata, filename, bltestBase64Encoded);
	key = rsakey_from_filedata(&keydata.buf);
	dump_rsakey(key);
    } else if (mode == bltestDSA) {
#if 0
	PQGParams *pqg;
	get_file_data(filename, &item, PR_TRUE);
	pqg = pqg_from_filedata(&item);
	dump_pqg(pqg);
#endif
	DSAPrivateKey *key;
	load_file_data(arena, &keydata, filename, bltestBase64Encoded);
	key = dsakey_from_filedata(&keydata.buf);
	dump_dsakey(key);
#ifdef NSS_ENABLE_ECC
    } else if (mode == bltestECDSA) {
	ECPrivateKey *key;
	load_file_data(arena, &keydata, filename, bltestBase64Encoded);
	key = eckey_from_filedata(&keydata.buf);
	dump_eckey(key);
#endif
    }
    PORT_FreeArena(arena, PR_FALSE);
    return SECFailure;
}

void ThreadExecTest(void *data)
{
    bltestCipherInfo *cipherInfo = (bltestCipherInfo*)data;

    if (cipherInfo->mCarlo == PR_TRUE) {
        int mciter;
        for (mciter=0; mciter<10000; mciter++) {
            cipherDoOp(cipherInfo);
            memcpy(cipherInfo->input.buf.data,
                   cipherInfo->output.buf.data,
                   cipherInfo->input.buf.len);
        }
    } else {
        cipherDoOp(cipherInfo);
    }
    cipherFinish(cipherInfo);
}

static void rsaPrivKeyReset(RSAPrivateKey *tstKey)
{
    PLArenaPool *arena;

    tstKey->version.data = NULL;
    tstKey->version.len = 0;
    tstKey->modulus.data = NULL;
    tstKey->modulus.len = 0;
    tstKey->publicExponent.data = NULL;
    tstKey->publicExponent.len = 0;
    tstKey->privateExponent.data = NULL;
    tstKey->privateExponent.len = 0;
    tstKey->prime1.data = NULL;
    tstKey->prime1.len = 0;
    tstKey->prime2.data = NULL;
    tstKey->prime2.len = 0;
    tstKey->exponent1.data = NULL;
    tstKey->exponent1.len = 0;
    tstKey->exponent2.data = NULL;
    tstKey->exponent2.len = 0;
    tstKey->coefficient.data = NULL;
    tstKey->coefficient.len = 0;

    arena = tstKey->arena;
    tstKey->arena = NULL;
    if (arena) {
	PORT_FreeArena(arena, PR_TRUE);
    }
}


#define RSA_TEST_EQUAL(comp) \
    if (!SECITEM_ItemsAreEqual(&(src->comp),&(dest->comp))) { \
	fprintf(stderr, "key->" #comp " not equal"); \
	if (src->comp.len != dest->comp.len) { \
	    fprintf(stderr, "src_len = %d, dest_len = %d",  \
					src->comp.len, dest->comp.len); \
	} \
	fprintf(stderr, "\n"); \
	areEqual = PR_FALSE; \
    }
	    

static PRBool rsaPrivKeysAreEqual(RSAPrivateKey *src, RSAPrivateKey *dest)
{
    PRBool areEqual = PR_TRUE;
    RSA_TEST_EQUAL(modulus)
    RSA_TEST_EQUAL(publicExponent)
    RSA_TEST_EQUAL(privateExponent)
    RSA_TEST_EQUAL(prime1)
    RSA_TEST_EQUAL(prime2)
    RSA_TEST_EQUAL(exponent1)
    RSA_TEST_EQUAL(exponent2)
    RSA_TEST_EQUAL(coefficient)
    if (!areEqual) {
	fprintf(stderr, "original key:\n");
	dump_rsakey(src);
	fprintf(stderr, "recreated key:\n");
	dump_rsakey(dest);
    }
    return areEqual;
}

/*
 * Test the RSA populate command to see that it can really build
 * keys from it's components.
 */
static int doRSAPopulateTest(unsigned int keySize, unsigned long exponent)
{
    RSAPrivateKey *srcKey;
    RSAPrivateKey tstKey = { 0 };
    SECItem expitem = { 0, 0, 0 };
    SECStatus rv;
    unsigned char pubExp[4];
    int expLen = 0;
    int failed = 0;
    int i;

    for (i=0; i < sizeof(unsigned long); i++) {
	int shift = (sizeof(unsigned long) - i -1 ) * 8;
	if (expLen || (exponent && ((unsigned long)0xffL << shift))) {
	    pubExp[expLen] = (unsigned char) ((exponent >> shift) & 0xff);
	    expLen++;
        }
    }

    expitem.data = pubExp;
    expitem.len = expLen;

    srcKey = RSA_NewKey(keySize, &expitem);
    if (srcKey == NULL) {
	fprintf(stderr, "RSA Key Gen failed");
	return -1;
    }

    /* test the basic case - most common, public exponent, modulus, prime */
    tstKey.arena = NULL;
    rsaPrivKeyReset(&tstKey);

    tstKey.publicExponent = srcKey->publicExponent;
    tstKey.modulus = srcKey->modulus;
    tstKey.prime1 = srcKey->prime1;

    rv = RSA_PopulatePrivateKey(&tstKey);
    if (rv != SECSuccess) {
	fprintf(stderr, "RSA Populate failed: pubExp mod p\n");
	failed = 1;
    } else if (!rsaPrivKeysAreEqual(&tstKey, srcKey)) {
	fprintf(stderr, "RSA Populate key mismatch: pubExp mod p\n");
	failed = 1;
    }

    /* test the basic2 case, public exponent, modulus, prime2 */
    rsaPrivKeyReset(&tstKey);

    tstKey.publicExponent = srcKey->publicExponent;
    tstKey.modulus = srcKey->modulus;
    tstKey.prime1 = srcKey->prime2; /* test with q in the prime1 position */

    rv = RSA_PopulatePrivateKey(&tstKey);
    if (rv != SECSuccess) {
	fprintf(stderr, "RSA Populate failed: pubExp mod q\n");
	failed = 1;
    } else if (!rsaPrivKeysAreEqual(&tstKey, srcKey)) {
	fprintf(stderr, "RSA Populate key mismatch: pubExp mod q\n");
	failed = 1;
    }

    /* test the medium case, private exponent, prime1, prime2 */
    rsaPrivKeyReset(&tstKey);

    tstKey.privateExponent = srcKey->privateExponent;
    tstKey.prime1 = srcKey->prime2; /* purposefully swap them to make */
    tstKey.prime2 = srcKey->prime1; /* sure populated swaps them back */

    rv = RSA_PopulatePrivateKey(&tstKey);
    if (rv != SECSuccess) {
	fprintf(stderr, "RSA Populate failed: privExp p q\n");
	failed = 1;
    } else if (!rsaPrivKeysAreEqual(&tstKey, srcKey)) {
	fprintf(stderr, "RSA Populate key mismatch: privExp  p q\n");
	failed = 1;
    }

    /* test the advanced case, public exponent, private exponent, prime2 */
    rsaPrivKeyReset(&tstKey);

    tstKey.privateExponent = srcKey->privateExponent;
    tstKey.publicExponent = srcKey->publicExponent;
    tstKey.prime2 = srcKey->prime2; /* use q in the prime2 position */

    rv = RSA_PopulatePrivateKey(&tstKey);
    if (rv != SECSuccess) {
	fprintf(stderr, "RSA Populate failed: pubExp privExp q\n");
	fprintf(stderr, " - not fatal\n");
	/* it's possible that we can't uniquely determine the original key
	 * from just the exponents and prime. Populate returns an error rather
	 * than return the wrong key. */
    } else if (!rsaPrivKeysAreEqual(&tstKey, srcKey)) {
	/* if we returned a key, it *must* be correct */
	fprintf(stderr, "RSA Populate key mismatch: pubExp privExp  q\n");
	rv = RSA_PrivateKeyCheck(&tstKey);
	failed = 1;
    }

    /* test the advanced case2, public exponent, private exponent, modulus */
    rsaPrivKeyReset(&tstKey);

    tstKey.privateExponent = srcKey->privateExponent;
    tstKey.publicExponent = srcKey->publicExponent;
    tstKey.modulus = srcKey->modulus;

    rv = RSA_PopulatePrivateKey(&tstKey);
    if (rv != SECSuccess) {
	fprintf(stderr, "RSA Populate failed: pubExp privExp mod\n");
	failed = 1;
    } else if (!rsaPrivKeysAreEqual(&tstKey, srcKey)) {
	fprintf(stderr, "RSA Populate key mismatch: pubExp privExp  mod\n");
	failed = 1;
    }

    return failed ? -1 : 0;
}



/* bltest commands */
enum {
    cmd_Decrypt = 0,
    cmd_Encrypt,
    cmd_FIPS,
    cmd_Hash,
    cmd_Nonce,
    cmd_Dump,
    cmd_RSAPopulate,
    cmd_Sign,
    cmd_SelfTest,
    cmd_Verify
};

/* bltest options */
enum {
    opt_B64 = 0,
    opt_BufSize,
    opt_Restart,
    opt_SelfTestDir,
    opt_Exponent,
    opt_SigFile,
    opt_KeySize,
    opt_Hex,
    opt_Input,
    opt_PQGFile,
    opt_Key,
    opt_HexWSpc,
    opt_Mode,
#ifdef NSS_ENABLE_ECC
    opt_CurveName,
#endif
    opt_Output,
    opt_Repetitions,
    opt_ZeroBuf,
    opt_Rounds,
    opt_Seed,
    opt_SigSeedFile,
    opt_CXReps,
    opt_IV,
    opt_WordSize,
    opt_UseSeed,
    opt_UseSigSeed,
    opt_SeedFile,
    opt_AAD,
    opt_InputOffset,
    opt_OutputOffset,
    opt_MonteCarlo,
    opt_ThreadNum,
    opt_SecondsToRun,
    opt_CmdLine
};

static secuCommandFlag bltest_commands[] =
{
    { /* cmd_Decrypt	*/ 'D', PR_FALSE, 0, PR_FALSE },
    { /* cmd_Encrypt	*/ 'E', PR_FALSE, 0, PR_FALSE },
    { /* cmd_FIPS	*/ 'F', PR_FALSE, 0, PR_FALSE },
    { /* cmd_Hash	*/ 'H', PR_FALSE, 0, PR_FALSE },
    { /* cmd_Nonce      */ 'N', PR_FALSE, 0, PR_FALSE },
    { /* cmd_Dump	*/ 'P', PR_FALSE, 0, PR_FALSE },
    { /* cmd_RSAPopulate*/ 'R', PR_FALSE, 0, PR_FALSE },
    { /* cmd_Sign	*/ 'S', PR_FALSE, 0, PR_FALSE },
    { /* cmd_SelfTest	*/ 'T', PR_FALSE, 0, PR_FALSE },
    { /* cmd_Verify	*/ 'V', PR_FALSE, 0, PR_FALSE }
};

static secuCommandFlag bltest_options[] =
{
    { /* opt_B64	  */ 'a', PR_FALSE, 0, PR_FALSE },
    { /* opt_BufSize	  */ 'b', PR_TRUE,  0, PR_FALSE },
    { /* opt_Restart	  */ 'c', PR_FALSE, 0, PR_FALSE },
    { /* opt_SelfTestDir  */ 'd', PR_TRUE,  0, PR_FALSE },
    { /* opt_Exponent	  */ 'e', PR_TRUE,  0, PR_FALSE },
    { /* opt_SigFile      */ 'f', PR_TRUE,  0, PR_FALSE },
    { /* opt_KeySize	  */ 'g', PR_TRUE,  0, PR_FALSE },
    { /* opt_Hex	  */ 'h', PR_FALSE, 0, PR_FALSE },
    { /* opt_Input	  */ 'i', PR_TRUE,  0, PR_FALSE },
    { /* opt_PQGFile	  */ 'j', PR_TRUE,  0, PR_FALSE },
    { /* opt_Key	  */ 'k', PR_TRUE,  0, PR_FALSE },
    { /* opt_HexWSpc	  */ 'l', PR_FALSE, 0, PR_FALSE },
    { /* opt_Mode	  */ 'm', PR_TRUE,  0, PR_FALSE },
#ifdef NSS_ENABLE_ECC
    { /* opt_CurveName	  */ 'n', PR_TRUE,  0, PR_FALSE },
#endif
    { /* opt_Output	  */ 'o', PR_TRUE,  0, PR_FALSE },
    { /* opt_Repetitions  */ 'p', PR_TRUE,  0, PR_FALSE },
    { /* opt_ZeroBuf	  */ 'q', PR_FALSE, 0, PR_FALSE },
    { /* opt_Rounds	  */ 'r', PR_TRUE,  0, PR_FALSE },
    { /* opt_Seed	  */ 's', PR_TRUE,  0, PR_FALSE },
    { /* opt_SigSeedFile  */ 't', PR_TRUE,  0, PR_FALSE },
    { /* opt_CXReps       */ 'u', PR_TRUE,  0, PR_FALSE },
    { /* opt_IV		  */ 'v', PR_TRUE,  0, PR_FALSE },
    { /* opt_WordSize	  */ 'w', PR_TRUE,  0, PR_FALSE },
    { /* opt_UseSeed	  */ 'x', PR_FALSE, 0, PR_FALSE },
    { /* opt_UseSigSeed	  */ 'y', PR_FALSE, 0, PR_FALSE },
    { /* opt_SeedFile	  */ 'z', PR_FALSE, 0, PR_FALSE },
    { /* opt_AAD	  */  0 , PR_TRUE,  0, PR_FALSE, "aad" },
    { /* opt_InputOffset  */ '1', PR_TRUE,  0, PR_FALSE },
    { /* opt_OutputOffset */ '2', PR_TRUE,  0, PR_FALSE },
    { /* opt_MonteCarlo   */ '3', PR_FALSE, 0, PR_FALSE },
    { /* opt_ThreadNum    */ '4', PR_TRUE,  0, PR_FALSE },
    { /* opt_SecondsToRun */ '5', PR_TRUE,  0, PR_FALSE },
    { /* opt_CmdLine	  */ '-', PR_FALSE, 0, PR_FALSE }
};

int main(int argc, char **argv)
{
    char *infileName, *outfileName, *keyfileName, *ivfileName;
    SECStatus rv = SECFailure;

    double              totalTime;
    PRIntervalTime      time1, time2;
    PRFileDesc          *outfile = NULL;
    bltestCipherInfo    *cipherInfoListHead, *cipherInfo;
    bltestIOMode        ioMode;
    int                 bufsize, exponent, curThrdNum;
#ifdef NSS_ENABLE_ECC
    char		*curveName = NULL;
#endif
    int			 i, commandsEntered;
    int			 inoff, outoff;
    int                  threads = 1;

    secuCommand bltest;
    bltest.numCommands = sizeof(bltest_commands) / sizeof(secuCommandFlag);
    bltest.numOptions = sizeof(bltest_options) / sizeof(secuCommandFlag);
    bltest.commands = bltest_commands;
    bltest.options = bltest_options;

    progName = strrchr(argv[0], '/');
    if (!progName) 
	progName = strrchr(argv[0], '\\');
    progName = progName ? progName+1 : argv[0];

    rv = NSS_InitializePRErrorTable();
    if (rv != SECSuccess) {
	SECU_PrintPRandOSError(progName);
	return -1;
    }
    rv = RNG_RNGInit();
    if (rv != SECSuccess) {
	SECU_PrintPRandOSError(progName);
	return -1;
    }
    rv = BL_Init();
    if (rv != SECSuccess) {
	SECU_PrintPRandOSError(progName);
	return -1;
    }
    RNG_SystemInfoForRNG();


    rv = SECU_ParseCommandLine(argc, argv, progName, &bltest);
    if (rv == SECFailure) {
	fprintf(stderr, "%s: command line parsing error!\n", progName);
	goto print_usage;
    }
    rv = SECFailure;

    cipherInfo = PORT_ZNew(bltestCipherInfo);
    cipherInfoListHead = cipherInfo;
    /* set some defaults */
    infileName = outfileName = keyfileName = ivfileName = NULL;

    /* Check the number of commands entered on the command line. */
    commandsEntered = 0;
    for (i=0; i<bltest.numCommands; i++)
	if (bltest.commands[i].activated)
	    commandsEntered++;

    if (commandsEntered > 1 &&
	!(commandsEntered == 2 && bltest.commands[cmd_SelfTest].activated)) {
	fprintf(stderr, "%s: one command at a time!\n", progName);
        goto print_usage;
    }

    if (commandsEntered == 0) {
	fprintf(stderr, "%s: you must enter a command!\n", progName);
        goto print_usage;
    }


    if (bltest.commands[cmd_Sign].activated)
	bltest.commands[cmd_Encrypt].activated = PR_TRUE;
    if (bltest.commands[cmd_Verify].activated)
	bltest.commands[cmd_Decrypt].activated = PR_TRUE;
    if (bltest.commands[cmd_Hash].activated)
	bltest.commands[cmd_Encrypt].activated = PR_TRUE;

    inoff = outoff = 0;
    if (bltest.options[opt_InputOffset].activated)
	inoff = PORT_Atoi(bltest.options[opt_InputOffset].arg);
    if (bltest.options[opt_OutputOffset].activated)
	outoff = PORT_Atoi(bltest.options[opt_OutputOffset].arg);

    testdir = (bltest.options[opt_SelfTestDir].activated) ? 
                 strdup(bltest.options[opt_SelfTestDir].arg) : ".";

    /*
     * Handle three simple cases first
     */

    /* test the RSA_PopulatePrivateKey function */
    if (bltest.commands[cmd_RSAPopulate].activated) {
	unsigned int keySize = 1024;
	unsigned long exponent = 65537;
	int rounds = 1;
	int ret;
	
	if (bltest.options[opt_KeySize].activated) {
	    keySize = PORT_Atoi(bltest.options[opt_KeySize].arg);
	}
	if (bltest.options[opt_Rounds].activated) {
	    rounds = PORT_Atoi(bltest.options[opt_Rounds].arg);
	}
	if (bltest.options[opt_Exponent].activated) {
	    exponent = PORT_Atoi(bltest.options[opt_Exponent].arg);
	}

	for (i=0; i < rounds; i++) {
	    printf("Running RSA Populate test round %d\n",i);
	    ret = doRSAPopulateTest(keySize,exponent);
	    if (ret != 0) {
		break;
	    }
	}
	if (ret != 0) {
	    fprintf(stderr,"RSA Populate test round %d: FAILED\n",i);
	}
	return ret;
    }

    /* Do BLAPI self-test */
    if (bltest.commands[cmd_SelfTest].activated) {
	PRBool encrypt = PR_TRUE, decrypt = PR_TRUE;
	/* user may specified a set of ciphers to test.  parse them. */
	bltestCipherMode modesToTest[NUMMODES];
	int numModesToTest = 0;
	char *tok, *str;
	str = bltest.options[opt_Mode].arg;
	while (str) {
	    tok = strchr(str, ',');
	    if (tok) *tok = '\0';
	    modesToTest[numModesToTest++] = get_mode(str);
	    if (tok) {
		*tok = ',';
		str = tok + 1;
	    } else {
		break;
	    }
	}
	if (bltest.commands[cmd_Decrypt].activated &&
	    !bltest.commands[cmd_Encrypt].activated)
	    encrypt = PR_FALSE;
	if (bltest.commands[cmd_Encrypt].activated &&
	    !bltest.commands[cmd_Decrypt].activated)
	    decrypt = PR_FALSE;
	rv = blapi_selftest(modesToTest, numModesToTest, inoff, outoff,
                            encrypt, decrypt);
        PORT_Free(cipherInfo);
        return rv;
    }

    /* Do FIPS self-test */
    if (bltest.commands[cmd_FIPS].activated) {
	CK_RV ckrv = sftk_fipsPowerUpSelfTest();
	fprintf(stdout, "CK_RV: %ld.\n", ckrv);
        PORT_Free(cipherInfo);
        if (ckrv == CKR_OK)
            return SECSuccess;
        return SECFailure;
    }

    /*
     * Check command line arguments for Encrypt/Decrypt/Hash/Sign/Verify
     */

    if ((bltest.commands[cmd_Decrypt].activated ||
	 bltest.commands[cmd_Verify].activated) &&
	bltest.options[opt_BufSize].activated) {
	fprintf(stderr, "%s: Cannot use a nonce as input to decrypt/verify.\n",
			 progName);
        goto print_usage;
    }

    if (bltest.options[opt_Mode].activated) {
	cipherInfo->mode = get_mode(bltest.options[opt_Mode].arg);
	if (cipherInfo->mode == bltestINVALID) {
            goto print_usage;
	}
    } else {
	fprintf(stderr, "%s: You must specify a cipher mode with -m.\n",
			 progName);
        goto print_usage;
    }

    
    if (bltest.options[opt_Repetitions].activated &&
        bltest.options[opt_SecondsToRun].activated) {
        fprintf(stderr, "%s: Operation time should be defined in either "
                "repetitions(-p) or seconds(-5) not both",
                progName);
        goto print_usage;
    }

    if (bltest.options[opt_Repetitions].activated) {
        cipherInfo->repetitionsToPerfom =
            PORT_Atoi(bltest.options[opt_Repetitions].arg);
    } else {
        cipherInfo->repetitionsToPerfom = 0;
    }

    if (bltest.options[opt_SecondsToRun].activated) {
        cipherInfo->seconds = PORT_Atoi(bltest.options[opt_SecondsToRun].arg);
    } else {
        cipherInfo->seconds = 0;
    }


    if (bltest.options[opt_CXReps].activated) {
        cipherInfo->cxreps = PORT_Atoi(bltest.options[opt_CXReps].arg);
    } else {
        cipherInfo->cxreps = 0;
    }

    if (bltest.options[opt_ThreadNum].activated) {
        threads = PORT_Atoi(bltest.options[opt_ThreadNum].arg);
        if (threads <= 0) {
            threads = 1;
        }
    }

    /* Dump a file (rsakey, dsakey, etc.) */
    if (bltest.commands[cmd_Dump].activated) {
        rv = dump_file(cipherInfo->mode, bltest.options[opt_Input].arg);
        PORT_Free(cipherInfo);
        return rv;
    }

    /* default input mode is binary */
    ioMode = (bltest.options[opt_B64].activated)     ? bltestBase64Encoded :
	     (bltest.options[opt_Hex].activated)     ? bltestHexStream :
	     (bltest.options[opt_HexWSpc].activated) ? bltestHexSpaceDelim :
						       bltestBinary;

    if (bltest.options[opt_Exponent].activated)
	exponent = PORT_Atoi(bltest.options[opt_Exponent].arg);
    else
	exponent = 65537;

#ifdef NSS_ENABLE_ECC
    if (bltest.options[opt_CurveName].activated)
	curveName = PORT_Strdup(bltest.options[opt_CurveName].arg);
    else
	curveName = NULL;
#endif

    if (bltest.commands[cmd_Verify].activated &&
        !bltest.options[opt_SigFile].activated) {
        fprintf(stderr, "%s: You must specify a signature file with -f.\n",
                progName);

      print_usage:
        PORT_Free(cipherInfo);
        Usage();
    }

    if (bltest.options[opt_MonteCarlo].activated) {
        cipherInfo->mCarlo = PR_TRUE;
    } else {
        cipherInfo->mCarlo = PR_FALSE;
    }

    for (curThrdNum = 0;curThrdNum < threads;curThrdNum++) {
        int            keysize = 0;
        PRFileDesc     *file = NULL, *infile;
        bltestParams   *params;
        char           *instr = NULL;
        PLArenaPool    *arena;

        if (curThrdNum > 0) {
            bltestCipherInfo *newCInfo = PORT_ZNew(bltestCipherInfo);
            if (!newCInfo) {
                fprintf(stderr, "%s: Can not allocate  memory.\n", progName);
                goto exit_point;
            }
            newCInfo->mode = cipherInfo->mode;
            newCInfo->mCarlo = cipherInfo->mCarlo;
            newCInfo->repetitionsToPerfom =
                cipherInfo->repetitionsToPerfom;
            newCInfo->seconds = cipherInfo->seconds;
            newCInfo->cxreps = cipherInfo->cxreps;
            cipherInfo->next = newCInfo;
            cipherInfo = newCInfo;
        }
        arena = PORT_NewArena(BLTEST_DEFAULT_CHUNKSIZE);
        if (!arena) {
            fprintf(stderr, "%s: Can not allocate memory.\n", progName);
            goto exit_point;
        }
        cipherInfo->arena = arena;
        params = &cipherInfo->params;
        
        /* Set up an encryption key. */
        keysize = 0;
        file = NULL;
        if (is_symmkeyCipher(cipherInfo->mode)) {
            char *keystr = NULL;  /* if key is on command line */
            if (bltest.options[opt_Key].activated) {
                if (bltest.options[opt_CmdLine].activated) {
                    keystr = bltest.options[opt_Key].arg;
                } else {
                    file = PR_Open(bltest.options[opt_Key].arg,
                                   PR_RDONLY, 00660);
                }
            } else {
                if (bltest.options[opt_KeySize].activated)
                    keysize = PORT_Atoi(bltest.options[opt_KeySize].arg);
                else
                    keysize = 8; /* use 64-bit default (DES) */
                /* save the random key for reference */
                file = PR_Open("tmp.key", PR_WRONLY|PR_CREATE_FILE, 00660);
            }
            params->key.mode = ioMode;
            setupIO(cipherInfo->arena, &params->key, file, keystr, keysize);
            if (file)
                PR_Close(file);
        } else if (is_pubkeyCipher(cipherInfo->mode)) {
            if (bltest.options[opt_Key].activated) {
                file = PR_Open(bltest.options[opt_Key].arg, PR_RDONLY, 00660);
            } else {
                if (bltest.options[opt_KeySize].activated)
                    keysize = PORT_Atoi(bltest.options[opt_KeySize].arg);
                else
                    keysize = 64; /* use 512-bit default */
                file = PR_Open("tmp.key", PR_WRONLY|PR_CREATE_FILE, 00660);
            }
            params->key.mode = bltestBase64Encoded;
#ifdef NSS_ENABLE_ECC
            pubkeyInitKey(cipherInfo, file, keysize, exponent, curveName);
#else
            pubkeyInitKey(cipherInfo, file, keysize, exponent);
#endif
            PR_Close(file);
        }

        /* set up an initialization vector. */
        if (cipher_requires_IV(cipherInfo->mode)) {
            char *ivstr = NULL;
            bltestSymmKeyParams *skp;
            file = NULL;
#ifdef NSS_SOFTOKEN_DOES_RC5
            if (cipherInfo->mode == bltestRC5_CBC)
                skp = (bltestSymmKeyParams *)&params->rc5;
            else
#endif
                skp = &params->sk;
            if (bltest.options[opt_IV].activated) {
                if (bltest.options[opt_CmdLine].activated) {
                    ivstr = bltest.options[opt_IV].arg;
                } else {
                    file = PR_Open(bltest.options[opt_IV].arg,
                                   PR_RDONLY, 00660);
                }
            } else {
                /* save the random iv for reference */
                file = PR_Open("tmp.iv", PR_WRONLY|PR_CREATE_FILE, 00660);
            }
            memset(&skp->iv, 0, sizeof skp->iv);
            skp->iv.mode = ioMode;
            setupIO(cipherInfo->arena, &skp->iv, file, ivstr, keysize);
            if (file) {
                PR_Close(file);
            }
        }

        /* set up an initialization vector. */
        if (is_authCipher(cipherInfo->mode)) {
            char *aadstr = NULL;
            bltestAuthSymmKeyParams *askp;
            file = NULL;
            askp = &params->ask;
            if (bltest.options[opt_AAD].activated) {
                if (bltest.options[opt_CmdLine].activated) {
                    aadstr = bltest.options[opt_AAD].arg;
                } else {
                    file = PR_Open(bltest.options[opt_AAD].arg,
                                   PR_RDONLY, 00660);
                }
            } else {
                file = NULL;
            }
            memset(&askp->aad, 0, sizeof askp->aad);
            askp->aad.mode = ioMode;
            setupIO(cipherInfo->arena, &askp->aad, file, aadstr, 0);
            if (file) {
                PR_Close(file);
            }
        }
        
        if (bltest.commands[cmd_Verify].activated) {
            file = PR_Open(bltest.options[opt_SigFile].arg, PR_RDONLY, 00660);
            if (cipherInfo->mode == bltestDSA) {
                memset(&cipherInfo->params.dsa.sig, 0, sizeof(bltestIO));
                cipherInfo->params.dsa.sig.mode = ioMode;
                setupIO(cipherInfo->arena, &cipherInfo->params.dsa.sig,
                        file, NULL, 0);
#ifdef NSS_ENABLE_ECC
            } else if (cipherInfo->mode == bltestECDSA) {
                memset(&cipherInfo->params.ecdsa.sig, 0, sizeof(bltestIO));
                cipherInfo->params.ecdsa.sig.mode = ioMode;
                setupIO(cipherInfo->arena, &cipherInfo->params.ecdsa.sig,
                        file, NULL, 0);
#endif
            }
            if (file) {
                PR_Close(file);
            }
        }
        
        if (bltest.options[opt_PQGFile].activated) {
            file = PR_Open(bltest.options[opt_PQGFile].arg, PR_RDONLY, 00660);
            params->dsa.pqgdata.mode = bltestBase64Encoded;
            setupIO(cipherInfo->arena, &params->dsa.pqgdata, file, NULL, 0);
            if (file) {
                PR_Close(file);
            }
        }

        /* Set up the input buffer */
        if (bltest.options[opt_Input].activated) {
            if (bltest.options[opt_CmdLine].activated) {
                instr = bltest.options[opt_Input].arg;
                infile = NULL;
            } else {
                /* form file name from testdir and input arg. */
                char * filename = bltest.options[opt_Input].arg;
                if (bltest.options[opt_SelfTestDir].activated && 
                    testdir && filename && filename[0] != '/') {
                    filename = PR_smprintf("%s/tests/%s/%s", testdir, 
                                           mode_strings[cipherInfo->mode],
                                           filename);
                    if (!filename) {
                        fprintf(stderr, "%s: Can not allocate memory.\n",
                                progName);
                        goto exit_point;
                    }
                    infile = PR_Open(filename, PR_RDONLY, 00660);
                    PR_smprintf_free(filename);
                } else {
                    infile = PR_Open(filename, PR_RDONLY, 00660);
                }
            }
        } else if (bltest.options[opt_BufSize].activated) {
            /* save the random plaintext for reference */
            char *tmpFName = PR_smprintf("tmp.in.%d", curThrdNum);
            if (!tmpFName) {
                fprintf(stderr, "%s: Can not allocate memory.\n", progName);
                goto exit_point;
            }
            infile = PR_Open(tmpFName, PR_WRONLY|PR_CREATE_FILE, 00660);
            PR_smprintf_free(tmpFName);
        } else {
            infile = PR_STDIN;
        }
        if (!infile) {
            fprintf(stderr, "%s: Failed to open input file.\n", progName);
            goto exit_point;
        }
        cipherInfo->input.mode = ioMode;

        /* Set up the output stream */
        if (bltest.options[opt_Output].activated) {
            /* form file name from testdir and input arg. */
            char * filename = bltest.options[opt_Output].arg;
            if (bltest.options[opt_SelfTestDir].activated && 
                testdir && filename && filename[0] != '/') {
                filename = PR_smprintf("%s/tests/%s/%s", testdir, 
                                       mode_strings[cipherInfo->mode],
                                       filename);
                if (!filename) {
                    fprintf(stderr, "%s: Can not allocate memory.\n", progName);
                    goto exit_point;
                }
                outfile = PR_Open(filename, PR_WRONLY|PR_CREATE_FILE, 00660);
                PR_smprintf_free(filename);
            } else {
                outfile = PR_Open(filename, PR_WRONLY|PR_CREATE_FILE, 00660);
            }
        } else {
            outfile = PR_STDOUT;
        }
        if (!outfile) {
            fprintf(stderr, "%s: Failed to open output file.\n", progName);
            rv = SECFailure;
            goto exit_point;
        }
        cipherInfo->output.mode = ioMode;
        if (bltest.options[opt_SelfTestDir].activated && ioMode == bltestBinary)
            cipherInfo->output.mode = bltestBase64Encoded;

        if (is_hashCipher(cipherInfo->mode))
            cipherInfo->params.hash.restart =
                bltest.options[opt_Restart].activated;

        bufsize = 0;
        if (bltest.options[opt_BufSize].activated)
            bufsize = PORT_Atoi(bltest.options[opt_BufSize].arg);

        /*infile = NULL;*/
        setupIO(cipherInfo->arena, &cipherInfo->input, infile, instr, bufsize);
        if (infile && infile != PR_STDIN)
            PR_Close(infile);
        misalignBuffer(cipherInfo->arena, &cipherInfo->input, inoff);

        cipherInit(cipherInfo, bltest.commands[cmd_Encrypt].activated);
        misalignBuffer(cipherInfo->arena, &cipherInfo->output, outoff);
    }

    if (!bltest.commands[cmd_Nonce].activated) {
        TIMESTART();
        cipherInfo = cipherInfoListHead;
        while (cipherInfo != NULL) {
            cipherInfo->cipherThread = 
                PR_CreateThread(PR_USER_THREAD,
                                    ThreadExecTest,
                                    cipherInfo,
                                    PR_PRIORITY_NORMAL,
                                    PR_GLOBAL_THREAD,
                                    PR_JOINABLE_THREAD,
                                    0);
            cipherInfo = cipherInfo->next;
        } 

        cipherInfo = cipherInfoListHead;
        while (cipherInfo != NULL) {
            PR_JoinThread(cipherInfo->cipherThread);
            finishIO(&cipherInfo->output, outfile);
            cipherInfo = cipherInfo->next;
        }
        TIMEFINISH(totalTime, 1);
    }
    
    cipherInfo = cipherInfoListHead;
    if (cipherInfo->repetitions > 0 || cipherInfo->cxreps > 0 ||
        threads > 1)
        dump_performance_info(cipherInfoListHead, totalTime,
                              bltest.commands[cmd_Encrypt].activated,
	                      (cipherInfo->repetitions == 0));
    
    rv = SECSuccess;

  exit_point:
    if (outfile && outfile != PR_STDOUT)
	PR_Close(outfile);
    cipherInfo = cipherInfoListHead;
    while (cipherInfo != NULL) {
        bltestCipherInfo *tmpInfo = cipherInfo;

        if (cipherInfo->arena)
            PORT_FreeArena(cipherInfo->arena, PR_TRUE);
        cipherInfo = cipherInfo->next;
        PORT_Free(tmpInfo);
    }

    /*NSS_Shutdown();*/

    return SECSuccess;
}

