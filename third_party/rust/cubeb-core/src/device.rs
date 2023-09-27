// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use ffi;
use std::str;
use util::opt_bytes;

/// The state of a device.
#[derive(PartialEq, Eq, Clone, Debug, Copy)]
pub enum DeviceState {
    /// The device has been disabled at the system level.
    Disabled,
    /// The device is enabled, but nothing is plugged into it.
    Unplugged,
    /// The device is enabled.
    Enabled,
}

bitflags! {
    /// Architecture specific sample type.
    pub struct DeviceFormat: ffi::cubeb_device_fmt {
        const S16LE = ffi::CUBEB_DEVICE_FMT_S16LE;
        const S16BE = ffi::CUBEB_DEVICE_FMT_S16BE;
        const F32LE = ffi::CUBEB_DEVICE_FMT_F32LE;
        const F32BE = ffi::CUBEB_DEVICE_FMT_F32BE;
    }
}

bitflags! {
    /// Channel type for a `cubeb_stream`. Depending on the backend and platform
    /// used, this can control inter-stream interruption, ducking, and volume
    /// control.
    pub struct DevicePref: ffi::cubeb_device_pref {
        const MULTIMEDIA = ffi::CUBEB_DEVICE_PREF_MULTIMEDIA;
        const VOICE = ffi::CUBEB_DEVICE_PREF_VOICE;
        const NOTIFICATION = ffi::CUBEB_DEVICE_PREF_NOTIFICATION;
        const ALL = ffi::CUBEB_DEVICE_PREF_ALL;
    }
}

impl DevicePref {
    pub const NONE: Self = Self::empty();
}

bitflags! {
    /// Whether a particular device is an input device (e.g. a microphone), or an
    /// output device (e.g. headphones).
    pub struct DeviceType: ffi::cubeb_device_type {
        const INPUT = ffi::CUBEB_DEVICE_TYPE_INPUT as _;
        const OUTPUT = ffi::CUBEB_DEVICE_TYPE_OUTPUT as _;
    }
}

impl DeviceType {
    pub const UNKNOWN: Self = Self::empty();
}

/// An opaque handle used to refer to a particular input or output device
/// across calls.
pub type DeviceId = ffi::cubeb_devid;

ffi_type_heap! {
    /// Audio device description
    type CType = ffi::cubeb_device;
    #[derive(Debug)]
    pub struct Device;
    pub struct DeviceRef;
}

impl DeviceRef {
    fn get_ref(&self) -> &ffi::cubeb_device {
        unsafe { &*self.as_ptr() }
    }

    /// Gets the output device name.
    ///
    /// May return `None` if there is no output device.
    pub fn output_name(&self) -> Option<&str> {
        self.output_name_bytes().map(|b| str::from_utf8(b).unwrap())
    }

    pub fn output_name_bytes(&self) -> Option<&[u8]> {
        unsafe { opt_bytes(self.get_ref().output_name) }
    }

    /// Gets the input device name.
    ///
    /// May return `None` if there is no input device.
    pub fn input_name(&self) -> Option<&str> {
        self.input_name_bytes().map(|b| str::from_utf8(b).unwrap())
    }

    pub fn input_name_bytes(&self) -> Option<&[u8]> {
        unsafe { opt_bytes(self.get_ref().input_name) }
    }
}

ffi_type_stack! {
    /// This structure holds the characteristics of an input or output
    /// audio device. It is obtained using `enumerate_devices`, which
    /// returns these structures via `device_collection` and must be
    /// destroyed via `device_collection_destroy`.
    type CType = ffi::cubeb_device_info;
    pub struct DeviceInfo;
    pub struct DeviceInfoRef;
}

impl DeviceInfoRef {
    fn get_ref(&self) -> &ffi::cubeb_device_info {
        unsafe { &*self.as_ptr() }
    }

    /// Device identifier handle.
    pub fn devid(&self) -> DeviceId {
        self.get_ref().devid
    }

