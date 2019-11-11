//! Socket interface functions
//!
//! [Further reading](http://man7.org/linux/man-pages/man7/socket.7.html)
use {Error, Result};
use errno::Errno;
use libc::{self, c_void, c_int, socklen_t, size_t};
use std::{fmt, mem, ptr, slice};
use std::os::unix::io::RawFd;
use sys::time::TimeVal;
use sys::uio::IoVec;

mod addr;
pub mod sockopt;

/*
 *
 * ===== Re-exports =====
 *
 */

pub use self::addr::{
    AddressFamily,
    SockAddr,
    InetAddr,
    UnixAddr,
    IpAddr,
    Ipv4Addr,
    Ipv6Addr,
    LinkAddr,
};
#[cfg(any(target_os = "android", target_os = "linux"))]
pub use ::sys::socket::addr::netlink::NetlinkAddr;

pub use libc::{
    cmsghdr,
    msghdr,
    sa_family_t,
    sockaddr,
    sockaddr_in,
    sockaddr_in6,
    sockaddr_storage,
    sockaddr_un,
};

/// These constants are used to specify the communication semantics
/// when creating a socket with [`socket()`](fn.socket.html)
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
#[repr(i32)]
pub enum SockType {
    /// Provides sequenced, reliable, two-way, connection-
    /// based byte streams.  An out-of-band data transmission
    /// mechanism may be supported.
    Stream = libc::SOCK_STREAM,
    /// Supports datagrams (connectionless, unreliable
    /// messages of a fixed maximum length).
    Datagram = libc::SOCK_DGRAM,
    /// Provides a sequenced, reliable, two-way connection-
    /// based data transmission path for datagrams of fixed
    /// maximum length; a consumer is required to read an
    /// entire packet with each input system call.
    SeqPacket = libc::SOCK_SEQPACKET,
    /// Provides raw network protocol access.
    Raw = libc::SOCK_RAW,
    /// Provides a reliable datagram layer that does not
    /// guarantee ordering.
    Rdm = libc::SOCK_RDM,
}

/// Constants used in [`socket`](fn.socket.html) and [`socketpair`](fn.socketpair.html)
/// to specify the protocol to use.
#[repr(i32)]
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum SockProtocol {
    /// TCP protocol ([ip(7)](http://man7.org/linux/man-pages/man7/ip.7.html))
    Tcp = libc::IPPROTO_TCP,
    /// UDP protocol ([ip(7)](http://man7.org/linux/man-pages/man7/ip.7.html))
    Udp = libc::IPPROTO_UDP,
    /// Allows applications and other KEXTs to be notified when certain kernel events occur
    /// ([ref](https://developer.apple.com/library/content/documentation/Darwin/Conceptual/NKEConceptual/control/control.html))
    #[cfg(any(target_os = "ios", target_os = "macos"))]
    KextEvent = libc::SYSPROTO_EVENT,
    /// Allows applications to configure and control a KEXT
    /// ([ref](https://developer.apple.com/library/content/documentation/Darwin/Conceptual/NKEConceptual/control/control.html))
    #[cfg(any(target_os = "ios", target_os = "macos"))]
    KextControl = libc::SYSPROTO_CONTROL,
}

libc_bitflags!{
    /// Additional socket options
    pub struct SockFlag: c_int {
        /// Set non-blocking mode on the new socket
        #[cfg(any(target_os = "android",
                  target_os = "dragonfly",
                  target_os = "freebsd",
                  target_os = "linux",
                  target_os = "netbsd",
                  target_os = "openbsd"))]
        SOCK_NONBLOCK;
        /// Set close-on-exec on the new descriptor
        #[cfg(any(target_os = "android",
                  target_os = "dragonfly",
                  target_os = "freebsd",
                  target_os = "linux",
                  target_os = "netbsd",
                  target_os = "openbsd"))]
        SOCK_CLOEXEC;
        /// Return `EPIPE` instead of raising `SIGPIPE`
        #[cfg(target_os = "netbsd")]
        SOCK_NOSIGPIPE;
        /// For domains `AF_INET(6)`, only allow `connect(2)`, `sendto(2)`, or `sendmsg(2)`
        /// to the DNS port (typically 53)
        #[cfg(target_os = "openbsd")]
        SOCK_DNS;
    }
}

