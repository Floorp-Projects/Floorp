/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozPoisonWrite.h"
#include "mozilla/Util.h"
#include "nsTraceRefcntImpl.h"
#include "mozilla/Assertions.h"
#include "mozilla/Scoped.h"
#include "mozilla/Mutex.h"
#include "nsStackWalk.h"
#include "mach_override.h"
#include <sys/stat.h>
#include <vector>
#include <algorithm>
#include <string.h>
#include <sys/uio.h>
#include <aio.h>
#include <dlfcn.h>

namespace {
using namespace mozilla;

struct FuncData {
    const char *Name;      // Name of the function for the ones we use dlsym
    const void *Wrapper;   // The function that we will replace 'Function' with
    void *Function;        // The function that will be replaced with 'Wrapper'
    void *Buffer;          // Will point to the jump buffer that lets us call
                           // 'Function' after it has been replaced.
};

typedef ssize_t (*write_t)(int fd, const void *buf, size_t count);
void AbortOnBadWrite(int fd, const void *wbuf, size_t count);
template<FuncData &foo>
ssize_t wrap_write_temp(int fd, const void *buf, size_t count) {
    AbortOnBadWrite(fd, buf, count);
    write_t old_write = (write_t) foo.Buffer;
    return old_write(fd, buf, count);
}

// FIXME: clang accepts usinging wrap_data_temp<X ## _data> in the struct
// initialization. Is this a gcc 4.2 bug?

// Define a FuncData for a function that can be found without dlsym.
#define DEFINE_F_DATA(X)                                             \
ssize_t wrap_ ## X (int fd, const void *buf, size_t count);          \
FuncData X ## _data = { 0, (void*) wrap_ ## X, (void*) X };          \
ssize_t wrap_ ## X (int fd, const void *buf, size_t count) {         \
    return wrap_write_temp<X ## _data>(fd, buf, count);              \
}

// Define a FuncData for a function that can only be found with dlsym.
#define DEFINE_F_DATA_DYN(X, NAME)                                   \
ssize_t wrap_ ## X (int fd, const void *buf, size_t count);          \
FuncData X ## _data = { NAME, (void*) wrap_ ## X };                  \
ssize_t wrap_ ## X (int fd, const void *buf, size_t count) {         \
    return wrap_write_temp<X ## _data>(fd, buf, count);              \
}

// Define a simple FuncData that just aborts.
#define DEFINE_F_DATA_ABORT(X)                                       \
void wrap_ ## X() { abort(); }                                       \
FuncData X ## _data = { 0, (void*) wrap_ ## X, (void*) X }

// Define a simple FuncData that just aborts for a function that needs dlsym.
#define DEFINE_F_DATA_ABORT_DYN(X, NAME)                             \
void wrap_ ## X() { abort(); }                                       \
FuncData X ## _data = { NAME, (void*) wrap_ ## X }

DEFINE_F_DATA_ABORT(aio_write);
DEFINE_F_DATA_ABORT(pwrite);

// These exist on 32 bit OS X
DEFINE_F_DATA_ABORT_DYN(writev_NOCANCEL_UNIX2003, "writev$NOCANCEL$UNIX2003");
DEFINE_F_DATA_ABORT_DYN(writev_UNIX2003, "writev$UNIX2003");
DEFINE_F_DATA_ABORT_DYN(pwrite_NOCANCEL_UNIX2003, "pwrite$NOCANCEL$UNIX2003");
DEFINE_F_DATA_ABORT_DYN(pwrite_UNIX2003, "pwrite$UNIX2003");

// These exist on 64 bit OS X
DEFINE_F_DATA_ABORT_DYN(writev_NOCANCEL, "writev$NOCANCEL");
DEFINE_F_DATA_ABORT_DYN(pwrite_NOCANCEL, "pwrite$NOCANCEL");

DEFINE_F_DATA(write);

typedef ssize_t (*writev_t)(int fd, const struct iovec *iov, int iovcnt);
ssize_t wrap_writev(int fd, const struct iovec *iov, int iovcnt);
FuncData writev_data = { 0, (void*) wrap_writev, (void*) writev };
ssize_t wrap_writev(int fd, const struct iovec *iov, int iovcnt) {
    AbortOnBadWrite(fd, 0, iovcnt);
    writev_t old_write = (writev_t) writev_data.Buffer;
    return old_write(fd, iov, iovcnt);
}

// These exist on 32 bit OS X
DEFINE_F_DATA_DYN(write_NOCANCEL_UNIX2003, "write$NOCANCEL$UNIX2003");
DEFINE_F_DATA_DYN(write_UNIX2003, "write$UNIX2003");

// These exist on 64 bit OS X
DEFINE_F_DATA_DYN(write_NOCANCEL, "write$NOCANCEL");

