use super::{CommonThreadInfo, Pid};
use crate::errors::ThreadInfoError;
use crate::minidump_cpu::imp::{
    MD_CONTEXT_ARM_FULL, MD_CONTEXT_ARM_GPR_COUNT, MD_FLOATINGSAVEAREA_ARM_FPEXTRA_COUNT,
    MD_FLOATINGSAVEAREA_ARM_FPR_COUNT,
};
use crate::minidump_cpu::RawContextCPU;
use libc;
use nix::sys::ptrace;

type Result<T> = std::result::Result<T, ThreadInfoError>;
// These are not (yet) part of the libc-crate
// #[repr(C)]
// #[derive(Debug, Eq, Hash, PartialEq, Copy, Clone, Default)]
// pub struct fp_reg {
//     // TODO: No bitfields at the moment, just the next best integer-type
//     sign1: u8,
//     unused: u16,
//     sign2: u8,
//     exponent: u16,
//     j: u8,
//     mantissa1: u32,
//     mantissa2: u32,
//     // unsigned int sign1:1;
//     // unsigned int unused:15;
//     // unsigned int sign2:1;
//     // unsigned int exponent:14;
//     // unsigned int j:1;
//     // unsigned int mantissa1:31;
//     // unsigned int mantissa0:32;
// }

#[repr(C)]
#[derive(Debug, Eq, Hash, PartialEq, Copy, Clone, Default)]
pub struct user_fpregs {
    // fpregs: [fp_reg; 8],
    fpregs: [u32; 8 * 3], // Fields not used, so shortening the struct to 3 x u32
    fpsr: u32,
    fpcr: u32,
    ftype: [u8; 8],
    init_flag: u32,
}

#[repr(C)]
#[derive(Debug, Eq, Hash, PartialEq, Copy, Clone, Default)]
pub struct user_regs {
    uregs: [libc::c_long; 18],
}

#[derive(Debug)]
pub struct ThreadInfoArm {
    pub stack_pointer: usize,
    pub tgid: Pid, // thread group id
    pub ppid: Pid, // parent process
    pub regs: user_regs,
    pub fpregs: user_fpregs,
}

impl CommonThreadInfo for ThreadInfoArm {}

impl ThreadInfoArm {
    // nix currently doesn't support PTRACE_GETFPREGS, so we have to do it ourselves
    fn getfpregs(pid: Pid) -> Result<user_fpregs> {
        Self::ptrace_get_data::<user_fpregs>(
            ptrace::Request::PTRACE_GETFPREGS,
            None,
            nix::unistd::Pid::from_raw(pid),
        )
    }

    // nix currently doesn't support PTRACE_GETFPREGS, so we have to do it ourselves
    fn getregs(pid: Pid) -> Result<user_regs> {
        Self::ptrace_get_data::<user_regs>(
            ptrace::Request::PTRACE_GETFPREGS,
            None,
            nix::unistd::Pid::from_raw(pid),
        )
    }

    pub fn get_instruction_pointer(&self) -> usize {
        self.regs.uregs[15] as usize
    }

    pub fn fill_cpu_context(&self, out: &mut RawContextCPU) {
        out.context_flags = MD_CONTEXT_ARM_FULL;
        for idx in 0..MD_CONTEXT_ARM_GPR_COUNT {
            out.iregs[idx] = self.regs.uregs[idx] as u32;
        }
        // No CPSR register in ThreadInfo(it's not accessible via ptrace)
        out.cpsr = 0;

        #[cfg(not(target_os = "android"))]
        {
            out.float_save.fpscr = self.fpregs.fpsr as u64 | ((self.fpregs.fpcr as u64) << 32);
            out.float_save.regs = [0; MD_FLOATINGSAVEAREA_ARM_FPR_COUNT];
            out.float_save.extra = [0; MD_FLOATINGSAVEAREA_ARM_FPEXTRA_COUNT];
        }
    }

    pub fn create_impl(_pid: Pid, tid: Pid) -> Result<Self> {
        let (ppid, tgid) = Self::get_ppid_and_tgid(tid)?;
        let regs = Self::getregs(tid)?;
        let fpregs = Self::getfpregs(tid)?;

        let stack_pointer = regs.uregs[13] as usize;

        Ok(ThreadInfoArm {
            stack_pointer,
            tgid,
            ppid,
            regs,
            fpregs,
        })
    }
}
