#[cfg(not(any(target_os = "macos", target_os = "ios")))]
use std::ptr;
use std::{
    io::{self, IoSliceMut},
    mem::{self, MaybeUninit},
    net::{IpAddr, Ipv4Addr, Ipv6Addr, SocketAddr, SocketAddrV4, SocketAddrV6},
    os::unix::io::AsRawFd,
    sync::{
        atomic::{AtomicBool, AtomicUsize, Ordering},
        Mutex,
    },
    time::Instant,
};

use socket2::SockRef;

use super::{
    cmsg, log_sendmsg_error, EcnCodepoint, RecvMeta, Transmit, UdpSockRef, IO_ERROR_LOG_INTERVAL,
};

#[cfg(target_os = "freebsd")]
type IpTosTy = libc::c_uchar;
#[cfg(not(target_os = "freebsd"))]
type IpTosTy = libc::c_int;

/// Tokio-compatible UDP socket with some useful specializations.
///
/// Unlike a standard tokio UDP socket, this allows ECN bits to be read and written on some
/// platforms.
#[derive(Debug)]
pub struct UdpSocketState {
    last_send_error: Mutex<Instant>,
    max_gso_segments: AtomicUsize,
    gro_segments: usize,
    may_fragment: bool,

    /// True if we have received EINVAL error from `sendmsg` or `sendmmsg` system call at least once.
    ///
    /// If enabled, we assume that old kernel is used and switch to fallback mode.
    /// In particular, we do not use IP_TOS cmsg_type in this case,
    /// which is not supported on Linux <3.13 and results in not sending the UDP packet at all.
    sendmsg_einval: AtomicBool,
}

