s! {
    pub struct glob_t {
        pub gl_pathc:   ::size_t,
        pub gl_matchc:  ::size_t,
        pub gl_offs:    ::size_t,
        pub gl_flags:   ::c_int,
        pub gl_pathv:   *mut *mut ::c_char,
        __unused1: *mut ::c_void,
        __unused2: *mut ::c_void,
        __unused3: *mut ::c_void,
        __unused4: *mut ::c_void,
        __unused5: *mut ::c_void,
        __unused6: *mut ::c_void,
        __unused7: *mut ::c_void,
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
        pub int_p_sep_by_space: ::c_char,
        pub int_n_cs_precedes: ::c_char,
        pub int_n_sep_by_space: ::c_char,
        pub int_p_sign_posn: ::c_char,
        pub int_n_sign_posn: ::c_char,
    }

    pub struct ufs_args {
        pub fspec: *mut ::c_char,
        pub export_info: export_args,
    }

    pub struct mfs_args {
        pub fspec: *mut ::c_char,
        pub export_info: export_args,
        // https://github.com/openbsd/src/blob/master/sys/sys/types.h#L134
        pub base: *mut ::c_char,
        pub size: ::c_ulong,
    }

    pub struct iso_args {
        pub fspec: *mut ::c_char,
        pub export_info: export_args,
        pub flags: ::c_int,
        pub sess: ::c_int,
    }

    pub struct nfs_args {
        pub version: ::c_int,
        pub addr: *mut ::sockaddr,
        pub addrlen: ::c_int,
        pub sotype: ::c_int,
        pub proto: ::c_int,
        pub fh: *mut ::c_uchar,
        pub fhsize: ::c_int,
        pub flags: ::c_int,
        pub wsize: ::c_int,
        pub rsize: ::c_int,
        pub readdirsize: ::c_int,
        pub timeo: ::c_int,
        pub retrans: ::c_int,
        pub maxgrouplist: ::c_int,
        pub readahead: ::c_int,
        pub leaseterm: ::c_int,
        pub deadthresh: ::c_int,
        pub hostname: *mut ::c_char,
        pub acregmin: ::c_int,
        pub acregmax: ::c_int,
        pub acdirmin: ::c_int,
        pub acdirmax: ::c_int,
    }

    pub struct msdosfs_args {
        pub fspec: *mut ::c_char,
        pub export_info: export_args,
        pub uid: ::uid_t,
        pub gid: ::gid_t,
        pub mask: ::mode_t,
        pub flags: ::c_int,
    }

    pub struct ntfs_args {
        pub fspec: *mut ::c_char,
        pub export_info: export_args,
        pub uid: ::uid_t,
        pub gid: ::gid_t,
        pub mode: ::mode_t,
        pub flag: ::c_ulong,
    }

    pub struct udf_args {
        pub fspec: *mut ::c_char,
        pub lastblock: ::uint32_t,
    }

    pub struct tmpfs_args {
        pub ta_version: ::c_int,
        pub ta_nodes_max: ::ino_t,
        pub ta_size_max: ::off_t,
        pub ta_root_uid: ::uid_t,
        pub ta_root_gid: ::gid_t,
        pub ta_root_mode: ::mode_t,
    }

    pub struct fusefs_args {
        pub name: *mut ::c_char,
        pub fd: ::c_int,
        pub max_read: ::c_int,
        pub allow_other: ::c_int,
    }

    pub struct xucred {
        pub cr_uid: ::uid_t,
        pub cr_gid: ::gid_t,
        pub cr_ngroups: ::c_short,
        //https://github.com/openbsd/src/blob/master/sys/sys/syslimits.h#L44
        pub cr_groups: [::gid_t; 16],
    }

    pub struct export_args {
        pub ex_flags: ::c_int,
        pub ex_root: ::uid_t,
        pub ex_anon: xucred,
        pub ex_addr: *mut ::sockaddr,
        pub ex_addrlen: ::c_int,
        pub ex_mask: *mut ::sockaddr,
        pub ex_masklen: ::c_int,
    }
}

s_no_extra_traits! {
    pub union mount_info {
        pub ufs_args: ufs_args,
        pub mfs_args: mfs_args,
        pub nfs_args: nfs_args,
        pub iso_args: iso_args,
        pub msdosfs_args: msdosfs_args,
        pub ntfs_args: ntfs_args,
        pub tmpfs_args: tmpfs_args,
        align: [::c_char; 160],
    }
}

