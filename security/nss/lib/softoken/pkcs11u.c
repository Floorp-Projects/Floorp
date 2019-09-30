/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Internal PKCS #11 functions. Should only be called by pkcs11.c
 */
#include "pkcs11.h"
#include "pkcs11i.h"
#include "lowkeyi.h"
#include "secasn1.h"
#include "blapi.h"
#include "secerr.h"
#include "prnetdb.h" /* for PR_ntohl */
#include "sftkdb.h"
#include "softoken.h"

/*
 * ******************** Error mapping *******************************
 */
/*
 * map all the SEC_ERROR_xxx error codes that may be returned by freebl
 * functions to CKR_xxx.  return CKR_DEVICE_ERROR by default for backward
 * compatibility.
 */
CK_RV
sftk_MapCryptError(int error)
{
    switch (error) {
        case SEC_ERROR_INVALID_ARGS:
        case SEC_ERROR_BAD_DATA: /* MP_RANGE gets mapped to this */
            return CKR_ARGUMENTS_BAD;
        case SEC_ERROR_INPUT_LEN:
            return CKR_DATA_LEN_RANGE;
        case SEC_ERROR_OUTPUT_LEN:
            return CKR_BUFFER_TOO_SMALL;
        case SEC_ERROR_LIBRARY_FAILURE:
            return CKR_GENERAL_ERROR;
        case SEC_ERROR_NO_MEMORY:
            return CKR_HOST_MEMORY;
        case SEC_ERROR_BAD_SIGNATURE:
            return CKR_SIGNATURE_INVALID;
        case SEC_ERROR_INVALID_KEY:
            return CKR_KEY_SIZE_RANGE;
        case SEC_ERROR_BAD_KEY:        /* an EC public key that fails validation */
            return CKR_KEY_SIZE_RANGE; /* the closest error code */
        case SEC_ERROR_UNSUPPORTED_EC_POINT_FORM:
            return CKR_TEMPLATE_INCONSISTENT;
        case SEC_ERROR_UNSUPPORTED_KEYALG:
            return CKR_MECHANISM_INVALID;
        case SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE:
            return CKR_DOMAIN_PARAMS_INVALID;
        /* key pair generation failed after max number of attempts */
        case SEC_ERROR_NEED_RANDOM:
            return CKR_FUNCTION_FAILED;
    }
    return CKR_DEVICE_ERROR;
}
/*
 * ******************** Attribute Utilities *******************************
 */

/*
 * create a new attribute with type, value, and length. Space is allocated
 * to hold value.
 */
static SFTKAttribute *
sftk_NewAttribute(SFTKObject *object,
                  CK_ATTRIBUTE_TYPE type, const void *value, CK_ULONG len)
{
    SFTKAttribute *attribute;

    SFTKSessionObject *so = sftk_narrowToSessionObject(object);
    int index;

    if (so == NULL) {
        /* allocate new attribute in a buffer */
        PORT_Assert(0);
        return NULL;
    }
    /*
     * We attempt to keep down contention on Malloc and Arena locks by
     * limiting the number of these calls on high traversed paths. This
     * is done for attributes by 'allocating' them from a pool already
     * allocated by the parent object.
     */
    PZ_Lock(so->attributeLock);
    index = so->nextAttr++;
    PZ_Unlock(so->attributeLock);
    PORT_Assert(index < MAX_OBJS_ATTRS);
    if (index >= MAX_OBJS_ATTRS)
        return NULL;

    attribute = &so->attrList[index];
    attribute->attrib.type = type;
    attribute->freeAttr = PR_FALSE;
    attribute->freeData = PR_FALSE;
    if (value) {
        if (len <= ATTR_SPACE) {
            attribute->attrib.pValue = attribute->space;
        } else {
            attribute->attrib.pValue = PORT_Alloc(len);
            attribute->freeData = PR_TRUE;
        }
        if (attribute->attrib.pValue == NULL) {
            return NULL;
        }
        PORT_Memcpy(attribute->attrib.pValue, value, len);
        attribute->attrib.ulValueLen = len;
    } else {
        attribute->attrib.pValue = NULL;
        attribute->attrib.ulValueLen = 0;
    }
    attribute->attrib.type = type;
    attribute->handle = type;
    attribute->next = attribute->prev = NULL;
    return attribute;
}

/*
 * Free up all the memory associated with an attribute. Reference count
 * must be zero to call this.
 */
static void
sftk_DestroyAttribute(SFTKAttribute *attribute)
{
    if (attribute->freeData) {
        if (attribute->attrib.pValue) {
            /* clear out the data in the attribute value... it may have been
             * sensitive data */
            PORT_Memset(attribute->attrib.pValue, 0,
                        attribute->attrib.ulValueLen);
        }
        PORT_Free(attribute->attrib.pValue);
    }
    PORT_Free(attribute);
}

/*
 * release a reference to an attribute structure
 */
void
sftk_FreeAttribute(SFTKAttribute *attribute)
{
    if (attribute->freeAttr) {
        sftk_DestroyAttribute(attribute);
        return;
    }
}

static SFTKAttribute *
sftk_FindTokenAttribute(SFTKTokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *myattribute = NULL;
    SFTKDBHandle *dbHandle = NULL;
    CK_RV crv = CKR_HOST_MEMORY;

    myattribute = (SFTKAttribute *)PORT_Alloc(sizeof(SFTKAttribute));
    if (myattribute == NULL) {
        goto loser;
    }

    dbHandle = sftk_getDBForTokenObject(object->obj.slot, object->obj.handle);

    myattribute->handle = type;
    myattribute->attrib.type = type;
    myattribute->attrib.pValue = myattribute->space;
    myattribute->attrib.ulValueLen = ATTR_SPACE;
    myattribute->next = myattribute->prev = NULL;
    myattribute->freeAttr = PR_TRUE;
    myattribute->freeData = PR_FALSE;

    crv = sftkdb_GetAttributeValue(dbHandle, object->obj.handle,
                                   &myattribute->attrib, 1);

    /* attribute is bigger than our attribute space buffer, malloc it */
    if (crv == CKR_BUFFER_TOO_SMALL) {
        myattribute->attrib.pValue = NULL;
        crv = sftkdb_GetAttributeValue(dbHandle, object->obj.handle,
                                       &myattribute->attrib, 1);
        if (crv != CKR_OK) {
            goto loser;
        }
        myattribute->attrib.pValue = PORT_Alloc(myattribute->attrib.ulValueLen);
        if (myattribute->attrib.pValue == NULL) {
            crv = CKR_HOST_MEMORY;
            goto loser;
        }
        myattribute->freeData = PR_TRUE;
        crv = sftkdb_GetAttributeValue(dbHandle, object->obj.handle,
                                       &myattribute->attrib, 1);
    }
loser:
    if (dbHandle) {
        sftk_freeDB(dbHandle);
    }
    if (crv != CKR_OK) {
        if (myattribute) {
            myattribute->attrib.ulValueLen = 0;
            sftk_FreeAttribute(myattribute);
            myattribute = NULL;
        }
    }
    return myattribute;
}

/*
 * look up and attribute structure from a type and Object structure.
 * The returned attribute is referenced and needs to be freed when
 * it is no longer needed.
 */
SFTKAttribute *
sftk_FindAttribute(SFTKObject *object, CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *attribute;
    SFTKSessionObject *sessObject = sftk_narrowToSessionObject(object);

    if (sessObject == NULL) {
        return sftk_FindTokenAttribute(sftk_narrowToTokenObject(object), type);
    }

    PZ_Lock(sessObject->attributeLock);
    sftkqueue_find(attribute, type, sessObject->head, sessObject->hashSize);
    PZ_Unlock(sessObject->attributeLock);

    return (attribute);
}

/*
 * Take a buffer and it's length and return it's true size in bits;
 */
unsigned int
sftk_GetLengthInBits(unsigned char *buf, unsigned int bufLen)
{
    unsigned int size = bufLen * 8;
    unsigned int i;

    /* Get the real length in bytes */
    for (i = 0; i < bufLen; i++) {
        unsigned char c = *buf++;
        if (c != 0) {
            unsigned char m;
            for (m = 0x80; m > 0; m = m >> 1) {
                if ((c & m) != 0) {
                    break;
                }
                size--;
            }
            break;
        }
        size -= 8;
    }
    return size;
}

