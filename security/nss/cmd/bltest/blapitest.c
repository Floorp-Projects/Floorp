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

char *progName;
char *testdir = ".";

#define CHECKERROR(rv, ln) \
	if (rv) { \
		char *errtxt = NULL; \
		if (PR_GetError() != 0) { \
		errtxt = PORT_Alloc(PR_GetErrorTextLength()); \
		PR_GetErrorText(errtxt); \
		} \
		PR_fprintf(PR_STDERR, "%s: ERR (%s) at line %d.\n", progName, \
		                       (errtxt) ? "" : errtxt, ln); \
		exit(-1); \
	}

static void Usage()
{
#define PRINTUSAGE(subject, option, predicate) \
	fprintf(stderr, "%10s %s\t%s\n", subject, option, predicate);
	fprintf(stderr, "\n");
	PRINTUSAGE(progName, "[-DEHSV] -m", "List available cipher modes.");
	fprintf(stderr, "\n");
	PRINTUSAGE(progName, "-E -m mode ", "Encrypt a buffer.");
	PRINTUSAGE("",      "", "[-i plaintext] [-o ciphertext] [-k key] [-v iv]");
	PRINTUSAGE("",      "", "[-b bufsize] [-g keysize] [-erw]");
	PRINTUSAGE("",      "", "[-p repetitions]");
	PRINTUSAGE("",      "-m", "cipher mode to use.");
	PRINTUSAGE("",      "-i", "file which contains input buffer.");
	PRINTUSAGE("",      "-o", "file for output buffer.");
	PRINTUSAGE("",      "-k", "file which contains key.");
	PRINTUSAGE("",      "-v", "file which contains initialization vector.");
	PRINTUSAGE("",      "-b", "size of input buffer.");
	PRINTUSAGE("",      "-g", "key size (in bytes).");
	PRINTUSAGE("",      "-p", "do performance test.");
	PRINTUSAGE("(rsa)", "-e", "rsa public exponent.");
#if NSS_SOFTOKEN_DOES_RC5
	PRINTUSAGE("(rc5)", "-r", "number of rounds.");
	PRINTUSAGE("(rc5)", "-w", "wordsize (32 or 64).");
#endif
	fprintf(stderr, "\n");
	PRINTUSAGE(progName, "-D -m mode", "Decrypt a buffer.");
	PRINTUSAGE("",      "", "[-i plaintext] [-o ciphertext] [-k key] [-v iv]");
	PRINTUSAGE("",      "", "[-p repetitions]");
	PRINTUSAGE("",      "-m", "cipher mode to use.");
	PRINTUSAGE("",      "-i", "file which contains input buffer.");
	PRINTUSAGE("",      "-o", "file for output buffer.");
	PRINTUSAGE("",      "-k", "file which contains key.");
	PRINTUSAGE("",      "-v", "file which contains initialization vector.");
	PRINTUSAGE("",      "-p", "do performance test.");
	fprintf(stderr, "\n");
	PRINTUSAGE(progName, "-H -m mode", "Hash a buffer.");
	PRINTUSAGE("",      "", "[-i plaintext] [-o hash]");
	PRINTUSAGE("",      "", "[-b bufsize]");
	PRINTUSAGE("",      "", "[-p repetitions]");
	PRINTUSAGE("",      "-m", "cipher mode to use.");
	PRINTUSAGE("",      "-i", "file which contains input buffer.");
	PRINTUSAGE("",      "-o", "file for hash.");
	PRINTUSAGE("",      "-b", "size of input buffer.");
	PRINTUSAGE("",      "-p", "do performance test.");
	fprintf(stderr, "\n");
	PRINTUSAGE(progName, "-S -m mode", "Sign a buffer.");
	PRINTUSAGE("",      "", "[-i plaintext] [-o signature] [-k key]");
	PRINTUSAGE("",      "", "[-b bufsize]");
	PRINTUSAGE("",      "", "[-p repetitions]");
	PRINTUSAGE("",      "-m", "cipher mode to use.");
	PRINTUSAGE("",      "-i", "file which contains input buffer.");
	PRINTUSAGE("",      "-o", "file for signature.");
	PRINTUSAGE("",      "-k", "file which contains key.");
	PRINTUSAGE("",      "-p", "do performance test.");
	fprintf(stderr, "\n");
	PRINTUSAGE(progName, "-V -m mode", "Verify a signed buffer.");
	PRINTUSAGE("",      "", "[-i plaintext] [-s signature] [-k key]");
	PRINTUSAGE("",      "", "[-p repetitions]");
	PRINTUSAGE("",      "-m", "cipher mode to use.");
	PRINTUSAGE("",      "-i", "file which contains input buffer.");
	PRINTUSAGE("",      "-s", "file which contains signature of input buffer.");
	PRINTUSAGE("",      "-k", "file which contains key.");
	PRINTUSAGE("",      "-p", "do performance test.");
	fprintf(stderr, "\n");
	PRINTUSAGE(progName, "-F", "Run the FIPS self-test.");
	fprintf(stderr, "\n");
	PRINTUSAGE(progName, "-T [mode1 mode2 ...]", "Run the BLAPI self-test.");
	fprintf(stderr, "\n");
	exit(1);
}

/*  Helper functions for ascii<-->binary conversion/reading/writing */

