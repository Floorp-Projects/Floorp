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

#ifndef MGCP_TIMER_H
#define MGCP_TIMER_H

#define TIMER_FREE 0x1          /* Indicates timer is free */
#define TIMER_INITIALIZED 0x2   /* Indicates timer is initialized */
#define TIMER_ACTIVE 0x4        /* Indicates timer is in list */

/* Timer event structure */
typedef struct timer_struct
{
    unsigned int expiration_time; /* Expiration time */
    int interval;                 /* Timer period */
    void *parameter1;             /* Timer expiration callback param */
    void *parameter2;             /* Second timer expiration callback param */
    void (*expiration_callback)
      (void *timer, void *parameter1, void *parameter2);  /* Expiry handler */
    int flags;                    /* Debugging flags */
    struct timer_struct *pred;    /* List predecessor */
    struct timer_struct *next;    /* List successor */
} timer_struct_type;

extern unsigned long current_time(void);
extern void timer_event_activate(timer_struct_type *timer);
extern void *timer_event_allocate(void);
extern void timer_event_cancel(timer_struct_type *timer);
extern void timer_event_free(timer_struct_type *timer);
extern void timer_event_initialize(timer_struct_type *timer,
                                   int period,
                                   void (*expiration)(void *timer_event,
                                                      void *param1,
                                                      void *param2),
                                   void *param1, void *param2);
extern void timer_event_process(void);
extern void timer_event_system_init(void);
extern boolean timer_expired(void);
extern void platform_timer_tick(void);
extern void platform_timer_init(void);

#ifdef _WIN32
extern void platform_timer_stop(void);
#endif
extern int timer_ms_to_ticks(int milliseconds);
extern boolean is_timer_active(timer_struct_type *timer);

#endif
