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
/* http.c: HTTP protocol test */

#include "bench.h"
#include "http-util.h"

#define MAX_HTTP_RESPONSE_LEN (2*1024)	/* size of sliding buffer */

/*
  these are protocol dependent timers for Pop, Imap, Smtp, Http
  There is one of these for every command in every thread.
*/
typedef struct http_stats {
    event_timer_t	connect;	/* initial connections */
    event_timer_t	reconnect;	/* re-connections */
    event_timer_t	msgread;	/* AKA retrieve */
    event_timer_t	logout;		/* AKA dis-connect */
					/* no local storage */
} http_stats_t;

/* These are common to POP, IMAP, SMTP, HTTP */
typedef struct http_command {
    resolved_addr_t 	hostInfo;	/* Host, port, and IP cache */

    char *	httpCommand;		/* Old: single HTTP command */

#if 0
    string_list_t getFirstCmds;		/* GET commands after connect */
    string_list_t getLoopCmds;		/* GET commands, each loop */
    string_list_t getLastCmds;		/* GET commands before disconnect */
#endif

} http_command_t;

/*
  State during command execution.
*/
typedef struct _doHTTP_state {
    int		nothingHere;
} doHTTP_state_t;

static void doHttpExit (ptcx_t ptcx, doHTTP_state_t *me);
static int HttpParseNameValue (pmail_command_t cmd, char *name, char *tok);

/*
  Set defaults in command structure
*/
int
HttpParseStart (pmail_command_t cmd,
		char *line,
		param_list_t *defparm)
{
    param_list_t	*pp;
    http_command_t	*mycmd = (http_command_t *)mycalloc
	(sizeof (http_command_t));
    cmd->data = mycmd;

    cmd->numLoops = 1;		/* default 1 downloads */
    mycmd->hostInfo.portNum = HTTP_PORT;	/* get default port */

    D_PRINTF(stderr, "Http Assign defaults\n");
    /* Fill in defaults first, ignore defaults we dont use */
    for (pp = defparm; pp; pp = pp->next) {
	(void)HttpParseNameValue (cmd, pp->name, pp->value);
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
    http_command_t	*mycmd = (http_command_t *)cmd->data;
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

	if (HttpParseNameValue (cmd, name, tok) < 0) {
	    /* not a known attr */
	    D_PRINTF(stderr,"unknown attribute '%s' '%s'\n", name, tok);
	    returnerr(stderr,"unknown attribute '%s' '%s'\n", name, tok);
	}	
    }

    /* check for some of the required command attrs */
    if (!mycmd->hostInfo.hostName) {
	D_PRINTF(stderr,"missing server for command");
	return returnerr(stderr,"missing server for command\n");
    }

    if (!mycmd->httpCommand) {
	D_PRINTF(stderr,"missing httpcommand for HTTP");
	return returnerr(stderr,"missing httpcommand for HTTP\n");
    }

    /* see if we can resolve the mailserver addr */
    if (resolve_addrs(mycmd->hostInfo.hostName, "tcp",
		      &(mycmd->hostInfo.host_phe),
		      &(mycmd->hostInfo.host_ppe),
		      &(mycmd->hostInfo.host_addr),
		      &(mycmd->hostInfo.host_type))) {
	return returnerr (stderr, "Error resolving hostname '%s'\n",
			  mycmd->hostInfo.hostName);
    } else {
	mycmd->hostInfo.resolved = 1;	/* mark the hostInfo resolved */
    }

    return 1;
}

/*
  Parse arguments for http command
 */
static int
HttpParseNameValue (pmail_command_t cmd,
		    char *name,
		    char *tok)
{
    http_command_t	*mycmd = (http_command_t *)cmd->data;
    D_PRINTF (stderr, "HttpParseNameValue(name='%s' value='%s')\n", name, tok);

    /* find a home for the attr/value */
    if (cmdParseNameValue(cmd, name, tok))
	;				/* done */
    else if (strcmp(name, "server") == 0)
	mycmd->hostInfo.hostName = mystrdup (tok);
    else if (strcmp(name, "portnum") == 0)
	mycmd->hostInfo.portNum = atoi(tok);
    else if (strcmp(name, "httpcommand") == 0)
	mycmd->httpCommand = mystrdup (tok);
    /*stringListAdd(&mycmd->getFirstCmds, tok);*/
    else {
	return -1;
    }
    return 0;
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
	/*http_command_t	*mycmd = (http_command_t *)cmd->data;
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

void *
doHttpStart(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer)
{
    doHTTP_state_t	*me = (doHTTP_state_t *)mycalloc (sizeof (doHTTP_state_t));
    http_stats_t	*stats = (http_stats_t *)ptimer->data;
    http_command_t	*mycmd = (http_command_t *)cmd->data;

    if (!me) return NULL;

    if (HttpConnect(ptcx, &mycmd->hostInfo, &stats->connect) == -1) {
	return NULL;
    }

    /* there is no banner or login state for HTTP */

    return me;
}

int
doHttpLoop (
    ptcx_t		ptcx,
    mail_command_t	*cmd,
    cmd_stats_t		*ptimer,
    void		*mystate)
{
    doHTTP_state_t	*me = (doHTTP_state_t *)mystate;
    char	command[MAX_COMMAND_LEN];
    char	respBuffer[MAX_HTTP_RESPONSE_LEN];
    int		rc;
    http_stats_t	*stats = (http_stats_t *)ptimer->data;
    http_command_t	*mycmd = (http_command_t *)cmd->data;

    if (BADSOCKET(ptcx->sock)) {	/* re-connect if needed */
	rc = HttpConnect(ptcx, &mycmd->hostInfo, &stats->connect);
	if (rc == -1) {
	    return -1;
	}
    }

    /* send the HTTP command */

    strcpy (command, mycmd->httpCommand);
    strcat (command, " HTTP/1.0\r\n");
    /* other headers go here... */
    strcat(command, "\r\n");
    rc = HttpCommandResponse(ptcx, cmd, mystate,
			     &mycmd->hostInfo, &stats->connect,
			     &stats->msgread,
			     command, respBuffer,
			     sizeof(respBuffer), NULL);
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
