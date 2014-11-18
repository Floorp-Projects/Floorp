// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/seccomp-bpf/trap.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#include <limits>

#include "base/logging.h"
#include "sandbox/linux/seccomp-bpf/codegen.h"
#include "sandbox/linux/seccomp-bpf/die.h"
#include "sandbox/linux/seccomp-bpf/syscall.h"

// Android's signal.h doesn't define ucontext etc.
#if defined(OS_ANDROID)
#include "sandbox/linux/services/android_ucontext.h"
#endif

namespace {

const int kCapacityIncrement = 20;

// Unsafe traps can only be turned on, if the user explicitly allowed them
// by setting the CHROME_SANDBOX_DEBUGGING environment variable.
const char kSandboxDebuggingEnv[] = "CHROME_SANDBOX_DEBUGGING";

// We need to tell whether we are performing a "normal" callback, or
// whether we were called recursively from within a UnsafeTrap() callback.
// This is a little tricky to do, because we need to somehow get access to
// per-thread data from within a signal context. Normal TLS storage is not
// safely accessible at this time. We could roll our own, but that involves
// a lot of complexity. Instead, we co-opt one bit in the signal mask.
// If BUS is blocked, we assume that we have been called recursively.
// There is a possibility for collision with other code that needs to do
// this, but in practice the risks are low.
// If SIGBUS turns out to be a problem, we could instead co-opt one of the
// realtime signals. There are plenty of them. Unfortunately, there is no
// way to mark a signal as allocated. So, the potential for collision is
// possibly even worse.
bool GetIsInSigHandler(const ucontext_t* ctx) {
  // Note: on Android, sigismember does not take a pointer to const.
  return sigismember(const_cast<sigset_t*>(&ctx->uc_sigmask), SIGBUS);
}

void SetIsInSigHandler() {
  sigset_t mask;
  if (sigemptyset(&mask) || sigaddset(&mask, SIGBUS) ||
      sigprocmask(SIG_BLOCK, &mask, NULL)) {
    SANDBOX_DIE("Failed to block SIGBUS");
  }
}

bool IsDefaultSignalAction(const struct sigaction& sa) {
  if (sa.sa_flags & SA_SIGINFO || sa.sa_handler != SIG_DFL) {
    return false;
  }
  return true;
}

}  // namespace

