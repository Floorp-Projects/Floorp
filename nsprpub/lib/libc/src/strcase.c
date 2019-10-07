/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "plstr.h"
#include <string.h>

static const unsigned char uc[] =
{
    '\000', '\001', '\002', '\003', '\004', '\005', '\006', '\007',
    '\010', '\011', '\012', '\013', '\014', '\015', '\016', '\017',
    '\020', '\021', '\022', '\023', '\024', '\025', '\026', '\027',
    '\030', '\031', '\032', '\033', '\034', '\035', '\036', '\037',
    ' ',    '!',    '"',    '#',    '$',    '%',    '&',    '\'',
    '(',    ')',    '*',    '+',    ',',    '-',    '.',    '/',
    '0',    '1',    '2',    '3',    '4',    '5',    '6',    '7',
    '8',    '9',    ':',    ';',    '<',    '=',    '>',    '?',
    '@',    'A',    'B',    'C',    'D',    'E',    'F',    'G',
    'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O',
    'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',
    'X',    'Y',    'Z',    '[',    '\\',   ']',    '^',    '_',
    '`',    'A',    'B',    'C',    'D',    'E',    'F',    'G',
    'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O',
    'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',
    'X',    'Y',    'Z',    '{',    '|',    '}',    '~',    '\177',
    0200,   0201,   0202,   0203,   0204,   0205,   0206,   0207,
    0210,   0211,   0212,   0213,   0214,   0215,   0216,   0217,
    0220,   0221,   0222,   0223,   0224,   0225,   0226,   0227,
    0230,   0231,   0232,   0233,   0234,   0235,   0236,   0237,
    0240,   0241,   0242,   0243,   0244,   0245,   0246,   0247,
    0250,   0251,   0252,   0253,   0254,   0255,   0256,   0257,
    0260,   0261,   0262,   0263,   0264,   0265,   0266,   0267,
    0270,   0271,   0272,   0273,   0274,   0275,   0276,   0277,
    0300,   0301,   0302,   0303,   0304,   0305,   0306,   0307,
    0310,   0311,   0312,   0313,   0314,   0315,   0316,   0317,
    0320,   0321,   0322,   0323,   0324,   0325,   0326,   0327,
    0330,   0331,   0332,   0333,   0334,   0335,   0336,   0337,
    0340,   0341,   0342,   0343,   0344,   0345,   0346,   0347,
    0350,   0351,   0352,   0353,   0354,   0355,   0356,   0357,
    0360,   0361,   0362,   0363,   0364,   0365,   0366,   0367,
    0370,   0371,   0372,   0373,   0374,   0375,   0376,   0377
};

PR_IMPLEMENT(PRIntn)
PL_strcasecmp(const char *a, const char *b)
{
    const unsigned char *ua = (const unsigned char *)a;
    const unsigned char *ub = (const unsigned char *)b;

    if( (const char *)0 == a ) {
        return ((const char *)0 == b) ? 0 : -1;
    }
    if( (const char *)0 == b ) {
        return 1;
    }

    while( (uc[*ua] == uc[*ub]) && ('\0' != *a) )
    {
        a++;
        ua++;
        ub++;
    }

    return (PRIntn)(uc[*ua] - uc[*ub]);
}

PR_IMPLEMENT(PRIntn)
PL_strncasecmp(const char *a, const char *b, PRUint32 max)
{
    const unsigned char *ua = (const unsigned char *)a;
    const unsigned char *ub = (const unsigned char *)b;

    if( (const char *)0 == a ) {
        return ((const char *)0 == b) ? 0 : -1;
    }
    if( (const char *)0 == b ) {
        return 1;
    }

    while( max && (uc[*ua] == uc[*ub]) && ('\0' != *a) )
    {
        a++;
        ua++;
        ub++;
        max--;
    }

    if( 0 == max ) {
        return (PRIntn)0;
    }

    return (PRIntn)(uc[*ua] - uc[*ub]);
}

PR_IMPLEMENT(char *)
PL_strcasestr(const char *big, const char *little)
{
    PRUint32 ll;

    if( ((const char *)0 == big) || ((const char *)0 == little) ) {
        return (char *)0;
    }
    if( ((char)0 == *big) || ((char)0 == *little) ) {
        return (char *)0;
    }

    ll = strlen(little);

    for( ; *big; big++ )
        /* obvious improvement available here */
        if( 0 == PL_strncasecmp(big, little, ll) ) {
            return (char *)big;
        }

    return (char *)0;
}

PR_IMPLEMENT(char *)
PL_strcaserstr(const char *big, const char *little)
{
    const char *p;
    PRUint32 bl, ll;

    if( ((const char *)0 == big) || ((const char *)0 == little) ) {
        return (char *)0;
    }
    if( ((char)0 == *big) || ((char)0 == *little) ) {
        return (char *)0;
    }

    bl = strlen(big);
    ll = strlen(little);
    if( bl < ll ) {
        return (char *)0;
    }
    p = &big[ bl - ll ];

    for( ; p >= big; p-- )
        /* obvious improvement available here */
        if( 0 == PL_strncasecmp(p, little, ll) ) {
            return (char *)p;
        }

    return (char *)0;
}

PR_IMPLEMENT(char *)
PL_strncasestr(const char *big, const char *little, PRUint32 max)
{
    PRUint32 ll;

    if( ((const char *)0 == big) || ((const char *)0 == little) ) {
        return (char *)0;
    }
    if( ((char)0 == *big) || ((char)0 == *little) ) {
        return (char *)0;
    }

    ll = strlen(little);
    if( ll > max ) {
        return (char *)0;
    }
    max -= ll;
    max++;

    for( ; max && *big; big++, max-- )
        /* obvious improvement available here */
        if( 0 == PL_strncasecmp(big, little, ll) ) {
            return (char *)big;
        }

    return (char *)0;
}

PR_IMPLEMENT(char *)
PL_strncaserstr(const char *big, const char *little, PRUint32 max)
{
    const char *p;
    PRUint32 ll;

    if( ((const char *)0 == big) || ((const char *)0 == little) ) {
        return (char *)0;
    }
    if( ((char)0 == *big) || ((char)0 == *little) ) {
        return (char *)0;
    }

    ll = strlen(little);

    for( p = big; max && *p; p++, max-- )
        ;

    p -= ll;
    if( p < big ) {
        return (char *)0;
    }

    for( ; p >= big; p-- )
        /* obvious improvement available here */
        if( 0 == PL_strncasecmp(p, little, ll) ) {
            return (char *)p;
        }

    return (char *)0;
}
