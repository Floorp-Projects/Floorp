pub type fflags_t = u32;
pub type clock_t = i32;

pub type vm_prot_t = u_char;
pub type kvaddr_t = u64;
pub type segsz_t = isize;
pub type __fixpt_t = u32;
pub type fixpt_t = __fixpt_t;
pub type __lwpid_t = i32;
pub type lwpid_t = __lwpid_t;
pub type blksize_t = i32;
pub type clockid_t = ::c_int;
pub type sem_t = _sem;
pub type timer_t = *mut __c_anonymous__timer;

pub type fsblkcnt_t = u64;
pub type fsfilcnt_t = u64;
pub type idtype_t = ::c_uint;

pub type msglen_t = ::c_ulong;
pub type msgqnum_t = ::c_ulong;

pub type cpulevel_t = ::c_int;
pub type cpuwhich_t = ::c_int;

pub type mqd_t = *mut ::c_void;
pub type posix_spawnattr_t = *mut ::c_void;
pub type posix_spawn_file_actions_t = *mut ::c_void;

pub type pthread_spinlock_t = *mut __c_anonymous_pthread_spinlock;
pub type pthread_barrierattr_t = *mut __c_anonymous_pthread_barrierattr;
pub type pthread_barrier_t = *mut __c_anonymous_pthread_barrier;

pub type uuid_t = ::uuid;
pub type u_int = ::c_uint;
pub type u_char = ::c_uchar;
pub type u_long = ::c_ulong;
pub type u_short = ::c_ushort;

pub type caddr_t = *mut ::c_char;

pub type fhandle_t = fhandle;

// It's an alias over "struct __kvm_t". However, its fields aren't supposed to be used directly,
// making the type definition system dependent. Better not bind it exactly.
pub type kvm_t = ::c_void;

#[cfg_attr(feature = "extra_traits", derive(Debug, Hash))]
#[derive(PartialEq, Eq)]
#[repr(u32)]
pub enum devstat_support_flags {
    DEVSTAT_ALL_SUPPORTED = 0x00,
    DEVSTAT_NO_BLOCKSIZE = 0x01,
    DEVSTAT_NO_ORDERED_TAGS = 0x02,
    DEVSTAT_BS_UNAVAILABLE = 0x04,
}
impl ::Copy for devstat_support_flags {}
impl ::Clone for devstat_support_flags {
    fn clone(&self) -> devstat_support_flags {
        *self
    }
}

#[cfg_attr(feature = "extra_traits", derive(Debug, Hash))]
#[derive(PartialEq, Eq)]
#[repr(u32)]
pub enum devstat_trans_flags {
    DEVSTAT_NO_DATA = 0x00,
    DEVSTAT_READ = 0x01,
    DEVSTAT_WRITE = 0x02,
    DEVSTAT_FREE = 0x03,
}

impl ::Copy for devstat_trans_flags {}
impl ::Clone for devstat_trans_flags {
    fn clone(&self) -> devstat_trans_flags {
        *self
    }
}

#[cfg_attr(feature = "extra_traits", derive(Debug, Hash))]
#[derive(PartialEq, Eq)]
#[repr(u32)]
pub enum devstat_tag_type {
    DEVSTAT_TAG_SIMPLE = 0x00,
    DEVSTAT_TAG_HEAD = 0x01,
    DEVSTAT_TAG_ORDERED = 0x02,
    DEVSTAT_TAG_NONE = 0x03,
}
impl ::Copy for devstat_tag_type {}
impl ::Clone for devstat_tag_type {
    fn clone(&self) -> devstat_tag_type {
        *self
    }
}

#[cfg_attr(feature = "extra_traits", derive(Debug, Hash))]
#[derive(PartialEq, Eq)]
#[repr(u32)]
pub enum devstat_match_flags {
    DEVSTAT_MATCH_NONE = 0x00,
    DEVSTAT_MATCH_TYPE = 0x01,
    DEVSTAT_MATCH_IF = 0x02,
    DEVSTAT_MATCH_PASS = 0x04,
}
impl ::Copy for devstat_match_flags {}
impl ::Clone for devstat_match_flags {
    fn clone(&self) -> devstat_match_flags {
        *self
    }
}

#[cfg_attr(feature = "extra_traits", derive(Debug, Hash))]
#[derive(PartialEq, Eq)]
#[repr(u32)]
pub enum devstat_priority {
    DEVSTAT_PRIORITY_MIN = 0x000,
    DEVSTAT_PRIORITY_OTHER = 0x020,
    DEVSTAT_PRIORITY_PASS = 0x030,
    DEVSTAT_PRIORITY_FD = 0x040,
    DEVSTAT_PRIORITY_WFD = 0x050,
    DEVSTAT_PRIORITY_TAPE = 0x060,
    DEVSTAT_PRIORITY_CD = 0x090,
    DEVSTAT_PRIORITY_DISK = 0x110,
    DEVSTAT_PRIORITY_ARRAY = 0x120,
    DEVSTAT_PRIORITY_MAX = 0xfff,
}
impl ::Copy for devstat_priority {}
impl ::Clone for devstat_priority {
    fn clone(&self) -> devstat_priority {
        *self
    }
}

#[cfg_attr(feature = "extra_traits", derive(Debug, Hash))]
#[derive(PartialEq, Eq)]
#[repr(u32)]
pub enum devstat_type_flags {
    DEVSTAT_TYPE_DIRECT = 0x000,
    DEVSTAT_TYPE_SEQUENTIAL = 0x001,
    DEVSTAT_TYPE_PRINTER = 0x002,
    DEVSTAT_TYPE_PROCESSOR = 0x003,
    DEVSTAT_TYPE_WORM = 0x004,
    DEVSTAT_TYPE_CDROM = 0x005,
    DEVSTAT_TYPE_SCANNER = 0x006,
    DEVSTAT_TYPE_OPTICAL = 0x007,
    DEVSTAT_TYPE_CHANGER = 0x008,
    DEVSTAT_TYPE_COMM = 0x009,
    DEVSTAT_TYPE_ASC0 = 0x00a,
    DEVSTAT_TYPE_ASC1 = 0x00b,
    DEVSTAT_TYPE_STORARRAY = 0x00c,
    DEVSTAT_TYPE_ENCLOSURE = 0x00d,
    DEVSTAT_TYPE_FLOPPY = 0x00e,
    DEVSTAT_TYPE_MASK = 0x00f,
    DEVSTAT_TYPE_IF_SCSI = 0x010,
    DEVSTAT_TYPE_IF_IDE = 0x020,
    DEVSTAT_TYPE_IF_OTHER = 0x030,
    DEVSTAT_TYPE_IF_MASK = 0x0f0,
    DEVSTAT_TYPE_PASS = 0x100,
}
impl ::Copy for devstat_type_flags {}
impl ::Clone for devstat_type_flags {
    fn clone(&self) -> devstat_type_flags {
        *self
    }
}

#[cfg_attr(feature = "extra_traits", derive(Debug, Hash))]
#[derive(PartialEq, Eq)]
#[repr(u32)]
pub enum devstat_metric {
    DSM_NONE,
    DSM_TOTAL_BYTES,
    DSM_TOTAL_BYTES_READ,
    DSM_TOTAL_BYTES_WRITE,
    DSM_TOTAL_TRANSFERS,
    DSM_TOTAL_TRANSFERS_READ,
    DSM_TOTAL_TRANSFERS_WRITE,
    DSM_TOTAL_TRANSFERS_OTHER,
    DSM_TOTAL_BLOCKS,
    DSM_TOTAL_BLOCKS_READ,
    DSM_TOTAL_BLOCKS_WRITE,
    DSM_KB_PER_TRANSFER,
    DSM_KB_PER_TRANSFER_READ,
    DSM_KB_PER_TRANSFER_WRITE,
    DSM_TRANSFERS_PER_SECOND,
    DSM_TRANSFERS_PER_SECOND_READ,
    DSM_TRANSFERS_PER_SECOND_WRITE,
    DSM_TRANSFERS_PER_SECOND_OTHER,
    DSM_MB_PER_SECOND,
    DSM_MB_PER_SECOND_READ,
    DSM_MB_PER_SECOND_WRITE,
    DSM_BLOCKS_PER_SECOND,
    DSM_BLOCKS_PER_SECOND_READ,
    DSM_BLOCKS_PER_SECOND_WRITE,
    DSM_MS_PER_TRANSACTION,
    DSM_MS_PER_TRANSACTION_READ,
    DSM_MS_PER_TRANSACTION_WRITE,
    DSM_SKIP,
    DSM_TOTAL_BYTES_FREE,
    DSM_TOTAL_TRANSFERS_FREE,
    DSM_TOTAL_BLOCKS_FREE,
    DSM_KB_PER_TRANSFER_FREE,
    DSM_MB_PER_SECOND_FREE,
    DSM_TRANSFERS_PER_SECOND_FREE,
    DSM_BLOCKS_PER_SECOND_FREE,
    DSM_MS_PER_TRANSACTION_OTHER,
    DSM_MS_PER_TRANSACTION_FREE,
    DSM_BUSY_PCT,
    DSM_QUEUE_LENGTH,
    DSM_TOTAL_DURATION,
    DSM_TOTAL_DURATION_READ,
    DSM_TOTAL_DURATION_WRITE,
    DSM_TOTAL_DURATION_FREE,
    DSM_TOTAL_DURATION_OTHER,
    DSM_TOTAL_BUSY_TIME,
    DSM_MAX,
}
impl ::Copy for devstat_metric {}
impl ::Clone for devstat_metric {
    fn clone(&self) -> devstat_metric {
        *self
    }
}

#[cfg_attr(feature = "extra_traits", derive(Debug, Hash))]
#[derive(PartialEq, Eq)]
#[repr(u32)]
pub enum devstat_select_mode {
    DS_SELECT_ADD,
    DS_SELECT_ONLY,
    DS_SELECT_REMOVE,
    DS_SELECT_ADDONLY,
}
impl ::Copy for devstat_select_mode {}
impl ::Clone for devstat_select_mode {
    fn clone(&self) -> devstat_select_mode {
        *self
    }
}

s! {
    pub struct aiocb {
        pub aio_fildes: ::c_int,
        pub aio_offset: ::off_t,
        pub aio_buf: *mut ::c_void,
        pub aio_nbytes: ::size_t,
        __unused1: [::c_int; 2],
        __unused2: *mut ::c_void,
        pub aio_lio_opcode: ::c_int,
        pub aio_reqprio: ::c_int,
        // unused 3 through 5 are the __aiocb_private structure
        __unused3: ::c_long,
        __unused4: ::c_long,
        __unused5: *mut ::c_void,
        pub aio_sigevent: sigevent
    }

    pub struct jail {
        pub version: u32,
        pub path: *mut ::c_char,
        pub hostname: *mut ::c_char,
        pub jailname: *mut ::c_char,
        pub ip4s: ::c_uint,
        pub ip6s: ::c_uint,
        pub ip4: *mut ::in_addr,
        pub ip6: *mut ::in6_addr,
    }

    pub struct statvfs {
        pub f_bavail: ::fsblkcnt_t,
        pub f_bfree: ::fsblkcnt_t,
        pub f_blocks: ::fsblkcnt_t,
        pub f_favail: ::fsfilcnt_t,
        pub f_ffree: ::fsfilcnt_t,
        pub f_files: ::fsfilcnt_t,
        pub f_bsize: ::c_ulong,
        pub f_flag: ::c_ulong,
        pub f_frsize: ::c_ulong,
        pub f_fsid: ::c_ulong,
        pub f_namemax: ::c_ulong,
    }

    // internal structure has changed over time
    pub struct _sem {
        data: [u32; 4],
    }
    pub struct sembuf {
        pub sem_num: ::c_ushort,
        pub sem_op: ::c_short,
        pub sem_flg: ::c_short,
    }

    pub struct msqid_ds {
        pub msg_perm: ::ipc_perm,
        __unused1: *mut ::c_void,
        __unused2: *mut ::c_void,
        pub msg_cbytes: ::msglen_t,
        pub msg_qnum: ::msgqnum_t,
        pub msg_qbytes: ::msglen_t,
        pub msg_lspid: ::pid_t,
        pub msg_lrpid: ::pid_t,
        pub msg_stime: ::time_t,
        pub msg_rtime: ::time_t,
        pub msg_ctime: ::time_t,
    }

    pub struct stack_t {
        pub ss_sp: *mut ::c_void,
        pub ss_size: ::size_t,
        pub ss_flags: ::c_int,
    }

    pub struct mmsghdr {
        pub msg_hdr: ::msghdr,
        pub msg_len: ::ssize_t,
    }

    pub struct sockcred {
        pub sc_uid: ::uid_t,
        pub sc_euid: ::uid_t,
        pub sc_gid: ::gid_t,
        pub sc_egid: ::gid_t,
        pub sc_ngroups: ::c_int,
        pub sc_groups: [::gid_t; 1],
    }

    pub struct accept_filter_arg {
        pub af_name: [::c_char; 16],
        af_arg: [[::c_char; 10]; 24],
    }

    pub struct ptrace_vm_entry {
        pub pve_entry: ::c_int,
        pub pve_timestamp: ::c_int,
        pub pve_start: ::c_ulong,
        pub pve_end: ::c_ulong,
        pub pve_offset: ::c_ulong,
        pub pve_prot: ::c_uint,
        pub pve_pathlen: ::c_uint,
        pub pve_fileid: ::c_long,
        pub pve_fsid: u32,
        pub pve_path: *mut ::c_char,
    }

    pub struct ptrace_lwpinfo {
        pub pl_lwpid: lwpid_t,
        pub pl_event: ::c_int,
        pub pl_flags: ::c_int,
        pub pl_sigmask: ::sigset_t,
        pub pl_siglist: ::sigset_t,
        pub pl_siginfo: ::siginfo_t,
        pub pl_tdname: [::c_char; ::MAXCOMLEN as usize + 1],
        pub pl_child_pid: ::pid_t,
        pub pl_syscall_code: ::c_uint,
        pub pl_syscall_narg: ::c_uint,
    }

    pub struct ptrace_sc_ret {
        pub sr_retval: [::register_t; 2],
        pub sr_error: ::c_int,
    }

    pub struct ptrace_coredump {
        pub pc_fd: ::c_int,
        pub pc_flags: u32,
        pub pc_limit: ::off_t,
    }

    pub struct cpuset_t {
        #[cfg(target_pointer_width = "64")]
        __bits: [::c_long; 4],
        #[cfg(target_pointer_width = "32")]
        __bits: [::c_long; 8],
    }

    pub struct cap_rights_t {
        cr_rights: [u64; 2],
    }

    pub struct umutex {
        m_owner: ::lwpid_t,
        m_flags: u32,
        m_ceilings: [u32; 2],
        m_rb_link: ::uintptr_t,
        #[cfg(target_pointer_width = "32")]
        m_pad: u32,
        m_spare: [u32; 2],

    }

    pub struct ucond {
        c_has_waiters: u32,
        c_flags: u32,
        c_clockid: u32,
        c_spare: [u32; 1],
    }

    pub struct uuid {
        pub time_low: u32,
        pub time_mid: u16,
        pub time_hi_and_version: u16,
        pub clock_seq_hi_and_reserved: u8,
        pub clock_seq_low: u8,
        pub node: [u8; _UUID_NODE_LEN],
    }

    pub struct __c_anonymous_pthread_spinlock {
        s_clock: umutex,
    }

    pub struct __c_anonymous_pthread_barrierattr {
        pshared: ::c_int,
    }

    pub struct __c_anonymous_pthread_barrier {
        b_lock: umutex,
        b_cv: ucond,
        b_cycle: i64,
        b_count: ::c_int,
        b_waiters: ::c_int,
        b_refcount: ::c_int,
        b_destroying: ::c_int,
    }

    pub struct kinfo_vmentry {
        pub kve_structsize: ::c_int,
        pub kve_type: ::c_int,
        pub kve_start: u64,
        pub kve_end: u64,
        pub kve_offset: u64,
        pub kve_vn_fileid: u64,
        #[cfg(not(freebsd11))]
        pub kve_vn_fsid_freebsd11: u32,
        #[cfg(freebsd11)]
        pub kve_vn_fsid: u32,
        pub kve_flags: ::c_int,
        pub kve_resident: ::c_int,
        pub kve_private_resident: ::c_int,
        pub kve_protection: ::c_int,
        pub kve_ref_count: ::c_int,
        pub kve_shadow_count: ::c_int,
        pub kve_vn_type: ::c_int,
        pub kve_vn_size: u64,
        #[cfg(not(freebsd11))]
        pub kve_vn_rdev_freebsd11: u32,
        #[cfg(freebsd11)]
        pub kve_vn_rdev: u32,
        pub kve_vn_mode: u16,
        pub kve_status: u16,
        #[cfg(not(freebsd11))]
        pub kve_vn_fsid: u64,
        #[cfg(not(freebsd11))]
        pub kve_vn_rdev: u64,
        #[cfg(not(freebsd11))]
        _kve_is_spare: [::c_int; 8],
        #[cfg(freebsd11)]
        _kve_is_spare: [::c_int; 12],
        pub kve_path: [[::c_char; 32]; 32],
    }

    pub struct __c_anonymous_filestat {
        pub stqe_next: *mut filestat,
    }

    pub struct filestat {
        pub fs_type: ::c_int,
        pub fs_flags: ::c_int,
        pub fs_fflags: ::c_int,
        pub fs_uflags: ::c_int,
        pub fs_fd: ::c_int,
        pub fs_ref_count: ::c_int,
        pub fs_offset: ::off_t,
        pub fs_typedep: *mut ::c_void,
        pub fs_path: *mut ::c_char,
        pub next: __c_anonymous_filestat,
        pub fs_cap_rights: cap_rights_t,
    }

    pub struct filestat_list {
        pub stqh_first: *mut filestat,
        pub stqh_last: *mut *mut filestat,
    }

    pub struct procstat {
        pub tpe: ::c_int,
        pub kd: ::uintptr_t,
        pub vmentries: *mut ::c_void,
        pub files: *mut ::c_void,
        pub argv: *mut ::c_void,
        pub envv: *mut ::c_void,
        pub core: ::uintptr_t,
    }

    pub struct itimerspec {
        pub it_interval: ::timespec,
        pub it_value: ::timespec,
    }

    pub struct __c_anonymous__timer {
        _priv: [::c_int; 3],
    }

    /// Used to hold a copy of the command line, if it had a sane length.
    pub struct pargs {
        /// Reference count.
        pub ar_ref: u_int,
        /// Length.
        pub ar_length: u_int,
        /// Arguments.
        pub ar_args: [::c_uchar; 1],
    }

    pub struct priority {
        /// Scheduling class.
        pub pri_class: u_char,
        /// Normal priority level.
        pub pri_level: u_char,
        /// Priority before propagation.
        pub pri_native: u_char,
        /// User priority based on p_cpu and p_nice.
        pub pri_user: u_char,
    }

    pub struct kvm_swap {
        pub ksw_devname: [::c_char; 32],
        pub ksw_used: u_int,
        pub ksw_total: u_int,
        pub ksw_flags: ::c_int,
        pub ksw_reserved1: u_int,
        pub ksw_reserved2: u_int,
    }

    pub struct nlist {
        /// symbol name (in memory)
        pub n_name: *const ::c_char,
        /// type defines
        pub n_type: ::c_uchar,
        /// "type" and binding information
        pub n_other: ::c_char,
        /// used by stab entries
        pub n_desc: ::c_short,
        pub n_value: ::c_ulong,
    }

    pub struct kvm_nlist {
        pub n_name: *const ::c_char,
        pub n_type: ::c_uchar,
        pub n_value: ::kvaddr_t,
    }

    pub struct __c_anonymous_sem {
        _priv: ::uintptr_t,
    }

    pub struct semid_ds {
        pub sem_perm: ::ipc_perm,
        pub __sem_base: *mut __c_anonymous_sem,
        pub sem_nsems: ::c_ushort,
        pub sem_otime: ::time_t,
        pub sem_ctime: ::time_t,
    }

    pub struct vmtotal {
        pub t_vm: u64,
        pub t_avm: u64,
        pub t_rm: u64,
        pub t_arm: u64,
        pub t_vmshr: u64,
        pub t_avmshr: u64,
        pub t_rmshr: u64,
        pub t_armshr: u64,
        pub t_free: u64,
        pub t_rq: i16,
        pub t_dw: i16,
        pub t_pw: i16,
        pub t_sl: i16,
        pub t_sw: i16,
        pub t_pad: [u16; 3],
    }

    pub struct sockstat {
        pub inp_ppcb: u64,
        pub so_addr: u64,
        pub so_pcb: u64,
        pub unp_conn: u64,
        pub dom_family: ::c_int,
        pub proto: ::c_int,
        pub so_rcv_sb_state: ::c_int,
        pub so_snd_sb_state: ::c_int,
        /// Socket address.
        pub sa_local: ::sockaddr_storage,
        /// Peer address.
        pub sa_peer: ::sockaddr_storage,
        pub type_: ::c_int,
        pub dname: [::c_char; 32],
        #[cfg(any(freebsd12, freebsd13, freebsd14))]
        pub sendq: ::c_uint,
        #[cfg(any(freebsd12, freebsd13, freebsd14))]
        pub recvq: ::c_uint,
    }

    pub struct shmstat {
        pub size: u64,
        pub mode: u16,
    }

    pub struct rusage_ext {
        pub rux_runtime: u64,
        pub rux_uticks: u64,
        pub rux_sticks: u64,
        pub rux_iticks: u64,
        pub rux_uu: u64,
        pub rux_su: u64,
        pub rux_tu: u64,
    }

    pub struct if_clonereq {
        pub ifcr_total: ::c_int,
        pub ifcr_count: ::c_int,
        pub ifcr_buffer: *mut ::c_char,
    }

    pub struct if_msghdr {
        /// to skip over non-understood messages
        pub ifm_msglen: ::c_ushort,
        /// future binary compatibility
        pub ifm_version: ::c_uchar,
        /// message type
        pub ifm_type: ::c_uchar,
        /// like rtm_addrs
        pub ifm_addrs: ::c_int,
        /// value of if_flags
        pub ifm_flags: ::c_int,
        /// index for associated ifp
        pub ifm_index: ::c_ushort,
        pub _ifm_spare1: ::c_ushort,
        /// statistics and other data about if
        pub ifm_data: if_data,
    }

    pub struct if_msghdrl {
        /// to skip over non-understood messages
        pub ifm_msglen: ::c_ushort,
        /// future binary compatibility
        pub ifm_version: ::c_uchar,
        /// message type
        pub ifm_type: ::c_uchar,
        /// like rtm_addrs
        pub ifm_addrs: ::c_int,
        /// value of if_flags
        pub ifm_flags: ::c_int,
        /// index for associated ifp
        pub ifm_index: ::c_ushort,
        /// spare space to grow if_index, see if_var.h
        pub _ifm_spare1: ::c_ushort,
        /// length of if_msghdrl incl. if_data
        pub ifm_len: ::c_ushort,
        /// offset of if_data from beginning
        pub ifm_data_off: ::c_ushort,
        pub _ifm_spare2: ::c_int,
        /// statistics and other data about if
        pub ifm_data: if_data,
    }

    pub struct ifa_msghdr {
        /// to skip over non-understood messages
        pub ifam_msglen: ::c_ushort,
        /// future binary compatibility
        pub ifam_version: ::c_uchar,
        /// message type
        pub ifam_type: ::c_uchar,
        /// like rtm_addrs
        pub ifam_addrs: ::c_int,
        /// value of ifa_flags
        pub ifam_flags: ::c_int,
        /// index for associated ifp
        pub ifam_index: ::c_ushort,
        pub _ifam_spare1: ::c_ushort,
        /// value of ifa_ifp->if_metric
        pub ifam_metric: ::c_int,
    }

    pub struct ifa_msghdrl {
        /// to skip over non-understood messages
        pub ifam_msglen: ::c_ushort,
        /// future binary compatibility
        pub ifam_version: ::c_uchar,
        /// message type
        pub ifam_type: ::c_uchar,
        /// like rtm_addrs
        pub ifam_addrs: ::c_int,
        /// value of ifa_flags
        pub ifam_flags: ::c_int,
        /// index for associated ifp
        pub ifam_index: ::c_ushort,
        /// spare space to grow if_index, see if_var.h
        pub _ifam_spare1: ::c_ushort,
        /// length of ifa_msghdrl incl. if_data
        pub ifam_len: ::c_ushort,
        /// offset of if_data from beginning
        pub ifam_data_off: ::c_ushort,
        /// value of ifa_ifp->if_metric
        pub ifam_metric: ::c_int,
        /// statistics and other data about if or address
        pub ifam_data: if_data,
    }

    pub struct ifma_msghdr {
        /// to skip over non-understood messages
        pub ifmam_msglen: ::c_ushort,
        /// future binary compatibility
        pub ifmam_version: ::c_uchar,
        /// message type
        pub ifmam_type: ::c_uchar,
        /// like rtm_addrs
        pub ifmam_addrs: ::c_int,
        /// value of ifa_flags
        pub ifmam_flags: ::c_int,
        /// index for associated ifp
        pub ifmam_index: ::c_ushort,
        pub _ifmam_spare1: ::c_ushort,
    }

    pub struct if_announcemsghdr {
        /// to skip over non-understood messages
        pub ifan_msglen: ::c_ushort,
        /// future binary compatibility
        pub ifan_version: ::c_uchar,
        /// message type
        pub ifan_type: ::c_uchar,
        /// index for associated ifp
        pub ifan_index: ::c_ushort,
        /// if name, e.g. "en0"
        pub ifan_name: [::c_char; ::IFNAMSIZ as usize],
        /// what type of announcement
        pub ifan_what: ::c_ushort,
    }

    pub struct ifreq_buffer {
        pub length: ::size_t,
        pub buffer: *mut ::c_void,
    }

    pub struct ifaliasreq {
        /// if name, e.g. "en0"
        pub ifra_name: [::c_char; ::IFNAMSIZ as usize],
        pub ifra_addr: ::sockaddr,
        pub ifra_broadaddr: ::sockaddr,
        pub ifra_mask: ::sockaddr,
        pub ifra_vhid: ::c_int,
    }

    /// 9.x compat
    pub struct oifaliasreq {
        /// if name, e.g. "en0"
        pub ifra_name: [::c_char; ::IFNAMSIZ as usize],
        pub ifra_addr: ::sockaddr,
        pub ifra_broadaddr: ::sockaddr,
        pub ifra_mask: ::sockaddr,
    }

    pub struct ifmediareq {
        /// if name, e.g. "en0"
        pub ifm_name: [::c_char; ::IFNAMSIZ as usize],
        /// current media options
        pub ifm_current: ::c_int,
        /// don't care mask
        pub ifm_mask: ::c_int,
        /// media status
        pub ifm_status: ::c_int,
        /// active options
        pub ifm_active: ::c_int,
        /// # entries in ifm_ulist array
        pub ifm_count: ::c_int,
        /// media words
        pub ifm_ulist: *mut ::c_int,
    }

    pub struct ifdrv {
        /// if name, e.g. "en0"
        pub ifd_name: [::c_char; ::IFNAMSIZ as usize],
        pub ifd_cmd: ::c_ulong,
        pub ifd_len: ::size_t,
        pub ifd_data: *mut ::c_void,
    }

    pub struct ifi2creq {
        /// i2c address (0xA0, 0xA2)
        pub dev_addr: u8,
        /// read offset
        pub offset: u8,
        /// read length
        pub len: u8,
        pub spare0: u8,
        pub spare1: u32,
        /// read buffer
        pub data: [u8; 8],
    }

    pub struct ifrsshash {
        /// if name, e.g. "en0"
        pub ifrh_name: [::c_char; ::IFNAMSIZ as usize],
        /// RSS_FUNC_
        pub ifrh_func: u8,
        pub ifrh_spare0: u8,
        pub ifrh_spare1: u16,
        /// RSS_TYPE_
        pub ifrh_types: u32,
    }

    pub struct ifmibdata {
        /// name of interface
        pub ifmd_name: [::c_char; ::IFNAMSIZ as usize],
        /// number of promiscuous listeners
        pub ifmd_pcount: ::c_int,
        /// interface flags
        pub ifmd_flags: ::c_int,
        /// instantaneous length of send queue
        pub ifmd_snd_len: ::c_int,
        /// maximum length of send queue
        pub ifmd_snd_maxlen: ::c_int,
        /// number of drops in send queue
        pub ifmd_snd_drops: ::c_int,
        /// for future expansion
        pub ifmd_filler: [::c_int; 4],
        /// generic information and statistics
        pub ifmd_data: if_data,
    }

    pub struct ifmib_iso_8802_3 {
        pub dot3StatsAlignmentErrors: u32,
        pub dot3StatsFCSErrors: u32,
        pub dot3StatsSingleCollisionFrames: u32,
        pub dot3StatsMultipleCollisionFrames: u32,
        pub dot3StatsSQETestErrors: u32,
        pub dot3StatsDeferredTransmissions: u32,
        pub dot3StatsLateCollisions: u32,
        pub dot3StatsExcessiveCollisions: u32,
        pub dot3StatsInternalMacTransmitErrors: u32,
        pub dot3StatsCarrierSenseErrors: u32,
        pub dot3StatsFrameTooLongs: u32,
        pub dot3StatsInternalMacReceiveErrors: u32,
        pub dot3StatsEtherChipSet: u32,
        pub dot3StatsMissedFrames: u32,
        pub dot3StatsCollFrequencies: [u32; 16],
        pub dot3Compliance: u32,
    }

    pub struct __c_anonymous_ph {
        pub ph1: u64,
        pub ph2: u64,
    }

    pub struct fid {
        pub fid_len: ::c_ushort,
        pub fid_data0: ::c_ushort,
        pub fid_data: [::c_char; ::MAXFIDSZ as usize],
    }

    pub struct fhandle {
        pub fh_fsid: ::fsid_t,
        pub fh_fid: fid,
    }

    pub struct bintime {
        pub sec: ::time_t,
        pub frac: u64,
    }

    pub struct clockinfo {
        /// clock frequency
        pub hz: ::c_int,
        /// micro-seconds per hz tick
        pub tick: ::c_int,
        pub spare: ::c_int,
        /// statistics clock frequency
        pub stathz: ::c_int,
        /// profiling clock frequency
        pub profhz: ::c_int,
    }

    pub struct __c_anonymous_stailq_entry_devstat {
        pub stqe_next: *mut devstat,
    }

    pub struct devstat {
        /// Update sequence
        pub sequence0: ::u_int,
        /// Allocated entry
        pub allocated: ::c_int,
        /// started ops
        pub start_count: ::u_int,
        /// completed ops
        pub end_count: ::u_int,
        /// busy time unaccounted for since this time
        pub busy_from: bintime,
        pub dev_links: __c_anonymous_stailq_entry_devstat,
        /// Devstat device number.
        pub device_number: u32,
        pub device_name: [::c_char; DEVSTAT_NAME_LEN as usize],
        pub unit_number: ::c_int,
        pub bytes: [u64; DEVSTAT_N_TRANS_FLAGS as usize],
        pub operations: [u64; DEVSTAT_N_TRANS_FLAGS as usize],
        pub duration: [bintime; DEVSTAT_N_TRANS_FLAGS as usize],
        pub busy_time: bintime,
        /// Time the device was created.
        pub creation_time: bintime,
        /// Block size, bytes
        pub block_size: u32,
        /// The number of simple, ordered, and head of queue tags sent.
        pub tag_types: [u64; 3],
        /// Which statistics are supported by a given device.
        pub flags: devstat_support_flags,
        /// Device type
        pub device_type: devstat_type_flags,
        /// Controls list pos.
        pub priority: devstat_priority,
        /// Identification for GEOM nodes
        pub id: *const ::c_void,
        /// Update sequence
        pub sequence1: ::u_int,
    }

    pub struct devstat_match {
        pub match_fields: devstat_match_flags,
        pub device_type: devstat_type_flags,
        pub num_match_categories: ::c_int,
    }

    pub struct devstat_match_table {
        pub match_str: *const ::c_char,
        pub type_: devstat_type_flags,
        pub match_field: devstat_match_flags,
    }

    pub struct device_selection {
        pub device_number: u32,
        pub device_name: [::c_char; DEVSTAT_NAME_LEN as usize],
        pub unit_number: ::c_int,
        pub selected: ::c_int,
        pub bytes: u64,
        pub position: ::c_int,
    }

    pub struct devinfo {
        pub devices: *mut devstat,
        pub mem_ptr: *mut u8,
        pub generation: ::c_long,
        pub numdevs: ::c_int,
    }

    pub struct sockcred2 {
        pub sc_version: ::c_int,
        pub sc_pid: ::pid_t,
        pub sc_uid: ::uid_t,
        pub sc_euid: ::uid_t,
        pub sc_gid: ::gid_t,
        pub sc_egid: ::gid_t,
        pub sc_ngroups: ::c_int,
        pub sc_groups: [::gid_t; 1],
    }
}

