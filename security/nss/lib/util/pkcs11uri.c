/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pkcs11.h"
#include "pkcs11uri.h"
#include "plarena.h"
#include "prprf.h"
#include "secport.h"

/* Character sets used in the ABNF rules in RFC7512. */
#define PK11URI_DIGIT "0123456789"
#define PK11URI_ALPHA "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define PK11URI_HEXDIG PK11URI_DIGIT "abcdefABCDEF"
#define PK11URI_UNRESERVED PK11URI_ALPHA PK11URI_DIGIT "-._~"
#define PK11URI_RES_AVAIL ":[]@!$'()*+,="
#define PK11URI_PATH_RES_AVAIL PK11URI_RES_AVAIL "&"
#define PK11URI_QUERY_RES_AVAIL PK11URI_RES_AVAIL "/?|"
#define PK11URI_ATTR_NM_CHAR PK11URI_ALPHA PK11URI_DIGIT "-_"
#define PK11URI_PCHAR PK11URI_UNRESERVED PK11URI_PATH_RES_AVAIL
#define PK11URI_QCHAR PK11URI_UNRESERVED PK11URI_QUERY_RES_AVAIL

/* Path attributes defined in RFC7512. */
static const char *pattr_names[] = {
    PK11URI_PATTR_TOKEN,
    PK11URI_PATTR_MANUFACTURER,
    PK11URI_PATTR_SERIAL,
    PK11URI_PATTR_MODEL,
    PK11URI_PATTR_LIBRARY_MANUFACTURER,
    PK11URI_PATTR_LIBRARY_DESCRIPTION,
    PK11URI_PATTR_LIBRARY_VERSION,
    PK11URI_PATTR_OBJECT,
    PK11URI_PATTR_TYPE,
    PK11URI_PATTR_ID,
    PK11URI_PATTR_SLOT_MANUFACTURER,
    PK11URI_PATTR_SLOT_DESCRIPTION,
    PK11URI_PATTR_SLOT_ID
};

/* Query attributes defined in RFC7512. */
static const char *qattr_names[] = {
    PK11URI_QATTR_PIN_SOURCE,
    PK11URI_QATTR_PIN_VALUE,
    PK11URI_QATTR_MODULE_NAME,
    PK11URI_QATTR_MODULE_PATH
};

struct PK11URIBufferStr {
    PLArenaPool *arena;
    char *data;
    size_t size;
    size_t allocated;
};
typedef struct PK11URIBufferStr PK11URIBuffer;

struct PK11URIAttributeListEntryStr {
    char *name;
    char *value;
};
typedef struct PK11URIAttributeListEntryStr PK11URIAttributeListEntry;

struct PK11URIAttributeListStr {
    PLArenaPool *arena;
    PK11URIAttributeListEntry *attrs;
    size_t num_attrs;
};
typedef struct PK11URIAttributeListStr PK11URIAttributeList;

struct PK11URIStr {
    PLArenaPool *arena;

    PK11URIAttributeList pattrs;
    PK11URIAttributeList vpattrs;

    PK11URIAttributeList qattrs;
    PK11URIAttributeList vqattrs;
};

#define PK11URI_ARENA_SIZE 1024

typedef int (*PK11URIAttributeCompareNameFunc)(const char *a, const char *b);

/* This belongs in secport.h */
#define PORT_ArenaGrowArray(poolp, oldptr, type, oldnum, newnum) \
    (type *)PORT_ArenaGrow((poolp), (oldptr),                    \
                           (oldnum) * sizeof(type), (newnum) * sizeof(type))
#define PORT_ReallocArray(oldptr, type, newnum) \
    (type *)PORT_Realloc((oldptr), (newnum) * sizeof(type))

/* Functions for resizable buffer. */
static SECStatus
pk11uri_AppendBuffer(PK11URIBuffer *buffer, const unsigned char *data,
                     size_t size)
{
    /* Check overflow. */
    if (buffer->size + size < buffer->size)
        return SECFailure;

    if (buffer->size + size > buffer->allocated) {
        size_t allocated = buffer->allocated * 2 + size;
        if (allocated < buffer->allocated)
            return SECFailure;
        if (buffer->arena)
            buffer->data = PORT_ArenaGrow(buffer->arena, buffer->data,
                                          buffer->allocated, allocated);
        else
            buffer->data = PORT_Realloc(buffer->data, allocated);
        if (buffer->data == NULL)
            return SECFailure;
        buffer->allocated = allocated;
    }

    memcpy(&buffer->data[buffer->size], data, size);
    buffer->size += size;

    return SECSuccess;
}

