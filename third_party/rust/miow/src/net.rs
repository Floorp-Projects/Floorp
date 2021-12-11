//! Extensions and types for the standard networking primitives.
//!
//! This module contains a number of extension traits for the types in
//! `std::net` for Windows-specific functionality.

use std::cmp;
use std::io;
use std::mem;
use std::sync::atomic::{AtomicUsize, Ordering, ATOMIC_USIZE_INIT};
use std::net::{TcpStream, UdpSocket, SocketAddr, TcpListener};
use std::net::{SocketAddrV4, Ipv4Addr, SocketAddrV6, Ipv6Addr};
use std::os::windows::prelude::*;

use winapi::ctypes::*;
use winapi::shared::guiddef::*;
use winapi::shared::minwindef::*;
use winapi::shared::minwindef::{FALSE, TRUE};
use winapi::shared::ntdef::*;
use winapi::shared::ws2def::*;
use winapi::shared::ws2def::SOL_SOCKET;
use winapi::shared::ws2ipdef::*;
use winapi::um::minwinbase::*;
use winapi::um::winsock2::*;

/// A type to represent a buffer in which a socket address will be stored.
///
/// This type is used with the `recv_from_overlapped` function on the
/// `UdpSocketExt` trait to provide space for the overlapped I/O operation to
/// fill in the address upon completion.
#[derive(Clone, Copy)]
pub struct SocketAddrBuf {
    buf: SOCKADDR_STORAGE,
    len: c_int,
}

/// A type to represent a buffer in which an accepted socket's address will be
/// stored.
///
/// This type is used with the `accept_overlapped` method on the
/// `TcpListenerExt` trait to provide space for the overlapped I/O operation to
/// fill in the socket addresses upon completion.
#[repr(C)]
pub struct AcceptAddrsBuf {
    // For AcceptEx we've got the restriction that the addresses passed in that
    // buffer need to be at least 16 bytes more than the maximum address length
    // for the protocol in question, so add some extra here and there
    local: SOCKADDR_STORAGE,
    _pad1: [u8; 16],
    remote: SOCKADDR_STORAGE,
    _pad2: [u8; 16],
}

/// The parsed return value of `AcceptAddrsBuf`.
pub struct AcceptAddrs<'a> {
    local: LPSOCKADDR,
    local_len: c_int,
    remote: LPSOCKADDR,
    remote_len: c_int,
    _data: &'a AcceptAddrsBuf,
}

struct WsaExtension {
    guid: GUID,
    val: AtomicUsize,
}

/// Additional methods for the `TcpStream` type in the standard library.
pub trait TcpStreamExt {
    /// Execute an overlapped read I/O operation on this TCP stream.
    ///
    /// This function will issue an overlapped I/O read (via `WSARecv`) on this
    /// socket. The provided buffer will be filled in when the operation
    /// completes and the given `OVERLAPPED` instance is used to track the
    /// overlapped operation.
    ///
    /// If the operation succeeds, `Ok(Some(n))` is returned indicating how
    /// many bytes were read. If the operation returns an error indicating that
    /// the I/O is currently pending, `Ok(None)` is returned. Otherwise, the
    /// error associated with the operation is returned and no overlapped
    /// operation is enqueued.
    ///
    /// The number of bytes read will be returned as part of the completion
    /// notification when the I/O finishes.
    ///
    /// # Unsafety
    ///
    /// This function is unsafe because the kernel requires that the `buf` and
    /// `overlapped` pointers are valid until the end of the I/O operation. The
    /// kernel also requires that `overlapped` is unique for this I/O operation
    /// and is not in use for any other I/O.
    ///
    /// To safely use this function callers must ensure that these two input
    /// pointers are valid until the I/O operation is completed, typically via
    /// completion ports and waiting to receive the completion notification on
    /// the port.
    unsafe fn read_overlapped(&self,
                              buf: &mut [u8],
                              overlapped: *mut OVERLAPPED)
                              -> io::Result<Option<usize>>;

    /// Execute an overlapped write I/O operation on this TCP stream.
    ///
    /// This function will issue an overlapped I/O write (via `WSASend`) on this
    /// socket. The provided buffer will be written when the operation completes
    /// and the given `OVERLAPPED` instance is used to track the overlapped
    /// operation.
    ///
    /// If the operation succeeds, `Ok(Some(n))` is returned where `n` is the
    /// number of bytes that were written. If the operation returns an error
    /// indicating that the I/O is currently pending, `Ok(None)` is returned.
    /// Otherwise, the error associated with the operation is returned and no
    /// overlapped operation is enqueued.
    ///
    /// The number of bytes written will be returned as part of the completion
    /// notification when the I/O finishes.
    ///
    /// # Unsafety
    ///
    /// This function is unsafe because the kernel requires that the `buf` and
    /// `overlapped` pointers are valid until the end of the I/O operation. The
    /// kernel also requires that `overlapped` is unique for this I/O operation
    /// and is not in use for any other I/O.
    ///
    /// To safely use this function callers must ensure that these two input
    /// pointers are valid until the I/O operation is completed, typically via
    /// completion ports and waiting to receive the completion notification on
    /// the port.
    unsafe fn write_overlapped(&self,
                               buf: &[u8],
                               overlapped: *mut OVERLAPPED)
                               -> io::Result<Option<usize>>;

