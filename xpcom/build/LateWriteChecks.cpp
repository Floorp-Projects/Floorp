/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "mozilla/IOInterposer.h"
#include "mozilla/PoisonIOInterposer.h"
#include "mozilla/ProcessedStack.h"
#include "mozilla/SHA1.h"
#include "mozilla/Scoped.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsPrintfCString.h"
#include "nsStackWalk.h"
#include "plstr.h"

#ifdef XP_WIN
#define NS_T(str) L ## str
#define NS_SLASH "\\"
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <windows.h>
#else
#define NS_SLASH "/"
#endif

#include "LateWriteChecks.h"

#if defined(MOZ_STACKWALKING)
#define OBSERVE_LATE_WRITES
#endif

using namespace mozilla;

/*************************** Auxiliary Declarations ***************************/

// This a wrapper over a file descriptor that provides a Printf method and
// computes the sha1 of the data that passes through it.
class SHA1Stream
{
public:
  explicit SHA1Stream(FILE* aStream)
    : mFile(aStream)
  {
    MozillaRegisterDebugFILE(mFile);
  }

  void Printf(const char* aFormat, ...)
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
  void Finish(SHA1Sum::Hash& aHash)
  {
    int fd = fileno(mFile);
    fflush(mFile);
    MozillaUnRegisterDebugFD(fd);
    fclose(mFile);
    mSHA1.finish(aHash);
    mFile = nullptr;
  }
private:
  FILE* mFile;
  SHA1Sum mSHA1;
};

static void
RecordStackWalker(uint32_t aFrameNumber, void* aPC, void* aSP, void* aClosure)
{
  std::vector<uintptr_t>* stack =
    static_cast<std::vector<uintptr_t>*>(aClosure);
  stack->push_back(reinterpret_cast<uintptr_t>(aPC));
}

/**************************** Late-Write Observer  ****************************/

/**
 * An implementation of IOInterposeObserver to be registered with IOInterposer.
 * This observer logs all writes as late writes.
 */
class LateWriteObserver MOZ_FINAL : public IOInterposeObserver
{
public:
  explicit LateWriteObserver(const char* aProfileDirectory)
    : mProfileDirectory(PL_strdup(aProfileDirectory))
  {
  }
  ~LateWriteObserver()
  {
    PL_strfree(mProfileDirectory);
    mProfileDirectory = nullptr;
  }

  void Observe(IOInterposeObserver::Observation& aObservation);
private:
  char* mProfileDirectory;
};

void
LateWriteObserver::Observe(IOInterposeObserver::Observation& aOb)
{
#ifdef OBSERVE_LATE_WRITES
  // Crash if that is the shutdown check mode
  if (gShutdownChecks == SCM_CRASH) {
    MOZ_CRASH();
  }

  // If we have shutdown mode SCM_NOTHING or we can't record then abort
  if (gShutdownChecks == SCM_NOTHING || !Telemetry::CanRecord()) {
    return;
  }

  // Write the stack and loaded libraries to a file. We can get here
  // concurrently from many writes, so we use multiple temporary files.
  std::vector<uintptr_t> rawStack;

  NS_StackWalk(RecordStackWalker, /* skipFrames */ 0, /* maxFrames */ 0,
               reinterpret_cast<void*>(&rawStack), 0, nullptr);
  Telemetry::ProcessedStack stack = Telemetry::GetStackAndModules(rawStack);

  nsPrintfCString nameAux("%s%s%s", mProfileDirectory,
                          NS_SLASH, "Telemetry.LateWriteTmpXXXXXX");
  char* name;
  nameAux.GetMutableData(&name);

  // We want the sha1 of the entire file, so please don't write to fd
  // directly; use sha1Stream.
  FILE* stream;
#ifdef XP_WIN
  HANDLE hFile;
  do {
    // mkstemp isn't supported so keep trying until we get a file
    int result = _mktemp_s(name, strlen(name) + 1);
    hFile = CreateFileA(name, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
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
    const Telemetry::ProcessedStack::Frame& frame = stack.GetFrame(i);
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
  // there were no telemetry runs by the time we shut down, we just add files
  // to the existing ones instead of replacing them. Given that each of these
  // files is a bug to be fixed, that is probably the right thing to do.

  // We append the sha1 of the contents to the file name. This provides a simple
  // client side deduplication.
  nsPrintfCString finalName("%s%s", mProfileDirectory,
                            "/Telemetry.LateWriteFinal-");
  for (int i = 0; i < 20; ++i) {
    finalName.AppendPrintf("%02x", sha1[i]);
  }
  PR_Delete(finalName.get());
  PR_Rename(name, finalName.get());
#endif
}

/******************************* Setup/Teardown *******************************/

static StaticAutoPtr<LateWriteObserver> sLateWriteObserver;

namespace mozilla {

void
InitLateWriteChecks()
{
  nsCOMPtr<nsIFile> mozFile;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mozFile));
  if (mozFile) {
    nsAutoCString nativePath;
    nsresult rv = mozFile->GetNativePath(nativePath);
    if (NS_SUCCEEDED(rv) && nativePath.get()) {
      sLateWriteObserver = new LateWriteObserver(nativePath.get());
    }
  }
}

void
BeginLateWriteChecks()
{
  if (sLateWriteObserver) {
    IOInterposer::Register(
      IOInterposeObserver::OpWriteFSync,
      sLateWriteObserver
    );
  }
}

void
StopLateWriteChecks()
{
  if (sLateWriteObserver) {
    IOInterposer::Unregister(
      IOInterposeObserver::OpAll,
      sLateWriteObserver
    );
    // Deallocation would not be thread-safe, and StopLateWriteChecks() is
    // called at shutdown and only in special cases.
    // sLateWriteObserver = nullptr;
  }
}

} // namespace mozilla
