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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: utf8.c,v $ $Revision: 1.3 $ $Date: 2003/11/27 05:08:20 $ $Name:  $";
#endif /* DEBUG */

#include "seccomon.h"
#include "secport.h"

/*
 * Define this if you want to support UTF-16 in UCS-2
 */
#define UTF16

/*
 * From RFC 2044:
 *
 * UCS-4 range (hex.)           UTF-8 octet sequence (binary)
 * 0000 0000-0000 007F   0xxxxxxx
 * 0000 0080-0000 07FF   110xxxxx 10xxxxxx
 * 0000 0800-0000 FFFF   1110xxxx 10xxxxxx 10xxxxxx
 * 0001 0000-001F FFFF   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 * 0020 0000-03FF FFFF   111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 * 0400 0000-7FFF FFFF   1111110x 10xxxxxx ... 10xxxxxx
 */  

/*
 * From http://www.imc.org/draft-hoffman-utf16
 *
 * For U on [0x00010000,0x0010FFFF]:  Let U' = U - 0x00010000
 *
 * U' = yyyyyyyyyyxxxxxxxxxx
 * W1 = 110110yyyyyyyyyy
 * W2 = 110111xxxxxxxxxx
 */

/*
 * This code is assuming NETWORK BYTE ORDER for the 16- and 32-bit
 * character values.  If you wish to use this code for working with
 * host byte order values, define the following:
 *
 * #if IS_BIG_ENDIAN
 * #define L_0 0
 * #define L_1 1
 * #define L_2 2
 * #define L_3 3
 * #define H_0 0
 * #define H_1 1
 * #else / * not everyone has elif * /
 * #if IS_LITTLE_ENDIAN
 * #define L_0 3
 * #define L_1 2
 * #define L_2 1
 * #define L_3 0
 * #define H_0 1
 * #define H_1 0
 * #else
 * #error "PDP and NUXI support deferred"
 * #endif / * IS_LITTLE_ENDIAN * /
 * #endif / * IS_BIG_ENDIAN * /
 */

#define L_0 0
#define L_1 1
#define L_2 2
#define L_3 3
#define H_0 0
#define H_1 1

PR_IMPLEMENT(PRBool)
sec_port_ucs4_utf8_conversion_function
(
  PRBool toUnicode,
  unsigned char *inBuf,
  unsigned int inBufLen,
  unsigned char *outBuf,
  unsigned int maxOutBufLen,
  unsigned int *outBufLen
)
{
#ifndef TEST_UTF8
  PORT_Assert((unsigned int *)NULL != outBufLen);
#endif /* TEST_UTF8 */

  if( toUnicode ) {
    unsigned int i, len = 0;

    for( i = 0; i < inBufLen; ) {
      if( (inBuf[i] & 0x80) == 0x00 ) i += 1;
      else if( (inBuf[i] & 0xE0) == 0xC0 ) i += 2;
      else if( (inBuf[i] & 0xF0) == 0xE0 ) i += 3;
      else if( (inBuf[i] & 0xF8) == 0xF0 ) i += 4;
      else if( (inBuf[i] & 0xFC) == 0xF8 ) i += 5;
      else if( (inBuf[i] & 0xFE) == 0xFC ) i += 6;
      else return PR_FALSE;

      len += 4;
    }

    if( len > maxOutBufLen ) {
      *outBufLen = len;
      return PR_FALSE;
    }

    len = 0;

    for( i = 0; i < inBufLen; ) {
      if( (inBuf[i] & 0x80) == 0x00 ) {
        /* 0000 0000-0000 007F <- 0xxxxxx */
        /* 0abcdefg -> 
           00000000 00000000 00000000 0abcdefg */

        outBuf[len+L_0] = 0x00;
        outBuf[len+L_1] = 0x00;
        outBuf[len+L_2] = 0x00;
        outBuf[len+L_3] = inBuf[i+0] & 0x7F;

        i += 1;
      } else if( (inBuf[i] & 0xE0) == 0xC0 ) {

        if( (inBuf[i+1] & 0xC0) != 0x80 ) return PR_FALSE;

        /* 0000 0080-0000 07FF <- 110xxxxx 10xxxxxx */
        /* 110abcde 10fghijk ->
           00000000 00000000 00000abc defghijk */

        outBuf[len+L_0] = 0x00;
        outBuf[len+L_1] = 0x00;
        outBuf[len+L_2] = ((inBuf[i+0] & 0x1C) >> 2);
        outBuf[len+L_3] = ((inBuf[i+0] & 0x03) << 6) | ((inBuf[i+1] & 0x3F) >> 0);

        i += 2;
      } else if( (inBuf[i] & 0xF0) == 0xE0 ) {

        if( (inBuf[i+1] & 0xC0) != 0x80 ) return PR_FALSE;
        if( (inBuf[i+2] & 0xC0) != 0x80 ) return PR_FALSE;

        /* 0000 0800-0000 FFFF <- 1110xxxx 10xxxxxx 10xxxxxx */
        /* 1110abcd 10efghij 10klmnop ->
           00000000 00000000 abcdefgh ijklmnop */

        outBuf[len+L_0] = 0x00;
        outBuf[len+L_1] = 0x00;
        outBuf[len+L_2] = ((inBuf[i+0] & 0x0F) << 4) | ((inBuf[i+1] & 0x3C) >> 2);
        outBuf[len+L_3] = ((inBuf[i+1] & 0x03) << 6) | ((inBuf[i+2] & 0x3F) >> 0);

        i += 3;
      } else if( (inBuf[i] & 0xF8) == 0xF0 ) {

        if( (inBuf[i+1] & 0xC0) != 0x80 ) return PR_FALSE;
        if( (inBuf[i+2] & 0xC0) != 0x80 ) return PR_FALSE;
        if( (inBuf[i+3] & 0xC0) != 0x80 ) return PR_FALSE;

        /* 0001 0000-001F FFFF <- 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        /* 11110abc 10defghi 10jklmno 10pqrstu -> 
           00000000 000abcde fghijklm nopqrstu */
           
        outBuf[len+L_0] = 0x00;
        outBuf[len+L_1] = ((inBuf[i+0] & 0x07) << 2) | ((inBuf[i+1] & 0x30) >> 4);
        outBuf[len+L_2] = ((inBuf[i+1] & 0x0F) << 4) | ((inBuf[i+2] & 0x3C) >> 2);
        outBuf[len+L_3] = ((inBuf[i+2] & 0x03) << 6) | ((inBuf[i+3] & 0x3F) >> 0);

        i += 4;
      } else if( (inBuf[i] & 0xFC) == 0xF8 ) {

        if( (inBuf[i+1] & 0xC0) != 0x80 ) return PR_FALSE;
        if( (inBuf[i+2] & 0xC0) != 0x80 ) return PR_FALSE;
        if( (inBuf[i+3] & 0xC0) != 0x80 ) return PR_FALSE;
        if( (inBuf[i+4] & 0xC0) != 0x80 ) return PR_FALSE;

        /* 0020 0000-03FF FFFF <- 111110xx 10xxxxxx ... 10xxxxxx */
        /* 111110ab 10cdefgh 10ijklmn 10opqrst 10uvwxyz -> 
           000000ab cdefghij klmnopqr stuvwxyz */

        outBuf[len+L_0] = inBuf[i+0] & 0x03;
        outBuf[len+L_1] = ((inBuf[i+1] & 0x3F) << 2) | ((inBuf[i+2] & 0x30) >> 4);
        outBuf[len+L_2] = ((inBuf[i+2] & 0x0F) << 4) | ((inBuf[i+3] & 0x3C) >> 2);
        outBuf[len+L_3] = ((inBuf[i+3] & 0x03) << 6) | ((inBuf[i+4] & 0x3F) >> 0);

        i += 5;
      } else /* if( (inBuf[i] & 0xFE) == 0xFC ) */ {

        if( (inBuf[i+1] & 0xC0) != 0x80 ) return PR_FALSE;
        if( (inBuf[i+2] & 0xC0) != 0x80 ) return PR_FALSE;
        if( (inBuf[i+3] & 0xC0) != 0x80 ) return PR_FALSE;
        if( (inBuf[i+4] & 0xC0) != 0x80 ) return PR_FALSE;
        if( (inBuf[i+5] & 0xC0) != 0x80 ) return PR_FALSE;

        /* 0400 0000-7FFF FFFF <- 1111110x 10xxxxxx ... 10xxxxxx */
        /* 1111110a 10bcdefg 10hijklm 10nopqrs 10tuvwxy 10zABCDE -> 
           0abcdefg hijklmno pqrstuvw xyzABCDE */

        outBuf[len+L_0] = ((inBuf[i+0] & 0x01) << 6) | ((inBuf[i+1] & 0x3F) >> 0);
        outBuf[len+L_1] = ((inBuf[i+2] & 0x3F) << 2) | ((inBuf[i+3] & 0x30) >> 4);
        outBuf[len+L_2] = ((inBuf[i+3] & 0x0F) << 4) | ((inBuf[i+4] & 0x3C) >> 2);
        outBuf[len+L_3] = ((inBuf[i+4] & 0x03) << 6) | ((inBuf[i+5] & 0x3F) >> 0);

        i += 6;
      }

      len += 4;
    }

    *outBufLen = len;
    return PR_TRUE;
  } else {
    unsigned int i, len = 0;
    PORT_Assert((inBufLen % 4) == 0);
    if ((inBufLen % 4) != 0) {
      *outBufLen = 0;
      return PR_FALSE;
    }

    for( i = 0; i < inBufLen; i += 4 ) {
      if( inBuf[i+L_0] >= 0x04 ) len += 6;
      else if( (inBuf[i+L_0] > 0x00) || (inBuf[i+L_1] >= 0x20) ) len += 5;
      else if( inBuf[i+L_1] >= 0x01 ) len += 4;
      else if( inBuf[i+L_2] >= 0x08 ) len += 3;
      else if( (inBuf[i+L_2] > 0x00) || (inBuf[i+L_3] >= 0x80) ) len += 2;
      else len += 1;
    }

    if( len > maxOutBufLen ) {
      *outBufLen = len;
      return PR_FALSE;
    }

    len = 0;

    for( i = 0; i < inBufLen; i += 4 ) {
      if( inBuf[i+L_0] >= 0x04 ) {
        /* 0400 0000-7FFF FFFF -> 1111110x 10xxxxxx ... 10xxxxxx */
        /* 0abcdefg hijklmno pqrstuvw xyzABCDE ->
           1111110a 10bcdefg 10hijklm 10nopqrs 10tuvwxy 10zABCDE */

        outBuf[len+0] = 0xFC | ((inBuf[i+L_0] & 0x40) >> 6);
        outBuf[len+1] = 0x80 | ((inBuf[i+L_0] & 0x3F) >> 0);
        outBuf[len+2] = 0x80 | ((inBuf[i+L_1] & 0xFC) >> 2);
        outBuf[len+3] = 0x80 | ((inBuf[i+L_1] & 0x03) << 4)
                             | ((inBuf[i+L_2] & 0xF0) >> 4);
        outBuf[len+4] = 0x80 | ((inBuf[i+L_2] & 0x0F) << 2)
                             | ((inBuf[i+L_3] & 0xC0) >> 6);
        outBuf[len+5] = 0x80 | ((inBuf[i+L_3] & 0x3F) >> 0);

        len += 6;
      } else if( (inBuf[i+L_0] > 0x00) || (inBuf[i+L_1] >= 0x20) ) {
        /* 0020 0000-03FF FFFF -> 111110xx 10xxxxxx ... 10xxxxxx */
        /* 000000ab cdefghij klmnopqr stuvwxyz ->
           111110ab 10cdefgh 10ijklmn 10opqrst 10uvwxyz */

        outBuf[len+0] = 0xF8 | ((inBuf[i+L_0] & 0x03) >> 0);
        outBuf[len+1] = 0x80 | ((inBuf[i+L_1] & 0xFC) >> 2);
        outBuf[len+2] = 0x80 | ((inBuf[i+L_1] & 0x03) << 4)
                             | ((inBuf[i+L_2] & 0xF0) >> 4);
        outBuf[len+3] = 0x80 | ((inBuf[i+L_2] & 0x0F) << 2)
                             | ((inBuf[i+L_3] & 0xC0) >> 6);
        outBuf[len+4] = 0x80 | ((inBuf[i+L_3] & 0x3F) >> 0);

        len += 5;
      } else if( inBuf[i+L_1] >= 0x01 ) {
        /* 0001 0000-001F FFFF -> 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        /* 00000000 000abcde fghijklm nopqrstu ->
           11110abc 10defghi 10jklmno 10pqrstu */

        outBuf[len+0] = 0xF0 | ((inBuf[i+L_1] & 0x1C) >> 2);
        outBuf[len+1] = 0x80 | ((inBuf[i+L_1] & 0x03) << 4)
                             | ((inBuf[i+L_2] & 0xF0) >> 4);
        outBuf[len+2] = 0x80 | ((inBuf[i+L_2] & 0x0F) << 2)
                             | ((inBuf[i+L_3] & 0xC0) >> 6);
        outBuf[len+3] = 0x80 | ((inBuf[i+L_3] & 0x3F) >> 0);

        len += 4;
      } else if( inBuf[i+L_2] >= 0x08 ) {
        /* 0000 0800-0000 FFFF -> 1110xxxx 10xxxxxx 10xxxxxx */
        /* 00000000 00000000 abcdefgh ijklmnop ->
           1110abcd 10efghij 10klmnop */

        outBuf[len+0] = 0xE0 | ((inBuf[i+L_2] & 0xF0) >> 4);
        outBuf[len+1] = 0x80 | ((inBuf[i+L_2] & 0x0F) << 2)
                             | ((inBuf[i+L_3] & 0xC0) >> 6);
        outBuf[len+2] = 0x80 | ((inBuf[i+L_3] & 0x3F) >> 0);

        len += 3;
      } else if( (inBuf[i+L_2] > 0x00) || (inBuf[i+L_3] >= 0x80) ) {
        /* 0000 0080-0000 07FF -> 110xxxxx 10xxxxxx */
        /* 00000000 00000000 00000abc defghijk ->
           110abcde 10fghijk */

        outBuf[len+0] = 0xC0 | ((inBuf[i+L_2] & 0x07) << 2)
                             | ((inBuf[i+L_3] & 0xC0) >> 6);
        outBuf[len+1] = 0x80 | ((inBuf[i+L_3] & 0x3F) >> 0);

        len += 2;
      } else {
        /* 0000 0000-0000 007F -> 0xxxxxx */
        /* 00000000 00000000 00000000 0abcdefg ->
           0abcdefg */

        outBuf[len+0] = (inBuf[i+L_3] & 0x7F);

        len += 1;
      }
    }
                            
    *outBufLen = len;
    return PR_TRUE;
  }
}

