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
#include "cmtcmn.h"
#include "cmtjs.h"
#include "appsock.h"
#include <stdarg.h>
#include <string.h>

#ifdef XP_UNIX
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef WIN32
#include <direct.h>
#endif

/*
 * This is a simple program that tries to detect if the psm server is loaded.
 * If the server is not loaded, it will start it.  The program will then 
 * connect to the server and fetch an HTML page from an SSL server.
 *
 * NOTE: This sample program does not implement a mutex for the libraries.
 * If implementing a threaded application, then pass in a mutex structure
 * so that connections to the psm server happen in a thread safe manner.
 */

#define NUM_CONNECT_TRIES 10
#define READ_BUFFER_SIZE  1024
void
usage(void)
{
    printf("Usage:\n"
           "\tcmtsample <secure site>\n\n"
           "This program will then echo the retrieved HTML to the screen\n");
}

void
errorMessage(int err,char *msg, ...)
{
    va_list args;
    
    va_start(args, msg);
    fprintf (stderr, "cmtSample%s: ", (err) ? " error" : "");
    vfprintf (stderr, msg, args);
    fprintf (stderr, "\n");
    va_end(args);
    if (err) {
        exit (err);
    }
}

#ifdef XP_UNIX
#define FILE_PATH_SEPARATOR '/'
#elif defined (WIN32)
#define FILE_PATH_SEPARATOR '\\'
#else
#error Tell me what the file path separator is.
#endif

PCMT_CONTROL
connect_to_psm(void)
{
    PCMT_CONTROL control=NULL;
    char path[256], *tmp;

#ifdef XP_UNIX
    if (getcwd(path,256) == NULL) {
      return NULL;
    }
#elif defined(WIN32)
    if (_getcwd(path,256) == NULL) {
      return NULL;
    }
#else
#error Teach me how to get the current working directory.
#endif
    tmp = &path[strlen(path)];
    sprintf(tmp,"%c%s", FILE_PATH_SEPARATOR, "psm");
    return CMT_EstablishControlConnection(path, &socketFuncs, NULL);
}

#define HTTPS_STRING "https://"

char*
extract_host_from_url(char *url)
{
    char *start, *end, *retString=NULL;

    while(isspace(*url)) {
        url++;
    }
    url = strdup(url);
    start = strstr(url, HTTPS_STRING);
    if (start == NULL) {
        return NULL;
    }
    start += strlen(HTTPS_STRING);
    /*
     * Figure out the end of the host name.
     */
    end = strchr(start, ':');
    if (end != NULL) {
        *end = '\0';
    } else {
        end = strchr(start, '/');
        if (end != NULL) {
            *end = '\0';
        } else {
            end = strchr(start, ' ');
            if (end != NULL) {
                *end = '\0';
            }
        }
    }
    retString = strdup(start);
    return retString;
}

CMUint32
get_port_from_url(char *url)
{
    char *colon, *port;

    url = strdup(url);
    colon = strrchr(url, ':');
    if (colon == NULL ||
        !isdigit(colon[1])) {
        /* Return the default SSL port. */
        free(url);
        return 443;
    }
    colon++;
    port = colon;
    while(isdigit(*colon))
        colon++;
    colon[1] = '\0';
    free(url);
    return (CMUint32)atol(port);
}

char*
extract_get_target(char *url)
{
    char *slash;

    slash = strstr(url, "//");
    slash += 2;
    slash = strchr(slash, '/');
    if (slash != NULL)
        return strdup (slash);
    else 
        return strdup ("/");
}

/*
 * We'll use this function for prompting for a password.
 */
char*
passwordCallback(void *arg, char *prompt, void *cotext, int isPaswd)
{
    char input[256];
    
    printf(prompt);
    fgets(input, 256, stdin);

    return strdup(input);
}

void
freeCallback(char *userInput)
{
  free (userInput);
}

#define NUM_PREFS 2

