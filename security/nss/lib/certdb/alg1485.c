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

#include "cert.h"
#include "xconst.h"
#include "genname.h"
#include "secitem.h"
#include "secerr.h"

struct NameToKind {
    char *name;
    SECOidTag kind;
};

static struct NameToKind name2kinds[] = {
    { "CN",   		SEC_OID_AVA_COMMON_NAME, 		},
    { "ST",   		SEC_OID_AVA_STATE_OR_PROVINCE, 		},
    { "OU",   		SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME,	},
    { "DC",   		SEC_OID_AVA_DC,	},
    { "C",    		SEC_OID_AVA_COUNTRY_NAME, 		},
    { "O",    		SEC_OID_AVA_ORGANIZATION_NAME, 		},
    { "L",    		SEC_OID_AVA_LOCALITY, 			},
    { "dnQualifier",	SEC_OID_AVA_DN_QUALIFIER, 		},
    { "E",    		SEC_OID_PKCS9_EMAIL_ADDRESS, 		},
    { "UID",  		SEC_OID_RFC1274_UID, 			},
    { "MAIL", 		SEC_OID_RFC1274_MAIL, 			},
    { 0,      		SEC_OID_UNKNOWN 			},
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







#if 0
/*
** Find the start and end of a <string>. Strings can be wrapped in double
** quotes to protect special characters.
*/
static int BracketThing(char **startp, char *end, char *result)
{
    char *start = *startp;
    char c;

    /* Skip leading white space */
    while (start < end) {
	c = *start++;
	if (!OPTIONAL_SPACE(c)) {
	    start--;
	    break;
	}
    }
    if (start == end) return 0;

    switch (*start) {
      case '#':
	/* Process hex thing */
	start++;
	*startp = start;
	while (start < end) {
	    c = *start++;
	    if (((c >= '0') && (c <= '9')) ||
		((c >= 'a') && (c <= 'f')) ||
		((c >= 'A') && (c <= 'F'))) {
		continue;
	    }
	    break;
	}
	rv = IS_HEX;
	break;

      case C_DOUBLE_QUOTE:
	start++;
	*startp = start;
	while (start < end) {
	    c = *start++;
	    if (c == C_DOUBLE_QUOTE) {
		break;
	    }
	    *result++ = c;
	}
	rv = IS_STRING;
	break;

      default:
	while (start < end) {
	    c = *start++;
	    if (SPECIAL_CHAR(c)) {
		start--;
		break;
	    }
	    *result++ = c;
	}
	rv = IS_STRING;
	break;
    }

    /* Terminate result string */
    *result = 0;
    return start;
}

static char *BracketSomething(char **startp, char* end, int spacesOK)
{
    char *start = *startp;
    char c;
    int stopAtDQ;

    /* Skip leading white space */
    while (start < end) {
	c = *start;
	if (!OPTIONAL_SPACE(c)) {
	    break;
	}
	start++;
    }
    if (start == end) return 0;
    stopAtDQ = 0;
    if (*start == C_DOUBLE_QUOTE) {
	stopAtDQ = 1;
    }

    /*
    ** Find the end of the something. The something is terminated most of
    ** the time by a space. However, if spacesOK is true then it is
    ** terminated by a special character only.
    */
    *startp = start;
    while (start < end) {
	c = *start;
	if (stopAtDQ) {
	    if (c == C_DOUBLE_QUOTE) {
		*start = ' ';
		break;
	    }
	} else {
	if (SPECIAL_CHAR(c)) {
	    break;
	}
	if (!spacesOK && OPTIONAL_SPACE(c)) {
	    break;
	}
	}
	start++;
    }
    return start;
}
#endif

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
    struct NameToKind *n2k;
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
		    vt = SEC_ASN1_UNIVERSAL_STRING;
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

static SECStatus
AppendStr(char **bufp, unsigned *buflenp, char *str)
{
    char *buf;
    unsigned bufLen, bufSize, len;

    /* Figure out how much to grow buf by (add in the '\0') */
    buf = *bufp;
    bufLen = *buflenp;
    len = PORT_Strlen(str);
    bufSize = bufLen + len;
    if (buf) {
	buf = (char*) PORT_Realloc(buf, bufSize);
    } else {
	bufSize++;
	buf = (char*) PORT_Alloc(bufSize);
    }
    if (!buf) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }
    *bufp = buf;
    *buflenp = bufSize;

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
    
