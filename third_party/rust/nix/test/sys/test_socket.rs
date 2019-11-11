use nix::sys::socket::{InetAddr, UnixAddr, getsockname};
use std::slice;
use std::net::{self, Ipv6Addr, SocketAddr, SocketAddrV6};
use std::path::Path;
use std::str::FromStr;
use std::os::unix::io::RawFd;
use libc::c_char;
use tempfile;

#[test]
pub fn test_inetv4_addr_to_sock_addr() {
    let actual: net::SocketAddr = FromStr::from_str("127.0.0.1:3000").unwrap();
    let addr = InetAddr::from_std(&actual);

    match addr {
        InetAddr::V4(addr) => {
            let ip: u32 = 0x7f00_0001;
            let port: u16 = 3000;
            let saddr = addr.sin_addr.s_addr;

            assert_eq!(saddr, ip.to_be());
            assert_eq!(addr.sin_port, port.to_be());
        }
        _ => panic!("nope"),
    }

    assert_eq!(addr.to_str(), "127.0.0.1:3000");

    let inet = addr.to_std();
    assert_eq!(actual, inet);
}

#[test]
pub fn test_inetv6_addr_to_sock_addr() {
    let port: u16 = 3000;
    let flowinfo: u32 = 1;
    let scope_id: u32 = 2;
    let ip: Ipv6Addr = "fe80::1".parse().unwrap();

    let actual = SocketAddr::V6(SocketAddrV6::new(ip, port, flowinfo, scope_id));
    let addr = InetAddr::from_std(&actual);

    match addr {
        InetAddr::V6(addr) => {
            assert_eq!(addr.sin6_port, port.to_be());
            assert_eq!(addr.sin6_flowinfo, flowinfo);
            assert_eq!(addr.sin6_scope_id, scope_id);
        }
        _ => panic!("nope"),
    }

    assert_eq!(actual, addr.to_std());
}

#[test]
pub fn test_path_to_sock_addr() {
    let path = "/foo/bar";
    let actual = Path::new(path);
    let addr = UnixAddr::new(actual).unwrap();

    let expect: &[c_char] = unsafe {
        slice::from_raw_parts(path.as_bytes().as_ptr() as *const c_char, path.len())
    };
    assert_eq!(&addr.0.sun_path[..8], expect);

    assert_eq!(addr.path(), Some(actual));
}

// Test getting/setting abstract addresses (without unix socket creation)
#[cfg(target_os = "linux")]
#[test]
pub fn test_abstract_uds_addr() {
    let empty = String::new();
    let addr = UnixAddr::new_abstract(empty.as_bytes()).unwrap();
    assert_eq!(addr.as_abstract(), Some(empty.as_bytes()));

    let name = String::from("nix\0abstract\0test");
    let addr = UnixAddr::new_abstract(name.as_bytes()).unwrap();
    assert_eq!(addr.as_abstract(), Some(name.as_bytes()));
    assert_eq!(addr.path(), None);

    // Internally, name is null-prefixed (abstract namespace)
    let internal: &[u8] = unsafe {
        slice::from_raw_parts(addr.0.sun_path.as_ptr() as *const u8, addr.1)
    };
    let mut abstract_name = name.clone();
    abstract_name.insert(0, '\0');
    assert_eq!(internal, abstract_name.as_bytes());
}

#[test]
pub fn test_getsockname() {
    use nix::sys::socket::{socket, AddressFamily, SockType, SockFlag};
    use nix::sys::socket::{bind, SockAddr};

    let tempdir = tempfile::tempdir().unwrap();
    let sockname = tempdir.path().join("sock");
    let sock = socket(AddressFamily::Unix, SockType::Stream, SockFlag::empty(), None)
               .expect("socket failed");
    let sockaddr = SockAddr::new_unix(&sockname).unwrap();
    bind(sock, &sockaddr).expect("bind failed");
    assert_eq!(sockaddr.to_str(),
               getsockname(sock).expect("getsockname failed").to_str());
}

#[test]
pub fn test_socketpair() {
    use nix::unistd::{read, write};
    use nix::sys::socket::{socketpair, AddressFamily, SockType, SockFlag};

    let (fd1, fd2) = socketpair(AddressFamily::Unix, SockType::Stream, None, SockFlag::empty())
                     .unwrap();
    write(fd1, b"hello").unwrap();
    let mut buf = [0;5];
    read(fd2, &mut buf).unwrap();

    assert_eq!(&buf[..], b"hello");
}

