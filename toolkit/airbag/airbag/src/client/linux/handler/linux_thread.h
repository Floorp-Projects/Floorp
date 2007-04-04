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
#ifndef CLIENT_LINUX_HANDLER_LINUX_THREAD_H__
#define CLIENT_LINUX_HANDLER_LINUX_THREAD_H__

#include <stdint.h>
#include <sys/user.h>

namespace google_breakpad {

// Max module path name length.
#define kMaxModuleNameLength 256

// Holding information about a thread in the process.
struct ThreadInfo {
  // Id of the thread group.
  int tgid;
  // Id of the thread.
  int pid;
  // Id of the parent process.
  int ppid;
};

// Holding infomaton about a module in the process.
struct ModuleInfo {
  char name[kMaxModuleNameLength];
  uintptr_t start_addr;
  int size;
};

// Holding debug registers.
struct DebugRegs {
  int dr0;
  int dr1;
  int dr2;
  int dr3;
  int dr4;
  int dr5;
  int dr6;
  int dr7;
};

// A callback to run when got a thread in the process.
// Return true will go on to the next thread while return false will stop the
// iteration.
typedef bool (*ThreadCallback)(const ThreadInfo &thread_info, void *context);

// A callback to run when a new module is found in the process.
// Return true will go on to the next module while return false will stop the
// iteration.
typedef bool (*ModuleCallback)(const ModuleInfo &module_info, void *context);

// Holding the callback information.
template<class CallbackFunc>
struct CallbackParam {
  // Callback function address.
  CallbackFunc call_back;
  // Callback context;
  void *context;

  CallbackParam() : call_back(NULL), context(NULL) {
  }

  CallbackParam(CallbackFunc func, void *func_context) :
    call_back(func), context(func_context) {
  }
};

///////////////////////////////////////////////////////////////////////////////

//
// LinuxThread
//
// Provides handy support for operation on linux threads.
// It uses ptrace to get thread registers. Since ptrace only works in a
// different process other than the one being ptraced, user of this class
// should create another process before using the class.
//
// The process should be created in the following way:
//    int cloned_pid = clone(ProcessEntryFunction, stack_address,
//                           CLONE_VM | CLONE_FILES | CLONE_FS | CLONE_UNTRACED,
//                           (void*)&arguments);
//    waitpid(cloned_pid, NULL, __WALL);
//
// If CLONE_VM is not used, GetThreadStackBottom, GetThreadStackDump
// will not work since it just use memcpy to get the stack dump.
//
class LinuxThread {
 public:
  // Create a LinuxThread instance to list all the threads in a process.
  explicit LinuxThread(int pid);
  ~LinuxThread();

  // Stop all the threads in the process.
  // Return the number of stopped threads in the process.
  // Return -1 means failed to stop threads.
  int SuspendAllThreads();

  // Resume all the suspended threads.
  void ResumeAllThreads() const;

  // Get the count of threads in the process.
  // Return -1 means error.
  int GetThreadCount() const;

  // List the threads of process.
  // Whenever there is a thread found, the callback will be invoked to process
  // the information.
  // Return number of threads listed.
  int ListThreads(CallbackParam<ThreadCallback> *thread_callback_param) const;

  // Get the general purpose registers of a thread.
  // The caller must get the thread pid by ListThreads.
  bool GetRegisters(int pid, user_regs_struct *regs) const;

  // Get the floating-point registers of a thread.
  // The caller must get the thread pid by ListThreads.
  bool GetFPRegisters(int pid, user_fpregs_struct *regs) const;

  // Get all the extended floating-point registers. May not work on all
  // machines.
  // The caller must get the thread pid by ListThreads.
  bool GetFPXRegisters(int pid, user_fpxregs_struct *regs) const;

  // Get the debug registers.
  // The caller must get the thread pid by ListThreads.
  bool GetDebugRegisters(int pid, DebugRegs *regs) const;

  // Get the stack memory dump.
  int GetThreadStackDump(uintptr_t current_ebp,
                         uintptr_t current_esp,
                         void *buf,
                         int buf_size) const;

  // Get the module count of the current process.
  int GetModuleCount() const;

  // Get the mapped modules in the address space.
  // Whenever a module is found, the callback will be invoked to process the
  // information.
  // Return how may modules are found.
  int ListModules(CallbackParam<ModuleCallback> *callback_param) const;

  // Get the bottom of the stack from ebp.
  uintptr_t GetThreadStackBottom(uintptr_t current_esp) const;

 private:
  // This callback will run when a new thread has been found.
  typedef bool (*PidCallback)(int pid, void *context);

  // Read thread information from /proc/$pid/task.
  // Whenever a thread has been found, and callback will be invoked with
  // the pid of the thread.
  // Return number of threads found.
  // Return -1 means the directory doesn't exist.
  int IterateProcSelfTask(int pid,
                          CallbackParam<PidCallback> *callback_param) const;

  // Check if the address is a valid virtual address.
  bool IsAddressMapped(uintptr_t address) const;

 private:
  // The pid of the process we are listing threads.
  int pid_;

  // Mark if we have suspended the threads.
  bool threads_suspened_;
};

}  // namespace google_breakpad

#endif  // CLIENT_LINUX_HANDLER_LINUX_THREAD_H__
