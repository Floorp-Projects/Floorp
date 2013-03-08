/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozPoisonWrite.h"
#include "mozPoisonWriteBase.h"
#include "mozilla/ProcessedStack.h"
#include "mozilla/Scoped.h"
#include "mozilla/SHA1.h"
#include "mozilla/Telemetry.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsStackWalk.h"
#include "nsPrintfCString.h"
#include "plstr.h"
#include <algorithm>
#ifdef XP_WIN
#define NS_T(str) L ## str
#define NS_SLASH "\\"
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#else
#define NS_SLASH "/"
#endif
using namespace mozilla;

namespace {
struct DebugFilesAutoLockTraits {
  typedef PRLock *type;
  const static type empty() {
    return nullptr;
  }
  const static void release(type aL) {
    PR_Unlock(aL);
  }
};

class DebugFilesAutoLock : public Scoped<DebugFilesAutoLockTraits> {
  static PRLock *Lock;
public:
  static void Clear();
  static PRLock *getDebugFileIDsLock() {
    // On windows this static is not thread safe, but we know that the first
    // call is from
    // * An early registration of a debug FD or
    // * The call to InitWritePoisoning.
    // Since the early debug FDs are logs created early in the main thread
    // and no writes are trapped before InitWritePoisoning, we are safe.
    static bool Initialized = false;
    if (!Initialized) {
      Lock = PR_NewLock();
      Initialized = true;
    }

    // We have to use something lower level than a mutex. If we don't, we
    // can get recursive in here when called from logging a call to free.
    return Lock;
  }

