/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef __SSM_MINIHTTP_H__
#define __SSM_MINIHTTP_H__

#include "ssmerrs.h"
#include "ssmdefs.h"
#include "serv.h"
#include "prthread.h"
#include "ctrlconn.h"
#include "nlsutil.h"

/* Result codes; see 
   http://info.internet.isi.edu:80/in-notes/rfc/files/rfc2068.txt
   for more information */

typedef enum
{
    HTTP_OK = (PRInt32) 200,
    HTTP_NO_CONTENT = 204,
    HTTP_BAD_REQUEST = 400,
    HTTP_UNAUTHORIZED = 401,
    HTTP_FORBIDDEN = 403,
    HTTP_NOT_FOUND = 404,
    HTTP_METHOD_NOT_ALLOWED = 405,
    HTTP_INTERNAL_ERROR = 500,
    HTTP_NOT_IMPLEMENTED = 501,
    HTTP_SERVICE_UNAVAILABLE = 503,
    HTTP_GATEWAY_TIMEOUT = 504,
    HTTP_VERSION_UNSUPPORTED = 505
} HTTPErrorCode;

/* ### mwelch - This has to be done better after the 11/1998 demo. */
typedef enum
{
    SSM_UI_NULL = (PRUint32) 0,
        
    SSM_UI_KEYGEN_PROGRESS, /* Frameset containing keygen components */
    
    SSM_UI_DIALOG_FRAMESET, /* Generic frameset for dialogs */
    SSM_UI_DIALOG_FB,       /* Dialogs: NSM -> client feedback, kept open */
    SSM_UI_DIALOG_CANCEL    /* A "cancel" command invokable by the client */
} SSMDialogSelector;

struct HTTPRequest
{
    PRFileDesc *sock;       /* Socket with which we talk to client */

    char *rawreqbuf;        /* Copy of the raw request we got */
    char *reqbuf;           /* The HTTP request we got, 
                               which is later parsed */
    char **paramNames;      /* Names of parameters passed to the URL */
    char **paramValues;     /* Values of parameters passed to the URL */
    PRUint32 numParams;     /* How many parameters were passed */

    PRUint32 slen;          /* The amount of actual text in the buffer(s) */
    PRUint32 len;           /* The allocated length of the buffer(s) */

    char *hdrbuf;           /* Response header */
    SSMStatus rv;            /* Internal result code (used if an 
                               internal error occurred) */
    HTTPErrorCode httprv;   /* HTTP result code (200, 401, 404, etc) */
    char *errormsg;         /* if a problem occurs, send this string back
                               to the client. */

    char *filename;         /* Canned file to fetch from data, if any */

    /* These point to chars within (reqbuf). */
    char *handlerName;      /* Command requested by client */
    char *params;           /* Parameters passed to GET */
    char *agent;            /* User-Agent value */

    char *ctrlrid;          /* Authorization: RID of control connection */
    char *password;         /* Authorization: RID of nonce */
    char *referer;          /* The URL for the referer */
    SSMControlConnection *ctrlconn;
                            /* Control connection owning this connection */
    SSMResource *target;    /* Target resource, if any */

    char *language;         /* Accept-Language value */
    char *charset;          /* Accept-Charset value */

    PRBool sentResponse;    /* Have we already sent a response to the user? */

    PRBool keepSock;        /* Are we keeping this socket (eg for logging)? */
    PRBool processText;     /* Should we interpret files we send out as text? */
    SSMResourceID rid;      /* ID of requested resource */
};

typedef struct SSMHTTPParamMultValuesStr{
    const char *key;
    char **values;
    int numValues;
} SSMHTTPParamMultValues;

/* Typedef for a command handler. */
typedef SSMStatus (*SSMCommandHandlerFunc)(HTTPRequest *req);

/*Quick macro for getting the target from a HTTP Request*/
#define REQ_TARGET(req) ((req)->target) ? (req)->target : &((req)->ctrlconn->super.super)

/* Register a command handler. */
SSMStatus SSM_HTTPRegisterCommandHandler(const char *name, 
                                         SSMCommandHandlerFunc func);

/* function to parse the request and submit html */
SSMStatus SSM_HTTPGetGenericLump(HTTPRequest *req, PRBool binary);

/* The default handler, which uses baseRef as a prefix for two strings
   to use in processing and returning HTTP headers and content. Can be 
   called by other handlers if no special header/content processing 
   for the user is necessary. */
SSMStatus SSM_HTTPDefaultCommandHandler(HTTPRequest *req);

/* ### sjlee: This function is called when you need to sleep one second
 *            before closing the window due to collision of dialog windows
 *            this is really the default command handler with a sleep
 */
SSMStatus SSM_HTTPCloseAndSleep(HTTPRequest* req);


/* Handler for getting binary data. This should be consolidated with
   the default handler. */
SSMStatus SSM_HTTPGetBinaryHandler(HTTPRequest *req);

/* Handler for monitoring resources. */
SSMStatus SSM_HTTPMonitorResourceHandler(HTTPRequest *req);

/* Handler which shuts down a resource. */
SSMStatus SSM_HTTPShutdownResourceHandler(HTTPRequest *req);

/* Hello World example */
SSMStatus SSM_HTTPHelloWorld(HTTPRequest *req);

/* NLS Hello World example */
SSMStatus SSM_HTTPHelloWorldWithNLS(HTTPRequest *req);

/* SSM_HTTPThread listens to the HTTP socket for connections from
   one or more clients. (arg) should be an open socket (PRFileDesc*),
   therefore the port needs to have been created before being passed 
   to this thread. */
void SSM_HTTPThread(void *arg);

/* Open the HTTP listener. */
SSMStatus SSM_InitHTTP(void);

/* Get the port on which we're listening for HTTP connections. */
PRUint32 SSM_GetHTTPPort(void);

/* Construct a URL and size info based on a known page/access point 
   and control connection. */
SSMStatus SSM_GenerateURL(SSMControlConnection *conn,
                          char *command,
                          char *baseRef,
                          SSMResource *targetRes, /* can pass NULL for this */
                          char *otherParams,
                          PRUint32 *width, PRUint32 *height,
                          char **url);
                          
/* Main listening thread for HTTP requests */
extern PRThread *httpListenThread;

extern PRUint32 httpListenPort;

/* Command handler utilities */
SSMStatus SSM_HTTPParamByName(HTTPRequest *req, 
                              const char *key, const char **value);

SSMStatus SSM_RIDTextToResource(HTTPRequest *req, const char *ridText,
                                SSMResource **res);

SSMStatus SSM_HTTPReportError(HTTPRequest *req, HTTPErrorCode statusCode);

void SSM_HTTPReportSpecificError(HTTPRequest *req, char *fmt, ...);

SSMStatus SSM_HTTPParamValue(HTTPRequest *req, const char *key, char **value);
SSMStatus SSM_HTTPParamValueMultiple(HTTPRequest *req, const char *key, 
                                     SSMHTTPParamMultValues *values);

SSMStatus SSM_HTTPAddParamValue(HTTPRequest *req, const char * key, 
                                const char * value);

SSMStatus
SSM_HTTPSendOKHeader(HTTPRequest *req, 
                     char * addtlHeaders,
                     char * contentType);

SSMStatus
SSM_HTTPSendUTF8String(HTTPRequest *req, char * str);

SSMStatus 
SSM_HTTPCloseWindow(HTTPRequest *req);

char * SSM_ConvertStringToHTMLString(char * string);

SSMStatus SSM_ProcessPasswordWindow(HTTPRequest * req);

char * SSM_GenerateChangePasswordURL(PK11SlotInfo *slot, SSMResource *target);
#endif

