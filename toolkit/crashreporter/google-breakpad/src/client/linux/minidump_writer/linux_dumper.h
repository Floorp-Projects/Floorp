// Copyright (c) 2009, Google Inc.
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

#ifndef CLIENT_LINUX_MINIDUMP_WRITER_LINUX_DUMPER_H_
#define CLIENT_LINUX_MINIDUMP_WRITER_LINUX_DUMPER_H_

#include <elf.h>
#include <linux/limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/user.h>

#include "common/memory.h"
#include "google_breakpad/common/minidump_format.h"

namespace google_breakpad {

typedef typeof(((struct user*) 0)->u_debugreg[0]) debugreg_t;

// Typedef for our parsing of the auxv variables in /proc/pid/auxv.
#if defined(__i386) || defined(__ARM_EABI__)
typedef Elf32_auxv_t elf_aux_entry;
#elif defined(__x86_64__)
typedef Elf64_auxv_t elf_aux_entry;
#endif
// When we find the VDSO mapping in the process's address space, this
// is the name we use for it when writing it to the minidump.
// This should always be less than NAME_MAX!
const char kLinuxGateLibraryName[] = "linux-gate.so";

// We produce one of these structures for each thread in the crashed process.
struct ThreadInfo {
  pid_t tid;    // thread id
  pid_t tgid;   // thread group id
  pid_t ppid;   // parent process

  // Even on platforms where the stack grows down, the following will point to
  // the smallest address in the stack.
  const void* stack;  // pointer to the stack area
  size_t stack_len;  // length of the stack to copy


#if defined(__i386) || defined(__x86_64)
  user_regs_struct regs;
  user_fpregs_struct fpregs;
  static const unsigned kNumDebugRegisters = 8;
  debugreg_t dregs[8];
#if defined(__i386)
  user_fpxregs_struct fpxregs;
#endif  // defined(__i386)

#elif defined(__ARM_EABI__)
  // Mimicking how strace does this(see syscall.c, search for GETREGS)
  struct user_regs regs;
  struct user_fpregs fpregs;
#endif
};

// One of these is produced for each mapping in the process (i.e. line in
// /proc/$x/maps).
struct MappingInfo {
  uintptr_t start_addr;
  size_t size;
  size_t offset;  // offset into the backed file.
  char name[NAME_MAX];
};

// Suspend a thread by attaching to it.
bool AttachThread(pid_t pid);

// Resume a thread by detaching from it.
bool DetachThread(pid_t pid);

// Fill |info| with the register state of |info->tid|.  The thread
// must be attached to the calling process.  Return true on success.
bool GetThreadRegisters(ThreadInfo* info);

class LinuxDumper {
 public:
  explicit LinuxDumper(pid_t pid);

  // Parse the data for |threads| and |mappings|.
  bool Init();

  // Attach/detach all threads in the given process.
  bool ThreadsAttach();
  bool ThreadsDetach();

  // Read information about the given thread. Returns true on success. One must
  // have called |ThreadsAttach| first.
  bool ThreadInfoGet(ThreadInfo* info);

  // These are only valid after a call to |Init|.
  const wasteful_vector<pid_t> &threads() { return threads_; }
  const wasteful_vector<MappingInfo*> &mappings() { return mappings_; }
  const MappingInfo* FindMapping(const void* address) const;

  // Find a block of memory to take as the stack given the top of stack pointer.
  //   stack: (output) the lowest address in the memory area
  //   stack_len: (output) the length of the memory area
  //   stack_top: the current top of the stack
  bool GetStackInfo(const void** stack, size_t* stack_len, uintptr_t stack_top);

  PageAllocator* allocator() { return &allocator_; }

  // memcpy from a remote process.
  static void CopyFromProcess(void* dest, pid_t child, const void* src,
                              size_t length);

  // Builds a proc path for a certain pid for a node.  path is a
  // character array that is overwritten, and node is the final node
  // without any slashes.
  void BuildProcPath(char* path, pid_t pid, const char* node) const;

  // Generate a File ID from the .text section of a mapped entry
  bool ElfFileIdentifierForMapping(unsigned int mapping_id,
                                   uint8_t identifier[sizeof(MDGUID)]);

  // Utility method to find the location of where the kernel has
  // mapped linux-gate.so in memory(shows up in /proc/pid/maps as
  // [vdso], but we can't guarantee that it's the only virtual dynamic
  // shared object.  Parsing the auxilary vector for AT_SYSINFO_EHDR
  // is the safest way to go.)
  void* FindBeginningOfLinuxGateSharedLibrary(const pid_t pid) const;
 private:
  bool EnumerateMappings(wasteful_vector<MappingInfo*>* result) const;
  bool EnumerateThreads(wasteful_vector<pid_t>* result) const;

  const pid_t pid_;

  mutable PageAllocator allocator_;

  bool threads_suspended_;
  wasteful_vector<pid_t> threads_;  // the ids of all the threads
  wasteful_vector<MappingInfo*> mappings_;  // info from /proc/<pid>/maps
};

}  // namespace google_breakpad

#endif  // CLIENT_LINUX_HANDLER_LINUX_DUMPER_H_
