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
#if defined(XP_UNIX) || defined(XP_BEOS) || defined(XP_OS2)
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#ifndef XP_BEOS
#include <netinet/tcp.h>
#endif
#else
#ifdef XP_MAC
#include <Events.h> // for WaitNextEvent
#else /* Windows */
#include <windows.h>
#include <winsock.h>
#include <direct.h>
#include <sys/stat.h>
#endif
#endif

#ifdef XP_OS2_VACPP
#include <libc/direct.h>
#endif

#include "messages.h"
#include "cmtcmn.h"
#include "cmtutils.h"
#include <string.h>

#if defined(XP_UNIX) || defined(XP_BEOS)
#define DIRECTORY_SEPARATOR '/'
#elif defined(WIN32) || defined(XP_OS2)
#define DIRECTORY_SEPARATOR '\\'
#elif defined XP_MAC
#define DIRECTORY_SEPARATOR ':'
#endif

/* Local defines */
#define CARTMAN_PORT	11111
#define MAX_PATH_LEN    256

/* write to the cmnav.log */
#if 0
#define LOG(x); do { FILE *f; f=fopen("cmnav.log","a+"); if (f) { \
   fprintf(f, x); fclose(f); } } while(0);
#define LOG_S(x); do { FILE *f; f=fopen("cmnav.log","a+"); if (f) { \
   fprintf(f, "%s", x); fclose(f); } } while(0);
#define ASSERT(x); if (!(x)) { LOG("ASSERT:"); LOG(#x); LOG("\n"); exit(-1); }
#else
#define LOG(x); ;
#define LOG_S(x); ;
#define ASSERT(x); ;
#endif

static char*
getCurrWorkDir(char *buf, int maxLen)
{
#if defined WIN32
    return _getcwd(buf, maxLen);
#elif defined(XP_UNIX) || defined(XP_BEOS) || defined(XP_OS2)
    return getcwd(buf, maxLen);
#else
    return NULL;
#endif
}

static void
setWorkingDir(char *path)
{
#if defined WIN32
    _chdir(path);
#elif defined(XP_UNIX) || defined(XP_BEOS) || defined(XP_OS2)
    chdir(path);
#else
    return;
#endif
}

static CMTStatus
launch_psm(char *executable)
{
#ifndef XP_MAC
    char command[MAX_PATH_LEN];
#endif
#ifdef WIN32
    STARTUPINFO sui;
    PROCESS_INFORMATION pi;
    UNALIGNED long *posfhnd;
    int i;
    char *posfile;

    sprintf(command,"%s > psmlog", executable);
    ZeroMemory( &sui, sizeof(sui) );
    sui.cb = sizeof(sui);
    sui.cbReserved2 = (WORD)(sizeof( int ) + (3 * (sizeof( char ) + 
                                                   sizeof( long ))));
    sui.lpReserved2 = calloc( sui.cbReserved2, 1 );
    *((UNALIGNED int *)(sui.lpReserved2)) = 3;
    posfile = (char *)(sui.lpReserved2 + sizeof( int ));
    posfhnd = (UNALIGNED long *)(sui.lpReserved2 + sizeof( int ) + 
                                 (3 * sizeof( char )));
    
    for ( i = 0, posfile = (char *)(sui.lpReserved2 + sizeof( int )),
              posfhnd = (UNALIGNED long *)(sui.lpReserved2 + sizeof( int ) + (3 * sizeof( char ))) ;
          i < 3 ; i++, posfile++, posfhnd++ ) {
        
        *posfile = 0;
        *posfhnd = (long)INVALID_HANDLE_VALUE;
    }
    /* Now, fire up PSM */
    if (!CreateProcess(NULL, command, NULL, NULL, TRUE, DETACHED_PROCESS, 
                       NULL, NULL, &sui, &pi)) {
        goto loser;
    }

    return CMTSuccess;
 loser:
    return CMTFailure;
#elif defined(XP_UNIX) || defined(XP_BEOS)
    sprintf(command,"./%s &", executable);
    if (system(command) == -1) {
        goto loser;
    }
    return CMTSuccess;
 loser:
    return CMTFailure;
#elif defined(XP_OS2)
    STARTDATA sd;
    ULONG sid;
    PID pid;
    APIRET rc;

    memset(&sd, 0, 50);
    sd.Length = 50;
    sd.InheritOpt = SSF_INHERTOPT_PARENT;
    sd.SessionType = SSF_TYPE_PM;
    sd.PgmName = executable;

    rc = DosStartSession( &sd, &sid, &pid );
    if( rc != NO_ERROR && rc != ERROR_SMG_START_IN_BACKGROUND ) {
      printf( "DosStartSession error: return code = %u\n", rc );
      return CMTFailure;
    }
    else
      return CMTSuccess;
#else
    return CMTFailure;
#endif
}

