/* arcfour.c - the arc four algorithm.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "prerr.h"
#include "secerr.h"

#include "prtypes.h"
#include "blapi.h"

/* Architecture-dependent defines */

#if defined(SOLARIS) || defined(HPUX) || defined(NSS_X86) || \
    defined(_WIN64)
/* Convert the byte-stream to a word-stream */
#define CONVERT_TO_WORDS
#endif

#if defined(AIX) || defined(OSF1) || defined(NSS_BEVAND_ARCFOUR)
/* Treat array variables as words, not bytes, on CPUs that take 
 * much longer to write bytes than to write words, or when using 
 * assembler code that required it.
 */
#define USE_WORD
#endif

#if defined(IS_64) || defined(NSS_BEVAND_ARCFOUR)
typedef PRUint64 WORD;
#else
typedef PRUint32 WORD;
#endif
#define WORDSIZE sizeof(WORD)

#if defined(USE_WORD)
typedef WORD Stype;
#else
typedef PRUint8 Stype;
#endif

#define ARCFOUR_STATE_SIZE 256

#define MASK1BYTE (WORD)(0xff)

#define SWAP(a, b) \
	tmp = a; \
	a = b; \
	b = tmp;

/*
 * State information for stream cipher.
 */
struct RC4ContextStr
{
#if defined(NSS_ARCFOUR_IJ_B4_S) || defined(NSS_BEVAND_ARCFOUR)
	Stype i;
	Stype j;
	Stype S[ARCFOUR_STATE_SIZE];
#else
	Stype S[ARCFOUR_STATE_SIZE];
	Stype i;
	Stype j;
#endif
};

/*
 * array indices [0..255] to initialize cx->S array (faster than loop).
 */
