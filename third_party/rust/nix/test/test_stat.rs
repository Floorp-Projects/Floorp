use std::fs::{self, File};
use std::os::unix::fs::{symlink, PermissionsExt};
use std::os::unix::prelude::AsRawFd;
use std::time::{Duration, UNIX_EPOCH};
use std::path::Path;

#[cfg(not(any(target_os = "netbsd")))]
use libc::{S_IFMT, S_IFLNK, mode_t};

use nix::{fcntl, Error};
use nix::errno::{Errno};
use nix::sys::stat::{self, fchmod, fchmodat, futimens, stat, utimes, utimensat, mkdirat};
#[cfg(any(target_os = "linux",
          target_os = "haiku",
          target_os = "ios",
          target_os = "macos",
          target_os = "freebsd",
          target_os = "netbsd"))]
use nix::sys::stat::lutimes;
use nix::sys::stat::{Mode, FchmodatFlags, UtimensatFlags};

#[cfg(not(any(target_os = "netbsd")))]
use nix::sys::stat::FileStat;

use nix::sys::time::{TimeSpec, TimeVal, TimeValLike};
use nix::unistd::chdir;

#[cfg(not(any(target_os = "netbsd")))]
use nix::Result;
use tempfile;

#[allow(unused_comparisons)]
// uid and gid are signed on Windows, but not on other platforms. This function
// allows warning free compiles on all platforms, and can be removed when
// expression-level #[allow] is available.
#[cfg(not(any(target_os = "netbsd")))]
fn valid_uid_gid(stat: FileStat) -> bool {
    // uid could be 0 for the `root` user. This quite possible when
    // the tests are being run on a rooted Android device.
    stat.st_uid >= 0 && stat.st_gid >= 0
}

#[cfg(not(any(target_os = "netbsd")))]
fn assert_stat_results(stat_result: Result<FileStat>) {
    let stats = stat_result.expect("stat call failed");
    assert!(stats.st_dev > 0);      // must be positive integer, exact number machine dependent
    assert!(stats.st_ino > 0);      // inode is positive integer, exact number machine dependent
    assert!(stats.st_mode > 0);     // must be positive integer
    assert!(stats.st_nlink == 1);   // there links created, must be 1
    assert!(valid_uid_gid(stats));  // must be positive integers
    assert!(stats.st_size == 0);    // size is 0 because we did not write anything to the file
    assert!(stats.st_blksize > 0);  // must be positive integer, exact number machine dependent
    assert!(stats.st_blocks <= 16);  // Up to 16 blocks can be allocated for a blank file
}

#[cfg(not(any(target_os = "netbsd")))]
fn assert_lstat_results(stat_result: Result<FileStat>) {
    let stats = stat_result.expect("stat call failed");
    assert!(stats.st_dev > 0);      // must be positive integer, exact number machine dependent
    assert!(stats.st_ino > 0);      // inode is positive integer, exact number machine dependent
    assert!(stats.st_mode > 0);     // must be positive integer

    // st_mode is c_uint (u32 on Android) while S_IFMT is mode_t
    // (u16 on Android), and that will be a compile error.
    // On other platforms they are the same (either both are u16 or u32).
    assert!((stats.st_mode as usize) & (S_IFMT as usize) == S_IFLNK as usize); // should be a link
    assert!(stats.st_nlink == 1);   // there links created, must be 1
    assert!(valid_uid_gid(stats));  // must be positive integers
    assert!(stats.st_size > 0);    // size is > 0 because it points to another file
    assert!(stats.st_blksize > 0);  // must be positive integer, exact number machine dependent

    // st_blocks depends on whether the machine's file system uses fast
    // or slow symlinks, so just make sure it's not negative
    // (Android's st_blocks is ulonglong which is always non-negative.)
    assert!(stats.st_blocks >= 0);
}

#[test]
#[cfg(not(any(target_os = "netbsd")))]
fn test_stat_and_fstat() {
    use nix::sys::stat::fstat;

    let tempdir = tempfile::tempdir().unwrap();
    let filename = tempdir.path().join("foo.txt");
    let file = File::create(&filename).unwrap();

    let stat_result = stat(&filename);
    assert_stat_results(stat_result);

    let fstat_result = fstat(file.as_raw_fd());
    assert_stat_results(fstat_result);
}