namespace sandbox {

Trap::Trap()
    : trap_array_(NULL),
      trap_array_size_(0),
      trap_array_capacity_(0),
      has_unsafe_traps_(false) {
  // Set new SIGSYS handler
  struct sigaction sa = {};
  sa.sa_sigaction = SigSysAction;
  sa.sa_flags = SA_SIGINFO | SA_NODEFER;
  struct sigaction old_sa;
  if (sigaction(SIGSYS, &sa, &old_sa) < 0) {
    SANDBOX_DIE("Failed to configure SIGSYS handler");
  }

  if (!IsDefaultSignalAction(old_sa)) {
    static const char kExistingSIGSYSMsg[] =
        "Existing signal handler when trying to install SIGSYS. SIGSYS needs "
        "to be reserved for seccomp-bpf.";
    DLOG(FATAL) << kExistingSIGSYSMsg;
    LOG(ERROR) << kExistingSIGSYSMsg;
  }

  // Unmask SIGSYS
  sigset_t mask;
  if (sigemptyset(&mask) || sigaddset(&mask, SIGSYS) ||
      sigprocmask(SIG_UNBLOCK, &mask, NULL)) {
    SANDBOX_DIE("Failed to configure SIGSYS handler");
  }
}

Trap* Trap::GetInstance() {
  // Note: This class is not thread safe. It is the caller's responsibility
  // to avoid race conditions. Normally, this is a non-issue as the sandbox
  // can only be initialized if there are no other threads present.
  // Also, this is not a normal singleton. Once created, the global trap
  // object must never be destroyed again.
  if (!global_trap_) {
    global_trap_ = new Trap();
    if (!global_trap_) {
      SANDBOX_DIE("Failed to allocate global trap handler");
    }
  }
  return global_trap_;
}

void Trap::SigSysAction(int nr, siginfo_t* info, void* void_context) {
  if (!global_trap_) {
    RAW_SANDBOX_DIE(
        "This can't happen. Found no global singleton instance "
        "for Trap() handling.");
  }
  global_trap_->SigSys(nr, info, void_context);
}

void Trap::SigSys(int nr, siginfo_t* info, void* void_context) {
  // Signal handlers should always preserve "errno". Otherwise, we could
  // trigger really subtle bugs.
  const int old_errno = errno;

  // Various sanity checks to make sure we actually received a signal
  // triggered by a BPF filter. If something else triggered SIGSYS
  // (e.g. kill()), there is really nothing we can do with this signal.
  if (nr != SIGSYS || info->si_code != SYS_SECCOMP || !void_context ||
      info->si_errno <= 0 ||
      static_cast<size_t>(info->si_errno) > trap_array_size_) {
    // ATI drivers seem to send SIGSYS, so this cannot be FATAL.
    // See crbug.com/178166.
    // TODO(jln): add a DCHECK or move back to FATAL.
    RAW_LOG(ERROR, "Unexpected SIGSYS received.");
    errno = old_errno;
    return;
  }

  // Obtain the signal context. This, most notably, gives us access to
  // all CPU registers at the time of the signal.
  ucontext_t* ctx = reinterpret_cast<ucontext_t*>(void_context);

  // Obtain the siginfo information that is specific to SIGSYS. Unfortunately,
  // most versions of glibc don't include this information in siginfo_t. So,
  // we need to explicitly copy it into a arch_sigsys structure.
  struct arch_sigsys sigsys;
  memcpy(&sigsys, &info->_sifields, sizeof(sigsys));

  // Some more sanity checks.
  if (sigsys.ip != reinterpret_cast<void*>(SECCOMP_IP(ctx)) ||
      sigsys.nr != static_cast<int>(SECCOMP_SYSCALL(ctx)) ||
      sigsys.arch != SECCOMP_ARCH) {
    // TODO(markus):
    // SANDBOX_DIE() can call LOG(FATAL). This is not normally async-signal
    // safe and can lead to bugs. We should eventually implement a different
    // logging and reporting mechanism that is safe to be called from
    // the sigSys() handler.
    RAW_SANDBOX_DIE("Sanity checks are failing after receiving SIGSYS.");
  }

  intptr_t rc;
  if (has_unsafe_traps_ && GetIsInSigHandler(ctx)) {
    errno = old_errno;
    if (sigsys.nr == __NR_clone) {
      RAW_SANDBOX_DIE("Cannot call clone() from an UnsafeTrap() handler.");
    }
    rc = Syscall::Call(sigsys.nr,
                       SECCOMP_PARM1(ctx),
                       SECCOMP_PARM2(ctx),
                       SECCOMP_PARM3(ctx),
                       SECCOMP_PARM4(ctx),
                       SECCOMP_PARM5(ctx),
                       SECCOMP_PARM6(ctx));
  } else {
    const ErrorCode& err = trap_array_[info->si_errno - 1];
    if (!err.safe_) {
      SetIsInSigHandler();
    }

    // Copy the seccomp-specific data into a arch_seccomp_data structure. This
    // is what we are showing to TrapFnc callbacks that the system call
    // evaluator registered with the sandbox.
    struct arch_seccomp_data data = {
        sigsys.nr, SECCOMP_ARCH, reinterpret_cast<uint64_t>(sigsys.ip),
        {static_cast<uint64_t>(SECCOMP_PARM1(ctx)),
         static_cast<uint64_t>(SECCOMP_PARM2(ctx)),
         static_cast<uint64_t>(SECCOMP_PARM3(ctx)),
         static_cast<uint64_t>(SECCOMP_PARM4(ctx)),
         static_cast<uint64_t>(SECCOMP_PARM5(ctx)),
         static_cast<uint64_t>(SECCOMP_PARM6(ctx))}};

    // Now call the TrapFnc callback associated with this particular instance
    // of SECCOMP_RET_TRAP.
    rc = err.fnc_(data, err.aux_);
  }

  // Update the CPU register that stores the return code of the system call
  // that we just handled, and restore "errno" to the value that it had
  // before entering the signal handler.
  SECCOMP_RESULT(ctx) = static_cast<greg_t>(rc);
  errno = old_errno;

  return;
}

bool Trap::TrapKey::operator<(const TrapKey& o) const {
  if (fnc != o.fnc) {
    return fnc < o.fnc;
  } else if (aux != o.aux) {
    return aux < o.aux;
  } else {
    return safe < o.safe;
  }
}

ErrorCode Trap::MakeTrap(TrapFnc fnc, const void* aux, bool safe) {
  return GetInstance()->MakeTrapImpl(fnc, aux, safe);
}

ErrorCode Trap::MakeTrapImpl(TrapFnc fnc, const void* aux, bool safe) {
  if (!safe && !SandboxDebuggingAllowedByUser()) {
    // Unless the user set the CHROME_SANDBOX_DEBUGGING environment variable,
    // we never return an ErrorCode that is marked as "unsafe". This also
    // means, the BPF compiler will never emit code that allow unsafe system
    // calls to by-pass the filter (because they use the magic return address
    // from Syscall::Call(-1)).

    // This SANDBOX_DIE() can optionally be removed. It won't break security,
    // but it might make error messages from the BPF compiler a little harder
    // to understand. Removing the SANDBOX_DIE() allows callers to easyly check
    // whether unsafe traps are supported (by checking whether the returned
    // ErrorCode is ET_INVALID).
    SANDBOX_DIE(
        "Cannot use unsafe traps unless CHROME_SANDBOX_DEBUGGING "
        "is enabled");

    return ErrorCode();
  }

  // Each unique pair of TrapFnc and auxiliary data make up a distinct instance
  // of a SECCOMP_RET_TRAP.
  TrapKey key(fnc, aux, safe);
  TrapIds::const_iterator iter = trap_ids_.find(key);

  // We return unique identifiers together with SECCOMP_RET_TRAP. This allows
  // us to associate trap with the appropriate handler. The kernel allows us
  // identifiers in the range from 0 to SECCOMP_RET_DATA (0xFFFF). We want to
  // avoid 0, as it could be confused for a trap without any specific id.
  // The nice thing about sequentially numbered identifiers is that we can also
  // trivially look them up from our signal handler without making any system
  // calls that might be async-signal-unsafe.
  // In order to do so, we store all of our traps in a C-style trap_array_.
  uint16_t id;
  if (iter != trap_ids_.end()) {
    // We have seen this pair before. Return the same id that we assigned
    // earlier.
    id = iter->second;
  } else {
    // This is a new pair. Remember it and assign a new id.
    if (trap_array_size_ >= SECCOMP_RET_DATA /* 0xFFFF */ ||
        trap_array_size_ >= std::numeric_limits<typeof(id)>::max()) {
      // In practice, this is pretty much impossible to trigger, as there
      // are other kernel limitations that restrict overall BPF program sizes.
      SANDBOX_DIE("Too many SECCOMP_RET_TRAP callback instances");
    }
    id = trap_array_size_ + 1;

    // Our callers ensure that there are no other threads accessing trap_array_
    // concurrently (typically this is done by ensuring that we are single-
    // threaded while the sandbox is being set up). But we nonetheless are
    // modifying a life data structure that could be accessed any time a
    // system call is made; as system calls could be triggering SIGSYS.
    // So, we have to be extra careful that we update trap_array_ atomically.
    // In particular, this means we shouldn't be using realloc() to resize it.
    // Instead, we allocate a new array, copy the values, and then switch the
    // pointer. We only really care about the pointer being updated atomically
    // and the data that is pointed to being valid, as these are the only
    // values accessed from the signal handler. It is OK if trap_array_size_
    // is inconsistent with the pointer, as it is monotonously increasing.
    // Also, we only care about compiler barriers, as the signal handler is
    // triggered synchronously from a system call. We don't have to protect
    // against issues with the memory model or with completely asynchronous
    // events.
    if (trap_array_size_ >= trap_array_capacity_) {
      trap_array_capacity_ += kCapacityIncrement;
      ErrorCode* old_trap_array = trap_array_;
      ErrorCode* new_trap_array = new ErrorCode[trap_array_capacity_];

      // Language specs are unclear on whether the compiler is allowed to move
      // the "delete[]" above our preceding assignments and/or memory moves,
      // iff the compiler believes that "delete[]" doesn't have any other
      // global side-effects.
      // We insert optimization barriers to prevent this from happening.
      // The first barrier is probably not needed, but better be explicit in
      // what we want to tell the compiler.
      // The clang developer mailing list couldn't answer whether this is a
      // legitimate worry; but they at least thought that the barrier is
      // sufficient to prevent the (so far hypothetical) problem of re-ordering
      // of instructions by the compiler.
      memcpy(new_trap_array, trap_array_, trap_array_size_ * sizeof(ErrorCode));
      asm volatile("" : "=r"(new_trap_array) : "0"(new_trap_array) : "memory");
      trap_array_ = new_trap_array;
      asm volatile("" : "=r"(trap_array_) : "0"(trap_array_) : "memory");

      delete[] old_trap_array;
    }
    trap_ids_[key] = id;
    trap_array_[trap_array_size_] = ErrorCode(fnc, aux, safe, id);
    return trap_array_[trap_array_size_++];
  }

  return ErrorCode(fnc, aux, safe, id);
}

bool Trap::SandboxDebuggingAllowedByUser() const {
  const char* debug_flag = getenv(kSandboxDebuggingEnv);
  return debug_flag && *debug_flag;
}

bool Trap::EnableUnsafeTrapsInSigSysHandler() {
  Trap* trap = GetInstance();
  if (!trap->has_unsafe_traps_) {
    // Unsafe traps are a one-way fuse. Once enabled, they can never be turned
    // off again.
    // We only allow enabling unsafe traps, if the user explicitly set an
    // appropriate environment variable. This prevents bugs that accidentally
    // disable all sandboxing for all users.
    if (trap->SandboxDebuggingAllowedByUser()) {
      // We only ever print this message once, when we enable unsafe traps the
      // first time.
      SANDBOX_INFO("WARNING! Disabling sandbox for debugging purposes");
      trap->has_unsafe_traps_ = true;
    } else {
      SANDBOX_INFO(
          "Cannot disable sandbox and use unsafe traps unless "
          "CHROME_SANDBOX_DEBUGGING is turned on first");
    }
  }
  // Returns the, possibly updated, value of has_unsafe_traps_.
  return trap->has_unsafe_traps_;
}

ErrorCode Trap::ErrorCodeFromTrapId(uint16_t id) {
  if (global_trap_ && id > 0 && id <= global_trap_->trap_array_size_) {
    return global_trap_->trap_array_[id - 1];
  } else {
    return ErrorCode();
  }
}

Trap* Trap::global_trap_;

}  // namespace sandbox
