/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#ifndef __CMTCMN_H__
#define __CMTCMN_H__

/*
** Macro shorthands for conditional C++ extern block delimiters.
*/
#ifdef __cplusplus
#define CMT_BEGIN_EXTERN_C     extern "C" {
#define CMT_END_EXTERN_C       }
#else
#define CMT_BEGIN_EXTERN_C
#define CMT_END_EXTERN_C
#endif


#include <stdio.h>
#include "ssmdefs.h"

#ifdef WIN32
#include <windows.h>
#include <winsock.h>
#else
#ifndef macintosh
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifndef XP_OS2_VACPP
#include <unistd.h>
#endif /* vacpp */
#endif
#endif

#ifdef XP_OS2
#define INCL_DOSSESMGR
#define INCL_DOSERRORS
#define INCL_DOSPROCESS
#include "os2.h"
#endif

#include "cmtclist.h"

typedef void (*void_fun) (void);

#ifdef XP_OS2_VACPP	/* OS/2 Visual Age */
typedef void (*_Optlink CMTP7ContentCallback)(void *arg, const char *buf,
				      unsigned long len);
#else
typedef void (* CMTP7ContentCallback)(void *arg, const char *buf,
				      unsigned long len);
#endif

typedef struct _CMTPrivate CMTPrivate;
typedef void (*CMTReclaimFunc)(CMTPrivate *priv);
struct _CMTPrivate {
    CMTReclaimFunc dest;
    /* void (* dest)(CMTPrivate *priv); */
};

/*
 * The version supported by the protocol library.
 * Pass this version to CMT_Hello.
 */
#define PROTOCOL_VERSION SSM_PROTOCOL_VERSION

/*
 * Socket Abstraction layer.
 */

typedef void* CMTSocket;

/*
 * This function should return a handle to an internet-streaming TCP socket.
 * For UNIX, need a UNIX socket for hello message.
 *
 * If parameter is 1, then we want UNIX socket.  Otherwise INET socket.
 */
typedef CMTSocket (*CMT_GET_SOCKET)(int);

/*
 * All connections will be on the same machine.  Below is the port number
 * to connect to.
 * If using a UNIX domain socket, then use path as the path to connect to.
 */
typedef CMTStatus (*CMT_CONNECT)(CMTSocket sock, short port, char* path);

/*
 * Will call this function to verify that UNIX domain sockets are
 * held by correct user.  If the socket is not, then the socket is
 * closed.
 */

typedef CMTStatus (*CMT_VERIFY_UNIX)(CMTSocket sock);

/*
 * Use this function to send data across the socket
 */
typedef CMInt32 (*CMT_SEND)(CMTSocket sock, void* buffer, size_t length);

/*
 * Use this function to select a socket. If poll is non-zero, then 
 * just poll the socket to see if there is any data waiting to be read.
 * Otherwise block until there is data waiting to be read.  Select any
 * of the sockets in the array and return the selected socket.
 */
typedef CMTSocket (*CMT_SELECT)(CMTSocket *sock, int numSocks, int poll);

/*
 * Use this function to receive data from a socket.  Function should
 * return number of bytes actually read. Return -1 in case of error.
 */
typedef CMInt32 (*CMT_RECEIVE)(CMTSocket sock, void *buffer, size_t bufSize);

/*
 * Use this function to shutdown writing to the socket.
 */
typedef CMTStatus (*CMT_SHUTDOWN)(CMTSocket sock);

/*
 * Prototype for function to close down the socket permanently.
 */
typedef CMTStatus (*CMT_CLOSE)(CMTSocket sock);


/*
 * This structure should be passed at initialization time.
 */
typedef struct CMT_SocketFuncsStr {
    CMT_GET_SOCKET  socket;
    CMT_CONNECT     connect;
    CMT_VERIFY_UNIX verify;    
    CMT_SEND        send;
    CMT_SELECT      select;
    CMT_RECEIVE     recv;
    CMT_SHUTDOWN    shutdown;
    CMT_CLOSE       close;
} CMT_SocketFuncs;

/*  mutex abstraction */
typedef void * CMTMutexPointer;
typedef void (*CMTMutexFunction)(CMTMutexPointer);

typedef struct _CMT_MUTEX {
    CMTMutexPointer mutex;
    CMTMutexFunction lock;
    CMTMutexFunction unlock;
} CMT_MUTEX;

#define CMT_LOCK(_m)     if (_m) _m->lock(_m->mutex)
#define CMT_UNLOCK(_m)   if (_m) _m->unlock(_m->mutex)

/*  session info */
typedef struct _CMT_DATA {
	CMTSocket sock;
	CMUint32 connectionID;
	CMTPrivate *priv;
	struct _CMT_DATA * next;
	struct _CMT_DATA * previous;
} CMT_DATA, *PCMT_DATA;

/*  event info */
typedef struct _CMT_EVENT {
    CMUint32 type;
    CMUint32 resourceID;
    void_fun handler;
    void *data;
    struct _CMT_EVENT * next;
    struct _CMT_EVENT * previous;
} CMT_EVENT, *PCMT_EVENT;

/*
 * Type defines for callbacks that are set in the CMT library.
 */

/*
 * FUNCTION TYPE: promptCallback_fn
 * -----------------------------------
 * INPUTS
 *    arg
 *        This is an opaque pointer that is provided to the library
 *        when the callback is registered.  The library merely passes
 *        it to the callback so the application can properly handle
 *        the password prompt.
 *    clientContext
 *        This is the client context pointer that is set the client.
 *    prompt
 *        The text to display to the user when prompting for a password.
 *    isPasswd
 *        If this value is non-zero, then this is prompt is for a password
 *        request and the text typed in by the user should not be echoed
 *        to the screen.  Meaning the text should be masked as asterisks
 *        or nothing should be displayed on the screen as the user types
 *        input.  If the value is zero, then the function should echo
 *        the user's input to the screen.
 *
 * NOTES:
 * This defines the type of function used for prompting the user for a
 * typed input.  The application is free to use that arg parameter as 
 * it sees fit.  The apllication provides the parameter when registering
 * the callback so the application will know what type of data the pointer
 * represents.  The application should display the text passed via the
 * prompt parameter. Then read the input typed by the user and return that 
 * value.  If isPasswd is non-zero, then the function should not echo
 * the user's input.
 *
 * RETURN:
 * This function should return the user's input or NULL if the user canceled
 * the operation or some other error occurred.
 */
typedef char * (*promptCallback_fn)(void *arg, char *prompt, 
                                    void* clientContext, int isPasswd);

/*
 * FUNCTION TYPE: applicationFreeCallback_fn
 * ------------------------------------
 * INPUTS
 *    userInput
 *        A string returned by callback of the type promptCallback_fn that
 *        the application has implemented.
 * NOTES:
 * This function is used to free the string returned by the callback of
 * type promptCallback_fn or filePathPromptCallback_fn.  
 * After calling the apllication provided function of type promptCallback_fn,
 *  the library will process the data and then
 * call the application provided function of type applicationFreeCallback_fn
 * so that the memory can be discarded of correctly.
 *
 * RETURN
 * This function has no return value. 
 */
typedef void (*applicationFreeCallback_fn)(char *userInput);

/*
 * FUNCTION TYPE: filePathPromptCallback_fn
 * ----------------------------------------
 * INPUTS
 *    arg
 *        This is an opaque pointer that is provided to the library
 *        when the callback is registered.  The library merely passes
 *        it to the callback so the application can properly handle
 *        the password prompt.
 *    prompt
 *        The text to display to the user when prompting for a file.
 *    fileRegEx
 *        This is the regular expression the selected file should 
 *        satisfy.  These will tend to be of the form *.<extension>
 *    shouldFileExist
 *        A flag indicating wheter or not the file selected by the user
 *        should already exist on disk.
 * NOTES:
 * This type defines the prototype for a function used to prompt the user 
 * for a file.  When the psm server needs to request the path to a file,
 * ie when doing PKCS-12 restore or backup, it will send an event and 
 * this a function of this type will ultimately be called.  The implementation
 * should display the text from the parameter prompt to the user.  The 
 * fileRegEx is intended as a guide for the types of file the user 
 * should select.  The application does not have to enforce choosing a file
 * that matches the regular expression, but is encouraged to relay the
 * extension type to the user.  If shouldFileExist is a non-zero value,
 * then the file selected by the user must already exist on disk.  If 
 * shouldFileExist has a value of zero, then the psm server will create
 * a file living at the path returned--overwriting any pre-existing files
 * or creating a new file if no file with the returned path exists.
 *
 * RETURN
 * The function should return a full path to the file the user has selected.
 * The returned string will be passed to the callback of type 
 * applicationFreeCallback_fn after the path is no longer needed.
 */
typedef char * (*filePathPromptCallback_fn)(void   *arg,
                                            char   *prompt, 
                                            char   *fileRegEx,
                                            CMUint32  shouldFileExist);

