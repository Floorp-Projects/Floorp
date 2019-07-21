use unix::bsd::O_SYNC;

pub type clock_t = i64;
pub type suseconds_t = ::c_long;
pub type dev_t = i32;
pub type sigset_t = ::c_uint;
pub type blksize_t = ::int32_t;
pub type fsblkcnt_t = ::uint64_t;
pub type fsfilcnt_t = ::uint64_t;
pub type pthread_attr_t = *mut ::c_void;
pub type pthread_mutex_t = *mut ::c_void;
pub type pthread_mutexattr_t = *mut ::c_void;
pub type pthread_cond_t = *mut ::c_void;
pub type pthread_condattr_t = *mut ::c_void;
pub type pthread_rwlock_t = *mut ::c_void;
pub type pthread_rwlockattr_t = *mut ::c_void;
pub type caddr_t = *mut ::c_char;

s! {
    pub struct ip_mreq {
        pub imr_multiaddr: in_addr,
        pub imr_interface: in_addr,
    }

    pub struct in_addr {
        pub s_addr: ::in_addr_t,
    }

    pub struct sockaddr_in {
        pub sin_len: u8,
        pub sin_family: ::sa_family_t,
        pub sin_port: ::in_port_t,
        pub sin_addr: ::in_addr,
        pub sin_zero: [::int8_t; 8],
    }

    pub struct kevent {
        pub ident: ::uintptr_t,
        pub filter: ::c_short,
        pub flags: ::c_ushort,
        pub fflags: ::c_uint,
        pub data: ::int64_t,
        pub udata: *mut ::c_void,
    }

    pub struct stat {
        pub st_mode: ::mode_t,
        pub st_dev: ::dev_t,
        pub st_ino: ::ino_t,
        pub st_nlink: ::nlink_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        pub st_rdev: ::dev_t,
        pub st_atime: ::time_t,
        pub st_atime_nsec: ::c_long,
        pub st_mtime: ::time_t,
        pub st_mtime_nsec: ::c_long,
        pub st_ctime: ::time_t,
        pub st_ctime_nsec: ::c_long,
        pub st_size: ::off_t,
        pub st_blocks: ::blkcnt_t,
        pub st_blksize: ::blksize_t,
        pub st_flags: ::uint32_t,
        pub st_gen: ::uint32_t,
        pub st_birthtime: ::time_t,
        pub st_birthtime_nsec: ::c_long,
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

    pub struct addrinfo {
        pub ai_flags: ::c_int,
        pub ai_family: ::c_int,
        pub ai_socktype: ::c_int,
        pub ai_protocol: ::c_int,
        pub ai_addrlen: ::socklen_t,
        pub ai_addr: *mut ::sockaddr,
        pub ai_canonname: *mut ::c_char,
        pub ai_next: *mut ::addrinfo,
    }

    pub struct Dl_info {
        pub dli_fname: *const ::c_char,
        pub dli_fbase: *mut ::c_void,
        pub dli_sname: *const ::c_char,
        pub dli_saddr: *mut ::c_void,
    }

    pub struct if_data {
        pub ifi_type: ::c_uchar,
        pub ifi_addrlen: ::c_uchar,
        pub ifi_hdrlen: ::c_uchar,
        pub ifi_link_state: ::c_uchar,
        pub ifi_mtu: u32,
        pub ifi_metric: u32,
        pub ifi_rdomain: u32,
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
        pub ifi_oqdrops: u64,
        pub ifi_noproto: u64,
        pub ifi_capabilities: u32,
        pub ifi_lastchange: ::timeval,
    }

    pub struct if_msghdr {
        pub ifm_msglen: ::c_ushort,
        pub ifm_version: ::c_uchar,
        pub ifm_type: ::c_uchar,
        pub ifm_hdrlen: ::c_ushort,
        pub ifm_index: ::c_ushort,
        pub ifm_tableid: ::c_ushort,
        pub ifm_pad1: ::c_uchar,
        pub ifm_pad2: ::c_uchar,
        pub ifm_addrs: ::c_int,
        pub ifm_flags: ::c_int,
        pub ifm_xflags: ::c_int,
        pub ifm_data: if_data,
    }

    pub struct sockaddr_dl {
        pub sdl_len: ::c_uchar,
        pub sdl_family: ::c_uchar,
        pub sdl_index: ::c_ushort,
        pub sdl_type: ::c_uchar,
        pub sdl_nlen: ::c_uchar,
        pub sdl_alen: ::c_uchar,
        pub sdl_slen: ::c_uchar,
        pub sdl_data: [::c_char; 24],
    }

    pub struct sockpeercred {
        pub uid: ::uid_t,
        pub gid: ::gid_t,
        pub pid: ::pid_t,
    }

    pub struct arphdr {
        pub ar_hrd: u16,
        pub ar_pro: u16,
        pub ar_hln: u8,
        pub ar_pln: u8,
        pub ar_op: u16,
    }
}

s_no_extra_traits! {
    pub struct dirent {
        pub d_fileno: ::ino_t,
        pub d_off: ::off_t,
        pub d_reclen: u16,
        pub d_type: u8,
        pub d_namlen: u8,
        __d_padding: [u8; 4],
        pub d_name: [::c_char; 256],
    }

    pub struct sockaddr_storage {
        pub ss_len: u8,
        pub ss_family: ::sa_family_t,
        __ss_pad1: [u8; 6],
        __ss_pad2: i64,
        __ss_pad3: [u8; 240],
    }

    pub struct siginfo_t {
        pub si_signo: ::c_int,
        pub si_code: ::c_int,
        pub si_errno: ::c_int,
        pub si_addr: *mut ::c_char,
        #[cfg(target_pointer_width = "32")]
        __pad: [u8; 112],
        #[cfg(target_pointer_width = "64")]
        __pad: [u8; 108],
    }

    pub struct lastlog {
        ll_time: ::time_t,
        ll_line: [::c_char; UT_LINESIZE],
        ll_host: [::c_char; UT_HOSTSIZE],
    }

    pub struct utmp {
        pub ut_line: [::c_char; UT_LINESIZE],
        pub ut_name: [::c_char; UT_NAMESIZE],
        pub ut_host: [::c_char; UT_HOSTSIZE],
        pub ut_time: ::time_t,
    }
}

cfg_if! {
    if #[cfg(feature = "extra_traits")] {
        impl PartialEq for dirent {
            fn eq(&self, other: &dirent) -> bool {
                self.d_fileno == other.d_fileno
                    && self.d_off == other.d_off
                    && self.d_reclen == other.d_reclen
                    && self.d_type == other.d_type
                    && self.d_namlen == other.d_namlen
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
                    .field("d_fileno", &self.d_fileno)
                    .field("d_off", &self.d_off)
                    .field("d_reclen", &self.d_reclen)
                    .field("d_type", &self.d_type)
                    .field("d_namlen", &self.d_namlen)
                // FIXME: .field("d_name", &self.d_name)
                    .finish()
            }
        }

        impl ::hash::Hash for dirent {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.d_fileno.hash(state);
                self.d_off.hash(state);
                self.d_reclen.hash(state);
                self.d_type.hash(state);
                self.d_namlen.hash(state);
                self.d_name.hash(state);
            }
        }

        impl PartialEq for sockaddr_storage {
            fn eq(&self, other: &sockaddr_storage) -> bool {
                self.ss_len == other.ss_len
                    && self.ss_family == other.ss_family
            }
        }

        impl Eq for sockaddr_storage {}

        impl ::fmt::Debug for sockaddr_storage {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("sockaddr_storage")
                    .field("ss_len", &self.ss_len)
                    .field("ss_family", &self.ss_family)
                    .finish()
            }
        }

        impl ::hash::Hash for sockaddr_storage {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.ss_len.hash(state);
                self.ss_family.hash(state);
            }
        }

        impl PartialEq for siginfo_t {
            fn eq(&self, other: &siginfo_t) -> bool {
                self.si_signo == other.si_signo
                    && self.si_code == other.si_code
                    && self.si_errno == other.si_errno
                    && self.si_addr == other.si_addr
            }
        }

        impl Eq for siginfo_t {}

        impl ::fmt::Debug for siginfo_t {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("siginfo_t")
                    .field("si_signo", &self.si_signo)
                    .field("si_code", &self.si_code)
                    .field("si_errno", &self.si_errno)
                    .field("si_addr", &self.si_addr)
                    .finish()
            }
        }

        impl ::hash::Hash for siginfo_t {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.si_signo.hash(state);
                self.si_code.hash(state);
                self.si_errno.hash(state);
                self.si_addr.hash(state);
            }
        }

        impl PartialEq for lastlog {
            fn eq(&self, other: &lastlog) -> bool {
                self.ll_time == other.ll_time
                    && self
                    .ll_line
                    .iter()
                    .zip(other.ll_line.iter())
                    .all(|(a,b)| a == b)
                    && self
                    .ll_host
                    .iter()
                    .zip(other.ll_host.iter())
                    .all(|(a,b)| a == b)
            }
        }

        impl Eq for lastlog {}

        impl ::fmt::Debug for lastlog {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("lastlog")
                    .field("ll_time", &self.ll_time)
                // FIXME: .field("ll_line", &self.ll_line)
                // FIXME: .field("ll_host", &self.ll_host)
                    .finish()
            }
        }

        impl ::hash::Hash for lastlog {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.ll_time.hash(state);
                self.ll_line.hash(state);
                self.ll_host.hash(state);
            }
        }

        impl PartialEq for utmp {
            fn eq(&self, other: &utmp) -> bool {
                self.ut_time == other.ut_time
                    && self
                    .ut_line
                    .iter()
                    .zip(other.ut_line.iter())
                    .all(|(a,b)| a == b)
                    && self
                    .ut_name
                    .iter()
                    .zip(other.ut_name.iter())
                    .all(|(a,b)| a == b)
                    && self
                    .ut_host
                    .iter()
                    .zip(other.ut_host.iter())
                    .all(|(a,b)| a == b)
            }
        }

        impl Eq for utmp {}

        impl ::fmt::Debug for utmp {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("utmp")
                // FIXME: .field("ut_line", &self.ut_line)
                // FIXME: .field("ut_name", &self.ut_name)
                // FIXME: .field("ut_host", &self.ut_host)
                    .field("ut_time", &self.ut_time)
                    .finish()
            }
        }

        impl ::hash::Hash for utmp {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                self.ut_line.hash(state);
                self.ut_name.hash(state);
                self.ut_host.hash(state);
                self.ut_time.hash(state);
            }
        }
    }
}

