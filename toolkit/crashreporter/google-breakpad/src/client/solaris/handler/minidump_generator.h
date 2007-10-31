// Copyright (c) 2007, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: Alfred Peng

#ifndef CLIENT_SOLARIS_HANDLER_MINIDUMP_GENERATOR_H__
#define CLIENT_SOLARIS_HANDLER_MINIDUMP_GENERATOR_H__

#if defined(sparc) || defined(__sparc__)
#define TARGET_CPU_SPARC 1
#elif defined(i386) || defined(__i386__)
#define TARGET_CPU_X86 1
#else
#error "cannot determine cpu type"
#endif

#include "client/minidump_file_writer.h"
#include "client/solaris/handler/solaris_lwp.h"
#include "google_breakpad/common/breakpad_types.h"
#include "google_breakpad/common/minidump_format.h"

namespace google_breakpad {

//
// MinidumpGenerator
//
// A minidump generator should be created before any exception happen.
//
class MinidumpGenerator {
  // Callback run for writing lwp information in the process.
  friend bool LwpInformationCallback(lwpstatus_t *lsp, void *context);

  // Callback run for writing module information in the process.
  friend bool ModuleInfoCallback(const ModuleInfo &module_info, void *context);

 public:
  MinidumpGenerator();

  ~MinidumpGenerator();

  // Write minidump.
  bool WriteMinidumpToFile(const char *file_pathname,
                           int signo);

 private:
  // Helpers
  bool WriteCVRecord(MDRawModule *module, const char *module_path);

  // Write the lwp stack information to dump file.
  bool WriteLwpStack(uintptr_t last_esp, UntypedMDRVA *memory,
                     MDMemoryDescriptor *loc);

  // Write CPU context based on provided registers.
#if TARGET_CPU_SPARC
  bool WriteContext(MDRawContextSPARC *context, prgregset_t regs,
                    prfpregset_t *fp_regs);
#elif TARGET_CPU_X86
  bool WriteContext(MDRawContextX86 *context, prgregset_t regs,
                    prfpregset_t *fp_regs);
#endif /* TARGET_CPU_XXX */

  // Write information about a lwp.
  // Only processes lwp running normally at the crash.
  bool WriteLwpStream(lwpstatus_t *lsp, MDRawThread *lwp);

  // Write the CPU information to the dump file.
  bool WriteCPUInformation(MDRawSystemInfo *sys_info);

  //Write the OS information to the dump file.
  bool WriteOSInformation(MDRawSystemInfo *sys_info);

  typedef bool (MinidumpGenerator::*WriteStreamFN)(MDRawDirectory *);

  // Write all the information to the dump file.
  void *Write();

  // Stream writers
  bool WriteLwpListStream(MDRawDirectory *dir);
  bool WriteModuleListStream(MDRawDirectory *dir);
  bool WriteSystemInfoStream(MDRawDirectory *dir);
  bool WriteExceptionStream(MDRawDirectory *dir);
  bool WriteMiscInfoStream(MDRawDirectory *dir);
  bool WriteBreakpadInfoStream(MDRawDirectory *dir);

 private:
  MinidumpFileWriter writer_;

  // Pid of the lwp who called WriteMinidumpToFile
  int requester_pid_;

  // Signal number when crash happed. Can be 0 if this is a requested dump.
  int signo_;

  // Used to get information about the lwps.
  SolarisLwp *lwp_lister_;
};

}  // namespace google_breakpad

#endif   // CLIENT_SOLARIS_HANDLER_MINIDUMP_GENERATOR_H_
