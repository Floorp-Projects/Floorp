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
/*
  bench.c has all the OS independent utilities.
*/
#include "bench.h"

/* allocate memory and exit if out of memory */
void *
mymalloc(size_t size)
{
    void *ptr;

    ptr = malloc(size);
    if (ptr == NULL)
	errexit(stderr, "Call to malloc(%d) failed\n", size);
    return(ptr);
}

void *
mycalloc(size_t size)
{
    void *retp;

    if ((retp = mymalloc(size)) == NULL)
	return NULL;
    memset(retp, 0, size);
    return(retp);
}

/* allocate memory and exit if out of memory */
void *
myrealloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    if (ptr == NULL)
	errexit(stderr, "Call to realloc(%d, %d) failed\n", ptr, size);
    return(ptr);
}

void
myfree(void *ptr)
{
    free(ptr);
}

char *
mystrdup(const char *cp)
{
    char *retcp;

    if ((retcp = (char *)mymalloc(strlen(cp)+1)) == NULL)
	return NULL;
    strcpy(retcp, cp);

    return retcp;
}

/*
 * waitReadWrite(int fd, int flags)
 * parameter: fd: file descriptor
 *		read_write:     0 --> read
 *			        1 --> write
 *				2 --> read & write
 * return:	NON-Zero	something is read
 * 		0		sth is wrong
 */

#define	CHECK_READ 	0x0
#define	CHECK_WRITE 	0x1
#define	CHECK_RW 	0x2
#define	CHECK_ALL	0x3
#define CHECK_FOREVER	0x4

#define waitReadable(fd)	waitReadWrite((fd),CHECK_READ)
#define waitWriteable(fd)	waitReadWrite((fd),CHECK_WRITE)
#define waitWriteableForever(fd) waitReadWrite((fd),CHECK_WRITE | CHECK_FOREVER)

/* Return 1 if bytes readable; 0 if error or time up */
int
waitReadWrite(int fd, int flags)
{
#ifdef _WIN32
    return 0;
#else
    struct pollfd pfd;
    int timeleft;
    int	ret;
    int	timeouts = 0;

    pfd.fd = fd;
    pfd.events = POLLHUP;

    if ((flags & CHECK_ALL) == CHECK_READ) {
	pfd.events |= POLLIN;
    } else if ((flags & CHECK_ALL) == CHECK_WRITE) {
	pfd.events |= POLLOUT;
    } else if ((flags & CHECK_ALL) == CHECK_RW) {
	pfd.events |= (POLLIN | POLLOUT);
    }
    for (;;) {
	if (flags & CHECK_FOREVER) {	/* for writing status out */
	    timeleft = 60;
	} else {
	    if (gf_timeexpired >= EXIT_FAST) {
		D_PRINTF(stderr, "waitReadWrite gf_timeexpired=%d\n",
			 gf_timeexpired);
		return 0;
	    }
	    timeleft = 5;
	}

	/*fprintf(stderr, "poll(%d,%d)\n", fd, timeleft*1000);*/
	ret = poll(&pfd, 1, timeleft*1000);
	/*fprintf(stderr, "poll(%d,%d)=%d\n", fd, timeleft*1000, ret);*/

	if (ret == 0) {
	    if (!(flags & CHECK_FOREVER) && (++timeouts >= 12)) {
		return 0;		/* time out after 60sec total */
	    }
	    continue;
	}

	if (ret < 0) {
	    if (errno == EAGAIN || errno == EINTR)
		continue;
	    D_PRINTF(stderr, "waitReadWrite error ret=%d\n", ret);
	    break;
	}

	return 1;
    }

    /* error */
    return 0;
#endif
}