PCMT_CONTROL CMT_EstablishControlConnection(char            *inPath, 
                                            CMT_SocketFuncs *sockFuncs,
                                            CMT_MUTEX       *mutex)
{
    PCMT_CONTROL control;
#ifndef XP_MAC
    char *executable;
    char *newWorkingDir;
    char oldWorkingDir[MAX_PATH_LEN];
    size_t stringLen;
#endif    
    int i;
    char *path = NULL;
    int rc;

    /*	On the Mac, we do special magic in the Seamonkey PSM component, so
    	if PSM isn't launched by the time we reach this point, we're not doing well. */
#ifndef XP_MAC

    struct stat stbuf;
    
    /*
     * Create our own copy of path.
     * I'd like to do a straight strdup here, but that caused problems
     * for https.
     */
    stringLen = strlen(inPath);

    path = (char*) malloc(stringLen+1);
    memcpy(path, inPath, stringLen);
    path[stringLen] = '\0';

    control = CMT_ControlConnect(mutex, sockFuncs);
    if (control != NULL) {
        return control;
    }
    /*
     * We have to try to launch it now, so it better be a valid 
     * path.
     */
    if (stat(path, &stbuf) == -1) {
        goto loser;
    }
    /*
     * Now we have to parse the path and launch the psm server.
     */
    executable = strrchr(path, DIRECTORY_SEPARATOR);
    if (executable != NULL) {
        *executable = '\0';
        executable ++;
        newWorkingDir = path;
    } else {
        executable = path;
        newWorkingDir = NULL;
    }
    if (getCurrWorkDir(oldWorkingDir, MAX_PATH_LEN) == NULL) {
        goto loser;
    }
    setWorkingDir(newWorkingDir);
    rc = launch_psm(executable);
    
    setWorkingDir(oldWorkingDir);

    if (rc != CMTSuccess) {
        goto loser;
    }
#endif

    /*
     * Now try to connect to the psm server.  We will try to connect
     * a maximum of 30 times and then give up.
     */
#ifdef WIN32
    for (i=0; i<30; i++) {
        Sleep(1000);
        control = CMT_ControlConnect(mutex, sockFuncs);
        if (control != NULL) {
            break;
        }
    }
#elif defined(XP_OS2)
    for (i=0; i<30; i++) {
        DosSleep(1000);
        control = CMT_ControlConnect(mutex, sockFuncs);
        if (control != NULL) {
            break;
        }
    }
#elif defined(XP_UNIX) || defined(XP_BEOS)
    i = 0;
    while (i<1000) {
        i += sleep(10);
	control = CMT_ControlConnect(mutex, sockFuncs);
	if (control != NULL) {
	  break;
	}
    }
#elif defined(XP_MAC)
	for (i=0; i<30; i++) 
	{
		EventRecord theEvent;
		WaitNextEvent(0, &theEvent, 30, NULL);
		control = CMT_ControlConnect(mutex, sockFuncs);
		if (control != NULL) 
			break;
	}

#else
    /*
     * Figure out how to sleep for a while first
     */
    for (i=0; i<30; i++) {
      control = CMT_ControlConnect(mutex, sockFuncs);
      if (control!= NULL) {
	break;
      }
    }
#endif
    if (control == NULL) {
        goto loser;
    }
    if (path) {
        free (path);
    }
    return control;
 loser:
    if (control != NULL) {
        CMT_CloseControlConnection(control);
    }
    if (path) {
        free(path);
    }
    return NULL;
}


PCMT_CONTROL CMT_ControlConnect(CMT_MUTEX *mutex, CMT_SocketFuncs *sockFuncs)
{
    PCMT_CONTROL control = NULL; 
    CMTSocket sock=NULL;
#ifdef XP_UNIX
    int unixSock = 1;
    char path[20];
#else
    int unixSock = 0;
    char *path=NULL;
#endif

    if (sockFuncs == NULL) {
        return NULL;
    }
#ifdef XP_UNIX
    sprintf(path, "/tmp/.nsmc-%d", (int)geteuid());
#endif    

    sock = sockFuncs->socket(unixSock);
    if (sock == NULL) {
        LOG("Could not create a socket to connect to Control Connection.\n");
        goto loser;
    }
    /* Connect to the psm process */
    if (sockFuncs->connect(sock, CARTMAN_PORT, path)) {
	  LOG("Could not connect to Cartman\n");
	  goto loser;
    }

#ifdef XP_UNIX
    if (sockFuncs->verify(sock) != CMTSuccess) {
        goto loser;
    }
#endif

	LOG("Connected to Cartman\n");
	
	/* fill in the CMTControl struct */
	control = (PCMT_CONTROL)calloc(sizeof(CMT_CONTROL), 1);
	if (control == NULL ) {
		goto loser;
	}
	control->sock = sock;
    if (mutex != NULL) {
        control->mutex = (CMT_MUTEX*)calloc(sizeof(CMT_MUTEX),1);
        if (control->mutex == NULL) {
            goto loser;
        }
        *control->mutex = *mutex;
    }
    memcpy(&control->sockFuncs, sockFuncs, sizeof(CMT_SocketFuncs));
    control->refCount = 1;
	goto done;
    
 loser:
	if (control != NULL) {
		free(control);
    }
    if (sock != NULL) {
        sockFuncs->close(sock);
    }
	control = NULL;
    
 done:
	return control;
}