#[test]
pub fn test_scm_rights() {
    use nix::sys::uio::IoVec;
    use nix::unistd::{pipe, read, write, close};
    use nix::sys::socket::{socketpair, sendmsg, recvmsg,
                           AddressFamily, SockType, SockFlag,
                           ControlMessage, CmsgSpace, MsgFlags};

    let (fd1, fd2) = socketpair(AddressFamily::Unix, SockType::Stream, None, SockFlag::empty())
                     .unwrap();
    let (r, w) = pipe().unwrap();
    let mut received_r: Option<RawFd> = None;

    {
        let iov = [IoVec::from_slice(b"hello")];
        let fds = [r];
        let cmsg = ControlMessage::ScmRights(&fds);
        assert_eq!(sendmsg(fd1, &iov, &[cmsg], MsgFlags::empty(), None).unwrap(), 5);
        close(r).unwrap();
        close(fd1).unwrap();
    }

    {
        let mut buf = [0u8; 5];
        let iov = [IoVec::from_mut_slice(&mut buf[..])];
        let mut cmsgspace: CmsgSpace<[RawFd; 1]> = CmsgSpace::new();
        let msg = recvmsg(fd2, &iov, Some(&mut cmsgspace), MsgFlags::empty()).unwrap();

        for cmsg in msg.cmsgs() {
            if let ControlMessage::ScmRights(fd) = cmsg {
                assert_eq!(received_r, None);
                assert_eq!(fd.len(), 1);
                received_r = Some(fd[0]);
            } else {
                panic!("unexpected cmsg");
            }
        }
        assert!(!msg.flags.intersects(MsgFlags::MSG_TRUNC | MsgFlags::MSG_CTRUNC));
        close(fd2).unwrap();
    }

    let received_r = received_r.expect("Did not receive passed fd");
    // Ensure that the received file descriptor works
    write(w, b"world").unwrap();
    let mut buf = [0u8; 5];
    read(received_r, &mut buf).unwrap();
    assert_eq!(&buf[..], b"world");
    close(received_r).unwrap();
    close(w).unwrap();
}

/// Tests that passing multiple fds using a single `ControlMessage` works.
#[test]
fn test_scm_rights_single_cmsg_multiple_fds() {
    use std::os::unix::net::UnixDatagram;
    use std::os::unix::io::{RawFd, AsRawFd};
    use std::thread;
    use nix::sys::socket::{CmsgSpace, ControlMessage, MsgFlags, sendmsg, recvmsg};
    use nix::sys::uio::IoVec;
    use libc;

    let (send, receive) = UnixDatagram::pair().unwrap();
    let thread = thread::spawn(move || {
        let mut buf = [0u8; 8];
        let iovec = [IoVec::from_mut_slice(&mut buf)];
        let mut space = CmsgSpace::<[RawFd; 2]>::new();
        let msg = recvmsg(
            receive.as_raw_fd(),
            &iovec,
            Some(&mut space),
            MsgFlags::empty()
        ).unwrap();
        assert!(!msg.flags.intersects(MsgFlags::MSG_TRUNC | MsgFlags::MSG_CTRUNC));

        let mut cmsgs = msg.cmsgs();
        match cmsgs.next() {
            Some(ControlMessage::ScmRights(fds)) => {
                assert_eq!(fds.len(), 2,
                           "unexpected fd count (expected 2 fds, got {})",
                           fds.len());
            },
            _ => panic!(),
        }
        assert!(cmsgs.next().is_none(), "unexpected control msg");

        assert_eq!(iovec[0].as_slice(), [1u8, 2, 3, 4, 5, 6, 7, 8]);
    });

    let slice = [1u8, 2, 3, 4, 5, 6, 7, 8];
    let iov = [IoVec::from_slice(&slice)];
    let fds = [libc::STDIN_FILENO, libc::STDOUT_FILENO];    // pass stdin and stdout
    let cmsg = [ControlMessage::ScmRights(&fds)];
    sendmsg(send.as_raw_fd(), &iov, &cmsg, MsgFlags::empty(), None).unwrap();
    thread.join().unwrap();
}

