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
 #			Marcel DePaolis <marcel@netcape.com>
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

/* these are protocol dependent timers for Pop, Imap, Smtp, Http*/
typedef struct pish_stats {
    event_timer_t	connect;
    event_timer_t	banner;
    event_timer_t	login;
    event_timer_t	cmd;
    event_timer_t	msgread;
    event_timer_t	msgwrite;
    event_timer_t	logout;

    /* protocol dependent local storage */
					/* should have local ranges too */
    range_t		loginRange;	/* login range for this thread */
    unsigned long	lastLogin;
    range_t		domainRange;	/* domain range for this thread */
    unsigned long	lastDomain;
    range_t		addressRange;	/* address range for this thread */
    unsigned long	lastAddress;
} pish_stats_t;

/* These are common to POP, IMAP, SMTP, HTTP */
typedef struct pish_command {
    resolved_addr_t 	hostInfo;	/* should be a read only cache */

    /* These are common to SMTP, POP, IMAP */
    char *	loginFormat;
    range_t	loginRange;		/* login range for all threads */

    range_t	domainRange;		/* domain range for all threads */

    char *	passwdFormat;

    long	flags;			/* protocol specific flags */

    /* SMTP command attrs */
    char *	addressFormat;
    range_t	addressRange;		/* address range for all threads */

    int 	numRecipients;	/* recpients per message */
    char *	smtpMailFrom;		/* default from address */
    char *	filePattern;		/* filename pattern */
    int		fileCount;		/* number of files */
    void *	files;			/* array of file info */

    /* IMAP command attrs */
    char *	imapSearchFolder;
    char *	imapSearchPattern;
    int 	imapSearchRate;
} pish_command_t;

					/* TRANSITION functions */
extern int pishParseNameValue (pmail_command_t cmd, char *name, char *tok);