/*
 * FUNCTION TYPE: uiHandlerCallback_fn
 * -----------------------------------
 * INPUTS
 *    resourceID
 *        The ID of the resource that generated the UI event.
 *    context
 *        A pointer that was originally created by a call to 
 *        uiHandlerCallback_fn.  When non-NULL, this value 
 *        be used as a map to a previously created window which
 *        should be the parent of whatever window is created by
 *        the current call.
 *    width
 *        The width of the new window created.
 *    height
 *        The height of the new window created.
 *    url
 *        The URL to load in the new window.
 *    data
 *        An opaque pointer that was passed in when registering your
 *        UI handler via CMT_SetUIHandler.  the application should
 *        use the pointer to help it bring up new windows.
 *
 * NOTES
 * This defines the signature of a function called whenever a UI event occurs.
 * resourceID is the handle of the resource that sent the UI event and 
 * context is a pointer returned by a previous call the uiHandlerCallback_fn
 * registered with the control connection.  If non-NULL, context should be
 * used as the parent window for the window the function creates.  The 
 * function should then create an http window of size width x height that can 
 * handle Basic-auth URL's and the psm server will send the data to the newly
 * created window.
 *
 * RETURN
 * The function should return some pointer that is associated with the
 * window just created so that a future call to this event handler can
 * reference a window that was previously created.
 */
typedef void* (*uiHandlerCallback_fn)(CMUint32 resourceID, void* context, 
                                      CMUint32 width, CMUint32 height, CMBool isModal,
                                      char* url, void* data);

/*
 * These #defines are to be used to fill in the type field for the
 * CMTSetPrefElement structure.
 */
#define CMT_PREF_STRING 0
#define CMT_PREF_BOOL   1
#define CMT_PREF_INT    2

/* structs to pack each preference item to pass between the psm server and 
 * the plugin
 */
typedef struct _CMTSetPrefElement {
    char* key;
    char* value;
    CMInt32 type;
} CMTSetPrefElement;

typedef struct _CMTGetPrefElement {
    char* key;
    CMInt32 type;
} CMTGetPrefElement;

/*
 * FUNCTION TYPE: savePrefsCallback_fn
 * -----------------------------------
 * INPUTS
 *    number
 *        The number of pref items to save.
 *    list
 *        The list of pref items delivered from the PSM server.
 *
 * NOTES
 * This defines the prototype for a function callback used for saving pref
 * changes passed from the PSM server.  Each preference item has a type
 * (string, boolean, or integer) so that the value string may be converted 
 * appropriately according to type.  The callback is not responsible for
 * freeing pref elements (keys and values).
 *
 * RETURN
 * None.
 */
typedef void (*savePrefsCallback_fn)(int number, CMTSetPrefElement* list);

typedef struct CMT_UserCallbacks {
    filePathPromptCallback_fn promptFilePath;
    void *filePromptArg;
    promptCallback_fn promptCallback;
    void *promptArg;
    applicationFreeCallback_fn userFree;
    savePrefsCallback_fn savePrefs;
} CMT_UserCallbacks;

#define RNG_OUT_BUFFER_LEN 4096
#define RNG_IN_BUFFER_LEN 4096

typedef struct CMT_RNGState
{
    char *outBuf;                     /* Outgoing random data cache */
    CMUint32 validOutBytes;            /* #bytes of random data to PSM */
    char *out_cur;                     /* Next CMT_RandomUpdate writes
                                          data here. */
    char *out_end;                         /* End of buffer */

    char *inBuf;                     /* Incoming random data cache */
    CMUint32 validInBytes;             /* #bytes of random data from PSM */
    char *in_cur;                    /* Next CMT_GenerateRandomBytes reads
                                        from here. */

} CMT_RNGState;

typedef struct _CMT_CONTROL {
    CMTSocket sock;
    CMUint32 sessionID;
    CMUint32 protocolVersion;
    CMUint32 port;
    CMTItem nonce;
    PCMT_DATA cmtDataConnections;
    PCMT_EVENT cmtEventHandlers;
    CMUint32 policy;
    CMInt32 refCount;
    CMT_MUTEX* mutex;
    char *serverStringVersion;
    CMT_SocketFuncs  sockFuncs;
    CMT_UserCallbacks userFuncs;
    CMT_RNGState rng;
} CMT_CONTROL, *PCMT_CONTROL;

/* Cert list structure */
typedef struct _CMT_CERT_LIST {
    CMTCList certs;
    CMInt32 count;
} CMT_CERT_LIST;

typedef struct _CMT_CERT_LIST_ELEMENT {
    CMTCList links;
    CMUint32 certResID;
} CMT_CERT_LIST_ELEMENT;

/* information required to pack the security advisor request */
typedef struct _CMTSecurityAdvisorData {
    CMInt32 infoContext;
    CMUint32 resID;
    char *hostname;
	char *senderAddr;
    CMUint32 encryptedP7CInfo;
    CMUint32 signedP7CInfo;
    CMInt32 decodeError;
    CMInt32 verifyError;
	CMBool encryptthis;
	CMBool signthis;
	int numRecipients;
	char **recipients;
} CMTSecurityAdvisorData;

CMT_BEGIN_EXTERN_C

/*
 * FUNCTION: CMT_ReferenceControlConnection
 * ----------------------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 * NOTES:
 * This function bumps up the reference count on the control connection
 * Each thread that has a pointer to the control connection should get
 * its own reference on the control connection to avoid having another thread
 * free up the memory associated with the control connection.
 *
 * RETURN:
 * A return value of CMTSuccess indicates the reference count of the 
 * control connection was successfully achieved.  Any other return value
 * indicates an error.
 */
CMTStatus CMT_ReferenceControlConnection(PCMT_CONTROL control);

/*
 * FUNCTION: CMT_EstablishControlConnection
 * ----------------------------------------
 * INPUTS
 *    path
 *        The full path to the psm server. (Including the psm executable.)
 *    sockFuncs
 *        A structure containing pointers to functions that implement
 *        socket functions using the applications I/O model.  These 
 *        functions will be used by the cmt library to communicate
 *        with the psm server.
 *    mutex
 *        A structure containig a pointer to a mutex defined by the 
 *        implementation.
 * NOTES:
 * This function will establish a control connection to a psm server.
 * First the function will attempt to connect to a psm server that 
 * is already running by calling CMT_ControlConnect.  If that function
 * call succeeds, then the function will return an established control
 * connection to a psm process that is already running.  If 
 * CMT_ControlConnect fails, then this function will launch the psm server
 * that resides in the directory passed in by path and establish a control
 * connection to it.  Read comments on the CMT_MUTEX structure for proper
 * semantics of the lock and un-lock functions.  If you pass in NULL for 
 * the mutex parameter, access to the control connection will not be 
 * thread safe.  If the application using this library is multi-threaded,
 * then it is highly recommended that the application provide a locking 
 * mutex to this function.  Before performing any other actions, the 
 * applicatin must call CMT_Hello to send the psm server a hello message 
 * which will fully establish a port for communication between the psm server
 * and the application.
 *
 * The application may choose to launch the psm server itself and then
 * just call CMT_ControlConnect, but when doing so the application must
 * launch the psm executable with the directory psm lives in as the working
 * directory when launching the psm server.
 *
 * RETURN
 * This function will return a pointer to an established control connection
 * with the psm server upon successful connection.  If the return value
 * is NULL, that means the function was not able to establish a connection
 * to the process created by invoking the parameter "path".  Make sure
 * the path is correct.  Another common reason for failure is not initializing
 * the network libraries.
 */
PCMT_CONTROL CMT_EstablishControlConnection(char            *path, 
                                            CMT_SocketFuncs *sockFuncs,
                                            CMT_MUTEX       *mutex);

/*
 * FUNCTION: CMT_ControlConnect
 * ----------------------------
 * INPUTS:
 *    mutex
 *        A structure containig a pointer to a mutex defined by the 
 *        implementation.
 *    sockFuncs
 *        A structure containing pointers to functions that implement
 *        socket functions using the applications I/O model
 * NOTES
 * This function tries to connect to the psm server establishing a
 * control connection between an already running  psm server and the client 
 * library.
 *
 * The mutex should contain an application defined mutex and corresponding
 * functions for locking and unlocking the mutex.  Read comments on the
 * CMT_MUTEX structure for the proper semantics of the lock and un-lock
 * functions. If you pass in NULL for the mutex parameter, access to the
 * control connection will not be thread safe.  If the application using this 
 * library is multi-threaded, then it is highly recommended that
 * the application provid a locking mutex to this function.  Before 
 * performing any other actions, the application must call CMT_Hello
 * to send the psm server a hello message which will fully establish
 * a port for communication between the psm server and the application.
 *
 * RETURN
 * This function will return a pointer to an established control connection
 * with the psm server upon successful connection.  If the return value is
 * NULL, that means the psm server is not running and that the application
 * must start the psm server before calling this function again.
 */
PCMT_CONTROL CMT_ControlConnect(CMT_MUTEX* mutex, CMT_SocketFuncs *sockFuncs);

/*
 * FUNCTION: CMT_CloseControlConnection
 * ------------------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 * NOTES:
 * This function closes down the control connection and frees the memory
 * associated with the passed in control connection.
 *
 * RETURN
 * A return value of CMTSuccess indicates successful destruction of the 
 * control connection.  Any other return value indicates an error and the
 * state of the connection betwenn the library and the psm server is 
 * undefined.
 */
CMTStatus CMT_CloseControlConnection(PCMT_CONTROL control);

