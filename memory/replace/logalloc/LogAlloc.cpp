/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>
#include <fcntl.h>

#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#  include <process.h>
#else
#  include <unistd.h>
#  include <pthread.h>
#endif

#include "replace_malloc.h"
#include "FdPrintf.h"
#include "Mutex.h"

static malloc_table_t sFuncs;
static intptr_t sFd = 0;
static bool sStdoutOrStderr = false;

static Mutex sMutex MOZ_UNANNOTATED;

#ifndef _WIN32
static void prefork() MOZ_NO_THREAD_SAFETY_ANALYSIS { sMutex.Lock(); }
static void postfork_parent() MOZ_NO_THREAD_SAFETY_ANALYSIS { sMutex.Unlock(); }
static void postfork_child() { sMutex.Init(); }
#endif

static size_t GetPid() { return size_t(getpid()); }

static size_t GetTid() {
#if defined(_WIN32)
  return size_t(GetCurrentThreadId());
#else
  return size_t(pthread_self());
#endif
}

#ifdef ANDROID
/* Android doesn't have pthread_atfork defined in pthread.h */
extern "C" MOZ_EXPORT int pthread_atfork(void (*)(void), void (*)(void),
                                         void (*)(void));
#endif

class LogAllocBridge : public ReplaceMallocBridge {
  virtual void InitDebugFd(mozilla::DebugFdRegistry& aRegistry) override {
    if (!sStdoutOrStderr) {
      aRegistry.RegisterHandle(sFd);
    }
  }
};

/* Do a simple, text-form, log of all calls to replace-malloc functions.
 * Use locking to guarantee that an allocation that did happen is logged
 * before any other allocation/free happens.
 */

static void* replace_malloc(size_t aSize) {
  MutexAutoLock lock(sMutex);
  void* ptr = sFuncs.malloc(aSize);
  FdPrintf(sFd, "%zu %zu malloc(%zu)=%p\n", GetPid(), GetTid(), aSize, ptr);
  return ptr;
}

static int replace_posix_memalign(void** aPtr, size_t aAlignment,
                                  size_t aSize) {
  MutexAutoLock lock(sMutex);
  int ret = sFuncs.posix_memalign(aPtr, aAlignment, aSize);
  FdPrintf(sFd, "%zu %zu posix_memalign(%zu,%zu)=%p\n", GetPid(), GetTid(),
           aAlignment, aSize, (ret == 0) ? *aPtr : nullptr);
  return ret;
}

static void* replace_aligned_alloc(size_t aAlignment, size_t aSize) {
  MutexAutoLock lock(sMutex);
  void* ptr = sFuncs.aligned_alloc(aAlignment, aSize);
  FdPrintf(sFd, "%zu %zu aligned_alloc(%zu,%zu)=%p\n", GetPid(), GetTid(),
           aAlignment, aSize, ptr);
  return ptr;
}

static void* replace_calloc(size_t aNum, size_t aSize) {
  MutexAutoLock lock(sMutex);
  void* ptr = sFuncs.calloc(aNum, aSize);
  FdPrintf(sFd, "%zu %zu calloc(%zu,%zu)=%p\n", GetPid(), GetTid(), aNum, aSize,
           ptr);
  return ptr;
}

static void* replace_realloc(void* aPtr, size_t aSize) {
  MutexAutoLock lock(sMutex);
  void* new_ptr = sFuncs.realloc(aPtr, aSize);
  FdPrintf(sFd, "%zu %zu realloc(%p,%zu)=%p\n", GetPid(), GetTid(), aPtr, aSize,
           new_ptr);
  return new_ptr;
}

static void replace_free(void* aPtr) {
  MutexAutoLock lock(sMutex);
  FdPrintf(sFd, "%zu %zu free(%p)\n", GetPid(), GetTid(), aPtr);
  sFuncs.free(aPtr);
}

