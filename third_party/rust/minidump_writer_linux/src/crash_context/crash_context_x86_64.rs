use super::CrashContext;
use crate::minidump_cpu::imp::*;
use crate::minidump_cpu::RawContextCPU;
use crate::thread_info::to_u128;
use libc::{
    REG_CSGSFS, REG_EFL, REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15, REG_R8, REG_R9,
    REG_RAX, REG_RBP, REG_RBX, REG_RCX, REG_RDI, REG_RDX, REG_RIP, REG_RSI, REG_RSP,
};

impl CrashContext {
    pub fn get_instruction_pointer(&self) -> usize {
        self.context.uc_mcontext.gregs[REG_RIP as usize] as usize
    }

    pub fn get_stack_pointer(&self) -> usize {
        self.context.uc_mcontext.gregs[REG_RSP as usize] as usize
    }

    pub fn fill_cpu_context(&self, out: &mut RawContextCPU) {
        out.context_flags = MD_CONTEXT_AMD64_FULL;
        out.cs = (self.context.uc_mcontext.gregs[REG_CSGSFS as usize] & 0xffff) as u16;

        out.fs = ((self.context.uc_mcontext.gregs[REG_CSGSFS as usize] >> 32) & 0xffff) as u16;
        out.gs = ((self.context.uc_mcontext.gregs[REG_CSGSFS as usize] >> 16) & 0xffff) as u16;

        out.eflags = self.context.uc_mcontext.gregs[REG_EFL as usize] as u32;

        out.rax = self.context.uc_mcontext.gregs[REG_RAX as usize] as u64;
        out.rcx = self.context.uc_mcontext.gregs[REG_RCX as usize] as u64;
        out.rdx = self.context.uc_mcontext.gregs[REG_RDX as usize] as u64;
        out.rbx = self.context.uc_mcontext.gregs[REG_RBX as usize] as u64;

        out.rsp = self.context.uc_mcontext.gregs[REG_RSP as usize] as u64;
        out.rbp = self.context.uc_mcontext.gregs[REG_RBP as usize] as u64;
        out.rsi = self.context.uc_mcontext.gregs[REG_RSI as usize] as u64;
        out.rdi = self.context.uc_mcontext.gregs[REG_RDI as usize] as u64;
        out.r8 = self.context.uc_mcontext.gregs[REG_R8 as usize] as u64;
        out.r9 = self.context.uc_mcontext.gregs[REG_R9 as usize] as u64;
        out.r10 = self.context.uc_mcontext.gregs[REG_R10 as usize] as u64;
        out.r11 = self.context.uc_mcontext.gregs[REG_R11 as usize] as u64;
        out.r12 = self.context.uc_mcontext.gregs[REG_R12 as usize] as u64;
        out.r13 = self.context.uc_mcontext.gregs[REG_R13 as usize] as u64;
        out.r14 = self.context.uc_mcontext.gregs[REG_R14 as usize] as u64;
        out.r15 = self.context.uc_mcontext.gregs[REG_R15 as usize] as u64;

        out.rip = self.context.uc_mcontext.gregs[REG_RIP as usize] as u64;

        out.flt_save.control_word = self.float_state.cwd;
        out.flt_save.status_word = self.float_state.swd;
        out.flt_save.tag_word = self.float_state.ftw as u8;
        out.flt_save.error_opcode = self.float_state.fop;
        out.flt_save.error_offset = self.float_state.rip as u32;
        out.flt_save.data_offset = self.float_state.rdp as u32;
        out.flt_save.error_selector = 0; // We don't have this.
        out.flt_save.data_selector = 0; // We don't have this.
        out.flt_save.mx_csr = self.float_state.mxcsr;
        out.flt_save.mx_csr_mask = self.float_state.mxcr_mask;

        let data = to_u128(&self.float_state.st_space);
        for idx in 0..data.len() {
            out.flt_save.float_registers[idx] = data[idx];
        }

        let data = to_u128(&self.float_state.xmm_space);
        for idx in 0..data.len() {
            out.flt_save.xmm_registers[idx] = data[idx];
        }
    }
}
