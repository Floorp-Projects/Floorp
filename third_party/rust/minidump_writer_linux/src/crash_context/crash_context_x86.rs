use super::CrashContext;
use crate::minidump_cpu::imp::*;
use crate::minidump_cpu::RawContextCPU;
use libc::{
    REG_CS, REG_DS, REG_EAX, REG_EBP, REG_EBX, REG_ECX, REG_EDI, REG_EDX, REG_EFL, REG_EIP, REG_ES,
    REG_ESI, REG_ESP, REG_FS, REG_GS, REG_SS, REG_UESP,
};
impl CrashContext {
    pub fn get_instruction_pointer(&self) -> usize {
        self.context.uc_mcontext.gregs[REG_EIP as usize] as usize
    }

    pub fn get_stack_pointer(&self) -> usize {
        self.context.uc_mcontext.gregs[REG_ESP as usize] as usize
    }

    pub fn fill_cpu_context(&self, out: &mut RawContextCPU) {
        out.context_flags = MD_CONTEXT_X86_FULL | MD_CONTEXT_X86_FLOATING_POINT;

        out.gs = self.context.uc_mcontext.gregs[REG_GS as usize] as u32;
        out.fs = self.context.uc_mcontext.gregs[REG_FS as usize] as u32;
        out.es = self.context.uc_mcontext.gregs[REG_ES as usize] as u32;
        out.ds = self.context.uc_mcontext.gregs[REG_DS as usize] as u32;

        out.edi = self.context.uc_mcontext.gregs[REG_EDI as usize] as u32;
        out.esi = self.context.uc_mcontext.gregs[REG_ESI as usize] as u32;
        out.ebx = self.context.uc_mcontext.gregs[REG_EBX as usize] as u32;
        out.edx = self.context.uc_mcontext.gregs[REG_EDX as usize] as u32;
        out.ecx = self.context.uc_mcontext.gregs[REG_ECX as usize] as u32;
        out.eax = self.context.uc_mcontext.gregs[REG_EAX as usize] as u32;

        out.ebp = self.context.uc_mcontext.gregs[REG_EBP as usize] as u32;
        out.eip = self.context.uc_mcontext.gregs[REG_EIP as usize] as u32;
        out.cs = self.context.uc_mcontext.gregs[REG_CS as usize] as u32;
        out.eflags = self.context.uc_mcontext.gregs[REG_EFL as usize] as u32;
        out.esp = self.context.uc_mcontext.gregs[REG_UESP as usize] as u32;
        out.ss = self.context.uc_mcontext.gregs[REG_SS as usize] as u32;

        out.float_save.control_word = self.float_state.cw;
        out.float_save.status_word = self.float_state.sw;
        out.float_save.tag_word = self.float_state.tag;
        out.float_save.error_offset = self.float_state.ipoff;
        out.float_save.error_selector = self.float_state.cssel;
        out.float_save.data_offset = self.float_state.dataoff;
        out.float_save.data_selector = self.float_state.datasel;

        // 8 registers * 10 bytes per register.
        // my_memcpy(out->float_save.register_area, fp->_st, 10 * 8);
    }
}
