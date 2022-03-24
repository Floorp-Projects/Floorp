/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pkcs11.h"

#ifndef DEVM_H
#include "devm.h"
#endif /* DEVM_H */

#ifndef CKHELPER_H
#include "ckhelper.h"
#endif /* CKHELPER_H */

#include "pk11func.h"
#include "dev3hack.h"
#include "secerr.h"

extern const NSSError NSS_ERROR_NOT_FOUND;
extern const NSSError NSS_ERROR_INVALID_ARGUMENT;
extern const NSSError NSS_ERROR_PKCS11;

/* The number of object handles to grab during each call to C_FindObjects */
#define OBJECT_STACK_SIZE 16

NSS_IMPLEMENT PRStatus
nssToken_Destroy(
    NSSToken *tok)
{
    if (tok) {
        if (PR_ATOMIC_DECREMENT(&tok->base.refCount) == 0) {
            PK11_FreeSlot(tok->pk11slot);
            PZ_DestroyLock(tok->base.lock);
            nssTokenObjectCache_Destroy(tok->cache);
            (void)nssSlot_Destroy(tok->slot);
            return nssArena_Destroy(tok->base.arena);
        }
    }
    return PR_SUCCESS;
}

NSS_IMPLEMENT void
nssToken_Remove(
    NSSToken *tok)
{
    nssTokenObjectCache_Clear(tok->cache);
}

NSS_IMPLEMENT NSSToken *
nssToken_AddRef(
    NSSToken *tok)
{
    PR_ATOMIC_INCREMENT(&tok->base.refCount);
    return tok;
}

NSS_IMPLEMENT NSSSlot *
nssToken_GetSlot(
    NSSToken *tok)
{
    return nssSlot_AddRef(tok->slot);
}

NSS_IMPLEMENT void *
nssToken_GetCryptokiEPV(
    NSSToken *token)
{
    return nssSlot_GetCryptokiEPV(token->slot);
}

NSS_IMPLEMENT nssSession *
nssToken_GetDefaultSession(
    NSSToken *token)
{
    return token->defaultSession;
}

NSS_IMPLEMENT NSSUTF8 *
nssToken_GetName(
    NSSToken *tok)
{
    if (tok == NULL) {
        return "";
    }
    if (tok->base.name[0] == 0) {
        (void)nssSlot_IsTokenPresent(tok->slot);
    }
    return tok->base.name;
}

NSS_IMPLEMENT NSSUTF8 *
NSSToken_GetName(
    NSSToken *token)
{
    return nssToken_GetName(token);
}

NSS_IMPLEMENT PRBool
nssToken_IsLoginRequired(
    NSSToken *token)
{
    return (token->ckFlags & CKF_LOGIN_REQUIRED);
}

NSS_IMPLEMENT PRBool
nssToken_NeedsPINInitialization(
    NSSToken *token)
{
    return (!(token->ckFlags & CKF_USER_PIN_INITIALIZED));
}

NSS_IMPLEMENT PRStatus
nssToken_DeleteStoredObject(
    nssCryptokiObject *instance)
{
    CK_RV ckrv;
    PRStatus status;
    PRBool createdSession = PR_FALSE;
    NSSToken *token = instance->token;
    nssSession *session = NULL;
    void *epv = nssToken_GetCryptokiEPV(instance->token);
    if (token->cache) {
        nssTokenObjectCache_RemoveObject(token->cache, instance);
    }
    if (instance->isTokenObject) {
        if (token->defaultSession &&
            nssSession_IsReadWrite(token->defaultSession)) {
            session = token->defaultSession;
        } else {
            session = nssSlot_CreateSession(token->slot, NULL, PR_TRUE);
            createdSession = PR_TRUE;
        }
    }
    if (session == NULL) {
        return PR_FAILURE;
    }
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(epv)->C_DestroyObject(session->handle, instance->handle);
    nssSession_ExitMonitor(session);
    if (createdSession) {
        nssSession_Destroy(session);
    }
    status = PR_SUCCESS;
    if (ckrv != CKR_OK) {
        status = PR_FAILURE;
        /* use the error stack to pass the PKCS #11 error out  */
        nss_SetError(ckrv);
        nss_SetError(NSS_ERROR_PKCS11);
    }
    return status;
}

static nssCryptokiObject *
import_object(
    NSSToken *tok,
    nssSession *sessionOpt,
    CK_ATTRIBUTE_PTR objectTemplate,
    CK_ULONG otsize)
{
    nssSession *session = NULL;
    PRBool createdSession = PR_FALSE;
    nssCryptokiObject *object = NULL;
    CK_OBJECT_HANDLE handle;
    CK_RV ckrv;
    void *epv = nssToken_GetCryptokiEPV(tok);
    if (nssCKObject_IsTokenObjectTemplate(objectTemplate, otsize)) {
        if (sessionOpt) {
            if (!nssSession_IsReadWrite(sessionOpt)) {
                nss_SetError(NSS_ERROR_INVALID_ARGUMENT);
                return NULL;
            }
            session = sessionOpt;
        } else if (tok->defaultSession &&
                   nssSession_IsReadWrite(tok->defaultSession)) {
            session = tok->defaultSession;
        } else {
            session = nssSlot_CreateSession(tok->slot, NULL, PR_TRUE);
            createdSession = PR_TRUE;
        }
    } else {
        session = (sessionOpt) ? sessionOpt : tok->defaultSession;
    }
    if (session == NULL) {
        nss_SetError(NSS_ERROR_INVALID_ARGUMENT);
        return NULL;
    }
    nssSession_EnterMonitor(session);
    ckrv = CKAPI(epv)->C_CreateObject(session->handle,
                                      objectTemplate, otsize,
                                      &handle);
    nssSession_ExitMonitor(session);
    if (ckrv == CKR_OK) {
        object = nssCryptokiObject_Create(tok, session, handle);
    } else {
        nss_SetError(ckrv);
        nss_SetError(NSS_ERROR_PKCS11);
    }
    if (createdSession) {
        nssSession_Destroy(session);
    }
    return object;
}

static nssCryptokiObject **
create_objects_from_handles(
    NSSToken *tok,
    nssSession *session,
    CK_OBJECT_HANDLE *handles,
    PRUint32 numH)
{
    nssCryptokiObject **objects;
    objects = nss_ZNEWARRAY(NULL, nssCryptokiObject *, numH + 1);
    if (objects) {
        PRInt32 i;
        for (i = 0; i < (PRInt32)numH; i++) {
            objects[i] = nssCryptokiObject_Create(tok, session, handles[i]);
            if (!objects[i]) {
                for (--i; i > 0; --i) {
                    nssCryptokiObject_Destroy(objects[i]);
                }
                nss_ZFreeIf(objects);
                objects = NULL;
                break;
            }
        }
    }
    return objects;
}

