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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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

#include "secder.h"
#include "secerr.h"

static PRUint32
der_indefinite_length(unsigned char *buf, unsigned char *end)
{
    PRUint32 len, ret, dataLen;
    unsigned char tag, lenCode;
    int dataLenLen;

    len = 0;
    while ( 1 ) {
	if ((buf + 2) > end) {
	    return(0);
	}
	
	tag = *buf++;
	lenCode = *buf++;
	len += 2;
	
	if ( ( tag == 0 ) && ( lenCode == 0 ) ) {
	    return(len);
	}
	
	if ( lenCode == 0x80 ) {	/* indefinite length */
	    ret = der_indefinite_length(buf, end); /* recurse to find length */
	    if (ret == 0)
		return 0;
	    len += ret;
	    buf += ret;
	} else {			/* definite length */
	    if (lenCode & 0x80) {
		/* Length of data is in multibyte format */
		dataLenLen = lenCode & 0x7f;
		switch (dataLenLen) {
		  case 1:
		    dataLen = buf[0];
		    break;
		  case 2:
		    dataLen = (buf[0]<<8)|buf[1];
		    break;
		  case 3:
		    dataLen = ((unsigned long)buf[0]<<16)|(buf[1]<<8)|buf[2];
		    break;
		  case 4:
		    dataLen = ((unsigned long)buf[0]<<24)|
			((unsigned long)buf[1]<<16)|(buf[2]<<8)|buf[3];
		    break;
		  default:
		    PORT_SetError(SEC_ERROR_BAD_DER);
		    return SECFailure;
		}
	    } else {
		/* Length of data is in single byte */
		dataLen = lenCode;
		dataLenLen = 0;
	    }

	    /* skip this item */
	    buf = buf + dataLenLen + dataLen;
	    len = len + dataLenLen + dataLen;
	}
    }
}

/*
** Capture the next thing in the buffer.
** Returns the length of the header and the length of the contents.
*/
static SECStatus
der_capture(unsigned char *buf, unsigned char *end,
	    int *header_len_p, PRUint32 *contents_len_p)
{
    unsigned char *bp;
    unsigned char whole_tag;
    PRUint32 contents_len;
    int tag_number;

    if ((buf + 2) > end) {
	*header_len_p = 0;
	*contents_len_p = 0;
	if (buf == end)
	    return SECSuccess;
	return SECFailure;
    }

    bp = buf;

    /* Get tag and verify that it is ok. */
    whole_tag = *bp++;
    tag_number = whole_tag & DER_TAGNUM_MASK;

    /*
     * XXX This code does not (yet) handle the high-tag-number form!
     */
    if (tag_number == DER_HIGH_TAG_NUMBER) {
	PORT_SetError(SEC_ERROR_BAD_DER);
	return SECFailure;
    }

    if ((whole_tag & DER_CLASS_MASK) == DER_UNIVERSAL) {
	/* Check that the universal tag number is one we implement.  */
	switch (tag_number) {
	  case DER_BOOLEAN:
	  case DER_INTEGER:
	  case DER_BIT_STRING:
	  case DER_OCTET_STRING:
	  case DER_NULL:
	  case DER_OBJECT_ID:
	  case DER_SEQUENCE:
	  case DER_SET:
	  case DER_PRINTABLE_STRING:
	  case DER_T61_STRING:
	  case DER_IA5_STRING:
	  case DER_VISIBLE_STRING:
	  case DER_UTC_TIME:
	  case 0:			/* end-of-contents tag */
	    break;
	  default:
	    PORT_SetError(SEC_ERROR_BAD_DER);
	    return SECFailure;
	}
    }

    /*
     * Get first byte of length code (might contain entire length, might not).
     */
    contents_len = *bp++;

    /*
     * If the high bit is set, then the length is in multibyte format,
     * or the thing has an indefinite-length.
     */
    if (contents_len & 0x80) {
	int bytes_of_encoded_len;

	bytes_of_encoded_len = contents_len & 0x7f;
	contents_len = 0;

	switch (bytes_of_encoded_len) {
	  case 4:
	    contents_len |= *bp++;
	    contents_len <<= 8;
	    /* fallthru */
	  case 3:
	    contents_len |= *bp++;
	    contents_len <<= 8;
	    /* fallthru */
	  case 2:
	    contents_len |= *bp++;
	    contents_len <<= 8;
	    /* fallthru */
	  case 1:
	    contents_len |= *bp++;
	    break;

	  case 0:
	    contents_len = der_indefinite_length (bp, end);
	    if (contents_len)
		break;
	    /* fallthru */
	  default:
	    PORT_SetError(SEC_ERROR_BAD_DER);
	    return SECFailure;
	}
    }

    if ((bp + contents_len) > end) {
	/* Ran past end of buffer */
	PORT_SetError(SEC_ERROR_BAD_DER);
	return SECFailure;
    }

    *header_len_p = bp - buf;
    *contents_len_p = contents_len;

    return SECSuccess;
}

SECStatus
DER_Lengths(SECItem *item, int *header_len_p, PRUint32 *contents_len_p)
{
    return(der_capture(item->data, &item->data[item->len], header_len_p,
		       contents_len_p));
}
