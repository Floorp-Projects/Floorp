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
#include "secoid.h"
#include "secder.h"	/* XXX remove this when remove the DERTemplates */
#include "secasn1.h"
#include "secitem.h"
#include <stdarg.h>
#include "secerr.h"

static const SEC_ASN1Template cert_AVATemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(CERTAVA) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(CERTAVA,type), },
    { SEC_ASN1_ANY,
	  offsetof(CERTAVA,value), },
    { 0, }
};

const SEC_ASN1Template CERT_RDNTemplate[] = {
    { SEC_ASN1_SET_OF,
	  offsetof(CERTRDN,avas), cert_AVATemplate, sizeof(CERTRDN) }
};


static int
CountArray(void **array)
{
    int count = 0;
    if (array) {
	while (*array++) {
	    count++;
	}
    }
    return count;
}

static void
**AddToArray(PRArenaPool *arena, void **array, void *element)
{
    unsigned count;
    void **ap;

    /* Count up number of slots already in use in the array */
    count = 0;
    ap = array;
    if (ap) {
	while (*ap++) {
	    count++;
	}
    }

    if (array) {
	array = (void**) PORT_ArenaGrow(arena, array,
					(count + 1) * sizeof(void *),
					(count + 2) * sizeof(void *));
    } else {
	array = (void**) PORT_ArenaAlloc(arena, (count + 2) * sizeof(void *));
    }
    if (array) {
	array[count] = element;
	array[count+1] = 0;
    }
    return array;
}

#if 0
static void
**RemoveFromArray(void **array, void *element)
{
    unsigned count;
    void **ap;
    int slot;

    /* Look for element */
    ap = array;
    if (ap) {
	count = 1;			/* count the null at the end */
	slot = -1;
	for (; *ap; ap++, count++) {
	    if (*ap == element) {
		/* Found it */
		slot = ap - array;
	    }
	}
	if (slot >= 0) {
	    /* Found it. Squish array down */
	    PORT_Memmove((void*) (array + slot), (void*) (array + slot + 1),
		       (count - slot - 1) * sizeof(void*));
	    /* Don't bother reallocing the memory */
	}
    }
    return array;
}
#endif /* 0 */

SECOidTag
CERT_GetAVATag(CERTAVA *ava)
{
    SECOidData *oid;
    if (!ava->type.data) return (SECOidTag)-1;

    oid = SECOID_FindOID(&ava->type);
    
    if ( oid ) {
	return(oid->offset);
    }
    return (SECOidTag)-1;
}

