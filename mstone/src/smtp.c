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
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):	Dan Christian <robodan@netscape.com>
 *			Marcel DePaolis <marcel@netcape.com>
 *			Mike Blakely
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
/* SMTP protocol tests */
 
#include "bench.h"
#include "pish.h"

#define MAXCOMMANDLEN	256

typedef struct _doSMTP_state {
    int	nothing;
} doSMTP_state_t;

static void doSMTPExit  (ptcx_t ptcx, doSMTP_state_t	*me);

/*
  Set defaults in command structure
*/
int
SmtpParseStart (pmail_command_t cmd,
		char *line,
		param_list_t *defparm)
{
    param_list_t	*pp;
    pish_command_t	*pish = (pish_command_t *)mycalloc
	(sizeof (pish_command_t));
    cmd->data = pish;

    cmd->numLoops = 1;		/* default 1 message */
    pish->hostInfo.portNum = SMTP_PORT;	/* get default port */

    D_PRINTF(stderr, "Smtp Assign defaults\n");
    /* Fill in defaults first, ignore defaults we dont use */
    for (pp = defparm; pp; pp = pp->next) {
	(void)pishParseNameValue (cmd, pp->name, pp->value);
    }

    return 1;
}

/*
  Fill in command structure from a list of lines
 */
int
SmtpParseEnd (pmail_command_t cmd,
		string_list_t *section,
		param_list_t *defparm)
{
    string_list_t	*sp;
    int fd;
    int bytesRead;
    struct stat statbuf;
    pish_command_t	*pish = (pish_command_t *)cmd->data;

    /* Now parse section lines */
    D_PRINTF(stderr, "Smtp Assign section lines\n");
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
    if (!pish->hostInfo.hostName) {
	D_PRINTF(stderr,"missing server for command");
	return returnerr(stderr,"missing server for command\n");
    }

    if (!pish->loginFormat) {
	D_PRINTF(stderr,"missing loginFormat for command");
	return returnerr(stderr,"missing loginFormat for command\n");
    }

    if (!pish->passwdFormat) {
	D_PRINTF(stderr,"missing passwdFormat for command");
	return returnerr(stderr,"missing passwdFormat for command\n");
    }

    if (!pish->addressFormat) {
	D_PRINTF(stderr,"missing addressFormat for command");
	return returnerr(stderr,"missing addressFormat for command\n");
    }

    /* check for required attrs */
    if (!pish->filename) {
	D_PRINTF(stderr,"missing file for SMTP command");
	return returnerr(stderr,"missing file for SMTP command\n");
    }

	/* read the contents of file into struct */
    memset(&statbuf, 0, sizeof(statbuf));
    if (stat(pish->filename, &statbuf) != 0) {
	return returnerr(stderr,"Couldn't stat file %s: errno=%d: %s\n", 
			 pish->filename, errno, strerror(errno));
    }

    /* open file */
    if ((fd = open(pish->filename, O_RDONLY)) <= 0) {
	return returnerr(stderr, "Cannot open file %s: errno=%d: %s\n",
			 pish->filename, errno, strerror(errno));
    }

    /* read into loaded_comm_list */
#define MSG_TRAILER CRLF "." CRLF

    pish->msgsize = statbuf.st_size;
    pish->msgMailFrom = NULL;
    pish->msgdata = (char *) mycalloc(pish->msgsize+strlen(MSG_TRAILER)+1);

    if ((bytesRead = read(fd, pish->msgdata, pish->msgsize)) <= 0) {
	close(fd);
	return returnerr(stderr, "Cannot read file %s: errno=%d: %s\n", 
			 pish->filename, errno, strerror(errno));
    }

    if (bytesRead != pish->msgsize) {
	returnerr(stderr, "Error reading file %s, got %d expected %d\n",
		  pish->filename, bytesRead, pish->msgsize);
	close(fd);
	return -1;
    }

    pish->msgdata[pish->msgsize] = 0;

    /* clean up message to rfc822 style */

#define PROTO_HEADER "SMTP"
#define PROTO_MAIL_FROM "\nMAIL FROM:"
#define PROTO_DATA "\nDATA\n"

    D_PRINTF (stderr, "checking for PROTO_HEADER [%s]\n", PROTO_HEADER);
    if (strncmp(pish->msgdata, PROTO_HEADER, strlen(PROTO_HEADER)) == 0) {
	    char *cp, *cp2;
	    int off;

	    D_PRINTF(stderr, "got PROTO_HEADER\n");
	    D_PRINTF(stderr, "find PROTO_MAIL_FROM...\n");
	    cp = strstr(pish->msgdata, PROTO_MAIL_FROM); 
	    if (cp != NULL) {
		    cp += strlen(PROTO_MAIL_FROM);
		    cp2 = strchr(cp, '\n');
		    off = cp2 - cp;
		    pish->msgMailFrom = (char *) mycalloc(off+1);
		    memcpy(pish->msgMailFrom, cp, off);
		    pish->msgMailFrom[off] = 0;
		    D_PRINTF(stderr, "got PROTO_MAIL_FROM:%s\n",pish->msgMailFrom);
	    }

	    D_PRINTF(stderr, "find PROTO_DATA...\n");
	    cp = strstr(pish->msgdata, PROTO_DATA);
	    if (cp != NULL) {
		    off = cp - pish->msgdata;
		    off += strlen(PROTO_DATA);
		    D_PRINTF(stderr, "got past PROTO_DATA at off=%d\n", off);
		    char *newmsg = (char *) mycalloc(pish->msgsize+strlen(MSG_TRAILER)+1-off);
		    memcpy(newmsg, pish->msgdata+off, pish->msgsize-off+1);
		    myfree(pish->msgdata);
		    pish->msgdata = newmsg;
		    D_PRINTF(stderr, "done msg shrink\n");
	    }
    }


    strcat(pish->msgdata, MSG_TRAILER);

    close(fd);

    /* see if we can resolve the mailserver addr */
    if (resolve_addrs(pish->hostInfo.hostName, "tcp",
		      &(pish->hostInfo.host_phe),
		      &(pish->hostInfo.host_ppe),
		      &(pish->hostInfo.host_addr),
		      &(pish->hostInfo.host_type))) {
	return returnerr (stderr, "Error resolving hostname '%s'\n",
			  pish->hostInfo.hostName);
    } else {
	pish->hostInfo.resolved = 1;	/* mark the hostInfo resolved */
    }
    rangeSetFirstCount (&pish->loginRange, pish->loginRange.first,
			   pish->loginRange.span, pish->loginRange.sequential);
    rangeSetFirstCount (&pish->domainRange, pish->domainRange.first,
			   pish->domainRange.span, pish->domainRange.sequential);
    rangeSetFirstCount (&pish->addressRange, pish->addressRange.first,
			   pish->addressRange.span, pish->addressRange.sequential);

    return 1;
}

