/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plstr.h"
#include "nspr.h"

#include <stdio.h>

/* PL_strlen */
PRBool test_001(void)
{
    static struct
    {
        const char *str;
        PRUint32    len;
    } array[] =
    {
        { (const char *)0, 0 },
        { "", 0 },
        { "a", 1 },
        { "abcdefg", 7 },
        { "abcdefg\0hijk", 7 }
    };

    int i;

    printf("Test 001 (PL_strlen)      ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        if( PL_strlen(array[i].str) != array[i].len )
        {
            printf("FAIL (%d: %s->%d, %d)\n", i,
                   array[i].str ? array[i].str : "(null)",
                   PL_strlen(array[i].str), array[i].len);
            return PR_FALSE;
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strnlen */
PRBool test_002(void)
{
    static struct
    {
        const char *str;
        PRUint32    max;
        PRUint32    len;
    } array[] =
    {
        { (const char *)0, 0, 0 },
        { (const char *)0, 12, 0 },
        { "", 0, 0 },
        { "", 12, 0 },
        { "a", 0, 0 },
        { "a", 1, 1 },
        { "a", 12, 1 },
        { "abcdefg", 0, 0 },
        { "abcdefg", 1, 1 },
        { "abcdefg", 7, 7 },
        { "abcdefg", 12, 7 },
        { "abcdefg\0hijk", 0, 0 },
        { "abcdefg\0hijk", 1, 1 },
        { "abcdefg\0hijk", 7, 7 },
        { "abcdefg\0hijk", 12, 7 },
    };

    int i;

    printf("Test 002 (PL_strnlen)     ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        if( PL_strnlen(array[i].str, array[i].max) != array[i].len )
        {
            printf("FAIL (%d: %s,%d->%d, %d)\n", i,
                   array[i].str ? array[i].str : "(null)", array[i].max,
                   PL_strnlen(array[i].str, array[i].max), array[i].len);
            return PR_FALSE;
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strcpy */
PRBool test_003(void)
{
    static char buffer[ 1024 ];

    static struct
    {
        const char *str;
        char       *dest;
        char       *rv;
        PRBool      comp;
    } array[] =
    {
        { (const char *)0, (char *)0, (char *)0, PR_FALSE },
        { (const char *)0, buffer, (char *)0, PR_FALSE },
        { "", (char *)0, (char *)0, PR_FALSE },
        { "", buffer, buffer, PR_TRUE },
        { "a", (char *)0, (char *)0, PR_FALSE },
        { "a", buffer, buffer, PR_TRUE },
        { "abcdefg", (char *)0, (char *)0, PR_FALSE },
        { "abcdefg", buffer, buffer, PR_TRUE },
        { "wxyz\0abcdefg", (char *)0, (char *)0, PR_FALSE },
        { "wxyz\0abcdefg", buffer, buffer, PR_TRUE }
    };

    int i;

    printf("Test 003 (PL_strcpy)      ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv;
        const char *a = array[i].str;
        const char *b = (const char *)array[i].dest;

        rv = PL_strcpy(array[i].dest, array[i].str);
        if( array[i].rv != rv )
        {
            printf("FAIL %d: (0x%x, %s)->0x%x\n", i, array[i].dest,
                   array[i].str ? array[i].str : "(null)", rv);
            return PR_FALSE;
        }

        if( array[i].comp )
        {
            while( 1 )
            {
                if( *a != *b )
                {
                    printf("FAIL %d: %s->%.32s\n", i,
                           array[i].str ? array[i].str : "(null)",
                           array[i].dest ? array[i].dest : "(null)");
                    return PR_FALSE;
                }

                if( (char)0 == *a ) {
                    break;
                }

                a++;
                b++;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strncpy */
PRBool test_004(void)
{
    static char buffer[ 1024 ];

    static struct
    {
        const char *str;
        PRUint32    len;
        char       *dest;
        char       *rv;
        PRBool      comp;
        const char *result;
        PRBool      nulled;
    } array[] =
    {
        { (const char *)0, 0, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { (const char *)0, 0, buffer, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { (const char *)0, 1, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { (const char *)0, 7, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { (const char *)0, 1, buffer, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { (const char *)0, 7, buffer, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "", 0, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "", 0, buffer, buffer, PR_FALSE, (const char *)0, PR_FALSE },
        { "", 1, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "", 7, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "", 1, buffer, buffer, PR_TRUE, "", PR_TRUE },
        { "", 7, buffer, buffer, PR_TRUE, "", PR_TRUE },
        { "a", 0, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "a", 0, buffer, buffer, PR_FALSE, (const char *)0, PR_FALSE },
        { "a", 1, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "a", 7, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "b", 1, buffer, buffer, PR_TRUE, "b", PR_FALSE },
        { "c", 7, buffer, buffer, PR_TRUE, "c", PR_TRUE },
        { "de", 0, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "de", 0, buffer, buffer, PR_FALSE, (const char *)0, PR_FALSE },
        { "de", 1, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "de", 7, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "fg", 1, buffer, buffer, PR_TRUE, "f", PR_FALSE },
        { "hi", 7, buffer, buffer, PR_TRUE, "hi", PR_TRUE },
        { "jklmnopq", 0, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "jklmnopq", 0, buffer, buffer, PR_FALSE, (const char *)0, PR_FALSE },
        { "jklmnopq", 1, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "jklmnopq", 7, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "rstuvwxy", 1, buffer, buffer, PR_TRUE, "r", PR_FALSE },
        { "zABCDEFG", 7, buffer, buffer, PR_TRUE, "zABCDEF", PR_FALSE },
        { "a\0XXX", 0, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "a\0XXX", 0, buffer, buffer, PR_FALSE, (const char *)0, PR_FALSE },
        { "a\0XXX", 1, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "a\0XXX", 7, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "b\0XXX", 1, buffer, buffer, PR_TRUE, "b", PR_FALSE },
        { "c\0XXX", 7, buffer, buffer, PR_TRUE, "c", PR_TRUE },
        { "de\0XXX", 0, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "de\0XXX", 0, buffer, buffer, PR_FALSE, (const char *)0, PR_FALSE },
        { "de\0XXX", 1, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "de\0XXX", 7, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "fg\0XXX", 1, buffer, buffer, PR_TRUE, "f", PR_FALSE },
        { "hi\0XXX", 7, buffer, buffer, PR_TRUE, "hi", PR_TRUE },
        { "jklmnopq\0XXX", 0, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "jklmnopq\0XXX", 0, buffer, buffer, PR_FALSE, (const char *)0, PR_FALSE },
        { "jklmnopq\0XXX", 1, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "jklmnopq\0XXX", 7, (char *)0, (char *)0, PR_FALSE, (const char *)0, PR_FALSE },
        { "rstuvwxy\0XXX", 1, buffer, buffer, PR_TRUE, "r", PR_FALSE },
        { "zABCDEFG\0XXX", 7, buffer, buffer, PR_TRUE, "zABCDEF", PR_FALSE },
    };

    int i;

    printf("Test 004 (PL_strncpy)     ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv;
        int j;

        for( j = 0; j < sizeof(buffer); j++ ) {
            buffer[j] = '-';
        }

        rv = PL_strncpy(array[i].dest, array[i].str, array[i].len);
        if( array[i].rv != rv )
        {
            printf("FAIL %d: (0x%x, %s, %lu)->0x%x\n", i, array[i].dest,
                   array[i].str ? array[i].str : "(null)", array[i].len, rv);
            return PR_FALSE;
        }

        if( array[i].comp )
        {
            const char *a = array[i].result;
            const char *b = array[i].dest;

            while( *a )
            {
                if( *a != *b )
                {
                    printf("FAIL %d: %s != %.32s\n", i,
                           array[i].result, array[i].dest);
                    return PR_FALSE;
                }

                a++;
                b++;
            }

            if( array[i].nulled )
            {
                if( *b != '\0' )
                {
                    printf("FAIL %d: not terminated\n", i);
                    return PR_FALSE;
                }
            }
            else
            {
                if( *b != '-' )
                {
                    printf("FAIL %d: overstepped\n", i);
                    return PR_FALSE;
                }
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strncpyz */
PRBool test_005(void)
{
    static char buffer[ 1024 ];

    static struct
    {
        const char *str;
        PRUint32    len;
        char       *dest;
        char       *rv;
        PRBool      comp;
        const char *result;
    } array[] =
    {
        { (const char *)0, 0, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { (const char *)0, 0, buffer, (char *)0, PR_FALSE, (const char *)0 },
        { (const char *)0, 1, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { (const char *)0, 7, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { (const char *)0, 1, buffer, (char *)0, PR_FALSE, (const char *)0 },
        { (const char *)0, 7, buffer, (char *)0, PR_FALSE, (const char *)0 },
        { "", 0, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "", 0, buffer, (char *)0, PR_FALSE, (const char *)0 },
        { "", 1, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "", 7, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "", 1, buffer, buffer, PR_TRUE, "" },
        { "", 7, buffer, buffer, PR_TRUE, "" },
        { "a", 0, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "a", 0, buffer, (char *)0, PR_FALSE, (const char *)0 },
        { "a", 1, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "a", 7, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "b", 1, buffer, buffer, PR_TRUE, "" },
        { "c", 7, buffer, buffer, PR_TRUE, "c" },
        { "de", 0, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "de", 0, buffer, (char *)0, PR_FALSE, (const char *)0 },
        { "de", 1, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "de", 7, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "fg", 1, buffer, buffer, PR_TRUE, "" },
        { "hi", 7, buffer, buffer, PR_TRUE, "hi" },
        { "jklmnopq", 0, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "jklmnopq", 0, buffer, (char *)0, PR_FALSE, (const char *)0 },
        { "jklmnopq", 1, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "jklmnopq", 7, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "rstuvwxy", 1, buffer, buffer, PR_TRUE, "" },
        { "zABCDEFG", 7, buffer, buffer, PR_TRUE, "zABCDE" },
        { "a\0XXX", 0, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "a\0XXX", 0, buffer, (char *)0, PR_FALSE, (const char *)0 },
        { "a\0XXX", 1, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "a\0XXX", 7, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "b\0XXX", 1, buffer, buffer, PR_TRUE, "" },
        { "c\0XXX", 7, buffer, buffer, PR_TRUE, "c" },
        { "de\0XXX", 0, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "de\0XXX", 0, buffer, (char *)0, PR_FALSE, (const char *)0 },
        { "de\0XXX", 1, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "de\0XXX", 7, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "fg\0XXX", 1, buffer, buffer, PR_TRUE, "" },
        { "hi\0XXX", 7, buffer, buffer, PR_TRUE, "hi" },
        { "jklmnopq\0XXX", 0, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "jklmnopq\0XXX", 0, buffer, (char *)0, PR_FALSE, (const char *)0 },
        { "jklmnopq\0XXX", 1, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "jklmnopq\0XXX", 7, (char *)0, (char *)0, PR_FALSE, (const char *)0 },
        { "rstuvwxy\0XXX", 1, buffer, buffer, PR_TRUE, "" },
        { "zABCDEFG\0XXX", 7, buffer, buffer, PR_TRUE, "zABCDE" },
    };

    int i;

    printf("Test 005 (PL_strncpyz)    ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv;
        int j;

        for( j = 0; j < sizeof(buffer); j++ ) {
            buffer[j] = '-';
        }

        rv = PL_strncpyz(array[i].dest, array[i].str, array[i].len);
        if( array[i].rv != rv )
        {
            printf("FAIL %d: (0x%x, %s, %lu)->0x%x\n", i, array[i].dest,
                   array[i].str ? array[i].str : "(null)", array[i].len, rv);
            return PR_FALSE;
        }

        if( array[i].comp )
        {
            const char *a = array[i].result;
            const char *b = array[i].dest;

            while( 1 )
            {
                if( *a != *b )
                {
                    printf("FAIL %d: %s != %.32s\n", i,
                           array[i].result, array[i].dest);
                    return PR_FALSE;
                }

                if( (char)0 == *a ) {
                    break;
                }

                a++;
                b++;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strdup */
PRBool test_006(void)
{
    static const char *array[] =
    {
        (const char *)0,
        "",
        "a",
        "abcdefg"
    };

    int i;

    printf("Test 006 (PL_strdup)      ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strdup(array[i]);

        if( (char *)0 == rv )
        {
            printf("FAIL %d: 0x%x -> 0\n", i, array[i]);
            return PR_FALSE;
        }

        if( (const char *)0 == array[i] )
        {
            if( (char)0 != *rv )
            {
                printf("FAIL %d: (const char *)0 -> %.32s\n", i, rv);
                return PR_FALSE;
            }
        }
        else
        {
            const char *a = array[i];
            const char *b = (const char *)rv;

            while( 1 )
            {
                if( *a != *b )
                {
                    printf("FAIL %d: %s != %.32s\n", i, array[i], rv);
                    return PR_FALSE;
                }

                if( (char)0 == *a ) {
                    break;
                }

                a++;
                b++;
            }

        }
        PL_strfree(rv);
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strndup */
PRBool test_007(void)
{
    static struct
    {
        const char *str;
        PRUint32    len;
        const char *result;
    } array[] =
    {
        { (const char *)0, 0, "" },
        { (const char *)0, 1, "" },
        { (const char *)0, 7, "" },
        { "", 0, "" },
        { "", 1, "" },
        { "", 7, "" },
        { "a", 0, "" },
        { "a", 1, "a" },
        { "a", 7, "a" },
        { "ab", 0, "" },
        { "ab", 1, "a" },
        { "ab", 7, "ab" },
        { "abcdefg", 0, "" },
        { "abcdefg", 1, "a" },
        { "abcdefg", 7, "abcdefg" },
        { "abcdefghijk", 0, "" },
        { "abcdefghijk", 1, "a" },
        { "abcdefghijk", 7, "abcdefg" },
        { "abcdef\0ghijk", 0, "" },
        { "abcdef\0ghijk", 1, "a" },
        { "abcdef\0ghijk", 7, "abcdef" }
    };

    int i;

    printf("Test 007 (PL_strndup)     ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strndup(array[i].str, array[i].len);
        const char *a;
        const char *b;

        if( (char *)0 == rv )
        {
            printf("FAIL %d: %s,%lu -> 0\n", i,
                   array[i].str ? array[i].str : "(null)", array[i].len);
            return PR_FALSE;
        }

        a = array[i].result;
        b = (const char *)rv;

        while( 1 )
        {
            if( *a != *b )
            {
                printf("FAIL %d: %s != %.32s\n", i, array[i].result, rv);
                return PR_FALSE;
            }

            if( (char)0 == *a ) {
                break;
            }

            a++;
            b++;
        }

        free(rv);
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strcat */
PRBool test_008(void)
{
    static struct
    {
        const char *first;
        const char *second;
        const char *result;
    } array[] =
    {
        { (const char *)0, (const char *)0, (const char *)0 },
        { (const char *)0, "xyz", (const char *)0 },
        { "", (const char *)0, "" },
        { "", "", "" },
        { "ab", "", "ab" },
        { "cd", "ef", "cdef" },
        { "gh\0X", "", "gh" },
        { "ij\0X", "kl", "ijkl" },
        { "mn\0X", "op\0X", "mnop" },
        { "qr", "st\0X", "qrst" },
        { "uv\0X", "wx\0X", "uvwx" }
    };

    int i;

    printf("Test 008 (PL_strcat)      ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char buffer[ 1024 ];
        int j;
        char *rv;

        for( j = 0; j < sizeof(buffer); j++ ) {
            buffer[j] = '-';
        }

        if( (const char *)0 != array[i].first ) {
            (void)PL_strcpy(buffer, array[i].first);
        }

        rv = PL_strcat(((const char *)0 == array[i].first) ? (char *)0 : buffer,
                       array[i].second);

        if( (const char *)0 == array[i].result )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s+%s -> %.32s, not zero\n", i,
                       array[i].first ? array[i].first : "(null)",
                       array[i].second ? array[i].second : "(null)",
                       rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s+%s -> null, not %s\n", i,
                       array[i].first ? array[i].first : "(null)",
                       array[i].second ? array[i].second : "(null)",
                       array[i].result);
                return PR_FALSE;
            }
            else
            {
                const char *a = array[i].result;
                const char *b = (const char *)rv;

                while( 1 )
                {
                    if( *a != *b )
                    {
                        printf("FAIL %d: %s+%s -> %.32s, not %s\n", i,
                               array[i].first ? array[i].first : "(null)",
                               array[i].second ? array[i].second : "(null)",
                               rv, array[i].result);
                        return PR_FALSE;
                    }

                    if( (char)0 == *a ) {
                        break;
                    }

                    a++;
                    b++;
                }
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strncat */
PRBool test_009(void)
{
    static struct
    {
        const char *first;
        const char *second;
        PRUint32    length;
        PRBool      nulled;
        const char *result;
    } array[] =
    {
        { (const char *)0, (const char *)0, 0, PR_FALSE, (const char *)0 },
        { (const char *)0, (const char *)0, 1, PR_FALSE, (const char *)0 },
        { (const char *)0, (const char *)0, 7, PR_FALSE, (const char *)0 },
        { (const char *)0, "", 0, PR_FALSE, (const char *)0 },
        { (const char *)0, "", 1, PR_FALSE, (const char *)0 },
        { (const char *)0, "", 7, PR_FALSE, (const char *)0 },
        { (const char *)0, "stuff", 0, PR_FALSE, (const char *)0 },
        { (const char *)0, "stuff", 1, PR_FALSE, (const char *)0 },
        { (const char *)0, "stuff", 7, PR_FALSE, (const char *)0 },
        { "", (const char *)0, 0, PR_TRUE, "" },
        { "", (const char *)0, 1, PR_TRUE, "" },
        { "", (const char *)0, 7, PR_TRUE, "" },
        { "", "", 0, PR_TRUE, "" },
        { "", "", 1, PR_TRUE, "" },
        { "", "", 7, PR_TRUE, "" },
        { "", "abcdefgh", 0, PR_TRUE, "" },
        { "", "abcdefgh", 1, PR_FALSE, "a" },
        { "", "abcdefgh", 7, PR_FALSE, "abcdefg" },
        { "xyz", (const char *)0, 0, PR_TRUE, "xyz" },
        { "xyz", (const char *)0, 1, PR_TRUE, "xyz" },
        { "xyz", (const char *)0, 7, PR_TRUE, "xyz" },
        { "xyz", "", 0, PR_TRUE, "xyz" },
        { "xyz", "", 1, PR_TRUE, "xyz" },
        { "xyz", "", 7, PR_TRUE, "xyz" },
        { "xyz", "abcdefgh", 0, PR_TRUE, "xyz" },
        { "xyz", "abcdefgh", 1, PR_FALSE, "xyza" },
        { "xyz", "abcdefgh", 7, PR_FALSE, "xyzabcdefg" }
    };

    int i;

    printf("Test 009 (PL_strncat)     ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char buffer[ 1024 ];
        int j;
        char *rv;

        for( j = 0; j < sizeof(buffer); j++ ) {
            buffer[j] = '-';
        }

        if( (const char *)0 != array[i].first ) {
            (void)PL_strcpy(buffer, array[i].first);
        }

        rv = PL_strncat(((const char *)0 == array[i].first) ? (char *)0 : buffer,
                        array[i].second, array[i].length);

        if( (const char *)0 == array[i].result )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s+%s/%lu -> %.32s, not zero\n", i,
                       array[i].first ? array[i].first : "(null)",
                       array[i].second ? array[i].second : "(null)",
                       array[i].length, rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s+%s/%lu -> null, not %s\n", i,
                       array[i].first ? array[i].first : "(null)",
                       array[i].second ? array[i].second : "(null)",
                       array[i].length, array[i].result);
                return PR_FALSE;
            }
            else
            {
                const char *a = array[i].result;
                const char *b = (const char *)rv;

                while( *a )
                {
                    if( *a != *b )
                    {
                        printf("FAIL %d: %s+%s/%lu -> %.32s, not %s\n", i,
                               array[i].first ? array[i].first : "(null)",
                               array[i].second ? array[i].second : "(null)",
                               array[i].length, rv, array[i].result);
                        return PR_FALSE;
                    }

                    a++;
                    b++;
                }

                if( array[i].nulled )
                {
                    if( (char)0 != *b )
                    {
                        printf("FAIL %d: %s+%s/%lu -> not nulled\n", i,
                               array[i].first ? array[i].first : "(null)",
                               array[i].second ? array[i].second : "(null)",
                               array[i].length);
                        return PR_FALSE;
                    }
                }
                else
                {
                    if( (char)0 == *b )
                    {
                        printf("FAIL %d: %s+%s/%lu -> overrun\n", i,
                               array[i].first ? array[i].first : "(null)",
                               array[i].second ? array[i].second : "(null)",
                               array[i].length);
                        return PR_FALSE;
                    }
                }
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strcatn */
PRBool test_010(void)
{
    static struct
    {
        const char *first;
        const char *second;
        PRUint32    length;
        const char *result;
    } array[] =
    {
        { (const char *)0, (const char *)0, 0, (const char *)0 },
        { (const char *)0, (const char *)0, 1, (const char *)0 },
        { (const char *)0, (const char *)0, 7, (const char *)0 },
        { (const char *)0, "", 0, (const char *)0 },
        { (const char *)0, "", 1, (const char *)0 },
        { (const char *)0, "", 7, (const char *)0 },
        { (const char *)0, "stuff", 0, (const char *)0 },
        { (const char *)0, "stuff", 1, (const char *)0 },
        { (const char *)0, "stuff", 7, (const char *)0 },
        { "", (const char *)0, 0, "" },
        { "", (const char *)0, 1, "" },
        { "", (const char *)0, 7, "" },
        { "", "", 0, "" },
        { "", "", 1, "" },
        { "", "", 7, "" },
        { "", "abcdefgh", 0, "" },
        { "", "abcdefgh", 1, "" },
        { "", "abcdefgh", 7, "abcdef" },
        { "xyz", (const char *)0, 0, "xyz" },
        { "xyz", (const char *)0, 1, "xyz" },
        { "xyz", (const char *)0, 7, "xyz" },
        { "xyz", "", 0, "xyz" },
        { "xyz", "", 1, "xyz" },
        { "xyz", "", 7, "xyz" },
        { "xyz", "abcdefgh", 0, "xyz" },
        { "xyz", "abcdefgh", 1, "xyz" },
        { "xyz", "abcdefgh", 7, "xyzabc" }
    };

    int i;

    printf("Test 010 (PL_strcatn)     ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char buffer[ 1024 ];
        int j;
        char *rv;

        for( j = 0; j < sizeof(buffer); j++ ) {
            buffer[j] = '-';
        }

        if( (const char *)0 != array[i].first ) {
            (void)PL_strcpy(buffer, array[i].first);
        }

        rv = PL_strcatn(((const char *)0 == array[i].first) ? (char *)0 : buffer,
                        array[i].length, array[i].second);

        if( (const char *)0 == array[i].result )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s+%s/%lu -> %.32s, not zero\n", i,
                       array[i].first ? array[i].first : "(null)",
                       array[i].second ? array[i].second : "(null)",
                       array[i].length, rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s+%s/%lu -> null, not %s\n", i,
                       array[i].first ? array[i].first : "(null)",
                       array[i].second ? array[i].second : "(null)",
                       array[i].length, array[i].result);
                return PR_FALSE;
            }
            else
            {
                const char *a = array[i].result;
                const char *b = (const char *)rv;

                while( 1 )
                {
                    if( *a != *b )
                    {
                        printf("FAIL %d: %s+%s/%lu -> %.32s, not %s\n", i,
                               array[i].first ? array[i].first : "(null)",
                               array[i].second ? array[i].second : "(null)",
                               array[i].length, rv, array[i].result);
                        return PR_FALSE;
                    }

                    if( (char)0 == *a ) {
                        break;
                    }

                    a++;
                    b++;
                }
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strcmp */
PRBool test_011(void)
{
    static struct
    {
        const char *one;
        const char *two;
        PRIntn      sign;
    } array[] =
    {
        { (const char *)0, (const char *)0, 0 },
        { (const char *)0, "word", -1 },
        { "word", (const char *)0, 1 },
        { "word", "word", 0 },
        { "aZYXVUT", "bZYXVUT", -1 },
        { "aZYXVUT", "bAAAAAA", -1 },
        { "a", "aa", -1 },
        { "a", "a", 0 },
        { "a", "A", 1 },
        { "aaaaa", "baaaa", -1 },
        { "aaaaa", "abaaa", -1 },
        { "aaaaa", "aabaa", -1 },
        { "aaaaa", "aaaba", -1 },
        { "aaaaa", "aaaab", -1 },
        { "bZYXVUT", "aZYXVUT", 1 },
        { "bAAAAAA", "aZYXVUT", 1 },
        { "aa", "a", 1 },
        { "A", "a", -1 },
        { "baaaa", "aaaaa", 1 },
        { "abaaa", "aaaaa", 1 },
        { "aabaa", "aaaaa", 1 },
        { "aaaba", "aaaaa", 1 },
        { "aaaab", "aaaaa", 1 },
        { "word", "Word", 1 },
        { "word", "wOrd", 1 },
        { "word", "woRd", 1 },
        { "word", "worD", 1 },
        { "WORD", "wORD", -1 },
        { "WORD", "WoRD", -1 },
        { "WORD", "WOrD", -1 },
        { "WORD", "WORd", -1 }
    };

    int i;

    printf("Test 011 (PL_strcmp)      ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRIntn rv = PL_strcmp(array[i].one, array[i].two);

        switch( array[i].sign )
        {
            case -1:
                if( rv < 0 ) {
                    continue;
                }
                break;
            case 1:
                if( rv > 0 ) {
                    continue;
                }
                break;
            case 0:
                if( 0 == rv ) {
                    continue;
                }
                break;
            default:
                PR_NOT_REACHED("static data inconsistancy");
                break;
        }

        printf("FAIL %d: %s-%s -> %d, not %d\n", i,
               array[i].one ? array[i].one : "(null)",
               array[i].two ? array[i].two : "(null)",
               rv, array[i].sign);
        return PR_FALSE;
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strncmp */
PRBool test_012(void)
{
    static struct
    {
        const char *one;
        const char *two;
        PRUint32    max;
        PRIntn      sign;
    } array[] =
    {
        { (const char *)0, (const char *)0, 0, 0 },
        { (const char *)0, (const char *)0, 1, 0 },
        { (const char *)0, (const char *)0, 4, 0 },
        { (const char *)0, "word", 0, -1 },
        { (const char *)0, "word", 1, -1 },
        { (const char *)0, "word", 4, -1 },
        { "word", (const char *)0, 0, 1 },
        { "word", (const char *)0, 1, 1 },
        { "word", (const char *)0, 4, 1 },
        { "word", "word", 0, 0 },
        { "word", "word", 1, 0 },
        { "word", "word", 3, 0 },
        { "word", "word", 5, 0 },
        { "aZYXVUT", "bZYXVUT", 0, 0 },
        { "aZYXVUT", "bZYXVUT", 1, -1 },
        { "aZYXVUT", "bZYXVUT", 4, -1 },
        { "aZYXVUT", "bZYXVUT", 9, -1 },
        { "aZYXVUT", "bAAAAAA", 0, 0 },
        { "aZYXVUT", "bAAAAAA", 1, -1 },
        { "aZYXVUT", "bAAAAAA", 4, -1 },
        { "aZYXVUT", "bAAAAAA", 5, -1 },
        { "a", "aa", 0, 0 },
        { "a", "aa", 1, 0 },
        { "a", "aa", 4, -1 },
        { "a", "a", 0, 0 },
        { "a", "a", 1, 0 },
        { "a", "a", 4, 0 },
        { "a", "A", 0, 0 },
        { "a", "A", 1, 1 },
        { "a", "A", 4, 1 },
        { "aaaaa", "baaaa", 0, 0 },
        { "aaaaa", "baaaa", 1, -1 },
        { "aaaaa", "baaaa", 4, -1 },
        { "aaaaa", "abaaa", 0, 0 },
        { "aaaaa", "abaaa", 1, 0 },
        { "aaaaa", "abaaa", 4, -1 },
        { "aaaaa", "aabaa", 0, 0 },
        { "aaaaa", "aabaa", 1, 0 },
        { "aaaaa", "aabaa", 4, -1 },
        { "aaaaa", "aaaba", 0, 0 },
        { "aaaaa", "aaaba", 1, 0 },
        { "aaaaa", "aaaba", 4, -1 },
        { "aaaaa", "aaaab", 0, 0 },
        { "aaaaa", "aaaab", 1, 0 },
        { "aaaaa", "aaaab", 4, 0 },
        { "bZYXVUT", "aZYXVUT", 0, 0 },
        { "bZYXVUT", "aZYXVUT", 1, 1 },
        { "bZYXVUT", "aZYXVUT", 4, 1 },
        { "bAAAAAA", "aZYXVUT", 0, 0 },
        { "bAAAAAA", "aZYXVUT", 1, 1 },
        { "bAAAAAA", "aZYXVUT", 4, 1 },
        { "aa", "a", 0, 0 },
        { "aa", "a", 1, 0 },
        { "aa", "a", 4, 1 },
        { "A", "a", 0, 0 },
        { "A", "a", 1, -1 },
        { "A", "a", 4, -1 },
        { "baaaa", "aaaaa", 0, 0 },
        { "baaaa", "aaaaa", 1, 1 },
        { "baaaa", "aaaaa", 4, 1 },
        { "abaaa", "aaaaa", 0, 0 },
        { "abaaa", "aaaaa", 1, 0 },
        { "abaaa", "aaaaa", 4, 1 },
        { "aabaa", "aaaaa", 0, 0 },
        { "aabaa", "aaaaa", 1, 0 },
        { "aabaa", "aaaaa", 4, 1 },
        { "aaaba", "aaaaa", 0, 0 },
        { "aaaba", "aaaaa", 1, 0 },
        { "aaaba", "aaaaa", 4, 1 },
        { "aaaab", "aaaaa", 0, 0 },
        { "aaaab", "aaaaa", 1, 0 },
        { "aaaab", "aaaaa", 4, 0 },
        { "word", "Word", 0, 0 },
        { "word", "Word", 1, 1 },
        { "word", "Word", 3, 1 },
        { "word", "wOrd", 0, 0 },
        { "word", "wOrd", 1, 0 },
        { "word", "wOrd", 3, 1 },
        { "word", "woRd", 0, 0 },
        { "word", "woRd", 1, 0 },
        { "word", "woRd", 3, 1 },
        { "word", "worD", 0, 0 },
        { "word", "worD", 1, 0 },
        { "word", "worD", 3, 0 },
        { "WORD", "wORD", 0, 0 },
        { "WORD", "wORD", 1, -1 },
        { "WORD", "wORD", 3, -1 },
        { "WORD", "WoRD", 0, 0 },
        { "WORD", "WoRD", 1, 0 },
        { "WORD", "WoRD", 3, -1 },
        { "WORD", "WOrD", 0, 0 },
        { "WORD", "WOrD", 1, 0 },
        { "WORD", "WOrD", 3, -1 },
        { "WORD", "WORd", 0, 0 },
        { "WORD", "WORd", 1, 0 },
        { "WORD", "WORd", 3, 0 }

    };

    int i;

    printf("Test 012 (PL_strncmp)     ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRIntn rv = PL_strncmp(array[i].one, array[i].two, array[i].max);

        switch( array[i].sign )
        {
            case -1:
                if( rv < 0 ) {
                    continue;
                }
                break;
            case 1:
                if( rv > 0 ) {
                    continue;
                }
                break;
            case 0:
                if( 0 == rv ) {
                    continue;
                }
                break;
            default:
                PR_NOT_REACHED("static data inconsistancy");
                break;
        }

        printf("FAIL %d: %s-%s/%ld -> %d, not %d\n", i,
               array[i].one ? array[i].one : "(null)",
               array[i].two ? array[i].two : "(null)",
               array[i].max, rv, array[i].sign);
        return PR_FALSE;
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strcasecmp */
PRBool test_013(void)
{
    static struct
    {
        const char *one;
        const char *two;
        PRIntn      sign;
    } array[] =
    {
        { (const char *)0, (const char *)0, 0 },
        { (const char *)0, "word", -1 },
        { "word", (const char *)0, 1 },
        { "word", "word", 0 },
        { "aZYXVUT", "bZYXVUT", -1 },
        { "aZYXVUT", "bAAAAAA", -1 },
        { "a", "aa", -1 },
        { "a", "a", 0 },
        { "a", "A", 0 },
        { "aaaaa", "baaaa", -1 },
        { "aaaaa", "abaaa", -1 },
        { "aaaaa", "aabaa", -1 },
        { "aaaaa", "aaaba", -1 },
        { "aaaaa", "aaaab", -1 },
        { "bZYXVUT", "aZYXVUT", 1 },
        { "bAAAAAA", "aZYXVUT", 1 },
        { "aa", "a", 1 },
        { "A", "a", 0 },
        { "baaaa", "aaaaa", 1 },
        { "abaaa", "aaaaa", 1 },
        { "aabaa", "aaaaa", 1 },
        { "aaaba", "aaaaa", 1 },
        { "aaaab", "aaaaa", 1 },
        { "word", "Word", 0 },
        { "word", "wOrd", 0 },
        { "word", "woRd", 0 },
        { "word", "worD", 0 },
        { "WORD", "wORD", 0 },
        { "WORD", "WoRD", 0 },
        { "WORD", "WOrD", 0 },
        { "WORD", "WORd", 0 }
    };

    int i;

    printf("Test 013 (PL_strcasecmp)  ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRIntn rv = PL_strcasecmp(array[i].one, array[i].two);

        switch( array[i].sign )
        {
            case -1:
                if( rv < 0 ) {
                    continue;
                }
                break;
            case 1:
                if( rv > 0 ) {
                    continue;
                }
                break;
            case 0:
                if( 0 == rv ) {
                    continue;
                }
                break;
            default:
                PR_NOT_REACHED("static data inconsistancy");
                break;
        }

        printf("FAIL %d: %s-%s -> %d, not %d\n", i,
               array[i].one ? array[i].one : "(null)",
               array[i].two ? array[i].two : "(null)",
               rv, array[i].sign);
        return PR_FALSE;
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strncasecmp */
PRBool test_014(void)
{
    static struct
    {
        const char *one;
        const char *two;
        PRUint32    max;
        PRIntn      sign;
    } array[] =
    {
        { (const char *)0, (const char *)0, 0, 0 },
        { (const char *)0, (const char *)0, 1, 0 },
        { (const char *)0, (const char *)0, 4, 0 },
        { (const char *)0, "word", 0, -1 },
        { (const char *)0, "word", 1, -1 },
        { (const char *)0, "word", 4, -1 },
        { "word", (const char *)0, 0, 1 },
        { "word", (const char *)0, 1, 1 },
        { "word", (const char *)0, 4, 1 },
        { "word", "word", 0, 0 },
        { "word", "word", 1, 0 },
        { "word", "word", 3, 0 },
        { "word", "word", 5, 0 },
        { "aZYXVUT", "bZYXVUT", 0, 0 },
        { "aZYXVUT", "bZYXVUT", 1, -1 },
        { "aZYXVUT", "bZYXVUT", 4, -1 },
        { "aZYXVUT", "bZYXVUT", 9, -1 },
        { "aZYXVUT", "bAAAAAA", 0, 0 },
        { "aZYXVUT", "bAAAAAA", 1, -1 },
        { "aZYXVUT", "bAAAAAA", 4, -1 },
        { "aZYXVUT", "bAAAAAA", 5, -1 },
        { "a", "aa", 0, 0 },
        { "a", "aa", 1, 0 },
        { "a", "aa", 4, -1 },
        { "a", "a", 0, 0 },
        { "a", "a", 1, 0 },
        { "a", "a", 4, 0 },
        { "a", "A", 0, 0 },
        { "a", "A", 1, 0 },
        { "a", "A", 4, 0 },
        { "aaaaa", "baaaa", 0, 0 },
        { "aaaaa", "baaaa", 1, -1 },
        { "aaaaa", "baaaa", 4, -1 },
        { "aaaaa", "abaaa", 0, 0 },
        { "aaaaa", "abaaa", 1, 0 },
        { "aaaaa", "abaaa", 4, -1 },
        { "aaaaa", "aabaa", 0, 0 },
        { "aaaaa", "aabaa", 1, 0 },
        { "aaaaa", "aabaa", 4, -1 },
        { "aaaaa", "aaaba", 0, 0 },
        { "aaaaa", "aaaba", 1, 0 },
        { "aaaaa", "aaaba", 4, -1 },
        { "aaaaa", "aaaab", 0, 0 },
        { "aaaaa", "aaaab", 1, 0 },
        { "aaaaa", "aaaab", 4, 0 },
        { "bZYXVUT", "aZYXVUT", 0, 0 },
        { "bZYXVUT", "aZYXVUT", 1, 1 },
        { "bZYXVUT", "aZYXVUT", 4, 1 },
        { "bAAAAAA", "aZYXVUT", 0, 0 },
        { "bAAAAAA", "aZYXVUT", 1, 1 },
        { "bAAAAAA", "aZYXVUT", 4, 1 },
        { "aa", "a", 0, 0 },
        { "aa", "a", 1, 0 },
        { "aa", "a", 4, 1 },
        { "A", "a", 0, 0 },
        { "A", "a", 1, 0 },
        { "A", "a", 4, 0 },
        { "baaaa", "aaaaa", 0, 0 },
        { "baaaa", "aaaaa", 1, 1 },
        { "baaaa", "aaaaa", 4, 1 },
        { "abaaa", "aaaaa", 0, 0 },
        { "abaaa", "aaaaa", 1, 0 },
        { "abaaa", "aaaaa", 4, 1 },
        { "aabaa", "aaaaa", 0, 0 },
        { "aabaa", "aaaaa", 1, 0 },
        { "aabaa", "aaaaa", 4, 1 },
        { "aaaba", "aaaaa", 0, 0 },
        { "aaaba", "aaaaa", 1, 0 },
        { "aaaba", "aaaaa", 4, 1 },
        { "aaaab", "aaaaa", 0, 0 },
        { "aaaab", "aaaaa", 1, 0 },
        { "aaaab", "aaaaa", 4, 0 },
        { "word", "Word", 0, 0 },
        { "word", "Word", 1, 0 },
        { "word", "Word", 3, 0 },
        { "word", "wOrd", 0, 0 },
        { "word", "wOrd", 1, 0 },
        { "word", "wOrd", 3, 0 },
        { "word", "woRd", 0, 0 },
        { "word", "woRd", 1, 0 },
        { "word", "woRd", 3, 0 },
        { "word", "worD", 0, 0 },
        { "word", "worD", 1, 0 },
        { "word", "worD", 3, 0 },
        { "WORD", "wORD", 0, 0 },
        { "WORD", "wORD", 1, 0 },
        { "WORD", "wORD", 3, 0 },
        { "WORD", "WoRD", 0, 0 },
        { "WORD", "WoRD", 1, 0 },
        { "WORD", "WoRD", 3, 0 },
        { "WORD", "WOrD", 0, 0 },
        { "WORD", "WOrD", 1, 0 },
        { "WORD", "WOrD", 3, 0 },
        { "WORD", "WORd", 0, 0 },
        { "WORD", "WORd", 1, 0 },
        { "WORD", "WORd", 3, 0 }
    };

    int i;

    printf("Test 014 (PL_strncasecmp) ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRIntn rv = PL_strncasecmp(array[i].one, array[i].two, array[i].max);

        switch( array[i].sign )
        {
            case -1:
                if( rv < 0 ) {
                    continue;
                }
                break;
            case 1:
                if( rv > 0 ) {
                    continue;
                }
                break;
            case 0:
                if( 0 == rv ) {
                    continue;
                }
                break;
            default:
                PR_NOT_REACHED("static data inconsistancy");
                break;
        }

        printf("FAIL %d: %s-%s/%ld -> %d, not %d\n", i,
               array[i].one ? array[i].one : "(null)",
               array[i].two ? array[i].two : "(null)",
               array[i].max, rv, array[i].sign);
        return PR_FALSE;
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strchr */
PRBool test_015(void)
{
    static struct
    {
        const char *str;
        char        chr;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, 'a', PR_FALSE, 0 },
        { (const char *)0, '\0', PR_FALSE, 0 },
        { "abcdefg", 'a', PR_TRUE, 0 },
        { "abcdefg", 'b', PR_TRUE, 1 },
        { "abcdefg", 'c', PR_TRUE, 2 },
        { "abcdefg", 'd', PR_TRUE, 3 },
        { "abcdefg", 'e', PR_TRUE, 4 },
        { "abcdefg", 'f', PR_TRUE, 5 },
        { "abcdefg", 'g', PR_TRUE, 6 },
        { "abcdefg", 'h', PR_FALSE, 0 },
        { "abcdefg", '\0', PR_TRUE, 7 },
        { "abcdefg", 'A', PR_FALSE, 0 },
        { "abcdefg", 'B', PR_FALSE, 0 },
        { "abcdefg", 'C', PR_FALSE, 0 },
        { "abcdefg", 'D', PR_FALSE, 0 },
        { "abcdefg", 'E', PR_FALSE, 0 },
        { "abcdefg", 'F', PR_FALSE, 0 },
        { "abcdefg", 'G', PR_FALSE, 0 },
        { "abcdefg", 'H', PR_FALSE, 0 },
        { "abcdefgabcdefg", 'a', PR_TRUE, 0 },
        { "abcdefgabcdefg", 'b', PR_TRUE, 1 },
        { "abcdefgabcdefg", 'c', PR_TRUE, 2 },
        { "abcdefgabcdefg", 'd', PR_TRUE, 3 },
        { "abcdefgabcdefg", 'e', PR_TRUE, 4 },
        { "abcdefgabcdefg", 'f', PR_TRUE, 5 },
        { "abcdefgabcdefg", 'g', PR_TRUE, 6 },
        { "abcdefgabcdefg", 'h', PR_FALSE, 0 },
        { "abcdefgabcdefg", '\0', PR_TRUE, 14 }
    };

    int i;

    printf("Test 015 (PL_strchr)      ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strchr(array[i].str, array[i].chr);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%c -> %.32s, not zero\n", i, array[i].str,
                       array[i].chr, rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%c -> null, not +%lu\n", i, array[i].str,
                       array[i].chr, array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%c -> 0x%x, not 0x%x+%lu\n", i, array[i].str,
                       array[i].chr, rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strrchr */
PRBool test_016(void)
{
    static struct
    {
        const char *str;
        char        chr;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, 'a', PR_FALSE, 0 },
        { (const char *)0, '\0', PR_FALSE, 0 },
        { "abcdefg", 'a', PR_TRUE, 0 },
        { "abcdefg", 'b', PR_TRUE, 1 },
        { "abcdefg", 'c', PR_TRUE, 2 },
        { "abcdefg", 'd', PR_TRUE, 3 },
        { "abcdefg", 'e', PR_TRUE, 4 },
        { "abcdefg", 'f', PR_TRUE, 5 },
        { "abcdefg", 'g', PR_TRUE, 6 },
        { "abcdefg", 'h', PR_FALSE, 0 },
        { "abcdefg", '\0', PR_TRUE, 7 },
        { "abcdefg", 'A', PR_FALSE, 0 },
        { "abcdefg", 'B', PR_FALSE, 0 },
        { "abcdefg", 'C', PR_FALSE, 0 },
        { "abcdefg", 'D', PR_FALSE, 0 },
        { "abcdefg", 'E', PR_FALSE, 0 },
        { "abcdefg", 'F', PR_FALSE, 0 },
        { "abcdefg", 'G', PR_FALSE, 0 },
        { "abcdefg", 'H', PR_FALSE, 0 },
        { "abcdefgabcdefg", 'a', PR_TRUE, 7 },
        { "abcdefgabcdefg", 'b', PR_TRUE, 8 },
        { "abcdefgabcdefg", 'c', PR_TRUE, 9 },
        { "abcdefgabcdefg", 'd', PR_TRUE, 10 },
        { "abcdefgabcdefg", 'e', PR_TRUE, 11 },
        { "abcdefgabcdefg", 'f', PR_TRUE, 12 },
        { "abcdefgabcdefg", 'g', PR_TRUE, 13 },
        { "abcdefgabcdefg", 'h', PR_FALSE, 0 },
        { "abcdefgabcdefg", '\0', PR_TRUE, 14 }
    };

    int i;

    printf("Test 016 (PL_strrchr)     ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strrchr(array[i].str, array[i].chr);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%c -> %.32s, not zero\n", i, array[i].str,
                       array[i].chr, rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%c -> null, not +%lu\n", i, array[i].str,
                       array[i].chr, array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%c -> 0x%x, not 0x%x+%lu\n", i, array[i].str,
                       array[i].chr, rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strnchr */
PRBool test_017(void)
{
    static struct
    {
        const char *str;
        char        chr;
        PRUint32    max;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, 'a', 2, PR_FALSE, 0 },
        { (const char *)0, '\0', 2, PR_FALSE, 0 },
        { "abcdefg", 'a', 5, PR_TRUE, 0 },
        { "abcdefg", 'b', 5, PR_TRUE, 1 },
        { "abcdefg", 'c', 5, PR_TRUE, 2 },
        { "abcdefg", 'd', 5, PR_TRUE, 3 },
        { "abcdefg", 'e', 5, PR_TRUE, 4 },
        { "abcdefg", 'f', 5, PR_FALSE, 0 },
        { "abcdefg", 'g', 5, PR_FALSE, 0 },
        { "abcdefg", 'h', 5, PR_FALSE, 0 },
        { "abcdefg", '\0', 5, PR_FALSE, 0 },
        { "abcdefg", '\0', 15, PR_TRUE, 7 },
        { "abcdefg", 'A', 5, PR_FALSE, 0 },
        { "abcdefg", 'B', 5, PR_FALSE, 0 },
        { "abcdefg", 'C', 5, PR_FALSE, 0 },
        { "abcdefg", 'D', 5, PR_FALSE, 0 },
        { "abcdefg", 'E', 5, PR_FALSE, 0 },
        { "abcdefg", 'F', 5, PR_FALSE, 0 },
        { "abcdefg", 'G', 5, PR_FALSE, 0 },
        { "abcdefg", 'H', 5, PR_FALSE, 0 },
        { "abcdefgabcdefg", 'a', 10, PR_TRUE, 0 },
        { "abcdefgabcdefg", 'b', 10, PR_TRUE, 1 },
        { "abcdefgabcdefg", 'c', 10, PR_TRUE, 2 },
        { "abcdefgabcdefg", 'd', 10, PR_TRUE, 3 },
        { "abcdefgabcdefg", 'e', 10, PR_TRUE, 4 },
        { "abcdefgabcdefg", 'f', 10, PR_TRUE, 5 },
        { "abcdefgabcdefg", 'g', 10, PR_TRUE, 6 },
        { "abcdefgabcdefg", 'h', 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", '\0', 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", '\0', 14, PR_FALSE, 0 },
        { "abcdefgabcdefg", '\0', 15, PR_TRUE, 14 }
    };

    int i;

    printf("Test 017 (PL_strnchr)     ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strnchr(array[i].str, array[i].chr, array[i].max);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%c/%lu -> %.32s, not zero\n", i, array[i].str,
                       array[i].chr, array[i].max, rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%c/%lu -> null, not +%lu\n", i, array[i].str,
                       array[i].chr, array[i].max, array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%c/%lu -> 0x%x, not 0x%x+%lu\n", i, array[i].str,
                       array[i].chr, array[i].max, rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strnrchr */
PRBool test_018(void)
{
    static struct
    {
        const char *str;
        char        chr;
        PRUint32    max;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, 'a', 2, PR_FALSE, 0 },
        { (const char *)0, '\0', 2, PR_FALSE, 0 },
        { "abcdefg", 'a', 5, PR_TRUE, 0 },
        { "abcdefg", 'b', 5, PR_TRUE, 1 },
        { "abcdefg", 'c', 5, PR_TRUE, 2 },
        { "abcdefg", 'd', 5, PR_TRUE, 3 },
        { "abcdefg", 'e', 5, PR_TRUE, 4 },
        { "abcdefg", 'f', 5, PR_FALSE, 0 },
        { "abcdefg", 'g', 5, PR_FALSE, 0 },
        { "abcdefg", 'h', 5, PR_FALSE, 0 },
        { "abcdefg", '\0', 5, PR_FALSE, 0 },
        { "abcdefg", '\0', 15, PR_TRUE, 7 },
        { "abcdefg", 'A', 5, PR_FALSE, 0 },
        { "abcdefg", 'B', 5, PR_FALSE, 0 },
        { "abcdefg", 'C', 5, PR_FALSE, 0 },
        { "abcdefg", 'D', 5, PR_FALSE, 0 },
        { "abcdefg", 'E', 5, PR_FALSE, 0 },
        { "abcdefg", 'F', 5, PR_FALSE, 0 },
        { "abcdefg", 'G', 5, PR_FALSE, 0 },
        { "abcdefg", 'H', 5, PR_FALSE, 0 },
        { "abcdefgabcdefg", 'a', 10, PR_TRUE, 7 },
        { "abcdefgabcdefg", 'b', 10, PR_TRUE, 8 },
        { "abcdefgabcdefg", 'c', 10, PR_TRUE, 9 },
        { "abcdefgabcdefg", 'd', 10, PR_TRUE, 3 },
        { "abcdefgabcdefg", 'e', 10, PR_TRUE, 4 },
        { "abcdefgabcdefg", 'f', 10, PR_TRUE, 5 },
        { "abcdefgabcdefg", 'g', 10, PR_TRUE, 6 },
        { "abcdefgabcdefg", 'h', 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", '\0', 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", '\0', 14, PR_FALSE, 0 },
        { "abcdefgabcdefg", '\0', 15, PR_TRUE, 14 }
    };

    int i;

    printf("Test 018 (PL_strnrchr)    ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strnrchr(array[i].str, array[i].chr, array[i].max);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%c/%lu -> %.32s, not zero\n", i, array[i].str,
                       array[i].chr, array[i].max, rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%c/%lu -> null, not +%lu\n", i, array[i].str,
                       array[i].chr, array[i].max, array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%c/%lu -> 0x%x, not 0x%x+%lu\n", i, array[i].str,
                       array[i].chr, array[i].max, rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strpbrk */
PRBool test_019(void)
{
    static struct
    {
        const char *str;
        const char *chrs;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, (const char *)0, PR_FALSE, 0 },
        { (const char *)0, "abc", PR_FALSE, 0 },
        { "abc", (const char *)0, PR_FALSE, 0 },
        { "abcdefg", "", PR_FALSE, 0 },
        { "", "aeiou", PR_FALSE, 0 },
        { "abcdefg", "ae", PR_TRUE, 0 },
        { "abcdefg", "ei", PR_TRUE, 4 },
        { "abcdefg", "io", PR_FALSE, 0 },
        { "abcdefg", "bcd", PR_TRUE, 1 },
        { "abcdefg", "cbd", PR_TRUE, 1 },
        { "abcdefg", "dbc", PR_TRUE, 1 },
        { "abcdefg", "ghi", PR_TRUE, 6 },
        { "abcdefg", "AE", PR_FALSE, 0 },
        { "abcdefg", "EI", PR_FALSE, 0 },
        { "abcdefg", "IO", PR_FALSE, 0 },
        { "abcdefg", "BCD", PR_FALSE, 0 },
        { "abcdefg", "CBD", PR_FALSE, 0 },
        { "abcdefg", "DBC", PR_FALSE, 0 },
        { "abcdefg", "GHI", PR_FALSE, 0 },
        { "abcdefgabcdefg", "ae", PR_TRUE, 0 },
        { "abcdefgabcdefg", "ei", PR_TRUE, 4 },
        { "abcdefgabcdefg", "io", PR_FALSE, 0 },
        { "abcdefgabcdefg", "bcd", PR_TRUE, 1 },
        { "abcdefgabcdefg", "cbd", PR_TRUE, 1 },
        { "abcdefgabcdefg", "dbc", PR_TRUE, 1 },
        { "abcdefgabcdefg", "ghi", PR_TRUE, 6 },
        { "abcdefgabcdefg", "AE", PR_FALSE, 0 },
        { "abcdefgabcdefg", "EI", PR_FALSE, 0 },
        { "abcdefgabcdefg", "IO", PR_FALSE, 0 },
        { "abcdefgabcdefg", "BCD", PR_FALSE, 0 },
        { "abcdefgabcdefg", "CBD", PR_FALSE, 0 },
        { "abcdefgabcdefg", "DBC", PR_FALSE, 0 },
        { "abcdefgabcdefg", "GHI", PR_FALSE, 0 }
    };

    int i;

    printf("Test 019 (PL_strpbrk)     ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strpbrk(array[i].str, array[i].chrs);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%s -> %.32s, not null\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].chrs ? array[i].chrs : "(null)",
                       rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%s -> null, not +%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].chrs ? array[i].chrs : "(null)",
                       array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%s -> 0x%x, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].chrs ? array[i].chrs : "(null)",
                       rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strprbrk */
PRBool test_020(void)
{
    static struct
    {
        const char *str;
        const char *chrs;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, (const char *)0, PR_FALSE, 0 },
        { (const char *)0, "abc", PR_FALSE, 0 },
        { "abc", (const char *)0, PR_FALSE, 0 },
        { "abcdefg", "", PR_FALSE, 0 },
        { "", "aeiou", PR_FALSE, 0 },
        { "abcdefg", "ae", PR_TRUE, 4 },
        { "abcdefg", "ei", PR_TRUE, 4 },
        { "abcdefg", "io", PR_FALSE, 0 },
        { "abcdefg", "bcd", PR_TRUE, 3 },
        { "abcdefg", "cbd", PR_TRUE, 3 },
        { "abcdefg", "dbc", PR_TRUE, 3 },
        { "abcdefg", "ghi", PR_TRUE, 6 },
        { "abcdefg", "AE", PR_FALSE, 0 },
        { "abcdefg", "EI", PR_FALSE, 0 },
        { "abcdefg", "IO", PR_FALSE, 0 },
        { "abcdefg", "BCD", PR_FALSE, 0 },
        { "abcdefg", "CBD", PR_FALSE, 0 },
        { "abcdefg", "DBC", PR_FALSE, 0 },
        { "abcdefg", "GHI", PR_FALSE, 0 },
        { "abcdefgabcdefg", "ae", PR_TRUE, 11 },
        { "abcdefgabcdefg", "ei", PR_TRUE, 11 },
        { "abcdefgabcdefg", "io", PR_FALSE, 0 },
        { "abcdefgabcdefg", "bcd", PR_TRUE, 10 },
        { "abcdefgabcdefg", "cbd", PR_TRUE, 10 },
        { "abcdefgabcdefg", "dbc", PR_TRUE, 10 },
        { "abcdefgabcdefg", "ghi", PR_TRUE, 13 },
        { "abcdefgabcdefg", "AE", PR_FALSE, 0 },
        { "abcdefgabcdefg", "EI", PR_FALSE, 0 },
        { "abcdefgabcdefg", "IO", PR_FALSE, 0 },
        { "abcdefgabcdefg", "BCD", PR_FALSE, 0 },
        { "abcdefgabcdefg", "CBD", PR_FALSE, 0 },
        { "abcdefgabcdefg", "DBC", PR_FALSE, 0 },
        { "abcdefgabcdefg", "GHI", PR_FALSE, 0 }
    };

    int i;

    printf("Test 020 (PL_strprbrk)    ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strprbrk(array[i].str, array[i].chrs);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%s -> %.32s, not null\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].chrs ? array[i].chrs : "(null)",
                       rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%s -> null, not +%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].chrs ? array[i].chrs : "(null)",
                       array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%s -> 0x%x, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].chrs ? array[i].chrs : "(null)",
                       rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strnpbrk */
PRBool test_021(void)
{
    static struct
    {
        const char *str;
        const char *chrs;
        PRUint32    max;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, (const char *)0, 3, PR_FALSE, 0 },
        { (const char *)0, "abc", 3, PR_FALSE, 0 },
        { "abc", (const char *)0, 3, PR_FALSE, 0 },
        { "abcdefg", "", 3, PR_FALSE, 0 },
        { "", "aeiou", 3, PR_FALSE, 0 },
        { "abcdefg", "ae", 0, PR_FALSE, 0 },
        { "abcdefg", "ae", 1, PR_TRUE, 0 },
        { "abcdefg", "ae", 4, PR_TRUE, 0 },
        { "abcdefg", "ae", 5, PR_TRUE, 0 },
        { "abcdefg", "ae", 6, PR_TRUE, 0 },
        { "abcdefg", "ei", 4, PR_FALSE, 0 },
        { "abcdefg", "io", 10, PR_FALSE, 0 },
        { "abcdefg", "bcd", 2, PR_TRUE, 1 },
        { "abcdefg", "cbd", 2, PR_TRUE, 1 },
        { "abcdefg", "dbc", 2, PR_TRUE, 1 },
        { "abcdefg", "ghi", 6, PR_FALSE, 0 },
        { "abcdefg", "ghi", 7, PR_TRUE, 6 },
        { "abcdefg", "AE", 9, PR_FALSE, 0 },
        { "abcdefg", "EI", 9, PR_FALSE, 0 },
        { "abcdefg", "IO", 9, PR_FALSE, 0 },
        { "abcdefg", "BCD", 9, PR_FALSE, 0 },
        { "abcdefg", "CBD", 9, PR_FALSE, 0 },
        { "abcdefg", "DBC", 9, PR_FALSE, 0 },
        { "abcdefg", "GHI", 9, PR_FALSE, 0 },
        { "abcdefgabcdefg", "ae", 10, PR_TRUE, 0 },
        { "abcdefgabcdefg", "ei", 10, PR_TRUE, 4 },
        { "abcdefgabcdefg", "io", 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", "bcd", 10, PR_TRUE, 1 },
        { "abcdefgabcdefg", "cbd", 10, PR_TRUE, 1 },
        { "abcdefgabcdefg", "dbc", 10, PR_TRUE, 1 },
        { "abcdefgabcdefg", "ghi", 10, PR_TRUE, 6 },
        { "abcdefgabcdefg", "AE", 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", "EI", 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", "IO", 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", "BCD", 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", "CBD", 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", "DBC", 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", "GHI", 10, PR_FALSE, 0 }
    };

    int i;

    printf("Test 021 (PL_strnpbrk)    ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strnpbrk(array[i].str, array[i].chrs, array[i].max);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%s/%lu -> %.32s, not null\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].chrs ? array[i].chrs : "(null)",
                       array[i].max, rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%s/%lu -> null, not +%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].chrs ? array[i].chrs : "(null)",
                       array[i].max, array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%s/%lu -> 0x%x, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].chrs ? array[i].chrs : "(null)",
                       array[i].max, rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strnprbrk */
PRBool test_022(void)
{
    static struct
    {
        const char *str;
        const char *chrs;
        PRUint32    max;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, (const char *)0, 3, PR_FALSE, 0 },
        { (const char *)0, "abc", 3, PR_FALSE, 0 },
        { "abc", (const char *)0, 3, PR_FALSE, 0 },
        { "abcdefg", "", 3, PR_FALSE, 0 },
        { "", "aeiou", 3, PR_FALSE, 0 },
        { "abcdefg", "ae", 0, PR_FALSE, 0 },
        { "abcdefg", "ae", 1, PR_TRUE, 0 },
        { "abcdefg", "ae", 4, PR_TRUE, 0 },
        { "abcdefg", "ae", 5, PR_TRUE, 4 },
        { "abcdefg", "ae", 6, PR_TRUE,  4 },
        { "abcdefg", "ei", 4, PR_FALSE, 0 },
        { "abcdefg", "io", 10, PR_FALSE, 0 },
        { "abcdefg", "bcd", 2, PR_TRUE, 1 },
        { "abcdefg", "cbd", 2, PR_TRUE, 1 },
        { "abcdefg", "dbc", 2, PR_TRUE, 1 },
        { "abcdefg", "bcd", 3, PR_TRUE, 2 },
        { "abcdefg", "cbd", 3, PR_TRUE, 2 },
        { "abcdefg", "dbc", 3, PR_TRUE, 2 },
        { "abcdefg", "bcd", 5, PR_TRUE, 3 },
        { "abcdefg", "cbd", 5, PR_TRUE, 3 },
        { "abcdefg", "dbc", 5, PR_TRUE, 3 },
        { "abcdefg", "bcd", 15, PR_TRUE, 3 },
        { "abcdefg", "cbd", 15, PR_TRUE, 3 },
        { "abcdefg", "dbc", 15, PR_TRUE, 3 },
        { "abcdefg", "ghi", 6, PR_FALSE, 0 },
        { "abcdefg", "ghi", 7, PR_TRUE, 6 },
        { "abcdefg", "AE", 9, PR_FALSE, 0 },
        { "abcdefg", "EI", 9, PR_FALSE, 0 },
        { "abcdefg", "IO", 9, PR_FALSE, 0 },
        { "abcdefg", "BCD", 9, PR_FALSE, 0 },
        { "abcdefg", "CBD", 9, PR_FALSE, 0 },
        { "abcdefg", "DBC", 9, PR_FALSE, 0 },
        { "abcdefg", "GHI", 9, PR_FALSE, 0 },
        { "abcdefgabcdefg", "ae", 10, PR_TRUE, 7 },
        { "abcdefgabcdefg", "ei", 10, PR_TRUE, 4 },
        { "abcdefgabcdefg", "io", 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", "bcd", 10, PR_TRUE, 9 },
        { "abcdefgabcdefg", "cbd", 10, PR_TRUE, 9 },
        { "abcdefgabcdefg", "dbc", 10, PR_TRUE, 9 },
        { "abcdefgabcdefg", "ghi", 10, PR_TRUE, 6 },
        { "abcdefgabcdefg", "AE", 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", "EI", 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", "IO", 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", "BCD", 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", "CBD", 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", "DBC", 10, PR_FALSE, 0 },
        { "abcdefgabcdefg", "GHI", 10, PR_FALSE, 0 }
    };

    int i;

    printf("Test 022 (PL_strnprbrk)   ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strnprbrk(array[i].str, array[i].chrs, array[i].max);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%s/%lu -> %.32s, not null\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].chrs ? array[i].chrs : "(null)",
                       array[i].max, rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%s/%lu -> null, not +%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].chrs ? array[i].chrs : "(null)",
                       array[i].max, array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%s/%lu -> 0x%x, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].chrs ? array[i].chrs : "(null)",
                       array[i].max, rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strstr */
PRBool test_023(void)
{
    static struct
    {
        const char *str;
        const char *sub;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, (const char *)0, PR_FALSE, 0 },
        { (const char *)0, "blah", PR_FALSE, 0 },
        { "blah-de-blah", (const char *)0, PR_FALSE, 0 },
        { "blah-de-blah", "blah", PR_TRUE, 0 },
        { "", "blah", PR_FALSE, 0 },
        { "blah-de-blah", "", PR_FALSE, 0 },
        { "abcdefg", "a", PR_TRUE, 0 },
        { "abcdefg", "c", PR_TRUE, 2 },
        { "abcdefg", "e", PR_TRUE, 4 },
        { "abcdefg", "g", PR_TRUE, 6 },
        { "abcdefg", "i", PR_FALSE, 0 },
        { "abcdefg", "ab", PR_TRUE, 0 },
        { "abcdefg", "cd", PR_TRUE, 2 },
        { "abcdefg", "ef", PR_TRUE, 4 },
        { "abcdefg", "gh", PR_FALSE, 0 },
        { "abcdabc", "bc", PR_TRUE, 1 },
        { "abcdefg", "abcdefg", PR_TRUE, 0 },
        { "abcdefgabcdefg", "a", PR_TRUE, 0 },
        { "abcdefgabcdefg", "c", PR_TRUE, 2 },
        { "abcdefgabcdefg", "e", PR_TRUE, 4 },
        { "abcdefgabcdefg", "g", PR_TRUE, 6 },
        { "abcdefgabcdefg", "i", PR_FALSE, 0 },
        { "abcdefgabcdefg", "ab", PR_TRUE, 0 },
        { "abcdefgabcdefg", "cd", PR_TRUE, 2 },
        { "abcdefgabcdefg", "ef", PR_TRUE, 4 },
        { "abcdefgabcdefg", "gh", PR_FALSE, 0 },
        { "abcdabcabcdabc", "bc", PR_TRUE, 1 },
        { "abcdefgabcdefg", "abcdefg", PR_TRUE, 0 },
        { "ABCDEFG", "a", PR_FALSE, 0 },
        { "ABCDEFG", "c", PR_FALSE, 0 },
        { "ABCDEFG", "e", PR_FALSE, 0 },
        { "ABCDEFG", "g", PR_FALSE, 0 },
        { "ABCDEFG", "i", PR_FALSE, 0 },
        { "ABCDEFG", "ab", PR_FALSE, 0 },
        { "ABCDEFG", "cd", PR_FALSE, 0 },
        { "ABCDEFG", "ef", PR_FALSE, 0 },
        { "ABCDEFG", "gh", PR_FALSE, 0 },
        { "ABCDABC", "bc", PR_FALSE, 0 },
        { "ABCDEFG", "abcdefg", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "a", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "c", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "e", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "g", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "i", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "ab", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "cd", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "ef", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "gh", PR_FALSE, 0 },
        { "ABCDABCABCDABC", "bc", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "abcdefg", PR_FALSE, 0 }
    };

    int i;

    printf("Test 023 (PL_strstr)      ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strstr(array[i].str, array[i].sub);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%s -> %.32s, not null\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%s -> null, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].str, array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%s -> 0x%x, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strrstr */
PRBool test_024(void)
{
    static struct
    {
        const char *str;
        const char *sub;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, (const char *)0, PR_FALSE, 0 },
        { (const char *)0, "blah", PR_FALSE, 0 },
        { "blah-de-blah", (const char *)0, PR_FALSE, 0 },
        { "blah-de-blah", "blah", PR_TRUE, 8 },
        { "", "blah", PR_FALSE, 0 },
        { "blah-de-blah", "", PR_FALSE, 0 },
        { "abcdefg", "a", PR_TRUE, 0 },
        { "abcdefg", "c", PR_TRUE, 2 },
        { "abcdefg", "e", PR_TRUE, 4 },
        { "abcdefg", "g", PR_TRUE, 6 },
        { "abcdefg", "i", PR_FALSE, 0 },
        { "abcdefg", "ab", PR_TRUE, 0 },
        { "abcdefg", "cd", PR_TRUE, 2 },
        { "abcdefg", "ef", PR_TRUE, 4 },
        { "abcdefg", "gh", PR_FALSE, 0 },
        { "abcdabc", "bc", PR_TRUE, 5 },
        { "abcdefg", "abcdefg", PR_TRUE, 0 },
        { "abcdefgabcdefg", "a", PR_TRUE, 7 },
        { "abcdefgabcdefg", "c", PR_TRUE, 9 },
        { "abcdefgabcdefg", "e", PR_TRUE, 11 },
        { "abcdefgabcdefg", "g", PR_TRUE, 13 },
        { "abcdefgabcdefg", "i", PR_FALSE, 0 },
        { "abcdefgabcdefg", "ab", PR_TRUE, 7 },
        { "abcdefgabcdefg", "cd", PR_TRUE, 9 },
        { "abcdefgabcdefg", "ef", PR_TRUE, 11 },
        { "abcdefgabcdefg", "gh", PR_FALSE, 0 },
        { "abcdabcabcdabc", "bc", PR_TRUE, 12 },
        { "abcdefgabcdefg", "abcdefg", PR_TRUE, 7 },
        { "ABCDEFG", "a", PR_FALSE, 0 },
        { "ABCDEFG", "c", PR_FALSE, 0 },
        { "ABCDEFG", "e", PR_FALSE, 0 },
        { "ABCDEFG", "g", PR_FALSE, 0 },
        { "ABCDEFG", "i", PR_FALSE, 0 },
        { "ABCDEFG", "ab", PR_FALSE, 0 },
        { "ABCDEFG", "cd", PR_FALSE, 0 },
        { "ABCDEFG", "ef", PR_FALSE, 0 },
        { "ABCDEFG", "gh", PR_FALSE, 0 },
        { "ABCDABC", "bc", PR_FALSE, 0 },
        { "ABCDEFG", "abcdefg", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "a", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "c", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "e", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "g", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "i", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "ab", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "cd", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "ef", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "gh", PR_FALSE, 0 },
        { "ABCDABCABCDABC", "bc", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "abcdefg", PR_FALSE, 0 }
    };

    int i;

    printf("Test 024 (PL_strrstr)     ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strrstr(array[i].str, array[i].sub);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%s -> %.32s, not null\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%s -> null, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].str, array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%s -> 0x%x, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strnstr */
PRBool test_025(void)
{
    static struct
    {
        const char *str;
        const char *sub;
        PRUint32    max;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, (const char *)0, 12, PR_FALSE, 0 },
        { (const char *)0, "blah", 12, PR_FALSE, 0 },
        { "blah-de-blah", (const char *)0, 12, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 0, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 2, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 3, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 4, PR_TRUE, 0 },
        { "blah-de-blah", "blah", 5, PR_TRUE, 0 },
        { "blah-de-blah", "blah", 12, PR_TRUE, 0 },
        { "", "blah", 12, PR_FALSE, 0 },
        { "blah-de-blah", "", 12, PR_FALSE, 0 },
        { "abcdefg", "a", 5, PR_TRUE, 0 },
        { "abcdefg", "c", 5, PR_TRUE, 2 },
        { "abcdefg", "e", 5, PR_TRUE, 4 },
        { "abcdefg", "g", 5, PR_FALSE, 0 },
        { "abcdefg", "i", 5, PR_FALSE, 0 },
        { "abcdefg", "ab", 5, PR_TRUE, 0 },
        { "abcdefg", "cd", 5, PR_TRUE, 2 },
        { "abcdefg", "ef", 5, PR_FALSE, 0 },
        { "abcdefg", "gh", 5, PR_FALSE, 0 },
        { "abcdabc", "bc", 5, PR_TRUE, 1 },
        { "abcdabc", "bc", 6, PR_TRUE, 1 },
        { "abcdabc", "bc", 7, PR_TRUE, 1 },
        { "abcdefg", "abcdefg", 6, PR_FALSE, 0 },
        { "abcdefg", "abcdefg", 7, PR_TRUE, 0 },
        { "abcdefg", "abcdefg", 8, PR_TRUE, 0 },
        { "abcdefgabcdefg", "a", 12, PR_TRUE, 0 },
        { "abcdefgabcdefg", "c", 12, PR_TRUE, 2 },
        { "abcdefgabcdefg", "e", 12, PR_TRUE, 4 },
        { "abcdefgabcdefg", "g", 12, PR_TRUE, 6 },
        { "abcdefgabcdefg", "i", 12, PR_FALSE, 0 },
        { "abcdefgabcdefg", "ab", 12, PR_TRUE, 0 },
        { "abcdefgabcdefg", "cd", 12, PR_TRUE, 2 },
        { "abcdefgabcdefg", "ef", 12, PR_TRUE, 4 },
        { "abcdefgabcdefg", "gh", 12, PR_FALSE, 0 },
        { "abcdabcabcdabc", "bc", 5, PR_TRUE, 1 },
        { "abcdabcabcdabc", "bc", 6, PR_TRUE, 1 },
        { "abcdabcabcdabc", "bc", 7, PR_TRUE, 1 },
        { "abcdefgabcdefg", "abcdefg", 6, PR_FALSE, 0 },
        { "abcdefgabcdefg", "abcdefg", 7, PR_TRUE, 0 },
        { "abcdefgabcdefg", "abcdefg", 8, PR_TRUE, 0 },
        { "ABCDEFG", "a", 5, PR_FALSE, 0 },
        { "ABCDEFG", "c", 5, PR_FALSE, 0 },
        { "ABCDEFG", "e", 5, PR_FALSE, 0 },
        { "ABCDEFG", "g", 5, PR_FALSE, 0 },
        { "ABCDEFG", "i", 5, PR_FALSE, 0 },
        { "ABCDEFG", "ab", 5, PR_FALSE, 0 },
        { "ABCDEFG", "cd", 5, PR_FALSE, 0 },
        { "ABCDEFG", "ef", 5, PR_FALSE, 0 },
        { "ABCDEFG", "gh", 5, PR_FALSE, 0 },
        { "ABCDABC", "bc", 5, PR_FALSE, 0 },
        { "ABCDABC", "bc", 6, PR_FALSE, 0 },
        { "ABCDABC", "bc", 7, PR_FALSE, 0 },
        { "ABCDEFG", "abcdefg", 6, PR_FALSE, 0 },
        { "ABCDEFG", "abcdefg", 7, PR_FALSE, 0 },
        { "ABCDEFG", "abcdefg", 8, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "a", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "c", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "e", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "g", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "i", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "ab", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "cd", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "ef", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "gh", 12, PR_FALSE, 0 },
        { "ABCDABCABCDABC", "bc", 5, PR_FALSE, 0 },
        { "ABCDABCABCDABC", "bc", 6, PR_FALSE, 0 },
        { "ABCDABCABCDABC", "bc", 7, PR_FALSE,  },
        { "ABCDEFGABCDEFG", "abcdefg", 6, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "abcdefg", 7, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "abcdefg", 8, PR_FALSE, 0 }
    };

    int i;

    printf("Test 025 (PL_strnstr)     ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strnstr(array[i].str, array[i].sub, array[i].max);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%s/%lu -> %.32s, not null\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].max, rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%s/%lu -> null, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].max, array[i].str, array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%s/%lu -> 0x%x, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].max, rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strnrstr */
PRBool test_026(void)
{
    static struct
    {
        const char *str;
        const char *sub;
        PRUint32    max;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, (const char *)0, 12, PR_FALSE, 0 },
        { (const char *)0, "blah", 12, PR_FALSE, 0 },
        { "blah-de-blah", (const char *)0, 12, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 0, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 2, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 3, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 4, PR_TRUE, 0 },
        { "blah-de-blah", "blah", 5, PR_TRUE, 0 },
        { "blah-de-blah", "blah", 11, PR_TRUE, 0 },
        { "blah-de-blah", "blah", 12, PR_TRUE, 8 },
        { "blah-de-blah", "blah", 13, PR_TRUE, 8 },
        { "", "blah", 12, PR_FALSE, 0 },
        { "blah-de-blah", "", 12, PR_FALSE, 0 },
        { "abcdefg", "a", 5, PR_TRUE, 0 },
        { "abcdefg", "c", 5, PR_TRUE, 2 },
        { "abcdefg", "e", 5, PR_TRUE, 4 },
        { "abcdefg", "g", 5, PR_FALSE, 0 },
        { "abcdefg", "i", 5, PR_FALSE, 0 },
        { "abcdefg", "ab", 5, PR_TRUE, 0 },
        { "abcdefg", "cd", 5, PR_TRUE, 2 },
        { "abcdefg", "ef", 5, PR_FALSE, 0 },
        { "abcdefg", "gh", 5, PR_FALSE, 0 },
        { "abcdabc", "bc", 5, PR_TRUE, 1 },
        { "abcdabc", "bc", 6, PR_TRUE, 1 },
        { "abcdabc", "bc", 7, PR_TRUE, 5 },
        { "abcdefg", "abcdefg", 6, PR_FALSE, 0 },
        { "abcdefg", "abcdefg", 7, PR_TRUE, 0 },
        { "abcdefg", "abcdefg", 8, PR_TRUE, 0 },
        { "abcdefgabcdefg", "a", 12, PR_TRUE, 7 },
        { "abcdefgabcdefg", "c", 12, PR_TRUE, 9 },
        { "abcdefgabcdefg", "e", 12, PR_TRUE, 11 },
        { "abcdefgabcdefg", "g", 12, PR_TRUE, 6 },
        { "abcdefgabcdefg", "i", 12, PR_FALSE, 0 },
        { "abcdefgabcdefg", "ab", 12, PR_TRUE, 7 },
        { "abcdefgabcdefg", "cd", 12, PR_TRUE, 9 },
        { "abcdefgabcdefg", "ef", 12, PR_TRUE, 4 },
        { "abcdefgabcdefg", "gh", 12, PR_FALSE, 0 },
        { "abcdabcabcdabc", "bc", 12, PR_TRUE, 8 },
        { "abcdabcabcdabc", "bc", 13, PR_TRUE, 8 },
        { "abcdabcabcdabc", "bc", 14, PR_TRUE, 12 },
        { "abcdefgabcdefg", "abcdefg", 13, PR_TRUE, 0 },
        { "abcdefgabcdefg", "abcdefg", 14, PR_TRUE, 7 },
        { "abcdefgabcdefg", "abcdefg", 15, PR_TRUE, 7 },
        { "ABCDEFG", "a", 5, PR_FALSE, 0 },
        { "ABCDEFG", "c", 5, PR_FALSE, 0 },
        { "ABCDEFG", "e", 5, PR_FALSE, 0 },
        { "ABCDEFG", "g", 5, PR_FALSE, 0 },
        { "ABCDEFG", "i", 5, PR_FALSE, 0 },
        { "ABCDEFG", "ab", 5, PR_FALSE, 0 },
        { "ABCDEFG", "cd", 5, PR_FALSE, 0 },
        { "ABCDEFG", "ef", 5, PR_FALSE, 0 },
        { "ABCDEFG", "gh", 5, PR_FALSE, 0 },
        { "ABCDABC", "bc", 5, PR_FALSE, 0 },
        { "ABCDABC", "bc", 6, PR_FALSE, 0 },
        { "ABCDABC", "bc", 7, PR_FALSE, 0 },
        { "ABCDEFG", "abcdefg", 6, PR_FALSE, 0 },
        { "ABCDEFG", "abcdefg", 7, PR_FALSE, 0 },
        { "ABCDEFG", "abcdefg", 8, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "a", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "c", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "e", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "g", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "i", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "ab", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "cd", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "ef", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "gh", 12, PR_FALSE, 0 },
        { "ABCDABCABCDABC", "bc", 12, PR_FALSE, 0 },
        { "ABCDABCABCDABC", "bc", 13, PR_FALSE, 0 },
        { "ABCDABCABCDABC", "bc", 14, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "abcdefg", 13, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "abcdefg", 14, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "abcdefg", 15, PR_FALSE, 0 }
    };

    int i;

    printf("Test 026 (PL_strnrstr)    ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strnrstr(array[i].str, array[i].sub, array[i].max);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%s/%lu -> %.32s, not null\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].max, rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%s/%lu -> null, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].max, array[i].str, array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%s/%lu -> 0x%x, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].max, rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strcasestr */
PRBool test_027(void)
{
    static struct
    {
        const char *str;
        const char *sub;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, (const char *)0, PR_FALSE, 0 },
        { (const char *)0, "blah", PR_FALSE, 0 },
        { "blah-de-blah", (const char *)0, PR_FALSE, 0 },
        { "blah-de-blah", "blah", PR_TRUE, 0 },
        { "", "blah", PR_FALSE, 0 },
        { "blah-de-blah", "", PR_FALSE, 0 },
        { "abcdefg", "a", PR_TRUE, 0 },
        { "abcdefg", "c", PR_TRUE, 2 },
        { "abcdefg", "e", PR_TRUE, 4 },
        { "abcdefg", "g", PR_TRUE, 6 },
        { "abcdefg", "i", PR_FALSE, 0 },
        { "abcdefg", "ab", PR_TRUE, 0 },
        { "abcdefg", "cd", PR_TRUE, 2 },
        { "abcdefg", "ef", PR_TRUE, 4 },
        { "abcdefg", "gh", PR_FALSE, 0 },
        { "abcdabc", "bc", PR_TRUE, 1 },
        { "abcdefg", "abcdefg", PR_TRUE, 0 },
        { "abcdefgabcdefg", "a", PR_TRUE, 0 },
        { "abcdefgabcdefg", "c", PR_TRUE, 2 },
        { "abcdefgabcdefg", "e", PR_TRUE, 4 },
        { "abcdefgabcdefg", "g", PR_TRUE, 6 },
        { "abcdefgabcdefg", "i", PR_FALSE, 0 },
        { "abcdefgabcdefg", "ab", PR_TRUE, 0 },
        { "abcdefgabcdefg", "cd", PR_TRUE, 2 },
        { "abcdefgabcdefg", "ef", PR_TRUE, 4 },
        { "abcdefgabcdefg", "gh", PR_FALSE, 0 },
        { "abcdabcabcdabc", "bc", PR_TRUE, 1 },
        { "abcdefgabcdefg", "abcdefg", PR_TRUE, 0 },
        { "ABCDEFG", "a", PR_TRUE, 0 },
        { "ABCDEFG", "c", PR_TRUE, 2 },
        { "ABCDEFG", "e", PR_TRUE, 4 },
        { "ABCDEFG", "g", PR_TRUE, 6 },
        { "ABCDEFG", "i", PR_FALSE, 0 },
        { "ABCDEFG", "ab", PR_TRUE, 0 },
        { "ABCDEFG", "cd", PR_TRUE, 2 },
        { "ABCDEFG", "ef", PR_TRUE, 4 },
        { "ABCDEFG", "gh", PR_FALSE, 0 },
        { "ABCDABC", "bc", PR_TRUE, 1 },
        { "ABCDEFG", "abcdefg", PR_TRUE, 0 },
        { "ABCDEFGABCDEFG", "a", PR_TRUE, 0 },
        { "ABCDEFGABCDEFG", "c", PR_TRUE, 2 },
        { "ABCDEFGABCDEFG", "e", PR_TRUE, 4 },
        { "ABCDEFGABCDEFG", "g", PR_TRUE, 6 },
        { "ABCDEFGABCDEFG", "i", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "ab", PR_TRUE, 0 },
        { "ABCDEFGABCDEFG", "cd", PR_TRUE, 2 },
        { "ABCDEFGABCDEFG", "ef", PR_TRUE, 4 },
        { "ABCDEFGABCDEFG", "gh", PR_FALSE, 0 },
        { "ABCDABCABCDABC", "bc", PR_TRUE, 1 },
        { "ABCDEFGABCDEFG", "abcdefg", PR_TRUE, 0 }
    };

    int i;

    printf("Test 027 (PL_strcasestr)  ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strcasestr(array[i].str, array[i].sub);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%s -> %.32s, not null\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%s -> null, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].str, array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%s -> 0x%x, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strcaserstr */
PRBool test_028(void)
{
    static struct
    {
        const char *str;
        const char *sub;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, (const char *)0, PR_FALSE, 0 },
        { (const char *)0, "blah", PR_FALSE, 0 },
        { "blah-de-blah", (const char *)0, PR_FALSE, 0 },
        { "blah-de-blah", "blah", PR_TRUE, 8 },
        { "", "blah", PR_FALSE, 0 },
        { "blah-de-blah", "", PR_FALSE, 0 },
        { "abcdefg", "a", PR_TRUE, 0 },
        { "abcdefg", "c", PR_TRUE, 2 },
        { "abcdefg", "e", PR_TRUE, 4 },
        { "abcdefg", "g", PR_TRUE, 6 },
        { "abcdefg", "i", PR_FALSE, 0 },
        { "abcdefg", "ab", PR_TRUE, 0 },
        { "abcdefg", "cd", PR_TRUE, 2 },
        { "abcdefg", "ef", PR_TRUE, 4 },
        { "abcdefg", "gh", PR_FALSE, 0 },
        { "abcdabc", "bc", PR_TRUE, 5 },
        { "abcdefg", "abcdefg", PR_TRUE, 0 },
        { "abcdefgabcdefg", "a", PR_TRUE, 7 },
        { "abcdefgabcdefg", "c", PR_TRUE, 9 },
        { "abcdefgabcdefg", "e", PR_TRUE, 11 },
        { "abcdefgabcdefg", "g", PR_TRUE, 13 },
        { "abcdefgabcdefg", "i", PR_FALSE, 0 },
        { "abcdefgabcdefg", "ab", PR_TRUE, 7 },
        { "abcdefgabcdefg", "cd", PR_TRUE, 9 },
        { "abcdefgabcdefg", "ef", PR_TRUE, 11 },
        { "abcdefgabcdefg", "gh", PR_FALSE, 0 },
        { "abcdabcabcdabc", "bc", PR_TRUE, 12 },
        { "abcdefgabcdefg", "abcdefg", PR_TRUE, 7 },
        { "ABCDEFG", "a", PR_TRUE, 0 },
        { "ABCDEFG", "c", PR_TRUE, 2 },
        { "ABCDEFG", "e", PR_TRUE, 4 },
        { "ABCDEFG", "g", PR_TRUE, 6 },
        { "ABCDEFG", "i", PR_FALSE, 0 },
        { "ABCDEFG", "ab", PR_TRUE, 0 },
        { "ABCDEFG", "cd", PR_TRUE, 2 },
        { "ABCDEFG", "ef", PR_TRUE, 4 },
        { "ABCDEFG", "gh", PR_FALSE, 0 },
        { "ABCDABC", "bc", PR_TRUE, 5 },
        { "ABCDEFG", "abcdefg", PR_TRUE, 0 },
        { "ABCDEFGABCDEFG", "a", PR_TRUE, 7 },
        { "ABCDEFGABCDEFG", "c", PR_TRUE, 9 },
        { "ABCDEFGABCDEFG", "e", PR_TRUE, 11 },
        { "ABCDEFGABCDEFG", "g", PR_TRUE, 13 },
        { "ABCDEFGABCDEFG", "i", PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "ab", PR_TRUE, 7 },
        { "ABCDEFGABCDEFG", "cd", PR_TRUE, 9 },
        { "ABCDEFGABCDEFG", "ef", PR_TRUE, 11 },
        { "ABCDEFGABCDEFG", "gh", PR_FALSE, 0 },
        { "ABCDABCABCDABC", "bc", PR_TRUE, 12 },
        { "ABCDEFGABCDEFG", "abcdefg", PR_TRUE, 7 }
    };

    int i;

    printf("Test 028 (PL_strcaserstr) ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strcaserstr(array[i].str, array[i].sub);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%s -> %.32s, not null\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%s -> null, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].str, array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%s -> 0x%x, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strncasestr */
PRBool test_029(void)
{
    static struct
    {
        const char *str;
        const char *sub;
        PRUint32    max;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, (const char *)0, 12, PR_FALSE, 0 },
        { (const char *)0, "blah", 12, PR_FALSE, 0 },
        { "blah-de-blah", (const char *)0, 12, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 0, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 2, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 3, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 4, PR_TRUE, 0 },
        { "blah-de-blah", "blah", 5, PR_TRUE, 0 },
        { "blah-de-blah", "blah", 12, PR_TRUE, 0 },
        { "", "blah", 12, PR_FALSE, 0 },
        { "blah-de-blah", "", 12, PR_FALSE, 0 },
        { "abcdefg", "a", 5, PR_TRUE, 0 },
        { "abcdefg", "c", 5, PR_TRUE, 2 },
        { "abcdefg", "e", 5, PR_TRUE, 4 },
        { "abcdefg", "g", 5, PR_FALSE, 0 },
        { "abcdefg", "i", 5, PR_FALSE, 0 },
        { "abcdefg", "ab", 5, PR_TRUE, 0 },
        { "abcdefg", "cd", 5, PR_TRUE, 2 },
        { "abcdefg", "ef", 5, PR_FALSE, 0 },
        { "abcdefg", "gh", 5, PR_FALSE, 0 },
        { "abcdabc", "bc", 5, PR_TRUE, 1 },
        { "abcdabc", "bc", 6, PR_TRUE, 1 },
        { "abcdabc", "bc", 7, PR_TRUE, 1 },
        { "abcdefg", "abcdefg", 6, PR_FALSE, 0 },
        { "abcdefg", "abcdefg", 7, PR_TRUE, 0 },
        { "abcdefg", "abcdefg", 8, PR_TRUE, 0 },
        { "abcdefgabcdefg", "a", 12, PR_TRUE, 0 },
        { "abcdefgabcdefg", "c", 12, PR_TRUE, 2 },
        { "abcdefgabcdefg", "e", 12, PR_TRUE, 4 },
        { "abcdefgabcdefg", "g", 12, PR_TRUE, 6 },
        { "abcdefgabcdefg", "i", 12, PR_FALSE, 0 },
        { "abcdefgabcdefg", "ab", 12, PR_TRUE, 0 },
        { "abcdefgabcdefg", "cd", 12, PR_TRUE, 2 },
        { "abcdefgabcdefg", "ef", 12, PR_TRUE, 4 },
        { "abcdefgabcdefg", "gh", 12, PR_FALSE, 0 },
        { "abcdabcabcdabc", "bc", 5, PR_TRUE, 1 },
        { "abcdabcabcdabc", "bc", 6, PR_TRUE, 1 },
        { "abcdabcabcdabc", "bc", 7, PR_TRUE, 1 },
        { "abcdefgabcdefg", "abcdefg", 6, PR_FALSE, 0 },
        { "abcdefgabcdefg", "abcdefg", 7, PR_TRUE, 0 },
        { "abcdefgabcdefg", "abcdefg", 8, PR_TRUE, 0 },
        { "ABCDEFG", "a", 5, PR_TRUE, 0 },
        { "ABCDEFG", "c", 5, PR_TRUE, 2 },
        { "ABCDEFG", "e", 5, PR_TRUE, 4 },
        { "ABCDEFG", "g", 5, PR_FALSE, 0 },
        { "ABCDEFG", "i", 5, PR_FALSE, 0 },
        { "ABCDEFG", "ab", 5, PR_TRUE, 0 },
        { "ABCDEFG", "cd", 5, PR_TRUE, 2 },
        { "ABCDEFG", "ef", 5, PR_FALSE, 0 },
        { "ABCDEFG", "gh", 5, PR_FALSE, 0 },
        { "ABCDABC", "bc", 5, PR_TRUE, 1 },
        { "ABCDABC", "bc", 6, PR_TRUE, 1 },
        { "ABCDABC", "bc", 7, PR_TRUE, 1 },
        { "ABCDEFG", "abcdefg", 6, PR_FALSE, 0 },
        { "ABCDEFG", "abcdefg", 7, PR_TRUE, 0 },
        { "ABCDEFG", "abcdefg", 8, PR_TRUE, 0 },
        { "ABCDEFGABCDEFG", "a", 12, PR_TRUE, 0 },
        { "ABCDEFGABCDEFG", "c", 12, PR_TRUE, 2 },
        { "ABCDEFGABCDEFG", "e", 12, PR_TRUE, 4 },
        { "ABCDEFGABCDEFG", "g", 12, PR_TRUE, 6 },
        { "ABCDEFGABCDEFG", "i", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "ab", 12, PR_TRUE, 0 },
        { "ABCDEFGABCDEFG", "cd", 12, PR_TRUE, 2 },
        { "ABCDEFGABCDEFG", "ef", 12, PR_TRUE, 4 },
        { "ABCDEFGABCDEFG", "gh", 12, PR_FALSE, 0 },
        { "ABCDABCABCDABC", "bc", 5, PR_TRUE, 1 },
        { "ABCDABCABCDABC", "bc", 6, PR_TRUE, 1 },
        { "ABCDABCABCDABC", "bc", 7, PR_TRUE, 1 },
        { "ABCDEFGABCDEFG", "abcdefg", 6, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "abcdefg", 7, PR_TRUE, 0 },
        { "ABCDEFGABCDEFG", "abcdefg", 8, PR_TRUE, 0 }
    };

    int i;

    printf("Test 029 (PL_strncasestr) ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strncasestr(array[i].str, array[i].sub, array[i].max);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%s/%lu -> %.32s, not null\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].max, rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%s/%lu -> null, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].max, array[i].str, array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%s/%lu -> 0x%x, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].max, rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strncaserstr */
PRBool test_030(void)
{
    static struct
    {
        const char *str;
        const char *sub;
        PRUint32    max;
        PRBool      ret;
        PRUint32    off;
    } array[] =
    {
        { (const char *)0, (const char *)0, 12, PR_FALSE, 0 },
        { (const char *)0, "blah", 12, PR_FALSE, 0 },
        { "blah-de-blah", (const char *)0, 12, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 0, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 2, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 3, PR_FALSE, 0 },
        { "blah-de-blah", "blah", 4, PR_TRUE, 0 },
        { "blah-de-blah", "blah", 5, PR_TRUE, 0 },
        { "blah-de-blah", "blah", 11, PR_TRUE, 0 },
        { "blah-de-blah", "blah", 12, PR_TRUE, 8 },
        { "blah-de-blah", "blah", 13, PR_TRUE, 8 },
        { "", "blah", 12, PR_FALSE, 0 },
        { "blah-de-blah", "", 12, PR_FALSE, 0 },
        { "abcdefg", "a", 5, PR_TRUE, 0 },
        { "abcdefg", "c", 5, PR_TRUE, 2 },
        { "abcdefg", "e", 5, PR_TRUE, 4 },
        { "abcdefg", "g", 5, PR_FALSE, 0 },
        { "abcdefg", "i", 5, PR_FALSE, 0 },
        { "abcdefg", "ab", 5, PR_TRUE, 0 },
        { "abcdefg", "cd", 5, PR_TRUE, 2 },
        { "abcdefg", "ef", 5, PR_FALSE, 0 },
        { "abcdefg", "gh", 5, PR_FALSE, 0 },
        { "abcdabc", "bc", 5, PR_TRUE, 1 },
        { "abcdabc", "bc", 6, PR_TRUE, 1 },
        { "abcdabc", "bc", 7, PR_TRUE, 5 },
        { "abcdefg", "abcdefg", 6, PR_FALSE, 0 },
        { "abcdefg", "abcdefg", 7, PR_TRUE, 0 },
        { "abcdefg", "abcdefg", 8, PR_TRUE, 0 },
        { "abcdefgabcdefg", "a", 12, PR_TRUE, 7 },
        { "abcdefgabcdefg", "c", 12, PR_TRUE, 9 },
        { "abcdefgabcdefg", "e", 12, PR_TRUE, 11 },
        { "abcdefgabcdefg", "g", 12, PR_TRUE, 6 },
        { "abcdefgabcdefg", "i", 12, PR_FALSE, 0 },
        { "abcdefgabcdefg", "ab", 12, PR_TRUE, 7 },
        { "abcdefgabcdefg", "cd", 12, PR_TRUE, 9 },
        { "abcdefgabcdefg", "ef", 12, PR_TRUE, 4 },
        { "abcdefgabcdefg", "gh", 12, PR_FALSE, 0 },
        { "abcdabcabcdabc", "bc", 12, PR_TRUE, 8 },
        { "abcdabcabcdabc", "bc", 13, PR_TRUE, 8 },
        { "abcdabcabcdabc", "bc", 14, PR_TRUE, 12 },
        { "abcdefgabcdefg", "abcdefg", 13, PR_TRUE, 0 },
        { "abcdefgabcdefg", "abcdefg", 14, PR_TRUE, 7 },
        { "abcdefgabcdefg", "abcdefg", 15, PR_TRUE, 7 },
        { "ABCDEFG", "a", 5, PR_TRUE, 0 },
        { "ABCDEFG", "c", 5, PR_TRUE, 2 },
        { "ABCDEFG", "e", 5, PR_TRUE, 4 },
        { "ABCDEFG", "g", 5, PR_FALSE, 0 },
        { "ABCDEFG", "i", 5, PR_FALSE, 0 },
        { "ABCDEFG", "ab", 5, PR_TRUE, 0 },
        { "ABCDEFG", "cd", 5, PR_TRUE, 2 },
        { "ABCDEFG", "ef", 5, PR_FALSE, 0 },
        { "ABCDEFG", "gh", 5, PR_FALSE, 0 },
        { "ABCDABC", "bc", 5, PR_TRUE, 1 },
        { "ABCDABC", "bc", 6, PR_TRUE, 1 },
        { "ABCDABC", "bc", 7, PR_TRUE, 5 },
        { "ABCDEFG", "abcdefg", 6, PR_FALSE, 0 },
        { "ABCDEFG", "abcdefg", 7, PR_TRUE, 0 },
        { "ABCDEFG", "abcdefg", 8, PR_TRUE, 0 },
        { "ABCDEFGABCDEFG", "a", 12, PR_TRUE, 7 },
        { "ABCDEFGABCDEFG", "c", 12, PR_TRUE, 9 },
        { "ABCDEFGABCDEFG", "e", 12, PR_TRUE, 11 },
        { "ABCDEFGABCDEFG", "g", 12, PR_TRUE, 6 },
        { "ABCDEFGABCDEFG", "i", 12, PR_FALSE, 0 },
        { "ABCDEFGABCDEFG", "ab", 12, PR_TRUE, 7 },
        { "ABCDEFGABCDEFG", "cd", 12, PR_TRUE, 9 },
        { "ABCDEFGABCDEFG", "ef", 12, PR_TRUE, 4 },
        { "ABCDEFGABCDEFG", "gh", 12, PR_FALSE, 0 },
        { "ABCDABCABCDABC", "bc", 12, PR_TRUE, 8 },
        { "ABCDABCABCDABC", "bc", 13, PR_TRUE, 8 },
        { "ABCDABCABCDABC", "bc", 14, PR_TRUE, 12 },
        { "ABCDEFGABCDEFG", "abcdefg", 13, PR_TRUE, 0 },
        { "ABCDEFGABCDEFG", "abcdefg", 14, PR_TRUE, 7 },
        { "ABCDEFGABCDEFG", "abcdefg", 15, PR_TRUE, 7 }
    };

    int i;

    printf("Test 030 (PL_strncaserstr)..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        char *rv = PL_strncaserstr(array[i].str, array[i].sub, array[i].max);

        if( PR_FALSE == array[i].ret )
        {
            if( (char *)0 != rv )
            {
                printf("FAIL %d: %s,%s/%lu -> %.32s, not null\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].max, rv);
                return PR_FALSE;
            }
        }
        else
        {
            if( (char *)0 == rv )
            {
                printf("FAIL %d: %s,%s/%lu -> null, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].max, array[i].str, array[i].off);
                return PR_FALSE;
            }

            if( &array[i].str[ array[i].off ] != rv )
            {
                printf("FAIL %d: %s,%s/%lu -> 0x%x, not 0x%x+%lu\n", i,
                       array[i].str ? array[i].str : "(null)",
                       array[i].sub ? array[i].sub : "(null)",
                       array[i].max, rv, array[i].str, array[i].off);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_strtok_r */
PRBool test_031(void)
{
    static const char *tokens[] = {
        "wtc", "relyea", "nelsonb", "jpierre", "nicolson",
        "ian.mcgreer", "kirk.erickson", "sonja.mirtitsch", "mhein"
    };

    static const char *seps[] = {
        ", ", ",", " ", "\t", ",,,", " ,", "    ", " \t\t", ","
    };

    static const char s2[] = ", \t";

    char string[ 1024 ];
    char *s1;
    char *token;
    char *lasts;
    unsigned int i;

    printf("Test 031 (PL_strtok_r)    ..."); fflush(stdout);

    /* Build the string. */
    string[0] = '\0';
    for( i = 0; i < sizeof(tokens)/sizeof(tokens[0]); i++ )
    {
        PL_strcat(string, tokens[i]);
        PL_strcat(string, seps[i]);
    }

    /* Scan the string for tokens. */
    i = 0;
    s1 = string;
    while( (token = PL_strtok_r(s1, s2, &lasts)) != NULL)
    {
        if( PL_strcmp(token, tokens[i]) != 0 )
        {
            printf("FAIL wrong token scanned\n");
            return PR_FALSE;
        }
        i++;
        s1 = NULL;
    }
    if( i != sizeof(tokens)/sizeof(tokens[0]) )
    {
        printf("FAIL wrong number of tokens scanned\n");
        return PR_FALSE;
    }

    printf("PASS\n");
    return PR_TRUE;
}

int
main
(
    int     argc,
    char   *argv[]
)
{
    printf("Testing the Portable Library string functions:\n");

    if( 1
        && test_001()
        && test_001()
        && test_002()
        && test_003()
        && test_004()
        && test_005()
        && test_006()
        && test_007()
        && test_008()
        && test_009()
        && test_010()
        && test_011()
        && test_012()
        && test_013()
        && test_014()
        && test_015()
        && test_016()
        && test_017()
        && test_018()
        && test_019()
        && test_020()
        && test_021()
        && test_022()
        && test_023()
        && test_024()
        && test_025()
        && test_026()
        && test_027()
        && test_028()
        && test_029()
        && test_030()
        && test_031()
      )
    {
        printf("Suite passed.\n");
        return 0;
    }
    else
    {
        printf("Suite failed.\n");
        return 1;
    }

    /*NOTREACHED*/
}
