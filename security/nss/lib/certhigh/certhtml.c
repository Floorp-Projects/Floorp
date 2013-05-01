/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * certhtml.c --- convert a cert to html
 *
 * $Id$
 */

#include "seccomon.h"
#include "secitem.h"
#include "sechash.h"
#include "cert.h"
#include "keyhi.h"
#include "secder.h"
#include "prprf.h"
#include "secport.h"
#include "secasn1.h"
#include "pk11func.h"

static char *hex = "0123456789ABCDEF";

/*
** Convert a der-encoded integer to a hex printable string form
*/
char *CERT_Hexify (SECItem *i, int do_colon)
{
    unsigned char *cp, *end;
    char *rv, *o;

    if (!i->len) {
	return PORT_Strdup("00");
    }

    rv = o = (char*) PORT_Alloc(i->len * 3);
    if (!rv) return rv;

    cp = i->data;
    end = cp + i->len;
    while (cp < end) {
	unsigned char ch = *cp++;
	*o++ = hex[(ch >> 4) & 0xf];
	*o++ = hex[ch & 0xf];
	if (cp != end) {
	    if (do_colon) {
		*o++ = ':';
	    }
	} 
    }
    *o = 0;           /* Null terminate the string */
    return rv;
}

#define BREAK "<br>"
#define BREAKLEN 4
#define COMMA ", "
#define COMMALEN 2

#define MAX_OUS 20
#define MAX_DC MAX_OUS


