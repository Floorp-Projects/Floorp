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

#include "minihttp.h"
#include "resource.h"
#include "dataconn.h"
#include "prtypes.h"
#include "prthread.h"
#include "prprf.h"
#include "serv.h"
#include "kgenctxt.h"
#include "base64.h"
#include "nlsutil.h"
#include "textgen.h"
#include "certlist.h"
#include "ssldlgs.h"
#include "ctrlconn.h"
#include "pkcs11ui.h"
#include "oldfunc.h"
#include "certres.h"
#include "advisor.h"
#include <string.h>

#define SSM_HTTP_REQUEST_CHUNKSIZE 256
#define SSM_STANDALONE_HTTP_PORT 9999

PRUint32 httpListenPort = 0;
PRThread *httpListenThread = NULL;

#ifdef ALLOW_STANDALONE
PRBool standalone = 0;
SSMControlConnection *standalone_conn = NULL;
SSMStatus SSMControlConnection_SetupNSS(SSMControlConnection *ctrl);
#endif

/* A command handler registration object */
typedef struct CommandHandlerEntry
{
    const char *key;
    SSMCommandHandlerFunc func;
} CommandHandlerEntry;

static SSMCollection *http_handlers = NULL;

SSMStatus SSM_HTTPFindCommandHandler(const char *name, 
                                     SSMCommandHandlerFunc *func);

SSMStatus SSM_HTTPOKCancelHandler(HTTPRequest *req);
SSMStatus SSM_HTTPFormSubmitHandler(HTTPRequest *req);
SSMStatus SSM_HTTPCloseIfCancel(HTTPRequest *req);


/* 
   -------------------------------------------
   Register command handlers here 
   -------------------------------------------
*/

