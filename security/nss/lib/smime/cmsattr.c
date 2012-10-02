/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * CMS attributes.
 *
 * $Id: cmsattr.c,v 1.10 2012/04/25 14:50:08 gerv%gerv.net Exp $
 */

#include "cmslocal.h"

#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"
#include "prtime.h"
#include "secerr.h"

/*
 * -------------------------------------------------------------------
 * XXX The following Attribute stuff really belongs elsewhere.
 * The Attribute type is *not* part of CMS but rather X.501.
 * But for now, since CMS is the only customer of attributes,
 * we define them here.  Once there is a use outside of CMS,
 * then change the attribute types and functions from internal
 * to external naming convention, and move them elsewhere!
 */


/*
 * NSS_CMSAttribute_Create - create an attribute
 *
 * if value is NULL, the attribute won't have a value. It can be added later
 * with NSS_CMSAttribute_AddValue.
 */
NSSCMSAttribute *
NSS_CMSAttribute_Create(PRArenaPool *poolp, SECOidTag oidtag, SECItem *value, PRBool encoded)
{
    NSSCMSAttribute *attr;
    SECItem *copiedvalue;
    void *mark;

    PORT_Assert (poolp != NULL);

    mark = PORT_ArenaMark (poolp);

    attr = (NSSCMSAttribute *)PORT_ArenaZAlloc(poolp, sizeof(NSSCMSAttribute));
    if (attr == NULL)
	goto loser;

    attr->typeTag = SECOID_FindOIDByTag(oidtag);
    if (attr->typeTag == NULL)
	goto loser;

    if (SECITEM_CopyItem(poolp, &(attr->type), &(attr->typeTag->oid)) != SECSuccess)
	goto loser;

    if (value != NULL) {
	if ((copiedvalue = SECITEM_ArenaDupItem(poolp, value)) == NULL)
	    goto loser;

	if (NSS_CMSArray_Add(poolp, (void ***)&(attr->values), (void *)copiedvalue) != SECSuccess)
	    goto loser;
    }

    attr->encoded = encoded;

    PORT_ArenaUnmark (poolp, mark);

    return attr;

loser:
    PORT_Assert (mark != NULL);
    PORT_ArenaRelease (poolp, mark);
    return NULL;
}

/*
 * NSS_CMSAttribute_AddValue - add another value to an attribute
 */
