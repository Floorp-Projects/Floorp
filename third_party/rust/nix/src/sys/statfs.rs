use std::fmt::{self, Debug};
use std::mem;
use std::os::unix::io::AsRawFd;
#[cfg(not(any(target_os = "linux", target_os = "android")))]
use std::ffi::CStr;

use libc;

use {NixPath, Result};
use errno::Errno;

#[cfg(target_os = "android")]
pub type fsid_t = libc::__fsid_t;
#[cfg(not(target_os = "android"))]
pub type fsid_t = libc::fsid_t;

#[derive(Clone, Copy)]
pub struct Statfs(libc::statfs);

#[cfg(target_os = "freebsd")]
#[derive(Eq, Copy, Clone, PartialEq, Debug)]
pub struct FsType(u32);
#[cfg(target_os = "android")]
#[derive(Eq, Copy, Clone, PartialEq, Debug)]
pub struct FsType(libc::c_ulong);
#[cfg(all(target_os = "linux", target_arch = "s390x"))]
#[derive(Eq, Copy, Clone, PartialEq, Debug)]
pub struct FsType(u32);
#[cfg(all(target_os = "linux", target_env = "musl"))]
#[derive(Eq, Copy, Clone, PartialEq, Debug)]
pub struct FsType(libc::c_ulong);
#[cfg(all(target_os = "linux", not(any(target_arch = "s390x", target_env = "musl"))))]
#[derive(Eq, Copy, Clone, PartialEq, Debug)]
pub struct FsType(libc::c_long);

#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const ADFS_SUPER_MAGIC: FsType = FsType(libc::ADFS_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const AFFS_SUPER_MAGIC: FsType = FsType(libc::AFFS_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const CODA_SUPER_MAGIC: FsType = FsType(libc::CODA_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const CRAMFS_MAGIC: FsType = FsType(libc::CRAMFS_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const EFS_SUPER_MAGIC: FsType = FsType(libc::EFS_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const EXT2_SUPER_MAGIC: FsType = FsType(libc::EXT2_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const EXT3_SUPER_MAGIC: FsType = FsType(libc::EXT3_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const EXT4_SUPER_MAGIC: FsType = FsType(libc::EXT4_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const HPFS_SUPER_MAGIC: FsType = FsType(libc::HPFS_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const HUGETLBFS_MAGIC: FsType = FsType(libc::HUGETLBFS_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const ISOFS_SUPER_MAGIC: FsType = FsType(libc::ISOFS_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const JFFS2_SUPER_MAGIC: FsType = FsType(libc::JFFS2_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const MINIX_SUPER_MAGIC: FsType = FsType(libc::MINIX_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const MINIX_SUPER_MAGIC2: FsType = FsType(libc::MINIX_SUPER_MAGIC2);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const MINIX2_SUPER_MAGIC: FsType = FsType(libc::MINIX2_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const MINIX2_SUPER_MAGIC2: FsType = FsType(libc::MINIX2_SUPER_MAGIC2);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const MSDOS_SUPER_MAGIC: FsType = FsType(libc::MSDOS_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const NCP_SUPER_MAGIC: FsType = FsType(libc::NCP_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const NFS_SUPER_MAGIC: FsType = FsType(libc::NFS_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const OPENPROM_SUPER_MAGIC: FsType = FsType(libc::OPENPROM_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const PROC_SUPER_MAGIC: FsType = FsType(libc::PROC_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const QNX4_SUPER_MAGIC: FsType = FsType(libc::QNX4_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const REISERFS_SUPER_MAGIC: FsType = FsType(libc::REISERFS_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const SMB_SUPER_MAGIC: FsType = FsType(libc::SMB_SUPER_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const TMPFS_MAGIC: FsType = FsType(libc::TMPFS_MAGIC);
#[cfg(all(target_os = "linux", not(target_env = "musl"), not(target_arch = "s390x")))]
pub const USBDEVICE_SUPER_MAGIC: FsType = FsType(libc::USBDEVICE_SUPER_MAGIC);

