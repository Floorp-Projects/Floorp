/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdio.h"
#include "prtypes.h"
#include "blapi.h"

/*
 * what follows is code thrown together to generate the myriad of tables
 * used by Rijndael, the AES cipher.
 */

#define WORD_LE(b0, b1, b2, b3) \
    (((b3) << 24) | ((b2) << 16) | ((b1) << 8) | b0)

#define WORD_BE(b0, b1, b2, b3) \
    (((b0) << 24) | ((b1) << 16) | ((b2) << 8) | b3)

static const PRUint8 __S[256] = {
    99, 124, 119, 123, 242, 107, 111, 197, 48, 1, 103, 43, 254, 215, 171, 118,
    202, 130, 201, 125, 250, 89, 71, 240, 173, 212, 162, 175, 156, 164, 114, 192,
    183, 253, 147, 38, 54, 63, 247, 204, 52, 165, 229, 241, 113, 216, 49, 21,
    4, 199, 35, 195, 24, 150, 5, 154, 7, 18, 128, 226, 235, 39, 178, 117,
    9, 131, 44, 26, 27, 110, 90, 160, 82, 59, 214, 179, 41, 227, 47, 132,
    83, 209, 0, 237, 32, 252, 177, 91, 106, 203, 190, 57, 74, 76, 88, 207,
    208, 239, 170, 251, 67, 77, 51, 133, 69, 249, 2, 127, 80, 60, 159, 168,
    81, 163, 64, 143, 146, 157, 56, 245, 188, 182, 218, 33, 16, 255, 243, 210,
    205, 12, 19, 236, 95, 151, 68, 23, 196, 167, 126, 61, 100, 93, 25, 115,
    96, 129, 79, 220, 34, 42, 144, 136, 70, 238, 184, 20, 222, 94, 11, 219,
    224, 50, 58, 10, 73, 6, 36, 92, 194, 211, 172, 98, 145, 149, 228, 121,
    231, 200, 55, 109, 141, 213, 78, 169, 108, 86, 244, 234, 101, 122, 174, 8,
    186, 120, 37, 46, 28, 166, 180, 198, 232, 221, 116, 31, 75, 189, 139, 138,
    112, 62, 181, 102, 72, 3, 246, 14, 97, 53, 87, 185, 134, 193, 29, 158,
    225, 248, 152, 17, 105, 217, 142, 148, 155, 30, 135, 233, 206, 85, 40, 223,
    140, 161, 137, 13, 191, 230, 66, 104, 65, 153, 45, 15, 176, 84, 187, 22
};

static const PRUint8 __SInv[256] = {
    82, 9, 106, 213, 48, 54, 165, 56, 191, 64, 163, 158, 129, 243, 215, 251,
    124, 227, 57, 130, 155, 47, 255, 135, 52, 142, 67, 68, 196, 222, 233, 203,
    84, 123, 148, 50, 166, 194, 35, 61, 238, 76, 149, 11, 66, 250, 195, 78,
    8, 46, 161, 102, 40, 217, 36, 178, 118, 91, 162, 73, 109, 139, 209, 37,
    114, 248, 246, 100, 134, 104, 152, 22, 212, 164, 92, 204, 93, 101, 182, 146,
    108, 112, 72, 80, 253, 237, 185, 218, 94, 21, 70, 87, 167, 141, 157, 132,
    144, 216, 171, 0, 140, 188, 211, 10, 247, 228, 88, 5, 184, 179, 69, 6,
    208, 44, 30, 143, 202, 63, 15, 2, 193, 175, 189, 3, 1, 19, 138, 107,
    58, 145, 17, 65, 79, 103, 220, 234, 151, 242, 207, 206, 240, 180, 230, 115,
    150, 172, 116, 34, 231, 173, 53, 133, 226, 249, 55, 232, 28, 117, 223, 110,
    71, 241, 26, 113, 29, 41, 197, 137, 111, 183, 98, 14, 170, 24, 190, 27,
    252, 86, 62, 75, 198, 210, 121, 32, 154, 219, 192, 254, 120, 205, 90, 244,
    31, 221, 168, 51, 136, 7, 199, 49, 177, 18, 16, 89, 39, 128, 236, 95,
    96, 81, 127, 169, 25, 181, 74, 13, 45, 229, 122, 159, 147, 201, 156, 239,
    160, 224, 59, 77, 174, 42, 245, 176, 200, 235, 187, 60, 131, 83, 153, 97,
    23, 43, 4, 126, 186, 119, 214, 38, 225, 105, 20, 99, 85, 33, 12, 125
};

