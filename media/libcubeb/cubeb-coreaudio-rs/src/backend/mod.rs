// Copyright © 2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.
#![allow(unused_assignments)]
#![allow(unused_must_use)]

extern crate coreaudio_sys_utils;
extern crate libc;

mod auto_array;
mod auto_release;
mod owned_critical_section;
mod property_address;
mod utils;

use self::auto_array::*;
use self::auto_release::*;
use self::coreaudio_sys_utils::aggregate_device::*;
use self::coreaudio_sys_utils::audio_object::*;
use self::coreaudio_sys_utils::audio_unit::*;
use self::coreaudio_sys_utils::cf_mutable_dict::*;
use self::coreaudio_sys_utils::dispatch::*;
use self::coreaudio_sys_utils::string::*;
use self::coreaudio_sys_utils::sys::*;
use self::owned_critical_section::*;
use self::property_address::*;
use self::utils::*;
use atomic;
use cubeb_backend::{
    ffi, ChannelLayout, Context, ContextOps, DeviceCollectionRef, DeviceId, DeviceRef, DeviceType,
    Error, Ops, Result, SampleFormat, State, Stream, StreamOps, StreamParams, StreamParamsRef,
    StreamPrefs,
};
use std::cmp;
use std::ffi::{CStr, CString};
use std::fmt;
use std::mem;
use std::os::raw::{c_char, c_void};
use std::ptr;
use std::slice;
use std::sync::atomic::{AtomicBool, AtomicI64, AtomicU32, AtomicU64, Ordering};
use std::sync::{Arc, Condvar, Mutex};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

const NO_ERR: OSStatus = 0;

const AU_OUT_BUS: AudioUnitElement = 0;
const AU_IN_BUS: AudioUnitElement = 1;

const DISPATCH_QUEUE_LABEL: &str = "org.mozilla.cubeb";
const PRIVATE_AGGREGATE_DEVICE_NAME: &str = "CubebAggregateDevice";

// Testing empirically, some headsets report a minimal latency that is very low,
// but this does not work in practice. Lie and say the minimum is 256 frames.
const SAFE_MIN_LATENCY_FRAMES: u32 = 256;
const SAFE_MAX_LATENCY_FRAMES: u32 = 512;

bitflags! {
    #[allow(non_camel_case_types)]
    struct device_flags: u32 {
        const DEV_UNKNOWN           = 0b0000_0000; // Unknown
        const DEV_INPUT             = 0b0000_0001; // Record device like mic
        const DEV_OUTPUT            = 0b0000_0010; // Playback device like speakers
        const DEV_SYSTEM_DEFAULT    = 0b0000_0100; // System default device
        const DEV_SELECTED_DEFAULT  = 0b0000_1000; // User selected to use the system default device
    }
}

#[allow(non_camel_case_types)]
#[derive(Debug, PartialEq)]
enum io_side {
    INPUT,
    OUTPUT,
}

impl fmt::Display for io_side {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let side = match self {
            io_side::INPUT => "input",
            io_side::OUTPUT => "output",
        };
        write!(f, "{}", side)
    }
}

fn make_sized_audio_channel_layout(sz: usize) -> AutoRelease<AudioChannelLayout> {
    assert!(sz >= mem::size_of::<AudioChannelLayout>());
    assert_eq!(
        (sz - mem::size_of::<AudioChannelLayout>()) % mem::size_of::<AudioChannelDescription>(),
        0
    );
    let acl = unsafe { libc::calloc(1, sz) } as *mut AudioChannelLayout;

    unsafe extern "C" fn free_acl(acl: *mut AudioChannelLayout) {
        libc::free(acl as *mut libc::c_void);
    }

    AutoRelease::new(acl, free_acl)
}

#[allow(non_camel_case_types)]
#[derive(Clone, Debug)]
struct device_info {
    id: AudioDeviceID,
    flags: device_flags,
}

impl Default for device_info {
    fn default() -> Self {
        Self {
            id: kAudioObjectUnknown,
            flags: device_flags::DEV_UNKNOWN,
        }
    }
}

#[allow(non_camel_case_types)]
#[derive(Debug)]
struct device_property_listener {
    device: AudioDeviceID,
    property: &'static AudioObjectPropertyAddress,
    listener: audio_object_property_listener_proc,
}

impl device_property_listener {
    fn new(
        device: AudioDeviceID,
        property: &'static AudioObjectPropertyAddress,
        listener: audio_object_property_listener_proc,
    ) -> Self {
        Self {
            device,
            property,
            listener,
        }
    }
}

#[derive(Debug, PartialEq)]
struct CAChannelLabel(AudioChannelLabel);

impl CAChannelLabel {
    fn get_raw_label(&self) -> AudioChannelLabel {
        self.0
    }
}

impl From<ChannelLayout> for CAChannelLabel {
    fn from(layout: ChannelLayout) -> Self {
        // Make sure the layout is a channel (only one bit set to 1)
        assert_eq!(layout.bits() & (layout.bits() - 1), 0);
        let channel = match layout {
            ChannelLayout::FRONT_LEFT => kAudioChannelLabel_Left,
            ChannelLayout::FRONT_RIGHT => kAudioChannelLabel_Right,
            ChannelLayout::FRONT_CENTER => kAudioChannelLabel_Center,
            ChannelLayout::LOW_FREQUENCY => kAudioChannelLabel_LFEScreen,
            ChannelLayout::BACK_LEFT => kAudioChannelLabel_LeftSurround,
            ChannelLayout::BACK_RIGHT => kAudioChannelLabel_RightSurround,
            ChannelLayout::FRONT_LEFT_OF_CENTER => kAudioChannelLabel_LeftCenter,
            ChannelLayout::FRONT_RIGHT_OF_CENTER => kAudioChannelLabel_RightCenter,
            ChannelLayout::BACK_CENTER => kAudioChannelLabel_CenterSurround,
            ChannelLayout::SIDE_LEFT => kAudioChannelLabel_LeftSurroundDirect,
            ChannelLayout::SIDE_RIGHT => kAudioChannelLabel_RightSurroundDirect,
            ChannelLayout::TOP_CENTER => kAudioChannelLabel_TopCenterSurround,
            ChannelLayout::TOP_FRONT_LEFT => kAudioChannelLabel_VerticalHeightLeft,
            ChannelLayout::TOP_FRONT_CENTER => kAudioChannelLabel_VerticalHeightCenter,
            ChannelLayout::TOP_FRONT_RIGHT => kAudioChannelLabel_VerticalHeightRight,
            ChannelLayout::TOP_BACK_LEFT => kAudioChannelLabel_TopBackLeft,
            ChannelLayout::TOP_BACK_CENTER => kAudioChannelLabel_TopBackCenter,
            ChannelLayout::TOP_BACK_RIGHT => kAudioChannelLabel_TopBackRight,
            _ => kAudioChannelLabel_Unknown,
        };
        Self(channel)
    }
}

impl Into<ChannelLayout> for CAChannelLabel {
    fn into(self) -> ChannelLayout {
        use self::coreaudio_sys_utils::sys;
        match self.0 {
            sys::kAudioChannelLabel_Left => ChannelLayout::FRONT_LEFT,
            sys::kAudioChannelLabel_Right => ChannelLayout::FRONT_RIGHT,
            sys::kAudioChannelLabel_Center => ChannelLayout::FRONT_CENTER,
            sys::kAudioChannelLabel_LFEScreen => ChannelLayout::LOW_FREQUENCY,
            sys::kAudioChannelLabel_LeftSurround => ChannelLayout::BACK_LEFT,
            sys::kAudioChannelLabel_RightSurround => ChannelLayout::BACK_RIGHT,
            sys::kAudioChannelLabel_LeftCenter => ChannelLayout::FRONT_LEFT_OF_CENTER,
            sys::kAudioChannelLabel_RightCenter => ChannelLayout::FRONT_RIGHT_OF_CENTER,
            sys::kAudioChannelLabel_CenterSurround => ChannelLayout::BACK_CENTER,
            sys::kAudioChannelLabel_LeftSurroundDirect => ChannelLayout::SIDE_LEFT,
            sys::kAudioChannelLabel_RightSurroundDirect => ChannelLayout::SIDE_RIGHT,
            sys::kAudioChannelLabel_TopCenterSurround => ChannelLayout::TOP_CENTER,
            sys::kAudioChannelLabel_VerticalHeightLeft => ChannelLayout::TOP_FRONT_LEFT,
            sys::kAudioChannelLabel_VerticalHeightCenter => ChannelLayout::TOP_FRONT_CENTER,
            sys::kAudioChannelLabel_VerticalHeightRight => ChannelLayout::TOP_FRONT_RIGHT,
            sys::kAudioChannelLabel_TopBackLeft => ChannelLayout::TOP_BACK_LEFT,
            sys::kAudioChannelLabel_TopBackCenter => ChannelLayout::TOP_BACK_CENTER,
            sys::kAudioChannelLabel_TopBackRight => ChannelLayout::TOP_BACK_RIGHT,
            _ => ChannelLayout::UNDEFINED,
        }
    }
}

fn set_notification_runloop() {
    let address = AudioObjectPropertyAddress {
        mSelector: kAudioHardwarePropertyRunLoop,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMaster,
    };

    // Ask HAL to manage its own thread for notification by setting the run_loop to NULL.
    // Otherwise HAL may use main thread to fire notifications.
    let run_loop: CFRunLoopRef = ptr::null_mut();
    let size = mem::size_of::<CFRunLoopRef>();
    let status =
        audio_object_set_property_data(kAudioObjectSystemObject, &address, size, &run_loop);
    if status != NO_ERR {
        cubeb_log!("Could not make global CoreAudio notifications use their own thread.");
    }
}

fn clamp_latency(latency_frames: u32) -> u32 {
    cmp::max(
        cmp::min(latency_frames, SAFE_MAX_LATENCY_FRAMES),
        SAFE_MIN_LATENCY_FRAMES,
    )
}

fn create_device_info(id: AudioDeviceID, devtype: DeviceType) -> Result<device_info> {
    assert_ne!(id, kAudioObjectSystemObject);

    let mut info = device_info {
        id: id,
        flags: match devtype {
            DeviceType::INPUT => device_flags::DEV_INPUT,
            DeviceType::OUTPUT => device_flags::DEV_OUTPUT,
            _ => panic!("Only accept input or output type"),
        },
    };

    let default_device_id = audiounit_get_default_device_id(devtype);
    if default_device_id == kAudioObjectUnknown {
        return Err(Error::error());
    }

    if id == kAudioObjectUnknown {
        info.id = default_device_id;
        info.flags |= device_flags::DEV_SELECTED_DEFAULT;
    }

    if info.id == default_device_id {
        info.flags |= device_flags::DEV_SYSTEM_DEFAULT;
    }

    Ok(info)
}

fn create_stream_description(stream_params: &StreamParams) -> Result<AudioStreamBasicDescription> {
    assert!(stream_params.rate() > 0);
    assert!(stream_params.channels() > 0);

    let mut desc = AudioStreamBasicDescription::default();

    match stream_params.format() {
        SampleFormat::S16LE => {
            desc.mBitsPerChannel = 16;
            desc.mFormatFlags = kAudioFormatFlagIsSignedInteger;
        }
        SampleFormat::S16BE => {
            desc.mBitsPerChannel = 16;
            desc.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian;
        }
        SampleFormat::Float32LE => {
            desc.mBitsPerChannel = 32;
            desc.mFormatFlags = kAudioFormatFlagIsFloat;
        }
        SampleFormat::Float32BE => {
            desc.mBitsPerChannel = 32;
            desc.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsBigEndian;
        }
        _ => {
            return Err(Error::invalid_format());
        }
    }

    desc.mFormatID = kAudioFormatLinearPCM;
    desc.mFormatFlags |= kLinearPCMFormatFlagIsPacked;
    desc.mSampleRate = f64::from(stream_params.rate());
    desc.mChannelsPerFrame = stream_params.channels();

    desc.mBytesPerFrame = (desc.mBitsPerChannel / 8) * desc.mChannelsPerFrame;
    desc.mFramesPerPacket = 1;
    desc.mBytesPerPacket = desc.mBytesPerFrame * desc.mFramesPerPacket;

    desc.mReserved = 0;

    Ok(desc)
}

fn create_auto_array(
    desc: AudioStreamBasicDescription,
    latency_frames: u32,
    capacity: usize,
) -> Result<Box<AutoArrayWrapper>> {
    assert_ne!(desc.mFormatFlags, 0);
    assert_ne!(desc.mChannelsPerFrame, 0);
    assert_ne!(latency_frames, 0);
    assert!(!contains_bits(
        desc.mFormatFlags,
        kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsFloat
    ));

    let size = (latency_frames * desc.mChannelsPerFrame) as usize * capacity;

    if desc.mFormatFlags & kAudioFormatFlagIsSignedInteger != 0 {
        return Ok(Box::new(AutoArrayImpl::<i16>::new(size)));
    }

    if desc.mFormatFlags & kAudioFormatFlagIsFloat != 0 {
        return Ok(Box::new(AutoArrayImpl::<f32>::new(size)));
    }

    fn contains_bits(mask: AudioFormatFlags, bits: AudioFormatFlags) -> bool {
        mask & bits == bits
    }

    Err(Error::invalid_format())
}

fn set_volume(unit: AudioUnit, volume: f32) -> Result<()> {
    assert!(!unit.is_null());
    let r = audio_unit_set_parameter(
        unit,
        kHALOutputParam_Volume,
        kAudioUnitScope_Global,
        0,
        volume,
        0,
    );
    if r == NO_ERR {
        Ok(())
    } else {
        cubeb_log!("AudioUnitSetParameter/kHALOutputParam_Volume rv={}", r);
        Err(Error::error())
    }
}

fn get_volume(unit: AudioUnit) -> Result<f32> {
    assert!(!unit.is_null());
    let mut volume: f32 = 0.0;
    let r = audio_unit_get_parameter(
        unit,
        kHALOutputParam_Volume,
        kAudioUnitScope_Global,
        0,
        &mut volume,
    );
    if r == NO_ERR {
        Ok(volume)
    } else {
        cubeb_log!("AudioUnitGetParameter/kHALOutputParam_Volume rv={}", r);
        Err(Error::error())
    }
}

fn audiounit_make_silent(io_data: &mut AudioBuffer) {
    assert!(!io_data.mData.is_null());
    let bytes = unsafe {
        let ptr = io_data.mData as *mut u8;
        let len = io_data.mDataByteSize as usize;
        slice::from_raw_parts_mut(ptr, len)
    };
    for data in bytes.iter_mut() {
        *data = 0;
    }
}

extern "C" fn audiounit_input_callback(
    user_ptr: *mut c_void,
    flags: *mut AudioUnitRenderActionFlags,
    tstamp: *const AudioTimeStamp,
    bus: u32,
    input_frames: u32,
    _: *mut AudioBufferList,
) -> OSStatus {
    let stm = unsafe { &mut *(user_ptr as *mut AudioUnitStream) };

    assert!(!stm.input_unit.is_null());
    assert_eq!(bus, AU_IN_BUS);

    if stm.shutdown.load(Ordering::SeqCst) {
        cubeb_log!("({:p}) input shutdown", stm as *const AudioUnitStream);
        return NO_ERR;
    }

    let r = stm.render_input(flags, tstamp, bus, input_frames);
    if r != NO_ERR {
        return r;
    }

    // Full Duplex. We'll call data_callback in the AudioUnit output callback.
    if !stm.output_unit.is_null() {
        return NO_ERR;
    }

    // Input only. Call the user callback through resampler.
    // Resampler will deliver input buffer in the correct rate.
    assert!(
        input_frames as usize
            <= stm.input_linear_buffer.as_ref().unwrap().elements()
                / stm.input_desc.mChannelsPerFrame as usize
    );
    let mut total_input_frames = (stm.input_linear_buffer.as_ref().unwrap().elements()
        / stm.input_desc.mChannelsPerFrame as usize) as i64;
    assert!(!stm.input_linear_buffer.as_ref().unwrap().as_ptr().is_null());
    let outframes = unsafe {
        ffi::cubeb_resampler_fill(
            stm.resampler.as_mut(),
            stm.input_linear_buffer.as_mut().unwrap().as_mut_ptr(),
            &mut total_input_frames,
            ptr::null_mut(),
            0,
        )
    };
    if outframes < total_input_frames {
        assert_eq!(audio_output_unit_stop(stm.input_unit), NO_ERR);

        stm.notify_state_changed(State::Drained);

        return NO_ERR;
    }

    // Reset input buffer
    stm.input_linear_buffer.as_mut().unwrap().clear();

    NO_ERR
}