static SECStatus
SetupAVAType(PRArenaPool *arena, SECOidTag type, SECItem *it, unsigned *maxLenp)
{
    unsigned char *oid;
    unsigned oidLen;
    unsigned char *cp;
    unsigned maxLen;
    SECOidData *oidrec;

    oidrec = SECOID_FindOIDByTag(type);
    if (oidrec == NULL)
	return SECFailure;

    oid = oidrec->oid.data;
    oidLen = oidrec->oid.len;

    switch (type) {
      case SEC_OID_AVA_COUNTRY_NAME:
	maxLen = 2;
	break;
      case SEC_OID_AVA_ORGANIZATION_NAME:
	maxLen = 64;
	break;
      case SEC_OID_AVA_COMMON_NAME:
	maxLen = 64;
	break;
      case SEC_OID_AVA_LOCALITY:
	maxLen = 128;
	break;
      case SEC_OID_AVA_STATE_OR_PROVINCE:
	maxLen = 128;
	break;
      case SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME:
	maxLen = 64;
	break;
      case SEC_OID_AVA_DC:
	maxLen = 128;
	break;
      case SEC_OID_AVA_DN_QUALIFIER:
	maxLen = 0x7fff;
	break;
      case SEC_OID_PKCS9_EMAIL_ADDRESS:
	maxLen = 128;
	break;
      case SEC_OID_RFC1274_UID:
	maxLen = 256;  /* RFC 1274 specifies 256 */
	break;
      case SEC_OID_RFC1274_MAIL:
	maxLen = 256;  /* RFC 1274 specifies 256 */
	break; 
      default:
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    it->data = cp = (unsigned char*) PORT_ArenaAlloc(arena, oidLen);
    if (cp == NULL) {
	return SECFailure;
    }
    it->len = oidLen;
    PORT_Memcpy(cp, oid, oidLen);
    *maxLenp = maxLen;
    return SECSuccess;
}

static SECStatus
SetupAVAValue(PRArenaPool *arena, int valueType, char *value, SECItem *it,
	      unsigned maxLen)
{
    unsigned valueLen, valueLenLen, total;
    unsigned ucs4Len = 0, ucs4MaxLen;
    unsigned char *cp, *ucs4Val;

    switch (valueType) {
      case SEC_ASN1_PRINTABLE_STRING:
      case SEC_ASN1_IA5_STRING:
      case SEC_ASN1_T61_STRING:
	valueLen = PORT_Strlen(value);
	break;
      case SEC_ASN1_UNIVERSAL_STRING:
	valueLen = PORT_Strlen(value);
	ucs4Val = (unsigned char *)PORT_ArenaZAlloc(arena, 
						    PORT_Strlen(value) * 6);
	ucs4MaxLen = PORT_Strlen(value) * 6;
	if(!ucs4Val || !PORT_UCS4_UTF8Conversion(PR_TRUE, (unsigned char *)value, valueLen,
					ucs4Val, ucs4MaxLen, &ucs4Len)) {
	    PORT_SetError(SEC_ERROR_INVALID_ARGS);
	    return SECFailure;
	}
	value = (char *)ucs4Val;
	valueLen = ucs4Len;
	break;
      default:
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    if (((valueType != SEC_ASN1_UNIVERSAL_STRING) && (valueLen > maxLen)) ||
      ((valueType == SEC_ASN1_UNIVERSAL_STRING) && (valueLen > (maxLen * 4)))) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    } 

    valueLenLen = DER_LengthLength(valueLen);
    total = 1 + valueLenLen + valueLen;
    it->data = cp = (unsigned char*) PORT_ArenaAlloc(arena, total);
    if (!cp) {
	return SECFailure;
    }
    it->len = total;
    cp = (unsigned char*) DER_StoreHeader(cp, valueType, valueLen);
    PORT_Memcpy(cp, value, valueLen);
    return SECSuccess;
}

CERTAVA *
CERT_CreateAVA(PRArenaPool *arena, SECOidTag kind, int valueType, char *value)
{
    CERTAVA *ava;
    int rv;
    unsigned maxLen;

    ava = (CERTAVA*) PORT_ArenaZAlloc(arena, sizeof(CERTAVA));
    if (ava) {
	rv = SetupAVAType(arena, kind, &ava->type, &maxLen);
	if (rv) {
	    /* Illegal AVA type */
	    return 0;
	}
	rv = SetupAVAValue(arena, valueType, value, &ava->value, maxLen);
	if (rv) {
	    /* Illegal value type */
	    return 0;
	}
    }
    return ava;
}

CERTAVA *
CERT_CopyAVA(PRArenaPool *arena, CERTAVA *from)
{
    CERTAVA *ava;
    int rv;

    ava = (CERTAVA*) PORT_ArenaZAlloc(arena, sizeof(CERTAVA));
    if (ava) {
	rv = SECITEM_CopyItem(arena, &ava->type, &from->type);
	if (rv) goto loser;
	rv = SECITEM_CopyItem(arena, &ava->value, &from->value);
	if (rv) goto loser;
    }
    return ava;

  loser:
    return 0;
}

/************************************************************************/
/* XXX This template needs to go away in favor of the new SEC_ASN1 version. */
static const SEC_ASN1Template cert_RDNTemplate[] = {
    { SEC_ASN1_SET_OF,
	  offsetof(CERTRDN,avas), cert_AVATemplate, sizeof(CERTRDN) }
};


CERTRDN *
CERT_CreateRDN(PRArenaPool *arena, CERTAVA *ava0, ...)
{
    CERTAVA *ava;
    CERTRDN *rdn;
    va_list ap;
    unsigned count;
    CERTAVA **avap;

    rdn = (CERTRDN*) PORT_ArenaAlloc(arena, sizeof(CERTRDN));
    if (rdn) {
	/* Count number of avas going into the rdn */
	count = 1;
	va_start(ap, ava0);
	while ((ava = va_arg(ap, CERTAVA*)) != 0) {
	    count++;
	}
	va_end(ap);

	/* Now fill in the pointers */
	rdn->avas = avap =
	    (CERTAVA**) PORT_ArenaAlloc( arena, (count + 1)*sizeof(CERTAVA*));
	if (!avap) {
	    return 0;
	}
	*avap++ = ava0;
	va_start(ap, ava0);
	while ((ava = va_arg(ap, CERTAVA*)) != 0) {
	    *avap++ = ava;
	}
	va_end(ap);
	*avap++ = 0;
    }
    return rdn;
}

SECStatus
CERT_AddAVA(PRArenaPool *arena, CERTRDN *rdn, CERTAVA *ava)
{
    rdn->avas = (CERTAVA**) AddToArray(arena, (void**) rdn->avas, ava);
    return rdn->avas ? SECSuccess : SECFailure;
}

SECStatus
CERT_CopyRDN(PRArenaPool *arena, CERTRDN *to, CERTRDN *from)
{
    CERTAVA **avas, *fava, *tava;
    SECStatus rv;

    /* Copy each ava from from */
    avas = from->avas;
    while ((fava = *avas++) != 0) {
	tava = CERT_CopyAVA(arena, fava);
	if (!tava) return SECFailure;
	rv = CERT_AddAVA(arena, to, tava);
	if (rv) return rv;
    }
    return SECSuccess;
}

/************************************************************************/

const SEC_ASN1Template CERT_NameTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF,
	  offsetof(CERTName,rdns), CERT_RDNTemplate, sizeof(CERTName) }
};