cfg_if! {
    if #[cfg(feature = "extra_traits")] {
        impl PartialEq for mount_info {
            fn eq(&self, other: &mount_info) -> bool {
                unsafe {
                    self.align
                        .iter()
                        .zip(other.align.iter())
                        .all(|(a,b)| a == b)
                }
            }
        }

        impl Eq for mount_info { }

        impl ::fmt::Debug for mount_info {
            fn fmt(&self, f: &mut ::fmt::Formatter) -> ::fmt::Result {
                f.debug_struct("mount_info")
                // FIXME: .field("align", &self.align)
                    .finish()
            }
        }

        impl ::hash::Hash for mount_info {
            fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                unsafe { self.align.hash(state) };
            }
        }
    }
}

cfg_if! {
    if #[cfg(libc_union)] {
        s_no_extra_traits! {
            // This type uses the union mount_info:
            pub struct statfs {
                pub f_flags: ::uint32_t,
                pub f_bsize: ::uint32_t,
                pub f_iosize: ::uint32_t,
                pub f_blocks: ::uint64_t,
                pub f_bfree: ::uint64_t,
                pub f_bavail: ::int64_t,
                pub f_files: ::uint64_t,
                pub f_ffree: ::uint64_t,
                pub f_favail: ::int64_t,
                pub f_syncwrites: ::uint64_t,
                pub f_syncreads: ::uint64_t,
                pub f_asyncwrites: ::uint64_t,
                pub f_asyncreads: ::uint64_t,
                pub f_fsid: ::fsid_t,
                pub f_namemax: ::uint32_t,
                pub f_owner: ::uid_t,
                pub f_ctime: ::uint64_t,
                pub f_fstypename: [::c_char; 16],
                pub f_mntonname: [::c_char; 90],
                pub f_mntfromname: [::c_char; 90],
                pub f_mntfromspec: [::c_char; 90],
                pub mount_info: mount_info,
            }
        }

        cfg_if! {
            if #[cfg(feature = "extra_traits")] {
                impl PartialEq for statfs {
                    fn eq(&self, other: &statfs) -> bool {
                        self.f_flags == other.f_flags
                            && self.f_bsize == other.f_bsize
                            && self.f_iosize == other.f_iosize
                            && self.f_blocks == other.f_blocks
                            && self.f_bfree == other.f_bfree
                            && self.f_bavail == other.f_bavail
                            && self.f_files == other.f_files
                            && self.f_ffree == other.f_ffree
                            && self.f_favail == other.f_favail
                            && self.f_syncwrites == other.f_syncwrites
                            && self.f_syncreads == other.f_syncreads
                            && self.f_asyncwrites == other.f_asyncwrites
                            && self.f_asyncreads == other.f_asyncreads
                            && self.f_fsid == other.f_fsid
                            && self.f_namemax == other.f_namemax
                            && self.f_owner == other.f_owner
                            && self.f_ctime == other.f_ctime
                            && self.f_fstypename
                            .iter()
                            .zip(other.f_fstypename.iter())
                            .all(|(a,b)| a == b)
                            && self.f_mntonname
                            .iter()
                            .zip(other.f_mntonname.iter())
                            .all(|(a,b)| a == b)
                            && self.f_mntfromname
                            .iter()
                            .zip(other.f_mntfromname.iter())
                            .all(|(a,b)| a == b)
                            && self.f_mntfromspec
                            .iter()
                            .zip(other.f_mntfromspec.iter())
                            .all(|(a,b)| a == b)
                            && self.mount_info == other.mount_info
                    }
                }

                impl Eq for statfs { }

                impl ::fmt::Debug for statfs {
                    fn fmt(&self, f: &mut ::fmt::Formatter)
                           -> ::fmt::Result {
                        f.debug_struct("statfs")
                            .field("f_flags", &self.f_flags)
                            .field("f_bsize", &self.f_bsize)
                            .field("f_iosize", &self.f_iosize)
                            .field("f_blocks", &self.f_blocks)
                            .field("f_bfree", &self.f_bfree)
                            .field("f_bavail", &self.f_bavail)
                            .field("f_files", &self.f_files)
                            .field("f_ffree", &self.f_ffree)
                            .field("f_favail", &self.f_favail)
                            .field("f_syncwrites", &self.f_syncwrites)
                            .field("f_syncreads", &self.f_syncreads)
                            .field("f_asyncwrites", &self.f_asyncwrites)
                            .field("f_asyncreads", &self.f_asyncreads)
                            .field("f_fsid", &self.f_fsid)
                            .field("f_namemax", &self.f_namemax)
                            .field("f_owner", &self.f_owner)
                            .field("f_ctime", &self.f_ctime)
                        // FIXME: .field("f_fstypename", &self.f_fstypename)
                        // FIXME: .field("f_mntonname", &self.f_mntonname)
                        // FIXME: .field("f_mntfromname", &self.f_mntfromname)
                        // FIXME: .field("f_mntfromspec", &self.f_mntfromspec)
                            .field("mount_info", &self.mount_info)
                            .finish()
                    }
                }

                impl ::hash::Hash for statfs {
                    fn hash<H: ::hash::Hasher>(&self, state: &mut H) {
                        self.f_flags.hash(state);
                        self.f_bsize.hash(state);
                        self.f_iosize.hash(state);
                        self.f_blocks.hash(state);
                        self.f_bfree.hash(state);
                        self.f_bavail.hash(state);
                        self.f_files.hash(state);
                        self.f_ffree.hash(state);
                        self.f_favail.hash(state);
                        self.f_syncwrites.hash(state);
                        self.f_syncreads.hash(state);
                        self.f_asyncwrites.hash(state);
                        self.f_asyncreads.hash(state);
                        self.f_fsid.hash(state);
                        self.f_namemax.hash(state);
                        self.f_owner.hash(state);
                        self.f_ctime.hash(state);
                        self.f_fstypename.hash(state);
                        self.f_mntonname.hash(state);
                        self.f_mntfromname.hash(state);
                        self.f_mntfromspec.hash(state);
                        self.mount_info.hash(state);
                    }
                }
            }
        }
    }
}

