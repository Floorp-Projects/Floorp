/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secder.h"
#include "secerr.h"

static PRUint32
der_indefinite_length(unsigned char *buf, unsigned char *end)
{
    PRUint32 len, ret, dataLen;
    unsigned char tag, lenCode;
    int dataLenLen;

    len = 0;
    while (1) {
        if ((buf + 2) > end) {
            return (0);
        }

        tag = *buf++;
        lenCode = *buf++;
        len += 2;

        if ((tag == 0) && (lenCode == 0)) {
            return (len);
        }

        if (lenCode == 0x80) {                     /* indefinite length */
            ret = der_indefinite_length(buf, end); /* recurse to find length */
            if (ret == 0)
                return 0;
            len += ret;
            buf += ret;
        } else { /* definite length */
            if (lenCode & 0x80) {
                /* Length of data is in multibyte format */
                dataLenLen = lenCode & 0x7f;
                switch (dataLenLen) {
                    case 1:
                        dataLen = buf[0];
                        break;
                    case 2:
                        dataLen = (buf[0] << 8) | buf[1];
                        break;
                    case 3:
                        dataLen = ((unsigned long)buf[0] << 16) | (buf[1] << 8) | buf[2];
                        break;
                    case 4:
                        dataLen = ((unsigned long)buf[0] << 24) |
                                  ((unsigned long)buf[1] << 16) | (buf[2] << 8) | buf[3];
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
            case 0: /* end-of-contents tag */
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
                contents_len = der_indefinite_length(bp, end);
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

    *header_len_p = (int)(bp - buf);
    *contents_len_p = contents_len;

    return SECSuccess;
}

SECStatus
DER_Lengths(SECItem *item, int *header_len_p, PRUint32 *contents_len_p)
{
    return (der_capture(item->data, &item->data[item->len], header_len_p,
                        contents_len_p));
}