pub const UT_NAMESIZE: usize = 32;
pub const UT_LINESIZE: usize = 8;
pub const UT_HOSTSIZE: usize = 256;

pub const O_CLOEXEC: ::c_int = 0x10000;
pub const O_DIRECTORY: ::c_int = 0x20000;
pub const O_RSYNC: ::c_int = O_SYNC;

pub const MS_SYNC : ::c_int = 0x0002;
pub const MS_INVALIDATE : ::c_int = 0x0004;

pub const POLLNORM: ::c_short = ::POLLRDNORM;

pub const ENOATTR : ::c_int = 83;
pub const EILSEQ : ::c_int = 84;
pub const EOVERFLOW : ::c_int = 87;
pub const ECANCELED : ::c_int = 88;
pub const EIDRM : ::c_int = 89;
pub const ENOMSG : ::c_int = 90;
pub const ENOTSUP : ::c_int = 91;
pub const EBADMSG : ::c_int = 92;
pub const ENOTRECOVERABLE : ::c_int = 93;
pub const EOWNERDEAD : ::c_int = 94;
pub const EPROTO : ::c_int = 95;
pub const ELAST : ::c_int = 95;

pub const F_DUPFD_CLOEXEC : ::c_int = 10;

pub const AT_FDCWD: ::c_int = -100;
pub const AT_EACCESS: ::c_int = 0x01;
pub const AT_SYMLINK_NOFOLLOW: ::c_int = 0x02;
pub const AT_SYMLINK_FOLLOW: ::c_int = 0x04;
pub const AT_REMOVEDIR: ::c_int = 0x08;

