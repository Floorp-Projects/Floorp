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

// Converts a minidump file to a core file which gdb can read.
// Large parts lifted from the userspace core dumper:
//   http://code.google.com/p/google-coredumper/
//
// Usage: minidump-2-core 1234.dmp > core

#include <vector>

#include <stdio.h>
#include <string.h>

#include <elf.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/user.h>
#include <sys/mman.h>

#include "google_breakpad/common/minidump_format.h"
#include "google_breakpad/common/minidump_cpu_x86.h"
#include "common/linux/linux_syscall_support.h"
#include "common/linux/minidump_format_linux.h"

#if __WORDSIZE == 64
  #define ELF_CLASS ELFCLASS64
  #define Ehdr      Elf64_Ehdr
  #define Phdr      Elf64_Phdr
  #define Shdr      Elf64_Shdr
  #define Nhdr      Elf64_Nhdr
  #define auxv_t    Elf64_auxv_t
#else
  #define ELF_CLASS ELFCLASS32
  #define Ehdr      Elf32_Ehdr
  #define Phdr      Elf32_Phdr
  #define Shdr      Elf32_Shdr
  #define Nhdr      Elf32_Nhdr
  #define auxv_t    Elf32_auxv_t
#endif


#if defined(__x86_64__)
  #define ELF_ARCH  EM_X86_64
#elif defined(__i386__)
  #define ELF_ARCH  EM_386
#elif defined(__ARM_ARCH_3__)
  #define ELF_ARCH  EM_ARM
#elif defined(__mips__)
  #define ELF_ARCH  EM_MIPS
#endif

static int usage(const char* argv0) {
  fprintf(stderr, "Usage: %s <minidump file>\n", argv0);
  return 1;
}

// Write all of the given buffer, handling short writes and EINTR. Return true
// iff successful.
static bool
writea(int fd, const void* idata, size_t length) {
  const uint8_t* data = (const uint8_t*) idata;

  size_t done = 0;
  while (done < length) {
    ssize_t r;
    do {
      r = write(fd, data + done, length - done);
    } while (r == -1 && errno == EINTR);

    if (r < 1)
      return false;
    done += r;
  }

  return true;
}

// A range of a mmaped file.
class MMappedRange {
 public:
  MMappedRange(const void* data, size_t length)
      : data_(reinterpret_cast<const uint8_t*>(data)),
        length_(length) {
  }

  // Get an object of |length| bytes at |offset| and return a pointer to it
  // unless it's out of bounds.
  const void* GetObject(size_t offset, size_t length) {
    if (offset + length < offset)
      return NULL;
    if (offset + length > length_)
      return NULL;
    return data_ + offset;
  }

  // Get element |index| of an array of objects of length |length| starting at
  // |offset| bytes. Return NULL if out of bounds.
  const void* GetArrayElement(size_t offset, size_t length, unsigned index) {
    const size_t element_offset = offset + index * length;
    return GetObject(element_offset, length);
  }

  // Return a new range which is a subset of this range.
  MMappedRange Subrange(const MDLocationDescriptor& location) const {
    if (location.rva > length_ ||
        location.rva + location.data_size < location.rva ||
        location.rva + location.data_size > length_) {
      return MMappedRange(NULL, 0);
    }

    return MMappedRange(data_ + location.rva, location.data_size);
  }

  const uint8_t* data() const { return data_; }
  size_t length() const { return length_; }

 private:
  const uint8_t* const data_;
  const size_t length_;
};

/* Dynamically determines the byte sex of the system. Returns non-zero
 * for big-endian machines.
 */
static inline int sex() {
  int probe = 1;
  return !*(char *)&probe;
}

typedef struct elf_timeval {    /* Time value with microsecond resolution    */
  long tv_sec;                  /* Seconds                                   */
  long tv_usec;                 /* Microseconds                              */
} elf_timeval;

typedef struct elf_siginfo {    /* Information about signal (unused)         */
  int32_t si_signo;             /* Signal number                             */
  int32_t si_code;              /* Extra code                                */
  int32_t si_errno;             /* Errno                                     */
} elf_siginfo;