impl Statfs {
    /// Magic code defining system type
    #[cfg(not(any(
        target_os = "openbsd",
        target_os = "ios",
        target_os = "macos"
    )))]
    pub fn filesystem_type(&self) -> FsType {
        FsType(self.0.f_type)
    }

    /// Magic code defining system type
    #[cfg(not(any(target_os = "linux", target_os = "android")))]
    pub fn filesystem_type_name(&self) -> &str {
        let c_str = unsafe { CStr::from_ptr(self.0.f_fstypename.as_ptr()) };
        c_str.to_str().unwrap()
    }

    /// Optimal transfer block size
    #[cfg(any(target_os = "ios", target_os = "macos", target_os = "openbsd"))]
    pub fn optimal_transfer_size(&self) -> i32 {
        self.0.f_iosize
    }

    /// Optimal transfer block size
    #[cfg(all(target_os = "linux", target_arch = "s390x"))]
    pub fn optimal_transfer_size(&self) -> u32 {
        self.0.f_bsize
    }

    /// Optimal transfer block size
    #[cfg(all(target_os = "linux", target_env = "musl"))]
    pub fn optimal_transfer_size(&self) -> libc::c_ulong {
        self.0.f_bsize
    }

    /// Optimal transfer block size
    #[cfg(all(target_os = "linux", not(any(target_arch = "s390x", target_env = "musl"))))]
    pub fn optimal_transfer_size(&self) -> libc::c_long {
        self.0.f_bsize
    }

    /// Optimal transfer block size
    #[cfg(target_os = "android")]
    pub fn optimal_transfer_size(&self) -> libc::c_ulong {
        self.0.f_bsize
    }

    /// Optimal transfer block size
    #[cfg(target_os = "dragonfly")]
    pub fn optimal_transfer_size(&self) -> libc::c_long {
        self.0.f_iosize
    }

    /// Optimal transfer block size
    #[cfg(target_os = "freebsd")]
    pub fn optimal_transfer_size(&self) -> u64 {
        self.0.f_iosize
    }

    /// Size of a block
    #[cfg(any(target_os = "ios", target_os = "macos", target_os = "openbsd"))]
    pub fn block_size(&self) -> u32 {
        self.0.f_bsize
    }

    /// Size of a block
    // f_bsize on linux: https://github.com/torvalds/linux/blob/master/fs/nfs/super.c#L471
    #[cfg(all(target_os = "linux", target_arch = "s390x"))]
    pub fn block_size(&self) -> u32 {
        self.0.f_bsize
    }

    /// Size of a block
    // f_bsize on linux: https://github.com/torvalds/linux/blob/master/fs/nfs/super.c#L471
    #[cfg(all(target_os = "linux", target_env = "musl"))]
    pub fn block_size(&self) -> libc::c_ulong {
        self.0.f_bsize
    }

    /// Size of a block
    // f_bsize on linux: https://github.com/torvalds/linux/blob/master/fs/nfs/super.c#L471
    #[cfg(all(target_os = "linux", not(any(target_arch = "s390x", target_env = "musl"))))]
    pub fn block_size(&self) -> libc::c_long {
        self.0.f_bsize
    }

    /// Size of a block
    #[cfg(target_os = "freebsd")]
    pub fn block_size(&self) -> u64 {
        self.0.f_bsize
    }

    /// Size of a block
    #[cfg(target_os = "android")]
    pub fn block_size(&self) -> libc::c_ulong {
        self.0.f_bsize
    }

    /// Size of a block
    #[cfg(target_os = "dragonfly")]
    pub fn block_size(&self) -> libc::c_long {
        self.0.f_bsize
    }

    /// Maximum length of filenames
    #[cfg(any(target_os = "freebsd", target_os = "openbsd"))]
    pub fn maximum_name_length(&self) -> u32 {
        self.0.f_namemax
    }

    /// Maximum length of filenames
    #[cfg(all(target_os = "linux", target_arch = "s390x"))]
    pub fn maximum_name_length(&self) -> u32 {
        self.0.f_namelen
    }

    /// Maximum length of filenames
    #[cfg(all(target_os = "linux", target_env = "musl"))]
    pub fn maximum_name_length(&self) -> libc::c_ulong {
        self.0.f_namelen
    }

    /// Maximum length of filenames
    #[cfg(all(target_os = "linux", not(any(target_arch = "s390x", target_env = "musl"))))]
    pub fn maximum_name_length(&self) -> libc::c_long {
        self.0.f_namelen
    }

    /// Maximum length of filenames
    #[cfg(target_os = "android")]
    pub fn maximum_name_length(&self) -> libc::c_ulong {
        self.0.f_namelen
    }

    /// Total data blocks in filesystem
    #[cfg(any(
        target_os = "ios",
        target_os = "macos",
        target_os = "android",
        target_os = "freebsd",
        target_os = "openbsd",
    ))]
    pub fn blocks(&self) -> u64 {
        self.0.f_blocks
    }

    /// Total data blocks in filesystem
    #[cfg(target_os = "dragonfly")]
    pub fn blocks(&self) -> libc::c_long {
        self.0.f_blocks
    }

    /// Total data blocks in filesystem
    #[cfg(all(target_os = "linux", target_env = "musl"))]
    pub fn blocks(&self) -> u64 {
        self.0.f_blocks
    }

    /// Total data blocks in filesystem
    #[cfg(not(any(
        target_os = "ios",
        target_os = "macos",
        target_os = "android",
        target_os = "freebsd",
        target_os = "openbsd",
        target_os = "dragonfly",
        all(target_os = "linux", target_env = "musl")
    )))]
    pub fn blocks(&self) -> libc::c_ulong {
        self.0.f_blocks
    }

    /// Free blocks in filesystem
    #[cfg(any(
        target_os = "ios",
        target_os = "macos",
        target_os = "android",
        target_os = "freebsd",
        target_os = "openbsd",
    ))]
    pub fn blocks_free(&self) -> u64 {
        self.0.f_bfree
    }

    /// Free blocks in filesystem
    #[cfg(target_os = "dragonfly")]
    pub fn blocks_free(&self) -> libc::c_long {
        self.0.f_bfree
    }

    /// Free blocks in filesystem
    #[cfg(all(target_os = "linux", target_env = "musl"))]
    pub fn blocks_free(&self) -> u64 {
        self.0.f_bfree
    }

    /// Free blocks in filesystem
    #[cfg(not(any(
        target_os = "ios",
        target_os = "macos",
        target_os = "android",
        target_os = "freebsd",
        target_os = "openbsd",
        target_os = "dragonfly",
        all(target_os = "linux", target_env = "musl")
    )))]
    pub fn blocks_free(&self) -> libc::c_ulong {
        self.0.f_bfree
    }

    /// Free blocks available to unprivileged user
    #[cfg(any(target_os = "ios", target_os = "macos", target_os = "android"))]
    pub fn blocks_available(&self) -> u64 {
        self.0.f_bavail
    }

    /// Free blocks available to unprivileged user
    #[cfg(target_os = "dragonfly")]
    pub fn blocks_available(&self) -> libc::c_long {
        self.0.f_bavail
    }

    /// Free blocks available to unprivileged user
    #[cfg(any(target_os = "freebsd", target_os = "openbsd"))]
    pub fn blocks_available(&self) -> i64 {
        self.0.f_bavail
    }

    /// Free blocks available to unprivileged user
    #[cfg(all(target_os = "linux", target_env = "musl"))]
    pub fn blocks_available(&self) -> u64 {
        self.0.f_bavail
    }

    /// Free blocks available to unprivileged user
    #[cfg(not(any(
        target_os = "ios",
        target_os = "macos",
        target_os = "android",
        target_os = "freebsd",
        target_os = "openbsd",
        target_os = "dragonfly",
        all(target_os = "linux", target_env = "musl")
    )))]
    pub fn blocks_available(&self) -> libc::c_ulong {
        self.0.f_bavail
    }

    /// Total file nodes in filesystem
    #[cfg(any(
        target_os = "ios",
        target_os = "macos",
        target_os = "android",
        target_os = "freebsd",
        target_os = "openbsd",
    ))]
    pub fn files(&self) -> u64 {
        self.0.f_files
    }

    /// Total file nodes in filesystem
    #[cfg(target_os = "dragonfly")]
    pub fn files(&self) -> libc::c_long {
        self.0.f_files
    }

    /// Total file nodes in filesystem
    #[cfg(all(target_os = "linux", target_env = "musl"))]
    pub fn files(&self) -> u64 {
        self.0.f_files
    }

    /// Total file nodes in filesystem
    #[cfg(not(any(
        target_os = "ios",
        target_os = "macos",
        target_os = "android",
        target_os = "freebsd",
        target_os = "openbsd",
        target_os = "dragonfly",
        all(target_os = "linux", target_env = "musl")
    )))]
    pub fn files(&self) -> libc::c_ulong {
        self.0.f_files
    }

    /// Free file nodes in filesystem
    #[cfg(any(target_os = "ios", target_os = "macos", target_os = "android"))]
    pub fn files_free(&self) -> u64 {
        self.0.f_ffree
    }

    /// Free file nodes in filesystem
    #[cfg(target_os = "dragonfly")]
    pub fn files_free(&self) -> libc::c_long {
        self.0.f_ffree
    }

    /// Free file nodes in filesystem
    #[cfg(any(target_os = "freebsd", target_os = "openbsd"))]
    pub fn files_free(&self) -> i64 {
        self.0.f_ffree
    }

    /// Free file nodes in filesystem
    #[cfg(all(target_os = "linux", target_env = "musl"))]
    pub fn files_free(&self) -> u64 {
        self.0.f_ffree
    }

    /// Free file nodes in filesystem
    #[cfg(not(any(
        target_os = "ios",
        target_os = "macos",
        target_os = "android",
        target_os = "freebsd",
        target_os = "openbsd",
        target_os = "dragonfly",
        all(target_os = "linux", target_env = "musl")
    )))]
    pub fn files_free(&self) -> libc::c_ulong {
        self.0.f_ffree
    }

    /// Filesystem ID
    pub fn filesystem_id(&self) -> fsid_t {
        self.0.f_fsid
    }
}