/* GF_MULTIPLY
 *
 * multiply two bytes represented in GF(2**8), mod (x**4 + 1)
 */
PRUint8
gf_multiply(PRUint8 a, PRUint8 b)
{
    PRUint8 res = 0;
    while (b > 0) {
        res = (b & 0x01) ? res ^ a : res;
        a = (a & 0x80) ? ((a << 1) ^ 0x1b) : (a << 1);
        b >>= 1;
    }
    return res;
}

void
make_T_Table(char *table, const PRUint8 Sx[256], FILE *file,
             unsigned char m0, unsigned char m1,
             unsigned char m2, unsigned char m3)
{
    PRUint32 Ti;
    int i;
    fprintf(file, "#ifdef IS_LITTLE_ENDIAN\n");
    fprintf(file, "static const PRUint32 _T%s[256] = \n{\n", table);
    for (i = 0; i < 256; i++) {
        Ti = WORD_LE(gf_multiply(Sx[i], m0),
                     gf_multiply(Sx[i], m1),
                     gf_multiply(Sx[i], m2),
                     gf_multiply(Sx[i], m3));
        if (Ti == 0)
            fprintf(file, "0x00000000%c%c", (i == 255) ? ' ' : ',',
                    (i % 6 == 5) ? '\n' : ' ');
        else
            fprintf(file, "%#.8x%c%c", Ti, (i == 255) ? ' ' : ',',
                    (i % 6 == 5) ? '\n' : ' ');
    }
    fprintf(file, "\n};\n");
    fprintf(file, "#else\n");
    fprintf(file, "static const PRUint32 _T%s[256] = \n{\n", table);
    for (i = 0; i < 256; i++) {
        Ti = WORD_BE(gf_multiply(Sx[i], m0),
                     gf_multiply(Sx[i], m1),
                     gf_multiply(Sx[i], m2),
                     gf_multiply(Sx[i], m3));
        if (Ti == 0)
            fprintf(file, "0x00000000%c%c", (i == 255) ? ' ' : ',',
                    (i % 6 == 5) ? '\n' : ' ');
        else
            fprintf(file, "%#.8x%c%c", Ti, (i == 255) ? ' ' : ',',
                    (i % 6 == 5) ? '\n' : ' ');
    }
    fprintf(file, "\n};\n");
    fprintf(file, "#endif\n\n");
}

void
make_InvMixCol_Table(int num, FILE *file, PRUint8 m0, PRUint8 m1, PRUint8 m2, PRUint8 m3)
{
    PRUint16 i;
    PRUint8 b0, b1, b2, b3;
    fprintf(file, "#ifdef IS_LITTLE_ENDIAN\n");
    fprintf(file, "static const PRUint32 _IMXC%d[256] = \n{\n", num);
    for (i = 0; i < 256; i++) {
        b0 = gf_multiply(i, m0);
        b1 = gf_multiply(i, m1);
        b2 = gf_multiply(i, m2);
        b3 = gf_multiply(i, m3);
        fprintf(file, "0x%.2x%.2x%.2x%.2x%c%c", b3, b2, b1, b0, (i == 255) ? ' ' : ',', (i % 6 == 5) ? '\n' : ' ');
    }
    fprintf(file, "\n};\n");
    fprintf(file, "#else\n");
    fprintf(file, "static const PRUint32 _IMXC%d[256] = \n{\n", num);
    for (i = 0; i < 256; i++) {
        b0 = gf_multiply(i, m0);
        b1 = gf_multiply(i, m1);
        b2 = gf_multiply(i, m2);
        b3 = gf_multiply(i, m3);
        fprintf(file, "0x%.2x%.2x%.2x%.2x%c%c", b0, b1, b2, b3, (i == 255) ? ' ' : ',', (i % 6 == 5) ? '\n' : ' ');
    }
    fprintf(file, "\n};\n");
    fprintf(file, "#endif\n\n");
}

