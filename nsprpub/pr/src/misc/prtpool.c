/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "prtpool.h"
#include "nspr.h"

/*
 * Thread pools
 *	Thread pools create and manage threads to provide support for
 *	scheduling jobs onto one or more threads.
 *
 */
#ifdef OPT_WINNT
#include <windows.h>
#endif

/*
 * worker thread
 */
typedef struct wthread {
	PRCList		links;
	PRThread	*thread;
} wthread;

/*
 * queue of timer jobs
 */
typedef struct timer_jobq {
	PRCList		list;
	PRLock		*lock;
	PRCondVar	*cv;
	PRInt32		cnt;
	PRCList 	wthreads;
} timer_jobq;

/*
 * queue of jobs
 */
typedef struct tp_jobq {
	PRCList		list;
	PRInt32		cnt;
	PRLock		*lock;
	PRCondVar	*cv;
	PRCList 	wthreads;
#ifdef OPT_WINNT
	HANDLE		nt_completion_port;
#endif
} tp_jobq;

/*
 * queue of IO jobs
 */
typedef struct io_jobq {
	PRCList		list;
	PRPollDesc  *pollfds;
	PRInt32  	npollfds;
	PRJob		**polljobs;
	PRLock		*lock;
	PRInt32		cnt;
	PRFileDesc	*notify_fd;
	PRCList 	wthreads;
} io_jobq;

/*
 * Threadpool
 */
struct PRThreadPool {
	PRInt32		init_threads;
	PRInt32		max_threads;
	PRInt32		current_threads;
	PRInt32		idle_threads;
	PRInt32		stacksize;
	tp_jobq		jobq;
	io_jobq		ioq;
	timer_jobq	timerq;
	PRCondVar	*shutdown_cv;
	PRBool		shutdown;
};

typedef enum io_op_type
	{ JOB_IO_READ, JOB_IO_WRITE, JOB_IO_CONNECT, JOB_IO_ACCEPT } io_op_type;

typedef enum _PRJobStatus
	{ JOB_ON_TIMERQ, JOB_ON_IOQ, JOB_QUEUED, JOB_RUNNING, JOB_COMPLETED,
					JOB_CANCELED, JOB_FREED } _PRJobStatus;

typedef void (* Jobfn)(void *arg);

#ifdef OPT_WINNT
typedef struct NT_notifier {
	OVERLAPPED overlapped;		/* must be first */
	PRJob	*jobp;
} NT_notifier;
#endif

struct PRJob {
	PRCList			links;		/* 	for linking jobs */
	_PRJobStatus 	status;
	PRBool			joinable;
	Jobfn			job_func;
	void 			*job_arg;
	PRLock			*jlock;
	PRCondVar		*join_cv;
	PRThreadPool	*tpool;		/* back pointer to thread pool */
	PRJobIoDesc		*iod;
	io_op_type		io_op;
	PRInt16			io_poll_flags;
	PRNetAddr		*netaddr;
	PRIntervalTime	timeout;	/* relative value */
	PRIntervalTime	absolute;
#ifdef OPT_WINNT
	NT_notifier		nt_notifier;	
#endif
};

#define JOB_LINKS_PTR(_qp) \
    ((PRJob *) ((char *) (_qp) - offsetof(PRJob, links)))

#define WTHREAD_LINKS_PTR(_qp) \
    ((wthread *) ((char *) (_qp) - offsetof(wthread, links)))

static void delete_job(PRJob *jobp);
static PRThreadPool * alloc_threadpool();
static PRJob * alloc_job(PRBool joinable);
static void notify_ioq(PRThreadPool *tp);
static void notify_timerq(PRThreadPool *tp);

/*
 * worker thread function
 */