int 
main(int argc, char **argv)
{
  PCMT_CONTROL control;
    CMTSocket sock, selSock;
    char *hostname;
    struct hostent *host;
    char *ipAddress;
    char buffer[READ_BUFFER_SIZE];
    size_t bytesRead;
    struct sockaddr_in destAddr;
    char *getString;
    char requestString[256];
    char *profile;
    CMTSetPrefElement prefs[NUM_PREFS];
    char profileDir[256];

#ifdef WIN32
    WORD WSAVersion = 0x0101;
    WSADATA WSAData;

    WSAStartup (WSAVersion, &WSAData);
#endif

    if (argc < 2) {
        usage();
        return 1;
    }
    errorMessage (0,"cmtsample v1.0");
    errorMessage (0,"Will try connecting to site %s", argv[1]);
    if (strstr(argv[1], "https://") == NULL) {
        errorMessage(2,"%s is not a secure site", argv[1]);
    }
    control = connect_to_psm();
    if (control == NULL) {
        errorMessage(3, "Could not connect to the psm server");
    }
    /* 
     * Now we have to send the hello message.
     */

#ifdef WIN32
    profile = strdup("default");
    sprintf(profileDir,"%s", "c:\\default");
#elif defined (XP_UNIX)
    if (argc > 2) {
        sprintf(profileDir,"%s", argv[2]);
    } else {
        profile = getenv("LOGNAME");
        sprintf(profileDir, "%s/.netscape", getenv("HOME"));
    }
#else
#error Teach me how to fill in the user profile.
#endif
    errorMessage(0,"Using directory <%s> for dbs.\n", profileDir);
    if (CMT_Hello(control, PROTOCOL_VERSION,
		  profile, profileDir) != CMTSuccess)
    {
        errorMessage(10, "Failed to send the Hello Message.");
    }
    CMT_SetPromptCallback(control, passwordCallback, NULL);
    CMT_SetAppFreeCallback(control, freeCallback);
    /*
     * Now pass along some preferences to psm.  We'll pass hard coded
     * ones here, but apps should figure out a way to manage their user's
     * preferences.
     */
    prefs[0].key   = "security.enable_ssl2";
    prefs[0].value = "true";
    prefs[0].type  = CMT_PREF_BOOL;
    prefs[1].key   = "security.enable_ssl3";
    prefs[1].value = "true";
    prefs[1].type  = CMT_PREF_BOOL;
    CMT_PassAllPrefs(control, NUM_PREFS, prefs);
    hostname = extract_host_from_url(argv[1]);
    host = gethostbyname(hostname);
    if (host == NULL) {
        errorMessage(11, "gethostbyname for %s failed", hostname);
    }
    if (host->h_length != 4) {
        errorMessage(4, "Site %s uses IV v6 socket.  Not supported by psm.");
    }
    
    /* Create the socket we will use to get the decrypted data back from
     * the psm server.
     */
    sock = APP_GetSocket(0);
    if (sock == NULL) {
        errorMessage(5, "Could not create new socket for communication with "
                        "the psm server.");
    }
    memcpy(&(destAddr.sin_addr.s_addr), host->h_addr, host->h_length);
    ipAddress = inet_ntoa(destAddr.sin_addr);
    errorMessage(0, "Mapped %s to the following IP address: %s", argv[1],
                 ipAddress);

    if (CMT_OpenSSLConnection(control, sock, SSM_REQUEST_SSL_DATA_SSL,
                              get_port_from_url(argv[1]), ipAddress, 
                              hostname, CM_FALSE, NULL) != CMTSuccess) {
        errorMessage(6, "Could not open SSL connection to %s.", argv[1]);
    }

    getString = extract_get_target(argv[1]);
    sprintf(requestString, 
            "GET %s HTTP/1.0\r\n"
            "\r\n", getString, hostname);
    APP_Send(sock, requestString, strlen(requestString));
    /*
     * Now all we have to do is sit here and fetch the data from the
     * socket.
     */
    errorMessage (0, "About to print out the fetched page.");
    while ((selSock=APP_Select(&sock, 1, 0)) != NULL) {
        if (selSock == sock) {
            bytesRead = APP_Receive(sock, buffer, READ_BUFFER_SIZE-1);
            if (bytesRead == -1 || bytesRead == 0) {
                break;
            }
            buffer[bytesRead] = '\0';
            fprintf(stderr, buffer);
        }
    }
    fprintf(stderr,"\n");
    if (bytesRead == -1) {
        errorMessage(7, "Error receiving decrypted data from psm.");
    }
    errorMessage(0, "Successfully read the entire page.");
    if (CMT_DestroyDataConnection(control, sock) != CMTSuccess) {
        errorMessage(8, "Error destroygin the SSL data connection "
                     "with the psm server.");
    }
    if (CMT_CloseControlConnection(control) != CMTSuccess) {
        errorMessage(9, "Error closing the control connection.");
    }
    return 0;
}