typedef struct prstatus {       /* Information about thread; includes CPU reg*/
  elf_siginfo    pr_info;       /* Info associated with signal               */
  uint16_t       pr_cursig;     /* Current signal                            */
  unsigned long  pr_sigpend;    /* Set of pending signals                    */
  unsigned long  pr_sighold;    /* Set of held signals                       */
  pid_t          pr_pid;        /* Process ID                                */
  pid_t          pr_ppid;       /* Parent's process ID                       */
  pid_t          pr_pgrp;       /* Group ID                                  */
  pid_t          pr_sid;        /* Session ID                                */
  elf_timeval    pr_utime;      /* User time                                 */
  elf_timeval    pr_stime;      /* System time                               */
  elf_timeval    pr_cutime;     /* Cumulative user time                      */
  elf_timeval    pr_cstime;     /* Cumulative system time                    */
  user_regs_struct pr_reg;      /* CPU registers                             */
  uint32_t       pr_fpvalid;    /* True if math co-processor being used      */
} prstatus;

typedef struct prpsinfo {       /* Information about process                 */
  unsigned char  pr_state;      /* Numeric process state                     */
  char           pr_sname;      /* Char for pr_state                         */
  unsigned char  pr_zomb;       /* Zombie                                    */
  signed char    pr_nice;       /* Nice val                                  */
  unsigned long  pr_flag;       /* Flags                                     */
#if defined(__x86_64__) || defined(__mips__)
  uint32_t       pr_uid;        /* User ID                                   */
  uint32_t       pr_gid;        /* Group ID                                  */
#else
  uint16_t       pr_uid;        /* User ID                                   */
  uint16_t       pr_gid;        /* Group ID                                  */
#endif
  pid_t          pr_pid;        /* Process ID                                */
  pid_t          pr_ppid;       /* Parent's process ID                       */
  pid_t          pr_pgrp;       /* Group ID                                  */
  pid_t          pr_sid;        /* Session ID                                */
  char           pr_fname[16];  /* Filename of executable                    */
  char           pr_psargs[80]; /* Initial part of arg list                  */
} prpsinfo;

// We parse the minidump file and keep the parsed information in this structure.
struct CrashedProcess {
  CrashedProcess()
      : crashing_tid(-1),
        auxv(NULL),
        auxv_length(0) {
    memset(&prps, 0, sizeof(prps));
    prps.pr_sname = 'R';
  }

  struct Mapping {
    uint64_t start_address, end_address;
  };
  std::vector<Mapping> mappings;

  pid_t crashing_tid;
  int fatal_signal;

  struct Thread {
    pid_t tid;
    user_regs_struct regs;
    user_fpregs_struct fpregs;
    user_fpxregs_struct fpxregs;
    uintptr_t stack_addr;
    const uint8_t* stack;
    size_t stack_length;
  };
  std::vector<Thread> threads;

  const uint8_t* auxv;
  size_t auxv_length;

  prpsinfo prps;
};

static uint32_t
U32(const uint8_t* data) {
  uint32_t v;
  memcpy(&v, data, sizeof(v));
  return v;
}

static uint16_t
U16(const uint8_t* data) {
  uint16_t v;
  memcpy(&v, data, sizeof(v));
  return v;
}

#if defined(__i386__)
static void
ParseThreadRegisters(CrashedProcess::Thread* thread, MMappedRange range) {
  const MDRawContextX86* rawregs =
      (const MDRawContextX86*) range.GetObject(0, sizeof(MDRawContextX86));

  thread->regs.ebx = rawregs->ebx;
  thread->regs.ecx = rawregs->ecx;
  thread->regs.edx = rawregs->edx;
  thread->regs.esi = rawregs->esi;
  thread->regs.edi = rawregs->edi;
  thread->regs.ebp = rawregs->ebp;
  thread->regs.eax = rawregs->eax;
  thread->regs.xds = rawregs->ds;
  thread->regs.xes = rawregs->es;
  thread->regs.xfs = rawregs->fs;
  thread->regs.xgs = rawregs->gs;
  thread->regs.orig_eax = rawregs->eax;
  thread->regs.eip = rawregs->eip;
  thread->regs.xcs = rawregs->cs;
  thread->regs.eflags = rawregs->eflags;
  thread->regs.esp = rawregs->esp;
  thread->regs.xss = rawregs->ss;

  thread->fpregs.cwd = rawregs->float_save.control_word;
  thread->fpregs.swd = rawregs->float_save.status_word;
  thread->fpregs.twd = rawregs->float_save.tag_word;
  thread->fpregs.fip = rawregs->float_save.error_offset;
  thread->fpregs.fcs = rawregs->float_save.error_selector;
  thread->fpregs.foo = rawregs->float_save.data_offset;
  thread->fpregs.fos = rawregs->float_save.data_selector;
  memcpy(thread->fpregs.st_space, rawregs->float_save.register_area,
         10 * 8);

  thread->fpxregs.cwd = rawregs->float_save.control_word;
  thread->fpxregs.swd = rawregs->float_save.status_word;
  thread->fpxregs.twd = rawregs->float_save.tag_word;
  thread->fpxregs.fop = U16(rawregs->extended_registers + 6);
  thread->fpxregs.fip = U16(rawregs->extended_registers + 8);
  thread->fpxregs.fcs = U16(rawregs->extended_registers + 12);
  thread->fpxregs.foo = U16(rawregs->extended_registers + 16);
  thread->fpxregs.fos = U16(rawregs->extended_registers + 20);
  thread->fpxregs.mxcsr = U32(rawregs->extended_registers + 24);
  memcpy(thread->fpxregs.st_space, rawregs->extended_registers + 32, 128);
  memcpy(thread->fpxregs.xmm_space, rawregs->extended_registers + 160, 128);
}
#else
#error "This code has not been ported to your platform yet"
#endif

