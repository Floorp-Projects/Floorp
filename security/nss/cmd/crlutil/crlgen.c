/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** crlgen.c
**
** utility for managing certificates revocation lists generation
**
*/

#include <stdio.h>
#include <math.h>

#include "nspr.h"
#include "plgetopt.h"
#include "nss.h"
#include "secutil.h"
#include "cert.h"
#include "certi.h"
#include "certdb.h"
#include "pk11func.h"
#include "crlgen.h"

/* Destroys extHandle and data. data was create on heap.
 * extHandle creaded by CERT_StartCRLEntryExtensions. entry
 * was allocated on arena.*/
static void
destroyEntryData(CRLGENEntryData *data)
{
    if (!data)
        return;
    PORT_Assert(data->entry);
    if (data->extHandle)
        CERT_FinishExtensions(data->extHandle);
    PORT_Free(data);
}

/* Prints error messages along with line number */
void
crlgen_PrintError(int line, char *msg, ...)
{
    va_list args;

    va_start(args, msg);

    fprintf(stderr, "crlgen: (line: %d) ", line);
    vfprintf(stderr, msg, args);

    va_end(args);
}
/* Finds CRLGENEntryData in hashtable according PRUint64 value
 * - certId : cert serial number*/
static CRLGENEntryData *
crlgen_FindEntry(CRLGENGeneratorData *crlGenData, SECItem *certId)
{
    if (!crlGenData->entryDataHashTable || !certId)
        return NULL;
    return (CRLGENEntryData *)
        PL_HashTableLookup(crlGenData->entryDataHashTable,
                           certId);
}

/* Removes CRLGENEntryData from hashtable according to certId
 * - certId : cert serial number*/
static SECStatus
crlgen_RmEntry(CRLGENGeneratorData *crlGenData, SECItem *certId)
{
    CRLGENEntryData *data = NULL;
    SECStatus rv = SECSuccess;

    if (!crlGenData->entryDataHashTable) {
        return SECSuccess;
    }

    data = crlgen_FindEntry(crlGenData, certId);
    if (!data) {
        return SECSuccess;
    }

    if (!PL_HashTableRemove(crlGenData->entryDataHashTable, certId)) {
        rv = SECFailure;
    }

    destroyEntryData(data);
    return rv;
}

/* Stores CRLGENEntryData in hashtable according to certId
 * - certId : cert serial number*/
static CRLGENEntryData *
crlgen_PlaceAnEntry(CRLGENGeneratorData *crlGenData,
                    CERTCrlEntry *entry, SECItem *certId)
{
    CRLGENEntryData *newData = NULL;

    PORT_Assert(crlGenData && crlGenData->entryDataHashTable &&
                entry);
    if (!crlGenData || !crlGenData->entryDataHashTable || !entry) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    newData = PORT_ZNew(CRLGENEntryData);
    if (!newData) {
        return NULL;
    }
    newData->entry = entry;
    newData->certId = certId;
    if (!PL_HashTableAdd(crlGenData->entryDataHashTable,
                         newData->certId, newData)) {
        crlgen_PrintError(crlGenData->parsedLineNum,
                          "Can not add entryData structure\n");
        return NULL;
    }
    return newData;
}

/* Use this structure to keep pointer when commiting entries extensions */
struct commitData {
    int pos;
    CERTCrlEntry **entries;
};

/* HT PL_HashTableEnumerateEntries callback. Sorts hashtable entries of the
 * table he. Returns value through arg parameter*/
static PRIntn PR_CALLBACK
crlgen_CommitEntryData(PLHashEntry *he, PRIntn i, void *arg)
{
    CRLGENEntryData *data = NULL;

    PORT_Assert(he);
    if (!he) {
        return HT_ENUMERATE_NEXT;
    }
    data = (CRLGENEntryData *)he->value;

    PORT_Assert(data);
    PORT_Assert(arg);

    if (data) {
        struct commitData *dt = (struct commitData *)arg;
        dt->entries[dt->pos++] = data->entry;
        destroyEntryData(data);
    }
    return HT_ENUMERATE_NEXT;
}

/* Copy char * datainto allocated in arena SECItem */
static SECStatus
crlgen_SetString(PLArenaPool *arena, const char *dataIn, SECItem *value)
{
    SECItem item;

    PORT_Assert(arena && dataIn);
    if (!arena || !dataIn) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    item.data = (void *)dataIn;
    item.len = PORT_Strlen(dataIn);

    return SECITEM_CopyItem(arena, value, &item);
}

