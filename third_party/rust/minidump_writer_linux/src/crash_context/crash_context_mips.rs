use super::CrashContext;
use libc::MD_CONTEXT_MIPS_REG_SP;

impl CrashContext {
    pub fn get_instruction_pointer(&self) -> usize {
        self.context.uc_mcontext.pc as usize
    }

    pub fn get_stack_pointer(&self) -> usize {
        self.context.uc_mcontext.gregs[MD_CONTEXT_MIPS_REG_SP as usize] as usize
    }
}
