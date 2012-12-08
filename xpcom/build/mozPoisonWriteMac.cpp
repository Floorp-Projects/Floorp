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

// This a wrapper over a file descriptor that provides a Printf method and
// computes the sha1 of the data that passes through it.
class SHA1Stream
{
public:
    SHA1Stream(int aFd) {
        MozillaRegisterDebugFD(aFd);
        mFile = fdopen(aFd, "w");
    }
    void Printf(const char *aFormat, ...)
    {
        MOZ_ASSERT(mFile);
        va_list list;
        va_start(list, aFormat);
        nsAutoCString str;
        str.AppendPrintf(aFormat, list);
        va_end(list);
        mSHA1.update(str.get(), str.Length());
        fwrite(str.get(), 1, str.Length(), mFile);
    }
    void Finish(SHA1Sum::Hash &aHash)
    {
        int fd = fileno(mFile);
        fflush(mFile);
        MozillaUnRegisterDebugFD(fd);
        fclose(mFile);
        mSHA1.finish(aHash);
        mFile = NULL;
    }
private:
    FILE *mFile;
    SHA1Sum mSHA1;
};

void RecordStackWalker(void *aPC, void *aSP, void *aClosure)
{
    std::vector<uintptr_t> *stack =
        static_cast<std::vector<uintptr_t>*>(aClosure);
    stack->push_back(reinterpret_cast<uintptr_t>(aPC));
}

char *sProfileDirectory = NULL;

bool ValidWriteAssert(bool ok)
{
    // On a debug build, just crash.
    MOZ_ASSERT(ok);

    if (ok || !sProfileDirectory || !Telemetry::CanRecord())
        return ok;

    // Write the stack and loaded libraries to a file. We can get here
    // concurrently from many writes, so we use multiple temporary files.
    std::vector<uintptr_t> rawStack;

    NS_StackWalk(RecordStackWalker, 0, reinterpret_cast<void*>(&rawStack), 0, nullptr);
    Telemetry::ProcessedStack stack = Telemetry::GetStackAndModules(rawStack);

    nsPrintfCString nameAux("%s%s", sProfileDirectory,
                            "/Telemetry.LateWriteTmpXXXXXX");
    char *name;
    nameAux.GetMutableData(&name);

    // We want the sha1 of the entire file, so please don't write to fd
    // directly; use sha1Stream.
    int fd = mkstemp(name);
    SHA1Stream sha1Stream(fd);
    fd = 0;

    size_t numModules = stack.GetNumModules();
    sha1Stream.Printf("%u\n", (unsigned)numModules);
    for (int i = 0; i < numModules; ++i) {
        Telemetry::ProcessedStack::Module module = stack.GetModule(i);
        sha1Stream.Printf("%s\n", module.mName.c_str());
    }

    size_t numFrames = stack.GetStackSize();
    sha1Stream.Printf("%u\n", (unsigned)numFrames);
    for (size_t i = 0; i < numFrames; ++i) {
        const Telemetry::ProcessedStack::Frame &frame =
            stack.GetFrame(i);
        // NOTE: We write the offsets, while the atos tool expects a value with
        // the virtual address added. For example, running otool -l on the the firefox
        // binary shows
        //      cmd LC_SEGMENT_64
        //      cmdsize 632
        //      segname __TEXT
        //      vmaddr 0x0000000100000000
        // so to print the line matching the offset 123 one has to run
        // atos -o firefox 0x100000123.
        sha1Stream.Printf("%d %x\n", frame.mModIndex, (unsigned)frame.mOffset);
    }

    SHA1Sum::Hash sha1;
    sha1Stream.Finish(sha1);

    // Note: These files should be deleted by telemetry once it reads them. If
    // there were no telemery runs by the time we shut down, we just add files
    // to the existing ones instead of replacing them. Given that each of these
    // files is a bug to be fixed, that is probably the right thing to do.

    // We append the sha1 of the contents to the file name. This provides a simple
    // client side deduplication.
    nsPrintfCString finalName("%s%s", sProfileDirectory, "/Telemetry.LateWriteFinal-");
    for (int i = 0; i < 20; ++i) {
        finalName.AppendPrintf("%02x", sha1[i]);
    }
    PR_Delete(finalName.get());
    PR_Rename(name, finalName.get());
    return false;
}

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

// This variable being true has two consequences
// * It prevents PoisonWrite from patching the write functions.
// * If the patching has already been done, it prevents AbortOnBadWrite from
//   asserting. Note that not all writes use AbortOnBadWrite at this point
//   (aio_write for example), so disabling writes after patching doesn't
//   completely undo it.
bool PoisoningDisabled = true;

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
    if (!ValidWriteAssert(rv == 0))
        return;

    // FIFOs are used for thread communication during shutdown.
    if ((buf.st_mode & S_IFMT) == S_IFIFO)
        return;

    {
        MyAutoLock lockedScope;

        // Debugging FDs are OK
        std::vector<int> &Vec = getDebugFDs();
        if (std::find(Vec.begin(), Vec.end(), fd) != Vec.end())
            return;
    }

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
    if (!ValidWriteAssert(r == count))
        return;
    int cmp = memcmp(wbuf, wbuf2, count);
    if (!ValidWriteAssert(cmp == 0))
        return;
    off_t pos2 = lseek(fd, pos, SEEK_SET);
    if (!ValidWriteAssert(pos2 == pos))
        return;
}

// We cannot use destructors to free the lock and the list of debug fds since
// we don't control the order the destructors are called. Instead, we use
// libc funcion __cleanup callback which runs after the destructors.
void (*OldCleanup)();
extern "C" void (*__cleanup)();
void FinalCleanup() {
    if (OldCleanup)
        OldCleanup();
    if (sProfileDirectory)
        PL_strfree(sProfileDirectory);
    sProfileDirectory = nullptr;
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
    void MozillaUnRegisterDebugFILE(FILE *f) {
        int fd = fileno(f);
        if (fd == 1 || fd == 2)
            return;
        fflush(f);
        MozillaUnRegisterDebugFD(fd);
    }
}

namespace mozilla {
void PoisonWrite() {
    // Quick sanity check that we don't poison twice.
    static bool WritesArePoisoned = false;
    MOZ_ASSERT(!WritesArePoisoned);
    if (WritesArePoisoned)
        return;
    WritesArePoisoned = true;

    if (PoisoningDisabled)
        return;

    nsCOMPtr<nsIFile> mozFile;
    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mozFile));
    if (mozFile) {
        nsAutoCString nativePath;
        nsresult rv = mozFile->GetNativePath(nativePath);
        if (NS_SUCCEEDED(rv)) {
            sProfileDirectory = PL_strdup(nativePath.get());
        }
    }

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
void EnableWritePoisoning() {
    PoisoningDisabled = false;
}
}
