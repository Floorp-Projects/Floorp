pub const MD_FLOATINGSAVEAREA_ARM64_FPR_COUNT: usize = 32;
pub const MD_CONTEXT_ARM64_GPR_COUNT: usize = 33;

/* Indices into iregs for registers with a dedicated or conventional
 * purpose.
 */
pub enum MDARM64RegisterNumbers {
    MD_CONTEXT_ARM64_REG_FP = 29,
    MD_CONTEXT_ARM64_REG_LR = 30,
    MD_CONTEXT_ARM64_REG_SP = 31,
    MD_CONTEXT_ARM64_REG_PC = 32,
}
