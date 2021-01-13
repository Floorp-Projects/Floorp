use super::CrashContext;
use libc::{greg_t, REG_EIP, REG_ESP};

impl CrashContext {
    pub fn get_instruction_pointer(&self) -> greg_t {
        self.context.uc_mcontext.gregs[REG_EIP as usize]
    }

    pub fn get_stack_pointer(&self) -> greg_t {
        self.context.uc_mcontext.gregs[REG_ESP as usize]
    }
}