impl UdpSocketState {
    pub fn new(sock: UdpSockRef<'_>) -> io::Result<Self> {
        let io = sock.0;
        let mut cmsg_platform_space = 0;
        if cfg!(target_os = "linux")
            || cfg!(target_os = "freebsd")
            || cfg!(target_os = "macos")
            || cfg!(target_os = "ios")
            || cfg!(target_os = "android")
        {
            cmsg_platform_space +=
                unsafe { libc::CMSG_SPACE(mem::size_of::<libc::in6_pktinfo>() as _) as usize };
        }

        assert!(
            CMSG_LEN
                >= unsafe { libc::CMSG_SPACE(mem::size_of::<libc::c_int>() as _) as usize }
                    + cmsg_platform_space
        );
        assert!(
            mem::align_of::<libc::cmsghdr>() <= mem::align_of::<cmsg::Aligned<[u8; 0]>>(),
            "control message buffers will be misaligned"
        );

        io.set_nonblocking(true)?;

        let addr = io.local_addr()?;
        let is_ipv4 = addr.family() == libc::AF_INET as libc::sa_family_t;

        // mac and ios do not support IP_RECVTOS on dual-stack sockets :(
        // older macos versions also don't have the flag and will error out if we don't ignore it
        if is_ipv4 || !io.only_v6()? {
            if let Err(err) = set_socket_option(&*io, libc::IPPROTO_IP, libc::IP_RECVTOS, OPTION_ON)
            {
                tracing::debug!("Ignoring error setting IP_RECVTOS on socket: {err:?}",);
            }
        }

        let mut may_fragment = false;
        #[cfg(any(target_os = "linux", target_os = "android"))]
        {
            // opportunistically try to enable GRO. See gro::gro_segments().
            #[cfg(target_os = "linux")]
            let _ = set_socket_option(&*io, libc::SOL_UDP, libc::UDP_GRO, OPTION_ON);

            // Forbid IPv4 fragmentation. Set even for IPv6 to account for IPv6 mapped IPv4 addresses.
            // Set `may_fragment` to `true` if this option is not supported on the platform.
            may_fragment |= !set_socket_option_supported(
                &*io,
                libc::IPPROTO_IP,
                libc::IP_MTU_DISCOVER,
                libc::IP_PMTUDISC_PROBE,
            )?;

            if is_ipv4 {
                set_socket_option(&*io, libc::IPPROTO_IP, libc::IP_PKTINFO, OPTION_ON)?;
            } else {
                // Set `may_fragment` to `true` if this option is not supported on the platform.
                may_fragment |= !set_socket_option_supported(
                    &*io,
                    libc::IPPROTO_IPV6,
                    libc::IPV6_MTU_DISCOVER,
                    libc::IP_PMTUDISC_PROBE,
                )?;
            }
        }
        #[cfg(any(target_os = "freebsd", target_os = "macos", target_os = "ios"))]
        {
            if is_ipv4 {
                // Set `may_fragment` to `true` if this option is not supported on the platform.
                may_fragment |= !set_socket_option_supported(
                    &*io,
                    libc::IPPROTO_IP,
                    libc::IP_DONTFRAG,
                    OPTION_ON,
                )?;
            }
        }
        #[cfg(any(target_os = "freebsd", target_os = "macos", target_os = "ios"))]
        // IP_RECVDSTADDR == IP_SENDSRCADDR on FreeBSD
        // macOS uses only IP_RECVDSTADDR, no IP_SENDSRCADDR on macOS
        // macOS also supports IP_PKTINFO
        {
            if is_ipv4 {
                set_socket_option(&*io, libc::IPPROTO_IP, libc::IP_RECVDSTADDR, OPTION_ON)?;
            }
        }

        // Options standardized in RFC 3542
        if !is_ipv4 {
            set_socket_option(&*io, libc::IPPROTO_IPV6, libc::IPV6_RECVPKTINFO, OPTION_ON)?;
            set_socket_option(&*io, libc::IPPROTO_IPV6, libc::IPV6_RECVTCLASS, OPTION_ON)?;
            // Linux's IP_PMTUDISC_PROBE allows us to operate under interface MTU rather than the
            // kernel's path MTU guess, but actually disabling fragmentation requires this too. See
            // __ip6_append_data in ip6_output.c.
            // Set `may_fragment` to `true` if this option is not supported on the platform.
            may_fragment |= !set_socket_option_supported(
                &*io,
                libc::IPPROTO_IPV6,
                libc::IPV6_DONTFRAG,
                OPTION_ON,
            )?;
        }

        let now = Instant::now();
        Ok(Self {
            last_send_error: Mutex::new(now.checked_sub(2 * IO_ERROR_LOG_INTERVAL).unwrap_or(now)),
            max_gso_segments: AtomicUsize::new(gso::max_gso_segments()),
            gro_segments: gro::gro_segments(),
            may_fragment,
            sendmsg_einval: AtomicBool::new(false),
        })
    }

    pub fn send(&self, socket: UdpSockRef<'_>, transmit: &Transmit<'_>) -> io::Result<()> {
        send(self, socket.0, transmit)
    }

