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
 * The Original Code is the Network Security Services.
 *
 * The Initial Developer of the Original Code is Nelson B Bolyard
 * Portions created by the Initial Developer are Copyright (C) 2007
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

#include <string.h>
#include "secitem.h"
#include "secport.h"
#include "secerr.h"

/* if to->data is not NULL, and to->len is large enough to hold the result,
 * then the resultant OID will be copyed into to->data, and to->len will be
 * changed to show the actual OID length.
 * Otherwise, memory for the OID will be allocated (from the caller's 
 * PLArenaPool, if pool is non-NULL) and to->data will receive the address
 * of the allocated data, and to->len will receive the OID length.
 * The original value of to->data is not freed when a new buffer is allocated.
 * 
 * The input string may begin with "OID." and this still be ignored.
 * The length of the input string is given in len.  If len == 0, then 
 * len will be computed as strlen(from), meaning it must be NUL terminated.
 * It is an error if from == NULL, or if *from == '\0'.
 */

SECStatus
SEC_StringToOID(PLArenaPool *pool, SECItem *to, const char *from, PRUint32 len)
{
    PRUint32 decimal_numbers = 0;
    PRUint32 result_bytes = 0;
    SECStatus rv;
    PRUint8 result[1024];

    static const PRUint32 max_decimal = (0xffffffff / 10);
    static const char OIDstring[] = {"OID."};

    if (!from || !to) {
    	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }
    if (!len) {
    	len = PL_strlen(from);
    }
    if (len >= 4 && !PL_strncasecmp(from, OIDstring, 4)) {
    	from += 4; /* skip leading "OID." if present */
	len  -= 4;
    }
    if (!len) {
bad_data:
    	PORT_SetError(SEC_ERROR_BAD_DATA);
	return SECFailure;
    }
    do {
	PRUint32 decimal = 0;
        while (len > 0 && isdigit(*from)) {
	    PRUint32 addend = (*from++ - '0');
	    --len;
	    if (decimal > max_decimal)  /* overflow */
		goto bad_data;
	    decimal = (decimal * 10) + addend;
	    if (decimal < addend)	/* overflow */
		goto bad_data;
	}
	if (len != 0 && *from != '.') {
	    goto bad_data;
	}
	if (decimal_numbers == 0) {
	    if (decimal > 2)
	    	goto bad_data;
	    result[0] = decimal * 40;
	    result_bytes = 1;
	} else if (decimal_numbers == 1) {
	    if (decimal > 40)
	    	goto bad_data;
	    result[0] += decimal;
	} else {
	    /* encode the decimal number,  */
	    PRUint8 * rp;
	    PRUint32 num_bytes = 0;
	    PRUint32 tmp = decimal;
	    while (tmp) {
	        num_bytes++;
		tmp >>= 7;
	    }
	    if (!num_bytes )
	    	++num_bytes;  /* use one byte for a zero value */
	    if (num_bytes + result_bytes > sizeof result)
	    	goto bad_data;
	    tmp = num_bytes;
	    rp = result + result_bytes - 1;
	    rp[tmp] = (PRUint8)(decimal & 0x7f);
	    decimal >>= 7;
	    while (--tmp > 0) {
		rp[tmp] = (PRUint8)(decimal | 0x80);
		decimal >>= 7;
	    }
	    result_bytes += num_bytes;
	}
	++decimal_numbers;
	if (len > 0) { /* skip trailing '.' */
	    ++from;
	    --len;
	}
    } while (len > 0);
    /* now result contains result_bytes of data */
    if (to->data && to->len >= result_bytes) {
    	PORT_Memcpy(to->data, result, to->len = result_bytes);
	rv = SECSuccess;
    } else {
    	SECItem result_item = {siBuffer, NULL, 0 };
	result_item.data = result;
	result_item.len  = result_bytes;
	rv = SECITEM_CopyItem(pool, to, &result_item);
    }
    return rv;
}
