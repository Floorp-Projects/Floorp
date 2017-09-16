extern crate libudev_sys as udev;
extern crate libc;

use std::str;

use std::ffi::{CString,CStr};

use self::libc::{c_int,c_short,c_char};


#[cfg(target_os = "linux")]
#[allow(non_camel_case_types)]
type nfds_t = libc::c_ulong;

#[cfg(not(target_os = "linux"))]
#[allow(non_camel_case_types)]
type nfds_t = libc::c_uint;

#[repr(C)]
struct PollFd {
  fd: c_int,
  events: c_short,
  revents: c_short
}

const POLLIN: c_short = 0x0001;

extern "C" {
  fn poll(fds: *mut PollFd, nfds: nfds_t, timeout: c_int) -> c_int;
}


fn main() {
    let netlink = CString::new("udev").unwrap();

    unsafe {
        let udev = udev::udev_new();

        if !udev.is_null() {
            let monitor = udev::udev_monitor_new_from_netlink(udev, netlink.as_ptr());

            if !monitor.is_null() {
                listen(monitor);
                udev::udev_monitor_unref(monitor);
            }

            udev::udev_unref(udev);
        }
    }
}

unsafe fn listen(monitor: *mut udev::udev_monitor) {
    udev::udev_monitor_ref(monitor);

    udev::udev_monitor_enable_receiving(monitor);

    let fd = udev::udev_monitor_get_fd(monitor);
    let mut fds = vec!(PollFd { fd: fd, events: POLLIN, revents: 0 });

    while poll((&mut fds[..]).as_mut_ptr(), fds.len() as nfds_t, -1) > 0 {
        if fds[0].revents & POLLIN != 0 {
            let device = udev::udev_monitor_receive_device(monitor);
            print_device(device);
            udev::udev_device_unref(device);
        }
    }

    udev::udev_monitor_unref(monitor);
}

unsafe fn print_device(device: *mut udev::udev_device) {
    let seqnum = udev::udev_device_get_seqnum(device);
    let action = unwrap_cstr(udev::udev_device_get_action(device));
    let devpath = unwrap_cstr(udev::udev_device_get_devpath(device));
    let initialized = udev::udev_device_get_is_initialized(device) != 0;
    let since = udev::udev_device_get_usec_since_initialized(device);

    println!("{}: [{}] {} (intialized={} usec={})", seqnum, action, devpath, initialized, since);
}

unsafe fn unwrap_cstr<'a>(ptr: *const c_char) -> &'a str {
    if ptr.is_null() {
        ""
    }
    else {
        str::from_utf8(CStr::from_ptr(ptr).to_bytes()).unwrap_or("")
    }
}