    /// Attempt to consume the internal socket in this builder by executing an
    /// overlapped connect operation.
    ///
    /// This function will issue a connect operation to the address specified on
    /// the underlying socket, flagging it as an overlapped operation which will
    /// complete asynchronously. If successful this function will return the
    /// corresponding TCP stream.
    ///
    /// The `buf` argument provided is an initial buffer of data that should be
    /// sent after the connection is initiated. It's acceptable to
    /// pass an empty slice here.
    ///
    /// This function will also return whether the connect immediately
    /// succeeded or not. If `None` is returned then the I/O operation is still
    /// pending and will complete at a later date, and if `Some(bytes)` is
    /// returned then that many bytes were transferred.
    ///
    /// Note that to succeed this requires that the underlying socket has
    /// previously been bound via a call to `bind` to a local address.
    ///
    /// # Unsafety
    ///
    /// This function is unsafe because the kernel requires that the
    /// `overlapped` and `buf` pointers to be  valid until the end of the I/O
    /// operation. The kernel also requires that `overlapped` is unique for
    /// this I/O operation and is not in use for any other I/O.
    ///
    /// To safely use this function callers must ensure that this pointer is
    /// valid until the I/O operation is completed, typically via completion
    /// ports and waiting to receive the completion notification on the port.
    unsafe fn connect_overlapped(&self,
                                 addr: &SocketAddr,
                                 buf: &[u8],
                                 overlapped: *mut OVERLAPPED)
                                 -> io::Result<Option<usize>>;

    /// Once a `connect_overlapped` has finished, this function needs to be
    /// called to finish the connect operation.
    ///
    /// Currently this just calls `setsockopt` with `SO_UPDATE_CONNECT_CONTEXT`
    /// to ensure that further functions like `getpeername` and `getsockname`
    /// work correctly.
    fn connect_complete(&self) -> io::Result<()>;

    /// Calls the `GetOverlappedResult` function to get the result of an
    /// overlapped operation for this handle.
    ///
    /// This function takes the `OVERLAPPED` argument which must have been used
    /// to initiate an overlapped I/O operation, and returns either the
    /// successful number of bytes transferred during the operation or an error
    /// if one occurred, along with the results of the `lpFlags` parameter of
    /// the relevant operation, if applicable.
    ///
    /// # Unsafety
    ///
    /// This function is unsafe as `overlapped` must have previously been used
    /// to execute an operation for this handle, and it must also be a valid
    /// pointer to an `OVERLAPPED` instance.
    ///
    /// # Panics
    ///
    /// This function will panic
    unsafe fn result(&self, overlapped: *mut OVERLAPPED)
                     -> io::Result<(usize, u32)>;
}

/// Additional methods for the `UdpSocket` type in the standard library.
pub trait UdpSocketExt {
    /// Execute an overlapped receive I/O operation on this UDP socket.
    ///
    /// This function will issue an overlapped I/O read (via `WSARecvFrom`) on
    /// this socket. The provided buffer will be filled in when the operation
    /// completes, the source from where the data came from will be written to
    /// `addr`, and the given `OVERLAPPED` instance is used to track the
    /// overlapped operation.
    ///
    /// If the operation succeeds, `Ok(Some(n))` is returned where `n` is the
    /// number of bytes that were read. If the operation returns an error
    /// indicating that the I/O is currently pending, `Ok(None)` is returned.
    /// Otherwise, the error associated with the operation is returned and no
    /// overlapped operation is enqueued.
    ///
    /// The number of bytes read will be returned as part of the completion
    /// notification when the I/O finishes.
    ///
    /// # Unsafety
    ///
    /// This function is unsafe because the kernel requires that the `buf`,
    /// `addr`, and `overlapped` pointers are valid until the end of the I/O
    /// operation. The kernel also requires that `overlapped` is unique for this
    /// I/O operation and is not in use for any other I/O.
    ///
    /// To safely use this function callers must ensure that these two input
    /// pointers are valid until the I/O operation is completed, typically via
    /// completion ports and waiting to receive the completion notification on
    /// the port.
    unsafe fn recv_from_overlapped(&self,
                                   buf: &mut [u8],
                                   addr: *mut SocketAddrBuf,
                                   overlapped: *mut OVERLAPPED)
                                   -> io::Result<Option<usize>>;

    /// Execute an overlapped receive I/O operation on this UDP socket.
    ///
    /// This function will issue an overlapped I/O read (via `WSARecv`) on
    /// this socket. The provided buffer will be filled in when the operation
    /// completes, the source from where the data came from will be written to
    /// `addr`, and the given `OVERLAPPED` instance is used to track the
    /// overlapped operation.
    ///
    /// If the operation succeeds, `Ok(Some(n))` is returned where `n` is the
    /// number of bytes that were read. If the operation returns an error
    /// indicating that the I/O is currently pending, `Ok(None)` is returned.
    /// Otherwise, the error associated with the operation is returned and no
    /// overlapped operation is enqueued.
    ///
    /// The number of bytes read will be returned as part of the completion
    /// notification when the I/O finishes.
    ///
    /// # Unsafety
    ///
    /// This function is unsafe because the kernel requires that the `buf`,
    /// and `overlapped` pointers are valid until the end of the I/O
    /// operation. The kernel also requires that `overlapped` is unique for this
    /// I/O operation and is not in use for any other I/O.
    ///
    /// To safely use this function callers must ensure that these two input
    /// pointers are valid until the I/O operation is completed, typically via
    /// completion ports and waiting to receive the completion notification on
    /// the port.
    unsafe fn recv_overlapped(&self,
                              buf: &mut [u8],
                              overlapped: *mut OVERLAPPED)
                              -> io::Result<Option<usize>>;

