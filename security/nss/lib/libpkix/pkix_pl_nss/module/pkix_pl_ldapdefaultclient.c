/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_ldapdefaultclient.c
 *
 * LDAPDefaultClient Function Definitions
 *
 */

/* We can't decode the length of a message without at least this many bytes */
#define MINIMUM_MSG_LENGTH 5

#include "pkix_pl_ldapdefaultclient.h"

/* --Private-LdapDefaultClient-Message-Building-Functions---------------- */

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_MakeBind
 * DESCRIPTION:
 *
 *  This function creates and encodes a Bind message, using the arena pointed
 *  to by "arena", the version number contained in "versionData", the
 *  LDAPBindAPI pointed to by "bindAPI", and the messageID contained in
 *  "msgNum", and stores a pointer to the encoded string at "pBindMsg".
 *
 *  See pkix_pl_ldaptemplates.c for the ASN.1 description of a Bind message.
 *
 *  This code is not used if the DefaultClient was created with a NULL pointer
 *  supplied for the LDAPBindAPI structure. (Bind and Unbind do not seem to be
 *  expected for anonymous Search requests.)
 *
 * PARAMETERS:
 *  "arena"
 *      The address of the PLArenaPool used in encoding the message. Must be
 *       non-NULL.
 *  "versionData"
 *      The Int32 containing the version number to be encoded in the Bind
 *      message.
 *  "bindAPI"
 *      The address of the LDAPBindAPI to be encoded in the Bind message. Must
 *      be non-NULL.
 *  "msgNum"
 *      The Int32 containing the MessageID to be encoded in the Bind message.
 *  "pBindMsg"
 *      The address at which the encoded Bind message will be stored. Must be
 *      non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_MakeBind(
        PLArenaPool *arena,
        PKIX_Int32 versionData,
        LDAPBindAPI *bindAPI,
        PKIX_UInt32 msgNum,
        SECItem **pBindMsg,
        void *plContext)
{
        LDAPMessage msg;
        char version = '\0';
        SECItem *encoded = NULL;
        PKIX_UInt32 len = 0;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_MakeBind");
        PKIX_NULLCHECK_TWO(arena, pBindMsg);

        PKIX_PL_NSSCALL(LDAPDEFAULTCLIENT, PORT_Memset,
                (&msg, 0, sizeof (LDAPMessage)));

        version = (char)versionData;

        msg.messageID.type = siUnsignedInteger;
        msg.messageID.data = (void*)&msgNum;
        msg.messageID.len = sizeof (msgNum);

        msg.protocolOp.selector = LDAP_BIND_TYPE;

        msg.protocolOp.op.bindMsg.version.type = siUnsignedInteger;
        msg.protocolOp.op.bindMsg.version.data = (void *)&version;
        msg.protocolOp.op.bindMsg.version.len = sizeof (char);

        /*
         * XXX At present we only know how to handle anonymous requests (no
         * authentication), and we are guessing how to do simple authentication.
         * This section will need to be revised and extended when other
         * authentication is needed.
         */
        if (bindAPI->selector == SIMPLE_AUTH) {
                msg.protocolOp.op.bindMsg.bindName.type = siAsciiString;
                msg.protocolOp.op.bindMsg.bindName.data =
                        (void *)bindAPI->chooser.simple.bindName;
                len = PL_strlen(bindAPI->chooser.simple.bindName);
                msg.protocolOp.op.bindMsg.bindName.len = len;

                msg.protocolOp.op.bindMsg.authentication.type = siAsciiString;
                msg.protocolOp.op.bindMsg.authentication.data =
                        (void *)bindAPI->chooser.simple.authentication;
                len = PL_strlen(bindAPI->chooser.simple.authentication);
                msg.protocolOp.op.bindMsg.authentication.len = len;
        }

        PKIX_PL_NSSCALLRV(LDAPDEFAULTCLIENT, encoded, SEC_ASN1EncodeItem,
                (arena, NULL, (void *)&msg, PKIX_PL_LDAPMessageTemplate));
        if (!encoded) {
                PKIX_ERROR(PKIX_SECASN1ENCODEITEMFAILED);
        }

        *pBindMsg = encoded;
cleanup:

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_MakeUnbind
 * DESCRIPTION:
 *
 *  This function creates and encodes a Unbind message, using the arena pointed
 *  to by "arena" and the messageID contained in "msgNum", and stores a pointer
 *  to the encoded string at "pUnbindMsg".
 *
 *  See pkix_pl_ldaptemplates.c for the ASN.1 description of an Unbind message.
 *
 *  This code is not used if the DefaultClient was created with a NULL pointer
 *  supplied for the LDAPBindAPI structure. (Bind and Unbind do not seem to be
 *  expected for anonymous Search requests.)
 *
 * PARAMETERS:
 *  "arena"
 *      The address of the PLArenaPool used in encoding the message. Must be
 *       non-NULL.
 *  "msgNum"
 *      The Int32 containing the MessageID to be encoded in the Unbind message.
 *  "pUnbindMsg"
 *      The address at which the encoded Unbind message will be stored. Must
 *      be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_MakeUnbind(
        PLArenaPool *arena,
        PKIX_UInt32 msgNum,
        SECItem **pUnbindMsg,
        void *plContext)
{
        LDAPMessage msg;
        SECItem *encoded = NULL;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_MakeUnbind");
        PKIX_NULLCHECK_TWO(arena, pUnbindMsg);

        PKIX_PL_NSSCALL(LDAPDEFAULTCLIENT, PORT_Memset,
                (&msg, 0, sizeof (LDAPMessage)));

        msg.messageID.type = siUnsignedInteger;
        msg.messageID.data = (void*)&msgNum;
        msg.messageID.len = sizeof (msgNum);

        msg.protocolOp.selector = LDAP_UNBIND_TYPE;

        msg.protocolOp.op.unbindMsg.dummy.type = siBuffer;
        msg.protocolOp.op.unbindMsg.dummy.data = NULL;
        msg.protocolOp.op.unbindMsg.dummy.len = 0;

        PKIX_PL_NSSCALLRV(LDAPDEFAULTCLIENT, encoded, SEC_ASN1EncodeItem,
                (arena, NULL, (void *)&msg, PKIX_PL_LDAPMessageTemplate));
        if (!encoded) {
                PKIX_ERROR(PKIX_SECASN1ENCODEITEMFAILED);
        }

        *pUnbindMsg = encoded;
cleanup:

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_MakeAbandon
 * DESCRIPTION:
 *
 *  This function creates and encodes a Abandon message, using the arena pointed
 *  to by "arena" and the messageID contained in "msgNum", and stores a pointer
 *  to the encoded string at "pAbandonMsg".
 *
 *  See pkix_pl_ldaptemplates.c for the ASN.1 description of an Abandon message.
 *
 * PARAMETERS:
 *  "arena"
 *      The address of the PLArenaPool used in encoding the message. Must be
 *       non-NULL.
 *  "msgNum"
 *      The Int32 containing the MessageID to be encoded in the Abandon message.
 *  "pAbandonMsg"
 *      The address at which the encoded Abandon message will be stored. Must
 *      be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_MakeAbandon(
        PLArenaPool *arena,
        PKIX_UInt32 msgNum,
        SECItem **pAbandonMsg,
        void *plContext)
{
        LDAPMessage msg;
        SECItem *encoded = NULL;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_MakeAbandon");
        PKIX_NULLCHECK_TWO(arena, pAbandonMsg);

        PKIX_PL_NSSCALL(LDAPDEFAULTCLIENT, PORT_Memset,
                (&msg, 0, sizeof (LDAPMessage)));

        msg.messageID.type = siUnsignedInteger;
        msg.messageID.data = (void*)&msgNum;
        msg.messageID.len = sizeof (msgNum);

        msg.protocolOp.selector = LDAP_ABANDONREQUEST_TYPE;

        msg.protocolOp.op.abandonRequestMsg.messageID.type = siBuffer;
        msg.protocolOp.op.abandonRequestMsg.messageID.data = (void*)&msgNum;
        msg.protocolOp.op.abandonRequestMsg.messageID.len = sizeof (msgNum);

        PKIX_PL_NSSCALLRV(LDAPDEFAULTCLIENT, encoded, SEC_ASN1EncodeItem,
                (arena, NULL, (void *)&msg, PKIX_PL_LDAPMessageTemplate));
        if (!encoded) {
                PKIX_ERROR(PKIX_SECASN1ENCODEITEMFAILED);
        }

        *pAbandonMsg = encoded;
cleanup:

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_DecodeBindResponse
 * DESCRIPTION:
 *
 *  This function decodes the encoded data pointed to by "src", using the arena
 *  pointed to by "arena", storing the decoded LDAPMessage at "pBindResponse"
 *  and the decoding status at "pStatus".
 *
 * PARAMETERS:
 *  "arena"
 *      The address of the PLArenaPool to be used in decoding the message. Must
 *      be  non-NULL.
 *  "src"
 *      The address of the SECItem containing the DER- (or BER-)encoded string.
 *       Must be non-NULL.
 *  "pBindResponse"
 *      The address at which the LDAPMessage is stored, if the decoding is
 *      successful (the returned status is SECSuccess). Must be non-NULL.
 *  "pStatus"
 *      The address at which the decoding status is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_DecodeBindResponse(
        PLArenaPool *arena,
        SECItem *src,
        LDAPMessage *pBindResponse,
        SECStatus *pStatus,
        void *plContext)
{
        SECStatus rv = SECFailure;
        LDAPMessage response;

        PKIX_ENTER
                (LDAPDEFAULTCLIENT,
                "pkix_pl_LdapDefaultClient_DecodeBindResponse");
        PKIX_NULLCHECK_FOUR(arena, src, pBindResponse, pStatus);

        PKIX_PL_NSSCALL
                (LDAPDEFAULTCLIENT,
                PORT_Memset,
                (&response, 0, sizeof (LDAPMessage)));

        PKIX_PL_NSSCALLRV(LDAPDEFAULTCLIENT, rv, SEC_ASN1DecodeItem,
            (arena, &response, PKIX_PL_LDAPMessageTemplate, src));

        if (rv == SECSuccess) {
                *pBindResponse = response;
        }

        *pStatus = rv;

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_VerifyBindResponse
 * DESCRIPTION:
 *
 *  This function verifies that the contents of the message in the rcvbuf of
 *  the LdapDefaultClient object pointed to by "client",  and whose length is
 *  provided by "buflen", is a response to a successful Bind.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "buflen"
 *      The value of the number of bytes in the receive buffer.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_VerifyBindResponse(
        PKIX_PL_LdapDefaultClient *client,
        PKIX_UInt32 bufLen,
        void *plContext)
{
        SECItem decode = {siBuffer, NULL, 0};
        SECStatus rv = SECFailure;
        LDAPMessage msg;
        LDAPBindResponse *ldapBindResponse = NULL;

        PKIX_ENTER
                (LDAPDEFAULTCLIENT,
                "pkix_pl_LdapDefaultClient_VerifyBindResponse");
        PKIX_NULLCHECK_TWO(client, client->rcvBuf);

        decode.data = (unsigned char *)(client->rcvBuf);
        decode.len = bufLen;

        PKIX_CHECK(pkix_pl_LdapDefaultClient_DecodeBindResponse
                (client->arena, &decode, &msg, &rv, plContext),
                PKIX_LDAPDEFAULTCLIENTDECODEBINDRESPONSEFAILED);

        if (rv == SECSuccess) {
                ldapBindResponse = &msg.protocolOp.op.bindResponseMsg;
                if (*(ldapBindResponse->resultCode.data) == SUCCESS) {
                        client->connectStatus = BOUND;
                } else {
                        PKIX_ERROR(PKIX_BINDREJECTEDBYSERVER);
                }
        } else {
                PKIX_ERROR(PKIX_CANTDECODEBINDRESPONSEFROMSERVER);
        }

cleanup:

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_RecvCheckComplete
 * DESCRIPTION:
 *
 *  This function determines whether the current response in the
 *  LdapDefaultClient pointed to by "client" is complete, in the sense that all
 *  bytes required to satisfy the message length field in the encoding have been
 *  received. If so, the pointer to input data is updated to reflect the number
 *  of bytes consumed, provided by "bytesProcessed". The state machine flag
 *  pointed to by "pKeepGoing" is updated to indicate whether processing can
 *  continue without further input.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "bytesProcessed"
 *      The UInt32 value of the number of bytes consumed from the current
 *      buffer.
 *  "pKeepGoing"
 *      The address at which the Boolean state machine flag is stored to
 *      indicate whether processing can continue without further input.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_RecvCheckComplete(
        PKIX_PL_LdapDefaultClient *client,
        PKIX_UInt32 bytesProcessed,
        PKIX_Boolean *pKeepGoing,
        void *plContext)
{
        PKIX_Boolean complete = PKIX_FALSE;
        SECStatus rv = SECFailure;
        LDAPMessageType messageType = 0;
        LDAPResultCode resultCode = 0;

        PKIX_ENTER
                (LDAPDEFAULTCLIENT,
                "pkix_pl_LdapDefaultClient_RecvCheckComplete");
        PKIX_NULLCHECK_TWO(client, pKeepGoing);

        PKIX_CHECK(pkix_pl_LdapResponse_IsComplete
                (client->currentResponse, &complete, plContext),
                PKIX_LDAPRESPONSEISCOMPLETEFAILED);

        if (complete) {
                PKIX_CHECK(pkix_pl_LdapResponse_Decode
                       (client->arena, client->currentResponse, &rv, plContext),
                        PKIX_LDAPRESPONSEDECODEFAILED);

                if (rv != SECSuccess) {
                        PKIX_ERROR(PKIX_CANTDECODESEARCHRESPONSEFROMSERVER);
                }

                PKIX_CHECK(pkix_pl_LdapResponse_GetMessageType
                        (client->currentResponse, &messageType, plContext),
                        PKIX_LDAPRESPONSEGETMESSAGETYPEFAILED);

                if (messageType == LDAP_SEARCHRESPONSEENTRY_TYPE) {

                        if (client->entriesFound == NULL) {
                                PKIX_CHECK(PKIX_List_Create
                                    (&(client->entriesFound), plContext),
                                    PKIX_LISTCREATEFAILED);
                        }

                        PKIX_CHECK(PKIX_List_AppendItem
                                (client->entriesFound,
                                (PKIX_PL_Object *)client->currentResponse,
                                plContext),
                                PKIX_LISTAPPENDITEMFAILED);

                        PKIX_DECREF(client->currentResponse);

                        /* current receive buffer empty? */
                        if (client->currentBytesAvailable == 0) {
                                client->connectStatus = RECV;
                                *pKeepGoing = PKIX_TRUE;
                        } else {
                                client->connectStatus = RECV_INITIAL;
                                client->currentInPtr = &((char *)
                                    (client->currentInPtr))[bytesProcessed];
                                *pKeepGoing = PKIX_TRUE;
                        }

                } else if (messageType == LDAP_SEARCHRESPONSERESULT_TYPE) {
                        PKIX_CHECK(pkix_pl_LdapResponse_GetResultCode
                                (client->currentResponse,
                                &resultCode,
                                plContext),
                                PKIX_LDAPRESPONSEGETRESULTCODEFAILED);

                        if ((client->entriesFound == NULL) &&
                            ((resultCode == SUCCESS) ||
                            (resultCode == NOSUCHOBJECT))) {
                                PKIX_CHECK(PKIX_List_Create
                                    (&(client->entriesFound), plContext),
                                    PKIX_LISTCREATEFAILED);
                        } else if (resultCode == SUCCESS) {
                                PKIX_CHECK(PKIX_List_SetImmutable
                                    (client->entriesFound, plContext),
                                    PKIX_LISTSETIMMUTABLEFAILED);
                                PKIX_CHECK(PKIX_PL_HashTable_Add
                                    (client->cachePtr,
                                    (PKIX_PL_Object *)client->currentRequest,
                                    (PKIX_PL_Object *)client->entriesFound,
                                    plContext),
                                    PKIX_HASHTABLEADDFAILED);
                        } else {
                            PKIX_ERROR(PKIX_UNEXPECTEDRESULTCODEINRESPONSE);
                        }

                        client->connectStatus = BOUND;
                        *pKeepGoing = PKIX_FALSE;
                        PKIX_DECREF(client->currentResponse);

                } else {
                        PKIX_ERROR(PKIX_SEARCHRESPONSEPACKETOFUNKNOWNTYPE);
                }
        } else {
                client->connectStatus = RECV;
                *pKeepGoing = PKIX_TRUE;
        }

cleanup:
        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/* --Private-LdapDefaultClient-Object-Functions------------------------- */

static PKIX_Error *
pkix_pl_LdapDefaultClient_InitiateRequest(
        PKIX_PL_LdapClient *client,
        LDAPRequestParams *requestParams,
        void **pPollDesc,
        PKIX_List **pResponse,
        void *plContext);

static PKIX_Error *
pkix_pl_LdapDefaultClient_ResumeRequest(
        PKIX_PL_LdapClient *client,
        void **pPollDesc,
        PKIX_List **pResponse,
        void *plContext);

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_CreateHelper
 * DESCRIPTION:
 *
 *  This function creates a new LdapDefaultClient using the Socket pointed to
 *  by "socket", the PRIntervalTime pointed to by "timeout", and the
 *  LDAPBindAPI pointed to by "bindAPI", and stores the result at "pClient".
 *
 *  A value of zero for "timeout" means the LDAPClient will use non-blocking
 *  I/O.
 *
 * PARAMETERS:
 *  "socket"
 *      Address of the Socket to be used for the client. Must be non-NULL.
 *  "bindAPI"
 *      The address of the LDAPBindAPI containing the Bind information to be
 *      encoded in the Bind message.
 *  "pClient"
 *      The address at which the created LdapDefaultClient is to be stored.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in
 *      a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapDefaultClient_CreateHelper(
        PKIX_PL_Socket *socket,
        LDAPBindAPI *bindAPI,
        PKIX_PL_LdapDefaultClient **pClient,
        void *plContext)
{
        PKIX_PL_HashTable *ht;
        PKIX_PL_LdapDefaultClient *ldapDefaultClient = NULL;
        PKIX_PL_Socket_Callback *callbackList;
        PRFileDesc *fileDesc = NULL;
        PLArenaPool *arena = NULL;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_CreateHelper");
        PKIX_NULLCHECK_TWO(socket, pClient);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_LDAPDEFAULTCLIENT_TYPE,
                    sizeof (PKIX_PL_LdapDefaultClient),
                    (PKIX_PL_Object **)&ldapDefaultClient,
                    plContext),
                    PKIX_COULDNOTCREATELDAPDEFAULTCLIENTOBJECT);

        ldapDefaultClient->vtable.initiateFcn =
                pkix_pl_LdapDefaultClient_InitiateRequest;
        ldapDefaultClient->vtable.resumeFcn =
                pkix_pl_LdapDefaultClient_ResumeRequest;

        PKIX_CHECK(pkix_pl_Socket_GetPRFileDesc
                (socket, &fileDesc, plContext),
                PKIX_SOCKETGETPRFILEDESCFAILED);

        ldapDefaultClient->pollDesc.fd = fileDesc;
        ldapDefaultClient->pollDesc.in_flags = 0;
        ldapDefaultClient->pollDesc.out_flags = 0;

        ldapDefaultClient->bindAPI = bindAPI;

        PKIX_CHECK(PKIX_PL_HashTable_Create
                (LDAP_CACHEBUCKETS, 0, &ht, plContext),
                PKIX_HASHTABLECREATEFAILED);

        ldapDefaultClient->cachePtr = ht;

        PKIX_CHECK(pkix_pl_Socket_GetCallbackList
                (socket, &callbackList, plContext),
                PKIX_SOCKETGETCALLBACKLISTFAILED);

        ldapDefaultClient->callbackList = callbackList;

        PKIX_INCREF(socket);
        ldapDefaultClient->clientSocket = socket;

        ldapDefaultClient->messageID = 0;

        ldapDefaultClient->bindAPI = bindAPI;

        arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
        if (!arena) {
            PKIX_ERROR_FATAL(PKIX_OUTOFMEMORY);
        }
        ldapDefaultClient->arena = arena;

        ldapDefaultClient->sendBuf = NULL;
        ldapDefaultClient->bytesToWrite = 0;

        PKIX_CHECK(PKIX_PL_Malloc
                (RCVBUFSIZE, &ldapDefaultClient->rcvBuf, plContext),
                PKIX_MALLOCFAILED);
        ldapDefaultClient->capacity = RCVBUFSIZE;

        ldapDefaultClient->bindMsg = NULL;
        ldapDefaultClient->bindMsgLen = 0;

        ldapDefaultClient->entriesFound = NULL;
        ldapDefaultClient->currentRequest = NULL;
        ldapDefaultClient->currentResponse = NULL;

        *pClient = ldapDefaultClient;

cleanup:

        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(ldapDefaultClient);
        }

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: PKIX_PL_LdapDefaultClient_Create
 * DESCRIPTION:
 *
 *  This function creates a new LdapDefaultClient using the PRNetAddr pointed to
 *  by "sockaddr", the PRIntervalTime pointed to by "timeout", and the
 *  LDAPBindAPI pointed to by "bindAPI", and stores the result at "pClient".
 *
 *  A value of zero for "timeout" means the LDAPClient will use non-blocking
 *  I/O.
 *
 * PARAMETERS:
 *  "sockaddr"
 *      Address of the PRNetAddr to be used for the socket connection. Must be
 *      non-NULL.
 *  "timeout"
 *      The PRIntervalTime to be used in I/O requests for this client.
 *  "bindAPI"
 *      The address of the LDAPBindAPI containing the Bind information to be
 *      encoded in the Bind message.
 *  "pClient"
 *      The address at which the created LdapDefaultClient is to be stored.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in
 *      a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
PKIX_PL_LdapDefaultClient_Create(
        PRNetAddr *sockaddr,
        PRIntervalTime timeout,
        LDAPBindAPI *bindAPI,
        PKIX_PL_LdapDefaultClient **pClient,
        void *plContext)
{
        PRErrorCode status = 0;
        PKIX_PL_Socket *socket = NULL;
        PKIX_PL_LdapDefaultClient *client = NULL;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "PKIX_PL_LdapDefaultClient_Create");
        PKIX_NULLCHECK_TWO(sockaddr, pClient);

        PKIX_CHECK(pkix_pl_Socket_Create
                (PKIX_FALSE, timeout, sockaddr, &status, &socket, plContext),
                PKIX_SOCKETCREATEFAILED);

        PKIX_CHECK(pkix_pl_LdapDefaultClient_CreateHelper
                (socket, bindAPI, &client, plContext),
                PKIX_LDAPDEFAULTCLIENTCREATEHELPERFAILED);

        /* Did Socket_Create say the connection was made? */
        if (status == 0) {
                if (client->bindAPI != NULL) {
                        client->connectStatus = CONNECTED;
                } else {
                        client->connectStatus = BOUND;
                }
        } else {
                client->connectStatus = CONNECT_PENDING;
        }

        *pClient = client;

cleanup:
        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(client);
        }

        PKIX_DECREF(socket);

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: PKIX_PL_LdapDefaultClient_CreateByName
 * DESCRIPTION:
 *
 *  This function creates a new LdapDefaultClient using the hostname pointed to
 *  by "hostname", the PRIntervalTime pointed to by "timeout", and the
 *  LDAPBindAPI pointed to by "bindAPI", and stores the result at "pClient".
 *
 *  A value of zero for "timeout" means the LDAPClient will use non-blocking
 *  I/O.
 *
 * PARAMETERS:
 *  "hostname"
 *      Address of the hostname to be used for the socket connection. Must be
 *      non-NULL.
 *  "timeout"
 *      The PRIntervalTime to be used in I/O requests for this client.
 *  "bindAPI"
 *      The address of the LDAPBindAPI containing the Bind information to be
 *      encoded in the Bind message.
 *  "pClient"
 *      The address at which the created LdapDefaultClient is to be stored.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in
 *      a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
PKIX_PL_LdapDefaultClient_CreateByName(
        char *hostname,
        PRIntervalTime timeout,
        LDAPBindAPI *bindAPI,
        PKIX_PL_LdapDefaultClient **pClient,
        void *plContext)
{
        PRErrorCode status = 0;
        PKIX_PL_Socket *socket = NULL;
        PKIX_PL_LdapDefaultClient *client = NULL;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "PKIX_PL_LdapDefaultClient_CreateByName");
        PKIX_NULLCHECK_TWO(hostname, pClient);

        PKIX_CHECK(pkix_pl_Socket_CreateByName
                (PKIX_FALSE, timeout, hostname, &status, &socket, plContext),
                PKIX_SOCKETCREATEBYNAMEFAILED);

        PKIX_CHECK(pkix_pl_LdapDefaultClient_CreateHelper
                (socket, bindAPI, &client, plContext),
                PKIX_LDAPDEFAULTCLIENTCREATEHELPERFAILED);

        /* Did Socket_Create say the connection was made? */
        if (status == 0) {
                if (client->bindAPI != NULL) {
                        client->connectStatus = CONNECTED;
                } else {
                        client->connectStatus = BOUND;
                }
        } else {
                client->connectStatus = CONNECT_PENDING;
        }

        *pClient = client;

cleanup:
        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(client);
        }

        PKIX_DECREF(socket);

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_Int32 bytesWritten = 0;
        PKIX_PL_LdapDefaultClient *client = NULL;
        PKIX_PL_Socket_Callback *callbackList = NULL;
        SECItem *encoded = NULL;

        PKIX_ENTER(LDAPDEFAULTCLIENT,
                    "pkix_pl_LdapDefaultClient_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_LDAPDEFAULTCLIENT_TYPE, plContext),
                    PKIX_OBJECTNOTANLDAPDEFAULTCLIENT);

        client = (PKIX_PL_LdapDefaultClient *)object;

        switch (client->connectStatus) {
        case CONNECT_PENDING:
                break;
        case CONNECTED:
        case BIND_PENDING:
        case BIND_RESPONSE:
        case BIND_RESPONSE_PENDING:
        case BOUND:
        case SEND_PENDING:
        case RECV:
        case RECV_PENDING:
        case RECV_INITIAL:
        case RECV_NONINITIAL:
        case ABANDON_PENDING:
                if (client->bindAPI != NULL) {
                        PKIX_CHECK(pkix_pl_LdapDefaultClient_MakeUnbind
                                (client->arena,
                                ++(client->messageID),
                                &encoded,
                                plContext),
                                PKIX_LDAPDEFAULTCLIENTMAKEUNBINDFAILED);

                        callbackList =
                                (PKIX_PL_Socket_Callback *)(client->callbackList);
                        PKIX_CHECK(callbackList->sendCallback
                                (client->clientSocket,
                                encoded->data,
                                encoded->len,
                                &bytesWritten,
                                plContext),
                                PKIX_SOCKETSENDFAILED);
                }
                break;
        default:
                PKIX_ERROR(PKIX_LDAPDEFAULTCLIENTINILLEGALSTATE);
        }

        PKIX_DECREF(client->cachePtr);
        PKIX_DECREF(client->clientSocket);
        PKIX_DECREF(client->entriesFound);
        PKIX_DECREF(client->currentRequest);
        PKIX_DECREF(client->currentResponse);

        PKIX_CHECK(PKIX_PL_Free
		(client->rcvBuf, plContext), PKIX_FREEFAILED);

        PKIX_PL_NSSCALL
                (LDAPDEFAULTCLIENT,
                PORT_FreeArena,
                (client->arena, PR_FALSE));

cleanup:

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_LdapDefaultClient *ldapDefaultClient = NULL;
        PKIX_UInt32 tempHash = 0;

        PKIX_ENTER
                (LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType
                (object, PKIX_LDAPDEFAULTCLIENT_TYPE, plContext),
                PKIX_OBJECTNOTANLDAPDEFAULTCLIENT);

        ldapDefaultClient = (PKIX_PL_LdapDefaultClient *)object;

        PKIX_CHECK(PKIX_PL_Object_Hashcode
                ((PKIX_PL_Object *)ldapDefaultClient->clientSocket,
                &tempHash,
                plContext),
                PKIX_SOCKETHASHCODEFAILED);

        if (ldapDefaultClient->bindAPI != NULL) {
                tempHash = (tempHash << 7) +
                        ldapDefaultClient->bindAPI->selector;
        }

        *pHashcode = tempHash;

cleanup:

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_Equals
 * (see comments for PKIX_PL_EqualsCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Int32 *pResult,
        void *plContext)
{
        PKIX_PL_LdapDefaultClient *firstClientContext = NULL;
        PKIX_PL_LdapDefaultClient *secondClientContext = NULL;
        PKIX_Int32 compare = 0;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        *pResult = PKIX_FALSE;

        PKIX_CHECK(pkix_CheckTypes
                (firstObject,
                secondObject,
                PKIX_LDAPDEFAULTCLIENT_TYPE,
                plContext),
                PKIX_OBJECTNOTANLDAPDEFAULTCLIENT);

        firstClientContext = (PKIX_PL_LdapDefaultClient *)firstObject;
        secondClientContext = (PKIX_PL_LdapDefaultClient *)secondObject;

        if (firstClientContext == secondClientContext) {
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        PKIX_CHECK(PKIX_PL_Object_Equals
                ((PKIX_PL_Object *)firstClientContext->clientSocket,
                (PKIX_PL_Object *)secondClientContext->clientSocket,
                &compare,
                plContext),
                PKIX_SOCKETEQUALSFAILED);

        if (!compare) {
                goto cleanup;
        }

        if (PKIX_EXACTLY_ONE_NULL
                (firstClientContext->bindAPI, secondClientContext->bindAPI)) {
                goto cleanup;
        }

        if (firstClientContext->bindAPI) {
                if (firstClientContext->bindAPI->selector !=
                    secondClientContext->bindAPI->selector) {
                        goto cleanup;
                }
        }

        *pResult = PKIX_TRUE;

cleanup:

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_RegisterSelf
 *
 * DESCRIPTION:
 *  Registers PKIX_PL_LDAPDEFAULTCLIENT_TYPE and its related
 *  functions with systemClasses[]
 *
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_LdapDefaultClient_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER
                (LDAPDEFAULTCLIENT,
                "pkix_pl_LdapDefaultClient_RegisterSelf");

        entry.description = "LdapDefaultClient";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_LdapDefaultClient);
        entry.destructor = pkix_pl_LdapDefaultClient_Destroy;
        entry.equalsFunction = pkix_pl_LdapDefaultClient_Equals;
        entry.hashcodeFunction = pkix_pl_LdapDefaultClient_Hashcode;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_LDAPDEFAULTCLIENT_TYPE] = entry;

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_GetPollDesc
 * DESCRIPTION:
 *
 *  This function retrieves the PRPollDesc from the LdapDefaultClient
 *  pointed to by "context" and stores the address at "pPollDesc".
 *
 * PARAMETERS:
 *  "context"
 *      The LdapDefaultClient whose PRPollDesc is desired. Must be non-NULL.
 *  "pPollDesc"
 *      Address where PRPollDesc will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapDefaultClient_GetPollDesc(
        PKIX_PL_LdapDefaultClient *context,
        PRPollDesc **pPollDesc,
        void *plContext)
{
        PKIX_ENTER
                (LDAPDEFAULTCLIENT,
                "pkix_pl_LdapDefaultClient_GetPollDesc");
        PKIX_NULLCHECK_TWO(context, pPollDesc);

        *pPollDesc = &(context->pollDesc);

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/* --Private-Ldap-CertStore-I/O-Functions---------------------------- */
/*
 * FUNCTION: pkix_pl_LdapDefaultClient_ConnectContinue
 * DESCRIPTION:
 *
 *  This function determines whether a socket Connect initiated earlier for the
 *  CertStore embodied in the LdapDefaultClient "client" has completed, and
 *  stores in "pKeepGoing" a flag indicating whether processing can continue
 *  without further input.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "pKeepGoing"
 *      The address at which the Boolean state machine flag is stored to
 *      indicate whether processing can continue without further input.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_ConnectContinue(
        PKIX_PL_LdapDefaultClient *client,
        PKIX_Boolean *pKeepGoing,
        void *plContext)
{
        PKIX_PL_Socket_Callback *callbackList;
        PRErrorCode status;
        PKIX_Boolean keepGoing = PKIX_FALSE;

        PKIX_ENTER
                (LDAPDEFAULTCLIENT,
                "pkix_pl_LdapDefaultClient_ConnectContinue");
        PKIX_NULLCHECK_ONE(client);

        callbackList = (PKIX_PL_Socket_Callback *)(client->callbackList);

        PKIX_CHECK(callbackList->connectcontinueCallback
                (client->clientSocket, &status, plContext),
                PKIX_SOCKETCONNECTCONTINUEFAILED);

        if (status == 0) {
                if (client->bindAPI != NULL) {
                        client->connectStatus = CONNECTED;
                } else {
                        client->connectStatus = BOUND;
                }
                keepGoing = PKIX_FALSE;
        } else if (status != PR_IN_PROGRESS_ERROR) {
                PKIX_ERROR(PKIX_UNEXPECTEDERRORINESTABLISHINGCONNECTION);
        }

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)client, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

        *pKeepGoing = keepGoing;

cleanup:
        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_Bind
 * DESCRIPTION:
 *
 *  This function creates and sends the LDAP-protocol Bind message for the
 *  CertStore embodied in the LdapDefaultClient "client", and stores in
 *  "pKeepGoing" a flag indicating whether processing can continue without
 *  further input.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "pKeepGoing"
 *      The address at which the Boolean state machine flag is stored to
 *      indicate whether processing can continue without further input.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_Bind(
        PKIX_PL_LdapDefaultClient *client,
        PKIX_Boolean *pKeepGoing,
        void *plContext)
{
        SECItem *encoded = NULL;
        PKIX_Int32 bytesWritten = 0;
        PKIX_PL_Socket_Callback *callbackList;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_Bind");
        PKIX_NULLCHECK_ONE(client);

        /* if we have not yet constructed the BIND message, build it now */
        if (!(client->bindMsg)) {
                PKIX_CHECK(pkix_pl_LdapDefaultClient_MakeBind
                        (client->arena,
                        3,
                        client->bindAPI,
                        client->messageID,
                        &encoded,
                        plContext),
                        PKIX_LDAPDEFAULTCLIENTMAKEBINDFAILED);
                client->bindMsg = encoded->data;
                client->bindMsgLen = encoded->len;
        }

        callbackList = (PKIX_PL_Socket_Callback *)(client->callbackList);

        PKIX_CHECK(callbackList->sendCallback
                (client->clientSocket,
                client->bindMsg,
                client->bindMsgLen,
                &bytesWritten,
                plContext),
                PKIX_SOCKETSENDFAILED);

        client->lastIO = PR_Now();

        if (bytesWritten < 0) {
                client->connectStatus = BIND_PENDING;
                *pKeepGoing = PKIX_FALSE;
        } else {
                client->connectStatus = BIND_RESPONSE;
                *pKeepGoing = PKIX_TRUE;
        }

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)client, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:
        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_BindContinue
 * DESCRIPTION:
 *
 *  This function determines whether the LDAP-protocol Bind message for the
 *  CertStore embodied in the LdapDefaultClient "client" has completed, and
 *  stores in "pKeepGoing" a flag indicating whether processing can continue
 *  without further input.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "pKeepGoing"
 *      The address at which the Boolean state machine flag is stored to
 *      indicate whether processing can continue without further input.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *pkix_pl_LdapDefaultClient_BindContinue(
        PKIX_PL_LdapDefaultClient *client,
        PKIX_Boolean *pKeepGoing,
        void *plContext)
{
        PKIX_Int32 bytesWritten = 0;
        PKIX_PL_Socket_Callback *callbackList = NULL;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_BindContinue");
        PKIX_NULLCHECK_ONE(client);

        *pKeepGoing = PKIX_FALSE;

        callbackList = (PKIX_PL_Socket_Callback *)(client->callbackList);

        PKIX_CHECK(callbackList->pollCallback
                (client->clientSocket, &bytesWritten, NULL, plContext),
                PKIX_SOCKETPOLLFAILED);

        /*
         * If the send completed we can proceed to try for the
         * response. If the send did not complete we will have
         * continue to poll.
         */
        if (bytesWritten >= 0) {

                client->connectStatus = BIND_RESPONSE;

                PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                        ((PKIX_PL_Object *)client, plContext),
                        PKIX_OBJECTINVALIDATECACHEFAILED);

                *pKeepGoing = PKIX_TRUE;
        }

cleanup:
        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_BindResponse
 * DESCRIPTION:
 *
 *  This function attempts to read the LDAP-protocol BindResponse message for
 *  the CertStore embodied in the LdapDefaultClient "client", and stores in
 *  "pKeepGoing" a flag indicating whether processing can continue without
 *  further input.
 *
 *  If a BindResponse is received with a Result code of 0 (success), we
 *  continue with the connection. If a non-zero Result code is received,
 *  we throw an Error. Some more sophisticated handling of that condition
 *  might be in order in the future.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "pKeepGoing"
 *      The address at which the Boolean state machine flag is stored to
 *      indicate whether processing can continue without further input.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_BindResponse(
        PKIX_PL_LdapDefaultClient *client,
        PKIX_Boolean *pKeepGoing,
        void *plContext)
{
        PKIX_Int32 bytesRead = 0;
        PKIX_PL_Socket_Callback *callbackList = NULL;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_BindResponse");
        PKIX_NULLCHECK_TWO(client, client->rcvBuf);

        callbackList = (PKIX_PL_Socket_Callback *)(client->callbackList);

        PKIX_CHECK(callbackList->recvCallback
                (client->clientSocket,
                client->rcvBuf,
                client->capacity,
                &bytesRead,
                plContext),
                PKIX_SOCKETRECVFAILED);

        client->lastIO = PR_Now();

        if (bytesRead > 0) {
                PKIX_CHECK(pkix_pl_LdapDefaultClient_VerifyBindResponse
                        (client, bytesRead, plContext),
                        PKIX_LDAPDEFAULTCLIENTVERIFYBINDRESPONSEFAILED);
                /*
                 * XXX What should we do if failure? At present if
                 * VerifyBindResponse throws an Error, we do too.
                 */
                client->connectStatus = BOUND;
        } else {
                client->connectStatus = BIND_RESPONSE_PENDING;
        }

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)client, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

        *pKeepGoing = PKIX_TRUE;

cleanup:
        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_BindResponseContinue
 * DESCRIPTION:
 *
 *  This function determines whether the LDAP-protocol BindResponse message for
 *  the CertStore embodied in the LdapDefaultClient "client" has completed, and
 *  stores in "pKeepGoing" a flag indicating whether processing can continue
 *  without further input.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "pKeepGoing"
 *      The address at which the Boolean state machine flag is stored to
 *      indicate whether processing can continue without further input.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_BindResponseContinue(
        PKIX_PL_LdapDefaultClient *client,
        PKIX_Boolean *pKeepGoing,
        void *plContext)
{
        PKIX_Int32 bytesRead = 0;
        PKIX_PL_Socket_Callback *callbackList = NULL;

        PKIX_ENTER
                (LDAPDEFAULTCLIENT,
                "pkix_pl_LdapDefaultClient_BindResponseContinue");
        PKIX_NULLCHECK_ONE(client);

        callbackList = (PKIX_PL_Socket_Callback *)(client->callbackList);

        PKIX_CHECK(callbackList->pollCallback
                (client->clientSocket, NULL, &bytesRead, plContext),
                PKIX_SOCKETPOLLFAILED);

        if (bytesRead > 0) {
                PKIX_CHECK(pkix_pl_LdapDefaultClient_VerifyBindResponse
                        (client, bytesRead, plContext),
                        PKIX_LDAPDEFAULTCLIENTVERIFYBINDRESPONSEFAILED);
                client->connectStatus = BOUND;

                PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                        ((PKIX_PL_Object *)client, plContext),
                        PKIX_OBJECTINVALIDATECACHEFAILED);

                *pKeepGoing = PKIX_TRUE;
        } else {
                *pKeepGoing = PKIX_FALSE;
        }

cleanup:
        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_Send
 * DESCRIPTION:
 *
 *  This function creates and sends an LDAP-protocol message for the
 *  CertStore embodied in the LdapDefaultClient "client", and stores in
 *  "pKeepGoing" a flag indicating whether processing can continue without
 *  further input, and at "pBytesTransferred" the number of bytes sent.
 *
 *  If "pBytesTransferred" is zero, it indicates that non-blocking I/O is in use
 *  and that transmission has not completed.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "pKeepGoing"
 *      The address at which the Boolean state machine flag is stored to
 *      indicate whether processing can continue without further input.
 *      Must be non-NULL.
 *  "pBytesTransferred"
 *      The address at which the number of bytes sent is stored. Must be
 *      non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_Send(
        PKIX_PL_LdapDefaultClient *client,
        PKIX_Boolean *pKeepGoing,
        PKIX_UInt32 *pBytesTransferred,
        void *plContext)
{
        PKIX_Int32 bytesWritten = 0;
        PKIX_PL_Socket_Callback *callbackList = NULL;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_Send");
        PKIX_NULLCHECK_THREE(client, pKeepGoing, pBytesTransferred);

        *pKeepGoing = PKIX_FALSE;

        /* Do we have anything waiting to go? */
        if (client->sendBuf) {
                callbackList = (PKIX_PL_Socket_Callback *)(client->callbackList);

                PKIX_CHECK(callbackList->sendCallback
                        (client->clientSocket,
                        client->sendBuf,
                        client->bytesToWrite,
                        &bytesWritten,
                        plContext),
                        PKIX_SOCKETSENDFAILED);

                client->lastIO = PR_Now();

                /*
                 * If the send completed we can proceed to try for the
                 * response. If the send did not complete we will have
                 * to poll for completion later.
                 */
                if (bytesWritten >= 0) {
                        client->sendBuf = NULL;
                        client->connectStatus = RECV;
                        *pKeepGoing = PKIX_TRUE;

                } else {
                        *pKeepGoing = PKIX_FALSE;
                        client->connectStatus = SEND_PENDING;
                }

        }

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)client, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

        *pBytesTransferred = bytesWritten;

cleanup:
        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_SendContinue
 * DESCRIPTION:
 *
 *  This function determines whether the sending of the LDAP-protocol message
 *  for the CertStore embodied in the LdapDefaultClient "client" has completed,
 *  and stores in "pKeepGoing" a flag indicating whether processing can continue
 *  without further input, and at "pBytesTransferred" the number of bytes sent.
 *
 *  If "pBytesTransferred" is zero, it indicates that non-blocking I/O is in use
 *  and that transmission has not completed.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "pKeepGoing"
 *      The address at which the Boolean state machine flag is stored to
 *      indicate whether processing can continue without further input.
 *      Must be non-NULL.
 *  "pBytesTransferred"
 *      The address at which the number of bytes sent is stored. Must be
 *      non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_SendContinue(
        PKIX_PL_LdapDefaultClient *client,
        PKIX_Boolean *pKeepGoing,
        PKIX_UInt32 *pBytesTransferred,
        void *plContext)
{
        PKIX_Int32 bytesWritten = 0;
        PKIX_PL_Socket_Callback *callbackList = NULL;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_SendContinue");
        PKIX_NULLCHECK_THREE(client, pKeepGoing, pBytesTransferred);

        *pKeepGoing = PKIX_FALSE;

        callbackList = (PKIX_PL_Socket_Callback *)(client->callbackList);

        PKIX_CHECK(callbackList->pollCallback
                (client->clientSocket, &bytesWritten, NULL, plContext),
                PKIX_SOCKETPOLLFAILED);

        /*
         * If the send completed we can proceed to try for the
         * response. If the send did not complete we will have
         * continue to poll.
         */
        if (bytesWritten >= 0) {
                client->sendBuf = NULL;
                client->connectStatus = RECV;

                PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                        ((PKIX_PL_Object *)client, plContext),
                        PKIX_OBJECTINVALIDATECACHEFAILED);

                *pKeepGoing = PKIX_TRUE;
        }

        *pBytesTransferred = bytesWritten;

cleanup:
        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_Recv
 * DESCRIPTION:
 *
 *  This function receives an LDAP-protocol message for the CertStore embodied
 *  in the LdapDefaultClient "client", and stores in "pKeepGoing" a flag
 *  indicating whether processing can continue without further input.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "pKeepGoing"
 *      The address at which the Boolean state machine flag is stored to
 *      indicate whether processing can continue without further input.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_Recv(
        PKIX_PL_LdapDefaultClient *client,
        PKIX_Boolean *pKeepGoing,
        void *plContext)
{
        PKIX_Int32 bytesRead = 0;
        PKIX_UInt32 bytesToRead = 0;
        PKIX_PL_Socket_Callback *callbackList = NULL;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_Recv");
        PKIX_NULLCHECK_THREE(client, pKeepGoing, client->rcvBuf);

        callbackList = (PKIX_PL_Socket_Callback *)(client->callbackList);

        /*
         * If we attempt to fill our buffer with every read, we increase
         * the risk of an ugly situation: one or two bytes of a new message
         * left over at the end of processing one message. With such a
         * fragment, we can't decode a byte count and so won't know how much
         * space to allocate for the next LdapResponse. We try to avoid that
         * case by reading just enough to complete the current message, unless
         * there will be at least MINIMUM_MSG_LENGTH bytes left over.
         */
        if (client->currentResponse) {
                PKIX_CHECK(pkix_pl_LdapResponse_GetCapacity
                        (client->currentResponse, &bytesToRead, plContext),
                        PKIX_LDAPRESPONSEGETCAPACITYFAILED);
                if ((bytesToRead > client->capacity) ||
                    ((bytesToRead + MINIMUM_MSG_LENGTH) < client->capacity)) {
                        bytesToRead = client->capacity;
                }
        } else {
                bytesToRead = client->capacity;
        }

        client->currentBytesAvailable = 0;

        PKIX_CHECK(callbackList->recvCallback
                (client->clientSocket,
                (void *)client->rcvBuf,
                bytesToRead,
                &bytesRead,
                plContext),
                PKIX_SOCKETRECVFAILED);

        client->currentInPtr = client->rcvBuf;
        client->lastIO = PR_Now();

        if (bytesRead > 0) {
                client->currentBytesAvailable = bytesRead;
                client->connectStatus = RECV_INITIAL;
                *pKeepGoing = PKIX_TRUE;
        } else {
                client->connectStatus = RECV_PENDING;
                *pKeepGoing = PKIX_FALSE;
        }

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)client, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:
        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_RecvContinue
 * DESCRIPTION:
 *
 *  This function determines whether the receiving of the LDAP-protocol message
 *  for the CertStore embodied in the LdapDefaultClient "client" has completed,
 *  and stores in "pKeepGoing" a flag indicating whether processing can continue
 *  without further input.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "pKeepGoing"
 *      The address at which the Boolean state machine flag is stored to
 *      indicate whether processing can continue without further input.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_RecvContinue(
        PKIX_PL_LdapDefaultClient *client,
        PKIX_Boolean *pKeepGoing,
        void *plContext)
{
        PKIX_Int32 bytesRead = 0;
        PKIX_PL_Socket_Callback *callbackList = NULL;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_RecvContinue");
        PKIX_NULLCHECK_TWO(client, pKeepGoing);

        callbackList = (PKIX_PL_Socket_Callback *)(client->callbackList);

        PKIX_CHECK(callbackList->pollCallback
                (client->clientSocket, NULL, &bytesRead, plContext),
                PKIX_SOCKETPOLLFAILED);

        if (bytesRead > 0) {
                client->currentBytesAvailable += bytesRead;
                client->connectStatus = RECV_INITIAL;
                *pKeepGoing = PKIX_TRUE;

                PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                        ((PKIX_PL_Object *)client, plContext),
                        PKIX_OBJECTINVALIDATECACHEFAILED);
        } else {
                *pKeepGoing = PKIX_FALSE;
        }

cleanup:
        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_AbandonContinue
 * DESCRIPTION:
 *
 *  This function determines whether the abandon-message request of the
 *  LDAP-protocol message for the CertStore embodied in the LdapDefaultClient
 *  "client" has completed, and stores in "pKeepGoing" a flag indicating whether
 *  processing can continue without further input.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "pKeepGoing"
 *      The address at which the Boolean state machine flag is stored to
 *      indicate whether processing can continue without further input.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_AbandonContinue(
        PKIX_PL_LdapDefaultClient *client,
        PKIX_Boolean *pKeepGoing,
        void *plContext)
{
        PKIX_Int32 bytesWritten = 0;
        PKIX_PL_Socket_Callback *callbackList = NULL;

        PKIX_ENTER
                (LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_AbandonContinue");
        PKIX_NULLCHECK_TWO(client, pKeepGoing);

        callbackList = (PKIX_PL_Socket_Callback *)(client->callbackList);

        PKIX_CHECK(callbackList->pollCallback
                (client->clientSocket, &bytesWritten, NULL, plContext),
                PKIX_SOCKETPOLLFAILED);

        if (bytesWritten > 0) {
                client->connectStatus = BOUND;
                *pKeepGoing = PKIX_TRUE;

                PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                        ((PKIX_PL_Object *)client, plContext),
                        PKIX_OBJECTINVALIDATECACHEFAILED);
        } else {
                *pKeepGoing = PKIX_FALSE;
        }

cleanup:
        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_RecvInitial
 * DESCRIPTION:
 *
 *  This function processes the contents of the first buffer of a received
 *  LDAP-protocol message for the CertStore embodied in the LdapDefaultClient
 *  "client", and stores in "pKeepGoing" a flag indicating whether processing can
 *  continue without further input.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "pKeepGoing"
 *      The address at which the Boolean state machine flag is stored to
 *      indicate whether processing can continue without further input.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_RecvInitial(
        PKIX_PL_LdapDefaultClient *client,
        PKIX_Boolean *pKeepGoing,
        void *plContext)
{
        unsigned char *msgBuf = NULL;
        unsigned char *to = NULL;
        unsigned char *from = NULL;
        PKIX_UInt32 dataIndex = 0;
        PKIX_UInt32 messageIdLen = 0;
        PKIX_UInt32 messageLength = 0;
        PKIX_UInt32 sizeofLength = 0;
        PKIX_UInt32 bytesProcessed = 0;
        unsigned char messageChar = 0;
        LDAPMessageType messageType = 0;
        PKIX_Int32 bytesRead = 0;
        PKIX_PL_Socket_Callback *callbackList = NULL;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_RecvInitial");
        PKIX_NULLCHECK_TWO(client, pKeepGoing);

        /*
         * Is there an LDAPResponse in progress? I.e., have we
         * already processed the tag and length at the beginning of
         * the message?
         */
        if (client->currentResponse) {
                client->connectStatus = RECV_NONINITIAL;
                *pKeepGoing = PKIX_TRUE;
                goto cleanup;
        }
        msgBuf = client->currentInPtr;

        /* Do we have enough of the message to decode the message length? */
        if (client->currentBytesAvailable < MINIMUM_MSG_LENGTH) {
                /*
                 * No! Move these few bytes to the beginning of rcvBuf
                 * and hang another read.
                 */

                to = (unsigned char *)client->rcvBuf;
                from = client->currentInPtr;
                for (dataIndex = 0;
                    dataIndex < client->currentBytesAvailable;
                    dataIndex++) {
                        *to++ = *from++;
                }
                callbackList = (PKIX_PL_Socket_Callback *)(client->callbackList);
                PKIX_CHECK(callbackList->recvCallback
                        (client->clientSocket,
                        (void *)to,
                        client->capacity - client->currentBytesAvailable,
                        &bytesRead,
                        plContext),
                        PKIX_SOCKETRECVFAILED);

                client->currentInPtr = client->rcvBuf;
                client->lastIO = PR_Now();

                if (bytesRead <= 0) {
                        client->connectStatus = RECV_PENDING;
                        *pKeepGoing = PKIX_FALSE;
                        goto cleanup;
                } else {
                        client->currentBytesAvailable += bytesRead;
                }
        }

        /*
         * We have to determine whether the response is an entry, with
         * application-specific tag LDAP_SEARCHRESPONSEENTRY_TYPE, or a
         * resultCode, with application tag LDAP_SEARCHRESPONSERESULT_TYPE.
         * First, we have to figure out where to look for the tag.
         */

        /* Is the message length short form (one octet) or long form? */
        if ((msgBuf[1] & 0x80) != 0) {
                sizeofLength = msgBuf[1] & 0x7F;
                for (dataIndex = 0; dataIndex < sizeofLength; dataIndex++) {
                        messageLength =
                                (messageLength << 8) + msgBuf[dataIndex + 2];
                }
        } else {
                messageLength = msgBuf[1];
        }

        /* How many bytes did the messageID require? */
        messageIdLen = msgBuf[dataIndex + 3];

        messageChar = msgBuf[dataIndex + messageIdLen + 4];

        /* Are we looking at an Entry message or a ResultCode message? */
        if ((SEC_ASN1_CONSTRUCTED | SEC_ASN1_APPLICATION |
            LDAP_SEARCHRESPONSEENTRY_TYPE) == messageChar) {

                messageType = LDAP_SEARCHRESPONSEENTRY_TYPE;

        } else if ((SEC_ASN1_CONSTRUCTED | SEC_ASN1_APPLICATION |
            LDAP_SEARCHRESPONSERESULT_TYPE) == messageChar) {

                messageType = LDAP_SEARCHRESPONSERESULT_TYPE;

        } else {

                PKIX_ERROR(PKIX_SEARCHRESPONSEPACKETOFUNKNOWNTYPE);

        }

        /*
         * messageLength is the length from (tag, length, value).
         * We have to allocate space for the tag and length bits too.
         */
        PKIX_CHECK(pkix_pl_LdapResponse_Create
                (messageType,
                messageLength + dataIndex + 2,
                client->currentBytesAvailable,
                msgBuf,
                &bytesProcessed,
                &(client->currentResponse),
                plContext),
                PKIX_LDAPRESPONSECREATEFAILED);

        client->currentBytesAvailable -= bytesProcessed;

        PKIX_CHECK(pkix_pl_LdapDefaultClient_RecvCheckComplete
                (client, bytesProcessed, pKeepGoing, plContext),
                PKIX_LDAPDEFAULTCLIENTRECVCHECKCOMPLETEFAILED);

cleanup:

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_RecvNonInitial
 * DESCRIPTION:
 *
 *  This function processes the contents of buffers, after the first, of a
 *  received LDAP-protocol message for the CertStore embodied in the
 *  LdapDefaultClient "client", and stores in "pKeepGoing" a flag indicating
 *  whether processing can continue without further input.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "pKeepGoing"
 *      The address at which the Boolean state machine flag is stored to
 *      indicate whether processing can continue without further input.
 *      Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_RecvNonInitial(
        PKIX_PL_LdapDefaultClient *client,
        PKIX_Boolean *pKeepGoing,
        void *plContext)
{

        PKIX_UInt32 bytesProcessed = 0;

        PKIX_ENTER
                (LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_RecvNonInitial");
        PKIX_NULLCHECK_TWO(client, pKeepGoing);

        PKIX_CHECK(pkix_pl_LdapResponse_Append
                (client->currentResponse,
                client->currentBytesAvailable,
                client->currentInPtr,
                &bytesProcessed,
                plContext),
                PKIX_LDAPRESPONSEAPPENDFAILED);

        client->currentBytesAvailable -= bytesProcessed;

        PKIX_CHECK(pkix_pl_LdapDefaultClient_RecvCheckComplete
                (client, bytesProcessed, pKeepGoing, plContext),
                PKIX_LDAPDEFAULTCLIENTRECVCHECKCOMPLETEFAILED);

cleanup:

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_Dispatch
 * DESCRIPTION:
 *
 *  This function is the state machine dispatcher for the CertStore embodied in
 *  the LdapDefaultClient pointed to by "client". Results are returned by
 *  changes to various fields in the context.
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_Dispatch(
        PKIX_PL_LdapDefaultClient *client,
        void *plContext)
{
        PKIX_UInt32 bytesTransferred = 0;
        PKIX_Boolean keepGoing = PKIX_TRUE;

        PKIX_ENTER(LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_Dispatch");
        PKIX_NULLCHECK_ONE(client);

        while (keepGoing) {
                switch (client->connectStatus) {
                case CONNECT_PENDING:
                        PKIX_CHECK
                                (pkix_pl_LdapDefaultClient_ConnectContinue
                                (client, &keepGoing, plContext),
                                PKIX_LDAPDEFAULTCLIENTCONNECTCONTINUEFAILED);
                        break;
                case CONNECTED:
                        PKIX_CHECK
                                (pkix_pl_LdapDefaultClient_Bind
                                (client, &keepGoing, plContext),
                                PKIX_LDAPDEFAULTCLIENTBINDFAILED);
                        break;
                case BIND_PENDING:
                        PKIX_CHECK
                                (pkix_pl_LdapDefaultClient_BindContinue
                                (client, &keepGoing, plContext),
                                PKIX_LDAPDEFAULTCLIENTBINDCONTINUEFAILED);
                        break;
                case BIND_RESPONSE:
                        PKIX_CHECK
                                (pkix_pl_LdapDefaultClient_BindResponse
                                (client, &keepGoing, plContext),
                                PKIX_LDAPDEFAULTCLIENTBINDRESPONSEFAILED);
                        break;
                case BIND_RESPONSE_PENDING:
                        PKIX_CHECK
                                (pkix_pl_LdapDefaultClient_BindResponseContinue
                                (client, &keepGoing, plContext),
                                PKIX_LDAPDEFAULTCLIENTBINDRESPONSECONTINUEFAILED);
                        break;
                case BOUND:
                        PKIX_CHECK
                                (pkix_pl_LdapDefaultClient_Send
                                (client, &keepGoing, &bytesTransferred, plContext),
                                PKIX_LDAPDEFAULTCLIENTSENDFAILED);
                        break;
                case SEND_PENDING:
                        PKIX_CHECK
                                (pkix_pl_LdapDefaultClient_SendContinue
                                (client, &keepGoing, &bytesTransferred, plContext),
                                PKIX_LDAPDEFAULTCLIENTSENDCONTINUEFAILED);
                        break;
                case RECV:
                        PKIX_CHECK
                                (pkix_pl_LdapDefaultClient_Recv
                                (client, &keepGoing, plContext),
                                PKIX_LDAPDEFAULTCLIENTRECVFAILED);
                        break;
                case RECV_PENDING:
                        PKIX_CHECK
                                (pkix_pl_LdapDefaultClient_RecvContinue
                                (client, &keepGoing, plContext),
                                PKIX_LDAPDEFAULTCLIENTRECVCONTINUEFAILED);
                        break;
                case RECV_INITIAL:
                        PKIX_CHECK
                                (pkix_pl_LdapDefaultClient_RecvInitial
                                (client, &keepGoing, plContext),
                                PKIX_LDAPDEFAULTCLIENTRECVINITIALFAILED);
                        break;
                case RECV_NONINITIAL:
                        PKIX_CHECK
                                (pkix_pl_LdapDefaultClient_RecvNonInitial
                                (client, &keepGoing, plContext),
                                PKIX_LDAPDEFAULTCLIENTRECVNONINITIALFAILED);
                        break;
                case ABANDON_PENDING:
                        PKIX_CHECK
                                (pkix_pl_LdapDefaultClient_AbandonContinue
                                (client, &keepGoing, plContext),
                                PKIX_LDAPDEFAULTCLIENTABANDONCONTINUEFAILED);
                        break;
                default:
                        PKIX_ERROR(PKIX_LDAPCERTSTOREINILLEGALSTATE);
                }
        }

cleanup:

        PKIX_RETURN(LDAPDEFAULTCLIENT);
}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_MakeAndFilter
 * DESCRIPTION:
 *
 *  This function allocates space from the arena pointed to by "arena" to
 *  construct a filter that will match components of the X500Name pointed to by
 *  XXX...
 *
 * PARAMETERS:
 *  "arena"
 *      The address of the PLArenaPool used in creating the filter. Must be
 *       non-NULL.
 *  "nameComponent"
 *      The address of a NULL-terminated list of LDAPNameComponents
 *      Must be non-NULL.
 *  "pFilter"
 *      The address at which the result is stored.
 *  "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a CertStore Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_MakeAndFilter(
        PLArenaPool *arena,
        LDAPNameComponent **nameComponents,
        LDAPFilter **pFilter,
        void *plContext)
{
        LDAPFilter **setOfFilter;
        LDAPFilter *andFilter = NULL;
        LDAPFilter *currentFilter = NULL;
        PKIX_UInt32 componentsPresent = 0;
        void *v = NULL;
        unsigned char *component = NULL;
        LDAPNameComponent **componentP = NULL;

        PKIX_ENTER(CERTSTORE, "pkix_pl_LdapDefaultClient_MakeAndFilter");
        PKIX_NULLCHECK_THREE(arena, nameComponents, pFilter);

        /* count how many components we were provided */
        for (componentP = nameComponents, componentsPresent = 0;
                *(componentP++) != NULL;
                componentsPresent++) {}

        /* Space for (componentsPresent + 1) pointers to LDAPFilter */
        PKIX_PL_NSSCALLRV(CERTSTORE, v, PORT_ArenaZAlloc,
                (arena, (componentsPresent + 1)*sizeof(LDAPFilter *)));
        setOfFilter = (LDAPFilter **)v;

        /* Space for AndFilter and <componentsPresent> EqualFilters */
        PKIX_PL_NSSCALLRV(CERTSTORE, v, PORT_ArenaZNewArray,
                (arena, LDAPFilter, componentsPresent + 1));
        setOfFilter[0] = (LDAPFilter *)v;

        /* Claim the first array element for the ANDFilter */
        andFilter = setOfFilter[0];

        /* Set ANDFilter to point to the first EqualFilter pointer */
        andFilter->selector = LDAP_ANDFILTER_TYPE;
        andFilter->filter.andFilter.filters = setOfFilter;

        currentFilter = andFilter + 1;

        for (componentP = nameComponents, componentsPresent = 0;
                *(componentP) != NULL; componentP++) {
                setOfFilter[componentsPresent++] = currentFilter;
                currentFilter->selector = LDAP_EQUALFILTER_TYPE;
                component = (*componentP)->attrType;
                currentFilter->filter.equalFilter.attrType.data = component;
                currentFilter->filter.equalFilter.attrType.len = 
                        PL_strlen((const char *)component);
                component = (*componentP)->attrValue;
                currentFilter->filter.equalFilter.attrValue.data = component;
                currentFilter->filter.equalFilter.attrValue.len =
                        PL_strlen((const char *)component);
                currentFilter++;
        }

        setOfFilter[componentsPresent] = NULL;

        *pFilter = andFilter;

        PKIX_RETURN(CERTSTORE);

}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_InitiateRequest
 * DESCRIPTION:
 *
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "requestParams"
 *      The address of an LdapClientParams object. Must be non-NULL.
 *  "pPollDesc"
 *      The location where the address of the PRPollDesc is stored, if the
 *      client returns with I/O pending.
 *  "pResponse"
 *      The address where the List of LDAPResponses, or NULL for an
 *      unfinished request, is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_InitiateRequest(
        PKIX_PL_LdapClient *genericClient,
        LDAPRequestParams *requestParams,
        void **pPollDesc,
        PKIX_List **pResponse,
        void *plContext)
{
        PKIX_List *searchResponseList = NULL;
        SECItem *encoded = NULL;
        LDAPFilter *filter = NULL;
        PKIX_PL_LdapDefaultClient *client = 0;

        PKIX_ENTER
                (LDAPDEFAULTCLIENT,
                "pkix_pl_LdapDefaultClient_InitiateRequest");
        PKIX_NULLCHECK_FOUR(genericClient, requestParams, pPollDesc, pResponse);

        PKIX_CHECK(pkix_CheckType
                ((PKIX_PL_Object *)genericClient,
                PKIX_LDAPDEFAULTCLIENT_TYPE,
                plContext),
                PKIX_GENERICCLIENTNOTANLDAPDEFAULTCLIENT);

        client = (PKIX_PL_LdapDefaultClient *)genericClient;

        PKIX_CHECK(pkix_pl_LdapDefaultClient_MakeAndFilter
                (client->arena, requestParams->nc, &filter, plContext),
                PKIX_LDAPDEFAULTCLIENTMAKEANDFILTERFAILED);

        PKIX_CHECK(pkix_pl_LdapRequest_Create
                (client->arena,
                client->messageID++,
                requestParams->baseObject,
                requestParams->scope,
                requestParams->derefAliases,
                requestParams->sizeLimit,
                requestParams->timeLimit,
                PKIX_FALSE,    /* attrs only */
                filter,
                requestParams->attributes,
                &client->currentRequest,
                plContext),
                PKIX_LDAPREQUESTCREATEFAILED);

        /* check hashtable for matching request */
        PKIX_CHECK(PKIX_PL_HashTable_Lookup
                (client->cachePtr,
                (PKIX_PL_Object *)(client->currentRequest),
                (PKIX_PL_Object **)&searchResponseList,
                plContext),
                PKIX_HASHTABLELOOKUPFAILED);

        if (searchResponseList != NULL) {
                *pPollDesc = NULL;
                *pResponse = searchResponseList;
                PKIX_DECREF(client->currentRequest);
                goto cleanup;
        }

        /* It wasn't cached. We'll have to actually send it. */

        PKIX_CHECK(pkix_pl_LdapRequest_GetEncoded
                (client->currentRequest, &encoded, plContext),
                PKIX_LDAPREQUESTGETENCODEDFAILED);

        client->sendBuf = encoded->data;
        client->bytesToWrite = encoded->len;

        PKIX_CHECK(pkix_pl_LdapDefaultClient_Dispatch(client, plContext),
                PKIX_LDAPDEFAULTCLIENTDISPATCHFAILED);

        /*
         * It's not enough that we may be done with a particular read.
         * We're still processing the transaction until we've gotten the
         * SearchResponseResult message and returned to the BOUND state.
         * Otherwise we must still have a read pending, and must hold off
         * on returning results.
         */
        if ((client->connectStatus == BOUND) &&
	    (client->entriesFound != NULL)) {
                *pPollDesc = NULL;
                *pResponse = client->entriesFound;
                client->entriesFound = NULL;
                PKIX_DECREF(client->currentRequest);
        } else {
                *pPollDesc = &client->pollDesc;
                *pResponse = NULL;
        }

cleanup:

        PKIX_RETURN(LDAPDEFAULTCLIENT);

}

/*
 * FUNCTION: pkix_pl_LdapDefaultClient_ResumeRequest
 * DESCRIPTION:
 *
 *
 * PARAMETERS:
 *  "client"
 *      The address of the LdapDefaultClient object. Must be non-NULL.
 *  "pPollDesc"
 *      The location where the address of the PRPollDesc is stored, if the
 *      client returns with I/O pending.
 *  "pResponse"
 *      The address where the List of LDAPResponses, or NULL for an
 *      unfinished request, is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a LdapDefaultClient Error if the function fails in a
 *      non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapDefaultClient_ResumeRequest(
        PKIX_PL_LdapClient *genericClient,
        void **pPollDesc,
        PKIX_List **pResponse,
        void *plContext)
{
        PKIX_PL_LdapDefaultClient *client = 0;

        PKIX_ENTER
                (LDAPDEFAULTCLIENT, "pkix_pl_LdapDefaultClient_ResumeRequest");
        PKIX_NULLCHECK_THREE(genericClient, pPollDesc, pResponse);

        PKIX_CHECK(pkix_CheckType
                ((PKIX_PL_Object *)genericClient,
                PKIX_LDAPDEFAULTCLIENT_TYPE,
                plContext),
                PKIX_GENERICCLIENTNOTANLDAPDEFAULTCLIENT);

        client = (PKIX_PL_LdapDefaultClient *)genericClient;

        PKIX_CHECK(pkix_pl_LdapDefaultClient_Dispatch(client, plContext),
                PKIX_LDAPDEFAULTCLIENTDISPATCHFAILED);

        /*
         * It's not enough that we may be done with a particular read.
         * We're still processing the transaction until we've gotten the
         * SearchResponseResult message and returned to the BOUND state.
         * Otherwise we must still have a read pending, and must hold off
         * on returning results.
         */
        if ((client->connectStatus == BOUND) &&
	    (client->entriesFound != NULL)) {
                *pPollDesc = NULL;
                *pResponse = client->entriesFound;
                client->entriesFound = NULL;
                PKIX_DECREF(client->currentRequest);
        } else {
                *pPollDesc = &client->pollDesc;
                *pResponse = NULL;
        }

cleanup:

        PKIX_RETURN(LDAPDEFAULTCLIENT);

}

/* --Public-LdapDefaultClient-Functions----------------------------------- */

/*
 * FUNCTION: PKIX_PL_LdapDefaultClient_AbandonRequest
 * DESCRIPTION:
 *
 *  This function creates and sends an LDAP-protocol "Abandon" message to the 
 *  server connected to the LdapDefaultClient pointed to by "client".
 *
 * PARAMETERS:
 *  "client"
 *      The LdapDefaultClient whose connection is to be abandoned. Must be
 *      non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
PKIX_PL_LdapDefaultClient_AbandonRequest(
        PKIX_PL_LdapDefaultClient *client,
        void *plContext)
{
        PKIX_Int32 bytesWritten = 0;
        PKIX_PL_Socket_Callback *callbackList = NULL;
        SECItem *encoded = NULL;

        PKIX_ENTER(CERTSTORE, "PKIX_PL_LdapDefaultClient_AbandonRequest");
        PKIX_NULLCHECK_ONE(client);

        if (client->connectStatus == RECV_PENDING) {
                PKIX_CHECK(pkix_pl_LdapDefaultClient_MakeAbandon
                        (client->arena,
                        (client->messageID) - 1,
                        &encoded,
                        plContext),
                        PKIX_LDAPDEFAULTCLIENTMAKEABANDONFAILED);

                callbackList = (PKIX_PL_Socket_Callback *)(client->callbackList);
                PKIX_CHECK(callbackList->sendCallback
                        (client->clientSocket,
                        encoded->data,
                        encoded->len,
                        &bytesWritten,
                        plContext),
                        PKIX_SOCKETSENDFAILED);

                if (bytesWritten < 0) {
                        client->connectStatus = ABANDON_PENDING;
                } else {
                        client->connectStatus = BOUND;
                }
        }

        PKIX_DECREF(client->entriesFound);
        PKIX_DECREF(client->currentRequest);
        PKIX_DECREF(client->currentResponse);

cleanup:

        PKIX_DECREF(client);

        PKIX_RETURN(CERTSTORE);
}
