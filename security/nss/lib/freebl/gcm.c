/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif
#include "blapii.h"
#include "blapit.h"
#include "gcm.h"
#include "ctr.h"
#include "secerr.h"
#include "prtypes.h"
#include "pkcs11t.h"

#include <limits.h>

/**************************************************************************
 *          First implement the Galois hash function of GCM (gcmHash)     *
 **************************************************************************/
#define GCM_HASH_LEN_LEN 8 /* gcm hash defines lengths to be 64 bits */

typedef struct gcmHashContextStr gcmHashContext;

static SECStatus gcmHash_InitContext(gcmHashContext *hash,
				     const unsigned char *H,
				     unsigned int blocksize);
static void gcmHash_DestroyContext(gcmHashContext *ghash, PRBool freeit);
static SECStatus gcmHash_Update(gcmHashContext *ghash,
				const unsigned char *buf, unsigned int len,
				unsigned int blocksize);
static SECStatus gcmHash_Sync(gcmHashContext *ghash, unsigned int blocksize);
static SECStatus gcmHash_Final(gcmHashContext *gcm, unsigned char *outbuf,
			       unsigned int *outlen, unsigned int maxout,
			       unsigned int blocksize);
static SECStatus gcmHash_Reset(gcmHashContext *ghash,
			       const unsigned char *inbuf,
			       unsigned int inbufLen, unsigned int blocksize);

/* compile time defines to select how the GF2 multiply is calculated.
 * There are currently 2 algorithms implemented here: MPI and ALGORITHM_1.
 *
 * MPI uses the GF2m implemented in mpi to support GF2 ECC.
 * ALGORITHM_1 is the Algorithm 1 in both NIST SP 800-38D and
 * "The Galois/Counter Mode of Operation (GCM)", McGrew & Viega.
 */
#if !defined(GCM_USE_ALGORITHM_1) && !defined(GCM_USE_MPI)
#define GCM_USE_MPI 1 /* MPI is about 5x faster with the
		       * same or less complexity. It's possible to use
		       * tables to speed things up even more */
#endif

/* GCM defines the bit string to be LSB first, which is exactly
 * opposite everyone else, including hardware. build array
 * to reverse everything. */
static const unsigned char gcm_byte_rev[256] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};


#ifdef GCM_TRACE
#include <stdio.h>

#define GCM_TRACE_X(ghash,label) { \
	unsigned char _X[MAX_BLOCK_SIZE]; int i; \
	gcm_getX(ghash, _X, blocksize); \
	printf(label,(ghash)->m); \
	for (i=0; i < blocksize; i++) printf("%02x",_X[i]); \
	printf("\n"); }
#define GCM_TRACE_BLOCK(label,buf,blocksize) {\
	printf(label); \
	for (i=0; i < blocksize; i++) printf("%02x",buf[i]); \
	printf("\n"); }
#else
#define GCM_TRACE_X(ghash,label)
#define GCM_TRACE_BLOCK(label,buf,blocksize)
#endif

#ifdef GCM_USE_MPI

#ifdef GCM_USE_ALGORITHM_1
#error "Only define one of GCM_USE_MPI, GCM_USE_ALGORITHM_1"
#endif
/* use the MPI functions to calculate Xn = (Xn-1^C_i)*H mod poly */
#include "mpi.h"
#include "secmpi.h"
#include "mplogic.h"
#include "mp_gf2m.h"

/* state needed to handle GCM Hash function */
struct gcmHashContextStr {
     mp_int H;
     mp_int X;
     mp_int C_i;
     const unsigned int *poly;
     unsigned char buffer[MAX_BLOCK_SIZE];
     unsigned int bufLen;
     int m; /* XXX what is m? */
     unsigned char counterBuf[2*GCM_HASH_LEN_LEN];
     PRUint64 cLen;
};

/* f = x^128 + x^7 + x^2 + x + 1 */
static const unsigned int poly_128[] = { 128, 7, 2, 1, 0 };

/* sigh, GCM defines the bit strings exactly backwards from everything else */
static void
gcm_reverse(unsigned char *target, const unsigned char *src,
							unsigned int blocksize)
{
    unsigned int i;
    for (i=0; i < blocksize; i++) {
	target[blocksize-i-1] = gcm_byte_rev[src[i]];
    }
}

