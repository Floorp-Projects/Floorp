/* -*- Mode: C; c-file-style: "stroustrup"; comment-column: 40 -*- */
/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Mailstone utility, 
 * released March 17, 2000.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1999-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):	Dan Christian <robodan@netscape.com>
 *			Marcel DePaolis <marcel@netcape.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License Version 2 or later (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of
 * those above.  If you wish to allow use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the NPL or the GPL.
 */
/* http-util.c -- Shared HTTP functions */

#include "bench.h"
#include "http-util.h"

/*
  Establish (or re-establish) connection to server.
  No data is exchanged.
*/
int
HttpConnect (
    ptcx_t ptcx,
    resolved_addr_t	*hostInfo,
    event_timer_t	*timer)
{
    int		rc=0;

    D_PRINTF(stderr, "HttpConnect()\n");

    event_start(ptcx, timer);
    ptcx->sock = connectSocket(ptcx, hostInfo, "tcp");
    event_stop(ptcx, timer);
    if (BADSOCKET(ptcx->sock)) {
	if (gf_timeexpired < EXIT_FAST) {
	    timer->errs++;
	    returnerr(debugfile,
		      "%s<HttpConnect: HTTP Couldn't connect to %s: %s\n",
		      ptcx->errMsg, hostInfo->hostName, neterrstr());
	}
	return -1;
    }

    if (gf_abortive_close) {
	if (set_abortive_close(ptcx->sock) != 0) {
	    returnerr(debugfile,
		      "%s<HttpConnect: HTTP: WARNING: Could not set abortive close\n",
		      ptcx->errMsg, neterrstr());
	}
    }

    return rc;
}

/*
  HttpReadResponse handles the details of reading a HTTP response It
  takes and interactive parsing function and a response buffer.  The
  response buffer will hold at least the last 1/2 buffer of the data
  stream.  This allows errors to be handled by the calling routine.
  Long responses should be parsed on the fly by the parsing callback.

  HttpReadResponse handles find the content-length and header break.
  Once the break is called, it will call the parsing callback so that it can
  set findStr (which is NULL).  

  The findStr search is case insensitive if the first character of findStr
  has distinct upper and lower cases (see toupper and tolower).

  The callback is called with the buffer, the bytes in the buffer,
  a pointer just past the findStr match, and a pointer to findStr.
  The callback should then update findStr and searchStr; and return -2
  for an error (abort message), 0 for success, and 1 for need more data.

  findStr is expected to be a constant or static storage.
  Before exit, the parsing callback is called one last time (with NULL 
  findStr and searchStr), so that it can do any cleanup.

  Management of the buffer is done automatically by the movement of searchStr.

  HttpReadResponse returns bytesInBuffer on success, -1 for IO error,
  -2 for error returned by the parser.
*/
int
HttpReadResponse(
    ptcx_t ptcx,
    mail_command_t *cmd,		/* command info for msgParse */
    void *me,				/* connection state for msgParse */
    char *buffer,
    int buflen,
    parseMsgPtr_t	msgParse
    )
{    
    /* read the server response and do nothing with it */
    /* Will need to look for HTTP headers and Content-Length/Keep-Alive */
    int gotheader = 0;
    int totalbytesread = 0;
    int bytesread;
    int bytesInBuffer = 0;
    const char *findStr;		/* string we are watching for */
    const char *oldFindStr = NULL;
    char findStrSpn[3];			/* case insensitive search start */
    int	findLen=0;			/* length of string */
    char *foundStr;			/* what we found */
    char *searchStart;			/* where next to look for findStr */
    int contentlength = 0;		/* HTTP content length */
    int bytestoread = buflen-1;		/* start by reading lots */
    int getBytes = 0;			/* need more input */

    D_PRINTF(stderr, "HttpReadResponse()\n");

    findStr = HTTP_CONTENT_LENGTH;
    searchStart = buffer;
    memset(buffer, 0, buflen);		/* DEBUG */
    while (1)
    {
	if (gf_timeexpired >= EXIT_FAST) {
	    D_PRINTF(stderr,"HttpReadResponse() Time expired.\n");
	    break;
	}
	if (findStr != oldFindStr) {	/* findStr changed */
	    if (NULL == findStr) {
		findLen = 0;
		findStrSpn[0] = 0;
	    } else {
		char upperC, lowerC;	/* upper and lower of findStr[0] */
		findLen = strlen (findStr);
		upperC = toupper (findStr[0]);
		lowerC = tolower (findStr[0]);
		if (lowerC != upperC) {
		    /*D_PRINTF (stderr, "New findStr [%s] upper %c lower %c\n",
		      findStr, upperC, lowerC);*/
		    findStrSpn[0] = upperC;
		    findStrSpn[1] = lowerC;
		    findStrSpn[2] = 0;
		} else {
		    /*D_PRINTF (stderr, "New findStr [%s] NOT case sensitive\n",
		      findStr);*/
		    findStrSpn[0] = 0;
		}
	    }
	    oldFindStr = findStr;
	}
	/* See if we should read more */
	if ((getBytes > 0)
	    || (0 == findLen)
	    || ((bytesInBuffer - (searchStart - buffer)) < findLen)) {
	    int count;

	    if (totalbytesread >= bytestoread) { /* done reading */
		break;
	    }

	    /* Make room for more data */
	    /* need to be careful to save data for response. shift by half */
	    if ((searchStart - buffer) > buflen/2) {
		/* This is the only place we reduce searchStart or bytesInBuffer */
		int toSkip = buflen/2;
		int toSave = bytesInBuffer - toSkip;
		/*D_PRINTF (stderr, "Shifting %d of %d bytes\n",
		  toSave, bytesInBuffer);*/
		memcpy (buffer, buffer+toSkip, toSave); /* shift data down */
		bytesInBuffer = toSave;
		buffer[bytesInBuffer] = 0; /* re-terminate??? */
		searchStart -= toSkip;
		assert (searchStart >= buffer);
	    }
	    count = MIN (bytestoread-totalbytesread,
			     buflen-bytesInBuffer-1);
	    /*D_PRINTF(stderr, "retryRead() size=%d %d/%d inBuffer=%d\n",
	      count, totalbytesread, bytestoread, bytesInBuffer);*/
	    bytesread = retryRead(ptcx, ptcx->sock, buffer+bytesInBuffer, count);

	    if (0 == bytesread) {		/* most likely an error */
		strcpy (ptcx->errMsg, "HttpReadResponse:got 0 bytes");
		break;
	    }
	    if (bytesread < 0) {		/* ERROR */
		strcat (ptcx->errMsg, "<HttpReadResponse");
		return -1;
	    }
	    T_PRINTF(ptcx->logfile, buffer+bytesInBuffer, bytesread,
		     "%s ReadResponse", cmd->proto->name); /* telemetry log */
	    totalbytesread += bytesread;
	    bytesInBuffer += bytesread;
	    buffer[bytesInBuffer] = 0;	/* terminate for string searches */
	    ptcx->bytesread += bytesread; /* log statistics */
	    getBytes = 0;
	} else {
	    /*D_PRINTF (stderr,
		      "Not reading, have %d only need %d. %d/%d\n",
		      bytesInBuffer- (searchStart - buffer), findLen,
		      bytestoread, totalbytesread);*/
	}

	if (NULL == findStr) {		/* nothing to search for */
	    searchStart = buffer+bytesInBuffer;	/* discard buffer */
	    continue;
	}

	/* otherwise search for findStr */
	if (findStrSpn[0]) {		/* do case insensitive version */
	    char *cp;
	    foundStr = NULL;
	    for (cp = searchStart; (cp - buffer) < bytesInBuffer; ++cp) {
		cp = strpbrk (cp, findStrSpn); /* look for first char match */
		if (NULL == cp) {
		    break;
		}
		if (!strnicmp(cp, findStr, findLen)) { /* check whole match */
		    foundStr = cp;
		    break;
		}
	    }
	} else {			/* non case sensitive version */
	    foundStr = strstr(searchStart, findStr);
	}

	if (NULL == foundStr) {		/* found nothing, shift and continue */
	    /* jump to findLen-1 from end */
	    if (bytesInBuffer < findLen)
		continue;		/* nothing to shift */
	    
	    searchStart = buffer + bytesInBuffer - findLen + 1;
	    continue;		/* get more input */
	}

	/* Found search string */
	/*D_PRINTF (stderr, "Found [%s] after %d bytes\n",
	  findStr, totalbytesread);*/
	searchStart = foundStr + findLen; /* found this much */

	if (0 == gotheader) {	/* found length */
	    contentlength = atoi(searchStart); /* save contentlength */
	    searchStart = strchr (searchStart, '\n'); /* jump to EOL */
	    if (NULL == searchStart) {	/* missing line end, get more */
		searchStart = buffer;
	    } else {			/* got complete contentlength line */
		gotheader++;
		findStr = HTTP_HEADER_BREAK; /* now look for break */
	    }
	    continue;
	}
	else if (1 == gotheader) {	/* found header break */
	    gotheader++;
	    /* now compute bytetoread */
	    bytestoread = contentlength
		+ ((searchStart - buffer) /* position in buffer */
		   + (totalbytesread - bytesInBuffer)); /* buffer offset */
	    D_PRINTF(stderr, "contentlength=%d, bytestoread=%d\n",
		     contentlength, bytestoread);

	    findStr = NULL;		/* should chain into extra searches */

	    if (msgParse) {		/* initialize callback */
		int rc;
		/* having findStr == NULL signal initial callback */
		rc = (*(msgParse)) (ptcx, cmd, me,
				    buffer, bytesInBuffer, &searchStart, &findStr);
		if (rc < 0) {		/* parser signaled error */
		    return rc;
		}
		if (NULL == searchStart) {
		    searchStart = buffer+bytesInBuffer; /* discard buffer */
		}
	    }
	}
	else {
	    if (msgParse) {		/* call extra searches */
		int rc;
		rc = (*(msgParse)) (ptcx, cmd, me,
				    buffer, bytesInBuffer, &searchStart, &findStr);
		if (rc < 0) {		/* protocol error */
		    return rc;
		}
		if (rc > 0) {		/* need more data */
		    getBytes = rc;
		    searchStart = foundStr; /* back up search string */
		    continue;
		}
		if (searchStart < buffer) { /* or NULL */
		    searchStart = buffer+bytesInBuffer; /* discard buffer */
		}
	    } else {
		searchStart = buffer+bytesInBuffer; /* discard buffer */
	    }
	}
    }
    D_PRINTF(stderr, "HttpReadResponse: Read %d/%d saved %d\n",
	     totalbytesread, bytestoread, bytesInBuffer);
    if (msgParse) {		/* call extra searches */
	findStr = NULL;			/* signal final callback */
	searchStart = NULL;
	(void)(*(msgParse)) (ptcx, cmd, me,
			     buffer, bytesInBuffer, &searchStart, &findStr);
    }

    return bytesInBuffer;
}

/* 
   Handle a command-response transaction.  Connects/retries as needed.
   Expects pointers to buffers of these sizes:
   char command[MAX_COMMAND_LEN]
   char response[resplen]
*/
int
HttpCommandResponse (
    ptcx_t ptcx,			/* thread state */
    mail_command_t *cmd,		/* command info [blind] */
    void *me,				/* connection state for msgParse */
    resolved_addr_t	*hostInfo,	/* reconnect info */
    event_timer_t *reconnect,		/* timer for reconnects */

    event_timer_t *timer,		/* timer for this command */
    char *command,			/* command to send */
    char *response,			/* response buffer */
    int	resplen,			/* size of response buffer */
    parseMsgPtr_t	msgParse)	/* response handler */
{
    int retries = 0;
    int ret;

    if (response == NULL)
	return -1;

    response[0] = 0;			/* make sure it makes sense on error */

    D_PRINTF(stderr, "HttpCommandResponse() command=[%.99s]\n", command);

    while (retries++ < HTTP_MAX_RECONNECTS) {
	if (BADSOCKET(ptcx->sock)) {
	    ret = HttpConnect(ptcx, hostInfo, reconnect);
	    if (ret == -1) {
		return -1;
	    }
	}
	/* send the command already formatted */
	T_PRINTF(ptcx->logfile, command, strlen (command),
		 "%s SendCommand", cmd->proto->name);
	event_start(ptcx, timer);
	ret = sendCommand(ptcx, ptcx->sock, command);
	if (ret == -1) {
	    event_stop(ptcx, timer);
	    reconnect->errs++;
	    /* this can only mean an IO error.  Probably EPIPE */
	    NETCLOSE(ptcx->sock);
	    ptcx->sock = BADSOCKET_VALUE;
	    strcat (ptcx->errMsg, "<HttpCommandResponse");
	    D_PRINTF (stderr, "%s Aborting connection %s\n",
		      ptcx->errMsg, neterrstr());
	    continue;
	}

	/* read server response */
	ret = HttpReadResponse(ptcx, cmd, me, response, resplen, msgParse);
	event_stop(ptcx, timer);
	if (ret == 0) {			/* got nothing */
	    /* Probably an IO error.  It will be definate if re-tried. */
	    MS_usleep (5000);		/* time for bytes to show up */
	    continue;			/* just re-try it */
	} else if (ret == -1) {		/* IO error */
	    reconnect->errs++;
	    NETCLOSE(ptcx->sock);
	    ptcx->sock = BADSOCKET_VALUE;
	    strcat (ptcx->errMsg, "<HttpCommandResponse");
	    D_PRINTF (stderr, "%s Aborting connection %s\n",
		      ptcx->errMsg, neterrstr());
	    continue;
	} else if (ret < -1) {		/* some other error */
	    timer->errs++;
					/* this is overkill */
	    NETCLOSE(ptcx->sock);
	    ptcx->sock = BADSOCKET_VALUE;
	    strcat (ptcx->errMsg, "<HttpCommandResponse: protocol ERROR");
	    return -1;
	}
	/*D_PRINTF (stderr, "HttpCommandResponse saved %d bytes response=[%s]\n",
	  ret, response);*/
	/* You get parent.timeoutCB on any kind of error (like bad sid) */
	if (strstr (response, "parent.timeoutCB()")) {
	    timer->errs++;
	    NETCLOSE(ptcx->sock);
	    ptcx->sock = BADSOCKET_VALUE;
	    strcpy (ptcx->errMsg, "HttpCommandResponse: got HTTP error msg");
	    return -1;
	}
	    
	return ret;
    }
    
    reconnect->errs++;
    NETCLOSE(ptcx->sock);
    ptcx->sock = BADSOCKET_VALUE;
    strcat (ptcx->errMsg, "<HttpCommandResponse: Too many connection retries");
    return -1;
}