static PRInt32
get_binary(void *arg, const unsigned char *ibuf, PRInt32 size)
{
	SECItem *binary = arg;
	SECItem *tmp;
	int index;
	if (binary->data == NULL) {
		tmp = SECITEM_AllocItem(NULL, NULL, size);
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

static PRInt32
get_ascii(void *arg, const char *ibuf, PRInt32 size)
{
	SECItem *ascii = arg;
	SECItem *tmp;
	int index;
	if (ascii->data == NULL) {
		tmp = SECITEM_AllocItem(NULL, NULL, size);
		ascii->data = tmp->data;
		ascii->len = tmp->len;
		index = 0;
	} else {
		SECITEM_ReallocItem(NULL, ascii, ascii->len, ascii->len + size);
		index = ascii->len;
	}
	PORT_Memcpy(&ascii->data[index], ibuf, size);
	return ascii->len;
}

static SECStatus
atob(SECItem *ascii, SECItem *binary)
{
	SECStatus status;
	NSSBase64Decoder *cx;
	int len;
	binary->data = NULL; 
	binary->len = 0;
	len = (strcmp(&ascii->data[ascii->len-2],"\r\n"))?ascii->len:ascii->len-2;
	cx = NSSBase64Decoder_Create(get_binary, binary);
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
btoa(SECItem *binary, SECItem *ascii)
{
	SECStatus status;
	NSSBase64Encoder *cx;
	ascii->data = NULL;
	ascii->len = 0;
	cx = NSSBase64Encoder_Create(get_ascii, ascii);
	status = NSSBase64Encoder_Update(cx, binary->data, binary->len);
	status = NSSBase64Encoder_Destroy(cx, PR_FALSE);
	return status;
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

static SECStatus
get_and_write_random_bytes(SECItem *item, PRInt32 numbytes, char *filename)
{
	SECStatus rv;
	PRFileDesc *file;
	item->len = numbytes;
	item->data = (unsigned char *)PORT_ZAlloc(numbytes);
	RNG_GenerateGlobalRandomBytes(item->data + 1, numbytes - 1);
	file = PR_Open(filename, PR_WRONLY|PR_CREATE_FILE, 00660);
	rv = btoa_file(item, file);
	CHECKERROR((rv < 0), __LINE__);
	return (rv < 0);
}

static RSAPrivateKey *
rsakey_from_filedata(SECItem *filedata)
{
	PRArenaPool *arena;
	RSAPrivateKey *key;
	unsigned char *buf = filedata->data;
	int fpos = 0;
	int i;
	SECItem *item;
	/*  Allocate space for key structure. */
	arena = PORT_NewArena(2048);
	key = (RSAPrivateKey *)PORT_ArenaZAlloc(arena, sizeof(RSAPrivateKey));
	key->arena = arena;
	item = &key->version;
	for (i=0; i<9; i++) {
		item->len  = (buf[fpos++] & 0xff) << 24;
		item->len |= (buf[fpos++] & 0xff) << 16;
		item->len |= (buf[fpos++] & 0xff) <<  8;
		item->len |= (buf[fpos++] & 0xff);
		if (item->len > 0) {
			item->data = PORT_ArenaAlloc(arena, item->len);
			PORT_Memcpy(item->data, &buf[fpos], item->len);
		} else {
			item->data = NULL;
		}
		fpos += item->len;
		item++;
	}
	return key;
}

static void
rsakey_to_file(RSAPrivateKey *key, char *filename)
{
	PRFileDesc *file;
	SECItem *item;
	unsigned char len[4];
	int i;
	SECStatus status;
	NSSBase64Encoder *cx;
	SECItem ascii;
	ascii.data = NULL; 
	ascii.len = 0;
	file  = PR_Open(filename, PR_WRONLY|PR_CREATE_FILE, 00660);
	cx = NSSBase64Encoder_Create(output_ascii, file);
	item = &key->version;
	for (i=0; i<9; i++) {
		len[0] = (item->len >> 24) & 0xff;
		len[1] = (item->len >> 16) & 0xff;
		len[2] = (item->len >>  8) & 0xff;
		len[3] = (item->len & 0xff);
		status = NSSBase64Encoder_Update(cx, len, 4);
		status = NSSBase64Encoder_Update(cx, item->data, item->len);
		item++;
	}
	status = NSSBase64Encoder_Destroy(cx, PR_FALSE);
	status = PR_Write(file, "\r\n", 2);
	PR_Close(file);
}

static PQGParams *
pqg_from_filedata(SECItem *filedata)
{
	PRArenaPool *arena;
	PQGParams *pqg;
	unsigned char *buf = filedata->data;
	int fpos = 0;
	int i;
	SECItem *item;
	/*  Allocate space for key structure. */
	arena = PORT_NewArena(2048);
	pqg = (PQGParams *)PORT_ArenaZAlloc(arena, sizeof(PQGParams));
	pqg->arena = arena;
	item = &pqg->prime;
	for (i=0; i<3; i++) {
		item->len  = (buf[fpos++] & 0xff) << 24;
		item->len |= (buf[fpos++] & 0xff) << 16;
		item->len |= (buf[fpos++] & 0xff) <<  8;
		item->len |= (buf[fpos++] & 0xff);
		if (item->len > 0) {
			item->data = PORT_ArenaAlloc(arena, item->len);
			PORT_Memcpy(item->data, &buf[fpos], item->len);
		} else {
			item->data = NULL;
		}
		fpos += item->len;
		item++;
	}
	return pqg;
}

static DSAPrivateKey *
dsakey_from_filedata(SECItem *filedata)
{
	PRArenaPool *arena;
	DSAPrivateKey *key;
	unsigned char *buf = filedata->data;
	int fpos = 0;
	int i;
	SECItem *item;
	/*  Allocate space for key structure. */
	arena = PORT_NewArena(2048);
	key = (DSAPrivateKey *)PORT_ArenaZAlloc(arena, sizeof(DSAPrivateKey));
	key->params.arena = arena;
	item = &key->params.prime;
	for (i=0; i<5; i++) {
		item->len  = (buf[fpos++] & 0xff) << 24;
		item->len |= (buf[fpos++] & 0xff) << 16;
		item->len |= (buf[fpos++] & 0xff) <<  8;
		item->len |= (buf[fpos++] & 0xff);
		if (item->len > 0) {
			item->data = PORT_ArenaAlloc(arena, item->len);
			PORT_Memcpy(item->data, &buf[fpos], item->len);
		} else {
			item->data = NULL;
		}
		fpos += item->len;
		item++;
	}
	return key;
}

static void
pqg_to_file(PQGParams *params, char *filename)
{
	PRFileDesc *file;
	SECItem *item;
	unsigned char len[4];
	int i;
	SECStatus status;
	NSSBase64Encoder *cx;
	SECItem ascii;
	ascii.data = NULL; 
	ascii.len = 0;
	file  = PR_Open(filename, PR_WRONLY|PR_CREATE_FILE, 00660);
	cx = NSSBase64Encoder_Create(output_ascii, file);
	item = &params->prime;
	for (i=0; i<3; i++) {
		len[0] = (item->len >> 24) & 0xff;
		len[1] = (item->len >> 16) & 0xff;
		len[2] = (item->len >>  8) & 0xff;
		len[3] = (item->len & 0xff);
		status = NSSBase64Encoder_Update(cx, len, 4);
		status = NSSBase64Encoder_Update(cx, item->data, item->len);
		item++;
	}
	status = NSSBase64Encoder_Destroy(cx, PR_FALSE);
	status = PR_Write(file, "\r\n", 2);
}

static void
dsakey_to_file(DSAPrivateKey *key, char *filename)
{
	PRFileDesc *file;
	SECItem *item;
	unsigned char len[4];
	int i;
	SECStatus status;
	NSSBase64Encoder *cx;
	SECItem ascii;
	ascii.data = NULL; 
	ascii.len = 0;
	file  = PR_Open(filename, PR_WRONLY|PR_CREATE_FILE, 00660);
	cx = NSSBase64Encoder_Create(output_ascii, file);
	item = &key->params.prime;
	for (i=0; i<5; i++) {
		len[0] = (item->len >> 24) & 0xff;
		len[1] = (item->len >> 16) & 0xff;
		len[2] = (item->len >>  8) & 0xff;
		len[3] = (item->len & 0xff);
		status = NSSBase64Encoder_Update(cx, len, 4);
		status = NSSBase64Encoder_Update(cx, item->data, item->len);
		item++;
	}
	status = NSSBase64Encoder_Destroy(cx, PR_FALSE);
	status = PR_Write(file, "\r\n", 2);
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

/*  Multi-purpose crypto information */
typedef struct 
{
	PRBool  encrypt;
	PRBool  decrypt;
	PRBool  sign;
	PRBool  verify;
	PRBool  hash;
	SECItem seed;
	SECItem pqg;
	SECItem key;
   	SECItem iv;
   	SECItem in;
   	SECItem out;
	SECItem sigseed;
	PRInt32 keysize;
	PRInt32 bufsize;
	PRBool  useseed;
	PRBool  usesigseed;
	PRBool  performance;
	PRBool  multihash;
	PQGParams *params;      /* DSA only */
	unsigned int rounds;    /* RC5 only */
	unsigned int wordsize;  /* RC5 only */
	unsigned int rsapubexp; /* RSA only */
	unsigned int repetitions; /* performance tests only */
} blapitestInfo;

/* Macros for performance timing. */
#define TIMESTART() \
	if (info->performance) \
		time1 = PR_IntervalNow();

#define TIMEFINISH(mode, nb) \
	if (info->performance) { \
		time2 = (PRIntervalTime)(PR_IntervalNow() - time1); \
		time1 = PR_IntervalToMilliseconds(time2); \
		printf("%s,%d,%.3f\n", mode, nb, ((float)(time1))/info->repetitions); \
	}

SECStatus
fillitem(SECItem *item, int numbytes, char *filename)
{
	if (item->len == 0)
		return get_and_write_random_bytes(item, numbytes, filename);
	return SECSuccess;
}

/************************
**  DES 
************************/

/*  encrypt/decrypt for all DES */
static SECStatus
des_common(DESContext *descx, blapitestInfo *info)
{
	PRInt32 maxsize;
	SECStatus rv;
	PRIntervalTime time1, time2;
	int i, numiter;
	numiter = info->repetitions;
	maxsize = info->in.len;
	info->out.data = (unsigned char *)PORT_ZAlloc(maxsize);
	if (info->encrypt) {
		TIMESTART();
		for (i=0; i<numiter; i++)
			rv = DES_Encrypt(descx, info->out.data, &info->out.len, maxsize,
			                 info->in.data, info->in.len);
		TIMEFINISH("DES ENCRYPT", maxsize);
		if (rv) {
			fprintf(stderr, "%s:  Failed to encrypt!\n", progName);
			CHECKERROR(rv, __LINE__);
		}
	} else {
		TIMESTART();
		for (i=0; i<numiter; i++)
			rv = DES_Decrypt(descx, info->out.data, &info->out.len, maxsize,
			                 info->in.data, info->in.len);
		TIMEFINISH("DES DECRYPT", maxsize);
		if (rv) {
			fprintf(stderr, "%s:  Failed to decrypt!\n", progName);
			CHECKERROR(rv, __LINE__);
		}
	}
	return rv;
}

/*  DES codebook mode */
static SECStatus
des_ecb_test(blapitestInfo *info)
{
	SECStatus rv;
	DESContext *descx;
	PRIntervalTime time1, time2;
	int i, numiter = info->repetitions;
	fillitem(&info->key, DES_KEY_LENGTH, "tmp.key");
	fillitem(&info->in, info->bufsize, "tmp.pt");
	TIMESTART();
	for (i=0; i<numiter-1; i++) {
		descx = DES_CreateContext(info->key.data, NULL, NSS_DES, info->encrypt);
		DES_DestroyContext(descx, PR_TRUE);
	}
	descx = DES_CreateContext(info->key.data, NULL, NSS_DES, info->encrypt);
	TIMEFINISH("DES ECB CONTEXT CREATE", info->key.len);
	if (!descx) {
		fprintf(stderr,"%s:  Failed to create encryption context!\n", progName);
		return SECFailure;
	}
	rv = des_common(descx, info);
	CHECKERROR(rv, __LINE__);
	DES_DestroyContext(descx, PR_TRUE);
	return rv;
}

/*  DES chaining mode */
static SECStatus
des_cbc_test(blapitestInfo *info)
{
	SECStatus rv;
	DESContext *descx;
	PRIntervalTime time1, time2;
	int i, numiter = info->repetitions;
	fillitem(&info->key, DES_KEY_LENGTH, "tmp.key");
	fillitem(&info->in, info->bufsize, "tmp.pt");
	fillitem(&info->iv, DES_KEY_LENGTH, "tmp.iv");
	TIMESTART();
	for (i=0; i<numiter-1; i++) {
		descx = DES_CreateContext(info->key.data, info->iv.data, NSS_DES_CBC, 
		                          info->encrypt);
		DES_DestroyContext(descx, PR_TRUE);
	}
	descx = DES_CreateContext(info->key.data, info->iv.data, NSS_DES_CBC, 
	                          info->encrypt);
	TIMEFINISH("DES CBC CONTEXT CREATE", info->key.len);
	if (!descx) {
		PR_fprintf(PR_STDERR, 
		  "%s:  Failed to create encryption context!\n", progName);
		return SECFailure;
	}
	if (info->performance) {
		/*  In chaining mode, repeated iterations of the encryption
		 *  function using the same context will alter the final output.
		 *  So, once the performance test is done, reset the context
		 *  and perform a single iteration to obtain the correct result.
		 */
		int tmp = info->repetitions;
		rv = des_common(descx, info);
		DES_DestroyContext(descx, PR_TRUE);
		descx = DES_CreateContext(info->key.data, info->iv.data, NSS_DES_CBC, 
	                          info->encrypt);
		info->performance = PR_FALSE;
		info->repetitions = 1;
		rv = des_common(descx, info);
		info->performance = PR_TRUE;
		info->repetitions = tmp;
	} else {
		rv = des_common(descx, info);
	}
	CHECKERROR(rv, __LINE__);
	DES_DestroyContext(descx, PR_TRUE);
	return rv;
}

/*  3-key Triple-DES codebook mode */
static SECStatus
des_ede_ecb_test(blapitestInfo *info)
{
	SECStatus rv;
	DESContext *descx;
	PRIntervalTime time1, time2;
	int i, numiter = info->repetitions;
	fillitem(&info->key, 3*DES_KEY_LENGTH, "tmp.key");
	fillitem(&info->in, info->bufsize, "tmp.pt");
	TIMESTART();
	for (i=0; i<numiter-1; i++) {
		descx = DES_CreateContext(info->key.data, NULL, NSS_DES_EDE3,
		                          info->encrypt);
		DES_DestroyContext(descx, PR_TRUE);
	}
	descx = DES_CreateContext(info->key.data, NULL, NSS_DES_EDE3,
	                          info->encrypt);
	TIMEFINISH("3DES ECB CONTEXT CREATE", info->key.len);
	if (!descx) {
		fprintf(stderr,"%s:  Failed to create encryption context!\n", progName);
		return SECFailure;
	}
	rv = des_common(descx, info);
	CHECKERROR(rv, __LINE__);
	DES_DestroyContext(descx, PR_TRUE);
	return rv;
}

/*  3-key Triple-DES chaining mode */
static SECStatus
des_ede_cbc_test(blapitestInfo *info)
{
	SECStatus rv;
	DESContext *descx;
	PRIntervalTime time1, time2;
	int i, numiter = info->repetitions;
	fillitem(&info->key, 3*DES_KEY_LENGTH, "tmp.key");
	fillitem(&info->in, info->bufsize, "tmp.pt");
	fillitem(&info->iv, DES_KEY_LENGTH, "tmp.iv");
	TIMESTART();
	for (i=0; i<numiter-1; i++) {
		descx = DES_CreateContext(info->key.data, info->iv.data, 
		                          NSS_DES_EDE3_CBC, info->encrypt);
		DES_DestroyContext(descx, PR_TRUE);
	}
	descx = DES_CreateContext(info->key.data, info->iv.data, NSS_DES_EDE3_CBC, 
	                          info->encrypt);
	TIMEFINISH("3DES CBC CONTEXT CREATE", info->key.len);
	if (!descx) {
		fprintf(stderr,"%s:  Failed to create encryption context!\n", progName);
		return SECFailure;
	}
	if (info->performance) {
		/*  In chaining mode, repeated iterations of the encryption
		 *  function using the same context will alter the final output.
		 *  So, once the performance test is done, reset the context
		 *  and perform a single iteration to obtain the correct result.
		 */
		int tmp = info->repetitions;
		rv = des_common(descx, info);
		DES_DestroyContext(descx, PR_TRUE);
		descx = DES_CreateContext(info->key.data, info->iv.data, 
		                          NSS_DES_EDE3_CBC, info->encrypt);
		info->performance = PR_FALSE;
		info->repetitions = 1;
		rv = des_common(descx, info);
		info->performance = PR_TRUE;
		info->repetitions = tmp;
	} else {
		rv = des_common(descx, info);
	}
	CHECKERROR(rv, __LINE__);
	DES_DestroyContext(descx, PR_TRUE);
	return rv;
}

/************************
**  RC2 
************************/

/*  RC2 ECB */
static SECStatus
rc2_ecb_test(blapitestInfo *info)
{
	SECStatus rv;
	RC2Context *rc2cx;
	PRIntervalTime time1, time2;
	int i, numiter = info->repetitions;
	fillitem(&info->key, info->keysize, "tmp.key");
	fillitem(&info->in, info->bufsize, "tmp.pt");
	TIMESTART();
	for (i=0; i<numiter-1; i++) {
		rc2cx = RC2_CreateContext(info->key.data, info->key.len, NULL, 
		                          NSS_RC2, info->key.len);
		RC2_DestroyContext(rc2cx, PR_TRUE);
	}
	rc2cx = RC2_CreateContext(info->key.data, info->key.len, NULL, 
	                          NSS_RC2, info->key.len);
	TIMEFINISH("RC2 ECB CONTEXT CREATE", info->key.len);
	if (!rc2cx) {
		fprintf(stderr,"%s:  Failed to create encryption context!\n", progName);
		return SECFailure;
	}
	info->out.len = 2*info->in.len;
	info->out.data = (unsigned char *)PORT_ZAlloc(info->out.len);
	if (info->encrypt) {
		TIMESTART();
		for (i=0; i<numiter; i++)
			rv = RC2_Encrypt(rc2cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		TIMEFINISH("RC2 ECB ENCRYPT", info->in.len);
		CHECKERROR(rv, __LINE__);
	} else {
		TIMESTART();
		for (i=0; i<numiter; i++)
			rv = RC2_Decrypt(rc2cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		TIMEFINISH("RC2 ECB DECRYPT", info->in.len);
		CHECKERROR(rv, __LINE__);
	}
	RC2_DestroyContext(rc2cx, PR_TRUE);
	return rv;
}

/*  RC2 CBC */
static SECStatus
rc2_cbc_test(blapitestInfo *info)
{
	SECStatus rv;
	RC2Context *rc2cx;
	PRIntervalTime time1, time2;
	int i, numiter = info->repetitions;
	fillitem(&info->key, info->keysize, "tmp.key");
	fillitem(&info->in, info->bufsize, "tmp.pt");
	fillitem(&info->iv, info->bufsize, "tmp.iv");
	TIMESTART();
	for (i=0; i<numiter-1; i++) {
		rc2cx = RC2_CreateContext(info->key.data, info->key.len, info->iv.data, 
		                          NSS_RC2_CBC, info->key.len);
		RC2_DestroyContext(rc2cx, PR_TRUE);
	}
	rc2cx = RC2_CreateContext(info->key.data, info->key.len, info->iv.data, 
	                          NSS_RC2_CBC, info->key.len);
	TIMEFINISH("RC2 CBC CONTEXT CREATE", info->key.len);
	if (!rc2cx) {
		fprintf(stderr,"%s:  Failed to create encryption context!\n", progName);
		return SECFailure;
	}
	info->out.len = 2*info->in.len;
	info->out.data = (unsigned char *)PORT_ZAlloc(info->out.len);
	if (info->encrypt) {
		TIMESTART();
		for (i=0; i<numiter; i++)
			rv = RC2_Encrypt(rc2cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		TIMEFINISH("RC2 CBC ENCRYPT", info->in.len);
		if (info->performance) {
			/*  reset the context */
			RC2_DestroyContext(rc2cx, PR_TRUE);
			rc2cx = RC2_CreateContext(info->key.data, info->key.len, 
			                         info->iv.data, NSS_RC2_CBC, info->key.len);
			rv = RC2_Encrypt(rc2cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		}
		CHECKERROR(rv, __LINE__);
		if (rv) {
			fprintf(stderr, "%s:  Failed to encrypt!\n", progName);
			CHECKERROR(rv, __LINE__);
		}
	} else {
		TIMESTART();
		for (i=0; i<numiter; i++)
			rv = RC2_Decrypt(rc2cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		TIMEFINISH("RC2 CBC DECRYPT", info->in.len);
		if (info->performance) {
			/*  reset the context */
			RC2_DestroyContext(rc2cx, PR_TRUE);
			rc2cx = RC2_CreateContext(info->key.data, info->key.len, 
			                         info->iv.data, NSS_RC2_CBC, info->key.len);
			rv = RC2_Decrypt(rc2cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		}
		if (rv) {
			fprintf(stderr, "%s:  Failed to decrypt!\n", progName);
			CHECKERROR(rv, __LINE__);
		}
	}
	RC2_DestroyContext(rc2cx, PR_TRUE);
	return rv;
}

/************************
**  RC4 
************************/

static SECStatus
rc4_test(blapitestInfo *info)
{
	SECStatus rv;
	RC4Context *rc4cx;
	PRIntervalTime time1, time2;
	int i, numiter;
	numiter = info->repetitions;
	fillitem(&info->key, info->keysize, "tmp.key");
	fillitem(&info->in, info->bufsize, "tmp.pt");
	TIMESTART();
	for (i=0; i<numiter-1; i++) {
		rc4cx = RC4_CreateContext(info->key.data, info->key.len);
		RC4_DestroyContext(rc4cx, PR_TRUE);
	}
	rc4cx = RC4_CreateContext(info->key.data, info->key.len);
	TIMEFINISH("RC4 CONTEXT CREATE", info->key.len);
	if (!rc4cx) {
		fprintf(stderr,"%s:  Failed to create encryption context!\n", progName);
		return SECFailure;
	}
	info->out.len = 2*info->in.len;
	info->out.data = (unsigned char *)PORT_ZAlloc(info->out.len);
	if (info->encrypt) {
		TIMESTART();
		for (i=0; i<numiter; i++)
			rv = RC4_Encrypt(rc4cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		TIMEFINISH("RC4 ENCRYPT", info->in.len);
		if (info->performance) {
			/*  reset the context */
			RC4_DestroyContext(rc4cx, PR_TRUE);
			rc4cx = RC4_CreateContext(info->key.data, info->key.len);
			rv = RC4_Encrypt(rc4cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		}
		if (rv) {
			fprintf(stderr, "%s:  Failed to encrypt!\n", progName);
			CHECKERROR(rv, __LINE__);
		}
	} else {
		TIMESTART();
		for (i=0; i<numiter; i++)
			rv = RC4_Decrypt(rc4cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		TIMEFINISH("RC4 DECRYPT", info->in.len);
		if (info->performance) {
			/*  reset the context */
			RC4_DestroyContext(rc4cx, PR_TRUE);
			rc4cx = RC4_CreateContext(info->key.data, info->key.len);
			rv = RC4_Decrypt(rc4cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		}
		if (rv) {
			fprintf(stderr, "%s:  Failed to decrypt!\n", progName);
			CHECKERROR(rv, __LINE__);
		}
	}
	RC4_DestroyContext(rc4cx, PR_TRUE);
	return rv;
}

#if NSS_SOFTOKEN_DOES_RC5
/************************
**  RC5 
************************/

/*  RC5 ECB */
static SECStatus
rc5_ecb_test(blapitestInfo *info)
{
	SECStatus rv;
	RC5Context *rc5cx;
	PRIntervalTime time1, time2;
	int i, numiter;
	numiter = info->repetitions;
	fillitem(&info->key, info->keysize, "tmp.key");
	fillitem(&info->in, info->bufsize, "tmp.pt");
	TIMESTART();
	for (i=0; i<numiter-1; i++) {
		rc5cx = RC5_CreateContext(&info->key, info->rounds, info->wordsize, 
		                          NULL, NSS_RC5);
		RC5_DestroyContext(rc5cx, PR_TRUE);
	}
	rc5cx = RC5_CreateContext(&info->key, info->rounds, info->wordsize, 
	                          NULL, NSS_RC5);
	TIMEFINISH("RC5 ECB CONTEXT CREATE", info->key.len);
	if (!rc5cx) {
		fprintf(stderr,"%s:  Failed to create encryption context!\n", progName);
		return SECFailure;
	}
	info->out.len = 2*info->in.len;
	info->out.data = (unsigned char *)PORT_ZAlloc(info->out.len);
	if (info->encrypt) {
		TIMESTART();
		for (i=0; i<numiter; i++)
			rv = RC5_Encrypt(rc5cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		TIMEFINISH("RC5 ECB ENCRYPT", info->in.len);
		if (rv) {
			fprintf(stderr, "%s:  Failed to encrypt!\n", progName);
			CHECKERROR(rv, __LINE__);
		}
	} else {
		TIMESTART();
		for (i=0; i<numiter; i++)
			rv = RC5_Decrypt(rc5cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		TIMEFINISH("RC5 ECB ENCRYPT", info->in.len);
		if (rv) {
			fprintf(stderr, "%s:  Failed to decrypt!\n", progName);
			CHECKERROR(rv, __LINE__);
		}
	}
	RC5_DestroyContext(rc5cx, PR_TRUE);
	return rv;
}

/*  RC5 CBC */
static SECStatus
rc5_cbc_test(blapitestInfo *info)
{
	SECStatus rv;
	RC5Context *rc5cx;
	PRIntervalTime time1, time2;
	int i, numiter;
	numiter = info->repetitions;
	fillitem(&info->key, info->keysize, "tmp.key");
	fillitem(&info->in, info->bufsize, "tmp.pt");
	fillitem(&info->iv, info->bufsize, "tmp.iv");
	TIMESTART();
	for (i=0; i<numiter-1; i++) {
		rc5cx = RC5_CreateContext(&info->key, info->rounds, info->wordsize, 
		                          info->iv.data, NSS_RC5_CBC);
		RC5_DestroyContext(rc5cx, PR_TRUE);
	}
	rc5cx = RC5_CreateContext(&info->key, info->rounds, info->wordsize, 
	                          info->iv.data, NSS_RC5_CBC);
	TIMEFINISH("RC5 CBC CONTEXT CREATE", info->key.len);
	if (!rc5cx) {
		fprintf(stderr,"%s:  Failed to create encryption context!\n", progName);
		return SECFailure;
	}
	info->out.len = 2*info->in.len;
	info->out.data = (unsigned char *)PORT_ZAlloc(info->out.len);
	if (info->encrypt) {
		TIMESTART();
		for (i=0; i<numiter; i++)
			rv = RC5_Encrypt(rc5cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		TIMEFINISH("RC5 CBC ENCRYPT", info->in.len);
		if (info->performance) {
			/*  reset the context */
			RC5_DestroyContext(rc5cx, PR_TRUE);
			rc5cx = RC5_CreateContext(&info->key, info->rounds, info->wordsize, 
			                          info->iv.data, NSS_RC5_CBC);
			rv = RC5_Encrypt(rc5cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		}
		if (rv) {
			fprintf(stderr, "%s:  Failed to encrypt!\n", progName);
			CHECKERROR(rv, __LINE__);
		}
	} else {
		TIMESTART();
		for (i=0; i<numiter; i++)
			rv = RC5_Decrypt(rc5cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		TIMEFINISH("RC5 CBC DECRYPT", info->in.len);
		if (info->performance) {
			/*  reset the context */
			RC5_DestroyContext(rc5cx, PR_TRUE);
			rc5cx = RC5_CreateContext(&info->key, info->rounds, info->wordsize, 
			                          info->iv.data, NSS_RC5_CBC);
			rv = RC5_Decrypt(rc5cx, info->out.data, &info->out.len, 
			                 info->out.len, info->in.data, info->in.len);
		}
		if (rv) {
			fprintf(stderr, "%s:  Failed to decrypt!\n", progName);
			CHECKERROR(rv, __LINE__);
		}
	}
	RC5_DestroyContext(rc5cx, PR_TRUE);
	return rv;
}
#endif

static SECStatus
rsa_test(blapitestInfo *info)
{
	RSAPrivateKey *key;
	SECItem expitem;
	SECStatus rv;
	PRIntervalTime time1, time2;
	int i, j, numiter;
	unsigned int modLen;
	numiter = info->repetitions;
	fillitem(&info->in, info->bufsize, "tmp.pt");
	if (info->key.len > 0) {
		key = rsakey_from_filedata(&info->key);
	} else {
		expitem.len = 4;
		expitem.data = (unsigned char *)PORT_ZAlloc(4);
		expitem.data[0] = (info->rsapubexp >> 24) & 0xff;
		expitem.data[1] = (info->rsapubexp >> 16) & 0xff;
		expitem.data[2] = (info->rsapubexp >>  8) & 0xff;
		expitem.data[3] = (info->rsapubexp & 0xff);
		TIMESTART();
		for (i=0; i<numiter-1; i++) {
			key = RSA_NewKey(info->keysize*8, &expitem);
			PORT_FreeArena(key->arena, PR_TRUE);
		}
		key = RSA_NewKey(info->keysize*8, &expitem);
		TIMEFINISH("RSA KEY GEN", info->keysize);
		rsakey_to_file(key, "tmp.key");
	}
	if (key->modulus.data[0] == 0) {
		/* integer value of input must be less than modulus */
		if (info->in.data[0] >= key->modulus.data[1])
			return SECFailure;
	} else {
		if (info->in.data[0] >= key->modulus.data[0])
			return SECFailure;
	}
	modLen = key->modulus.len - !key->modulus.data[0];
	if (info->in.len % modLen != 0) {
		fprintf(stderr, "Input buffer must be a multiple of modulus length!\n");
		return SECFailure;
	}
	info->out.len = info->in.len; 
	info->out.data = (unsigned char *)PORT_ZAlloc(info->out.len);
	if (info->encrypt) {
		RSAPublicKey pubkey;
		SECITEM_CopyItem(key->arena, &pubkey.modulus, &key->modulus);
		SECITEM_CopyItem(key->arena, &pubkey.publicExponent, 
		                             &key->publicExponent);
		TIMESTART();
		for (i=0; i<numiter; i++) {
			for (j=0; j<info->in.len; j+=pubkey.modulus.len) {
				rv = RSA_PublicKeyOp(&pubkey, &info->out.data[j], 
				                              &info->in.data[j]);
			}
		}
		TIMEFINISH("RSA ENCRYPT", info->in.len);
		CHECKERROR(rv, __LINE__);
	} else {
		TIMESTART();
		for (i=info->repetitions; i>0; i--) {
			for (j=0; j<info->in.len; j+=key->modulus.len) {
				rv = RSA_PrivateKeyOp(key, &info->out.data[j], 
				                           &info->in.data[j]);
			}
		}
		TIMEFINISH("RSA DECRYPT", info->in.len);
		CHECKERROR(rv, __LINE__);
	}
	PORT_FreeArena(key->arena, PR_TRUE);
	return SECSuccess;
}

static SECStatus
pqg_test(blapitestInfo *info)
{
	SECStatus rv = SECSuccess;
	PQGVerify *verify;
	PRIntervalTime time1, time2;
	int i, numiter;
	numiter = info->repetitions;
	if (info->pqg.len > 0) {
		info->params = pqg_from_filedata(&info->pqg);
	} else {
		TIMESTART();
		for (i=0; i<numiter-1; i++) {
			rv = PQG_ParamGen(info->keysize, &info->params, &verify);
			PORT_FreeArena(info->params->arena, PR_TRUE);
		}
		rv = PQG_ParamGen(info->keysize, &info->params, &verify);
		TIMEFINISH("PQG PARAM GEN", info->keysize);
		pqg_to_file(info->params, "tmp.pqg");
	}
	CHECKERROR(rv, __LINE__);
	return rv;
}

static SECStatus
dsa_test(blapitestInfo *info)
{
	DSAPrivateKey *key;
	SECStatus rv = SECSuccess;
	PRIntervalTime time1, time2;
	int i, numiter;
	numiter = info->repetitions;
	fillitem(&info->in, info->bufsize, "tmp.pt");
	if (info->key.len > 0) {
		key = dsakey_from_filedata(&info->key);
	} else {
		pqg_test(info);
		if (info->useseed) {
			if (info->seed.len == 0)
				get_and_write_random_bytes(&info->seed, DSA_SUBPRIME_LEN, 
				                           "tmp.seed");
			rv = DSA_NewKeyFromSeed(info->params, info->seed.data, &key);
		} else {
			rv = DSA_NewKey(info->params, &key);
		}
		CHECKERROR(rv, __LINE__);
		dsakey_to_file(key, "tmp.key");
	}
	if (info->sign) {
		info->out.len = DSA_SIGNATURE_LEN;
		info->out.data = (unsigned char *)PORT_ZAlloc(info->out.len);
		if (info->usesigseed) {
			if (info->sigseed.len == 0)
				get_and_write_random_bytes(&info->sigseed, DSA_SUBPRIME_LEN,
				                           "tmp.sigseed");
			TIMESTART();
			rv = DSA_SignDigestWithSeed(key, &info->out, &info->in, 
			                            info->sigseed.data);
			TIMEFINISH("DSA SIGN", info->in.len);
		} else {
			TIMESTART();
			for (i=0; i<numiter; i++)
				rv = DSA_SignDigest(key, &info->out, &info->in);
			TIMEFINISH("DSA SIGN", info->in.len);
		}
		CHECKERROR(rv, __LINE__);
	} else {
		DSAPublicKey pubkey;
		PRArenaPool *arena;
		arena = key->params.arena;
		SECITEM_CopyItem(arena, &pubkey.params.prime, &key->params.prime);
		SECITEM_CopyItem(arena, &pubkey.params.subPrime, &key->params.subPrime);
		SECITEM_CopyItem(arena, &pubkey.params.base, &key->params.base);
		SECITEM_CopyItem(arena, &pubkey.publicValue, &key->publicValue);
		TIMESTART();
		for (i=0; i<numiter; i++)
			rv = DSA_VerifyDigest(&pubkey, &info->out, &info->in);
		TIMEFINISH("DSA VERIFY", info->in.len);
		if (rv != SECSuccess) {
			PR_fprintf(PR_STDOUT, "Signature failed verification!\n");
			CHECKERROR(rv, __LINE__);
		} /*else {
			PR_fprintf(PR_STDOUT, "Signature verified.\n");
		}*/
	}
	PORT_FreeArena(key->params.arena, PR_TRUE);
	return SECSuccess;
}

static SECStatus
md5_multi_test(blapitestInfo *info)
{
	SECStatus rv = SECSuccess;
	MD5Context *md5cx;
	unsigned int len;
	MD5Context *foomd5cx;
	unsigned char *foomd5;
	int i;
	if (info->in.len == 0) {
		rv = get_and_write_random_bytes(&info->in, info->bufsize, "tmp.pt");
		CHECKERROR(rv, __LINE__);
	}
	md5cx = MD5_NewContext();
	if (!md5cx) {
		PR_fprintf(PR_STDERR, 
		   "%s:  Failed to create hash context!\n", progName);
		return SECFailure;
	}
	info->out.len = MD5_LENGTH;
	info->out.data = (unsigned char *)PORT_ZAlloc(info->out.len);
	MD5_Begin(md5cx);
	for (i=0; i<info->bufsize/8; i++) {
		MD5_Update(md5cx, &info->in.data[i*8], 8);
		len = MD5_FlattenSize(md5cx);
		foomd5 = PORT_Alloc(len);
		MD5_Flatten(md5cx, foomd5);
		foomd5cx = MD5_Resurrect(foomd5, NULL);
		rv = PORT_Memcmp(foomd5cx, md5cx, len);
		if (rv != SECSuccess)
			PR_fprintf(PR_STDERR, "%s:  MD5_Resurrect failed!\n", progName);
		MD5_DestroyContext(foomd5cx, PR_TRUE);
		PORT_Free(foomd5);
	}
	MD5_End(md5cx, info->out.data, &len, MD5_LENGTH);
	if (len != MD5_LENGTH)
		PR_fprintf(PR_STDERR, "%s: Bad hash size %d.\n", progName, len);
	MD5_DestroyContext(md5cx, PR_TRUE);
	return rv;
}

static SECStatus
md5_test(blapitestInfo *info)
{
	SECStatus rv = SECSuccess;
	PRIntervalTime time1, time2;
	int i;
	if (!info->hash) return SECFailure;
	if (info->multihash) return md5_multi_test(info);
	if (info->in.len == 0) {
		rv = get_and_write_random_bytes(&info->in, info->bufsize, "tmp.pt");
		CHECKERROR(rv, __LINE__);
	}
	info->out.len = MD5_LENGTH;
	info->out.data = (unsigned char *)PORT_ZAlloc(info->out.len);
	TIMESTART();
	for (i=info->repetitions; i>0; i--) {
		MD5_HashBuf(info->out.data, info->in.data, info->in.len);
	}
	TIMEFINISH("MD5 HASH", info->in.len);
	return rv;
}

static SECStatus
md2_multi_test(blapitestInfo *info)
{
	SECStatus rv = SECSuccess;
	MD2Context *md2cx;
	unsigned int len;
	MD2Context *foomd2cx;
	unsigned char *foomd2;
	int i;
	if (!info->hash) return SECFailure;
	if (info->in.len == 0) {
		rv = get_and_write_random_bytes(&info->in, info->bufsize, "tmp.pt");
		CHECKERROR(rv, __LINE__);
	}
	md2cx = MD2_NewContext();
	if (!md2cx) {
		PR_fprintf(PR_STDERR, 
		   "%s:  Failed to create hash context!\n", progName);
		return SECFailure;
	}
	info->out.len = MD2_LENGTH;
	info->out.data = (unsigned char *)PORT_ZAlloc(info->out.len);
	MD2_Begin(md2cx);
	for (i=0; i<info->bufsize/8; i++) {
		MD2_Update(md2cx, &info->in.data[i*8], 8);
		len = MD2_FlattenSize(md2cx);
		foomd2 = PORT_Alloc(len);
		MD2_Flatten(md2cx, foomd2);
		foomd2cx = MD2_Resurrect(foomd2, NULL);
		rv = PORT_Memcmp(foomd2cx, md2cx, len);
		if (rv != SECSuccess)
			PR_fprintf(PR_STDERR, "%s:  MD2_Resurrect failed!\n", progName);
		MD2_DestroyContext(foomd2cx, PR_TRUE);
		PORT_Free(foomd2);
	}
	MD2_End(md2cx, info->out.data, &len, MD2_LENGTH);
	if (len != MD2_LENGTH)
		PR_fprintf(PR_STDERR, "%s: Bad hash size %d.\n", progName, len);
	MD2_DestroyContext(md2cx, PR_TRUE);
	return rv;
}

static SECStatus
md2_test(blapitestInfo *info)
{
	unsigned int len;
	MD2Context *cx = MD2_NewContext();
	SECStatus rv = SECSuccess;
	PRIntervalTime time1, time2;
	int i;
	if (!info->hash) return SECFailure;
	if (info->multihash) return md2_multi_test(info);
	if (info->in.len == 0) {
		rv = get_and_write_random_bytes(&info->in, info->bufsize, "tmp.pt");
		CHECKERROR(rv, __LINE__);
	}
	info->out.len = 16;
	info->out.data = (unsigned char *)PORT_ZAlloc(info->out.len);
	info->in.data[info->in.len] = '\0';
	TIMESTART();
	for (i=0; i<info->repetitions; i++) {
		MD2_Begin(cx);
		MD2_Update(cx, info->in.data, info->in.len);
		MD2_End(cx, info->out.data, &len, 16);
	}
	TIMEFINISH("MD2 HASH", info->in.len);
	MD2_DestroyContext(cx, PR_TRUE);
	return rv;
}

static SECStatus
sha1_multi_test(blapitestInfo *info)
{
	SECStatus rv = SECSuccess;
	SHA1Context *sha1cx;
	unsigned int len;
	SHA1Context *foosha1cx;
	unsigned char *foosha1;
	int i;
	if (info->in.len == 0) {
		rv = get_and_write_random_bytes(&info->in, info->bufsize, "tmp.pt");
		CHECKERROR(rv, __LINE__);
	}
	sha1cx = SHA1_NewContext();
	if (!sha1cx) {
		PR_fprintf(PR_STDERR, 
		   "%s:  Failed to create hash context!\n", progName);
		return SECFailure;
	}
	info->out.len = SHA1_LENGTH;
	info->out.data = (unsigned char *)PORT_ZAlloc(info->out.len);
	SHA1_Begin(sha1cx);
	for (i=0; i<info->bufsize/8; i++) {
		SHA1_Update(sha1cx, &info->in.data[i*8], 8);
		len = SHA1_FlattenSize(sha1cx);
		foosha1 = PORT_Alloc(len);
		SHA1_Flatten(sha1cx, foosha1);
		foosha1cx = SHA1_Resurrect(foosha1, NULL);
		rv = PORT_Memcmp(foosha1cx, sha1cx, len);
		if (rv != SECSuccess)
			PR_fprintf(PR_STDERR, "%s:  SHA1_Resurrect failed!\n", progName);
		SHA1_DestroyContext(foosha1cx, PR_TRUE);
		PORT_Free(foosha1);
	}
	SHA1_End(sha1cx, info->out.data, &len, SHA1_LENGTH);
	if (len != SHA1_LENGTH)
		PR_fprintf(PR_STDERR, "%s: Bad hash size %d.\n", progName, len);
	SHA1_DestroyContext(sha1cx, PR_TRUE);
	return rv;
}

static SECStatus
sha1_test(blapitestInfo *info)
{
	unsigned int len;
	SHA1Context *cx = SHA1_NewContext();
	SECStatus rv = SECSuccess;
	PRIntervalTime time1, time2;
	int i;
	if (!info->hash) return SECFailure;
	if (info->multihash) return sha1_multi_test(info);
	if (info->in.len == 0) {
		rv = get_and_write_random_bytes(&info->in, info->bufsize, "tmp.pt");
		CHECKERROR(rv, __LINE__);
	}
	info->out.len = SHA1_LENGTH;
	info->out.data = (unsigned char *)PORT_ZAlloc(info->out.len);
	info->in.data[info->in.len] = '\0';
	TIMESTART();
	for (i=info->repetitions; i>0; i--) {
		SHA1_Begin(cx);
		SHA1_Update(cx, info->in.data, info->in.len);
		SHA1_End(cx, info->out.data, &len, SHA1_LENGTH);
	}
	TIMEFINISH("SHA1 HASH", info->in.len);
	SHA1_DestroyContext(cx, PR_TRUE);
	return rv;
}

typedef SECStatus (* blapitestCryptoFn)(blapitestInfo *);

static blapitestCryptoFn crypto_fns[] =
{
	des_ecb_test,
	des_cbc_test,
	des_ede_ecb_test,
	des_ede_cbc_test,
	rc2_ecb_test,
	rc2_cbc_test,
	rc4_test,
#if NSS_SOFTOKEN_DOES_RC5
	rc5_ecb_test,
	rc5_cbc_test,
#endif
	rsa_test,
	NULL,
	pqg_test,
	dsa_test,
	NULL,
	md5_test,
	md2_test,
	sha1_test,
	NULL
};

static char *mode_strings[] =
{
	"des_ecb",
	"des_cbc",
	"des3_ecb",
	"des3_cbc",
	"rc2_ecb",
	"rc2_cbc",
	"rc4",
#if NSS_SOFTOKEN_DOES_RC5
	"rc5_ecb",
	"rc5_cbc",
#endif
	"rsa",
	"#endencrypt",
	"pqg",
	"dsa",
	"#endsign",
	"md5",
	"md2",
	"sha1",
	"#endhash"
};

static void
printmodes(blapitestInfo *info)
{
	int i = 0;
	char *mode = mode_strings[0];
	PR_fprintf(PR_STDERR, "Available modes: (specify with -m)\n", progName);
	while (mode[0] != '#') {
		if (info->encrypt || info->decrypt)
			fprintf(stderr, "%s\n", mode);
		mode = mode_strings[++i];
	}
	mode = mode_strings[++i];
	while (mode[0] != '#') {
		if (info->sign || info->verify)
			fprintf(stderr, "%s\n", mode);
		mode = mode_strings[++i];
	}
	mode = mode_strings[++i];
	while (mode[0] != '#') {
		if (info->hash)
			fprintf(stderr, "%s\n", mode);
		mode = mode_strings[++i];
	}
}

static blapitestCryptoFn
get_test_mode(const char *modestring)
{
	int i;
	int nummodes = sizeof(mode_strings) / sizeof(char *);
	for (i=0; i<nummodes; i++)
		if (PL_strcmp(modestring, mode_strings[i]) == 0)
			return crypto_fns[i];
	PR_fprintf(PR_STDERR, "%s: invalid mode: %s\n", progName, modestring);
	return NULL;
}

static void
get_params(blapitestInfo *info, char *mode, int num)
{
	SECItem *item;
	/* XXX
	 * this should use NSPR, but the string functions (strchr and atoi)
	 * barf when the commented code below is used.
	PRFileDesc *file;
	*/
	FILE *file;
	char filename[256];
	char *mark, *param, *val;
	int index = 0;
	int len;
	sprintf(filename, "%s/tests/%s/params%d", testdir, mode, num);
	/*
	file = PR_Open(filename, PR_RDONLY, 00440);
	if (file)
		SECU_FileToItem(item, file);
	else
		return;
	param = (char *)item->data;
	*/
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
			info->rounds = atoi(val);
		} else if (PL_strcmp(param, "wordsize") == 0) {
			info->wordsize = atoi(val);
		}
		index += PL_strlen(param) + PL_strlen(val) + 2;
		param = mark + 1;
	}
}

static SECStatus
get_ascii_file_data(SECItem *item, char *mode, char *type, int num)
{
	char filename[256];
	PRFileDesc *file;
	SECStatus rv;
	sprintf(filename, "%s/tests/%s/%s%d", testdir, mode, type, num);
	file = PR_Open(filename, PR_RDONLY, 00440);
	if (file) {
		rv = SECU_FileToItem(item, file);
	} else {
		/* Not a failure if "mode" does not need "type". */
		return SECSuccess;
	}
	if ((PL_strcmp(mode, "rsa") == 0 || PL_strcmp(mode, "dsa") == 0) && 
	    PL_strcmp(type, "key") == 0)
		atob(SECITEM_DupItem(item), item);
	if (PL_strcmp(mode, "dsa") == 0 && PL_strcmp(type, "plaintext") == 0)
		atob(SECITEM_DupItem(item), item);
	/* remove a trailing newline, else byte count will be wrong */
	if (item->data[item->len-1] == '\n')
		item->len--;
	PR_Close(file);
	return rv;
}

static SECStatus
blapi_selftest(char **modesToTest, int numModesToTest, 
               PRBool encrypt, PRBool decrypt)
{
	blapitestCryptoFn cryptofn;
	blapitestInfo info;
	SECItem output, asciiOut, item, inpCopy;
	SECStatus rv;
	char filename[256];
	PRFileDesc *file;
	char *mode;
	int i, j, nummodes;

	PORT_Memset(&info, 0, sizeof(info));
	info.repetitions = 1;
	info.useseed = PR_TRUE;
	info.usesigseed = PR_TRUE;
	if (modesToTest) {
		/* user gave a list of modes to test */
		nummodes = numModesToTest;
	} else {
		/* test all modes */
		nummodes = sizeof(mode_strings) / sizeof(char *);
	}
	for (i=0; i<nummodes; i++) {
		if (modesToTest) {
			mode = modesToTest[i];
		} else {
			mode = mode_strings[i];
		}
		/* skip pqg - nothing to do for self-test. */
		if (PL_strcmp(mode, "pqg") == 0)
			continue;
		cryptofn = get_test_mode(mode);
		if (mode[0] == '#') continue;
		/* get the number of tests in the directory */
		sprintf(filename, "%s/tests/%s/%s", testdir, mode, "numtests");
		file = PR_Open(filename, PR_RDONLY, 00440);
		if (!file) {
			fprintf(stderr, "File %s does not exist.\n", filename);
			return SECFailure;
		}
		rv = SECU_FileToItem(&item, file);
		PR_Close(file);
		/* loop over the tests in the directory */
		for (j=0; j<(int)(item.data[0] - '0'); j++) {
			rv = get_ascii_file_data(&info.key, mode, "key", j);
			rv = get_ascii_file_data(&info.iv, mode, "iv", j);
			rv = get_ascii_file_data(&info.in, mode, "plaintext", j);
			rv = get_ascii_file_data(&info.seed, mode, "keyseed", j);
			rv = get_ascii_file_data(&info.sigseed, mode, "sigseed", j);
			SECITEM_CopyItem(NULL, &inpCopy, &info.in);
			get_params(&info, mode, j);
			sprintf(filename, "%s/tests/%s/%s%d", testdir, mode, 
			        "ciphertext", j);
			file = PR_Open(filename, PR_RDONLY, 00440);
			rv = SECU_FileToItem(&asciiOut, file);
			PR_Close(file);
			rv = atob(&asciiOut, &output);
			if (!encrypt) goto decrypt;
			info.encrypt = info.hash = info.sign = PR_TRUE;
			(*cryptofn)(&info);
			if (SECITEM_CompareItem(&output, &info.out) != 0) {
				printf("encrypt self-test for %s failed!\n", mode);
			} /*else {
				printf("encrypt self-test for %s passed.\n", mode);
			}*/
decrypt:
			if (!decrypt) continue;
			info.encrypt = info.hash = info.sign = PR_FALSE;
			info.decrypt = info.verify = PR_TRUE;
			if (PL_strcmp(mode, "dsa") == 0) {
				if (info.out.len == 0)
				    SECITEM_CopyItem(NULL, &info.out, &output);
				rv = (*cryptofn)(&info);
				if (rv != SECSuccess) {
					printf("signature self-test for %s failed!\n", mode);
				} /*else {
					printf("signature self-test for %s passed.\n", mode);
				}*/
			} else {
				SECITEM_ZfreeItem(&info.in, PR_FALSE);
				SECITEM_ZfreeItem(&info.out, PR_FALSE);
				SECITEM_CopyItem(NULL, &info.in, &output);
				info.out.len = 0;
				rv = (*cryptofn)(&info);
				if (rv == SECSuccess) {
					if (SECITEM_CompareItem(&inpCopy, &info.out) != 0) {
						printf("decrypt self-test for %s failed!\n", mode);
					} /*else {
						printf("decrypt self-test for %s passed.\n", mode);
					}*/
				}
			}
		}
	}
	return SECSuccess;
}

static SECStatus
get_file_data(char *filename, SECItem *item, PRBool b64) 
{
	SECStatus rv = SECSuccess;
	PRFileDesc *file = PR_Open(filename, PR_RDONLY, 006600);
	if (file) {
		SECItem asciiItem;
		rv = SECU_FileToItem(&asciiItem, file);
		CHECKERROR(rv, __LINE__);
		if (b64) {
			rv = atob(&asciiItem, item);
		} else {
			SECITEM_CopyItem(NULL, item, &asciiItem);
			if (item->data[item->len-1] == '\n')
				item->len--;
		}
		CHECKERROR(rv, __LINE__);
		PR_Close(file);
	}
	return rv;
}

SECStatus
dump_file(char *mode, char *filename)
{
	SECItem item;
	if (PL_strcmp(mode, "rsa") == 0) {
	} else if (PL_strcmp(mode, "pqg") == 0) {
		PQGParams *pqg;
		get_file_data(filename, &item, PR_TRUE);
		pqg = pqg_from_filedata(&item);
		dump_pqg(pqg);
	} else if (PL_strcmp(mode, "dsa") == 0) {
		DSAPrivateKey *key;
		get_file_data(filename, &item, PR_TRUE);
		key = dsakey_from_filedata(&item);
		dump_dsakey(key);
	}
	return SECFailure;
}

int main(int argc, char **argv) 
{
	char *infile, *outfile, *keyfile, *ivfile, *sigfile, *seedfile,
	     *sigseedfile, *pqgfile;
	PRBool b64 = PR_TRUE;
	blapitestInfo info;
	blapitestCryptoFn cryptofn = NULL;
	PLOptState *optstate;
	PLOptStatus status;
	PRBool dofips = PR_FALSE;
	PRBool doselftest = PR_FALSE;
	PRBool zerobuffer = PR_FALSE;
	char *dumpfile = NULL;
	char *mode = NULL;
	char *modesToTest[20];
	int numModesToTest = 0;
	SECStatus rv;

	PORT_Memset(&info, 0, sizeof(info));
	info.bufsize = 8;
	info.keysize = DES_KEY_LENGTH;
	info.rsapubexp = 17;
	info.rounds = 10;
	info.wordsize = 4;
	infile=outfile=keyfile=pqgfile=ivfile=sigfile=seedfile=sigseedfile=NULL;
	info.repetitions = 1;
    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];
	optstate = PL_CreateOptState(argc, argv, 
	    "DEFHP:STVab:d:ce:g:j:i:o:p:k:m:t:qr:s:v:w:xyz:");
	while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
		switch (optstate->option) {
		case 'D':  info.decrypt = PR_TRUE; break;
		case 'E':  info.encrypt = PR_TRUE; break;
		case 'F':  dofips = PR_TRUE; break;
		case 'H':  info.hash = PR_TRUE; break;
		case 'P':  dumpfile = PL_strdup(optstate->value); break;
		case 'S':  info.sign = PR_TRUE; break;
		case 'T':  doselftest = PR_TRUE; break;
		case 'V':  info.verify = PR_TRUE; break;
		case 'a':  b64 = PR_FALSE; break;
		case 'b':  info.bufsize = PORT_Atoi(optstate->value); break;
		case 'c':  info.multihash = PR_TRUE; break;
		case 'd':  testdir = PL_strdup(optstate->value); break;
		case 'e':  info.rsapubexp = PORT_Atoi(optstate->value); break;
		case 'g':  info.keysize = PORT_Atoi(optstate->value); break;
		case 'i':  infile  = PL_strdup(optstate->value); break;
		case 'j':  pqgfile = PL_strdup(optstate->value); break;
		case 'k':  keyfile = PL_strdup(optstate->value); break;
		case 'm':  cryptofn = get_test_mode(optstate->value);
		           mode = PL_strdup(optstate->value); 
		           break;
		case 'o':  outfile = PL_strdup(optstate->value); break;
		case 'p':  info.performance = PR_TRUE; 
			   info.repetitions = PORT_Atoi(optstate->value); 
			   break;
		case 'q':  zerobuffer = PR_TRUE; break;
		case 'r':  info.rounds = PORT_Atoi(optstate->value); break;
		case 's':  sigfile  = PL_strdup(optstate->value); break;
		case 't':  sigseedfile = PL_strdup(optstate->value); break;
		case 'v':  ivfile  = PL_strdup(optstate->value); break;
		case 'w':  info.wordsize = PORT_Atoi(optstate->value); break;
		case 'x':  info.useseed = PR_TRUE; break;
		case 'y':  info.usesigseed = PR_TRUE; break;
		case 'z':  seedfile  = PL_strdup(optstate->value); break;
		case '\0': if (optstate->value[0] != '-')
					   modesToTest[numModesToTest++] = 
						PL_strdup(optstate->value);
				   break;
		default: break;
		}
	}

	if (dumpfile)
		return dump_file(mode, dumpfile);

	if (doselftest) {
		if (!info.encrypt && !info.decrypt) {
			info.encrypt = info.decrypt = PR_TRUE;  /* do both */
		}
		if (numModesToTest > 0) {
			return blapi_selftest(modesToTest, numModesToTest,
			                      info.encrypt, info.decrypt);
		} else {
			return blapi_selftest(NULL, 0, info.encrypt, info.decrypt);
		}
	}

	if (dofips) {
		CK_RV foo = pk11_fipsPowerUpSelfTest();
		PR_fprintf(PR_STDOUT, "CK_RV: %d.\n", foo);
		return 0;
	}

	if (!info.encrypt && !info.decrypt && !info.hash && 
	    !info.sign && !info.verify)
		Usage();

	if (!cryptofn) {
		printmodes(&info);
		return -1;
	}

	if (info.decrypt && !infile)
		Usage();

	if (info.performance) {
		char buf[256];
		PRStatus stat;
		stat = PR_GetSystemInfo(PR_SI_HOSTNAME, buf, sizeof(buf));
		printf("HOST: %s\n", buf);
		stat = PR_GetSystemInfo(PR_SI_SYSNAME, buf, sizeof(buf));
		printf("SYSTEM: %s\n", buf);
		stat = PR_GetSystemInfo(PR_SI_RELEASE, buf, sizeof(buf));
		printf("RELEASE: %s\n", buf);
		stat = PR_GetSystemInfo(PR_SI_ARCHITECTURE, buf, sizeof(buf));
		printf("ARCH: %s\n", buf);
	}

	RNG_RNGInit();
	RNG_SystemInfoForRNG();

	if (keyfile) {
		/* RSA and DSA keys are always b64 encoded. */
		if (b64 || PL_strcmp(mode,"rsa")==0 || PL_strcmp(mode,"dsa")==0)
			get_file_data(keyfile, &info.key, PR_TRUE);
		else
			get_file_data(keyfile, &info.key, b64);
	}
	if (ivfile)
		get_file_data(ivfile, &info.iv, b64);
	if (infile)
		get_file_data(infile, &info.in, b64);
	if (sigfile)
		get_file_data(sigfile, &info.out, PR_TRUE);
	if (seedfile) {
		get_file_data(seedfile, &info.seed, b64);
		info.useseed = PR_TRUE;
	}
	if (sigseedfile) {
		get_file_data(sigseedfile, &info.sigseed, b64);
		info.usesigseed = PR_TRUE;
	}
	if (pqgfile)
		get_file_data(pqgfile, &info.pqg, PR_TRUE);

	if (zerobuffer) {
		PRFileDesc *ifile;
		info.in.len = info.bufsize;
		info.in.data = PORT_ZAlloc(info.in.len);
		ifile = PR_Open("tmp.pt", PR_WRONLY|PR_CREATE_FILE, 00660);
		rv = btoa_file(&info.in, ifile);
		CHECKERROR((rv < 0), __LINE__);
		PR_Close(ifile);
	}

	rv = (*cryptofn)(&info);
		CHECKERROR(rv, __LINE__);

	if (!sigfile && info.out.len > 0) {
		PRFileDesc *ofile;
		if (!outfile)
			ofile = PR_Open("tmp.out", PR_WRONLY|PR_CREATE_FILE, 00660);
		else
			ofile = PR_Open(outfile, PR_WRONLY|PR_CREATE_FILE, 00660);
		rv = btoa_file(&info.out, ofile);
		PR_Close(ofile);
		CHECKERROR((rv < 0), __LINE__);
	}

	RNG_RNGShutdown();

	return SECSuccess;
}
