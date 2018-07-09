// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from Chromium's /src/tools/cygprofile_win/cygprofile.cc.

#include <stdio.h>
#include <atomic>
#include <string>
#include <unordered_set>

#include <windows.h>  // Needs to be included before the others.

#include <dbghelp.h>
#include <process.h>

#include "mozilla/Sprintf.h"
#include "mozilla/Types.h"

namespace {

// The main purpose of the order file is to optimize startup time,
// so capturing the first N function calls is enough.
static constexpr int kSamplesCapacity = 25 * 1024 * 1024;

void* samples[kSamplesCapacity];
std::atomic_int num_samples;
std::atomic_int done;


// Symbolize the samples and write them to disk.
void dump(void*) {
  HMODULE dbghelp = LoadLibraryA("dbghelp.dll");
  auto sym_from_addr = reinterpret_cast<decltype(::SymFromAddr)*>(
      ::GetProcAddress(dbghelp, "SymFromAddr"));
  auto sym_initialize = reinterpret_cast<decltype(::SymInitialize)*>(
      ::GetProcAddress(dbghelp, "SymInitialize"));
  auto sym_set_options = reinterpret_cast<decltype(::SymSetOptions)*>(
      ::GetProcAddress(dbghelp, "SymSetOptions"));

  // Path to the dump file. %s will be substituted by objdir path.
  static const char kDumpFile[] = "%s/cygprofile.txt";

  char filename[MAX_PATH];
  const char* objdir = ::getenv("MOZ_OBJDIR");

  if (!objdir) {
    fprintf(stderr, "ERROR: cannot determine objdir\n");
    return;
  }

  SprintfLiteral(filename, kDumpFile, objdir);

  FILE* f = fopen(filename, "w");
  if (!f) {
    fprintf(stderr, "ERROR: Cannot open %s\n", filename);
    return;
  }

  sym_initialize(::GetCurrentProcess(), NULL, TRUE);
  sym_set_options(SYMOPT_DEFERRED_LOADS | SYMOPT_PUBLICS_ONLY);
  char sym_buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];

  std::unordered_set<void*> seen;
  std::unordered_set<std::string> seen_names;

  for (void* sample : samples) {
    // Only print the first call of a function.
    if (seen.count(sample))
      continue;
    seen.insert(sample);

    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(sym_buf);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;
    DWORD64 offset = 0;

    if (sym_from_addr(::GetCurrentProcess(), reinterpret_cast<DWORD64>(sample),
                      &offset, symbol)) {
      const char* name = symbol->Name;
      if (name[0] == '_')
        name++;
      if (seen_names.count(name))
        continue;
      seen_names.insert(name);

      fprintf(f, "%s\n", name);
    }
  }

  fclose(f);
}

}  // namespace

extern "C" {

MOZ_EXPORT void
__cyg_profile_func_enter(void* this_fn, void* call_site_unused) {
  if (done)
    return;

  // Get our index for the samples array atomically.
  int n = num_samples++;

  if (n < kSamplesCapacity) {
    samples[n] = this_fn;

    if (n + 1 == kSamplesCapacity) {
      // This is the final sample; start dumping the samples to a file (on a
      // separate thread so as not to disturb the main program).
      done = 1;
      _beginthread(dump, 0, nullptr);
    }
  }
}

MOZ_EXPORT void
__cyg_profile_func_exit(void* this_fn, void* call_site) {}

}  // extern "C"
