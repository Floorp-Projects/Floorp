/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#ifdef XP_WIN
#  include <process.h>
#  define getpid _getpid
#else
#  include <signal.h>
#  include <unistd.h>
#endif
#include "mozilla/dom/ScriptSettings.h"  // for AutoJSAPI
#include "mozilla/CodeCoverageHandler.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "nsAppRunner.h"
#include "nsIOutputStream.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "prtime.h"

using namespace mozilla;

// The __gcov_flush function writes the coverage counters to gcda files and then
// resets them to zero. It is defined at
// https://github.com/gcc-mirror/gcc/blob/aad93da1a579b9ae23ede6b9cf8523360f0a08b4/libgcc/libgcov-interface.c.
// __gcov_flush is protected by a mutex in GCC, but not in LLVM, so we are using
// a CrossProcessMutex to protect it.

// We rename __gcov_flush to __custom_llvm_gcov_flush in our build of LLVM for
// Linux, to avoid naming clashes in builds which mix GCC and LLVM. So, when we
// are building with LLVM exclusively, we need to use __custom_llvm_gcov_flush
// instead.
#if !defined(XP_WIN) && defined(__clang__)
#  define __gcov_flush __custom_llvm_gcov_flush
#endif

extern "C" void __gcov_flush();

StaticAutoPtr<CodeCoverageHandler> CodeCoverageHandler::instance;

void CodeCoverageHandler::FlushCounters() {
  printf_stderr("[CodeCoverage] Requested flush for %d.\n", getpid());

  CrossProcessMutexAutoLock lock(*CodeCoverageHandler::Get()->GetMutex());

  __gcov_flush();

  printf_stderr("[CodeCoverage] flush completed.\n");

  const char* outDir = getenv("JS_CODE_COVERAGE_OUTPUT_DIR");
  if (!outDir || *outDir == 0) {
    return;
  }

  dom::AutoJSAPI jsapi;
  jsapi.Init();
  size_t length;
  char* result = js::GetCodeCoverageSummary(jsapi.cx(), &length);
  if (!result) {
    return;
  }

  nsCOMPtr<nsIFile> file;

  nsresult rv = NS_NewNativeLocalFile(nsDependentCString(outDir), false,
                                      getter_AddRefs(file));
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = file->AppendNative(
      nsPrintfCString("%lu-%d.info", PR_Now() / PR_USEC_PER_MSEC, getpid()));

  rv = file->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0666);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), file);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  char* data = result;
  while (length) {
    uint32_t n = 0;
    rv = outputStream->Write(data, length, &n);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    data += n;
    length -= n;
  }

  rv = outputStream->Close();
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  free(result);

  printf_stderr("[CodeCoverage] JS flush completed.\n");
}

void CodeCoverageHandler::FlushCountersSignalHandler(int) { FlushCounters(); }

void CodeCoverageHandler::SetSignalHandlers() {
#ifndef XP_WIN
  printf_stderr("[CodeCoverage] Setting handlers for process %d.\n", getpid());

  struct sigaction dump_sa;
  dump_sa.sa_handler = CodeCoverageHandler::FlushCountersSignalHandler;
  dump_sa.sa_flags = SA_RESTART;
  sigemptyset(&dump_sa.sa_mask);
  DebugOnly<int> r1 = sigaction(SIGUSR1, &dump_sa, nullptr);
  MOZ_ASSERT(r1 == 0, "Failed to install GCOV SIGUSR1 handler");
#endif
}

CodeCoverageHandler::CodeCoverageHandler() : mGcovLock("GcovLock") {
  SetSignalHandlers();
}

CodeCoverageHandler::CodeCoverageHandler(const CrossProcessMutexHandle& aHandle)
    : mGcovLock(aHandle) {
  SetSignalHandlers();
}

void CodeCoverageHandler::Init() {
  MOZ_ASSERT(!instance);
  MOZ_ASSERT(XRE_IsParentProcess());
  instance = new CodeCoverageHandler();
  ClearOnShutdown(&instance);
}

void CodeCoverageHandler::Init(const CrossProcessMutexHandle& aHandle) {
  MOZ_ASSERT(!instance);
  MOZ_ASSERT(!XRE_IsParentProcess());
  instance = new CodeCoverageHandler(aHandle);
  ClearOnShutdown(&instance);
}

CodeCoverageHandler* CodeCoverageHandler::Get() {
  MOZ_ASSERT(instance);
  return instance;
}

CrossProcessMutex* CodeCoverageHandler::GetMutex() { return &mGcovLock; }

CrossProcessMutexHandle CodeCoverageHandler::GetMutexHandle(int aProcId) {
  return mGcovLock.ShareToProcess(aProcId);
}
