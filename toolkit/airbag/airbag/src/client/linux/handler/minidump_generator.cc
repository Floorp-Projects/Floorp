// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Author: Li Liu
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

#include <fcntl.h>
#include <pthread.h>
#include <asm/sigcontext.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include <cstdlib>
#include <ctime>

#include "common/linux/file_id.h"
#include "client/linux/handler/linux_thread.h"
#include "client/minidump_file_writer.h"
#include "client/minidump_file_writer-inl.h"
#include "google_breakpad/common/minidump_format.h"
#include "client/linux/handler/minidump_generator.h"

#ifndef CLONE_UNTRACED
#define CLONE_UNTRACED 0x00800000
#endif

// This unnamed namespace contains helper functions.
namespace {

using namespace google_breakpad;

// Argument for the writer function.
struct WriterArgument {
  MinidumpFileWriter *minidump_writer;

  // Context for the callback.
  void *version_context;

  // Pid of the thread who called WriteMinidumpToFile
  int requester_pid;

  // The stack bottom of the thread which caused the dump.
  // Mainly used to find the thread id of the crashed thread since signal
  // handler may not be called in the thread who caused it.
  uintptr_t crashed_stack_bottom;

  // Pid of the crashing thread.
  int crashed_pid;

  // Signal number when crash happed. Can be 0 if this is a requested dump.
  int signo;

  // Signal contex when crash happed. Can be NULL if this is a requested dump.
  const struct sigcontext *sig_ctx;

  // Used to get information about the threads.
  LinuxThread *thread_lister;
};

// Holding context information for the callback of finding the crashing thread.
struct FindCrashThreadContext {
  const LinuxThread *thread_lister;
  uintptr_t crashing_stack_bottom;
  int crashing_thread_pid;