#[test]
#[cfg(not(any(target_os = "netbsd")))]
fn test_fstatat() {
    let tempdir = tempfile::tempdir().unwrap();
    let filename = tempdir.path().join("foo.txt");
    File::create(&filename).unwrap();
    let dirfd = fcntl::open(tempdir.path(),
                            fcntl::OFlag::empty(),
                            stat::Mode::empty());

    let result = stat::fstatat(dirfd.unwrap(),
                               &filename,
                               fcntl::AtFlags::empty());
    assert_stat_results(result);
}

#[test]
#[cfg(not(any(target_os = "netbsd")))]
fn test_stat_fstat_lstat() {
    use nix::sys::stat::{fstat, lstat};

    let tempdir = tempfile::tempdir().unwrap();
    let filename = tempdir.path().join("bar.txt");
    let linkname = tempdir.path().join("barlink");

    File::create(&filename).unwrap();
    symlink("bar.txt", &linkname).unwrap();
    let link = File::open(&linkname).unwrap();

    // should be the same result as calling stat,
    // since it's a regular file
    let stat_result = stat(&filename);
    assert_stat_results(stat_result);

    let lstat_result = lstat(&linkname);
    assert_lstat_results(lstat_result);

    let fstat_result = fstat(link.as_raw_fd());
    assert_stat_results(fstat_result);
}

#[test]
fn test_fchmod() {
    let tempdir = tempfile::tempdir().unwrap();
    let filename = tempdir.path().join("foo.txt");
    let file = File::create(&filename).unwrap();

    let mut mode1 = Mode::empty();
    mode1.insert(Mode::S_IRUSR);
    mode1.insert(Mode::S_IWUSR);
    fchmod(file.as_raw_fd(), mode1).unwrap();

    let file_stat1 = stat(&filename).unwrap();
    assert_eq!(file_stat1.st_mode & 0o7777, mode1.bits());

    let mut mode2 = Mode::empty();
    mode2.insert(Mode::S_IROTH);
    fchmod(file.as_raw_fd(), mode2).unwrap();

    let file_stat2 = stat(&filename).unwrap();
    assert_eq!(file_stat2.st_mode & 0o7777, mode2.bits());
}

#[test]
fn test_fchmodat() {
    let _dr = ::DirRestore::new();
    let tempdir = tempfile::tempdir().unwrap();
    let filename = "foo.txt";
    let fullpath = tempdir.path().join(filename);
    File::create(&fullpath).unwrap();

    let dirfd = fcntl::open(tempdir.path(), fcntl::OFlag::empty(), stat::Mode::empty()).unwrap();

    let mut mode1 = Mode::empty();
    mode1.insert(Mode::S_IRUSR);
    mode1.insert(Mode::S_IWUSR);
    fchmodat(Some(dirfd), filename, mode1, FchmodatFlags::FollowSymlink).unwrap();

    let file_stat1 = stat(&fullpath).unwrap();
    assert_eq!(file_stat1.st_mode & 0o7777, mode1.bits());

    chdir(tempdir.path()).unwrap();

    let mut mode2 = Mode::empty();
    mode2.insert(Mode::S_IROTH);
    fchmodat(None, filename, mode2, FchmodatFlags::FollowSymlink).unwrap();

    let file_stat2 = stat(&fullpath).unwrap();
    assert_eq!(file_stat2.st_mode & 0o7777, mode2.bits());
}

/// Asserts that the atime and mtime in a file's metadata match expected values.
///
/// The atime and mtime are expressed with a resolution of seconds because some file systems
/// (like macOS's HFS+) do not have higher granularity.
fn assert_times_eq(exp_atime_sec: u64, exp_mtime_sec: u64, attr: &fs::Metadata) {
    assert_eq!(
        Duration::new(exp_atime_sec, 0),
        attr.accessed().unwrap().duration_since(UNIX_EPOCH).unwrap());
    assert_eq!(
        Duration::new(exp_mtime_sec, 0),
        attr.modified().unwrap().duration_since(UNIX_EPOCH).unwrap());
}

#[test]
fn test_utimes() {
    let tempdir = tempfile::tempdir().unwrap();
    let fullpath = tempdir.path().join("file");
    drop(File::create(&fullpath).unwrap());

    utimes(&fullpath, &TimeVal::seconds(9990), &TimeVal::seconds(5550)).unwrap();
    assert_times_eq(9990, 5550, &fs::metadata(&fullpath).unwrap());
}

