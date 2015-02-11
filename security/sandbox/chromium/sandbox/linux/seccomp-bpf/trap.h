// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SECCOMP_BPF_TRAP_H__
#define SANDBOX_LINUX_SECCOMP_BPF_TRAP_H__

#include <signal.h>
#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// The Trap class allows a BPF filter program to branch out to user space by
// raising a SIGSYS signal.
// N.B.: This class does not perform any synchronization operations. If
//   modifications are made to any of the traps, it is the caller's
//   responsibility to ensure that this happens in a thread-safe fashion.
//   Preferably, that means that no other threads should be running at that
//   time. For the purposes of our sandbox, this assertion should always be
//   true. Threads are incompatible with the seccomp sandbox anyway.
class SANDBOX_EXPORT Trap : public bpf_dsl::TrapRegistry {
 public:
  virtual uint16_t Add(TrapFnc fnc, const void* aux, bool safe) override;

  virtual bool EnableUnsafeTraps() override;

  // Registry returns the trap registry used by Trap's SIGSYS handler,
  // creating it if necessary.
  static bpf_dsl::TrapRegistry* Registry();

  // Registers a new trap handler and sets up the appropriate SIGSYS handler
  // as needed.
  // N.B.: This makes a permanent state change. Traps cannot be unregistered,
  //   as that would break existing BPF filters that are still active.
  // TODO(mdempsky): Deprecated; remove.
  static uint16_t MakeTrap(TrapFnc fnc, const void* aux, bool safe);

  // Enables support for unsafe traps in the SIGSYS signal handler. This is a
  // one-way fuse. It works in conjunction with the BPF compiler emitting code
  // that unconditionally allows system calls, if they have a magic return
  // address (i.e. SandboxSyscall(-1)).
  // Once unsafe traps are enabled, the sandbox is essentially compromised.
  // But this is still a very useful feature for debugging purposes. Use with
  // care. This feature is availably only if enabled by the user (see above).
  // Returns "true", if unsafe traps were turned on.
  // TODO(mdempsky): Deprecated; remove.
  static bool EnableUnsafeTrapsInSigSysHandler();

 private:
  struct TrapKey {
    TrapKey() : fnc(NULL), aux(NULL), safe(false) {}
    TrapKey(TrapFnc f, const void* a, bool s) : fnc(f), aux(a), safe(s) {}
    TrapFnc fnc;
    const void* aux;
    bool safe;
    bool operator<(const TrapKey&) const;
  };
  typedef std::map<TrapKey, uint16_t> TrapIds;

  // Our constructor is private. A shared global instance is created
  // automatically as needed.
  Trap();

  // The destructor is unimplemented. Don't ever attempt to destruct this
  // object. It'll break subsequent system calls that trigger a SIGSYS.
  ~Trap();

  static void SigSysAction(int nr, siginfo_t* info, void* void_context);

  // Make sure that SigSys is not inlined in order to get slightly better crash
  // dumps.
  void SigSys(int nr, siginfo_t* info, void* void_context)
      __attribute__((noinline));
  bool SandboxDebuggingAllowedByUser() const;

  // We have a global singleton that handles all of our SIGSYS traps. This
  // variable must never be deallocated after it has been set up initially, as
  // there is no way to reset in-kernel BPF filters that generate SIGSYS
  // events.
  static Trap* global_trap_;

  TrapIds trap_ids_;            // Maps from TrapKeys to numeric ids
  TrapKey* trap_array_;         // Array of TrapKeys indexed by ids
  size_t trap_array_size_;      // Currently used size of array
  size_t trap_array_capacity_;  // Currently allocated capacity of array
  bool has_unsafe_traps_;       // Whether unsafe traps have been enabled

  // Copying and assigning is unimplemented. It doesn't make sense for a
  // singleton.
  DISALLOW_COPY_AND_ASSIGN(Trap);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SECCOMP_BPF_TRAP_H__