/* Creates CERTGeneralName from parsed data for the Authority Key Extension */
static CERTGeneralName *
crlgen_GetGeneralName(PLArenaPool *arena, CRLGENGeneratorData *crlGenData,
                      const char *data)
{
    CERTGeneralName *namesList = NULL;
    CERTGeneralName *current;
    CERTGeneralName *tail = NULL;
    SECStatus rv = SECSuccess;
    const char *nextChunk = NULL;
    const char *currData = NULL;
    int intValue;
    char buffer[512];
    void *mark;

    if (!data)
        return NULL;
    PORT_Assert(arena);
    if (!arena) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    mark = PORT_ArenaMark(arena);

    nextChunk = data;
    currData = data;
    do {
        int nameLen = 0;
        char name[128];
        const char *sepPrt = NULL;
        nextChunk = PORT_Strchr(currData, '|');
        if (!nextChunk)
            nextChunk = data + strlen(data);
        sepPrt = PORT_Strchr(currData, ':');
        if (sepPrt == NULL || sepPrt >= nextChunk) {
            *buffer = '\0';
            sepPrt = nextChunk;
        } else {
            PORT_Memcpy(buffer, sepPrt + 1,
                        (nextChunk - sepPrt - 1));
            buffer[nextChunk - sepPrt - 1] = '\0';
        }
        nameLen = PR_MIN(sepPrt - currData, sizeof(name) - 1);
        PORT_Memcpy(name, currData, nameLen);
        name[nameLen] = '\0';
        currData = nextChunk + 1;

        if (!PORT_Strcmp(name, "otherName"))
            intValue = certOtherName;
        else if (!PORT_Strcmp(name, "rfc822Name"))
            intValue = certRFC822Name;
        else if (!PORT_Strcmp(name, "dnsName"))
            intValue = certDNSName;
        else if (!PORT_Strcmp(name, "x400Address"))
            intValue = certX400Address;
        else if (!PORT_Strcmp(name, "directoryName"))
            intValue = certDirectoryName;
        else if (!PORT_Strcmp(name, "ediPartyName"))
            intValue = certEDIPartyName;
        else if (!PORT_Strcmp(name, "URI"))
            intValue = certURI;
        else if (!PORT_Strcmp(name, "ipAddress"))
            intValue = certIPAddress;
        else if (!PORT_Strcmp(name, "registerID"))
            intValue = certRegisterID;
        else
            intValue = -1;

        if (intValue >= certOtherName && intValue <= certRegisterID) {
            if (namesList == NULL) {
                namesList = current = tail = PORT_ArenaZNew(arena,
                                                            CERTGeneralName);
            } else {
                current = PORT_ArenaZNew(arena, CERTGeneralName);
            }
            if (current == NULL) {
                rv = SECFailure;
                break;
            }
        } else {
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            break;
        }
        current->type = intValue;
        switch (current->type) {
            case certURI:
            case certDNSName:
            case certRFC822Name:
                current->name.other.data = PORT_ArenaAlloc(arena, strlen(buffer));
                if (current->name.other.data == NULL) {
                    rv = SECFailure;
                    break;
                }
                PORT_Memcpy(current->name.other.data, buffer,
                            current->name.other.len = strlen(buffer));
                break;

            case certEDIPartyName:
            case certIPAddress:
            case certOtherName:
            case certRegisterID:
            case certX400Address: {

                current->name.other.data = PORT_ArenaAlloc(arena, strlen(buffer) + 2);
                if (current->name.other.data == NULL) {
                    rv = SECFailure;
                    break;
                }

                PORT_Memcpy(current->name.other.data + 2, buffer, strlen(buffer));
                /* This may not be accurate for all cases.For now, use this tag type */
                current->name.other.data[0] = (char)(((current->type - 1) & 0x1f) | 0x80);
                current->name.other.data[1] = (char)strlen(buffer);
                current->name.other.len = strlen(buffer) + 2;
                break;
            }

            case certDirectoryName: {
                CERTName *directoryName = NULL;

                directoryName = CERT_AsciiToName(buffer);
                if (!directoryName) {
                    rv = SECFailure;
                    break;
                }

                rv = CERT_CopyName(arena, &current->name.directoryName, directoryName);
                CERT_DestroyName(directoryName);

                break;
            }
        }
        if (rv != SECSuccess)
            break;
        current->l.next = &(namesList->l);
        current->l.prev = &(tail->l);
        tail->l.next = &(current->l);
        tail = current;

    } while (nextChunk != data + strlen(data));

    if (rv != SECSuccess) {
        PORT_ArenaRelease(arena, mark);
        namesList = NULL;
    }
    return (namesList);
}

/* Creates CERTGeneralName from parsed data for the Authority Key Extension */
static CERTGeneralName *
crlgen_DistinguishedName(PLArenaPool *arena, CRLGENGeneratorData *crlGenData,
                         const char *data)
{
    CERTName *directoryName = NULL;
    CERTGeneralName *current;
    SECStatus rv = SECFailure;
    void *mark;

    if (!data)
        return NULL;
    PORT_Assert(arena);
    if (!arena) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    mark = PORT_ArenaMark(arena);

    current = PORT_ArenaZNew(arena, CERTGeneralName);
    if (current == NULL) {
        goto loser;
    }
    current->type = certDirectoryName;
    current->l.next = &current->l;
    current->l.prev = &current->l;

    directoryName = CERT_AsciiToName((char *)data);
    if (!directoryName) {
        goto loser;
    }

    rv = CERT_CopyName(arena, &current->name.directoryName, directoryName);
    CERT_DestroyName(directoryName);

loser:
    if (rv != SECSuccess) {
        PORT_SetError(rv);
        PORT_ArenaRelease(arena, mark);
        current = NULL;
    }
    return (current);
}

/* Adding Authority Key ID extension to extension handle. */
static SECStatus
crlgen_AddAuthKeyID(CRLGENGeneratorData *crlGenData,
                    const char **dataArr)
{
    void *extHandle = NULL;
    CERTAuthKeyID *authKeyID = NULL;
    PLArenaPool *arena = NULL;
    SECStatus rv = SECSuccess;

    PORT_Assert(dataArr && crlGenData);
    if (!crlGenData || !dataArr) {
        return SECFailure;
    }

    extHandle = crlGenData->crlExtHandle;

    if (!dataArr[0] || !dataArr[1] || !dataArr[2]) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        crlgen_PrintError(crlGenData->parsedLineNum,
                          "insufficient number of parameters.\n");
        return SECFailure;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) {
        return SECFailure;
    }

    authKeyID = PORT_ArenaZNew(arena, CERTAuthKeyID);
    if (authKeyID == NULL) {
        rv = SECFailure;
        goto loser;
    }

    if (dataArr[3] == NULL) {
        rv = crlgen_SetString(arena, dataArr[2], &authKeyID->keyID);
        if (rv != SECSuccess)
            goto loser;
    } else {
        rv = crlgen_SetString(arena, dataArr[3],
                              &authKeyID->authCertSerialNumber);
        if (rv != SECSuccess)
            goto loser;

        authKeyID->authCertIssuer =
            crlgen_DistinguishedName(arena, crlGenData, dataArr[2]);
        if (authKeyID->authCertIssuer == NULL && SECFailure == PORT_GetError()) {
            crlgen_PrintError(crlGenData->parsedLineNum, "syntax error.\n");
            rv = SECFailure;
            goto loser;
        }
    }

    rv =
        SECU_EncodeAndAddExtensionValue(arena, extHandle, authKeyID,
                                        (*dataArr[1] == '1') ? PR_TRUE : PR_FALSE,
                                        SEC_OID_X509_AUTH_KEY_ID,
                                        (EXTEN_EXT_VALUE_ENCODER)CERT_EncodeAuthKeyID);
