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

/*
 * Utility routines to complement the ASN.1 encoding and decoding functions.
 *
 * $Id: secasn1u.c,v 1.2 2001/01/07 08:13:12 nelsonb%netscape.com Exp $
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