static void* replace_memalign(size_t aAlignment, size_t aSize) {
  MutexAutoLock lock(sMutex);
  void* ptr = sFuncs.memalign(aAlignment, aSize);
  FdPrintf(sFd, "%zu %zu memalign(%zu,%zu)=%p\n", GetPid(), GetTid(),
           aAlignment, aSize, ptr);
  return ptr;
}

static void* replace_valloc(size_t aSize) {
  MutexAutoLock lock(sMutex);
  void* ptr = sFuncs.valloc(aSize);
  FdPrintf(sFd, "%zu %zu valloc(%zu)=%p\n", GetPid(), GetTid(), aSize, ptr);
  return ptr;
}

static void replace_jemalloc_stats(jemalloc_stats_t* aStats,
                                   jemalloc_bin_stats_t* aBinStats) {
  MutexAutoLock lock(sMutex);
  sFuncs.jemalloc_stats_internal(aStats, aBinStats);
  FdPrintf(sFd, "%zu %zu jemalloc_stats()\n", GetPid(), GetTid());
}

void replace_init(malloc_table_t* aTable, ReplaceMallocBridge** aBridge) {
  /* Initialize output file descriptor from the MALLOC_LOG environment
   * variable. Numbers up to 9999 are considered as a preopened file
   * descriptor number. Other values are considered as a file name. */
#ifdef _WIN32
  wchar_t* log = _wgetenv(L"MALLOC_LOG");
#else
  char* log = getenv("MALLOC_LOG");
#endif
  if (log && *log) {
    int fd = 0;
    const auto* fd_num = log;
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
      handle =
          CreateFileW(log, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                      nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
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

  // Don't initialize if we weren't passed a valid MALLOC_LOG.
  if (sFd == 0) {
    return;
  }

  sMutex.Init();
  static LogAllocBridge bridge;
  sFuncs = *aTable;
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC_BASE
#define MALLOC_DECL(name, ...) aTable->name = replace_##name;
#include "malloc_decls.h"
  aTable->jemalloc_stats_internal = replace_jemalloc_stats;
  if (!getenv("MALLOC_LOG_MINIMAL")) {
    aTable->posix_memalign = replace_posix_memalign;
    aTable->aligned_alloc = replace_aligned_alloc;
    aTable->valloc = replace_valloc;
  }
  *aBridge = &bridge;

#ifndef _WIN32
  /* When another thread has acquired a lock before forking, the child
   * process will inherit the lock state but the thread, being nonexistent
   * in the child process, will never release it, leading to a dead-lock
   * whenever the child process gets the lock. We thus need to ensure no
   * other thread is holding the lock before forking, by acquiring it
   * ourselves, and releasing it after forking in the parent process and
   * resetting it to its initial state in the child process. The latter is
   * important because some implementations (notably macOS) prevent a lock from
   * being unlocked by a different thread than the one which locked it in the
   * first place.
   * Windows doesn't have this problem since there is no fork().
   * The real allocator, however, might be doing the same thing (jemalloc
   * does). But pthread_atfork `prepare` handlers (first argument) are
   * processed in reverse order they were established. But replace_init
   * runs before the real allocator has had any chance to initialize and
   * call pthread_atfork itself. This leads to its prefork running before
   * ours. This leads to a race condition that can lead to a deadlock like
   * the following:
   *   - thread A forks.
   *   - libc calls real allocator's prefork, so thread A holds the real
   *     allocator lock.
   *   - thread B calls malloc, which calls our replace_malloc.
   *   - consequently, thread B holds our lock.
   *   - thread B then proceeds to call the real allocator's malloc, and
   *     waits for the real allocator's lock, which thread A holds.
   *   - libc calls our prefork, so thread A waits for our lock, which
   *     thread B holds.
   * To avoid this race condition, the real allocator's prefork must be
   * called after ours, which means it needs to be registered before ours.
   * So trick the real allocator into initializing itself without more side
   * effects by calling malloc with a size it can't possibly allocate. */
  sFuncs.malloc(-1);
  pthread_atfork(prefork, postfork_parent, postfork_child);
#endif
}
