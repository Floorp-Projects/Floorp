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

#include "plbase64.h"
#include "plstr.h"
#include "nspr.h"

#include <stdio.h>

static unsigned char *base = (unsigned char *)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* PL_Base64Encode, single characters */
PRBool test_001(void)
{
    PRUint32 a, b;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char result[ 8 ];
    char *rv;

    printf("Test 001 (PL_Base64Encode, single characters)                         ..."); fflush(stdout);

    plain[1] = plain[2] = plain[3] = (unsigned char)0;
    cypher[2] = cypher[3] = (unsigned char)'=';
    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];

        for( b = 0; b < 4; b++ )
        {
            plain[0] = (unsigned char)(a * 4 + b);
            cypher[1] = base[(b * 16)];

            rv = PL_Base64Encode((char *)plain, 1, result);
            if( rv != result )
            {
                printf("FAIL\n\t(%d, %d): return value\n", a, b);
                return PR_FALSE;
            }

            if( 0 != PL_strncmp((char *)cypher, result, 4) )
            {
                printf("FAIL\n\t(%d, %d): expected \"%s,\" got \"%.4s.\"\n",
                       a, b, cypher, result);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Encode, double characters */
PRBool test_002(void)
{
    PRUint32 a, b, c, d;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char result[ 8 ];
    char *rv;

    printf("Test 002 (PL_Base64Encode, double characters)                         ..."); fflush(stdout);

    plain[2] = plain[3] = (unsigned char)0;
    cypher[3] = (unsigned char)'=';
    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];
        for( b = 0; b < 4; b++ )
        {
            plain[0] = (a*4) + b;
            for( c = 0; c < 16; c++ )
            {
                cypher[1] = base[b*16 + c];
                for( d = 0; d < 16; d++ )
                {
                    plain[1] = c*16 + d;
                    cypher[2] = base[d*4];

                    rv = PL_Base64Encode((char *)plain, 2, result);
                    if( rv != result )
                    {
                        printf("FAIL\n\t(%d, %d, %d, %d): return value\n", a, b, c, d);
                        return PR_FALSE;
                    }

                    if( 0 != PL_strncmp((char *)cypher, result, 4) )
                    {
                        printf("FAIL\n\t(%d, %d, %d, %d): expected \"%s,\" got \"%.4s.\"\n",
                               a, b, c, d, cypher, result);
                        return PR_FALSE;
                    }
                }
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Encode, triple characters */
PRBool test_003(void)
{
    PRUint32 a, b, c, d, e, f;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char result[ 8 ];
    char *rv;

    printf("Test 003 (PL_Base64Encode, triple characters)                         ..."); fflush(stdout);

    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];
        for( b = 0; b < 4; b++ )
        {
            plain[0] = (a*4) + b;
            for( c = 0; c < 16; c++ )
            {
                cypher[1] = base[b*16 + c];
                for( d = 0; d < 16; d++ )
                {
                    plain[1] = c*16 + d;
                    for( e = 0; e < 4; e++ )
                    {
                        cypher[2] = base[d*4 + e];
                        for( f = 0; f < 64; f++ )
                        {
                            plain[2] = e * 64 + f;
                            cypher[3] = base[f];

                            rv = PL_Base64Encode((char *)plain, 3, result);
                            if( rv != result )
                            {
                                printf("FAIL\n\t(%d, %d, %d, %d, %d, %d): return value\n", a, b, c, d, e, f);
                                return PR_FALSE;
                            }

                            if( 0 != PL_strncmp((char *)cypher, result, 4) )
                            {
                                printf("FAIL\n\t(%d, %d, %d, %d, %d, %d): expected \"%s,\" got \"%.4s.\"\n",
                                       a, b, c, d, e, f, cypher, result);
                                return PR_FALSE;
                            }
                        }
                    }
                }
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

    static struct
    {
        const char *plaintext;
        const char *cyphertext;
    } array[] =
      {
          /* Cyphertexts generated with uuenview 0.5.13 */
          { " ", "IA==" },
          { ".", "Lg==" },
          { "/", "Lw==" },
          { "C", "Qw==" },
          { "H", "SA==" },
          { "S", "Uw==" },
          { "^", "Xg==" },
          { "a", "YQ==" },
          { "o", "bw==" },
          { "t", "dA==" },

          { "AB", "QUI=" },
          { "AH", "QUg=" },
          { "AQ", "QVE=" },
          { "BD", "QkQ=" },
          { "CR", "Q1I=" },
          { "CS", "Q1M=" },
          { "DB", "REI=" },
          { "DC", "REM=" },
          { "EK", "RUs=" },
          { "ET", "RVQ=" },
          { "IM", "SU0=" },
          { "JR", "SlI=" },
          { "LO", "TE8=" },
          { "LW", "TFc=" },
          { "ML", "TUw=" },
          { "SB", "U0I=" },
          { "TO", "VE8=" },
          { "VS", "VlM=" },
          { "WP", "V1A=" },
          /* legitimate two-letter words */
          { "ad", "YWQ=" },
          { "ah", "YWg=" },
          { "am", "YW0=" },
          { "an", "YW4=" },
          { "as", "YXM=" },
          { "at", "YXQ=" },
          { "ax", "YXg=" },
          { "be", "YmU=" },
          { "by", "Ynk=" },
          { "do", "ZG8=" },
          { "go", "Z28=" },
          { "he", "aGU=" },
          { "hi", "aGk=" },
          { "if", "aWY=" },
          { "in", "aW4=" },
          { "is", "aXM=" },
          { "it", "aXQ=" },
          { "me", "bWU=" },
          { "my", "bXk=" },
          { "no", "bm8=" },
          { "of", "b2Y=" },
          { "on", "b24=" },
          { "or", "b3I=" },
          { "ox", "b3g=" },
          { "so", "c28=" },
          { "to", "dG8=" },
          { "up", "dXA=" },
          { "us", "dXM=" },
          { "we", "d2U=" },
          /* all three-letter entries in /usr/dict/words */
          { "1st", "MXN0" },
          { "2nd", "Mm5k" },
          { "3rd", "M3Jk" },
          { "4th", "NHRo" },
          { "5th", "NXRo" },
          { "6th", "NnRo" },
          { "7th", "N3Ro" },
          { "8th", "OHRo" },
          { "9th", "OXRo" },
          { "AAA", "QUFB" },
          { "AAU", "QUFV" },
          { "ABA", "QUJB" },
          { "abc", "YWJj" },
          { "Abe", "QWJl" },
          { "Abo", "QWJv" },
          { "ace", "YWNl" },
          { "ACM", "QUNN" },
          { "ACS", "QUNT" },
          { "act", "YWN0" },
          { "Ada", "QWRh" },
          { "add", "YWRk" },
          { "ado", "YWRv" },
          { "aft", "YWZ0" },
          { "age", "YWdl" },
          { "ago", "YWdv" },
          { "aid", "YWlk" },
          { "ail", "YWls" },
          { "aim", "YWlt" },
          { "air", "YWly" },
          { "ala", "YWxh" },
          { "alb", "YWxi" },
          { "ale", "YWxl" },
          { "Ali", "QWxp" },
          { "all", "YWxs" },
          { "alp", "YWxw" },
          { "A&M", "QSZN" },
          { "AMA", "QU1B" },
          { "ami", "YW1p" },
          { "amp", "YW1w" },
          { "Amy", "QW15" },
          { "amy", "YW15" },
          { "ana", "YW5h" },
          { "and", "YW5k" },
          { "ani", "YW5p" },
          { "Ann", "QW5u" },
          { "ant", "YW50" },
          { "any", "YW55" },
          { "A&P", "QSZQ" },
          { "ape", "YXBl" },
          { "Apr", "QXBy" },
          { "APS", "QVBT" },
          { "apt", "YXB0" },
          { "arc", "YXJj" },
          { "are", "YXJl" },
          { "ark", "YXJr" },
          { "arm", "YXJt" },
          { "art", "YXJ0" },
          { "a's", "YSdz" },
          { "ash", "YXNo" },
          { "ask", "YXNr" },
          { "ass", "YXNz" },
          { "ate", "YXRl" },
          { "Aug", "QXVn" },
          { "auk", "YXVr" },
          { "Ave", "QXZl" },
          { "awe", "YXdl" },
          { "awl", "YXds" },
          { "awn", "YXdu" },
          { "axe", "YXhl" },
          { "aye", "YXll" },
          { "bad", "YmFk" },
          { "bag", "YmFn" },
          { "bah", "YmFo" },
          { "bam", "YmFt" },
          { "ban", "YmFu" },
          { "bar", "YmFy" },
          { "bat", "YmF0" },
          { "bay", "YmF5" },
          { "bed", "YmVk" },
          { "bee", "YmVl" },
          { "beg", "YmVn" },
          { "bel", "YmVs" },
          { "Ben", "QmVu" },
          { "bet", "YmV0" },
          { "bey", "YmV5" },
          { "bib", "Ymli" },
          { "bid", "Ymlk" },
          { "big", "Ymln" },
          { "bin", "Ymlu" },
          { "bit", "Yml0" },
          { "biz", "Yml6" },
          { "BMW", "Qk1X" },
          { "boa", "Ym9h" },
          { "bob", "Ym9i" },
          { "bog", "Ym9n" },
          { "bon", "Ym9u" },
          { "boo", "Ym9v" },
          { "bop", "Ym9w" },
          { "bow", "Ym93" },
          { "box", "Ym94" },
          { "boy", "Ym95" },
          { "b's", "Yidz" },
          { "BTL", "QlRM" },
          { "BTU", "QlRV" },
          { "bub", "YnVi" },
          { "bud", "YnVk" },
          { "bug", "YnVn" },
          { "bum", "YnVt" },
          { "bun", "YnVu" },
          { "bus", "YnVz" },
          { "but", "YnV0" },
          { "buy", "YnV5" },
          { "bye", "Ynll" },
          { "cab", "Y2Fi" },
          { "Cal", "Q2Fs" },
          { "cam", "Y2Ft" },
          { "can", "Y2Fu" },
          { "cap", "Y2Fw" },
          { "car", "Y2Fy" },
          { "cat", "Y2F0" },
          { "caw", "Y2F3" },
          { "CBS", "Q0JT" },
          { "CDC", "Q0RD" },
          { "CEQ", "Q0VR" },
          { "chi", "Y2hp" },
          { "CIA", "Q0lB" },
          { "cit", "Y2l0" },
          { "cod", "Y29k" },
          { "cog", "Y29n" },
          { "col", "Y29s" },
          { "con", "Y29u" },
          { "coo", "Y29v" },
          { "cop", "Y29w" },
          { "cos", "Y29z" },
          { "cot", "Y290" },
          { "cow", "Y293" },
          { "cox", "Y294" },
          { "coy", "Y295" },
          { "CPA", "Q1BB" },
          { "cpu", "Y3B1" },
          { "CRT", "Q1JU" },
          { "cry", "Y3J5" },
          { "c's", "Yydz" },
          { "cub", "Y3Vi" },
          { "cud", "Y3Vk" },
          { "cue", "Y3Vl" },
          { "cup", "Y3Vw" },
          { "cur", "Y3Vy" },
          { "cut", "Y3V0" },
          { "dab", "ZGFi" },
          { "dad", "ZGFk" },
          { "dam", "ZGFt" },
          { "Dan", "RGFu" },
          { "Dar", "RGFy" },
          { "day", "ZGF5" },
          { "Dec", "RGVj" },
          { "Dee", "RGVl" },
          { "Del", "RGVs" },
          { "den", "ZGVu" },
          { "Des", "RGVz" },
          { "dew", "ZGV3" },
          { "dey", "ZGV5" },
          { "did", "ZGlk" },
          { "die", "ZGll" },
          { "dig", "ZGln" },
          { "dim", "ZGlt" },
          { "din", "ZGlu" },
          { "dip", "ZGlw" },
          { "Dis", "RGlz" },
          { "DNA", "RE5B" },
          { "DOD", "RE9E" },
          { "doe", "ZG9l" },
          { "dog", "ZG9n" },
          { "don", "ZG9u" },
          { "dot", "ZG90" },
          { "Dow", "RG93" },
          { "dry", "ZHJ5" },
          { "d's", "ZCdz" },
          { "dub", "ZHVi" },
          { "dud", "ZHVk" },
          { "due", "ZHVl" },
          { "dug", "ZHVn" },
          { "dun", "ZHVu" },
          { "dye", "ZHll" },
          { "ear", "ZWFy" },
          { "eat", "ZWF0" },
          { "ebb", "ZWJi" },
          { "EDT", "RURU" },
          { "eel", "ZWVs" },
          { "eft", "ZWZ0" },
          { "e.g", "ZS5n" },
          { "egg", "ZWdn" },
          { "ego", "ZWdv" },
          { "eke", "ZWtl" },
          { "Eli", "RWxp" },
          { "elk", "ZWxr" },
          { "ell", "ZWxs" },
          { "elm", "ZWxt" },
          { "Ely", "RWx5" },
          { "end", "ZW5k" },
          { "Eng", "RW5n" },
          { "EPA", "RVBB" },
          { "era", "ZXJh" },
          { "ere", "ZXJl" },
          { "erg", "ZXJn" },
          { "err", "ZXJy" },
          { "e's", "ZSdz" },
          { "EST", "RVNU" },
          { "eta", "ZXRh" },
          { "etc", "ZXRj" },
          { "Eva", "RXZh" },
          { "eve", "ZXZl" },
          { "ewe", "ZXdl" },
          { "eye", "ZXll" },
          { "FAA", "RkFB" },
          { "fad", "ZmFk" },
          { "fag", "ZmFn" },
          { "fan", "ZmFu" },
          { "far", "ZmFy" },
          { "fat", "ZmF0" },
          { "fay", "ZmF5" },
          { "FBI", "RkJJ" },
          { "FCC", "RkND" },
          { "FDA", "RkRB" },
          { "Feb", "RmVi" },
          { "fed", "ZmVk" },
          { "fee", "ZmVl" },
          { "few", "ZmV3" },
          { "fib", "Zmli" },
          { "fig", "Zmln" },
          { "fin", "Zmlu" },
          { "fir", "Zmly" },
          { "fit", "Zml0" },
          { "fix", "Zml4" },
          { "Flo", "Rmxv" },
          { "flu", "Zmx1" },
          { "fly", "Zmx5" },
          { "FMC", "Rk1D" },
          { "fob", "Zm9i" },
          { "foe", "Zm9l" },
          { "fog", "Zm9n" },
          { "fop", "Zm9w" },
          { "for", "Zm9y" },
          { "fox", "Zm94" },
          { "FPC", "RlBD" },
          { "fro", "ZnJv" },
          { "fry", "ZnJ5" },
          { "f's", "Zidz" },
          { "FTC", "RlRD" },
          { "fum", "ZnVt" },
          { "fun", "ZnVu" },
          { "fur", "ZnVy" },
          { "gab", "Z2Fi" },
          { "gad", "Z2Fk" },
          { "gag", "Z2Fn" },
          { "gal", "Z2Fs" },
          { "gam", "Z2Ft" },
          { "GAO", "R0FP" },
          { "gap", "Z2Fw" },
          { "gar", "Z2Fy" },
          { "gas", "Z2Fz" },
          { "gay", "Z2F5" },
          { "gee", "Z2Vl" },
          { "gel", "Z2Vs" },
          { "gem", "Z2Vt" },
          { "get", "Z2V0" },
          { "gig", "Z2ln" },
          { "Gil", "R2ls" },
          { "gin", "Z2lu" },
          { "GMT", "R01U" },
          { "GNP", "R05Q" },
          { "gnu", "Z251" },
          { "Goa", "R29h" },
          { "gob", "Z29i" },
          { "god", "Z29k" },
          { "gog", "Z29n" },
          { "GOP", "R09Q" },
          { "got", "Z290" },
          { "GPO", "R1BP" },
          { "g's", "Zydz" },
          { "GSA", "R1NB" },
          { "gum", "Z3Vt" },
          { "gun", "Z3Vu" },
          { "Gus", "R3Vz" },
          { "gut", "Z3V0" },
          { "guy", "Z3V5" },
          { "gym", "Z3lt" },
          { "gyp", "Z3lw" },
          { "had", "aGFk" },
          { "Hal", "SGFs" },
          { "ham", "aGFt" },
          { "Han", "SGFu" },
          { "hap", "aGFw" },
          { "hat", "aGF0" },
          { "haw", "aGF3" },
          { "hay", "aGF5" },
          { "hem", "aGVt" },
          { "hen", "aGVu" },
          { "her", "aGVy" },
          { "hew", "aGV3" },
          { "hex", "aGV4" },
          { "hey", "aGV5" },
          { "hid", "aGlk" },
          { "him", "aGlt" },
          { "hip", "aGlw" },
          { "his", "aGlz" },
          { "hit", "aGl0" },
          { "hob", "aG9i" },
          { "hoc", "aG9j" },
          { "hoe", "aG9l" },
          { "hog", "aG9n" },
          { "hoi", "aG9p" },
          { "Hom", "SG9t" },
          { "hop", "aG9w" },
          { "hot", "aG90" },
          { "how", "aG93" },
          { "hoy", "aG95" },
          { "h's", "aCdz" },
          { "hub", "aHVi" },
          { "hue", "aHVl" },
          { "hug", "aHVn" },
          { "huh", "aHVo" },
          { "hum", "aHVt" },
          { "Hun", "SHVu" },
          { "hut", "aHV0" },
          { "Ian", "SWFu" },
          { "IBM", "SUJN" },
          { "Ibn", "SWJu" },
          { "ICC", "SUND" },
          { "ice", "aWNl" },
          { "icy", "aWN5" },
          { "I'd", "SSdk" },
          { "Ida", "SWRh" },
          { "i.e", "aS5l" },
          { "iii", "aWlp" },
          { "Ike", "SWtl" },
          { "ill", "aWxs" },
          { "I'm", "SSdt" },
          { "imp", "aW1w" },
          { "Inc", "SW5j" },
          { "ink", "aW5r" },
          { "inn", "aW5u" },
          { "ion", "aW9u" },
          { "Ira", "SXJh" },
          { "ire", "aXJl" },
          { "irk", "aXJr" },
          { "IRS", "SVJT" },
          { "i's", "aSdz" },
          { "Ito", "SXRv" },
          { "ITT", "SVRU" },
          { "ivy", "aXZ5" },
          { "jab", "amFi" },
          { "jag", "amFn" },
          { "jam", "amFt" },
          { "Jan", "SmFu" },
          { "jar", "amFy" },
          { "jaw", "amF3" },
          { "jay", "amF5" },
          { "Jed", "SmVk" },
          { "jet", "amV0" },
          { "Jew", "SmV3" },
          { "jig", "amln" },
          { "Jim", "Smlt" },
          { "job", "am9i" },
          { "Joe", "Sm9l" },
          { "jog", "am9n" },
          { "Jon", "Sm9u" },
          { "jot", "am90" },
          { "joy", "am95" },
          { "j's", "aidz" },
          { "jug", "anVn" },
          { "jut", "anV0" },
          { "Kay", "S2F5" },
          { "keg", "a2Vn" },
          { "ken", "a2Vu" },
          { "key", "a2V5" },
          { "kid", "a2lk" },
          { "Kim", "S2lt" },
          { "kin", "a2lu" },
          { "kit", "a2l0" },
          { "k's", "aydz" },
          { "lab", "bGFi" },
          { "lac", "bGFj" },
          { "lad", "bGFk" },
          { "lag", "bGFn" },
          { "lam", "bGFt" },
          { "Lao", "TGFv" },
          { "lap", "bGFw" },
          { "law", "bGF3" },
          { "lax", "bGF4" },
          { "lay", "bGF5" },
          { "lea", "bGVh" },
          { "led", "bGVk" },
          { "lee", "bGVl" },
          { "leg", "bGVn" },
          { "Len", "TGVu" },
          { "Leo", "TGVv" },
          { "let", "bGV0" },
          { "Lev", "TGV2" },
          { "Lew", "TGV3" },
          { "lew", "bGV3" },
          { "lid", "bGlk" },
          { "lie", "bGll" },
          { "lim", "bGlt" },
          { "Lin", "TGlu" },
          { "lip", "bGlw" },
          { "lit", "bGl0" },
          { "Liz", "TGl6" },
          { "lob", "bG9i" },
          { "log", "bG9n" },
          { "lop", "bG9w" },
          { "Los", "TG9z" },
          { "lot", "bG90" },
          { "Lou", "TG91" },
          { "low", "bG93" },
          { "loy", "bG95" },
          { "l's", "bCdz" },
          { "LSI", "TFNJ" },
          { "Ltd", "THRk" },
          { "LTV", "TFRW" },
          { "lug", "bHVn" },
          { "lux", "bHV4" },
          { "lye", "bHll" },
          { "Mac", "TWFj" },
          { "mad", "bWFk" },
          { "Mae", "TWFl" },
          { "man", "bWFu" },
          { "Mao", "TWFv" },
          { "map", "bWFw" },
          { "mar", "bWFy" },
          { "mat", "bWF0" },
          { "maw", "bWF3" },
          { "Max", "TWF4" },
          { "max", "bWF4" },
          { "may", "bWF5" },
          { "MBA", "TUJB" },
          { "Meg", "TWVn" },
          { "Mel", "TWVs" },
          { "men", "bWVu" },
          { "met", "bWV0" },
          { "mew", "bWV3" },
          { "mid", "bWlk" },
          { "mig", "bWln" },
          { "min", "bWlu" },
          { "MIT", "TUlU" },
          { "mix", "bWl4" },
          { "mob", "bW9i" },
          { "Moe", "TW9l" },
          { "moo", "bW9v" },
          { "mop", "bW9w" },
          { "mot", "bW90" },
          { "mow", "bW93" },
          { "MPH", "TVBI" },
          { "Mrs", "TXJz" },
          { "m's", "bSdz" },
          { "mud", "bXVk" },
          { "mug", "bXVn" },
          { "mum", "bXVt" },
          { "nab", "bmFi" },
          { "nag", "bmFn" },
          { "Nan", "TmFu" },
          { "nap", "bmFw" },
          { "Nat", "TmF0" },
          { "nay", "bmF5" },
          { "NBC", "TkJD" },
          { "NBS", "TkJT" },
          { "NCO", "TkNP" },
          { "NCR", "TkNS" },
          { "Ned", "TmVk" },
          { "nee", "bmVl" },
          { "net", "bmV0" },
          { "new", "bmV3" },
          { "nib", "bmli" },
          { "NIH", "TklI" },
          { "nil", "bmls" },
          { "nip", "bmlw" },
          { "nit", "bml0" },
          { "NNE", "Tk5F" },
          { "NNW", "Tk5X" },
          { "nob", "bm9i" },
          { "nod", "bm9k" },
          { "non", "bm9u" },
          { "nor", "bm9y" },
          { "not", "bm90" },
          { "Nov", "Tm92" },
          { "now", "bm93" },
          { "NRC", "TlJD" },
          { "n's", "bidz" },
          { "NSF", "TlNG" },
          { "nun", "bnVu" },
          { "nut", "bnV0" },
          { "NYC", "TllD" },
          { "NYU", "TllV" },
          { "oaf", "b2Fm" },
          { "oak", "b2Fr" },
          { "oar", "b2Fy" },
          { "oat", "b2F0" },
          { "Oct", "T2N0" },
          { "odd", "b2Rk" },
          { "ode", "b2Rl" },
          { "off", "b2Zm" },
          { "oft", "b2Z0" },
          { "ohm", "b2ht" },
          { "oil", "b2ls" },
          { "old", "b2xk" },
          { "one", "b25l" },
          { "opt", "b3B0" },
          { "orb", "b3Ji" },
          { "ore", "b3Jl" },
          { "Orr", "T3Jy" },
          { "o's", "bydz" },
          { "Ott", "T3R0" },
          { "our", "b3Vy" },
          { "out", "b3V0" },
          { "ova", "b3Zh" },
          { "owe", "b3dl" },
          { "owl", "b3ds" },
          { "own", "b3du" },
          { "pad", "cGFk" },
          { "pal", "cGFs" },
          { "Pam", "UGFt" },
          { "pan", "cGFu" },
          { "pap", "cGFw" },
          { "par", "cGFy" },
          { "pat", "cGF0" },
          { "paw", "cGF3" },
          { "pax", "cGF4" },
          { "pay", "cGF5" },
          { "Paz", "UGF6" },
          { "PBS", "UEJT" },
          { "PDP", "UERQ" },
          { "pea", "cGVh" },
          { "pee", "cGVl" },
          { "peg", "cGVn" },
          { "pen", "cGVu" },
          { "pep", "cGVw" },
          { "per", "cGVy" },
          { "pet", "cGV0" },
          { "pew", "cGV3" },
          { "PhD", "UGhE" },
          { "phi", "cGhp" },
          { "pie", "cGll" },
          { "pig", "cGln" },
          { "pin", "cGlu" },
          { "pip", "cGlw" },
          { "pit", "cGl0" },
          { "ply", "cGx5" },
          { "pod", "cG9k" },
          { "Poe", "UG9l" },
          { "poi", "cG9p" },
          { "pol", "cG9s" },
          { "pop", "cG9w" },
          { "pot", "cG90" },
          { "pow", "cG93" },
          { "ppm", "cHBt" },
          { "pro", "cHJv" },
          { "pry", "cHJ5" },
          { "p's", "cCdz" },
          { "psi", "cHNp" },
          { "PTA", "UFRB" },
          { "pub", "cHVi" },
          { "PUC", "UFVD" },
          { "pug", "cHVn" },
          { "pun", "cHVu" },
          { "pup", "cHVw" },
          { "pus", "cHVz" },
          { "put", "cHV0" },
          { "PVC", "UFZD" },
          { "QED", "UUVE" },
          { "q's", "cSdz" },
          { "qua", "cXVh" },
          { "quo", "cXVv" },
          { "Rae", "UmFl" },
          { "rag", "cmFn" },
          { "raj", "cmFq" },
          { "ram", "cmFt" },
          { "ran", "cmFu" },
          { "rap", "cmFw" },
          { "rat", "cmF0" },
          { "raw", "cmF3" },
          { "ray", "cmF5" },
          { "RCA", "UkNB" },
          { "R&D", "UiZE" },
          { "reb", "cmVi" },
          { "red", "cmVk" },
          { "rep", "cmVw" },
          { "ret", "cmV0" },
          { "rev", "cmV2" },
          { "Rex", "UmV4" },
          { "rho", "cmhv" },
          { "rib", "cmli" },
          { "rid", "cmlk" },
          { "rig", "cmln" },
          { "rim", "cmlt" },
          { "Rio", "Umlv" },
          { "rip", "cmlw" },
          { "RNA", "Uk5B" },
          { "rob", "cm9i" },
          { "rod", "cm9k" },
          { "roe", "cm9l" },
          { "Ron", "Um9u" },
          { "rot", "cm90" },
          { "row", "cm93" },
          { "Roy", "Um95" },
          { "RPM", "UlBN" },
          { "r's", "cidz" },
          { "rub", "cnVi" },
          { "rue", "cnVl" },
          { "rug", "cnVn" },
          { "rum", "cnVt" },
          { "run", "cnVu" },
          { "rut", "cnV0" },
          { "rye", "cnll" },
          { "sac", "c2Fj" },
          { "sad", "c2Fk" },
          { "sag", "c2Fn" },
          { "Sal", "U2Fs" },
          { "Sam", "U2Ft" },
          { "San", "U2Fu" },
          { "Sao", "U2Fv" },
          { "sap", "c2Fw" },
          { "sat", "c2F0" },
          { "saw", "c2F3" },
          { "sax", "c2F4" },
          { "say", "c2F5" },
          { "Sci", "U2Np" },
          { "SCM", "U0NN" },
          { "sea", "c2Vh" },
          { "sec", "c2Vj" },
          { "see", "c2Vl" },
          { "sen", "c2Vu" },
          { "seq", "c2Vx" },
          { "set", "c2V0" },
          { "sew", "c2V3" },
          { "sex", "c2V4" },
          { "she", "c2hl" },
          { "Shu", "U2h1" },
          { "shy", "c2h5" },
          { "sib", "c2li" },
          { "sic", "c2lj" },
          { "sin", "c2lu" },
          { "sip", "c2lw" },
          { "sir", "c2ly" },
          { "sis", "c2lz" },
          { "sit", "c2l0" },
          { "six", "c2l4" },
          { "ski", "c2tp" },
          { "sky", "c2t5" },
          { "sly", "c2x5" },
          { "sob", "c29i" },
          { "Soc", "U29j" },
          { "sod", "c29k" },
          { "Sol", "U29s" },
          { "son", "c29u" },
          { "sop", "c29w" },
          { "sou", "c291" },
          { "sow", "c293" },
          { "soy", "c295" },
          { "spa", "c3Bh" },
          { "spy", "c3B5" },
          { "Sri", "U3Jp" },
          { "s's", "cydz" },
          { "SSE", "U1NF" },
          { "SST", "U1NU" },
          { "SSW", "U1NX" },
          { "Stu", "U3R1" },
          { "sub", "c3Vi" },
          { "sud", "c3Vk" },
          { "sue", "c3Vl" },
          { "sum", "c3Vt" },
          { "sun", "c3Vu" },
          { "sup", "c3Vw" },
          { "Sus", "U3Vz" },
          { "tab", "dGFi" },
          { "tad", "dGFk" },
          { "tag", "dGFn" },
          { "tam", "dGFt" },
          { "tan", "dGFu" },
          { "tao", "dGFv" },
          { "tap", "dGFw" },
          { "tar", "dGFy" },
          { "tat", "dGF0" },
          { "tau", "dGF1" },
          { "tax", "dGF4" },
          { "tea", "dGVh" },
          { "Ted", "VGVk" },
          { "ted", "dGVk" },
          { "tee", "dGVl" },
          { "Tel", "VGVs" },
          { "ten", "dGVu" },
          { "the", "dGhl" },
          { "thy", "dGh5" },
          { "tic", "dGlj" },
          { "tid", "dGlk" },
          { "tie", "dGll" },
          { "til", "dGls" },
          { "Tim", "VGlt" },
          { "tin", "dGlu" },
          { "tip", "dGlw" },
          { "tit", "dGl0" },
          { "TNT", "VE5U" },
          { "toe", "dG9l" },
          { "tog", "dG9n" },
          { "Tom", "VG9t" },
          { "ton", "dG9u" },
          { "too", "dG9v" },
          { "top", "dG9w" },
          { "tor", "dG9y" },
          { "tot", "dG90" },
          { "tow", "dG93" },
          { "toy", "dG95" },
          { "TRW", "VFJX" },
          { "try", "dHJ5" },
          { "t's", "dCdz" },
          { "TTL", "VFRM" },
          { "TTY", "VFRZ" },
          { "tub", "dHVi" },
          { "tug", "dHVn" },
          { "tum", "dHVt" },
          { "tun", "dHVu" },
          { "TVA", "VFZB" },
          { "TWA", "VFdB" },
          { "two", "dHdv" },
          { "TWX", "VFdY" },
          { "ugh", "dWdo" },
          { "UHF", "VUhG" },
          { "Uri", "VXJp" },
          { "urn", "dXJu" },
          { "U.S", "VS5T" },
          { "u's", "dSdz" },
          { "USA", "VVNB" },
          { "USC", "VVND" },
          { "use", "dXNl" },
          { "USN", "VVNO" },
          { "van", "dmFu" },
          { "vat", "dmF0" },
          { "vee", "dmVl" },
          { "vet", "dmV0" },
          { "vex", "dmV4" },
          { "VHF", "VkhG" },
          { "via", "dmlh" },
          { "vie", "dmll" },
          { "vii", "dmlp" },
          { "vis", "dmlz" },
          { "viz", "dml6" },
          { "von", "dm9u" },
          { "vow", "dm93" },
          { "v's", "didz" },
          { "WAC", "V0FD" },
          { "wad", "d2Fk" },
          { "wag", "d2Fn" },
          { "wah", "d2Fo" },
          { "wan", "d2Fu" },
          { "war", "d2Fy" },
          { "was", "d2Fz" },
          { "wax", "d2F4" },
          { "way", "d2F5" },
          { "web", "d2Vi" },
          { "wed", "d2Vk" },
          { "wee", "d2Vl" },
          { "Wei", "V2Vp" },
          { "wet", "d2V0" },
          { "who", "d2hv" },
          { "why", "d2h5" },
          { "wig", "d2ln" },
          { "win", "d2lu" },
          { "wit", "d2l0" },
          { "woe", "d29l" },
          { "wok", "d29r" },
          { "won", "d29u" },
          { "woo", "d29v" },
          { "wop", "d29w" },
          { "wow", "d293" },
          { "wry", "d3J5" },
          { "w's", "dydz" },
          { "x's", "eCdz" },
          { "yah", "eWFo" },
          { "yak", "eWFr" },
          { "yam", "eWFt" },
          { "yap", "eWFw" },
          { "yaw", "eWF3" },
          { "yea", "eWVh" },
          { "yen", "eWVu" },
          { "yet", "eWV0" },
          { "yin", "eWlu" },
          { "yip", "eWlw" },
          { "yon", "eW9u" },
          { "you", "eW91" },
          { "yow", "eW93" },
          { "y's", "eSdz" },
          { "yuh", "eXVo" },
          { "zag", "emFn" },
          { "Zan", "WmFu" },
          { "zap", "emFw" },
          { "Zen", "WmVu" },
          { "zig", "emln" },
          { "zip", "emlw" },
          { "Zoe", "Wm9l" },
          { "zoo", "em9v" },
          { "z's", "eidz" },
          /* the false rumors file */
          { "\"So when I die, the first thing I will see in heaven is a score list?\"", 
            "IlNvIHdoZW4gSSBkaWUsIHRoZSBmaXJzdCB0aGluZyBJIHdpbGwgc2VlIGluIGhlYXZlbiBpcyBhIHNjb3JlIGxpc3Q/Ig==" },
          { "1st Law of Hacking: leaving is much more difficult than entering.", 
            "MXN0IExhdyBvZiBIYWNraW5nOiBsZWF2aW5nIGlzIG11Y2ggbW9yZSBkaWZmaWN1bHQgdGhhbiBlbnRlcmluZy4=" },
          { "2nd Law of Hacking: first in, first out.", 
            "Mm5kIExhdyBvZiBIYWNraW5nOiBmaXJzdCBpbiwgZmlyc3Qgb3V0Lg==" },
          { "3rd Law of Hacking: the last blow counts most.", 
            "M3JkIExhdyBvZiBIYWNraW5nOiB0aGUgbGFzdCBibG93IGNvdW50cyBtb3N0Lg==" },
          { "4th Law of Hacking: you will find the exit at the entrance.", 
            "NHRoIExhdyBvZiBIYWNraW5nOiB5b3Ugd2lsbCBmaW5kIHRoZSBleGl0IGF0IHRoZSBlbnRyYW5jZS4=" },
          { "A chameleon imitating a mail daemon often delivers scrolls of fire.", 
            "QSBjaGFtZWxlb24gaW1pdGF0aW5nIGEgbWFpbCBkYWVtb24gb2Z0ZW4gZGVsaXZlcnMgc2Nyb2xscyBvZiBmaXJlLg==" },
          { "A cockatrice corpse is guaranteed to be untainted!", 
            "QSBjb2NrYXRyaWNlIGNvcnBzZSBpcyBndWFyYW50ZWVkIHRvIGJlIHVudGFpbnRlZCE=" },
          { "A dead cockatrice is just a dead lizard.", 
            "QSBkZWFkIGNvY2thdHJpY2UgaXMganVzdCBhIGRlYWQgbGl6YXJkLg==" },
          { "A dragon is just a snake that ate a scroll of fire.", 
            "QSBkcmFnb24gaXMganVzdCBhIHNuYWtlIHRoYXQgYXRlIGEgc2Nyb2xsIG9mIGZpcmUu" },
          { "A fading corridor enlightens your insight.", 
            "QSBmYWRpbmcgY29ycmlkb3IgZW5saWdodGVucyB5b3VyIGluc2lnaHQu" },
          { "A glowing potion is too hot to drink.", 
            "QSBnbG93aW5nIHBvdGlvbiBpcyB0b28gaG90IHRvIGRyaW5rLg==" },
          { "A good amulet may protect you against guards.", 
            "QSBnb29kIGFtdWxldCBtYXkgcHJvdGVjdCB5b3UgYWdhaW5zdCBndWFyZHMu" },
          { "A lizard corpse is a good thing to turn undead.", 
            "QSBsaXphcmQgY29ycHNlIGlzIGEgZ29vZCB0aGluZyB0byB0dXJuIHVuZGVhZC4=" },
          { "A long worm can be defined recursively. So how should you attack it?", 
            "QSBsb25nIHdvcm0gY2FuIGJlIGRlZmluZWQgcmVjdXJzaXZlbHkuIFNvIGhvdyBzaG91bGQgeW91IGF0dGFjayBpdD8=" },
          { "A monstrous mind is a toy forever.", 
            "QSBtb25zdHJvdXMgbWluZCBpcyBhIHRveSBmb3JldmVyLg==" },
          { "A nymph will be very pleased if you call her by her real name: Lorelei.", 
            "QSBueW1waCB3aWxsIGJlIHZlcnkgcGxlYXNlZCBpZiB5b3UgY2FsbCBoZXIgYnkgaGVyIHJlYWwgbmFtZTogTG9yZWxlaS4=" },
          { "A ring of dungeon master control is a great find.", 
            "QSByaW5nIG9mIGR1bmdlb24gbWFzdGVyIGNvbnRyb2wgaXMgYSBncmVhdCBmaW5kLg==" },
          { "A ring of extra ring finger is useless if not enchanted.", 
            "QSByaW5nIG9mIGV4dHJhIHJpbmcgZmluZ2VyIGlzIHVzZWxlc3MgaWYgbm90IGVuY2hhbnRlZC4=" },
          { "A rope may form a trail in a maze.", 
            "QSByb3BlIG1heSBmb3JtIGEgdHJhaWwgaW4gYSBtYXplLg==" },
          { "A staff may recharge if you drop it for awhile.", 
            "QSBzdGFmZiBtYXkgcmVjaGFyZ2UgaWYgeW91IGRyb3AgaXQgZm9yIGF3aGlsZS4=" },
          { "A visit to the Zoo is very educational; you meet interesting animals.", 
            "QSB2aXNpdCB0byB0aGUgWm9vIGlzIHZlcnkgZWR1Y2F0aW9uYWw7IHlvdSBtZWV0IGludGVyZXN0aW5nIGFuaW1hbHMu" },
          { "A wand of deaf is a more dangerous weapon than a wand of sheep.", 
            "QSB3YW5kIG9mIGRlYWYgaXMgYSBtb3JlIGRhbmdlcm91cyB3ZWFwb24gdGhhbiBhIHdhbmQgb2Ygc2hlZXAu" },
          { "A wand of vibration might bring the whole cave crashing about your ears.", 
            "QSB3YW5kIG9mIHZpYnJhdGlvbiBtaWdodCBicmluZyB0aGUgd2hvbGUgY2F2ZSBjcmFzaGluZyBhYm91dCB5b3VyIGVhcnMu" },
          { "A winner never quits. A quitter never wins.", 
            "QSB3aW5uZXIgbmV2ZXIgcXVpdHMuIEEgcXVpdHRlciBuZXZlciB3aW5zLg==" },
          { "A wish? Okay, make me a fortune cookie!", 
            "QSB3aXNoPyBPa2F5LCBtYWtlIG1lIGEgZm9ydHVuZSBjb29raWUh" },
          { "Afraid of mimics? Try to wear a ring of true seeing.", 
            "QWZyYWlkIG9mIG1pbWljcz8gVHJ5IHRvIHdlYXIgYSByaW5nIG9mIHRydWUgc2VlaW5nLg==" },
          { "All monsters are created evil, but some are more evil than others.", 
            "QWxsIG1vbnN0ZXJzIGFyZSBjcmVhdGVkIGV2aWwsIGJ1dCBzb21lIGFyZSBtb3JlIGV2aWwgdGhhbiBvdGhlcnMu" },
          { "Always attack a floating eye from behind!", 
            "QWx3YXlzIGF0dGFjayBhIGZsb2F0aW5nIGV5ZSBmcm9tIGJlaGluZCE=" },
          { "An elven cloak is always the height of fashion.", 
            "QW4gZWx2ZW4gY2xvYWsgaXMgYWx3YXlzIHRoZSBoZWlnaHQgb2YgZmFzaGlvbi4=" },
          { "Any small object that is accidentally dropped will hide under a larger object.", 
            "QW55IHNtYWxsIG9iamVjdCB0aGF0IGlzIGFjY2lkZW50YWxseSBkcm9wcGVkIHdpbGwgaGlkZSB1bmRlciBhIGxhcmdlciBvYmplY3Qu" },
          { "Balrogs do not appear above level 20.", 
            "QmFscm9ncyBkbyBub3QgYXBwZWFyIGFib3ZlIGxldmVsIDIwLg==" },
          { "Banana peels work especially well against Keystone Kops.", 
            "QmFuYW5hIHBlZWxzIHdvcmsgZXNwZWNpYWxseSB3ZWxsIGFnYWluc3QgS2V5c3RvbmUgS29wcy4=" },
          { "Be careful when eating bananas. Monsters might slip on the peels.", 
            "QmUgY2FyZWZ1bCB3aGVuIGVhdGluZyBiYW5hbmFzLiBNb25zdGVycyBtaWdodCBzbGlwIG9uIHRoZSBwZWVscy4=" },
          { "Better leave the dungeon; otherwise you might get hurt badly.", 
            "QmV0dGVyIGxlYXZlIHRoZSBkdW5nZW9uOyBvdGhlcndpc2UgeW91IG1pZ2h0IGdldCBodXJ0IGJhZGx5Lg==" },
          { "Beware of the potion of nitroglycerin -- it's not for the weak of heart.", 
            "QmV3YXJlIG9mIHRoZSBwb3Rpb24gb2Ygbml0cm9nbHljZXJpbiAtLSBpdCdzIG5vdCBmb3IgdGhlIHdlYWsgb2YgaGVhcnQu" },
          { "Beware: there's always a chance that your wand explodes as you try to zap it!", 
            "QmV3YXJlOiB0aGVyZSdzIGFsd2F5cyBhIGNoYW5jZSB0aGF0IHlvdXIgd2FuZCBleHBsb2RlcyBhcyB5b3UgdHJ5IHRvIHphcCBpdCE=" },
          { "Beyond the 23rd level lies a happy retirement in a room of your own.", 
            "QmV5b25kIHRoZSAyM3JkIGxldmVsIGxpZXMgYSBoYXBweSByZXRpcmVtZW50IGluIGEgcm9vbSBvZiB5b3VyIG93bi4=" },
          { "Changing your suit without dropping your sword? You must be kidding!", 
            "Q2hhbmdpbmcgeW91ciBzdWl0IHdpdGhvdXQgZHJvcHBpbmcgeW91ciBzd29yZD8gWW91IG11c3QgYmUga2lkZGluZyE=" },
          { "Cockatrices might turn themselves to stone faced with a mirror.", 
            "Q29ja2F0cmljZXMgbWlnaHQgdHVybiB0aGVtc2VsdmVzIHRvIHN0b25lIGZhY2VkIHdpdGggYSBtaXJyb3Iu" },
          { "Consumption of home-made food is strictly forbidden in this dungeon.", 
            "Q29uc3VtcHRpb24gb2YgaG9tZS1tYWRlIGZvb2QgaXMgc3RyaWN0bHkgZm9yYmlkZGVuIGluIHRoaXMgZHVuZ2Vvbi4=" },
          { "Dark room? Your chance to develop your photographs!", 
            "RGFyayByb29tPyBZb3VyIGNoYW5jZSB0byBkZXZlbG9wIHlvdXIgcGhvdG9ncmFwaHMh" },
          { "Dark rooms are not *completely* dark: just wait and let your eyes adjust...", 
            "RGFyayByb29tcyBhcmUgbm90ICpjb21wbGV0ZWx5KiBkYXJrOiBqdXN0IHdhaXQgYW5kIGxldCB5b3VyIGV5ZXMgYWRqdXN0Li4u" },
          { "David London sez, \"Hey guys, *WIELD* a lizard corpse against a cockatrice!\"", 
            "RGF2aWQgTG9uZG9uIHNleiwgIkhleSBndXlzLCAqV0lFTEQqIGEgbGl6YXJkIGNvcnBzZSBhZ2FpbnN0IGEgY29ja2F0cmljZSEi" },
          { "Death is just life's way of telling you you've been fired.", 
            "RGVhdGggaXMganVzdCBsaWZlJ3Mgd2F5IG9mIHRlbGxpbmcgeW91IHlvdSd2ZSBiZWVuIGZpcmVkLg==" },
          { "Demi-gods don't need any help from the gods.", 
            "RGVtaS1nb2RzIGRvbid0IG5lZWQgYW55IGhlbHAgZnJvbSB0aGUgZ29kcy4=" },
          { "Demons *HATE* Priests and Priestesses.", 
            "RGVtb25zICpIQVRFKiBQcmllc3RzIGFuZCBQcmllc3Rlc3Nlcy4=" },
          { "Didn't you forget to pay?", 
            "RGlkbid0IHlvdSBmb3JnZXQgdG8gcGF5Pw==" },
          { "Didn't your mother tell you not to eat food off the floor?", 
            "RGlkbid0IHlvdXIgbW90aGVyIHRlbGwgeW91IG5vdCB0byBlYXQgZm9vZCBvZmYgdGhlIGZsb29yPw==" },
          { "Direct a direct hit on your direct opponent, directing in the right direction.", 
            "RGlyZWN0IGEgZGlyZWN0IGhpdCBvbiB5b3VyIGRpcmVjdCBvcHBvbmVudCwgZGlyZWN0aW5nIGluIHRoZSByaWdodCBkaXJlY3Rpb24u" },
          { "Do you want to make more money? Sure, we all do! Join the Fort Ludios guard!", 
            "RG8geW91IHdhbnQgdG8gbWFrZSBtb3JlIG1vbmV5PyBTdXJlLCB3ZSBhbGwgZG8hIEpvaW4gdGhlIEZvcnQgTHVkaW9zIGd1YXJkIQ==" },
          { "Don't eat too much: you might start hiccoughing!", 
            "RG9uJ3QgZWF0IHRvbyBtdWNoOiB5b3UgbWlnaHQgc3RhcnQgaGljY291Z2hpbmch" },
          { "Don't play hack at your work; your boss might hit you!", 
            "RG9uJ3QgcGxheSBoYWNrIGF0IHlvdXIgd29yazsgeW91ciBib3NzIG1pZ2h0IGhpdCB5b3Uh" },
          { "Don't tell a soul you found a secret door, otherwise it isn't a secret anymore.", 
            "RG9uJ3QgdGVsbCBhIHNvdWwgeW91IGZvdW5kIGEgc2VjcmV0IGRvb3IsIG90aGVyd2lzZSBpdCBpc24ndCBhIHNlY3JldCBhbnltb3JlLg==" },
          { "Drinking potions of booze may land you in jail if you are under 21.", 
            "RHJpbmtpbmcgcG90aW9ucyBvZiBib296ZSBtYXkgbGFuZCB5b3UgaW4gamFpbCBpZiB5b3UgYXJlIHVuZGVyIDIxLg==" },
          { "Drop your vanity and get rid of your jewels! Pickpockets about!", 
            "RHJvcCB5b3VyIHZhbml0eSBhbmQgZ2V0IHJpZCBvZiB5b3VyIGpld2VscyEgUGlja3BvY2tldHMgYWJvdXQh" },
          { "Eat 10 cloves of garlic and keep all humans at a two-square distance.", 
            "RWF0IDEwIGNsb3ZlcyBvZiBnYXJsaWMgYW5kIGtlZXAgYWxsIGh1bWFucyBhdCBhIHR3by1zcXVhcmUgZGlzdGFuY2Uu" },
          { "Eels hide under mud. Use a unicorn to clear the water and make them visible.", 
            "RWVscyBoaWRlIHVuZGVyIG11ZC4gVXNlIGEgdW5pY29ybiB0byBjbGVhciB0aGUgd2F0ZXIgYW5kIG1ha2UgdGhlbSB2aXNpYmxlLg==" },
          { "Engrave your wishes with a wand of wishing.", 
            "RW5ncmF2ZSB5b3VyIHdpc2hlcyB3aXRoIGEgd2FuZCBvZiB3aXNoaW5nLg==" },
          { "Eventually you will come to admire the swift elegance of a retreating nymph.", 
            "RXZlbnR1YWxseSB5b3Ugd2lsbCBjb21lIHRvIGFkbWlyZSB0aGUgc3dpZnQgZWxlZ2FuY2Ugb2YgYSByZXRyZWF0aW5nIG55bXBoLg==" },
          { "Ever heard hissing outside? I *knew* you hadn't!", 
            "RXZlciBoZWFyZCBoaXNzaW5nIG91dHNpZGU/IEkgKmtuZXcqIHlvdSBoYWRuJ3Qh" },
          { "Ever lifted a dragon corpse?", 
            "RXZlciBsaWZ0ZWQgYSBkcmFnb24gY29ycHNlPw==" },
          { "Ever seen a leocrotta dancing the tengu?", 
            "RXZlciBzZWVuIGEgbGVvY3JvdHRhIGRhbmNpbmcgdGhlIHRlbmd1Pw==" },
          { "Ever seen your weapon glow plaid?", 
            "RXZlciBzZWVuIHlvdXIgd2VhcG9uIGdsb3cgcGxhaWQ/" },
          { "Ever tamed a shopkeeper?", 
            "RXZlciB0YW1lZCBhIHNob3BrZWVwZXI/" },
          { "Ever tried digging through a Vault Guard?", 
            "RXZlciB0cmllZCBkaWdnaW5nIHRocm91Z2ggYSBWYXVsdCBHdWFyZD8=" },
          { "Ever tried enchanting a rope?", 
            "RXZlciB0cmllZCBlbmNoYW50aW5nIGEgcm9wZT8=" },
          { "Floating eyes can't stand Hawaiian shirts.", 
            "RmxvYXRpbmcgZXllcyBjYW4ndCBzdGFuZCBIYXdhaWlhbiBzaGlydHMu" },
          { "For any remedy there is a misery.", 
            "Rm9yIGFueSByZW1lZHkgdGhlcmUgaXMgYSBtaXNlcnku" },
          { "Giant bats turn into giant vampires.", 
            "R2lhbnQgYmF0cyB0dXJuIGludG8gZ2lhbnQgdmFtcGlyZXMu" },
          { "Good day for overcoming obstacles. Try a steeplechase.", 
            "R29vZCBkYXkgZm9yIG92ZXJjb21pbmcgb2JzdGFjbGVzLiBUcnkgYSBzdGVlcGxlY2hhc2Uu" },
          { "Half Moon tonight. (At least it's better than no Moon at all.)", 
            "SGFsZiBNb29uIHRvbmlnaHQuIChBdCBsZWFzdCBpdCdzIGJldHRlciB0aGFuIG5vIE1vb24gYXQgYWxsLik=" },
          { "Help! I'm being held prisoner in a fortune cookie factory!", 
            "SGVscCEgSSdtIGJlaW5nIGhlbGQgcHJpc29uZXIgaW4gYSBmb3J0dW5lIGNvb2tpZSBmYWN0b3J5IQ==" },
          { "Housecats have nine lives, kittens only one.", 
            "SG91c2VjYXRzIGhhdmUgbmluZSBsaXZlcywga2l0dGVucyBvbmx5IG9uZS4=" },
          { "How long can you tread water?", 
            "SG93IGxvbmcgY2FuIHlvdSB0cmVhZCB3YXRlcj8=" },
          { "Hungry? There is an abundance of food on the next level.", 
            "SHVuZ3J5PyBUaGVyZSBpcyBhbiBhYnVuZGFuY2Ugb2YgZm9vZCBvbiB0aGUgbmV4dCBsZXZlbC4=" },
          { "I guess you've never hit a mail daemon with the Amulet of Yendor...", 
            "SSBndWVzcyB5b3UndmUgbmV2ZXIgaGl0IGEgbWFpbCBkYWVtb24gd2l0aCB0aGUgQW11bGV0IG9mIFllbmRvci4uLg==" },
          { "If you are the shopkeeper, you can take things for free.", 
            "SWYgeW91IGFyZSB0aGUgc2hvcGtlZXBlciwgeW91IGNhbiB0YWtlIHRoaW5ncyBmb3IgZnJlZS4=" },
          { "If you can't learn to do it well, learn to enjoy doing it badly.", 
            "SWYgeW91IGNhbid0IGxlYXJuIHRvIGRvIGl0IHdlbGwsIGxlYXJuIHRvIGVuam95IGRvaW5nIGl0IGJhZGx5Lg==" },
          { "If you thought the Wizard was bad, just wait till you meet the Warlord!", 
            "SWYgeW91IHRob3VnaHQgdGhlIFdpemFyZCB3YXMgYmFkLCBqdXN0IHdhaXQgdGlsbCB5b3UgbWVldCB0aGUgV2FybG9yZCE=" },
          { "If you turn blind, don't expect your dog to be turned into a seeing-eye dog.", 
            "SWYgeW91IHR1cm4gYmxpbmQsIGRvbid0IGV4cGVjdCB5b3VyIGRvZyB0byBiZSB0dXJuZWQgaW50byBhIHNlZWluZy1leWUgZG9nLg==" },
          { "If you want to feel great, you must eat something real big.", 
            "SWYgeW91IHdhbnQgdG8gZmVlbCBncmVhdCwgeW91IG11c3QgZWF0IHNvbWV0aGluZyByZWFsIGJpZy4=" },
          { "If you want to float, you'd better eat a floating eye.", 
            "SWYgeW91IHdhbnQgdG8gZmxvYXQsIHlvdSdkIGJldHRlciBlYXQgYSBmbG9hdGluZyBleWUu" },
          { "If your ghost kills a player, it increases your score.", 
            "SWYgeW91ciBnaG9zdCBraWxscyBhIHBsYXllciwgaXQgaW5jcmVhc2VzIHlvdXIgc2NvcmUu" },
          { "Increase mindpower: Tame your own ghost!", 
            "SW5jcmVhc2UgbWluZHBvd2VyOiBUYW1lIHlvdXIgb3duIGdob3N0IQ==" },
          { "It furthers one to see the great man.", 
            "SXQgZnVydGhlcnMgb25lIHRvIHNlZSB0aGUgZ3JlYXQgbWFuLg==" },
          { "It's easy to overlook a monster in a wood.", 
            "SXQncyBlYXN5IHRvIG92ZXJsb29rIGEgbW9uc3RlciBpbiBhIHdvb2Qu" },
          { "Just below any trapdoor there may be another one. Just keep falling!", 
            "SnVzdCBiZWxvdyBhbnkgdHJhcGRvb3IgdGhlcmUgbWF5IGJlIGFub3RoZXIgb25lLiBKdXN0IGtlZXAgZmFsbGluZyE=" },
          { "Katanas are very sharp; watch you don't cut yourself.", 
            "S2F0YW5hcyBhcmUgdmVyeSBzaGFycDsgd2F0Y2ggeW91IGRvbid0IGN1dCB5b3Vyc2VsZi4=" },
          { "Keep a clear mind: quaff clear potions.", 
            "S2VlcCBhIGNsZWFyIG1pbmQ6IHF1YWZmIGNsZWFyIHBvdGlvbnMu" },
          { "Kicking the terminal doesn't hurt the monsters.", 
            "S2lja2luZyB0aGUgdGVybWluYWwgZG9lc24ndCBodXJ0IHRoZSBtb25zdGVycy4=" },
          { "Killer bees keep appearing till you kill their queen.", 
            "S2lsbGVyIGJlZXMga2VlcCBhcHBlYXJpbmcgdGlsbCB5b3Uga2lsbCB0aGVpciBxdWVlbi4=" },
          { "Killer bunnies can be tamed with carrots only.", 
            "S2lsbGVyIGJ1bm5pZXMgY2FuIGJlIHRhbWVkIHdpdGggY2Fycm90cyBvbmx5Lg==" },
          { "Latest news? Put `rec.games.roguelike.nethack' in your .newsrc!", 
            "TGF0ZXN0IG5ld3M/IFB1dCBgcmVjLmdhbWVzLnJvZ3VlbGlrZS5uZXRoYWNrJyBpbiB5b3VyIC5uZXdzcmMh" },
          { "Learn how to spell. Play NetHack!", 
            "TGVhcm4gaG93IHRvIHNwZWxsLiBQbGF5IE5ldEhhY2sh" },
          { "Leprechauns hide their gold in a secret room.", 
            "TGVwcmVjaGF1bnMgaGlkZSB0aGVpciBnb2xkIGluIGEgc2VjcmV0IHJvb20u" },
          { "Let your fingers do the walking on the yulkjhnb keys.", 
            "TGV0IHlvdXIgZmluZ2VycyBkbyB0aGUgd2Fsa2luZyBvbiB0aGUgeXVsa2pobmIga2V5cy4=" },
          { "Let's face it: this time you're not going to win.", 
            "TGV0J3MgZmFjZSBpdDogdGhpcyB0aW1lIHlvdSdyZSBub3QgZ29pbmcgdG8gd2luLg==" },
          { "Let's have a party, drink a lot of booze.", 
            "TGV0J3MgaGF2ZSBhIHBhcnR5LCBkcmluayBhIGxvdCBvZiBib296ZS4=" },
          { "Liquor sellers do not drink; they hate to see you twice.", 
            "TGlxdW9yIHNlbGxlcnMgZG8gbm90IGRyaW5rOyB0aGV5IGhhdGUgdG8gc2VlIHlvdSB0d2ljZS4=" },
          { "Lunar eclipse tonight. May as well quit now!", 
            "THVuYXIgZWNsaXBzZSB0b25pZ2h0LiBNYXkgYXMgd2VsbCBxdWl0IG5vdyE=" },
          { "Meeting your own ghost decreases your luck considerably!", 
            "TWVldGluZyB5b3VyIG93biBnaG9zdCBkZWNyZWFzZXMgeW91ciBsdWNrIGNvbnNpZGVyYWJseSE=" },
          { "Money to invest? Take it to the local branch of the Magic Memory Vault!", 
            "TW9uZXkgdG8gaW52ZXN0PyBUYWtlIGl0IHRvIHRoZSBsb2NhbCBicmFuY2ggb2YgdGhlIE1hZ2ljIE1lbW9yeSBWYXVsdCE=" },
          { "Monsters come from nowhere to hit you everywhere.", 
            "TW9uc3RlcnMgY29tZSBmcm9tIG5vd2hlcmUgdG8gaGl0IHlvdSBldmVyeXdoZXJlLg==" },
          { "Monsters sleep because you are boring, not because they ever get tired.", 
            "TW9uc3RlcnMgc2xlZXAgYmVjYXVzZSB5b3UgYXJlIGJvcmluZywgbm90IGJlY2F1c2UgdGhleSBldmVyIGdldCB0aXJlZC4=" },
          { "Most monsters prefer minced meat. That's why they are hitting you!", 
            "TW9zdCBtb25zdGVycyBwcmVmZXIgbWluY2VkIG1lYXQuIFRoYXQncyB3aHkgdGhleSBhcmUgaGl0dGluZyB5b3Uh" },
          { "Most of the bugs in NetHack are on the floor.", 
            "TW9zdCBvZiB0aGUgYnVncyBpbiBOZXRIYWNrIGFyZSBvbiB0aGUgZmxvb3Iu" },
          { "Much ado Nothing Happens.", 
            "TXVjaCBhZG8gTm90aGluZyBIYXBwZW5zLg==" },
          { "Multi-player NetHack is a myth.", 
            "TXVsdGktcGxheWVyIE5ldEhhY2sgaXMgYSBteXRoLg==" },
          { "NetHack is addictive. Too late, you're already hooked.", 
            "TmV0SGFjayBpcyBhZGRpY3RpdmUuIFRvbyBsYXRlLCB5b3UncmUgYWxyZWFkeSBob29rZWQu" },
          { "Never ask a shopkeeper for a price list.", 
            "TmV2ZXIgYXNrIGEgc2hvcGtlZXBlciBmb3IgYSBwcmljZSBsaXN0Lg==" },
          { "Never burn a tree, unless you like getting whacked with a +5 shovel.", 
            "TmV2ZXIgYnVybiBhIHRyZWUsIHVubGVzcyB5b3UgbGlrZSBnZXR0aW5nIHdoYWNrZWQgd2l0aCBhICs1IHNob3ZlbC4=" },
          { "Never eat with glowing hands!", 
            "TmV2ZXIgZWF0IHdpdGggZ2xvd2luZyBoYW5kcyE=" },
          { "Never mind the monsters hitting you: they just replace the charwomen.", 
            "TmV2ZXIgbWluZCB0aGUgbW9uc3RlcnMgaGl0dGluZyB5b3U6IHRoZXkganVzdCByZXBsYWNlIHRoZSBjaGFyd29tZW4u" },
          { "Never play leapfrog with a unicorn.", 
            "TmV2ZXIgcGxheSBsZWFwZnJvZyB3aXRoIGEgdW5pY29ybi4=" },
          { "Never step on a cursed engraving.", 
            "TmV2ZXIgc3RlcCBvbiBhIGN1cnNlZCBlbmdyYXZpbmcu" },
          { "Never swim with a camera: there's nothing to take pictures of.", 
            "TmV2ZXIgc3dpbSB3aXRoIGEgY2FtZXJhOiB0aGVyZSdzIG5vdGhpbmcgdG8gdGFrZSBwaWN0dXJlcyBvZi4=" },
          { "Never teach your pet rust monster to fetch.", 
            "TmV2ZXIgdGVhY2ggeW91ciBwZXQgcnVzdCBtb25zdGVyIHRvIGZldGNoLg==" },
          { "Never trust a random generator in magic fields.", 
            "TmV2ZXIgdHJ1c3QgYSByYW5kb20gZ2VuZXJhdG9yIGluIG1hZ2ljIGZpZWxkcy4=" },
          { "Never use a wand of death.", 
            "TmV2ZXIgdXNlIGEgd2FuZCBvZiBkZWF0aC4=" },
          { "No level contains two shops. The maze is no level. So...", 
            "Tm8gbGV2ZWwgY29udGFpbnMgdHdvIHNob3BzLiBUaGUgbWF6ZSBpcyBubyBsZXZlbC4gU28uLi4=" },
          { "No part of this fortune may be reproduced, stored in a retrieval system, ...", 
            "Tm8gcGFydCBvZiB0aGlzIGZvcnR1bmUgbWF5IGJlIHJlcHJvZHVjZWQsIHN0b3JlZCBpbiBhIHJldHJpZXZhbCBzeXN0ZW0sIC4uLg==" },
          { "Not all rumors are as misleading as this one.", 
            "Tm90IGFsbCBydW1vcnMgYXJlIGFzIG1pc2xlYWRpbmcgYXMgdGhpcyBvbmUu" },
          { "Nymphs and nurses like beautiful rings.", 
            "TnltcGhzIGFuZCBudXJzZXMgbGlrZSBiZWF1dGlmdWwgcmluZ3Mu" },
          { "Nymphs are blondes. Are you a gentleman?", 
            "TnltcGhzIGFyZSBibG9uZGVzLiBBcmUgeW91IGEgZ2VudGxlbWFuPw==" },
          { "Offering a unicorn a worthless piece of glass might prove to be fatal!", 
            "T2ZmZXJpbmcgYSB1bmljb3JuIGEgd29ydGhsZXNzIHBpZWNlIG9mIGdsYXNzIG1pZ2h0IHByb3ZlIHRvIGJlIGZhdGFsIQ==" },
          { "Old hackers never die: young ones do.", 
            "T2xkIGhhY2tlcnMgbmV2ZXIgZGllOiB5b3VuZyBvbmVzIGRvLg==" },
          { "One has to leave shops before closing time.", 
            "T25lIGhhcyB0byBsZWF2ZSBzaG9wcyBiZWZvcmUgY2xvc2luZyB0aW1lLg==" },
          { "One homunculus a day keeps the doctor away.", 
            "T25lIGhvbXVuY3VsdXMgYSBkYXkga2VlcHMgdGhlIGRvY3RvciBhd2F5Lg==" },
          { "One level further down somebody is getting killed, right now.", 
            "T25lIGxldmVsIGZ1cnRoZXIgZG93biBzb21lYm9keSBpcyBnZXR0aW5nIGtpbGxlZCwgcmlnaHQgbm93Lg==" },
          { "Only a wizard can use a magic whistle.", 
            "T25seSBhIHdpemFyZCBjYW4gdXNlIGEgbWFnaWMgd2hpc3RsZS4=" },
          { "Only adventurers of evil alignment think of killing their dog.", 
            "T25seSBhZHZlbnR1cmVycyBvZiBldmlsIGFsaWdubWVudCB0aGluayBvZiBraWxsaW5nIHRoZWlyIGRvZy4=" },
          { "Only chaotic evils kill sleeping monsters.", 
            "T25seSBjaGFvdGljIGV2aWxzIGtpbGwgc2xlZXBpbmcgbW9uc3RlcnMu" },
          { "Only real trappers escape traps.", 
            "T25seSByZWFsIHRyYXBwZXJzIGVzY2FwZSB0cmFwcy4=" },
          { "Only real wizards can write scrolls.", 
            "T25seSByZWFsIHdpemFyZHMgY2FuIHdyaXRlIHNjcm9sbHMu" },
          { "Operation OVERKILL has started now.", 
            "T3BlcmF0aW9uIE9WRVJLSUxMIGhhcyBzdGFydGVkIG5vdy4=" },
          { "PLEASE ignore previous rumor.", 
            "UExFQVNFIGlnbm9yZSBwcmV2aW91cyBydW1vci4=" },
          { "Polymorph into an ettin; meet your opponents face to face to face.", 
            "UG9seW1vcnBoIGludG8gYW4gZXR0aW47IG1lZXQgeW91ciBvcHBvbmVudHMgZmFjZSB0byBmYWNlIHRvIGZhY2Uu" },
          { "Praying will frighten demons.", 
            "UHJheWluZyB3aWxsIGZyaWdodGVuIGRlbW9ucy4=" },
          { "Row (3x) that boat gently down the stream, Charon (4x), death is but a dream.", 
            "Um93ICgzeCkgdGhhdCBib2F0IGdlbnRseSBkb3duIHRoZSBzdHJlYW0sIENoYXJvbiAoNHgpLCBkZWF0aCBpcyBidXQgYSBkcmVhbS4=" },
          { "Running is good for your legs.", 
            "UnVubmluZyBpcyBnb29kIGZvciB5b3VyIGxlZ3Mu" },
          { "Screw up your courage! You've screwed up everything else.", 
            "U2NyZXcgdXAgeW91ciBjb3VyYWdlISBZb3UndmUgc2NyZXdlZCB1cCBldmVyeXRoaW5nIGVsc2Uu" },
          { "Seepage? Leaky pipes? Rising damp? Summon the plumber!", 
            "U2VlcGFnZT8gTGVha3kgcGlwZXM/IFJpc2luZyBkYW1wPyBTdW1tb24gdGhlIHBsdW1iZXIh" },
          { "Segmentation fault (core dumped).", 
            "U2VnbWVudGF0aW9uIGZhdWx0IChjb3JlIGR1bXBlZCku" },
          { "Shopkeepers sometimes die from old age.", 
            "U2hvcGtlZXBlcnMgc29tZXRpbWVzIGRpZSBmcm9tIG9sZCBhZ2Uu" },
          { "Some mazes (especially small ones) have no solutions, says man 6 maze.", 
            "U29tZSBtYXplcyAoZXNwZWNpYWxseSBzbWFsbCBvbmVzKSBoYXZlIG5vIHNvbHV0aW9ucywgc2F5cyBtYW4gNiBtYXplLg==" },
          { "Some questions the Sphynx asks just *don't* have any answers.", 
            "U29tZSBxdWVzdGlvbnMgdGhlIFNwaHlueCBhc2tzIGp1c3QgKmRvbid0KiBoYXZlIGFueSBhbnN3ZXJzLg==" },
          { "Sometimes \"mu\" is the answer.", 
            "U29tZXRpbWVzICJtdSIgaXMgdGhlIGFuc3dlci4=" },
          { "Sorry, no fortune this time. Better luck next cookie!", 
            "U29ycnksIG5vIGZvcnR1bmUgdGhpcyB0aW1lLiBCZXR0ZXIgbHVjayBuZXh0IGNvb2tpZSE=" },
          { "Spare your scrolls of make-edible until it's really necessary!", 
            "U3BhcmUgeW91ciBzY3JvbGxzIG9mIG1ha2UtZWRpYmxlIHVudGlsIGl0J3MgcmVhbGx5IG5lY2Vzc2FyeSE=" },
          { "Suddenly, the dungeon will collapse...", 
            "U3VkZGVubHksIHRoZSBkdW5nZW9uIHdpbGwgY29sbGFwc2UuLi4=" },
          { "Taming a mail daemon may cause a system security violation.", 
            "VGFtaW5nIGEgbWFpbCBkYWVtb24gbWF5IGNhdXNlIGEgc3lzdGVtIHNlY3VyaXR5IHZpb2xhdGlvbi4=" },
          { "The crowd was so tough, the Stooges won't play the Dungeon anymore, nyuk nyuk.", 
            "VGhlIGNyb3dkIHdhcyBzbyB0b3VnaCwgdGhlIFN0b29nZXMgd29uJ3QgcGxheSB0aGUgRHVuZ2VvbiBhbnltb3JlLCBueXVrIG55dWsu" },
          { "The leprechauns hide their treasure in a small hidden room.", 
            "VGhlIGxlcHJlY2hhdW5zIGhpZGUgdGhlaXIgdHJlYXN1cmUgaW4gYSBzbWFsbCBoaWRkZW4gcm9vbS4=" },
          { "The longer the wand the better.", 
            "VGhlIGxvbmdlciB0aGUgd2FuZCB0aGUgYmV0dGVyLg==" },
          { "The magic word is \"XYZZY\".", 
            "VGhlIG1hZ2ljIHdvcmQgaXMgIlhZWlpZIi4=" },
          { "The meek shall inherit your bones files.", 
            "VGhlIG1lZWsgc2hhbGwgaW5oZXJpdCB5b3VyIGJvbmVzIGZpbGVzLg==" },
          { "The mines are dark and deep, and I have levels to go before I sleep.", 
            "VGhlIG1pbmVzIGFyZSBkYXJrIGFuZCBkZWVwLCBhbmQgSSBoYXZlIGxldmVscyB0byBnbyBiZWZvcmUgSSBzbGVlcC4=" },
          { "The use of dynamite is dangerous.", 
            "VGhlIHVzZSBvZiBkeW5hbWl0ZSBpcyBkYW5nZXJvdXMu" },
          { "There are no worms in the UNIX version.", 
            "VGhlcmUgYXJlIG5vIHdvcm1zIGluIHRoZSBVTklYIHZlcnNpb24u" },
          { "There is a trap on this level!", 
            "VGhlcmUgaXMgYSB0cmFwIG9uIHRoaXMgbGV2ZWwh" },
          { "They say that Demogorgon, Asmodeus, Orcus, Yeenoghu & Juiblex is no law firm.", 
            "VGhleSBzYXkgdGhhdCBEZW1vZ29yZ29uLCBBc21vZGV1cywgT3JjdXMsIFllZW5vZ2h1ICYgSnVpYmxleCBpcyBubyBsYXcgZmlybS4=" },
          { "They say that Geryon has an evil twin, beware!", 
            "VGhleSBzYXkgdGhhdCBHZXJ5b24gaGFzIGFuIGV2aWwgdHdpbiwgYmV3YXJlIQ==" },
          { "They say that Medusa would make a terrible pet.", 
            "VGhleSBzYXkgdGhhdCBNZWR1c2Egd291bGQgbWFrZSBhIHRlcnJpYmxlIHBldC4=" },
          { "They say that NetHack bugs are Seldon planned.", 
            "VGhleSBzYXkgdGhhdCBOZXRIYWNrIGJ1Z3MgYXJlIFNlbGRvbiBwbGFubmVkLg==" },
          { "They say that NetHack comes in 256 flavors.", 
            "VGhleSBzYXkgdGhhdCBOZXRIYWNrIGNvbWVzIGluIDI1NiBmbGF2b3JzLg==" },
          { "They say that NetHack is just a computer game.", 
            "VGhleSBzYXkgdGhhdCBOZXRIYWNrIGlzIGp1c3QgYSBjb21wdXRlciBnYW1lLg==" },
          { "They say that NetHack is more than just a computer game.", 
            "VGhleSBzYXkgdGhhdCBOZXRIYWNrIGlzIG1vcmUgdGhhbiBqdXN0IGEgY29tcHV0ZXIgZ2FtZS4=" },
          { "They say that NetHack is never what it used to be.", 
            "VGhleSBzYXkgdGhhdCBOZXRIYWNrIGlzIG5ldmVyIHdoYXQgaXQgdXNlZCB0byBiZS4=" },
          { "They say that a baby dragon is too small to hurt or help you.", 
            "VGhleSBzYXkgdGhhdCBhIGJhYnkgZHJhZ29uIGlzIHRvbyBzbWFsbCB0byBodXJ0IG9yIGhlbHAgeW91Lg==" },
          { "They say that a black pudding is simply a brown pudding gone bad.", 
            "VGhleSBzYXkgdGhhdCBhIGJsYWNrIHB1ZGRpbmcgaXMgc2ltcGx5IGEgYnJvd24gcHVkZGluZyBnb25lIGJhZC4=" },
          { "They say that a black sheep has 3 bags full of wool.", 
            "VGhleSBzYXkgdGhhdCBhIGJsYWNrIHNoZWVwIGhhcyAzIGJhZ3MgZnVsbCBvZiB3b29sLg==" },
          { "They say that a blank scroll is like a blank check.", 
            "VGhleSBzYXkgdGhhdCBhIGJsYW5rIHNjcm9sbCBpcyBsaWtlIGEgYmxhbmsgY2hlY2su" },
          { "They say that a cat named Morris has nine lives.", 
            "VGhleSBzYXkgdGhhdCBhIGNhdCBuYW1lZCBNb3JyaXMgaGFzIG5pbmUgbGl2ZXMu" },
          { "They say that a desperate shopper might pay any price in a shop.", 
            "VGhleSBzYXkgdGhhdCBhIGRlc3BlcmF0ZSBzaG9wcGVyIG1pZ2h0IHBheSBhbnkgcHJpY2UgaW4gYSBzaG9wLg==" },
          { "They say that a diamond dog is everybody's best friend.", 
            "VGhleSBzYXkgdGhhdCBhIGRpYW1vbmQgZG9nIGlzIGV2ZXJ5Ym9keSdzIGJlc3QgZnJpZW5kLg==" },
          { "They say that a dwarf lord can carry a pick-axe because his armor is light.", 
            "VGhleSBzYXkgdGhhdCBhIGR3YXJmIGxvcmQgY2FuIGNhcnJ5IGEgcGljay1heGUgYmVjYXVzZSBoaXMgYXJtb3IgaXMgbGlnaHQu" },
          { "They say that a floating eye can defeat Medusa.", 
            "VGhleSBzYXkgdGhhdCBhIGZsb2F0aW5nIGV5ZSBjYW4gZGVmZWF0IE1lZHVzYS4=" },
          { "They say that a fortune only has 1 line and you can't read between it.", 
            "VGhleSBzYXkgdGhhdCBhIGZvcnR1bmUgb25seSBoYXMgMSBsaW5lIGFuZCB5b3UgY2FuJ3QgcmVhZCBiZXR3ZWVuIGl0Lg==" },
          { "They say that a fortune only has 1 line, but you can read between it.", 
            "VGhleSBzYXkgdGhhdCBhIGZvcnR1bmUgb25seSBoYXMgMSBsaW5lLCBidXQgeW91IGNhbiByZWFkIGJldHdlZW4gaXQu" },
          { "They say that a fountain looks nothing like a regularly erupting geyser.", 
            "VGhleSBzYXkgdGhhdCBhIGZvdW50YWluIGxvb2tzIG5vdGhpbmcgbGlrZSBhIHJlZ3VsYXJseSBlcnVwdGluZyBnZXlzZXIu" },
          { "They say that a gold doubloon is worth more than its weight in gold.", 
            "VGhleSBzYXkgdGhhdCBhIGdvbGQgZG91Ymxvb24gaXMgd29ydGggbW9yZSB0aGFuIGl0cyB3ZWlnaHQgaW4gZ29sZC4=" },
          { "They say that a grid bug won't pay a shopkeeper for zapping you in a shop.", 
            "VGhleSBzYXkgdGhhdCBhIGdyaWQgYnVnIHdvbid0IHBheSBhIHNob3BrZWVwZXIgZm9yIHphcHBpbmcgeW91IGluIGEgc2hvcC4=" },
          { "They say that a gypsy could tell your fortune for a price.", 
            "VGhleSBzYXkgdGhhdCBhIGd5cHN5IGNvdWxkIHRlbGwgeW91ciBmb3J0dW5lIGZvciBhIHByaWNlLg==" },
          { "They say that a hacker named Alice once level teleported by using a mirror.", 
            "VGhleSBzYXkgdGhhdCBhIGhhY2tlciBuYW1lZCBBbGljZSBvbmNlIGxldmVsIHRlbGVwb3J0ZWQgYnkgdXNpbmcgYSBtaXJyb3Iu" },
          { "They say that a hacker named David once slew a giant with a sling and a rock.", 
            "VGhleSBzYXkgdGhhdCBhIGhhY2tlciBuYW1lZCBEYXZpZCBvbmNlIHNsZXcgYSBnaWFudCB3aXRoIGEgc2xpbmcgYW5kIGEgcm9jay4=" },
          { "They say that a hacker named Dorothy once rode a fog cloud to Oz.", 
            "VGhleSBzYXkgdGhhdCBhIGhhY2tlciBuYW1lZCBEb3JvdGh5IG9uY2Ugcm9kZSBhIGZvZyBjbG91ZCB0byBPei4=" },
          { "They say that a hacker named Mary once lost a white sheep in the mazes.", 
            "VGhleSBzYXkgdGhhdCBhIGhhY2tlciBuYW1lZCBNYXJ5IG9uY2UgbG9zdCBhIHdoaXRlIHNoZWVwIGluIHRoZSBtYXplcy4=" },
          { "They say that a helm of brilliance is not to be taken lightly.", 
            "VGhleSBzYXkgdGhhdCBhIGhlbG0gb2YgYnJpbGxpYW5jZSBpcyBub3QgdG8gYmUgdGFrZW4gbGlnaHRseS4=" },
          { "They say that a hot dog and a hell hound are the same thing.", 
            "VGhleSBzYXkgdGhhdCBhIGhvdCBkb2cgYW5kIGEgaGVsbCBob3VuZCBhcmUgdGhlIHNhbWUgdGhpbmcu" },
          { "They say that a lamp named Aladdin's Lamp contains a djinni with 3 wishes.", 
            "VGhleSBzYXkgdGhhdCBhIGxhbXAgbmFtZWQgQWxhZGRpbidzIExhbXAgY29udGFpbnMgYSBkamlubmkgd2l0aCAzIHdpc2hlcy4=" },
          { "They say that a large dog named Lassie will lead you to the amulet.", 
            "VGhleSBzYXkgdGhhdCBhIGxhcmdlIGRvZyBuYW1lZCBMYXNzaWUgd2lsbCBsZWFkIHlvdSB0byB0aGUgYW11bGV0Lg==" },
          { "They say that a long sword is not a light sword.", 
            "VGhleSBzYXkgdGhhdCBhIGxvbmcgc3dvcmQgaXMgbm90IGEgbGlnaHQgc3dvcmQu" },
          { "They say that a manes won't mince words with you.", 
            "VGhleSBzYXkgdGhhdCBhIG1hbmVzIHdvbid0IG1pbmNlIHdvcmRzIHdpdGggeW91Lg==" },
          { "They say that a mind is a terrible thing to waste.", 
            "VGhleSBzYXkgdGhhdCBhIG1pbmQgaXMgYSB0ZXJyaWJsZSB0aGluZyB0byB3YXN0ZS4=" },
          { "They say that a plain nymph will only wear a wire ring in one ear.", 
            "VGhleSBzYXkgdGhhdCBhIHBsYWluIG55bXBoIHdpbGwgb25seSB3ZWFyIGEgd2lyZSByaW5nIGluIG9uZSBlYXIu" },
          { "They say that a plumed hat could be a previously used crested helmet.", 
            "VGhleSBzYXkgdGhhdCBhIHBsdW1lZCBoYXQgY291bGQgYmUgYSBwcmV2aW91c2x5IHVzZWQgY3Jlc3RlZCBoZWxtZXQu" },
          { "They say that a potion of oil is difficult to grasp.", 
            "VGhleSBzYXkgdGhhdCBhIHBvdGlvbiBvZiBvaWwgaXMgZGlmZmljdWx0IHRvIGdyYXNwLg==" },
          { "They say that a potion of yogurt is a cancelled potion of sickness.", 
            "VGhleSBzYXkgdGhhdCBhIHBvdGlvbiBvZiB5b2d1cnQgaXMgYSBjYW5jZWxsZWQgcG90aW9uIG9mIHNpY2tuZXNzLg==" },
          { "They say that a purple worm is not a baby purple dragon.", 
            "VGhleSBzYXkgdGhhdCBhIHB1cnBsZSB3b3JtIGlzIG5vdCBhIGJhYnkgcHVycGxlIGRyYWdvbi4=" },
          { "They say that a quivering blob tastes different than a gelatinous cube.", 
            "VGhleSBzYXkgdGhhdCBhIHF1aXZlcmluZyBibG9iIHRhc3RlcyBkaWZmZXJlbnQgdGhhbiBhIGdlbGF0aW5vdXMgY3ViZS4=" },
          { "They say that a runed broadsword named Stormbringer attracts vortices.", 
            "VGhleSBzYXkgdGhhdCBhIHJ1bmVkIGJyb2Fkc3dvcmQgbmFtZWQgU3Rvcm1icmluZ2VyIGF0dHJhY3RzIHZvcnRpY2VzLg==" },
          { "They say that a scroll of summoning has other names.", 
            "VGhleSBzYXkgdGhhdCBhIHNjcm9sbCBvZiBzdW1tb25pbmcgaGFzIG90aGVyIG5hbWVzLg==" },
          { "They say that a shaman can bestow blessings but usually doesn't.", 
            "VGhleSBzYXkgdGhhdCBhIHNoYW1hbiBjYW4gYmVzdG93IGJsZXNzaW5ncyBidXQgdXN1YWxseSBkb2Vzbid0Lg==" },
          { "They say that a shaman will bless you for an eye of newt and wing of bat.", 
            "VGhleSBzYXkgdGhhdCBhIHNoYW1hbiB3aWxsIGJsZXNzIHlvdSBmb3IgYW4gZXllIG9mIG5ld3QgYW5kIHdpbmcgb2YgYmF0Lg==" },
          { "They say that a shimmering gold shield is not a polished silver shield.", 
            "VGhleSBzYXkgdGhhdCBhIHNoaW1tZXJpbmcgZ29sZCBzaGllbGQgaXMgbm90IGEgcG9saXNoZWQgc2lsdmVyIHNoaWVsZC4=" },
          { "They say that a spear will hit a neo-otyugh. (Do YOU know what that is?)", 
            "VGhleSBzYXkgdGhhdCBhIHNwZWFyIHdpbGwgaGl0IGEgbmVvLW90eXVnaC4gKERvIFlPVSBrbm93IHdoYXQgdGhhdCBpcz8p" },
          { "They say that a spotted dragon is the ultimate shape changer.", 
            "VGhleSBzYXkgdGhhdCBhIHNwb3R0ZWQgZHJhZ29uIGlzIHRoZSB1bHRpbWF0ZSBzaGFwZSBjaGFuZ2VyLg==" },
          { "They say that a stethoscope is no good if you can only hear your heartbeat.", 
            "VGhleSBzYXkgdGhhdCBhIHN0ZXRob3Njb3BlIGlzIG5vIGdvb2QgaWYgeW91IGNhbiBvbmx5IGhlYXIgeW91ciBoZWFydGJlYXQu" },
          { "They say that a succubus named Suzy will sometimes warn you of danger.", 
            "VGhleSBzYXkgdGhhdCBhIHN1Y2N1YnVzIG5hbWVkIFN1enkgd2lsbCBzb21ldGltZXMgd2FybiB5b3Ugb2YgZGFuZ2VyLg==" },
          { "They say that a wand of cancellation is not like a wand of polymorph.", 
            "VGhleSBzYXkgdGhhdCBhIHdhbmQgb2YgY2FuY2VsbGF0aW9uIGlzIG5vdCBsaWtlIGEgd2FuZCBvZiBwb2x5bW9ycGgu" },
          { "They say that a wood golem named Pinocchio would be easy to control.", 
            "VGhleSBzYXkgdGhhdCBhIHdvb2QgZ29sZW0gbmFtZWQgUGlub2NjaGlvIHdvdWxkIGJlIGVhc3kgdG8gY29udHJvbC4=" },
          { "They say that after killing a dragon it's time for a change of scenery.", 
            "VGhleSBzYXkgdGhhdCBhZnRlciBraWxsaW5nIGEgZHJhZ29uIGl0J3MgdGltZSBmb3IgYSBjaGFuZ2Ugb2Ygc2NlbmVyeS4=" },
          { "They say that an amulet of strangulation is worse than ring around the collar.", 
            "VGhleSBzYXkgdGhhdCBhbiBhbXVsZXQgb2Ygc3RyYW5ndWxhdGlvbiBpcyB3b3JzZSB0aGFuIHJpbmcgYXJvdW5kIHRoZSBjb2xsYXIu" },
          { "They say that an attic is the best place to hide your toys.", 
            "VGhleSBzYXkgdGhhdCBhbiBhdHRpYyBpcyB0aGUgYmVzdCBwbGFjZSB0byBoaWRlIHlvdXIgdG95cy4=" },
          { "They say that an axe named Cleaver once belonged to a hacker named Beaver.", 
            "VGhleSBzYXkgdGhhdCBhbiBheGUgbmFtZWQgQ2xlYXZlciBvbmNlIGJlbG9uZ2VkIHRvIGEgaGFja2VyIG5hbWVkIEJlYXZlci4=" },
          { "They say that an eye of newt and a wing of bat are double the trouble.", 
            "VGhleSBzYXkgdGhhdCBhbiBleWUgb2YgbmV3dCBhbmQgYSB3aW5nIG9mIGJhdCBhcmUgZG91YmxlIHRoZSB0cm91YmxlLg==" },
          { "They say that an incubus named Izzy sometimes makes women feel sensitive.", 
            "VGhleSBzYXkgdGhhdCBhbiBpbmN1YnVzIG5hbWVkIEl6enkgc29tZXRpbWVzIG1ha2VzIHdvbWVuIGZlZWwgc2Vuc2l0aXZlLg==" },
          { "They say that an opulent throne room is rarely a place to wish you'd be in.", 
            "VGhleSBzYXkgdGhhdCBhbiBvcHVsZW50IHRocm9uZSByb29tIGlzIHJhcmVseSBhIHBsYWNlIHRvIHdpc2ggeW91J2QgYmUgaW4u" },
          { "They say that an unlucky hacker once had a nose bleed at an altar and died.", 
            "VGhleSBzYXkgdGhhdCBhbiB1bmx1Y2t5IGhhY2tlciBvbmNlIGhhZCBhIG5vc2UgYmxlZWQgYXQgYW4gYWx0YXIgYW5kIGRpZWQu" },
          { "They say that and they say this but they never say never, never!", 
            "VGhleSBzYXkgdGhhdCBhbmQgdGhleSBzYXkgdGhpcyBidXQgdGhleSBuZXZlciBzYXkgbmV2ZXIsIG5ldmVyIQ==" },
          { "They say that any quantum mechanic knows that speed kills.", 
            "VGhleSBzYXkgdGhhdCBhbnkgcXVhbnR1bSBtZWNoYW5pYyBrbm93cyB0aGF0IHNwZWVkIGtpbGxzLg==" },
          { "They say that applying a unicorn horn means you've missed the point.", 
            "VGhleSBzYXkgdGhhdCBhcHBseWluZyBhIHVuaWNvcm4gaG9ybiBtZWFucyB5b3UndmUgbWlzc2VkIHRoZSBwb2ludC4=" },
          { "They say that blue stones are radioactive, beware.", 
            "VGhleSBzYXkgdGhhdCBibHVlIHN0b25lcyBhcmUgcmFkaW9hY3RpdmUsIGJld2FyZS4=" },
          { "They say that building a dungeon is a team effort.", 
            "VGhleSBzYXkgdGhhdCBidWlsZGluZyBhIGR1bmdlb24gaXMgYSB0ZWFtIGVmZm9ydC4=" },
          { "They say that chaotic characters never get a kick out of altars.", 
            "VGhleSBzYXkgdGhhdCBjaGFvdGljIGNoYXJhY3RlcnMgbmV2ZXIgZ2V0IGEga2ljayBvdXQgb2YgYWx0YXJzLg==" },
          { "They say that collapsing a dungeon often creates a panic.", 
            "VGhleSBzYXkgdGhhdCBjb2xsYXBzaW5nIGEgZHVuZ2VvbiBvZnRlbiBjcmVhdGVzIGEgcGFuaWMu" },
          { "They say that counting your eggs before they hatch shows that you care.", 
            "VGhleSBzYXkgdGhhdCBjb3VudGluZyB5b3VyIGVnZ3MgYmVmb3JlIHRoZXkgaGF0Y2ggc2hvd3MgdGhhdCB5b3UgY2FyZS4=" },
          { "They say that dipping a bag of tricks in a fountain won't make it an icebox.", 
            "VGhleSBzYXkgdGhhdCBkaXBwaW5nIGEgYmFnIG9mIHRyaWNrcyBpbiBhIGZvdW50YWluIHdvbid0IG1ha2UgaXQgYW4gaWNlYm94Lg==" },
          { "They say that dipping an eel and brown mold in hot water makes bouillabaisse.", 
            "VGhleSBzYXkgdGhhdCBkaXBwaW5nIGFuIGVlbCBhbmQgYnJvd24gbW9sZCBpbiBob3Qgd2F0ZXIgbWFrZXMgYm91aWxsYWJhaXNzZS4=" },
          { "They say that donating a doubloon is extremely pious charity.", 
            "VGhleSBzYXkgdGhhdCBkb25hdGluZyBhIGRvdWJsb29uIGlzIGV4dHJlbWVseSBwaW91cyBjaGFyaXR5Lg==" },
          { "They say that eating royal jelly attracts grizzly owlbears.", 
            "VGhleSBzYXkgdGhhdCBlYXRpbmcgcm95YWwgamVsbHkgYXR0cmFjdHMgZ3JpenpseSBvd2xiZWFycy4=" },
          { "They say that eggs, pancakes and juice are just a mundane breakfast.", 
            "VGhleSBzYXkgdGhhdCBlZ2dzLCBwYW5jYWtlcyBhbmQganVpY2UgYXJlIGp1c3QgYSBtdW5kYW5lIGJyZWFrZmFzdC4=" },
          { "They say that everyone knows why Medusa stands alone in the dark.", 
            "VGhleSBzYXkgdGhhdCBldmVyeW9uZSBrbm93cyB3aHkgTWVkdXNhIHN0YW5kcyBhbG9uZSBpbiB0aGUgZGFyay4=" },
          { "They say that everyone wanted rec.games.hack to undergo a name change.", 
            "VGhleSBzYXkgdGhhdCBldmVyeW9uZSB3YW50ZWQgcmVjLmdhbWVzLmhhY2sgdG8gdW5kZXJnbyBhIG5hbWUgY2hhbmdlLg==" },
          { "They say that finding a winning strategy is a deliberate move on your part.", 
            "VGhleSBzYXkgdGhhdCBmaW5kaW5nIGEgd2lubmluZyBzdHJhdGVneSBpcyBhIGRlbGliZXJhdGUgbW92ZSBvbiB5b3VyIHBhcnQu" },
          { "They say that finding worthless glass is worth something.", 
            "VGhleSBzYXkgdGhhdCBmaW5kaW5nIHdvcnRobGVzcyBnbGFzcyBpcyB3b3J0aCBzb21ldGhpbmcu" },
          { "They say that fortune cookies are food for thought.", 
            "VGhleSBzYXkgdGhhdCBmb3J0dW5lIGNvb2tpZXMgYXJlIGZvb2QgZm9yIHRob3VnaHQu" },
          { "They say that gold is only wasted on a pet dragon.", 
            "VGhleSBzYXkgdGhhdCBnb2xkIGlzIG9ubHkgd2FzdGVkIG9uIGEgcGV0IGRyYWdvbi4=" },
          { "They say that good things come to those that wait.", 
            "VGhleSBzYXkgdGhhdCBnb29kIHRoaW5ncyBjb21lIHRvIHRob3NlIHRoYXQgd2FpdC4=" },
          { "They say that greased objects will slip out of monsters' hands.", 
            "VGhleSBzYXkgdGhhdCBncmVhc2VkIG9iamVjdHMgd2lsbCBzbGlwIG91dCBvZiBtb25zdGVycycgaGFuZHMu" },
          { "They say that if you can't spell then you'll wish you had a spell book.", 
            "VGhleSBzYXkgdGhhdCBpZiB5b3UgY2FuJ3Qgc3BlbGwgdGhlbiB5b3UnbGwgd2lzaCB5b3UgaGFkIGEgc3BlbGwgYm9vay4=" },
          { "They say that if you live by the sword, you'll die by the sword.", 
            "VGhleSBzYXkgdGhhdCBpZiB5b3UgbGl2ZSBieSB0aGUgc3dvcmQsIHlvdSdsbCBkaWUgYnkgdGhlIHN3b3JkLg==" },
          { "They say that if you play like a monster you'll have a better game.", 
            "VGhleSBzYXkgdGhhdCBpZiB5b3UgcGxheSBsaWtlIGEgbW9uc3RlciB5b3UnbGwgaGF2ZSBhIGJldHRlciBnYW1lLg==" },
          { "They say that if you sleep with a demon you might awake with a headache.", 
            "VGhleSBzYXkgdGhhdCBpZiB5b3Ugc2xlZXAgd2l0aCBhIGRlbW9uIHlvdSBtaWdodCBhd2FrZSB3aXRoIGEgaGVhZGFjaGUu" },
          { "They say that if you step on a crack you could break your mother's back.", 
            "VGhleSBzYXkgdGhhdCBpZiB5b3Ugc3RlcCBvbiBhIGNyYWNrIHlvdSBjb3VsZCBicmVhayB5b3VyIG1vdGhlcidzIGJhY2su" },
          { "They say that if you're invisible you can still be heard!", 
            "VGhleSBzYXkgdGhhdCBpZiB5b3UncmUgaW52aXNpYmxlIHlvdSBjYW4gc3RpbGwgYmUgaGVhcmQh" },
          { "They say that if you're lucky you can feel the runes on a scroll.", 
            "VGhleSBzYXkgdGhhdCBpZiB5b3UncmUgbHVja3kgeW91IGNhbiBmZWVsIHRoZSBydW5lcyBvbiBhIHNjcm9sbC4=" },
          { "They say that in the big picture gold is only small change.", 
            "VGhleSBzYXkgdGhhdCBpbiB0aGUgYmlnIHBpY3R1cmUgZ29sZCBpcyBvbmx5IHNtYWxsIGNoYW5nZS4=" },
          { "They say that in the dungeon it's not what you know that really matters.", 
            "VGhleSBzYXkgdGhhdCBpbiB0aGUgZHVuZ2VvbiBpdCdzIG5vdCB3aGF0IHlvdSBrbm93IHRoYXQgcmVhbGx5IG1hdHRlcnMu" },
          { "They say that in the dungeon moon rocks are really dilithium crystals.", 
            "VGhleSBzYXkgdGhhdCBpbiB0aGUgZHVuZ2VvbiBtb29uIHJvY2tzIGFyZSByZWFsbHkgZGlsaXRoaXVtIGNyeXN0YWxzLg==" },
          { "They say that in the dungeon the boorish customer is never right.", 
            "VGhleSBzYXkgdGhhdCBpbiB0aGUgZHVuZ2VvbiB0aGUgYm9vcmlzaCBjdXN0b21lciBpcyBuZXZlciByaWdodC4=" },
          { "They say that in the dungeon you don't need a watch to tell time.", 
            "VGhleSBzYXkgdGhhdCBpbiB0aGUgZHVuZ2VvbiB5b3UgZG9uJ3QgbmVlZCBhIHdhdGNoIHRvIHRlbGwgdGltZS4=" },
          { "They say that in the dungeon you need something old, new, burrowed and blue.", 
            "VGhleSBzYXkgdGhhdCBpbiB0aGUgZHVuZ2VvbiB5b3UgbmVlZCBzb21ldGhpbmcgb2xkLCBuZXcsIGJ1cnJvd2VkIGFuZCBibHVlLg==" },
          { "They say that in the dungeon you should always count your blessings.", 
            "VGhleSBzYXkgdGhhdCBpbiB0aGUgZHVuZ2VvbiB5b3Ugc2hvdWxkIGFsd2F5cyBjb3VudCB5b3VyIGJsZXNzaW5ncy4=" },
          { "They say that iron golem plate mail isn't worth wishing for.", 
            "VGhleSBzYXkgdGhhdCBpcm9uIGdvbGVtIHBsYXRlIG1haWwgaXNuJ3Qgd29ydGggd2lzaGluZyBmb3Iu" },
          { "They say that it takes four quarterstaffs to make one staff.", 
            "VGhleSBzYXkgdGhhdCBpdCB0YWtlcyBmb3VyIHF1YXJ0ZXJzdGFmZnMgdG8gbWFrZSBvbmUgc3RhZmYu" },
          { "They say that it's not over till the fat ladies sing.", 
            "VGhleSBzYXkgdGhhdCBpdCdzIG5vdCBvdmVyIHRpbGwgdGhlIGZhdCBsYWRpZXMgc2luZy4=" },
          { "They say that it's not over till the fat lady shouts `Off with its head'.", 
            "VGhleSBzYXkgdGhhdCBpdCdzIG5vdCBvdmVyIHRpbGwgdGhlIGZhdCBsYWR5IHNob3V0cyBgT2ZmIHdpdGggaXRzIGhlYWQnLg==" },
          { "They say that kicking a heavy statue is really a dumb move.", 
            "VGhleSBzYXkgdGhhdCBraWNraW5nIGEgaGVhdnkgc3RhdHVlIGlzIHJlYWxseSBhIGR1bWIgbW92ZS4=" },
          { "They say that kicking a valuable gem doesn't seem to make sense.", 
            "VGhleSBzYXkgdGhhdCBraWNraW5nIGEgdmFsdWFibGUgZ2VtIGRvZXNuJ3Qgc2VlbSB0byBtYWtlIHNlbnNlLg==" },
          { "They say that leprechauns know Latin and you should too.", 
            "VGhleSBzYXkgdGhhdCBsZXByZWNoYXVucyBrbm93IExhdGluIGFuZCB5b3Ugc2hvdWxkIHRvby4=" },
          { "They say that minotaurs get lost outside of the mazes.", 
            "VGhleSBzYXkgdGhhdCBtaW5vdGF1cnMgZ2V0IGxvc3Qgb3V0c2lkZSBvZiB0aGUgbWF6ZXMu" },
          { "They say that most trolls are born again.", 
            "VGhleSBzYXkgdGhhdCBtb3N0IHRyb2xscyBhcmUgYm9ybiBhZ2Fpbi4=" },
          { "They say that naming your cat Garfield will make you more attractive.", 
            "VGhleSBzYXkgdGhhdCBuYW1pbmcgeW91ciBjYXQgR2FyZmllbGQgd2lsbCBtYWtlIHlvdSBtb3JlIGF0dHJhY3RpdmUu" },
          { "They say that no one knows everything about everything in the dungeon.", 
            "VGhleSBzYXkgdGhhdCBubyBvbmUga25vd3MgZXZlcnl0aGluZyBhYm91dCBldmVyeXRoaW5nIGluIHRoZSBkdW5nZW9uLg==" },
          { "They say that no one plays NetHack just for the fun of it.", 
            "VGhleSBzYXkgdGhhdCBubyBvbmUgcGxheXMgTmV0SGFjayBqdXN0IGZvciB0aGUgZnVuIG9mIGl0Lg==" },
          { "They say that no one really subscribes to rec.games.roguelike.nethack.", 
            "VGhleSBzYXkgdGhhdCBubyBvbmUgcmVhbGx5IHN1YnNjcmliZXMgdG8gcmVjLmdhbWVzLnJvZ3VlbGlrZS5uZXRoYWNrLg==" },
          { "They say that no one will admit to starting a rumor.", 
            "VGhleSBzYXkgdGhhdCBubyBvbmUgd2lsbCBhZG1pdCB0byBzdGFydGluZyBhIHJ1bW9yLg==" },
          { "They say that nurses sometimes carry scalpels and never use them.", 
            "VGhleSBzYXkgdGhhdCBudXJzZXMgc29tZXRpbWVzIGNhcnJ5IHNjYWxwZWxzIGFuZCBuZXZlciB1c2UgdGhlbS4=" },
          { "They say that once you've met one wizard you've met them all.", 
            "VGhleSBzYXkgdGhhdCBvbmNlIHlvdSd2ZSBtZXQgb25lIHdpemFyZCB5b3UndmUgbWV0IHRoZW0gYWxsLg==" },
          { "They say that one troll is worth 10,000 newts.", 
            "VGhleSBzYXkgdGhhdCBvbmUgdHJvbGwgaXMgd29ydGggMTAsMDAwIG5ld3RzLg==" },
          { "They say that only David can find the zoo!", 
            "VGhleSBzYXkgdGhhdCBvbmx5IERhdmlkIGNhbiBmaW5kIHRoZSB6b28h" },
          { "They say that only angels play their harps for their pets.", 
            "VGhleSBzYXkgdGhhdCBvbmx5IGFuZ2VscyBwbGF5IHRoZWlyIGhhcnBzIGZvciB0aGVpciBwZXRzLg==" },
          { "They say that only big spenders carry gold.", 
            "VGhleSBzYXkgdGhhdCBvbmx5IGJpZyBzcGVuZGVycyBjYXJyeSBnb2xkLg==" },
          { "They say that orc shamans are healthy, wealthy and wise.", 
            "VGhleSBzYXkgdGhhdCBvcmMgc2hhbWFucyBhcmUgaGVhbHRoeSwgd2VhbHRoeSBhbmQgd2lzZS4=" },
          { "They say that playing NetHack is like walking into a death trap.", 
            "VGhleSBzYXkgdGhhdCBwbGF5aW5nIE5ldEhhY2sgaXMgbGlrZSB3YWxraW5nIGludG8gYSBkZWF0aCB0cmFwLg==" },
          { "They say that problem breathing is best treated by a proper diet.", 
            "VGhleSBzYXkgdGhhdCBwcm9ibGVtIGJyZWF0aGluZyBpcyBiZXN0IHRyZWF0ZWQgYnkgYSBwcm9wZXIgZGlldC4=" },
          { "They say that quaffing many potions of levitation can give you a headache.", 
            "VGhleSBzYXkgdGhhdCBxdWFmZmluZyBtYW55IHBvdGlvbnMgb2YgbGV2aXRhdGlvbiBjYW4gZ2l2ZSB5b3UgYSBoZWFkYWNoZS4=" },
          { "They say that queen bees get that way by eating royal jelly.", 
            "VGhleSBzYXkgdGhhdCBxdWVlbiBiZWVzIGdldCB0aGF0IHdheSBieSBlYXRpbmcgcm95YWwgamVsbHku" },
          { "They say that reading a scare monster scroll is the same as saying Elbereth.", 
            "VGhleSBzYXkgdGhhdCByZWFkaW5nIGEgc2NhcmUgbW9uc3RlciBzY3JvbGwgaXMgdGhlIHNhbWUgYXMgc2F5aW5nIEVsYmVyZXRoLg==" },
          { "They say that real hackers always are controlled.", 
            "VGhleSBzYXkgdGhhdCByZWFsIGhhY2tlcnMgYWx3YXlzIGFyZSBjb250cm9sbGVkLg==" },
          { "They say that real hackers never sleep.", 
            "VGhleSBzYXkgdGhhdCByZWFsIGhhY2tlcnMgbmV2ZXIgc2xlZXAu" },
          { "They say that shopkeepers are insured by Croesus himself!", 
            "VGhleSBzYXkgdGhhdCBzaG9wa2VlcGVycyBhcmUgaW5zdXJlZCBieSBDcm9lc3VzIGhpbXNlbGYh" },
          { "They say that shopkeepers never carry more than 20 gold pieces, at night.", 
            "VGhleSBzYXkgdGhhdCBzaG9wa2VlcGVycyBuZXZlciBjYXJyeSBtb3JlIHRoYW4gMjAgZ29sZCBwaWVjZXMsIGF0IG5pZ2h0Lg==" },
          { "They say that shopkeepers never sell blessed potions of invisibility.", 
            "VGhleSBzYXkgdGhhdCBzaG9wa2VlcGVycyBuZXZlciBzZWxsIGJsZXNzZWQgcG90aW9ucyBvZiBpbnZpc2liaWxpdHku" },
          { "They say that soldiers wear kid gloves and silly helmets.", 
            "VGhleSBzYXkgdGhhdCBzb2xkaWVycyB3ZWFyIGtpZCBnbG92ZXMgYW5kIHNpbGx5IGhlbG1ldHMu" },
          { "They say that some Kops are on the take.", 
            "VGhleSBzYXkgdGhhdCBzb21lIEtvcHMgYXJlIG9uIHRoZSB0YWtlLg==" },
          { "They say that some guards' palms can be greased.", 
            "VGhleSBzYXkgdGhhdCBzb21lIGd1YXJkcycgcGFsbXMgY2FuIGJlIGdyZWFzZWQu" },
          { "They say that some monsters may kiss your boots to stop your drum playing.", 
            "VGhleSBzYXkgdGhhdCBzb21lIG1vbnN0ZXJzIG1heSBraXNzIHlvdXIgYm9vdHMgdG8gc3RvcCB5b3VyIGRydW0gcGxheWluZy4=" },
          { "They say that sometimes you can be the hit of the party when playing a horn.", 
            "VGhleSBzYXkgdGhhdCBzb21ldGltZXMgeW91IGNhbiBiZSB0aGUgaGl0IG9mIHRoZSBwYXJ0eSB3aGVuIHBsYXlpbmcgYSBob3JuLg==" },
          { "They say that the NetHack gods generally welcome your sacrifices.", 
            "VGhleSBzYXkgdGhhdCB0aGUgTmV0SGFjayBnb2RzIGdlbmVyYWxseSB3ZWxjb21lIHlvdXIgc2FjcmlmaWNlcy4=" },
          { "They say that the Three Rings are named Vilya, Nenya and Narya.", 
            "VGhleSBzYXkgdGhhdCB0aGUgVGhyZWUgUmluZ3MgYXJlIG5hbWVkIFZpbHlhLCBOZW55YSBhbmQgTmFyeWEu" },
          { "They say that the Wizard of Yendor has a death wish.", 
            "VGhleSBzYXkgdGhhdCB0aGUgV2l6YXJkIG9mIFllbmRvciBoYXMgYSBkZWF0aCB3aXNoLg==" },
          { "They say that the `hair of the dog' is sometimes an effective remedy.", 
            "VGhleSBzYXkgdGhhdCB0aGUgYGhhaXIgb2YgdGhlIGRvZycgaXMgc29tZXRpbWVzIGFuIGVmZmVjdGl2ZSByZW1lZHku" },
          { "They say that the best time to save your game is now before its too late.", 
            "VGhleSBzYXkgdGhhdCB0aGUgYmVzdCB0aW1lIHRvIHNhdmUgeW91ciBnYW1lIGlzIG5vdyBiZWZvcmUgaXRzIHRvbyBsYXRlLg==" },
          { "They say that the biggest obstacle in NetHack is your mind.", 
            "VGhleSBzYXkgdGhhdCB0aGUgYmlnZ2VzdCBvYnN0YWNsZSBpbiBOZXRIYWNrIGlzIHlvdXIgbWluZC4=" },
          { "They say that the gods are angry when they hit you with objects.", 
            "VGhleSBzYXkgdGhhdCB0aGUgZ29kcyBhcmUgYW5ncnkgd2hlbiB0aGV5IGhpdCB5b3Ugd2l0aCBvYmplY3RzLg==" },
          { "They say that the priesthood are specially favored by the gods.", 
            "VGhleSBzYXkgdGhhdCB0aGUgcHJpZXN0aG9vZCBhcmUgc3BlY2lhbGx5IGZhdm9yZWQgYnkgdGhlIGdvZHMu" },
          { "They say that the way to make a unicorn happy is to give it what it wants.", 
            "VGhleSBzYXkgdGhhdCB0aGUgd2F5IHRvIG1ha2UgYSB1bmljb3JuIGhhcHB5IGlzIHRvIGdpdmUgaXQgd2hhdCBpdCB3YW50cy4=" },
          { "They say that there are no black or white stones, only gray.", 
            "VGhleSBzYXkgdGhhdCB0aGVyZSBhcmUgbm8gYmxhY2sgb3Igd2hpdGUgc3RvbmVzLCBvbmx5IGdyYXku" },
          { "They say that there are no skeletons hence there are no skeleton keys.", 
            "VGhleSBzYXkgdGhhdCB0aGVyZSBhcmUgbm8gc2tlbGV0b25zIGhlbmNlIHRoZXJlIGFyZSBubyBza2VsZXRvbiBrZXlzLg==" },
          { "They say that there is a clever rogue in every hacker just dying to escape.", 
            "VGhleSBzYXkgdGhhdCB0aGVyZSBpcyBhIGNsZXZlciByb2d1ZSBpbiBldmVyeSBoYWNrZXIganVzdCBkeWluZyB0byBlc2NhcGUu" },
          { "They say that there is no such thing as free advice.", 
            "VGhleSBzYXkgdGhhdCB0aGVyZSBpcyBubyBzdWNoIHRoaW5nIGFzIGZyZWUgYWR2aWNlLg==" },
          { "They say that there is only one way to win at NetHack.", 
            "VGhleSBzYXkgdGhhdCB0aGVyZSBpcyBvbmx5IG9uZSB3YXkgdG8gd2luIGF0IE5ldEhhY2su" },
          { "They say that there once was a fearsome chaotic samurai named Luk No.", 
            "VGhleSBzYXkgdGhhdCB0aGVyZSBvbmNlIHdhcyBhIGZlYXJzb21lIGNoYW90aWMgc2FtdXJhaSBuYW1lZCBMdWsgTm8u" },
          { "They say that there was a time when cursed holy water wasn't water.", 
            "VGhleSBzYXkgdGhhdCB0aGVyZSB3YXMgYSB0aW1lIHdoZW4gY3Vyc2VkIGhvbHkgd2F0ZXIgd2Fzbid0IHdhdGVyLg==" },
          { "They say that there's no point in crying over a gray ooze.", 
            "VGhleSBzYXkgdGhhdCB0aGVyZSdzIG5vIHBvaW50IGluIGNyeWluZyBvdmVyIGEgZ3JheSBvb3plLg==" },
          { "They say that there's only hope left after you've opened Pandora's box.", 
            "VGhleSBzYXkgdGhhdCB0aGVyZSdzIG9ubHkgaG9wZSBsZWZ0IGFmdGVyIHlvdSd2ZSBvcGVuZWQgUGFuZG9yYSdzIGJveC4=" },
          { "They say that trapdoors should always be marked `Caution: Trap Door'.", 
            "VGhleSBzYXkgdGhhdCB0cmFwZG9vcnMgc2hvdWxkIGFsd2F5cyBiZSBtYXJrZWQgYENhdXRpb246IFRyYXAgRG9vcicu" },
          { "They say that using an amulet of change isn't a difficult operation.", 
            "VGhleSBzYXkgdGhhdCB1c2luZyBhbiBhbXVsZXQgb2YgY2hhbmdlIGlzbid0IGEgZGlmZmljdWx0IG9wZXJhdGlvbi4=" },
          { "They say that water walking boots are better if you are fast like Hermes.", 
            "VGhleSBzYXkgdGhhdCB3YXRlciB3YWxraW5nIGJvb3RzIGFyZSBiZXR0ZXIgaWYgeW91IGFyZSBmYXN0IGxpa2UgSGVybWVzLg==" },
          { "They say that when you wear a circular amulet you might resemble a troll.", 
            "VGhleSBzYXkgdGhhdCB3aGVuIHlvdSB3ZWFyIGEgY2lyY3VsYXIgYW11bGV0IHlvdSBtaWdodCByZXNlbWJsZSBhIHRyb2xsLg==" },
          { "They say that when you're hungry you can get a pizza in 30 moves or it's free.", 
            "VGhleSBzYXkgdGhhdCB3aGVuIHlvdSdyZSBodW5ncnkgeW91IGNhbiBnZXQgYSBwaXp6YSBpbiAzMCBtb3ZlcyBvciBpdCdzIGZyZWUu" },
          { "They say that when your god is angry you should try another one.", 
            "VGhleSBzYXkgdGhhdCB3aGVuIHlvdXIgZ29kIGlzIGFuZ3J5IHlvdSBzaG91bGQgdHJ5IGFub3RoZXIgb25lLg==" },
          { "They say that wielding a unicorn horn takes strength.", 
            "VGhleSBzYXkgdGhhdCB3aWVsZGluZyBhIHVuaWNvcm4gaG9ybiB0YWtlcyBzdHJlbmd0aC4=" },
          { "They say that with speed boots you never worry about hit and run accidents.", 
            "VGhleSBzYXkgdGhhdCB3aXRoIHNwZWVkIGJvb3RzIHlvdSBuZXZlciB3b3JyeSBhYm91dCBoaXQgYW5kIHJ1biBhY2NpZGVudHMu" },
          { "They say that you can defeat a killer bee with a unicorn horn.", 
            "VGhleSBzYXkgdGhhdCB5b3UgY2FuIGRlZmVhdCBhIGtpbGxlciBiZWUgd2l0aCBhIHVuaWNvcm4gaG9ybi4=" },
          { "They say that you can only cross the River Styx in Charon's boat.", 
            "VGhleSBzYXkgdGhhdCB5b3UgY2FuIG9ubHkgY3Jvc3MgdGhlIFJpdmVyIFN0eXggaW4gQ2hhcm9uJ3MgYm9hdC4=" },
          { "They say that you can only kill a lich once and then you'd better be careful.", 
            "VGhleSBzYXkgdGhhdCB5b3UgY2FuIG9ubHkga2lsbCBhIGxpY2ggb25jZSBhbmQgdGhlbiB5b3UnZCBiZXR0ZXIgYmUgY2FyZWZ1bC4=" },
          { "They say that you can only wish for things you've already had.", 
            "VGhleSBzYXkgdGhhdCB5b3UgY2FuIG9ubHkgd2lzaCBmb3IgdGhpbmdzIHlvdSd2ZSBhbHJlYWR5IGhhZC4=" },
          { "They say that you can train a cat by talking gently to it.", 
            "VGhleSBzYXkgdGhhdCB5b3UgY2FuIHRyYWluIGEgY2F0IGJ5IHRhbGtpbmcgZ2VudGx5IHRvIGl0Lg==" },
          { "They say that you can train a dog by talking firmly to it.", 
            "VGhleSBzYXkgdGhhdCB5b3UgY2FuIHRyYWluIGEgZG9nIGJ5IHRhbGtpbmcgZmlybWx5IHRvIGl0Lg==" },
          { "They say that you can trust your gold with the king.", 
            "VGhleSBzYXkgdGhhdCB5b3UgY2FuIHRydXN0IHlvdXIgZ29sZCB3aXRoIHRoZSBraW5nLg==" },
          { "They say that you can't wipe your greasy bare hands on a blank scroll.", 
            "VGhleSBzYXkgdGhhdCB5b3UgY2FuJ3Qgd2lwZSB5b3VyIGdyZWFzeSBiYXJlIGhhbmRzIG9uIGEgYmxhbmsgc2Nyb2xsLg==" },
          { "They say that you cannot trust scrolls of rumor.", 
            "VGhleSBzYXkgdGhhdCB5b3UgY2Fubm90IHRydXN0IHNjcm9sbHMgb2YgcnVtb3Iu" },
          { "They say that you could fall head over heels for an energy vortex.", 
            "VGhleSBzYXkgdGhhdCB5b3UgY291bGQgZmFsbCBoZWFkIG92ZXIgaGVlbHMgZm9yIGFuIGVuZXJneSB2b3J0ZXgu" },
          { "They say that you need a key in order to open locked doors.", 
            "VGhleSBzYXkgdGhhdCB5b3UgbmVlZCBhIGtleSBpbiBvcmRlciB0byBvcGVuIGxvY2tlZCBkb29ycy4=" },
          { "They say that you need a mirror to notice a mimic in an antique shop.", 
            "VGhleSBzYXkgdGhhdCB5b3UgbmVlZCBhIG1pcnJvciB0byBub3RpY2UgYSBtaW1pYyBpbiBhbiBhbnRpcXVlIHNob3Au" },
          { "They say that you really can use a pick-axe unless you really can't.", 
            "VGhleSBzYXkgdGhhdCB5b3UgcmVhbGx5IGNhbiB1c2UgYSBwaWNrLWF4ZSB1bmxlc3MgeW91IHJlYWxseSBjYW4ndC4=" },
          { "They say that you should always store your tools in the cellar.", 
            "VGhleSBzYXkgdGhhdCB5b3Ugc2hvdWxkIGFsd2F5cyBzdG9yZSB5b3VyIHRvb2xzIGluIHRoZSBjZWxsYXIu" },
          { "They say that you should be careful while climbing the ladder to success.", 
            "VGhleSBzYXkgdGhhdCB5b3Ugc2hvdWxkIGJlIGNhcmVmdWwgd2hpbGUgY2xpbWJpbmcgdGhlIGxhZGRlciB0byBzdWNjZXNzLg==" },
          { "They say that you should call your armor `rustproof'.", 
            "VGhleSBzYXkgdGhhdCB5b3Ugc2hvdWxkIGNhbGwgeW91ciBhcm1vciBgcnVzdHByb29mJy4=" },
          { "They say that you should name your dog Spuds to have a cool pet.", 
            "VGhleSBzYXkgdGhhdCB5b3Ugc2hvdWxkIG5hbWUgeW91ciBkb2cgU3B1ZHMgdG8gaGF2ZSBhIGNvb2wgcGV0Lg==" },
          { "They say that you should name your weapon after your first monster kill.", 
            "VGhleSBzYXkgdGhhdCB5b3Ugc2hvdWxkIG5hbWUgeW91ciB3ZWFwb24gYWZ0ZXIgeW91ciBmaXJzdCBtb25zdGVyIGtpbGwu" },
          { "They say that you should never introduce a rope golem to a succubus.", 
            "VGhleSBzYXkgdGhhdCB5b3Ugc2hvdWxkIG5ldmVyIGludHJvZHVjZSBhIHJvcGUgZ29sZW0gdG8gYSBzdWNjdWJ1cy4=" },
          { "They say that you should never sleep near invisible ring wraiths.", 
            "VGhleSBzYXkgdGhhdCB5b3Ugc2hvdWxkIG5ldmVyIHNsZWVwIG5lYXIgaW52aXNpYmxlIHJpbmcgd3JhaXRocy4=" },
          { "They say that you should never try to leave the dungeon with a bag of gems.", 
            "VGhleSBzYXkgdGhhdCB5b3Ugc2hvdWxkIG5ldmVyIHRyeSB0byBsZWF2ZSB0aGUgZHVuZ2VvbiB3aXRoIGEgYmFnIG9mIGdlbXMu" },
          { "They say that you should remove your armor before sitting on a throne.", 
            "VGhleSBzYXkgdGhhdCB5b3Ugc2hvdWxkIHJlbW92ZSB5b3VyIGFybW9yIGJlZm9yZSBzaXR0aW5nIG9uIGEgdGhyb25lLg==" },
          { "This fortune cookie is copy protected.", 
            "VGhpcyBmb3J0dW5lIGNvb2tpZSBpcyBjb3B5IHByb3RlY3RlZC4=" },
          { "This fortune cookie is the property of Fortune Cookies, Inc.", 
            "VGhpcyBmb3J0dW5lIGNvb2tpZSBpcyB0aGUgcHJvcGVydHkgb2YgRm9ydHVuZSBDb29raWVzLCBJbmMu" },
          { "Tired? Try a scroll of charging on yourself.", 
            "VGlyZWQ/IFRyeSBhIHNjcm9sbCBvZiBjaGFyZ2luZyBvbiB5b3Vyc2VsZi4=" },
          { "To achieve the next higher rating, you need 3 more points.", 
            "VG8gYWNoaWV2ZSB0aGUgbmV4dCBoaWdoZXIgcmF0aW5nLCB5b3UgbmVlZCAzIG1vcmUgcG9pbnRzLg==" },
          { "To reach heaven, escape the dungeon while wearing a ring of levitation.", 
            "VG8gcmVhY2ggaGVhdmVuLCBlc2NhcGUgdGhlIGR1bmdlb24gd2hpbGUgd2VhcmluZyBhIHJpbmcgb2YgbGV2aXRhdGlvbi4=" },
          { "Tourists wear shirts loud enough to wake the dead.", 
            "VG91cmlzdHMgd2VhciBzaGlydHMgbG91ZCBlbm91Z2ggdG8gd2FrZSB0aGUgZGVhZC4=" },
          { "Try calling your katana Moulinette.", 
            "VHJ5IGNhbGxpbmcgeW91ciBrYXRhbmEgTW91bGluZXR0ZS4=" },
          { "Ulch! That meat was painted!", 
            "VWxjaCEgVGhhdCBtZWF0IHdhcyBwYWludGVkIQ==" },
          { "Unfortunately, this message was left intentionally blank.", 
            "VW5mb3J0dW5hdGVseSwgdGhpcyBtZXNzYWdlIHdhcyBsZWZ0IGludGVudGlvbmFsbHkgYmxhbmsu" },
          { "Using a morning star in the evening has no effect.", 
            "VXNpbmcgYSBtb3JuaW5nIHN0YXIgaW4gdGhlIGV2ZW5pbmcgaGFzIG5vIGVmZmVjdC4=" },
          { "Want a hint? Zap a wand of make invisible on your weapon!", 
            "V2FudCBhIGhpbnQ/IFphcCBhIHdhbmQgb2YgbWFrZSBpbnZpc2libGUgb24geW91ciB3ZWFwb24h" },
          { "Want to ascend in a hurry? Apply at Gizmonic Institute.", 
            "V2FudCB0byBhc2NlbmQgaW4gYSBodXJyeT8gQXBwbHkgYXQgR2l6bW9uaWMgSW5zdGl0dXRlLg==" },
          { "Wanted: shopkeepers. Send a scroll of mail to Mage of Yendor/Level 35/Dungeon.", 
            "V2FudGVkOiBzaG9wa2VlcGVycy4gU2VuZCBhIHNjcm9sbCBvZiBtYWlsIHRvIE1hZ2Ugb2YgWWVuZG9yL0xldmVsIDM1L0R1bmdlb24u" },
          { "Warning: fortune reading can be hazardous to your health.", 
            "V2FybmluZzogZm9ydHVuZSByZWFkaW5nIGNhbiBiZSBoYXphcmRvdXMgdG8geW91ciBoZWFsdGgu" },
          { "We have new ways of detecting treachery...", 
            "V2UgaGF2ZSBuZXcgd2F5cyBvZiBkZXRlY3RpbmcgdHJlYWNoZXJ5Li4u" },
          { "Wet towels make great weapons!", 
            "V2V0IHRvd2VscyBtYWtlIGdyZWF0IHdlYXBvbnMh" },
          { "What a pity, you cannot read it!", 
            "V2hhdCBhIHBpdHksIHlvdSBjYW5ub3QgcmVhZCBpdCE=" },
          { "When a piercer drops in on you, you will be tempted to hit the ceiling!", 
            "V2hlbiBhIHBpZXJjZXIgZHJvcHMgaW4gb24geW91LCB5b3Ugd2lsbCBiZSB0ZW1wdGVkIHRvIGhpdCB0aGUgY2VpbGluZyE=" },
          { "When in a maze follow the right wall and you will never get lost.", 
            "V2hlbiBpbiBhIG1hemUgZm9sbG93IHRoZSByaWdodCB3YWxsIGFuZCB5b3Ugd2lsbCBuZXZlciBnZXQgbG9zdC4=" },
          { "When you have a key, you don't have to wait for the guard.", 
            "V2hlbiB5b3UgaGF2ZSBhIGtleSwgeW91IGRvbid0IGhhdmUgdG8gd2FpdCBmb3IgdGhlIGd1YXJkLg==" },
          { "Why are you wasting time reading fortunes?", 
            "V2h5IGFyZSB5b3Ugd2FzdGluZyB0aW1lIHJlYWRpbmcgZm9ydHVuZXM/" },
          { "Wish for a master key and open the Magic Memory Vault!", 
            "V2lzaCBmb3IgYSBtYXN0ZXIga2V5IGFuZCBvcGVuIHRoZSBNYWdpYyBNZW1vcnkgVmF1bHQh" },
          { "Wizard expects every monster to do its duty.", 
            "V2l6YXJkIGV4cGVjdHMgZXZlcnkgbW9uc3RlciB0byBkbyBpdHMgZHV0eS4=" },
          { "Wow! You could've had a potion of fruit juice!", 
            "V293ISBZb3UgY291bGQndmUgaGFkIGEgcG90aW9uIG9mIGZydWl0IGp1aWNlIQ==" },
          { "Yet Another Silly Message (YASM).", 
            "WWV0IEFub3RoZXIgU2lsbHkgTWVzc2FnZSAoWUFTTSku" },
          { "You are destined to be misled by a fortune.", 
            "WW91IGFyZSBkZXN0aW5lZCB0byBiZSBtaXNsZWQgYnkgYSBmb3J0dW5lLg==" },
          { "You can get a genuine Amulet of Yendor by doing the following: --More--", 
            "WW91IGNhbiBnZXQgYSBnZW51aW5lIEFtdWxldCBvZiBZZW5kb3IgYnkgZG9pbmcgdGhlIGZvbGxvd2luZzogLS1Nb3JlLS0=" },
          { "You can protect yourself from black dragons by doing the following: --More--", 
            "WW91IGNhbiBwcm90ZWN0IHlvdXJzZWxmIGZyb20gYmxhY2sgZHJhZ29ucyBieSBkb2luZyB0aGUgZm9sbG93aW5nOiAtLU1vcmUtLQ==" },
          { "You can't get by the snake.", 
            "WW91IGNhbid0IGdldCBieSB0aGUgc25ha2Uu" },
          { "You feel like someone is pulling your leg.", 
            "WW91IGZlZWwgbGlrZSBzb21lb25lIGlzIHB1bGxpbmcgeW91ciBsZWcu" },
          { "You have to outwit the Sphynx or pay her.", 
            "WW91IGhhdmUgdG8gb3V0d2l0IHRoZSBTcGh5bnggb3IgcGF5IGhlci4=" },
          { "You hear the fortune cookie's hissing!", 
            "WW91IGhlYXIgdGhlIGZvcnR1bmUgY29va2llJ3MgaGlzc2luZyE=" },
          { "You may get rich selling letters, but beware of being blackmailed!", 
            "WW91IG1heSBnZXQgcmljaCBzZWxsaW5nIGxldHRlcnMsIGJ1dCBiZXdhcmUgb2YgYmVpbmcgYmxhY2ttYWlsZWQh" },
          { "You offend Shai-Hulud by sheathing your crysknife without having drawn blood.", 
            "WW91IG9mZmVuZCBTaGFpLUh1bHVkIGJ5IHNoZWF0aGluZyB5b3VyIGNyeXNrbmlmZSB3aXRob3V0IGhhdmluZyBkcmF3biBibG9vZC4=" },
          { "You swallowed the fortune!", 
            "WW91IHN3YWxsb3dlZCB0aGUgZm9ydHVuZSE=" },
          { "You want to regain strength? Two levels ahead is a guesthouse!", 
            "WW91IHdhbnQgdG8gcmVnYWluIHN0cmVuZ3RoPyBUd28gbGV2ZWxzIGFoZWFkIGlzIGEgZ3Vlc3Rob3VzZSE=" },
          { "You will encounter a tall, dark, and gruesome creature...", 
            "WW91IHdpbGwgZW5jb3VudGVyIGEgdGFsbCwgZGFyaywgYW5kIGdydWVzb21lIGNyZWF0dXJlLi4u" },

          { "The End", "VGhlIEVuZA==" }
      };

/* PL_Base64Encode, random strings */
PRBool test_004(void)
{
    int i;
    char result[ 4096 ];

    printf("Test 004 (PL_Base64Encode, random strings)                            ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRUint32 plen = PL_strlen(array[i].plaintext);
        PRUint32 clen = ((plen + 2)/3)*4;

        char *rv = PL_Base64Encode(array[i].plaintext, plen, result);

        if( rv != result )
        {
            printf("FAIL\n\t(%d): return value\n", i);
            return PR_FALSE;
        }

        if( 0 != PL_strncmp(result, array[i].cyphertext, clen) )
        {
            printf("FAIL\n\t(%d, \"%s\"): expected \n\"%s,\" got \n\"%.*s.\"\n", 
                   i, array[i].plaintext, array[i].cyphertext, clen, result);
            return PR_FALSE;
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Encode, single characters, malloc */
PRBool test_005(void)
{
    PRUint32 a, b;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char *rv;

    printf("Test 005 (PL_Base64Encode, single characters, malloc)                 ..."); fflush(stdout);

    plain[1] = plain[2] = plain[3] = (unsigned char)0;
    cypher[2] = cypher[3] = (unsigned char)'=';
    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];

        for( b = 0; b < 4; b++ )
        {
            plain[0] = (unsigned char)(a * 4 + b);
            cypher[1] = base[(b * 16)];

            rv = PL_Base64Encode((char *)plain, 1, (char *)0);
            if( (char *)0 == rv )
            {
                printf("FAIL\n\t(%d, %d): no return value\n", a, b);
                return PR_FALSE;
            }

            if( 0 != PL_strcmp((char *)cypher, rv) )
            {
                printf("FAIL\n\t(%d, %d): expected \"%s,\" got \"%s.\"\n",
                       a, b, cypher, rv);
                PR_DELETE(rv);
                return PR_FALSE;
            }

            PR_DELETE(rv);
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Encode, double characters, malloc */
PRBool test_006(void)
{
    PRUint32 a, b, c, d;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char *rv;

    printf("Test 006 (PL_Base64Encode, double characters, malloc)                 ..."); fflush(stdout);

    plain[2] = plain[3] = (unsigned char)0;
    cypher[3] = (unsigned char)'=';
    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];
        for( b = 0; b < 4; b++ )
        {
            plain[0] = (a*4) + b;
            for( c = 0; c < 16; c++ )
            {
                cypher[1] = base[b*16 + c];
                for( d = 0; d < 16; d++ )
                {
                    plain[1] = c*16 + d;
                    cypher[2] = base[d*4];

                    rv = PL_Base64Encode((char *)plain, 2, (char *)0);
                    if( (char *)0 == rv )
                    {
                        printf("FAIL\n\t(%d, %d, %d, %d): no return value\n", a, b, c, d);
                        return PR_FALSE;
                    }

                    if( 0 != PL_strcmp((char *)cypher, rv) )
                    {
                        printf("FAIL\n\t(%d, %d, %d, %d): expected \"%s,\" got \"%s.\"\n",
                               a, b, c, d, cypher, rv);
                        PR_DELETE(rv);
                        return PR_FALSE;
                    }

                    PR_DELETE(rv);
                }
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Encode, triple characters, malloc */
PRBool test_007(void)
{
    PRUint32 a, b, c, d, e, f;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char *rv;

    printf("Test 007 (PL_Base64Encode, triple characters, malloc)                 ..."); fflush(stdout);

    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];
        for( b = 0; b < 4; b++ )
        {
            plain[0] = (a*4) + b;
            for( c = 0; c < 16; c++ )
            {
                cypher[1] = base[b*16 + c];
                for( d = 0; d < 16; d++ )
                {
                    plain[1] = c*16 + d;
                    for( e = 0; e < 4; e++ )
                    {
                        cypher[2] = base[d*4 + e];
                        for( f = 0; f < 64; f++ )
                        {
                            plain[2] = e * 64 + f;
                            cypher[3] = base[f];

                            rv = PL_Base64Encode((char *)plain, 3, (char *)0);
                            if( (char *)0 == rv )
                            {
                                printf("FAIL\n\t(%d, %d, %d, %d, %d, %d): no return value\n", a, b, c, d, e, f);
                                return PR_FALSE;
                            }

                            if( 0 != PL_strcmp((char *)cypher, rv) )
                            {
                                printf("FAIL\n\t(%d, %d, %d, %d, %d, %d): expected \"%s,\" got \"%.4s.\"\n",
                                       a, b, c, d, e, f, cypher, rv);
                                PR_DELETE(rv);
                                return PR_FALSE;
                            }

                            PR_DELETE(rv);
                        }
                    }
                }
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Encode, random strings, malloc */
PRBool test_008(void)
{
    int i;

    printf("Test 008 (PL_Base64Encode, random strings, malloc)                    ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRUint32 plen = PL_strlen(array[i].plaintext);
        PRUint32 clen = ((plen + 2)/3)*4;

        char *rv = PL_Base64Encode(array[i].plaintext, plen, (char *)0);

        if( (char *)0 == rv )
        {
            printf("FAIL\n\t(%d): no return value\n", i);
            return PR_FALSE;
        }

        if( 0 != PL_strcmp(rv, array[i].cyphertext) )
        {
            printf("FAIL\n\t(%d, \"%s\"): expected \n\"%s,\" got \n\"%s.\"\n", 
                   i, array[i].plaintext, array[i].cyphertext, rv);
            return PR_FALSE;
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, single characters */
PRBool test_009(void)
{
    PRUint32 a, b;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char result[ 8 ];
    char *rv;

    printf("Test 009 (PL_Base64Decode, single characters, equals)                 ..."); fflush(stdout);

    plain[1] = plain[2] = plain[3] = (unsigned char)0;
    cypher[2] = cypher[3] = (unsigned char)'=';
    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];

        for( b = 0; b < 4; b++ )
        {
            plain[0] = (unsigned char)(a * 4 + b);
            cypher[1] = base[(b * 16)];

            rv = PL_Base64Decode((char *)cypher, 4, result);
            if( rv != result )
            {
                printf("FAIL\n\t(%d, %d): return value\n", a, b);
                return PR_FALSE;
            }

            if( 0 != PL_strncmp((char *)plain, result, 1) )
            {
                printf("FAIL\n\t(%d, %d): expected \"%s,\" got \"%.1s.\"\n",
                       a, b, plain, result);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, single characters */
PRBool test_010(void)
{
    PRUint32 a, b;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char result[ 8 ];
    char *rv;

    printf("Test 010 (PL_Base64Decode, single characters, no equals)              ..."); fflush(stdout);

    plain[1] = plain[2] = plain[3] = (unsigned char)0;
    cypher[2] = cypher[3] = (unsigned char)0;
    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];

        for( b = 0; b < 4; b++ )
        {
            plain[0] = (unsigned char)(a * 4 + b);
            cypher[1] = base[(b * 16)];

            rv = PL_Base64Decode((char *)cypher, 2, result);
            if( rv != result )
            {
                printf("FAIL\n\t(%d, %d): return value\n", a, b);
                return PR_FALSE;
            }

            if( 0 != PL_strncmp((char *)plain, result, 1) )
            {
                printf("FAIL\n\t(%d, %d): expected \"%s,\" got \"%.1s.\"\n",
                       a, b, plain, result);
                return PR_FALSE;
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, double characters */
PRBool test_011(void)
{
    PRUint32 a, b, c, d;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char result[ 8 ];
    char *rv;

    printf("Test 011 (PL_Base64Decode, double characters, equals)                 ..."); fflush(stdout);

    plain[2] = plain[3] = (unsigned char)0;
    cypher[3] = (unsigned char)'=';
    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];
        for( b = 0; b < 4; b++ )
        {
            plain[0] = (a*4) + b;
            for( c = 0; c < 16; c++ )
            {
                cypher[1] = base[b*16 + c];
                for( d = 0; d < 16; d++ )
                {
                    plain[1] = c*16 + d;
                    cypher[2] = base[d*4];

                    rv = PL_Base64Decode((char *)cypher, 4, result);
                    if( rv != result )
                    {
                        printf("FAIL\n\t(%d, %d, %d, %d): return value\n", a, b, c, d);
                        return PR_FALSE;
                    }

                    if( 0 != PL_strncmp((char *)plain, result, 2) )
                    {
                        printf("FAIL\n\t(%d, %d, %d, %d): expected \"%s,\" got \"%.2s.\"\n",
                               a, b, c, d, plain, result);
                        return PR_FALSE;
                    }
                }
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, double characters */
PRBool test_012(void)
{
    PRUint32 a, b, c, d;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char result[ 8 ];
    char *rv;

    printf("Test 012 (PL_Base64Decode, double characters, no equals)              ..."); fflush(stdout);

    plain[2] = plain[3] = (unsigned char)0;
    cypher[3] = (unsigned char)0;
    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];
        for( b = 0; b < 4; b++ )
        {
            plain[0] = (a*4) + b;
            for( c = 0; c < 16; c++ )
            {
                cypher[1] = base[b*16 + c];
                for( d = 0; d < 16; d++ )
                {
                    plain[1] = c*16 + d;
                    cypher[2] = base[d*4];

                    rv = PL_Base64Decode((char *)cypher, 3, result);
                    if( rv != result )
                    {
                        printf("FAIL\n\t(%d, %d, %d, %d): return value\n", a, b, c, d);
                        return PR_FALSE;
                    }

                    if( 0 != PL_strncmp((char *)plain, result, 2) )
                    {
                        printf("FAIL\n\t(%d, %d, %d, %d): expected \"%s,\" got \"%.2s.\"\n",
                               a, b, c, d, cypher, result);
                        return PR_FALSE;
                    }
                }
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, triple characters */
PRBool test_013(void)
{
    PRUint32 a, b, c, d, e, f;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char result[ 8 ];
    char *rv;

    printf("Test 013 (PL_Base64Decode, triple characters)                         ..."); fflush(stdout);

    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];
        for( b = 0; b < 4; b++ )
        {
            plain[0] = (a*4) + b;
            for( c = 0; c < 16; c++ )
            {
                cypher[1] = base[b*16 + c];
                for( d = 0; d < 16; d++ )
                {
                    plain[1] = c*16 + d;
                    for( e = 0; e < 4; e++ )
                    {
                        cypher[2] = base[d*4 + e];
                        for( f = 0; f < 64; f++ )
                        {
                            plain[2] = e * 64 + f;
                            cypher[3] = base[f];

                            rv = PL_Base64Decode((char *)cypher, 4, result);
                            if( rv != result )
                            {
                                printf("FAIL\n\t(%d, %d, %d, %d, %d, %d): return value\n", a, b, c, d, e, f);
                                return PR_FALSE;
                            }

                            if( 0 != PL_strncmp((char *)plain, result, 3) )
                            {
                                printf("FAIL\n\t(%d, %d, %d, %d, %d, %d): expected \"%s,\" got \"%.3s.\"\n",
                                       a, b, c, d, e, f, plain, result);
                                return PR_FALSE;
                            }
                        }
                    }
                }
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, random strings */
PRBool test_014(void)
{
    int i;
    char result[ 4096 ];

    printf("Test 014 (PL_Base64Decode, random strings, equals)                    ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRUint32 clen = PL_strlen(array[i].cyphertext);
        PRUint32 plen = (clen * 3) / 4;

        char *rv = PL_Base64Decode(array[i].cyphertext, clen, result);

        if( rv != result )
        {
            printf("FAIL\n\t(%d): return value\n", i);
            return PR_FALSE;
        }

        if( 0 == (clen & 3) )
        {
            if( '=' == array[i].cyphertext[clen-1] )
            {
                if( '=' == array[i].cyphertext[clen-2] )
                {
                    plen -= 2;
                }
                else
                {
                    plen -= 1;
                }
            }
        }

        if( 0 != PL_strncmp(result, array[i].plaintext, plen) )
        {
            printf("FAIL\n\t(%d, \"%s\"): expected \n\"%s,\" got \n\"%.*s.\"\n", 
                   i, array[i].cyphertext, array[i].plaintext, plen, result);
            return PR_FALSE;
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, random strings */
PRBool test_015(void)
{
    int i;
    char buffer[ 4096 ];
    char result[ 4096 ];
    char *rv;

    printf("Test 015 (PL_Base64Decode, random strings, no equals)                 ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRUint32 clen, plen;

        PL_strcpy(buffer, array[i].cyphertext);
        clen = PL_strlen(buffer);

        if( 0 == (clen & 3) )
        {
            if( '=' == buffer[clen-1] )
            {
                if( '=' == buffer[clen-2] )
                {
                    buffer[clen-2] = buffer[clen-1] = (char)0;
                    clen -= 2;
                }
                else
                {
                    buffer[clen-1] = (char)0;
                    clen -= 1;
                }
            }
        }

        plen = (clen * 3) / 4;

        rv = PL_Base64Decode(buffer, clen, result);

        if( rv != result )
        {
            printf("FAIL\n\t(%d): return value\n", i);
            return PR_FALSE;
        }

        if( 0 != PL_strncmp(result, array[i].plaintext, plen) )
        {
            printf("FAIL\n\t(%d, \"%s\"): expected \n\"%s,\" got \n\"%.*s.\"\n", 
                   i, array[i].cyphertext, array[i].plaintext, plen, result);
            return PR_FALSE;
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, single characters, malloc */
PRBool test_016(void)
{
    PRUint32 a, b;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char *rv;

    printf("Test 016 (PL_Base64Decode, single characters, equals, malloc)         ..."); fflush(stdout);

    plain[1] = plain[2] = plain[3] = (unsigned char)0;
    cypher[2] = cypher[3] = (unsigned char)'=';
    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];

        for( b = 0; b < 4; b++ )
        {
            plain[0] = (unsigned char)(a * 4 + b);
            cypher[1] = base[(b * 16)];

            rv = PL_Base64Decode((char *)cypher, 4, (char *)0);
            if( (char *)0 == rv )
            {
                printf("FAIL\n\t(%d, %d): no return value\n", a, b);
                return PR_FALSE;
            }

            if( 0 != PL_strcmp((char *)plain, rv) )
            {
                printf("FAIL\n\t(%d, %d): expected \"%s,\" got \"%s.\"\n",
                       a, b, plain, rv);
                PR_DELETE(rv);
                return PR_FALSE;
            }

            PR_DELETE(rv);
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, single characters, malloc */
PRBool test_017(void)
{
    PRUint32 a, b;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char *rv;

    printf("Test 017 (PL_Base64Decode, single characters, no equals, malloc)      ..."); fflush(stdout);

    plain[1] = plain[2] = plain[3] = (unsigned char)0;
    cypher[2] = cypher[3] = (unsigned char)0;
    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];

        for( b = 0; b < 4; b++ )
        {
            plain[0] = (unsigned char)(a * 4 + b);
            cypher[1] = base[(b * 16)];

            rv = PL_Base64Decode((char *)cypher, 2, (char *)0);
            if( (char *)0 == rv )
            {
                printf("FAIL\n\t(%d, %d): no return value\n", a, b);
                return PR_FALSE;
            }

            if( 0 != PL_strcmp((char *)plain, rv) )
            {
                printf("FAIL\n\t(%d, %d): expected \"%s,\" got \"%s.\"\n",
                       a, b, plain, rv);
                PR_DELETE(rv);
                return PR_FALSE;
            }

            PR_DELETE(rv);
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, double characters, malloc */
PRBool test_018(void)
{
    PRUint32 a, b, c, d;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char *rv;

    printf("Test 018 (PL_Base64Decode, double characters, equals, malloc)         ..."); fflush(stdout);

    plain[2] = plain[3] = (unsigned char)0;
    cypher[3] = (unsigned char)'=';
    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];
        for( b = 0; b < 4; b++ )
        {
            plain[0] = (a*4) + b;
            for( c = 0; c < 16; c++ )
            {
                cypher[1] = base[b*16 + c];
                for( d = 0; d < 16; d++ )
                {
                    plain[1] = c*16 + d;
                    cypher[2] = base[d*4];

                    rv = PL_Base64Decode((char *)cypher, 4, (char *)0);
                    if( (char *)0 == rv )
                    {
                        printf("FAIL\n\t(%d, %d, %d, %d): no return value\n", a, b, c, d);
                        return PR_FALSE;
                    }

                    if( 0 != PL_strcmp((char *)plain, rv) )
                    {
                        printf("FAIL\n\t(%d, %d, %d, %d): expected \"%s,\" got \"%s.\"\n",
                               a, b, c, d, plain, rv);
                        PR_DELETE(rv);
                        return PR_FALSE;
                    }

                    PR_DELETE(rv);
                }
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, double characters, malloc */
PRBool test_019(void)
{
    PRUint32 a, b, c, d;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char *rv;

    printf("Test 019 (PL_Base64Decode, double characters, no equals, malloc)      ..."); fflush(stdout);

    plain[2] = plain[3] = (unsigned char)0;
    cypher[3] = (unsigned char)0;
    cypher[4] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];
        for( b = 0; b < 4; b++ )
        {
            plain[0] = (a*4) + b;
            for( c = 0; c < 16; c++ )
            {
                cypher[1] = base[b*16 + c];
                for( d = 0; d < 16; d++ )
                {
                    plain[1] = c*16 + d;
                    cypher[2] = base[d*4];

                    rv = PL_Base64Decode((char *)cypher, 3, (char *)0);
                    if( (char *)0 == rv )
                    {
                        printf("FAIL\n\t(%d, %d, %d, %d): no return value\n", a, b, c, d);
                        return PR_FALSE;
                    }

                    if( 0 != PL_strcmp((char *)plain, rv) )
                    {
                        printf("FAIL\n\t(%d, %d, %d, %d): expected \"%s,\" got \"%s.\"\n",
                               a, b, c, d, cypher, rv);
                        PR_DELETE(rv);
                        return PR_FALSE;
                    }

                    PR_DELETE(rv);
                }
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, triple characters, malloc */
PRBool test_020(void)
{
    PRUint32 a, b, c, d, e, f;
    unsigned char plain[ 4 ];
    unsigned char cypher[ 5 ];
    char *rv;

    printf("Test 020 (PL_Base64Decode, triple characters, malloc)                 ..."); fflush(stdout);

    cypher[4] = (unsigned char)0;
    plain[3] = (unsigned char)0;

    for( a = 0; a < 64; a++ )
    {
        cypher[0] = base[a];
        for( b = 0; b < 4; b++ )
        {
            plain[0] = (a*4) + b;
            for( c = 0; c < 16; c++ )
            {
                cypher[1] = base[b*16 + c];
                for( d = 0; d < 16; d++ )
                {
                    plain[1] = c*16 + d;
                    for( e = 0; e < 4; e++ )
                    {
                        cypher[2] = base[d*4 + e];
                        for( f = 0; f < 64; f++ )
                        {
                            plain[2] = e * 64 + f;
                            cypher[3] = base[f];

                            rv = PL_Base64Decode((char *)cypher, 4, (char *)0);
                            if( (char *)0 == rv )
                            {
                                printf("FAIL\n\t(%d, %d, %d, %d, %d, %d): no return value\n", a, b, c, d, e, f);
                                return PR_FALSE;
                            }

                            if( 0 != PL_strcmp((char *)plain, rv) )
                            {
                                printf("FAIL\n\t(%d, %d, %d, %d, %d, %d): expected \"%s,\" got \"%.3s.\"\n",
                                       a, b, c, d, e, f, plain, rv);
                                PR_DELETE(rv);
                                return PR_FALSE;
                            }

                            PR_DELETE(rv);
                        }
                    }
                }
            }
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, random strings, malloc */
PRBool test_021(void)
{
    int i;

    printf("Test 021 (PL_Base64Decode, random strings, equals, malloc)            ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRUint32 clen = PL_strlen(array[i].cyphertext);

        char *rv = PL_Base64Decode(array[i].cyphertext, clen, (char *)0);

        if( (char *)0 == rv )
        {
            printf("FAIL\n\t(%d): no return value\n", i);
            return PR_FALSE;
        }

        if( 0 != PL_strcmp(rv, array[i].plaintext) )
        {
            printf("FAIL\n\t(%d, \"%s\"): expected \n\"%s,\" got \n\"%s.\"\n", 
                   i, array[i].cyphertext, array[i].plaintext, rv);
            PR_DELETE(rv);
            return PR_FALSE;
        }

        PR_DELETE(rv);
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Encode, random strings, malloc */
PRBool test_022(void)
{
    int i;
    char buffer[ 4096 ];
    char *rv;

    printf("Test 022 (PL_Base64Decode, random strings, no equals, malloc)         ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRUint32 clen;

        PL_strcpy(buffer, array[i].cyphertext);
        clen = PL_strlen(buffer);

        if( 0 == (clen & 3) )
        {
            if( '=' == buffer[clen-1] )
            {
                if( '=' == buffer[clen-2] )
                {
                    buffer[clen-2] = buffer[clen-1] = (char)0;
                    clen -= 2;
                }
                else
                {
                    buffer[clen-1] = (char)0;
                    clen -= 1;
                }
            }
        }

        rv = PL_Base64Decode(buffer, clen, (char *)0);

        if( (char *)0 == rv )
        {
            printf("FAIL\n\t(%d): no return value\n", i);
            return PR_FALSE;
        }

        if( 0 != PL_strcmp(rv, array[i].plaintext) )
        {
            printf("FAIL\n\t(%d, \"%s\"): expected \n\"%s,\" got \n\"%s.\"\n", 
                   i, array[i].cyphertext, array[i].plaintext, rv);
            return PR_FALSE;
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Encode, random strings */
PRBool test_023(void)
{
    int i;
    char result[ 4096 ];

    printf("Test 023 (PL_Base64Encode, random strings, strlen)                    ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRUint32 plen = PL_strlen(array[i].plaintext);
        PRUint32 clen = ((plen + 2)/3)*4;

        char *rv = PL_Base64Encode(array[i].plaintext, 0, result);

        if( rv != result )
        {
            printf("FAIL\n\t(%d): return value\n", i);
            return PR_FALSE;
        }

        if( 0 != PL_strncmp(result, array[i].cyphertext, clen) )
        {
            printf("FAIL\n\t(%d, \"%s\"): expected \n\"%s,\" got \n\"%.*s.\"\n", 
                   i, array[i].plaintext, array[i].cyphertext, clen, result);
            return PR_FALSE;
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Encode, random strings, malloc */
PRBool test_024(void)
{
    int i;

    printf("Test 024 (PL_Base64Encode, random strings, malloc, strlen)            ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRUint32 plen = PL_strlen(array[i].plaintext);
        PRUint32 clen = ((plen + 2)/3)*4;

        char *rv = PL_Base64Encode(array[i].plaintext, 0, (char *)0);

        if( (char *)0 == rv )
        {
            printf("FAIL\n\t(%d): no return value\n", i);
            return PR_FALSE;
        }

        if( 0 != PL_strcmp(rv, array[i].cyphertext) )
        {
            printf("FAIL\n\t(%d, \"%s\"): expected \n\"%s,\" got \n\"%s.\"\n", 
                   i, array[i].plaintext, array[i].cyphertext, rv);
            return PR_FALSE;
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, random strings */
PRBool test_025(void)
{
    int i;
    char result[ 4096 ];

    printf("Test 025 (PL_Base64Decode, random strings, equals, strlen)            ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRUint32 clen = PL_strlen(array[i].cyphertext);
        PRUint32 plen = (clen * 3) / 4;

        char *rv = PL_Base64Decode(array[i].cyphertext, 0, result);

        if( rv != result )
        {
            printf("FAIL\n\t(%d): return value\n", i);
            return PR_FALSE;
        }

        if( 0 == (clen & 3) )
        {
            if( '=' == array[i].cyphertext[clen-1] )
            {
                if( '=' == array[i].cyphertext[clen-2] )
                {
                    plen -= 2;
                }
                else
                {
                    plen -= 1;
                }
            }
        }

        if( 0 != PL_strncmp(result, array[i].plaintext, plen) )
        {
            printf("FAIL\n\t(%d, \"%s\"): expected \n\"%s,\" got \n\"%.*s.\"\n", 
                   i, array[i].cyphertext, array[i].plaintext, plen, result);
            return PR_FALSE;
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, random strings */
PRBool test_026(void)
{
    int i;
    char buffer[ 4096 ];
    char result[ 4096 ];
    char *rv;

    printf("Test 026 (PL_Base64Decode, random strings, no equals, strlen)         ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRUint32 clen, plen;

        PL_strcpy(buffer, array[i].cyphertext);
        clen = PL_strlen(buffer);

        if( 0 == (clen & 3) )
        {
            if( '=' == buffer[clen-1] )
            {
                if( '=' == buffer[clen-2] )
                {
                    buffer[clen-2] = buffer[clen-1] = (char)0;
                    clen -= 2;
                }
                else
                {
                    buffer[clen-1] = (char)0;
                    clen -= 1;
                }
            }
        }

        plen = (clen * 3) / 4;

        rv = PL_Base64Decode(buffer, 0, result);

        if( rv != result )
        {
            printf("FAIL\n\t(%d): return value\n", i);
            return PR_FALSE;
        }

        if( 0 != PL_strncmp(result, array[i].plaintext, plen) )
        {
            printf("FAIL\n\t(%d, \"%s\"): expected \n\"%s,\" got \n\"%.*s.\"\n", 
                   i, array[i].cyphertext, array[i].plaintext, plen, result);
            return PR_FALSE;
        }
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Decode, random strings, malloc */
PRBool test_027(void)
{
    int i;

    printf("Test 027 (PL_Base64Decode, random strings, equals, malloc, strlen)    ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRUint32 clen = PL_strlen(array[i].cyphertext);

        char *rv = PL_Base64Decode(array[i].cyphertext, 0, (char *)0);

        if( (char *)0 == rv )
        {
            printf("FAIL\n\t(%d): no return value\n", i);
            return PR_FALSE;
        }

        if( 0 != PL_strcmp(rv, array[i].plaintext) )
        {
            printf("FAIL\n\t(%d, \"%s\"): expected \n\"%s,\" got \n\"%s.\"\n", 
                   i, array[i].cyphertext, array[i].plaintext, rv);
            PR_DELETE(rv);
            return PR_FALSE;
        }

        PR_DELETE(rv);
    }

    printf("PASS\n");
    return PR_TRUE;
}

/* PL_Base64Encode, random strings, malloc */
PRBool test_028(void)
{
    int i;
    char buffer[ 4096 ];
    char *rv;

    printf("Test 028 (PL_Base64Decode, random strings, no equals, malloc, strlen) ..."); fflush(stdout);

    for( i = 0; i < sizeof(array)/sizeof(array[0]); i++ )
    {
        PRUint32 clen;

        PL_strcpy(buffer, array[i].cyphertext);
        clen = PL_strlen(buffer);

        if( 0 == (clen & 3) )
        {
            if( '=' == buffer[clen-1] )
            {
                if( '=' == buffer[clen-2] )
                {
                    buffer[clen-2] = buffer[clen-1] = (char)0;
                    clen -= 2;
                }
                else
                {
                    buffer[clen-1] = (char)0;
                    clen -= 1;
                }
            }
        }

        rv = PL_Base64Decode(buffer, 0, (char *)0);

        if( (char *)0 == rv )
        {
            printf("FAIL\n\t(%d): no return value\n", i);
            return PR_FALSE;
        }

        if( 0 != PL_strcmp(rv, array[i].plaintext) )
        {
            printf("FAIL\n\t(%d, \"%s\"): expected \n\"%s,\" got \n\"%s.\"\n", 
                   i, array[i].cyphertext, array[i].plaintext, rv);
            return PR_FALSE;
        }
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
    printf("Testing the Portable Library base64 functions:\n");
    printf("(warning: the \"triple characters\" tests are slow)\n");

    if( 1
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