    pub fn recv(
        &self,
        socket: UdpSockRef<'_>,
        bufs: &mut [IoSliceMut<'_>],
        meta: &mut [RecvMeta],
    ) -> io::Result<usize> {
        recv(socket.0, bufs, meta)
    }

    /// The maximum amount of segments which can be transmitted if a platform
    /// supports Generic Send Offload (GSO).
    ///
    /// This is 1 if the platform doesn't support GSO. Subject to change if errors are detected
    /// while using GSO.
    #[inline]
    pub fn max_gso_segments(&self) -> usize {
        self.max_gso_segments.load(Ordering::Relaxed)
    }

    /// The number of segments to read when GRO is enabled. Used as a factor to
    /// compute the receive buffer size.
    ///
    /// Returns 1 if the platform doesn't support GRO.
    #[inline]
    pub fn gro_segments(&self) -> usize {
        self.gro_segments
    }

    /// Whether transmitted datagrams might get fragmented by the IP layer
    ///
    /// Returns `false` on targets which employ e.g. the `IPV6_DONTFRAG` socket option.
    #[inline]
    pub fn may_fragment(&self) -> bool {
        self.may_fragment
    }

    /// Returns true if we previously got an EINVAL error from `sendmsg` or `sendmmsg` syscall.
    fn sendmsg_einval(&self) -> bool {
        self.sendmsg_einval.load(Ordering::Relaxed)
    }

    /// Sets the flag indicating we got EINVAL error from `sendmsg` or `sendmmsg` syscall.
    #[cfg(not(any(target_os = "macos", target_os = "ios")))]
    fn set_sendmsg_einval(&self) {
        self.sendmsg_einval.store(true, Ordering::Relaxed)
    }
}

#[cfg(not(any(target_os = "macos", target_os = "ios")))]
fn send(
    #[allow(unused_variables)] // only used on Linux
    state: &UdpSocketState,
    io: SockRef<'_>,
    transmit: &Transmit<'_>,
) -> io::Result<()> {
    #[allow(unused_mut)] // only mutable on FreeBSD
    let mut encode_src_ip = true;
    #[cfg(target_os = "freebsd")]
    {
        let addr = io.local_addr()?;
        let is_ipv4 = addr.family() == libc::AF_INET as libc::sa_family_t;
        if is_ipv4 {
            if let Some(socket) = addr.as_socket_ipv4() {
                encode_src_ip = socket.ip() == &Ipv4Addr::UNSPECIFIED;
            }
        }
    }
    let mut msg_hdr: libc::msghdr = unsafe { mem::zeroed() };
    let mut iovec: libc::iovec = unsafe { mem::zeroed() };
    let mut cmsgs = cmsg::Aligned([0u8; CMSG_LEN]);
    let dst_addr = socket2::SockAddr::from(transmit.destination);
    prepare_msg(
        transmit,
        &dst_addr,
        &mut msg_hdr,
        &mut iovec,
        &mut cmsgs,
        encode_src_ip,
        state.sendmsg_einval(),
    );

    loop {
        let n = unsafe { libc::sendmsg(io.as_raw_fd(), &msg_hdr, 0) };
        if n == -1 {
            let e = io::Error::last_os_error();
            match e.kind() {
                io::ErrorKind::Interrupted => {
                    // Retry the transmission
                    continue;
                }
                io::ErrorKind::WouldBlock => return Err(e),
                _ => {
                    // Some network adapters and drivers do not support GSO. Unfortunately, Linux
                    // offers no easy way for us to detect this short of an EIO or sometimes EINVAL
                    // when we try to actually send datagrams using it.
                    #[cfg(target_os = "linux")]
                    if let Some(libc::EIO) | Some(libc::EINVAL) = e.raw_os_error() {
                        // Prevent new transmits from being scheduled using GSO. Existing GSO transmits
                        // may already be in the pipeline, so we need to tolerate additional failures.
                        if state.max_gso_segments() > 1 {
                            tracing::error!("got transmit error, halting segmentation offload");
                            state
                                .max_gso_segments
                                .store(1, std::sync::atomic::Ordering::Relaxed);
                        }
                    }

                    if e.raw_os_error() == Some(libc::EINVAL) {
                        // Some arguments to `sendmsg` are not supported.
                        // Switch to fallback mode.
                        state.set_sendmsg_einval();
                    }

                    // Other errors are ignored, since they will usually be handled
                    // by higher level retransmits and timeouts.
                    // - PermissionDenied errors have been observed due to iptable rules.
                    //   Those are not fatal errors, since the
                    //   configuration can be dynamically changed.
                    // - Destination unreachable errors have been observed for other
                    // - EMSGSIZE is expected for MTU probes. Future work might be able to avoid
                    //   these by automatically clamping the MTUD upper bound to the interface MTU.
                    if e.raw_os_error() != Some(libc::EMSGSIZE) {
                        log_sendmsg_error(&state.last_send_error, e, transmit);
                    }

                    return Ok(());
                }
            }
        }
        return Ok(());
    }
}

#[cfg(any(target_os = "macos", target_os = "ios"))]
fn send(state: &UdpSocketState, io: SockRef<'_>, transmit: &Transmit<'_>) -> io::Result<()> {
    let mut hdr: libc::msghdr = unsafe { mem::zeroed() };
    let mut iov: libc::iovec = unsafe { mem::zeroed() };
    let mut ctrl = cmsg::Aligned([0u8; CMSG_LEN]);
    let addr = socket2::SockAddr::from(transmit.destination);
    prepare_msg(
        transmit,
        &addr,
        &mut hdr,
        &mut iov,
        &mut ctrl,
        // Only tested on macOS and iOS
        cfg!(target_os = "macos") || cfg!(target_os = "ios"),
        state.sendmsg_einval(),
    );
    let n = unsafe { libc::sendmsg(io.as_raw_fd(), &hdr, 0) };
    if n == -1 {
        let e = io::Error::last_os_error();
        match e.kind() {
            io::ErrorKind::Interrupted => {
                // Retry the transmission
            }
            io::ErrorKind::WouldBlock => return Err(e),
            _ => {
                // Other errors are ignored, since they will usually be handled
                // by higher level retransmits and timeouts.
                // - PermissionDenied errors have been observed due to iptable rules.
                //   Those are not fatal errors, since the
                //   configuration can be dynamically changed.
                // - Destination unreachable errors have been observed for other
                // - EMSGSIZE is expected for MTU probes. Future work might be able to avoid
                //   these by automatically clamping the MTUD upper bound to the interface MTU.
                if e.raw_os_error() != Some(libc::EMSGSIZE) {
                    log_sendmsg_error(&state.last_send_error, e, transmit);
                }
            }
        }
    }
    Ok(())
}

#[cfg(not(any(target_os = "macos", target_os = "ios")))]
fn recv(io: SockRef<'_>, bufs: &mut [IoSliceMut<'_>], meta: &mut [RecvMeta]) -> io::Result<usize> {
    let mut names = [MaybeUninit::<libc::sockaddr_storage>::uninit(); BATCH_SIZE];
    let mut ctrls = [cmsg::Aligned(MaybeUninit::<[u8; CMSG_LEN]>::uninit()); BATCH_SIZE];
    let mut hdrs = unsafe { mem::zeroed::<[libc::mmsghdr; BATCH_SIZE]>() };
    let max_msg_count = bufs.len().min(BATCH_SIZE);
    for i in 0..max_msg_count {
        prepare_recv(
            &mut bufs[i],
            &mut names[i],
            &mut ctrls[i],
            &mut hdrs[i].msg_hdr,
        );
    }
    let msg_count = loop {
        let n = unsafe {
            recvmmsg_with_fallback(
                io.as_raw_fd(),
                hdrs.as_mut_ptr(),
                bufs.len().min(BATCH_SIZE) as _,
            )
        };
        if n == -1 {
            let e = io::Error::last_os_error();
            if e.kind() == io::ErrorKind::Interrupted {
                continue;
            }
            return Err(e);
        }
        break n;
    };
    for i in 0..(msg_count as usize) {
        meta[i] = decode_recv(&names[i], &hdrs[i].msg_hdr, hdrs[i].msg_len as usize);
    }
    Ok(msg_count as usize)
}

