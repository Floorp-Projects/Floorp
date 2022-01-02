/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_socket.c
 *
 * Socket Function Definitions
 *
 */

/*
 * If Socket Tracing is active, messages sent and received will be
 * timestamped and dumped (to stdout) in standard hex-dump format. E.g.,
 *
 * 1116612359156140:
 * 28F0: 48 65 6C 6C 6F 2C 20 77   6F 72 6C 64 21 00    Hello, world!.
 *
 * The timestamp is not formatted to be meaningful except as an increasing
 * value of seconds.microseconds, which is good enough to correlate two
 * sides of a message exchange and to figure durations.
 *
 * Code to perform Socket tracing will be compiled in if PKIX_SOCKETTRACE
 * is defined, but that doesn't mean socket tracing is active. Tracing also
 * requires that the Boolean socketTraceFlag is set to PKIX_TRUE. That is
 * the default value, but it can be overridden by using the debugger to
 * change its value -- allowing tracing to be turned on and off at various
 * breakpoints -- or by setting the environment variable SOCKETTRACE. A
 * value of 1 sets socketTraceFlag to PKIX_TRUE (tracing on), and any other
 * value sets socketTraceFlag to PKIX_FALSE (tracing off). The environment
 * value is checked during system initialization.
 */
#ifndef BUILD_OPT
#define PKIX_SOCKETTRACE 1
#endif

#ifdef PKIX_SOCKETDEBUG
#define PKIX_SOCKETTRACE 1
#endif

#include "pkix_pl_socket.h"

/* --Private-Socket-Functions---------------------------------- */

#ifdef PKIX_SOCKETTRACE
static PKIX_Boolean socketTraceFlag = PKIX_FALSE;

/*
 * FUNCTION: pkix_pl_socket_timestamp
 * DESCRIPTION:
 *
 *  This functions prints to stdout the time of day, as obtained from the
 *  system function gettimeofday, as seconds.microseconds. Its resolution
 *  is whatever the system call provides.
 *
 * PARAMETERS:
 *  none
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static void pkix_pl_socket_timestamp() {
        PRInt64 prTime;
        prTime = PR_Now();
/* We shouldn't use PR_ALTERNATE_INT64_TYPEDEF, but nor can we use PRId64 */
#if PR_BYTES_PER_LONG == 8 && !defined(PR_ALTERNATE_INT64_TYPEDEF)
        printf("%ld:\n", prTime);
#else
        printf("%lld:\n", prTime);
#endif
}

/*
 * FUNCTION: pkix_pl_socket_hexDigit
 * DESCRIPTION:
 *
 *  This functions prints to stdout the byte "byteVal" as two hex digits.
 *
 * PARAMETERS:
 *  "byteVal"
 *      The value to be printed.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static void pkix_pl_socket_hexDigit(char byteVal) {
        int n = 0;
        char cHi = '\0';
        char cLow = '\0';
        n = ((byteVal >> 4) & 0xf);
        if (n > 9) {
                cHi = (char) ((n - 10) + 'A');
        } else {
                cHi = (char) (n + '0');
        }
        n = byteVal & 0xf;
        if (n > 9) {
                cLow = (char) ((n - 10) + 'A');
        } else {
                cLow = (char) (n + '0');
        }
        (void) printf("%c%c", cHi, cLow);
}

/*
 * FUNCTION: pkix_pl_socket_linePrefix
 * DESCRIPTION:
 *
 *  This functions prints to stdout the address provided by "addr" as four
 *  hexadecimal digits followed by a colon and a space.
 *
 * PARAMETERS:
 *  "addr"
 *      The address to be printed
 *  none
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static void pkix_pl_socket_linePrefix(PKIX_UInt32 addr) {
        pkix_pl_socket_hexDigit((char)((addr >> 8) & 0xff));
        pkix_pl_socket_hexDigit((char)(addr & 0xff));
        (void) printf(": ");
}

/*
 * FUNCTION: pkix_pl_socket_traceLine
 * DESCRIPTION:
 *
 *  This functions prints to stdout the sixteen bytes beginning at the
 *  address pointed to by "ptr". The bytes are printed as sixteen pairs
 *  of hexadecimal characters followed by an ascii interpretation, in which
 *  characters from 0x20 to 0x7d are shown as their ascii equivalents, and
 *  other values are represented as periods.
 *
 * PARAMETERS:
 *  "ptr"
 *      The address of the first of the bytes to be printed
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static void pkix_pl_socket_traceLine(char *ptr) {
        PKIX_UInt32 i = 0;
        pkix_pl_socket_linePrefix((PKIX_UInt32)((char *)ptr - (char *)NULL));
        for (i = 0; i < 16; i++) {
                printf(" ");
                pkix_pl_socket_hexDigit(ptr[i]);
                if (i == 7) {
                        printf("  ");
                }
        }
        printf("  ");
        for (i = 0; i < 16; i++) {
                if ((ptr[i] < ' ') || (ptr[i] > '}')) {
                        printf(".");
                } else {
                        printf("%c", ptr[i]);
                }
        }
        printf("\n");
}

/*
 * FUNCTION: pkix_pl_socket_tracePartialLine
 * DESCRIPTION:
 *
 *  This functions prints to stdout the number of bytes given by "nBytes",
 *  beginning at the address pointed to by "ptr". The bytes are printed as
 *  pairs of hexadecimal characters followed by an ascii interpretation, in
 *  which characters from 0x20 to 0x7d are shown as their ascii equivalents,
 *  and other values are represented as periods.
 *
 * PARAMETERS:
 *  "ptr"
 *      The address of the first of the bytes to be printed
 *  "nBytes"
 *      The Int32 value giving the number of bytes to be printed. If "nBytes"
 *      is greater than sixteen, the results will be unattractive.
 *  none
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static void pkix_pl_socket_tracePartialLine(char *ptr, PKIX_UInt32 nBytes) {
        PKIX_UInt32 i = 0;
        if (nBytes > 0) {
                pkix_pl_socket_linePrefix((PKIX_UInt32)((char *)ptr - (char *)NULL));
        }
        for (i = 0; i < nBytes; i++) {
                printf(" ");
                pkix_pl_socket_hexDigit(ptr[i]);
                if (i == 7) {
                        printf("  ");
                }
        }
        for (i = nBytes; i < 16; i++) {
                printf("   ");
                if (i == 7) {
                        printf("  ");
                }
        }
        printf("  ");
        for (i = 0; i < nBytes; i++) {
                if ((ptr[i] < ' ') || (ptr[i] > '}')) {
                        printf(".");
                } else {
                        printf("%c", ptr[i]);
                }
        }
        printf("\n");
}

/*
 * FUNCTION: pkix_pl_socket_tracebuff
 * DESCRIPTION:
 *
 *  This functions prints to stdout the number of bytes given by "nBytes",
 *  beginning with the byte pointed to by "buf". The output is preceded by
 *  a timestamp, and each group of sixteen (and a remainder, if any) is
 *  preceded by its address. The contents are shown in hexadecimal and as
 *  ascii characters. If "nBytes" is zero, the timestamp and starting
 *  address are displayed.
 *
 * PARAMETERS:
 *  "buf"
 *      The starting address of the bytes to be printed
 *  "nBytes"
 *      The number of bytes to be printed
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
void pkix_pl_socket_tracebuff(void *buf, PKIX_UInt32 nBytes) {
        PKIX_UInt32 bytesRemaining = nBytes;
        PKIX_UInt32 offset = 0;
        char *bufptr = (char *)buf;

        if (socketTraceFlag == PKIX_FALSE) return;

        pkix_pl_socket_timestamp();
        /*
         * Special case: if called with length of zero, just do address
         */
        if (nBytes == 0) {
                pkix_pl_socket_linePrefix((PKIX_UInt32)((char *)buf - (char *)NULL));
                printf("\n");
        } else {
                while (bytesRemaining >= 16) {
                        pkix_pl_socket_traceLine(&bufptr[offset]);
                        bytesRemaining -= 16;
                        offset += 16;
                }
                pkix_pl_socket_tracePartialLine
                        (&bufptr[offset], bytesRemaining);
        }
}