libc_bitflags!{
    /// Flags for send/recv and their relatives
    pub struct MsgFlags: c_int {
        /// Sends or requests out-of-band data on sockets that support this notion
        /// (e.g., of type [`Stream`](enum.SockType.html)); the underlying protocol must also
        /// support out-of-band data.
        MSG_OOB;
        /// Peeks at an incoming message. The data is treated as unread and the next
        /// [`recv()`](fn.recv.html)
        /// or similar function shall still return this data.
        MSG_PEEK;
        /// Enables nonblocking operation; if the operation would block,
        /// `EAGAIN` or `EWOULDBLOCK` is returned.  This provides similar
        /// behavior to setting the `O_NONBLOCK` flag
        /// (via the [`fcntl`](../../fcntl/fn.fcntl.html)
        /// `F_SETFL` operation), but differs in that `MSG_DONTWAIT` is a per-
        /// call option, whereas `O_NONBLOCK` is a setting on the open file
        /// description (see [open(2)](http://man7.org/linux/man-pages/man2/open.2.html)),
        /// which will affect all threads in
        /// the calling process and as well as other processes that hold
        /// file descriptors referring to the same open file description.
        MSG_DONTWAIT;
        /// Receive flags: Control Data was discarded (buffer too small)
        MSG_CTRUNC;
        /// For raw ([`Packet`](addr/enum.AddressFamily.html)), Internet datagram
        /// (since Linux 2.4.27/2.6.8),
        /// netlink (since Linux 2.6.22) and UNIX datagram (since Linux 3.4)
        /// sockets: return the real length of the packet or datagram, even
        /// when it was longer than the passed buffer. Not implemented for UNIX
        /// domain ([unix(7)](https://linux.die.net/man/7/unix)) sockets.
        ///
        /// For use with Internet stream sockets, see [tcp(7)](https://linux.die.net/man/7/tcp).
        MSG_TRUNC;
        /// Terminates a record (when this notion is supported, as for
        /// sockets of type [`SeqPacket`](enum.SockType.html)).
        MSG_EOR;
        /// This flag specifies that queued errors should be received from
        /// the socket error queue. (For more details, see
        /// [recvfrom(2)](https://linux.die.net/man/2/recvfrom))
        #[cfg(any(target_os = "android", target_os = "linux"))]
        MSG_ERRQUEUE;
        /// Set the `close-on-exec` flag for the file descriptor received via a UNIX domain
        /// file descriptor using the `SCM_RIGHTS` operation (described in
        /// [unix(7)](https://linux.die.net/man/7/unix)).
        /// This flag is useful for the same reasons as the `O_CLOEXEC` flag of
        /// [open(2)](http://pubs.opengroup.org/onlinepubs/9699919799/functions/open.html).
        ///
        /// Only used in [`recvmsg`](fn.recvmsg.html) function.
        #[cfg(any(target_os = "android",
                  target_os = "dragonfly",
                  target_os = "freebsd",
                  target_os = "linux",
                  target_os = "netbsd",
                  target_os = "openbsd"))]
        MSG_CMSG_CLOEXEC;
    }
}

cfg_if! {
    if #[cfg(any(target_os = "android", target_os = "linux"))] {
        /// Unix credentials of the sending process.
        ///
        /// This struct is used with the `SO_PEERCRED` ancillary message for UNIX sockets.
        #[repr(C)]
        #[derive(Clone, Copy)]
        pub struct UnixCredentials(libc::ucred);

        impl UnixCredentials {
            /// Returns the process identifier
            pub fn pid(&self) -> libc::pid_t {
                self.0.pid
            }

            /// Returns the user identifier
            pub fn uid(&self) -> libc::uid_t {
                self.0.uid
            }

            /// Returns the group identifier
            pub fn gid(&self) -> libc::gid_t {
                self.0.gid
            }
        }

        impl PartialEq for UnixCredentials {
            fn eq(&self, other: &Self) -> bool {
                self.0.pid == other.0.pid && self.0.uid == other.0.uid && self.0.gid == other.0.gid
            }
        }
        impl Eq for UnixCredentials {}

        impl From<libc::ucred> for UnixCredentials {
            fn from(cred: libc::ucred) -> Self {
                UnixCredentials(cred)
            }
        }

        impl Into<libc::ucred> for UnixCredentials {
            fn into(self) -> libc::ucred {
                self.0
            }
        }

        impl fmt::Debug for UnixCredentials {
            fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
                f.debug_struct("UnixCredentials")
                    .field("pid", &self.0.pid)
                    .field("uid", &self.0.uid)
                    .field("gid", &self.0.gid)
                    .finish()
            }
        }
    }
}

/// Request for multicast socket operations
///
/// This is a wrapper type around `ip_mreq`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct IpMembershipRequest(libc::ip_mreq);

impl IpMembershipRequest {
    /// Instantiate a new `IpMembershipRequest`
    ///
    /// If `interface` is `None`, then `Ipv4Addr::any()` will be used for the interface.
    pub fn new(group: Ipv4Addr, interface: Option<Ipv4Addr>) -> Self {
        IpMembershipRequest(libc::ip_mreq {
            imr_multiaddr: group.0,
            imr_interface: interface.unwrap_or_else(Ipv4Addr::any).0,
        })
    }
}

impl PartialEq for IpMembershipRequest {
    fn eq(&self, other: &Self) -> bool {
        self.0.imr_multiaddr.s_addr == other.0.imr_multiaddr.s_addr
            && self.0.imr_interface.s_addr == other.0.imr_interface.s_addr
    }
}
impl Eq for IpMembershipRequest {}

impl fmt::Debug for IpMembershipRequest {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mref = &self.0.imr_multiaddr;
        let maddr = mref.s_addr;
        let iref = &self.0.imr_interface;
        let ifaddr = iref.s_addr;
        f.debug_struct("IpMembershipRequest")
            .field("imr_multiaddr", &maddr)
            .field("imr_interface", &ifaddr)
            .finish()
    }
}

/// Request for ipv6 multicast socket operations
///
/// This is a wrapper type around `ipv6_mreq`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Ipv6MembershipRequest(libc::ipv6_mreq);

impl Ipv6MembershipRequest {
    /// Instantiate a new `Ipv6MembershipRequest`
    pub fn new(group: Ipv6Addr) -> Self {
        Ipv6MembershipRequest(libc::ipv6_mreq {
            ipv6mr_multiaddr: group.0,
            ipv6mr_interface: 0,
        })
    }
}

impl PartialEq for Ipv6MembershipRequest {
    fn eq(&self, other: &Self) -> bool {
        self.0.ipv6mr_multiaddr.s6_addr == other.0.ipv6mr_multiaddr.s6_addr &&
            self.0.ipv6mr_interface == other.0.ipv6mr_interface
    }
}
impl Eq for Ipv6MembershipRequest {}