extern "C" fn audiounit_output_callback(
    user_ptr: *mut c_void,
    _: *mut AudioUnitRenderActionFlags,
    _tstamp: *const AudioTimeStamp,
    bus: u32,
    output_frames: u32,
    out_buffer_list: *mut AudioBufferList,
) -> OSStatus {
    assert_eq!(bus, AU_OUT_BUS);
    assert!(!out_buffer_list.is_null());
    let out_buffer_list_ref = unsafe { &mut (*out_buffer_list) };

    assert_eq!(out_buffer_list_ref.mNumberBuffers, 1);

    let stm = unsafe { &mut *(user_ptr as *mut AudioUnitStream) };
    let buffers = unsafe {
        let ptr = out_buffer_list_ref.mBuffers.as_mut_ptr();
        let len = out_buffer_list_ref.mNumberBuffers as usize;
        slice::from_raw_parts_mut(ptr, len)
    };

    cubeb_logv!(
        "({:p}) output: buffers {}, size {}, channels {}, frames {}, total input frames {}.",
        stm as *const AudioUnitStream,
        buffers.len(),
        buffers[0].mDataByteSize,
        buffers[0].mNumberChannels,
        output_frames,
        if stm.input_linear_buffer.is_some() {
            stm.input_linear_buffer.as_ref().unwrap().elements()
                / stm.input_desc.mChannelsPerFrame as usize
        } else {
            0
        }
    );

    let mut input_frames: i64 = 0;
    let mut output_buffer = ptr::null_mut::<c_void>();
    let mut input_buffer = ptr::null_mut::<c_void>();

    if stm.shutdown.load(Ordering::SeqCst) {
        cubeb_log!("({:p}) output shutdown.", stm as *const AudioUnitStream);
        audiounit_make_silent(&mut buffers[0]);
        return NO_ERR;
    }

    if stm.draining.load(Ordering::SeqCst) {
        assert_eq!(audio_output_unit_stop(stm.output_unit), NO_ERR);
        if !stm.input_unit.is_null() {
            assert_eq!(audio_output_unit_stop(stm.input_unit), NO_ERR);
        }
        stm.notify_state_changed(State::Drained);
        audiounit_make_silent(&mut buffers[0]);
        return NO_ERR;
    }

    // Get output buffer
    if stm.mixer.as_ptr().is_null() {
        output_buffer = buffers[0].mData;
    } else {
        // If remixing needs to occur, we can't directly work in our final
        // destination buffer as data may be overwritten or too small to start with.
        let size_needed = (output_frames * stm.output_stream_params.channels()) as usize
            * cubeb_sample_size(stm.output_stream_params.format());
        if stm.temp_buffer_size < size_needed {
            stm.temp_buffer = allocate_array_by_size(size_needed);
            assert_eq!(stm.temp_buffer.len() * mem::size_of::<u8>(), size_needed);
            stm.temp_buffer_size = size_needed;
        }
        output_buffer = stm.temp_buffer.as_mut_ptr() as *mut c_void;
    }

    stm.frames_written
        .fetch_add(i64::from(output_frames), Ordering::SeqCst);

    // If Full duplex get also input buffer
    if !stm.input_unit.is_null() {
        // If the output callback came first and this is a duplex stream, we need to
        // fill in some additional silence in the resampler.
        // Otherwise, if we had more than expected callbacks in a row, or we're
        // currently switching, we add some silence as well to compensate for the
        // fact that we're lacking some input data.
        let frames_written = stm.frames_written.load(Ordering::SeqCst);
        let input_frames_needed = stm.minimum_resampling_input_frames(frames_written);
        let missing_frames = input_frames_needed - stm.frames_read.load(Ordering::SeqCst);
        if missing_frames > 0 {
            stm.input_linear_buffer.as_mut().unwrap().push_zeros(
                (missing_frames * i64::from(stm.input_desc.mChannelsPerFrame)) as usize,
            );
            stm.frames_read.store(input_frames_needed, Ordering::SeqCst);
            let stm_ptr = stm as *const AudioUnitStream;
            cubeb_log!(
                "({:p}) {} pushed {} frames of input silence.",
                stm_ptr,
                if stm.frames_read.load(Ordering::SeqCst) == 0 {
                    "Input hasn't started,"
                } else if stm.switching_device.load(Ordering::SeqCst) {
                    "Device switching,"
                } else {
                    "Drop out,"
                },
                missing_frames
            );
        }
        input_buffer = stm.input_linear_buffer.as_mut().unwrap().as_mut_ptr();
        // Number of input frames in the buffer. It will change to actually used frames
        // inside fill
        assert_ne!(stm.input_desc.mChannelsPerFrame, 0);
        input_frames = (stm.input_linear_buffer.as_ref().unwrap().elements()
            / stm.input_desc.mChannelsPerFrame as usize) as i64;
    }

    // Call user callback through resampler.
    assert!(!output_buffer.is_null());
    let outframes = unsafe {
        ffi::cubeb_resampler_fill(
            stm.resampler.as_mut(),
            input_buffer,
            if input_buffer.is_null() {
                ptr::null_mut()
            } else {
                &mut input_frames
            },
            output_buffer,
            i64::from(output_frames),
        )
    };

    if !input_buffer.is_null() {
        // Pop from the buffer the frames used by the the resampler.
        stm.input_linear_buffer
            .as_mut()
            .unwrap()
            .pop(input_frames as usize * stm.input_desc.mChannelsPerFrame as usize);
    }

    if outframes < 0 || outframes > i64::from(output_frames) {
        *stm.shutdown.get_mut() = true;
        assert_eq!(audio_output_unit_stop(stm.output_unit), NO_ERR);
        if !stm.input_unit.is_null() {
            assert_eq!(audio_output_unit_stop(stm.input_unit), NO_ERR);
        }
        stm.notify_state_changed(State::Error);
        audiounit_make_silent(&mut buffers[0]);
        return NO_ERR;
    }

    *stm.draining.get_mut() = outframes < i64::from(output_frames);
    stm.frames_played
        .store(stm.frames_queued, atomic::Ordering::SeqCst);
    stm.frames_queued += outframes as u64;

    let outaff = stm.output_desc.mFormatFlags;
    let panning = if stm.output_desc.mChannelsPerFrame == 2 {
        stm.panning.load(Ordering::Relaxed)
    } else {
        0.0
    };

    // Post process output samples.
    if stm.draining.load(Ordering::SeqCst) {
        // Clear missing frames (silence)
        let count_bytes = |frames: usize| -> usize {
            let sample_size = cubeb_sample_size(stm.output_stream_params.format());
            frames * sample_size / mem::size_of::<u8>()
        };
        let out_bytes = unsafe {
            slice::from_raw_parts_mut(
                output_buffer as *mut u8,
                count_bytes(output_frames as usize),
            )
        };
        let start = count_bytes(outframes as usize);
        for i in start..out_bytes.len() {
            out_bytes[i] = 0;
        }
    }

    // Mixing
    if stm.mixer.as_ptr().is_null() {
        // Pan stereo.
        if panning != 0.0 {
            unsafe {
                if outaff & kAudioFormatFlagIsFloat != 0 {
                    ffi::cubeb_pan_stereo_buffer_float(
                        output_buffer as *mut f32,
                        outframes as u32,
                        panning,
                    );
                } else if outaff & kAudioFormatFlagIsSignedInteger != 0 {
                    ffi::cubeb_pan_stereo_buffer_int(
                        output_buffer as *mut i16,
                        outframes as u32,
                        panning,
                    );
                }
            }
        }
    } else {
        assert!(!stm.temp_buffer.is_empty());
        assert_eq!(
            stm.temp_buffer_size,
            stm.temp_buffer.len() * mem::size_of::<u8>()
        );
        assert_eq!(output_buffer, stm.temp_buffer.as_mut_ptr() as *mut c_void);
        let temp_buffer_size = stm.temp_buffer_size;
        stm.mix_output_buffer(
            output_frames as usize,
            output_buffer,
            temp_buffer_size,
            buffers[0].mData,
            buffers[0].mDataByteSize as usize,
        );
    }

    NO_ERR
}

extern "C" fn audiounit_property_listener_callback(
    id: AudioObjectID,
    address_count: u32,
    addresses: *const AudioObjectPropertyAddress,
    user: *mut c_void,
) -> OSStatus {
    use self::coreaudio_sys_utils::sys;

    let stm = unsafe { &mut *(user as *mut AudioUnitStream) };
    let addrs = unsafe { slice::from_raw_parts(addresses, address_count as usize) };
    let property_selector = PropertySelector::new(addrs[0].mSelector);
    if stm.switching_device.load(Ordering::SeqCst) {
        cubeb_log!(
            "Switching is already taking place. Skip Event {} for id={}",
            property_selector,
            id
        );
        return NO_ERR;
    }
    *stm.switching_device.get_mut() = true;

    cubeb_log!(
        "({:p}) Audio device changed, {} events.",
        stm as *const AudioUnitStream,
        address_count
    );
    for (i, addr) in addrs.iter().enumerate() {
        match addr.mSelector {
            sys::kAudioHardwarePropertyDefaultOutputDevice => {
                cubeb_log!(
                    "Event[{}] - mSelector == kAudioHardwarePropertyDefaultOutputDevice for id={}",
                    i,
                    id
                );
            }
            sys::kAudioHardwarePropertyDefaultInputDevice => {
                cubeb_log!(
                    "Event[{}] - mSelector == kAudioHardwarePropertyDefaultInputDevice for id={}",
                    i,
                    id
                );
            }
            sys::kAudioDevicePropertyDeviceIsAlive => {
                cubeb_log!(
                    "Event[{}] - mSelector == kAudioDevicePropertyDeviceIsAlive for id={}",
                    i,
                    id
                );
                // If this is the default input device ignore the event,
                // kAudioHardwarePropertyDefaultInputDevice will take care of the switch
                if stm
                    .input_device
                    .flags
                    .contains(device_flags::DEV_SYSTEM_DEFAULT)
                {
                    cubeb_log!("It's the default input device, ignore the event");
                    *stm.switching_device.get_mut() = false;
                    return NO_ERR;
                }
            }
            sys::kAudioDevicePropertyDataSource => {
                cubeb_log!(
                    "Event[{}] - mSelector == kAudioDevicePropertyDataSource for id={}",
                    i,
                    id
                );
            }
            _ => {
                cubeb_log!(
                    "Event[{}] - mSelector == Unexpected Event id {}, return",
                    i,
                    addr.mSelector
                );
                *stm.switching_device.get_mut() = false;
                return NO_ERR;
            }
        }
    }

    for _addr in addrs.iter() {
        let _dev_cb_lock = AutoLock::new(&mut stm.device_changed_callback_lock);
        if let Some(device_changed_callback) = stm.device_changed_callback {
            unsafe {
                device_changed_callback(stm.user_ptr);
            }
        }
    }

    stm.reinit_async();

    NO_ERR
}

fn audiounit_get_acceptable_latency_range() -> Result<AudioValueRange> {
    let output_device_buffer_size_range = AudioObjectPropertyAddress {
        mSelector: kAudioDevicePropertyBufferFrameSizeRange,
        mScope: kAudioDevicePropertyScopeOutput,
        mElement: kAudioObjectPropertyElementMaster,
    };

    let output_device_id = audiounit_get_default_device_id(DeviceType::OUTPUT);
    if output_device_id == kAudioObjectUnknown {
        cubeb_log!("Could not get default output device id.");
        return Err(Error::error());
    }

    // Get the buffer size range this device supports
    let mut range = AudioValueRange::default();
    let mut size = mem::size_of::<AudioValueRange>();
    let r = audio_object_get_property_data(
        output_device_id,
        &output_device_buffer_size_range,
        &mut size,
        &mut range,
    );
    if r != NO_ERR {
        cubeb_log!("AudioObjectGetPropertyData/buffer size range rv={}", r);
        return Err(Error::error());
    }

    Ok(range)
}

fn audiounit_get_default_device_id(devtype: DeviceType) -> AudioObjectID {
    assert!(devtype == DeviceType::INPUT || devtype == DeviceType::OUTPUT);

    let adr = if devtype == DeviceType::OUTPUT {
        &DEFAULT_OUTPUT_DEVICE_PROPERTY_ADDRESS
    } else {
        &DEFAULT_INPUT_DEVICE_PROPERTY_ADDRESS
    };

    let mut devid: AudioDeviceID = kAudioObjectUnknown;
    let mut size = mem::size_of::<AudioDeviceID>();
    if audio_object_get_property_data(kAudioObjectSystemObject, adr, &mut size, &mut devid)
        != NO_ERR
    {
        return kAudioObjectUnknown;
    }

    devid
}

fn audiounit_convert_channel_layout(layout: &AudioChannelLayout) -> ChannelLayout {
    // When having one or two channel, force mono or stereo. Some devices (namely,
    // Bose QC35, mark 1 and 2), expose a single channel mapped to the right for
    // some reason.
    if layout.mNumberChannelDescriptions == 1 {
        return ChannelLayout::MONO;
    } else if layout.mNumberChannelDescriptions == 2 {
        return ChannelLayout::STEREO;
    }

    if layout.mChannelLayoutTag != kAudioChannelLayoutTag_UseChannelDescriptions {
        // kAudioChannelLayoutTag_UseChannelBitmap
        // kAudioChannelLayoutTag_Mono
        // kAudioChannelLayoutTag_Stereo
        // ....
        cubeb_log!("Only handle UseChannelDescriptions for now.\n");
        return ChannelLayout::UNDEFINED;
    }

    let channel_descriptions = unsafe {
        slice::from_raw_parts(
            layout.mChannelDescriptions.as_ptr(),
            layout.mNumberChannelDescriptions as usize,
        )
    };

    let mut cl = ChannelLayout::UNDEFINED;
    for description in channel_descriptions {
        let label = CAChannelLabel(description.mChannelLabel);
        let channel: ChannelLayout = label.into();
        if channel == ChannelLayout::UNDEFINED {
            return ChannelLayout::UNDEFINED;
        }
        cl |= channel;
    }

    cl
}

fn audiounit_get_preferred_channel_layout(output_unit: AudioUnit) -> ChannelLayout {
    let mut rv = NO_ERR;
    let mut size: usize = 0;
    rv = audio_unit_get_property_info(
        output_unit,
        kAudioDevicePropertyPreferredChannelLayout,
        kAudioUnitScope_Output,
        AU_OUT_BUS,
        &mut size,
        ptr::null_mut(),
    );
    if rv != NO_ERR {
        cubeb_log!(
            "AudioUnitGetPropertyInfo/kAudioDevicePropertyPreferredChannelLayout rv={}",
            rv
        );
        return ChannelLayout::UNDEFINED;
    }
    assert!(size > 0);

    let mut layout = make_sized_audio_channel_layout(size);
    rv = audio_unit_get_property(
        output_unit,
        kAudioDevicePropertyPreferredChannelLayout,
        kAudioUnitScope_Output,
        AU_OUT_BUS,
        layout.as_mut(),
        &mut size,
    );
    if rv != NO_ERR {
        cubeb_log!(
            "AudioUnitGetProperty/kAudioDevicePropertyPreferredChannelLayout rv={}",
            rv
        );
        return ChannelLayout::UNDEFINED;
    }

    audiounit_convert_channel_layout(layout.as_ref())
}

fn audiounit_get_current_channel_layout(output_unit: AudioUnit) -> ChannelLayout {
    let mut rv = NO_ERR;
    let mut size: usize = 0;
    rv = audio_unit_get_property_info(
        output_unit,
        kAudioUnitProperty_AudioChannelLayout,
        kAudioUnitScope_Output,
        AU_OUT_BUS,
        &mut size,
        ptr::null_mut(),
    );
    if rv != NO_ERR {
        cubeb_log!(
            "AudioUnitGetPropertyInfo/kAudioUnitProperty_AudioChannelLayout rv={}",
            rv
        );
        // This property isn't known before macOS 10.12, attempt another method.
        return audiounit_get_preferred_channel_layout(output_unit);
    }
    assert!(size > 0);

    let mut layout = make_sized_audio_channel_layout(size);
    rv = audio_unit_get_property(
        output_unit,
        kAudioUnitProperty_AudioChannelLayout,
        kAudioUnitScope_Output,
        AU_OUT_BUS,
        layout.as_mut(),
        &mut size,
    );
    if rv != NO_ERR {
        cubeb_log!(
            "AudioUnitGetProperty/kAudioUnitProperty_AudioChannelLayout rv={}",
            rv
        );
        return ChannelLayout::UNDEFINED;
    }

    audiounit_convert_channel_layout(layout.as_ref())
}

fn audiounit_set_channel_layout(
    unit: AudioUnit,
    side: io_side,
    layout: ChannelLayout,
) -> Result<()> {
    assert!(!unit.is_null());

    if side != io_side::OUTPUT {
        return Err(Error::error());
    }

    if layout == ChannelLayout::UNDEFINED {
        // We leave everything as-is...
        return Ok(());
    }

    let mut r = NO_ERR;
    let nb_channels = unsafe { ffi::cubeb_channel_layout_nb_channels(layout.into()) };

    // We do not use CoreAudio standard layout for lack of documentation on what
    // the actual channel orders are. So we set a custom layout.
    assert!(nb_channels >= 1);
    let size = mem::size_of::<AudioChannelLayout>()
        + (nb_channels as usize - 1) * mem::size_of::<AudioChannelDescription>();
    let mut au_layout = make_sized_audio_channel_layout(size);
    au_layout.as_mut().mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
    au_layout.as_mut().mNumberChannelDescriptions = nb_channels;
    let channel_descriptions = unsafe {
        slice::from_raw_parts_mut(
            au_layout.as_mut().mChannelDescriptions.as_mut_ptr(),
            nb_channels as usize,
        )
    };

    let mut channels: usize = 0;
    let mut channel_map: ffi::cubeb_channel_layout = layout.into();
    let i = 0;
    while channel_map != 0 {
        assert!(channels < nb_channels as usize);
        let channel = (channel_map & 1) << i;
        if channel != 0 {
            let layout = ChannelLayout::from(channel);
            let label = CAChannelLabel::from(layout);
            channel_descriptions[channels].mChannelLabel = label.get_raw_label();
            channel_descriptions[channels].mChannelFlags = kAudioChannelFlags_AllOff;
            channels += 1;
        }
        channel_map >>= 1;
    }

    r = audio_unit_set_property(
        unit,
        kAudioUnitProperty_AudioChannelLayout,
        kAudioUnitScope_Input,
        AU_OUT_BUS,
        au_layout.as_ref(),
        size,
    );
    if r != NO_ERR {
        cubeb_log!(
            "AudioUnitSetProperty/{}/kAudioUnitProperty_AudioChannelLayout rv={}",
            side.to_string(),
            r
        );
        return Err(Error::error());
    }

    Ok(())
}

fn audiounit_get_sub_devices(device_id: AudioDeviceID) -> Vec<AudioObjectID> {
    assert_ne!(device_id, kAudioObjectUnknown);

    let mut sub_devices = Vec::new();
    let property_address = AudioObjectPropertyAddress {
        mSelector: kAudioAggregateDevicePropertyActiveSubDeviceList,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMaster,
    };
    let mut size: usize = 0;
    let rv = audio_object_get_property_data_size(device_id, &property_address, &mut size);

    if rv != NO_ERR {
        sub_devices.push(device_id);
        return sub_devices;
    }

    assert_ne!(size, 0);

    let count = size / mem::size_of::<AudioObjectID>();
    sub_devices = allocate_array(count);
    let rv = audio_object_get_property_data(
        device_id,
        &property_address,
        &mut size,
        sub_devices.as_mut_ptr(),
    );

    if rv != NO_ERR {
        sub_devices.clear();
        sub_devices.push(device_id);
    } else {
        cubeb_log!("Found {} sub-devices", count);
    }
    sub_devices
}

