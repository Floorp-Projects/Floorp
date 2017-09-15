extern crate libudev;
extern crate libc;

use std::io;
use std::ptr;
use std::thread;
use std::time::Duration;

use std::os::unix::io::{AsRawFd};

use libc::{c_void,c_int,c_short,c_ulong,timespec};

#[repr(C)]
struct pollfd {
    fd: c_int,
    events: c_short,
    revents: c_short,
}

#[repr(C)]
struct sigset_t {
    __private: c_void
}

#[allow(non_camel_case_types)]
type nfds_t = c_ulong;

const POLLIN: c_short = 0x0001;

extern "C" {
    fn ppoll(fds: *mut pollfd, nfds: nfds_t, timeout_ts: *mut libc::timespec, sigmask: *const sigset_t) -> c_int;
}

fn main() {
    let context = libudev::Context::new().unwrap();
    monitor(&context).unwrap();
}

fn monitor(context: &libudev::Context) -> io::Result<()> {
    let mut monitor = try!(libudev::Monitor::new(&context));

    try!(monitor.match_subsystem_devtype("usb", "usb_device"));
    let mut socket = try!(monitor.listen());

    let mut fds = vec!(pollfd { fd: socket.as_raw_fd(), events: POLLIN, revents: 0 });

    loop {
        let result = unsafe { ppoll((&mut fds[..]).as_mut_ptr(), fds.len() as nfds_t, ptr::null_mut(), ptr::null()) };

        if result < 0 {
            return Err(io::Error::last_os_error());
        }

        let event = match socket.receive_event() {
            Some(evt) => evt,
            None => {
                thread::sleep(Duration::from_millis(10));
                continue;
            }
        };

        println!("{}: {} {} (subsystem={}, sysname={}, devtype={})",
                 event.sequence_number(),
                 event.event_type(),
                 event.syspath().to_str().unwrap_or("---"),
                 event.subsystem().to_str().unwrap_or(""),
                 event.sysname().to_str().unwrap_or(""),
                 event.devtype().map_or("", |s| { s.to_str().unwrap_or("") }));
    }
}
