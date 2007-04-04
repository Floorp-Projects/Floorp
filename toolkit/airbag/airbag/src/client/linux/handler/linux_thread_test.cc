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

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "client/linux/handler/linux_thread.h"

using namespace google_breakpad;

// Thread use this to see if it should stop working.
static bool should_exit = false;

static void foo2(int *a) {
  // Stack variable, used for debugging stack dumps.
  int c = 0xcccccccc;
  c = c;
  while (!should_exit)
    sleep(1);
}

static void foo() {
  // Stack variable, used for debugging stack dumps.
  int a = 0xaaaaaaaa;
  foo2(&a);
}

static void *thread_main(void *) {
  // Stack variable, used for debugging stack dumps.
  int b = 0xbbbbbbbb;
  b = b;
  while (!should_exit) {
    foo();
  }
  return NULL;
}

static void CreateThreads(int num) {
  pthread_t handle;
  for (int i = 0; i < num; i++) {
    if (0 != pthread_create(&handle, NULL, thread_main, NULL))
      fprintf(stderr, "Failed to create thread.\n");
    else
      pthread_detach(handle);
  }
}

static bool ProcessOneModule(const struct ModuleInfo &module_info,
                             void *context) {
  printf("0x%x[%8d]         %s\n", module_info.start_addr, module_info.size,
         module_info.name);
  return true;
}