fn audiounit_create_blank_aggregate_device(
    plugin_id: &mut AudioObjectID,
    aggregate_device_id: &mut AudioDeviceID,
) -> Result<()> {
    let address_plugin_bundle_id = AudioObjectPropertyAddress {
        mSelector: kAudioHardwarePropertyPlugInForBundleID,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMaster,
    };

    let mut size: usize = 0;
    let mut r = audio_object_get_property_data_size(
        kAudioObjectSystemObject,
        &address_plugin_bundle_id,
        &mut size,
    );
    if r != NO_ERR {
        cubeb_log!(
            "AudioObjectGetPropertyDataSize/kAudioHardwarePropertyPlugInForBundleID, rv={}",
            r
        );
        return Err(Error::error());
    }
    assert_ne!(size, 0);

    let mut in_bundle_ref = cfstringref_from_static_string("com.apple.audio.CoreAudio");
    let mut translation_value = AudioValueTranslation {
        mInputData: &mut in_bundle_ref as *mut CFStringRef as *mut c_void,
        mInputDataSize: mem::size_of_val(&in_bundle_ref) as u32,
        mOutputData: plugin_id as *mut AudioObjectID as *mut c_void,
        mOutputDataSize: mem::size_of_val(plugin_id) as u32,
    };

    r = audio_object_get_property_data(
        kAudioObjectSystemObject,
        &address_plugin_bundle_id,
        &mut size,
        &mut translation_value,
    );
    if r != NO_ERR {
        cubeb_log!(
            "AudioObjectGetPropertyData/kAudioHardwarePropertyPlugInForBundleID, rv={}",
            r
        );
        return Err(Error::error());
    }
    assert_ne!(*plugin_id, kAudioObjectUnknown);

    let create_aggregate_device_address = AudioObjectPropertyAddress {
        mSelector: kAudioPlugInCreateAggregateDevice,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMaster,
    };

    r = audio_object_get_property_data_size(
        *plugin_id,
        &create_aggregate_device_address,
        &mut size,
    );
    if r != NO_ERR {
        cubeb_log!(
            "AudioObjectGetPropertyDataSize/kAudioPlugInCreateAggregateDevice, rv={}",
            r
        );
        return Err(Error::error());
    }
    assert_ne!(size, 0);

    let sys_time = SystemTime::now();
    let time_id = sys_time.duration_since(UNIX_EPOCH).unwrap().as_nanos();
    let device_name = format!("{}_{}", PRIVATE_AGGREGATE_DEVICE_NAME, time_id);
    let device_uid = format!("org.mozilla.{}", device_name);

    unsafe {
        let aggregate_device_dict = CFMutableDictRef::default();

        let aggregate_device_name = cfstringref_from_string(&device_name);
        aggregate_device_dict.add_value(
            cfstringref_from_static_string(AGGREGATE_DEVICE_NAME_KEY),
            aggregate_device_name,
        );
        CFRelease(aggregate_device_name as *const c_void);

        let aggregate_device_uid = cfstringref_from_string(&device_uid);
        aggregate_device_dict.add_value(
            cfstringref_from_static_string(AGGREGATE_DEVICE_UID_KEY),
            aggregate_device_uid,
        );
        CFRelease(aggregate_device_uid as *const c_void);

        let private_value: i32 = 1;
        let aggregate_device_private_key = CFNumberCreate(
            kCFAllocatorDefault,
            i64::from(kCFNumberIntType),
            &private_value as *const i32 as *const c_void,
        );
        aggregate_device_dict.add_value(
            cfstringref_from_static_string(AGGREGATE_DEVICE_PRIVATE_KEY),
            aggregate_device_private_key,
        );
        CFRelease(aggregate_device_private_key as *const c_void);

        let stacked_value: i32 = 0;
        let aggregate_device_stacked_key = CFNumberCreate(
            kCFAllocatorDefault,
            i64::from(kCFNumberIntType),
            &stacked_value as *const i32 as *const c_void,
        );
        aggregate_device_dict.add_value(
            cfstringref_from_static_string(AGGREGATE_DEVICE_STACKED_KEY),
            aggregate_device_stacked_key,
        );
        CFRelease(aggregate_device_stacked_key as *const c_void);

        // This call will fire `audiounit_collection_changed_callback` indirectly!
        r = audio_object_get_property_data_with_qualifier(
            *plugin_id,
            &create_aggregate_device_address,
            mem::size_of_val(&aggregate_device_dict),
            &aggregate_device_dict,
            &mut size,
            aggregate_device_id,
        );
        if r != NO_ERR {
            cubeb_log!(
                "AudioObjectGetPropertyData/kAudioPlugInCreateAggregateDevice, rv={}",
                r
            );
            return Err(Error::error());
        }
        assert_ne!(*aggregate_device_id, kAudioObjectUnknown);
        cubeb_log!("New aggregate device {}", *aggregate_device_id);
    }

    Ok(())
}

fn audiounit_create_blank_aggregate_device_sync(
    plugin_id: &mut AudioObjectID,
    aggregate_device_id: &mut AudioDeviceID,
) -> Result<()> {
    let waiting_time = Duration::new(5, 0);

    let condvar_pair = Arc::new((Mutex::new(Vec::<AudioObjectID>::new()), Condvar::new()));
    let mut cloned_condvar_pair = condvar_pair.clone();
    let data_ptr = &mut cloned_condvar_pair;

    assert_eq!(
        audio_object_add_property_listener(
            kAudioObjectSystemObject,
            &DEVICES_PROPERTY_ADDRESS,
            devices_changed_callback,
            data_ptr,
        ),
        NO_ERR
    );

    let _teardown = finally(|| {
        assert_eq!(
            audio_object_remove_property_listener(
                kAudioObjectSystemObject,
                &DEVICES_PROPERTY_ADDRESS,
                devices_changed_callback,
                data_ptr,
            ),
            NO_ERR
        );
    });

    audiounit_create_blank_aggregate_device(plugin_id, aggregate_device_id)?;

    // Wait until the aggregate is created.
    let &(ref lock, ref cvar) = &*condvar_pair;
    let devices = lock.lock().unwrap();
    if !devices.contains(aggregate_device_id) {
        let (devs, timeout_res) = cvar.wait_timeout(devices, waiting_time).unwrap();
        if timeout_res.timed_out() {
            cubeb_log!(
                "Time out for waiting the creation of aggregate device {}!",
                aggregate_device_id
            );
        }
        if !devs.contains(aggregate_device_id) {
            return Err(Error::device_unavailable());
        }
    }

    extern "C" fn devices_changed_callback(
        id: AudioObjectID,
        _number_of_addresses: u32,
        _addresses: *const AudioObjectPropertyAddress,
        data: *mut c_void,
    ) -> OSStatus {
        assert_eq!(id, kAudioObjectSystemObject);
        let pair = unsafe { &mut *(data as *mut Arc<(Mutex<Vec<AudioObjectID>>, Condvar)>) };
        let &(ref lock, ref cvar) = &**pair;
        let mut devices = lock.lock().unwrap();
        *devices = audiounit_get_devices();
        cvar.notify_one();
        NO_ERR
    }

    Ok(())
}

fn get_device_name(id: AudioDeviceID) -> CFStringRef {
    let mut size = mem::size_of::<CFStringRef>();
    let mut uiname: CFStringRef = ptr::null();
    let address_uuid = AudioObjectPropertyAddress {
        mSelector: kAudioDevicePropertyDeviceUID,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMaster,
    };
    let err = audio_object_get_property_data(id, &address_uuid, &mut size, &mut uiname);
    if err == NO_ERR {
        uiname
    } else {
        ptr::null()
    }
}

fn audiounit_set_aggregate_sub_device_list(
    aggregate_device_id: AudioDeviceID,
    input_device_id: AudioDeviceID,
    output_device_id: AudioDeviceID,
) -> Result<()> {
    assert_ne!(aggregate_device_id, kAudioObjectUnknown);
    assert_ne!(input_device_id, kAudioObjectUnknown);
    assert_ne!(output_device_id, kAudioObjectUnknown);
    assert_ne!(input_device_id, output_device_id);

    cubeb_log!(
        "Add devices input {} and output {} into aggregate device {}",
        input_device_id,
        output_device_id,
        aggregate_device_id
    );
    let output_sub_devices = audiounit_get_sub_devices(output_device_id);
    let input_sub_devices = audiounit_get_sub_devices(input_device_id);

    unsafe {
        let aggregate_sub_devices_array =
            CFArrayCreateMutable(ptr::null(), 0, &kCFTypeArrayCallBacks);
        // The order of the items in the array is significant and is used to determine the order of the streams
        // of the AudioAggregateDevice.
        for device in output_sub_devices {
            let strref = get_device_name(device);
            if strref.is_null() {
                CFRelease(aggregate_sub_devices_array as *const c_void);
                return Err(Error::error());
            }
            CFArrayAppendValue(aggregate_sub_devices_array, strref as *const c_void);
            CFRelease(strref as *const c_void);
        }

        for device in input_sub_devices {
            let strref = get_device_name(device);
            if strref.is_null() {
                CFRelease(aggregate_sub_devices_array as *const c_void);
                return Err(Error::error());
            }
            CFArrayAppendValue(aggregate_sub_devices_array, strref as *const c_void);
            CFRelease(strref as *const c_void);
        }

        let aggregate_sub_device_list = AudioObjectPropertyAddress {
            mSelector: kAudioAggregateDevicePropertyFullSubDeviceList,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMaster,
        };

        let size = mem::size_of::<CFMutableArrayRef>();
        let rv = audio_object_set_property_data(
            aggregate_device_id,
            &aggregate_sub_device_list,
            size,
            &aggregate_sub_devices_array,
        );
        CFRelease(aggregate_sub_devices_array as *const c_void);
        if rv != NO_ERR {
            cubeb_log!(
                "AudioObjectSetPropertyData/kAudioAggregateDevicePropertyFullSubDeviceList, rv={}",
                rv
            );
            return Err(Error::error());
        }
    }

    Ok(())
}

fn audiounit_set_aggregate_sub_device_list_sync(
    aggregate_device_id: AudioDeviceID,
    input_device_id: AudioDeviceID,
    output_device_id: AudioDeviceID,
) -> Result<()> {
    let address = AudioObjectPropertyAddress {
        mSelector: kAudioAggregateDevicePropertyFullSubDeviceList,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMaster,
    };

    let waiting_time = Duration::new(5, 0);

    let condvar_pair = Arc::new((Mutex::new(AudioObjectID::default()), Condvar::new()));
    let mut cloned_condvar_pair = condvar_pair.clone();
    let data_ptr = &mut cloned_condvar_pair;

    assert_eq!(
        audio_object_add_property_listener(
            aggregate_device_id,
            &address,
            devices_changed_callback,
            data_ptr,
        ),
        NO_ERR
    );

    let _teardown = finally(|| {
        assert_eq!(
            audio_object_remove_property_listener(
                aggregate_device_id,
                &address,
                devices_changed_callback,
                data_ptr,
            ),
            NO_ERR
        );
    });

    audiounit_set_aggregate_sub_device_list(
        aggregate_device_id,
        input_device_id,
        output_device_id,
    )?;

    // Wait until the sub devices are added.
    let &(ref lock, ref cvar) = &*condvar_pair;
    let device = lock.lock().unwrap();
    if *device != aggregate_device_id {
        let (dev, timeout_res) = cvar.wait_timeout(device, waiting_time).unwrap();
        if timeout_res.timed_out() {
            cubeb_log!(
                "Time out for waiting the devices({}, {}) adding!",
                input_device_id,
                output_device_id
            );
        }
        if *dev != aggregate_device_id {
            return Err(Error::device_unavailable());
        }
    }

    extern "C" fn devices_changed_callback(
        id: AudioObjectID,
        _number_of_addresses: u32,
        _addresses: *const AudioObjectPropertyAddress,
        data: *mut c_void,
    ) -> OSStatus {
        let pair = unsafe { &mut *(data as *mut Arc<(Mutex<AudioObjectID>, Condvar)>) };
        let &(ref lock, ref cvar) = &**pair;
        let mut device = lock.lock().unwrap();
        *device = id;
        cvar.notify_one();
        NO_ERR
    }

    Ok(())
}

fn audiounit_set_master_aggregate_device(aggregate_device_id: AudioDeviceID) -> Result<()> {
    assert_ne!(aggregate_device_id, kAudioObjectUnknown);
    let master_aggregate_sub_device = AudioObjectPropertyAddress {
        mSelector: kAudioAggregateDevicePropertyMasterSubDevice,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMaster,
    };

    // Master become the 1st output sub device
    let output_device_id = audiounit_get_default_device_id(DeviceType::OUTPUT);
    assert_ne!(output_device_id, kAudioObjectUnknown);
    let output_sub_devices = audiounit_get_sub_devices(output_device_id);
    assert!(!output_sub_devices.is_empty());
    let master_sub_device = get_device_name(output_sub_devices[0]);
    let size = mem::size_of::<CFStringRef>();
    let rv = audio_object_set_property_data(
        aggregate_device_id,
        &master_aggregate_sub_device,
        size,
        &master_sub_device,
    );
    if !master_sub_device.is_null() {
        unsafe {
            CFRelease(master_sub_device as *const c_void);
        }
    }
    if rv != NO_ERR {
        cubeb_log!(
            "AudioObjectSetPropertyData/kAudioAggregateDevicePropertyMasterSubDevice, rv={}",
            rv
        );
        return Err(Error::error());
    }
    Ok(())
}

fn audiounit_activate_clock_drift_compensation(aggregate_device_id: AudioDeviceID) -> Result<()> {
    assert_ne!(aggregate_device_id, kAudioObjectUnknown);
    let address_owned = AudioObjectPropertyAddress {
        mSelector: kAudioObjectPropertyOwnedObjects,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMaster,
    };

    let qualifier_data_size = mem::size_of::<AudioObjectID>();
    let class_id: AudioClassID = kAudioSubDeviceClassID;
    let qualifier_data = &class_id;
    let mut size: usize = 0;

    let mut rv = audio_object_get_property_data_size_with_qualifier(
        aggregate_device_id,
        &address_owned,
        qualifier_data_size,
        qualifier_data,
        &mut size,
    );

    if rv != NO_ERR {
        cubeb_log!(
            "AudioObjectGetPropertyDataSize/kAudioObjectPropertyOwnedObjects, rv={}",
            rv
        );
        return Err(Error::error());
    }

    assert!(
        size > 0,
        "The sub devices of the aggregate device have not been added yet."
    );

    let subdevices_num = size / mem::size_of::<AudioObjectID>();
    let mut sub_devices: Vec<AudioObjectID> = allocate_array(subdevices_num);

    rv = audio_object_get_property_data_with_qualifier(
        aggregate_device_id,
        &address_owned,
        qualifier_data_size,
        qualifier_data,
        &mut size,
        sub_devices.as_mut_ptr(),
    );

    if rv != NO_ERR {
        cubeb_log!(
            "AudioObjectGetPropertyData/kAudioObjectPropertyOwnedObjects, rv={}",
            rv
        );
        return Err(Error::error());
    }

    let address_drift = AudioObjectPropertyAddress {
        mSelector: kAudioSubDevicePropertyDriftCompensation,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMaster,
    };

    assert!(
        subdevices_num >= 2,
        "We should have at least 2 devices for the aggregate device."
    );
    // Start from the second device since the first is the master clock
    for device in &sub_devices[1..] {
        let drift_compensation_value: u32 = 1;
        rv = audio_object_set_property_data(
            *device,
            &address_drift,
            mem::size_of::<u32>(),
            &drift_compensation_value,
        );
        if rv != NO_ERR {
            cubeb_log!(
                "AudioObjectSetPropertyData/kAudioSubDevicePropertyDriftCompensation, rv={}",
                rv
            );
            return Ok(());
        }
    }

    Ok(())
}

fn audiounit_destroy_aggregate_device(
    plugin_id: AudioObjectID,
    aggregate_device_id: &mut AudioDeviceID,
) -> Result<()> {
    assert_ne!(plugin_id, kAudioObjectUnknown);
    assert_ne!(*aggregate_device_id, kAudioObjectUnknown);

    let destroy_aggregate_device_addr = AudioObjectPropertyAddress {
        mSelector: kAudioPlugInDestroyAggregateDevice,
        mScope: kAudioObjectPropertyScopeGlobal,
        mElement: kAudioObjectPropertyElementMaster,
    };

    let mut size: usize = 0;
    let mut rv =
        audio_object_get_property_data_size(plugin_id, &destroy_aggregate_device_addr, &mut size);
    if rv != NO_ERR {
        cubeb_log!(
            "AudioObjectGetPropertyDataSize/kAudioPlugInDestroyAggregateDevice, rv={}",
            rv
        );
        return Err(Error::error());
    }

    assert!(size > 0);

    rv = audio_object_get_property_data(
        plugin_id,
        &destroy_aggregate_device_addr,
        &mut size,
        aggregate_device_id,
    );
    if rv != NO_ERR {
        cubeb_log!(
            "AudioObjectGetPropertyData/kAudioPlugInDestroyAggregateDevice, rv={}",
            rv
        );
        return Err(Error::error());
    }

    cubeb_log!("Destroyed aggregate device {}", *aggregate_device_id);
    *aggregate_device_id = kAudioObjectUnknown;

    Ok(())
}

fn create_audiounit(device: &device_info) -> Result<AudioUnit> {
    assert!(device
        .flags
        .intersects(device_flags::DEV_INPUT | device_flags::DEV_OUTPUT));
    assert!(!device
        .flags
        .contains(device_flags::DEV_INPUT | device_flags::DEV_OUTPUT));

    let unit = create_default_audiounit(device.flags)?;
    if device
        .flags
        .contains(device_flags::DEV_SYSTEM_DEFAULT | device_flags::DEV_OUTPUT)
    {
        return Ok(unit);
    }

    if device.flags.contains(device_flags::DEV_INPUT) {
        // Input only.
        enable_audiounit_scope(unit, io_side::INPUT, true).map_err(|e| {
            cubeb_log!("Fail to enable audiounit input scope. Error: {}", e);
            Error::error()
        })?;
        enable_audiounit_scope(unit, io_side::OUTPUT, false).map_err(|e| {
            cubeb_log!("Fail to disable audiounit output scope. Error: {}", e);
            Error::error()
        })?;
    }

    if device.flags.contains(device_flags::DEV_OUTPUT) {
        // Output only.
        enable_audiounit_scope(unit, io_side::OUTPUT, true).map_err(|e| {
            cubeb_log!("Fail to enable audiounit output scope. Error: {}", e);
            Error::error()
        })?;
        enable_audiounit_scope(unit, io_side::INPUT, false).map_err(|e| {
            cubeb_log!("Fail to disable audiounit input scope. Error: {}", e);
            Error::error()
        })?;
    }

    set_device_to_audiounit(unit, device.id).map_err(|e| {
        cubeb_log!(
            "Fail to set device {} to the created audiounit. Error: {}",
            device.id,
            e
        );
        Error::error()
    })?;

    Ok(unit)
}

