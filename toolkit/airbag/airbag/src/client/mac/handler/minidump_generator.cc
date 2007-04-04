// Copyright (c) 2006, Google Inc.
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

#include <cstdio>

#include <mach/host_info.h>
#include <mach/vm_statistics.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <sys/sysctl.h>

#include <CoreFoundation/CoreFoundation.h>

#include "client/mac/handler/minidump_generator.h"
#include "client/minidump_file_writer-inl.h"
#include "common/mac/file_id.h"
#include "common/mac/string_utilities.h"

using MacStringUtils::ConvertToString;
using MacStringUtils::IntegerValueAtIndex;

namespace google_breakpad {
  
MinidumpGenerator::MinidumpGenerator()
    : exception_type_(0),
      exception_code_(0),
      exception_thread_(0) {
}

MinidumpGenerator::~MinidumpGenerator() {
}

char MinidumpGenerator::build_string_[16];
int MinidumpGenerator::os_major_version_ = 0;
int MinidumpGenerator::os_minor_version_ = 0;
int MinidumpGenerator::os_build_number_ = 0;

// static
void MinidumpGenerator::GatherSystemInformation() {
  // If this is non-zero, then we've already gathered the information
  if (os_major_version_)
    return;
  
  // This code extracts the version and build information from the OS
  CFStringRef vers_path =
    CFSTR("/System/Library/CoreServices/SystemVersion.plist");
  CFURLRef sys_vers =
    CFURLCreateWithFileSystemPath(NULL, vers_path, kCFURLPOSIXPathStyle, false);
  CFDataRef data;
  SInt32 error;
  CFURLCreateDataAndPropertiesFromResource(NULL, sys_vers, &data, NULL, NULL,
                                           &error);
  
  if (!data)
    return;
  
  CFDictionaryRef list = static_cast<CFDictionaryRef>
    (CFPropertyListCreateFromXMLData(NULL, data, kCFPropertyListImmutable,
                                     NULL));
  if (!list)
    return;
  
  CFStringRef build_version = static_cast<CFStringRef>
    (CFDictionaryGetValue(list, CFSTR("ProductBuildVersion")));
  CFStringRef product_version = static_cast<CFStringRef>
    (CFDictionaryGetValue(list, CFSTR("ProductVersion")));
  string build_str = ConvertToString(build_version);
  string product_str = ConvertToString(product_version);

  CFRelease(list);
  CFRelease(sys_vers);
  CFRelease(data);

  strlcpy(build_string_, build_str.c_str(), sizeof(build_string_));

  // Parse the string that looks like "10.4.8"
  os_major_version_ = IntegerValueAtIndex(product_str, 0);
  os_minor_version_ = IntegerValueAtIndex(product_str, 1);
  os_build_number_ = IntegerValueAtIndex(product_str, 2);
}

string MinidumpGenerator::UniqueNameInDirectory(const string &dir,
                                                string *unique_name) {
  CFUUIDRef uuid = CFUUIDCreate(NULL);
  CFStringRef uuid_cfstr = CFUUIDCreateString(NULL, uuid);
  CFRelease(uuid);
  string file_name(ConvertToString(uuid_cfstr));
  CFRelease(uuid_cfstr);
  string path(dir);

  // Ensure that the directory (if non-empty) has a trailing slash so that
  // we can append the file name and have a valid pathname.
  if (!dir.empty()) {
    if (dir.at(dir.size() - 1) != '/')
      path.append(1, '/');
  }

  path.append(file_name);
  path.append(".dmp");

  if (unique_name)
    *unique_name = file_name;

  return path;
}

bool MinidumpGenerator::Write(const char *path) {
  WriteStreamFN writers[] = {
    &MinidumpGenerator::WriteThreadListStream,
    &MinidumpGenerator::WriteSystemInfoStream,
    &MinidumpGenerator::WriteModuleListStream,
    &MinidumpGenerator::WriteMiscInfoStream,
    &MinidumpGenerator::WriteBreakpadInfoStream,
    // Exception stream needs to be the last entry in this array as it may
    // be omitted in the case where the minidump is written without an
    // exception.
    &MinidumpGenerator::WriteExceptionStream,
  };
  bool result = true;

  // If opening was successful, create the header, directory, and call each
  // writer.  The destructor for the TypedMDRVAs will cause the data to be
  // flushed.  The destructor for the MinidumpFileWriter will close the file.
  if (writer_.Open(path)) {
    TypedMDRVA<MDRawHeader> header(&writer_);
    TypedMDRVA<MDRawDirectory> dir(&writer_);

    if (!header.Allocate())
      return false;

    int writer_count = sizeof(writers) / sizeof(writers[0]);

    // If we don't have exception information, don't write out the
    // exception stream
    if (!exception_thread_ && !exception_type_)
      --writer_count;

    // Add space for all writers
    if (!dir.AllocateArray(writer_count))
      return false;

    MDRawHeader *header_ptr = header.get();
    header_ptr->signature = MD_HEADER_SIGNATURE;
    header_ptr->version = MD_HEADER_VERSION;
    time(reinterpret_cast<time_t *>(&(header_ptr->time_date_stamp)));
    header_ptr->stream_count = writer_count;
    header_ptr->stream_directory_rva = dir.position();

    MDRawDirectory local_dir;
    for (int i = 0; (result) && (i < writer_count); ++i) {
      result = (this->*writers[i])(&local_dir);

      if (result)
        dir.CopyIndex(i, &local_dir);
    }
  }

  return result;
}

static size_t CalculateStackSize(vm_address_t start_addr) {
  vm_address_t stack_region_base = start_addr;
  vm_size_t stack_region_size;
  natural_t nesting_level = 0;
  vm_region_submap_info submap_info;
  mach_msg_type_number_t info_count = VM_REGION_SUBMAP_INFO_COUNT;
  kern_return_t result = 
    vm_region_recurse(mach_task_self(), &stack_region_base, &stack_region_size,
                      &nesting_level, 
                      reinterpret_cast<vm_region_recurse_info_t>(&submap_info),
                      &info_count);

  if ((stack_region_base + stack_region_size) == 0xbffff000) {
    // The stack for thread 0 needs to extend all the way to 0xc0000000
    // For many processes the stack is first created in one page
    // from 0xbffff000 - 0xc0000000 and is then later extended to
    // a much larger size by creating a new VM region immediately below
    // the initial page

    // include the original stack frame page (0xbffff000 - 0xc0000000)
    stack_region_size += 0x1000;  
  }

  return result == KERN_SUCCESS ? 
    stack_region_base + stack_region_size - start_addr : 0;
}

bool MinidumpGenerator::WriteStackFromStartAddress(
    vm_address_t start_addr,
    MDMemoryDescriptor *stack_location) {
  UntypedMDRVA memory(&writer_);
  size_t size = CalculateStackSize(start_addr);

  // If there's an error in the calculation, return at least the current
  // stack information
  if (size == 0)
    size = 16;

  if (!memory.Allocate(size))
    return false;

  bool result = memory.Copy(reinterpret_cast<const void *>(start_addr), size);
  stack_location->start_of_memory_range = start_addr;
  stack_location->memory = memory.location();

  return result;
}

#if TARGET_CPU_PPC
bool MinidumpGenerator::WriteStack(thread_state_data_t state,
                                   MDMemoryDescriptor *stack_location) {
  ppc_thread_state_t *machine_state =
    reinterpret_cast<ppc_thread_state_t *>(state);
  vm_address_t start_addr = machine_state->r1;
  return WriteStackFromStartAddress(start_addr, stack_location);
}

u_int64_t MinidumpGenerator::CurrentPCForStack(thread_state_data_t state) {
  ppc_thread_state_t *machine_state =
    reinterpret_cast<ppc_thread_state_t *>(state);

  return machine_state->srr0;
}

bool MinidumpGenerator::WriteContext(thread_state_data_t state,
                                     MDLocationDescriptor *register_location) {
  TypedMDRVA<MDRawContextPPC> context(&writer_);
  ppc_thread_state_t *machine_state =
    reinterpret_cast<ppc_thread_state_t *>(state);

  if (!context.Allocate())
    return false;

  *register_location = context.location();
  MDRawContextPPC *context_ptr = context.get();
  context_ptr->context_flags = MD_CONTEXT_PPC_BASE;

#define AddReg(a) context_ptr->a = machine_state->a
#define AddGPR(a) context_ptr->gpr[a] = machine_state->r ## a
  AddReg(srr0);
  AddReg(cr);
  AddReg(xer);
  AddReg(ctr);
  AddReg(mq);
  AddReg(lr);
  AddReg(vrsave);

  AddGPR(0);
  AddGPR(1);
  AddGPR(2);
  AddGPR(3);
  AddGPR(4);
  AddGPR(5);
  AddGPR(6);
  AddGPR(7);
  AddGPR(8);
  AddGPR(9);
  AddGPR(10);
  AddGPR(11);
  AddGPR(12);
  AddGPR(13);
  AddGPR(14);
  AddGPR(15);
  AddGPR(16);
  AddGPR(17);
  AddGPR(18);
  AddGPR(19);
  AddGPR(20);
  AddGPR(21);
  AddGPR(22);
  AddGPR(23);
  AddGPR(24);
  AddGPR(25);
  AddGPR(26);
  AddGPR(27);
  AddGPR(28);
  AddGPR(29);
  AddGPR(30);
  AddGPR(31);
  return true;
}

#elif TARGET_CPU_X86
bool MinidumpGenerator::WriteStack(thread_state_data_t state,
                                   MDMemoryDescriptor *stack_location) {
  x86_thread_state_t *machine_state =
    reinterpret_cast<x86_thread_state_t *>(state);
  vm_address_t start_addr = machine_state->uts.ts32.esp;
  return WriteStackFromStartAddress(start_addr, stack_location);
}

u_int64_t MinidumpGenerator::CurrentPCForStack(thread_state_data_t state) {
  x86_thread_state_t *machine_state =
    reinterpret_cast<x86_thread_state_t *>(state);

  return machine_state->uts.ts32.eip;
}

bool MinidumpGenerator::WriteContext(thread_state_data_t state,
                                     MDLocationDescriptor *register_location) {
  TypedMDRVA<MDRawContextX86> context(&writer_);
  x86_thread_state_t *machine_state =
    reinterpret_cast<x86_thread_state_t *>(state);

  if (!context.Allocate())
    return false;

  *register_location = context.location();
  MDRawContextX86 *context_ptr = context.get();
  context_ptr->context_flags = MD_CONTEXT_X86;
#define AddReg(a) context_ptr->a = machine_state->uts.ts32.a
  AddReg(cs);
  AddReg(ds);
  AddReg(ss);
  AddReg(es);
  AddReg(fs);
  AddReg(gs);
  AddReg(eflags);

  AddReg(eip);
  AddReg(eax);
  AddReg(ebx);
  AddReg(ecx);
  AddReg(edx);
  AddReg(esi);
  AddReg(edi);
  AddReg(ebp);
  AddReg(esp);
  return true;
}
#endif

bool MinidumpGenerator::WriteThreadStream(mach_port_t thread_id,
                                          MDRawThread *thread) {
  thread_state_data_t state;
  mach_msg_type_number_t state_count = sizeof(state);

  if (thread_get_state(thread_id, MACHINE_THREAD_STATE, state, &state_count) ==
      KERN_SUCCESS) {
    if (!WriteStack(state, &thread->stack))
      return false;

    if (!WriteContext(state, &thread->thread_context))
      return false;

    thread->thread_id = thread_id;
  } else {
    return false;
  }

  return true;
}

bool MinidumpGenerator::WriteThreadListStream(
    MDRawDirectory *thread_list_stream) {
  TypedMDRVA<MDRawThreadList> list(&writer_);
  thread_act_port_array_t threads_for_task;
  mach_msg_type_number_t thread_count;
  int non_generator_thread_count;

  if (task_threads(mach_task_self(), &threads_for_task, &thread_count))
    return false;

  // Don't include the generator thread
  non_generator_thread_count = thread_count - 1;
  if (!list.AllocateObjectAndArray(non_generator_thread_count,
                                   sizeof(MDRawThread)))
    return false;

  thread_list_stream->stream_type = MD_THREAD_LIST_STREAM;
  thread_list_stream->location = list.location();

  list.get()->number_of_threads = non_generator_thread_count;

  MDRawThread thread;
  int thread_idx = 0;

  for (unsigned int i = 0; i < thread_count; ++i) {
    memset(&thread, 0, sizeof(MDRawThread));

    if (threads_for_task[i] != mach_thread_self()) {
      if (!WriteThreadStream(threads_for_task[i], &thread))
        return false;

      list.CopyIndexAfterObject(thread_idx++, &thread, sizeof(MDRawThread));
    }
  }

  return true;
}

bool MinidumpGenerator::WriteExceptionStream(MDRawDirectory *exception_stream) {
  TypedMDRVA<MDRawExceptionStream> exception(&writer_);

  if (!exception.Allocate())
    return false;

  exception_stream->stream_type = MD_EXCEPTION_STREAM;
  exception_stream->location = exception.location();
  MDRawExceptionStream *exception_ptr = exception.get();
  exception_ptr->thread_id = exception_thread_;

  // This naming is confusing, but it is the proper translation from
  // mach naming to minidump naming.
  exception_ptr->exception_record.exception_code = exception_type_;
  exception_ptr->exception_record.exception_flags = exception_code_;

  thread_state_data_t state;
  mach_msg_type_number_t stateCount = sizeof(state);

  if (thread_get_state(exception_thread_, MACHINE_THREAD_STATE, state,
                       &stateCount) != KERN_SUCCESS)
    return false;

  if (!WriteContext(state, &exception_ptr->thread_context))
    return false;

  exception_ptr->exception_record.exception_address = CurrentPCForStack(state);

  return true;
}

bool MinidumpGenerator::WriteSystemInfoStream(
    MDRawDirectory *system_info_stream) {
  TypedMDRVA<MDRawSystemInfo> info(&writer_);

  if (!info.Allocate())
    return false;

  system_info_stream->stream_type = MD_SYSTEM_INFO_STREAM;
  system_info_stream->location = info.location();

  // CPU Information
  uint32_t cpu_type;
  size_t len = sizeof(cpu_type);
  sysctlbyname("hw.cputype", &cpu_type, &len, NULL, 0);
  uint32_t number_of_processors;
  len = sizeof(number_of_processors);
  sysctlbyname("hw.ncpu", &number_of_processors, &len, NULL, 0);
  MDRawSystemInfo *info_ptr = info.get();

  switch (cpu_type) {
    case CPU_TYPE_POWERPC:
      info_ptr->processor_architecture = MD_CPU_ARCHITECTURE_PPC;
      break;
    case CPU_TYPE_I386:
      info_ptr->processor_architecture = MD_CPU_ARCHITECTURE_X86;
      break;
    default:
      info_ptr->processor_architecture = MD_CPU_ARCHITECTURE_UNKNOWN;
      break;
  }

  info_ptr->number_of_processors = number_of_processors;
  info_ptr->platform_id = MD_OS_MAC_OS_X;

  MDLocationDescriptor build_string_loc;

  if (!writer_.WriteString(build_string_, 0,
                           &build_string_loc))
    return false;

  info_ptr->csd_version_rva = build_string_loc.rva;
  info_ptr->major_version = os_major_version_;
  info_ptr->minor_version = os_minor_version_;
  info_ptr->build_number = os_build_number_;

  return true;
}

bool MinidumpGenerator::WriteModuleStream(unsigned int index,
                                          MDRawModule *module) {
  const struct mach_header *header = _dyld_get_image_header(index);

  if (!header)
    return false;

  int cpu_type = header->cputype;
  unsigned long slide = _dyld_get_image_vmaddr_slide(index);
  const char* name = _dyld_get_image_name(index);
  const struct load_command *cmd =
    reinterpret_cast<const struct load_command *>(header + 1);

  memset(module, 0, sizeof(MDRawModule));

  for (unsigned int i = 0; cmd && (i < header->ncmds); i++) {
    if (cmd->cmd == LC_SEGMENT) {
      const struct segment_command *seg =
        reinterpret_cast<const struct segment_command *>(cmd);
      if (!strcmp(seg->segname, "__TEXT")) {
        MDLocationDescriptor string_location;

        if (!writer_.WriteString(name, 0, &string_location))
          return false;

        module->base_of_image = seg->vmaddr + slide;
        module->size_of_image = seg->vmsize;
        module->module_name_rva = string_location.rva;

        if (!WriteCVRecord(module, cpu_type, name))
          return false;

        return true;
      }
    }

    cmd = reinterpret_cast<struct load_command *>((char *)cmd + cmd->cmdsize);
  }

  return true;
}

static int FindExecutableModule() {
  int image_count = _dyld_image_count();
  const struct mach_header *header;

  for (int i = 0; i < image_count; ++i) {
    header = _dyld_get_image_header(i);

    if (header->filetype == MH_EXECUTE)
      return i;
  }

  return 0;
}

bool MinidumpGenerator::WriteCVRecord(MDRawModule *module, int cpu_type,
                                      const char *module_path) {
  TypedMDRVA<MDCVInfoPDB70> cv(&writer_);

  // Only return the last path component of the full module path
  char *module_name = strrchr(module_path, '/');

  // Increment past the slash
  if (module_name)
    ++module_name;
  else
    module_name = "<Unknown>";

  size_t module_name_length = strlen(module_name);

  if (!cv.AllocateObjectAndArray(module_name_length + 1, sizeof(u_int8_t)))
    return false;

  if (!cv.CopyIndexAfterObject(0, module_name, module_name_length))
    return false;

  module->cv_record = cv.location();
  MDCVInfoPDB70 *cv_ptr = cv.get();
  cv_ptr->cv_signature = MD_CVINFOPDB70_SIGNATURE;
  cv_ptr->age = 0;

  // Get the module identifier
  FileID file_id(module_path);
  unsigned char identifier[16];
  
  if (file_id.MachoIdentifier(cpu_type, identifier)) {
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

bool MinidumpGenerator::WriteModuleListStream(
    MDRawDirectory *module_list_stream) {
  TypedMDRVA<MDRawModuleList> list(&writer_);

  if (!_dyld_present())
    return false;

  int image_count = _dyld_image_count();

  if (!list.AllocateObjectAndArray(image_count, MD_MODULE_SIZE))
    return false;

  module_list_stream->stream_type = MD_MODULE_LIST_STREAM;
  module_list_stream->location = list.location();
  list.get()->number_of_modules = image_count;

  // Write out the executable module as the first one
  MDRawModule module;
  int executableIndex = FindExecutableModule();

  if (!WriteModuleStream(executableIndex, &module))
    return false;

  list.CopyIndexAfterObject(0, &module, MD_MODULE_SIZE);
  int destinationIndex = 1;  // Write all other modules after this one

  for (int i = 0; i < image_count; ++i) {
    if (i != executableIndex) {
      if (!WriteModuleStream(i, &module))
        return false;

      list.CopyIndexAfterObject(destinationIndex++, &module, MD_MODULE_SIZE);
    }
  }

  return true;
}

bool MinidumpGenerator::WriteMiscInfoStream(MDRawDirectory *misc_info_stream) {
  TypedMDRVA<MDRawMiscInfo> info(&writer_);

  if (!info.Allocate())
    return false;

  misc_info_stream->stream_type = MD_MISC_INFO_STREAM;
  misc_info_stream->location = info.location();

  MDRawMiscInfo *info_ptr = info.get();
  info_ptr->size_of_info = sizeof(MDRawMiscInfo);
  info_ptr->flags1 = MD_MISCINFO_FLAGS1_PROCESS_ID |
    MD_MISCINFO_FLAGS1_PROCESS_TIMES |
    MD_MISCINFO_FLAGS1_PROCESSOR_POWER_INFO;

  // Process ID
  info_ptr->process_id = getpid();

  // Times
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) != -1) {
    // Omit the fractional time since the MDRawMiscInfo only wants seconds
    info_ptr->process_user_time = usage.ru_utime.tv_sec;
    info_ptr->process_kernel_time = usage.ru_stime.tv_sec;
  }
  int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, info_ptr->process_id };
  size_t size;
  if (!sysctl(mib, sizeof(mib) / sizeof(mib[0]), NULL, &size, NULL, 0)) {
    vm_address_t addr;
    if (vm_allocate(mach_task_self(), &addr, size, true) == KERN_SUCCESS) {
      struct kinfo_proc *proc = (struct kinfo_proc *)addr;
      if (!sysctl(mib, sizeof(mib) / sizeof(mib[0]), proc, &size, NULL, 0))
        info_ptr->process_create_time = proc->kp_proc.p_starttime.tv_sec;
      vm_deallocate(mach_task_self(), addr, size);
    }
  }

