/* -*- Mode: C; c-file-style: "bsd"; comment-column: 40 -*- */
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
/* http.c: HTTP protocol test */

#include "bench.h"
#include "pish.h"

#define MAX_HTTP_RESPONSE_LEN (2*1024)	/* size of sliding buffer */

/*
  these are protocol dependent timers for Pop, Imap, Smtp, Http
  There is one of these for every command in every thread.
*/
typedef struct http_stats {
    event_timer_t	connect;
    event_timer_t	msgread;	/* AKA retrieve */
    event_timer_t	logout;		/* AKA dis-connect */
					/* no local storage */
} http_stats_t;

/*
  State during command execution.
*/
typedef struct _doHTTP_state {
    int		numMsgs;		/* messages in folder */
    int		totalMsgLength;		/* total msg length */
    int		msgCounter;		/* count in download */
} doHTTP_state_t;

static void doHttpExit (ptcx_t ptcx, doHTTP_state_t *me);
static int readHttpResponse(ptcx_t ptcx, SOCKET sock, char *buffer, int buflen);
static int doHttpCommandResponse(ptcx_t ptcx, SOCKET sock, char *command, char *response, int resplen);

/*
  Set defaults in command structure
*/
int
HttpParseStart (pmail_command_t cmd,
		char *line,
		param_list_t *defparm)
{
    param_list_t	*pp;
    pish_command_t	*pish = (pish_command_t *)mycalloc
	(sizeof (pish_command_t));
    cmd->data = pish;

    cmd->numLoops = 1;		/* default 1 downloads */
    pish->portNum = HTTP_PORT;	/* get default port */

    D_PRINTF(stderr, "Http Assign defaults\n");
    /* Fill in defaults first, ignore defaults we dont use */
    for (pp = defparm; pp; pp = pp->next) {
	(void)pishParseNameValue (cmd, pp->name, pp->value);
    }

    return 1;
}

/*
  Fill in structure from a list of lines
 */
int
HttpParseEnd (pmail_command_t cmd,
		string_list_t *section,
		param_list_t *defparm)
{
    pish_command_t	*pish = (pish_command_t *)cmd->data;
    string_list_t	*sp;

    /* Now parse section lines */
    D_PRINTF(stderr, "Http Assign section lines\n");
					/* skip first and last */
    for (sp = section->next; sp->next; sp = sp->next) {
	char *name = sp->value;
	char *tok = name + strcspn(name, " \t=");
	*tok++ = 0;			/* split name off */
	tok += strspn(tok, " \t=");

	string_tolower(name);
	tok = string_unquote(tok);

	if (pishParseNameValue (cmd, name, tok) < 0) {
	    /* not a known attr */
	    D_PRINTF(stderr,"unknown attribute '%s' '%s'\n", name, tok);
	    returnerr(stderr,"unknown attribute '%s' '%s'\n", name, tok);
	}	
    }

    /* check for some of the required command attrs */
    if (!pish->mailServer) {
	D_PRINTF(stderr,"missing server for command");
	return returnerr(stderr,"missing server for command\n");
    }

    if (!pish->httpCommand) {
	D_PRINTF(stderr,"missing httpcommand for HTTP");
	return returnerr(stderr,"missing httpcommand for HTTP\n");
    }

    /* see if we can resolve the mailserver addr */
    if (resolve_addrs(pish->mailServer, "tcp",
		      &(pish->hostInfo.host_phe),
		      &(pish->hostInfo.host_ppe),
		      &(pish->hostInfo.host_addr),
		      &(pish->hostInfo.host_type))) {
	return returnerr (stderr, "Error resolving hostname '%s'\n",
			  pish->mailServer);
    } else {
	pish->hostInfo.resolved = 1;	/* mark the hostInfo resolved */
    }

    return 1;
}