SECStatus
NSS_CMSAttribute_AddValue(PLArenaPool *poolp, NSSCMSAttribute *attr, SECItem *value)
{
    SECItem *copiedvalue;
    void *mark;

    PORT_Assert (poolp != NULL);

    mark = PORT_ArenaMark(poolp);

    if (value == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	goto loser;
    }

    if ((copiedvalue = SECITEM_ArenaDupItem(poolp, value)) == NULL)
	goto loser;

    if (NSS_CMSArray_Add(poolp, (void ***)&(attr->values), (void *)copiedvalue) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_Assert (mark != NULL);
    PORT_ArenaRelease (poolp, mark);
    return SECFailure;
}

/*
 * NSS_CMSAttribute_GetType - return the OID tag
 */
SECOidTag
NSS_CMSAttribute_GetType(NSSCMSAttribute *attr)
{
    SECOidData *typetag;

    typetag = SECOID_FindOID(&(attr->type));
    if (typetag == NULL)
	return SEC_OID_UNKNOWN;

    return typetag->offset;
}

/*
 * NSS_CMSAttribute_GetValue - return the first attribute value
 *
 * We do some sanity checking first:
 * - Multiple values are *not* expected.
 * - Empty values are *not* expected.
 */
SECItem *
NSS_CMSAttribute_GetValue(NSSCMSAttribute *attr)
{
    SECItem *value;

    if (attr == NULL)
	return NULL;

    value = attr->values[0];

    if (value == NULL || value->data == NULL || value->len == 0)
	return NULL;

    if (attr->values[1] != NULL)
	return NULL;

    return value;
}

/*
 * NSS_CMSAttribute_CompareValue - compare the attribute's first value against data
 */
PRBool
NSS_CMSAttribute_CompareValue(NSSCMSAttribute *attr, SECItem *av)
{
    SECItem *value;
    
    if (attr == NULL)
	return PR_FALSE;

    value = NSS_CMSAttribute_GetValue(attr);

    return (value != NULL && value->len == av->len &&
	PORT_Memcmp (value->data, av->data, value->len) == 0);
}

/*
 * templates and functions for separate ASN.1 encoding of attributes
 *
 * used in NSS_CMSAttributeArray_Reorder
 */

/*
 * helper function for dynamic template determination of the attribute value
 */
static const SEC_ASN1Template *
cms_attr_choose_attr_value_template(void *src_or_dest, PRBool encoding)
{
    const SEC_ASN1Template *theTemplate;
    NSSCMSAttribute *attribute;
    SECOidData *oiddata;
    PRBool encoded;

    PORT_Assert (src_or_dest != NULL);
    if (src_or_dest == NULL)
	return NULL;

    attribute = (NSSCMSAttribute *)src_or_dest;

    if (encoding && (!attribute->values || !attribute->values[0] ||
        attribute->encoded)) {
        /* we're encoding, and the attribute has no value or the attribute
         * value is already encoded. */
        return SEC_ASN1_GET(SEC_AnyTemplate);
    }

    /* get attribute's typeTag */
    oiddata = attribute->typeTag;
    if (oiddata == NULL) {
	oiddata = SECOID_FindOID(&attribute->type);
	attribute->typeTag = oiddata;
    }

    if (oiddata == NULL) {
	/* still no OID tag? OID is unknown then. en/decode value as ANY. */
	encoded = PR_TRUE;
	theTemplate = SEC_ASN1_GET(SEC_AnyTemplate);
    } else {
	switch (oiddata->offset) {
	case SEC_OID_PKCS9_SMIME_CAPABILITIES:
	case SEC_OID_SMIME_ENCRYPTION_KEY_PREFERENCE:
	    /* these guys need to stay DER-encoded */
	default:
	    /* same goes for OIDs that are not handled here */
	    encoded = PR_TRUE;
	    theTemplate = SEC_ASN1_GET(SEC_AnyTemplate);
	    break;
	    /* otherwise choose proper template */
	case SEC_OID_PKCS9_EMAIL_ADDRESS:
	case SEC_OID_RFC1274_MAIL:
	case SEC_OID_PKCS9_UNSTRUCTURED_NAME:
	    encoded = PR_FALSE;
	    theTemplate = SEC_ASN1_GET(SEC_IA5StringTemplate);
	    break;
	case SEC_OID_PKCS9_CONTENT_TYPE:
	    encoded = PR_FALSE;
	    theTemplate = SEC_ASN1_GET(SEC_ObjectIDTemplate);
	    break;
	case SEC_OID_PKCS9_MESSAGE_DIGEST:
	    encoded = PR_FALSE;
	    theTemplate = SEC_ASN1_GET(SEC_OctetStringTemplate);
	    break;
	case SEC_OID_PKCS9_SIGNING_TIME:
	    encoded = PR_FALSE;
	    theTemplate = SEC_ASN1_GET(CERT_TimeChoiceTemplate);
	    break;
	  /* XXX Want other types here, too */
	}
    }

    if (encoding) {
	/*
	 * If we are encoding and we think we have an already-encoded value,
	 * then the code which initialized this attribute should have set
	 * the "encoded" property to true (and we would have returned early,
	 * up above).  No devastating error, but that code should be fixed.
	 * (It could indicate that the resulting encoded bytes are wrong.)
	 */
	PORT_Assert (!encoded);
    } else {
	/*
	 * We are decoding; record whether the resulting value is
	 * still encoded or not.
	 */
	attribute->encoded = encoded;
    }
    return theTemplate;
}

static const SEC_ASN1TemplateChooserPtr cms_attr_chooser
	= cms_attr_choose_attr_value_template;

const SEC_ASN1Template nss_cms_attribute_template[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSSCMSAttribute) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(NSSCMSAttribute,type) },
    { SEC_ASN1_DYNAMIC | SEC_ASN1_SET_OF,
	  offsetof(NSSCMSAttribute,values),
	  &cms_attr_chooser },
    { 0 }
};

const SEC_ASN1Template nss_cms_set_of_attribute_template[] = {
    { SEC_ASN1_SET_OF, 0, nss_cms_attribute_template },
};

/* =============================================================================
 * Attribute Array methods
 */

/*
 * NSS_CMSAttributeArray_Encode - encode an Attribute array as SET OF Attributes
 *
 * If you are wondering why this routine does not reorder the attributes
 * first, and might be tempted to make it do so, see the comment by the
 * call to ReorderAttributes in cmsencode.c.  (Or, see who else calls this
 * and think long and hard about the implications of making it always
 * do the reordering.)
 */
