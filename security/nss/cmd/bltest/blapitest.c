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
 * Communications Corporation.	Portions created by Netscape are
 * Copyright (C) 1994-2000 Netscape Communications Corporation.	 All
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

#include <stdio.h>
#include <stdlib.h>

#include "blapi.h"
#include "prmem.h"
#include "prprf.h"
#include "prtime.h"
#include "prsystem.h"
#include "plstr.h"
#include "nssb64.h"
#include "secutil.h"
#include "plgetopt.h"
#include "softoken.h"
#include "nss.h"

/* Temporary - add debugging ouput on windows for RSA to track QA failure */
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
                   prerror, SECU_Strerror(prerror), ln); \
	exit(-1); \
    }

/* Macros for performance timing. */
#define TIMESTART() \
    time1 = PR_IntervalNow();

#define TIMEFINISH(time, reps) \
    time2 = (PRIntervalTime)(PR_IntervalNow() - time1); \
    time1 = PR_IntervalToMilliseconds(time2); \
    time = ((double)(time1))/reps;

static void Usage()
{
#define PRINTUSAGE(subject, option, predicate) \
    fprintf(stderr, "%10s %s\t%s\n", subject, option, predicate);
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "[-DEHSV]", "List available cipher modes"); /* XXX */
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-E -m mode ", "Encrypt a buffer");
    PRINTUSAGE("",	"", "[-i plaintext] [-o ciphertext] [-k key] [-v iv]");
    PRINTUSAGE("",	"", "[-b bufsize] [-g keysize] [-e exp] [-r rounds]");
    PRINTUSAGE("",	"", "[-w wordsize] [-p repetitions]");
    PRINTUSAGE("",	"-m", "cipher mode to use");
    PRINTUSAGE("",	"-i", "file which contains input buffer");
    PRINTUSAGE("",	"-o", "file for output buffer");
    PRINTUSAGE("",	"-k", "file which contains key");
    PRINTUSAGE("",	"-v", "file which contains initialization vector");
    PRINTUSAGE("",	"-b", "size of input buffer");
    PRINTUSAGE("",	"-g", "key size (in bytes)");
    PRINTUSAGE("",	"-p", "do performance test");
    PRINTUSAGE("(rsa)", "-e", "rsa public exponent");
    PRINTUSAGE("(rc5)", "-r", "number of rounds");
    PRINTUSAGE("(rc5)", "-w", "wordsize (32 or 64)");
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-D -m mode", "Decrypt a buffer");
    PRINTUSAGE("",	"", "[-i plaintext] [-o ciphertext] [-k key] [-v iv]");
    PRINTUSAGE("",	"", "[-p repetitions]");
    PRINTUSAGE("",	"-m", "cipher mode to use");
    PRINTUSAGE("",	"-i", "file which contains input buffer");
    PRINTUSAGE("",	"-o", "file for output buffer");
    PRINTUSAGE("",	"-k", "file which contains key");
    PRINTUSAGE("",	"-v", "file which contains initialization vector");
    PRINTUSAGE("",	"-p", "do performance test");
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-H -m mode", "Hash a buffer");
    PRINTUSAGE("",	"", "[-i plaintext] [-o hash]");
    PRINTUSAGE("",	"", "[-b bufsize]");
    PRINTUSAGE("",	"", "[-p repetitions]");
    PRINTUSAGE("",	"-m", "cipher mode to use");
    PRINTUSAGE("",	"-i", "file which contains input buffer");
    PRINTUSAGE("",	"-o", "file for hash");
    PRINTUSAGE("",	"-b", "size of input buffer");
    PRINTUSAGE("",	"-p", "do performance test");
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-S -m mode", "Sign a buffer");
    PRINTUSAGE("",	"", "[-i plaintext] [-o signature] [-k key]");
    PRINTUSAGE("",	"", "[-b bufsize]");
    PRINTUSAGE("",	"", "[-p repetitions]");
    PRINTUSAGE("",	"-m", "cipher mode to use");
    PRINTUSAGE("",	"-i", "file which contains input buffer");
    PRINTUSAGE("",	"-o", "file for signature");
    PRINTUSAGE("",	"-k", "file which contains key");
    PRINTUSAGE("",	"-p", "do performance test");
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-V -m mode", "Verify a signed buffer");
    PRINTUSAGE("",	"", "[-i plaintext] [-s signature] [-k key]");
    PRINTUSAGE("",	"", "[-p repetitions]");
    PRINTUSAGE("",	"-m", "cipher mode to use");
    PRINTUSAGE("",	"-i", "file which contains input buffer");
    PRINTUSAGE("",	"-s", "file which contains signature of input buffer");
    PRINTUSAGE("",	"-k", "file which contains key");
    PRINTUSAGE("",	"-p", "do performance test");
    fprintf(stderr, "\n");
    PRINTUSAGE(progName, "-N -m mode -b bufsize", 
                                            "Create a nonce plaintext and key");
    PRINTUSAGE("",      "", "[-g keysize] [-u cxreps]");
    PRINTUSAGE("",	"-g", "key size (in bytes)");
    PRINTUSAGE("",      "-u", "number of repetitions of context creation");
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
    PRArenaPool *arena;
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
atob(SECItem *ascii, SECItem *binary, PRArenaPool *arena)
{
    SECStatus status;
    NSSBase64Decoder *cx;
    struct item_with_arena it;
    int len;
    binary->data = NULL;
    binary->len = 0;
    it.item = binary;
    it.arena = arena;
    len = (strcmp(&ascii->data[ascii->len-2],"\r\n")) ? 
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
    SECItem ascii;
    ascii.data = NULL;
    ascii.len = 0;
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
char2_from_hex(unsigned char byteval, unsigned char *c2)
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
    SECItem ascii;
    ascii.data = NULL;
    ascii.len = 0;
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
key_from_filedata(PRArenaPool *arena, SECItem *it, int ni, SECItem *filedata)
{
    int fpos = 0;
    int i;
    unsigned char *buf = filedata->data;
    for (i=0; i<ni; i++, it++) {
	it->len	 = (buf[fpos++] & 0xff) << 24;
	it->len |= (buf[fpos++] & 0xff) << 16;
	it->len |= (buf[fpos++] & 0xff) <<  8;
	it->len |= (buf[fpos++] & 0xff);
	if (it->len > 0) {
	    it->data = PORT_ArenaAlloc(arena, it->len);
	    PORT_Memcpy(it->data, &buf[fpos], it->len);
	} else {
	    it->data = NULL;
	}
	fpos += it->len;
    }
}

static RSAPrivateKey *
rsakey_from_filedata(SECItem *filedata)
{
    RSAPrivateKey *key;
    PRArenaPool *arena;
    arena = PORT_NewArena(BLTEST_DEFAULT_CHUNKSIZE);
    key = (RSAPrivateKey *)PORT_ArenaZAlloc(arena, sizeof(RSAPrivateKey));
    key->arena = arena;
    key_from_filedata(arena, &key->version, 9, filedata);
    return key;
}

static PQGParams *
pqg_from_filedata(SECItem *filedata)
{
    PQGParams *pqg;
    PRArenaPool *arena;
    arena = PORT_NewArena(BLTEST_DEFAULT_CHUNKSIZE);
    pqg = (PQGParams *)PORT_ArenaZAlloc(arena, sizeof(PQGParams));
    pqg->arena = arena;
    key_from_filedata(arena, &pqg->prime, 3, filedata);
    return pqg;
}

static DSAPrivateKey *
dsakey_from_filedata(SECItem *filedata)
{
    DSAPrivateKey *key;
    PRArenaPool *arena;
    arena = PORT_NewArena(BLTEST_DEFAULT_CHUNKSIZE);
    key = (DSAPrivateKey *)PORT_ArenaZAlloc(arena, sizeof(DSAPrivateKey));
    key->params.arena = arena;
    key_from_filedata(arena, &key->params.prime, 5, filedata);
    return key;
}

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
					   unsigned char *output,
					   const unsigned char *input);