    /// Execute an overlapped send I/O operation on this UDP socket.
    ///
    /// This function will issue an overlapped I/O write (via `WSASendTo`) on
    /// this socket to the address specified by `addr`. The provided buffer will
    /// be written when the operation completes and the given `OVERLAPPED`
    /// instance is used to track the overlapped operation.
    ///
    /// If the operation succeeds, `Ok(Some(n0)` is returned where `n` byte
    /// were written. If the operation returns an error indicating that the I/O
    /// is currently pending, `Ok(None)` is returned. Otherwise, the error
    /// associated with the operation is returned and no overlapped operation
    /// is enqueued.
    ///
    /// The number of bytes written will be returned as part of the completion
    /// notification when the I/O finishes.
    ///
    /// # Unsafety
    ///
    /// This function is unsafe because the kernel requires that the `buf` and
    /// `overlapped` pointers are valid until the end of the I/O operation. The
    /// kernel also requires that `overlapped` is unique for this I/O operation
    /// and is not in use for any other I/O.
    ///
    /// To safely use this function callers must ensure that these two input
    /// pointers are valid until the I/O operation is completed, typically via
    /// completion ports and waiting to receive the completion notification on
    /// the port.
    unsafe fn send_to_overlapped(&self,
                                 buf: &[u8],
                                 addr: &SocketAddr,
                                 overlapped: *mut OVERLAPPED)
                                 -> io::Result<Option<usize>>;

    /// Execute an overlapped send I/O operation on this UDP socket.
    ///
    /// This function will issue an overlapped I/O write (via `WSASend`) on
    /// this socket to the address it was previously connected to. The provided
    /// buffer will be written when the operation completes and the given `OVERLAPPED`
    /// instance is used to track the overlapped operation.
    ///
    /// If the operation succeeds, `Ok(Some(n0)` is returned where `n` byte
    /// were written. If the operation returns an error indicating that the I/O
    /// is currently pending, `Ok(None)` is returned. Otherwise, the error
    /// associated with the operation is returned and no overlapped operation
    /// is enqueued.
    ///
    /// The number of bytes written will be returned as part of the completion
    /// notification when the I/O finishes.
    ///
    /// # Unsafety
    ///
    /// This function is unsafe because the kernel requires that the `buf` and
    /// `overlapped` pointers are valid until the end of the I/O operation. The
    /// kernel also requires that `overlapped` is unique for this I/O operation
    /// and is not in use for any other I/O.
    ///
    /// To safely use this function callers must ensure that these two input
    /// pointers are valid until the I/O operation is completed, typically via
    /// completion ports and waiting to receive the completion notification on
    /// the port.
    unsafe fn send_overlapped(&self,
                                 buf: &[u8],
                                 overlapped: *mut OVERLAPPED)
                                 -> io::Result<Option<usize>>;

    /// Calls the `GetOverlappedResult` function to get the result of an
    /// overlapped operation for this handle.
    ///
    /// This function takes the `OVERLAPPED` argument which must have been used
    /// to initiate an overlapped I/O operation, and returns either the
    /// successful number of bytes transferred during the operation or an error
    /// if one occurred, along with the results of the `lpFlags` parameter of
    /// the relevant operation, if applicable.
    ///
    /// # Unsafety
    ///
    /// This function is unsafe as `overlapped` must have previously been used
    /// to execute an operation for this handle, and it must also be a valid
    /// pointer to an `OVERLAPPED` instance.
    ///
    /// # Panics
    ///
    /// This function will panic
    unsafe fn result(&self, overlapped: *mut OVERLAPPED)
                     -> io::Result<(usize, u32)>;
}

/// Additional methods for the `TcpListener` type in the standard library.
pub trait TcpListenerExt {
    /// Perform an accept operation on this listener, accepting a connection in
    /// an overlapped fashion.
    ///
    /// This function will issue an I/O request to accept an incoming connection
    /// with the specified overlapped instance. The `socket` provided must be a
    /// configured but not bound or connected socket, and if successful this
    /// will consume the internal socket of the builder to return a TCP stream.
    ///
    /// The `addrs` buffer provided will be filled in with the local and remote
    /// addresses of the connection upon completion.
    ///
    /// If the accept succeeds immediately, `Ok(true)` is returned. If
    /// the connect indicates that the I/O is currently pending, `Ok(false)` is
    /// returned. Otherwise, the error associated with the operation is
    /// returned and no overlapped operation is enqueued.
    ///
    /// # Unsafety
    ///
    /// This function is unsafe because the kernel requires that the
    /// `addrs` and `overlapped` pointers are valid until the end of the I/O
    /// operation. The kernel also requires that `overlapped` is unique for this
    /// I/O operation and is not in use for any other I/O.
    ///
    /// To safely use this function callers must ensure that the pointers are
    /// valid until the I/O operation is completed, typically via completion
    /// ports and waiting to receive the completion notification on the port.
    unsafe fn accept_overlapped(&self,
                                socket: &TcpStream,
                                addrs: &mut AcceptAddrsBuf,
                                overlapped: *mut OVERLAPPED)
                                -> io::Result<bool>;

    /// Once an `accept_overlapped` has finished, this function needs to be
    /// called to finish the accept operation.
    ///
    /// Currently this just calls `setsockopt` with `SO_UPDATE_ACCEPT_CONTEXT`
    /// to ensure that further functions like `getpeername` and `getsockname`
    /// work correctly.
    fn accept_complete(&self, socket: &TcpStream) -> io::Result<()>;

