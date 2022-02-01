//! Apple (ios/darwin)-specific definitions
//!
//! This covers *-apple-* triples currently
pub type c_char = i8;
pub type wchar_t = i32;
pub type clock_t = c_ulong;
pub type time_t = c_long;
pub type suseconds_t = i32;
pub type dev_t = i32;
pub type ino_t = u64;
pub type mode_t = u16;
pub type nlink_t = u16;
pub type blksize_t = i32;
pub type rlim_t = u64;
pub type pthread_key_t = c_ulong;
pub type sigset_t = u32;
pub type clockid_t = ::c_uint;
pub type fsblkcnt_t = ::c_uint;
pub type fsfilcnt_t = ::c_uint;
pub type speed_t = ::c_ulong;
pub type tcflag_t = ::c_ulong;
pub type nl_item = ::c_int;
pub type id_t = ::c_uint;
pub type sem_t = ::c_int;
pub type idtype_t = ::c_uint;
pub type integer_t = ::c_int;
pub type cpu_type_t = integer_t;
pub type cpu_subtype_t = integer_t;
pub type natural_t = u32;
pub type mach_msg_type_number_t = natural_t;
pub type kern_return_t = ::c_int;
pub type uuid_t = [u8; 16];
pub type task_info_t = *mut integer_t;
pub type host_info_t = *mut integer_t;
pub type task_flavor_t = natural_t;
pub type rusage_info_t = *mut ::c_void;
pub type vm_offset_t = ::uintptr_t;
pub type vm_size_t = ::uintptr_t;
pub type vm_address_t = vm_offset_t;

pub type posix_spawnattr_t = *mut ::c_void;
pub type posix_spawn_file_actions_t = *mut ::c_void;
pub type key_t = ::c_int;
pub type shmatt_t = ::c_ushort;

pub type sae_associd_t = u32;
pub type sae_connid_t = u32;

pub type mach_port_t = ::c_uint;
pub type host_t = ::c_uint;
pub type host_flavor_t = integer_t;
pub type host_info64_t = *mut integer_t;
pub type processor_flavor_t = ::c_int;
pub type thread_flavor_t = natural_t;
pub type thread_inspect_t = ::mach_port_t;
pub type policy_t = ::c_int;
pub type mach_vm_address_t = u64;
pub type mach_vm_offset_t = u64;
pub type mach_vm_size_t = u64;
pub type vm_map_t = ::mach_port_t;
pub type mem_entry_name_port_t = ::mach_port_t;
pub type memory_object_t = ::mach_port_t;
pub type memory_object_offset_t = ::c_ulonglong;
pub type vm_inherit_t = ::c_uint;
pub type vm_prot_t = ::c_int;

pub type iconv_t = *mut ::c_void;

pub type processor_cpu_load_info_t = *mut processor_cpu_load_info;
pub type processor_cpu_load_info_data_t = processor_cpu_load_info;
pub type processor_basic_info_t = *mut processor_basic_info;
pub type processor_basic_info_data_t = processor_basic_info;
pub type processor_set_basic_info_data_t = processor_set_basic_info;
pub type processor_set_basic_info_t = *mut processor_set_basic_info;
pub type processor_set_load_info_data_t = processor_set_load_info;
pub type processor_set_load_info_t = *mut processor_set_load_info;
pub type processor_info_t = *mut integer_t;
pub type processor_info_array_t = *mut integer_t;

pub type mach_task_basic_info_data_t = mach_task_basic_info;
pub type mach_task_basic_info_t = *mut mach_task_basic_info;
pub type task_thread_times_info_data_t = task_thread_times_info;
pub type task_thread_times_info_t = *mut task_thread_times_info;

pub type thread_info_t = *mut integer_t;
pub type thread_basic_info_t = *mut thread_basic_info;
pub type thread_basic_info_data_t = thread_basic_info;
pub type thread_identifier_info_t = *mut thread_identifier_info;
pub type thread_identifier_info_data_t = thread_identifier_info;
pub type thread_extended_info_t = *mut thread_extended_info;
pub type thread_extended_info_data_t = thread_extended_info;

pub type thread_t = ::mach_port_t;
pub type thread_policy_flavor_t = natural_t;
pub type thread_policy_t = *mut integer_t;
pub type thread_latency_qos_t = integer_t;
pub type thread_throughput_qos_t = integer_t;
pub type thread_standard_policy_data_t = thread_standard_policy;
pub type thread_standard_policy_t = *mut thread_standard_policy;
pub type thread_extended_policy_data_t = thread_extended_policy;
pub type thread_extended_policy_t = *mut thread_extended_policy;
pub type thread_time_constraint_policy_data_t = thread_time_constraint_policy;
pub type thread_time_constraint_policy_t = *mut thread_time_constraint_policy;
pub type thread_precedence_policy_data_t = thread_precedence_policy;
pub type thread_precedence_policy_t = *mut thread_precedence_policy;
pub type thread_affinity_policy_data_t = thread_affinity_policy;
pub type thread_affinity_policy_t = *mut thread_affinity_policy;
pub type thread_background_policy_data_t = thread_background_policy;
pub type thread_background_policy_t = *mut thread_background_policy;
pub type thread_latency_qos_policy_data_t = thread_latency_qos_policy;
pub type thread_latency_qos_policy_t = *mut thread_latency_qos_policy;
pub type thread_throughput_qos_policy_data_t = thread_throughput_qos_policy;
pub type thread_throughput_qos_policy_t = *mut thread_throughput_qos_policy;

pub type pthread_introspection_hook_t =
    extern "C" fn(event: ::c_uint, thread: ::pthread_t, addr: *mut ::c_void, size: ::size_t);

pub type vm_statistics_t = *mut vm_statistics;
pub type vm_statistics_data_t = vm_statistics;
pub type vm_statistics64_t = *mut vm_statistics64;
pub type vm_statistics64_data_t = vm_statistics64;

pub type task_t = ::mach_port_t;

pub type sysdir_search_path_enumeration_state = ::c_uint;

pub type CCStatus = i32;
pub type CCCryptorStatus = i32;
pub type CCRNGStatus = ::CCCryptorStatus;

deprecated_mach! {
    pub type mach_timebase_info_data_t = mach_timebase_info;
}

#[cfg_attr(feature = "extra_traits", derive(Debug))]
pub enum timezone {}
impl ::Copy for timezone {}
impl ::Clone for timezone {
    fn clone(&self) -> timezone {
        *self
    }
}

#[cfg_attr(feature = "extra_traits", derive(Debug))]
#[repr(u32)]
pub enum qos_class_t {
    QOS_CLASS_USER_INTERACTIVE = 0x21,
    QOS_CLASS_USER_INITIATED = 0x19,
    QOS_CLASS_DEFAULT = 0x15,
    QOS_CLASS_UTILITY = 0x11,
    QOS_CLASS_BACKGROUND = 0x09,
    QOS_CLASS_UNSPECIFIED = 0x00,
}
impl ::Copy for qos_class_t {}
impl ::Clone for qos_class_t {
    fn clone(&self) -> qos_class_t {
        *self
    }
}

#[cfg_attr(feature = "extra_traits", derive(Debug))]
#[repr(u32)]
pub enum sysdir_search_path_directory_t {
    SYSDIR_DIRECTORY_APPLICATION = 1,
    SYSDIR_DIRECTORY_DEMO_APPLICATION = 2,
    SYSDIR_DIRECTORY_DEVELOPER_APPLICATION = 3,
    SYSDIR_DIRECTORY_ADMIN_APPLICATION = 4,
    SYSDIR_DIRECTORY_LIBRARY = 5,
    SYSDIR_DIRECTORY_DEVELOPER = 6,
    SYSDIR_DIRECTORY_USER = 7,
    SYSDIR_DIRECTORY_DOCUMENTATION = 8,
    SYSDIR_DIRECTORY_DOCUMENT = 9,
    SYSDIR_DIRECTORY_CORESERVICE = 10,
    SYSDIR_DIRECTORY_AUTOSAVED_INFORMATION = 11,
    SYSDIR_DIRECTORY_DESKTOP = 12,
    SYSDIR_DIRECTORY_CACHES = 13,
    SYSDIR_DIRECTORY_APPLICATION_SUPPORT = 14,
    SYSDIR_DIRECTORY_DOWNLOADS = 15,
    SYSDIR_DIRECTORY_INPUT_METHODS = 16,
    SYSDIR_DIRECTORY_MOVIES = 17,
    SYSDIR_DIRECTORY_MUSIC = 18,
    SYSDIR_DIRECTORY_PICTURES = 19,
    SYSDIR_DIRECTORY_PRINTER_DESCRIPTION = 20,
    SYSDIR_DIRECTORY_SHARED_PUBLIC = 21,
    SYSDIR_DIRECTORY_PREFERENCE_PANES = 22,
    SYSDIR_DIRECTORY_ALL_APPLICATIONS = 100,
    SYSDIR_DIRECTORY_ALL_LIBRARIES = 101,
}
impl ::Copy for sysdir_search_path_directory_t {}
impl ::Clone for sysdir_search_path_directory_t {
    fn clone(&self) -> sysdir_search_path_directory_t {
        *self
    }
}

#[cfg_attr(feature = "extra_traits", derive(Debug))]
#[repr(u32)]
pub enum sysdir_search_path_domain_mask_t {
    SYSDIR_DOMAIN_MASK_USER = (1 << 0),
    SYSDIR_DOMAIN_MASK_LOCAL = (1 << 1),
    SYSDIR_DOMAIN_MASK_NETWORK = (1 << 2),
    SYSDIR_DOMAIN_MASK_SYSTEM = (1 << 3),
    SYSDIR_DOMAIN_MASK_ALL = 0x0ffff,
}
impl ::Copy for sysdir_search_path_domain_mask_t {}
impl ::Clone for sysdir_search_path_domain_mask_t {
    fn clone(&self) -> sysdir_search_path_domain_mask_t {
        *self
    }
}

s! {
    pub struct ip_mreq {
        pub imr_multiaddr: in_addr,
        pub imr_interface: in_addr,
    }

    pub struct aiocb {
        pub aio_fildes: ::c_int,
        pub aio_offset: ::off_t,
        pub aio_buf: *mut ::c_void,
        pub aio_nbytes: ::size_t,
        pub aio_reqprio: ::c_int,
        pub aio_sigevent: sigevent,
        pub aio_lio_opcode: ::c_int
    }

    pub struct glob_t {
        pub gl_pathc:  ::size_t,
        __unused1: ::c_int,
        pub gl_offs:   ::size_t,
        __unused2: ::c_int,
        pub gl_pathv:  *mut *mut ::c_char,

        __unused3: *mut ::c_void,

        __unused4: *mut ::c_void,
        __unused5: *mut ::c_void,
        __unused6: *mut ::c_void,
        __unused7: *mut ::c_void,
        __unused8: *mut ::c_void,
    }

    pub struct addrinfo {
        pub ai_flags: ::c_int,
        pub ai_family: ::c_int,
        pub ai_socktype: ::c_int,
        pub ai_protocol: ::c_int,
        pub ai_addrlen: ::socklen_t,
        pub ai_canonname: *mut ::c_char,
        pub ai_addr: *mut ::sockaddr,
        pub ai_next: *mut addrinfo,
    }

    #[deprecated(
        since = "0.2.55",
        note = "Use the `mach2` crate instead",
    )]
    pub struct mach_timebase_info {
        pub numer: u32,
        pub denom: u32,
    }

    pub struct stat {
        pub st_dev: dev_t,
        pub st_mode: mode_t,
        pub st_nlink: nlink_t,
        pub st_ino: ino_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: dev_t,
        pub st_atime: time_t,
        pub st_atime_nsec: c_long,
        pub st_mtime: time_t,
        pub st_mtime_nsec: c_long,
        pub st_ctime: time_t,
        pub st_ctime_nsec: c_long,
        pub st_birthtime: time_t,
        pub st_birthtime_nsec: c_long,
        pub st_size: ::off_t,
        pub st_blocks: ::blkcnt_t,
        pub st_blksize: blksize_t,
        pub st_flags: u32,
        pub st_gen: u32,
        pub st_lspare: i32,
        pub st_qspare: [i64; 2],
    }

    pub struct pthread_mutexattr_t {
        __sig: ::c_long,
        __opaque: [u8; 8],
    }

    pub struct pthread_condattr_t {
        __sig: ::c_long,
        __opaque: [u8; __PTHREAD_CONDATTR_SIZE__],
    }

    pub struct pthread_rwlockattr_t {
        __sig: ::c_long,
        __opaque: [u8; __PTHREAD_RWLOCKATTR_SIZE__],
    }

    pub struct siginfo_t {
        pub si_signo: ::c_int,
        pub si_errno: ::c_int,
        pub si_code: ::c_int,
        pub si_pid: ::pid_t,
        pub si_uid: ::uid_t,
        pub si_status: ::c_int,
        pub si_addr: *mut ::c_void,
        //Requires it to be union for tests
        //pub si_value: ::sigval,
        _pad: [usize; 9],
    }

    pub struct sigaction {
        // FIXME: this field is actually a union
        pub sa_sigaction: ::sighandler_t,
        pub sa_mask: sigset_t,
        pub sa_flags: ::c_int,
    }

    pub struct stack_t {
        pub ss_sp: *mut ::c_void,
        pub ss_size: ::size_t,
        pub ss_flags: ::c_int,
    }

    pub struct fstore_t {
        pub fst_flags: ::c_uint,
        pub fst_posmode: ::c_int,
        pub fst_offset: ::off_t,
        pub fst_length: ::off_t,
        pub fst_bytesalloc: ::off_t,
    }

    pub struct radvisory {
        pub ra_offset: ::off_t,
        pub ra_count: ::c_int,
    }

    pub struct statvfs {
        pub f_bsize: ::c_ulong,
        pub f_frsize: ::c_ulong,
        pub f_blocks: ::fsblkcnt_t,
        pub f_bfree: ::fsblkcnt_t,
        pub f_bavail: ::fsblkcnt_t,
        pub f_files: ::fsfilcnt_t,
        pub f_ffree: ::fsfilcnt_t,
        pub f_favail: ::fsfilcnt_t,
        pub f_fsid: ::c_ulong,
        pub f_flag: ::c_ulong,
        pub f_namemax: ::c_ulong,
    }

    pub struct Dl_info {
        pub dli_fname: *const ::c_char,
        pub dli_fbase: *mut ::c_void,
        pub dli_sname: *const ::c_char,
        pub dli_saddr: *mut ::c_void,
    }

    pub struct sockaddr_in {
        pub sin_len: u8,
        pub sin_family: ::sa_family_t,
        pub sin_port: ::in_port_t,
        pub sin_addr: ::in_addr,
        pub sin_zero: [::c_char; 8],
    }

    pub struct kevent64_s {
        pub ident: u64,
        pub filter: i16,
        pub flags: u16,
        pub fflags: u32,
        pub data: i64,
        pub udata: u64,
        pub ext: [u64; 2],
    }

    pub struct dqblk {
        pub dqb_bhardlimit: u64,
        pub dqb_bsoftlimit: u64,
        pub dqb_curbytes: u64,
        pub dqb_ihardlimit: u32,
        pub dqb_isoftlimit: u32,
        pub dqb_curinodes: u32,
        pub dqb_btime: u32,
        pub dqb_itime: u32,
        pub dqb_id: u32,
        pub dqb_spare: [u32; 4],
    }

    pub struct if_msghdr {
        pub ifm_msglen: ::c_ushort,
        pub ifm_version: ::c_uchar,
        pub ifm_type: ::c_uchar,
        pub ifm_addrs: ::c_int,
        pub ifm_flags: ::c_int,
        pub ifm_index: ::c_ushort,
        pub ifm_data: if_data,
    }

    pub struct termios {
        pub c_iflag: ::tcflag_t,
        pub c_oflag: ::tcflag_t,
        pub c_cflag: ::tcflag_t,
        pub c_lflag: ::tcflag_t,
        pub c_cc: [::cc_t; ::NCCS],
        pub c_ispeed: ::speed_t,
        pub c_ospeed: ::speed_t,
    }

    pub struct flock {
        pub l_start: ::off_t,
        pub l_len: ::off_t,
        pub l_pid: ::pid_t,
        pub l_type: ::c_short,
        pub l_whence: ::c_short,
    }

    pub struct sf_hdtr {
        pub headers: *mut ::iovec,
        pub hdr_cnt: ::c_int,
        pub trailers: *mut ::iovec,
        pub trl_cnt: ::c_int,
    }

    pub struct lconv {
        pub decimal_point: *mut ::c_char,
        pub thousands_sep: *mut ::c_char,
        pub grouping: *mut ::c_char,
        pub int_curr_symbol: *mut ::c_char,
        pub currency_symbol: *mut ::c_char,
        pub mon_decimal_point: *mut ::c_char,
        pub mon_thousands_sep: *mut ::c_char,
        pub mon_grouping: *mut ::c_char,
        pub positive_sign: *mut ::c_char,
        pub negative_sign: *mut ::c_char,
        pub int_frac_digits: ::c_char,
        pub frac_digits: ::c_char,
        pub p_cs_precedes: ::c_char,
        pub p_sep_by_space: ::c_char,
        pub n_cs_precedes: ::c_char,
        pub n_sep_by_space: ::c_char,
        pub p_sign_posn: ::c_char,
        pub n_sign_posn: ::c_char,
        pub int_p_cs_precedes: ::c_char,
        pub int_n_cs_precedes: ::c_char,
        pub int_p_sep_by_space: ::c_char,
        pub int_n_sep_by_space: ::c_char,
        pub int_p_sign_posn: ::c_char,
        pub int_n_sign_posn: ::c_char,
    }

    pub struct proc_taskinfo {
        pub pti_virtual_size: u64,
        pub pti_resident_size: u64,
        pub pti_total_user: u64,
        pub pti_total_system: u64,
        pub pti_threads_user: u64,
        pub pti_threads_system: u64,
        pub pti_policy: i32,
        pub pti_faults: i32,
        pub pti_pageins: i32,
        pub pti_cow_faults: i32,
        pub pti_messages_sent: i32,
        pub pti_messages_received: i32,
        pub pti_syscalls_mach: i32,
        pub pti_syscalls_unix: i32,
        pub pti_csw: i32,
        pub pti_threadnum: i32,
        pub pti_numrunning: i32,
        pub pti_priority: i32,
    }

    pub struct proc_bsdinfo {
        pub pbi_flags: u32,
        pub pbi_status: u32,
        pub pbi_xstatus: u32,
        pub pbi_pid: u32,
        pub pbi_ppid: u32,
        pub pbi_uid: ::uid_t,
        pub pbi_gid: ::gid_t,
        pub pbi_ruid: ::uid_t,
        pub pbi_rgid: ::gid_t,
        pub pbi_svuid: ::uid_t,
        pub pbi_svgid: ::gid_t,
        pub rfu_1: u32,
        pub pbi_comm: [::c_char; MAXCOMLEN],
        pub pbi_name: [::c_char; 32], // MAXCOMLEN * 2, but macro isn't happy...
        pub pbi_nfiles: u32,
        pub pbi_pgid: u32,
        pub pbi_pjobc: u32,
        pub e_tdev: u32,
        pub e_tpgid: u32,
        pub pbi_nice: i32,
        pub pbi_start_tvsec: u64,
        pub pbi_start_tvusec: u64,
    }

    pub struct proc_taskallinfo {
        pub pbsd: proc_bsdinfo,
        pub ptinfo: proc_taskinfo,
    }

    pub struct xsw_usage {
        pub xsu_total: u64,
        pub xsu_avail: u64,
        pub xsu_used: u64,
        pub xsu_pagesize: u32,
        pub xsu_encrypted: ::boolean_t,
    }

    pub struct xucred {
        pub cr_version: ::c_uint,
        pub cr_uid: ::uid_t,
        pub cr_ngroups: ::c_short,
        pub cr_groups: [::gid_t;16]
    }

    #[deprecated(
        since = "0.2.55",
        note = "Use the `mach2` crate instead",
    )]
    pub struct mach_header {
        pub magic: u32,
        pub cputype: cpu_type_t,
        pub cpusubtype: cpu_subtype_t,
        pub filetype: u32,
        pub ncmds: u32,
        pub sizeofcmds: u32,
        pub flags: u32,
    }

    #[deprecated(
        since = "0.2.55",
        note = "Use the `mach2` crate instead",
    )]
    pub struct mach_header_64 {
        pub magic: u32,
        pub cputype: cpu_type_t,
        pub cpusubtype: cpu_subtype_t,
        pub filetype: u32,
        pub ncmds: u32,
        pub sizeofcmds: u32,
        pub flags: u32,
        pub reserved: u32,
    }

    pub struct segment_command {
        pub cmd: u32,
        pub cmdsize: u32,
        pub segname: [::c_char; 16],
        pub vmaddr: u32,
        pub vmsize: u32,
        pub fileoff: u32,
        pub filesize: u32,
        pub maxprot: vm_prot_t,
        pub initprot: vm_prot_t,
        pub nsects: u32,
        pub flags: u32,
    }

    pub struct segment_command_64 {
        pub cmd: u32,
        pub cmdsize: u32,
        pub segname: [::c_char; 16],
        pub vmaddr: u64,
        pub vmsize: u64,
        pub fileoff: u64,
        pub filesize: u64,
        pub maxprot: vm_prot_t,
        pub initprot: vm_prot_t,
        pub nsects: u32,
        pub flags: u32,
    }

    pub struct load_command {
        pub cmd: u32,
        pub cmdsize: u32,
    }

    pub struct sockaddr_dl {
        pub sdl_len: ::c_uchar,
        pub sdl_family: ::c_uchar,
        pub sdl_index: ::c_ushort,
        pub sdl_type: ::c_uchar,
        pub sdl_nlen: ::c_uchar,
        pub sdl_alen: ::c_uchar,
        pub sdl_slen: ::c_uchar,
        pub sdl_data: [::c_char; 12],
    }

    pub struct sockaddr_inarp {
        pub sin_len: ::c_uchar,
        pub sin_family: ::c_uchar,
        pub sin_port: ::c_ushort,
        pub sin_addr: ::in_addr,
        pub sin_srcaddr: ::in_addr,
        pub sin_tos: ::c_ushort,
        pub sin_other: ::c_ushort,
    }

    pub struct sockaddr_ctl {
        pub sc_len: ::c_uchar,
        pub sc_family: ::c_uchar,
        pub ss_sysaddr: u16,
        pub sc_id: u32,
        pub sc_unit: u32,
        pub sc_reserved: [u32; 5],
    }

    pub struct in_pktinfo {
        pub ipi_ifindex: ::c_uint,
        pub ipi_spec_dst: ::in_addr,
        pub ipi_addr: ::in_addr,
    }

    pub struct in6_pktinfo {
        pub ipi6_addr: ::in6_addr,
        pub ipi6_ifindex: ::c_uint,
    }

    // sys/ipc.h:

    pub struct ipc_perm {
        pub uid: ::uid_t,
        pub gid: ::gid_t,
        pub cuid: ::uid_t,
        pub cgid: ::gid_t,
        pub mode: ::mode_t,
        pub _seq: ::c_ushort,
        pub _key: ::key_t,
    }

    // sys/sem.h

    pub struct sembuf {
        pub sem_num: ::c_ushort,
        pub sem_op: ::c_short,
        pub sem_flg: ::c_short,
    }

    // sys/shm.h

    pub struct arphdr {
        pub ar_hrd: u16,
        pub ar_pro: u16,
        pub ar_hln: u8,
        pub ar_pln: u8,
        pub ar_op: u16,
    }

    pub struct in_addr {
        pub s_addr: ::in_addr_t,
    }

    // sys/socket.h

    pub struct sa_endpoints_t {
        pub sae_srcif: ::c_uint, // optional source interface
        pub sae_srcaddr: *const ::sockaddr, // optional source address
        pub sae_srcaddrlen: ::socklen_t, // size of source address
        pub sae_dstaddr: *const ::sockaddr, // destination address
        pub sae_dstaddrlen: ::socklen_t, // size of destination address
    }

    pub struct timex {
        pub modes: ::c_uint,
        pub offset: ::c_long,
        pub freq: ::c_long,
        pub maxerror: ::c_long,
        pub esterror: ::c_long,
        pub status: ::c_int,
        pub constant: ::c_long,
        pub precision: ::c_long,
        pub tolerance: ::c_long,
        pub ppsfreq: ::c_long,
        pub jitter: ::c_long,
        pub shift: ::c_int,
        pub stabil: ::c_long,
        pub jitcnt: ::c_long,
        pub calcnt: ::c_long,
        pub errcnt: ::c_long,
        pub stbcnt: ::c_long,
    }

    pub struct ntptimeval {
        pub time: ::timespec,
        pub maxerror: ::c_long,
        pub esterror: ::c_long,
        pub tai: ::c_long,
        pub time_state: ::c_int,
    }

    pub struct thread_standard_policy {
        pub no_data: natural_t,
    }

    pub struct thread_extended_policy {
        pub timeshare: boolean_t,
    }

    pub struct thread_time_constraint_policy {
        pub period: u32,
        pub computation: u32,
        pub constraint: u32,
        pub preemptible: boolean_t,
    }

    pub struct thread_precedence_policy {
        pub importance: integer_t,
    }

    pub struct thread_affinity_policy {
        pub affinity_tag: integer_t,
    }

    pub struct thread_background_policy {
        pub priority: integer_t,
    }

    pub struct thread_latency_qos_policy {
        pub thread_latency_qos_tier: thread_latency_qos_t,
    }

    pub struct thread_throughput_qos_policy {
        pub thread_throughput_qos_tier: thread_throughput_qos_t,
    }

    // malloc/malloc.h
    pub struct malloc_statistics_t {
        pub blocks_in_use: ::c_uint,
        pub size_in_use: ::size_t,
        pub max_size_in_use: ::size_t,
        pub size_allocated: ::size_t,
    }

    pub struct mstats {
        pub bytes_total: ::size_t,
        pub chunks_used: ::size_t,
        pub bytes_used: ::size_t,
        pub chunks_free: ::size_t,
        pub bytes_free: ::size_t,
    }

    pub struct vm_range_t {
        pub address: ::vm_address_t,
        pub size: ::vm_size_t,
    }

    // sched.h
    pub struct sched_param {
        pub sched_priority: ::c_int,
        __opaque: [::c_char; 4],
    }

    pub struct vinfo_stat {
        pub vst_dev: u32,
        pub vst_mode: u16,
        pub vst_nlink: u16,
        pub vst_ino: u64,
        pub vst_uid: ::uid_t,
        pub vst_gid: ::gid_t,
        pub vst_atime: i64,
        pub vst_atimensec: i64,
        pub vst_mtime: i64,
        pub vst_mtimensec: i64,
        pub vst_ctime: i64,
        pub vst_ctimensec: i64,
        pub vst_birthtime: i64,
        pub vst_birthtimensec: i64,
        pub vst_size: ::off_t,
        pub vst_blocks: i64,
        pub vst_blksize: i32,
        pub vst_flags: u32,
        pub vst_gen: u32,
        pub vst_rdev: u32,
        pub vst_qspare: [i64; 2],
    }

    pub struct vnode_info {
        pub vi_stat: vinfo_stat,
        pub vi_type: ::c_int,
        pub vi_pad: ::c_int,
        pub vi_fsid: ::fsid_t,
    }

    pub struct vnode_info_path {
        pub vip_vi: vnode_info,
        // Normally it's `vip_path: [::c_char; MAXPATHLEN]` but because libc supports an old rustc
        // version, we go around this limitation like this.
        pub vip_path: [[::c_char; 32]; 32],
    }

    pub struct proc_vnodepathinfo {
        pub pvi_cdir: vnode_info_path,
        pub pvi_rdir: vnode_info_path,
    }

    pub struct vm_statistics {
        pub free_count: natural_t,
        pub active_count: natural_t,
        pub inactive_count: natural_t,
        pub wire_count: natural_t,
        pub zero_fill_count: natural_t,
        pub reactivations: natural_t,
        pub pageins: natural_t,
        pub pageouts: natural_t,
        pub faults: natural_t,
        pub cow_faults: natural_t,
        pub lookups: natural_t,
        pub hits: natural_t,
        pub purgeable_count: natural_t,
        pub purges: natural_t,
        pub speculative_count: natural_t,
    }

    pub struct task_thread_times_info {
        pub user_time: time_value_t,
        pub system_time: time_value_t,
    }

    pub struct rusage_info_v0 {
        pub ri_uuid: [u8; 16],
        pub ri_user_time: u64,
        pub ri_system_time: u64,
        pub ri_pkg_idle_wkups: u64,
        pub ri_interrupt_wkups: u64,
        pub ri_pageins: u64,
        pub ri_wired_size: u64,
        pub ri_resident_size: u64,
        pub ri_phys_footprint: u64,
        pub ri_proc_start_abstime: u64,
        pub ri_proc_exit_abstime: u64,
    }

    pub struct rusage_info_v1 {
        pub ri_uuid: [u8; 16],
        pub ri_user_time: u64,
        pub ri_system_time: u64,
        pub ri_pkg_idle_wkups: u64,
        pub ri_interrupt_wkups: u64,
        pub ri_pageins: u64,
        pub ri_wired_size: u64,
        pub ri_resident_size: u64,
        pub ri_phys_footprint: u64,
        pub ri_proc_start_abstime: u64,
        pub ri_proc_exit_abstime: u64,
        pub ri_child_user_time: u64,
        pub ri_child_system_time: u64,
        pub ri_child_pkg_idle_wkups: u64,
        pub ri_child_interrupt_wkups: u64,
        pub ri_child_pageins: u64,
        pub ri_child_elapsed_abstime: u64,
    }

    pub struct rusage_info_v2 {
        pub ri_uuid: [u8; 16],
        pub ri_user_time: u64,
        pub ri_system_time: u64,
        pub ri_pkg_idle_wkups: u64,
        pub ri_interrupt_wkups: u64,
        pub ri_pageins: u64,
        pub ri_wired_size: u64,
        pub ri_resident_size: u64,
        pub ri_phys_footprint: u64,
        pub ri_proc_start_abstime: u64,
        pub ri_proc_exit_abstime: u64,
        pub ri_child_user_time: u64,
        pub ri_child_system_time: u64,
        pub ri_child_pkg_idle_wkups: u64,
        pub ri_child_interrupt_wkups: u64,
        pub ri_child_pageins: u64,
        pub ri_child_elapsed_abstime: u64,
        pub ri_diskio_bytesread: u64,
        pub ri_diskio_byteswritten: u64,
    }

    pub struct rusage_info_v3 {
        pub ri_uuid: [u8; 16],
        pub ri_user_time: u64,
        pub ri_system_time: u64,
        pub ri_pkg_idle_wkups: u64,
        pub ri_interrupt_wkups: u64,
        pub ri_pageins: u64,
        pub ri_wired_size: u64,
        pub ri_resident_size: u64,
        pub ri_phys_footprint: u64,
        pub ri_proc_start_abstime: u64,
        pub ri_proc_exit_abstime: u64,
        pub ri_child_user_time: u64,
        pub ri_child_system_time: u64,
        pub ri_child_pkg_idle_wkups: u64,
        pub ri_child_interrupt_wkups: u64,
        pub ri_child_pageins: u64,
        pub ri_child_elapsed_abstime: u64,
        pub ri_diskio_bytesread: u64,
        pub ri_diskio_byteswritten: u64,
        pub ri_cpu_time_qos_default: u64,
        pub ri_cpu_time_qos_maintenance: u64,
        pub ri_cpu_time_qos_background: u64,
        pub ri_cpu_time_qos_utility: u64,
        pub ri_cpu_time_qos_legacy: u64,
        pub ri_cpu_time_qos_user_initiated: u64,
        pub ri_cpu_time_qos_user_interactive: u64,
        pub ri_billed_system_time: u64,
        pub ri_serviced_system_time: u64,
    }

    pub struct rusage_info_v4 {
        pub ri_uuid: [u8; 16],
        pub ri_user_time: u64,
        pub ri_system_time: u64,
        pub ri_pkg_idle_wkups: u64,
        pub ri_interrupt_wkups: u64,
        pub ri_pageins: u64,
        pub ri_wired_size: u64,
        pub ri_resident_size: u64,
        pub ri_phys_footprint: u64,
        pub ri_proc_start_abstime: u64,
        pub ri_proc_exit_abstime: u64,
        pub ri_child_user_time: u64,
        pub ri_child_system_time: u64,
        pub ri_child_pkg_idle_wkups: u64,
        pub ri_child_interrupt_wkups: u64,
        pub ri_child_pageins: u64,
        pub ri_child_elapsed_abstime: u64,
        pub ri_diskio_bytesread: u64,
        pub ri_diskio_byteswritten: u64,
        pub ri_cpu_time_qos_default: u64,
        pub ri_cpu_time_qos_maintenance: u64,
        pub ri_cpu_time_qos_background: u64,
        pub ri_cpu_time_qos_utility: u64,
        pub ri_cpu_time_qos_legacy: u64,
        pub ri_cpu_time_qos_user_initiated: u64,
        pub ri_cpu_time_qos_user_interactive: u64,
        pub ri_billed_system_time: u64,
        pub ri_serviced_system_time: u64,
        pub ri_logical_writes: u64,
        pub ri_lifetime_max_phys_footprint: u64,
        pub ri_instructions: u64,
        pub ri_cycles: u64,
        pub ri_billed_energy: u64,
        pub ri_serviced_energy: u64,
        pub ri_interval_max_phys_footprint: u64,
        pub ri_runnable_time: u64,
    }
}