fn enable_audiounit_scope(
    unit: AudioUnit,
    side: io_side,
    enable_io: bool,
) -> std::result::Result<(), OSStatus> {
    assert!(!unit.is_null());

    let enable: u32 = if enable_io { 1 } else { 0 };
    let (scope, element) = match side {
        io_side::INPUT => (kAudioUnitScope_Input, AU_IN_BUS),
        io_side::OUTPUT => (kAudioUnitScope_Output, AU_OUT_BUS),
    };
    let status = audio_unit_set_property(
        unit,
        kAudioOutputUnitProperty_EnableIO,
        scope,
        element,
        &enable,
        mem::size_of::<u32>(),
    );
    if status == NO_ERR {
        Ok(())
    } else {
        Err(status)
    }
}

fn set_device_to_audiounit(
    unit: AudioUnit,
    device_id: AudioObjectID,
) -> std::result::Result<(), OSStatus> {
    assert!(!unit.is_null());

    let status = audio_unit_set_property(
        unit,
        kAudioOutputUnitProperty_CurrentDevice,
        kAudioUnitScope_Global,
        0,
        &device_id,
        mem::size_of::<AudioDeviceID>(),
    );
    if status == NO_ERR {
        Ok(())
    } else {
        Err(status)
    }
}

fn create_default_audiounit(flags: device_flags) -> Result<AudioUnit> {
    let desc = get_audiounit_description(flags);
    create_audiounit_by_description(desc)
}

fn get_audiounit_description(flags: device_flags) -> AudioComponentDescription {
    AudioComponentDescription {
        componentType: kAudioUnitType_Output,
        // Use the DefaultOutputUnit for output when no device is specified
        // so we retain automatic output device switching when the default
        // changes. Once we have complete support for device notifications
        // and switching, we can use the AUHAL for everything.
        #[cfg(not(target_os = "ios"))]
        componentSubType: if flags
            .contains(device_flags::DEV_SYSTEM_DEFAULT | device_flags::DEV_OUTPUT)
        {
            kAudioUnitSubType_DefaultOutput
        } else {
            kAudioUnitSubType_HALOutput
        },
        #[cfg(target_os = "ios")]
        componentSubType: kAudioUnitSubType_RemoteIO,
        componentManufacturer: kAudioUnitManufacturer_Apple,
        componentFlags: 0,
        componentFlagsMask: 0,
    }
}

fn create_audiounit_by_description(desc: AudioComponentDescription) -> Result<AudioUnit> {
    let comp = unsafe { AudioComponentFindNext(ptr::null_mut(), &desc) };
    if comp.is_null() {
        cubeb_log!("Could not find matching audio hardware.");
        return Err(Error::error());
    }
    let mut unit: AudioUnit = ptr::null_mut();
    let status = unsafe { AudioComponentInstanceNew(comp, &mut unit) };
    if status == NO_ERR {
        assert!(!unit.is_null());
        Ok(unit)
    } else {
        cubeb_log!("Fail to get a new AudioUnit. Error: {}", status);
        Err(Error::error())
    }
}

// Change buffer size is prone to deadlock thus we change it following the steps:
// - register a listener for the buffer size property
// - change the property
// - wait until the listener is executed
// - property has changed, remove the listener
extern "C" fn buffer_size_changed_callback(
    in_client_data: *mut c_void,
    in_unit: AudioUnit,
    in_property_id: AudioUnitPropertyID,
    in_scope: AudioUnitScope,
    in_element: AudioUnitElement,
) {
    use self::coreaudio_sys_utils::sys;

    let stm = unsafe { &mut *(in_client_data as *mut AudioUnitStream) };

    let au = in_unit;
    let au_element = in_element;

    let (au_scope, au_type) = if AU_IN_BUS == in_element {
        (kAudioUnitScope_Output, "input")
    } else {
        (kAudioUnitScope_Input, "output")
    };

    match in_property_id {
        // Using coreaudio_sys as prefix so kAudioDevicePropertyBufferFrameSize
        // won't be treated as a new variable introduced in the match arm.
        sys::kAudioDevicePropertyBufferFrameSize => {
            if in_scope != au_scope {
                // filter out the callback for global scope
                return;
            }
            let mut new_buffer_size: u32 = 0;
            let mut out_size = mem::size_of::<u32>();
            let r = audio_unit_get_property(
                au,
                kAudioDevicePropertyBufferFrameSize,
                au_scope,
                au_element,
                &mut new_buffer_size,
                &mut out_size,
            );
            if r != NO_ERR {
                cubeb_log!("({:p}) Event: kAudioDevicePropertyBufferFrameSize: Cannot get current buffer size", stm as *const AudioUnitStream);
            } else {
                cubeb_log!("({:p}) Event: kAudioDevicePropertyBufferFrameSize: New {} buffer size = {} for scope {}", stm as *const AudioUnitStream,
                           au_type, new_buffer_size, in_scope);
            }
            *stm.buffer_size_change_state.get_mut() = true;
        }
        _ => {}
    }
}

fn convert_uint32_into_string(data: u32) -> CString {
    let empty = CString::default();
    if data == 0 {
        return empty;
    }

    // Reverse 0xWXYZ into 0xZYXW.
    let mut buffer = vec![b'\x00'; 4]; // 4 bytes for uint32.
    buffer[0] = (data >> 24) as u8;
    buffer[1] = (data >> 16) as u8;
    buffer[2] = (data >> 8) as u8;
    buffer[3] = (data) as u8;

    // CString::new() will consume the input bytes vec and add a '\0' at the
    // end of the bytes. The input bytes vec must not contain any 0 bytes in
    // it in case causing memory leaks.
    CString::new(buffer).unwrap_or(empty)
}

fn audiounit_get_default_datasource(side: io_side) -> Result<(u32)> {
    let (devtype, address) = match side {
        io_side::INPUT => (DeviceType::INPUT, INPUT_DATA_SOURCE_PROPERTY_ADDRESS),
        io_side::OUTPUT => (DeviceType::OUTPUT, OUTPUT_DATA_SOURCE_PROPERTY_ADDRESS),
    };
    let id = audiounit_get_default_device_id(devtype);
    if id == kAudioObjectUnknown {
        return Err(Error::error());
    }

    let mut data: u32 = 0;
    let mut size = mem::size_of::<u32>();
    // This fails with some USB headsets (e.g., Plantronic .Audio 628).
    let r = audio_object_get_property_data(id, &address, &mut size, &mut data);
    if r != NO_ERR {
        data = 0;
    }

    Ok(data)
}

fn audiounit_get_default_datasource_string(side: io_side) -> Result<CString> {
    let data = audiounit_get_default_datasource(side)?;
    Ok(convert_uint32_into_string(data))
}

fn audiounit_strref_to_cstr_utf8(strref: CFStringRef) -> CString {
    let empty = CString::default();
    if strref.is_null() {
        return empty;
    }

    let len = unsafe { CFStringGetLength(strref) };
    // Add 1 to size to allow for '\0' termination character.
    let size = unsafe { CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1 };
    let mut buffer = vec![b'\x00'; size as usize];

    let success = unsafe {
        CFStringGetCString(
            strref,
            buffer.as_mut_ptr() as *mut c_char,
            size,
            kCFStringEncodingUTF8,
        ) != 0
    };
    if !success {
        buffer.clear();
        return empty;
    }

    // CString::new() will consume the input bytes vec and add a '\0' at the
    // end of the bytes. We need to remove the '\0' from the bytes data
    // returned from CFStringGetCString by ourselves to avoid memory leaks.
    // The size returned from CFStringGetMaximumSizeForEncoding is always
    // greater than or equal to the string length, where the string length
    // is the number of characters from the beginning to nul-terminator('\0'),
    // so we should shrink the string vector to fit that size.
    let str_len = unsafe { libc::strlen(buffer.as_ptr() as *mut c_char) };
    buffer.truncate(str_len); // Drop the elements from '\0'(including '\0').

    CString::new(buffer).unwrap_or(empty)
}

fn audiounit_get_channel_count(devid: AudioObjectID, scope: AudioObjectPropertyScope) -> u32 {
    let mut count: u32 = 0;
    let mut size: usize = 0;

    let adr = AudioObjectPropertyAddress {
        mSelector: kAudioDevicePropertyStreamConfiguration,
        mScope: scope,
        mElement: kAudioObjectPropertyElementMaster,
    };

    if audio_object_get_property_data_size(devid, &adr, &mut size) == NO_ERR && size > 0 {
        let mut data: Vec<u8> = allocate_array_by_size(size);
        let ptr = data.as_mut_ptr() as *mut AudioBufferList;
        if audio_object_get_property_data(devid, &adr, &mut size, ptr) == NO_ERR {
            let list: &AudioBufferList = unsafe { &(*ptr) };
            let ptr = list.mBuffers.as_ptr() as *const AudioBuffer;
            let len = list.mNumberBuffers as usize;
            if len == 0 {
                return 0;
            }
            let buffers = unsafe { slice::from_raw_parts(ptr, len) };
            for buffer in buffers {
                count += buffer.mNumberChannels;
            }
        }
    }
    count
}

fn audiounit_get_available_samplerate(
    devid: AudioObjectID,
    scope: AudioObjectPropertyScope,
    min: &mut u32,
    max: &mut u32,
    def: &mut u32,
) {
    let mut adr = AudioObjectPropertyAddress {
        mSelector: 0,
        mScope: scope,
        mElement: kAudioObjectPropertyElementMaster,
    };

    adr.mSelector = kAudioDevicePropertyNominalSampleRate;
    if audio_object_has_property(devid, &adr) {
        let mut size = mem::size_of::<f64>();
        let mut fvalue: f64 = 0.0;
        if audio_object_get_property_data(devid, &adr, &mut size, &mut fvalue) == NO_ERR {
            *def = fvalue as u32;
        }
    }

    adr.mSelector = kAudioDevicePropertyAvailableNominalSampleRates;
    let mut size = 0;
    let mut range = AudioValueRange::default();
    if audio_object_has_property(devid, &adr)
        && audio_object_get_property_data_size(devid, &adr, &mut size) == NO_ERR
    {
        let mut ranges: Vec<AudioValueRange> = allocate_array_by_size(size);
        range.mMinimum = std::f64::MAX;
        range.mMaximum = std::f64::MIN;
        if audio_object_get_property_data(devid, &adr, &mut size, ranges.as_mut_ptr()) == NO_ERR {
            for rng in &ranges {
                if rng.mMaximum > range.mMaximum {
                    range.mMaximum = rng.mMaximum;
                }
                if rng.mMinimum < range.mMinimum {
                    range.mMinimum = rng.mMinimum;
                }
            }
        }
        *max = range.mMaximum as u32;
        *min = range.mMinimum as u32;
    } else {
        *max = 0;
        *min = 0;
    }
}

fn audiounit_get_device_presentation_latency(
    devid: AudioObjectID,
    scope: AudioObjectPropertyScope,
) -> u32 {
    let mut adr = AudioObjectPropertyAddress {
        mSelector: 0,
        mScope: scope,
        mElement: kAudioObjectPropertyElementMaster,
    };
    let mut size: usize = 0;
    let mut dev: u32 = 0;
    let mut stream: u32 = 0;
    let mut sid: [AudioStreamID; 1] = [kAudioObjectUnknown];

    adr.mSelector = kAudioDevicePropertyLatency;
    size = mem::size_of::<u32>();
    if audio_object_get_property_data(devid, &adr, &mut size, &mut dev) != NO_ERR {
        dev = 0;
    }

    adr.mSelector = kAudioDevicePropertyStreams;
    size = mem::size_of_val(&sid);
    assert_eq!(size, mem::size_of::<AudioStreamID>());
    if audio_object_get_property_data(devid, &adr, &mut size, sid.as_mut_ptr()) == NO_ERR {
        adr.mSelector = kAudioStreamPropertyLatency;
        size = mem::size_of::<u32>();
        audio_object_get_property_data(sid[0], &adr, &mut size, &mut stream);
    }

    dev + stream
}

fn audiounit_create_device_from_hwdev(
    dev_info: &mut ffi::cubeb_device_info,
    devid: AudioObjectID,
    devtype: DeviceType,
) -> Result<()> {
    assert!(devtype == DeviceType::INPUT || devtype == DeviceType::OUTPUT);

    let mut adr = AudioObjectPropertyAddress {
        mSelector: 0,
        mScope: 0,
        mElement: kAudioObjectPropertyElementMaster,
    };
    let mut size: usize = 0;

    adr.mScope = if devtype == DeviceType::OUTPUT {
        kAudioDevicePropertyScopeOutput
    } else {
        kAudioDevicePropertyScopeInput
    };

    let ch = audiounit_get_channel_count(devid, adr.mScope);
    if ch == 0 {
        return Err(Error::error());
    }

    *dev_info = ffi::cubeb_device_info::default();

    let mut device_id_str: CFStringRef = ptr::null();
    size = mem::size_of::<CFStringRef>();
    adr.mSelector = kAudioDevicePropertyDeviceUID;
    let mut ret = audio_object_get_property_data(devid, &adr, &mut size, &mut device_id_str);
    if ret == NO_ERR && !device_id_str.is_null() {
        let c_string = audiounit_strref_to_cstr_utf8(device_id_str);
        dev_info.device_id = c_string.into_raw();

        assert!(
            mem::size_of::<ffi::cubeb_devid>() >= mem::size_of_val(&devid),
            "cubeb_devid can't represent devid"
        );
        dev_info.devid = devid as ffi::cubeb_devid;

        dev_info.group_id = dev_info.device_id;

        unsafe {
            CFRelease(device_id_str as *const c_void);
        }
    }

    let mut friendly_name_str: CFStringRef = ptr::null();
    let mut ds: u32 = 0;
    size = mem::size_of::<u32>();
    adr.mSelector = kAudioDevicePropertyDataSource;
    ret = audio_object_get_property_data(devid, &adr, &mut size, &mut ds);
    if ret == NO_ERR {
        let mut trl = AudioValueTranslation {
            mInputData: &mut ds as *mut u32 as *mut c_void,
            mInputDataSize: mem::size_of_val(&ds) as u32,
            mOutputData: &mut friendly_name_str as *mut CFStringRef as *mut c_void,
            mOutputDataSize: mem::size_of::<CFStringRef>() as u32,
        };
        adr.mSelector = kAudioDevicePropertyDataSourceNameForIDCFString;
        size = mem::size_of::<AudioValueTranslation>();
        audio_object_get_property_data(devid, &adr, &mut size, &mut trl);
    }

    // If there is no datasource for this device, fall back to the
    // device name.
    if friendly_name_str.is_null() {
        size = mem::size_of::<CFStringRef>();
        adr.mSelector = kAudioObjectPropertyName;
        audio_object_get_property_data(devid, &adr, &mut size, &mut friendly_name_str);
    }

    if friendly_name_str.is_null() {
        // Couldn't get a datasource name nor a device name, return a
        // valid string of length 0.
        let c_string = CString::default();
        dev_info.friendly_name = c_string.into_raw();
    } else {
        let c_string = audiounit_strref_to_cstr_utf8(friendly_name_str);
        dev_info.friendly_name = c_string.into_raw();
        unsafe {
            CFRelease(friendly_name_str as *const c_void);
        }
    };

    let mut vendor_name_str: CFStringRef = ptr::null();
    size = mem::size_of::<CFStringRef>();
    adr.mSelector = kAudioObjectPropertyManufacturer;
    ret = audio_object_get_property_data(devid, &adr, &mut size, &mut vendor_name_str);
    if ret == NO_ERR && !vendor_name_str.is_null() {
        let c_string = audiounit_strref_to_cstr_utf8(vendor_name_str);
        dev_info.vendor_name = c_string.into_raw();
        unsafe {
            CFRelease(vendor_name_str as *const c_void);
        }
    }

    dev_info.device_type = if devtype == DeviceType::OUTPUT {
        ffi::CUBEB_DEVICE_TYPE_OUTPUT
    } else {
        ffi::CUBEB_DEVICE_TYPE_INPUT
    };
    dev_info.state = ffi::CUBEB_DEVICE_STATE_ENABLED;
    dev_info.preferred = if devid == audiounit_get_default_device_id(devtype) {
        ffi::CUBEB_DEVICE_PREF_ALL
    } else {
        ffi::CUBEB_DEVICE_PREF_NONE
    };

    dev_info.max_channels = ch;
    dev_info.format = ffi::CUBEB_DEVICE_FMT_ALL;
    dev_info.default_format = ffi::CUBEB_DEVICE_FMT_F32NE;
    audiounit_get_available_samplerate(
        devid,
        adr.mScope,
        &mut dev_info.min_rate,
        &mut dev_info.max_rate,
        &mut dev_info.default_rate,
    );

    let latency = audiounit_get_device_presentation_latency(devid, adr.mScope);
    let mut range = AudioValueRange::default();
    adr.mSelector = kAudioDevicePropertyBufferFrameSizeRange;
    size = mem::size_of::<AudioValueRange>();
    ret = audio_object_get_property_data(devid, &adr, &mut size, &mut range);
    if ret == NO_ERR {
        dev_info.latency_lo = latency + range.mMinimum as u32;
        dev_info.latency_hi = latency + range.mMaximum as u32;
    } else {
        dev_info.latency_lo = 10 * dev_info.default_rate / 1000; // Default to 10ms
        dev_info.latency_hi = 100 * dev_info.default_rate / 1000; // Default to 10ms
    }

    Ok(())
}

fn is_aggregate_device(device_info: &ffi::cubeb_device_info) -> bool {
    assert!(!device_info.friendly_name.is_null());
    let private_name =
        CString::new(PRIVATE_AGGREGATE_DEVICE_NAME).expect("Fail to create a private name");
    unsafe {
        libc::strncmp(
            device_info.friendly_name,
            private_name.as_ptr(),
            libc::strlen(private_name.as_ptr()),
        ) == 0
    }
}

fn audiounit_device_destroy(device: &mut ffi::cubeb_device_info) {
    // This should be mapped to the memory allocation in audiounit_create_device_from_hwdev.
    // Set the pointers to null in case it points to some released memory.
    unsafe {
        if !device.device_id.is_null() {
            // group_id is a mirror to device_id, so we could skip it.
            assert!(!device.group_id.is_null());
            assert_eq!(device.device_id, device.group_id);
            let _ = CString::from_raw(device.device_id as *mut _);
            device.device_id = ptr::null();
            device.group_id = ptr::null();
        }
        if !device.friendly_name.is_null() {
            let _ = CString::from_raw(device.friendly_name as *mut _);
            device.friendly_name = ptr::null();
        }
        if !device.vendor_name.is_null() {
            let _ = CString::from_raw(device.vendor_name as *mut _);
            device.vendor_name = ptr::null();
        }
    }
}

