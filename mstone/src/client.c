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
/* 
   client.c handles thread creation
   login, loop, logout
   status report generation
   throttling (not supported yet)
   thread joining
 */

#include "bench.h"

static int	StartedThreads = 0;   /* counter semaphore for children */
static int	FinishedThreads = 0;   /* counter semaphore for children */

#ifdef _WIN32
#define DEBUG_FILE	"mstone-debug"
#define LOG_FILE	"mstone-log"
#else
#define DEBUG_FILE	"mstone-debug"
#define LOG_FILE	"mstone-log"
#endif /* _WIN32 */

/*
  This is a sleep that knows about test end.
  Should also do throttling.
  We dont check for signals because the only signal expected is test end
 */
void
MS_idle(ptcx_t ptcx, int idleSecs)
{
    int secsLeft = 0;

    secsLeft = gt_shutdowntime - time(0L);

    D_PRINTF(debugfile, "secsLeft=%d, idleSecs=%d\n", secsLeft, idleSecs);

    if (secsLeft <= 0) {			/* time is up, start exiting */
	if (gf_timeexpired < EXIT_SOON)
	    gf_timeexpired = EXIT_SOON;
	return;
    }

    if (idleSecs > secsLeft)  idleSecs = secsLeft;
    if (idleSecs <= 0) return;

    MS_sleep(idleSecs);
}

/* used by POP, SMTP, IMAP4 methods */
void
throttle(ptcx_t ptcx, mail_command_t *comm, cmd_stats_t *timer )
{

    int           chokeSecs = 0;
    struct timeval exittime;

    /* check if we need to throttle this puppy */
    if (comm->throttle <= 0)
	return;

    /* we probably have not reached NETCLOSE yet, so we do not
       know what timer->exittime is */
    timeval_stamp(&exittime);

    /* time to sleep = throttle - (exittime - entertime) */ 
    chokeSecs = comm->throttle - ( exittime.tv_sec - ptcx->starttime.tv_sec );


    /* if chokeSecs is negative, don't bother sleeping */
    if (chokeSecs > 0) {
	d_printf(debugfile, "throttle=%d, chokeSecs=%d\n",
		 comm->throttle, chokeSecs);
	MS_idle(ptcx, chokeSecs);
    }

} /* end throttle */

/* 
 * Perform the given command block
 *
 * commNum = the number of the comm (offset in loaded_comm_list[])
 *
 * returns 1
 */
static int 
do_command(ptcx_t ptcx,			/* thread state */
	   int commNum)			/* command block number */

