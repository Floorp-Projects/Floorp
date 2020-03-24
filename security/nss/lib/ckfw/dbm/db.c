/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ckdbm.h"

#define PREFIX_METADATA "0000"
#define PREFIX_OBJECT "0001"
#define PREFIX_INDEX "0002"

static CK_VERSION nss_dbm_db_format_version = { 1, 0 };
struct handle {
    char prefix[4];
    CK_ULONG id;
};

NSS_IMPLEMENT nss_dbm_db_t *
nss_dbm_db_open(
    NSSArena *arena,
    NSSCKFWInstance *fwInstance,
    char *filename,
    int flags,
    CK_RV *pError)
{
    nss_dbm_db_t *rv;
    CK_VERSION db_version;

    rv = nss_ZNEW(arena, nss_dbm_db_t);
    if ((nss_dbm_db_t *)NULL == rv) {
        *pError = CKR_HOST_MEMORY;
        return (nss_dbm_db_t *)NULL;
    }

    rv->db = dbopen(filename, flags, 0600, DB_HASH, (const void *)NULL);
    if ((DB *)NULL == rv->db) {
        *pError = CKR_TOKEN_NOT_PRESENT;
        return (nss_dbm_db_t *)NULL;
    }

    rv->crustylock = NSSCKFWInstance_CreateMutex(fwInstance, arena, pError);
    if ((NSSCKFWMutex *)NULL == rv->crustylock) {
        return (nss_dbm_db_t *)NULL;
    }

    db_version = nss_dbm_db_get_format_version(rv);
    if (db_version.major != nss_dbm_db_format_version.major) {
        nss_dbm_db_close(rv);
        *pError = CKR_TOKEN_NOT_RECOGNIZED;
        return (nss_dbm_db_t *)NULL;
    }

    return rv;
}

NSS_IMPLEMENT void
nss_dbm_db_close(
    nss_dbm_db_t *db)
{
    if ((NSSCKFWMutex *)NULL != db->crustylock) {
        (void)NSSCKFWMutex_Destroy(db->crustylock);
    }

    if ((DB *)NULL != db->db) {
        (void)db->db->close(db->db);
    }

    nss_ZFreeIf(db);
}

NSS_IMPLEMENT CK_VERSION
nss_dbm_db_get_format_version(
    nss_dbm_db_t *db)
{
    CK_VERSION rv;
    DBT k, v;
    int dbrv;
    char buffer[64];

    rv.major = rv.minor = 0;

    k.data = PREFIX_METADATA "FormatVersion";
    k.size = nssUTF8_Size((NSSUTF8 *)k.data, (PRStatus *)NULL);
    (void)memset(&v, 0, sizeof(v));

    /* Locked region */
    {
        if (CKR_OK != NSSCKFWMutex_Lock(db->crustylock)) {
            return rv;
        }

        dbrv = db->db->get(db->db, &k, &v, 0);
        if (dbrv == 0) {
            CK_ULONG major = 0, minor = 0;
            (void)PR_sscanf(v.data, "%ld.%ld", &major, &minor);
            rv.major = major;
            rv.minor = minor;
        } else if (dbrv > 0) {
            (void)PR_snprintf(buffer, sizeof(buffer), "%ld.%ld", nss_dbm_db_format_version.major,
                              nss_dbm_db_format_version.minor);
            v.data = buffer;
            v.size = nssUTF8_Size((NSSUTF8 *)v.data, (PRStatus *)NULL);
            dbrv = db->db->put(db->db, &k, &v, 0);
            (void)db->db->sync(db->db, 0);
            rv = nss_dbm_db_format_version;
        } else {
            /* No error return.. */
            ;
        }

        (void)NSSCKFWMutex_Unlock(db->crustylock);
    }

    return rv;
}