fn audiounit_get_devices() -> Vec<AudioObjectID> {
    let mut size: usize = 0;
    let mut ret = audio_object_get_property_data_size(
        kAudioObjectSystemObject,
        &DEVICES_PROPERTY_ADDRESS,
        &mut size,
    );
    if ret != NO_ERR {
        return Vec::new();
    }
    // Total number of input and output devices.
    let mut devices: Vec<AudioObjectID> = allocate_array_by_size(size);
    ret = audio_object_get_property_data(
        kAudioObjectSystemObject,
        &DEVICES_PROPERTY_ADDRESS,
        &mut size,
        devices.as_mut_ptr(),
    );
    if ret != NO_ERR {
        return Vec::new();
    }
    devices
}

fn audiounit_get_devices_of_type(devtype: DeviceType) -> Vec<AudioObjectID> {
    assert!(devtype.intersects(DeviceType::INPUT | DeviceType::OUTPUT));

    let mut devices = audiounit_get_devices();

    // Remove the aggregate device from the list of devices (if any).
    devices.retain(|&device| {
        let name = get_device_name(device);
        if name.is_null() {
            return true;
        }
        let private_device = cfstringref_from_static_string(PRIVATE_AGGREGATE_DEVICE_NAME);
        unsafe {
            let found = CFStringFind(name, private_device, 0).location;
            CFRelease(private_device as *const c_void);
            CFRelease(name as *const c_void);
            found == kCFNotFound
        }
    });

    // Expected sorted but did not find anything in the docs.
    devices.sort();
    if devtype.contains(DeviceType::INPUT | DeviceType::OUTPUT) {
        return devices;
    }

    let scope = if devtype == DeviceType::INPUT {
        kAudioDevicePropertyScopeInput
    } else {
        kAudioDevicePropertyScopeOutput
    };
    let mut devices_in_scope = Vec::new();
    for device in devices {
        if audiounit_get_channel_count(device, scope) > 0 {
            devices_in_scope.push(device);
        }
    }

    devices_in_scope
}

extern "C" fn audiounit_collection_changed_callback(
    _in_object_id: AudioObjectID,
    _in_number_addresses: u32,
    _in_addresses: *const AudioObjectPropertyAddress,
    in_client_data: *mut c_void,
) -> OSStatus {
    let context = unsafe { &mut *(in_client_data as *mut AudioUnitContext) };

    let queue = context.serial_queue;
    let mutexed_context = Arc::new(Mutex::new(context));
    let also_mutexed_context = Arc::clone(&mutexed_context);

    // This can be called from inside an AudioUnit function, dispatch to another queue.
    async_dispatch(queue, move || {
        let ctx_guard = also_mutexed_context.lock().unwrap();
        let ctx_ptr = *ctx_guard as *const AudioUnitContext;

        let mut devices = ctx_guard.devices.lock().unwrap();

        if devices.input.changed_callback.is_none() && devices.output.changed_callback.is_none() {
            return;
        }
        if devices.input.changed_callback.is_some() {
            let input_devices = audiounit_get_devices_of_type(DeviceType::INPUT);
            if devices.input.update_devices(input_devices) {
                unsafe {
                    devices.input.changed_callback.unwrap()(
                        ctx_ptr as *mut ffi::cubeb,
                        devices.input.callback_user_ptr,
                    );
                }
            }
        }
        if devices.output.changed_callback.is_some() {
            let output_devices = audiounit_get_devices_of_type(DeviceType::OUTPUT);
            if devices.output.update_devices(output_devices) {
                unsafe {
                    devices.output.changed_callback.unwrap()(
                        ctx_ptr as *mut ffi::cubeb,
                        devices.output.callback_user_ptr,
                    );
                }
            }
        }
    });

    NO_ERR
}

#[derive(Debug)]
struct DevicesData {
    changed_callback: ffi::cubeb_device_collection_changed_callback,
    callback_user_ptr: *mut c_void,
    devices: Vec<AudioObjectID>,
}

impl DevicesData {
    fn set(
        &mut self,
        changed_callback: ffi::cubeb_device_collection_changed_callback,
        callback_user_ptr: *mut c_void,
        devices: Vec<AudioObjectID>,
    ) {
        self.changed_callback = changed_callback;
        self.callback_user_ptr = callback_user_ptr;
        self.devices = devices;
    }

    fn update_devices(&mut self, devices: Vec<AudioObjectID>) -> bool {
        // Elements in the vector expected sorted.
        if self.devices == devices {
            return false;
        }
        self.devices = devices;
        true
    }

    fn clear(&mut self) {
        self.changed_callback = None;
        self.callback_user_ptr = ptr::null_mut();
        self.devices.clear();
    }

    fn is_empty(&self) -> bool {
        self.changed_callback == None
            && self.callback_user_ptr == ptr::null_mut()
            && self.devices.is_empty()
    }
}

impl Default for DevicesData {
    fn default() -> Self {
        Self {
            changed_callback: None,
            callback_user_ptr: ptr::null_mut(),
            devices: Vec::new(),
        }
    }
}

#[derive(Debug)]
struct SharedDevices {
    input: DevicesData,
    output: DevicesData,
}

impl Default for SharedDevices {
    fn default() -> Self {
        Self {
            input: DevicesData::default(),
            output: DevicesData::default(),
        }
    }
}

#[derive(Debug)]
struct LatencyController {
    streams: u32,
    latency: Option<u32>,
}

impl LatencyController {
    fn add_stream(&mut self, latency: u32) -> Option<u32> {
        self.streams += 1;
        // For the 1st stream set anything within safe min-max
        if self.streams == 1 {
            assert!(self.latency.is_none());
            // Silently clamp the latency down to the platform default, because we
            // synthetize the clock from the callbacks, and we want the clock to update often.
            self.latency = Some(clamp_latency(latency));
        }
        self.latency
    }

    fn subtract_stream(&mut self) -> Option<u32> {
        self.streams -= 1;
        if self.streams == 0 {
            assert!(self.latency.is_some());
            self.latency = None;
        }
        self.latency
    }
}

impl Default for LatencyController {
    fn default() -> Self {
        Self {
            streams: 0,
            latency: None,
        }
    }
}

pub const OPS: Ops = capi_new!(AudioUnitContext, AudioUnitStream);

// The fisrt member of the Cubeb context must be a pointer to a Ops struct. The Ops struct is an
// interface to link to all the Cubeb APIs, and the Cubeb interface use this assumption to operate
// the Cubeb APIs on different implementation.
// #[repr(C)] is used to prevent any padding from being added in the beginning of the AudioUnitContext.
#[repr(C)]
#[derive(Debug)]
pub struct AudioUnitContext {
    _ops: *const Ops,
    // serial_queue will be created by dispatch_queue_create(create_dispatch_queue)
    // without ARC(Automatic Reference Counting) support, so it should be released
    // by dispatch_release(release_dispatch_queue).
    serial_queue: dispatch_queue_t,
    layout: atomic::Atomic<ChannelLayout>,
    channels: u32,
    latency_controller: Mutex<LatencyController>,
    devices: Mutex<SharedDevices>,
}

impl AudioUnitContext {
    fn new() -> Self {
        Self {
            _ops: &OPS as *const _,
            serial_queue: create_dispatch_queue(DISPATCH_QUEUE_LABEL, DISPATCH_QUEUE_SERIAL),
            layout: atomic::Atomic::new(ChannelLayout::UNDEFINED),
            channels: 0,
            latency_controller: Mutex::new(LatencyController::default()),
            devices: Mutex::new(SharedDevices::default()),
        }
    }

    fn active_streams(&self) -> u32 {
        let controller = self.latency_controller.lock().unwrap();
        controller.streams
    }

    fn update_latency_by_adding_stream(&self, latency_frames: u32) -> Option<u32> {
        let mut controller = self.latency_controller.lock().unwrap();
        controller.add_stream(latency_frames)
    }

    fn update_latency_by_removing_stream(&self) -> Option<u32> {
        let mut controller = self.latency_controller.lock().unwrap();
        controller.subtract_stream()
    }

    fn add_devices_changed_listener(
        &mut self,
        devtype: DeviceType,
        collection_changed_callback: ffi::cubeb_device_collection_changed_callback,
        user_ptr: *mut c_void,
    ) -> Result<()> {
        assert!(devtype.intersects(DeviceType::INPUT | DeviceType::OUTPUT));
        assert!(collection_changed_callback.is_some());

        let context_ptr = self as *mut AudioUnitContext;
        let mut devices = self.devices.lock().unwrap();

        // Note: second register without unregister first causes 'nope' error.
        // Current implementation requires unregister before register a new cb.
        if devtype.contains(DeviceType::INPUT) && devices.input.changed_callback.is_some()
            || devtype.contains(DeviceType::OUTPUT) && devices.output.changed_callback.is_some()
        {
            return Err(Error::invalid_parameter());
        }

        if devices.input.changed_callback.is_none() && devices.output.changed_callback.is_none() {
            let ret = audio_object_add_property_listener(
                kAudioObjectSystemObject,
                &DEVICES_PROPERTY_ADDRESS,
                audiounit_collection_changed_callback,
                context_ptr,
            );
            if ret != NO_ERR {
                cubeb_log!(
                    "Cannot add devices-changed listener for {:?}, Error: {}",
                    devtype,
                    ret
                );
                return Err(Error::error());
            }
        }

        if devtype.contains(DeviceType::INPUT) {
            // Expected empty after unregister.
            assert!(devices.input.is_empty());
            devices.input.set(
                collection_changed_callback,
                user_ptr,
                audiounit_get_devices_of_type(DeviceType::INPUT),
            );
        }

        if devtype.contains(DeviceType::OUTPUT) {
            // Expected empty after unregister.
            assert!(devices.output.is_empty());
            devices.output.set(
                collection_changed_callback,
                user_ptr,
                audiounit_get_devices_of_type(DeviceType::OUTPUT),
            );
        }

        Ok(())
    }

    fn remove_devices_changed_listener(&mut self, devtype: DeviceType) -> Result<()> {
        if !devtype.intersects(DeviceType::INPUT | DeviceType::OUTPUT) {
            return Err(Error::invalid_parameter());
        }

        let context_ptr = self as *mut AudioUnitContext;
        let mut devices = self.devices.lock().unwrap();

        if devtype.contains(DeviceType::INPUT) {
            devices.input.clear();
        }

        if devtype.contains(DeviceType::OUTPUT) {
            devices.output.clear();
        }

        if devices.input.changed_callback.is_some() || devices.output.changed_callback.is_some() {
            return Ok(());
        }

        // Note: unregister a non registered cb is not a problem, not checking.
        let r = audio_object_remove_property_listener(
            kAudioObjectSystemObject,
            &DEVICES_PROPERTY_ADDRESS,
            audiounit_collection_changed_callback,
            context_ptr,
        );
        if r == NO_ERR {
            Ok(())
        } else {
            cubeb_log!(
                "Cannot remove devices-changed listener for {:?}, Error: {}",
                devtype,
                r
            );
            Err(Error::error())
        }
    }
}

impl ContextOps for AudioUnitContext {
    fn init(_context_name: Option<&CStr>) -> Result<Context> {
        set_notification_runloop();
        let ctx = Box::new(AudioUnitContext::new());
        Ok(unsafe { Context::from_ptr(Box::into_raw(ctx) as *mut _) })
    }

    fn backend_id(&mut self) -> &'static CStr {
        unsafe { CStr::from_ptr(b"audiounit-rust\0".as_ptr() as *const _) }
    }
    #[cfg(target_os = "ios")]
    fn max_channel_count(&mut self) -> Result<u32> {
        Ok(2u32)
    }
    #[cfg(not(target_os = "ios"))]
    fn max_channel_count(&mut self) -> Result<u32> {
        let mut size: usize = 0;
        let mut r = NO_ERR;
        let mut output_device_id: AudioDeviceID = kAudioObjectUnknown;
        let mut stream_format = AudioStreamBasicDescription::default();
        let stream_format_address = AudioObjectPropertyAddress {
            mSelector: kAudioDevicePropertyStreamFormat,
            mScope: kAudioDevicePropertyScopeOutput,
            mElement: kAudioObjectPropertyElementMaster,
        };

        output_device_id = audiounit_get_default_device_id(DeviceType::OUTPUT);
        if output_device_id == kAudioObjectUnknown {
            return Err(Error::error());
        }

        size = mem::size_of_val(&stream_format);
        assert_eq!(size, mem::size_of::<AudioStreamBasicDescription>());

        r = audio_object_get_property_data(
            output_device_id,
            &stream_format_address,
            &mut size,
            &mut stream_format,
        );

        if r != NO_ERR {
            cubeb_log!("AudioObjectPropertyAddress/StreamFormat rv={}", r);
            return Err(Error::error());
        }

        Ok(stream_format.mChannelsPerFrame)
    }
    #[cfg(target_os = "ios")]
    fn min_latency(&mut self, _params: StreamParams) -> Result<u32> {
        Err(not_supported());
    }
    #[cfg(not(target_os = "ios"))]
    fn min_latency(&mut self, _params: StreamParams) -> Result<u32> {
        let range = audiounit_get_acceptable_latency_range().map_err(|e| {
            cubeb_log!("Could not get acceptable latency range.");
            e
        })?;
        Ok(cmp::max(range.mMinimum as u32, SAFE_MIN_LATENCY_FRAMES))
    }
    #[cfg(target_os = "ios")]
    fn preferred_sample_rate(&mut self) -> Result<u32> {
        Err(not_supported());
    }
    #[cfg(not(target_os = "ios"))]
    fn preferred_sample_rate(&mut self) -> Result<u32> {
        let mut size: usize = 0;
        let mut r = NO_ERR;
        let mut fsamplerate: f64 = 0.0;
        let mut output_device_id: AudioDeviceID = kAudioObjectUnknown;
        let samplerate_address = AudioObjectPropertyAddress {
            mSelector: kAudioDevicePropertyNominalSampleRate,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMaster,
        };

        output_device_id = audiounit_get_default_device_id(DeviceType::OUTPUT);
        if output_device_id == kAudioObjectUnknown {
            return Err(Error::error());
        }

        size = mem::size_of_val(&fsamplerate);
        assert_eq!(size, mem::size_of::<f64>());
        r = audio_object_get_property_data(
            output_device_id,
            &samplerate_address,
            &mut size,
            &mut fsamplerate,
        );

        if r != NO_ERR {
            return Err(Error::error());
        }

        Ok(fsamplerate as u32)
    }
    fn enumerate_devices(
        &mut self,
        devtype: DeviceType,
        collection: &DeviceCollectionRef,
    ) -> Result<()> {
        let input_devs = if devtype.contains(DeviceType::INPUT) {
            audiounit_get_devices_of_type(DeviceType::INPUT)
        } else {
            Vec::<AudioObjectID>::new()
        };

        let output_devs = if devtype.contains(DeviceType::OUTPUT) {
            audiounit_get_devices_of_type(DeviceType::OUTPUT)
        } else {
            Vec::<AudioObjectID>::new()
        };

        // Count number of input and output devices.  This is not necessarily the same as
        // the count of raw devices supported by the system since, for example, with Soundflower
        // installed, some devices may report as being both input *and* output and cubeb
        // separates those into two different devices.

        let mut devices: Vec<ffi::cubeb_device_info> =
            allocate_array(output_devs.len() + input_devs.len());

        let mut count = 0;
        if devtype.contains(DeviceType::OUTPUT) {
            for dev in output_devs {
                let device = &mut devices[count];
                if audiounit_create_device_from_hwdev(device, dev, DeviceType::OUTPUT).is_err()
                    || is_aggregate_device(device)
                {
                    continue;
                }
                count += 1;
            }
        }

        if devtype.contains(DeviceType::INPUT) {
            for dev in input_devs {
                let device = &mut devices[count];
                if audiounit_create_device_from_hwdev(device, dev, DeviceType::INPUT).is_err()
                    || is_aggregate_device(device)
                {
                    continue;
                }
                count += 1;
            }
        }

        // Remove the redundant space, set len to count.
        devices.truncate(count);

        let coll = unsafe { &mut *collection.as_ptr() };
        if count > 0 {
            let (ptr, len) = forget_vec(devices);
            coll.device = ptr;
            coll.count = len;
        } else {
            coll.device = ptr::null_mut();
            coll.count = 0;
        }

        Ok(())
    }
    fn device_collection_destroy(&mut self, collection: &mut DeviceCollectionRef) -> Result<()> {
        assert!(!collection.as_ptr().is_null());
        let coll = unsafe { &mut *collection.as_ptr() };
        if coll.device.is_null() {
            return Ok(());
        }

        let mut devices = retake_forgotten_vec(coll.device, coll.count);
        for device in &mut devices {
            audiounit_device_destroy(device);
        }
        drop(devices); // Release the memory.
        coll.device = ptr::null_mut();
        coll.count = 0;
        Ok(())
    }
    fn stream_init(
        &mut self,
        _stream_name: Option<&CStr>,
        input_device: DeviceId,
        input_stream_params: Option<&StreamParamsRef>,
        output_device: DeviceId,
        output_stream_params: Option<&StreamParamsRef>,
        latency_frames: u32,
        data_callback: ffi::cubeb_data_callback,
        state_callback: ffi::cubeb_state_callback,
        user_ptr: *mut c_void,
    ) -> Result<Stream> {
        if (!input_device.is_null() && input_stream_params.is_none())
            || (!output_device.is_null() && output_stream_params.is_none())
        {
            return Err(Error::invalid_parameter());
        }

        // Latency cannot change if another stream is operating in parallel. In this case
        // latency is set to the other stream value.
        let global_latency_frames = self
            .update_latency_by_adding_stream(latency_frames)
            .unwrap();
        if global_latency_frames != latency_frames {
            cubeb_log!(
                "Use global latency {} instead of the requested latency {}.",
                global_latency_frames,
                latency_frames
            );
        }

        let mut boxed_stream = Box::new(AudioUnitStream::new(
            self,
            user_ptr,
            data_callback,
            state_callback,
            global_latency_frames,
        ));
        boxed_stream.init_mutex();

        if let Some(stream_params_ref) = input_stream_params {
            assert!(!stream_params_ref.as_ptr().is_null());
            boxed_stream.input_stream_params =
                StreamParams::from(unsafe { (*stream_params_ref.as_ptr()) });
            boxed_stream.input_device =
                create_device_info(input_device as AudioDeviceID, DeviceType::INPUT).map_err(
                    |e| {
                        cubeb_log!(
                            "({:p}) Fail to create device info for input.",
                            boxed_stream.as_ref()
                        );
                        e
                    },
                )?;
        }
        if let Some(stream_params_ref) = output_stream_params {
            assert!(!stream_params_ref.as_ptr().is_null());
            boxed_stream.output_stream_params =
                StreamParams::from(unsafe { *(stream_params_ref.as_ptr()) });
            boxed_stream.output_device =
                create_device_info(output_device as AudioDeviceID, DeviceType::OUTPUT).map_err(
                    |e| {
                        cubeb_log!(
                            "({:p}) Fail to create device info for output.",
                            boxed_stream.as_ref()
                        );
                        e
                    },
                )?;
        }

        if let Err(r) = {
            // It's not critical to lock here, because no other thread has been started
            // yet, but it allows to assert that the lock has been taken in `AudioUnitStream::setup`.
            let mutex_ptr = &mut boxed_stream.mutex as *mut OwnedCriticalSection;
            let _lock = AutoLock::new(unsafe { &mut (*mutex_ptr) });
            boxed_stream.setup()
        } {
            cubeb_log!(
                "({:p}) Could not setup the audiounit stream.",
                boxed_stream.as_ref()
            );
            return Err(r);
        }

        if let Err(r) = boxed_stream.install_system_changed_callback() {
            cubeb_log!(
                "({:p}) Could not install the device change callback.",
                boxed_stream.as_ref()
            );
            return Err(r);
        }

        let cubeb_stream = unsafe { Stream::from_ptr(Box::into_raw(boxed_stream) as *mut _) };
        cubeb_log!(
            "({:p}) Cubeb stream init successful.",
            &cubeb_stream as *const Stream
        );
        Ok(cubeb_stream)
    }
    fn register_device_collection_changed(
        &mut self,
        devtype: DeviceType,
        collection_changed_callback: ffi::cubeb_device_collection_changed_callback,
        user_ptr: *mut c_void,
    ) -> Result<()> {
        if devtype == DeviceType::UNKNOWN {
            return Err(Error::invalid_parameter());
        }
        if collection_changed_callback.is_some() {
            self.add_devices_changed_listener(devtype, collection_changed_callback, user_ptr)
        } else {
            self.remove_devices_changed_listener(devtype)
        }
    }
}

