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
/*
 * Software implementation of FORTEZZA skipjack primatives
 */
#include "maci.h"
#include "seccomon.h"
#include "swforti.h" 

/*
 * Xor the IV into the plaintext buffer either just before encryption, or
 * just after decryption.
 */
static void
fort_XorIV(unsigned char *obuffer, unsigned char *buffer, unsigned char *iv) {
    int i;
#ifdef USE_INT32
    if ((buffer & 0x3) == 0) && ((iv & 0x3) == 0)) {
	int32 *ibuffer = (int32 *)buffer;
	int32 *iobuffer = (int32 *)obuffer;
	int32 *iiv = (int32 *)iv;

	iobuffer[0] = ibuffer[0] ^ iiv[0];
	iobuffer[1] = ibuffer[1] ^ iiv[1];
	return;
    }
#endif

    for (i=0; i < SKIPJACK_BLOCK_SIZE; i++) {
	obuffer[i] = buffer[i] ^ iv[i];
    }
}


/* the F-table for Skipjack */
unsigned char F[256] = { 
    0xa3, 0xd7, 0x09, 0x83, 0xf8, 0x48, 0xf6, 0xf4,
    0xb3, 0x21, 0x15, 0x78, 0x99, 0xb1, 0xaf, 0xf9,
    0xe7, 0x2d, 0x4d, 0x8a, 0xce, 0x4c, 0xca, 0x2e,
    0x52, 0x95, 0xd9, 0x1e, 0x4e, 0x38, 0x44, 0x28,
    0x0a, 0xdf, 0x02, 0xa0, 0x17, 0xf1, 0x60, 0x68,
    0x12, 0xb7, 0x7a, 0xc3, 0xe9, 0xfa, 0x3d, 0x53,
    0x96, 0x84, 0x6b, 0xba, 0xf2, 0x63, 0x9a, 0x19,
    0x7c, 0xae, 0xe5, 0xf5, 0xf7, 0x16, 0x6a, 0xa2,
    0x39, 0xb6, 0x7b, 0x0f, 0xc1, 0x93, 0x81, 0x1b,
    0xee, 0xb4, 0x1a, 0xea, 0xd0, 0x91, 0x2f, 0xb8,
    0x55, 0xb9, 0xda, 0x85, 0x3f, 0x41, 0xbf, 0xe0,
    0x5a, 0x58, 0x80, 0x5f, 0x66, 0x0b, 0xd8, 0x90,
    0x35, 0xd5, 0xc0, 0xa7, 0x33, 0x06, 0x65, 0x69,
    0x45, 0x00, 0x94, 0x56, 0x6d, 0x98, 0x9b, 0x76,
    0x97, 0xfc, 0xb2, 0xc2, 0xb0, 0xfe, 0xdb, 0x20,
    0xe1, 0xeb, 0xd6, 0xe4, 0xdd, 0x47, 0x4a, 0x1d,
    0x42, 0xed, 0x9e, 0x6e, 0x49, 0x3c, 0xcd, 0x43,
    0x27, 0xd2, 0x07, 0xd4, 0xde, 0xc7, 0x67, 0x18,
    0x89, 0xcb, 0x30, 0x1f, 0x8d, 0xc6, 0x8f, 0xaa,
    0xc8, 0x74, 0xdc, 0xc9, 0x5d, 0x5c, 0x31, 0xa4,
    0x70, 0x88, 0x61, 0x2c, 0x9f, 0x0d, 0x2b, 0x87,
    0x50, 0x82, 0x54, 0x64, 0x26, 0x7d, 0x03, 0x40,
    0x34, 0x4b, 0x1c, 0x73, 0xd1, 0xc4, 0xfd, 0x3b,
    0xcc, 0xfb, 0x7f, 0xab, 0xe6, 0x3e, 0x5b, 0xa5,
    0xad, 0x04, 0x23, 0x9c, 0x14, 0x51, 0x22, 0xf0,
    0x29, 0x79, 0x71, 0x7e, 0xff, 0x8c, 0x0e, 0xe2,
    0x0c, 0xef, 0xbc, 0x72, 0x75, 0x6f, 0x37, 0xa1,
    0xec, 0xd3, 0x8e, 0x62, 0x8b, 0x86, 0x10, 0xe8,
    0x08, 0x77, 0x11, 0xbe, 0x92, 0x4f, 0x24, 0xc5,
    0x32, 0x36, 0x9d, 0xcf, 0xf3, 0xa6, 0xbb, 0xac,
    0x5e, 0x6c, 0xa9, 0x13, 0x57, 0x25, 0xb5, 0xe3,
    0xbd, 0xa8, 0x3a, 0x01, 0x05, 0x59, 0x2a, 0x46
};

typedef unsigned char fort_keysched[32*4];

