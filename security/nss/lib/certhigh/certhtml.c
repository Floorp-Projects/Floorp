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

/*
 * certhtml.c --- convert a cert to html
 *
 * $Id: certhtml.c,v 1.10 2010/08/28 18:00:28 nelson%bolyard.com Exp $
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

