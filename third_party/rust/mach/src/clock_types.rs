//! This module roughly corresponds to `mach/clock_types.h`.

pub type alarm_type_t = ::libc::c_int;
pub type sleep_type_t = ::libc::c_int;
pub type clock_id_t = ::libc::c_int;
pub type clock_flavor_t = ::libc::c_int;
pub type clock_attr_t = *mut ::libc::c_int;
pub type clock_res_t = ::libc::c_int;

#[repr(C)]
pub struct mach_timespec {
    tv_sec: ::libc::c_uint,
    tv_nsec: clock_res_t
}
pub type mach_timespec_t = mach_timespec;
