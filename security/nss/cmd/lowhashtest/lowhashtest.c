#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "nspr.h"

/* nss headers */
#include "prtypes.h"
#include "plgetopt.h"
#include "hasht.h"
#include "nsslowhash.h"
#include "secport.h"
#include "hasht.h"
#include "basicutil.h"

static char *progName = NULL;

static int
test_long_message(NSSLOWInitContext *initCtx,
                  HASH_HashType algoType, unsigned int hashLen,
                  const PRUint8 expected[], PRUint8 results[])
{
    unsigned int len, i, rv = 0;
    NSSLOWHASHContext *ctx;

    /* The message is meant to be 'a' repeated 1,000,000 times.
     * This is too much to allocate on the stack so we will use a 1,000 char
     * buffer and call update 1,000 times.
     */
    unsigned char buf[1000];
    (void)PORT_Memset(buf, 'a', sizeof(buf));

    ctx = NSSLOWHASH_NewContext(initCtx, algoType);
    if (ctx == NULL) {
        SECU_PrintError(progName, "Couldn't get hash context\n");
        return 1;
    }

    NSSLOWHASH_Begin(ctx);
    for (i = 0; i < 1000; ++i) {
        NSSLOWHASH_Update(ctx, buf, 1000);
    }

    NSSLOWHASH_End(ctx, results, &len, hashLen);
    PR_ASSERT(len == hashLen);
    PR_ASSERT(PORT_Memcmp(expected, results, hashLen) == 0);
    if (PORT_Memcmp(expected, results, len) != 0) {
        SECU_PrintError(progName, "Hash mismatch\n");
        SECU_PrintBuf(stdout, "Expected: ", expected, hashLen);
        SECU_PrintBuf(stdout, "Actual:   ", results, len);
        rv = 1;
    }

    NSSLOWHASH_Destroy(ctx);
    NSSLOW_Shutdown(initCtx);

    return rv;
}

static int
test_long_message_sha1(NSSLOWInitContext *initCtx)
{
    PRUint8 results[SHA1_LENGTH];
    /* Test vector from FIPS 180-2: appendix B.3.  */

    /* 34aa973c d4c4daa4 f61eeb2b dbad2731 6534016f. */
    static const PRUint8 expected[SHA256_LENGTH] =
        { 0x34, 0xaa, 0x97, 0x3c, 0xd4, 0xc4, 0xda, 0xa4, 0xf6, 0x1e, 0xeb, 0x2b,
          0xdb, 0xad, 0x27, 0x31, 0x65, 0x34, 0x01, 0x6f };
    unsigned char buf[1000];
    (void)PORT_Memset(buf, 'a', sizeof(buf));
    return test_long_message(initCtx, HASH_AlgSHA1,
                             SHA1_LENGTH, &expected[0], results);
}

static int
test_long_message_sha256(NSSLOWInitContext *initCtx)
{
    PRUint8 results[SHA256_LENGTH];
    /* cdc76e5c 9914fb92 81a1c7e2 84d73e67 f1809a48 a497200e 046d39cc c7112cd0. */
    static const PRUint8 expected[SHA256_LENGTH] =
        { 0xcd, 0xc7, 0x6e, 0x5c, 0x99, 0x14, 0xfb, 0x92, 0x81, 0xa1, 0xc7, 0xe2, 0x84, 0xd7, 0x3e, 0x67,
          0xf1, 0x80, 0x9a, 0x48, 0xa4, 0x97, 0x20, 0x0e, 0x04, 0x6d, 0x39, 0xcc, 0xc7, 0x11, 0x2c, 0xd0 };
    unsigned char buf[1000];
    (void)PORT_Memset(buf, 'a', sizeof(buf));
    return test_long_message(initCtx, HASH_AlgSHA256,
                             SHA256_LENGTH, &expected[0], results);
}