  // Speed
  uint64_t speed;
  size = sizeof(speed);
  sysctlbyname("hw.cpufrequency_max", &speed, &size, NULL, 0);
  info_ptr->processor_max_mhz = speed / (1000 * 1000);
  info_ptr->processor_mhz_limit = speed / (1000 * 1000);
  size = sizeof(speed);
  sysctlbyname("hw.cpufrequency", &speed, &size, NULL, 0);
  info_ptr->processor_current_mhz = speed / (1000 * 1000);

  return true;
}

bool MinidumpGenerator::WriteBreakpadInfoStream(
    MDRawDirectory *breakpad_info_stream) {
  TypedMDRVA<MDRawBreakpadInfo> info(&writer_);

  if (!info.Allocate())
    return false;

  breakpad_info_stream->stream_type = MD_BREAKPAD_INFO_STREAM;
  breakpad_info_stream->location = info.location();
  MDRawBreakpadInfo *info_ptr = info.get();

  if (exception_thread_ && exception_type_) {
    info_ptr->validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID |
                         MD_BREAKPAD_INFO_VALID_REQUESTING_THREAD_ID;
    info_ptr->dump_thread_id = mach_thread_self();
    info_ptr->requesting_thread_id = exception_thread_;
  } else {
    info_ptr->validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID;
    info_ptr->dump_thread_id = mach_thread_self();
    info_ptr->requesting_thread_id = 0;
  }

  return true;
}

}  // namespace google_breakpad
