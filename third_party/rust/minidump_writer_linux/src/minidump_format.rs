#[repr(C)]
#[derive(Debug, Default, PartialEq)]
pub struct MDGUID {
    data1: u32,
    data2: u16,
    data3: u16,
    data4: [u8; 8],
}

#[repr(C)]
#[derive(Debug, Default, PartialEq, Clone, Copy)]
pub struct MDVSFixedFileInfo {
    pub signature: u32,
    pub struct_version: u32,
    pub file_version_hi: u32,
    pub file_version_lo: u32,
    pub product_version_hi: u32,
    pub product_version_lo: u32,
    pub file_flags_mask: u32, /* Identifies valid bits in fileFlags */
    pub file_flags: u32,
    pub file_os: u32,
    pub file_type: u32,
    pub file_subtype: u32,
    pub file_date_hi: u32,
    pub file_date_lo: u32,
}

/* An MDRVA is an offset into the minidump file.  The beginning of the
 * MDRawHeader is at offset 0. */
pub type MDRVA = u32;

#[repr(C)]
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct MDLocationDescriptor {
    pub data_size: u32,
    pub rva: MDRVA,
}

#[repr(C)]
#[derive(Debug, Default, Clone, PartialEq)]
pub struct MDMemoryDescriptor {
    /* The base address of the memory range on the host that produced the
     * minidump. */
    pub start_of_memory_range: u64,
    pub memory: MDLocationDescriptor,
}

#[repr(C)]
#[derive(Debug, Default, PartialEq)]
pub struct MDRawHeader {
    pub signature: u32,
    pub version: u32,
    pub stream_count: u32,
    pub stream_directory_rva: MDRVA, /* A |stream_count|-sized array of
                                      * MDRawDirectory structures. */
    pub checksum: u32,        /* Can be 0.  In fact, that's all that's
                               * been found in minidump files. */
    pub time_date_stamp: u32, /* time_t */
    pub flags: u64,
}

/* For (MDRawHeader).signature and (MDRawHeader).version.  Note that only the
 * low 16 bits of (MDRawHeader).version are MD_HEADER_VERSION.  Per the
 * documentation, the high 16 bits are implementation-specific. */
pub const MD_HEADER_SIGNATURE: u32 = 0x504d444d; /* 'PMDM' */
/* MINIDUMP_SIGNATURE */
pub const MD_HEADER_VERSION: u32 = 0x0000a793; /* 42899 */
/* MINIDUMP_VERSION */

#[repr(C)]
#[derive(Debug, Default, PartialEq)]
pub struct MDRawThread {
    pub thread_id: u32,
    pub suspend_count: u32,
    pub priority_class: u32,
    pub priority: u32,
    pub teb: u64, /* Thread environment block */
    pub stack: MDMemoryDescriptor,
    pub thread_context: MDLocationDescriptor, /* MDRawContext[CPU] */
}

pub type MDRawThreadList = Vec<MDRawThread>;

/* The inclusion of a 64-bit type in MINIDUMP_MODULE forces the struct to
 * be tail-padded out to a multiple of 64 bits under some ABIs (such as PPC).
 * This doesn't occur on systems that don't tail-pad in this manner.  Define
 * this macro to be the usable size of the MDRawModule struct, and use it in
 * place of sizeof(MDRawModule). */
// pub const MD_MODULE_SIZE: usize = 108;
// NOTE: We use "packed" here instead, to size_of::<MDRawModule>() == 108
//       "packed" should be safe here, as we don't address reserved{0,1} at all
//       and padding should happen only at the tail
#[repr(C, packed)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MDRawModule {
    pub base_of_image: u64,
    pub size_of_image: u32,
    pub checksum: u32,          /* 0 if unknown */
    pub time_date_stamp: u32,   /* time_t */
    pub module_name_rva: MDRVA, /* MDString, pathname or filename */
    pub version_info: MDVSFixedFileInfo,

    /* The next field stores a CodeView record and is populated when a module's
     * debug information resides in a PDB file.  It identifies the PDB file. */
    pub cv_record: MDLocationDescriptor,

    /* The next field is populated when a module's debug information resides
     * in a DBG file.  It identifies the DBG file.  This field is effectively
     * obsolete with modules built by recent toolchains. */
    pub misc_record: MDLocationDescriptor,

    /* Alignment problem: reserved0 and reserved1 are defined by the platform
     * SDK as 64-bit quantities.  However, that results in a structure whose
     * alignment is unpredictable on different CPUs and ABIs.  If the ABI
     * specifies full alignment of 64-bit quantities in structures (as ppc
     * does), there will be padding between miscRecord and reserved0.  If
     * 64-bit quantities can be aligned on 32-bit boundaries (as on x86),
     * this padding will not exist.  (Note that the structure up to this point
     * contains 1 64-bit member followed by 21 32-bit members.)
     * As a workaround, reserved0 and reserved1 are instead defined here as
     * four 32-bit quantities.  This should be harmless, as there are
     * currently no known uses for these fields. */
    pub reserved0: [u32; 2],
    pub reserved1: [u32; 2],
}

