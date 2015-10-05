/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_ldapresponse.c
 *
 */

#include <fcntl.h>
#include "pkix_pl_ldapresponse.h"

/* --Private-LdapResponse-Functions------------------------------------- */

/*
 * FUNCTION: pkix_pl_LdapResponse_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_LdapResponse_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_LdapResponse *ldapRsp = NULL;
        LDAPMessage *m = NULL;
        LDAPSearchResponseEntry *entry = NULL;
        LDAPSearchResponseResult *result = NULL;
        LDAPSearchResponseAttr **attributes = NULL;
        LDAPSearchResponseAttr *attr = NULL;
        SECItem **valp = NULL;
        SECItem *val = NULL;

        PKIX_ENTER(LDAPRESPONSE, "pkix_pl_LdapResponse_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_LDAPRESPONSE_TYPE, plContext),
                    PKIX_OBJECTNOTLDAPRESPONSE);

        ldapRsp = (PKIX_PL_LdapResponse *)object;

        m = &ldapRsp->decoded;

        if (m->messageID.data != NULL) {
                PR_Free(m->messageID.data);
        }

        if (m->protocolOp.selector ==
                LDAP_SEARCHRESPONSEENTRY_TYPE) {
                entry = &m->protocolOp.op.searchResponseEntryMsg;
                if (entry->objectName.data != NULL) {
                        PR_Free(entry->objectName.data);
                }
                if (entry->attributes != NULL) {
                        for (attributes = entry->attributes;
                                *attributes != NULL;
                                attributes++) {
                                attr = *attributes;
                                PR_Free(attr->attrType.data);
                                for (valp = attr->val; *valp != NULL; valp++) {
                                        val = *valp;
                                        if (val->data != NULL) {
                                                PR_Free(val->data);
                                        }
                                        PR_Free(val);
                                }
                                PR_Free(attr->val);
                                PR_Free(attr);
                        }
                        PR_Free(entry->attributes);
                }
        } else if (m->protocolOp.selector ==
                LDAP_SEARCHRESPONSERESULT_TYPE) {
                result = &m->protocolOp.op.searchResponseResultMsg;
                if (result->resultCode.data != NULL) {
                        PR_Free(result->resultCode.data);
                }
        }

        PKIX_FREE(ldapRsp->derEncoded.data);

cleanup:

        PKIX_RETURN(LDAPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_LdapResponse_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_LdapResponse_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_UInt32 dataLen = 0;
        PKIX_UInt32 dindex = 0;
        PKIX_UInt32 sizeOfLength = 0;
        PKIX_UInt32 idLen = 0;
        const unsigned char *msgBuf = NULL;
        PKIX_PL_LdapResponse *ldapRsp = NULL;

        PKIX_ENTER(LDAPRESPONSE, "pkix_pl_LdapResponse_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_LDAPRESPONSE_TYPE, plContext),
                    PKIX_OBJECTNOTLDAPRESPONSE);

        ldapRsp = (PKIX_PL_LdapResponse *)object;

        *pHashcode = 0;

        /*
         * Two responses that differ only in msgnum are a match! Therefore,
         * start hashcoding beyond the encoded messageID field.
         */
        if (ldapRsp->derEncoded.data) {
                msgBuf = (const unsigned char *)ldapRsp->derEncoded.data;
                /* Is message length short form (one octet) or long form? */
                if ((msgBuf[1] & 0x80) != 0) {
                        sizeOfLength = msgBuf[1] & 0x7F;
                        for (dindex = 0; dindex < sizeOfLength; dindex++) {
                                dataLen = (dataLen << 8) + msgBuf[dindex + 2];
                        }
                } else {
                        dataLen = msgBuf[1];
                }

                /* How many bytes for the messageID? (Assume short form) */
                idLen = msgBuf[dindex + 3] + 2;
                dindex += idLen;
                dataLen -= idLen;
                msgBuf = &msgBuf[dindex + 2];

                PKIX_CHECK(pkix_hash(msgBuf, dataLen, pHashcode, plContext),
                        PKIX_HASHFAILED);
        }

cleanup:

        PKIX_RETURN(LDAPRESPONSE);

}

