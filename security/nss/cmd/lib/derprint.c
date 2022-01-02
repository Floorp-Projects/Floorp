/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "secutil.h"
#include "secoid.h"

#include <stdint.h>

#ifdef __sun
extern int fprintf(FILE *strm, const char *format, ... /* args */);
extern int fflush(FILE *stream);
#endif

#define RIGHT_MARGIN 24
/*#define RAW_BYTES 1 */

static int prettyColumn = 0;

static int
getInteger256(const unsigned char *data, unsigned int nb)
{
    int val;

    switch (nb) {
        case 1:
            val = data[0];
            break;
        case 2:
            val = (data[0] << 8) | data[1];
            break;
        case 3:
            val = (data[0] << 16) | (data[1] << 8) | data[2];
            break;
        case 4:
            /* If the most significant bit of data[0] is 1, val would be negative.
             * Treat it as an error.
             */
            if (data[0] & 0x80) {
                PORT_SetError(SEC_ERROR_BAD_DER);
                return -1;
            }
            val = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            break;
        default:
            PORT_SetError(SEC_ERROR_BAD_DER);
            return -1;
    }

    return val;
}

static int
prettyNewline(FILE *out)
{
    int rv;

    if (prettyColumn != -1) {
        rv = fprintf(out, "\n");
        prettyColumn = -1;
        if (rv < 0) {
            PORT_SetError(SEC_ERROR_IO);
            return rv;
        }
    }
    return 0;
}

static int
prettyIndent(FILE *out, unsigned level)
{
    unsigned int i;
    int rv;

    if (prettyColumn == -1) {
        prettyColumn = level;
        for (i = 0; i < level; i++) {
            rv = fprintf(out, "   ");
            if (rv < 0) {
                PORT_SetError(SEC_ERROR_IO);
                return rv;
            }
        }
    }

    return 0;
}

static int
prettyPrintByte(FILE *out, unsigned char item, unsigned int level)
{
    int rv;

    rv = prettyIndent(out, level);
    if (rv < 0)
        return rv;

    rv = fprintf(out, "%02x ", item);
    if (rv < 0) {
        PORT_SetError(SEC_ERROR_IO);
        return rv;
    }

    prettyColumn++;
    if (prettyColumn >= RIGHT_MARGIN) {
        return prettyNewline(out);
    }

    return 0;
}

static int
prettyPrintLeaf(FILE *out, const unsigned char *data,
                unsigned int len, unsigned int lv)
{
    unsigned int i;
    int rv;

    for (i = 0; i < len; i++) {
        rv = prettyPrintByte(out, *data++, lv);
        if (rv < 0)
            return rv;
    }
    return prettyNewline(out);
}

static int
prettyPrintStringStart(FILE *out, const unsigned char *str,
                       unsigned int len, unsigned int level)
{
#define BUF_SIZE 100
    unsigned char buf[BUF_SIZE];
    int rv;

    if (len >= BUF_SIZE)
        len = BUF_SIZE - 1;

    rv = prettyNewline(out);
    if (rv < 0)
        return rv;

    rv = prettyIndent(out, level);
    if (rv < 0)
        return rv;

    memcpy(buf, str, len);
    buf[len] = '\000';

    rv = fprintf(out, "\"%s\"", buf);
    if (rv < 0) {
        PORT_SetError(SEC_ERROR_IO);
        return rv;
    }

    return 0;
#undef BUF_SIZE
}

static int
prettyPrintString(FILE *out, const unsigned char *str,
                  unsigned int len, unsigned int level, PRBool raw)
{
    int rv;

    rv = prettyPrintStringStart(out, str, len, level);
    if (rv < 0)
        return rv;

    rv = prettyNewline(out);
    if (rv < 0)
        return rv;

    if (raw) {
        rv = prettyPrintLeaf(out, str, len, level);
        if (rv < 0)
            return rv;
    }

    return 0;
}

static int
prettyPrintTime(FILE *out, const unsigned char *str,
                unsigned int len, unsigned int level, PRBool raw, PRBool utc)
{
    SECItem time_item;
    int rv;

    rv = prettyPrintStringStart(out, str, len, level);
    if (rv < 0)
        return rv;

    time_item.data = (unsigned char *)str;
    time_item.len = len;

    rv = fprintf(out, " (");
    if (rv < 0) {
        PORT_SetError(SEC_ERROR_IO);
        return rv;
    }

    if (utc)
        SECU_PrintUTCTime(out, &time_item, NULL, 0);
    else
        SECU_PrintGeneralizedTime(out, &time_item, NULL, 0);

    rv = fprintf(out, ")");
    if (rv < 0) {
        PORT_SetError(SEC_ERROR_IO);
        return rv;
    }

    rv = prettyNewline(out);
    if (rv < 0)
        return rv;

    if (raw) {
        rv = prettyPrintLeaf(out, str, len, level);
        if (rv < 0)
            return rv;
    }

    return 0;
}