NSS_IMPLEMENT CK_RV
nss_dbm_db_set_label(
    nss_dbm_db_t *db,
    NSSUTF8 *label)
{
    CK_RV rv;
    DBT k, v;
    int dbrv;

    k.data = PREFIX_METADATA "Label";
    k.size = nssUTF8_Size((NSSUTF8 *)k.data, (PRStatus *)NULL);
    v.data = label;
    v.size = nssUTF8_Size((NSSUTF8 *)v.data, (PRStatus *)NULL);

    /* Locked region */
    {
        rv = NSSCKFWMutex_Lock(db->crustylock);
        if (CKR_OK != rv) {
            return rv;
        }

        dbrv = db->db->put(db->db, &k, &v, 0);
        if (0 != dbrv) {
            rv = CKR_DEVICE_ERROR;
        }

        dbrv = db->db->sync(db->db, 0);
        if (0 != dbrv) {
            rv = CKR_DEVICE_ERROR;
        }

        (void)NSSCKFWMutex_Unlock(db->crustylock);
    }

    return rv;
}

NSS_IMPLEMENT NSSUTF8 *
nss_dbm_db_get_label(
    nss_dbm_db_t *db,
    NSSArena *arena,
    CK_RV *pError)
{
    NSSUTF8 *rv = (NSSUTF8 *)NULL;
    DBT k, v;
    int dbrv;

    k.data = PREFIX_METADATA "Label";
    k.size = nssUTF8_Size((NSSUTF8 *)k.data, (PRStatus *)NULL);

    /* Locked region */
    {
        if (CKR_OK != NSSCKFWMutex_Lock(db->crustylock)) {
            return rv;
        }

        dbrv = db->db->get(db->db, &k, &v, 0);
        if (0 == dbrv) {
            rv = nssUTF8_Duplicate((NSSUTF8 *)v.data, arena);
            if ((NSSUTF8 *)NULL == rv) {
                *pError = CKR_HOST_MEMORY;
            }
        } else if (dbrv > 0) {
            /* Just return null */
            ;
        } else {
            *pError = CKR_DEVICE_ERROR;
            ;
        }

        (void)NSSCKFWMutex_Unlock(db->crustylock);
    }

    return rv;
}

NSS_IMPLEMENT CK_RV
nss_dbm_db_delete_object(
    nss_dbm_dbt_t *dbt)
{
    CK_RV rv;
    int dbrv;

    /* Locked region */
    {
        rv = NSSCKFWMutex_Lock(dbt->my_db->crustylock);
        if (CKR_OK != rv) {
            return rv;
        }

        dbrv = dbt->my_db->db->del(dbt->my_db->db, &dbt->dbt, 0);
        if (0 != dbrv) {
            rv = CKR_DEVICE_ERROR;
            goto done;
        }

        dbrv = dbt->my_db->db->sync(dbt->my_db->db, 0);
        if (0 != dbrv) {
            rv = CKR_DEVICE_ERROR;
            goto done;
        }

    done:
        (void)NSSCKFWMutex_Unlock(dbt->my_db->crustylock);
    }

    return rv;
}

static CK_ULONG
nss_dbm_db_new_handle(
    nss_dbm_db_t *db,
    DBT *dbt, /* pre-allocated */
    CK_RV *pError)
{
    CK_ULONG rv;
    DBT k, v;
    CK_ULONG align = 0, id, myid;
    struct handle *hp;

    if (sizeof(struct handle) != dbt->size) {
        return EINVAL;
    }

    /* Locked region */
    {
        *pError = NSSCKFWMutex_Lock(db->crustylock);
        if (CKR_OK != *pError) {
            return EINVAL;
        }

        k.data = PREFIX_METADATA "LastID";
        k.size = nssUTF8_Size((NSSUTF8 *)k.data, (PRStatus *)NULL);
        (void)memset(&v, 0, sizeof(v));

        rv = db->db->get(db->db, &k, &v, 0);
        if (0 == rv) {
            (void)memcpy(&align, v.data, sizeof(CK_ULONG));
            id = ntohl(align);
        } else if (rv > 0) {
            id = 0;
        } else {
            goto done;
        }

        myid = id;
        id++;
        align = htonl(id);
        v.data = &align;
        v.size = sizeof(CK_ULONG);

        rv = db->db->put(db->db, &k, &v, 0);
        if (0 != rv) {
            goto done;
        }

        rv = db->db->sync(db->db, 0);
        if (0 != rv) {
            goto done;
        }

    done:
        (void)NSSCKFWMutex_Unlock(db->crustylock);
    }

    if (0 != rv) {
        return rv;
    }

    hp = (struct handle *)dbt->data;
    (void)memcpy(&hp->prefix[0], PREFIX_OBJECT, 4);
    hp->id = myid;

    return 0;
}