/* Initialize a gcmHashContext */
static SECStatus
gcmHash_InitContext(gcmHashContext *ghash, const unsigned char *H,
		    unsigned int blocksize)
{
    mp_err err = MP_OKAY;
    unsigned char H_rev[MAX_BLOCK_SIZE];

    MP_DIGITS(&ghash->H) = 0;
    MP_DIGITS(&ghash->X) = 0;
    MP_DIGITS(&ghash->C_i) = 0;
    CHECK_MPI_OK( mp_init(&ghash->H) );
    CHECK_MPI_OK( mp_init(&ghash->X) );
    CHECK_MPI_OK( mp_init(&ghash->C_i) );

    mp_zero(&ghash->X);
    gcm_reverse(H_rev, H, blocksize);
    CHECK_MPI_OK( mp_read_unsigned_octets(&ghash->H, H_rev, blocksize) );

    /* set the irreducible polynomial. Each blocksize has its own polynomial.
     * for now only blocksize 16 (=128 bits) is defined */
    switch (blocksize) {
    case 16: /* 128 bits */
	ghash->poly = poly_128;
	break;
    default:
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	goto cleanup;
    }
    ghash->cLen = 0;
    ghash->bufLen = 0;
    ghash->m = 0;
    PORT_Memset(ghash->counterBuf, 0, sizeof(ghash->counterBuf));
    return SECSuccess;
cleanup:
    gcmHash_DestroyContext(ghash, PR_FALSE);
    return SECFailure;
}

/* Destroy a HashContext (Note we zero the digits so this function
 * is idempotent if called with freeit == PR_FALSE */
static void
gcmHash_DestroyContext(gcmHashContext *ghash, PRBool freeit)
{
    mp_clear(&ghash->H);
    mp_clear(&ghash->X);
    mp_clear(&ghash->C_i);
    MP_DIGITS(&ghash->H) = 0;
    MP_DIGITS(&ghash->X) = 0;
    MP_DIGITS(&ghash->C_i) = 0;
    if (freeit) {
	PORT_Free(ghash);
    }
}

static SECStatus
gcm_getX(gcmHashContext *ghash, unsigned char *T, unsigned int blocksize)
{
    int len;
    mp_err err;
    unsigned char tmp_buf[MAX_BLOCK_SIZE];
    unsigned char *X;

    len = mp_unsigned_octet_size(&ghash->X);
    if (len <= 0) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return SECFailure;
    }
    X = tmp_buf;
    PORT_Assert((unsigned int)len <= blocksize);
    if ((unsigned int)len > blocksize) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return SECFailure;
    }
    /* zero pad the result */
    if (len != blocksize) {
	PORT_Memset(X,0,blocksize-len);
	X += blocksize-len;
    }

    err = mp_to_unsigned_octets(&ghash->X, X, len);
    if (err < 0) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return SECFailure;
    }
    gcm_reverse(T, tmp_buf, blocksize);
    return SECSuccess;
}

static SECStatus
gcm_HashMult(gcmHashContext *ghash, const unsigned char *buf,
		unsigned int count, unsigned int blocksize)
{
    SECStatus rv = SECFailure;
    mp_err err = MP_OKAY;
    unsigned char tmp_buf[MAX_BLOCK_SIZE];
    unsigned int i;

    for (i=0; i < count; i++, buf += blocksize) {
	ghash->m++;
	gcm_reverse(tmp_buf, buf, blocksize);
	CHECK_MPI_OK(mp_read_unsigned_octets(&ghash->C_i, tmp_buf, blocksize));
	CHECK_MPI_OK(mp_badd(&ghash->X, &ghash->C_i, &ghash->C_i));
	/*
	 * Looking to speed up GCM, this the the place to do it.
	 * There are two areas that can be exploited to speed up this code.
	 *
	 * 1) H is a constant in this multiply. We can precompute H * (0 - 255)
	 * at init time and this becomes an blockize xors of our table lookup.
	 *
	 * 2) poly is a constant for each blocksize. We can calculate the
	 * modulo reduction by a series of adds and shifts.
	 *
	 * For now we are after functionality, so we will go ahead and use
	 * the builtin bmulmod from mpi
	 */
        CHECK_MPI_OK(mp_bmulmod(&ghash->C_i, &ghash->H,
					ghash->poly, &ghash->X));
	GCM_TRACE_X(ghash, "X%d = ")
    }
    rv = SECSuccess;
cleanup:
    if (rv != SECSuccess) {
	MP_TO_SEC_ERROR(err);
    }
    return rv;
}

