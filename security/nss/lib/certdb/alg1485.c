/* alg1485.c - implementation of RFCs 1485, 1779 and 2253.
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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

#include "prprf.h"
#include "cert.h"
#include "xconst.h"
#include "genname.h"
#include "secitem.h"
#include "secerr.h"

/* for better RFC 2253 compliance. */
#define NSS_STRICT_RFC_2253_VALUES_ONLY 1

struct NameToKind {
    const char * name;
    unsigned int maxLen; /* max bytes in UTF8 encoded string value */
    SECOidTag    kind;
};

/* Add new entries to this table, and maybe to function CERT_ParseRFC1485AVA */
static const struct NameToKind name2kinds[] = {
/* keywords given in RFC 2253 */
    { "CN",                      64, SEC_OID_AVA_COMMON_NAME              },
    { "L",                      128, SEC_OID_AVA_LOCALITY                 },
    { "ST",                     128, SEC_OID_AVA_STATE_OR_PROVINCE        },
    { "O",                       64, SEC_OID_AVA_ORGANIZATION_NAME        },
    { "OU",                      64, SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME },
    { "C",                        2, SEC_OID_AVA_COUNTRY_NAME             },
    { "STREET",                 128, SEC_OID_AVA_STREET_ADDRESS           },
    { "DC",                     128, SEC_OID_AVA_DC                       },
    { "UID",                    256, SEC_OID_RFC1274_UID                  },

#ifndef NSS_STRICT_RFC_2253_KEYWORDS_ONLY
/* NSS legacy keywords */
    { "dnQualifier",          32767, SEC_OID_AVA_DN_QUALIFIER             },
    { "E",                      128, SEC_OID_PKCS9_EMAIL_ADDRESS          },
    { "MAIL",                   256, SEC_OID_RFC1274_MAIL                 },

#ifndef NSS_LEGACY_KEYWORDS_ONLY
/* values from draft-ietf-ldapbis-user-schema-05 */
    { "SN",                      64, SEC_OID_AVA_SURNAME                  },
    { "serialNumber",            64, SEC_OID_AVA_SERIAL_NUMBER            },
    { "title",                   64, SEC_OID_AVA_TITLE                    },
    { "postalAddress",          128, SEC_OID_AVA_POSTAL_ADDRESS           },
    { "postalCode",              40, SEC_OID_AVA_POSTAL_CODE              },
    { "postOfficeBox",           40, SEC_OID_AVA_POST_OFFICE_BOX          },
    { "givenName",               64, SEC_OID_AVA_GIVEN_NAME               },
    { "initials",                64, SEC_OID_AVA_INITIALS                 },
    { "generationQualifier",     64, SEC_OID_AVA_GENERATION_QUALIFIER     },
    { "houseIdentifier",         64, SEC_OID_AVA_HOUSE_IDENTIFIER         },
#if 0 /* removed.  Not yet in any IETF draft or RFC. */
    { "pseudonym",               64, SEC_OID_AVA_PSEUDONYM                },
#endif
#endif
#endif
    { 0,                        256, SEC_OID_UNKNOWN                      }
};

#define C_DOUBLE_QUOTE '\042'

#define C_BACKSLASH '\134'

#define C_EQUAL '='

#define OPTIONAL_SPACE(c) \
    (((c) == ' ') || ((c) == '\r') || ((c) == '\n'))

#define SPECIAL_CHAR(c)						\
    (((c) == ',') || ((c) == '=') || ((c) == C_DOUBLE_QUOTE) ||	\
     ((c) == '\r') || ((c) == '\n') || ((c) == '+') ||		\
     ((c) == '<') || ((c) == '>') || ((c) == '#') ||		\
     ((c) == ';') || ((c) == C_BACKSLASH))


#define IS_PRINTABLE(c)						\
    ((((c) >= 'a') && ((c) <= 'z')) ||				\
     (((c) >= 'A') && ((c) <= 'Z')) ||				\
     (((c) >= '0') && ((c) <= '9')) ||				\
     ((c) == ' ') ||						\
     ((c) == '\'') ||						\
     ((c) == '\050') ||				/* ( */		\
     ((c) == '\051') ||				/* ) */		\
     (((c) >= '+') && ((c) <= '/')) ||		/* + , - . / */	\
     ((c) == ':') ||						\
     ((c) == '=') ||						\
     ((c) == '?'))

int
cert_AVAOidTagToMaxLen(SECOidTag tag)
{
    const struct NameToKind *n2k = name2kinds;

    while (n2k->kind != tag && n2k->kind != SEC_OID_UNKNOWN) {
	++n2k;
    }
    return (n2k->kind != SEC_OID_UNKNOWN) ? n2k->maxLen : -1;
}

