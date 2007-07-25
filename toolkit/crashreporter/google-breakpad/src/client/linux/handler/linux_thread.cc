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
//
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <functional>

#include "client/linux/handler/linux_thread.h"

using namespace google_breakpad;

// This unamed namespace contains helper function.
namespace {

// Context information for the callbacks when validating address by listing
// modules.
struct AddressValidatingContext {
  uintptr_t address;
  bool is_mapped;

  AddressValidatingContext() : address(0UL), is_mapped(false) {
  }
};

// Convert from string to int.
bool LocalAtoi(char *s, int *r) {
  assert(s != NULL);
  assert(r != NULL);
  char *endptr = NULL;
  int ret = strtol(s, &endptr, 10);
  if (endptr == s)
    return false;
  *r = ret;
  return true;
}

// Fill the proc path of a thread given its id.
void FillProcPath(int pid, char *path, int path_size) {
  char pid_str[32];
  snprintf(pid_str, sizeof(pid_str), "%d", pid);
  snprintf(path, path_size, "/proc/%s/", pid_str);
}

// Read thread info from /proc/$pid/status.
bool ReadThreadInfo(int pid, ThreadInfo *info) {
  assert(info != NULL);
  char status_path[80];
  // Max size we want to read from status file.
  static const int kStatusMaxSize = 1024;
  char status_content[kStatusMaxSize];

  FillProcPath(pid, status_path, sizeof(status_path));
  strcat(status_path, "status");
  int fd = open(status_path, O_RDONLY, 0);
  if (fd < 0)
    return false;

  int num_read = read(fd, status_content, kStatusMaxSize - 1);
  if (num_read < 0) {
    close(fd);
    return false;
  }
  close(fd);
  status_content[num_read] = '\0';

  char *tgid_start = strstr(status_content, "Tgid:");
  if (tgid_start)
    sscanf(tgid_start, "Tgid:\t%d\n", &(info->tgid));
  else
    // tgid not supported by kernel??
    info->tgid = 0;

  tgid_start = strstr(status_content, "Pid:");
  if (tgid_start) {
    sscanf(tgid_start, "Pid:\t%d\n" "PPid:\t%d\n", &(info->pid),
           &(info->ppid));
    return true;
  }
  return false;
}

// Callback invoked for each mapped module.
// It use the module's adderss range to validate the address.
bool IsAddressInModuleCallback(const ModuleInfo &module_info,
                               void *context) {
  AddressValidatingContext *addr =
    reinterpret_cast<AddressValidatingContext *>(context);
  addr->is_mapped = ((addr->address >= module_info.start_addr) &&
                     (addr->address <= module_info.start_addr +
                      module_info.size));
  return !addr->is_mapped;
}

#if defined(__i386__) && !defined(NO_FRAME_POINTER)
void *GetNextFrame(void **last_ebp) {
  void *sp = *last_ebp;
  if ((unsigned long)sp == (unsigned long)last_ebp)
    return NULL;
  if ((unsigned long)sp & (sizeof(void *) - 1))
    return NULL;
  if ((unsigned long)sp - (unsigned long)last_ebp > 100000)
    return NULL;
  return sp;
}
#else
void *GetNextFrame(void **last_ebp) {
  return reinterpret_cast<void*>(last_ebp);
}
#endif

// Suspend a thread by attaching to it.
bool SuspendThread(int pid, void *context) {
  // This may fail if the thread has just died or debugged.
  errno = 0;
  if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) != 0 &&
      errno != 0) {
    return false;
  }
  while (waitpid(pid, NULL, __WALL) < 0) {
    if (errno != EINTR) {
      ptrace(PTRACE_DETACH, pid, NULL, NULL);
      return false;
    }
  }
  return true;
}

// Resume a thread by detaching from it.
bool ResumeThread(int pid, void *context) {
  return ptrace(PTRACE_DETACH, pid, NULL, NULL) >= 0;
}

// Callback to get the thread information.
// Will be called for each thread found.
bool ThreadInfoCallback(int pid, void *context) {
  CallbackParam<ThreadCallback> *thread_callback =
    reinterpret_cast<CallbackParam<ThreadCallback> *>(context);
  ThreadInfo thread_info;
  if (ReadThreadInfo(pid, &thread_info) && thread_callback) {
    // Invoke callback from caller.
    return (thread_callback->call_back)(thread_info, thread_callback->context);
  }
  return false;
}

}  // namespace