/* do the key schedule work once for efficency */
static void
fort_skipKeySchedule(FORTSkipjackKeyPtr key,fort_keysched keysched)
{
    unsigned char *keyptr = key;
    unsigned char *first = keyptr +sizeof(FORTSkipjackKey)-1;
    int i;

    keyptr = first;

    for (i=0; i < (32*4); i++) {
	keysched[i] = *keyptr--;
	if (keyptr < key) keyptr = first;
    }
    return;
}

static void
fort_clearShedule(fort_keysched keysched)
{
    PORT_Memset(keysched, 0, sizeof(keysched));
}


static unsigned int 
G(fort_keysched cv, int k, unsigned int wordIn)
{
    unsigned char g1, g2, g3, g4, g5, g6;

    g1 = (unsigned char) (wordIn >> 8) & 0xff;
    g2 = (unsigned char) wordIn & 0xff;

    g3 = F[g2^cv[4*k]]^g1;
    g4 = F[g3^cv[4*k+1]]^g2;
    g5 = F[g4^cv[4*k+2]]^g3;
    g6 = F[g5^cv[4*k+3]]^g4;

    return ((g5<<8)+g6);
}

static unsigned int 
G1(fort_keysched cv, int k, unsigned int wordIn)
{
    unsigned char g1, g2, g3, g4, g5, g6;

    g5 = (unsigned char) (wordIn >> 8) & 0xff;
    g6 = (unsigned char) wordIn & 0xff;

    g4 = F[g5^cv[4*k+3]]^g6;
    g3 = F[g4^cv[4*k+2]]^g5;
    g2 = F[g3^cv[4*k+1]]^g4;
    g1 = F[g2^cv[4*k]]^g3;

    return ((g1<<8)+g2);
}

static void 
ruleA(fort_keysched cv,int round,unsigned int *w)
{
    unsigned int w4;
    int i;

    for(i=0; i<8; i++) {
	int k = round*16+i;
	int counter = k+1;

	w4 = w[4];
	w[4] = w[3];
	w[3] = w[2];
	w[2] = G(cv,k,w[1]);
	w[1] = G(cv,k,w[1]) ^ w4 ^ counter;
    }
    return;
}

static void 
ruleB(fort_keysched cv,int round,unsigned int *w)
{
    unsigned int w4;
    int i;

    for(i=0; i<8; i++) {
	int k = round*16+i+8;
	int counter = k+1;

	w4 = w[4];
	w[4] = w[3];
	w[3] = w[1] ^ w[2] ^ counter; 
	w[2] = G(cv,k,w[1]);
	w[1] = w4;
    }
    return;
}

static void 
ruleA1(fort_keysched cv,int round,unsigned int *w)
{
    unsigned int w4;
    int i;

    for(i=7; i>=0; i--) {
	int k = round*16+i;
	int counter = k+1;

	w4 = w[4];
	w[4] = w[1] ^ w[2] ^ counter;
	w[1] = G1(cv,k,w[2]);
	w[2] = w[3];
	w[3] = w4;
    }
    return;
}

static void 
ruleB1(fort_keysched cv,int round,unsigned int *w)
{
    unsigned int w4;
    int i;

    for(i=7; i>=0; i--) {
	int k = round*16+i+8;
	int counter = k+1;

	w4 = w[4];
	w[4] = w[1];
	w[1] = G1(cv,k,w[2]);
	w[2] = G1(cv,k,w[2]) ^ w[3] ^ counter; 
	w[3] = w4;
    }
    return;
}


static void 
fort_doskipD(fort_keysched cv,unsigned char *cipherIn,
						 unsigned char *plainOut) {
  unsigned int w[5]; /* ignore w[0] so the code matches the doc */

  /* initial byte swap */
  w[1]=(cipherIn[7]<<8)+cipherIn[6];
  w[2]=(cipherIn[5]<<8)+cipherIn[4];
  w[3]=(cipherIn[3]<<8)+cipherIn[2];
  w[4]=(cipherIn[1]<<8)+cipherIn[0];

  ruleB1(cv,1,w);
  ruleA1(cv,1,w);
  ruleB1(cv,0,w);
  ruleA1(cv,0,w);

  /* final byte swap */
  plainOut[0] = w[4] & 0xff;
  plainOut[1] = (w[4] >> 8) & 0xff;
  plainOut[2] = w[3] & 0xff;
  plainOut[3] = (w[3] >> 8) & 0xff;
  plainOut[4] = w[2] & 0xff;
  plainOut[5] = (w[2] >> 8) & 0xff;
  plainOut[6] = w[1] & 0xff;
  plainOut[7] = (w[1] >> 8) & 0xff;
  return;
}