static int
test_long_message_sha384(NSSLOWInitContext *initCtx)
{
    PRUint8 results[SHA384_LENGTH];
    /* Test vector from FIPS 180-2: appendix B.3.  */
    /*
    9d0e1809716474cb
    086e834e310a4a1c
    ed149e9c00f24852
    7972cec5704c2a5b
    07b8b3dc38ecc4eb
    ae97ddd87f3d8985.
    */
    static const PRUint8 expected[SHA384_LENGTH] =
        { 0x9d, 0x0e, 0x18, 0x09, 0x71, 0x64, 0x74, 0xcb,
          0x08, 0x6e, 0x83, 0x4e, 0x31, 0x0a, 0x4a, 0x1c,
          0xed, 0x14, 0x9e, 0x9c, 0x00, 0xf2, 0x48, 0x52,
          0x79, 0x72, 0xce, 0xc5, 0x70, 0x4c, 0x2a, 0x5b,
          0x07, 0xb8, 0xb3, 0xdc, 0x38, 0xec, 0xc4, 0xeb,
          0xae, 0x97, 0xdd, 0xd8, 0x7f, 0x3d, 0x89, 0x85 };
    unsigned char buf[1000];
    (void)PORT_Memset(buf, 'a', sizeof(buf));

    return test_long_message(initCtx, HASH_AlgSHA384,
                             SHA384_LENGTH, &expected[0], results);
}

static int
test_long_message_sha512(NSSLOWInitContext *initCtx)
{
    PRUint8 results[SHA512_LENGTH];
    /* Test vector from FIPS 180-2: appendix B.3.  */
    static const PRUint8 expected[SHA512_LENGTH] =
        { 0xe7, 0x18, 0x48, 0x3d, 0x0c, 0xe7, 0x69, 0x64, 0x4e, 0x2e, 0x42, 0xc7, 0xbc, 0x15, 0xb4, 0x63,
          0x8e, 0x1f, 0x98, 0xb1, 0x3b, 0x20, 0x44, 0x28, 0x56, 0x32, 0xa8, 0x03, 0xaf, 0xa9, 0x73, 0xeb,
          0xde, 0x0f, 0xf2, 0x44, 0x87, 0x7e, 0xa6, 0x0a, 0x4c, 0xb0, 0x43, 0x2c, 0xe5, 0x77, 0xc3, 0x1b,
          0xeb, 0x00, 0x9c, 0x5c, 0x2c, 0x49, 0xaa, 0x2e, 0x4e, 0xad, 0xb2, 0x17, 0xad, 0x8c, 0xc0, 0x9b };
    unsigned char buf[1000];
    (void)PORT_Memset(buf, 'a', sizeof(buf));

    return test_long_message(initCtx, HASH_AlgSHA512,
                             SHA512_LENGTH, &expected[0], results);
}

static int
testMessageDigest(NSSLOWInitContext *initCtx,
                  HASH_HashType algoType, unsigned int hashLen,
                  const unsigned char *message,
                  const PRUint8 expected[], PRUint8 results[])
{
    NSSLOWHASHContext *ctx;
    unsigned int len;
    int rv = 0;

    ctx = NSSLOWHASH_NewContext(initCtx, algoType);
    if (ctx == NULL) {
        SECU_PrintError(progName, "Couldn't get hash context\n");
        return 1;
    }

    NSSLOWHASH_Begin(ctx);
    NSSLOWHASH_Update(ctx, message, PORT_Strlen((const char *)message));
    NSSLOWHASH_End(ctx, results, &len, hashLen);
    PR_ASSERT(len == hashLen);
    PR_ASSERT(PORT_Memcmp(expected, results, len) == 0);

    if (PORT_Memcmp(expected, results, len) != 0) {
        SECU_PrintError(progName, "Hash mismatch\n");
        SECU_PrintBuf(stdout, "Expected: ", expected, hashLen);
        SECU_PrintBuf(stdout, "Actual:   ", results, len);
        rv = 1;
    }

    NSSLOWHASH_Destroy(ctx);
    NSSLOW_Shutdown(initCtx);

    return rv;
}