int
retryRead(ptcx_t ptcx, SOCKET sock, char *buf, int count)
{
    int ret;
    int bytesread = 0;

    D_PRINTF(debugfile, "retryRead(%d, %d (gf_timeexpired=%d))\n",
	     sock, count, gf_timeexpired);
    while (count) {
	if (gf_timeexpired >= EXIT_FAST) {
	    D_PRINTF (debugfile, "retryRead gf_timeexpired\n");
	    strcpy (ptcx->errMsg, "retryRead:TIMEOUT");
	    break;
	}
	if (0 == waitReadable (sock)) {
	    D_PRINTF (debugfile, "retryRead waitReadable time/error\n");
	    strcpy (ptcx->errMsg, "waitReadable TIMEOUT<retryRead");
	    break;
	}
	ret = NETREAD(sock, buf, count);
	if (ret < 0) {
	    if (errno == EINTR)
		continue;
	    if (errno == EAGAIN) {
		if (bytesread > 0)
		    break; /* return what we got so far */
		if (waitReadable(sock)) {
		    continue;
		}
	    }
	    if (bytesread == 0) {
		bytesread = -1;
	    }
	    sprintf (ptcx->errMsg, "retryRead(sock=%d) IO error", sock);
	    break;
	}		    
	bytesread += ret;
	buf += ret;
	count -= ret;
 	break; /* return any good bytes */
    }
    D_PRINTF(debugfile, "retryRead(%d, %d)=%d\n", sock, count, bytesread);
    return bytesread;
}

int
retryWrite(ptcx_t ptcx, SOCKET sock, char *buf, int count)
{
    int ret;
    int byteswritten = 0;

    D_PRINTF(debugfile, "retryWrite(%d, %d)...\n", sock, count);
    while (count) {
	if (gf_timeexpired >= EXIT_FAST) {
	    D_PRINTF (debugfile, "retryWrite gf_timeexpired\n");
	    strcpy (ptcx->errMsg, "read:timeout");
	    break;
	}
	ret = NETWRITE(sock, buf, count);
	if (ret < 0) {
	    if (errno == EINTR)
		continue;
	    if (errno == EAGAIN) {
		if (waitWriteable(sock)) {
		    continue;
		}
	    }
	    if (byteswritten == 0) {
		byteswritten = -1;
	    }
	    sprintf (ptcx->errMsg,
		     "retryWrite(sock=%d, n=%d) IO error", sock, count);
	    break;
	}		    
	byteswritten += ret;
	buf += ret;
	count -= ret;
    }
    D_PRINTF(debugfile, "retryWrite(%d, ?)=%d\n", sock, byteswritten);
    return byteswritten;
}


int
readResponse(ptcx_t ptcx, SOCKET sock, char *buffer, int buflen)
{    
    /* read the server response and do nothing with it */
    int totalbytesread = 0;
    int resplen = 0;
    int bytesread;
    char *offset;

    memset (buffer, 0, sizeof(buffer));
    while (totalbytesread < buflen)
    {
	if (gf_timeexpired >= EXIT_FAST) {
	    D_PRINTF(debugfile,"readResponse() Time expired.\n");
	    break;
	}
	if ((bytesread = retryRead(ptcx, sock, buffer+totalbytesread,
				   buflen-totalbytesread)) <= 0) {
	    strcat (ptcx->errMsg, "<ReadResponse");
	    return -1;
	}
        totalbytesread += bytesread;

	buffer[totalbytesread] = 0;
	/* search for end of response (assume single line) */
	if ((offset = strstr(buffer, "\n"))) {
	    resplen = offset - buffer + 1;
	    break;
	} else if ((offset = strstr(buffer, "\r\n"))) {
	    resplen = offset - buffer + 2;
	    break;
	}
    }
    D_PRINTF(debugfile, "Read from server: %s\n", buffer );

    ptcx->bytesread += resplen;

    return resplen;
}

/* expects pointers to buffers of these sizes */
/* char command[MAX_COMMAND_LEN] */
/* char response[MAX_RESPONSE_LEN] */

