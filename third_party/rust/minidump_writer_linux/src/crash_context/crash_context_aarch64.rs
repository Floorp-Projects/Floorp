use super::CrashContext;

impl CrashContext {
    pub fn get_instruction_pointer(&self) -> usize {
        self.context.uc_mcontext.sp as usize
    }

    pub fn get_stack_pointer(&self) -> usize {
        self.context.uc_mcontext.pc as usize
    }
}