impl fmt::Debug for Ipv6MembershipRequest {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("Ipv6MembershipRequest")
            .field("ipv6mr_multiaddr", &self.0.ipv6mr_multiaddr.s6_addr)
            .field("ipv6mr_interface", &self.0.ipv6mr_interface)
            .finish()
    }
}

/// Copy the in-memory representation of `src` into the byte slice `dst`.
///
/// Returns the remainder of `dst`.
///
/// Panics when `dst` is too small for `src` (more precisely, panics if
/// `mem::size_of_val(src) >= dst.len()`).
///
/// Unsafe because it transmutes `src` to raw bytes, which is only safe for some
/// types `T`. Refer to the [Rustonomicon] for details.
///
/// [Rustonomicon]: https://doc.rust-lang.org/nomicon/transmutes.html
unsafe fn copy_bytes<'a, T: ?Sized>(src: &T, dst: &'a mut [u8]) -> &'a mut [u8] {
    let srclen = mem::size_of_val(src);
    ptr::copy_nonoverlapping(
        src as *const T as *const u8,
        dst[..srclen].as_mut_ptr(),
        srclen
    );

    &mut dst[srclen..]
}

/// Fills `dst` with `len` zero bytes and returns the remainder of the slice.
///
/// Panics when `len >= dst.len()`.
fn pad_bytes(len: usize, dst: &mut [u8]) -> &mut [u8] {
    for pad in &mut dst[..len] {
        *pad = 0;
    }

    &mut dst[len..]
}

cfg_if! {
    // Darwin and DragonFly BSD always align struct cmsghdr to 32-bit only.
    if #[cfg(any(target_os = "dragonfly", target_os = "ios", target_os = "macos"))] {
        type align_of_cmsg_data = u32;
    } else {
        type align_of_cmsg_data = size_t;
    }
}

/// A structure used to make room in a cmsghdr passed to recvmsg. The
/// size and alignment match that of a cmsghdr followed by a T, but the
/// fields are not accessible, as the actual types will change on a call
/// to recvmsg.
///
/// To make room for multiple messages, nest the type parameter with
/// tuples:
///
/// ```
/// use std::os::unix::io::RawFd;
/// use nix::sys::socket::CmsgSpace;
/// let cmsg: CmsgSpace<([RawFd; 3], CmsgSpace<[RawFd; 2]>)> = CmsgSpace::new();
/// ```
#[repr(C)]
#[allow(missing_debug_implementations)]
pub struct CmsgSpace<T> {
    _hdr: cmsghdr,
    _pad: [align_of_cmsg_data; 0],
    _data: T,
}

impl<T> CmsgSpace<T> {
    /// Create a CmsgSpace<T>. The structure is used only for space, so
    /// the fields are uninitialized.
    pub fn new() -> Self {
        // Safe because the fields themselves aren't accessible.
        unsafe { mem::uninitialized() }
    }
}

#[derive(Debug)]
pub struct RecvMsg<'a> {
    // The number of bytes received.
    pub bytes: usize,
    cmsg_buffer: &'a [u8],
    pub address: Option<SockAddr>,
    pub flags: MsgFlags,
}

impl<'a> RecvMsg<'a> {
    /// Iterate over the valid control messages pointed to by this
    /// msghdr.
    pub fn cmsgs(&self) -> CmsgIterator {
        CmsgIterator {
            buf: self.cmsg_buffer,
        }
    }
}

#[derive(Debug)]
pub struct CmsgIterator<'a> {
    /// Control message buffer to decode from. Must adhere to cmsg alignment.
    buf: &'a [u8],
}

impl<'a> Iterator for CmsgIterator<'a> {
    type Item = ControlMessage<'a>;

    // The implementation loosely follows CMSG_FIRSTHDR / CMSG_NXTHDR,
    // although we handle the invariants in slightly different places to
    // get a better iterator interface.
    fn next(&mut self) -> Option<ControlMessage<'a>> {
        if self.buf.len() == 0 {
            // The iterator assumes that `self.buf` always contains exactly the
            // bytes we need, so we're at the end when the buffer is empty.
            return None;
        }

        // Safe if: `self.buf` is `cmsghdr`-aligned.
        let cmsg: &'a cmsghdr = unsafe {
            &*(self.buf[..mem::size_of::<cmsghdr>()].as_ptr() as *const cmsghdr)
        };

        let cmsg_len = cmsg.cmsg_len as usize;

        // Advance our internal pointer.
        let cmsg_data = &self.buf[cmsg_align(mem::size_of::<cmsghdr>())..cmsg_len];
        self.buf = &self.buf[cmsg_align(cmsg_len)..];

        // Safe if: `cmsg_data` contains the expected (amount of) content. This
        // is verified by the kernel.
        unsafe {
            Some(ControlMessage::decode_from(cmsg, cmsg_data))
        }
    }
}