/*
 * FUNCTION: CMT_Hello
 * ------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    data
 *        Data needed for the Hello message.  It has following subfields.
 *    version
 *        The version of the psm protocol.  For this release, the version
 *        should always be 1.
 *    profile
 *        << This value is currently not used by PSM, but passing in a >>
 *        << proper profile name is recommended for consistency.       >>
 *        The Communicator profile to use when initializing the crypto engine
 *        in the psm server.  If Communicator doesn't support profiles on
 *        the platform you are running on, pass in the empty string for
 *        this parameter.
 *    profileDir
 *        The full absolute path to the profile directory that corresponds
 *        to the profile.  If the application wants to use a default profile,
 *        an empty string is passed.
 * NOTES:
 * This function sends a hello message to the psm server which establishes 
 * the nonce for communication between the application and the psm server 
 * and initializes the crypto engine on the psm server.  After calling this
 * function, the applicatior can successfully call any other function that 
 * talks to the psm server.
 *
 * RETURN
 * A return value of CMTSuccess indicates the hello message was received and 
 * correctly processed by the psm server.  Any other return value indicates
 * a connection to the psm server was not established.
 */
CMTStatus CMT_Hello(PCMT_CONTROL control, CMUint32 version, char* profile,
                    char* profileDir);


/*
 * FUNCTION: CMT_PassAllPrefs
 * --------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    num
 *        Number of items that are passed to the psm server.
 *    list
 *        The list of actual preference items.
 *        - key: string for the preference key.
 *        - value: string for the preference value.
 *        - type: preference type (0: string, 1: boolean, 2: integer).
 * NOTES:
 * This function passes in all necessary preferences the psm server uses,
 * including necessary application-specific preferences.  This function must
 * be called after CMT_Hello() returns and before any crypto operations
 * to ensure a correct behavior.  Here is a description of some important 
 * preference items.
 *
 * - KEY                              VALUE                           TYPE 
 *   (DESCRIPTION)
 * --------------------------------------------------------------------------
 * - "security.enable_ssl2"           "true" | "false"                boolean 
 *   (whether to enable SSL2 cipher families)
 * - "security.enable_ssl3"           "true" | "false"                boolean 
 *   (whether to enable SSL3 cipher families)
 * - "security.default_personal_cert" "Select Automatically" | 
 *                                    "Ask Every Time"                string
 *   (whether to select automatically a personal certificate for client 
 *    authentication)
 * - "security.default_mail_cert"     [certificate's nickname] | NULL string
 *   (default certificate to be used for signing email messages)
 * - "security.ask_for_password"      "0" | "1" | "2"                 integer
 *   (mode for prompting the user for the certificate store password:
 *    0: ask for password initially and password does not expire,
 *    1: always ask for password,
 *    2: ask for password initially and stay logged on until the password
 *       expires)
 * - "security.password_lifetime"     [number of minutes]             integer
 *   (number of minutes for password expiration: used only if 
 *    ask_for_password == 2)
 *
 * One can add more application-specific items to the list.
 *
 * RETURN
 * A return value of CMTSuccess indicates successful transmission of the 
 * preference values.  Any other return value indicates an error.
 */
CMTStatus CMT_PassAllPrefs(PCMT_CONTROL control, int num, 
                           CMTSetPrefElement* list);

/*
 * FUNCTION: CMT_GetServerStringVersion
 * ------------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *
 * NOTES:
 * This function returns the string representing the version of the psm
 * server that was sent as part of the hello reply.  This string originated
 * in the psm server.
 *
 * RETURN
 * A string.  A NULL return value indicates an error.  The user must not free
 * this memory since it is memory owned by the control connection.
 */
char* CMT_GetServerStringVersion(PCMT_CONTROL control);

/* SSL functions */
/*
 * FUNCTION: CMT_OpenSSLConnection
 * -------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    sock
 *        The file descriptor of the socket to use for feeding the data
 *        back to the application.
 *    requestType
 *        The type of SSL connection to establish.  View ssmdefs.h for
 *        the possible Connection types to pass in.
 *    port
 *        The port which the psm server should connect to.
 *    hostIP
 *        The IP address of the server with which to establish an SSL
 *        connection.
 *    hostName
 *        The host name of the site to connect to.
 *    forceHandshake
 *        Indicates whether forced handshakes are required.  Should be CM_TRUE
 *        for protocols in which the server initiates the data transfer
 *        (e.g. IMAP over SSL or NNTP over SSL).  Otherwise, always set to
 *        CM_FALSE.
 *    clientContext
 *        Client supplied data pointer that is returned to the client during UI.
 * NOTES:
 * This function sends a message to the psm server requesting an SSL connection
 * be established.  The parameter "sock" is a file descriptor to use for
 * reading the decrypted data the psm server has fetched.  Afte all of the
 * contents have been read from the socket, the application should call 
 * CMT_DestroyDataConnection passing in the 2 parameters "control" and
 * "sock" that were passed into this function.
 *
 * Each SSL connection has a socket status variable associated with it.  The 
 * ssl data connection structure on the PSM server will exist, ie the memory
 * associated with it will not be freed, until the application tells the
 * PSM server what to do with socket status structure.  The application 
 * should call either CMT_ReleaseSSLSocketStatus or CMT_GetSSLSocketStatus
 * (but never both) so that the memory associated with the ssl connection
 * can be disposed of properly.
 *
 * RETURN
 * A return value of CMTSuccess indicates the psm server has established an 
 * SSL connection with the site passed in.  Any other return value indicates
 * an error setting up the connection and the application should not try 
 * to read any data from the socket "sock" passed in.
 */
CMTStatus CMT_OpenSSLConnection(PCMT_CONTROL control, CMTSocket sock,
                                SSMSSLConnectionRequestType requestType, 
                                CMUint32 port, char * hostIP, 
                                char * hostName, CMBool forceHandshake, void* clientContext);

CMTStatus CMT_GetSSLDataErrorCode(PCMT_CONTROL control, CMTSocket sock,
                                  CMInt32* errorCode);

/*
 * FUNCTION: CMT_GetSSLSocketStatus
 * --------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    sock
 *        The socket which was passed into CMT_OpenSSLConnection as the file
 *        descriptor for the application to read data from.
 *    pickledStatus
 *        On return, filled with data blob that contains pickled socket 
 *        status.
 *    level
 *        On return, filled with the security level indicator.
 * NOTES
 * This function requests socket status information that is relevant to the
 * client.
 *
 * RETURN
 * A return value of CMTSuccess indicates retrieving the Socket Status 
 * resource on the psm server was successful.  Any other return value 
 * indicates an error in getting the socket status resource.
 */
CMTStatus CMT_GetSSLSocketStatus(PCMT_CONTROL control, CMTSocket sock, 
                                 CMTItem* pickledStatus, CMInt32* level);

/*
 * FUNCTION: CMT_ReleaseSSLSocketStatus
 * ------------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    sock
 *        The socket which was passed into CMT_OpenSSLConnection as the file
 *        descriptor for the application to read data from.
 * NOTES
 * This function instructs the SSL connection to discard the Socket Status
 * variable associated with it.
 *
 * RETURN
 * A return value of CMTSuccess indicates the socket status structure was
 * successfully discarded.  Any other return value indicates an error.
 */
CMTStatus CMT_ReleaseSSLSocketStatus(PCMT_CONTROL control, CMTSocket sock);

/*
 * FUNCTION: CMT_OpenTLSConnection
 * -------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        PSM server.
 *    sock
 *        The file descriptor of the socket to use for feeding the data
 *        back to the application.
 *    port
 *        The port which the PSM server should connect to.
 *    hostIP
 *        The IP address of the server with which to establish a TLS
 *        connection.
 *    hostName
 *        The host name of the site to connect to.
 *
 * NOTES:
 * This function sends a message to the PSM server requesting a TLS connection
 * to be established.  A TLS connection is the one that starts out as a regular
 * TCP socket but later turns into a secure connection upon request.  The
 * parameter "sock" is a file descriptor to use for reading data from the PSM
 * server.  After all of the contents have been read from the socket, the
 * application should call CMT_DestroyDataConnection passing in the two
 * parameters "control" and "sock" that were passed into this function.
 *
 * RETURN
 * A return value of CMTSuccess indicates the PSM server has established a
 * TLS connection with the site passed in.  Any other return value indicates
 * an error setting up the connection and the application should not try
 * to read any data from the socket "sock" passed in.
 */
CMTStatus CMT_OpenTLSConnection(PCMT_CONTROL control, CMTSocket sock,
                                CMUint32 port, char* hostIP, char* hostName);

/*
 * FUNCTION: CMT_TLSStepUp
 * -----------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the PSM
 *        server.
 *    sock
 *        The file descriptor to use for exchanging data with the PSM server.
 *    clientContext
 *        The client context that is to be saved.
 *
 * RETURN
 * A return value of CMTSuccess indicates that the PSM server successfully
 * upgraded the connection to a secure one.  Any other return value indicates
 * the TLS step-up did not succeed.
 */
CMTStatus CMT_TLSStepUp(PCMT_CONTROL control, CMTSocket sock, 
                        void* clientContext);

/*
 * FUNCTION: CMT_OpenSSLProxyConnection
 * ------------------------------------
 * INPUTS
 *     control
 *         A control connection that has established a connection with the PSM
 *         server.
 *     sock
 *         The file descriptor to use for exchanging data with the PSM server.
 *     port
 *         The port which the PSM server should connect to.
 *     hostIP
 *         The IP address of the server with which to establish a proxy
 *         connection.
 *     hostName
 *         The host name of the server to connect to.
 *
 * NOTES
 * This function opens a connection to an SSL proxy server in the clear.  It 
 * is almost identical to the role of CMT_OpenTLSConnection(), but is offered 
 * to be clear of the fact that it is opening a connection to a proxy server.
 * Consult the usage of CMT_OpenTLSConnection() for more information.  Also,
 * note that this by itself does not carry out any authorization (or 
 * authentication) other than simply connecting to the port.  Further exchange
 * is left to the client.  Moreover, once it is ready to transmit actual data, 
 * the client is required to call CMT_ProxyStepUp() to turn on security on the
 * connection.
 *
 * RETURN
 * A return value of CMTSuccess indicates the PSM server has established a
 * connection with the SSL proxy server.  Any other return value indicates
 * an error setting up the connection and the application should not try
 * to read any data from the socket "sock" passed in.
 */