PR_IMPLEMENT(PRBool)
sec_port_ucs2_utf8_conversion_function
(
  PRBool toUnicode,
  unsigned char *inBuf,
  unsigned int inBufLen,
  unsigned char *outBuf,
  unsigned int maxOutBufLen,
  unsigned int *outBufLen
)
{
#ifndef TEST_UTF8
  PORT_Assert((unsigned int *)NULL != outBufLen);
#endif /* TEST_UTF8 */

  if( toUnicode ) {
    unsigned int i, len = 0;

    for( i = 0; i < inBufLen; ) {
      if( (inBuf[i] & 0x80) == 0x00 ) {
        i += 1;
        len += 2;
      } else if( (inBuf[i] & 0xE0) == 0xC0 ) {
        i += 2;
        len += 2;
      } else if( (inBuf[i] & 0xF0) == 0xE0 ) {
        i += 3;
        len += 2;
#ifdef UTF16
      } else if( (inBuf[i] & 0xF8) == 0xF0 ) { 
        i += 4;
        len += 4;

        if( (inBuf[i] & 0x04) && 
            ((inBuf[i] & 0x03) || (inBuf[i+1] & 0x30)) ) {
          /* Not representable as UTF16 */
          return PR_FALSE;
        }

#endif /* UTF16 */
      } else return PR_FALSE;
    }

    if( len > maxOutBufLen ) {
      *outBufLen = len;
      return PR_FALSE;
    }

    len = 0;

    for( i = 0; i < inBufLen; ) {
      if( (inBuf[i] & 0x80) == 0x00 ) {
        /* 0000-007F <- 0xxxxxx */
        /* 0abcdefg -> 00000000 0abcdefg */

        outBuf[len+H_0] = 0x00;
        outBuf[len+H_1] = inBuf[i+0] & 0x7F;

        i += 1;
        len += 2;
      } else if( (inBuf[i] & 0xE0) == 0xC0 ) {

        if( (inBuf[i+1] & 0xC0) != 0x80 ) return PR_FALSE;

        /* 0080-07FF <- 110xxxxx 10xxxxxx */
        /* 110abcde 10fghijk -> 00000abc defghijk */

        outBuf[len+H_0] = ((inBuf[i+0] & 0x1C) >> 2);
        outBuf[len+H_1] = ((inBuf[i+0] & 0x03) << 6) | ((inBuf[i+1] & 0x3F) >> 0);

        i += 2;
        len += 2;
      } else if( (inBuf[i] & 0xF0) == 0xE0 ) {

        if( (inBuf[i+1] & 0xC0) != 0x80 ) return PR_FALSE;
        if( (inBuf[i+2] & 0xC0) != 0x80 ) return PR_FALSE;

        /* 0800-FFFF <- 1110xxxx 10xxxxxx 10xxxxxx */
        /* 1110abcd 10efghij 10klmnop -> abcdefgh ijklmnop */

        outBuf[len+H_0] = ((inBuf[i+0] & 0x0F) << 4) | ((inBuf[i+1] & 0x3C) >> 2);
        outBuf[len+H_1] = ((inBuf[i+1] & 0x03) << 6) | ((inBuf[i+2] & 0x3F) >> 0);

        i += 3;
        len += 2;
#ifdef UTF16
      } else if( (inBuf[i] & 0xF8) == 0xF0 ) { 
        int abcde, BCDE;

        if( (inBuf[i+1] & 0xC0) != 0x80 ) return PR_FALSE;
        if( (inBuf[i+2] & 0xC0) != 0x80 ) return PR_FALSE;
        if( (inBuf[i+3] & 0xC0) != 0x80 ) return PR_FALSE;

        /* 0001 0000-001F FFFF <- 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx -> [D800-DBFF] [DC00-DFFF] */
           
        /* 11110abc 10defghi 10jklmno 10pqrstu -> 
           { Let 0BCDE = abcde - 1 }
           110110BC DEfghijk 110111lm nopqrstu */

        abcde = ((inBuf[i+0] & 0x07) << 2) | ((inBuf[i+1] & 0x30) >> 4);
        BCDE = abcde - 1;

#ifndef TEST_UTF8
        PORT_Assert(BCDE < 0x10); /* should have been caught above */
#endif /* TEST_UTF8 */

        outBuf[len+0+H_0] = 0xD8 | ((BCDE & 0x0C) >> 2);
        outBuf[len+0+H_1] = ((BCDE & 0x03) << 6) 
                      | ((inBuf[i+1] & 0x0F) << 2)
                      | ((inBuf[i+2] & 0x30) >> 4);
        outBuf[len+2+H_0] = 0xDC | ((inBuf[i+2] & 0x0C) >> 2);
        outBuf[len+2+H_1] = ((inBuf[i+2] & 0x03) << 6) | ((inBuf[i+3] & 0x3F) >> 0);

        i += 4;
        len += 4;
#endif /* UTF16 */
      } else return PR_FALSE;
    }

    *outBufLen = len;
    return PR_TRUE;
  } else {
    unsigned int i, len = 0;
    PORT_Assert((inBufLen % 2) == 0);
    if ((inBufLen % 2) != 0) {
      *outBufLen = 0;
      return PR_FALSE;
    }

    for( i = 0; i < inBufLen; i += 2 ) {
      if( (inBuf[i+H_0] == 0x00) && ((inBuf[i+H_0] & 0x80) == 0x00) ) len += 1;
      else if( inBuf[i+H_0] < 0x08 ) len += 2;
#ifdef UTF16
      else if( ((inBuf[i+0+H_0] & 0xDC) == 0xD8) ) {
        if( ((inBuf[i+2+H_0] & 0xDC) == 0xDC) && ((inBufLen - i) > 2) ) {
          i += 2;
          len += 4;
        } else {
          return PR_FALSE;
        }
      }
#endif /* UTF16 */
      else len += 3;
    }

    if( len > maxOutBufLen ) {
      *outBufLen = len;
      return PR_FALSE;
    }

    len = 0;

    for( i = 0; i < inBufLen; i += 2 ) {
      if( (inBuf[i+H_0] == 0x00) && ((inBuf[i+H_1] & 0x80) == 0x00) ) {
        /* 0000-007F -> 0xxxxxx */
        /* 00000000 0abcdefg -> 0abcdefg */

        outBuf[len] = inBuf[i+H_1] & 0x7F;

        len += 1;
      } else if( inBuf[i+H_0] < 0x08 ) {
        /* 0080-07FF -> 110xxxxx 10xxxxxx */
        /* 00000abc defghijk -> 110abcde 10fghijk */

        outBuf[len+0] = 0xC0 | ((inBuf[i+H_0] & 0x07) << 2) 
                             | ((inBuf[i+H_1] & 0xC0) >> 6);
        outBuf[len+1] = 0x80 | ((inBuf[i+H_1] & 0x3F) >> 0);

        len += 2;
#ifdef UTF16
      } else if( (inBuf[i+H_0] & 0xDC) == 0xD8 ) {
        int abcde, BCDE;

#ifndef TEST_UTF8
        PORT_Assert(((inBuf[i+2+H_0] & 0xDC) == 0xDC) && ((inBufLen - i) > 2));
#endif /* TEST_UTF8 */

        /* D800-DBFF DC00-DFFF -> 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        /* 110110BC DEfghijk 110111lm nopqrstu ->
           { Let abcde = BCDE + 1 }
           11110abc 10defghi 10jklmno 10pqrstu */

        BCDE = ((inBuf[i+H_0] & 0x03) << 2) | ((inBuf[i+H_1] & 0xC0) >> 6);
        abcde = BCDE + 1;

        outBuf[len+0] = 0xF0 | ((abcde & 0x1C) >> 2);
        outBuf[len+1] = 0x80 | ((abcde & 0x03) << 4) 
                             | ((inBuf[i+0+H_1] & 0x3C) >> 2);
        outBuf[len+2] = 0x80 | ((inBuf[i+0+H_1] & 0x03) << 4)
                             | ((inBuf[i+2+H_0] & 0x03) << 2)
                             | ((inBuf[i+2+H_1] & 0xC0) >> 6);
        outBuf[len+3] = 0x80 | ((inBuf[i+2+H_1] & 0x3F) >> 0);

        i += 2;
        len += 4;
#endif /* UTF16 */
      } else {
        /* 0800-FFFF -> 1110xxxx 10xxxxxx 10xxxxxx */
        /* abcdefgh ijklmnop -> 1110abcd 10efghij 10klmnop */

        outBuf[len+0] = 0xE0 | ((inBuf[i+H_0] & 0xF0) >> 4);
        outBuf[len+1] = 0x80 | ((inBuf[i+H_0] & 0x0F) << 2) 
                             | ((inBuf[i+H_1] & 0xC0) >> 6);
        outBuf[len+2] = 0x80 | ((inBuf[i+H_1] & 0x3F) >> 0);

        len += 3;
      }
    }

    *outBufLen = len;
    return PR_TRUE;
  }
}

#ifdef TEST_UTF8

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h> /* for htonl and htons */

/*
 * UCS-4 vectors
 */

struct ucs4 {
  PRUint32 c;
  char *utf8;
};

/*
 * UCS-2 vectors
 */

struct ucs2 {
  PRUint16 c;
  char *utf8;
};

#ifdef UTF16
/*
 * UTF-16 vectors
 */

struct utf16 {
  PRUint32 c;
  PRUint16 w[2];
};
#endif /* UTF16 */


/*
 * UCS-4 vectors
 */

struct ucs4 ucs4[] = {
  { 0x00000001, "\x01" },
  { 0x00000002, "\x02" },
  { 0x00000003, "\x03" },
  { 0x00000004, "\x04" },
  { 0x00000007, "\x07" },
  { 0x00000008, "\x08" },
  { 0x0000000F, "\x0F" },
  { 0x00000010, "\x10" },
  { 0x0000001F, "\x1F" },
  { 0x00000020, "\x20" },
  { 0x0000003F, "\x3F" },
  { 0x00000040, "\x40" },
  { 0x0000007F, "\x7F" },
          