// Verify `sendmsg` builds a valid `msghdr` when passing an empty
// `cmsgs` argument.  This should result in a msghdr with a nullptr
// msg_control field and a msg_controllen of 0 when calling into the
// raw `sendmsg`.
#[test]
pub fn test_sendmsg_empty_cmsgs() {
    use nix::sys::uio::IoVec;
    use nix::unistd::close;
    use nix::sys::socket::{socketpair, sendmsg, recvmsg,
                           AddressFamily, SockType, SockFlag,
                           CmsgSpace, MsgFlags};

    let (fd1, fd2) = socketpair(AddressFamily::Unix, SockType::Stream, None, SockFlag::empty())
                     .unwrap();

    {
        let iov = [IoVec::from_slice(b"hello")];
        assert_eq!(sendmsg(fd1, &iov, &[], MsgFlags::empty(), None).unwrap(), 5);
        close(fd1).unwrap();
    }

    {
        let mut buf = [0u8; 5];
        let iov = [IoVec::from_mut_slice(&mut buf[..])];
        let mut cmsgspace: CmsgSpace<[RawFd; 1]> = CmsgSpace::new();
        let msg = recvmsg(fd2, &iov, Some(&mut cmsgspace), MsgFlags::empty()).unwrap();

        for _ in msg.cmsgs() {
            panic!("unexpected cmsg");
        }
        assert!(!msg.flags.intersects(MsgFlags::MSG_TRUNC | MsgFlags::MSG_CTRUNC));
        close(fd2).unwrap();
    }
}

#[cfg(any(target_os = "android", target_os = "linux"))]
#[test]
fn test_scm_credentials() {
    use libc;
    use nix::sys::uio::IoVec;
    use nix::unistd::{close, getpid, getuid, getgid};
    use nix::sys::socket::{socketpair, sendmsg, recvmsg, setsockopt,
                           AddressFamily, SockType, SockFlag,
                           ControlMessage, CmsgSpace, MsgFlags};
    use nix::sys::socket::sockopt::PassCred;

    let (send, recv) = socketpair(AddressFamily::Unix, SockType::Stream, None, SockFlag::empty())
        .unwrap();
    setsockopt(recv, PassCred, &true).unwrap();

    {
        let iov = [IoVec::from_slice(b"hello")];
        let cred = libc::ucred {
            pid: getpid().as_raw(),
            uid: getuid().as_raw(),
            gid: getgid().as_raw(),
        };
        let cmsg = ControlMessage::ScmCredentials(&cred);
        assert_eq!(sendmsg(send, &iov, &[cmsg], MsgFlags::empty(), None).unwrap(), 5);
        close(send).unwrap();
    }

    {
        let mut buf = [0u8; 5];
        let iov = [IoVec::from_mut_slice(&mut buf[..])];
        let mut cmsgspace: CmsgSpace<libc::ucred> = CmsgSpace::new();
        let msg = recvmsg(recv, &iov, Some(&mut cmsgspace), MsgFlags::empty()).unwrap();
        let mut received_cred = None;

        for cmsg in msg.cmsgs() {
            if let ControlMessage::ScmCredentials(cred) = cmsg {
                assert!(received_cred.is_none());
                assert_eq!(cred.pid, getpid().as_raw());
                assert_eq!(cred.uid, getuid().as_raw());
                assert_eq!(cred.gid, getgid().as_raw());
                received_cred = Some(*cred);
            } else {
                panic!("unexpected cmsg");
            }
        }
        received_cred.expect("no creds received");
        assert!(!msg.flags.intersects(MsgFlags::MSG_TRUNC | MsgFlags::MSG_CTRUNC));
        close(recv).unwrap();
    }
}

/// Ensure that we can send `SCM_CREDENTIALS` and `SCM_RIGHTS` with a single
/// `sendmsg` call.
#[cfg(any(target_os = "android", target_os = "linux"))]
// qemu's handling of multiple cmsgs is bugged, ignore tests on non-x86
// see https://bugs.launchpad.net/qemu/+bug/1781280
#[cfg_attr(not(any(target_arch = "x86_64", target_arch = "x86")), ignore)]
#[test]
fn test_scm_credentials_and_rights() {
    use nix::sys::socket::CmsgSpace;
    use libc;

    test_impl_scm_credentials_and_rights(CmsgSpace::<(libc::ucred, CmsgSpace<RawFd>)>::new());
}

/// Ensure that passing a `CmsgSpace` with too much space for the received
/// messages still works.
#[cfg(any(target_os = "android", target_os = "linux"))]
// qemu's handling of multiple cmsgs is bugged, ignore tests on non-x86
// see https://bugs.launchpad.net/qemu/+bug/1781280
#[cfg_attr(not(any(target_arch = "x86_64", target_arch = "x86")), ignore)]
#[test]
fn test_too_large_cmsgspace() {
    use nix::sys::socket::CmsgSpace;

    test_impl_scm_credentials_and_rights(CmsgSpace::<[u8; 1024]>::new());
}

