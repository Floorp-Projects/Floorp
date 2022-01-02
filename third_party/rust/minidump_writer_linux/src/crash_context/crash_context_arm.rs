use super::CrashContext;
use crate::minidump_cpu::imp::*;
use crate::minidump_cpu::RawContextCPU;

impl CrashContext {
    pub fn get_instruction_pointer(&self) -> usize {
        self.context.uc_mcontext.arm_pc as usize
    }

    pub fn get_stack_pointer(&self) -> usize {
        self.context.uc_mcontext.arm_sp as usize
    }

    pub fn fill_cpu_context(&self, out: &mut RawContextCPU) {
        out.context_flags = MD_CONTEXT_ARM_FULL;
        out.iregs[0] = self.context.uc_mcontext.arm_r0;
        out.iregs[1] = self.context.uc_mcontext.arm_r1;
        out.iregs[2] = self.context.uc_mcontext.arm_r2;
        out.iregs[3] = self.context.uc_mcontext.arm_r3;
        out.iregs[4] = self.context.uc_mcontext.arm_r4;
        out.iregs[5] = self.context.uc_mcontext.arm_r5;
        out.iregs[6] = self.context.uc_mcontext.arm_r6;
        out.iregs[7] = self.context.uc_mcontext.arm_r7;
        out.iregs[8] = self.context.uc_mcontext.arm_r8;
        out.iregs[9] = self.context.uc_mcontext.arm_r9;
        out.iregs[10] = self.context.uc_mcontext.arm_r10;

        out.iregs[11] = self.context.uc_mcontext.arm_fp;
        out.iregs[12] = self.context.uc_mcontext.arm_ip;
        out.iregs[13] = self.context.uc_mcontext.arm_sp;
        out.iregs[14] = self.context.uc_mcontext.arm_lr;
        out.iregs[15] = self.context.uc_mcontext.arm_pc;

        out.cpsr = self.context.uc_mcontext.arm_cpsr;

        // TODO: fix this after fixing ExceptionHandler
        out.float_save.fpscr = 0;
        out.float_save.regs = [0; MD_FLOATINGSAVEAREA_ARM_FPR_COUNT];
        out.float_save.extra = [0; MD_FLOATINGSAVEAREA_ARM_FPEXTRA_COUNT];
    }
}