  DebugFilesAutoLock() :
    Scoped<DebugFilesAutoLockTraits>(getDebugFileIDsLock()) {
    PR_Lock(get());
  }
};

PRLock *DebugFilesAutoLock::Lock;
void DebugFilesAutoLock::Clear() {
  MOZ_ASSERT(Lock != nullptr);
  Lock = nullptr;
}

static char *sProfileDirectory = NULL;

// Return a vector used to hold the IDs of the current debug files. On unix
// an ID is a file descriptor. On Windows it is a file HANDLE.
std::vector<intptr_t>* getDebugFileIDs() {
  PRLock *lock = DebugFilesAutoLock::getDebugFileIDsLock();
  PR_ASSERT_CURRENT_THREAD_OWNS_LOCK(lock);
  // We have to use new as some write happen during static destructors
  // so an static std::vector might be destroyed while we still need it.
  static std::vector<intptr_t> *DebugFileIDs = new std::vector<intptr_t>();
  return DebugFileIDs;
}

// This a wrapper over a file descriptor that provides a Printf method and
// computes the sha1 of the data that passes through it.
class SHA1Stream
{
public:
    explicit SHA1Stream(FILE *stream)
      : mFile(stream)
    {
      MozillaRegisterDebugFILE(mFile);
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

static void RecordStackWalker(void *aPC, void *aSP, void *aClosure)
{
    std::vector<uintptr_t> *stack =
        static_cast<std::vector<uintptr_t>*>(aClosure);
    stack->push_back(reinterpret_cast<uintptr_t>(aPC));
}


enum PoisonState {
  POISON_UNINITIALIZED = 0,
  POISON_ON,
  POISON_OFF
};

// POISON_OFF has two consequences
// * It prevents PoisonWrite from patching the write functions.
// * If the patching has already been done, it prevents AbortOnBadWrite from
//   asserting. Note that not all writes use AbortOnBadWrite at this point
//   (aio_write for example), so disabling writes after patching doesn't
//   completely undo it.
PoisonState sPoisoningState = POISON_UNINITIALIZED;
}

namespace mozilla {

void InitWritePoisoning()
{
  // Stdout and Stderr are OK.
  MozillaRegisterDebugFD(1);
  MozillaRegisterDebugFD(2);

  nsCOMPtr<nsIFile> mozFile;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mozFile));
  if (mozFile) {
    nsAutoCString nativePath;
    nsresult rv = mozFile->GetNativePath(nativePath);
    if (NS_SUCCEEDED(rv)) {
      sProfileDirectory = PL_strdup(nativePath.get());
    }
  }
}

bool ValidWriteAssert(bool ok)
{
    if (gShutdownChecks == SCM_CRASH && !ok) {
        MOZ_CRASH();
    }

    // We normally don't poison writes if gShutdownChecks is SCM_NOTHING, but
    // write poisoning can get more users in the future (profiling for example),
    // so make sure we behave correctly.
    if (gShutdownChecks == SCM_NOTHING || ok || !sProfileDirectory ||
        !Telemetry::CanRecord()) {
        return ok;
    }

    // Write the stack and loaded libraries to a file. We can get here
    // concurrently from many writes, so we use multiple temporary files.
    std::vector<uintptr_t> rawStack;

    NS_StackWalk(RecordStackWalker, /* skipFrames */ 0, /* maxFrames */ 0,
                 reinterpret_cast<void*>(&rawStack), 0, nullptr);
    Telemetry::ProcessedStack stack = Telemetry::GetStackAndModules(rawStack);

    nsPrintfCString nameAux("%s%s%s", sProfileDirectory,
                            NS_SLASH, "Telemetry.LateWriteTmpXXXXXX");
    char *name;
    nameAux.GetMutableData(&name);

    // We want the sha1 of the entire file, so please don't write to fd
    // directly; use sha1Stream.
    FILE *stream;
#ifdef XP_WIN
    HANDLE hFile;
    do {
      // mkstemp isn't supported so keep trying until we get a file
      int result = _mktemp_s(name, strlen(name) + 1);
      hFile = CreateFileA(name, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    } while (GetLastError() == ERROR_FILE_EXISTS);

    if (hFile == INVALID_HANDLE_VALUE) {
      NS_RUNTIMEABORT("Um, how did we get here?");
    }

    // http://support.microsoft.com/kb/139640
    int fd = _open_osfhandle((intptr_t)hFile, _O_APPEND);
    if (fd == -1) {
      NS_RUNTIMEABORT("Um, how did we get here?");
    }

    stream = _fdopen(fd, "w");
#else
    int fd = mkstemp(name);
    stream = fdopen(fd, "w");
#endif

    SHA1Stream sha1Stream(stream);

    size_t numModules = stack.GetNumModules();
    sha1Stream.Printf("%u\n", (unsigned)numModules);
    for (size_t i = 0; i < numModules; ++i) {
        Telemetry::ProcessedStack::Module module = stack.GetModule(i);
        sha1Stream.Printf("%s %s\n", module.mBreakpadId.c_str(),
                          module.mName.c_str());
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

void DisableWritePoisoning() {
  if (sPoisoningState != POISON_ON)
    return;

  sPoisoningState = POISON_OFF;
  PL_strfree(sProfileDirectory);
  sProfileDirectory = nullptr;

  PRLock *Lock;
  {
    DebugFilesAutoLock lockedScope;
    delete getDebugFileIDs();
    Lock = DebugFilesAutoLock::getDebugFileIDsLock();
    DebugFilesAutoLock::Clear();
  }
  PR_DestroyLock(Lock);
}
void EnableWritePoisoning() {
  sPoisoningState = POISON_ON;
}

bool PoisonWriteEnabled()
{
  return sPoisoningState == POISON_ON;
}

bool IsDebugFile(intptr_t aFileID) {
  DebugFilesAutoLock lockedScope;

  std::vector<intptr_t> &Vec = *getDebugFileIDs();
  return std::find(Vec.begin(), Vec.end(), aFileID) != Vec.end();
}

} // mozilla

extern "C" {
  void MozillaRegisterDebugFD(int fd) {
    if (sPoisoningState == POISON_OFF)
      return;
    DebugFilesAutoLock lockedScope;
    intptr_t fileId = FileDescriptorToID(fd);
    std::vector<intptr_t> &Vec = *getDebugFileIDs();
    MOZ_ASSERT(std::find(Vec.begin(), Vec.end(), fileId) == Vec.end());
    Vec.push_back(fileId);
  }
  void MozillaRegisterDebugFILE(FILE *f) {
    if (sPoisoningState == POISON_OFF)
      return;
    int fd = fileno(f);
    if (fd == 1 || fd == 2)
      return;
    MozillaRegisterDebugFD(fd);
  }
  void MozillaUnRegisterDebugFD(int fd) {
    if (sPoisoningState == POISON_OFF)
      return;
    DebugFilesAutoLock lockedScope;
    intptr_t fileId = FileDescriptorToID(fd);
    std::vector<intptr_t> &Vec = *getDebugFileIDs();
    std::vector<intptr_t>::iterator i =
      std::find(Vec.begin(), Vec.end(), fileId);
    MOZ_ASSERT(i != Vec.end());
    Vec.erase(i);
  }
  void MozillaUnRegisterDebugFILE(FILE *f) {
    if (sPoisoningState == POISON_OFF)
      return;
    int fd = fileno(f);
    if (fd == 1 || fd == 2)
      return;
    fflush(f);
    MozillaUnRegisterDebugFD(fd);
  }
}