CMTStatus CMT_OpenSSLProxyConnection(PCMT_CONTROL control, CMTSocket sock,
                                     CMUint32 port, char* hostIP, 
                                     char* hostName);

/*
 * FUNCTION: CMT_ProxyStepUp
 * -------------------------
 * INPUTS
 *     control
 *         A control connection that has established a connection with the PSM
 *         server.
 *     sock
 *         The file descriptor to use for exchanging data with the PSM server.
 *     clientContext
 *         The client context that is to be saved.
 *     remoteUrl
 *         The URL of the remote host.
 *
 * NOTES
 * This function instructs PSM to turn on security on the connection.  Once it
 * returns, the connection is ready for SSL data exchange.  The remoteUrl
 * argument is used in validating the SSL connection for the man-in-the-middle
 * attack during the SSL handshake.
 *
 * RETURN
 * A return value of CMTSuccess indicates that the PSM server has turned on
 * security on the connection.  Any other return value indicates an error
 * setting up the connection and the application should not try to read/write
 * data from the socket.
 */
CMTStatus CMT_ProxyStepUp(PCMT_CONTROL control, CMTSocket sock,
                          void* clientContext, char* remoteUrl);

/* PKCS 7 Functions */
/*
 * FUNCTION: CMT_PKCS7DecoderStart
 * -------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    connectionID
 *        A pointer to a pre-allocated block of memory where the library
 *        can place the resource ID of the data connection associated with
 *        this PKCS7 decode process.
 *    cb
 *        A pointer to a function that will receive the content bytes as
 *        they are recovered while decoding.
 *    cb_arg
 *        An opaque pointer that will get passed to the callback function 
 *        "cb" when "cb" is invoked.
 *
 * NOTES
 * This function sends a message to the psm server requesting a context with
 * which to decode a PKCS7 stream.  The contents of the decoded stream will
 * be passed to the function cb.
 *
 * RETURN
 * A return value of CMTSuccess indicates a context for decoding a PKCS7 
 * stream was created on the psm server and is ready to process a PKCS stream.
 * Any other return value indicates an error and that no context for decoding
 * a PKCS7 stream was created.
 */
CMTStatus CMT_PKCS7DecoderStart(PCMT_CONTROL control, void * clientContext, CMUint32 * connectionID, CMInt32 * result,
                                CMTP7ContentCallback cb, void *cb_arg);

/*
 * FUNCTION: CMT_PKCS7DecoderUpdate
 * --------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    connectionID
 *        The resource ID of a PKCS Decoder Context returned by the 
 *        function CMT_PKCS7DecoderStart.
 *    buf
 *        The next section of a PKCS7 stream to feed to the PKCS7 decoder
 *        context.
 *    len
 *        The length of the buffer "buff" passed in.
 * NOTES
 * This function sends a buffer to a PKCS7 decoder context.  The context then
 * parses the data and updates its internal state.
 *
 * RETURN
 * A return value of CMTSuccess indicates the PKCS7 decoder context 
 * successfully read and parsed the buffer passed in as a PKCS7 buffer.
 * Any other return value indicates an error while processing the buffer.
 */
CMTStatus CMT_PKCS7DecoderUpdate(PCMT_CONTROL control, CMUint32 connectionID, 
                                 const char * buf, CMUint32 len);

/*
 * FUNCTION: CMT_PKCS7DecoderFinish
 * --------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    connectionID
 *        The resource ID of a PKCS Decoder Context returned by the 
 *        function CMT_PKCS7DecoderStart.
 *    resourceID
 *        A pointer to a pre-allocated chunk of memory where the library
 *        can place a copy of the resource ID associated with the content
 *        info produced while the decoder context existed.
 * NOTES:
 * This function shuts down a PKCS7 decoder context on the psm server and
 * returns the resource ID of the content info that was decoded from the
 * PKCS7 stream passed in to the decoder context via CMT_PKCS7DecoderUpdate 
 * calls.  The attributes you can retrieve from the Content Info via the 
 * functions CMT_GetNumericAttribute or CMT_GetStringAttribute are as
 * follows:
 *
 * Attribute                        Type      What it means
 * ---------                        ----      -------------
 * SSM_FID_P7CINFO_IS_SIGNED        Numeric   If non-zero, then the content
 *                                            info is signed.
 *
 * SSM_FID_P7CINFO_IS_ENCRYPTED     Numeric   If non-zero, then the content
 *                                            info is encrypted.
 *
 * SSM_FID_P7CINFO_SIGNER_CERT      Numeric   The resource ID of the 
 *                                            certificate used to sign the 
 *                                            content info.
 *
 * RETURN
 * A return value of CMTSuccess indicates the PKCS7 Decoder Context was 
 * properly shutdown and that a resource for the Content Info exists on 
 * the psm server.  Any other return  value indicates an error.  The library
 * will have tried to shutdown the PKCS7 decoder context, but may have failed.
 * The Content Info will not exist on the psm server in this case.
 */
CMTStatus CMT_PKCS7DecoderFinish(PCMT_CONTROL control, CMUint32 connectionID, 
                                 CMUint32 * resourceID);

/*
 * FUNCTION: CMT_PKCS7DestroyContentInfo
 * -------------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    resourceID
 *        The resource ID of content info returned via the function 
 *        CMT_PKCS7DecoderFinish or CMT_CreateSigned.
 * NOTES
 * This function destroys the content info on the psm server.
 * 
 * RETURN
 * A return value of CMTSuccess indicates the content info was successfully
 * destroyed.  Any other return value indicates an error and that the 
 * resource with the resource ID passed in was not destroyed.
 */
CMTStatus CMT_PKCS7DestroyContentInfo(PCMT_CONTROL control, 
                                      CMUint32 resourceID);

/*
 * FUNCTION: CMT_PKCS7VerifyDetachedSignature
 * ------------------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    resourceID
 *        The resource ID of content info returned via the function 
 *        CMT_PKCS7DecoderFinish or CMT_CreateSigned.
 *    certUsage
 *        How the certificate that were used to sign should be interpretted.
 *    hashAlgID
 *        An identifier which tells the psm server which hash algorithm was
 *        to produce the signature.
 *    keepCerts
 *        If non-zero, the psm server will store any new certificates in
 *        content info into the local certificate database.
 *    digest
 *        A pre-calculated digest of the input.
 *    result
 *        A pointer to a pre-allocated chunk of memory where the library
 *        can place the result code of the verfication process.
 * NOTES
 * This function requests the psm server verify a signature within a 
 * Content Info.  
 *
 * Valid values for certUsage:
 * Use              Value
 * ---              -----
 * Email Signer     4
 * Object Signer    6
 *
 * Valid values for hashAlgID:
 * Hash Algorithm           Value
 * --------------           -----
 * MD2                      1
 * MD5                      2
 * SHA1                     3
 *
 * RETURN
 * If the function returns CMTSuccess, then psm server completed the operation
 * of verifying the signature and the result is located at *result.  If 
 * *result is non-zero, then the signature did not verify.  If the result is
 * zero, then the signature did verify.  Any other return value indicates
 * an error and the value at *result should be ignored.
 */
CMTStatus CMT_PKCS7VerifyDetachedSignature(PCMT_CONTROL control, 
                                           CMUint32     resourceID, 
                                           CMUint32     certUsage, 
                                           CMUint32     hashAlgID, 
                                           CMUint32     keepCerts, 
                                           CMTItem     *digest, 
                                           CMInt32     *result);

/*
 * FUNCTION: CMT_CreateSigned
 * --------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    scertID
 *        The resource ID of the certificate to use for signing data.
 *    ecertID
 *        The resource ID of the encryption cert associated with scertID.
 *        If the certificates are different, then the encryption cert
 *        will also be included in the signed message so that the recipient
 *        can save it for future encryption.
 *    dig_alg
 *        A representation of what algorithm to use for generating the 
 *        digest.
 *    digest
 *        The actual digest of the data.  
 *    ciRID
 *        A pointer to a pre-allocated chunk of memory where the library
 *        can place the resource ID of the content info created by the psm
 *        server.
 *    errCode
 *        A pointer to a pre-allocated chunk of memory where the library 
 *        can place the error code returned by the psm server in case of
 *        error. NOTE: The error codes need to be documented.
 * NOTES
 * This function creates a PKCS7 Content Info on the psm server that will
 * be used to sign the digest.  After creating this content info the 
 * application must use CMT_PKCS7Encoder{Start|Update|Finish} function
 * calls to encode the content info.
 * Currently there is only one supported value for digest algorithm:
 * Digest Algorithm     Value
 * ----------------     -----
 * SHA1                 4
 *
 * RETURN
 * A return value of CMTSuccess indicates the content info was successfully
 * created on the psm server and the application can proceed to encode the
 * content info with CMT_PKCS7Encoder* function calls.  Any other return
 * value indicates an error and the content info was not created.
 */
CMTStatus CMT_CreateSigned(PCMT_CONTROL control, CMUint32 scertID,
                           CMUint32 ecertID, CMUint32 dig_alg, 
                           CMTItem *digest,CMUint32 *ciRID,CMInt32 *errCode);

