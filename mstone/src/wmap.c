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
/* wmap.c -- Test Netscape Messaging WebMail 4.x (know as Web-IMAP or WMAP) */

#include "bench.h"
#include "http-util.h"

/*
  these are protocol dependent timers
  There is one of these for every command in every thread.
*/
typedef struct wmap_stats {
    event_timer_t	connect;	/* initial connections */
    event_timer_t	reconnect;	/* re connections */
    event_timer_t	banner;		/* get initial screen */
    event_timer_t	login;		/* do authentication */
    event_timer_t	cmd;		/* arbitrary commands */
    event_timer_t	headers;	/* headers read */
    event_timer_t	msgread;	/* messages read */
    event_timer_t	msgwrite;	/* messages written */
    event_timer_t	logout;		/* AKA dis-connect */
					/* no local storage */

    range_t		loginRange;	/* login range for this thread */
    unsigned long	lastLogin;
    range_t		domainRange;	/* domain range for this thread */
    unsigned long	lastDomain;
    range_t		addressRange;	/* address range for this thread */
    unsigned long	lastAddress;
} wmap_stats_t;

/* Command information */
typedef struct wmap_command {
    resolved_addr_t 	hostInfo;	/* should be a read only cache */

    /* These are common to SMTP, POP, IMAP, WMAP */
    char *	loginFormat;
    range_t	loginRange;		/* login range for all threads */
    range_t	domainRange;		/* domain range for all threads */
    char *	passwdFormat;

    /* info for sending mail */
    char *	addressFormat;
    range_t	addressRange;		/* address range for all threads */
    char *	smtpMailFrom;
    char *	filename;
    int 	numRecipients;	/* recpients per message */
    int 	msgsize;	/* message size without trailing CRLF.CRLF */
    char *	msgdata;	/* cache the file in mem */

    /* flag to leave mail on server (dont delete read mail) */
    int 	leaveMailOnServer;

    /* WMAP client http header */
    char *	wmapClientHeader;	/* http header for requests w/ %s=referhost %s=host */

    /* WMAP login stage */
    string_list_t wmapBannerCmds;	/* GET commands for initial screen */

    /* WMAP do actual login */
    char *	wmapLoginCmd;		/* login POST command */
    char *	wmapLoginData;		/* login POST data w/ %s=user, %s=pass */

    /* WMAP inbox stage */
    string_list_t wmapInboxCmds;	/* GET commands for inbox screen, %s=session */

    /* WMAP inbox stage */
    string_list_t wmapCheckCmds;	/* GET commands for message list, %s=session */

    /* WMAP message read */
    string_list_t wmapMsgReadCmds;	/* GET commands to read a message, %s=session, %d=msguid */

    /* WMAP message write */
    string_list_t wmapMsgWriteCmds;	/* GET/POST command to submit a message */

    /* WMAP logout stage */
    string_list_t wmapLogoutCmds;	/* GET commands for logging out */

    /* WMAP command HACK: just doing HTTP for initial porting. */
    char *	wmapCommand;
} wmap_command_t;

/*
  State during command execution.
*/
/* This is (now) parsed on the fly, doesnt have to be huge */
#define RESPONSE_BUFFER_SIZE	(3*1024)

typedef struct _doWMAP_state {
    char *	redirectURL;		/* URL returned by login w/ session ID */
    char *	sessionID;		/* session ID parsed from redirectURL */
    int		msgCounter;		/* count in download */
    int		numMsgs;		/* messages in folder */
    int	*	msgList;		/* array[numMsgs] of message IDs */
    char	response_buffer[RESPONSE_BUFFER_SIZE];
} doWMAP_state_t;
static int WmapParseNameValue(pmail_command_t cmd, char *name, char *tok);

static int wmapBanner(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer, doWMAP_state_t *me);
static int wmapLogin(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer, doWMAP_state_t *me);
static int wmapLogout(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer, doWMAP_state_t *me);

static void doWmapExit(ptcx_t ptcx, mail_command_t *cmd, doWMAP_state_t *me);

/*
  Set defaults in command structure
*/
int
WmapParseStart(pmail_command_t cmd, char *line, param_list_t *defparm)
{
    param_list_t	*pp;
    wmap_command_t	*wmap = (wmap_command_t *)mycalloc
	(sizeof (wmap_command_t));
    cmd->data = wmap;

    cmd->numLoops = 1;		/* default 1 downloads */
    wmap->hostInfo.portNum = WMAP_PORT;	/* get default port */

    D_PRINTF(stderr, "Wmap Assign defaults\n");
    /* Fill in defaults first, ignore defaults we dont use */
    for (pp = defparm; pp; pp = pp->next) {
	(void)WmapParseNameValue(cmd, pp->name, pp->value);
    }

    return 1;
}

/*
  Fill in structure from a list of lines
 */