SECItem *
NSS_CMSAttributeArray_Encode(PRArenaPool *poolp, NSSCMSAttribute ***attrs, SECItem *dest)
{
    return SEC_ASN1EncodeItem (poolp, dest, (void *)attrs, nss_cms_set_of_attribute_template);
}

/*
 * NSS_CMSAttributeArray_Reorder - sort attribute array by attribute's DER encoding
 *
 * make sure that the order of the attributes guarantees valid DER (which must be
 * in lexigraphically ascending order for a SET OF); if reordering is necessary it
 * will be done in place (in attrs).
 */
SECStatus
NSS_CMSAttributeArray_Reorder(NSSCMSAttribute **attrs)
{
    return NSS_CMSArray_SortByDER((void **)attrs, nss_cms_attribute_template, NULL);
}

/*
 * NSS_CMSAttributeArray_FindAttrByOidTag - look through a set of attributes and
 * find one that matches the specified object ID.
 *
 * If "only" is true, then make sure that there is not more than one attribute
 * of the same type.  Otherwise, just return the first one found. (XXX Does
 * anybody really want that first-found behavior?  It was like that when I found it...)
 */
NSSCMSAttribute *
NSS_CMSAttributeArray_FindAttrByOidTag(NSSCMSAttribute **attrs, SECOidTag oidtag, PRBool only)
{
    SECOidData *oid;
    NSSCMSAttribute *attr1, *attr2;

    if (attrs == NULL)
	return NULL;

    oid = SECOID_FindOIDByTag(oidtag);
    if (oid == NULL)
	return NULL;

    while ((attr1 = *attrs++) != NULL) {
	if (attr1->type.len == oid->oid.len && PORT_Memcmp (attr1->type.data,
							    oid->oid.data,
							    oid->oid.len) == 0)
	    break;
    }

    if (attr1 == NULL)
	return NULL;

    if (!only)
	return attr1;

    while ((attr2 = *attrs++) != NULL) {
	if (attr2->type.len == oid->oid.len && PORT_Memcmp (attr2->type.data,
							    oid->oid.data,
							    oid->oid.len) == 0)
	    break;
    }

    if (attr2 != NULL)
	return NULL;

    return attr1;
}

/*
 * NSS_CMSAttributeArray_AddAttr - add an attribute to an
 * array of attributes. 
 */
SECStatus
NSS_CMSAttributeArray_AddAttr(PLArenaPool *poolp, NSSCMSAttribute ***attrs, NSSCMSAttribute *attr)
{
    NSSCMSAttribute *oattr;
    void *mark;
    SECOidTag type;

    mark = PORT_ArenaMark(poolp);

    /* find oidtag of attr */
    type = NSS_CMSAttribute_GetType(attr);

    /* see if we have one already */
    oattr = NSS_CMSAttributeArray_FindAttrByOidTag(*attrs, type, PR_FALSE);
    PORT_Assert (oattr == NULL);
    if (oattr != NULL)
	goto loser;	/* XXX or would it be better to replace it? */

    /* no, shove it in */
    if (NSS_CMSArray_Add(poolp, (void ***)attrs, (void *)attr) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

/*
 * NSS_CMSAttributeArray_SetAttr - set an attribute's value in a set of attributes
 */
SECStatus
NSS_CMSAttributeArray_SetAttr(PLArenaPool *poolp, NSSCMSAttribute ***attrs, SECOidTag type, SECItem *value, PRBool encoded)
{
    NSSCMSAttribute *attr;
    void *mark;

    mark = PORT_ArenaMark(poolp);

    /* see if we have one already */
    attr = NSS_CMSAttributeArray_FindAttrByOidTag(*attrs, type, PR_FALSE);
    if (attr == NULL) {
	/* not found? create one! */
	attr = NSS_CMSAttribute_Create(poolp, type, value, encoded);
	if (attr == NULL)
	    goto loser;
	/* and add it to the list */
	if (NSS_CMSArray_Add(poolp, (void ***)attrs, (void *)attr) != SECSuccess)
	    goto loser;
    } else {
	/* found, shove it in */
	/* XXX we need a decent memory model @#$#$!#!!! */
	attr->values[0] = value;
	attr->encoded = encoded;
    }

    PORT_ArenaUnmark (poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease (poolp, mark);
    return SECFailure;
}