int
doCommandResponse(ptcx_t ptcx, SOCKET sock, char *command, char *response, int resplen)
{
    int ret;

    if (response == NULL)
	return -1;
    memset(response, 0, resplen);

    /* send the command already formatted */
    if ((ret = sendCommand(ptcx, sock, command)) == -1) {
	strcat (ptcx->errMsg, "<doCommandResponse");
	return -1;
    }

    /* read server response line */
    if ((ret = readResponse(ptcx, sock, response, resplen)) <= 0) {
	strcat (ptcx->errMsg, "<doCommandResponse");
	return -1;
    }
    return ret;
}

int
sendCommand(ptcx_t ptcx, SOCKET sock, char *command)
{
    int writelen;
    int sentbytes = 0;
    int sent;

    D_PRINTF(debugfile, "sendCommand(%s)\n", command );

    writelen = strlen(command);

    while (sentbytes < writelen) {
	if ((sent = retryWrite(ptcx, sock, command + sentbytes,
			       writelen - sentbytes)) == -1) {
	    strcat (ptcx->errMsg, "<sendCommand");
            return -1;
        }

	sentbytes += sent;

	if (gf_timeexpired >= EXIT_FAST) {
	    D_PRINTF(debugfile,"sendCommand() Time expired.\n");
	    break;
	}
    }

    ptcx->byteswritten += sentbytes;

    return sentbytes;
}

/* This is how status messages are sent */
int
sendOutput(int fd, char *string)
{
    int writelen;
    int sentbytes = 0;
    int sent;

    /*D_PRINTF(stderr, "sendOutput(%d, %s)\n", fd, string );*/

    writelen = strlen(string);

    while (sentbytes < writelen) {
    	sent = OUTPUT_WRITE(fd, string + sentbytes, writelen - sentbytes);

	if (sent == -1) {
	    if (errno == EINTR)
		continue;
            if (errno == EAGAIN) {
		if (waitWriteableForever(fd))
		    continue;
		else
		    returnerr(stderr,
			      "sendOutput(%d) - Got EAGAIN and fd not ready\n",
			      fd);	/* has this ever happened? die??? */
            }
            return -1;
        }

	sentbytes += sent;

#if 0
	if (gf_timeexpired >= EXIT_FAST) {
	    D_PRINTF(stderr,"sendOutput() Time expired.\n");
	    break;
	}
#endif
    }

    return sentbytes;
}

/* read from socket until we find <CRLF>.<CRLF> */
int
retrMsg(ptcx_t ptcx, char *buffer, int maxBytes, SOCKET sock)
{    
    int totalbytesread = 0;
    int bytesread;
    int sz;
    char garbage[10000], *sp;

    if (buffer) {
	sp = buffer;
	sz = maxBytes-1;
    } else {
	sp = garbage;
	sz = sizeof (garbage)-1;
    }

    while (!buffer || (totalbytesread < maxBytes)) {
	if (gf_timeexpired >= EXIT_FAST) {
	    D_PRINTF(debugfile,"Time expired while reading messages - in retrMsg\n");
	    break;
	}
	
	bytesread = retryRead(ptcx, sock,
			      sp+totalbytesread, sz-totalbytesread);
	
	/*D_PRINTF (stderr, "retrMsg: got %d bytes\n", bytesread);*/
        if (bytesread <= 0) {
	    strcat (ptcx->errMsg, "<retrMsg");
	    return -1;
	}
	
	ptcx->bytesread += bytesread;
	totalbytesread += bytesread;

	sp[totalbytesread] = 0;		/* terminate string */
	if (NULL != strstr (sp, "\r\n.\r\n")) {
	    D_PRINTF (stderr, "retrMsg: saw terminating string\n");
	    break;
	}
	
	if (!strncmp (sp, "-ERR", 4)) { /* watch for error response */
	    trimEndWhite (sp);
	    sprintf (ptcx->errMsg, "retrMsg: ERROR response=[%.40s]", sp);
	    return -1;
	}

	if (!buffer && (totalbytesread > 5)) { /* reset our scratch buffer */
	    int i;
	    char *from, *to;
					/* shuffle last 5 bytes to start */
	    from = sp + totalbytesread - 5;
	    to = garbage;
	    for (i=5; i > 0; --i)
		*to++ = *from++;
	    totalbytesread = 5;
	}
    }


    return totalbytesread;
}