int
WmapParseEnd(pmail_command_t cmd, string_list_t *section, param_list_t *defparm)
{
    wmap_command_t	*wmap = (wmap_command_t *)cmd->data;
    string_list_t	*sp;

    /* Now parse section lines */
    D_PRINTF(stderr, "Wmap Assign section lines\n");
					/* skip first and last */
    for (sp = section->next; sp->next; sp = sp->next) {
	char *name = sp->value;
	char *tok = name + strcspn(name, " \t=");
	*tok++ = 0;			/* split name off */
	tok += strspn(tok, " \t=");

	string_tolower(name);
	tok = string_unquote(tok);

	if (WmapParseNameValue (cmd, name, tok) < 0) {
	    /* not a known attr */
	    D_PRINTF(stderr,"unknown attribute '%s' '%s'\n", name, tok);
	    returnerr(stderr,"unknown attribute '%s' '%s'\n", name, tok);
	}	
    }

    /* check for some of the required command attrs */
    if (!wmap->hostInfo.hostName) {
	D_PRINTF(stderr,"missing server for command");
	return returnerr(stderr,"missing server for command\n");
    }

    if (!wmap->wmapClientHeader) {
	D_PRINTF(stderr,"missing wmapClientHeader for WMAP");
	return returnerr(stderr,"missing wmapClientHeader for WMAP\n");
    }

    if (!wmap->wmapBannerCmds.value) {
	D_PRINTF(stderr,"missing wmapBannerCmds for WMAP");
	return returnerr(stderr,"missing wmapBannerCmds for WMAP\n");
    }

    if (!wmap->wmapLoginCmd) {
	D_PRINTF(stderr,"missing wmapLoginCmd for WMAP");
	return returnerr(stderr,"missing wmapLoginCmd for WMAP\n");
    }

    if (!wmap->wmapLoginData) {
	D_PRINTF(stderr,"missing wmapLoginData for WMAP");
	return returnerr(stderr,"missing wmapLoginData for WMAP\n");
    }

    if (!wmap->wmapMsgReadCmds.value) {
	D_PRINTF(stderr,"missing wmapMsgReadCmds for WMAP");
	return returnerr(stderr,"missing wmapMsgReadCmds for WMAP\n");
    }

    if (!wmap->wmapMsgWriteCmds.value) {
	D_PRINTF(stderr,"missing wmapMsgWriteCmds for WMAP");
	return returnerr(stderr,"missing wmapMsgWriteCmds for WMAP\n");
    }

    if (!wmap->wmapLogoutCmds.value) {
	D_PRINTF(stderr,"missing wmapLogoutCmds for WMAP");
	return returnerr(stderr,"missing wmapLogoutCmds for WMAP\n");
    }

    /* see if we can resolve the mailserver addr */
    if (resolve_addrs(wmap->hostInfo.hostName, "tcp",
		      &(wmap->hostInfo.host_phe),
		      &(wmap->hostInfo.host_ppe),
		      &(wmap->hostInfo.host_addr),
		      &(wmap->hostInfo.host_type))) {
	return returnerr (stderr, "Error resolving hostname '%s'\n",
			  wmap->hostInfo.hostName);
    } else {
	wmap->hostInfo.resolved = 1;	/* mark the hostInfo resolved */
    }
    rangeSetFirstCount(&wmap->loginRange, wmap->loginRange.first,
		       wmap->loginRange.span, wmap->loginRange.sequential);
    rangeSetFirstCount(&wmap->domainRange, wmap->domainRange.first,
		       wmap->domainRange.span, wmap->domainRange.sequential);
    rangeSetFirstCount(&wmap->addressRange, wmap->addressRange.first,
		       wmap->addressRange.span, wmap->addressRange.sequential);

    return 1;
}

/*
  Parse parameter names for the WMAP section.
 */