CERTName *
CERT_CreateName(CERTRDN *rdn0, ...)
{
    CERTRDN *rdn;
    CERTName *name;
    va_list ap;
    unsigned count;
    CERTRDN **rdnp;
    PRArenaPool *arena;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if ( !arena ) {
	return(0);
    }
    
    name = (CERTName*) PORT_ArenaAlloc(arena, sizeof(CERTName));
    if (name) {
	name->arena = arena;
	
	/* Count number of RDNs going into the Name */
	if (!rdn0) {
	    count = 0;
	} else {
	    count = 1;
	    va_start(ap, rdn0);
	    while ((rdn = va_arg(ap, CERTRDN*)) != 0) {
		count++;
	    }
	    va_end(ap);
	}

	/* Allocate space (including space for terminal null ptr) */
	name->rdns = rdnp =
	    (CERTRDN**) PORT_ArenaAlloc(arena, (count + 1) * sizeof(CERTRDN*));
	if (!name->rdns) {
	    goto loser;
	}

	/* Now fill in the pointers */
	if (count > 0) {
	    *rdnp++ = rdn0;
	    va_start(ap, rdn0);
	    while ((rdn = va_arg(ap, CERTRDN*)) != 0) {
		*rdnp++ = rdn;
	    }
	    va_end(ap);
	}

	/* null terminate the list */
	*rdnp++ = 0;
    }
    return name;

loser:
    PORT_FreeArena(arena, PR_FALSE);
    return(0);
}

void
CERT_DestroyName(CERTName *name)
{
    if (name)
    {
        PRArenaPool *arena = name->arena;
        name->rdns = NULL;
	name->arena = NULL;
	if (arena) PORT_FreeArena(arena, PR_FALSE);
    }
}

SECStatus
CERT_AddRDN(CERTName *name, CERTRDN *rdn)
{
    name->rdns = (CERTRDN**) AddToArray(name->arena, (void**) name->rdns, rdn);
    return name->rdns ? SECSuccess : SECFailure;
}

SECStatus
CERT_CopyName(PRArenaPool *arena, CERTName *to, CERTName *from)
{
    CERTRDN **rdns, *frdn, *trdn;
    SECStatus rv;

    CERT_DestroyName(to);
    to->arena = arena;

    /* Copy each rdn from from */
    rdns = from->rdns;
    while ((frdn = *rdns++) != 0) {
	trdn = CERT_CreateRDN(arena, 0);
	if ( trdn == NULL ) {
	    return(SECFailure);
	}
	rv = CERT_CopyRDN(arena, trdn, frdn);
	if (rv) return rv;
	rv = CERT_AddRDN(to, trdn);
	if (rv) return rv;
    }
    return SECSuccess;
}

/************************************************************************/

SECComparison
CERT_CompareAVA(CERTAVA *a, CERTAVA *b)
{
    SECComparison rv;

    rv = SECITEM_CompareItem(&a->type, &b->type);
    if (rv) {
	/*
	** XXX for now we are going to just assume that a bitwise
	** comparison of the value codes will do the trick.
	*/
    }
    rv = SECITEM_CompareItem(&a->value, &b->value);
    return rv;
}