{
    mail_command_t *comm = &g_loaded_comm_list[commNum];
    cmd_stats_t *cmd_stats =  &(ptcx->cmd_stats[commNum]);
    int cnt, cntEnd;			/* loop counters */
    void *state = NULL;

    cntEnd = comm->numLoops+1; /* transfer count +1 */
    D_PRINTF(debugfile, "do_command start t=%lu commNum=%d cntEnd=%d\n",
	     time(0L), commNum, cntEnd);

    /* Start and End are special loop cases to make summations easier */
    for (cnt = 0; cnt <= cntEnd; cnt++) { 

	if (gf_timeexpired >= EXIT_FAST) /* no more calls */
	    break;
	if (gf_timeexpired >= EXIT_SOON) {
	    if (!state) break;		/* no shutdown to do */
	    cnt = cntEnd;		/* go to shutdown */
	}

	D_PRINTF(debugfile, "do_command t=%lu count=%d of %d\n",
		 time(0L), cnt, cntEnd);

	if (0 == cnt) {			/* first time */
	    cmd_stats->totalcommands++; /* track command blocks trys */
	    state = (*(comm->proto->cmdStart)) (ptcx, comm, cmd_stats);
	    if (NULL == state) {
		D_PRINTF(debugfile, "do_command Start returned NULL\n");
		break;
	    }
	    if (comm->idleTime && (gf_timeexpired < EXIT_SOON)) {
		D_PRINTF(debugfile,"do_command delay %d after Setup\n",
			 comm->idleTime);
		event_start(ptcx, &cmd_stats->idle);
		MS_idle(ptcx, comm->idleTime);
		event_stop(ptcx, &cmd_stats->idle);
	    }
	}
	else if ((cntEnd == cnt) && comm->proto->cmdEnd) { /* last */
	    (*(comm->proto->cmdEnd)) (ptcx, comm, cmd_stats, state);
	    break;			/* done with loop */
	}
	else if (comm->proto->cmdLoop) { /* do transfers */
	    int rc;
	    rc = (*(comm->proto->cmdLoop)) (ptcx, comm, cmd_stats, state);
	    if (rc < 0) {
		D_PRINTF(debugfile, "do_command Loop returned error/done\n");
		cnt = cntEnd -1;	/* end loop */
	    }
					/* do loopDelay even if error/done */
	    if (comm->loopDelay && (gf_timeexpired < EXIT_SOON)) {
		D_PRINTF(debugfile,"do_command delay %d in loop\n",
			 comm->loopDelay);
		event_start(ptcx, &cmd_stats->idle);
		MS_idle(ptcx, comm->loopDelay);
		event_stop(ptcx, &cmd_stats->idle);
	    }
	}
    }

    /* do blockTime even if we hit an error connecting */
    if (comm->blockTime && (gf_timeexpired < EXIT_SOON)) {
	D_PRINTF(debugfile,"do_command delay %d after Block\n",
		 comm->blockTime);
	event_start(ptcx, &cmd_stats->idle);
	MS_idle(ptcx, comm->blockTime);
	event_stop(ptcx, &cmd_stats->idle);
    }
    D_PRINTF(debugfile, "do_command end t=%lu commNum=%d cntEdn=%d\n",
	     time(0L), commNum, cntEnd);

    return 1;				/* return connections */
} /* END do_command() */

/*
  Initialize sub process context
*/
int
clientInit(ptcx_t ptcx)
{
    ptcx->dfile = stderr;
 
    if (gn_debug) {
	/* open a debug log file */
	char debug_file_name[255];
	fflush(stderr);
	if (ptcx->threadnum >= 0) {
	    sprintf(debug_file_name, "%s.%d.%d",
		    DEBUG_FILE, ptcx->processnum, ptcx->threadnum);
	} else {
	    sprintf(debug_file_name, "%s.%d",
		    DEBUG_FILE, ptcx->processnum);
	}
	ptcx->dfile = fopen(debug_file_name, "w+");
	if (ptcx->dfile == NULL) {
	    /*returnerr(stderr, "Can't open debug file\n");
	      return -1;*/
	    gn_debug = 0;
	    d_printf (stderr, "Can't open debug file.  Debug mode disabled\n");
	}
	D_PRINTF(debugfile, "Running in debug mode\n");
    }

    if (gn_record_telemetry) {
	/* open a transaction log file. */
	char log_file_name[255];
	sprintf(log_file_name, "%s.%d.%d",
		LOG_FILE, ptcx->processnum, ptcx->threadnum);
	ptcx->logfile = open(log_file_name,
			     O_CREAT | O_TRUNC | O_WRONLY, 0664);
	returnerr(debugfile,"Log file is %s [%d]\n", log_file_name, ptcx->logfile);
    } else {
	ptcx->logfile = -1;
    }

    /* Initialize random number generator */
    SRANDOM(ptcx->random_seed);
    D_PRINTF(debugfile, "Random seed: 0x%08x\n", ptcx->random_seed );

    stats_init(&ptcx->timestat);

    ptcx->timestat.total_num_of_commands = gn_number_of_commands;

    return 0;
}

/* Continuously handle command blocks until time is up
 */