static int
WmapParseNameValue(pmail_command_t cmd, char *name, char *tok)
{
    wmap_command_t	*wmap = (wmap_command_t *)cmd->data;
    D_PRINTF(stderr, "WmapParseNameValue(name='%s' value='%s')\n", name, tok);

    /* find a home for the attr/value */
    if (cmdParseNameValue(cmd, name, tok))/* generic stuff */
	;				/* done */
    else if (strcmp(name, "server") == 0)
	wmap->hostInfo.hostName = mystrdup(tok);
    else if (strcmp(name, "loginformat") == 0)
	wmap->loginFormat = mystrdup(tok);
    else if (strcmp(name, "firstlogin") == 0)
	wmap->loginRange.first = atoi(tok);
    else if (strcmp(name, "numlogins") == 0)
	wmap->loginRange.span = atoi(tok);
    else if (strcmp(name, "sequentiallogins") == 0)
	wmap->loginRange.sequential = atoi(tok);
    else if (strcmp(name, "firstdomain") == 0)
	wmap->domainRange.first = atoi(tok);
    else if (strcmp(name, "numdomains") == 0)
	wmap->domainRange.span = atoi(tok);
    else if (strcmp(name, "sequentialdomains") == 0)
	wmap->domainRange.sequential = atoi(tok);
    else if (strcmp(name, "smtpmailfrom") == 0)
	wmap->smtpMailFrom = mystrdup(tok);
    else if (strcmp(name, "addressformat") == 0)
	wmap->addressFormat = mystrdup(tok);
    else if (strcmp(name, "firstaddress") == 0)
	wmap->addressRange.first = atoi(tok);
    else if (strcmp(name, "numaddresses") == 0)
	wmap->addressRange.span = atoi(tok);
    else if (strcmp(name, "sequentialaddresses") == 0)
	wmap->addressRange.sequential = atoi(tok);
    else if (strcmp(name, "file") == 0)
	wmap->filename = mystrdup(tok);
    else if (strcmp(name, "numrecips") == 0)
	wmap->numRecipients = atoi(tok);
    else if (strcmp(name, "numrecipients") == 0)
	wmap->numRecipients = atoi(tok);
    else if (strcmp(name, "portnum") == 0)
	wmap->hostInfo.portNum = atoi(tok);
    else if (strcmp(name, "passwdformat") == 0)
	wmap->passwdFormat = mystrdup(tok);
    else if (strcmp(name, "leavemailonserver") == 0)
	wmap->leaveMailOnServer = atoi(tok);
    else if (strcmp(name, "wmapclientheader") == 0)
	wmap->wmapClientHeader = mystrdup(tok);
    else if (strcmp(name, "wmapbannercmds") == 0)
	stringListAdd(&wmap->wmapBannerCmds, tok);
    else if (strcmp(name, "wmaplogincmd") == 0)
	wmap->wmapLoginCmd = mystrdup(tok);
    else if (strcmp(name, "wmaplogindata") == 0)
	wmap->wmapLoginData = mystrdup(tok);
    else if (strcmp(name, "wmapinboxcmds") == 0)
	stringListAdd(&wmap->wmapInboxCmds, tok);
    else if (strcmp(name, "wmapcheckcmds") == 0)
	stringListAdd(&wmap->wmapCheckCmds, tok);
    else if (strcmp(name, "wmapmsgreadcmds") == 0)
	stringListAdd(&wmap->wmapMsgReadCmds, tok);
    else if (strcmp(name, "wmapmsgwritecmds") == 0)
	stringListAdd(&wmap->wmapMsgWriteCmds, tok);
    else if (strcmp(name, "wmaplogoutcmds") == 0)
	stringListAdd(&wmap->wmapLogoutCmds, tok);
    else {
	return -1;
    }
    return 0;
}

/* PROTOCOL specific */
void
WmapStatsInit(mail_command_t *cmd, cmd_stats_t *p, int procNum, int threadNum)
{
    assert (NULL != p);

    if (!p->data) {			/* create it  */
	p->data = mycalloc(sizeof (wmap_stats_t));
    } else {				/* zero it */
	memset(p->data, 0, sizeof (wmap_stats_t));
    }

    if (cmd) {				/* do sub-range calulations */
	wmap_command_t	*wmap = (wmap_command_t *)cmd->data;
	wmap_stats_t	*stats = (wmap_stats_t *)p->data;

	rangeSplit(&wmap->loginRange, &stats->loginRange,
		   procNum, threadNum);
	rangeSplit(&wmap->domainRange, &stats->domainRange,
		   procNum, threadNum);
	rangeSplit(&wmap->addressRange, &stats->addressRange,
		   procNum, threadNum);
					/* initialize sequential range */
	stats->lastLogin = (stats->loginRange.sequential < 0)
	    ? stats->loginRange.last+1 : stats->loginRange.first-1;
	stats->lastDomain = (stats->domainRange.sequential < 0)
	    ? stats->domainRange.last+1 : stats->domainRange.first-1;
	stats->lastAddress = (stats->addressRange.sequential < 0)
	    ? stats->addressRange.last+1 : stats->addressRange.first-1;
    }
}

