use super::CrashContext;
use libc::greg_t;

impl CrashContext {
    pub fn get_instruction_pointer(&self) -> greg_t {
        self.context.uc_mcontext.sp
    }

    pub fn get_stack_pointer(&self) -> greg_t {
        self.context.uc_mcontext.pc
    }
}