pub const RLIM_NLIMITS: ::c_int = 9;

pub const SO_TIMESTAMP: ::c_int = 0x0800;
pub const SO_SNDTIMEO: ::c_int = 0x1005;
pub const SO_RCVTIMEO: ::c_int = 0x1006;
pub const SO_BINDANY: ::c_int = 0x1000;
pub const SO_NETPROC: ::c_int = 0x1020;
pub const SO_RTABLE: ::c_int = 0x1021;
pub const SO_PEERCRED: ::c_int = 0x1022;
pub const SO_SPLICE: ::c_int = 0x1023;

// sys/netinet/in.h
// Protocols (RFC 1700)
// NOTE: These are in addition to the constants defined in src/unix/mod.rs

// IPPROTO_IP defined in src/unix/mod.rs
/// Hop-by-hop option header
pub const IPPROTO_HOPOPTS: ::c_int = 0;
// IPPROTO_ICMP defined in src/unix/mod.rs
/// group mgmt protocol
pub const IPPROTO_IGMP: ::c_int = 2;
/// gateway^2 (deprecated)
pub const IPPROTO_GGP: ::c_int = 3;
/// for compatibility
pub const IPPROTO_IPIP: ::c_int = 4;
// IPPROTO_TCP defined in src/unix/mod.rs
/// exterior gateway protocol
pub const IPPROTO_EGP: ::c_int = 8;
/// pup
pub const IPPROTO_PUP: ::c_int = 12;
// IPPROTO_UDP defined in src/unix/mod.rs
/// xns idp
pub const IPPROTO_IDP: ::c_int = 22;
/// tp-4 w/ class negotiation
pub const IPPROTO_TP: ::c_int = 29;
// IPPROTO_IPV6 defined in src/unix/mod.rs
/// IP6 routing header
pub const IPPROTO_ROUTING: ::c_int = 43;
/// IP6 fragmentation header
pub const IPPROTO_FRAGMENT: ::c_int = 44;
/// resource reservation
pub const IPPROTO_RSVP: ::c_int = 46;
/// General Routing Encap.
pub const IPPROTO_GRE: ::c_int = 47;
/// IP6 Encap Sec. Payload
pub const IPPROTO_ESP: ::c_int = 50;
/// IP6 Auth Header
pub const IPPROTO_AH: ::c_int = 51;
/// IP Mobility RFC 2004
pub const IPPROTO_MOBILE: ::c_int = 55;
// IPPROTO_ICMPV6 defined in src/unix/mod.rs
/// IP6 no next header
pub const IPPROTO_NONE: ::c_int = 59;
/// IP6 destination option
pub const IPPROTO_DSTOPTS: ::c_int = 60;
/// ISO cnlp
pub const IPPROTO_EON: ::c_int = 80;
/// Ethernet-in-IP
pub const IPPROTO_ETHERIP: ::c_int = 97;
/// encapsulation header
pub const IPPROTO_ENCAP: ::c_int = 98;
/// Protocol indep. multicast
pub const IPPROTO_PIM: ::c_int = 103;
/// IP Payload Comp. Protocol
pub const IPPROTO_IPCOMP: ::c_int = 108;
/// CARP
pub const IPPROTO_CARP: ::c_int = 112;
/// unicast MPLS packet
pub const IPPROTO_MPLS: ::c_int = 137;
/// PFSYNC
pub const IPPROTO_PFSYNC: ::c_int = 240;
pub const IPPROTO_MAX: ::c_int = 256;

/* Only used internally, so it can be outside the range of valid IP protocols */
/// Divert sockets
pub const IPPROTO_DIVERT: ::c_int = 258;