s_no_extra_traits! {
    pub struct utmpx {
        pub ut_type: ::c_short,
        pub ut_tv: ::timeval,
        pub ut_id: [::c_char; 8],
        pub ut_pid: ::pid_t,
        pub ut_user: [::c_char; 32],
        pub ut_line: [::c_char; 16],
        pub ut_host: [::c_char; 128],
        pub __ut_spare: [::c_char; 64],
    }

    #[cfg(libc_union)]
    pub union __c_anonymous_cr_pid {
        __cr_unused: *mut ::c_void,
        pub cr_pid: ::pid_t,
    }

    pub struct xucred {
        pub cr_version: ::c_uint,
        pub cr_uid: ::uid_t,
        pub cr_ngroups: ::c_short,
        pub cr_groups: [::gid_t; 16],
        #[cfg(libc_union)]
        pub cr_pid__c_anonymous_union: __c_anonymous_cr_pid,
        #[cfg(not(libc_union))]
        __cr_unused1: *mut ::c_void,
    }

    pub struct sockaddr_dl {
        pub sdl_len: ::c_uchar,
        pub sdl_family: ::c_uchar,
        pub sdl_index: ::c_ushort,
        pub sdl_type: ::c_uchar,
        pub sdl_nlen: ::c_uchar,
        pub sdl_alen: ::c_uchar,
        pub sdl_slen: ::c_uchar,
        pub sdl_data: [::c_char; 46],
    }

    pub struct mq_attr {
        pub mq_flags: ::c_long,
        pub mq_maxmsg: ::c_long,
        pub mq_msgsize: ::c_long,
        pub mq_curmsgs: ::c_long,
        __reserved: [::c_long; 4]
    }

    pub struct sigevent {
        pub sigev_notify: ::c_int,
        pub sigev_signo: ::c_int,
        pub sigev_value: ::sigval,
        //The rest of the structure is actually a union.  We expose only
        //sigev_notify_thread_id because it's the most useful union member.
        pub sigev_notify_thread_id: ::lwpid_t,
        #[cfg(target_pointer_width = "64")]
        __unused1: ::c_int,
        __unused2: [::c_long; 7]
    }

    pub struct ptsstat {
        #[cfg(any(freebsd12, freebsd13, freebsd14))]
        pub dev: u64,
        #[cfg(not(any(freebsd12, freebsd13, freebsd14)))]
        pub dev: u32,
        pub devname: [::c_char; SPECNAMELEN as usize + 1],
    }

    #[cfg(libc_union)]
    pub union __c_anonymous_elf32_auxv_union {
        pub a_val: ::c_int,
    }

    pub struct Elf32_Auxinfo {
        pub a_type: ::c_int,
        #[cfg(libc_union)]
        pub a_un: __c_anonymous_elf32_auxv_union,
    }

    #[cfg(libc_union)]
    pub union __c_anonymous_ifi_epoch {
        pub tt: ::time_t,
        pub ph: u64,
    }

    #[cfg(libc_union)]
    pub union __c_anonymous_ifi_lastchange {
        pub tv: ::timeval,
        pub ph: __c_anonymous_ph,
    }

    pub struct if_data {
        /// ethernet, tokenring, etc
        pub ifi_type: u8,
        /// e.g., AUI, Thinnet, 10base-T, etc
        pub ifi_physical: u8,
        /// media address length
        pub ifi_addrlen: u8,
        /// media header length
        pub ifi_hdrlen: u8,
        /// current link state
        pub ifi_link_state: u8,
        /// carp vhid
        pub ifi_vhid: u8,
        /// length of this data struct
        pub ifi_datalen: u16,
        /// maximum transmission unit
        pub ifi_mtu: u32,
        /// routing metric (external only)
        pub ifi_metric: u32,
        /// linespeed
        pub ifi_baudrate: u64,
        /// packets received on interface
        pub ifi_ipackets: u64,
        /// input errors on interface
        pub ifi_ierrors: u64,
        /// packets sent on interface
        pub ifi_opackets: u64,
        /// output errors on interface
        pub ifi_oerrors: u64,
        /// collisions on csma interfaces
        pub ifi_collisions: u64,
        /// total number of octets received
        pub ifi_ibytes: u64,
        /// total number of octets sent
        pub ifi_obytes: u64,
        /// packets received via multicast
        pub ifi_imcasts: u64,
        /// packets sent via multicast
        pub ifi_omcasts: u64,
        /// dropped on input
        pub ifi_iqdrops: u64,
        /// dropped on output
        pub ifi_oqdrops: u64,
        /// destined for unsupported protocol
        pub ifi_noproto: u64,
        /// HW offload capabilities, see IFCAP
        pub ifi_hwassist: u64,
        /// uptime at attach or stat reset
        #[cfg(libc_union)]
        pub __ifi_epoch: __c_anonymous_ifi_epoch,
        /// uptime at attach or stat reset
        #[cfg(not(libc_union))]
        pub __ifi_epoch: u64,
        /// time of last administrative change
        #[cfg(libc_union)]
        pub __ifi_lastchange: __c_anonymous_ifi_lastchange,
        /// time of last administrative change
        #[cfg(not(libc_union))]
        pub __ifi_lastchange: ::timeval,
    }

    #[cfg(libc_union)]
    pub union __c_anonymous_ifr_ifru {
        pub ifru_addr: ::sockaddr,
        pub ifru_dstaddr: ::sockaddr,
        pub ifru_broadaddr: ::sockaddr,
        pub ifru_buffer: ifreq_buffer,
        pub ifru_flags: [::c_short; 2],
        pub ifru_index: ::c_short,
        pub ifru_jid: ::c_int,
        pub ifru_metric: ::c_int,
        pub ifru_mtu: ::c_int,
        pub ifru_phys: ::c_int,
        pub ifru_media: ::c_int,
        pub ifru_data: ::caddr_t,
        pub ifru_cap: [::c_int; 2],
        pub ifru_fib: ::c_uint,
        pub ifru_vlan_pcp: ::c_uchar,
    }

    pub struct ifreq {
        /// if name, e.g. "en0"
        pub ifr_name: [::c_char; ::IFNAMSIZ],
        #[cfg(libc_union)]
        pub ifr_ifru: __c_anonymous_ifr_ifru,
        #[cfg(not(libc_union))]
        pub ifr_ifru: ::sockaddr,
    }

    pub struct ifstat {
        /// if name, e.g. "en0"
        pub ifs_name: [::c_char; ::IFNAMSIZ as usize],
        pub ascii: [::c_char; ::IFSTATMAX as usize + 1],
    }

    pub struct ifrsskey {
        /// if name, e.g. "en0"
        pub ifrk_name: [::c_char; ::IFNAMSIZ as usize],
        /// RSS_FUNC_
        pub ifrk_func: u8,
        pub ifrk_spare0: u8,
        pub ifrk_keylen: u16,
        pub ifrk_key: [u8; ::RSS_KEYLEN as usize],
    }

    pub struct ifdownreason {
        pub ifdr_name: [::c_char; ::IFNAMSIZ as usize],
        pub ifdr_reason: u32,
        pub ifdr_vendor: u32,
        pub ifdr_msg: [::c_char; ::IFDR_MSG_SIZE as usize],
    }
}

