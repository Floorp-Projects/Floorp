//! This module corresponds to `bootstrap.h`

use kern_return::{kern_return_t};
use libc;
use port::{mach_port_t};

pub const BOOTSTRAP_MAX_NAME_LEN: libc::c_uint = 128;

extern "C" {
    pub fn bootstrap_look_up(bp: mach_port_t,
                             service_name: *const libc::c_char,
                             sp: *mut mach_port_t)
                             -> kern_return_t;
}