static PRBool
IsPrintable(unsigned char *data, unsigned len)
{
    unsigned char ch, *end;

    end = data + len;
    while (data < end) {
	ch = *data++;
	if (!IS_PRINTABLE(ch)) {
	    return PR_FALSE;
	}
    }
    return PR_TRUE;
}

static PRBool
Is7Bit(unsigned char *data, unsigned len)
{
    unsigned char ch, *end;

    end = data + len;
    while (data < end) {
        ch = *data++;
        if ((ch & 0x80)) {
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}

static void
skipSpace(char **pbp, char *endptr)
{
    char *bp = *pbp;
    while (bp < endptr && OPTIONAL_SPACE(*bp)) {
	bp++;
    }
    *pbp = bp;
}

static SECStatus
scanTag(char **pbp, char *endptr, char *tagBuf, int tagBufSize)
{
    char *bp, *tagBufp;
    int taglen;

    PORT_Assert(tagBufSize > 0);
    
    /* skip optional leading space */
    skipSpace(pbp, endptr);
    if (*pbp == endptr) {
	/* nothing left */
	return SECFailure;
    }
    
    /* fill tagBuf */
    taglen = 0;
    bp = *pbp;
    tagBufp = tagBuf;
    while (bp < endptr && !OPTIONAL_SPACE(*bp) && (*bp != C_EQUAL)) {
	if (++taglen >= tagBufSize) {
	    *pbp = bp;
	    return SECFailure;
	}
	*tagBufp++ = *bp++;
    }
    /* null-terminate tagBuf -- guaranteed at least one space left */
    *tagBufp++ = 0;
    *pbp = bp;
    
    /* skip trailing spaces till we hit something - should be an equal sign */
    skipSpace(pbp, endptr);
    if (*pbp == endptr) {
	/* nothing left */
	return SECFailure;
    }
    if (**pbp != C_EQUAL) {
	/* should be an equal sign */
	return SECFailure;
    }
    /* skip over the equal sign */
    (*pbp)++;
    
    return SECSuccess;
}

static SECStatus
scanVal(char **pbp, char *endptr, char *valBuf, int valBufSize)  
{
    char *bp, *valBufp;
    int vallen;
    PRBool isQuoted;
    
    PORT_Assert(valBufSize > 0);
    
    /* skip optional leading space */
    skipSpace(pbp, endptr);
    if(*pbp == endptr) {
	/* nothing left */
	return SECFailure;
    }
    
    bp = *pbp;
    
    /* quoted? */
    if (*bp == C_DOUBLE_QUOTE) {
	isQuoted = PR_TRUE;
	/* skip over it */
	bp++;
    } else {
	isQuoted = PR_FALSE;
    }
    
    valBufp = valBuf;
    vallen = 0;
    while (bp < endptr) {
	char c = *bp;
	if (c == C_BACKSLASH) {
	    /* escape character */
	    bp++;
	    if (bp >= endptr) {
		/* escape charater must appear with paired char */
		*pbp = bp;
		return SECFailure;
	    }
	} else if (!isQuoted && SPECIAL_CHAR(c)) {
	    /* unescaped special and not within quoted value */
	    break;
	} else if (c == C_DOUBLE_QUOTE) {
	    /* reached unescaped double quote */
	    break;
	}
	/* append character */
        vallen++;
	if (vallen >= valBufSize) {
	    *pbp = bp;
	    return SECFailure;
	}
	*valBufp++ = *bp++;
    }
    
    /* stip trailing spaces from unquoted values */
    if (!isQuoted) {
	if (valBufp > valBuf) {
	    valBufp--;
	    while ((valBufp > valBuf) && OPTIONAL_SPACE(*valBufp)) {
		valBufp--;
	    }
	    valBufp++;
	}
    }
    
    if (isQuoted) {
	/* insist that we stopped on a double quote */
	if (*bp != C_DOUBLE_QUOTE) {
	    *pbp = bp;
	    return SECFailure;
	}
	/* skip over the quote and skip optional space */
	bp++;
	skipSpace(&bp, endptr);
    }
    
    *pbp = bp;
    
    if (valBufp == valBuf) {
	/* empty value -- not allowed */
	return SECFailure;
    }
    
    /* null-terminate valBuf -- guaranteed at least one space left */
    *valBufp++ = 0;
    
    return SECSuccess;
}

CERTAVA *
CERT_ParseRFC1485AVA(PRArenaPool *arena, char **pbp, char *endptr,
		    PRBool singleAVA) 
{
    CERTAVA *a;
    const struct NameToKind *n2k;
    int vt;
    int valLen;
    char *bp;

    char tagBuf[32];
    char valBuf[384];

    if (scanTag(pbp, endptr, tagBuf, sizeof(tagBuf)) == SECFailure ||
	scanVal(pbp, endptr, valBuf, sizeof(valBuf)) == SECFailure) {
	PORT_SetError(SEC_ERROR_INVALID_AVA);
	return 0;
    }

    /* insist that if we haven't finished we've stopped on a separator */
    bp = *pbp;
    if (bp < endptr) {
	if (singleAVA || (*bp != ',' && *bp != ';')) {
	    PORT_SetError(SEC_ERROR_INVALID_AVA);
	    *pbp = bp;
	    return 0;
	}
	/* ok, skip over separator */
	bp++;
    }
    *pbp = bp;

    for (n2k = name2kinds; n2k->name; n2k++) {
	if (PORT_Strcasecmp(n2k->name, tagBuf) == 0) {
	    valLen = PORT_Strlen(valBuf);
	    if (n2k->kind == SEC_OID_AVA_COUNTRY_NAME) {
		vt = SEC_ASN1_PRINTABLE_STRING;
		if (valLen != 2) {
		    PORT_SetError(SEC_ERROR_INVALID_AVA);
		    return 0;
		}
		if (!IsPrintable((unsigned char*) valBuf, 2)) {
		    PORT_SetError(SEC_ERROR_INVALID_AVA);
		    return 0;
		}
	    } else if ((n2k->kind == SEC_OID_PKCS9_EMAIL_ADDRESS) ||
		       (n2k->kind == SEC_OID_RFC1274_MAIL)) {
		vt = SEC_ASN1_IA5_STRING;
	    } else {
		/* Hack -- for rationale see X.520 DirectoryString defn */
		if (IsPrintable((unsigned char*)valBuf, valLen)) {
		    vt = SEC_ASN1_PRINTABLE_STRING;
                } else if (Is7Bit((unsigned char *)valBuf, valLen)) {
                    vt = SEC_ASN1_T61_STRING;
		} else {
		    /* according to RFC3280, UTF8String is preferred encoding */
		    vt = SEC_ASN1_UTF8_STRING;
		}
	    }
	    a = CERT_CreateAVA(arena, n2k->kind, vt, (char *) valBuf);
	    return a;
	}
    }
    /* matched no kind -- invalid tag */
    PORT_SetError(SEC_ERROR_INVALID_AVA);
    return 0;
}

static CERTName *
ParseRFC1485Name(char *buf, int len)
{
    SECStatus rv;
    CERTName *name;
    char *bp, *e;
    CERTAVA *ava;
    CERTRDN *rdn;

    name = CERT_CreateName(NULL);
    if (name == NULL) {
	return NULL;
    }
    
    e = buf + len;
    bp = buf;
    while (bp < e) {
	ava = CERT_ParseRFC1485AVA(name->arena, &bp, e, PR_FALSE);
	if (ava == 0) goto loser;
	rdn = CERT_CreateRDN(name->arena, ava, 0);
	if (rdn == 0) goto loser;
	rv = CERT_AddRDN(name, rdn);
	if (rv) goto loser;
	skipSpace(&bp, e);
    }

    if (name->rdns[0] == 0) {
	/* empty name -- illegal */
	goto loser;
    }

    /* Reverse order of RDNS to comply with RFC */
    {
	CERTRDN **firstRdn;
	CERTRDN **lastRdn;
	CERTRDN *tmp;
	
	/* get first one */
	firstRdn = name->rdns;
	
	/* find last one */
	lastRdn = name->rdns;
	while (*lastRdn) lastRdn++;
	lastRdn--;
	
	/* reverse list */
	for ( ; firstRdn < lastRdn; firstRdn++, lastRdn--) {
	    tmp = *firstRdn;
	    *firstRdn = *lastRdn;
	    *lastRdn = tmp;
	}
    }
    
    /* return result */
    return name;
    
  loser:
    CERT_DestroyName(name);
    return NULL;
}

CERTName *
CERT_AsciiToName(char *string)
{
    CERTName *name;
    name = ParseRFC1485Name(string, PORT_Strlen(string));
    return name;
}

/************************************************************************/

typedef struct stringBufStr {
    char *buffer;
    unsigned offset;
    unsigned size;
} stringBuf;

#define DEFAULT_BUFFER_SIZE 200

static SECStatus
AppendStr(stringBuf *bufp, char *str)
{
    char *buf;
    unsigned bufLen, bufSize, len;
    int size = 0;

    /* Figure out how much to grow buf by (add in the '\0') */
    buf = bufp->buffer;
    bufLen = bufp->offset;
    len = PORT_Strlen(str);
    bufSize = bufLen + len;
    if (!buf) {
	bufSize++;
	size = PR_MAX(DEFAULT_BUFFER_SIZE,bufSize*2);
	buf = (char *) PORT_Alloc(size);
	bufp->size = size;
    } else if (bufp->size < bufSize) {
	size = bufSize*2;
	buf =(char *) PORT_Realloc(buf,size);
	bufp->size = size;
    }
    if (!buf) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    bufp->buffer = buf;
    bufp->offset = bufSize;

    /* Concatenate str onto buf */
    buf = buf + bufLen;
    if (bufLen) buf--;			/* stomp on old '\0' */
    PORT_Memcpy(buf, str, len+1);		/* put in new null */
    return SECSuccess;
}

SECStatus
CERT_RFC1485_EscapeAndQuote(char *dst, int dstlen, char *src, int srclen)
{
    int i, reqLen=0;
    char *d = dst;
    PRBool needsQuoting = PR_FALSE;
    char lastC = 0;
    
    /* need to make an initial pass to determine if quoting is needed */
    for (i = 0; i < srclen; i++) {
	char c = src[i];
	reqLen++;
	if (!needsQuoting && (SPECIAL_CHAR(c) ||
	    (OPTIONAL_SPACE(c) && OPTIONAL_SPACE(lastC)))) {
	    /* entirety will need quoting */
	    needsQuoting = PR_TRUE;
	}
	if (c == C_DOUBLE_QUOTE || c == C_BACKSLASH) {
	    /* this char will need escaping */
	    reqLen++;
	}
	lastC = c;
    }
    /* if it begins or ends in optional space it needs quoting */
    if (!needsQuoting && srclen > 0 && 
	(OPTIONAL_SPACE(src[srclen-1]) || OPTIONAL_SPACE(src[0]))) {
	needsQuoting = PR_TRUE;
    }
    
    if (needsQuoting) reqLen += 2;

    /* space for terminal null */
    reqLen++;
    
    if (reqLen > dstlen) {
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	return SECFailure;
    }
    
    d = dst;
    if (needsQuoting) *d++ = C_DOUBLE_QUOTE;
    for (i = 0; i < srclen; i++) {
	char c = src[i];
	if (c == C_DOUBLE_QUOTE || c == C_BACKSLASH) {
	    /* escape it */
	    *d++ = C_BACKSLASH;
	}
	*d++ = c;
    }
    if (needsQuoting) *d++ = C_DOUBLE_QUOTE;
    *d++ = 0;
    return SECSuccess;
}

/* convert an OID to dotted-decimal representation */
/* Returns a string that must be freed with PR_smprintf_free(), */
char *
CERT_GetOidString(const SECItem *oid)
{
    PRUint8 *end;
    PRUint8 *d;
    PRUint8 *e;
    char *a         = NULL;
    char *b;

#define MAX_OID_LEN 1024 /* bytes */

    if (oid->len > MAX_OID_LEN) {
    	PORT_SetError(SEC_ERROR_INPUT_LEN);
	return NULL;
    }

    /* d will point to the next sequence of bytes to decode */
    d = (PRUint8 *)oid->data;
    /* end points to one past the legitimate data */
    end = &d[ oid->len ];

    /*
     * Check for our pseudo-encoded single-digit OIDs
     */
    if( (*d == 0x80) && (2 == oid->len) ) {
	/* Funky encoding.  The second byte is the number */
	a = PR_smprintf("%lu", (PRUint32)d[1]);
	if( (char *)NULL == a ) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    return (char *)NULL;
	}
	return a;
    }

    for( ; d < end; d = &e[1] ) {
    
	for( e = d; e < end; e++ ) {
	    if( 0 == (*e & 0x80) ) {
		break;
	    }
	}
    
	if( ((e-d) > 4) || (((e-d) == 4) && (*d & 0x70)) ) {
	    /* More than a 32-bit number */
	} else {
	    PRUint32 n = 0;
      
	    switch( e-d ) {
	    case 4:
		n |= ((PRUint32)(e[-4] & 0x0f)) << 28;
	    case 3:
		n |= ((PRUint32)(e[-3] & 0x7f)) << 21;
	    case 2:
		n |= ((PRUint32)(e[-2] & 0x7f)) << 14;
	    case 1:
		n |= ((PRUint32)(e[-1] & 0x7f)) <<  7;
	    case 0:
		n |= ((PRUint32)(e[-0] & 0x7f))      ;
	    }
      
	    if( (char *)NULL == a ) {
		/* This is the first number.. decompose it */
		PRUint32 one = PR_MIN(n/40, 2); /* never > 2 */
		PRUint32 two = n - one * 40;
        
		a = PR_smprintf("OID.%lu.%lu", one, two);
		if( (char *)NULL == a ) {
		    PORT_SetError(SEC_ERROR_NO_MEMORY);
		    return (char *)NULL;
		}
	    } else {
		b = PR_smprintf("%s.%lu", a, n);
		if( (char *)NULL == b ) {
		    PR_smprintf_free(a);
		    PORT_SetError(SEC_ERROR_NO_MEMORY);
		    return (char *)NULL;
		}
        
		PR_smprintf_free(a);
		a = b;
	    }
	}
    }

    return a;
}

/* convert DER-encoded hex to a string */
static SECItem *
get_hex_string(SECItem *data)
{
    SECItem *rv;
    unsigned int i, j;
    static const char hex[] = { "0123456789ABCDEF" };

    /* '#' + 2 chars per octet + terminator */
    rv = SECITEM_AllocItem(NULL, NULL, data->len*2 + 2);
    if (!rv) {
	return NULL;
    }
    rv->data[0] = '#';
    rv->len = 1 + 2 * data->len;
    for (i=0; i<data->len; i++) {
	j = data->data[i];
	rv->data[2*i+1] = hex[j >> 4];
	rv->data[2*i+2] = hex[j & 15];
    }
    rv->data[rv->len] = 0;
    return rv;
}

static SECStatus
AppendAVA(stringBuf *bufp, CERTAVA *ava)
{
    const struct NameToKind *n2k = name2kinds;
    const char *tagName;
    unsigned len, maxLen;
    int tag;
    SECStatus rv;
    SECItem *avaValue = NULL;
    char *unknownTag = NULL;
    PRBool hexValue = PR_FALSE;
    char tmpBuf[384];

    tag = CERT_GetAVATag(ava);
    while (n2k->kind != tag && n2k->kind != SEC_OID_UNKNOWN) {
        ++n2k;
    }
    if (n2k->kind != SEC_OID_UNKNOWN) {
        tagName = n2k->name;
    } else {
	/* handle unknown attribute types per RFC 2253 */
	tagName = unknownTag = CERT_GetOidString(&ava->type);
	if (!tagName)
	    return SECFailure;
    }
    maxLen = n2k->maxLen;

#ifdef NSS_STRICT_RFC_2253_VALUES_ONLY
    if (!unknownTag)
#endif
    avaValue = CERT_DecodeAVAValue(&ava->value);
    if(!avaValue) {
	/* the attribute value is not recognized, get the hex value */
	avaValue = get_hex_string(&ava->value);
	if(!avaValue) {
	    if (unknownTag) PR_smprintf_free(unknownTag);
	    return SECFailure;
	}
	hexValue = PR_TRUE;
    }

    /* Check value length */
    if (avaValue->len > maxLen + 3) {  /* must be room for "..." */
	/* avaValue is a UTF8 string, freshly allocated and returned to us 
	** by CERT_DecodeAVAValue or get_hex_string just above, so we can
	** modify it here.  See if we're in the middle of a multi-byte
	** UTF8 character.
	*/
	while (((avaValue->data[maxLen] & 0xc0) == 0x80) && maxLen > 0) {
	   maxLen--;
	}
	/* add elipsis to signify truncation. */
	avaValue->data[maxLen++] = '.'; 
	avaValue->data[maxLen++] = '.';
	avaValue->data[maxLen++] = '.';
	avaValue->data[maxLen]   = 0;
	avaValue->len = maxLen;
    }

    len = PORT_Strlen(tagName);
    if (len+1 > sizeof(tmpBuf)) {
	if (unknownTag) PR_smprintf_free(unknownTag);
	SECITEM_FreeItem(avaValue, PR_TRUE);
	PORT_SetError(SEC_ERROR_OUTPUT_LEN);
	return SECFailure;
    }
    PORT_Memcpy(tmpBuf, tagName, len);
    if (unknownTag) PR_smprintf_free(unknownTag);
    tmpBuf[len++] = '=';
    
    /* escape and quote as necessary - don't quote hex strings */
    if (hexValue) {
        /* appent avaValue to tmpBuf */
	if (avaValue->len + len + 1 > sizeof tmpBuf) {
	    PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	    rv = SECFailure;
    	} else {
	    PORT_Strncpy(tmpBuf+len, (char *)avaValue->data, avaValue->len + 1);
	    rv = SECSuccess;
	}
    } else 
	rv = CERT_RFC1485_EscapeAndQuote(tmpBuf+len, sizeof(tmpBuf)-len, 
		    		     (char *)avaValue->data, avaValue->len);
    SECITEM_FreeItem(avaValue, PR_TRUE);
    if (rv) return SECFailure;
    
    rv = AppendStr(bufp, tmpBuf);
    return rv;
}

char *
CERT_NameToAscii(CERTName *name)
{
    CERTRDN** rdns;
    CERTRDN** lastRdn;
    CERTRDN** rdn;
    PRBool first = PR_TRUE;
    stringBuf strBuf = { NULL, 0, 0 };
    
    rdns = name->rdns;
    if (rdns == NULL) {
	return NULL;
    }
    
    /* find last RDN */
    lastRdn = rdns;
    while (*lastRdn) lastRdn++;
    lastRdn--;
    
    /*
     * Loop over name contents in _reverse_ RDN order appending to string
     */
    for (rdn = lastRdn; rdn >= rdns; rdn--) {
	CERTAVA** avas = (*rdn)->avas;
	CERTAVA* ava;
	PRBool newRDN = PR_TRUE;

	/* 
	 * XXX Do we need to traverse the AVAs in reverse order, too?
	 */
	while (avas && (ava = *avas++) != NULL) {
	    SECStatus rv;
	    /* Put in comma or plus separator */
	    if (!first) {
		/* Use of spaces is deprecated in RFC 2253. */
		rv = AppendStr(&strBuf, newRDN ? "," : "+");
		if (rv) goto loser;
	    } else {
		first = PR_FALSE;
	    }
	    
	    /* Add in tag type plus value into buf */
	    rv = AppendAVA(&strBuf, ava);
	    if (rv) goto loser;
	    newRDN = PR_FALSE;
	}
    }
    return strBuf.buffer;
loser:
    if (strBuf.buffer) {
	PORT_Free(strBuf.buffer);
    }
    return NULL;
}

/*
 * Return the string representation of a DER encoded distinguished name
 * "dername" - The DER encoded name to convert
 */
char *
CERT_DerNameToAscii(SECItem *dername)
{
    int rv;
    PRArenaPool *arena = NULL;
    CERTName name;
    char *retstr = NULL;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    
    if ( arena == NULL) {
	goto loser;
    }
    
    rv = SEC_QuickDERDecodeItem(arena, &name, CERT_NameTemplate, dername);
    
    if ( rv != SECSuccess ) {
	goto loser;
    }

    retstr = CERT_NameToAscii(&name);

loser:
    if ( arena != NULL ) {
	PORT_FreeArena(arena, PR_FALSE);
    }
    
    return(retstr);
}

/* RDNs are sorted from most general to most specific.
 * This code returns the FIRST one found, the most general one found.
 */
static char *
CERT_GetNameElement(PRArenaPool *arena, CERTName *name, int wantedTag)
{
    CERTRDN** rdns;
    CERTRDN *rdn;
    char *buf = 0;
    
    rdns = name->rdns;
    while (rdns && (rdn = *rdns++) != 0) {
	CERTAVA** avas = rdn->avas;
	CERTAVA*  ava;
	while (avas && (ava = *avas++) != 0) {
	    int tag = CERT_GetAVATag(ava);
	    if ( tag == wantedTag ) {
		SECItem *decodeItem = CERT_DecodeAVAValue(&ava->value);
		if(!decodeItem) {
		    return NULL;
		}
		if (arena) {
		    buf = (char *)PORT_ArenaZAlloc(arena,decodeItem->len + 1);
		} else {
		    buf = (char *)PORT_ZAlloc(decodeItem->len + 1);
		}
		if ( buf ) {
		    PORT_Memcpy(buf, decodeItem->data, decodeItem->len);
		    buf[decodeItem->len] = 0;
		}
		SECITEM_FreeItem(decodeItem, PR_TRUE);
		goto done;
	    }
	}
    }
    
  done:
    return buf;
}

/* RDNs are sorted from most general to most specific.
 * This code returns the LAST one found, the most specific one found.
 * This is particularly appropriate for Common Name.  See RFC 2818.
 */
static char *
CERT_GetLastNameElement(PRArenaPool *arena, CERTName *name, int wantedTag)
{
    CERTRDN** rdns;
    CERTRDN *rdn;
    CERTAVA * lastAva = NULL;
    char *buf = 0;
    
    rdns = name->rdns;
    while (rdns && (rdn = *rdns++) != 0) {
	CERTAVA** avas = rdn->avas;
	CERTAVA*  ava;
	while (avas && (ava = *avas++) != 0) {
	    int tag = CERT_GetAVATag(ava);
	    if ( tag == wantedTag ) {
		lastAva = ava;
	    }
	}
    }

    if (lastAva) {
	SECItem *decodeItem = CERT_DecodeAVAValue(&lastAva->value);
	if(!decodeItem) {
	    return NULL;
	}
	if (arena) {
	    buf = (char *)PORT_ArenaZAlloc(arena,decodeItem->len + 1);
	} else {
	    buf = (char *)PORT_ZAlloc(decodeItem->len + 1);
	}
	if ( buf ) {
	    PORT_Memcpy(buf, decodeItem->data, decodeItem->len);
	    buf[decodeItem->len] = 0;
	}
	SECITEM_FreeItem(decodeItem, PR_TRUE);
    }    
    return buf;
}

char *
CERT_GetCertificateEmailAddress(CERTCertificate *cert)
{
    char *rawEmailAddr = NULL;
    SECItem subAltName;
    SECStatus rv;
    CERTGeneralName *nameList = NULL;
    CERTGeneralName *current;
    PRArenaPool *arena = NULL;
    int i;
    
    subAltName.data = NULL;

    rawEmailAddr = CERT_GetNameElement(cert->arena, &(cert->subject),
						 SEC_OID_PKCS9_EMAIL_ADDRESS);
    if ( rawEmailAddr == NULL ) {
	rawEmailAddr = CERT_GetNameElement(cert->arena, &(cert->subject), 
							SEC_OID_RFC1274_MAIL);
    }
    if ( rawEmailAddr == NULL) {

	rv = CERT_FindCertExtension(cert,  SEC_OID_X509_SUBJECT_ALT_NAME, 
								&subAltName);
	if (rv != SECSuccess) {
	    goto finish;
	}
	arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
	if (!arena) {
	    goto finish;
	}
	nameList = current = CERT_DecodeAltNameExtension(arena, &subAltName);
	if (!nameList ) {
	    goto finish;
	}
	if (nameList != NULL) {
	    do {
		if (current->type == certDirectoryName) {
		    rawEmailAddr = CERT_GetNameElement(cert->arena,
			&(current->name.directoryName), 
					       SEC_OID_PKCS9_EMAIL_ADDRESS);
		    if ( rawEmailAddr == NULL ) {
			rawEmailAddr = CERT_GetNameElement(cert->arena,
			  &(current->name.directoryName), SEC_OID_RFC1274_MAIL);
		    }
		} else if (current->type == certRFC822Name) {
		    rawEmailAddr = (char*)PORT_ArenaZAlloc(cert->arena,
						current->name.other.len + 1);
		    if (!rawEmailAddr) {
			goto finish;
		    }
		    PORT_Memcpy(rawEmailAddr, current->name.other.data, 
				current->name.other.len);
		    rawEmailAddr[current->name.other.len] = '\0';
		}
		if (rawEmailAddr) {
		    break;
		}
		current = CERT_GetNextGeneralName(current);
	    } while (current != nameList);
	}
    }
    if (rawEmailAddr) {
	for (i = 0; i <= (int) PORT_Strlen(rawEmailAddr); i++) {
	    rawEmailAddr[i] = tolower(rawEmailAddr[i]);
	}
    } 

finish:

    /* Don't free nameList, it's part of the arena. */

    if (arena) {
	PORT_FreeArena(arena, PR_FALSE);
    }

    if ( subAltName.data ) {
	SECITEM_FreeItem(&subAltName, PR_FALSE);
    }

    return(rawEmailAddr);
}

static char *
appendStringToBuf(char *dest, char *src, PRUint32 *pRemaining)
{
    PRUint32 len;
    if (dest && src && src[0] && *pRemaining > (len = PL_strlen(src))) {
	PRUint32 i;
	for (i = 0; i < len; ++i)
	    dest[i] = tolower(src[i]);
	dest[len] = 0;
	dest        += len + 1;
	*pRemaining -= len + 1;
    }
    return dest;
}

static char *
appendItemToBuf(char *dest, SECItem *src, PRUint32 *pRemaining)
{
    if (dest && src && src->data && src->len && src->data[0] && 
        *pRemaining > src->len + 1 ) {
	PRUint32 len = src->len;
	PRUint32 i;
	for (i = 0; i < len && src->data[i] ; ++i)
	    dest[i] = tolower(src->data[i]);
	dest[len] = 0;
	dest        += len + 1;
	*pRemaining -= len + 1;
    }
    return dest;
}

/* Returns a pointer to an environment-like string, a series of 
** null-terminated strings, terminated by a zero-length string.
** This function is intended to be internal to NSS.
*/
char *
cert_GetCertificateEmailAddresses(CERTCertificate *cert)
{
    char *           rawEmailAddr = NULL;
    char *           addrBuf      = NULL;
    char *           pBuf         = NULL;
    PRArenaPool *    tmpArena     = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    PRUint32         maxLen       = 0;
    PRInt32          finalLen     = 0;
    SECStatus        rv;
    SECItem          subAltName;
    
    if (!tmpArena) 
    	return addrBuf;

    subAltName.data = NULL;
    maxLen = cert->derCert.len;
    PORT_Assert(maxLen);
    if (!maxLen) 
	maxLen = 2000;  /* a guess, should never happen */

    pBuf = addrBuf = (char *)PORT_ArenaZAlloc(tmpArena, maxLen + 1);
    if (!addrBuf) 
    	goto loser;

    rawEmailAddr = CERT_GetNameElement(tmpArena, &cert->subject,
				       SEC_OID_PKCS9_EMAIL_ADDRESS);
    pBuf = appendStringToBuf(pBuf, rawEmailAddr, &maxLen);

    rawEmailAddr = CERT_GetNameElement(tmpArena, &cert->subject, 
				       SEC_OID_RFC1274_MAIL);
    pBuf = appendStringToBuf(pBuf, rawEmailAddr, &maxLen);

    rv = CERT_FindCertExtension(cert,  SEC_OID_X509_SUBJECT_ALT_NAME, 
				&subAltName);
    if (rv == SECSuccess && subAltName.data) {
	CERTGeneralName *nameList     = NULL;

	if (!!(nameList = CERT_DecodeAltNameExtension(tmpArena, &subAltName))) {
	    CERTGeneralName *current = nameList;
	    do {
		if (current->type == certDirectoryName) {
		    rawEmailAddr = CERT_GetNameElement(tmpArena,
			                       &current->name.directoryName, 
					       SEC_OID_PKCS9_EMAIL_ADDRESS);
		    pBuf = appendStringToBuf(pBuf, rawEmailAddr, &maxLen);

		    rawEmailAddr = CERT_GetNameElement(tmpArena,
					      &current->name.directoryName, 
					      SEC_OID_RFC1274_MAIL);
		    pBuf = appendStringToBuf(pBuf, rawEmailAddr, &maxLen);
		} else if (current->type == certRFC822Name) {
		    pBuf = appendItemToBuf(pBuf, &current->name.other, &maxLen);
		}
		current = CERT_GetNextGeneralName(current);
	    } while (current != nameList);
	}
	SECITEM_FreeItem(&subAltName, PR_FALSE);
	/* Don't free nameList, it's part of the tmpArena. */
    }
    /* now copy superstring to cert's arena */
    finalLen = (pBuf - addrBuf) + 1;
    pBuf = NULL;
    if (finalLen > 1) {
	pBuf = PORT_ArenaAlloc(cert->arena, finalLen);
	if (pBuf) {
	    PORT_Memcpy(pBuf, addrBuf, finalLen);
	}
    }
loser:
    if (tmpArena)
	PORT_FreeArena(tmpArena, PR_FALSE);

    return pBuf;
}

/* returns pointer to storage in cert's arena.  Storage remains valid
** as long as cert's reference count doesn't go to zero.
** Caller should strdup or otherwise copy.
*/
const char *	/* const so caller won't muck with it. */
CERT_GetFirstEmailAddress(CERTCertificate * cert)
{
    if (cert && cert->emailAddr && cert->emailAddr[0])
    	return (const char *)cert->emailAddr;
    return NULL;
}

/* returns pointer to storage in cert's arena.  Storage remains valid
** as long as cert's reference count doesn't go to zero.
** Caller should strdup or otherwise copy.
*/
const char *	/* const so caller won't muck with it. */
CERT_GetNextEmailAddress(CERTCertificate * cert, const char * prev)
{
    if (cert && prev && prev[0]) {
    	PRUint32 len = PL_strlen(prev);
	prev += len + 1;
	if (prev && prev[0])
	    return prev;
    }
    return NULL;
}

/* This is seriously bogus, now that certs store their email addresses in
** subject Alternative Name extensions. 
** Returns a string allocated by PORT_StrDup, which the caller must free.
*/
char *
CERT_GetCertEmailAddress(CERTName *name)
{
    char *rawEmailAddr;
    char *emailAddr;

    
    rawEmailAddr = CERT_GetNameElement(NULL, name, SEC_OID_PKCS9_EMAIL_ADDRESS);
    if ( rawEmailAddr == NULL ) {
	rawEmailAddr = CERT_GetNameElement(NULL, name, SEC_OID_RFC1274_MAIL);
    }
    emailAddr = CERT_FixupEmailAddr(rawEmailAddr);
    if ( rawEmailAddr ) {
	PORT_Free(rawEmailAddr);
    }
    return(emailAddr);
}

/* The return value must be freed with PORT_Free. */
char *
CERT_GetCommonName(CERTName *name)
{
    return(CERT_GetLastNameElement(NULL, name, SEC_OID_AVA_COMMON_NAME));
}

char *
CERT_GetCountryName(CERTName *name)
{
    return(CERT_GetNameElement(NULL, name, SEC_OID_AVA_COUNTRY_NAME));
}

char *
CERT_GetLocalityName(CERTName *name)
{
    return(CERT_GetNameElement(NULL, name, SEC_OID_AVA_LOCALITY));
}

char *
CERT_GetStateName(CERTName *name)
{
    return(CERT_GetNameElement(NULL, name, SEC_OID_AVA_STATE_OR_PROVINCE));
}

char *
CERT_GetOrgName(CERTName *name)
{
    return(CERT_GetNameElement(NULL, name, SEC_OID_AVA_ORGANIZATION_NAME));
}

char *
CERT_GetDomainComponentName(CERTName *name)
{
    return(CERT_GetNameElement(NULL, name, SEC_OID_AVA_DC));
}

char *
CERT_GetOrgUnitName(CERTName *name)
{
    return(CERT_GetNameElement(NULL, name, SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME));
}

char *
CERT_GetDnQualifier(CERTName *name)
{
    return(CERT_GetNameElement(NULL, name, SEC_OID_AVA_DN_QUALIFIER));
}

char *
CERT_GetCertUid(CERTName *name)
{
    return(CERT_GetNameElement(NULL, name, SEC_OID_RFC1274_UID));
}