/// A type-safe wrapper around a single control message. More types may
/// be added to this enum; do not exhaustively pattern-match it.
/// [Further reading](http://man7.org/linux/man-pages/man3/cmsg.3.html)
#[allow(missing_debug_implementations)]
pub enum ControlMessage<'a> {
    /// A message of type `SCM_RIGHTS`, containing an array of file
    /// descriptors passed between processes.
    ///
    /// See the description in the "Ancillary messages" section of the
    /// [unix(7) man page](http://man7.org/linux/man-pages/man7/unix.7.html).
    ///
    /// Using multiple `ScmRights` messages for a single `sendmsg` call isn't recommended since it
    /// causes platform-dependent behaviour: It might swallow all but the first `ScmRights` message
    /// or fail with `EINVAL`. Instead, you can put all fds to be passed into a single `ScmRights`
    /// message.
    ScmRights(&'a [RawFd]),
    /// A message of type `SCM_CREDENTIALS`, containing the pid, uid and gid of
    /// a process connected to the socket.
    ///
    /// This is similar to the socket option `SO_PEERCRED`, but requires a
    /// process to explicitly send its credentials. A process running as root is
    /// allowed to specify any credentials, while credentials sent by other
    /// processes are verified by the kernel.
    ///
    /// For further information, please refer to the
    /// [`unix(7)`](http://man7.org/linux/man-pages/man7/unix.7.html) man page.
    // FIXME: When `#[repr(transparent)]` is stable, use it on `UnixCredentials`
    // and put that in here instead of a raw ucred.
    #[cfg(any(target_os = "android", target_os = "linux"))]
    ScmCredentials(&'a libc::ucred),
    /// A message of type `SCM_TIMESTAMP`, containing the time the
    /// packet was received by the kernel.
    ///
    /// See the kernel's explanation in "SO_TIMESTAMP" of
    /// [networking/timestamping](https://www.kernel.org/doc/Documentation/networking/timestamping.txt).
    ///
    /// # Examples
    ///
    // Disable this test on FreeBSD i386
    // https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=222039
    #[cfg_attr(not(all(target_os = "freebsd", target_arch = "x86")), doc = " ```")]
    #[cfg_attr(all(target_os = "freebsd", target_arch = "x86"), doc = " ```no_run")]
    /// use nix::sys::socket::*;
    /// use nix::sys::uio::IoVec;
    /// use nix::sys::time::*;
    /// use std::time::*;
    ///
    /// // Set up
    /// let message = "Ohayō!".as_bytes();
    /// let in_socket = socket(
    ///     AddressFamily::Inet,
    ///     SockType::Datagram,
    ///     SockFlag::empty(),
    ///     None).unwrap();
    /// setsockopt(in_socket, sockopt::ReceiveTimestamp, &true).unwrap();
    /// let localhost = InetAddr::new(IpAddr::new_v4(127, 0, 0, 1), 0);
    /// bind(in_socket, &SockAddr::new_inet(localhost)).unwrap();
    /// let address = getsockname(in_socket).unwrap();
    /// // Get initial time
    /// let time0 = SystemTime::now();
    /// // Send the message
    /// let iov = [IoVec::from_slice(message)];
    /// let flags = MsgFlags::empty();
    /// let l = sendmsg(in_socket, &iov, &[], flags, Some(&address)).unwrap();
    /// assert_eq!(message.len(), l);
    /// // Receive the message
    /// let mut buffer = vec![0u8; message.len()];
    /// let mut cmsgspace: CmsgSpace<TimeVal> = CmsgSpace::new();
    /// let iov = [IoVec::from_mut_slice(&mut buffer)];
    /// let r = recvmsg(in_socket, &iov, Some(&mut cmsgspace), flags).unwrap();
    /// let rtime = match r.cmsgs().next() {
    ///     Some(ControlMessage::ScmTimestamp(&rtime)) => rtime,
    ///     Some(_) => panic!("Unexpected control message"),
    ///     None => panic!("No control message")
    /// };
    /// // Check the final time
    /// let time1 = SystemTime::now();
    /// // the packet's received timestamp should lie in-between the two system
    /// // times, unless the system clock was adjusted in the meantime.
    /// let rduration = Duration::new(rtime.tv_sec() as u64,
    ///                               rtime.tv_usec() as u32 * 1000);
    /// assert!(time0.duration_since(UNIX_EPOCH).unwrap() <= rduration);
    /// assert!(rduration <= time1.duration_since(UNIX_EPOCH).unwrap());
    /// // Close socket
    /// nix::unistd::close(in_socket).unwrap();
    /// ```
    ScmTimestamp(&'a TimeVal),

    #[cfg(any(
        target_os = "android",
        target_os = "ios",
        target_os = "linux",
        target_os = "macos"
    ))]
    Ipv4PacketInfo(&'a libc::in_pktinfo),
    #[cfg(any(
        target_os = "android",
        target_os = "freebsd",
        target_os = "ios",
        target_os = "linux",
        target_os = "macos"
    ))]
    Ipv6PacketInfo(&'a libc::in6_pktinfo),

    /// Catch-all variant for unimplemented cmsg types.
    #[doc(hidden)]
    Unknown(UnknownCmsg<'a>),
}

// An opaque structure used to prevent cmsghdr from being a public type
#[doc(hidden)]
#[allow(missing_debug_implementations)]
pub struct UnknownCmsg<'a>(&'a cmsghdr, &'a [u8]);

// Round `len` up to meet the platform's required alignment for
// `cmsghdr`s and trailing `cmsghdr` data.  This should match the
// behaviour of CMSG_ALIGN from the Linux headers and do the correct
// thing on other platforms that don't usually provide CMSG_ALIGN.
#[inline]
fn cmsg_align(len: usize) -> usize {
    let align_bytes = mem::size_of::<align_of_cmsg_data>() - 1;
    (len + align_bytes) & !align_bytes
}

impl<'a> ControlMessage<'a> {
    /// The value of CMSG_SPACE on this message.
    fn space(&self) -> usize {
        cmsg_align(self.len())
    }

    /// The value of CMSG_LEN on this message.
    fn len(&self) -> usize {
        cmsg_align(mem::size_of::<cmsghdr>()) + match *self {
            ControlMessage::ScmRights(fds) => {
                mem::size_of_val(fds)
            },
            #[cfg(any(target_os = "android", target_os = "linux"))]
            ControlMessage::ScmCredentials(creds) => {
                mem::size_of_val(creds)
            }
            ControlMessage::ScmTimestamp(t) => {
                mem::size_of_val(t)
            },
            #[cfg(any(
                target_os = "android",
                target_os = "ios",
                target_os = "linux",
                target_os = "macos"
            ))]
            ControlMessage::Ipv4PacketInfo(pktinfo) => {
                mem::size_of_val(pktinfo)
            },
            #[cfg(any(
                target_os = "android",
                target_os = "freebsd",
                target_os = "ios",
                target_os = "linux",
                target_os = "macos"
            ))]
            ControlMessage::Ipv6PacketInfo(pktinfo) => {
                mem::size_of_val(pktinfo)
            },
            ControlMessage::Unknown(UnknownCmsg(_, bytes)) => {
                mem::size_of_val(bytes)
            }
        }
    }

    /// Returns the value to put into the `cmsg_level` field of the header.
    fn cmsg_level(&self) -> libc::c_int {
        match *self {
            ControlMessage::ScmRights(_) => libc::SOL_SOCKET,
            #[cfg(any(target_os = "android", target_os = "linux"))]
            ControlMessage::ScmCredentials(_) => libc::SOL_SOCKET,
            ControlMessage::ScmTimestamp(_) => libc::SOL_SOCKET,
            #[cfg(any(
                target_os = "android",
                target_os = "ios",
                target_os = "linux",
                target_os = "macos"
            ))]
            ControlMessage::Ipv4PacketInfo(_) => libc::IPPROTO_IP,
            #[cfg(any(
                target_os = "android",
                target_os = "freebsd",
                target_os = "ios",
                target_os = "linux",
                target_os = "macos"
            ))]
            ControlMessage::Ipv6PacketInfo(_) => libc::IPPROTO_IPV6,
            ControlMessage::Unknown(ref cmsg) => cmsg.0.cmsg_level,
        }
    }

    /// Returns the value to put into the `cmsg_type` field of the header.
    fn cmsg_type(&self) -> libc::c_int {
        match *self {
            ControlMessage::ScmRights(_) => libc::SCM_RIGHTS,
            #[cfg(any(target_os = "android", target_os = "linux"))]
            ControlMessage::ScmCredentials(_) => libc::SCM_CREDENTIALS,
            ControlMessage::ScmTimestamp(_) => libc::SCM_TIMESTAMP,
            #[cfg(any(
                target_os = "android",
                target_os = "ios",
                target_os = "linux",
                target_os = "macos"
            ))]
            ControlMessage::Ipv4PacketInfo(_) => libc::IP_PKTINFO,
            #[cfg(any(
                target_os = "android",
                target_os = "freebsd",
                target_os = "ios",
                target_os = "linux",
                target_os = "macos"
            ))]
            ControlMessage::Ipv6PacketInfo(_) => libc::IPV6_PKTINFO,
            ControlMessage::Unknown(ref cmsg) => cmsg.0.cmsg_type,
        }
    }

    // Unsafe: start and end of buffer must be cmsg_align'd. Updates
    // the provided slice; panics if the buffer is too small.
    unsafe fn encode_into(&self, buf: &mut [u8]) {
        let final_buf = if let ControlMessage::Unknown(ref cmsg) = *self {
            let &UnknownCmsg(orig_cmsg, bytes) = cmsg;

            let buf = copy_bytes(orig_cmsg, buf);

            let padlen = cmsg_align(mem::size_of_val(&orig_cmsg)) -
                mem::size_of_val(&orig_cmsg);
            let buf = pad_bytes(padlen, buf);

            copy_bytes(bytes, buf)
        } else {
            let cmsg = cmsghdr {
                cmsg_len: self.len() as _,
                cmsg_level: self.cmsg_level(),
                cmsg_type: self.cmsg_type(),
                ..mem::zeroed() // zero out platform-dependent padding fields
            };
            let buf = copy_bytes(&cmsg, buf);

            let padlen = cmsg_align(mem::size_of_val(&cmsg)) -
                mem::size_of_val(&cmsg);
            let buf = pad_bytes(padlen, buf);

            match *self {
                ControlMessage::ScmRights(fds) => {
                    copy_bytes(fds, buf)
                },
                #[cfg(any(target_os = "android", target_os = "linux"))]
                ControlMessage::ScmCredentials(creds) => {
                    copy_bytes(creds, buf)
                },
                ControlMessage::ScmTimestamp(t) => {
                    copy_bytes(t, buf)
                },
                #[cfg(any(
                    target_os = "android",
                    target_os = "ios",
                    target_os = "linux",
                    target_os = "macos"
                ))]
                ControlMessage::Ipv4PacketInfo(pktinfo) => {
                    copy_bytes(pktinfo, buf)
                },
                #[cfg(any(
                    target_os = "android",
                    target_os = "freebsd",
                    target_os = "ios",
                    target_os = "linux",
                    target_os = "macos"
                ))]
                ControlMessage::Ipv6PacketInfo(pktinfo) => {
                    copy_bytes(pktinfo, buf)
                }
                ControlMessage::Unknown(_) => unreachable!(),
            }
        };

        let padlen = self.space() - self.len();
        pad_bytes(padlen, final_buf);
    }

    /// Decodes a `ControlMessage` from raw bytes.
    ///
    /// This is only safe to call if the data is correct for the message type
    /// specified in the header. Normally, the kernel ensures that this is the
    /// case. "Correct" in this case includes correct length, alignment and
    /// actual content.
    unsafe fn decode_from(header: &'a cmsghdr, data: &'a [u8]) -> ControlMessage<'a> {
        match (header.cmsg_level, header.cmsg_type) {
            (libc::SOL_SOCKET, libc::SCM_RIGHTS) => {
                ControlMessage::ScmRights(
                    slice::from_raw_parts(data.as_ptr() as *const _,
                                          data.len() / mem::size_of::<RawFd>()))
            },
            #[cfg(any(target_os = "android", target_os = "linux"))]
            (libc::SOL_SOCKET, libc::SCM_CREDENTIALS) => {
                ControlMessage::ScmCredentials(
                    &*(data.as_ptr() as *const _)
                )
            }
            (libc::SOL_SOCKET, libc::SCM_TIMESTAMP) => {
                ControlMessage::ScmTimestamp(
                    &*(data.as_ptr() as *const _))
            },
            #[cfg(any(
                target_os = "android",
                target_os = "freebsd",
                target_os = "ios",
                target_os = "linux",
                target_os = "macos"
            ))]
            (libc::IPPROTO_IPV6, libc::IPV6_PKTINFO) => {
                ControlMessage::Ipv6PacketInfo(
                    &*(data.as_ptr() as *const _))
            }
            #[cfg(any(
                target_os = "android",
                target_os = "ios",
                target_os = "linux",
                target_os = "macos"
            ))]
            (libc::IPPROTO_IP, libc::IP_PKTINFO) => {
                ControlMessage::Ipv4PacketInfo(
                    &*(data.as_ptr() as *const _))
            }

            (_, _) => {
                ControlMessage::Unknown(UnknownCmsg(header, data))
            }
        }
    }
}