int
clientLoop(ptcx_t ptcx)
{
    for(ptcx->blockCount = 0; gf_timeexpired < EXIT_SOON; ) {
	int comm_index = 0;

	if (gn_number_of_commands > 1) {
	    int ran_number;
	    /* Handle the weighted distribution of commands */
	    /* HAVE FILELIST */

	    D_PRINTF(debugfile, "Total weight %d\n", gn_total_weight);
	    /* random number between 0 and totalweight-1 */
	    ran_number = (RANDOM() % gn_total_weight);
	    D_PRINTF(debugfile, "random %ld\n", ran_number );
		
	    /* loop through pages, find correct one 
	     * while ran_number is positive, decrement it
	     * by the weight of the current page
	     * example: ran_number is 5, pages have weights of 10 and 10
	     *          first iteration comm_index = 0, ran_number = -5
	     *          iteration halted, comm_index = 0
	     */
	    comm_index = -1;
	    while (ran_number >= 0) {
		comm_index++;
		D_PRINTF(debugfile, "Current command index %d: %ld - %d\n",
			 comm_index, ran_number, 
			 g_loaded_comm_list[comm_index].weight
		    );
		ran_number -= g_loaded_comm_list[comm_index].weight;
	    } 

	    if (comm_index >= gn_number_of_commands) { /* shouldnt happen */
		D_PRINTF(debugfile, "Command weight overrun %d %d\n",
			 ran_number, gn_total_weight);
		comm_index--;
	    }
	    /*D_PRINTF(debugfile, "Final page index %d\n", comm_index );*/
	}

	/* run the command */
	ptcx->connectCount += do_command(ptcx, comm_index);
	++ptcx->blockCount;

	if (gf_timeexpired >= EXIT_SOON) break;	/* done, dont throttle */

	/* For the single processes/thread case, this should be exact */
	if ((gn_maxBlockCnt)		/* check for max loops */
	    && (ptcx->blockCount >= gn_maxBlockCnt)) {
	    D_PRINTF (debugfile, "Saw enough loops %d, exiting\n",
		      ptcx->blockCount);
	    beginShutdown ();		/* indicate early exit */
	    break;
	}

	/* throttle code mikeb@netscape.com 
	 * keeps client from going faster than client_throttle
	 * operations per minute 
	 */
	if (gn_client_throttle &&
	    (ptcx->timestat.endtime.tv_sec > ptcx->timestat.starttime.tv_sec)) {
	    timeval_stamp(&(ptcx->timestat.endtime));
	    while ( 60 * ptcx->connectCount / 
		    (ptcx->timestat.endtime.tv_sec - ptcx->timestat.starttime.tv_sec)
		    > gn_client_throttle ) {
		D_PRINTF(debugfile, "%.2f > %d, throttling\n",
			 ( 60 * ptcx->connectCount /
			   (ptcx->timestat.endtime.tv_sec - ptcx->timestat.starttime.tv_sec) ),
			 gn_client_throttle);
		/* sleep a little */
		MS_usleep( 100 );
	    } /* end while too fast */
	}
    } /* END while blockCount */

    return 0;
}

/*
  Thread of execution.
  Also works in un-threaded case.
  Initialize, do some system housekeeping, run test, housekeeping, clean up 
*/
THREAD_RET
clientThread(void *targ)
{
    time_t	currentTime;
    struct tm	*tmptm;
    char	timeStamp[DATESTAMP_LEN];
    int		ret = 0;
    tcx_t	*ptcx = (ptcx_t)targ;
    /*char	buf[256];*/

    if (clientInit(ptcx)) {		/* should never fail */
#ifdef USE_PTHREADS
	if (ptcx->threadnum >= 0)
	    pthread_exit((void *)((1 << 16) | ptcx->threadnum));
	return NULL;
#else
	return;
#endif
    }

    /*sprintf(buf, "NOTICE: client process=%d thread=%d started\n",
      ptcx->processnum, ptcx->threadnum);
      sendOutput(ptcx->ofd, buf);*/

    if (gn_debug) {
	/* write current time to debug file */
	time(&currentTime);
	tmptm = localtime(&currentTime); 
	strftime(timeStamp, DATESTAMP_LEN, "%Y%m%d%H%M%S", tmptm);
	D_PRINTF(debugfile, "Time Stamp: %s\n", timeStamp);
	D_PRINTF(debugfile, "mailstone run dateStamp=%s\n", gs_dateStamp);
    }

#ifdef _WIN32
#define WAIT_INFINITY 100000000
    /* Tell parent we're ready */
    InterlockedIncrement(&StartedThreads);
#else
    ++StartedThreads;			/* thread safe??? */
#endif /* _WIN32 */
    
    timeval_stamp(&(ptcx->timestat.starttime));
    D_PRINTF(debugfile, "entering clientLoop\n");

    ret = clientLoop(ptcx);		/* do the work */

    timeval_stamp(&(ptcx->timestat.endtime));
    D_PRINTF(debugfile, "Test run complete\n" );

    /* write current time to debug file */
    time(&currentTime);
    tmptm = localtime(&currentTime); 
    strftime(timeStamp, DATESTAMP_LEN, "%Y%m%d%H%M%S", tmptm);
    D_PRINTF(debugfile, "Time Stamp: %s\n", timeStamp);

    if (gn_record_telemetry && (ptcx->logfile > 0)) {
	close(ptcx->logfile);
	ptcx->logfile = -1;
    }

    D_PRINTF(debugfile, "client exiting.\n" );

    if (gn_debug && ptcx->dfile) {
	fflush(ptcx->dfile);
	if (ptcx->dfile != stderr) {
	    fclose(ptcx->dfile);
	    ptcx->dfile = stderr;
	}
    }

    /*sprintf(buf, "NOTICE: client process=%d thread=%d ending\n",
      ptcx->processnum, ptcx->threadnum);
      sendOutput(ptcx->ofd, buf);*/

#ifdef _WIN32
    /* tell parent we're done */
    InterlockedIncrement(&FinishedThreads);
#else  /* _WIN32 */
    ++FinishedThreads;			/* thread safe??? */
#ifdef USE_PTHREADS
    if (ptcx->threadnum >= 0)
	pthread_exit((void *)((ret << 16) | ptcx->threadnum));
#endif
    return NULL;
#endif /* _WIN32 */
} /* END clientThread() */