/*
 * This attribute-type-dependent swapping should probably
 * be in the Framework, because it'll be a concern of just
 * about every Module.  Of course any Framework implementation
 * will have to be augmentable or overridable by a Module.
 */

enum swap_type { type_byte,
                 type_short,
                 type_long,
                 type_opaque };

static enum swap_type
nss_dbm_db_swap_type(
    CK_ATTRIBUTE_TYPE type)
{
    switch (type) {
        case CKA_CLASS:
            return type_long;
        case CKA_TOKEN:
            return type_byte;
        case CKA_PRIVATE:
            return type_byte;
        case CKA_LABEL:
            return type_opaque;
        case CKA_APPLICATION:
            return type_opaque;
        case CKA_VALUE:
            return type_opaque;
        case CKA_CERTIFICATE_TYPE:
            return type_long;
        case CKA_ISSUER:
            return type_opaque;
        case CKA_SERIAL_NUMBER:
            return type_opaque;
        case CKA_KEY_TYPE:
            return type_long;
        case CKA_SUBJECT:
            return type_opaque;
        case CKA_ID:
            return type_opaque;
        case CKA_SENSITIVE:
            return type_byte;
        case CKA_ENCRYPT:
            return type_byte;
        case CKA_DECRYPT:
            return type_byte;
        case CKA_WRAP:
            return type_byte;
        case CKA_UNWRAP:
            return type_byte;
        case CKA_SIGN:
            return type_byte;
        case CKA_SIGN_RECOVER:
            return type_byte;
        case CKA_VERIFY:
            return type_byte;
        case CKA_VERIFY_RECOVER:
            return type_byte;
        case CKA_DERIVE:
            return type_byte;
        case CKA_START_DATE:
            return type_opaque;
        case CKA_END_DATE:
            return type_opaque;
        case CKA_MODULUS:
            return type_opaque;
        case CKA_MODULUS_BITS:
            return type_long;
        case CKA_PUBLIC_EXPONENT:
            return type_opaque;
        case CKA_PRIVATE_EXPONENT:
            return type_opaque;
        case CKA_PRIME_1:
            return type_opaque;
        case CKA_PRIME_2:
            return type_opaque;
        case CKA_EXPONENT_1:
            return type_opaque;
        case CKA_EXPONENT_2:
            return type_opaque;
        case CKA_COEFFICIENT:
            return type_opaque;
        case CKA_PRIME:
            return type_opaque;
        case CKA_SUBPRIME:
            return type_opaque;
        case CKA_BASE:
            return type_opaque;
        case CKA_VALUE_BITS:
            return type_long;
        case CKA_VALUE_LEN:
            return type_long;
        case CKA_EXTRACTABLE:
            return type_byte;
        case CKA_LOCAL:
            return type_byte;
        case CKA_NEVER_EXTRACTABLE:
            return type_byte;
        case CKA_ALWAYS_SENSITIVE:
            return type_byte;
        case CKA_MODIFIABLE:
            return type_byte;
        case CKA_NSS_URL:
            return type_opaque;
        case CKA_NSS_EMAIL:
            return type_opaque;
        case CKA_NSS_SMIME_INFO:
            return type_opaque;
        case CKA_NSS_SMIME_TIMESTAMP:
            return type_opaque;
        case CKA_NSS_PKCS8_SALT:
            return type_opaque;
        case CKA_NSS_PASSWORD_CHECK:
            return type_opaque;
        case CKA_NSS_EXPIRES:
            return type_opaque;
        case CKA_TRUST_DIGITAL_SIGNATURE:
            return type_long;
        case CKA_TRUST_NON_REPUDIATION:
            return type_long;
        case CKA_TRUST_KEY_ENCIPHERMENT:
            return type_long;
        case CKA_TRUST_DATA_ENCIPHERMENT:
            return type_long;
        case CKA_TRUST_KEY_AGREEMENT:
            return type_long;
        case CKA_TRUST_KEY_CERT_SIGN:
            return type_long;
        case CKA_TRUST_CRL_SIGN:
            return type_long;
        case CKA_TRUST_SERVER_AUTH:
            return type_long;
        case CKA_TRUST_CLIENT_AUTH:
            return type_long;
        case CKA_TRUST_CODE_SIGNING:
            return type_long;
        case CKA_TRUST_EMAIL_PROTECTION:
            return type_long;
        case CKA_TRUST_IPSEC_END_SYSTEM:
            return type_long;
        case CKA_TRUST_IPSEC_TUNNEL:
            return type_long;
        case CKA_TRUST_IPSEC_USER:
            return type_long;
        case CKA_TRUST_TIME_STAMPING:
            return type_long;
        case CKA_NSS_DB:
            return type_opaque;
        case CKA_NSS_TRUST:
            return type_opaque;
        default:
            return type_opaque;
    }
}

