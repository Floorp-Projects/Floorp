/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#![cfg_attr(feature = "cargo-clippy", allow(clippy::cast_lossless))]

extern crate libc;

use std::io;
use std::os::unix::io::RawFd;

use super::hidwrapper::{_HIDIOCGRDESC, _HIDIOCGRDESCSIZE};
use crate::consts::MAX_HID_RPT_SIZE;
use crate::transport::hidproto::*;
use crate::util::{from_unix_result, io_err};

#[allow(non_camel_case_types)]
#[repr(C)]
pub struct LinuxReportDescriptor {
    size: ::libc::c_int,
    value: [u8; 4096],
}

const HID_MAX_DESCRIPTOR_SIZE: usize = 4096;

#[cfg(not(target_env = "musl"))]
type IocType = libc::c_ulong;
#[cfg(target_env = "musl")]
type IocType = libc::c_int;

pub unsafe fn hidiocgrdescsize(
    fd: libc::c_int,
    val: *mut ::libc::c_int,
) -> io::Result<libc::c_int> {
    from_unix_result(libc::ioctl(fd, _HIDIOCGRDESCSIZE as IocType, val))
}

pub unsafe fn hidiocgrdesc(
    fd: libc::c_int,
    val: *mut LinuxReportDescriptor,
) -> io::Result<libc::c_int> {
    from_unix_result(libc::ioctl(fd, _HIDIOCGRDESC as IocType, val))
}

pub fn is_u2f_device(fd: RawFd) -> bool {
    match read_report_descriptor(fd) {
        Ok(desc) => has_fido_usage(desc),
        Err(_) => false, // Upon failure, just say it's not a U2F device.
    }
}

pub fn read_hid_rpt_sizes_or_defaults(fd: RawFd) -> (usize, usize) {
    let default_rpt_sizes = (MAX_HID_RPT_SIZE, MAX_HID_RPT_SIZE);
    let desc = read_report_descriptor(fd);
    if let Ok(desc) = desc {
        if let Ok(rpt_sizes) = read_hid_rpt_sizes(desc) {
            rpt_sizes
        } else {
            default_rpt_sizes
        }
    } else {
        default_rpt_sizes
    }
}

fn read_report_descriptor(fd: RawFd) -> io::Result<ReportDescriptor> {
    let mut desc = LinuxReportDescriptor {
        size: 0,
        value: [0; HID_MAX_DESCRIPTOR_SIZE],
    };

    let _ = unsafe { hidiocgrdescsize(fd, &mut desc.size)? };
    if desc.size == 0 || desc.size as usize > desc.value.len() {
        return Err(io_err("unexpected hidiocgrdescsize() result"));
    }

    let _ = unsafe { hidiocgrdesc(fd, &mut desc)? };
    let mut value = Vec::from(&desc.value[..]);
    value.truncate(desc.size as usize);
    Ok(ReportDescriptor { value })
}