    /// Device identifier which might be presented in a UI.
    pub fn device_id(&self) -> Option<&str> {
        self.device_id_bytes().map(|b| str::from_utf8(b).unwrap())
    }

    pub fn device_id_bytes(&self) -> Option<&[u8]> {
        unsafe { opt_bytes(self.get_ref().device_id) }
    }

    /// Friendly device name which might be presented in a UI.
    pub fn friendly_name(&self) -> Option<&str> {
        self.friendly_name_bytes()
            .map(|b| str::from_utf8(b).unwrap())
    }

    pub fn friendly_name_bytes(&self) -> Option<&[u8]> {
        unsafe { opt_bytes(self.get_ref().friendly_name) }
    }

    /// Two devices have the same group identifier if they belong to
    /// the same physical device; for example a headset and
    /// microphone.
    pub fn group_id(&self) -> Option<&str> {
        self.group_id_bytes().map(|b| str::from_utf8(b).unwrap())
    }

    pub fn group_id_bytes(&self) -> Option<&[u8]> {
        unsafe { opt_bytes(self.get_ref().group_id) }
    }

    /// Optional vendor name, may be None.
    pub fn vendor_name(&self) -> Option<&str> {
        self.vendor_name_bytes().map(|b| str::from_utf8(b).unwrap())
    }

    pub fn vendor_name_bytes(&self) -> Option<&[u8]> {
        unsafe { opt_bytes(self.get_ref().vendor_name) }
    }

    /// Type of device (Input/Output).
    pub fn device_type(&self) -> DeviceType {
        DeviceType::from_bits_truncate(self.get_ref().device_type)
    }

    /// State of device disabled/enabled/unplugged.
    pub fn state(&self) -> DeviceState {
        let state = self.get_ref().state;
        macro_rules! check( ($($raw:ident => $real:ident),*) => (
            $(if state == ffi::$raw {
                DeviceState::$real
            }) else *
            else {
                panic!("unknown device state: {}", state)
            }
        ));

        check!(CUBEB_DEVICE_STATE_DISABLED => Disabled,
               CUBEB_DEVICE_STATE_UNPLUGGED => Unplugged,
               CUBEB_DEVICE_STATE_ENABLED => Enabled)
    }

    /// Preferred device.
    pub fn preferred(&self) -> DevicePref {
        DevicePref::from_bits(self.get_ref().preferred).unwrap()
    }

    /// Sample format supported.
    pub fn format(&self) -> DeviceFormat {
        DeviceFormat::from_bits(self.get_ref().format).unwrap()
    }

    /// The default sample format for this device.
    pub fn default_format(&self) -> DeviceFormat {
        DeviceFormat::from_bits(self.get_ref().default_format).unwrap()
    }

    /// Channels.
    pub fn max_channels(&self) -> u32 {
        self.get_ref().max_channels
    }

    /// Default/Preferred sample rate.
    pub fn default_rate(&self) -> u32 {
        self.get_ref().default_rate
    }

    /// Maximum sample rate supported.
    pub fn max_rate(&self) -> u32 {
        self.get_ref().max_rate
    }

    /// Minimum sample rate supported.
    pub fn min_rate(&self) -> u32 {
        self.get_ref().min_rate
    }

    /// Lowest possible latency in frames.
    pub fn latency_lo(&self) -> u32 {
        self.get_ref().latency_lo
    }

    /// Higest possible latency in frames.
    pub fn latency_hi(&self) -> u32 {
        self.get_ref().latency_hi
    }
}

#[cfg(test)]
mod tests {
    use ffi::cubeb_device;
    use Device;

    #[test]
    fn device_device_ref_same_ptr() {
        let ptr: *mut cubeb_device = 0xDEAD_BEEF as *mut _;
        let device = unsafe { Device::from_ptr(ptr) };
        assert_eq!(device.as_ptr(), ptr);
        assert_eq!(device.as_ptr(), device.as_ref().as_ptr());
    }
}