    /* need to make an initial pass to determine if quoting is needed */
    for (i = 0; i < srclen; i++) {
	char c = src[i];
	reqLen++;
	if (SPECIAL_CHAR(c)) {
	    /* entirety will need quoting */
	    needsQuoting = PR_TRUE;
	}
	if (c == C_DOUBLE_QUOTE || c == C_BACKSLASH) {
	    /* this char will need escaping */
	    reqLen++;
	}
    }
    /* if it begins or ends in optional space it needs quoting */
    if (srclen > 0 &&
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

static SECStatus
AppendAVA(char **bufp, unsigned *buflenp, CERTAVA *ava)
{
    char *tagName;
    char tmpBuf[384];
    unsigned len, maxLen;
    int lenLen;
    int tag;
    SECStatus rv;
    SECItem *avaValue = NULL;

    tag = CERT_GetAVATag(ava);
    switch (tag) {
      case SEC_OID_AVA_COUNTRY_NAME:
	tagName = "C";
	maxLen = 2;
	break;
      case SEC_OID_AVA_ORGANIZATION_NAME:
	tagName = "O";
	maxLen = 64;
	break;
      case SEC_OID_AVA_COMMON_NAME:
	tagName = "CN";
	maxLen = 64;
	break;
      case SEC_OID_AVA_LOCALITY:
	tagName = "L";
	maxLen = 128;
	break;
      case SEC_OID_AVA_STATE_OR_PROVINCE:
	tagName = "ST";
	maxLen = 128;
	break;
      case SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME:
	tagName = "OU";
	maxLen = 64;
	break;
      case SEC_OID_AVA_DC:
	tagName = "DC";
	maxLen = 128;
	break;
      case SEC_OID_AVA_DN_QUALIFIER:
	tagName = "dnQualifier";
	maxLen = 0x7fff;
	break;
      case SEC_OID_PKCS9_EMAIL_ADDRESS:
	tagName = "E";
	maxLen = 128;
	break;
      case SEC_OID_RFC1274_UID:
	tagName = "UID";
	maxLen = 256;
	break;
      case SEC_OID_RFC1274_MAIL:
	tagName = "MAIL";
	maxLen = 256;
	break;
      default:
#if 0
	PORT_SetError(SEC_ERROR_INVALID_AVA);
	return SECFailure;
#else
	rv = AppendStr(bufp, buflenp, "ERR=Unknown AVA");
	return rv;
#endif
    }

    avaValue = CERT_DecodeAVAValue(&ava->value);
    if(!avaValue) {
	return SECFailure;
    }

    /* Check value length */
    if (avaValue->len > maxLen) {
	PORT_SetError(SEC_ERROR_INVALID_AVA);
	return SECFailure;
    }

    len = PORT_Strlen(tagName);
    PORT_Memcpy(tmpBuf, tagName, len);
    tmpBuf[len++] = '=';
    
    /* escape and quote as necessary */
    rv = CERT_RFC1485_EscapeAndQuote(tmpBuf+len, sizeof(tmpBuf)-len, 
		    		     (char *)avaValue->data, avaValue->len);
    SECITEM_FreeItem(avaValue, PR_TRUE);
    if (rv) return SECFailure;
    
    rv = AppendStr(bufp, buflenp, tmpBuf);
    return rv;
}

char *
CERT_NameToAscii(CERTName *name)
{
    SECStatus rv;
    CERTRDN** rdns;
    CERTRDN** lastRdn;
    CERTRDN** rdn;
    CERTAVA** avas;
    CERTAVA* ava;
    PRBool first = PR_TRUE;
    char *buf = NULL;
    unsigned buflen = 0;
    
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
	avas = (*rdn)->avas;
	while ((ava = *avas++) != NULL) {
	    /* Put in comma separator */
	    if (!first) {
		rv = AppendStr(&buf, &buflen, ", ");
		if (rv) goto loser;
	    } else {
		first = PR_FALSE;
	    }
	    
	    /* Add in tag type plus value into buf */
	    rv = AppendAVA(&buf, &buflen, ava);
	    if (rv) goto loser;
	}
    }
    return buf;
  loser:
    PORT_Free(buf);
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
    
    rv = SEC_ASN1DecodeItem(arena, &name, CERT_NameTemplate, dername);
    
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

static char *
CERT_GetNameElement(CERTName *name, int wantedTag)
{
    CERTRDN** rdns;
    CERTRDN *rdn;
    CERTAVA** avas;
    CERTAVA* ava;
    char *buf = 0;
    int tag;
    SECItem *decodeItem = NULL;
    
    rdns = name->rdns;
    while ((rdn = *rdns++) != 0) {
	avas = rdn->avas;
	while ((ava = *avas++) != 0) {
	    tag = CERT_GetAVATag(ava);
	    if ( tag == wantedTag ) {
		decodeItem = CERT_DecodeAVAValue(&ava->value);
		if(!decodeItem) {
		    return NULL;
		}
		buf = (char *)PORT_ZAlloc(decodeItem->len + 1);
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

char *
CERT_GetCertificateEmailAddress(CERTCertificate *cert)
{
    char *rawEmailAddr = NULL;
    char *emailAddr = NULL;
    SECItem subAltName;
    SECStatus rv;
    CERTGeneralName *nameList = NULL;
    CERTGeneralName *current;
    PRArenaPool *arena = NULL;
    int i;
    
    subAltName.data = NULL;

    rawEmailAddr = CERT_GetNameElement(&(cert->subject), SEC_OID_PKCS9_EMAIL_ADDRESS);
    if ( rawEmailAddr == NULL ) {
	rawEmailAddr = CERT_GetNameElement(&(cert->subject), SEC_OID_RFC1274_MAIL);
    }
    if ( rawEmailAddr == NULL) {

	rv = CERT_FindCertExtension(cert,  SEC_OID_X509_SUBJECT_ALT_NAME, &subAltName);
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
		    rawEmailAddr = CERT_GetNameElement(&(current->name.directoryName), 
						       SEC_OID_PKCS9_EMAIL_ADDRESS);
		    if ( rawEmailAddr == NULL ) {
			rawEmailAddr = CERT_GetNameElement(&(current->name.directoryName), 
							   SEC_OID_RFC1274_MAIL);
		    }
		} else if (current->type == certRFC822Name) {
		    rawEmailAddr = (char*)PORT_ZAlloc(current->name.other.len + 1);
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
		current = cert_get_next_general_name(current);
	    } while (current != nameList);
	}
    }
    if (rawEmailAddr) {
	emailAddr = (char*)PORT_ArenaZAlloc(cert->arena, PORT_Strlen(rawEmailAddr) + 1);
	for (i = 0; i <= PORT_Strlen(rawEmailAddr); i++) {
	    emailAddr[i] = tolower(rawEmailAddr[i]);
	}
    } else {
	emailAddr = NULL;
    }

finish:
    if ( rawEmailAddr ) {
	PORT_Free(rawEmailAddr);
    }

    /* Don't free nameList, it's part of the arena. */

    if (arena) {
	PORT_FreeArena(arena, PR_FALSE);
    }

    if ( subAltName.data ) {
	SECITEM_FreeItem(&subAltName, PR_FALSE);
    }

    return(emailAddr);
}

char *
CERT_GetCertEmailAddress(CERTName *name)
{
    char *rawEmailAddr;
    char *emailAddr;

    
    rawEmailAddr = CERT_GetNameElement(name, SEC_OID_PKCS9_EMAIL_ADDRESS);
    if ( rawEmailAddr == NULL ) {
	rawEmailAddr = CERT_GetNameElement(name, SEC_OID_RFC1274_MAIL);
    }
    emailAddr = CERT_FixupEmailAddr(rawEmailAddr);
    if ( rawEmailAddr ) {
	PORT_Free(rawEmailAddr);
    }
    return(emailAddr);
}

char *
CERT_GetCommonName(CERTName *name)
{
    return(CERT_GetNameElement(name, SEC_OID_AVA_COMMON_NAME));
}

char *
CERT_GetCountryName(CERTName *name)
{
    return(CERT_GetNameElement(name, SEC_OID_AVA_COUNTRY_NAME));
}

char *
CERT_GetLocalityName(CERTName *name)
{
    return(CERT_GetNameElement(name, SEC_OID_AVA_LOCALITY));
}

char *
CERT_GetStateName(CERTName *name)
{
    return(CERT_GetNameElement(name, SEC_OID_AVA_STATE_OR_PROVINCE));
}

char *
CERT_GetOrgName(CERTName *name)
{
    return(CERT_GetNameElement(name, SEC_OID_AVA_ORGANIZATION_NAME));
}

char *
CERT_GetDomainComponentName(CERTName *name)
{
    return(CERT_GetNameElement(name, SEC_OID_AVA_DC));
}

char *
CERT_GetOrgUnitName(CERTName *name)
{
    return(CERT_GetNameElement(name, SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME));
}

char *
CERT_GetDnQualifier(CERTName *name)
{
    return(CERT_GetNameElement(name, SEC_OID_AVA_DN_QUALIFIER));
}

char *
CERT_GetCertUid(CERTName *name)
{
    return(CERT_GetNameElement(name, SEC_OID_RFC1274_UID));
}