#[cfg(any(target_os = "macos", target_os = "ios"))]
fn recv(io: SockRef<'_>, bufs: &mut [IoSliceMut<'_>], meta: &mut [RecvMeta]) -> io::Result<usize> {
    let mut name = MaybeUninit::<libc::sockaddr_storage>::uninit();
    let mut ctrl = cmsg::Aligned(MaybeUninit::<[u8; CMSG_LEN]>::uninit());
    let mut hdr = unsafe { mem::zeroed::<libc::msghdr>() };
    prepare_recv(&mut bufs[0], &mut name, &mut ctrl, &mut hdr);
    let n = loop {
        let n = unsafe { libc::recvmsg(io.as_raw_fd(), &mut hdr, 0) };
        if n == -1 {
            let e = io::Error::last_os_error();
            if e.kind() == io::ErrorKind::Interrupted {
                continue;
            }
            return Err(e);
        }
        if hdr.msg_flags & libc::MSG_TRUNC != 0 {
            continue;
        }
        break n;
    };
    meta[0] = decode_recv(&name, &hdr, n as usize);
    Ok(1)
}

/// Implementation of `recvmmsg` with a fallback
/// to `recvmsg` if syscall is not available.
///
/// It uses [`libc::syscall`] instead of [`libc::recvmmsg`]
/// to avoid linking error on systems where libc does not contain `recvmmsg`.
#[cfg(not(any(target_os = "macos", target_os = "ios")))]
unsafe fn recvmmsg_with_fallback(
    sockfd: libc::c_int,
    msgvec: *mut libc::mmsghdr,
    vlen: libc::c_uint,
) -> libc::c_int {
    let flags = 0;
    let timeout = ptr::null_mut::<libc::timespec>();

    #[cfg(not(target_os = "freebsd"))]
    {
        let ret =
            libc::syscall(libc::SYS_recvmmsg, sockfd, msgvec, vlen, flags, timeout) as libc::c_int;
        if ret != -1 {
            return ret;
        }
    }

    // libc on FreeBSD implements `recvmmsg` as a high-level abstraction over `recvmsg`,
    // thus `SYS_recvmmsg` constant and direct system call do not exist
    #[cfg(target_os = "freebsd")]
    {
        let ret = libc::recvmmsg(sockfd, msgvec, vlen as usize, flags, timeout) as libc::c_int;
        if ret != -1 {
            return ret;
        }
    }

    let e = io::Error::last_os_error();
    match e.raw_os_error() {
        Some(libc::ENOSYS) => {
            // Fallback to `recvmsg`.
            recvmmsg_fallback(sockfd, msgvec, vlen)
        }
        _ => -1,
    }
}