typedef SECStatus (* bltestHashCipherFn)(unsigned char *dest,
					 const unsigned char *src,
					 uint32 src_length);

typedef enum {
    bltestINVALID = -1,
    bltestDES_ECB,	  /* Symmetric Key Ciphers */
    bltestDES_CBC,	  /* .			   */
    bltestDES_EDE_ECB,	  /* .			   */
    bltestDES_EDE_CBC,	  /* .			   */
    bltestRC2_ECB,	  /* .			   */
    bltestRC2_CBC,	  /* .			   */
    bltestRC4,		  /* .			   */
    bltestRC5_ECB,	  /* .			   */
    bltestRC5_CBC,	  /* .			   */
    bltestAES_ECB,        /* .                     */
    bltestAES_CBC,        /* .                     */
    bltestRSA,		  /* Public Key Ciphers	   */
    bltestDSA,		  /* . (Public Key Sig.)   */
    bltestMD2,		  /* Hash algorithms	   */
    bltestMD5,		  /* .			   */
    bltestSHA1,           /* .			   */
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
    "rc5_ecb",
    "rc5_cbc",
    "aes_ecb",
    "aes_cbc",
    "rsa",
    /*"pqg",*/
    "dsa",
    "md2",
    "md5",
    "sha1",
};

typedef struct
{
    bltestIO key;
    bltestIO iv;
} bltestSymmKeyParams;

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
    unsigned int j;
    bltestIO   keyseed;
    bltestIO   sigseed;
    bltestIO   sig; /* if doing verify, have additional input */
    PQGParams *pqg;
    DSAPrivateKey *dsakey;
} bltestDSAParams;

typedef struct
{
    bltestIO   key; /* unused */
    PRBool     restart;
} bltestHashParams;

typedef union
{
    bltestIO		key;
    bltestSymmKeyParams sk;
    bltestRC5Params	rc5;
    bltestRSAParams	rsa;
    bltestDSAParams	dsa;
    bltestHashParams	hash;
} bltestParams;