int
main()
{
    int i, j;
    PRUint8 cur, last;
    PRUint32 tmp;
    FILE *optfile;
    optfile = fopen("rijndael32.tab", "w");
    /* output S, if there are no T tables */
    fprintf(optfile, "#ifndef RIJNDAEL_INCLUDE_TABLES\n");
    fprintf(optfile, "static const PRUint8 _S[256] = \n{\n");
    for (i = 0; i < 256; i++) {
        fprintf(optfile, "%3d%c%c", __S[i], (i == 255) ? ' ' : ',',
                (i % 16 == 15) ? '\n' : ' ');
    }
    fprintf(optfile, "};\n#endif /* not RIJNDAEL_INCLUDE_TABLES */\n\n");
    /* output S**-1 */
    fprintf(optfile, "static const PRUint8 _SInv[256] = \n{\n");
    for (i = 0; i < 256; i++) {
        fprintf(optfile, "%3d%c%c", __SInv[i], (i == 255) ? ' ' : ',',
                (i % 16 == 15) ? '\n' : ' ');
    }
    fprintf(optfile, "};\n\n");
    fprintf(optfile, "#ifdef RIJNDAEL_INCLUDE_TABLES\n");
    /* The 32-bit word tables for optimized implementation */
    /* T0 = [ S[a] * 02, S[a], S[a], S[a] * 03 ] */
    make_T_Table("0", __S, optfile, 0x02, 0x01, 0x01, 0x03);
    /* T1 = [ S[a] * 03, S[a] * 02, S[a], S[a] ] */
    make_T_Table("1", __S, optfile, 0x03, 0x02, 0x01, 0x01);
    /* T2 = [ S[a], S[a] * 03, S[a] * 02, S[a] ] */
    make_T_Table("2", __S, optfile, 0x01, 0x03, 0x02, 0x01);
    /* T3 = [ S[a], S[a], S[a] * 03, S[a] * 02 ] */
    make_T_Table("3", __S, optfile, 0x01, 0x01, 0x03, 0x02);
    /* TInv0 = [ Si[a] * 0E, Si[a] * 09, Si[a] * 0D, Si[a] * 0B ] */
    make_T_Table("Inv0", __SInv, optfile, 0x0e, 0x09, 0x0d, 0x0b);
    /* TInv1 = [ Si[a] * 0B, Si[a] * 0E, Si[a] * 09, Si[a] * 0D ] */
    make_T_Table("Inv1", __SInv, optfile, 0x0b, 0x0e, 0x09, 0x0d);
    /* TInv2 = [ Si[a] * 0D, Si[a] * 0B, Si[a] * 0E, Si[a] * 09 ] */
    make_T_Table("Inv2", __SInv, optfile, 0x0d, 0x0b, 0x0e, 0x09);
    /* TInv3 = [ Si[a] * 09, Si[a] * 0D, Si[a] * 0B, Si[a] * 0E ] */
    make_T_Table("Inv3", __SInv, optfile, 0x09, 0x0d, 0x0b, 0x0e);
    /* byte multiply tables for inverse key expansion (mimics InvMixColumn) */
    make_InvMixCol_Table(0, optfile, 0x0e, 0x09, 0x0d, 0x0b);
    make_InvMixCol_Table(1, optfile, 0x0b, 0x0E, 0x09, 0x0d);
    make_InvMixCol_Table(2, optfile, 0x0d, 0x0b, 0x0e, 0x09);
    make_InvMixCol_Table(3, optfile, 0x09, 0x0d, 0x0b, 0x0e);
    fprintf(optfile, "#endif /* RIJNDAEL_INCLUDE_TABLES */\n\n");
    /* round constants for key expansion */
    fprintf(optfile, "#ifdef IS_LITTLE_ENDIAN\n");
    fprintf(optfile, "static const PRUint32 Rcon[30] = {\n");
    cur = 0x01;
    for (i = 0; i < 30; i++) {
        fprintf(optfile, "%#.8x%c%c", WORD_LE(cur, 0, 0, 0),
                (i == 29) ? ' ' : ',', (i % 6 == 5) ? '\n' : ' ');
        last = cur;
        cur = gf_multiply(last, 0x02);
    }
    fprintf(optfile, "};\n");
    fprintf(optfile, "#else\n");
    fprintf(optfile, "static const PRUint32 Rcon[30] = {\n");
    cur = 0x01;
    for (i = 0; i < 30; i++) {
        fprintf(optfile, "%#.8x%c%c", WORD_BE(cur, 0, 0, 0),
                (i == 29) ? ' ' : ',', (i % 6 == 5) ? '\n' : ' ');
        last = cur;
        cur = gf_multiply(last, 0x02);
    }
    fprintf(optfile, "};\n");
    fprintf(optfile, "#endif\n\n");
    fclose(optfile);
    return 0;
}