#[cfg(any(target_os = "android", target_os = "linux"))]
fn test_impl_scm_credentials_and_rights<T>(mut space: ::nix::sys::socket::CmsgSpace<T>) {
    use libc;
    use nix::sys::uio::IoVec;
    use nix::unistd::{pipe, read, write, close, getpid, getuid, getgid};
    use nix::sys::socket::{socketpair, sendmsg, recvmsg, setsockopt,
                           AddressFamily, SockType, SockFlag,
                           ControlMessage, MsgFlags};
    use nix::sys::socket::sockopt::PassCred;

    let (send, recv) = socketpair(AddressFamily::Unix, SockType::Stream, None, SockFlag::empty())
        .unwrap();
    setsockopt(recv, PassCred, &true).unwrap();

    let (r, w) = pipe().unwrap();
    let mut received_r: Option<RawFd> = None;

    {
        let iov = [IoVec::from_slice(b"hello")];
        let cred = libc::ucred {
            pid: getpid().as_raw(),
            uid: getuid().as_raw(),
            gid: getgid().as_raw(),
        };
        let fds = [r];
        let cmsgs = [
            ControlMessage::ScmCredentials(&cred),
            ControlMessage::ScmRights(&fds),
        ];
        assert_eq!(sendmsg(send, &iov, &cmsgs, MsgFlags::empty(), None).unwrap(), 5);
        close(r).unwrap();
        close(send).unwrap();
    }

    {
        let mut buf = [0u8; 5];
        let iov = [IoVec::from_mut_slice(&mut buf[..])];
        let msg = recvmsg(recv, &iov, Some(&mut space), MsgFlags::empty()).unwrap();
        let mut received_cred = None;

        assert_eq!(msg.cmsgs().count(), 2, "expected 2 cmsgs");

        for cmsg in msg.cmsgs() {
            match cmsg {
                ControlMessage::ScmRights(fds) => {
                    assert_eq!(received_r, None, "already received fd");
                    assert_eq!(fds.len(), 1);
                    received_r = Some(fds[0]);
                }
                ControlMessage::ScmCredentials(cred) => {
                    assert!(received_cred.is_none());
                    assert_eq!(cred.pid, getpid().as_raw());
                    assert_eq!(cred.uid, getuid().as_raw());
                    assert_eq!(cred.gid, getgid().as_raw());
                    received_cred = Some(*cred);
                }
                _ => panic!("unexpected cmsg"),
            }
        }
        received_cred.expect("no creds received");
        assert!(!msg.flags.intersects(MsgFlags::MSG_TRUNC | MsgFlags::MSG_CTRUNC));
        close(recv).unwrap();
    }

    let received_r = received_r.expect("Did not receive passed fd");
    // Ensure that the received file descriptor works
    write(w, b"world").unwrap();
    let mut buf = [0u8; 5];
    read(received_r, &mut buf).unwrap();
    assert_eq!(&buf[..], b"world");
    close(received_r).unwrap();
    close(w).unwrap();
}

// Test creating and using named unix domain sockets
#[test]
pub fn test_unixdomain() {
    use nix::sys::socket::{AddressFamily, SockType, SockFlag};
    use nix::sys::socket::{bind, socket, connect, listen, accept, SockAddr};
    use nix::unistd::{read, write, close};
    use std::thread;

    let tempdir = tempfile::tempdir().unwrap();
    let sockname = tempdir.path().join("sock");
    let s1 = socket(AddressFamily::Unix, SockType::Stream,
                    SockFlag::empty(), None).expect("socket failed");
    let sockaddr = SockAddr::new_unix(&sockname).unwrap();
    bind(s1, &sockaddr).expect("bind failed");
    listen(s1, 10).expect("listen failed");

    let thr = thread::spawn(move || {
        let s2 = socket(AddressFamily::Unix, SockType::Stream, SockFlag::empty(), None)
                 .expect("socket failed");
        connect(s2, &sockaddr).expect("connect failed");
        write(s2, b"hello").expect("write failed");
        close(s2).unwrap();
    });

    let s3 = accept(s1).expect("accept failed");

    let mut buf = [0;5];
    read(s3, &mut buf).unwrap();
    close(s3).unwrap();
    close(s1).unwrap();
    thr.join().unwrap();

    assert_eq!(&buf[..], b"hello");
}