  { 0x00000080, "\xC2\x80" },
  { 0x00000081, "\xC2\x81" },
  { 0x00000082, "\xC2\x82" },
  { 0x00000084, "\xC2\x84" },
  { 0x00000088, "\xC2\x88" },
  { 0x00000090, "\xC2\x90" },
  { 0x000000A0, "\xC2\xA0" },
  { 0x000000C0, "\xC3\x80" },
  { 0x000000FF, "\xC3\xBF" },
  { 0x00000100, "\xC4\x80" },
  { 0x00000101, "\xC4\x81" },
  { 0x00000102, "\xC4\x82" },
  { 0x00000104, "\xC4\x84" },
  { 0x00000108, "\xC4\x88" },
  { 0x00000110, "\xC4\x90" },
  { 0x00000120, "\xC4\xA0" },
  { 0x00000140, "\xC5\x80" },
  { 0x00000180, "\xC6\x80" },
  { 0x000001FF, "\xC7\xBF" },
  { 0x00000200, "\xC8\x80" },
  { 0x00000201, "\xC8\x81" },
  { 0x00000202, "\xC8\x82" },
  { 0x00000204, "\xC8\x84" },
  { 0x00000208, "\xC8\x88" },
  { 0x00000210, "\xC8\x90" },
  { 0x00000220, "\xC8\xA0" },
  { 0x00000240, "\xC9\x80" },
  { 0x00000280, "\xCA\x80" },
  { 0x00000300, "\xCC\x80" },
  { 0x000003FF, "\xCF\xBF" },
  { 0x00000400, "\xD0\x80" },
  { 0x00000401, "\xD0\x81" },
  { 0x00000402, "\xD0\x82" },
  { 0x00000404, "\xD0\x84" },
  { 0x00000408, "\xD0\x88" },
  { 0x00000410, "\xD0\x90" },
  { 0x00000420, "\xD0\xA0" },
  { 0x00000440, "\xD1\x80" },
  { 0x00000480, "\xD2\x80" },
  { 0x00000500, "\xD4\x80" },
  { 0x00000600, "\xD8\x80" },
  { 0x000007FF, "\xDF\xBF" },
          
  { 0x00000800, "\xE0\xA0\x80" },
  { 0x00000801, "\xE0\xA0\x81" },
  { 0x00000802, "\xE0\xA0\x82" },
  { 0x00000804, "\xE0\xA0\x84" },
  { 0x00000808, "\xE0\xA0\x88" },
  { 0x00000810, "\xE0\xA0\x90" },
  { 0x00000820, "\xE0\xA0\xA0" },
  { 0x00000840, "\xE0\xA1\x80" },
  { 0x00000880, "\xE0\xA2\x80" },
  { 0x00000900, "\xE0\xA4\x80" },
  { 0x00000A00, "\xE0\xA8\x80" },
  { 0x00000C00, "\xE0\xB0\x80" },
  { 0x00000FFF, "\xE0\xBF\xBF" },
  { 0x00001000, "\xE1\x80\x80" },
  { 0x00001001, "\xE1\x80\x81" },
  { 0x00001002, "\xE1\x80\x82" },
  { 0x00001004, "\xE1\x80\x84" },
  { 0x00001008, "\xE1\x80\x88" },
  { 0x00001010, "\xE1\x80\x90" },
  { 0x00001020, "\xE1\x80\xA0" },
  { 0x00001040, "\xE1\x81\x80" },
  { 0x00001080, "\xE1\x82\x80" },
  { 0x00001100, "\xE1\x84\x80" },
  { 0x00001200, "\xE1\x88\x80" },
  { 0x00001400, "\xE1\x90\x80" },
  { 0x00001800, "\xE1\xA0\x80" },
  { 0x00001FFF, "\xE1\xBF\xBF" },
  { 0x00002000, "\xE2\x80\x80" },
  { 0x00002001, "\xE2\x80\x81" },
  { 0x00002002, "\xE2\x80\x82" },
  { 0x00002004, "\xE2\x80\x84" },
  { 0x00002008, "\xE2\x80\x88" },
  { 0x00002010, "\xE2\x80\x90" },
  { 0x00002020, "\xE2\x80\xA0" },
  { 0x00002040, "\xE2\x81\x80" },
  { 0x00002080, "\xE2\x82\x80" },
  { 0x00002100, "\xE2\x84\x80" },
  { 0x00002200, "\xE2\x88\x80" },
  { 0x00002400, "\xE2\x90\x80" },
  { 0x00002800, "\xE2\xA0\x80" },
  { 0x00003000, "\xE3\x80\x80" },
  { 0x00003FFF, "\xE3\xBF\xBF" },
  { 0x00004000, "\xE4\x80\x80" },
  { 0x00004001, "\xE4\x80\x81" },
  { 0x00004002, "\xE4\x80\x82" },
  { 0x00004004, "\xE4\x80\x84" },
  { 0x00004008, "\xE4\x80\x88" },
  { 0x00004010, "\xE4\x80\x90" },
  { 0x00004020, "\xE4\x80\xA0" },
  { 0x00004040, "\xE4\x81\x80" },
  { 0x00004080, "\xE4\x82\x80" },
  { 0x00004100, "\xE4\x84\x80" },
  { 0x00004200, "\xE4\x88\x80" },
  { 0x00004400, "\xE4\x90\x80" },
  { 0x00004800, "\xE4\xA0\x80" },
  { 0x00005000, "\xE5\x80\x80" },
  { 0x00006000, "\xE6\x80\x80" },
  { 0x00007FFF, "\xE7\xBF\xBF" },
  { 0x00008000, "\xE8\x80\x80" },
  { 0x00008001, "\xE8\x80\x81" },
  { 0x00008002, "\xE8\x80\x82" },
  { 0x00008004, "\xE8\x80\x84" },
  { 0x00008008, "\xE8\x80\x88" },
  { 0x00008010, "\xE8\x80\x90" },
  { 0x00008020, "\xE8\x80\xA0" },
  { 0x00008040, "\xE8\x81\x80" },
  { 0x00008080, "\xE8\x82\x80" },
  { 0x00008100, "\xE8\x84\x80" },
  { 0x00008200, "\xE8\x88\x80" },
  { 0x00008400, "\xE8\x90\x80" },
  { 0x00008800, "\xE8\xA0\x80" },
  { 0x00009000, "\xE9\x80\x80" },
  { 0x0000A000, "\xEA\x80\x80" },
  { 0x0000C000, "\xEC\x80\x80" },
  { 0x0000FFFF, "\xEF\xBF\xBF" },
          
  { 0x00010000, "\xF0\x90\x80\x80" },
  { 0x00010001, "\xF0\x90\x80\x81" },
  { 0x00010002, "\xF0\x90\x80\x82" },
  { 0x00010004, "\xF0\x90\x80\x84" },
  { 0x00010008, "\xF0\x90\x80\x88" },
  { 0x00010010, "\xF0\x90\x80\x90" },
  { 0x00010020, "\xF0\x90\x80\xA0" },
  { 0x00010040, "\xF0\x90\x81\x80" },
  { 0x00010080, "\xF0\x90\x82\x80" },
  { 0x00010100, "\xF0\x90\x84\x80" },
  { 0x00010200, "\xF0\x90\x88\x80" },
  { 0x00010400, "\xF0\x90\x90\x80" },
  { 0x00010800, "\xF0\x90\xA0\x80" },
  { 0x00011000, "\xF0\x91\x80\x80" },
  { 0x00012000, "\xF0\x92\x80\x80" },
  { 0x00014000, "\xF0\x94\x80\x80" },
  { 0x00018000, "\xF0\x98\x80\x80" },
  { 0x0001FFFF, "\xF0\x9F\xBF\xBF" },
  { 0x00020000, "\xF0\xA0\x80\x80" },
  { 0x00020001, "\xF0\xA0\x80\x81" },
  { 0x00020002, "\xF0\xA0\x80\x82" },
  { 0x00020004, "\xF0\xA0\x80\x84" },
  { 0x00020008, "\xF0\xA0\x80\x88" },
  { 0x00020010, "\xF0\xA0\x80\x90" },
  { 0x00020020, "\xF0\xA0\x80\xA0" },
  { 0x00020040, "\xF0\xA0\x81\x80" },
  { 0x00020080, "\xF0\xA0\x82\x80" },
  { 0x00020100, "\xF0\xA0\x84\x80" },
  { 0x00020200, "\xF0\xA0\x88\x80" },
  { 0x00020400, "\xF0\xA0\x90\x80" },
  { 0x00020800, "\xF0\xA0\xA0\x80" },
  { 0x00021000, "\xF0\xA1\x80\x80" },
  { 0x00022000, "\xF0\xA2\x80\x80" },
  { 0x00024000, "\xF0\xA4\x80\x80" },
  { 0x00028000, "\xF0\xA8\x80\x80" },
  { 0x00030000, "\xF0\xB0\x80\x80" },
  { 0x0003FFFF, "\xF0\xBF\xBF\xBF" },
  { 0x00040000, "\xF1\x80\x80\x80" },
  { 0x00040001, "\xF1\x80\x80\x81" },
  { 0x00040002, "\xF1\x80\x80\x82" },
  { 0x00040004, "\xF1\x80\x80\x84" },
  { 0x00040008, "\xF1\x80\x80\x88" },
  { 0x00040010, "\xF1\x80\x80\x90" },
  { 0x00040020, "\xF1\x80\x80\xA0" },
  { 0x00040040, "\xF1\x80\x81\x80" },
  { 0x00040080, "\xF1\x80\x82\x80" },
  { 0x00040100, "\xF1\x80\x84\x80" },
  { 0x00040200, "\xF1\x80\x88\x80" },
  { 0x00040400, "\xF1\x80\x90\x80" },
  { 0x00040800, "\xF1\x80\xA0\x80" },
  { 0x00041000, "\xF1\x81\x80\x80" },
  { 0x00042000, "\xF1\x82\x80\x80" },
  { 0x00044000, "\xF1\x84\x80\x80" },
  { 0x00048000, "\xF1\x88\x80\x80" },
  { 0x00050000, "\xF1\x90\x80\x80" },
  { 0x00060000, "\xF1\xA0\x80\x80" },
  { 0x0007FFFF, "\xF1\xBF\xBF\xBF" },
  { 0x00080000, "\xF2\x80\x80\x80" },
  { 0x00080001, "\xF2\x80\x80\x81" },
  { 0x00080002, "\xF2\x80\x80\x82" },
  { 0x00080004, "\xF2\x80\x80\x84" },
  { 0x00080008, "\xF2\x80\x80\x88" },
  { 0x00080010, "\xF2\x80\x80\x90" },
  { 0x00080020, "\xF2\x80\x80\xA0" },
  { 0x00080040, "\xF2\x80\x81\x80" },
  { 0x00080080, "\xF2\x80\x82\x80" },
  { 0x00080100, "\xF2\x80\x84\x80" },
  { 0x00080200, "\xF2\x80\x88\x80" },
  { 0x00080400, "\xF2\x80\x90\x80" },
  { 0x00080800, "\xF2\x80\xA0\x80" },
  { 0x00081000, "\xF2\x81\x80\x80" },
  { 0x00082000, "\xF2\x82\x80\x80" },
  { 0x00084000, "\xF2\x84\x80\x80" },
  { 0x00088000, "\xF2\x88\x80\x80" },
  { 0x00090000, "\xF2\x90\x80\x80" },
  { 0x000A0000, "\xF2\xA0\x80\x80" },
  { 0x000C0000, "\xF3\x80\x80\x80" },
  { 0x000FFFFF, "\xF3\xBF\xBF\xBF" },
  { 0x00100000, "\xF4\x80\x80\x80" },
  { 0x00100001, "\xF4\x80\x80\x81" },
  { 0x00100002, "\xF4\x80\x80\x82" },
  { 0x00100004, "\xF4\x80\x80\x84" },
  { 0x00100008, "\xF4\x80\x80\x88" },
  { 0x00100010, "\xF4\x80\x80\x90" },
  { 0x00100020, "\xF4\x80\x80\xA0" },
  { 0x00100040, "\xF4\x80\x81\x80" },
  { 0x00100080, "\xF4\x80\x82\x80" },
  { 0x00100100, "\xF4\x80\x84\x80" },
  { 0x00100200, "\xF4\x80\x88\x80" },
  { 0x00100400, "\xF4\x80\x90\x80" },
  { 0x00100800, "\xF4\x80\xA0\x80" },
  { 0x00101000, "\xF4\x81\x80\x80" },
  { 0x00102000, "\xF4\x82\x80\x80" },
  { 0x00104000, "\xF4\x84\x80\x80" },
  { 0x00108000, "\xF4\x88\x80\x80" },
  { 0x00110000, "\xF4\x90\x80\x80" },
  { 0x00120000, "\xF4\xA0\x80\x80" },
  { 0x00140000, "\xF5\x80\x80\x80" },
  { 0x00180000, "\xF6\x80\x80\x80" },
  { 0x001FFFFF, "\xF7\xBF\xBF\xBF" },
          