/*
 * Constrain a big num attribute. to size and padding
 * minLength means length of the object must be greater than equal to minLength
 * maxLength means length of the object must be less than equal to maxLength
 * minMultiple means that object length mod minMultiple must equal 0.
 * all input sizes are in bits.
 * if any constraint is '0' that constraint is not checked.
 */
CK_RV
sftk_ConstrainAttribute(SFTKObject *object, CK_ATTRIBUTE_TYPE type,
                        int minLength, int maxLength, int minMultiple)
{
    SFTKAttribute *attribute;
    int size;
    unsigned char *ptr;

    attribute = sftk_FindAttribute(object, type);
    if (!attribute) {
        return CKR_TEMPLATE_INCOMPLETE;
    }
    ptr = (unsigned char *)attribute->attrib.pValue;
    if (ptr == NULL) {
        sftk_FreeAttribute(attribute);
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    size = sftk_GetLengthInBits(ptr, attribute->attrib.ulValueLen);
    sftk_FreeAttribute(attribute);

    if ((minLength != 0) && (size < minLength)) {
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    if ((maxLength != 0) && (size > maxLength)) {
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    if ((minMultiple != 0) && ((size % minMultiple) != 0)) {
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }
    return CKR_OK;
}

PRBool
sftk_hasAttributeToken(SFTKTokenObject *object, CK_ATTRIBUTE_TYPE type)
{
    CK_ATTRIBUTE template;
    CK_RV crv;
    SFTKDBHandle *dbHandle;

    dbHandle = sftk_getDBForTokenObject(object->obj.slot, object->obj.handle);
    template.type = type;
    template.pValue = NULL;
    template.ulValueLen = 0;

    crv = sftkdb_GetAttributeValue(dbHandle, object->obj.handle, &template, 1);
    sftk_freeDB(dbHandle);

    /* attribute is bigger than our attribute space buffer, malloc it */
    return (crv == CKR_OK) ? PR_TRUE : PR_FALSE;
}

/*
 * return true if object has attribute
 */
PRBool
sftk_hasAttribute(SFTKObject *object, CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *attribute;
    SFTKSessionObject *sessObject = sftk_narrowToSessionObject(object);

    if (sessObject == NULL) {
        return sftk_hasAttributeToken(sftk_narrowToTokenObject(object), type);
    }

    PZ_Lock(sessObject->attributeLock);
    sftkqueue_find(attribute, type, sessObject->head, sessObject->hashSize);
    PZ_Unlock(sessObject->attributeLock);

    return (PRBool)(attribute != NULL);
}

/*
 * add an attribute to an object
 */
static void
sftk_AddAttribute(SFTKObject *object, SFTKAttribute *attribute)
{
    SFTKSessionObject *sessObject = sftk_narrowToSessionObject(object);

    if (sessObject == NULL)
        return;
    PZ_Lock(sessObject->attributeLock);
    sftkqueue_add(attribute, attribute->handle,
                  sessObject->head, sessObject->hashSize);
    PZ_Unlock(sessObject->attributeLock);
}

/*
 * copy an unsigned attribute into a SECItem. Secitem is allocated in
 * the specified arena.
 */
CK_RV
sftk_Attribute2SSecItem(PLArenaPool *arena, SECItem *item, SFTKObject *object,
                        CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *attribute;

    item->data = NULL;

    attribute = sftk_FindAttribute(object, type);
    if (attribute == NULL)
        return CKR_TEMPLATE_INCOMPLETE;

    (void)SECITEM_AllocItem(arena, item, attribute->attrib.ulValueLen);
    if (item->data == NULL) {
        sftk_FreeAttribute(attribute);
        return CKR_HOST_MEMORY;
    }
    PORT_Memcpy(item->data, attribute->attrib.pValue, item->len);
    sftk_FreeAttribute(attribute);
    return CKR_OK;
}

/*
 * fetch multiple attributes into  SECItems. Secitem data is allocated in
 * the specified arena.
 */
CK_RV
sftk_MultipleAttribute2SecItem(PLArenaPool *arena, SFTKObject *object,
                               SFTKItemTemplate *itemTemplate, int itemTemplateCount)
{

    CK_RV crv = CKR_OK;
    CK_ATTRIBUTE templateSpace[SFTK_MAX_ITEM_TEMPLATE];
    CK_ATTRIBUTE *template;
    SFTKTokenObject *tokObject;
    SFTKDBHandle *dbHandle = NULL;
    int i;

    tokObject = sftk_narrowToTokenObject(object);

    /* session objects, just loop through the list */
    if (tokObject == NULL) {
        for (i = 0; i < itemTemplateCount; i++) {
            crv = sftk_Attribute2SecItem(arena, itemTemplate[i].item, object,
                                         itemTemplate[i].type);
            if (crv != CKR_OK) {
                return crv;
            }
        }
        return CKR_OK;
    }

    /* don't do any work if none is required */
    if (itemTemplateCount == 0) {
        return CKR_OK;
    }

    /* don't allocate the template unless we need it */
    if (itemTemplateCount > SFTK_MAX_ITEM_TEMPLATE) {
        template = PORT_NewArray(CK_ATTRIBUTE, itemTemplateCount);
    } else {
        template = templateSpace;
    }

    if (template == NULL) {
        crv = CKR_HOST_MEMORY;
        goto loser;
    }

    dbHandle = sftk_getDBForTokenObject(object->slot, object->handle);
    if (dbHandle == NULL) {
        crv = CKR_OBJECT_HANDLE_INVALID;
        goto loser;
    }

    /* set up the PKCS #11 template */
    for (i = 0; i < itemTemplateCount; i++) {
        template[i].type = itemTemplate[i].type;
        template[i].pValue = NULL;
        template[i].ulValueLen = 0;
    }

    /* fetch the attribute lengths */
    crv = sftkdb_GetAttributeValue(dbHandle, object->handle,
                                   template, itemTemplateCount);
    if (crv != CKR_OK) {
        goto loser;
    }

    /* allocate space for the attributes */
    for (i = 0; i < itemTemplateCount; i++) {
        template[i].pValue = PORT_ArenaAlloc(arena, template[i].ulValueLen);
        if (template[i].pValue == NULL) {
            crv = CKR_HOST_MEMORY;
            goto loser;
        }
    }

    /* fetch the attributes */
    crv = sftkdb_GetAttributeValue(dbHandle, object->handle,
                                   template, itemTemplateCount);
    if (crv != CKR_OK) {
        goto loser;
    }

    /* Fill in the items */
    for (i = 0; i < itemTemplateCount; i++) {
        itemTemplate[i].item->data = template[i].pValue;
        itemTemplate[i].item->len = template[i].ulValueLen;
    }

loser:
    if (template != templateSpace) {
        PORT_Free(template);
    }
    if (dbHandle) {
        sftk_freeDB(dbHandle);
    }

    return crv;
}

/*
 * delete an attribute from an object
 */
static void
sftk_DeleteAttribute(SFTKObject *object, SFTKAttribute *attribute)
{
    SFTKSessionObject *sessObject = sftk_narrowToSessionObject(object);

    if (sessObject == NULL) {
        return;
    }
    PZ_Lock(sessObject->attributeLock);
    if (sftkqueue_is_queued(attribute, attribute->handle,
                            sessObject->head, sessObject->hashSize)) {
        sftkqueue_delete(attribute, attribute->handle,
                         sessObject->head, sessObject->hashSize);
    }
    PZ_Unlock(sessObject->attributeLock);
}

/*
 * this is only valid for CK_BBOOL type attributes. Return the state
 * of that attribute.
 */
PRBool
sftk_isTrue(SFTKObject *object, CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *attribute;
    PRBool tok = PR_FALSE;

    attribute = sftk_FindAttribute(object, type);
    if (attribute == NULL) {
        return PR_FALSE;
    }
    tok = (PRBool)(*(CK_BBOOL *)attribute->attrib.pValue);
    sftk_FreeAttribute(attribute);

    return tok;
}

/*
 * force an attribute to null.
 * this is for sensitive keys which are stored in the database, we don't
 * want to keep this info around in memory in the clear.
 */
void
sftk_nullAttribute(SFTKObject *object, CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *attribute;

    attribute = sftk_FindAttribute(object, type);
    if (attribute == NULL)
        return;

    if (attribute->attrib.pValue != NULL) {
        PORT_Memset(attribute->attrib.pValue, 0, attribute->attrib.ulValueLen);
        if (attribute->freeData) {
            PORT_Free(attribute->attrib.pValue);
        }
        attribute->freeData = PR_FALSE;
        attribute->attrib.pValue = NULL;
        attribute->attrib.ulValueLen = 0;
    }
    sftk_FreeAttribute(attribute);
}

static CK_RV
sftk_forceTokenAttribute(SFTKObject *object, CK_ATTRIBUTE_TYPE type,
                         const void *value, unsigned int len)
{
    CK_ATTRIBUTE attribute;
    SFTKDBHandle *dbHandle = NULL;
    SFTKTokenObject *to = sftk_narrowToTokenObject(object);
    CK_RV crv;

    PORT_Assert(to);
    if (to == NULL) {
        return CKR_DEVICE_ERROR;
    }

    dbHandle = sftk_getDBForTokenObject(object->slot, object->handle);

    attribute.type = type;
    attribute.pValue = (void *)value;
    attribute.ulValueLen = len;

    crv = sftkdb_SetAttributeValue(dbHandle, object, &attribute, 1);
    sftk_freeDB(dbHandle);
    return crv;
}

/*
 * force an attribute to a specifc value.
 */
CK_RV
sftk_forceAttribute(SFTKObject *object, CK_ATTRIBUTE_TYPE type,
                    const void *value, unsigned int len)
{
    SFTKAttribute *attribute;
    void *att_val = NULL;
    PRBool freeData = PR_FALSE;

    PORT_Assert(object);
    PORT_Assert(object->refCount);
    PORT_Assert(object->slot);
    if (!object ||
        !object->refCount ||
        !object->slot) {
        return CKR_DEVICE_ERROR;
    }
    if (sftk_isToken(object->handle)) {
        return sftk_forceTokenAttribute(object, type, value, len);
    }
    attribute = sftk_FindAttribute(object, type);
    if (attribute == NULL)
        return sftk_AddAttributeType(object, type, value, len);

    if (value) {
        if (len <= ATTR_SPACE) {
            att_val = attribute->space;
        } else {
            att_val = PORT_Alloc(len);
            freeData = PR_TRUE;
        }
        if (att_val == NULL) {
            return CKR_HOST_MEMORY;
        }
        if (attribute->attrib.pValue == att_val) {
            PORT_Memset(attribute->attrib.pValue, 0,
                        attribute->attrib.ulValueLen);
        }
        PORT_Memcpy(att_val, value, len);
    }
    if (attribute->attrib.pValue != NULL) {
        if (attribute->attrib.pValue != att_val) {
            PORT_Memset(attribute->attrib.pValue, 0,
                        attribute->attrib.ulValueLen);
        }
        if (attribute->freeData) {
            PORT_Free(attribute->attrib.pValue);
        }
        attribute->freeData = PR_FALSE;
        attribute->attrib.pValue = NULL;
        attribute->attrib.ulValueLen = 0;
    }
    if (att_val) {
        attribute->attrib.pValue = att_val;
        attribute->attrib.ulValueLen = len;
        attribute->freeData = freeData;
    }
    sftk_FreeAttribute(attribute);
    return CKR_OK;
}

/*
 * return a null terminated string from attribute 'type'. This string
 * is allocated and needs to be freed with PORT_Free() When complete.
 */
char *
sftk_getString(SFTKObject *object, CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *attribute;
    char *label = NULL;

    attribute = sftk_FindAttribute(object, type);
    if (attribute == NULL)
        return NULL;

    if (attribute->attrib.pValue != NULL) {
        label = (char *)PORT_Alloc(attribute->attrib.ulValueLen + 1);
        if (label == NULL) {
            sftk_FreeAttribute(attribute);
            return NULL;
        }

        PORT_Memcpy(label, attribute->attrib.pValue,
                    attribute->attrib.ulValueLen);
        label[attribute->attrib.ulValueLen] = 0;
    }
    sftk_FreeAttribute(attribute);
    return label;
}

/*
 * decode when a particular attribute may be modified
 *  SFTK_NEVER: This attribute must be set at object creation time and
 *  can never be modified.
 *  SFTK_ONCOPY: This attribute may be modified only when you copy the
 *  object.
 *  SFTK_SENSITIVE: The CKA_SENSITIVE attribute can only be changed from
 *  CK_FALSE to CK_TRUE.
 *  SFTK_ALWAYS: This attribute can always be modified.
 * Some attributes vary their modification type based on the class of the
 *   object.
 */
SFTKModifyType
sftk_modifyType(CK_ATTRIBUTE_TYPE type, CK_OBJECT_CLASS inClass)
{
    /* if we don't know about it, user user defined, always allow modify */
    SFTKModifyType mtype = SFTK_ALWAYS;

    switch (type) {
        /* NEVER */
        case CKA_CLASS:
        case CKA_CERTIFICATE_TYPE:
        case CKA_KEY_TYPE:
        case CKA_MODULUS:
        case CKA_MODULUS_BITS:
        case CKA_PUBLIC_EXPONENT:
        case CKA_PRIVATE_EXPONENT:
        case CKA_PRIME:
        case CKA_SUBPRIME:
        case CKA_BASE:
        case CKA_PRIME_1:
        case CKA_PRIME_2:
        case CKA_EXPONENT_1:
        case CKA_EXPONENT_2:
        case CKA_COEFFICIENT:
        case CKA_VALUE_LEN:
        case CKA_ALWAYS_SENSITIVE:
        case CKA_NEVER_EXTRACTABLE:
        case CKA_NETSCAPE_DB:
            mtype = SFTK_NEVER;
            break;

        /* ONCOPY */
        case CKA_TOKEN:
        case CKA_PRIVATE:
        case CKA_MODIFIABLE:
            mtype = SFTK_ONCOPY;
            break;

        /* SENSITIVE */
        case CKA_SENSITIVE:
        case CKA_EXTRACTABLE:
            mtype = SFTK_SENSITIVE;
            break;

        /* ALWAYS */
        case CKA_LABEL:
        case CKA_APPLICATION:
        case CKA_ID:
        case CKA_SERIAL_NUMBER:
        case CKA_START_DATE:
        case CKA_END_DATE:
        case CKA_DERIVE:
        case CKA_ENCRYPT:
        case CKA_DECRYPT:
        case CKA_SIGN:
        case CKA_VERIFY:
        case CKA_SIGN_RECOVER:
        case CKA_VERIFY_RECOVER:
        case CKA_WRAP:
        case CKA_UNWRAP:
            mtype = SFTK_ALWAYS;
            break;

        /* DEPENDS ON CLASS */
        case CKA_VALUE:
            mtype = (inClass == CKO_DATA) ? SFTK_ALWAYS : SFTK_NEVER;
            break;

        case CKA_SUBJECT:
            mtype = (inClass == CKO_CERTIFICATE) ? SFTK_NEVER : SFTK_ALWAYS;
            break;
        default:
            break;
    }
    return mtype;
}

/* decode if a particular attribute is sensitive (cannot be read
 * back to the user of if the object is set to SENSITIVE) */
PRBool
sftk_isSensitive(CK_ATTRIBUTE_TYPE type, CK_OBJECT_CLASS inClass)
{
    switch (type) {
        /* ALWAYS */
        case CKA_PRIVATE_EXPONENT:
        case CKA_PRIME_1:
        case CKA_PRIME_2:
        case CKA_EXPONENT_1:
        case CKA_EXPONENT_2:
        case CKA_COEFFICIENT:
            return PR_TRUE;

        /* DEPENDS ON CLASS */
        case CKA_VALUE:
            /* PRIVATE and SECRET KEYS have SENSITIVE values */
            return (PRBool)((inClass == CKO_PRIVATE_KEY) || (inClass == CKO_SECRET_KEY));

        default:
            break;
    }
    return PR_FALSE;
}

/*
 * copy an attribute into a SECItem. Secitem is allocated in the specified
 * arena.
 */
CK_RV
sftk_Attribute2SecItem(PLArenaPool *arena, SECItem *item, SFTKObject *object,
                       CK_ATTRIBUTE_TYPE type)
{
    int len;
    SFTKAttribute *attribute;

    attribute = sftk_FindAttribute(object, type);
    if (attribute == NULL)
        return CKR_TEMPLATE_INCOMPLETE;
    len = attribute->attrib.ulValueLen;

    if (arena) {
        item->data = (unsigned char *)PORT_ArenaAlloc(arena, len);
    } else {
        item->data = (unsigned char *)PORT_Alloc(len);
    }
    if (item->data == NULL) {
        sftk_FreeAttribute(attribute);
        return CKR_HOST_MEMORY;
    }
    item->len = len;
    PORT_Memcpy(item->data, attribute->attrib.pValue, len);
    sftk_FreeAttribute(attribute);
    return CKR_OK;
}

CK_RV
sftk_GetULongAttribute(SFTKObject *object, CK_ATTRIBUTE_TYPE type,
                       CK_ULONG *longData)
{
    SFTKAttribute *attribute;

    attribute = sftk_FindAttribute(object, type);
    if (attribute == NULL)
        return CKR_TEMPLATE_INCOMPLETE;

    if (attribute->attrib.ulValueLen != sizeof(CK_ULONG)) {
        return CKR_ATTRIBUTE_VALUE_INVALID;
    }

    *longData = *(CK_ULONG *)attribute->attrib.pValue;
    sftk_FreeAttribute(attribute);
    return CKR_OK;
}

void
sftk_DeleteAttributeType(SFTKObject *object, CK_ATTRIBUTE_TYPE type)
{
    SFTKAttribute *attribute;
    attribute = sftk_FindAttribute(object, type);
    if (attribute == NULL)
        return;
    sftk_DeleteAttribute(object, attribute);
    sftk_FreeAttribute(attribute);
}

CK_RV
sftk_AddAttributeType(SFTKObject *object, CK_ATTRIBUTE_TYPE type,
                      const void *valPtr, CK_ULONG length)
{
    SFTKAttribute *attribute;
    attribute = sftk_NewAttribute(object, type, valPtr, length);
    if (attribute == NULL) {
        return CKR_HOST_MEMORY;
    }
    sftk_AddAttribute(object, attribute);
    return CKR_OK;
}

/*
 * ******************** Object Utilities *******************************
 */

/* must be called holding sftk_tokenKeyLock(slot) */
static SECItem *
sftk_lookupTokenKeyByHandle(SFTKSlot *slot, CK_OBJECT_HANDLE handle)
{
    return (SECItem *)PL_HashTableLookup(slot->tokObjHashTable, (void *)handle);
}

/*
 * use the refLock. This operations should be very rare, so the added
 * contention on the ref lock should be lower than the overhead of adding
 * a new lock. We use separate functions for this just in case I'm wrong.
 */
static void
sftk_tokenKeyLock(SFTKSlot *slot)
{
    SKIP_AFTER_FORK(PZ_Lock(slot->objectLock));
}

static void
sftk_tokenKeyUnlock(SFTKSlot *slot)
{
    SKIP_AFTER_FORK(PZ_Unlock(slot->objectLock));
}

static PRIntn
sftk_freeHashItem(PLHashEntry *entry, PRIntn index, void *arg)
{
    SECItem *item = (SECItem *)entry->value;

    SECITEM_FreeItem(item, PR_TRUE);
    return HT_ENUMERATE_NEXT;
}

CK_RV
SFTK_ClearTokenKeyHashTable(SFTKSlot *slot)
{
    sftk_tokenKeyLock(slot);
    PORT_Assert(!slot->present);
    PL_HashTableEnumerateEntries(slot->tokObjHashTable, sftk_freeHashItem, NULL);
    sftk_tokenKeyUnlock(slot);
    return CKR_OK;
}

/* allocation hooks that allow us to recycle old object structures */
static SFTKObjectFreeList sessionObjectList = { NULL, NULL, 0 };
static SFTKObjectFreeList tokenObjectList = { NULL, NULL, 0 };

SFTKObject *
sftk_GetObjectFromList(PRBool *hasLocks, PRBool optimizeSpace,
                       SFTKObjectFreeList *list, unsigned int hashSize, PRBool isSessionObject)
{
    SFTKObject *object;
    int size = 0;

    if (!optimizeSpace) {
        PZ_Lock(list->lock);
        object = list->head;
        if (object) {
            list->head = object->next;
            list->count--;
        }
        PZ_Unlock(list->lock);
        if (object) {
            object->next = object->prev = NULL;
            *hasLocks = PR_TRUE;
            return object;
        }
    }
    size = isSessionObject ? sizeof(SFTKSessionObject) + hashSize * sizeof(SFTKAttribute *) : sizeof(SFTKTokenObject);

    object = (SFTKObject *)PORT_ZAlloc(size);
    if (isSessionObject && object) {
        ((SFTKSessionObject *)object)->hashSize = hashSize;
    }
    *hasLocks = PR_FALSE;
    return object;
}

static void
sftk_PutObjectToList(SFTKObject *object, SFTKObjectFreeList *list,
                     PRBool isSessionObject)
{

    /* the code below is equivalent to :
     *     optimizeSpace = isSessionObject ? object->optimizeSpace : PR_FALSE;
     * just faster.
     */
    PRBool optimizeSpace = isSessionObject &&
                           ((SFTKSessionObject *)object)->optimizeSpace;
    if (object->refLock && !optimizeSpace && (list->count < MAX_OBJECT_LIST_SIZE)) {
        PZ_Lock(list->lock);
        object->next = list->head;
        list->head = object;
        list->count++;
        PZ_Unlock(list->lock);
        return;
    }
    if (isSessionObject) {
        SFTKSessionObject *so = (SFTKSessionObject *)object;
        PZ_DestroyLock(so->attributeLock);
        so->attributeLock = NULL;
    }
    if (object->refLock) {
        PZ_DestroyLock(object->refLock);
        object->refLock = NULL;
    }
    PORT_Free(object);
}

static SFTKObject *
sftk_freeObjectData(SFTKObject *object)
{
    SFTKObject *next = object->next;

    PORT_Free(object);
    return next;
}

static void
sftk_InitFreeList(SFTKObjectFreeList *list)
{
    if (!list->lock) {
        list->lock = PZ_NewLock(nssILockObject);
    }
}

void
sftk_InitFreeLists(void)
{
    sftk_InitFreeList(&sessionObjectList);
    sftk_InitFreeList(&tokenObjectList);
}

static void
sftk_CleanupFreeList(SFTKObjectFreeList *list, PRBool isSessionList)
{
    SFTKObject *object;

    if (!list->lock) {
        return;
    }
    SKIP_AFTER_FORK(PZ_Lock(list->lock));
    for (object = list->head; object != NULL;
         object = sftk_freeObjectData(object)) {
        PZ_DestroyLock(object->refLock);
        if (isSessionList) {
            PZ_DestroyLock(((SFTKSessionObject *)object)->attributeLock);
        }
    }
    list->count = 0;
    list->head = NULL;
    SKIP_AFTER_FORK(PZ_Unlock(list->lock));
    SKIP_AFTER_FORK(PZ_DestroyLock(list->lock));
    list->lock = NULL;
}

void
sftk_CleanupFreeLists(void)
{
    sftk_CleanupFreeList(&sessionObjectList, PR_TRUE);
    sftk_CleanupFreeList(&tokenObjectList, PR_FALSE);
}

/*
 * Create a new object
 */
SFTKObject *
sftk_NewObject(SFTKSlot *slot)
{
    SFTKObject *object;
    SFTKSessionObject *sessObject;
    PRBool hasLocks = PR_FALSE;
    unsigned int i;
    unsigned int hashSize = 0;

    hashSize = (slot->optimizeSpace) ? SPACE_ATTRIBUTE_HASH_SIZE : TIME_ATTRIBUTE_HASH_SIZE;

    object = sftk_GetObjectFromList(&hasLocks, slot->optimizeSpace,
                                    &sessionObjectList, hashSize, PR_TRUE);
    if (object == NULL) {
        return NULL;
    }
    sessObject = (SFTKSessionObject *)object;
    sessObject->nextAttr = 0;

    for (i = 0; i < MAX_OBJS_ATTRS; i++) {
        sessObject->attrList[i].attrib.pValue = NULL;
        sessObject->attrList[i].freeData = PR_FALSE;
    }
    sessObject->optimizeSpace = slot->optimizeSpace;

    object->handle = 0;
    object->next = object->prev = NULL;
    object->slot = slot;

    object->refCount = 1;
    sessObject->sessionList.next = NULL;
    sessObject->sessionList.prev = NULL;
    sessObject->sessionList.parent = object;
    sessObject->session = NULL;
    sessObject->wasDerived = PR_FALSE;
    if (!hasLocks)
        object->refLock = PZ_NewLock(nssILockRefLock);
    if (object->refLock == NULL) {
        PORT_Free(object);
        return NULL;
    }
    if (!hasLocks)
        sessObject->attributeLock = PZ_NewLock(nssILockAttribute);
    if (sessObject->attributeLock == NULL) {
        PZ_DestroyLock(object->refLock);
        PORT_Free(object);
        return NULL;
    }
    for (i = 0; i < sessObject->hashSize; i++) {
        sessObject->head[i] = NULL;
    }
    object->objectInfo = NULL;
    object->infoFree = NULL;
    return object;
}

static CK_RV
sftk_DestroySessionObjectData(SFTKSessionObject *so)
{
    int i;

    for (i = 0; i < MAX_OBJS_ATTRS; i++) {
        unsigned char *value = so->attrList[i].attrib.pValue;
        if (value) {
            PORT_Memset(value, 0, so->attrList[i].attrib.ulValueLen);
            if (so->attrList[i].freeData) {
                PORT_Free(value);
            }
            so->attrList[i].attrib.pValue = NULL;
            so->attrList[i].freeData = PR_FALSE;
        }
    }
    /*  PZ_DestroyLock(so->attributeLock);*/
    return CKR_OK;
}

/*
 * free all the data associated with an object. Object reference count must
 * be 'zero'.
 */
static CK_RV
sftk_DestroyObject(SFTKObject *object)
{
    CK_RV crv = CKR_OK;
    SFTKSessionObject *so = sftk_narrowToSessionObject(object);
    SFTKTokenObject *to = sftk_narrowToTokenObject(object);

    PORT_Assert(object->refCount == 0);

    /* delete the database value */
    if (to) {
        if (to->dbKey.data) {
            PORT_Free(to->dbKey.data);
            to->dbKey.data = NULL;
        }
    }
    if (so) {
        sftk_DestroySessionObjectData(so);
    }
    if (object->objectInfo) {
        (*object->infoFree)(object->objectInfo);
        object->objectInfo = NULL;
        object->infoFree = NULL;
    }
    if (so) {
        sftk_PutObjectToList(object, &sessionObjectList, PR_TRUE);
    } else {
        sftk_PutObjectToList(object, &tokenObjectList, PR_FALSE);
    }
    return crv;
}

void
sftk_ReferenceObject(SFTKObject *object)
{
    PZ_Lock(object->refLock);
    object->refCount++;
    PZ_Unlock(object->refLock);
}

static SFTKObject *
sftk_ObjectFromHandleOnSlot(CK_OBJECT_HANDLE handle, SFTKSlot *slot)
{
    SFTKObject *object;
    PRUint32 index = sftk_hash(handle, slot->sessObjHashSize);

    if (sftk_isToken(handle)) {
        return sftk_NewTokenObject(slot, NULL, handle);
    }

    PZ_Lock(slot->objectLock);
    sftkqueue_find2(object, handle, index, slot->sessObjHashTable);
    if (object) {
        sftk_ReferenceObject(object);
    }
    PZ_Unlock(slot->objectLock);

    return (object);
}
/*
 * look up and object structure from a handle. OBJECT_Handles only make
 * sense in terms of a given session.  make a reference to that object
 * structure returned.
 */
SFTKObject *
sftk_ObjectFromHandle(CK_OBJECT_HANDLE handle, SFTKSession *session)
{
    SFTKSlot *slot = sftk_SlotFromSession(session);

    return sftk_ObjectFromHandleOnSlot(handle, slot);
}

/*
 * release a reference to an object handle
 */
SFTKFreeStatus
sftk_FreeObject(SFTKObject *object)
{
    PRBool destroy = PR_FALSE;
    CK_RV crv;

    PZ_Lock(object->refLock);
    if (object->refCount == 1)
        destroy = PR_TRUE;
    object->refCount--;
    PZ_Unlock(object->refLock);

    if (destroy) {
        crv = sftk_DestroyObject(object);
        if (crv != CKR_OK) {
            return SFTK_DestroyFailure;
        }
        return SFTK_Destroyed;
    }
    return SFTK_Busy;
}

/*
 * add an object to a slot and session queue. These two functions
 * adopt the object.
 */
void
sftk_AddSlotObject(SFTKSlot *slot, SFTKObject *object)
{
    PRUint32 index = sftk_hash(object->handle, slot->sessObjHashSize);
    sftkqueue_init_element(object);
    PZ_Lock(slot->objectLock);
    sftkqueue_add2(object, object->handle, index, slot->sessObjHashTable);
    PZ_Unlock(slot->objectLock);
}

void
sftk_AddObject(SFTKSession *session, SFTKObject *object)
{
    SFTKSlot *slot = sftk_SlotFromSession(session);
    SFTKSessionObject *so = sftk_narrowToSessionObject(object);

    if (so) {
        PZ_Lock(session->objectLock);
        sftkqueue_add(&so->sessionList, 0, session->objects, 0);
        so->session = session;
        PZ_Unlock(session->objectLock);
    }
    sftk_AddSlotObject(slot, object);
    sftk_ReferenceObject(object);
}

/*
 * delete an object from a slot and session queue
 */
CK_RV
sftk_DeleteObject(SFTKSession *session, SFTKObject *object)
{
    SFTKSlot *slot = sftk_SlotFromSession(session);
    SFTKSessionObject *so = sftk_narrowToSessionObject(object);
    CK_RV crv = CKR_OK;
    PRUint32 index = sftk_hash(object->handle, slot->sessObjHashSize);

    /* Handle Token case */
    if (so && so->session) {
        session = so->session;
        PZ_Lock(session->objectLock);
        sftkqueue_delete(&so->sessionList, 0, session->objects, 0);
        PZ_Unlock(session->objectLock);
        PZ_Lock(slot->objectLock);
        sftkqueue_delete2(object, object->handle, index, slot->sessObjHashTable);
        PZ_Unlock(slot->objectLock);
        sftkqueue_clear_deleted_element(object);
        sftk_FreeObject(object); /* free the reference owned by the queue */
    } else {
        SFTKDBHandle *handle = sftk_getDBForTokenObject(slot, object->handle);
#ifdef DEBUG
        SFTKTokenObject *to = sftk_narrowToTokenObject(object);
        PORT_Assert(to);
#endif
        crv = sftkdb_DestroyObject(handle, object->handle);
        sftk_freeDB(handle);
    }
    return crv;
}

/*
 * Token objects don't explicitly store their attributes, so we need to know
 * what attributes make up a particular token object before we can copy it.
 * below are the tables by object type.
 */
static const CK_ATTRIBUTE_TYPE commonAttrs[] = {
    CKA_CLASS, CKA_TOKEN, CKA_PRIVATE, CKA_LABEL, CKA_MODIFIABLE
};
static const CK_ULONG commonAttrsCount =
    sizeof(commonAttrs) / sizeof(commonAttrs[0]);

static const CK_ATTRIBUTE_TYPE commonKeyAttrs[] = {
    CKA_ID, CKA_START_DATE, CKA_END_DATE, CKA_DERIVE, CKA_LOCAL, CKA_KEY_TYPE
};
static const CK_ULONG commonKeyAttrsCount =
    sizeof(commonKeyAttrs) / sizeof(commonKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE secretKeyAttrs[] = {
    CKA_SENSITIVE, CKA_EXTRACTABLE, CKA_ENCRYPT, CKA_DECRYPT, CKA_SIGN,
    CKA_VERIFY, CKA_WRAP, CKA_UNWRAP, CKA_VALUE
};
static const CK_ULONG secretKeyAttrsCount =
    sizeof(secretKeyAttrs) / sizeof(secretKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE commonPubKeyAttrs[] = {
    CKA_ENCRYPT, CKA_VERIFY, CKA_VERIFY_RECOVER, CKA_WRAP, CKA_SUBJECT
};
static const CK_ULONG commonPubKeyAttrsCount =
    sizeof(commonPubKeyAttrs) / sizeof(commonPubKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE rsaPubKeyAttrs[] = {
    CKA_MODULUS, CKA_PUBLIC_EXPONENT
};
static const CK_ULONG rsaPubKeyAttrsCount =
    sizeof(rsaPubKeyAttrs) / sizeof(rsaPubKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE dsaPubKeyAttrs[] = {
    CKA_SUBPRIME, CKA_PRIME, CKA_BASE, CKA_VALUE
};
static const CK_ULONG dsaPubKeyAttrsCount =
    sizeof(dsaPubKeyAttrs) / sizeof(dsaPubKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE dhPubKeyAttrs[] = {
    CKA_PRIME, CKA_BASE, CKA_VALUE
};
static const CK_ULONG dhPubKeyAttrsCount =
    sizeof(dhPubKeyAttrs) / sizeof(dhPubKeyAttrs[0]);
static const CK_ATTRIBUTE_TYPE ecPubKeyAttrs[] = {
    CKA_EC_PARAMS, CKA_EC_POINT
};
static const CK_ULONG ecPubKeyAttrsCount =
    sizeof(ecPubKeyAttrs) / sizeof(ecPubKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE commonPrivKeyAttrs[] = {
    CKA_DECRYPT, CKA_SIGN, CKA_SIGN_RECOVER, CKA_UNWRAP, CKA_SUBJECT,
    CKA_SENSITIVE, CKA_EXTRACTABLE, CKA_NETSCAPE_DB, CKA_PUBLIC_KEY_INFO
};
static const CK_ULONG commonPrivKeyAttrsCount =
    sizeof(commonPrivKeyAttrs) / sizeof(commonPrivKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE rsaPrivKeyAttrs[] = {
    CKA_MODULUS, CKA_PUBLIC_EXPONENT, CKA_PRIVATE_EXPONENT,
    CKA_PRIME_1, CKA_PRIME_2, CKA_EXPONENT_1, CKA_EXPONENT_2, CKA_COEFFICIENT
};
static const CK_ULONG rsaPrivKeyAttrsCount =
    sizeof(rsaPrivKeyAttrs) / sizeof(rsaPrivKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE dsaPrivKeyAttrs[] = {
    CKA_SUBPRIME, CKA_PRIME, CKA_BASE, CKA_VALUE
};
static const CK_ULONG dsaPrivKeyAttrsCount =
    sizeof(dsaPrivKeyAttrs) / sizeof(dsaPrivKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE dhPrivKeyAttrs[] = {
    CKA_PRIME, CKA_BASE, CKA_VALUE
};
static const CK_ULONG dhPrivKeyAttrsCount =
    sizeof(dhPrivKeyAttrs) / sizeof(dhPrivKeyAttrs[0]);
static const CK_ATTRIBUTE_TYPE ecPrivKeyAttrs[] = {
    CKA_EC_PARAMS, CKA_VALUE
};
static const CK_ULONG ecPrivKeyAttrsCount =
    sizeof(ecPrivKeyAttrs) / sizeof(ecPrivKeyAttrs[0]);

static const CK_ATTRIBUTE_TYPE certAttrs[] = {
    CKA_CERTIFICATE_TYPE, CKA_VALUE, CKA_SUBJECT, CKA_ISSUER, CKA_SERIAL_NUMBER
};
static const CK_ULONG certAttrsCount =
    sizeof(certAttrs) / sizeof(certAttrs[0]);

static const CK_ATTRIBUTE_TYPE trustAttrs[] = {
    CKA_ISSUER, CKA_SERIAL_NUMBER, CKA_CERT_SHA1_HASH, CKA_CERT_MD5_HASH,
    CKA_TRUST_SERVER_AUTH, CKA_TRUST_CLIENT_AUTH, CKA_TRUST_EMAIL_PROTECTION,
    CKA_TRUST_CODE_SIGNING, CKA_TRUST_STEP_UP_APPROVED
};
static const CK_ULONG trustAttrsCount =
    sizeof(trustAttrs) / sizeof(trustAttrs[0]);

static const CK_ATTRIBUTE_TYPE smimeAttrs[] = {
    CKA_SUBJECT, CKA_NETSCAPE_EMAIL, CKA_NETSCAPE_SMIME_TIMESTAMP, CKA_VALUE
};
static const CK_ULONG smimeAttrsCount =
    sizeof(smimeAttrs) / sizeof(smimeAttrs[0]);

static const CK_ATTRIBUTE_TYPE crlAttrs[] = {
    CKA_SUBJECT, CKA_VALUE, CKA_NETSCAPE_URL, CKA_NETSCAPE_KRL
};
static const CK_ULONG crlAttrsCount =
    sizeof(crlAttrs) / sizeof(crlAttrs[0]);

/* copy an object based on it's table */
CK_RV
stfk_CopyTokenAttributes(SFTKObject *destObject, SFTKTokenObject *src_to,
                         const CK_ATTRIBUTE_TYPE *attrArray, CK_ULONG attrCount)
{
    SFTKAttribute *attribute;
    SFTKAttribute *newAttribute;
    CK_RV crv = CKR_OK;
    unsigned int i;

    for (i = 0; i < attrCount; i++) {
        if (!sftk_hasAttribute(destObject, attrArray[i])) {
            attribute = sftk_FindAttribute(&src_to->obj, attrArray[i]);
            if (!attribute) {
                continue; /* return CKR_ATTRIBUTE_VALUE_INVALID; */
            }
            /* we need to copy the attribute since each attribute
             * only has one set of link list pointers */
            newAttribute = sftk_NewAttribute(destObject,
                                             sftk_attr_expand(&attribute->attrib));
            sftk_FreeAttribute(attribute); /* free the old attribute */
            if (!newAttribute) {
                return CKR_HOST_MEMORY;
            }
            sftk_AddAttribute(destObject, newAttribute);
        }
    }
    return crv;
}

CK_RV
stfk_CopyTokenPrivateKey(SFTKObject *destObject, SFTKTokenObject *src_to)
{
    CK_RV crv;
    CK_KEY_TYPE key_type;
    SFTKAttribute *attribute;

    /* copy the common attributes for all keys first */
    crv = stfk_CopyTokenAttributes(destObject, src_to, commonKeyAttrs,
                                   commonKeyAttrsCount);
    if (crv != CKR_OK) {
        goto fail;
    }
    /* copy the common attributes for all private keys next */
    crv = stfk_CopyTokenAttributes(destObject, src_to, commonPrivKeyAttrs,
                                   commonPrivKeyAttrsCount);
    if (crv != CKR_OK) {
        goto fail;
    }
    attribute = sftk_FindAttribute(&src_to->obj, CKA_KEY_TYPE);
    PORT_Assert(attribute); /* if it wasn't here, ww should have failed
                 * copying the common attributes */
    if (!attribute) {
        /* OK, so CKR_ATTRIBUTE_VALUE_INVALID is the immediate error, but
         * the fact is, the only reason we couldn't get the attribute would
         * be a memory error or database error (an error in the 'device').
         * if we have a database error code, we could return it here */
        crv = CKR_DEVICE_ERROR;
        goto fail;
    }
    key_type = *(CK_KEY_TYPE *)attribute->attrib.pValue;
    sftk_FreeAttribute(attribute);

    /* finally copy the attributes for various private key types */
    switch (key_type) {
        case CKK_RSA:
            crv = stfk_CopyTokenAttributes(destObject, src_to, rsaPrivKeyAttrs,
                                           rsaPrivKeyAttrsCount);
            break;
        case CKK_DSA:
            crv = stfk_CopyTokenAttributes(destObject, src_to, dsaPrivKeyAttrs,
                                           dsaPrivKeyAttrsCount);
            break;
        case CKK_DH:
            crv = stfk_CopyTokenAttributes(destObject, src_to, dhPrivKeyAttrs,
                                           dhPrivKeyAttrsCount);
            break;
        case CKK_EC:
            crv = stfk_CopyTokenAttributes(destObject, src_to, ecPrivKeyAttrs,
                                           ecPrivKeyAttrsCount);
            break;
        default:
            crv = CKR_DEVICE_ERROR; /* shouldn't happen unless we store more types
                                     * of token keys into our database. */
    }
fail:
    return crv;
}

CK_RV
stfk_CopyTokenPublicKey(SFTKObject *destObject, SFTKTokenObject *src_to)
{
    CK_RV crv;
    CK_KEY_TYPE key_type;
    SFTKAttribute *attribute;

    /* copy the common attributes for all keys first */
    crv = stfk_CopyTokenAttributes(destObject, src_to, commonKeyAttrs,
                                   commonKeyAttrsCount);
    if (crv != CKR_OK) {
        goto fail;
    }

    /* copy the common attributes for all public keys next */
    crv = stfk_CopyTokenAttributes(destObject, src_to, commonPubKeyAttrs,
                                   commonPubKeyAttrsCount);
    if (crv != CKR_OK) {
        goto fail;
    }
    attribute = sftk_FindAttribute(&src_to->obj, CKA_KEY_TYPE);
    PORT_Assert(attribute); /* if it wasn't here, ww should have failed
                 * copying the common attributes */
    if (!attribute) {
        /* OK, so CKR_ATTRIBUTE_VALUE_INVALID is the immediate error, but
         * the fact is, the only reason we couldn't get the attribute would
         * be a memory error or database error (an error in the 'device').
         * if we have a database error code, we could return it here */
        crv = CKR_DEVICE_ERROR;
        goto fail;
    }
    key_type = *(CK_KEY_TYPE *)attribute->attrib.pValue;
    sftk_FreeAttribute(attribute);

    /* finally copy the attributes for various public key types */
    switch (key_type) {
        case CKK_RSA:
            crv = stfk_CopyTokenAttributes(destObject, src_to, rsaPubKeyAttrs,
                                           rsaPubKeyAttrsCount);
            break;
        case CKK_DSA:
            crv = stfk_CopyTokenAttributes(destObject, src_to, dsaPubKeyAttrs,
                                           dsaPubKeyAttrsCount);
            break;
        case CKK_DH:
            crv = stfk_CopyTokenAttributes(destObject, src_to, dhPubKeyAttrs,
                                           dhPubKeyAttrsCount);
            break;
        case CKK_EC:
            crv = stfk_CopyTokenAttributes(destObject, src_to, ecPubKeyAttrs,
                                           ecPubKeyAttrsCount);
            break;
        default:
            crv = CKR_DEVICE_ERROR; /* shouldn't happen unless we store more types
                                     * of token keys into our database. */
    }
fail:
    return crv;
}
CK_RV
stfk_CopyTokenSecretKey(SFTKObject *destObject, SFTKTokenObject *src_to)
{
    CK_RV crv;
    crv = stfk_CopyTokenAttributes(destObject, src_to, commonKeyAttrs,
                                   commonKeyAttrsCount);
    if (crv != CKR_OK) {
        goto fail;
    }
    crv = stfk_CopyTokenAttributes(destObject, src_to, secretKeyAttrs,
                                   secretKeyAttrsCount);
fail:
    return crv;
}

/*
 * Copy a token object. We need to explicitly copy the relevant
 * attributes since token objects don't store those attributes in
 * the token itself.
 */
CK_RV
sftk_CopyTokenObject(SFTKObject *destObject, SFTKObject *srcObject)
{
    SFTKTokenObject *src_to = sftk_narrowToTokenObject(srcObject);
    CK_RV crv;

    PORT_Assert(src_to);
    if (src_to == NULL) {
        return CKR_DEVICE_ERROR; /* internal state inconsistant */
    }

    crv = stfk_CopyTokenAttributes(destObject, src_to, commonAttrs,
                                   commonAttrsCount);
    if (crv != CKR_OK) {
        goto fail;
    }
    switch (src_to->obj.objclass) {
        case CKO_CERTIFICATE:
            crv = stfk_CopyTokenAttributes(destObject, src_to, certAttrs,
                                           certAttrsCount);
            break;
        case CKO_NETSCAPE_TRUST:
            crv = stfk_CopyTokenAttributes(destObject, src_to, trustAttrs,
                                           trustAttrsCount);
            break;
        case CKO_NETSCAPE_SMIME:
            crv = stfk_CopyTokenAttributes(destObject, src_to, smimeAttrs,
                                           smimeAttrsCount);
            break;
        case CKO_NETSCAPE_CRL:
            crv = stfk_CopyTokenAttributes(destObject, src_to, crlAttrs,
                                           crlAttrsCount);
            break;
        case CKO_PRIVATE_KEY:
            crv = stfk_CopyTokenPrivateKey(destObject, src_to);
            break;
        case CKO_PUBLIC_KEY:
            crv = stfk_CopyTokenPublicKey(destObject, src_to);
            break;
        case CKO_SECRET_KEY:
            crv = stfk_CopyTokenSecretKey(destObject, src_to);
            break;
        default:
            crv = CKR_DEVICE_ERROR; /* shouldn't happen unless we store more types
                                     * of token keys into our database. */
    }
fail:
    return crv;
}

/*
 * copy the attributes from one object to another. Don't overwrite existing
 * attributes. NOTE: This is a pretty expensive operation since it
 * grabs the attribute locks for the src object for a *long* time.
 */
CK_RV
sftk_CopyObject(SFTKObject *destObject, SFTKObject *srcObject)
{
    SFTKAttribute *attribute;
    SFTKSessionObject *src_so = sftk_narrowToSessionObject(srcObject);
    unsigned int i;

    if (src_so == NULL) {
        return sftk_CopyTokenObject(destObject, srcObject);
    }

    PZ_Lock(src_so->attributeLock);
    for (i = 0; i < src_so->hashSize; i++) {
        attribute = src_so->head[i];
        do {
            if (attribute) {
                if (!sftk_hasAttribute(destObject, attribute->handle)) {
                    /* we need to copy the attribute since each attribute
                     * only has one set of link list pointers */
                    SFTKAttribute *newAttribute = sftk_NewAttribute(
                        destObject, sftk_attr_expand(&attribute->attrib));
                    if (newAttribute == NULL) {
                        PZ_Unlock(src_so->attributeLock);
                        return CKR_HOST_MEMORY;
                    }
                    sftk_AddAttribute(destObject, newAttribute);
                }
                attribute = attribute->next;
            }
        } while (attribute != NULL);
    }
    PZ_Unlock(src_so->attributeLock);

    return CKR_OK;
}

/*
 * ******************** Search Utilities *******************************
 */

/* add an object to a search list */
CK_RV
AddToList(SFTKObjectListElement **list, SFTKObject *object)
{
    SFTKObjectListElement *newElem =
        (SFTKObjectListElement *)PORT_Alloc(sizeof(SFTKObjectListElement));

    if (newElem == NULL)
        return CKR_HOST_MEMORY;

    newElem->next = *list;
    newElem->object = object;
    sftk_ReferenceObject(object);

    *list = newElem;
    return CKR_OK;
}

/* return true if the object matches the template */
PRBool
sftk_objectMatch(SFTKObject *object, CK_ATTRIBUTE_PTR theTemplate, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        SFTKAttribute *attribute = sftk_FindAttribute(object, theTemplate[i].type);
        if (attribute == NULL) {
            return PR_FALSE;
        }
        if (attribute->attrib.ulValueLen == theTemplate[i].ulValueLen) {
            if (PORT_Memcmp(attribute->attrib.pValue, theTemplate[i].pValue,
                            theTemplate[i].ulValueLen) == 0) {
                sftk_FreeAttribute(attribute);
                continue;
            }
        }
        sftk_FreeAttribute(attribute);
        return PR_FALSE;
    }
    return PR_TRUE;
}

/* search through all the objects in the queue and return the template matches
 * in the object list.
 */
CK_RV
sftk_searchObjectList(SFTKSearchResults *search, SFTKObject **head,
                      unsigned int size, PZLock *lock, CK_ATTRIBUTE_PTR theTemplate,
                      int count, PRBool isLoggedIn)
{
    unsigned int i;
    SFTKObject *object;
    CK_RV crv = CKR_OK;

    PZ_Lock(lock);
    for (i = 0; i < size; i++) {
        for (object = head[i]; object != NULL; object = object->next) {
            if (sftk_objectMatch(object, theTemplate, count)) {
                /* don't return objects that aren't yet visible */
                if ((!isLoggedIn) && sftk_isTrue(object, CKA_PRIVATE))
                    continue;
                sftk_addHandle(search, object->handle);
            }
        }
    }
    PZ_Unlock(lock);
    return crv;
}

/*
 * free a single list element. Return the Next object in the list.
 */
SFTKObjectListElement *
sftk_FreeObjectListElement(SFTKObjectListElement *objectList)
{
    SFTKObjectListElement *ol = objectList->next;

    sftk_FreeObject(objectList->object);
    PORT_Free(objectList);
    return ol;
}

/* free an entire object list */
void
sftk_FreeObjectList(SFTKObjectListElement *objectList)
{
    SFTKObjectListElement *ol;

    for (ol = objectList; ol != NULL; ol = sftk_FreeObjectListElement(ol)) {
    }
}

/*
 * free a search structure
 */
void
sftk_FreeSearch(SFTKSearchResults *search)
{
    if (search->handles) {
        PORT_Free(search->handles);
    }
    PORT_Free(search);
}

/*
 * ******************** Session Utilities *******************************
 */

/* update the sessions state based in it's flags and wether or not it's
 * logged in */
void
sftk_update_state(SFTKSlot *slot, SFTKSession *session)
{
    if (slot->isLoggedIn) {
        if (slot->ssoLoggedIn) {
            session->info.state = CKS_RW_SO_FUNCTIONS;
        } else if (session->info.flags & CKF_RW_SESSION) {
            session->info.state = CKS_RW_USER_FUNCTIONS;
        } else {
            session->info.state = CKS_RO_USER_FUNCTIONS;
        }
    } else {
        if (session->info.flags & CKF_RW_SESSION) {
            session->info.state = CKS_RW_PUBLIC_SESSION;
        } else {
            session->info.state = CKS_RO_PUBLIC_SESSION;
        }
    }
}

/* update the state of all the sessions on a slot */
void
sftk_update_all_states(SFTKSlot *slot)
{
    unsigned int i;
    SFTKSession *session;

    for (i = 0; i < slot->sessHashSize; i++) {
        PZLock *lock = SFTK_SESSION_LOCK(slot, i);
        PZ_Lock(lock);
        for (session = slot->head[i]; session; session = session->next) {
            sftk_update_state(slot, session);
        }
        PZ_Unlock(lock);
    }
}

/*
 * context are cipher and digest contexts that are associated with a session
 */
void
sftk_FreeContext(SFTKSessionContext *context)
{
    if (context->cipherInfo) {
        (*context->destroy)(context->cipherInfo, PR_TRUE);
    }
    if (context->hashInfo) {
        (*context->hashdestroy)(context->hashInfo, PR_TRUE);
    }
    if (context->key) {
        sftk_FreeObject(context->key);
        context->key = NULL;
    }
    PORT_Free(context);
}

/*
 * create a new nession. NOTE: The session handle is not set, and the
 * session is not added to the slot's session queue.
 */
SFTKSession *
sftk_NewSession(CK_SLOT_ID slotID, CK_NOTIFY notify, CK_VOID_PTR pApplication,
                CK_FLAGS flags)
{
    SFTKSession *session;
    SFTKSlot *slot = sftk_SlotFromID(slotID, PR_FALSE);

    if (slot == NULL)
        return NULL;

    session = (SFTKSession *)PORT_Alloc(sizeof(SFTKSession));
    if (session == NULL)
        return NULL;

    session->next = session->prev = NULL;
    session->enc_context = NULL;
    session->hash_context = NULL;
    session->sign_context = NULL;
    session->search = NULL;
    session->objectIDCount = 1;
    session->objectLock = PZ_NewLock(nssILockObject);
    if (session->objectLock == NULL) {
        PORT_Free(session);
        return NULL;
    }
    session->objects[0] = NULL;

    session->slot = slot;
    session->notify = notify;
    session->appData = pApplication;
    session->info.flags = flags;
    session->info.slotID = slotID;
    session->info.ulDeviceError = 0;
    sftk_update_state(slot, session);
    return session;
}

/* free all the data associated with a session. */
void
sftk_DestroySession(SFTKSession *session)
{
    SFTKObjectList *op, *next;

    /* clean out the attributes */
    /* since no one is referencing us, it's safe to walk the chain
     * without a lock */
    for (op = session->objects[0]; op != NULL; op = next) {
        next = op->next;
        /* paranoia */
        op->next = op->prev = NULL;
        sftk_DeleteObject(session, op->parent);
    }
    PZ_DestroyLock(session->objectLock);
    if (session->enc_context) {
        sftk_FreeContext(session->enc_context);
    }
    if (session->hash_context) {
        sftk_FreeContext(session->hash_context);
    }
    if (session->sign_context) {
        sftk_FreeContext(session->sign_context);
    }
    if (session->search) {
        sftk_FreeSearch(session->search);
    }
    PORT_Free(session);
}

/*
 * look up a session structure from a session handle
 * generate a reference to it.
 */
SFTKSession *
sftk_SessionFromHandle(CK_SESSION_HANDLE handle)
{
    SFTKSlot *slot = sftk_SlotFromSessionHandle(handle);
    SFTKSession *session;
    PZLock *lock;

    if (!slot)
        return NULL;
    lock = SFTK_SESSION_LOCK(slot, handle);

    PZ_Lock(lock);
    sftkqueue_find(session, handle, slot->head, slot->sessHashSize);
    PZ_Unlock(lock);

    return (session);
}

/*
 * release a reference to a session handle. This method of using SFTKSessions
 * is deprecated, but the pattern should be retained until a future effort
 * to refactor all SFTKSession users at once is completed.
 */
void
sftk_FreeSession(SFTKSession *session)
{
    return;
}

void
sftk_addHandle(SFTKSearchResults *search, CK_OBJECT_HANDLE handle)
{
    if (search->handles == NULL) {
        return;
    }
    if (search->size >= search->array_size) {
        search->array_size += NSC_SEARCH_BLOCK_SIZE;
        search->handles = (CK_OBJECT_HANDLE *)PORT_Realloc(search->handles,
                                                           sizeof(CK_OBJECT_HANDLE) * search->array_size);
        if (search->handles == NULL) {
            return;
        }
    }
    search->handles[search->size] = handle;
    search->size++;
}

static CK_RV
handleToClass(SFTKSlot *slot, CK_OBJECT_HANDLE handle,
              CK_OBJECT_CLASS *objClass)
{
    SFTKDBHandle *dbHandle = sftk_getDBForTokenObject(slot, handle);
    CK_ATTRIBUTE objClassTemplate;
    CK_RV crv;

    *objClass = CKO_DATA;
    objClassTemplate.type = CKA_CLASS;
    objClassTemplate.pValue = objClass;
    objClassTemplate.ulValueLen = sizeof(*objClass);
    crv = sftkdb_GetAttributeValue(dbHandle, handle, &objClassTemplate, 1);
    sftk_freeDB(dbHandle);
    return crv;
}

SFTKObject *
sftk_NewTokenObject(SFTKSlot *slot, SECItem *dbKey, CK_OBJECT_HANDLE handle)
{
    SFTKObject *object = NULL;
    PRBool hasLocks = PR_FALSE;
    CK_RV crv;

    object = sftk_GetObjectFromList(&hasLocks, PR_FALSE, &tokenObjectList, 0,
                                    PR_FALSE);
    if (object == NULL) {
        return NULL;
    }

    object->handle = handle;
    /* every object must have a class, if we can't get it, the object
     * doesn't exist */
    crv = handleToClass(slot, handle, &object->objclass);
    if (crv != CKR_OK) {
        goto loser;
    }
    object->slot = slot;
    object->objectInfo = NULL;
    object->infoFree = NULL;
    if (!hasLocks) {
        object->refLock = PZ_NewLock(nssILockRefLock);
    }
    if (object->refLock == NULL) {
        goto loser;
    }
    object->refCount = 1;

    return object;
loser:
    (void)sftk_DestroyObject(object);
    return NULL;
}

SFTKTokenObject *
sftk_convertSessionToToken(SFTKObject *obj)
{
    SECItem *key;
    SFTKSessionObject *so = (SFTKSessionObject *)obj;
    SFTKTokenObject *to = sftk_narrowToTokenObject(obj);
    SECStatus rv;

    sftk_DestroySessionObjectData(so);
    PZ_DestroyLock(so->attributeLock);
    if (to == NULL) {
        return NULL;
    }
    sftk_tokenKeyLock(so->obj.slot);
    key = sftk_lookupTokenKeyByHandle(so->obj.slot, so->obj.handle);
    if (key == NULL) {
        sftk_tokenKeyUnlock(so->obj.slot);
        return NULL;
    }
    rv = SECITEM_CopyItem(NULL, &to->dbKey, key);
    sftk_tokenKeyUnlock(so->obj.slot);
    if (rv == SECFailure) {
        return NULL;
    }

    return to;
}

SFTKSessionObject *
sftk_narrowToSessionObject(SFTKObject *obj)
{
    return !sftk_isToken(obj->handle) ? (SFTKSessionObject *)obj : NULL;
}

SFTKTokenObject *
sftk_narrowToTokenObject(SFTKObject *obj)
{
    return sftk_isToken(obj->handle) ? (SFTKTokenObject *)obj : NULL;
}