/* PROTOCOL specific */
void
HttpStatsInit(mail_command_t *cmd, cmd_stats_t *p, int procNum, int threadNum)
{
    assert (NULL != p);

    if (!p->data) {			/* create it  */
	p->data = mycalloc (sizeof (http_stats_t));
    } else {				/* zero it */
	memset (p->data, 0, sizeof (http_stats_t));
    }

    if (cmd) {				/* do sub-range calulations */
	/*pish_command_t	*pish = (pish_command_t *)cmd->data;
	  http_stats_t	*stats = (http_stats_t *)p->data;*/
    }
}

/* PROTOCOL specific */
void
HttpStatsUpdate(protocol_t *proto,
		 cmd_stats_t *sum,
		 cmd_stats_t *incr)
{
    http_stats_t	*ss = (http_stats_t *)sum->data;
    http_stats_t	*is = (http_stats_t *)incr->data;

    event_sum(&sum->idle, &incr->idle);
    event_sum(&ss->connect, &is->connect);
    event_sum(&ss->msgread, &is->msgread);
    event_sum(&ss->logout, &is->logout);

    event_reset(&incr->combined);	/* figure out total */
    event_sum(&incr->combined, &incr->idle);
    event_sum(&incr->combined, &is->connect);
    event_sum(&incr->combined, &is->msgread);
    event_sum(&incr->combined, &is->logout);

    event_sum(&sum->combined, &incr->combined);	/* add our total to sum-total*/

    sum->totalerrs += incr->totalerrs;
    sum->totalcommands += incr->totalcommands;
}

/*  PROTOCOL specific */
void
HttpStatsOutput(protocol_t *proto,
		  cmd_stats_t *ptimer,
		  char *buf)
{
    char eventtextbuf[SIZEOF_EVENTTEXT];
    http_stats_t	*stats = (http_stats_t *)ptimer->data;

    *buf = 0;

					/* BUG blocks done in clientSummary */
    event_to_text(&ptimer->combined, eventtextbuf);
    sprintf(&buf[strlen(buf)], "total=%s ", eventtextbuf);

    event_to_text(&stats->connect, eventtextbuf);
    sprintf(&buf[strlen(buf)], "conn=%s ", eventtextbuf);

    event_to_text(&stats->msgread, eventtextbuf);
    sprintf(&buf[strlen(buf)], "retrieve=%s ", eventtextbuf);

    event_to_text(&stats->logout, eventtextbuf);
    sprintf(&buf[strlen(buf)], "logout=%s ", eventtextbuf);

    event_to_text(&ptimer->idle, eventtextbuf);
    sprintf(&buf[strlen(buf)], "idle=%s ", eventtextbuf);

}

/*  PROTOCOL specific */
void
HttpStatsFormat (protocol_t *pp,
		  const char *extra,	/* extra text to insert (client=) */
		  char *buf)
{
    static char	*timerList[] = {	/* must match order of StatsOutput */
	"total",
	"conn", "retrieve", "logout",
	"idle" };

    char	**tp;
    char	*cp = buf;
    int		ii;

    /* Define the contents of each timer
      These must all the same, to that the core time functions
      can be qualified.  We specify each one for reduce.pl to work right.
    */

    for (ii=0, tp=timerList;
	 ii < (sizeof (timerList)/sizeof (timerList[0]));
	 ++ii, ++tp) {
	sprintf(cp, "<FORMAT %s TIMER=[%s]>%s</FORMAT>\n",
		extra, *tp,
		gs_eventToTextFormat); /* match event_to_text*/
	for (; *cp; ++cp);	/* skip to the end of the string */
    }
					/* BUG blocks matches clientSummary */
    sprintf(cp, "<FORMAT %s PROTOCOL={%s}>blocks=blocks ",
	    extra, pp->name);
    for (; *cp; ++cp);	/* skip to the end of the string */
    for (ii=0, tp=timerList;	/* same as above list (for now) */
	 ii < (sizeof (timerList)/sizeof (timerList[0]));
	 ++ii, ++tp) {
	sprintf (cp, "%s=[%s] ", *tp, *tp);
	for (; *cp; ++cp);	/* skip to the end of the string */
    }
    strcat (cp, "</FORMAT>\n");
}    