  { 0x00200000, "\xF8\x88\x80\x80\x80" },
  { 0x00200001, "\xF8\x88\x80\x80\x81" },
  { 0x00200002, "\xF8\x88\x80\x80\x82" },
  { 0x00200004, "\xF8\x88\x80\x80\x84" },
  { 0x00200008, "\xF8\x88\x80\x80\x88" },
  { 0x00200010, "\xF8\x88\x80\x80\x90" },
  { 0x00200020, "\xF8\x88\x80\x80\xA0" },
  { 0x00200040, "\xF8\x88\x80\x81\x80" },
  { 0x00200080, "\xF8\x88\x80\x82\x80" },
  { 0x00200100, "\xF8\x88\x80\x84\x80" },
  { 0x00200200, "\xF8\x88\x80\x88\x80" },
  { 0x00200400, "\xF8\x88\x80\x90\x80" },
  { 0x00200800, "\xF8\x88\x80\xA0\x80" },
  { 0x00201000, "\xF8\x88\x81\x80\x80" },
  { 0x00202000, "\xF8\x88\x82\x80\x80" },
  { 0x00204000, "\xF8\x88\x84\x80\x80" },
  { 0x00208000, "\xF8\x88\x88\x80\x80" },
  { 0x00210000, "\xF8\x88\x90\x80\x80" },
  { 0x00220000, "\xF8\x88\xA0\x80\x80" },
  { 0x00240000, "\xF8\x89\x80\x80\x80" },
  { 0x00280000, "\xF8\x8A\x80\x80\x80" },
  { 0x00300000, "\xF8\x8C\x80\x80\x80" },
  { 0x003FFFFF, "\xF8\x8F\xBF\xBF\xBF" },
  { 0x00400000, "\xF8\x90\x80\x80\x80" },
  { 0x00400001, "\xF8\x90\x80\x80\x81" },
  { 0x00400002, "\xF8\x90\x80\x80\x82" },
  { 0x00400004, "\xF8\x90\x80\x80\x84" },
  { 0x00400008, "\xF8\x90\x80\x80\x88" },
  { 0x00400010, "\xF8\x90\x80\x80\x90" },
  { 0x00400020, "\xF8\x90\x80\x80\xA0" },
  { 0x00400040, "\xF8\x90\x80\x81\x80" },
  { 0x00400080, "\xF8\x90\x80\x82\x80" },
  { 0x00400100, "\xF8\x90\x80\x84\x80" },
  { 0x00400200, "\xF8\x90\x80\x88\x80" },
  { 0x00400400, "\xF8\x90\x80\x90\x80" },
  { 0x00400800, "\xF8\x90\x80\xA0\x80" },
  { 0x00401000, "\xF8\x90\x81\x80\x80" },
  { 0x00402000, "\xF8\x90\x82\x80\x80" },
  { 0x00404000, "\xF8\x90\x84\x80\x80" },
  { 0x00408000, "\xF8\x90\x88\x80\x80" },
  { 0x00410000, "\xF8\x90\x90\x80\x80" },
  { 0x00420000, "\xF8\x90\xA0\x80\x80" },
  { 0x00440000, "\xF8\x91\x80\x80\x80" },
  { 0x00480000, "\xF8\x92\x80\x80\x80" },
  { 0x00500000, "\xF8\x94\x80\x80\x80" },
  { 0x00600000, "\xF8\x98\x80\x80\x80" },
  { 0x007FFFFF, "\xF8\x9F\xBF\xBF\xBF" },
  { 0x00800000, "\xF8\xA0\x80\x80\x80" },
  { 0x00800001, "\xF8\xA0\x80\x80\x81" },
  { 0x00800002, "\xF8\xA0\x80\x80\x82" },
  { 0x00800004, "\xF8\xA0\x80\x80\x84" },
  { 0x00800008, "\xF8\xA0\x80\x80\x88" },
  { 0x00800010, "\xF8\xA0\x80\x80\x90" },
  { 0x00800020, "\xF8\xA0\x80\x80\xA0" },
  { 0x00800040, "\xF8\xA0\x80\x81\x80" },
  { 0x00800080, "\xF8\xA0\x80\x82\x80" },
  { 0x00800100, "\xF8\xA0\x80\x84\x80" },
  { 0x00800200, "\xF8\xA0\x80\x88\x80" },
  { 0x00800400, "\xF8\xA0\x80\x90\x80" },
  { 0x00800800, "\xF8\xA0\x80\xA0\x80" },
  { 0x00801000, "\xF8\xA0\x81\x80\x80" },
  { 0x00802000, "\xF8\xA0\x82\x80\x80" },
  { 0x00804000, "\xF8\xA0\x84\x80\x80" },
  { 0x00808000, "\xF8\xA0\x88\x80\x80" },
  { 0x00810000, "\xF8\xA0\x90\x80\x80" },
  { 0x00820000, "\xF8\xA0\xA0\x80\x80" },
  { 0x00840000, "\xF8\xA1\x80\x80\x80" },
  { 0x00880000, "\xF8\xA2\x80\x80\x80" },
  { 0x00900000, "\xF8\xA4\x80\x80\x80" },
  { 0x00A00000, "\xF8\xA8\x80\x80\x80" },
  { 0x00C00000, "\xF8\xB0\x80\x80\x80" },
  { 0x00FFFFFF, "\xF8\xBF\xBF\xBF\xBF" },
  { 0x01000000, "\xF9\x80\x80\x80\x80" },
  { 0x01000001, "\xF9\x80\x80\x80\x81" },
  { 0x01000002, "\xF9\x80\x80\x80\x82" },
  { 0x01000004, "\xF9\x80\x80\x80\x84" },
  { 0x01000008, "\xF9\x80\x80\x80\x88" },
  { 0x01000010, "\xF9\x80\x80\x80\x90" },
  { 0x01000020, "\xF9\x80\x80\x80\xA0" },
  { 0x01000040, "\xF9\x80\x80\x81\x80" },
  { 0x01000080, "\xF9\x80\x80\x82\x80" },
  { 0x01000100, "\xF9\x80\x80\x84\x80" },
  { 0x01000200, "\xF9\x80\x80\x88\x80" },
  { 0x01000400, "\xF9\x80\x80\x90\x80" },
  { 0x01000800, "\xF9\x80\x80\xA0\x80" },
  { 0x01001000, "\xF9\x80\x81\x80\x80" },
  { 0x01002000, "\xF9\x80\x82\x80\x80" },
  { 0x01004000, "\xF9\x80\x84\x80\x80" },
  { 0x01008000, "\xF9\x80\x88\x80\x80" },
  { 0x01010000, "\xF9\x80\x90\x80\x80" },
  { 0x01020000, "\xF9\x80\xA0\x80\x80" },
  { 0x01040000, "\xF9\x81\x80\x80\x80" },
  { 0x01080000, "\xF9\x82\x80\x80\x80" },
  { 0x01100000, "\xF9\x84\x80\x80\x80" },
  { 0x01200000, "\xF9\x88\x80\x80\x80" },
  { 0x01400000, "\xF9\x90\x80\x80\x80" },
  { 0x01800000, "\xF9\xA0\x80\x80\x80" },
  { 0x01FFFFFF, "\xF9\xBF\xBF\xBF\xBF" },
  { 0x02000000, "\xFA\x80\x80\x80\x80" },
  { 0x02000001, "\xFA\x80\x80\x80\x81" },
  { 0x02000002, "\xFA\x80\x80\x80\x82" },
  { 0x02000004, "\xFA\x80\x80\x80\x84" },
  { 0x02000008, "\xFA\x80\x80\x80\x88" },
  { 0x02000010, "\xFA\x80\x80\x80\x90" },
  { 0x02000020, "\xFA\x80\x80\x80\xA0" },
  { 0x02000040, "\xFA\x80\x80\x81\x80" },
  { 0x02000080, "\xFA\x80\x80\x82\x80" },
  { 0x02000100, "\xFA\x80\x80\x84\x80" },
  { 0x02000200, "\xFA\x80\x80\x88\x80" },
  { 0x02000400, "\xFA\x80\x80\x90\x80" },
  { 0x02000800, "\xFA\x80\x80\xA0\x80" },
  { 0x02001000, "\xFA\x80\x81\x80\x80" },
  { 0x02002000, "\xFA\x80\x82\x80\x80" },
  { 0x02004000, "\xFA\x80\x84\x80\x80" },
  { 0x02008000, "\xFA\x80\x88\x80\x80" },
  { 0x02010000, "\xFA\x80\x90\x80\x80" },
  { 0x02020000, "\xFA\x80\xA0\x80\x80" },
  { 0x02040000, "\xFA\x81\x80\x80\x80" },
  { 0x02080000, "\xFA\x82\x80\x80\x80" },
  { 0x02100000, "\xFA\x84\x80\x80\x80" },
  { 0x02200000, "\xFA\x88\x80\x80\x80" },
  { 0x02400000, "\xFA\x90\x80\x80\x80" },
  { 0x02800000, "\xFA\xA0\x80\x80\x80" },
  { 0x03000000, "\xFB\x80\x80\x80\x80" },
  { 0x03FFFFFF, "\xFB\xBF\xBF\xBF\xBF" },
          