static nssCryptokiObject **
find_objects(
    NSSToken *tok,
    nssSession *sessionOpt,
    CK_ATTRIBUTE_PTR obj_template,
    CK_ULONG otsize,
    PRUint32 maximumOpt,
    PRStatus *statusOpt)
{
    CK_RV ckrv = CKR_OK;
    CK_ULONG count;
    CK_OBJECT_HANDLE *objectHandles = NULL;
    CK_OBJECT_HANDLE staticObjects[OBJECT_STACK_SIZE];
    PRUint32 arraySize, numHandles;
    void *epv = nssToken_GetCryptokiEPV(tok);
    nssCryptokiObject **objects;
    nssSession *session = (sessionOpt) ? sessionOpt : tok->defaultSession;

    /* Don't ask the module to use an invalid session handle. */
    if (!session || session->handle == CK_INVALID_HANDLE) {
        ckrv = CKR_SESSION_HANDLE_INVALID;
        goto loser;
    }

    /* the arena is only for the array of object handles */
    if (maximumOpt > 0) {
        arraySize = maximumOpt;
    } else {
        arraySize = OBJECT_STACK_SIZE;
    }
    numHandles = 0;
    if (arraySize <= OBJECT_STACK_SIZE) {
        objectHandles = staticObjects;
    } else {
        objectHandles = nss_ZNEWARRAY(NULL, CK_OBJECT_HANDLE, arraySize);
    }
    if (!objectHandles) {
        ckrv = CKR_HOST_MEMORY;
        goto loser;
    }
    nssSession_EnterMonitor(session); /* ==== session lock === */
    /* Initialize the find with the template */
    ckrv = CKAPI(epv)->C_FindObjectsInit(session->handle,
                                         obj_template, otsize);
    if (ckrv != CKR_OK) {
        nssSession_ExitMonitor(session);
        goto loser;
    }
    while (PR_TRUE) {
        /* Issue the find for up to arraySize - numHandles objects */
        ckrv = CKAPI(epv)->C_FindObjects(session->handle,
                                         objectHandles + numHandles,
                                         arraySize - numHandles,
                                         &count);
        if (ckrv != CKR_OK) {
            nssSession_ExitMonitor(session);
            goto loser;
        }
        /* bump the number of found objects */
        numHandles += count;
        if (maximumOpt > 0 || numHandles < arraySize) {
            /* When a maximum is provided, the search is done all at once,
             * so the search is finished.  If the number returned was less
             * than the number sought, the search is finished.
             */
            break;
        }
        /* the array is filled, double it and continue */
        arraySize *= 2;
        if (objectHandles == staticObjects) {
            objectHandles = nss_ZNEWARRAY(NULL, CK_OBJECT_HANDLE, arraySize);
            if (objectHandles) {
                PORT_Memcpy(objectHandles, staticObjects,
                            OBJECT_STACK_SIZE * sizeof(objectHandles[1]));
            }
        } else {
            objectHandles = nss_ZREALLOCARRAY(objectHandles,
                                              CK_OBJECT_HANDLE,
                                              arraySize);
        }
        if (!objectHandles) {
            nssSession_ExitMonitor(session);
            ckrv = CKR_HOST_MEMORY;
            goto loser;
        }
    }
    ckrv = CKAPI(epv)->C_FindObjectsFinal(session->handle);
    nssSession_ExitMonitor(session); /* ==== end session lock === */
    if (ckrv != CKR_OK) {
        goto loser;
    }
    if (numHandles > 0) {
        objects = create_objects_from_handles(tok, session,
                                              objectHandles, numHandles);
    } else {
        nss_SetError(NSS_ERROR_NOT_FOUND);
        objects = NULL;
    }
    if (objectHandles && objectHandles != staticObjects) {
        nss_ZFreeIf(objectHandles);
    }
    if (statusOpt)
        *statusOpt = PR_SUCCESS;
    return objects;
loser:
    if (objectHandles && objectHandles != staticObjects) {
        nss_ZFreeIf(objectHandles);
    }
    /*
     * These errors should be treated the same as if the objects just weren't
     * found..
     */
    if ((ckrv == CKR_ATTRIBUTE_TYPE_INVALID) ||
        (ckrv == CKR_ATTRIBUTE_VALUE_INVALID) ||
        (ckrv == CKR_DATA_INVALID) ||
        (ckrv == CKR_DATA_LEN_RANGE) ||
        (ckrv == CKR_FUNCTION_NOT_SUPPORTED) ||
        (ckrv == CKR_TEMPLATE_INCOMPLETE) ||
        (ckrv == CKR_TEMPLATE_INCONSISTENT)) {

        nss_SetError(NSS_ERROR_NOT_FOUND);
        if (statusOpt)
            *statusOpt = PR_SUCCESS;
    } else {
        nss_SetError(ckrv);
        nss_SetError(NSS_ERROR_PKCS11);
        if (statusOpt)
            *statusOpt = PR_FAILURE;
    }
    return (nssCryptokiObject **)NULL;
}

NSS_IMPLEMENT nssCryptokiObject **
nssToken_FindObjectsByTemplate(
    NSSToken *token,
    nssSession *sessionOpt,
    CK_ATTRIBUTE_PTR obj_template,
    CK_ULONG otsize,
    PRUint32 maximumOpt,
    PRStatus *statusOpt)
{
    CK_OBJECT_CLASS objclass = (CK_OBJECT_CLASS)-1;
    nssCryptokiObject **objects = NULL;
    PRUint32 i;

    if (!token) {
        PORT_SetError(SEC_ERROR_NO_TOKEN);
        if (statusOpt)
            *statusOpt = PR_FAILURE;
        return NULL;
    }
    for (i = 0; i < otsize; i++) {
        if (obj_template[i].type == CKA_CLASS) {
            objclass = *(CK_OBJECT_CLASS *)obj_template[i].pValue;
            break;
        }
    }
    PR_ASSERT(i < otsize);
    if (i == otsize) {
        PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
        if (statusOpt)
            *statusOpt = PR_FAILURE;
        return NULL;
    }
    /* If these objects are being cached, try looking there first */
    if (token->cache &&
        nssTokenObjectCache_HaveObjectClass(token->cache, objclass)) {
        PRStatus status;
        objects = nssTokenObjectCache_FindObjectsByTemplate(token->cache,
                                                            objclass,
                                                            obj_template,
                                                            otsize,
                                                            maximumOpt,
                                                            &status);
        if (status == PR_SUCCESS) {
            if (statusOpt)
                *statusOpt = status;
            return objects;
        }
    }
    /* Either they are not cached, or cache failed; look on token. */
    objects = find_objects(token, sessionOpt,
                           obj_template, otsize,
                           maximumOpt, statusOpt);
    return objects;
}