namespace google_breakpad {

LinuxThread::LinuxThread(int pid) : pid_(pid) , threads_suspened_(false) {
}

LinuxThread::~LinuxThread() {
  if (threads_suspened_)
    ResumeAllThreads();
}

int LinuxThread::SuspendAllThreads() {
  CallbackParam<PidCallback> callback_param(SuspendThread, NULL);
  int thread_count = 0;
  if ((thread_count = IterateProcSelfTask(pid_, &callback_param)) > 0)
    threads_suspened_ = true;
  return thread_count;
}

void LinuxThread::ResumeAllThreads() const {
  CallbackParam<PidCallback> callback_param(ResumeThread, NULL);
  IterateProcSelfTask(pid_, &callback_param);
}

int LinuxThread::GetThreadCount() const {
  return IterateProcSelfTask(pid_, NULL);
}

int LinuxThread::ListThreads(
    CallbackParam<ThreadCallback> *thread_callback_param) const {
  CallbackParam<PidCallback> callback_param(ThreadInfoCallback,
                                            thread_callback_param);
  return IterateProcSelfTask(pid_, &callback_param);
}

bool LinuxThread::GetRegisters(int pid, user_regs_struct *regs) const {
  assert(regs);
  return (regs != NULL &&
          (ptrace(PTRACE_GETREGS, pid, NULL, regs) == 0) &&
          errno == 0);
}

// Get the floating-point registers of a thread.
// The caller must get the thread pid by ListThreads.
bool LinuxThread::GetFPRegisters(int pid, user_fpregs_struct *regs) const {
  assert(regs);
  return (regs != NULL &&
          (ptrace(PTRACE_GETREGS, pid, NULL, regs) ==0) &&
          errno == 0);
}

bool LinuxThread::GetFPXRegisters(int pid, user_fpxregs_struct *regs) const {
  assert(regs);
  return (regs != NULL &&
          (ptrace(PTRACE_GETFPREGS, pid, NULL, regs) != 0) &&
          errno == 0);
}

bool LinuxThread::GetDebugRegisters(int pid, DebugRegs *regs) const {
  assert(regs);

#define GET_DR(name, num)\
  name->dr##num = ptrace(PTRACE_PEEKUSER, pid,\
                         offsetof(struct user, u_debugreg[num]), NULL)
  GET_DR(regs, 0);
  GET_DR(regs, 1);
  GET_DR(regs, 2);
  GET_DR(regs, 3);
  GET_DR(regs, 4);
  GET_DR(regs, 5);
  GET_DR(regs, 6);
  GET_DR(regs, 7);
  return true;
}

int LinuxThread::GetThreadStackDump(uintptr_t current_ebp,
                                    uintptr_t current_esp,
                                    void *buf,
                                    int buf_size) const {
  assert(buf);
  assert(buf_size > 0);

  uintptr_t stack_bottom = GetThreadStackBottom(current_ebp);
  int size = stack_bottom - current_esp;
  size = buf_size > size ? size : buf_size;
  if (size > 0)
    memcpy(buf, reinterpret_cast<void*>(current_esp), size);
  return size;
}

// Get the stack bottom of a thread by stack walking. It works
// unless the stack has been corrupted or the frame pointer has been omited.
// This is just a temporary solution before we get better ideas about how
// this can be done.
//
// We will check each frame address by checking into module maps.
// TODO(liuli): Improve it.
uintptr_t LinuxThread::GetThreadStackBottom(uintptr_t current_ebp) const {
  void **sp = reinterpret_cast<void **>(current_ebp);
  void **previous_sp = sp;
  while (sp && IsAddressMapped((uintptr_t)sp)) {
    previous_sp = sp;
    sp = reinterpret_cast<void **>(GetNextFrame(sp));
  }
  return (uintptr_t)previous_sp;
}

int LinuxThread::GetModuleCount() const {
  return ListModules(NULL);
}

int LinuxThread::ListModules(
    CallbackParam<ModuleCallback> *callback_param) const {
  char line[512];
  const char *maps_path = "/proc/self/maps";

  int module_count = 0;
  FILE *fp = fopen(maps_path, "r");
  if (fp == NULL)
    return -1;

  uintptr_t start_addr;
  uintptr_t end_addr;
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (sscanf(line, "%x-%x", &start_addr, &end_addr) == 2) {
      ModuleInfo module;
      memset(&module, 0, sizeof(module));
      module.start_addr = start_addr;
      module.size = end_addr - start_addr;
      char *name = NULL;
      assert(module.size > 0);
      // Only copy name if the name is a valid path name.
      if ((name = strchr(line, '/')) != NULL) {
        // Get rid of the last '\n' in line
        char *last_return = strchr(line, '\n');
        if (last_return != NULL)
          *last_return = '\0';
        // Keep a space for the ending 0.
        strncpy(module.name, name, sizeof(module.name) - 1);
        ++module_count;
      }
      if (callback_param &&
          !(callback_param->call_back(module, callback_param->context)))
        break;
    }
  }
  fclose(fp);
  return module_count;
}

// Parse /proc/$pid/tasks to list all the threads of the process identified by
// pid.
int LinuxThread::IterateProcSelfTask(int pid,
                          CallbackParam<PidCallback> *callback_param) const {
  char task_path[80];
  FillProcPath(pid, task_path, sizeof(task_path));
  strcat(task_path, "task");

  DIR *dir = opendir(task_path);
  if (dir == NULL)
    return -1;

  int pid_number = 0;
  // Record the last pid we've found. This is used for duplicated thread
  // removal. Duplicated thread information can be found in /proc/$pid/tasks.
  int last_pid = -1;
  struct dirent *entry = NULL;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") &&
        strcmp(entry->d_name, "..")) {
      int tpid = 0;
      if (LocalAtoi(entry->d_name, &tpid) &&
          last_pid != tpid) {
        last_pid = tpid;
        ++pid_number;
        // Invoke the callback.
        if (callback_param &&
            !(callback_param->call_back)(tpid, callback_param->context))
          break;
      }
    }
  }
  closedir(dir);
  return pid_number;
}

// Check if the address is a valid virtual address.
// If the address is in any of the mapped modules, we take it as valid.
// Otherwise it is invalid.
bool LinuxThread::IsAddressMapped(uintptr_t address) const {
  AddressValidatingContext addr;
  addr.address = address;
  CallbackParam<ModuleCallback> callback_param(IsAddressInModuleCallback,
                                               &addr);
  ListModules(&callback_param);
  return addr.is_mapped;
}

}  // namespace google_breakpad
