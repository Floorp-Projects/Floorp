use {Result, NixPath};
use errno::Errno;
use std::os::unix::io::AsRawFd;
use libc;

pub fn statfs<P: ?Sized + NixPath>(path: &P, stat: &mut libc::statfs) -> Result<()> {
    unsafe {
        Errno::clear();
        let res = path.with_nix_path(|path| libc::statfs(path.as_ptr(), stat))?;

        Errno::result(res).map(drop)
    }
}

pub fn fstatfs<T: AsRawFd>(fd: &T, stat: &mut libc::statfs) -> Result<()> {
    unsafe {
        Errno::clear();
        Errno::result(libc::fstatfs(fd.as_raw_fd(), stat)).map(drop)
    }
}
