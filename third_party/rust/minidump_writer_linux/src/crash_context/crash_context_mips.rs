use super::CrashContext;
use libc::{greg_t, MD_CONTEXT_MIPS_REG_SP};

impl CrashContext {
    pub fn get_instruction_pointer(&self) -> greg_t {
        self.context.uc_mcontext.pc
    }

    pub fn get_stack_pointer(&self) -> greg_t {
        self.context.uc_mcontext.gregs[MD_CONTEXT_MIPS_REG_SP as usize]
    }
}