static void wstart(void *arg)
{
PRThreadPool *tp = (PRThreadPool *) arg;
PRCList *head;

	/*
	 * execute jobs until shutdown
	 */
	while (!tp->shutdown) {
		PRJob *jobp;
#ifdef OPT_WINNT
		BOOL rv;
		DWORD unused, shutdown;
		LPOVERLAPPED olp;

		PR_Lock(tp->jobq.lock);
		tp->idle_threads++;
		PR_Unlock(tp->jobq.lock);
		rv = GetQueuedCompletionStatus(tp->jobq.nt_completion_port,
					&unused, &shutdown, &olp, INFINITE);
		
		PR_ASSERT(rv);
		if (shutdown)
			break;
		jobp = ((NT_notifier *) olp)->jobp;
		PR_Lock(tp->jobq.lock);
		tp->idle_threads--;
		tp->jobq.cnt--;
		PR_Unlock(tp->jobq.lock);
		PR_Lock(jobp->jlock);
		jobp->status = JOB_RUNNING;
		PR_Unlock(jobp->jlock);
#else

		PR_Lock(tp->jobq.lock);
		while (PR_CLIST_IS_EMPTY(&tp->jobq.list) && (!tp->shutdown)) {
			tp->idle_threads++;
			PR_WaitCondVar(tp->jobq.cv, PR_INTERVAL_NO_TIMEOUT);
		}	
		tp->idle_threads--;
		if (tp->shutdown) {
			PR_Unlock(tp->jobq.lock);
			break;
		}
		head = PR_LIST_HEAD(&tp->jobq.list);
		/*
		 * remove job from queue
		 */
		PR_REMOVE_AND_INIT_LINK(head);
		tp->jobq.cnt--;
		jobp = JOB_LINKS_PTR(head);
		PR_Lock(jobp->jlock);
		jobp->status = JOB_RUNNING;
		PR_Unlock(jobp->jlock);
		PR_Unlock(tp->jobq.lock);
#endif

		jobp->job_func(jobp->job_arg);
		if (!jobp->joinable) {
			delete_job(jobp);
		} else {
			PR_Lock(jobp->jlock);
			jobp->status = JOB_COMPLETED;
			PR_NotifyCondVar(jobp->join_cv);
			PR_Unlock(jobp->jlock);
		}
	}
	PR_Lock(tp->jobq.lock);
	tp->current_threads--;
	PR_Unlock(tp->jobq.lock);
}

/*
 * add a job to the work queue
 */
static void
add_to_jobq(PRThreadPool *tp, PRJob *jobp)
{
	/*
	 * add to jobq
	 */
#ifdef OPT_WINNT
	PR_Lock(tp->jobq.lock);
	tp->jobq.cnt++;
	PR_Unlock(tp->jobq.lock);
	/*
	 * notify worker thread(s)
	 */
	PostQueuedCompletionStatus(tp->jobq.nt_completion_port, 0,
            FALSE, &jobp->nt_notifier.overlapped);
#else
	PR_Lock(tp->jobq.lock);
	PR_APPEND_LINK(&jobp->links,&tp->jobq.list);
	jobp->status = JOB_QUEUED;
	tp->jobq.cnt++;
	if ((tp->idle_threads < tp->jobq.cnt) &&
					(tp->current_threads < tp->max_threads)) {
		PRThread *thr;
		wthread *wthrp;
		/*
		 * increment thread count and unlock the jobq lock
		 */
		tp->current_threads++;
		PR_Unlock(tp->jobq.lock);
		/* create new worker thread */
		thr = PR_CreateThread(PR_USER_THREAD, wstart,
						tp, PR_PRIORITY_NORMAL,
						PR_GLOBAL_THREAD,PR_JOINABLE_THREAD,tp->stacksize);
		PR_Lock(tp->jobq.lock);
		if (NULL == thr) {
			tp->current_threads--;
		} else {
			wthrp = PR_NEWZAP(wthread);
			wthrp->thread = thr;
			PR_APPEND_LINK(&wthrp->links, &tp->jobq.wthreads);
		}
	}
	/*
	 * wakeup a worker thread
	 */
	PR_NotifyCondVar(tp->jobq.cv);
	PR_Unlock(tp->jobq.lock);
#endif
}

/*
 * io worker thread function
 */