// Test creating and using named system control sockets
#[cfg(any(target_os = "macos", target_os = "ios"))]
#[test]
pub fn test_syscontrol() {
    use nix::Error;
    use nix::errno::Errno;
    use nix::sys::socket::{AddressFamily, socket, SockAddr, SockType, SockFlag, SockProtocol};

    let fd = socket(AddressFamily::System, SockType::Datagram,
                    SockFlag::empty(), SockProtocol::KextControl)
             .expect("socket failed");
    let _sockaddr = SockAddr::new_sys_control(fd, "com.apple.net.utun_control", 0).expect("resolving sys_control name failed");
    assert_eq!(SockAddr::new_sys_control(fd, "foo.bar.lol", 0).err(), Some(Error::Sys(Errno::ENOENT)));

    // requires root privileges
    // connect(fd, &sockaddr).expect("connect failed");
}

#[cfg(any(
    target_os = "android",
    target_os = "ios",
    target_os = "linux",
    target_os = "macos"
))]
// qemu doesn't seem to be emulating this correctly in these architectures
#[cfg_attr(any(
    target_arch = "mips",
    target_arch = "mips64",
    target_arch = "powerpc64",
), ignore)]
#[test]
pub fn test_recv_ipv4pktinfo() {
    use libc;
    use nix::ifaddrs::{getifaddrs, InterfaceAddress};
    use nix::net::if_::*;
    use nix::sys::socket::sockopt::Ipv4PacketInfo;
    use nix::sys::socket::{bind, AddressFamily, SockFlag, SockType};
    use nix::sys::socket::{getsockname, setsockopt, socket, SockAddr};
    use nix::sys::socket::{recvmsg, sendmsg, CmsgSpace, ControlMessage, MsgFlags};
    use nix::sys::uio::IoVec;
    use std::io;
    use std::io::Write;
    use std::thread;

    fn loopback_v4addr() -> Option<InterfaceAddress> {
        let addrs = match getifaddrs() {
            Ok(iter) => iter,
            Err(e) => {
                let stdioerr = io::stderr();
                let mut handle = stdioerr.lock();
                writeln!(handle, "getifaddrs: {:?}", e).unwrap();
                return None;
            },
        };
        for ifaddr in addrs {
            if ifaddr.flags.contains(InterfaceFlags::IFF_LOOPBACK) {
                match ifaddr.address {
                    Some(SockAddr::Inet(InetAddr::V4(..))) => {
                        return Some(ifaddr);
                    }
                    _ => continue,
                }
            }
        }
        None
    }

    let lo_ifaddr = loopback_v4addr();
    let (lo_name, lo) = match lo_ifaddr {
        Some(ifaddr) => (ifaddr.interface_name,
                         ifaddr.address.expect("Expect IPv4 address on interface")),
        None => return,
    };
    let receive = socket(
        AddressFamily::Inet,
        SockType::Datagram,
        SockFlag::empty(),
        None,
    ).expect("receive socket failed");
    bind(receive, &lo).expect("bind failed");
    let sa = getsockname(receive).expect("getsockname failed");
    setsockopt(receive, Ipv4PacketInfo, &true).expect("setsockopt failed");

    let thread = thread::spawn(move || {
        let mut buf = [0u8; 8];
        let iovec = [IoVec::from_mut_slice(&mut buf)];
        let mut space = CmsgSpace::<libc::in_pktinfo>::new();
        let msg = recvmsg(
            receive,
            &iovec,
            Some(&mut space),
            MsgFlags::empty(),
        ).expect("recvmsg failed");
        assert!(
            !msg.flags
                .intersects(MsgFlags::MSG_TRUNC | MsgFlags::MSG_CTRUNC)
        );

        let mut cmsgs = msg.cmsgs();
        match cmsgs.next() {
            Some(ControlMessage::Ipv4PacketInfo(pktinfo)) => {
                let i = if_nametoindex(lo_name.as_bytes()).expect("if_nametoindex");
                assert_eq!(
                    pktinfo.ipi_ifindex as libc::c_uint,
                    i,
                    "unexpected ifindex (expected {}, got {})",
                    i,
                    pktinfo.ipi_ifindex
                );
            }
            _ => (),
        }
        assert!(cmsgs.next().is_none(), "unexpected additional control msg");
        assert_eq!(
            iovec[0].as_slice(),
            [1u8, 2, 3, 4, 5, 6, 7, 8]
        );
    });

    let slice = [1u8, 2, 3, 4, 5, 6, 7, 8];
    let iov = [IoVec::from_slice(&slice)];

    let send = socket(
        AddressFamily::Inet,
        SockType::Datagram,
        SockFlag::empty(),
        None,
    ).expect("send socket failed");
    sendmsg(send, &iov, &[], MsgFlags::empty(), Some(&sa)).expect("sendmsg failed");

    thread.join().unwrap();
}