/// Fallback implementation of `recvmmsg` using `recvmsg`
/// for systems which do not support `recvmmsg`
/// such as Linux <2.6.33.
#[cfg(not(any(target_os = "macos", target_os = "ios")))]
unsafe fn recvmmsg_fallback(
    sockfd: libc::c_int,
    msgvec: *mut libc::mmsghdr,
    vlen: libc::c_uint,
) -> libc::c_int {
    let flags = 0;
    if vlen == 0 {
        return 0;
    }

    let n = libc::recvmsg(sockfd, &mut (*msgvec).msg_hdr, flags);
    if n == -1 {
        -1
    } else {
        // type of `msg_len` field differs on Linux and FreeBSD,
        // it is up to the compiler to infer and cast `n` to correct type
        (*msgvec).msg_len = n as _;
        1
    }
}

const CMSG_LEN: usize = 88;

fn prepare_msg(
    transmit: &Transmit<'_>,
    dst_addr: &socket2::SockAddr,
    hdr: &mut libc::msghdr,
    iov: &mut libc::iovec,
    ctrl: &mut cmsg::Aligned<[u8; CMSG_LEN]>,
    #[allow(unused_variables)] // only used on FreeBSD & macOS
    encode_src_ip: bool,
    sendmsg_einval: bool,
) {
    iov.iov_base = transmit.contents.as_ptr() as *const _ as *mut _;
    iov.iov_len = transmit.contents.len();

    // SAFETY: Casting the pointer to a mutable one is legal,
    // as sendmsg is guaranteed to not alter the mutable pointer
    // as per the POSIX spec. See the section on the sys/socket.h
    // header for details. The type is only mutable in the first
    // place because it is reused by recvmsg as well.
    let name = dst_addr.as_ptr() as *mut libc::c_void;
    let namelen = dst_addr.len();
    hdr.msg_name = name as *mut _;
    hdr.msg_namelen = namelen;
    hdr.msg_iov = iov;
    hdr.msg_iovlen = 1;

    hdr.msg_control = ctrl.0.as_mut_ptr() as _;
    hdr.msg_controllen = CMSG_LEN as _;
    let mut encoder = unsafe { cmsg::Encoder::new(hdr) };
    let ecn = transmit.ecn.map_or(0, |x| x as libc::c_int);
    // True for IPv4 or IPv4-Mapped IPv6
    let is_ipv4 = transmit.destination.is_ipv4()
        || matches!(transmit.destination.ip(), IpAddr::V6(addr) if addr.to_ipv4_mapped().is_some());
    if is_ipv4 {
        if !sendmsg_einval {
            encoder.push(libc::IPPROTO_IP, libc::IP_TOS, ecn as IpTosTy);
        }
    } else {
        encoder.push(libc::IPPROTO_IPV6, libc::IPV6_TCLASS, ecn);
    }

    if let Some(segment_size) = transmit.segment_size {
        gso::set_segment_size(&mut encoder, segment_size as u16);
    }

    if let Some(ip) = &transmit.src_ip {
        match ip {
            IpAddr::V4(v4) => {
                #[cfg(any(target_os = "linux", target_os = "android"))]
                {
                    let pktinfo = libc::in_pktinfo {
                        ipi_ifindex: 0,
                        ipi_spec_dst: libc::in_addr {
                            s_addr: u32::from_ne_bytes(v4.octets()),
                        },
                        ipi_addr: libc::in_addr { s_addr: 0 },
                    };
                    encoder.push(libc::IPPROTO_IP, libc::IP_PKTINFO, pktinfo);
                }
                #[cfg(any(target_os = "freebsd", target_os = "macos", target_os = "ios"))]
                {
                    if encode_src_ip {
                        let addr = libc::in_addr {
                            s_addr: u32::from_ne_bytes(v4.octets()),
                        };
                        encoder.push(libc::IPPROTO_IP, libc::IP_RECVDSTADDR, addr);
                    }
                }
            }
            IpAddr::V6(v6) => {
                let pktinfo = libc::in6_pktinfo {
                    ipi6_ifindex: 0,
                    ipi6_addr: libc::in6_addr {
                        s6_addr: v6.octets(),
                    },
                };
                encoder.push(libc::IPPROTO_IPV6, libc::IPV6_PKTINFO, pktinfo);
            }
        }
    }

    encoder.finish();
}