  { 0x04000000, "\xFC\x84\x80\x80\x80\x80" },
  { 0x04000001, "\xFC\x84\x80\x80\x80\x81" },
  { 0x04000002, "\xFC\x84\x80\x80\x80\x82" },
  { 0x04000004, "\xFC\x84\x80\x80\x80\x84" },
  { 0x04000008, "\xFC\x84\x80\x80\x80\x88" },
  { 0x04000010, "\xFC\x84\x80\x80\x80\x90" },
  { 0x04000020, "\xFC\x84\x80\x80\x80\xA0" },
  { 0x04000040, "\xFC\x84\x80\x80\x81\x80" },
  { 0x04000080, "\xFC\x84\x80\x80\x82\x80" },
  { 0x04000100, "\xFC\x84\x80\x80\x84\x80" },
  { 0x04000200, "\xFC\x84\x80\x80\x88\x80" },
  { 0x04000400, "\xFC\x84\x80\x80\x90\x80" },
  { 0x04000800, "\xFC\x84\x80\x80\xA0\x80" },
  { 0x04001000, "\xFC\x84\x80\x81\x80\x80" },
  { 0x04002000, "\xFC\x84\x80\x82\x80\x80" },
  { 0x04004000, "\xFC\x84\x80\x84\x80\x80" },
  { 0x04008000, "\xFC\x84\x80\x88\x80\x80" },
  { 0x04010000, "\xFC\x84\x80\x90\x80\x80" },
  { 0x04020000, "\xFC\x84\x80\xA0\x80\x80" },
  { 0x04040000, "\xFC\x84\x81\x80\x80\x80" },
  { 0x04080000, "\xFC\x84\x82\x80\x80\x80" },
  { 0x04100000, "\xFC\x84\x84\x80\x80\x80" },
  { 0x04200000, "\xFC\x84\x88\x80\x80\x80" },
  { 0x04400000, "\xFC\x84\x90\x80\x80\x80" },
  { 0x04800000, "\xFC\x84\xA0\x80\x80\x80" },
  { 0x05000000, "\xFC\x85\x80\x80\x80\x80" },
  { 0x06000000, "\xFC\x86\x80\x80\x80\x80" },
  { 0x07FFFFFF, "\xFC\x87\xBF\xBF\xBF\xBF" },
  { 0x08000000, "\xFC\x88\x80\x80\x80\x80" },
  { 0x08000001, "\xFC\x88\x80\x80\x80\x81" },
  { 0x08000002, "\xFC\x88\x80\x80\x80\x82" },
  { 0x08000004, "\xFC\x88\x80\x80\x80\x84" },
  { 0x08000008, "\xFC\x88\x80\x80\x80\x88" },
  { 0x08000010, "\xFC\x88\x80\x80\x80\x90" },
  { 0x08000020, "\xFC\x88\x80\x80\x80\xA0" },
  { 0x08000040, "\xFC\x88\x80\x80\x81\x80" },
  { 0x08000080, "\xFC\x88\x80\x80\x82\x80" },
  { 0x08000100, "\xFC\x88\x80\x80\x84\x80" },
  { 0x08000200, "\xFC\x88\x80\x80\x88\x80" },
  { 0x08000400, "\xFC\x88\x80\x80\x90\x80" },
  { 0x08000800, "\xFC\x88\x80\x80\xA0\x80" },
  { 0x08001000, "\xFC\x88\x80\x81\x80\x80" },
  { 0x08002000, "\xFC\x88\x80\x82\x80\x80" },
  { 0x08004000, "\xFC\x88\x80\x84\x80\x80" },
  { 0x08008000, "\xFC\x88\x80\x88\x80\x80" },
  { 0x08010000, "\xFC\x88\x80\x90\x80\x80" },
  { 0x08020000, "\xFC\x88\x80\xA0\x80\x80" },
  { 0x08040000, "\xFC\x88\x81\x80\x80\x80" },
  { 0x08080000, "\xFC\x88\x82\x80\x80\x80" },
  { 0x08100000, "\xFC\x88\x84\x80\x80\x80" },
  { 0x08200000, "\xFC\x88\x88\x80\x80\x80" },
  { 0x08400000, "\xFC\x88\x90\x80\x80\x80" },
  { 0x08800000, "\xFC\x88\xA0\x80\x80\x80" },
  { 0x09000000, "\xFC\x89\x80\x80\x80\x80" },
  { 0x0A000000, "\xFC\x8A\x80\x80\x80\x80" },
  { 0x0C000000, "\xFC\x8C\x80\x80\x80\x80" },
  { 0x0FFFFFFF, "\xFC\x8F\xBF\xBF\xBF\xBF" },
  { 0x10000000, "\xFC\x90\x80\x80\x80\x80" },
  { 0x10000001, "\xFC\x90\x80\x80\x80\x81" },
  { 0x10000002, "\xFC\x90\x80\x80\x80\x82" },
  { 0x10000004, "\xFC\x90\x80\x80\x80\x84" },
  { 0x10000008, "\xFC\x90\x80\x80\x80\x88" },
  { 0x10000010, "\xFC\x90\x80\x80\x80\x90" },
  { 0x10000020, "\xFC\x90\x80\x80\x80\xA0" },
  { 0x10000040, "\xFC\x90\x80\x80\x81\x80" },
  { 0x10000080, "\xFC\x90\x80\x80\x82\x80" },
  { 0x10000100, "\xFC\x90\x80\x80\x84\x80" },
  { 0x10000200, "\xFC\x90\x80\x80\x88\x80" },
  { 0x10000400, "\xFC\x90\x80\x80\x90\x80" },
  { 0x10000800, "\xFC\x90\x80\x80\xA0\x80" },
  { 0x10001000, "\xFC\x90\x80\x81\x80\x80" },
  { 0x10002000, "\xFC\x90\x80\x82\x80\x80" },
  { 0x10004000, "\xFC\x90\x80\x84\x80\x80" },
  { 0x10008000, "\xFC\x90\x80\x88\x80\x80" },
  { 0x10010000, "\xFC\x90\x80\x90\x80\x80" },
  { 0x10020000, "\xFC\x90\x80\xA0\x80\x80" },
  { 0x10040000, "\xFC\x90\x81\x80\x80\x80" },
  { 0x10080000, "\xFC\x90\x82\x80\x80\x80" },
  { 0x10100000, "\xFC\x90\x84\x80\x80\x80" },
  { 0x10200000, "\xFC\x90\x88\x80\x80\x80" },
  { 0x10400000, "\xFC\x90\x90\x80\x80\x80" },
  { 0x10800000, "\xFC\x90\xA0\x80\x80\x80" },
  { 0x11000000, "\xFC\x91\x80\x80\x80\x80" },
  { 0x12000000, "\xFC\x92\x80\x80\x80\x80" },
  { 0x14000000, "\xFC\x94\x80\x80\x80\x80" },
  { 0x18000000, "\xFC\x98\x80\x80\x80\x80" },
  { 0x1FFFFFFF, "\xFC\x9F\xBF\xBF\xBF\xBF" },
  { 0x20000000, "\xFC\xA0\x80\x80\x80\x80" },
  { 0x20000001, "\xFC\xA0\x80\x80\x80\x81" },
  { 0x20000002, "\xFC\xA0\x80\x80\x80\x82" },
  { 0x20000004, "\xFC\xA0\x80\x80\x80\x84" },
  { 0x20000008, "\xFC\xA0\x80\x80\x80\x88" },
  { 0x20000010, "\xFC\xA0\x80\x80\x80\x90" },
  { 0x20000020, "\xFC\xA0\x80\x80\x80\xA0" },
  { 0x20000040, "\xFC\xA0\x80\x80\x81\x80" },
  { 0x20000080, "\xFC\xA0\x80\x80\x82\x80" },
  { 0x20000100, "\xFC\xA0\x80\x80\x84\x80" },
  { 0x20000200, "\xFC\xA0\x80\x80\x88\x80" },
  { 0x20000400, "\xFC\xA0\x80\x80\x90\x80" },
  { 0x20000800, "\xFC\xA0\x80\x80\xA0\x80" },
  { 0x20001000, "\xFC\xA0\x80\x81\x80\x80" },
  { 0x20002000, "\xFC\xA0\x80\x82\x80\x80" },
  { 0x20004000, "\xFC\xA0\x80\x84\x80\x80" },
  { 0x20008000, "\xFC\xA0\x80\x88\x80\x80" },
  { 0x20010000, "\xFC\xA0\x80\x90\x80\x80" },
  { 0x20020000, "\xFC\xA0\x80\xA0\x80\x80" },
  { 0x20040000, "\xFC\xA0\x81\x80\x80\x80" },
  { 0x20080000, "\xFC\xA0\x82\x80\x80\x80" },
  { 0x20100000, "\xFC\xA0\x84\x80\x80\x80" },
  { 0x20200000, "\xFC\xA0\x88\x80\x80\x80" },
  { 0x20400000, "\xFC\xA0\x90\x80\x80\x80" },
  { 0x20800000, "\xFC\xA0\xA0\x80\x80\x80" },
  { 0x21000000, "\xFC\xA1\x80\x80\x80\x80" },
  { 0x22000000, "\xFC\xA2\x80\x80\x80\x80" },
  { 0x24000000, "\xFC\xA4\x80\x80\x80\x80" },
  { 0x28000000, "\xFC\xA8\x80\x80\x80\x80" },
  { 0x30000000, "\xFC\xB0\x80\x80\x80\x80" },
  { 0x3FFFFFFF, "\xFC\xBF\xBF\xBF\xBF\xBF" },
  { 0x40000000, "\xFD\x80\x80\x80\x80\x80" },
  { 0x40000001, "\xFD\x80\x80\x80\x80\x81" },
  { 0x40000002, "\xFD\x80\x80\x80\x80\x82" },
  { 0x40000004, "\xFD\x80\x80\x80\x80\x84" },
  { 0x40000008, "\xFD\x80\x80\x80\x80\x88" },
  { 0x40000010, "\xFD\x80\x80\x80\x80\x90" },
  { 0x40000020, "\xFD\x80\x80\x80\x80\xA0" },
  { 0x40000040, "\xFD\x80\x80\x80\x81\x80" },
  { 0x40000080, "\xFD\x80\x80\x80\x82\x80" },
  { 0x40000100, "\xFD\x80\x80\x80\x84\x80" },
  { 0x40000200, "\xFD\x80\x80\x80\x88\x80" },
  { 0x40000400, "\xFD\x80\x80\x80\x90\x80" },
  { 0x40000800, "\xFD\x80\x80\x80\xA0\x80" },
  { 0x40001000, "\xFD\x80\x80\x81\x80\x80" },
  { 0x40002000, "\xFD\x80\x80\x82\x80\x80" },
  { 0x40004000, "\xFD\x80\x80\x84\x80\x80" },
  { 0x40008000, "\xFD\x80\x80\x88\x80\x80" },
  { 0x40010000, "\xFD\x80\x80\x90\x80\x80" },
  { 0x40020000, "\xFD\x80\x80\xA0\x80\x80" },
  { 0x40040000, "\xFD\x80\x81\x80\x80\x80" },
  { 0x40080000, "\xFD\x80\x82\x80\x80\x80" },
  { 0x40100000, "\xFD\x80\x84\x80\x80\x80" },
  { 0x40200000, "\xFD\x80\x88\x80\x80\x80" },
  { 0x40400000, "\xFD\x80\x90\x80\x80\x80" },
  { 0x40800000, "\xFD\x80\xA0\x80\x80\x80" },
  { 0x41000000, "\xFD\x81\x80\x80\x80\x80" },
  { 0x42000000, "\xFD\x82\x80\x80\x80\x80" },
  { 0x44000000, "\xFD\x84\x80\x80\x80\x80" },
  { 0x48000000, "\xFD\x88\x80\x80\x80\x80" },
  { 0x50000000, "\xFD\x90\x80\x80\x80\x80" },
  { 0x60000000, "\xFD\xA0\x80\x80\x80\x80" },
  { 0x7FFFFFFF, "\xFD\xBF\xBF\xBF\xBF\xBF" }
};

/*
 * UCS-2 vectors
 */

struct ucs2 ucs2[] = {
  { 0x0001, "\x01" },
  { 0x0002, "\x02" },
  { 0x0003, "\x03" },
  { 0x0004, "\x04" },
  { 0x0007, "\x07" },
  { 0x0008, "\x08" },
  { 0x000F, "\x0F" },
  { 0x0010, "\x10" },
  { 0x001F, "\x1F" },
  { 0x0020, "\x20" },
  { 0x003F, "\x3F" },
  { 0x0040, "\x40" },
  { 0x007F, "\x7F" },
          
  { 0x0080, "\xC2\x80" },
  { 0x0081, "\xC2\x81" },
  { 0x0082, "\xC2\x82" },
  { 0x0084, "\xC2\x84" },
  { 0x0088, "\xC2\x88" },
  { 0x0090, "\xC2\x90" },
  { 0x00A0, "\xC2\xA0" },
  { 0x00C0, "\xC3\x80" },
  { 0x00FF, "\xC3\xBF" },
  { 0x0100, "\xC4\x80" },
  { 0x0101, "\xC4\x81" },
  { 0x0102, "\xC4\x82" },
  { 0x0104, "\xC4\x84" },
  { 0x0108, "\xC4\x88" },
  { 0x0110, "\xC4\x90" },
  { 0x0120, "\xC4\xA0" },
  { 0x0140, "\xC5\x80" },
  { 0x0180, "\xC6\x80" },
  { 0x01FF, "\xC7\xBF" },
  { 0x0200, "\xC8\x80" },
  { 0x0201, "\xC8\x81" },
  { 0x0202, "\xC8\x82" },
  { 0x0204, "\xC8\x84" },
  { 0x0208, "\xC8\x88" },
  { 0x0210, "\xC8\x90" },
  { 0x0220, "\xC8\xA0" },
  { 0x0240, "\xC9\x80" },
  { 0x0280, "\xCA\x80" },
  { 0x0300, "\xCC\x80" },
  { 0x03FF, "\xCF\xBF" },
  { 0x0400, "\xD0\x80" },
  { 0x0401, "\xD0\x81" },
  { 0x0402, "\xD0\x82" },
  { 0x0404, "\xD0\x84" },
  { 0x0408, "\xD0\x88" },
  { 0x0410, "\xD0\x90" },
  { 0x0420, "\xD0\xA0" },
  { 0x0440, "\xD1\x80" },
  { 0x0480, "\xD2\x80" },
  { 0x0500, "\xD4\x80" },
  { 0x0600, "\xD8\x80" },
  { 0x07FF, "\xDF\xBF" },
          