    /// Calls the `GetOverlappedResult` function to get the result of an
    /// overlapped operation for this handle.
    ///
    /// This function takes the `OVERLAPPED` argument which must have been used
    /// to initiate an overlapped I/O operation, and returns either the
    /// successful number of bytes transferred during the operation or an error
    /// if one occurred, along with the results of the `lpFlags` parameter of
    /// the relevant operation, if applicable.
    ///
    /// # Unsafety
    ///
    /// This function is unsafe as `overlapped` must have previously been used
    /// to execute an operation for this handle, and it must also be a valid
    /// pointer to an `OVERLAPPED` instance.
    ///
    /// # Panics
    ///
    /// This function will panic
    unsafe fn result(&self, overlapped: *mut OVERLAPPED)
                     -> io::Result<(usize, u32)>;
}

#[doc(hidden)]
trait NetInt {
    fn from_be(i: Self) -> Self;
    fn to_be(&self) -> Self;
}
macro_rules! doit {
    ($($t:ident)*) => ($(impl NetInt for $t {
        fn from_be(i: Self) -> Self { <$t>::from_be(i) }
        fn to_be(&self) -> Self { <$t>::to_be(*self) }
    })*)
}
doit! { i8 i16 i32 i64 isize u8 u16 u32 u64 usize }

// fn hton<I: NetInt>(i: I) -> I { i.to_be() }
fn ntoh<I: NetInt>(i: I) -> I { I::from_be(i) }

fn last_err() -> io::Result<Option<usize>> {
    let err = unsafe { WSAGetLastError() };
    if err == WSA_IO_PENDING as i32 {
        Ok(None)
    } else {
        Err(io::Error::from_raw_os_error(err))
    }
}

fn cvt(i: c_int, size: DWORD) -> io::Result<Option<usize>> {
    if i == SOCKET_ERROR {
        last_err()
    } else {
        Ok(Some(size as usize))
    }
}

fn socket_addr_to_ptrs(addr: &SocketAddr) -> (*const SOCKADDR, c_int) {
    match *addr {
        SocketAddr::V4(ref a) => {
            (a as *const _ as *const _, mem::size_of::<SOCKADDR_IN>() as c_int)
        }
        SocketAddr::V6(ref a) => {
            (a as *const _ as *const _, mem::size_of::<SOCKADDR_IN6_LH>() as c_int)
        }
    }
}

unsafe fn ptrs_to_socket_addr(ptr: *const SOCKADDR,
                              len: c_int) -> Option<SocketAddr> {
    if (len as usize) < mem::size_of::<c_int>() {
        return None
    }
    match (*ptr).sa_family as i32 {
        AF_INET if len as usize >= mem::size_of::<SOCKADDR_IN>() => {
            let b = &*(ptr as *const SOCKADDR_IN);
            let ip = ntoh(*b.sin_addr.S_un.S_addr());
            let ip = Ipv4Addr::new((ip >> 24) as u8,
                                   (ip >> 16) as u8,
                                   (ip >>  8) as u8,
                                   (ip >>  0) as u8);
            Some(SocketAddr::V4(SocketAddrV4::new(ip, ntoh(b.sin_port))))
        }
        AF_INET6 if len as usize >= mem::size_of::<SOCKADDR_IN6_LH>() => {
            let b = &*(ptr as *const SOCKADDR_IN6_LH);
            let arr = b.sin6_addr.u.Byte();
            let ip = Ipv6Addr::new(
                ((arr[0] as u16) << 8) | (arr[1] as u16),
                ((arr[2] as u16) << 8) | (arr[3] as u16),
                ((arr[4] as u16) << 8) | (arr[5] as u16),
                ((arr[6] as u16) << 8) | (arr[7] as u16),
                ((arr[8] as u16) << 8) | (arr[9] as u16),
                ((arr[10] as u16) << 8) | (arr[11] as u16),
                ((arr[12] as u16) << 8) | (arr[13] as u16),
                ((arr[14] as u16) << 8) | (arr[15] as u16));
            let addr = SocketAddrV6::new(ip, ntoh(b.sin6_port),
                                         ntoh(b.sin6_flowinfo),
                                         ntoh(*b.u.sin6_scope_id()));
            Some(SocketAddr::V6(addr))
        }
        _ => None
    }
}

unsafe fn slice2buf(slice: &[u8]) -> WSABUF {
    WSABUF {
        len: cmp::min(slice.len(), <u_long>::max_value() as usize) as u_long,
        buf: slice.as_ptr() as *mut _,
    }
}

unsafe fn result(socket: SOCKET, overlapped: *mut OVERLAPPED)
                 -> io::Result<(usize, u32)> {
    let mut transferred = 0;
    let mut flags = 0;
    let r = WSAGetOverlappedResult(socket,
                                   overlapped,
                                   &mut transferred,
                                   FALSE,
                                   &mut flags);
    if r == 0 {
        Err(io::Error::last_os_error())
    } else {
        Ok((transferred as usize, flags))
    }
}

impl TcpStreamExt for TcpStream {
    unsafe fn read_overlapped(&self,
                              buf: &mut [u8],
                              overlapped: *mut OVERLAPPED)
                              -> io::Result<Option<usize>> {
        let mut buf = slice2buf(buf);
        let mut flags = 0;
        let mut bytes_read: DWORD = 0;
        let r = WSARecv(self.as_raw_socket() as SOCKET, &mut buf, 1,
                        &mut bytes_read, &mut flags, overlapped, None);
        cvt(r, bytes_read)
    }

