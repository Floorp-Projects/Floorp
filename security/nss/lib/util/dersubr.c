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
#include <limits.h>
#include "secerr.h"

int
DER_LengthLength(uint32 len)
{
    if (len > 127) {
	if (len > 255) {
	    if (len > 65535L) {
		if (len > 16777215L) {
		    return 5;
		} else {
		    return 4;
		}
	    } else {
		return 3;
	    }
	} else {
	    return 2;
	}
    } else {
	return 1;
    }
}

unsigned char *
DER_StoreHeader(unsigned char *buf, unsigned int code, uint32 len)
{
    unsigned char b[4];

    b[0] = (len >> 24) & 0xff;
    b[1] = (len >> 16) & 0xff;
    b[2] = (len >> 8) & 0xff;
    b[3] = len & 0xff;
    if ((code & DER_TAGNUM_MASK) == DER_SET
	|| (code & DER_TAGNUM_MASK) == DER_SEQUENCE)
	code |= DER_CONSTRUCTED;
    *buf++ = code;
    if (len > 127) {
	if (len > 255) {
	    if (len > 65535) {
		if (len > 16777215) {
		    *buf++ = 0x84;
		    *buf++ = b[0];
		    *buf++ = b[1];
		    *buf++ = b[2];
		    *buf++ = b[3];
		} else {
		    *buf++ = 0x83;
		    *buf++ = b[1];
		    *buf++ = b[2];
		    *buf++ = b[3];
		}
	    } else {
		*buf++ = 0x82;
		*buf++ = b[2];
		*buf++ = b[3];
	    }
	} else {
	    *buf++ = 0x81;
	    *buf++ = b[3];
	}
    } else {
	*buf++ = b[3];
    }
    return buf;
}

/*
 * XXX This should be rewritten, generalized, to take a long instead
 * of an int32.
 */
SECStatus
DER_SetInteger(PRArenaPool *arena, SECItem *it, int32 i)
{
    unsigned char bb[4];
    unsigned len;

    bb[0] = (unsigned char) (i >> 24);
    bb[1] = (unsigned char) (i >> 16);
    bb[2] = (unsigned char) (i >> 8);
    bb[3] = (unsigned char) (i);

    /*
    ** Small integers are encoded in a single byte. Larger integers
    ** require progressively more space.
    */
    if (i < -128) {
	if (i < -32768L) {
	    if (i < -8388608L) {
		len = 4;
	    } else {
		len = 3;
	    }
	} else {
	    len = 2;
	}
    } else if (i > 127) {
	if (i > 32767L) {
	    if (i > 8388607L) {
		len = 4;
	    } else {
		len = 3;
	    }
	} else {
	    len = 2;
	}
    } else {
	len = 1;
    }
    it->data = (unsigned char*) PORT_ArenaAlloc(arena, len);
    if (!it->data) {
	return SECFailure;
    }
    it->len = len;
    PORT_Memcpy(it->data, bb + (4 - len), len);
    return SECSuccess;
}

/*
 * XXX This should be rewritten, generalized, to take an unsigned long instead
 * of a uint32.
 */
SECStatus
DER_SetUInteger(PRArenaPool *arena, SECItem *it, uint32 ui)
{
    unsigned char bb[5];
    int len;

    bb[0] = 0;
    bb[1] = (unsigned char) (ui >> 24);
    bb[2] = (unsigned char) (ui >> 16);
    bb[3] = (unsigned char) (ui >> 8);
    bb[4] = (unsigned char) (ui);

    /*
    ** Small integers are encoded in a single byte. Larger integers
    ** require progressively more space.
    */
    if (ui > 0x7f) {
	if (ui > 0x7fff) {
	    if (ui > 0x7fffffL) {
		if (ui >= 0x80000000L) {
		    len = 5;
		} else {
		    len = 4;
		}
	    } else {
		len = 3;
	    }
	} else {
	    len = 2;
	}
    } else {
	len = 1;
    }

    it->data = (unsigned char *)PORT_ArenaAlloc(arena, len);
    if (it->data == NULL) {
	return SECFailure;
    }

    it->len = len;
    PORT_Memcpy(it->data, bb + (sizeof(bb) - len), len);

    return SECSuccess;
}

/*
** Convert a der encoded *signed* integer into a machine integral value.
** If an underflow/overflow occurs, sets error code and returns min/max.
*/
long
DER_GetInteger(SECItem *it)
{
    long ival = 0;
    unsigned len = it->len;
    unsigned char *cp = it->data;
    unsigned long overflow = 0xffL << ((sizeof(ival) - 1)*8);

    while (len) {
	if (ival & overflow) {
	    PORT_SetError(SEC_ERROR_BAD_DER);
	    if (ival < 0) {
		return LONG_MIN;
	    }
	    return LONG_MAX;
	}
	ival = ival << 8;
	ival |= *cp++;
	--len;
    }
    return ival;
}

/*
** Convert a der encoded *unsigned* integer into a machine integral value.
** If an underflow/overflow occurs, sets error code and returns min/max.
*/
unsigned long
DER_GetUInteger(SECItem *it)
{
    unsigned long ival = 0;
    unsigned len = it->len;
    unsigned char *cp = it->data;
    unsigned long overflow = 0xffL << ((sizeof(ival) - 1)*8);

    /* Cannot put a negative value into an unsigned container. */
    if (*cp & 0x80) {
	PORT_SetError(SEC_ERROR_BAD_DER);
	return 0;
    }

    while (len) {
	if (ival & overflow) {
	    PORT_SetError(SEC_ERROR_BAD_DER);
	    return ULONG_MAX;
	}
	ival = ival << 8;
	ival |= *cp++;
	--len;
    }
    return ival;
}