  { 0x0800, "\xE0\xA0\x80" },
  { 0x0801, "\xE0\xA0\x81" },
  { 0x0802, "\xE0\xA0\x82" },
  { 0x0804, "\xE0\xA0\x84" },
  { 0x0808, "\xE0\xA0\x88" },
  { 0x0810, "\xE0\xA0\x90" },
  { 0x0820, "\xE0\xA0\xA0" },
  { 0x0840, "\xE0\xA1\x80" },
  { 0x0880, "\xE0\xA2\x80" },
  { 0x0900, "\xE0\xA4\x80" },
  { 0x0A00, "\xE0\xA8\x80" },
  { 0x0C00, "\xE0\xB0\x80" },
  { 0x0FFF, "\xE0\xBF\xBF" },
  { 0x1000, "\xE1\x80\x80" },
  { 0x1001, "\xE1\x80\x81" },
  { 0x1002, "\xE1\x80\x82" },
  { 0x1004, "\xE1\x80\x84" },
  { 0x1008, "\xE1\x80\x88" },
  { 0x1010, "\xE1\x80\x90" },
  { 0x1020, "\xE1\x80\xA0" },
  { 0x1040, "\xE1\x81\x80" },
  { 0x1080, "\xE1\x82\x80" },
  { 0x1100, "\xE1\x84\x80" },
  { 0x1200, "\xE1\x88\x80" },
  { 0x1400, "\xE1\x90\x80" },
  { 0x1800, "\xE1\xA0\x80" },
  { 0x1FFF, "\xE1\xBF\xBF" },
  { 0x2000, "\xE2\x80\x80" },
  { 0x2001, "\xE2\x80\x81" },
  { 0x2002, "\xE2\x80\x82" },
  { 0x2004, "\xE2\x80\x84" },
  { 0x2008, "\xE2\x80\x88" },
  { 0x2010, "\xE2\x80\x90" },
  { 0x2020, "\xE2\x80\xA0" },
  { 0x2040, "\xE2\x81\x80" },
  { 0x2080, "\xE2\x82\x80" },
  { 0x2100, "\xE2\x84\x80" },
  { 0x2200, "\xE2\x88\x80" },
  { 0x2400, "\xE2\x90\x80" },
  { 0x2800, "\xE2\xA0\x80" },
  { 0x3000, "\xE3\x80\x80" },
  { 0x3FFF, "\xE3\xBF\xBF" },
  { 0x4000, "\xE4\x80\x80" },
  { 0x4001, "\xE4\x80\x81" },
  { 0x4002, "\xE4\x80\x82" },
  { 0x4004, "\xE4\x80\x84" },
  { 0x4008, "\xE4\x80\x88" },
  { 0x4010, "\xE4\x80\x90" },
  { 0x4020, "\xE4\x80\xA0" },
  { 0x4040, "\xE4\x81\x80" },
  { 0x4080, "\xE4\x82\x80" },
  { 0x4100, "\xE4\x84\x80" },
  { 0x4200, "\xE4\x88\x80" },
  { 0x4400, "\xE4\x90\x80" },
  { 0x4800, "\xE4\xA0\x80" },
  { 0x5000, "\xE5\x80\x80" },
  { 0x6000, "\xE6\x80\x80" },
  { 0x7FFF, "\xE7\xBF\xBF" },
  { 0x8000, "\xE8\x80\x80" },
  { 0x8001, "\xE8\x80\x81" },
  { 0x8002, "\xE8\x80\x82" },
  { 0x8004, "\xE8\x80\x84" },
  { 0x8008, "\xE8\x80\x88" },
  { 0x8010, "\xE8\x80\x90" },
  { 0x8020, "\xE8\x80\xA0" },
  { 0x8040, "\xE8\x81\x80" },
  { 0x8080, "\xE8\x82\x80" },
  { 0x8100, "\xE8\x84\x80" },
  { 0x8200, "\xE8\x88\x80" },
  { 0x8400, "\xE8\x90\x80" },
  { 0x8800, "\xE8\xA0\x80" },
  { 0x9000, "\xE9\x80\x80" },
  { 0xA000, "\xEA\x80\x80" },
  { 0xC000, "\xEC\x80\x80" },
  { 0xFFFF, "\xEF\xBF\xBF" }

};

#ifdef UTF16
/*
 * UTF-16 vectors
 */

struct utf16 utf16[] = {
  { 0x00010000, { 0xD800, 0xDC00 } },
  { 0x00010001, { 0xD800, 0xDC01 } },
  { 0x00010002, { 0xD800, 0xDC02 } },
  { 0x00010003, { 0xD800, 0xDC03 } },
  { 0x00010004, { 0xD800, 0xDC04 } },
  { 0x00010007, { 0xD800, 0xDC07 } },
  { 0x00010008, { 0xD800, 0xDC08 } },
  { 0x0001000F, { 0xD800, 0xDC0F } },
  { 0x00010010, { 0xD800, 0xDC10 } },
  { 0x0001001F, { 0xD800, 0xDC1F } },
  { 0x00010020, { 0xD800, 0xDC20 } },
  { 0x0001003F, { 0xD800, 0xDC3F } },
  { 0x00010040, { 0xD800, 0xDC40 } },
  { 0x0001007F, { 0xD800, 0xDC7F } },
  { 0x00010080, { 0xD800, 0xDC80 } },
  { 0x00010081, { 0xD800, 0xDC81 } },
  { 0x00010082, { 0xD800, 0xDC82 } },
  { 0x00010084, { 0xD800, 0xDC84 } },
  { 0x00010088, { 0xD800, 0xDC88 } },
  { 0x00010090, { 0xD800, 0xDC90 } },
  { 0x000100A0, { 0xD800, 0xDCA0 } },
  { 0x000100C0, { 0xD800, 0xDCC0 } },
  { 0x000100FF, { 0xD800, 0xDCFF } },
  { 0x00010100, { 0xD800, 0xDD00 } },
  { 0x00010101, { 0xD800, 0xDD01 } },
  { 0x00010102, { 0xD800, 0xDD02 } },
  { 0x00010104, { 0xD800, 0xDD04 } },
  { 0x00010108, { 0xD800, 0xDD08 } },
  { 0x00010110, { 0xD800, 0xDD10 } },
  { 0x00010120, { 0xD800, 0xDD20 } },
  { 0x00010140, { 0xD800, 0xDD40 } },
  { 0x00010180, { 0xD800, 0xDD80 } },
  { 0x000101FF, { 0xD800, 0xDDFF } },
  { 0x00010200, { 0xD800, 0xDE00 } },
  { 0x00010201, { 0xD800, 0xDE01 } },
  { 0x00010202, { 0xD800, 0xDE02 } },
  { 0x00010204, { 0xD800, 0xDE04 } },
  { 0x00010208, { 0xD800, 0xDE08 } },
  { 0x00010210, { 0xD800, 0xDE10 } },
  { 0x00010220, { 0xD800, 0xDE20 } },
  { 0x00010240, { 0xD800, 0xDE40 } },
  { 0x00010280, { 0xD800, 0xDE80 } },
  { 0x00010300, { 0xD800, 0xDF00 } },
  { 0x000103FF, { 0xD800, 0xDFFF } },
  { 0x00010400, { 0xD801, 0xDC00 } },
  { 0x00010401, { 0xD801, 0xDC01 } },
  { 0x00010402, { 0xD801, 0xDC02 } },
  { 0x00010404, { 0xD801, 0xDC04 } },
  { 0x00010408, { 0xD801, 0xDC08 } },
  { 0x00010410, { 0xD801, 0xDC10 } },
  { 0x00010420, { 0xD801, 0xDC20 } },
  { 0x00010440, { 0xD801, 0xDC40 } },
  { 0x00010480, { 0xD801, 0xDC80 } },
  { 0x00010500, { 0xD801, 0xDD00 } },
  { 0x00010600, { 0xD801, 0xDE00 } },
  { 0x000107FF, { 0xD801, 0xDFFF } },
  { 0x00010800, { 0xD802, 0xDC00 } },
  { 0x00010801, { 0xD802, 0xDC01 } },
  { 0x00010802, { 0xD802, 0xDC02 } },
  { 0x00010804, { 0xD802, 0xDC04 } },
  { 0x00010808, { 0xD802, 0xDC08 } },
  { 0x00010810, { 0xD802, 0xDC10 } },
  { 0x00010820, { 0xD802, 0xDC20 } },
  { 0x00010840, { 0xD802, 0xDC40 } },
  { 0x00010880, { 0xD802, 0xDC80 } },
  { 0x00010900, { 0xD802, 0xDD00 } },
  { 0x00010A00, { 0xD802, 0xDE00 } },
  { 0x00010C00, { 0xD803, 0xDC00 } },
  { 0x00010FFF, { 0xD803, 0xDFFF } },
  { 0x00011000, { 0xD804, 0xDC00 } },
  { 0x00011001, { 0xD804, 0xDC01 } },
  { 0x00011002, { 0xD804, 0xDC02 } },
  { 0x00011004, { 0xD804, 0xDC04 } },
  { 0x00011008, { 0xD804, 0xDC08 } },
  { 0x00011010, { 0xD804, 0xDC10 } },
  { 0x00011020, { 0xD804, 0xDC20 } },
  { 0x00011040, { 0xD804, 0xDC40 } },
  { 0x00011080, { 0xD804, 0xDC80 } },
  { 0x00011100, { 0xD804, 0xDD00 } },
  { 0x00011200, { 0xD804, 0xDE00 } },
  { 0x00011400, { 0xD805, 0xDC00 } },
  { 0x00011800, { 0xD806, 0xDC00 } },
  { 0x00011FFF, { 0xD807, 0xDFFF } },
  { 0x00012000, { 0xD808, 0xDC00 } },
  { 0x00012001, { 0xD808, 0xDC01 } },
  { 0x00012002, { 0xD808, 0xDC02 } },
  { 0x00012004, { 0xD808, 0xDC04 } },
  { 0x00012008, { 0xD808, 0xDC08 } },
  { 0x00012010, { 0xD808, 0xDC10 } },
  { 0x00012020, { 0xD808, 0xDC20 } },
  { 0x00012040, { 0xD808, 0xDC40 } },
  { 0x00012080, { 0xD808, 0xDC80 } },
  { 0x00012100, { 0xD808, 0xDD00 } },
  { 0x00012200, { 0xD808, 0xDE00 } },
  { 0x00012400, { 0xD809, 0xDC00 } },
  { 0x00012800, { 0xD80A, 0xDC00 } },
  { 0x00013000, { 0xD80C, 0xDC00 } },
  { 0x00013FFF, { 0xD80F, 0xDFFF } },
  { 0x00014000, { 0xD810, 0xDC00 } },
  { 0x00014001, { 0xD810, 0xDC01 } },
  { 0x00014002, { 0xD810, 0xDC02 } },
  { 0x00014004, { 0xD810, 0xDC04 } },
  { 0x00014008, { 0xD810, 0xDC08 } },
  { 0x00014010, { 0xD810, 0xDC10 } },
  { 0x00014020, { 0xD810, 0xDC20 } },
  { 0x00014040, { 0xD810, 0xDC40 } },
  { 0x00014080, { 0xD810, 0xDC80 } },
  { 0x00014100, { 0xD810, 0xDD00 } },
  { 0x00014200, { 0xD810, 0xDE00 } },
  { 0x00014400, { 0xD811, 0xDC00 } },
  { 0x00014800, { 0xD812, 0xDC00 } },
  { 0x00015000, { 0xD814, 0xDC00 } },
  { 0x00016000, { 0xD818, 0xDC00 } },
  { 0x00017FFF, { 0xD81F, 0xDFFF } },
  { 0x00018000, { 0xD820, 0xDC00 } },
  { 0x00018001, { 0xD820, 0xDC01 } },
  { 0x00018002, { 0xD820, 0xDC02 } },
  { 0x00018004, { 0xD820, 0xDC04 } },
  { 0x00018008, { 0xD820, 0xDC08 } },
  { 0x00018010, { 0xD820, 0xDC10 } },
  { 0x00018020, { 0xD820, 0xDC20 } },
  { 0x00018040, { 0xD820, 0xDC40 } },
  { 0x00018080, { 0xD820, 0xDC80 } },
  { 0x00018100, { 0xD820, 0xDD00 } },
  { 0x00018200, { 0xD820, 0xDE00 } },
  { 0x00018400, { 0xD821, 0xDC00 } },
  { 0x00018800, { 0xD822, 0xDC00 } },
  { 0x00019000, { 0xD824, 0xDC00 } },
  { 0x0001A000, { 0xD828, 0xDC00 } },
  { 0x0001C000, { 0xD830, 0xDC00 } },
  { 0x0001FFFF, { 0xD83F, 0xDFFF } },
  { 0x00020000, { 0xD840, 0xDC00 } },
  { 0x00020001, { 0xD840, 0xDC01 } },
  { 0x00020002, { 0xD840, 0xDC02 } },
  { 0x00020004, { 0xD840, 0xDC04 } },
  { 0x00020008, { 0xD840, 0xDC08 } },
  { 0x00020010, { 0xD840, 0xDC10 } },
  { 0x00020020, { 0xD840, 0xDC20 } },
  { 0x00020040, { 0xD840, 0xDC40 } },
  { 0x00020080, { 0xD840, 0xDC80 } },
  { 0x00020100, { 0xD840, 0xDD00 } },
  { 0x00020200, { 0xD840, 0xDE00 } },
  { 0x00020400, { 0xD841, 0xDC00 } },
  { 0x00020800, { 0xD842, 0xDC00 } },
  { 0x00021000, { 0xD844, 0xDC00 } },
  { 0x00022000, { 0xD848, 0xDC00 } },
  { 0x00024000, { 0xD850, 0xDC00 } },
  { 0x00028000, { 0xD860, 0xDC00 } },
  { 0x0002FFFF, { 0xD87F, 0xDFFF } },
  { 0x00030000, { 0xD880, 0xDC00 } },
  { 0x00030001, { 0xD880, 0xDC01 } },
  { 0x00030002, { 0xD880, 0xDC02 } },
  { 0x00030004, { 0xD880, 0xDC04 } },
  { 0x00030008, { 0xD880, 0xDC08 } },
  { 0x00030010, { 0xD880, 0xDC10 } },
  { 0x00030020, { 0xD880, 0xDC20 } },
  { 0x00030040, { 0xD880, 0xDC40 } },
  { 0x00030080, { 0xD880, 0xDC80 } },
  { 0x00030100, { 0xD880, 0xDD00 } },
  { 0x00030200, { 0xD880, 0xDE00 } },
  { 0x00030400, { 0xD881, 0xDC00 } },
  { 0x00030800, { 0xD882, 0xDC00 } },
  { 0x00031000, { 0xD884, 0xDC00 } },
  { 0x00032000, { 0xD888, 0xDC00 } },
  { 0x00034000, { 0xD890, 0xDC00 } },
  { 0x00038000, { 0xD8A0, 0xDC00 } },
  { 0x0003FFFF, { 0xD8BF, 0xDFFF } },
  { 0x00040000, { 0xD8C0, 0xDC00 } },
  { 0x00040001, { 0xD8C0, 0xDC01 } },
  { 0x00040002, { 0xD8C0, 0xDC02 } },
  { 0x00040004, { 0xD8C0, 0xDC04 } },
  { 0x00040008, { 0xD8C0, 0xDC08 } },
  { 0x00040010, { 0xD8C0, 0xDC10 } },
  { 0x00040020, { 0xD8C0, 0xDC20 } },
  { 0x00040040, { 0xD8C0, 0xDC40 } },
  { 0x00040080, { 0xD8C0, 0xDC80 } },
  { 0x00040100, { 0xD8C0, 0xDD00 } },
  { 0x00040200, { 0xD8C0, 0xDE00 } },
  { 0x00040400, { 0xD8C1, 0xDC00 } },
  { 0x00040800, { 0xD8C2, 0xDC00 } },
  { 0x00041000, { 0xD8C4, 0xDC00 } },
  { 0x00042000, { 0xD8C8, 0xDC00 } },
  { 0x00044000, { 0xD8D0, 0xDC00 } },
  { 0x00048000, { 0xD8E0, 0xDC00 } },
  { 0x0004FFFF, { 0xD8FF, 0xDFFF } },
  { 0x00050000, { 0xD900, 0xDC00 } },
  { 0x00050001, { 0xD900, 0xDC01 } },
  { 0x00050002, { 0xD900, 0xDC02 } },
  { 0x00050004, { 0xD900, 0xDC04 } },
  { 0x00050008, { 0xD900, 0xDC08 } },
  { 0x00050010, { 0xD900, 0xDC10 } },
  { 0x00050020, { 0xD900, 0xDC20 } },
  { 0x00050040, { 0xD900, 0xDC40 } },
  { 0x00050080, { 0xD900, 0xDC80 } },
  { 0x00050100, { 0xD900, 0xDD00 } },
  { 0x00050200, { 0xD900, 0xDE00 } },
  { 0x00050400, { 0xD901, 0xDC00 } },
  { 0x00050800, { 0xD902, 0xDC00 } },
  { 0x00051000, { 0xD904, 0xDC00 } },
  { 0x00052000, { 0xD908, 0xDC00 } },
  { 0x00054000, { 0xD910, 0xDC00 } },
  { 0x00058000, { 0xD920, 0xDC00 } },
  { 0x00060000, { 0xD940, 0xDC00 } },
  { 0x00070000, { 0xD980, 0xDC00 } },
  { 0x0007FFFF, { 0xD9BF, 0xDFFF } },
  { 0x00080000, { 0xD9C0, 0xDC00 } },
  { 0x00080001, { 0xD9C0, 0xDC01 } },
  { 0x00080002, { 0xD9C0, 0xDC02 } },
  { 0x00080004, { 0xD9C0, 0xDC04 } },
  { 0x00080008, { 0xD9C0, 0xDC08 } },
  { 0x00080010, { 0xD9C0, 0xDC10 } },
  { 0x00080020, { 0xD9C0, 0xDC20 } },
  { 0x00080040, { 0xD9C0, 0xDC40 } },
  { 0x00080080, { 0xD9C0, 0xDC80 } },
  { 0x00080100, { 0xD9C0, 0xDD00 } },
  { 0x00080200, { 0xD9C0, 0xDE00 } },
  { 0x00080400, { 0xD9C1, 0xDC00 } },
  { 0x00080800, { 0xD9C2, 0xDC00 } },
  { 0x00081000, { 0xD9C4, 0xDC00 } },
  { 0x00082000, { 0xD9C8, 0xDC00 } },
  { 0x00084000, { 0xD9D0, 0xDC00 } },
  { 0x00088000, { 0xD9E0, 0xDC00 } },
  { 0x0008FFFF, { 0xD9FF, 0xDFFF } },
  { 0x00090000, { 0xDA00, 0xDC00 } },
  { 0x00090001, { 0xDA00, 0xDC01 } },
  { 0x00090002, { 0xDA00, 0xDC02 } },
  { 0x00090004, { 0xDA00, 0xDC04 } },
  { 0x00090008, { 0xDA00, 0xDC08 } },
  { 0x00090010, { 0xDA00, 0xDC10 } },
  { 0x00090020, { 0xDA00, 0xDC20 } },
  { 0x00090040, { 0xDA00, 0xDC40 } },
  { 0x00090080, { 0xDA00, 0xDC80 } },
  { 0x00090100, { 0xDA00, 0xDD00 } },
  { 0x00090200, { 0xDA00, 0xDE00 } },
  { 0x00090400, { 0xDA01, 0xDC00 } },
  { 0x00090800, { 0xDA02, 0xDC00 } },
  { 0x00091000, { 0xDA04, 0xDC00 } },
  { 0x00092000, { 0xDA08, 0xDC00 } },
  { 0x00094000, { 0xDA10, 0xDC00 } },
  { 0x00098000, { 0xDA20, 0xDC00 } },
  { 0x000A0000, { 0xDA40, 0xDC00 } },
  { 0x000B0000, { 0xDA80, 0xDC00 } },
  { 0x000C0000, { 0xDAC0, 0xDC00 } },
  { 0x000D0000, { 0xDB00, 0xDC00 } },
  { 0x000FFFFF, { 0xDBBF, 0xDFFF } },
  { 0x0010FFFF, { 0xDBFF, 0xDFFF } }

};
#endif /* UTF16 */