s_no_extra_traits! {
    #[cfg_attr(libc_packedN, repr(packed(4)))]
    pub struct kevent {
        pub ident: ::uintptr_t,
        pub filter: i16,
        pub flags: u16,
        pub fflags: u32,
        pub data: ::intptr_t,
        pub udata: *mut ::c_void,
    }

    #[cfg_attr(libc_packedN, repr(packed(4)))]
    pub struct semid_ds {
        // Note the manpage shows different types than the system header.
        pub sem_perm: ipc_perm,
        pub sem_base: i32,
        pub sem_nsems: ::c_ushort,
        pub sem_otime: ::time_t,
        pub sem_pad1: i32,
        pub sem_ctime: ::time_t,
        pub sem_pad2: i32,
        pub sem_pad3: [i32; 4],
    }

    #[cfg_attr(libc_packedN, repr(packed(4)))]
    pub struct shmid_ds {
        pub shm_perm: ipc_perm,
        pub shm_segsz: ::size_t,
        pub shm_lpid: ::pid_t,
        pub shm_cpid: ::pid_t,
        pub shm_nattch: ::shmatt_t,
        pub shm_atime: ::time_t,  // FIXME: 64-bit wrong align => wrong offset
        pub shm_dtime: ::time_t,  // FIXME: 64-bit wrong align => wrong offset
        pub shm_ctime: ::time_t,  // FIXME: 64-bit wrong align => wrong offset
        // FIXME: 64-bit wrong align => wrong offset:
        pub shm_internal: *mut ::c_void,
    }

    pub struct proc_threadinfo {
        pub pth_user_time: u64,
        pub pth_system_time: u64,
        pub pth_cpu_usage: i32,
        pub pth_policy: i32,
        pub pth_run_state: i32,
        pub pth_flags: i32,
        pub pth_sleep_time: i32,
        pub pth_curpri: i32,
        pub pth_priority: i32,
        pub pth_maxpriority: i32,
        pub pth_name: [::c_char; MAXTHREADNAMESIZE],
    }

    pub struct statfs {
        pub f_bsize: u32,
        pub f_iosize: i32,
        pub f_blocks: u64,
        pub f_bfree: u64,
        pub f_bavail: u64,
        pub f_files: u64,
        pub f_ffree: u64,
        pub f_fsid: ::fsid_t,
        pub f_owner: ::uid_t,
        pub f_type: u32,
        pub f_flags: u32,
        pub f_fssubtype: u32,
        pub f_fstypename: [::c_char; 16],
        pub f_mntonname: [::c_char; 1024],
        pub f_mntfromname: [::c_char; 1024],
        pub f_reserved: [u32; 8],
    }

    pub struct dirent {
        pub d_ino: u64,
        pub d_seekoff: u64,
        pub d_reclen: u16,
        pub d_namlen: u16,
        pub d_type: u8,
        pub d_name: [::c_char; 1024],
    }

    pub struct pthread_rwlock_t {
        __sig: ::c_long,
        __opaque: [u8; __PTHREAD_RWLOCK_SIZE__],
    }

    pub struct pthread_mutex_t {
        __sig: ::c_long,
        __opaque: [u8; __PTHREAD_MUTEX_SIZE__],
    }

    pub struct pthread_cond_t {
        __sig: ::c_long,
        __opaque: [u8; __PTHREAD_COND_SIZE__],
    }

    pub struct sockaddr_storage {
        pub ss_len: u8,
        pub ss_family: ::sa_family_t,
        __ss_pad1: [u8; 6],
        __ss_align: i64,
        __ss_pad2: [u8; 112],
    }

    pub struct utmpx {
        pub ut_user: [::c_char; _UTX_USERSIZE],
        pub ut_id: [::c_char; _UTX_IDSIZE],
        pub ut_line: [::c_char; _UTX_LINESIZE],
        pub ut_pid: ::pid_t,
        pub ut_type: ::c_short,
        pub ut_tv: ::timeval,
        pub ut_host: [::c_char; _UTX_HOSTSIZE],
        ut_pad: [u32; 16],
    }

    pub struct sigevent {
        pub sigev_notify: ::c_int,
        pub sigev_signo: ::c_int,
        pub sigev_value: ::sigval,
        __unused1: *mut ::c_void,       //actually a function pointer
        pub sigev_notify_attributes: *mut ::pthread_attr_t
    }

    pub struct processor_cpu_load_info {
        pub cpu_ticks: [::c_uint; CPU_STATE_MAX as usize],
    }

    pub struct processor_basic_info {
        pub cpu_type: cpu_type_t,
        pub cpu_subtype: cpu_subtype_t,
        pub running: ::boolean_t,
        pub slot_num: ::c_int,
        pub is_master: ::boolean_t,
    }

    pub struct processor_set_basic_info {
        pub processor_count: ::c_int,
        pub default_policy: ::c_int,
    }

    pub struct processor_set_load_info {
        pub task_count: ::c_int,
        pub thread_count: ::c_int,
        pub load_average: integer_t,
        pub mach_factor: integer_t,
    }

    pub struct time_value_t {
        pub seconds: integer_t,
        pub microseconds: integer_t,
    }

    pub struct thread_basic_info {
        pub user_time: time_value_t,
        pub system_time: time_value_t,
        pub cpu_usage: ::integer_t,
        pub policy: ::policy_t,
        pub run_state: ::integer_t,
        pub flags: ::integer_t,
        pub suspend_count: ::integer_t,
        pub sleep_time: ::integer_t,
    }

    pub struct thread_identifier_info {
        pub thread_id: u64,
        pub thread_handle: u64,
        pub dispatch_qaddr: u64,
    }

    pub struct thread_extended_info {
        pub pth_user_time: u64,
        pub pth_system_time: u64,
        pub pth_cpu_usage: i32,
        pub pth_policy: i32,
        pub pth_run_state: i32,
        pub pth_flags: i32,
        pub pth_sleep_time: i32,
        pub pth_curpri: i32,
        pub pth_priority: i32,
        pub pth_maxpriority: i32,
        pub pth_name: [::c_char; MAXTHREADNAMESIZE],
    }

    #[cfg_attr(libc_packedN, repr(packed(4)))]
    pub struct if_data64 {
        pub ifi_type: ::c_uchar,
        pub ifi_typelen: ::c_uchar,
        pub ifi_physical: ::c_uchar,
        pub ifi_addrlen: ::c_uchar,
        pub ifi_hdrlen: ::c_uchar,
        pub ifi_recvquota: ::c_uchar,
        pub ifi_xmitquota: ::c_uchar,
        pub ifi_unused1: ::c_uchar,
        pub ifi_mtu: u32,
        pub ifi_metric: u32,
        pub ifi_baudrate: u64,
        pub ifi_ipackets: u64,
        pub ifi_ierrors: u64,
        pub ifi_opackets: u64,
        pub ifi_oerrors: u64,
        pub ifi_collisions: u64,
        pub ifi_ibytes: u64,
        pub ifi_obytes: u64,
        pub ifi_imcasts: u64,
        pub ifi_omcasts: u64,
        pub ifi_iqdrops: u64,
        pub ifi_noproto: u64,
        pub ifi_recvtiming: u32,
        pub ifi_xmittiming: u32,
        #[cfg(any(target_arch = "arm", target_arch = "x86"))]
        pub ifi_lastchange: ::timeval,
        #[cfg(not(any(target_arch = "arm", target_arch = "x86")))]
        pub ifi_lastchange: timeval32,
    }

    #[cfg_attr(libc_packedN, repr(packed(4)))]
    pub struct if_msghdr2 {
        pub ifm_msglen: ::c_ushort,
        pub ifm_version: ::c_uchar,
        pub ifm_type: ::c_uchar,
        pub ifm_addrs: ::c_int,
        pub ifm_flags: ::c_int,
        pub ifm_index: ::c_ushort,
        pub ifm_snd_len: ::c_int,
        pub ifm_snd_maxlen: ::c_int,
        pub ifm_snd_drops: ::c_int,
        pub ifm_timer: ::c_int,
        pub ifm_data: if_data64,
    }

    #[cfg_attr(libc_packedN, repr(packed(8)))]
    pub struct vm_statistics64 {
        pub free_count: natural_t,
        pub active_count: natural_t,
        pub inactive_count: natural_t,
        pub wire_count: natural_t,
        pub zero_fill_count: u64,
        pub reactivations: u64,
        pub pageins: u64,
        pub pageouts: u64,
        pub faults: u64,
        pub cow_faults: u64,
        pub lookups: u64,
        pub hits: u64,
        pub purges: u64,
        pub purgeable_count: natural_t,
        pub speculative_count: natural_t,
        pub decompressions: u64,
        pub compressions: u64,
        pub swapins: u64,
        pub swapouts: u64,
        pub compressor_page_count: natural_t,
        pub throttled_count: natural_t,
        pub external_page_count: natural_t,
        pub internal_page_count: natural_t,
        pub total_uncompressed_pages_in_compressor: u64,
    }

    #[cfg_attr(libc_packedN, repr(packed(4)))]
    pub struct mach_task_basic_info {
        pub virtual_size: mach_vm_size_t,
        pub resident_size: mach_vm_size_t,
        pub resident_size_max: mach_vm_size_t,
        pub user_time: time_value_t,
        pub system_time: time_value_t,
        pub policy: ::policy_t,
        pub suspend_count: integer_t,
    }
}

impl siginfo_t {
    pub unsafe fn si_addr(&self) -> *mut ::c_void {
        self.si_addr
    }

    pub unsafe fn si_value(&self) -> ::sigval {
        #[repr(C)]
        struct siginfo_timer {
            _si_signo: ::c_int,
            _si_errno: ::c_int,
            _si_code: ::c_int,
            _si_pid: ::pid_t,
            _si_uid: ::uid_t,
            _si_status: ::c_int,
            _si_addr: *mut ::c_void,
            si_value: ::sigval,
        }

        (*(self as *const siginfo_t as *const siginfo_timer)).si_value
    }

    pub unsafe fn si_pid(&self) -> ::pid_t {
        self.si_pid
    }

    pub unsafe fn si_uid(&self) -> ::uid_t {
        self.si_uid
    }

    pub unsafe fn si_status(&self) -> ::c_int {
        self.si_status
    }
}

cfg_if! {
    if #[cfg(libc_union)] {
        s_no_extra_traits! {
            pub union semun {
                pub val: ::c_int,
                pub buf: *mut semid_ds,
                pub array: *mut ::c_ushort,
            }
        }

        cfg_if! {
            if #[cfg(feature = "extra_traits")] {
                impl PartialEq for semun {
                    fn eq(&self, other: &semun) -> bool {
                        unsafe { self.val == other.val }
                    }
                }
                impl Eq for semun {}
                impl ::fmt::Debug for semun {
                    fn fmt(&self, f: &mut ::fmt::Formatter)
                           -> ::fmt::Result {
                        f.debug_struct("semun")
                            .field("val", unsafe { &self.val })
                            .finish()
                    }
                }
                impl ::hash::Hash for semun {
                    fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                        unsafe { self.val.hash(state) };
                    }
                }
            }
        }
    }
}

