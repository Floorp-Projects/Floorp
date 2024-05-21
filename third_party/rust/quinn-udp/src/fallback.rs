use std::{
    io::{self, IoSliceMut},
    sync::Mutex,
    time::Instant,
};

use super::{log_sendmsg_error, RecvMeta, Transmit, UdpSockRef, IO_ERROR_LOG_INTERVAL};

/// Fallback UDP socket interface that stubs out all special functionality
///
/// Used when a better implementation is not available for a particular target, at the cost of
/// reduced performance compared to that enabled by some target-specific interfaces.
#[derive(Debug)]
pub struct UdpSocketState {
    last_send_error: Mutex<Instant>,
}

impl UdpSocketState {
    pub fn new(socket: UdpSockRef<'_>) -> io::Result<Self> {
        socket.0.set_nonblocking(true)?;
        let now = Instant::now();
        Ok(Self {
            last_send_error: Mutex::new(now.checked_sub(2 * IO_ERROR_LOG_INTERVAL).unwrap_or(now)),
        })
    }

    pub fn send(&self, socket: UdpSockRef<'_>, transmit: &Transmit<'_>) -> io::Result<()> {
        let Err(e) = socket.0.send_to(
            transmit.contents,
            &socket2::SockAddr::from(transmit.destination),
        ) else {
            return Ok(());
        };
        if e.kind() == io::ErrorKind::WouldBlock {
            return Err(e);
        }

        // Other errors are ignored, since they will usually be handled
        // by higher level retransmits and timeouts.
        // - PermissionDenied errors have been observed due to iptable rules.
        //   Those are not fatal errors, since the
        //   configuration can be dynamically changed.
        // - Destination unreachable errors have been observed for other
        log_sendmsg_error(&self.last_send_error, e, transmit);
        Ok(())
    }

    pub fn recv(
        &self,
        socket: UdpSockRef<'_>,
        bufs: &mut [IoSliceMut<'_>],
        meta: &mut [RecvMeta],
    ) -> io::Result<usize> {
        // Safety: both `IoSliceMut` and `MaybeUninitSlice` promise to have the
        // same layout, that of `iovec`/`WSABUF`. Furthermore `recv_vectored`
        // promises to not write unitialised bytes to the `bufs` and pass it
        // directly to the `recvmsg` system call, so this is safe.
        let bufs = unsafe {
            &mut *(bufs as *mut [IoSliceMut<'_>] as *mut [socket2::MaybeUninitSlice<'_>])
        };
        let (len, _flags, addr) = socket.0.recv_from_vectored(bufs)?;
        meta[0] = RecvMeta {
            len,
            stride: len,
            addr: addr.as_socket().unwrap(),
            ecn: None,
            dst_ip: None,
        };
        Ok(1)
    }

    #[inline]
    pub fn max_gso_segments(&self) -> usize {
        1
    }

    #[inline]
    pub fn gro_segments(&self) -> usize {
        1
    }

    #[inline]
    pub fn may_fragment(&self) -> bool {
        true
    }
}

pub(crate) const BATCH_SIZE: usize = 1;