static void
dump_utf8
(
  char *word,
  unsigned char *utf8,
  char *end
)
{
  fprintf(stdout, "%s ", word);
  for( ; *utf8; utf8++ ) {
    fprintf(stdout, "%02.2x ", (unsigned int)*utf8);
  }
  fprintf(stdout, "%s", end);
}

static PRBool
test_ucs4_chars
(
  void
)
{
  PRBool rv = PR_TRUE;
  int i;

  for( i = 0; i < sizeof(ucs4)/sizeof(ucs4[0]); i++ ) {
    struct ucs4 *e = &ucs4[i];
    PRBool result;
    unsigned char utf8[8];
    unsigned int len = 0;
    PRUint32 back = 0;

    (void)memset(utf8, 0, sizeof(utf8));
    
    result = sec_port_ucs4_utf8_conversion_function(PR_FALSE, 
      (unsigned char *)&e->c, sizeof(e->c), utf8, sizeof(utf8), &len);

    if( !result ) {
      fprintf(stdout, "Failed to convert UCS-4 0x%08.8x to UTF-8\n", e->c);
      rv = PR_FALSE;
      continue;
    }

    if( (len >= sizeof(utf8)) ||
        (strlen(e->utf8) != len) ||
        (utf8[len] = '\0', 0 != strcmp(e->utf8, utf8)) ) {
      fprintf(stdout, "Wrong conversion of UCS-4 0x%08.8x to UTF-8: ", e->c);
      dump_utf8("expected", e->utf8, ", ");
      dump_utf8("received", utf8, "\n");
      rv = PR_FALSE;
      continue;
    }

    result = sec_port_ucs4_utf8_conversion_function(PR_TRUE,
      utf8, len, (unsigned char *)&back, sizeof(back), &len);

    if( !result ) {
      dump_utf8("Failed to convert UTF-8", utf8, "to UCS-4\n");
      rv = PR_FALSE;
      continue;
    }

    if( (sizeof(back) != len) || (e->c != back) ) {
      dump_utf8("Wrong conversion of UTF-8", utf8, " to UCS-4:");
      fprintf(stdout, "expected 0x%08.8x, received 0x%08.8x\n", e->c, back);
      rv = PR_FALSE;
      continue;
    }
  }

  return rv;
}

static PRBool
test_ucs2_chars
(
  void
)
{
  PRBool rv = PR_TRUE;
  int i;

  for( i = 0; i < sizeof(ucs2)/sizeof(ucs2[0]); i++ ) {
    struct ucs2 *e = &ucs2[i];
    PRBool result;
    unsigned char utf8[8];
    unsigned int len = 0;
    PRUint16 back = 0;

    (void)memset(utf8, 0, sizeof(utf8));
    
    result = sec_port_ucs2_utf8_conversion_function(PR_FALSE,
      (unsigned char *)&e->c, sizeof(e->c), utf8, sizeof(utf8), &len);

    if( !result ) {
      fprintf(stdout, "Failed to convert UCS-2 0x%04.4x to UTF-8\n", e->c);
      rv = PR_FALSE;
      continue;
    }

    if( (len >= sizeof(utf8)) ||
        (strlen(e->utf8) != len) ||
        (utf8[len] = '\0', 0 != strcmp(e->utf8, utf8)) ) {
      fprintf(stdout, "Wrong conversion of UCS-2 0x%04.4x to UTF-8: ", e->c);
      dump_utf8("expected", e->utf8, ", ");
      dump_utf8("received", utf8, "\n");
      rv = PR_FALSE;
      continue;
    }

    result = sec_port_ucs2_utf8_conversion_function(PR_TRUE,
      utf8, len, (unsigned char *)&back, sizeof(back), &len);

    if( !result ) {
      dump_utf8("Failed to convert UTF-8", utf8, "to UCS-2\n");
      rv = PR_FALSE;
      continue;
    }

    if( (sizeof(back) != len) || (e->c != back) ) {
      dump_utf8("Wrong conversion of UTF-8", utf8, "to UCS-2:");
      fprintf(stdout, "expected 0x%08.8x, received 0x%08.8x\n", e->c, back);
      rv = PR_FALSE;
      continue;
    }
  }

  return rv;
}

#ifdef UTF16
static PRBool
test_utf16_chars
(
  void
)
{
  PRBool rv = PR_TRUE;
  int i;

  for( i = 0; i < sizeof(utf16)/sizeof(utf16[0]); i++ ) {
    struct utf16 *e = &utf16[i];
    PRBool result;
    unsigned char utf8[8];
    unsigned int len = 0;
    PRUint32 back32 = 0;
    PRUint16 back[2];

    (void)memset(utf8, 0, sizeof(utf8));
    
    result = sec_port_ucs2_utf8_conversion_function(PR_FALSE, 
      (unsigned char *)&e->w[0], sizeof(e->w), utf8, sizeof(utf8), &len);

    if( !result ) {
      fprintf(stdout, "Failed to convert UTF-16 0x%04.4x 0x%04.4x to UTF-8\n", 
              e->w[0], e->w[1]);
      rv = PR_FALSE;
      continue;
    }

    result = sec_port_ucs4_utf8_conversion_function(PR_TRUE,
      utf8, len, (unsigned char *)&back32, sizeof(back32), &len);

    if( 4 != len ) {
      fprintf(stdout, "Failed to convert UTF-16 0x%04.4x 0x%04.4x to UTF-8: "
              "unexpected len %d\n", e->w[0], e->w[1], len);
      rv = PR_FALSE;
      continue;
    }

    utf8[len] = '\0'; /* null-terminate for printing */

    if( !result ) {
      dump_utf8("Failed to convert UTF-8", utf8, "to UCS-4 (utf-16 test)\n");
      rv = PR_FALSE;
      continue;
    }

    if( (sizeof(back32) != len) || (e->c != back32) ) {
      fprintf(stdout, "Wrong conversion of UTF-16 0x%04.4x 0x%04.4x ", 
              e->w[0], e->w[1]);
      dump_utf8("to UTF-8", utf8, "and then to UCS-4: ");
      if( sizeof(back32) != len ) {
        fprintf(stdout, "len is %d\n", len);
      } else {
        fprintf(stdout, "expected 0x%08.8x, received 0x%08.8x\n", e->c, back32);
      }
      rv = PR_FALSE;
      continue;
    }

    (void)memset(utf8, 0, sizeof(utf8));
    back[0] = back[1] = 0;

    result = sec_port_ucs4_utf8_conversion_function(PR_FALSE,
      (unsigned char *)&e->c, sizeof(e->c), utf8, sizeof(utf8), &len);

    if( !result ) {
      fprintf(stdout, "Failed to convert UCS-4 0x%08.8x to UTF-8 (utf-16 test)\n",
              e->c);
      rv = PR_FALSE;
      continue;
    }

    result = sec_port_ucs2_utf8_conversion_function(PR_TRUE,
      utf8, len, (unsigned char *)&back[0], sizeof(back), &len);

    if( 4 != len ) {
      fprintf(stdout, "Failed to convert UCS-4 0x%08.8x to UTF-8: "
              "unexpected len %d\n", e->c, len);
      rv = PR_FALSE;
      continue;
    }

    utf8[len] = '\0'; /* null-terminate for printing */

    if( !result ) {
      dump_utf8("Failed to convert UTF-8", utf8, "to UTF-16\n");
      rv = PR_FALSE;
      continue;
    }

    if( (sizeof(back) != len) || (e->w[0] != back[0]) || (e->w[1] != back[1]) ) {
      fprintf(stdout, "Wrong conversion of UCS-4 0x%08.8x to UTF-8", e->c);
      dump_utf8("", utf8, "and then to UTF-16:");
      if( sizeof(back) != len ) {
        fprintf(stdout, "len is %d\n", len);
      } else {
        fprintf(stdout, "expected 0x%04.4x 0x%04.4x, received 0x%04.4x 0x%04.4xx\n",
                e->w[0], e->w[1], back[0], back[1]);
      }
      rv = PR_FALSE;
      continue;
    }
  }

  return rv;
}
#endif /* UTF16 */