/// Send data in scatter-gather vectors to a socket, possibly accompanied
/// by ancillary data. Optionally direct the message at the given address,
/// as with sendto.
///
/// Allocates if cmsgs is nonempty.
pub fn sendmsg<'a>(fd: RawFd, iov: &[IoVec<&'a [u8]>], cmsgs: &[ControlMessage<'a>], flags: MsgFlags, addr: Option<&'a SockAddr>) -> Result<usize> {
    let mut capacity = 0;
    for cmsg in cmsgs {
        capacity += cmsg.space();
    }
    // Note that the resulting vector claims to have length == capacity,
    // so it's presently uninitialized.
    let mut cmsg_buffer = unsafe {
        let mut vec = Vec::<u8>::with_capacity(capacity);
        vec.set_len(capacity);
        vec
    };
    {
        let mut ofs = 0;
        for cmsg in cmsgs {
            let ptr = &mut cmsg_buffer[ofs..];
            unsafe {
                cmsg.encode_into(ptr);
            }
            ofs += cmsg.space();
        }
    }

    let (name, namelen) = match addr {
        Some(addr) => { let (x, y) = unsafe { addr.as_ffi_pair() }; (x as *const _, y) }
        None => (ptr::null(), 0),
    };

    let cmsg_ptr = if capacity > 0 {
        cmsg_buffer.as_ptr() as *const c_void
    } else {
        ptr::null()
    };

    let mhdr = unsafe {
        let mut mhdr: msghdr = mem::uninitialized();
        mhdr.msg_name =  name as *mut _;
        mhdr.msg_namelen =  namelen;
        mhdr.msg_iov =  iov.as_ptr() as *mut _;
        mhdr.msg_iovlen =  iov.len() as _;
        mhdr.msg_control =  cmsg_ptr as *mut _;
        mhdr.msg_controllen =  capacity as _;
        mhdr.msg_flags =  0;
        mhdr
    };
    let ret = unsafe { libc::sendmsg(fd, &mhdr, flags.bits()) };

    Errno::result(ret).map(|r| r as usize)
}