static void
gcm_zeroX(gcmHashContext *ghash)
{
    mp_zero(&ghash->X);
    ghash->m = 0;
}

#endif

#ifdef GCM_USE_ALGORITHM_1
/* use algorithm 1 of McGrew & Viega "The Galois/Counter Mode of Operation" */

#define GCM_ARRAY_SIZE (MAX_BLOCK_SIZE/sizeof(unsigned long))

struct gcmHashContextStr {
     unsigned long H[GCM_ARRAY_SIZE];
     unsigned long X[GCM_ARRAY_SIZE];
     unsigned long R;
     unsigned char buffer[MAX_BLOCK_SIZE];
     unsigned int bufLen;
     int m;
     unsigned char counterBuf[2*GCM_HASH_LEN_LEN];
     PRUint64 cLen;
};

static void
gcm_bytes_to_longs(unsigned long *l, const unsigned char *c, unsigned int len)
{
    int i,j;
    int array_size = len/sizeof(unsigned long);

    PORT_Assert(len % sizeof(unsigned long) == 0);
    for (i=0; i < array_size; i++) {
	unsigned long tmp = 0;
	int byte_offset = i * sizeof(unsigned long);
	for (j=sizeof(unsigned long)-1; j >= 0; j--) {
	    tmp = (tmp << PR_BITS_PER_BYTE) | gcm_byte_rev[c[byte_offset+j]];
	}
	l[i] = tmp;
    }
}

static void
gcm_longs_to_bytes(const unsigned long *l, unsigned char *c, unsigned int len)
{
    int i,j;
    int array_size = len/sizeof(unsigned long);

    PORT_Assert(len % sizeof(unsigned long) == 0);
    for (i=0; i < array_size; i++) {
	unsigned long tmp = l[i];
	int byte_offset = i * sizeof(unsigned long);
	for (j=0; j < sizeof(unsigned long); j++) {
	    c[byte_offset+j] = gcm_byte_rev[tmp & 0xff];
	    tmp = (tmp >> PR_BITS_PER_BYTE);
	}
    }
}


/* Initialize a gcmHashContext */
static SECStatus
gcmHash_InitContext(gcmHashContext *ghash, const unsigned char *H,
		    unsigned int blocksize)
{
    PORT_Memset(ghash->X, 0, sizeof(ghash->X));
    PORT_Memset(ghash->H, 0, sizeof(ghash->H));
    gcm_bytes_to_longs(ghash->H, H, blocksize);

    /* set the irreducible polynomial. Each blocksize has its own polynommial
     * for now only blocksize 16 (=128 bits) is defined */
    switch (blocksize) {
    case 16: /* 128 bits */
	ghash->R = (unsigned long) 0x87; /* x^7 + x^2 + x +1 */
	break;
    default:
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	goto cleanup;
    }
    ghash->cLen = 0;
    ghash->bufLen = 0;
    ghash->m = 0;
    PORT_Memset(ghash->counterBuf, 0, sizeof(ghash->counterBuf));
    return SECSuccess;
cleanup:
    return SECFailure;
}

/* Destroy a HashContext (Note we zero the digits so this function
 * is idempotent if called with freeit == PR_FALSE */
static void
gcmHash_DestroyContext(gcmHashContext *ghash, PRBool freeit)
{
    if (freeit) {
	PORT_Free(ghash);
    }
}

static unsigned long
gcm_shift_one(unsigned long *t, unsigned int count)
{
    unsigned long carry = 0;
    unsigned long nextcarry = 0;
    unsigned int i;
    for (i=0; i < count; i++) {
	nextcarry = t[i] >> ((sizeof(unsigned long)*PR_BITS_PER_BYTE)-1);
	t[i] = (t[i] << 1) | carry;
	carry = nextcarry;
    }
    return carry;
}

