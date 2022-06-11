pub mod data;
pub mod parser;
pub mod result;

use io::{BufRead, BufReader};
use libc::{c_int, connect, nfds_t, poll, pollfd, sockaddr_un, socket, AF_UNIX, POLLIN, SOCK_SEQPACKET};
use std::cmp::Ordering;
use std::os::unix::io::{FromRawFd, RawFd};
use std::os::unix::net::UnixStream;
use std::{io, mem, ptr};

pub use data::*;
pub use result::*;

const SOCKET_PATH: &str = "/var/run/devd.seqpacket.pipe";

pub fn parse_devd_event(event: &str) -> Result<Event> {
    match parser::event(event) {
        Ok((_, x)) => Ok(x),
        _ => Err(Error::Parse),
    }
}

#[derive(Debug)]
pub struct Context {
    sock: BufReader<UnixStream>,
    sockfd: RawFd,
    buffer: String,
}

impl Context {
    pub fn new() -> Result<Context> {
        unsafe {
            let sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
            if sockfd < 0 {
                return Err(io::Error::last_os_error().into());
            }
            let mut sockaddr = sockaddr_un { sun_family: AF_UNIX as _, ..mem::zeroed() };
            ptr::copy_nonoverlapping(SOCKET_PATH.as_ptr(), sockaddr.sun_path.as_mut_ptr() as *mut u8, SOCKET_PATH.len());
            if connect(
                sockfd,
                &sockaddr as *const sockaddr_un as *const _,
                (mem::size_of_val(&AF_UNIX) + SOCKET_PATH.len()) as _,
            ) < 0
            {
                return Err(io::Error::last_os_error().into());
            }
            Ok(Context {
                sock: BufReader::new(UnixStream::from_raw_fd(sockfd)),
                sockfd,
                buffer: String::new(),
            })
        }
    }

    pub fn wait_for_event_raw_internal(&mut self, timeout_ms: usize) -> Result<&str> {
        let mut fds = [pollfd { fd: self.sockfd, events: POLLIN, revents: 0 }];
        let x = unsafe { poll((&mut fds).as_mut_ptr(), fds.len() as nfds_t, timeout_ms as c_int) };

        match x.cmp(&0) {
            Ordering::Less => Err(io::Error::last_os_error().into()),
            Ordering::Equal => Err(Error::Timeout),
            Ordering::Greater => {
                self.buffer.clear();
                self.sock.read_line(&mut self.buffer)?;
                Ok(&self.buffer)
            }
        }
    }

    /// Waits for an event using poll(), reads it but does not parse
    pub fn wait_for_event_raw(&mut self, timeout_ms: usize) -> Result<String> {
        self.wait_for_event_raw_internal(timeout_ms).map(ToOwned::to_owned)
    }

    /// Waits for an event using poll(), reads and parses it
    pub fn wait_for_event(&mut self, timeout_ms: usize) -> Result<Event> {
        self.wait_for_event_raw_internal(timeout_ms).and_then(parse_devd_event)
    }

    /// Returns the devd socket file descriptor in case you want to select/poll on it together with
    /// other file descriptors
    pub fn fd(&self) -> RawFd {
        self.sockfd
    }

    /// Reads an event and parses it. Use when polling on the raw fd by yourself
    pub fn read_event(&mut self) -> Result<Event> {
        self.buffer.clear();
        self.sock.read_line(&mut self.buffer)?;
        parse_devd_event(&self.buffer)
    }
}