impl Drop for AudioUnitContext {
    fn drop(&mut self) {
        {
            let controller = self.latency_controller.lock().unwrap();
            // Disabling this assert for bug 1083664 -- we seem to leak a stream
            // assert(controller.streams == 0);
            if controller.streams > 0 {
                cubeb_log!(
                    "({:p}) API misuse, {} streams active when context destroyed!",
                    self as *const AudioUnitContext,
                    controller.streams
                );
            }
        }

        // Unregister the callback if necessary.
        self.remove_devices_changed_listener(DeviceType::INPUT);
        self.remove_devices_changed_listener(DeviceType::OUTPUT);

        release_dispatch_queue(self.serial_queue);
    }
}

unsafe impl Send for AudioUnitContext {}
unsafe impl Sync for AudioUnitContext {}

// The fisrt two members of the Cubeb stream must be a pointer to its Cubeb context and a void user
// defined pointer. The Cubeb interface use this assumption to operate the Cubeb APIs.
// #[repr(C)] is used to prevent any padding from being added in the beginning of the AudioUnitStream.
#[repr(C)]
#[derive(Debug)]
struct AudioUnitStream<'ctx> {
    context: &'ctx mut AudioUnitContext,
    user_ptr: *mut c_void,

    data_callback: ffi::cubeb_data_callback,
    state_callback: ffi::cubeb_state_callback,
    device_changed_callback: ffi::cubeb_device_changed_callback,
    device_changed_callback_lock: OwnedCriticalSection,
    // Stream creation parameters
    input_stream_params: StreamParams,
    output_stream_params: StreamParams,
    input_device: device_info,
    output_device: device_info,
    // Format descriptions
    input_desc: AudioStreamBasicDescription,
    output_desc: AudioStreamBasicDescription,
    // I/O AudioUnits
    input_unit: AudioUnit,
    output_unit: AudioUnit,
    // I/O device sample rate
    input_hw_rate: f64,
    output_hw_rate: f64,
    mutex: OwnedCriticalSection,
    // Hold the input samples in every input callback iteration.
    // Only accessed on input/output callback thread and during initial configure.
    input_linear_buffer: Option<Box<AutoArrayWrapper>>,
    // Frame counters
    frames_played: AtomicU64,
    frames_queued: u64,
    // How many frames got read from the input since the stream started (includes
    // padded silence)
    frames_read: AtomicI64,
    // How many frames got written to the output device since the stream started
    frames_written: AtomicI64,
    shutdown: AtomicBool,
    draining: AtomicBool,
    reinit_pending: AtomicBool,
    destroy_pending: AtomicBool,
    // Latency requested by the user.
    latency_frames: u32,
    current_latency_frames: AtomicU32,
    panning: atomic::Atomic<f32>,
    resampler: AutoRelease<ffi::cubeb_resampler>,
    // This is true if a device change callback is currently running.
    switching_device: AtomicBool,
    buffer_size_change_state: AtomicBool,
    aggregate_device_id: AudioDeviceID, // the aggregate device id
    plugin_id: AudioObjectID,           // used to create aggregate device
    // Mixer interface
    mixer: AutoRelease<ffi::cubeb_mixer>,
    // Buffer where remixing/resampling will occur when upmixing is required
    // Only accessed from callback thread
    temp_buffer: Vec<u8>,
    temp_buffer_size: usize,
    // Listeners indicating what system events are monitored.
    default_input_listener: Option<device_property_listener>,
    default_output_listener: Option<device_property_listener>,
    input_alive_listener: Option<device_property_listener>,
    input_source_listener: Option<device_property_listener>,
    output_source_listener: Option<device_property_listener>,
}

impl<'ctx> AudioUnitStream<'ctx> {
    fn new(
        context: &'ctx mut AudioUnitContext,
        user_ptr: *mut c_void,
        data_callback: ffi::cubeb_data_callback,
        state_callback: ffi::cubeb_state_callback,
        latency_frames: u32,
    ) -> Self {
        AudioUnitStream {
            context,
            user_ptr,
            data_callback,
            state_callback,
            device_changed_callback: None,
            device_changed_callback_lock: OwnedCriticalSection::new(),
            input_stream_params: StreamParams::from(ffi::cubeb_stream_params {
                format: ffi::CUBEB_SAMPLE_FLOAT32NE,
                rate: 0,
                channels: 0,
                layout: ffi::CUBEB_LAYOUT_UNDEFINED,
                prefs: ffi::CUBEB_STREAM_PREF_NONE,
            }),
            output_stream_params: StreamParams::from(ffi::cubeb_stream_params {
                format: ffi::CUBEB_SAMPLE_FLOAT32NE,
                rate: 0,
                channels: 0,
                layout: ffi::CUBEB_LAYOUT_UNDEFINED,
                prefs: ffi::CUBEB_STREAM_PREF_NONE,
            }),
            input_device: device_info::default(),
            output_device: device_info::default(),
            input_desc: AudioStreamBasicDescription::default(),
            output_desc: AudioStreamBasicDescription::default(),
            input_unit: ptr::null_mut(),
            output_unit: ptr::null_mut(),
            input_hw_rate: 0_f64,
            output_hw_rate: 0_f64,
            mutex: OwnedCriticalSection::new(),
            input_linear_buffer: None,
            frames_played: AtomicU64::new(0),
            frames_queued: 0,
            frames_read: AtomicI64::new(0),
            frames_written: AtomicI64::new(0),
            shutdown: AtomicBool::new(true),
            draining: AtomicBool::new(false),
            reinit_pending: AtomicBool::new(false),
            destroy_pending: AtomicBool::new(false),
            latency_frames,
            current_latency_frames: AtomicU32::new(0),
            panning: atomic::Atomic::new(0.0_f32),
            resampler: AutoRelease::new(ptr::null_mut(), ffi::cubeb_resampler_destroy),
            switching_device: AtomicBool::new(false),
            buffer_size_change_state: AtomicBool::new(false),
            aggregate_device_id: kAudioObjectUnknown,
            plugin_id: kAudioObjectUnknown,
            mixer: AutoRelease::new(ptr::null_mut(), ffi::cubeb_mixer_destroy),
            temp_buffer: Vec::new(),
            temp_buffer_size: 0,
            default_input_listener: None,
            default_output_listener: None,
            input_alive_listener: None,
            input_source_listener: None,
            output_source_listener: None,
        }
    }

    fn init_mutex(&mut self) {
        self.device_changed_callback_lock.init();
        self.mutex.init();
    }

    fn add_device_listener(&self, listener: &device_property_listener) -> OSStatus {
        audio_object_add_property_listener(
            listener.device,
            listener.property,
            listener.listener,
            self as *const Self as *mut Self,
        )
    }

    fn remove_device_listener(&self, listener: &device_property_listener) -> OSStatus {
        audio_object_remove_property_listener(
            listener.device,
            listener.property,
            listener.listener,
            self as *const Self as *mut Self,
        )
    }

    fn notify_state_changed(&mut self, state: State) {
        if self.state_callback.is_none() {
            return;
        }
        let callback = self.state_callback.unwrap();
        unsafe {
            callback(
                self as *const AudioUnitStream as *mut ffi::cubeb_stream,
                self.user_ptr,
                state.into(),
            );
        }
    }

    fn has_input(&self) -> bool {
        self.input_stream_params.rate() > 0
    }

    fn has_output(&self) -> bool {
        self.output_stream_params.rate() > 0
    }

    fn render_input(
        &mut self,
        flags: *mut AudioUnitRenderActionFlags,
        tstamp: *const AudioTimeStamp,
        bus: u32,
        input_frames: u32,
    ) -> OSStatus {
        // Create the AudioBufferList to store input.
        let mut input_buffer_list = AudioBufferList::default();
        input_buffer_list.mBuffers[0].mDataByteSize = self.input_desc.mBytesPerFrame * input_frames;
        input_buffer_list.mBuffers[0].mData = ptr::null_mut();
        input_buffer_list.mBuffers[0].mNumberChannels = self.input_desc.mChannelsPerFrame;
        input_buffer_list.mNumberBuffers = 1;

        assert!(!self.input_unit.is_null());
        let r = audio_unit_render(
            self.input_unit,
            flags,
            tstamp,
            bus,
            input_frames,
            &mut input_buffer_list,
        );

        if r != NO_ERR {
            cubeb_log!("AudioUnitRender rv={}", r);
            if r != kAudioUnitErr_CannotDoInCurrentContext {
                return r;
            }
            if !self.output_unit.is_null() {
                // kAudioUnitErr_CannotDoInCurrentContext is returned when using a BT
                // headset and the profile is changed from A2DP to HFP/HSP. The previous
                // output device is no longer valid and must be reset.
                self.reinit_async();
            }
            // For now state that no error occurred and feed silence, stream will be
            // resumed once reinit has completed.
            cubeb_logv!(
                "({:p}) input: reinit pending feeding silence instead",
                self as *const AudioUnitStream
            );
            self.input_linear_buffer
                .as_mut()
                .unwrap()
                .push_zeros((input_frames * self.input_desc.mChannelsPerFrame) as usize);
        } else {
            // Copy input data in linear buffer.
            self.input_linear_buffer.as_mut().unwrap().push(
                input_buffer_list.mBuffers[0].mData,
                (input_frames * self.input_desc.mChannelsPerFrame) as usize,
            );
        }

        // Advance input frame counter.
        assert!(input_frames > 0);
        self.frames_read
            .fetch_add(i64::from(input_frames), Ordering::SeqCst);

        cubeb_logv!(
            "({:p}) input: buffers {}, size {}, channels {}, rendered frames {}, total frames {}.",
            self as *const AudioUnitStream,
            input_buffer_list.mNumberBuffers,
            input_buffer_list.mBuffers[0].mDataByteSize,
            input_buffer_list.mBuffers[0].mNumberChannels,
            input_frames,
            self.input_linear_buffer.as_ref().unwrap().elements()
                / self.input_desc.mChannelsPerFrame as usize
        );

        NO_ERR
    }

    fn mix_output_buffer(
        &mut self,
        output_frames: usize,
        input_buffer: *mut c_void,
        input_buffer_size: usize,
        output_buffer: *mut c_void,
        output_buffer_size: usize,
    ) {
        assert!(
            input_buffer_size
                >= cubeb_sample_size(self.output_stream_params.format())
                    * self.output_stream_params.channels() as usize
                    * output_frames
        );
        assert!(output_buffer_size >= self.output_desc.mBytesPerFrame as usize * output_frames);

        let r = unsafe {
            ffi::cubeb_mixer_mix(
                self.mixer.as_mut(),
                output_frames,
                input_buffer,
                input_buffer_size,
                output_buffer,
                output_buffer_size,
            )
        };
        if r != 0 {
            cubeb_log!("Remix error = {}", r);
        }
    }

    fn minimum_resampling_input_frames(&self, output_frames: i64) -> i64 {
        assert_ne!(self.input_hw_rate, 0_f64);
        assert_ne!(self.output_stream_params.rate(), 0);
        if self.input_hw_rate == f64::from(self.output_stream_params.rate()) {
            // Fast path.
            return output_frames;
        }
        (self.input_hw_rate * output_frames as f64 / f64::from(self.output_stream_params.rate()))
            .ceil() as i64
    }

    fn reinit(&mut self) -> Result<()> {
        if !self.shutdown.load(Ordering::SeqCst) {
            self.stop_internal();
        }

        if self.uninstall_device_changed_callback().is_err() {
            cubeb_log!(
                "({:p}) Could not uninstall all device change listeners.",
                self as *const AudioUnitStream
            );
        }

        {
            let mutex_ptr = &mut self.mutex as *mut OwnedCriticalSection;
            let _lock = AutoLock::new(unsafe { &mut (*mutex_ptr) });

            assert!(!self.input_unit.is_null() || !self.output_unit.is_null());
            let vol_rv = if self.output_unit.is_null() {
                Err(Error::error())
            } else {
                get_volume(self.output_unit)
            };

            self.close();

            // Reinit occurs in one of the following case:
            // - When the device is not alive any more
            // - When the default system device change.
            // - The bluetooth device changed from A2DP to/from HFP/HSP profile
            // We first attempt to re-use the same device id, should that fail we will
            // default to the (potentially new) default device.
            let has_input = !self.input_unit.is_null();
            let input_device = if has_input {
                self.input_device.id
            } else {
                kAudioObjectUnknown
            };

            if has_input {
                self.input_device = create_device_info(input_device, DeviceType::INPUT).map_err(|e| {
                    cubeb_log!(
                        "({:p}) Create input device info failed. This can happen when last media device is unplugged",
                        self as *const AudioUnitStream
                    );
                    e
                })?;
            }

            // Always use the default output on reinit. This is not correct in every
            // case but it is sufficient for Firefox and prevent reinit from reporting
            // failures. It will change soon when reinit mechanism will be updated.
            self.output_device = create_device_info(kAudioObjectUnknown, DeviceType::OUTPUT).map_err(|e| {
                cubeb_log!(
                    "({:p}) Create output device info failed. This can happen when last media device is unplugged",
                    self as *const AudioUnitStream
                );
                e
            })?;

            if self.setup().is_err() {
                cubeb_log!(
                    "({:p}) Stream reinit failed.",
                    self as *const AudioUnitStream
                );
                if has_input && input_device != kAudioObjectUnknown {
                    // Attempt to re-use the same device-id failed, so attempt again with
                    // default input device.
                    self.close();
                    self.input_device = create_device_info(kAudioObjectUnknown, DeviceType::INPUT)
                        .map_err(|e| {
                            cubeb_log!(
                                "({:p}) Create input device info failed. This can happen when last media device is unplugged",
                                self as *const AudioUnitStream
                            );
                            e
                        })?;
                    self.setup().map_err(|e| {
                        cubeb_log!(
                            "({:p}) Second stream reinit failed.",
                            self as *const AudioUnitStream
                        );
                        e
                    })?;
                }
            }

            if vol_rv.is_ok() {
                set_volume(self.output_unit, vol_rv.unwrap());
            }

            // If the stream was running, start it again.
            if !self.shutdown.load(Ordering::SeqCst) {
                self.start_internal()?;
            }
        }

        Ok(())
    }

    fn reinit_async(&mut self) {
        if self.reinit_pending.swap(true, Ordering::SeqCst) {
            // A reinit task is already pending, nothing more to do.
            cubeb_log!(
                "({:p}) re-init stream task already pending, cancelling request",
                self as *const AudioUnitStream
            );
            return;
        }

        let queue = self.context.serial_queue;
        let mutexed_stm = Arc::new(Mutex::new(self));
        let also_mutexed_stm = Arc::clone(&mutexed_stm);
        // Use a new thread, through the queue, to avoid deadlock when calling
        // Get/SetProperties method from inside notify callback
        async_dispatch(queue, move || {
            let mut stm_guard = also_mutexed_stm.lock().unwrap();
            let stm_ptr = *stm_guard as *const AudioUnitStream;
            if stm_guard.destroy_pending.load(Ordering::SeqCst) {
                cubeb_log!(
                    "({:p}) stream pending destroy, cancelling reinit task",
                    stm_ptr
                );
                return;
            }

            if stm_guard.reinit().is_err() {
                if stm_guard.uninstall_system_changed_callback().is_err() {
                    cubeb_log!(
                        "({:p}) Could not uninstall system changed callback",
                        stm_ptr
                    );
                }
                stm_guard.notify_state_changed(State::Error);
                cubeb_log!(
                    "({:p}) Could not reopen the stream after switching.",
                    stm_ptr
                );
            }
            *stm_guard.switching_device.get_mut() = false;
            *stm_guard.reinit_pending.get_mut() = false;
        });
    }