static void io_wstart(void *arg)
{
PRThreadPool *tp = (PRThreadPool *) arg;
int pollfd_cnt, pollfds_used;
int rv;
PRCList *qp;
PRPollDesc *pollfds;
PRJob **polljobs;
int poll_timeout;
PRIntervalTime now;

	/*
	 * scan io_jobq
	 * construct poll list
	 * call PR_Poll
	 * for all fds, for which poll returns true, move the job to
	 * jobq and wakeup worker thread.
	 */
	while (!tp->shutdown) {
		PRJob *jobp;

		pollfd_cnt = tp->ioq.cnt + 10;
		if (pollfd_cnt > tp->ioq.npollfds) {

			/*
			 * re-allocate pollfd array if the current one is not large
			 * enough
			 */
			if (NULL != tp->ioq.pollfds)
				PR_Free(tp->ioq.pollfds);
			tp->ioq.pollfds = (PRPollDesc *) PR_Malloc(pollfd_cnt *
						(sizeof(PRPollDesc) + sizeof(PRJob *)));
			PR_ASSERT(NULL != tp->ioq.pollfds);
			/*
			 * array of pollfds
			 */
			pollfds = tp->ioq.pollfds;
			tp->ioq.polljobs = (PRJob **) (&tp->ioq.pollfds[pollfd_cnt]);
			/*
			 * parallel array of jobs
			 */
			polljobs = tp->ioq.polljobs;
			tp->ioq.npollfds = pollfd_cnt;
		}

		pollfds_used = 0;
		/*
		 * add the notify fd; used for unblocking io thread(s)
		 */
		pollfds[pollfds_used].fd = tp->ioq.notify_fd;
		pollfds[pollfds_used].in_flags = PR_POLL_READ;
		pollfds[pollfds_used].out_flags = 0;
		polljobs[pollfds_used] = NULL;
		pollfds_used++;
		/*
		 * fill in the pollfd array
		 */
		PR_Lock(tp->ioq.lock);
		for (qp = tp->ioq.list.next; qp != &tp->ioq.list; qp = qp->next) {
			jobp = JOB_LINKS_PTR(qp);
			if (pollfds_used == (pollfd_cnt))
				break;
			pollfds[pollfds_used].fd = jobp->iod->socket;
			pollfds[pollfds_used].in_flags = jobp->io_poll_flags;
			pollfds[pollfds_used].out_flags = 0;
			polljobs[pollfds_used] = jobp;

			pollfds_used++;
		}
		if (!PR_CLIST_IS_EMPTY(&tp->ioq.list)) {
			qp = tp->ioq.list.next;
			jobp = JOB_LINKS_PTR(qp);
			if (PR_INTERVAL_NO_TIMEOUT == jobp->timeout)
				poll_timeout = PR_INTERVAL_NO_TIMEOUT;
			else if (PR_INTERVAL_NO_WAIT == jobp->timeout)
				poll_timeout = PR_INTERVAL_NO_WAIT;
			else {
				poll_timeout = jobp->absolute - PR_IntervalNow();
				if (poll_timeout <= 0) /* already timed out */
					poll_timeout = PR_INTERVAL_NO_WAIT;
			}
		} else {
			poll_timeout = PR_INTERVAL_NO_TIMEOUT;
		}
		PR_Unlock(tp->ioq.lock);

		/*
		 * XXXX
		 * should retry if more jobs have been added to the queue?
		 *
		 */
		PR_ASSERT(pollfds_used <= pollfd_cnt);
		rv = PR_Poll(tp->ioq.pollfds, pollfds_used, poll_timeout);

		if (tp->shutdown) {
			break;
		}

		if (rv > 0) {
			/*
			 * at least one io event is set
			 */
			PRStatus rval_status;
			PRInt32 index;

			PR_ASSERT(pollfds[0].fd == tp->ioq.notify_fd);
			/*
			 * reset the pollable event, if notified
			 */
			if (pollfds[0].out_flags & PR_POLL_READ) {
				rval_status = PR_WaitForPollableEvent(tp->ioq.notify_fd);
				PR_ASSERT(PR_SUCCESS == rval_status);
			}

			for(index = 1; index < (pollfds_used); index++) {
                PRInt16 events = pollfds[index].in_flags;
                PRInt16 revents = pollfds[index].out_flags;	
				jobp = polljobs[index];	

                if ((revents & PR_POLL_NVAL) ||  /* busted in all cases */
                			((events & PR_POLL_WRITE) &&
							(revents & PR_POLL_HUP))) { /* write op & hup */
					PR_Lock(tp->ioq.lock);
					PR_REMOVE_AND_INIT_LINK(&jobp->links);
					tp->ioq.cnt--;
					PR_Unlock(tp->ioq.lock);

					/* set error */
                    if (PR_POLL_NVAL & revents)
						jobp->iod->error = PR_BAD_DESCRIPTOR_ERROR;
                    else if (PR_POLL_HUP & revents)
						jobp->iod->error = PR_CONNECT_RESET_ERROR;

					/*
					 * add to jobq
					 */
					add_to_jobq(tp, jobp);
				} else if (revents & events) {
					/*
					 * add to jobq
					 */
					PR_Lock(tp->ioq.lock);
					PR_REMOVE_AND_INIT_LINK(&jobp->links);
					tp->ioq.cnt--;
					PR_Unlock(tp->ioq.lock);

					jobp->iod->error = 0;
					add_to_jobq(tp, jobp);
				}
			}
		}
		/*
		 * timeout processing
		 */
		now = PR_IntervalNow();
		PR_Lock(tp->ioq.lock);
		for (qp = tp->ioq.list.next; qp != &tp->ioq.list; qp = qp->next) {
			jobp = JOB_LINKS_PTR(qp);
			if (PR_INTERVAL_NO_TIMEOUT == jobp->timeout)
				break;
			if ((PR_INTERVAL_NO_WAIT != jobp->timeout) &&
								((PRInt32)(jobp->absolute - now) > 0))
				break;
			PR_REMOVE_AND_INIT_LINK(&jobp->links);
			tp->ioq.cnt--;
			jobp->iod->error = PR_IO_TIMEOUT_ERROR;
			add_to_jobq(tp, jobp);
		}
		PR_Unlock(tp->ioq.lock);
	}
}