static void
ParseThreadList(CrashedProcess* crashinfo, MMappedRange range,
                const MMappedRange& full_file) {
  const uint32_t num_threads =
      *(const uint32_t*) range.GetObject(0, sizeof(uint32_t));
  for (unsigned i = 0; i < num_threads; ++i) {
    CrashedProcess::Thread thread;
    memset(&thread, 0, sizeof(thread));
    const MDRawThread* rawthread =
        (MDRawThread*) range.GetArrayElement(sizeof(uint32_t),
                                             sizeof(MDRawThread), i);
    thread.tid = rawthread->thread_id;
    thread.stack_addr = rawthread->stack.start_of_memory_range;
    MMappedRange stack_range = full_file.Subrange(rawthread->stack.memory);
    thread.stack = stack_range.data();
    thread.stack_length = rawthread->stack.memory.data_size;

    ParseThreadRegisters(&thread,
                         full_file.Subrange(rawthread->thread_context));

    crashinfo->threads.push_back(thread);
  }
}

static void
ParseAuxVector(CrashedProcess* crashinfo, MMappedRange range) {
  crashinfo->auxv = range.data();
  crashinfo->auxv_length = range.length();
}

static void
ParseCmdLine(CrashedProcess* crashinfo, MMappedRange range) {
  const char* cmdline = (const char*) range.data();
  for (size_t i = 0; i < range.length(); ++i) {
    if (cmdline[i] == 0) {
      static const size_t fname_len = sizeof(crashinfo->prps.pr_fname) - 1;
      static const size_t args_len = sizeof(crashinfo->prps.pr_psargs) - 1;
      memset(crashinfo->prps.pr_fname, 0, fname_len + 1);
      memset(crashinfo->prps.pr_psargs, 0, args_len + 1);
      const char* binary_name = strrchr(cmdline, '/');
      if (binary_name) {
        binary_name++;
        const unsigned len = strlen(binary_name);
        memcpy(crashinfo->prps.pr_fname, binary_name,
               len > fname_len ? fname_len : len);
      } else {
        memcpy(crashinfo->prps.pr_fname, cmdline,
               i > fname_len ? fname_len : i);
      }

      const unsigned len = range.length() > args_len ?
                           args_len : range.length();
      memcpy(crashinfo->prps.pr_psargs, cmdline, len);
      for (unsigned i = 0; i < len; ++i) {
        if (crashinfo->prps.pr_psargs[i] == 0)
          crashinfo->prps.pr_psargs[i] = ' ';
      }
    }
  }
}

static void
ParseExceptionStream(CrashedProcess* crashinfo, MMappedRange range) {
  const MDRawExceptionStream* exp =
      (MDRawExceptionStream*) range.GetObject(0, sizeof(MDRawExceptionStream));
  crashinfo->crashing_tid = exp->thread_id;
  crashinfo->fatal_signal = (int) exp->exception_record.exception_code;
}

