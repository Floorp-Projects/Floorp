/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


/*
 * Author: Wan-Teh Chang
 *
 * Given an HTTP URL, httpget uses the GET method to fetch the file.
 * The fetched file is written to stdout by default, or can be
 * saved in an output file.
 *
 * This is a single-threaded program.
 */

#include "prio.h"
#include "prnetdb.h"
#include "prlog.h"
#include "prerror.h"
#include "prprf.h"
#include "prinit.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>  /* for atoi */

#define FCOPY_BUFFER_SIZE (16 * 1024)
#define INPUT_BUFFER_SIZE 1024
#define LINE_SIZE 512
#define HOST_SIZE 256
#define PORT_SIZE 32
#define PATH_SIZE 512

/*
 * A buffer for storing the excess input data for ReadLine.
 * The data in the buffer starts from (including) the element pointed to
 * by inputHead, and ends just before (not including) the element pointed
 * to by inputTail.  The buffer is empty if inputHead == inputTail.
 */

static char inputBuf[INPUT_BUFFER_SIZE];
/*
 * inputBufEnd points just past the end of inputBuf
 */
static char *inputBufEnd = inputBuf + sizeof(inputBuf);
static char *inputHead = inputBuf;
static char *inputTail = inputBuf;

static PRBool endOfStream = PR_FALSE;

/*
 * ReadLine --
 *
 * Read in a line of text, terminated by CRLF or LF, from fd into buf.
 * The terminating CRLF or LF is included (always as '\n').  The text
 * in buf is terminated by a null byte.  The excess bytes are stored in
 * inputBuf for use in the next ReadLine call or FetchFile call.
 * Returns the number of bytes in buf.  0 means end of stream.  Returns
 * -1 if read fails.
 */

PRInt32 ReadLine(PRFileDesc *fd, char *buf, PRUint32 bufSize)
{
    char *dst = buf;
    char *bufEnd = buf + bufSize;  /* just past the end of buf */
    PRBool lineFound = PR_FALSE;
    char *crPtr = NULL;  /* points to the CR ('\r') character */
    PRInt32 nRead;

loop:
    PR_ASSERT(inputBuf <= inputHead && inputHead <= inputTail
	    && inputTail <= inputBufEnd);
    while (lineFound == PR_FALSE && inputHead != inputTail
	    && dst < bufEnd - 1) {
	if (*inputHead == '\r') {
	    crPtr = dst;
	} else if (*inputHead == '\n') {
	    lineFound = PR_TRUE;
	    if (crPtr == dst - 1) {
		dst--; 
	    }
	}
	*(dst++) = *(inputHead++);
    }
    if (lineFound == PR_TRUE || dst == bufEnd - 1 || endOfStream == PR_TRUE) {
	*dst = '\0';
	return dst - buf;
    }

    /*
     * The input buffer should be empty now
     */
    PR_ASSERT(inputHead == inputTail);

    nRead = PR_Read(fd, inputBuf, sizeof(inputBuf));
    if (nRead == -1) {
	*dst = '\0';
	return -1;
    } else if (nRead == 0) {
	endOfStream = PR_TRUE;
	*dst = '\0';
	return dst - buf;
    }
    inputHead = inputBuf;
    inputTail = inputBuf + nRead;
    goto loop;
}

PRInt32 DrainInputBuffer(char *buf, PRUint32 bufSize)
{
    PRInt32 nBytes = inputTail - inputHead;

    if (nBytes == 0) {
	if (endOfStream) {
	    return -1;
	} else {
	    return 0;
	}
    }
    if ((PRInt32) bufSize < nBytes) {
	nBytes = bufSize;
    }
    memcpy(buf, inputHead, nBytes);
    inputHead += nBytes;
    return nBytes;
}

PRStatus FetchFile(PRFileDesc *in, PRFileDesc *out)
{
    char buf[FCOPY_BUFFER_SIZE];
    PRInt32 nBytes;

    while ((nBytes = DrainInputBuffer(buf, sizeof(buf))) > 0) {
	if (PR_Write(out, buf, nBytes) != nBytes) {
            fprintf(stderr, "httpget: cannot write to file\n");
	    return PR_FAILURE;
	}
    }
    if (nBytes < 0) {
	/* Input buffer is empty and end of stream */
	return PR_SUCCESS;
    }
    while ((nBytes = PR_Read(in, buf, sizeof(buf))) > 0) {
	if (PR_Write(out, buf, nBytes) != nBytes) {
	    fprintf(stderr, "httpget: cannot write to file\n");
	    return PR_FAILURE;
        }
    }
    if (nBytes < 0) {
	fprintf(stderr, "httpget: cannot read from socket\n");
	return PR_FAILURE;
    }
    return PR_SUCCESS;
}

