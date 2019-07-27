use libc::{uid_t, gid_t};

/// Credentials of a process
#[derive(Copy, Clone, Eq, PartialEq, Hash, Debug)]
pub struct UCred {
    /// UID (user ID) of the process
    pub uid: uid_t,
    /// GID (group ID) of the process
    pub gid: gid_t,
}

#[cfg(target_os = "linux")]
pub use self::impl_linux::get_peer_cred;

#[cfg(any(target_os = "macos", target_os = "ios", target_os = "freebsd"))]
pub use self::impl_macos::get_peer_cred;

#[cfg(target_os = "linux")]
pub mod impl_linux {
    use libc::{getsockopt, SOL_SOCKET, SO_PEERCRED, c_void};
    use std::{io, mem};
    use UnixStream;
    use std::os::unix::io::AsRawFd;

    use libc::ucred;

    pub fn get_peer_cred(sock: &UnixStream) -> io::Result<super::UCred> {
        unsafe {
            let raw_fd = sock.as_raw_fd();

            let mut ucred = ucred { pid: 0, uid: 0, gid: 0 };

            let ucred_size = mem::size_of::<ucred>();

            // These paranoid checks should be optimized-out
            assert!(mem::size_of::<u32>() <= mem::size_of::<usize>());
            assert!(ucred_size <= u32::max_value() as usize);

            let mut ucred_size = ucred_size as u32;
            
            let ret = getsockopt(raw_fd, SOL_SOCKET, SO_PEERCRED, &mut ucred as *mut ucred as *mut c_void, &mut ucred_size);
            if ret == 0 && ucred_size as usize == mem::size_of::<ucred>() {
                Ok(super::UCred {
                    uid: ucred.uid,
                    gid: ucred.gid,
                })
            } else {
                Err(io::Error::last_os_error())
            }
        }
    }
}

#[cfg(any(target_os = "macos", target_os = "ios", target_os = "freebsd"))]
pub mod impl_macos {
    use libc::getpeereid;
    use std::{io, mem};
    use UnixStream;
    use std::os::unix::io::AsRawFd;

    pub fn get_peer_cred(sock: &UnixStream) -> io::Result<super::UCred> {
        unsafe {
            let raw_fd = sock.as_raw_fd();

            let mut cred: super::UCred = mem::uninitialized();

            let ret = getpeereid(raw_fd, &mut cred.uid, &mut cred.gid);

            if ret == 0 {
                Ok(cred)
            } else {
                Err(io::Error::last_os_error())
            }
        }
    }
}

#[cfg(test)]
mod test {
    use tokio_core::reactor::Core;
    use UnixStream;
    use libc::geteuid;
    use libc::getegid;

    #[test]
    fn test_socket_pair() {
        let core = Core::new().unwrap();
        let handle = core.handle();

        let (a, b) = UnixStream::pair(&handle).unwrap();
        let cred_a = a.peer_cred().unwrap();
        let cred_b = b.peer_cred().unwrap();
        assert_eq!(cred_a, cred_b);

        let uid = unsafe { geteuid() };
        let gid = unsafe { getegid() };

        assert_eq!(cred_a.uid, uid);
        assert_eq!(cred_a.gid, gid);
    }
}