/*
 * FUNCTION: CMT_PKCS7EncoderStart
 * ------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    ciRID
 *        The resource ID of the content info to encode.
 *    connectionID
 *        A pointer to a pre-allocated chunk of memory where the library can
 *        place the resource ID of the resulting PKCS7 Encoder Context.
 *    cb
 *        A callback function that will get called as the content info
 *        is encoded.
 *    cb_arg
 *        An opaque pointer that will get passed to cb every time cb is
 *        called.
 *
 * NOTES
 * This function creates a PKCS7 encoder context on the psm server which
 * the application can use to encode a data as a PKCS7 content info.  The 
 * function cb will be used to pass back encoded buffers to the application.
 * The applicaton should concatenate the buffer passed in to cb to any buffer
 * previously passed in to the function cb.  The concatenation of all the
 * buffers passed in to cb will be the final product of the encoding 
 * procedure.
 *
 * RETURN
 * A return value of CMTSuccess indicates successful creation of a PKCS7
 * encoder context on the psm server.  Any other return value indicates 
 * an error and that no encoder context was created on the psm server.
 */
CMTStatus CMT_PKCS7EncoderStart(PCMT_CONTROL control, CMUint32 ciRID,
                                CMUint32 *connectionID, 
                                CMTP7ContentCallback cb,
                                void *cb_arg);

/*
 * FUNCTION: CMT_PKCS7EncoderUpdate
 * --------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    connectionID
 *        The resource ID of a PKCS7 Encoder context returned by the function
 *        CMT_PKCS7EncoderStart
 *    buf
 *        The next chunk of buffer to set as the data of the content info.
 *    len
 *        The length of the buffer passed in.
 * 
 * NOTES
 * This function sets the next buffer to include as part of the content to
 * encode.  The application can repeatedly call this function until all the
 * data has been fed to the encoder context.
 *
 * RETURN
 * A return value of CMTSuccess indicates the the encoder context on the psm
 * server successfully added the data to the encoder context.  Any other 
 * return value indicates an error.
 * 
 */
CMTStatus CMT_PKCS7EncoderUpdate(PCMT_CONTROL control, CMUint32 connectionID,
                                 const char *buf, CMUint32 len);

/*
 * FUNCTION: CMT_PKCS7EncoderFinish
 * --------------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    connectionID
 *        The resource ID of a PKCS7 Encoder context returned by the function
 *        CMT_PKCS7EncoderStart
 *
 * NOTES
 * This function destroys the PKCS7 encoder context with the resource ID of
 * connectionID on the psm server.
 *
 * RETURN
 * A return value of CMTSuccess indicates the PKCS7 encoder context was 
 * successfully destroyed.  Any other return value indcates an error while
 * trying to destroy the PKCS7 encoder context.
 */
CMTStatus CMT_PKCS7EncoderFinish(PCMT_CONTROL control, 
                                 CMUint32 connectionID);


/* Hash functions */
/*
 * FUNCTION: CMT_HashCreate
 * ------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    algID 
 *        A numeric value representing what kind of hash to perform.
 *    connID
 *        A pointer to a pre-allocated chunk of memory where the library
 *        can place a copy of the resource ID associated with the hashing
 *        context created by this function.
 * NOTES
 * This function sends a message to the psm server requesting a context be
 * created for performing a hashing operation.  The type of hashing operation
 * performed depends on the parameter passed in for algID.  The valid values
 * are:
 *
 * Hash Algorithm    Value
 * --------------    -----
 * MD2               1
 * MD5               2
 * SHA1              3
 *
 * RETURN
 * A return value of CMTSuccess indicates successful creation of a hashing
 * context ont he psm server.  The resource ID of the hashing context is 
 * located at *connID.  Any other return value indicates an error and the
 * value at *connID should be ignored.
 */
CMTStatus CMT_HashCreate(PCMT_CONTROL control, CMUint32 algID, 
                         CMUint32 * connID);

/*
 * FUNCTION: CMT_HASH_Destroy
 * --------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    connectionID
 *        The resource ID of the Hash context on psm to destroy.
 * NOTES
 * This function sends a message to the psm server requesting that the hashing
 * context with the resource ID of "connectionID" be destroyed.  This function
 * should be called after the hashing context is no longe needed.
 *
 * RETURN
 * A return value of CMTSuccess indicates the hashing context was successfully
 * destroyed.  Any other return value indicates an error while destroying
 * the resource with resource ID connectionID.
 */
CMTStatus CMT_HASH_Destroy(PCMT_CONTROL control, CMUint32 connectionID);

/*
 * FUNCTION: CMT_HASH_Begin
 * ------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    connectionID
 *        The resource ID of a hashing context on the psm server.
 * NOTES
 * This function will send a message to the psm server requesting the hashing
 * context initialize its internal state before beginning the process of hasing
 * data.
 *
 * RETURN
 * A return value of CMTSuccess indicates the state of the hashing context 
 * successfully initialized its state and that the application can start 
 * feeding the data to hash via the CMT_HASH_Update function.  Any other return
 * value indicates an error and the hashing context should not be used after
 * this function call.
 */
CMTStatus CMT_HASH_Begin(PCMT_CONTROL control, CMUint32 connectionID);

/*
 * FUNCTION: CMT_HASH_Update
 * -------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    connectionID
 *        The resource ID of a hashing context on the psm server.
 *    buf
 *        The data to feed to the hashing context.
 *    len
 *        The length of the buffer passed in as data.
 *
 * NOTES
 * This function sends the next buffer of data to be hashed as part
 * of the hash context associated with the parameter connecionID.  The 
 * application may call this function multiple times each time feeding
 * in the next chunk of data to be hashed.  The end result will be the hash
 * of the concatenation of the data passed into each successive call to 
 * CMT_HASH_Update.  To get the final hash of the data call CMT_HASH_End
 * after feeding all of the data to the context via this function.
 *
 * RETURN
 * A return value of CMTSuccess indicates the hash context on the psm server
 * successfully accepted the data and updated its internal state.  Any other 
 * return value indicates an error and the state of the hashing context is
 * undefined from this point forward.
 */
CMTStatus CMT_HASH_Update(PCMT_CONTROL control, CMUint32 connectionID, 
                          const unsigned char * buf, CMUint32 len);

/*
 * FUNCTION: CMT_HASH_End
 * ----------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    connectionID
 *        The resource ID of a hashing context on the psm server.
 *    result
 *        A pre-allocated buffer where the library can place the hash of
 *        the data that was fed to the hashing context.
 *    resultlen
 *        A pointer to a pre-allocated CMUint32 where the library can place
 *        the length of the hash returned via the parameter result.
 *    maxLen
 *        The alocated length of the buffer "result" that is passed in.  The 
 *        library will not write the hash out to "result" if the length of
 *        the hash of the data is greater than this parameter.
 *
 * NOTES
 * This function tells the psm server that no more data will be fed to 
 * the hashing context.  The hashing context finishes its hashing operation
 * and places the final hash of the processed data in the buffer result and
 * places the length of the resultant hash at *result.
 *
 * RETURN
 * A return value of CMTSuccess indicates the hashing context successfully 
 * finished the hashing operation and placed the resulting hash in the buffer
 * "result" as well as the hash's length at *resultLen.  Any other return 
 * value indicates an error and the values in buffer and *resultLen should 
 * ignored.
 */
CMTStatus CMT_HASH_End(PCMT_CONTROL control, CMUint32 connectionID, 
                       unsigned char * result, CMUint32 * resultlen, 
                       CMUint32 maxLen);

/* Resources */
/*
 * FUNCTION: CMT_GetNumericAttribute
 * ---------------------------------
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    resourceID
 *        The resource ID of the resource on the psm server which the 
 *        application wants to retrieve an attribute for.
 *    fieldID
 *        The numerical representation of the attribute the application wants
 *        to retrieve.
 *    value
 *        A pointer to a pre-allocated CMUint32 where the library can place
 *        a copy of the numeric attribute retrieved from the resource on the
 *        psm server
 *
 * NOTES
 * This function requests that the psm server query a resource for a numeric 
 * attribute.  The fieldID should be one of the enumerations defined by 
 * the enumeration SSMAttributeID.  Each resource has a set of attributes
 * that can be retrieved from the psm server.  Refer to the function where
 * a resource is created for a list of attributes that a given resource has.
 *
 * RETURN
 * A return value of CMTSuccess indicates the resource on the psm server 
 * returned the requested numeric attribute and the corresponding attribute 
 * value can be found at *value.  Any other return value indicates an error
 * and the value at *value should be ignored.
 */
CMTStatus CMT_GetNumericAttribute(PCMT_CONTROL control, CMUint32 resourceID, 
                                  CMUint32 fieldID, CMInt32* value);

/*
 * FUNCTION: CMT_GetStringAttribute
 * --------------------------------
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    resourceID
 *        The resource ID of the resource on the psm server which the 
 *        application wants to retrieve an attribute for.
 *    fieldID
 *        The numerical representation of the attribute the application wants
 *        to retrieve.
 *    value
 *        A pinter to a CMTItem that the library can store the string attribute 
 *        retrieved from the resource on the psm server.
 *
 * NOTES
 * This function requests that the psm server query a resource for a string 
 * attribute.  The fieldID should be one of the enumerations defined by 
 * the enumeration SSMAttributeID.  Each resource has a set of attributes
 * that can be retrieved from the psm server.  Refer to the function where
 * a resource is created for a list of attributes that a given resource has.
 *
 * RETURN
 * A return value of CMTSuccess indicates the resource on the psm server 
 * returned the requested string attribute and the corresponding attribute 
 * value can be found at *value.  Any other return value indicates an error
 * and the value at *value should be ignored.
 */