    unsafe fn write_overlapped(&self,
                               buf: &[u8],
                               overlapped: *mut OVERLAPPED)
                               -> io::Result<Option<usize>> {
        let mut buf = slice2buf(buf);
        let mut bytes_written = 0;

        // Note here that we capture the number of bytes written. The
        // documentation on MSDN, however, states:
        //
        // > Use NULL for this parameter if the lpOverlapped parameter is not
        // > NULL to avoid potentially erroneous results. This parameter can be
        // > NULL only if the lpOverlapped parameter is not NULL.
        //
        // If we're not passing a null overlapped pointer here, then why are we
        // then capturing the number of bytes! Well so it turns out that this is
        // clearly faster to learn the bytes here rather than later calling
        // `WSAGetOverlappedResult`, and in practice almost all implementations
        // use this anyway [1].
        //
        // As a result we use this to and report back the result.
        //
        // [1]: https://github.com/carllerche/mio/pull/520#issuecomment-273983823
        let r = WSASend(self.as_raw_socket() as SOCKET, &mut buf, 1,
                        &mut bytes_written, 0, overlapped, None);
        cvt(r, bytes_written)
    }

    unsafe fn connect_overlapped(&self,
                                 addr: &SocketAddr,
                                 buf: &[u8],
                                 overlapped: *mut OVERLAPPED)
                                 -> io::Result<Option<usize>> {
        connect_overlapped(self.as_raw_socket() as SOCKET, addr, buf, overlapped)
    }

    fn connect_complete(&self) -> io::Result<()> {
        const SO_UPDATE_CONNECT_CONTEXT: c_int = 0x7010;
        let result = unsafe {
            setsockopt(self.as_raw_socket() as SOCKET,
                       SOL_SOCKET,
                       SO_UPDATE_CONNECT_CONTEXT,
                       0 as *const _,
                       0)
        };
        if result == 0 {
            Ok(())
        } else {
            Err(io::Error::last_os_error())
        }
    }

    unsafe fn result(&self, overlapped: *mut OVERLAPPED)
                     -> io::Result<(usize, u32)> {
        result(self.as_raw_socket() as SOCKET, overlapped)
    }
}

unsafe fn connect_overlapped(socket: SOCKET,
                             addr: &SocketAddr,
                             buf: &[u8],
                             overlapped: *mut OVERLAPPED)
                             -> io::Result<Option<usize>> {
    static CONNECTEX: WsaExtension = WsaExtension {
        guid: GUID {
            Data1: 0x25a207b9,
            Data2: 0xddf3,
            Data3: 0x4660,
            Data4: [0x8e, 0xe9, 0x76, 0xe5, 0x8c, 0x74, 0x06, 0x3e],
        },
        val: ATOMIC_USIZE_INIT,
    };
    type ConnectEx = unsafe extern "system" fn(SOCKET, *const SOCKADDR,
                                               c_int, PVOID, DWORD, LPDWORD,
                                               LPOVERLAPPED) -> BOOL;

    let ptr = try!(CONNECTEX.get(socket));
    assert!(ptr != 0);
    let connect_ex = mem::transmute::<_, ConnectEx>(ptr);

    let (addr_buf, addr_len) = socket_addr_to_ptrs(addr);
    let mut bytes_sent: DWORD = 0;
    let r = connect_ex(socket, addr_buf, addr_len,
                       buf.as_ptr() as *mut _,
                       buf.len() as u32,
                       &mut bytes_sent, overlapped);
    if r == TRUE {
        Ok(Some(bytes_sent as usize))
    } else {
        last_err()
    }
}

impl UdpSocketExt for UdpSocket {
    unsafe fn recv_from_overlapped(&self,
                                   buf: &mut [u8],
                                   addr: *mut SocketAddrBuf,
                                   overlapped: *mut OVERLAPPED)
                                   -> io::Result<Option<usize>> {
        let mut buf = slice2buf(buf);
        let mut flags = 0;
        let mut received_bytes: DWORD = 0;
        let r = WSARecvFrom(self.as_raw_socket() as SOCKET, &mut buf, 1,
                            &mut received_bytes, &mut flags,
                            &mut (*addr).buf as *mut _ as *mut _,
                            &mut (*addr).len,
                            overlapped, None);
        cvt(r, received_bytes)
    }

    unsafe fn recv_overlapped(&self,
                              buf: &mut [u8],
                              overlapped: *mut OVERLAPPED)
                              -> io::Result<Option<usize>> {
        let mut buf = slice2buf(buf);
        let mut flags = 0;
        let mut received_bytes: DWORD = 0;
        let r = WSARecv(self.as_raw_socket() as SOCKET, &mut buf, 1,
                            &mut received_bytes, &mut flags,
                            overlapped, None);
        cvt(r, received_bytes)
    }

    unsafe fn send_to_overlapped(&self,
                                 buf: &[u8],
                                 addr: &SocketAddr,
                                 overlapped: *mut OVERLAPPED)
                                 -> io::Result<Option<usize>> {
        let (addr_buf, addr_len) = socket_addr_to_ptrs(addr);
        let mut buf = slice2buf(buf);
        let mut sent_bytes = 0;
        let r = WSASendTo(self.as_raw_socket() as SOCKET, &mut buf, 1,
                          &mut sent_bytes, 0,
                          addr_buf as *const _, addr_len,
                          overlapped, None);
        cvt(r, sent_bytes)
    }