static bool ProcessOneThread(const struct ThreadInfo &thread_info,
                             void *context) {
  printf("\n\nPID: %d, TGID: %d, PPID: %d\n",
         thread_info.pid,
         thread_info.tgid,
         thread_info.ppid);

  struct user_regs_struct regs;
  struct user_fpregs_struct fp_regs;
  struct user_fpxregs_struct fpx_regs;
  struct DebugRegs dbg_regs;

  LinuxThread *threads = reinterpret_cast<LinuxThread *>(context);
  memset(&regs, 0, sizeof(regs));
  if (threads->GetRegisters(thread_info.pid, &regs)) {
    printf("  gs                           = 0x%lx\n", regs.xgs);
    printf("  fs                           = 0x%lx\n", regs.xfs);
    printf("  es                           = 0x%lx\n", regs.xes);
    printf("  ds                           = 0x%lx\n", regs.xds);
    printf("  edi                          = 0x%lx\n", regs.edi);
    printf("  esi                          = 0x%lx\n", regs.esi);
    printf("  ebx                          = 0x%lx\n", regs.ebx);
    printf("  edx                          = 0x%lx\n", regs.edx);
    printf("  ecx                          = 0x%lx\n", regs.ecx);
    printf("  eax                          = 0x%lx\n", regs.eax);
    printf("  ebp                          = 0x%lx\n", regs.ebp);
    printf("  eip                          = 0x%lx\n", regs.eip);
    printf("  cs                           = 0x%lx\n", regs.xcs);
    printf("  eflags                       = 0x%lx\n", regs.eflags);
    printf("  esp                          = 0x%lx\n", regs.esp);
    printf("  ss                           = 0x%lx\n", regs.xss);
  } else {
    fprintf(stderr, "ERROR: Failed to get general purpose registers\n");
  }
  memset(&fp_regs, 0, sizeof(fp_regs));
  if (threads->GetFPRegisters(thread_info.pid, &fp_regs)) {
    printf("\n Floating point registers:\n");
    printf("  fctl                         = 0x%lx\n", fp_regs.cwd);
    printf("  fstat                        = 0x%lx\n", fp_regs.swd);
    printf("  ftag                         = 0x%lx\n", fp_regs.twd);
    printf("  fioff                        = 0x%lx\n", fp_regs.fip);
    printf("  fiseg                        = 0x%lx\n", fp_regs.fcs);
    printf("  fooff                        = 0x%lx\n", fp_regs.foo);
    printf("  foseg                        = 0x%lx\n", fp_regs.fos);
    int st_space_size = sizeof(fp_regs.st_space) / sizeof(fp_regs.st_space[0]);
    printf("  st_space[%2d]                 = 0x", st_space_size);
    for (int i = 0; i < st_space_size; ++i)
      printf("%02lx", fp_regs.st_space[i]);
    printf("\n");
  } else {
    fprintf(stderr, "ERROR: Failed to get floating-point registers\n");
  }
  memset(&fpx_regs, 0, sizeof(fpx_regs));
  if (threads->GetFPXRegisters(thread_info.pid, &fpx_regs)) {
    printf("\n Extended floating point registers:\n");
    printf("  fctl                         = 0x%x\n", fpx_regs.cwd);
    printf("  fstat                        = 0x%x\n", fpx_regs.swd);
    printf("  ftag                         = 0x%x\n", fpx_regs.twd);
    printf("  fioff                        = 0x%lx\n", fpx_regs.fip);
    printf("  fiseg                        = 0x%lx\n", fpx_regs.fcs);
    printf("  fooff                        = 0x%lx\n", fpx_regs.foo);
    printf("  foseg                        = 0x%lx\n", fpx_regs.fos);
    printf("  fop                          = 0x%x\n", fpx_regs.fop);
    printf("  mxcsr                        = 0x%lx\n", fpx_regs.mxcsr);
    int space_size = sizeof(fpx_regs.st_space) / sizeof(fpx_regs.st_space[0]);
    printf("  st_space[%2d]                 = 0x", space_size);
    for (int i = 0; i < space_size; ++i)
      printf("%02lx", fpx_regs.st_space[i]);
    printf("\n");
    space_size = sizeof(fpx_regs.xmm_space) / sizeof(fpx_regs.xmm_space[0]);
    printf("  xmm_space[%2d]                = 0x", space_size);
    for (int i = 0; i < space_size; ++i)
      printf("%02lx", fpx_regs.xmm_space[i]);
    printf("\n");
  }
  if (threads->GetDebugRegisters(thread_info.pid, &dbg_regs)) {
    printf("\n Debug registers:\n");
    printf("  dr0                          = 0x%x\n", dbg_regs.dr0);
    printf("  dr1                          = 0x%x\n", dbg_regs.dr1);
    printf("  dr2                          = 0x%x\n", dbg_regs.dr2);
    printf("  dr3                          = 0x%x\n", dbg_regs.dr3);
    printf("  dr4                          = 0x%x\n", dbg_regs.dr4);
    printf("  dr5                          = 0x%x\n", dbg_regs.dr5);
    printf("  dr6                          = 0x%x\n", dbg_regs.dr6);
    printf("  dr7                          = 0x%x\n", dbg_regs.dr7);
    printf("\n");
  }
  if (regs.esp != 0) {
    // Print the stack content.
    int size = 1024 * 2;
    char *buf = new char[size];
    size = threads->GetThreadStackDump(regs.ebp,
                                       regs.esp,
                                      (void*)buf, size);
    printf(" Stack content:                 = 0x");
    size /= sizeof(unsigned long);
    unsigned long *p_buf = (unsigned long *)(buf);
    for (int i = 0; i < size; i += 1)
      printf("%.8lx ", p_buf[i]);
    delete []buf;
    printf("\n");
  }
  return true;
}

static int PrintAllThreads(void *argument) {
  int pid = (int)argument;

  LinuxThread threads(pid);
  int total_thread = threads.SuspendAllThreads();
  printf("There are %d threads in the process: %d\n", total_thread, pid);
  int total_module = threads.GetModuleCount();
  printf("There are %d modules in the process: %d\n", total_module, pid);
  CallbackParam<ModuleCallback> module_callback(ProcessOneModule, &threads);
  threads.ListModules(&module_callback);
  CallbackParam<ThreadCallback> thread_callback(ProcessOneThread, &threads);
  threads.ListThreads(&thread_callback);
  return 0;
}

int main(int argc, char **argv) {
  int pid = getpid();
  printf("Main thread is %d\n", pid);
  CreateThreads(1);
  // Create stack for the process.
  char *stack = new char[1024 * 100];
  int cloned_pid = clone(PrintAllThreads, stack + 1024 * 100,
                           CLONE_VM | CLONE_FILES | CLONE_FS | CLONE_UNTRACED,
                           (void*)getpid());
  waitpid(cloned_pid, NULL, __WALL);
  should_exit = true;
  printf("Test finished.\n");

  delete []stack;
  return 0;
}