cfg_if! {
    if #[cfg(feature = "extra_traits")] {
        impl PartialEq for kevent {
            fn eq(&self, other: &kevent) -> bool {
                self.ident == other.ident
                    && self.filter == other.filter
                    && self.flags == other.flags
                    && self.fflags == other.fflags
                    && self.data == other.data
                    && self.udata == other.udata
            }
        }
        impl Eq for kevent {}
        impl ::fmt::Debug for kevent {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                let ident = self.ident;
                let filter = self.filter;
                let flags = self.flags;
                let fflags = self.fflags;
                let data = self.data;
                let udata = self.udata;
                f.debug_struct("kevent")
                    .field("ident", &ident)
                    .field("filter", &filter)
                    .field("flags", &flags)
                    .field("fflags", &fflags)
                    .field("data", &data)
                    .field("udata", &udata)
                    .finish()
            }
        }
        impl ::hash::Hash for kevent {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                let ident = self.ident;
                let filter = self.filter;
                let flags = self.flags;
                let fflags = self.fflags;
                let data = self.data;
                let udata = self.udata;
                ident.hash(state);
                filter.hash(state);
                flags.hash(state);
                fflags.hash(state);
                data.hash(state);
                udata.hash(state);
            }
        }

        impl PartialEq for semid_ds {
            fn eq(&self, other: &semid_ds) -> bool {
                let sem_perm = self.sem_perm;
                let sem_pad3 = self.sem_pad3;
                let other_sem_perm = other.sem_perm;
                let other_sem_pad3 = other.sem_pad3;
                sem_perm == other_sem_perm
                    && self.sem_base == other.sem_base
                    && self.sem_nsems == other.sem_nsems
                    && self.sem_otime == other.sem_otime
                    && self.sem_pad1 == other.sem_pad1
                    && self.sem_ctime == other.sem_ctime
                    && self.sem_pad2 == other.sem_pad2
                    && sem_pad3 == other_sem_pad3
            }
        }
        impl Eq for semid_ds {}
        impl ::fmt::Debug for semid_ds {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                let sem_perm = self.sem_perm;
                let sem_base = self.sem_base;
                let sem_nsems = self.sem_nsems;
                let sem_otime = self.sem_otime;
                let sem_pad1 = self.sem_pad1;
                let sem_ctime = self.sem_ctime;
                let sem_pad2 = self.sem_pad2;
                let sem_pad3 = self.sem_pad3;
                f.debug_struct("semid_ds")
                    .field("sem_perm", &sem_perm)
                    .field("sem_base", &sem_base)
                    .field("sem_nsems", &sem_nsems)
                    .field("sem_otime", &sem_otime)
                    .field("sem_pad1", &sem_pad1)
                    .field("sem_ctime", &sem_ctime)
                    .field("sem_pad2", &sem_pad2)
                    .field("sem_pad3", &sem_pad3)
                    .finish()
            }
        }
        impl ::hash::Hash for semid_ds {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                let sem_perm = self.sem_perm;
                let sem_base = self.sem_base;
                let sem_nsems = self.sem_nsems;
                let sem_otime = self.sem_otime;
                let sem_pad1 = self.sem_pad1;
                let sem_ctime = self.sem_ctime;
                let sem_pad2 = self.sem_pad2;
                let sem_pad3 = self.sem_pad3;
                sem_perm.hash(state);
                sem_base.hash(state);
                sem_nsems.hash(state);
                sem_otime.hash(state);
                sem_pad1.hash(state);
                sem_ctime.hash(state);
                sem_pad2.hash(state);
                sem_pad3.hash(state);
            }
        }

        impl PartialEq for shmid_ds {
            fn eq(&self, other: &shmid_ds) -> bool {
                let shm_perm = self.shm_perm;
                let other_shm_perm = other.shm_perm;
                shm_perm == other_shm_perm
                    && self.shm_segsz == other.shm_segsz
                    && self.shm_lpid == other.shm_lpid
                    && self.shm_cpid == other.shm_cpid
                    && self.shm_nattch == other.shm_nattch
                    && self.shm_atime == other.shm_atime
                    && self.shm_dtime == other.shm_dtime
                    && self.shm_ctime == other.shm_ctime
                    && self.shm_internal == other.shm_internal
            }
        }
        impl Eq for shmid_ds {}
        impl ::fmt::Debug for shmid_ds {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                let shm_perm = self.shm_perm;
                let shm_segsz = self.shm_segsz;
                let shm_lpid = self.shm_lpid;
                let shm_cpid = self.shm_cpid;
                let shm_nattch = self.shm_nattch;
                let shm_atime = self.shm_atime;
                let shm_dtime = self.shm_dtime;
                let shm_ctime = self.shm_ctime;
                let shm_internal = self.shm_internal;
                f.debug_struct("shmid_ds")
                    .field("shm_perm", &shm_perm)
                    .field("shm_segsz", &shm_segsz)
                    .field("shm_lpid", &shm_lpid)
                    .field("shm_cpid", &shm_cpid)
                    .field("shm_nattch", &shm_nattch)
                    .field("shm_atime", &shm_atime)
                    .field("shm_dtime", &shm_dtime)
                    .field("shm_ctime", &shm_ctime)
                    .field("shm_internal", &shm_internal)
                    .finish()
            }
        }
        impl ::hash::Hash for shmid_ds {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                let shm_perm = self.shm_perm;
                let shm_segsz = self.shm_segsz;
                let shm_lpid = self.shm_lpid;
                let shm_cpid = self.shm_cpid;
                let shm_nattch = self.shm_nattch;
                let shm_atime = self.shm_atime;
                let shm_dtime = self.shm_dtime;
                let shm_ctime = self.shm_ctime;
                let shm_internal = self.shm_internal;
                shm_perm.hash(state);
                shm_segsz.hash(state);
                shm_lpid.hash(state);
                shm_cpid.hash(state);
                shm_nattch.hash(state);
                shm_atime.hash(state);
                shm_dtime.hash(state);
                shm_ctime.hash(state);
                shm_internal.hash(state);
            }
        }

        impl PartialEq for proc_threadinfo {
            fn eq(&self, other: &proc_threadinfo) -> bool {
                self.pth_user_time == other.pth_user_time
                    && self.pth_system_time == other.pth_system_time
                    && self.pth_cpu_usage == other.pth_cpu_usage
                    && self.pth_policy == other.pth_policy
                    && self.pth_run_state == other.pth_run_state
                    && self.pth_flags == other.pth_flags
                    && self.pth_sleep_time == other.pth_sleep_time
                    && self.pth_curpri == other.pth_curpri
                    && self.pth_priority == other.pth_priority
                    && self.pth_maxpriority == other.pth_maxpriority
                    && self.pth_name
                           .iter()
                           .zip(other.pth_name.iter())
                           .all(|(a,b)| a == b)
            }
        }
        impl Eq for proc_threadinfo {}
        impl ::fmt::Debug for proc_threadinfo {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("proc_threadinfo")
                    .field("pth_user_time", &self.pth_user_time)
                    .field("pth_system_time", &self.pth_system_time)
                    .field("pth_cpu_usage", &self.pth_cpu_usage)
                    .field("pth_policy", &self.pth_policy)
                    .field("pth_run_state", &self.pth_run_state)
                    .field("pth_flags", &self.pth_flags)
                    .field("pth_sleep_time", &self.pth_sleep_time)
                    .field("pth_curpri", &self.pth_curpri)
                    .field("pth_priority", &self.pth_priority)
                    .field("pth_maxpriority", &self.pth_maxpriority)
                      // FIXME: .field("pth_name", &self.pth_name)
                    .finish()
            }
        }
        impl ::hash::Hash for proc_threadinfo {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.pth_user_time.hash(state);
                self.pth_system_time.hash(state);
                self.pth_cpu_usage.hash(state);
                self.pth_policy.hash(state);
                self.pth_run_state.hash(state);
                self.pth_flags.hash(state);
                self.pth_sleep_time.hash(state);
                self.pth_curpri.hash(state);
                self.pth_priority.hash(state);
                self.pth_maxpriority.hash(state);
                self.pth_name.hash(state);
            }
        }

        impl PartialEq for statfs {
            fn eq(&self, other: &statfs) -> bool {
                self.f_bsize == other.f_bsize
                    && self.f_iosize == other.f_iosize
                    && self.f_blocks == other.f_blocks
                    && self.f_bfree == other.f_bfree
                    && self.f_bavail == other.f_bavail
                    && self.f_files == other.f_files
                    && self.f_ffree == other.f_ffree
                    && self.f_fsid == other.f_fsid
                    && self.f_owner == other.f_owner
                    && self.f_flags == other.f_flags
                    && self.f_fssubtype == other.f_fssubtype
                    && self.f_fstypename == other.f_fstypename
                    && self.f_type == other.f_type
                    && self
                    .f_mntonname
                    .iter()
                    .zip(other.f_mntonname.iter())
                    .all(|(a,b)| a == b)
                    && self
                    .f_mntfromname
                    .iter()
                    .zip(other.f_mntfromname.iter())
                    .all(|(a,b)| a == b)
                    && self.f_reserved == other.f_reserved
            }
        }

        impl Eq for statfs {}
        impl ::fmt::Debug for statfs {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("statfs")
                    .field("f_bsize", &self.f_bsize)
                    .field("f_iosize", &self.f_iosize)
                    .field("f_blocks", &self.f_blocks)
                    .field("f_bfree", &self.f_bfree)
                    .field("f_bavail", &self.f_bavail)
                    .field("f_files", &self.f_files)
                    .field("f_ffree", &self.f_ffree)
                    .field("f_fsid", &self.f_fsid)
                    .field("f_owner", &self.f_owner)
                    .field("f_flags", &self.f_flags)
                    .field("f_fssubtype", &self.f_fssubtype)
                    .field("f_fstypename", &self.f_fstypename)
                    .field("f_type", &self.f_type)
                // FIXME: .field("f_mntonname", &self.f_mntonname)
                // FIXME: .field("f_mntfromname", &self.f_mntfromname)
                    .field("f_reserved", &self.f_reserved)
                    .finish()
            }
        }

        impl ::hash::Hash for statfs {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.f_bsize.hash(state);
                self.f_iosize.hash(state);
                self.f_blocks.hash(state);
                self.f_bfree.hash(state);
                self.f_bavail.hash(state);
                self.f_files.hash(state);
                self.f_ffree.hash(state);
                self.f_fsid.hash(state);
                self.f_owner.hash(state);
                self.f_flags.hash(state);
                self.f_fssubtype.hash(state);
                self.f_fstypename.hash(state);
                self.f_type.hash(state);
                self.f_mntonname.hash(state);
                self.f_mntfromname.hash(state);
                self.f_reserved.hash(state);
            }
        }

        impl PartialEq for dirent {
            fn eq(&self, other: &dirent) -> bool {
                self.d_ino == other.d_ino
                    && self.d_seekoff == other.d_seekoff
                    && self.d_reclen == other.d_reclen
                    && self.d_namlen == other.d_namlen
                    && self.d_type == other.d_type
                    && self
                    .d_name
                    .iter()
                    .zip(other.d_name.iter())
                    .all(|(a,b)| a == b)
            }
        }
        impl Eq for dirent {}
        impl ::fmt::Debug for dirent {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("dirent")
                    .field("d_ino", &self.d_ino)
                    .field("d_seekoff", &self.d_seekoff)
                    .field("d_reclen", &self.d_reclen)
                    .field("d_namlen", &self.d_namlen)
                    .field("d_type", &self.d_type)
                    // FIXME: .field("d_name", &self.d_name)
                    .finish()
            }
        }
        impl ::hash::Hash for dirent {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.d_ino.hash(state);
                self.d_seekoff.hash(state);
                self.d_reclen.hash(state);
                self.d_namlen.hash(state);
                self.d_type.hash(state);
                self.d_name.hash(state);
            }
        }
        impl PartialEq for pthread_rwlock_t {
            fn eq(&self, other: &pthread_rwlock_t) -> bool {
                self.__sig == other.__sig
                    && self.
                    __opaque
                    .iter()
                    .zip(other.__opaque.iter())
                    .all(|(a,b)| a == b)
            }
        }
        impl Eq for pthread_rwlock_t {}
        impl ::fmt::Debug for pthread_rwlock_t {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("pthread_rwlock_t")
                    .field("__sig", &self.__sig)
                    // FIXME: .field("__opaque", &self.__opaque)
                    .finish()
            }
        }
        impl ::hash::Hash for pthread_rwlock_t {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.__sig.hash(state);
                self.__opaque.hash(state);
            }
        }

        impl PartialEq for pthread_mutex_t {
            fn eq(&self, other: &pthread_mutex_t) -> bool {
                self.__sig == other.__sig
                    && self.
                    __opaque
                    .iter()
                    .zip(other.__opaque.iter())
                    .all(|(a,b)| a == b)
            }
        }

        impl Eq for pthread_mutex_t {}

        impl ::fmt::Debug for pthread_mutex_t {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("pthread_mutex_t")
                    .field("__sig", &self.__sig)
                    // FIXME: .field("__opaque", &self.__opaque)
                    .finish()
            }
        }

        impl ::hash::Hash for pthread_mutex_t {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.__sig.hash(state);
                self.__opaque.hash(state);
            }
        }

        impl PartialEq for pthread_cond_t {
            fn eq(&self, other: &pthread_cond_t) -> bool {
                self.__sig == other.__sig
                    && self.
                    __opaque
                    .iter()
                    .zip(other.__opaque.iter())
                    .all(|(a,b)| a == b)
            }
        }

        impl Eq for pthread_cond_t {}

        impl ::fmt::Debug for pthread_cond_t {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("pthread_cond_t")
                    .field("__sig", &self.__sig)
                    // FIXME: .field("__opaque", &self.__opaque)
                    .finish()
            }
        }

        impl ::hash::Hash for pthread_cond_t {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.__sig.hash(state);
                self.__opaque.hash(state);
            }
        }

        impl PartialEq for sockaddr_storage {
            fn eq(&self, other: &sockaddr_storage) -> bool {
                self.ss_len == other.ss_len
                    && self.ss_family == other.ss_family
                    && self
                    .__ss_pad1
                    .iter()
                    .zip(other.__ss_pad1.iter())
                    .all(|(a, b)| a == b)
                    && self.__ss_align == other.__ss_align
                    && self
                    .__ss_pad2
                    .iter()
                    .zip(other.__ss_pad2.iter())
                    .all(|(a, b)| a == b)
            }
        }

        impl Eq for sockaddr_storage {}

        impl ::fmt::Debug for sockaddr_storage {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("sockaddr_storage")
                    .field("ss_len", &self.ss_len)
                    .field("ss_family", &self.ss_family)
                    .field("__ss_pad1", &self.__ss_pad1)
                    .field("__ss_align", &self.__ss_align)
                    // FIXME: .field("__ss_pad2", &self.__ss_pad2)
                    .finish()
            }
        }

        impl ::hash::Hash for sockaddr_storage {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.ss_len.hash(state);
                self.ss_family.hash(state);
                self.__ss_pad1.hash(state);
                self.__ss_align.hash(state);
                self.__ss_pad2.hash(state);
            }
        }

        impl PartialEq for utmpx {
            fn eq(&self, other: &utmpx) -> bool {
                self.ut_user
                    .iter()
                    .zip(other.ut_user.iter())
                    .all(|(a,b)| a == b)
                    && self.ut_id == other.ut_id
                    && self.ut_line == other.ut_line
                    && self.ut_pid == other.ut_pid
                    && self.ut_type == other.ut_type
                    && self.ut_tv == other.ut_tv
                    && self
                    .ut_host
                    .iter()
                    .zip(other.ut_host.iter())
                    .all(|(a,b)| a == b)
                    && self.ut_pad == other.ut_pad
            }
        }

        impl Eq for utmpx {}

        impl ::fmt::Debug for utmpx {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("utmpx")
                    // FIXME: .field("ut_user", &self.ut_user)
                    .field("ut_id", &self.ut_id)
                    .field("ut_line", &self.ut_line)
                    .field("ut_pid", &self.ut_pid)
                    .field("ut_type", &self.ut_type)
                    .field("ut_tv", &self.ut_tv)
                    // FIXME: .field("ut_host", &self.ut_host)
                    .field("ut_pad", &self.ut_pad)
                    .finish()
            }
        }

        impl ::hash::Hash for utmpx {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.ut_user.hash(state);
                self.ut_id.hash(state);
                self.ut_line.hash(state);
                self.ut_pid.hash(state);
                self.ut_type.hash(state);
                self.ut_tv.hash(state);
                self.ut_host.hash(state);
                self.ut_pad.hash(state);
            }
        }

        impl PartialEq for sigevent {
            fn eq(&self, other: &sigevent) -> bool {
                self.sigev_notify == other.sigev_notify
                    && self.sigev_signo == other.sigev_signo
                    && self.sigev_value == other.sigev_value
                    && self.sigev_notify_attributes
                        == other.sigev_notify_attributes
            }
        }

        impl Eq for sigevent {}

        impl ::fmt::Debug for sigevent {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("sigevent")
                    .field("sigev_notify", &self.sigev_notify)
                    .field("sigev_signo", &self.sigev_signo)
                    .field("sigev_value", &self.sigev_value)
                    .field("sigev_notify_attributes",
                           &self.sigev_notify_attributes)
                    .finish()
            }
        }

        impl ::hash::Hash for sigevent {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.sigev_notify.hash(state);
                self.sigev_signo.hash(state);
                self.sigev_value.hash(state);
                self.sigev_notify_attributes.hash(state);
            }
        }

        impl PartialEq for processor_cpu_load_info {
            fn eq(&self, other: &processor_cpu_load_info) -> bool {
                self.cpu_ticks == other.cpu_ticks
            }
        }
        impl Eq for processor_cpu_load_info {}
        impl ::fmt::Debug for processor_cpu_load_info {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("processor_cpu_load_info")
                    .field("cpu_ticks", &self.cpu_ticks)
                    .finish()
            }
        }
        impl ::hash::Hash for processor_cpu_load_info {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.cpu_ticks.hash(state);
            }
        }

        impl PartialEq for processor_basic_info {
            fn eq(&self, other: &processor_basic_info) -> bool {
                self.cpu_type == other.cpu_type
                    && self.cpu_subtype == other.cpu_subtype
                    && self.running == other.running
                    && self.slot_num == other.slot_num
                    && self.is_master == other.is_master
            }
        }
        impl Eq for processor_basic_info {}
        impl ::fmt::Debug for processor_basic_info {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("processor_basic_info")
                    .field("cpu_type", &self.cpu_type)
                    .field("cpu_subtype", &self.cpu_subtype)
                    .field("running", &self.running)
                    .field("slot_num", &self.slot_num)
                    .field("is_master", &self.is_master)
                    .finish()
            }
        }
        impl ::hash::Hash for processor_basic_info {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.cpu_type.hash(state);
                self.cpu_subtype.hash(state);
                self.running.hash(state);
                self.slot_num.hash(state);
                self.is_master.hash(state);
            }
        }

        impl PartialEq for processor_set_basic_info {
            fn eq(&self, other: &processor_set_basic_info) -> bool {
                self.processor_count == other.processor_count
                    && self.default_policy == other.default_policy
            }
        }
        impl Eq for processor_set_basic_info {}
        impl ::fmt::Debug for processor_set_basic_info {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("processor_set_basic_info")
                    .field("processor_count", &self.processor_count)
                    .field("default_policy", &self.default_policy)
                    .finish()
            }
        }
        impl ::hash::Hash for processor_set_basic_info {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.processor_count.hash(state);
                self.default_policy.hash(state);
            }
        }

        impl PartialEq for processor_set_load_info {
            fn eq(&self, other: &processor_set_load_info) -> bool {
                self.task_count == other.task_count
                    && self.thread_count == other.thread_count
                    && self.load_average == other.load_average
                    && self.mach_factor == other.mach_factor
            }
        }
        impl Eq for processor_set_load_info {}
        impl ::fmt::Debug for processor_set_load_info {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("processor_set_load_info")
                    .field("task_count", &self.task_count)
                    .field("thread_count", &self.thread_count)
                    .field("load_average", &self.load_average)
                    .field("mach_factor", &self.mach_factor)
                    .finish()
            }
        }
        impl ::hash::Hash for processor_set_load_info {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.task_count.hash(state);
                self.thread_count.hash(state);
                self.load_average.hash(state);
                self.mach_factor.hash(state);
            }
        }

        impl PartialEq for time_value_t {
            fn eq(&self, other: &time_value_t) -> bool {
                self.seconds == other.seconds
                    && self.microseconds == other.microseconds
            }
        }
        impl Eq for time_value_t {}
        impl ::fmt::Debug for time_value_t {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("time_value_t")
                    .field("seconds", &self.seconds)
                    .field("microseconds", &self.microseconds)
                    .finish()
            }
        }
        impl ::hash::Hash for time_value_t {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.seconds.hash(state);
                self.microseconds.hash(state);
            }
        }
        impl PartialEq for thread_basic_info {
            fn eq(&self, other: &thread_basic_info) -> bool {
                self.user_time == other.user_time
                    && self.system_time == other.system_time
                    && self.cpu_usage == other.cpu_usage
                    && self.policy == other.policy
                    && self.run_state == other.run_state
                    && self.flags == other.flags
                    && self.suspend_count == other.suspend_count
                    && self.sleep_time == other.sleep_time
            }
        }
        impl Eq for thread_basic_info {}
        impl ::fmt::Debug for thread_basic_info {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("thread_basic_info")
                    .field("user_time", &self.user_time)
                    .field("system_time", &self.system_time)
                    .field("cpu_usage", &self.cpu_usage)
                    .field("policy", &self.policy)
                    .field("run_state", &self.run_state)
                    .field("flags", &self.flags)
                    .field("suspend_count", &self.suspend_count)
                    .field("sleep_time", &self.sleep_time)
                    .finish()
            }
        }
        impl ::hash::Hash for thread_basic_info {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.user_time.hash(state);
                self.system_time.hash(state);
                self.cpu_usage.hash(state);
                self.policy.hash(state);
                self.run_state.hash(state);
                self.flags.hash(state);
                self.suspend_count.hash(state);
                self.sleep_time.hash(state);
            }
        }
        impl PartialEq for thread_extended_info {
            fn eq(&self, other: &thread_extended_info) -> bool {
                self.pth_user_time == other.pth_user_time
                    && self.pth_system_time == other.pth_system_time
                    && self.pth_cpu_usage == other.pth_cpu_usage
                    && self.pth_policy == other.pth_policy
                    && self.pth_run_state == other.pth_run_state
                    && self.pth_flags == other.pth_flags
                    && self.pth_sleep_time == other.pth_sleep_time
                    && self.pth_curpri == other.pth_curpri
                    && self.pth_priority == other.pth_priority
                    && self.pth_maxpriority == other.pth_maxpriority
                    && self.pth_name
                           .iter()
                           .zip(other.pth_name.iter())
                           .all(|(a,b)| a == b)
            }
        }
        impl Eq for thread_extended_info {}
        impl ::fmt::Debug for thread_extended_info {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("proc_threadinfo")
                    .field("pth_user_time", &self.pth_user_time)
                    .field("pth_system_time", &self.pth_system_time)
                    .field("pth_cpu_usage", &self.pth_cpu_usage)
                    .field("pth_policy", &self.pth_policy)
                    .field("pth_run_state", &self.pth_run_state)
                    .field("pth_flags", &self.pth_flags)
                    .field("pth_sleep_time", &self.pth_sleep_time)
                    .field("pth_curpri", &self.pth_curpri)
                    .field("pth_priority", &self.pth_priority)
                    .field("pth_maxpriority", &self.pth_maxpriority)
                      // FIXME: .field("pth_name", &self.pth_name)
                    .finish()
            }
        }
        impl ::hash::Hash for thread_extended_info {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.pth_user_time.hash(state);
                self.pth_system_time.hash(state);
                self.pth_cpu_usage.hash(state);
                self.pth_policy.hash(state);
                self.pth_run_state.hash(state);
                self.pth_flags.hash(state);
                self.pth_sleep_time.hash(state);
                self.pth_curpri.hash(state);
                self.pth_priority.hash(state);
                self.pth_maxpriority.hash(state);
                self.pth_name.hash(state);
            }
        }
        impl PartialEq for thread_identifier_info {
            fn eq(&self, other: &thread_identifier_info) -> bool {
                self.thread_id == other.thread_id
                    && self.thread_handle == other.thread_handle
                    && self.dispatch_qaddr == other.dispatch_qaddr
            }
        }
        impl Eq for thread_identifier_info {}
        impl ::fmt::Debug for thread_identifier_info {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("thread_identifier_info")
                    .field("thread_id", &self.thread_id)
                    .field("thread_handle", &self.thread_handle)
                    .field("dispatch_qaddr", &self.dispatch_qaddr)
                    .finish()
            }
        }
        impl ::hash::Hash for thread_identifier_info {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.thread_id.hash(state);
                self.thread_handle.hash(state);
                self.dispatch_qaddr.hash(state);
            }
        }
        impl PartialEq for if_data64 {
            fn eq(&self, other: &if_data64) -> bool {
                self.ifi_type == other.ifi_type &&
                self.ifi_typelen == other.ifi_typelen &&
                self.ifi_physical == other.ifi_physical &&
                self.ifi_addrlen == other.ifi_addrlen &&
                self.ifi_hdrlen == other.ifi_hdrlen &&
                self.ifi_recvquota == other.ifi_recvquota &&
                self.ifi_xmitquota == other.ifi_xmitquota &&
                self.ifi_unused1 == other.ifi_unused1 &&
                self.ifi_mtu == other.ifi_mtu &&
                self.ifi_metric == other.ifi_metric &&
                self.ifi_baudrate == other.ifi_baudrate &&
                self.ifi_ipackets == other.ifi_ipackets &&
                self.ifi_ierrors == other.ifi_ierrors &&
                self.ifi_opackets == other.ifi_opackets &&
                self.ifi_oerrors == other.ifi_oerrors &&
                self.ifi_collisions == other.ifi_collisions &&
                self.ifi_ibytes == other.ifi_ibytes &&
                self.ifi_obytes == other.ifi_obytes &&
                self.ifi_imcasts == other.ifi_imcasts &&
                self.ifi_omcasts == other.ifi_omcasts &&
                self.ifi_iqdrops == other.ifi_iqdrops &&
                self.ifi_noproto == other.ifi_noproto &&
                self.ifi_recvtiming == other.ifi_recvtiming &&
                self.ifi_xmittiming == other.ifi_xmittiming &&
                self.ifi_lastchange == other.ifi_lastchange
            }
        }
        impl Eq for if_data64 {}
        impl ::fmt::Debug for if_data64 {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                let ifi_type = self.ifi_type;
                let ifi_typelen = self.ifi_typelen;
                let ifi_physical = self.ifi_physical;
                let ifi_addrlen = self.ifi_addrlen;
                let ifi_hdrlen = self.ifi_hdrlen;
                let ifi_recvquota = self.ifi_recvquota;
                let ifi_xmitquota = self.ifi_xmitquota;
                let ifi_unused1 = self.ifi_unused1;
                let ifi_mtu = self.ifi_mtu;
                let ifi_metric = self.ifi_metric;
                let ifi_baudrate = self.ifi_baudrate;
                let ifi_ipackets = self.ifi_ipackets;
                let ifi_ierrors = self.ifi_ierrors;
                let ifi_opackets = self.ifi_opackets;
                let ifi_oerrors = self.ifi_oerrors;
                let ifi_collisions = self.ifi_collisions;
                let ifi_ibytes = self.ifi_ibytes;
                let ifi_obytes = self.ifi_obytes;
                let ifi_imcasts = self.ifi_imcasts;
                let ifi_omcasts = self.ifi_omcasts;
                let ifi_iqdrops = self.ifi_iqdrops;
                let ifi_noproto = self.ifi_noproto;
                let ifi_recvtiming = self.ifi_recvtiming;
                let ifi_xmittiming = self.ifi_xmittiming;
                let ifi_lastchange = self.ifi_lastchange;
                f.debug_struct("if_data64")
                    .field("ifi_type", &ifi_type)
                    .field("ifi_typelen", &ifi_typelen)
                    .field("ifi_physical", &ifi_physical)
                    .field("ifi_addrlen", &ifi_addrlen)
                    .field("ifi_hdrlen", &ifi_hdrlen)
                    .field("ifi_recvquota", &ifi_recvquota)
                    .field("ifi_xmitquota", &ifi_xmitquota)
                    .field("ifi_unused1", &ifi_unused1)
                    .field("ifi_mtu", &ifi_mtu)
                    .field("ifi_metric", &ifi_metric)
                    .field("ifi_baudrate", &ifi_baudrate)
                    .field("ifi_ipackets", &ifi_ipackets)
                    .field("ifi_ierrors", &ifi_ierrors)
                    .field("ifi_opackets", &ifi_opackets)
                    .field("ifi_oerrors", &ifi_oerrors)
                    .field("ifi_collisions", &ifi_collisions)
                    .field("ifi_ibytes", &ifi_ibytes)
                    .field("ifi_obytes", &ifi_obytes)
                    .field("ifi_imcasts", &ifi_imcasts)
                    .field("ifi_omcasts", &ifi_omcasts)
                    .field("ifi_iqdrops", &ifi_iqdrops)
                    .field("ifi_noproto", &ifi_noproto)
                    .field("ifi_recvtiming", &ifi_recvtiming)
                    .field("ifi_xmittiming", &ifi_xmittiming)
                    .field("ifi_lastchange", &ifi_lastchange)
                    .finish()
            }
        }
        impl ::hash::Hash for if_data64 {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                let ifi_type = self.ifi_type;
                let ifi_typelen = self.ifi_typelen;
                let ifi_physical = self.ifi_physical;
                let ifi_addrlen = self.ifi_addrlen;
                let ifi_hdrlen = self.ifi_hdrlen;
                let ifi_recvquota = self.ifi_recvquota;
                let ifi_xmitquota = self.ifi_xmitquota;
                let ifi_unused1 = self.ifi_unused1;
                let ifi_mtu = self.ifi_mtu;
                let ifi_metric = self.ifi_metric;
                let ifi_baudrate = self.ifi_baudrate;
                let ifi_ipackets = self.ifi_ipackets;
                let ifi_ierrors = self.ifi_ierrors;
                let ifi_opackets = self.ifi_opackets;
                let ifi_oerrors = self.ifi_oerrors;
                let ifi_collisions = self.ifi_collisions;
                let ifi_ibytes = self.ifi_ibytes;
                let ifi_obytes = self.ifi_obytes;
                let ifi_imcasts = self.ifi_imcasts;
                let ifi_omcasts = self.ifi_omcasts;
                let ifi_iqdrops = self.ifi_iqdrops;
                let ifi_noproto = self.ifi_noproto;
                let ifi_recvtiming = self.ifi_recvtiming;
                let ifi_xmittiming = self.ifi_xmittiming;
                let ifi_lastchange = self.ifi_lastchange;
                ifi_type.hash(state);
                ifi_typelen.hash(state);
                ifi_physical.hash(state);
                ifi_addrlen.hash(state);
                ifi_hdrlen.hash(state);
                ifi_recvquota.hash(state);
                ifi_xmitquota.hash(state);
                ifi_unused1.hash(state);
                ifi_mtu.hash(state);
                ifi_metric.hash(state);
                ifi_baudrate.hash(state);
                ifi_ipackets.hash(state);
                ifi_ierrors.hash(state);
                ifi_opackets.hash(state);
                ifi_oerrors.hash(state);
                ifi_collisions.hash(state);
                ifi_ibytes.hash(state);
                ifi_obytes.hash(state);
                ifi_imcasts.hash(state);
                ifi_omcasts.hash(state);
                ifi_iqdrops.hash(state);
                ifi_noproto.hash(state);
                ifi_recvtiming.hash(state);
                ifi_xmittiming.hash(state);
                ifi_lastchange.hash(state);
            }
        }
        impl PartialEq for if_msghdr2 {
            fn eq(&self, other: &if_msghdr2) -> bool {
                self.ifm_msglen == other.ifm_msglen &&
                self.ifm_version == other.ifm_version &&
                self.ifm_type == other.ifm_type &&
                self.ifm_addrs == other.ifm_addrs &&
                self.ifm_flags == other.ifm_flags &&
                self.ifm_index == other.ifm_index &&
                self.ifm_snd_len == other.ifm_snd_len &&
                self.ifm_snd_maxlen == other.ifm_snd_maxlen &&
                self.ifm_snd_drops == other.ifm_snd_drops &&
                self.ifm_timer == other.ifm_timer &&
                self.ifm_data == other.ifm_data
            }
        }
        impl Eq for if_msghdr2 {}
        impl ::fmt::Debug for if_msghdr2 {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                let ifm_msglen = self.ifm_msglen;
                let ifm_version = self.ifm_version;
                let ifm_type = self.ifm_type;
                let ifm_addrs = self.ifm_addrs;
                let ifm_flags = self.ifm_flags;
                let ifm_index = self.ifm_index;
                let ifm_snd_len = self.ifm_snd_len;
                let ifm_snd_maxlen = self.ifm_snd_maxlen;
                let ifm_snd_drops = self.ifm_snd_drops;
                let ifm_timer = self.ifm_timer;
                let ifm_data = self.ifm_data;
                f.debug_struct("if_msghdr2")
                    .field("ifm_msglen", &ifm_msglen)
                    .field("ifm_version", &ifm_version)
                    .field("ifm_type", &ifm_type)
                    .field("ifm_addrs", &ifm_addrs)
                    .field("ifm_flags", &ifm_flags)
                    .field("ifm_index", &ifm_index)
                    .field("ifm_snd_len", &ifm_snd_len)
                    .field("ifm_snd_maxlen", &ifm_snd_maxlen)
                    .field("ifm_snd_drops", &ifm_snd_drops)
                    .field("ifm_timer", &ifm_timer)
                    .field("ifm_data", &ifm_data)
                    .finish()
            }
        }
        impl ::hash::Hash for if_msghdr2 {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                let ifm_msglen = self.ifm_msglen;
                let ifm_version = self.ifm_version;
                let ifm_type = self.ifm_type;
                let ifm_addrs = self.ifm_addrs;
                let ifm_flags = self.ifm_flags;
                let ifm_index = self.ifm_index;
                let ifm_snd_len = self.ifm_snd_len;
                let ifm_snd_maxlen = self.ifm_snd_maxlen;
                let ifm_snd_drops = self.ifm_snd_drops;
                let ifm_timer = self.ifm_timer;
                let ifm_data = self.ifm_data;
                ifm_msglen.hash(state);
                ifm_version.hash(state);
                ifm_type.hash(state);
                ifm_addrs.hash(state);
                ifm_flags.hash(state);
                ifm_index.hash(state);
                ifm_snd_len.hash(state);
                ifm_snd_maxlen.hash(state);
                ifm_snd_drops.hash(state);
                ifm_timer.hash(state);
                ifm_data.hash(state);
            }
        }
        impl PartialEq for vm_statistics64 {
            fn eq(&self, other: &vm_statistics64) -> bool {
                // Otherwise rustfmt crashes...
                let total_uncompressed = self.total_uncompressed_pages_in_compressor;
                self.free_count == other.free_count &&
                self.active_count == other.active_count &&
                self.inactive_count == other.inactive_count &&
                self.wire_count == other.wire_count &&
                self.zero_fill_count == other.zero_fill_count &&
                self.reactivations == other.reactivations &&
                self.pageins == other.pageins &&
                self.pageouts == other.pageouts &&
                self.faults == other.faults &&
                self.cow_faults == other.cow_faults &&
                self.lookups == other.lookups &&
                self.hits == other.hits &&
                self.purges == other.purges &&
                self.purgeable_count == other.purgeable_count &&
                self.speculative_count == other.speculative_count &&
                self.decompressions == other.decompressions &&
                self.compressions == other.compressions &&
                self.swapins == other.swapins &&
                self.swapouts == other.swapouts &&
                self.compressor_page_count == other.compressor_page_count &&
                self.throttled_count == other.throttled_count &&
                self.external_page_count == other.external_page_count &&
                self.internal_page_count == other.internal_page_count &&
                total_uncompressed == other.total_uncompressed_pages_in_compressor
            }
        }
        impl Eq for vm_statistics64 {}
        impl ::fmt::Debug for vm_statistics64 {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                let free_count = self.free_count;
                let active_count = self.active_count;
                let inactive_count = self.inactive_count;
                let wire_count = self.wire_count;
                let zero_fill_count = self.zero_fill_count;
                let reactivations = self.reactivations;
                let pageins = self.pageins;
                let pageouts = self.pageouts;
                let faults = self.faults;
                let cow_faults = self.cow_faults;
                let lookups = self.lookups;
                let hits = self.hits;
                let purges = self.purges;
                let purgeable_count = self.purgeable_count;
                let speculative_count = self.speculative_count;
                let decompressions = self.decompressions;
                let compressions = self.compressions;
                let swapins = self.swapins;
                let swapouts = self.swapouts;
                let compressor_page_count = self.compressor_page_count;
                let throttled_count = self.throttled_count;
                let external_page_count = self.external_page_count;
                let internal_page_count = self.internal_page_count;
                // Otherwise rustfmt crashes...
                let total_uncompressed = self.total_uncompressed_pages_in_compressor;
                f.debug_struct("vm_statistics64")
                    .field("free_count", &free_count)
                    .field("active_count", &active_count)
                    .field("inactive_count", &inactive_count)
                    .field("wire_count", &wire_count)
                    .field("zero_fill_count", &zero_fill_count)
                    .field("reactivations", &reactivations)
                    .field("pageins", &pageins)
                    .field("pageouts", &pageouts)
                    .field("faults", &faults)
                    .field("cow_faults", &cow_faults)
                    .field("lookups", &lookups)
                    .field("hits", &hits)
                    .field("purges", &purges)
                    .field("purgeable_count", &purgeable_count)
                    .field("speculative_count", &speculative_count)
                    .field("decompressions", &decompressions)
                    .field("compressions", &compressions)
                    .field("swapins", &swapins)
                    .field("swapouts", &swapouts)
                    .field("compressor_page_count", &compressor_page_count)
                    .field("throttled_count", &throttled_count)
                    .field("external_page_count", &external_page_count)
                    .field("internal_page_count", &internal_page_count)
                    .field("total_uncompressed_pages_in_compressor", &total_uncompressed)
                    .finish()
            }
        }
        impl ::hash::Hash for vm_statistics64 {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                let free_count = self.free_count;
                let active_count = self.active_count;
                let inactive_count = self.inactive_count;
                let wire_count = self.wire_count;
                let zero_fill_count = self.zero_fill_count;
                let reactivations = self.reactivations;
                let pageins = self.pageins;
                let pageouts = self.pageouts;
                let faults = self.faults;
                let cow_faults = self.cow_faults;
                let lookups = self.lookups;
                let hits = self.hits;
                let purges = self.purges;
                let purgeable_count = self.purgeable_count;
                let speculative_count = self.speculative_count;
                let decompressions = self.decompressions;
                let compressions = self.compressions;
                let swapins = self.swapins;
                let swapouts = self.swapouts;
                let compressor_page_count = self.compressor_page_count;
                let throttled_count = self.throttled_count;
                let external_page_count = self.external_page_count;
                let internal_page_count = self.internal_page_count;
                // Otherwise rustfmt crashes...
                let total_uncompressed = self.total_uncompressed_pages_in_compressor;
                free_count.hash(state);
                active_count.hash(state);
                inactive_count.hash(state);
                wire_count.hash(state);
                zero_fill_count.hash(state);
                reactivations.hash(state);
                pageins.hash(state);
                pageouts.hash(state);
                faults.hash(state);
                cow_faults.hash(state);
                lookups.hash(state);
                hits.hash(state);
                purges.hash(state);
                purgeable_count.hash(state);
                speculative_count.hash(state);
                decompressions.hash(state);
                compressions.hash(state);
                swapins.hash(state);
                swapouts.hash(state);
                compressor_page_count.hash(state);
                throttled_count.hash(state);
                external_page_count.hash(state);
                internal_page_count.hash(state);
                total_uncompressed.hash(state);
            }
        }

        impl PartialEq for mach_task_basic_info {
            fn eq(&self, other: &mach_task_basic_info) -> bool {
                self.virtual_size == other.virtual_size
                    && self.resident_size == other.resident_size
                    && self.resident_size_max == other.resident_size_max
                    && self.user_time == other.user_time
                    && self.system_time == other.system_time
                    && self.policy == other.policy
                    && self.suspend_count == other.suspend_count
            }
        }
        impl Eq for mach_task_basic_info {}
        impl ::fmt::Debug for mach_task_basic_info {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                let virtual_size = self.virtual_size;
                let resident_size = self.resident_size;
                let resident_size_max = self.resident_size_max;
                let user_time = self.user_time;
                let system_time = self.system_time;
                let policy = self.policy;
                let suspend_count = self.suspend_count;
                f.debug_struct("mach_task_basic_info")
                    .field("virtual_size", &virtual_size)
                    .field("resident_size", &resident_size)
                    .field("resident_size_max", &resident_size_max)
                    .field("user_time", &user_time)
                    .field("system_time", &system_time)
                    .field("policy", &policy)
                    .field("suspend_count", &suspend_count)
                    .finish()
            }
        }
        impl ::hash::Hash for mach_task_basic_info {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                let virtual_size = self.virtual_size;
                let resident_size = self.resident_size;
                let resident_size_max = self.resident_size_max;
                let user_time = self.user_time;
                let system_time = self.system_time;
                let policy = self.policy;
                let suspend_count = self.suspend_count;
                virtual_size.hash(state);
                resident_size.hash(state);
                resident_size_max.hash(state);
                user_time.hash(state);
                system_time.hash(state);
                policy.hash(state);
                suspend_count.hash(state);
            }
        }
    }
}

pub const _UTX_USERSIZE: usize = 256;
pub const _UTX_LINESIZE: usize = 32;
pub const _UTX_IDSIZE: usize = 4;
pub const _UTX_HOSTSIZE: usize = 256;

pub const EMPTY: ::c_short = 0;
pub const RUN_LVL: ::c_short = 1;
pub const BOOT_TIME: ::c_short = 2;
pub const OLD_TIME: ::c_short = 3;
pub const NEW_TIME: ::c_short = 4;
pub const INIT_PROCESS: ::c_short = 5;
pub const LOGIN_PROCESS: ::c_short = 6;
pub const USER_PROCESS: ::c_short = 7;
pub const DEAD_PROCESS: ::c_short = 8;
pub const ACCOUNTING: ::c_short = 9;
pub const SIGNATURE: ::c_short = 10;
pub const SHUTDOWN_TIME: ::c_short = 11;

pub const LC_COLLATE_MASK: ::c_int = 1 << 0;
pub const LC_CTYPE_MASK: ::c_int = 1 << 1;
pub const LC_MESSAGES_MASK: ::c_int = 1 << 2;
pub const LC_MONETARY_MASK: ::c_int = 1 << 3;
pub const LC_NUMERIC_MASK: ::c_int = 1 << 4;
pub const LC_TIME_MASK: ::c_int = 1 << 5;
pub const LC_ALL_MASK: ::c_int = LC_COLLATE_MASK
    | LC_CTYPE_MASK
    | LC_MESSAGES_MASK
    | LC_MONETARY_MASK
    | LC_NUMERIC_MASK
    | LC_TIME_MASK;

pub const CODESET: ::nl_item = 0;
pub const D_T_FMT: ::nl_item = 1;
pub const D_FMT: ::nl_item = 2;
pub const T_FMT: ::nl_item = 3;
pub const T_FMT_AMPM: ::nl_item = 4;
pub const AM_STR: ::nl_item = 5;
pub const PM_STR: ::nl_item = 6;

pub const DAY_1: ::nl_item = 7;
pub const DAY_2: ::nl_item = 8;
pub const DAY_3: ::nl_item = 9;
pub const DAY_4: ::nl_item = 10;
pub const DAY_5: ::nl_item = 11;
pub const DAY_6: ::nl_item = 12;
pub const DAY_7: ::nl_item = 13;

pub const ABDAY_1: ::nl_item = 14;
pub const ABDAY_2: ::nl_item = 15;
pub const ABDAY_3: ::nl_item = 16;
pub const ABDAY_4: ::nl_item = 17;
pub const ABDAY_5: ::nl_item = 18;
pub const ABDAY_6: ::nl_item = 19;
pub const ABDAY_7: ::nl_item = 20;

pub const MON_1: ::nl_item = 21;
pub const MON_2: ::nl_item = 22;
pub const MON_3: ::nl_item = 23;
pub const MON_4: ::nl_item = 24;
pub const MON_5: ::nl_item = 25;
pub const MON_6: ::nl_item = 26;
pub const MON_7: ::nl_item = 27;
pub const MON_8: ::nl_item = 28;
pub const MON_9: ::nl_item = 29;
pub const MON_10: ::nl_item = 30;
pub const MON_11: ::nl_item = 31;
pub const MON_12: ::nl_item = 32;

pub const ABMON_1: ::nl_item = 33;
pub const ABMON_2: ::nl_item = 34;
pub const ABMON_3: ::nl_item = 35;
pub const ABMON_4: ::nl_item = 36;
pub const ABMON_5: ::nl_item = 37;
pub const ABMON_6: ::nl_item = 38;
pub const ABMON_7: ::nl_item = 39;
pub const ABMON_8: ::nl_item = 40;
pub const ABMON_9: ::nl_item = 41;
pub const ABMON_10: ::nl_item = 42;
pub const ABMON_11: ::nl_item = 43;
pub const ABMON_12: ::nl_item = 44;

pub const CLOCK_REALTIME: ::clockid_t = 0;
pub const CLOCK_MONOTONIC: ::clockid_t = 6;
pub const CLOCK_PROCESS_CPUTIME_ID: ::clockid_t = 12;
pub const CLOCK_THREAD_CPUTIME_ID: ::clockid_t = 16;

pub const ERA: ::nl_item = 45;
pub const ERA_D_FMT: ::nl_item = 46;
pub const ERA_D_T_FMT: ::nl_item = 47;
pub const ERA_T_FMT: ::nl_item = 48;
pub const ALT_DIGITS: ::nl_item = 49;

pub const RADIXCHAR: ::nl_item = 50;
pub const THOUSEP: ::nl_item = 51;

pub const YESEXPR: ::nl_item = 52;
pub const NOEXPR: ::nl_item = 53;

pub const YESSTR: ::nl_item = 54;
pub const NOSTR: ::nl_item = 55;

pub const CRNCYSTR: ::nl_item = 56;

pub const D_MD_ORDER: ::nl_item = 57;