static bool
WriteThread(const CrashedProcess::Thread& thread, int fatal_signal) {
  struct prstatus pr;
  memset(&pr, 0, sizeof(pr));

  pr.pr_info.si_signo = fatal_signal;
  pr.pr_cursig = fatal_signal;
  pr.pr_pid = thread.tid;
  memcpy(&pr.pr_reg, &thread.regs, sizeof(user_regs_struct));

  Nhdr nhdr;
  memset(&nhdr, 0, sizeof(nhdr));
  nhdr.n_namesz = 5;
  nhdr.n_descsz = sizeof(struct prstatus);
  nhdr.n_type = NT_PRSTATUS;
  if (!writea(1, &nhdr, sizeof(nhdr)) ||
      !writea(1, "CORE\0\0\0\0", 8) ||
      !writea(1, &pr, sizeof(struct prstatus))) {
    return false;
  }

  nhdr.n_descsz = sizeof(user_fpregs_struct);
  nhdr.n_type = NT_FPREGSET;
  if (!writea(1, &nhdr, sizeof(nhdr)) ||
      !writea(1, "CORE\0\0\0\0", 8) ||
      !writea(1, &thread.fpregs, sizeof(user_fpregs_struct))) {
    return false;
  }

  nhdr.n_descsz = sizeof(user_fpxregs_struct);
  nhdr.n_type = NT_PRXFPREG;
  if (!writea(1, &nhdr, sizeof(nhdr)) ||
      !writea(1, "LINUX\0\0\0", 8) ||
      !writea(1, &thread.fpxregs, sizeof(user_fpxregs_struct))) {
    return false;
  }

  return true;
}

static void
ParseModuleStream(CrashedProcess* crashinfo, MMappedRange range) {
  const uint32_t num_mappings =
      *(const uint32_t*) range.GetObject(0, sizeof(uint32_t));
  for (unsigned i = 0; i < num_mappings; ++i) {
    CrashedProcess::Mapping mapping;
    const MDRawModule* rawmodule =
        (MDRawModule*) range.GetArrayElement(sizeof(uint32_t),
                                             MD_MODULE_SIZE, i);
    mapping.start_address = rawmodule->base_of_image;
    mapping.end_address = rawmodule->size_of_image + rawmodule->base_of_image;

    crashinfo->mappings.push_back(mapping);
  }
}