fn prepare_recv(
    buf: &mut IoSliceMut,
    name: &mut MaybeUninit<libc::sockaddr_storage>,
    ctrl: &mut cmsg::Aligned<MaybeUninit<[u8; CMSG_LEN]>>,
    hdr: &mut libc::msghdr,
) {
    hdr.msg_name = name.as_mut_ptr() as _;
    hdr.msg_namelen = mem::size_of::<libc::sockaddr_storage>() as _;
    hdr.msg_iov = buf as *mut IoSliceMut as *mut libc::iovec;
    hdr.msg_iovlen = 1;
    hdr.msg_control = ctrl.0.as_mut_ptr() as _;
    hdr.msg_controllen = CMSG_LEN as _;
    hdr.msg_flags = 0;
}

fn decode_recv(
    name: &MaybeUninit<libc::sockaddr_storage>,
    hdr: &libc::msghdr,
    len: usize,
) -> RecvMeta {
    let name = unsafe { name.assume_init() };
    let mut ecn_bits = 0;
    let mut dst_ip = None;
    #[allow(unused_mut)] // only mutable on Linux
    let mut stride = len;

    let cmsg_iter = unsafe { cmsg::Iter::new(hdr) };
    for cmsg in cmsg_iter {
        match (cmsg.cmsg_level, cmsg.cmsg_type) {
            // FreeBSD uses IP_RECVTOS here, and we can be liberal because cmsgs are opt-in.
            (libc::IPPROTO_IP, libc::IP_TOS) | (libc::IPPROTO_IP, libc::IP_RECVTOS) => unsafe {
                ecn_bits = cmsg::decode::<u8, libc::cmsghdr>(cmsg);
            },
            (libc::IPPROTO_IPV6, libc::IPV6_TCLASS) => unsafe {
                // Temporary hack around broken macos ABI. Remove once upstream fixes it.
                // https://bugreport.apple.com/web/?problemID=48761855
                #[allow(clippy::unnecessary_cast)] // cmsg.cmsg_len defined as size_t
                if (cfg!(target_os = "macos") || cfg!(target_os = "ios"))
                    && cmsg.cmsg_len as usize == libc::CMSG_LEN(mem::size_of::<u8>() as _) as usize
                {
                    ecn_bits = cmsg::decode::<u8, libc::cmsghdr>(cmsg);
                } else {
                    ecn_bits = cmsg::decode::<libc::c_int, libc::cmsghdr>(cmsg) as u8;
                }
            },
            #[cfg(any(target_os = "linux", target_os = "android"))]
            (libc::IPPROTO_IP, libc::IP_PKTINFO) => {
                let pktinfo = unsafe { cmsg::decode::<libc::in_pktinfo, libc::cmsghdr>(cmsg) };
                dst_ip = Some(IpAddr::V4(Ipv4Addr::from(
                    pktinfo.ipi_addr.s_addr.to_ne_bytes(),
                )));
            }
            #[cfg(any(target_os = "freebsd", target_os = "macos", target_os = "ios"))]
            (libc::IPPROTO_IP, libc::IP_RECVDSTADDR) => {
                let in_addr = unsafe { cmsg::decode::<libc::in_addr, libc::cmsghdr>(cmsg) };
                dst_ip = Some(IpAddr::V4(Ipv4Addr::from(in_addr.s_addr.to_ne_bytes())));
            }
            (libc::IPPROTO_IPV6, libc::IPV6_PKTINFO) => {
                let pktinfo = unsafe { cmsg::decode::<libc::in6_pktinfo, libc::cmsghdr>(cmsg) };
                dst_ip = Some(IpAddr::V6(Ipv6Addr::from(pktinfo.ipi6_addr.s6_addr)));
            }
            #[cfg(target_os = "linux")]
            (libc::SOL_UDP, libc::UDP_GRO) => unsafe {
                stride = cmsg::decode::<libc::c_int, libc::cmsghdr>(cmsg) as usize;
            },
            _ => {}
        }
    }

    let addr = match libc::c_int::from(name.ss_family) {
        libc::AF_INET => {
            // Safety: if the ss_family field is AF_INET then storage must be a sockaddr_in.
            let addr: &libc::sockaddr_in =
                unsafe { &*(&name as *const _ as *const libc::sockaddr_in) };
            SocketAddr::V4(SocketAddrV4::new(
                Ipv4Addr::from(addr.sin_addr.s_addr.to_ne_bytes()),
                u16::from_be(addr.sin_port),
            ))
        }
        libc::AF_INET6 => {
            // Safety: if the ss_family field is AF_INET6 then storage must be a sockaddr_in6.
            let addr: &libc::sockaddr_in6 =
                unsafe { &*(&name as *const _ as *const libc::sockaddr_in6) };
            SocketAddr::V6(SocketAddrV6::new(
                Ipv6Addr::from(addr.sin6_addr.s6_addr),
                u16::from_be(addr.sin6_port),
                addr.sin6_flowinfo,
                addr.sin6_scope_id,
            ))
        }
        _ => unreachable!(),
    };

    RecvMeta {
        len,
        stride,
        addr,
        ecn: EcnCodepoint::from_bits(ecn_bits),
        dst_ip,
    }
}