pub const IP_RECVDSTADDR: ::c_int = 7;
pub const IP_SENDSRCADDR: ::c_int = IP_RECVDSTADDR;
pub const IP_RECVIF: ::c_int = 30;

// sys/netinet/in.h
pub const TCP_MD5SIG: ::c_int = 0x04;
pub const TCP_NOPUSH: ::c_int = 0x10;

pub const AF_ECMA: ::c_int = 8;
pub const AF_ROUTE: ::c_int = 17;
pub const AF_ENCAP: ::c_int = 28;
pub const AF_SIP: ::c_int = 29;
pub const AF_KEY: ::c_int = 30;
pub const pseudo_AF_HDRCMPLT: ::c_int = 31;
pub const AF_BLUETOOTH: ::c_int = 32;
pub const AF_MPLS: ::c_int = 33;
pub const pseudo_AF_PFLOW: ::c_int = 34;
pub const pseudo_AF_PIPEX: ::c_int = 35;
#[doc(hidden)]
pub const AF_MAX: ::c_int = 36;

#[doc(hidden)]
pub const NET_MAXID: ::c_int = AF_MAX;
pub const NET_RT_DUMP: ::c_int = 1;
pub const NET_RT_FLAGS: ::c_int = 2;
pub const NET_RT_IFLIST: ::c_int = 3;
pub const NET_RT_STATS: ::c_int = 4;
pub const NET_RT_TABLE: ::c_int = 5;
pub const NET_RT_IFNAMES: ::c_int = 6;
#[doc(hidden)]
pub const NET_RT_MAXID: ::c_int = 7;

pub const IPV6_JOIN_GROUP: ::c_int = 12;
pub const IPV6_LEAVE_GROUP: ::c_int = 13;

pub const PF_ROUTE: ::c_int = AF_ROUTE;
pub const PF_ECMA: ::c_int = AF_ECMA;
pub const PF_ENCAP: ::c_int = AF_ENCAP;
pub const PF_SIP: ::c_int = AF_SIP;
pub const PF_KEY: ::c_int = AF_KEY;
pub const PF_BPF: ::c_int = pseudo_AF_HDRCMPLT;
pub const PF_BLUETOOTH: ::c_int = AF_BLUETOOTH;
pub const PF_MPLS: ::c_int = AF_MPLS;
pub const PF_PFLOW: ::c_int = pseudo_AF_PFLOW;
pub const PF_PIPEX: ::c_int = pseudo_AF_PIPEX;
#[doc(hidden)]
pub const PF_MAX: ::c_int = AF_MAX;

pub const SCM_TIMESTAMP: ::c_int = 0x04;

pub const O_DSYNC : ::c_int = 128;

pub const MAP_RENAME : ::c_int = 0x0000;
pub const MAP_NORESERVE : ::c_int = 0x0000;
pub const MAP_HASSEMAPHORE : ::c_int = 0x0000;

pub const EIPSEC : ::c_int = 82;
pub const ENOMEDIUM : ::c_int = 85;
pub const EMEDIUMTYPE : ::c_int = 86;

pub const EAI_BADFLAGS: ::c_int = -1;
pub const EAI_NONAME: ::c_int = -2;
pub const EAI_AGAIN: ::c_int = -3;
pub const EAI_FAIL: ::c_int = -4;
pub const EAI_NODATA: ::c_int = -5;
pub const EAI_FAMILY: ::c_int = -6;
pub const EAI_SOCKTYPE: ::c_int = -7;
pub const EAI_SERVICE: ::c_int = -8;
pub const EAI_MEMORY: ::c_int = -10;
pub const EAI_SYSTEM: ::c_int = -11;
pub const EAI_OVERFLOW: ::c_int = -14;

pub const RUSAGE_THREAD: ::c_int = 1;

pub const MAP_COPY : ::c_int = 0x0002;
pub const MAP_NOEXTEND : ::c_int = 0x0000;

pub const _PC_LINK_MAX : ::c_int = 1;
pub const _PC_MAX_CANON : ::c_int = 2;
pub const _PC_MAX_INPUT : ::c_int = 3;
pub const _PC_NAME_MAX : ::c_int = 4;
pub const _PC_PATH_MAX : ::c_int = 5;
pub const _PC_PIPE_BUF : ::c_int = 6;
pub const _PC_CHOWN_RESTRICTED : ::c_int = 7;
pub const _PC_NO_TRUNC : ::c_int = 8;
pub const _PC_VDISABLE : ::c_int = 9;
pub const _PC_2_SYMLINKS : ::c_int = 10;
pub const _PC_ALLOC_SIZE_MIN : ::c_int = 11;
pub const _PC_ASYNC_IO : ::c_int = 12;
pub const _PC_FILESIZEBITS : ::c_int = 13;
pub const _PC_PRIO_IO : ::c_int = 14;
pub const _PC_REC_INCR_XFER_SIZE : ::c_int = 15;
pub const _PC_REC_MAX_XFER_SIZE : ::c_int = 16;
pub const _PC_REC_MIN_XFER_SIZE : ::c_int = 17;
pub const _PC_REC_XFER_ALIGN : ::c_int = 18;
pub const _PC_SYMLINK_MAX : ::c_int = 19;
pub const _PC_SYNC_IO : ::c_int = 20;
pub const _PC_TIMESTAMP_RESOLUTION : ::c_int = 21;

