pub const MD_FLOATINGSAVEAREA_X86_REGISTERAREA_SIZE: usize = 80;

#[repr(C)]
#[derive(Debug, PartialEq)]
pub struct MDFloatingSaveAreaX86 {
    pub control_word: u32,
    pub status_word: u32,
    pub tag_word: u32,
    pub error_offset: u32,
    pub error_selector: u32,
    pub data_offset: u32,
    pub data_selector: u32,

    /* register_area contains eight 80-bit (x87 "long double") quantities for
     * floating-point registers %st0 (%mm0) through %st7 (%mm7). */
    pub register_area: [u8; MD_FLOATINGSAVEAREA_X86_REGISTERAREA_SIZE],
    pub cr0_npx_state: u32,
}

// The std library doesn't provide "Default" for all
// array-lengths. Only up to 32. So we have to implement
// our own default, because of `reserved4: [u8; 96]`
impl Default for MDFloatingSaveAreaX86 {
    #[inline]
    fn default() -> Self {
        MDFloatingSaveAreaX86 {
            control_word: 0,
            status_word: 0,
            tag_word: 0,
            error_offset: 0,
            error_selector: 0,
            data_offset: 0,
            data_selector: 0,
            register_area: [0; MD_FLOATINGSAVEAREA_X86_REGISTERAREA_SIZE],
            cr0_npx_state: 0,
        }
    }
}

const MD_CONTEXT_X86_EXTENDED_REGISTERS_SIZE: usize = 512;
/* MAXIMUM_SUPPORTED_EXTENSION */

#[repr(C)]
#[derive(Debug, PartialEq)]
pub struct MDRawContextX86 {
    /* The next field determines the layout of the structure, and which parts
     * of it are populated */
    pub context_flags: u32,

    /* The next 6 registers are included with MD_CONTEXT_X86_DEBUG_REGISTERS */
    pub dr0: u32,
    pub dr1: u32,
    pub dr2: u32,
    pub dr3: u32,
    pub dr6: u32,
    pub dr7: u32,

    /* The next field is included with MD_CONTEXT_X86_FLOATING_POINT */
    pub float_save: MDFloatingSaveAreaX86,

    /* The next 4 registers are included with MD_CONTEXT_X86_SEGMENTS */
    pub gs: u32,
    pub fs: u32,
    pub es: u32,
    pub ds: u32,
    /* The next 6 registers are included with MD_CONTEXT_X86_INTEGER */
    pub edi: u32,
    pub esi: u32,
    pub ebx: u32,
    pub edx: u32,
    pub ecx: u32,
    pub eax: u32,

    /* The next 6 registers are included with MD_CONTEXT_X86_CONTROL */
    pub ebp: u32,
    pub eip: u32,
    pub cs: u32,     /* WinNT.h says "must be sanitized" */
    pub eflags: u32, /* WinNT.h says "must be sanitized" */
    pub esp: u32,
    pub ss: u32,

    /* The next field is included with MD_CONTEXT_X86_EXTENDED_REGISTERS.
     * It contains vector (MMX/SSE) registers.  It it laid out in the
     * format used by the fxsave and fsrstor instructions, so it includes
     * a copy of the x87 floating-point registers as well.  See FXSAVE in
     * "Intel Architecture Software Developer's Manual, Volume 2." */
    pub extended_registers: [u8; MD_CONTEXT_X86_EXTENDED_REGISTERS_SIZE],
}

impl Default for MDRawContextX86 {
    #[inline]
    fn default() -> Self {
        MDRawContextX86 {
            context_flags: 0,
            dr0: 0,
            dr1: 0,
            dr2: 0,
            dr3: 0,
            dr6: 0,
            dr7: 0,
            float_save: Default::default(),
            gs: 0,
            fs: 0,
            es: 0,
            ds: 0,
            edi: 0,
            esi: 0,
            ebx: 0,
            edx: 0,
            ecx: 0,
            eax: 0,
            ebp: 0,
            eip: 0,
            cs: 0,
            eflags: 0,
            esp: 0,
            ss: 0,
            extended_registers: [0; MD_CONTEXT_X86_EXTENDED_REGISTERS_SIZE],
        }
    }
}

/* For (MDRawContextX86).context_flags.  These values indicate the type of
 * context stored in the structure.  The high 24 bits identify the CPU, the
 * low 8 bits identify the type of context saved. */
pub const MD_CONTEXT_X86: u32 = 0x00010000;
/* CONTEXT_i386, CONTEXT_i486: identifies CPU */
pub const MD_CONTEXT_X86_CONTROL: u32 = MD_CONTEXT_X86 | 0x00000001;
/* CONTEXT_CONTROL */
pub const MD_CONTEXT_X86_INTEGER: u32 = MD_CONTEXT_X86 | 0x00000002;
/* CONTEXT_INTEGER */
pub const MD_CONTEXT_X86_SEGMENTS: u32 = MD_CONTEXT_X86 | 0x00000004;
/* CONTEXT_SEGMENTS */
pub const MD_CONTEXT_X86_FLOATING_POINT: u32 = MD_CONTEXT_X86 | 0x00000008;
/* CONTEXT_FLOATING_POINT */
pub const MD_CONTEXT_X86_DEBUG_REGISTERS: u32 = MD_CONTEXT_X86 | 0x00000010;
/* CONTEXT_DEBUG_REGISTERS */
pub const MD_CONTEXT_X86_EXTENDED_REGISTERS: u32 = MD_CONTEXT_X86 | 0x00000020;
/* CONTEXT_EXTENDED_REGISTERS */
pub const MD_CONTEXT_X86_XSTATE: u32 = MD_CONTEXT_X86 | 0x00000040;
/* CONTEXT_XSTATE */

pub const MD_CONTEXT_X86_FULL: u32 =
    MD_CONTEXT_X86_CONTROL | MD_CONTEXT_X86_INTEGER | MD_CONTEXT_X86_SEGMENTS;
/* CONTEXT_FULL */

pub const MD_CONTEXT_X86_ALL: u32 = MD_CONTEXT_X86_FULL
    | MD_CONTEXT_X86_FLOATING_POINT
    | MD_CONTEXT_X86_DEBUG_REGISTERS
    | MD_CONTEXT_X86_EXTENDED_REGISTERS;
/* CONTEXT_ALL */