int
main(int argc, char** argv) {
  if (argc != 2)
    return usage(argv[0]);

  const int fd = open(argv[1], O_RDONLY);
  if (fd < 0)
    return usage(argv[0]);

  struct stat st;
  fstat(fd, &st);

  const void* bytes = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (bytes == MAP_FAILED) {
    perror("Failed to mmap dump file");
    return 1;
  }

  MMappedRange dump(bytes, st.st_size);

  const MDRawHeader* header =
      (const MDRawHeader*) dump.GetObject(0, sizeof(MDRawHeader));

  CrashedProcess crashinfo;

  for (unsigned i = 0; i < header->stream_count; ++i) {
    const MDRawDirectory* dirent =
        (const MDRawDirectory*) dump.GetArrayElement(
            header->stream_directory_rva, sizeof(MDRawDirectory), i);
    switch (dirent->stream_type) {
      case MD_THREAD_LIST_STREAM:
        ParseThreadList(&crashinfo, dump.Subrange(dirent->location), dump);
        break;
      case MD_LINUX_AUXV:
        ParseAuxVector(&crashinfo, dump.Subrange(dirent->location));
        break;
      case MD_LINUX_CMD_LINE:
        ParseCmdLine(&crashinfo, dump.Subrange(dirent->location));
        break;
      case MD_EXCEPTION_STREAM:
        ParseExceptionStream(&crashinfo, dump.Subrange(dirent->location));
        break;
      case MD_MODULE_LIST_STREAM:
        ParseModuleStream(&crashinfo, dump.Subrange(dirent->location));
      default:
        fprintf(stderr, "Skipping %x\n", dirent->stream_type);
    }
  }

  // Write the ELF header. The file will look like:
  //   ELF header
  //   Phdr for the PT_NOTE
  //   Phdr for each of the thread stacks
  //   PT_NOTE
  //   each of the thread stacks
  Ehdr ehdr;
  memset(&ehdr, 0, sizeof(Ehdr));
  ehdr.e_ident[0] = ELFMAG0;
  ehdr.e_ident[1] = ELFMAG1;
  ehdr.e_ident[2] = ELFMAG2;
  ehdr.e_ident[3] = ELFMAG3;
  ehdr.e_ident[4] = ELF_CLASS;
  ehdr.e_ident[5] = sex() ? ELFDATA2MSB : ELFDATA2LSB;
  ehdr.e_ident[6] = EV_CURRENT;
  ehdr.e_type     = ET_CORE;
  ehdr.e_machine  = ELF_ARCH;
  ehdr.e_version  = EV_CURRENT;
  ehdr.e_phoff    = sizeof(Ehdr);
  ehdr.e_ehsize   = sizeof(Ehdr);
  ehdr.e_phentsize= sizeof(Phdr);
  ehdr.e_phnum    = 1 + crashinfo.threads.size() + crashinfo.mappings.size();
  ehdr.e_shentsize= sizeof(Shdr);
  if (!writea(1, &ehdr, sizeof(Ehdr)))
    return 1;

  size_t offset = sizeof(Ehdr) +
                  (1 + crashinfo.threads.size() +
                   crashinfo.mappings.size()) * sizeof(Phdr);
  size_t filesz = sizeof(Nhdr) + 8 + sizeof(prpsinfo) +
                  // sizeof(Nhdr) + 8 + sizeof(user) +
                  sizeof(Nhdr) + 8 + crashinfo.auxv_length +
                  crashinfo.threads.size() * (
                    (sizeof(Nhdr) + 8 + sizeof(prstatus)) +
                     sizeof(Nhdr) + 8 + sizeof(user_fpregs_struct) +
                     sizeof(Nhdr) + 8 + sizeof(user_fpxregs_struct));

  Phdr phdr;
  memset(&phdr, 0, sizeof(Phdr));
  phdr.p_type = PT_NOTE;
  phdr.p_offset = offset;
  phdr.p_filesz = filesz;
  if (!writea(1, &phdr, sizeof(phdr)))
    return 1;

  phdr.p_type = PT_LOAD;
  phdr.p_align = getpagesize();
  size_t note_align = phdr.p_align - ((offset+filesz) % phdr.p_align);
  if (note_align == phdr.p_align)
    note_align = 0;
  offset += note_align;

  for (unsigned i = 0; i < crashinfo.threads.size(); ++i) {
    const CrashedProcess::Thread& thread = crashinfo.threads[i];
    offset += filesz;
    filesz = thread.stack_length;
    phdr.p_offset = offset;
    phdr.p_vaddr = thread.stack_addr;
    phdr.p_filesz = phdr.p_memsz = filesz;
    phdr.p_flags = PF_R | PF_W;
    if (!writea(1, &phdr, sizeof(phdr)))
      return 1;
  }

  for (unsigned i = 0; i < crashinfo.mappings.size(); ++i) {
    const CrashedProcess::Mapping& mapping = crashinfo.mappings[i];
    phdr.p_offset = 0;
    phdr.p_vaddr = mapping.start_address;
    phdr.p_filesz = 0;
    phdr.p_flags = PF_R;
    phdr.p_memsz = mapping.end_address - mapping.start_address;
    if (!writea(1, &phdr, sizeof(phdr)))
      return 1;
  }

  Nhdr nhdr;
  memset(&nhdr, 0, sizeof(nhdr));
  nhdr.n_namesz = 5;
  nhdr.n_descsz = sizeof(prpsinfo);
  nhdr.n_type = NT_PRPSINFO;
  if (!writea(1, &nhdr, sizeof(nhdr)) ||
      !writea(1, "CORE\0\0\0\0", 8) ||
      !writea(1, &crashinfo.prps, sizeof(prpsinfo))) {
    return 1;
  }

  nhdr.n_descsz = crashinfo.auxv_length;
  nhdr.n_type = NT_AUXV;
  if (!writea(1, &nhdr, sizeof(nhdr)) ||
      !writea(1, "CORE\0\0\0\0", 8) ||
      !writea(1, &crashinfo.auxv, crashinfo.auxv_length)) {
    return 1;
  }

  for (unsigned i = 0; i < crashinfo.threads.size(); ++i) {
    if (crashinfo.threads[i].tid == crashinfo.crashing_tid) {
      WriteThread(crashinfo.threads[i], crashinfo.fatal_signal);
      break;
    }
  }

  for (unsigned i = 0; i < crashinfo.threads.size(); ++i) {
    if (crashinfo.threads[i].tid != crashinfo.crashing_tid)
      WriteThread(crashinfo.threads[i], 0);
  }

  if (note_align) {
    char scratch[note_align];
    memset(scratch, 0, sizeof(scratch));
    if (!writea(1, scratch, sizeof(scratch)))
      return 1;
  }

  for (unsigned i = 0; i < crashinfo.threads.size(); ++i) {
    const CrashedProcess::Thread& thread = crashinfo.threads[i];
    if (!writea(1, thread.stack, thread.stack_length))
      return 1;
  }

  munmap(const_cast<void*>(bytes), st.st_size);

  return 0;
}
