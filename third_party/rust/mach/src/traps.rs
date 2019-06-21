//! This module corresponds to `mach/mach_traps.h`.

use kern_return::{kern_return_t};
use port::{mach_port_name_t, mach_port_t};

extern "C" {
    pub fn mach_task_self() -> mach_port_t;
    pub fn task_for_pid(target_tport: mach_port_name_t,
                        pid: ::libc::c_int,
                        tn: *mut mach_port_name_t) -> kern_return_t;
}

#[test]
fn mach_task_self_sanity_test() {
    unsafe {
        let this_task = mach_task_self();
        println!("{:?}", this_task);
    }
}