extern const NSSError NSS_ERROR_INVALID_CERTIFICATE;

NSS_IMPLEMENT nssCryptokiObject *
nssToken_ImportCertificate(
    NSSToken *tok,
    nssSession *sessionOpt,
    NSSCertificateType certType,
    NSSItem *id,
    const NSSUTF8 *nickname,
    NSSDER *encoding,
    NSSDER *issuer,
    NSSDER *subject,
    NSSDER *serial,
    NSSASCII7 *email,
    PRBool asTokenObject)
{
    PRStatus status;
    CK_CERTIFICATE_TYPE cert_type;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE cert_tmpl[10];
    CK_ULONG ctsize;
    nssTokenSearchType searchType;
    nssCryptokiObject *rvObject = NULL;

    if (!tok) {
        PORT_SetError(SEC_ERROR_NO_TOKEN);
        return NULL;
    }
    if (certType == NSSCertificateType_PKIX) {
        cert_type = CKC_X_509;
    } else {
        return (nssCryptokiObject *)NULL;
    }
    NSS_CK_TEMPLATE_START(cert_tmpl, attr, ctsize);
    if (asTokenObject) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
        searchType = nssTokenSearchType_TokenOnly;
    } else {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
        searchType = nssTokenSearchType_SessionOnly;
    }
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_CERTIFICATE_TYPE, cert_type);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ID, id);
    NSS_CK_SET_ATTRIBUTE_UTF8(attr, CKA_LABEL, nickname);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_VALUE, encoding);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ISSUER, issuer);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SUBJECT, subject);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SERIAL_NUMBER, serial);
    if (email) {
        NSS_CK_SET_ATTRIBUTE_UTF8(attr, CKA_NSS_EMAIL, email);
    }
    NSS_CK_TEMPLATE_FINISH(cert_tmpl, attr, ctsize);
    /* see if the cert is already there */
    rvObject = nssToken_FindCertificateByIssuerAndSerialNumber(tok,
                                                               sessionOpt,
                                                               issuer,
                                                               serial,
                                                               searchType,
                                                               NULL);
    if (rvObject) {
        NSSItem existingDER;
        NSSSlot *slot = nssToken_GetSlot(tok);
        nssSession *session = nssSlot_CreateSession(slot, NULL, PR_TRUE);
        if (!session) {
            nssCryptokiObject_Destroy(rvObject);
            nssSlot_Destroy(slot);
            return (nssCryptokiObject *)NULL;
        }
        /* Reject any attempt to import a new cert that has the same
         * issuer/serial as an existing cert, but does not have the
         * same encoding
         */
        NSS_CK_TEMPLATE_START(cert_tmpl, attr, ctsize);
        NSS_CK_SET_ATTRIBUTE_NULL(attr, CKA_VALUE);
        NSS_CK_TEMPLATE_FINISH(cert_tmpl, attr, ctsize);
        status = nssCKObject_GetAttributes(rvObject->handle,
                                           cert_tmpl, ctsize, NULL,
                                           session, slot);
        NSS_CK_ATTRIBUTE_TO_ITEM(cert_tmpl, &existingDER);
        if (status == PR_SUCCESS) {
            if (!nssItem_Equal(encoding, &existingDER, NULL)) {
                nss_SetError(NSS_ERROR_INVALID_CERTIFICATE);
                status = PR_FAILURE;
            }
            nss_ZFreeIf(existingDER.data);
        }
        if (status == PR_FAILURE) {
            nssCryptokiObject_Destroy(rvObject);
            nssSession_Destroy(session);
            nssSlot_Destroy(slot);
            return (nssCryptokiObject *)NULL;
        }
        /* according to PKCS#11, label, ID, issuer, and serial number
         * may change after the object has been created.  For PKIX, the
         * last two attributes can't change, so for now we'll only worry
         * about the first two.
         */
        NSS_CK_TEMPLATE_START(cert_tmpl, attr, ctsize);
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ID, id);
        if (!rvObject->label && nickname) {
            NSS_CK_SET_ATTRIBUTE_UTF8(attr, CKA_LABEL, nickname);
        }
        NSS_CK_TEMPLATE_FINISH(cert_tmpl, attr, ctsize);
        /* reset the mutable attributes on the token */
        nssCKObject_SetAttributes(rvObject->handle,
                                  cert_tmpl, ctsize,
                                  session, slot);
        if (!rvObject->label && nickname) {
            rvObject->label = nssUTF8_Duplicate(nickname, NULL);
        }
        nssSession_Destroy(session);
        nssSlot_Destroy(slot);
    } else {
        /* Import the certificate onto the token */
        rvObject = import_object(tok, sessionOpt, cert_tmpl, ctsize);
    }
    if (rvObject && tok->cache) {
        /* The cache will overwrite the attributes if the object already
         * exists.
         */
        nssTokenObjectCache_ImportObject(tok->cache, rvObject,
                                         CKO_CERTIFICATE,
                                         cert_tmpl, ctsize);
    }
    return rvObject;
}

/* traverse all objects of the given class - this should only happen
 * if the token has been marked as "traversable"
 */
NSS_IMPLEMENT nssCryptokiObject **
nssToken_FindObjects(
    NSSToken *token,
    nssSession *sessionOpt,
    CK_OBJECT_CLASS objclass,
    nssTokenSearchType searchType,
    PRUint32 maximumOpt,
    PRStatus *statusOpt)
{
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE obj_template[2];
    CK_ULONG obj_size;
    nssCryptokiObject **objects;
    NSS_CK_TEMPLATE_START(obj_template, attr, obj_size);
    /* Set the search to token/session only if provided */
    if (searchType == nssTokenSearchType_SessionOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (searchType == nssTokenSearchType_TokenOnly ||
               searchType == nssTokenSearchType_TokenForced) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_CLASS, objclass);
    NSS_CK_TEMPLATE_FINISH(obj_template, attr, obj_size);

    if (searchType == nssTokenSearchType_TokenForced) {
        objects = find_objects(token, sessionOpt,
                               obj_template, obj_size,
                               maximumOpt, statusOpt);
    } else {
        objects = nssToken_FindObjectsByTemplate(token, sessionOpt,
                                                 obj_template, obj_size,
                                                 maximumOpt, statusOpt);
    }
    return objects;
}