/*
  The FORMAT format is:
  FORMAT: client=<NUMBER> <TYPE>:<NAME>\t<VALUE>

  TYPE is one of: TIMER, PROTOCOL, LINE

  Everything breaks down to attribute=value pairs.
  "attribute=" is literal, the listed value names the field

  Attribute names must be unique with each timer.

  The [] and {} indicate timers and protocols respectively.
  everything else is a literal (including whitespace).
  The protocols and timers will be expanded with simple text
  substitutions.

  The literal text is as high in the description as possible.
  The processing splits out the values based on surrounding text.
*/
/*
   Output the format that the summaries will be in.
   This call the protocol specific formats, then outputs
   the complete line formats
 */
void
clientSummaryFormat(int clientnum, int outfd)
{
    char 	buf[SIZEOF_SUMMARYTEXT], *cp;
    char	extra[96];
    protocol_t	*pp;

    sprintf (extra, "client=%d", clientnum); /* extra stuff on each line */

    /* Define the contents of each protocol */
    for (pp=g_protocols; pp->name != NULL; ++pp) {
	if (!pp->cmdCount) continue;	/* not used */

	(pp->statsInit)(NULL, &(pp->stats), 0, 0); /* init stats (HERE?) */

	(pp->statsFormat)(pp, extra, buf); /* output lines of format info */
	sendOutput(outfd, buf);
    }

    /* Define the periodic update summaries */
    /* This is the most common message, so keep is as short as practical */
    cp = buf;
    sprintf(cp,
	    "<FORMAT %s LINE=SUMMARY-TIME><TS %s>",
	    extra,
	    "client=client t=time blocks=blocks");
    for (; *cp; ++cp);	/* skip to the end of the string */
    for (pp=g_protocols; pp->name != NULL; ++pp) {
	if (!pp->cmdCount) continue;	/* not used */
	sprintf(cp, "\t<%s {%s}/>", pp->name, pp->name);
	for (; *cp; ++cp);		/* skip to the end of the string */
    }
    strcat (cp, "</TS></FORMAT>\n");
    sendOutput(outfd, buf);

    /* BLOCK-STATISTICS format */
    for (pp=g_protocols; pp->name != NULL; ++pp) {
	cp = buf;
	sprintf(cp,
		"<FORMAT %s LINE=BLOCK-STATISTICS-%s><BS-%s %s>",
		extra, pp->name, pp->name,
		"client=client thread=thread t=time blockID=blockID");
	for (; *cp; ++cp);		/* skip to the end of the string */

	sprintf(cp, "\t<%s {%s}/>", pp->name, pp->name);
	for (; *cp; ++cp);		/* skip to the end of the string */
	sprintf (cp, "</BS-%s></FORMAT>\n", pp->name);
	sendOutput(outfd, buf);
    }

    /* Notice Format */
    sprintf(buf,
	    "<FORMAT %s LINE=NOTICE-1>%s</FORMAT>\n",
	    extra,
	    "<NOTICE client=client ... ");
    sendOutput(outfd, buf);

    /* Abort Format */
    sprintf(buf,
	    "<FORMAT %s LINE=NOTICE-2>%s</FORMAT>\n",
	    extra,
	    "<ABORT client=client ... ");
    sendOutput(outfd, buf);
}

