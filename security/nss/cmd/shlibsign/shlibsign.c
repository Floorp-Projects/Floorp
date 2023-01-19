/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * shlibsign creates the checksum (.chk) files for the NSS libraries,
 * libsoftokn3/softokn3 and libfreebl/freebl (platforms can have
 * multiple freebl variants), that contain the NSS cryptograhic boundary.
 *
 * The generated .chk files must be put in the same directory as
 * the NSS libraries they were generated for.
 *
 * When in FIPS 140 mode, the NSS Internal FIPS PKCS #11 Module will
 * compute the checksum for the NSS cryptographic boundary libraries
 * and compare the checksum with the value in .chk file.
 */

#ifdef XP_UNIX
#define USES_LINKS 1
#endif

#define COMPAT_MAJOR 0x01
#define COMPAT_MINOR 0x02

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef USES_LINKS
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

/* nspr headers */
#include "prlink.h"
#include "prprf.h"
#include "prenv.h"
#include "plgetopt.h"
#include "prinit.h"
#include "prmem.h"
#include "plstr.h"
#include "prerror.h"

/* softoken headers */
#include "pkcs11.h"
#include "pkcs11t.h"

/* freebl headers */
#include "shsign.h"

/* nss headers for definition of HASH_HashType */
#include "hasht.h"

CK_BBOOL cktrue = CK_TRUE;
CK_BBOOL ckfalse = CK_FALSE;
static PRBool verbose = PR_FALSE;
static PRBool verify = PR_FALSE;
static PRBool compat = PR_FALSE;

typedef struct HashTableStruct {
    char *name;
    CK_MECHANISM_TYPE hash;
    CK_MECHANISM_TYPE hmac;
    CK_KEY_TYPE keyType;
    HASH_HashType hashType;
    CK_ULONG hashLength;
} HashTable;

#define CKR_INTERNAL_OUT_FAILURE 0x80111111
#define CKR_INTERNAL_IN_FAILURE 0x80222222
#define CKM_SHA1 CKM_SHA_1
#define CKM_SHA1_HMAC CKM_SHA_1_HMAC
#define CKK_SHA1_HMAC CKK_SHA_1_HMAC
#define MKHASH(name, mech)                                   \
    {                                                        \
        name, CKM_##mech, CKM_##mech##_HMAC,                 \
            CKK_##mech##_HMAC, HASH_Alg##mech, mech##_LENGTH \
    }
static HashTable hashTable[] = {
    MKHASH("sha-1", SHA1), MKHASH("sha1", SHA1), MKHASH("sha224", SHA224),
    MKHASH("sha256", SHA256), MKHASH("sha384", SHA384),
    MKHASH("sha512", SHA512)
};
static size_t hashTableSize = PR_ARRAY_SIZE(hashTable);

const HashTable *
findHash(const char *hashName)
{
    int i;

    for (i = 0; i < hashTableSize; i++) {
        if (PL_strcasecmp(hashTable[i].name, hashName) == 0) {
            return &hashTable[i];
        }
    }
    return NULL;
}

static void
usage(const char *program_name)
{
    int i;
    const char *comma = "";
    PRFileDesc *debug_out = PR_GetSpecialFD(PR_StandardError);
    PR_fprintf(debug_out,
               "type %s -H for more detail information.\n", program_name);
    PR_fprintf(debug_out,
               "Usage: %s [-v] [-V] [-o outfile] [-d dbdir] [-f pwfile]\n"
               "          [-F] [-p pwd] -[P dbprefix ] [-t hash]"
               "          [-D] [-k keysize] [-c]"
               "-i shared_library_name\n",
               program_name);
    PR_fprintf(debug_out, "Valid Hashes: ");
    for (i = 0; i < hashTableSize; i++) {
        PR_fprintf(debug_out, "%s%s", comma, hashTable[i].name);
        comma = ", ";
    }
    PR_fprintf(debug_out, "\n");
    exit(1);
}

static void
long_usage(const char *program_name)
{
    PRFileDesc *debug_out = PR_GetSpecialFD(PR_StandardError);
    int i;
    const char *comma = "";
    PR_fprintf(debug_out, "%s test program usage:\n", program_name);
    PR_fprintf(debug_out, "\t-i <infile>  shared_library_name to process\n");
    PR_fprintf(debug_out, "\t-o <outfile> checksum outfile\n");
    PR_fprintf(debug_out, "\t-d <path>    database path location\n");
    PR_fprintf(debug_out, "\t-t <hash>    Hash for HMAC/or DSA\n");
    PR_fprintf(debug_out, "\t-D           Sign with DSA rather than HMAC\n");
    PR_fprintf(debug_out, "\t-k <keysize> size of the DSA key\n");
    PR_fprintf(debug_out, "\t-c           Use compatible versions for old NSS\n");
    PR_fprintf(debug_out, "\t-P <prefix>  database prefix\n");
    PR_fprintf(debug_out, "\t-f <file>    password File : echo pw > file \n");
    PR_fprintf(debug_out, "\t-F           FIPS mode\n");
    PR_fprintf(debug_out, "\t-p <pwd>     password\n");
    PR_fprintf(debug_out, "\t-v           verbose output\n");
    PR_fprintf(debug_out, "\t-V           perform Verify operations\n");
    PR_fprintf(debug_out, "\t-?           short help message\n");
    PR_fprintf(debug_out, "\t-h           short help message\n");
    PR_fprintf(debug_out, "\t-H           this help message\n");
    PR_fprintf(debug_out, "\n\n\tNote: Use of FIPS mode requires your ");
    PR_fprintf(debug_out, "library path is using \n");
    PR_fprintf(debug_out, "\t      pre-existing libraries with generated ");
    PR_fprintf(debug_out, "checksum files\n");
    PR_fprintf(debug_out, "\t      and database in FIPS mode \n");
    PR_fprintf(debug_out, "Valid Hashes: ");
    for (i = 0; i < hashTableSize; i++) {
        PR_fprintf(debug_out, "%s%s", comma, hashTable[i].name);
        comma = ", ";
    }
    PR_fprintf(debug_out, "\n");
    exit(1);
}

static char *
mkoutput(const char *input)
{
    int in_len = strlen(input);
    char *output = PR_Malloc(in_len + sizeof(SGN_SUFFIX));
    int index = in_len + 1 - sizeof("." SHLIB_SUFFIX);

    if ((index > 0) &&
        (PL_strncmp(&input[index],
                    "." SHLIB_SUFFIX, sizeof("." SHLIB_SUFFIX)) == 0)) {
        in_len = index;
    }
    memcpy(output, input, in_len);
    memcpy(&output[in_len], SGN_SUFFIX, sizeof(SGN_SUFFIX));
    return output;
}

static void
lperror(const char *string)
{
    PRErrorCode errorcode;

    errorcode = PR_GetError();
    PR_fprintf(PR_STDERR, "%s: %d: %s\n", string, errorcode,
               PR_ErrorToString(errorcode, PR_LANGUAGE_I_DEFAULT));
}

static void
encodeInt(unsigned char *buf, int val)
{
    buf[3] = (val >> 0) & 0xff;
    buf[2] = (val >> 8) & 0xff;
    buf[1] = (val >> 16) & 0xff;
    buf[0] = (val >> 24) & 0xff;
    return;
}