cfg_if! {
    if #[cfg(feature = "extra_traits")] {
        impl PartialEq for utmpx {
            fn eq(&self, other: &utmpx) -> bool {
                self.ut_type == other.ut_type
                    && self.ut_tv == other.ut_tv
                    && self.ut_id == other.ut_id
                    && self.ut_pid == other.ut_pid
                    && self.ut_user == other.ut_user
                    && self.ut_line == other.ut_line
                    && self
                    .ut_host
                    .iter()
                    .zip(other.ut_host.iter())
                    .all(|(a,b)| a == b)
                    && self
                    .__ut_spare
                    .iter()
                    .zip(other.__ut_spare.iter())
                    .all(|(a,b)| a == b)
            }
        }
        impl Eq for utmpx {}
        impl ::fmt::Debug for utmpx {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("utmpx")
                    .field("ut_type", &self.ut_type)
                    .field("ut_tv", &self.ut_tv)
                    .field("ut_id", &self.ut_id)
                    .field("ut_pid", &self.ut_pid)
                    .field("ut_user", &self.ut_user)
                    .field("ut_line", &self.ut_line)
                    // FIXME: .field("ut_host", &self.ut_host)
                    // FIXME: .field("__ut_spare", &self.__ut_spare)
                    .finish()
            }
        }
        impl ::hash::Hash for utmpx {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.ut_type.hash(state);
                self.ut_tv.hash(state);
                self.ut_id.hash(state);
                self.ut_pid.hash(state);
                self.ut_user.hash(state);
                self.ut_line.hash(state);
                self.ut_host.hash(state);
                self.__ut_spare.hash(state);
            }
        }

        #[cfg(libc_union)]
        impl PartialEq for __c_anonymous_cr_pid {
            fn eq(&self, other: &__c_anonymous_cr_pid) -> bool {
                unsafe { self.cr_pid == other.cr_pid}
            }
        }
        #[cfg(libc_union)]
        impl Eq for __c_anonymous_cr_pid {}
        #[cfg(libc_union)]
        impl ::fmt::Debug for __c_anonymous_cr_pid {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("cr_pid")
                    .field("cr_pid", unsafe { &self.cr_pid })
                    .finish()
            }
        }
        #[cfg(libc_union)]
        impl ::hash::Hash for __c_anonymous_cr_pid {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                unsafe { self.cr_pid.hash(state) };
            }
        }

        impl PartialEq for xucred {
            fn eq(&self, other: &xucred) -> bool {
                #[cfg(libc_union)]
                let equal_cr_pid = self.cr_pid__c_anonymous_union
                    == other.cr_pid__c_anonymous_union;
                #[cfg(not(libc_union))]
                let equal_cr_pid = self.__cr_unused1 == other.__cr_unused1;

                self.cr_version == other.cr_version
                    && self.cr_uid == other.cr_uid
                    && self.cr_ngroups == other.cr_ngroups
                    && self.cr_groups == other.cr_groups
                    && equal_cr_pid
            }
        }
        impl Eq for xucred {}
        impl ::fmt::Debug for xucred {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                let mut struct_formatter = f.debug_struct("xucred");
                struct_formatter.field("cr_version", &self.cr_version);
                struct_formatter.field("cr_uid", &self.cr_uid);
                struct_formatter.field("cr_ngroups", &self.cr_ngroups);
                struct_formatter.field("cr_groups", &self.cr_groups);
                #[cfg(libc_union)]
                struct_formatter.field(
                    "cr_pid__c_anonymous_union",
                    &self.cr_pid__c_anonymous_union
                );
                struct_formatter.finish()
            }
        }
        impl ::hash::Hash for xucred {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.cr_version.hash(state);
                self.cr_uid.hash(state);
                self.cr_ngroups.hash(state);
                self.cr_groups.hash(state);
                #[cfg(libc_union)]
                self.cr_pid__c_anonymous_union.hash(state);
                #[cfg(not(libc_union))]
                self.__cr_unused1.hash(state);
            }
        }

        impl PartialEq for sockaddr_dl {
            fn eq(&self, other: &sockaddr_dl) -> bool {
                self.sdl_len == other.sdl_len
                    && self.sdl_family == other.sdl_family
                    && self.sdl_index == other.sdl_index
                    && self.sdl_type == other.sdl_type
                    && self.sdl_nlen == other.sdl_nlen
                    && self.sdl_alen == other.sdl_alen
                    && self.sdl_slen == other.sdl_slen
                    && self
                    .sdl_data
                    .iter()
                    .zip(other.sdl_data.iter())
                    .all(|(a,b)| a == b)
            }
        }
        impl Eq for sockaddr_dl {}
        impl ::fmt::Debug for sockaddr_dl {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("sockaddr_dl")
                    .field("sdl_len", &self.sdl_len)
                    .field("sdl_family", &self.sdl_family)
                    .field("sdl_index", &self.sdl_index)
                    .field("sdl_type", &self.sdl_type)
                    .field("sdl_nlen", &self.sdl_nlen)
                    .field("sdl_alen", &self.sdl_alen)
                    .field("sdl_slen", &self.sdl_slen)
                    // FIXME: .field("sdl_data", &self.sdl_data)
                    .finish()
            }
        }
        impl ::hash::Hash for sockaddr_dl {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.sdl_len.hash(state);
                self.sdl_family.hash(state);
                self.sdl_index.hash(state);
                self.sdl_type.hash(state);
                self.sdl_nlen.hash(state);
                self.sdl_alen.hash(state);
                self.sdl_slen.hash(state);
                self.sdl_data.hash(state);
            }
        }

        impl PartialEq for mq_attr {
            fn eq(&self, other: &mq_attr) -> bool {
                self.mq_flags == other.mq_flags &&
                self.mq_maxmsg == other.mq_maxmsg &&
                self.mq_msgsize == other.mq_msgsize &&
                self.mq_curmsgs == other.mq_curmsgs
            }
        }
        impl Eq for mq_attr {}
        impl ::fmt::Debug for mq_attr {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("mq_attr")
                    .field("mq_flags", &self.mq_flags)
                    .field("mq_maxmsg", &self.mq_maxmsg)
                    .field("mq_msgsize", &self.mq_msgsize)
                    .field("mq_curmsgs", &self.mq_curmsgs)
                    .finish()
            }
        }
        impl ::hash::Hash for mq_attr {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.mq_flags.hash(state);
                self.mq_maxmsg.hash(state);
                self.mq_msgsize.hash(state);
                self.mq_curmsgs.hash(state);
            }
        }

        impl PartialEq for sigevent {
            fn eq(&self, other: &sigevent) -> bool {
                self.sigev_notify == other.sigev_notify
                    && self.sigev_signo == other.sigev_signo
                    && self.sigev_value == other.sigev_value
                    && self.sigev_notify_thread_id
                        == other.sigev_notify_thread_id
            }
        }
        impl Eq for sigevent {}
        impl ::fmt::Debug for sigevent {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("sigevent")
                    .field("sigev_notify", &self.sigev_notify)
                    .field("sigev_signo", &self.sigev_signo)
                    .field("sigev_value", &self.sigev_value)
                    .field("sigev_notify_thread_id",
                           &self.sigev_notify_thread_id)
                    .finish()
            }
        }
        impl ::hash::Hash for sigevent {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.sigev_notify.hash(state);
                self.sigev_signo.hash(state);
                self.sigev_value.hash(state);
                self.sigev_notify_thread_id.hash(state);
            }
        }

        impl PartialEq for ptsstat {
            fn eq(&self, other: &ptsstat) -> bool {
                let self_devname: &[::c_char] = &self.devname;
                let other_devname: &[::c_char] = &other.devname;

                self.dev == other.dev && self_devname == other_devname
            }
        }
        impl Eq for ptsstat {}
        impl ::fmt::Debug for ptsstat {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                let self_devname: &[::c_char] = &self.devname;

                f.debug_struct("ptsstat")
                    .field("dev", &self.dev)
                    .field("devname", &self_devname)
                    .finish()
            }
        }
        impl ::hash::Hash for ptsstat {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                let self_devname: &[::c_char] = &self.devname;

                self.dev.hash(state);
                self_devname.hash(state);
            }
        }

        #[cfg(libc_union)]
        impl PartialEq for __c_anonymous_elf32_auxv_union {
            fn eq(&self, other: &__c_anonymous_elf32_auxv_union) -> bool {
                unsafe { self.a_val == other.a_val}
            }
        }
        #[cfg(libc_union)]
        impl Eq for __c_anonymous_elf32_auxv_union {}
        #[cfg(libc_union)]
        impl ::fmt::Debug for __c_anonymous_elf32_auxv_union {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("a_val")
                    .field("a_val", unsafe { &self.a_val })
                    .finish()
            }
        }
        #[cfg(not(libc_union))]
        impl PartialEq for Elf32_Auxinfo {
            fn eq(&self, other: &Elf32_Auxinfo) -> bool {
                self.a_type == other.a_type
            }
        }
        #[cfg(libc_union)]
        impl PartialEq for Elf32_Auxinfo {
            fn eq(&self, other: &Elf32_Auxinfo) -> bool {
                self.a_type == other.a_type
                    && self.a_un == other.a_un
            }
        }
        impl Eq for Elf32_Auxinfo {}
        #[cfg(not(libc_union))]
        impl ::fmt::Debug for Elf32_Auxinfo {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("Elf32_Auxinfo")
                    .field("a_type", &self.a_type)
                    .finish()
            }
        }
        #[cfg(libc_union)]
        impl ::fmt::Debug for Elf32_Auxinfo {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("Elf32_Auxinfo")
                    .field("a_type", &self.a_type)
                    .field("a_un", &self.a_un)
                    .finish()
            }
        }

        #[cfg(libc_union)]
        impl PartialEq for __c_anonymous_ifr_ifru {
            fn eq(&self, other: &__c_anonymous_ifr_ifru) -> bool {
                unsafe {
                    self.ifru_addr == other.ifru_addr &&
                    self.ifru_dstaddr == other.ifru_dstaddr &&
                    self.ifru_broadaddr == other.ifru_broadaddr &&
                    self.ifru_buffer == other.ifru_buffer &&
                    self.ifru_flags == other.ifru_flags &&
                    self.ifru_index == other.ifru_index &&
                    self.ifru_jid == other.ifru_jid &&
                    self.ifru_metric == other.ifru_metric &&
                    self.ifru_mtu == other.ifru_mtu &&
                    self.ifru_phys == other.ifru_phys &&
                    self.ifru_media == other.ifru_media &&
                    self.ifru_data == other.ifru_data &&
                    self.ifru_cap == other.ifru_cap &&
                    self.ifru_fib == other.ifru_fib &&
                    self.ifru_vlan_pcp == other.ifru_vlan_pcp
                }
            }
        }
        #[cfg(libc_union)]
        impl Eq for __c_anonymous_ifr_ifru {}
        #[cfg(libc_union)]
        impl ::fmt::Debug for __c_anonymous_ifr_ifru {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("ifr_ifru")
                    .field("ifru_addr", unsafe { &self.ifru_addr })
                    .field("ifru_dstaddr", unsafe { &self.ifru_dstaddr })
                    .field("ifru_broadaddr", unsafe { &self.ifru_broadaddr })
                    .field("ifru_buffer", unsafe { &self.ifru_buffer })
                    .field("ifru_flags", unsafe { &self.ifru_flags })
                    .field("ifru_index", unsafe { &self.ifru_index })
                    .field("ifru_jid", unsafe { &self.ifru_jid })
                    .field("ifru_metric", unsafe { &self.ifru_metric })
                    .field("ifru_mtu", unsafe { &self.ifru_mtu })
                    .field("ifru_phys", unsafe { &self.ifru_phys })
                    .field("ifru_media", unsafe { &self.ifru_media })
                    .field("ifru_data", unsafe { &self.ifru_data })
                    .field("ifru_cap", unsafe { &self.ifru_cap })
                    .field("ifru_fib", unsafe { &self.ifru_fib })
                    .field("ifru_vlan_pcp", unsafe { &self.ifru_vlan_pcp })
                    .finish()
            }
        }
        #[cfg(libc_union)]
        impl ::hash::Hash for __c_anonymous_ifr_ifru {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                unsafe { self.ifru_addr.hash(state) };
                unsafe { self.ifru_dstaddr.hash(state) };
                unsafe { self.ifru_broadaddr.hash(state) };
                unsafe { self.ifru_buffer.hash(state) };
                unsafe { self.ifru_flags.hash(state) };
                unsafe { self.ifru_index.hash(state) };
                unsafe { self.ifru_jid.hash(state) };
                unsafe { self.ifru_metric.hash(state) };
                unsafe { self.ifru_mtu.hash(state) };
                unsafe { self.ifru_phys.hash(state) };
                unsafe { self.ifru_media.hash(state) };
                unsafe { self.ifru_data.hash(state) };
                unsafe { self.ifru_cap.hash(state) };
                unsafe { self.ifru_fib.hash(state) };
                unsafe { self.ifru_vlan_pcp.hash(state) };
            }
        }

        impl PartialEq for ifreq {
            fn eq(&self, other: &ifreq) -> bool {
                self.ifr_name == other.ifr_name && self.ifr_ifru == other.ifr_ifru
            }
        }
        impl Eq for ifreq {}
        impl ::fmt::Debug for ifreq {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("ifreq")
                    .field("ifr_name", &self.ifr_name)
                    .field("ifr_ifru", &self.ifr_ifru)
                    .finish()
            }
        }
        impl ::hash::Hash for ifreq {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.ifr_name.hash(state);
                self.ifr_ifru.hash(state);
            }
        }

        impl PartialEq for ifstat {
            fn eq(&self, other: &ifstat) -> bool {
                let self_ascii: &[::c_char] = &self.ascii;
                let other_ascii: &[::c_char] = &other.ascii;

                self.ifs_name == other.ifs_name && self_ascii == other_ascii
            }
        }
        impl Eq for ifstat {}
        impl ::fmt::Debug for ifstat {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                let ascii: &[::c_char] = &self.ascii;

                f.debug_struct("ifstat")
                    .field("ifs_name", &self.ifs_name)
                    .field("ascii", &ascii)
                    .finish()
            }
        }
        impl ::hash::Hash for ifstat {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.ifs_name.hash(state);
                self.ascii.hash(state);
            }
        }

        impl PartialEq for ifrsskey {
            fn eq(&self, other: &ifrsskey) -> bool {
                let self_ifrk_key: &[u8] = &self.ifrk_key;
                let other_ifrk_key: &[u8] = &other.ifrk_key;

                self.ifrk_name == other.ifrk_name &&
                self.ifrk_func == other.ifrk_func &&
                self.ifrk_spare0 == other.ifrk_spare0 &&
                self.ifrk_keylen == other.ifrk_keylen &&
                self_ifrk_key == other_ifrk_key
            }
        }
        impl Eq for ifrsskey {}
        impl ::fmt::Debug for ifrsskey {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                let ifrk_key: &[u8] = &self.ifrk_key;

                f.debug_struct("ifrsskey")
                    .field("ifrk_name", &self.ifrk_name)
                    .field("ifrk_func", &self.ifrk_func)
                    .field("ifrk_spare0", &self.ifrk_spare0)
                    .field("ifrk_keylen", &self.ifrk_keylen)
                    .field("ifrk_key", &ifrk_key)
                    .finish()
            }
        }
        impl ::hash::Hash for ifrsskey {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.ifrk_name.hash(state);
                self.ifrk_func.hash(state);
                self.ifrk_spare0.hash(state);
                self.ifrk_keylen.hash(state);
                self.ifrk_key.hash(state);
            }
        }

        impl PartialEq for ifdownreason {
            fn eq(&self, other: &ifdownreason) -> bool {
                let self_ifdr_msg: &[::c_char] = &self.ifdr_msg;
                let other_ifdr_msg: &[::c_char] = &other.ifdr_msg;

                self.ifdr_name == other.ifdr_name &&
                self.ifdr_reason == other.ifdr_reason &&
                self.ifdr_vendor == other.ifdr_vendor &&
                self_ifdr_msg == other_ifdr_msg
            }
        }
        impl Eq for ifdownreason {}
        impl ::fmt::Debug for ifdownreason {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                let ifdr_msg: &[::c_char] = &self.ifdr_msg;

                f.debug_struct("ifdownreason")
                    .field("ifdr_name", &self.ifdr_name)
                    .field("ifdr_reason", &self.ifdr_reason)
                    .field("ifdr_vendor", &self.ifdr_vendor)
                    .field("ifdr_msg", &ifdr_msg)
                    .finish()
            }
        }
        impl ::hash::Hash for ifdownreason {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.ifdr_name.hash(state);
                self.ifdr_reason.hash(state);
                self.ifdr_vendor.hash(state);
                self.ifdr_msg.hash(state);
            }
        }

        #[cfg(libc_union)]
        impl PartialEq for __c_anonymous_ifi_epoch {
            fn eq(&self, other: &__c_anonymous_ifi_epoch) -> bool {
                unsafe {
                    self.tt == other.tt &&
                    self.ph == other.ph
                }
            }
        }
        #[cfg(libc_union)]
        impl Eq for __c_anonymous_ifi_epoch {}
        #[cfg(libc_union)]
        impl ::fmt::Debug for __c_anonymous_ifi_epoch {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("__c_anonymous_ifi_epoch")
                    .field("tt", unsafe { &self.tt })
                    .field("ph", unsafe { &self.ph })
                    .finish()
            }
        }
        #[cfg(libc_union)]
        impl ::hash::Hash for __c_anonymous_ifi_epoch {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                unsafe {
                    self.tt.hash(state);
                    self.ph.hash(state);
                }
            }
        }

        #[cfg(libc_union)]
        impl PartialEq for __c_anonymous_ifi_lastchange {
            fn eq(&self, other: &__c_anonymous_ifi_lastchange) -> bool {
                unsafe {
                    self.tv == other.tv &&
                    self.ph == other.ph
                }
            }
        }
        #[cfg(libc_union)]
        impl Eq for __c_anonymous_ifi_lastchange {}
        #[cfg(libc_union)]
        impl ::fmt::Debug for __c_anonymous_ifi_lastchange {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("__c_anonymous_ifi_lastchange")
                    .field("tv", unsafe { &self.tv })
                    .field("ph", unsafe { &self.ph })
                    .finish()
            }
        }
        #[cfg(libc_union)]
        impl ::hash::Hash for __c_anonymous_ifi_lastchange {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                unsafe {
                    self.tv.hash(state);
                    self.ph.hash(state);
                }
            }
        }

        impl PartialEq for if_data {
            fn eq(&self, other: &if_data) -> bool {
                self.ifi_type == other.ifi_type &&
                self.ifi_physical == other.ifi_physical &&
                self.ifi_addrlen == other.ifi_addrlen &&
                self.ifi_hdrlen == other.ifi_hdrlen &&
                self.ifi_link_state == other.ifi_link_state &&
                self.ifi_vhid == other.ifi_vhid &&
                self.ifi_datalen == other.ifi_datalen &&
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
                self.ifi_oqdrops == other.ifi_oqdrops &&
                self.ifi_noproto == other.ifi_noproto &&
                self.ifi_hwassist == other.ifi_hwassist &&
                self.__ifi_epoch == other.__ifi_epoch &&
                self.__ifi_lastchange == other.__ifi_lastchange
            }
        }
        impl Eq for if_data {}
        impl ::fmt::Debug for if_data {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("if_data")
                    .field("ifi_type", &self.ifi_type)
                    .field("ifi_physical", &self.ifi_physical)
                    .field("ifi_addrlen", &self.ifi_addrlen)
                    .field("ifi_hdrlen", &self.ifi_hdrlen)
                    .field("ifi_link_state", &self.ifi_link_state)
                    .field("ifi_vhid", &self.ifi_vhid)
                    .field("ifi_datalen", &self.ifi_datalen)
                    .field("ifi_mtu", &self.ifi_mtu)
                    .field("ifi_metric", &self.ifi_metric)
                    .field("ifi_baudrate", &self.ifi_baudrate)
                    .field("ifi_ipackets", &self.ifi_ipackets)
                    .field("ifi_ierrors", &self.ifi_ierrors)
                    .field("ifi_opackets", &self.ifi_opackets)
                    .field("ifi_oerrors", &self.ifi_oerrors)
                    .field("ifi_collisions", &self.ifi_collisions)
                    .field("ifi_ibytes", &self.ifi_ibytes)
                    .field("ifi_obytes", &self.ifi_obytes)
                    .field("ifi_imcasts", &self.ifi_imcasts)
                    .field("ifi_omcasts", &self.ifi_omcasts)
                    .field("ifi_iqdrops", &self.ifi_iqdrops)
                    .field("ifi_oqdrops", &self.ifi_oqdrops)
                    .field("ifi_noproto", &self.ifi_noproto)
                    .field("ifi_hwassist", &self.ifi_hwassist)
                    .field("__ifi_epoch", &self.__ifi_epoch)
                    .field("__ifi_lastchange", &self.__ifi_lastchange)
                    .finish()
            }
        }
        impl ::hash::Hash for if_data {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.ifi_type.hash(state);
                self.ifi_physical.hash(state);
                self.ifi_addrlen.hash(state);
                self.ifi_hdrlen.hash(state);
                self.ifi_link_state.hash(state);
                self.ifi_vhid.hash(state);
                self.ifi_datalen.hash(state);
                self.ifi_mtu.hash(state);
                self.ifi_metric.hash(state);
                self.ifi_baudrate.hash(state);
                self.ifi_ipackets.hash(state);
                self.ifi_ierrors.hash(state);
                self.ifi_opackets.hash(state);
                self.ifi_oerrors.hash(state);
                self.ifi_collisions.hash(state);
                self.ifi_ibytes.hash(state);
                self.ifi_obytes.hash(state);
                self.ifi_imcasts.hash(state);
                self.ifi_omcasts.hash(state);
                self.ifi_iqdrops.hash(state);
                self.ifi_oqdrops.hash(state);
                self.ifi_noproto.hash(state);
                self.ifi_hwassist.hash(state);
                self.__ifi_epoch.hash(state);
                self.__ifi_lastchange.hash(state);
            }
        }
    }
}

#[cfg_attr(feature = "extra_traits", derive(Debug))]
#[repr(u32)]
pub enum dot3Vendors {
    dot3VendorAMD = 1,
    dot3VendorIntel = 2,
    dot3VendorNational = 4,
    dot3VendorFujitsu = 5,
    dot3VendorDigital = 6,
    dot3VendorWesternDigital = 7,
}
impl ::Copy for dot3Vendors {}
impl ::Clone for dot3Vendors {
    fn clone(&self) -> dot3Vendors {
        *self
    }
}

// sys/devicestat.h
pub const DEVSTAT_N_TRANS_FLAGS: ::c_int = 4;
pub const DEVSTAT_NAME_LEN: ::c_int = 16;

pub const SIGEV_THREAD_ID: ::c_int = 4;

pub const EXTATTR_NAMESPACE_EMPTY: ::c_int = 0;
pub const EXTATTR_NAMESPACE_USER: ::c_int = 1;
pub const EXTATTR_NAMESPACE_SYSTEM: ::c_int = 2;

pub const PTHREAD_STACK_MIN: ::size_t = MINSIGSTKSZ;
pub const PTHREAD_MUTEX_ADAPTIVE_NP: ::c_int = 4;
pub const PTHREAD_MUTEX_STALLED: ::c_int = 0;
pub const PTHREAD_MUTEX_ROBUST: ::c_int = 1;
pub const SIGSTKSZ: ::size_t = MINSIGSTKSZ + 32768;
pub const SF_NODISKIO: ::c_int = 0x00000001;
pub const SF_MNOWAIT: ::c_int = 0x00000002;
pub const SF_SYNC: ::c_int = 0x00000004;
pub const SF_USER_READAHEAD: ::c_int = 0x00000008;
pub const SF_NOCACHE: ::c_int = 0x00000010;
pub const O_CLOEXEC: ::c_int = 0x00100000;
pub const O_DIRECTORY: ::c_int = 0x00020000;
pub const O_EXEC: ::c_int = 0x00040000;
pub const O_TTY_INIT: ::c_int = 0x00080000;
pub const F_GETLK: ::c_int = 11;
pub const F_SETLK: ::c_int = 12;
pub const F_SETLKW: ::c_int = 13;
pub const ENOTCAPABLE: ::c_int = 93;
pub const ECAPMODE: ::c_int = 94;
pub const ENOTRECOVERABLE: ::c_int = 95;
pub const EOWNERDEAD: ::c_int = 96;
pub const EINTEGRITY: ::c_int = 97;
pub const RLIMIT_NPTS: ::c_int = 11;
pub const RLIMIT_SWAP: ::c_int = 12;
pub const RLIMIT_KQUEUES: ::c_int = 13;
pub const RLIMIT_UMTXP: ::c_int = 14;
#[deprecated(since = "0.2.64", note = "Not stable across OS versions")]
pub const RLIM_NLIMITS: ::rlim_t = 15;
pub const RLIM_SAVED_MAX: ::rlim_t = ::RLIM_INFINITY;
pub const RLIM_SAVED_CUR: ::rlim_t = ::RLIM_INFINITY;

pub const CP_USER: ::c_int = 0;
pub const CP_NICE: ::c_int = 1;
pub const CP_SYS: ::c_int = 2;
pub const CP_INTR: ::c_int = 3;
pub const CP_IDLE: ::c_int = 4;
pub const CPUSTATES: ::c_int = 5;

pub const NI_NOFQDN: ::c_int = 0x00000001;
pub const NI_NUMERICHOST: ::c_int = 0x00000002;
pub const NI_NAMEREQD: ::c_int = 0x00000004;
pub const NI_NUMERICSERV: ::c_int = 0x00000008;
pub const NI_DGRAM: ::c_int = 0x00000010;
pub const NI_NUMERICSCOPE: ::c_int = 0x00000020;

pub const XU_NGROUPS: ::c_int = 16;

pub const Q_GETQUOTA: ::c_int = 0x700;
pub const Q_SETQUOTA: ::c_int = 0x800;

pub const MAP_GUARD: ::c_int = 0x00002000;
pub const MAP_EXCL: ::c_int = 0x00004000;
pub const MAP_PREFAULT_READ: ::c_int = 0x00040000;
pub const MAP_ALIGNED_SUPER: ::c_int = 1 << 24;

pub const POSIX_FADV_NORMAL: ::c_int = 0;
pub const POSIX_FADV_RANDOM: ::c_int = 1;
pub const POSIX_FADV_SEQUENTIAL: ::c_int = 2;
pub const POSIX_FADV_WILLNEED: ::c_int = 3;
pub const POSIX_FADV_DONTNEED: ::c_int = 4;
pub const POSIX_FADV_NOREUSE: ::c_int = 5;

pub const POLLINIGNEOF: ::c_short = 0x2000;

pub const EVFILT_READ: i16 = -1;
pub const EVFILT_WRITE: i16 = -2;
pub const EVFILT_AIO: i16 = -3;
pub const EVFILT_VNODE: i16 = -4;
pub const EVFILT_PROC: i16 = -5;
pub const EVFILT_SIGNAL: i16 = -6;
pub const EVFILT_TIMER: i16 = -7;
pub const EVFILT_PROCDESC: i16 = -8;
pub const EVFILT_FS: i16 = -9;
pub const EVFILT_LIO: i16 = -10;
pub const EVFILT_USER: i16 = -11;
pub const EVFILT_SENDFILE: i16 = -12;
pub const EVFILT_EMPTY: i16 = -13;

