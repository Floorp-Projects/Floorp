/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Utility routines to complement the ASN.1 encoding and decoding functions.
 *
 * $Id: secasn1u.c,v 1.6 2012/04/25 14:50:16 gerv%gerv.net Exp $
 */

#include "secasn1.h"


/*
 * We have a length that needs to be encoded; how many bytes will the
 * encoding take?
 *
 * The rules are that 0 - 0x7f takes one byte (the length itself is the
 * entire encoding); everything else takes one plus the number of bytes
 * in the length.
 */
int
SEC_ASN1LengthLength (unsigned long len)
{
    int lenlen = 1;

    if (len > 0x7f) {
	do {
	    lenlen++;
	    len >>= 8;
	} while (len);
    }

    return lenlen;
}


/*
 * XXX Move over (and rewrite as appropriate) the rest of the
 * stuff in dersubr.c!
 */


/*
 * Find the appropriate subtemplate for the given template.
 * This may involve calling a "chooser" function, or it may just
 * be right there.  In either case, it is expected to *have* a
 * subtemplate; this is asserted in debug builds (in non-debug
 * builds, NULL will be returned).
 *
 * "thing" is a pointer to the structure being encoded/decoded
 * "encoding", when true, means that we are in the process of encoding
 *	(as opposed to in the process of decoding)
 */
const SEC_ASN1Template *
SEC_ASN1GetSubtemplate (const SEC_ASN1Template *theTemplate, void *thing,
			PRBool encoding)
{
    const SEC_ASN1Template *subt = NULL;

    PORT_Assert (theTemplate->sub != NULL);
    if (theTemplate->sub != NULL) {
	if (theTemplate->kind & SEC_ASN1_DYNAMIC) {
	    SEC_ASN1TemplateChooserPtr chooserp;

	    chooserp = *(SEC_ASN1TemplateChooserPtr *) theTemplate->sub;
	    if (chooserp) {
		if (thing != NULL)
		    thing = (char *)thing - theTemplate->offset;
		subt = (* chooserp)(thing, encoding);
	    }
	} else {
	    subt = (SEC_ASN1Template*)theTemplate->sub;
	}
    }
    return subt;
}

PRBool SEC_ASN1IsTemplateSimple(const SEC_ASN1Template *theTemplate)
{
    if (!theTemplate) {
	return PR_TRUE; /* it doesn't get any simpler than NULL */
    }
    /* only templates made of one primitive type or a choice of primitive
       types are considered simple */
    if (! (theTemplate->kind & (~SEC_ASN1_TAGNUM_MASK))) {
	return PR_TRUE; /* primitive type */
    }
    if (!(theTemplate->kind & SEC_ASN1_CHOICE)) {
	return PR_FALSE; /* no choice means not simple */
    }
    while (++theTemplate && theTemplate->kind) {
	if (theTemplate->kind & (~SEC_ASN1_TAGNUM_MASK)) {
	    return PR_FALSE; /* complex type */
	}
    }
    return PR_TRUE; /* choice of primitive types */
}

