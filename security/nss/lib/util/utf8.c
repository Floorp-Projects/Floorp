/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "seccomon.h"
#include "secport.h"

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

#define BAD_UTF8 ((PRUint32)-1)

/*
 * Parse a single UTF-8 character per the spec. in section 3.9 (D36)
 * of Unicode 4.0.0.
 *
 * Parameters:
 * index - Points to the byte offset in inBuf of character to read.  On success,
 *         updated to the offset of the following character.
 * inBuf - Input buffer, UTF-8 encoded
 * inbufLen - Length of input buffer, in bytes.
 *
 * Returns:
 * Success - The UCS4 encoded character
 * Failure - BAD_UTF8
 */
static PRUint32
sec_port_read_utf8(unsigned int *index, unsigned char *inBuf, unsigned int inBufLen)
{
    PRUint32 result;
    unsigned int i = *index;
    int bytes_left;
    PRUint32 min_value;

    PORT_Assert(i < inBufLen);

    if ((inBuf[i] & 0x80) == 0x00) {
        result = inBuf[i++];
        bytes_left = 0;
        min_value = 0;
    } else if ((inBuf[i] & 0xE0) == 0xC0) {
        result = inBuf[i++] & 0x1F;
        bytes_left = 1;
        min_value = 0x80;
    } else if ((inBuf[i] & 0xF0) == 0xE0) {
        result = inBuf[i++] & 0x0F;
        bytes_left = 2;
        min_value = 0x800;
    } else if ((inBuf[i] & 0xF8) == 0xF0) {
        result = inBuf[i++] & 0x07;
        bytes_left = 3;
        min_value = 0x10000;
    } else {
        return BAD_UTF8;
    }

    while (bytes_left--) {
        if (i >= inBufLen || (inBuf[i] & 0xC0) != 0x80)
            return BAD_UTF8;
        result = (result << 6) | (inBuf[i++] & 0x3F);
    }

    /* Check for overlong sequences, surrogates, and outside unicode range */
    if (result < min_value || (result & 0xFFFFF800) == 0xD800 || result > 0x10FFFF) {
        return BAD_UTF8;
    }

    *index = i;
    return result;
}

