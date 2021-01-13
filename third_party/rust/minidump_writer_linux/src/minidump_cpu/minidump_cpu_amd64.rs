#[repr(C)]
pub struct MDXmmSaveArea32AMD64 {
    pub control_word: u16,
    pub status_word: u16,
    pub tag_word: u8,
    pub reserved1: u8,
    pub error_opcode: u16,
    pub error_offset: u32,
    pub error_selector: u16,
    pub reserved2: u16,
    pub data_offset: u32,
    pub data_selector: u16,
    pub reserved3: u16,
    pub mx_csr: u32,
    pub mx_csr_mask: u32,
    pub float_registers: [u128; 8],
    pub xmm_registers: [u128; 16],
    pub reserved4: [u8; 96],
}

// The std library doesn't provide "Default" for all
// array-lengths. Only up to 32. So we have to implement
// our own default, because of `reserved4: [u8; 96]`
impl Default for MDXmmSaveArea32AMD64 {
    #[inline]
    fn default() -> Self {
        MDXmmSaveArea32AMD64 {
            control_word: 0,
            status_word: 0,
            tag_word: 0,
            reserved1: 0,
            error_opcode: 0,
            error_offset: 0,
            error_selector: 0,
            reserved2: 0,
            data_offset: 0,
            data_selector: 0,
            reserved3: 0,
            mx_csr: 0,
            mx_csr_mask: 0,
            float_registers: [0; 8],
            xmm_registers: [0; 16],
            reserved4: [0; 96],
        }
    }
}

const MD_CONTEXT_AMD64_VR_COUNT: usize = 26;

#[repr(C)]
#[derive(Default)]
pub struct MDRawContextAMD64 {
    /*
     * Register parameter home addresses.
     */
    pub p1_home: u64,
    pub p2_home: u64,
    pub p3_home: u64,
    pub p4_home: u64,
    pub p5_home: u64,
    pub p6_home: u64,

    /* The next field determines the layout of the structure, and which parts
     * of it are populated */
    pub context_flags: u32,
    pub mx_csr: u32,

    /* The next register is included with MD_CONTEXT_AMD64_CONTROL */
    pub cs: u16,

    /* The next 4 registers are included with MD_CONTEXT_AMD64_SEGMENTS */
    pub ds: u16,
    pub es: u16,
    pub fs: u16,
    pub gs: u16,

    /* The next 2 registers are included with MD_CONTEXT_AMD64_CONTROL */
    pub ss: u16,
    pub eflags: u32,

    /* The next 6 registers are included with MD_CONTEXT_AMD64_DEBUG_REGISTERS */
    pub dr0: u64,
    pub dr1: u64,
    pub dr2: u64,
    pub dr3: u64,
    pub dr6: u64,
    pub dr7: u64,

    /* The next 4 registers are included with MD_CONTEXT_AMD64_INTEGER */
    pub rax: u64,
    pub rcx: u64,
    pub rdx: u64,
    pub rbx: u64,

    /* The next register is included with MD_CONTEXT_AMD64_CONTROL */
    pub rsp: u64,

    /* The next 11 registers are included with MD_CONTEXT_AMD64_INTEGER */
    pub rbp: u64,
    pub rsi: u64,
    pub rdi: u64,
    pub r8: u64,
    pub r9: u64,
    pub r10: u64,
    pub r11: u64,
    pub r12: u64,
    pub r13: u64,
    pub r14: u64,
    pub r15: u64,

    /* The next register is included with MD_CONTEXT_AMD64_CONTROL */
    pub rip: u64,

    /* The next set of registers are included with
     * MD_CONTEXT_AMD64_FLOATING_POINT
     */
    pub flt_save: MDXmmSaveArea32AMD64,
    // union {
    //   MDXmmSaveArea32AMD64 flt_save;
    //   struct {
    //     uint128_struct header[2];
    //     uint128_struct legacy[8];
    //     uint128_struct xmm0;
    //     uint128_struct xmm1;
    //     uint128_struct xmm2;
    //     uint128_struct xmm3;
    //     uint128_struct xmm4;
    //     uint128_struct xmm5;
    //     uint128_struct xmm6;
    //     uint128_struct xmm7;
    //     uint128_struct xmm8;
    //     uint128_struct xmm9;
    //     uint128_struct xmm10;
    //     uint128_struct xmm11;
    //     uint128_struct xmm12;
    //     uint128_struct xmm13;
    //     uint128_struct xmm14;
    //     uint128_struct xmm15;
    //   } sse_registers;
    // };
    pub vector_register: [u128; MD_CONTEXT_AMD64_VR_COUNT],
    pub vector_control: u64,

    /* The next 5 registers are included with MD_CONTEXT_AMD64_DEBUG_REGISTERS */
    pub debug_control: u64,
    pub last_branch_to_rip: u64,
    pub last_branch_from_rip: u64,
    pub last_exception_to_rip: u64,
    pub last_exception_from_rip: u64,
}

/* For (MDRawContextAMD64).context_flags.  These values indicate the type of
* context stored in the structure.  The high 24 bits identify the CPU, the
* low 8 bits identify the type of context saved. */
pub const MD_CONTEXT_AMD64: u32 = 0x00100000; /* CONTEXT_AMD64 */
pub const MD_CONTEXT_AMD64_CONTROL: u32 = MD_CONTEXT_AMD64 | 0x00000001;
/* CONTEXT_CONTROL */
pub const MD_CONTEXT_AMD64_INTEGER: u32 = MD_CONTEXT_AMD64 | 0x00000002;
/* CONTEXT_INTEGER */
pub const MD_CONTEXT_AMD64_SEGMENTS: u32 = MD_CONTEXT_AMD64 | 0x00000004;
/* CONTEXT_SEGMENTS */
pub const MD_CONTEXT_AMD64_FLOATING_POINT: u32 = MD_CONTEXT_AMD64 | 0x00000008;
/* CONTEXT_FLOATING_POINT */
pub const MD_CONTEXT_AMD64_DEBUG_REGISTERS: u32 = MD_CONTEXT_AMD64 | 0x00000010;
/* CONTEXT_DEBUG_REGISTERS */
pub const MD_CONTEXT_AMD64_XSTATE: u32 = MD_CONTEXT_AMD64 | 0x00000040;
/* CONTEXT_XSTATE */

/* WinNT.h refers to CONTEXT_MMX_REGISTERS but doesn't appear to define it
* I think it really means CONTEXT_FLOATING_POINT.
*/

pub const MD_CONTEXT_AMD64_FULL: u32 =
    MD_CONTEXT_AMD64_CONTROL | MD_CONTEXT_AMD64_INTEGER | MD_CONTEXT_AMD64_FLOATING_POINT;
/* CONTEXT_FULL */

pub const MD_CONTEXT_AMD64_ALL: u32 =
    MD_CONTEXT_AMD64_FULL | MD_CONTEXT_AMD64_SEGMENTS | MD_CONTEXT_AMD64_DEBUG_REGISTERS;
/* CONTEXT_ALL */
