use libc;

// Minidump defines register structures which are different from the raw
// structures which we get from the kernel. These are platform specific
// functions to juggle the ucontext_t and user structures into minidump format.

#[cfg(target_arch = "x86_64")]
#[path = "crash_context_x86_64.rs"]
pub mod imp;
#[cfg(target_arch = "x86")]
#[path = "crash_context_x86.rs"]
pub mod imp;
#[cfg(target_arch = "arm")]
#[path = "crash_context_arm.rs"]
pub mod imp;
#[cfg(target_arch = "aarch64")]
#[path = "crash_context_aarch64.rs"]
pub mod imp;
#[cfg(target_arch = "mips")]
#[path = "crash_context_mips.rs"]
pub mod imp;

#[cfg(target_arch = "aarch64")]
pub type fpstate_t = libc::fpsimd_context; // Currently not part of libc! This will produce an error.
#[cfg(not(any(
    target_arch = "aarch64",
    target_arch = "mips",
    target_arch = "arm-eabi"
)))]

#[cfg(target_arch = "x86")]
#[allow(non_camel_case_types)]
pub type fpstate_t = libc::_libc_fpstate;
#[cfg(target_arch = "x86_64")]
#[allow(non_camel_case_types)]
pub type fpstate_t = libc::user_fpregs_struct;

#[repr(C)]
#[derive(Clone)]
pub struct CrashContext {
    pub siginfo: libc::siginfo_t,
    pub tid: libc::pid_t, // the crashing thread.
    pub context: libc::ucontext_t,
    // #ifdef this out because FP state is not part of user ABI for Linux ARM.
    // In case of MIPS Linux FP state is already part of ucontext_t so
    // 'float_state' is not required.
    #[cfg(not(any(target_arch = "mips", target_arch = "arm-eabi")))]
    pub float_state: fpstate_t,
}