static int
prettyPrintObjectID(FILE *out, const unsigned char *data,
                    unsigned int len, unsigned int level, PRBool raw)
{
    SECOidData *oiddata;
    SECItem oiditem;
    unsigned int i;
    unsigned long val;
    int rv;

    /*
     * First print the Object Id in numeric format
     */

    rv = prettyIndent(out, level);
    if (rv < 0)
        return rv;

    if (len == 0) {
        PORT_SetError(SEC_ERROR_BAD_DER);
        return -1;
    }
    val = data[0];
    i = val % 40;
    val = val / 40;
    rv = fprintf(out, "%lu %u ", val, i);
    if (rv < 0) {
        PORT_SetError(SEC_ERROR_IO);
        return rv;
    }

    val = 0;
    for (i = 1; i < len; ++i) {
        unsigned long j;

        j = data[i];
        val = (val << 7) | (j & 0x7f);
        if (j & 0x80)
            continue;
        rv = fprintf(out, "%lu ", val);
        if (rv < 0) {
            PORT_SetError(SEC_ERROR_IO);
            return rv;
        }
        val = 0;
    }

    /*
     * Now try to look it up and print a symbolic version.
     */
    oiditem.data = (unsigned char *)data;
    oiditem.len = len;
    oiddata = SECOID_FindOID(&oiditem);
    if (oiddata != NULL) {
        i = PORT_Strlen(oiddata->desc);
        if ((prettyColumn + 1 + (i / 3)) > RIGHT_MARGIN) {
            rv = prettyNewline(out);
            if (rv < 0)
                return rv;
        }

        rv = prettyIndent(out, level);
        if (rv < 0)
            return rv;

        rv = fprintf(out, "(%s)", oiddata->desc);
        if (rv < 0) {
            PORT_SetError(SEC_ERROR_IO);
            return rv;
        }
    }

    rv = prettyNewline(out);
    if (rv < 0)
        return rv;

    if (raw) {
        rv = prettyPrintLeaf(out, data, len, level);
        if (rv < 0)
            return rv;
    }

    return 0;
}

static char *prettyTagType[32] = {
    "End of Contents",
    "Boolean",
    "Integer",
    "Bit String",
    "Octet String",
    "NULL",
    "Object Identifier",
    "0x07",
    "0x08",
    "0x09",
    "Enumerated",
    "0x0B",
    "UTF8 String",
    "0x0D",
    "0x0E",
    "0x0F",
    "Sequence",
    "Set",
    "0x12",
    "Printable String",
    "T61 String",
    "0x15",
    "IA5 String",
    "UTC Time",
    "Generalized Time",
    "0x19",
    "Visible String",
    "0x1B",
    "Universal String",
    "0x1D",
    "BMP String",
    "High-Tag-Number"
};

static int
prettyPrintTag(FILE *out, const unsigned char *src, const unsigned char *end,
               unsigned char *codep, unsigned int level, PRBool raw)
{
    int rv;
    unsigned char code, tagnum;

    if (src >= end) {
        PORT_SetError(SEC_ERROR_BAD_DER);
        return -1;
    }

    code = *src;
    tagnum = code & SEC_ASN1_TAGNUM_MASK;

    /*
     * NOTE: This code does not (yet) handle the high-tag-number form!
     */
    if (tagnum == SEC_ASN1_HIGH_TAG_NUMBER) {
        PORT_SetError(SEC_ERROR_BAD_DER);
        return -1;
    }

    if (raw)
        rv = prettyPrintByte(out, code, level);
    else
        rv = prettyIndent(out, level);

    if (rv < 0)
        return rv;

    if (code & SEC_ASN1_CONSTRUCTED) {
        rv = fprintf(out, "C-");
        if (rv < 0) {
            PORT_SetError(SEC_ERROR_IO);
            return rv;
        }
    }

    switch (code & SEC_ASN1_CLASS_MASK) {
        case SEC_ASN1_UNIVERSAL:
            rv = fprintf(out, "%s ", prettyTagType[tagnum]);
            break;
        case SEC_ASN1_APPLICATION:
            rv = fprintf(out, "Application: %d ", tagnum);
            break;
        case SEC_ASN1_CONTEXT_SPECIFIC:
            rv = fprintf(out, "[%d] ", tagnum);
            break;
        case SEC_ASN1_PRIVATE:
            rv = fprintf(out, "Private: %d ", tagnum);
            break;
    }

    if (rv < 0) {
        PORT_SetError(SEC_ERROR_IO);
        return rv;
    }

    *codep = code;

    return 1;
}

