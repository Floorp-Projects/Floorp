/* -*- Mode: C++; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http:// www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 */

#include "primpl.h"

PR_EXTERN(void) _PR_MD_INIT_CPUS();
PR_EXTERN(void) _PR_MD_WAKEUP_CPUS();
PR_EXTERN(void) _PR_MD_START_INTERRUPTS(void);
PR_EXTERN(void) _PR_MD_STOP_INTERRUPTS(void);
PR_EXTERN(void) _PR_MD_DISABLE_CLOCK_INTERRUPTS(void);
PR_EXTERN(void) _PR_MD_BLOCK_CLOCK_INTERRUPTS(void);
PR_EXTERN(void) _PR_MD_UNBLOCK_CLOCK_INTERRUPTS(void);
PR_EXTERN(void) _PR_MD_CLOCK_INTERRUPT(void);
PR_EXTERN(void) _PR_MD_INIT_STACK(PRThreadStack *ts, PRIntn redzone);
PR_EXTERN(void) _PR_MD_CLEAR_STACK(PRThreadStack* ts);
PR_EXTERN(PRInt32) _PR_MD_GET_INTSOFF(void);
PR_EXTERN(void) _PR_MD_SET_INTSOFF(PRInt32 _val);
PR_EXTERN(_PRCPU*) _PR_MD_CURRENT_CPU(void);
PR_EXTERN(void) _PR_MD_SET_CURRENT_CPU(_PRCPU *cpu);
PR_EXTERN(void) _PR_MD_INIT_RUNNING_CPU(_PRCPU *cpu);
PR_EXTERN(PRInt32) _PR_MD_PAUSE_CPU(PRIntervalTime timeout);
