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
#include <glob.h>			/* for glob, globfree */

#define MAXCOMMANDLEN	256		/* parse buffer length */

/* SMTP protocol line to send after a message */
#define MSG_TRAILER CRLF "." CRLF

/* run time state */
typedef struct _doSMTP_state {
    int	nothing;
} doSMTP_state_t;

/* info on files/messages to send */
typedef struct {
    char *	filename;		/* file name */
    int		offset;			/* where to start sending from */
    char *	msgMailFrom; /* message mail from (envelope sender) */
} smtp_file_t;

/* flags definitions */
#define useEHLO 0x01			/* use EHLO instead of HELO */
#define useAUTHLOGIN 0x02	  /* use AUTH LOGIN to authenticate */
#define useAUTHPLAIN 0x04	  /* use AUTH PLAIN to authenticate */

static void doSMTPExit  (ptcx_t ptcx, doSMTP_state_t	*me);

/* ================ Support routines ================ */
/*
  Handle the initial look at the specified file
*/
static int
SmtpFileInit (pmail_command_t cmd,	/* command being checked */
	      param_list_t *defparm,	/* default parameters */
	      smtp_file_t	*fileEntry) /* file entry to fill in */
{
    /*pish_command_t	*pish = (pish_command_t *)cmd->data;*/
    int fd;
    int bytesRead;
    struct stat statbuf;
    char *msgdata;			/* temp buffer */
    int msgsize;	 /* message size without trailing CRLF.CRLF */


    /* Handle multiple files.  This means that the file contents
       (starting from an offset) are read when the block is executed.
       The start of the file is scanned here for special commands. */

    D_PRINTF (stderr, "SmtpFileInit: %s\n", fileEntry->filename);

    /* read the contents of file into struct */
    memset(&statbuf, 0, sizeof(statbuf));
    if (stat(fileEntry->filename, &statbuf) != 0) {
	return returnerr(stderr,"Couldn't stat file %s: errno=%d: %s\n", 
			 fileEntry->filename, errno, strerror(errno));
    }

    /* open file */
    if ((fd = open(fileEntry->filename, O_RDONLY)) <= 0) {
	return returnerr(stderr, "Cannot open file %s: errno=%d: %s\n",
			 fileEntry->filename, errno, strerror(errno));
    }

    /* We don't really need much of the file, but it warms up the
     * cache.  Limit at 1Mb */
    msgsize = MIN (statbuf.st_size, 1024*1024);
    msgdata = (char *) mycalloc(msgsize+strlen(MSG_TRAILER)+1);

    if ((bytesRead = read(fd, msgdata, msgsize)) <= 0) {
	close(fd);
	return returnerr(stderr, "Cannot read file %s: errno=%d: %s\n", 
			 fileEntry->filename, errno, strerror(errno));
    }

    if (bytesRead != msgsize) {
	returnerr(stderr, "Error reading file %s, got %d expected %d\n",
		  fileEntry->filename, bytesRead, msgsize);
	close(fd);
	return -1;
    }

    msgdata[msgsize] = 0;

    /* If the file starts with SMTP, then it has extra info in it */
    /* clean up message to rfc822 style */

#define PROTO_HEADER "SMTP"
#define PROTO_MAIL_FROM "\nMAIL FROM:"
#define PROTO_DATA "\nDATA\n"

    if (strncmp(msgdata, PROTO_HEADER, strlen(PROTO_HEADER)) == 0) {
	char *cp, *cp2;
	int off;

	D_PRINTF(stderr, "found PROTO_HEADER  [%s]\n", PROTO_HEADER);
	cp = strstr(msgdata, PROTO_MAIL_FROM); 
	if (cp != NULL) {		/* copy out FROM field */
	    cp += strlen(PROTO_MAIL_FROM);
	    cp2 = strchr(cp, '\n');
	    off = cp2 - cp;
	    fileEntry->msgMailFrom = (char *) mycalloc(off+1);
	    memcpy(fileEntry->msgMailFrom, cp, off);
	    fileEntry->msgMailFrom[off] = 0;
	    D_PRINTF(stderr, "got PROTO_MAIL_FROM:%s\n",
		     fileEntry->msgMailFrom);
	}

	cp = strstr(msgdata, PROTO_DATA);
	if (cp != NULL) { /* DATA marks the real message, copy up */
	    off = cp - msgdata;
	    off += strlen(PROTO_DATA);
	    fileEntry->offset = off;
	    D_PRINTF(stderr, "found PROTO_DATA at off=%d\n", off);
	} else {
	    D_PRINTF(stderr,
		     "WARNING: PROTO_HEADER [%s] given, but PROTO_DATA [%s] never found\n",
		     PROTO_HEADER, PROTO_DATA);
	}
    }


    myfree (msgdata);
    close(fd);
    return 0;
}