/*
 * timer worker thread function
 */
static void timer_wstart(void *arg)
{
PRThreadPool *tp = (PRThreadPool *) arg;
PRCList *qp;
PRIntervalTime timeout;
PRIntervalTime now;

	/*
	 * call PR_WaitCondVar with minimum value of all timeouts
	 */
	while (!tp->shutdown) {
		PRJob *jobp;

		PR_Lock(tp->timerq.lock);
		if (PR_CLIST_IS_EMPTY(&tp->timerq.list)) {
			timeout = PR_INTERVAL_NO_TIMEOUT;
		} else {
			PRCList *qp;

			qp = tp->timerq.list.next;
			jobp = JOB_LINKS_PTR(qp);

			timeout = jobp->absolute - PR_IntervalNow();
            if (timeout <= 0)
				timeout = PR_INTERVAL_NO_WAIT;  /* already timed out */
		}
		if (PR_INTERVAL_NO_WAIT != timeout)
			PR_WaitCondVar(tp->timerq.cv, timeout);
		if (tp->shutdown) {
			PR_Unlock(tp->timerq.lock);
			break;
		}
		/*
		 * move expired-timer jobs to jobq
		 */
		now = PR_IntervalNow();	
		while (!PR_CLIST_IS_EMPTY(&tp->timerq.list)) {
			qp = tp->timerq.list.next;
			jobp = JOB_LINKS_PTR(qp);

			if ((PRInt32)(jobp->absolute - now) > 0) {
				break;
			}
			/*
			 * job timed out
			 */
			PR_REMOVE_AND_INIT_LINK(&jobp->links);
			tp->timerq.cnt--;
			add_to_jobq(tp, jobp);
		}
		PR_Unlock(tp->timerq.lock);
	}
}

static void
delete_threadpool(PRThreadPool *tp)
{
	if (NULL != tp) {
		if (NULL != tp->shutdown_cv)
			PR_DestroyCondVar(tp->shutdown_cv);
		if (NULL != tp->jobq.cv)
			PR_DestroyCondVar(tp->jobq.cv);
		if (NULL != tp->jobq.lock)
			PR_DestroyLock(tp->jobq.lock);
#ifdef OPT_WINNT
		if (NULL != tp->jobq.nt_completion_port)
			CloseHandle(tp->jobq.nt_completion_port);
#endif
		/* Timer queue */
		if (NULL != tp->timerq.cv)
			PR_DestroyCondVar(tp->timerq.cv);
		if (NULL != tp->timerq.lock)
			PR_DestroyLock(tp->timerq.lock);

		if (NULL != tp->ioq.lock)
			PR_DestroyLock(tp->ioq.lock);
		if (NULL != tp->ioq.pollfds)
			PR_Free(tp->ioq.pollfds);
		if (NULL != tp->ioq.notify_fd)
			PR_DestroyPollableEvent(tp->ioq.notify_fd);
		PR_Free(tp);
	}
	return;
}

static PRThreadPool *
alloc_threadpool()
{
PRThreadPool *tp;

	tp = PR_CALLOC(sizeof(*tp));
	if (NULL == tp)
		goto failed;
	tp->jobq.lock = PR_NewLock();
	if (NULL == tp->jobq.lock)
		goto failed;
	tp->jobq.cv = PR_NewCondVar(tp->jobq.lock);
	if (NULL == tp->jobq.cv)
		goto failed;
#ifdef OPT_WINNT
	tp->jobq.nt_completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
									NULL, 0, 0);
	if (NULL == tp->jobq.nt_completion_port)
		goto failed;
#endif

	tp->ioq.lock = PR_NewLock();
	if (NULL == tp->ioq.lock)
		goto failed;

	/* Timer queue */

	tp->timerq.lock = PR_NewLock();
	if (NULL == tp->timerq.lock)
		goto failed;
	tp->timerq.cv = PR_NewCondVar(tp->timerq.lock);
	if (NULL == tp->timerq.cv)
		goto failed;

	tp->shutdown_cv = PR_NewCondVar(tp->jobq.lock);
	if (NULL == tp->shutdown_cv)
		goto failed;
	tp->ioq.notify_fd = PR_NewPollableEvent();
	if (NULL == tp->ioq.notify_fd)
		goto failed;
	return tp;