NSS_IMPLEMENT nssCryptokiObject **
nssToken_FindCertificatesBySubject(
    NSSToken *token,
    nssSession *sessionOpt,
    NSSDER *subject,
    nssTokenSearchType searchType,
    PRUint32 maximumOpt,
    PRStatus *statusOpt)
{
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE subj_template[3];
    CK_ULONG stsize;
    nssCryptokiObject **objects;
    NSS_CK_TEMPLATE_START(subj_template, attr, stsize);
    /* Set the search to token/session only if provided */
    if (searchType == nssTokenSearchType_SessionOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (searchType == nssTokenSearchType_TokenOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SUBJECT, subject);
    NSS_CK_TEMPLATE_FINISH(subj_template, attr, stsize);
    /* now locate the token certs matching this template */
    objects = nssToken_FindObjectsByTemplate(token, sessionOpt,
                                             subj_template, stsize,
                                             maximumOpt, statusOpt);
    return objects;
}

NSS_IMPLEMENT nssCryptokiObject **
nssToken_FindCertificatesByNickname(
    NSSToken *token,
    nssSession *sessionOpt,
    const NSSUTF8 *name,
    nssTokenSearchType searchType,
    PRUint32 maximumOpt,
    PRStatus *statusOpt)
{
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE nick_template[3];
    CK_ULONG ntsize;
    nssCryptokiObject **objects;
    NSS_CK_TEMPLATE_START(nick_template, attr, ntsize);
    NSS_CK_SET_ATTRIBUTE_UTF8(attr, CKA_LABEL, name);
    /* Set the search to token/session only if provided */
    if (searchType == nssTokenSearchType_SessionOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (searchType == nssTokenSearchType_TokenOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_cert);
    NSS_CK_TEMPLATE_FINISH(nick_template, attr, ntsize);
    /* now locate the token certs matching this template */
    objects = nssToken_FindObjectsByTemplate(token, sessionOpt,
                                             nick_template, ntsize,
                                             maximumOpt, statusOpt);
    if (!objects) {
        /* This is to workaround the fact that PKCS#11 doesn't specify
         * whether the '\0' should be included.  XXX Is that still true?
         * im - this is not needed by the current softoken.  However, I'm
         * leaving it in until I have surveyed more tokens to see if it needed.
         * well, its needed by the builtin token...
         */
        nick_template[0].ulValueLen++;
        objects = nssToken_FindObjectsByTemplate(token, sessionOpt,
                                                 nick_template, ntsize,
                                                 maximumOpt, statusOpt);
    }
    return objects;
}

/* XXX
 * This function *does not* use the token object cache, because not even
 * the softoken will return a value for CKA_NSS_EMAIL from a call
 * to GetAttributes.  The softoken does allow searches with that attribute,
 * it just won't return a value for it.
 */
NSS_IMPLEMENT nssCryptokiObject **
nssToken_FindCertificatesByEmail(
    NSSToken *token,
    nssSession *sessionOpt,
    NSSASCII7 *email,
    nssTokenSearchType searchType,
    PRUint32 maximumOpt,
    PRStatus *statusOpt)
{
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE email_template[3];
    CK_ULONG etsize;
    nssCryptokiObject **objects;
    NSS_CK_TEMPLATE_START(email_template, attr, etsize);
    NSS_CK_SET_ATTRIBUTE_UTF8(attr, CKA_NSS_EMAIL, email);
    /* Set the search to token/session only if provided */
    if (searchType == nssTokenSearchType_SessionOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (searchType == nssTokenSearchType_TokenOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_cert);
    NSS_CK_TEMPLATE_FINISH(email_template, attr, etsize);
    /* now locate the token certs matching this template */
    objects = find_objects(token, sessionOpt,
                           email_template, etsize,
                           maximumOpt, statusOpt);
    if (!objects) {
        /* This is to workaround the fact that PKCS#11 doesn't specify
         * whether the '\0' should be included.  XXX Is that still true?
         * im - this is not needed by the current softoken.  However, I'm
         * leaving it in until I have surveyed more tokens to see if it needed.
         * well, its needed by the builtin token...
         */
        email_template[0].ulValueLen++;
        objects = find_objects(token, sessionOpt,
                               email_template, etsize,
                               maximumOpt, statusOpt);
    }
    return objects;
}

NSS_IMPLEMENT nssCryptokiObject **
nssToken_FindCertificatesByID(
    NSSToken *token,
    nssSession *sessionOpt,
    NSSItem *id,
    nssTokenSearchType searchType,
    PRUint32 maximumOpt,
    PRStatus *statusOpt)
{
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE id_template[3];
    CK_ULONG idtsize;
    nssCryptokiObject **objects;
    NSS_CK_TEMPLATE_START(id_template, attr, idtsize);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ID, id);
    /* Set the search to token/session only if provided */
    if (searchType == nssTokenSearchType_SessionOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (searchType == nssTokenSearchType_TokenOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_cert);
    NSS_CK_TEMPLATE_FINISH(id_template, attr, idtsize);
    /* now locate the token certs matching this template */
    objects = nssToken_FindObjectsByTemplate(token, sessionOpt,
                                             id_template, idtsize,
                                             maximumOpt, statusOpt);
    return objects;
}

/*
 * decode the serial item and return our result.
 * NOTE serialDecode's data is really stored in serial. Don't free it.
 */
static PRStatus
nssToken_decodeSerialItem(NSSItem *serial, NSSItem *serialDecode)
{
    unsigned char *data = (unsigned char *)serial->data;
    int data_left, data_len, index;

    if ((serial->size >= 3) && (data[0] == 0x2)) {
        /* remove the der encoding of the serial number before generating the
         * key.. */
        data_left = serial->size - 2;
        data_len = data[1];
        index = 2;

        /* extended length ? (not very likely for a serial number) */
        if (data_len & 0x80) {
            int len_count = data_len & 0x7f;

            data_len = 0;
            data_left -= len_count;
            if (data_left > 0) {
                while (len_count--) {
                    data_len = (data_len << 8) | data[index++];
                }
            }
        }
        /* XXX leaving any leading zeros on the serial number for backwards
         * compatibility
         */
        /* not a valid der, must be just an unlucky serial number value */
        if (data_len == data_left) {
            serialDecode->size = data_len;
            serialDecode->data = &data[index];
            return PR_SUCCESS;
        }
    }
    return PR_FAILURE;
}

NSS_IMPLEMENT nssCryptokiObject *
nssToken_FindCertificateByIssuerAndSerialNumber(
    NSSToken *token,
    nssSession *sessionOpt,
    NSSDER *issuer,
    NSSDER *serial,
    nssTokenSearchType searchType,
    PRStatus *statusOpt)
{
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE_PTR serialAttr;
    CK_ATTRIBUTE cert_template[4];
    CK_ULONG ctsize;
    nssCryptokiObject **objects;
    nssCryptokiObject *rvObject = NULL;
    NSS_CK_TEMPLATE_START(cert_template, attr, ctsize);

    if (!token) {
        PORT_SetError(SEC_ERROR_NO_TOKEN);
        if (statusOpt)
            *statusOpt = PR_FAILURE;
        return NULL;
    }
    /* Set the search to token/session only if provided */
    if (searchType == nssTokenSearchType_SessionOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if ((searchType == nssTokenSearchType_TokenOnly) ||
               (searchType == nssTokenSearchType_TokenForced)) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    /* Set the unique id */
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ISSUER, issuer);
    serialAttr = attr;
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SERIAL_NUMBER, serial);
    NSS_CK_TEMPLATE_FINISH(cert_template, attr, ctsize);
    /* get the object handle */
    if (searchType == nssTokenSearchType_TokenForced) {
        objects = find_objects(token, sessionOpt,
                               cert_template, ctsize,
                               1, statusOpt);
    } else {
        objects = nssToken_FindObjectsByTemplate(token, sessionOpt,
                                                 cert_template, ctsize,
                                                 1, statusOpt);
    }
    if (objects) {
        rvObject = objects[0];
        nss_ZFreeIf(objects);
    }

    /*
     * NSS used to incorrectly store serial numbers in their decoded form.
     * because of this old tokens have decoded serial numbers.
     */
    if (!objects) {
        NSSItem serialDecode;
        PRStatus status;

        status = nssToken_decodeSerialItem(serial, &serialDecode);
        if (status != PR_SUCCESS) {
            return NULL;
        }
        NSS_CK_SET_ATTRIBUTE_ITEM(serialAttr, CKA_SERIAL_NUMBER, &serialDecode);
        if (searchType == nssTokenSearchType_TokenForced) {
            objects = find_objects(token, sessionOpt,
                                   cert_template, ctsize,
                                   1, statusOpt);
        } else {
            objects = nssToken_FindObjectsByTemplate(token, sessionOpt,
                                                     cert_template, ctsize,
                                                     1, statusOpt);
        }
        if (objects) {
            rvObject = objects[0];
            nss_ZFreeIf(objects);
        }
    }
    return rvObject;
}

NSS_IMPLEMENT nssCryptokiObject *
nssToken_FindCertificateByEncodedCertificate(
    NSSToken *token,
    nssSession *sessionOpt,
    NSSBER *encodedCertificate,
    nssTokenSearchType searchType,
    PRStatus *statusOpt)
{
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE cert_template[3];
    CK_ULONG ctsize;
    nssCryptokiObject **objects;
    nssCryptokiObject *rvObject = NULL;
    NSS_CK_TEMPLATE_START(cert_template, attr, ctsize);
    /* Set the search to token/session only if provided */
    if (searchType == nssTokenSearchType_SessionOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (searchType == nssTokenSearchType_TokenOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_cert);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_VALUE, encodedCertificate);
    NSS_CK_TEMPLATE_FINISH(cert_template, attr, ctsize);
    /* get the object handle */
    objects = nssToken_FindObjectsByTemplate(token, sessionOpt,
                                             cert_template, ctsize,
                                             1, statusOpt);
    if (objects) {
        rvObject = objects[0];
        nss_ZFreeIf(objects);
    }
    return rvObject;
}

NSS_IMPLEMENT nssCryptokiObject **
nssToken_FindPrivateKeys(
    NSSToken *token,
    nssSession *sessionOpt,
    nssTokenSearchType searchType,
    PRUint32 maximumOpt,
    PRStatus *statusOpt)
{
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE key_template[2];
    CK_ULONG ktsize;
    nssCryptokiObject **objects;

    NSS_CK_TEMPLATE_START(key_template, attr, ktsize);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_privkey);
    if (searchType == nssTokenSearchType_SessionOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (searchType == nssTokenSearchType_TokenOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_TEMPLATE_FINISH(key_template, attr, ktsize);

    objects = nssToken_FindObjectsByTemplate(token, sessionOpt,
                                             key_template, ktsize,
                                             maximumOpt, statusOpt);
    return objects;
}

/* XXX ?there are no session cert objects, so only search token objects */
NSS_IMPLEMENT nssCryptokiObject *
nssToken_FindPrivateKeyByID(
    NSSToken *token,
    nssSession *sessionOpt,
    NSSItem *keyID)
{
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE key_template[3];
    CK_ULONG ktsize;
    nssCryptokiObject **objects;
    nssCryptokiObject *rvKey = NULL;

    NSS_CK_TEMPLATE_START(key_template, attr, ktsize);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_privkey);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ID, keyID);
    NSS_CK_TEMPLATE_FINISH(key_template, attr, ktsize);

    objects = nssToken_FindObjectsByTemplate(token, sessionOpt,
                                             key_template, ktsize,
                                             1, NULL);
    if (objects) {
        rvKey = objects[0];
        nss_ZFreeIf(objects);
    }
    return rvKey;
}

/* XXX ?there are no session cert objects, so only search token objects */
NSS_IMPLEMENT nssCryptokiObject *
nssToken_FindPublicKeyByID(
    NSSToken *token,
    nssSession *sessionOpt,
    NSSItem *keyID)
{
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE key_template[3];
    CK_ULONG ktsize;
    nssCryptokiObject **objects;
    nssCryptokiObject *rvKey = NULL;

    NSS_CK_TEMPLATE_START(key_template, attr, ktsize);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_pubkey);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ID, keyID);
    NSS_CK_TEMPLATE_FINISH(key_template, attr, ktsize);

    objects = nssToken_FindObjectsByTemplate(token, sessionOpt,
                                             key_template, ktsize,
                                             1, NULL);
    if (objects) {
        rvKey = objects[0];
        nss_ZFreeIf(objects);
    }
    return rvKey;
}