SECComparison
CERT_CompareRDN(CERTRDN *a, CERTRDN *b)
{
    CERTAVA **aavas, *aava;
    CERTAVA **bavas, *bava;
    int ac, bc;
    SECComparison rv = SECEqual;

    aavas = a->avas;
    bavas = b->avas;

    /*
    ** Make sure array of ava's are the same length. If not, then we are
    ** not equal
    */
    ac = CountArray((void**) aavas);
    bc = CountArray((void**) bavas);
    if (ac < bc) return SECLessThan;
    if (ac > bc) return SECGreaterThan;

    for (;;) {
	aava = *aavas++;
	bava = *bavas++;
	if (!aava) {
	    break;
	}
	rv = CERT_CompareAVA(aava, bava);
	if (rv) return rv;
    }
    return rv;
}

SECComparison
CERT_CompareName(CERTName *a, CERTName *b)
{
    CERTRDN **ardns, *ardn;
    CERTRDN **brdns, *brdn;
    int ac, bc;
    SECComparison rv = SECEqual;

    ardns = a->rdns;
    brdns = b->rdns;

    /*
    ** Make sure array of rdn's are the same length. If not, then we are
    ** not equal
    */
    ac = CountArray((void**) ardns);
    bc = CountArray((void**) brdns);
    if (ac < bc) return SECLessThan;
    if (ac > bc) return SECGreaterThan;

    for (;;) {
	ardn = *ardns++;
	brdn = *brdns++;
	if (!ardn) {
	    break;
	}
	rv = CERT_CompareRDN(ardn, brdn);
	if (rv) return rv;
    }
    return rv;
}

/* Moved from certhtml.c */
SECItem *
CERT_DecodeAVAValue(SECItem *derAVAValue)
{
          SECItem          *retItem; 
    const SEC_ASN1Template *theTemplate       = NULL;
          PRBool            convertUCS4toUTF8 = PR_FALSE;
          PRBool            convertUCS2toUTF8 = PR_FALSE;
          SECItem           avaValue          = {siBuffer, 0}; 

    if(!derAVAValue) {
	return NULL;
    }

    switch(derAVAValue->data[0]) {
	case SEC_ASN1_UNIVERSAL_STRING:
	    convertUCS4toUTF8 = PR_TRUE;
	    theTemplate = SEC_UniversalStringTemplate;
	    break;
	case SEC_ASN1_IA5_STRING:
	    theTemplate = SEC_IA5StringTemplate;
	    break;
	case SEC_ASN1_PRINTABLE_STRING:
	    theTemplate = SEC_PrintableStringTemplate;
	    break;
	case SEC_ASN1_T61_STRING:
	    theTemplate = SEC_T61StringTemplate;
	    break;
	case SEC_ASN1_BMP_STRING:
	    convertUCS2toUTF8 = PR_TRUE;
	    theTemplate = SEC_BMPStringTemplate;
	    break;
	case SEC_ASN1_UTF8_STRING:
	    /* No conversion needed ! */
	    theTemplate = SEC_UTF8StringTemplate;
	    break;
	default:
	    return NULL;
    }

    PORT_Memset(&avaValue, 0, sizeof(SECItem));
    if(SEC_ASN1DecodeItem(NULL, &avaValue, theTemplate, derAVAValue) 
				!= SECSuccess) {
	return NULL;
    }

    if (convertUCS4toUTF8) {
	unsigned int   utf8ValLen = avaValue.len * 3;
	unsigned char *utf8Val    = (unsigned char*)PORT_ZAlloc(utf8ValLen);

	if(!PORT_UCS4_UTF8Conversion(PR_FALSE, avaValue.data, avaValue.len,
				     utf8Val, utf8ValLen, &utf8ValLen)) {
	    PORT_Free(utf8Val);
	    PORT_Free(avaValue.data);
	    return NULL;
	}

	PORT_Free(avaValue.data);
	avaValue.data = utf8Val;
	avaValue.len = utf8ValLen;

    } else if (convertUCS2toUTF8) {

	unsigned int   utf8ValLen = avaValue.len * 3;
	unsigned char *utf8Val    = (unsigned char*)PORT_ZAlloc(utf8ValLen);

	if(!PORT_UCS2_UTF8Conversion(PR_FALSE, avaValue.data, avaValue.len,
				     utf8Val, utf8ValLen, &utf8ValLen)) {
	    PORT_Free(utf8Val);
	    PORT_Free(avaValue.data);
	    return NULL;
	}

	PORT_Free(avaValue.data);
	avaValue.data = utf8Val;
	avaValue.len = utf8ValLen;
    }

    retItem = SECITEM_DupItem(&avaValue);
    PORT_Free(avaValue.data);
    return retItem;
}