pub const EXIT_FAILURE: ::c_int = 1;
pub const EXIT_SUCCESS: ::c_int = 0;
pub const RAND_MAX: ::c_int = 2147483647;
pub const EOF: ::c_int = -1;
pub const SEEK_SET: ::c_int = 0;
pub const SEEK_CUR: ::c_int = 1;
pub const SEEK_END: ::c_int = 2;
pub const _IOFBF: ::c_int = 0;
pub const _IONBF: ::c_int = 2;
pub const _IOLBF: ::c_int = 1;
pub const BUFSIZ: ::c_uint = 1024;
pub const FOPEN_MAX: ::c_uint = 20;
pub const FILENAME_MAX: ::c_uint = 1024;
pub const L_tmpnam: ::c_uint = 1024;
pub const TMP_MAX: ::c_uint = 308915776;
pub const _PC_LINK_MAX: ::c_int = 1;
pub const _PC_MAX_CANON: ::c_int = 2;
pub const _PC_MAX_INPUT: ::c_int = 3;
pub const _PC_NAME_MAX: ::c_int = 4;
pub const _PC_PATH_MAX: ::c_int = 5;
pub const _PC_PIPE_BUF: ::c_int = 6;
pub const _PC_CHOWN_RESTRICTED: ::c_int = 7;
pub const _PC_NO_TRUNC: ::c_int = 8;
pub const _PC_VDISABLE: ::c_int = 9;
pub const O_DSYNC: ::c_int = 0x400000;
pub const O_NOCTTY: ::c_int = 0x20000;
pub const O_CLOEXEC: ::c_int = 0x1000000;
pub const O_DIRECTORY: ::c_int = 0x100000;
pub const O_SYMLINK: ::c_int = 0x200000;
pub const S_IFIFO: mode_t = 4096;
pub const S_IFCHR: mode_t = 8192;
pub const S_IFBLK: mode_t = 24576;
pub const S_IFDIR: mode_t = 16384;
pub const S_IFREG: mode_t = 32768;
pub const S_IFLNK: mode_t = 40960;
pub const S_IFSOCK: mode_t = 49152;
pub const S_IFMT: mode_t = 61440;
pub const S_IEXEC: mode_t = 64;
pub const S_IWRITE: mode_t = 128;
pub const S_IREAD: mode_t = 256;
pub const S_IRWXU: mode_t = 448;
pub const S_IXUSR: mode_t = 64;
pub const S_IWUSR: mode_t = 128;
pub const S_IRUSR: mode_t = 256;
pub const S_IRWXG: mode_t = 56;
pub const S_IXGRP: mode_t = 8;
pub const S_IWGRP: mode_t = 16;
pub const S_IRGRP: mode_t = 32;
pub const S_IRWXO: mode_t = 7;
pub const S_IXOTH: mode_t = 1;
pub const S_IWOTH: mode_t = 2;
pub const S_IROTH: mode_t = 4;
pub const F_OK: ::c_int = 0;
pub const R_OK: ::c_int = 4;
pub const W_OK: ::c_int = 2;
pub const X_OK: ::c_int = 1;
pub const STDIN_FILENO: ::c_int = 0;
pub const STDOUT_FILENO: ::c_int = 1;
pub const STDERR_FILENO: ::c_int = 2;
pub const F_LOCK: ::c_int = 1;
pub const F_TEST: ::c_int = 3;
pub const F_TLOCK: ::c_int = 2;
pub const F_ULOCK: ::c_int = 0;
pub const F_GETLK: ::c_int = 7;
pub const F_SETLK: ::c_int = 8;
pub const F_SETLKW: ::c_int = 9;
pub const SIGHUP: ::c_int = 1;
pub const SIGINT: ::c_int = 2;
pub const SIGQUIT: ::c_int = 3;
pub const SIGILL: ::c_int = 4;
pub const SIGABRT: ::c_int = 6;
pub const SIGEMT: ::c_int = 7;
pub const SIGFPE: ::c_int = 8;
pub const SIGKILL: ::c_int = 9;
pub const SIGSEGV: ::c_int = 11;
pub const SIGPIPE: ::c_int = 13;
pub const SIGALRM: ::c_int = 14;
pub const SIGTERM: ::c_int = 15;

pub const PROT_NONE: ::c_int = 0;
pub const PROT_READ: ::c_int = 1;
pub const PROT_WRITE: ::c_int = 2;
pub const PROT_EXEC: ::c_int = 4;

pub const PT_TRACE_ME: ::c_int = 0;
pub const PT_READ_I: ::c_int = 1;
pub const PT_READ_D: ::c_int = 2;
pub const PT_READ_U: ::c_int = 3;
pub const PT_WRITE_I: ::c_int = 4;
pub const PT_WRITE_D: ::c_int = 5;
pub const PT_WRITE_U: ::c_int = 6;
pub const PT_CONTINUE: ::c_int = 7;
pub const PT_KILL: ::c_int = 8;
pub const PT_STEP: ::c_int = 9;
pub const PT_ATTACH: ::c_int = 10;
pub const PT_DETACH: ::c_int = 11;
pub const PT_SIGEXC: ::c_int = 12;
pub const PT_THUPDATE: ::c_int = 13;
pub const PT_ATTACHEXC: ::c_int = 14;

pub const PT_FORCEQUOTA: ::c_int = 30;
pub const PT_DENY_ATTACH: ::c_int = 31;
pub const PT_FIRSTMACH: ::c_int = 32;

pub const MAP_FILE: ::c_int = 0x0000;
pub const MAP_SHARED: ::c_int = 0x0001;
pub const MAP_PRIVATE: ::c_int = 0x0002;
pub const MAP_FIXED: ::c_int = 0x0010;
pub const MAP_ANON: ::c_int = 0x1000;
pub const MAP_ANONYMOUS: ::c_int = MAP_ANON;

pub const CPU_STATE_USER: ::c_int = 0;
pub const CPU_STATE_SYSTEM: ::c_int = 1;
pub const CPU_STATE_IDLE: ::c_int = 2;
pub const CPU_STATE_NICE: ::c_int = 3;
pub const CPU_STATE_MAX: ::c_int = 4;

pub const PROCESSOR_BASIC_INFO: ::c_int = 1;
pub const PROCESSOR_CPU_LOAD_INFO: ::c_int = 2;
pub const PROCESSOR_PM_REGS_INFO: ::c_int = 0x10000001;
pub const PROCESSOR_TEMPERATURE: ::c_int = 0x10000002;
pub const PROCESSOR_SET_LOAD_INFO: ::c_int = 4;
pub const PROCESSOR_SET_BASIC_INFO: ::c_int = 5;

deprecated_mach! {
    pub const VM_FLAGS_FIXED: ::c_int = 0x0000;
    pub const VM_FLAGS_ANYWHERE: ::c_int = 0x0001;
    pub const VM_FLAGS_PURGABLE: ::c_int = 0x0002;
    pub const VM_FLAGS_RANDOM_ADDR: ::c_int = 0x0008;
    pub const VM_FLAGS_NO_CACHE: ::c_int = 0x0010;
    pub const VM_FLAGS_RESILIENT_CODESIGN: ::c_int = 0x0020;
    pub const VM_FLAGS_RESILIENT_MEDIA: ::c_int = 0x0040;
    pub const VM_FLAGS_OVERWRITE: ::c_int = 0x4000;
    pub const VM_FLAGS_SUPERPAGE_MASK: ::c_int = 0x70000;
    pub const VM_FLAGS_RETURN_DATA_ADDR: ::c_int = 0x100000;
    pub const VM_FLAGS_RETURN_4K_DATA_ADDR: ::c_int = 0x800000;
    pub const VM_FLAGS_ALIAS_MASK: ::c_int = 0xFF000000;
    pub const VM_FLAGS_USER_ALLOCATE: ::c_int = 0xff07401f;
    pub const VM_FLAGS_USER_MAP: ::c_int = 0xff97401f;
    pub const VM_FLAGS_USER_REMAP: ::c_int = VM_FLAGS_FIXED |
                                             VM_FLAGS_ANYWHERE |
                                             VM_FLAGS_RANDOM_ADDR |
                                             VM_FLAGS_OVERWRITE |
                                             VM_FLAGS_RETURN_DATA_ADDR |
                                             VM_FLAGS_RESILIENT_CODESIGN;

    pub const VM_FLAGS_SUPERPAGE_SHIFT: ::c_int = 16;
    pub const SUPERPAGE_NONE: ::c_int = 0;
    pub const SUPERPAGE_SIZE_ANY: ::c_int = 1;
    pub const VM_FLAGS_SUPERPAGE_NONE: ::c_int = SUPERPAGE_NONE <<
                                                 VM_FLAGS_SUPERPAGE_SHIFT;
    pub const VM_FLAGS_SUPERPAGE_SIZE_ANY: ::c_int = SUPERPAGE_SIZE_ANY <<
                                                     VM_FLAGS_SUPERPAGE_SHIFT;
    pub const SUPERPAGE_SIZE_2MB: ::c_int = 2;
    pub const VM_FLAGS_SUPERPAGE_SIZE_2MB: ::c_int = SUPERPAGE_SIZE_2MB <<
                                                     VM_FLAGS_SUPERPAGE_SHIFT;

    pub const VM_MEMORY_MALLOC: ::c_int = 1;
    pub const VM_MEMORY_MALLOC_SMALL: ::c_int = 2;
    pub const VM_MEMORY_MALLOC_LARGE: ::c_int = 3;
    pub const VM_MEMORY_MALLOC_HUGE: ::c_int = 4;
    pub const VM_MEMORY_SBRK: ::c_int = 5;
    pub const VM_MEMORY_REALLOC: ::c_int = 6;
    pub const VM_MEMORY_MALLOC_TINY: ::c_int = 7;
    pub const VM_MEMORY_MALLOC_LARGE_REUSABLE: ::c_int = 8;
    pub const VM_MEMORY_MALLOC_LARGE_REUSED: ::c_int = 9;
    pub const VM_MEMORY_ANALYSIS_TOOL: ::c_int = 10;
    pub const VM_MEMORY_MALLOC_NANO: ::c_int = 11;
    pub const VM_MEMORY_MACH_MSG: ::c_int = 20;
    pub const VM_MEMORY_IOKIT: ::c_int = 21;
    pub const VM_MEMORY_STACK: ::c_int = 30;
    pub const VM_MEMORY_GUARD: ::c_int = 31;
    pub const VM_MEMORY_SHARED_PMAP: ::c_int = 32;
    pub const VM_MEMORY_DYLIB: ::c_int = 33;
    pub const VM_MEMORY_OBJC_DISPATCHERS: ::c_int = 34;
    pub const VM_MEMORY_UNSHARED_PMAP: ::c_int = 35;
    pub const VM_MEMORY_APPKIT: ::c_int = 40;
    pub const VM_MEMORY_FOUNDATION: ::c_int = 41;
    pub const VM_MEMORY_COREGRAPHICS: ::c_int = 42;
    pub const VM_MEMORY_CORESERVICES: ::c_int = 43;
    pub const VM_MEMORY_CARBON: ::c_int = VM_MEMORY_CORESERVICES;
    pub const VM_MEMORY_JAVA: ::c_int = 44;
    pub const VM_MEMORY_COREDATA: ::c_int = 45;
    pub const VM_MEMORY_COREDATA_OBJECTIDS: ::c_int = 46;
    pub const VM_MEMORY_ATS: ::c_int = 50;
    pub const VM_MEMORY_LAYERKIT: ::c_int = 51;
    pub const VM_MEMORY_CGIMAGE: ::c_int = 52;
    pub const VM_MEMORY_TCMALLOC: ::c_int = 53;
    pub const VM_MEMORY_COREGRAPHICS_DATA: ::c_int = 54;
    pub const VM_MEMORY_COREGRAPHICS_SHARED: ::c_int = 55;
    pub const VM_MEMORY_COREGRAPHICS_FRAMEBUFFERS: ::c_int = 56;
    pub const VM_MEMORY_COREGRAPHICS_BACKINGSTORES: ::c_int = 57;
    pub const VM_MEMORY_COREGRAPHICS_XALLOC: ::c_int = 58;
    pub const VM_MEMORY_COREGRAPHICS_MISC: ::c_int = VM_MEMORY_COREGRAPHICS;
    pub const VM_MEMORY_DYLD: ::c_int = 60;
    pub const VM_MEMORY_DYLD_MALLOC: ::c_int = 61;
    pub const VM_MEMORY_SQLITE: ::c_int = 62;
    pub const VM_MEMORY_JAVASCRIPT_CORE: ::c_int = 63;
    pub const VM_MEMORY_JAVASCRIPT_JIT_EXECUTABLE_ALLOCATOR: ::c_int = 64;
    pub const VM_MEMORY_JAVASCRIPT_JIT_REGISTER_FILE: ::c_int = 65;
    pub const VM_MEMORY_GLSL: ::c_int = 66;
    pub const VM_MEMORY_OPENCL: ::c_int = 67;
    pub const VM_MEMORY_COREIMAGE: ::c_int = 68;
    pub const VM_MEMORY_WEBCORE_PURGEABLE_BUFFERS: ::c_int = 69;
    pub const VM_MEMORY_IMAGEIO: ::c_int = 70;
    pub const VM_MEMORY_COREPROFILE: ::c_int = 71;
    pub const VM_MEMORY_ASSETSD: ::c_int = 72;
    pub const VM_MEMORY_OS_ALLOC_ONCE: ::c_int = 73;
    pub const VM_MEMORY_LIBDISPATCH: ::c_int = 74;
    pub const VM_MEMORY_ACCELERATE: ::c_int = 75;
    pub const VM_MEMORY_COREUI: ::c_int = 76;
    pub const VM_MEMORY_COREUIFILE: ::c_int = 77;
    pub const VM_MEMORY_GENEALOGY: ::c_int = 78;
    pub const VM_MEMORY_RAWCAMERA: ::c_int = 79;
    pub const VM_MEMORY_CORPSEINFO: ::c_int = 80;
    pub const VM_MEMORY_ASL: ::c_int = 81;
    pub const VM_MEMORY_SWIFT_RUNTIME: ::c_int = 82;
    pub const VM_MEMORY_SWIFT_METADATA: ::c_int = 83;
    pub const VM_MEMORY_DHMM: ::c_int = 84;
    pub const VM_MEMORY_SCENEKIT: ::c_int = 86;
    pub const VM_MEMORY_SKYWALK: ::c_int = 87;
    pub const VM_MEMORY_APPLICATION_SPECIFIC_1: ::c_int = 240;
    pub const VM_MEMORY_APPLICATION_SPECIFIC_16: ::c_int = 255;
}

pub const MAP_FAILED: *mut ::c_void = !0 as *mut ::c_void;

pub const MCL_CURRENT: ::c_int = 0x0001;
pub const MCL_FUTURE: ::c_int = 0x0002;

pub const MS_ASYNC: ::c_int = 0x0001;
pub const MS_INVALIDATE: ::c_int = 0x0002;
pub const MS_SYNC: ::c_int = 0x0010;

pub const MS_KILLPAGES: ::c_int = 0x0004;
pub const MS_DEACTIVATE: ::c_int = 0x0008;

pub const EPERM: ::c_int = 1;
pub const ENOENT: ::c_int = 2;
pub const ESRCH: ::c_int = 3;
pub const EINTR: ::c_int = 4;
pub const EIO: ::c_int = 5;
pub const ENXIO: ::c_int = 6;
pub const E2BIG: ::c_int = 7;
pub const ENOEXEC: ::c_int = 8;
pub const EBADF: ::c_int = 9;
pub const ECHILD: ::c_int = 10;
pub const EDEADLK: ::c_int = 11;
pub const ENOMEM: ::c_int = 12;
pub const EACCES: ::c_int = 13;
pub const EFAULT: ::c_int = 14;
pub const ENOTBLK: ::c_int = 15;
pub const EBUSY: ::c_int = 16;
pub const EEXIST: ::c_int = 17;
pub const EXDEV: ::c_int = 18;
pub const ENODEV: ::c_int = 19;
pub const ENOTDIR: ::c_int = 20;
pub const EISDIR: ::c_int = 21;
pub const EINVAL: ::c_int = 22;
pub const ENFILE: ::c_int = 23;
pub const EMFILE: ::c_int = 24;
pub const ENOTTY: ::c_int = 25;
pub const ETXTBSY: ::c_int = 26;
pub const EFBIG: ::c_int = 27;
pub const ENOSPC: ::c_int = 28;
pub const ESPIPE: ::c_int = 29;
pub const EROFS: ::c_int = 30;
pub const EMLINK: ::c_int = 31;
pub const EPIPE: ::c_int = 32;
pub const EDOM: ::c_int = 33;
pub const ERANGE: ::c_int = 34;
pub const EAGAIN: ::c_int = 35;
pub const EWOULDBLOCK: ::c_int = EAGAIN;
pub const EINPROGRESS: ::c_int = 36;
pub const EALREADY: ::c_int = 37;
pub const ENOTSOCK: ::c_int = 38;
pub const EDESTADDRREQ: ::c_int = 39;
pub const EMSGSIZE: ::c_int = 40;
pub const EPROTOTYPE: ::c_int = 41;
pub const ENOPROTOOPT: ::c_int = 42;
pub const EPROTONOSUPPORT: ::c_int = 43;
pub const ESOCKTNOSUPPORT: ::c_int = 44;
pub const ENOTSUP: ::c_int = 45;
pub const EPFNOSUPPORT: ::c_int = 46;
pub const EAFNOSUPPORT: ::c_int = 47;
pub const EADDRINUSE: ::c_int = 48;
pub const EADDRNOTAVAIL: ::c_int = 49;
pub const ENETDOWN: ::c_int = 50;
pub const ENETUNREACH: ::c_int = 51;
pub const ENETRESET: ::c_int = 52;
pub const ECONNABORTED: ::c_int = 53;
pub const ECONNRESET: ::c_int = 54;
pub const ENOBUFS: ::c_int = 55;
pub const EISCONN: ::c_int = 56;
pub const ENOTCONN: ::c_int = 57;
pub const ESHUTDOWN: ::c_int = 58;
pub const ETOOMANYREFS: ::c_int = 59;
pub const ETIMEDOUT: ::c_int = 60;
pub const ECONNREFUSED: ::c_int = 61;
pub const ELOOP: ::c_int = 62;
pub const ENAMETOOLONG: ::c_int = 63;
pub const EHOSTDOWN: ::c_int = 64;
pub const EHOSTUNREACH: ::c_int = 65;
pub const ENOTEMPTY: ::c_int = 66;
pub const EPROCLIM: ::c_int = 67;
pub const EUSERS: ::c_int = 68;
pub const EDQUOT: ::c_int = 69;
pub const ESTALE: ::c_int = 70;
pub const EREMOTE: ::c_int = 71;
pub const EBADRPC: ::c_int = 72;
pub const ERPCMISMATCH: ::c_int = 73;
pub const EPROGUNAVAIL: ::c_int = 74;
pub const EPROGMISMATCH: ::c_int = 75;
pub const EPROCUNAVAIL: ::c_int = 76;
pub const ENOLCK: ::c_int = 77;
pub const ENOSYS: ::c_int = 78;
pub const EFTYPE: ::c_int = 79;
pub const EAUTH: ::c_int = 80;
pub const ENEEDAUTH: ::c_int = 81;
pub const EPWROFF: ::c_int = 82;
pub const EDEVERR: ::c_int = 83;
pub const EOVERFLOW: ::c_int = 84;
pub const EBADEXEC: ::c_int = 85;
pub const EBADARCH: ::c_int = 86;
pub const ESHLIBVERS: ::c_int = 87;
pub const EBADMACHO: ::c_int = 88;
pub const ECANCELED: ::c_int = 89;
pub const EIDRM: ::c_int = 90;
pub const ENOMSG: ::c_int = 91;
pub const EILSEQ: ::c_int = 92;
pub const ENOATTR: ::c_int = 93;
pub const EBADMSG: ::c_int = 94;
pub const EMULTIHOP: ::c_int = 95;
pub const ENODATA: ::c_int = 96;
pub const ENOLINK: ::c_int = 97;
pub const ENOSR: ::c_int = 98;
pub const ENOSTR: ::c_int = 99;
pub const EPROTO: ::c_int = 100;
pub const ETIME: ::c_int = 101;
pub const EOPNOTSUPP: ::c_int = 102;
pub const ENOPOLICY: ::c_int = 103;
pub const ENOTRECOVERABLE: ::c_int = 104;
pub const EOWNERDEAD: ::c_int = 105;
pub const EQFULL: ::c_int = 106;
pub const ELAST: ::c_int = 106;

pub const EAI_AGAIN: ::c_int = 2;
pub const EAI_BADFLAGS: ::c_int = 3;
pub const EAI_FAIL: ::c_int = 4;
pub const EAI_FAMILY: ::c_int = 5;
pub const EAI_MEMORY: ::c_int = 6;
pub const EAI_NODATA: ::c_int = 7;
pub const EAI_NONAME: ::c_int = 8;
pub const EAI_SERVICE: ::c_int = 9;
pub const EAI_SOCKTYPE: ::c_int = 10;
pub const EAI_SYSTEM: ::c_int = 11;
pub const EAI_OVERFLOW: ::c_int = 14;

pub const F_DUPFD: ::c_int = 0;
pub const F_DUPFD_CLOEXEC: ::c_int = 67;
pub const F_GETFD: ::c_int = 1;
pub const F_SETFD: ::c_int = 2;
pub const F_GETFL: ::c_int = 3;
pub const F_SETFL: ::c_int = 4;
pub const F_PREALLOCATE: ::c_int = 42;
pub const F_RDADVISE: ::c_int = 44;
pub const F_RDAHEAD: ::c_int = 45;
pub const F_NOCACHE: ::c_int = 48;
pub const F_GETPATH: ::c_int = 50;
pub const F_FULLFSYNC: ::c_int = 51;
pub const F_FREEZE_FS: ::c_int = 53;
pub const F_THAW_FS: ::c_int = 54;
pub const F_GLOBAL_NOCACHE: ::c_int = 55;
pub const F_NODIRECT: ::c_int = 62;

pub const F_ALLOCATECONTIG: ::c_uint = 0x02;
pub const F_ALLOCATEALL: ::c_uint = 0x04;

pub const F_PEOFPOSMODE: ::c_int = 3;
pub const F_VOLPOSMODE: ::c_int = 4;

pub const AT_FDCWD: ::c_int = -2;
pub const AT_EACCESS: ::c_int = 0x0010;
pub const AT_SYMLINK_NOFOLLOW: ::c_int = 0x0020;
pub const AT_SYMLINK_FOLLOW: ::c_int = 0x0040;
pub const AT_REMOVEDIR: ::c_int = 0x0080;

pub const PTHREAD_INTROSPECTION_THREAD_CREATE: ::c_uint = 1;
pub const PTHREAD_INTROSPECTION_THREAD_START: ::c_uint = 2;
pub const PTHREAD_INTROSPECTION_THREAD_TERMINATE: ::c_uint = 3;
pub const PTHREAD_INTROSPECTION_THREAD_DESTROY: ::c_uint = 4;

pub const TIOCMODG: ::c_ulong = 0x40047403;
pub const TIOCMODS: ::c_ulong = 0x80047404;
pub const TIOCM_LE: ::c_int = 0x1;
pub const TIOCM_DTR: ::c_int = 0x2;
pub const TIOCM_RTS: ::c_int = 0x4;
pub const TIOCM_ST: ::c_int = 0x8;
pub const TIOCM_SR: ::c_int = 0x10;
pub const TIOCM_CTS: ::c_int = 0x20;
pub const TIOCM_CAR: ::c_int = 0x40;
pub const TIOCM_CD: ::c_int = 0x40;
pub const TIOCM_RNG: ::c_int = 0x80;
pub const TIOCM_RI: ::c_int = 0x80;
pub const TIOCM_DSR: ::c_int = 0x100;
pub const TIOCEXCL: ::c_int = 0x2000740d;
pub const TIOCNXCL: ::c_int = 0x2000740e;
pub const TIOCFLUSH: ::c_ulong = 0x80047410;
pub const TIOCGETD: ::c_ulong = 0x4004741a;
pub const TIOCSETD: ::c_ulong = 0x8004741b;
pub const TIOCIXON: ::c_uint = 0x20007481;
pub const TIOCIXOFF: ::c_uint = 0x20007480;
pub const TIOCSDTR: ::c_uint = 0x20007479;
pub const TIOCCDTR: ::c_uint = 0x20007478;
pub const TIOCGPGRP: ::c_ulong = 0x40047477;
pub const TIOCSPGRP: ::c_ulong = 0x80047476;
pub const TIOCOUTQ: ::c_ulong = 0x40047473;
pub const TIOCSTI: ::c_ulong = 0x80017472;
pub const TIOCNOTTY: ::c_uint = 0x20007471;
pub const TIOCPKT: ::c_ulong = 0x80047470;
pub const TIOCPKT_DATA: ::c_int = 0x0;
pub const TIOCPKT_FLUSHREAD: ::c_int = 0x1;
pub const TIOCPKT_FLUSHWRITE: ::c_int = 0x2;
pub const TIOCPKT_STOP: ::c_int = 0x4;
pub const TIOCPKT_START: ::c_int = 0x8;
pub const TIOCPKT_NOSTOP: ::c_int = 0x10;
pub const TIOCPKT_DOSTOP: ::c_int = 0x20;
pub const TIOCPKT_IOCTL: ::c_int = 0x40;
pub const TIOCSTOP: ::c_uint = 0x2000746f;
pub const TIOCSTART: ::c_uint = 0x2000746e;
pub const TIOCMSET: ::c_ulong = 0x8004746d;
pub const TIOCMBIS: ::c_ulong = 0x8004746c;
pub const TIOCMBIC: ::c_ulong = 0x8004746b;
pub const TIOCMGET: ::c_ulong = 0x4004746a;
pub const TIOCREMOTE: ::c_ulong = 0x80047469;
pub const TIOCGWINSZ: ::c_ulong = 0x40087468;
pub const TIOCSWINSZ: ::c_ulong = 0x80087467;
pub const TIOCUCNTL: ::c_ulong = 0x80047466;
pub const TIOCSTAT: ::c_uint = 0x20007465;
pub const TIOCSCONS: ::c_uint = 0x20007463;
pub const TIOCCONS: ::c_ulong = 0x80047462;
pub const TIOCSCTTY: ::c_uint = 0x20007461;
pub const TIOCEXT: ::c_ulong = 0x80047460;
pub const TIOCSIG: ::c_uint = 0x2000745f;
pub const TIOCDRAIN: ::c_uint = 0x2000745e;
pub const TIOCMSDTRWAIT: ::c_ulong = 0x8004745b;
pub const TIOCMGDTRWAIT: ::c_ulong = 0x4004745a;
pub const TIOCSDRAINWAIT: ::c_ulong = 0x80047457;
pub const TIOCGDRAINWAIT: ::c_ulong = 0x40047456;
pub const TIOCDSIMICROCODE: ::c_uint = 0x20007455;
pub const TIOCPTYGRANT: ::c_uint = 0x20007454;
pub const TIOCPTYGNAME: ::c_uint = 0x40807453;
pub const TIOCPTYUNLK: ::c_uint = 0x20007452;

pub const BIOCGRSIG: ::c_ulong = 0x40044272;
pub const BIOCSRSIG: ::c_ulong = 0x80044273;
pub const BIOCSDLT: ::c_ulong = 0x80044278;
pub const BIOCGSEESENT: ::c_ulong = 0x40044276;
pub const BIOCSSEESENT: ::c_ulong = 0x80044277;
pub const BIOCGDLTLIST: ::c_ulong = 0xc00c4279;

pub const FIODTYPE: ::c_ulong = 0x4004667a;

pub const B0: speed_t = 0;
pub const B50: speed_t = 50;
pub const B75: speed_t = 75;
pub const B110: speed_t = 110;
pub const B134: speed_t = 134;
pub const B150: speed_t = 150;
pub const B200: speed_t = 200;
pub const B300: speed_t = 300;
pub const B600: speed_t = 600;
pub const B1200: speed_t = 1200;
pub const B1800: speed_t = 1800;
pub const B2400: speed_t = 2400;
pub const B4800: speed_t = 4800;
pub const B9600: speed_t = 9600;
pub const B19200: speed_t = 19200;
pub const B38400: speed_t = 38400;
pub const B7200: speed_t = 7200;
pub const B14400: speed_t = 14400;
pub const B28800: speed_t = 28800;
pub const B57600: speed_t = 57600;
pub const B76800: speed_t = 76800;
pub const B115200: speed_t = 115200;
pub const B230400: speed_t = 230400;
pub const EXTA: speed_t = 19200;
pub const EXTB: speed_t = 38400;

pub const SIGTRAP: ::c_int = 5;

pub const GLOB_APPEND: ::c_int = 0x0001;
pub const GLOB_DOOFFS: ::c_int = 0x0002;
pub const GLOB_ERR: ::c_int = 0x0004;
pub const GLOB_MARK: ::c_int = 0x0008;
pub const GLOB_NOCHECK: ::c_int = 0x0010;
pub const GLOB_NOSORT: ::c_int = 0x0020;
pub const GLOB_NOESCAPE: ::c_int = 0x2000;

pub const GLOB_NOSPACE: ::c_int = -1;
pub const GLOB_ABORTED: ::c_int = -2;
pub const GLOB_NOMATCH: ::c_int = -3;

pub const POSIX_MADV_NORMAL: ::c_int = 0;
pub const POSIX_MADV_RANDOM: ::c_int = 1;
pub const POSIX_MADV_SEQUENTIAL: ::c_int = 2;
pub const POSIX_MADV_WILLNEED: ::c_int = 3;
pub const POSIX_MADV_DONTNEED: ::c_int = 4;

pub const _SC_IOV_MAX: ::c_int = 56;
pub const _SC_GETGR_R_SIZE_MAX: ::c_int = 70;
pub const _SC_GETPW_R_SIZE_MAX: ::c_int = 71;
pub const _SC_LOGIN_NAME_MAX: ::c_int = 73;
pub const _SC_MQ_PRIO_MAX: ::c_int = 75;
pub const _SC_THREAD_ATTR_STACKADDR: ::c_int = 82;
pub const _SC_THREAD_ATTR_STACKSIZE: ::c_int = 83;
pub const _SC_THREAD_DESTRUCTOR_ITERATIONS: ::c_int = 85;
pub const _SC_THREAD_KEYS_MAX: ::c_int = 86;
pub const _SC_THREAD_PRIO_INHERIT: ::c_int = 87;
pub const _SC_THREAD_PRIO_PROTECT: ::c_int = 88;
pub const _SC_THREAD_PRIORITY_SCHEDULING: ::c_int = 89;
pub const _SC_THREAD_PROCESS_SHARED: ::c_int = 90;
pub const _SC_THREAD_SAFE_FUNCTIONS: ::c_int = 91;
pub const _SC_THREAD_STACK_MIN: ::c_int = 93;
pub const _SC_THREAD_THREADS_MAX: ::c_int = 94;
pub const _SC_THREADS: ::c_int = 96;
pub const _SC_TTY_NAME_MAX: ::c_int = 101;
pub const _SC_ATEXIT_MAX: ::c_int = 107;
pub const _SC_XOPEN_CRYPT: ::c_int = 108;
pub const _SC_XOPEN_ENH_I18N: ::c_int = 109;
pub const _SC_XOPEN_LEGACY: ::c_int = 110;
pub const _SC_XOPEN_REALTIME: ::c_int = 111;
pub const _SC_XOPEN_REALTIME_THREADS: ::c_int = 112;
pub const _SC_XOPEN_SHM: ::c_int = 113;
pub const _SC_XOPEN_UNIX: ::c_int = 115;
pub const _SC_XOPEN_VERSION: ::c_int = 116;
pub const _SC_XOPEN_XCU_VERSION: ::c_int = 121;
pub const _SC_PHYS_PAGES: ::c_int = 200;