static void
pk11uri_InitBuffer(PK11URIBuffer *buffer, PLArenaPool *arena)
{
    memset(buffer, 0, sizeof(PK11URIBuffer));
    buffer->arena = arena;
}

static void
pk11uri_DestroyBuffer(PK11URIBuffer *buffer)
{
    if (buffer->arena == NULL) {
        PORT_Free(buffer->data);
    }
}

/* URI encoding functions. */
static char *
pk11uri_Escape(PLArenaPool *arena, const char *value, size_t length,
               const char *available)
{
    PK11URIBuffer buffer;
    const char *p;
    unsigned char buf[4];
    char *result = NULL;
    SECStatus ret;

    pk11uri_InitBuffer(&buffer, arena);

    for (p = value; p < value + length; p++) {
        if (strchr(available, *p) == NULL) {
            if (PR_snprintf((char *)buf, sizeof(buf), "%%%02X", *p) == (PRUint32)-1) {
                goto fail;
            }
            ret = pk11uri_AppendBuffer(&buffer, buf, 3);
            if (ret != SECSuccess) {
                goto fail;
            }
        } else {
            ret = pk11uri_AppendBuffer(&buffer, (const unsigned char *)p, 1);
            if (ret != SECSuccess) {
                goto fail;
            }
        }
    }
    buf[0] = '\0';
    ret = pk11uri_AppendBuffer(&buffer, buf, 1);
    if (ret != SECSuccess) {
        goto fail;
    }

    /* Steal the memory allocated in buffer. */
    result = buffer.data;
    buffer.data = NULL;

fail:
    pk11uri_DestroyBuffer(&buffer);

    return result;
}

static char *
pk11uri_Unescape(PLArenaPool *arena, const char *value, size_t length)
{
    PK11URIBuffer buffer;
    const char *p;
    unsigned char buf[1];
    char *result = NULL;
    SECStatus ret;

    pk11uri_InitBuffer(&buffer, arena);

    for (p = value; p < value + length; p++) {
        if (*p == '%') {
            int c;
            size_t i;

            p++;
            for (c = 0, i = 0; i < 2; i++) {
                int h = *(p + i);
                if ('0' <= h && h <= '9') {
                    c = (c << 4) | (h - '0');
                } else if ('a' <= h && h <= 'f') {
                    c = (c << 4) | (h - 'a' + 10);
                } else if ('A' <= h && h <= 'F') {
                    c = (c << 4) | (h - 'A' + 10);
                } else {
                    break;
                }
            }
            if (i != 2) {
                goto fail;
            }
            p++;
            buf[0] = c;
        } else {
            buf[0] = *p;
        }
        ret = pk11uri_AppendBuffer(&buffer, buf, 1);
        if (ret != SECSuccess) {
            goto fail;
        }
    }
    buf[0] = '\0';
    ret = pk11uri_AppendBuffer(&buffer, buf, 1);
    if (ret != SECSuccess) {
        goto fail;
    }

    result = buffer.data;
    buffer.data = NULL;

fail:
    pk11uri_DestroyBuffer(&buffer);

    return result;
}

/* Functions for manipulating attributes array. */

/* Compare two attribute names by the array index in attr_names.  Both
 * attribute names must be present in attr_names, otherwise it is a
 * programming error. */
static int
pk11uri_CompareByPosition(const char *a, const char *b,
                          const char **attr_names, size_t num_attr_names)
{
    int i, j;

    for (i = 0; i < num_attr_names; i++) {
        if (strcmp(a, attr_names[i]) == 0) {
            break;
        }
    }
    PR_ASSERT(i < num_attr_names);

    for (j = 0; j < num_attr_names; j++) {
        if (strcmp(b, attr_names[j]) == 0) {
            break;
        }
    }
    PR_ASSERT(j < num_attr_names);

    return i - j;
}

