/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>
#include <cstdio>
#include <fcntl.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

#include "replace_malloc.h"
#include "FdPrintf.h"

#include "base/lock.h"

static const malloc_table_t* sFuncs = nullptr;
static intptr_t sFd = 0;
static bool sStdoutOrStderr = false;

static Lock sLock;

static void
prefork() {
  sLock.Acquire();
}

static void
postfork() {
  sLock.Release();
}

#ifdef ANDROID
/* See mozglue/android/APKOpen.cpp */
extern "C" MOZ_EXPORT __attribute__((weak))
void* __dso_handle;

/* Android doesn't have pthread_atfork defined in pthread.h */
extern "C" MOZ_EXPORT
int pthread_atfork(void (*)(void), void (*)(void), void (*)(void));
#endif

class LogAllocBridge : public ReplaceMallocBridge
{
  virtual void InitDebugFd(mozilla::DebugFdRegistry& aRegistry) override {
    if (!sStdoutOrStderr) {
      aRegistry.RegisterHandle(sFd);
    }
  }
};

void
replace_init(const malloc_table_t* aTable)
{
  sFuncs = aTable;

#ifndef _WIN32
  /* When another thread has acquired a lock before forking, the child
   * process will inherit the lock state but the thread, being nonexistent
   * in the child process, will never release it, leading to a dead-lock
   * whenever the child process gets the lock. We thus need to ensure no
   * other thread is holding the lock before forking, by acquiring it
   * ourselves, and releasing it after forking, both in the parent and child
   * processes.
   * Windows doesn't have this problem since there is no fork(). */
  pthread_atfork(prefork, postfork, postfork);
#endif

  /* Initialize output file descriptor from the MALLOC_LOG environment
   * variable. Numbers up to 9999 are considered as a preopened file
   * descriptor number. Other values are considered as a file name. */
  char* log = getenv("MALLOC_LOG");
  if (log && *log) {
    int fd = 0;
    const char *fd_num = log;
    while (*fd_num) {
      /* Reject non digits. */
      if (*fd_num < '0' || *fd_num > '9') {
        fd = -1;
        break;
      }
      fd = fd * 10 + (*fd_num - '0');
      /* Reject values >= 10000. */
      if (fd >= 10000) {
        fd = -1;
        break;
      }
      fd_num++;
    }
    if (fd == 1 || fd == 2) {
      sStdoutOrStderr = true;
    }
#ifdef _WIN32
    // See comment in FdPrintf.h as to why CreateFile is used.
    HANDLE handle;
    if (fd > 0) {
      handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    } else {
      handle = CreateFileA(log, FILE_APPEND_DATA, FILE_SHARE_READ |
                           FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    if (handle != INVALID_HANDLE_VALUE) {
      sFd = reinterpret_cast<intptr_t>(handle);
    }
#else
    if (fd == -1) {
      fd = open(log, O_WRONLY | O_CREAT | O_APPEND, 0644);
    }
    if (fd > 0) {
      sFd = fd;
    }
#endif
  }
}

ReplaceMallocBridge*
replace_get_bridge()
{
  static LogAllocBridge bridge;
  return &bridge;
}

/* Do a simple, text-form, log of all calls to replace-malloc functions.
 * Use locking to guarantee that an allocation that did happen is logged
 * before any other allocation/free happens.
 * TODO: Add a thread id to the log: different allocators, or even different
 * configurations of jemalloc behave differently when allocations are coming
 * from different threads. Reproducing those multi-threaded workloads would be
 * useful to test those differences.
 */

void*
replace_malloc(size_t aSize)
{
  AutoLock lock(sLock);
  void* ptr = sFuncs->malloc(aSize);
  if (ptr) {
    FdPrintf(sFd, "%zu malloc(%zu)=%p\n", size_t(getpid()), aSize, ptr);
  }
  return ptr;
}

int
replace_posix_memalign(void** aPtr, size_t aAlignment, size_t aSize)
{
  AutoLock lock(sLock);
  int ret = sFuncs->posix_memalign(aPtr, aAlignment, aSize);
  if (ret == 0) {
    FdPrintf(sFd, "%zu posix_memalign(%zu,%zu)=%p\n", size_t(getpid()),
             aAlignment, aSize, *aPtr);
  }
  return ret;
}

void*
replace_aligned_alloc(size_t aAlignment, size_t aSize)
{
  AutoLock lock(sLock);
  void* ptr = sFuncs->aligned_alloc(aAlignment, aSize);
  if (ptr) {
    FdPrintf(sFd, "%zu aligned_alloc(%zu,%zu)=%p\n", size_t(getpid()),
             aAlignment, aSize, ptr);
  }
  return ptr;
}

void*
replace_calloc(size_t aNum, size_t aSize)
{
  AutoLock lock(sLock);
  void* ptr = sFuncs->calloc(aNum, aSize);
  if (ptr) {
    FdPrintf(sFd, "%zu calloc(%zu,%zu)=%p\n", size_t(getpid()), aNum, aSize, ptr);
  }
  return ptr;
}

void*
replace_realloc(void* aPtr, size_t aSize)
{
  AutoLock lock(sLock);
  void* new_ptr = sFuncs->realloc(aPtr, aSize);
  if (new_ptr || !aSize) {
    FdPrintf(sFd, "%zu realloc(%p,%zu)=%p\n", size_t(getpid()), aPtr, aSize,
             new_ptr);
  }
  return new_ptr;
}

void
replace_free(void* aPtr)
{
  AutoLock lock(sLock);
  if (aPtr) {
    FdPrintf(sFd, "%zu free(%p)\n", size_t(getpid()), aPtr);
  }
  sFuncs->free(aPtr);
}

void*
replace_memalign(size_t aAlignment, size_t aSize)
{
  AutoLock lock(sLock);
  void* ptr = sFuncs->memalign(aAlignment, aSize);
  if (ptr) {
    FdPrintf(sFd, "%zu memalign(%zu,%zu)=%p\n", size_t(getpid()), aAlignment,
             aSize, ptr);
  }
  return ptr;
}

void*
replace_valloc(size_t aSize)
{
  AutoLock lock(sLock);
  void* ptr = sFuncs->valloc(aSize);
  if (ptr) {
    FdPrintf(sFd, "%zu valloc(%zu)=%p\n", size_t(getpid()), aSize, ptr);
  }
  return ptr;
}

void
replace_jemalloc_stats(jemalloc_stats_t* aStats)
{
  AutoLock lock(sLock);
  sFuncs->jemalloc_stats(aStats);
  FdPrintf(sFd, "%zu jemalloc_stats()\n", size_t(getpid()));
}
