pub const MD_CONTEXT_ARM_GPR_COUNT: usize = 16;

pub const MD_FLOATINGSAVEAREA_ARM_FPR_COUNT: usize = 32;
pub const MD_FLOATINGSAVEAREA_ARM_FPEXTRA_COUNT: usize = 8;
/*
 * Note that these structures *do not* map directly to the CONTEXT
 * structure defined in WinNT.h in the Windows Mobile SDK. That structure
 * does not accomodate VFPv3, and I'm unsure if it was ever used in the
 * wild anyway, as Windows CE only seems to produce "cedumps" which
 * are not exactly minidumps.
 */
#[repr(C)]
pub struct MDFloatingSaveAreaARM {
    pub fpscr: u64, /* FPU status register */

    /* 32 64-bit floating point registers, d0 .. d31. */
    pub regs: [u64; MD_FLOATINGSAVEAREA_ARM_FPR_COUNT],

    /* Miscellaneous control words */
    pub extra: [u32; MD_FLOATINGSAVEAREA_ARM_FPEXTRA_COUNT],
}

// The std library doesn't provide "Default" for all
// array-lengths. Only up to 32. So we have to implement
// our own default, because of `reserved4: [u8; 96]`
impl Default for MDFloatingSaveAreaARM {
    #[inline]
    fn default() -> Self {
        MDFloatingSaveAreaARM {
            fpscr: 0,
            regs: [0; MD_FLOATINGSAVEAREA_ARM_FPR_COUNT],
            extra: [0; MD_FLOATINGSAVEAREA_ARM_FPEXTRA_COUNT],
        }
    }
}

#[repr(C)]
pub struct MDRawContextARM {
    /* The next field determines the layout of the structure, and which parts
     * of it are populated
     */
    pub context_flags: u32,

    /* 16 32-bit integer registers, r0 .. r15
     * Note the following fixed uses:
     *   r13 is the stack pointer
     *   r14 is the link register
     *   r15 is the program counter
     */
    pub iregs: [u32; MD_CONTEXT_ARM_GPR_COUNT],

    /* CPSR (flags, basically): 32 bits:
       bit 31 - N (negative)
       bit 30 - Z (zero)
       bit 29 - C (carry)
       bit 28 - V (overflow)
       bit 27 - Q (saturation flag, sticky)
    All other fields -- ignore */
    pub cpsr: u32,

    /* The next field is included with MD_CONTEXT_ARM_FLOATING_POINT */
    pub float_save: MDFloatingSaveAreaARM,
}

impl Default for MDRawContextARM {
    #[inline]
    fn default() -> Self {
        MDRawContextARM {
            context_flags: 0,
            iregs: [0; MD_CONTEXT_ARM_GPR_COUNT],
            cpsr: 0,
            float_save: Default::default(),
        }
    }
}

/* Indices into iregs for registers with a dedicated or conventional
 * purpose.
 */
// enum MDARMRegisterNumbers {
//   MD_CONTEXT_ARM_REG_IOS_FP = 7,
//   MD_CONTEXT_ARM_REG_FP     = 11,
//   MD_CONTEXT_ARM_REG_SP     = 13,
//   MD_CONTEXT_ARM_REG_LR     = 14,
//   MD_CONTEXT_ARM_REG_PC     = 15
// }

/* For (MDRawContextARM).context_flags.  These values indicate the type of
 * context stored in the structure. */
/* CONTEXT_ARM from the Windows CE 5.0 SDK. This value isn't correct
 * because this bit can be used for flags. Presumably this value was
 * never actually used in minidumps, but only in "CEDumps" which
 * are a whole parallel minidump file format for Windows CE.
 * Therefore, Breakpad defines its own value for ARM CPUs.
 */
pub const MD_CONTEXT_ARM_OLD: u32 = 0x00000040;
/* This value was chosen to avoid likely conflicts with MD_CONTEXT_*
 * for other CPUs. */
pub const MD_CONTEXT_ARM: u32 = 0x40000000;
pub const MD_CONTEXT_ARM_INTEGER: u32 = MD_CONTEXT_ARM | 0x00000002;
pub const MD_CONTEXT_ARM_FLOATING_POINT: u32 = MD_CONTEXT_ARM | 0x00000004;
pub const MD_CONTEXT_ARM_FULL: u32 = MD_CONTEXT_ARM_INTEGER | MD_CONTEXT_ARM_FLOATING_POINT;
pub const MD_CONTEXT_ARM_ALL: u32 = MD_CONTEXT_ARM_INTEGER | MD_CONTEXT_ARM_FLOATING_POINT;