loser:
    if (arena)
        PORT_FreeArena(arena, PR_FALSE);
    return rv;
}

/* Creates and add Subject Alternative Names extension */
static SECStatus
crlgen_AddIssuerAltNames(CRLGENGeneratorData *crlGenData,
                         const char **dataArr)
{
    CERTGeneralName *nameList = NULL;
    PLArenaPool *arena = NULL;
    void *extHandle = NULL;
    SECStatus rv = SECSuccess;

    PORT_Assert(dataArr && crlGenData);
    if (!crlGenData || !dataArr) {
        return SECFailure;
    }

    if (!dataArr || !dataArr[0] || !dataArr[1] || !dataArr[2]) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        crlgen_PrintError(crlGenData->parsedLineNum,
                          "insufficient number of arguments.\n");
        return SECFailure;
    }

    PORT_Assert(dataArr && crlGenData);
    if (!crlGenData || !dataArr) {
        return SECFailure;
    }

    extHandle = crlGenData->crlExtHandle;

    if (!dataArr[0] || !dataArr[1] || !dataArr[2]) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        crlgen_PrintError(crlGenData->parsedLineNum,
                          "insufficient number of parameters.\n");
        return SECFailure;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) {
        return SECFailure;
    }

    nameList = crlgen_GetGeneralName(arena, crlGenData, dataArr[2]);
    if (nameList == NULL) {
        crlgen_PrintError(crlGenData->parsedLineNum, "syntax error.\n");
        rv = SECFailure;
        goto loser;
    }

    rv =
        SECU_EncodeAndAddExtensionValue(arena, extHandle, nameList,
                                        (*dataArr[1] == '1') ? PR_TRUE : PR_FALSE,
                                        SEC_OID_X509_ISSUER_ALT_NAME,
                                        (EXTEN_EXT_VALUE_ENCODER)CERT_EncodeAltNameExtension);
loser:
    if (arena)
        PORT_FreeArena(arena, PR_FALSE);
    return rv;
}

/* Creates and adds CRLNumber extension to extension handle.
 * Since, this is CRL extension, extension handle is the one
 * related to CRL extensions */
static SECStatus
crlgen_AddCrlNumber(CRLGENGeneratorData *crlGenData, const char **dataArr)
{
    PLArenaPool *arena = NULL;
    SECItem encodedItem;
    void *dummy;
    SECStatus rv = SECFailure;
    int code = 0;

    PORT_Assert(dataArr && crlGenData);
    if (!crlGenData || !dataArr) {
        goto loser;
    }

    if (!dataArr[0] || !dataArr[1] || !dataArr[2]) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        crlgen_PrintError(crlGenData->parsedLineNum,
                          "insufficient number of arguments.\n");
        goto loser;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        goto loser;
    }

    code = atoi(dataArr[2]);
    if (code == 0 && *dataArr[2] != '0') {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }

    dummy = SEC_ASN1EncodeInteger(arena, &encodedItem, code);
    if (!dummy) {
        rv = SECFailure;
        goto loser;
    }

    rv = CERT_AddExtension(crlGenData->crlExtHandle, SEC_OID_X509_CRL_NUMBER,
                           &encodedItem,
                           (*dataArr[1] == '1') ? PR_TRUE : PR_FALSE,
                           PR_TRUE);

loser:
    if (arena)
        PORT_FreeArena(arena, PR_FALSE);
    return rv;
}

/* Creates Cert Revocation Reason code extension. Encodes it and
 * returns as SECItem structure */
static SECItem *
crlgen_CreateReasonCode(PLArenaPool *arena, const char **dataArr,
                        int *extCode)
{
    SECItem *encodedItem;
    void *dummy;
    void *mark = NULL;
    int code = 0;

    PORT_Assert(arena && dataArr);
    if (!arena || !dataArr) {
        goto loser;
    }

    mark = PORT_ArenaMark(arena);

    encodedItem = PORT_ArenaZNew(arena, SECItem);
    if (encodedItem == NULL) {
        goto loser;
    }

    if (dataArr[2] == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }

    code = atoi(dataArr[2]);
    /* aACompromise(10) is the last possible of the values
     * for the Reason Core Extension */
    if ((code == 0 && *dataArr[2] != '0') || code > 10) {

        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        goto loser;
    }

    dummy = SEC_ASN1EncodeInteger(arena, encodedItem, code);
    if (!dummy) {
        goto loser;
    }

    *extCode = SEC_OID_X509_REASON_CODE;
    return encodedItem;

loser:
    if (mark) {
        PORT_ArenaRelease(arena, mark);
    }
    return NULL;
}

/* Creates Cert Invalidity Date extension. Encodes it and
 * returns as SECItem structure */
static SECItem *
crlgen_CreateInvalidityDate(PLArenaPool *arena, const char **dataArr,
                            int *extCode)
{
    SECItem *encodedItem;
    int length = 0;
    void *mark = NULL;

    PORT_Assert(arena && dataArr);
    if (!arena || !dataArr) {
        goto loser;
    }

    mark = PORT_ArenaMark(arena);

    encodedItem = PORT_ArenaZNew(arena, SECItem);
    if (encodedItem == NULL) {
        goto loser;
    }

    length = PORT_Strlen(dataArr[2]);

    encodedItem->type = siGeneralizedTime;
    encodedItem->data = PORT_ArenaAlloc(arena, length);
    if (!encodedItem->data) {
        goto loser;
    }

    PORT_Memcpy(encodedItem->data, dataArr[2], (encodedItem->len = length) * sizeof(char));

    *extCode = SEC_OID_X509_INVALID_DATE;
    return encodedItem;

loser:
    if (mark) {
        PORT_ArenaRelease(arena, mark);
    }
    return NULL;
}

/* Creates(by calling extCreator function) and adds extension to a set
 * of already added certs. Uses values of rangeFrom and rangeTo from
 * CRLGENCrlGenCtl structure for identifying the inclusive set of certs */
