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
#include "prio.h"
#include "prprf.h"
#include "prmem.h"
#include "seccomon.h"
#include "secerr.h"
#include "secder.h"
#include "secoid.h"
#include "prerror.h"
#include "secasn1.h"
#include "serv.h"

#define RIGHT_MARGIN	24
/*#define RAW_BYTES 1 */

static int prettyColumn = 0;

static int
getInteger256(unsigned char *data, unsigned int nb)
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
	val = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
	break;
      default:
	PR_SetError(SEC_ERROR_BAD_DER,0);
	return -1;
    }

    return val;
}

static int
prettyNewline(PRFileDesc *out)
{
    int rv;

    if (prettyColumn != -1) {
	rv = PR_Write(out, "\n",1);
	prettyColumn = -1;
	if (rv < 0) {
	    PR_SetError(SEC_ERROR_IO,0);
	    return rv;
	}
    }
    return 0;
}

#define PRETTY_INDENT "   "

static int
prettyIndent(PRFileDesc *out, unsigned level)
{
    unsigned int i;
    int rv;

    if (prettyColumn == -1) {
	prettyColumn = level;
	for (i = 0; i < level; i++) {
	    rv = PR_Write(out, PRETTY_INDENT, PL_strlen(PRETTY_INDENT));
	    if (rv < 0) {
		PR_SetError(SEC_ERROR_IO,0);
		return rv;
	    }
	}
    }

    return 0;
}

static int
prettyPrintByte(PRFileDesc *out, unsigned char item, unsigned int level)
{
    int rv;
    char *outdata;

    rv = prettyIndent(out, level);
    if (rv < 0)
	return rv;

    outdata = PR_smprintf("%02x ", item);
    rv = PR_Write(out, outdata, PL_strlen(outdata));
    PR_Free(outdata);
    if (rv < 0) {
	PR_SetError(SEC_ERROR_IO,0);
	return rv;
    }

    prettyColumn++;
    if (prettyColumn >= RIGHT_MARGIN) {
	return prettyNewline(out);
    }

    return 0;
}

