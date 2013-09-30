/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NUWA_H_
#define __NUWA_H_

#include <setjmp.h>
#include <unistd.h>

#include "mozilla/Types.h"

/**
 * Nuwa is a goddess who created mankind according to her figure in the
 * ancient Chinese mythology.
 */

// Max number of top level protocol that can be handled by Nuwa process.
#ifndef NUWA_TOPLEVEL_MAX
#define NUWA_TOPLEVEL_MAX 8
#endif

struct NuwaProtoFdInfo {
    int protoId;
    int originFd;
    int newFds[2];
};

#define NUWA_NEWFD_PARENT 0
#define NUWA_NEWFD_CHILD 1

extern "C" {

/**
 * The following 3 methods are used to synchronously spawn the Nuwa process.
 * The typical usage is:
 *
 *   The IPC thread                                The main thread
 *   1. Receives the request.
 *   2. NuwaSpawnPrepare().
 *   3. Request main thread to
 *      spawn. --------------------------------------+
 *   4. NuwaSpawnWait().                             V
 *                                                 5. NuwaSpawn() to clone all
 *                                                    the threads in the Nuwa
 *                                                    process (including main
 *                                                    & IPC threads).
 *                                                 6. NuwaSpawn() finishes and
 *            +-------------------------------------- signals NuwaSpawnWait().
 *            V
 *   7. NuwaSpawnWait() returns.
 */

/**
 * Prepare for spawning a new process. The calling thread (typically the IPC
 * thread) will block further requests to spawn a new process.
 */
MFBT_API void NuwaSpawnPrepare();

/**
 * Called on the thread that receives the request. Used to wait until
 * NuwaSpawn() finishes and maintain the stack of the calling thread.
 */
MFBT_API void NuwaSpawnWait();

/**
 * Spawn a new content process, called in the Nuwa process. This has to run on
 * the main thread.
 *
 * @return The process ID of the new process.
 */
MFBT_API pid_t NuwaSpawn();

/**
 * The following methods are for marking threads created in the Nuwa process so
 * they can be frozen and then recreated in the spawned process. All threads
 * except the main thread must be marked by one of the following 4 methods;
 * otherwise, we have a thread that is created but unknown to us. The Nuwa
 * process will never become ready and this needs to be fixed.
 */

/**
 * Mark the current thread as supporting Nuwa. The thread will implicitly freeze
 * by calling a blocking method such as pthread_mutex_lock() or epoll_wait().
 *
 * @param recreate The custom function that will be called in the spawned
 *                 process, after the thread is recreated. Can be nullptr if no
 *                 custom function to be called after the thread is recreated.
 * @param arg The argument passed to the custom function. Can be nullptr.
 */
MFBT_API void NuwaMarkCurrentThread(void (*recreate)(void *), void *arg);

/**
 * Mark the current thread as not supporting Nuwa. The calling thread will not
 * be automatically cloned from the Nuwa process to spawned process. If this
 * thread needs to be created in the spawned process, use NuwaAddConstructor()
 * or NuwaAddFinalConstructor() to do it.
 */
MFBT_API void NuwaSkipCurrentThread();

/**
 * Force the current thread to freeze explicitly at the current point.
 */
MFBT_API void NuwaFreezeCurrentThread();

/**
 * Helper functions for the NuwaCheckpointCurrentThread() macro.
 */
MFBT_API jmp_buf* NuwaCheckpointCurrentThread1();

MFBT_API bool NuwaCheckpointCurrentThread2(int setjmpCond);

/**
 * Set the point to recover after thread recreation. The calling thread is not
 * blocked. This is used for the thread that cannot freeze (i.e. the IPC
 * thread).
 */
#define NuwaCheckpointCurrentThread() \
  NuwaCheckpointCurrentThread2(setjmp(*NuwaCheckpointCurrentThread1()))

/**
 * The following methods are called in the initialization stage of the Nuwa
 * process.
 */

/**
 * Prepare for making the Nuwa process.
 */
MFBT_API void PrepareNuwaProcess();

/**
 * Make the current process a Nuwa process. Start to freeze the threads.
 */
MFBT_API void MakeNuwaProcess();

/**
 * Register a method to be invoked after a new process is spawned. The method
 * will be invoked on the main thread.
 *
 * @param construct The method to be invoked.
 * @param arg The argument passed to the method.
 */
MFBT_API void NuwaAddConstructor(void (*construct)(void *), void *arg);

/**
 * Register a method to be invoked after a new process is spawned and threads
 * are recreated. The method will be invoked on the main thread.
 *
 * @param construct The method to be invoked.
 * @param arg The argument passed to the method.
 */
MFBT_API void NuwaAddFinalConstructor(void (*construct)(void *), void *arg);

/**
 * The methods to query the Nuwa-related states.
 */

/**
 * @return If the current process is the Nuwa process.
 */
MFBT_API bool IsNuwaProcess();

/**
 * @return If the Nuwa process is ready for spawning new processes.
 */
MFBT_API bool IsNuwaReady();
};

#endif /* __NUWA_H_ */