/* ====================================================================== 
   These routine will become protcol specific.
   Since SMTP is the most complex, they are here for now.
 */

/*
  TRANSITION: handles all the old names-fields for POP, IMAP, SMTP, HTTP
  This goes away when commands have protocol extensible fields
 */
int
pishParseNameValue (pmail_command_t cmd,
		    char *name,
		    char *tok)
{
    pish_command_t	*pish = (pish_command_t *)cmd->data;
    D_PRINTF (stderr, "pishParseNameValue(name='%s' value='%s')\n", name, tok);

    /* find a home for the attr/value */
    if (cmdParseNameValue(cmd, name, tok))
	;				/* done */
    else if (strcmp(name, "server") == 0)
	pish->hostInfo.hostName = mystrdup (tok);
    else if (strcmp(name, "smtpmailfrom") == 0)
	pish->smtpMailFrom = mystrdup (tok);
    else if (strcmp(name, "loginformat") == 0)
	pish->loginFormat = mystrdup (tok);
    else if (strcmp(name, "addressformat") == 0)
	pish->addressFormat = mystrdup (tok);
    else if (strcmp(name, "firstlogin") == 0)
	pish->loginRange.first = atoi(tok);
    else if (strcmp(name, "numlogins") == 0)
	pish->loginRange.span = atoi(tok);
    else if (strcmp(name, "sequentiallogins") == 0)
	pish->loginRange.sequential = atoi(tok);
    else if (strcmp(name, "firstdomain") == 0)
	pish->domainRange.first = atoi(tok);
    else if (strcmp(name, "numdomains") == 0)
	pish->domainRange.span = atoi(tok);
    else if (strcmp(name, "sequentialdomains") == 0)
	pish->domainRange.sequential = atoi(tok);
    else if (strcmp(name, "firstaddress") == 0)
	pish->addressRange.first = atoi(tok);
    else if (strcmp(name, "numaddresses") == 0)
	pish->addressRange.span = atoi(tok);
    else if (strcmp(name, "sequentialaddresses") == 0)
	pish->addressRange.sequential = atoi(tok);
    else if (strcmp(name, "portnum") == 0)
	pish->hostInfo.portNum = atoi(tok);
    else if (strcmp(name, "numrecips") == 0)
	pish->numRecipients = atoi(tok);
    else if (strcmp(name, "numrecipients") == 0)
	pish->numRecipients = atoi(tok);
    else if (strcmp(name, "file") == 0)
	pish->filename= mystrdup (tok);
    else if (strcmp(name, "passwdformat") == 0)
	pish->passwdFormat = mystrdup (tok);
    else if (strcmp(name, "searchfolder") == 0)
	pish->imapSearchFolder = mystrdup (tok);
    else if (strcmp(name, "searchpattern") == 0)
	pish->imapSearchPattern = mystrdup (tok);
    else if (strcmp(name, "searchrate") == 0)
	pish->imapSearchRate = atoi(tok);
    else if (strcmp(name, "useehlo") == 0)
	pish->useEHLO = atoi(tok);
    else if (strcmp(name, "useauthlogin") == 0)
	pish->useAUTHLOGIN = atoi(tok);
    else if (strcmp(name, "useauthplain") == 0)
	pish->useAUTHPLAIN = atoi(tok);
    else {
	return -1;
    }
    return 0;
}

