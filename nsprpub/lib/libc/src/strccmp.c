/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "plstr.h"

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

    if( ((const char *)0 == a) || (const char *)0 == b ) 
        return (PRIntn)(a-b);

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

    if( ((const char *)0 == a) || (const char *)0 == b ) 
        return (PRIntn)(a-b);

    while( max && (uc[*ua] == uc[*ub]) && ('\0' != *a) )
    {
        a++;
        ua++;
        ub++;
        max--;
    }

    if( 0 == max ) return (PRIntn)0;

    return (PRIntn)(uc[*ua] - uc[*ub]);
}