CMTStatus CMT_GetStringAttribute(PCMT_CONTROL control, CMUint32 resourceID, 
                                 CMUint32 fieldID, CMTItem *value);

/*
 * FUNCTION: CMT_SetStringAttribute
 * --------------------------------
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    resourceID
 *        The resource ID of the resource on the psm server which the 
 *        application wants to set an attribute for.
 *    fieldID
 *        The numerical representation of the attribute the application wants
 *        to set.
 *    value
 *        A pointer to a CMTItem containing the string (binary or ASCII) that
 *        the application wants to set as the attribute value.
 *
 * NOTES
 * This function requests that the psm server set a string attribute for 
 * a resource.  The fieldID should be one of the enumerations defined by
 * then enumeration SSMAttributeID.  Each resource has a set of attributes
 * that can be set on the psm server.  Refer to the function where a 
 * resource is created for a list of attributes that a given resource has.
 *
 * RETURN
 * A return value of CMTSuccess indicates the psm server successfully set
 * requested string attribute for the resource.  Any other return value
 * indicates an error in setting the resource.
 */
CMTStatus CMT_SetStringAttribute(PCMT_CONTROL control, CMUint32 resourceID,
                                 CMUint32 fieldID, CMTItem *value);

/*
 * FUNCTION: CMT_SetNumericAttribute
 * ---------------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    resourceID
 *        The resource ID of the resource on the psm server which the 
 *        application wants to set an attribute for.
 *    fieldID
 *        The numerical representation of the attribute the application wants
 *        to set.
 *    value
 *        A pointer to a CMTItem containing the string (binary or ASCII) that
 *        the application wants to set as the attribute value.
 *
 * NOTES
 * This function requests that the psm server set a numeric attribute for 
 * a resource.  The fieldID should be one of the enumerations defined by
 * then enumeration SSMAttributeID.  Each resource has a set of attributes
 * that can be set on the psm server.  Refer to the function where a 
 * resource is created for a list of attributes that a given resource has.
 *
 * RETURN
 * A return value of CMTSuccess indicates the psm server successfully set
 * requested numeric attribute for the resource.  Any other return value
 * indicates an error in setting the resource.
 */
CMTStatus CMT_SetNumericAttribute(PCMT_CONTROL control, CMUint32 resourceID,
                                  CMUint32 fieldID, CMInt32 value);

/*
 * FUNCTION: CMT_GetRIDAttribute
 * -----------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    resourceID
 *        The resource ID of the resource on the psm server the application
 *        wants to retrieve an attributre from.
 *    fieldID
 *        The numerical representation of the attribute the application wants
 *        to retrieve.
 *    value
 *        A pointer to a pre-allocated CMUint32 where the library can place 
 *        a copy of the desired RID attribute value retrieved from the 
 *        resource.
 *
 * NOTES
 * This function sends a message to the psm server requesting an attribute 
 * from the resource with ID "resourceID" that in turn is a resource ID.
 * The parameter fieldID should be one of the values defined by the enumeration
 * SSMAttributeID.  Refer to the function where a resource is created for a
 * list of attributes that a given resource has.  The application should
 * use this function to retrieve attributes that are resource ID's instead
 * of CMT_GetNumericAttribute because this funcion will increase the reference
 * count on the resource corresponding to the retrieved resource ID so that
 * the resource does not disappear while the application can reference it.
 *
 * RETURN
 * A return value of CMTSuccess indicates the psm server successfully 
 * retrieved the desired attribute and place it's value at *value. Any
 * other return value indicates an error and the value at *value should
 * be ignored.
 */
CMTStatus CMT_GetRIDAttribute(PCMT_CONTROL control, CMUint32 resourceID, 
                              CMUint32 fieldID, CMUint32 *value);

/*
 * FUNCTION: CMT_DestroyResource
 * -----------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    resourceID
 *        The resource ID of the resource on the psm server the application
 *        wants to destroy.
 *    resourceType
 *        The type of resource the application is trying to destroy. This value
 *        should be one defined by the enumeration SSMResourceType.
 *
 * NOTES
 * This function sends a message to the psm server release its reference on
 * the resource passed in.
 */
CMTStatus CMT_DestroyResource(PCMT_CONTROL control, CMUint32 resourceID, 
                              CMUint32 resourceType);

/*
 * FUNCTION: CMT_PickleResource
 * ----------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    resourceID
 *        The resource ID of the resource on the psm server the application
 *        wants to destroy.
 *    pickledResource
 *        A pointer to a CMTItem where the library can place 
 *        the pickled resource on successful return.
 * NOTES
 * This function sends a message to the psm server requesting the resource 
 * passed in be converted to a binary stream that can be re-instantiated 
 * at a later time by a call to CMT_UnpickleResource (during the same
 * execution of the application).
 *
 * RETURN
 * A return value of CMTSuccess indicates the resource was pickled successfully
 * and the resulting stream is located at *pickledResource.  After the pickled
 * resource is no longer needed, the application should free the pickled 
 * resource by calling CMT_FreeItem.  Any other return value indicates an 
 * error and the value at *pickledResource should be ignored.
 */
CMTStatus CMT_PickleResource(PCMT_CONTROL control, CMUint32 resourceID, 
                             CMTItem * pickledResource);

/*
 * FUNCTION: CMT_UnpickleResource
 * ------------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    resourceType
 *        A value defined by the enumeration SSMResourceType which is the
 *        type of the resource to unpickle.
 *    pickledResource
 *        The pickled resource as returned by CMT_PickleResource.
 *    resourceID
 *        A pointer to a pre-allocated CMUint32 where the library can
 *        place the resource ID of the re-instantiated resource.
 * NOTES
 * This function sends a message to the psm server requesting a pickled
 * resource be unpickled and re-instantiated.  
 *
 * RETURN
 * A return value of CMTSuccess indicates the psm server successfully 
 * re-instantiated a resource and the ID of the re-instantiated resource can
 * be found at *resourceID. Any other return value indicates an error
 * and the value at *resourceID should be ignored.
 */
CMTStatus CMT_UnpickleResource(PCMT_CONTROL control, CMUint32 resourceType, 
                               CMTItem  pickledResource, 
                               CMUint32 * resourceID);

/*
 * FUNCTION: CMT_DuplicateResource
 * -------------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    resourceID
 *        The resource ID of the resource to duplicate.
 *    newResID
 *        A pointer  to a pre-allocated CMUint32 where the library can place
 *        a copy of the duplicated resource's ID.
 *
 * NOTES
 * This function requests the resource passed in be duplicated and returns
 * the resource ID of the duplicated resource.
 *
 * RETURN
 * A return value of CMTSuccess indicates the resource was duplicated and
 * the application can refer to the resource stored at *newResID.  The 
 * application must also call CMT_DestroyResource when the new resource is
 * no longer needed.  Any other return value indicates an error and the
 * value at *newResID should be ignored.
 */
CMTStatus CMT_DuplicateResource(PCMT_CONTROL control, CMUint32 resourceID,
                                CMUint32 *newResID);

/*
 * FUNCTION: CMT_DestroyDataConnection
 * -----------------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    sock
 *        The File socket the application is using
 *        to read data from the psm server.
 * NOTES
 * This function destroys a data connection between the psm server and 
 * the application.  A Data Connection is created when an
 * SSL connection is established with the psm server.  After an SSL
 * connection is no longer necessary, the application should 
 * pass that socket to this function
 */
int CMT_DestroyDataConnection(PCMT_CONTROL control, CMTSocket sock);

/*
 * FUNCTION: CMT_CompareForRedirect
 * --------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    status1
 *        A pickled socket status resource that will be used as the first
 *        source for the re-direct comparison.
 *    status2
 *        A pickled socket status resource that will be used as the second
 *        source for the re-direct comparison.
 * NOTES
 * This function takes two pickled SSL Socket status resources.  The pickled
 * socket status should be a value obtained via the function 
 * CMT_GetSSLSocketStatus.  
 *
 * RETURN
 * A return value of CMTSuccess indicates a message was successfully sent and
 * retrieved from the psm server.  If the value at *res is 0 then the 
 * comparison for re-direction was unsuccessful and the user may be getting 
 * re-directed to an un-safe location.  Any other value for *res indicates
 * a safe re-direction.  Any other return value from this function indicates an
 * error and that the value at *res should be ingored.
 */
CMTStatus CMT_CompareForRedirect(PCMT_CONTROL control, CMTItem *status1, 
                                 CMTItem *status2, CMUint32 *res);

/*
 * FUNCTION: CMT_DecodeAndAddCRL
 * -----------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    derCrl
 *        The DER encoded CRL to add.
 *    len
 *        The length of the DER encoded CRL.
 *    url
 *        The URL associated with the URL being decoded.
 *    type
 *        An integer representation of the type of CRL that is being decoded.
 *    errMessage
 *        A pointer to a pre-allocated char* where the libraries can place 
 *        an error message that the application can display to the user in
 *        case of an error.
 *
 * NOTES
 * This function takes a DER encoded CRL and sends it to the psm server which
 * then decodes the CRL and tries to import into its profile.
 *
 * Valid values for type are as follows:
 * Value                              Meaning
 * -----                              -------
 * 0                                  This a Key Revocation List (KRL)
 * 1                                  This a Certificate Revocation List (CRL)
 *
 * RETURN:
 * A return value of CMTSuccess indicates the CRL was successfully decoded and
 * imported into the current profile.  Any other return value indicates 
 * failure.
 */