failed:
	delete_threadpool(tp);
	PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
	return NULL;
}

/* Create thread pool */
PR_IMPLEMENT(PRThreadPool *)
PR_CreateThreadPool(PRInt32 initial_threads, PRInt32 max_threads,
                                PRSize stacksize)
{
PRThreadPool *tp;
PRThread *thr;
int i;
wthread *wthrp;

	tp = alloc_threadpool();
	if (NULL == tp)
		return NULL;

	tp->init_threads = initial_threads;
	tp->max_threads = max_threads;
	tp->stacksize = stacksize;
	PR_INIT_CLIST(&tp->jobq.list);
	PR_INIT_CLIST(&tp->ioq.list);
	PR_INIT_CLIST(&tp->timerq.list);
	PR_INIT_CLIST(&tp->jobq.wthreads);
	PR_INIT_CLIST(&tp->ioq.wthreads);
	PR_INIT_CLIST(&tp->timerq.wthreads);
	tp->shutdown = PR_FALSE;

	PR_Lock(tp->jobq.lock);
	for(i=0; i < initial_threads; ++i) {

		thr = PR_CreateThread(PR_USER_THREAD, wstart,
						tp, PR_PRIORITY_NORMAL,
						PR_GLOBAL_THREAD, PR_JOINABLE_THREAD,stacksize);
		PR_ASSERT(thr);
		wthrp = PR_NEWZAP(wthread);
		PR_ASSERT(wthrp);
		wthrp->thread = thr;
		PR_APPEND_LINK(&wthrp->links, &tp->jobq.wthreads);
	}
	tp->current_threads = initial_threads;

	thr = PR_CreateThread(PR_USER_THREAD, io_wstart,
					tp, PR_PRIORITY_NORMAL,
					PR_GLOBAL_THREAD,PR_JOINABLE_THREAD,stacksize);
	PR_ASSERT(thr);
	wthrp = PR_NEWZAP(wthread);
	PR_ASSERT(wthrp);
	wthrp->thread = thr;
	PR_APPEND_LINK(&wthrp->links, &tp->ioq.wthreads);

	thr = PR_CreateThread(PR_USER_THREAD, timer_wstart,
					tp, PR_PRIORITY_NORMAL,
					PR_GLOBAL_THREAD,PR_JOINABLE_THREAD,stacksize);
	PR_ASSERT(thr);
	wthrp = PR_NEWZAP(wthread);
	PR_ASSERT(wthrp);
	wthrp->thread = thr;
	PR_APPEND_LINK(&wthrp->links, &tp->timerq.wthreads);

	PR_Unlock(tp->jobq.lock);
	return tp;
}

static void
delete_job(PRJob *jobp)
{
	if (NULL != jobp) {
		if (NULL != jobp->jlock) {
			PR_DestroyLock(jobp->jlock);
			jobp->jlock = NULL;
		}
		if (NULL != jobp->join_cv) {
			PR_DestroyCondVar(jobp->join_cv);
			jobp->join_cv = NULL;
		}
		jobp->status = JOB_FREED;
		PR_DELETE(jobp);
	}
}

static PRJob *
alloc_job(PRBool joinable)
{
	PRJob *jobp;

	jobp = PR_NEWZAP(PRJob);
	if (NULL == jobp) 
		goto failed;
	jobp->jlock = PR_NewLock();
	if (NULL == jobp->jlock)
		goto failed;
	if (joinable) {
		jobp->join_cv = PR_NewCondVar(jobp->jlock);
		if (NULL == jobp->join_cv)
			goto failed;
	} else {
		jobp->join_cv = NULL;
	}
#ifdef OPT_WINNT
	jobp->nt_notifier.jobp = jobp;
#endif
	return jobp;
failed:
	delete_job(jobp);
	PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
	return NULL;
}

/* queue a job */
PR_IMPLEMENT(PRJob *)
PR_QueueJob(PRThreadPool *tpool, JobFn fn, void *arg, PRBool joinable)
{
	PRJob *jobp;

	jobp = alloc_job(joinable);
	if (NULL == jobp)
		return NULL;

	jobp->job_func = fn;
	jobp->job_arg = arg;
	jobp->tpool = tpool;
	jobp->joinable = joinable;

	add_to_jobq(tpool, jobp);
	return jobp;
}

