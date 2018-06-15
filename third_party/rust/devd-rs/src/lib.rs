extern crate libc;
#[macro_use]
extern crate nom;

pub mod result;
pub mod data;
pub mod parser;

use libc::{
    c_int, nfds_t,
    poll, pollfd, POLLIN,
    socket, connect, sockaddr_un, AF_UNIX, SOCK_SEQPACKET
};
use std::os::unix::io::{FromRawFd, RawFd};
use std::os::unix::net::UnixStream;
use std::{io, mem, ptr};
use io::{BufRead, BufReader};

pub use result::*;
pub use data::*;

const SOCKET_PATH: &'static str = "/var/run/devd.seqpacket.pipe";

pub fn parse_devd_event(e: String) -> Result<Event> {
    match parser::event(e.as_bytes()) {
        parser::IResult::Done(_, x) => Ok(x),
        _ => Err(Error::Parse),
    }
}

#[derive(Debug)]
pub struct Context {
    sock: BufReader<UnixStream>,
    sockfd: RawFd,
}

impl Context {
    pub fn new() -> Result<Context> {
        unsafe {
            let sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
            if sockfd < 0 {
                return Err(io::Error::last_os_error().into());
            }
            let mut sockaddr = sockaddr_un {
                sun_family: AF_UNIX as _,
                .. mem::zeroed()
            };
            ptr::copy_nonoverlapping(
                SOCKET_PATH.as_ptr(),
                sockaddr.sun_path.as_mut_ptr() as *mut u8,
                SOCKET_PATH.len());
            if connect(
                sockfd,
                &sockaddr as *const sockaddr_un as *const _,
                (mem::size_of_val(&AF_UNIX) + SOCKET_PATH.len()) as _) < 0 {
                return Err(io::Error::last_os_error().into());
            }
            Ok(Context {
                sock: BufReader::new(UnixStream::from_raw_fd(sockfd)),
                sockfd: sockfd,
            })
        }
    }

    /// Waits for an event using poll(), reads it but does not parse
    pub fn wait_for_event_raw(&mut self, timeout_ms: usize) -> Result<String> {
        let mut fds = vec![pollfd { fd: self.sockfd, events: POLLIN, revents: 0 }];
        let x = unsafe { poll((&mut fds).as_mut_ptr(), fds.len() as nfds_t, timeout_ms as c_int) };
        if x < 0 {
            Err(io::Error::last_os_error().into())
        } else if x == 0 {
            Err(Error::Timeout)
        } else {
            let mut s = String::new();
            let _ = self.sock.read_line(&mut s);
            Ok(s)
        }
    }

    /// Waits for an event using poll(), reads and parses it
    pub fn wait_for_event<'a>(&mut self, timeout_ms: usize) -> Result<Event> {
        self.wait_for_event_raw(timeout_ms)
            .and_then(parse_devd_event)
    }

    /// Returns the devd socket file descriptor in case you want to select/poll on it together with
    /// other file descriptors
    pub fn fd(&self) -> RawFd {
        self.sockfd
    }

    /// Reads an event and parses it. Use when polling on the raw fd by yourself
    pub fn read_event(&mut self) -> Result<Event> {
        let mut s = String::new();
        let _ = self.sock.read_line(&mut s);
        parse_devd_event(s)
    }
}