/*
 * FUNCTION: pkix_pl_LdapResponse_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_LdapResponse_Equals(
        PKIX_PL_Object *firstObj,
        PKIX_PL_Object *secondObj,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_PL_LdapResponse *rsp1 = NULL;
        PKIX_PL_LdapResponse *rsp2 = NULL;
        PKIX_UInt32 secondType = 0;
        PKIX_UInt32 firstLen = 0;
        const unsigned char *firstData = NULL;
        const unsigned char *secondData = NULL;
        PKIX_UInt32 sizeOfLength = 0;
        PKIX_UInt32 dindex = 0;
        PKIX_UInt32 i = 0;

        PKIX_ENTER(LDAPRESPONSE, "pkix_pl_LdapResponse_Equals");
        PKIX_NULLCHECK_THREE(firstObj, secondObj, pResult);

        /* test that firstObj is a LdapResponse */
        PKIX_CHECK(pkix_CheckType(firstObj, PKIX_LDAPRESPONSE_TYPE, plContext),
                    PKIX_FIRSTOBJARGUMENTNOTLDAPRESPONSE);

        /*
         * Since we know firstObj is a LdapResponse, if both references are
         * identical, they must be equal
         */
        if (firstObj == secondObj){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondObj isn't a LdapResponse, we don't throw an error.
         * We simply return a Boolean result of FALSE
         */
        *pResult = PKIX_FALSE;
        PKIX_CHECK(PKIX_PL_Object_GetType(secondObj, &secondType, plContext),
                PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);
        if (secondType != PKIX_LDAPRESPONSE_TYPE) {
                goto cleanup;
        }

        rsp1 = (PKIX_PL_LdapResponse *)firstObj;
        rsp2 = (PKIX_PL_LdapResponse *)secondObj;

        /* If either lacks an encoded string, they cannot be compared */
        if (!(rsp1->derEncoded.data) || !(rsp2->derEncoded.data)) {
                goto cleanup;
        }

        if (rsp1->derEncoded.len != rsp2->derEncoded.len) {
                goto cleanup;
        }

        firstData = (const unsigned char *)rsp1->derEncoded.data;
        secondData = (const unsigned char *)rsp2->derEncoded.data;

        /*
         * Two responses that differ only in msgnum are equal! Therefore,
         * start the byte comparison beyond the encoded messageID field.
         */

        /* Is message length short form (one octet) or long form? */
        if ((firstData[1] & 0x80) != 0) {
                sizeOfLength = firstData[1] & 0x7F;
                for (dindex = 0; dindex < sizeOfLength; dindex++) {
                        firstLen = (firstLen << 8) + firstData[dindex + 2];
                }
        } else {
                firstLen = firstData[1];
        }

        /* How many bytes for the messageID? (Assume short form) */
        i = firstData[dindex + 3] + 2;
        dindex += i;
        firstLen -= i;
        firstData = &firstData[dindex + 2];

        /*
         * In theory, we have to calculate where the second message data
         * begins by checking its length encodings. But if these messages
         * are equal, we can re-use the calculation we already did. If they
         * are not equal, the byte comparisons will surely fail.
         */

        secondData = &secondData[dindex + 2];
        
        for (i = 0; i < firstLen; i++) {
                if (firstData[i] != secondData[i]) {
                        goto cleanup;
                }
        }

        *pResult = PKIX_TRUE;

cleanup:

        PKIX_RETURN(LDAPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_LdapResponse_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_LDAPRESPONSE_TYPE and its related functions with
 *  systemClasses[]
 * PARAMETERS:
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_LdapResponse_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(LDAPRESPONSE, "pkix_pl_LdapResponse_RegisterSelf");

        entry.description = "LdapResponse";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_LdapResponse);
        entry.destructor = pkix_pl_LdapResponse_Destroy;
        entry.equalsFunction = pkix_pl_LdapResponse_Equals;
        entry.hashcodeFunction = pkix_pl_LdapResponse_Hashcode;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_LDAPRESPONSE_TYPE] = entry;

        PKIX_RETURN(LDAPRESPONSE);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: pkix_pl_LdapResponse_Create
 * DESCRIPTION:
 *
 *  This function creates an LdapResponse for the LDAPMessageType provided in
 *  "responseType" and a buffer capacity provided by "totalLength". It copies
 *  into its buffer either "totalLength" or "bytesAvailable" bytes, whichever
 *  is less, from the buffer pointed to by "partialData", storing the number of
 *  bytes copied at "pBytesConsumed" and storing the address of the LdapResponse
 *  at "pLdapResponse".
 *
 *  If a message is complete in a single I/O buffer, the LdapResponse will be
 *  complete when this function returns. If the message carries over into
 *  additional buffers, their contents will be added to the LdapResponse by
 *  susequent calls to pkix_pl_LdapResponse_Append.
 *
 * PARAMETERS
 *  "responseType"
 *      The value of the message type (LDAP_SEARCHRESPONSEENTRY_TYPE or
 *      LDAP_SEARCHRESPONSERESULT_TYPE) for the LdapResponse being created
 *  "totalLength"
 *      The UInt32 value for the total length of the encoded message to be
 *      stored in the LdapResponse
 *  "bytesAvailable"
 *      The UInt32 value for the number of bytes of data available in the
 *      current buffer.
 *  "partialData"
 *      The address from which data is to be copied.
 *  "pBytesConsumed"
 *      The address at which is stored the UInt32 number of bytes taken from the
 *      current buffer. If this number is less than "bytesAvailable", then bytes
 *      remain in the buffer for the next LdapResponse. Must be non-NULL.
 *  "pLdapResponse"
 *      The address where the created LdapResponse is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an LdapResponse Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapResponse_Create(
        LDAPMessageType responseType,
        PKIX_UInt32 totalLength,
        PKIX_UInt32 bytesAvailable,
        void *partialData,
        PKIX_UInt32 *pBytesConsumed,
        PKIX_PL_LdapResponse **pLdapResponse,
        void *plContext)
{
        PKIX_UInt32 bytesConsumed = 0;
        PKIX_PL_LdapResponse *ldapResponse = NULL;
        void *data = NULL;

        PKIX_ENTER(LDAPRESPONSE, "PKIX_PL_LdapResponse_Create");
        PKIX_NULLCHECK_ONE(pLdapResponse);

        if (bytesAvailable <= totalLength) {
                bytesConsumed = bytesAvailable;
        } else {
                bytesConsumed = totalLength;
        }

        /* create a PKIX_PL_LdapResponse object */
        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_LDAPRESPONSE_TYPE,
                    sizeof (PKIX_PL_LdapResponse),
                    (PKIX_PL_Object **)&ldapResponse,
                    plContext),
                    PKIX_COULDNOTCREATEOBJECT);

        ldapResponse->decoded.protocolOp.selector = responseType;
        ldapResponse->totalLength = totalLength;
        ldapResponse->partialLength = bytesConsumed;

        if (totalLength != 0){
                /* Alloc space for array */
                PKIX_NULLCHECK_ONE(partialData);

                PKIX_CHECK(PKIX_PL_Malloc
                    (totalLength,
                    &data,
                    plContext),
                    PKIX_MALLOCFAILED);

                PKIX_PL_NSSCALL
                    (LDAPRESPONSE,
                    PORT_Memcpy,
                    (data, partialData, bytesConsumed));
        }

        ldapResponse->derEncoded.type = siBuffer;
        ldapResponse->derEncoded.data = data;
        ldapResponse->derEncoded.len = totalLength;
        *pBytesConsumed = bytesConsumed;
        *pLdapResponse = ldapResponse;

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(ldapResponse);
        }

        PKIX_RETURN(LDAPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_LdapResponse_Append
 * DESCRIPTION:
 *
 *  This function updates the LdapResponse pointed to by "response" with up to
 *  "incrLength" from the buffer pointer to by "incrData", storing the number of
 *  bytes copied at "pBytesConsumed".
 *
 * PARAMETERS
 *  "response"
 *      The address of the LdapResponse being updated. Must be non-zero.
 *  "incrLength"
 *      The UInt32 value for the number of bytes of data available in the
 *      current buffer.
 *  "incrData"
 *      The address from which data is to be copied.
 *  "pBytesConsumed"
 *      The address at which is stored the UInt32 number of bytes taken from the
 *      current buffer. If this number is less than "incrLength", then bytes
 *      remain in the buffer for the next LdapResponse. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an LdapResponse Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapResponse_Append(
        PKIX_PL_LdapResponse *response,
        PKIX_UInt32 incrLength,
        void *incrData,
        PKIX_UInt32 *pBytesConsumed,
        void *plContext)
{
        PKIX_UInt32 newPartialLength = 0;
        PKIX_UInt32 bytesConsumed = 0;
        void *dest = NULL;

        PKIX_ENTER(LDAPRESPONSE, "PKIX_PL_LdapResponse_Append");
        PKIX_NULLCHECK_TWO(response, pBytesConsumed);

        if (incrLength > 0) {

                /* Calculate how many bytes we have room for. */
                bytesConsumed =
                        response->totalLength - response->partialLength;

                if (bytesConsumed > incrLength) {
                        bytesConsumed = incrLength;
                }

                newPartialLength = response->partialLength + bytesConsumed;

                PKIX_NULLCHECK_ONE(incrData);

                dest = &(((char *)response->derEncoded.data)[
                        response->partialLength]);

                PKIX_PL_NSSCALL
                        (LDAPRESPONSE,
                        PORT_Memcpy,
                        (dest, incrData, bytesConsumed));

                response->partialLength = newPartialLength;
        }

        *pBytesConsumed = bytesConsumed;

        PKIX_RETURN(LDAPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_LdapResponse_IsComplete
 * DESCRIPTION:
 *
 *  This function determines whether the LdapResponse pointed to by "response"
 *  contains all the data called for by the "totalLength" parameter provided
 *  when it was created, storing PKIX_TRUE at "pIsComplete" if so, and
 *  PKIX_FALSE otherwise.
 *
 * PARAMETERS
 *  "response"
 *      The address of the LdapResponse being evaluaTED. Must be non-zero.
 *  "incrLength"
 *      The UInt32 value for the number of bytes of data available in the
 *      current buffer.
 *  "incrData"
 *      The address from which data is to be copied.
 *  "pIsComplete"
 *      The address at which is stored the Boolean indication of whether the
 *      LdapResponse is complete. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an LdapResponse Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapResponse_IsComplete(
        PKIX_PL_LdapResponse *response,
        PKIX_Boolean *pIsComplete,
        void *plContext)
{
        PKIX_ENTER(LDAPRESPONSE, "PKIX_PL_LdapResponse_IsComplete");
        PKIX_NULLCHECK_TWO(response, pIsComplete);

        if (response->totalLength == response->partialLength) {
                *pIsComplete = PKIX_TRUE;
        } else {
                *pIsComplete = PKIX_FALSE;
        }

        PKIX_RETURN(LDAPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_LdapResponse_Decode
 * DESCRIPTION:
 *
 *  This function decodes the DER data contained in the LdapResponse pointed to
 *  by "response", using the arena pointed to by "arena", and storing at
 *  "pStatus" SECSuccess if the decoding was successful and SECFailure
 *  otherwise. The decoded message is stored in an element of "response".
 *
 * PARAMETERS
 *  "arena"
 *      The address of the PLArenaPool to be used in the decoding. Must be
 *      non-NULL.
 *  "response"
 *      The address of the LdapResponse whose DER data is to be decoded. Must
 *      be non-NULL.
 *  "pStatus"
 *      The address at which is stored the status from the decoding, SECSuccess
 *      if successful, SECFailure otherwise. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an LdapResponse Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapResponse_Decode(
        PLArenaPool *arena,
        PKIX_PL_LdapResponse *response,
        SECStatus *pStatus,
        void *plContext)
{
        LDAPMessage *msg;
        SECStatus rv = SECFailure;

        PKIX_ENTER(LDAPRESPONSE, "PKIX_PL_LdapResponse_Decode");
        PKIX_NULLCHECK_THREE(arena, response, pStatus);

        if (response->totalLength != response->partialLength) {
                PKIX_ERROR(PKIX_ATTEMPTTODECODEANINCOMPLETERESPONSE);
        }

        msg = &(response->decoded);

        PKIX_PL_NSSCALL
                (LDAPRESPONSE, PORT_Memset, (msg, 0, sizeof (LDAPMessage)));

        PKIX_PL_NSSCALLRV(LDAPRESPONSE, rv, SEC_ASN1DecodeItem,
            (NULL, msg, PKIX_PL_LDAPMessageTemplate, &(response->derEncoded)));

        *pStatus = rv;
cleanup:

        PKIX_RETURN(LDAPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_LdapResponse_GetMessage
 * DESCRIPTION:
 *
 *  This function obtains the decoded message from the LdapResponse pointed to
 *  by "response", storing the result at "pMessage".
 *
 * PARAMETERS
 *  "response"
 *      The address of the LdapResponse whose decoded message is to be
 *      retrieved. Must be non-NULL.
 *  "pMessage"
 *      The address at which is stored the address of the decoded message. Must
 *      be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapResponse_GetMessage(
        PKIX_PL_LdapResponse *response,
        LDAPMessage **pMessage,
        void *plContext)
{
        PKIX_ENTER(LDAPRESPONSE, "PKIX_PL_LdapResponse_GetMessage");
        PKIX_NULLCHECK_TWO(response, pMessage);

        *pMessage = &response->decoded;

        PKIX_RETURN(LDAPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_LdapResponse_GetCapacity
 * DESCRIPTION:
 *
 *  This function obtains from the LdapResponse pointed to by "response" the
 *  number of bytes remaining to be read, based on the totalLength that was
 *  provided to LdapResponse_Create and the data subsequently provided to
 *  LdapResponse_Append, storing the result at "pMessage".
 *
 * PARAMETERS
 *  "response"
 *      The address of the LdapResponse whose remaining capacity is to be
 *      retrieved. Must be non-NULL.
 *  "pCapacity"
 *      The address at which is stored the address of the decoded message. Must
 *      be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an LdapResponse Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapResponse_GetCapacity(
        PKIX_PL_LdapResponse *response,
        PKIX_UInt32 *pCapacity,
        void *plContext)
{
        PKIX_ENTER(LDAPRESPONSE, "PKIX_PL_LdapResponse_GetCapacity");
        PKIX_NULLCHECK_TWO(response, pCapacity);

        *pCapacity = response->totalLength - response->partialLength;

        PKIX_RETURN(LDAPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_LdapResponse_GetMessageType
 * DESCRIPTION:
 *
 *  This function obtains the message type from the LdapResponse pointed to
 *  by "response", storing the result at "pMessageType".
 *
 * PARAMETERS
 *  "response"
 *      The address of the LdapResponse whose message type is to be
 *      retrieved. Must be non-NULL.
 *  "pMessageType" 
 *      The address at which is stored the type of the response message. Must
 *      be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapResponse_GetMessageType(
        PKIX_PL_LdapResponse *response,
        LDAPMessageType *pMessageType,
        void *plContext)
{
        PKIX_ENTER(LDAPRESPONSE, "PKIX_PL_LdapResponse_GetMessageType");
        PKIX_NULLCHECK_TWO(response, pMessageType);

        *pMessageType = response->decoded.protocolOp.selector;

        PKIX_RETURN(LDAPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_LdapResponse_GetResultCode
 * DESCRIPTION:
 *
 *  This function obtains the result code from the LdapResponse pointed to
 *  by "response", storing the result at "pResultCode".
 *
 * PARAMETERS
 *  "response"
 *      The address of the LdapResponse whose result code is to be
 *      retrieved. Must be non-NULL.
 *  "pResultCode"
 *      The address at which is stored the address of the decoded message. Must
 *      be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an LdapResponse Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapResponse_GetResultCode(
        PKIX_PL_LdapResponse *response,
        LDAPResultCode *pResultCode,
        void *plContext)
{
        LDAPMessageType messageType = 0;
        LDAPSearchResponseResult *resultMsg = NULL;

        PKIX_ENTER(LDAPRESPONSE, "PKIX_PL_LdapResponse_GetResultCode");
        PKIX_NULLCHECK_TWO(response, pResultCode);

        messageType = response->decoded.protocolOp.selector;

        if (messageType != LDAP_SEARCHRESPONSERESULT_TYPE) {
                PKIX_ERROR(PKIX_GETRESULTCODECALLEDFORNONRESULTMESSAGE);
        }

        resultMsg = &response->decoded.protocolOp.op.searchResponseResultMsg;

        *pResultCode = *(char *)(resultMsg->resultCode.data);

cleanup:

        PKIX_RETURN(LDAPRESPONSE);
}

/*
 * FUNCTION: pkix_pl_LdapResponse_GetAttributes
 * DESCRIPTION:
 *
 *  This function obtains the attributes from the LdapResponse pointed to
 *  by "response", storing the result at "pAttributes".
 *
 * PARAMETERS
 *  "response"
 *      The address of the LdapResponse whose decoded message is to be
 *      retrieved. Must be non-NULL.
 *  "pAttributes"
 *      The address at which is stored the attributes of the message. Must be
 *      non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an LdapResponse Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapResponse_GetAttributes(
        PKIX_PL_LdapResponse *response,
        LDAPSearchResponseAttr ***pAttributes,
        void *plContext)
{
        LDAPMessageType messageType = 0;

        PKIX_ENTER(LDAPRESPONSE, "PKIX_PL_LdapResponse_GetResultCode");
        PKIX_NULLCHECK_TWO(response, pAttributes);

        messageType = response->decoded.protocolOp.selector;

        if (messageType != LDAP_SEARCHRESPONSEENTRY_TYPE) {
                PKIX_ERROR(PKIX_GETATTRIBUTESCALLEDFORNONENTRYMESSAGE);
        }

        *pAttributes = response->
                decoded.protocolOp.op.searchResponseEntryMsg.attributes;

cleanup:

        PKIX_RETURN(LDAPRESPONSE);
}