PRStatus FastFetchFile(PRFileDesc *in, PRFileDesc *out, PRUint32 size)
{
    PRInt32 nBytes;
    PRFileMap *outfMap;
    void *addr;
    char *start;
    PRUint32 rem;
    PRUint32 bytesToRead;
    PRStatus rv;
    PRInt64 sz64;

    LL_UI2L(sz64, size);
    outfMap = PR_CreateFileMap(out, sz64, PR_PROT_READWRITE);
    PR_ASSERT(outfMap);
    addr = PR_MemMap(outfMap, LL_ZERO, size);
    if (addr == NULL) {
	fprintf(stderr, "cannot memory-map file: (%d, %d)\n", PR_GetError(),
		PR_GetOSError());

	PR_CloseFileMap(outfMap);
	return PR_FAILURE;
    }
    start = (char *) addr;
    rem = size;
    while ((nBytes = DrainInputBuffer(start, rem)) > 0) {
	start += nBytes;
	rem -= nBytes;
    }
    if (nBytes < 0) {
	/* Input buffer is empty and end of stream */
	return PR_SUCCESS;
    }
    bytesToRead = (rem < FCOPY_BUFFER_SIZE) ? rem : FCOPY_BUFFER_SIZE;
    while (rem > 0 && (nBytes = PR_Read(in, start, bytesToRead)) > 0) {
	start += nBytes;
	rem -= nBytes;
        bytesToRead = (rem < FCOPY_BUFFER_SIZE) ? rem : FCOPY_BUFFER_SIZE;
    }
    if (nBytes < 0) {
	fprintf(stderr, "httpget: cannot read from socket\n");
	return PR_FAILURE;
    }
    rv = PR_MemUnmap(addr, size);
    PR_ASSERT(rv == PR_SUCCESS);
    rv = PR_CloseFileMap(outfMap);
    PR_ASSERT(rv == PR_SUCCESS);
    return PR_SUCCESS;
}

PRStatus ParseURL(char *url, char *host, PRUint32 hostSize,
    char *port, PRUint32 portSize, char *path, PRUint32 pathSize)
{
    char *start, *end;
    char *dst;
    char *hostEnd;
    char *portEnd;
    char *pathEnd;

    if (strncmp(url, "http", 4)) {
	fprintf(stderr, "httpget: the protocol must be http\n");
	return PR_FAILURE;
    }
    if (strncmp(url + 4, "://", 3) || url[7] == '\0') {
	fprintf(stderr, "httpget: malformed URL: %s\n", url);
	return PR_FAILURE;
    }

    start = end = url + 7;
    dst = host;
    hostEnd = host + hostSize;
    while (*end && *end != ':' && *end != '/') {
	if (dst == hostEnd - 1) {
	    fprintf(stderr, "httpget: host name too long\n");
	    return PR_FAILURE;
	}
	*(dst++) = *(end++);
    }
    *dst = '\0';

    if (*end == '\0') {
	PR_snprintf(port, portSize, "%d", 80);
	PR_snprintf(path, pathSize, "%s", "/");
	return PR_SUCCESS;
    }

    if (*end == ':') {
	end++;
	dst = port;
	portEnd = port + portSize;
	while (*end && *end != '/') {
	    if (dst == portEnd - 1) {
		fprintf(stderr, "httpget: port number too long\n");
		return PR_FAILURE;
	    }
	    *(dst++) = *(end++);
        }
	*dst = '\0';
	if (*end == '\0') {
	    PR_snprintf(path, pathSize, "%s", "/");
	    return PR_SUCCESS;
        }
    } else {
	PR_snprintf(port, portSize, "%d", 80);
    }

    dst = path;
    pathEnd = path + pathSize;
    while (*end) {
	if (dst == pathEnd - 1) {
	    fprintf(stderr, "httpget: file pathname too long\n");
	    return PR_FAILURE;
	}
	*(dst++) = *(end++);
    }
    *dst = '\0';
    return PR_SUCCESS;
}

void PrintUsage(void) {
    fprintf(stderr, "usage: httpget url\n"
		    "       httpget -o outputfile url\n"
		    "       httpget url -o outputfile\n");
}