/* queue a job, when a socket is readable */
static PRJob *
queue_io_job(PRThreadPool *tpool, PRJobIoDesc *iod, JobFn fn, void * arg,
				PRBool joinable, io_op_type op)
{
	PRJob *jobp;
	PRIntervalTime now;

	jobp = alloc_job(joinable);
	if (NULL == jobp) {
		return NULL;
	}

	/*
	 * Add a new job to io_jobq
	 * wakeup io worker thread
	 */

	jobp->job_func = fn;
	jobp->job_arg = arg;
	jobp->status = JOB_ON_IOQ;
	jobp->tpool = tpool;
	jobp->joinable = joinable;
	jobp->iod = iod;
	if (JOB_IO_READ == op) {
		jobp->io_op = JOB_IO_READ;
		jobp->io_poll_flags = PR_POLL_READ;
	} else if (JOB_IO_WRITE == op) {
		jobp->io_op = JOB_IO_WRITE;
		jobp->io_poll_flags = PR_POLL_WRITE;
	} else if (JOB_IO_ACCEPT == op) {
		jobp->io_op = JOB_IO_ACCEPT;
		jobp->io_poll_flags = PR_POLL_READ;
	} else if (JOB_IO_CONNECT == op) {
		jobp->io_op = JOB_IO_CONNECT;
		jobp->io_poll_flags = PR_POLL_WRITE;
	} else {
		delete_job(jobp);
		PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
		return NULL;
	}

	jobp->timeout = iod->timeout;
	if ((PR_INTERVAL_NO_TIMEOUT == iod->timeout) ||
			(PR_INTERVAL_NO_WAIT == iod->timeout)) {
		jobp->absolute = iod->timeout;
	} else {
		now = PR_IntervalNow();
		jobp->absolute = now + iod->timeout;
	}


	PR_Lock(tpool->ioq.lock);

	if (PR_CLIST_IS_EMPTY(&tpool->ioq.list) ||
			(PR_INTERVAL_NO_TIMEOUT == iod->timeout)) {
		PR_APPEND_LINK(&jobp->links,&tpool->ioq.list);
	} else if (PR_INTERVAL_NO_WAIT == iod->timeout) {
		PR_INSERT_LINK(&jobp->links,&tpool->ioq.list);
	} else {
		PRCList *qp;
		PRJob *tmp_jobp;
		/*
		 * insert into the timeout-sorted ioq
		 */
		for (qp = tpool->ioq.list.prev; qp != &tpool->ioq.list;
							qp = qp->prev) {
			tmp_jobp = JOB_LINKS_PTR(qp);
			if ((PRInt32)(jobp->absolute - tmp_jobp->absolute) >= 0) {
				break;
			}
		}
		PR_INSERT_AFTER(&jobp->links,qp);
	}

	tpool->ioq.cnt++;
	/*
	 * notify io worker thread(s)
	 */
	PR_Unlock(tpool->ioq.lock);
	notify_ioq(tpool);
	return jobp;
}

/* queue a job, when a socket is readable */
PR_IMPLEMENT(PRJob *)
PR_QueueJob_Read(PRThreadPool *tpool, PRJobIoDesc *iod, JobFn fn, void * arg,
											PRBool joinable)
{
	return (queue_io_job(tpool, iod, fn, arg, joinable, JOB_IO_READ));
}

/* queue a job, when a socket is writeable */
PR_IMPLEMENT(PRJob *)
PR_QueueJob_Write(PRThreadPool *tpool, PRJobIoDesc *iod, JobFn fn,void * arg,
										PRBool joinable)
{
	return (queue_io_job(tpool, iod, fn, arg, joinable, JOB_IO_WRITE));
}


/* queue a job, when a socket has a pending connection */
PR_IMPLEMENT(PRJob *)
PR_QueueJob_Accept(PRThreadPool *tpool, PRJobIoDesc *iod, JobFn fn, void * arg,
												PRBool joinable)
{
	return (queue_io_job(tpool, iod, fn, arg, joinable, JOB_IO_ACCEPT));
}

/* queue a job, when a socket can be connected */
PR_IMPLEMENT(PRJob *)
PR_QueueJob_Connect(PRThreadPool *tpool, PRJobIoDesc *iod, PRNetAddr *addr,
				JobFn fn, void * arg, PRBool joinable)
{
	/*
	 * not implemented
	 */
	 return NULL;
}