static SECStatus
crlgen_AddEntryExtension(CRLGENGeneratorData *crlGenData,
                         const char **dataArr, char *extName,
                         SECItem *(*extCreator)(PLArenaPool *arena,
                                                const char **dataArr,
                                                int *extCode))
{
    PRUint64 i = 0;
    SECStatus rv = SECFailure;
    int extCode = 0;
    PRUint64 lastRange;
    SECItem *ext = NULL;
    PLArenaPool *arena = NULL;

    PORT_Assert(crlGenData && dataArr);
    if (!crlGenData || !dataArr) {
        goto loser;
    }

    if (!dataArr[0] || !dataArr[1]) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        crlgen_PrintError(crlGenData->parsedLineNum,
                          "insufficient number of arguments.\n");
    }

    lastRange = crlGenData->rangeTo - crlGenData->rangeFrom + 1;

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL) {
        goto loser;
    }

    ext = extCreator(arena, dataArr, &extCode);
    if (ext == NULL) {
        crlgen_PrintError(crlGenData->parsedLineNum,
                          "got error while creating extension: %s\n",
                          extName);
        goto loser;
    }

    for (i = 0; i < lastRange; i++) {
        CRLGENEntryData *extData = NULL;
        void *extHandle = NULL;
        SECItem *certIdItem =
            SEC_ASN1EncodeInteger(arena, NULL,
                                  crlGenData->rangeFrom + i);
        if (!certIdItem) {
            rv = SECFailure;
            goto loser;
        }

        extData = crlgen_FindEntry(crlGenData, certIdItem);
        if (!extData) {
            crlgen_PrintError(crlGenData->parsedLineNum,
                              "can not add extension: crl entry "
                              "(serial number: %d) is not in the list yet.\n",
                              crlGenData->rangeFrom + i);
            continue;
        }

        extHandle = extData->extHandle;
        if (extHandle == NULL) {
            extHandle = extData->extHandle =
                CERT_StartCRLEntryExtensions(&crlGenData->signCrl->crl,
                                             (CERTCrlEntry *)extData->entry);
        }
        rv = CERT_AddExtension(extHandle, extCode, ext,
                               (*dataArr[1] == '1') ? PR_TRUE : PR_FALSE,
                               PR_TRUE);
        if (rv == SECFailure) {
            goto loser;
        }
    }

loser:
    if (arena)
        PORT_FreeArena(arena, PR_FALSE);
    return rv;
}

/* Commits all added entries and their's extensions into CRL. */
SECStatus
CRLGEN_CommitExtensionsAndEntries(CRLGENGeneratorData *crlGenData)
{
    int size = 0;
    CERTCrl *crl;
    PLArenaPool *arena;
    SECStatus rv = SECSuccess;
    void *mark;

    PORT_Assert(crlGenData && crlGenData->signCrl && crlGenData->signCrl->arena);
    if (!crlGenData || !crlGenData->signCrl || !crlGenData->signCrl->arena) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    arena = crlGenData->signCrl->arena;
    crl = &crlGenData->signCrl->crl;

    mark = PORT_ArenaMark(arena);

    if (crlGenData->crlExtHandle)
        CERT_FinishExtensions(crlGenData->crlExtHandle);

    size = crlGenData->entryDataHashTable->nentries;
    crl->entries = NULL;
    if (size) {
        crl->entries = PORT_ArenaZNewArray(arena, CERTCrlEntry *, size + 1);
        if (!crl->entries) {
            rv = SECFailure;
        } else {
            struct commitData dt;
            dt.entries = crl->entries;
            dt.pos = 0;
            PL_HashTableEnumerateEntries(crlGenData->entryDataHashTable,
                                         &crlgen_CommitEntryData, &dt);
            /* Last should be NULL */
            crl->entries[size] = NULL;
        }
    }

    if (rv != SECSuccess)
        PORT_ArenaRelease(arena, mark);
    return rv;
}

/* Initializes extHandle with data from extensions array */
static SECStatus
crlgen_InitExtensionHandle(void *extHandle,
                           CERTCertExtension **extensions)
{
    CERTCertExtension *extension = NULL;

    if (!extensions)
        return SECSuccess;

    PORT_Assert(extHandle != NULL);
    if (!extHandle) {
        return SECFailure;
    }

    extension = *extensions;
    while (extension) {
        SECOidTag oidTag = SECOID_FindOIDTag(&extension->id);
        /* shell we skip unknown extensions? */
        CERT_AddExtension(extHandle, oidTag, &extension->value,
                          (extension->critical.len != 0) ? PR_TRUE : PR_FALSE,
                          PR_FALSE);
        extension = *(++extensions);
    }
    return SECSuccess;
}

/* Used for initialization of extension handles for crl and certs
 * extensions from existing CRL data then modifying existing CRL.*/
SECStatus
CRLGEN_ExtHandleInit(CRLGENGeneratorData *crlGenData)
{
    CERTCrl *crl = NULL;
    PRUint64 maxSN = 0;

    PORT_Assert(crlGenData && crlGenData->signCrl &&
                crlGenData->entryDataHashTable);
    if (!crlGenData || !crlGenData->signCrl ||
        !crlGenData->entryDataHashTable) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    crl = &crlGenData->signCrl->crl;
    crlGenData->crlExtHandle = CERT_StartCRLExtensions(crl);
    crlgen_InitExtensionHandle(crlGenData->crlExtHandle,
                               crl->extensions);
    crl->extensions = NULL;

    if (crl->entries) {
        CERTCrlEntry **entry = crl->entries;
        while (*entry) {
            PRUint64 sn = DER_GetInteger(&(*entry)->serialNumber);
            CRLGENEntryData *extData =
                crlgen_PlaceAnEntry(crlGenData, *entry, &(*entry)->serialNumber);
            if ((*entry)->extensions) {
                extData->extHandle =
                    CERT_StartCRLEntryExtensions(&crlGenData->signCrl->crl,
                                                 (CERTCrlEntry *)extData->entry);
                if (crlgen_InitExtensionHandle(extData->extHandle,
                                               (*entry)->extensions) == SECFailure)
                    return SECFailure;
            }
            (*entry)->extensions = NULL;
            entry++;
            maxSN = PR_MAX(maxSN, sn);
        }
    }

    crlGenData->rangeFrom = crlGenData->rangeTo = maxSN + 1;
    return SECSuccess;
}