/*
   Output the block information for each thread
 */
void
clientBlockSummary(ptcx_t ptcxs, int ntcxs, int clientnum, int outfd)
{
    time_t 	curtime;
    protocol_t	*pp;
    int 	jj, kk;
    char 	buf[SIZEOF_SUMMARYTEXT], *bp;

    D_PRINTF(stderr, "clientSummaryBS starting.\n" );
    curtime = time(0L);
    for (kk = 0; kk < gn_number_of_commands; ++kk) { /* all commands */
	for (jj = 0; jj < ntcxs; ++jj) { /* all threads */
	    if (0 == ptcxs[jj].cmd_stats[kk].totalcommands) /* nothing */
		continue;
	    pp = g_loaded_comm_list[kk].proto;
					/* output proto independent part */
	    bp = buf;
	    sprintf (bp,
		     "<BS-%s client=%d thread=%d t=%lu blockID=%d>",
		     pp->name, clientnum, jj, curtime,
		     g_loaded_comm_list[kk].blockID);
	    for (; *bp; ++bp);	/* find end of buffer */

	    sprintf (bp,
		     "\t<%s blocks=%ld ",
		     pp->name,
		     ptcxs[jj].cmd_stats[kk].totalcommands);
	    for (; *bp; ++bp);	/* find end of buffer */

	    (pp->statsOutput)(pp, &(ptcxs[jj].cmd_stats[kk]), bp);
	    for (; *bp; ++bp);	/* find end of buffer */

	    sprintf (bp, "/></BS-%s>\n", pp->name); /* end it */
	    sendOutput(outfd, buf);
	}
    }
    D_PRINTF(stderr, "clientSummaryBS done.\n");
}