FuncData *Functions[] = { &aio_write_data,

                          &pwrite_data,
                          &pwrite_NOCANCEL_UNIX2003_data,
                          &pwrite_UNIX2003_data,
                          &pwrite_NOCANCEL_data,

                          &write_data,
                          &write_NOCANCEL_UNIX2003_data,
                          &write_UNIX2003_data,
                          &write_NOCANCEL_data,

                          &writev_data,
                          &writev_NOCANCEL_UNIX2003_data,
                          &writev_UNIX2003_data,
                          &writev_NOCANCEL_data};

const int NumFunctions = ArrayLength(Functions);

std::vector<int>& getDebugFDs() {
    // We have to use new as some write happen during static destructors
    // so an static std::vector might be destroyed while we still need it.
    static std::vector<int> *DebugFDs = new std::vector<int>();
    return *DebugFDs;
}

struct AutoLockTraits {
    typedef PRLock *type;
    const static type empty() {
      return NULL;
    }
    const static void release(type aL) {
        PR_Unlock(aL);
    }
};

class MyAutoLock : public Scoped<AutoLockTraits> {
public:
    static PRLock *getDebugFDsLock() {
        // We have to use something lower level than a mutex. If we don't, we
        // can get recursive in here when called from logging a call to free.
        static PRLock *Lock = PR_NewLock();
        return Lock;
    }

    MyAutoLock() : Scoped<AutoLockTraits>(getDebugFDsLock()) {
        PR_Lock(get());
    }
};

bool PoisoningDisabled = false;
void AbortOnBadWrite(int fd, const void *wbuf, size_t count) {
    if (PoisoningDisabled)
        return;

    // Ignore writes of zero bytes, firefox does some during shutdown.
    if (count == 0)
        return;

    // Stdout and Stderr are OK.
    if(fd == 1 || fd == 2)
        return;

    struct stat buf;
    int rv = fstat(fd, &buf);
    MOZ_ASSERT(rv == 0);

    // FIFOs are used for thread communication during shutdown.
    if ((buf.st_mode & S_IFMT) == S_IFIFO)
        return;

    MyAutoLock lockedScope;

    // Debugging FDs are OK
    std::vector<int> &Vec = getDebugFDs();
    if (std::find(Vec.begin(), Vec.end(), fd) != Vec.end())
        return;

    // For writev we pass NULL in wbuf. We should only get here from
    // dbm, and it uses write, so assert that we have wbuf.
    MOZ_ASSERT(wbuf);

    // As a really bad hack, accept writes that don't change the on disk
    // content. This is needed because dbm doesn't keep track of dirty bits
    // and can end up writing the same data to disk twice. Once when the
    // user (nss) asks it to sync and once when closing the database.
    void *wbuf2 = malloc(count);
    MOZ_ASSERT(wbuf2);
    off_t pos = lseek(fd, 0, SEEK_CUR);
    MOZ_ASSERT(pos != -1);
    ssize_t r = read(fd, wbuf2, count);
    MOZ_ASSERT(r == count);
    int cmp = memcmp(wbuf, wbuf2, count);
    MOZ_ASSERT(cmp == 0);
    free(wbuf2);
    off_t pos2 = lseek(fd, pos, SEEK_SET);
    MOZ_ASSERT(pos2 == pos);
}

// We cannot use destructors to free the lock and the list of debug fds since
// we don't control the order the destructors are called. Instead, we use
// libc funcion __cleanup callback which runs after the destructors.
void (*OldCleanup)();
extern "C" void (*__cleanup)();
void FinalCleanup() {
    if (OldCleanup)
        OldCleanup();
    delete &getDebugFDs();
    PR_DestroyLock(MyAutoLock::getDebugFDsLock());
}

} // anonymous namespace

extern "C" {
    void MozillaRegisterDebugFD(int fd) {
        MyAutoLock lockedScope;
        std::vector<int> &Vec = getDebugFDs();
        MOZ_ASSERT(std::find(Vec.begin(), Vec.end(), fd) == Vec.end());
        Vec.push_back(fd);
    }
    void MozillaUnRegisterDebugFD(int fd) {
        MyAutoLock lockedScope;
        std::vector<int> &Vec = getDebugFDs();
        std::vector<int>::iterator i = std::find(Vec.begin(), Vec.end(), fd);
        MOZ_ASSERT(i != Vec.end());
        Vec.erase(i);
    }
}

namespace mozilla {
void PoisonWrite() {
    // For now only poison writes in debug builds.
#ifndef DEBUG
    return;
#endif

    PoisoningDisabled = false;
    OldCleanup = __cleanup;
    __cleanup = FinalCleanup;

    for (int i = 0; i < NumFunctions; ++i) {
        FuncData *d = Functions[i];
        if (!d->Function)
            d->Function = dlsym(RTLD_DEFAULT, d->Name);
        if (!d->Function)
            continue;
        mach_error_t t = mach_override_ptr(d->Function, d->Wrapper,
                                           &d->Buffer);
        MOZ_ASSERT(t == err_none);
    }
}
void DisableWritePoisoning() {
    PoisoningDisabled = true;
}
}
