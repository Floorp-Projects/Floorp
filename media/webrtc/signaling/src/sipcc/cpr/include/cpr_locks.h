/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_LOCKS_H_
#define _CPR_LOCKS_H_

#include "cpr_types.h"
#include "cpr_time.h"

__BEGIN_DECLS

/**
 * Define handle for mutexes
 */
typedef void* cprMutex_t;

/**
 * Mutex information needed to hide OS differences in implementation.
 * To use mutexes, the application code may pass in a name to the
 * create function for mutexes. CPR does not use this field, it is
 * solely for the convience of the application and to aid in debugging.
 * Upon an application calling the init routine, CPR will malloc the
 * memory for a mutex, set the handlePtr or handleInt as appropriate
 * and return a pointer to the mutex structure.
 */
typedef struct {
    const char*  name;
    uint16_t lockId;
    union {
      void* handlePtr;
      uint32_t handleInt;
    } u;
} cpr_mutex_t;


/**
 * Define handle for conditions
 */
typedef void* cprSignal_t;

/**
 * Condition information needed to hide OS differences in implementation.
 * To use conditions, the application code may pass in a name to the
 * create function for mutexes. CPR does not use this field, it is
 * solely for the convience of the application and to aid in debugging.
 * Upon an application calling the init routine, CPR will malloc the
 * memory for a condition, set the handlePtr or handleInt as appropriate
 * and return a pointer to the condition structure.
 */
typedef struct {
    const char *name;
    uint16_t lockId;
    union {
      void *handlePtr;
      uint32_t handleInt;
    } u;
} cpr_signal_t;


__END_DECLS

#endif