pub const PTHREAD_PROCESS_PRIVATE: ::c_int = 2;
pub const PTHREAD_PROCESS_SHARED: ::c_int = 1;
pub const PTHREAD_CREATE_JOINABLE: ::c_int = 1;
pub const PTHREAD_CREATE_DETACHED: ::c_int = 2;
#[cfg(target_arch = "aarch64")]
pub const PTHREAD_STACK_MIN: ::size_t = 16384;
#[cfg(not(target_arch = "aarch64"))]
pub const PTHREAD_STACK_MIN: ::size_t = 8192;

pub const RLIMIT_CPU: ::c_int = 0;
pub const RLIMIT_FSIZE: ::c_int = 1;
pub const RLIMIT_DATA: ::c_int = 2;
pub const RLIMIT_STACK: ::c_int = 3;
pub const RLIMIT_CORE: ::c_int = 4;
pub const RLIMIT_AS: ::c_int = 5;
pub const RLIMIT_RSS: ::c_int = RLIMIT_AS;
pub const RLIMIT_MEMLOCK: ::c_int = 6;
pub const RLIMIT_NPROC: ::c_int = 7;
pub const RLIMIT_NOFILE: ::c_int = 8;
#[deprecated(since = "0.2.64", note = "Not stable across OS versions")]
pub const RLIM_NLIMITS: ::c_int = 9;
pub const _RLIMIT_POSIX_FLAG: ::c_int = 0x1000;

pub const RLIM_INFINITY: rlim_t = 0x7fff_ffff_ffff_ffff;

pub const RUSAGE_SELF: ::c_int = 0;
pub const RUSAGE_CHILDREN: ::c_int = -1;

pub const MADV_NORMAL: ::c_int = 0;
pub const MADV_RANDOM: ::c_int = 1;
pub const MADV_SEQUENTIAL: ::c_int = 2;
pub const MADV_WILLNEED: ::c_int = 3;
pub const MADV_DONTNEED: ::c_int = 4;
pub const MADV_FREE: ::c_int = 5;
pub const MADV_ZERO_WIRED_PAGES: ::c_int = 6;
pub const MADV_FREE_REUSABLE: ::c_int = 7;
pub const MADV_FREE_REUSE: ::c_int = 8;
pub const MADV_CAN_REUSE: ::c_int = 9;

pub const MINCORE_INCORE: ::c_int = 0x1;
pub const MINCORE_REFERENCED: ::c_int = 0x2;
pub const MINCORE_MODIFIED: ::c_int = 0x4;
pub const MINCORE_REFERENCED_OTHER: ::c_int = 0x8;
pub const MINCORE_MODIFIED_OTHER: ::c_int = 0x10;

//
// sys/netinet/in.h
// Protocols (RFC 1700)
// NOTE: These are in addition to the constants defined in src/unix/mod.rs

// IPPROTO_IP defined in src/unix/mod.rs
/// IP6 hop-by-hop options
pub const IPPROTO_HOPOPTS: ::c_int = 0;
// IPPROTO_ICMP defined in src/unix/mod.rs
/// group mgmt protocol
pub const IPPROTO_IGMP: ::c_int = 2;
/// gateway<sup>2</sup> (deprecated)
pub const IPPROTO_GGP: ::c_int = 3;
/// for compatibility
pub const IPPROTO_IPIP: ::c_int = 4;
// IPPROTO_TCP defined in src/unix/mod.rs
/// Stream protocol II.
pub const IPPROTO_ST: ::c_int = 7;
/// exterior gateway protocol
pub const IPPROTO_EGP: ::c_int = 8;
/// private interior gateway
pub const IPPROTO_PIGP: ::c_int = 9;
/// BBN RCC Monitoring
pub const IPPROTO_RCCMON: ::c_int = 10;
/// network voice protocol
pub const IPPROTO_NVPII: ::c_int = 11;
/// pup
pub const IPPROTO_PUP: ::c_int = 12;
/// Argus
pub const IPPROTO_ARGUS: ::c_int = 13;
/// EMCON
pub const IPPROTO_EMCON: ::c_int = 14;
/// Cross Net Debugger
pub const IPPROTO_XNET: ::c_int = 15;
/// Chaos
pub const IPPROTO_CHAOS: ::c_int = 16;
// IPPROTO_UDP defined in src/unix/mod.rs
/// Multiplexing
pub const IPPROTO_MUX: ::c_int = 18;
/// DCN Measurement Subsystems
pub const IPPROTO_MEAS: ::c_int = 19;
/// Host Monitoring
pub const IPPROTO_HMP: ::c_int = 20;
/// Packet Radio Measurement
pub const IPPROTO_PRM: ::c_int = 21;
/// xns idp
pub const IPPROTO_IDP: ::c_int = 22;
/// Trunk-1
pub const IPPROTO_TRUNK1: ::c_int = 23;
/// Trunk-2
pub const IPPROTO_TRUNK2: ::c_int = 24;
/// Leaf-1
pub const IPPROTO_LEAF1: ::c_int = 25;
/// Leaf-2
pub const IPPROTO_LEAF2: ::c_int = 26;
/// Reliable Data
pub const IPPROTO_RDP: ::c_int = 27;
/// Reliable Transaction
pub const IPPROTO_IRTP: ::c_int = 28;
/// tp-4 w/ class negotiation
pub const IPPROTO_TP: ::c_int = 29;
/// Bulk Data Transfer
pub const IPPROTO_BLT: ::c_int = 30;
/// Network Services
pub const IPPROTO_NSP: ::c_int = 31;
/// Merit Internodal
pub const IPPROTO_INP: ::c_int = 32;
/// Sequential Exchange
pub const IPPROTO_SEP: ::c_int = 33;
/// Third Party Connect
pub const IPPROTO_3PC: ::c_int = 34;
/// InterDomain Policy Routing
pub const IPPROTO_IDPR: ::c_int = 35;
/// XTP
pub const IPPROTO_XTP: ::c_int = 36;
/// Datagram Delivery
pub const IPPROTO_DDP: ::c_int = 37;
/// Control Message Transport
pub const IPPROTO_CMTP: ::c_int = 38;
/// TP++ Transport
pub const IPPROTO_TPXX: ::c_int = 39;
/// IL transport protocol
pub const IPPROTO_IL: ::c_int = 40;
// IPPROTO_IPV6 defined in src/unix/mod.rs
/// Source Demand Routing
pub const IPPROTO_SDRP: ::c_int = 42;
/// IP6 routing header
pub const IPPROTO_ROUTING: ::c_int = 43;
/// IP6 fragmentation header
pub const IPPROTO_FRAGMENT: ::c_int = 44;
/// InterDomain Routing
pub const IPPROTO_IDRP: ::c_int = 45;
/// resource reservation
pub const IPPROTO_RSVP: ::c_int = 46;
/// General Routing Encap.
pub const IPPROTO_GRE: ::c_int = 47;
/// Mobile Host Routing
pub const IPPROTO_MHRP: ::c_int = 48;
/// BHA
pub const IPPROTO_BHA: ::c_int = 49;
/// IP6 Encap Sec. Payload
pub const IPPROTO_ESP: ::c_int = 50;
/// IP6 Auth Header
pub const IPPROTO_AH: ::c_int = 51;
/// Integ. Net Layer Security
pub const IPPROTO_INLSP: ::c_int = 52;
/// IP with encryption
pub const IPPROTO_SWIPE: ::c_int = 53;
/// Next Hop Resolution
pub const IPPROTO_NHRP: ::c_int = 54;
/* 55-57: Unassigned */
// IPPROTO_ICMPV6 defined in src/unix/mod.rs
/// IP6 no next header
pub const IPPROTO_NONE: ::c_int = 59;
/// IP6 destination option
pub const IPPROTO_DSTOPTS: ::c_int = 60;
/// any host internal protocol
pub const IPPROTO_AHIP: ::c_int = 61;
/// CFTP
pub const IPPROTO_CFTP: ::c_int = 62;
/// "hello" routing protocol
pub const IPPROTO_HELLO: ::c_int = 63;
/// SATNET/Backroom EXPAK
pub const IPPROTO_SATEXPAK: ::c_int = 64;
/// Kryptolan
pub const IPPROTO_KRYPTOLAN: ::c_int = 65;
/// Remote Virtual Disk
pub const IPPROTO_RVD: ::c_int = 66;
/// Pluribus Packet Core
pub const IPPROTO_IPPC: ::c_int = 67;
/// Any distributed FS
pub const IPPROTO_ADFS: ::c_int = 68;
/// Satnet Monitoring
pub const IPPROTO_SATMON: ::c_int = 69;
/// VISA Protocol
pub const IPPROTO_VISA: ::c_int = 70;
/// Packet Core Utility
pub const IPPROTO_IPCV: ::c_int = 71;
/// Comp. Prot. Net. Executive
pub const IPPROTO_CPNX: ::c_int = 72;
/// Comp. Prot. HeartBeat
pub const IPPROTO_CPHB: ::c_int = 73;
/// Wang Span Network
pub const IPPROTO_WSN: ::c_int = 74;
/// Packet Video Protocol
pub const IPPROTO_PVP: ::c_int = 75;
/// BackRoom SATNET Monitoring
pub const IPPROTO_BRSATMON: ::c_int = 76;
/// Sun net disk proto (temp.)
pub const IPPROTO_ND: ::c_int = 77;
/// WIDEBAND Monitoring
pub const IPPROTO_WBMON: ::c_int = 78;
/// WIDEBAND EXPAK
pub const IPPROTO_WBEXPAK: ::c_int = 79;
/// ISO cnlp
pub const IPPROTO_EON: ::c_int = 80;
/// VMTP
pub const IPPROTO_VMTP: ::c_int = 81;
/// Secure VMTP
pub const IPPROTO_SVMTP: ::c_int = 82;
/// Banyon VINES
pub const IPPROTO_VINES: ::c_int = 83;
/// TTP
pub const IPPROTO_TTP: ::c_int = 84;
/// NSFNET-IGP
pub const IPPROTO_IGP: ::c_int = 85;
/// dissimilar gateway prot.
pub const IPPROTO_DGP: ::c_int = 86;
/// TCF
pub const IPPROTO_TCF: ::c_int = 87;
/// Cisco/GXS IGRP
pub const IPPROTO_IGRP: ::c_int = 88;
/// OSPFIGP
pub const IPPROTO_OSPFIGP: ::c_int = 89;
/// Strite RPC protocol
pub const IPPROTO_SRPC: ::c_int = 90;
/// Locus Address Resoloution
pub const IPPROTO_LARP: ::c_int = 91;
/// Multicast Transport
pub const IPPROTO_MTP: ::c_int = 92;
/// AX.25 Frames
pub const IPPROTO_AX25: ::c_int = 93;
/// IP encapsulated in IP
pub const IPPROTO_IPEIP: ::c_int = 94;
/// Mobile Int.ing control
pub const IPPROTO_MICP: ::c_int = 95;
/// Semaphore Comm. security
pub const IPPROTO_SCCSP: ::c_int = 96;
/// Ethernet IP encapsulation
pub const IPPROTO_ETHERIP: ::c_int = 97;
/// encapsulation header
pub const IPPROTO_ENCAP: ::c_int = 98;
/// any private encr. scheme
pub const IPPROTO_APES: ::c_int = 99;
/// GMTP
pub const IPPROTO_GMTP: ::c_int = 100;

/* 101-254: Partly Unassigned */
/// Protocol Independent Mcast
pub const IPPROTO_PIM: ::c_int = 103;
/// payload compression (IPComp)
pub const IPPROTO_IPCOMP: ::c_int = 108;
/// PGM
pub const IPPROTO_PGM: ::c_int = 113;
/// SCTP
pub const IPPROTO_SCTP: ::c_int = 132;

/* 255: Reserved */
/* BSD Private, local use, namespace incursion */
/// divert pseudo-protocol
pub const IPPROTO_DIVERT: ::c_int = 254;
/// raw IP packet
pub const IPPROTO_RAW: ::c_int = 255;
pub const IPPROTO_MAX: ::c_int = 256;
/// last return value of *_input(), meaning "all job for this pkt is done".
pub const IPPROTO_DONE: ::c_int = 257;

pub const AF_UNSPEC: ::c_int = 0;
pub const AF_LOCAL: ::c_int = 1;
pub const AF_UNIX: ::c_int = AF_LOCAL;
pub const AF_INET: ::c_int = 2;
pub const AF_IMPLINK: ::c_int = 3;
pub const AF_PUP: ::c_int = 4;
pub const AF_CHAOS: ::c_int = 5;
pub const AF_NS: ::c_int = 6;
pub const AF_ISO: ::c_int = 7;
pub const AF_OSI: ::c_int = AF_ISO;
pub const AF_ECMA: ::c_int = 8;
pub const AF_DATAKIT: ::c_int = 9;
pub const AF_CCITT: ::c_int = 10;
pub const AF_SNA: ::c_int = 11;
pub const AF_DECnet: ::c_int = 12;
pub const AF_DLI: ::c_int = 13;
pub const AF_LAT: ::c_int = 14;
pub const AF_HYLINK: ::c_int = 15;
pub const AF_APPLETALK: ::c_int = 16;
pub const AF_ROUTE: ::c_int = 17;
pub const AF_LINK: ::c_int = 18;
pub const pseudo_AF_XTP: ::c_int = 19;
pub const AF_COIP: ::c_int = 20;
pub const AF_CNT: ::c_int = 21;
pub const pseudo_AF_RTIP: ::c_int = 22;
pub const AF_IPX: ::c_int = 23;
pub const AF_SIP: ::c_int = 24;
pub const pseudo_AF_PIP: ::c_int = 25;
pub const AF_ISDN: ::c_int = 28;
pub const AF_E164: ::c_int = AF_ISDN;
pub const pseudo_AF_KEY: ::c_int = 29;
pub const AF_INET6: ::c_int = 30;
pub const AF_NATM: ::c_int = 31;
pub const AF_SYSTEM: ::c_int = 32;
pub const AF_NETBIOS: ::c_int = 33;
pub const AF_PPP: ::c_int = 34;
pub const pseudo_AF_HDRCMPLT: ::c_int = 35;
pub const AF_SYS_CONTROL: ::c_int = 2;

pub const SYSPROTO_EVENT: ::c_int = 1;
pub const SYSPROTO_CONTROL: ::c_int = 2;

pub const PF_UNSPEC: ::c_int = AF_UNSPEC;
pub const PF_LOCAL: ::c_int = AF_LOCAL;
pub const PF_UNIX: ::c_int = PF_LOCAL;
pub const PF_INET: ::c_int = AF_INET;
pub const PF_IMPLINK: ::c_int = AF_IMPLINK;
pub const PF_PUP: ::c_int = AF_PUP;
pub const PF_CHAOS: ::c_int = AF_CHAOS;
pub const PF_NS: ::c_int = AF_NS;
pub const PF_ISO: ::c_int = AF_ISO;
pub const PF_OSI: ::c_int = AF_ISO;
pub const PF_ECMA: ::c_int = AF_ECMA;
pub const PF_DATAKIT: ::c_int = AF_DATAKIT;
pub const PF_CCITT: ::c_int = AF_CCITT;
pub const PF_SNA: ::c_int = AF_SNA;
pub const PF_DECnet: ::c_int = AF_DECnet;
pub const PF_DLI: ::c_int = AF_DLI;
pub const PF_LAT: ::c_int = AF_LAT;
pub const PF_HYLINK: ::c_int = AF_HYLINK;
pub const PF_APPLETALK: ::c_int = AF_APPLETALK;
pub const PF_ROUTE: ::c_int = AF_ROUTE;
pub const PF_LINK: ::c_int = AF_LINK;
pub const PF_XTP: ::c_int = pseudo_AF_XTP;
pub const PF_COIP: ::c_int = AF_COIP;
pub const PF_CNT: ::c_int = AF_CNT;
pub const PF_SIP: ::c_int = AF_SIP;
pub const PF_IPX: ::c_int = AF_IPX;
pub const PF_RTIP: ::c_int = pseudo_AF_RTIP;
pub const PF_PIP: ::c_int = pseudo_AF_PIP;
pub const PF_ISDN: ::c_int = AF_ISDN;
pub const PF_KEY: ::c_int = pseudo_AF_KEY;
pub const PF_INET6: ::c_int = AF_INET6;
pub const PF_NATM: ::c_int = AF_NATM;
pub const PF_SYSTEM: ::c_int = AF_SYSTEM;
pub const PF_NETBIOS: ::c_int = AF_NETBIOS;
pub const PF_PPP: ::c_int = AF_PPP;

pub const NET_RT_DUMP: ::c_int = 1;
pub const NET_RT_FLAGS: ::c_int = 2;
pub const NET_RT_IFLIST: ::c_int = 3;

pub const SOMAXCONN: ::c_int = 128;

pub const SOCK_MAXADDRLEN: ::c_int = 255;

pub const SOCK_STREAM: ::c_int = 1;
pub const SOCK_DGRAM: ::c_int = 2;
pub const SOCK_RAW: ::c_int = 3;
pub const SOCK_RDM: ::c_int = 4;
pub const SOCK_SEQPACKET: ::c_int = 5;
pub const IP_TTL: ::c_int = 4;
pub const IP_HDRINCL: ::c_int = 2;
pub const IP_RECVDSTADDR: ::c_int = 7;
pub const IP_ADD_MEMBERSHIP: ::c_int = 12;
pub const IP_DROP_MEMBERSHIP: ::c_int = 13;
pub const IP_RECVIF: ::c_int = 20;
pub const IP_BOUND_IF: ::c_int = 25;
pub const IP_PKTINFO: ::c_int = 26;
pub const IP_RECVTOS: ::c_int = 27;
pub const IPV6_JOIN_GROUP: ::c_int = 12;
pub const IPV6_LEAVE_GROUP: ::c_int = 13;
pub const IPV6_CHECKSUM: ::c_int = 26;
pub const IPV6_RECVTCLASS: ::c_int = 35;
pub const IPV6_TCLASS: ::c_int = 36;
pub const IPV6_PKTINFO: ::c_int = 46;
pub const IPV6_HOPLIMIT: ::c_int = 47;
pub const IPV6_RECVPKTINFO: ::c_int = 61;

pub const TCP_NOPUSH: ::c_int = 4;
pub const TCP_NOOPT: ::c_int = 8;
pub const TCP_KEEPALIVE: ::c_int = 0x10;
pub const TCP_KEEPINTVL: ::c_int = 0x101;
pub const TCP_KEEPCNT: ::c_int = 0x102;
/// Enable/Disable TCP Fastopen on this socket
pub const TCP_FASTOPEN: ::c_int = 0x105;

pub const SOL_LOCAL: ::c_int = 0;

pub const LOCAL_PEERCRED: ::c_int = 0x001;
pub const LOCAL_PEERPID: ::c_int = 0x002;
pub const LOCAL_PEEREPID: ::c_int = 0x003;
pub const LOCAL_PEERUUID: ::c_int = 0x004;
pub const LOCAL_PEEREUUID: ::c_int = 0x005;

pub const SOL_SOCKET: ::c_int = 0xffff;

pub const SO_DEBUG: ::c_int = 0x01;
pub const SO_ACCEPTCONN: ::c_int = 0x0002;
pub const SO_REUSEADDR: ::c_int = 0x0004;
pub const SO_KEEPALIVE: ::c_int = 0x0008;
pub const SO_DONTROUTE: ::c_int = 0x0010;
pub const SO_BROADCAST: ::c_int = 0x0020;
pub const SO_USELOOPBACK: ::c_int = 0x0040;
pub const SO_LINGER: ::c_int = 0x0080;
pub const SO_OOBINLINE: ::c_int = 0x0100;
pub const SO_REUSEPORT: ::c_int = 0x0200;
pub const SO_TIMESTAMP: ::c_int = 0x0400;
pub const SO_TIMESTAMP_MONOTONIC: ::c_int = 0x0800;
pub const SO_DONTTRUNC: ::c_int = 0x2000;
pub const SO_WANTMORE: ::c_int = 0x4000;
pub const SO_WANTOOBFLAG: ::c_int = 0x8000;
pub const SO_SNDBUF: ::c_int = 0x1001;
pub const SO_RCVBUF: ::c_int = 0x1002;
pub const SO_SNDLOWAT: ::c_int = 0x1003;
pub const SO_RCVLOWAT: ::c_int = 0x1004;
pub const SO_SNDTIMEO: ::c_int = 0x1005;
pub const SO_RCVTIMEO: ::c_int = 0x1006;
pub const SO_ERROR: ::c_int = 0x1007;
pub const SO_TYPE: ::c_int = 0x1008;
pub const SO_LABEL: ::c_int = 0x1010;
pub const SO_PEERLABEL: ::c_int = 0x1011;
pub const SO_NREAD: ::c_int = 0x1020;
pub const SO_NKE: ::c_int = 0x1021;
pub const SO_NOSIGPIPE: ::c_int = 0x1022;
pub const SO_NOADDRERR: ::c_int = 0x1023;
pub const SO_NWRITE: ::c_int = 0x1024;
pub const SO_REUSESHAREUID: ::c_int = 0x1025;
pub const SO_NOTIFYCONFLICT: ::c_int = 0x1026;
pub const SO_LINGER_SEC: ::c_int = 0x1080;
pub const SO_RANDOMPORT: ::c_int = 0x1082;
pub const SO_NP_EXTENSIONS: ::c_int = 0x1083;

pub const MSG_OOB: ::c_int = 0x1;
pub const MSG_PEEK: ::c_int = 0x2;
pub const MSG_DONTROUTE: ::c_int = 0x4;
pub const MSG_EOR: ::c_int = 0x8;
pub const MSG_TRUNC: ::c_int = 0x10;
pub const MSG_CTRUNC: ::c_int = 0x20;
pub const MSG_WAITALL: ::c_int = 0x40;
pub const MSG_DONTWAIT: ::c_int = 0x80;
pub const MSG_EOF: ::c_int = 0x100;
pub const MSG_FLUSH: ::c_int = 0x400;
pub const MSG_HOLD: ::c_int = 0x800;
pub const MSG_SEND: ::c_int = 0x1000;
pub const MSG_HAVEMORE: ::c_int = 0x2000;
pub const MSG_RCVMORE: ::c_int = 0x4000;
// pub const MSG_COMPAT: ::c_int = 0x8000;

pub const SCM_TIMESTAMP: ::c_int = 0x02;
pub const SCM_CREDS: ::c_int = 0x03;

// https://github.com/aosm/xnu/blob/master/bsd/net/if.h#L140-L156
pub const IFF_UP: ::c_int = 0x1; // interface is up
pub const IFF_BROADCAST: ::c_int = 0x2; // broadcast address valid
pub const IFF_DEBUG: ::c_int = 0x4; // turn on debugging
pub const IFF_LOOPBACK: ::c_int = 0x8; // is a loopback net
pub const IFF_POINTOPOINT: ::c_int = 0x10; // interface is point-to-point link
pub const IFF_NOTRAILERS: ::c_int = 0x20; // obsolete: avoid use of trailers
pub const IFF_RUNNING: ::c_int = 0x40; // resources allocated
pub const IFF_NOARP: ::c_int = 0x80; // no address resolution protocol
pub const IFF_PROMISC: ::c_int = 0x100; // receive all packets
pub const IFF_ALLMULTI: ::c_int = 0x200; // receive all multicast packets
pub const IFF_OACTIVE: ::c_int = 0x400; // transmission in progress
pub const IFF_SIMPLEX: ::c_int = 0x800; // can't hear own transmissions
pub const IFF_LINK0: ::c_int = 0x1000; // per link layer defined bit
pub const IFF_LINK1: ::c_int = 0x2000; // per link layer defined bit
pub const IFF_LINK2: ::c_int = 0x4000; // per link layer defined bit
pub const IFF_ALTPHYS: ::c_int = IFF_LINK2; // use alternate physical connection
pub const IFF_MULTICAST: ::c_int = 0x8000; // supports multicast

pub const SHUT_RD: ::c_int = 0;
pub const SHUT_WR: ::c_int = 1;
pub const SHUT_RDWR: ::c_int = 2;

pub const SAE_ASSOCID_ANY: ::sae_associd_t = 0;
/// ((sae_associd_t)(-1ULL))
pub const SAE_ASSOCID_ALL: ::sae_associd_t = 0xffffffff;

pub const SAE_CONNID_ANY: ::sae_connid_t = 0;
/// ((sae_connid_t)(-1ULL))
pub const SAE_CONNID_ALL: ::sae_connid_t = 0xffffffff;

// connectx() flag parameters

/// resume connect() on read/write
pub const CONNECT_RESUME_ON_READ_WRITE: ::c_uint = 0x1;
/// data is idempotent
pub const CONNECT_DATA_IDEMPOTENT: ::c_uint = 0x2;
/// data includes security that replaces the TFO-cookie
pub const CONNECT_DATA_AUTHENTICATED: ::c_uint = 0x4;

pub const LOCK_SH: ::c_int = 1;
pub const LOCK_EX: ::c_int = 2;
pub const LOCK_NB: ::c_int = 4;
pub const LOCK_UN: ::c_int = 8;

pub const MAP_COPY: ::c_int = 0x0002;
pub const MAP_RENAME: ::c_int = 0x0020;
pub const MAP_NORESERVE: ::c_int = 0x0040;
pub const MAP_NOEXTEND: ::c_int = 0x0100;
pub const MAP_HASSEMAPHORE: ::c_int = 0x0200;
pub const MAP_NOCACHE: ::c_int = 0x0400;
pub const MAP_JIT: ::c_int = 0x0800;

pub const _SC_ARG_MAX: ::c_int = 1;
pub const _SC_CHILD_MAX: ::c_int = 2;
pub const _SC_CLK_TCK: ::c_int = 3;
pub const _SC_NGROUPS_MAX: ::c_int = 4;
pub const _SC_OPEN_MAX: ::c_int = 5;
pub const _SC_JOB_CONTROL: ::c_int = 6;
pub const _SC_SAVED_IDS: ::c_int = 7;
pub const _SC_VERSION: ::c_int = 8;
pub const _SC_BC_BASE_MAX: ::c_int = 9;
pub const _SC_BC_DIM_MAX: ::c_int = 10;
pub const _SC_BC_SCALE_MAX: ::c_int = 11;
pub const _SC_BC_STRING_MAX: ::c_int = 12;
pub const _SC_COLL_WEIGHTS_MAX: ::c_int = 13;
pub const _SC_EXPR_NEST_MAX: ::c_int = 14;
pub const _SC_LINE_MAX: ::c_int = 15;
pub const _SC_RE_DUP_MAX: ::c_int = 16;
pub const _SC_2_VERSION: ::c_int = 17;
pub const _SC_2_C_BIND: ::c_int = 18;
pub const _SC_2_C_DEV: ::c_int = 19;
pub const _SC_2_CHAR_TERM: ::c_int = 20;
pub const _SC_2_FORT_DEV: ::c_int = 21;
pub const _SC_2_FORT_RUN: ::c_int = 22;
pub const _SC_2_LOCALEDEF: ::c_int = 23;
pub const _SC_2_SW_DEV: ::c_int = 24;
pub const _SC_2_UPE: ::c_int = 25;
pub const _SC_STREAM_MAX: ::c_int = 26;
pub const _SC_TZNAME_MAX: ::c_int = 27;
pub const _SC_ASYNCHRONOUS_IO: ::c_int = 28;
pub const _SC_PAGESIZE: ::c_int = 29;
pub const _SC_MEMLOCK: ::c_int = 30;
pub const _SC_MEMLOCK_RANGE: ::c_int = 31;
pub const _SC_MEMORY_PROTECTION: ::c_int = 32;
pub const _SC_MESSAGE_PASSING: ::c_int = 33;
pub const _SC_PRIORITIZED_IO: ::c_int = 34;
pub const _SC_PRIORITY_SCHEDULING: ::c_int = 35;
pub const _SC_REALTIME_SIGNALS: ::c_int = 36;
pub const _SC_SEMAPHORES: ::c_int = 37;
pub const _SC_FSYNC: ::c_int = 38;
pub const _SC_SHARED_MEMORY_OBJECTS: ::c_int = 39;
pub const _SC_SYNCHRONIZED_IO: ::c_int = 40;
pub const _SC_TIMERS: ::c_int = 41;
pub const _SC_AIO_LISTIO_MAX: ::c_int = 42;
pub const _SC_AIO_MAX: ::c_int = 43;
pub const _SC_AIO_PRIO_DELTA_MAX: ::c_int = 44;
pub const _SC_DELAYTIMER_MAX: ::c_int = 45;
pub const _SC_MQ_OPEN_MAX: ::c_int = 46;
pub const _SC_MAPPED_FILES: ::c_int = 47;
pub const _SC_RTSIG_MAX: ::c_int = 48;
pub const _SC_SEM_NSEMS_MAX: ::c_int = 49;
pub const _SC_SEM_VALUE_MAX: ::c_int = 50;
pub const _SC_SIGQUEUE_MAX: ::c_int = 51;
pub const _SC_TIMER_MAX: ::c_int = 52;
pub const _SC_NPROCESSORS_CONF: ::c_int = 57;
pub const _SC_NPROCESSORS_ONLN: ::c_int = 58;
pub const _SC_2_PBS: ::c_int = 59;
pub const _SC_2_PBS_ACCOUNTING: ::c_int = 60;
pub const _SC_2_PBS_CHECKPOINT: ::c_int = 61;
pub const _SC_2_PBS_LOCATE: ::c_int = 62;
pub const _SC_2_PBS_MESSAGE: ::c_int = 63;
pub const _SC_2_PBS_TRACK: ::c_int = 64;
pub const _SC_ADVISORY_INFO: ::c_int = 65;
pub const _SC_BARRIERS: ::c_int = 66;
pub const _SC_CLOCK_SELECTION: ::c_int = 67;
pub const _SC_CPUTIME: ::c_int = 68;
pub const _SC_FILE_LOCKING: ::c_int = 69;
pub const _SC_HOST_NAME_MAX: ::c_int = 72;
pub const _SC_MONOTONIC_CLOCK: ::c_int = 74;
pub const _SC_READER_WRITER_LOCKS: ::c_int = 76;
pub const _SC_REGEXP: ::c_int = 77;
pub const _SC_SHELL: ::c_int = 78;
pub const _SC_SPAWN: ::c_int = 79;
pub const _SC_SPIN_LOCKS: ::c_int = 80;
pub const _SC_SPORADIC_SERVER: ::c_int = 81;
pub const _SC_THREAD_CPUTIME: ::c_int = 84;
pub const _SC_THREAD_SPORADIC_SERVER: ::c_int = 92;
pub const _SC_TIMEOUTS: ::c_int = 95;
pub const _SC_TRACE: ::c_int = 97;
pub const _SC_TRACE_EVENT_FILTER: ::c_int = 98;
pub const _SC_TRACE_INHERIT: ::c_int = 99;
pub const _SC_TRACE_LOG: ::c_int = 100;
pub const _SC_TYPED_MEMORY_OBJECTS: ::c_int = 102;
pub const _SC_V6_ILP32_OFF32: ::c_int = 103;
pub const _SC_V6_ILP32_OFFBIG: ::c_int = 104;
pub const _SC_V6_LP64_OFF64: ::c_int = 105;
pub const _SC_V6_LPBIG_OFFBIG: ::c_int = 106;
pub const _SC_IPV6: ::c_int = 118;
pub const _SC_RAW_SOCKETS: ::c_int = 119;
pub const _SC_SYMLOOP_MAX: ::c_int = 120;
pub const _SC_PAGE_SIZE: ::c_int = _SC_PAGESIZE;
pub const _SC_XOPEN_STREAMS: ::c_int = 114;
pub const _SC_XBS5_ILP32_OFF32: ::c_int = 122;
pub const _SC_XBS5_ILP32_OFFBIG: ::c_int = 123;
pub const _SC_XBS5_LP64_OFF64: ::c_int = 124;
pub const _SC_XBS5_LPBIG_OFFBIG: ::c_int = 125;
pub const _SC_SS_REPL_MAX: ::c_int = 126;
pub const _SC_TRACE_EVENT_NAME_MAX: ::c_int = 127;
pub const _SC_TRACE_NAME_MAX: ::c_int = 128;
pub const _SC_TRACE_SYS_MAX: ::c_int = 129;
pub const _SC_TRACE_USER_EVENT_MAX: ::c_int = 130;
pub const _SC_PASS_MAX: ::c_int = 131;

