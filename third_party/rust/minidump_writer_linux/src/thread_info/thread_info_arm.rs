use super::Pid;
use crate::minidump_cpu::imp::MD_CONTEXT_ARM_GPR_COUNT;
use libc;

#[derive(Debug)]
pub struct ThreadInfoArm {
    pub stack_pointer: libc::c_ulonglong,
    pub tgid: Pid, // thread group id
    pub ppid: Pid, // parent process
    pub regs: libc::user_regs,
    pub fpregs: libc::user_fpregs,
}

impl ThreadInfoArm {
    pub fn get_instruction_pointer(&self) -> libc::c_ulonglong {
        self.regs.uregs[15]
    }

    pub fn fill_cpu_context(&self, out: &mut RawContextCPU) {
        // out->context_flags = MD_CONTEXT_ARM_FULL;
        for idx in 0..MD_CONTEXT_ARM_GPR_COUNT {
            out.iregs[idx] = self.regs.uregs[idx];
        }
        // No CPSR register in ThreadInfo(it's not accessible via ptrace)
        out.cpsr = 0;

        #[not(cfg(target_os = "android"))]
        {
            out.float_save.fpscr = self.fpregs.fpsr | (self.fpregs.fpcr as u64) << 32;
            // TODO:
            // my_memset(&out->float_save.regs, 0, sizeof(out->float_save.regs));
            // my_memset(&out->float_save.extra, 0, sizeof(out->float_save.extra));
        }
    }
}