CMTStatus CMT_CloseControlConnection(PCMT_CONTROL control)
{
	/* XXX Don't know what to do here yet */
    if (control != NULL) {
        CMInt32 refCount;
        CMT_LOCK(control->mutex);
        control->refCount--;
        refCount = control->refCount;
        CMT_UNLOCK(control->mutex);
        if (refCount <= 0) {
            if (control->mutex != NULL) {
                free (control->mutex);
            }
            control->sockFuncs.close(control->sock);
            free(control);
        }
    }
    
	return CMTSuccess;
}

CMTStatus CMT_Hello(PCMT_CONTROL control, CMUint32 version, char* profile, 
                    char* profileDir)
{
	CMTItem message;
    PCMT_EVENT eventHandler;
    CMBool doesUI;
    HelloRequest request;
    HelloReply reply;

	/* Check the passed parameters */
	if (!control) {
		return CMTFailure;
	}
	if (!profile) {
		return CMTFailure;
	}
    if (!profileDir) {
        return CMTFailure;
    }
    
	/* Create the hello message */
    eventHandler = CMT_GetEventHandler(control, SSM_UI_EVENT, 0);
    doesUI = (eventHandler == NULL) ? CM_FALSE : CM_TRUE;

    /* Setup the request struct */
    request.version = version;
    request.policy = 0; /* no more policy */
    request.doesUI = doesUI;
    request.profile = profile;
    request.profileDir = profileDir;

    message.type = SSM_REQUEST_MESSAGE | SSM_HELLO_MESSAGE;

    if (CMT_EncodeMessage(HelloRequestTemplate, &message, &request) != CMTSuccess) {
        goto loser;
    }

	/* Send the message and get the response */
	if (CMT_SendMessage(control, &message) != CMTSuccess) {
        goto loser;
	}

    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_HELLO_MESSAGE)) {
        goto loser;
    }

    /* Decode the message */
    if (CMT_DecodeMessage(HelloReplyTemplate, &reply, &message) != CMTSuccess) {
        goto loser;
    }

	/* Successful response */
	if (reply.result == 0) {
            /* Save the nonce value */
            control->sessionID = reply.sessionID;
            control->protocolVersion = reply.version;
            control->port = reply.httpPort;
            control->nonce = reply.nonce;
            control->policy = reply.policy;
            control->serverStringVersion = reply.stringVersion;

            /* XXX Free the messages */
            return CMTSuccess;
	}
loser:
	/* XXX Free the messages */
	return CMTFailure;
}

CMTStatus CMT_PassAllPrefs(PCMT_CONTROL control, int num,
                           CMTSetPrefElement* list)
{
    SetPrefListMessage request;
    SingleNumMessage reply;
    CMTItem message;

    if ((control == NULL) || (list == NULL)) {
        return CMTFailure;
    }

    /* pack the request */
    request.length = num;
    request.list = (SetPrefElement*)list;

    if (CMT_EncodeMessage(SetPrefListMessageTemplate, &message, &request) !=
        CMTSuccess) {
        goto loser;
    }
    message.type = SSM_REQUEST_MESSAGE | SSM_PREF_ACTION;

    /* send the message */
    if (CMT_SendMessage(control, &message) != CMTSuccess) {
        goto loser;
    }

    if (message.type != (SSM_REPLY_OK_MESSAGE | SSM_PREF_ACTION)) {
        goto loser;
    }

    if (CMT_DecodeMessage(SingleNumMessageTemplate, &reply, &message) !=
        CMTSuccess) {
        goto loser;
    }

    /* don't really need to check the return value */
    return CMTSuccess;
loser:
    return CMTFailure;
}

char* CMT_GetServerStringVersion(PCMT_CONTROL control)
{
    if (control == NULL) {
        return NULL;
    }
    return control->serverStringVersion;
}