static int
readHttpResponse(ptcx_t ptcx, SOCKET sock, char *buffer, int buflen)
{    
    /* read the server response and do nothing with it */
    int totalbytesread = 0;
    int bytesread;

    memset (buffer, 0, buflen);
    while (totalbytesread < buflen)
    {
	if (gf_timeexpired >= EXIT_FAST) {
	    D_PRINTF(debugfile,"readHttpResponse() Time expired.\n");
	    break;
	}
	bytesread = retryRead(ptcx, sock, buffer, buflen-1);
	if (bytesread == 0)		/* just read until we hit emtpy */
	    break;
	if (bytesread < 0) {		/* IO error */
	    strcat (ptcx->errMsg, "<ReadHttpResponse");
	    return -1;
	}
        totalbytesread += bytesread;
	buffer[bytesread] = 0;		/* terminate for later searches */
	ptcx->bytesread += bytesread;
	T_PRINTF(ptcx->logfile, buffer, bytesread, "HTTP ReadResponse");
    }
    D_PRINTF(debugfile, "Read %d from server [%.99s]\n", totalbytesread, buffer);

    if (0 == totalbytesread) {
	strcpy (ptcx->errMsg, "ReadHttpResponse: Read 0 bytes");
	return -1;
    }
	

    return totalbytesread;
}

/* expects pointers to buffers of these sizes */
/* char command[MAX_COMMAND_LEN] */
/* char response[resplen] */

static int
doHttpCommandResponse(ptcx_t ptcx, SOCKET sock, char *command, char *response, int resplen)
{
    int ret;

    if (response == NULL)
	return -1;
    memset(response, 0, resplen);

    T_PRINTF(ptcx->logfile, command, strlen (command), "HTTP SendCommand");
    /* send the command already formatted */
    if ((ret = sendCommand(ptcx, sock, command)) == -1) {
	if (gf_timeexpired < EXIT_FAST) {
	    returnerr(debugfile, "Error sending [%s] command to server: %s\n",
		      command, neterrstr());
	}
	return -1;
    }

    /* read server response */
    if ((ret = readHttpResponse(ptcx, sock, response, resplen)) <= 0) {
	if (gf_timeexpired < EXIT_FAST) {
	    trimEndWhite (command);
	    returnerr(debugfile, "Error reading [%s] response: %s\n",
		      command, neterrstr());
	}
	return -1;
    }
    return ret;
}

#if 0
int
httpLogin(ptcx_t ptcx, mail_command_t *cmd, SOCKET sock)
{
  char	command[MAX_COMMAND_LEN];
  char	respBuffer[MAX_RESPONSE_LEN];
  char	mailUser[MAX_MAILADDR_LEN];
  char	userPasswd[MAX_MAILADDR_LEN];
  unsigned long loginNum;

  /* generate a random username (with a mailbox on the server) */
  loginNum = (RANDOM() % pish->numLogins);
  /* add the the base user */
  loginNum += pish->firstLogin;

  sprintf(mailUser, pish->loginFormat, loginNum);
  D_PRINTF(debugfile,"mailUser=%s\n", mailUser);
  sprintf(command, "USER %s%s", mailUser, CRLF);

  if (doHttpCommandResponse(ptcx, sock, command, respBuffer) == -1) {
      return -1;
  }

  /* send Password */
  sprintf(userPasswd, pish->passwdFormat, loginNum);
  sprintf(command, "PASS %s%s", userPasswd, CRLF);
  if (doHttpCommandResponse(ptcx, sock, command, respBuffer) == -1) {
      if (gf_timeexpired < EXIT_FAST) {
	  returnerr(debugfile,"HTTP cannot login user=%s pass=%s\n",
		    mailUser, userPasswd);
      }
      return -1;
  }
  return 0;
}
#endif

