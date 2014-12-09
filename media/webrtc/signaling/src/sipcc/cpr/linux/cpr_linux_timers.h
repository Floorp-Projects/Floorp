/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_LINUX_TIMERS_H_
#define _CPR_LINUX_TIMERS_H_
#include <pthread.h>

/*
 * Linux does not provide native support for non-blocking timers
 * so CPR provides that functionality for Linux.
 */

/*
 * Determine the granularity of the timers in
 * milliseconds. ie how often does the TickThread
 * wake up to decrement the timer intervals
 */
#define timerGranularity 10

//struct timerBlk_s timerBlk;

typedef struct cpr_timer_s
{
  const char *name;
  uint32_t cprTimerId;
  cprMsgQueue_t callBackMsgQueue;
  uint16_t applicationTimerId;
  uint16_t applicationMsgId;
  void *data;
  union {
    void *handlePtr;
  }u;
}cpr_timer_t;

/* Linked List of currently running timers */
typedef struct timerDef
{
    int32_t duration;
    boolean timerActive;
    cpr_timer_t *cprTimerPtr;
    struct timerDef *previous;
    struct timerDef *next;
} timerBlk;

/* Timer mutex for updating the timer linked list */
extern pthread_mutex_t timerMutex;

/* Start routines for the timer threads */
extern void *linuxTimerTick(void *);

cprRC_t cpr_timer_pre_init(void);
cprRC_t cpr_timer_de_init(void);

#endif
