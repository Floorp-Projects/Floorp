/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * File:    sprintf.c
 * Description:
 *     This is a test program for the PR_snprintf() functions defined
 *     in prprf.c.  This test program is based on ns/nspr/tests/sprintf.c,
 *     revision 1.10.
 * Modification History:
 *  20-May-1997 AGarcia replaced printf statment to return PASS\n. This is to be used by the
 *              regress tool parsing routine.
 ** 04-June-97 AGarcia removed the Test_Result function. Regress tool has been updated to
 *          recognize the return code from tha main program.
 */

#include "prinit.h"
#include "prprf.h"
#include "prlog.h"
#include "prlong.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char sbuf[20000];


/*
** Perform a three way test against PR_smprintf, PR_snprintf, and sprintf.
** Make sure the results are identical
*/
static void test_i(char *pattern, int i)
{
    char *s;
    char buf[200];
    int n;

    /* try all three routines */
    s = PR_smprintf(pattern, i);
    PR_ASSERT(s != 0);
    n = PR_snprintf(buf, sizeof(buf), pattern, i);
    PR_ASSERT(n <= sizeof(buf));
    sprintf(sbuf, pattern, i);

    /* compare results */
    if ((strncmp(s, buf, sizeof(buf)) != 0) ||
        (strncmp(s, sbuf, sizeof(sbuf)) != 0)) {
        fprintf(stderr,
                "pattern='%s' i=%d\nPR_smprintf='%s'\nPR_snprintf='%s'\n    sprintf='%s'\n",
                pattern, i, s, buf, sbuf);
        PR_smprintf_free(s);
        exit(-1);
    }
    PR_smprintf_free(s);
}

static void TestI(void)
{
    static int nums[] = {
        0, 1, -1, 10, -10,
        32767, -32768,
    };
    static char *signs[] = {
        "",
        "0",    "-",    "+", " ",
        "0-",   "0+",   "0 ",   "-0",   "-+",   "- ",
        "+0",   "+-",   "+ ",   " 0",   " -",   " +",
        "0-+",  "0- ",  "0+-",  "0+ ",  "0 -",  "0 +",
        "-0+",  "-0 ",  "-+0",  "-+ ",  "- 0",  "- +",
        "+0-",  "+0 ",  "+-0",  "+- ",  "+ 0",  "+ -",
        " 0-",  " 0+",  " -0",  " -+",  " +0",  " +-",
        "0-+ ", "0- +", "0+- ", "0+ -", "0 -+", "0 +-",
        "-0+ ", "-0 +", "-+0 ", "-+ 0", "- 0+", "- +0",
        "+0- ", "+0 -", "+-0 ", "+- 0", "+ 0-", "+ -0",
        " 0-+", " 0+-", " -0+", " -+0", " +0-", " +-0",
    };
    static char *precs[] = {
        "", "3", "5", "43",
        "7.3", "7.5", "7.11", "7.43",
    };
    static char *formats[] = {
        "d", "o", "x", "u",
        "hd", "ho", "hx", "hu"
    };
    int f, s, n, p;
    char fmt[20];

    for (f = 0; f < PR_ARRAY_SIZE(formats); f++) {
        for (s = 0; s < PR_ARRAY_SIZE(signs); s++) {
            for (p = 0; p < PR_ARRAY_SIZE(precs); p++) {
                fmt[0] = '%';
                fmt[1] = 0;
                if (signs[s]) {
                    strcat(fmt, signs[s]);
                }
                if (precs[p]) {
                    strcat(fmt, precs[p]);
                }
                if (formats[f]) {
                    strcat(fmt, formats[f]);
                }
                for (n = 0; n < PR_ARRAY_SIZE(nums); n++) {
                    test_i(fmt, nums[n]);
                }
            }
        }
    }
}

/************************************************************************/