static void
sha1_hash(NSSItem *input, NSSItem *output)
{
    NSSAlgorithmAndParameters *ap;
    PK11SlotInfo *internal = PK11_GetInternalSlot();
    NSSToken *token = PK11Slot_GetNSSToken(internal);
    ap = NSSAlgorithmAndParameters_CreateSHA1Digest(NULL);
    (void)nssToken_Digest(token, NULL, ap, input, output, NULL);
    nss_ZFreeIf(ap);
    (void)nssToken_Destroy(token);
    PK11_FreeSlot(internal);
}

static void
md5_hash(NSSItem *input, NSSItem *output)
{
    NSSAlgorithmAndParameters *ap;
    PK11SlotInfo *internal = PK11_GetInternalSlot();
    NSSToken *token = PK11Slot_GetNSSToken(internal);
    ap = NSSAlgorithmAndParameters_CreateMD5Digest(NULL);
    (void)nssToken_Digest(token, NULL, ap, input, output, NULL);
    nss_ZFreeIf(ap);
    (void)nssToken_Destroy(token);
    PK11_FreeSlot(internal);
}

static CK_TRUST
get_ck_trust(
    nssTrustLevel nssTrust)
{
    CK_TRUST t;
    switch (nssTrust) {
        case nssTrustLevel_NotTrusted:
            t = CKT_NSS_NOT_TRUSTED;
            break;
        case nssTrustLevel_TrustedDelegator:
            t = CKT_NSS_TRUSTED_DELEGATOR;
            break;
        case nssTrustLevel_ValidDelegator:
            t = CKT_NSS_VALID_DELEGATOR;
            break;
        case nssTrustLevel_Trusted:
            t = CKT_NSS_TRUSTED;
            break;
        case nssTrustLevel_MustVerify:
            t = CKT_NSS_MUST_VERIFY_TRUST;
            break;
        case nssTrustLevel_Unknown:
        default:
            t = CKT_NSS_TRUST_UNKNOWN;
            break;
    }
    return t;
}