impl Debug for Statfs {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("Statfs")
            .field("optimal_transfer_size", &self.optimal_transfer_size())
            .field("block_size", &self.block_size())
            .field("blocks", &self.blocks())
            .field("blocks_free", &self.blocks_free())
            .field("blocks_available", &self.blocks_available())
            .field("files", &self.files())
            .field("files_free", &self.files_free())
            .field("filesystem_id", &self.filesystem_id())
            .finish()
    }
}

pub fn statfs<P: ?Sized + NixPath>(path: &P) -> Result<Statfs> {
    unsafe {
        let mut stat: Statfs = mem::uninitialized();
        let res = path.with_nix_path(|path| libc::statfs(path.as_ptr(), &mut stat.0))?;
        Errno::result(res).map(|_| stat)
    }
}

pub fn fstatfs<T: AsRawFd>(fd: &T) -> Result<Statfs> {
    unsafe {
        let mut stat: Statfs = mem::uninitialized();
        Errno::result(libc::fstatfs(fd.as_raw_fd(), &mut stat.0)).map(|_| stat)
    }
}

#[cfg(test)]
mod test {
    use std::fs::File;

    use sys::statfs::*;
    use sys::statvfs::*;
    use std::path::Path;

    #[test]
    fn statfs_call() {
        check_statfs("/tmp");
        check_statfs("/dev");
        check_statfs("/run");
        check_statfs("/");
    }