/*
** Perform a three way test against PR_smprintf, PR_snprintf, and sprintf.
** Make sure the results are identical
*/
static void test_l(char *pattern, char *spattern, PRInt32 l)
{
    char *s;
    char buf[200];
    int n;

    /* try all three routines */
    s = PR_smprintf(pattern, l);
    PR_ASSERT(s != 0);
    n = PR_snprintf(buf, sizeof(buf), pattern, l);
    PR_ASSERT(n <= sizeof(buf));
    sprintf(sbuf, spattern, l);

    /* compare results */
    if ((strncmp(s, buf, sizeof(buf)) != 0) ||
        (strncmp(s, sbuf, sizeof(sbuf)) != 0)) {
        fprintf(stderr,
                "pattern='%s' l=%ld\nPR_smprintf='%s'\nPR_snprintf='%s'\n    sprintf='%s'\n",
                pattern, l, s, buf, sbuf);
        PR_smprintf_free(s);
        exit(-1);
    }
    PR_smprintf_free(s);
}

static void TestL(void)
{
    static PRInt32 nums[] = {
        0,
        1,
        -1,
        10,
        -10,
        32767,
        -32768,
        PR_INT32(0x7fffffff), /* 2147483647L */
        -1 - PR_INT32(0x7fffffff)  /* -2147483648L */
    };
    static char *signs[] = {
        "",
        "0",    "-",    "+", " ",
        "0-",   "0+",   "0 ",   "-0",   "-+",   "- ",
        "+0",   "+-",   "+ ",   " 0",   " -",   " +",
        "0-+",  "0- ",  "0+-",  "0+ ",  "0 -",  "0 +",
        "-0+",  "-0 ",  "-+0",  "-+ ",  "- 0",  "- +",
        "+0-",  "+0 ",  "+-0",  "+- ",  "+ 0",  "+ -",
        " 0-",  " 0+",  " -0",  " -+",  " +0",  " +-",
        "0-+ ", "0- +", "0+- ", "0+ -", "0 -+", "0 +-",
        "-0+ ", "-0 +", "-+0 ", "-+ 0", "- 0+", "- +0",
        "+0- ", "+0 -", "+-0 ", "+- 0", "+ 0-", "+ -0",
        " 0-+", " 0+-", " -0+", " -+0", " +0-", " +-0",
    };
    static char *precs[] = {
        "", "3", "5", "43",
        ".3", ".43",
        "7.3", "7.5", "7.11", "7.43",
    };
    static char *formats[] = { "ld", "lo", "lx", "lu" };

#if PR_BYTES_PER_INT == 4
    static char *sformats[] = { "d", "o", "x", "u" };
#elif PR_BYTES_PER_LONG == 4
    static char *sformats[] = { "ld", "lo", "lx", "lu" };
#else
#error Neither int nor long is 4 bytes on this platform
#endif

    int f, s, n, p;
    char fmt[40], sfmt[40];

    for (f = 0; f < PR_ARRAY_SIZE(formats); f++) {
        for (s = 0; s < PR_ARRAY_SIZE(signs); s++) {
            for (p = 0; p < PR_ARRAY_SIZE(precs); p++) {
                fmt[0] = '%';
                fmt[1] = 0;
                if (signs[s]) {
                    strcat(fmt, signs[s]);
                }
                if (precs[p]) {
                    strcat(fmt, precs[p]);
                }
                strcpy(sfmt, fmt);
                if (formats[f]) {
                    strcat(fmt, formats[f]);
                }
                if (sformats[f]) {
                    strcat(sfmt, sformats[f]);
                }
                for (n = 0; n < PR_ARRAY_SIZE(nums); n++) {
                    test_l(fmt, sfmt, nums[n]);
                }
            }
        }
    }
}

/************************************************************************/