NSS_IMPLEMENT nssCryptokiObject *
nssToken_ImportTrust(
    NSSToken *tok,
    nssSession *sessionOpt,
    NSSDER *certEncoding,
    NSSDER *certIssuer,
    NSSDER *certSerial,
    nssTrustLevel serverAuth,
    nssTrustLevel clientAuth,
    nssTrustLevel codeSigning,
    nssTrustLevel emailProtection,
    PRBool stepUpApproved,
    PRBool asTokenObject)
{
    nssCryptokiObject *object;
    CK_OBJECT_CLASS tobjc = CKO_NSS_TRUST;
    CK_TRUST ckSA, ckCA, ckCS, ckEP;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE trust_tmpl[11];
    CK_ULONG tsize;
    PRUint8 sha1[20]; /* this is cheating... */
    PRUint8 md5[16];
    NSSItem sha1_result, md5_result;
    sha1_result.data = sha1;
    sha1_result.size = sizeof sha1;
    md5_result.data = md5;
    md5_result.size = sizeof md5;
    sha1_hash(certEncoding, &sha1_result);
    md5_hash(certEncoding, &md5_result);
    ckSA = get_ck_trust(serverAuth);
    ckCA = get_ck_trust(clientAuth);
    ckCS = get_ck_trust(codeSigning);
    ckEP = get_ck_trust(emailProtection);
    NSS_CK_TEMPLATE_START(trust_tmpl, attr, tsize);
    if (asTokenObject) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    } else {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    }
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_CLASS, tobjc);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ISSUER, certIssuer);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SERIAL_NUMBER, certSerial);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CERT_SHA1_HASH, &sha1_result);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CERT_MD5_HASH, &md5_result);
    /* now set the trust values */
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_SERVER_AUTH, ckSA);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_CLIENT_AUTH, ckCA);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_CODE_SIGNING, ckCS);
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_TRUST_EMAIL_PROTECTION, ckEP);
    if (stepUpApproved) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TRUST_STEP_UP_APPROVED,
                                  &g_ck_true);
    } else {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TRUST_STEP_UP_APPROVED,
                                  &g_ck_false);
    }
    NSS_CK_TEMPLATE_FINISH(trust_tmpl, attr, tsize);
    /* import the trust object onto the token */
    object = import_object(tok, sessionOpt, trust_tmpl, tsize);
    if (object && tok->cache) {
        nssTokenObjectCache_ImportObject(tok->cache, object, tobjc,
                                         trust_tmpl, tsize);
    }
    return object;
}

NSS_IMPLEMENT nssCryptokiObject *
nssToken_FindTrustForCertificate(
    NSSToken *token,
    nssSession *sessionOpt,
    NSSDER *certEncoding,
    NSSDER *certIssuer,
    NSSDER *certSerial,
    nssTokenSearchType searchType)
{
    CK_OBJECT_CLASS tobjc = CKO_NSS_TRUST;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE tobj_template[5];
    CK_ULONG tobj_size;
    nssSession *session = sessionOpt ? sessionOpt : token->defaultSession;
    nssCryptokiObject *object = NULL, **objects;

    /* Don't ask the module to use an invalid session handle. */
    if (!session || session->handle == CK_INVALID_HANDLE) {
        PORT_SetError(SEC_ERROR_NO_TOKEN);
        return object;
    }

    NSS_CK_TEMPLATE_START(tobj_template, attr, tobj_size);
    if (searchType == nssTokenSearchType_TokenOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_CLASS, tobjc);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_ISSUER, certIssuer);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SERIAL_NUMBER, certSerial);
    NSS_CK_TEMPLATE_FINISH(tobj_template, attr, tobj_size);
    objects = nssToken_FindObjectsByTemplate(token, session,
                                             tobj_template, tobj_size,
                                             1, NULL);
    if (objects) {
        object = objects[0];
        nss_ZFreeIf(objects);
    }
    return object;
}

NSS_IMPLEMENT nssCryptokiObject *
nssToken_ImportCRL(
    NSSToken *token,
    nssSession *sessionOpt,
    NSSDER *subject,
    NSSDER *encoding,
    PRBool isKRL,
    NSSUTF8 *url,
    PRBool asTokenObject)
{
    nssCryptokiObject *object;
    CK_OBJECT_CLASS crlobjc = CKO_NSS_CRL;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE crl_tmpl[6];
    CK_ULONG crlsize;

    NSS_CK_TEMPLATE_START(crl_tmpl, attr, crlsize);
    if (asTokenObject) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    } else {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    }
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_CLASS, crlobjc);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SUBJECT, subject);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_VALUE, encoding);
    NSS_CK_SET_ATTRIBUTE_UTF8(attr, CKA_NSS_URL, url);
    if (isKRL) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_NSS_KRL, &g_ck_true);
    } else {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_NSS_KRL, &g_ck_false);
    }
    NSS_CK_TEMPLATE_FINISH(crl_tmpl, attr, crlsize);

    /* import the crl object onto the token */
    object = import_object(token, sessionOpt, crl_tmpl, crlsize);
    if (object && token->cache) {
        nssTokenObjectCache_ImportObject(token->cache, object, crlobjc,
                                         crl_tmpl, crlsize);
    }
    return object;
}