//https://github.com/openbsd/src/blob/master/sys/sys/mount.h
pub const ISOFSMNT_NORRIP: ::c_int = 0x1; // disable Rock Ridge Ext
pub const ISOFSMNT_GENS: ::c_int = 0x2; // enable generation numbers
pub const ISOFSMNT_EXTATT: ::c_int = 0x4; // enable extended attr
pub const ISOFSMNT_NOJOLIET: ::c_int = 0x8; // disable Joliet Ext
pub const ISOFSMNT_SESS: ::c_int = 0x10; // use iso_args.sess

pub const NFS_ARGSVERSION: ::c_int = 4; // change when nfs_args changes

pub const NFSMNT_RESVPORT: ::c_int = 0; // always use reserved ports
pub const NFSMNT_SOFT: ::c_int = 0x1; // soft mount (hard is default)
pub const NFSMNT_WSIZE: ::c_int = 0x2; // set write size
pub const NFSMNT_RSIZE: ::c_int = 0x4; // set read size
pub const NFSMNT_TIMEO: ::c_int = 0x8; // set initial timeout
pub const NFSMNT_RETRANS: ::c_int = 0x10; // set number of request retries
pub const NFSMNT_MAXGRPS: ::c_int = 0x20; // set maximum grouplist size
pub const NFSMNT_INT: ::c_int = 0x40; // allow interrupts on hard mount
pub const NFSMNT_NOCONN: ::c_int = 0x80; // Don't Connect the socket
pub const NFSMNT_NQNFS: ::c_int = 0x100; // Use Nqnfs protocol
pub const NFSMNT_NFSV3: ::c_int = 0x200; // Use NFS Version 3 protocol
pub const NFSMNT_KERB: ::c_int = 0x400; // Use Kerberos authentication
pub const NFSMNT_DUMBTIMR: ::c_int = 0x800; // Don't estimate rtt dynamically
pub const NFSMNT_LEASETERM: ::c_int = 0x1000; // set lease term (nqnfs)
pub const NFSMNT_READAHEAD: ::c_int = 0x2000; // set read ahead
pub const NFSMNT_DEADTHRESH: ::c_int = 0x4000; // set dead server retry thresh
pub const NFSMNT_NOAC: ::c_int = 0x8000; // disable attribute cache
pub const NFSMNT_RDIRPLUS: ::c_int = 0x10000; // Use Readdirplus for V3
pub const NFSMNT_READDIRSIZE: ::c_int = 0x20000; // Set readdir size

/* Flags valid only in mount syscall arguments */
pub const NFSMNT_ACREGMIN: ::c_int = 0x40000; // acregmin field valid
pub const NFSMNT_ACREGMAX: ::c_int = 0x80000; // acregmax field valid
pub const NFSMNT_ACDIRMIN: ::c_int = 0x100000; // acdirmin field valid
pub const NFSMNT_ACDIRMAX: ::c_int = 0x200000; // acdirmax field valid

/* Flags valid only in kernel */
pub const NFSMNT_INTERNAL: ::c_int = 0xfffc0000; // Bits set internally
pub const NFSMNT_HASWRITEVERF: ::c_int = 0x40000; // Has write verifier for V3
pub const NFSMNT_GOTPATHCONF: ::c_int = 0x80000; // Got the V3 pathconf info
pub const NFSMNT_GOTFSINFO: ::c_int = 0x100000; // Got the V3 fsinfo
pub const NFSMNT_MNTD: ::c_int = 0x200000; // Mnt server for mnt point
pub const NFSMNT_DISMINPROG: ::c_int = 0x400000; // Dismount in progress
pub const NFSMNT_DISMNT: ::c_int = 0x800000; // Dismounted
pub const NFSMNT_SNDLOCK: ::c_int = 0x1000000; // Send socket lock
pub const NFSMNT_WANTSND: ::c_int = 0x2000000; // Want above
pub const NFSMNT_RCVLOCK: ::c_int = 0x4000000; // Rcv socket lock
pub const NFSMNT_WANTRCV: ::c_int = 0x8000000; // Want above
pub const NFSMNT_WAITAUTH: ::c_int = 0x10000000; // Wait for authentication
pub const NFSMNT_HASAUTH: ::c_int = 0x20000000; // Has authenticator
pub const NFSMNT_WANTAUTH: ::c_int = 0x40000000; // Wants an authenticator
pub const NFSMNT_AUTHERR: ::c_int = 0x80000000; // Authentication error

