use coreaudio_sys::*;
use std::fmt;
use std::os::raw::c_void;
use std::ptr;

pub fn audio_object_has_property(id: AudioObjectID, address: &AudioObjectPropertyAddress) -> bool {
    unsafe { AudioObjectHasProperty(id, address) != 0 }
}

pub fn audio_object_get_property_data<T>(
    id: AudioObjectID,
    address: &AudioObjectPropertyAddress,
    size: *mut usize,
    data: *mut T,
) -> OSStatus {
    unsafe {
        AudioObjectGetPropertyData(
            id,
            address,
            0,
            ptr::null(),
            size as *mut UInt32,
            data as *mut c_void,
        )
    }
}

pub fn audio_object_get_property_data_with_qualifier<T, Q>(
    id: AudioObjectID,
    address: &AudioObjectPropertyAddress,
    qualifier_size: usize,
    qualifier_data: *const Q,
    size: *mut usize,
    data: *mut T,
) -> OSStatus {
    unsafe {
        AudioObjectGetPropertyData(
            id,
            address,
            qualifier_size as UInt32,
            qualifier_data as *const c_void,
            size as *mut UInt32,
            data as *mut c_void,
        )
    }
}

pub fn audio_object_get_property_data_size(
    id: AudioObjectID,
    address: &AudioObjectPropertyAddress,
    size: *mut usize,
) -> OSStatus {
    unsafe { AudioObjectGetPropertyDataSize(id, address, 0, ptr::null(), size as *mut UInt32) }
}

pub fn audio_object_get_property_data_size_with_qualifier<Q>(
    id: AudioObjectID,
    address: &AudioObjectPropertyAddress,
    qualifier_size: usize,
    qualifier_data: *const Q,
    size: *mut usize,
) -> OSStatus {
    unsafe {
        AudioObjectGetPropertyDataSize(
            id,
            address,
            qualifier_size as UInt32,
            qualifier_data as *const c_void,
            size as *mut UInt32,
        )
    }
}

pub fn audio_object_set_property_data<T>(
    id: AudioObjectID,
    address: &AudioObjectPropertyAddress,
    size: usize,
    data: *const T,
) -> OSStatus {
    unsafe {
        AudioObjectSetPropertyData(
            id,
            address,
            0,
            ptr::null(),
            size as UInt32,
            data as *const c_void,
        )
    }
}

#[allow(non_camel_case_types)]
pub type audio_object_property_listener_proc =
    extern "C" fn(AudioObjectID, u32, *const AudioObjectPropertyAddress, *mut c_void) -> OSStatus;

pub fn audio_object_add_property_listener<T>(
    id: AudioObjectID,
    address: &AudioObjectPropertyAddress,
    listener: audio_object_property_listener_proc,
    data: *mut T,
) -> OSStatus {
    unsafe { AudioObjectAddPropertyListener(id, address, Some(listener), data as *mut c_void) }
}

pub fn audio_object_remove_property_listener<T>(
    id: AudioObjectID,
    address: &AudioObjectPropertyAddress,
    listener: audio_object_property_listener_proc,
    data: *mut T,
) -> OSStatus {
    unsafe { AudioObjectRemovePropertyListener(id, address, Some(listener), data as *mut c_void) }
}

#[derive(Debug)]
pub struct PropertySelector(AudioObjectPropertySelector);

impl PropertySelector {
    pub fn new(selector: AudioObjectPropertySelector) -> Self {
        Self(selector)
    }
}

impl fmt::Display for PropertySelector {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use coreaudio_sys as sys;
        let s = match self.0 {
            sys::kAudioHardwarePropertyDefaultOutputDevice => {
                "kAudioHardwarePropertyDefaultOutputDevice"
            }
            sys::kAudioHardwarePropertyDefaultInputDevice => {
                "kAudioHardwarePropertyDefaultInputDevice"
            }
            sys::kAudioDevicePropertyDeviceIsAlive => "kAudioDevicePropertyDeviceIsAlive",
            sys::kAudioDevicePropertyDataSource => "kAudioDevicePropertyDataSource",
            _ => "Unknown",
        };
        write!(f, "{}", s)
    }
}