/// Receive message in scatter-gather vectors from a socket, and
/// optionally receive ancillary data into the provided buffer.
/// If no ancillary data is desired, use () as the type parameter.
pub fn recvmsg<'a, T>(fd: RawFd, iov: &[IoVec<&mut [u8]>], cmsg_buffer: Option<&'a mut CmsgSpace<T>>, flags: MsgFlags) -> Result<RecvMsg<'a>> {
    let mut address: sockaddr_storage = unsafe { mem::uninitialized() };
    let (msg_control, msg_controllen) = match cmsg_buffer {
        Some(cmsg_buffer) => (cmsg_buffer as *mut _, mem::size_of_val(cmsg_buffer)),
        None => (ptr::null_mut(), 0),
    };
    let mut mhdr = unsafe {
        let mut mhdr: msghdr = mem::uninitialized();
        mhdr.msg_name =  &mut address as *mut _ as *mut _;
        mhdr.msg_namelen =  mem::size_of::<sockaddr_storage>() as socklen_t;
        mhdr.msg_iov =  iov.as_ptr() as *mut _;
        mhdr.msg_iovlen =  iov.len() as _;
        mhdr.msg_control =  msg_control as *mut _;
        mhdr.msg_controllen =  msg_controllen as _;
        mhdr.msg_flags =  0;
        mhdr
    };
    let ret = unsafe { libc::recvmsg(fd, &mut mhdr, flags.bits()) };

    let cmsg_buffer = if msg_controllen > 0 {
        // got control message(s)
        debug_assert!(!mhdr.msg_control.is_null());
        unsafe {
            // Safe: The pointer is not null and the length is correct as part of `recvmsg`s
            // contract.
            slice::from_raw_parts(mhdr.msg_control as *const u8,
                                  mhdr.msg_controllen as usize)
        }
    } else {
        // No control message, create an empty buffer to avoid creating a slice from a null pointer
        &[]
    };

    Ok(unsafe { RecvMsg {
        bytes: Errno::result(ret)? as usize,
        cmsg_buffer,
        address: sockaddr_storage_to_addr(&address,
                                          mhdr.msg_namelen as usize).ok(),
        flags: MsgFlags::from_bits_truncate(mhdr.msg_flags),
    } })
}


