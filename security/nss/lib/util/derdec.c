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

#include "secder.h"
#include "secerr.h"

static uint32
der_indefinite_length(unsigned char *buf, unsigned char *end)
{
    uint32 len, ret, dataLen;
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
	    int *header_len_p, uint32 *contents_len_p)
{
    unsigned char *bp;
    unsigned char whole_tag;
    uint32 contents_len;
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

static unsigned char *
der_decode(PRArenaPool *arena, void *dest, DERTemplate *dtemplate,
	   unsigned char *buf, int header_len, uint32 contents_len)
{
    unsigned char *orig_buf, *end;
    unsigned long encode_kind, under_kind;
    PRBool explicit, optional, universal, check_tag;
    SECItem *item;
    SECStatus rv;
    PRBool indefinite_length, explicit_indefinite_length;

    encode_kind = dtemplate->kind;
    explicit = (encode_kind & DER_EXPLICIT) ? PR_TRUE : PR_FALSE;
    optional = (encode_kind & DER_OPTIONAL) ? PR_TRUE : PR_FALSE;
    universal = ((encode_kind & DER_CLASS_MASK) == DER_UNIVERSAL)
		? PR_TRUE : PR_FALSE;

    PORT_Assert (!(explicit && universal));	/* bad templates */

    if (header_len == 0) {
	if (optional || (encode_kind & DER_ANY))
	    return buf;
	PORT_SetError(SEC_ERROR_BAD_DER);
	return NULL;
    }

    if (encode_kind & DER_POINTER) {
	void *place, **placep;
	int offset;

	if (dtemplate->sub != NULL) {
	    dtemplate = dtemplate->sub;
	    under_kind = dtemplate->kind;
	    if (universal) {
		encode_kind = under_kind;
	    }
	    place = PORT_ArenaZAlloc(arena, dtemplate->arg);
	    offset = dtemplate->offset;
	} else {
	    if (universal) {
		under_kind = encode_kind & ~DER_POINTER;
	    } else {
		under_kind = dtemplate->arg;
	    }
	    place = PORT_ArenaZAlloc(arena, sizeof(SECItem));
	    offset = 0;
	}
	if (place == NULL) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    return NULL;		/* Out of memory */
	}
	placep = (void **)dest;
	*placep = place;
	dest = (void *)((char *)place + offset);
    } else if (encode_kind & DER_INLINE) {
	PORT_Assert (dtemplate->sub != NULL);
	dtemplate = dtemplate->sub;
	under_kind = dtemplate->kind;
	if (universal) {
	    encode_kind = under_kind;
	}
	dest = (void *)((char *)dest + dtemplate->offset);
    } else if (universal) {
	under_kind = encode_kind;
    } else {
	under_kind = dtemplate->arg;
    }

    orig_buf = buf;
    end = buf + header_len + contents_len;

    explicit_indefinite_length = PR_FALSE;

    if (explicit) {
	/*
	 * This tag is expected to match exactly.
	 * (The template has all of the bits specified.)
	 */
	if (*buf != (encode_kind & DER_TAG_MASK)) {
	    if (optional)
		return buf;
	    PORT_SetError(SEC_ERROR_BAD_DER);
	    return NULL;
	}
	if ((header_len == 2) && (*(buf + 1) == 0x80))
	    explicit_indefinite_length = PR_TRUE;
	buf += header_len;
	rv = der_capture (buf, end, &header_len, &contents_len);
	if (rv != SECSuccess)
	    return NULL;
	if (header_len == 0) {		/* XXX is this right? */
	    PORT_SetError(SEC_ERROR_BAD_DER);
	    return NULL;
	}
	optional = PR_FALSE;		/* can no longer be optional */
	encode_kind = under_kind;
    }

    check_tag = PR_TRUE;
    if (encode_kind & (DER_DERPTR | DER_ANY | DER_FORCE | DER_SKIP)) {
	PORT_Assert ((encode_kind & DER_ANY) || !optional);
	encode_kind = encode_kind & (~DER_FORCE);
	under_kind = under_kind & (~DER_FORCE);
	check_tag = PR_FALSE;
    }

    if (check_tag) {
	PRBool wrong;
	unsigned char expect_tag, expect_num;

	/*
	 * This tag is expected to match, but the simple types
	 * may or may not have the constructed bit set, so we
	 * have to have all this extra logic.
	 */
	wrong = PR_TRUE;
	expect_tag = (unsigned char)encode_kind & DER_TAG_MASK;
	expect_num = expect_tag & DER_TAGNUM_MASK;
	if (expect_num == DER_SET || expect_num == DER_SEQUENCE) {
	    if (*buf == (expect_tag | DER_CONSTRUCTED))
		wrong = PR_FALSE;
	} else {
	    if (*buf == expect_tag)
		wrong = PR_FALSE;
	    else if (*buf == (expect_tag | DER_CONSTRUCTED))
		wrong = PR_FALSE;
	}
	if (wrong) {
	    if (optional)
		return buf;
	    PORT_SetError(SEC_ERROR_BAD_DER);
	    return NULL;
	}
    }

    if (under_kind & DER_DERPTR) {
	item = (SECItem *)dest;
	if (under_kind & DER_OUTER) {
	    item->data = buf;
	    item->len = header_len + contents_len;
	} else {
	    item->data = buf + header_len;
	    item->len = contents_len;
	}
	return orig_buf;
    }

    if (encode_kind & DER_ANY) {
	contents_len += header_len;
	header_len = 0;
    }

    if ((header_len == 2) && (*(buf + 1) == 0x80))
	indefinite_length = PR_TRUE;
    else
	indefinite_length = PR_FALSE;

    buf += header_len;

    if (contents_len == 0)
	return buf;

    under_kind &= ~DER_OPTIONAL;

    if (under_kind & DER_INDEFINITE) {
	int count, thing_size;
	unsigned char *sub_buf;
	DERTemplate *tmpt;
	void *things, **indp, ***placep;

	under_kind &= ~DER_INDEFINITE;

	/*
	 * Count items.
	 */
	count = 0;
	sub_buf = buf;
	while (sub_buf < end) {
	    if (indefinite_length && sub_buf[0] == 0 && sub_buf[1] == 0) {
		break; 
	    }
	    rv = der_capture (sub_buf, end, &header_len, &contents_len);
	    if (rv != SECSuccess)
		return NULL;
	    count++;
	    sub_buf += header_len + contents_len;
	}

	/*
	 * Allocate an array of pointers to items; extra one is for a NULL.
	 */
	indp = (void**)PORT_ArenaZAlloc(arena, (count + 1) * sizeof(void *));
	if (indp == NULL) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    return NULL;
	}

	/*
	 * Prepare.
	 */
	if (under_kind == DER_SET || under_kind == DER_SEQUENCE) {
	    tmpt = dtemplate->sub;
	    PORT_Assert (tmpt != NULL);
	    thing_size = tmpt->arg;
	    PORT_Assert (thing_size != 0);
	} else {
	    tmpt = NULL;
	    thing_size = sizeof(SECItem);
	}

	/*
	 * Allocate the items themselves.
	 */
	things = PORT_ArenaZAlloc(arena, count * thing_size);
	if (things == NULL) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    return NULL;
	}

	placep = (void ***)dest;
	*placep = indp;

	while (count) {
	    /* ignore return value because we already did whole thing above */
	    (void) der_capture (buf, end, &header_len, &contents_len);
	    if (tmpt != NULL) {
		void *sub_thing;

		sub_thing = (void *)((char *)things + tmpt->offset);
		buf = der_decode (arena, sub_thing, tmpt,
				  buf, header_len, contents_len);
		if (buf == NULL)
		    return NULL;
	    } else {
		item = (SECItem *)things;
		if (under_kind == DER_ANY) {
		    contents_len += header_len;
		    header_len = 0;
		}
		buf += header_len;
		if (under_kind == DER_BIT_STRING) {
		    item->data = buf + 1;
		    item->len = ((contents_len - 1) << 3) - *buf;
		} else {
		    item->data = buf;
		    item->len = contents_len;
		}
		buf += contents_len;
	    }
	    *indp++ = things;
	    things = (void *)((char *)things + thing_size);
	    count--;
	}

	*indp = NULL;

	goto der_decode_done;
    }

    switch (under_kind) {
      case DER_SEQUENCE:
      case DER_SET:
	{
	    DERTemplate *tmpt;
	    void *sub_dest;

	    for (tmpt = dtemplate + 1; tmpt->kind; tmpt++) {
		sub_dest = (void *)((char *)dest + tmpt->offset);
		rv = der_capture (buf, end, &header_len, &contents_len);
		if (rv != SECSuccess)
		    return NULL;
		buf = der_decode (arena, sub_dest, tmpt,
				  buf, header_len, contents_len);
		if (buf == NULL)
		    return NULL;
	    }
	}
	break;

      case DER_BIT_STRING:
	item = (SECItem *)dest;
	item->data = buf + 1;
	item->len = ((contents_len - 1) << 3) - *buf;
	buf += contents_len;
	break;

      case DER_SKIP:
	buf += contents_len;
	break;

      default:
	item = (SECItem *)dest;
	item->data = buf;
	item->len = contents_len;
	buf += contents_len;
	break;
    }

der_decode_done:

    if (indefinite_length && buf[0] == 0 && buf[1] == 0) {
	buf += 2;
    }

    if (explicit_indefinite_length && buf[0] == 0 && buf[1] == 0) {
	buf += 2;
    }

    return buf;
}

SECStatus
DER_Decode(PRArenaPool *arena, void *dest, DERTemplate *dtemplate, SECItem *src)
{
    unsigned char *buf;
    uint32 buf_len, contents_len;
    int header_len;
    SECStatus rv;

    buf = src->data;
    buf_len = src->len;

    rv = der_capture (buf, buf + buf_len, &header_len, &contents_len);
    if (rv != SECSuccess)
	return rv;

    dest = (void *)((char *)dest + dtemplate->offset);
    buf = der_decode (arena, dest, dtemplate, buf, header_len, contents_len);
    if (buf == NULL)
	return SECFailure;

    return SECSuccess;
}

SECStatus
DER_Lengths(SECItem *item, int *header_len_p, uint32 *contents_len_p)
{
    return(der_capture(item->data, &item->data[item->len], header_len_p,
		       contents_len_p));
}