static SECStatus
gcm_getX(gcmHashContext *ghash, unsigned char *T, unsigned int blocksize)
{
    gcm_longs_to_bytes(ghash->X, T, blocksize);
    return SECSuccess;
}

#define GCM_XOR(t, s, len) \
	for (l=0; l < len; l++) t[l] ^= s[l]

static SECStatus
gcm_HashMult(gcmHashContext *ghash, const unsigned char *buf,
		unsigned int count, unsigned int blocksize)
{
    unsigned long C_i[GCM_ARRAY_SIZE];
    unsigned int arraysize = blocksize/sizeof(unsigned long);
    unsigned int i, j, k, l;

    for (i=0; i < count; i++, buf += blocksize) {
	ghash->m++;
	gcm_bytes_to_longs(C_i, buf, blocksize);
	GCM_XOR(C_i, ghash->X, arraysize);
	/* multiply X = C_i * H */
	PORT_Memset(ghash->X, 0, sizeof(ghash->X));
	for (j=0; j < arraysize; j++) {
	    unsigned long H = ghash->H[j];
	    for (k=0; k < sizeof(unsigned long)*PR_BITS_PER_BYTE; k++) {
		if (H & 1) {
		    GCM_XOR(ghash->X, C_i, arraysize);
		}
		if (gcm_shift_one(C_i, arraysize)) {
		    C_i[0] = C_i[0] ^ ghash->R;
		}
		H = H >> 1;
	    }
	}
	GCM_TRACE_X(ghash, "X%d = ")
    }
    return SECSuccess;
}


static void
gcm_zeroX(gcmHashContext *ghash)
{
    PORT_Memset(ghash->X, 0, sizeof(ghash->X));
    ghash->m = 0;
}
#endif

/*
 * implement GCM GHASH using the freebl GHASH function. The gcm_HashMult
 * function always takes blocksize lengths of data. gcmHash_Update will
 * format the data properly.
 */
static SECStatus
gcmHash_Update(gcmHashContext *ghash, const unsigned char *buf,
	       unsigned int len, unsigned int blocksize)
{
    unsigned int blocks;
    SECStatus rv;

    ghash->cLen += (len*PR_BITS_PER_BYTE);

    /* first deal with the current buffer of data. Try to fill it out so
     * we can hash it */
    if (ghash->bufLen) {
	unsigned int needed = PR_MIN(len, blocksize - ghash->bufLen);
	if (needed != 0) {
	    PORT_Memcpy(ghash->buffer+ghash->bufLen, buf, needed);
	}
	buf += needed;
	len -= needed;
	ghash->bufLen += needed;
	if (len == 0) {
	    /* didn't add enough to hash the data, nothing more do do */
	    return SECSuccess;
	}
	PORT_Assert(ghash->bufLen == blocksize);
	/* hash the buffer and clear it */
	rv = gcm_HashMult(ghash, ghash->buffer, 1, blocksize);
	PORT_Memset(ghash->buffer, 0, blocksize);
	ghash->bufLen = 0;
	if (rv != SECSuccess) {
	    return SECFailure;
	}
    }
    /* now hash any full blocks remaining in the data stream */
    blocks = len/blocksize;
    if (blocks) {
	rv = gcm_HashMult(ghash, buf, blocks, blocksize);
	if (rv != SECSuccess) {
	    return SECFailure;
	}
	buf += blocks*blocksize;
	len -= blocks*blocksize;
    }

    /* save any remainder in the buffer to be hashed with the next call */
    if (len != 0) {
	PORT_Memcpy(ghash->buffer, buf, len);
	ghash->bufLen = len;
    }
    return SECSuccess;
}

/*
 * write out any partial blocks zero padded through the GHASH engine,
 * save the lengths for the final completion of the hash
 */