NSS_IMPLEMENT nssCryptokiObject **
nssToken_FindCRLsBySubject(
    NSSToken *token,
    nssSession *sessionOpt,
    NSSDER *subject,
    nssTokenSearchType searchType,
    PRUint32 maximumOpt,
    PRStatus *statusOpt)
{
    CK_OBJECT_CLASS crlobjc = CKO_NSS_CRL;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE crlobj_template[3];
    CK_ULONG crlobj_size;
    nssCryptokiObject **objects = NULL;
    nssSession *session = sessionOpt ? sessionOpt : token->defaultSession;

    /* Don't ask the module to use an invalid session handle. */
    if (!session || session->handle == CK_INVALID_HANDLE) {
        PORT_SetError(SEC_ERROR_NO_TOKEN);
        return objects;
    }

    NSS_CK_TEMPLATE_START(crlobj_template, attr, crlobj_size);
    if (searchType == nssTokenSearchType_SessionOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (searchType == nssTokenSearchType_TokenOnly ||
               searchType == nssTokenSearchType_TokenForced) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_VAR(attr, CKA_CLASS, crlobjc);
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_SUBJECT, subject);
    NSS_CK_TEMPLATE_FINISH(crlobj_template, attr, crlobj_size);

    objects = nssToken_FindObjectsByTemplate(token, session,
                                             crlobj_template, crlobj_size,
                                             maximumOpt, statusOpt);
    return objects;
}

NSS_IMPLEMENT PRStatus
nssToken_GetCachedObjectAttributes(
    NSSToken *token,
    NSSArena *arenaOpt,
    nssCryptokiObject *object,
    CK_OBJECT_CLASS objclass,
    CK_ATTRIBUTE_PTR atemplate,
    CK_ULONG atlen)
{
    if (!token->cache) {
        return PR_FAILURE;
    }
    return nssTokenObjectCache_GetObjectAttributes(token->cache, arenaOpt,
                                                   object, objclass,
                                                   atemplate, atlen);
}

NSS_IMPLEMENT NSSItem *
nssToken_Digest(
    NSSToken *tok,
    nssSession *sessionOpt,
    NSSAlgorithmAndParameters *ap,
    NSSItem *data,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    CK_RV ckrv;
    CK_ULONG digestLen;
    CK_BYTE_PTR digest;
    NSSItem *rvItem = NULL;
    void *epv = nssToken_GetCryptokiEPV(tok);
    nssSession *session = (sessionOpt) ? sessionOpt : tok->defaultSession;

    /* Don't ask the module to use an invalid session handle. */
    if (!session || session->handle == CK_INVALID_HANDLE) {
        PORT_SetError(SEC_ERROR_NO_TOKEN);
        return rvItem;
    }

    nssSession_EnterMonitor(session);
    ckrv = CKAPI(epv)->C_DigestInit(session->handle, &ap->mechanism);
    if (ckrv != CKR_OK) {
        nssSession_ExitMonitor(session);
        return NULL;
    }
#if 0
    /* XXX the standard says this should work, but it doesn't */
    ckrv = CKAPI(epv)->C_Digest(session->handle, NULL, 0, NULL, &digestLen);
    if (ckrv != CKR_OK) {
	nssSession_ExitMonitor(session);
	return NULL;
    }
#endif
    digestLen = 0; /* XXX for now */
    digest = NULL;
    if (rvOpt) {
        if (rvOpt->size > 0 && rvOpt->size < digestLen) {
            nssSession_ExitMonitor(session);
            /* the error should be bad args */
            return NULL;
        }
        if (rvOpt->data) {
            digest = rvOpt->data;
        }
        digestLen = rvOpt->size;
    }
    if (!digest) {
        digest = (CK_BYTE_PTR)nss_ZAlloc(arenaOpt, digestLen);
        if (!digest) {
            nssSession_ExitMonitor(session);
            return NULL;
        }
    }
    ckrv = CKAPI(epv)->C_Digest(session->handle,
                                (CK_BYTE_PTR)data->data,
                                (CK_ULONG)data->size,
                                (CK_BYTE_PTR)digest,
                                &digestLen);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
        nss_ZFreeIf(digest);
        return NULL;
    }
    if (!rvOpt) {
        rvItem = nssItem_Create(arenaOpt, NULL, digestLen, (void *)digest);
    }
    return rvItem;
}

NSS_IMPLEMENT PRStatus
nssToken_BeginDigest(
    NSSToken *tok,
    nssSession *sessionOpt,
    NSSAlgorithmAndParameters *ap)
{
    CK_RV ckrv;
    void *epv = nssToken_GetCryptokiEPV(tok);
    nssSession *session = (sessionOpt) ? sessionOpt : tok->defaultSession;

    /* Don't ask the module to use an invalid session handle. */
    if (!session || session->handle == CK_INVALID_HANDLE) {
        PORT_SetError(SEC_ERROR_NO_TOKEN);
        return PR_FAILURE;
    }

    nssSession_EnterMonitor(session);
    ckrv = CKAPI(epv)->C_DigestInit(session->handle, &ap->mechanism);
    nssSession_ExitMonitor(session);
    return (ckrv == CKR_OK) ? PR_SUCCESS : PR_FAILURE;
}

NSS_IMPLEMENT PRStatus
nssToken_ContinueDigest(
    NSSToken *tok,
    nssSession *sessionOpt,
    NSSItem *item)
{
    CK_RV ckrv;
    void *epv = nssToken_GetCryptokiEPV(tok);
    nssSession *session = (sessionOpt) ? sessionOpt : tok->defaultSession;

    /* Don't ask the module to use an invalid session handle. */
    if (!session || session->handle == CK_INVALID_HANDLE) {
        PORT_SetError(SEC_ERROR_NO_TOKEN);
        return PR_FAILURE;
    }

    nssSession_EnterMonitor(session);
    ckrv = CKAPI(epv)->C_DigestUpdate(session->handle,
                                      (CK_BYTE_PTR)item->data,
                                      (CK_ULONG)item->size);
    nssSession_ExitMonitor(session);
    return (ckrv == CKR_OK) ? PR_SUCCESS : PR_FAILURE;
}