static PRStatus
writeItem(PRFileDesc *fd, CK_VOID_PTR pValue, CK_ULONG ulValueLen)
{
    unsigned char buf[4];
    int bytesWritten;
    if (ulValueLen == 0) {
        PR_fprintf(PR_STDERR, "call to writeItem with 0 bytes of data.\n");
        return PR_FAILURE;
    }

    encodeInt(buf, ulValueLen);
    bytesWritten = PR_Write(fd, buf, 4);
    if (bytesWritten != 4) {
        return PR_FAILURE;
    }
    bytesWritten = PR_Write(fd, pValue, ulValueLen);
    if (bytesWritten < 0 || (CK_ULONG)bytesWritten != ulValueLen) {
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

static const unsigned char prime[] = { 0x00,
                                       0x97, 0x44, 0x1d, 0xcc, 0x0d, 0x39, 0x0d, 0x8d,
                                       0xcb, 0x75, 0xdc, 0x24, 0x25, 0x6f, 0x01, 0x92,
                                       0xa1, 0x11, 0x07, 0x6b, 0x70, 0xac, 0x73, 0xd7,
                                       0x82, 0x28, 0xdf, 0xab, 0x82, 0x0c, 0x41, 0x0c,
                                       0x95, 0xb3, 0x3c, 0x3d, 0xea, 0x8a, 0xe6, 0x44,
                                       0x0a, 0xb8, 0xab, 0x90, 0x15, 0x41, 0x11, 0xe8,
                                       0x48, 0x7b, 0x8d, 0xb0, 0x9c, 0xd3, 0xf2, 0x69,
                                       0x66, 0xff, 0x66, 0x4b, 0x70, 0x2b, 0xbf, 0xfb,
                                       0xd6, 0x68, 0x85, 0x76, 0x1e, 0x34, 0xaa, 0xc5,
                                       0x57, 0x6e, 0x23, 0x02, 0x08, 0x60, 0x6e, 0xfd,
                                       0x67, 0x76, 0xe1, 0x7c, 0xc8, 0xcb, 0x51, 0x77,
                                       0xcf, 0xb1, 0x3b, 0x00, 0x2e, 0xfa, 0x21, 0xcd,
                                       0x34, 0x76, 0x75, 0x01, 0x19, 0xfe, 0xf8, 0x5d,
                                       0x43, 0xc5, 0x34, 0xf3, 0x7a, 0x95, 0xdc, 0xc2,
                                       0x58, 0x07, 0x19, 0x2f, 0x1d, 0x6f, 0x9a, 0x77,
                                       0x7e, 0x55, 0xaa, 0xe7, 0x5a, 0x50, 0x43, 0xd3 };

static const unsigned char subprime[] = { 0x0,
                                          0xd8, 0x16, 0x23, 0x34, 0x8a, 0x9e, 0x3a, 0xf5,
                                          0xd9, 0x10, 0x13, 0x35, 0xaa, 0xf3, 0xf3, 0x54,
                                          0x0b, 0x31, 0x24, 0xf1 };

static const unsigned char base[] = {
    0x03, 0x3a, 0xad, 0xfa, 0x3a, 0x0c, 0xea, 0x0a,
    0x4e, 0x43, 0x32, 0x92, 0xbb, 0x87, 0xf1, 0x11,
    0xc0, 0xad, 0x39, 0x38, 0x56, 0x1a, 0xdb, 0x23,
    0x66, 0xb1, 0x08, 0xda, 0xb6, 0x19, 0x51, 0x42,
    0x93, 0x4f, 0xc3, 0x44, 0x43, 0xa8, 0x05, 0xc1,
    0xf8, 0x71, 0x62, 0x6f, 0x3d, 0xe2, 0xab, 0x6f,
    0xd7, 0x80, 0x22, 0x6f, 0xca, 0x0d, 0xf6, 0x9f,
    0x45, 0x27, 0x83, 0xec, 0x86, 0x0c, 0xda, 0xaa,
    0xd6, 0xe0, 0xd0, 0x84, 0xfd, 0xb1, 0x4f, 0xdc,
    0x08, 0xcd, 0x68, 0x3a, 0x77, 0xc2, 0xc5, 0xf1,
    0x99, 0x0f, 0x15, 0x1b, 0x6a, 0x8c, 0x3d, 0x18,
    0x2b, 0x6f, 0xdc, 0x2b, 0xd8, 0xb5, 0x9b, 0xb8,
    0x2d, 0x57, 0x92, 0x1c, 0x46, 0x27, 0xaf, 0x6d,
    0xe1, 0x45, 0xcf, 0x0b, 0x3f, 0xfa, 0x07, 0xcc,
    0x14, 0x8e, 0xe7, 0xb8, 0xaa, 0xd5, 0xd1, 0x36,
    0x1d, 0x7e, 0x5e, 0x7d, 0xfa, 0x5b, 0x77, 0x1f
};

/*
 * The constants h, seed, & counter aren't used in the code; they're provided
 * here (commented-out) so that human readers can verify that our our PQG
 * parameters were generated properly.
static const unsigned char h[] = {
    0x41, 0x87, 0x47, 0x79, 0xd8, 0xba, 0x4e, 0xac,
    0x44, 0x4f, 0x6b, 0xd2, 0x16, 0x5e, 0x04, 0xc6,
    0xc2, 0x29, 0x93, 0x5e, 0xbd, 0xc7, 0xa9, 0x8f,
    0x23, 0xa1, 0xc8, 0xee, 0x80, 0x64, 0xd5, 0x67,
    0x3c, 0xba, 0x59, 0x9a, 0x06, 0x0c, 0xcc, 0x29,
    0x56, 0xc0, 0xb2, 0x21, 0xe0, 0x5b, 0x52, 0xcd,
    0x84, 0x73, 0x57, 0xfd, 0xd8, 0xc3, 0x5b, 0x13,
    0x54, 0xd7, 0x4a, 0x06, 0x86, 0x63, 0x09, 0xa5,
    0xb0, 0x59, 0xe2, 0x32, 0x9e, 0x09, 0xa3, 0x9f,
    0x49, 0x62, 0xcc, 0xa6, 0xf9, 0x54, 0xd5, 0xb2,
    0xc3, 0x08, 0x71, 0x7e, 0xe3, 0x37, 0x50, 0xd6,
    0x7b, 0xa7, 0xc2, 0x60, 0xc1, 0xeb, 0x51, 0x32,
    0xfa, 0xad, 0x35, 0x25, 0x17, 0xf0, 0x7f, 0x23,
    0xe5, 0xa8, 0x01, 0x52, 0xcf, 0x2f, 0xd9, 0xa9,
    0xf6, 0x00, 0x21, 0x15, 0xf1, 0xf7, 0x70, 0xb7,
    0x57, 0x8a, 0xd0, 0x59, 0x6a, 0x82, 0xdc, 0x9c };

static const unsigned char seed[] = { 0x00,
    0xcc, 0x4c, 0x69, 0x74, 0xf6, 0x72, 0x24, 0x68,
    0x24, 0x4f, 0xd7, 0x50, 0x11, 0x40, 0x81, 0xed,
    0x19, 0x3c, 0x8a, 0x25, 0xbc, 0x78, 0x0a, 0x85,
    0x82, 0x53, 0x70, 0x20, 0xf6, 0x54, 0xa5, 0x1b,
    0xf4, 0x15, 0xcd, 0xff, 0xc4, 0x88, 0xa7, 0x9d,
    0xf3, 0x47, 0x1c, 0x0a, 0xbe, 0x10, 0x29, 0x83,
    0xb9, 0x0f, 0x4c, 0xdf, 0x90, 0x16, 0x83, 0xa2,
    0xb3, 0xe3, 0x2e, 0xc1, 0xc2, 0x24, 0x6a, 0xc4,
    0x9d, 0x57, 0xba, 0xcb, 0x0f, 0x18, 0x75, 0x00,
    0x33, 0x46, 0x82, 0xec, 0xd6, 0x94, 0x77, 0xc3,
    0x4f, 0x4c, 0x58, 0x1c, 0x7f, 0x61, 0x3c, 0x36,
    0xd5, 0x2f, 0xa5, 0x66, 0xd8, 0x2f, 0xce, 0x6e,
    0x8e, 0x20, 0x48, 0x4a, 0xbb, 0xe3, 0xe0, 0xb2,
    0x50, 0x33, 0x63, 0x8a, 0x5b, 0x2d, 0x6a, 0xbe,
    0x4c, 0x28, 0x81, 0x53, 0x5b, 0xe4, 0xf6, 0xfc,
    0x64, 0x06, 0x13, 0x51, 0xeb, 0x4a, 0x91, 0x9c };

static const unsigned int counter=1496;
 */

static const unsigned char prime2[] = { 0x00,
                                        0xa4, 0xc2, 0x83, 0x4f, 0x36, 0xd3, 0x4f, 0xae,
                                        0xa0, 0xb1, 0x47, 0x43, 0xa8, 0x15, 0xee, 0xad,
                                        0xa3, 0x98, 0xa3, 0x29, 0x45, 0xae, 0x5c, 0xd9,
                                        0x12, 0x99, 0x09, 0xdc, 0xef, 0x05, 0xb4, 0x98,
                                        0x05, 0xaa, 0x07, 0xaa, 0x83, 0x89, 0xd7, 0xba,
                                        0xd1, 0x25, 0x56, 0x58, 0xd1, 0x73, 0x3c, 0xd0,
                                        0x91, 0x65, 0xbe, 0x27, 0x92, 0x94, 0x86, 0x95,
                                        0xdb, 0xcf, 0x07, 0x13, 0xa0, 0x85, 0xd6, 0xaa,
                                        0x6c, 0x1d, 0x63, 0xbf, 0xdd, 0xdf, 0xbc, 0x30,
                                        0xeb, 0x42, 0x2f, 0x52, 0x11, 0xec, 0x6e, 0x65,
                                        0xdf, 0x50, 0xbe, 0x28, 0x3d, 0xa4, 0xec, 0x45,
                                        0x19, 0x4c, 0x13, 0x0f, 0x59, 0x74, 0x57, 0x69,
                                        0x99, 0x4f, 0x4a, 0x74, 0x7f, 0x8c, 0x9e, 0xa2,
                                        0xe7, 0x94, 0xc9, 0x70, 0x70, 0xd0, 0xc4, 0xda,
                                        0x49, 0x5b, 0x7a, 0x7d, 0xd9, 0x71, 0x7c, 0x3b,
                                        0xdc, 0xd2, 0x8a, 0x74, 0x5f, 0xce, 0x09, 0xa2,
                                        0xdb, 0xec, 0xa4, 0xba, 0x75, 0xaa, 0x0a, 0x97,
                                        0xa6, 0x82, 0x25, 0x90, 0x90, 0x37, 0xe4, 0x40,
                                        0x05, 0x28, 0x8f, 0x98, 0x8e, 0x68, 0x01, 0xaf,
                                        0x9b, 0x08, 0x2a, 0x9b, 0xd5, 0xb9, 0x8c, 0x14,
                                        0xbf, 0xba, 0xcb, 0x5b, 0xda, 0x4c, 0x95, 0xb8,
                                        0xdf, 0x67, 0xa6, 0x6b, 0x76, 0x8c, 0xad, 0x4f,
                                        0xfd, 0x6a, 0xd6, 0xcc, 0x62, 0x71, 0x30, 0x30,
                                        0xc1, 0x29, 0x84, 0xe4, 0x8e, 0x32, 0x51, 0xb6,
                                        0xea, 0xfa, 0xba, 0x00, 0x99, 0x76, 0xea, 0x86,
                                        0x90, 0xab, 0x2d, 0xe9, 0xfd, 0x1e, 0x8c, 0xcc,
                                        0x3c, 0x2b, 0x5d, 0x13, 0x1b, 0x47, 0xb4, 0xf5,
                                        0x09, 0x74, 0x1d, 0xd4, 0x78, 0xb2, 0x42, 0x19,
                                        0xd6, 0x24, 0xd1, 0x68, 0xbf, 0x11, 0xf1, 0x38,
                                        0xa0, 0x44, 0x9c, 0xc6, 0x51, 0x33, 0xaa, 0x42,
                                        0x93, 0x9e, 0x30, 0x58, 0x9e, 0xc0, 0x70, 0xdf,
                                        0x7e, 0x64, 0xb1, 0xd8, 0x68, 0x75, 0x98, 0xa7 };

static const unsigned char subprime2[] = { 0x00,
                                           0x8e, 0xab, 0xf4, 0xbe, 0x45, 0xeb, 0xa3, 0x58,
                                           0x4e, 0x60, 0x15, 0x66, 0x5a, 0x4b, 0x25, 0xcf,
                                           0x45, 0x77, 0x89, 0x3f, 0x73, 0x34, 0x4a, 0xe0,
                                           0x9e, 0xac, 0xfd, 0xdc, 0xff, 0x9c, 0x8d, 0xe7 };

static const unsigned char base2[] = { 0x00,
                                       0x8d, 0x72, 0x32, 0x46, 0xa6, 0x5c, 0x80, 0xe3,
                                       0x43, 0x0a, 0x9e, 0x94, 0x35, 0x86, 0xd4, 0x58,
                                       0xa1, 0xca, 0x22, 0xb9, 0x73, 0x46, 0x0b, 0xfb,
                                       0x3e, 0x33, 0xf1, 0xd5, 0xd3, 0xb4, 0x26, 0xbf,
                                       0x50, 0xd7, 0xf2, 0x09, 0x33, 0x6e, 0xc0, 0x31,
                                       0x1b, 0x6d, 0x07, 0x70, 0x86, 0xca, 0x57, 0xf7,
                                       0x0b, 0x4a, 0x63, 0xf0, 0x6f, 0xc8, 0x8a, 0xed,
                                       0x50, 0x60, 0xf3, 0x11, 0xc7, 0x44, 0xf3, 0xce,
                                       0x4e, 0x50, 0x42, 0x2d, 0x85, 0x33, 0x54, 0x57,
                                       0x03, 0x8d, 0xdc, 0x66, 0x4d, 0x61, 0x83, 0x17,
                                       0x1c, 0x7b, 0x0d, 0x65, 0xbc, 0x8f, 0x2c, 0x19,
                                       0x86, 0xfc, 0xe2, 0x9f, 0x5d, 0x67, 0xfc, 0xd4,
                                       0xa5, 0xf8, 0x23, 0xa1, 0x1a, 0xa2, 0xe1, 0x11,
                                       0x15, 0x84, 0x32, 0x01, 0xee, 0x88, 0xf1, 0x55,
                                       0x30, 0xe9, 0x74, 0x3c, 0x1a, 0x2b, 0x54, 0x45,
                                       0x2e, 0x39, 0xb9, 0x77, 0xe1, 0x32, 0xaf, 0x2d,
                                       0x97, 0xe0, 0x21, 0xec, 0xf5, 0x58, 0xe1, 0xc7,
                                       0x2e, 0xe0, 0x71, 0x3d, 0x29, 0xa4, 0xd6, 0xe2,
                                       0x5f, 0x85, 0x9c, 0x05, 0x04, 0x46, 0x41, 0x89,
                                       0x03, 0x3c, 0xfa, 0xb2, 0xcf, 0xfa, 0xd5, 0x67,
                                       0xcc, 0xec, 0x68, 0xfc, 0x83, 0xd9, 0x1f, 0x2e,
                                       0x4e, 0x9a, 0x5e, 0x77, 0xa1, 0xff, 0xe6, 0x6f,
                                       0x04, 0x8b, 0xf9, 0x6b, 0x47, 0xc6, 0x49, 0xd2,
                                       0x88, 0x6e, 0x29, 0xa3, 0x1b, 0xae, 0xe0, 0x4f,
                                       0x72, 0x8a, 0x28, 0x94, 0x0c, 0x1d, 0x8c, 0x99,
                                       0xa2, 0x6f, 0xf8, 0xba, 0x99, 0x90, 0xc7, 0xe5,
                                       0xb1, 0x3c, 0x10, 0x34, 0x86, 0x6a, 0x6a, 0x1f,
                                       0x39, 0x63, 0x58, 0xe1, 0x5e, 0x97, 0x95, 0x45,
                                       0x40, 0x38, 0x45, 0x6f, 0x02, 0xb5, 0x86, 0x6e,
                                       0xae, 0x2f, 0x32, 0x7e, 0xa1, 0x3a, 0x34, 0x2c,
                                       0x1c, 0xd3, 0xff, 0x4e, 0x2c, 0x38, 0x1c, 0xaa,
                                       0x2e, 0x66, 0xbe, 0x32, 0x3e, 0x3c, 0x06, 0x5f };

/*
 * The constants h2, seed2, & counter2 aren't used in the code; they're provided
 * here (commented-out) so that human readers can verify that our our PQG
 * parameters were generated properly.
static const unsigned char h2[] = {
    0x30, 0x91, 0xa1, 0x2e, 0x40, 0xa5, 0x7d, 0xf7,
    0xdc, 0xed, 0xee, 0x05, 0xc2, 0x31, 0x91, 0x37,
    0xda, 0xc5, 0xe3, 0x47, 0xb5, 0x35, 0x4b, 0xfd,
    0x18, 0xb2, 0x7e, 0x67, 0x1e, 0x92, 0x22, 0xe7,
    0xf5, 0x00, 0x71, 0xc0, 0x86, 0x8d, 0x90, 0x31,
    0x36, 0x3e, 0xd0, 0x94, 0x5d, 0x2f, 0x9a, 0x68,
    0xd2, 0xf8, 0x3d, 0x5e, 0x84, 0x42, 0x35, 0xda,
    0x75, 0xdd, 0x05, 0xf0, 0x03, 0x31, 0x39, 0xe5,
    0xfd, 0x2f, 0x5a, 0x7d, 0x56, 0xd8, 0x26, 0xa0,
    0x51, 0x5e, 0x32, 0xb4, 0xad, 0xee, 0xd4, 0x89,
    0xae, 0x01, 0x7f, 0xac, 0x86, 0x98, 0x77, 0x26,
    0x5c, 0x31, 0xd2, 0x5e, 0xbb, 0x7f, 0xf5, 0x4c,
    0x9b, 0xf0, 0xa6, 0x37, 0x34, 0x08, 0x86, 0x6b,
    0xce, 0xeb, 0x85, 0x66, 0x0a, 0x26, 0x8a, 0x14,
    0x92, 0x12, 0x74, 0xf4, 0xf0, 0xcb, 0xb5, 0xfc,
    0x38, 0xd5, 0x1e, 0xa1, 0x2f, 0x4a, 0x1a, 0xca,
    0x66, 0xde, 0x6e, 0xe6, 0x6e, 0x1c, 0xef, 0x50,
    0x41, 0x31, 0x09, 0xe7, 0x4a, 0xb8, 0xa3, 0xaa,
    0x5a, 0x22, 0xbd, 0x63, 0x0f, 0xe9, 0x0e, 0xdb,
    0xb3, 0xca, 0x7e, 0x8d, 0x40, 0xb3, 0x3e, 0x0b,
    0x12, 0x8b, 0xb0, 0x80, 0x4d, 0x6d, 0xb0, 0x54,
    0xbb, 0x4c, 0x1d, 0x6c, 0xa0, 0x5c, 0x9d, 0x91,
    0xb3, 0xbb, 0xd9, 0xfc, 0x60, 0xec, 0xc1, 0xbc,
    0xae, 0x72, 0x3f, 0xa5, 0x4f, 0x36, 0x2d, 0x2c,
    0x81, 0x03, 0x86, 0xa2, 0x03, 0x38, 0x36, 0x8e,
    0xad, 0x1d, 0x53, 0xc6, 0xc5, 0x9e, 0xda, 0x08,
    0x35, 0x4f, 0xb2, 0x78, 0xba, 0xd1, 0x22, 0xde,
    0xc4, 0x6b, 0xbe, 0x83, 0x71, 0x0f, 0xee, 0x38,
    0x4a, 0x9f, 0xda, 0x90, 0x93, 0x6b, 0x9a, 0xf2,
    0xeb, 0x23, 0xfe, 0x41, 0x3f, 0xf1, 0xfc, 0xee,
    0x7f, 0x67, 0xa7, 0xb8, 0xab, 0x29, 0xf4, 0x75,
    0x1c, 0xe9, 0xd1, 0x47, 0x7d, 0x86, 0x44, 0xe2 };

static const unsigned char seed2[] = { 0x00,
    0xbc, 0xae, 0xc4, 0xea, 0x4e, 0xd2, 0xed, 0x1c,
    0x8d, 0x48, 0xed, 0xf2, 0xa5, 0xb4, 0x18, 0xba,
    0x00, 0xcb, 0x9c, 0x75, 0x8a, 0x39, 0x94, 0x3b,
    0xd0, 0xd6, 0x01, 0xf7, 0xc1, 0xf5, 0x9d, 0xe5,
    0xe3, 0xb4, 0x1d, 0xf5, 0x30, 0xfe, 0x99, 0xe4,
    0x01, 0xab, 0xc0, 0x88, 0x4e, 0x67, 0x8f, 0xc6,
    0x72, 0x39, 0x2e, 0xac, 0x51, 0xec, 0x91, 0x41,
    0x47, 0x71, 0x14, 0x8a, 0x1d, 0xca, 0x88, 0x15,
    0xea, 0xc9, 0x48, 0x9a, 0x71, 0x50, 0x19, 0x38,
    0xdb, 0x4e, 0x65, 0xd5, 0x13, 0xd8, 0x2a, 0xc4,
    0xcd, 0xfd, 0x0c, 0xe3, 0xc3, 0x60, 0xae, 0x6d,
    0x88, 0xf2, 0x3a, 0xd0, 0x64, 0x73, 0x32, 0x89,
    0xcd, 0x0b, 0xb8, 0xc7, 0xa5, 0x27, 0x84, 0xd5,
    0x83, 0x3f, 0x0e, 0x10, 0x63, 0x10, 0x78, 0xac,
    0x6b, 0x56, 0xb2, 0x62, 0x3a, 0x44, 0x56, 0xc0,
    0xe4, 0x33, 0xd7, 0x63, 0x4c, 0xc9, 0x6b, 0xae,
    0xfb, 0xe2, 0x9b, 0xf4, 0x96, 0xc7, 0xf0, 0x2a,
    0x50, 0xde, 0x86, 0x69, 0x4f, 0x42, 0x4b, 0x1c,
    0x7c, 0xa8, 0x6a, 0xfb, 0x54, 0x47, 0x1b, 0x41,
    0x31, 0x9e, 0x0a, 0xc6, 0xc0, 0xbc, 0x88, 0x7f,
    0x5a, 0x42, 0xa9, 0x82, 0x58, 0x32, 0xb3, 0xeb,
    0x54, 0x83, 0x84, 0x26, 0x92, 0xa6, 0xc0, 0x6e,
    0x2b, 0xa6, 0x82, 0x82, 0x43, 0x58, 0x84, 0x53,
    0x31, 0xcf, 0xd0, 0x0a, 0x11, 0x09, 0x44, 0xc8,
    0x11, 0x36, 0xe0, 0x04, 0x85, 0x2e, 0xd1, 0x29,
    0x6b, 0x7b, 0x00, 0x71, 0x5f, 0xef, 0x7b, 0x7a,
    0x2d, 0x91, 0xf9, 0x84, 0x45, 0x4d, 0xc7, 0xe1,
    0xee, 0xd4, 0xb8, 0x61, 0x3b, 0x13, 0xb7, 0xba,
    0x95, 0x39, 0xf6, 0x3d, 0x89, 0xbd, 0xa5, 0x80,
    0x93, 0xf7, 0xe5, 0x17, 0x05, 0xc5, 0x65, 0xb7,
    0xde, 0xc9, 0x9f, 0x04, 0x87, 0xcf, 0x4f, 0x86,
    0xc3, 0x29, 0x7d, 0xb7, 0x89, 0xbf, 0xe3, 0xde };

static const unsigned int counter2=210;
 */

struct tuple_str {
    CK_RV errNum;
    const char *errString;
};

typedef struct tuple_str tuple_str;

static const tuple_str errStrings[] = {
    { CKR_OK, "CKR_OK                              " },
    { CKR_CANCEL, "CKR_CANCEL                          " },
    { CKR_HOST_MEMORY, "CKR_HOST_MEMORY                     " },
    { CKR_SLOT_ID_INVALID, "CKR_SLOT_ID_INVALID                 " },
    { CKR_GENERAL_ERROR, "CKR_GENERAL_ERROR                   " },
    { CKR_FUNCTION_FAILED, "CKR_FUNCTION_FAILED                 " },
    { CKR_ARGUMENTS_BAD, "CKR_ARGUMENTS_BAD                   " },
    { CKR_NO_EVENT, "CKR_NO_EVENT                        " },
    { CKR_NEED_TO_CREATE_THREADS, "CKR_NEED_TO_CREATE_THREADS          " },
    { CKR_CANT_LOCK, "CKR_CANT_LOCK                       " },
    { CKR_ATTRIBUTE_READ_ONLY, "CKR_ATTRIBUTE_READ_ONLY             " },
    { CKR_ATTRIBUTE_SENSITIVE, "CKR_ATTRIBUTE_SENSITIVE             " },
    { CKR_ATTRIBUTE_TYPE_INVALID, "CKR_ATTRIBUTE_TYPE_INVALID          " },
    { CKR_ATTRIBUTE_VALUE_INVALID, "CKR_ATTRIBUTE_VALUE_INVALID         " },
    { CKR_DATA_INVALID, "CKR_DATA_INVALID                    " },
    { CKR_DATA_LEN_RANGE, "CKR_DATA_LEN_RANGE                  " },
    { CKR_DEVICE_ERROR, "CKR_DEVICE_ERROR                    " },
    { CKR_DEVICE_MEMORY, "CKR_DEVICE_MEMORY                   " },
    { CKR_DEVICE_REMOVED, "CKR_DEVICE_REMOVED                  " },
    { CKR_ENCRYPTED_DATA_INVALID, "CKR_ENCRYPTED_DATA_INVALID          " },
    { CKR_ENCRYPTED_DATA_LEN_RANGE, "CKR_ENCRYPTED_DATA_LEN_RANGE        " },
    { CKR_FUNCTION_CANCELED, "CKR_FUNCTION_CANCELED               " },
    { CKR_FUNCTION_NOT_PARALLEL, "CKR_FUNCTION_NOT_PARALLEL           " },
    { CKR_FUNCTION_NOT_SUPPORTED, "CKR_FUNCTION_NOT_SUPPORTED          " },
    { CKR_KEY_HANDLE_INVALID, "CKR_KEY_HANDLE_INVALID              " },
    { CKR_KEY_SIZE_RANGE, "CKR_KEY_SIZE_RANGE                  " },
    { CKR_KEY_TYPE_INCONSISTENT, "CKR_KEY_TYPE_INCONSISTENT           " },
    { CKR_KEY_NOT_NEEDED, "CKR_KEY_NOT_NEEDED                  " },
    { CKR_KEY_CHANGED, "CKR_KEY_CHANGED                     " },
    { CKR_KEY_NEEDED, "CKR_KEY_NEEDED                      " },
    { CKR_KEY_INDIGESTIBLE, "CKR_KEY_INDIGESTIBLE                " },
    { CKR_KEY_FUNCTION_NOT_PERMITTED, "CKR_KEY_FUNCTION_NOT_PERMITTED      " },
    { CKR_KEY_NOT_WRAPPABLE, "CKR_KEY_NOT_WRAPPABLE               " },
    { CKR_KEY_UNEXTRACTABLE, "CKR_KEY_UNEXTRACTABLE               " },
    { CKR_MECHANISM_INVALID, "CKR_MECHANISM_INVALID               " },
    { CKR_MECHANISM_PARAM_INVALID, "CKR_MECHANISM_PARAM_INVALID         " },
    { CKR_OBJECT_HANDLE_INVALID, "CKR_OBJECT_HANDLE_INVALID           " },
    { CKR_OPERATION_ACTIVE, "CKR_OPERATION_ACTIVE                " },
    { CKR_OPERATION_NOT_INITIALIZED, "CKR_OPERATION_NOT_INITIALIZED       " },
    { CKR_PIN_INCORRECT, "CKR_PIN_INCORRECT                   " },
    { CKR_PIN_INVALID, "CKR_PIN_INVALID                     " },
    { CKR_PIN_LEN_RANGE, "CKR_PIN_LEN_RANGE                   " },
    { CKR_PIN_EXPIRED, "CKR_PIN_EXPIRED                     " },
    { CKR_PIN_LOCKED, "CKR_PIN_LOCKED                      " },
    { CKR_SESSION_CLOSED, "CKR_SESSION_CLOSED                  " },
    { CKR_SESSION_COUNT, "CKR_SESSION_COUNT                   " },
    { CKR_SESSION_HANDLE_INVALID, "CKR_SESSION_HANDLE_INVALID          " },
    { CKR_SESSION_PARALLEL_NOT_SUPPORTED, "CKR_SESSION_PARALLEL_NOT_SUPPORTED  " },
    { CKR_SESSION_READ_ONLY, "CKR_SESSION_READ_ONLY               " },
    { CKR_SESSION_EXISTS, "CKR_SESSION_EXISTS                  " },
    { CKR_SESSION_READ_ONLY_EXISTS, "CKR_SESSION_READ_ONLY_EXISTS        " },
    { CKR_SESSION_READ_WRITE_SO_EXISTS, "CKR_SESSION_READ_WRITE_SO_EXISTS    " },
    { CKR_SIGNATURE_INVALID, "CKR_SIGNATURE_INVALID               " },
    { CKR_SIGNATURE_LEN_RANGE, "CKR_SIGNATURE_LEN_RANGE             " },
    { CKR_TEMPLATE_INCOMPLETE, "CKR_TEMPLATE_INCOMPLETE             " },
    { CKR_TEMPLATE_INCONSISTENT, "CKR_TEMPLATE_INCONSISTENT           " },
    { CKR_TOKEN_NOT_PRESENT, "CKR_TOKEN_NOT_PRESENT               " },
    { CKR_TOKEN_NOT_RECOGNIZED, "CKR_TOKEN_NOT_RECOGNIZED            " },
    { CKR_TOKEN_WRITE_PROTECTED, "CKR_TOKEN_WRITE_PROTECTED           " },
    { CKR_UNWRAPPING_KEY_HANDLE_INVALID, "CKR_UNWRAPPING_KEY_HANDLE_INVALID   " },
    { CKR_UNWRAPPING_KEY_SIZE_RANGE, "CKR_UNWRAPPING_KEY_SIZE_RANGE       " },
    { CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT, "CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT" },
    { CKR_USER_ALREADY_LOGGED_IN, "CKR_USER_ALREADY_LOGGED_IN          " },
    { CKR_USER_NOT_LOGGED_IN, "CKR_USER_NOT_LOGGED_IN              " },
    { CKR_USER_PIN_NOT_INITIALIZED, "CKR_USER_PIN_NOT_INITIALIZED        " },
    { CKR_USER_TYPE_INVALID, "CKR_USER_TYPE_INVALID               " },
    { CKR_USER_ANOTHER_ALREADY_LOGGED_IN, "CKR_USER_ANOTHER_ALREADY_LOGGED_IN  " },
    { CKR_USER_TOO_MANY_TYPES, "CKR_USER_TOO_MANY_TYPES             " },
    { CKR_WRAPPED_KEY_INVALID, "CKR_WRAPPED_KEY_INVALID             " },
    { CKR_WRAPPED_KEY_LEN_RANGE, "CKR_WRAPPED_KEY_LEN_RANGE           " },
    { CKR_WRAPPING_KEY_HANDLE_INVALID, "CKR_WRAPPING_KEY_HANDLE_INVALID     " },
    { CKR_WRAPPING_KEY_SIZE_RANGE, "CKR_WRAPPING_KEY_SIZE_RANGE         " },
    { CKR_WRAPPING_KEY_TYPE_INCONSISTENT, "CKR_WRAPPING_KEY_TYPE_INCONSISTENT  " },
    { CKR_RANDOM_SEED_NOT_SUPPORTED, "CKR_RANDOM_SEED_NOT_SUPPORTED       " },
    { CKR_RANDOM_NO_RNG, "CKR_RANDOM_NO_RNG                   " },
    { CKR_DOMAIN_PARAMS_INVALID, "CKR_DOMAIN_PARAMS_INVALID           " },
    { CKR_BUFFER_TOO_SMALL, "CKR_BUFFER_TOO_SMALL                " },
    { CKR_SAVED_STATE_INVALID, "CKR_SAVED_STATE_INVALID             " },
    { CKR_INFORMATION_SENSITIVE, "CKR_INFORMATION_SENSITIVE           " },
    { CKR_STATE_UNSAVEABLE, "CKR_STATE_UNSAVEABLE                " },
    { CKR_CRYPTOKI_NOT_INITIALIZED, "CKR_CRYPTOKI_NOT_INITIALIZED        " },
    { CKR_CRYPTOKI_ALREADY_INITIALIZED, "CKR_CRYPTOKI_ALREADY_INITIALIZED    " },
    { CKR_MUTEX_BAD, "CKR_MUTEX_BAD                       " },
    { CKR_MUTEX_NOT_LOCKED, "CKR_MUTEX_NOT_LOCKED                " },
    { CKR_FUNCTION_REJECTED, "CKR_FUNCTION_REJECTED               " },
    { CKR_VENDOR_DEFINED, "CKR_VENDOR_DEFINED                  " },
    { 0xCE534351, "CKR_NSS_CERTDB_FAILED          " },
    { 0xCE534352, "CKR_NSS_KEYDB_FAILED           " }

};

static const CK_ULONG numStrings = sizeof(errStrings) / sizeof(tuple_str);

/* Returns constant error string for "CRV".
 * Returns "unknown error" if errNum is unknown.
 */
static const char *
CK_RVtoStr(CK_RV errNum)
{
    CK_ULONG low = 1;
    CK_ULONG high = numStrings - 1;
    CK_ULONG i;
    CK_RV num;
    static int initDone;

    /* make sure table is in  ascending order.
     * binary search depends on it.
     */
    if (!initDone) {
        CK_RV lastNum = CKR_OK;
        for (i = low; i <= high; ++i) {
            num = errStrings[i].errNum;
            if (num <= lastNum) {
                PR_fprintf(PR_STDERR,
                           "sequence error in error strings at item %d\n"
                           "error %d (%s)\n"
                           "should come after \n"
                           "error %d (%s)\n",
                           (int)i, (int)lastNum, errStrings[i - 1].errString,
                           (int)num, errStrings[i].errString);
            }
            lastNum = num;
        }
        initDone = 1;
    }

    /* Do binary search of table. */
    while (low + 1 < high) {
        i = low + (high - low) / 2;
        num = errStrings[i].errNum;
        if (errNum == num)
            return errStrings[i].errString;
        if (errNum < num)
            high = i;
        else
            low = i;
    }
    if (errNum == errStrings[low].errNum)
        return errStrings[low].errString;
    if (errNum == errStrings[high].errNum)
        return errStrings[high].errString;
    return "unknown error";
}

static void
pk11error(const char *string, CK_RV crv)
{
    PRErrorCode errorcode;

    PR_fprintf(PR_STDERR, "%s: 0x%08lX, %-26s\n", string, crv, CK_RVtoStr(crv));

    errorcode = PR_GetError();
    if (errorcode) {
        PR_fprintf(PR_STDERR, "NSPR error code: %d: %s\n", errorcode,
                   PR_ErrorToString(errorcode, PR_LANGUAGE_I_DEFAULT));
    }
}

static void
logIt(const char *fmt, ...)
{
    va_list args;

    if (verbose) {
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
}

static CK_RV
softokn_Init(CK_FUNCTION_LIST_PTR pFunctionList, const char *configDir,
             const char *dbPrefix)
{

    CK_RV crv = CKR_OK;
    CK_C_INITIALIZE_ARGS initArgs;
    char *moduleSpec = NULL;

    initArgs.CreateMutex = NULL;
    initArgs.DestroyMutex = NULL;
    initArgs.LockMutex = NULL;
    initArgs.UnlockMutex = NULL;
    initArgs.flags = CKF_OS_LOCKING_OK;
    if (configDir) {
        moduleSpec = PR_smprintf("configdir='%s' certPrefix='%s' "
                                 "keyPrefix='%s' secmod='secmod.db' flags=ReadOnly ",
                                 configDir, dbPrefix, dbPrefix);
    } else {
        moduleSpec = PR_smprintf("configdir='' certPrefix='' keyPrefix='' "
                                 "secmod='' flags=noCertDB, noModDB");
    }
    if (!moduleSpec) {
        PR_fprintf(PR_STDERR, "softokn_Init: out of memory error\n");
        return CKR_HOST_MEMORY;
    }
    logIt("moduleSpec %s\n", moduleSpec);
    initArgs.LibraryParameters = (CK_CHAR_PTR *)moduleSpec;
    initArgs.pReserved = NULL;

    crv = pFunctionList->C_Initialize(&initArgs);
    if (crv != CKR_OK) {
        pk11error("C_Initialize failed", crv);
        goto cleanup;
    }

cleanup:
    if (moduleSpec) {
        PR_smprintf_free(moduleSpec);
    }

    return crv;
}

static char *
filePasswd(char *pwFile)
{
    unsigned char phrase[500];
    PRFileDesc *fd;
    PRInt32 nb;
    int i;

    if (!pwFile)
        return 0;

    fd = PR_Open(pwFile, PR_RDONLY, 0);
    if (!fd) {
        lperror(pwFile);
        return NULL;
    }

    nb = PR_Read(fd, phrase, sizeof(phrase));

    PR_Close(fd);
    /* handle the Windows EOL case */
    i = 0;
    while (phrase[i] != '\r' && phrase[i] != '\n' && i < nb)
        i++;
    phrase[i] = '\0';
    if (nb == 0) {
        PR_fprintf(PR_STDERR, "password file contains no data\n");
        return NULL;
    }
    return (char *)PL_strdup((char *)phrase);
}

static void
checkPath(char *string)
{
    char *src;
    char *dest;

    /*
     * windows support convert any back slashes to
     * forward slashes.
     */
    for (src = string, dest = string; *src; src++, dest++) {
        if (*src == '\\') {
            *dest = '/';
        }
    }
    dest--;
    /* if the last char is a / set it to 0 */
    if (*dest == '/')
        *dest = 0;
}

static CK_SLOT_ID *
getSlotList(CK_FUNCTION_LIST_PTR pFunctionList,
            CK_ULONG slotIndex)
{
    CK_RV crv = CKR_OK;
    CK_SLOT_ID *pSlotList = NULL;
    CK_ULONG slotCount;

    /* Get slot list */
    crv = pFunctionList->C_GetSlotList(CK_FALSE /* all slots */,
                                       NULL, &slotCount);
    if (crv != CKR_OK) {
        pk11error("C_GetSlotList failed", crv);
        return NULL;
    }

    if (slotIndex >= slotCount) {
        PR_fprintf(PR_STDERR, "provided slotIndex is greater than the slot count.");
        return NULL;
    }

    pSlotList = (CK_SLOT_ID *)PR_Malloc(slotCount * sizeof(CK_SLOT_ID));
    if (!pSlotList) {
        lperror("failed to allocate slot list");
        return NULL;
    }
    crv = pFunctionList->C_GetSlotList(CK_FALSE /* all slots */,
                                       pSlotList, &slotCount);
    if (crv != CKR_OK) {
        pk11error("C_GetSlotList failed", crv);
        if (pSlotList)
            PR_Free(pSlotList);
        return NULL;
    }
    return pSlotList;
}

CK_RV
shlibSignDSA(CK_FUNCTION_LIST_PTR pFunctionList, CK_SLOT_ID slot,
             CK_SESSION_HANDLE hRwSession, int keySize, PRFileDesc *ifd,
             PRFileDesc *ofd, const HashTable *hash)
{
    CK_MECHANISM digestmech;
    CK_ULONG digestLen = 0;
    CK_BYTE digest[HASH_LENGTH_MAX];
    CK_BYTE sign[64]; /* DSA2 SIGNATURE LENGTH */
    CK_ULONG signLen = 0;
    CK_ULONG expectedSigLen = sizeof(sign);
    CK_MECHANISM signMech = {
        CKM_DSA, NULL, 0
    };
    int bytesRead;
    int bytesWritten;
    unsigned char file_buf[512];
    NSSSignChkHeader header;
    int count = 0;
    CK_RV crv = CKR_GENERAL_ERROR;
    PRStatus rv = PR_SUCCESS;
    const char *hashName = "sha256"; /* default hash value */
    int i;

    /*** DSA Key ***/
    CK_MECHANISM dsaKeyPairGenMech;
    CK_ATTRIBUTE dsaPubKeyTemplate[5];
    CK_ATTRIBUTE dsaPrivKeyTemplate[5];
    CK_OBJECT_HANDLE hDSApubKey = CK_INVALID_HANDLE;
    CK_OBJECT_HANDLE hDSAprivKey = CK_INVALID_HANDLE;
    CK_BYTE dsaPubKey[384];
    CK_ATTRIBUTE dsaPubKeyValue;

    if ((keySize == 0) || (keySize > 1024)) {
        CK_MECHANISM_INFO mechInfo;
        crv = pFunctionList->C_GetMechanismInfo(slot,
                                                CKM_DSA, &mechInfo);
        if (crv != CKR_OK) {
            pk11error("Couldn't get mechanism info for DSA", crv);
            return crv;
        }

        if (keySize && (mechInfo.ulMaxKeySize < keySize)) {
            PR_fprintf(PR_STDERR,
                       "token doesn't support DSA2 (Max key size=%d)\n",
                       mechInfo.ulMaxKeySize);
            return crv;
        }

        if ((keySize == 0) && mechInfo.ulMaxKeySize >= 2048) {
            keySize = 2048;
        } else {
            keySize = 1024;
        }
    }

    /* DSA key init */
    if (keySize == 1024) {
        dsaPubKeyTemplate[0].type = CKA_PRIME;
        dsaPubKeyTemplate[0].pValue = (CK_VOID_PTR)&prime;
        dsaPubKeyTemplate[0].ulValueLen = sizeof(prime);
        dsaPubKeyTemplate[1].type = CKA_SUBPRIME;
        dsaPubKeyTemplate[1].pValue = (CK_VOID_PTR)&subprime;
        dsaPubKeyTemplate[1].ulValueLen = sizeof(subprime);
        dsaPubKeyTemplate[2].type = CKA_BASE;
        dsaPubKeyTemplate[2].pValue = (CK_VOID_PTR)&base;
        dsaPubKeyTemplate[2].ulValueLen = sizeof(base);
        hashName = "sha-1"; /* use sha-1 for old dsa keys */
        expectedSigLen = 32;
    } else if (keySize == 2048) {
        dsaPubKeyTemplate[0].type = CKA_PRIME;
        dsaPubKeyTemplate[0].pValue = (CK_VOID_PTR)&prime2;
        dsaPubKeyTemplate[0].ulValueLen = sizeof(prime2);
        dsaPubKeyTemplate[1].type = CKA_SUBPRIME;
        dsaPubKeyTemplate[1].pValue = (CK_VOID_PTR)&subprime2;
        dsaPubKeyTemplate[1].ulValueLen = sizeof(subprime2);
        dsaPubKeyTemplate[2].type = CKA_BASE;
        dsaPubKeyTemplate[2].pValue = (CK_VOID_PTR)&base2;
        dsaPubKeyTemplate[2].ulValueLen = sizeof(base2);
        digestmech.mechanism = hash ? hash->hash : CKM_SHA256;
        digestmech.pParameter = NULL;
        digestmech.ulParameterLen = 0;
    } else {
        PR_fprintf(PR_STDERR, "Only keysizes 1024 and 2048 are supported");
        return CKR_GENERAL_ERROR;
    }
    if (hash == NULL) {
        hash = findHash(hashName);
    }
    if (hash == NULL) {
        PR_fprintf(PR_STDERR,
                   "Internal error,  couldn't find hash '%s' in table.\n",
                   hashName);
        return CKR_GENERAL_ERROR;
    }
    digestmech.mechanism = hash->hash;
    digestmech.pParameter = NULL;
    digestmech.ulParameterLen = 0;
    dsaPubKeyTemplate[3].type = CKA_TOKEN;
    dsaPubKeyTemplate[3].pValue = &ckfalse; /* session object */
    dsaPubKeyTemplate[3].ulValueLen = sizeof(ckfalse);
    dsaPubKeyTemplate[4].type = CKA_VERIFY;
    dsaPubKeyTemplate[4].pValue = &cktrue;
    dsaPubKeyTemplate[4].ulValueLen = sizeof(cktrue);
    dsaKeyPairGenMech.mechanism = CKM_DSA_KEY_PAIR_GEN;
    dsaKeyPairGenMech.pParameter = NULL;
    dsaKeyPairGenMech.ulParameterLen = 0;
    dsaPrivKeyTemplate[0].type = CKA_TOKEN;
    dsaPrivKeyTemplate[0].pValue = &ckfalse; /* session object */
    dsaPrivKeyTemplate[0].ulValueLen = sizeof(ckfalse);
    dsaPrivKeyTemplate[1].type = CKA_PRIVATE;
    dsaPrivKeyTemplate[1].pValue = &cktrue;
    dsaPrivKeyTemplate[1].ulValueLen = sizeof(cktrue);
    dsaPrivKeyTemplate[2].type = CKA_SENSITIVE;
    dsaPrivKeyTemplate[2].pValue = &cktrue;
    dsaPrivKeyTemplate[2].ulValueLen = sizeof(cktrue);
    dsaPrivKeyTemplate[3].type = CKA_SIGN,
    dsaPrivKeyTemplate[3].pValue = &cktrue;
    dsaPrivKeyTemplate[3].ulValueLen = sizeof(cktrue);
    dsaPrivKeyTemplate[4].type = CKA_EXTRACTABLE;
    dsaPrivKeyTemplate[4].pValue = &ckfalse;
    dsaPrivKeyTemplate[4].ulValueLen = sizeof(ckfalse);

    /* Generate a DSA key pair */
    logIt("Generate a DSA key pair ... \n");
    crv = pFunctionList->C_GenerateKeyPair(hRwSession, &dsaKeyPairGenMech,
                                           dsaPubKeyTemplate,
                                           PR_ARRAY_SIZE(dsaPubKeyTemplate),
                                           dsaPrivKeyTemplate,
                                           PR_ARRAY_SIZE(dsaPrivKeyTemplate),
                                           &hDSApubKey, &hDSAprivKey);
    if (crv != CKR_OK) {
        pk11error("DSA key pair generation failed", crv);
        return crv;
    }

    /* compute the digest */
    memset(digest, 0, sizeof(digest));
    crv = pFunctionList->C_DigestInit(hRwSession, &digestmech);
    if (crv != CKR_OK) {
        pk11error("C_DigestInit failed", crv);
        return crv;
    }

    /* Digest the file */
    while ((bytesRead = PR_Read(ifd, file_buf, sizeof(file_buf))) > 0) {
        crv = pFunctionList->C_DigestUpdate(hRwSession, (CK_BYTE_PTR)file_buf,
                                            bytesRead);
        if (crv != CKR_OK) {
            pk11error("C_DigestUpdate failed", crv);
            return crv;
        }
        count += bytesRead;
    }

    if (bytesRead < 0) {
        lperror("0 bytes read from input file");
        return CKR_INTERNAL_IN_FAILURE;
    }

    digestLen = sizeof(digest);
    crv = pFunctionList->C_DigestFinal(hRwSession, (CK_BYTE_PTR)digest,
                                       &digestLen);
    if (crv != CKR_OK) {
        pk11error("C_DigestFinal failed", crv);
        return crv;
    }

    if (digestLen != hash->hashLength) {
        PR_fprintf(PR_STDERR, "digestLen has incorrect length %lu "
                              "it should be %lu \n",
                   digestLen, sizeof(digest));
        return crv;
    }

    /* sign the hash */
    memset(sign, 0, sizeof(sign));
    /* SignUpdate  */
    crv = pFunctionList->C_SignInit(hRwSession, &signMech, hDSAprivKey);
    if (crv != CKR_OK) {
        pk11error("C_SignInit failed", crv);
        return crv;
    }

    signLen = sizeof(sign);
    crv = pFunctionList->C_Sign(hRwSession, (CK_BYTE *)digest, digestLen,
                                sign, &signLen);
    if (crv != CKR_OK) {
        pk11error("C_Sign failed", crv);
        return crv;
    }

    if (signLen != expectedSigLen) {
        PR_fprintf(PR_STDERR, "signLen has incorrect length %lu "
                              "it should be %lu \n",
                   signLen, expectedSigLen);
        return crv;
    }

    if (verify) {
        crv = pFunctionList->C_VerifyInit(hRwSession, &signMech, hDSApubKey);
        if (crv != CKR_OK) {
            pk11error("C_VerifyInit failed", crv);
            return crv;
        }
        crv = pFunctionList->C_Verify(hRwSession, digest, digestLen,
                                      sign, signLen);
        if (crv != CKR_OK) {
            pk11error("C_Verify failed", crv);
            return crv;
        }
    }

    if (verbose) {
        int j;
        PR_fprintf(PR_STDERR, "Library File Size: %d bytes\n", count);
        PR_fprintf(PR_STDERR, "  hash: %lu bytes\n", digestLen);
#define STEP 10
        for (i = 0; i < (int)digestLen; i += STEP) {
            PR_fprintf(PR_STDERR, "   ");
            for (j = 0; j < STEP && (i + j) < (int)digestLen; j++) {
                PR_fprintf(PR_STDERR, " %02x", digest[i + j]);
            }
            PR_fprintf(PR_STDERR, "\n");
        }
        PR_fprintf(PR_STDERR, "  signature: %lu bytes\n", signLen);
        for (i = 0; i < (int)signLen; i += STEP) {
            PR_fprintf(PR_STDERR, "   ");
            for (j = 0; j < STEP && (i + j) < (int)signLen; j++) {
                PR_fprintf(PR_STDERR, " %02x", sign[i + j]);
            }
            PR_fprintf(PR_STDERR, "\n");
        }
    }

    /*
     * we write the key out in a straight binary format because very
     * low level libraries need to read an parse this file. Ideally we should
     * just derEncode the public key (which would be pretty simple, and be
     * more general), but then we'd need to link the ASN.1 decoder with the
     * freebl libraries.
     */

    header.magic1 = NSS_SIGN_CHK_MAGIC1;
    header.magic2 = NSS_SIGN_CHK_MAGIC2;
    header.majorVersion = compat ? COMPAT_MAJOR : NSS_SIGN_CHK_MAJOR_VERSION;
    header.minorVersion = compat ? COMPAT_MINOR : NSS_SIGN_CHK_MINOR_VERSION;
    encodeInt(header.offset, sizeof(header)); /* offset to data start */
    encodeInt(header.type, CKK_DSA);
    bytesWritten = PR_Write(ofd, &header, sizeof(header));
    if (bytesWritten != sizeof(header)) {
        return CKR_INTERNAL_OUT_FAILURE;
    }

    /* get DSA Public KeyValue */
    memset(dsaPubKey, 0, sizeof(dsaPubKey));
    dsaPubKeyValue.type = CKA_VALUE;
    dsaPubKeyValue.pValue = (CK_VOID_PTR)&dsaPubKey;
    dsaPubKeyValue.ulValueLen = sizeof(dsaPubKey);

    crv = pFunctionList->C_GetAttributeValue(hRwSession, hDSApubKey,
                                             &dsaPubKeyValue, 1);
    if (crv != CKR_OK && crv != CKR_ATTRIBUTE_TYPE_INVALID) {
        pk11error("C_GetAttributeValue failed", crv);
        return crv;
    }

    /* CKA_PRIME */
    rv = writeItem(ofd, dsaPubKeyTemplate[0].pValue,
                   dsaPubKeyTemplate[0].ulValueLen);
    if (rv != PR_SUCCESS) {
        return CKR_INTERNAL_OUT_FAILURE;
    }
    /* CKA_SUBPRIME */
    rv = writeItem(ofd, dsaPubKeyTemplate[1].pValue,
                   dsaPubKeyTemplate[1].ulValueLen);
    if (rv != PR_SUCCESS) {
        return CKR_INTERNAL_OUT_FAILURE;
    }
    /* CKA_BASE */
    rv = writeItem(ofd, dsaPubKeyTemplate[2].pValue,
                   dsaPubKeyTemplate[2].ulValueLen);
    if (rv != PR_SUCCESS) {
        return CKR_INTERNAL_OUT_FAILURE;
    }
    /* DSA Public Key value */
    rv = writeItem(ofd, dsaPubKeyValue.pValue,
                   dsaPubKeyValue.ulValueLen);
    if (rv != PR_SUCCESS) {
        return CKR_INTERNAL_OUT_FAILURE;
    }
    /* DSA SIGNATURE */
    rv = writeItem(ofd, &sign, signLen);
    if (rv != PR_SUCCESS) {
        return CKR_INTERNAL_OUT_FAILURE;
    }

    return CKR_OK;
}

CK_RV
shlibSignHMAC(CK_FUNCTION_LIST_PTR pFunctionList, CK_SLOT_ID slot,
              CK_SESSION_HANDLE hRwSession, int keySize, PRFileDesc *ifd,
              PRFileDesc *ofd, const HashTable *hash)
{
    CK_MECHANISM hmacMech = { 0, NULL, 0 };
    CK_MECHANISM hmacKeyGenMech = { 0, NULL, 0 };
    CK_BYTE keyBuf[HASH_LENGTH_MAX];
    CK_ULONG keyLen = 0;
    CK_BYTE sign[HASH_LENGTH_MAX];
    CK_ULONG signLen = 0;
    int bytesRead;
    int bytesWritten;
    unsigned char file_buf[512];
    NSSSignChkHeader header;
    int count = 0;
    CK_RV crv = CKR_GENERAL_ERROR;
    PRStatus rv = PR_SUCCESS;
    int i;

    /*** HMAC Key ***/
    CK_ATTRIBUTE hmacKeyTemplate[7];
    CK_ATTRIBUTE hmacKeyValue;
    CK_OBJECT_HANDLE hHMACKey = CK_INVALID_HANDLE;

    if (hash == NULL) {
        hash = findHash("sha256");
    }
    if (hash == NULL) {
        PR_fprintf(PR_STDERR,
                   "Internal error:Could find sha256 entry in table.\n");
    }

    hmacKeyTemplate[0].type = CKA_TOKEN;
    hmacKeyTemplate[0].pValue = &ckfalse; /* session object */
    hmacKeyTemplate[0].ulValueLen = sizeof(ckfalse);
    hmacKeyTemplate[1].type = CKA_PRIVATE;
    hmacKeyTemplate[1].pValue = &cktrue;
    hmacKeyTemplate[1].ulValueLen = sizeof(cktrue);
    hmacKeyTemplate[2].type = CKA_SENSITIVE;
    hmacKeyTemplate[2].pValue = &ckfalse;
    hmacKeyTemplate[2].ulValueLen = sizeof(cktrue);
    hmacKeyTemplate[3].type = CKA_SIGN;
    hmacKeyTemplate[3].pValue = &cktrue;
    hmacKeyTemplate[3].ulValueLen = sizeof(cktrue);
    hmacKeyTemplate[4].type = CKA_EXTRACTABLE;
    hmacKeyTemplate[4].pValue = &ckfalse;
    hmacKeyTemplate[4].ulValueLen = sizeof(ckfalse);
    hmacKeyTemplate[5].type = CKA_VALUE_LEN;
    hmacKeyTemplate[5].pValue = (void *)&hash->hashLength;
    hmacKeyTemplate[5].ulValueLen = sizeof(hash->hashLength);
    hmacKeyTemplate[6].type = CKA_KEY_TYPE;
    hmacKeyTemplate[6].pValue = (void *)&hash->keyType;
    hmacKeyTemplate[6].ulValueLen = sizeof(hash->keyType);
    hmacKeyGenMech.mechanism = CKM_GENERIC_SECRET_KEY_GEN;
    hmacMech.mechanism = hash->hmac;

    /* Generate a DSA key pair */
    logIt("Generate an HMAC key ... \n");
    crv = pFunctionList->C_GenerateKey(hRwSession, &hmacKeyGenMech,
                                       hmacKeyTemplate,
                                       PR_ARRAY_SIZE(hmacKeyTemplate),
                                       &hHMACKey);
    if (crv != CKR_OK) {
        pk11error("HMAC key generation failed", crv);
        return crv;
    }

    /* compute the digest */
    memset(sign, 0, sizeof(sign));
    crv = pFunctionList->C_SignInit(hRwSession, &hmacMech, hHMACKey);
    if (crv != CKR_OK) {
        pk11error("C_SignInit failed", crv);
        return crv;
    }

    /* Digest the file */
    while ((bytesRead = PR_Read(ifd, file_buf, sizeof(file_buf))) > 0) {
        crv = pFunctionList->C_SignUpdate(hRwSession, (CK_BYTE_PTR)file_buf,
                                          bytesRead);
        if (crv != CKR_OK) {
            pk11error("C_SignUpdate failed", crv);
            return crv;
        }
        count += bytesRead;
    }

    if (bytesRead < 0) {
        lperror("0 bytes read from input file");
        return CKR_INTERNAL_IN_FAILURE;
    }

    signLen = sizeof(sign);
    crv = pFunctionList->C_SignFinal(hRwSession, (CK_BYTE_PTR)sign,
                                     &signLen);
    if (crv != CKR_OK) {
        pk11error("C_SignFinal failed", crv);
        return crv;
    }

    if (signLen != hash->hashLength) {
        PR_fprintf(PR_STDERR, "digestLen has incorrect length %lu "
                              "it should be %lu \n",
                   signLen, hash->hashLength);
        return crv;
    }
    /* get HMAC KeyValue */
    memset(keyBuf, 0, sizeof(keyBuf));
    hmacKeyValue.type = CKA_VALUE;
    hmacKeyValue.pValue = (CK_VOID_PTR)&keyBuf;
    hmacKeyValue.ulValueLen = sizeof(keyBuf);

    crv = pFunctionList->C_GetAttributeValue(hRwSession, hHMACKey,
                                             &hmacKeyValue, 1);
    if (crv != CKR_OK && crv != CKR_ATTRIBUTE_TYPE_INVALID) {
        pk11error("C_GetAttributeValue failed", crv);
        return crv;
    }
    keyLen = hmacKeyValue.ulValueLen;

    if (verbose) {
        int j;
        PR_fprintf(PR_STDERR, "Library File Size: %d bytes\n", count);
        PR_fprintf(PR_STDERR, "  key: %lu bytes\n", keyLen);
#define STEP 10
        for (i = 0; i < (int)keyLen; i += STEP) {
            PR_fprintf(PR_STDERR, "   ");
            for (j = 0; j < STEP && (i + j) < (int)keyLen; j++) {
                PR_fprintf(PR_STDERR, " %02x", keyBuf[i + j]);
            }
            PR_fprintf(PR_STDERR, "\n");
        }
        PR_fprintf(PR_STDERR, "  signature: %lu bytes\n", signLen);
        for (i = 0; i < (int)signLen; i += STEP) {
            PR_fprintf(PR_STDERR, "   ");
            for (j = 0; j < STEP && (i + j) < (int)signLen; j++) {
                PR_fprintf(PR_STDERR, " %02x", sign[i + j]);
            }
            PR_fprintf(PR_STDERR, "\n");
        }
    }

    /*
     * we write the key out in a straight binary format because very
     * low level libraries need to read an parse this file. Ideally we should
     * just derEncode the public key (which would be pretty simple, and be
     * more general), but then we'd need to link the ASN.1 decoder with the
     * freebl libraries.
     */

    header.magic1 = NSS_SIGN_CHK_MAGIC1;
    header.magic2 = NSS_SIGN_CHK_MAGIC2;
    header.majorVersion = NSS_SIGN_CHK_MAJOR_VERSION;
    header.minorVersion = NSS_SIGN_CHK_MINOR_VERSION;
    encodeInt(header.offset, sizeof(header)); /* offset to data start */
    encodeInt(header.type, NSS_SIGN_CHK_FLAG_HMAC | hash->hashType);
    bytesWritten = PR_Write(ofd, &header, sizeof(header));
    if (bytesWritten != sizeof(header)) {
        return CKR_INTERNAL_OUT_FAILURE;
    }

    /* HMACKey */
    rv = writeItem(ofd, keyBuf, keyLen);
    if (rv != PR_SUCCESS) {
        return CKR_INTERNAL_OUT_FAILURE;
    }
    /* HMAC SIGNATURE */
    rv = writeItem(ofd, &sign, signLen);
    if (rv != PR_SUCCESS) {
        return CKR_INTERNAL_OUT_FAILURE;
    }

    return CKR_OK;
}

int
main(int argc, char **argv)
{
    PLOptState *optstate;
    char *program_name;
    char *libname = NULL;
    PRLibrary *lib = NULL;
    PRFileDesc *ifd = NULL;
    PRFileDesc *ofd = NULL;
    const char *input_file = NULL; /* read/create encrypted data from here */
    char *output_file = NULL;      /* write new encrypted data here */
    unsigned int keySize = 0;
    static PRBool FIPSMODE = PR_FALSE;
    static PRBool useDSA = PR_FALSE;
    PRBool successful = PR_FALSE;
    const HashTable *hash = NULL;

#ifdef USES_LINKS
    int ret;
    struct stat stat_buf;
    char link_buf[MAXPATHLEN + 1];
    char *link_file = NULL;
#endif

    char *pwd = NULL;
    char *configDir = NULL;
    char *dbPrefix = NULL;
    char *disableUnload = NULL;

    CK_C_GetFunctionList pC_GetFunctionList;
    CK_TOKEN_INFO tokenInfo;
    CK_FUNCTION_LIST_PTR pFunctionList = NULL;
    CK_RV crv = CKR_OK;
    CK_SESSION_HANDLE hRwSession;
    CK_SLOT_ID *pSlotList = NULL;
    CK_ULONG slotIndex = 0;

    program_name = strrchr(argv[0], '/');
    program_name = program_name ? (program_name + 1) : argv[0];
    optstate = PL_CreateOptState(argc, argv, "i:o:f:Fd:hH?k:p:P:vVs:t:Dc");
    if (optstate == NULL) {
        lperror("PL_CreateOptState failed");
        return 1;
    }

    while (PL_GetNextOpt(optstate) == PL_OPT_OK) {
        switch (optstate->option) {

            case 'd':
                if (!optstate->value) {
                    PL_DestroyOptState(optstate);
                    usage(program_name);
                }
                configDir = PL_strdup(optstate->value);
                checkPath(configDir);
                break;

            case 'D':
                useDSA = PR_TRUE;
                break;

            case 'c':
                compat = PR_TRUE;
                break;

            case 'i':
                if (!optstate->value) {
                    PL_DestroyOptState(optstate);
                    usage(program_name);
                }
                input_file = optstate->value;
                break;

            case 'o':
                if (!optstate->value) {
                    PL_DestroyOptState(optstate);
                    usage(program_name);
                }
                output_file = PL_strdup(optstate->value);
                break;

            case 'k':
                if (!optstate->value) {
                    PL_DestroyOptState(optstate);
                    usage(program_name);
                }
                keySize = atoi(optstate->value);
                break;

            case 'f':
                if (!optstate->value) {
                    PL_DestroyOptState(optstate);
                    usage(program_name);
                }
                pwd = filePasswd((char *)optstate->value);
                if (!pwd)
                    usage(program_name);
                break;

            case 'F':
                FIPSMODE = PR_TRUE;
                break;

            case 'p':
                if (!optstate->value) {
                    PL_DestroyOptState(optstate);
                    usage(program_name);
                }
                pwd = PL_strdup(optstate->value);
                break;

            case 'P':
                if (!optstate->value) {
                    PL_DestroyOptState(optstate);
                    usage(program_name);
                }
                dbPrefix = PL_strdup(optstate->value);
                break;

            case 't':
                if (!optstate->value) {
                    PL_DestroyOptState(optstate);
                    usage(program_name);
                }
                hash = findHash(optstate->value);
                if (hash == NULL) {
                    PR_fprintf(PR_STDERR, "Invalid hash '%s'\n",
                               optstate->value);
                    usage(program_name);
                }
                break;
            case 'v':
                verbose = PR_TRUE;
                break;

            case 'V':
                verify = PR_TRUE;
                break;

            case 'H':
                PL_DestroyOptState(optstate);
                long_usage(program_name);
                return 1;
                break;

            case 'h':
            case '?':
            default:
                PL_DestroyOptState(optstate);
                usage(program_name);
                return 1;
                break;
        }
    }
    PL_DestroyOptState(optstate);

    if (!input_file) {
        usage(program_name);
        return 1;
    }

    /* Get the platform-dependent library name of the
     * NSS cryptographic module.
     */
    libname = PR_GetLibraryName(NULL, "softokn3");
    assert(libname != NULL);
    if (!libname) {
        PR_fprintf(PR_STDERR, "getting softokn3 failed");
        goto cleanup;
    }
    lib = PR_LoadLibrary(libname);
    assert(lib != NULL);
    if (!lib) {
        PR_fprintf(PR_STDERR, "loading softokn3 failed");
        goto cleanup;
    }
    PR_FreeLibraryName(libname);

    if (FIPSMODE) {
        /* FIPSMODE == FC_GetFunctionList */
        /* library path must be set to an already signed softokn3/freebl */
        pC_GetFunctionList = (CK_C_GetFunctionList)
            PR_FindFunctionSymbol(lib, "FC_GetFunctionList");
    } else {
        /* NON FIPS mode  == C_GetFunctionList */
        pC_GetFunctionList = (CK_C_GetFunctionList)
            PR_FindFunctionSymbol(lib, "C_GetFunctionList");
    }
    assert(pC_GetFunctionList != NULL);
    if (!pC_GetFunctionList) {
        PR_fprintf(PR_STDERR, "getting function list failed");
        goto cleanup;
    }

    crv = (*pC_GetFunctionList)(&pFunctionList);
    assert(crv == CKR_OK);
    if (crv != CKR_OK) {
        PR_fprintf(PR_STDERR, "loading function list failed");
        goto cleanup;
    }

    if (configDir) {
        if (!dbPrefix) {
            dbPrefix = PL_strdup("");
        }
        crv = softokn_Init(pFunctionList, configDir, dbPrefix);
        if (crv != CKR_OK) {
            logIt("Failed to use provided database directory "
                  "will just initialize the volatile certdb.\n");
            crv = softokn_Init(pFunctionList, NULL, NULL); /* NoDB Init */
        }
    } else {
        crv = softokn_Init(pFunctionList, NULL, NULL); /* NoDB Init */
    }

    if (crv != CKR_OK) {
        pk11error("Initiailzing softoken failed", crv);
        goto cleanup;
    }

    pSlotList = getSlotList(pFunctionList, slotIndex);
    if (pSlotList == NULL) {
        PR_fprintf(PR_STDERR, "getSlotList failed");
        goto cleanup;
    }

    crv = pFunctionList->C_OpenSession(pSlotList[slotIndex],
                                       CKF_RW_SESSION | CKF_SERIAL_SESSION,
                                       NULL, NULL, &hRwSession);
    if (crv != CKR_OK) {
        pk11error("Opening a read/write session failed", crv);
        goto cleanup;
    }

    /* check if a password is needed */
    crv = pFunctionList->C_GetTokenInfo(pSlotList[slotIndex], &tokenInfo);
    if (crv != CKR_OK) {
        pk11error("C_GetTokenInfo failed", crv);
        goto cleanup;
    }
    if (tokenInfo.flags & CKF_LOGIN_REQUIRED) {
        if (pwd) {
            int pwdLen = strlen((const char *)pwd);
            crv = pFunctionList->C_Login(hRwSession, CKU_USER,
                                         (CK_UTF8CHAR_PTR)pwd, (CK_ULONG)pwdLen);
            if (crv != CKR_OK) {
                pk11error("C_Login failed", crv);
                goto cleanup;
            }
        } else {
            PR_fprintf(PR_STDERR, "Please provide the password for the token");
            goto cleanup;
        }
    } else if (pwd) {
        logIt("A password was provided but the password was not used.\n");
    }

    /* open the shared library */
    ifd = PR_OpenFile(input_file, PR_RDONLY, 0);
    if (ifd == NULL) {
        lperror(input_file);
        goto cleanup;
    }
#ifdef USES_LINKS
    ret = lstat(input_file, &stat_buf);
    if (ret < 0) {
        perror(input_file);
        goto cleanup;
    }
    if (S_ISLNK(stat_buf.st_mode)) {
        char *dirpath, *dirend;
        ret = readlink(input_file, link_buf, sizeof(link_buf) - 1);
        if (ret < 0) {
            perror(input_file);
            goto cleanup;
        }
        link_buf[ret] = 0;
        link_file = mkoutput(input_file);
        /* get the dirname of input_file */
        dirpath = PL_strdup(input_file);
        dirend = strrchr(dirpath, '/');
        if (dirend) {
            *dirend = '\0';
            ret = chdir(dirpath);
            if (ret < 0) {
                perror(dirpath);
                goto cleanup;
            }
        }
        PL_strfree(dirpath);
        input_file = link_buf;
        /* get the basename of link_file */
        dirend = strrchr(link_file, '/');
        if (dirend) {
            char *tmp_file = NULL;
            tmp_file = PL_strdup(dirend + 1);
            PL_strfree(link_file);
            link_file = tmp_file;
        }
    }
#endif
    if (output_file == NULL) {
        output_file = mkoutput(input_file);
    }

    if (verbose) {
        PR_fprintf(PR_STDERR, "Library File: %s\n", input_file);
        PR_fprintf(PR_STDERR, "Check File: %s\n", output_file);
#ifdef USES_LINKS
        if (link_file) {
            PR_fprintf(PR_STDERR, "Link: %s\n", link_file);
        }
#endif
    }

    /* open the target signature file */
    ofd = PR_Open(output_file, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0666);
    if (ofd == NULL) {
        lperror(output_file);
        goto cleanup;
    }

    if (useDSA) {
        crv = shlibSignDSA(pFunctionList, pSlotList[slotIndex], hRwSession,
                           keySize, ifd, ofd, hash);
    } else {
        crv = shlibSignHMAC(pFunctionList, pSlotList[slotIndex], hRwSession,
                            keySize, ifd, ofd, hash);
    }
    if (crv == CKR_INTERNAL_OUT_FAILURE) {
        lperror(output_file);
    }
    if (crv == CKR_INTERNAL_IN_FAILURE) {
        lperror(input_file);
    }

    PR_Close(ofd);
    ofd = NULL;
    /* close the input_File */
    PR_Close(ifd);
    ifd = NULL;

#ifdef USES_LINKS
    if (link_file) {
        (void)unlink(link_file);
        ret = symlink(output_file, link_file);
        if (ret < 0) {
            perror(link_file);
            goto cleanup;
        }
    }
#endif
    successful = PR_TRUE;

cleanup:
    if (pFunctionList) {
        /* C_Finalize will automatically logout, close session, */
        /* and delete the temp objects on the token */
        crv = pFunctionList->C_Finalize(NULL);
        if (crv != CKR_OK) {
            pk11error("C_Finalize failed", crv);
        }
    }
    if (pSlotList) {
        PR_Free(pSlotList);
    }
    if (pwd) {
        PL_strfree(pwd);
    }
    if (configDir) {
        PL_strfree(configDir);
    }
    if (dbPrefix) {
        PL_strfree(dbPrefix);
    }
    if (output_file) { /* allocated by mkoutput function */
        PL_strfree(output_file);
    }
#ifdef USES_LINKS
    if (link_file) { /* allocated by mkoutput function */
        PL_strfree(link_file);
    }
#endif
    if (ifd) {
        PR_Close(ifd);
    }
    if (ofd) {
        PR_Close(ofd);
    }

    disableUnload = PR_GetEnvSecure("NSS_DISABLE_UNLOAD");
    if (!disableUnload && lib) {
        PR_UnloadLibrary(lib);
    }
    PR_Cleanup();

    if (crv != CKR_OK)
        return crv;

    return (successful) ? 0 : 1;
}