/* PROTOCOL specific */
void
WmapStatsUpdate(protocol_t *proto, cmd_stats_t *sum, cmd_stats_t *incr)
{
    wmap_stats_t	*ss = (wmap_stats_t *)sum->data;
    wmap_stats_t	*is = (wmap_stats_t *)incr->data;

    event_sum(&sum->idle,	&incr->idle);
    event_sum(&ss->connect,	&is->connect);
    event_sum(&ss->reconnect,	&is->reconnect);
    event_sum(&ss->banner,	&is->banner);
    event_sum(&ss->login, 	&is->login);
    event_sum(&ss->cmd, 	&is->cmd);
    event_sum(&ss->headers, 	&is->headers);
    event_sum(&ss->msgread, 	&is->msgread);
    event_sum(&ss->msgwrite, 	&is->msgwrite);
    event_sum(&ss->logout, 	&is->logout);

    event_reset(&incr->combined);	/* figure out total */
    event_sum(&incr->combined, &incr->idle);
    event_sum(&incr->combined, &is->connect);
    event_sum(&incr->combined, &is->reconnect);
    event_sum(&incr->combined, &is->banner);
    event_sum(&incr->combined, &is->login);
    event_sum(&incr->combined, &is->cmd);
    event_sum(&incr->combined, &is->headers);
    event_sum(&incr->combined, &is->msgread);
    event_sum(&incr->combined, &is->msgwrite);
    event_sum(&incr->combined, &is->logout);

    event_sum(&sum->combined, &incr->combined);	/* add our total to sum-total*/

    sum->totalerrs += incr->totalerrs;
    sum->totalcommands += incr->totalcommands;
}

/*  PROTOCOL specific */
void
WmapStatsOutput(protocol_t *proto, cmd_stats_t *ptimer, char *buf)
{
    char eventtextbuf[SIZEOF_EVENTTEXT];
    wmap_stats_t	*stats = (wmap_stats_t *)ptimer->data;

    *buf = 0;

					/* BUG blocks done in clientSummary */
    event_to_text(&ptimer->combined, eventtextbuf);
    sprintf(&buf[strlen(buf)], "total=%s ", eventtextbuf);

    event_to_text(&stats->connect, eventtextbuf);
    sprintf(&buf[strlen(buf)], "conn=%s ", eventtextbuf);

    event_to_text(&stats->reconnect, eventtextbuf);
    sprintf(&buf[strlen(buf)], "reconn=%s ", eventtextbuf);

    event_to_text(&stats->banner, eventtextbuf);
    sprintf(&buf[strlen(buf)], "banner=%s ", eventtextbuf);

    event_to_text(&stats->login, eventtextbuf);
    sprintf(&buf[strlen(buf)], "login=%s ", eventtextbuf);

    event_to_text(&stats->cmd, eventtextbuf);
    sprintf(&buf[strlen(buf)], "cmd=%s ", eventtextbuf);

    event_to_text(&stats->headers, eventtextbuf);
    sprintf(&buf[strlen(buf)], "headers=%s ", eventtextbuf);

    event_to_text(&stats->msgwrite, eventtextbuf);
    sprintf(&buf[strlen(buf)], "submit=%s ", eventtextbuf);

    event_to_text(&stats->msgread, eventtextbuf);
    sprintf(&buf[strlen(buf)], "retrieve=%s ", eventtextbuf);

    event_to_text(&stats->logout, eventtextbuf);
    sprintf(&buf[strlen(buf)], "logout=%s ", eventtextbuf);

    event_to_text(&ptimer->idle, eventtextbuf);
    sprintf(&buf[strlen(buf)], "idle=%s ", eventtextbuf);

}