#endif

/*
 * FUNCTION: pkix_pl_Socket_SetNonBlocking
 * DESCRIPTION:
 *
 *  This functions sets the socket represented by the PRFileDesc "fileDesc"
 *  to nonblocking mode.
 *
 * PARAMETERS:
 *  "fileDesc"
 *      The address of the PRFileDesc whose I/O mode is to be set
 *      non-blocking. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static PKIX_Error *
pkix_pl_Socket_SetNonBlocking(
        PRFileDesc *fileDesc,
        void *plContext)
{
        PRStatus rv = PR_FAILURE;
        PRSocketOptionData sockOptionData;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_SetNonBlocking");
        PKIX_NULLCHECK_ONE(fileDesc);

        sockOptionData.option = PR_SockOpt_Nonblocking;
        sockOptionData.value.non_blocking = PR_TRUE;

        PKIX_PL_NSSCALLRV(SOCKET, rv, fileDesc->methods->setsocketoption,
                (fileDesc, &sockOptionData));

        if (rv != PR_SUCCESS) {
                PKIX_ERROR(PKIX_UNABLETOSETSOCKETTONONBLOCKING);
        }
cleanup:

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_CreateClient
 * DESCRIPTION:
 *
 *  This functions creates a client socket for the PKIX_PL_Socket pointed to
 *  by "socket". If "socket" was created with a timeout value of zero, the
 *  client socket is set to use nonblocking I/O.
 *
 * PARAMETERS:
 *  "socket"
 *      The address of the Socket for which a client socket is to be
 *      created. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */

static PKIX_Error *
pkix_pl_Socket_CreateClient(
        PKIX_PL_Socket *socket,
        void *plContext)
{
#ifdef PKIX_SOCKETDEBUG
        PRErrorCode errorcode = 0;
#endif
        PRFileDesc *mySock = NULL;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_CreateClient");
        PKIX_NULLCHECK_ONE(socket);

        PKIX_PL_NSSCALLRV(SOCKET, mySock, PR_NewTCPSocket, ());
        if (!mySock) {
#ifdef PKIX_SOCKETDEBUG
                errorcode = PR_GetError();
                printf
                        ("pkix_pl_Socket_CreateClient: %s\n",
                        PR_ErrorToString(errorcode, PR_LANGUAGE_EN));
#endif
                PKIX_ERROR(PKIX_PRNEWTCPSOCKETFAILED);
        }

#ifdef PKIX_SOCKETDEBUG
        printf("Created socket, PRFileDesc @  %#X\n", mySock);
#endif

        socket->clientSock = mySock;
        socket->status = SOCKET_UNCONNECTED;
        if (socket->timeout == 0) {
                PKIX_CHECK(pkix_pl_Socket_SetNonBlocking(mySock, plContext),
                        PKIX_SOCKETSETNONBLOCKINGFAILED);
        }

cleanup:

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_CreateServer
 * DESCRIPTION:
 *
 *  This functions creates a server socket for the PKIX_PL_Socket pointed to
 *  by "socket". If "socket" was created with a timeout value of zero, the
 *  server socket is set to use nonblocking I/O.
 *
 *  Warning: there seems to be a problem with operating a server socket in
 *  non-blocking mode. If the server calls Recv prior to a corresponding
 *  Send, the message may be lost.
 *
 * PARAMETERS:
 *  "socket"
 *      The address of the Socket for which a server socket is to be
 *      created. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static PKIX_Error *
pkix_pl_Socket_CreateServer(
        PKIX_PL_Socket *socket,
        void *plContext)
{
/* #ifdef PKIX_SOCKETDEBUG */
        PRErrorCode errorcode = 0;
/* #endif */
        PRStatus rv = PR_FAILURE;
        PRFileDesc *serverSock = NULL;
        PRSocketOptionData sockOptionData;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_CreateServer");
        PKIX_NULLCHECK_ONE(socket);

        PKIX_PL_NSSCALLRV(SOCKET, serverSock, PR_NewTCPSocket, ());
        if (!serverSock) {
#ifdef PKIX_SOCKETDEBUG
                errorcode = PR_GetError();
                printf
                        ("pkix_pl_Socket_CreateServer: %s\n",
                        PR_ErrorToString(errorcode, PR_LANGUAGE_EN));
#endif
                PKIX_ERROR(PKIX_PRNEWTCPSOCKETFAILED);
        }

        socket->serverSock = serverSock;

#ifdef PKIX_SOCKETDEBUG
        printf("Created socket, PRFileDesc @  %#X\n", serverSock);
#endif

        if (socket->timeout == 0) {
                PKIX_CHECK(pkix_pl_Socket_SetNonBlocking(serverSock, plContext),
                        PKIX_SOCKETSETNONBLOCKINGFAILED);
        }

        sockOptionData.option = PR_SockOpt_Reuseaddr;
        sockOptionData.value.reuse_addr = PR_TRUE;

        PKIX_PL_NSSCALLRV(SOCKET, rv, serverSock->methods->setsocketoption,
                (serverSock, &sockOptionData));

        if (rv != PR_SUCCESS) {
                PKIX_ERROR(PKIX_UNABLETOSETSOCKETTONONBLOCKING);
        }

        PKIX_PL_NSSCALLRV(SOCKET, rv, PR_Bind, (serverSock, socket->netAddr));

        if (rv == PR_FAILURE) {
/* #ifdef PKIX_SOCKETDEBUG */
                errorcode = PR_GetError();
                printf
                        ("pkix_pl_Socket_CreateServer: %s\n",
                        PR_ErrorToString(errorcode, PR_LANGUAGE_EN));
/* #endif */
                PKIX_ERROR(PKIX_PRBINDFAILED);
        }

#ifdef PKIX_SOCKETDEBUG
        printf("Successful bind!\n");
#endif

        socket->status = SOCKET_BOUND;

cleanup:

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_Connect
 * DESCRIPTION:
 *
 *  This functions performs the connect function for the client socket
 *  specified in "socket", storing the status at "pStatus".
 *
 * PARAMETERS:
 *  "socket"
 *      The address of the Socket for which a connect is to be performed.
 *      Must be non-NULL.
 *  "pStatus"
 *      The address at which the connection status is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static PKIX_Error *
pkix_pl_Socket_Connect(
        PKIX_PL_Socket *socket,
        PRErrorCode *pStatus,
        void *plContext)
{
        PRStatus rv = PR_FAILURE;
        PRErrorCode errorcode = 0;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_Connect");
        PKIX_NULLCHECK_TWO(socket, socket->clientSock);

        PKIX_PL_NSSCALLRV(SOCKET, rv, PR_Connect,
                (socket->clientSock, socket->netAddr, socket->timeout));

        if (rv == PR_FAILURE) {
                errorcode = PR_GetError();
                *pStatus = errorcode;
                if (errorcode == PR_IN_PROGRESS_ERROR) {
                        socket->status = SOCKET_CONNECTPENDING;
                        goto cleanup;
                } else {
#ifdef PKIX_SOCKETDEBUG
                        printf
                                ("pkix_pl_Socket_Connect: %s\n",
                                PR_ErrorToString(errorcode, PR_LANGUAGE_EN));
#endif
                        PKIX_ERROR(PKIX_PRCONNECTFAILED);
                }
        }

#ifdef PKIX_SOCKETDEBUG
        printf("Successful connect!\n");
#endif

        *pStatus = 0;
        socket->status = SOCKET_CONNECTED;

cleanup:

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_ConnectContinue
 * DESCRIPTION:
 *
 *  This functions continues the connect function for the client socket
 *  specified in "socket", storing the status at "pStatus". It is expected that
 *  the non-blocking connect has returned PR_IN_PROGRESS_ERROR.
 *
 * PARAMETERS:
 *  "socket"
 *      The address of the Socket for which a connect is to be continued.
 *      Must be non-NULL.
 *  "pStatus"
 *      The address at which the connection status is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static PKIX_Error *
pkix_pl_Socket_ConnectContinue(
        PKIX_PL_Socket *socket,
        PRErrorCode *pStatus,
        void *plContext)
{
        PRStatus rv = PR_FAILURE;
        PRErrorCode errorcode = 0;
        PRPollDesc pollDesc;
        PRInt32 numEvents = 0;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_ConnectContinue");
        PKIX_NULLCHECK_TWO(socket, socket->clientSock);

        pollDesc.fd = socket->clientSock;
        pollDesc.in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
        pollDesc.out_flags = 0;
        PKIX_PL_NSSCALLRV(SOCKET, numEvents, PR_Poll, (&pollDesc, 1, 0));
        if (numEvents < 0) {
                PKIX_ERROR(PKIX_PRPOLLFAILED);
        }

        if (numEvents == 0) {
                *pStatus = PR_IN_PROGRESS_ERROR;
                goto cleanup;
        }

        PKIX_PL_NSSCALLRV(SOCKET, rv, PR_ConnectContinue,
                (socket->clientSock, pollDesc.out_flags));

        /*
         * PR_ConnectContinue sometimes lies. It returns PR_SUCCESS
         * even though the connection is not yet ready. But its deceit
         * is betrayed by the contents of out_flags!
         */
        if ((rv == PR_SUCCESS) && (pollDesc.out_flags == PR_POLL_ERR)) {
                *pStatus = PR_IN_PROGRESS_ERROR;
                goto cleanup;
        }

        if (rv == PR_FAILURE) {
                errorcode = PR_GetError();
                *pStatus = errorcode;
                if (errorcode == PR_IN_PROGRESS_ERROR) {
                        goto cleanup;
                } else {
#ifdef PKIX_SOCKETDEBUG
                        printf
                                ("pkix_pl_Socket_ConnectContinue: %s\n",
                                PR_ErrorToString(errorcode, PR_LANGUAGE_EN));
#endif
                        PKIX_ERROR(PKIX_PRCONNECTCONTINUEFAILED);
                }
        }

#ifdef PKIX_SOCKETDEBUG
        printf("Successful connect!\n");
#endif

        *pStatus = 0;
        socket->status = SOCKET_CONNECTED;

cleanup:

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_Socket_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_Socket *socket = NULL;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType
                    (object, PKIX_SOCKET_TYPE, plContext),
                    PKIX_OBJECTNOTANSOCKET);

        socket = (PKIX_PL_Socket *)object;

        if (socket->isServer) {
                if (socket->serverSock) {
                        PR_Close(socket->serverSock);
                }
        } else {
                if (socket->clientSock) {
                        PR_Close(socket->clientSock);
                }
        }

cleanup:

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_Socket_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PL_Socket *socket = NULL;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_SOCKET_TYPE, plContext),
                PKIX_OBJECTNOTSOCKET);

        socket = (PKIX_PL_Socket *)object;

        *pHashcode = (((socket->timeout << 3) +
                 (socket->netAddr->inet.family << 3)) +
                 (*((PKIX_UInt32 *)&(socket->netAddr->inet.ip)))) +
                 socket->netAddr->inet.port;

cleanup:

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_Equals
 * (see comments for PKIX_PL_EqualsCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_Socket_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Int32 *pResult,
        void *plContext)
{
        PKIX_PL_Socket *firstSocket = NULL;
        PKIX_PL_Socket *secondSocket = NULL;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        *pResult = PKIX_FALSE;

        PKIX_CHECK(pkix_CheckTypes
                (firstObject, secondObject, PKIX_SOCKET_TYPE, plContext),
                PKIX_OBJECTNOTSOCKET);

        firstSocket = (PKIX_PL_Socket *)firstObject;
        secondSocket = (PKIX_PL_Socket *)secondObject;

        if (firstSocket->timeout != secondSocket->timeout) {
                goto cleanup;
        }

        if (firstSocket->netAddr == secondSocket->netAddr) {
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        if ((firstSocket->netAddr->inet.family !=
                secondSocket->netAddr->inet.family) ||
            (*((PKIX_UInt32 *)&(firstSocket->netAddr->inet.ip)) !=
                *((PKIX_UInt32 *)&(secondSocket->netAddr->inet.ip))) ||
            (firstSocket->netAddr->inet.port !=
                secondSocket->netAddr->inet.port)) {

                goto cleanup;

        }

        *pResult = PKIX_TRUE;

cleanup:

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_RegisterSelf
 *
 * DESCRIPTION:
 *  Registers PKIX_PL_SOCKET_TYPE and its related
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
pkix_pl_Socket_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_RegisterSelf");

        entry.description = "Socket";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_Socket);
        entry.destructor = pkix_pl_Socket_Destroy;
        entry.equalsFunction = pkix_pl_Socket_Equals;
        entry.hashcodeFunction = pkix_pl_Socket_Hashcode;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = NULL;

        systemClasses[PKIX_SOCKET_TYPE] = entry;

#ifdef PKIX_SOCKETTRACE
        {
                char *val = NULL;
                val = PR_GetEnvSecure("SOCKETTRACE");
                /* Is SOCKETTRACE set in the environment? */
                if ((val != NULL) && (*val != '\0')) {
                        socketTraceFlag =
                                ((*val == '1')?PKIX_TRUE:PKIX_FALSE);
                }
        }
#endif

        PKIX_RETURN(SOCKET);
}

/* --Public-Socket-Functions----------------------------------- */

/*
 * FUNCTION: pkix_pl_Socket_Listen
 * DESCRIPTION:
 *
 *  This functions establishes a listening queue for the server Socket
 *  pointed to by "socket".
 *
 * PARAMETERS:
 *  "socket"
 *      The address of the server socket for which the queue is to be
 *      established. Must be non-NULL.
 *  "backlog"
 *      The UInt32 value of the length of the queue to be established.
 *  "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static PKIX_Error *
pkix_pl_Socket_Listen(
        PKIX_PL_Socket *socket,
        PKIX_UInt32 backlog,
        void *plContext)
{
#ifdef PKIX_SOCKETDEBUG
        PRErrorCode errorcode = 0;
#endif
        PRStatus rv = PR_FAILURE;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_Listen");
        PKIX_NULLCHECK_TWO(socket, socket->serverSock);

        PKIX_PL_NSSCALLRV(SOCKET, rv, PR_Listen,
                (socket->serverSock, (PRIntn)backlog));

        if (rv == PR_FAILURE) {
#ifdef PKIX_SOCKETDEBUG
                errorcode = PR_GetError();
                printf
                        ("pkix_pl_Socket_Listen: %s\n",
                        PR_ErrorToString(errorcode, PR_LANGUAGE_EN));
#endif
                PKIX_ERROR(PKIX_PRLISTENFAILED);
        }

#ifdef PKIX_SOCKETDEBUG
        printf("Successful listen!\n");
#endif

        socket->status = SOCKET_LISTENING;
cleanup:

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_Shutdown
 * DESCRIPTION:
 *
 *  This functions performs the shutdown of any connections controlled by the
 *  socket pointed to by "socket".
 *
 * PARAMETERS:
 *  "socket"
 *      The address of the socket to be shut down. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static PKIX_Error *
pkix_pl_Socket_Shutdown(
        PKIX_PL_Socket *socket,
        void *plContext)
{
#ifdef PKIX_SOCKETDEBUG
        PRErrorCode errorcode = 0;
#endif
        PRStatus rv = PR_FAILURE;
        PRFileDesc *fileDesc = NULL;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_Shutdown");
        PKIX_NULLCHECK_ONE(socket);

        fileDesc =
                (socket->isServer)?(socket->serverSock):(socket->clientSock);

        PKIX_PL_NSSCALLRV(SOCKET, rv, PR_Shutdown,
                (fileDesc, PR_SHUTDOWN_BOTH));

        if (rv == PR_FAILURE) {
#ifdef PKIX_SOCKETDEBUG
                errorcode = PR_GetError();
                printf
                        ("pkix_pl_Socket_Shutdown: %s\n",
                        PR_ErrorToString(errorcode, PR_LANGUAGE_EN));
#endif
                PKIX_ERROR(PKIX_PRSHUTDOWNFAILED);
        }
        socket->status = SOCKET_SHUTDOWN;

cleanup:

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_Send
 * DESCRIPTION:
 *
 *  This functions sends a message using the socket pointed to by "sendSock",
 *  from the buffer pointed to by "buf", of the number of bytes given by
 *  "bytesToWrite", storing the number of bytes actually written at
 *  "pBytesWritten". If "socket" is in non-blocking mode, the send operation
 *  may store -1 at "pBytesWritten" and the write is not complete until a
 *  corresponding pkix_pl_Poll call has indicated its completion by returning
 *  a non-negative value for bytes written.
 *
 * PARAMETERS:
 *  "sendSock"
 *      The address of the Socket on which the message is to be sent. Must
 *      be non-NULL.
 *  "buf"
 *      The address of the data to be sent. Must be non-NULL.
 *  "bytesToWrite""
 *      The UInt32 value indicating the number of bytes to write.
 *  "pBytesWritten"
 *      The address at which the Int32 value indicating the number of bytes
 *      actually written is to be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static PKIX_Error *
pkix_pl_Socket_Send(
        PKIX_PL_Socket *sendSock,
        void *buf,
        PKIX_UInt32 bytesToWrite,
        PKIX_Int32 *pBytesWritten,
        void *plContext)
{
        PRInt32 bytesWritten = 0;
        PRErrorCode errorcode = 0;
        PRFileDesc *fd = NULL;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_Send");
        PKIX_NULLCHECK_TWO(buf, pBytesWritten);

        fd = sendSock->clientSock;

        PKIX_PL_NSSCALLRV(SOCKET, bytesWritten, PR_Send,
                (fd, buf, (PRInt32)bytesToWrite, 0, sendSock->timeout));

        if (bytesWritten >= 0) {
                if (sendSock->status == SOCKET_SENDRCVPENDING) {
                        sendSock->status = SOCKET_RCVPENDING;
                } else {
                        sendSock->status = SOCKET_CONNECTED;
                }
#ifdef PKIX_SOCKETTRACE
                pkix_pl_socket_tracebuff(buf, bytesWritten);
#endif
        } else {
                errorcode = PR_GetError();
                if (errorcode != PR_WOULD_BLOCK_ERROR) {
#ifdef PKIX_SOCKETDEBUG
                        printf
                                ("pkix_pl_Socket_Send: %s\n",
                                PR_ErrorToString(errorcode, PR_LANGUAGE_EN));
#endif
                        PKIX_ERROR(PKIX_PRSENDFAILED);
                }

                sendSock->writeBuf = buf;
                sendSock->writeBufSize = bytesToWrite;
                if (sendSock->status == SOCKET_RCVPENDING) {
                        sendSock->status = SOCKET_SENDRCVPENDING;
                } else {
                        sendSock->status = SOCKET_SENDPENDING;
                }
        }

        *pBytesWritten = (PKIX_Int32)bytesWritten;

cleanup:

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_Recv
 * DESCRIPTION:
 *
 *  This functions receives a message on the socket pointed to by "rcvSock",
 *  into the buffer pointed to by "buf", of capacity given by "capacity",
 *  storing the number of bytes actually received at "pBytesRead". If "socket"
 *  is in non-blocking mode, the receive operation may store -1 at
 *  "pBytesWritten". In that case the write is not complete until a
 *  corresponding pkix_pl_Poll call has indicated its completion by returning
 *  a non-negative value for bytes read.
 *
 * PARAMETERS:
 *  "rcvSock"
 *      The address of the Socket on which the message is to be received.
 *      Must be non-NULL.
 *  "buf"
 *      The address of the buffer into which the message is to be received.
 *      Must be non-NULL.
 *  "capacity"
 *      The UInt32 value of the size of the buffer; that is, the maximum
 *      number of bytes that can be received.
 *  "pBytesRead"
 *      The address at which is stored the Int32 value of the number of bytes
 *      actually received.
 *  "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static PKIX_Error *
pkix_pl_Socket_Recv(
        PKIX_PL_Socket *rcvSock,
        void *buf,
        PKIX_UInt32 capacity,
        PKIX_Int32 *pBytesRead,
        void *plContext)
{
        PRErrorCode errorcode = 0;
        PRInt32 bytesRead = 0;
        PRFileDesc *fd = NULL;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_Recv");
        PKIX_NULLCHECK_THREE(rcvSock, buf, pBytesRead);

        fd = rcvSock->clientSock;

        PKIX_PL_NSSCALLRV(SOCKET, bytesRead, PR_Recv,
                (fd, buf, (PRInt32)capacity, 0, rcvSock->timeout));

        if (bytesRead > 0) {
                if (rcvSock->status == SOCKET_SENDRCVPENDING) {
                        rcvSock->status = SOCKET_SENDPENDING;
                } else {
                        rcvSock->status = SOCKET_CONNECTED;
                }
#ifdef PKIX_SOCKETTRACE
                pkix_pl_socket_tracebuff(buf, bytesRead);
#endif
        } else if (bytesRead == 0) {
                PKIX_ERROR(PKIX_PRRECVREPORTSNETWORKCONNECTIONCLOSED);
        } else {
                errorcode = PR_GetError();
                if (errorcode != PR_WOULD_BLOCK_ERROR) {
#ifdef PKIX_SOCKETDEBUG
                        printf
                                ("pkix_pl_Socket_Recv: %s\n",
                                PR_ErrorToString(errorcode, PR_LANGUAGE_EN));
#endif
                        PKIX_ERROR(PKIX_PRRECVFAILED);
                }
                rcvSock->readBuf = buf;
                rcvSock->readBufSize = capacity;
                if (rcvSock->status == SOCKET_SENDPENDING) {
                        rcvSock->status = SOCKET_SENDRCVPENDING;
                } else {
                        rcvSock->status = SOCKET_RCVPENDING;
                }

        }

        *pBytesRead = (PKIX_Int32)bytesRead;

cleanup:

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_Poll
 * DESCRIPTION:
 *
 *  This functions checks for completion of an earlier Send or Recv on the
 *  socket pointed to by "sock", storing in "pBytesWritten" the number of bytes
 *  written by a completed Send and in "pBytesRead" the number of bytes
 *  received in a completed Recv. A value of -1 returned indicates the
 *  operation has still not completed. A NULL pointer may be supplied for
 *  "pBytesWritten" to avoid checking for completion of a Send. A NULL pointer
 *  may be supplied for "pBytesRead" to avoid checking for completion of a Recv.
 *
 * PARAMETERS:
 *  "sock"
 *      The address of the socket for which completions are to be checked.
 *  "pBytesWritten"
 *      The address at which the number of bytes written is to be stored, if
 *      a pending Send has completed. If NULL, Sends are not checked.
 *  "pBytesRead"
 *      The address at which the number of bytes read is to be stored, if
 *      a pending Recv has completed. If NULL, Recvs are not checked.
 *  "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static PKIX_Error *
pkix_pl_Socket_Poll(
        PKIX_PL_Socket *sock,
        PKIX_Int32 *pBytesWritten,
        PKIX_Int32 *pBytesRead,
        void *plContext)
{
        PRPollDesc pollDesc;
        PRInt32 numEvents = 0;
        PKIX_Int32 bytesRead = 0;
        PKIX_Int32 bytesWritten = 0;
        PRErrorCode errorcode = 0;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_Poll");
        PKIX_NULLCHECK_ONE(sock);

        pollDesc.fd = sock->clientSock;
        pollDesc.in_flags = 0;
        pollDesc.out_flags = 0;

        if ((pBytesWritten) &&
            ((sock->status == SOCKET_SENDPENDING) ||
            (sock->status == SOCKET_SENDRCVPENDING))) {
                pollDesc.in_flags = PR_POLL_WRITE;
        }

        if ((pBytesRead) &&
            ((sock->status == SOCKET_RCVPENDING) ||
            (sock->status == SOCKET_SENDRCVPENDING))) {
                pollDesc.in_flags |= PR_POLL_READ;
        }

        PKIX_PL_NSSCALLRV(SOCKET, numEvents, PR_Poll, (&pollDesc, 1, 0));

        if (numEvents < 0) {
                PKIX_ERROR(PKIX_PRPOLLFAILED);
        } else if (numEvents > 0) {
                if (pollDesc.out_flags & PR_POLL_WRITE) {
                        PKIX_CHECK(pkix_pl_Socket_Send
                                (sock,
                                sock->writeBuf,
                                sock->writeBufSize,
                                &bytesWritten,
                                plContext),
                                PKIX_SOCKETSENDFAILED);
                        *pBytesWritten = (PKIX_Int32)bytesWritten;
                        if (bytesWritten >= 0) {
                                sock->writeBuf = NULL;
                                sock->writeBufSize = 0;
                        }
                }

                if (pollDesc.out_flags & PR_POLL_READ) {
                        PKIX_CHECK(pkix_pl_Socket_Recv
                                (sock,
                                sock->readBuf,
                                sock->readBufSize,
                                &bytesRead,
                                plContext),
                                PKIX_SOCKETRECVFAILED);
                        *pBytesRead = (PKIX_Int32)bytesRead;
                        if (bytesRead >= 0) {
                                sock->readBuf = NULL;
                                sock->readBufSize = 0;
                        }
                }
        } else if (numEvents == 0) {
                errorcode = PR_GetError();
                if (errorcode != PR_WOULD_BLOCK_ERROR) {
#ifdef PKIX_SOCKETDEBUG
                        printf
                                ("pkix_pl_Socket_Poll: %s\n",
                                PR_ErrorToString(errorcode, PR_LANGUAGE_EN));
#endif
                        PKIX_ERROR(PKIX_PRPOLLFAILED);
                }
                if (pBytesWritten) {
                        *pBytesWritten = 0;
                }
                if (pBytesRead) {
                        *pBytesRead = 0;
                }
        }

cleanup:

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_Accept
 * DESCRIPTION:
 *
 *  This functions accepts a client connection for the server Socket pointed
 *  to by "serverSocket", creating a new Socket and storing the result at
 *  "pRendezvousSocket". If "serverSocket" is in non-blocking mode, this
 *  function will return NULL if there is no client connection to accept.
 *  Otherwise this function will block until a connection is available.
 *  When a client connection is available the new Socket will have the same
 *  blocking/non-blocking property as "serverSocket".
 *
 * PARAMETERS:
 *   "serverSocket"
 *      The address of the Socket for which a client connection is to be
 *      accepted. Must be non-NULL.
 *   "pRendezvousSocket"
 *      The address at which the created Socket is stored, when a client
 *      connection is available, or at which NULL is stored, if no connection
 *      is available for a non-blocking "serverSocket". Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety definitions in Programmer's Guide)
 * RETURNS:
 *  none
 */
static PKIX_Error *
pkix_pl_Socket_Accept(
        PKIX_PL_Socket *serverSocket,
        PKIX_PL_Socket **pRendezvousSocket,
        void *plContext)
{
        PRErrorCode errorcode = 0;
        PRFileDesc *rendezvousSock = NULL;
        PRNetAddr *clientAddr = NULL;
        PKIX_PL_Socket *newSocket = NULL;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_Accept");
        PKIX_NULLCHECK_TWO(serverSocket, pRendezvousSocket);

        PKIX_PL_NSSCALLRV(SOCKET, rendezvousSock, PR_Accept,
                (serverSocket->serverSock, clientAddr, serverSocket->timeout));

        if (!rendezvousSock) {
                errorcode = PR_GetError();
                if (errorcode != PR_WOULD_BLOCK_ERROR) {
#ifdef PKIX_SOCKETDEBUG
                        printf
                                ("pkix_pl_Socket_Accept: %s\n",
                                PR_ErrorToString(errorcode, PR_LANGUAGE_EN));
#endif
                        PKIX_ERROR(PKIX_PRACCEPTFAILED);
                }
                serverSocket->status = SOCKET_ACCEPTPENDING;
                *pRendezvousSocket = NULL;
                goto cleanup;

        }

#ifdef PKIX_SOCKETDEBUG
        printf("Successful accept!\n");
#endif

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_SOCKET_TYPE,
                    sizeof (PKIX_PL_Socket),
                    (PKIX_PL_Object **)&newSocket,
                    plContext),
                    PKIX_COULDNOTCREATESOCKETOBJECT);

        newSocket->isServer = PKIX_FALSE;
        newSocket->timeout = serverSocket->timeout;
        newSocket->clientSock = rendezvousSock;
        newSocket->serverSock = NULL;
        newSocket->netAddr = NULL;
        newSocket->status = SOCKET_CONNECTED;
        newSocket->callbackList.shutdownCallback = pkix_pl_Socket_Shutdown;
        newSocket->callbackList.listenCallback = pkix_pl_Socket_Listen;
        newSocket->callbackList.acceptCallback = pkix_pl_Socket_Accept;
        newSocket->callbackList.connectcontinueCallback =
                pkix_pl_Socket_ConnectContinue;
        newSocket->callbackList.sendCallback = pkix_pl_Socket_Send;
        newSocket->callbackList.recvCallback = pkix_pl_Socket_Recv;
        newSocket->callbackList.pollCallback = pkix_pl_Socket_Poll;

        if (serverSocket->timeout == 0) {
                PKIX_CHECK(pkix_pl_Socket_SetNonBlocking
                        (rendezvousSock, plContext),
                        PKIX_SOCKETSETNONBLOCKINGFAILED);
        }

        *pRendezvousSocket = newSocket;