pub const EV_ADD: u16 = 0x1;
pub const EV_DELETE: u16 = 0x2;
pub const EV_ENABLE: u16 = 0x4;
pub const EV_DISABLE: u16 = 0x8;
pub const EV_ONESHOT: u16 = 0x10;
pub const EV_CLEAR: u16 = 0x20;
pub const EV_RECEIPT: u16 = 0x40;
pub const EV_DISPATCH: u16 = 0x80;
pub const EV_DROP: u16 = 0x1000;
pub const EV_FLAG1: u16 = 0x2000;
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
pub const NOTE_EXIT: u32 = 0x80000000;
pub const NOTE_FORK: u32 = 0x40000000;
pub const NOTE_EXEC: u32 = 0x20000000;
pub const NOTE_PDATAMASK: u32 = 0x000fffff;
pub const NOTE_PCTRLMASK: u32 = 0xf0000000;
pub const NOTE_TRACK: u32 = 0x00000001;
pub const NOTE_TRACKERR: u32 = 0x00000002;
pub const NOTE_CHILD: u32 = 0x00000004;
pub const NOTE_SECONDS: u32 = 0x00000001;
pub const NOTE_MSECONDS: u32 = 0x00000002;
pub const NOTE_USECONDS: u32 = 0x00000004;
pub const NOTE_NSECONDS: u32 = 0x00000008;

pub const MADV_PROTECT: ::c_int = 10;

#[doc(hidden)]
#[deprecated(
    since = "0.2.72",
    note = "CTL_UNSPEC is deprecated. Use CTL_SYSCTL instead"
)]
pub const CTL_UNSPEC: ::c_int = 0;
pub const CTL_SYSCTL: ::c_int = 0;
pub const CTL_KERN: ::c_int = 1;
pub const CTL_VM: ::c_int = 2;
pub const CTL_VFS: ::c_int = 3;
pub const CTL_NET: ::c_int = 4;
pub const CTL_DEBUG: ::c_int = 5;
pub const CTL_HW: ::c_int = 6;
pub const CTL_MACHDEP: ::c_int = 7;
pub const CTL_USER: ::c_int = 8;
pub const CTL_P1003_1B: ::c_int = 9;

// sys/sysctl.h
pub const CTL_MAXNAME: ::c_int = 24;

pub const CTLTYPE: ::c_int = 0xf;
pub const CTLTYPE_NODE: ::c_int = 1;
pub const CTLTYPE_INT: ::c_int = 2;
pub const CTLTYPE_STRING: ::c_int = 3;
pub const CTLTYPE_S64: ::c_int = 4;
pub const CTLTYPE_OPAQUE: ::c_int = 5;
pub const CTLTYPE_STRUCT: ::c_int = CTLTYPE_OPAQUE;
pub const CTLTYPE_UINT: ::c_int = 6;
pub const CTLTYPE_LONG: ::c_int = 7;
pub const CTLTYPE_ULONG: ::c_int = 8;
pub const CTLTYPE_U64: ::c_int = 9;
pub const CTLTYPE_U8: ::c_int = 0xa;
pub const CTLTYPE_U16: ::c_int = 0xb;
pub const CTLTYPE_S8: ::c_int = 0xc;
pub const CTLTYPE_S16: ::c_int = 0xd;
pub const CTLTYPE_S32: ::c_int = 0xe;
pub const CTLTYPE_U32: ::c_int = 0xf;

pub const CTLFLAG_RD: ::c_int = 0x80000000;
pub const CTLFLAG_WR: ::c_int = 0x40000000;
pub const CTLFLAG_RW: ::c_int = CTLFLAG_RD | CTLFLAG_WR;
pub const CTLFLAG_DORMANT: ::c_int = 0x20000000;
pub const CTLFLAG_ANYBODY: ::c_int = 0x10000000;
pub const CTLFLAG_SECURE: ::c_int = 0x08000000;
pub const CTLFLAG_PRISON: ::c_int = 0x04000000;
pub const CTLFLAG_DYN: ::c_int = 0x02000000;
pub const CTLFLAG_SKIP: ::c_int = 0x01000000;
pub const CTLMASK_SECURE: ::c_int = 0x00F00000;
pub const CTLFLAG_TUN: ::c_int = 0x00080000;
pub const CTLFLAG_RDTUN: ::c_int = CTLFLAG_RD | CTLFLAG_TUN;
pub const CTLFLAG_RWTUN: ::c_int = CTLFLAG_RW | CTLFLAG_TUN;
pub const CTLFLAG_MPSAFE: ::c_int = 0x00040000;
pub const CTLFLAG_VNET: ::c_int = 0x00020000;
pub const CTLFLAG_DYING: ::c_int = 0x00010000;
pub const CTLFLAG_CAPRD: ::c_int = 0x00008000;
pub const CTLFLAG_CAPWR: ::c_int = 0x00004000;
pub const CTLFLAG_STATS: ::c_int = 0x00002000;
pub const CTLFLAG_NOFETCH: ::c_int = 0x00001000;
pub const CTLFLAG_CAPRW: ::c_int = CTLFLAG_CAPRD | CTLFLAG_CAPWR;
pub const CTLFLAG_NEEDGIANT: ::c_int = 0x00000800;

pub const CTLSHIFT_SECURE: ::c_int = 20;
pub const CTLFLAG_SECURE1: ::c_int = CTLFLAG_SECURE | (0 << CTLSHIFT_SECURE);
pub const CTLFLAG_SECURE2: ::c_int = CTLFLAG_SECURE | (1 << CTLSHIFT_SECURE);
pub const CTLFLAG_SECURE3: ::c_int = CTLFLAG_SECURE | (2 << CTLSHIFT_SECURE);

pub const OID_AUTO: ::c_int = -1;

pub const CTL_SYSCTL_DEBUG: ::c_int = 0;
pub const CTL_SYSCTL_NAME: ::c_int = 1;
pub const CTL_SYSCTL_NEXT: ::c_int = 2;
pub const CTL_SYSCTL_NAME2OID: ::c_int = 3;
pub const CTL_SYSCTL_OIDFMT: ::c_int = 4;
pub const CTL_SYSCTL_OIDDESCR: ::c_int = 5;
pub const CTL_SYSCTL_OIDLABEL: ::c_int = 6;
pub const CTL_SYSCTL_NEXTNOSKIP: ::c_int = 7;

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
pub const KERN_UPDATEINTERVAL: ::c_int = 23;
pub const KERN_OSRELDATE: ::c_int = 24;
pub const KERN_NTP_PLL: ::c_int = 25;
pub const KERN_BOOTFILE: ::c_int = 26;
pub const KERN_MAXFILESPERPROC: ::c_int = 27;
pub const KERN_MAXPROCPERUID: ::c_int = 28;
pub const KERN_DUMPDEV: ::c_int = 29;
pub const KERN_IPC: ::c_int = 30;
pub const KERN_DUMMY: ::c_int = 31;
pub const KERN_PS_STRINGS: ::c_int = 32;
pub const KERN_USRSTACK: ::c_int = 33;
pub const KERN_LOGSIGEXIT: ::c_int = 34;
pub const KERN_IOV_MAX: ::c_int = 35;
pub const KERN_HOSTUUID: ::c_int = 36;
pub const KERN_ARND: ::c_int = 37;
pub const KERN_MAXPHYS: ::c_int = 38;
pub const KERN_STACKTOP: ::c_int = 39;

pub const KERN_PROC_ALL: ::c_int = 0;
pub const KERN_PROC_PID: ::c_int = 1;
pub const KERN_PROC_PGRP: ::c_int = 2;
pub const KERN_PROC_SESSION: ::c_int = 3;
pub const KERN_PROC_TTY: ::c_int = 4;
pub const KERN_PROC_UID: ::c_int = 5;
pub const KERN_PROC_RUID: ::c_int = 6;
pub const KERN_PROC_ARGS: ::c_int = 7;
pub const KERN_PROC_PROC: ::c_int = 8;
pub const KERN_PROC_SV_NAME: ::c_int = 9;
pub const KERN_PROC_RGID: ::c_int = 10;
pub const KERN_PROC_GID: ::c_int = 11;
pub const KERN_PROC_PATHNAME: ::c_int = 12;
pub const KERN_PROC_OVMMAP: ::c_int = 13;
pub const KERN_PROC_OFILEDESC: ::c_int = 14;
pub const KERN_PROC_KSTACK: ::c_int = 15;
pub const KERN_PROC_INC_THREAD: ::c_int = 0x10;
pub const KERN_PROC_VMMAP: ::c_int = 32;
pub const KERN_PROC_FILEDESC: ::c_int = 33;
pub const KERN_PROC_GROUPS: ::c_int = 34;
pub const KERN_PROC_ENV: ::c_int = 35;
pub const KERN_PROC_AUXV: ::c_int = 36;
pub const KERN_PROC_RLIMIT: ::c_int = 37;
pub const KERN_PROC_PS_STRINGS: ::c_int = 38;
pub const KERN_PROC_UMASK: ::c_int = 39;
pub const KERN_PROC_OSREL: ::c_int = 40;
pub const KERN_PROC_SIGTRAMP: ::c_int = 41;
pub const KERN_PROC_CWD: ::c_int = 42;
pub const KERN_PROC_NFDS: ::c_int = 43;
pub const KERN_PROC_SIGFASTBLK: ::c_int = 44;

pub const KIPC_MAXSOCKBUF: ::c_int = 1;
pub const KIPC_SOCKBUF_WASTE: ::c_int = 2;
pub const KIPC_SOMAXCONN: ::c_int = 3;
pub const KIPC_MAX_LINKHDR: ::c_int = 4;
pub const KIPC_MAX_PROTOHDR: ::c_int = 5;
pub const KIPC_MAX_HDR: ::c_int = 6;
pub const KIPC_MAX_DATALEN: ::c_int = 7;

pub const HW_MACHINE: ::c_int = 1;
pub const HW_MODEL: ::c_int = 2;
pub const HW_NCPU: ::c_int = 3;
pub const HW_BYTEORDER: ::c_int = 4;
pub const HW_PHYSMEM: ::c_int = 5;
pub const HW_USERMEM: ::c_int = 6;
pub const HW_PAGESIZE: ::c_int = 7;
pub const HW_DISKNAMES: ::c_int = 8;
pub const HW_DISKSTATS: ::c_int = 9;
pub const HW_FLOATINGPT: ::c_int = 10;
pub const HW_MACHINE_ARCH: ::c_int = 11;
pub const HW_REALMEM: ::c_int = 12;

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
pub const USER_LOCALBASE: ::c_int = 21;

pub const CTL_P1003_1B_ASYNCHRONOUS_IO: ::c_int = 1;
pub const CTL_P1003_1B_MAPPED_FILES: ::c_int = 2;
pub const CTL_P1003_1B_MEMLOCK: ::c_int = 3;
pub const CTL_P1003_1B_MEMLOCK_RANGE: ::c_int = 4;
pub const CTL_P1003_1B_MEMORY_PROTECTION: ::c_int = 5;
pub const CTL_P1003_1B_MESSAGE_PASSING: ::c_int = 6;
pub const CTL_P1003_1B_PRIORITIZED_IO: ::c_int = 7;
pub const CTL_P1003_1B_PRIORITY_SCHEDULING: ::c_int = 8;
pub const CTL_P1003_1B_REALTIME_SIGNALS: ::c_int = 9;
pub const CTL_P1003_1B_SEMAPHORES: ::c_int = 10;
pub const CTL_P1003_1B_FSYNC: ::c_int = 11;
pub const CTL_P1003_1B_SHARED_MEMORY_OBJECTS: ::c_int = 12;
pub const CTL_P1003_1B_SYNCHRONIZED_IO: ::c_int = 13;
pub const CTL_P1003_1B_TIMERS: ::c_int = 14;
pub const CTL_P1003_1B_AIO_LISTIO_MAX: ::c_int = 15;
pub const CTL_P1003_1B_AIO_MAX: ::c_int = 16;
pub const CTL_P1003_1B_AIO_PRIO_DELTA_MAX: ::c_int = 17;
pub const CTL_P1003_1B_DELAYTIMER_MAX: ::c_int = 18;
pub const CTL_P1003_1B_MQ_OPEN_MAX: ::c_int = 19;
pub const CTL_P1003_1B_PAGESIZE: ::c_int = 20;
pub const CTL_P1003_1B_RTSIG_MAX: ::c_int = 21;
pub const CTL_P1003_1B_SEM_NSEMS_MAX: ::c_int = 22;
pub const CTL_P1003_1B_SEM_VALUE_MAX: ::c_int = 23;
pub const CTL_P1003_1B_SIGQUEUE_MAX: ::c_int = 24;
pub const CTL_P1003_1B_TIMER_MAX: ::c_int = 25;

pub const TIOCGPTN: ::c_uint = 0x4004740f;
pub const TIOCPTMASTER: ::c_uint = 0x2000741c;
pub const TIOCSIG: ::c_uint = 0x2004745f;
pub const TIOCM_DCD: ::c_int = 0x40;
pub const H4DISC: ::c_int = 0x7;

pub const VM_TOTAL: ::c_int = 1;

pub const BIOCSETFNR: ::c_ulong = 0x80104282;

pub const FIODGNAME: ::c_ulong = 0x80106678;
pub const FIONWRITE: ::c_ulong = 0x40046677;
pub const FIONSPACE: ::c_ulong = 0x40046676;
pub const FIOSEEKDATA: ::c_ulong = 0xc0086661;
pub const FIOSEEKHOLE: ::c_ulong = 0xc0086662;

pub const JAIL_API_VERSION: u32 = 2;
pub const JAIL_CREATE: ::c_int = 0x01;
pub const JAIL_UPDATE: ::c_int = 0x02;
pub const JAIL_ATTACH: ::c_int = 0x04;
pub const JAIL_DYING: ::c_int = 0x08;
pub const JAIL_SET_MASK: ::c_int = 0x0f;
pub const JAIL_GET_MASK: ::c_int = 0x08;
pub const JAIL_SYS_DISABLE: ::c_int = 0;
pub const JAIL_SYS_NEW: ::c_int = 1;
pub const JAIL_SYS_INHERIT: ::c_int = 2;

pub const MNT_ACLS: ::c_int = 0x08000000;
pub const MNT_BYFSID: ::c_int = 0x08000000;
pub const MNT_GJOURNAL: ::c_int = 0x02000000;
pub const MNT_MULTILABEL: ::c_int = 0x04000000;
pub const MNT_NFS4ACLS: ::c_int = 0x00000010;
pub const MNT_SNAPSHOT: ::c_int = 0x01000000;
pub const MNT_UNION: ::c_int = 0x00000020;
pub const MNT_EXPUBLIC: ::c_int = 0x20000000;
pub const MNT_NONBUSY: ::c_int = 0x04000000;

pub const SCM_CREDS2: ::c_int = 0x08;

pub const SO_BINTIME: ::c_int = 0x2000;
pub const SO_NO_OFFLOAD: ::c_int = 0x4000;
pub const SO_NO_DDP: ::c_int = 0x8000;
pub const SO_REUSEPORT_LB: ::c_int = 0x10000;
pub const SO_LABEL: ::c_int = 0x1009;
pub const SO_PEERLABEL: ::c_int = 0x1010;
pub const SO_LISTENQLIMIT: ::c_int = 0x1011;
pub const SO_LISTENQLEN: ::c_int = 0x1012;
pub const SO_LISTENINCQLEN: ::c_int = 0x1013;
pub const SO_SETFIB: ::c_int = 0x1014;
pub const SO_USER_COOKIE: ::c_int = 0x1015;
pub const SO_PROTOCOL: ::c_int = 0x1016;
pub const SO_PROTOTYPE: ::c_int = SO_PROTOCOL;
pub const SO_DOMAIN: ::c_int = 0x1019;
pub const SO_VENDOR: ::c_int = 0x80000000;

pub const LOCAL_CREDS: ::c_int = 2;
pub const LOCAL_CREDS_PERSISTENT: ::c_int = 3;
pub const LOCAL_CONNWAIT: ::c_int = 4;
pub const LOCAL_VENDOR: ::c_int = SO_VENDOR;

pub const PL_EVENT_NONE: ::c_int = 0;
pub const PL_EVENT_SIGNAL: ::c_int = 1;
pub const PL_FLAG_SA: ::c_int = 0x01;
pub const PL_FLAG_BOUND: ::c_int = 0x02;
pub const PL_FLAG_SCE: ::c_int = 0x04;
pub const PL_FLAG_SCX: ::c_int = 0x08;
pub const PL_FLAG_EXEC: ::c_int = 0x10;
pub const PL_FLAG_SI: ::c_int = 0x20;
pub const PL_FLAG_FORKED: ::c_int = 0x40;
pub const PL_FLAG_CHILD: ::c_int = 0x80;
pub const PL_FLAG_BORN: ::c_int = 0x100;
pub const PL_FLAG_EXITED: ::c_int = 0x200;
pub const PL_FLAG_VFORKED: ::c_int = 0x400;
pub const PL_FLAG_VFORK_DONE: ::c_int = 0x800;

pub const PT_LWPINFO: ::c_int = 13;
pub const PT_GETNUMLWPS: ::c_int = 14;
pub const PT_GETLWPLIST: ::c_int = 15;
pub const PT_CLEARSTEP: ::c_int = 16;
pub const PT_SETSTEP: ::c_int = 17;
pub const PT_SUSPEND: ::c_int = 18;
pub const PT_RESUME: ::c_int = 19;
pub const PT_TO_SCE: ::c_int = 20;
pub const PT_TO_SCX: ::c_int = 21;
pub const PT_SYSCALL: ::c_int = 22;
pub const PT_FOLLOW_FORK: ::c_int = 23;
pub const PT_LWP_EVENTS: ::c_int = 24;
pub const PT_GET_EVENT_MASK: ::c_int = 25;
pub const PT_SET_EVENT_MASK: ::c_int = 26;
pub const PT_GET_SC_ARGS: ::c_int = 27;
pub const PT_GET_SC_RET: ::c_int = 28;
pub const PT_COREDUMP: ::c_int = 29;
pub const PT_GETREGS: ::c_int = 33;
pub const PT_SETREGS: ::c_int = 34;
pub const PT_GETFPREGS: ::c_int = 35;
pub const PT_SETFPREGS: ::c_int = 36;
pub const PT_GETDBREGS: ::c_int = 37;
pub const PT_SETDBREGS: ::c_int = 38;
pub const PT_VM_TIMESTAMP: ::c_int = 40;
pub const PT_VM_ENTRY: ::c_int = 41;
pub const PT_FIRSTMACH: ::c_int = 64;

pub const PTRACE_EXEC: ::c_int = 0x0001;
pub const PTRACE_SCE: ::c_int = 0x0002;
pub const PTRACE_SCX: ::c_int = 0x0004;
pub const PTRACE_SYSCALL: ::c_int = PTRACE_SCE | PTRACE_SCX;
pub const PTRACE_FORK: ::c_int = 0x0008;
pub const PTRACE_LWP: ::c_int = 0x0010;
pub const PTRACE_VFORK: ::c_int = 0x0020;
pub const PTRACE_DEFAULT: ::c_int = PTRACE_EXEC;

pub const PC_COMPRESS: u32 = 0x00000001;
pub const PC_ALL: u32 = 0x00000002;

pub const PROC_SPROTECT: ::c_int = 1;
pub const PROC_REAP_ACQUIRE: ::c_int = 2;
pub const PROC_REAP_RELEASE: ::c_int = 3;
pub const PROC_REAP_STATUS: ::c_int = 4;
pub const PROC_REAP_GETPIDS: ::c_int = 5;
pub const PROC_REAP_KILL: ::c_int = 6;
pub const PROC_TRACE_CTL: ::c_int = 7;
pub const PROC_TRACE_STATUS: ::c_int = 8;
pub const PROC_TRAPCAP_CTL: ::c_int = 9;
pub const PROC_TRAPCAP_STATUS: ::c_int = 10;
pub const PROC_PDEATHSIG_CTL: ::c_int = 11;
pub const PROC_PDEATHSIG_STATUS: ::c_int = 12;
pub const PROC_ASLR_CTL: ::c_int = 13;
pub const PROC_ASLR_STATUS: ::c_int = 14;
pub const PROC_PROTMAX_CTL: ::c_int = 15;
pub const PROC_PROTMAX_STATUS: ::c_int = 16;
pub const PROC_STACKGAP_CTL: ::c_int = 17;
pub const PROC_STACKGAP_STATUS: ::c_int = 18;
pub const PROC_PROCCTL_MD_MIN: ::c_int = 0x10000000;

pub const PPROT_SET: ::c_int = 1;
pub const PPROT_CLEAR: ::c_int = 2;
pub const PPROT_DESCEND: ::c_int = 0x10;
pub const PPROT_INHERIT: ::c_int = 0x20;

pub const PROC_TRACE_CTL_ENABLE: ::c_int = 1;
pub const PROC_TRACE_CTL_DISABLE: ::c_int = 2;
pub const PROC_TRACE_CTL_DISABLE_EXEC: ::c_int = 3;

pub const PROC_TRAPCAP_CTL_ENABLE: ::c_int = 1;
pub const PROC_TRAPCAP_CTL_DISABLE: ::c_int = 2;

pub const PROC_ASLR_FORCE_ENABLE: ::c_int = 1;
pub const PROC_ASLR_FORCE_DISABLE: ::c_int = 2;
pub const PROC_ASLR_NOFORCE: ::c_int = 3;
pub const PROC_ASLR_ACTIVE: ::c_int = 0x80000000;

pub const PROC_PROTMAX_FORCE_ENABLE: ::c_int = 1;
pub const PROC_PROTMAX_FORCE_DISABLE: ::c_int = 2;
pub const PROC_PROTMAX_NOFORCE: ::c_int = 3;
pub const PROC_PROTMAX_ACTIVE: ::c_int = 0x80000000;

pub const PROC_STACKGAP_ENABLE: ::c_int = 0x0001;
pub const PROC_STACKGAP_DISABLE: ::c_int = 0x0002;
pub const PROC_STACKGAP_ENABLE_EXEC: ::c_int = 0x0004;
pub const PROC_STACKGAP_DISABLE_EXEC: ::c_int = 0x0008;

pub const AF_SLOW: ::c_int = 33;
pub const AF_SCLUSTER: ::c_int = 34;
pub const AF_ARP: ::c_int = 35;
pub const AF_BLUETOOTH: ::c_int = 36;
pub const AF_IEEE80211: ::c_int = 37;
pub const AF_INET_SDP: ::c_int = 40;
pub const AF_INET6_SDP: ::c_int = 42;

