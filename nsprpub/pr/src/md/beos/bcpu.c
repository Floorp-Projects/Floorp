/* -*- Mode: C++; c-basic-offset: 4 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