/*
** Perform a three way test against PR_smprintf, PR_snprintf, and sprintf.
** Make sure the results are identical
*/
static void test_ll(char *pattern, char *spattern, PRInt64 l)
{
    char *s;
    char buf[200];
    int n;

    /* try all three routines */
    s = PR_smprintf(pattern, l);
    PR_ASSERT(s != 0);
    n = PR_snprintf(buf, sizeof(buf), pattern, l);
    PR_ASSERT(n <= sizeof(buf));
#if defined(HAVE_LONG_LONG)
    sprintf(sbuf, spattern, l);

    /* compare results */
    if ((strncmp(s, buf, sizeof(buf)) != 0) ||
        (strncmp(s, sbuf, sizeof(sbuf)) != 0)) {
#if PR_BYTES_PER_LONG == 8
#define FORMAT_SPEC "%ld"
#elif defined(WIN16)
#define FORMAT_SPEC "%Ld"
#elif defined(WIN32)
#define FORMAT_SPEC "%I64d"
#else
#define FORMAT_SPEC "%lld"
#endif
        fprintf(stderr,
                "pattern='%s' ll=" FORMAT_SPEC "\nPR_smprintf='%s'\nPR_snprintf='%s'\n    sprintf='%s'\n",
                pattern, l, s, buf, sbuf);
        printf("FAIL\n");
        PR_smprintf_free(s);
        exit(-1);
    }
    PR_smprintf_free(s);
#else
    /* compare results */
    if ((strncmp(s, buf, sizeof(buf)) != 0)) {
        fprintf(stderr,
                "pattern='%s'\nPR_smprintf='%s'\nPR_snprintf='%s'\n    sprintf='%s'\n",
                pattern, s, buf, sbuf);
        printf("FAIL\n");
        PR_smprintf_free(s);
        exit(-1);
    }
    PR_smprintf_free(s);
#endif
}

static void TestLL(void)
{
    static PRInt64 nums[] = {
        LL_INIT(0, 0),
        LL_INIT(0, 1),
        LL_INIT(0xffffffff, 0xffffffff),  /* -1 */
        LL_INIT(0, 10),
        LL_INIT(0xffffffff, 0xfffffff6),  /* -10 */
        LL_INIT(0, 32767),
        LL_INIT(0xffffffff, 0xffff8000),  /* -32768 */
        LL_INIT(0, 0x7fffffff),  /* 2147483647 */
        LL_INIT(0xffffffff, 0x80000000),  /* -2147483648 */
        LL_INIT(0x7fffffff, 0xffffffff),  /* 9223372036854775807 */
        LL_INIT(0x80000000, 0),           /* -9223372036854775808 */
        PR_INT64(0),
        PR_INT64(1),
        PR_INT64(-1),
        PR_INT64(10),
        PR_INT64(-10),
        PR_INT64(32767),
        PR_INT64(-32768),
        PR_INT64(2147483647),
        PR_INT64(-2147483648),
        PR_INT64(9223372036854775807),
        PR_INT64(-9223372036854775808)
    };

    static char *signs[] = {
        "",
        "0",    "-",    "+", " ",
        "0-",   "0+",   "0 ",   "-0",   "-+",   "- ",
        "+0",   "+-",   "+ ",   " 0",   " -",   " +",
        "0-+",  "0- ",  "0+-",  "0+ ",  "0 -",  "0 +",
        "-0+",  "-0 ",  "-+0",  "-+ ",  "- 0",  "- +",
        "+0-",  "+0 ",  "+-0",  "+- ",  "+ 0",  "+ -",
        " 0-",  " 0+",  " -0",  " -+",  " +0",  " +-",
        "0-+ ", "0- +", "0+- ", "0+ -", "0 -+", "0 +-",
        "-0+ ", "-0 +", "-+0 ", "-+ 0", "- 0+", "- +0",
        "+0- ", "+0 -", "+-0 ", "+- 0", "+ 0-", "+ -0",
        " 0-+", " 0+-", " -0+", " -+0", " +0-", " +-0",
    };
    static char *precs[] = {
        "", "3", "5", "43",
        ".3", ".43",
        "7.3", "7.5", "7.11", "7.43",
    };
    static char *formats[] = { "lld", "llo", "llx", "llu" };

#if PR_BYTES_PER_LONG == 8
    static char *sformats[] = { "ld", "lo", "lx", "lu" };
#elif defined(WIN16)
    /* Watcom uses the format string "%Ld" instead of "%lld". */
    static char *sformats[] = { "Ld", "Lo", "Lx", "Lu" };
#elif defined(WIN32)
    static char *sformats[] = { "I64d", "I64o", "I64x", "I64u" };
#else
    static char *sformats[] = { "lld", "llo", "llx", "llu" };
#endif

    int f, s, n, p;
    char fmt[40], sfmt[40];

    for (f = 0; f < PR_ARRAY_SIZE(formats); f++) {
        for (s = 0; s < PR_ARRAY_SIZE(signs); s++) {
            for (p = 0; p < PR_ARRAY_SIZE(precs); p++) {
                fmt[0] = '%';
                fmt[1] = 0;
                if (signs[s]) {
                    strcat(fmt, signs[s]);
                }
                if (precs[p]) {
                    strcat(fmt, precs[p]);
                }
                strcpy(sfmt, fmt);
                if (formats[f]) {
                    strcat(fmt, formats[f]);
                }
                if (sformats[f]) {
                    strcat(sfmt, sformats[f]);
                }
                for (n = 0; n < PR_ARRAY_SIZE(nums); n++) {
                    test_ll(fmt, sfmt, nums[n]);
                }
            }
        }
    }
}