// sys/net/if.h
pub const IF_MAXUNIT: ::c_int = 0x7fff;
/// (n) interface is up
pub const IFF_UP: ::c_int = 0x1;
/// (i) broadcast address valid
pub const IFF_BROADCAST: ::c_int = 0x2;
/// (n) turn on debugging
pub const IFF_DEBUG: ::c_int = 0x4;
/// (i) is a loopback net
pub const IFF_LOOPBACK: ::c_int = 0x8;
/// (i) is a point-to-point link
pub const IFF_POINTOPOINT: ::c_int = 0x10;
/// (i) calls if_input in net epoch
pub const IFF_KNOWSEPOCH: ::c_int = 0x20;
/// (d) resources allocated
pub const IFF_RUNNING: ::c_int = 0x40;
#[doc(hidden)]
#[deprecated(
    since = "0.2.54",
    note = "IFF_DRV_RUNNING is deprecated. Use the portable IFF_RUNNING instead"
)]
/// (d) resources allocate
pub const IFF_DRV_RUNNING: ::c_int = 0x40;
/// (n) no address resolution protocol
pub const IFF_NOARP: ::c_int = 0x80;
/// (n) receive all packets
pub const IFF_PROMISC: ::c_int = 0x100;
/// (n) receive all multicast packets
pub const IFF_ALLMULTI: ::c_int = 0x200;
/// (d) tx hardware queue is full
pub const IFF_OACTIVE: ::c_int = 0x400;
#[doc(hidden)]
#[deprecated(since = "0.2.54", note = "Use the portable `IFF_OACTIVE` instead")]
/// (d) tx hardware queue is full
pub const IFF_DRV_OACTIVE: ::c_int = 0x400;
/// (i) can't hear own transmissions
pub const IFF_SIMPLEX: ::c_int = 0x800;
/// per link layer defined bit
pub const IFF_LINK0: ::c_int = 0x1000;
/// per link layer defined bit
pub const IFF_LINK1: ::c_int = 0x2000;
/// per link layer defined bit
pub const IFF_LINK2: ::c_int = 0x4000;
/// use alternate physical connection
pub const IFF_ALTPHYS: ::c_int = IFF_LINK2;
/// (i) supports multicast
pub const IFF_MULTICAST: ::c_int = 0x8000;
/// (i) unconfigurable using ioctl(2)
pub const IFF_CANTCONFIG: ::c_int = 0x10000;
/// (n) user-requested promisc mode
pub const IFF_PPROMISC: ::c_int = 0x20000;
/// (n) user-requested monitor mode
pub const IFF_MONITOR: ::c_int = 0x40000;
/// (n) static ARP
pub const IFF_STATICARP: ::c_int = 0x80000;
/// (n) interface is winding down
pub const IFF_DYING: ::c_int = 0x200000;
/// (n) interface is being renamed
pub const IFF_RENAMING: ::c_int = 0x400000;
/// interface is not part of any groups
pub const IFF_NOGROUP: ::c_int = 0x800000;

/// link invalid/unknown
pub const LINK_STATE_UNKNOWN: ::c_int = 0;
/// link is down
pub const LINK_STATE_DOWN: ::c_int = 1;
/// link is up
pub const LINK_STATE_UP: ::c_int = 2;

/// can offload checksum on RX
pub const IFCAP_RXCSUM: ::c_int = 0x00001;
/// can offload checksum on TX
pub const IFCAP_TXCSUM: ::c_int = 0x00002;
/// can be a network console
pub const IFCAP_NETCONS: ::c_int = 0x00004;
/// VLAN-compatible MTU
pub const IFCAP_VLAN_MTU: ::c_int = 0x00008;
/// hardware VLAN tag support
pub const IFCAP_VLAN_HWTAGGING: ::c_int = 0x00010;
/// 9000 byte MTU supported
pub const IFCAP_JUMBO_MTU: ::c_int = 0x00020;
/// driver supports polling
pub const IFCAP_POLLING: ::c_int = 0x00040;
/// can do IFCAP_HWCSUM on VLANs
pub const IFCAP_VLAN_HWCSUM: ::c_int = 0x00080;
/// can do TCP Segmentation Offload
pub const IFCAP_TSO4: ::c_int = 0x00100;
/// can do TCP6 Segmentation Offload
pub const IFCAP_TSO6: ::c_int = 0x00200;
/// can do Large Receive Offload
pub const IFCAP_LRO: ::c_int = 0x00400;
/// wake on any unicast frame
pub const IFCAP_WOL_UCAST: ::c_int = 0x00800;
/// wake on any multicast frame
pub const IFCAP_WOL_MCAST: ::c_int = 0x01000;
/// wake on any Magic Packet
pub const IFCAP_WOL_MAGIC: ::c_int = 0x02000;
/// interface can offload TCP
pub const IFCAP_TOE4: ::c_int = 0x04000;
/// interface can offload TCP6
pub const IFCAP_TOE6: ::c_int = 0x08000;
/// interface hw can filter vlan tag
pub const IFCAP_VLAN_HWFILTER: ::c_int = 0x10000;
/// can do IFCAP_TSO on VLANs
pub const IFCAP_VLAN_HWTSO: ::c_int = 0x40000;
/// the runtime link state is dynamic
pub const IFCAP_LINKSTATE: ::c_int = 0x80000;
/// netmap mode supported/enabled
pub const IFCAP_NETMAP: ::c_int = 0x100000;
/// can offload checksum on IPv6 RX
pub const IFCAP_RXCSUM_IPV6: ::c_int = 0x200000;
/// can offload checksum on IPv6 TX
pub const IFCAP_TXCSUM_IPV6: ::c_int = 0x400000;
/// manages counters internally
pub const IFCAP_HWSTATS: ::c_int = 0x800000;
/// hardware supports TX rate limiting
pub const IFCAP_TXRTLMT: ::c_int = 0x1000000;
/// hardware rx timestamping
pub const IFCAP_HWRXTSTMP: ::c_int = 0x2000000;
/// understands M_EXTPG mbufs
pub const IFCAP_MEXTPG: ::c_int = 0x4000000;
/// can do TLS encryption and segmentation for TCP
pub const IFCAP_TXTLS4: ::c_int = 0x8000000;
/// can do TLS encryption and segmentation for TCP6
pub const IFCAP_TXTLS6: ::c_int = 0x10000000;
/// can do IFCAN_HWCSUM on VXLANs
pub const IFCAP_VXLAN_HWCSUM: ::c_int = 0x20000000;
/// can do IFCAP_TSO on VXLANs
pub const IFCAP_VXLAN_HWTSO: ::c_int = 0x40000000;
/// can do TLS with rate limiting
pub const IFCAP_TXTLS_RTLMT: ::c_int = 0x80000000;

pub const IFCAP_HWCSUM_IPV6: ::c_int = IFCAP_RXCSUM_IPV6 | IFCAP_TXCSUM_IPV6;
pub const IFCAP_HWCSUM: ::c_int = IFCAP_RXCSUM | IFCAP_TXCSUM;
pub const IFCAP_TSO: ::c_int = IFCAP_TSO4 | IFCAP_TSO6;
pub const IFCAP_WOL: ::c_int = IFCAP_WOL_UCAST | IFCAP_WOL_MCAST | IFCAP_WOL_MAGIC;
pub const IFCAP_TOE: ::c_int = IFCAP_TOE4 | IFCAP_TOE6;
pub const IFCAP_TXTLS: ::c_int = IFCAP_TXTLS4 | IFCAP_TXTLS6;
pub const IFCAP_CANTCHANGE: ::c_int = IFCAP_NETMAP;

pub const IFQ_MAXLEN: ::c_int = 50;
pub const IFNET_SLOWHZ: ::c_int = 1;

pub const IFAN_ARRIVAL: ::c_int = 0;
pub const IFAN_DEPARTURE: ::c_int = 1;

pub const IFSTATMAX: ::c_int = 800;

pub const RSS_FUNC_NONE: ::c_int = 0;
pub const RSS_FUNC_PRIVATE: ::c_int = 1;
pub const RSS_FUNC_TOEPLITZ: ::c_int = 2;

pub const RSS_TYPE_IPV4: ::c_int = 0x00000001;
pub const RSS_TYPE_TCP_IPV4: ::c_int = 0x00000002;
pub const RSS_TYPE_IPV6: ::c_int = 0x00000004;
pub const RSS_TYPE_IPV6_EX: ::c_int = 0x00000008;
pub const RSS_TYPE_TCP_IPV6: ::c_int = 0x00000010;
pub const RSS_TYPE_TCP_IPV6_EX: ::c_int = 0x00000020;
pub const RSS_TYPE_UDP_IPV4: ::c_int = 0x00000040;
pub const RSS_TYPE_UDP_IPV6: ::c_int = 0x00000080;
pub const RSS_TYPE_UDP_IPV6_EX: ::c_int = 0x00000100;
pub const RSS_KEYLEN: ::c_int = 128;

pub const IFNET_PCP_NONE: ::c_int = 0xff;
pub const IFDR_MSG_SIZE: ::c_int = 64;
pub const IFDR_REASON_MSG: ::c_int = 1;
pub const IFDR_REASON_VENDOR: ::c_int = 2;

// sys/net/if_mib.h

/// non-interface-specific
pub const IFMIB_SYSTEM: ::c_int = 1;
/// per-interface data table
pub const IFMIB_IFDATA: ::c_int = 2;

/// generic stats for all kinds of ifaces
pub const IFDATA_GENERAL: ::c_int = 1;
/// specific to the type of interface
pub const IFDATA_LINKSPECIFIC: ::c_int = 2;
/// driver name and unit
pub const IFDATA_DRIVERNAME: ::c_int = 3;

/// number of interfaces configured
pub const IFMIB_IFCOUNT: ::c_int = 1;

/// functions not specific to a type of iface
pub const NETLINK_GENERIC: ::c_int = 0;

pub const DOT3COMPLIANCE_STATS: ::c_int = 1;
pub const DOT3COMPLIANCE_COLLS: ::c_int = 2;

pub const dot3ChipSetAMD7990: ::c_int = 1;
pub const dot3ChipSetAMD79900: ::c_int = 2;
pub const dot3ChipSetAMD79C940: ::c_int = 3;

pub const dot3ChipSetIntel82586: ::c_int = 1;
pub const dot3ChipSetIntel82596: ::c_int = 2;
pub const dot3ChipSetIntel82557: ::c_int = 3;

pub const dot3ChipSetNational8390: ::c_int = 1;
pub const dot3ChipSetNationalSonic: ::c_int = 2;

pub const dot3ChipSetFujitsu86950: ::c_int = 1;

pub const dot3ChipSetDigitalDC21040: ::c_int = 1;
pub const dot3ChipSetDigitalDC21140: ::c_int = 2;
pub const dot3ChipSetDigitalDC21041: ::c_int = 3;
pub const dot3ChipSetDigitalDC21140A: ::c_int = 4;
pub const dot3ChipSetDigitalDC21142: ::c_int = 5;

pub const dot3ChipSetWesternDigital83C690: ::c_int = 1;
pub const dot3ChipSetWesternDigital83C790: ::c_int = 2;

// sys/netinet/in.h
// Protocols (RFC 1700)
// NOTE: These are in addition to the constants defined in src/unix/mod.rs

// IPPROTO_IP defined in src/unix/mod.rs
/// IP6 hop-by-hop options
pub const IPPROTO_HOPOPTS: ::c_int = 0;
// IPPROTO_ICMP defined in src/unix/mod.rs
/// group mgmt protocol
pub const IPPROTO_IGMP: ::c_int = 2;
/// gateway^2 (deprecated)
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
#[doc(hidden)]
#[deprecated(
    since = "0.2.72",
    note = "IPPROTO_SEP is deprecated. Use IPPROTO_DCCP instead"
)]
pub const IPPROTO_SEP: ::c_int = 33;
/// Datagram Congestion Control Protocol
pub const IPPROTO_DCCP: ::c_int = 33;
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
/// IP Mobility
pub const IPPROTO_MOBILE: ::c_int = 55;
/// Transport Layer Security
pub const IPPROTO_TLSP: ::c_int = 56;
/// SKIP
pub const IPPROTO_SKIP: ::c_int = 57;
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
/// payload compression (IPComp)
pub const IPPROTO_IPCOMP: ::c_int = 108;
/// SCTP
pub const IPPROTO_SCTP: ::c_int = 132;
/// IPv6 Mobility Header
pub const IPPROTO_MH: ::c_int = 135;
/// UDP-Lite
pub const IPPROTO_UDPLITE: ::c_int = 136;
/// IP6 Host Identity Protocol
pub const IPPROTO_HIP: ::c_int = 139;
/// IP6 Shim6 Protocol
pub const IPPROTO_SHIM6: ::c_int = 140;

/* 101-254: Partly Unassigned */
/// Protocol Independent Mcast
pub const IPPROTO_PIM: ::c_int = 103;
/// CARP
pub const IPPROTO_CARP: ::c_int = 112;
/// PGM
pub const IPPROTO_PGM: ::c_int = 113;
/// MPLS-in-IP
pub const IPPROTO_MPLS: ::c_int = 137;
/// PFSYNC
pub const IPPROTO_PFSYNC: ::c_int = 240;

/* 255: Reserved */
/* BSD Private, local use, namespace incursion, no longer used */
/// OLD divert pseudo-proto
pub const IPPROTO_OLD_DIVERT: ::c_int = 254;
pub const IPPROTO_MAX: ::c_int = 256;
/// last return value of *_input(), meaning "all job for this pkt is done".
pub const IPPROTO_DONE: ::c_int = 257;

/* Only used internally, so can be outside the range of valid IP protocols. */
/// divert pseudo-protocol
pub const IPPROTO_DIVERT: ::c_int = 258;
/// SeND pseudo-protocol
pub const IPPROTO_SEND: ::c_int = 259;

// sys/netinet/TCP.h
pub const TCP_MD5SIG: ::c_int = 16;
pub const TCP_INFO: ::c_int = 32;
pub const TCP_CONGESTION: ::c_int = 64;
pub const TCP_CCALGOOPT: ::c_int = 65;
pub const TCP_KEEPINIT: ::c_int = 128;
pub const TCP_FASTOPEN: ::c_int = 1025;
pub const TCP_PCAP_OUT: ::c_int = 2048;
pub const TCP_PCAP_IN: ::c_int = 4096;

pub const IP_BINDANY: ::c_int = 24;
pub const IP_BINDMULTI: ::c_int = 25;
pub const IP_RSS_LISTEN_BUCKET: ::c_int = 26;
pub const IP_ORIGDSTADDR: ::c_int = 27;
pub const IP_RECVORIGDSTADDR: ::c_int = IP_ORIGDSTADDR;

pub const IP_RECVTOS: ::c_int = 68;

pub const IPV6_BINDANY: ::c_int = 64;
pub const IPV6_ORIGDSTADDR: ::c_int = 72;
pub const IPV6_RECVORIGDSTADDR: ::c_int = IPV6_ORIGDSTADDR;

pub const PF_SLOW: ::c_int = AF_SLOW;
pub const PF_SCLUSTER: ::c_int = AF_SCLUSTER;
pub const PF_ARP: ::c_int = AF_ARP;
pub const PF_BLUETOOTH: ::c_int = AF_BLUETOOTH;
pub const PF_IEEE80211: ::c_int = AF_IEEE80211;
pub const PF_INET_SDP: ::c_int = AF_INET_SDP;
pub const PF_INET6_SDP: ::c_int = AF_INET6_SDP;

pub const NET_RT_DUMP: ::c_int = 1;
pub const NET_RT_FLAGS: ::c_int = 2;
pub const NET_RT_IFLIST: ::c_int = 3;
pub const NET_RT_IFMALIST: ::c_int = 4;
pub const NET_RT_IFLISTL: ::c_int = 5;

// System V IPC
pub const IPC_INFO: ::c_int = 3;
pub const MSG_NOERROR: ::c_int = 0o10000;
pub const SHM_LOCK: ::c_int = 11;
pub const SHM_UNLOCK: ::c_int = 12;
pub const SHM_STAT: ::c_int = 13;
pub const SHM_INFO: ::c_int = 14;
pub const SHM_ANON: *mut ::c_char = 1 as *mut ::c_char;

// The *_MAXID constants never should've been used outside of the
// FreeBSD base system.  And with the exception of CTL_P1003_1B_MAXID,
// they were all removed in svn r262489.  They remain here for backwards
// compatibility only, and are scheduled to be removed in libc 1.0.0.
#[doc(hidden)]
#[deprecated(since = "0.2.54", note = "Removed in FreeBSD 11")]
pub const CTL_MAXID: ::c_int = 10;
#[doc(hidden)]
#[deprecated(since = "0.2.54", note = "Removed in FreeBSD 11")]
pub const KERN_MAXID: ::c_int = 38;
#[doc(hidden)]
#[deprecated(since = "0.2.54", note = "Removed in FreeBSD 11")]
pub const HW_MAXID: ::c_int = 13;
#[doc(hidden)]
#[deprecated(since = "0.2.54", note = "Removed in FreeBSD 11")]
pub const USER_MAXID: ::c_int = 21;
#[doc(hidden)]
#[deprecated(since = "0.2.74", note = "Removed in FreeBSD 13")]
pub const CTL_P1003_1B_MAXID: ::c_int = 26;

pub const MSG_NOTIFICATION: ::c_int = 0x00002000;
pub const MSG_NBIO: ::c_int = 0x00004000;
pub const MSG_COMPAT: ::c_int = 0x00008000;
pub const MSG_CMSG_CLOEXEC: ::c_int = 0x00040000;
pub const MSG_NOSIGNAL: ::c_int = 0x20000;

// utmpx entry types
pub const EMPTY: ::c_short = 0;
pub const BOOT_TIME: ::c_short = 1;
pub const OLD_TIME: ::c_short = 2;
pub const NEW_TIME: ::c_short = 3;
pub const USER_PROCESS: ::c_short = 4;
pub const INIT_PROCESS: ::c_short = 5;
pub const LOGIN_PROCESS: ::c_short = 6;
pub const DEAD_PROCESS: ::c_short = 7;
pub const SHUTDOWN_TIME: ::c_short = 8;
// utmp database types
pub const UTXDB_ACTIVE: ::c_int = 0;
pub const UTXDB_LASTLOGIN: ::c_int = 1;
pub const UTXDB_LOG: ::c_int = 2;

pub const LC_COLLATE_MASK: ::c_int = 1 << 0;
pub const LC_CTYPE_MASK: ::c_int = 1 << 1;
pub const LC_MONETARY_MASK: ::c_int = 1 << 2;
pub const LC_NUMERIC_MASK: ::c_int = 1 << 3;
pub const LC_TIME_MASK: ::c_int = 1 << 4;
pub const LC_MESSAGES_MASK: ::c_int = 1 << 5;
pub const LC_ALL_MASK: ::c_int = LC_COLLATE_MASK
    | LC_CTYPE_MASK
    | LC_MESSAGES_MASK
    | LC_MONETARY_MASK
    | LC_NUMERIC_MASK
    | LC_TIME_MASK;

pub const WSTOPPED: ::c_int = 2; // same as WUNTRACED
pub const WCONTINUED: ::c_int = 4;
pub const WNOWAIT: ::c_int = 8;
pub const WEXITED: ::c_int = 16;
pub const WTRAPPED: ::c_int = 32;

// FreeBSD defines a great many more of these, we only expose the
// standardized ones.
pub const P_PID: idtype_t = 0;
pub const P_PGID: idtype_t = 2;
pub const P_ALL: idtype_t = 7;

pub const UTIME_OMIT: c_long = -2;
pub const UTIME_NOW: c_long = -1;

pub const B460800: ::speed_t = 460800;
pub const B921600: ::speed_t = 921600;

pub const AT_FDCWD: ::c_int = -100;
pub const AT_EACCESS: ::c_int = 0x100;
pub const AT_SYMLINK_NOFOLLOW: ::c_int = 0x200;
pub const AT_SYMLINK_FOLLOW: ::c_int = 0x400;
pub const AT_REMOVEDIR: ::c_int = 0x800;

pub const AT_NULL: ::c_int = 0;
pub const AT_IGNORE: ::c_int = 1;
pub const AT_EXECFD: ::c_int = 2;
pub const AT_PHDR: ::c_int = 3;
pub const AT_PHENT: ::c_int = 4;
pub const AT_PHNUM: ::c_int = 5;
pub const AT_PAGESZ: ::c_int = 6;
pub const AT_BASE: ::c_int = 7;
pub const AT_FLAGS: ::c_int = 8;
pub const AT_ENTRY: ::c_int = 9;
pub const AT_NOTELF: ::c_int = 10;
pub const AT_UID: ::c_int = 11;
pub const AT_EUID: ::c_int = 12;
pub const AT_GID: ::c_int = 13;
pub const AT_EGID: ::c_int = 14;
pub const AT_EXECPATH: ::c_int = 15;

pub const TABDLY: ::tcflag_t = 0x00000004;
pub const TAB0: ::tcflag_t = 0x00000000;
pub const TAB3: ::tcflag_t = 0x00000004;

pub const _PC_ACL_NFS4: ::c_int = 64;

pub const _SC_CPUSET_SIZE: ::c_int = 122;

pub const _UUID_NODE_LEN: usize = 6;

// Flags which can be passed to pdfork(2)
pub const PD_DAEMON: ::c_int = 0x00000001;
pub const PD_CLOEXEC: ::c_int = 0x00000002;
pub const PD_ALLOWED_AT_FORK: ::c_int = PD_DAEMON | PD_CLOEXEC;

// Values for struct rtprio (type_ field)
pub const RTP_PRIO_REALTIME: ::c_ushort = 2;
pub const RTP_PRIO_NORMAL: ::c_ushort = 3;
pub const RTP_PRIO_IDLE: ::c_ushort = 4;

pub const POSIX_SPAWN_RESETIDS: ::c_int = 0x01;
pub const POSIX_SPAWN_SETPGROUP: ::c_int = 0x02;
pub const POSIX_SPAWN_SETSCHEDPARAM: ::c_int = 0x04;
pub const POSIX_SPAWN_SETSCHEDULER: ::c_int = 0x08;
pub const POSIX_SPAWN_SETSIGDEF: ::c_int = 0x10;
pub const POSIX_SPAWN_SETSIGMASK: ::c_int = 0x20;

// Flags for chflags(2)
pub const UF_SYSTEM: ::c_ulong = 0x00000080;
pub const UF_SPARSE: ::c_ulong = 0x00000100;
pub const UF_OFFLINE: ::c_ulong = 0x00000200;
pub const UF_REPARSE: ::c_ulong = 0x00000400;
pub const UF_ARCHIVE: ::c_ulong = 0x00000800;
pub const UF_READONLY: ::c_ulong = 0x00001000;
pub const UF_HIDDEN: ::c_ulong = 0x00008000;
pub const SF_SNAPSHOT: ::c_ulong = 0x00200000;

