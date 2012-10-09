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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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

#include "sdp_base64.h"

/*
 * Local definitions for Base64 to Raw table entries.
 */
#define INVALID_CHAR 0xFF /* Character not in supported Base64 set */
#define WHITE_SPACE  0xFE /* Space, tab, newline, etc character */
#define PADDING      0xFD /* The character '=' */

#define PAD_CHAR     '=' /* The character '=' */

/* Maximum length of a base64 encoded line */
#define MAX_BASE64_LINE_LENGTH 76

/*
 * base64_result_table
 *  String table for translating base64 error codes into human
 *  understanable strings.
 */
char *base64_result_table[BASE64_RESULT_MAX] =
{
    "Base64 successful",
    "Base64 Buffer Overrun",
    "Base64 Bad Data",
    "Base64 Bad Padding",
    "Base64 Bad Block Size"
};

/*
 * base64_to_raw_table
 *  Heart of the Base64 decoding algorithm. Lookup table to convert
 *  the Base64 characters into their specified representative values.
 *  Invalid characters are marked with 0xFF, white space characters
 *  are marked with 0xFE, and the special pading character is marked
 *  with 0xFD.
 */
unsigned char base64_to_raw_table[128] =
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, /* 0-9 */
    0xFE, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* 10-19 */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* 20-29 */
    0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* 30-39 */
    0xFF, 0xFF, 0xFF,   62, 0xFF, 0xFF, 0xFF,   63,   52,   53, /* 40-49 */
      54,   55,   56,   57,   58,   59,   60,   61, 0xFF, 0xFF, /* 50-59 */
    0xFF, 0xFD, 0xFF, 0xFF, 0xFF,    0,    1,    2,    3,    4, /* 60-69 */
       5,    6,    7,    8,    9,   10,   11,   12,   13,   14, /* 70-79 */
      15,   16,   17,   18,   19,   20,   21,   22,   23,   24, /* 80-89 */
      25, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   26,   27,   28, /* 90-99 */
      29,   30,   31,   32,   33,   34,   35,   36,   37,   38, /* 100-109 */
      39,   40,   41,   42,   43,   44,   45,   46,   47,   48, /* 110-119 */
      49,   50,   51, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF              /* 120-127 */
};

unsigned char raw_to_base64_table[64] =
{
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', /* 0-9 */
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', /* 10-19 */
    'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', /* 20-29 */
    'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', /* 30-39 */
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', /* 40-49 */
    'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', /* 50-59 */
    '8', '9', '+', '/'				      /* 60-63 */
};

/*
 * base64_encode_size_bytes
 *
 * DESCRIPTION
 *  Estimates the size of buffer required for holding the result of
 *  encoding data of size raw_size_bytes.
 *
 * PARAMETERS
 *  raw_size_bytes = Estimated size of the un-encoded data in bytes.
 *
 * RETURN VALUE
 *  The size of destination buffer to use for encoding in bytes.
 */
int base64_est_encode_size_bytes (int raw_size_bytes)
{
    int length;

    /* 
     * Find the number of bytes needed to represent the data
     * using a 4/3 expansion ratio. That result must be 
     * rounded to the next higher multiple of four to account
     * for padding. Then add in a term to account for any '\n's
     * added.
     */
    length = ((((raw_size_bytes * 4 + 2)/ 3) + 3) & ~(0x3)) + 
	raw_size_bytes / MAX_BASE64_LINE_LENGTH;
    
    return length;
}

/*
 * base64_decode_size_bytes
 *
 * DESCRIPTION
 *  Estimates the size of buffer required for holding the result of
 *  decoding data of size base64_size_bytes.
 *
 * PARAMETERS
 *  base64_size_bytes = Estimated size of the Base64 data in bytes.
 *
 * RETURN VALUE
 *  The size of destination buffer to use for decoding in bytes.
 */
int base64_est_decode_size_bytes (int base64_size_bytes)
{
    int length;

    length = (base64_size_bytes * 3 + 3) / 4; 
    return length;
}

