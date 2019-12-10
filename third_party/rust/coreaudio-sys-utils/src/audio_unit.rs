use coreaudio_sys::*;
use std::os::raw::c_void;

pub fn audio_unit_get_property_info(
    unit: AudioUnit,
    property: AudioUnitPropertyID,
    scope: AudioUnitScope,
    element: AudioUnitElement,
    size: *mut usize,
    writable: *mut Boolean,
) -> OSStatus {
    assert!(!unit.is_null());
    unsafe {
        AudioUnitGetPropertyInfo(
            unit,
            property,
            scope,
            element,
            size as *mut UInt32,
            writable,
        )
    }
}

pub fn audio_unit_get_property<T>(
    unit: AudioUnit,
    property: AudioUnitPropertyID,
    scope: AudioUnitScope,
    element: AudioUnitElement,
    data: *mut T,
    size: *mut usize,
) -> OSStatus {
    assert!(!unit.is_null());
    unsafe {
        AudioUnitGetProperty(
            unit,
            property,
            scope,
            element,
            data as *mut c_void,
            size as *mut UInt32,
        )
    }
}

pub fn audio_unit_set_property<T>(
    unit: AudioUnit,
    property: AudioUnitPropertyID,
    scope: AudioUnitScope,
    element: AudioUnitElement,
    data: *const T,
    size: usize,
) -> OSStatus {
    assert!(!unit.is_null());
    unsafe {
        AudioUnitSetProperty(
            unit,
            property,
            scope,
            element,
            data as *const c_void,
            size as UInt32,
        )
    }
}

pub fn audio_unit_get_parameter(
    unit: AudioUnit,
    id: AudioUnitParameterID,
    scope: AudioUnitScope,
    element: AudioUnitElement,
    value: &mut AudioUnitParameterValue,
) -> OSStatus {
    assert!(!unit.is_null());
    unsafe {
        AudioUnitGetParameter(
            unit,
            id,
            scope,
            element,
            value as *mut AudioUnitParameterValue,
        )
    }
}

pub fn audio_unit_set_parameter(
    unit: AudioUnit,
    id: AudioUnitParameterID,
    scope: AudioUnitScope,
    element: AudioUnitElement,
    value: AudioUnitParameterValue,
    buffer_offset_in_frames: UInt32,
) -> OSStatus {
    assert!(!unit.is_null());
    unsafe { AudioUnitSetParameter(unit, id, scope, element, value, buffer_offset_in_frames) }
}

pub fn audio_unit_initialize(unit: AudioUnit) -> OSStatus {
    assert!(!unit.is_null());
    unsafe { AudioUnitInitialize(unit) }
}

pub fn audio_unit_uninitialize(unit: AudioUnit) -> OSStatus {
    assert!(!unit.is_null());
    unsafe { AudioUnitUninitialize(unit) }
}

pub fn dispose_audio_unit(unit: AudioUnit) -> OSStatus {
    unsafe { AudioComponentInstanceDispose(unit) }
}

pub fn audio_output_unit_start(unit: AudioUnit) -> OSStatus {
    assert!(!unit.is_null());
    unsafe { AudioOutputUnitStart(unit) }
}

pub fn audio_output_unit_stop(unit: AudioUnit) -> OSStatus {
    assert!(!unit.is_null());
    unsafe { AudioOutputUnitStop(unit) }
}

pub fn audio_unit_render(
    in_unit: AudioUnit,
    io_action_flags: *mut AudioUnitRenderActionFlags,
    in_time_stamp: *const AudioTimeStamp,
    in_output_bus_number: u32,
    in_number_frames: u32,
    io_data: *mut AudioBufferList,
) -> OSStatus {
    assert!(!in_unit.is_null());
    unsafe {
        AudioUnitRender(
            in_unit,
            io_action_flags,
            in_time_stamp,
            in_output_bus_number,
            in_number_frames,
            io_data,
        )
    }
}

#[allow(non_camel_case_types)]
pub type audio_unit_property_listener_proc =
    extern "C" fn(*mut c_void, AudioUnit, AudioUnitPropertyID, AudioUnitScope, AudioUnitElement);

pub fn audio_unit_add_property_listener<T>(
    unit: AudioUnit,
    id: AudioUnitPropertyID,
    listener: audio_unit_property_listener_proc,
    data: *mut T,
) -> OSStatus {
    assert!(!unit.is_null());
    unsafe { AudioUnitAddPropertyListener(unit, id, Some(listener), data as *mut c_void) }
}

pub fn audio_unit_remove_property_listener_with_user_data<T>(
    unit: AudioUnit,
    id: AudioUnitPropertyID,
    listener: audio_unit_property_listener_proc,
    data: *mut T,
) -> OSStatus {
    assert!(!unit.is_null());
    unsafe {
        AudioUnitRemovePropertyListenerWithUserData(unit, id, Some(listener), data as *mut c_void)
    }
}