// fcntl commands
pub const F_ADD_SEALS: ::c_int = 19;
pub const F_DUP2FD: ::c_int = 10;
pub const F_DUP2FD_CLOEXEC: ::c_int = 18;
pub const F_GET_SEALS: ::c_int = 20;
pub const F_OGETLK: ::c_int = 7;
pub const F_OSETLK: ::c_int = 8;
pub const F_OSETLKW: ::c_int = 9;
pub const F_RDAHEAD: ::c_int = 16;
pub const F_READAHEAD: ::c_int = 15;
pub const F_SETLK_REMOTE: ::c_int = 14;

// for use with F_ADD_SEALS
pub const F_SEAL_GROW: ::c_int = 4;
pub const F_SEAL_SEAL: ::c_int = 1;
pub const F_SEAL_SHRINK: ::c_int = 2;
pub const F_SEAL_WRITE: ::c_int = 8;

// For getrandom()
pub const GRND_NONBLOCK: ::c_uint = 0x1;
pub const GRND_RANDOM: ::c_uint = 0x2;
pub const GRND_INSECURE: ::c_uint = 0x4;

// For realhostname* api
pub const HOSTNAME_FOUND: ::c_int = 0;
pub const HOSTNAME_INCORRECTNAME: ::c_int = 1;
pub const HOSTNAME_INVALIDADDR: ::c_int = 2;
pub const HOSTNAME_INVALIDNAME: ::c_int = 3;

// For rfork
pub const RFFDG: ::c_int = 4;
pub const RFPROC: ::c_int = 16;
pub const RFMEM: ::c_int = 32;
pub const RFNOWAIT: ::c_int = 64;
pub const RFCFDG: ::c_int = 4096;
pub const RFTHREAD: ::c_int = 8192;
pub const RFLINUXTHPN: ::c_int = 65536;
pub const RFTSIGZMB: ::c_int = 524288;
pub const RFSPAWN: ::c_int = 2147483648;

// For eventfd
pub const EFD_SEMAPHORE: ::c_int = 0x1;
pub const EFD_NONBLOCK: ::c_int = 0x4;
pub const EFD_CLOEXEC: ::c_int = 0x100000;

pub const MALLOCX_ZERO: ::c_int = 0x40;

/// size of returned wchan message
pub const WMESGLEN: usize = 8;
/// size of returned lock name
pub const LOCKNAMELEN: usize = 8;
/// size of returned thread name
pub const TDNAMLEN: usize = 16;
/// size of returned ki_comm name
pub const COMMLEN: usize = 19;
/// size of returned ki_emul
pub const KI_EMULNAMELEN: usize = 16;
/// number of groups in ki_groups
pub const KI_NGROUPS: usize = 16;
cfg_if! {
    if #[cfg(freebsd11)] {
        pub const KI_NSPARE_INT: usize = 4;
    } else {
        pub const KI_NSPARE_INT: usize = 2;
    }
}
pub const KI_NSPARE_LONG: usize = 12;
/// Flags for the process credential.
pub const KI_CRF_CAPABILITY_MODE: usize = 0x00000001;
/// Steal a bit from ki_cr_flags to indicate that the cred had more than
/// KI_NGROUPS groups.
pub const KI_CRF_GRP_OVERFLOW: usize = 0x80000000;
/// controlling tty vnode active
pub const KI_CTTY: usize = 0x00000001;
/// session leader
pub const KI_SLEADER: usize = 0x00000002;
/// proc blocked on lock ki_lockname
pub const KI_LOCKBLOCK: usize = 0x00000004;
/// size of returned ki_login
pub const LOGNAMELEN: usize = 17;
/// size of returned ki_loginclass
pub const LOGINCLASSLEN: usize = 17;

pub const KF_ATTR_VALID: ::c_int = 0x0001;
pub const KF_TYPE_NONE: ::c_int = 0;
pub const KF_TYPE_VNODE: ::c_int = 1;
pub const KF_TYPE_SOCKET: ::c_int = 2;
pub const KF_TYPE_PIPE: ::c_int = 3;
pub const KF_TYPE_FIFO: ::c_int = 4;
pub const KF_TYPE_KQUEUE: ::c_int = 5;
pub const KF_TYPE_MQUEUE: ::c_int = 7;
pub const KF_TYPE_SHM: ::c_int = 8;
pub const KF_TYPE_SEM: ::c_int = 9;
pub const KF_TYPE_PTS: ::c_int = 10;
pub const KF_TYPE_PROCDESC: ::c_int = 11;
pub const KF_TYPE_DEV: ::c_int = 12;
pub const KF_TYPE_UNKNOWN: ::c_int = 255;

pub const KF_VTYPE_VNON: ::c_int = 0;
pub const KF_VTYPE_VREG: ::c_int = 1;
pub const KF_VTYPE_VDIR: ::c_int = 2;
pub const KF_VTYPE_VBLK: ::c_int = 3;
pub const KF_VTYPE_VCHR: ::c_int = 4;
pub const KF_VTYPE_VLNK: ::c_int = 5;
pub const KF_VTYPE_VSOCK: ::c_int = 6;
pub const KF_VTYPE_VFIFO: ::c_int = 7;
pub const KF_VTYPE_VBAD: ::c_int = 8;
pub const KF_VTYPE_UNKNOWN: ::c_int = 255;

/// Current working directory
pub const KF_FD_TYPE_CWD: ::c_int = -1;
/// Root directory
pub const KF_FD_TYPE_ROOT: ::c_int = -2;
/// Jail directory
pub const KF_FD_TYPE_JAIL: ::c_int = -3;
/// Ktrace vnode
pub const KF_FD_TYPE_TRACE: ::c_int = -4;
pub const KF_FD_TYPE_TEXT: ::c_int = -5;
/// Controlling terminal
pub const KF_FD_TYPE_CTTY: ::c_int = -6;
pub const KF_FLAG_READ: ::c_int = 0x00000001;
pub const KF_FLAG_WRITE: ::c_int = 0x00000002;
pub const KF_FLAG_APPEND: ::c_int = 0x00000004;
pub const KF_FLAG_ASYNC: ::c_int = 0x00000008;
pub const KF_FLAG_FSYNC: ::c_int = 0x00000010;
pub const KF_FLAG_NONBLOCK: ::c_int = 0x00000020;
pub const KF_FLAG_DIRECT: ::c_int = 0x00000040;
pub const KF_FLAG_HASLOCK: ::c_int = 0x00000080;
pub const KF_FLAG_SHLOCK: ::c_int = 0x00000100;
pub const KF_FLAG_EXLOCK: ::c_int = 0x00000200;
pub const KF_FLAG_NOFOLLOW: ::c_int = 0x00000400;
pub const KF_FLAG_CREAT: ::c_int = 0x00000800;
pub const KF_FLAG_TRUNC: ::c_int = 0x00001000;
pub const KF_FLAG_EXCL: ::c_int = 0x00002000;
pub const KF_FLAG_EXEC: ::c_int = 0x00004000;

pub const KVME_TYPE_NONE: ::c_int = 0;
pub const KVME_TYPE_DEFAULT: ::c_int = 1;
pub const KVME_TYPE_VNODE: ::c_int = 2;
pub const KVME_TYPE_SWAP: ::c_int = 3;
pub const KVME_TYPE_DEVICE: ::c_int = 4;
pub const KVME_TYPE_PHYS: ::c_int = 5;
pub const KVME_TYPE_DEAD: ::c_int = 6;
pub const KVME_TYPE_SG: ::c_int = 7;
pub const KVME_TYPE_MGTDEVICE: ::c_int = 8;
// Present in `sys/user.h` but is undefined for whatever reason...
// pub const KVME_TYPE_GUARD: ::c_int = 9;
pub const KVME_TYPE_UNKNOWN: ::c_int = 255;
pub const KVME_PROT_READ: ::c_int = 0x00000001;
pub const KVME_PROT_WRITE: ::c_int = 0x00000002;
pub const KVME_PROT_EXEC: ::c_int = 0x00000004;
pub const KVME_FLAG_COW: ::c_int = 0x00000001;
pub const KVME_FLAG_NEEDS_COPY: ::c_int = 0x00000002;
pub const KVME_FLAG_NOCOREDUMP: ::c_int = 0x00000004;
pub const KVME_FLAG_SUPER: ::c_int = 0x00000008;
pub const KVME_FLAG_GROWS_UP: ::c_int = 0x00000010;
pub const KVME_FLAG_GROWS_DOWN: ::c_int = 0x00000020;
pub const KVME_FLAG_USER_WIRED: ::c_int = 0x00000040;

pub const KKST_MAXLEN: ::c_int = 1024;
/// Stack is valid.
pub const KKST_STATE_STACKOK: ::c_int = 0;
/// Stack swapped out.
pub const KKST_STATE_SWAPPED: ::c_int = 1;
pub const KKST_STATE_RUNNING: ::c_int = 2;

// Constants about priority.
pub const PRI_MIN: ::c_int = 0;
pub const PRI_MAX: ::c_int = 255;
pub const PRI_MIN_ITHD: ::c_int = PRI_MIN;
pub const PRI_MAX_ITHD: ::c_int = PRI_MIN_REALTIME - 1;
pub const PI_REALTIME: ::c_int = PRI_MIN_ITHD + 0;
pub const PI_AV: ::c_int = PRI_MIN_ITHD + 4;
pub const PI_NET: ::c_int = PRI_MIN_ITHD + 8;
pub const PI_DISK: ::c_int = PRI_MIN_ITHD + 12;
pub const PI_TTY: ::c_int = PRI_MIN_ITHD + 16;
pub const PI_DULL: ::c_int = PRI_MIN_ITHD + 20;
pub const PI_SOFT: ::c_int = PRI_MIN_ITHD + 24;
pub const PRI_MIN_REALTIME: ::c_int = 48;
pub const PRI_MAX_REALTIME: ::c_int = PRI_MIN_KERN - 1;
pub const PRI_MIN_KERN: ::c_int = 80;
pub const PRI_MAX_KERN: ::c_int = PRI_MIN_TIMESHARE - 1;
pub const PSWP: ::c_int = PRI_MIN_KERN + 0;
pub const PVM: ::c_int = PRI_MIN_KERN + 4;
pub const PINOD: ::c_int = PRI_MIN_KERN + 8;
pub const PRIBIO: ::c_int = PRI_MIN_KERN + 12;
pub const PVFS: ::c_int = PRI_MIN_KERN + 16;
pub const PZERO: ::c_int = PRI_MIN_KERN + 20;
pub const PSOCK: ::c_int = PRI_MIN_KERN + 24;
pub const PWAIT: ::c_int = PRI_MIN_KERN + 28;
pub const PLOCK: ::c_int = PRI_MIN_KERN + 32;
pub const PPAUSE: ::c_int = PRI_MIN_KERN + 36;
pub const PRI_MIN_TIMESHARE: ::c_int = 120;
pub const PRI_MAX_TIMESHARE: ::c_int = PRI_MIN_IDLE - 1;
pub const PUSER: ::c_int = PRI_MIN_TIMESHARE;
pub const PRI_MIN_IDLE: ::c_int = 224;
pub const PRI_MAX_IDLE: ::c_int = PRI_MAX;

pub const NZERO: ::c_int = 0;

// Resource utilization information.
pub const RUSAGE_THREAD: ::c_int = 1;

cfg_if! {
    if #[cfg(any(freebsd11, target_pointer_width = "32"))] {
        pub const ARG_MAX: ::c_int = 256 * 1024;
    } else {
        pub const ARG_MAX: ::c_int = 2 * 256 * 1024;
    }
}
pub const CHILD_MAX: ::c_int = 40;
/// max command name remembered
pub const MAXCOMLEN: usize = 19;
/// max interpreter file name length
pub const MAXINTERP: ::c_int = ::PATH_MAX;
/// max login name length (incl. NUL)
pub const MAXLOGNAME: ::c_int = 33;
/// max simultaneous processes
pub const MAXUPRC: ::c_int = CHILD_MAX;
/// max bytes for an exec function
pub const NCARGS: ::c_int = ARG_MAX;
///  /* max number groups
pub const NGROUPS: ::c_int = NGROUPS_MAX + 1;
/// max open files per process
pub const NOFILE: ::c_int = OPEN_MAX;
/// marker for empty group set member
pub const NOGROUP: ::c_int = 65535;
/// max hostname size
pub const MAXHOSTNAMELEN: ::c_int = 256;
/// max bytes in term canon input line
pub const MAX_CANON: ::c_int = 255;
/// max bytes in terminal input
pub const MAX_INPUT: ::c_int = 255;
/// max bytes in a file name
pub const NAME_MAX: ::c_int = 255;
pub const MAXSYMLINKS: ::c_int = 32;
/// max supplemental group id's
pub const NGROUPS_MAX: ::c_int = 1023;
/// max open files per process
pub const OPEN_MAX: ::c_int = 64;

pub const _POSIX_ARG_MAX: ::c_int = 4096;
pub const _POSIX_LINK_MAX: ::c_int = 8;
pub const _POSIX_MAX_CANON: ::c_int = 255;
pub const _POSIX_MAX_INPUT: ::c_int = 255;
pub const _POSIX_NAME_MAX: ::c_int = 14;
pub const _POSIX_PIPE_BUF: ::c_int = 512;
pub const _POSIX_SSIZE_MAX: ::c_int = 32767;
pub const _POSIX_STREAM_MAX: ::c_int = 8;

/// max ibase/obase values in bc(1)
pub const BC_BASE_MAX: ::c_int = 99;
/// max array elements in bc(1)
pub const BC_DIM_MAX: ::c_int = 2048;
/// max scale value in bc(1)
pub const BC_SCALE_MAX: ::c_int = 99;
/// max const string length in bc(1)
pub const BC_STRING_MAX: ::c_int = 1000;
/// max character class name size
pub const CHARCLASS_NAME_MAX: ::c_int = 14;
/// max weights for order keyword
pub const COLL_WEIGHTS_MAX: ::c_int = 10;
/// max expressions nested in expr(1)
pub const EXPR_NEST_MAX: ::c_int = 32;
/// max bytes in an input line
pub const LINE_MAX: ::c_int = 2048;
/// max RE's in interval notation
pub const RE_DUP_MAX: ::c_int = 255;

pub const _POSIX2_BC_BASE_MAX: ::c_int = 99;
pub const _POSIX2_BC_DIM_MAX: ::c_int = 2048;
pub const _POSIX2_BC_SCALE_MAX: ::c_int = 99;
pub const _POSIX2_BC_STRING_MAX: ::c_int = 1000;
pub const _POSIX2_CHARCLASS_NAME_MAX: ::c_int = 14;
pub const _POSIX2_COLL_WEIGHTS_MAX: ::c_int = 2;
pub const _POSIX2_EQUIV_CLASS_MAX: ::c_int = 2;
pub const _POSIX2_EXPR_NEST_MAX: ::c_int = 32;
pub const _POSIX2_LINE_MAX: ::c_int = 2048;
pub const _POSIX2_RE_DUP_MAX: ::c_int = 255;

// sys/proc.h
pub const TDF_BORROWING: ::c_int = 0x00000001;
pub const TDF_INPANIC: ::c_int = 0x00000002;
pub const TDF_INMEM: ::c_int = 0x00000004;
pub const TDF_SINTR: ::c_int = 0x00000008;
pub const TDF_TIMEOUT: ::c_int = 0x00000010;
pub const TDF_IDLETD: ::c_int = 0x00000020;
pub const TDF_CANSWAP: ::c_int = 0x00000040;
pub const TDF_KTH_SUSP: ::c_int = 0x00000100;
pub const TDF_ALLPROCSUSP: ::c_int = 0x00000200;
pub const TDF_BOUNDARY: ::c_int = 0x00000400;
pub const TDF_ASTPENDING: ::c_int = 0x00000800;
pub const TDF_SBDRY: ::c_int = 0x00002000;
pub const TDF_UPIBLOCKED: ::c_int = 0x00004000;
pub const TDF_NEEDSUSPCHK: ::c_int = 0x00008000;
pub const TDF_NEEDRESCHED: ::c_int = 0x00010000;
pub const TDF_NEEDSIGCHK: ::c_int = 0x00020000;
pub const TDF_NOLOAD: ::c_int = 0x00040000;
pub const TDF_SERESTART: ::c_int = 0x00080000;
pub const TDF_THRWAKEUP: ::c_int = 0x00100000;
pub const TDF_SEINTR: ::c_int = 0x00200000;
pub const TDF_SWAPINREQ: ::c_int = 0x00400000;
pub const TDF_UNUSED23: ::c_int = 0x00800000;
pub const TDF_SCHED0: ::c_int = 0x01000000;
pub const TDF_SCHED1: ::c_int = 0x02000000;
pub const TDF_SCHED2: ::c_int = 0x04000000;
pub const TDF_SCHED3: ::c_int = 0x08000000;
pub const TDF_ALRMPEND: ::c_int = 0x10000000;
pub const TDF_PROFPEND: ::c_int = 0x20000000;
pub const TDF_MACPEND: ::c_int = 0x40000000;

pub const TDB_SUSPEND: ::c_int = 0x00000001;
pub const TDB_XSIG: ::c_int = 0x00000002;
pub const TDB_USERWR: ::c_int = 0x00000004;
pub const TDB_SCE: ::c_int = 0x00000008;
pub const TDB_SCX: ::c_int = 0x00000010;
pub const TDB_EXEC: ::c_int = 0x00000020;
pub const TDB_FORK: ::c_int = 0x00000040;
pub const TDB_STOPATFORK: ::c_int = 0x00000080;
pub const TDB_CHILD: ::c_int = 0x00000100;
pub const TDB_BORN: ::c_int = 0x00000200;
pub const TDB_EXIT: ::c_int = 0x00000400;
pub const TDB_VFORK: ::c_int = 0x00000800;
pub const TDB_FSTP: ::c_int = 0x00001000;
pub const TDB_STEP: ::c_int = 0x00002000;

pub const TDP_OLDMASK: ::c_int = 0x00000001;
pub const TDP_INKTR: ::c_int = 0x00000002;
pub const TDP_INKTRACE: ::c_int = 0x00000004;
pub const TDP_BUFNEED: ::c_int = 0x00000008;
pub const TDP_COWINPROGRESS: ::c_int = 0x00000010;
pub const TDP_ALTSTACK: ::c_int = 0x00000020;
pub const TDP_DEADLKTREAT: ::c_int = 0x00000040;
pub const TDP_NOFAULTING: ::c_int = 0x00000080;
pub const TDP_OWEUPC: ::c_int = 0x00000200;
pub const TDP_ITHREAD: ::c_int = 0x00000400;
pub const TDP_SYNCIO: ::c_int = 0x00000800;
pub const TDP_SCHED1: ::c_int = 0x00001000;
pub const TDP_SCHED2: ::c_int = 0x00002000;
pub const TDP_SCHED3: ::c_int = 0x00004000;
pub const TDP_SCHED4: ::c_int = 0x00008000;
pub const TDP_GEOM: ::c_int = 0x00010000;
pub const TDP_SOFTDEP: ::c_int = 0x00020000;
pub const TDP_NORUNNINGBUF: ::c_int = 0x00040000;
pub const TDP_WAKEUP: ::c_int = 0x00080000;
pub const TDP_INBDFLUSH: ::c_int = 0x00100000;
pub const TDP_KTHREAD: ::c_int = 0x00200000;
pub const TDP_CALLCHAIN: ::c_int = 0x00400000;
pub const TDP_IGNSUSP: ::c_int = 0x00800000;
pub const TDP_AUDITREC: ::c_int = 0x01000000;
pub const TDP_RFPPWAIT: ::c_int = 0x02000000;
pub const TDP_RESETSPUR: ::c_int = 0x04000000;
pub const TDP_NERRNO: ::c_int = 0x08000000;
pub const TDP_EXECVMSPC: ::c_int = 0x40000000;

pub const TDI_SUSPENDED: ::c_int = 0x0001;
pub const TDI_SLEEPING: ::c_int = 0x0002;
pub const TDI_SWAPPED: ::c_int = 0x0004;
pub const TDI_LOCK: ::c_int = 0x0008;
pub const TDI_IWAIT: ::c_int = 0x0010;

pub const P_ADVLOCK: ::c_int = 0x00000001;
pub const P_CONTROLT: ::c_int = 0x00000002;
pub const P_KPROC: ::c_int = 0x00000004;
pub const P_UNUSED3: ::c_int = 0x00000008;
pub const P_PPWAIT: ::c_int = 0x00000010;
pub const P_PROFIL: ::c_int = 0x00000020;
pub const P_STOPPROF: ::c_int = 0x00000040;
pub const P_HADTHREADS: ::c_int = 0x00000080;
pub const P_SUGID: ::c_int = 0x00000100;
pub const P_SYSTEM: ::c_int = 0x00000200;
pub const P_SINGLE_EXIT: ::c_int = 0x00000400;
pub const P_TRACED: ::c_int = 0x00000800;
pub const P_WAITED: ::c_int = 0x00001000;
pub const P_WEXIT: ::c_int = 0x00002000;
pub const P_EXEC: ::c_int = 0x00004000;
pub const P_WKILLED: ::c_int = 0x00008000;
pub const P_CONTINUED: ::c_int = 0x00010000;
pub const P_STOPPED_SIG: ::c_int = 0x00020000;
pub const P_STOPPED_TRACE: ::c_int = 0x00040000;
pub const P_STOPPED_SINGLE: ::c_int = 0x00080000;
pub const P_PROTECTED: ::c_int = 0x00100000;
pub const P_SIGEVENT: ::c_int = 0x00200000;
pub const P_SINGLE_BOUNDARY: ::c_int = 0x00400000;
pub const P_HWPMC: ::c_int = 0x00800000;
pub const P_JAILED: ::c_int = 0x01000000;
pub const P_TOTAL_STOP: ::c_int = 0x02000000;
pub const P_INEXEC: ::c_int = 0x04000000;
pub const P_STATCHILD: ::c_int = 0x08000000;
pub const P_INMEM: ::c_int = 0x10000000;
pub const P_SWAPPINGOUT: ::c_int = 0x20000000;
pub const P_SWAPPINGIN: ::c_int = 0x40000000;
pub const P_PPTRACE: ::c_int = 0x80000000;
pub const P_STOPPED: ::c_int = P_STOPPED_SIG | P_STOPPED_SINGLE | P_STOPPED_TRACE;

pub const P2_INHERIT_PROTECTED: ::c_int = 0x00000001;
pub const P2_NOTRACE: ::c_int = 0x00000002;
pub const P2_NOTRACE_EXEC: ::c_int = 0x00000004;
pub const P2_AST_SU: ::c_int = 0x00000008;
pub const P2_PTRACE_FSTP: ::c_int = 0x00000010;
pub const P2_TRAPCAP: ::c_int = 0x00000020;
pub const P2_STKGAP_DISABLE: ::c_int = 0x00000800;
pub const P2_STKGAP_DISABLE_EXEC: ::c_int = 0x00001000;

