extern crate libudev_sys as udev;
extern crate libc;

use std::ffi::CStr;
use std::str;

use libc::c_char;

fn main() {
    unsafe {
        let udev = udev::udev_new();

        if !udev.is_null() {
            enumerate_devices(udev);
            udev::udev_unref(udev);
        }
    }
}

unsafe fn enumerate_devices(udev: *mut udev::udev) {
    udev::udev_ref(udev);

    let enumerate = udev::udev_enumerate_new(udev);

    if !enumerate.is_null() {
        list_devices(enumerate);
        udev::udev_enumerate_unref(enumerate);
    }

    udev::udev_unref(udev);
}

unsafe fn list_devices(enumerate: *mut udev::udev_enumerate) {
    udev::udev_enumerate_ref(enumerate);

    if udev::udev_enumerate_scan_devices(enumerate) < 0 {
        udev::udev_enumerate_unref(enumerate);
        return;
    }

    print_devices(
        udev::udev_enumerate_get_udev(enumerate),
        udev::udev_enumerate_get_list_entry(enumerate));

    udev::udev_enumerate_unref(enumerate);
}

unsafe fn print_devices(udev: *mut udev::udev, list_entry: *mut udev::udev_list_entry) {
    if list_entry.is_null() {
        return;
    }

    let syspath = udev::udev_list_entry_get_name(list_entry);
    let device = udev::udev_device_new_from_syspath(udev, syspath);

    if !device.is_null() {
        print_device(device);
    }

    print_devices(udev, udev::udev_list_entry_get_next(list_entry));
}

unsafe fn print_device(device: *mut udev::udev_device) {
    udev::udev_device_ref(device);

    println!("");

    let initialized = udev::udev_device_get_is_initialized(device) != 0;
    let since = udev::udev_device_get_usec_since_initialized(device);

    println!("initialized: {} usec={}", initialized, since);
    println!("     devnum: {}", udev::udev_device_get_devnum(device));
    println!("    devpath: {}", unwrap_cstr(udev::udev_device_get_devpath(device)));
    println!("  subsystem: {}", unwrap_cstr(udev::udev_device_get_subsystem(device)));
    println!("    devtype: {}", unwrap_cstr(udev::udev_device_get_devtype(device)));
    println!("    syspath: {}", unwrap_cstr(udev::udev_device_get_syspath(device)));
    println!("    sysname: {}", unwrap_cstr(udev::udev_device_get_sysname(device)));
    println!("     sysnum: {}", unwrap_cstr(udev::udev_device_get_sysnum(device)));
    println!("    devnode: {}", unwrap_cstr(udev::udev_device_get_devnode(device)));
    println!("     driver: {}", unwrap_cstr(udev::udev_device_get_driver(device)));

    println!(" properties:");
    print_device_properties(udev::udev_device_get_properties_list_entry(device), device);

    println!(" attributes:");
    print_device_attributes(udev::udev_device_get_sysattr_list_entry(device), device);

    udev::udev_device_unref(device);
}

unsafe fn print_device_properties(list_entry: *mut udev::udev_list_entry, device: *mut udev::udev_device) {
    if list_entry.is_null() {
        return;
    }

    let propname = udev::udev_list_entry_get_name(list_entry);
    let propval = udev::udev_device_get_property_value(device, propname);

    println!("{:>15}: {}", unwrap_cstr(propname), unwrap_cstr(propval));

    print_device_properties(udev::udev_list_entry_get_next(list_entry), device);
}

unsafe fn print_device_attributes(list_entry: *mut udev::udev_list_entry, device: *mut udev::udev_device) {
    if list_entry.is_null() {
        return;
    }

    let attrname = udev::udev_list_entry_get_name(list_entry);
    let attrval = udev::udev_device_get_sysattr_value(device, attrname);

    println!("{:>15}: {}", unwrap_cstr(attrname), unwrap_cstr(attrval));

    print_device_attributes(udev::udev_list_entry_get_next(list_entry), device);
}

unsafe fn unwrap_cstr<'a>(ptr: *const c_char) -> &'a str {
    if ptr.is_null() {
        ""
    }
    else {
        str::from_utf8(CStr::from_ptr(ptr).to_bytes()).unwrap_or("")
    }
}
