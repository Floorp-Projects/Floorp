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
#include "secutil.h"

#ifdef __sun
extern int fprintf(FILE *strm, const char *format, .../* args */);
extern int fflush(FILE *stream);
#endif

#define RIGHT_MARGIN 24
/*#define RAW_BYTES 1 */

typedef unsigned char Byte;

static int prettyColumn;	/* NOTE: This is NOT reentrant. */

static char *prettyTagType [32] = {
  "End of Contents", "Boolean", "Integer", "Bit String", "Octet String",
  "NULL", "Object Identifier", "0x07", "0x08", "0x09", "0x0A",
  "0x0B", "0X0C", "0X0D", "0X0E", "0X0F", "Sequence", "Set",
  "0x12", "Printable String", "T61 String", "0x15", "IA5 String",
  "UTCTime",
  "0x18", "0x19", "0x1A", "0x1B", "0x1C", "0x1D", "0x1E",
  "High-Tag-Number"
};

static SECStatus prettyNewline(FILE *out)
{
    int rv;

    if (prettyColumn != -1) {
	rv = fprintf(out, "\n");
	prettyColumn = -1;
	if (rv < 0) {
	    PORT_SetError(SEC_ERROR_IO);
	    return SECFailure;
	}
    }
    return SECSuccess;
}

static SECStatus prettyIndent(FILE *out, unsigned level)
{
    unsigned i;
    SECStatus rv;

    if (prettyColumn == -1) {
	prettyColumn = level;
	for (i = 0; i < level; i++) {
	    rv = (SECStatus) fprintf(out, "   ");
	    if (rv < 0) {
		PORT_SetError(SEC_ERROR_IO);
		return SECFailure;
	    }
	}
    }
    return SECSuccess;
}

static SECStatus prettyPrintByte(FILE *out, Byte item, unsigned level)
{
    SECStatus rv;

    rv = prettyIndent(out, level);
    if (rv) return rv;

    rv = (SECStatus) fprintf(out, "%02x ", item);
    if (rv < 0) {
	PORT_SetError(SEC_ERROR_IO);
	return SECFailure;
    }
    prettyColumn++;
    if (prettyColumn >= RIGHT_MARGIN) {
	rv = prettyNewline(out);
	return rv;
    }
    return SECSuccess;
}

static SECStatus prettyPrintCString(FILE *out, char *str, unsigned level)
{
    SECStatus rv;

    rv = prettyNewline(out);
    if (rv) return rv;

    rv = prettyIndent(out, level);
    if (rv) return rv;

    rv = (SECStatus) fprintf(out, str);
    if (rv < 0) {
	PORT_SetError(SEC_ERROR_IO);
	return SECFailure;
    }

    rv = prettyNewline(out);
    return rv;
}

static SECStatus prettyPrintLeafString(FILE *out, SECArb *arb, unsigned lv)
{
    unsigned char buf[100];
    SECStatus rv;
    int length = arb->body.item.len;

    if (length > 0 && arb->body.item.data == NULL)
	return prettyPrintCString(out, "(string contents not available)", lv);

    if (length >= sizeof(buf))
      length = sizeof(buf) - 1;

    rv = prettyNewline(out);
    if (rv) return rv;

    rv = prettyIndent(out, lv);
    if (rv) return rv;

    memcpy(buf, arb->body.item.data, length);
    buf[length] = '\000';

    rv = (SECStatus) fprintf(out, "\"%s\"", buf);
    if (rv < 0) {
	PORT_SetError(SEC_ERROR_IO);
	return SECFailure;
    }
    rv = prettyNewline(out);
    if (rv) return rv;

    return SECSuccess;
}

static SECStatus prettyPrintLeaf(FILE *out, SECArb* arb, unsigned lv)
{
    unsigned i;
    SECStatus rv;
    Byte *data = arb->body.item.data;

    if (data == NULL && arb->body.item.len > 0)
	return prettyPrintCString(out, "(data not availabe)", lv);

    for (i = 0; i < arb->body.item.len; i++) {
	rv = prettyPrintByte(out, *data++, lv);
	if (rv) return rv;
    }

    rv = prettyNewline(out);
    return rv;
}

static SECStatus prettyPrintEndContents(FILE *out, unsigned level)
{
      return prettyPrintCString(out, prettyTagType[0], level);
}

static SECStatus prettyPrintTag(FILE *out, SECArb *arb, unsigned level)
{
    int rv;

#ifdef RAW_BYTES
    if (prettyPrintByte(out, arb->tag, level)) return PR_TRUE;
#else
    if (prettyIndent(out, level)) return PR_TRUE;
#endif

    if (arb->tag & DER_CONSTRUCTED) {
        rv = fprintf(out, "C-");
	if (rv < 0) {
	    PORT_SetError(SEC_ERROR_IO);
	    return PR_TRUE;
	}
    }

    switch(arb->tag & 0xC0) {
    case DER_UNIVERSAL:
        rv = fprintf(out, "%s ", prettyTagType[arb->tag & 0x17]);
	break;
    case DER_APPLICATION:
        rv = fprintf(out, "Application: %d ", arb->tag & 0x17);
	break;
    case DER_CONTEXT_SPECIFIC:
        rv = fprintf(out, "[%d] ", arb->tag & 0x17);
	break;
    case DER_PRIVATE:
        rv = fprintf(out, "Private: %d ", arb->tag & 0x17);
	break;
    }

    if (rv < 0) {
        PORT_SetError(SEC_ERROR_IO);
	return PR_TRUE;
    }
    return PR_FALSE;
}