pub const _SC_CLK_TCK : ::c_int = 3;
pub const _SC_SEM_NSEMS_MAX : ::c_int = 31;
pub const _SC_SEM_VALUE_MAX : ::c_int = 32;
pub const _SC_HOST_NAME_MAX : ::c_int = 33;
pub const _SC_MONOTONIC_CLOCK : ::c_int = 34;
pub const _SC_2_PBS : ::c_int = 35;
pub const _SC_2_PBS_ACCOUNTING : ::c_int = 36;
pub const _SC_2_PBS_CHECKPOINT : ::c_int = 37;
pub const _SC_2_PBS_LOCATE : ::c_int = 38;
pub const _SC_2_PBS_MESSAGE : ::c_int = 39;
pub const _SC_2_PBS_TRACK : ::c_int = 40;
pub const _SC_ADVISORY_INFO : ::c_int = 41;
pub const _SC_AIO_LISTIO_MAX : ::c_int = 42;
pub const _SC_AIO_MAX : ::c_int = 43;
pub const _SC_AIO_PRIO_DELTA_MAX : ::c_int = 44;
pub const _SC_ASYNCHRONOUS_IO : ::c_int = 45;
pub const _SC_ATEXIT_MAX : ::c_int = 46;
pub const _SC_BARRIERS : ::c_int = 47;
pub const _SC_CLOCK_SELECTION : ::c_int = 48;
pub const _SC_CPUTIME : ::c_int = 49;
pub const _SC_DELAYTIMER_MAX : ::c_int = 50;
pub const _SC_IOV_MAX : ::c_int = 51;
pub const _SC_IPV6 : ::c_int = 52;
pub const _SC_MAPPED_FILES : ::c_int = 53;
pub const _SC_MEMLOCK : ::c_int = 54;
pub const _SC_MEMLOCK_RANGE : ::c_int = 55;
pub const _SC_MEMORY_PROTECTION : ::c_int = 56;
pub const _SC_MESSAGE_PASSING : ::c_int = 57;
pub const _SC_MQ_OPEN_MAX : ::c_int = 58;
pub const _SC_MQ_PRIO_MAX : ::c_int = 59;
pub const _SC_PRIORITIZED_IO : ::c_int = 60;
pub const _SC_PRIORITY_SCHEDULING : ::c_int = 61;
pub const _SC_RAW_SOCKETS : ::c_int = 62;
pub const _SC_READER_WRITER_LOCKS : ::c_int = 63;
pub const _SC_REALTIME_SIGNALS : ::c_int = 64;
pub const _SC_REGEXP : ::c_int = 65;
pub const _SC_RTSIG_MAX : ::c_int = 66;
pub const _SC_SEMAPHORES : ::c_int = 67;
pub const _SC_SHARED_MEMORY_OBJECTS : ::c_int = 68;
pub const _SC_SHELL : ::c_int = 69;
pub const _SC_SIGQUEUE_MAX : ::c_int = 70;
pub const _SC_SPAWN : ::c_int = 71;
pub const _SC_SPIN_LOCKS : ::c_int = 72;
pub const _SC_SPORADIC_SERVER : ::c_int = 73;
pub const _SC_SS_REPL_MAX : ::c_int = 74;
pub const _SC_SYNCHRONIZED_IO : ::c_int = 75;
pub const _SC_SYMLOOP_MAX : ::c_int = 76;
pub const _SC_THREAD_ATTR_STACKADDR : ::c_int = 77;
pub const _SC_THREAD_ATTR_STACKSIZE : ::c_int = 78;
pub const _SC_THREAD_CPUTIME : ::c_int = 79;
pub const _SC_THREAD_DESTRUCTOR_ITERATIONS : ::c_int = 80;
pub const _SC_THREAD_KEYS_MAX : ::c_int = 81;
pub const _SC_THREAD_PRIO_INHERIT : ::c_int = 82;
pub const _SC_THREAD_PRIO_PROTECT : ::c_int = 83;
pub const _SC_THREAD_PRIORITY_SCHEDULING : ::c_int = 84;
pub const _SC_THREAD_PROCESS_SHARED : ::c_int = 85;
pub const _SC_THREAD_ROBUST_PRIO_INHERIT : ::c_int = 86;
pub const _SC_THREAD_ROBUST_PRIO_PROTECT : ::c_int = 87;
pub const _SC_THREAD_SPORADIC_SERVER : ::c_int = 88;
pub const _SC_THREAD_STACK_MIN : ::c_int = 89;
pub const _SC_THREAD_THREADS_MAX : ::c_int = 90;
pub const _SC_THREADS : ::c_int = 91;
pub const _SC_TIMEOUTS : ::c_int = 92;
pub const _SC_TIMER_MAX : ::c_int = 93;
pub const _SC_TIMERS : ::c_int = 94;
pub const _SC_TRACE : ::c_int = 95;
pub const _SC_TRACE_EVENT_FILTER : ::c_int = 96;
pub const _SC_TRACE_EVENT_NAME_MAX : ::c_int = 97;
pub const _SC_TRACE_INHERIT : ::c_int = 98;
pub const _SC_TRACE_LOG : ::c_int = 99;
pub const _SC_GETGR_R_SIZE_MAX : ::c_int = 100;
pub const _SC_GETPW_R_SIZE_MAX : ::c_int = 101;
pub const _SC_LOGIN_NAME_MAX : ::c_int = 102;
pub const _SC_THREAD_SAFE_FUNCTIONS : ::c_int = 103;
pub const _SC_TRACE_NAME_MAX : ::c_int = 104;
pub const _SC_TRACE_SYS_MAX : ::c_int = 105;
pub const _SC_TRACE_USER_EVENT_MAX : ::c_int = 106;
pub const _SC_TTY_NAME_MAX : ::c_int = 107;
pub const _SC_TYPED_MEMORY_OBJECTS : ::c_int = 108;
pub const _SC_V6_ILP32_OFF32 : ::c_int = 109;
pub const _SC_V6_ILP32_OFFBIG : ::c_int = 110;
pub const _SC_V6_LP64_OFF64 : ::c_int = 111;
pub const _SC_V6_LPBIG_OFFBIG : ::c_int = 112;
pub const _SC_V7_ILP32_OFF32 : ::c_int = 113;
pub const _SC_V7_ILP32_OFFBIG : ::c_int = 114;
pub const _SC_V7_LP64_OFF64 : ::c_int = 115;
pub const _SC_V7_LPBIG_OFFBIG : ::c_int = 116;
pub const _SC_XOPEN_CRYPT : ::c_int = 117;
pub const _SC_XOPEN_ENH_I18N : ::c_int = 118;
pub const _SC_XOPEN_LEGACY : ::c_int = 119;
pub const _SC_XOPEN_REALTIME : ::c_int = 120;
pub const _SC_XOPEN_REALTIME_THREADS : ::c_int = 121;
pub const _SC_XOPEN_STREAMS : ::c_int = 122;
pub const _SC_XOPEN_UNIX : ::c_int = 123;
pub const _SC_XOPEN_UUCP : ::c_int = 124;
pub const _SC_XOPEN_VERSION : ::c_int = 125;
pub const _SC_PHYS_PAGES : ::c_int = 500;
pub const _SC_AVPHYS_PAGES : ::c_int = 501;
pub const _SC_NPROCESSORS_CONF : ::c_int = 502;
pub const _SC_NPROCESSORS_ONLN : ::c_int = 503;

