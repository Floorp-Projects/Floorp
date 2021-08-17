#[derive(Debug)]
/// Unix file descriptor.
pub struct Fd(i32);
impl From<i32> for Fd {
    fn from(fd: i32) -> Self {
        Self(fd)
    }
}
#[cfg(unix)]
impl std::os::unix::io::AsRawFd for Fd {
    fn as_raw_fd(&self) -> std::os::unix::io::RawFd {
        self.0
    }
}
impl std::ops::Deref for Fd {
    type Target = i32;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}