/* PROTOCOL specific */
void
pishStatsInit(mail_command_t *cmd, cmd_stats_t *p, int procNum, int threadNum)
{
    assert (NULL != p);

    if (!p->data) {			/* create it  */
	p->data = mycalloc (sizeof (pish_stats_t));
    } else {				/* zero it */
	memset (p->data, 0, sizeof (pish_stats_t));
    }

    if (cmd) {				/* do sub-range calulations */
	pish_command_t	*pish = (pish_command_t *)cmd->data;
	pish_stats_t	*stats = (pish_stats_t *)p->data;

	rangeSplit (&pish->loginRange, &stats->loginRange,
		    procNum, threadNum);
	rangeSplit (&pish->domainRange, &stats->domainRange,
		    procNum, threadNum);
	rangeSplit (&pish->addressRange, &stats->addressRange,
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
pishStatsUpdate(protocol_t *proto,
		 cmd_stats_t *sum,
		 cmd_stats_t *incr)
{
    pish_stats_t	*ss = (pish_stats_t *)sum->data;
    pish_stats_t	*is = (pish_stats_t *)incr->data;

    event_sum(&sum->idle, &incr->idle);
    event_sum(&ss->connect, &is->connect);
    event_sum(&ss->banner, &is->banner);
    event_sum(&ss->login, &is->login);
    event_sum(&ss->cmd, &is->cmd);
    event_sum(&ss->msgread, &is->msgread);
    event_sum(&ss->msgwrite, &is->msgwrite);
    event_sum(&ss->logout, &is->logout);

    event_reset(&incr->combined);	/* figure out total */
    event_sum(&incr->combined, &incr->idle);
    event_sum(&incr->combined, &is->connect);
    event_sum(&incr->combined, &is->banner);
    event_sum(&incr->combined, &is->login);
    event_sum(&incr->combined, &is->cmd);
    event_sum(&incr->combined, &is->msgread);
    event_sum(&incr->combined, &is->msgwrite);
    event_sum(&incr->combined, &is->logout);

    event_sum(&sum->combined, &incr->combined);	/* add our total to sum-total*/

    sum->totalerrs += incr->totalerrs;
    sum->totalcommands += incr->totalcommands;
}

/*  PROTOCOL specific */
void
pishStatsOutput(protocol_t *proto,
		  cmd_stats_t *ptimer,
		  char *buf)
{
    char eventtextbuf[SIZEOF_EVENTTEXT];
    pish_stats_t	*stats = (pish_stats_t *)ptimer->data;

    *buf = 0;

    /* blocks=%ld is handled for us */

    /* output proto independent total */
    event_to_text(&ptimer->combined, eventtextbuf);
    sprintf(&buf[strlen(buf)], "total=%s ", eventtextbuf);

    event_to_text(&stats->connect, eventtextbuf);
    sprintf(&buf[strlen(buf)], "conn=%s ", eventtextbuf);

    event_to_text(&stats->banner, eventtextbuf);
    sprintf(&buf[strlen(buf)], "banner=%s ", eventtextbuf);

    event_to_text(&stats->login, eventtextbuf);
    sprintf(&buf[strlen(buf)], "login=%s ", eventtextbuf);

    event_to_text(&stats->cmd, eventtextbuf);
    sprintf(&buf[strlen(buf)], "cmd=%s ", eventtextbuf);

    event_to_text(&stats->msgwrite, eventtextbuf);
    sprintf(&buf[strlen(buf)], "submit=%s ", eventtextbuf);

    event_to_text(&stats->msgread, eventtextbuf);
    sprintf(&buf[strlen(buf)], "retrieve=%s ", eventtextbuf);

    event_to_text(&stats->logout, eventtextbuf);
    sprintf(&buf[strlen(buf)], "logout=%s ", eventtextbuf);

    /* output proto independent idle */
    event_to_text(&ptimer->idle, eventtextbuf);
    sprintf(&buf[strlen(buf)], "idle=%s ", eventtextbuf);

}

/*  PROTOCOL specific */
void
pishStatsFormat (protocol_t *pp,
		  const char *extra,	/* extra text to insert (client=) */
		  char *buf)
{
    static char	*timerList[] = {	/* must match order of StatsOutput */
	"total",
	"conn", "banner", "login", "cmd", "submit", "retrieve", "logout",
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
		gs_eventToTextFormat);	/* match event_to_text*/
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



/* 
   Actual protocol handling code
*/
int
isSmtpResponseOK(char * s)
{
    if (s[0] == '2' || s[0] == '3')
	return 1;
    return 0;
}

int
doSmtpCommandResponse(ptcx_t ptcx, SOCKET sock, char *command, char *response, int resplen)
{
    int rc;

    T_PRINTF(ptcx->logfile, command, strlen (command), "SMTP SendCommand");
    rc = doCommandResponse(ptcx, sock, command, response, resplen);
    if (rc == -1)
	return rc;
    T_PRINTF(ptcx->logfile, response, strlen(response),
	     "SMTP ReadResponse");	/* telemetry log. should be lower level */
    /* D_PRINTF(stderr, "SMTP command=[%s] response=[%s]\n", command, response); */
    if (!isSmtpResponseOK(response)) {
	if (gf_timeexpired < EXIT_FAST) {
	    /* dont modify command (in case it could be re-tried) */
	    trimEndWhite (response);	/* clean up for printing */
	    strcpy (ptcx->errMsg, "SmtpCommandResponse: got SMTP error response");
	}
	return -1;
    }
    return 0;
}

static const char basis_64[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int
str_to_base64(const char *str, unsigned int len, char *buf, unsigned int buflen)
{
    unsigned int bufused = 0;
    int c1, c2, c3;

    while (len) {
	if (bufused >= buflen-4) {
	    /* out of space */
	    return -1;
	}
	
	c1 = (unsigned char)*str++;
	buf[bufused++] = basis_64[c1>>2];

	if (--len == 0) c2 = 0;
	else c2 = (unsigned char)*str++;
	buf[bufused++] = basis_64[((c1 & 0x3)<< 4) | ((c2 & 0xF0) >> 4)];

	if (len == 0) {
	    buf[bufused++] = '=';
	    buf[bufused++] = '=';
	    break;
	}

	if (--len == 0) c3 = 0;
	else c3 = (unsigned char)*str++;

	buf[bufused++] = basis_64[((c2 & 0xF) << 2) | ((c3 & 0xC0) >>6)];
	if (len == 0) {
	    buf[bufused++] = '=';
	    break;
	}

	--len;
	buf[bufused++] = basis_64[c3 & 0x3F];
    }

    if (bufused >= buflen-2) {
	/* out of space */
	return -1;
    }
    buf[bufused] = '\0';
    return bufused;
}

int
smtpAuthPlain(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer, SOCKET sock)
{
    char	command[MAX_COMMAND_LEN];
    char	respBuffer[MAX_RESPONSE_LEN];
    char	mailUser[MAX_MAILADDR_LEN];
    char	userPasswd[MAX_MAILADDR_LEN];
    unsigned long loginNum;
    unsigned long domainNum;
    char	auth_msg[1024];
    int		auth_msg_len=0;
    char	base64_buf[1024];
    int	rc;
    pish_stats_t	*stats = (pish_stats_t *)ptimer->data;
    pish_command_t	*pish = (pish_command_t *)cmd->data;

    /* generate a random username (with a mailbox on the server) */
    domainNum = rangeNext (&stats->domainRange, stats->lastDomain);
    stats->lastDomain = domainNum;
    loginNum = rangeNext (&stats->loginRange, stats->lastLogin);
    stats->lastLogin = loginNum;

    sprintf(mailUser, pish->loginFormat, loginNum, domainNum);
    sprintf(userPasswd, pish->passwdFormat, loginNum);

    D_PRINTF(debugfile,"mailUser=[%.64s]\n", mailUser);

    sprintf(command, "AUTH PLAIN%s", CRLF);

    event_start(ptcx, &stats->cmd);
    rc = doSmtpCommandResponse(ptcx, sock, command, respBuffer, sizeof(respBuffer));
    event_stop(ptcx, &stats->cmd);
    if (rc == -1) {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->login.errs++;
	    strcat (ptcx->errMsg, "<SmtpLogin: failure sending AUTH PLAIN");
	    trimEndWhite (command);
	    returnerr(debugfile, "%s command=[%.99s] response=[%.99s]\n", /* ??? */
		      ptcx->errMsg, command, respBuffer);
	}
	return -1;
    }

    /* should now have a 3xx continue code */

    /* authenticate id */
    strcpy(auth_msg, ""); /* who you want to be (must have rights to do this) */
    auth_msg_len += strlen("");
    auth_msg[auth_msg_len++] = '\0';
    /* authorize id */
    strcpy(auth_msg + auth_msg_len, mailUser); /* who you are */
    auth_msg_len += strlen(mailUser);
    auth_msg[auth_msg_len++] = '\0';
    /* password */
    strcpy(auth_msg + auth_msg_len, userPasswd); /* your credentials */
    auth_msg_len += strlen(userPasswd);

    if (str_to_base64(auth_msg, auth_msg_len, base64_buf, sizeof(base64_buf)) == -1) {
	stats->login.errs++;
	strcpy (ptcx->errMsg, "SmtpLogin: Internal error encoding user");
	returnerr(debugfile, "%s [%.199s]\n", ptcx->errMsg, mailUser); /* ??? */
	return -1;
    }
    sprintf(command, "%s%s", base64_buf, CRLF);

    event_start(ptcx, &stats->login);
    rc = doSmtpCommandResponse(ptcx, sock, command, respBuffer, sizeof(respBuffer));
    event_stop(ptcx, &stats->login);
    if (rc == -1) {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->login.errs++;
	    strcat (ptcx->errMsg, "<SmtpLogin: failure sending auth message");
	    returnerr(debugfile,"%s [%.199s]\n", ptcx->errMsg, mailUser); /* ??? */
	}
	return -1;
    }

    /* should look for 2xx code for ok, 4xx is ldap error, 5xx means invalid login */

    return 0;
}

int
smtpAuthLogin(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer, SOCKET sock)
{
    char	command[MAX_COMMAND_LEN];
    char	respBuffer[MAX_RESPONSE_LEN];
    char	mailUser[MAX_MAILADDR_LEN];
    char	userPasswd[MAX_MAILADDR_LEN];
    unsigned long loginNum;
    unsigned long domainNum;
    char	base64_buf[1024];
    int	rc;
    pish_stats_t	*stats = (pish_stats_t *)ptimer->data;
    pish_command_t	*pish = (pish_command_t *)cmd->data;

    /* generate a random username (with a mailbox on the server) */
    domainNum = rangeNext (&stats->domainRange, stats->lastDomain);
    stats->lastDomain = domainNum;
    loginNum = rangeNext (&stats->loginRange, stats->lastLogin);
    stats->lastLogin = loginNum;

    sprintf(mailUser, pish->loginFormat, loginNum, domainNum);
    sprintf(userPasswd, pish->passwdFormat, loginNum);

    D_PRINTF(debugfile,"mailUser=[%.64s]\n", mailUser);

    sprintf(command, "AUTH LOGIN%s", CRLF);

    event_start(ptcx, &stats->cmd);
    rc = doSmtpCommandResponse(ptcx, sock, command, respBuffer, sizeof(respBuffer));
    event_stop(ptcx, &stats->cmd);
    if (rc == -1) {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->login.errs++;
	    strcat (ptcx->errMsg, "<SmtpLogin: failure sending AUTH LOGIN");
	    trimEndWhite (command);
	    returnerr(debugfile, "%s command=[%.99s] response=[%.99s]\n", /* ??? */
		      ptcx->errMsg, command, respBuffer);
	}
	return -1;
    }

    /* should now have a 3xx continue code */

    if (str_to_base64(mailUser, strlen(mailUser),
		      base64_buf, sizeof(base64_buf)) == -1) {
	stats->login.errs++;
	strcpy (ptcx->errMsg, "SmtpLogin: Internal error encoding user");
	returnerr(debugfile, "%s [%.199s]\n", ptcx->errMsg, mailUser); /* ??? */
	return -1;
    }
    sprintf(command, "%s%s", base64_buf, CRLF);

    event_start(ptcx, &stats->cmd);
    rc = doSmtpCommandResponse(ptcx, sock, command, respBuffer, sizeof(respBuffer));
    event_stop(ptcx, &stats->cmd);
    if (rc == -1) {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->login.errs++;
	    strcat (ptcx->errMsg, "<SmtpLogin: failure sending user");
	    returnerr(debugfile,"%s [%.199s]\n", ptcx->errMsg, mailUser); /* ??? */
	}
	return -1;
    }

    /* should now have a 3xx continue code */

    if (str_to_base64(userPasswd, strlen(userPasswd),
		      base64_buf, sizeof(base64_buf)) == -1) {
	stats->login.errs++;
	strcpy (ptcx->errMsg, "SmtpLogin: Internal error encoding password");
	returnerr(debugfile, "%s [%.199s]\n", ptcx->errMsg, userPasswd); /* ??? */
	return -1;
    }
    sprintf(command, "%s%s", base64_buf, CRLF);

    event_start(ptcx, &stats->login);
    rc = doSmtpCommandResponse(ptcx, sock, command, respBuffer, sizeof(respBuffer));
    event_stop(ptcx, &stats->login);
    if (rc == -1) {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->login.errs++;
	    strcat (ptcx->errMsg, "<SmtpLogin: failure sending password");
	    returnerr(debugfile,"%s user=%.99s pass=%.99s\n", /* ??? */
		      ptcx->errMsg, mailUser, userPasswd);
	}
	return -1;
    }

    /* should look for 2xx code for ok, 4xx is ldap error, 5xx means invalid login */

    return 0;
}

