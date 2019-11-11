use libc::{c_int, c_short, c_void, sigset_t, size_t};
#[derive(Clone, Copy, Debug)]
#[repr(C)]
pub struct sigaltstack {
    pub ss_sp: *const c_void,
    pub ss_size: size_t,
    pub ss_flags: c_int,
}

#[derive(Clone, Copy, Debug)]
#[repr(C)]
pub struct x86_exception_state64 {
    pub trapno: u16,
    pub cpu: u16,
    pub err: u32,
    pub faultvaddr: u64,
}

#[derive(Clone, Copy, Debug)]
#[repr(C)]
pub struct x86_thread_state64 {
    pub rax: u64,
    pub rbx: u64,
    pub rcx: u64,
    pub rdx: u64,
    pub rdi: u64,
    pub rsi: u64,
    pub rbp: u64,
    pub rsp: u64,
    pub r8: u64,
    pub r9: u64,
    pub r10: u64,
    pub r11: u64,
    pub r12: u64,
    pub r13: u64,
    pub r14: u64,
    pub r15: u64,
    pub rip: u64,
    pub rflags: u64,
    pub cs: u64,
    pub fs: u64,
    pub gs: u64,
}

#[derive(Clone, Copy, Debug)]
#[repr(C)]
pub struct mmst_reg {
    pub mmst_reg: [u8; 10],
    pub rsrv: [u8; 6],
}

#[derive(Clone, Copy, Debug)]
#[repr(C)]
pub struct xmm_reg([u8; 16]);

#[derive(Clone, Copy, Debug)]
#[repr(C)]
pub struct x86_float_state64 {
    pub fpu_reserved: [c_int; 2],
    pub fpu_fcw: c_short,
    pub fpu_fsw: c_short,
    pub fpu_ftw: u8,
    pub fpu_rsrv1: u8,
    pub fpu_fop: u16,
    pub fpu_ip: u32,
    pub fpu_cs: u16,
    pub fpu_rsrv2: u16,
    pub fpu_dp: u32,
    pub fpu_ds: u16,
    pub fpu_rsrv3: u16,
    pub fpu_mxcsr: u32,
    pub fpu_mxcsrmask: u32,
    pub fpu_stmm0: mmst_reg,
    pub fpu_stmm1: mmst_reg,
    pub fpu_stmm2: mmst_reg,
    pub fpu_stmm3: mmst_reg,
    pub fpu_stmm4: mmst_reg,
    pub fpu_stmm5: mmst_reg,
    pub fpu_stmm6: mmst_reg,
    pub fpu_stmm7: mmst_reg,
    pub fpu_xmm0: xmm_reg,
    pub fpu_xmm1: xmm_reg,
    pub fpu_xmm2: xmm_reg,
    pub fpu_xmm3: xmm_reg,
    pub fpu_xmm4: xmm_reg,
    pub fpu_xmm5: xmm_reg,
    pub fpu_xmm6: xmm_reg,
    pub fpu_xmm7: xmm_reg,
    pub fpu_xmm8: xmm_reg,
    pub fpu_xmm9: xmm_reg,
    pub fpu_xmm10: xmm_reg,
    pub fpu_xmm11: xmm_reg,
    pub fpu_xmm12: xmm_reg,
    pub fpu_xmm13: xmm_reg,
    pub fpu_xmm14: xmm_reg,
    pub fpu_xmm15: xmm_reg,
    pub fpu_rsrv4_0: [u8; 16],
    pub fpu_rsrv4_1: [u8; 16],
    pub fpu_rsrv4_2: [u8; 16],
    pub fpu_rsrv4_3: [u8; 16],
    pub fpu_rsrv4_4: [u8; 16],
    pub fpu_rsrv4_5: [u8; 16],
    pub fpu_reserved1: c_int,
}

#[derive(Clone, Copy, Debug)]
#[repr(C)]
pub struct mcontext64 {
    pub es: x86_exception_state64,
    pub ss: x86_thread_state64,
    pub fs: x86_float_state64,
}
#[derive(Clone, Copy, Debug)]
#[repr(C)]
pub struct ucontext_t {
    pub uc_onstack: c_int,
    pub uc_sigmask: sigset_t,
    pub uc_stack: sigaltstack,
    pub uc_link: *const ucontext_t,
    pub uc_mcsize: size_t,
    pub uc_mcontext: *const mcontext64,
}

#[derive(Clone, Copy, Debug)]
pub struct UContextPtr(*const ucontext_t);

impl UContextPtr {
    #[inline]
    pub fn new(ptr: *const c_void) -> Self {
        assert!(!ptr.is_null(), "non-null context");
        UContextPtr(ptr as *const ucontext_t)
    }

    #[inline]
    pub fn get_ip(self) -> *const c_void {
        let mcontext = &unsafe { *(*self.0).uc_mcontext };
        mcontext.ss.rip as *const _
    }
}

#[derive(Clone, Copy)]
#[repr(C)]
pub struct UContext {
    context: ucontext_t,
    mcontext: mcontext64,
}

impl UContext {
    #[inline]
    pub fn new(ptr: *const c_void) -> Self {
        let context = *unsafe {
            (ptr as *const ucontext_t)
                .as_ref()
                .expect("non-null context")
        };
        let mcontext = unsafe { *context.uc_mcontext };
        UContext { context, mcontext }
    }

    pub fn as_ptr(&mut self) -> UContextPtr {
        self.context.uc_mcontext = &self.mcontext;
        UContextPtr::new(&self.context as *const _ as *const _)
    }
}

impl Into<UContext> for UContextPtr {
    #[inline]
    fn into(self) -> UContext {
        UContext::new(self.0 as *const _)
    }
}
