/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