/*
 * base64_encode
 *
 * DESCRIPTION
 *  Encode data pointed to by src into the buffer pointer to by dest
 *  using the Base64 algorithm.
 *
 *  NOTE: No trailing '\n' character will be added.
 *
 *  NOTE: As per specification, '\n' will be placed every 76 chars.
 *
 * PARAMETERS
 *  src = Pointer to the raw data to base64 encode.
 *  src_bytes = The number of bytes in the src buffer to encode.
 *  dest = Pointer to the destination buffer where the converted data
 *	will reside when complete.
 *  dest_bytes = Initially holds the size of the destination buffer
 *	but at completion holds the number of bytes converted.
 *
 * RETURN VALUE
 *  base64_success if the buffer was successfully converted, the 
 *  appropriate error code otherwise. 
 *
 *  The dest parameter holds the converted data.
 *
 *  The dest_bytes parameter holds the actual number of bytes converted.
 */
base64_result_t base64_encode(unsigned char *src, int src_bytes, unsigned char *dest, int *dest_bytes)
{
    int i, j=0;
    int line_count = 0;
    unsigned char index; /* index into base64 lookup table */
    int smax = src_bytes-2; /* only do full multiples of 3 */
    int dmax = *dest_bytes; /* destination maximum */

    *dest_bytes = 0;

    /* Do full groups. Base64 must be done in blocks of 3 src bytes */
    for (i=0; i<smax; i+=3) {
	/* Check to see if newline should be injected */
	if (line_count>=MAX_BASE64_LINE_LENGTH) {
	    if (j<dmax){
		dest[j++] = '\n';
	    } else {
		return BASE64_BUFFER_OVERRUN;
	    }
	    line_count = 0;
	}

	line_count += 4;

	if ((j+3) < dmax) {
	    
	    /* Find mapping of upper 6 bits */
	    index = (src[i] >> 2) & 0x3F;
	    dest[j++] = raw_to_base64_table[index];

	    /* bottom 2 bits of first word, high 4 bits of second word */
	    index = ((src[i] << 4) & 0x30) | ((src[i+1] >> 4) & 0x0F);
	    dest[j++] = raw_to_base64_table[index];

	    /* bottom 4 bits of second word, high 2 bits of third word */
	    index = ((src[i+1] << 2) & 0x3C) | ((src[i+2] >> 6) & 0x03);
	    dest[j++] = raw_to_base64_table[index];

	    /* bottom 6 bits of third word */
	    index = src[i+2] & 0x3F;
	    dest[j++] = raw_to_base64_table[index];
	} else {
	    return BASE64_BUFFER_OVERRUN;
	}
    }

    /* Check to see if any more work must be done */
    if (i<src_bytes) {
	
	/* Check to see if a newline should be output */
	if (line_count>=MAX_BASE64_LINE_LENGTH) {
	    if (j<dmax){
		dest[j++] = '\n';
	    } else {
		return BASE64_BUFFER_OVERRUN;
	    }
	    line_count = 0;
	}

	line_count += 4;

	/* Must fill another quantum */
	if (j+4>dmax) {
	    /* No room left in output buffer! */
	    return BASE64_BUFFER_OVERRUN;
	}

	/* Find mapping of upper 6 bits */
	index = (src[i] >> 2) & 0x3F;
	dest[j++] = raw_to_base64_table[index];

	/* check for another stragler */
	if ((i+1)<src_bytes) {
	    /* bottom 2 bits of first word, high 4 bits of second word */
	    index = ((src[i] << 4) & 0x30) | ((src[i+1] >> 4) & 0x0F);
	    dest[j++] = raw_to_base64_table[index];

	    /* bottom 4 bits of second word */
	    index = (src[i+1] << 2) & 0x3C;
	    dest[j++] = raw_to_base64_table[index];
	    dest[j++] = PAD_CHAR;
	} else {
	    /* bottom 2 bits of first word */
	    index = (src[i] << 4) & 0x30;
	    dest[j++] = raw_to_base64_table[index];
	    dest[j++] = PAD_CHAR;
	    dest[j++] = PAD_CHAR;
	}
    }

    *dest_bytes = j;

    return BASE64_SUCCESS;
}