static SSMStatus
http_register_handlers(void)
{
    SSMStatus rv = SSM_SUCCESS;

    /* Default command handler */
    rv = SSM_HTTPRegisterCommandHandler("get", SSM_HTTPDefaultCommandHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("getbin", SSM_HTTPGetBinaryHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("resmonitor", 
                                        SSM_HTTPMonitorResourceHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("shutdown", 
                                        SSM_HTTPShutdownResourceHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

#if 0
    rv = SSM_HTTPRegisterCommandHandler("setLocale", 
                                        SSM_SetLocaleCommandHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("reloadText", 
                                        SSM_ReloadTextCommandHandler);
    if (rv != SSM_SUCCESS)
        goto loser;
#endif

    rv = SSM_HTTPRegisterCommandHandler("BCAButton", 
                                        SSM_HTTPBadClientAuthButtonHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("unknownIssuerStep1",
                                        SSM_HTTPUnknownIssuerStep1ButtonHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("tokenSelect",
                                        SSMTokenUI_CommandHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("SetFIPSMode",
                                        SSM_SetFIPSModeCommandHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("PKCS11ShowSlots",
                                        SSM_ShowSlotsCommandHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("PKCS11ShowSlot",
                                        SSM_ShowSlotCommandHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("PKCS11LoginSlot",
                                        SSM_LoginSlotCommandHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("PKCS11LogoutSlot",
                                        SSM_LogoutSlotCommandHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("PKCS11LogoutAllSlots",
                                        SSM_LogoutAllSlotsCommandHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("OKButton", 
					SSM_OKButtonCommandHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    /* handler for the second step is called form formsubmit_handler 
     * from finish_import_cert form submission
     */
    rv = SSM_HTTPRegisterCommandHandler("importCACert1",
                                        SSM_CertCAImportCommandHandler1);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("check_for_cancel",
                                        SSM_SubmitFormFromButtonHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("check_for_cancel_and_free_target",
                                        SSM_SubmitFormFromButtonAndFreeTarget);

    rv = SSM_HTTPRegisterCommandHandler("delete_certificate",
                                        SSM_DeleteCertHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("ldap_request_others",
					SSM_ProcessLDAPRequestHandler);
    if (rv != SSM_SUCCESS)
	goto loser;

    rv = SSM_HTTPRegisterCommandHandler("remove_privileges",
                                        SSM_RemovePrivilegesHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    /* Hello World example */
    rv = SSM_HTTPRegisterCommandHandler("hello", SSM_HTTPHelloWorld);
    if (rv != SSM_SUCCESS)
        goto loser;

    /* Unicode Hello World example */
    rv = SSM_HTTPRegisterCommandHandler("hello_unicode", 
                                        SSM_HTTPHelloWorldWithNLS);
    if (rv != SSM_SUCCESS)
        goto loser;
    rv = SSM_HTTPRegisterCommandHandler("ok_cancel_handler",
                                        SSM_HTTPOKCancelHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("cert",
                                         SSM_HTTPCertListHandler);
    if (rv != SSM_SUCCESS)
        goto loser;


    /* Put your command handler here */
    rv = SSM_HTTPRegisterCommandHandler("formsubmit_handler",
                                         SSM_HTTPFormSubmitHandler);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("close_if_cancel",
                                        SSM_HTTPCloseIfCancel);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("showIssuer",
                                        SSM_ShowCertIssuer);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPRegisterCommandHandler("certPrettyPrint",
                                        SSM_PrettyPrintCert);
    if (rv != SSM_SUCCESS)
        goto loser;

    goto done;
 loser:
    if (rv == SSM_SUCCESS)
        rv = SSM_FAILURE;
 done:
    return rv;
}

static void
http_request_destroy(HTTPRequest *r)
{
    if (r->target)
        SSM_FreeResource(r->target);
    PR_FREEIF(r->reqbuf);
    PR_FREEIF(r->rawreqbuf);
    PR_FREEIF(r->hdrbuf);
    PR_FREEIF(r->filename);
    PR_FREEIF(r->paramNames);
    PR_FREEIF(r->paramValues);
    PR_FREEIF(r->ctrlrid);
    PR_FREEIF(r->password);
    PR_FREEIF(r->errormsg);

    if ((r->sock) && (!r->keepSock))
    {
        SSMStatus rv;

        /* wtc says I should shut down before closing */
        rv = PR_Shutdown(r->sock, PR_SHUTDOWN_BOTH);
        if (rv != PR_SUCCESS) {
            SSM_DEBUG("Tried to shutdown socket, but got return val %d\n"
                      "\tWill try to close socket anywary.\n", rv);
        }
        /* close the socket */
        rv = PR_Close(r->sock);
        if (rv != PR_SUCCESS) {
            SSM_DEBUG("Error trying to close http socket: %d\n",rv);
        }
    }
    else if (!r->sock)
        SSM_DEBUG("Hey! There's no socket to close!\n");
    (void) memset(r, 0, sizeof(HTTPRequest));
    PR_Free(r);
}

static SSMStatus
http_request_grow_request(HTTPRequest *req, PRUint32 newSize)
{
    PRUint32 newLimit = req->len;
    char *newPtr;
    SSMStatus rv = SSM_SUCCESS;

    /* If we're already big enough, we don't need to do anything. */
    if (newSize <= req->len)
        return SSM_SUCCESS;

    /* Ensure that the request is sized to the next power of two above
       (newSize). (This cuts down on the number of reallocations.) */
    if (newLimit == 0)
        newLimit = 1;
    while(newLimit < newSize)
        newLimit <<= 1;

    if (newLimit == 0)
    {
        /* this should never happen */
        rv = SSM_FAILURE;
        goto loser;
    }

    /* Reallocate the string to the new length. */
    newPtr = (char *) PR_REALLOC(req->reqbuf, newLimit);
    if (!newPtr) goto loser;

    req->reqbuf = newPtr;
    req->len = newLimit;

    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
 done:
    return rv;
}

static SSMStatus
http_request_add_chunk(HTTPRequest *req, char *newChunk, PRUint32 len)
{
    SSMStatus rv = http_request_grow_request(req, req->slen + len + 1);

    if (rv == SSM_SUCCESS)
    {
        char *c = req->reqbuf + req->slen;
        strncpy(c, newChunk, len);
        *(c + len) = '\0'; /* null terminate the end */
        req->slen += len;
    }
    return rv;
}

static HTTPRequest *
http_request_create(PRFileDesc *sock)
{
    PRUint32 size = SSM_HTTP_REQUEST_CHUNKSIZE;
    HTTPRequest *result = (HTTPRequest *) PR_CALLOC(sizeof(HTTPRequest));
    if (!result) goto loser;

    if (http_request_grow_request(result, size) != SSM_SUCCESS)
        goto loser;

	result->processText = PR_TRUE; /* assume we're acting on text */
    result->sock = sock;

    goto done;
 loser:
    if (result) 
    {
        http_request_destroy(result);
        result = NULL;
    }
 done:
    return result;
}

/* Eat a line of text from the request. Advances (*buf) to the next line. */
static char *
nextLine(char **buf)
{
    char *rv = *buf;
    char *c = *buf;

    if (!*c) return NULL;

    while (!IS_EOL(*c) && (*c))
        c++;

    /* Blank out as many consecutive newline characters as we have. */
    while (IS_EOL(*c))
        *c++ = '\0';

    *buf = c;
    return rv;
}

PRInt32
httpparse_count_params(HTTPRequest *req)
{
    char *c;
    PRInt32 numParams = req->params ? 1 : 0;

    if (numParams > 0)
    {
        /* find the parameters. */
        for(c = req->params;*c != '\0';c++)
            { 
                if (*c == '&')
                    numParams++;
            }
    }
    req->numParams = numParams;
    return numParams;
}

static void
httpparse_unescape_param(char* str)
{
    char *plusSign, *percentSign;
    
    /*First look for all of the '+' and convert them to spaces */
    plusSign = str;
    while ((plusSign=strchr(plusSign, '+')) != NULL) {
        *plusSign = ' ';
    }
    percentSign = str;
    while ((percentSign=strchr(percentSign, '%')) != NULL) {
        char string[3];
        int  value;
        
        string[0] = percentSign[1];
        string[1] = percentSign[2];
        string[2] = '\0';
        
        PR_sscanf(string,"%x", &value);
        *percentSign = (char)value;
        memmove(&percentSign[1], &percentSign[3], 1+strlen(&percentSign[3]));
    }
}

SSMStatus
httpparse_parse_params(HTTPRequest *req)
{
    SSMStatus rv = SSM_SUCCESS;
    char *c, d, *start;
    PRInt32 numParams, i;

    numParams = httpparse_count_params(req);
    SSM_DEBUG("Found %d params\n", numParams);
    req->paramNames = (char **) PR_Calloc(numParams+1, sizeof(char *));
    if (!req->paramNames)
        goto loser;

    req->paramValues = (char **) PR_Calloc(numParams+1, sizeof(char *));
    if (!req->paramValues)
        goto loser;

    /* Put the params in their place. */
    for(i=0, c = start = req->params; i < numParams; i++)
    {
        /* Get the parameter name. */
        while((*c) && (*c != '=') && (*c != '&'))
            c++;
        req->paramNames[i] = start; /* name goes here */
        if (*c == '\0')
        {
            /* reached end of params while looking for a name.
               leave the value null, end. */
            break;
        }
        
        /* save off the delimiter so that we can decide how to proceed */
        d = *c;

        /* null out the delimiter so that the param name is usable */
        *c++ = '\0';
        start = c;

        if (d == '&')
            /* found a name but no value. leave value null, loop back. */
            continue;
        
        /* Now, copy the value. */
        req->paramValues[i] = start;
        /* If we have more params to process, 
           tie off the end of this parameter value. */
        if (i != (numParams-1))
        {
            while((*c) && (*c != '&')) /* (*c) check just to be safe */
                {   
                    if (*c == '~')
                        *c = '&';
                    c++;
                }
            *c++ = '\0';
            start = c; /* get ready to mark off next parameter */
        }
        httpparse_unescape_param(req->paramNames[i]);
        httpparse_unescape_param(req->paramValues[i]);
        SSM_DEBUG("Param Name: %s, Param Value: %s\n", req->paramNames[i],
                                                       req->paramValues[i]);
    }
    goto done;
 loser:
    PR_FREEIF(req->paramNames);
    PR_FREEIF(req->paramValues);
 done:
    return rv;
}

/* Parse an incoming request. */
static SSMStatus
http_parse_request(HTTPRequest *req)
{
    char *line, *chunk = req->reqbuf;
    char *b, *e; /* beginning and end of something we want */
    char *p; /* beginning of params */
    SECItem tmpItem = {siBuffer, NULL, 0};

    while ((line = nextLine(&chunk)) != NULL)
    {
        /* Is it a request we're interested in? */
        if (!STRNCASECMP(line, "GET ", 4))
        {
            SSM_DEBUG("Parsing GET request: `%s'\n", line);

            /* Parse the GET request. */
            b = strchr(line, ' ');
            e = strrchr(line, ' ');

            if (!b)
                SSM_DEBUG("Couldn't find beginning of file!");
            if (!e)
                SSM_DEBUG("Couldn't find end of file!");
            if ((!b) || (!e))
                continue;

            while ((IS_WHITESPACE(*b)) && (b < e)) b++;
            while ((IS_WHITESPACE(*e)) && (b < e)) e--;
            e++;
            *e = '\0';
            if (*b == '/') b++; /* move past the first slash */
            req->handlerName = b; /* this is the command handler */
            SSM_DEBUG("Base ref: `%s'.\n", req->handlerName);
            p = strchr(b, '?');
            if (p)
            {
                *p++ = '\0';
                req->params = p;
                SSM_DEBUG("Params: `%s'.\n", 
                          req->params ? req->params : "(none)");
                httpparse_parse_params(req);
            }
            continue;
        }

        /* If we get here, then we assume we have a header with a colon
           in it (i.e. a header line other than the GET). 
           We're interested in anything after the colon. */
        b = strchr(line, ':');
        if (!b)
            /* Dunno what to do, ignore this line */
            continue;

        do
        {
            b++;
        }
        while (IS_WHITESPACE(*b));

        if (!STRNCASECMP(line, "User-Agent", 10))
        {
            SSM_DEBUG("Found User-Agent: `%s'.\n", b);
            req->agent = b;
        }
        else if (!STRNCASECMP(line, "Accept-Language", 15))
        {
            SSM_DEBUG("Found Accept-Language: `%s'.\n", b);
            req->language = b;
        }
        else if (!STRNCASECMP(line, "Accept-Charset", 14))
        {
            SSM_DEBUG("Found Accept-Charset: `%s'.\n", b);
            req->charset = b;
        }
        else if (!STRNCASECMP(line, "Referer", 7)) {
            /* Make the new request inherit the same target as 
             * its referer
             */
            req->referer = b;
        }
        else if (!STRNCASECMP(line, "Authorization", 13))
        {
            SECStatus srv;
            SSM_DEBUG("Found Authorization: `%s'.\n", b);

            /* If the auth type is basic, process the auth info. */
            if (!STRNCASECMP(b, "Basic", 5))
            {
                PRIntn len;

                /* move to the auth info itself */
                while ((*b) && (!IS_WHITESPACE(*b))) b++;
                while ((*b) && (IS_WHITESPACE(*b))) b++;

                /* decode base-64. yikes. */
                srv = ATOB_ConvertAsciiToItem(&tmpItem, b);
                if (srv == SECSuccess)
                {
                    /* We need to copy the ascii over and NULL
                     * terminate it correctly becuase the
                     * Base64 decoder doesn't guarantee that.
                     */
                    char *w = (char *) PR_Malloc(tmpItem.len+1);
                    
                    PL_strncpy(w, (char *) tmpItem.data, tmpItem.len);
                    w[tmpItem.len] = '\0';
                    SSM_DEBUG("Decoded auth: '%s'.\n", w);
                    PR_Free(tmpItem.data);
                    tmpItem.data = (unsigned char *) w;
                    len = tmpItem.len;
                    /* 
                       divide control RID (username) and nonce (password).
                       ### mwelch - I walk manually through this because
                       the string isn't null-terminated.
                    */
                    while((len > 0) && (*w != ':'))
                    {
                        w++;
                        len--;
                    }
                    if (len > 0)
                    {
                        PRIntn ridLen = tmpItem.len-len;
                        req->ctrlrid = (char *) PR_CALLOC(ridLen+1);
                        req->password = (char *) PR_CALLOC(len+1);
                        if (req->ctrlrid)
                        {
                            strncpy(req->ctrlrid, (char *) tmpItem.data, (unsigned long) ridLen);
                            SSM_DEBUG("ctrlrid: %s\n", req->ctrlrid);
                        }
                        else
                            SSM_DEBUG("Couldn't allocate a ctrlrid.\n");
                        if (req->password)
                        {
                            /* Skip over the colon separating rid and nonce. */
                            strncpy(req->password, ++w, --len);
                            SSM_DEBUG("password: %s\n", req->password);
                        }
                        else
                            SSM_DEBUG("Couldn't allocate a password.\n");
                    }
                    else
                        SSM_DEBUG("Couldn't find ':' in between ctrlrid and password.\n");
                    SECITEM_FreeItem(&tmpItem, PR_FALSE);
                }
                else
                    SSM_DEBUG("srv %ld decoding basic auth info.\n", (long) srv);
            }
            else
                SSM_HTTPReportError(req, HTTP_UNAUTHORIZED);
        }
    }
    return SSM_SUCCESS;
}

/* Read an HTTP request. */
SSMStatus
http_read_request(PRFileDesc *sock, HTTPRequest **returnReq)
{
    PRIntn read;
    PRBool detectEOR = PR_FALSE; /* Have we detected the end of the request? */
    char buf[LINESIZE];
    SSMStatus rv = SSM_SUCCESS;
    PRUint32 tail;
    HTTPRequest *req = NULL;

    if (!sock || !returnReq)
    {
        rv = PR_INVALID_ARGUMENT_ERROR;
        goto loser;
    }

    /* Set up the request object. */
    req = http_request_create(sock);

    if (!req) goto loser;

    /* Get the request in chunks. */
    while(!detectEOR)
    {
        PR_SetError(0, 0); /* reset so that error detect below works */
        read = PR_Recv(sock, buf, LINESIZE, 0, PR_INTERVAL_NO_TIMEOUT);
        if (read <= 0)
        {
            rv = PR_GetError();
            if (rv == PR_SUCCESS) rv = PR_GetOSError();
            goto loser;
        }

        /* Add the latest bits onto the end of the request */
        rv = http_request_add_chunk(req, buf, read);
        if (rv != SSM_SUCCESS) goto loser;

        /* Look for LF+LF, CR+CR, CR+LF+CR+LF, or (heh) LF+CR+LF+CR.
           If we find any of these, it means that we have the end of
           request. */

        (void) memcpy((char *) &tail, &(req->reqbuf[req->slen-4]), 4);

        /* Do the four-byte comparisons first, they're easier
           and more common */
        if ((tail == 0x0d0a0d0a) || (tail == 0x0a0d0a0d))
            detectEOR = PR_TRUE;
        else 
        {
            /* Look for CR+CR or LF+LF. */
            tail &= 0x0000FFFF;
            if ((tail == 0x00000d0d) || (tail == 0x00000a0a))
                detectEOR = PR_TRUE;
        }
    }

    /* duplicate the buffer, use one copy for parsing */
    req->rawreqbuf = (char *) PR_CALLOC(req->slen+1);
    if (!req->rawreqbuf) goto loser;
    (void) memcpy(req->rawreqbuf, req->reqbuf, req->slen+1);

    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
 done:
    if (returnReq) *returnReq = req;
    return rv;
}

static SSMStatus
ssm_http_param_from_index(HTTPRequest *req, const char *key, char **value,
                          int startIndex, int *valueIndex)
{
    PRUint32 i;
    SSMStatus rv = SSM_FAILURE;

    if (!value) 
      return rv;

    *value = NULL; /* in case we fail */
    *valueIndex = -1;
    for(i=startIndex; i < req->numParams; i++)
    {
        if (req->paramNames[i] && (!PL_strcmp(req->paramNames[i], key)))
        {
            *value = req->paramValues[i];
            *valueIndex = i;
            rv = SSM_SUCCESS;
            break;
        }
    }
    if (rv != SSM_SUCCESS)
        SSM_DEBUG("HTTPParamValue: Error %d attempting to get parameter '%s'.\n"
                  , rv, key);
    return rv;
}

SSMStatus
SSM_HTTPParamValueMultiple(HTTPRequest *req, const char *key,
                           SSMHTTPParamMultValues *values)
{
    int currIndex, nextIndex;
    unsigned int i, numVars = 0;
    char *tmp;
    SSMStatus rv;
    /*
     * First, let's count the number of parameters there are, the fill in
     * the array.
     */
    PR_ASSERT(values->key == NULL && values->values == NULL);
    for (i=0; i<req->numParams; i++) {
        rv = ssm_http_param_from_index(req,key,&tmp,i,&currIndex);
        if (rv != SSM_SUCCESS || currIndex == -1) {
            break;
        }
        numVars++;
        i = currIndex;
    }
    if (numVars == 0) {
        goto loser;
    }
    values->values = SSM_NEW_ARRAY(char*, numVars);
    currIndex = 0;
    for (i=0; i<numVars;i++) {
        rv = ssm_http_param_from_index(req,key,&values->values[i], currIndex,
                                       &nextIndex);
        if (rv != SSM_SUCCESS || nextIndex == -1) {
            break;
        }
        currIndex = nextIndex+1;
    }
    values->numValues = numVars;
    values->key = key;
    return SSM_SUCCESS;
 loser:
    PR_FREEIF(values->values);
    values->values = NULL;
    values->numValues=0;
    return SSM_FAILURE;
}

SSMStatus
SSM_HTTPParamValue(HTTPRequest *req, const char *key, char **value)
{
    int valueIndex;

    return ssm_http_param_from_index(req, key, value, 0, &valueIndex);
}

static SSMStatus
http_authenticate(HTTPRequest *req)
{
    SSMStatus rv = SSM_FAILURE;
    SSMResource *res;
    SSMControlConnection *ctrlconn = NULL;

#ifdef ALLOW_STANDALONE
    if (standalone)
    {
        PR_ASSERT(standalone_conn);
        req->ctrlconn = standalone_conn;
        SSM_DEBUG("authenticate: Standalone mode, passing through.\n");
        return SSM_SUCCESS;
    }
#endif
    /* do we have auth information? */
    if ((req->ctrlrid) && (req->password))
    {
        /* Get the control connection object. */
        rv = SSM_RIDTextToResource(req, req->ctrlrid, &res);
        if (rv == SSM_SUCCESS)
        {
            ctrlconn = (SSMControlConnection *) res;
            req->ctrlconn = ctrlconn;
            /* Verify restype and nonce. */
            if ((!SSM_IsAKindOf(res, SSM_RESTYPE_CONTROL_CONNECTION))
                || (PL_strcmp(req->password, ctrlconn->m_nonce)))
            {
                SSM_DEBUG("------------------------------\n");
                SSM_DEBUG("Password doesn't match nonce.\n");
                SSM_DEBUG("Nonce: %s\n", ctrlconn->m_nonce);
                SSM_DEBUG("Submitted password: %s\n", req->password);
                SSM_DEBUG("------------------------------\n");
                PR_ASSERT(!"Couldn't authenticate this connection\n");
                rv = SSM_FAILURE;
            }
        }
        else
            SSM_DEBUG("authenticate: Couldn't find a control connection.\n");
    }
    else
    {
        if (!req->ctrlrid)
            SSM_DEBUG("authenticate: No ctrlrid available.\n");
        else if (!req->password)
            SSM_DEBUG("authenticate: No password available.\n");
    }

    if (rv != SSM_SUCCESS)
        req->httprv = HTTP_UNAUTHORIZED;
    else
        req->ctrlconn = ctrlconn; /* type must check out */
    return rv;
}

/* Returns the suffix of the filename, if it exists. */
char *
http_find_suffix(HTTPRequest *req)
{
    char *result = NULL;
    char *c;

    c = req->filename;
    if (!c) return result;

    while(*c && (*c != '.'))
        c++;
    if (*c)
        result = ++c;
    else
        result = NULL;

    return result;
}

static SSMStatus
http_spool_file(HTTPRequest *req)
{
    SSMStatus rv = SSM_FAILURE;
    char *path = NULL;
    PRFileDesc *fl = NULL;
    char *type;
    char buf[256];
    PRInt32 numbytes;

    path = PR_smprintf("%s%s", PSM_DOC_DIR, req->handlerName);
    if (!path) 
    {
        rv = PR_GetError();
        goto done;
    }

    fl = PR_Open(path, PR_RDONLY, 0);
    if (!fl)
    {
        rv = PR_GetError();
        goto done;
    }

    /* We have the file open. Send the file type. */

    if (PL_strstr(path, ".gif") || PL_strstr(path, ".GIF"))
        type = "image/gif";
    else
        type = "text/html";

    rv = SSM_HTTPSendOKHeader(req, "", type);

    /* Send its contents back to the user. */
    while(1)
    {
        PRInt32 frv;
        
        numbytes = PR_Read(fl, buf, 256);
        if (numbytes <= 0)
            break; /* done reading */
        frv = PR_Write(req->sock, buf, numbytes);
        PR_ASSERT(numbytes == frv);
    }

    if (numbytes == 0)
        rv = SSM_SUCCESS;

 done:
    if (fl)
        PR_Close(fl);
    if (path)
        PR_Free(path);
    return rv;
}

/* Based on the file requested, do some function within Cartman.
   ### mwelch This NEEDS to be changed to use a string-keyed hash table,
              so that people adding code can add their own functionality
              independently of this module. */
static SSMStatus
http_dispatch(HTTPRequest *req)
{
    SSMStatus rv = SSM_SUCCESS;
    SSMCommandHandlerFunc handlerFunc = NULL;

    /* Look up the command handler in the registry. */
    rv = SSM_HTTPFindCommandHandler(req->handlerName, &handlerFunc);
    if (rv != SSM_SUCCESS)
    {
        SSM_HTTPReportSpecificError(req, "Could not find handler for command '%s' (error %d).", req->handlerName, rv);
    }

    if (!handlerFunc)
    {
        /* Attempt to fetch a file from the doc directory. */
        rv = http_spool_file(req);
        if (rv != SSM_SUCCESS)
            handlerFunc = SSM_HTTPDefaultCommandHandler; /* fallback */
        else
            req->sentResponse = PR_TRUE;
    }

    if (handlerFunc)
        rv = (*handlerFunc)(req);

    return rv;
}

static void
http_find_target(HTTPRequest *req)
{
    SSMResource *target = NULL;
    char *targetID = NULL;
    SSMStatus rv;

    /* Attempt to find a target resource. If one is not available, use
       the authenticated control connection. */
    rv = SSM_HTTPParamValue(req, "target", &targetID);
    if (rv == SSM_SUCCESS){
        rv = SSM_RIDTextToResource(req, targetID, &target);
    } else if (req->referer != NULL){
        /* See if the referer had a target.  If it did, then use it's
         * target as this request's target.
         */
        char *refTarget = strstr(req->referer, "target");
        if (refTarget != NULL) {
            refTarget = strchr(refTarget, '=');
            if (refTarget != NULL) {
                char ridText[20];
                char *nextParam;
                
                refTarget++;
                nextParam = strchr(refTarget, '&');
                if (nextParam != NULL) {
                    memcpy(ridText, refTarget, nextParam-refTarget);
                    ridText[nextParam-refTarget] = '\0';
                } else {
                    memcpy(ridText, refTarget, strlen(refTarget)+1);
                }
                SSM_RIDTextToResource(req, ridText, &target);
            }
        }

    }
    /*
     * SSM_RIDTextToResource gets a reference for us, so there's no
     * need to get another one here.
     */
    req->target = target;
}

/* Take care of an incoming request. */
static void
http_handle_request(void *arg)
{
    SSMStatus rv = SSM_SUCCESS;
    HTTPRequest *req;
    PRFileDesc *sock = (PRFileDesc *) arg;
    char *name = NULL;

    SSM_RegisterThread("http server", NULL);
    SSM_DEBUG("Responding to request.\n");

    /* Read in the request. */
    if ((rv = http_read_request(sock, &req)) != SSM_SUCCESS) 
        goto loser;

#ifdef DEBUG
    name = PR_smprintf("http server %lx", (long) req);
    if (name)
        SSM_RegisterThread(name, NULL);
#endif

    /* Parse the buffer, get interesting bits */
    if ((rv = http_parse_request(req)) != SSM_SUCCESS) 
        goto loser;

    /* Attempt to authenticate the connection. */
    if ((rv = http_authenticate(req)) != SSM_SUCCESS) 
        goto loser;

    /* Find the target object, if one has been explicitly specified. */
    http_find_target(req);

#if 0 /* XP_MAC */
	/* Test pattern for now. */
	rv = SSM_HTTPSendUTF8String(req, "Hi, I got all of your request.\r\n");
	rv = SSM_HTTPSendUTF8String(req, req->rawreqbuf);
	goto done;
#endif

    /* Do the right thing depending on what has been requested. */
    if ((rv = http_dispatch(req)) != SSM_SUCCESS) 
        goto loser;

    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
done:
    if (req && !req->sentResponse)
    {
        /* An error occurred along the way. Send the appropriate
           error response, or an internal server error if nothing
           was set along the way. */
        if (req->httprv <= HTTP_OK)
            req->httprv = HTTP_NOT_IMPLEMENTED;
        SSM_HTTPReportError(req, req->httprv);
    }

    if (req) 
        http_request_destroy(req);
    else
        SSM_DEBUG("Would destroy request, but it doesn't exist!\n");

    SSM_DEBUG("Closing, status %ld.\n", rv);
#ifdef DEBUG
    PR_Free(name);
#endif    
}

/* SSM_HTTPThread listens to the HTTP socket for connections from
   one or more clients. (arg) should be an open socket (PRFileDesc*),
   therefore the port needs to have been created before being passed 
   to this thread. */
void SSM_HTTPThread(void *arg)
{
    PRFileDesc *listenSock = (PRFileDesc *) arg;
    PRFileDesc *acceptSock = NULL;
    PRNetAddr clientaddr;
    PRBool alive = PR_TRUE;
    SSMStatus rv = SSM_SUCCESS;
    
    SSM_RegisterThread("http listener", NULL);

    /* Wait for connections. */
    while(alive)
    {
        SSM_DEBUG("Ready for connection on port %ld.\n", (long) httpListenPort);

        acceptSock = PR_Accept(listenSock, &clientaddr, 
                               PR_INTERVAL_NO_TIMEOUT);
        if (acceptSock)
        {
            PRThread *handler;
            handler = SSM_CreateAndRegisterThread(PR_USER_THREAD,
                                      http_handle_request,
                                      (void *)acceptSock,
                                      PR_PRIORITY_NORMAL,
                                      PR_LOCAL_THREAD,
                                      PR_UNJOINABLE_THREAD, 0);
            if (!handler)
            {
                SSM_DEBUG("Couldn't create handler for new socket %p.",
                          acceptSock);
                alive = PR_FALSE;
            }
        }
        else
            alive = PR_FALSE;
    }
    rv = PR_GetError();
    /* Close down in an orderly fashion. */
#ifdef DEBUG
    SSM_DEBUG("http: ** Closing, status = %d **\n", rv);
#endif    
}

SSMStatus
SSM_HTTPSendUTF8String(HTTPRequest *req, char *str)
{
    PRInt32 len;
    PRInt32 numSent;
    SSMStatus rv = SSM_FAILURE;
    SSM_DEBUG("Sending the following string to the client:\n%s\n",str);
    /* Extract the text from the UnicodeString and send it. */
    if (str)
    {
        len = (PRInt32)PL_strlen(str);
        
        numSent = PR_Send(req->sock, str, len, 0, 
                          PR_INTERVAL_NO_TIMEOUT);
        if (numSent == len)
            rv = SSM_SUCCESS;
        else {
            SSM_DEBUG("Error %d trying to send unicode string.\n", 
                      PR_GetError());
        }
    }
    return rv;
}

SSMStatus
SSM_RIDTextToResource(HTTPRequest *req, const char *ridText,
                      SSMResource **res)
{
    SSMResourceID rid = 0;
    SSMStatus rv = PR_FAILURE;

    *res = NULL; /* in case we fail */

    /* scan the text for a number. */
    PR_sscanf(ridText, "%ld", &rid);

    /* search for the resource in the authenticated control connection. */
    if (req->ctrlconn) {
        rv = SSMControlConnection_GetResource(req->ctrlconn, rid, res);
    } else {
        SSMControlConnection *ctrl;

        rv = SSM_GetControlConnection(rid, &ctrl);
        *res = &ctrl->super.super;
    }

    return ((SSMStatus) rv);
}

SSMStatus
SSM_HTTPReportError(HTTPRequest *req, HTTPErrorCode statusCode)
{
    char key[256];
    char *name = NULL, *hdrs = NULL, *desc = NULL;
    char *tmpl = NULL, *out = NULL;
    char *errmsg = req->errormsg;
    char *noSpecificErr = NULL;
    SSMStatus rv = SSM_SUCCESS;
    SSMTextGenContext *cx=NULL;

    if (statusCode == HTTP_OK)
        goto done;

    rv = SSMTextGen_NewTopLevelContext(req, &cx);
    if (rv != SSM_SUCCESS)
        goto loser;

    /* Grab "http_error_%d_name", "http_error_%d_hdrs", and
       "http_error_%d_desc" for the specific text. */
    PR_snprintf(key, 256, "http_error_%d_name", statusCode);
    rv = SSM_GetAndExpandText(cx, key, &name);
    if (rv != SSM_SUCCESS)
        goto loser;

    if (statusCode != HTTP_NO_CONTENT) {
        PR_snprintf(key, 256, "http_error_%d_hdrs", statusCode);
        rv = SSM_GetAndExpandText(cx, key, &hdrs);
        if (rv != SSM_SUCCESS)
            goto loser;
        PR_snprintf(key, 256, "http_error_%d_desc", statusCode);
        rv = SSM_GetAndExpandText(cx, key, &desc);
        if (rv != SSM_SUCCESS)
            goto loser;
    } else {
        desc = "";
        hdrs = "";
    }
    /* Now that we have the strings to insert, get the header template. */
    rv = SSM_GetAndExpandText(cx, "http_error_header_template", &tmpl);
    if (rv != SSM_SUCCESS)
        goto loser;
    
    /* If no specific error message was reported, report generically. */
    if (!errmsg)
    {
        rv = SSM_GetAndExpandText(cx, "http_error_no_spec_err", 
                                  &noSpecificErr);
        if (rv != SSM_SUCCESS) {
            errmsg = "";
        } else {
            errmsg = noSpecificErr;
        }
    }

    out = PR_smprintf(tmpl, (long)statusCode, name, hdrs, desc, errmsg);
    /* Send the result string. */
    rv = SSM_HTTPSendUTF8String(req, out);
    PR_Free(tmpl);
    tmpl = NULL; /* So we don't free it twice. */
    if (rv != SSM_SUCCESS)
        goto loser;

    /* Empty the string in preparation for the next part of the page */
    SSMTextGen_UTF8StringClear(&out);

    if (statusCode != HTTP_NO_CONTENT)
    {		
        /* Get and send the appropriate description. */
        /* Format with the same arguments as before. */
        rv = SSM_GetAndExpandText(cx, "http_error_content", &tmpl);
        if (rv != SSM_SUCCESS)
            goto loser;

        out = PR_smprintf(tmpl, statusCode, name, hdrs, desc, errmsg);
        rv = SSM_HTTPSendUTF8String(req, out);
        if (rv != SSM_SUCCESS)
            goto loser;
    }

    req->sentResponse = PR_TRUE; 
    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
 done:
    PR_FREEIF(name);
    if (statusCode != HTTP_NO_CONTENT) {
        PR_FREEIF(desc);
        PR_FREEIF(hdrs);
    }
    PR_FREEIF(tmpl);
    PR_FREEIF(noSpecificErr);
    PR_FREEIF(out);
    if (cx != NULL) {
        SSMTextGen_DestroyContext(cx);
    }
    return rv;
}

SSMStatus
SSM_HTTPSendOKHeader(HTTPRequest *req, 
                     char *addtlHeaders,
                     char *contentType)
{
    char *tmpl = NULL, *out = NULL;
    char *hdrs = addtlHeaders, *type = contentType;
    SSMStatus rv = SSM_SUCCESS;
    SSMTextGenContext *cx;

    req->httprv = HTTP_OK;

    /* Create a textgen context so that we can process the templates. */
    rv = SSMTextGen_NewTopLevelContext(req, &cx);
    if (rv != SSM_SUCCESS)
        goto loser;

    /* Get the header template. */
    rv = SSM_GetAndExpandText(cx, "http_header_template", &tmpl);
    if (rv != SSM_SUCCESS)
        goto loser;

    /* prevent null headers from interfering with content-type header */
    if (hdrs == NULL)
        hdrs = "";
    if (type == NULL)
        type = "text/html";
    out = PR_smprintf(tmpl, hdrs, type);
    /* Send the result string. */
    rv = SSM_HTTPSendUTF8String(req, out);

    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
 done:
    if (tmpl)
        PR_Free(tmpl);
    if (out)
        PR_Free(out);
    if (cx)
        SSMTextGen_DestroyContext(cx);
    return rv;
}

/*  
    Simple Hello World command handler. I've deliberately left Unicode
    out of this example, in order to illustrate the use of command
    handlers in isolation.
    
    Most of the time, you will not even need to explicitly send anything
    out on the HTTP connection. Most of the time, you will probably 
    want to write simple handlers that perform specific actions inside 
    Cartman (such as making changes to the cert/key db, performing 
    some operation such as form signing, etc), then sending back a 
    generic response by calling SSM_HTTPDefaultCommandHandler. This should
    even be true for special content that you construct programmatically,
    such as cert lists; this is because you will want to write keyword
    handlers (see nlsutil.[ch]) to perform such processing inline.
    
    See SSM_HTTPMonitorResourceHandler for an example of the kind of
    handler you will typically want to write. (I've tried to comment
    that routine extensively so that you will get an idea of what
    it is doing.)  
*/
SSMStatus
SSM_HTTPHelloWorld(HTTPRequest *req)
{
    size_t sent;
    char outbuf[256];

    /* 
       Send HTTP headers first. req->sock is an NSPR socket on which
       we send back the response. Note: Normally you would not send
       things back down the raw socket. If you want to send something
       other than a canned HTML page, there are convenience
       routines that you'll use to send UnicodeStrings down the socket,
       as well as canned generic headers. You'll see that in the NLS
       Hello World example below.
    */
    PR_snprintf(outbuf, 256, 
                "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
    sent = PR_Send(req->sock, outbuf, strlen(outbuf), 
                   0, PR_INTERVAL_NO_TIMEOUT);

    /* Now, send some content down the same socket. */
    PR_snprintf(outbuf, 256,
                "<HTML><HEAD><TITLE>Hello World</TITLE></HEAD>"
                "<BODY><h1>Hello, world!</h1>"
                "<p>This is a static string within Netscape Security Manager."
                "</p><br><br><hr>"
                "</BODY></HTML>");
    sent = PR_Send(req->sock, outbuf, strlen(outbuf), 
                   0, PR_INTERVAL_NO_TIMEOUT);
    
    /* This next line tells the HTTP dispatcher that we've already
       sent a response back to the user, so no default response is
       needed. */
    req->sentResponse = PR_TRUE;

    /* That's it. */
    return SSM_SUCCESS;
}

/* 
   libnls Hello World example. This is about as complex as
   the "simple" Hello World example shown above. However,
   this example grabs string resources from the properties
   file and sends them back to the user.
*/
SSMStatus
SSM_HTTPHelloWorldWithNLS(HTTPRequest *req)
{
    char *type = NULL, *content = NULL;
    SSMTextGenContext *cx = NULL;
    SSMStatus rv;

    /* 
       Grab the "hello_type" string from the properties file.
       We'll use this when sending the canned HTTP header. 
       The first parameter is a resource object which (when
       non-NULL) provides parameters for formatting the text.
       However, we can pass NULL if we know that the text
       we're extracting doesn't have any NLS formatting parameters
       inside (numeric params such as {0}, {1}, etc).

       See nsm.properties.in for the definition of the "hello_type"
       and "hello_content" strings. 
    */
    SSM_DEBUG("Getting type string.\n");
    rv = SSMTextGen_NewTopLevelContext(req, &cx);
    if (rv != SSM_SUCCESS)
        goto loser;
    
    rv = SSM_GetUTF8Text(cx, "hello_type", &type);
    if (rv != SSM_SUCCESS)
        goto loser;
    
    /* Grab the property string containing the content we want
       ("hello_content"). */
    SSM_DEBUG("Getting content string.\n");
    rv = SSM_GetUTF8Text(cx, "hello_content", &content);
    if (rv != SSM_SUCCESS)
        goto loser;

    /* 
       Send HTTP headers, using a standard template that I've
       already hacked up. You pass in the HTTPRequest object,
       along with the content type and any additional headers
       you may want to embed. You can pass NULL for the content
       type or the header parameter. If you pass a NULL content
       type, "text/html" will be used.
    */
    SSM_DEBUG("Sending OK header.\n");
    rv = SSM_HTTPSendOKHeader(req, NULL, type); /* no addt'l headers */
    if (rv != SSM_SUCCESS)
        goto loser;

    /* 
       Send the content string. I wrote a convenience routine
       called SSM_SendUTF8String which, given an HTTPRequest
       and a UnicodeString, sends the UnicodeString in the
       requested locale. (Well, it will, once I make sure that
       it works with locales other than en_US. :-)
    */
    SSM_DEBUG("Sending actual content text.\n");
    rv = SSM_HTTPSendUTF8String(req, content);


    /* As in the first example, tell the HTTP dispatcher that we've 
       already sent a response back to the user, so no default 
       response is needed. */
    req->sentResponse = PR_TRUE;

    /* That's it. */
 loser:
    /* Clean up after ourselves. */
    PR_FREEIF(type);
    PR_FREEIF(content);
    if (cx)
        SSMTextGen_DestroyContext(cx);

    SSM_DEBUG("return value is %ld.\n", (long) rv);

    return rv;
}


SSMStatus
SSM_HTTPGetGenericLump(HTTPRequest *req, PRBool binary)
{
    SSMResource *target;
    SSMStatus rv = SSM_SUCCESS;
    char *baseRef;
    char name[256], *type_ch = NULL;
    char *type = NULL, *hdrs = NULL, *content = NULL;
    SSMTextGenContext *cx = NULL;

    rv = SSMTextGen_NewTopLevelContext(req, &cx);
    if (rv != SSM_SUCCESS)
    {
        SSM_HTTPReportSpecificError(req, "HTTPGetGenericLump: Error %d attempting to create textgen context.", rv);
        goto loser;
    }
    
    /* Get target object. */
    target = SSMTextGen_GetTargetObject(cx);

    rv = SSM_HTTPParamValue(req, "baseRef", &baseRef);
    if (rv != SSM_SUCCESS)
    {
        req->httprv = HTTP_BAD_REQUEST;
        goto loser;
    }
    /* Use the baseRef parameter to grab two strings from the 
       property files. */

    PR_snprintf(name, 256, "%s_type", baseRef);
    rv = SSM_GetUTF8Text(cx, name, &type);
    if (rv != SSM_SUCCESS)
        goto loser;

    /* If the content type of this lump is not "text / *", 
       set binary flag true. */
    type_ch = type;
    if (type_ch && strncmp(type_ch, "text", 4))
        binary = PR_TRUE;

    PR_snprintf(name, 256, "%s_hdrs", baseRef);
    rv = SSM_GetUTF8Text(cx, name, &hdrs);
    if (rv != SSM_SUCCESS)
	hdrs = NULL;

    PR_snprintf(name, 256, "%s_content", baseRef);
    rv = SSM_GetUTF8Text(cx, name, &content);
    if (rv != SSM_SUCCESS)
        goto loser;
    
    /* Send headers, followed by content. */
    rv = SSM_HTTPSendOKHeader(req, hdrs, type);
    if (rv != SSM_SUCCESS)
        goto loser;


    if (binary)
    {
        SECItem item = {siBuffer, NULL, 0};
        PRInt32 numSent;

        /* Decode base64 content from the unicode string,
           then send the binary bits. */
        if (ATOB_ConvertAsciiToItem(&item, content) != SECSuccess)
            goto loser;

        numSent = PR_Send(req->sock, item.data, item.len, 
                          0, PR_INTERVAL_NO_TIMEOUT);
        PR_Free(item.data);
        if (numSent != item.len) /* error of some sort */
            goto loser;
    }
    else
    {
        rv = SSM_HTTPSendUTF8String(req, content);
        if (rv != SSM_SUCCESS)
            goto loser;
    }

    req->sentResponse = PR_TRUE;

    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
 done:
    PR_FREEIF(type);
    PR_FREEIF(hdrs);
    PR_FREEIF(content);
    if (cx != NULL) {
        SSMTextGen_DestroyContext(cx);
    }
    return rv;
}

SSMStatus
SSM_HTTPGetBinaryHandler(HTTPRequest *req)
{
    return SSM_HTTPGetGenericLump(req, PR_TRUE);
}

SSMStatus
SSM_HTTPDefaultCommandHandler(HTTPRequest *req)
{
    return SSM_HTTPGetGenericLump(req, PR_FALSE);
}

SSMStatus SSM_HTTPCloseAndSleep(HTTPRequest* req)
{
    SSMStatus rv = SSM_HTTPGetGenericLump(req, PR_FALSE);
    /* ### sjlee: this unfortunate hack is due to the fact that bad things
     *            happen if we do not finish closing the window before
     *            another window pops up: this will be supplanted by a
     *            more complete fix later
     */
    PR_Sleep(PR_TicksPerSecond()*1);

    return rv;
}

/* 
   Command handler for monitoring resources. This is used by the keygen
   dialog, where one of the frames causes this handler to run, then
   after the keygen context is finished processing keygen requests,
   the dialog is closed by javascript content sent back by this
   handler.
*/
SSMStatus 
SSM_HTTPMonitorResourceHandler(HTTPRequest *req)
{
    SSMResource *target = NULL;
    SSMStatus rv;

    /* Get the target resource. */
    target = (req->target ? req->target : (SSMResource *) req->ctrlconn);

    if (target)
    {
        /* Get a reference on the target object so that it doesn't 
           get deleted from underneath us. */
        SSM_LockResource(target);
        SSM_GetResourceReference(target);
        SSM_UnlockResource(target);
		
        /* if this is a keygen context, post an urgent open message. */
        /* ### mwelch Very hacky. 
           Should make a more specific handler for this. */
        if (SSM_IsAKindOf(target, SSM_RESTYPE_KEYGEN_CONTEXT))
        {
            SSMKeyGenContext *ct = (SSMKeyGenContext *) target;
            if (ct->m_ctxtype == SSM_CRMF_KEYGEN) {
              SSM_DEBUG("Posting open message on keygen queue.\n");
              SSM_SendQMessage(ct->m_incomingQ, SSM_PRIORITY_SHUTDOWN,
                             SSM_DATA_PROVIDER_OPEN, 0, NULL,
                             PR_TRUE);
            }
            /* Now, wait for something/someone to shut down 
               the target object's threads. */
            /* Currently only KEYGEN_CONTEXT supports this, so if it
             * isn't a KEYGEN_CONTEXT, don't block 'cause bad things 
             * may happen.
             */
            SSM_WaitForResourceShutdown(target);
        }
        else{
            PR_Sleep(PR_TicksPerSecond());
        }
        SSM_FreeResource(target);
    }

    /* Call the default HTTP handler to send out the requested page. */
    rv = SSM_HTTPDefaultCommandHandler(req);

    /* All done. */
    return rv;
}

/* Handler which shuts down a resource. */
SSMStatus 
SSM_HTTPShutdownResourceHandler(HTTPRequest *req)
{
    SSMResource *target;
    SSMStatus rv;

    target = (req->target ? req->target : (SSMResource *) req->ctrlconn);

    if (target)
    {
        /* Get a reference on the object so that it doesn't get deleted
           from underneath us. */
        SSM_LockResource(target);
        SSM_GetResourceReference(target);
        SSM_UnlockResource(target);
		
        /* if this is a keygen context, post an urgent open message */
        /* ### mwelch Very hacky. 
           Should make a specific handler for this. */
        if (SSM_IsAKindOf(target, SSM_RESTYPE_KEYGEN_CONTEXT))
        {
            SSM_DEBUG("Closing down key gen.\n");
            SSMKeyGenContext_CancelKeyGen((SSMKeyGenContext *)target);
        }
    }

    /* Call the default handler to send out the requested page. */
    rv = SSM_HTTPDefaultCommandHandler(req);
    SSM_FreeResource(target);
    return rv;
}

/* Find a command handler by name. */
SSMStatus
SSM_HTTPFindCommandHandler(const char *name, SSMCommandHandlerFunc *func)
{
    CommandHandlerEntry *e, *found = NULL;
    PRIntn i;
    PRIntn numEntries = SSM_Count(http_handlers);

    *func = NULL; /* in case we fail */

    if (name)
    {
        for(i=0;i<numEntries;i++)
        {
            e = (CommandHandlerEntry *) SSM_At(http_handlers, i);
            if (!PL_strcmp(e->key, name))
            {
                found = e;
                break;
            }
        }
    }

    if (found)
        *func = found->func;

    return (*func ? SSM_SUCCESS : SSM_FAILURE);
}

/* Register a command handler. */
SSMStatus 
SSM_HTTPRegisterCommandHandler(const char *name, 
                               SSMCommandHandlerFunc func)
{
    SSMStatus rv = SSM_SUCCESS;
    CommandHandlerEntry *ent = NULL;

    ent = (CommandHandlerEntry *) PR_CALLOC(sizeof(CommandHandlerEntry));
    if (!ent)
        goto loser;
    ent->key = name;
    ent->func = func;
    
    SSM_Enqueue(http_handlers, SSM_PRIORITY_NORMAL, ent);

    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
    PR_FREEIF(ent);
 done:
    return rv;
}

SSMStatus
http_init_handlers(void)
{
    SSMStatus rv = SSM_SUCCESS;

    http_handlers = SSM_NewCollection();
    if (!http_handlers)
    {
        PR_ASSERT(!"Could not create HTTP handler registry.");
        goto loser;
    }

    rv = http_register_handlers();
    if (rv != SSM_SUCCESS)
        goto loser;

    goto done;
 loser:
    if (rv == SSM_SUCCESS)
        rv = SSM_FAILURE;
 done:
    return rv;
}

SSMStatus SSM_InitHTTP(void)
{
    PRFileDesc *httpListenSocket = NULL;
    SSMStatus rv = SSM_SUCCESS;
    PRNetAddr httpAddr;

#ifdef ALLOW_STANDALONE
    char *saEnv = NULL;

    /* Determine if we're in standalone mode. */
    saEnv = PR_GetEnv(SSM_ENV_STANDALONE);
    if (saEnv)
        standalone = 1;
#endif

    /* Initialize command handlers. */
    rv = http_init_handlers();
    if (rv != SSM_SUCCESS)
        goto loser;

#ifdef ALLOW_STANDALONE
    if (standalone)
    {
        SSM_DEBUG("*** STANDALONE MODE ***\n");
        SSM_DEBUG("Opening a socket on port %d.\n", SSM_STANDALONE_HTTP_PORT);
        httpListenPort = SSM_STANDALONE_HTTP_PORT;

        httpListenSocket = PR_NewTCPSocket();
        if (!httpListenSocket) 
            goto loser;

        /* bind to PSM http port on loopback address: connections from
         * non-localhosts will be disallowed
         */
        rv = PR_InitializeNetAddr(PR_IpAddrLoopback, 
                                  (PRUint16) SSM_STANDALONE_HTTP_PORT,
                                  &httpAddr);
        if (rv != PR_SUCCESS)
            goto loser;

        rv = PR_Bind(httpListenSocket, &httpAddr);
        if (rv != PR_SUCCESS)
            goto loser;
        rv = PR_Listen(httpListenSocket, MAX_CONNECTIONS);
        if (rv != PR_SUCCESS)
            goto loser;

        SSM_DEBUG("Creating a control connection for standalone mode.\n");
        rv = SSMControlConnection_Create(NULL, NULL, 
                                         (SSMResource **) &standalone_conn);
        if (rv != PR_SUCCESS)
            goto loser;
        
        /* We have to invoke the Hello handler ourselves, since there
           will be no Hello message forthcoming. */

        /* Set up profile and password information. */
        if (standalone_conn->m_profileName == NULL)
        {
            /* For now, use a default of "cartman". */
            standalone_conn->m_profileName = PL_strdup("cartman");
            PR_ASSERT(standalone_conn->m_profileName);
        }

        rv = SSMControlConnection_SetupNSS(standalone_conn);
        if (rv != PR_SUCCESS)
            goto loser;
    }
    else
    {
#endif
        /* Open a port for HTTP connections. Can be any port. */
        httpListenSocket = SSM_OpenPort();
        if (!httpListenSocket) 
            goto loser;

        /* Publish the port number for client use */
        rv = PR_GetSockName(httpListenSocket, &httpAddr);
        if (rv != PR_SUCCESS) 
            goto loser;
        httpListenPort = (PRUint32) PR_ntohs(httpAddr.inet.port);
#ifdef ALLOW_STANDALONE
    }
#endif

    /* Start listening for HTTP connections. */
    httpListenThread = SSM_CreateAndRegisterThread(PR_USER_THREAD,
                                       SSM_HTTPThread,
                                       (void *)httpListenSocket,
                                       PR_PRIORITY_NORMAL,
                                       PR_LOCAL_THREAD,
                                       PR_UNJOINABLE_THREAD, 0);

    if (!httpListenThread) 
        goto loser;

    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
    if (httpListenSocket)
        PR_Close(httpListenSocket);
    if (httpListenPort)
        httpListenPort = 0;
 done:
    
    return rv;
}

PRUint32 SSM_GetHTTPPort(void)
{
    return httpListenPort;
}

SSMStatus
SSM_AccessPtToURL(SSMDialogSelector sel, char *url)
{
    return PR_SUCCESS;
}

SSMStatus
SSM_GenerateURL(SSMControlConnection *conn,
                char *command,
                char *baseRef, /* can pass NULL for this (but don't do that) */
                SSMResource *target, /* can pass NULL for this */
                char *otherParams, /* can pass NULL for this */
                PRUint32 *width, PRUint32 *height,
                char **url)
{
    char *result = NULL;
    char *targetParam = NULL;
    char *baseRefParam = NULL;
    char *auth = NULL;
    char * htmlParams = NULL;
    SSMStatus rv = SSM_SUCCESS;
    SSMTextGenContext *cx=NULL;

    /* in case we fail */
    *url = NULL;
    *width = 0;
    *height = 0;

    if (conn)
    {
        auth = PR_smprintf("%d:%s@", conn->super.super.m_id, conn->m_nonce);
        if (!auth) goto loser;
    }

    rv = SSMTextGen_NewTopLevelContext(NULL, &cx);
    if (rv != SSM_SUCCESS)
        goto loser;

    if ((!target) && (conn))
        /* target == control connection by default */
        target = &conn->super.super;

    if (baseRef)
        baseRefParam = PR_smprintf("baseRef=%s", baseRef);

    if (target)
        targetParam = PR_smprintf("%starget=%ld",
                                  baseRefParam ? "&" : "",
                                  target->m_id);

    if (otherParams) {
        char * params = PL_strdup(otherParams);
        char * name = NULL, * value = NULL;
        char * result = NULL, * tmp;
        char * htmlname=NULL, * htmlvalue=NULL;
        
        name = strtok(params, "=");
        value = strtok(NULL, "&");
        
        while (name && value)
            {
                htmlname = SSM_ConvertStringToHTMLString(name);
                htmlvalue = SSM_ConvertStringToHTMLString(value);
                if (!htmlname || !htmlvalue) {
                    PR_FREEIF(htmlname);
                    PR_FREEIF(htmlvalue); 
                    goto loser;
                } 
                  
                tmp = PR_smprintf("%s%s%s=%s", result?result:"", result?"&":"", 
				   htmlname, htmlvalue);
                PR_FREEIF(result);
                result = tmp;
                name = strtok(NULL, "=");
                value = strtok(NULL, "&");
            }
        PR_Free(params);
        htmlParams = result;
    }

    
    /* ### mwelch - We should do a gethost* to figure out the real
       IP address. */
    result = PR_smprintf("http://%s127.0.0.1:%d/%s?%s%s%s%s",
                         auth ? auth : "",
                         httpListenPort, 
                         command,
                         baseRefParam ? baseRefParam : "",
                         targetParam ? targetParam : "",
                         ((baseRefParam || targetParam) 
                          && otherParams) ? "&" : "",
                         otherParams ? htmlParams : "");
    if (!result) goto loser;

    SSM_DEBUG("Generated URL %s\n", result);

    /* Get width and height, if they are available. We look for strings
       (baseRef)_width and (baseRef)_height in the properties file. */
    if (baseRef && conn)
    {
        char key[256];

        PR_snprintf(key, 256, "%s_width", baseRef);
        rv = SSM_GetNumber(cx, key, (PRInt32 *) width);
        if (rv != SSM_SUCCESS)
            goto loser;
        
        PR_snprintf(key, 256, "%s_height", baseRef);
        rv = SSM_GetNumber(cx, key, (PRInt32 *) height);
        if (rv != SSM_SUCCESS)
            goto loser;
    }

    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = PR_OUT_OF_MEMORY_ERROR;
    PR_FREEIF(result);
 done:
    PR_FREEIF(htmlParams);
    PR_FREEIF(baseRefParam);
    PR_FREEIF(targetParam);
    if (url) *url = result;
    if (cx != NULL) {
        SSMTextGen_DestroyContext(cx);
    }
    PR_FREEIF(auth);
    
    return rv;
}

void
SSM_HTTPReportSpecificError(HTTPRequest *req, char *fmt, ...)
{
    char *msg = NULL;
    va_list argp;
    PRExplodedTime exploded;
    char* timeStamp = NULL;
    char *finalmsg = NULL;
    char *tmp = NULL;

    va_start(argp, fmt);
    msg = PR_vsmprintf(fmt, argp);
    va_end(argp);
    if (!msg)
        goto loser;

    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &exploded);
    timeStamp = PR_smprintf("%02i:%02i:%02i", exploded.tm_hour, 
                            exploded.tm_min, 
                            exploded.tm_sec);
    if (!timeStamp)
        goto loser;

    /* Check (req) param after getting the specific message and timestamp,
       because we can still report it in the log. */
    if (!req)
        goto loser;

    finalmsg = PR_smprintf("[%s] %s", timeStamp, msg);
    if (!finalmsg)
        goto loser;
    tmp = req->errormsg;
    req->errormsg = PR_smprintf("%s%s", (tmp) ? tmp : "", finalmsg);
    if (req->errormsg == NULL) {
        goto loser;
    }
    PR_FREEIF(tmp);
    tmp = NULL;
    goto done;
loser:
    SSM_DEBUG("Couldn't report the following error message to the HTTP client.\n");
done:
    if (msg)
    {
        SSM_DEBUG("HTTP error msg: %s\n", msg);
        PR_smprintf_free(msg);
    }
    if (timeStamp)
        PR_smprintf_free(timeStamp);
    if (finalmsg)
        PR_smprintf_free(finalmsg);
    if (tmp != NULL)
        PR_smprintf_free(tmp);
}

SSMStatus SSM_HTTPHandleOKCancelClick(HTTPRequest *req, PRBool closeIfCancel)
{
    SSMResource *res = NULL;
    char        *buttonValue;
    SSMStatus   rv;

    res = (req->target) ? req->target : (SSMResource *)req->ctrlconn;
     /* First let's figure out which button was pressed */
    rv = SSM_HTTPParamValue(req, "OK", &buttonValue);
    if (rv == SSM_SUCCESS) {
        res->m_buttonType = SSM_BUTTON_OK;
    } else {
        rv = SSM_HTTPParamValue(req, "Cancel", &buttonValue);
        if (rv == SSM_SUCCESS) {
            res->m_buttonType = SSM_BUTTON_CANCEL;
        }
    }
    if (rv != SSM_SUCCESS) {
        /* Could not find either OK or Cancel, something went really
         * wrong
         */
        rv = SSM_ERR_NO_BUTTON;
        goto loser;
    }
    rv = SSM_HTTPParamValue(req, "formName", &buttonValue);
    if (rv != SSM_SUCCESS || buttonValue == NULL) {
        /*
         * The form name was not included in the form to submit.
         */
        rv = SSM_ERR_NO_FORM_NAME;
    }
    if (res->m_formName != NULL) {
        /* Free any previously allocated strings. */
        PR_Free(res->m_formName);
    }
    res->m_formName = PL_strdup(buttonValue);
    /* Fall thru on success */
 loser:
    if (res->m_buttonType == SSM_BUTTON_CANCEL && closeIfCancel) {
        rv = SSM_HTTPCloseWindow(req);
        /*
         * Usually some other function is called to notify the resource,
         * but this makes that no longer true, so let's do it here.
         */
        SSM_LockResource(res);
        SSM_NotifyResource(res);
        SSM_UnlockResource(res);
    } else {
        rv = SSM_HTTPDefaultCommandHandler(req);
    }
    /* There should be some thread waiting on this event, notify it
     * now that we've processed the button press
     */
    SSM_NotifyOKCancelEvent(res);
    return rv;   
}

SSMStatus SSM_HTTPOKCancelHandler(HTTPRequest *req)
{
    SSMResource *target = NULL;
    SSMStatus    rv=SSM_ERR_NO_TARGET;

    target = (req->target) ? req->target : (SSMResource *)req->ctrlconn;
    if (target) {
        /* Get a reference on the target object so that it doesn't
           get deleted from underneath us. */
        rv = SSM_HTTPHandleOKCancelClick(req, PR_FALSE);
    }
    return rv;
}

SSMStatus SSM_HTTPFormSubmitHandler(HTTPRequest *req)
{
    if (req->target != NULL) {
        return(*req->target->m_submit_func)(req->target, req);
    }
    return SSM_FAILURE;
}

SSMStatus SSM_HTTPCloseWindow(HTTPRequest *req)
{
    SSMTextGenContext * cx = NULL;
    SSMResource * target;
    char * redirectHTML = NULL;
    SSMStatus rv = SSM_FAILURE;

    /* Get the target resource. */
    target = (req->target ? req->target : (SSMResource *) req->ctrlconn);
    PR_ASSERT(target);
    
    rv = SSMTextGen_NewTopLevelContext(req, &cx);
    rv = SSM_GetAndExpandText(cx, "windowclose_doclose_js_content", &redirectHTML);
     if (rv != SSM_SUCCESS)
         SSM_DEBUG("HTTPCloseWindow: can't create redirectHTML");
    req->sentResponse = PR_TRUE;
    SSMTextGen_DestroyContext(cx);
    rv = SSM_HTTPSendOKHeader(req, NULL, "text/html");
    rv = SSM_HTTPSendUTF8String(req, redirectHTML);
    PR_Free(redirectHTML);
    return rv;
}

char *
SSM_ConvertStringToHTMLString(char * string)
{
    char * result = NULL, * resultptr, * ptr;
    PRIntn len;
 
    PR_ASSERT(string);
    len = strlen(string);
 
    ptr = string;
    while (*ptr)
        if (!isalnum(*ptr++))
            len += 2;
   
    result = (char *) PORT_ZAlloc(len+1);
    if (!result) {
        SSM_DEBUG("ConvertStringToHTMLString: can't allocate memory!\n");
        goto loser;
    }
    /* no special character, just copy the string */
    if (len == strlen(string)) {
        PL_strcpy(result, string);
        goto done;
    }
 
    ptr = string;
    resultptr = result;
    /* copy by character, substituting ascii value for special characters */
    while (*ptr) {
        if (!isalnum(*ptr)) {
            sprintf(resultptr,"%%%.2x", *ptr);
            resultptr += 3;
        } else
            *resultptr++ = *ptr;
        ptr++;
    }
       
done:
    return result;
loser:
    PR_FREEIF(result);
    return NULL;
}

SSMStatus 
SSM_HTTPCloseIfCancel(HTTPRequest *req)
{
    return SSM_HTTPHandleOKCancelClick(req, PR_TRUE);
}