static int
prettyPrintLength(FILE *out, const unsigned char *data, const unsigned char *end,
                  int *lenp, PRBool *indefinitep, unsigned int lv, PRBool raw)
{
    unsigned char lbyte;
    int lenLen;
    int rv;

    if (data >= end) {
        PORT_SetError(SEC_ERROR_BAD_DER);
        return -1;
    }

    rv = fprintf(out, " ");
    if (rv < 0) {
        PORT_SetError(SEC_ERROR_IO);
        return rv;
    }

    *indefinitep = PR_FALSE;

    lbyte = *data++;
    lenLen = 1;
    if (lbyte >= 0x80) {
        /* Multibyte length */
        unsigned nb = (unsigned)(lbyte & 0x7f);
        if (nb > 4) {
            PORT_SetError(SEC_ERROR_BAD_DER);
            return -1;
        }
        if (nb > 0) {
            int il;

            if ((data + nb) > end) {
                PORT_SetError(SEC_ERROR_BAD_DER);
                return -1;
            }
            il = getInteger256(data, nb);
            if (il < 0)
                return -1;
            *lenp = (unsigned)il;
        } else {
            *lenp = 0;
            *indefinitep = PR_TRUE;
        }
        lenLen += nb;
        if (raw) {
            unsigned int i;

            rv = prettyPrintByte(out, lbyte, lv);
            if (rv < 0)
                return rv;
            for (i = 0; i < nb; i++) {
                rv = prettyPrintByte(out, data[i], lv);
                if (rv < 0)
                    return rv;
            }
        }
    } else {
        *lenp = lbyte;
        if (raw) {
            rv = prettyPrintByte(out, lbyte, lv);
            if (rv < 0)
                return rv;
        }
    }
    if (*indefinitep)
        rv = fprintf(out, "(indefinite)\n");
    else
        rv = fprintf(out, "(%d)\n", *lenp);
    if (rv < 0) {
        PORT_SetError(SEC_ERROR_IO);
        return rv;
    }

    prettyColumn = -1;
    return lenLen;
}

static int
prettyPrintItem(FILE *out, const unsigned char *data, const unsigned char *end,
                unsigned int lv, PRBool raw)
{
    int slen;
    int lenLen;
    const unsigned char *orig = data;
    int rv;

    while (data < end) {
        unsigned char code;
        PRBool indefinite;

        slen = prettyPrintTag(out, data, end, &code, lv, raw);
        if (slen < 0)
            return slen;
        data += slen;

        lenLen = prettyPrintLength(out, data, end, &slen, &indefinite, lv, raw);
        if (lenLen < 0)
            return lenLen;
        data += lenLen;

        /*
         * Just quit now if slen more bytes puts us off the end.
         */
        if (data > end || slen > (end - data)) {
            PORT_SetError(SEC_ERROR_BAD_DER);
            return -1;
        }

        if (code & SEC_ASN1_CONSTRUCTED) {
            if (slen > 0 || indefinite) {
                slen = prettyPrintItem(out, data,
                                       slen == 0 ? end : data + slen,
                                       lv + 1, raw);
                if (slen < 0)
                    return slen;
                data += slen;
            }
        } else if (code == 0) {
            if (slen != 0 || lenLen != 1) {
                PORT_SetError(SEC_ERROR_BAD_DER);
                return -1;
            }
            break;
        } else {
            switch (code) {
                case SEC_ASN1_PRINTABLE_STRING:
                case SEC_ASN1_IA5_STRING:
                case SEC_ASN1_VISIBLE_STRING:
                    rv = prettyPrintString(out, data, slen, lv + 1, raw);
                    if (rv < 0)
                        return rv;
                    break;
                case SEC_ASN1_UTC_TIME:
                    rv = prettyPrintTime(out, data, slen, lv + 1, raw, PR_TRUE);
                    if (rv < 0)
                        return rv;
                    break;
                case SEC_ASN1_GENERALIZED_TIME:
                    rv = prettyPrintTime(out, data, slen, lv + 1, raw, PR_FALSE);
                    if (rv < 0)
                        return rv;
                    break;
                case SEC_ASN1_OBJECT_ID:
                    rv = prettyPrintObjectID(out, data, slen, lv + 1, raw);
                    if (rv < 0)
                        return rv;
                    break;
                case SEC_ASN1_BOOLEAN:    /* could do nicer job */
                case SEC_ASN1_INTEGER:    /* could do nicer job */
                case SEC_ASN1_BIT_STRING: /* could do nicer job */
                case SEC_ASN1_OCTET_STRING:
                case SEC_ASN1_NULL:
                case SEC_ASN1_ENUMERATED: /* could do nicer job, as INTEGER */
                case SEC_ASN1_UTF8_STRING:
                case SEC_ASN1_T61_STRING: /* print as printable string? */
                case SEC_ASN1_UNIVERSAL_STRING:
                case SEC_ASN1_BMP_STRING:
                default:
                    rv = prettyPrintLeaf(out, data, slen, lv + 1);
                    if (rv < 0)
                        return rv;
                    break;
            }
            data += slen;
        }
    }

    rv = prettyNewline(out);
    if (rv < 0)
        return rv;

    return data - orig;
}

SECStatus
DER_PrettyPrint(FILE *out, const SECItem *it, PRBool raw)
{
    int rv;

    prettyColumn = -1;

    rv = prettyPrintItem(out, it->data, it->data + it->len, 0, raw);
    if (rv < 0)
        return SECFailure;
    return SECSuccess;
}