static void 
fort_doskipE(fort_keysched cv,unsigned char *cipherIn,
						 unsigned char *plainOut) {
  unsigned int w[5]; /* ignore w[0] so the code matches the doc */

  /* initial byte swap */
  w[1]=(cipherIn[7]<<8)+cipherIn[6];
  w[2]=(cipherIn[5]<<8)+cipherIn[4];
  w[3]=(cipherIn[3]<<8)+cipherIn[2];
  w[4]=(cipherIn[1]<<8)+cipherIn[0];

  ruleA(cv,0,w);
  ruleB(cv,0,w);
  ruleA(cv,1,w);
  ruleB(cv,1,w);

  /* final byte swap */
  plainOut[0] = w[4] & 0xff;
  plainOut[1] = (w[4] >> 8) & 0xff;
  plainOut[2] = w[3] & 0xff;
  plainOut[3] = (w[3] >> 8) & 0xff;
  plainOut[4] = w[2] & 0xff;
  plainOut[5] = (w[2] >> 8) & 0xff;
  plainOut[6] = w[1] & 0xff;
  plainOut[7] = (w[1] >> 8) & 0xff;
  return;
}

/* Checksums are calculated by encrypted a fixed string with the key, then
 * taking 16 bytes of the result from the block */
static int
fort_CalcKeyChecksum(FORTSkipjackKeyPtr key, unsigned char *sum) {
    unsigned char ckdata[8] = {
		 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 };
    unsigned char ckres[8];
    fort_keysched keysched;


    fort_skipKeySchedule(key,keysched);

    fort_doskipE(keysched,ckdata,ckres);
    fort_clearShedule(keysched);
    PORT_Memcpy(sum,&ckres[1],2);
    return CI_OK;
}

/* These function actually implements skipjack CBC Decrypt */
int
fort_skipjackDecrypt(FORTSkipjackKeyPtr key, unsigned char *iv, 
		unsigned long size, unsigned char *cipherIn, 
						unsigned char *plainOut) {
    unsigned char ivdata1[SKIPJACK_BLOCK_SIZE];
    unsigned char ivdata2[SKIPJACK_BLOCK_SIZE];
    unsigned char *lastiv, *nextiv, *tmpiv;
    fort_keysched keysched;

    /* do the key schedule work once for efficency */
    fort_skipKeySchedule(key,keysched);

    /* As we decrypt, we need to save the last block so that we can
     * Xor it out of decrypted text to get the real plain text. We actually
     * have to save it because cipherIn and plainOut may point to the same
     * buffer. */
    lastiv =ivdata1;
    nextiv = ivdata2;
    PORT_Memcpy(lastiv,iv,SKIPJACK_BLOCK_SIZE);
    while (size >= SKIPJACK_BLOCK_SIZE) {
	/* save the IV for the next block */
	PORT_Memcpy(nextiv,cipherIn,SKIPJACK_BLOCK_SIZE);
   	fort_doskipD(keysched,cipherIn,plainOut);
	/* xor out the last IV */
	fort_XorIV(plainOut,plainOut,lastiv);

	/* swap the IV buffers */
	tmpiv = lastiv;
	lastiv = nextiv;
	nextiv =tmpiv;

	/* increment the loop pointers... be sure to get the input, output,
	 * and size (decrement) each fortdoskipD operates on an entire block*/
	cipherIn += SKIPJACK_BLOCK_SIZE;
	plainOut += SKIPJACK_BLOCK_SIZE;
	size -= SKIPJACK_BLOCK_SIZE;
    }
    fort_clearShedule(keysched); /* don't leave the key lying around the stack*/
    if (size != 0) return CI_INV_SIZE;
    return CI_OK;
}

/* These function actually implements skipjack CBC Encrypt */
int
fort_skipjackEncrypt(FORTSkipjackKeyPtr key, unsigned char *iv, 
		unsigned long size, unsigned char *plainIn, 
						unsigned char *cipherOut) {
    unsigned char *tmpiv;
    fort_keysched keysched;
    unsigned char plain[SKIPJACK_BLOCK_SIZE];

    fort_skipKeySchedule(key,keysched);
    tmpiv = iv;
    while (size >= SKIPJACK_BLOCK_SIZE) {
	/* We Xor into a temp buffer because we don't want to modify plainIn,
	 * doing so may make the caller very unhappy:). */
	fort_XorIV(plain,plainIn,tmpiv);
   	fort_doskipE(keysched,plain,cipherOut);
	tmpiv = cipherOut;
	cipherOut += SKIPJACK_BLOCK_SIZE;
	plainIn += SKIPJACK_BLOCK_SIZE;
	size -= SKIPJACK_BLOCK_SIZE;
    }
    fort_clearShedule(keysched); /* don't leave the key lying around the stack*/
    if (size != 0) return CI_INV_SIZE;
    return CI_OK;
}

   