pub const FD_SETSIZE: usize = 1024;

pub const ST_NOSUID: ::c_ulong = 2;

pub const PTHREAD_MUTEX_INITIALIZER: pthread_mutex_t = 0 as *mut _;
pub const PTHREAD_COND_INITIALIZER: pthread_cond_t = 0 as *mut _;
pub const PTHREAD_RWLOCK_INITIALIZER: pthread_rwlock_t = 0 as *mut _;

pub const PTHREAD_MUTEX_ERRORCHECK: ::c_int = 1;
pub const PTHREAD_MUTEX_RECURSIVE: ::c_int = 2;
pub const PTHREAD_MUTEX_NORMAL: ::c_int = 3;
pub const PTHREAD_MUTEX_STRICT_NP: ::c_int = 4;
pub const PTHREAD_MUTEX_DEFAULT: ::c_int = PTHREAD_MUTEX_STRICT_NP;

pub const EVFILT_AIO: ::int16_t = -3;
pub const EVFILT_PROC: ::int16_t = -5;
pub const EVFILT_READ: ::int16_t = -1;
pub const EVFILT_SIGNAL: ::int16_t = -6;
pub const EVFILT_TIMER: ::int16_t = -7;
pub const EVFILT_VNODE: ::int16_t = -4;
pub const EVFILT_WRITE: ::int16_t = -2;

pub const EV_ADD: ::uint16_t = 0x1;
pub const EV_DELETE: ::uint16_t = 0x2;
pub const EV_ENABLE: ::uint16_t = 0x4;
pub const EV_DISABLE: ::uint16_t = 0x8;
pub const EV_ONESHOT: ::uint16_t = 0x10;
pub const EV_CLEAR: ::uint16_t = 0x20;
pub const EV_RECEIPT: ::uint16_t = 0x40;
pub const EV_DISPATCH: ::uint16_t = 0x80;
pub const EV_FLAG1: ::uint16_t = 0x2000;
pub const EV_ERROR: ::uint16_t = 0x4000;
pub const EV_EOF: ::uint16_t = 0x8000;
pub const EV_SYSFLAGS: ::uint16_t = 0xf000;

pub const NOTE_LOWAT: ::uint32_t = 0x00000001;
pub const NOTE_EOF: ::uint32_t = 0x00000002;
pub const NOTE_DELETE: ::uint32_t = 0x00000001;
pub const NOTE_WRITE: ::uint32_t = 0x00000002;
pub const NOTE_EXTEND: ::uint32_t = 0x00000004;
pub const NOTE_ATTRIB: ::uint32_t = 0x00000008;
pub const NOTE_LINK: ::uint32_t = 0x00000010;
pub const NOTE_RENAME: ::uint32_t = 0x00000020;
pub const NOTE_REVOKE: ::uint32_t = 0x00000040;
pub const NOTE_TRUNCATE: ::uint32_t = 0x00000080;
pub const NOTE_EXIT: ::uint32_t = 0x80000000;
pub const NOTE_FORK: ::uint32_t = 0x40000000;
pub const NOTE_EXEC: ::uint32_t = 0x20000000;
pub const NOTE_PDATAMASK: ::uint32_t = 0x000fffff;
pub const NOTE_PCTRLMASK: ::uint32_t = 0xf0000000;
pub const NOTE_TRACK: ::uint32_t = 0x00000001;
pub const NOTE_TRACKERR: ::uint32_t = 0x00000002;
pub const NOTE_CHILD: ::uint32_t = 0x00000004;