void *
doHttpStart(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer)
{
    doHTTP_state_t	*me = (doHTTP_state_t *)mycalloc (sizeof (doHTTP_state_t));
    http_stats_t	*stats = (http_stats_t *)ptimer->data;
    pish_command_t	*pish = (pish_command_t *)cmd->data;
    NETPORT		port;

    if (!me) return NULL;

    me->numMsgs = 0;
    me->totalMsgLength = 0;
    me->msgCounter = 0;
    port = pish->portNum;
  
    event_start(ptcx, &stats->connect);
    ptcx->sock = connectsock(ptcx, pish->mailServer, &pish->hostInfo, port, "tcp");
    event_stop(ptcx, &stats->connect);
    if (BADSOCKET(ptcx->sock)) {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->connect.errs++;
	    returnerr(debugfile,
		      "%s<wmapConnect: HTTP Couldn't connect to %s: %s\n",
		      ptcx->errMsg, pish->mailServer, neterrstr());
	}
	myfree (me);
	return NULL;
    }

    if (gf_abortive_close) {
	if (set_abortive_close(ptcx->sock) != 0) {
	    returnerr (debugfile,
		       "HTTP: WARNING: Could not set abortive close\n");
	}
    }

    /* there is no banner or login state for HTTP */

    return me;
}

int
doHttpLoop(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer, void *mystate)
{
    doHTTP_state_t	*me = (doHTTP_state_t *)mystate;
    char	command[MAX_COMMAND_LEN];
    char	respBuffer[MAX_HTTP_RESPONSE_LEN];
    int		rc;
    http_stats_t	*stats = (http_stats_t *)ptimer->data;
    pish_command_t	*pish = (pish_command_t *)cmd->data;

#if 0
    if (me->msgCounter >= me->numMsgs)
       return -1; /* done, close */
#endif

    /* send the HTTP command */

    /* will have to deal with encoding URLs before too long... */
    sprintf(command, pish->httpCommand, me->msgCounter);
    strcat(command, " HTTP/1.0\r\n");
    /* other headers go here... */
    strcat(command, "\r\n");
    event_start(ptcx, &stats->msgread);
    rc = doHttpCommandResponse(ptcx, ptcx->sock,
			       command, respBuffer, sizeof(respBuffer));
    event_stop(ptcx, &stats->msgread);
    if (rc == 0) {
	doHttpExit (ptcx, me);
	return -1;
    }
    if (rc == -1) {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->msgread.errs++;
	}
	doHttpExit (ptcx, me);
	return -1;
    }

    return 0;
}

void
doHttpEnd(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer, void *mystate)
{
    doHTTP_state_t	*me = (doHTTP_state_t *)mystate;
    http_stats_t	*stats = (http_stats_t *)ptimer->data;

    if (BADSOCKET(ptcx->sock)) return;	/* closed by previous error */

#if 0
    int	rc;
    char	command[MAX_COMMAND_LEN];
    char	respBuffer[MAX_RESPONSE_LEN];

    /* send HTTP QUIT equivalent */
    sprintf(command, "QUIT%s", CRLF);
    event_start(ptcx, &stats->logout);
    rc = doHttpCommandResponse(ptcx, ptcx->sock, command, respBuffer);
    event_stop(ptcx, &stats->logout);
    if (rc == -1) {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->logout.errs++;
	}
    }
#else
    event_start(ptcx, &stats->logout);
    NETCLOSE(ptcx->sock); ptcx->sock = BADSOCKET_VALUE;
    event_stop(ptcx, &stats->logout);
#endif

    doHttpExit (ptcx, me);
}

void
doHttpExit (ptcx_t ptcx, doHTTP_state_t	*me)
{
  if (!BADSOCKET(ptcx->sock))
    NETCLOSE(ptcx->sock);
  ptcx->sock = BADSOCKET_VALUE;

  myfree (me);
}  
