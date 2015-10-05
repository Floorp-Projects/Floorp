/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secder.h"
#include "secerr.h"

#if 0
/*
 * Generic templates for individual/simple items.
 */

DERTemplate SECAnyTemplate[] = {
    { DER_ANY,
	  0, NULL, sizeof(SECItem) }
};

DERTemplate SECBitStringTemplate[] = {
    { DER_BIT_STRING,
	  0, NULL, sizeof(SECItem) }
};

DERTemplate SECBooleanTemplate[] = {
    { DER_BOOLEAN,
	  0, NULL, sizeof(SECItem) }
};

DERTemplate SECIA5StringTemplate[] = {
    { DER_IA5_STRING,
	  0, NULL, sizeof(SECItem) }
};

DERTemplate SECIntegerTemplate[] = {
    { DER_INTEGER,
	  0, NULL, sizeof(SECItem) }
};

DERTemplate SECNullTemplate[] = {
    { DER_NULL,
	  0, NULL, sizeof(SECItem) }
};

DERTemplate SECObjectIDTemplate[] = {
    { DER_OBJECT_ID,
	  0, NULL, sizeof(SECItem) }
};

DERTemplate SECOctetStringTemplate[] = {
    { DER_OCTET_STRING,
	  0, NULL, sizeof(SECItem) }
};

DERTemplate SECPrintableStringTemplate[] = {
    { DER_PRINTABLE_STRING,
	  0, NULL, sizeof(SECItem) }
};

DERTemplate SECT61StringTemplate[] = {
    { DER_T61_STRING,
	  0, NULL, sizeof(SECItem) }
};

DERTemplate SECUTCTimeTemplate[] = {
    { DER_UTC_TIME,
	  0, NULL, sizeof(SECItem) }
};

#endif

static int
header_length(DERTemplate *dtemplate, PRUint32 contents_len)
{
    PRUint32 len;
    unsigned long encode_kind, under_kind;
    PRBool explicit, optional, universal;

    encode_kind = dtemplate->kind;

    explicit = (encode_kind & DER_EXPLICIT) ? PR_TRUE : PR_FALSE;
    optional = (encode_kind & DER_OPTIONAL) ? PR_TRUE : PR_FALSE;
    universal = ((encode_kind & DER_CLASS_MASK) == DER_UNIVERSAL)
		? PR_TRUE : PR_FALSE;

    PORT_Assert (!(explicit && universal));	/* bad templates */

    if (encode_kind & DER_POINTER) {
	if (dtemplate->sub != NULL) {
	    under_kind = dtemplate->sub->kind;
	    if (universal) {
		encode_kind = under_kind;
	    }
	} else if (universal) {
	    under_kind = encode_kind & ~DER_POINTER;
	} else {
	    under_kind = dtemplate->arg;
	}
    } else if (encode_kind & DER_INLINE) {
	PORT_Assert (dtemplate->sub != NULL);
	under_kind = dtemplate->sub->kind;
	if (universal) {
	    encode_kind = under_kind;
	}
    } else if (universal) {
	under_kind = encode_kind;
    } else {
	under_kind = dtemplate->arg;
    }

    /* This is only used in decoding; it plays no part in encoding.  */
    if (under_kind & DER_DERPTR)
	return 0;

    /* No header at all for an "empty" optional.  */
    if ((contents_len == 0) && optional)
	return 0;

    /* And no header for a full DER_ANY.  */
    if (encode_kind & DER_ANY)
	return 0;

    /*
     * The common case: one octet for identifier and as many octets
     * as necessary to hold the content length.
     */
    len = 1 + DER_LengthLength(contents_len);

    /* Account for the explicit wrapper, if necessary.  */
    if (explicit) {
#if 0		/*
		 * Well, I was trying to do something useful, but these
		 * assertions are too restrictive on valid templates.
		 * I wanted to make sure that the top-level "kind" of
		 * a template does not also specify DER_EXPLICIT, which
		 * should only modify a component field.  Maybe later
		 * I can figure out a better way to detect such a problem,
		 * but for now I must remove these checks altogether.
		 */
	/*
	 * This modifier applies only to components of a set or sequence;
	 * it should never be used on a set/sequence itself -- confirm.
	 */
	PORT_Assert (under_kind != DER_SEQUENCE);
	PORT_Assert (under_kind != DER_SET);
#endif

	len += 1 + DER_LengthLength(len + contents_len);
    }

    return len;
}


