//! 32-bit specific definitions for linux-like values

use pthread_mutex_t;

pub type c_long = i32;
pub type c_ulong = u32;
pub type clock_t = i32;

pub type shmatt_t = ::c_ulong;
pub type msgqnum_t = ::c_ulong;
pub type msglen_t = ::c_ulong;
pub type nlink_t = u32;
pub type __u64 = ::c_ulonglong;
pub type __fsword_t = i32;
pub type fsblkcnt64_t = u64;
pub type fsfilcnt64_t = u64;

cfg_if! {
    if #[cfg(target_arch = "riscv32")] {
        pub type time_t = i64;
        pub type suseconds_t = i64;
        pub type ino_t = u64;
        pub type off_t = i64;
        pub type blkcnt_t = i64;
        pub type fsblkcnt_t = u64;
        pub type fsfilcnt_t = u64;
        pub type rlim_t = u64;
        pub type blksize_t = i64;
    } else {
        pub type time_t = i32;
        pub type suseconds_t = i32;
        pub type ino_t = u32;
        pub type off_t = i32;
        pub type blkcnt_t = i32;
        pub type fsblkcnt_t = ::c_ulong;
        pub type fsfilcnt_t = ::c_ulong;
        pub type rlim_t = c_ulong;
        pub type blksize_t = i32;
    }
}

s! {
    pub struct stat {
        #[cfg(not(target_arch = "mips"))]
        pub st_dev: ::dev_t,
        #[cfg(target_arch = "mips")]
        pub st_dev: ::c_ulong,

        #[cfg(not(target_arch = "mips"))]
        __pad1: ::c_short,
        #[cfg(target_arch = "mips")]
        st_pad1: [::c_long; 3],
        pub st_ino: ::ino_t,
        pub st_mode: ::mode_t,
        pub st_nlink: ::nlink_t,
        pub st_uid: ::uid_t,
        pub st_gid: ::gid_t,
        #[cfg(not(target_arch = "mips"))]
        pub st_rdev: ::dev_t,
        #[cfg(target_arch = "mips")]
        pub st_rdev: ::c_ulong,
        #[cfg(not(target_arch = "mips"))]
        __pad2: ::c_short,
        #[cfg(target_arch = "mips")]
        st_pad2: [::c_long; 2],
        pub st_size: ::off_t,
        #[cfg(target_arch = "mips")]
        st_pad3: ::c_long,
        #[cfg(not(target_arch = "mips"))]
        pub st_blksize: ::blksize_t,
        #[cfg(not(target_arch = "mips"))]
        pub st_blocks: ::blkcnt_t,
        pub st_atime: ::time_t,
        pub st_atime_nsec: ::c_long,
        pub st_mtime: ::time_t,
        pub st_mtime_nsec: ::c_long,
        pub st_ctime: ::time_t,
        pub st_ctime_nsec: ::c_long,
        #[cfg(not(target_arch = "mips"))]
        __unused4: ::c_long,
        #[cfg(not(target_arch = "mips"))]
        __unused5: ::c_long,
        #[cfg(target_arch = "mips")]
        pub st_blksize: ::blksize_t,
        #[cfg(target_arch = "mips")]
        pub st_blocks: ::blkcnt_t,
        #[cfg(target_arch = "mips")]
        st_pad5: [::c_long; 14],
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
        __f_unused: ::c_int,
        pub f_flag: ::c_ulong,
        pub f_namemax: ::c_ulong,
        __f_spare: [::c_int; 6],
    }

    pub struct pthread_attr_t {
        __size: [u32; 9]
    }

    pub struct sigset_t {
        __val: [::c_ulong; 32],
    }

    pub struct sysinfo {
        pub uptime: ::c_long,
        pub loads: [::c_ulong; 3],
        pub totalram: ::c_ulong,
        pub freeram: ::c_ulong,
        pub sharedram: ::c_ulong,
        pub bufferram: ::c_ulong,
        pub totalswap: ::c_ulong,
        pub freeswap: ::c_ulong,
        pub procs: ::c_ushort,
        #[deprecated(
            since = "0.2.58",
            note = "This padding field might become private in the future"
        )]
        pub pad: ::c_ushort,
        pub totalhigh: ::c_ulong,
        pub freehigh: ::c_ulong,
        pub mem_unit: ::c_uint,
        pub _f: [::c_char; 8],
    }

    pub struct ip_mreqn {
        pub imr_multiaddr: ::in_addr,
        pub imr_address: ::in_addr,
        pub imr_ifindex: ::c_int,
    }
}

pub const POSIX_FADV_DONTNEED: ::c_int = 4;
pub const POSIX_FADV_NOREUSE: ::c_int = 5;

pub const F_OFD_GETLK: ::c_int = 36;
pub const F_OFD_SETLK: ::c_int = 37;
pub const F_OFD_SETLKW: ::c_int = 38;

pub const __SIZEOF_PTHREAD_CONDATTR_T: usize = 4;
pub const __SIZEOF_PTHREAD_MUTEX_T: usize = 24;
pub const __SIZEOF_PTHREAD_RWLOCK_T: usize = 32;
pub const __SIZEOF_PTHREAD_MUTEXATTR_T: usize = 4;
pub const __SIZEOF_PTHREAD_RWLOCKATTR_T: usize = 8;