pub const PTHREAD_MUTEX_NORMAL: ::c_int = 0;
pub const PTHREAD_MUTEX_ERRORCHECK: ::c_int = 1;
pub const PTHREAD_MUTEX_RECURSIVE: ::c_int = 2;
pub const PTHREAD_MUTEX_DEFAULT: ::c_int = PTHREAD_MUTEX_NORMAL;
pub const _PTHREAD_MUTEX_SIG_init: ::c_long = 0x32AAABA7;
pub const _PTHREAD_COND_SIG_init: ::c_long = 0x3CB0B1BB;
pub const _PTHREAD_RWLOCK_SIG_init: ::c_long = 0x2DA8B3B4;
pub const PTHREAD_MUTEX_INITIALIZER: pthread_mutex_t = pthread_mutex_t {
    __sig: _PTHREAD_MUTEX_SIG_init,
    __opaque: [0; __PTHREAD_MUTEX_SIZE__],
};
pub const PTHREAD_COND_INITIALIZER: pthread_cond_t = pthread_cond_t {
    __sig: _PTHREAD_COND_SIG_init,
    __opaque: [0; __PTHREAD_COND_SIZE__],
};
pub const PTHREAD_RWLOCK_INITIALIZER: pthread_rwlock_t = pthread_rwlock_t {
    __sig: _PTHREAD_RWLOCK_SIG_init,
    __opaque: [0; __PTHREAD_RWLOCK_SIZE__],
};

pub const MINSIGSTKSZ: ::size_t = 32768;
pub const SIGSTKSZ: ::size_t = 131072;

pub const FD_SETSIZE: usize = 1024;

pub const ST_NOSUID: ::c_ulong = 2;

pub const SCHED_OTHER: ::c_int = 1;
pub const SCHED_FIFO: ::c_int = 4;
pub const SCHED_RR: ::c_int = 2;

pub const EVFILT_READ: i16 = -1;
pub const EVFILT_WRITE: i16 = -2;
pub const EVFILT_AIO: i16 = -3;
pub const EVFILT_VNODE: i16 = -4;
pub const EVFILT_PROC: i16 = -5;
pub const EVFILT_SIGNAL: i16 = -6;
pub const EVFILT_TIMER: i16 = -7;
pub const EVFILT_MACHPORT: i16 = -8;
pub const EVFILT_FS: i16 = -9;
pub const EVFILT_USER: i16 = -10;
pub const EVFILT_VM: i16 = -12;

pub const EV_ADD: u16 = 0x1;
pub const EV_DELETE: u16 = 0x2;
pub const EV_ENABLE: u16 = 0x4;
pub const EV_DISABLE: u16 = 0x8;
pub const EV_ONESHOT: u16 = 0x10;
pub const EV_CLEAR: u16 = 0x20;
pub const EV_RECEIPT: u16 = 0x40;
pub const EV_DISPATCH: u16 = 0x80;
pub const EV_FLAG0: u16 = 0x1000;
pub const EV_POLL: u16 = 0x1000;
pub const EV_FLAG1: u16 = 0x2000;
pub const EV_OOBAND: u16 = 0x2000;
pub const EV_ERROR: u16 = 0x4000;
pub const EV_EOF: u16 = 0x8000;
pub const EV_SYSFLAGS: u16 = 0xf000;

pub const NOTE_TRIGGER: u32 = 0x01000000;
pub const NOTE_FFNOP: u32 = 0x00000000;
pub const NOTE_FFAND: u32 = 0x40000000;
pub const NOTE_FFOR: u32 = 0x80000000;
pub const NOTE_FFCOPY: u32 = 0xc0000000;
pub const NOTE_FFCTRLMASK: u32 = 0xc0000000;
pub const NOTE_FFLAGSMASK: u32 = 0x00ffffff;
pub const NOTE_LOWAT: u32 = 0x00000001;
pub const NOTE_DELETE: u32 = 0x00000001;
pub const NOTE_WRITE: u32 = 0x00000002;
pub const NOTE_EXTEND: u32 = 0x00000004;
pub const NOTE_ATTRIB: u32 = 0x00000008;
pub const NOTE_LINK: u32 = 0x00000010;
pub const NOTE_RENAME: u32 = 0x00000020;
pub const NOTE_REVOKE: u32 = 0x00000040;
pub const NOTE_NONE: u32 = 0x00000080;
pub const NOTE_EXIT: u32 = 0x80000000;
pub const NOTE_FORK: u32 = 0x40000000;
pub const NOTE_EXEC: u32 = 0x20000000;
#[doc(hidden)]
#[deprecated(since = "0.2.49", note = "Deprecated since MacOSX 10.9")]
pub const NOTE_REAP: u32 = 0x10000000;
pub const NOTE_SIGNAL: u32 = 0x08000000;
pub const NOTE_EXITSTATUS: u32 = 0x04000000;
pub const NOTE_EXIT_DETAIL: u32 = 0x02000000;
pub const NOTE_PDATAMASK: u32 = 0x000fffff;
pub const NOTE_PCTRLMASK: u32 = 0xfff00000;
#[doc(hidden)]
#[deprecated(since = "0.2.49", note = "Deprecated since MacOSX 10.9")]
pub const NOTE_EXIT_REPARENTED: u32 = 0x00080000;
pub const NOTE_EXIT_DETAIL_MASK: u32 = 0x00070000;
pub const NOTE_EXIT_DECRYPTFAIL: u32 = 0x00010000;
pub const NOTE_EXIT_MEMORY: u32 = 0x00020000;
pub const NOTE_EXIT_CSERROR: u32 = 0x00040000;
pub const NOTE_VM_PRESSURE: u32 = 0x80000000;
pub const NOTE_VM_PRESSURE_TERMINATE: u32 = 0x40000000;
pub const NOTE_VM_PRESSURE_SUDDEN_TERMINATE: u32 = 0x20000000;
pub const NOTE_VM_ERROR: u32 = 0x10000000;
pub const NOTE_SECONDS: u32 = 0x00000001;
pub const NOTE_USECONDS: u32 = 0x00000002;
pub const NOTE_NSECONDS: u32 = 0x00000004;
pub const NOTE_ABSOLUTE: u32 = 0x00000008;
pub const NOTE_LEEWAY: u32 = 0x00000010;
pub const NOTE_CRITICAL: u32 = 0x00000020;
pub const NOTE_BACKGROUND: u32 = 0x00000040;
pub const NOTE_TRACK: u32 = 0x00000001;
pub const NOTE_TRACKERR: u32 = 0x00000002;
pub const NOTE_CHILD: u32 = 0x00000004;

pub const OCRNL: ::tcflag_t = 0x00000010;
pub const ONOCR: ::tcflag_t = 0x00000020;
pub const ONLRET: ::tcflag_t = 0x00000040;
pub const OFILL: ::tcflag_t = 0x00000080;
pub const NLDLY: ::tcflag_t = 0x00000300;
pub const TABDLY: ::tcflag_t = 0x00000c04;
pub const CRDLY: ::tcflag_t = 0x00003000;
pub const FFDLY: ::tcflag_t = 0x00004000;
pub const BSDLY: ::tcflag_t = 0x00008000;
pub const VTDLY: ::tcflag_t = 0x00010000;
pub const OFDEL: ::tcflag_t = 0x00020000;

pub const NL0: ::tcflag_t = 0x00000000;
pub const NL1: ::tcflag_t = 0x00000100;
pub const TAB0: ::tcflag_t = 0x00000000;
pub const TAB1: ::tcflag_t = 0x00000400;
pub const TAB2: ::tcflag_t = 0x00000800;
pub const CR0: ::tcflag_t = 0x00000000;
pub const CR1: ::tcflag_t = 0x00001000;
pub const CR2: ::tcflag_t = 0x00002000;
pub const CR3: ::tcflag_t = 0x00003000;
pub const FF0: ::tcflag_t = 0x00000000;
pub const FF1: ::tcflag_t = 0x00004000;
pub const BS0: ::tcflag_t = 0x00000000;
pub const BS1: ::tcflag_t = 0x00008000;
pub const TAB3: ::tcflag_t = 0x00000004;
pub const VT0: ::tcflag_t = 0x00000000;
pub const VT1: ::tcflag_t = 0x00010000;
pub const IUTF8: ::tcflag_t = 0x00004000;
pub const CRTSCTS: ::tcflag_t = 0x00030000;

pub const NI_MAXHOST: ::socklen_t = 1025;
pub const NI_MAXSERV: ::socklen_t = 32;
pub const NI_NOFQDN: ::c_int = 0x00000001;
pub const NI_NUMERICHOST: ::c_int = 0x00000002;
pub const NI_NAMEREQD: ::c_int = 0x00000004;
pub const NI_NUMERICSERV: ::c_int = 0x00000008;
pub const NI_NUMERICSCOPE: ::c_int = 0x00000100;
pub const NI_DGRAM: ::c_int = 0x00000010;

pub const Q_GETQUOTA: ::c_int = 0x300;
pub const Q_SETQUOTA: ::c_int = 0x400;

pub const RENAME_SWAP: ::c_uint = 0x00000002;
pub const RENAME_EXCL: ::c_uint = 0x00000004;

pub const RTLD_LOCAL: ::c_int = 0x4;
pub const RTLD_FIRST: ::c_int = 0x100;
pub const RTLD_NODELETE: ::c_int = 0x80;
pub const RTLD_NOLOAD: ::c_int = 0x10;
pub const RTLD_GLOBAL: ::c_int = 0x8;

pub const _WSTOPPED: ::c_int = 0o177;

pub const LOG_NETINFO: ::c_int = 12 << 3;
pub const LOG_REMOTEAUTH: ::c_int = 13 << 3;
pub const LOG_INSTALL: ::c_int = 14 << 3;
pub const LOG_RAS: ::c_int = 15 << 3;
pub const LOG_LAUNCHD: ::c_int = 24 << 3;
pub const LOG_NFACILITIES: ::c_int = 25;

pub const CTLTYPE: ::c_int = 0xf;
pub const CTLTYPE_NODE: ::c_int = 1;
pub const CTLTYPE_INT: ::c_int = 2;
pub const CTLTYPE_STRING: ::c_int = 3;
pub const CTLTYPE_QUAD: ::c_int = 4;
pub const CTLTYPE_OPAQUE: ::c_int = 5;
pub const CTLTYPE_STRUCT: ::c_int = CTLTYPE_OPAQUE;
pub const CTLFLAG_RD: ::c_int = 0x80000000;
pub const CTLFLAG_WR: ::c_int = 0x40000000;
pub const CTLFLAG_RW: ::c_int = CTLFLAG_RD | CTLFLAG_WR;
pub const CTLFLAG_NOLOCK: ::c_int = 0x20000000;
pub const CTLFLAG_ANYBODY: ::c_int = 0x10000000;
pub const CTLFLAG_SECURE: ::c_int = 0x08000000;
pub const CTLFLAG_MASKED: ::c_int = 0x04000000;
pub const CTLFLAG_NOAUTO: ::c_int = 0x02000000;
pub const CTLFLAG_KERN: ::c_int = 0x01000000;
pub const CTLFLAG_LOCKED: ::c_int = 0x00800000;
pub const CTLFLAG_OID2: ::c_int = 0x00400000;
pub const CTL_UNSPEC: ::c_int = 0;
pub const CTL_KERN: ::c_int = 1;
pub const CTL_VM: ::c_int = 2;
pub const CTL_VFS: ::c_int = 3;
pub const CTL_NET: ::c_int = 4;
pub const CTL_DEBUG: ::c_int = 5;
pub const CTL_HW: ::c_int = 6;
pub const CTL_MACHDEP: ::c_int = 7;
pub const CTL_USER: ::c_int = 8;
pub const CTL_MAXID: ::c_int = 9;
pub const KERN_OSTYPE: ::c_int = 1;
pub const KERN_OSRELEASE: ::c_int = 2;
pub const KERN_OSREV: ::c_int = 3;
pub const KERN_VERSION: ::c_int = 4;
pub const KERN_MAXVNODES: ::c_int = 5;
pub const KERN_MAXPROC: ::c_int = 6;
pub const KERN_MAXFILES: ::c_int = 7;
pub const KERN_ARGMAX: ::c_int = 8;
pub const KERN_SECURELVL: ::c_int = 9;
pub const KERN_HOSTNAME: ::c_int = 10;
pub const KERN_HOSTID: ::c_int = 11;
pub const KERN_CLOCKRATE: ::c_int = 12;
pub const KERN_VNODE: ::c_int = 13;
pub const KERN_PROC: ::c_int = 14;
pub const KERN_FILE: ::c_int = 15;
pub const KERN_PROF: ::c_int = 16;
pub const KERN_POSIX1: ::c_int = 17;
pub const KERN_NGROUPS: ::c_int = 18;
pub const KERN_JOB_CONTROL: ::c_int = 19;
pub const KERN_SAVED_IDS: ::c_int = 20;
pub const KERN_BOOTTIME: ::c_int = 21;
pub const KERN_NISDOMAINNAME: ::c_int = 22;
pub const KERN_DOMAINNAME: ::c_int = KERN_NISDOMAINNAME;
pub const KERN_MAXPARTITIONS: ::c_int = 23;
pub const KERN_KDEBUG: ::c_int = 24;
pub const KERN_UPDATEINTERVAL: ::c_int = 25;
pub const KERN_OSRELDATE: ::c_int = 26;
pub const KERN_NTP_PLL: ::c_int = 27;
pub const KERN_BOOTFILE: ::c_int = 28;
pub const KERN_MAXFILESPERPROC: ::c_int = 29;
pub const KERN_MAXPROCPERUID: ::c_int = 30;
pub const KERN_DUMPDEV: ::c_int = 31;
pub const KERN_IPC: ::c_int = 32;
pub const KERN_DUMMY: ::c_int = 33;
pub const KERN_PS_STRINGS: ::c_int = 34;
pub const KERN_USRSTACK32: ::c_int = 35;
pub const KERN_LOGSIGEXIT: ::c_int = 36;
pub const KERN_SYMFILE: ::c_int = 37;
pub const KERN_PROCARGS: ::c_int = 38;
pub const KERN_NETBOOT: ::c_int = 40;
pub const KERN_SYSV: ::c_int = 42;
pub const KERN_AFFINITY: ::c_int = 43;
pub const KERN_TRANSLATE: ::c_int = 44;
pub const KERN_CLASSIC: ::c_int = KERN_TRANSLATE;
pub const KERN_EXEC: ::c_int = 45;
pub const KERN_CLASSICHANDLER: ::c_int = KERN_EXEC;
pub const KERN_AIOMAX: ::c_int = 46;
pub const KERN_AIOPROCMAX: ::c_int = 47;
pub const KERN_AIOTHREADS: ::c_int = 48;
pub const KERN_COREFILE: ::c_int = 50;
pub const KERN_COREDUMP: ::c_int = 51;
pub const KERN_SUGID_COREDUMP: ::c_int = 52;
pub const KERN_PROCDELAYTERM: ::c_int = 53;
pub const KERN_SHREG_PRIVATIZABLE: ::c_int = 54;
pub const KERN_LOW_PRI_WINDOW: ::c_int = 56;
pub const KERN_LOW_PRI_DELAY: ::c_int = 57;
pub const KERN_POSIX: ::c_int = 58;
pub const KERN_USRSTACK64: ::c_int = 59;
pub const KERN_NX_PROTECTION: ::c_int = 60;
pub const KERN_TFP: ::c_int = 61;
pub const KERN_PROCNAME: ::c_int = 62;
pub const KERN_THALTSTACK: ::c_int = 63;
pub const KERN_SPECULATIVE_READS: ::c_int = 64;
pub const KERN_OSVERSION: ::c_int = 65;
pub const KERN_SAFEBOOT: ::c_int = 66;
pub const KERN_RAGEVNODE: ::c_int = 68;
pub const KERN_TTY: ::c_int = 69;
pub const KERN_CHECKOPENEVT: ::c_int = 70;
pub const KERN_THREADNAME: ::c_int = 71;
pub const KERN_MAXID: ::c_int = 72;
pub const KERN_RAGE_PROC: ::c_int = 1;
pub const KERN_RAGE_THREAD: ::c_int = 2;
pub const KERN_UNRAGE_PROC: ::c_int = 3;
pub const KERN_UNRAGE_THREAD: ::c_int = 4;
pub const KERN_OPENEVT_PROC: ::c_int = 1;
pub const KERN_UNOPENEVT_PROC: ::c_int = 2;
pub const KERN_TFP_POLICY: ::c_int = 1;
pub const KERN_TFP_POLICY_DENY: ::c_int = 0;
pub const KERN_TFP_POLICY_DEFAULT: ::c_int = 2;
pub const KERN_KDEFLAGS: ::c_int = 1;
pub const KERN_KDDFLAGS: ::c_int = 2;
pub const KERN_KDENABLE: ::c_int = 3;
pub const KERN_KDSETBUF: ::c_int = 4;
pub const KERN_KDGETBUF: ::c_int = 5;
pub const KERN_KDSETUP: ::c_int = 6;
pub const KERN_KDREMOVE: ::c_int = 7;
pub const KERN_KDSETREG: ::c_int = 8;
pub const KERN_KDGETREG: ::c_int = 9;
pub const KERN_KDREADTR: ::c_int = 10;
pub const KERN_KDPIDTR: ::c_int = 11;
pub const KERN_KDTHRMAP: ::c_int = 12;
pub const KERN_KDPIDEX: ::c_int = 14;
pub const KERN_KDSETRTCDEC: ::c_int = 15;
pub const KERN_KDGETENTROPY: ::c_int = 16;
pub const KERN_KDWRITETR: ::c_int = 17;
pub const KERN_KDWRITEMAP: ::c_int = 18;
#[doc(hidden)]
#[deprecated(since = "0.2.49", note = "Removed in MacOSX 10.12")]
pub const KERN_KDENABLE_BG_TRACE: ::c_int = 19;
#[doc(hidden)]
#[deprecated(since = "0.2.49", note = "Removed in MacOSX 10.12")]
pub const KERN_KDDISABLE_BG_TRACE: ::c_int = 20;
pub const KERN_KDREADCURTHRMAP: ::c_int = 21;
pub const KERN_KDSET_TYPEFILTER: ::c_int = 22;
pub const KERN_KDBUFWAIT: ::c_int = 23;
pub const KERN_KDCPUMAP: ::c_int = 24;
pub const KERN_PROC_ALL: ::c_int = 0;
pub const KERN_PROC_PID: ::c_int = 1;
pub const KERN_PROC_PGRP: ::c_int = 2;
pub const KERN_PROC_SESSION: ::c_int = 3;
pub const KERN_PROC_TTY: ::c_int = 4;
pub const KERN_PROC_UID: ::c_int = 5;
pub const KERN_PROC_RUID: ::c_int = 6;
pub const KERN_PROC_LCID: ::c_int = 7;
pub const KERN_SUCCESS: ::c_int = 0;
pub const KERN_INVALID_ADDRESS: ::c_int = 1;
pub const KERN_PROTECTION_FAILURE: ::c_int = 2;
pub const KERN_NO_SPACE: ::c_int = 3;
pub const KERN_INVALID_ARGUMENT: ::c_int = 4;
pub const KERN_FAILURE: ::c_int = 5;
pub const KERN_RESOURCE_SHORTAGE: ::c_int = 6;
pub const KERN_NOT_RECEIVER: ::c_int = 7;
pub const KERN_NO_ACCESS: ::c_int = 8;
pub const KERN_MEMORY_FAILURE: ::c_int = 9;
pub const KERN_MEMORY_ERROR: ::c_int = 10;
pub const KERN_ALREADY_IN_SET: ::c_int = 11;
pub const KERN_NOT_IN_SET: ::c_int = 12;
pub const KERN_NAME_EXISTS: ::c_int = 13;
pub const KERN_ABORTED: ::c_int = 14;
pub const KERN_INVALID_NAME: ::c_int = 15;
pub const KERN_INVALID_TASK: ::c_int = 16;
pub const KERN_INVALID_RIGHT: ::c_int = 17;
pub const KERN_INVALID_VALUE: ::c_int = 18;
pub const KERN_UREFS_OVERFLOW: ::c_int = 19;
pub const KERN_INVALID_CAPABILITY: ::c_int = 20;
pub const KERN_RIGHT_EXISTS: ::c_int = 21;
pub const KERN_INVALID_HOST: ::c_int = 22;
pub const KERN_MEMORY_PRESENT: ::c_int = 23;
pub const KERN_MEMORY_DATA_MOVED: ::c_int = 24;
pub const KERN_MEMORY_RESTART_COPY: ::c_int = 25;
pub const KERN_INVALID_PROCESSOR_SET: ::c_int = 26;
pub const KERN_POLICY_LIMIT: ::c_int = 27;
pub const KERN_INVALID_POLICY: ::c_int = 28;
pub const KERN_INVALID_OBJECT: ::c_int = 29;
pub const KERN_ALREADY_WAITING: ::c_int = 30;
pub const KERN_DEFAULT_SET: ::c_int = 31;
pub const KERN_EXCEPTION_PROTECTED: ::c_int = 32;
pub const KERN_INVALID_LEDGER: ::c_int = 33;
pub const KERN_INVALID_MEMORY_CONTROL: ::c_int = 34;
pub const KERN_INVALID_SECURITY: ::c_int = 35;
pub const KERN_NOT_DEPRESSED: ::c_int = 36;
pub const KERN_TERMINATED: ::c_int = 37;
pub const KERN_LOCK_SET_DESTROYED: ::c_int = 38;
pub const KERN_LOCK_UNSTABLE: ::c_int = 39;
pub const KERN_LOCK_OWNED: ::c_int = 40;
pub const KERN_LOCK_OWNED_SELF: ::c_int = 41;
pub const KERN_SEMAPHORE_DESTROYED: ::c_int = 42;
pub const KERN_RPC_SERVER_TERMINATED: ::c_int = 43;
pub const KERN_RPC_TERMINATE_ORPHAN: ::c_int = 44;
pub const KERN_RPC_CONTINUE_ORPHAN: ::c_int = 45;
pub const KERN_NOT_SUPPORTED: ::c_int = 46;
pub const KERN_NODE_DOWN: ::c_int = 47;
pub const KERN_NOT_WAITING: ::c_int = 48;
pub const KERN_OPERATION_TIMED_OUT: ::c_int = 49;
pub const KERN_CODESIGN_ERROR: ::c_int = 50;
pub const KERN_POLICY_STATIC: ::c_int = 51;
pub const KERN_INSUFFICIENT_BUFFER_SIZE: ::c_int = 52;
pub const KIPC_MAXSOCKBUF: ::c_int = 1;
pub const KIPC_SOCKBUF_WASTE: ::c_int = 2;
pub const KIPC_SOMAXCONN: ::c_int = 3;
pub const KIPC_MAX_LINKHDR: ::c_int = 4;
pub const KIPC_MAX_PROTOHDR: ::c_int = 5;
pub const KIPC_MAX_HDR: ::c_int = 6;
pub const KIPC_MAX_DATALEN: ::c_int = 7;
pub const KIPC_MBSTAT: ::c_int = 8;
pub const KIPC_NMBCLUSTERS: ::c_int = 9;
pub const KIPC_SOQLIMITCOMPAT: ::c_int = 10;
pub const VM_METER: ::c_int = 1;
pub const VM_LOADAVG: ::c_int = 2;
pub const VM_MACHFACTOR: ::c_int = 4;
pub const VM_SWAPUSAGE: ::c_int = 5;
pub const VM_MAXID: ::c_int = 6;
pub const VM_PROT_NONE: ::vm_prot_t = 0x00;
pub const VM_PROT_READ: ::vm_prot_t = 0x01;
pub const VM_PROT_WRITE: ::vm_prot_t = 0x02;
pub const VM_PROT_EXECUTE: ::vm_prot_t = 0x04;
pub const MEMORY_OBJECT_NULL: ::memory_object_t = 0;
pub const HW_MACHINE: ::c_int = 1;
pub const HW_MODEL: ::c_int = 2;
pub const HW_NCPU: ::c_int = 3;
pub const HW_BYTEORDER: ::c_int = 4;
pub const HW_PHYSMEM: ::c_int = 5;
pub const HW_USERMEM: ::c_int = 6;
pub const HW_PAGESIZE: ::c_int = 7;
pub const HW_DISKNAMES: ::c_int = 8;
pub const HW_DISKSTATS: ::c_int = 9;
pub const HW_EPOCH: ::c_int = 10;
pub const HW_FLOATINGPT: ::c_int = 11;
pub const HW_MACHINE_ARCH: ::c_int = 12;
pub const HW_VECTORUNIT: ::c_int = 13;
pub const HW_BUS_FREQ: ::c_int = 14;
pub const HW_CPU_FREQ: ::c_int = 15;
pub const HW_CACHELINE: ::c_int = 16;
pub const HW_L1ICACHESIZE: ::c_int = 17;
pub const HW_L1DCACHESIZE: ::c_int = 18;
pub const HW_L2SETTINGS: ::c_int = 19;
pub const HW_L2CACHESIZE: ::c_int = 20;
pub const HW_L3SETTINGS: ::c_int = 21;
pub const HW_L3CACHESIZE: ::c_int = 22;
pub const HW_TB_FREQ: ::c_int = 23;
pub const HW_MEMSIZE: ::c_int = 24;
pub const HW_AVAILCPU: ::c_int = 25;
pub const HW_TARGET: ::c_int = 26;
pub const HW_PRODUCT: ::c_int = 27;
pub const HW_MAXID: ::c_int = 28;
pub const USER_CS_PATH: ::c_int = 1;
pub const USER_BC_BASE_MAX: ::c_int = 2;
pub const USER_BC_DIM_MAX: ::c_int = 3;
pub const USER_BC_SCALE_MAX: ::c_int = 4;
pub const USER_BC_STRING_MAX: ::c_int = 5;
pub const USER_COLL_WEIGHTS_MAX: ::c_int = 6;
pub const USER_EXPR_NEST_MAX: ::c_int = 7;
pub const USER_LINE_MAX: ::c_int = 8;
pub const USER_RE_DUP_MAX: ::c_int = 9;
pub const USER_POSIX2_VERSION: ::c_int = 10;
pub const USER_POSIX2_C_BIND: ::c_int = 11;
pub const USER_POSIX2_C_DEV: ::c_int = 12;
pub const USER_POSIX2_CHAR_TERM: ::c_int = 13;
pub const USER_POSIX2_FORT_DEV: ::c_int = 14;
pub const USER_POSIX2_FORT_RUN: ::c_int = 15;
pub const USER_POSIX2_LOCALEDEF: ::c_int = 16;
pub const USER_POSIX2_SW_DEV: ::c_int = 17;
pub const USER_POSIX2_UPE: ::c_int = 18;
pub const USER_STREAM_MAX: ::c_int = 19;
pub const USER_TZNAME_MAX: ::c_int = 20;
pub const USER_MAXID: ::c_int = 21;
pub const CTL_DEBUG_NAME: ::c_int = 0;
pub const CTL_DEBUG_VALUE: ::c_int = 1;
pub const CTL_DEBUG_MAXID: ::c_int = 20;

pub const PRIO_DARWIN_THREAD: ::c_int = 3;
pub const PRIO_DARWIN_PROCESS: ::c_int = 4;
pub const PRIO_DARWIN_BG: ::c_int = 0x1000;
pub const PRIO_DARWIN_NONUI: ::c_int = 0x1001;

pub const SEM_FAILED: *mut sem_t = -1isize as *mut ::sem_t;

pub const AI_PASSIVE: ::c_int = 0x00000001;
pub const AI_CANONNAME: ::c_int = 0x00000002;
pub const AI_NUMERICHOST: ::c_int = 0x00000004;
pub const AI_NUMERICSERV: ::c_int = 0x00001000;
pub const AI_MASK: ::c_int =
    AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST | AI_NUMERICSERV | AI_ADDRCONFIG;