CMTStatus CMT_DecodeAndAddCRL(PCMT_CONTROL control, unsigned char *derCrl,
			      CMUint32 len, char *url, int type, 
                              char **errMess);

/*
 * FUNCTION: CMT_LogoutAllTokens
 * -----------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 * NOTES
 * This function will send a message to the psm server requesting the psm
 * server log out of all installed PKCS11 tokens.  (ie the internal key 
 * database and any smart cards being used.)
 *
 * RETURN
 * A return value of CMTSuccess indicates the psm server successfully logged 
 * out of all the tokens.  Any other return value indicates an error while
 * trying to log out of the tokens.
 */
CMTStatus CMT_LogoutAllTokens(PCMT_CONTROL control);

/*
 * FUNCTION: CMT_GetSSLCapabilites
 * -------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    capabilities
 *        A pointer to a pre-allocated CMUint32 where the library can place
 *        the resulting bit mask which represents the SSL capablities of the
 *        psm server.
 * NOTES
 * The function returns a bit mask via *capabilities which tells the user 
 * which SSL ciphers are enabled.
 *
 *  Relevent Cipher                       Value
 *  ----- --------                        -----
 *  RSA                                   0x00000001L
 *  MD2                                   0x00000010L
 *  MD5                                   0x00000020L
 *  RC2_CBC                               0x00001000L
 *  RC4                                   0x00002000L
 *  DES_CBC                               0x00004000L
 *  DES_EDE3_CBC                          0x00008000L
 *  IDEA_CBC                              0x00010000L
 *
 * RETURN
 * A return value of CMTSuccess indicates the capabilities was 
 * successfully retrieved.  Any other return value indicates an
 * error and the value at *capabilities should be ignored.
 */
CMTStatus CMT_GetSSLCapabilities(PCMT_CONTROL control, CMInt32 *capabilites);
/* Events */
CMTStatus CMT_RegisterEventHandler(PCMT_CONTROL control, CMUint32 type, 
                                   CMUint32 resourceID, 
                                   void_fun handler, void* data);
CMTStatus CMT_UnregisterEventHandler(PCMT_CONTROL control, CMUint32 type, 
                                     CMUint32 resourceID);

/*
 * FUNCTION: CMT_EventLoop
 * -----------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 * NOTES
 * This function polls the control connection to see if there is an event
 * waiting to be processed.  The function blocks until there is data on the
 * socket waiting to be processed, fetches the data, parses the message,
 * and calls the appropriate callback.  The function will then wait again
 * until there is data waiting on the socket.  So this function should be
 * called on its own thread where no other code exists because this function
 * will not return unless there was an error.
 * 
 * Code for your thread should look like this:
 *
 * void PSM_EventThread(..)
 * {
 *    CMT_EventLoop(control);
 * }
 *
 * If this function ever returns, that means either the control connection
 * has shut down, or there was an error trying read/receive data over the
 * control connection.
 *
 * RETURN
 * This function does not have a return value.
 */
void CMT_EventLoop(PCMT_CONTROL control);

/* Certificates */
/* Process KEYGEN tag */
typedef enum { 
    CM_KEYGEN_START = 11,
    CM_KEYGEN_PICK_TOKEN,
    CM_KEYGEN_SET_PASSWORD,
    CM_KEYGEN_ERR,
    CM_KEYGEN_DONE
} CMKeyGenTagReq;

char ** CMT_GetKeyChoiceList(PCMT_CONTROL control, char * type, char * pqgString);
typedef struct { 
    CMKeyGenTagReq op;
    int rid;
    int cancel;
    char * tokenName;
    void * current;
} CMKeyGenTagArg; 

typedef struct {
    char * choiceString;
    char * challenge;
    char * typeString;
    char * pqgString;
} CMKeyGenParams;    

typedef struct { 
    int needpwd;
    int minpwd;
    int maxpwd;
    int internalToken;
    char * password;
} CMKeyGenPassword;
    
/* string list structure */
typedef struct _NameList {
   int numitems;
   char ** names;
} NameList;

CMTStatus CMT_CreateKeyGenContextForKeyGenTag(PCMT_CONTROL control, 
                                              CMUint32       *keyGenContext,
                                              CMUint32       *errorCode);

char * CMT_GenKeyOldStyle(PCMT_CONTROL control, CMKeyGenTagArg * arg, 
                          CMKeyGenTagReq * next);
char * CMT_GetGenKeyResponse(PCMT_CONTROL control, CMKeyGenTagArg * arg, 
                             CMKeyGenTagReq *next);
/* Certificates */
CMTStatus CMT_FindCertificateByNickname(PCMT_CONTROL control, char * nickname, CMUint32 *resID);
CMTStatus CMT_FindCertificateByKey(PCMT_CONTROL control, CMTItem *key, CMUint32 *resID);
CMTStatus CMT_FindCertificateByEmailAddr(PCMT_CONTROL control, char * emailAddr, CMUint32 *resID);
CMTStatus CMT_AddCertificateToDB(PCMT_CONTROL control, CMUint32 resID, char *nickname, CMInt32 ssl, CMInt32 email, CMInt32 objectSigning);
CMUint32 CMT_DecodeCertFromPackage(PCMT_CONTROL control, char * certbuf, int len);
CMT_CERT_LIST *CMT_MatchUserCert(PCMT_CONTROL control, CMInt32 certUsage, CMInt32 numCANames, char **caNames);
void CMT_DestroyCertList(CMT_CERT_LIST *certList);
void CMT_DestroyCertificate(PCMT_CONTROL control, CMUint32 certID);
CMTStatus CMT_FindCertExtension(PCMT_CONTROL control, CMUint32 resID, 
                                CMUint32 extensiocmtcern, CMTItem *extValue);
CMTStatus CMT_HTMLCertInfo(PCMT_CONTROL control, CMUint32 certID, 
                           CMBool showImages, CMBool showIssuer, 
                           char **retHtml);

/*
 * FUNCTION: CMT_DecodeAndCreateTempCert
 * -------------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    data
 *        The DER encoded certificate.
 *    len
 *        The length of the DER data passed in.
 *    type
 *        A number corresponding to the type of cert being decoded. 
 *
 * NOTES
 * This function sends a message to the psm server requesting a raw DER
 * be decoded and creates a temporary certificate and returns the resource ID
 * associated with the newly created certificate resource.
 *
 * Valid values for the type parameter are as follows:
 *
 * Value                             Meaning
 * -----                             -------
 * 1                                 CA cert
 * 2                                 Server cert
 * 3                                 User cert (ie one for which user 
 *                                              has private key)
 * 4                                 Someone else's email cert
 *
 * RETURN:
 * A return value of 0 indicates an error and that the certificate was not
 * added to the temporary database as requested.  Any other return value
 * indicates success and the return value is the resource ID of the new 
 * certificate.
 */
CMUint32 CMT_DecodeAndCreateTempCert(PCMT_CONTROL control, char * data,
                                      CMUint32 len, int type);

CMTStatus CMT_SecurityAdvisor(PCMT_CONTROL control, CMTSecurityAdvisorData* data, 
                              CMUint32 *resID);

/* SecurityConfig (javascript) related functions */
typedef struct _CMTime {
    CMInt32 year;
    CMInt32 month;
    CMInt32 day;
    CMInt32 hour;
    CMInt32 minute;
    CMInt32 second;
} CMTime;

typedef struct _CMCertEnum {
    char* name;
    CMTItem certKey;
} CMCertEnum;

CMTItem* CMT_SCAddCertToTempDB(PCMT_CONTROL control, char* certStr, 
                               CMUint32 certLen);
CMTStatus CMT_SCAddTempCertToPermDB(PCMT_CONTROL control, CMTItem* certKey,
                                    char* trustStr, char* nickname);
CMTStatus CMT_SCDeletePermCerts(PCMT_CONTROL control, CMTItem* certKey,
                                CMBool deleteAll);
CMTItem* CMT_SCFindCertKeyByNickname(PCMT_CONTROL control, char* name);
CMTItem* CMT_SCFindCertKeyByEmailAddr(PCMT_CONTROL control, char* name);
CMTItem* CMT_SCFindCertKeyByNameString(PCMT_CONTROL control, char* name);
char* CMT_SCGetCertPropNickname(PCMT_CONTROL control, CMTItem* certKey);
char* CMT_SCGetCertPropEmailAddress(PCMT_CONTROL control, CMTItem* certKey);
char* CMT_SCGetCertPropDN(PCMT_CONTROL control, CMTItem* certKey);
char* CMT_SCGetCertPropTrust(PCMT_CONTROL control, CMTItem* certKey);
char* CMT_SCGetCertPropSerialNumber(PCMT_CONTROL control, CMTItem* certKey);
char* CMT_SCGetCertPropIssuerName(PCMT_CONTROL control, CMTItem* certKey);
CMTStatus CMT_SCGetCertPropTimeNotBefore(PCMT_CONTROL control, 
                                         CMTItem* certKey, CMTime* beforetime);
CMTStatus CMT_SCGetCertPropTimeNotAfter(PCMT_CONTROL control, 
                                        CMTItem* certKey, CMTime* aftertime);