    #[test]
    fn fstatfs_call() {
        check_fstatfs("/tmp");
        check_fstatfs("/dev");
        check_fstatfs("/run");
        check_fstatfs("/");
    }

    fn check_fstatfs(path: &str) {
        if !Path::new(path).exists() {
            return;
        }
        let vfs = statvfs(path.as_bytes()).unwrap();
        let file = File::open(path).unwrap();
        let fs = fstatfs(&file).unwrap();
        assert_fs_equals(fs, vfs);
    }

    fn check_statfs(path: &str) {
        if !Path::new(path).exists() {
            return;
        }
        let vfs = statvfs(path.as_bytes()).unwrap();
        let fs = statfs(path.as_bytes()).unwrap();
        assert_fs_equals(fs, vfs);
    }

    fn assert_fs_equals(fs: Statfs, vfs: Statvfs) {
        assert_eq!(fs.files() as u64, vfs.files() as u64);
        assert_eq!(fs.blocks() as u64, vfs.blocks() as u64);
        assert_eq!(fs.block_size() as u64, vfs.fragment_size() as u64);
    }

    // This test is ignored because files_free/blocks_free can change after statvfs call and before
    // statfs call.
    #[test]
    #[ignore]
    fn statfs_call_strict() {
        check_statfs_strict("/tmp");
        check_statfs_strict("/dev");
        check_statfs_strict("/run");
        check_statfs_strict("/");
    }

    // This test is ignored because files_free/blocks_free can change after statvfs call and before
    // fstatfs call.
    #[test]
    #[ignore]
    fn fstatfs_call_strict() {
        check_fstatfs_strict("/tmp");
        check_fstatfs_strict("/dev");
        check_fstatfs_strict("/run");
        check_fstatfs_strict("/");
    }

    fn check_fstatfs_strict(path: &str) {
        if !Path::new(path).exists() {
            return;
        }
        let vfs = statvfs(path.as_bytes());
        let file = File::open(path).unwrap();
        let fs = fstatfs(&file);
        assert_fs_equals_strict(fs.unwrap(), vfs.unwrap())
    }

    fn check_statfs_strict(path: &str) {
        if !Path::new(path).exists() {
            return;
        }
        let vfs = statvfs(path.as_bytes());
        let fs = statfs(path.as_bytes());
        assert_fs_equals_strict(fs.unwrap(), vfs.unwrap())
    }

    fn assert_fs_equals_strict(fs: Statfs, vfs: Statvfs) {
        assert_eq!(fs.files_free() as u64, vfs.files_free() as u64);
        assert_eq!(fs.blocks_free() as u64, vfs.blocks_free() as u64);
        assert_eq!(fs.blocks_available() as u64, vfs.blocks_available() as u64);
        assert_eq!(fs.files() as u64, vfs.files() as u64);
        assert_eq!(fs.blocks() as u64, vfs.blocks() as u64);
        assert_eq!(fs.block_size() as u64, vfs.fragment_size() as u64);
    }
}