char *CERT_FormatName (CERTName *name)
{
    CERTRDN** rdns;
    CERTRDN * rdn;
    CERTAVA** avas;
    CERTAVA*  ava;
    char *    buf	= 0;
    char *    tmpbuf	= 0;
    SECItem * cn	= 0;
    SECItem * email	= 0;
    SECItem * org	= 0;
    SECItem * loc	= 0;
    SECItem * state	= 0;
    SECItem * country	= 0;
    SECItem * dq     	= 0;

    unsigned  len 	= 0;
    int       tag;
    int       i;
    int       ou_count = 0;
    int       dc_count = 0;
    PRBool    first;
    SECItem * orgunit[MAX_OUS];
    SECItem * dc[MAX_DC];

    /* Loop over name components and gather the interesting ones */
    rdns = name->rdns;
    while ((rdn = *rdns++) != 0) {
	avas = rdn->avas;
	while ((ava = *avas++) != 0) {
	    tag = CERT_GetAVATag(ava);
	    switch(tag) {
	      case SEC_OID_AVA_COMMON_NAME:
		if (cn) {
			break;
		}
		cn = CERT_DecodeAVAValue(&ava->value);
		if (!cn) {
 			goto loser;
		}
		len += cn->len;
		break;
	      case SEC_OID_AVA_COUNTRY_NAME:
		if (country) {
			break;
		}
		country = CERT_DecodeAVAValue(&ava->value);
		if (!country) {
 			goto loser;
		}
		len += country->len;
		break;
	      case SEC_OID_AVA_LOCALITY:
		if (loc) {
			break;
		}
		loc = CERT_DecodeAVAValue(&ava->value);
		if (!loc) {
 			goto loser;
		}
		len += loc->len;
		break;
	      case SEC_OID_AVA_STATE_OR_PROVINCE:
		if (state) {
			break;
		}
		state = CERT_DecodeAVAValue(&ava->value);
		if (!state) {
 			goto loser;
		}
		len += state->len;
		break;
	      case SEC_OID_AVA_ORGANIZATION_NAME:
		if (org) {
			break;
		}
		org = CERT_DecodeAVAValue(&ava->value);
		if (!org) {
 			goto loser;
		}
		len += org->len;
		break;
	      case SEC_OID_AVA_DN_QUALIFIER:
		if (dq) {
			break;
		}
		dq = CERT_DecodeAVAValue(&ava->value);
		if (!dq) {
 			goto loser;
		}
		len += dq->len;
		break;
	      case SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME:
		if (ou_count < MAX_OUS) {
			orgunit[ou_count] = CERT_DecodeAVAValue(&ava->value);
			if (!orgunit[ou_count]) {
				goto loser;
                        }
			len += orgunit[ou_count++]->len;
		}
		break;
	      case SEC_OID_AVA_DC:
		if (dc_count < MAX_DC) {
			dc[dc_count] = CERT_DecodeAVAValue(&ava->value);
			if (!dc[dc_count]) {
				goto loser;
			}
			len += dc[dc_count++]->len;
		}
		break;
	      case SEC_OID_PKCS9_EMAIL_ADDRESS:
	      case SEC_OID_RFC1274_MAIL:
		if (email) {
			break;
		}
		email = CERT_DecodeAVAValue(&ava->value);
		if (!email) {
			goto loser;
		}
		len += email->len;
		break;
	      default:
		break;
	    }
	}
    }

    /* XXX - add some for formatting */
    len += 128;

    /* allocate buffer */
    buf = (char *)PORT_Alloc(len);
    if ( !buf ) {
	goto loser;
    }

    tmpbuf = buf;
    
    if ( cn ) {
	PORT_Memcpy(tmpbuf, cn->data, cn->len);
	tmpbuf += cn->len;
	PORT_Memcpy(tmpbuf, BREAK, BREAKLEN);
	tmpbuf += BREAKLEN;
    }
    if ( email ) {
	PORT_Memcpy(tmpbuf, email->data, email->len);
	tmpbuf += ( email->len );
	PORT_Memcpy(tmpbuf, BREAK, BREAKLEN);
	tmpbuf += BREAKLEN;
    }
    for (i=ou_count-1; i >= 0; i--) {
	PORT_Memcpy(tmpbuf, orgunit[i]->data, orgunit[i]->len);
	tmpbuf += ( orgunit[i]->len );
	PORT_Memcpy(tmpbuf, BREAK, BREAKLEN);
	tmpbuf += BREAKLEN;
    }
    if ( dq ) {
	PORT_Memcpy(tmpbuf, dq->data, dq->len);
	tmpbuf += ( dq->len );
	PORT_Memcpy(tmpbuf, BREAK, BREAKLEN);
	tmpbuf += BREAKLEN;
    }
    if ( org ) {
	PORT_Memcpy(tmpbuf, org->data, org->len);
	tmpbuf += ( org->len );
	PORT_Memcpy(tmpbuf, BREAK, BREAKLEN);
	tmpbuf += BREAKLEN;
    }
    for (i=dc_count-1; i >= 0; i--) {
	PORT_Memcpy(tmpbuf, dc[i]->data, dc[i]->len);
	tmpbuf += ( dc[i]->len );
	PORT_Memcpy(tmpbuf, BREAK, BREAKLEN);
	tmpbuf += BREAKLEN;
    }
    first = PR_TRUE;
    if ( loc ) {
	PORT_Memcpy(tmpbuf, loc->data,  loc->len);
	tmpbuf += ( loc->len );
	first = PR_FALSE;
    }
    if ( state ) {
	if ( !first ) {
	    PORT_Memcpy(tmpbuf, COMMA, COMMALEN);
	    tmpbuf += COMMALEN;
	}
	PORT_Memcpy(tmpbuf, state->data, state->len);
	tmpbuf += ( state->len );
	first = PR_FALSE;
    }
    if ( country ) {
	if ( !first ) {
	    PORT_Memcpy(tmpbuf, COMMA, COMMALEN);
	    tmpbuf += COMMALEN;
	}
	PORT_Memcpy(tmpbuf, country->data, country->len);
	tmpbuf += ( country->len );
	first = PR_FALSE;
    }
    if ( !first ) {
	PORT_Memcpy(tmpbuf, BREAK, BREAKLEN);
	tmpbuf += BREAKLEN;
    }

    *tmpbuf = 0;

    /* fall through and clean */
loser:
    if ( cn ) {
	SECITEM_FreeItem(cn, PR_TRUE);
    }
    if ( email ) {
	SECITEM_FreeItem(email, PR_TRUE);
    }
    for (i=ou_count-1; i >= 0; i--) {
	SECITEM_FreeItem(orgunit[i], PR_TRUE);
    }
    if ( dq ) {
	SECITEM_FreeItem(dq, PR_TRUE);
    }
    if ( org ) {
	SECITEM_FreeItem(org, PR_TRUE);
    }
    for (i=dc_count-1; i >= 0; i--) {
	SECITEM_FreeItem(dc[i], PR_TRUE);
    }
    if ( loc ) {
	SECITEM_FreeItem(loc, PR_TRUE);
    }
    if ( state ) {
	SECITEM_FreeItem(state, PR_TRUE);
    }
    if ( country ) {
	SECITEM_FreeItem(country, PR_TRUE);
    }

    return(buf);
}

