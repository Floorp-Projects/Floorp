/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;

use std::io;
use std::mem;
use std::os::raw::c_int;
use std::os::raw::c_uchar;

use crate::transport::hidproto::has_fido_usage;
use crate::transport::hidproto::ReportDescriptor;
use crate::transport::platform::fd::Fd;
use crate::util::io_err;

/* sys/ioccom.h */

const IOCPARM_MASK: u32 = 0x1fff;
const IOCPARM_SHIFT: u32 = 16;
const IOCGROUP_SHIFT: u32 = 8;

//const IOC_VOID: u32 = 0x20000000;
const IOC_OUT: u32 = 0x40000000;
const IOC_IN: u32 = 0x80000000;
//const IOC_INOUT: u32 = IOC_IN|IOC_OUT;

macro_rules! ioctl {
    ($dir:expr, $name:ident, $group:expr, $nr:expr, $ty:ty) => {
        unsafe fn $name(fd: libc::c_int, val: *mut $ty) -> io::Result<libc::c_int> {
            let ioc = ($dir as u32)
                | ((mem::size_of::<$ty>() as u32 & IOCPARM_MASK) << IOCPARM_SHIFT)
                | (($group as u32) << IOCGROUP_SHIFT)
                | ($nr as u32);
            let rv = libc::ioctl(fd, ioc as libc::c_ulong, val);
            if rv == -1 {
                return Err(io::Error::last_os_error());
            }
            Ok(rv)
        }
    };
}

#[allow(non_camel_case_types)]
#[repr(C)]
struct usb_ctl_report_desc {
    ucrd_size: c_int,
    ucrd_data: [c_uchar; 1024],
}

ioctl!(IOC_OUT, usb_get_report_desc, b'U', 21, usb_ctl_report_desc);

fn read_report_descriptor(fd: &Fd) -> io::Result<ReportDescriptor> {
    let mut desc = unsafe { mem::zeroed() };
    unsafe { usb_get_report_desc(fd.fileno, &mut desc) }?;
    if desc.ucrd_size < 0 {
        return Err(io_err("negative report descriptor size"));
    }
    let size = desc.ucrd_size as usize;
    let value = Vec::from(&desc.ucrd_data[..size]);
    Ok(ReportDescriptor { value })
}

pub fn is_u2f_device(fd: &Fd) -> bool {
    match read_report_descriptor(fd) {
        Ok(desc) => has_fido_usage(desc),
        Err(_) => false,
    }
}

ioctl!(IOC_IN, usb_hid_set_raw_ioctl, b'h', 2, c_int);

pub fn hid_set_raw(fd: &Fd, raw: bool) -> io::Result<()> {
    let mut raw_int: c_int = if raw { 1 } else { 0 };
    unsafe { usb_hid_set_raw_ioctl(fd.fileno, &mut raw_int) }?;
    Ok(())
}
