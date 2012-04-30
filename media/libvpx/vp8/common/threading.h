/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef _PTHREAD_EMULATION
#define _PTHREAD_EMULATION

#if CONFIG_OS_SUPPORT && CONFIG_MULTITHREAD

/* Thread management macros */
#ifdef _WIN32
/* Win32 */
#include <process.h>
#include <windows.h>
#define THREAD_FUNCTION DWORD WINAPI
#define THREAD_FUNCTION_RETURN DWORD
#define THREAD_SPECIFIC_INDEX DWORD
#define pthread_t HANDLE
#define pthread_attr_t DWORD
#define pthread_create(thhandle,attr,thfunc,tharg) (int)((*thhandle=(HANDLE)_beginthreadex(NULL,0,(unsigned int (__stdcall *)(void *))thfunc,tharg,0,NULL))==NULL)
#define pthread_join(thread, result) ((WaitForSingleObject((thread),INFINITE)!=WAIT_OBJECT_0) || !CloseHandle(thread))
#define pthread_detach(thread) if(thread!=NULL)CloseHandle(thread)
#define thread_sleep(nms) Sleep(nms)
#define pthread_cancel(thread) terminate_thread(thread,0)
#define ts_key_create(ts_key, destructor) {ts_key = TlsAlloc();};
#define pthread_getspecific(ts_key) TlsGetValue(ts_key)
#define pthread_setspecific(ts_key, value) TlsSetValue(ts_key, (void *)value)
#define pthread_self() GetCurrentThreadId()
#else
#ifdef __APPLE__
#include <mach/mach_init.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#include <time.h>
#include <unistd.h>

#else
#include <semaphore.h>
#endif

#include <pthread.h>
/* pthreads */
/* Nearly everything is already defined */
#define THREAD_FUNCTION void *
#define THREAD_FUNCTION_RETURN void *
#define THREAD_SPECIFIC_INDEX pthread_key_t
#define ts_key_create(ts_key, destructor) pthread_key_create (&(ts_key), destructor);
#endif

/* Syncrhronization macros: Win32 and Pthreads */
#ifdef _WIN32
#define sem_t HANDLE
#define pause(voidpara) __asm PAUSE
#define sem_init(sem, sem_attr1, sem_init_value) (int)((*sem = CreateSemaphore(NULL,0,32768,NULL))==NULL)
#define sem_wait(sem) (int)(WAIT_OBJECT_0 != WaitForSingleObject(*sem,INFINITE))
#define sem_post(sem) ReleaseSemaphore(*sem,1,NULL)
#define sem_destroy(sem) if(*sem)((int)(CloseHandle(*sem))==TRUE)
#define thread_sleep(nms) Sleep(nms)

#else

#ifdef __APPLE__
#define sem_t semaphore_t
#define sem_init(X,Y,Z) semaphore_create(mach_task_self(), X, SYNC_POLICY_FIFO, Z)
#define sem_wait(sem) (semaphore_wait(*sem) )
#define sem_post(sem) semaphore_signal(*sem)
#define sem_destroy(sem) semaphore_destroy(mach_task_self(),*sem)
#define thread_sleep(nms) /* { struct timespec ts;ts.tv_sec=0; ts.tv_nsec = 1000*nms;nanosleep(&ts, NULL);} */
#else
#include <unistd.h>
#include <sched.h>
#define thread_sleep(nms) sched_yield();/* {struct timespec ts;ts.tv_sec=0; ts.tv_nsec = 1000*nms;nanosleep(&ts, NULL);} */
#endif
/* Not Windows. Assume pthreads */

#endif

#if ARCH_X86 || ARCH_X86_64
#include "vpx_ports/x86.h"
#else
#define x86_pause_hint()
#endif

#endif /* CONFIG_OS_SUPPORT && CONFIG_MULTITHREAD */

#endif