#[test]
#[cfg(any(target_os = "linux",
          target_os = "haiku",
          target_os = "ios",
          target_os = "macos",
          target_os = "freebsd",
          target_os = "netbsd"))]
fn test_lutimes() {
    let tempdir = tempfile::tempdir().unwrap();
    let target = tempdir.path().join("target");
    let fullpath = tempdir.path().join("symlink");
    drop(File::create(&target).unwrap());
    symlink(&target, &fullpath).unwrap();

    let exp_target_metadata = fs::symlink_metadata(&target).unwrap();
    lutimes(&fullpath, &TimeVal::seconds(4560), &TimeVal::seconds(1230)).unwrap();
    assert_times_eq(4560, 1230, &fs::symlink_metadata(&fullpath).unwrap());

    let target_metadata = fs::symlink_metadata(&target).unwrap();
    assert_eq!(exp_target_metadata.accessed().unwrap(), target_metadata.accessed().unwrap(),
               "atime of symlink target was unexpectedly modified");
    assert_eq!(exp_target_metadata.modified().unwrap(), target_metadata.modified().unwrap(),
               "mtime of symlink target was unexpectedly modified");
}

#[test]
fn test_futimens() {
    let tempdir = tempfile::tempdir().unwrap();
    let fullpath = tempdir.path().join("file");
    drop(File::create(&fullpath).unwrap());

    let fd = fcntl::open(&fullpath, fcntl::OFlag::empty(), stat::Mode::empty()).unwrap();

    futimens(fd, &TimeSpec::seconds(10), &TimeSpec::seconds(20)).unwrap();
    assert_times_eq(10, 20, &fs::metadata(&fullpath).unwrap());
}

#[test]
fn test_utimensat() {
    let _dr = ::DirRestore::new();
    let tempdir = tempfile::tempdir().unwrap();
    let filename = "foo.txt";
    let fullpath = tempdir.path().join(filename);
    drop(File::create(&fullpath).unwrap());

    let dirfd = fcntl::open(tempdir.path(), fcntl::OFlag::empty(), stat::Mode::empty()).unwrap();

    utimensat(Some(dirfd), filename, &TimeSpec::seconds(12345), &TimeSpec::seconds(678),
              UtimensatFlags::FollowSymlink).unwrap();
    assert_times_eq(12345, 678, &fs::metadata(&fullpath).unwrap());

    chdir(tempdir.path()).unwrap();

    utimensat(None, filename, &TimeSpec::seconds(500), &TimeSpec::seconds(800),
              UtimensatFlags::FollowSymlink).unwrap();
    assert_times_eq(500, 800, &fs::metadata(&fullpath).unwrap());
}

#[test]
fn test_mkdirat_success_path() {
    let tempdir = tempfile::tempdir().unwrap();
    let filename = "example_subdir";
    let dirfd = fcntl::open(tempdir.path(), fcntl::OFlag::empty(), stat::Mode::empty()).unwrap();
    assert!((mkdirat(dirfd, filename, Mode::S_IRWXU)).is_ok());
    assert!(Path::exists(&tempdir.path().join(filename)));
}

#[test]
fn test_mkdirat_success_mode() {
    let expected_bits = stat::SFlag::S_IFDIR.bits() | stat::Mode::S_IRWXU.bits();
    let tempdir = tempfile::tempdir().unwrap();
    let filename = "example_subdir";
    let dirfd = fcntl::open(tempdir.path(), fcntl::OFlag::empty(), stat::Mode::empty()).unwrap();
    assert!((mkdirat(dirfd, filename, Mode::S_IRWXU)).is_ok());
    let permissions = fs::metadata(tempdir.path().join(filename)).unwrap().permissions();
    let mode = permissions.mode();
    assert_eq!(mode as mode_t, expected_bits)
}

#[test]
fn test_mkdirat_fail() {
    let tempdir = tempfile::tempdir().unwrap();
    let not_dir_filename= "example_not_dir";
    let filename = "example_subdir_dir";
    let dirfd = fcntl::open(&tempdir.path().join(not_dir_filename), fcntl::OFlag::O_CREAT,
                            stat::Mode::empty()).unwrap();
    let result = mkdirat(dirfd, filename, Mode::S_IRWXU).unwrap_err();
    assert_eq!(result, Error::Sys(Errno::ENOTDIR));
}