/* Output the periodic activity summary */
void
clientTimeSummary(ptcx_t ptcxs, int ntcxs, int clientnum, int outfd)
{
    time_t 	curtime;
    int 	blockCount = 0, connectCount = 0;
    protocol_t	*pp;
    int 	jj, kk;
    char 	buf[SIZEOF_SUMMARYTEXT];
    char 	rqsttextbuf[SIZEOF_RQSTSTATSTEXT+1];
    static int	oldThreadStarts= 0;
    static int	oldThreadEnds= 0;

    D_PRINTF(stderr, "clientTimeSummary starting.\n" );
    curtime = time(0L);

    for (pp=g_protocols; pp->name != NULL; ++pp) { /* zero protocol stats */
	if (!pp->cmdCount) continue;	/* not used */
	cmdStatsInit (&(pp->stats));	/* clear proto independent part */
	(pp->statsInit)(NULL, &(pp->stats), 0, 0); /* clear proto part */
    }

    /* sum by protocol all commands, all threads */
    for (jj = 0; jj < ntcxs; ++jj) {
	blockCount += ptcxs[jj].blockCount;
	connectCount += ptcxs[jj].connectCount;
	for (kk = 0; kk < gn_number_of_commands; ++kk) {
	    pp = g_loaded_comm_list[kk].proto;
	    (pp->statsUpdate)(pp, &pp->stats, &(ptcxs[jj].cmd_stats[kk]));
	}
    }

					/* output proto independent part */
    sprintf(buf, "<TS client=%d t=%lu blocks=%d>",
	    clientnum, curtime, blockCount);

    for (pp=g_protocols; pp->name != NULL; ++pp) { /* output proto parts */
	if (!pp->cmdCount) continue;	/* not used */
	(pp->statsOutput)(pp, &pp->stats, rqsttextbuf);
	/* The \t seperates sections for report parsing */
	sprintf(&buf[strlen(buf)], "\t<%s blocks=%ld %s/>",
		pp->name, pp->stats.totalcommands,
		rqsttextbuf);
    }
    strcat(buf, "</TS>\n");		/* end it */
    sendOutput(outfd, buf);

					/* do additional status updates */
    if (oldThreadStarts != StartedThreads) {
	sprintf(buf, "<NOTICE client=%d threadStarts=%d/>\n",
		clientnum, StartedThreads - oldThreadStarts);
	sendOutput(outfd, buf);
	oldThreadStarts = StartedThreads;
    }

    if (oldThreadEnds != FinishedThreads) {
	sprintf(buf, "<NOTICE client=%d threadFinishes=%d/>\n",
		clientnum, FinishedThreads - oldThreadEnds);
	sendOutput(outfd, buf);
	oldThreadEnds = FinishedThreads;
    }

    if (gn_maxerrorcnt) {		/* check for max error count */
	int	errors = 0;
	for (pp=g_protocols; pp->name != NULL; ++pp) { /* sum total */
	    if (!pp->cmdCount) continue;	/* not used */
	    errors += pp->stats.combined.errs;
	}

	if (errors > gn_maxerrorcnt) {
	    returnerr (stderr,
		       "<ABORT client=%d errorCount=%ld errorLimit=%ld/>\n",
		       clientnum, errors, gn_maxerrorcnt);
	    beginShutdown ();
	}
    }

    if ((gn_maxBlockCnt)
	&& (blockCount >= gn_maxBlockCnt)) { /* check for max loops */
	returnerr (stderr,
		   "<ABORT client=%d blockCount=%ld blockLimit=%ld/>\n",
		   clientnum, blockCount, gn_maxBlockCnt);
	beginShutdown ();
    }

    D_PRINTF(stderr, "clientTimeSummary done.\n");
}

/*
  Thread that calls clientTimeSummary at the right rate
*/
THREAD_RET
summaryThread(void *targ)
{
    ptcx_t	ptcxs = (ptcx_t)targ; /* thread contexts */

    D_PRINTF(stderr, "summaryThread starting...\n");

    /* client threads running...dump periodic stats */
    while (gn_feedback_secs && (gf_timeexpired < EXIT_FAST)) {
	D_PRINTF(stderr, "client %d: clientTimeSummary\n", ptcxs[0].processnum);
	clientTimeSummary(ptcxs, gn_numthreads, ptcxs[0].processnum, ptcxs[0].ofd);
	D_PRINTF(stderr, "client %d: waiting %d seconds before feedback\n",
		 ptcxs[0].processnum, gn_feedback_secs);
	MS_sleep(gn_feedback_secs);
    }    

    D_PRINTF(stderr, "summaryThread exiting...\n");

#ifdef USE_PTHREADS
    pthread_exit(0);
#endif

#ifndef _WIN32
    return NULL;
#endif
}

/*
  Initialize per thread context
*/
void
initTcx(ptcx_t ptcx, int ofd, int pnum, int tnum)
{
    int	kk;
    ptcx->processnum = pnum;
    ptcx->threadnum = tnum;
    ptcx->random_seed = (tnum << 16) + getpid();
    ptcx->cmd_stats =
	(cmd_stats_t *)mycalloc(sizeof(cmd_stats_t)*gn_number_of_commands);

					/* do PROTO specific init */
    for (kk = 0; kk < gn_number_of_commands; ++kk) {
	(g_loaded_comm_list[kk].proto->statsInit)
	    (&g_loaded_comm_list[kk], &(ptcx->cmd_stats[kk]), pnum, tnum);
    }

    ptcx->ofd = ofd;
    ptcx->sock = BADSOCKET_VALUE;
}

void
destroyTcx(ptcx_t ptcx)
{
    if (ptcx->cmd_stats) {
	free(ptcx->cmd_stats);
	ptcx->cmd_stats = 0;
    }
}

static time_t bailtime;