pub const P_TREE_ORPHANED: ::c_int = 0x00000001;
pub const P_TREE_FIRST_ORPHAN: ::c_int = 0x00000002;
pub const P_TREE_REAPER: ::c_int = 0x00000004;

pub const SIDL: ::c_char = 1;
pub const SRUN: ::c_char = 2;
pub const SSLEEP: ::c_char = 3;
pub const SSTOP: ::c_char = 4;
pub const SZOMB: ::c_char = 5;
pub const SWAIT: ::c_char = 6;
pub const SLOCK: ::c_char = 7;

pub const P_MAGIC: ::c_int = 0xbeefface;

pub const TDP_SIGFASTBLOCK: ::c_int = 0x00000100;
pub const TDP_UIOHELD: ::c_int = 0x10000000;
pub const TDP_SIGFASTPENDING: ::c_int = 0x80000000;
pub const TDP2_COMPAT32RB: ::c_int = 0x00000002;
pub const P2_PROTMAX_ENABLE: ::c_int = 0x00000200;
pub const P2_PROTMAX_DISABLE: ::c_int = 0x00000400;
pub const TDP2_SBPAGES: ::c_int = 0x00000001;
pub const P2_ASLR_ENABLE: ::c_int = 0x00000040;
pub const P2_ASLR_DISABLE: ::c_int = 0x00000080;
pub const P2_ASLR_IGNSTART: ::c_int = 0x00000100;
pub const P_TREE_GRPEXITED: ::c_int = 0x00000008;

// libprocstat.h
pub const PS_FST_VTYPE_VNON: ::c_int = 1;
pub const PS_FST_VTYPE_VREG: ::c_int = 2;
pub const PS_FST_VTYPE_VDIR: ::c_int = 3;
pub const PS_FST_VTYPE_VBLK: ::c_int = 4;
pub const PS_FST_VTYPE_VCHR: ::c_int = 5;
pub const PS_FST_VTYPE_VLNK: ::c_int = 6;
pub const PS_FST_VTYPE_VSOCK: ::c_int = 7;
pub const PS_FST_VTYPE_VFIFO: ::c_int = 8;
pub const PS_FST_VTYPE_VBAD: ::c_int = 9;
pub const PS_FST_VTYPE_UNKNOWN: ::c_int = 255;

pub const PS_FST_TYPE_VNODE: ::c_int = 1;
pub const PS_FST_TYPE_FIFO: ::c_int = 2;
pub const PS_FST_TYPE_SOCKET: ::c_int = 3;
pub const PS_FST_TYPE_PIPE: ::c_int = 4;
pub const PS_FST_TYPE_PTS: ::c_int = 5;
pub const PS_FST_TYPE_KQUEUE: ::c_int = 6;
pub const PS_FST_TYPE_MQUEUE: ::c_int = 8;
pub const PS_FST_TYPE_SHM: ::c_int = 9;
pub const PS_FST_TYPE_SEM: ::c_int = 10;
pub const PS_FST_TYPE_UNKNOWN: ::c_int = 11;
pub const PS_FST_TYPE_NONE: ::c_int = 12;
pub const PS_FST_TYPE_PROCDESC: ::c_int = 13;
pub const PS_FST_TYPE_DEV: ::c_int = 14;
pub const PS_FST_TYPE_EVENTFD: ::c_int = 15;

pub const PS_FST_UFLAG_RDIR: ::c_int = 0x0001;
pub const PS_FST_UFLAG_CDIR: ::c_int = 0x0002;
pub const PS_FST_UFLAG_JAIL: ::c_int = 0x0004;
pub const PS_FST_UFLAG_TRACE: ::c_int = 0x0008;
pub const PS_FST_UFLAG_TEXT: ::c_int = 0x0010;
pub const PS_FST_UFLAG_MMAP: ::c_int = 0x0020;
pub const PS_FST_UFLAG_CTTY: ::c_int = 0x0040;

pub const PS_FST_FFLAG_READ: ::c_int = 0x0001;
pub const PS_FST_FFLAG_WRITE: ::c_int = 0x0002;
pub const PS_FST_FFLAG_NONBLOCK: ::c_int = 0x0004;
pub const PS_FST_FFLAG_APPEND: ::c_int = 0x0008;
pub const PS_FST_FFLAG_SHLOCK: ::c_int = 0x0010;
pub const PS_FST_FFLAG_EXLOCK: ::c_int = 0x0020;
pub const PS_FST_FFLAG_ASYNC: ::c_int = 0x0040;
pub const PS_FST_FFLAG_SYNC: ::c_int = 0x0080;
pub const PS_FST_FFLAG_NOFOLLOW: ::c_int = 0x0100;
pub const PS_FST_FFLAG_CREAT: ::c_int = 0x0200;
pub const PS_FST_FFLAG_TRUNC: ::c_int = 0x0400;
pub const PS_FST_FFLAG_EXCL: ::c_int = 0x0800;
pub const PS_FST_FFLAG_DIRECT: ::c_int = 0x1000;
pub const PS_FST_FFLAG_EXEC: ::c_int = 0x2000;
pub const PS_FST_FFLAG_HASLOCK: ::c_int = 0x4000;

// sys/mount.h

/// File identifier.
/// These are unique per filesystem on a single machine.
///
/// Note that the offset of fid_data is 4 bytes, so care must be taken to avoid
/// undefined behavior accessing unaligned fields within an embedded struct.
pub const MAXFIDSZ: ::c_int = 16;
/// Length of type name including null.
pub const MFSNAMELEN: ::c_int = 16;
cfg_if! {
    if #[cfg(any(freebsd10, freebsd11))] {
        /// Size of on/from name bufs.
        pub const MNAMELEN: ::c_int = 88;
    } else {
        /// Size of on/from name bufs.
        pub const MNAMELEN: ::c_int = 1024;
    }
}

/// Using journaled soft updates.
pub const MNT_SUJ: u64 = 0x100000000;
/// Mounted by automountd(8).
pub const MNT_AUTOMOUNTED: u64 = 0x200000000;
/// Filesys metadata untrusted.
pub const MNT_UNTRUSTED: u64 = 0x800000000;

/// Require TLS.
pub const MNT_EXTLS: u64 = 0x4000000000;
/// Require TLS with client cert.
pub const MNT_EXTLSCERT: u64 = 0x8000000000;
/// Require TLS with user cert.
pub const MNT_EXTLSCERTUSER: u64 = 0x10000000000;

/// Filesystem is stored locally.
pub const MNT_LOCAL: u64 = 0x000001000;
/// Quotas are enabled on fs.
pub const MNT_QUOTA: u64 = 0x000002000;
/// Identifies the root fs.
pub const MNT_ROOTFS: u64 = 0x000004000;
/// Mounted by a user.
pub const MNT_USER: u64 = 0x000008000;
/// Do not show entry in df.
pub const MNT_IGNORE: u64 = 0x000800000;
/// Filesystem is verified.
pub const MNT_VERIFIED: u64 = 0x400000000;

/// Do not cover a mount point.
pub const MNT_NOCOVER: u64 = 0x001000000000;
/// Only mount on empty dir.
pub const MNT_EMPTYDIR: u64 = 0x002000000000;
/// Recursively unmount uppers.
pub const MNT_RECURSE: u64 = 0x100000000000;
/// Unmount in async context.
pub const MNT_DEFERRED: u64 = 0x200000000000;

/// Forced unmount in progress.
pub const MNTK_UNMOUNTF: u32 = 0x00000001;
/// Filtered async flag.
pub const MNTK_ASYNC: u32 = 0x00000002;
/// Async disabled by softdep.
pub const MNTK_SOFTDEP: u32 = 0x00000004;
/// Don't do msync.
pub const MNTK_NOMSYNC: u32 = 0x00000008;
/// Lock draining is happening.
pub const MNTK_DRAINING: u32 = 0x00000010;
/// Refcount expiring is happening.
pub const MNTK_REFEXPIRE: u32 = 0x00000020;
/// Allow shared locking for more ops.
pub const MNTK_EXTENDED_SHARED: u32 = 0x00000040;
/// Allow shared locking for writes.
pub const MNTK_SHARED_WRITES: u32 = 0x00000080;
/// Disallow page faults during reads and writes. Filesystem shall properly handle i/o
/// state on EFAULT.
pub const MNTK_NO_IOPF: u32 = 0x00000100;
/// Pending recursive unmount.
pub const MNTK_RECURSE: u32 = 0x00000200;
/// Waiting to drain MNTK_UPPER_PENDING.
pub const MNTK_UPPER_WAITER: u32 = 0x00000400;
pub const MNTK_LOOKUP_EXCL_DOTDOT: u32 = 0x00000800;
pub const MNTK_UNMAPPED_BUFS: u32 = 0x00002000;
/// FS uses the buffer cache.
pub const MNTK_USES_BCACHE: u32 = 0x00004000;
/// Keep use ref for text.
pub const MNTK_TEXT_REFS: u32 = 0x00008000;
pub const MNTK_VMSETSIZE_BUG: u32 = 0x00010000;
/// A hack for F_ISUNIONSTACK.
pub const MNTK_UNIONFS: u32 = 0x00020000;
/// fast path lookup is supported.
pub const MNTK_FPLOOKUP: u32 = 0x00040000;
/// Suspended by all-fs suspension.
pub const MNTK_SUSPEND_ALL: u32 = 0x00080000;
/// Waiting on unmount taskqueue.
pub const MNTK_TASKQUEUE_WAITER: u32 = 0x00100000;
/// Disable async.
pub const MNTK_NOASYNC: u32 = 0x00800000;
/// Unmount in progress.
pub const MNTK_UNMOUNT: u32 = 0x01000000;
/// Waiting for unmount to finish.
pub const MNTK_MWAIT: u32 = 0x02000000;
/// Request write suspension.
pub const MNTK_SUSPEND: u32 = 0x08000000;
/// Block secondary writes.
pub const MNTK_SUSPEND2: u32 = 0x04000000;
/// Write operations are suspended.
pub const MNTK_SUSPENDED: u32 = 0x10000000;
/// auto disable cache for nullfs mounts over this fs.
pub const MNTK_NULL_NOCACHE: u32 = 0x20000000;
/// FS supports shared lock lookups.
pub const MNTK_LOOKUP_SHARED: u32 = 0x40000000;
/// Don't send KNOTEs from VOP hooks.
pub const MNTK_NOKNOTE: u32 = 0x80000000;

/// Get configured filesystems.
pub const VFS_VFSCONF: ::c_int = 0;
/// Generic filesystem information.
pub const VFS_GENERIC: ::c_int = 0;

/// int: highest defined filesystem type.
pub const VFS_MAXTYPENUM: ::c_int = 1;
/// struct: vfsconf for filesystem given as next argument.
pub const VFS_CONF: ::c_int = 2;

/// Synchronously wait for I/O to complete.
pub const MNT_WAIT: ::c_int = 1;
/// Start all I/O, but do not wait for it.
pub const MNT_NOWAIT: ::c_int = 2;
/// Push data not written by filesystem syncer.
pub const MNT_LAZY: ::c_int = 3;
/// Suspend file system after sync.
pub const MNT_SUSPEND: ::c_int = 4;

pub const MAXSECFLAVORS: ::c_int = 5;

/// Statically compiled into kernel.
pub const VFCF_STATIC: ::c_int = 0x00010000;
/// May get data over the network.
pub const VFCF_NETWORK: ::c_int = 0x00020000;
/// Writes are not implemented.
pub const VFCF_READONLY: ::c_int = 0x00040000;
/// Data does not represent real files.
pub const VFCF_SYNTHETIC: ::c_int = 0x00080000;
/// Aliases some other mounted FS.
pub const VFCF_LOOPBACK: ::c_int = 0x00100000;
/// Stores file names as Unicode.
pub const VFCF_UNICODE: ::c_int = 0x00200000;
/// Can be mounted from within a jail.
pub const VFCF_JAIL: ::c_int = 0x00400000;
/// Supports delegated administration.
pub const VFCF_DELEGADMIN: ::c_int = 0x00800000;
/// Stop at Boundary: defer stop requests to kernel->user (AST) transition.
pub const VFCF_SBDRY: ::c_int = 0x01000000;

// time.h

/// not on dst
pub const DST_NONE: ::c_int = 0;
/// USA style dst
pub const DST_USA: ::c_int = 1;
/// Australian style dst
pub const DST_AUST: ::c_int = 2;
/// Western European dst
pub const DST_WET: ::c_int = 3;
/// Middle European dst
pub const DST_MET: ::c_int = 4;
/// Eastern European dst
pub const DST_EET: ::c_int = 5;
/// Canada
pub const DST_CAN: ::c_int = 6;

pub const CPUCLOCK_WHICH_PID: ::c_int = 0;
pub const CPUCLOCK_WHICH_TID: ::c_int = 1;

const_fn! {
    {const} fn _ALIGN(p: usize) -> usize {
        (p + _ALIGNBYTES) & !_ALIGNBYTES
    }
}

f! {
    pub fn CMSG_DATA(cmsg: *const ::cmsghdr) -> *mut ::c_uchar {
        (cmsg as *mut ::c_uchar)
            .offset(_ALIGN(::mem::size_of::<::cmsghdr>()) as isize)
    }

    pub fn CMSG_LEN(length: ::c_uint) -> ::c_uint {
        _ALIGN(::mem::size_of::<::cmsghdr>()) as ::c_uint + length
    }

    pub fn CMSG_NXTHDR(mhdr: *const ::msghdr, cmsg: *const ::cmsghdr)
        -> *mut ::cmsghdr
    {
        if cmsg.is_null() {
            return ::CMSG_FIRSTHDR(mhdr);
        };
        let next = cmsg as usize + _ALIGN((*cmsg).cmsg_len as usize)
            + _ALIGN(::mem::size_of::<::cmsghdr>());
        let max = (*mhdr).msg_control as usize
            + (*mhdr).msg_controllen as usize;
        if next > max {
            0 as *mut ::cmsghdr
        } else {
            (cmsg as usize + _ALIGN((*cmsg).cmsg_len as usize))
                as *mut ::cmsghdr
        }
    }

    pub {const} fn CMSG_SPACE(length: ::c_uint) -> ::c_uint {
        (_ALIGN(::mem::size_of::<::cmsghdr>()) + _ALIGN(length as usize))
            as ::c_uint
    }

    pub fn MALLOCX_ALIGN(lg: ::c_uint) -> ::c_int {
        ffsl(lg as ::c_long - 1)
    }

    pub {const} fn MALLOCX_TCACHE(tc: ::c_int) -> ::c_int {
        (tc + 2) << 8 as ::c_int
    }

    pub {const} fn MALLOCX_ARENA(a: ::c_int) -> ::c_int {
        (a + 1) << 20 as ::c_int
    }

    pub fn SOCKCREDSIZE(ngrps: usize) -> usize {
        let ngrps = if ngrps > 0 {
            ngrps - 1
        } else {
            0
        };
        ::mem::size_of::<sockcred>() + ::mem::size_of::<::gid_t>() * ngrps
    }

    pub fn uname(buf: *mut ::utsname) -> ::c_int {
        __xuname(256, buf as *mut ::c_void)
    }

    pub fn CPU_ZERO(cpuset: &mut cpuset_t) -> () {
        for slot in cpuset.__bits.iter_mut() {
            *slot = 0;
        }
    }

    pub fn CPU_FILL(cpuset: &mut cpuset_t) -> () {
        for slot in cpuset.__bits.iter_mut() {
            *slot = !0;
        }
    }

    pub fn CPU_SET(cpu: usize, cpuset: &mut cpuset_t) -> () {
        let bitset_bits = ::mem::size_of::<::c_long>();
        let (idx, offset) = (cpu / bitset_bits, cpu % bitset_bits);
        cpuset.__bits[idx] |= 1 << offset;
        ()
    }

    pub fn CPU_CLR(cpu: usize, cpuset: &mut cpuset_t) -> () {
        let bitset_bits = ::mem::size_of::<::c_long>();
        let (idx, offset) = (cpu / bitset_bits, cpu % bitset_bits);
        cpuset.__bits[idx] &= !(1 << offset);
        ()
    }

    pub fn CPU_ISSET(cpu: usize, cpuset: &cpuset_t) -> bool {
        let bitset_bits = ::mem::size_of::<::c_long>();
        let (idx, offset) = (cpu / bitset_bits, cpu % bitset_bits);
        0 != cpuset.__bits[idx] & (1 << offset)
    }

    pub fn SOCKCRED2SIZE(ngrps: usize) -> usize {
        let ngrps = if ngrps > 0 {
            ngrps - 1
        } else {
            0
        };
        ::mem::size_of::<sockcred2>() + ::mem::size_of::<::gid_t>() * ngrps
    }
}

safe_f! {
    pub {const} fn WIFSIGNALED(status: ::c_int) -> bool {
        (status & 0o177) != 0o177 && (status & 0o177) != 0 && status != 0x13
    }
}

cfg_if! {
    if #[cfg(not(any(freebsd10, freebsd11)))] {
        extern "C" {
            pub fn fhlink(fhp: *mut fhandle_t, to: *const ::c_char) -> ::c_int;
            pub fn fhlinkat(fhp: *mut fhandle_t, tofd: ::c_int, to: *const ::c_char) -> ::c_int;
            pub fn fhreadlink(
                fhp: *mut fhandle_t,
                buf: *mut ::c_char,
                bufsize: ::size_t,
            ) -> ::c_int;
            pub fn getfhat(
                fd: ::c_int,
                path: *mut ::c_char,
                fhp: *mut fhandle,
                flag: ::c_int,
            ) -> ::c_int;
        }
    }
}