static SECStatus prettyPrintLength(FILE *out, SECArb *arb, unsigned lv)
{
    int rv = fprintf(out, " (%d)\n", arb->length);
    if (rv < 0) {
        PORT_SetError(SEC_ERROR_IO);
	return -1;
    }
    prettyColumn = -1;
    return PR_FALSE;
}

static SECStatus prettyPrint(FILE *out, SECArb *arb, unsigned lv)
{
    int i;

    prettyPrintTag(out, arb, lv);
    prettyPrintLength(out, arb, lv);

    if (arb->tag & DER_CONSTRUCTED) {
	for (i = 0; i < arb->body.cons.numSubs; i++)
	    prettyPrint(out, arb->body.cons.subs[i], lv + 1);
	if (arb->length == 0)
	    prettyPrintEndContents(out, lv);
    } else {
	if (arb->tag == DER_PRINTABLE_STRING || arb->tag == DER_UTC_TIME) {
	    if (prettyPrintLeafString(out, arb, lv+1)) return PR_TRUE;
#ifdef RAW_BYTES
	    if (prettyPrintLeaf(out, arb, lv+1)) return PR_TRUE;
#endif
	} else {
	    if (prettyPrintLeaf(out, arb, lv+1)) return PR_TRUE;
	}
    }

    return prettyNewline(out);
}

SECStatus BER_PrettyPrintArb(FILE *out, SECArb *a)
{
    prettyColumn = -1;
    return prettyPrint(out, a, 0);
}

/*
 * PackHeader puts the tag and length field into a buffer provided by the
 * client. It assumes the client has made the buffer at least big
 * enough. (8 bytes will suffice for now). It returns the number of
 * valid bytes
 */
static int PackHeader(SECArb *arb, unsigned char *header)
{
    int tagBytes, lengthBytes;
    unsigned long length = arb->length;
    /*
     * XXX only handles short form tags
     */
    header[0] = arb->tag;
    tagBytes = 1;

    if (length < 0x80) {
	if ((arb->tag & DER_CONSTRUCTED) && (arb->length == 0))
	    header[tagBytes] = 0x80; /* indefinite length form */
	else
	    header[tagBytes] = arb->length;
	lengthBytes = 1;
    } else {
	int i;
	lengthBytes = 0;
	for (i = 0; i < 4; i++) {
	    if ((length & 0xFF000000) || (lengthBytes > 0)) {
		header[tagBytes + lengthBytes + 1] = length >> 24;
		lengthBytes++;
	    }
	    length <<= 8;
	}
	header[tagBytes] = 0x80 + lengthBytes;
	lengthBytes++;  /* allow for the room for the length length */
    }
    return tagBytes + lengthBytes;
}

SECStatus BER_Unparse(SECArb *arb, BERUnparseProc proc, void *instance)
{
    int i, length;
    SECStatus rv;
    unsigned char header[12];

    length = PackHeader(arb, header);
    rv = (*proc)(instance, header, length, arb);
    if (rv) return rv;

    if (arb->tag & DER_CONSTRUCTED) {
	for (i = 0; i < arb->body.cons.numSubs; i++) {
	    rv = BER_Unparse(arb->body.cons.subs[i], proc, instance);
	    if (rv) return rv;
	}
	if (arb->length == 0) {
	    rv = (*proc)(instance, "\0\0", 2, arb);
	    if (rv) return rv;
	}
    } else {
	if ((arb->length > 0) && (arb->body.item.data == NULL))
	    return SECFailure;
	if (arb->length != arb->body.item.len)
	    return SECFailure;
	rv = (*proc)(instance, arb->body.item.data, arb->length, arb);
    }
    return SECSuccess;
}

static int ResolveLength(SECArb *arb)
{
    int length, i, rv;
    unsigned char header[12];

    /*
     * One theory is that there might be valid subtrees along with
     * still yet uknown length trees. That would imply that the scan
     * should not be aborted on first failure.
     */

    length = PackHeader(arb, header);
    if (arb->tag | DER_CONSTRUCTED) {
	if (arb->length > 0)
	    length += arb->length;
	else {
	    for (i = 0; i < arb->body.cons.numSubs; i++) {
		rv = ResolveLength(arb->body.cons.subs[i]);
		if (rv < 0) return -1;
		length += rv;
	    }
	    arb->length = length;
	}
    } else {
	if (arb->length != arb->body.item.len)
	    return -1;
	length += arb->length;
    }
    return length;
}

SECStatus BER_ResolveLengths(SECArb *arb)
{
    int rv;
    rv = ResolveLength(arb);
    if (rv < 0) return SECFailure;
    return SECSuccess;    
}