#[cfg(any(
    target_os = "android",
    target_os = "freebsd",
    target_os = "ios",
    target_os = "linux",
    target_os = "macos"
))]
// qemu doesn't seem to be emulating this correctly in these architectures
#[cfg_attr(any(
    target_arch = "mips",
    target_arch = "mips64",
    target_arch = "powerpc64",
), ignore)]
#[test]
pub fn test_recv_ipv6pktinfo() {
    use libc;
    use nix::ifaddrs::{getifaddrs, InterfaceAddress};
    use nix::net::if_::*;
    use nix::sys::socket::sockopt::Ipv6RecvPacketInfo;
    use nix::sys::socket::{bind, AddressFamily, SockFlag, SockType};
    use nix::sys::socket::{getsockname, setsockopt, socket, SockAddr};
    use nix::sys::socket::{recvmsg, sendmsg, CmsgSpace, ControlMessage, MsgFlags};
    use nix::sys::uio::IoVec;
    use std::io;
    use std::io::Write;
    use std::thread;

    fn loopback_v6addr() -> Option<InterfaceAddress> {
        let addrs = match getifaddrs() {
            Ok(iter) => iter,
            Err(e) => {
                let stdioerr = io::stderr();
                let mut handle = stdioerr.lock();
                writeln!(handle, "getifaddrs: {:?}", e).unwrap();
                return None;
            },
        };
        for ifaddr in addrs {
            if ifaddr.flags.contains(InterfaceFlags::IFF_LOOPBACK) {
                match ifaddr.address {
                    Some(SockAddr::Inet(InetAddr::V6(..))) => {
                        return Some(ifaddr);
                    }
                    _ => continue,
                }
            }
        }
        None
    }

    let lo_ifaddr = loopback_v6addr();
    let (lo_name, lo) = match lo_ifaddr {
        Some(ifaddr) => (ifaddr.interface_name,
                         ifaddr.address.expect("Expect IPv4 address on interface")),
        None => return,
    };
    let receive = socket(
        AddressFamily::Inet6,
        SockType::Datagram,
        SockFlag::empty(),
        None,
    ).expect("receive socket failed");
    bind(receive, &lo).expect("bind failed");
    let sa = getsockname(receive).expect("getsockname failed");
    setsockopt(receive, Ipv6RecvPacketInfo, &true).expect("setsockopt failed");

    let thread = thread::spawn(move || {
        let mut buf = [0u8; 8];
        let iovec = [IoVec::from_mut_slice(&mut buf)];
        let mut space = CmsgSpace::<libc::in6_pktinfo>::new();
        let msg = recvmsg(
            receive,
            &iovec,
            Some(&mut space),
            MsgFlags::empty(),
        ).expect("recvmsg failed");
        assert!(
            !msg.flags
                .intersects(MsgFlags::MSG_TRUNC | MsgFlags::MSG_CTRUNC)
        );

        let mut cmsgs = msg.cmsgs();
        match cmsgs.next() {
            Some(ControlMessage::Ipv6PacketInfo(pktinfo)) => {
                let i = if_nametoindex(lo_name.as_bytes()).expect("if_nametoindex");
                assert_eq!(
                    pktinfo.ipi6_ifindex,
                    i,
                    "unexpected ifindex (expected {}, got {})",
                    i,
                    pktinfo.ipi6_ifindex
                );
            }
            _ => (),
        }
        assert!(cmsgs.next().is_none(), "unexpected additional control msg");
        assert_eq!(
            iovec[0].as_slice(),
            [1u8, 2, 3, 4, 5, 6, 7, 8]
        );
    });

    let slice = [1u8, 2, 3, 4, 5, 6, 7, 8];
    let iov = [IoVec::from_slice(&slice)];

    let send = socket(
        AddressFamily::Inet6,
        SockType::Datagram,
        SockFlag::empty(),
        None,
    ).expect("send socket failed");
    sendmsg(send, &iov, &[], MsgFlags::empty(), Some(&sa)).expect("sendmsg failed");

    thread.join().unwrap();
}