/* Those pk11uri_Compare{Path,Query}AttributeName functions are used
 * to reorder attributes when inserting. */
static int
pk11uri_ComparePathAttributeName(const char *a, const char *b)
{
    return pk11uri_CompareByPosition(a, b, pattr_names, PR_ARRAY_SIZE(pattr_names));
}

static int
pk11uri_CompareQueryAttributeName(const char *a, const char *b)
{
    return pk11uri_CompareByPosition(a, b, qattr_names, PR_ARRAY_SIZE(qattr_names));
}

static SECStatus
pk11uri_InsertToAttributeList(PK11URIAttributeList *attrs,
                              char *name, char *value,
                              PK11URIAttributeCompareNameFunc compare_name,
                              PRBool allow_duplicate)
{
    size_t i;

    if (attrs->arena) {
        attrs->attrs = PORT_ArenaGrowArray(attrs->arena, attrs->attrs,
                                           PK11URIAttributeListEntry,
                                           attrs->num_attrs,
                                           attrs->num_attrs + 1);
    } else {
        attrs->attrs = PORT_ReallocArray(attrs->attrs,
                                         PK11URIAttributeListEntry,
                                         attrs->num_attrs + 1);
    }
    if (attrs->attrs == NULL) {
        return SECFailure;
    }

    for (i = 0; i < attrs->num_attrs; i++) {
        if (!allow_duplicate && strcmp(name, attrs->attrs[i].name) == 0) {
            return SECFailure;
        }
        if (compare_name(name, attrs->attrs[i].name) < 0) {
            memmove(&attrs->attrs[i + 1], &attrs->attrs[i],
                    sizeof(PK11URIAttributeListEntry) * (attrs->num_attrs - i));
            break;
        }
    }

    attrs->attrs[i].name = name;
    attrs->attrs[i].value = value;

    attrs->num_attrs++;

    return SECSuccess;
}

static SECStatus
pk11uri_InsertToAttributeListEscaped(PK11URIAttributeList *attrs,
                                     const char *name, size_t name_size,
                                     const char *value, size_t value_size,
                                     PK11URIAttributeCompareNameFunc compare_name,
                                     PRBool allow_duplicate)
{
    char *name_copy = NULL, *value_copy = NULL;
    SECStatus ret;

    if (attrs->arena) {
        name_copy = PORT_ArenaNewArray(attrs->arena, char, name_size + 1);
    } else {
        name_copy = PORT_Alloc(name_size + 1);
    }
    if (name_copy == NULL) {
        goto fail;
    }
    memcpy(name_copy, name, name_size);
    name_copy[name_size] = '\0';

    value_copy = pk11uri_Unescape(attrs->arena, value, value_size);
    if (value_copy == NULL) {
        goto fail;
    }

    ret = pk11uri_InsertToAttributeList(attrs, name_copy, value_copy, compare_name,
                                        allow_duplicate);
    if (ret != SECSuccess) {
        goto fail;
    }

    return ret;

fail:
    if (attrs->arena == NULL) {
        PORT_Free(name_copy);
        PORT_Free(value_copy);
    }

    return SECFailure;
}

static void
pk11uri_InitAttributeList(PK11URIAttributeList *attrs, PLArenaPool *arena)
{
    memset(attrs, 0, sizeof(PK11URIAttributeList));
    attrs->arena = arena;
}

static void
pk11uri_DestroyAttributeList(PK11URIAttributeList *attrs)
{
    if (attrs->arena == NULL) {
        size_t i;

        for (i = 0; i < attrs->num_attrs; i++) {
            PORT_Free(attrs->attrs[i].name);
            PORT_Free(attrs->attrs[i].value);
        }
        PORT_Free(attrs->attrs);
    }
}