    unsafe fn send_overlapped(&self,
                              buf: &[u8],
                              overlapped: *mut OVERLAPPED)
                              -> io::Result<Option<usize>> {
        let mut buf = slice2buf(buf);
        let mut sent_bytes = 0;
        let r = WSASend(self.as_raw_socket() as SOCKET, &mut buf, 1,
                          &mut sent_bytes, 0,
                          overlapped, None);
        cvt(r, sent_bytes)
    }

    unsafe fn result(&self, overlapped: *mut OVERLAPPED)
                     -> io::Result<(usize, u32)> {
        result(self.as_raw_socket() as SOCKET, overlapped)
    }
}

impl TcpListenerExt for TcpListener {
    unsafe fn accept_overlapped(&self,
                                socket: &TcpStream,
                                addrs: &mut AcceptAddrsBuf,
                                overlapped: *mut OVERLAPPED)
                                -> io::Result<bool> {
        static ACCEPTEX: WsaExtension = WsaExtension {
            guid: GUID {
                Data1: 0xb5367df1,
                Data2: 0xcbac,
                Data3: 0x11cf,
                Data4: [0x95, 0xca, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92],
            },
            val: ATOMIC_USIZE_INIT,
        };
        type AcceptEx = unsafe extern "system" fn(SOCKET, SOCKET, PVOID,
                                                  DWORD, DWORD, DWORD, LPDWORD,
                                                  LPOVERLAPPED) -> BOOL;

        let ptr = try!(ACCEPTEX.get(self.as_raw_socket() as SOCKET));
        assert!(ptr != 0);
        let accept_ex = mem::transmute::<_, AcceptEx>(ptr);

        let mut bytes = 0;
        let (a, b, c, d) = (*addrs).args();
        let r = accept_ex(self.as_raw_socket() as SOCKET, socket.as_raw_socket() as SOCKET,
                          a, b, c, d, &mut bytes, overlapped);
        let succeeded = if r == TRUE {
            true
        } else {
            try!(last_err());
            false
        };
        Ok(succeeded)
    }

    fn accept_complete(&self, socket: &TcpStream) -> io::Result<()> {
        const SO_UPDATE_ACCEPT_CONTEXT: c_int = 0x700B;
        let me = self.as_raw_socket();
        let result = unsafe {
            setsockopt(socket.as_raw_socket() as SOCKET,
                       SOL_SOCKET,
                       SO_UPDATE_ACCEPT_CONTEXT,
                       &me as *const _ as *const _,
                       mem::size_of_val(&me) as c_int)
        };
        if result == 0 {
            Ok(())
        } else {
            Err(io::Error::last_os_error())
        }
    }

    unsafe fn result(&self, overlapped: *mut OVERLAPPED)
                     -> io::Result<(usize, u32)> {
        result(self.as_raw_socket() as SOCKET, overlapped)
    }
}

impl SocketAddrBuf {
    /// Creates a new blank socket address buffer.
    ///
    /// This should be used before a call to `recv_from_overlapped` overlapped
    /// to create an instance to pass down.
    pub fn new() -> SocketAddrBuf {
        SocketAddrBuf {
            buf: unsafe { mem::zeroed() },
            len: mem::size_of::<SOCKADDR_STORAGE>() as c_int,
        }
    }

    /// Parses this buffer to return a standard socket address.
    ///
    /// This function should be called after the buffer has been filled in with
    /// a call to `recv_from_overlapped` being completed. It will interpret the
    /// address filled in and return the standard socket address type.
    ///
    /// If an error is encountered then `None` is returned.
    pub fn to_socket_addr(&self) -> Option<SocketAddr> {
        unsafe {
            ptrs_to_socket_addr(&self.buf as *const _ as *const _, self.len)
        }
    }
}

static GETACCEPTEXSOCKADDRS: WsaExtension = WsaExtension {
    guid: GUID {
        Data1: 0xb5367df2,
        Data2: 0xcbac,
        Data3: 0x11cf,
        Data4: [0x95, 0xca, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92],
    },
    val: ATOMIC_USIZE_INIT,
};
type GetAcceptExSockaddrs = unsafe extern "system" fn(PVOID, DWORD, DWORD, DWORD,
                                                      *mut LPSOCKADDR, LPINT,
                                                      *mut LPSOCKADDR, LPINT);

impl AcceptAddrsBuf {
    /// Creates a new blank buffer ready to be passed to a call to
    /// `accept_overlapped`.
    pub fn new() -> AcceptAddrsBuf {
        unsafe { mem::zeroed() }
    }

    /// Parses the data contained in this address buffer, returning the parsed
    /// result if successful.
    ///
    /// This function can be called after a call to `accept_overlapped` has
    /// succeeded to parse out the data that was written in.
    pub fn parse(&self, socket: &TcpListener) -> io::Result<AcceptAddrs> {
        let mut ret = AcceptAddrs {
            local: 0 as *mut _, local_len: 0,
            remote: 0 as *mut _, remote_len: 0,
            _data: self,
        };
        let ptr = try!(GETACCEPTEXSOCKADDRS.get(socket.as_raw_socket() as SOCKET));
        assert!(ptr != 0);
        unsafe {
            let get_sockaddrs = mem::transmute::<_, GetAcceptExSockaddrs>(ptr);
            let (a, b, c, d) = self.args();
            get_sockaddrs(a, b, c, d,
                          &mut ret.local, &mut ret.local_len,
                          &mut ret.remote, &mut ret.remote_len);
            Ok(ret)
        }
    }