    fn install_device_changed_callback(&mut self) -> Result<()> {
        let mut rv = NO_ERR;
        let mut r = Ok(());

        if !self.output_unit.is_null() {
            // This event will notify us when the data source on the same device changes,
            // for example when the user plugs in a normal (non-usb) headset in the
            // headphone jack.
            assert_ne!(self.output_device.id, kAudioObjectUnknown);
            assert_ne!(self.output_device.id, kAudioObjectSystemObject);

            self.output_source_listener = Some(device_property_listener::new(
                self.output_device.id,
                &OUTPUT_DATA_SOURCE_PROPERTY_ADDRESS,
                audiounit_property_listener_callback,
            ));
            rv = self.add_device_listener(self.output_source_listener.as_ref().unwrap());
            if rv != NO_ERR {
                self.output_source_listener = None;
                cubeb_log!("AudioObjectAddPropertyListener/output/kAudioDevicePropertyDataSource rv={}, device id={}", rv, self.output_device.id);
                r = Err(Error::error());
            }
        }

        if !self.input_unit.is_null() {
            // This event will notify us when the data source on the input device changes.
            assert_ne!(self.input_device.id, kAudioObjectUnknown);
            assert_ne!(self.input_device.id, kAudioObjectSystemObject);

            self.input_source_listener = Some(device_property_listener::new(
                self.input_device.id,
                &INPUT_DATA_SOURCE_PROPERTY_ADDRESS,
                audiounit_property_listener_callback,
            ));
            rv = self.add_device_listener(self.input_source_listener.as_ref().unwrap());
            if rv != NO_ERR {
                self.input_source_listener = None;
                cubeb_log!("AudioObjectAddPropertyListener/input/kAudioDevicePropertyDataSource rv={}, device id={}", rv, self.input_device.id);
                r = Err(Error::error());
            }

            // Event to notify when the input is going away.
            self.input_alive_listener = Some(device_property_listener::new(
                self.input_device.id,
                &DEVICE_IS_ALIVE_PROPERTY_ADDRESS,
                audiounit_property_listener_callback,
            ));
            rv = self.add_device_listener(self.input_alive_listener.as_ref().unwrap());
            if rv != NO_ERR {
                self.input_alive_listener = None;
                cubeb_log!("AudioObjectAddPropertyListener/input/kAudioDevicePropertyDeviceIsAlive rv={}, device id ={}", rv, self.input_device.id);
                r = Err(Error::error());
            }
        }

        r
    }

    fn install_system_changed_callback(&mut self) -> Result<()> {
        let mut r = NO_ERR;

        if !self.output_unit.is_null() {
            // This event will notify us when the default audio device changes,
            // for example when the user plugs in a USB headset and the system chooses it
            // automatically as the default, or when another device is chosen in the
            // dropdown list.
            self.default_output_listener = Some(device_property_listener::new(
                kAudioObjectSystemObject,
                &DEFAULT_OUTPUT_DEVICE_PROPERTY_ADDRESS,
                audiounit_property_listener_callback,
            ));
            r = self.add_device_listener(self.default_output_listener.as_ref().unwrap());
            if r != NO_ERR {
                self.default_output_listener = None;
                cubeb_log!("AudioObjectAddPropertyListener/output/kAudioHardwarePropertyDefaultOutputDevice rv={}", r);
                return Err(Error::error());
            }
        }

        if !self.input_unit.is_null() {
            // This event will notify us when the default input device changes.
            self.default_input_listener = Some(device_property_listener::new(
                kAudioObjectSystemObject,
                &DEFAULT_INPUT_DEVICE_PROPERTY_ADDRESS,
                audiounit_property_listener_callback,
            ));
            r = self.add_device_listener(self.default_input_listener.as_ref().unwrap());
            if r != NO_ERR {
                self.default_input_listener = None;
                cubeb_log!("AudioObjectAddPropertyListener/input/kAudioHardwarePropertyDefaultInputDevice rv={}", r);
                return Err(Error::error());
            }
        }

        Ok(())
    }

    fn uninstall_device_changed_callback(&mut self) -> Result<()> {
        let mut rv = NO_ERR;
        // Failing to uninstall listeners is not a fatal error.
        let mut r = Ok(());

        if self.output_source_listener.is_some() {
            rv = self.remove_device_listener(self.output_source_listener.as_ref().unwrap());
            if rv != NO_ERR {
                cubeb_log!("AudioObjectRemovePropertyListener/output/kAudioDevicePropertyDataSource rv={}, device id={}", rv, self.output_device.id);
                r = Err(Error::error());
            }
            self.output_source_listener = None;
        }

        if self.input_source_listener.is_some() {
            rv = self.remove_device_listener(self.input_source_listener.as_ref().unwrap());
            if rv != NO_ERR {
                cubeb_log!("AudioObjectRemovePropertyListener/input/kAudioDevicePropertyDataSource rv={}, device id={}", rv, self.input_device.id);
                r = Err(Error::error());
            }
            self.input_source_listener = None;
        }

        if self.input_alive_listener.is_some() {
            rv = self.remove_device_listener(self.input_alive_listener.as_ref().unwrap());
            if rv != NO_ERR {
                cubeb_log!("AudioObjectRemovePropertyListener/input/kAudioDevicePropertyDeviceIsAlive rv={}, device id={}", rv, self.input_device.id);
                r = Err(Error::error());
            }
            self.input_alive_listener = None;
        }

        r
    }

    fn uninstall_system_changed_callback(&mut self) -> Result<()> {
        let mut r = NO_ERR;

        if self.default_output_listener.is_some() {
            r = self.remove_device_listener(self.default_output_listener.as_ref().unwrap());
            if r != NO_ERR {
                return Err(Error::error());
            }
            self.default_output_listener = None;
        }

        if self.default_input_listener.is_some() {
            r = self.remove_device_listener(self.default_input_listener.as_ref().unwrap());
            if r != NO_ERR {
                return Err(Error::error());
            }
            self.default_input_listener = None;
        }

        Ok(())
    }

    fn init_mixer(&mut self) {
        self.mixer.reset(unsafe {
            ffi::cubeb_mixer_create(
                self.output_stream_params.format().into(),
                self.output_stream_params.channels(),
                self.output_stream_params.layout().into(),
                self.context.channels,
                self.context.layout.load(atomic::Ordering::SeqCst).into(),
            )
        });
        assert!(!self.mixer.as_ptr().is_null());
    }

    fn layout_init(&mut self, side: io_side) {
        // We currently don't support the input layout setting.
        if side == io_side::INPUT {
            return;
        }

        self.context.layout.store(
            audiounit_get_current_channel_layout(self.output_unit),
            atomic::Ordering::SeqCst,
        );
        audiounit_set_channel_layout(
            self.output_unit,
            io_side::OUTPUT,
            self.context.layout.load(atomic::Ordering::SeqCst),
        );
    }

    fn workaround_for_airpod(&self) {
        assert_ne!(self.input_device.id, self.output_device.id);
        assert_ne!(self.aggregate_device_id, kAudioObjectUnknown);

        let mut input_device_info = ffi::cubeb_device_info::default();
        assert_ne!(self.input_device.id, kAudioObjectUnknown);
        audiounit_create_device_from_hwdev(
            &mut input_device_info,
            self.input_device.id,
            DeviceType::INPUT,
        );

        let mut output_device_info = ffi::cubeb_device_info::default();
        assert_ne!(self.output_device.id, kAudioObjectUnknown);
        audiounit_create_device_from_hwdev(
            &mut output_device_info,
            self.output_device.id,
            DeviceType::OUTPUT,
        );

        let input_name_str = unsafe {
            CString::from_raw(input_device_info.friendly_name as *mut c_char)
                .into_string()
                .expect("Fail to convert input name from CString into String")
        };
        input_device_info.friendly_name = ptr::null();

        let output_name_str = unsafe {
            CString::from_raw(output_device_info.friendly_name as *mut c_char)
                .into_string()
                .expect("Fail to convert output name from CString into String")
        };
        output_device_info.friendly_name = ptr::null();

        if input_name_str.contains("AirPods") && output_name_str.contains("AirPods") {
            let mut input_min_rate = 0;
            let mut input_max_rate = 0;
            let mut input_nominal_rate = 0;
            audiounit_get_available_samplerate(
                self.input_device.id,
                kAudioObjectPropertyScopeGlobal,
                &mut input_min_rate,
                &mut input_max_rate,
                &mut input_nominal_rate,
            );
            cubeb_log!(
                "({:p}) Input device {}, name: {}, min: {}, max: {}, nominal rate: {}",
                self as *const AudioUnitStream,
                self.input_device.id,
                input_name_str,
                input_min_rate,
                input_max_rate,
                input_nominal_rate
            );

            let mut output_min_rate = 0;
            let mut output_max_rate = 0;
            let mut output_nominal_rate = 0;
            audiounit_get_available_samplerate(
                self.output_device.id,
                kAudioObjectPropertyScopeGlobal,
                &mut output_min_rate,
                &mut output_max_rate,
                &mut output_nominal_rate,
            );
            cubeb_log!(
                "({:p}) Output device {}, name: {}, min: {}, max: {}, nominal rate: {}",
                self as *const AudioUnitStream,
                self.output_device.id,
                output_name_str,
                output_min_rate,
                output_max_rate,
                output_nominal_rate
            );

            let rate = f64::from(input_nominal_rate);
            let addr = AudioObjectPropertyAddress {
                mSelector: kAudioDevicePropertyNominalSampleRate,
                mScope: kAudioObjectPropertyScopeGlobal,
                mElement: kAudioObjectPropertyElementMaster,
            };

            let rv = audio_object_set_property_data(
                self.aggregate_device_id,
                &addr,
                mem::size_of::<f64>(),
                &rate,
            );
            if rv != NO_ERR {
                cubeb_log!("Non fatal error, AudioObjectSetPropertyData/kAudioDevicePropertyNominalSampleRate, rv={}", rv);
            }
        }

        // Retrieve the rest lost memory.
        // No need to retrieve the memory of {input,output}_device_info.friendly_name
        // since they are already retrieved/retaken above.
        assert!(input_device_info.friendly_name.is_null());
        audiounit_device_destroy(&mut input_device_info);
        assert!(output_device_info.friendly_name.is_null());
        audiounit_device_destroy(&mut output_device_info);
    }

    // Aggregate Device is a virtual audio interface which utilizes inputs and outputs
    // of one or more physical audio interfaces. It is possible to use the clock of
    // one of the devices as a master clock for all the combined devices and enable
    // drift compensation for the devices that are not designated clock master.
    //
    // Creating a new aggregate device programmatically requires [0][1]:
    // 1. Locate the base plug-in ("com.apple.audio.CoreAudio")
    // 2. Create a dictionary that describes the aggregate device
    //    (don't add sub-devices in that step, prone to fail [0])
    // 3. Ask the base plug-in to create the aggregate device (blank)
    // 4. Add the array of sub-devices.
    // 5. Set the master device (1st output device in our case)
    // 6. Enable drift compensation for the non-master devices
    //
    // [0] https://lists.apple.com/archives/coreaudio-api/2006/Apr/msg00092.html
    // [1] https://lists.apple.com/archives/coreaudio-api/2005/Jul/msg00150.html
    // [2] CoreAudio.framework/Headers/AudioHardware.h
    fn create_aggregate_device(&mut self) -> Result<()> {
        if let Err(r) = audiounit_create_blank_aggregate_device_sync(
            &mut self.plugin_id,
            &mut self.aggregate_device_id,
        ) {
            cubeb_log!(
                "({:p}) Failed to create blank aggregate device",
                self as *const AudioUnitStream
            );
            return Err(r);
        }

        // The aggregate device may not be created at this point!
        // It's better to listen the system devices changing to make sure it's added.

        if let Err(r) = audiounit_set_aggregate_sub_device_list_sync(
            self.aggregate_device_id,
            self.input_device.id,
            self.output_device.id,
        ) {
            cubeb_log!(
                "({:p}) Failed to set aggregate sub-device list",
                self as *const AudioUnitStream
            );
            audiounit_destroy_aggregate_device(self.plugin_id, &mut self.aggregate_device_id);
            return Err(r);
        }

        if let Err(r) = audiounit_set_master_aggregate_device(self.aggregate_device_id) {
            cubeb_log!(
                "({:p}) Failed to set master sub-device for aggregate device",
                self as *const AudioUnitStream
            );
            audiounit_destroy_aggregate_device(self.plugin_id, &mut self.aggregate_device_id);
            return Err(r);
        }

        if let Err(r) = audiounit_activate_clock_drift_compensation(self.aggregate_device_id) {
            cubeb_log!(
                "({:p}) Failed to activate clock drift compensation for aggregate device",
                self as *const AudioUnitStream
            );
            audiounit_destroy_aggregate_device(self.plugin_id, &mut self.aggregate_device_id);
            return Err(r);
        }

        self.workaround_for_airpod();

        Ok(())
    }

    fn set_buffer_size(&mut self, new_size_frames: u32, side: io_side) -> Result<()> {
        use std::thread;

        assert_ne!(new_size_frames, 0);
        let (au, au_scope, au_element) = if side == io_side::INPUT {
            (self.input_unit, kAudioUnitScope_Output, AU_IN_BUS)
        } else {
            (self.output_unit, kAudioUnitScope_Input, AU_OUT_BUS)
        };
        assert!(!au.is_null());

        let mut buffer_frames: u32 = 0;
        let mut size = mem::size_of_val(&buffer_frames);
        let mut r = audio_unit_get_property(
            au,
            kAudioDevicePropertyBufferFrameSize,
            au_scope,
            au_element,
            &mut buffer_frames,
            &mut size,
        );
        if r != NO_ERR {
            cubeb_log!(
                "AudioUnitGetProperty/{}/kAudioDevicePropertyBufferFrameSize rv={}",
                side.to_string(),
                r
            );
            return Err(Error::error());
        }

        assert_ne!(buffer_frames, 0);

        if new_size_frames == buffer_frames {
            cubeb_log!(
                "({:p}) No need to update {} buffer size already {} frames",
                self as *const AudioUnitStream,
                side.to_string(),
                buffer_frames
            );
            return Ok(());
        }

        r = audio_unit_add_property_listener(
            au,
            kAudioDevicePropertyBufferFrameSize,
            buffer_size_changed_callback,
            self as *mut AudioUnitStream as *mut c_void,
        );
        if r != NO_ERR {
            cubeb_log!(
                "AudioUnitAddPropertyListener/{}/kAudioDevicePropertyBufferFrameSize rv={}",
                side.to_string(),
                r
            );
            return Err(Error::error());
        }

        *self.buffer_size_change_state.get_mut() = false;

        r = audio_unit_set_property(
            au,
            kAudioDevicePropertyBufferFrameSize,
            au_scope,
            au_element,
            &new_size_frames,
            mem::size_of_val(&new_size_frames),
        );
        if r != NO_ERR {
            cubeb_log!(
                "AudioUnitSetProperty/{}/kAudioDevicePropertyBufferFrameSize rv={}",
                side.to_string(),
                r
            );

            r = audio_unit_remove_property_listener_with_user_data(
                au,
                kAudioDevicePropertyBufferFrameSize,
                buffer_size_changed_callback,
                self as *mut AudioUnitStream as *mut c_void,
            );
            if r != NO_ERR {
                cubeb_log!(
                    "AudioUnitAddPropertyListener/{}/kAudioDevicePropertyBufferFrameSize rv={}",
                    side.to_string(),
                    r
                );
            }

            return Err(Error::error());
        }

        let mut count: u32 = 0;
        let duration = Duration::from_millis(100); // 0.1 sec

        while !self.buffer_size_change_state.load(Ordering::SeqCst) && count < 30 {
            count += 1;
            thread::sleep(duration);
            cubeb_log!(
                "({:p}) audiounit_set_buffer_size : wait count = {}",
                self as *const AudioUnitStream,
                count
            );
        }

        r = audio_unit_remove_property_listener_with_user_data(
            au,
            kAudioDevicePropertyBufferFrameSize,
            buffer_size_changed_callback,
            self as *mut AudioUnitStream as *mut c_void,
        );
        if r != NO_ERR {
            cubeb_log!(
                "AudioUnitAddPropertyListener/{}/kAudioDevicePropertyBufferFrameSize rv={}",
                side.to_string(),
                r
            );
            return Err(Error::error());
        }

        if !self.buffer_size_change_state.load(Ordering::SeqCst) && count >= 30 {
            cubeb_log!(
                "({:p}) Error, did not get buffer size change callback ...",
                self as *const AudioUnitStream
            );
            return Err(Error::error());
        }

        cubeb_log!(
            "({:p}) {} buffer size changed to {} frames.",
            self as *const AudioUnitStream,
            side.to_string(),
            new_size_frames
        );
        Ok(())
    }

    fn configure_input(&mut self) -> Result<()> {
        assert!(!self.input_unit.is_null());

        let mut r = NO_ERR;
        let mut size: usize = 0;
        let mut aurcbs_in = AURenderCallbackStruct::default();

        cubeb_log!(
            "({:p}) Opening input side: rate {}, channels {}, format {:?}, layout {:?}, prefs {:?}, latency in frames {}.",
            self as *const AudioUnitStream,
            self.input_stream_params.rate(),
            self.input_stream_params.channels(),
            self.input_stream_params.format(),
            self.input_stream_params.layout(),
            self.input_stream_params.prefs(),
            self.latency_frames
        );

        // Get input device sample rate.
        let mut input_hw_desc = AudioStreamBasicDescription::default();
        size = mem::size_of::<AudioStreamBasicDescription>();
        r = audio_unit_get_property(
            self.input_unit,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            AU_IN_BUS,
            &mut input_hw_desc,
            &mut size,
        );
        if r != NO_ERR {
            cubeb_log!(
                "AudioUnitGetProperty/input/kAudioUnitProperty_StreamFormat rv={}",
                r
            );
            return Err(Error::error());
        }
        self.input_hw_rate = input_hw_desc.mSampleRate;
        cubeb_log!(
            "({:p}) Input hardware description: {:?}",
            self as *const AudioUnitStream,
            input_hw_desc
        );

        // Set format description according to the input params.
        self.input_desc = create_stream_description(&self.input_stream_params).map_err(|e| {
            cubeb_log!(
                "({:p}) Setting format description for input failed.",
                self as *const AudioUnitStream
            );
            e
        })?;

        // Use latency to set buffer size
        assert_ne!(self.latency_frames, 0);
        let latency_frames = self.latency_frames;
        if let Err(r) = self.set_buffer_size(latency_frames, io_side::INPUT) {
            cubeb_log!(
                "({:p}) Error in change input buffer size.",
                self as *const AudioUnitStream
            );
            return Err(r);
        }

        let mut src_desc = self.input_desc;
        // Input AudioUnit must be configured with device's sample rate.
        // we will resample inside input callback.
        src_desc.mSampleRate = self.input_hw_rate;

        r = audio_unit_set_property(
            self.input_unit,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output,
            AU_IN_BUS,
            &src_desc,
            mem::size_of::<AudioStreamBasicDescription>(),
        );
        if r != NO_ERR {
            cubeb_log!(
                "AudioUnitSetProperty/input/kAudioUnitProperty_StreamFormat rv={}",
                r
            );
            return Err(Error::error());
        }

        // Frames per buffer in the input callback.
        r = audio_unit_set_property(
            self.input_unit,
            kAudioUnitProperty_MaximumFramesPerSlice,
            kAudioUnitScope_Global,
            AU_IN_BUS,
            &self.latency_frames,
            mem::size_of::<u32>(),
        );
        if r != NO_ERR {
            cubeb_log!(
                "AudioUnitSetProperty/input/kAudioUnitProperty_MaximumFramesPerSlice rv={}",
                r
            );
            return Err(Error::error());
        }

        let array_capacity = if self.has_output() {
            8 // Full-duplex increase capacity
        } else {
            1 // Input only capacity
        };

        self.input_linear_buffer = Some(create_auto_array(
            self.input_desc,
            self.latency_frames,
            array_capacity,
        )?);

        aurcbs_in.inputProc = Some(audiounit_input_callback);
        aurcbs_in.inputProcRefCon = self as *mut AudioUnitStream as *mut c_void;

        r = audio_unit_set_property(
            self.input_unit,
            kAudioOutputUnitProperty_SetInputCallback,
            kAudioUnitScope_Global,
            AU_OUT_BUS,
            &aurcbs_in,
            mem::size_of_val(&aurcbs_in),
        );
        if r != NO_ERR {
            cubeb_log!(
                "AudioUnitSetProperty/input/kAudioOutputUnitProperty_SetInputCallback rv={}",
                r
            );
            return Err(Error::error());
        }

        self.frames_read.store(0, Ordering::SeqCst);

        cubeb_log!(
            "({:p}) Input audiounit init successfully.",
            self as *const AudioUnitStream
        );

        Ok(())
    }