static SECStatus
pk11uri_AppendAttributeListToBuffer(PK11URIBuffer *buffer,
                                    PK11URIAttributeList *attrs,
                                    int separator,
                                    const char *unescaped)
{
    size_t i;
    SECStatus ret;

    for (i = 0; i < attrs->num_attrs; i++) {
        unsigned char sep[1];
        char *escaped;
        PK11URIAttributeListEntry *attr = &attrs->attrs[i];

        if (i > 0) {
            sep[0] = separator;
            ret = pk11uri_AppendBuffer(buffer, sep, 1);
            if (ret != SECSuccess) {
                return ret;
            }
        }

        ret = pk11uri_AppendBuffer(buffer, (unsigned char *)attr->name,
                                   strlen(attr->name));
        if (ret != SECSuccess) {
            return ret;
        }

        sep[0] = '=';
        ret = pk11uri_AppendBuffer(buffer, sep, 1);
        if (ret != SECSuccess) {
            return ret;
        }

        escaped = pk11uri_Escape(buffer->arena, attr->value, strlen(attr->value),
                                 unescaped);
        if (escaped == NULL) {
            return ret;
        }
        ret = pk11uri_AppendBuffer(buffer, (unsigned char *)escaped,
                                   strlen(escaped));
        if (buffer->arena == NULL) {
            PORT_Free(escaped);
        }
        if (ret != SECSuccess) {
            return ret;
        }
    }

    return SECSuccess;
}

/* Creation of PK11URI object. */
static PK11URI *
pk11uri_AllocURI(void)
{
    PLArenaPool *arena;
    PK11URI *result;

    arena = PORT_NewArena(PK11URI_ARENA_SIZE);
    if (arena == NULL) {
        return NULL;
    }

    result = PORT_ArenaZAlloc(arena, sizeof(PK11URI));
    if (result == NULL) {
        PORT_FreeArena(arena, PR_FALSE);
        return NULL;
    }

    result->arena = arena;
    pk11uri_InitAttributeList(&result->pattrs, arena);
    pk11uri_InitAttributeList(&result->vpattrs, arena);
    pk11uri_InitAttributeList(&result->qattrs, arena);
    pk11uri_InitAttributeList(&result->vqattrs, arena);

    return result;
}

static SECStatus
pk11uri_InsertAttributes(PK11URIAttributeList *dest_attrs,
                         PK11URIAttributeList *dest_vattrs,
                         const PK11URIAttribute *attrs,
                         size_t num_attrs,
                         const char **attr_names,
                         size_t num_attr_names,
                         PK11URIAttributeCompareNameFunc compare_name,
                         PRBool allow_duplicate,
                         PRBool vendor_allow_duplicate)
{
    SECStatus ret;
    size_t i;

    for (i = 0; i < num_attrs; i++) {
        char *name, *value;
        const char *p;
        size_t j;

        p = attrs[i].name;

        /* The attribute must not be empty. */
        if (*p == '\0') {
            return SECFailure;
        }

        /* Check that the name doesn't contain invalid character. */
        for (; *p != '\0'; p++) {
            if (strchr(PK11URI_ATTR_NM_CHAR, *p) == NULL) {
                return SECFailure;
            }
        }

        name = PORT_ArenaStrdup(dest_attrs->arena, attrs[i].name);
        if (name == NULL) {
            return SECFailure;
        }

        value = PORT_ArenaStrdup(dest_attrs->arena, attrs[i].value);
        if (value == NULL) {
            return SECFailure;
        }

        for (j = 0; j < num_attr_names; j++) {
            if (strcmp(name, attr_names[j]) == 0) {
                break;
            }
        }
        if (j < num_attr_names) {
            /* Named attribute. */
            ret = pk11uri_InsertToAttributeList(dest_attrs,
                                                name, value,
                                                compare_name,
                                                allow_duplicate);
            if (ret != SECSuccess) {
                return ret;
            }
        } else {
            /* Vendor attribute. */
            ret = pk11uri_InsertToAttributeList(dest_vattrs,
                                                name, value,
                                                strcmp,
                                                vendor_allow_duplicate);
            if (ret != SECSuccess) {
                return ret;
            }
        }
    }

    return SECSuccess;
}