/*****************************************************************************
 * Parser trigger functions start here
 */

/* Sets new internal range value for add/rm certs.*/
static SECStatus
crlgen_SetNewRangeField(CRLGENGeneratorData *crlGenData, char *value)
{
    long rangeFrom = 0, rangeTo = 0;
    char *dashPos = NULL;

    PORT_Assert(crlGenData);
    if (!crlGenData) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (value == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        crlgen_PrintError(crlGenData->parsedLineNum,
                          "insufficient number of arguments.\n");
        return SECFailure;
    }

    if ((dashPos = strchr(value, '-')) != NULL) {
        char *rangeToS, *rangeFromS = value;
        *dashPos = '\0';
        rangeFrom = atoi(rangeFromS);
        *dashPos = '-';

        rangeToS = (char *)(dashPos + 1);
        rangeTo = atol(rangeToS);
    } else {
        rangeFrom = atol(value);
        rangeTo = rangeFrom;
    }

    if (rangeFrom < 1 || rangeTo < rangeFrom) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        crlgen_PrintError(crlGenData->parsedLineNum,
                          "bad cert id range: %s.\n", value);
        return SECFailure;
    }

    crlGenData->rangeFrom = rangeFrom;
    crlGenData->rangeTo = rangeTo;

    return SECSuccess;
}

/* Changes issuer subject field in CRL. By default this data is taken from
 * issuer cert subject field.Not yet implemented */
static SECStatus
crlgen_SetIssuerField(CRLGENGeneratorData *crlGenData, char *value)
{
    crlgen_PrintError(crlGenData->parsedLineNum,
                      "Can not change CRL issuer field.\n");
    return SECFailure;
}

/* Encode and sets CRL thisUpdate and nextUpdate time fields*/
static SECStatus
crlgen_SetTimeField(CRLGENGeneratorData *crlGenData, char *value,
                    PRBool setThisUpdate)
{
    CERTSignedCrl *signCrl;
    PLArenaPool *arena;
    CERTCrl *crl;
    int length = 0;
    SECItem *timeDest = NULL;

    PORT_Assert(crlGenData && crlGenData->signCrl &&
                crlGenData->signCrl->arena);
    if (!crlGenData || !crlGenData->signCrl || !crlGenData->signCrl->arena) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    signCrl = crlGenData->signCrl;
    arena = signCrl->arena;
    crl = &signCrl->crl;

    if (value == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        crlgen_PrintError(crlGenData->parsedLineNum,
                          "insufficient number of arguments.\n");
        return SECFailure;
    }
    length = PORT_Strlen(value);

    if (setThisUpdate == PR_TRUE) {
        timeDest = &crl->lastUpdate;
    } else {
        timeDest = &crl->nextUpdate;
    }

    timeDest->type = siGeneralizedTime;
    timeDest->data = PORT_ArenaAlloc(arena, length);
    if (!timeDest->data) {
        return SECFailure;
    }
    PORT_Memcpy(timeDest->data, value, length);
    timeDest->len = length;

    return SECSuccess;
}

/* Adds new extension into CRL or added cert handles */
static SECStatus
crlgen_AddExtension(CRLGENGeneratorData *crlGenData, const char **extData)
{
    PORT_Assert(crlGenData && crlGenData->crlExtHandle);
    if (!crlGenData || !crlGenData->crlExtHandle) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (extData == NULL || *extData == NULL) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        crlgen_PrintError(crlGenData->parsedLineNum,
                          "insufficient number of arguments.\n");
        return SECFailure;
    }
    if (!PORT_Strcmp(*extData, "authKeyId"))
        return crlgen_AddAuthKeyID(crlGenData, extData);
    else if (!PORT_Strcmp(*extData, "issuerAltNames"))
        return crlgen_AddIssuerAltNames(crlGenData, extData);
    else if (!PORT_Strcmp(*extData, "crlNumber"))
        return crlgen_AddCrlNumber(crlGenData, extData);
    else if (!PORT_Strcmp(*extData, "reasonCode"))
        return crlgen_AddEntryExtension(crlGenData, extData, "reasonCode",
                                        crlgen_CreateReasonCode);
    else if (!PORT_Strcmp(*extData, "invalidityDate"))
        return crlgen_AddEntryExtension(crlGenData, extData, "invalidityDate",
                                        crlgen_CreateInvalidityDate);
    else {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        crlgen_PrintError(crlGenData->parsedLineNum,
                          "insufficient number of arguments.\n");
        return SECFailure;
    }
}

/* Created CRLGENEntryData for cert with serial number certId and
 * adds it to entryDataHashTable. certId can be a single cert serial
 * number or an inclusive rage of certs */