  FindCrashThreadContext() :
    thread_lister(NULL),
    crashing_stack_bottom(0UL),
    crashing_thread_pid(-1) {
  }
};

// Callback for list threads.
// It will compare the stack bottom of the provided thread with the stack
// bottom of the crashed thread, it they are eqaul, this is thread is the one
// who crashed.
bool IsThreadCrashedCallback(const ThreadInfo &thread_info, void *context) {
  FindCrashThreadContext *crashing_context =
    static_cast<FindCrashThreadContext *>(context);
  const LinuxThread *thread_lister = crashing_context->thread_lister;
  struct user_regs_struct regs;
  if (thread_lister->GetRegisters(thread_info.pid, &regs)) {
    uintptr_t last_ebp = regs.ebp;
    uintptr_t stack_bottom = thread_lister->GetThreadStackBottom(last_ebp);
    if (stack_bottom > last_ebp &&
        stack_bottom == crashing_context->crashing_stack_bottom) {
      // Got it. Stop iteration.
      crashing_context->crashing_thread_pid = thread_info.pid;
      return false;
    }
  }
  return true;
}

// Find the crashing thread id.
// This is done based on stack bottom comparing.
int FindCrashingThread(uintptr_t crashing_stack_bottom,
                       int requester_pid,
                       const LinuxThread *thread_lister) {
  FindCrashThreadContext context;
  context.thread_lister = thread_lister;
  context.crashing_stack_bottom = crashing_stack_bottom;
  CallbackParam<ThreadCallback> callback_param(IsThreadCrashedCallback,
                                               &context);
  thread_lister->ListThreads(&callback_param);
  return context.crashing_thread_pid;
}

// Write the thread stack info minidump.
bool WriteThreadStack(uintptr_t last_ebp,
                      uintptr_t last_esp,
                      const LinuxThread *thread_lister,
                      UntypedMDRVA *memory,
                      MDMemoryDescriptor *loc) {
  // Maximum stack size for a thread.
  uintptr_t stack_bottom = thread_lister->GetThreadStackBottom(last_ebp);
  if (stack_bottom > last_esp) {
    int size = stack_bottom - last_esp;
    if (size > 0) {
      if (!memory->Allocate(size))
        return false;
      memory->Copy(reinterpret_cast<void*>(last_esp), size);
      loc->start_of_memory_range = 0 | last_esp;
      loc->memory = memory->location();
    }
    return true;
  }
  return false;
}

// Write CPU context based on signal context.
bool WriteContext(MDRawContextX86 *context, const struct sigcontext *sig_ctx,
                  const DebugRegs *debug_regs) {
  assert(sig_ctx != NULL);
  context->context_flags = MD_CONTEXT_X86_FULL;
  context->gs = sig_ctx->gs;
  context->fs = sig_ctx->fs;
  context->es = sig_ctx->es;
  context->ds = sig_ctx->ds;
  context->cs = sig_ctx->cs;
  context->ss = sig_ctx->ss;
  context->edi = sig_ctx->edi;
  context->esi = sig_ctx->esi;
  context->ebp = sig_ctx->ebp;
  context->esp = sig_ctx->esp;
  context->ebx = sig_ctx->ebx;
  context->edx = sig_ctx->edx;
  context->ecx = sig_ctx->ecx;
  context->eax = sig_ctx->eax;
  context->eip = sig_ctx->eip;
  context->eflags = sig_ctx->eflags;
  if (sig_ctx->fpstate != NULL) {
    context->context_flags = MD_CONTEXT_X86_FULL |
      MD_CONTEXT_X86_FLOATING_POINT;
    context->float_save.control_word = sig_ctx->fpstate->cw;
    context->float_save.status_word = sig_ctx->fpstate->sw;
    context->float_save.tag_word = sig_ctx->fpstate->tag;
    context->float_save.error_offset = sig_ctx->fpstate->ipoff;
    context->float_save.error_selector = sig_ctx->fpstate->cssel;
    context->float_save.data_offset = sig_ctx->fpstate->dataoff;
    context->float_save.data_selector = sig_ctx->fpstate->datasel;
    memcpy(context->float_save.register_area, sig_ctx->fpstate->_st,
           sizeof(context->float_save.register_area));
  }

  if (debug_regs != NULL) {
    context->context_flags |= MD_CONTEXT_X86_DEBUG_REGISTERS;
    context->dr0 = debug_regs->dr0;
    context->dr1 = debug_regs->dr1;
    context->dr2 = debug_regs->dr2;
    context->dr3 = debug_regs->dr3;
    context->dr6 = debug_regs->dr6;
    context->dr7 = debug_regs->dr7;
  }
  return true;
}

// Write CPU context based on provided registers.
bool WriteContext(MDRawContextX86 *context,
                  const struct user_regs_struct *regs,
                  const struct user_fpregs_struct *fp_regs,
                  const DebugRegs *dbg_regs) {
  if (!context || !regs)
    return false;

  context->context_flags = MD_CONTEXT_X86_FULL;

  context->cs = regs->xcs;
  context->ds = regs->xds;
  context->es = regs->xes;
  context->fs = regs->xfs;
  context->gs = regs->xgs;
  context->ss = regs->xss;
  context->edi = regs->edi;
  context->esi = regs->esi;
  context->ebx = regs->ebx;
  context->edx = regs->edx;
  context->ecx = regs->ecx;
  context->eax = regs->eax;
  context->ebp = regs->ebp;
  context->eip = regs->eip;
  context->esp = regs->esp;
  context->eflags = regs->eflags;

  if (dbg_regs != NULL) {
    context->context_flags |= MD_CONTEXT_X86_DEBUG_REGISTERS;
    context->dr0 = dbg_regs->dr0;
    context->dr1 = dbg_regs->dr1;
    context->dr2 = dbg_regs->dr2;
    context->dr3 = dbg_regs->dr3;
    context->dr6 = dbg_regs->dr6;
    context->dr7 = dbg_regs->dr7;
  }

  if (fp_regs != NULL) {
    context->context_flags |= MD_CONTEXT_X86_FLOATING_POINT;
    context->float_save.control_word = fp_regs->cwd;
    context->float_save.status_word = fp_regs->swd;
    context->float_save.tag_word = fp_regs->twd;
    context->float_save.error_offset = fp_regs->fip;
    context->float_save.error_selector = fp_regs->fcs;
    context->float_save.data_offset = fp_regs->foo;
    context->float_save.data_selector = fp_regs->fos;
    context->float_save.data_selector = fp_regs->fos;

    memcpy(context->float_save.register_area, fp_regs->st_space,
           sizeof(context->float_save.register_area));
  }
  return true;
}

// Write information about a crashed thread.
// When a thread crash, kernel will write something on the stack for processing
// signal. This makes the current stack not reliable, and our stack walker
// won't figure out the whole call stack for this. So we write the stack at the
// time of the crash into the minidump file, not the current stack.
bool WriteCrashedThreadStream(MinidumpFileWriter *minidump_writer,
                       const WriterArgument *writer_args,
                       const ThreadInfo &thread_info,
                       MDRawThread *thread) {
  assert(writer_args->sig_ctx != NULL);

  thread->thread_id = thread_info.pid;

  UntypedMDRVA memory(minidump_writer);
  if (!WriteThreadStack(writer_args->sig_ctx->ebp,
                   writer_args->sig_ctx->esp,
                   writer_args->thread_lister,
                   &memory,
                   &thread->stack))
    return false;

  TypedMDRVA<MDRawContextX86> context(minidump_writer);
  if (!context.Allocate())
    return false;
  thread->thread_context = context.location();
  memset(context.get(), 0, sizeof(MDRawContextX86));
  return WriteContext(context.get(), writer_args->sig_ctx, NULL);
}

// Write information about a thread.
// This function only processes thread running normally at the crash.
bool WriteThreadStream(MinidumpFileWriter *minidump_writer,
                       const LinuxThread *thread_lister,
                       const ThreadInfo &thread_info,
                       MDRawThread *thread) {
  thread->thread_id = thread_info.pid;

  struct user_regs_struct regs;
  memset(&regs, 0, sizeof(regs));
  if (!thread_lister->GetRegisters(thread_info.pid, &regs)) {
    perror(NULL);
    return false;
  }

  UntypedMDRVA memory(minidump_writer);
  if (!WriteThreadStack(regs.ebp,
                   regs.esp,
                   thread_lister,
                   &memory,
                   &thread->stack))
    return false;

  struct user_fpregs_struct fp_regs;
  DebugRegs dbg_regs;
  memset(&fp_regs, 0, sizeof(fp_regs));
  // Get all the registers.
  thread_lister->GetFPRegisters(thread_info.pid, &fp_regs);
  thread_lister->GetDebugRegisters(thread_info.pid, &dbg_regs);

  // Write context
  TypedMDRVA<MDRawContextX86> context(minidump_writer);
  if (!context.Allocate())
    return false;
  thread->thread_context = context.location();
  memset(context.get(), 0, sizeof(MDRawContextX86));
  return WriteContext(context.get(), &regs, &fp_regs, &dbg_regs);
}

bool WriteCPUInformation(MDRawSystemInfo *sys_info) {
  const char *proc_cpu_path = "/proc/cpuinfo";
  char line[128];

  struct CpuInfoEntry {
    const char *info_name;
    int value;
  } cpu_info_table[] = {
    { "processor", -1 },
    { "model", 0 },
    { "stepping",  0 },
    { "cpuid level", 0 },
    { NULL, -1 },
  };

  FILE *fp = fopen(proc_cpu_path, "r");
  if (fp != NULL) {
    while (fgets(line, sizeof(line), fp)) {
      CpuInfoEntry *entry = &cpu_info_table[0];
      while (entry->info_name != NULL) {
        if (!strncmp(line, entry->info_name, strlen(entry->info_name))) {
          char *value = strchr(line, ':');
          value++;
          if (value != NULL)
            sscanf(value, " %d", &(entry->value));
        }
        entry++;
      }
    }
    fclose(fp);
  }

  // /proc/cpuinfo contains cpu id, change it into number by adding one.
  cpu_info_table[0].value++;

  sys_info->number_of_processors = cpu_info_table[0].value;
  sys_info->processor_level      = cpu_info_table[3].value;
  sys_info->processor_revision   = cpu_info_table[1].value << 8 |
                                   cpu_info_table[2].value;

  sys_info->processor_architecture = MD_CPU_ARCHITECTURE_UNKNOWN;
  struct utsname uts;
  if (uname(&uts) == 0) {
    // Match i*86 and x86* as X86 architecture.
    if ((strstr(uts.machine, "x86") == uts.machine) ||
        (strlen(uts.machine) == 4 &&
         uts.machine[0] == 'i' &&
         uts.machine[2] == '8' &&
         uts.machine[3] == '6'))
      sys_info->processor_architecture = MD_CPU_ARCHITECTURE_X86;
  }
  return true;
}

bool WriteOSInformation(MinidumpFileWriter *minidump_writer,
                        MDRawSystemInfo *sys_info) {
  sys_info->platform_id = MD_OS_LINUX;

  struct utsname uts;
  if (uname(&uts) == 0) {
    char os_version[512];
    size_t space_left = sizeof(os_version);
    memset(os_version, 0, space_left);
    const char *os_info_table[] = {
      uts.sysname,
      uts.release,
      uts.version,
      uts.machine,
      "GNU/Linux",
      NULL
    };
    for (const char **cur_os_info = os_info_table;
         *cur_os_info != NULL;
         cur_os_info++) {
      if (cur_os_info != os_info_table && space_left > 1) {
        strcat(os_version, " ");
        space_left--;
      }
      if (space_left > strlen(*cur_os_info)) {
        strcat(os_version, *cur_os_info);
        space_left -= strlen(*cur_os_info);
      } else {
        break;
      }
    }

    MDLocationDescriptor location;
    if (!minidump_writer->WriteString(os_version, 0, &location))
      return false;
    sys_info->csd_version_rva = location.rva;
  }
  return true;
}

// Callback context for get writting thread information.
struct ThreadInfoCallbackCtx {
  MinidumpFileWriter *minidump_writer;
  const WriterArgument *writer_args;
  TypedMDRVA<MDRawThreadList> *list;
  int thread_index;
};

// Callback run for writing threads information in the process.
bool ThreadInfomationCallback(const ThreadInfo &thread_info,
                                 void *context) {
  ThreadInfoCallbackCtx *callback_context =
    static_cast<ThreadInfoCallbackCtx *>(context);
  bool success = true;
  MDRawThread thread;
  memset(&thread, 0, sizeof(MDRawThread));
  if (thread_info.pid != callback_context->writer_args->crashed_pid ||
      callback_context->writer_args->sig_ctx == NULL) {
    success = WriteThreadStream(callback_context->minidump_writer,
                           callback_context->writer_args->thread_lister,
                           thread_info, &thread);
  } else {
    success = WriteCrashedThreadStream(callback_context->minidump_writer,
                                       callback_context->writer_args,
                                       thread_info, &thread);
  }
  if (success) {
    callback_context->list->CopyIndexAfterObject(
        callback_context->thread_index++,
        &thread, sizeof(MDRawThread));
  }
  return success;
}

// Stream writers
bool WriteThreadListStream(MinidumpFileWriter *minidump_writer,
                           const WriterArgument *writer_args,
                           MDRawDirectory *dir) {
  // Get the thread information.
  const LinuxThread *thread_lister = writer_args->thread_lister;
  int thread_count = thread_lister->GetThreadCount();
  if (thread_count < 0)
    return false;
  TypedMDRVA<MDRawThreadList> list(minidump_writer);
  if (!list.AllocateObjectAndArray(thread_count, sizeof(MDRawThread)))
    return false;
  dir->stream_type = MD_THREAD_LIST_STREAM;
  dir->location = list.location();
  list.get()->number_of_threads = thread_count;

  ThreadInfoCallbackCtx context;
  context.minidump_writer = minidump_writer;
  context.writer_args = writer_args;
  context.list = &list;
  context.thread_index = 0;
  CallbackParam<ThreadCallback> callback_param(ThreadInfomationCallback,
                                               &context);
  int written = thread_lister->ListThreads(&callback_param);
  return written == thread_count;
}

bool WriteCVRecord(MinidumpFileWriter *minidump_writer,
                   MDRawModule *module,
                   const char *module_path) {
  TypedMDRVA<MDCVInfoPDB70> cv(minidump_writer);

  // Only return the last path component of the full module path
  const char *module_name = strrchr(module_path, '/');
  // Increment past the slash
  if (module_name)
    ++module_name;
  else
    module_name = "<Unknown>";

  size_t module_name_length = strlen(module_name);
  if (!cv.AllocateObjectAndArray(module_name_length + 1, sizeof(u_int8_t)))
    return false;
  if (!cv.CopyIndexAfterObject(0, const_cast<char *>(module_name),
                               module_name_length))
    return false;

  module->cv_record = cv.location();
  MDCVInfoPDB70 *cv_ptr = cv.get();
  memset(cv_ptr, 0, sizeof(MDCVInfoPDB70));
  cv_ptr->cv_signature = MD_CVINFOPDB70_SIGNATURE;
  cv_ptr->age = 0;

  // Get the module identifier
  FileID file_id(module_path);
  unsigned char identifier[16];

  if (file_id.ElfFileIdentifier(identifier)) {
    cv_ptr->signature.data1 = (uint32_t)identifier[0] << 24 |
      (uint32_t)identifier[1] << 16 | (uint32_t)identifier[2] << 8 |
      (uint32_t)identifier[3];
    cv_ptr->signature.data2 = (uint32_t)identifier[4] << 8 | identifier[5];
    cv_ptr->signature.data3 = (uint32_t)identifier[6] << 8 | identifier[7];
    cv_ptr->signature.data4[0] = identifier[8];
    cv_ptr->signature.data4[1] = identifier[9];
    cv_ptr->signature.data4[2] = identifier[10];
    cv_ptr->signature.data4[3] = identifier[11];
    cv_ptr->signature.data4[4] = identifier[12];
    cv_ptr->signature.data4[5] = identifier[13];
    cv_ptr->signature.data4[6] = identifier[14];
    cv_ptr->signature.data4[7] = identifier[15];
  }
  return true;
}

struct ModuleInfoCallbackCtx {
  MinidumpFileWriter *minidump_writer;
  const WriterArgument *writer_args;
  TypedMDRVA<MDRawModuleList> *list;
  int module_index;
};

bool ModuleInfoCallback(const ModuleInfo &module_info,
                           void *context) {
  ModuleInfoCallbackCtx *callback_context =
    static_cast<ModuleInfoCallbackCtx *>(context);
  // Skip those modules without name, or those that are not modules.
  if (strlen(module_info.name) == 0 ||
      !strchr(module_info.name, '/'))
    return true;

  MDRawModule module;
  memset(&module, 0, sizeof(module));
  MDLocationDescriptor loc;
  if (!callback_context->minidump_writer->WriteString(module_info.name, 0,
                                                      &loc))
    return false;
  module.base_of_image = (u_int64_t)module_info.start_addr;
  module.size_of_image = module_info.size;
  module.module_name_rva = loc.rva;

  if (!WriteCVRecord(callback_context->minidump_writer, &module,
                     module_info.name))
    return false;
  callback_context->list->CopyIndexAfterObject(
      callback_context->module_index++, &module, MD_MODULE_SIZE);
  return true;
}

bool WriteModuleListStream(MinidumpFileWriter *minidump_writer,
                           const WriterArgument *writer_args,
                           MDRawDirectory *dir) {
  TypedMDRVA<MDRawModuleList> list(minidump_writer);
  int module_count  = writer_args->thread_lister->GetModuleCount();
  if (module_count <= 0 ||
      !list.AllocateObjectAndArray(module_count, MD_MODULE_SIZE))
    return false;
  dir->stream_type = MD_MODULE_LIST_STREAM;
  dir->location = list.location();
  list.get()->number_of_modules = module_count;
  ModuleInfoCallbackCtx context;
  context.minidump_writer = minidump_writer;
  context.writer_args = writer_args;
  context.list = &list;
  context.module_index = 0;
  CallbackParam<ModuleCallback> callback(ModuleInfoCallback, &context);
  return writer_args->thread_lister->ListModules(&callback) == module_count;
}

bool WriteSystemInfoStream(MinidumpFileWriter *minidump_writer,
                           const WriterArgument *writer_args,
                           MDRawDirectory *dir) {
  TypedMDRVA<MDRawSystemInfo> sys_info(minidump_writer);
  if (!sys_info.Allocate())
    return false;
  dir->stream_type = MD_SYSTEM_INFO_STREAM;
  dir->location = sys_info.location();

  return WriteCPUInformation(sys_info.get()) &&
         WriteOSInformation(minidump_writer, sys_info.get());
}

bool WriteExceptionStream(MinidumpFileWriter *minidump_writer,
                           const WriterArgument *writer_args,
                           MDRawDirectory *dir) {
  // This happenes when this is not a crash, but a requested dump.
  if (writer_args->sig_ctx == NULL)
    return false;

  TypedMDRVA<MDRawExceptionStream> exception(minidump_writer);
  if (!exception.Allocate())
    return false;

  dir->stream_type = MD_EXCEPTION_STREAM;
  dir->location = exception.location();
  exception.get()->thread_id = writer_args->crashed_pid;
  exception.get()->exception_record.exception_code = writer_args->signo;
  exception.get()->exception_record.exception_flags = 0;
  if (writer_args->sig_ctx != NULL) {
    exception.get()->exception_record.exception_address =
      writer_args->sig_ctx->eip;
  } else {
    return true;
  }

  // Write context of the exception.
  TypedMDRVA<MDRawContextX86> context(minidump_writer);
  if (!context.Allocate())
    return false;
  exception.get()->thread_context = context.location();
  memset(context.get(), 0, sizeof(MDRawContextX86));
  return WriteContext(context.get(), writer_args->sig_ctx, NULL);
}

bool WriteMiscInfoStream(MinidumpFileWriter *minidump_writer,
                           const WriterArgument *writer_args,
                           MDRawDirectory *dir) {
  TypedMDRVA<MDRawMiscInfo> info(minidump_writer);
  if (!info.Allocate())
    return false;

  dir->stream_type = MD_MISC_INFO_STREAM;
  dir->location = info.location();
  info.get()->size_of_info = sizeof(MDRawMiscInfo);
  info.get()->flags1 = MD_MISCINFO_FLAGS1_PROCESS_ID;
  info.get()->process_id = writer_args->requester_pid;

  return true;
}

bool WriteBreakpadInfoStream(MinidumpFileWriter *minidump_writer,
                           const WriterArgument *writer_args,
                           MDRawDirectory *dir) {
  TypedMDRVA<MDRawBreakpadInfo> info(minidump_writer);
  if (!info.Allocate())
    return false;

  dir->stream_type = MD_BREAKPAD_INFO_STREAM;
  dir->location = info.location();

  info.get()->validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID |
                        MD_BREAKPAD_INFO_VALID_REQUESTING_THREAD_ID;
  info.get()->dump_thread_id = getpid();
  info.get()->requesting_thread_id = writer_args->requester_pid;
  return true;
}

// Prototype of writer functions.
typedef bool (*WriteStringFN)(MinidumpFileWriter *,
                              const WriterArgument *,
                              MDRawDirectory *);

// Function table to writer a full minidump.
WriteStringFN writers[] = {
  WriteThreadListStream,
  WriteModuleListStream,
  WriteSystemInfoStream,
  WriteExceptionStream,
  WriteMiscInfoStream,
  WriteBreakpadInfoStream,
};

// Will call each writer function in the writers table.
// It runs in a different process from the crashing process, but sharing
// the same address space. This enables it to use ptrace functions.
int Write(void *argument) {
  WriterArgument *writer_args =
    static_cast<WriterArgument *>(argument);

  if (!writer_args->thread_lister->SuspendAllThreads())
    return -1;

  if (writer_args->sig_ctx != NULL) {
    writer_args->crashed_stack_bottom =
    writer_args->thread_lister->GetThreadStackBottom(writer_args->sig_ctx->ebp);
    int crashed_pid =  FindCrashingThread(writer_args->crashed_stack_bottom,
                                         writer_args->requester_pid,
                                         writer_args->thread_lister);
    if (crashed_pid > 0)
      writer_args->crashed_pid = crashed_pid;
  }


  MinidumpFileWriter *minidump_writer = writer_args->minidump_writer;
  TypedMDRVA<MDRawHeader> header(minidump_writer);
  TypedMDRVA<MDRawDirectory> dir(minidump_writer);
  if (!header.Allocate())
    return 0;

  int writer_count = sizeof(writers) / sizeof(writers[0]);
  // Need directory space for all writers.
  if (!dir.AllocateArray(writer_count))
    return 0;
  header.get()->signature = MD_HEADER_SIGNATURE;
  header.get()->version = MD_HEADER_VERSION;
  header.get()->time_date_stamp = time(NULL);
  header.get()->stream_count = writer_count;
  header.get()->stream_directory_rva = dir.position();

  int dir_index = 0;
  MDRawDirectory local_dir;
  for (int i = 0; i < writer_count; ++i) {
    if (writers[i](minidump_writer, writer_args, &local_dir))
      dir.CopyIndex(dir_index++, &local_dir);
  }

  writer_args->thread_lister->ResumeAllThreads();
  return 0;
}

}  // namespace