/*
  Handle file expansion for the given file pattern
*/
static int
SmtpFilePrep (pmail_command_t cmd,	/* command being checked */
	      param_list_t *defparm)	/* default parameters */
{
    pish_command_t	*pish = (pish_command_t *)cmd->data;
    int		ii;
    glob_t	globs;
    smtp_file_t	*fileEntry;

    /* We randomly pick files, so dont bother to sort them */
    if (glob (pish->filePattern, GLOB_NOSORT | GLOB_MARK, NULL, &globs) < 0) {
	globfree (&globs);
	return returnerr(stderr,"Error globbing '%s': errno=%d: %s\n", 
			 pish->filePattern, errno, strerror(errno));
    }

    pish->fileCount = globs.gl_pathc;
    D_PRINTF (stderr, "SmtpFilePrep: filePattern='%s' entries=%d\n",
	      pish->filePattern, pish->fileCount);
    pish->files = mycalloc (pish->fileCount * sizeof (smtp_file_t));
    fileEntry = pish->files;
    for (ii = 0; ii < pish->fileCount; ++ii, ++fileEntry) {
	fileEntry->filename = mystrdup (globs.gl_pathv[ii]);
	if (SmtpFileInit (cmd, defparm, fileEntry) < 0) {
	    globfree (&globs);
	    return -1;
	}
    }

    globfree (&globs);
    return 0;
}

/*
  Send a file with leading and trailing text.
  Currently only used by SMTP.
  The leading and trailing text may be NULL.
  For SMTP, put the <CRLF>.<CRLF> in the trailing text.
 */
static int
sendFile (
	ptcx_t ptcx,			/* timer context */
	SOCKET sock,			/* socket to send on */
	char *leading,			/* initial text/headers or NULL */
	char *fileName,			/* filename */
	int  offset,			/* offset into file */
	char *trailing)			/* trailing text or NULL */
{
    int leadLen = 0, trailLen = 0;
    int sentbytes = 0;
    int sent, todo, done, fd;
    char buff[16*1024];

    fd = open (fileName, O_RDONLY);
    if (fd < 0) {
	snprintf (buff, sizeof (buff),
		  "<sendfile: Error opening '%s' (%s)",
		  fileName, strerror (errno));
	strcat (ptcx->errMsg, buff);
	return -1;
    }


    if (NULL != leading) leadLen = strlen(leading);
    if (NULL != trailing) trailLen = strlen(trailing);

    D_PRINTF(debugfile, "Writing file '%s' to server: leadLen=%d trailLen=%d offset=%d\n",
	     fileName, leadLen, trailLen, offset);

    /* Send leading text */
    todo = leadLen;
    done = 0;
    while (todo > 0) {
	if ((sent = retryWrite(ptcx, sock, leading + done,
			       todo - done)) == -1) {
	    strcat (ptcx->errMsg, "<sendFile");
            return -1;
        }

	done += sent;
	todo -= sent;

	if (gf_timeexpired >= EXIT_FAST) {
	    D_PRINTF(debugfile,"sendFile() Time expired.\n");
	    sentbytes += done;
	    goto exitFast;
	}
    }
    sentbytes += done;

    /* Send the file */
    if (offset > 0) {			/* skip an initial offset */
	if (lseek (fd, offset, SEEK_SET) < 0) {
	    strcat (ptcx->errMsg, "<sendFile() seek failed.\n");
	    return -1;
	}
    }
    while ((todo = read (fd, buff, sizeof (buff))) > 0) {
	done = 0;
	while (todo > 0) {
	    if ((sent = retryWrite(ptcx, sock, buff + done,
				   todo - done)) == -1) {
		strcat (ptcx->errMsg, "<sendFile");
		return -1;
	    }

	    done += sent;
	    todo -= sent;

	    if (gf_timeexpired >= EXIT_FAST) {
		D_PRINTF(debugfile,"sendFile() Time expired.\n");
		sentbytes += done;
		goto exitFast;
	    }
	}
	sentbytes += done;
    }
    if (todo < 0) {
	D_PRINTF(debugfile,"sendFile() File read.\n");
    }
    close (fd);
    
    /* Send trailing text */
    todo = trailLen;
    done = 0;
    while (todo > 0) {
	if ((sent = retryWrite(ptcx, sock, trailing + done,
			       todo - done)) == -1) {
	    strcat (ptcx->errMsg, "<sendFile");
            return -1;
        }

	done += sent;
	todo -= sent;

	if (gf_timeexpired >= EXIT_FAST) {
	    D_PRINTF(debugfile,"sendFile() Time expired.\n");
	    sentbytes += done;
	    goto exitFast;
	}
    }
    sentbytes += done;

exitFast:
    ptcx->byteswritten += sentbytes;
    return sentbytes;
}