static SECStatus
crlgen_AddCert(CRLGENGeneratorData *crlGenData,
               char *certId, char *revocationDate)
{
    CERTSignedCrl *signCrl;
    SECItem *certIdItem;
    PLArenaPool *arena;
    PRUint64 rangeFrom = 0, rangeTo = 0, i = 0;
    int timeValLength = -1;
    SECStatus rv = SECFailure;
    void *mark;

    PORT_Assert(crlGenData && crlGenData->signCrl &&
                crlGenData->signCrl->arena);
    if (!crlGenData || !crlGenData->signCrl || !crlGenData->signCrl->arena) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    signCrl = crlGenData->signCrl;
    arena = signCrl->arena;

    if (!certId || !revocationDate) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        crlgen_PrintError(crlGenData->parsedLineNum,
                          "insufficient number of arguments.\n");
        return SECFailure;
    }

    timeValLength = strlen(revocationDate);

    if (crlgen_SetNewRangeField(crlGenData, certId) == SECFailure &&
        certId) {
        return SECFailure;
    }
    rangeFrom = crlGenData->rangeFrom;
    rangeTo = crlGenData->rangeTo;

    for (i = 0; i < rangeTo - rangeFrom + 1; i++) {
        CERTCrlEntry *entry;
        mark = PORT_ArenaMark(arena);
        entry = PORT_ArenaZNew(arena, CERTCrlEntry);
        if (entry == NULL) {
            goto loser;
        }

        certIdItem = SEC_ASN1EncodeInteger(arena, &entry->serialNumber,
                                           rangeFrom + i);
        if (!certIdItem) {
            goto loser;
        }

        if (crlgen_FindEntry(crlGenData, certIdItem)) {
            crlgen_PrintError(crlGenData->parsedLineNum,
                              "entry already exists. Use \"range\" "
                              "and \"rmcert\" before adding a new one with the "
                              "same serial number %ld\n",
                              rangeFrom + i);
            goto loser;
        }

        entry->serialNumber.type = siBuffer;

        entry->revocationDate.type = siGeneralizedTime;

        entry->revocationDate.data =
            PORT_ArenaAlloc(arena, timeValLength);
        if (entry->revocationDate.data == NULL) {
            goto loser;
        }

        PORT_Memcpy(entry->revocationDate.data, revocationDate,
                    timeValLength * sizeof(char));
        entry->revocationDate.len = timeValLength;

        entry->extensions = NULL;
        if (!crlgen_PlaceAnEntry(crlGenData, entry, certIdItem)) {
            goto loser;
        }
        mark = NULL;
    }

    rv = SECSuccess;
loser:
    if (mark) {
        PORT_ArenaRelease(arena, mark);
    }
    return rv;
}

/* Removes certs from entryDataHashTable which have certId serial number.
 * certId can have value of a range of certs */
static SECStatus
crlgen_RmCert(CRLGENGeneratorData *crlGenData, char *certId)
{
    PRUint64 i = 0;

    PORT_Assert(crlGenData && certId);
    if (!crlGenData || !certId) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (crlgen_SetNewRangeField(crlGenData, certId) == SECFailure &&
        certId) {
        return SECFailure;
    }

    for (i = 0; i < crlGenData->rangeTo - crlGenData->rangeFrom + 1; i++) {
        SECItem *certIdItem = SEC_ASN1EncodeInteger(NULL, NULL,
                                                    crlGenData->rangeFrom + i);
        if (certIdItem) {
            CRLGENEntryData *extData =
                crlgen_FindEntry(crlGenData, certIdItem);
            if (!extData) {
                printf("Cert with id %s is not in the list\n", certId);
            } else {
                crlgen_RmEntry(crlGenData, certIdItem);
            }
            SECITEM_FreeItem(certIdItem, PR_TRUE);
        }
    }

    return SECSuccess;
}

/*************************************************************************
 * Lex Parser Helper functions are used to store parsed information
 * in context related structures. Context(or state) is identified base on
 * a type of a instruction parser currently is going through. New context
 * is identified by first token in a line. It can be addcert context,
 * addext context, etc. */

/* Updates CRL field depending on current context */
static SECStatus
crlgen_updateCrlFn_field(CRLGENGeneratorData *crlGenData, void *str)
{
    CRLGENCrlField *fieldStr = (CRLGENCrlField *)str;

    PORT_Assert(crlGenData);
    if (!crlGenData) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    switch (crlGenData->contextId) {
        case CRLGEN_ISSUER_CONTEXT:
            crlgen_SetIssuerField(crlGenData, fieldStr->value);
            break;
        case CRLGEN_UPDATE_CONTEXT:
            return crlgen_SetTimeField(crlGenData, fieldStr->value, PR_TRUE);
            break;
        case CRLGEN_NEXT_UPDATE_CONTEXT:
            return crlgen_SetTimeField(crlGenData, fieldStr->value, PR_FALSE);
            break;
        case CRLGEN_CHANGE_RANGE_CONTEXT:
            return crlgen_SetNewRangeField(crlGenData, fieldStr->value);
            break;
        default:
            crlgen_PrintError(crlGenData->parsedLineNum,
                              "syntax error (unknow token type: %d)\n",
                              crlGenData->contextId);
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            return SECFailure;
    }
    return SECSuccess;
}

/* Sets parsed data for CRL field update into temporary structure */
static SECStatus
crlgen_setNextDataFn_field(CRLGENGeneratorData *crlGenData, void *str,
                           void *data, unsigned short dtype)
{
    CRLGENCrlField *fieldStr = (CRLGENCrlField *)str;

    PORT_Assert(crlGenData);
    if (!crlGenData) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    switch (crlGenData->contextId) {
        case CRLGEN_CHANGE_RANGE_CONTEXT:
            if (dtype != CRLGEN_TYPE_DIGIT && dtype != CRLGEN_TYPE_DIGIT_RANGE) {
                crlgen_PrintError(crlGenData->parsedLineNum,
                                  "range value should have "
                                  "numeric or numeric range values.\n");
                return SECFailure;
            }
            break;
        case CRLGEN_NEXT_UPDATE_CONTEXT:
        case CRLGEN_UPDATE_CONTEXT:
            if (dtype != CRLGEN_TYPE_ZDATE) {
                crlgen_PrintError(crlGenData->parsedLineNum,
                                  "bad formated date. Should be "
                                  "YYYYMMDDHHMMSSZ.\n");
                return SECFailure;
            }
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            crlgen_PrintError(crlGenData->parsedLineNum,
                              "syntax error (unknow token type: %d).\n",
                              crlGenData->contextId, data);
            return SECFailure;
    }
    fieldStr->value = PORT_Strdup(data);
    if (!fieldStr->value) {
        return SECFailure;
    }
    return SECSuccess;
}