static PRUint32
contents_length(DERTemplate *dtemplate, void *src)
{
    PRUint32 len;
    unsigned long encode_kind, under_kind;
    PRBool universal;


    PORT_Assert (src != NULL);

    encode_kind = dtemplate->kind;

    universal = ((encode_kind & DER_CLASS_MASK) == DER_UNIVERSAL)
		? PR_TRUE : PR_FALSE;
    encode_kind &= ~DER_OPTIONAL;

    if (encode_kind & DER_POINTER) {
	src = *(void **)src;
	if (src == NULL) {
	    return 0;
	}
	if (dtemplate->sub != NULL) {
	    dtemplate = dtemplate->sub;
	    under_kind = dtemplate->kind;
	    src = (void *)((char *)src + dtemplate->offset);
	} else if (universal) {
	    under_kind = encode_kind & ~DER_POINTER;
	} else {
	    under_kind = dtemplate->arg;
	}
    } else if (encode_kind & DER_INLINE) {
	PORT_Assert (dtemplate->sub != NULL);
	dtemplate = dtemplate->sub;
	under_kind = dtemplate->kind;
	src = (void *)((char *)src + dtemplate->offset);
    } else if (universal) {
	under_kind = encode_kind;
    } else {
	under_kind = dtemplate->arg;
    }

    /* Having any of these bits is not expected here...  */
    PORT_Assert ((under_kind & (DER_EXPLICIT | DER_INLINE | DER_OPTIONAL
				| DER_POINTER | DER_SKIP)) == 0);

    /* This is only used in decoding; it plays no part in encoding.  */
    if (under_kind & DER_DERPTR)
	return 0;

    if (under_kind & DER_INDEFINITE) {
	PRUint32 sub_len;
	void   **indp = *(void ***)src;

	if (indp == NULL)
	    return 0;

	len = 0;
	under_kind &= ~DER_INDEFINITE;

	if (under_kind == DER_SET || under_kind == DER_SEQUENCE) {
	    DERTemplate *tmpt = dtemplate->sub;
	    PORT_Assert (tmpt != NULL);

	    for (; *indp != NULL; indp++) {
		void *sub_src = (void *)((char *)(*indp) + tmpt->offset);
		sub_len = contents_length (tmpt, sub_src);
		len += sub_len + header_length (tmpt, sub_len);
	    }
	} else {
	    /*
	     * XXX Lisa is not sure this code (for handling, for example,
	     * DER_INDEFINITE | DER_OCTET_STRING) is right.
	     */
	    for (; *indp != NULL; indp++) {
		SECItem *item = (SECItem *)(*indp);
		sub_len = item->len;
		if (under_kind == DER_BIT_STRING) {
		    sub_len = (sub_len + 7) >> 3;
		    /* bit string contents involve an extra octet */
		    if (sub_len)
			sub_len++;
		}
		if (under_kind != DER_ANY)
		    len += 1 + DER_LengthLength (sub_len);
	    }
	}

	return len;
    }

    switch (under_kind) {
      case DER_SEQUENCE:
      case DER_SET:
	{
	    DERTemplate *tmpt;
	    void *sub_src;
	    PRUint32 sub_len;

	    len = 0;
	    for (tmpt = dtemplate + 1; tmpt->kind; tmpt++) {
		sub_src = (void *)((char *)src + tmpt->offset);
		sub_len = contents_length (tmpt, sub_src);
		len += sub_len + header_length (tmpt, sub_len);
	    }
	}
	break;

      case DER_BIT_STRING:
	len = (((SECItem *)src)->len + 7) >> 3;
	/* bit string contents involve an extra octet */
	if (len)
	    len++;
	break;

      default:
	len = ((SECItem *)src)->len;
	break;
    }

    return len;
}