pub const AI_ALL: ::c_int = 0x00000100;
pub const AI_V4MAPPED_CFG: ::c_int = 0x00000200;
pub const AI_ADDRCONFIG: ::c_int = 0x00000400;
pub const AI_V4MAPPED: ::c_int = 0x00000800;
pub const AI_DEFAULT: ::c_int = AI_V4MAPPED_CFG | AI_ADDRCONFIG;
pub const AI_UNUSABLE: ::c_int = 0x10000000;

pub const SIGEV_NONE: ::c_int = 0;
pub const SIGEV_SIGNAL: ::c_int = 1;
pub const SIGEV_THREAD: ::c_int = 3;

pub const AIO_CANCELED: ::c_int = 2;
pub const AIO_NOTCANCELED: ::c_int = 4;
pub const AIO_ALLDONE: ::c_int = 1;
#[deprecated(
    since = "0.2.64",
    note = "Can vary at runtime.  Use sysconf(3) instead"
)]
pub const AIO_LISTIO_MAX: ::c_int = 16;
pub const LIO_NOP: ::c_int = 0;
pub const LIO_WRITE: ::c_int = 2;
pub const LIO_READ: ::c_int = 1;
pub const LIO_WAIT: ::c_int = 2;
pub const LIO_NOWAIT: ::c_int = 1;

pub const WEXITED: ::c_int = 0x00000004;
pub const WSTOPPED: ::c_int = 0x00000008;
pub const WCONTINUED: ::c_int = 0x00000010;
pub const WNOWAIT: ::c_int = 0x00000020;

pub const P_ALL: idtype_t = 0;
pub const P_PID: idtype_t = 1;
pub const P_PGID: idtype_t = 2;

pub const UTIME_OMIT: c_long = -2;
pub const UTIME_NOW: c_long = -1;

pub const XATTR_NOFOLLOW: ::c_int = 0x0001;
pub const XATTR_CREATE: ::c_int = 0x0002;
pub const XATTR_REPLACE: ::c_int = 0x0004;
pub const XATTR_NOSECURITY: ::c_int = 0x0008;
pub const XATTR_NODEFAULT: ::c_int = 0x0010;
pub const XATTR_SHOWCOMPRESSION: ::c_int = 0x0020;

pub const NET_RT_IFLIST2: ::c_int = 0x0006;

// net/route.h
pub const RTF_UP: ::c_int = 0x1;
pub const RTF_GATEWAY: ::c_int = 0x2;
pub const RTF_HOST: ::c_int = 0x4;
pub const RTF_REJECT: ::c_int = 0x8;
pub const RTF_DYNAMIC: ::c_int = 0x10;
pub const RTF_MODIFIED: ::c_int = 0x20;
pub const RTF_DONE: ::c_int = 0x40;
pub const RTF_DELCLONE: ::c_int = 0x80;
pub const RTF_CLONING: ::c_int = 0x100;
pub const RTF_XRESOLVE: ::c_int = 0x200;
pub const RTF_LLINFO: ::c_int = 0x400;
pub const RTF_STATIC: ::c_int = 0x800;
pub const RTF_BLACKHOLE: ::c_int = 0x1000;
pub const RTF_NOIFREF: ::c_int = 0x2000;
pub const RTF_PROTO2: ::c_int = 0x4000;
pub const RTF_PROTO1: ::c_int = 0x8000;
pub const RTF_PRCLONING: ::c_int = 0x10000;
pub const RTF_WASCLONED: ::c_int = 0x20000;
pub const RTF_PROTO3: ::c_int = 0x40000;
pub const RTF_PINNED: ::c_int = 0x100000;
pub const RTF_LOCAL: ::c_int = 0x200000;
pub const RTF_BROADCAST: ::c_int = 0x400000;
pub const RTF_MULTICAST: ::c_int = 0x800000;
pub const RTF_IFSCOPE: ::c_int = 0x1000000;
pub const RTF_CONDEMNED: ::c_int = 0x2000000;
pub const RTF_IFREF: ::c_int = 0x4000000;
pub const RTF_PROXY: ::c_int = 0x8000000;
pub const RTF_ROUTER: ::c_int = 0x10000000;
pub const RTF_DEAD: ::c_int = 0x20000000;
pub const RTF_GLOBAL: ::c_int = 0x40000000;

pub const RTM_VERSION: ::c_int = 5;

// Message types
pub const RTM_ADD: ::c_int = 0x1;
pub const RTM_DELETE: ::c_int = 0x2;
pub const RTM_CHANGE: ::c_int = 0x3;
pub const RTM_GET: ::c_int = 0x4;
pub const RTM_LOSING: ::c_int = 0x5;
pub const RTM_REDIRECT: ::c_int = 0x6;
pub const RTM_MISS: ::c_int = 0x7;
pub const RTM_LOCK: ::c_int = 0x8;
pub const RTM_OLDADD: ::c_int = 0x9;
pub const RTM_OLDDEL: ::c_int = 0xa;
pub const RTM_RESOLVE: ::c_int = 0xb;
pub const RTM_NEWADDR: ::c_int = 0xc;
pub const RTM_DELADDR: ::c_int = 0xd;
pub const RTM_IFINFO: ::c_int = 0xe;
pub const RTM_NEWMADDR: ::c_int = 0xf;
pub const RTM_DELMADDR: ::c_int = 0x10;
pub const RTM_IFINFO2: ::c_int = 0x12;
pub const RTM_NEWMADDR2: ::c_int = 0x13;
pub const RTM_GET2: ::c_int = 0x14;

// Bitmask values for rtm_inits and rmx_locks.
pub const RTV_MTU: ::c_int = 0x1;
pub const RTV_HOPCOUNT: ::c_int = 0x2;
pub const RTV_EXPIRE: ::c_int = 0x4;
pub const RTV_RPIPE: ::c_int = 0x8;
pub const RTV_SPIPE: ::c_int = 0x10;
pub const RTV_SSTHRESH: ::c_int = 0x20;
pub const RTV_RTT: ::c_int = 0x40;
pub const RTV_RTTVAR: ::c_int = 0x80;

// Bitmask values for rtm_addrs.
pub const RTA_DST: ::c_int = 0x1;
pub const RTA_GATEWAY: ::c_int = 0x2;
pub const RTA_NETMASK: ::c_int = 0x4;
pub const RTA_GENMASK: ::c_int = 0x8;
pub const RTA_IFP: ::c_int = 0x10;
pub const RTA_IFA: ::c_int = 0x20;
pub const RTA_AUTHOR: ::c_int = 0x40;
pub const RTA_BRD: ::c_int = 0x80;

// Index offsets for sockaddr array for alternate internal encoding.
pub const RTAX_DST: ::c_int = 0;
pub const RTAX_GATEWAY: ::c_int = 1;
pub const RTAX_NETMASK: ::c_int = 2;
pub const RTAX_GENMASK: ::c_int = 3;
pub const RTAX_IFP: ::c_int = 4;
pub const RTAX_IFA: ::c_int = 5;
pub const RTAX_AUTHOR: ::c_int = 6;
pub const RTAX_BRD: ::c_int = 7;
pub const RTAX_MAX: ::c_int = 8;

pub const KERN_PROCARGS2: ::c_int = 49;

pub const PROC_PIDTASKALLINFO: ::c_int = 2;
pub const PROC_PIDTBSDINFO: ::c_int = 3;
pub const PROC_PIDTASKINFO: ::c_int = 4;
pub const PROC_PIDTHREADINFO: ::c_int = 5;
pub const PROC_PIDVNODEPATHINFO: ::c_int = 9;
pub const PROC_PIDPATHINFO_MAXSIZE: ::c_int = 4096;
pub const PROC_CSM_ALL: ::c_uint = 0x0001;
pub const PROC_CSM_NOSMT: ::c_uint = 0x0002;
pub const PROC_CSM_TECS: ::c_uint = 0x0004;
pub const MAXCOMLEN: usize = 16;
pub const MAXTHREADNAMESIZE: usize = 64;

pub const XUCRED_VERSION: ::c_uint = 0;

pub const LC_SEGMENT: u32 = 0x1;
pub const LC_SEGMENT_64: u32 = 0x19;

pub const MH_MAGIC: u32 = 0xfeedface;
pub const MH_MAGIC_64: u32 = 0xfeedfacf;

// net/if_utun.h
pub const UTUN_OPT_FLAGS: ::c_int = 1;
pub const UTUN_OPT_IFNAME: ::c_int = 2;

// net/bpf.h
pub const DLT_NULL: ::c_uint = 0; // no link-layer encapsulation
pub const DLT_EN10MB: ::c_uint = 1; // Ethernet (10Mb)
pub const DLT_EN3MB: ::c_uint = 2; // Experimental Ethernet (3Mb)
pub const DLT_AX25: ::c_uint = 3; // Amateur Radio AX.25
pub const DLT_PRONET: ::c_uint = 4; // Proteon ProNET Token Ring
pub const DLT_CHAOS: ::c_uint = 5; // Chaos
pub const DLT_IEEE802: ::c_uint = 6; // IEEE 802 Networks
pub const DLT_ARCNET: ::c_uint = 7; // ARCNET
pub const DLT_SLIP: ::c_uint = 8; // Serial Line IP
pub const DLT_PPP: ::c_uint = 9; // Point-to-point Protocol
pub const DLT_FDDI: ::c_uint = 10; // FDDI
pub const DLT_ATM_RFC1483: ::c_uint = 11; // LLC/SNAP encapsulated atm
pub const DLT_RAW: ::c_uint = 12; // raw IP
pub const DLT_LOOP: ::c_uint = 108;

// https://github.com/apple/darwin-xnu/blob/master/bsd/net/bpf.h#L100
// sizeof(i32)
pub const BPF_ALIGNMENT: ::c_int = 4;

// sys/mount.h
pub const MNT_NODEV: ::c_int = 0x00000010;
pub const MNT_UNION: ::c_int = 0x00000020;
pub const MNT_CPROTECT: ::c_int = 0x00000080;

// MAC labeled / "quarantined" flag
pub const MNT_QUARANTINE: ::c_int = 0x00000400;

// Flags set by internal operations.
pub const MNT_LOCAL: ::c_int = 0x00001000;
pub const MNT_QUOTA: ::c_int = 0x00002000;
pub const MNT_ROOTFS: ::c_int = 0x00004000;
pub const MNT_DOVOLFS: ::c_int = 0x00008000;

pub const MNT_DONTBROWSE: ::c_int = 0x00100000;
pub const MNT_IGNORE_OWNERSHIP: ::c_int = 0x00200000;
pub const MNT_AUTOMOUNTED: ::c_int = 0x00400000;
pub const MNT_JOURNALED: ::c_int = 0x00800000;
pub const MNT_NOUSERXATTR: ::c_int = 0x01000000;
pub const MNT_DEFWRITE: ::c_int = 0x02000000;
pub const MNT_MULTILABEL: ::c_int = 0x04000000;
pub const MNT_NOATIME: ::c_int = 0x10000000;
pub const MNT_SNAPSHOT: ::c_int = 0x40000000;

// External filesystem command modifier flags.
pub const MNT_NOBLOCK: ::c_int = 0x00020000;

// sys/spawn.h:
pub const POSIX_SPAWN_RESETIDS: ::c_int = 0x01;
pub const POSIX_SPAWN_SETPGROUP: ::c_int = 0x02;
pub const POSIX_SPAWN_SETSIGDEF: ::c_int = 0x04;
pub const POSIX_SPAWN_SETSIGMASK: ::c_int = 0x08;
pub const POSIX_SPAWN_SETEXEC: ::c_int = 0x40;
pub const POSIX_SPAWN_START_SUSPENDED: ::c_int = 0x80;
pub const POSIX_SPAWN_CLOEXEC_DEFAULT: ::c_int = 0x4000;

// sys/ipc.h:
pub const IPC_CREAT: ::c_int = 0x200;
pub const IPC_EXCL: ::c_int = 0x400;
pub const IPC_NOWAIT: ::c_int = 0x800;
pub const IPC_PRIVATE: key_t = 0;

pub const IPC_RMID: ::c_int = 0;
pub const IPC_SET: ::c_int = 1;
pub const IPC_STAT: ::c_int = 2;

pub const IPC_R: ::c_int = 0x100;
pub const IPC_W: ::c_int = 0x80;
pub const IPC_M: ::c_int = 0x1000;

// sys/sem.h
pub const SEM_UNDO: ::c_int = 0o10000;

pub const GETNCNT: ::c_int = 3;
pub const GETPID: ::c_int = 4;
pub const GETVAL: ::c_int = 5;
pub const GETALL: ::c_int = 6;
pub const GETZCNT: ::c_int = 7;
pub const SETVAL: ::c_int = 8;
pub const SETALL: ::c_int = 9;

// sys/shm.h
pub const SHM_RDONLY: ::c_int = 0x1000;
pub const SHM_RND: ::c_int = 0x2000;
#[cfg(target_arch = "aarch64")]
pub const SHMLBA: ::c_int = 16 * 1024;
#[cfg(not(target_arch = "aarch64"))]
pub const SHMLBA: ::c_int = 4096;
pub const SHM_R: ::c_int = IPC_R;
pub const SHM_W: ::c_int = IPC_W;

// Flags for chflags(2)
pub const UF_SETTABLE: ::c_uint = 0x0000ffff;
pub const UF_NODUMP: ::c_uint = 0x00000001;
pub const UF_IMMUTABLE: ::c_uint = 0x00000002;
pub const UF_APPEND: ::c_uint = 0x00000004;
pub const UF_OPAQUE: ::c_uint = 0x00000008;
pub const UF_COMPRESSED: ::c_uint = 0x00000020;
pub const UF_TRACKED: ::c_uint = 0x00000040;
pub const SF_SETTABLE: ::c_uint = 0xffff0000;
pub const SF_ARCHIVED: ::c_uint = 0x00010000;
pub const SF_IMMUTABLE: ::c_uint = 0x00020000;
pub const SF_APPEND: ::c_uint = 0x00040000;
pub const UF_HIDDEN: ::c_uint = 0x00008000;

//<sys/timex.h>
pub const NTP_API: ::c_int = 4;
pub const MAXPHASE: ::c_long = 500000000;
pub const MAXFREQ: ::c_long = 500000;
pub const MINSEC: ::c_int = 256;
pub const MAXSEC: ::c_int = 2048;
pub const NANOSECOND: ::c_long = 1000000000;
pub const SCALE_PPM: ::c_int = 65;
pub const MAXTC: ::c_int = 10;
pub const MOD_OFFSET: ::c_uint = 0x0001;
pub const MOD_FREQUENCY: ::c_uint = 0x0002;
pub const MOD_MAXERROR: ::c_uint = 0x0004;
pub const MOD_ESTERROR: ::c_uint = 0x0008;
pub const MOD_STATUS: ::c_uint = 0x0010;
pub const MOD_TIMECONST: ::c_uint = 0x0020;
pub const MOD_PPSMAX: ::c_uint = 0x0040;
pub const MOD_TAI: ::c_uint = 0x0080;
pub const MOD_MICRO: ::c_uint = 0x1000;
pub const MOD_NANO: ::c_uint = 0x2000;
pub const MOD_CLKB: ::c_uint = 0x4000;
pub const MOD_CLKA: ::c_uint = 0x8000;
pub const STA_PLL: ::c_int = 0x0001;
pub const STA_PPSFREQ: ::c_int = 0x0002;
pub const STA_PPSTIME: ::c_int = 0x0004;
pub const STA_FLL: ::c_int = 0x0008;
pub const STA_INS: ::c_int = 0x0010;
pub const STA_DEL: ::c_int = 0x0020;
pub const STA_UNSYNC: ::c_int = 0x0040;
pub const STA_FREQHOLD: ::c_int = 0x0080;
pub const STA_PPSSIGNAL: ::c_int = 0x0100;
pub const STA_PPSJITTER: ::c_int = 0x0200;
pub const STA_PPSWANDER: ::c_int = 0x0400;
pub const STA_PPSERROR: ::c_int = 0x0800;
pub const STA_CLOCKERR: ::c_int = 0x1000;
pub const STA_NANO: ::c_int = 0x2000;
pub const STA_MODE: ::c_int = 0x4000;
pub const STA_CLK: ::c_int = 0x8000;
pub const STA_RONLY: ::c_int = STA_PPSSIGNAL
    | STA_PPSJITTER
    | STA_PPSWANDER
    | STA_PPSERROR
    | STA_CLOCKERR
    | STA_NANO
    | STA_MODE
    | STA_CLK;
pub const TIME_OK: ::c_int = 0;
pub const TIME_INS: ::c_int = 1;
pub const TIME_DEL: ::c_int = 2;
pub const TIME_OOP: ::c_int = 3;
pub const TIME_WAIT: ::c_int = 4;
pub const TIME_ERROR: ::c_int = 5;

// <sys/mount.h>
pub const MNT_WAIT: ::c_int = 1;
pub const MNT_NOWAIT: ::c_int = 2;

// <mach/thread_policy.h>
pub const THREAD_STANDARD_POLICY: ::c_int = 1;
pub const THREAD_STANDARD_POLICY_COUNT: ::c_int = 0;
pub const THREAD_EXTENDED_POLICY: ::c_int = 1;
pub const THREAD_TIME_CONSTRAINT_POLICY: ::c_int = 2;
pub const THREAD_PRECEDENCE_POLICY: ::c_int = 3;
pub const THREAD_AFFINITY_POLICY: ::c_int = 4;
pub const THREAD_AFFINITY_TAG_NULL: ::c_int = 0;
pub const THREAD_BACKGROUND_POLICY: ::c_int = 5;
pub const THREAD_BACKGROUND_POLICY_DARWIN_BG: ::c_int = 0x1000;
pub const THREAD_LATENCY_QOS_POLICY: ::c_int = 7;
pub const THREAD_THROUGHPUT_QOS_POLICY: ::c_int = 8;

// <mach/thread_info.h>
pub const TH_STATE_RUNNING: ::c_int = 1;
pub const TH_STATE_STOPPED: ::c_int = 2;
pub const TH_STATE_WAITING: ::c_int = 3;
pub const TH_STATE_UNINTERRUPTIBLE: ::c_int = 4;
pub const TH_STATE_HALTED: ::c_int = 5;
pub const TH_FLAGS_SWAPPED: ::c_int = 0x1;
pub const TH_FLAGS_IDLE: ::c_int = 0x2;
pub const TH_FLAGS_GLOBAL_FORCED_IDLE: ::c_int = 0x4;
pub const THREAD_BASIC_INFO: ::c_int = 3;
pub const THREAD_IDENTIFIER_INFO: ::c_int = 4;
pub const THREAD_EXTENDED_INFO: ::c_int = 5;

// CommonCrypto/CommonCryptoError.h
pub const kCCSuccess: i32 = 0;
pub const kCCParamError: i32 = -4300;
pub const kCCBufferTooSmall: i32 = -4301;
pub const kCCMemoryFailure: i32 = -4302;
pub const kCCAlignmentError: i32 = -4303;
pub const kCCDecodeError: i32 = -4304;
pub const kCCUnimplemented: i32 = -4305;
pub const kCCOverflow: i32 = -4306;
pub const kCCRNGFailure: i32 = -4307;
pub const kCCUnspecifiedError: i32 = -4308;
pub const kCCCallSequenceError: i32 = -4309;
pub const kCCKeySizeError: i32 = -4310;
pub const kCCInvalidKey: i32 = -4311;

// mach/host_info.h
pub const HOST_LOAD_INFO: i32 = 1;
pub const HOST_VM_INFO: i32 = 2;
pub const HOST_CPU_LOAD_INFO: i32 = 3;
pub const HOST_VM_INFO64: i32 = 4;
pub const HOST_EXTMOD_INFO64: i32 = 5;
pub const HOST_EXPIRED_TASK_INFO: i32 = 6;

// mach/vm_statistics.h
pub const VM_PAGE_QUERY_PAGE_PRESENT: i32 = 0x1;
pub const VM_PAGE_QUERY_PAGE_FICTITIOUS: i32 = 0x2;
pub const VM_PAGE_QUERY_PAGE_REF: i32 = 0x4;
pub const VM_PAGE_QUERY_PAGE_DIRTY: i32 = 0x8;
pub const VM_PAGE_QUERY_PAGE_PAGED_OUT: i32 = 0x10;
pub const VM_PAGE_QUERY_PAGE_COPIED: i32 = 0x20;
pub const VM_PAGE_QUERY_PAGE_SPECULATIVE: i32 = 0x40;
pub const VM_PAGE_QUERY_PAGE_EXTERNAL: i32 = 0x80;
pub const VM_PAGE_QUERY_PAGE_CS_VALIDATED: i32 = 0x100;
pub const VM_PAGE_QUERY_PAGE_CS_TAINTED: i32 = 0x200;
pub const VM_PAGE_QUERY_PAGE_CS_NX: i32 = 0x400;

// mach/task_info.h
pub const TASK_THREAD_TIMES_INFO: u32 = 3;
pub const HOST_CPU_LOAD_INFO_COUNT: u32 = 4;
pub const MACH_TASK_BASIC_INFO: u32 = 20;

pub const MACH_PORT_NULL: i32 = 0;

pub const RUSAGE_INFO_V0: ::c_int = 0;
pub const RUSAGE_INFO_V1: ::c_int = 1;
pub const RUSAGE_INFO_V2: ::c_int = 2;
pub const RUSAGE_INFO_V3: ::c_int = 3;
pub const RUSAGE_INFO_V4: ::c_int = 4;

cfg_if! {
    if #[cfg(libc_const_extern_fn)] {
        const fn __DARWIN_ALIGN32(p: usize) -> usize {
            const __DARWIN_ALIGNBYTES32: usize = ::mem::size_of::<u32>() - 1;
            p + __DARWIN_ALIGNBYTES32 & !__DARWIN_ALIGNBYTES32
        }
    } else if #[cfg(libc_const_size_of)] {
        fn __DARWIN_ALIGN32(p: usize) -> usize {
            const __DARWIN_ALIGNBYTES32: usize = ::mem::size_of::<u32>() - 1;
            p + __DARWIN_ALIGNBYTES32 & !__DARWIN_ALIGNBYTES32
        }
        pub const THREAD_EXTENDED_POLICY_COUNT: mach_msg_type_number_t =
            (::mem::size_of::<thread_extended_policy_data_t>() / ::mem::size_of::<integer_t>())
            as mach_msg_type_number_t;
        pub const THREAD_TIME_CONSTRAINT_POLICY_COUNT: mach_msg_type_number_t =
            (::mem::size_of::<thread_time_constraint_policy_data_t>() /
             ::mem::size_of::<integer_t>()) as mach_msg_type_number_t;
        pub const THREAD_PRECEDENCE_POLICY_COUNT: mach_msg_type_number_t =
            (::mem::size_of::<thread_precedence_policy_data_t>() / ::mem::size_of::<integer_t>())
            as mach_msg_type_number_t;
        pub const THREAD_AFFINITY_POLICY_COUNT: mach_msg_type_number_t =
            (::mem::size_of::<thread_affinity_policy_data_t>() / ::mem::size_of::<integer_t>())
            as mach_msg_type_number_t;
        pub const THREAD_BACKGROUND_POLICY_COUNT: mach_msg_type_number_t =
            (::mem::size_of::<thread_background_policy_data_t>() / ::mem::size_of::<integer_t>())
            as mach_msg_type_number_t;
        pub const THREAD_LATENCY_QOS_POLICY_COUNT: mach_msg_type_number_t =
            (::mem::size_of::<thread_latency_qos_policy_data_t>() / ::mem::size_of::<integer_t>())
            as mach_msg_type_number_t;
        pub const THREAD_THROUGHPUT_QOS_POLICY_COUNT: mach_msg_type_number_t =
            (::mem::size_of::<thread_throughput_qos_policy_data_t>() /
             ::mem::size_of::<integer_t>()) as mach_msg_type_number_t;
        pub const THREAD_BASIC_INFO_COUNT: mach_msg_type_number_t =
            (::mem::size_of::<thread_basic_info_data_t>() / ::mem::size_of::<integer_t>())
            as mach_msg_type_number_t;
        pub const THREAD_IDENTIFIER_INFO_COUNT: mach_msg_type_number_t =
            (::mem::size_of::<thread_identifier_info_data_t>() / ::mem::size_of::<integer_t>())
            as mach_msg_type_number_t;
        pub const THREAD_EXTENDED_INFO_COUNT: mach_msg_type_number_t =
            (::mem::size_of::<thread_extended_info_data_t>() / ::mem::size_of::<integer_t>())
            as mach_msg_type_number_t;

        pub const TASK_THREAD_TIMES_INFO_COUNT: u32 =
            (::mem::size_of::<task_thread_times_info_data_t>()
            / ::mem::size_of::<natural_t>()) as u32;
        pub const MACH_TASK_BASIC_INFO_COUNT: u32 = (::mem::size_of::<mach_task_basic_info_data_t>()
            / ::mem::size_of::<natural_t>()) as u32;
        pub const HOST_VM_INFO64_COUNT: mach_msg_type_number_t =
            (::mem::size_of::<vm_statistics64_data_t>() / ::mem::size_of::<integer_t>())
            as mach_msg_type_number_t;
    } else {
        fn __DARWIN_ALIGN32(p: usize) -> usize {
            let __DARWIN_ALIGNBYTES32: usize = ::mem::size_of::<u32>() - 1;
            p + __DARWIN_ALIGNBYTES32 & !__DARWIN_ALIGNBYTES32
        }
        pub const THREAD_EXTENDED_POLICY_COUNT: mach_msg_type_number_t = 1;
        pub const THREAD_TIME_CONSTRAINT_POLICY_COUNT: mach_msg_type_number_t = 4;
        pub const THREAD_PRECEDENCE_POLICY_COUNT: mach_msg_type_number_t = 1;
        pub const THREAD_AFFINITY_POLICY_COUNT: mach_msg_type_number_t = 1;
        pub const THREAD_BACKGROUND_POLICY_COUNT: mach_msg_type_number_t = 1;
        pub const THREAD_LATENCY_QOS_POLICY_COUNT: mach_msg_type_number_t = 1;
        pub const THREAD_THROUGHPUT_QOS_POLICY_COUNT: mach_msg_type_number_t = 1;
        pub const THREAD_BASIC_INFO_COUNT: mach_msg_type_number_t = 10;
        pub const THREAD_IDENTIFIER_INFO_COUNT: mach_msg_type_number_t = 6;
        pub const THREAD_EXTENDED_INFO_COUNT: mach_msg_type_number_t = 28;
        pub const TASK_THREAD_TIMES_INFO_COUNT: u32 = 4;
        pub const MACH_TASK_BASIC_INFO_COUNT: u32 = 12;
        pub const HOST_VM_INFO64_COUNT: mach_msg_type_number_t = 38;
    }
}

f! {
    pub fn CMSG_NXTHDR(mhdr: *const ::msghdr,
                       cmsg: *const ::cmsghdr) -> *mut ::cmsghdr {
        if cmsg.is_null() {
            return ::CMSG_FIRSTHDR(mhdr);
        };
        let cmsg_len = (*cmsg).cmsg_len as usize;
        let next = cmsg as usize + __DARWIN_ALIGN32(cmsg_len as usize);
        let max = (*mhdr).msg_control as usize
                    + (*mhdr).msg_controllen as usize;
        if next + __DARWIN_ALIGN32(::mem::size_of::<::cmsghdr>()) > max {
            0 as *mut ::cmsghdr
        } else {
            next as *mut ::cmsghdr
        }
    }

    pub fn CMSG_DATA(cmsg: *const ::cmsghdr) -> *mut ::c_uchar {
        (cmsg as *mut ::c_uchar)
            .offset(__DARWIN_ALIGN32(::mem::size_of::<::cmsghdr>()) as isize)
    }

    pub {const} fn CMSG_SPACE(length: ::c_uint) -> ::c_uint {
        (__DARWIN_ALIGN32(::mem::size_of::<::cmsghdr>())
            + __DARWIN_ALIGN32(length as usize))
            as ::c_uint
    }

    pub fn CMSG_LEN(length: ::c_uint) -> ::c_uint {
        (__DARWIN_ALIGN32(::mem::size_of::<::cmsghdr>()) + length as usize)
            as ::c_uint
    }

    pub {const} fn VM_MAKE_TAG(id: u8) -> u32 {
        (id as u32) << 24u32
    }
}

safe_f! {
    pub {const} fn WSTOPSIG(status: ::c_int) -> ::c_int {
        status >> 8
    }

    pub {const} fn _WSTATUS(status: ::c_int) -> ::c_int {
        status & 0x7f
    }

    pub {const} fn WIFCONTINUED(status: ::c_int) -> bool {
        _WSTATUS(status) == _WSTOPPED && WSTOPSIG(status) == 0x13
    }

    pub {const} fn WIFSIGNALED(status: ::c_int) -> bool {
        _WSTATUS(status) != _WSTOPPED && _WSTATUS(status) != 0
    }

    pub {const} fn WIFSTOPPED(status: ::c_int) -> bool {
        _WSTATUS(status) == _WSTOPPED && WSTOPSIG(status) != 0x13
    }
}