    fn configure_output(&mut self) -> Result<()> {
        assert!(!self.output_unit.is_null());

        let mut r = NO_ERR;
        let mut aurcbs_out = AURenderCallbackStruct::default();
        let mut size: usize = 0;

        cubeb_log!(
            "({:p}) Opening output side: rate {}, channels {}, format {:?}, layout {:?}, prefs {:?}, latency in frames {}.",
            self as *const AudioUnitStream,
            self.output_stream_params.rate(),
            self.output_stream_params.channels(),
            self.output_stream_params.format(),
            self.output_stream_params.layout(),
            self.output_stream_params.prefs(),
            self.latency_frames
        );

        self.output_desc = create_stream_description(&self.output_stream_params).map_err(|e| {
            cubeb_log!(
                "({:p}) Could not initialize the audio stream description.",
                self as *const AudioUnitStream
            );
            e
        })?;

        // Get output device sample rate.
        let mut output_hw_desc = AudioStreamBasicDescription::default();
        size = mem::size_of::<AudioStreamBasicDescription>();
        r = audio_unit_get_property(
            self.output_unit,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Output,
            AU_OUT_BUS,
            &mut output_hw_desc,
            &mut size,
        );
        if r != NO_ERR {
            cubeb_log!(
                "AudioUnitGetProperty/output/kAudioUnitProperty_StreamFormat rv={}",
                r
            );
            return Err(Error::error());
        }
        self.output_hw_rate = output_hw_desc.mSampleRate;
        cubeb_log!(
            "({:p}) Output hardware description: {:?}",
            self as *const AudioUnitStream,
            output_hw_desc
        );
        self.context.channels = output_hw_desc.mChannelsPerFrame;

        // Set the input layout to match the output device layout.
        self.layout_init(io_side::OUTPUT);
        cubeb_log!(
            "({:p}) Output hardware layout: {:?}",
            self,
            self.context.layout
        );
        if self.context.channels != self.output_stream_params.channels()
            || self.context.layout.load(atomic::Ordering::SeqCst)
                != self.output_stream_params.layout()
        {
            cubeb_log!("Incompatible channel layouts detected, setting up remixer");
            self.init_mixer();
            // We will be remixing the data before it reaches the output device.
            // We need to adjust the number of channels and other
            // AudioStreamDescription details.
            self.output_desc.mChannelsPerFrame = self.context.channels;
            self.output_desc.mBytesPerFrame =
                (self.output_desc.mBitsPerChannel / 8) * self.output_desc.mChannelsPerFrame;
            self.output_desc.mBytesPerPacket =
                self.output_desc.mBytesPerFrame * self.output_desc.mFramesPerPacket;
        } else {
            self.mixer.reset(ptr::null_mut());
        }

        r = audio_unit_set_property(
            self.output_unit,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            AU_OUT_BUS,
            &self.output_desc,
            mem::size_of::<AudioStreamBasicDescription>(),
        );
        if r != NO_ERR {
            cubeb_log!(
                "AudioUnitSetProperty/output/kAudioUnitProperty_StreamFormat rv={}",
                r
            );
            return Err(Error::error());
        }

        // Use latency to set buffer size
        assert_ne!(self.latency_frames, 0);
        let latency_frames = self.latency_frames;
        if let Err(r) = self.set_buffer_size(latency_frames, io_side::OUTPUT) {
            cubeb_log!(
                "({:p}) Error in change output buffer size.",
                self as *const AudioUnitStream
            );
            return Err(r);
        }

        // Frames per buffer in the input callback.
        r = audio_unit_set_property(
            self.output_unit,
            kAudioUnitProperty_MaximumFramesPerSlice,
            kAudioUnitScope_Global,
            AU_OUT_BUS,
            &self.latency_frames,
            mem::size_of::<u32>(),
        );
        if r != NO_ERR {
            cubeb_log!(
                "AudioUnitSetProperty/output/kAudioUnitProperty_MaximumFramesPerSlice rv={}",
                r
            );
            return Err(Error::error());
        }

        aurcbs_out.inputProc = Some(audiounit_output_callback);
        aurcbs_out.inputProcRefCon = self as *mut AudioUnitStream as *mut c_void;
        r = audio_unit_set_property(
            self.output_unit,
            kAudioUnitProperty_SetRenderCallback,
            kAudioUnitScope_Global,
            AU_OUT_BUS,
            &aurcbs_out,
            mem::size_of_val(&aurcbs_out),
        );
        if r != NO_ERR {
            cubeb_log!(
                "AudioUnitSetProperty/output/kAudioUnitProperty_SetRenderCallback rv={}",
                r
            );
            return Err(Error::error());
        }

        self.frames_written.store(0, Ordering::SeqCst);

        cubeb_log!(
            "({:p}) Output audiounit init successfully.",
            self as *const AudioUnitStream
        );
        Ok(())
    }

    fn setup(&mut self) -> Result<()> {
        self.mutex.assert_current_thread_owns();

        if self
            .input_stream_params
            .prefs()
            .contains(StreamPrefs::LOOPBACK)
            || self
                .output_stream_params
                .prefs()
                .contains(StreamPrefs::LOOPBACK)
        {
            cubeb_log!(
                "({:p}) Loopback not supported for audiounit.",
                self as *const AudioUnitStream
            );
            return Err(Error::not_supported());
        }

        let mut in_dev_info = self.input_device.clone();
        let mut out_dev_info = self.output_device.clone();

        if self.has_input() && self.has_output() && self.input_device.id != self.output_device.id {
            if self.create_aggregate_device().is_err() {
                self.aggregate_device_id = kAudioObjectUnknown;
                cubeb_log!(
                    "({:p}) Create aggregate devices failed.",
                    self as *const AudioUnitStream
                );
            // !!!NOTE: It is not necessary to return here. If it does not
            // return it will fallback to the old implementation. The intention
            // is to investigate how often it fails. I plan to remove
            // it after a couple of weeks.
            } else {
                in_dev_info.id = self.aggregate_device_id;
                out_dev_info.id = self.aggregate_device_id;
                in_dev_info.flags = device_flags::DEV_INPUT;
                out_dev_info.flags = device_flags::DEV_OUTPUT;
            }
        }

        // Configure I/O stream
        if self.has_input() {
            self.input_unit = create_audiounit(&in_dev_info).map_err(|e| {
                cubeb_log!(
                    "({:p}) AudioUnit creation for input failed.",
                    self as *const AudioUnitStream
                );
                e
            })?;

            if let Err(r) = self.configure_input() {
                cubeb_log!(
                    "({:p}) Configure audiounit input failed.",
                    self as *const AudioUnitStream
                );
                return Err(r);
            }
        }

        if self.has_output() {
            self.output_unit = create_audiounit(&out_dev_info).map_err(|e| {
                cubeb_log!(
                    "({:p}) AudioUnit creation for output failed.",
                    self as *const AudioUnitStream
                );
                e
            })?;

            if let Err(r) = self.configure_output() {
                cubeb_log!(
                    "({:p}) Configure audiounit output failed.",
                    self as *const AudioUnitStream
                );
                return Err(r);
            }
        }

        // We use a resampler because input AudioUnit operates
        // reliable only in the capture device sample rate.
        // Resampler will convert it to the user sample rate
        // and deliver it to the callback.
        let target_sample_rate = if self.has_input() {
            self.input_stream_params.rate()
        } else {
            assert!(self.has_output());
            self.output_stream_params.rate()
        };

        let mut input_unconverted_params: ffi::cubeb_stream_params =
            unsafe { ::std::mem::zeroed() };
        if self.has_input() {
            input_unconverted_params = unsafe { (*(self.input_stream_params.as_ptr())) }; // Perform copy.
            input_unconverted_params.rate = self.input_hw_rate as u32;
        }

        let stm_ptr = self as *mut AudioUnitStream as *mut ffi::cubeb_stream;
        let stm_input_params = if self.has_input() {
            &mut input_unconverted_params
        } else {
            ptr::null_mut()
        };
        let stm_output_params = if self.has_output() {
            self.output_stream_params.as_ptr()
        } else {
            ptr::null_mut()
        };
        self.resampler.reset(unsafe {
            ffi::cubeb_resampler_create(
                stm_ptr,
                stm_input_params,
                stm_output_params,
                target_sample_rate,
                self.data_callback,
                self.user_ptr,
                ffi::CUBEB_RESAMPLER_QUALITY_DESKTOP,
            )
        });

        if self.resampler.as_ptr().is_null() {
            cubeb_log!(
                "({:p}) Could not create resampler.",
                self as *const AudioUnitStream
            );
            return Err(Error::error());
        }

        if !self.input_unit.is_null() {
            let r = audio_unit_initialize(self.input_unit);
            if r != NO_ERR {
                cubeb_log!("AudioUnitInitialize/input rv={}", r);
                return Err(Error::error());
            }
        }

        if !self.output_unit.is_null() {
            let r = audio_unit_initialize(self.output_unit);
            if r != NO_ERR {
                cubeb_log!("AudioUnitInitialize/output rv={}", r);
                return Err(Error::error());
            }

            self.current_latency_frames.store(
                audiounit_get_device_presentation_latency(
                    self.output_device.id,
                    kAudioDevicePropertyScopeOutput,
                ),
                Ordering::SeqCst,
            );

            let mut unit_s: f64 = 0.0;
            let mut size = mem::size_of_val(&unit_s);
            if audio_unit_get_property(
                self.output_unit,
                kAudioUnitProperty_Latency,
                kAudioUnitScope_Global,
                0,
                &mut unit_s,
                &mut size,
            ) == NO_ERR
            {
                self.current_latency_frames.fetch_add(
                    (unit_s * self.output_desc.mSampleRate) as u32,
                    Ordering::SeqCst,
                );
            }
        }

        if self.install_device_changed_callback().is_err() {
            cubeb_log!(
                "({:p}) Could not install all device change callback.",
                self as *const AudioUnitStream
            );
        }

        Ok(())
    }

    fn close(&mut self) {
        self.mutex.assert_current_thread_owns();

        if !self.input_unit.is_null() {
            audio_unit_uninitialize(self.input_unit);
            dispose_audio_unit(self.input_unit);
            self.input_unit = ptr::null_mut();
        }

        if !self.output_unit.is_null() {
            audio_unit_uninitialize(self.output_unit);
            dispose_audio_unit(self.output_unit);
            self.output_unit = ptr::null_mut();
        }

        self.resampler.reset(ptr::null_mut());
        self.mixer.reset(ptr::null_mut());

        if self.aggregate_device_id != kAudioObjectUnknown {
            audiounit_destroy_aggregate_device(self.plugin_id, &mut self.aggregate_device_id);
            self.aggregate_device_id = kAudioObjectUnknown;
        }
    }

    fn destroy_internal(&mut self) {
        if self.uninstall_system_changed_callback().is_err() {
            cubeb_log!(
                "({:p}) Could not uninstall the device changed callback",
                self as *const AudioUnitStream
            );
        }

        if self.uninstall_device_changed_callback().is_err() {
            cubeb_log!(
                "({:p}) Could not uninstall all device change listeners",
                self as *const AudioUnitStream
            );
        }

        let mutex_ptr = &mut self.mutex as *mut OwnedCriticalSection;
        let _lock = AutoLock::new(unsafe { &mut (*mutex_ptr) });
        self.close();
        assert!(self.context.active_streams() >= 1);
        self.context.update_latency_by_removing_stream();
    }

    fn start_internal(&self) -> Result<()> {
        if !self.input_unit.is_null() {
            let r = audio_output_unit_start(self.input_unit);
            if r != NO_ERR {
                cubeb_log!("AudioOutputUnitStart (input) rv={}", r);
                return Err(Error::error());
            }
        }
        if !self.output_unit.is_null() {
            let r = audio_output_unit_start(self.output_unit);
            if r != NO_ERR {
                cubeb_log!("AudioOutputUnitStart (output) rv={}", r);
                return Err(Error::error());
            }
        }
        Ok(())
    }

    fn stop_internal(&self) {
        if !self.input_unit.is_null() {
            assert_eq!(audio_output_unit_stop(self.input_unit), NO_ERR);
        }
        if !self.output_unit.is_null() {
            assert_eq!(audio_output_unit_stop(self.output_unit), NO_ERR);
        }
    }

    fn destroy(&mut self) {
        if !self.shutdown.load(Ordering::SeqCst) {
            self.stop_internal();
            *self.shutdown.get_mut() = true;
        }

        *self.destroy_pending.get_mut() = true;

        let queue = self.context.serial_queue;
        let mutexed_stm = Arc::new(Mutex::new(self));
        let also_mutexed_stm = Arc::clone(&mutexed_stm);
        // Execute close in serial queue to avoid collision
        // with reinit when un/plug devices
        sync_dispatch(queue, move || {
            let mut stm_guard = also_mutexed_stm.lock().unwrap();
            stm_guard.destroy_internal();
        });

        let stm_guard = mutexed_stm.lock().unwrap();
        let stm_ptr = *stm_guard as *const AudioUnitStream;
        cubeb_log!("Cubeb stream ({:p}) destroyed successful.", stm_ptr);
    }
}

impl<'ctx> Drop for AudioUnitStream<'ctx> {
    fn drop(&mut self) {
        self.destroy();
    }
}

impl<'ctx> StreamOps for AudioUnitStream<'ctx> {
    fn start(&mut self) -> Result<()> {
        *self.shutdown.get_mut() = false;
        *self.draining.get_mut() = false;

        self.start_internal()?;

        self.notify_state_changed(State::Started);

        cubeb_log!(
            "Cubeb stream ({:p}) started successfully.",
            self as *const AudioUnitStream
        );
        Ok(())
    }
    fn stop(&mut self) -> Result<()> {
        *self.shutdown.get_mut() = true;

        self.stop_internal();

        self.notify_state_changed(State::Stopped);

        cubeb_log!(
            "Cubeb stream ({:p}) stopped successfully.",
            self as *const AudioUnitStream
        );
        Ok(())
    }
    fn reset_default_device(&mut self) -> Result<()> {
        Err(Error::not_supported())
    }
    fn position(&mut self) -> Result<u64> {
        let current_latency_frames = u64::from(self.current_latency_frames.load(Ordering::SeqCst));
        let frames_played = self.frames_played.load(Ordering::SeqCst);
        let position = if current_latency_frames > frames_played {
            0
        } else {
            frames_played - current_latency_frames
        };
        Ok(position)
    }
    #[cfg(target_os = "ios")]
    fn latency(&mut self) -> Result<u32> {
        Err(not_supported())
    }
    #[cfg(not(target_os = "ios"))]
    fn latency(&mut self) -> Result<u32> {
        Ok(self.current_latency_frames.load(Ordering::SeqCst))
    }
    fn set_volume(&mut self, volume: f32) -> Result<()> {
        set_volume(self.output_unit, volume)
    }
    fn set_panning(&mut self, panning: f32) -> Result<()> {
        if self.output_desc.mChannelsPerFrame > 2 {
            return Err(Error::invalid_format());
        }

        self.panning.store(panning, Ordering::Relaxed);
        Ok(())
    }
    #[cfg(target_os = "ios")]
    fn current_device(&mut self) -> Result<&DeviceRef> {
        Err(not_supported())
    }
    #[cfg(not(target_os = "ios"))]
    fn current_device(&mut self) -> Result<&DeviceRef> {
        let mut device: Box<ffi::cubeb_device> = Box::new(ffi::cubeb_device::default());
        let input_source = audiounit_get_default_datasource_string(io_side::INPUT)?;
        device.input_name = input_source.into_raw();
        let output_source = audiounit_get_default_datasource_string(io_side::OUTPUT)?;
        device.output_name = output_source.into_raw();
        Ok(unsafe { DeviceRef::from_ptr(Box::into_raw(device)) })
    }
    #[cfg(target_os = "ios")]
    fn device_destroy(&mut self, device: &DeviceRef) -> Result<()> {
        Err(not_supported())
    }
    #[cfg(not(target_os = "ios"))]
    fn device_destroy(&mut self, device: &DeviceRef) -> Result<()> {
        if device.as_ptr().is_null() {
            Err(Error::error())
        } else {
            unsafe {
                let mut dev: Box<ffi::cubeb_device> = Box::from_raw(device.as_ptr() as *mut _);
                if !dev.output_name.is_null() {
                    let _ = CString::from_raw(dev.output_name as *mut _);
                    dev.output_name = ptr::null_mut();
                }
                if !dev.input_name.is_null() {
                    let _ = CString::from_raw(dev.input_name as *mut _);
                    dev.input_name = ptr::null_mut();
                }
                drop(dev);
            }
            Ok(())
        }
    }
    fn register_device_changed_callback(
        &mut self,
        device_changed_callback: ffi::cubeb_device_changed_callback,
    ) -> Result<()> {
        let _dev_cb_lock = AutoLock::new(&mut self.device_changed_callback_lock);
        // Note: second register without unregister first causes 'nope' error.
        // Current implementation requires unregister before register a new cb.
        assert!(device_changed_callback.is_none() || self.device_changed_callback.is_none());
        self.device_changed_callback = device_changed_callback;
        Ok(())
    }
}

unsafe impl<'ctx> Send for AudioUnitStream<'ctx> {}
unsafe impl<'ctx> Sync for AudioUnitStream<'ctx> {}