/*
  Record current time
*/
int
timeval_stamp(struct timeval *tv)
{
    int rc;

    rc = GETTIMEOFDAY(tv, NULL);
    if (rc != 0) {
	errexit(stderr, "Error from gettimeofday()\n");
    }
    return rc;
}

/*
  This is the main event timing routine.  This resets counters
  Note that event timing cannot be nested.
 */
void
event_start(ptcx_t ptcx, event_timer_t *pevent)
{
    ptcx->bytesread = 0;
    ptcx->byteswritten = 0;
    timeval_stamp(&ptcx->starttime);
}

/*
  This ends an event and increments the counters
  Multiple stops are no longer allowed.  (broke min, max, and sum of t*t)
*/
void
event_stop(ptcx_t ptcx, event_timer_t *pevent)
{
    struct timeval tv;
    double t;

    timeval_stamp(&tv);			/* get the time */

    if (gf_timeexpired >= EXIT_FAST) {	/* if aborting run, ignore it */
	return;
    }
    pevent->trys++;			/* count try with time */
    pevent->bytesread += ptcx->bytesread;
    pevent->byteswritten += ptcx->byteswritten;
    t = compdifftime_double(&tv, &ptcx->starttime);	/* find time span */
    pevent->elapsedtime += t;
    pevent->elapsedtimesq += t * t;

    if (t > pevent->maxtime)	/* check max time */
	pevent->maxtime = t;

    /* this gets initialized to 0.0 */
    if (!pevent->mintime ||		/* check min time */
	((t > 0.0) && (t < pevent->mintime))) /* smallest non 0 time */
	pevent->mintime = t;
}

/*
  reset the event structure
*/
void
event_reset(event_timer_t *pevent)
{
    memset(pevent, 0, sizeof(event_timer_t));
}

/*
  Add pincr event into psum event
*/
void
event_sum(event_timer_t *psum, event_timer_t *pincr)
{
    psum->trys += pincr->trys;
    psum->errs += pincr->errs;
    psum->bytesread += pincr->bytesread;
    psum->byteswritten += pincr->byteswritten;
    psum->elapsedtime += pincr->elapsedtime;
    psum->elapsedtimesq += pincr->elapsedtimesq;
    if (pincr->maxtime > psum->maxtime)
	psum->maxtime = pincr->maxtime;
    if (!psum->mintime ||
	((pincr->mintime > 0.0) && (pincr->mintime < psum->mintime)))
	psum->mintime = pincr->mintime;
}

/* Format string for every timer.  Must match event_to_text */
const char *gs_eventToTextFormat = "Try+Error/BytesR+BytesW/Time[TimeMin,TimeMax]Time2";

/*
  Output event into ebuf.
*/
char *
event_to_text(event_timer_t *pevent, char *ebuf)
{
    /* these have to be sane to avoid overflowing the print buffer */
    assert (pevent->bytesread < 1.0e20);
    assert (pevent->byteswritten < 1.0e20);
    assert (pevent->elapsedtime < 1.0e10);
    assert (pevent->maxtime < 1.0e10);
    assert (pevent->elapsedtimesq < 1.0e20);
    if (pevent->elapsedtime) {
	sprintf(ebuf, "%ld+%ld/%.0f+%.0f/%.6f[%.6f,%.6f]%.6f",
		pevent->trys, pevent->errs,
		pevent->bytesread, pevent->byteswritten,
		pevent->elapsedtime,
		pevent->mintime,
		pevent->maxtime,
		pevent->elapsedtimesq);
    } else {				/* trim extra 0s for simple case*/
	sprintf(ebuf, "%ld+%ld/%.0f+%.0f/0.0[0.0,0.0]0.0",
		pevent->trys, pevent->errs,
		pevent->bytesread, pevent->byteswritten);
    }
    return ebuf;
}

