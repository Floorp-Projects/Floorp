/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