CMTItem* CMT_SCGetCertPropIssuerKey(PCMT_CONTROL control, CMTItem* certKey);
CMTItem* CMT_SCGetCertPropSubjectNext(PCMT_CONTROL control, CMTItem* certKey);
CMTItem* CMT_SCGetCertPropSubjectPrev(PCMT_CONTROL control, CMTItem* certKey);
CMTStatus CMT_SCGetCertPropIsPerm(PCMT_CONTROL control, CMTItem* certKey,
                                  CMBool* isPerm);
CMTStatus CMT_SCCertIndexEnum(PCMT_CONTROL control, CMInt32 type,
                              CMInt32* number, CMCertEnum** list);


/* Misc */
/*
 * FUNCTION: CMT_SetPromptCallback
 * -------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    f
 *        A function that the library will call whenever the psm
 *        server wants to prompt the user.  
 *    arg
 *        An opaque pointer that will get passed to the callback function
 *        when invoked by the library.
 * NOTES:
 * This function sets the function the library should use when the psm server
 * wants to prompt the user for input.  The two cases would be to prompt for
 * a password or to prompt for a name to give to a certificate.  Refer to 
 * the description of the type promptCallback_fn above for details on 
 * the proper semantics for the function f.
 *
 * RETURN:
 * This function does not return any value.
 */
void CMT_SetPromptCallback(PCMT_CONTROL control, promptCallback_fn f, 
                           void* arg);

/*
 * FUNCTION: CMT_SetAppFreeCallback
 * --------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    f
 *        A function that will free the memory returned by the callbacks
 *        supplied to CMT_SetPromptCallback and CMT_SetFilePathPromptCallback.
 * NOTES
 * This function will be called after the values returned by the callbacks for
 * promptCallback_fn and filePromptCallback_fn are no longer needed.  Read
 * the comments for the type applicatoinFreeCallback_fn for details about
 * the proper semantics for the function passed in.
 *
 * RETURN
 * This function does not return any value. 
 */
void CMT_SetAppFreeCallback(PCMT_CONTROL control, 
                            applicationFreeCallback_fn f);

/*
 * FUNCTION: CMT_SetFilePathPromptCallback
 * ---------------------------------------
 * INPUTS:
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    f
 *        A function that the library will call whenever the psm server
 *        requests a file path.
 *    arg
 *        An opaque pointer that will be passed to the callback function
 *        f whenever f is invoked.
 * NOTES
 * This function sets the callback function the library will use whenever
 * the psm server requests a file path.  Read the comments on the definition
 * of filePathPromptCallback_fn for the proper semantics of the function f.
 *
 * RETURN
 * This function does not return any value.
 */
void CMT_SetFilePathPromptCallback(PCMT_CONTROL control, 
                                   filePathPromptCallback_fn f, void* arg);
/*
 * FUNCTION: CMT_SetUIHandlerCallback
 * ----------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    f
 *        A function pointer to the function that should be called whenever
 *        a UI event needs to be processed.
 *    data
 *        An opaque pointer that will be passed as the data parameter 
 *        whenever the regsitered functions is called.
 * NOTES
 * This functions sets the function that will be called whenever a UI event
 * happens.  Refer to the definition of uiHandlerCallback_fn for the proper
 * semantics of the function.
 *
 * In order to ensure all UI events are handled properly, the application 
 * linking with this library must call CMT_EventLoop in its own thread. 
 * Currently CMT_EventLoop is a blocking call that should never return.
 * So the application should create a thread that just calls CMT_EventLoop 
 * and does nothing else.
 *
 * This function should be called before CMT_Hello so that the psm server
 * will know your application is capable of handling UI events.
 *
 * RETURN
 * This function return CMTSuccess if registering the UI handler function
 * was successful.  Any other return value indicates an error while 
 * registering the ui handler.
 */
CMTStatus CMT_SetUIHandlerCallback(PCMT_CONTROL control, 
                                   uiHandlerCallback_fn f, void *data);

/*
 * FUNCTION: CMT_SetSavePrefsCallback
 * ----------------------------------
 * INPUTS
 *    control
 *        A control connection that has established a connection with the
 *        psm server.
 *    f
 *        A function pointer to the function that should be called for
 *        saving preferences.
 * NOTES
 * This function sets the callback that handles saving preferences.  Refer
 * to the definition of savePrefsCallback_fn for the proper semantics of the
 * function.
 *
 * RETURN
 * None.
 */
void CMT_SetSavePrefsCallback(PCMT_CONTROL control,
                              savePrefsCallback_fn f);

/*
 * FUNCTION: CMT_FreeItem
 * ----------------------
 * INPUTS
 *    p
 *        A pointer to a CMTItem which was returned or allocated by cmt 
 *        library.
 * NOTES
 * This function will free up all the memory associated with a CMTItem.
 * You should only call this function when you have a CMTItem that was
 * populated by the cmt library.
 *
 * RETURN
 * This function does not return any value.
 */
void CMT_FreeItem(CMTItem* p);


/* Random number support */

/*
 * FUNCTION: CMT_GenerateRandomBytes
 * ----------------------------------
 * INPUTS
 *    control
 *        A control connection that has been established with the psm server.
 *    buf
 *        A buffer into which random data will be written.
 *    maxbytes
 *        The size of (buf); the maximum number of bytes of random
 *        data to be written.
 * NOTES
 * CMT_GenerateRandomBytes obtains no more than (maxbytes) bytes 
 * of random data from the psm server.
 *
 * RETURN
 * The number of bytes of random data actually obtained.
 */
size_t CMT_GenerateRandomBytes(PCMT_CONTROL control, 
                               void *buf, CMUint32 maxbytes);

/*
 * FUNCTION: CMT_RandomUpdate
 * ----------------------------------
 * INPUTS
 *    control
 *        A control connection that has been established with the psm server.
 *    buf
 *        A buffer from which random data will be sent to the psm server.
 *    numbytes
 *        The number of bytes of random data in (buf).
 * NOTES
 * CMT_RandomUpdate collects random data from (buf) for eventual forwarding
 * to the psm server. Data is not sent immediately, but rather is piggybacked
 * onto existing protocol messages.
 *
 * RETURN
 * A return value of CMTSuccess indicates that the random data was
 * successfully mixed into the random data pool. Any other return value
 * indicates failure.
 */
CMTStatus CMT_RandomUpdate(PCMT_CONTROL control, void *data, size_t numbytes);

/*
 * FUNCTION: CMT_FlushPendingRandomData
 * ----------------------------------
 * INPUTS
 *    control
 *        A control connection that has been established with the psm server.
 * NOTES
 * CMT_FlushPendingRandomData flushes the random data cache, which was
 * populated by previous calls to CMT_RandomUpdate, by sending this random
 * data to the PSM server.
 *
 * RETURN
 * A return value of CMTSuccess indicates that the random data was
 * successfully sent to psm. Any other value indicates failure.
 */
CMTStatus CMT_FlushPendingRandomData(PCMT_CONTROL control);

/*
 * FUNCTION: CMT_SDREncrypt
 * ----------------------------------
 * INPUTS
 *    control
 *        A control connection that has been established with the psm server.
 *    ctx
 *        A pointer to application defined context.  It will be returned with
 *        the password callback request.
 *    key
 *        A buffer containing the key identifier to use for encrypting. May
 *        be NULL if keyLen is 0, which uses the "default" key.
 *    keyLen
 *        The length of the key identifier.
 *    data
 *        A buffer containing the data to encrypt
 *    dataLen
 *        The length of the data buffer
 *    result
 *        Recieves a pointer to a buffer containing the result of the
 *        encryption.
 *    resultLen
 *        Receives the length of the result buffer
 * NOTES
 *
 * RETURN
 *     CMTSuccess - the encryption worked.
 *     CMTFailure - some (unspecified) error occurred  (needs work)
 */
CMTStatus CMT_SDREncrypt(PCMT_CONTROL control, void *ctx,
               const unsigned char *key, CMUint32 keyLen,
               const unsigned char *data, CMUint32 dataLen,
               unsigned char **result, CMUint32 *resultLen);

/*
 * FUNCTION: CMT_SDRDecrypt
 * ----------------------------------
 * INPUTS
 *    control
 *        A control connection that has been established with the psm server.
 *    ctx
 *        A pointer to application defined context.  It will be returned with
 *        the password callback request.
 *    data
 *        A buffer containing the the results of a call to SDREncrypt
 *    dataLen
 *        The length of the data buffer
 *    result
 *        Recieves a pointer to a buffer containing the result of the
 *        decryption
 *    resultLen
 *        Receives the length of the result buffer
 * NOTES
 *
 * RETURN
 *     CMTSuccess - the encryption worked.
 *     CMTFailure - some (unspecified) error occurred  (needs work)
 */
CMTStatus CMT_SDRDecrypt(PCMT_CONTROL control, void *ctx,
               const unsigned char *data, CMUint32 dataLen,
               unsigned char **result, CMUint32 *resultLen);

/*
 * FUNCTION: CMT_SDRChangePassword
 * ----------------------------------
 * INPUTS
 *    control
 *        A control connection that has been established with the psm server.
 *    ctx
 *        A context pointer that may be provided in callbacks
 * NOTES
 *
 * RETURN
 *     CMTSuccess - the operation completed normally.
 *     CMTFailure - some (unspecified) error occurred. (probably not useful)
 */
CMTStatus CMT_SDRChangePassword(PCMT_CONTROL control, void *ctx);


/* Lock operations */
void CMT_LockConnection(PCMT_CONTROL control);
void CMT_UnlockConnection(PCMT_CONTROL control);


CMT_END_EXTERN_C

#endif /* __CMTCMN_H__ */