extern "C" {
    pub fn __error() -> *mut ::c_int;

    pub fn aio_cancel(fd: ::c_int, aiocbp: *mut aiocb) -> ::c_int;
    pub fn aio_error(aiocbp: *const aiocb) -> ::c_int;
    pub fn aio_fsync(op: ::c_int, aiocbp: *mut aiocb) -> ::c_int;
    pub fn aio_read(aiocbp: *mut aiocb) -> ::c_int;
    pub fn aio_return(aiocbp: *mut aiocb) -> ::ssize_t;
    pub fn aio_suspend(
        aiocb_list: *const *const aiocb,
        nitems: ::c_int,
        timeout: *const ::timespec,
    ) -> ::c_int;
    pub fn aio_write(aiocbp: *mut aiocb) -> ::c_int;

    pub fn devname_r(
        dev: ::dev_t,
        mode: ::mode_t,
        buf: *mut ::c_char,
        len: ::c_int,
    ) -> *mut ::c_char;

    pub fn extattr_delete_fd(
        fd: ::c_int,
        attrnamespace: ::c_int,
        attrname: *const ::c_char,
    ) -> ::c_int;
    pub fn extattr_delete_file(
        path: *const ::c_char,
        attrnamespace: ::c_int,
        attrname: *const ::c_char,
    ) -> ::c_int;
    pub fn extattr_delete_link(
        path: *const ::c_char,
        attrnamespace: ::c_int,
        attrname: *const ::c_char,
    ) -> ::c_int;
    pub fn extattr_get_fd(
        fd: ::c_int,
        attrnamespace: ::c_int,
        attrname: *const ::c_char,
        data: *mut ::c_void,
        nbytes: ::size_t,
    ) -> ::ssize_t;
    pub fn extattr_get_file(
        path: *const ::c_char,
        attrnamespace: ::c_int,
        attrname: *const ::c_char,
        data: *mut ::c_void,
        nbytes: ::size_t,
    ) -> ::ssize_t;
    pub fn extattr_get_link(
        path: *const ::c_char,
        attrnamespace: ::c_int,
        attrname: *const ::c_char,
        data: *mut ::c_void,
        nbytes: ::size_t,
    ) -> ::ssize_t;
    pub fn extattr_list_fd(
        fd: ::c_int,
        attrnamespace: ::c_int,
        data: *mut ::c_void,
        nbytes: ::size_t,
    ) -> ::ssize_t;
    pub fn extattr_list_file(
        path: *const ::c_char,
        attrnamespace: ::c_int,
        data: *mut ::c_void,
        nbytes: ::size_t,
    ) -> ::ssize_t;
    pub fn extattr_list_link(
        path: *const ::c_char,
        attrnamespace: ::c_int,
        data: *mut ::c_void,
        nbytes: ::size_t,
    ) -> ::ssize_t;
    pub fn extattr_set_fd(
        fd: ::c_int,
        attrnamespace: ::c_int,
        attrname: *const ::c_char,
        data: *const ::c_void,
        nbytes: ::size_t,
    ) -> ::ssize_t;
    pub fn extattr_set_file(
        path: *const ::c_char,
        attrnamespace: ::c_int,
        attrname: *const ::c_char,
        data: *const ::c_void,
        nbytes: ::size_t,
    ) -> ::ssize_t;
    pub fn extattr_set_link(
        path: *const ::c_char,
        attrnamespace: ::c_int,
        attrname: *const ::c_char,
        data: *const ::c_void,
        nbytes: ::size_t,
    ) -> ::ssize_t;

    pub fn jail(jail: *mut ::jail) -> ::c_int;
    pub fn jail_attach(jid: ::c_int) -> ::c_int;
    pub fn jail_remove(jid: ::c_int) -> ::c_int;
    pub fn jail_get(iov: *mut ::iovec, niov: ::c_uint, flags: ::c_int) -> ::c_int;
    pub fn jail_set(iov: *mut ::iovec, niov: ::c_uint, flags: ::c_int) -> ::c_int;

    pub fn lio_listio(
        mode: ::c_int,
        aiocb_list: *const *mut aiocb,
        nitems: ::c_int,
        sevp: *mut sigevent,
    ) -> ::c_int;

    pub fn mkostemp(template: *mut ::c_char, flags: ::c_int) -> ::c_int;
    pub fn mkostemps(template: *mut ::c_char, suffixlen: ::c_int, flags: ::c_int) -> ::c_int;

    pub fn getutxuser(user: *const ::c_char) -> *mut utmpx;
    pub fn setutxdb(_type: ::c_int, file: *const ::c_char) -> ::c_int;

    pub fn aio_waitcomplete(iocbp: *mut *mut aiocb, timeout: *mut ::timespec) -> ::ssize_t;
    pub fn mq_getfd_np(mqd: ::mqd_t) -> ::c_int;

    pub fn waitid(
        idtype: idtype_t,
        id: ::id_t,
        infop: *mut ::siginfo_t,
        options: ::c_int,
    ) -> ::c_int;

    pub fn ftok(pathname: *const ::c_char, proj_id: ::c_int) -> ::key_t;
    pub fn shmget(key: ::key_t, size: ::size_t, shmflg: ::c_int) -> ::c_int;
    pub fn shmat(shmid: ::c_int, shmaddr: *const ::c_void, shmflg: ::c_int) -> *mut ::c_void;
    pub fn shmdt(shmaddr: *const ::c_void) -> ::c_int;
    pub fn shmctl(shmid: ::c_int, cmd: ::c_int, buf: *mut ::shmid_ds) -> ::c_int;
    pub fn semget(key: ::key_t, nsems: ::c_int, semflg: ::c_int) -> ::c_int;
    pub fn semctl(semid: ::c_int, semnum: ::c_int, cmd: ::c_int, ...) -> ::c_int;
    pub fn semop(semid: ::c_int, sops: *mut sembuf, nsops: ::size_t) -> ::c_int;
    pub fn msgctl(msqid: ::c_int, cmd: ::c_int, buf: *mut ::msqid_ds) -> ::c_int;
    pub fn msgget(key: ::key_t, msgflg: ::c_int) -> ::c_int;
    pub fn msgsnd(
        msqid: ::c_int,
        msgp: *const ::c_void,
        msgsz: ::size_t,
        msgflg: ::c_int,
    ) -> ::c_int;
    pub fn cfmakesane(termios: *mut ::termios);

    pub fn pdfork(fdp: *mut ::c_int, flags: ::c_int) -> ::pid_t;
    pub fn pdgetpid(fd: ::c_int, pidp: *mut ::pid_t) -> ::c_int;
    pub fn pdkill(fd: ::c_int, signum: ::c_int) -> ::c_int;

    pub fn rtprio_thread(function: ::c_int, lwpid: ::lwpid_t, rtp: *mut super::rtprio) -> ::c_int;

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
    pub fn posix_spawnattr_getschedpolicy(
        attr: *const posix_spawnattr_t,
        flags: *mut ::c_int,
    ) -> ::c_int;
    pub fn posix_spawnattr_setschedpolicy(attr: *mut posix_spawnattr_t, flags: ::c_int) -> ::c_int;
    pub fn posix_spawnattr_getschedparam(
        attr: *const posix_spawnattr_t,
        param: *mut ::sched_param,
    ) -> ::c_int;
    pub fn posix_spawnattr_setschedparam(
        attr: *mut posix_spawnattr_t,
        param: *const ::sched_param,
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

    pub fn uuidgen(store: *mut uuid, count: ::c_int) -> ::c_int;

    pub fn thr_kill(id: ::c_long, sig: ::c_int) -> ::c_int;
    pub fn thr_kill2(pid: ::pid_t, id: ::c_long, sig: ::c_int) -> ::c_int;
    pub fn thr_self(tid: *mut ::c_long) -> ::c_int;
    pub fn pthread_getthreadid_np() -> ::c_int;
    pub fn pthread_getaffinity_np(
        td: ::pthread_t,
        cpusetsize: ::size_t,
        cpusetp: *mut cpuset_t,
    ) -> ::c_int;
    pub fn pthread_setaffinity_np(
        td: ::pthread_t,
        cpusetsize: ::size_t,
        cpusetp: *const cpuset_t,
    ) -> ::c_int;

    // sched.h linux compatibility api
    pub fn sched_getaffinity(pid: ::pid_t, cpusetsz: ::size_t, cpuset: *mut ::cpuset_t) -> ::c_int;
    // FIXME: the first argument's type might not be correct, fix later if that changes.
    pub fn sched_setaffinity(
        pid: ::c_int,
        cpusetsz: ::size_t,
        cpuset: *const ::cpuset_t,
    ) -> ::c_int;
    pub fn sched_getcpu() -> ::c_int;

    pub fn pthread_mutex_consistent(mutex: *mut ::pthread_mutex_t) -> ::c_int;

    pub fn pthread_mutexattr_getrobust(
        attr: *mut ::pthread_mutexattr_t,
        robust: *mut ::c_int,
    ) -> ::c_int;
    pub fn pthread_mutexattr_setrobust(
        attr: *mut ::pthread_mutexattr_t,
        robust: ::c_int,
    ) -> ::c_int;

    pub fn pthread_spin_init(lock: *mut pthread_spinlock_t, pshared: ::c_int) -> ::c_int;
    pub fn pthread_spin_destroy(lock: *mut pthread_spinlock_t) -> ::c_int;
    pub fn pthread_spin_lock(lock: *mut pthread_spinlock_t) -> ::c_int;
    pub fn pthread_spin_trylock(lock: *mut pthread_spinlock_t) -> ::c_int;
    pub fn pthread_spin_unlock(lock: *mut pthread_spinlock_t) -> ::c_int;

    #[cfg_attr(all(target_os = "freebsd", freebsd11), link_name = "statfs@FBSD_1.0")]
    pub fn statfs(path: *const ::c_char, buf: *mut statfs) -> ::c_int;
    #[cfg_attr(all(target_os = "freebsd", freebsd11), link_name = "fstatfs@FBSD_1.0")]
    pub fn fstatfs(fd: ::c_int, buf: *mut statfs) -> ::c_int;

    pub fn dup3(src: ::c_int, dst: ::c_int, flags: ::c_int) -> ::c_int;
    pub fn __xuname(nmln: ::c_int, buf: *mut ::c_void) -> ::c_int;

    pub fn sendmmsg(
        sockfd: ::c_int,
        msgvec: *mut ::mmsghdr,
        vlen: ::size_t,
        flags: ::c_int,
    ) -> ::ssize_t;
    pub fn recvmmsg(
        sockfd: ::c_int,
        msgvec: *mut ::mmsghdr,
        vlen: ::size_t,
        flags: ::c_int,
        timeout: *const ::timespec,
    ) -> ::ssize_t;
    pub fn memmem(
        haystack: *const ::c_void,
        haystacklen: ::size_t,
        needle: *const ::c_void,
        needlelen: ::size_t,
    ) -> *mut ::c_void;

    pub fn fhopen(fhp: *const fhandle_t, flags: ::c_int) -> ::c_int;
    pub fn fhstat(fhp: *const fhandle, buf: *mut ::stat) -> ::c_int;
    pub fn fhstatfs(fhp: *const fhandle_t, buf: *mut ::statfs) -> ::c_int;
    pub fn getfh(path: *const ::c_char, fhp: *mut fhandle_t) -> ::c_int;
    pub fn lgetfh(path: *const ::c_char, fhp: *mut fhandle_t) -> ::c_int;
    pub fn getfsstat(buf: *mut ::statfs, bufsize: ::c_long, mode: ::c_int) -> ::c_int;
    #[cfg_attr(
        all(target_os = "freebsd", freebsd11),
        link_name = "getmntinfo@FBSD_1.0"
    )]
    pub fn getmntinfo(mntbufp: *mut *mut ::statfs, mode: ::c_int) -> ::c_int;
    pub fn mount(
        type_: *const ::c_char,
        dir: *const ::c_char,
        flags: ::c_int,
        data: *mut ::c_void,
    ) -> ::c_int;
    pub fn nmount(iov: *mut ::iovec, niov: ::c_uint, flags: ::c_int) -> ::c_int;

    pub fn setproctitle(fmt: *const ::c_char, ...);
    pub fn rfork(flags: ::c_int) -> ::c_int;
    pub fn cpuset_getaffinity(
        level: cpulevel_t,
        which: cpuwhich_t,
        id: ::id_t,
        setsize: ::size_t,
        mask: *mut cpuset_t,
    ) -> ::c_int;
    pub fn cpuset_setaffinity(
        level: cpulevel_t,
        which: cpuwhich_t,
        id: ::id_t,
        setsize: ::size_t,
        mask: *const cpuset_t,
    ) -> ::c_int;
    pub fn cap_enter() -> ::c_int;
    pub fn cap_getmode(modep: *mut ::c_uint) -> ::c_int;
    pub fn __cap_rights_init(version: ::c_int, rights: *mut cap_rights_t, ...)
        -> *mut cap_rights_t;
    pub fn __cap_rights_set(rights: *mut cap_rights_t, ...) -> *mut cap_rights_t;
    pub fn __cap_rights_clear(rights: *mut cap_rights_t, ...) -> *mut cap_rights_t;
    pub fn __cap_rights_is_set(rights: *const cap_rights_t, ...) -> bool;
    pub fn cap_rights_is_valid(rights: *const cap_rights_t) -> bool;
    pub fn cap_rights_limit(fd: ::c_int, rights: *const cap_rights_t) -> ::c_int;
    pub fn cap_rights_merge(dst: *mut cap_rights_t, src: *const cap_rights_t) -> *mut cap_rights_t;
    pub fn cap_rights_remove(dst: *mut cap_rights_t, src: *const cap_rights_t)
        -> *mut cap_rights_t;
    pub fn cap_rights_contains(big: *const cap_rights_t, little: *const cap_rights_t) -> bool;

    pub fn reallocarray(ptr: *mut ::c_void, nmemb: ::size_t, size: ::size_t) -> *mut ::c_void;

    pub fn ffs(value: ::c_int) -> ::c_int;
    pub fn ffsl(value: ::c_long) -> ::c_int;
    pub fn ffsll(value: ::c_longlong) -> ::c_int;
    pub fn fls(value: ::c_int) -> ::c_int;
    pub fn flsl(value: ::c_long) -> ::c_int;
    pub fn flsll(value: ::c_longlong) -> ::c_int;
    pub fn malloc_usable_size(ptr: *const ::c_void) -> ::size_t;
    pub fn malloc_stats_print(
        write_cb: unsafe extern "C" fn(*mut ::c_void, *const ::c_char),
        cbopaque: *mut ::c_void,
        opt: *const ::c_char,
    );
    pub fn mallctl(
        name: *const ::c_char,
        oldp: *mut ::c_void,
        oldlenp: *mut ::size_t,
        newp: *mut ::c_void,
        newlen: ::size_t,
    ) -> ::c_int;
    pub fn mallctlnametomib(
        name: *const ::c_char,
        mibp: *mut ::size_t,
        miplen: *mut ::size_t,
    ) -> ::c_int;
    pub fn mallctlbymib(
        mib: *const ::size_t,
        mible: ::size_t,
        oldp: *mut ::c_void,
        oldlenp: *mut ::size_t,
        newp: *mut ::c_void,
        newlen: ::size_t,
    ) -> ::c_int;
    pub fn mallocx(size: ::size_t, flags: ::c_int) -> *mut ::c_void;
    pub fn rallocx(ptr: *mut ::c_void, size: ::size_t, flags: ::c_int) -> *mut ::c_void;
    pub fn xallocx(ptr: *mut ::c_void, size: ::size_t, extra: ::size_t, flags: ::c_int)
        -> ::size_t;
    pub fn sallocx(ptr: *const ::c_void, flags: ::c_int) -> ::size_t;
    pub fn dallocx(ptr: *mut ::c_void, flags: ::c_int);
    pub fn sdallocx(ptr: *mut ::c_void, size: ::size_t, flags: ::c_int);
    pub fn nallocx(size: ::size_t, flags: ::c_int) -> ::size_t;

    pub fn procctl(idtype: ::idtype_t, id: ::id_t, cmd: ::c_int, data: *mut ::c_void) -> ::c_int;

    pub fn getpagesize() -> ::c_int;

    pub fn adjtime(arg1: *const ::timeval, arg2: *mut ::timeval) -> ::c_int;
    pub fn clock_getcpuclockid2(arg1: ::id_t, arg2: ::c_int, arg3: *mut clockid_t) -> ::c_int;
}

#[link(name = "kvm")]
extern "C" {
    pub fn kvm_open(
        execfile: *const ::c_char,
        corefile: *const ::c_char,
        swapfile: *const ::c_char,
        flags: ::c_int,
        errstr: *const ::c_char,
    ) -> *mut kvm_t;
    pub fn kvm_close(kd: *mut kvm_t) -> ::c_int;
    pub fn kvm_dpcpu_setcpu(kd: *mut kvm_t, cpu: ::c_uint) -> ::c_int;
    pub fn kvm_getargv(kd: *mut kvm_t, p: *const kinfo_proc, nchr: ::c_int) -> *mut *mut ::c_char;
    pub fn kvm_getcptime(kd: *mut kvm_t, cp_time: *mut ::c_long) -> ::c_int;
    pub fn kvm_getenvv(kd: *mut kvm_t, p: *const kinfo_proc, nchr: ::c_int) -> *mut *mut ::c_char;
    pub fn kvm_geterr(kd: *mut kvm_t) -> *mut ::c_char;
    pub fn kvm_getloadavg(kd: *mut kvm_t, loadavg: *mut ::c_double, nelem: ::c_int) -> ::c_int;
    pub fn kvm_getmaxcpu(kd: *mut kvm_t) -> ::c_int;
    pub fn kvm_getncpus(kd: *mut kvm_t) -> ::c_int;
    pub fn kvm_getpcpu(kd: *mut kvm_t, cpu: ::c_int) -> *mut ::c_void;
    pub fn kvm_counter_u64_fetch(kd: *mut kvm_t, base: ::c_ulong) -> u64;
    pub fn kvm_getprocs(
        kd: *mut kvm_t,
        op: ::c_int,
        arg: ::c_int,
        cnt: *mut ::c_int,
    ) -> *mut kinfo_proc;
    pub fn kvm_getswapinfo(
        kd: *mut kvm_t,
        info: *mut kvm_swap,
        maxswap: ::c_int,
        flags: ::c_int,
    ) -> ::c_int;
    pub fn kvm_native(kd: *mut kvm_t) -> ::c_int;
    pub fn kvm_nlist(kd: *mut kvm_t, nl: *mut nlist) -> ::c_int;
    pub fn kvm_nlist2(kd: *mut kvm_t, nl: *mut kvm_nlist) -> ::c_int;
    pub fn kvm_openfiles(
        execfile: *const ::c_char,
        corefile: *const ::c_char,
        swapfile: *const ::c_char,
        flags: ::c_int,
        errbuf: *mut ::c_char,
    ) -> *mut kvm_t;
    pub fn kvm_read(
        kd: *mut kvm_t,
        addr: ::c_ulong,
        buf: *mut ::c_void,
        nbytes: ::size_t,
    ) -> ::ssize_t;
    pub fn kvm_read_zpcpu(
        kd: *mut kvm_t,
        base: ::c_ulong,
        buf: *mut ::c_void,
        size: ::size_t,
        cpu: ::c_int,
    ) -> ::ssize_t;
    pub fn kvm_read2(
        kd: *mut kvm_t,
        addr: kvaddr_t,
        buf: *mut ::c_void,
        nbytes: ::size_t,
    ) -> ::ssize_t;
    pub fn kvm_write(
        kd: *mut kvm_t,
        addr: ::c_ulong,
        buf: *const ::c_void,
        nbytes: ::size_t,
    ) -> ::ssize_t;
}

#[link(name = "util")]
extern "C" {
    pub fn extattr_namespace_to_string(
        attrnamespace: ::c_int,
        string: *mut *mut ::c_char,
    ) -> ::c_int;
    pub fn extattr_string_to_namespace(
        string: *const ::c_char,
        attrnamespace: *mut ::c_int,
    ) -> ::c_int;
    pub fn realhostname(host: *mut ::c_char, hsize: ::size_t, ip: *const ::in_addr) -> ::c_int;
    pub fn realhostname_sa(
        host: *mut ::c_char,
        hsize: ::size_t,
        addr: *mut ::sockaddr,
        addrlen: ::c_int,
    ) -> ::c_int;

    pub fn kld_isloaded(name: *const ::c_char) -> ::c_int;
    pub fn kld_load(name: *const ::c_char) -> ::c_int;

    pub fn kinfo_getvmmap(pid: ::pid_t, cntp: *mut ::c_int) -> *mut kinfo_vmentry;

    pub fn hexdump(ptr: *const ::c_void, length: ::c_int, hdr: *const ::c_char, flags: ::c_int);
    pub fn humanize_number(
        buf: *mut ::c_char,
        len: ::size_t,
        number: i64,
        suffix: *const ::c_char,
        scale: ::c_int,
        flags: ::c_int,
    ) -> ::c_int;
}

#[link(name = "procstat")]
extern "C" {
    pub fn procstat_open_sysctl() -> *mut procstat;
    pub fn procstat_getfiles(
        procstat: *mut procstat,
        kp: *mut kinfo_proc,
        mmapped: ::c_int,
    ) -> *mut filestat_list;
    pub fn procstat_freefiles(procstat: *mut procstat, head: *mut filestat_list);
    pub fn procstat_getprocs(
        procstat: *mut procstat,
        what: ::c_int,
        arg: ::c_int,
        count: *mut ::c_uint,
    ) -> *mut kinfo_proc;
    pub fn procstat_freeprocs(procstat: *mut procstat, p: *mut kinfo_proc);
    pub fn procstat_getvmmap(
        procstat: *mut procstat,
        kp: *mut kinfo_proc,
        count: *mut ::c_uint,
    ) -> *mut kinfo_vmentry;
    pub fn procstat_freevmmap(procstat: *mut procstat, vmmap: *mut kinfo_vmentry);
    pub fn procstat_close(procstat: *mut procstat);
    pub fn procstat_freeargv(procstat: *mut procstat);
    pub fn procstat_freeenvv(procstat: *mut procstat);
    pub fn procstat_freegroups(procstat: *mut procstat, groups: *mut ::gid_t);
    pub fn procstat_freeptlwpinfo(procstat: *mut procstat, pl: *mut ptrace_lwpinfo);
    pub fn procstat_getargv(
        procstat: *mut procstat,
        kp: *mut kinfo_proc,
        nchr: ::size_t,
    ) -> *mut *mut ::c_char;
    pub fn procstat_getenvv(
        procstat: *mut procstat,
        kp: *mut kinfo_proc,
        nchr: ::size_t,
    ) -> *mut *mut ::c_char;
    pub fn procstat_getgroups(
        procstat: *mut procstat,
        kp: *mut kinfo_proc,
        count: *mut ::c_uint,
    ) -> *mut ::gid_t;
    pub fn procstat_getosrel(
        procstat: *mut procstat,
        kp: *mut kinfo_proc,
        osrelp: *mut ::c_int,
    ) -> ::c_int;
    pub fn procstat_getpathname(
        procstat: *mut procstat,
        kp: *mut kinfo_proc,
        pathname: *mut ::c_char,
        maxlen: ::size_t,
    ) -> ::c_int;
    pub fn procstat_getrlimit(
        procstat: *mut procstat,
        kp: *mut kinfo_proc,
        which: ::c_int,
        rlimit: *mut ::rlimit,
    ) -> ::c_int;
    pub fn procstat_getumask(
        procstat: *mut procstat,
        kp: *mut kinfo_proc,
        maskp: *mut ::c_ushort,
    ) -> ::c_int;
    pub fn procstat_open_core(filename: *const ::c_char) -> *mut procstat;
    pub fn procstat_open_kvm(nlistf: *const ::c_char, memf: *const ::c_char) -> *mut procstat;
    pub fn procstat_get_socket_info(
        proc_: *mut procstat,
        fst: *mut filestat,
        sock: *mut sockstat,
        errbuf: *mut ::c_char,
    ) -> ::c_int;
    pub fn procstat_get_vnode_info(
        proc_: *mut procstat,
        fst: *mut filestat,
        vn: *mut vnstat,
        errbuf: *mut ::c_char,
    ) -> ::c_int;
    pub fn procstat_get_pts_info(
        proc_: *mut procstat,
        fst: *mut filestat,
        pts: *mut ptsstat,
        errbuf: *mut ::c_char,
    ) -> ::c_int;
    pub fn procstat_get_shm_info(
        proc_: *mut procstat,
        fst: *mut filestat,
        shm: *mut shmstat,
        errbuf: *mut ::c_char,
    ) -> ::c_int;
}

#[link(name = "rt")]
extern "C" {
    pub fn timer_create(clock_id: clockid_t, evp: *mut sigevent, timerid: *mut timer_t) -> ::c_int;
    pub fn timer_delete(timerid: timer_t) -> ::c_int;
    pub fn timer_getoverrun(timerid: timer_t) -> ::c_int;
    pub fn timer_gettime(timerid: timer_t, value: *mut itimerspec) -> ::c_int;
    pub fn timer_settime(
        timerid: timer_t,
        flags: ::c_int,
        value: *const itimerspec,
        ovalue: *mut itimerspec,
    ) -> ::c_int;
}

#[link(name = "devstat")]
extern "C" {
    pub fn devstat_getnumdevs(kd: *mut kvm_t) -> ::c_int;
    pub fn devstat_getgeneration(kd: *mut kvm_t) -> ::c_long;
    pub fn devstat_getversion(kd: *mut kvm_t) -> ::c_int;
    pub fn devstat_checkversion(kd: *mut kvm_t) -> ::c_int;
    pub fn devstat_selectdevs(
        dev_select: *mut *mut device_selection,
        num_selected: *mut ::c_int,
        num_selections: *mut ::c_int,
        select_generation: *mut ::c_long,
        current_generation: ::c_long,
        devices: *mut devstat,
        numdevs: ::c_int,
        matches: *mut devstat_match,
        num_matches: ::c_int,
        dev_selections: *mut *mut ::c_char,
        num_dev_selections: ::c_int,
        select_mode: devstat_select_mode,
        maxshowdevs: ::c_int,
        perf_select: ::c_int,
    ) -> ::c_int;
    pub fn devstat_buildmatch(
        match_str: *mut ::c_char,
        matches: *mut *mut devstat_match,
        num_matches: *mut ::c_int,
    ) -> ::c_int;
}

cfg_if! {
    if #[cfg(freebsd14)] {
        mod freebsd14;
        pub use self::freebsd14::*;
    } else if #[cfg(freebsd13)] {
        mod freebsd13;
        pub use self::freebsd13::*;
    } else if #[cfg(freebsd12)] {
        mod freebsd12;
        pub use self::freebsd12::*;
    } else if #[cfg(any(freebsd10, freebsd11))] {
        mod freebsd11;
        pub use self::freebsd11::*;
    } else {
        // Unknown freebsd version
    }
}

cfg_if! {
    if #[cfg(target_arch = "x86")] {
        mod x86;
        pub use self::x86::*;
    } else if #[cfg(target_arch = "x86_64")] {
        mod x86_64;
        pub use self::x86_64::*;
    } else if #[cfg(target_arch = "aarch64")] {
        mod aarch64;
        pub use self::aarch64::*;
    } else if #[cfg(target_arch = "arm")] {
        mod arm;
        pub use self::arm::*;
    } else if #[cfg(target_arch = "powerpc64")] {
        mod powerpc64;
        pub use self::powerpc64::*;
    } else if #[cfg(target_arch = "powerpc")] {
        mod powerpc;
        pub use self::powerpc::*;
    } else if #[cfg(target_arch = "riscv64")] {
        mod riscv64;
        pub use self::riscv64::*;
    } else {
        // Unknown target_arch
    }
}