PRBool
sec_port_ucs4_utf8_conversion_function(
    PRBool toUnicode,
    unsigned char *inBuf,
    unsigned int inBufLen,
    unsigned char *outBuf,
    unsigned int maxOutBufLen,
    unsigned int *outBufLen)
{
    PORT_Assert((unsigned int *)NULL != outBufLen);

    if (toUnicode) {
        unsigned int i, len = 0;

        for (i = 0; i < inBufLen;) {
            if ((inBuf[i] & 0x80) == 0x00)
                i += 1;
            else if ((inBuf[i] & 0xE0) == 0xC0)
                i += 2;
            else if ((inBuf[i] & 0xF0) == 0xE0)
                i += 3;
            else if ((inBuf[i] & 0xF8) == 0xF0)
                i += 4;
            else
                return PR_FALSE;

            len += 4;
        }

        if (len > maxOutBufLen) {
            *outBufLen = len;
            return PR_FALSE;
        }

        len = 0;

        for (i = 0; i < inBufLen;) {
            PRUint32 ucs4 = sec_port_read_utf8(&i, inBuf, inBufLen);

            if (ucs4 == BAD_UTF8)
                return PR_FALSE;

            outBuf[len + L_0] = 0x00;
            outBuf[len + L_1] = (unsigned char)(ucs4 >> 16);
            outBuf[len + L_2] = (unsigned char)(ucs4 >> 8);
            outBuf[len + L_3] = (unsigned char)ucs4;

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

        for (i = 0; i < inBufLen; i += 4) {
            if ((inBuf[i + L_0] > 0x00) || (inBuf[i + L_1] > 0x10)) {
                *outBufLen = 0;
                return PR_FALSE;
            } else if (inBuf[i + L_1] >= 0x01)
                len += 4;
            else if (inBuf[i + L_2] >= 0x08)
                len += 3;
            else if ((inBuf[i + L_2] > 0x00) || (inBuf[i + L_3] >= 0x80))
                len += 2;
            else
                len += 1;
        }

        if (len > maxOutBufLen) {
            *outBufLen = len;
            return PR_FALSE;
        }

        len = 0;

        for (i = 0; i < inBufLen; i += 4) {
            if (inBuf[i + L_1] >= 0x01) {
                /* 0001 0000-001F FFFF -> 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
                /* 00000000 000abcde fghijklm nopqrstu ->
                   11110abc 10defghi 10jklmno 10pqrstu */

                outBuf[len + 0] = 0xF0 | ((inBuf[i + L_1] & 0x1C) >> 2);
                outBuf[len + 1] = 0x80 | ((inBuf[i + L_1] & 0x03) << 4) | ((inBuf[i + L_2] & 0xF0) >> 4);
                outBuf[len + 2] = 0x80 | ((inBuf[i + L_2] & 0x0F) << 2) | ((inBuf[i + L_3] & 0xC0) >> 6);
                outBuf[len + 3] = 0x80 | ((inBuf[i + L_3] & 0x3F) >> 0);

                len += 4;
            } else if (inBuf[i + L_2] >= 0x08) {
                /* 0000 0800-0000 FFFF -> 1110xxxx 10xxxxxx 10xxxxxx */
                /* 00000000 00000000 abcdefgh ijklmnop ->
                   1110abcd 10efghij 10klmnop */

                outBuf[len + 0] = 0xE0 | ((inBuf[i + L_2] & 0xF0) >> 4);
                outBuf[len + 1] = 0x80 | ((inBuf[i + L_2] & 0x0F) << 2) | ((inBuf[i + L_3] & 0xC0) >> 6);
                outBuf[len + 2] = 0x80 | ((inBuf[i + L_3] & 0x3F) >> 0);

                len += 3;
            } else if ((inBuf[i + L_2] > 0x00) || (inBuf[i + L_3] >= 0x80)) {
                /* 0000 0080-0000 07FF -> 110xxxxx 10xxxxxx */
                /* 00000000 00000000 00000abc defghijk ->
                   110abcde 10fghijk */

                outBuf[len + 0] = 0xC0 | ((inBuf[i + L_2] & 0x07) << 2) | ((inBuf[i + L_3] & 0xC0) >> 6);
                outBuf[len + 1] = 0x80 | ((inBuf[i + L_3] & 0x3F) >> 0);

                len += 2;
            } else {
                /* 0000 0000-0000 007F -> 0xxxxxx */
                /* 00000000 00000000 00000000 0abcdefg ->
                   0abcdefg */

                outBuf[len + 0] = (inBuf[i + L_3] & 0x7F);

                len += 1;
            }
        }

        *outBufLen = len;
        return PR_TRUE;
    }
}

PRBool
sec_port_ucs2_utf8_conversion_function(
    PRBool toUnicode,
    unsigned char *inBuf,
    unsigned int inBufLen,
    unsigned char *outBuf,
    unsigned int maxOutBufLen,
    unsigned int *outBufLen)
{
    PORT_Assert((unsigned int *)NULL != outBufLen);

    if (toUnicode) {
        unsigned int i, len = 0;

        for (i = 0; i < inBufLen;) {
            if ((inBuf[i] & 0x80) == 0x00) {
                i += 1;
                len += 2;
            } else if ((inBuf[i] & 0xE0) == 0xC0) {
                i += 2;
                len += 2;
            } else if ((inBuf[i] & 0xF0) == 0xE0) {
                i += 3;
                len += 2;
            } else if ((inBuf[i] & 0xF8) == 0xF0) {
                i += 4;
                len += 4;
            } else
                return PR_FALSE;
        }

        if (len > maxOutBufLen) {
            *outBufLen = len;
            return PR_FALSE;
        }

        len = 0;

        for (i = 0; i < inBufLen;) {
            PRUint32 ucs4 = sec_port_read_utf8(&i, inBuf, inBufLen);

            if (ucs4 == BAD_UTF8)
                return PR_FALSE;

            if (ucs4 < 0x10000) {
                outBuf[len + H_0] = (unsigned char)(ucs4 >> 8);
                outBuf[len + H_1] = (unsigned char)ucs4;
                len += 2;
            } else {
                ucs4 -= 0x10000;
                outBuf[len + 0 + H_0] = (unsigned char)(0xD8 | ((ucs4 >> 18) & 0x3));
                outBuf[len + 0 + H_1] = (unsigned char)(ucs4 >> 10);
                outBuf[len + 2 + H_0] = (unsigned char)(0xDC | ((ucs4 >> 8) & 0x3));
                outBuf[len + 2 + H_1] = (unsigned char)ucs4;
                len += 4;
            }
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

        for (i = 0; i < inBufLen; i += 2) {
            if ((inBuf[i + H_0] == 0x00) && ((inBuf[i + H_1] & 0x80) == 0x00))
                len += 1;
            else if (inBuf[i + H_0] < 0x08)
                len += 2;
            else if (((inBuf[i + H_0] & 0xFC) == 0xD8)) {
                if (((inBufLen - i) > 2) && ((inBuf[i + 2 + H_0] & 0xFC) == 0xDC)) {
                    i += 2;
                    len += 4;
                } else {
                    return PR_FALSE;
                }
            } else if ((inBuf[i + H_0] & 0xFC) == 0xDC) {
                return PR_FALSE;
            } else {
                len += 3;
            }
        }

        if (len > maxOutBufLen) {
            *outBufLen = len;
            return PR_FALSE;
        }

        len = 0;

        for (i = 0; i < inBufLen; i += 2) {
            if ((inBuf[i + H_0] == 0x00) && ((inBuf[i + H_1] & 0x80) == 0x00)) {
                /* 0000-007F -> 0xxxxxx */
                /* 00000000 0abcdefg -> 0abcdefg */

                outBuf[len] = inBuf[i + H_1] & 0x7F;

                len += 1;
            } else if (inBuf[i + H_0] < 0x08) {
                /* 0080-07FF -> 110xxxxx 10xxxxxx */
                /* 00000abc defghijk -> 110abcde 10fghijk */

                outBuf[len + 0] = 0xC0 | ((inBuf[i + H_0] & 0x07) << 2) | ((inBuf[i + H_1] & 0xC0) >> 6);
                outBuf[len + 1] = 0x80 | ((inBuf[i + H_1] & 0x3F) >> 0);

                len += 2;
            } else if ((inBuf[i + H_0] & 0xFC) == 0xD8) {
                int abcde, BCDE;

                PORT_Assert(((inBufLen - i) > 2) && ((inBuf[i + 2 + H_0] & 0xFC) == 0xDC));

                /* D800-DBFF DC00-DFFF -> 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
                /* 110110BC DEfghijk 110111lm nopqrstu ->
                   { Let abcde = BCDE + 1 }
                   11110abc 10defghi 10jklmno 10pqrstu */

                BCDE = ((inBuf[i + H_0] & 0x03) << 2) | ((inBuf[i + H_1] & 0xC0) >> 6);
                abcde = BCDE + 1;

                outBuf[len + 0] = 0xF0 | ((abcde & 0x1C) >> 2);
                outBuf[len + 1] = 0x80 | ((abcde & 0x03) << 4) | ((inBuf[i + 0 + H_1] & 0x3C) >> 2);
                outBuf[len + 2] = 0x80 | ((inBuf[i + 0 + H_1] & 0x03) << 4) | ((inBuf[i + 2 + H_0] & 0x03) << 2) | ((inBuf[i + 2 + H_1] & 0xC0) >> 6);
                outBuf[len + 3] = 0x80 | ((inBuf[i + 2 + H_1] & 0x3F) >> 0);

                i += 2;
                len += 4;
            } else {
                /* 0800-FFFF -> 1110xxxx 10xxxxxx 10xxxxxx */
                /* abcdefgh ijklmnop -> 1110abcd 10efghij 10klmnop */

                outBuf[len + 0] = 0xE0 | ((inBuf[i + H_0] & 0xF0) >> 4);
                outBuf[len + 1] = 0x80 | ((inBuf[i + H_0] & 0x0F) << 2) | ((inBuf[i + H_1] & 0xC0) >> 6);
                outBuf[len + 2] = 0x80 | ((inBuf[i + H_1] & 0x3F) >> 0);

                len += 3;
            }
        }

        *outBufLen = len;
        return PR_TRUE;
    }
}

PRBool
sec_port_iso88591_utf8_conversion_function(
    const unsigned char *inBuf,
    unsigned int inBufLen,
    unsigned char *outBuf,
    unsigned int maxOutBufLen,
    unsigned int *outBufLen)
{
    unsigned int i, len = 0;

    PORT_Assert((unsigned int *)NULL != outBufLen);

    for (i = 0; i < inBufLen; i++) {
        if ((inBuf[i] & 0x80) == 0x00)
            len += 1;
        else
            len += 2;
    }

    if (len > maxOutBufLen) {
        *outBufLen = len;
        return PR_FALSE;
    }

    len = 0;

    for (i = 0; i < inBufLen; i++) {
        if ((inBuf[i] & 0x80) == 0x00) {
            /* 00-7F -> 0xxxxxxx */
            /* 0abcdefg -> 0abcdefg */

            outBuf[len] = inBuf[i];
            len += 1;
        } else {
            /* 80-FF <- 110xxxxx 10xxxxxx */
            /* 00000000 abcdefgh -> 110000ab 10cdefgh */

            outBuf[len + 0] = 0xC0 | ((inBuf[i] & 0xC0) >> 6);
            outBuf[len + 1] = 0x80 | ((inBuf[i] & 0x3F) >> 0);

            len += 2;
        }
    }

    *outBufLen = len;
    return PR_TRUE;
}