/* Triggers cert entries update depending on current context */
static SECStatus
crlgen_updateCrlFn_cert(CRLGENGeneratorData *crlGenData, void *str)
{
    CRLGENCertEntry *certStr = (CRLGENCertEntry *)str;

    PORT_Assert(crlGenData);
    if (!crlGenData) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    switch (crlGenData->contextId) {
        case CRLGEN_ADD_CERT_CONTEXT:
            return crlgen_AddCert(crlGenData, certStr->certId,
                                  certStr->revocationTime);
        case CRLGEN_RM_CERT_CONTEXT:
            return crlgen_RmCert(crlGenData, certStr->certId);
        default:
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            crlgen_PrintError(crlGenData->parsedLineNum,
                              "syntax error (unknow token type: %d).\n",
                              crlGenData->contextId);
            return SECFailure;
    }
}

/* Sets parsed data for CRL entries update into temporary structure */
static SECStatus
crlgen_setNextDataFn_cert(CRLGENGeneratorData *crlGenData, void *str,
                          void *data, unsigned short dtype)
{
    CRLGENCertEntry *certStr = (CRLGENCertEntry *)str;

    PORT_Assert(crlGenData);
    if (!crlGenData) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    switch (dtype) {
        case CRLGEN_TYPE_DIGIT:
        case CRLGEN_TYPE_DIGIT_RANGE:
            certStr->certId = PORT_Strdup(data);
            if (!certStr->certId) {
                return SECFailure;
            }
            break;
        case CRLGEN_TYPE_DATE:
        case CRLGEN_TYPE_ZDATE:
            certStr->revocationTime = PORT_Strdup(data);
            if (!certStr->revocationTime) {
                return SECFailure;
            }
            break;
        default:
            PORT_SetError(SEC_ERROR_INVALID_ARGS);
            crlgen_PrintError(crlGenData->parsedLineNum,
                              "syntax error (unknow token type: %d).\n",
                              crlGenData->contextId);
            return SECFailure;
    }
    return SECSuccess;
}

/* Triggers cert entries/crl extension update */
static SECStatus
crlgen_updateCrlFn_extension(CRLGENGeneratorData *crlGenData, void *str)
{
    CRLGENExtensionEntry *extStr = (CRLGENExtensionEntry *)str;

    return crlgen_AddExtension(crlGenData, (const char **)extStr->extData);
}

/* Defines maximum number of fields extension may have */
#define MAX_EXT_DATA_LENGTH 10

/* Sets parsed extension data for CRL entries/CRL extensions update
 * into temporary structure */
static SECStatus
crlgen_setNextDataFn_extension(CRLGENGeneratorData *crlGenData, void *str,
                               void *data, unsigned short dtype)
{
    CRLGENExtensionEntry *extStr = (CRLGENExtensionEntry *)str;

    PORT_Assert(crlGenData);
    if (!crlGenData) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    if (extStr->extData == NULL) {
        extStr->extData = PORT_ZNewArray(char *, MAX_EXT_DATA_LENGTH);
        if (!extStr->extData) {
            return SECFailure;
        }
    }
    if (extStr->nextUpdatedData >= MAX_EXT_DATA_LENGTH) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        crlgen_PrintError(crlGenData->parsedLineNum,
                          "number of fields in extension "
                          "exceeded maximum allowed data length: %d.\n",
                          MAX_EXT_DATA_LENGTH);
        return SECFailure;
    }
    extStr->extData[extStr->nextUpdatedData] = PORT_Strdup(data);
    if (!extStr->extData[extStr->nextUpdatedData]) {
        return SECFailure;
    }
    extStr->nextUpdatedData += 1;

    return SECSuccess;
}

/****************************************************************************************
 * Top level functions are triggered directly by parser.
 */

/*
 * crl generation script parser recreates a temporary data staructure
 * for each line it is going through. This function cleans temp structure.
 */
void
crlgen_destroyTempData(CRLGENGeneratorData *crlGenData)
{
    if (crlGenData->contextId != CRLGEN_UNKNOWN_CONTEXT) {
        switch (crlGenData->contextId) {
            case CRLGEN_ISSUER_CONTEXT:
            case CRLGEN_UPDATE_CONTEXT:
            case CRLGEN_NEXT_UPDATE_CONTEXT:
            case CRLGEN_CHANGE_RANGE_CONTEXT:
                if (crlGenData->crlField->value)
                    PORT_Free(crlGenData->crlField->value);
                PORT_Free(crlGenData->crlField);
                break;
            case CRLGEN_ADD_CERT_CONTEXT:
            case CRLGEN_RM_CERT_CONTEXT:
                if (crlGenData->certEntry->certId)
                    PORT_Free(crlGenData->certEntry->certId);
                if (crlGenData->certEntry->revocationTime)
                    PORT_Free(crlGenData->certEntry->revocationTime);
                PORT_Free(crlGenData->certEntry);
                break;
            case CRLGEN_ADD_EXTENSION_CONTEXT:
                if (crlGenData->extensionEntry->extData) {
                    int i = 0;
                    for (; i < crlGenData->extensionEntry->nextUpdatedData; i++)
                        PORT_Free(*(crlGenData->extensionEntry->extData + i));
                    PORT_Free(crlGenData->extensionEntry->extData);
                }
                PORT_Free(crlGenData->extensionEntry);
                break;
        }
        crlGenData->contextId = CRLGEN_UNKNOWN_CONTEXT;
    }
}

SECStatus
crlgen_updateCrl(CRLGENGeneratorData *crlGenData)
{
    SECStatus rv = SECSuccess;

    PORT_Assert(crlGenData);
    if (!crlGenData) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    switch (crlGenData->contextId) {
        case CRLGEN_ISSUER_CONTEXT:
        case CRLGEN_UPDATE_CONTEXT:
        case CRLGEN_NEXT_UPDATE_CONTEXT:
        case CRLGEN_CHANGE_RANGE_CONTEXT:
            rv = crlGenData->crlField->updateCrlFn(crlGenData, crlGenData->crlField);
            break;
        case CRLGEN_RM_CERT_CONTEXT:
        case CRLGEN_ADD_CERT_CONTEXT:
            rv = crlGenData->certEntry->updateCrlFn(crlGenData, crlGenData->certEntry);
            break;
        case CRLGEN_ADD_EXTENSION_CONTEXT:
            rv = crlGenData->extensionEntry->updateCrlFn(crlGenData, crlGenData->extensionEntry);
            break;
        case CRLGEN_UNKNOWN_CONTEXT:
            break;
        default:
            crlgen_PrintError(crlGenData->parsedLineNum,
                              "unknown lang context type code: %d.\n",
                              crlGenData->contextId);
            PORT_Assert(0);
            return SECFailure;
    }
    /* Clrean structures after crl update */
    crlgen_destroyTempData(crlGenData);

    crlGenData->parsedLineNum += 1;

    return rv;
}