pub const TMP_MAX : ::c_uint = 0x7fffffff;

pub const NI_MAXHOST: ::size_t = 256;

pub const RTLD_LOCAL: ::c_int = 0;
pub const CTL_MAXNAME: ::c_int = 12;
pub const CTLTYPE_NODE: ::c_int = 1;
pub const CTLTYPE_INT: ::c_int = 2;
pub const CTLTYPE_STRING: ::c_int = 3;
pub const CTLTYPE_QUAD: ::c_int = 4;
pub const CTLTYPE_STRUCT: ::c_int = 5;
pub const CTL_UNSPEC: ::c_int = 0;
pub const CTL_KERN: ::c_int = 1;
pub const CTL_VM: ::c_int = 2;
pub const CTL_FS: ::c_int = 3;
pub const CTL_NET: ::c_int = 4;
pub const CTL_DEBUG: ::c_int = 5;
pub const CTL_HW: ::c_int = 6;
pub const CTL_MACHDEP: ::c_int = 7;
pub const CTL_DDB: ::c_int = 9;
pub const CTL_VFS: ::c_int = 10;
pub const CTL_MAXID: ::c_int = 11;
pub const HW_NCPUONLINE: ::c_int = 25;
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
pub const KERN_PROF: ::c_int = 16;
pub const KERN_POSIX1: ::c_int = 17;
pub const KERN_NGROUPS: ::c_int = 18;
pub const KERN_JOB_CONTROL: ::c_int = 19;
pub const KERN_SAVED_IDS: ::c_int = 20;
pub const KERN_BOOTTIME: ::c_int = 21;
pub const KERN_DOMAINNAME: ::c_int = 22;
pub const KERN_MAXPARTITIONS: ::c_int = 23;
pub const KERN_RAWPARTITION: ::c_int = 24;
pub const KERN_MAXTHREAD: ::c_int = 25;
pub const KERN_NTHREADS: ::c_int = 26;
pub const KERN_OSVERSION: ::c_int = 27;
pub const KERN_SOMAXCONN: ::c_int = 28;
pub const KERN_SOMINCONN: ::c_int = 29;
pub const KERN_USERMOUNT: ::c_int = 30;
pub const KERN_NOSUIDCOREDUMP: ::c_int = 32;
pub const KERN_FSYNC: ::c_int = 33;
pub const KERN_SYSVMSG: ::c_int = 34;
pub const KERN_SYSVSEM: ::c_int = 35;
pub const KERN_SYSVSHM: ::c_int = 36;
pub const KERN_ARND: ::c_int = 37;
pub const KERN_MSGBUFSIZE: ::c_int = 38;
pub const KERN_MALLOCSTATS: ::c_int = 39;
pub const KERN_CPTIME: ::c_int = 40;
pub const KERN_NCHSTATS: ::c_int = 41;
pub const KERN_FORKSTAT: ::c_int = 42;
pub const KERN_NSELCOLL: ::c_int = 43;
pub const KERN_TTY: ::c_int = 44;
pub const KERN_CCPU: ::c_int = 45;
pub const KERN_FSCALE: ::c_int = 46;
pub const KERN_NPROCS: ::c_int = 47;
pub const KERN_MSGBUF: ::c_int = 48;
pub const KERN_POOL: ::c_int = 49;
pub const KERN_STACKGAPRANDOM: ::c_int = 50;
pub const KERN_SYSVIPC_INFO: ::c_int = 51;
pub const KERN_SPLASSERT: ::c_int = 54;
pub const KERN_PROC_ARGS: ::c_int = 55;
pub const KERN_NFILES: ::c_int = 56;
pub const KERN_TTYCOUNT: ::c_int = 57;
pub const KERN_NUMVNODES: ::c_int = 58;
pub const KERN_MBSTAT: ::c_int = 59;
pub const KERN_SEMINFO: ::c_int = 61;
pub const KERN_SHMINFO: ::c_int = 62;
pub const KERN_INTRCNT: ::c_int = 63;
pub const KERN_WATCHDOG: ::c_int = 64;
pub const KERN_PROC: ::c_int = 66;
pub const KERN_MAXCLUSTERS: ::c_int = 67;
pub const KERN_EVCOUNT: ::c_int = 68;
pub const KERN_TIMECOUNTER: ::c_int = 69;
pub const KERN_MAXLOCKSPERUID: ::c_int = 70;
pub const KERN_CPTIME2: ::c_int = 71;
pub const KERN_CACHEPCT: ::c_int = 72;
pub const KERN_FILE: ::c_int = 73;
pub const KERN_CONSDEV: ::c_int = 75;
pub const KERN_NETLIVELOCKS: ::c_int = 76;
pub const KERN_POOL_DEBUG: ::c_int = 77;
pub const KERN_PROC_CWD: ::c_int = 78;
pub const KERN_PROC_NOBROADCASTKILL: ::c_int = 79;
pub const KERN_PROC_VMMAP: ::c_int = 80;
pub const KERN_GLOBAL_PTRACE: ::c_int = 81;
pub const KERN_CONSBUFSIZE: ::c_int = 82;
pub const KERN_CONSBUF: ::c_int = 83;
pub const KERN_AUDIO: ::c_int = 84;
pub const KERN_CPUSTATS: ::c_int = 85;
pub const KERN_MAXID: ::c_int = 86;
pub const KERN_PROC_ALL: ::c_int = 0;
pub const KERN_PROC_PID: ::c_int = 1;
pub const KERN_PROC_PGRP: ::c_int = 2;
pub const KERN_PROC_SESSION: ::c_int = 3;
pub const KERN_PROC_TTY: ::c_int = 4;
pub const KERN_PROC_UID: ::c_int = 5;
pub const KERN_PROC_RUID: ::c_int = 6;
pub const KERN_PROC_KTHREAD: ::c_int = 7;
pub const KERN_PROC_SHOW_THREADS: ::c_int = 0x40000000;
pub const KERN_SYSVIPC_MSG_INFO: ::c_int = 1;
pub const KERN_SYSVIPC_SEM_INFO: ::c_int = 2;
pub const KERN_SYSVIPC_SHM_INFO: ::c_int = 3;
pub const KERN_PROC_ARGV: ::c_int = 1;
pub const KERN_PROC_NARGV: ::c_int = 2;
pub const KERN_PROC_ENV: ::c_int = 3;
pub const KERN_PROC_NENV: ::c_int = 4;
pub const KI_NGROUPS: ::c_int = 16;
pub const KI_MAXCOMLEN: ::c_int = 24;
pub const KI_WMESGLEN: ::c_int = 8;
pub const KI_MAXLOGNAME: ::c_int = 32;
pub const KI_EMULNAMELEN: ::c_int = 8;