static unsigned char *
der_encode(unsigned char *buf, DERTemplate *dtemplate, void *src)
{
    int header_len;
    PRUint32 contents_len;
    unsigned long encode_kind, under_kind;
    PRBool explicit, optional, universal;


    /*
     * First figure out how long the encoding will be.  Do this by
     * traversing the template from top to bottom and accumulating
     * the length of each leaf item.
     */
    contents_len = contents_length (dtemplate, src);
    header_len = header_length (dtemplate, contents_len);

    /*
     * Enough smarts was involved already, so that if both the
     * header and the contents have a length of zero, then we
     * are not doing any encoding for this element.
     */
    if (header_len == 0 && contents_len == 0)
	return buf;

    encode_kind = dtemplate->kind;

    explicit = (encode_kind & DER_EXPLICIT) ? PR_TRUE : PR_FALSE;
    optional = (encode_kind & DER_OPTIONAL) ? PR_TRUE : PR_FALSE;
    encode_kind &= ~DER_OPTIONAL;
    universal = ((encode_kind & DER_CLASS_MASK) == DER_UNIVERSAL)
		? PR_TRUE : PR_FALSE;

    if (encode_kind & DER_POINTER) {
	if (contents_len) {
	    src = *(void **)src;
	    PORT_Assert (src != NULL);
	}
	if (dtemplate->sub != NULL) {
	    dtemplate = dtemplate->sub;
	    under_kind = dtemplate->kind;
	    if (universal) {
		encode_kind = under_kind;
	    }
	    src = (void *)((char *)src + dtemplate->offset);
	} else if (universal) {
	    under_kind = encode_kind & ~DER_POINTER;
	} else {
	    under_kind = dtemplate->arg;
	}
    } else if (encode_kind & DER_INLINE) {
	dtemplate = dtemplate->sub;
	under_kind = dtemplate->kind;
	if (universal) {
	    encode_kind = under_kind;
	}
	src = (void *)((char *)src + dtemplate->offset);
    } else if (universal) {
	under_kind = encode_kind;
    } else {
	under_kind = dtemplate->arg;
    }

    if (explicit) {
	buf = DER_StoreHeader (buf, encode_kind,
			       (1 + DER_LengthLength(contents_len)
				+ contents_len));
	encode_kind = under_kind;
    }

    if ((encode_kind & DER_ANY) == 0) {	/* DER_ANY already contains header */
	buf = DER_StoreHeader (buf, encode_kind, contents_len);
    }

    /* If no real contents to encode, then we are done.  */
    if (contents_len == 0)
	return buf;

    if (under_kind & DER_INDEFINITE) {
	void **indp;

	indp = *(void ***)src;
	PORT_Assert (indp != NULL);

	under_kind &= ~DER_INDEFINITE;
	if (under_kind == DER_SET || under_kind == DER_SEQUENCE) {
	    DERTemplate *tmpt = dtemplate->sub;
	    PORT_Assert (tmpt != NULL);
	    for (; *indp != NULL; indp++) {
		void *sub_src = (void *)((char *)(*indp) + tmpt->offset);
		buf = der_encode (buf, tmpt, sub_src);
	    }
	} else {
	    for (; *indp != NULL; indp++) {
		SECItem *item;
		int sub_len;

		item = (SECItem *)(*indp);
		sub_len = item->len;
		if (under_kind == DER_BIT_STRING) {
		    if (sub_len) {
			int rem;

			sub_len = (sub_len + 7) >> 3;
			buf = DER_StoreHeader (buf, under_kind, sub_len + 1);
			rem = (sub_len << 3) - item->len;
			*buf++ = rem;		/* remaining bits */
		    } else {
			buf = DER_StoreHeader (buf, under_kind, 0);
		    }
		} else if (under_kind != DER_ANY) {
		    buf = DER_StoreHeader (buf, under_kind, sub_len);
		}
		PORT_Memcpy (buf, item->data, sub_len);
		buf += sub_len;
	    }
	}
	return buf;
    }

    switch (under_kind) {
      case DER_SEQUENCE:
      case DER_SET:
	{
	    DERTemplate *tmpt;
	    void *sub_src;

	    for (tmpt = dtemplate + 1; tmpt->kind; tmpt++) {
		sub_src = (void *)((char *)src + tmpt->offset);
		buf = der_encode (buf, tmpt, sub_src);
	    }
	}
	break;

      case DER_BIT_STRING:
	{
	    SECItem *item;
	    int rem;

	    /*
	     * The contents length includes our extra octet; subtract
	     * it off so we just have the real string length there.
	     */
	    contents_len--;
	    item = (SECItem *)src;
	    PORT_Assert (contents_len == ((item->len + 7) >> 3));
	    rem = (contents_len << 3) - item->len;
	    *buf++ = rem;		/* remaining bits */
	    PORT_Memcpy (buf, item->data, contents_len);
	    buf += contents_len;
	}
	break;

      default:
	{
	    SECItem *item;

	    item = (SECItem *)src;
	    PORT_Assert (contents_len == item->len);
	    PORT_Memcpy (buf, item->data, contents_len);
	    buf += contents_len;
	}
	break;
    }

    return buf;
}


SECStatus
DER_Encode(PLArenaPool *arena, SECItem *dest, DERTemplate *dtemplate, void *src)
{
    unsigned int contents_len, header_len;

    src = (void **)((char *)src + dtemplate->offset);

    /*
     * First figure out how long the encoding will be. Do this by
     * traversing the template from top to bottom and accumulating
     * the length of each leaf item.
     */
    contents_len = contents_length (dtemplate, src);
    header_len = header_length (dtemplate, contents_len);

    dest->len = contents_len + header_len;

    /* Allocate storage to hold the encoding */
    dest->data = (unsigned char*) PORT_ArenaAlloc(arena, dest->len);
    if (dest->data == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }

    /* Now encode into the buffer */
    (void) der_encode (dest->data, dtemplate, src);

    return SECSuccess;
}