/// Create an endpoint for communication
///
/// The `protocol` specifies a particular protocol to be used with the
/// socket.  Normally only a single protocol exists to support a
/// particular socket type within a given protocol family, in which case
/// protocol can be specified as `None`.  However, it is possible that many
/// protocols may exist, in which case a particular protocol must be
/// specified in this manner.
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/socket.html)
pub fn socket<T: Into<Option<SockProtocol>>>(domain: AddressFamily, ty: SockType, flags: SockFlag, protocol: T) -> Result<RawFd> {
    let protocol = match protocol.into() {
        None => 0,
        Some(p) => p as c_int,
    };

    // SockFlags are usually embedded into `ty`, but we don't do that in `nix` because it's a
    // little easier to understand by separating it out. So we have to merge these bitfields
    // here.
    let mut ty = ty as c_int;
    ty |= flags.bits();

    let res = unsafe { libc::socket(domain as c_int, ty, protocol) };

    Errno::result(res)
}

/// Create a pair of connected sockets
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/socketpair.html)
pub fn socketpair<T: Into<Option<SockProtocol>>>(domain: AddressFamily, ty: SockType, protocol: T,
                  flags: SockFlag) -> Result<(RawFd, RawFd)> {
    let protocol = match protocol.into() {
        None => 0,
        Some(p) => p as c_int,
    };

    // SockFlags are usually embedded into `ty`, but we don't do that in `nix` because it's a
    // little easier to understand by separating it out. So we have to merge these bitfields
    // here.
    let mut ty = ty as c_int;
    ty |= flags.bits();

    let mut fds = [-1, -1];

    let res = unsafe { libc::socketpair(domain as c_int, ty, protocol, fds.as_mut_ptr()) };
    Errno::result(res)?;

    Ok((fds[0], fds[1]))
}

/// Listen for connections on a socket
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/listen.html)
pub fn listen(sockfd: RawFd, backlog: usize) -> Result<()> {
    let res = unsafe { libc::listen(sockfd, backlog as c_int) };

    Errno::result(res).map(drop)
}

/// Bind a name to a socket
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/bind.html)
pub fn bind(fd: RawFd, addr: &SockAddr) -> Result<()> {
    let res = unsafe {
        let (ptr, len) = addr.as_ffi_pair();
        libc::bind(fd, ptr, len)
    };

    Errno::result(res).map(drop)
}

/// Accept a connection on a socket
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/accept.html)
pub fn accept(sockfd: RawFd) -> Result<RawFd> {
    let res = unsafe { libc::accept(sockfd, ptr::null_mut(), ptr::null_mut()) };

    Errno::result(res)
}

/// Accept a connection on a socket
///
/// [Further reading](http://man7.org/linux/man-pages/man2/accept.2.html)
#[cfg(any(target_os = "android",
          target_os = "freebsd",
          target_os = "linux",
          target_os = "openbsd"))]
pub fn accept4(sockfd: RawFd, flags: SockFlag) -> Result<RawFd> {
    let res = unsafe { libc::accept4(sockfd, ptr::null_mut(), ptr::null_mut(), flags.bits()) };

    Errno::result(res)
}

/// Initiate a connection on a socket
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/connect.html)
pub fn connect(fd: RawFd, addr: &SockAddr) -> Result<()> {
    let res = unsafe {
        let (ptr, len) = addr.as_ffi_pair();
        libc::connect(fd, ptr, len)
    };

    Errno::result(res).map(drop)
}

/// Receive data from a connection-oriented socket. Returns the number of
/// bytes read
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/recv.html)
pub fn recv(sockfd: RawFd, buf: &mut [u8], flags: MsgFlags) -> Result<usize> {
    unsafe {
        let ret = libc::recv(
            sockfd,
            buf.as_ptr() as *mut c_void,
            buf.len() as size_t,
            flags.bits());

        Errno::result(ret).map(|r| r as usize)
    }
}

/// Receive data from a connectionless or connection-oriented socket. Returns
/// the number of bytes read and the socket address of the sender.
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/recvfrom.html)
pub fn recvfrom(sockfd: RawFd, buf: &mut [u8]) -> Result<(usize, SockAddr)> {
    unsafe {
        let addr: sockaddr_storage = mem::zeroed();
        let mut len = mem::size_of::<sockaddr_storage>() as socklen_t;

        let ret = Errno::result(libc::recvfrom(
            sockfd,
            buf.as_ptr() as *mut c_void,
            buf.len() as size_t,
            0,
            mem::transmute(&addr),
            &mut len as *mut socklen_t))?;

        sockaddr_storage_to_addr(&addr, len as usize)
            .map(|addr| (ret as usize, addr))
    }
}

/// Send a message to a socket
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/sendto.html)
pub fn sendto(fd: RawFd, buf: &[u8], addr: &SockAddr, flags: MsgFlags) -> Result<usize> {
    let ret = unsafe {
        let (ptr, len) = addr.as_ffi_pair();
        libc::sendto(fd, buf.as_ptr() as *const c_void, buf.len() as size_t, flags.bits(), ptr, len)
    };

    Errno::result(ret).map(|r| r as usize)
}

/// Send data to a connection-oriented socket. Returns the number of bytes read
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/send.html)
pub fn send(fd: RawFd, buf: &[u8], flags: MsgFlags) -> Result<usize> {
    let ret = unsafe {
        libc::send(fd, buf.as_ptr() as *const c_void, buf.len() as size_t, flags.bits())
    };

    Errno::result(ret).map(|r| r as usize)
}