cleanup:

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_Create
 * DESCRIPTION:
 *
 *  This function creates a new Socket, setting it to be a server or a client
 *  according to the value of "isServer", setting its timeout value from
 *  "timeout" and server address from "netAddr", and stores the created Socket
 *  at "pSocket".
 *
 * PARAMETERS:
 *  "isServer"
 *      The Boolean value indicating if PKIX_TRUE, that a server socket (using
 *      Bind, Listen, and Accept) is to be created, or if PKIX_FALSE, that a
 *      client socket (using Connect) is to be created.
 *  "timeout"
 *      A PRTimeInterval value to be used for I/O waits for this socket. If
 *      zero, non-blocking I/O is to be used.
 *  "netAddr"
 *      The PRNetAddr to be used for the Bind function, if this is a server
 *      socket, or for the Connect, if this is a client socket.
 *  "pSocket"
 *      The address at which the Socket is to be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Socket Error if the function fails in
 *      a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_Socket_Create(
        PKIX_Boolean isServer,
        PRIntervalTime timeout,
        PRNetAddr *netAddr,
        PRErrorCode *status,
        PKIX_PL_Socket **pSocket,
        void *plContext)
{
        PKIX_PL_Socket *socket = NULL;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_Create");
        PKIX_NULLCHECK_ONE(pSocket);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_SOCKET_TYPE,
                    sizeof (PKIX_PL_Socket),
                    (PKIX_PL_Object **)&socket,
                    plContext),
                    PKIX_COULDNOTCREATESOCKETOBJECT);

        socket->isServer = isServer;
        socket->timeout = timeout;
        socket->clientSock = NULL;
        socket->serverSock = NULL;
        socket->netAddr = netAddr;

        socket->callbackList.listenCallback = pkix_pl_Socket_Listen;
        socket->callbackList.acceptCallback = pkix_pl_Socket_Accept;
        socket->callbackList.connectcontinueCallback =
                 pkix_pl_Socket_ConnectContinue;
        socket->callbackList.sendCallback = pkix_pl_Socket_Send;
        socket->callbackList.recvCallback = pkix_pl_Socket_Recv;
        socket->callbackList.pollCallback = pkix_pl_Socket_Poll;
        socket->callbackList.shutdownCallback = pkix_pl_Socket_Shutdown;

        if (isServer) {
                PKIX_CHECK(pkix_pl_Socket_CreateServer(socket, plContext),
                        PKIX_SOCKETCREATESERVERFAILED);
                *status = 0;
        } else {
                socket->timeout = timeout;
                PKIX_CHECK(pkix_pl_Socket_CreateClient(socket, plContext),
                        PKIX_SOCKETCREATECLIENTFAILED);
                PKIX_CHECK(pkix_pl_Socket_Connect(socket, status, plContext),
                        PKIX_SOCKETCONNECTFAILED);
        }

        *pSocket = socket;