/*  PROTOCOL specific */
void
WmapStatsFormat(protocol_t *pp,
		const char *extra,	/* extra text to insert (client=) */
		char *buf)
{
    static char	*timerList[] = {	/* must match order of StatsOutput */
	"total",
	"conn", "reconn", "banner", "login", "cmd", "headers", "submit", "retrieve", "logout",
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
    strcat(cp, "</FORMAT>\n");
}    

/*
  Get the initial screen
*/
static int
wmapBanner(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer, doWMAP_state_t *me)
{
    wmap_command_t	*wmap = (wmap_command_t *)cmd->data;
    wmap_stats_t	*stats = (wmap_stats_t *)ptimer->data;
    string_list_t	*pl;
    char	header[MAX_COMMAND_LEN];
    char	command[MAX_COMMAND_LEN];
    int		rc = 0;

    if (BADSOCKET(ptcx->sock)) {	/* should always be true */
	rc = HttpConnect(ptcx, &wmap->hostInfo, &stats->connect);
	if (rc == -1) {
	    return -1;
	}
    }

    /* BUG D_PRINTF(stderr, "wmapBanner()\n", pl->value);*/

    for (pl = &wmap->wmapBannerCmds; pl && pl->value; pl = pl->next) {

	D_PRINTF(stderr, "wmapBanner() cmd=[%s]\n", pl->value);

	sprintf(header, wmap->wmapClientHeader, wmap->hostInfo.hostName, wmap->hostInfo.hostName);
	sprintf(command, "%s\r\n%s\r\n", pl->value, header);
	assert (strlen(command) < MAX_COMMAND_LEN);

	rc = HttpCommandResponse(ptcx, cmd, me,
				   &wmap->hostInfo, &stats->reconnect,
				   &stats->banner,
				   command, me->response_buffer,
				   sizeof(me->response_buffer), NULL);
	if (rc == -1) {
	    if (gf_timeexpired < EXIT_FAST) {
		returnerr(debugfile,"%s WMAP cannot get banner page [%s] %s\n",
			  ptcx->errMsg, wmap->wmapBannerCmds, neterrstr());
	    }
	    return -1;
	}
    }

    D_PRINTF(stderr, "wmapBanner() done.\n", wmap->wmapBannerCmds);

    return rc;
}

/*
  Login by posting the necessary information
  Parse off the re-direct URL and session ID.
*/
static int
wmapLogin(
    ptcx_t ptcx,
    mail_command_t *cmd,
    cmd_stats_t *ptimer,
    doWMAP_state_t *me)
{
    wmap_command_t	*wmap = (wmap_command_t *)cmd->data;
    wmap_stats_t	*stats = (wmap_stats_t *)ptimer->data;
    char	header[MAX_COMMAND_LEN];
    char	content[MAX_COMMAND_LEN];
    char	command[MAX_COMMAND_LEN];
    char	mailUser[MAX_MAILADDR_LEN];
    char	userPasswd[MAX_MAILADDR_LEN];
    unsigned long loginNum;
    unsigned long domainNum;
    int		rc;
    char	*cp;

    if (BADSOCKET(ptcx->sock)) {
	rc = HttpConnect(ptcx, &wmap->hostInfo, &stats->reconnect);
	if (rc == -1) {
	    return -1;
	}
    }

    D_PRINTF(stderr, "wmapLogin() wmapLoginCmd=[%s], wmapLoginData=[%s]\n",
	     wmap->wmapLoginCmd, wmap->wmapLoginData);

    /* generate a random username and domain(with a mailbox on the server) */
    domainNum = rangeNext(&stats->domainRange, stats->lastDomain);
    stats->lastDomain = domainNum;
    loginNum = rangeNext(&stats->loginRange, stats->lastLogin);
    stats->lastLogin = loginNum;

    sprintf(mailUser, wmap->loginFormat, loginNum, domainNum);
    sprintf(userPasswd, wmap->passwdFormat, loginNum);
    D_PRINTF(stderr,"mailUser=%s pass=%s\n", mailUser, userPasswd);

    /* send wmapLoginCmd, read response into wmapRedirectURL */
    sprintf(content, wmap->wmapLoginData, mailUser, userPasswd);
    sprintf(header, wmap->wmapClientHeader, wmap->hostInfo.hostName, wmap->hostInfo.hostName);
    sprintf(command, "%s\r\n%s"
	    HTTP_CONTENT_TYPE_POST "\r\n"
	    HTTP_CONTENT_LENGTH " %d\r\n\r\n%s\r\n",
	    wmap->wmapLoginCmd, header, strlen(content), content);
    assert (strlen(command) < MAX_COMMAND_LEN);

    rc = HttpCommandResponse(ptcx, cmd, me,
			       &wmap->hostInfo, &stats->reconnect,
			       &stats->banner,
			       command, me->response_buffer,
			       sizeof(me->response_buffer), NULL);
    if (rc == -1) {
	if (gf_timeexpired < EXIT_FAST) {
	    NETCLOSE(ptcx->sock); ptcx->sock = BADSOCKET_VALUE;
	    returnerr(debugfile,"%s WMAP cannot login user=%s pass=%s %s\n",
		      ptcx->errMsg, mailUser, userPasswd, neterrstr());
	}
	return -1;
    }
    if (me->redirectURL) {		/* clean out old values */
	free(me->redirectURL);
	me->redirectURL = NULL;
    }
    if (me->sessionID) {
	free(me->sessionID);
	me->sessionID = NULL;
    }
    if (me->msgList) {
	free(me->msgList);
	me->msgList = NULL;
    }

    cp = strstr(me->response_buffer, HTTP_LOCATION);
    if (cp == NULL) {
	stats->login.errs++;
	returnerr(debugfile, "WMAP cannot find login redirect location in [%s]\n", me->response_buffer);
	return -1;
    }

    cp += strlen(HTTP_LOCATION)+1;
    /* skip protocol */
    if (!strncmp(cp, HTTP_PROTOCOL, strlen(HTTP_PROTOCOL))) {
	cp += strlen(HTTP_PROTOCOL);
    }
    /* skip host */
    cp = strchr(cp, '/');
    me->redirectURL = mystrdup (cp);
    assert (NULL != me->redirectURL);
    cp = strchr(me->redirectURL, '\r');
    *cp = '\0';

    D_PRINTF(stderr, "redirect location URL = [%s]\n", me->redirectURL);

    /* parse wmapRedirectURL and extract wmapSessionID */
    content[0] = '\0';
    cp = strstr(me->redirectURL, "sid=");
    if (cp) sscanf(cp, "sid=%[^&\n]", content);
    if (content[0] == '\0') {
	stats->login.errs++;
	returnerr(debugfile,
		  "WMAP cannot find sid= in redirect [%s], login probably failed.\n",
		  me->redirectURL);
	return -1;
    }

    me->sessionID = mystrdup(content);
    assert (NULL != me->sessionID);

    D_PRINTF(stderr, "sid=%s\n", me->sessionID);

    return rc;
}

/* Get the "inbox" page with all its contents */
static int
wmapInbox(
    ptcx_t ptcx,
    mail_command_t *cmd,
    cmd_stats_t *ptimer,
    doWMAP_state_t *me)
{
    wmap_command_t	*wmap = (wmap_command_t *)cmd->data;
    wmap_stats_t	*stats = (wmap_stats_t *)ptimer->data;
    string_list_t	*pl;
    int		rc = 0;
    char	header[MAX_COMMAND_LEN];
    char	getcmd[MAX_COMMAND_LEN];
    char	command[MAX_COMMAND_LEN];

    D_PRINTF(stderr, "wmapInbox()\n");

    /* First grab the redirect URL */
    sprintf(command, "GET %s HTTP/1.0\r\n%s" HTTP_CONTENT_LENGTH " 0\r\n\r\n",
	    me->redirectURL, header);
    rc = HttpCommandResponse(ptcx, cmd,  me,
			       &wmap->hostInfo, &stats->reconnect,
			       &stats->banner,
			       command, me->response_buffer,
			       sizeof(me->response_buffer), NULL);
    if (rc == -1) {			/* most common disconnect point */
	if (gf_timeexpired < EXIT_FAST) {
	    returnerr(debugfile,"%s WMAP cannot get inbox %s\n",
		      ptcx->errMsg, neterrstr());
	    return -1;
	}
    }
    /*D_PRINTF(stderr, "redirect inbox page=[%s]\n", me->response_buffer);*/

    /* Now get all the user configure commands */
    for (pl = &wmap->wmapInboxCmds; pl && pl->value; pl = pl->next) {
	D_PRINTF(stderr, "wmapInboxCmd() cmd=[%s]\n", pl->value);

    /* send wmapInboxCmd, read response into wmapRedirectURL */
	sprintf(header, wmap->wmapClientHeader,
		wmap->hostInfo.hostName, wmap->hostInfo.hostName);
	sprintf(getcmd, pl->value,
		me->sessionID);
	sprintf(command, "%s\r\n%s" HTTP_CONTENT_LENGTH " 0\r\n\r\n",
		getcmd, header);
	assert (strlen(command) < MAX_COMMAND_LEN);

	rc = HttpCommandResponse(ptcx, cmd,  me,
				   &wmap->hostInfo, &stats->reconnect,
				   &stats->banner,
				   command, me->response_buffer,
				   sizeof(me->response_buffer), NULL);
	if (rc == -1) {			/* most common disconnect point */
	    if (gf_timeexpired < EXIT_FAST) {
		returnerr(debugfile,"%s WMAP cannot get inbox %s\n",
			  ptcx->errMsg, neterrstr());
		return -1;
	    }
	}
    }

    return rc;
}

static int
wmapLogout(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer, doWMAP_state_t *me)
{
    wmap_command_t	*wmap = (wmap_command_t *)cmd->data;
    wmap_stats_t	*stats = (wmap_stats_t *)ptimer->data;
    string_list_t	*pl;
    char	header[MAX_COMMAND_LEN];
    char	getcmd[MAX_COMMAND_LEN];
    char	command[MAX_COMMAND_LEN];
    int		rc = 0;

    if (BADSOCKET(ptcx->sock)) {
	rc = HttpConnect(ptcx, &wmap->hostInfo, &stats->reconnect);
	if (rc == -1) {
	    return -1;
	}
    }

    D_PRINTF(stderr, "wmapLogout()\n");

    for (pl = &wmap->wmapLogoutCmds; pl && pl->value; pl = pl->next) {

	D_PRINTF(stderr, "wmapLogout() cmd=[%s]\n", pl->value);

	sprintf(header, wmap->wmapClientHeader, wmap->hostInfo.hostName, wmap->hostInfo.hostName);
	sprintf(getcmd, pl->value, me->sessionID);
	sprintf(command, "%s\r\n%s" HTTP_CONTENT_LENGTH " 0\r\n\r\n", getcmd, header);

	assert (strlen(command) < MAX_COMMAND_LEN);
	rc = HttpCommandResponse(ptcx, cmd,  me,
				   &wmap->hostInfo, &stats->reconnect,
				   &stats->banner,
				   command, me->response_buffer,
				   sizeof(me->response_buffer), NULL);
	if (rc == -1) {
	    if (gf_timeexpired < EXIT_FAST) {
		returnerr(debugfile,"%s WMAP cannot get logout page [%s] %s\n",
			  ptcx->errMsg, wmap->wmapLogoutCmds, neterrstr());
	    }
	    return -1;
	}
    }

    D_PRINTF(stderr, "wmapLogout() done, response=[%s]\n", me->response_buffer);

    return rc;
}

void *
doWmapStart(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer)
{
    doWMAP_state_t	*me = (doWMAP_state_t *)mycalloc (sizeof (doWMAP_state_t));
    /*wmap_stats_t	*stats = (wmap_stats_t *)ptimer->data;*/
    /*wmap_command_t	*wmap = (wmap_command_t *)cmd->data;*/
    int			rc;

    D_PRINTF(stderr, "doWmapStart()\n");

    if (!me) return NULL;

    me->redirectURL = NULL;
    me->sessionID = NULL;
    me->msgList = NULL;
    me->numMsgs = 0;
    me->msgCounter = 0;
  
    ptcx->sock = BADSOCKET_VALUE;

    /* get banner pages */
    rc = wmapBanner(ptcx, cmd, ptimer, me);
    if (rc == -1) {
	doWmapExit(ptcx, cmd, me);
	return NULL;
    }

    /* get other login pages and perform login */
    rc = wmapLogin(ptcx, cmd, ptimer, me);
    if (rc == -1) {
	doWmapExit(ptcx, cmd, me);
	return NULL;
    }

    /* get main inbox screens */
    rc = wmapInbox(ptcx, cmd, ptimer, me);
    if (rc == -1) {
	doWmapExit(ptcx, cmd, me);
	return NULL;
    }

    return me;
}

/* Parse callback to find the message info for a folder */
static int
GetMessageNumbers (
    ptcx_t ptcx,
    pmail_command_t cmd,
    void *conState,
    char *buffer,
    int bytesInBuffer,
    char **searchStart,
    const char **findStr)
{
    doWMAP_state_t	*me = (doWMAP_state_t *)conState;
    static const char MSG_NEW_ARRAY[] = "\nvar msg=new Array(";
    static const char MSG_ENTRY[] = "\nmsg[";
    assert (NULL != findStr);
    assert (NULL != searchStart);
    if ((NULL == *findStr) && (NULL == *searchStart)) {
	/*D_PRINTF (stderr, "GetMessageNumbers() Final call with %d bytes\n",
	  bytesInBuffer);*/
    }
    else if (NULL == *findStr) {
	/*D_PRINTF (stderr, "GetMessageNumbers() Initial call with %d bytes\n",
	  bytesInBuffer);*/
	/* Set string to search for */
	*findStr = MSG_NEW_ARRAY;
    }
    else {				/* string match */
	/*D_PRINTF (stderr, "GetMessageNumbers() called with %d bytes find=[%s] offset=%d buffer=[%s]\n",
	  bytesInBuffer, *findStr, *searchStart - buffer, buffer);*/
	if (MSG_NEW_ARRAY == *findStr) { /* got initial length string */
	    me->numMsgs = atoi (*searchStart);
	    D_PRINTF (stderr, "GetMessageNumber() length=%d\n", me->numMsgs);
	    if (me->numMsgs > 0) {
		if (me->msgList)
		    free (me->msgList);
		me->msgList = (int *)mycalloc (me->numMsgs * sizeof (int));
		assert (NULL != me->msgList);
	    }
	    *findStr = MSG_ENTRY;	/* now search for each message */
	} else {			/* got a message string */
	    int index, id, len, date, flags;
	    /*char from[256], subject[256];*/
	    int n;
	    n = sscanf (*searchStart, "%d]=new M(%d,%d,%d,%d,",
		    &index, &id, &len, &date, &flags);
	    if (n < 5) {		/* need more data */
		return 1;
	    }

	    D_PRINTF (stderr,
		      "GetMessageNumber() index=%d id=%d len=%d date=%d flags=%d\n",
		      index, id, len, date, flags);
	    assert (index < me->numMsgs);
	    me->msgList[index] = id;	/* hold onto the msg id */
	}
	*searchStart = strchr (*searchStart, '\n'); /* skip to end of line */
    }
    return 0;
}

/*
  This is the "check messages" loop within a block.
  Similar to IMAP, it should check new mail every minute or so.
*/
int
doWmapLoop(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer, void *mystate)
{
    wmap_command_t	*wmap = (wmap_command_t *)cmd->data;
    wmap_stats_t	*stats = (wmap_stats_t *)ptimer->data;
    doWMAP_state_t	*me = (doWMAP_state_t *)mystate;
    string_list_t	*pl;
    int		rc = 0;
    int		seq;			/* message sequence numbers */
    char	header[MAX_COMMAND_LEN];
    char	getcmd[MAX_COMMAND_LEN];
    char	command[MAX_COMMAND_LEN];

    D_PRINTF(stderr, "doWmapLoop()\n");

    /* send the WMAP command to get message info, parse results on the fly */
    for (pl = &wmap->wmapCheckCmds; pl && pl->value; pl = pl->next) {
	if (gf_timeexpired >= EXIT_FAST) break;
	D_PRINTF(stderr, "wmapCheckCmd() cmd=[%s]\n", pl->value);

	sprintf(getcmd, pl->value,
		me->sessionID);
	sprintf(header, wmap->wmapClientHeader,
		wmap->hostInfo.hostName, wmap->hostInfo.hostName);
	sprintf(command, "%s\r\n%s" HTTP_CONTENT_LENGTH " 0\r\n\r\n",
		getcmd, header);
	assert (strlen(command) < MAX_COMMAND_LEN);

	rc = HttpCommandResponse(ptcx, cmd,  me,
				   &wmap->hostInfo, &stats->reconnect,
				   &stats->banner,
				   command, me->response_buffer,
				   sizeof(me->response_buffer), &GetMessageNumbers);
	if (rc == -1) {			/* un-recoverable error */
	    if (gf_timeexpired < EXIT_FAST) {
		returnerr(debugfile,"%s WMAP cannot check messages %s\n",
			  ptcx->errMsg, neterrstr());
	    }
	    doWmapExit(ptcx, cmd, me);
	    return -1;
	}
    }

					/* read messages */
    D_PRINTF(stderr, "doWmapLoop() reading from %d to %d\n",
	     me->msgCounter, me->numMsgs);

    for (seq = me->msgCounter; seq < me->numMsgs; ++seq) {
	assert (NULL != me->msgList);
	if (gf_timeexpired >= EXIT_FAST) break;
	/* send the WMAP command to get the message */
	for (pl = &wmap->wmapMsgReadCmds; pl && pl->value; pl = pl->next) {
	    if (gf_timeexpired >= EXIT_FAST) break;
	    D_PRINTF(stderr, "wmapMsgReadCmd() cmd=[%s]\n", pl->value);

	    sprintf(getcmd, pl->value,	/* fill out command string */
		    me->sessionID, me->msgList[seq]);
	    sprintf(header, wmap->wmapClientHeader, /* fill out header */
		    wmap->hostInfo.hostName, wmap->hostInfo.hostName);
	    sprintf(command, "%s\r\n%s" HTTP_CONTENT_LENGTH " 0\r\n\r\n",
		    getcmd, header);
	    assert (strlen(command) < MAX_COMMAND_LEN);

	    rc = HttpCommandResponse(ptcx, cmd,  me,
				       &wmap->hostInfo, &stats->reconnect,
				       &stats->banner,
				       command, me->response_buffer,
				       sizeof(me->response_buffer), NULL);
	    if (rc == -1) {			/* un-recoverable error */
		returnerr(debugfile,"%s WMAP cannot read messages %s\n",
			  ptcx->errMsg, neterrstr());
		doWmapExit(ptcx, cmd, me);
		return -1;
	    }

	    if (strstr (pl->value, "GET /msg.msc")) {
		D_PRINTF (stderr, "doWmapLoop() message body [%s]\n",
			  me->response_buffer);
	    }
	}
	D_PRINTF (stderr, "doWmapLoop() read message %d\n", seq);
	me->msgCounter++;
    }

    D_PRINTF(stderr, "doWmapLoop Done.\n");
    return 0;
}

/*
  This is the "close connection" end of the block.
*/
void
doWmapEnd(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer, void *mystate)
{
    doWMAP_state_t	*me = (doWMAP_state_t *)mystate;
    /*wmap_stats_t	*stats = (wmap_stats_t *)ptimer->data;*/
    int	rc;

    D_PRINTF(stderr, "doWmapEnd()\n");

    if (BADSOCKET(ptcx->sock)) return;	/* closed by previous error */

    rc = wmapLogout(ptcx, cmd, ptimer, me);
    if (rc == -1) {
	/* error during logout... */
    }

    doWmapExit(ptcx, cmd, me);
}

void
doWmapExit(ptcx_t ptcx, mail_command_t *cmd, doWMAP_state_t *me)
{
    /*wmap_command_t	*wmap = (wmap_command_t *)cmd->data;*/

    D_PRINTF(stderr, "doWmapExit()\n");

    if (!BADSOCKET(ptcx->sock)) {
	NETCLOSE(ptcx->sock);
	ptcx->sock = BADSOCKET_VALUE;
    }

    if (me->redirectURL) {
	free(me->redirectURL);
    }
    if (me->sessionID) {
	free(me->sessionID);
    }
    if (me->msgList) {
	free (me->msgList);
    }

    myfree(me);
}  