static const Stype Kinit[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
	0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
	0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
	0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
	0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

RC4Context *
RC4_AllocateContext(void)
{
    return PORT_ZNew(RC4Context);
}

SECStatus   
RC4_InitContext(RC4Context *cx, const unsigned char *key, unsigned int len,
	        const unsigned char * unused1, int unused2, 
		unsigned int unused3, unsigned int unused4)
{
	unsigned int i;
	PRUint8 j, tmp;
	PRUint8 K[256];
	PRUint8 *L;

	/* verify the key length. */
	PORT_Assert(len > 0 && len < ARCFOUR_STATE_SIZE);
	if (len == 0 || len >= ARCFOUR_STATE_SIZE) {
		PORT_SetError(SEC_ERROR_BAD_KEY);
		return SECFailure;
	}
	if (cx == NULL) {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    return SECFailure;
	}
	/* Initialize the state using array indices. */
	memcpy(cx->S, Kinit, sizeof cx->S);
	/* Fill in K repeatedly with values from key. */
	L = K;
	for (i = sizeof K; i > len; i-= len) {
		memcpy(L, key, len);
		L += len;
	}
	memcpy(L, key, i);
	/* Stir the state of the generator.  At this point it is assumed
	 * that the key is the size of the state buffer.  If this is not
	 * the case, the key bytes are repeated to fill the buffer.
	 */
	j = 0;
#define ARCFOUR_STATE_STIR(ii) \
	j = j + cx->S[ii] + K[ii]; \
	SWAP(cx->S[ii], cx->S[j]);
	for (i=0; i<ARCFOUR_STATE_SIZE; i++) {
		ARCFOUR_STATE_STIR(i);
	}
	cx->i = 0;
	cx->j = 0;
	return SECSuccess;
}


/*
 * Initialize a new generator.
 */
RC4Context *
RC4_CreateContext(const unsigned char *key, int len)
{
    RC4Context *cx = RC4_AllocateContext();
    if (cx) {
	SECStatus rv = RC4_InitContext(cx, key, len, NULL, 0, 0, 0);
	if (rv != SECSuccess) {
	    PORT_ZFree(cx, sizeof(*cx));
	    cx = NULL;
	}
    }
    return cx;
}

void 
RC4_DestroyContext(RC4Context *cx, PRBool freeit)
{
	if (freeit)
		PORT_ZFree(cx, sizeof(*cx));
}

#if defined(NSS_BEVAND_ARCFOUR)
extern void ARCFOUR(RC4Context *cx, WORD inputLen, 
	const unsigned char *input, unsigned char *output);
#else
/*
 * Generate the next byte in the stream.
 */
#define ARCFOUR_NEXT_BYTE() \
	tmpSi = cx->S[++tmpi]; \
	tmpj += tmpSi; \
	tmpSj = cx->S[tmpj]; \
	cx->S[tmpi] = tmpSj; \
	cx->S[tmpj] = tmpSi; \
	t = tmpSi + tmpSj;

#ifdef CONVERT_TO_WORDS
/*
 * Straight ARCFOUR op.  No optimization.
 */
static SECStatus 
rc4_no_opt(RC4Context *cx, unsigned char *output,
           unsigned int *outputLen, unsigned int maxOutputLen,
           const unsigned char *input, unsigned int inputLen)
{
    PRUint8 t;
	Stype tmpSi, tmpSj;
	register PRUint8 tmpi = cx->i;
	register PRUint8 tmpj = cx->j;
	unsigned int index;
	PORT_Assert(maxOutputLen >= inputLen);
	if (maxOutputLen < inputLen) {
		PORT_SetError(SEC_ERROR_OUTPUT_LEN);
		return SECFailure;
	}
	for (index=0; index < inputLen; index++) {
		/* Generate next byte from stream. */
		ARCFOUR_NEXT_BYTE();
		/* output = next stream byte XOR next input byte */
		output[index] = cx->S[t] ^ input[index];
	}
	*outputLen = inputLen;
	cx->i = tmpi;
	cx->j = tmpj;
	return SECSuccess;
}

#else
/* !CONVERT_TO_WORDS */

/*
 * Byte-at-a-time ARCFOUR, unrolling the loop into 8 pieces.
 */
static SECStatus 
rc4_unrolled(RC4Context *cx, unsigned char *output,
             unsigned int *outputLen, unsigned int maxOutputLen,
             const unsigned char *input, unsigned int inputLen)
{
	PRUint8 t;
	Stype tmpSi, tmpSj;
	register PRUint8 tmpi = cx->i;
	register PRUint8 tmpj = cx->j;
	int index;
	PORT_Assert(maxOutputLen >= inputLen);
	if (maxOutputLen < inputLen) {
		PORT_SetError(SEC_ERROR_OUTPUT_LEN);
		return SECFailure;
	}
	for (index = inputLen / 8; index-- > 0; input += 8, output += 8) {
		ARCFOUR_NEXT_BYTE();
		output[0] = cx->S[t] ^ input[0];
		ARCFOUR_NEXT_BYTE();
		output[1] = cx->S[t] ^ input[1];
		ARCFOUR_NEXT_BYTE();
		output[2] = cx->S[t] ^ input[2];
		ARCFOUR_NEXT_BYTE();
		output[3] = cx->S[t] ^ input[3];
		ARCFOUR_NEXT_BYTE();
		output[4] = cx->S[t] ^ input[4];
		ARCFOUR_NEXT_BYTE();
		output[5] = cx->S[t] ^ input[5];
		ARCFOUR_NEXT_BYTE();
		output[6] = cx->S[t] ^ input[6];
		ARCFOUR_NEXT_BYTE();
		output[7] = cx->S[t] ^ input[7];
	}
	index = inputLen % 8;
	if (index) {
		input += index;
		output += index;
		switch (index) {
		case 7:
			ARCFOUR_NEXT_BYTE();
			output[-7] = cx->S[t] ^ input[-7]; /* FALLTHRU */
		case 6:
			ARCFOUR_NEXT_BYTE();
			output[-6] = cx->S[t] ^ input[-6]; /* FALLTHRU */
		case 5:
			ARCFOUR_NEXT_BYTE();
			output[-5] = cx->S[t] ^ input[-5]; /* FALLTHRU */
		case 4:
			ARCFOUR_NEXT_BYTE();
			output[-4] = cx->S[t] ^ input[-4]; /* FALLTHRU */
		case 3:
			ARCFOUR_NEXT_BYTE();
			output[-3] = cx->S[t] ^ input[-3]; /* FALLTHRU */
		case 2:
			ARCFOUR_NEXT_BYTE();
			output[-2] = cx->S[t] ^ input[-2]; /* FALLTHRU */
		case 1:
			ARCFOUR_NEXT_BYTE();
			output[-1] = cx->S[t] ^ input[-1]; /* FALLTHRU */
		default:
			/* FALLTHRU */
			; /* hp-ux build breaks without this */
		}
	}
	cx->i = tmpi;
	cx->j = tmpj;
	*outputLen = inputLen;
	return SECSuccess;
}
#endif

#ifdef IS_LITTLE_ENDIAN
#define ARCFOUR_NEXT4BYTES_L(n) \
	ARCFOUR_NEXT_BYTE(); streamWord |= (WORD)cx->S[t] << (n     ); \
	ARCFOUR_NEXT_BYTE(); streamWord |= (WORD)cx->S[t] << (n +  8); \
	ARCFOUR_NEXT_BYTE(); streamWord |= (WORD)cx->S[t] << (n + 16); \
	ARCFOUR_NEXT_BYTE(); streamWord |= (WORD)cx->S[t] << (n + 24);
#else
#define ARCFOUR_NEXT4BYTES_B(n) \
	ARCFOUR_NEXT_BYTE(); streamWord |= (WORD)cx->S[t] << (n + 24); \
	ARCFOUR_NEXT_BYTE(); streamWord |= (WORD)cx->S[t] << (n + 16); \
	ARCFOUR_NEXT_BYTE(); streamWord |= (WORD)cx->S[t] << (n +  8); \
	ARCFOUR_NEXT_BYTE(); streamWord |= (WORD)cx->S[t] << (n     );
#endif

#if (defined(IS_64) && !defined(__sparc)) || defined(NSS_USE_64)
/* 64-bit wordsize */
#ifdef IS_LITTLE_ENDIAN
#define ARCFOUR_NEXT_WORD() \
	{ streamWord = 0; ARCFOUR_NEXT4BYTES_L(0); ARCFOUR_NEXT4BYTES_L(32); }
#else
#define ARCFOUR_NEXT_WORD() \
	{ streamWord = 0; ARCFOUR_NEXT4BYTES_B(32); ARCFOUR_NEXT4BYTES_B(0); }
#endif
#else
/* 32-bit wordsize */
#ifdef IS_LITTLE_ENDIAN
#define ARCFOUR_NEXT_WORD() \
	{ streamWord = 0; ARCFOUR_NEXT4BYTES_L(0); }
#else
#define ARCFOUR_NEXT_WORD() \
	{ streamWord = 0; ARCFOUR_NEXT4BYTES_B(0); }
#endif
#endif

#ifdef IS_LITTLE_ENDIAN
#define RSH <<
#define LSH >>
#else
#define RSH >>
#define LSH <<
#endif

#ifdef IS_LITTLE_ENDIAN
#define LEFTMOST_BYTE_SHIFT 0
#define NEXT_BYTE_SHIFT(shift) shift + 8
#else
#define LEFTMOST_BYTE_SHIFT 8*(WORDSIZE - 1)
#define NEXT_BYTE_SHIFT(shift) shift - 8
#endif

#ifdef CONVERT_TO_WORDS
static SECStatus 
rc4_wordconv(RC4Context *cx, unsigned char *output,
             unsigned int *outputLen, unsigned int maxOutputLen,
             const unsigned char *input, unsigned int inputLen)
{
	PR_STATIC_ASSERT(sizeof(PRUword) == sizeof(ptrdiff_t));
	unsigned int inOffset = (PRUword)input % WORDSIZE;
	unsigned int outOffset = (PRUword)output % WORDSIZE;
	register WORD streamWord;
	register const WORD *pInWord;
	register WORD *pOutWord;
	register WORD inWord, nextInWord;
	PRUint8 t;
	register Stype tmpSi, tmpSj;
	register PRUint8 tmpi = cx->i;
	register PRUint8 tmpj = cx->j;
	unsigned int bufShift, invBufShift;
	unsigned int i;
	const unsigned char *finalIn;
	unsigned char *finalOut;

	PORT_Assert(maxOutputLen >= inputLen);
	if (maxOutputLen < inputLen) {
		PORT_SetError(SEC_ERROR_OUTPUT_LEN);
		return SECFailure;
	}
	if (inputLen < 2*WORDSIZE) {
		/* Ignore word conversion, do byte-at-a-time */
		return rc4_no_opt(cx, output, outputLen, maxOutputLen, input, inputLen);
	}
	*outputLen = inputLen;
	pInWord = (const WORD *)(input - inOffset);
	pOutWord = (WORD *)(output - outOffset);
	if (inOffset <= outOffset) {
		bufShift = 8*(outOffset - inOffset);
		invBufShift = 8*WORDSIZE - bufShift;
	} else {
		invBufShift = 8*(inOffset - outOffset);
		bufShift = 8*WORDSIZE - invBufShift;
	}
	/*****************************************************************/
	/* Step 1:                                                       */
	/* If the first output word is partial, consume the bytes in the */
	/* first partial output word by loading one or two words of      */
	/* input and shifting them accordingly.  Otherwise, just load    */
	/* in the first word of input.  At the end of this block, at     */
	/* least one partial word of input should ALWAYS be loaded.      */
	/*****************************************************************/
	if (outOffset) {
		unsigned int byteCount = WORDSIZE - outOffset; 
		for (i = 0; i < byteCount; i++) {
			ARCFOUR_NEXT_BYTE();
			output[i] = cx->S[t] ^ input[i];
		}
		/* Consumed byteCount bytes of input */
		inputLen -= byteCount;
		pInWord++;

		/* move to next word of output */
		pOutWord++;

		/* If buffers are relatively misaligned, shift the bytes in inWord
		 * to be aligned to the output buffer.
		 */
		if (inOffset < outOffset) {
			/* The first input word (which may be partial) has more bytes
			 * than needed.  Copy the remainder to inWord.
			 */
			unsigned int shift = LEFTMOST_BYTE_SHIFT;
			inWord = 0;
			for (i = 0; i < outOffset - inOffset; i++) {
				inWord |= (WORD)input[byteCount + i] << shift;
				shift = NEXT_BYTE_SHIFT(shift);
			}
		} else if (inOffset > outOffset) {
			/* Consumed some bytes in the second input word.  Copy the
			 * remainder to inWord.
			 */
			inWord = *pInWord++;
			inWord = inWord LSH invBufShift;
		} else {
			inWord = 0;
		}
	} else {
		/* output is word-aligned */
		if (inOffset) {
			/* Input is not word-aligned.  The first word load of input 
			 * will not produce a full word of input bytes, so one word
			 * must be pre-loaded.  The main loop below will load in the
			 * next input word and shift some of its bytes into inWord
			 * in order to create a full input word.  Note that the main
			 * loop must execute at least once because the input must
			 * be at least two words.
			 */
			unsigned int shift = LEFTMOST_BYTE_SHIFT;
			inWord = 0;
			for (i = 0; i < WORDSIZE - inOffset; i++) {
				inWord |= (WORD)input[i] << shift;
				shift = NEXT_BYTE_SHIFT(shift);
			}
			pInWord++;
		} else {
			/* Input is word-aligned.  The first word load of input 
			 * will produce a full word of input bytes, so nothing
			 * needs to be loaded here.
			 */
			inWord = 0;
		}
	}
	/*****************************************************************/
	/* Step 2: main loop                                             */
	/* At this point the output buffer is word-aligned.  Any unused  */
	/* bytes from above will be in inWord (shifted correctly).  If   */
	/* the input buffer is unaligned relative to the output buffer,  */
	/* shifting has to be done.                                      */
	/*****************************************************************/
	if (bufShift) {
		/* preloadedByteCount is the number of input bytes pre-loaded
		 * in inWord.
		 */
		unsigned int preloadedByteCount = bufShift/8;
		for (; inputLen >= preloadedByteCount + WORDSIZE;
		     inputLen -= WORDSIZE) {
			nextInWord = *pInWord++;
			inWord |= nextInWord RSH bufShift;
			nextInWord = nextInWord LSH invBufShift;
			ARCFOUR_NEXT_WORD();
			*pOutWord++ = inWord ^ streamWord;
			inWord = nextInWord;
		}
		if (inputLen == 0) {
			/* Nothing left to do. */
			cx->i = tmpi;
			cx->j = tmpj;
			return SECSuccess;
		}
		finalIn = (const unsigned char *)pInWord - preloadedByteCount;
	} else {
		for (; inputLen >= WORDSIZE; inputLen -= WORDSIZE) {
			inWord = *pInWord++;
			ARCFOUR_NEXT_WORD();
			*pOutWord++ = inWord ^ streamWord;
		}
		if (inputLen == 0) {
			/* Nothing left to do. */
			cx->i = tmpi;
			cx->j = tmpj;
			return SECSuccess;
		}
		finalIn = (const unsigned char *)pInWord;
	}
	/*****************************************************************/
	/* Step 3:                                                       */
	/* Do the remaining partial word of input one byte at a time.    */
	/*****************************************************************/
	finalOut = (unsigned char *)pOutWord;
	for (i = 0; i < inputLen; i++) {
		ARCFOUR_NEXT_BYTE();
		finalOut[i] = cx->S[t] ^ finalIn[i];
	}
	cx->i = tmpi;
	cx->j = tmpj;
	return SECSuccess;
}
#endif
#endif /* NSS_BEVAND_ARCFOUR */

SECStatus 
RC4_Encrypt(RC4Context *cx, unsigned char *output,
            unsigned int *outputLen, unsigned int maxOutputLen,
            const unsigned char *input, unsigned int inputLen)
{
	PORT_Assert(maxOutputLen >= inputLen);
	if (maxOutputLen < inputLen) {
		PORT_SetError(SEC_ERROR_OUTPUT_LEN);
		return SECFailure;
	}
#if defined(NSS_BEVAND_ARCFOUR)
	ARCFOUR(cx, inputLen, input, output);
        *outputLen = inputLen;
	return SECSuccess;
#elif defined( CONVERT_TO_WORDS )
	/* Convert the byte-stream to a word-stream */
	return rc4_wordconv(cx, output, outputLen, maxOutputLen, input, inputLen);
#else
	/* Operate on bytes, but unroll the main loop */
	return rc4_unrolled(cx, output, outputLen, maxOutputLen, input, inputLen);
#endif
}

SECStatus RC4_Decrypt(RC4Context *cx, unsigned char *output,
                      unsigned int *outputLen, unsigned int maxOutputLen,
                      const unsigned char *input, unsigned int inputLen)
{
	PORT_Assert(maxOutputLen >= inputLen);
	if (maxOutputLen < inputLen) {
		PORT_SetError(SEC_ERROR_OUTPUT_LEN);
		return SECFailure;
	}
	/* decrypt and encrypt are same operation. */
#if defined(NSS_BEVAND_ARCFOUR)
	ARCFOUR(cx, inputLen, input, output);
        *outputLen = inputLen;
	return SECSuccess;
#elif defined( CONVERT_TO_WORDS )
	/* Convert the byte-stream to a word-stream */
	return rc4_wordconv(cx, output, outputLen, maxOutputLen, input, inputLen);
#else
	/* Operate on bytes, but unroll the main loop */
	return rc4_unrolled(cx, output, outputLen, maxOutputLen, input, inputLen);
#endif
}

#undef CONVERT_TO_WORDS
#undef USE_WORD