/* queue a job, when a timer expires */
PR_IMPLEMENT(PRJob *)
PR_QueueJob_Timer(PRThreadPool *tpool, PRIntervalTime timeout,
							JobFn fn, void * arg, PRBool joinable)
{
	PRIntervalTime now;
	PRJob *jobp;

	if (PR_INTERVAL_NO_TIMEOUT == timeout) {
		PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
		return NULL;
	}
	if (PR_INTERVAL_NO_WAIT == timeout) {
		/*
		 * no waiting; add to jobq right away
		 */
		return(PR_QueueJob(tpool, fn, arg, joinable));
	}
	jobp = alloc_job(joinable);
	if (NULL == jobp) {
		return NULL;
	}

	/*
	 * Add a new job to timer_jobq
	 * wakeup timer worker thread
	 */

	jobp->job_func = fn;
	jobp->job_arg = arg;
	jobp->status = JOB_ON_TIMERQ;
	jobp->tpool = tpool;
	jobp->joinable = joinable;
	jobp->timeout = timeout;

	now = PR_IntervalNow();
	jobp->absolute = now + timeout;


	PR_Lock(tpool->timerq.lock);
	if (PR_CLIST_IS_EMPTY(&tpool->timerq.list))
		PR_APPEND_LINK(&jobp->links,&tpool->timerq.list);
	else {
		PRCList *qp;
		PRJob *tmp_jobp;
		/*
		 * insert into the sorted timer jobq
		 */
		for (qp = tpool->timerq.list.prev; qp != &tpool->timerq.list;
							qp = qp->prev) {
			tmp_jobp = JOB_LINKS_PTR(qp);
			if ((PRInt32)(jobp->absolute - tmp_jobp->absolute) >= 0) {
				break;
			}
		}
		PR_INSERT_AFTER(&jobp->links,qp);
	}
	tpool->timerq.cnt++;
	/*
	 * notify timer worker thread(s)
	 */
	notify_timerq(tpool);
	PR_Unlock(tpool->timerq.lock);
	return jobp;
}

static void
notify_timerq(PRThreadPool *tp)
{
	/*
	 * wakeup the timer thread(s)
	 */
	PR_NotifyCondVar(tp->timerq.cv);
}

static void
notify_ioq(PRThreadPool *tp)
{
PRStatus rval_status;

	/*
	 * wakeup the io thread(s)
	 */
	rval_status = PR_SetPollableEvent(tp->ioq.notify_fd);
	PR_ASSERT(PR_SUCCESS == rval_status);
}

/*
 * cancel a job
 *
 *	XXXX: is this needed? likely to be removed
 */
PR_IMPLEMENT(PRStatus)
PR_CancelJob(PRJob *jobp) {

	PRStatus rval = PR_FAILURE;
	PRThreadPool *tp;

	if (JOB_QUEUED == jobp->status) {
		/*
		 * now, check again while holding thread pool lock
		 */
		tp = jobp->tpool;
		PR_Lock(tp->jobq.lock);
		PR_Lock(jobp->jlock);
		if (JOB_QUEUED == jobp->status) {
			PR_REMOVE_AND_INIT_LINK(&jobp->links);
			if (!jobp->joinable) {
				PR_Unlock(jobp->jlock);
				delete_job(jobp);
			} else {
				jobp->status = JOB_CANCELED;
				PR_NotifyCondVar(jobp->join_cv);
				PR_Unlock(jobp->jlock);
			}
			rval = PR_SUCCESS;
		}
		PR_Unlock(tp->jobq.lock);
	}
	return rval;
}

/* join a job, wait until completion */
PR_IMPLEMENT(PRStatus)
PR_JoinJob(PRJob *jobp)
{
	/*
	 * No references to the thread pool
	 */
	if (!jobp->joinable) {
		PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
		return PR_FAILURE;
	}
	PR_Lock(jobp->jlock);
	while((JOB_COMPLETED != jobp->status) &&
			(JOB_CANCELED != jobp->status))
		PR_WaitCondVar(jobp->join_cv, PR_INTERVAL_NO_TIMEOUT);

	PR_Unlock(jobp->jlock);
	delete_job(jobp);
	return PR_SUCCESS;
}

/* shutdown threadpool */
PR_IMPLEMENT(PRStatus)
PR_ShutdownThreadPool(PRThreadPool *tpool)
{
PRStatus rval = PR_SUCCESS;

	PR_Lock(tpool->jobq.lock);
	tpool->shutdown = PR_TRUE;
	PR_NotifyAllCondVar(tpool->shutdown_cv);
	PR_Unlock(tpool->jobq.lock);

	return rval;
}

/*
 * join thread pool
 * 	wait for termination of worker threads
 *	reclaim threadpool resources
 */