/* advance directly to shutdown phase.  May be called from signal handlers */
void
beginShutdown (void)
{
    if (gf_timeexpired >= EXIT_SOON) return; /* already shutting down */

    gf_timeexpired = EXIT_SOON;
    gt_shutdowntime = bailtime = time(0); /* advance end time to now */

    /* Changing aborttime probably has no effect (wrong process) */
    gt_aborttime = gt_shutdowntime + gt_stopinterval*2*EXIT_FASTEST;
}

/* This is the guts of each sub process.
   The socket for data output has already be setup.
   init context
   start threads (if possible/needed) with proper rampup delays
   wait for threads to end
 */
int
clientProc(int pnum, SOCKET outfd,
	   unsigned int testtime,
	   unsigned int thread_stagger_usec)
{
    int tnum;
    int ret;
    ptcx_t	ptcxs; /* thread contexts */
    THREAD_ID summary_tid;
    int status;


    bailtime = gt_shutdowntime;

    returnerr(stderr, "Child starting\n"); /* get our pid and time printed */
    D_PRINTF(stderr, "clientProc(%d, %d, %d) starting\n",
	     pnum, testtime, thread_stagger_usec);

    if (testtime <= 0) {		/* never happens, checked in main.c */
	D_PRINTF (stderr, "ABORTING testtime=%d\n", testtime);
	return 0;
    }

    setup_signal_handlers ();
#ifndef _WIN32
    alarm(testtime);			/* promptly notice test end */
#endif

    clientSummaryFormat (pnum, outfd);

#if defined(USE_PTHREADS) || defined(_WIN32)
    if (gn_numthreads > 0) {
	ptcxs = (ptcx_t)mycalloc(sizeof(tcx_t) * gn_numthreads);

	for (tnum = 0; tnum < gn_numthreads; ++tnum) {
	    initTcx(&ptcxs[tnum], outfd, pnum, tnum);
	}

	/*fprintf(stderr, "launching summary\n");*/
	if ((ret=sysdep_thread_create(&summary_tid, summaryThread,
				      (void *)ptcxs)) != 0) {
	    returnerr(stderr, "client %d: summary thread create failed ret=%d errno=%d: %s\n",
		      pnum, ret, errno, strerror(errno));
	    ptcxs[tnum].tid = 0;
	}
	/*fprintf(stderr, "summary should be running...\n");*/

	for (tnum = 0; tnum < gn_numthreads; ++tnum) {
	    if (gf_timeexpired)
		break;

	    /* sleep between each client thread we try to start */
	    if (tnum && thread_stagger_usec) {
		MS_usleep(thread_stagger_usec);
		if (gf_timeexpired)
		    break;
	    }

	    D_PRINTF(stderr, "client %d: thread %d testtime %d\n",
		     pnum, tnum, testtime);

	    if ((ret=sysdep_thread_create(&(ptcxs[tnum].tid), clientThread,
					  (void *)&ptcxs[tnum])) != 0) {
		returnerr(stderr, "client %d: thread %d create() failed ret=%d errno=%d: %s\n",
			  pnum, tnum, ret, errno, strerror(errno));
		ptcxs[tnum].tid = 0;
	    }
	    D_PRINTF(stderr, "client %d: thread %d created with ID %d\n",
		     pnum, tnum, ptcxs[tnum].tid);
	}

	/* threads are going, but wait for them to get through setup */
	while (StartedThreads < gn_numthreads) {
	    int tm = time(0);
	    if (tm > bailtime) {
		++gf_timeexpired;
		bailtime += gt_stopinterval;
	    }
	    if (gf_timeexpired >= EXIT_SOON) /* failsafe if thread count bad */
		break;
	    MS_sleep(2);
	}
	D_PRINTF(stderr, "client %d: started all threads.\n", pnum);


	/* Wait for all threads to exit or overtime */
	while (FinishedThreads < StartedThreads) {
	    int tm = time(0);
	    if (tm > bailtime) {
		++gf_timeexpired;
		bailtime += gt_stopinterval;
#ifndef _WIN32
		if (gf_timeexpired >= EXIT_FAST) {
		    returnerr (stderr, "Client signaling exit, started=%d finished=%d timeexpired=%d\n",
			       StartedThreads, FinishedThreads, gf_timeexpired);
		    kill (0, SIGALRM);	/* wake children */
		}
#endif
	    }
	    if (gf_timeexpired >= EXIT_FASTEST) {
		returnerr (stderr, "Forcing sockets closed.\n");
		/* close all client sockets, to force calls to exit */
		for (tnum = 0; tnum < gn_numthreads; ++tnum) {
		    if (BADSOCKET(ptcxs[tnum].sock)) continue;
		    D_PRINTF (stderr, "Closing sock=%d tnum=%d\n",
			      ptcxs[tnum].sock, tnum);
		    set_abortive_close(ptcxs[tnum].sock);
		    NETCLOSE(ptcxs[tnum].sock);
		}
		returnerr (stderr, "Forced socket close complete.\n");
		break;
	    }
	    MS_sleep(1);
	}


	D_PRINTF (stderr, "Shutdown timeexpired=%d\n", gf_timeexpired);
	if (gf_timeexpired < EXIT_FAST)	{
	    gf_timeexpired = EXIT_FAST; /* signal summary thread to exit */
	    returnerr (stderr, "Clean child shutdown\n");
	} else if (gf_timeexpired >=  EXIT_FASTEST) {
	    returnerr (stderr, "Forced child shutdown\n");
	} else {
	    returnerr (stderr, "Accellerated child shutdown\n");
	}

	D_PRINTF(stderr, "client %d: joining summary thread\n", pnum);
	if ((ret=sysdep_thread_join(summary_tid, &status)) != 0) {
	    returnerr(stderr,
		      "client %d: summary thread join failed ret=%d errno=%d: %s\n",
		      pnum, ret, errno, strerror(errno));
	}

#ifndef _WIN32
	/* do a summary now in case we hang in joins (needed?) */
	D_PRINTF(stderr, "client %d: pre-join clientTimeSummary\n", pnum);
	clientTimeSummary(ptcxs, gn_numthreads, pnum, outfd);

	for (tnum = 0; tnum < gn_numthreads; ++tnum) {
	    D_PRINTF(stderr, "client %d: joining thread %d ID %d\n",
		     pnum, tnum, ptcxs[tnum].tid);
	    if (ptcxs[tnum].tid) {
		sysdep_thread_join(ptcxs[tnum].tid, &status);
		D_PRINTF(stderr, "client %d: thread %d joined ID %d, ret=%d status=%d\n",
			 pnum, tnum, ptcxs[tnum].tid, ret, status);
		ptcxs[tnum].tid = 0;
	    }
	}
#endif /* _WIN32 */
    } else
#endif /* USE_PTHREADS || _WIN32*/
    {				/* thread un-available or 0 */
	gn_numthreads = 1;
	ptcxs = (ptcx_t)mycalloc(sizeof(tcx_t) * gn_numthreads);

	initTcx(&ptcxs[0], outfd, pnum, -1);

	D_PRINTF(stderr, "client %d: testtime: %d\n", pnum);

	/* set initial data point */
	D_PRINTF(stderr, "client %d: initial clientTimeSummary\n", 0);
	clientTimeSummary(ptcxs, gn_numthreads, pnum, outfd);

	clientThread(&ptcxs[0]);
    }

    /* final time summary feedback */
    D_PRINTF(stderr, "client %d: final summaries\n", 0);
    clientTimeSummary(ptcxs, gn_numthreads, pnum, outfd);
    clientBlockSummary(ptcxs, gn_numthreads, pnum, outfd);

#if 0
    for (tnum = 0; tnum < gn_numthreads; ++tnum) { /* extra reporting */
	D_PRINTF(stderr, "client %d: thread %d stats\n", pnum, tnum);
	clientStats(&ptcxs[tnum]);
    }
#endif

    for (tnum = 0; tnum < gn_numthreads; ++tnum) { /* clean up */
	D_PRINTF(stderr, "client %d: thread %d destroyed\n", pnum, tnum);
	destroyTcx(&ptcxs[tnum]);
    }

    D_PRINTF(stderr, "child: %d done\n", pnum);
    return 0;
}