NSS_IMPLEMENT NSSItem *
nssToken_FinishDigest(
    NSSToken *tok,
    nssSession *sessionOpt,
    NSSItem *rvOpt,
    NSSArena *arenaOpt)
{
    CK_RV ckrv;
    CK_ULONG digestLen;
    CK_BYTE_PTR digest;
    NSSItem *rvItem = NULL;
    void *epv = nssToken_GetCryptokiEPV(tok);
    nssSession *session = (sessionOpt) ? sessionOpt : tok->defaultSession;

    /* Don't ask the module to use an invalid session handle. */
    if (!session || session->handle == CK_INVALID_HANDLE) {
        PORT_SetError(SEC_ERROR_NO_TOKEN);
        return NULL;
    }

    nssSession_EnterMonitor(session);
    ckrv = CKAPI(epv)->C_DigestFinal(session->handle, NULL, &digestLen);
    if (ckrv != CKR_OK || digestLen == 0) {
        nssSession_ExitMonitor(session);
        return NULL;
    }
    digest = NULL;
    if (rvOpt) {
        if (rvOpt->size > 0 && rvOpt->size < digestLen) {
            nssSession_ExitMonitor(session);
            /* the error should be bad args */
            return NULL;
        }
        if (rvOpt->data) {
            digest = rvOpt->data;
        }
        digestLen = rvOpt->size;
    }
    if (!digest) {
        digest = (CK_BYTE_PTR)nss_ZAlloc(arenaOpt, digestLen);
        if (!digest) {
            nssSession_ExitMonitor(session);
            return NULL;
        }
    }
    ckrv = CKAPI(epv)->C_DigestFinal(session->handle, digest, &digestLen);
    nssSession_ExitMonitor(session);
    if (ckrv != CKR_OK) {
        nss_ZFreeIf(digest);
        return NULL;
    }
    if (!rvOpt) {
        rvItem = nssItem_Create(arenaOpt, NULL, digestLen, (void *)digest);
    }
    return rvItem;
}

NSS_IMPLEMENT PRBool
nssToken_IsPresent(
    NSSToken *token)
{
    return nssSlot_IsTokenPresent(token->slot);
}

/* Sigh.  The methods to find objects declared above cause problems with
 * the low-level object cache in the softoken -- the objects are found in
 * toto, then one wave of GetAttributes is done, then another.  Having a
 * large number of objects causes the cache to be thrashed, as the objects
 * are gone before there's any chance to ask for their attributes.
 * So, for now, bringing back traversal methods for certs.  This way all of
 * the cert's attributes can be grabbed immediately after finding it,
 * increasing the likelihood that the cache takes care of it.
 */
NSS_IMPLEMENT PRStatus
nssToken_TraverseCertificates(
    NSSToken *token,
    nssSession *sessionOpt,
    nssTokenSearchType searchType,
    PRStatus (*callback)(nssCryptokiObject *instance, void *arg),
    void *arg)
{
    CK_RV ckrv;
    CK_ULONG count;
    CK_OBJECT_HANDLE *objectHandles;
    CK_ATTRIBUTE_PTR attr;
    CK_ATTRIBUTE cert_template[2];
    CK_ULONG ctsize;
    NSSArena *arena;
    PRUint32 arraySize, numHandles;
    nssCryptokiObject **objects;
    void *epv = nssToken_GetCryptokiEPV(token);
    nssSession *session = (sessionOpt) ? sessionOpt : token->defaultSession;

    /* Don't ask the module to use an invalid session handle. */
    if (!session || session->handle == CK_INVALID_HANDLE) {
        PORT_SetError(SEC_ERROR_NO_TOKEN);
        return PR_FAILURE;
    }

    /* template for all certs */
    NSS_CK_TEMPLATE_START(cert_template, attr, ctsize);
    if (searchType == nssTokenSearchType_SessionOnly) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_false);
    } else if (searchType == nssTokenSearchType_TokenOnly ||
               searchType == nssTokenSearchType_TokenForced) {
        NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_TOKEN, &g_ck_true);
    }
    NSS_CK_SET_ATTRIBUTE_ITEM(attr, CKA_CLASS, &g_ck_class_cert);
    NSS_CK_TEMPLATE_FINISH(cert_template, attr, ctsize);

    /* the arena is only for the array of object handles */
    arena = nssArena_Create();
    if (!arena) {
        return PR_FAILURE;
    }
    arraySize = OBJECT_STACK_SIZE;
    numHandles = 0;
    objectHandles = nss_ZNEWARRAY(arena, CK_OBJECT_HANDLE, arraySize);
    if (!objectHandles) {
        goto loser;
    }
    nssSession_EnterMonitor(session); /* ==== session lock === */
    /* Initialize the find with the template */
    ckrv = CKAPI(epv)->C_FindObjectsInit(session->handle,
                                         cert_template, ctsize);
    if (ckrv != CKR_OK) {
        nssSession_ExitMonitor(session);
        goto loser;
    }
    while (PR_TRUE) {
        /* Issue the find for up to arraySize - numHandles objects */
        ckrv = CKAPI(epv)->C_FindObjects(session->handle,
                                         objectHandles + numHandles,
                                         arraySize - numHandles,
                                         &count);
        if (ckrv != CKR_OK) {
            nssSession_ExitMonitor(session);
            goto loser;
        }
        /* bump the number of found objects */
        numHandles += count;
        if (numHandles < arraySize) {
            break;
        }
        /* the array is filled, double it and continue */
        arraySize *= 2;
        objectHandles = nss_ZREALLOCARRAY(objectHandles,
                                          CK_OBJECT_HANDLE,
                                          arraySize);
        if (!objectHandles) {
            nssSession_ExitMonitor(session);
            goto loser;
        }
    }
    ckrv = CKAPI(epv)->C_FindObjectsFinal(session->handle);
    nssSession_ExitMonitor(session); /* ==== end session lock === */
    if (ckrv != CKR_OK) {
        goto loser;
    }
    if (numHandles > 0) {
        objects = create_objects_from_handles(token, session,
                                              objectHandles, numHandles);
        if (objects) {
            nssCryptokiObject **op;
            for (op = objects; *op; op++) {
                (void)(*callback)(*op, arg);
            }
            nss_ZFreeIf(objects);
        }
    }
    nssArena_Destroy(arena);
    return PR_SUCCESS;
loser:
    nssArena_Destroy(arena);
    return PR_FAILURE;
}

NSS_IMPLEMENT PRBool
nssToken_IsPrivateKeyAvailable(
    NSSToken *token,
    NSSCertificate *c,
    nssCryptokiObject *instance)
{
    CK_OBJECT_CLASS theClass;

    if (token == NULL)
        return PR_FALSE;
    if (c == NULL)
        return PR_FALSE;

    theClass = CKO_PRIVATE_KEY;
    if (!nssSlot_IsLoggedIn(token->slot)) {
        theClass = CKO_PUBLIC_KEY;
    }
    if (PK11_MatchItem(token->pk11slot, instance->handle, theClass) !=
        CK_INVALID_HANDLE) {
        return PR_TRUE;
    }
    return PR_FALSE;
}