static int
testMD5(NSSLOWInitContext *initCtx)
{
    /* test vectors that glibc, our API main client, uses */

    static const struct {
        const unsigned char *input;
        const PRUint8 result[MD5_LENGTH];
    } md5tests[] = {
        { (unsigned char *)"",
          { 0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04, 0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e } },
        { (unsigned char *)"a",
          { 0x0c, 0xc1, 0x75, 0xb9, 0xc0, 0xf1, 0xb6, 0xa8, 0x31, 0xc3, 0x99, 0xe2, 0x69, 0x77, 0x26, 0x61 } },
        { (unsigned char *)"abc",
          { 0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0, 0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72 } },
        { (unsigned char *)"message digest",
          { 0xf9, 0x6b, 0x69, 0x7d, 0x7c, 0xb7, 0x93, 0x8d, 0x52, 0x5a, 0x2f, 0x31, 0xaa, 0xf1, 0x61, 0xd0 } },
        { (unsigned char *)"abcdefghijklmnopqrstuvwxyz",
          { 0xc3, 0xfc, 0xd3, 0xd7, 0x61, 0x92, 0xe4, 0x00, 0x7d, 0xfb, 0x49, 0x6c, 0xca, 0x67, 0xe1, 0x3b } },
        { (unsigned char *)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
          { 0xd1, 0x74, 0xab, 0x98, 0xd2, 0x77, 0xd9, 0xf5, 0xa5, 0x61, 0x1c, 0x2c, 0x9f, 0x41, 0x9d, 0x9f } },
        { (unsigned char *)"123456789012345678901234567890123456789012345678901234567890"
                           "12345678901234567890",
          { 0x57, 0xed, 0xf4, 0xa2, 0x2b, 0xe3, 0xc9, 0x55, 0xac, 0x49, 0xda, 0x2e, 0x21, 0x07, 0xb6, 0x7a } }
    };
    PRUint8 results[MD5_LENGTH];
    int rv = 0, cnt, numTests;

    numTests = sizeof(md5tests) / sizeof(md5tests[0]);
    for (cnt = 0; cnt < numTests; cnt++) {
        rv += testMessageDigest(initCtx, HASH_AlgMD5, MD5_LENGTH,
                                (const unsigned char *)md5tests[cnt].input,
                                md5tests[cnt].result, &results[0]);
    }
    return rv;
}

/*
 * Tests with test vectors from FIPS 180-2 Appendixes B.1, B.2, B.3, C, and D
 *
 */