int main(int argc, char **argv)
{
    PRHostEnt hostentry;
    char buf[PR_NETDB_BUF_SIZE];
    PRNetAddr addr;
    PRFileDesc *socket = NULL, *file = NULL;
    PRIntn cmdSize;
    char host[HOST_SIZE];
    char port[PORT_SIZE];
    char path[PATH_SIZE];
    char line[LINE_SIZE];
    int exitStatus = 0;
    PRBool endOfHeader = PR_FALSE;
    char *url;
    char *fileName = NULL;
    PRUint32 fileSize;

    if (argc != 2 && argc != 4) {
	PrintUsage();
	exit(1);
    }

    if (argc == 2) {
	/*
	 * case 1: httpget url
	 */
	url = argv[1];
    } else {
	if (strcmp(argv[1], "-o") == 0) {
	    /*
	     * case 2: httpget -o outputfile url
	     */
	    fileName = argv[2];
	    url = argv[3];
        } else {
	    /*
	     * case 3: httpget url -o outputfile
	     */
	    url = argv[1];
	    if (strcmp(argv[2], "-o") != 0) {
		PrintUsage();
		exit(1);
            }
	    fileName = argv[3];
	}
    }

    if (ParseURL(url, host, sizeof(host), port, sizeof(port),
	    path, sizeof(path)) == PR_FAILURE) {
	exit(1);
    }

    if (PR_GetHostByName(host, buf, sizeof(buf), &hostentry)
	    == PR_FAILURE) {
        fprintf(stderr, "httpget: unknown host name: %s\n", host);
	exit(1);
    }

    addr.inet.family = PR_AF_INET;
    addr.inet.port = PR_htons((short) atoi(port));
    addr.inet.ip = *((PRUint32 *) hostentry.h_addr_list[0]);

    socket = PR_NewTCPSocket();
    if (socket == NULL) {
	fprintf(stderr, "httpget: cannot create new tcp socket\n");
	exit(1);
    }

    if (PR_Connect(socket, &addr, PR_INTERVAL_NO_TIMEOUT) == PR_FAILURE) {
	fprintf(stderr, "httpget: cannot connect to http server\n");
	exitStatus = 1;
	goto done;
    }

    if (fileName == NULL) {
	file = PR_STDOUT;
    } else {
        file = PR_Open(fileName, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE,
		00777);
        if (file == NULL) {
	    fprintf(stderr, "httpget: cannot open file %s: (%d, %d)\n",
		    fileName, PR_GetError(), PR_GetOSError());
	    exitStatus = 1;
	    goto done;
	}
    }

    cmdSize = PR_snprintf(buf, sizeof(buf), "GET %s HTTP/1.0\r\n\r\n", path);
    PR_ASSERT(cmdSize == (PRIntn) strlen("GET  HTTP/1.0\r\n\r\n")
            + (PRIntn) strlen(path));
    if (PR_Write(socket, buf, cmdSize) != cmdSize) {
	fprintf(stderr, "httpget: cannot write to http server\n");
	exitStatus = 1;
	goto done;
    }

    if (ReadLine(socket, line, sizeof(line)) <= 0) {
	fprintf(stderr, "httpget: cannot read line from http server\n");
	exitStatus = 1;
	goto done;
    }

    /* HTTP response: 200 == OK */
    if (strstr(line, "200") == NULL) {
	fprintf(stderr, "httpget: %s\n", line);
	exitStatus = 1;
	goto done;
    }

    while (ReadLine(socket, line, sizeof(line)) > 0) {
	if (line[0] == '\n') {
	    endOfHeader = PR_TRUE;
	    break;
	}
	if (strncmp(line, "Content-Length", 14) == 0
		|| strncmp(line, "Content-length", 14) == 0) {
	    char *p = line + 14;

	    while (*p == ' ' || *p == '\t') {
		p++;
	    }
	    if (*p != ':') {
		continue;
            }
	    p++;
	    while (*p == ' ' || *p == '\t') {
		p++;
	    }
	    fileSize = 0;
	    while ('0' <= *p && *p <= '9') {
		fileSize = 10 * fileSize + (*p - '0');
		p++;
            }
	}
    }
    if (endOfHeader == PR_FALSE) {
	fprintf(stderr, "httpget: cannot read line from http server\n");
	exitStatus = 1;
	goto done;
    }

    if (fileName == NULL || fileSize == 0) {
        FetchFile(socket, file);
    } else {
	FastFetchFile(socket, file, fileSize);
    }

done:
    if (socket) PR_Close(socket);
    if (file) PR_Close(file);
    PR_Cleanup();
    return exitStatus;
}