PR_IMPLEMENT(PRStatus)
PR_JoinThreadPool(PRThreadPool *tpool)
{
PRStatus rval = PR_SUCCESS;
PRCList *head;
PRStatus rval_status;

	PR_Lock(tpool->jobq.lock);
	while (!tpool->shutdown)
		PR_WaitCondVar(tpool->shutdown_cv, PR_INTERVAL_NO_TIMEOUT);

	/*
	 * wakeup worker threads
	 */
#ifdef OPT_WINNT
	/*
	 * post shutdown notification for all threads
	 */
	{
		int i;
		for(i=0; i < tpool->current_threads; i++) {
			PostQueuedCompletionStatus(tpool->jobq.nt_completion_port, 0,
												TRUE, NULL);
		}
	}
#else
	PR_NotifyAllCondVar(tpool->jobq.cv);
#endif

	/*
	 * wakeup io thread(s)
	 */
	PR_Lock(tpool->ioq.lock);
	notify_ioq(tpool);
	PR_Unlock(tpool->ioq.lock);

	/*
	 * wakeup timer thread(s)
	 */
	PR_Lock(tpool->timerq.lock);
	notify_timerq(tpool);
	PR_Unlock(tpool->timerq.lock);

	while (!PR_CLIST_IS_EMPTY(&tpool->jobq.wthreads)) {
		wthread *wthrp;

		head = PR_LIST_HEAD(&tpool->jobq.wthreads);
		PR_REMOVE_AND_INIT_LINK(head);
		PR_Unlock(tpool->jobq.lock);
		wthrp = WTHREAD_LINKS_PTR(head);
		rval_status = PR_JoinThread(wthrp->thread);
		PR_ASSERT(PR_SUCCESS == rval_status);
		PR_DELETE(wthrp);
		PR_Lock(tpool->jobq.lock);
	}
	PR_Unlock(tpool->jobq.lock);
	while (!PR_CLIST_IS_EMPTY(&tpool->ioq.wthreads)) {
		wthread *wthrp;

		head = PR_LIST_HEAD(&tpool->ioq.wthreads);
		PR_REMOVE_AND_INIT_LINK(head);
		wthrp = WTHREAD_LINKS_PTR(head);
		rval_status = PR_JoinThread(wthrp->thread);
		PR_ASSERT(PR_SUCCESS == rval_status);
		PR_DELETE(wthrp);
	}

	while (!PR_CLIST_IS_EMPTY(&tpool->timerq.wthreads)) {
		wthread *wthrp;

		head = PR_LIST_HEAD(&tpool->timerq.wthreads);
		PR_REMOVE_AND_INIT_LINK(head);
		wthrp = WTHREAD_LINKS_PTR(head);
		rval_status = PR_JoinThread(wthrp->thread);
		PR_ASSERT(PR_SUCCESS == rval_status);
		PR_DELETE(wthrp);
	}

	/*
	 * Delete unjoinable jobs; joinable jobs must be reclaimed by the user
	 */
	while (!PR_CLIST_IS_EMPTY(&tpool->jobq.list)) {
		PRJob *jobp;

		head = PR_LIST_HEAD(&tpool->jobq.list);
		PR_REMOVE_AND_INIT_LINK(head);
		jobp = JOB_LINKS_PTR(head);
		tpool->jobq.cnt--;

		if (!jobp->joinable) {
			delete_job(jobp);
		} else {
			PR_Lock(jobp->jlock);
			jobp->status = JOB_CANCELED;
			PR_NotifyCondVar(jobp->join_cv);
			PR_Unlock(jobp->jlock);
		}
	}

	while (!PR_CLIST_IS_EMPTY(&tpool->ioq.list)) {
		PRJob *jobp;

		head = PR_LIST_HEAD(&tpool->ioq.list);
		PR_REMOVE_AND_INIT_LINK(head);
		tpool->ioq.cnt--;
		jobp = JOB_LINKS_PTR(head);
		if (!jobp->joinable) {
			delete_job(jobp);
		} else {
			PR_Lock(jobp->jlock);
			jobp->status = JOB_CANCELED;
			PR_NotifyCondVar(jobp->join_cv);
			PR_Unlock(jobp->jlock);
		}
	}

	while (!PR_CLIST_IS_EMPTY(&tpool->timerq.list)) {
		PRJob *jobp;

		head = PR_LIST_HEAD(&tpool->timerq.list);
		PR_REMOVE_AND_INIT_LINK(head);
		tpool->timerq.cnt--;
		jobp = JOB_LINKS_PTR(head);
		if (!jobp->joinable) {
			delete_job(jobp);
		} else {
			PR_Lock(jobp->jlock);
			jobp->status = JOB_CANCELED;
			PR_NotifyCondVar(jobp->join_cv);
			PR_Unlock(jobp->jlock);
		}
	}

	PR_ASSERT(0 == tpool->jobq.cnt);
	PR_ASSERT(0 == tpool->ioq.cnt);
	PR_ASSERT(0 == tpool->timerq.cnt);

	delete_threadpool(tpool);
	return rval;
}