#[cfg(not(any(target_os = "macos", target_os = "ios")))]
// Chosen somewhat arbitrarily; might benefit from additional tuning.
pub(crate) const BATCH_SIZE: usize = 32;

#[cfg(any(target_os = "macos", target_os = "ios"))]
pub(crate) const BATCH_SIZE: usize = 1;

#[cfg(target_os = "linux")]
mod gso {
    use super::*;

    /// Checks whether GSO support is available by setting the UDP_SEGMENT
    /// option on a socket
    pub(crate) fn max_gso_segments() -> usize {
        const GSO_SIZE: libc::c_int = 1500;

        let socket = match std::net::UdpSocket::bind("[::]:0")
            .or_else(|_| std::net::UdpSocket::bind("127.0.0.1:0"))
        {
            Ok(socket) => socket,
            Err(_) => return 1,
        };

        // As defined in linux/udp.h
        // #define UDP_MAX_SEGMENTS        (1 << 6UL)
        match set_socket_option(&socket, libc::SOL_UDP, libc::UDP_SEGMENT, GSO_SIZE) {
            Ok(()) => 64,
            Err(_) => 1,
        }
    }

    pub(crate) fn set_segment_size(encoder: &mut cmsg::Encoder<libc::msghdr>, segment_size: u16) {
        encoder.push(libc::SOL_UDP, libc::UDP_SEGMENT, segment_size);
    }
}