/* Entry point for running tests */
void *
sendSMTPStart(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer)
{
    doSMTP_state_t	*me = (doSMTP_state_t *)mycalloc (sizeof (doSMTP_state_t));
    char	respBuffer[MAX_RESPONSE_LEN];
    char	command[MAXCOMMANDLEN];
    int		numBytes;
    int		rc;
    pish_command_t	*pish = (pish_command_t *)cmd->data;
    pish_stats_t	*stats = (pish_stats_t *)ptimer->data;

    if (!me) return NULL;


    event_start(ptcx, &stats->connect);
    ptcx->sock = connectSocket(ptcx, &pish->hostInfo, "tcp");
    event_stop(ptcx, &stats->connect);
    if (BADSOCKET(ptcx->sock)) {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->connect.errs++;
	    returnerr(debugfile, "%s SMTP Couldn't connect to %s: %s\n",
		      ptcx->errMsg, pish->hostInfo.hostName, neterrstr());
	}
	myfree (me);
	return NULL;
    }

    if (gf_abortive_close) {
	if (set_abortive_close(ptcx->sock) != 0) {
	    returnerr (debugfile, "SMTP: WARNING: Could not set abortive close\n");
	}
    }

    /* READ connect response from server */
    event_start(ptcx, &stats->banner);
    numBytes = readResponse(ptcx, ptcx->sock, respBuffer, sizeof(respBuffer));
    event_stop(ptcx, &stats->banner);
    if (numBytes <= 0) {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->banner.errs++;
	    returnerr(debugfile,"%s SMTP Error reading banner: %s\n",
		      ptcx->errMsg, neterrstr());
	}
	doSMTPExit (ptcx, me);
	return NULL;
    }
    if (isSmtpResponseOK(respBuffer) == 0) {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->banner.errs++;
	    returnerr(debugfile, "%s Got SMTP ERROR response [%.99s]\n",
		      ptcx->errMsg, respBuffer);
	}
	doSMTPExit (ptcx, me);
	return NULL;
    }
    D_PRINTF(debugfile,"read connect response\n");


    if (pish->useEHLO != 0) {
	/* send extended EHLO */
	sprintf(command, "EHLO %s" CRLF, gs_thishostname);
    } else {
	/* send normal HELO */
	sprintf(command, "HELO %s" CRLF, gs_thishostname);
    }
    event_start(ptcx, &stats->cmd);
    rc = doSmtpCommandResponse(ptcx, ptcx->sock, command, respBuffer, sizeof(respBuffer));
    event_stop(ptcx, &stats->cmd);
    if (rc == -1) {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->cmd.errs++;
	    trimEndWhite (command);
	    returnerr(debugfile, "%s SMTP HELO/EHLO [%.99s] ERROR reading response [%.99s]\n",
		      ptcx->errMsg, command, respBuffer);
	}
	doSMTPExit (ptcx, me);
	return NULL;
    }

    if (pish->useAUTHPLAIN) {
	/* look for AUTH PLAIN LOGIN in respBuffer */
	if (strstr(respBuffer, "AUTH PLAIN LOGIN") != NULL) {
	    /* FIX: time get base64 time and multiple round trips */
	    rc = smtpAuthPlain(ptcx, cmd, ptimer, ptcx->sock);
	    if (rc != 0) {
		doSMTPExit (ptcx, me);
		return NULL;
	    }
	}
    } else if (pish->useAUTHLOGIN) {
	/* look for AUTH LOGIN in respBuffer */
	if (strstr(respBuffer, "AUTH=LOGIN") != NULL) {
	    /* FIX: time get base64 time and multiple round trips */
	    rc = smtpAuthLogin(ptcx, cmd, ptimer, ptcx->sock);
	    if (rc != 0) {
		doSMTPExit (ptcx, me);
		return NULL;
	    }
	}
    }

    return me;
}

  /*
   * SEND A MESSAGE
   */