static SECStatus
gcmHash_Sync(gcmHashContext *ghash, unsigned int blocksize)
{
    int i;
    SECStatus rv;

    /* copy the previous counter to the upper block */
    PORT_Memcpy(ghash->counterBuf, &ghash->counterBuf[GCM_HASH_LEN_LEN],
							GCM_HASH_LEN_LEN);
    /* copy the current counter in the lower block */
    for (i=0; i < GCM_HASH_LEN_LEN; i++) {
	ghash->counterBuf[GCM_HASH_LEN_LEN+i] =
	    (ghash->cLen >> ((GCM_HASH_LEN_LEN-1-i)*PR_BITS_PER_BYTE)) & 0xff;
    }
    ghash->cLen = 0;

    /* now zero fill the buffer and hash the last block */
    if (ghash->bufLen) {
	PORT_Memset(ghash->buffer+ghash->bufLen, 0, blocksize - ghash->bufLen);
	rv = gcm_HashMult(ghash, ghash->buffer, 1, blocksize);
	PORT_Memset(ghash->buffer, 0, blocksize);
	ghash->bufLen = 0;
	if (rv != SECSuccess) {
	    return SECFailure;
	}
    }
    return SECSuccess;
}

/*
 * This does the final sync, hashes the lengths, then returns
 * "T", the hashed output.
 */
static SECStatus
gcmHash_Final(gcmHashContext *ghash, unsigned char *outbuf,
		unsigned int *outlen, unsigned int maxout,
		unsigned int blocksize)
{
    unsigned char T[MAX_BLOCK_SIZE];
    SECStatus rv;

    rv = gcmHash_Sync(ghash, blocksize);
    if (rv != SECSuccess) {
	return SECFailure;
    }

    rv = gcm_HashMult(ghash, ghash->counterBuf, (GCM_HASH_LEN_LEN*2)/blocksize,
								blocksize);
    if (rv != SECSuccess) {
	return SECFailure;
    }

    GCM_TRACE_X(ghash, "GHASH(H,A,C) = ")

    rv = gcm_getX(ghash, T, blocksize);
    if (rv != SECSuccess) {
	return SECFailure;
    }

    if (maxout > blocksize) maxout = blocksize;
    PORT_Memcpy(outbuf, T, maxout);
    *outlen = maxout;
    return SECSuccess;
}

SECStatus
gcmHash_Reset(gcmHashContext *ghash, const unsigned char *AAD,
	      unsigned int AADLen, unsigned int blocksize)
{
    SECStatus rv;

    ghash->cLen = 0;
    PORT_Memset(ghash->counterBuf, 0, GCM_HASH_LEN_LEN*2);
    ghash->bufLen = 0;
    gcm_zeroX(ghash);

    /* now kick things off by hashing the Additional Authenticated Data */
    if (AADLen != 0) {
	rv = gcmHash_Update(ghash, AAD, AADLen, blocksize);
	if (rv != SECSuccess) {
	    return SECFailure;
	}
	rv = gcmHash_Sync(ghash, blocksize);
	if (rv != SECSuccess) {
	    return SECFailure;
	}
    }
    return SECSuccess;
}

/**************************************************************************
 *           Now implement the GCM using gcmHash and CTR                  *
 **************************************************************************/

/* state to handle the full GCM operation (hash and counter) */
struct GCMContextStr {
    gcmHashContext ghash_context;
    CTRContext ctr_context;
    unsigned long tagBits;
    unsigned char tagKey[MAX_BLOCK_SIZE];
};