/*
  Given the last value we returned, return the next sequential or random value
  If the initial value is out of the range, a valid number will be returned.
*/
unsigned long
rangeNext (range_t *range, unsigned long last)
{
    unsigned long	n;
    
    assert (range != NULL);

    if (range->span == 0)		/* empty range (span = 0) */
	return range->first;

    if (range->sequential > 0) {	/* incrementing sequential */
	n = last + 1;
    } else if (range->sequential < 0) {	/* decrementing sequential */
	n = last - 1;
    } else {				/* random */
	n = range->first + (RANDOM() % (range->span+1));
	assert ((n >= range->first) && (n <= range->last));
    }
					/* check range */
    if (n > range->last) n = range->first;
    if (n < range->first) n = range->last;
    return n;
}

/*
  Setup range given a first and last valid values (inclusive)
  Direction: + incrementing, 0 random, - decrementing
  If first and last are reversed, it will adjust accordingly
*/
void
rangeSetFirstLast (range_t *range,
		   unsigned long first,
		   unsigned long last,
		   int dir)
{
    assert (range != NULL);
    if (last > first) {
	range->first = first;
	range->last = last;
	range->sequential = dir;
    } else {
	range->first = last;
	range->last = first;
	range->sequential = -dir;
    }
    range->span = range->last - range->first;
}

/*
  Setup range given a first valid value (inclusive) and number (1..)
  Direction: + incrementing, 0 random, - decrementing
*/
void
rangeSetFirstCount (range_t *range,
		    unsigned long first,
		    unsigned long num,
		    int dir)
{
    assert (range != NULL);
    if (num > 0) --num;		/* adjust to internal notation */
    range->first = first;
    range->span = num;
    range->last = range->first + range->span;
    range->sequential = dir;
}

/*
  Given a specified range, split it based on process and thread
*/
void
rangeSplit (range_t *whole, range_t *sub, int pnum, int tnum)
{
    unsigned long perproc, first, count;

    if (!whole->sequential) {		/* just copy it */
	sub->first = whole->first;
	sub->last = whole->last;
	sub->span = whole->span;
	sub->sequential = whole->sequential;
	return;
    }
    /* To avoid cumulative rounding errors,
       the 0th process/thread rounds up, all others round down */
    perproc = (pnum > 0)
	? (whole->span+1) / gn_numprocs
	: (whole->span+1 + gn_numprocs - 1) / gn_numprocs;

    if (perproc <= 0) perproc = 1;      /* in case logins > processes */

    if (gn_numthreads > 0) {
	unsigned long perthread;
	perthread = (tnum > 0)
	    ? perproc / gn_numthreads
	    : (perproc + gn_numthreads - 1) / gn_numthreads;

	if (perthread <= 0) perthread = 1;      /* in case logins > threads */

	first = whole->first + (perproc * pnum) + (perthread * tnum);
	count = perthread;
    } else {
	first = whole->first + (perproc * pnum);
	count = perproc;
    }
    /* If logins > processes*threads, we have to wrap the space */
    while (first >= (whole->first + whole->span+1)) {
	first -= whole->span+1;
    }
    assert (first >= whole->first);

    rangeSetFirstCount (sub, first, count, whole->sequential);
}

/*
  clear protocol independ parts of cmd_stats_t
*/
void
cmdStatsInit (cmd_stats_t *p)
{
    assert (NULL != p);

    p->totalerrs = 0;
    p->totalcommands = 0;
    event_reset (&p->combined);
    event_reset (&p->idle);
}

void
stats_init(stats_t *p)
{
    memset(p, 0, sizeof(*p));
}