cleanup:
        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(socket);
        }

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_CreateByName
 * DESCRIPTION:
 *
 *  This function creates a new Socket, setting it to be a server or a client
 *  according to the value of "isServer", setting its timeout value from
 *  "timeout" and server address and port number from "serverName", and stores
 *  the status at "pStatus" and the created Socket at "pSocket".
 *
 *  If isServer is PKIX_TRUE, it is attempted to create the socket with an ip
 *  address of PR_INADDR_ANY.
 *
 * PARAMETERS:
 *  "isServer"
 *      The Boolean value indicating if PKIX_TRUE, that a server socket (using
 *      Bind, Listen, and Accept) is to be created, or if PKIX_FALSE, that a
 *      client socket (using Connect) is to be created.
 *  "timeout"
 *      A PRTimeInterval value to be used for I/O waits for this socket. If
 *      zero, non-blocking I/O is to be used.
 *  "serverName"
 *      Address of a character string consisting of the server's domain name
 *      followed by a colon and a port number for the desired socket.
 *  "pStatus"
 *      Address at which the PRErrorCode resulting from the create is
 *      stored. Must be non-NULL.
 *  "pSocket"
 *      The address at which the Socket is to be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Socket Error if the function fails in
 *      a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_Socket_CreateByName(
        PKIX_Boolean isServer,
        PRIntervalTime timeout,
        char *serverName,
        PRErrorCode *pStatus,
        PKIX_PL_Socket **pSocket,
        void *plContext)
{
        PRNetAddr netAddr;
        PKIX_PL_Socket *socket = NULL;
        char *sepPtr = NULL;
        PRHostEnt hostent;
        PRIntn hostenum;
        PRStatus prstatus = PR_FAILURE;
        char buf[PR_NETDB_BUF_SIZE];
        PRUint16 portNum = 0;
        char *localCopyName = NULL;

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_CreateByName");
        PKIX_NULLCHECK_TWO(serverName, pSocket);

        localCopyName = PL_strdup(serverName);

        sepPtr = strchr(localCopyName, ':');
        /* First strip off the portnum, if present, from the end of the name */
        if (sepPtr) {
                *sepPtr++ = '\0';
                 portNum = (PRUint16)atoi(sepPtr);
        } else {
                 portNum = (PRUint16)LDAP_PORT;
        }

        prstatus = PR_GetHostByName(localCopyName, buf, sizeof(buf), &hostent);

        if ((prstatus != PR_SUCCESS) || (hostent.h_length != 4)) {
                /*
                 * The hostname may be a fully-qualified name. Try using just
                 * the leftmost component in our lookup.
                 */
                sepPtr = strchr(localCopyName, '.');
                if (sepPtr) {
                        *sepPtr++ = '\0';
                }
                prstatus = PR_GetHostByName
                        (localCopyName, buf, sizeof(buf), &hostent);

                if ((prstatus != PR_SUCCESS) || (hostent.h_length != 4)) {
                        PKIX_ERROR
                                (PKIX_PRGETHOSTBYNAMEREJECTSHOSTNAMEARGUMENT);
                }
        }

        netAddr.inet.family = PR_AF_INET;
        netAddr.inet.port = PR_htons(portNum);

        if (isServer) {

                netAddr.inet.ip = PR_INADDR_ANY;

        } else {

                hostenum = PR_EnumerateHostEnt(0, &hostent, portNum, &netAddr);
                if (hostenum == -1) {
                        PKIX_ERROR(PKIX_PRENUMERATEHOSTENTFAILED);
                }
        }

        PKIX_CHECK(PKIX_PL_Object_Alloc
                (PKIX_SOCKET_TYPE,
                sizeof (PKIX_PL_Socket),
                (PKIX_PL_Object **)&socket,
                plContext),
                PKIX_COULDNOTCREATESOCKETOBJECT);

        socket->isServer = isServer;
        socket->timeout = timeout;
        socket->clientSock = NULL;
        socket->serverSock = NULL;
        socket->netAddr = &netAddr;

        socket->callbackList.listenCallback = pkix_pl_Socket_Listen;
        socket->callbackList.acceptCallback = pkix_pl_Socket_Accept;
        socket->callbackList.connectcontinueCallback =
                 pkix_pl_Socket_ConnectContinue;
        socket->callbackList.sendCallback = pkix_pl_Socket_Send;
        socket->callbackList.recvCallback = pkix_pl_Socket_Recv;
        socket->callbackList.pollCallback = pkix_pl_Socket_Poll;
        socket->callbackList.shutdownCallback = pkix_pl_Socket_Shutdown;

        if (isServer) {
                PKIX_CHECK(pkix_pl_Socket_CreateServer(socket, plContext),
                        PKIX_SOCKETCREATESERVERFAILED);
                *pStatus = 0;
        } else {
                PKIX_CHECK(pkix_pl_Socket_CreateClient(socket, plContext),
                        PKIX_SOCKETCREATECLIENTFAILED);
                PKIX_CHECK(pkix_pl_Socket_Connect(socket, pStatus, plContext),
                        PKIX_SOCKETCONNECTFAILED);
        }

        *pSocket = socket;

cleanup:
        PL_strfree(localCopyName);

        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(socket);
        }

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_CreateByHostAndPort
 * DESCRIPTION:
 *
 *  This function creates a new Socket, setting it to be a server or a client
 *  according to the value of "isServer", setting its timeout value from
 *  "timeout", host from "hostname", and port number from "portNum", and stores
 *  the status at "pStatus" and the created Socket at "pSocket".
 *
 *  If isServer is PKIX_TRUE, it is attempted to create the socket with an ip
 *  address of PR_INADDR_ANY.
 *
 * PARAMETERS:
 *  "isServer"
 *      The Boolean value indicating if PKIX_TRUE, that a server socket (using
 *      Bind, Listen, and Accept) is to be created, or if PKIX_FALSE, that a
 *      client socket (using Connect) is to be created.
 *  "timeout"
 *      A PRTimeInterval value to be used for I/O waits for this socket. If
 *      zero, non-blocking I/O is to be used.
 *  "hostname"
 *      Address of a character string consisting of the server's domain name.
 *  "portNum"
 *      UInt16 value of the port number for the desired socket.
 *  "pStatus"
 *      Address at which the PRErrorCode resulting from the create is
 *      stored. Must be non-NULL.
 *  "pSocket"
 *      The address at which the Socket is to be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Socket Error if the function fails in
 *      a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_Socket_CreateByHostAndPort(
        PKIX_Boolean isServer,
        PRIntervalTime timeout,
        char *hostname,
        PRUint16 portnum,
        PRErrorCode *pStatus,
        PKIX_PL_Socket **pSocket,
        void *plContext)
{
        PRNetAddr netAddr;
        PKIX_PL_Socket *socket = NULL;
        char *sepPtr = NULL;
        PRHostEnt hostent;
        PRIntn hostenum;
        PRStatus prstatus = PR_FAILURE;
        char buf[PR_NETDB_BUF_SIZE];

        PKIX_ENTER(SOCKET, "pkix_pl_Socket_CreateByHostAndPort");
        PKIX_NULLCHECK_THREE(hostname, pStatus, pSocket);


        prstatus = PR_GetHostByName(hostname, buf, sizeof(buf), &hostent);

        if ((prstatus != PR_SUCCESS) || (hostent.h_length != 4)) {
                /*
                 * The hostname may be a fully-qualified name. Try using just
                 * the leftmost component in our lookup.
                 */
                sepPtr = strchr(hostname, '.');
                if (sepPtr) {
                        *sepPtr++ = '\0';
                }
                prstatus = PR_GetHostByName(hostname, buf, sizeof(buf), &hostent);

                if ((prstatus != PR_SUCCESS) || (hostent.h_length != 4)) {
                        PKIX_ERROR
                                (PKIX_PRGETHOSTBYNAMEREJECTSHOSTNAMEARGUMENT);
                }
        }

        netAddr.inet.family = PR_AF_INET;
        netAddr.inet.port = PR_htons(portnum);

        if (isServer) {

                netAddr.inet.ip = PR_INADDR_ANY;

        } else {

                hostenum = PR_EnumerateHostEnt(0, &hostent, portnum, &netAddr);
                if (hostenum == -1) {
                        PKIX_ERROR(PKIX_PRENUMERATEHOSTENTFAILED);
                }
        }

        PKIX_CHECK(PKIX_PL_Object_Alloc
                (PKIX_SOCKET_TYPE,
                sizeof (PKIX_PL_Socket),
                (PKIX_PL_Object **)&socket,
                plContext),
                PKIX_COULDNOTCREATESOCKETOBJECT);

        socket->isServer = isServer;
        socket->timeout = timeout;
        socket->clientSock = NULL;
        socket->serverSock = NULL;
        socket->netAddr = &netAddr;

        socket->callbackList.listenCallback = pkix_pl_Socket_Listen;
        socket->callbackList.acceptCallback = pkix_pl_Socket_Accept;
        socket->callbackList.connectcontinueCallback =
                 pkix_pl_Socket_ConnectContinue;
        socket->callbackList.sendCallback = pkix_pl_Socket_Send;
        socket->callbackList.recvCallback = pkix_pl_Socket_Recv;
        socket->callbackList.pollCallback = pkix_pl_Socket_Poll;
        socket->callbackList.shutdownCallback = pkix_pl_Socket_Shutdown;

        if (isServer) {
                PKIX_CHECK(pkix_pl_Socket_CreateServer(socket, plContext),
                        PKIX_SOCKETCREATESERVERFAILED);
                *pStatus = 0;
        } else {
                PKIX_CHECK(pkix_pl_Socket_CreateClient(socket, plContext),
                        PKIX_SOCKETCREATECLIENTFAILED);
                PKIX_CHECK(pkix_pl_Socket_Connect(socket, pStatus, plContext),
                        PKIX_SOCKETCONNECTFAILED);
        }

        *pSocket = socket;

cleanup:
        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(socket);
        }

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_GetCallbackList
 */
PKIX_Error *
pkix_pl_Socket_GetCallbackList(
        PKIX_PL_Socket *socket,
        PKIX_PL_Socket_Callback **pCallbackList,
        void *plContext)
{
        PKIX_ENTER(SOCKET, "pkix_pl_Socket_GetCallbackList");
        PKIX_NULLCHECK_TWO(socket, pCallbackList);

        *pCallbackList = &(socket->callbackList);

        PKIX_RETURN(SOCKET);
}

/*
 * FUNCTION: pkix_pl_Socket_GetPRFileDesc
 */
PKIX_Error *
pkix_pl_Socket_GetPRFileDesc(
        PKIX_PL_Socket *socket,
        PRFileDesc **pDesc,
        void *plContext)
{
        PKIX_ENTER(SOCKET, "pkix_pl_Socket_GetPRFileDesc");
        PKIX_NULLCHECK_TWO(socket, pDesc);

        *pDesc = socket->clientSock;

        PKIX_RETURN(SOCKET);
}