GCMContext *
GCM_CreateContext(void *context, freeblCipherFunc cipher,
		  const unsigned char *params, unsigned int blocksize)
{
    GCMContext *gcm = NULL;
    gcmHashContext *ghash;
    unsigned char H[MAX_BLOCK_SIZE];
    unsigned int tmp;
    PRBool freeCtr = PR_FALSE;
    PRBool freeHash = PR_FALSE;
    const CK_GCM_PARAMS *gcmParams = (const CK_GCM_PARAMS *)params;
    CK_AES_CTR_PARAMS ctrParams;
    SECStatus rv;

    if (blocksize > MAX_BLOCK_SIZE || blocksize > sizeof(ctrParams.cb)) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return NULL;
    }
    gcm = PORT_ZNew(GCMContext);
    if (gcm == NULL) {
	return NULL;
    }
    /* first fill in the ghash context */
    ghash = &gcm->ghash_context;
    PORT_Memset(H, 0, blocksize);
    rv = (*cipher)(context, H, &tmp, blocksize, H, blocksize, blocksize);
    if (rv != SECSuccess) {
	goto loser;
    }
    rv = gcmHash_InitContext(ghash, H, blocksize);
    if (rv != SECSuccess) {
	goto loser;
    }
    freeHash = PR_TRUE;

    /* fill in the Counter context */
    ctrParams.ulCounterBits = 32;
    PORT_Memset(ctrParams.cb, 0, sizeof(ctrParams.cb));
    if ((blocksize == 16) && (gcmParams->ulIvLen == 12)) {
	PORT_Memcpy(ctrParams.cb, gcmParams->pIv, gcmParams->ulIvLen);
	ctrParams.cb[blocksize-1] = 1;
    } else {
	rv = gcmHash_Update(ghash, gcmParams->pIv, gcmParams->ulIvLen,
			    blocksize);
	if (rv != SECSuccess) {
	    goto loser;
	}
	rv = gcmHash_Final(ghash, ctrParams.cb, &tmp, blocksize, blocksize);
	if (rv != SECSuccess) {
	    goto loser;
	}
    }
    rv = CTR_InitContext(&gcm->ctr_context, context, cipher,
				(unsigned char *)&ctrParams, blocksize);
    if (rv != SECSuccess) {
	goto loser;
    }
    freeCtr = PR_TRUE;

    /* fill in the gcm structure */
    gcm->tagBits = gcmParams->ulTagBits; /* save for final step */
    /* calculate the final tag key. NOTE: gcm->tagKey is zero to start with.
     * if this assumption changes, we would need to explicitly clear it here */
    rv = CTR_Update(&gcm->ctr_context, gcm->tagKey, &tmp, blocksize,
		    gcm->tagKey, blocksize, blocksize);
    if (rv != SECSuccess) {
	goto loser;
    }

    /* finally mix in the AAD data */
    rv = gcmHash_Reset(ghash, gcmParams->pAAD, gcmParams->ulAADLen, blocksize);
    if (rv != SECSuccess) {
	goto loser;
    }

    return gcm;

loser:
    if (freeCtr) {
	CTR_DestroyContext(&gcm->ctr_context, PR_FALSE);
    }
    if (freeHash) {
	gcmHash_DestroyContext(&gcm->ghash_context, PR_FALSE);
    }
    if (gcm) {
	PORT_Free(gcm);
    }
    return NULL;
}

void
GCM_DestroyContext(GCMContext *gcm, PRBool freeit)
{
    /* these two are statically allocated and will be freed when we free
     * gcm. call their destroy functions to free up any locally
     * allocated data (like mp_int's) */
    CTR_DestroyContext(&gcm->ctr_context, PR_FALSE);
    gcmHash_DestroyContext(&gcm->ghash_context, PR_FALSE);
    if (freeit) {
	PORT_Free(gcm);
    }
}

static SECStatus
gcm_GetTag(GCMContext *gcm, unsigned char *outbuf,
	unsigned int *outlen, unsigned int maxout,
	unsigned int blocksize)
{
    unsigned int tagBytes;
    unsigned int extra;
    unsigned int i;
    SECStatus rv;

    tagBytes = (gcm->tagBits + (PR_BITS_PER_BYTE-1)) / PR_BITS_PER_BYTE;
    extra = tagBytes*PR_BITS_PER_BYTE - gcm->tagBits;

    if (outbuf == NULL) {
	*outlen = tagBytes;
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	return SECFailure;
    }

    if (maxout < tagBytes) {
	*outlen = tagBytes;
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	return SECFailure;
    }
    maxout = tagBytes;
    rv = gcmHash_Final(&gcm->ghash_context, outbuf, outlen, maxout, blocksize);
    if (rv != SECSuccess) {
	return SECFailure;
    }

    GCM_TRACE_BLOCK("GHASH=", outbuf, blocksize);
    GCM_TRACE_BLOCK("Y0=", gcm->tagKey, blocksize);
    for (i=0; i < *outlen; i++) {
	outbuf[i] ^= gcm->tagKey[i];
    }
    GCM_TRACE_BLOCK("Y0=", gcm->tagKey, blocksize);
    GCM_TRACE_BLOCK("T=", outbuf, blocksize);
    /* mask off any extra bits we got */
    if (extra) {
	outbuf[tagBytes-1] &= ~((1 << extra)-1);
    }
    return SECSuccess;
}