cfg_if! {
    if #[cfg(target_arch = "sparc")] {
        pub const O_NOATIME: ::c_int = 0x200000;
        pub const O_PATH: ::c_int = 0x1000000;
        pub const O_TMPFILE: ::c_int = 0x2000000 | O_DIRECTORY;

        pub const SA_ONSTACK: ::c_int = 1;

        pub const PTRACE_DETACH: ::c_uint = 11;

        pub const F_SETLK: ::c_int = 8;
        pub const F_SETLKW: ::c_int = 9;

        pub const F_RDLCK: ::c_int = 1;
        pub const F_WRLCK: ::c_int = 2;
        pub const F_UNLCK: ::c_int = 3;

        pub const SFD_CLOEXEC: ::c_int = 0x400000;

        pub const NCCS: usize = 17;

        pub const O_TRUNC: ::c_int = 0x400;
        pub const O_CLOEXEC: ::c_int = 0x400000;

        pub const EBFONT: ::c_int = 109;
        pub const ENOSTR: ::c_int = 72;
        pub const ENODATA: ::c_int = 111;
        pub const ETIME: ::c_int = 73;
        pub const ENOSR: ::c_int = 74;
        pub const ENONET: ::c_int = 80;
        pub const ENOPKG: ::c_int = 113;
        pub const EREMOTE: ::c_int = 71;
        pub const ENOLINK: ::c_int = 82;
        pub const EADV: ::c_int = 83;
        pub const ESRMNT: ::c_int = 84;
        pub const ECOMM: ::c_int = 85;
        pub const EPROTO: ::c_int = 86;
        pub const EDOTDOT: ::c_int = 88;

        pub const SA_NODEFER: ::c_int = 0x20;
        pub const SA_RESETHAND: ::c_int = 0x4;
        pub const SA_RESTART: ::c_int = 0x2;
        pub const SA_NOCLDSTOP: ::c_int = 0x00000008;

        pub const EPOLL_CLOEXEC: ::c_int = 0x400000;

        pub const EFD_CLOEXEC: ::c_int = 0x400000;
    } else {
        pub const O_NOATIME: ::c_int = 0o1000000;
        pub const O_PATH: ::c_int = 0o10000000;
        pub const O_TMPFILE: ::c_int = 0o20000000 | O_DIRECTORY;

        pub const SA_ONSTACK: ::c_int = 0x08000000;

        pub const PTRACE_DETACH: ::c_uint = 17;

        pub const F_SETLK: ::c_int = 6;
        pub const F_SETLKW: ::c_int = 7;

        pub const F_RDLCK: ::c_int = 0;
        pub const F_WRLCK: ::c_int = 1;
        pub const F_UNLCK: ::c_int = 2;

        pub const SFD_CLOEXEC: ::c_int = 0x080000;

        pub const NCCS: usize = 32;

        pub const O_TRUNC: ::c_int = 512;
        pub const O_CLOEXEC: ::c_int = 0x80000;
        pub const EBFONT: ::c_int = 59;
        pub const ENOSTR: ::c_int = 60;
        pub const ENODATA: ::c_int = 61;
        pub const ETIME: ::c_int = 62;
        pub const ENOSR: ::c_int = 63;
        pub const ENONET: ::c_int = 64;
        pub const ENOPKG: ::c_int = 65;
        pub const EREMOTE: ::c_int = 66;
        pub const ENOLINK: ::c_int = 67;
        pub const EADV: ::c_int = 68;
        pub const ESRMNT: ::c_int = 69;
        pub const ECOMM: ::c_int = 70;
        pub const EPROTO: ::c_int = 71;
        pub const EDOTDOT: ::c_int = 73;

        pub const SA_NODEFER: ::c_int = 0x40000000;
        pub const SA_RESETHAND: ::c_int = 0x80000000;
        pub const SA_RESTART: ::c_int = 0x10000000;
        pub const SA_NOCLDSTOP: ::c_int = 0x00000001;

        pub const EPOLL_CLOEXEC: ::c_int = 0x80000;

        pub const EFD_CLOEXEC: ::c_int = 0x80000;
    }
}

align_const! {
    #[cfg(target_endian = "little")]
    pub const PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0,
            ],
        };
    #[cfg(target_endian = "little")]
    pub const PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0,
            ],
        };
    #[cfg(target_endian = "little")]
    pub const PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0,
            ],
        };
    #[cfg(target_endian = "big")]
    pub const PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
                0, 0, 0,
            ],
        };
    #[cfg(target_endian = "big")]
    pub const PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0,
                0, 0, 0,
            ],
        };
    #[cfg(target_endian = "big")]
    pub const PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP: ::pthread_mutex_t =
        pthread_mutex_t {
            size: [
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0,
                0, 0, 0,
            ],
        };
}

pub const PTRACE_GETFPREGS: ::c_uint = 14;
pub const PTRACE_SETFPREGS: ::c_uint = 15;
pub const PTRACE_GETREGS: ::c_uint = 12;
pub const PTRACE_SETREGS: ::c_uint = 13;

pub const TIOCSBRK: ::c_int = 0x5427;
pub const TIOCCBRK: ::c_int = 0x5428;

extern "C" {
    pub fn sysctl(
        name: *mut ::c_int,
        namelen: ::c_int,
        oldp: *mut ::c_void,
        oldlenp: *mut ::size_t,
        newp: *mut ::c_void,
        newlen: ::size_t,
    ) -> ::c_int;
}

cfg_if! {
    if #[cfg(target_arch = "x86")] {
        mod x86;
        pub use self::x86::*;
    } else if #[cfg(target_arch = "arm")] {
        mod arm;
        pub use self::arm::*;
    } else if #[cfg(target_arch = "mips")] {
        mod mips;
        pub use self::mips::*;
    } else if #[cfg(target_arch = "powerpc")] {
        mod powerpc;
        pub use self::powerpc::*;
    } else if #[cfg(target_arch = "sparc")] {
        mod sparc;
        pub use self::sparc::*;
    } else if #[cfg(target_arch = "riscv32")] {
        mod riscv32;
        pub use self::riscv32::*;
    } else {
        // Unknown target_arch
    }
}