SECStatus
crlgen_setNextData(CRLGENGeneratorData *crlGenData, void *data,
                   unsigned short dtype)
{
    SECStatus rv = SECSuccess;

    PORT_Assert(crlGenData);
    if (!crlGenData) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    switch (crlGenData->contextId) {
        case CRLGEN_ISSUER_CONTEXT:
        case CRLGEN_UPDATE_CONTEXT:
        case CRLGEN_NEXT_UPDATE_CONTEXT:
        case CRLGEN_CHANGE_RANGE_CONTEXT:
            rv = crlGenData->crlField->setNextDataFn(crlGenData, crlGenData->crlField,
                                                     data, dtype);
            break;
        case CRLGEN_ADD_CERT_CONTEXT:
        case CRLGEN_RM_CERT_CONTEXT:
            rv = crlGenData->certEntry->setNextDataFn(crlGenData, crlGenData->certEntry,
                                                      data, dtype);
            break;
        case CRLGEN_ADD_EXTENSION_CONTEXT:
            rv =
                crlGenData->extensionEntry->setNextDataFn(crlGenData, crlGenData->extensionEntry, data, dtype);
            break;
        case CRLGEN_UNKNOWN_CONTEXT:
            break;
        default:
            crlgen_PrintError(crlGenData->parsedLineNum,
                              "unknown context type: %d.\n",
                              crlGenData->contextId);
            PORT_Assert(0);
            return SECFailure;
    }
    return rv;
}

SECStatus
crlgen_createNewLangStruct(CRLGENGeneratorData *crlGenData,
                           unsigned structType)
{
    PORT_Assert(crlGenData &&
                crlGenData->contextId == CRLGEN_UNKNOWN_CONTEXT);
    if (!crlGenData ||
        crlGenData->contextId != CRLGEN_UNKNOWN_CONTEXT) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    switch (structType) {
        case CRLGEN_ISSUER_CONTEXT:
        case CRLGEN_UPDATE_CONTEXT:
        case CRLGEN_NEXT_UPDATE_CONTEXT:
        case CRLGEN_CHANGE_RANGE_CONTEXT:
            crlGenData->crlField = PORT_New(CRLGENCrlField);
            if (!crlGenData->crlField) {
                return SECFailure;
            }
            crlGenData->contextId = structType;
            crlGenData->crlField->value = NULL;
            crlGenData->crlField->updateCrlFn = &crlgen_updateCrlFn_field;
            crlGenData->crlField->setNextDataFn = &crlgen_setNextDataFn_field;
            break;
        case CRLGEN_RM_CERT_CONTEXT:
        case CRLGEN_ADD_CERT_CONTEXT:
            crlGenData->certEntry = PORT_New(CRLGENCertEntry);
            if (!crlGenData->certEntry) {
                return SECFailure;
            }
            crlGenData->contextId = structType;
            crlGenData->certEntry->certId = 0;
            crlGenData->certEntry->revocationTime = NULL;
            crlGenData->certEntry->updateCrlFn = &crlgen_updateCrlFn_cert;
            crlGenData->certEntry->setNextDataFn = &crlgen_setNextDataFn_cert;
            break;
        case CRLGEN_ADD_EXTENSION_CONTEXT:
            crlGenData->extensionEntry = PORT_New(CRLGENExtensionEntry);
            if (!crlGenData->extensionEntry) {
                return SECFailure;
            }
            crlGenData->contextId = structType;
            crlGenData->extensionEntry->extData = NULL;
            crlGenData->extensionEntry->nextUpdatedData = 0;
            crlGenData->extensionEntry->updateCrlFn =
                &crlgen_updateCrlFn_extension;
            crlGenData->extensionEntry->setNextDataFn =
                &crlgen_setNextDataFn_extension;
            break;
        case CRLGEN_UNKNOWN_CONTEXT:
            break;
        default:
            crlgen_PrintError(crlGenData->parsedLineNum,
                              "unknown context type: %d.\n", structType);
            PORT_Assert(0);
            return SECFailure;
    }
    return SECSuccess;
}

/* Parser initialization function */
CRLGENGeneratorData *
CRLGEN_InitCrlGeneration(CERTSignedCrl *signCrl, PRFileDesc *src)
{
    CRLGENGeneratorData *crlGenData = NULL;

    PORT_Assert(signCrl && src);
    if (!signCrl || !src) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return NULL;
    }

    crlGenData = PORT_ZNew(CRLGENGeneratorData);
    if (!crlGenData) {
        return NULL;
    }

    crlGenData->entryDataHashTable =
        PL_NewHashTable(0, SECITEM_Hash, SECITEM_HashCompare,
                        PL_CompareValues, NULL, NULL);
    if (!crlGenData->entryDataHashTable) {
        PORT_Free(crlGenData);
        return NULL;
    }

    crlGenData->src = src;
    crlGenData->parsedLineNum = 1;
    crlGenData->contextId = CRLGEN_UNKNOWN_CONTEXT;
    crlGenData->signCrl = signCrl;
    crlGenData->rangeFrom = 0;
    crlGenData->rangeTo = 0;
    crlGenData->crlExtHandle = NULL;

    PORT_SetError(0);

    return crlGenData;
}

void
CRLGEN_FinalizeCrlGeneration(CRLGENGeneratorData *crlGenData)
{
    if (!crlGenData)
        return;
    if (crlGenData->src)
        PR_Close(crlGenData->src);
    PL_HashTableDestroy(crlGenData->entryDataHashTable);
    PORT_Free(crlGenData);
}
