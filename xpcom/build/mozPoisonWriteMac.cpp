/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozPoisonWrite.h"
#include "mozPoisonWriteBase.h"
#include "mozilla/Util.h"
#include "nsTraceRefcntImpl.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Scoped.h"
#include "mozilla/Mutex.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ProcessedStack.h"
#include "nsStackWalk.h"
#include "nsPrintfCString.h"
#include "mach_override.h"
#include "prio.h"
#include "plstr.h"
#include "nsCOMPtr.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "mozilla/SHA1.h"
#include <sys/stat.h>
#include <sys/socket.h>
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

// Wrap aio_write. We have not seen it before, so just assert/report it.
typedef ssize_t (*aio_write_t)(struct aiocb *aiocbp);
ssize_t wrap_aio_write(struct aiocb *aiocbp);
FuncData aio_write_data = { 0, (void*) wrap_aio_write, (void*) aio_write };
ssize_t wrap_aio_write(struct aiocb *aiocbp) {
    ValidWriteAssert(0);
    aio_write_t old_write = (aio_write_t) aio_write_data.Buffer;
    return old_write(aiocbp);
}

// Wrap pwrite-like functions.
// We have not seen them before, so just assert/report it.
typedef ssize_t (*pwrite_t)(int fd, const void *buf, size_t nbyte, off_t offset);
template<FuncData &foo>
ssize_t wrap_pwrite_temp(int fd, const void *buf, size_t nbyte, off_t offset) {
    ValidWriteAssert(0);
    pwrite_t old_write = (pwrite_t) foo.Buffer;
    return old_write(fd, buf, nbyte, offset);
}

// Define a FuncData for a pwrite-like functions.
#define DEFINE_PWRITE_DATA(X, NAME)                                        \
FuncData X ## _data = { NAME, (void*) wrap_pwrite_temp<X ## _data> };      \

// This exists everywhere.
DEFINE_PWRITE_DATA(pwrite, "pwrite")
// These exist on 32 bit OS X
DEFINE_PWRITE_DATA(pwrite_NOCANCEL_UNIX2003, "pwrite$NOCANCEL$UNIX2003");
DEFINE_PWRITE_DATA(pwrite_UNIX2003, "pwrite$UNIX2003");
// This exists on 64 bit OS X
DEFINE_PWRITE_DATA(pwrite_NOCANCEL, "pwrite$NOCANCEL");

void AbortOnBadWrite(int fd, const void *wbuf, size_t count);

typedef ssize_t (*writev_t)(int fd, const struct iovec *iov, int iovcnt);
template<FuncData &foo>
ssize_t wrap_writev_temp(int fd, const struct iovec *iov, int iovcnt) {
    AbortOnBadWrite(fd, 0, iovcnt);
    writev_t old_write = (writev_t) foo.Buffer;
    return old_write(fd, iov, iovcnt);
}

// Define a FuncData for a writev-like functions.
#define DEFINE_WRITEV_DATA(X, NAME)                                   \
FuncData X ## _data = { NAME, (void*) wrap_writev_temp<X ## _data> }; \

// This exists everywhere.
DEFINE_WRITEV_DATA(writev, "writev");
// These exist on 32 bit OS X
DEFINE_WRITEV_DATA(writev_NOCANCEL_UNIX2003, "writev$NOCANCEL$UNIX2003");
DEFINE_WRITEV_DATA(writev_UNIX2003, "writev$UNIX2003");
// This exists on 64 bit OS X
DEFINE_WRITEV_DATA(writev_NOCANCEL, "writev$NOCANCEL");

typedef ssize_t (*write_t)(int fd, const void *buf, size_t count);
template<FuncData &foo>
ssize_t wrap_write_temp(int fd, const void *buf, size_t count) {
    AbortOnBadWrite(fd, buf, count);
    write_t old_write = (write_t) foo.Buffer;
    return old_write(fd, buf, count);
}

// Define a FuncData for a write-like functions.
#define DEFINE_WRITE_DATA(X, NAME)                                   \
FuncData X ## _data = { NAME, (void*) wrap_write_temp<X ## _data> }; \

// This exists everywhere.
DEFINE_WRITE_DATA(write, "write");
// These exist on 32 bit OS X
DEFINE_WRITE_DATA(write_NOCANCEL_UNIX2003, "write$NOCANCEL$UNIX2003");
DEFINE_WRITE_DATA(write_UNIX2003, "write$UNIX2003");
// This exists on 64 bit OS X
DEFINE_WRITE_DATA(write_NOCANCEL, "write$NOCANCEL");

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

// We want to detect "actual" writes, not IPC. Some IPC mechanisms are
// implemented with file descriptors, so filter them out.
bool IsIPCWrite(int fd, const struct stat &buf) {
    if ((buf.st_mode & S_IFMT) == S_IFIFO) {
        return true;
    }

    if ((buf.st_mode & S_IFMT) != S_IFSOCK) {
        return false;
    }

    sockaddr_storage address;
    socklen_t len = sizeof(address);
    if (getsockname(fd, (sockaddr*) &address, &len) != 0) {
        return true; // Ignore the fd if we can't find what it is.
    }

    return address.ss_family == AF_UNIX;
}

void AbortOnBadWrite(int fd, const void *wbuf, size_t count) {
    if (!PoisonWriteEnabled())
        return;

    // Ignore writes of zero bytes, firefox does some during shutdown.
    if (count == 0)
        return;

    struct stat buf;
    int rv = fstat(fd, &buf);
    if (!ValidWriteAssert(rv == 0))
        return;

    if (IsIPCWrite(fd, buf))
        return;

    // Debugging FDs are OK
    if (IsDebugFile(fd))
        return;

    // For writev we pass NULL in wbuf. We should only get here from
    // dbm, and it uses write, so assert that we have wbuf.
    if (!ValidWriteAssert(wbuf))
        return;

    // As a really bad hack, accept writes that don't change the on disk
    // content. This is needed because dbm doesn't keep track of dirty bits
    // and can end up writing the same data to disk twice. Once when the
    // user (nss) asks it to sync and once when closing the database.
    ScopedFreePtr<void> wbuf2(malloc(count));
    if (!ValidWriteAssert(wbuf2))
        return;
    off_t pos = lseek(fd, 0, SEEK_CUR);
    if (!ValidWriteAssert(pos != -1))
        return;
    ssize_t r = read(fd, wbuf2, count);
    if (!ValidWriteAssert(r == static_cast<ssize_t>(count)))
        return;
    int cmp = memcmp(wbuf, wbuf2, count);
    if (!ValidWriteAssert(cmp == 0))
        return;
    off_t pos2 = lseek(fd, pos, SEEK_SET);
    if (!ValidWriteAssert(pos2 == pos))
        return;
}
} // anonymous namespace

namespace mozilla {

intptr_t FileDescriptorToID(int aFd) {
    return aFd;
}

void PoisonWrite() {
    // Quick sanity check that we don't poison twice.
    static bool WritesArePoisoned = false;
    MOZ_ASSERT(!WritesArePoisoned);
    if (WritesArePoisoned)
        return;
    WritesArePoisoned = true;

    if (!PoisonWriteEnabled())
        return;

    for (int i = 0; i < NumFunctions; ++i) {
        FuncData *d = Functions[i];
        if (!d->Function)
            d->Function = dlsym(RTLD_DEFAULT, d->Name);
        if (!d->Function)
            continue;
        DebugOnly<mach_error_t> t = mach_override_ptr(d->Function, d->Wrapper,
                                           &d->Buffer);
        MOZ_ASSERT(t == err_none);
    }
}
}