PK11URI *
PK11URI_CreateURI(const PK11URIAttribute *pattrs,
                  size_t num_pattrs,
                  const PK11URIAttribute *qattrs,
                  size_t num_qattrs)
{
    PK11URI *result;
    SECStatus ret;

    result = pk11uri_AllocURI();

    ret = pk11uri_InsertAttributes(&result->pattrs, &result->vpattrs,
                                   pattrs, num_pattrs,
                                   pattr_names, PR_ARRAY_SIZE(pattr_names),
                                   pk11uri_ComparePathAttributeName,
                                   PR_FALSE, PR_FALSE);
    if (ret != SECSuccess) {
        goto fail;
    }

    ret = pk11uri_InsertAttributes(&result->qattrs, &result->vqattrs,
                                   qattrs, num_qattrs,
                                   qattr_names, PR_ARRAY_SIZE(qattr_names),
                                   pk11uri_CompareQueryAttributeName,
                                   PR_FALSE, PR_TRUE);
    if (ret != SECSuccess) {
        goto fail;
    }

    return result;

fail:
    PK11URI_DestroyURI(result);

    return NULL;
}

/* Parsing. */
static SECStatus
pk11uri_ParseAttributes(const char **string,
                        const char *stop_chars,
                        int separator,
                        const char *accept_chars,
                        const char **attr_names, size_t num_attr_names,
                        PK11URIAttributeList *attrs,
                        PK11URIAttributeList *vattrs,
                        PK11URIAttributeCompareNameFunc compare_name,
                        PRBool allow_duplicate,
                        PRBool vendor_allow_duplicate)
{
    const char *p = *string;

    for (; *p != '\0'; p++) {
        const char *name_start, *name_end, *value_start, *value_end;
        size_t name_length, value_length, i;
        SECStatus ret;

        if (strchr(stop_chars, *p) != NULL) {
            break;
        }
        for (name_start = p; *p != '=' && *p != '\0'; p++) {
            if (strchr(PK11URI_ATTR_NM_CHAR, *p) != NULL)
                continue;

            return SECFailure;
        }
        if (*p == '\0') {
            return SECFailure;
        }
        name_end = p++;

        /* The attribute name must not be empty. */
        if (name_end == name_start) {
            return SECFailure;
        }

        for (value_start = p; *p != separator && *p != '\0'; p++) {
            if (strchr(stop_chars, *p) != NULL) {
                break;
            }
            if (strchr(accept_chars, *p) != NULL) {
                continue;
            }
            if (*p == '%') {
                const char ch2 = *++p;
                if (strchr(PK11URI_HEXDIG, ch2) != NULL) {
                    const char ch3 = *++p;
                    if (strchr(PK11URI_HEXDIG, ch3) != NULL)
                        continue;
                }
            }

            return SECFailure;
        }
        value_end = p;

        name_length = name_end - name_start;
        value_length = value_end - value_start;

        for (i = 0; i < num_attr_names; i++) {
            if (name_length == strlen(attr_names[i]) &&
                memcmp(name_start, attr_names[i], name_length) == 0) {
                break;
            }
        }
        if (i < num_attr_names) {
            /* Named attribute. */
            ret = pk11uri_InsertToAttributeListEscaped(attrs,
                                                       name_start, name_length,
                                                       value_start, value_length,
                                                       compare_name,
                                                       allow_duplicate);
            if (ret != SECSuccess) {
                return ret;
            }
        } else {
            /* Vendor attribute. */
            ret = pk11uri_InsertToAttributeListEscaped(vattrs,
                                                       name_start, name_length,
                                                       value_start, value_length,
                                                       strcmp,
                                                       vendor_allow_duplicate);
            if (ret != SECSuccess) {
                return ret;
            }
        }

        if (*p == '?' || *p == '\0') {
            break;
        }
    }

    *string = p;
    return SECSuccess;
}

PK11URI *
PK11URI_ParseURI(const char *string)
{
    PK11URI *result;
    const char *p = string;
    SECStatus ret;

    if (strncmp("pkcs11:", p, 7) != 0) {
        return NULL;
    }
    p += 7;

    result = pk11uri_AllocURI();
    if (result == NULL) {
        return NULL;
    }

    /* Parse the path component and its attributes. */
    ret = pk11uri_ParseAttributes(&p, "?", ';', PK11URI_PCHAR,
                                  pattr_names, PR_ARRAY_SIZE(pattr_names),
                                  &result->pattrs, &result->vpattrs,
                                  pk11uri_ComparePathAttributeName,
                                  PR_FALSE, PR_FALSE);
    if (ret != SECSuccess) {
        goto fail;
    }

    /* Parse the query component and its attributes. */
    if (*p == '?') {
        p++;
        ret = pk11uri_ParseAttributes(&p, "", '&', PK11URI_QCHAR,
                                      qattr_names, PR_ARRAY_SIZE(qattr_names),
                                      &result->qattrs, &result->vqattrs,
                                      pk11uri_CompareQueryAttributeName,
                                      PR_FALSE, PR_TRUE);
        if (ret != SECSuccess) {
            goto fail;
        }
    }

    return result;

fail:
    PK11URI_DestroyURI(result);

    return NULL;
}

