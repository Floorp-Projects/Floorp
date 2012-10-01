/*
 *  mknewpc2.c
 *
 *  Generate PC-2 tables for DES-150 library
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

typedef unsigned char BYTE;
typedef unsigned int  HALF;

#define DES_ENCRYPT 0
#define DES_DECRYPT 1

/* two 28-bit registers defined in key schedule production process */
static HALF C0, D0;

static HALF L0, R0;

/* key schedule, 16 internal keys, each with 8 6-bit parts */
static BYTE KS [8] [16];


/*
 * This table takes the 56 bits in C0 and D0 and shows show they are 
 * permuted into the 8 6-bit parts of the key in the key schedule.
 * The bits of C0 are numbered left to right, 1-28.
 * The bits of D0 are numbered left to right, 29-56.
 * Zeros in this table represent bits that are always zero.
 * Note that all the bits in the first  4 rows come from C0, 
 *       and all the bits in the second 4 rows come from D0.
 */
static const BYTE PC2[64] = {
    14, 17, 11, 24,  1,  5,  0,  0,	/* S1 */
     3, 28, 15,  6, 21, 10,  0,  0,	/* S2 */
    23, 19, 12,  4, 26,  8,  0,  0,	/* S3 */
    16,  7, 27, 20, 13,  2,  0,  0,	/* S4 */

    41, 52, 31, 37, 47, 55,  0,  0,	/* S5 */
    30, 40, 51, 45, 33, 48,  0,  0,	/* S6 */
    44, 49, 39, 56, 34, 53,  0,  0,	/* S7 */
    46, 42, 50, 36, 29, 32,  0,  0	/* S8 */
};

/* This table represents the same info as PC2, except that 
 * The bits of C0 and D0 are each numbered right to left, 0-27.
 * -1 values indicate bits that are always zero.
 * As before all the bits in the first  4 rows come from C0, 
 *       and all the bits in the second 4 rows come from D0.
 */
static       signed char PC2a[64] = {
/* bits of C0 */
    14, 11, 17,  4, 27, 23, -1, -1,	/* S1 */
    25,  0, 13, 22,  7, 18, -1, -1,	/* S2 */
     5,  9, 16, 24,  2, 20, -1, -1,	/* S3 */
    12, 21,  1,  8, 15, 26, -1, -1,	/* S4 */
/* bits of D0 */
    15,  4, 25, 19,  9,  1, -1, -1,	/* S5 */
    26, 16,  5, 11, 23,  8, -1, -1,	/* S6 */
    12,  7, 17,  0, 22,  3, -1, -1,	/* S7 */
    10, 14,  6, 20, 27, 24, -1, -1 	/* S8 */
};

/* This table represents the same info as PC2a, except that 
 * The order of of the rows has been changed to increase the efficiency
 * with which the key sechedule is created.
 * Fewer shifts and ANDs are required to make the KS from these.
 */
static const signed char PC2b[64] = {
/* bits of C0 */
    14, 11, 17,  4, 27, 23, -1, -1,	/* S1 */
     5,  9, 16, 24,  2, 20, -1, -1,	/* S3 */
    25,  0, 13, 22,  7, 18, -1, -1,	/* S2 */
    12, 21,  1,  8, 15, 26, -1, -1,	/* S4 */
/* bits of D0 */
    26, 16,  5, 11, 23,  8, -1, -1,	/* S6 */
    10, 14,  6, 20, 27, 24, -1, -1,	/* S8 */
    15,  4, 25, 19,  9,  1, -1, -1,	/* S5 */
    12,  7, 17,  0, 22,  3, -1, -1 	/* S7 */
};

/* Only 24 of the 28 bits in C0 and D0 are used in PC2.
 * The used bits of C0 and D0 are grouped into 4 groups of 6,
 * so that the PC2 permutation can be accomplished with 4 lookups
 * in tables of 64 entries.
 * The following table shows how the bits of C0 and D0 are grouped
 * into indexes for the respective table lookups.  
 * Bits are numbered right-to-left, 0-27, as in PC2b.
 */