/*
 * See The Galois/Counter Mode of Operation, McGrew and Viega.
 *  GCM is basically counter mode with a specific initialization and
 *  built in macing operation.
 */
SECStatus
GCM_EncryptUpdate(GCMContext *gcm, unsigned char *outbuf,
		unsigned int *outlen, unsigned int maxout,
		const unsigned char *inbuf, unsigned int inlen,
		unsigned int blocksize)
{
    SECStatus rv;
    unsigned int tagBytes;
    unsigned int len;

    tagBytes = (gcm->tagBits + (PR_BITS_PER_BYTE-1)) / PR_BITS_PER_BYTE;
    if (UINT_MAX - inlen < tagBytes) {
	PORT_SetError(SEC_ERROR_INPUT_LEN);
	return SECFailure;
    }
    if (maxout < inlen + tagBytes) {
	*outlen = inlen + tagBytes;
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	return SECFailure;
    }

    rv = CTR_Update(&gcm->ctr_context, outbuf, outlen, maxout,
			inbuf, inlen, blocksize);
    if (rv != SECSuccess) {
	return SECFailure;
    }
    rv = gcmHash_Update(&gcm->ghash_context, outbuf, *outlen, blocksize);
    if (rv != SECSuccess) {
	PORT_Memset(outbuf, 0, *outlen); /* clear the output buffer */
	*outlen = 0;
	return SECFailure;
    }
    rv = gcm_GetTag(gcm, outbuf + *outlen, &len, maxout - *outlen, blocksize);
    if (rv != SECSuccess) {
	PORT_Memset(outbuf, 0, *outlen); /* clear the output buffer */
	*outlen = 0;
	return SECFailure;
    };
    *outlen += len;
    return SECSuccess;
}

/*
 * See The Galois/Counter Mode of Operation, McGrew and Viega.
 *  GCM is basically counter mode with a specific initialization and
 *  built in macing operation. NOTE: the only difference between Encrypt
 *  and Decrypt is when we calculate the mac. That is because the mac must
 *  always be calculated on the cipher text, not the plain text, so for
 *  encrypt, we do the CTR update first and for decrypt we do the mac first.
 */
SECStatus
GCM_DecryptUpdate(GCMContext *gcm, unsigned char *outbuf,
		unsigned int *outlen, unsigned  int maxout,
		const unsigned char *inbuf, unsigned int inlen,
		unsigned int blocksize)
{
    SECStatus rv;
    unsigned int tagBytes;
    unsigned char tag[MAX_BLOCK_SIZE];
    const unsigned char *intag;
    unsigned int len;

    tagBytes = (gcm->tagBits + (PR_BITS_PER_BYTE-1)) / PR_BITS_PER_BYTE;

    /* get the authentication block */
    if (inlen < tagBytes) {
	PORT_SetError(SEC_ERROR_INPUT_LEN);
	return SECFailure;
    }

    inlen -= tagBytes;
    intag = inbuf + inlen;

    /* verify the block */
    rv = gcmHash_Update(&gcm->ghash_context, inbuf, inlen, blocksize);
    if (rv != SECSuccess) {
	return SECFailure;
    }
    rv = gcm_GetTag(gcm, tag, &len, blocksize, blocksize);
    if (rv != SECSuccess) {
	return SECFailure;
    }
    /* Don't decrypt if we can't authenticate the encrypted data!
     * This assumes that if tagBits is not a multiple of 8, intag will
     * preserve the masked off missing bits.  */
    if (NSS_SecureMemcmp(tag, intag, tagBytes) != 0) {
	/* force a CKR_ENCRYPTED_DATA_INVALID error at in softoken */
	PORT_SetError(SEC_ERROR_BAD_DATA);
	return SECFailure;
    }
    /* finish the decryption */
    return CTR_Update(&gcm->ctr_context, outbuf, outlen, maxout,
			  inbuf, inlen, blocksize);
}