    fn args(&self) -> (PVOID, DWORD, DWORD, DWORD) {
        let remote_offset = unsafe {
            &(*(0 as *const AcceptAddrsBuf)).remote as *const _ as usize
        };
        (self as *const _ as *mut _, 0, remote_offset as DWORD,
         (mem::size_of_val(self) - remote_offset) as DWORD)
    }
}

impl<'a> AcceptAddrs<'a> {
    /// Returns the local socket address contained in this buffer.
    pub fn local(&self) -> Option<SocketAddr> {
        unsafe { ptrs_to_socket_addr(self.local, self.local_len) }
    }

    /// Returns the remote socket address contained in this buffer.
    pub fn remote(&self) -> Option<SocketAddr> {
        unsafe { ptrs_to_socket_addr(self.remote, self.remote_len) }
    }
}

impl WsaExtension {
    fn get(&self, socket: SOCKET) -> io::Result<usize> {
        let prev = self.val.load(Ordering::SeqCst);
        if prev != 0 && !cfg!(debug_assertions) {
            return Ok(prev)
        }
        let mut ret = 0 as usize;
        let mut bytes = 0;
        let r = unsafe {
            WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                     &self.guid as *const _ as *mut _,
                     mem::size_of_val(&self.guid) as DWORD,
                     &mut ret as *mut _ as *mut _,
                     mem::size_of_val(&ret) as DWORD,
                     &mut bytes,
                     0 as *mut _, None)
        };
        cvt(r, 0).map(|_| {
            debug_assert_eq!(bytes as usize, mem::size_of_val(&ret));
            debug_assert!(prev == 0 || prev == ret);
            self.val.store(ret, Ordering::SeqCst);
            ret
        })

    }
}

#[cfg(test)]
mod tests {
    use std::net::{TcpListener, UdpSocket, TcpStream, SocketAddr};
    use std::thread;
    use std::io::prelude::*;

    use socket2::{Socket, Type, Domain};

    use Overlapped;
    use iocp::CompletionPort;
    use net::{TcpStreamExt, UdpSocketExt, SocketAddrBuf};
    use net::{TcpListenerExt, AcceptAddrsBuf};

    fn each_ip(f: &mut FnMut(SocketAddr)) {
        f(t!("127.0.0.1:0".parse()));
        f(t!("[::1]:0".parse()));
    }

    #[test]
    fn tcp_read() {
        each_ip(&mut |addr| {
            let l = t!(TcpListener::bind(addr));
            let addr = t!(l.local_addr());
            let t = thread::spawn(move || {
                let mut a = t!(l.accept()).0;
                t!(a.write_all(&[1, 2, 3]));
            });

            let cp = t!(CompletionPort::new(1));
            let s = t!(TcpStream::connect(addr));
            t!(cp.add_socket(1, &s));

            let mut b = [0; 10];
            let a = Overlapped::zero();
            unsafe {
                t!(s.read_overlapped(&mut b, a.raw()));
            }
            let status = t!(cp.get(None));
            assert_eq!(status.bytes_transferred(), 3);
            assert_eq!(status.token(), 1);
            assert_eq!(status.overlapped(), a.raw());
            assert_eq!(&b[0..3], &[1, 2, 3]);

            t!(t.join());
        })
    }

    #[test]
    fn tcp_write() {
        each_ip(&mut |addr| {
            let l = t!(TcpListener::bind(addr));
            let addr = t!(l.local_addr());
            let t = thread::spawn(move || {
                let mut a = t!(l.accept()).0;
                let mut b = [0; 10];
                let n = t!(a.read(&mut b));
                assert_eq!(n, 3);
                assert_eq!(&b[0..3], &[1, 2, 3]);
            });

            let cp = t!(CompletionPort::new(1));
            let s = t!(TcpStream::connect(addr));
            t!(cp.add_socket(1, &s));

            let b = [1, 2, 3];
            let a = Overlapped::zero();
            unsafe {
                t!(s.write_overlapped(&b, a.raw()));
            }
            let status = t!(cp.get(None));
            assert_eq!(status.bytes_transferred(), 3);
            assert_eq!(status.token(), 1);
            assert_eq!(status.overlapped(), a.raw());

            t!(t.join());
        })
    }

    #[test]
    fn tcp_connect() {
        each_ip(&mut |addr_template| {
            let l = t!(TcpListener::bind(addr_template));
            let addr = t!(l.local_addr());
            let t = thread::spawn(move || {
                t!(l.accept());
            });

            let cp = t!(CompletionPort::new(1));
            let domain = match addr {
                SocketAddr::V4(..) => Domain::ipv4(),
                SocketAddr::V6(..) => Domain::ipv6(),
            };
            let socket = t!(Socket::new(domain, Type::stream(), None));
            t!(socket.bind(&addr_template.into()));
            let socket = socket.into_tcp_stream();
            t!(cp.add_socket(1, &socket));

            let a = Overlapped::zero();
            unsafe {
                t!(socket.connect_overlapped(&addr, &[], a.raw()));
            }
            let status = t!(cp.get(None));
            assert_eq!(status.bytes_transferred(), 0);
            assert_eq!(status.token(), 1);
            assert_eq!(status.overlapped(), a.raw());
            t!(socket.connect_complete());

            t!(t.join());
        })
    }