static PRBool
test_zeroes
(
  void
)
{
  PRBool rv = PR_TRUE;
  PRBool result;
  PRUint32 lzero = 0;
  PRUint16 szero = 0;
  unsigned char utf8[8];
  unsigned int len = 0;
  PRUint32 lback = 1;
  PRUint16 sback = 1;

  (void)memset(utf8, 1, sizeof(utf8));

  result = sec_port_ucs4_utf8_conversion_function(PR_FALSE, 
    (unsigned char *)&lzero, sizeof(lzero), utf8, sizeof(utf8), &len);

  if( !result ) {
    fprintf(stdout, "Failed to convert UCS-4 0x00000000 to UTF-8\n");
    rv = PR_FALSE;
  } else if( 1 != len ) {
    fprintf(stdout, "Wrong conversion of UCS-4 0x00000000: len = %d\n", len);
    rv = PR_FALSE;
  } else if( '\0' != *utf8 ) {
    fprintf(stdout, "Wrong conversion of UCS-4 0x00000000: expected 00 ,"
            "received %02.2x\n", (unsigned int)*utf8);
    rv = PR_FALSE;
  }

  result = sec_port_ucs4_utf8_conversion_function(PR_TRUE,
    "", 1, (unsigned char *)&lback, sizeof(lback), &len);

  if( !result ) {
    fprintf(stdout, "Failed to convert UTF-8 00 to UCS-4\n");
    rv = PR_FALSE;
  } else if( 4 != len ) {
    fprintf(stdout, "Wrong conversion of UTF-8 00 to UCS-4: len = %d\n", len);
    rv = PR_FALSE;
  } else if( 0 != lback ) {
    fprintf(stdout, "Wrong conversion of UTF-8 00 to UCS-4: "
            "expected 0x00000000, received 0x%08.8x\n", lback);
    rv = PR_FALSE;
  }

  (void)memset(utf8, 1, sizeof(utf8));

  result = sec_port_ucs2_utf8_conversion_function(PR_FALSE, 
    (unsigned char *)&szero, sizeof(szero), utf8, sizeof(utf8), &len);

  if( !result ) {
    fprintf(stdout, "Failed to convert UCS-2 0x0000 to UTF-8\n");
    rv = PR_FALSE;
  } else if( 1 != len ) {
    fprintf(stdout, "Wrong conversion of UCS-2 0x0000: len = %d\n", len);
    rv = PR_FALSE;
  } else if( '\0' != *utf8 ) {
    fprintf(stdout, "Wrong conversion of UCS-2 0x0000: expected 00 ,"
            "received %02.2x\n", (unsigned int)*utf8);
    rv = PR_FALSE;
  }

  result = sec_port_ucs2_utf8_conversion_function(PR_TRUE,
    "", 1, (unsigned char *)&sback, sizeof(sback), &len);

  if( !result ) {
    fprintf(stdout, "Failed to convert UTF-8 00 to UCS-2\n");
    rv = PR_FALSE;
  } else if( 2 != len ) {
    fprintf(stdout, "Wrong conversion of UTF-8 00 to UCS-2: len = %d\n", len);
    rv = PR_FALSE;
  } else if( 0 != sback ) {
    fprintf(stdout, "Wrong conversion of UTF-8 00 to UCS-2: "
            "expected 0x0000, received 0x%04.4x\n", sback);
    rv = PR_FALSE;
  }

  return rv;
}

static PRBool
test_multichars
(
  void
)
{
  int i;
  unsigned int len, lenout;
  PRUint32 *ucs4s;
  char *ucs4_utf8;
  PRUint16 *ucs2s;
  char *ucs2_utf8;
  void *tmp;
  PRBool result;

  ucs4s = (PRUint32 *)calloc(sizeof(ucs4)/sizeof(ucs4[0]), sizeof(PRUint32));
  ucs2s = (PRUint16 *)calloc(sizeof(ucs2)/sizeof(ucs2[0]), sizeof(PRUint16));

  if( ((PRUint32 *)NULL == ucs4s) || ((PRUint16 *)NULL == ucs2s) ) {
    fprintf(stderr, "out of memory\n");
    exit(1);
  }

  len = 0;
  for( i = 0; i < sizeof(ucs4)/sizeof(ucs4[0]); i++ ) {
    ucs4s[i] = ucs4[i].c;
    len += strlen(ucs4[i].utf8);
  }

  ucs4_utf8 = (char *)malloc(len);

  len = 0;
  for( i = 0; i < sizeof(ucs2)/sizeof(ucs2[0]); i++ ) {
    ucs2s[i] = ucs2[i].c;
    len += strlen(ucs2[i].utf8);
  }

  ucs2_utf8 = (char *)malloc(len);

  if( ((char *)NULL == ucs4_utf8) || ((char *)NULL == ucs2_utf8) ) {
    fprintf(stderr, "out of memory\n");
    exit(1);
  }

  *ucs4_utf8 = '\0';
  for( i = 0; i < sizeof(ucs4)/sizeof(ucs4[0]); i++ ) {
    strcat(ucs4_utf8, ucs4[i].utf8);
  }

  *ucs2_utf8 = '\0';
  for( i = 0; i < sizeof(ucs2)/sizeof(ucs2[0]); i++ ) {
    strcat(ucs2_utf8, ucs2[i].utf8);
  }

  /* UTF-8 -> UCS-4 */
  len = sizeof(ucs4)/sizeof(ucs4[0]) * sizeof(PRUint32);
  tmp = calloc(len, 1);
  if( (void *)NULL == tmp ) {
    fprintf(stderr, "out of memory\n");
    exit(1);
  }

  result = sec_port_ucs4_utf8_conversion_function(PR_TRUE,
    ucs4_utf8, strlen(ucs4_utf8), tmp, len, &lenout);
  if( !result ) {
    fprintf(stdout, "Failed to convert much UTF-8 to UCS-4\n");
    goto done;
  }

  if( lenout != len ) {
    fprintf(stdout, "Unexpected length converting much UTF-8 to UCS-4\n");
    goto loser;
  }

  if( 0 != memcmp(ucs4s, tmp, len) ) {
    fprintf(stdout, "Wrong conversion of much UTF-8 to UCS-4\n");
    goto loser;
  }

  free(tmp); tmp = (void *)NULL;

  /* UCS-4 -> UTF-8 */
  len = strlen(ucs4_utf8);
  tmp = calloc(len, 1);
  if( (void *)NULL == tmp ) {
    fprintf(stderr, "out of memory\n");
    exit(1);
  }

  result = sec_port_ucs4_utf8_conversion_function(PR_FALSE,
    (unsigned char *)ucs4s, sizeof(ucs4)/sizeof(ucs4[0]) * sizeof(PRUint32), 
    tmp, len, &lenout);
  if( !result ) {
    fprintf(stdout, "Failed to convert much UCS-4 to UTF-8\n");
    goto done;
  }

  if( lenout != len ) {
    fprintf(stdout, "Unexpected length converting much UCS-4 to UTF-8\n");
    goto loser;
  }

  if( 0 != strncmp(ucs4_utf8, tmp, len) ) {
    fprintf(stdout, "Wrong conversion of much UCS-4 to UTF-8\n");
    goto loser;
  }

  free(tmp); tmp = (void *)NULL;

  /* UTF-8 -> UCS-2 */
  len = sizeof(ucs2)/sizeof(ucs2[0]) * sizeof(PRUint16);
  tmp = calloc(len, 1);
  if( (void *)NULL == tmp ) {
    fprintf(stderr, "out of memory\n");
    exit(1);
  }

  result = sec_port_ucs2_utf8_conversion_function(PR_TRUE,
    ucs2_utf8, strlen(ucs2_utf8), tmp, len, &lenout);
  if( !result ) {
    fprintf(stdout, "Failed to convert much UTF-8 to UCS-2\n");
    goto done;
  }

  if( lenout != len ) {
    fprintf(stdout, "Unexpected length converting much UTF-8 to UCS-2\n");
    goto loser;
  }

  if( 0 != memcmp(ucs2s, tmp, len) ) {
    fprintf(stdout, "Wrong conversion of much UTF-8 to UCS-2\n");
    goto loser;
  }

  free(tmp); tmp = (void *)NULL;

  /* UCS-2 -> UTF-8 */
  len = strlen(ucs2_utf8);
  tmp = calloc(len, 1);
  if( (void *)NULL == tmp ) {
    fprintf(stderr, "out of memory\n");
    exit(1);
  }

  result = sec_port_ucs2_utf8_conversion_function(PR_FALSE,
    (unsigned char *)ucs2s, sizeof(ucs2)/sizeof(ucs2[0]) * sizeof(PRUint16), 
    tmp, len, &lenout);
  if( !result ) {
    fprintf(stdout, "Failed to convert much UCS-2 to UTF-8\n");
    goto done;
  }

  if( lenout != len ) {
    fprintf(stdout, "Unexpected length converting much UCS-2 to UTF-8\n");
    goto loser;
  }

  if( 0 != strncmp(ucs2_utf8, tmp, len) ) {
    fprintf(stdout, "Wrong conversion of much UCS-2 to UTF-8\n");
    goto loser;
  }

#ifdef UTF16
  /* implement me */
#endif /* UTF16 */

  result = PR_TRUE;
  goto done;

 loser:
  result = PR_FALSE;
 done:
  free(ucs4s);
  free(ucs4_utf8);
  free(ucs2s);
  free(ucs2_utf8);
  if( (void *)NULL != tmp ) free(tmp);
  return result;
}

void
byte_order
(
  void
)
{
  /*
   * The implementation (now) expects the 16- and 32-bit characters
   * to be in network byte order, not host byte order.  Therefore I
   * have to byteswap all those test vectors above.  hton[ls] may be
   * functions, so I have to do this dynamically.  If you want to 
   * use this code to do host byte order conversions, just remove
   * the call in main() to this function.
   */

  int i;

  for( i = 0; i < sizeof(ucs4)/sizeof(ucs4[0]); i++ ) {
    struct ucs4 *e = &ucs4[i];
    e->c = htonl(e->c);
  }

  for( i = 0; i < sizeof(ucs2)/sizeof(ucs2[0]); i++ ) {
    struct ucs2 *e = &ucs2[i];
    e->c = htons(e->c);
  }

#ifdef UTF16
  for( i = 0; i < sizeof(utf16)/sizeof(utf16[0]); i++ ) {
    struct utf16 *e = &utf16[i];
    e->c = htonl(e->c);
    e->w[0] = htons(e->w[0]);
    e->w[1] = htons(e->w[1]);
  }
#endif /* UTF16 */

  return;
}

int
main
(
  int argc,
  char *argv[]
)
{
  byte_order();

  if( test_ucs4_chars() &&
      test_ucs2_chars() &&
#ifdef UTF16
      test_utf16_chars() &&
#endif /* UTF16 */
      test_zeroes() &&
      test_multichars() &&
      PR_TRUE ) {
    fprintf(stderr, "PASS\n");
    return 1;
  } else {
    fprintf(stderr, "FAIL\n");
    return 0;
  }
}

#endif /* TEST_UTF8 */