extern "C" {
    pub fn setgrent();
    #[doc(hidden)]
    #[deprecated(since = "0.2.49", note = "Deprecated in MacOSX 10.5")]
    #[cfg_attr(not(target_arch = "aarch64"), link_name = "daemon$1050")]
    pub fn daemon(nochdir: ::c_int, noclose: ::c_int) -> ::c_int;
    #[doc(hidden)]
    #[deprecated(since = "0.2.49", note = "Deprecated in MacOSX 10.10")]
    pub fn sem_destroy(sem: *mut sem_t) -> ::c_int;
    #[doc(hidden)]
    #[deprecated(since = "0.2.49", note = "Deprecated in MacOSX 10.10")]
    pub fn sem_init(sem: *mut sem_t, pshared: ::c_int, value: ::c_uint) -> ::c_int;
    pub fn aio_read(aiocbp: *mut aiocb) -> ::c_int;
    pub fn aio_write(aiocbp: *mut aiocb) -> ::c_int;
    pub fn aio_fsync(op: ::c_int, aiocbp: *mut aiocb) -> ::c_int;
    pub fn aio_error(aiocbp: *const aiocb) -> ::c_int;
    pub fn aio_return(aiocbp: *mut aiocb) -> ::ssize_t;
    #[cfg_attr(
        all(target_os = "macos", target_arch = "x86"),
        link_name = "aio_suspend$UNIX2003"
    )]
    pub fn aio_suspend(
        aiocb_list: *const *const aiocb,
        nitems: ::c_int,
        timeout: *const ::timespec,
    ) -> ::c_int;
    pub fn aio_cancel(fd: ::c_int, aiocbp: *mut aiocb) -> ::c_int;
    pub fn chflags(path: *const ::c_char, flags: ::c_uint) -> ::c_int;
    pub fn fchflags(fd: ::c_int, flags: ::c_uint) -> ::c_int;
    pub fn clock_getres(clk_id: ::clockid_t, tp: *mut ::timespec) -> ::c_int;
    pub fn clock_gettime(clk_id: ::clockid_t, tp: *mut ::timespec) -> ::c_int;
    pub fn lio_listio(
        mode: ::c_int,
        aiocb_list: *const *mut aiocb,
        nitems: ::c_int,
        sevp: *mut sigevent,
    ) -> ::c_int;

    pub fn dirfd(dirp: *mut ::DIR) -> ::c_int;

    pub fn lutimes(file: *const ::c_char, times: *const ::timeval) -> ::c_int;

    pub fn gettimeofday(tp: *mut ::timeval, tz: *mut ::c_void) -> ::c_int;
    pub fn getutxent() -> *mut utmpx;
    pub fn getutxid(ut: *const utmpx) -> *mut utmpx;
    pub fn getutxline(ut: *const utmpx) -> *mut utmpx;
    pub fn pututxline(ut: *const utmpx) -> *mut utmpx;
    pub fn setutxent();
    pub fn endutxent();
    pub fn utmpxname(file: *const ::c_char) -> ::c_int;

    pub fn getnameinfo(
        sa: *const ::sockaddr,
        salen: ::socklen_t,
        host: *mut ::c_char,
        hostlen: ::socklen_t,
        serv: *mut ::c_char,
        sevlen: ::socklen_t,
        flags: ::c_int,
    ) -> ::c_int;
    pub fn mincore(addr: *const ::c_void, len: ::size_t, vec: *mut ::c_char) -> ::c_int;
    pub fn sysctlnametomib(
        name: *const ::c_char,
        mibp: *mut ::c_int,
        sizep: *mut ::size_t,
    ) -> ::c_int;
    #[cfg_attr(
        all(target_os = "macos", target_arch = "x86"),
        link_name = "mprotect$UNIX2003"
    )]
    pub fn mprotect(addr: *mut ::c_void, len: ::size_t, prot: ::c_int) -> ::c_int;
    pub fn semget(key: key_t, nsems: ::c_int, semflg: ::c_int) -> ::c_int;
    #[cfg_attr(
        all(target_os = "macos", target_arch = "x86"),
        link_name = "semctl$UNIX2003"
    )]
    pub fn semctl(semid: ::c_int, semnum: ::c_int, cmd: ::c_int, ...) -> ::c_int;
    pub fn semop(semid: ::c_int, sops: *mut sembuf, nsops: ::size_t) -> ::c_int;
    pub fn shm_open(name: *const ::c_char, oflag: ::c_int, ...) -> ::c_int;
    pub fn ftok(pathname: *const c_char, proj_id: ::c_int) -> key_t;
    pub fn shmat(shmid: ::c_int, shmaddr: *const ::c_void, shmflg: ::c_int) -> *mut ::c_void;
    pub fn shmdt(shmaddr: *const ::c_void) -> ::c_int;
    #[cfg_attr(
        all(target_os = "macos", target_arch = "x86"),
        link_name = "shmctl$UNIX2003"
    )]
    pub fn shmctl(shmid: ::c_int, cmd: ::c_int, buf: *mut ::shmid_ds) -> ::c_int;
    pub fn shmget(key: key_t, size: ::size_t, shmflg: ::c_int) -> ::c_int;
    pub fn sysctl(
        name: *mut ::c_int,
        namelen: ::c_uint,
        oldp: *mut ::c_void,
        oldlenp: *mut ::size_t,
        newp: *mut ::c_void,
        newlen: ::size_t,
    ) -> ::c_int;
    pub fn sysctlbyname(
        name: *const ::c_char,
        oldp: *mut ::c_void,
        oldlenp: *mut ::size_t,
        newp: *mut ::c_void,
        newlen: ::size_t,
    ) -> ::c_int;
    #[deprecated(since = "0.2.55", note = "Use the `mach2` crate instead")]
    pub fn mach_absolute_time() -> u64;
    #[deprecated(since = "0.2.55", note = "Use the `mach2` crate instead")]
    #[allow(deprecated)]
    pub fn mach_timebase_info(info: *mut ::mach_timebase_info) -> ::c_int;
    pub fn mach_host_self() -> mach_port_t;
    pub fn mach_thread_self() -> mach_port_t;
    pub fn pthread_setname_np(name: *const ::c_char) -> ::c_int;
    pub fn pthread_getname_np(thread: ::pthread_t, name: *mut ::c_char, len: ::size_t) -> ::c_int;
    pub fn pthread_mach_thread_np(thread: ::pthread_t) -> ::mach_port_t;
    pub fn pthread_from_mach_thread_np(port: ::mach_port_t) -> ::pthread_t;
    pub fn pthread_create_from_mach_thread(
        thread: *mut ::pthread_t,
        attr: *const ::pthread_attr_t,
        f: extern "C" fn(*mut ::c_void) -> *mut ::c_void,
        value: *mut ::c_void,
    ) -> ::c_int;
    pub fn pthread_get_stackaddr_np(thread: ::pthread_t) -> *mut ::c_void;
    pub fn pthread_get_stacksize_np(thread: ::pthread_t) -> ::size_t;
    pub fn pthread_condattr_setpshared(attr: *mut pthread_condattr_t, pshared: ::c_int) -> ::c_int;
    pub fn pthread_condattr_getpshared(
        attr: *const pthread_condattr_t,
        pshared: *mut ::c_int,
    ) -> ::c_int;
    pub fn pthread_mutexattr_setpshared(
        attr: *mut pthread_mutexattr_t,
        pshared: ::c_int,
    ) -> ::c_int;
    pub fn pthread_mutexattr_getpshared(
        attr: *const pthread_mutexattr_t,
        pshared: *mut ::c_int,
    ) -> ::c_int;
    pub fn pthread_rwlockattr_getpshared(
        attr: *const pthread_rwlockattr_t,
        val: *mut ::c_int,
    ) -> ::c_int;
    pub fn pthread_rwlockattr_setpshared(attr: *mut pthread_rwlockattr_t, val: ::c_int) -> ::c_int;
    pub fn pthread_threadid_np(thread: ::pthread_t, thread_id: *mut u64) -> ::c_int;
    pub fn pthread_attr_set_qos_class_np(
        attr: *mut pthread_attr_t,
        class: qos_class_t,
        priority: ::c_int,
    ) -> ::c_int;
    pub fn pthread_attr_get_qos_class_np(
        attr: *mut pthread_attr_t,
        class: *mut qos_class_t,
        priority: *mut ::c_int,
    ) -> ::c_int;
    pub fn pthread_set_qos_class_self_np(class: qos_class_t, priority: ::c_int) -> ::c_int;
    pub fn pthread_get_qos_class_np(
        thread: ::pthread_t,
        class: *mut qos_class_t,
        priority: *mut ::c_int,
    ) -> ::c_int;
    pub fn pthread_attr_getschedparam(
        attr: *const ::pthread_attr_t,
        param: *mut sched_param,
    ) -> ::c_int;
    pub fn pthread_attr_setschedparam(
        attr: *mut ::pthread_attr_t,
        param: *const sched_param,
    ) -> ::c_int;
    pub fn pthread_getschedparam(
        thread: ::pthread_t,
        policy: *mut ::c_int,
        param: *mut sched_param,
    ) -> ::c_int;
    pub fn pthread_setschedparam(
        thread: ::pthread_t,
        policy: ::c_int,
        param: *const sched_param,
    ) -> ::c_int;

    // Available from Big Sur
    pub fn pthread_introspection_hook_install(
        hook: ::pthread_introspection_hook_t,
    ) -> ::pthread_introspection_hook_t;
    pub fn pthread_introspection_setspecific_np(
        thread: ::pthread_t,
        key: ::pthread_key_t,
        value: *const ::c_void,
    ) -> ::c_int;
    pub fn pthread_introspection_getspecific_np(
        thread: ::pthread_t,
        key: ::pthread_key_t,
    ) -> *mut ::c_void;
    pub fn pthread_jit_write_protect_np(enabled: ::c_int);
    pub fn pthread_jit_write_protect_supported_np() -> ::c_int;
    pub fn pthread_cpu_number_np(cpu_number_out: *mut ::size_t) -> ::c_int;

    pub fn thread_policy_set(
        thread: thread_t,
        flavor: thread_policy_flavor_t,
        policy_info: thread_policy_t,
        count: mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn thread_policy_get(
        thread: thread_t,
        flavor: thread_policy_flavor_t,
        policy_info: thread_policy_t,
        count: *mut mach_msg_type_number_t,
        get_default: *mut boolean_t,
    ) -> kern_return_t;
    pub fn thread_info(
        target_act: thread_inspect_t,
        flavor: thread_flavor_t,
        thread_info_out: thread_info_t,
        thread_info_outCnt: *mut mach_msg_type_number_t,
    ) -> kern_return_t;
    pub fn __error() -> *mut ::c_int;
    pub fn backtrace(buf: *mut *mut ::c_void, sz: ::c_int) -> ::c_int;
    #[cfg_attr(
        all(target_os = "macos", not(target_arch = "aarch64")),
        link_name = "statfs$INODE64"
    )]
    pub fn statfs(path: *const ::c_char, buf: *mut statfs) -> ::c_int;
    #[cfg_attr(
        all(target_os = "macos", not(target_arch = "aarch64")),
        link_name = "fstatfs$INODE64"
    )]
    pub fn fstatfs(fd: ::c_int, buf: *mut statfs) -> ::c_int;
    pub fn kevent(
        kq: ::c_int,
        changelist: *const ::kevent,
        nchanges: ::c_int,
        eventlist: *mut ::kevent,
        nevents: ::c_int,
        timeout: *const ::timespec,
    ) -> ::c_int;
    pub fn kevent64(
        kq: ::c_int,
        changelist: *const ::kevent64_s,
        nchanges: ::c_int,
        eventlist: *mut ::kevent64_s,
        nevents: ::c_int,
        flags: ::c_uint,
        timeout: *const ::timespec,
    ) -> ::c_int;
    pub fn mount(
        src: *const ::c_char,
        target: *const ::c_char,
        flags: ::c_int,
        data: *mut ::c_void,
    ) -> ::c_int;
    pub fn fmount(
        src: *const ::c_char,
        fd: ::c_int,
        flags: ::c_int,
        data: *mut ::c_void,
    ) -> ::c_int;
    pub fn ptrace(request: ::c_int, pid: ::pid_t, addr: *mut ::c_char, data: ::c_int) -> ::c_int;
    pub fn quotactl(
        special: *const ::c_char,
        cmd: ::c_int,
        id: ::c_int,
        data: *mut ::c_char,
    ) -> ::c_int;
    pub fn sethostname(name: *const ::c_char, len: ::c_int) -> ::c_int;
    pub fn sendfile(
        fd: ::c_int,
        s: ::c_int,
        offset: ::off_t,
        len: *mut ::off_t,
        hdtr: *mut ::sf_hdtr,
        flags: ::c_int,
    ) -> ::c_int;
    pub fn futimens(fd: ::c_int, times: *const ::timespec) -> ::c_int;
    pub fn utimensat(
        dirfd: ::c_int,
        path: *const ::c_char,
        times: *const ::timespec,
        flag: ::c_int,
    ) -> ::c_int;
    pub fn openpty(
        amaster: *mut ::c_int,
        aslave: *mut ::c_int,
        name: *mut ::c_char,
        termp: *mut termios,
        winp: *mut ::winsize,
    ) -> ::c_int;
    pub fn forkpty(
        amaster: *mut ::c_int,
        name: *mut ::c_char,
        termp: *mut termios,
        winp: *mut ::winsize,
    ) -> ::pid_t;
    pub fn login_tty(fd: ::c_int) -> ::c_int;
    pub fn duplocale(base: ::locale_t) -> ::locale_t;
    pub fn freelocale(loc: ::locale_t) -> ::c_int;
    pub fn localeconv_l(loc: ::locale_t) -> *mut lconv;
    pub fn newlocale(mask: ::c_int, locale: *const ::c_char, base: ::locale_t) -> ::locale_t;
    pub fn uselocale(loc: ::locale_t) -> ::locale_t;
    pub fn querylocale(mask: ::c_int, loc: ::locale_t) -> *const ::c_char;
    pub fn getpriority(which: ::c_int, who: ::id_t) -> ::c_int;
    pub fn setpriority(which: ::c_int, who: ::id_t, prio: ::c_int) -> ::c_int;
    pub fn getdomainname(name: *mut ::c_char, len: ::c_int) -> ::c_int;
    pub fn setdomainname(name: *const ::c_char, len: ::c_int) -> ::c_int;
    pub fn preadv(fd: ::c_int, iov: *const ::iovec, iovcnt: ::c_int, offset: ::off_t) -> ::ssize_t;
    pub fn pwritev(fd: ::c_int, iov: *const ::iovec, iovcnt: ::c_int, offset: ::off_t)
        -> ::ssize_t;
    pub fn getxattr(
        path: *const ::c_char,
        name: *const ::c_char,
        value: *mut ::c_void,
        size: ::size_t,
        position: u32,
        flags: ::c_int,
    ) -> ::ssize_t;
    pub fn fgetxattr(
        filedes: ::c_int,
        name: *const ::c_char,
        value: *mut ::c_void,
        size: ::size_t,
        position: u32,
        flags: ::c_int,
    ) -> ::ssize_t;
    pub fn setxattr(
        path: *const ::c_char,
        name: *const ::c_char,
        value: *const ::c_void,
        size: ::size_t,
        position: u32,
        flags: ::c_int,
    ) -> ::c_int;
    pub fn fsetxattr(
        filedes: ::c_int,
        name: *const ::c_char,
        value: *const ::c_void,
        size: ::size_t,
        position: u32,
        flags: ::c_int,
    ) -> ::c_int;
    pub fn listxattr(
        path: *const ::c_char,
        list: *mut ::c_char,
        size: ::size_t,
        flags: ::c_int,
    ) -> ::ssize_t;
    pub fn flistxattr(
        filedes: ::c_int,
        list: *mut ::c_char,
        size: ::size_t,
        flags: ::c_int,
    ) -> ::ssize_t;
    pub fn removexattr(path: *const ::c_char, name: *const ::c_char, flags: ::c_int) -> ::c_int;
    pub fn renamex_np(from: *const ::c_char, to: *const ::c_char, flags: ::c_uint) -> ::c_int;
    pub fn renameatx_np(
        fromfd: ::c_int,
        from: *const ::c_char,
        tofd: ::c_int,
        to: *const ::c_char,
        flags: ::c_uint,
    ) -> ::c_int;
    pub fn fremovexattr(filedes: ::c_int, name: *const ::c_char, flags: ::c_int) -> ::c_int;

    pub fn getgrouplist(
        name: *const ::c_char,
        basegid: ::c_int,
        groups: *mut ::c_int,
        ngroups: *mut ::c_int,
    ) -> ::c_int;
    pub fn initgroups(user: *const ::c_char, basegroup: ::c_int) -> ::c_int;

    #[cfg_attr(
        all(target_os = "macos", target_arch = "x86"),
        link_name = "waitid$UNIX2003"
    )]
    pub fn waitid(idtype: idtype_t, id: id_t, infop: *mut ::siginfo_t, options: ::c_int)
        -> ::c_int;
    pub fn brk(addr: *const ::c_void) -> *mut ::c_void;
    pub fn sbrk(increment: ::c_int) -> *mut ::c_void;
    pub fn settimeofday(tv: *const ::timeval, tz: *const ::timezone) -> ::c_int;
    #[deprecated(since = "0.2.55", note = "Use the `mach2` crate instead")]
    pub fn _dyld_image_count() -> u32;
    #[deprecated(since = "0.2.55", note = "Use the `mach2` crate instead")]
    #[allow(deprecated)]
    pub fn _dyld_get_image_header(image_index: u32) -> *const mach_header;
    #[deprecated(since = "0.2.55", note = "Use the `mach2` crate instead")]
    pub fn _dyld_get_image_vmaddr_slide(image_index: u32) -> ::intptr_t;
    #[deprecated(since = "0.2.55", note = "Use the `mach2` crate instead")]
    pub fn _dyld_get_image_name(image_index: u32) -> *const ::c_char;

    pub fn posix_spawn(
        pid: *mut ::pid_t,
        path: *const ::c_char,
        file_actions: *const ::posix_spawn_file_actions_t,
        attrp: *const ::posix_spawnattr_t,
        argv: *const *mut ::c_char,
        envp: *const *mut ::c_char,
    ) -> ::c_int;
    pub fn posix_spawnp(
        pid: *mut ::pid_t,
        file: *const ::c_char,
        file_actions: *const ::posix_spawn_file_actions_t,
        attrp: *const ::posix_spawnattr_t,
        argv: *const *mut ::c_char,
        envp: *const *mut ::c_char,
    ) -> ::c_int;
    pub fn posix_spawnattr_init(attr: *mut posix_spawnattr_t) -> ::c_int;
    pub fn posix_spawnattr_destroy(attr: *mut posix_spawnattr_t) -> ::c_int;
    pub fn posix_spawnattr_getsigdefault(
        attr: *const posix_spawnattr_t,
        default: *mut ::sigset_t,
    ) -> ::c_int;
    pub fn posix_spawnattr_setsigdefault(
        attr: *mut posix_spawnattr_t,
        default: *const ::sigset_t,
    ) -> ::c_int;
    pub fn posix_spawnattr_getsigmask(
        attr: *const posix_spawnattr_t,
        default: *mut ::sigset_t,
    ) -> ::c_int;
    pub fn posix_spawnattr_setsigmask(
        attr: *mut posix_spawnattr_t,
        default: *const ::sigset_t,
    ) -> ::c_int;
    pub fn posix_spawnattr_getflags(
        attr: *const posix_spawnattr_t,
        flags: *mut ::c_short,
    ) -> ::c_int;
    pub fn posix_spawnattr_setflags(attr: *mut posix_spawnattr_t, flags: ::c_short) -> ::c_int;
    pub fn posix_spawnattr_getpgroup(
        attr: *const posix_spawnattr_t,
        flags: *mut ::pid_t,
    ) -> ::c_int;
    pub fn posix_spawnattr_setpgroup(attr: *mut posix_spawnattr_t, flags: ::pid_t) -> ::c_int;
    pub fn posix_spawnattr_setarchpref_np(
        attr: *mut posix_spawnattr_t,
        count: ::size_t,
        pref: *mut ::cpu_type_t,
        subpref: *mut ::cpu_subtype_t,
        ocount: *mut ::size_t,
    ) -> ::c_int;
    pub fn posix_spawnattr_getarchpref_np(
        attr: *const posix_spawnattr_t,
        count: ::size_t,
        pref: *mut ::cpu_type_t,
        subpref: *mut ::cpu_subtype_t,
        ocount: *mut ::size_t,
    ) -> ::c_int;

    pub fn posix_spawn_file_actions_init(actions: *mut posix_spawn_file_actions_t) -> ::c_int;
    pub fn posix_spawn_file_actions_destroy(actions: *mut posix_spawn_file_actions_t) -> ::c_int;
    pub fn posix_spawn_file_actions_addopen(
        actions: *mut posix_spawn_file_actions_t,
        fd: ::c_int,
        path: *const ::c_char,
        oflag: ::c_int,
        mode: ::mode_t,
    ) -> ::c_int;
    pub fn posix_spawn_file_actions_addclose(
        actions: *mut posix_spawn_file_actions_t,
        fd: ::c_int,
    ) -> ::c_int;
    pub fn posix_spawn_file_actions_adddup2(
        actions: *mut posix_spawn_file_actions_t,
        fd: ::c_int,
        newfd: ::c_int,
    ) -> ::c_int;
    pub fn uname(buf: *mut ::utsname) -> ::c_int;

    pub fn connectx(
        socket: ::c_int,
        endpoints: *const sa_endpoints_t,
        associd: sae_associd_t,
        flags: ::c_uint,
        iov: *const ::iovec,
        iovcnt: ::c_uint,
        len: *mut ::size_t,
        connid: *mut sae_connid_t,
    ) -> ::c_int;
    pub fn disconnectx(socket: ::c_int, associd: sae_associd_t, connid: sae_connid_t) -> ::c_int;

    pub fn ntp_adjtime(buf: *mut timex) -> ::c_int;
    pub fn ntp_gettime(buf: *mut ntptimeval) -> ::c_int;

    #[cfg_attr(
        all(target_os = "macos", not(target_arch = "aarch64")),
        link_name = "getmntinfo$INODE64"
    )]
    pub fn getmntinfo(mntbufp: *mut *mut statfs, flags: ::c_int) -> ::c_int;
    #[cfg_attr(
        all(target_os = "macos", not(target_arch = "aarch64")),
        link_name = "getfsstat$INODE64"
    )]
    pub fn getfsstat(mntbufp: *mut statfs, bufsize: ::c_int, flags: ::c_int) -> ::c_int;

    // Copy-on-write functions.
    // According to the man page `flags` is an `int` but in the header
    // this is a `uint32_t`.
    pub fn clonefile(src: *const ::c_char, dst: *const ::c_char, flags: u32) -> ::c_int;
    pub fn clonefileat(
        src_dirfd: ::c_int,
        src: *const ::c_char,
        dst_dirfd: ::c_int,
        dst: *const ::c_char,
        flags: u32,
    ) -> ::c_int;
    pub fn fclonefileat(
        srcfd: ::c_int,
        dst_dirfd: ::c_int,
        dst: *const ::c_char,
        flags: u32,
    ) -> ::c_int;

    // Added in macOS 10.13
    // ISO/IEC 9899:2011 ("ISO C11") K.3.7.4.1
    pub fn memset_s(s: *mut ::c_void, smax: ::size_t, c: ::c_int, n: ::size_t) -> ::c_int;
    // Added in macOS 10.5
    pub fn memset_pattern4(b: *mut ::c_void, pattern4: *const ::c_void, len: ::size_t);
    pub fn memset_pattern8(b: *mut ::c_void, pattern8: *const ::c_void, len: ::size_t);
    pub fn memset_pattern16(b: *mut ::c_void, pattern16: *const ::c_void, len: ::size_t);

    // Inherited from BSD but available from Big Sur only
    pub fn strtonum(
        __numstr: *const ::c_char,
        __minval: ::c_longlong,
        __maxval: ::c_longlong,
        errstrp: *mut *const ::c_char,
    ) -> ::c_longlong;

    pub fn mstats() -> mstats;
    pub fn malloc_printf(format: *const ::c_char, ...);
    pub fn malloc_zone_check(zone: *mut ::malloc_zone_t) -> ::boolean_t;
    pub fn malloc_zone_print(zone: *mut ::malloc_zone_t, verbose: ::boolean_t);
    pub fn malloc_zone_statistics(zone: *mut ::malloc_zone_t, stats: *mut malloc_statistics_t);
    pub fn malloc_zone_log(zone: *mut ::malloc_zone_t, address: *mut ::c_void);
    pub fn malloc_zone_print_ptr_info(ptr: *mut ::c_void);
    pub fn malloc_default_zone() -> *mut ::malloc_zone_t;
    pub fn malloc_zone_from_ptr(ptr: *const ::c_void) -> *mut ::malloc_zone_t;
    pub fn malloc_zone_malloc(zone: *mut ::malloc_zone_t, size: ::size_t) -> *mut ::c_void;
    pub fn malloc_zone_valloc(zone: *mut ::malloc_zone_t, size: ::size_t) -> *mut ::c_void;
    pub fn malloc_zone_calloc(
        zone: *mut ::malloc_zone_t,
        num_items: ::size_t,
        size: ::size_t,
    ) -> *mut ::c_void;
    pub fn malloc_zone_realloc(
        zone: *mut ::malloc_zone_t,
        ptr: *mut ::c_void,
        size: ::size_t,
    ) -> *mut ::c_void;
    pub fn malloc_zone_free(zone: *mut ::malloc_zone_t, ptr: *mut ::c_void);

    pub fn proc_listpids(
        t: u32,
        typeinfo: u32,
        buffer: *mut ::c_void,
        buffersize: ::c_int,
    ) -> ::c_int;
    pub fn proc_listallpids(buffer: *mut ::c_void, buffersize: ::c_int) -> ::c_int;
    pub fn proc_listpgrppids(
        pgrpid: ::pid_t,
        buffer: *mut ::c_void,
        buffersize: ::c_int,
    ) -> ::c_int;
    pub fn proc_listchildpids(ppid: ::pid_t, buffer: *mut ::c_void, buffersize: ::c_int)
        -> ::c_int;
    pub fn proc_pidinfo(
        pid: ::c_int,
        flavor: ::c_int,
        arg: u64,
        buffer: *mut ::c_void,
        buffersize: ::c_int,
    ) -> ::c_int;
    pub fn proc_pidfdinfo(
        pid: ::c_int,
        fd: ::c_int,
        flavor: ::c_int,
        buffer: *mut ::c_void,
        buffersize: ::c_int,
    ) -> ::c_int;
    pub fn proc_pidfileportinfo(
        pid: ::c_int,
        fileport: u32,
        flavor: ::c_int,
        buffer: *mut ::c_void,
        buffersize: ::c_int,
    ) -> ::c_int;
    pub fn proc_pidpath(pid: ::c_int, buffer: *mut ::c_void, buffersize: u32) -> ::c_int;
    pub fn proc_name(pid: ::c_int, buffer: *mut ::c_void, buffersize: u32) -> ::c_int;
    pub fn proc_regionfilename(
        pid: ::c_int,
        address: u64,
        buffer: *mut ::c_void,
        buffersize: u32,
    ) -> ::c_int;
    pub fn proc_kmsgbuf(buffer: *mut ::c_void, buffersize: u32) -> ::c_int;
    pub fn proc_libversion(major: *mut ::c_int, mintor: *mut ::c_int) -> ::c_int;
    pub fn proc_pid_rusage(pid: ::c_int, flavor: ::c_int, buffer: *mut rusage_info_t) -> ::c_int;

    // Available from Big Sur
    pub fn proc_set_no_smt() -> ::c_int;
    pub fn proc_setthread_no_smt() -> ::c_int;
    pub fn proc_set_csm(flags: u32) -> ::c_int;
    pub fn proc_setthread_csm(flags: u32) -> ::c_int;
    /// # Notes
    ///
    /// `id` is of type [`uuid_t`].
    pub fn gethostuuid(id: *mut u8, timeout: *const ::timespec) -> ::c_int;

    pub fn gethostid() -> ::c_long;
    pub fn sethostid(hostid: ::c_long);

    pub fn CCRandomGenerateBytes(bytes: *mut ::c_void, size: ::size_t) -> ::CCRNGStatus;

    pub fn _NSGetExecutablePath(buf: *mut ::c_char, bufsize: *mut u32) -> ::c_int;
    pub fn _NSGetEnviron() -> *mut *mut *mut ::c_char;

    pub fn mach_vm_map(
        target_task: ::vm_map_t,
        address: *mut ::mach_vm_address_t,
        size: ::mach_vm_size_t,
        mask: ::mach_vm_offset_t,
        flags: ::c_int,
        object: ::mem_entry_name_port_t,
        offset: ::memory_object_offset_t,
        copy: ::boolean_t,
        cur_protection: ::vm_prot_t,
        max_protection: ::vm_prot_t,
        inheritance: ::vm_inherit_t,
    ) -> ::kern_return_t;

    pub fn vm_deallocate(
        target_task: vm_map_t,
        address: vm_address_t,
        size: vm_size_t,
    ) -> ::kern_return_t;

    pub fn host_statistics64(
        host_priv: host_t,
        flavor: host_flavor_t,
        host_info64_out: host_info64_t,
        host_info64_outCnt: *mut mach_msg_type_number_t,
    ) -> ::kern_return_t;
    pub fn host_processor_info(
        host: host_t,
        flavor: processor_flavor_t,
        out_processor_count: *mut natural_t,
        out_processor_info: *mut processor_info_array_t,
        out_processor_infoCnt: *mut mach_msg_type_number_t,
    ) -> ::kern_return_t;

    pub static mut mach_task_self_: ::mach_port_t;
    pub fn task_for_pid(
        host: ::mach_port_t,
        pid: ::pid_t,
        task: *mut ::mach_port_t,
    ) -> ::kern_return_t;
    pub fn task_info(
        host: ::mach_port_t,
        flavor: task_flavor_t,
        task_info_out: task_info_t,
        task_info_count: *mut mach_msg_type_number_t,
    ) -> ::kern_return_t;
    pub fn host_statistics(
        host_priv: host_t,
        flavor: host_flavor_t,
        host_info_out: host_info_t,
        host_info_outCnt: *mut mach_msg_type_number_t,
    ) -> ::kern_return_t;

    // sysdir.h
    pub fn sysdir_start_search_path_enumeration(
        dir: sysdir_search_path_directory_t,
        domainMask: sysdir_search_path_domain_mask_t,
    ) -> ::sysdir_search_path_enumeration_state;
    pub fn sysdir_get_next_search_path_enumeration(
        state: ::sysdir_search_path_enumeration_state,
        path: *mut ::c_char,
    ) -> ::sysdir_search_path_enumeration_state;

    pub static vm_page_size: vm_size_t;
}

pub unsafe fn mach_task_self() -> ::mach_port_t {
    mach_task_self_
}

cfg_if! {
    if #[cfg(target_os = "macos")] {
        extern "C" {
            pub fn clock_settime(clock_id: ::clockid_t, tp: *const ::timespec) -> ::c_int;
            pub fn memmem(
                haystack: *const ::c_void,
                haystacklen: ::size_t,
                needle: *const ::c_void,
                needlelen: ::size_t,
            ) -> *mut ::c_void;
        }
    }
}

#[link(name = "iconv")]
extern "C" {
    pub fn iconv_open(tocode: *const ::c_char, fromcode: *const ::c_char) -> iconv_t;
    pub fn iconv(
        cd: iconv_t,
        inbuf: *mut *mut ::c_char,
        inbytesleft: *mut ::size_t,
        outbuf: *mut *mut ::c_char,
        outbytesleft: *mut ::size_t,
    ) -> ::size_t;
    pub fn iconv_close(cd: iconv_t) -> ::c_int;
}

cfg_if! {
    if #[cfg(any(target_arch = "arm", target_arch = "x86"))] {
        mod b32;
        pub use self::b32::*;
    } else if #[cfg(any(target_arch = "x86_64", target_arch = "aarch64"))] {
        mod b64;
        pub use self::b64::*;
    } else {
        // Unknown target_arch
    }
}