/*
 *
 * ===== Socket Options =====
 *
 */

/// The protocol level at which to get / set socket options. Used as an
/// argument to `getsockopt` and `setsockopt`.
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/setsockopt.html)
#[repr(i32)]
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum SockLevel {
    Socket = libc::SOL_SOCKET,
    Tcp = libc::IPPROTO_TCP,
    Ip = libc::IPPROTO_IP,
    Ipv6 = libc::IPPROTO_IPV6,
    Udp = libc::IPPROTO_UDP,
    #[cfg(any(target_os = "android", target_os = "linux"))]
    Netlink = libc::SOL_NETLINK,
}

/// Represents a socket option that can be accessed or set. Used as an argument
/// to `getsockopt`
pub trait GetSockOpt : Copy {
    type Val;

    #[doc(hidden)]
    fn get(&self, fd: RawFd) -> Result<Self::Val>;
}

/// Represents a socket option that can be accessed or set. Used as an argument
/// to `setsockopt`
pub trait SetSockOpt : Copy {
    type Val;

    #[doc(hidden)]
    fn set(&self, fd: RawFd, val: &Self::Val) -> Result<()>;
}

/// Get the current value for the requested socket option
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/getsockopt.html)
pub fn getsockopt<O: GetSockOpt>(fd: RawFd, opt: O) -> Result<O::Val> {
    opt.get(fd)
}

/// Sets the value for the requested socket option
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/setsockopt.html)
///
/// # Examples
///
/// ```
/// use nix::sys::socket::setsockopt;
/// use nix::sys::socket::sockopt::KeepAlive;
/// use std::net::TcpListener;
/// use std::os::unix::io::AsRawFd;
///
/// let listener = TcpListener::bind("0.0.0.0:0").unwrap();
/// let fd = listener.as_raw_fd();
/// let res = setsockopt(fd, KeepAlive, &true);
/// assert!(res.is_ok());
/// ```
pub fn setsockopt<O: SetSockOpt>(fd: RawFd, opt: O, val: &O::Val) -> Result<()> {
    opt.set(fd, val)
}

/// Get the address of the peer connected to the socket `fd`.
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/getpeername.html)
pub fn getpeername(fd: RawFd) -> Result<SockAddr> {
    unsafe {
        let addr: sockaddr_storage = mem::uninitialized();
        let mut len = mem::size_of::<sockaddr_storage>() as socklen_t;

        let ret = libc::getpeername(fd, mem::transmute(&addr), &mut len);

        Errno::result(ret)?;

        sockaddr_storage_to_addr(&addr, len as usize)
    }
}

/// Get the current address to which the socket `fd` is bound.
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/getsockname.html)
pub fn getsockname(fd: RawFd) -> Result<SockAddr> {
    unsafe {
        let addr: sockaddr_storage = mem::uninitialized();
        let mut len = mem::size_of::<sockaddr_storage>() as socklen_t;

        let ret = libc::getsockname(fd, mem::transmute(&addr), &mut len);

        Errno::result(ret)?;

        sockaddr_storage_to_addr(&addr, len as usize)
    }
}

/// Return the appropriate `SockAddr` type from a `sockaddr_storage` of a certain
/// size.  In C this would usually be done by casting.  The `len` argument
/// should be the number of bytes in the `sockaddr_storage` that are actually
/// allocated and valid.  It must be at least as large as all the useful parts
/// of the structure.  Note that in the case of a `sockaddr_un`, `len` need not
/// include the terminating null.
pub unsafe fn sockaddr_storage_to_addr(
    addr: &sockaddr_storage,
    len: usize) -> Result<SockAddr> {

    if len < mem::size_of_val(&addr.ss_family) {
        return Err(Error::Sys(Errno::ENOTCONN));
    }

    match addr.ss_family as c_int {
        libc::AF_INET => {
            assert!(len as usize == mem::size_of::<sockaddr_in>());
            let ret = *(addr as *const _ as *const sockaddr_in);
            Ok(SockAddr::Inet(InetAddr::V4(ret)))
        }
        libc::AF_INET6 => {
            assert!(len as usize == mem::size_of::<sockaddr_in6>());
            Ok(SockAddr::Inet(InetAddr::V6(*(addr as *const _ as *const sockaddr_in6))))
        }
        libc::AF_UNIX => {
            let sun = *(addr as *const _ as *const sockaddr_un);
            let pathlen = len - offset_of!(sockaddr_un, sun_path);
            Ok(SockAddr::Unix(UnixAddr(sun, pathlen)))
        }
        #[cfg(any(target_os = "android", target_os = "linux"))]
        libc::AF_NETLINK => {
            use libc::sockaddr_nl;
            Ok(SockAddr::Netlink(NetlinkAddr(*(addr as *const _ as *const sockaddr_nl))))
        }
        af => panic!("unexpected address family {}", af),
    }
}


#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum Shutdown {
    /// Further receptions will be disallowed.
    Read,
    /// Further  transmissions will be disallowed.
    Write,
    /// Further receptions and transmissions will be disallowed.
    Both,
}

/// Shut down part of a full-duplex connection.
///
/// [Further reading](http://pubs.opengroup.org/onlinepubs/9699919799/functions/shutdown.html)
pub fn shutdown(df: RawFd, how: Shutdown) -> Result<()> {
    unsafe {
        use libc::shutdown;

        let how = match how {
            Shutdown::Read  => libc::SHUT_RD,
            Shutdown::Write => libc::SHUT_WR,
            Shutdown::Both  => libc::SHUT_RDWR,
        };

        Errno::result(shutdown(df, how)).map(drop)
    }
}
