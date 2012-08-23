/*
 * Copyright (c) 2000,2001,2002 Japan Network Information Center.
 * All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "nsIDNKitInterface.h"


#define RACE_2OCTET_MODE	0xd8
#define RACE_ESCAPE		0xff
#define RACE_ESCAPE_2ND		0x99

/*
 * Compression type.
 */
enum {
	compress_one,	/* all characters are in a single row */
	compress_two,	/* row 0 and another row */
	compress_none	/* nope */
};


idn_result_t
race_decode_decompress(const char *from, uint16_t *buf, size_t buflen)
{
	uint16_t *p = buf;
	unsigned int bitbuf = 0;
	int bitlen = 0;
	unsigned int i, j;
	size_t len;

	while (*from != '\0') {
		int c = *from++;
		int x;

		if ('a' <= c && c <= 'z')
			x = c - 'a';
		else if ('A' <= c && c <= 'Z')
			x = c - 'A';
		else if ('2' <= c && c <= '7')
			x = c - '2' + 26;
		else
			return (idn_invalid_encoding);

		bitbuf = (bitbuf << 5) + x;
		bitlen += 5;
		if (bitlen >= 8) {
			*p++ = (bitbuf >> (bitlen - 8)) & 0xff;
			bitlen -= 8;
		}
	}
	len = p - buf;

	/*
	 * Now 'buf' holds the decoded string.
	 */

	/*
	 * Decompress.
	 */
	if (buf[0] == RACE_2OCTET_MODE) {
		if ((len - 1) % 2 != 0)
			return (idn_invalid_encoding);
		for (i = 1, j = 0; i < len; i += 2, j++)
			buf[j] = (buf[i] << 8) + buf[i + 1];
		len = j;
	} else {
		uint16_t c = buf[0] << 8;	/* higher octet */

		for (i = 1, j = 0; i < len; j++) {
			if (buf[i] == RACE_ESCAPE) {
				if (i + 1 >= len)
					return (idn_invalid_encoding);
				else if (buf[i + 1] == RACE_ESCAPE_2ND)
					buf[j] = c | 0xff;
				else
					buf[j] = buf[i + 1];
				i += 2;

			} else if (buf[i] == 0x99 && c == 0x00) {
				/*
				 * The RACE specification says this is error.
				 */
				return (idn_invalid_encoding);
				 
			} else {
				buf[j] = c | buf[i++];
			}
		}
		len = j;
	}
	buf[len] = '\0';

	return (idn_success);
}

idn_result_t
race_compress_encode(const uint16_t *p, int compress_mode,
		     char *to, size_t tolen)
{
	uint32_t bitbuf = *p++;	/* bit stream buffer */
	int bitlen = 8;			/* # of bits in 'bitbuf' */

	while (*p != '\0' || bitlen > 0) {
		unsigned int c = *p;

		if (c == '\0') {
			/* End of data.  Flush. */
			bitbuf <<= (5 - bitlen);
			bitlen = 5;
		} else if (compress_mode == compress_none) {
			/* Push 16 bit data. */
			bitbuf = (bitbuf << 16) | c;
			bitlen += 16;
			p++;
		} else {/* compress_mode == compress_one/compress_two */
			/* Push 8 or 16 bit data. */
			if (compress_mode == compress_two &&
			    (c & 0xff00) == 0) {
				/* Upper octet is zero (and not U1). */
				bitbuf = (bitbuf << 16) | 0xff00 | c;
				bitlen += 16;
			} else if ((c & 0xff) == 0xff) {
				/* Lower octet is 0xff. */
				bitbuf = (bitbuf << 16) |
					(RACE_ESCAPE << 8) | RACE_ESCAPE_2ND;
				bitlen += 16;
			} else {
				/* Just output lower octet. */
				bitbuf = (bitbuf << 8) | (c & 0xff);
				bitlen += 8;
			}
			p++;
		}

		/*
		 * Output bits in 'bitbuf' in 5-bit unit.
		 */
		while (bitlen >= 5) {
			int x;

			/* Get top 5 bits. */
			x = (bitbuf >> (bitlen - 5)) & 0x1f;
			bitlen -= 5;

			/* Encode. */
			if (x < 26)
				x += 'a';
			else
				x = (x - 26) + '2';

			if (tolen < 1)
				return (idn_buffer_overflow);

			*to++ = x;
			tolen--;
		}
	}

	if (tolen <= 0)
		return (idn_buffer_overflow);

	*to = '\0';
	return (idn_success);
}

int
get_compress_mode(uint16_t *p) {
	int zero = 0;
	unsigned int upper = 0;
	uint16_t *modepos = p - 1;

	while (*p != '\0') {
		unsigned int hi = *p++ & 0xff00;

		if (hi == 0) {
			zero++;
		} else if (hi == upper) {
			;
		} else if (upper == 0) {
			upper = hi;
		} else {
			*modepos = RACE_2OCTET_MODE;
			return (compress_none);
		}
	}
	*modepos = upper >> 8;
	if (upper > 0 && zero > 0)
		return (compress_two);
	else
		return (compress_one);
}