/*
 * unwrap is used for key generation and mixing
 */
int
fort_skipjackUnwrap(FORTSkipjackKeyPtr key,unsigned long len, 
			unsigned char *cipherIn, unsigned char *plainOut) {
    unsigned char low[10];
    fort_keysched keysched;
    int i,ret;

    /* unwrap can only unwrap 80 bit symetric keys and 160 private keys 
     * sometimes these values have checksums. When they do, we should verify
     * those checksums. */
    switch (len) {
    case 20:	/* private key */
    case 24:   /* private key with checksum */
	ret = fort_skipjackUnwrap(key,len/2,cipherIn,plainOut);
	if (ret != CI_OK) return ret;
	ret = fort_skipjackUnwrap(key,len/2,&cipherIn[len/2],low);

	/* unmunge the low word */
	for (i=0; i < 10; i++) {
	    low[i] = low[i] ^ plainOut[i];
	}

	/* the unwrap will fail above because the checkword is on
	 * low, not low ^ high.
	 */
	if (ret == CI_CHECKWORD_FAIL) {
	    unsigned char checksum[2];

            ret = fort_CalcKeyChecksum(low,checksum);
	    if (ret != CI_OK) return ret;
	    if (PORT_Memcmp(checksum,&cipherIn[len-2],2) != 0) {
		return CI_CHECKWORD_FAIL;
	    }
	}
	if (ret != CI_OK) return ret;

	/* re-order the low word */
	PORT_Memcpy(&plainOut[10],&low[8],2);
	PORT_Memcpy(&plainOut[12],&low[0],8);
	return CI_OK;
    case 10: /* 80 bit skipjack key */
    case 12: /* 80 bit skipjack key with checksum */
	fort_skipKeySchedule(key,keysched);
	fort_doskipD(keysched,cipherIn,plainOut);
	plainOut[8] = cipherIn[8] ^ plainOut[0];
	plainOut[9] = cipherIn[9] ^ plainOut[1];
	fort_doskipD(keysched,plainOut,plainOut);
	fort_clearShedule(keysched); 
	/* if we have a checkum, verify it */
	if (len == 12) {
	    unsigned char checksum[2];

            ret = fort_CalcKeyChecksum(plainOut,checksum);
	    if (ret != CI_OK) return ret;
	    if (PORT_Memcmp(checksum,&cipherIn[10],2) != 0) {
		return CI_CHECKWORD_FAIL;
	    }
        }
	return CI_OK;
    default:
	break;
    }
    return CI_INV_SIZE;
}

/*
 * unwrap is used for key generation and mixing
 */
int
fort_skipjackWrap(FORTSkipjackKeyPtr key,unsigned long len, 
			unsigned char *plainIn, unsigned char *cipherOut) {
    unsigned char low[10];
    unsigned char checksum[2];
    fort_keysched keysched;
    int i,ret;


    /* NOTE: length refers to the target in the case of wrap */
    /* Wrap can only Wrap 80 bit symetric keys and 160 private keys 
     * sometimes these values have checksums. When they do, we should verify
     * those checksums. */
    switch (len) {
    case 20:	/* private key */
    case 24:   /* private key with checksum */
	/* re-order the low word */
	PORT_Memcpy(&low[8],&plainIn[10],2);
	PORT_Memcpy(&low[0],&plainIn[12],8);
	if (len == 24) {
            ret = fort_CalcKeyChecksum(low,checksum);
	    if (ret != CI_OK) return ret;
	}
	/* munge the low word */
	for (i=0; i < 10; i++) {
	    low[i] = low[i] ^ plainIn[i];
	}
	ret = fort_skipjackWrap(key,len/2,plainIn,cipherOut);
	ret = fort_skipjackWrap(key,len/2,low,&cipherOut[len/2]);
	if (len == 24) {
	    PORT_Memcpy(&cipherOut[len - 2], checksum, sizeof(checksum));
	}

	return CI_OK;
    case 10: /* 80 bit skipjack key */
    case 12: /* 80 bit skipjack key with checksum */

	fort_skipKeySchedule(key,keysched);
	fort_doskipE(keysched,plainIn,cipherOut);
	cipherOut[8] = plainIn[8] ^ cipherOut[0];
	cipherOut[9] = plainIn[9] ^ cipherOut[1];
	fort_doskipE(keysched,cipherOut,cipherOut);
	fort_clearShedule(keysched); 
	/* if we need a checkum, get it */
	if (len == 12) {
            ret = fort_CalcKeyChecksum(plainIn,&cipherOut[10]);
	    if (ret != CI_OK) return ret;
        }
	return CI_OK;
    default:
	break;
    }
    return CI_INV_SIZE;
}

