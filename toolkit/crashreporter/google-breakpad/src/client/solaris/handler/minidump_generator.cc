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

#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>

#include <cstdlib>
#include <ctime>

#include "client/solaris/handler/minidump_generator.h"
#include "client/minidump_file_writer-inl.h"
#include "common/solaris/file_id.h"

namespace google_breakpad {

MinidumpGenerator::MinidumpGenerator()
    : requester_pid_(0),
      signo_(0),
      lwp_lister_(NULL) {
}

MinidumpGenerator::~MinidumpGenerator() {
}

bool MinidumpGenerator::WriteLwpStack(uintptr_t last_esp,
                                      UntypedMDRVA *memory,
                                      MDMemoryDescriptor *loc) {
  uintptr_t stack_bottom = lwp_lister_->GetLwpStackBottom(last_esp);
  if (stack_bottom >= last_esp) {
    int size = stack_bottom - last_esp;
    if (size > 0) {
      if (!memory->Allocate(size))
        return false;
      memory->Copy(reinterpret_cast<void *>(last_esp), size);
      loc->start_of_memory_range = last_esp;
      loc->memory = memory->location();
    }
    return true;
  }
  return false;
}

#if TARGET_CPU_SPARC
bool MinidumpGenerator::WriteContext(MDRawContextSPARC *context, prgregset_t regs,
                                     prfpregset_t *fp_regs) {
  if (!context || !regs)
    return false;

  context->context_flags = MD_CONTEXT_SPARC_FULL;

  context->ccr = (unsigned int)(regs[32]);
  context->pc = (unsigned int)(regs[R_PC]);
  context->npc = (unsigned int)(regs[R_nPC]);
  context->y = (unsigned int)(regs[R_Y]);
  context->asi = (unsigned int)(regs[36]);
  context->fprs = (unsigned int)(regs[37]);

  for ( int i = 0 ; i < 32 ; ++i ){
    context->g_r[i] = (unsigned int)(regs[i]);
  }

  return true;
}
#elif TARGET_CPU_X86
bool MinidumpGenerator::WriteContext(MDRawContextX86 *context, prgregset_t regs,
                                     prfpregset_t *fp_regs) {
  if (!context || !regs)
    return false;

  context->context_flags = MD_CONTEXT_X86_FULL;

  context->cs = regs[CS];
  context->ds = regs[DS];
  context->es = regs[ES];
  context->fs = regs[FS];
  context->gs = regs[GS];
  context->ss = regs[SS];
  context->edi = regs[EDI];
  context->esi = regs[ESI];
  context->ebx = regs[EBX];
  context->edx = regs[EDX];
  context->ecx = regs[ECX];
  context->eax = regs[EAX];
  context->ebp = regs[EBP];
  context->eip = regs[EIP];
  context->esp = regs[UESP];
  context->eflags = regs[EFL];

  return true;
}
#endif /* TARGET_CPU_XXX */

bool MinidumpGenerator::WriteLwpStream(lwpstatus_t *lsp, MDRawThread *lwp) {
  prfpregset_t fp_regs = lsp->pr_fpreg;
  prgregset_t *gregs = &(lsp->pr_reg);
  UntypedMDRVA memory(&writer_);
#if TARGET_CPU_SPARC
  if (!WriteLwpStack((*gregs)[R_SP],
                     &memory,
                     &lwp->stack))
    return false;

  // Write context
  TypedMDRVA<MDRawContextSPARC> context(&writer_);
  if (!context.Allocate())
    return false;
  // should be the thread_id
  lwp->thread_id = lsp->pr_lwpid;
  lwp->thread_context = context.location();
  memset(context.get(), 0, sizeof(MDRawContextSPARC));
#elif TARGET_CPU_X86
  if (!WriteLwpStack((*gregs)[UESP],
                     &memory,
                     &lwp->stack))
  return false;

  // Write context
  TypedMDRVA<MDRawContextX86> context(&writer_);
  if (!context.Allocate())
    return false;
  // should be the thread_id
  lwp->thread_id = lsp->pr_lwpid;
  lwp->thread_context = context.location();
  memset(context.get(), 0, sizeof(MDRawContextX86));
#endif /* TARGET_CPU_XXX */
  return WriteContext(context.get(), (int *)gregs, &fp_regs);
}

bool MinidumpGenerator::WriteCPUInformation(MDRawSystemInfo *sys_info) {
  struct utsname uts;
  char *major, *minor, *build;

  sys_info->number_of_processors = sysconf(_SC_NPROCESSORS_CONF);
  sys_info->processor_architecture = MD_CPU_ARCHITECTURE_UNKNOWN;
  if (uname(&uts) != -1) {
    // Match "i86pc" as X86 architecture.
    if (strcmp(uts.machine, "i86pc") == 0)
      sys_info->processor_architecture = MD_CPU_ARCHITECTURE_X86;
    else if (strcmp(uts.machine, "sun4u") == 0)
      sys_info->processor_architecture = MD_CPU_ARCHITECTURE_SPARC;
  }

  major = uts.release;
  minor = strchr(major, '.');
  *minor = '\0';
  ++minor;
  sys_info->major_version = atoi(major);
  sys_info->minor_version = atoi(minor);

  build = strchr(uts.version, '_');
  ++build;
  sys_info->build_number = atoi(build);
  
  return true;
}

bool MinidumpGenerator::WriteOSInformation(MDRawSystemInfo *sys_info) {
  sys_info->platform_id = MD_OS_SOLARIS;

  struct utsname uts;
  if (uname(&uts) != -1) {
    char os_version[512];
    size_t space_left = sizeof(os_version);
    memset(os_version, 0, space_left);
    const char *os_info_table[] = {
      uts.sysname,
      uts.release,
      uts.version,
      uts.machine,
      "OpenSolaris",
      NULL
    };
    for (const char **cur_os_info = os_info_table;
         *cur_os_info != NULL;
         ++cur_os_info) {
      if (cur_os_info != os_info_table && space_left > 1) {
        strcat(os_version, " ");
        --space_left;
      }
      if (space_left > strlen(*cur_os_info)) {
        strcat(os_version, *cur_os_info);
        space_left -= strlen(*cur_os_info);
      } else {
        break;
      }
    }

    MDLocationDescriptor location;
    if (!writer_.WriteString(os_version, 0, &location))
      return false;
    sys_info->csd_version_rva = location.rva;
  }
  return true;
}

// Callback context for get writting lwp information.
struct LwpInfoCallbackCtx {
  MinidumpGenerator *generator;
  TypedMDRVA<MDRawThreadList> *list;
  int lwp_index;
};

bool LwpInformationCallback(lwpstatus_t *lsp, void *context) {
  bool success = true;
  // The current thread is the one to handle the crash. Ignore it.
  if (lsp->pr_lwpid != pthread_self()) {
    LwpInfoCallbackCtx *callback_context =
      static_cast<LwpInfoCallbackCtx *>(context);
    MDRawThread lwp;
    memset(&lwp, 0, sizeof(MDRawThread));

    success = callback_context->generator->WriteLwpStream(lsp, &lwp);
    if (success) {
      callback_context->list->CopyIndexAfterObject(
          callback_context->lwp_index++,
          &lwp, sizeof(MDRawThread));
    }
  }

  return success;
}

bool MinidumpGenerator::WriteLwpListStream(MDRawDirectory *dir) {
  // Get the lwp information.
  int lwp_count = lwp_lister_->GetLwpCount();
  if (lwp_count < 0)
    return false;
  TypedMDRVA<MDRawThreadList> list(&writer_);
  if (!list.AllocateObjectAndArray(lwp_count - 1, sizeof(MDRawThread)))
    return false;
  dir->stream_type = MD_THREAD_LIST_STREAM;
  dir->location = list.location();
  list.get()->number_of_threads = lwp_count - 1;

  LwpInfoCallbackCtx context;
  context.generator = this;
  context.list = &list;
  context.lwp_index = 0;
  CallbackParam<LwpCallback> callback_param(LwpInformationCallback,
                                            &context);
  int written =
    lwp_lister_->Lwp_iter_all(lwp_lister_->getpid(), &callback_param);
  return written == lwp_count;
}

bool MinidumpGenerator::WriteCVRecord(MDRawModule *module,
                                      const char *module_path) {
  TypedMDRVA<MDCVInfoPDB70> cv(&writer_);

  char path[PATH_MAX];
  const char *module_name = module_path ? module_path : "<Unknown>";
  snprintf(path, sizeof(path), "/proc/self/object/%s", module_name);

  size_t module_name_length = strlen(module_name);
  if (!cv.AllocateObjectAndArray(module_name_length + 1, sizeof(u_int8_t)))
    return false;
  if (!cv.CopyIndexAfterObject(0, const_cast<char *>(module_name),
                               module_name_length)) {
    return false;
  }

  module->cv_record = cv.location();
  MDCVInfoPDB70 *cv_ptr = cv.get();
  memset(cv_ptr, 0, sizeof(MDCVInfoPDB70));
  cv_ptr->cv_signature = MD_CVINFOPDB70_SIGNATURE;
  cv_ptr->age = 0;

  // Get the module identifier
  FileID file_id(path);
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
  MinidumpGenerator *generator;
  MinidumpFileWriter *minidump_writer;
  TypedMDRVA<MDRawModuleList> *list;
  int module_index;
};

bool ModuleInfoCallback(const ModuleInfo &module_info, void *context) {
  ModuleInfoCallbackCtx *callback_context =
    static_cast<ModuleInfoCallbackCtx *>(context);
  // Skip those modules without name, or those that are not modules.
  if (strlen(module_info.name) == 0)
    return true;

  MDRawModule module;
  memset(&module, 0, sizeof(module));
  MDLocationDescriptor loc;
  if (!callback_context->minidump_writer->WriteString(module_info.name,
                                                      0, &loc)) {
    return false;
  }

  module.base_of_image = (u_int64_t)module_info.start_addr;
  module.size_of_image = module_info.size;
  module.module_name_rva = loc.rva;

  if (!callback_context->generator->WriteCVRecord(&module, module_info.name))
    return false;

  callback_context->list->CopyIndexAfterObject(
      callback_context->module_index++, &module, MD_MODULE_SIZE);
  return true;
}

bool MinidumpGenerator::WriteModuleListStream(MDRawDirectory *dir) {
  TypedMDRVA<MDRawModuleList> list(&writer_);
  int module_count = lwp_lister_->GetModuleCount();

  if (module_count <= 0 ||
      !list.AllocateObjectAndArray(module_count, MD_MODULE_SIZE)) {
    return false;
  }

  dir->stream_type = MD_MODULE_LIST_STREAM;
  dir->location = list.location();
  list.get()->number_of_modules = module_count;
  ModuleInfoCallbackCtx context;
  context.generator = this;
  context.minidump_writer = &writer_;
  context.list = &list;
  context.module_index = 0;
  CallbackParam<ModuleCallback> callback(ModuleInfoCallback, &context);
  return lwp_lister_->ListModules(&callback) == module_count;
}

bool MinidumpGenerator::WriteSystemInfoStream(MDRawDirectory *dir) {
  TypedMDRVA<MDRawSystemInfo> sys_info(&writer_);

  if (!sys_info.Allocate())
    return false;

  dir->stream_type = MD_SYSTEM_INFO_STREAM;
  dir->location = sys_info.location();

  return WriteCPUInformation(sys_info.get()) &&
         WriteOSInformation(sys_info.get());
}

bool MinidumpGenerator::WriteExceptionStream(MDRawDirectory *dir) {
  ucontext_t uc;
  gregset_t *gregs;
  fpregset_t fp_regs;

  if (getcontext(&uc) != 0)
    return false;

  TypedMDRVA<MDRawExceptionStream> exception(&writer_);
  if (!exception.Allocate())
    return false;

  dir->stream_type = MD_EXCEPTION_STREAM;
  dir->location = exception.location();
  exception.get()->thread_id = requester_pid_;
  exception.get()->exception_record.exception_code = signo_;
  exception.get()->exception_record.exception_flags = 0;

  gregs = &(uc.uc_mcontext.gregs);
  fp_regs = uc.uc_mcontext.fpregs;
#if TARGET_CPU_SPARC
  exception.get()->exception_record.exception_address = ((unsigned int *)gregs)[1];
  // Write context of the exception.
  TypedMDRVA<MDRawContextSPARC> context(&writer_);
  if (!context.Allocate())
    return false;
  exception.get()->thread_context = context.location();
  memset(context.get(), 0, sizeof(MDRawContextSPARC));
  
  // On Solaris i386, gregset_t = prgregset_t, fpregset_t = prfpregset_t
  // But on Solaris Sparc are diffrent, see sys/regset.h and sys/procfs_isa.h
  context.get()->context_flags = MD_CONTEXT_SPARC_FULL;
  context.get()->ccr = ((unsigned int *)gregs)[0];
  context.get()->pc = ((unsigned int *)gregs)[1];
  context.get()->npc = ((unsigned int *)gregs)[2];
  context.get()->y = ((unsigned int *)gregs)[3];
  context.get()->asi = ((unsigned int *)gregs)[19];
  context.get()->fprs = ((unsigned int *)gregs)[20];
  for (int i = 0; i < 32; ++i) {
    context.get()->g_r[i] = 0;
  }
  for (int i = 1; i < 16; ++i) {
    context.get()->g_r[i] = ((unsigned int *)gregs)[i + 3];
  }
 
  return true;
#elif TARGET_CPU_X86
  exception.get()->exception_record.exception_address = (*gregs)[EIP];
  // Write context of the exception.
  TypedMDRVA<MDRawContextX86> context(&writer_);
  if (!context.Allocate())
    return false;
  exception.get()->thread_context = context.location();
  memset(context.get(), 0, sizeof(MDRawContextX86));
  return WriteContext(context.get(), (int *)gregs, &fp_regs);
#endif /* TARGET_CPU_XXX */
}

bool MinidumpGenerator::WriteMiscInfoStream(MDRawDirectory *dir) {
  TypedMDRVA<MDRawMiscInfo> info(&writer_);

  if (!info.Allocate())
    return false;

  dir->stream_type = MD_MISC_INFO_STREAM;
  dir->location = info.location();
  info.get()->size_of_info = sizeof(MDRawMiscInfo);
  info.get()->flags1 = MD_MISCINFO_FLAGS1_PROCESS_ID;
  info.get()->process_id = requester_pid_;

  return true;
}

bool MinidumpGenerator::WriteBreakpadInfoStream(MDRawDirectory *dir) {
  TypedMDRVA<MDRawBreakpadInfo> info(&writer_);

  if (!info.Allocate())
    return false;

  dir->stream_type = MD_BREAKPAD_INFO_STREAM;
  dir->location = info.location();

  info.get()->validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID |
                         MD_BREAKPAD_INFO_VALID_REQUESTING_THREAD_ID;
  info.get()->dump_thread_id = getpid();
  info.get()->requesting_thread_id = requester_pid_;
  return true;
}

class AutoLwpResumer {
 public:
  AutoLwpResumer(SolarisLwp *lwp) : lwp_(lwp) {}
  ~AutoLwpResumer() { lwp_->ControlAllLwps(false); }
 private:
  SolarisLwp *lwp_;
};

// Will call each writer function in the writers table.
void* MinidumpGenerator::Write() {
  // Function table to writer a full minidump.
  const WriteStreamFN writers[] = {
    &MinidumpGenerator::WriteLwpListStream,
    &MinidumpGenerator::WriteModuleListStream,
    &MinidumpGenerator::WriteSystemInfoStream,
    &MinidumpGenerator::WriteExceptionStream,
    &MinidumpGenerator::WriteMiscInfoStream,
    &MinidumpGenerator::WriteBreakpadInfoStream,
  };

  if (!lwp_lister_->ControlAllLwps(true))
    return NULL;

  AutoLwpResumer lwpResumer(lwp_lister_);

  TypedMDRVA<MDRawHeader> header(&writer_);
  TypedMDRVA<MDRawDirectory> dir(&writer_);
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
    if ((this->*writers[i])(&local_dir))
      dir.CopyIndex(dir_index++, &local_dir);
  }

  return 0;
}

// Write minidump into file.
// It runs in a different thread from the crashing thread.
bool MinidumpGenerator::WriteMinidumpToFile(const char *file_pathname,
                                            int signo) {
  assert(file_pathname != NULL);

  if (file_pathname == NULL)
    return false;

  if (writer_.Open(file_pathname)) {
    SolarisLwp lwp_lister(getpid());
    lwp_lister_ = &lwp_lister;
    requester_pid_ = getpid();
    signo_ = signo;
    if (Write())
      return true;
  }

  return false;
}

}  // namespace google_breakpad