/*
  Given a buffer, trim tailing CR-NL and whitespace from it
  Moves 0 terminator to exclude extra portion.
 */
void
trimEndWhite (char *buff)
{
    char *cp;

    if (!buff) return;

    for (cp = buff; *cp; ++cp);		/* find the end */
    while (cp > buff) {
	--cp;
	if ((*cp != '\n') && (*cp !='\r')
	    && (*cp != '\t') && (*cp != ' ')) return;
	*cp = 0;
    }
}

#if 0					/* never used */

int
timeval_clear(struct timeval *tv)
{
    if (tv == NULL)
	return -1;

    tv->tv_sec = 0;
    tv->tv_usec = 0;

    return 0;
}

/* timetextbuf should be (SIZEOF_TIMEVALTEXT + 1) */
char *
timeval_to_text(const struct timeval *the_timeval, char *timetextbuf)
{
    /* 
     * given a timeval (seconds and microseconds), put the text
     * "seconds.microseconds" into timeval_as_text
     */
    int seconds, microseconds;

    seconds = the_timeval->tv_sec;
    microseconds = the_timeval->tv_usec;
    sprintf(timetextbuf, "%10d.%6.6d\t", seconds, microseconds);
    return timetextbuf;
}


/* doubletextbuf should be (SIZEOF_DOUBLETEXT+1) */

char *
double_to_text(const double the_double, char *doubletextbuf)
{
    /*
     * given a double, return text
     */
    sprintf(doubletextbuf, "%17.01f\t", the_double);
    return(doubletextbuf);
}

struct timeval
text_to_timeval(ptcx_t ptcx, char *timeval_as_text) { 
    long int seconds, microseconds;
    struct timeval the_timeval;

    D_PRINTF(debugfile,"T/%d %s\n", (int)timeval_as_text, timeval_as_text);
    sscanf(timeval_as_text, "%ld.%ld", &seconds, &microseconds);
    the_timeval.tv_sec = seconds;
    the_timeval.tv_usec = microseconds;
    return the_timeval;
}

double
text_to_double(ptcx_t ptcx, char *double_as_text) {
    double the_double = 0;
    int returnval = 0;

    D_PRINTF(debugfile,"D/%d %s\n", (int)double_as_text, double_as_text);
    returnval = sscanf(double_as_text, "%lf", &the_double);
    if (returnval == 1)
	return(the_double);
    else
	return(0.0);
}   
#endif

#if 0
/* not currently used, but useful for debugging */
void
dumpevent(ptcx_t ptcx, event_timer_t *pevent)
{
    D_PRINTF(debugfile, "trys=%d errs=%d br=%f bw=%f elapsed=%f sq=%f\n",
	     pevent->trys, pevent->errs,
	     pevent->bytesread, pevent->byteswritten,
	     pevent->elapsedtime,
	     pevent->elapsedtimesq);
}

void
dumptimer(ptcx_t ptcx, cmd_stats_t *rqsttimer)
{
    if (gn_debug) {
	D_PRINTF(debugfile, "Connect: ");
	dumpevent(ptcx, &rqsttimer->connect);

	D_PRINTF(debugfile, "Banner: ");
	dumpevent(ptcx, &rqsttimer->banner);

	D_PRINTF(debugfile, "Login: ");
	dumpevent(ptcx, &rqsttimer->login);

	D_PRINTF(debugfile, "Cmd: ");
	dumpevent(ptcx, &rqsttimer->cmd);

	D_PRINTF(debugfile, "MsgRead: ");
	dumpevent(ptcx, &rqsttimer->msgread);

	D_PRINTF(debugfile, "MsgWrite: ");
	dumpevent(ptcx, &rqsttimer->msgwrite);

	D_PRINTF(debugfile, "Idle: ");
	dumpevent(ptcx, &rqsttimer->idle);

	D_PRINTF(debugfile, "Logout: ");
	dumpevent(ptcx, &rqsttimer->logout);
    }
}
#endif