/************************************************************************/

/*
** Perform a three way test against PR_smprintf, PR_snprintf, and sprintf.
** Make sure the results are identical
*/
static void test_s(char *pattern, char *ss)
{
    char *s;
    unsigned char before[8];
    char buf[200];
    unsigned char after[8];
    int n;

    memset(before, 0xBB, 8);
    memset(after, 0xAA, 8);

    /* try all three routines */
    s = PR_smprintf(pattern, ss);
    PR_ASSERT(s != 0);
    n = PR_snprintf(buf, sizeof(buf), pattern, ss);
    PR_ASSERT(n <= sizeof(buf));
    sprintf(sbuf, pattern, ss);

    for (n = 0; n < 8; n++) {
        PR_ASSERT(before[n] == 0xBB);
        PR_ASSERT(after[n] == 0xAA);
    }

    /* compare results */
    if ((strncmp(s, buf, sizeof(buf)) != 0) ||
        (strncmp(s, sbuf, sizeof(sbuf)) != 0)) {
        fprintf(stderr,
                "pattern='%s' ss=%.20s\nPR_smprintf='%s'\nPR_snprintf='%s'\n    sprintf='%s'\n",
                pattern, ss, s, buf, sbuf);
        printf("FAIL\n");
        PR_smprintf_free(s);
        exit(-1);
    }
    PR_smprintf_free(s);
}

static void TestS(void)
{
    static char *strs[] = {
        "",
        "a",
        "abc",
        "abcde",
        "abcdefABCDEF",
        "abcdefghijklmnopqrstuvwxyz0123456789!@#$"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$"
        "abcdefghijklmnopqrstuvwxyz0123456789!@#$",
    };
    /* '0' is not relevant to printing strings */
    static char *signs[] = {
        "",
        "-",    "+",    " ",
        "-+",   "- ",   "+-",   "+ ",   " -",   " +",
        "-+ ",  "- +",  "+- ",  "+ -",  " -+",  " +-",
    };
    static char *precs[] = {
        "", "3", "5", "43",
        ".3", ".43",
        "7.3", "7.5", "7.11", "7.43",
    };
    static char *formats[] = { "s" };
    int f, s, n, p;
    char fmt[40];

    for (f = 0; f < PR_ARRAY_SIZE(formats); f++) {
        for (s = 0; s < PR_ARRAY_SIZE(signs); s++) {
            for (p = 0; p < PR_ARRAY_SIZE(precs); p++) {
                fmt[0] = '%';
                fmt[1] = 0;
                if (signs[s]) {
                    strcat(fmt+strlen(fmt), signs[s]);
                }
                if (precs[p]) {
                    strcat(fmt+strlen(fmt), precs[p]);
                }
                if (formats[f]) {
                    strcat(fmt+strlen(fmt), formats[f]);
                }
                for (n = 0; n < PR_ARRAY_SIZE(strs); n++) {
                    test_s(fmt, strs[n]);
                }
            }
        }
    }
}

/************************************************************************/

int main(int argc, char **argv)
{
    PR_STDIO_INIT();
    TestI();
    TestL();
    TestLL();
    TestS();
    printf("PASS\n");
    return 0;
}