#[cfg(not(target_os = "linux"))]
mod gso {
    use super::*;

    pub(super) fn max_gso_segments() -> usize {
        1
    }

    pub(super) fn set_segment_size(_encoder: &mut cmsg::Encoder<libc::msghdr>, _segment_size: u16) {
        panic!("Setting a segment size is not supported on current platform");
    }
}

#[cfg(target_os = "linux")]
mod gro {
    use super::*;

    pub(crate) fn gro_segments() -> usize {
        let socket = match std::net::UdpSocket::bind("[::]:0")
            .or_else(|_| std::net::UdpSocket::bind("127.0.0.1:0"))
        {
            Ok(socket) => socket,
            Err(_) => return 1,
        };

        // As defined in net/ipv4/udp_offload.c
        // #define UDP_GRO_CNT_MAX 64
        //
        // NOTE: this MUST be set to UDP_GRO_CNT_MAX to ensure that the receive buffer size
        // (get_max_udp_payload_size() * gro_segments()) is large enough to hold the largest GRO
        // list the kernel might potentially produce. See
        // https://github.com/quinn-rs/quinn/pull/1354.
        match set_socket_option(&socket, libc::SOL_UDP, libc::UDP_GRO, OPTION_ON) {
            Ok(()) => 64,
            Err(_) => 1,
        }
    }
}

/// Returns whether the given socket option is supported on the current platform
///
/// Yields `Ok(true)` if the option was set successfully, `Ok(false)` if setting
/// the option raised an `ENOPROTOOPT` error, and `Err` for any other error.
fn set_socket_option_supported(
    socket: &impl AsRawFd,
    level: libc::c_int,
    name: libc::c_int,
    value: libc::c_int,
) -> io::Result<bool> {
    match set_socket_option(socket, level, name, value) {
        Ok(()) => Ok(true),
        Err(err) if err.raw_os_error() == Some(libc::ENOPROTOOPT) => Ok(false),
        Err(err) => Err(err),
    }
}

fn set_socket_option(
    socket: &impl AsRawFd,
    level: libc::c_int,
    name: libc::c_int,
    value: libc::c_int,
) -> io::Result<()> {
    let rc = unsafe {
        libc::setsockopt(
            socket.as_raw_fd(),
            level,
            name,
            &value as *const _ as _,
            mem::size_of_val(&value) as _,
        )
    };

    match rc == 0 {
        true => Ok(()),
        false => Err(io::Error::last_os_error()),
    }
}

const OPTION_ON: libc::c_int = 1;

#[cfg(not(target_os = "linux"))]
mod gro {
    pub(super) fn gro_segments() -> usize {
        1
    }
}
