/*-
 * Copyright (c) 2012 Michael Tuexen
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#if !defined (__Userspace_os_Windows)
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctp_sysctl.h>
#include "netinet/sctp_callout.h"

/* This is the polling time of callqueue in milliseconds
 * 10ms seems to work well. 1ms was giving erratic behavior
 */
#define TIMEOUT_INTERVAL 10

extern int ticks;

void *
user_sctp_timer_iterate(void *arg)
{
	sctp_os_timer_t *c;
	void (*c_func)(void *);
	void *c_arg;
	sctp_os_timer_t *sctp_os_timer_next;
	/*
	 * The MSEC_TO_TICKS conversion depends on hz. The to_ticks in
	 * sctp_os_timer_start also depends on hz. E.g. if hz=1000 then
	 * for multiple INIT the to_ticks is 2000, 4000, 8000, 16000, 32000, 60000
	 * and further to_ticks level off at 60000 i.e. 60 seconds.
	 * If hz=100 then for multiple INIT the to_ticks are 200, 400, 800 and so-on.
	 */
	for (;;) {
#if defined (__Userspace_os_Windows)
		Sleep(TIMEOUT_INTERVAL);
#else
		struct timeval timeout;

		timeout.tv_sec  = 0;
		timeout.tv_usec = 1000 * TIMEOUT_INTERVAL;
		select(0, NULL, NULL, NULL, &timeout);
#endif
		if (SCTP_BASE_VAR(timer_thread_should_exit)) {
			break;
		}
		SCTP_TIMERQ_LOCK();
		/* update our tick count */
		ticks += MSEC_TO_TICKS(TIMEOUT_INTERVAL);
		c = TAILQ_FIRST(&SCTP_BASE_INFO(callqueue));
		while (c) {
			if (c->c_time <= ticks) {
				sctp_os_timer_next = TAILQ_NEXT(c, tqe);
				TAILQ_REMOVE(&SCTP_BASE_INFO(callqueue), c, tqe);
				c_func = c->c_func;
				c_arg = c->c_arg;
				c->c_flags &= ~SCTP_CALLOUT_PENDING;
				SCTP_TIMERQ_UNLOCK();
				c_func(c_arg);
				SCTP_TIMERQ_LOCK();
				c = sctp_os_timer_next;
			} else {
				c = TAILQ_NEXT(c, tqe);
			}
		}
		SCTP_TIMERQ_UNLOCK();
	}
	return (NULL);
}

void
sctp_start_timer(void)
{
	/*
	 * No need to do SCTP_TIMERQ_LOCK_INIT();
	 * here, it is being done in sctp_pcb_init()
	 */
#if defined (__Userspace_os_Windows)
	if ((SCTP_BASE_VAR(timer_thread) = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)user_sctp_timer_iterate, NULL, 0, NULL)) == NULL) {
		SCTP_PRINTF("ERROR; Creating ithread failed\n");
	}
#else
	int rc;

	rc = pthread_create(&SCTP_BASE_VAR(timer_thread), NULL, user_sctp_timer_iterate, NULL);
	if (rc) {
		SCTP_PRINTF("ERROR; return code from pthread_create() is %d\n", rc);
	}
#endif
}