static void
nss_dbm_db_swap_copy(
    CK_ATTRIBUTE_TYPE type,
    void *dest,
    void *src,
    CK_ULONG len)
{
    switch (nss_dbm_db_swap_type(type)) {
        case type_byte:
        case type_opaque:
            (void)memcpy(dest, src, len);
            break;
        case type_short: {
            CK_USHORT s, d;
            (void)memcpy(&s, src, sizeof(CK_USHORT));
            d = htons(s);
            (void)memcpy(dest, &d, sizeof(CK_USHORT));
            break;
        }
        case type_long: {
            CK_ULONG s, d;
            (void)memcpy(&s, src, sizeof(CK_ULONG));
            d = htonl(s);
            (void)memcpy(dest, &d, sizeof(CK_ULONG));
            break;
        }
    }
}

static CK_RV
nss_dbm_db_wrap_object(
    NSSArena *arena,
    CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulAttributeCount,
    DBT *object)
{
    CK_ULONG object_size;
    CK_ULONG i;
    CK_ULONG *pulData;
    char *pcData;
    CK_ULONG offset;

    object_size = (1 + ulAttributeCount * 3) * sizeof(CK_ULONG);
    offset = object_size;
    for (i = 0; i < ulAttributeCount; i++) {
        object_size += pTemplate[i].ulValueLen;
    }

    object->size = object_size;
    object->data = nss_ZAlloc(arena, object_size);
    if ((void *)NULL == object->data) {
        return CKR_HOST_MEMORY;
    }

    pulData = (CK_ULONG *)object->data;
    pcData = (char *)object->data;

    pulData[0] = htonl(ulAttributeCount);
    for (i = 0; i < ulAttributeCount; i++) {
        CK_ULONG len = pTemplate[i].ulValueLen;
        pulData[1 + i * 3] = htonl(pTemplate[i].type);
        pulData[2 + i * 3] = htonl(len);
        pulData[3 + i * 3] = htonl(offset);
        nss_dbm_db_swap_copy(pTemplate[i].type, &pcData[offset], pTemplate[i].pValue, len);
        offset += len;
    }

    return CKR_OK;
}