    #[test]
    fn udp_recv_from() {
        each_ip(&mut |addr| {
            let a = t!(UdpSocket::bind(addr));
            let b = t!(UdpSocket::bind(addr));
            let a_addr = t!(a.local_addr());
            let b_addr = t!(b.local_addr());
            let t = thread::spawn(move || {
                t!(a.send_to(&[1, 2, 3], b_addr));
            });

            let cp = t!(CompletionPort::new(1));
            t!(cp.add_socket(1, &b));

            let mut buf = [0; 10];
            let a = Overlapped::zero();
            let mut addr = SocketAddrBuf::new();
            unsafe {
                t!(b.recv_from_overlapped(&mut buf, &mut addr, a.raw()));
            }
            let status = t!(cp.get(None));
            assert_eq!(status.bytes_transferred(), 3);
            assert_eq!(status.token(), 1);
            assert_eq!(status.overlapped(), a.raw());
            assert_eq!(&buf[..3], &[1, 2, 3]);
            assert_eq!(addr.to_socket_addr(), Some(a_addr));

            t!(t.join());
        })
    }

    #[test]
    fn udp_recv() {
        each_ip(&mut |addr| {
            let a = t!(UdpSocket::bind(addr));
            let b = t!(UdpSocket::bind(addr));
            let a_addr = t!(a.local_addr());
            let b_addr = t!(b.local_addr());
            assert!(b.connect(a_addr).is_ok());
            assert!(a.connect(b_addr).is_ok());
            let t = thread::spawn(move || {
                t!(a.send_to(&[1, 2, 3], b_addr));
            });

            let cp = t!(CompletionPort::new(1));
            t!(cp.add_socket(1, &b));

            let mut buf = [0; 10];
            let a = Overlapped::zero();
            unsafe {
                t!(b.recv_overlapped(&mut buf, a.raw()));
            }
            let status = t!(cp.get(None));
            assert_eq!(status.bytes_transferred(), 3);
            assert_eq!(status.token(), 1);
            assert_eq!(status.overlapped(), a.raw());
            assert_eq!(&buf[..3], &[1, 2, 3]);

            t!(t.join());
        })
    }

    #[test]
    fn udp_send_to() {
        each_ip(&mut |addr| {
            let a = t!(UdpSocket::bind(addr));
            let b = t!(UdpSocket::bind(addr));
            let a_addr = t!(a.local_addr());
            let b_addr = t!(b.local_addr());
            let t = thread::spawn(move || {
                let mut b = [0; 100];
                let (n, addr) = t!(a.recv_from(&mut b));
                assert_eq!(n, 3);
                assert_eq!(addr, b_addr);
                assert_eq!(&b[..3], &[1, 2, 3]);
            });

            let cp = t!(CompletionPort::new(1));
            t!(cp.add_socket(1, &b));

            let a = Overlapped::zero();
            unsafe {
                t!(b.send_to_overlapped(&[1, 2, 3], &a_addr, a.raw()));
            }
            let status = t!(cp.get(None));
            assert_eq!(status.bytes_transferred(), 3);
            assert_eq!(status.token(), 1);
            assert_eq!(status.overlapped(), a.raw());

            t!(t.join());
        })
    }

    #[test]
    fn udp_send() {
        each_ip(&mut |addr| {
            let a = t!(UdpSocket::bind(addr));
            let b = t!(UdpSocket::bind(addr));
            let a_addr = t!(a.local_addr());
            let b_addr = t!(b.local_addr());
            assert!(b.connect(a_addr).is_ok());
            assert!(a.connect(b_addr).is_ok());
            let t = thread::spawn(move || {
                let mut b = [0; 100];
                let (n, addr) = t!(a.recv_from(&mut b));
                assert_eq!(n, 3);
                assert_eq!(addr, b_addr);
                assert_eq!(&b[..3], &[1, 2, 3]);
            });

            let cp = t!(CompletionPort::new(1));
            t!(cp.add_socket(1, &b));

            let a = Overlapped::zero();
            unsafe {
                t!(b.send_overlapped(&[1, 2, 3], a.raw()));
            }
            let status = t!(cp.get(None));
            assert_eq!(status.bytes_transferred(), 3);
            assert_eq!(status.token(), 1);
            assert_eq!(status.overlapped(), a.raw());

            t!(t.join());
        })
    }

    #[test]
    fn tcp_accept() {
        each_ip(&mut |addr_template| {
            let l = t!(TcpListener::bind(addr_template));
            let addr = t!(l.local_addr());
            let t = thread::spawn(move || {
                let socket = t!(TcpStream::connect(addr));
                (socket.local_addr().unwrap(), socket.peer_addr().unwrap())
            });

            let cp = t!(CompletionPort::new(1));
            let domain = match addr {
                SocketAddr::V4(..) => Domain::ipv4(),
                SocketAddr::V6(..) => Domain::ipv6(),
            };
            let socket = t!(Socket::new(domain, Type::stream(), None))
                .into_tcp_stream();
            t!(cp.add_socket(1, &l));

            let a = Overlapped::zero();
            let mut addrs = AcceptAddrsBuf::new();
            unsafe {
                t!(l.accept_overlapped(&socket, &mut addrs, a.raw()));
            }
            let status = t!(cp.get(None));
            assert_eq!(status.bytes_transferred(), 0);
            assert_eq!(status.token(), 1);
            assert_eq!(status.overlapped(), a.raw());
            t!(l.accept_complete(&socket));

            let (remote, local) = t!(t.join());
            let addrs = addrs.parse(&l).unwrap();
            assert_eq!(addrs.local(), Some(local));
            assert_eq!(addrs.remote(), Some(remote));
        })
    }
}