namespace google_breakpad {

MinidumpGenerator::MinidumpGenerator() {
  AllocateStack();
}

MinidumpGenerator::~MinidumpGenerator() {
}

void MinidumpGenerator::AllocateStack() {
  stack_.reset(new char[kStackSize]);
}

bool MinidumpGenerator::WriteMinidumpToFile(const char *file_pathname,
                                   int signo,
                                   const struct sigcontext *sig_ctx) const {
  assert(file_pathname != NULL);
  assert(stack_ != NULL);

  if (stack_ == NULL || file_pathname == NULL)
    return false;

  MinidumpFileWriter minidump_writer;
  if (minidump_writer.Open(file_pathname)) {
    WriterArgument argument;
    memset(&argument, 0, sizeof(argument));
    LinuxThread thread_lister(getpid());
    argument.thread_lister = &thread_lister;
    argument.minidump_writer = &minidump_writer;
    argument.requester_pid = getpid();
    argument.crashed_pid = getpid();
    argument.signo = signo;
    argument.sig_ctx = sig_ctx;

    int cloned_pid = clone(Write, stack_.get() + kStackSize,
                           CLONE_VM | CLONE_FILES | CLONE_FS | CLONE_UNTRACED,
                           (void*)&argument);
    waitpid(cloned_pid, NULL, __WALL);
    return true;
  }

  return false;
}

}  // namespace google_breakpad