static CK_RV
nss_dbm_db_unwrap_object(
    NSSArena *arena,
    DBT *object,
    CK_ATTRIBUTE_PTR *ppTemplate,
    CK_ULONG *pulAttributeCount)
{
    CK_ULONG *pulData;
    char *pcData;
    CK_ULONG n, i;
    CK_ATTRIBUTE_PTR pTemplate;

    pulData = (CK_ULONG *)object->data;
    pcData = (char *)object->data;

    n = ntohl(pulData[0]);
    *pulAttributeCount = n;
    pTemplate = nss_ZNEWARRAY(arena, CK_ATTRIBUTE, n);
    if ((CK_ATTRIBUTE_PTR)NULL == pTemplate) {
        return CKR_HOST_MEMORY;
    }

    for (i = 0; i < n; i++) {
        CK_ULONG len;
        CK_ULONG offset;
        void *p;

        pTemplate[i].type = ntohl(pulData[1 + i * 3]);
        len = ntohl(pulData[2 + i * 3]);
        offset = ntohl(pulData[3 + i * 3]);

        p = nss_ZAlloc(arena, len);
        if ((void *)NULL == p) {
            return CKR_HOST_MEMORY;
        }

        nss_dbm_db_swap_copy(pTemplate[i].type, p, &pcData[offset], len);
        pTemplate[i].ulValueLen = len;
        pTemplate[i].pValue = p;
    }

    *ppTemplate = pTemplate;
    return CKR_OK;
}

NSS_IMPLEMENT nss_dbm_dbt_t *
nss_dbm_db_create_object(
    NSSArena *arena,
    nss_dbm_db_t *db,
    CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulAttributeCount,
    CK_RV *pError,
    CK_ULONG *pdbrv)
{
    NSSArena *tmparena = (NSSArena *)NULL;
    nss_dbm_dbt_t *rv = (nss_dbm_dbt_t *)NULL;
    DBT object;

    rv = nss_ZNEW(arena, nss_dbm_dbt_t);
    if ((nss_dbm_dbt_t *)NULL == rv) {
        *pError = CKR_HOST_MEMORY;
        return (nss_dbm_dbt_t *)NULL;
    }

    rv->my_db = db;
    rv->dbt.size = sizeof(struct handle);
    rv->dbt.data = nss_ZAlloc(arena, rv->dbt.size);
    if ((void *)NULL == rv->dbt.data) {
        *pError = CKR_HOST_MEMORY;
        return (nss_dbm_dbt_t *)NULL;
    }

    *pdbrv = nss_dbm_db_new_handle(db, &rv->dbt, pError);
    if (0 != *pdbrv) {
        return (nss_dbm_dbt_t *)NULL;
    }

    tmparena = NSSArena_Create();
    if ((NSSArena *)NULL == tmparena) {
        *pError = CKR_HOST_MEMORY;
        return (nss_dbm_dbt_t *)NULL;
    }

    *pError = nss_dbm_db_wrap_object(tmparena, pTemplate, ulAttributeCount, &object);
    if (CKR_OK != *pError) {
        return (nss_dbm_dbt_t *)NULL;
    }

    /* Locked region */
    {
        *pError = NSSCKFWMutex_Lock(db->crustylock);
        if (CKR_OK != *pError) {
            goto loser;
        }

        *pdbrv = db->db->put(db->db, &rv->dbt, &object, 0);
        if (0 != *pdbrv) {
            *pError = CKR_DEVICE_ERROR;
        }

        (void)db->db->sync(db->db, 0);

        (void)NSSCKFWMutex_Unlock(db->crustylock);
    }

loser:
    if ((NSSArena *)NULL != tmparena) {
        (void)NSSArena_Destroy(tmparena);
    }

    return rv;
}