pub const CHWFLOW: ::tcflag_t = ::MDMBUF | ::CRTSCTS;
pub const OLCUC: ::tcflag_t = 0x20;
pub const ONOCR: ::tcflag_t = 0x40;
pub const ONLRET: ::tcflag_t = 0x80;

pub const SOCK_CLOEXEC: ::c_int = 0x8000;
pub const SOCK_NONBLOCK: ::c_int = 0x4000;
pub const SOCK_DNS: ::c_int = 0x1000;

pub const PTRACE_FORK: ::c_int = 0x0002;

pub const WCONTINUED: ::c_int = 8;

f! {
    pub fn WIFCONTINUED(status: ::c_int) -> bool {
        status & 0o177777 == 0o177777
    }
}

extern {
    pub fn chflags(path: *const ::c_char, flags: ::c_uint) -> ::c_int;
    pub fn fchflags(fd: ::c_int, flags: ::c_uint) -> ::c_int;
    pub fn chflagsat(fd: ::c_int, path: *const ::c_char, flags: ::c_uint,
                     atflag: ::c_int) -> ::c_int;
    pub fn dirfd(dirp: *mut ::DIR) -> ::c_int;
    pub fn getnameinfo(sa: *const ::sockaddr,
                       salen: ::socklen_t,
                       host: *mut ::c_char,
                       hostlen: ::size_t,
                       serv: *mut ::c_char,
                       servlen: ::size_t,
                       flags: ::c_int) -> ::c_int;
    pub fn kevent(kq: ::c_int,
                  changelist: *const ::kevent,
                  nchanges: ::c_int,
                  eventlist: *mut ::kevent,
                  nevents: ::c_int,
                  timeout: *const ::timespec) -> ::c_int;
    pub fn mprotect(addr: *mut ::c_void, len: ::size_t, prot: ::c_int)
                    -> ::c_int;
    pub fn pthread_main_np() -> ::c_int;
    pub fn pthread_set_name_np(tid: ::pthread_t, name: *const ::c_char);
    pub fn pthread_stackseg_np(thread: ::pthread_t,
                               sinfo: *mut ::stack_t) -> ::c_int;
    pub fn sysctl(name: *const ::c_int,
                  namelen: ::c_uint,
                  oldp: *mut ::c_void,
                  oldlenp: *mut ::size_t,
                  newp: *mut ::c_void,
                  newlen: ::size_t)
                  -> ::c_int;
    pub fn getentropy(buf: *mut ::c_void, buflen: ::size_t) -> ::c_int;
    pub fn setresgid(rgid: ::gid_t, egid: ::gid_t, sgid: ::gid_t) -> ::c_int;
    pub fn setresuid(ruid: ::uid_t, euid: ::uid_t, suid: ::uid_t) -> ::c_int;
    pub fn ptrace(request: ::c_int,
                  pid: ::pid_t,
                  addr: caddr_t,
                  data: ::c_int) -> ::c_int;
}

cfg_if! {
    if #[cfg(target_os = "openbsd")] {
        mod openbsd;
        pub use self::openbsd::*;
    } else if #[cfg(target_os = "bitrig")] {
        mod bitrig;
        pub use self::bitrig::*;
    } else {
        // Unknown target_os
    }
}