#if 0					/* not currently used */
/*
  Send a message from memory.  Currently only used by SMTP.
  Assumes we already included the <CRLF>.<CRLF> at the end of message
*/
static int
sendMessage(ptcx_t ptcx, SOCKET sock, char *message)
{
    int writelen;
    int sentbytes = 0;
    int sent;

    writelen = strlen(message);

    D_PRINTF(debugfile, "Writing message to server: len=%d\n", writelen );

    while (sentbytes < writelen) {
	if ((sent = retryWrite(ptcx, sock, message + sentbytes,
			       writelen - sentbytes)) == -1) {
	    strcat (ptcx->errMsg, "<sendMessage");
            return -1;
        }

	sentbytes += sent;

	if (gf_timeexpired >= EXIT_FAST) {
	    D_PRINTF(debugfile,"sendMessage() Time expired.\n");
	    break;
	}
    }

    ptcx->byteswritten += sentbytes;

    return sentbytes;
}
#endif

/* ================ SMTP protocol entry points ================ */
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
    if (!pish->filePattern) {
	D_PRINTF(stderr,"missing file for SMTP command");
	return returnerr(stderr,"missing file for SMTP command\n");
    }

    SmtpFilePrep (cmd, defparm);

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
	pish->filePattern = mystrdup (tok);
    else if (strcmp(name, "passwdformat") == 0)
	pish->passwdFormat = mystrdup (tok);
    else if (strcmp(name, "searchfolder") == 0)
	pish->imapSearchFolder = mystrdup (tok);
    else if (strcmp(name, "searchpattern") == 0)
	pish->imapSearchPattern = mystrdup (tok);
    else if (strcmp(name, "searchrate") == 0)
	pish->imapSearchRate = atoi(tok);
    else if (strcmp(name, "useehlo") == 0)
	if (atoi(tok)) {
	    pish->flags |= useEHLO;
	} else {
	    pish->flags &= ~useEHLO;
	}
    else if (strcmp(name, "useauthlogin") == 0)
	if (atoi(tok) > 0) {
	    pish->flags |= useAUTHLOGIN;
	} else {
	    pish->flags &= ~useAUTHLOGIN;
	}
    else if (strcmp(name, "useauthplain") == 0)
	if (atoi(tok) > 0) {
	    pish->flags |= useAUTHPLAIN;
	} else {
	    pish->flags &= ~useAUTHPLAIN;
	}
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


    if (pish->flags & useEHLO) {
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

    if (pish->flags & useAUTHPLAIN) {
	/* look for AUTH PLAIN LOGIN in respBuffer */
	if (strstr(respBuffer, "AUTH PLAIN LOGIN") != NULL) {
	    /* FIX: time get base64 time and multiple round trips */
	    rc = smtpAuthPlain(ptcx, cmd, ptimer, ptcx->sock);
	    if (rc != 0) {
		doSMTPExit (ptcx, me);
		return NULL;
	    }
	}
    } else if (pish->flags & useAUTHLOGIN) {
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
    smtp_file_t	*fileEntry;

    /* send MAIL FROM:<username> */
    if (pish->fileCount > 1) {		/* randomly pick file */
	int	ff;
	ff = (RANDOM() % pish->fileCount);
	/*D_PRINTF(debugfile,"random file<%d=%d\n", pish->fileCount, ff);*/
	fileEntry = ((smtp_file_t *)pish->files) + ff;
    } else {
	fileEntry = (smtp_file_t *)pish->files;
    }
    if (fileEntry->msgMailFrom != NULL) {
	sprintf(command, "MAIL FROM:%s%s", fileEntry->msgMailFrom, CRLF);
    } else {
	sprintf(command, "MAIL FROM:<%s>%s", pish->smtpMailFrom, CRLF);
    }
    /*D_PRINTF(debugfile,"%s\n", command);*/
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

    /* send message */
    event_start(ptcx, &stats->msgwrite);
    numBytes = sendFile(ptcx, ptcx->sock,
			NULL,
			fileEntry->filename, fileEntry->offset,
			MSG_TRAILER);
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