static BYTE NDX[48] = {
/* Bits of C0 */
    27, 26, 25, 24, 23, 22,	/* C0 table 0 */
    18, 17, 16, 15, 14, 13,	/* C0 table 1 */
     9,  8,  7,  2,  1,  0,	/* C0 table 2 */
     5,  4, 21, 20, 12, 11,	/* C0 table 3 */
/* bits of D0 */
    27, 26, 25, 24, 23, 22,	/* D0 table 0 */
    20, 19, 17, 16, 15, 14,	/* D0 table 1 */
    12, 11, 10,  9,  8,  7,	/* D0 table 2 */
     6,  5,  4,  3,  1,  0	/* D0 table 3 */
};

/* Here's the code that does that grouping. 
	left   = PC2LOOKUP(0, 0, ((c0 >> 22) & 0x3F) );
	left  |= PC2LOOKUP(0, 1, ((c0 >> 13) & 0x3F) );
	left  |= PC2LOOKUP(0, 2, ((c0 >>  4) & 0x38) | (c0 & 0x7) );
	left  |= PC2LOOKUP(0, 3, ((c0>>18)&0xC) | ((c0>>11)&0x3) | (c0&0x30));

	right  = PC2LOOKUP(1, 0, ((d0 >> 22) & 0x3F) );
	right |= PC2LOOKUP(1, 1, ((d0 >> 15) & 0x30) | ((d0 >> 14) & 0xf) );
	right |= PC2LOOKUP(1, 2, ((d0 >>  7) & 0x3F) );
	right |= PC2LOOKUP(1, 3, ((d0 >>  1) & 0x3C) | (d0 & 0x3));
*/

void
make_pc2a( void )
{

    int i, j;

    for ( i = 0; i < 64; ++i ) {
	j = PC2[i];
	if (j == 0)
	    j = -1;
	else if ( j < 29 )
	    j = 28 - j ;
	else
	    j = 56 - j;
	PC2a[i] = j;
    }
    for ( i = 0; i < 64; i += 8 ) {
	printf("%3d,%3d,%3d,%3d,%3d,%3d,%3d,%3d,\n",
		PC2a[i+0],PC2a[i+1],PC2a[i+2],PC2a[i+3],
		PC2a[i+4],PC2a[i+5],PC2a[i+6],PC2a[i+7] );
    }
}

HALF PC2cd0[64];

HALF PC_2H[8][64];

void
mktable( )
{
    int i;
    int table;
    const BYTE * ndx   = NDX;
    HALF         mask;

    mask  = 0x80000000;
    for (i = 0; i < 32; ++i, mask >>= 1) {
	int bit = PC2b[i];
	if (bit < 0)
	    continue;
	PC2cd0[bit + 32] = mask;
    }

    mask  = 0x80000000;
    for (i = 32; i < 64; ++i, mask >>= 1) {
	int bit = PC2b[i];
	if (bit < 0)
	    continue;
	PC2cd0[bit] = mask;
    }

#if DEBUG
    for (i = 0; i < 64; ++i) {
    	printf("0x%08x,\n", PC2cd0[i]);
    }
#endif
    for (i = 0; i < 24; ++i) {
    	NDX[i] += 32;	/* because c0 is the upper half */
    }

    for (table = 0; table < 8; ++table) {
	HALF bitvals[6];
    	for (i = 0; i < 6; ++i) {
	    bitvals[5-i] = PC2cd0[*ndx++];
	}
	for (i = 0; i < 64; ++i) {
	    int  j;
	    int  k     = 0;
	    HALF value = 0;

	    for (j = i; j; j >>= 1, ++k) {
	    	if (j & 1) {
		    value |= bitvals[k];
		}
	    }
	    PC_2H[table][i] = value;
	}
	printf("/* table %d */ {\n", table );
	for (i = 0; i < 64; i += 4) {
	    printf("    0x%08x, 0x%08x, 0x%08x, 0x%08x, \n",
		    PC_2H[table][i],   PC_2H[table][i+1],
		    PC_2H[table][i+2], PC_2H[table][i+3]);
	}
	printf("  },\n");
    }
}


int
main(void)
{
/*   make_pc2a(); */
   mktable();
   return 0;
}
