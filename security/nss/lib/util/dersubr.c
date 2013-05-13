/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secder.h"
#include <limits.h>
#include "secerr.h"

int
DER_LengthLength(PRUint32 len)
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
DER_StoreHeader(unsigned char *buf, unsigned int code, PRUint32 len)
{
    unsigned char b[4];

    b[0] = (unsigned char)(len >> 24);
    b[1] = (unsigned char)(len >> 16);
    b[2] = (unsigned char)(len >> 8);
    b[3] = (unsigned char)len;
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
 * of a PRInt32.
 */
SECStatus
DER_SetInteger(PLArenaPool *arena, SECItem *it, PRInt32 i)
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
 * of a PRUint32.
 */
SECStatus
DER_SetUInteger(PLArenaPool *arena, SECItem *it, PRUint32 ui)
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
DER_GetInteger(const SECItem *it)
{
    long ival = 0;
    unsigned len = it->len;
    unsigned char *cp = it->data;
    unsigned long overflow = 0x1ffUL << (((sizeof(ival) - 1) * 8) - 1);
    unsigned long ofloinit;

    PORT_Assert(len);
    if (!len) {
	PORT_SetError(SEC_ERROR_INPUT_LEN);
	return 0;
    }

    if (*cp & 0x80)
    	ival = -1L;
    ofloinit = ival & overflow;

    while (len) {
	if ((ival & overflow) != ofloinit) {
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
** If an overflow occurs, sets error code and returns max.
*/
unsigned long
DER_GetUInteger(SECItem *it)
{
    unsigned long ival = 0;
    unsigned len = it->len;
    unsigned char *cp = it->data;
    unsigned long overflow = 0xffUL << ((sizeof(ival) - 1) * 8);

    PORT_Assert(len);
    if (!len) {
	PORT_SetError(SEC_ERROR_INPUT_LEN);
	return 0;
    }

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