#[repr(C)]
#[derive(Debug, Default, PartialEq, Clone)]
pub struct MDRawDirectory {
    pub stream_type: u32,
    pub location: MDLocationDescriptor,
}

#[repr(C)]
#[derive(Debug, Default, PartialEq)]
pub struct MDException {
    pub exception_code: u32, /* Windows: MDExceptionCodeWin,
                              * Mac OS X: MDExceptionMac,
                              * Linux: MDExceptionCodeLinux. */
    pub exception_flags: u32, /* Windows: 1 if noncontinuable,
                              Mac OS X: MDExceptionCodeMac. */
    pub exception_record: u64, /* Address (in the minidump-producing host's
                                * memory) of another MDException, for
                                * nested exceptions. */
    pub exception_address: u64, /* The address that caused the exception.
                                 * Mac OS X: exception subcode (which is
                                 *           typically the address). */
    pub number_parameters: u32, /* Number of valid elements in
                                 * exception_information. */
    pub __align: u32,
    pub exception_information: [u64; 15],
}

#[repr(C)]
#[derive(Debug, Default, PartialEq)]
pub struct MDRawExceptionStream {
    pub thread_id: u32, /* Thread in which the exception
                         * occurred.  Corresponds to
                         * (MDRawThread).thread_id. */
    pub __align: u32,
    pub exception_record: MDException,
    pub thread_context: MDLocationDescriptor, /* MDRawContext[CPU] */
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[repr(C)]
#[derive(Debug, Default, PartialEq)]
pub struct MDCPUInformation {
    pub vendor_id: [u32; 3],            /* cpuid 0: ebx, edx, ecx */
    pub version_information: u32,       /* cpuid 1: eax */
    pub feature_information: u32,       /* cpuid 1: edx */
    pub amd_extended_cpu_features: u32, /* cpuid 0x80000001, ebx */
}

#[cfg(any(target_arch = "arm", target_arch = "aarch64"))]
#[repr(C)]
#[derive(Debug, Default, PartialEq)]
pub struct MDCPUInformation {
    pub cpuid: u32,
    pub elf_hwcaps: u32, /* linux specific, 0 otherwise */
    _padding: [u32; 4],
}

#[cfg(target_arch = "mips")]
#[repr(C)]
#[derive(Debug, Default, PartialEq)]
pub struct MDCPUInformation {
    pub cpuid: [u64; 2],
    _padding: [u32; 2],
}

/* For (MDCPUInformation).arm_cpu_info.elf_hwcaps.
 * This matches the Linux kernel definitions from <asm/hwcaps.h> */
#[repr(u32)]
pub enum MDCPUInformationARMElfHwCaps {
    Swp = 1 << 0,
    Half = 1 << 1,
    Thumb = 1 << 2,
    Bit26 = 1 << 3,
    FastMult = 1 << 4,
    Fpa = 1 << 5,
    Vfp = 1 << 6,
    Edsp = 1 << 7,
    Java = 1 << 8,
    Iwmmxt = 1 << 9,
    Crunch = 1 << 10,
    Thumbee = 1 << 11,
    Neon = 1 << 12,
    Vfpv3 = 1 << 13,
    Vfpv3d16 = 1 << 14,
    Tls = 1 << 15,
    Vfpv4 = 1 << 16,
    Idiva = 1 << 17,
    Idivt = 1 << 18,
}

#[repr(C)]
#[derive(Debug, Default, PartialEq)]
pub struct MDRawSystemInfo {
    /* The next 3 fields and numberOfProcessors are from the SYSTEM_INFO
     * structure as returned by GetSystemInfo */
    pub processor_architecture: u16,
    pub processor_level: u16, /* x86: 5 = 586, 6 = 686, ... */
    /* ARM: 6 = ARMv6, 7 = ARMv7 ... */
    pub processor_revision: u16, /* x86: 0xMMSS, where MM=model,
                                  *      SS=stepping */
    /* ARM: 0 */
    pub number_of_processors: u8,
    pub product_type: u8, /* Windows: VER_NT_* from WinNT.h */