int
sendSMTPLoop(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer, void *mystate)
{
    doSMTP_state_t	*me = (doSMTP_state_t *)mystate;
    char	command[MAXCOMMANDLEN];
    char	respBuffer[MAX_RESPONSE_LEN];
    char	rcptToUser[MAX_MAILADDR_LEN];
    int	rc, jj;
    long		addressNum;
    long		domainNum;
    int		numBytes;
    pish_stats_t	*stats = (pish_stats_t *)ptimer->data;
    pish_command_t	*pish = (pish_command_t *)cmd->data;

    /* send MAIL FROM:<username> */
    if (pish->msgMailFrom != NULL) {
	sprintf(command, "MAIL FROM:%s%s", pish->msgMailFrom, CRLF);
    } else {
	sprintf(command, "MAIL FROM:<%s>%s", pish->smtpMailFrom, CRLF);
    }
    event_start(ptcx, &stats->cmd);
    rc = doSmtpCommandResponse(ptcx, ptcx->sock, command, respBuffer, sizeof(respBuffer));
    event_stop(ptcx, &stats->cmd);
    if (rc == -1) {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->cmd.errs++;
	    trimEndWhite (command);
	    returnerr(debugfile, "%s SMTP FROM [%.99s], ERROR reading [%.99s] response [%.99s]\n",
		      ptcx->errMsg, command, respBuffer);
	}
	doSMTPExit (ptcx, me);
	return -1;
    }

    /* send RCPT TO:<username> for each recipient */
    for (jj = 0; jj < pish->numRecipients; jj++) {
	/* generate a random recipient (but with an account on the server) */

	domainNum = rangeNext (&stats->domainRange, stats->lastDomain);
	stats->lastDomain = domainNum;
	addressNum = rangeNext (&stats->addressRange, stats->lastAddress);
	stats->lastAddress = addressNum;

	sprintf(rcptToUser, pish->addressFormat, addressNum, domainNum);
	D_PRINTF(debugfile,"rcptToUser=%s\n", rcptToUser);
	sprintf(command, "RCPT TO:<%s>%s", rcptToUser, CRLF);
	event_start(ptcx, &stats->cmd);
	rc = doSmtpCommandResponse(ptcx, ptcx->sock, command, respBuffer, sizeof(respBuffer));
	event_stop(ptcx, &stats->cmd);
	if (rc == -1) {
	    if (gf_timeexpired < EXIT_FAST) {
		stats->cmd.errs++;
		trimEndWhite (command);
		returnerr(debugfile, "%s SMTP RCPT [%.99s], ERROR reading [%.99s] response [%.99s]\n",
			  ptcx->errMsg, command, respBuffer);
	    }
	    doSMTPExit (ptcx, me);
	    return -1;
	}
    }

    /* send DATA */
    event_start(ptcx, &stats->cmd);
    sprintf(command, "DATA%s", CRLF);
    event_stop(ptcx, &stats->cmd);
    if (doSmtpCommandResponse(ptcx, ptcx->sock, command, respBuffer, sizeof(respBuffer)) == -1 ||
	respBuffer[0] != '3') {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->cmd.errs++;
	    returnerr(debugfile, "%s SMTP DATA ERROR, response [%.99s]\n",
		      ptcx->errMsg, respBuffer);
	}
	doSMTPExit (ptcx, me);
	return -1;
    }

    D_PRINTF(debugfile, "data response %s\n", respBuffer);

    /* send data as message (we already added the CRLF.CRLF) */
    event_start(ptcx, &stats->msgwrite);
    numBytes = sendMessage(ptcx, ptcx->sock, pish->msgdata);
    if (numBytes == -1) {
	event_stop(ptcx, &stats->msgwrite);
	if (gf_timeexpired < EXIT_FAST) {
	    returnerr(debugfile, "%s SMTP Error sending mail message: %s\n",
		      ptcx->errMsg, neterrstr());
	    stats->msgwrite.errs++;
	}
	doSMTPExit (ptcx, me);
	return -1;
    }

    /* read server response */
    numBytes = readResponse(ptcx, ptcx->sock, respBuffer, sizeof(respBuffer));
    event_stop(ptcx, &stats->msgwrite);
    if (numBytes <= 0) {
	if (gf_timeexpired < EXIT_FAST) {
	    returnerr(debugfile,"%s SMTP Error reading send message response: %s\n",
		      ptcx->errMsg, neterrstr());
	    stats->msgwrite.errs++;
	}
	doSMTPExit (ptcx, me);
	return -1;
    }
    return 0;
}

