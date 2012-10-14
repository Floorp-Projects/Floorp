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
 * cprCreateMutex
 *
 * @brief Creates a mutual exclusion block
 *
 * The cprCreateMutex function is called to allow the OS to perform whatever
 * work is needed to create a mutex.
 *
 * @param[in] name  - name of the mutex. If present, CPR assigns this name to
 * the mutex to assist in debugging.
 *
 * @return Mutex handle or NULL if creation failed. If NULL, set errno
 */
cprMutex_t
cprCreateMutex(const char * name);


/**
 * cprDestroyMutex
 *
 * @brief Destroys the mutex passed in.
 *
 * The cprDestroyMutex function is called to destroy a mutex. It is the
 * application's responsibility to ensure that the mutex is unlocked when
 * destroyed. Unpredictiable behavior will occur if an application
 * destroys a locked mutex.
 *
 * @param[in] mutex - mutex to destroy
 *
 * @return CPR_SUCCESS or CPR_FAILURE. errno should be set for CPR_FAILURE.
 */
cprRC_t
cprDestroyMutex(cprMutex_t mutex);


/**
 * cprGetMutex
 *
 * @brief Acquire ownership of a mutex
 *
 * This function locks the mutex referenced by the mutex parameter. If the mutex
 * is locked by another thread, the calling thread will block until the mutex is
 * released.
 *
 * @param[in] mutex - Which mutex to acquire
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 */
cprRC_t
cprGetMutex(cprMutex_t mutex);


/**
 * cprReleaseMutex
 *
 * @brief Release ownership of a mutex
 *
 * This function unlocks the mutex referenced by the mutex parameter.
 * @param[in] mutex - Which mutex to release
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 */
cprRC_t
cprReleaseMutex(cprMutex_t mutex);


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
