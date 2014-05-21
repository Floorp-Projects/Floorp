// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_TRAP_H__
#define SANDBOX_LINUX_SECCOMP_BPF_TRAP_H__

#include <signal.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "sandbox/linux/sandbox_export.h"

namespace sandbox {

class ErrorCode;

// The Trap class allows a BPF filter program to branch out to user space by
// raising a SIGSYS signal.
// N.B.: This class does not perform any synchronization operations. If
//   modifications are made to any of the traps, it is the caller's
//   responsibility to ensure that this happens in a thread-safe fashion.
//   Preferably, that means that no other threads should be running at that
//   time. For the purposes of our sandbox, this assertion should always be
//   true. Threads are incompatible with the seccomp sandbox anyway.
class SANDBOX_EXPORT Trap {
 public:
  // TrapFnc is a pointer to a function that handles Seccomp traps in
  // user-space. The seccomp policy can request that a trap handler gets
  // installed; it does so by returning a suitable ErrorCode() from the
  // syscallEvaluator. See the ErrorCode() constructor for how to pass in
  // the function pointer.
  // Please note that TrapFnc is executed from signal context and must be
  // async-signal safe:
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html
  // Also note that it follows the calling convention of native system calls.
  // In other words, it reports an error by returning an exit code in the
  // range -1..-4096. It should not set errno when reporting errors; on the
  // other hand, accidentally modifying errno is harmless and the changes will
  // be undone afterwards.
  typedef intptr_t (*TrapFnc)(const struct arch_seccomp_data& args, void* aux);

  // Registers a new trap handler and sets up the appropriate SIGSYS handler
  // as needed.
  // N.B.: This makes a permanent state change. Traps cannot be unregistered,
  //   as that would break existing BPF filters that are still active.
  static ErrorCode MakeTrap(TrapFnc fnc, const void* aux, bool safe);

  // Enables support for unsafe traps in the SIGSYS signal handler. This is a
  // one-way fuse. It works in conjunction with the BPF compiler emitting code
  // that unconditionally allows system calls, if they have a magic return
  // address (i.e. SandboxSyscall(-1)).
  // Once unsafe traps are enabled, the sandbox is essentially compromised.
  // But this is still a very useful feature for debugging purposes. Use with
  // care. This feature is availably only if enabled by the user (see above).
  // Returns "true", if unsafe traps were turned on.
  static bool EnableUnsafeTrapsInSigSysHandler();

  // Returns the ErrorCode associate with a particular trap id.
  static ErrorCode ErrorCodeFromTrapId(uint16_t id);

 private:
  // The destructor is unimplemented. Don't ever attempt to destruct this
  // object. It'll break subsequent system calls that trigger a SIGSYS.
  ~Trap();

  struct TrapKey {
    TrapKey(TrapFnc f, const void* a, bool s) : fnc(f), aux(a), safe(s) {}
    TrapFnc fnc;
    const void* aux;
    bool safe;
    bool operator<(const TrapKey&) const;
  };
  typedef std::map<TrapKey, uint16_t> TrapIds;

  // We only have a very small number of methods. We opt to make them static
  // and have them internally call GetInstance(). This is a little more
  // convenient than having each caller obtain short-lived reference to the
  // singleton.
  // It also gracefully deals with methods that should check for the singleton,
  // but avoid instantiating it, if it doesn't exist yet
  // (e.g. ErrorCodeFromTrapId()).
  static Trap* GetInstance();
  static void SigSysAction(int nr, siginfo_t* info, void* void_context);

  // Make sure that SigSys is not inlined in order to get slightly better crash
  // dumps.
  void SigSys(int nr, siginfo_t* info, void* void_context)
      __attribute__((noinline));
  ErrorCode MakeTrapImpl(TrapFnc fnc, const void* aux, bool safe);
  bool SandboxDebuggingAllowedByUser() const;

  // We have a global singleton that handles all of our SIGSYS traps. This
  // variable must never be deallocated after it has been set up initially, as
  // there is no way to reset in-kernel BPF filters that generate SIGSYS
  // events.
  static Trap* global_trap_;

  TrapIds trap_ids_;            // Maps from TrapKeys to numeric ids
  ErrorCode* trap_array_;       // Array of ErrorCodes indexed by ids
  size_t trap_array_size_;      // Currently used size of array
  size_t trap_array_capacity_;  // Currently allocated capacity of array
  bool has_unsafe_traps_;       // Whether unsafe traps have been enabled

  // Our constructor is private. A shared global instance is created
  // automatically as needed.
  // Copying and assigning is unimplemented. It doesn't make sense for a
  // singleton.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Trap);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_TRAP_H__