void
sendSMTPEnd(ptcx_t ptcx, mail_command_t *cmd, cmd_stats_t *ptimer, void *mystate)
{
    doSMTP_state_t	*me = (doSMTP_state_t *)mystate;
    char	command[MAXCOMMANDLEN];
    char	respBuffer[MAX_RESPONSE_LEN];
    int	rc;
    pish_stats_t	*stats = (pish_stats_t *)ptimer->data;

    if (BADSOCKET(ptcx->sock)) return;	/* closed by previous error */

    /* send QUIT */
    sprintf(command, "QUIT%s", CRLF);
    event_start(ptcx, &stats->logout);
    rc = doSmtpCommandResponse(ptcx, ptcx->sock, command, respBuffer, sizeof(respBuffer));
    event_stop(ptcx, &stats->logout);
    if (rc == -1) {
	if (gf_timeexpired < EXIT_FAST) {
	    stats->logout.errs++;
	    returnerr(debugfile, "%s SMTP QUIT ERROR, response [%.99s]\n",
		      ptcx->errMsg, respBuffer);
	}
    }
    doSMTPExit (ptcx, me);
}

void
doSMTPExit  (ptcx_t ptcx, doSMTP_state_t	*me)
{
  if (!BADSOCKET(ptcx->sock))
    NETCLOSE(ptcx->sock);
  ptcx->sock = BADSOCKET_VALUE;

  myfree (me);
}  