NSS_IMPLEMENT CK_RV
nss_dbm_db_find_objects(
    nss_dbm_find_t *find,
    nss_dbm_db_t *db,
    CK_ATTRIBUTE_PTR pTemplate,
    CK_ULONG ulAttributeCount,
    CK_ULONG *pdbrv)
{
    CK_RV rv = CKR_OK;

    if ((nss_dbm_db_t *)NULL != db) {
        DBT k, v;

        rv = NSSCKFWMutex_Lock(db->crustylock);
        if (CKR_OK != rv) {
            return rv;
        }

        *pdbrv = db->db->seq(db->db, &k, &v, R_FIRST);
        while (0 == *pdbrv) {
            CK_ULONG i, j;
            NSSArena *tmparena = (NSSArena *)NULL;
            CK_ULONG ulac;
            CK_ATTRIBUTE_PTR pt;

            if ((k.size < 4) || (0 != memcmp(k.data, PREFIX_OBJECT, 4))) {
                goto nomatch;
            }

            tmparena = NSSArena_Create();

            rv = nss_dbm_db_unwrap_object(tmparena, &v, &pt, &ulac);
            if (CKR_OK != rv) {
                goto loser;
            }

            for (i = 0; i < ulAttributeCount; i++) {
                for (j = 0; j < ulac; j++) {
                    if (pTemplate[i].type ==
                        pt[j].type) {
                        if (pTemplate[i].ulValueLen !=
                            pt[j].ulValueLen) {
                            goto nomatch;
                        }
                        if (0 !=
                            memcmp(pTemplate[i].pValue, pt[j].pValue, pt[j].ulValueLen)) {
                            goto nomatch;
                        }
                        break;
                    }
                }
                if (j == ulac) {
                    goto nomatch;
                }
            }

            /* entire template matches */
            {
                struct nss_dbm_dbt_node *node;

                node = nss_ZNEW(find->arena, struct nss_dbm_dbt_node);
                if ((struct nss_dbm_dbt_node *)NULL == node) {
                    rv =
                        CKR_HOST_MEMORY;
                    goto loser;
                }

                node->dbt = nss_ZNEW(find->arena, nss_dbm_dbt_t);
                if ((nss_dbm_dbt_t *)NULL == node->dbt) {
                    rv =
                        CKR_HOST_MEMORY;
                    goto loser;
                }

                node->dbt->dbt.size = k.size;
                node->dbt->dbt.data = nss_ZAlloc(find->arena, k.size);
                if ((void *)NULL == node->dbt->dbt.data) {
                    rv =
                        CKR_HOST_MEMORY;
                    goto loser;
                }

                (void)memcpy(node->dbt->dbt.data, k.data, k.size);

                node->dbt->my_db = db;

                node->next = find->found;
                find->found = node;
            }

        nomatch:
            if ((NSSArena *)NULL != tmparena) {
                (void)NSSArena_Destroy(tmparena);
            }
            *pdbrv = db->db->seq(db->db, &k, &v, R_NEXT);
        }

        if (*pdbrv < 0) {
            rv = CKR_DEVICE_ERROR;
            goto loser;
        }

        rv = CKR_OK;

    loser:
        (void)NSSCKFWMutex_Unlock(db->crustylock);
    }

    return rv;
}

NSS_IMPLEMENT CK_BBOOL
nss_dbm_db_object_still_exists(
    nss_dbm_dbt_t *dbt)
{
    CK_BBOOL rv;
    CK_RV ckrv;
    int dbrv;
    DBT object;

    ckrv = NSSCKFWMutex_Lock(dbt->my_db->crustylock);
    if (CKR_OK != ckrv) {
        return CK_FALSE;
    }

    dbrv = dbt->my_db->db->get(dbt->my_db->db, &dbt->dbt, &object, 0);
    if (0 == dbrv) {
        rv = CK_TRUE;
    } else {
        rv = CK_FALSE;
    }

    (void)NSSCKFWMutex_Unlock(dbt->my_db->crustylock);

    return rv;
}

NSS_IMPLEMENT CK_ULONG
nss_dbm_db_get_object_attribute_count(
    nss_dbm_dbt_t *dbt,
    CK_RV *pError,
    CK_ULONG *pdbrv)
{
    CK_ULONG rv = 0;
    DBT object;
    CK_ULONG *pulData;

    /* Locked region */
    {
        *pError = NSSCKFWMutex_Lock(dbt->my_db->crustylock);
        if (CKR_OK != *pError) {
            return rv;
        }

        *pdbrv = dbt->my_db->db->get(dbt->my_db->db, &dbt->dbt, &object, 0);
        if (0 == *pdbrv) {
            ;
        } else if (*pdbrv > 0) {
            *pError = CKR_OBJECT_HANDLE_INVALID;
            goto done;
        } else {
            *pError = CKR_DEVICE_ERROR;
            goto done;
        }

        pulData = (CK_ULONG *)object.data;
        rv = ntohl(pulData[0]);

    done:
        (void)NSSCKFWMutex_Unlock(dbt->my_db->crustylock);
    }

    return rv;
}

