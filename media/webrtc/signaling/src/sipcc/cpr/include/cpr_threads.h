/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_THREADS_H_
#define _CPR_THREADS_H_

#include "cpr_types.h"

__BEGIN_DECLS

/**
 * @typedef void *cprThread_t
 * Define handle for threads
 */
typedef void *cprThread_t;

/**
 * @struct cpr_thread_t
 * System thread information needed to hide OS differences in implementation.
 * To use threads, the application code may pass in a name to the
 * create function for threads. CPR does not use this field, it is
 * solely for the convenience of the application and to aid in debugging.
 * Upon an application calling the init routine, CPR will malloc the
 * memory for a thread, set the handlePtr or handleInt as appropriate
 * and return a pointer to the thread structure.
 */
typedef struct {
    const char *name;
    uint32_t threadId;
    union {
        void *handlePtr;
        uint64_t handleInt;
    } u;
} cpr_thread_t;

/**
 * Thread start routine signature
 */
typedef void *(*cprThreadStartRoutine)(void *data);

/* Function prototypes */

/**
 * cprCreateThread
 *
 * @brief Create a thread
 *
 *  The cprCreateThread function creates another execution thread within the
 *  current process. If the input parameter "name" is present, then this is used
 *  for debugging purposes. The startRoutine is the address of the function where
 *  the thread execution begins. The start routine prototype is defined as
 *  follows
 *  @code
 *     int32_t (*cprThreadStartRoutine)(void* data)
 *  @endcode
 *
 * @param[in]  name         - name of the thread created (optional)
 * @param[in]  startRoutine - function where thread execution begins
 * @param[in]  stackSize    - size of the thread's stack (IGNORED)
 * @param[in]  priority     - thread's execution priority (IGNORED)
 * @param[in]  data         - parameter to pass to startRoutine
 *
 * Return Value: Thread handle or NULL if creation failed.
 */
cprThread_t cprCreateThread(const char *name,
                            cprThreadStartRoutine startRoutine,
                            uint16_t stackSize,
                            uint16_t priority,
                            void *data);


/*
 * cprJoinThread
 *
 * wait for thread termination
 */
void cprJoinThread(cprThread_t thread);

/**
 * cprDestroyThread
 *
 * @brief Destroys the thread passed in.
 *
 * The cprDestroyThread function is called to destroy a thread. The thread
 * parameter may be any valid thread including the calling thread itself.
 *
 * @param[in] thread - thread to destroy.
 *
 * @return CPR_SUCCESS or CPR_FAILURE. errno should be set for FAILURE case.
 *
 * @note In Linux there will never be a success indication as the
 *       calling thread will have been terminated.
 */
cprRC_t cprDestroyThread(cprThread_t thread);

/**
 * cprAdjustRelativeThreadPriority
 *
 * @brief The function sets the relative thread priority up or down by the given value.
 *
 * pSIPCC specific important notes:
 * This function is used by pSIPCC to set up the priority for its threads.
 * When pSIPCC creates threads, it requests default priority
 * and then subsequently calls this function to adjust the level to desired
 * value.
 * relPri ranges from -20 (Maximum priority) to +19 (Minimum priority).
 * The relPri values are normalized to Linux Nice levels. This implies
 * that these values must be mapped to a range or values of your OS.
 * For example:
 * if your OS supports thread priorities ranging from 1 to 10 then you
 * could do a simple linear mapping of 40 nice values to 10 values
 * by mapping 1 to 19 and 10 to -20 and so on.
 * The mapping is completely determined by your implementation below.
 * Note that pSIPCC threads must run at a sufficiently higher priority to
 * provide proper signaling cut through times.
 * obviously, signaling cut through ultimately affects the media cut through.
 * pSIPCC also uses virtual timers which require sufficiently high priority
 * to provide accurate timeout behavior.
 * A general guideline is to map the relPri passed such that it is somewhere
 * in between the non critical threads and highly critical threads such as
 * media or timer processing on your system.
 *
 * @param[in] relPri - normalized relative Priority of the thread [-20,+19]
 *
 * @return CPR_SUCCESS or CPR_FAILURE
 */
cprRC_t cprAdjustRelativeThreadPriority(int relPri);

__END_DECLS

#endif