/*
 * base64_decode
 *
 * DESCRIPTION
 *  Decode data pointed to by src into the buffer pointer to by dest
 *  using the Base64 algorithm.
 *
 * PARAMETERS
 *  src = Pointer to the Base64 data to decode.
 *  src_bytes = The number of bytes in the src buffer to decode.
 *  dest = Pointer to the destination buffer where the converted data
 *	will reside when complete.
 *  dest_bytes = Initially holds the size of the destination buffer
 *	but at completion holds the number of bytes converted.
 *
 * RETURN VALUE
 *  base64_success if the buffer was successfully converted, the 
 *  appropriate error code otherwise. 
 *
 *  The dest parameter holds the converted data.
 *
 *  The dest_bytes parameter holds the actual number of bytes converted.
 */
base64_result_t base64_decode(unsigned char *src, int src_bytes, unsigned char *dest, int *dest_bytes)
{
    int i, j = 0;
    int sindex = 0;			/* Current NON-whitespace source 
					 * index */
    int pad_count=0;			/* Number of padding characters 
					 * encountered */
    int dest_size_bytes = *dest_bytes;	/* Save size of destination buffer */
    unsigned char cindex;		/* The current Base64 character to
					 * process */
    unsigned char val;			/* The value of the current Base64 
					 * character */

    *dest_bytes = 0;

    for (i=0; i<src_bytes; i++) {
	cindex = src[i];

	if ((cindex & 0x80) || /* only have 128 values, MSB must not be set! */
	    ((val = base64_to_raw_table[cindex]) == INVALID_CHAR)) {
	    /* Invalid base64 character */
	    return BASE64_BAD_DATA;
	}

	if (val == WHITE_SPACE) {
	    /* Ignore white space */
	    continue;
	}

	if (val == PADDING) {
	    /* we must be at the end-finish up */
	    pad_count++;
	    if (++i<src_bytes) {
		/* can have up to 2 pad chars */
		if (base64_to_raw_table[src[i]] != PADDING) {
		    return BASE64_BAD_PADDING;
		}

		if (++i<src_bytes) {
		    /* should not have any more padding! */
		    return BASE64_BAD_PADDING;
		}

		pad_count++;
	    }

	    /* DONE! */
	    break;
	}

	/* Determine which portion of the 3 bytes this data will fill */
	switch (sindex & 0x3) {
	case 0:
	    /* Fill upper 6 bits */
	    if (j<dest_size_bytes) {
		dest[j] = val << 2;
	    } else {
		return BASE64_BUFFER_OVERRUN;
	    }
	    break;
	case 1:
	    /* Fill Bottom 2 bits */
	    dest[j++] |= val >> 4;

	    if (j<dest_size_bytes) {
		/* Fill Top 4 bits */
		dest[j] = (val << 4) & 0xF0;
	    } else {
		/* 
		 * Check to see if there is any more data present.
		 * Next base64 character MUST be a pad character and 
		 * the rest of this data MUST be zero.
		 *
		 * If this is not the end of data then a buffer overrun
		 * has occurred
		 */
		if ((val & 0x0F) ||
		    (i+1>=src_bytes) ||
		    (base64_to_raw_table[src[i+1]] != PADDING)) {
		    return BASE64_BUFFER_OVERRUN;
		}
	    }
	    break;
	case 2:
	    /* Fill Bottom 4 bits */
	    dest[j++] |= val >> 2;

	    if (j<dest_size_bytes) {
		/* Fill Top 2 bits */
		dest[j] = (val << 6) & 0xC0;
	    } else {
		/* 
		 * Check to see if there is any more data present.
		 * Next base64 character MUST be a pad character and 
		 * the rest of this data MUST be zero.
		 *
		 * If this is not the end of data then a buffer overrun
		 * has occurred
		 */
		if ((val & 0x03) ||
		    (i+1>=src_bytes) ||
		    (base64_to_raw_table[src[i+1]] != PADDING)) {
		    return BASE64_BUFFER_OVERRUN;
		}
	    }
	    break;
	case 3:
	    /*
	     *  No need to check for overrun here since the
	     *  previous case was already checked. If another
	     *  group is present then case 0 will check again.
	     */

	    /* Fill Bottom 6 bits */
	    dest[j++] |= val;
	    break;
	}
	sindex++;
    }

    /* Check length for multiple of 3 bytes */
    if (((j + pad_count)% 3) != 0) {
	return BASE64_BAD_BLOCK_SIZE;
    }

    /* Save off the number of bytes converted */
    *dest_bytes = j;

    return BASE64_SUCCESS;
}