NSS_IMPLEMENT CK_RV
nss_dbm_db_get_object_attribute_types(
    nss_dbm_dbt_t *dbt,
    CK_ATTRIBUTE_TYPE_PTR typeArray,
    CK_ULONG ulCount,
    CK_ULONG *pdbrv)
{
    CK_RV rv = CKR_OK;
    DBT object;
    CK_ULONG *pulData;
    CK_ULONG n, i;

    /* Locked region */
    {
        rv = NSSCKFWMutex_Lock(dbt->my_db->crustylock);
        if (CKR_OK != rv) {
            return rv;
        }

        *pdbrv = dbt->my_db->db->get(dbt->my_db->db, &dbt->dbt, &object, 0);
        if (0 == *pdbrv) {
            ;
        } else if (*pdbrv > 0) {
            rv = CKR_OBJECT_HANDLE_INVALID;
            goto done;
        } else {
            rv = CKR_DEVICE_ERROR;
            goto done;
        }

        pulData = (CK_ULONG *)object.data;
        n = ntohl(pulData[0]);

        if (ulCount < n) {
            rv = CKR_BUFFER_TOO_SMALL;
            goto done;
        }

        for (i = 0; i < n; i++) {
            typeArray[i] = ntohl(pulData[1 + i * 3]);
        }

    done:
        (void)NSSCKFWMutex_Unlock(dbt->my_db->crustylock);
    }

    return rv;
}

NSS_IMPLEMENT CK_ULONG
nss_dbm_db_get_object_attribute_size(
    nss_dbm_dbt_t *dbt,
    CK_ATTRIBUTE_TYPE type,
    CK_RV *pError,
    CK_ULONG *pdbrv)
{
    CK_ULONG rv = 0;
    DBT object;
    CK_ULONG *pulData;
    CK_ULONG n, i;

    /* Locked region */
    {
        *pError = NSSCKFWMutex_Lock(dbt->my_db->crustylock);
        if (CKR_OK != *pError) {
            return rv;
        }

        *pdbrv = dbt->my_db->db->get(dbt->my_db->db, &dbt->dbt, &object, 0);
        if (0 == *pdbrv) {
            ;
        } else if (*pdbrv > 0) {
            *pError = CKR_OBJECT_HANDLE_INVALID;
            goto done;
        } else {
            *pError = CKR_DEVICE_ERROR;
            goto done;
        }

        pulData = (CK_ULONG *)object.data;
        n = ntohl(pulData[0]);

        for (i = 0; i < n; i++) {
            if (type == ntohl(pulData[1 + i * 3])) {
                rv = ntohl(pulData[2 + i * 3]);
            }
        }

        if (i == n) {
            *pError = CKR_ATTRIBUTE_TYPE_INVALID;
            goto done;
        }

    done:
        (void)NSSCKFWMutex_Unlock(dbt->my_db->crustylock);
    }

    return rv;
}

NSS_IMPLEMENT NSSItem *
nss_dbm_db_get_object_attribute(
    nss_dbm_dbt_t *dbt,
    NSSArena *arena,
    CK_ATTRIBUTE_TYPE type,
    CK_RV *pError,
    CK_ULONG *pdbrv)
{
    NSSItem *rv = (NSSItem *)NULL;
    DBT object;
    CK_ULONG i;
    NSSArena *tmp = NSSArena_Create();
    CK_ATTRIBUTE_PTR pTemplate;
    CK_ULONG ulAttributeCount;

    /* Locked region */
    {
        *pError = NSSCKFWMutex_Lock(dbt->my_db->crustylock);
        if (CKR_OK != *pError) {
            goto loser;
        }

        *pdbrv = dbt->my_db->db->get(dbt->my_db->db, &dbt->dbt, &object, 0);
        if (0 == *pdbrv) {
            ;
        } else if (*pdbrv > 0) {
            *pError = CKR_OBJECT_HANDLE_INVALID;
            goto done;
        } else {
            *pError = CKR_DEVICE_ERROR;
            goto done;
        }

        *pError = nss_dbm_db_unwrap_object(tmp, &object, &pTemplate, &ulAttributeCount);
        if (CKR_OK != *pError) {
            goto done;
        }

        for (i = 0; i < ulAttributeCount; i++) {
            if (type == pTemplate[i].type) {
                rv = nss_ZNEW(arena, NSSItem);
                if ((NSSItem *)NULL == rv) {
                    *pError =
                        CKR_HOST_MEMORY;
                    goto done;
                }
                rv->size = pTemplate[i].ulValueLen;
                rv->data = nss_ZAlloc(arena, rv->size);
                if ((void *)NULL == rv->data) {
                    *pError =
                        CKR_HOST_MEMORY;
                    goto done;
                }
                (void)memcpy(rv->data, pTemplate[i].pValue, rv->size);
                break;
            }
        }
        if (ulAttributeCount == i) {
            *pError = CKR_ATTRIBUTE_TYPE_INVALID;
            goto done;
        }

    done:
        (void)NSSCKFWMutex_Unlock(dbt->my_db->crustylock);
    }

loser:
    if ((NSSArena *)NULL != tmp) {
        NSSArena_Destroy(tmp);
    }

    return rv;
}