static int
prettyPrintLeaf(PRFileDesc *out, unsigned char *data,
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
prettyPrintStringStart(PRFileDesc *out, unsigned char *str,
		       unsigned int len, unsigned int level)
{
#define BUF_SIZE 100
    unsigned char staticBuf[BUF_SIZE];
    unsigned char *buf, *dynamicBuf = NULL;
    int rv;

    if (len >= BUF_SIZE) {
      /* len = BUF_SIZE - 1; */
        buf = dynamicBuf = SSM_NEW_ARRAY(char, len+1);
    } else {
        buf = staticBuf;
    }

    rv = prettyNewline(out);
    if (rv < 0)
        goto loser;

    rv = prettyIndent(out, level);
    if (rv < 0)
        goto loser;

    memcpy(buf, str, len);
    buf[len] = '\000';
    
    rv = PR_Write(out, buf, len);
    if (rv < 0) {
	PR_SetError(SEC_ERROR_IO,0);
	goto loser;
    }

    return 0;
 loser:
    PR_FREEIF(dynamicBuf);
    return rv;
#undef BUF_SIZE
}

static int
prettyPrintString(PRFileDesc *out, unsigned char *str,
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

static void
pretty_PrintTime(PRFileDesc *out, int64 time, char *m, int level)
{
    PRExplodedTime printableTime;
    char *timeString;
    char *newmessage;

    /* Convert to local time */
    PR_ExplodeTime(time, PR_GMTParameters, &printableTime);

    timeString = PORT_Alloc(100);
    if (timeString == NULL)
        return;

    if (m != NULL) {
      /*        SECU_Indent(out, level);*/
        prettyIndent(out, level);
	newmessage = PR_smprintf("%s: ", m);
        PR_Write(out, newmessage, PL_strlen(newmessage));
	PR_Free(newmessage);
    }

    PR_FormatTime(timeString, 100, "%a %b %d %H:%M:%S %Y", &printableTime);
    PR_Write(out, timeString, PL_strlen(timeString));

    if (m != NULL)
        PR_Write(out, "\n", 1);

    PR_Free(timeString);
}

static int
prettyPrintObjectID(PRFileDesc *out, unsigned char *data,
                    unsigned int len, unsigned int level, PRBool raw)
{
    SECOidData *oiddata;
    SECItem oiditem;
    unsigned int i;
    unsigned long val;
    int rv;
    char buf[300];


    /*
     * First print the Object Id in numeric format
     */

    rv = prettyIndent(out, level);
    if (rv < 0)
        return rv;

    val = data[0];
    i   = val % 40;
    val = val / 40;
    PR_snprintf(buf, 300, "%lu %u ", val, i); 
    rv = PR_Write(out,buf,PL_strlen(buf)); 
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
	PR_snprintf(buf, 300, "%lu ", val);
        rv = PR_Write(out, buf, PL_strlen(buf));
        if (rv < 0) {
            PR_SetError(SEC_ERROR_IO,0);
            return rv;
        }
        val = 0;
    }

    /*
     * Now try to look it up and print a symbolic version.
     */
    oiditem.data = data;
    oiditem.len = len;
    oiddata = SECOID_FindOID(&oiditem);
    if (oiddata != NULL) {
        i = PL_strlen(oiddata->desc);
        if ((prettyColumn + 1 + (i / 3)) > RIGHT_MARGIN) {
            rv = prettyNewline(out);
            if (rv < 0)
                return rv;
        }

        rv = prettyIndent(out, level);
        if (rv < 0)
	    return rv;
	
	PR_snprintf(buf, 300, "(%s)", oiddata->desc);
        rv = PR_Write(out, buf, PL_strlen(buf));
        if (rv < 0) {
	    PR_SetError(SEC_ERROR_IO,0);
            return rv;
        }
    }

    /*
     * Finally, on a new line, print the raw bytes (if requested).
     */
    if (raw) {
        rv = prettyNewline(out);
        if (rv < 0) {
            PR_SetError(SEC_ERROR_IO,0);
            return rv;
        }

        for (i = 0; i < len; i++) {
            rv = prettyPrintByte(out, *data++, level);
            if (rv < 0)
                return rv;
        }
    }

    return prettyNewline(out);
}

static char *prettyTagType [32] = {
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
prettyPrintTime(PRFileDesc *out, unsigned char *str,
		unsigned int len, unsigned int level, PRBool raw, PRBool utc)
{
    SECItem time_item;
    int rv;
    SECStatus srv;
    int64 time;

    rv = prettyPrintStringStart(out, str, len, level);
    if (rv < 0)
	return rv;

    time_item.data = str;
    time_item.len = len;

    rv = PR_Write(out, " (", 2);
    if (rv < 0) {
	PR_SetError(SEC_ERROR_IO,0);
	return rv;
    }

    if (utc) {
        srv = DER_UTCTimeToTime(&time, &time_item);
	if (srv == SECSuccess) 
	    pretty_PrintTime(out, time, NULL, 0);
    } else{
        srv = DER_GeneralizedTimeToTime(&time, &time_item);
	if (srv == SECSuccess)
	    pretty_PrintTime(out, time, NULL, 0);
    }

    rv = PR_Write(out, ")", 1);
    if (rv < 0) {
	PR_SetError(SEC_ERROR_IO,0);
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
prettyPrintTag(PRFileDesc *out, unsigned char *src, unsigned char *end,
	       unsigned char *codep, unsigned int level, PRBool raw)
{
    int rv;
    unsigned char code, tagnum;
    char *msg;

    if (src >= end) {
	PR_SetError(SEC_ERROR_BAD_DER,0);
	return -1;
    }

    code = *src;
    tagnum = code & SEC_ASN1_TAGNUM_MASK;

    /*
     * NOTE: This code does not (yet) handle the high-tag-number form!
     */
    if (tagnum == SEC_ASN1_HIGH_TAG_NUMBER) {
        PR_SetError(SEC_ERROR_BAD_DER,0);
	return -1;
    }

    if (raw)
	rv = prettyPrintByte(out, code, level);
    else
	rv = prettyIndent(out, level);

    if (rv < 0)
	return rv;

    if (code & SEC_ASN1_CONSTRUCTED) {
        rv = PR_Write(out, "C-",2);
	if (rv < 0) {
	    PR_SetError(SEC_ERROR_IO,0);
	    return rv;
	}
    }

    switch (code & SEC_ASN1_CLASS_MASK) {
    case SEC_ASN1_UNIVERSAL:
        rv = PR_Write(out, prettyTagType[tagnum], 
		      PL_strlen(prettyTagType[tagnum]));
	break;
    case SEC_ASN1_APPLICATION:
        msg = PR_smprintf("Application: %d ", tagnum);
        rv = PR_Write(out,msg, PL_strlen(msg));
	PR_Free(msg);
	break;
    case SEC_ASN1_CONTEXT_SPECIFIC:
        msg = PR_smprintf("[%d] ", tagnum);
        rv = PR_Write(out, msg, PL_strlen(msg));
	PR_Free(msg);
	break;
    case SEC_ASN1_PRIVATE:
        msg = PR_smprintf("Private: %d ", tagnum);
        rv = PR_Write(out, msg, PL_strlen(msg));
	break;
    }

    if (rv < 0) {
        PR_SetError(SEC_ERROR_IO,0);
	return rv;
    }

    *codep = code;

    return 1;
}

static int
prettyPrintLength(PRFileDesc *out, unsigned char *data, unsigned char *end,
		  int *lenp, PRBool *indefinitep, unsigned int lv, PRBool raw)
{
    unsigned char lbyte;
    int lenLen;
    int rv;

    if (data >= end) {
	PR_SetError(SEC_ERROR_BAD_DER,0);
	return -1;
    }

    rv = PR_Write(out, " ",1);
    if (rv < 0) {
        PR_SetError(SEC_ERROR_IO,0);
	return rv;
    }

    *indefinitep = PR_FALSE;

    lbyte = *data++;
    if (lbyte >= 0x80) {
	/* Multibyte length */
	unsigned nb = (unsigned) (lbyte & 0x7f);
	if (nb > 4) {
	    PR_SetError(SEC_ERROR_BAD_DER,0);
	    return -1;
	}
	if (nb > 0) {
	    int il;

	    if ((data + nb) > end) {
		PR_SetError(SEC_ERROR_BAD_DER,0);
		return -1;
	    }
	    il = getInteger256(data, nb);
	    if (il < 0) return -1;
	    *lenp = (unsigned) il;
	} else {
	    *lenp = 0;
	    *indefinitep = PR_TRUE;
	}
	lenLen = nb + 1;
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
	lenLen = 1;
	if (raw) {
	    rv = prettyPrintByte(out, lbyte, lv);
	    if (rv < 0)
		return rv;
	}
    }
    if (*indefinitep) {
	rv = PR_Write(out, "(indefinite)\n", PL_strlen("(indefinite)\n"));
    } else {
        char *msg;

        msg= PR_smprintf("(%d)\n", *lenp);
	rv = PR_Write(out, msg, PL_strlen(msg));
    }
    if (rv < 0) {
        PR_SetError(SEC_ERROR_IO,0);
	return rv;
    }

    prettyColumn = -1;
    return lenLen;
}

static int
prettyPrintItem(PRFileDesc *out, unsigned char *data, unsigned char *end,
		unsigned int lv, PRBool raw)
{
    int slen;
    int lenLen;
    unsigned char *orig = data;
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
	if ((data + slen) > end) {
	    PR_SetError(SEC_ERROR_BAD_DER,0);
	    return -1;
	}

        if (code & SEC_ASN1_CONSTRUCTED) {
	    if (slen > 0 || indefinite) {
		slen = prettyPrintItem(out, data,
				       slen == 0 ? end : data + slen,
				       lv+1, raw);
		if (slen < 0)
		    return slen;
		data += slen;
	    }
	} else if (code == 0) {
	    if (slen != 0 || lenLen != 1) {
		PR_SetError(SEC_ERROR_BAD_DER,0);
		return -1;
	    }
	    break;
	} else {
	    switch (code) {
	      case SEC_ASN1_PRINTABLE_STRING:
	      case SEC_ASN1_IA5_STRING:
	      case SEC_ASN1_VISIBLE_STRING:
	      case SEC_ASN1_T61_STRING:
	      case SEC_ASN1_UTF8_STRING:
	        rv = prettyPrintString(out, data, slen, lv+1, raw);
		if (rv < 0)
		    return rv;
		break;
	      case SEC_ASN1_UTC_TIME:
	        rv = prettyPrintTime(out, data, slen, lv+1, raw, PR_TRUE);
		if (rv < 0)
		    return rv;
		break;
	      case SEC_ASN1_GENERALIZED_TIME:
	        rv = prettyPrintTime(out, data, slen, lv+1, raw, PR_FALSE);
		if (rv < 0)
		    return rv;
		break;
	      case SEC_ASN1_OBJECT_ID:
	        rv = prettyPrintObjectID(out, data, slen, lv+1, raw);
		if (rv < 0)
		    return rv;
		break;
	      case SEC_ASN1_OCTET_STRING:
		/* rv = prettyPrintOctetString(out,data,slen,lv+1); */
		rv = prettyPrintItem(out,data,data+slen,lv+1, raw);
		if (rv < 0)
		    return rv;
		break;
	      case SEC_ASN1_BOOLEAN:	/* could do nicer job */
	      case SEC_ASN1_INTEGER:	/* could do nicer job */
	      case SEC_ASN1_BIT_STRING:	/* could do nicer job */
	      case SEC_ASN1_NULL:
	      case SEC_ASN1_ENUMERATED:	/* could do nicer job, as INTEGER */
	      case SEC_ASN1_UNIVERSAL_STRING:
	      case SEC_ASN1_BMP_STRING:
	      default:
	        rv = prettyPrintLeaf(out, data, slen, lv+1);
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

SSMStatus
SSM_PrettyPrintDER(PRFileDesc *out, SECItem *it, PRBool raw)
{
    int rv;

    prettyColumn = -1;

    rv = prettyPrintItem(out, it->data, it->data + it->len, 0, raw);
    if (rv < 0)
	return SSM_FAILURE;
    return SSM_SUCCESS;
}
