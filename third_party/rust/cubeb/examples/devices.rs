// Copyright © 2011 Mozilla Foundation
// Copyright © 2015 Haakon Sporsheim <haakon.sporsheim@telenordigital.com>
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

//! libcubeb enumerate device test/example.
//! Prints out a list of input/output devices connected to the system.
extern crate cubeb;

mod common;

use cubeb::{DeviceFormat, DeviceType};

fn print_device_info(info: &cubeb::DeviceInfo) {
    let devtype = if info.device_type().contains(DeviceType::INPUT) {
        "input"
    } else if info.device_type().contains(DeviceType::OUTPUT) {
        "output"
    } else {
        "unknown?"
    };

    let devstate = match info.state() {
        cubeb::DeviceState::Disabled => "disabled",
        cubeb::DeviceState::Unplugged => "unplugged",
        cubeb::DeviceState::Enabled => "enabled",
    };

    let devdeffmt = match info.default_format() {
        DeviceFormat::S16LE => "S16LE",
        DeviceFormat::S16BE => "S16BE",
        DeviceFormat::F32LE => "F32LE",
        DeviceFormat::F32BE => "F32BE",
        _ => "unknown?",
    };

    let mut devfmts = "".to_string();
    if info.format().contains(DeviceFormat::S16LE) {
        devfmts = format!("{} S16LE", devfmts);
    }
    if info.format().contains(DeviceFormat::S16BE) {
        devfmts = format!("{} S16BE", devfmts);
    }
    if info.format().contains(DeviceFormat::F32LE) {
        devfmts = format!("{} F32LE", devfmts);
    }
    if info.format().contains(DeviceFormat::F32BE) {
        devfmts = format!("{} F32BE", devfmts);
    }

    if let Some(device_id) = info.device_id() {
        let preferred = if info.preferred().is_empty() {
            ""
        } else {
            " (PREFERRED)"
        };
        println!("dev: \"{}\"{}", device_id, preferred);
    }
    if let Some(friendly_name) = info.friendly_name() {
        println!("\tName:    \"{}\"", friendly_name);
    }
    if let Some(group_id) = info.group_id() {
        println!("\tGroup:   \"{}\"", group_id);
    }
    if let Some(vendor_name) = info.vendor_name() {
        println!("\tVendor:  \"{}\"", vendor_name);
    }
    println!("\tType:    {}", devtype);
    println!("\tState:   {}", devstate);
    println!("\tCh:      {}", info.max_channels());
    println!(
        "\tFormat:  {} (0x{:x}) (default: {})",
        &devfmts[1..],
        info.format(),
        devdeffmt
    );
    println!(
        "\tRate:    {} - {} (default: {})",
        info.min_rate(),
        info.max_rate(),
        info.default_rate()
    );
    println!(
        "\tLatency: lo {} frames, hi {} frames",
        info.latency_lo(),
        info.latency_hi()
    );
}

fn main() {
    let ctx = common::init("Cubeb audio test").expect("Failed to create cubeb context");

    println!("Enumerating input devices for backend {}", ctx.backend_id());

    let devices = match ctx.enumerate_devices(DeviceType::INPUT) {
        Ok(devices) => devices,
        Err(e) if e.code() == cubeb::ErrorCode::NotSupported => {
            println!("Device enumeration not support for this backend.");
            return;
        }
        Err(e) => {
            println!("Error enumerating devices: {}", e);
            return;
        }
    };

    println!("Found {} input devices", devices.len());
    for d in devices.iter() {
        print_device_info(d);
    }

    println!(
        "Enumerating output devices for backend {}",
        ctx.backend_id()
    );

    let devices = match ctx.enumerate_devices(DeviceType::OUTPUT) {
        Ok(devices) => devices,
        Err(e) => {
            println!("Error enumerating devices: {}", e);
            return;
        }
    };

    println!("Found {} output devices", devices.len());
    for d in devices.iter() {
        print_device_info(d);
    }
}