/* Formatting. */
char *
PK11URI_FormatURI(PLArenaPool *arena, PK11URI *uri)
{
    PK11URIBuffer buffer;
    SECStatus ret;
    char *result = NULL;

    pk11uri_InitBuffer(&buffer, arena);

    ret = pk11uri_AppendBuffer(&buffer, (unsigned char *)"pkcs11:", 7);
    if (ret != SECSuccess)
        goto fail;

    ret = pk11uri_AppendAttributeListToBuffer(&buffer, &uri->pattrs, ';', PK11URI_PCHAR);
    if (ret != SECSuccess) {
        goto fail;
    }

    if (uri->pattrs.num_attrs > 0 && uri->vpattrs.num_attrs > 0) {
        ret = pk11uri_AppendBuffer(&buffer, (unsigned char *)";", 1);
        if (ret != SECSuccess) {
            goto fail;
        }
    }

    ret = pk11uri_AppendAttributeListToBuffer(&buffer, &uri->vpattrs, ';',
                                              PK11URI_PCHAR);
    if (ret != SECSuccess) {
        goto fail;
    }

    if (uri->qattrs.num_attrs > 0 || uri->vqattrs.num_attrs > 0) {
        ret = pk11uri_AppendBuffer(&buffer, (unsigned char *)"?", 1);
        if (ret != SECSuccess) {
            goto fail;
        }
    }

    ret = pk11uri_AppendAttributeListToBuffer(&buffer, &uri->qattrs, '&', PK11URI_QCHAR);
    if (ret != SECSuccess) {
        goto fail;
    }

    if (uri->qattrs.num_attrs > 0 && uri->vqattrs.num_attrs > 0) {
        ret = pk11uri_AppendBuffer(&buffer, (unsigned char *)"&", 1);
        if (ret != SECSuccess) {
            goto fail;
        }
    }

    ret = pk11uri_AppendAttributeListToBuffer(&buffer, &uri->vqattrs, '&',
                                              PK11URI_QCHAR);
    if (ret != SECSuccess) {
        goto fail;
    }

    ret = pk11uri_AppendBuffer(&buffer, (unsigned char *)"\0", 1);
    if (ret != SECSuccess) {
        goto fail;
    }

    result = buffer.data;
    buffer.data = NULL;

fail:
    pk11uri_DestroyBuffer(&buffer);

    return result;
}

/* Deallocating. */
void
PK11URI_DestroyURI(PK11URI *uri)
{
    pk11uri_DestroyAttributeList(&uri->pattrs);
    pk11uri_DestroyAttributeList(&uri->vpattrs);
    pk11uri_DestroyAttributeList(&uri->qattrs);
    pk11uri_DestroyAttributeList(&uri->vqattrs);
    PORT_FreeArena(uri->arena, PR_FALSE);
}

/* Accessors. */
static const char *
pk11uri_GetAttribute(PK11URIAttributeList *attrs,
                     PK11URIAttributeList *vattrs,
                     const char *name)
{
    size_t i;

    for (i = 0; i < attrs->num_attrs; i++) {
        if (strcmp(name, attrs->attrs[i].name) == 0) {
            return attrs->attrs[i].value;
        }
    }

    for (i = 0; i < vattrs->num_attrs; i++) {
        if (strcmp(name, vattrs->attrs[i].name) == 0) {
            return vattrs->attrs[i].value;
        }
    }

    return NULL;
}

const char *
PK11URI_GetPathAttribute(PK11URI *uri, const char *name)
{
    return pk11uri_GetAttribute(&uri->pattrs, &uri->vpattrs, name);
}

const char *
PK11URI_GetQueryAttribute(PK11URI *uri, const char *name)
{
    return pk11uri_GetAttribute(&uri->qattrs, &uri->vqattrs, name);
}