typedef struct
{
    PRArenaPool *arena;
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
    int	  repetitions;
    int   cxreps;
    double cxtime;
    double optime;
} bltestCipherInfo;

PRBool
is_symmkeyCipher(bltestCipherMode mode)
{
    /* change as needed! */
    if (mode >= bltestDES_ECB && mode <= bltestAES_CBC)
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
    if (mode >= bltestMD2 && mode <= bltestSHA1)
	return PR_TRUE;
    return PR_FALSE;
}

PRBool
is_sigCipher(bltestCipherMode mode)
{
    /* change as needed! */
    if (mode >= bltestDSA && mode <= bltestDSA)
	return PR_TRUE;
    return PR_FALSE;
}

PRBool
cipher_requires_IV(bltestCipherMode mode)
{
    /* change as needed! */
    if (mode == bltestDES_CBC || mode == bltestDES_EDE_CBC ||
	mode == bltestRC2_CBC || mode == bltestRC5_CBC     ||
        mode == bltestAES_CBC)
	return PR_TRUE;
    return PR_FALSE;
}

SECStatus finishIO(bltestIO *output, PRFileDesc *file);

SECStatus
setupIO(PRArenaPool *arena, bltestIO *input, PRFileDesc *file,
	char *str, int numBytes)
{
    SECStatus rv;
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
	fileData.data = str;
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
	rv = atob(in, &input->buf, arena);
	break;
    case bltestBinary:
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
    SECStatus rv;
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
bltestCopyIO(PRArenaPool *arena, bltestIO *dest, bltestIO *src)
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
misalignBuffer(PRArenaPool *arena, bltestIO *io, int off)
{
    ptrdiff_t offset = (ptrdiff_t)io->buf.data % WORDSIZE;
    int length = io->buf.len;
    if (offset != off) {
	SECITEM_ReallocItem(arena, &io->buf, length, length + 2*WORDSIZE);
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
rsa_PublicKeyOp(void *key, unsigned char *output, const unsigned char *input)
{
    return RSA_PublicKeyOp((RSAPublicKey *)key, output, input);
}

SECStatus
rsa_PrivateKeyOp(void *key, unsigned char *output, const unsigned char *input)
{
    return RSA_PrivateKeyOp((RSAPrivateKey *)key, output, input);
}

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
#if NSS_SOFTOKEN_DOES_RC5
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
    PRIntervalTime time1, time2;
    bltestSymmKeyParams *aesp = &cipherInfo->params.sk;
    int minorMode;
    int i;
    /* XXX */ int keylen, blocklen;
    keylen = aesp->key.buf.len;
    blocklen = cipherInfo->input.buf.len;
    switch (cipherInfo->mode) {
    case bltestAES_ECB:	    minorMode = NSS_AES;	  break;
    case bltestAES_CBC:	    minorMode = NSS_AES_CBC;	  break;
    default:
	return SECFailure;
    }
    cipherInfo->cx = (void*)AES_CreateContext(aesp->key.buf.data,
					      aesp->iv.buf.data,
					      minorMode, encrypt, 
                                              keylen, blocklen);
    if (cipherInfo->cxreps > 0) {
	AESContext **dummycx;
	dummycx = PORT_Alloc(cipherInfo->cxreps * sizeof(AESContext *));
	TIMESTART();
	for (i=0; i<cipherInfo->cxreps; i++) {
	    dummycx[i] = (void*)AES_CreateContext(aesp->key.buf.data,
					          aesp->iv.buf.data,
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
bltest_pqg_init(bltestDSAParams *dsap)
{
    SECStatus rv, res;
    PQGVerify *vfy = NULL;
    rv = PQG_ParamGen(dsap->j, &dsap->pqg, &vfy);
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
	    PQG_ParamGen(dsap->j, &dummypqg, &ignore);
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
	cipherInfo->cipher.pubkeyCipher = DSA_SignDigest;
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
	cipherInfo->cipher.pubkeyCipher = DSA_VerifyDigest;
    }
    return SECSuccess;
}

/* XXX unfortunately, this is not defined in blapi.h */
SECStatus
md2_HashBuf(unsigned char *dest, const unsigned char *src, uint32 src_length)
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
md2_restart(unsigned char *dest, const unsigned char *src, uint32 src_length)
{
    MD2Context *cx, *cx_cpy;
    unsigned char *cxbytes;
    unsigned int len;
    unsigned int i, quarter;
    SECStatus rv;
    cx = MD2_NewContext();
    /* divide message by 4, restarting 3 times */
    quarter = src_length / 4;
    for (i=0; i<4; i++) {
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
	    PR_fprintf(PR_STDERR, "%s: MD2_Resurrect failed!\n", progName);
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
md5_restart(unsigned char *dest, const unsigned char *src, uint32 src_length)
{
    SECStatus rv;
    MD5Context *cx, *cx_cpy;
    unsigned char *cxbytes;
    unsigned int len;
    unsigned int i, quarter;
    cx = MD5_NewContext();
    /* divide message by 4, restarting 3 times */
    quarter = src_length / 4;
    for (i=0; i<4; i++) {
	MD5_Update(cx, src + i*quarter, PR_MIN(quarter, src_length));
	len = MD5_FlattenSize(cx);
	cxbytes = PORT_Alloc(len);
	MD5_Flatten(cx, cxbytes);
	cx_cpy = MD5_Resurrect(cxbytes, NULL);
	if (!cx_cpy) {
	    PR_fprintf(PR_STDERR, "%s: MD5_Resurrect failed!\n", progName);
	    goto finish;
	}
	rv = PORT_Memcmp(cx, cx_cpy, len);
	if (rv) {
	    MD5_DestroyContext(cx_cpy, PR_TRUE);
	    PR_fprintf(PR_STDERR, "%s: MD5_Resurrect failed!\n", progName);
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
sha1_restart(unsigned char *dest, const unsigned char *src, uint32 src_length)
{
    SECStatus rv;
    SHA1Context *cx, *cx_cpy;
    unsigned char *cxbytes;
    unsigned int len;
    unsigned int i, quarter;
    cx = SHA1_NewContext();
    /* divide message by 4, restarting 3 times */
    quarter = src_length / 4;
    for (i=0; i<4; i++) {
	SHA1_Update(cx, src + i*quarter, PR_MIN(quarter, src_length));
	len = SHA1_FlattenSize(cx);
	cxbytes = PORT_Alloc(len);
	SHA1_Flatten(cx, cxbytes);
	cx_cpy = SHA1_Resurrect(cxbytes, NULL);
	if (!cx_cpy) {
	    PR_fprintf(PR_STDERR, "%s: SHA1_Resurrect failed!\n", progName);
	    goto finish;
	}
	rv = PORT_Memcmp(cx, cx_cpy, len);
	if (rv) {
	    SHA1_DestroyContext(cx_cpy, PR_TRUE);
	    PR_fprintf(PR_STDERR, "%s: SHA1_Resurrect failed!\n", progName);
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
pubkeyInitKey(bltestCipherInfo *cipherInfo, PRFileDesc *file,
	      int keysize, int exponent)
{
    int i;
    SECStatus rv;
    bltestRSAParams *rsap;
    bltestDSAParams *dsap;
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
	    dsap->j = PQG_PBITS_TO_INDEX(8*keysize);
	    if (!dsap->pqg)
		bltest_pqg_init(dsap);
	    rv = DSA_NewKey(dsap->pqg, &dsap->dsakey);
	    CHECKERROR(rv, __LINE__);
	    serialize_key(&dsap->dsakey->params.prime, 5, file);
	} else {
	    setupIO(cipherInfo->arena, &cipherInfo->params.key, file, NULL, 0);
	    dsap->dsakey = dsakey_from_filedata(&cipherInfo->params.key.buf);
	    dsap->j = PQG_PBITS_TO_INDEX(8*dsap->dsakey->params.prime.len);
	}
	break;
    default:
	return SECFailure;
    }
    return SECSuccess;
}

SECStatus
cipherInit(bltestCipherInfo *cipherInfo, PRBool encrypt)
{
    PRBool restart;
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
    case bltestRC5_ECB:
    case bltestRC5_CBC:
#if NSS_SOFTOKEN_DOES_RC5
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  cipherInfo->input.pBuf.len);
#endif
	return bltest_rc5_init(cipherInfo, encrypt);
	break;
    case bltestAES_ECB:
    case bltestAES_CBC:
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  cipherInfo->input.pBuf.len);
	return bltest_aes_init(cipherInfo, encrypt);
	break;
    case bltestRSA:
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  cipherInfo->input.pBuf.len);
	return bltest_rsa_init(cipherInfo, encrypt);
	break;
    case bltestDSA:
	SECITEM_AllocItem(cipherInfo->arena, &cipherInfo->output.buf,
			  DSA_SIGNATURE_LEN);
	return bltest_dsa_init(cipherInfo, encrypt);
	break;
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
    default:
	return SECFailure;
    }
    return SECSuccess;
}

SECStatus
dsaOp(bltestCipherInfo *cipherInfo)
{
    PRIntervalTime time1, time2;
    SECStatus rv;
    int i;
    int maxLen = cipherInfo->output.pBuf.len;
    SECItem dummyOut = { 0, 0, 0 };
    SECITEM_AllocItem(NULL, &dummyOut, maxLen);
    if (cipherInfo->cipher.pubkeyCipher == 
             (bltestPubKeyCipherFn)DSA_SignDigest) {
	if (cipherInfo->params.dsa.sigseed.buf.len > 0) {
	    rv = DSA_SignDigestWithSeed((DSAPrivateKey *)cipherInfo->cx,
				       &cipherInfo->output.pBuf,
				       &cipherInfo->input.pBuf,
				       cipherInfo->params.dsa.sigseed.buf.data);
	    CHECKERROR(rv, __LINE__);
	    TIMESTART();
	    for (i=0; i<cipherInfo->repetitions; i++) {
		rv |= DSA_SignDigestWithSeed((DSAPrivateKey *)cipherInfo->cx,
				       &dummyOut,
				       &cipherInfo->input.pBuf,
				       cipherInfo->params.dsa.sigseed.buf.data);
	    }
	    TIMEFINISH(cipherInfo->optime, 1.0);
	    CHECKERROR(rv, __LINE__);
	} else {
	    rv = DSA_SignDigest((DSAPrivateKey *)cipherInfo->cx,
				&cipherInfo->output.pBuf,
				&cipherInfo->input.pBuf);
	    CHECKERROR(rv, __LINE__);
	    TIMESTART();
	    for (i=0; i<cipherInfo->repetitions; i++) {
		DSA_SignDigest((DSAPrivateKey *)cipherInfo->cx, &dummyOut,
			       &cipherInfo->input.pBuf);
	    }
	    TIMEFINISH(cipherInfo->optime, 1.0);
	}
	bltestCopyIO(cipherInfo->arena, &cipherInfo->params.dsa.sig, 
	             &cipherInfo->output);
    } else {
	rv = DSA_VerifyDigest((DSAPublicKey *)cipherInfo->cx,
			      &cipherInfo->params.dsa.sig.buf,
			      &cipherInfo->input.pBuf);
	CHECKERROR(rv, __LINE__);
	TIMESTART();
	for (i=0; i<cipherInfo->repetitions; i++) {
	    DSA_VerifyDigest((DSAPublicKey *)cipherInfo->cx,
			     &cipherInfo->params.dsa.sig.buf,
			     &cipherInfo->input.pBuf);
	}
	TIMEFINISH(cipherInfo->optime, 1.0);
    }
    SECITEM_FreeItem(&dummyOut, PR_FALSE);
    return rv;
}

SECStatus
cipherDoOp(bltestCipherInfo *cipherInfo)
{
    PRIntervalTime time1, time2;
    SECStatus rv;
    int i, len;
    int maxLen = cipherInfo->output.pBuf.len;
    unsigned char *dummyOut;
    if (cipherInfo->mode == bltestDSA)
	return dsaOp(cipherInfo);
    dummyOut = PORT_Alloc(maxLen);
    if (is_symmkeyCipher(cipherInfo->mode)) {
	rv = (*cipherInfo->cipher.symmkeyCipher)(cipherInfo->cx,
						 cipherInfo->output.pBuf.data,
						 &len, maxLen,
						 cipherInfo->input.pBuf.data,
						 cipherInfo->input.pBuf.len);
	TIMESTART();
	for (i=0; i<cipherInfo->repetitions; i++) {
	    (*cipherInfo->cipher.symmkeyCipher)(cipherInfo->cx, dummyOut,
						&len, maxLen,
						cipherInfo->input.pBuf.data,
						cipherInfo->input.pBuf.len);

	}
	TIMEFINISH(cipherInfo->optime, 1.0);
    } else if (is_pubkeyCipher(cipherInfo->mode)) {
	rv = (*cipherInfo->cipher.pubkeyCipher)(cipherInfo->cx,
						cipherInfo->output.pBuf.data,
						cipherInfo->input.pBuf.data);
	TIMESTART();
	for (i=0; i<cipherInfo->repetitions; i++) {
	    (*cipherInfo->cipher.pubkeyCipher)(cipherInfo->cx, dummyOut,
					       cipherInfo->input.pBuf.data);
	}
	TIMEFINISH(cipherInfo->optime, 1.0);
    } else if (is_hashCipher(cipherInfo->mode)) {
	rv = (*cipherInfo->cipher.hashCipher)(cipherInfo->output.pBuf.data,
					      cipherInfo->input.pBuf.data,
					      cipherInfo->input.pBuf.len);
	TIMESTART();
	for (i=0; i<cipherInfo->repetitions; i++) {
	    (*cipherInfo->cipher.hashCipher)(dummyOut,
					     cipherInfo->input.pBuf.data,
					     cipherInfo->input.pBuf.len);
	}
	TIMEFINISH(cipherInfo->optime, 1.0);
    }
    PORT_Free(dummyOut);
    return rv;
}

SECStatus
cipherFinish(bltestCipherInfo *cipherInfo)
{
    switch (cipherInfo->mode) {
    case bltestDES_ECB:
    case bltestDES_CBC:
    case bltestDES_EDE_ECB:
    case bltestDES_EDE_CBC:
	DES_DestroyContext((DESContext *)cipherInfo->cx, PR_TRUE);
	break;
    case bltestRC2_ECB:
    case bltestRC2_CBC:
	RC2_DestroyContext((RC2Context *)cipherInfo->cx, PR_TRUE);
	break;
    case bltestRC4:
	RC4_DestroyContext((RC4Context *)cipherInfo->cx, PR_TRUE);
	break;
#if NSS_SOFTOKEN_DOES_RC5
    case bltestRC5_ECB:
    case bltestRC5_CBC:
	RC5_DestroyContext((RC5Context *)cipherInfo->cx, PR_TRUE);
	break;
#endif
    case bltestRSA: /* keys are alloc'ed within cipherInfo's arena, */
    case bltestDSA: /* will be freed with it. */
    case bltestMD2: /* hash contexts are ephemeral */
    case bltestMD5:
    case bltestSHA1:
	return SECSuccess;
	break;
    default:
	return SECFailure;
    }
    return SECSuccess;
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

void
dump_performance_info(bltestCipherInfo *info, PRBool encrypt, PRBool cxonly)
{
    PRBool td = PR_TRUE;
    fprintf(stdout, "#%9s", "mode");
    fprintf(stdout, "%12s", "in");
print_td:
    switch (info->mode) {
    case bltestDES_ECB:
    case bltestDES_CBC:
    case bltestDES_EDE_ECB:
    case bltestDES_EDE_CBC:
    case bltestRC2_ECB:
    case bltestRC2_CBC:
    case bltestRC4:
	if (td)
	    fprintf(stdout, "%8s", "symmkey");
	else
	    fprintf(stdout, "%8d", 8*info->params.sk.key.buf.len);
	break;
#if NSS_SOFTOKEN_DOES_RC5
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
	    fprintf(stdout, "%8d", PQG_INDEX_TO_PBITS(info->params.dsa.j));
	break;
    case bltestMD2:
    case bltestMD5:
    case bltestSHA1:
    default:
	break;
    }
    if (!td) {
	fprintf(stdout, "%8d", info->repetitions);
	fprintf(stdout, "%8d", info->cxreps);
	fprintf(stdout, "%12.3f", info->cxtime);
	fprintf(stdout, "%12.3f", info->optime);
	fprintf(stdout, "\n");
	return;
    }

    fprintf(stdout, "%8s", "opreps");
    fprintf(stdout, "%8s", "cxreps");
    fprintf(stdout, "%12s", "context");
    fprintf(stdout, "%12s", "op");
    fprintf(stdout, "\n");
    fprintf(stdout, "%8s", mode_strings[info->mode]);
    fprintf(stdout, "_%c", (cxonly) ? 'c' : (encrypt) ? 'e' : 'd');
    fprintf(stdout, "%12d", info->input.buf.len * info->repetitions);

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
load_file_data(PRArenaPool *arena, bltestIO *data,
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
get_params(PRArenaPool *arena, bltestParams *params, 
	   bltestCipherMode mode, int j)
{
    char filename[256];
    char *modestr = mode_strings[mode];
#if NSS_SOFTOKEN_DOES_RC5
    FILE *file;
    char *mark, *param, *val;
    int index = 0;
#endif
    switch (mode) {
    case bltestDES_CBC:
    case bltestDES_EDE_CBC:
    case bltestRC2_CBC:
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "iv", j);
	load_file_data(arena, &params->sk.iv, filename, bltestBinary);
    case bltestDES_ECB:
    case bltestDES_EDE_ECB:
    case bltestRC2_ECB:
    case bltestRC4:
	sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr, "key", j);
	load_file_data(arena, &params->sk.key, filename, bltestBinary);
	break;
#if NSS_SOFTOKEN_DOES_RC5
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
    case bltestMD2:
    case bltestMD5:
    case bltestSHA1:
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
    res = SECITEM_CompareItem(&result->buf, &cmp->buf);
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
    int i, j, nummodes;
    char *modestr;
    char filename[256];
    PRFileDesc *file;
    PRArenaPool *arena;
    SECItem item;
    PRBool finished;
    SECStatus rv, srv;

    PORT_Memset(&cipherInfo, 0, sizeof(cipherInfo));
    arena = PORT_NewArena(BLTEST_DEFAULT_CHUNKSIZE);
    cipherInfo.arena = arena;

    finished = PR_FALSE;
    nummodes = (numModes == 0) ? NUMMODES : numModes;
    for (i=0; i < nummodes && !finished; i++) {
	if (i == bltestRC5_ECB || i == bltestRC5_CBC) continue;
	if (numModes > 0)
	    mode = modes[i];
	else
	    mode = i;
	modestr = mode_strings[mode];
	cipherInfo.mode = mode;
	params = &cipherInfo.params;
	if (mode == bltestINVALID) {
	    fprintf(stderr, "%s: Skipping invalid mode %s.\n",progName,modestr);
	    continue;
	}
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
	for (j=0; j<(int)(item.data[0] - '0'); j++) { /* XXX bug when > 10 */
#ifdef TRACK_BLTEST_BUG
	    if (mode == bltestRSA) {
		fprintf(stderr, "[%s] Executing self-test #%d\n", __bltDBG, j);
	    }
#endif
	    sprintf(filename, "%s/tests/%s/%s%d", testdir, modestr,
			      "plaintext", j);
	    if (mode == bltestDSA)
	    load_file_data(arena, &pt, filename, bltestBase64Encoded);
	    else
	    load_file_data(arena, &pt, filename, bltestBinary);
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
	    if (mode != bltestDSA)
		bltestCopyIO(arena, &cipherInfo.input, &ct);
	    else
		bltestCopyIO(arena, &cipherInfo.input, &pt);
	    misalignBuffer(arena, &cipherInfo.input, outoff);
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
    PRArenaPool *arena = NULL;
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
    }
    PORT_FreeArena(arena, PR_FALSE);
    return SECFailure;
}

/* bltest commands */
enum {
    cmd_Decrypt = 0,
    cmd_Encrypt,
    cmd_FIPS,
    cmd_Hash,
    cmd_Nonce,
    cmd_Dump,
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
    opt_InputOffset,
    opt_OutputOffset,
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
    { /* opt_InputOffset  */ '1', PR_TRUE,  0, PR_FALSE },
    { /* opt_OutputOffset */ '2', PR_TRUE,  0, PR_FALSE },
    { /* opt_CmdLine	  */ '-', PR_FALSE, 0, PR_FALSE }
};

int main(int argc, char **argv)
{
    char *infileName, *outfileName, *keyfileName, *ivfileName;
    SECStatus rv;

    bltestCipherInfo	 cipherInfo;
    bltestParams	*params;
    PRFileDesc		*file, *infile, *outfile;
    char		*instr = NULL;
    PRArenaPool		*arena;
    bltestIOMode	 ioMode;
    int			 keysize, bufsize, exponent;
    int			 i, commandsEntered;
    int			 inoff, outoff;

    secuCommand bltest;
    bltest.numCommands = sizeof(bltest_commands) / sizeof(secuCommandFlag);
    bltest.numOptions = sizeof(bltest_options) / sizeof(secuCommandFlag);
    bltest.commands = bltest_commands;
    bltest.options = bltest_options;

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];

    rv = NSS_NoDB_Init(NULL);
    if (rv != SECSuccess) {
    	SECU_PrintPRandOSError(progName);
	return -1;
    }

    rv = SECU_ParseCommandLine(argc, argv, progName, &bltest);

    PORT_Memset(&cipherInfo, 0, sizeof(cipherInfo));
    arena = PORT_NewArena(BLTEST_DEFAULT_CHUNKSIZE);
    cipherInfo.arena = arena;
    params = &cipherInfo.params;
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
	Usage();
    }
    if (commandsEntered == 0) {
	fprintf(stderr, "%s: you must enter a command!\n", progName);
	Usage();
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
	return blapi_selftest(modesToTest, numModesToTest, inoff, outoff,
	                      encrypt, decrypt);
    }

    /* Do FIPS self-test */
    if (bltest.commands[cmd_FIPS].activated) {
	CK_RV ckrv = pk11_fipsPowerUpSelfTest();
	fprintf(stdout, "CK_RV: %ld.\n", ckrv);
	return 0;
    }

    /*
     * Check command line arguments for Encrypt/Decrypt/Hash/Sign/Verify
     */

    if ((bltest.commands[cmd_Decrypt].activated ||
	 bltest.commands[cmd_Verify].activated) &&
	bltest.options[opt_BufSize].activated) {
	fprintf(stderr, "%s: cannot use a nonce as input to decrypt/verify.\n",
			 progName);
	Usage();
    }

    if (bltest.options[opt_Mode].activated) {
	cipherInfo.mode = get_mode(bltest.options[opt_Mode].arg);
	if (cipherInfo.mode == bltestINVALID) {
	    fprintf(stderr, "%s: Invalid mode \"%s\"\n", progName,
			     bltest.options[opt_Mode].arg);
	    Usage();
	}
    } else {
	fprintf(stderr, "%s: You must specify a cipher mode with -m.\n",
			 progName);
	Usage();
    }

    if (bltest.options[opt_Repetitions].activated) {
	cipherInfo.repetitions = PORT_Atoi(bltest.options[opt_Repetitions].arg);
    } else {
	cipherInfo.repetitions = 0;
    }


    if (bltest.options[opt_CXReps].activated) {
	cipherInfo.cxreps = PORT_Atoi(bltest.options[opt_CXReps].arg);
    } else {
	cipherInfo.cxreps = 0;
    }

    /* Dump a file (rsakey, dsakey, etc.) */
    if (bltest.commands[cmd_Dump].activated) {
	return dump_file(cipherInfo.mode, bltest.options[opt_Input].arg);
    }

    /* default input mode is binary */
    ioMode = (bltest.options[opt_B64].activated)     ? bltestBase64Encoded :
	     (bltest.options[opt_Hex].activated)     ? bltestHexStream :
	     (bltest.options[opt_HexWSpc].activated) ? bltestHexSpaceDelim :
						       bltestBinary;

    if (bltest.options[opt_KeySize].activated)
	keysize = PORT_Atoi(bltest.options[opt_KeySize].arg);
    else
	keysize = 0;

    if (bltest.options[opt_Exponent].activated)
	exponent = PORT_Atoi(bltest.options[opt_Exponent].arg);
    else
	exponent = 65537;

    /* Set up an encryption key. */
    keysize = 0;
    file = NULL;
    if (is_symmkeyCipher(cipherInfo.mode)) {
	char *keystr = NULL;  /* if key is on command line */
	if (bltest.options[opt_Key].activated) {
	    if (bltest.options[opt_CmdLine].activated) {
		keystr = bltest.options[opt_Key].arg;
	    } else {
		file = PR_Open(bltest.options[opt_Key].arg, PR_RDONLY, 00660);
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
	setupIO(cipherInfo.arena, &params->key, file, keystr, keysize);
	if (file)
	    PR_Close(file);
    } else if (is_pubkeyCipher(cipherInfo.mode)) {
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
	pubkeyInitKey(&cipherInfo, file, keysize, exponent);
	PR_Close(file);
    }

    /* set up an initialization vector. */
    if (cipher_requires_IV(cipherInfo.mode)) {
	char *ivstr = NULL;
	bltestSymmKeyParams *skp;
	file = NULL;
	if (cipherInfo.mode == bltestRC5_CBC)
	    skp = (bltestSymmKeyParams *)&params->rc5;
	else
	    skp = &params->sk;
	if (bltest.options[opt_IV].activated) {
	    if (bltest.options[opt_CmdLine].activated) {
		ivstr = bltest.options[opt_IV].arg;
	    } else {
		file = PR_Open(bltest.options[opt_IV].arg, PR_RDONLY, 00660);
	    }
	} else {
	    /* save the random iv for reference */
	    file = PR_Open("tmp.iv", PR_WRONLY|PR_CREATE_FILE, 00660);
	}
	memset(&skp->iv, 0, sizeof skp->iv);
	skp->iv.mode = ioMode;
	setupIO(cipherInfo.arena, &skp->iv, file, ivstr, keysize);
	if (file)
	    PR_Close(file);
    }

    if (bltest.commands[cmd_Verify].activated) {
	if (!bltest.options[opt_SigFile].activated) {
	    fprintf(stderr, "%s: You must specify a signature file with -f.\n",
	            progName);
	    exit(-1);
	}
	file = PR_Open(bltest.options[opt_SigFile].arg, PR_RDONLY, 00660);
	memset(&cipherInfo.params.dsa.sig, 0, sizeof(bltestIO));
	cipherInfo.params.dsa.sig.mode = ioMode;
	setupIO(cipherInfo.arena, &cipherInfo.params.dsa.sig, file, NULL, 0);
    }

    if (bltest.options[opt_PQGFile].activated) {
	file = PR_Open(bltest.options[opt_PQGFile].arg, PR_RDONLY, 00660);
	params->dsa.pqgdata.mode = bltestBase64Encoded;
	setupIO(cipherInfo.arena, &params->dsa.pqgdata, file, NULL, 0);
    }

    /* Set up the input buffer */
    if (bltest.options[opt_Input].activated) {
	if (bltest.options[opt_CmdLine].activated) {
	    instr = bltest.options[opt_Input].arg;
	    infile = NULL;
	} else {
	    infile = PR_Open(bltest.options[opt_Input].arg, PR_RDONLY, 00660);
	}
    } else if (bltest.options[opt_BufSize].activated) {
	/* save the random plaintext for reference */
	infile = PR_Open("tmp.in", PR_WRONLY|PR_CREATE_FILE, 00660);
    } else {
	infile = PR_STDIN;
    }
    cipherInfo.input.mode = ioMode;

    /* Set up the output stream */
    if (bltest.options[opt_Output].activated) {
	outfile = PR_Open(bltest.options[opt_Output].arg,
			  PR_WRONLY|PR_CREATE_FILE, 00660);
    } else {
	outfile = PR_STDOUT;
    }
    cipherInfo.output.mode = ioMode;

    if (is_hashCipher(cipherInfo.mode))
	cipherInfo.params.hash.restart = bltest.options[opt_Restart].activated;

    bufsize = 0;
    if (bltest.options[opt_BufSize].activated)
	bufsize = PORT_Atoi(bltest.options[opt_BufSize].arg);

    /*infile = NULL;*/
    setupIO(cipherInfo.arena, &cipherInfo.input, infile, instr, bufsize);
    misalignBuffer(cipherInfo.arena, &cipherInfo.input, inoff);

    cipherInit(&cipherInfo, bltest.commands[cmd_Encrypt].activated);
    misalignBuffer(cipherInfo.arena, &cipherInfo.output, outoff);

    if (!bltest.commands[cmd_Nonce].activated) {
	cipherDoOp(&cipherInfo);
	cipherFinish(&cipherInfo);
	finishIO(&cipherInfo.output, outfile);
    }

    if (cipherInfo.repetitions > 0 || cipherInfo.cxreps > 0)
	dump_performance_info(&cipherInfo, 
	                      bltest.commands[cmd_Encrypt].activated,
	                      (cipherInfo.repetitions == 0));

    if (infile && infile != PR_STDIN)
	PR_Close(infile);
    if (outfile && outfile != PR_STDOUT)
	PR_Close(outfile);
    PORT_FreeArena(cipherInfo.arena, PR_TRUE);

    /*NSS_Shutdown();*/

    return SECSuccess;
}