    /* The next 5 fields are from the OSVERSIONINFO structure as returned
     * by GetVersionEx */
    pub major_version: u32,
    pub minor_version: u32,
    pub build_number: u32,
    pub platform_id: u32,
    pub csd_version_rva: MDRVA, /* MDString further identifying the
                                 * host OS.
                                 * Windows: name of the installed OS
                                 *          service pack.
                                 * Mac OS X: the Apple OS build number
                                 *           (sw_vers -buildVersion).
                                 * Linux: uname -srvmo */

    pub suite_mask: u16, /* Windows: VER_SUITE_* from WinNT.h */
    pub reserved2: u16,

    pub cpu: MDCPUInformation,
}

#[cfg(target_pointer_width = "64")]
#[derive(Debug, Default, PartialEq)]
pub struct MDRawLinkMap {
    pub addr: u64,
    pub name: MDRVA,
    pub ld: u64,
}

#[cfg(target_pointer_width = "64")]
#[derive(Debug, Default, PartialEq)]
pub struct MDRawDebug {
    pub version: u32,
    pub map: MDRVA, /* array of MDRawLinkMap64 */
    pub dso_count: u32,
    pub brk: u64,
    pub ldbase: u64,
    pub dynamic: u64,
}

#[cfg(target_pointer_width = "32")]
#[derive(Debug, Default, PartialEq)]
pub struct MDRawLinkMap {
    pub addr: u32,
    pub name: MDRVA,
    pub ld: u32,
}

#[cfg(target_pointer_width = "32")]
#[derive(Debug, Default, PartialEq)]
pub struct MDRawDebug {
    pub version: u32,
    pub map: MDRVA, /* array of MDRawLinkMap32 */
    pub dso_count: u32,
    pub brk: u32,
    pub ldbase: u32,
    pub dynamic: u32,
}

/* For (MDRawSystemInfo).processor_architecture: */
#[repr(u16)]
pub enum MDCPUArchitecture {
    X86 = 0,   /* PROCESSOR_ARCHITECTURE_INTEL */
    Mips = 1,  /* PROCESSOR_ARCHITECTURE_MIPS */
    Alpha = 2, /* PROCESSOR_ARCHITECTURE_ALPHA */
    Ppc = 3,   /* PROCESSOR_ARCHITECTURE_PPC */
    Shx = 4,   /* PROCESSOR_ARCHITECTURE_SHX
                * (Super-H) */
    Arm = 5,     /* PROCESSOR_ARCHITECTURE_ARM */
    Ia64 = 6,    /* PROCESSOR_ARCHITECTURE_IA64 */
    Alpha64 = 7, /* PROCESSOR_ARCHITECTURE_ALPHA64 */
    Msil = 8,    /* PROCESSOR_ARCHITECTURE_MSIL
                  * (Microsoft Intermediate Language) */
    Amd64 = 9, /* PROCESSOR_ARCHITECTURE_AMD64 */
    X86Win64 = 10,
    /* PROCESSOR_ARCHITECTURE_IA32_ON_WIN64 (WoW64) */
    Arm64 = 12,        /* PROCESSOR_ARCHITECTURE_ARM64 */
    Sparc = 0x8001,    /* Breakpad-defined value for SPARC */
    Ppc64 = 0x8002,    /* Breakpad-defined value for PPC64 */
    Arm64Old = 0x8003, /* Breakpad-defined value for ARM64 */
    Mips64 = 0x8004,   /* Breakpad-defined value for MIPS64 */
    Unknown = 0xffff,  /* PROCESSOR_ARCHITECTURE_UNKNOWN */
}

/* For (MDRawSystemInfo).platform_id: */
#[repr(u32)]
pub enum MDOSPlatform {
    Win32s = 0,       /* VER_PLATFORM_WIN32s (Windows 3.1) */
    Win32Windows = 1, /* VER_PLATFORM_WIN32_WINDOWS (Windows 95-98-Me) */
    Win32Nt = 2,      /* VER_PLATFORM_WIN32_NT (Windows NT, 2000+) */
    Win32Ce = 3,      /* VER_PLATFORM_WIN32_CE, VER_PLATFORM_WIN32_HH
                       * (Windows CE, Windows Mobile, "Handheld") */
    /* The following values are Breakpad-defined. */
    Unix = 0x8000,    /* Generic Unix-ish */
    MacOsX = 0x8101,  /* Mac OS X/Darwin */
    Ios = 0x8102,     /* iOS */
    Linux = 0x8201,   /* Linux */
    Solaris = 0x8202, /* Solaris */
    Android = 0x8203, /* Android */
    Ps3 = 0x8204,     /* PS3 */
    Nacl = 0x8205,    /* Native Client (NaCl) */
    Fuchsia = 0x8206, /* Fuchsia */
}

/*
 * Modern ELF toolchains insert a "build id" into the ELF headers that
 * usually contains a hash of some ELF headers + sections to uniquely
 * identify a binary.
 *
 * https://access.redhat.com/documentation/en-US/Red_Hat_Enterprise_Linux/6/html/Developer_Guide/compiling-build-id.html
 * https://sourceware.org/binutils/docs-2.26/ld/Options.html#index-g_t_002d_002dbuild_002did-292
 */
pub const MD_CVINFOELF_SIGNATURE: u32 = 0x4270454c; /* cvSignature = 'BpEL' */
/* Signature is followed by the bytes of the
 * build id from GNU_BUILD_ID ELF note.
 * This is variable-length, but usually 20 bytes
 * as the binutils ld default is a SHA-1 hash. */

/* For (MDRawHeader).flags: */
pub enum MDType {
    /* MD_NORMAL is the standard type of minidump.  It includes full
     * streams for the thread list, module list, exception, system info,
     * and miscellaneous info.  A memory list stream is also present,
     * pointing to the same stack memory contained in the thread list,
     * as well as a 256-byte region around the instruction address that
     * was executing when the exception occurred.  Stack memory is from
     * 4 bytes below a thread's stack pointer up to the top of the
     * memory region encompassing the stack. */
    Normal = 0x00000000,
    WithDataSegs = 0x00000001,
    WithFullMemory = 0x00000002,
    WithHandleData = 0x00000004,
    FilterMemory = 0x00000008,
    ScanMemory = 0x00000010,
    WithUnloadedModules = 0x00000020,
    WithIndirectlyReferencedMemory = 0x00000040,
    FilterModulePaths = 0x00000080,
    WithProcessThreadData = 0x00000100,
    WithPrivateReadWriteMemory = 0x00000200,
    WithoutOptionalData = 0x00000400,
    WithFullMemoryInfo = 0x00000800,
    WithThreadInfo = 0x00001000,
    WithCodeSegs = 0x00002000,
    WithoutAuxilliarySegs = 0x00004000,
    WithFullAuxilliaryState = 0x00008000,
    WithPrivateWriteCopyMemory = 0x00010000,
    IgnoreInaccessibleMemory = 0x00020000,
    WithTokenInformation = 0x00040000,
}

/* For (MDRawDirectory).stream_type */
#[repr(u32)]
pub enum MDStreamType {
    UnusedStream = 0,
    ReservedStream0 = 1,
    ReservedStream1 = 2,
    ThreadListStream = 3, /* MDRawThreadList */
    ModuleListStream = 4, /* MDRawModuleList */
    MemoryListStream = 5, /* MDRawMemoryList */
    ExceptionStream = 6,  /* MDRawExceptionStream */
    SystemInfoStream = 7, /* MDRawSystemInfo */
    ThreadExListStream = 8,
    Memory64ListStream = 9,
    CommentStreamA = 10,
    CommentStreamW = 11,
    HandleDataStream = 12,
    FunctionTableStream = 13,
    UnloadedModuleListStream = 14,
    MiscInfoStream = 15,       /* MDRawMiscInfo */
    MemoryInfoListStream = 16, /* MDRawMemoryInfoList */
    ThreadInfoListStream = 17,
    HandleOperationListStream = 18,
    TokenStream = 19,
    JavascriptDataStream = 20,
    SystemMemoryInfoStream = 21,
    ProcessVmCountersStream = 22,
    LastReservedStream = 0x0000ffff,

    /* Breakpad extension types.  0x4767 = "Gg" */
    BreakpadInfoStream = 0x47670001,  /* MDRawBreakpadInfo  */
    AssertionInfoStream = 0x47670002, /* MDRawAssertionInfo */
    /* These are additional minidump stream values which are specific to
     * the linux breakpad implementation. */
    LinuxCpuInfo = 0x47670003,    /* /proc/cpuinfo      */
    LinuxProcStatus = 0x47670004, /* /proc/$x/status    */
    LinuxLsbRelease = 0x47670005, /* /etc/lsb-release   */
    LinuxCmdLine = 0x47670006,    /* /proc/$x/cmdline   */
    LinuxEnviron = 0x47670007,    /* /proc/$x/environ   */
    LinuxAuxv = 0x47670008,       /* /proc/$x/auxv      */
    LinuxMaps = 0x47670009,       /* /proc/$x/maps      */
    LinuxDsoDebug = 0x4767000A,   /* MDRawDebug{32,64}  */

    /* Crashpad extension types. 0x4350 = "CP"
     * See Crashpad's minidump/minidump_extensions.h. */
    CrashpadInfoStream = 0x43500001, /* MDRawCrashpadInfo  */
}