NSS_IMPLEMENT CK_RV
nss_dbm_db_set_object_attribute(
    nss_dbm_dbt_t *dbt,
    CK_ATTRIBUTE_TYPE type,
    NSSItem *value,
    CK_ULONG *pdbrv)
{
    CK_RV rv = CKR_OK;
    DBT object;
    CK_ULONG i;
    NSSArena *tmp = NSSArena_Create();
    CK_ATTRIBUTE_PTR pTemplate;
    CK_ULONG ulAttributeCount;

    /* Locked region */
    {
        rv = NSSCKFWMutex_Lock(dbt->my_db->crustylock);
        if (CKR_OK != rv) {
            goto loser;
        }

        *pdbrv = dbt->my_db->db->get(dbt->my_db->db, &dbt->dbt, &object, 0);
        if (0 == *pdbrv) {
            ;
        } else if (*pdbrv > 0) {
            rv = CKR_OBJECT_HANDLE_INVALID;
            goto done;
        } else {
            rv = CKR_DEVICE_ERROR;
            goto done;
        }

        rv = nss_dbm_db_unwrap_object(tmp, &object, &pTemplate, &ulAttributeCount);
        if (CKR_OK != rv) {
            goto done;
        }

        for (i = 0; i < ulAttributeCount; i++) {
            if (type == pTemplate[i].type) {
                /* Replacing an existing attribute */
                pTemplate[i].ulValueLen = value->size;
                pTemplate[i].pValue = value->data;
                break;
            }
        }

        if (i == ulAttributeCount) {
            /* Adding a new attribute */
            CK_ATTRIBUTE_PTR npt = nss_ZNEWARRAY(tmp, CK_ATTRIBUTE, ulAttributeCount + 1);
            if ((CK_ATTRIBUTE_PTR)NULL == npt) {
                rv = CKR_DEVICE_ERROR;
                goto done;
            }

            for (i = 0; i < ulAttributeCount; i++) {
                npt[i] = pTemplate[i];
            }

            npt[ulAttributeCount].type = type;
            npt[ulAttributeCount].ulValueLen = value->size;
            npt[ulAttributeCount].pValue = value->data;

            pTemplate = npt;
            ulAttributeCount++;
        }

        rv = nss_dbm_db_wrap_object(tmp, pTemplate, ulAttributeCount, &object);
        if (CKR_OK != rv) {
            goto done;
        }

        *pdbrv = dbt->my_db->db->put(dbt->my_db->db, &dbt->dbt, &object, 0);
        if (0 != *pdbrv) {
            rv = CKR_DEVICE_ERROR;
            goto done;
        }

        (void)dbt->my_db->db->sync(dbt->my_db->db, 0);

    done:
        (void)NSSCKFWMutex_Unlock(dbt->my_db->crustylock);
    }

loser:
    if ((NSSArena *)NULL != tmp) {
        NSSArena_Destroy(tmp);
    }

    return rv;
}