static int
testSHA1(NSSLOWInitContext *initCtx)
{
    static const struct {
        const unsigned char *input;
        const PRUint8 result[SHA1_LENGTH];
    } sha1tests[] = {
        /* one block messsage */
        {
            (const unsigned char *)"abc",
            /* a9993e36 4706816a ba3e2571 7850c26c 9cd0d89d. */

            { 0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a,  /* a9993e36 4706816a */
              0xba, 0x3e, 0x25, 0x71,                          /* ba3e2571 */
              0x78, 0x50, 0xc2, 0x6c, 0x9c, 0xd0, 0xd8, 0x9d } /* 7850c26c 9cd0d89d */
        },
        { (const unsigned char *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
          /* 84983e44 1c3bd26e baae4aa1 f95129e5 e54670f1. */
          { 0x84, 0x98, 0x3e, 0x44, 0x1c, 0x3b, 0xd2, 0x6e, 0xba, 0xae, 0x4a, 0xa1,
            0xf9, 0x51, 0x29, 0xe5, 0xe5, 0x46, 0x70, 0xf1 } }
    };

    PRUint8 results[SHA1_LENGTH];
    int rv = 0, cnt, numTests;

    numTests = sizeof(sha1tests) / sizeof(sha1tests[0]);
    for (cnt = 0; cnt < numTests; cnt++) {
        rv += testMessageDigest(initCtx, HASH_AlgSHA1, SHA1_LENGTH,
                                (const unsigned char *)sha1tests[cnt].input,
                                sha1tests[cnt].result, &results[0]);
    }

    rv += test_long_message_sha1(initCtx);
    return rv;
}

static int
testSHA224(NSSLOWInitContext *initCtx)
{
    static const struct {
        const unsigned char *input;
        const PRUint8 result[SHA224_LENGTH];
    } sha224tests[] = {
        /* one block messsage */
        { (const unsigned char *)"abc",
          { 0x23, 0x09, 0x7D, 0x22, 0x34, 0x05, 0xD8, 0x22, 0x86, 0x42, 0xA4, 0x77, 0xBD, 0xA2, 0x55, 0xB3,
            0x2A, 0xAD, 0xBC, 0xE4, 0xBD, 0xA0, 0xB3, 0xF7, 0xE3, 0x6C, 0x9D, 0xA7 } },
        /* two block message */
        { (const unsigned char *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
          { 0x75, 0x38, 0x8B, 0x16, 0x51, 0x27, 0x76, 0xCC, 0x5D, 0xBA, 0x5D, 0xA1, 0xFD, 0x89, 0x01, 0x50,
            0xB0, 0xC6, 0x45, 0x5C, 0xB4, 0xF5, 0x8B, 0x19, 0x52, 0x52, 0x25, 0x25 } }
    };

    PRUint8 results[SHA224_LENGTH];
    int rv = 0, cnt, numTests;

    numTests = sizeof(sha224tests) / sizeof(sha224tests[0]);
    for (cnt = 0; cnt < numTests; cnt++) {
        rv += testMessageDigest(initCtx, HASH_AlgSHA224, SHA224_LENGTH,
                                (const unsigned char *)sha224tests[cnt].input,
                                sha224tests[cnt].result, &results[0]);
    }

    return rv;
}

static int
testSHA256(NSSLOWInitContext *initCtx)
{
    static const struct {
        const unsigned char *input;
        const PRUint8 result[SHA256_LENGTH];
    } sha256tests[] = {
        /* Test vectors from FIPS 180-2: appendix B.1.  */
        { (unsigned char *)"abc",
          { 0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
            0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad } },
        /* Test vectors from FIPS 180-2: appendix B.2.  */
        { (unsigned char *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
          { 0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8, 0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
            0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67, 0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1 } }
    };

    PRUint8 results[SHA256_LENGTH];
    int rv = 0, cnt, numTests;

    numTests = sizeof(sha256tests) / sizeof(sha256tests[0]);
    for (cnt = 0; cnt < numTests; cnt++) {
        rv += testMessageDigest(initCtx, HASH_AlgSHA256, SHA256_LENGTH,
                                (const unsigned char *)sha256tests[cnt].input,
                                sha256tests[cnt].result, &results[0]);
    }

    rv += test_long_message_sha256(initCtx);
    return rv;
}

static int
testSHA384(NSSLOWInitContext *initCtx)
{
    static const struct {
        const unsigned char *input;
        const PRUint8 result[SHA384_LENGTH];
    } sha384tests[] = {
        /* Test vector from FIPS 180-2: appendix D, single-block message.  */
        { (unsigned char *)"abc",
          { 0xcb, 0x00, 0x75, 0x3f, 0x45, 0xa3, 0x5e, 0x8b,
            0xb5, 0xa0, 0x3d, 0x69, 0x9a, 0xc6, 0x50, 0x07,
            0x27, 0x2c, 0x32, 0xab, 0x0e, 0xde, 0xd1, 0x63,
            0x1a, 0x8b, 0x60, 0x5a, 0x43, 0xff, 0x5b, 0xed,
            0x80, 0x86, 0x07, 0x2b, 0xa1, 0xe7, 0xcc, 0x23,
            0x58, 0xba, 0xec, 0xa1, 0x34, 0xc8, 0x25, 0xa7 } },

        /* Test vectors from FIPS 180-2: appendix D, multi-block message.  */
        { (unsigned char *)"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                           "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
          /*
        09330c33f71147e8
        3d192fc782cd1b47
        53111b173b3b05d2
        2fa08086e3b0f712
        fcc7c71a557e2db9
        66c3e9fa91746039.
        */
          { 0x09, 0x33, 0x0c, 0x33, 0xf7, 0x11, 0x47, 0xe8,
            0x3d, 0x19, 0x2f, 0xc7, 0x82, 0xcd, 0x1b, 0x47,
            0x53, 0x11, 0x1b, 0x17, 0x3b, 0x3b, 0x05, 0xd2,
            0x2f, 0xa0, 0x80, 0x86, 0xe3, 0xb0, 0xf7, 0x12,
            0xfc, 0xc7, 0xc7, 0x1a, 0x55, 0x7e, 0x2d, 0xb9,
            0x66, 0xc3, 0xe9, 0xfa, 0x91, 0x74, 0x60, 0x39 } }
    };

    PRUint8 results[SHA384_LENGTH];
    int rv = 0, cnt, numTests;

    numTests = sizeof(sha384tests) / sizeof(sha384tests[0]);
    for (cnt = 0; cnt < numTests; cnt++) {
        rv += testMessageDigest(initCtx, HASH_AlgSHA384, SHA384_LENGTH,
                                (const unsigned char *)sha384tests[cnt].input,
                                sha384tests[cnt].result, &results[0]);
    }
    rv += test_long_message_sha384(initCtx);

    return rv;
}

int
testSHA512(NSSLOWInitContext *initCtx)
{
    static const struct {
        const unsigned char *input;
        const PRUint8 result[SHA512_LENGTH];
    } sha512tests[] = {
        /* Test vectors from FIPS 180-2: appendix C.1.  */
        { (unsigned char *)"abc",
          { 0xdd, 0xaf, 0x35, 0xa1, 0x93, 0x61, 0x7a, 0xba, 0xcc, 0x41, 0x73, 0x49, 0xae, 0x20, 0x41, 0x31,
            0x12, 0xe6, 0xfa, 0x4e, 0x89, 0xa9, 0x7e, 0xa2, 0x0a, 0x9e, 0xee, 0xe6, 0x4b, 0x55, 0xd3, 0x9a,
            0x21, 0x92, 0x99, 0x2a, 0x27, 0x4f, 0xc1, 0xa8, 0x36, 0xba, 0x3c, 0x23, 0xa3, 0xfe, 0xeb, 0xbd,
            0x45, 0x4d, 0x44, 0x23, 0x64, 0x3c, 0xe8, 0x0e, 0x2a, 0x9a, 0xc9, 0x4f, 0xa5, 0x4c, 0xa4, 0x9f } },
        /* Test vectors from FIPS 180-2: appendix C.2.  */
        { (unsigned char *)"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                           "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
          { 0x8e, 0x95, 0x9b, 0x75, 0xda, 0xe3, 0x13, 0xda, 0x8c, 0xf4, 0xf7, 0x28, 0x14, 0xfc, 0x14, 0x3f,
            0x8f, 0x77, 0x79, 0xc6, 0xeb, 0x9f, 0x7f, 0xa1, 0x72, 0x99, 0xae, 0xad, 0xb6, 0x88, 0x90, 0x18,
            0x50, 0x1d, 0x28, 0x9e, 0x49, 0x00, 0xf7, 0xe4, 0x33, 0x1b, 0x99, 0xde, 0xc4, 0xb5, 0x43, 0x3a,
            0xc7, 0xd3, 0x29, 0xee, 0xb6, 0xdd, 0x26, 0x54, 0x5e, 0x96, 0xe5, 0x5b, 0x87, 0x4b, 0xe9, 0x09 } }
    };

    PRUint8 results[SHA512_LENGTH];
    int rv = 0, cnt, numTests;

    numTests = sizeof(sha512tests) / sizeof(sha512tests[0]);
    for (cnt = 0; cnt < numTests; cnt++) {
        rv = testMessageDigest(initCtx, HASH_AlgSHA512, SHA512_LENGTH,
                               (const unsigned char *)sha512tests[cnt].input,
                               sha512tests[cnt].result, &results[0]);
    }
    rv += test_long_message_sha512(initCtx);
    return rv;
}

static void
Usage()
{
    fprintf(stderr, "Usage: %s [algorithm]\n",
            progName);
    fprintf(stderr, "algorithm must be one of %s\n",
            "{ MD5 | SHA1 | SHA224 | SHA256 | SHA384 | SHA512 }");
    fprintf(stderr, "default is to test all\n");
    exit(-1);
}

int
main(int argc, char **argv)
{
    NSSLOWInitContext *initCtx;
    int rv = 0; /* counts the number of failures */

    progName = strrchr(argv[0], '/');
    progName = progName ? progName + 1 : argv[0];

    initCtx = NSSLOW_Init();
    if (initCtx == NULL) {
        SECU_PrintError(progName, "Couldn't initialize for hashing\n");
        return 1;
    }

    if (argc < 2 || !argv[1] || strlen(argv[1]) == 0) {
        rv += testMD5(initCtx);
        rv += testSHA1(initCtx);
        rv += testSHA224(initCtx);
        rv += testSHA256(initCtx);
        rv += testSHA384(initCtx);
        rv += testSHA512(initCtx);
    } else if (strcmp(argv[1], "MD5") == 0) {
        rv += testMD5(initCtx);
    } else if (strcmp(argv[1], "SHA1") == 0) {
        rv += testSHA1(initCtx);
    } else if (strcmp(argv[1], "SHA224") == 0) {
        rv += testSHA224(initCtx);
    } else if (strcmp(argv[1], "SHA256") == 0) {
        rv += testSHA256(initCtx);
    } else if (strcmp(argv[1], "SHA384") == 0) {
        rv += testSHA384(initCtx);
    } else if (strcmp(argv[1], "SHA512") == 0) {
        rv += testSHA512(initCtx);
    } else {
        SECU_PrintError(progName, "Unsupported hash type %s\n", argv[0]);
        Usage();
    }

    NSSLOW_Shutdown(initCtx);

    return (rv == 0) ? 0 : 1;
}