pub const MSDOSFSMNT_SHORTNAME: ::c_int = 0x1; // Force old DOS short names only
pub const MSDOSFSMNT_LONGNAME: ::c_int = 0x2; // Force Win'95 long names
pub const MSDOSFSMNT_NOWIN95: ::c_int = 0x4; // Completely ignore Win95 entries

pub const NTFS_MFLAG_CASEINS: ::c_int = 0x1;
pub const NTFS_MFLAG_ALLNAMES: ::c_int = 0x2;

pub const TMPFS_ARGS_VERSION: ::c_int = 1;

pub const MAP_STACK : ::c_int = 0x4000;

// https://github.com/openbsd/src/blob/master/sys/net/if.h#L187
pub const IFF_UP: ::c_int = 0x1; // interface is up
pub const IFF_BROADCAST: ::c_int = 0x2; // broadcast address valid
pub const IFF_DEBUG: ::c_int = 0x4; // turn on debugging
pub const IFF_LOOPBACK: ::c_int = 0x8; // is a loopback net
pub const IFF_POINTOPOINT: ::c_int = 0x10; // interface is point-to-point link
pub const IFF_STATICARP: ::c_int = 0x20; // only static ARP
pub const IFF_RUNNING: ::c_int = 0x40; // resources allocated
pub const IFF_NOARP: ::c_int = 0x80; // no address resolution protocol
pub const IFF_PROMISC: ::c_int = 0x100; // receive all packets
pub const IFF_ALLMULTI: ::c_int = 0x200; // receive all multicast packets
pub const IFF_OACTIVE: ::c_int = 0x400; // transmission in progress
pub const IFF_SIMPLEX: ::c_int = 0x800; // can't hear own transmissions
pub const IFF_LINK0: ::c_int = 0x1000; // per link layer defined bit
pub const IFF_LINK1: ::c_int = 0x2000; // per link layer defined bit
pub const IFF_LINK2: ::c_int = 0x4000; // per link layer defined bit
pub const IFF_MULTICAST: ::c_int = 0x8000; // supports multicast

pub const PTHREAD_STACK_MIN : ::size_t = 4096;
pub const SIGSTKSZ : ::size_t = 28672;

pub const PT_FIRSTMACH: ::c_int = 32;

fn _ALIGN(p: usize) -> usize {
    (p + _ALIGNBYTES) & !_ALIGNBYTES
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

    pub fn CMSG_SPACE(length: ::c_uint) -> ::c_uint {
        (_ALIGN(::mem::size_of::<::cmsghdr>()) + _ALIGN(length as usize))
            as ::c_uint
    }

    pub fn WSTOPSIG(status: ::c_int) -> ::c_int {
        status >> 8
    }

    pub fn WIFSIGNALED(status: ::c_int) -> bool {
        (status & 0o177) != 0o177 && (status & 0o177) != 0
    }

    pub fn WIFSTOPPED(status: ::c_int) -> bool {
        (status & 0o177) == 0o177
    }
}

extern {
    pub fn accept4(s: ::c_int, addr: *mut ::sockaddr,
                   addrlen: *mut ::socklen_t, flags: ::c_int) -> ::c_int;
    pub fn execvpe(file: *const ::c_char, argv: *const *const ::c_char,
                   envp: *const *const ::c_char) -> ::c_int;
    pub fn pledge(promises: *const ::c_char,
                  execpromises: *const ::c_char) -> ::c_int;
    pub fn strtonum(nptr: *const ::c_char, minval: ::c_longlong,
                    maxval: ::c_longlong,
                    errstr: *mut *const ::c_char) -> ::c_longlong;
    pub fn dup3(src: ::c_int, dst: ::c_int, flags: ::c_int) -> ::c_int;
}

cfg_if! {
    if #[cfg(libc_union)] {
        extern {
            // these functions use statfs which uses the union mount_info:
            pub fn statfs(path: *const ::c_char, buf: *mut statfs) -> ::c_int;
            pub fn fstatfs(fd: ::c_int, buf: *mut statfs) -> ::c_int;
        }
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
    } else {
        // Unknown target_arch
    }
}
