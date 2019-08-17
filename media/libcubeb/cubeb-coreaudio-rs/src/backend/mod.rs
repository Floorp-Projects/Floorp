// Copyright © 2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.
#![allow(unused_assignments)]
#![allow(unused_must_use)]

extern crate coreaudio_sys_utils;
extern crate libc;

mod aggregate_device;
mod auto_array;
mod auto_release;
mod device_property;
mod mixer;
mod property_address;
mod resampler;
mod utils;

use self::aggregate_device::*;
use self::auto_array::*;
use self::auto_release::*;
use self::coreaudio_sys_utils::aggregate_device::*;
use self::coreaudio_sys_utils::audio_object::*;
use self::coreaudio_sys_utils::audio_unit::*;
use self::coreaudio_sys_utils::cf_mutable_dict::*;
use self::coreaudio_sys_utils::dispatch::*;
use self::coreaudio_sys_utils::string::*;
use self::coreaudio_sys_utils::sys::*;
use self::device_property::*;
use self::mixer::*;
use self::property_address::*;
use self::resampler::*;
use self::utils::*;
use atomic;
use cubeb_backend::{
    ffi, ChannelLayout, Context, ContextOps, DeviceCollectionRef, DeviceId, DeviceRef, DeviceType,
    Error, Ops, Result, SampleFormat, State, Stream, StreamOps, StreamParams, StreamParamsRef,
    StreamPrefs,
};
use mach::mach_time::{mach_absolute_time, mach_timebase_info};
use std::cmp;
use std::ffi::{CStr, CString};
use std::fmt;
use std::mem;
use std::os::raw::c_void;
use std::ptr;
use std::slice;
use std::sync::atomic::{AtomicBool, AtomicI64, AtomicU32, AtomicU64, Ordering};
use std::sync::{Arc, Condvar, Mutex};
use std::time::Duration;

const NO_ERR: OSStatus = 0;

const AU_OUT_BUS: AudioUnitElement = 0;
const AU_IN_BUS: AudioUnitElement = 1;

const DISPATCH_QUEUE_LABEL: &str = "org.mozilla.cubeb";
const PRIVATE_AGGREGATE_DEVICE_NAME: &str = "CubebAggregateDevice";

// Testing empirically, some headsets report a minimal latency that is very low,
// but this does not work in practice. Lie and say the minimum is 256 frames.
const SAFE_MIN_LATENCY_FRAMES: u32 = 128;
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

lazy_static! {
    static ref HOST_TIME_TO_NS_RATIO: (u32, u32) = {
        let mut timebase_info = mach_timebase_info { numer: 0, denom: 0 };
        unsafe {
            mach_timebase_info(&mut timebase_info);
        }
        (timebase_info.numer, timebase_info.denom)
    };
}

#[allow(non_camel_case_types)]
#[derive(Clone, Debug, PartialEq)]
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
) -> Result<Box<dyn AutoArrayWrapper>> {
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

fn minimum_resampling_input_frames(input_rate: f64, output_rate: f64, output_frames: i64) -> i64 {
    assert_ne!(input_rate, 0_f64);
    assert_ne!(output_rate, 0_f64);
    if input_rate == output_rate {
        return output_frames;
    }
    (input_rate * output_frames as f64 / output_rate).ceil() as i64
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
    enum ErrorHandle {
        Return(OSStatus),
        Reinit,
    };

    assert!(input_frames > 0);
    assert_eq!(bus, AU_IN_BUS);

    assert!(!user_ptr.is_null());
    let stm = unsafe { &mut *(user_ptr as *mut AudioUnitStream) };

    if stm.shutdown.load(Ordering::SeqCst) {
        cubeb_log!("({:p}) input shutdown", stm as *const AudioUnitStream);
        return NO_ERR;
    }

    let handler = |stm: &mut AudioUnitStream,
                   flags: *mut AudioUnitRenderActionFlags,
                   tstamp: *const AudioTimeStamp,
                   bus: u32,
                   input_frames: u32|
     -> (ErrorHandle, Option<State>) {
        assert_eq!(
            stm.core_stream_data.stm_ptr,
            user_ptr as *const AudioUnitStream
        );

        // Create the AudioBufferList to store input.
        let mut input_buffer_list = AudioBufferList::default();
        input_buffer_list.mBuffers[0].mDataByteSize =
            stm.core_stream_data.input_desc.mBytesPerFrame * input_frames;
        input_buffer_list.mBuffers[0].mData = ptr::null_mut();
        input_buffer_list.mBuffers[0].mNumberChannels =
            stm.core_stream_data.input_desc.mChannelsPerFrame;
        input_buffer_list.mNumberBuffers = 1;

        assert!(!stm.core_stream_data.input_unit.is_null());
        let status = audio_unit_render(
            stm.core_stream_data.input_unit,
            flags,
            tstamp,
            bus,
            input_frames,
            &mut input_buffer_list,
        );
        if (status != NO_ERR)
            && (status != kAudioUnitErr_CannotDoInCurrentContext
                || stm.core_stream_data.output_unit.is_null())
        {
            return (ErrorHandle::Return(status), None);
        }
        let handle = if status == kAudioUnitErr_CannotDoInCurrentContext {
            assert!(!stm.core_stream_data.output_unit.is_null());
            // kAudioUnitErr_CannotDoInCurrentContext is returned when using a BT
            // headset and the profile is changed from A2DP to HFP/HSP. The previous
            // output device is no longer valid and must be reset.
            // For now state that no error occurred and feed silence, stream will be
            // resumed once reinit has completed.
            cubeb_logv!(
                "({:p}) input: reinit pending feeding silence instead",
                stm.core_stream_data.stm_ptr
            );
            let elements =
                (input_frames * stm.core_stream_data.input_desc.mChannelsPerFrame) as usize;
            stm.core_stream_data
                .input_linear_buffer
                .as_mut()
                .unwrap()
                .push_zeros(elements);
            ErrorHandle::Reinit
        } else {
            assert_eq!(status, NO_ERR);
            // Copy input data in linear buffer.
            let elements =
                (input_frames * stm.core_stream_data.input_desc.mChannelsPerFrame) as usize;
            stm.core_stream_data
                .input_linear_buffer
                .as_mut()
                .unwrap()
                .push(input_buffer_list.mBuffers[0].mData, elements);
            ErrorHandle::Return(status)
        };

        // Advance input frame counter.
        stm.frames_read
            .fetch_add(i64::from(input_frames), atomic::Ordering::SeqCst);

        cubeb_logv!(
            "({:p}) input: buffers {}, size {}, channels {}, rendered frames {}, total frames {}.",
            stm.core_stream_data.stm_ptr,
            input_buffer_list.mNumberBuffers,
            input_buffer_list.mBuffers[0].mDataByteSize,
            input_buffer_list.mBuffers[0].mNumberChannels,
            input_frames,
            stm.core_stream_data
                .input_linear_buffer
                .as_ref()
                .unwrap()
                .elements()
                / stm.core_stream_data.input_desc.mChannelsPerFrame as usize
        );

        // Full Duplex. We'll call data_callback in the AudioUnit output callback.
        if !stm.core_stream_data.output_unit.is_null() {
            return (handle, None);
        }

        // Input only. Call the user callback through resampler.
        // Resampler will deliver input buffer in the correct rate.
        assert!(
            input_frames as usize
                <= stm
                    .core_stream_data
                    .input_linear_buffer
                    .as_ref()
                    .unwrap()
                    .elements()
                    / stm.core_stream_data.input_desc.mChannelsPerFrame as usize
        );
        let mut total_input_frames =
            (stm.core_stream_data
                .input_linear_buffer
                .as_ref()
                .unwrap()
                .elements()
                / stm.core_stream_data.input_desc.mChannelsPerFrame as usize) as i64;
        assert!(!stm
            .core_stream_data
            .input_linear_buffer
            .as_ref()
            .unwrap()
            .as_ptr()
            .is_null());
        let input_buffer = stm
            .core_stream_data
            .input_linear_buffer
            .as_mut()
            .unwrap()
            .as_mut_ptr();
        let outframes = stm.core_stream_data.resampler.fill(
            input_buffer,
            &mut total_input_frames,
            ptr::null_mut(),
            0,
        );
        if outframes < total_input_frames {
            assert_eq!(
                audio_output_unit_stop(stm.core_stream_data.input_unit),
                NO_ERR
            );
            return (handle, Some(State::Drained));
        }
        // Reset input buffer
        stm.core_stream_data
            .input_linear_buffer
            .as_mut()
            .unwrap()
            .clear();

        (handle, None)
    };

    let (handle, notification) = handler(stm, flags, tstamp, bus, input_frames);
    if let Some(state) = notification {
        stm.notify_state_changed(state);
    }
    let status = match handle {
        ErrorHandle::Reinit => {
            stm.reinit_async();
            NO_ERR
        }
        ErrorHandle::Return(s) => s,
    };
    status
}

fn host_time_to_ns(host_time: u64) -> u64 {
    let mut rv: f64 = host_time as f64;
    rv *= HOST_TIME_TO_NS_RATIO.0 as f64;
    rv /= HOST_TIME_TO_NS_RATIO.1 as f64;
    return rv as u64;
}

fn compute_output_latency(stm: &AudioUnitStream, host_time: u64) -> u32 {
    let now = host_time_to_ns(unsafe { mach_absolute_time() });
    let audio_output_time = host_time_to_ns(host_time);
    let output_latency_ns = (audio_output_time - now) as u64;

    const NS2S: u64 = 1_000_000_000;
    // The total output latency is the timestamp difference + the stream latency +
    // the hardware latency.
    let out_hw_rate = stm.core_stream_data.output_hw_rate as u64;
    (output_latency_ns * out_hw_rate / NS2S
        + stm.current_latency_frames.load(Ordering::SeqCst) as u64) as u32
}

extern "C" fn audiounit_output_callback(
    user_ptr: *mut c_void,
    _: *mut AudioUnitRenderActionFlags,
    tstamp: *const AudioTimeStamp,
    bus: u32,
    output_frames: u32,
    out_buffer_list: *mut AudioBufferList,
) -> OSStatus {
    assert_eq!(bus, AU_OUT_BUS);
    assert!(!out_buffer_list.is_null());

    assert!(!user_ptr.is_null());
    let stm = unsafe { &mut *(user_ptr as *mut AudioUnitStream) };

    let out_buffer_list_ref = unsafe { &mut (*out_buffer_list) };
    assert_eq!(out_buffer_list_ref.mNumberBuffers, 1);
    let mut buffers = unsafe {
        let ptr = out_buffer_list_ref.mBuffers.as_mut_ptr();
        let len = out_buffer_list_ref.mNumberBuffers as usize;
        slice::from_raw_parts_mut(ptr, len)
    };

    let output_latency_frames = compute_output_latency(&stm, unsafe { (*tstamp).mHostTime });

    stm.total_output_latency_frames
        .store(output_latency_frames, Ordering::SeqCst);

    cubeb_logv!(
        "({:p}) output: buffers {}, size {}, channels {}, frames {}.",
        stm as *const AudioUnitStream,
        buffers.len(),
        buffers[0].mDataByteSize,
        buffers[0].mNumberChannels,
        output_frames
    );

    if stm.shutdown.load(Ordering::SeqCst) {
        cubeb_log!("({:p}) output shutdown.", stm as *const AudioUnitStream);
        audiounit_make_silent(&mut buffers[0]);
        return NO_ERR;
    }

    if stm.draining.load(Ordering::SeqCst) {
        stm.core_stream_data.stop_audiounits();
        stm.notify_state_changed(State::Drained);
        audiounit_make_silent(&mut buffers[0]);
        return NO_ERR;
    }

    let handler = |stm: &mut AudioUnitStream,
                   output_frames: u32,
                   buffers: &mut [AudioBuffer]|
     -> (OSStatus, Option<State>) {
        // Get output buffer
        let output_buffer = match stm.core_stream_data.mixer.as_mut() {
            None => buffers[0].mData,
            Some(mixer) => {
                // If remixing needs to occur, we can't directly work in our final
                // destination buffer as data may be overwritten or too small to start with.
                mixer.update_buffer_size(output_frames as usize);
                mixer.get_buffer_mut_ptr() as *mut c_void
            }
        };

        stm.frames_written
            .fetch_add(i64::from(output_frames), Ordering::SeqCst);

        // Also get the input buffer if the stream is duplex
        let (input_buffer, mut input_frames) = if !stm.core_stream_data.input_unit.is_null() {
            assert!(stm.core_stream_data.input_linear_buffer.is_some());
            assert_ne!(stm.core_stream_data.input_desc.mChannelsPerFrame, 0);
            // If the output callback came first and this is a duplex stream, we need to
            // fill in some additional silence in the resampler.
            // Otherwise, if we had more than expected callbacks in a row, or we're
            // currently switching, we add some silence as well to compensate for the
            // fact that we're lacking some input data.
            let frames_written = stm.frames_written.load(Ordering::SeqCst);
            let input_frames_needed = minimum_resampling_input_frames(
                stm.core_stream_data.input_hw_rate,
                f64::from(stm.core_stream_data.output_stream_params.rate()),
                frames_written,
            );
            let missing_frames = input_frames_needed - stm.frames_read.load(Ordering::SeqCst);
            let elements = (missing_frames
                * i64::from(stm.core_stream_data.input_desc.mChannelsPerFrame))
                as usize;
            if missing_frames > 0 {
                stm.core_stream_data
                    .input_linear_buffer
                    .as_mut()
                    .unwrap()
                    .push_zeros(elements);
                stm.frames_read.store(input_frames_needed, Ordering::SeqCst);
                cubeb_log!(
                    "({:p}) {} pushed {} frames of input silence.",
                    stm.core_stream_data.stm_ptr,
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
            let input_frames = stm
                .core_stream_data
                .input_linear_buffer
                .as_ref()
                .unwrap()
                .elements()
                / stm.core_stream_data.input_desc.mChannelsPerFrame as usize;
            cubeb_logv!("Total input frames: {}", input_frames);
            (
                stm.core_stream_data
                    .input_linear_buffer
                    .as_mut()
                    .unwrap()
                    .as_mut_ptr(),
                input_frames as i64,
            )
        } else {
            (ptr::null_mut::<c_void>(), 0)
        };

        // Call user callback through resampler.
        assert!(!output_buffer.is_null());
        let outframes = stm.core_stream_data.resampler.fill(
            input_buffer,
            if input_buffer.is_null() {
                ptr::null_mut()
            } else {
                &mut input_frames
            },
            output_buffer,
            i64::from(output_frames),
        );
        if !input_buffer.is_null() {
            // Pop from the buffer the frames used by the the resampler.
            let elements =
                input_frames as usize * stm.core_stream_data.input_desc.mChannelsPerFrame as usize;
            stm.core_stream_data
                .input_linear_buffer
                .as_mut()
                .unwrap()
                .pop(elements);
        }

        if outframes < 0 || outframes > i64::from(output_frames) {
            *stm.shutdown.get_mut() = true;
            stm.core_stream_data.stop_audiounits();
            audiounit_make_silent(&mut buffers[0]);
            return (NO_ERR, Some(State::Error));
        }

        *stm.draining.get_mut() = outframes < i64::from(output_frames);
        stm.frames_played
            .store(stm.frames_queued, atomic::Ordering::SeqCst);
        stm.frames_queued += outframes as u64;

        let outaff = stm.core_stream_data.output_desc.mFormatFlags;
        let panning = if stm.core_stream_data.output_desc.mChannelsPerFrame == 2 {
            stm.panning.load(Ordering::Relaxed)
        } else {
            0.0
        };

        // Post process output samples.
        if stm.draining.load(Ordering::SeqCst) {
            // Clear missing frames (silence)
            let count_bytes = |frames: usize| -> usize {
                let sample_size =
                    cubeb_sample_size(stm.core_stream_data.output_stream_params.format());
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
        if stm.core_stream_data.mixer.is_none() {
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
            assert!(
                buffers[0].mDataByteSize
                    >= stm.core_stream_data.output_desc.mBytesPerFrame * output_frames
            );
            stm.core_stream_data.mixer.as_mut().unwrap().mix(
                output_frames as usize,
                buffers[0].mData,
                buffers[0].mDataByteSize as usize,
            );
        }

        (NO_ERR, None)
    };

    let (status, notification) = handler(stm, output_frames, &mut buffers);
    if let Some(state) = notification {
        stm.notify_state_changed(state);
    }
    status
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
                    .core_stream_data
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
        let callback = stm.device_changed_callback.lock().unwrap();
        if let Some(device_changed_callback) = *callback {
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

fn start_audiounit(unit: AudioUnit) -> Result<()> {
    let status = audio_output_unit_start(unit);
    if status == NO_ERR {
        Ok(())
    } else {
        cubeb_log!("Cannot start audiounit @ {:p}. Error: {}", unit, status);
        Err(Error::error())
    }
}

fn stop_audiounit(unit: AudioUnit) -> Result<()> {
    let status = audio_output_unit_stop(unit);
    if status == NO_ERR {
        Ok(())
    } else {
        cubeb_log!("Cannot stop audiounit @ {:p}. Error: {}", unit, status);
        Err(Error::error())
    }
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

fn get_buffer_size(unit: AudioUnit, side: io_side) -> std::result::Result<u32, OSStatus> {
    assert!(!unit.is_null());
    let (scope, element) = match side {
        io_side::INPUT => (kAudioUnitScope_Output, AU_IN_BUS),
        io_side::OUTPUT => (kAudioUnitScope_Input, AU_OUT_BUS),
    };
    let mut frames: u32 = 0;
    let mut size = mem::size_of::<u32>();
    let status = audio_unit_get_property(
        unit,
        kAudioDevicePropertyBufferFrameSize,
        scope,
        element,
        &mut frames,
        &mut size,
    );
    if status == NO_ERR {
        assert_ne!(frames, 0);
        Ok(frames)
    } else {
        Err(status)
    }
}

fn set_buffer_size(
    unit: AudioUnit,
    side: io_side,
    frames: u32,
) -> std::result::Result<(), OSStatus> {
    assert!(!unit.is_null());
    let (scope, element) = match side {
        io_side::INPUT => (kAudioUnitScope_Output, AU_IN_BUS),
        io_side::OUTPUT => (kAudioUnitScope_Input, AU_OUT_BUS),
    };
    let status = audio_unit_set_property(
        unit,
        kAudioDevicePropertyBufferFrameSize,
        scope,
        element,
        &frames,
        mem::size_of_val(&frames),
    );
    if status == NO_ERR {
        Ok(())
    } else {
        Err(status)
    }
}

fn set_buffer_size_sync(unit: AudioUnit, side: io_side, frames: u32) -> Result<()> {
    let current_frames = get_buffer_size(unit, side.clone()).map_err(|r| {
        cubeb_log!(
            "AudioUnitGetProperty/{}/kAudioDevicePropertyBufferFrameSize rv={}",
            side.to_string(),
            r
        );
        Error::error()
    })?;
    if frames == current_frames {
        cubeb_log!(
            "The size of {} buffer frames is already {}",
            side.to_string(),
            frames
        );
        return Ok(());
    }

    let waiting_time = Duration::from_millis(100);
    let pair = Arc::new((Mutex::new(false), Condvar::new()));
    let mut pair2 = pair.clone();
    let pair_ptr = &mut pair2;

    assert_eq!(
        audio_unit_add_property_listener(
            unit,
            kAudioDevicePropertyBufferFrameSize,
            buffer_size_changed_callback,
            pair_ptr,
        ),
        NO_ERR
    );

    let _teardown = finally(|| {
        assert_eq!(
            audio_unit_remove_property_listener_with_user_data(
                unit,
                kAudioDevicePropertyBufferFrameSize,
                buffer_size_changed_callback,
                pair_ptr,
            ),
            NO_ERR
        );
    });

    set_buffer_size(unit, side.clone(), frames).map_err(|r| {
        cubeb_log!(
            "AudioUnitSetProperty/{}/kAudioDevicePropertyBufferFrameSize rv={}",
            side.to_string(),
            r
        );
        Error::error()
    })?;

    let &(ref lock, ref cvar) = &*pair;
    let changed = lock.lock().unwrap();
    if !*changed {
        let (chg, timeout_res) = cvar.wait_timeout(changed, waiting_time).unwrap();
        if timeout_res.timed_out() {
            cubeb_log!(
                "Time out for waiting the {} buffer size setting!",
                side.to_string()
            );
        }
        if !*chg {
            return Err(Error::error());
        }
    }

    let new_frames = get_buffer_size(unit, side.clone()).map_err(|r| {
        cubeb_log!(
            "Cannot get new {} buffer size. Error: {}",
            side.to_string(),
            r
        );
        Error::error()
    })?;
    cubeb_log!(
        "The new size of {} buffer frames is {}",
        side.to_string(),
        new_frames
    );

    extern "C" fn buffer_size_changed_callback(
        in_client_data: *mut c_void,
        _in_unit: AudioUnit,
        in_property_id: AudioUnitPropertyID,
        in_scope: AudioUnitScope,
        in_element: AudioUnitElement,
    ) {
        if in_scope == 0 {
            // filter out the callback for global scope.
            return;
        }
        assert!(in_element == AU_IN_BUS || in_element == AU_OUT_BUS);
        assert_eq!(in_property_id, kAudioDevicePropertyBufferFrameSize);
        let pair = unsafe { &mut *(in_client_data as *mut Arc<(Mutex<bool>, Condvar)>) };
        let &(ref lock, ref cvar) = &**pair;
        let mut changed = lock.lock().unwrap();
        *changed = true;
        cvar.notify_one();
    }

    Ok(())
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

fn audiounit_get_default_datasource(side: io_side) -> Result<u32> {
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

fn get_channel_count(devid: AudioObjectID, devtype: DeviceType) -> Result<u32> {
    assert_ne!(devid, kAudioObjectUnknown);

    let adr = AudioObjectPropertyAddress {
        mSelector: kAudioDevicePropertyStreamConfiguration,
        mScope: match devtype {
            DeviceType::INPUT => kAudioDevicePropertyScopeInput,
            DeviceType::OUTPUT => kAudioDevicePropertyScopeOutput,
            _ => panic!("Invalid type"),
        },
        mElement: kAudioObjectPropertyElementMaster,
    };

    let mut size: usize = 0;
    let r = audio_object_get_property_data_size(devid, &adr, &mut size);
    if r != NO_ERR {
        return Err(Error::error());
    }
    assert_ne!(size, 0);

    let mut data: Vec<u8> = allocate_array_by_size(size);
    let ptr = data.as_mut_ptr() as *mut AudioBufferList;
    let r = audio_object_get_property_data(devid, &adr, &mut size, ptr);
    if r != NO_ERR {
        return Err(Error::error());
    }

    let list = unsafe { &(*ptr) };
    let ptr = list.mBuffers.as_ptr() as *const AudioBuffer;
    let len = list.mNumberBuffers as usize;

    let mut count = 0;
    let buffers = unsafe { slice::from_raw_parts(ptr, len) };
    for buffer in buffers {
        count += buffer.mNumberChannels;
    }
    Ok(count)
}

fn audiounit_get_available_samplerate(
    devid: AudioObjectID,
    devtype: DeviceType,
    min: &mut u32,
    max: &mut u32,
    def: &mut u32,
) {
    const GLOBAL: ffi::cubeb_device_type =
        ffi::CUBEB_DEVICE_TYPE_INPUT | ffi::CUBEB_DEVICE_TYPE_OUTPUT;
    let mut adr = AudioObjectPropertyAddress {
        mSelector: 0,
        mScope: match devtype.bits() {
            ffi::CUBEB_DEVICE_TYPE_INPUT => kAudioDevicePropertyScopeInput,
            ffi::CUBEB_DEVICE_TYPE_OUTPUT => kAudioDevicePropertyScopeOutput,
            GLOBAL => kAudioObjectPropertyScopeGlobal,
            _ => panic!("Invalid type"),
        },
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

fn audiounit_get_device_presentation_latency(devid: AudioObjectID, devtype: DeviceType) -> u32 {
    const GLOBAL: ffi::cubeb_device_type =
        ffi::CUBEB_DEVICE_TYPE_INPUT | ffi::CUBEB_DEVICE_TYPE_OUTPUT;
    let mut adr = AudioObjectPropertyAddress {
        mSelector: 0,
        mScope: match devtype.bits() {
            ffi::CUBEB_DEVICE_TYPE_INPUT => kAudioDevicePropertyScopeInput,
            ffi::CUBEB_DEVICE_TYPE_OUTPUT => kAudioDevicePropertyScopeOutput,
            GLOBAL => kAudioObjectPropertyScopeGlobal,
            _ => panic!("Invalid type"),
        },
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

fn create_cubeb_device_info(
    devid: AudioObjectID,
    devtype: DeviceType,
) -> Result<ffi::cubeb_device_info> {
    let channels = get_channel_count(devid, devtype)?;
    if channels == 0 {
        // Invalid type for this device.
        return Err(Error::error());
    }

    let mut dev_info = ffi::cubeb_device_info::default();
    dev_info.max_channels = channels;

    assert!(
        mem::size_of::<ffi::cubeb_devid>() >= mem::size_of_val(&devid),
        "cubeb_devid can't represent devid"
    );
    dev_info.devid = devid as ffi::cubeb_devid;

    match get_device_uid(devid, devtype) {
        Ok(uid) => {
            let c_string = uid.into_cstring();
            dev_info.device_id = c_string.into_raw();
            dev_info.group_id = dev_info.device_id;
        }
        Err(e) => {
            cubeb_log!(
                "Cannot get the uid for device {} in {:?} scope. Error: {}",
                devid,
                devtype,
                e
            );
        }
    }

    let label = match get_device_label(devid, devtype) {
        Ok(label) => label.into_cstring(),
        Err(e) => {
            cubeb_log!(
                "Cannot get the label for device {} in {:?} scope. Error: {}",
                devid,
                devtype,
                e
            );
            CString::default()
        }
    };
    dev_info.friendly_name = label.into_raw();

    match get_device_manufacturer(devid, devtype) {
        Ok(vendor) => {
            let vendor = vendor.into_cstring();
            dev_info.vendor_name = vendor.into_raw();
        }
        Err(e) => {
            cubeb_log!(
                "Cannot get the manufacturer for device {} in {:?} scope. Error: {}",
                devid,
                devtype,
                e
            );
        }
    }

    dev_info.device_type = match devtype {
        DeviceType::INPUT => ffi::CUBEB_DEVICE_TYPE_INPUT,
        DeviceType::OUTPUT => ffi::CUBEB_DEVICE_TYPE_OUTPUT,
        _ => panic!("invalid type"),
    };

    dev_info.state = ffi::CUBEB_DEVICE_STATE_ENABLED;
    dev_info.preferred = if devid == audiounit_get_default_device_id(devtype) {
        ffi::CUBEB_DEVICE_PREF_ALL
    } else {
        ffi::CUBEB_DEVICE_PREF_NONE
    };

    dev_info.format = ffi::CUBEB_DEVICE_FMT_ALL;
    dev_info.default_format = ffi::CUBEB_DEVICE_FMT_F32NE;
    audiounit_get_available_samplerate(
        devid,
        devtype,
        &mut dev_info.min_rate,
        &mut dev_info.max_rate,
        &mut dev_info.default_rate,
    );

    let latency = audiounit_get_device_presentation_latency(devid, devtype);

    let (latency_low, latency_high) = match get_device_buffer_frame_size_range(devid, devtype) {
        Ok((min, max)) => (latency + min as u32, latency + max as u32),
        Err(e) => {
            cubeb_log!("Cannot get the buffer frame size for device {} in {:?} scope. Use default value instead. Error: {}", devid, devtype, e);
            (
                10 * dev_info.default_rate / 1000,
                100 * dev_info.default_rate / 1000,
            )
        }
    };
    dev_info.latency_lo = latency_low;
    dev_info.latency_hi = latency_high;

    Ok(dev_info)
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
        if let Ok(uid) = get_device_global_uid(device) {
            let uid = uid.into_string();
            uid != PRIVATE_AGGREGATE_DEVICE_NAME
        } else {
            // Fail to get device uid.
            true
        }
    });

    // Expected sorted but did not find anything in the docs.
    devices.sort();
    if devtype.contains(DeviceType::INPUT | DeviceType::OUTPUT) {
        return devices;
    }

    let mut devices_in_scope = Vec::new();
    for device in devices {
        if get_channel_count(device, devtype).unwrap() > 0 {
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
    latency_controller: Mutex<LatencyController>,
    devices: Mutex<SharedDevices>,
}

impl AudioUnitContext {
    fn new() -> Self {
        Self {
            _ops: &OPS as *const _,
            serial_queue: create_dispatch_queue(DISPATCH_QUEUE_LABEL, DISPATCH_QUEUE_SERIAL),
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
        let mut device_infos = Vec::new();
        let dev_types = [DeviceType::INPUT, DeviceType::OUTPUT];
        for dev_type in dev_types.iter() {
            if !devtype.contains(*dev_type) {
                continue;
            }
            let devices = audiounit_get_devices_of_type(*dev_type);
            for device in devices {
                if let Ok(info) = create_cubeb_device_info(device, *dev_type) {
                    if !is_aggregate_device(&info) {
                        device_infos.push(info);
                    }
                }
            }
        }
        let (ptr, len) = if device_infos.is_empty() {
            (ptr::null_mut(), 0)
        } else {
            forget_vec(device_infos)
        };
        let coll = unsafe { &mut *collection.as_ptr() };
        coll.device = ptr;
        coll.count = len;
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

        let in_stm_settings = if let Some(params) = input_stream_params {
            let in_device = create_device_info(input_device as AudioDeviceID, DeviceType::INPUT)
                .map_err(|e| {
                    cubeb_log!("Fail to create device info for input.");
                    e
                })?;
            let stm_params = StreamParams::from(unsafe { (*params.as_ptr()) });
            Some((stm_params, in_device))
        } else {
            None
        };

        let out_stm_settings = if let Some(params) = output_stream_params {
            let out_device = create_device_info(output_device as AudioDeviceID, DeviceType::OUTPUT)
                .map_err(|e| {
                    cubeb_log!("Fail to create device info for output.");
                    e
                })?;
            let stm_params = StreamParams::from(unsafe { (*params.as_ptr()) });
            Some((stm_params, out_device))
        } else {
            None
        };

        let mut boxed_stream = Box::new(AudioUnitStream::new(
            self,
            user_ptr,
            data_callback,
            state_callback,
            global_latency_frames,
        ));

        boxed_stream.core_stream_data =
            CoreStreamData::new(boxed_stream.as_ref(), in_stm_settings, out_stm_settings);

        if let Err(r) = boxed_stream.core_stream_data.setup() {
            cubeb_log!(
                "({:p}) Could not setup the audiounit stream.",
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

#[derive(Debug)]
struct CoreStreamData<'ctx> {
    stm_ptr: *const AudioUnitStream<'ctx>,
    aggregate_device: AggregateDevice,
    mixer: Option<Mixer>,
    resampler: Resampler,
    // Stream creation parameters.
    input_stream_params: StreamParams,
    output_stream_params: StreamParams,
    // Format descriptions.
    input_desc: AudioStreamBasicDescription,
    output_desc: AudioStreamBasicDescription,
    // I/O AudioUnits.
    input_unit: AudioUnit,
    output_unit: AudioUnit,
    // Info of the I/O devices.
    input_device: device_info,
    output_device: device_info,
    // Sample rates of the I/O devices.
    input_hw_rate: f64,
    output_hw_rate: f64,
    // Channel layout of the output AudioUnit.
    device_layout: ChannelLayout,
    // Hold the input samples in every input callback iteration.
    // Only accessed on input/output callback thread and during initial configure.
    input_linear_buffer: Option<Box<dyn AutoArrayWrapper>>,
    // Listeners indicating what system events are monitored.
    default_input_listener: Option<device_property_listener>,
    default_output_listener: Option<device_property_listener>,
    input_alive_listener: Option<device_property_listener>,
    input_source_listener: Option<device_property_listener>,
    output_source_listener: Option<device_property_listener>,
}

impl<'ctx> Default for CoreStreamData<'ctx> {
    fn default() -> Self {
        Self {
            stm_ptr: ptr::null(),
            aggregate_device: AggregateDevice::default(),
            mixer: None,
            resampler: Resampler::default(),
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
            input_desc: AudioStreamBasicDescription::default(),
            output_desc: AudioStreamBasicDescription::default(),
            input_unit: ptr::null_mut(),
            output_unit: ptr::null_mut(),
            input_device: device_info::default(),
            output_device: device_info::default(),
            input_hw_rate: 0_f64,
            output_hw_rate: 0_f64,
            device_layout: ChannelLayout::UNDEFINED,
            input_linear_buffer: None,
            default_input_listener: None,
            default_output_listener: None,
            input_alive_listener: None,
            input_source_listener: None,
            output_source_listener: None,
        }
    }
}

impl<'ctx> CoreStreamData<'ctx> {
    fn new(
        stm: &AudioUnitStream<'ctx>,
        input_stream_settings: Option<(StreamParams, device_info)>,
        output_stream_settings: Option<(StreamParams, device_info)>,
    ) -> Self {
        fn get_default_sttream_params() -> StreamParams {
            StreamParams::from(ffi::cubeb_stream_params {
                format: ffi::CUBEB_SAMPLE_FLOAT32NE,
                rate: 0,
                channels: 0,
                layout: ffi::CUBEB_LAYOUT_UNDEFINED,
                prefs: ffi::CUBEB_STREAM_PREF_NONE,
            })
        }
        let (in_stm_params, in_dev) =
            input_stream_settings.unwrap_or((get_default_sttream_params(), device_info::default()));
        let (out_stm_params, out_dev) = output_stream_settings
            .unwrap_or((get_default_sttream_params(), device_info::default()));
        Self {
            stm_ptr: stm,
            aggregate_device: AggregateDevice::default(),
            mixer: None,
            resampler: Resampler::default(),
            input_stream_params: in_stm_params,
            output_stream_params: out_stm_params,
            input_desc: AudioStreamBasicDescription::default(),
            output_desc: AudioStreamBasicDescription::default(),
            input_unit: ptr::null_mut(),
            output_unit: ptr::null_mut(),
            input_device: in_dev,
            output_device: out_dev,
            input_hw_rate: 0_f64,
            output_hw_rate: 0_f64,
            device_layout: ChannelLayout::UNDEFINED,
            input_linear_buffer: None,
            default_input_listener: None,
            default_output_listener: None,
            input_alive_listener: None,
            input_source_listener: None,
            output_source_listener: None,
        }
    }

    fn start_audiounits(&self) -> Result<()> {
        if !self.input_unit.is_null() {
            start_audiounit(self.input_unit)?;
        }
        if !self.output_unit.is_null() {
            start_audiounit(self.output_unit)?;
        }
        Ok(())
    }

    fn stop_audiounits(&self) {
        if !self.input_unit.is_null() {
            assert!(stop_audiounit(self.input_unit).is_ok());
        }
        if !self.output_unit.is_null() {
            assert!(stop_audiounit(self.output_unit).is_ok());
        }
    }

    fn has_input(&self) -> bool {
        self.input_stream_params.rate() > 0
    }

    fn has_output(&self) -> bool {
        self.output_stream_params.rate() > 0
    }

    fn setup(&mut self) -> Result<()> {
        if self
            .input_stream_params
            .prefs()
            .contains(StreamPrefs::LOOPBACK)
            || self
                .output_stream_params
                .prefs()
                .contains(StreamPrefs::LOOPBACK)
        {
            cubeb_log!("({:p}) Loopback not supported for audiounit.", self.stm_ptr);
            return Err(Error::not_supported());
        }

        let mut in_dev_info = self.input_device.clone();
        let mut out_dev_info = self.output_device.clone();

        if self.has_input() && self.has_output() && in_dev_info.id != out_dev_info.id {
            match AggregateDevice::new(in_dev_info.id, out_dev_info.id) {
                Ok(device) => {
                    in_dev_info.id = device.get_device_id();
                    out_dev_info.id = device.get_device_id();
                    in_dev_info.flags = device_flags::DEV_INPUT;
                    out_dev_info.flags = device_flags::DEV_OUTPUT;
                    self.aggregate_device = device;
                }
                Err(status) => {
                    cubeb_log!(
                        "({:p}) Create aggregate devices failed. Error: {}",
                        self.stm_ptr,
                        status
                    );
                    // !!!NOTE: It is not necessary to return here. If it does not
                    // return it will fallback to the old implementation. The intention
                    // is to investigate how often it fails. I plan to remove
                    // it after a couple of weeks.
                }
            }
        }

        assert!(!self.stm_ptr.is_null());
        let stream = unsafe { &(*self.stm_ptr) };

        // Configure I/O stream
        if self.has_input() {
            self.input_unit = create_audiounit(&in_dev_info).map_err(|e| {
                cubeb_log!("({:p}) AudioUnit creation for input failed.", self.stm_ptr);
                e
            })?;

            cubeb_log!(
                "({:p}) Opening input side: rate {}, channels {}, format {:?}, layout {:?}, prefs {:?}, latency in frames {}.",
                self.stm_ptr,
                self.input_stream_params.rate(),
                self.input_stream_params.channels(),
                self.input_stream_params.format(),
                self.input_stream_params.layout(),
                self.input_stream_params.prefs(),
                stream.latency_frames
            );

            // Get input device sample rate.
            let mut input_hw_desc = AudioStreamBasicDescription::default();
            let mut size = mem::size_of::<AudioStreamBasicDescription>();
            let r = audio_unit_get_property(
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
            cubeb_log!(
                "({:p}) Input hardware description: {:?}",
                self.stm_ptr,
                input_hw_desc
            );
            self.input_hw_rate = input_hw_desc.mSampleRate;

            // Set format description according to the input params.
            self.input_desc =
                create_stream_description(&self.input_stream_params).map_err(|e| {
                    cubeb_log!(
                        "({:p}) Setting format description for input failed.",
                        self.stm_ptr
                    );
                    e
                })?;

            // Use latency to set buffer size
            assert_ne!(stream.latency_frames, 0);
            if let Err(r) =
                set_buffer_size_sync(self.input_unit, io_side::INPUT, stream.latency_frames)
            {
                cubeb_log!("({:p}) Error in change input buffer size.", self.stm_ptr);
                return Err(r);
            }

            let mut src_desc = self.input_desc;
            // Input AudioUnit must be configured with device's sample rate.
            // we will resample inside input callback.
            src_desc.mSampleRate = self.input_hw_rate;
            let r = audio_unit_set_property(
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
            let r = audio_unit_set_property(
                self.input_unit,
                kAudioUnitProperty_MaximumFramesPerSlice,
                kAudioUnitScope_Global,
                AU_IN_BUS,
                &stream.latency_frames,
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
                stream.latency_frames,
                array_capacity,
            )?);

            let aurcbs_in = AURenderCallbackStruct {
                inputProc: Some(audiounit_input_callback),
                inputProcRefCon: self.stm_ptr as *mut c_void,
            };

            let r = audio_unit_set_property(
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

            stream.frames_read.store(0, Ordering::SeqCst);

            cubeb_log!(
                "({:p}) Input audiounit init with device {} successfully.",
                self.stm_ptr,
                in_dev_info.id
            );
        }

        if self.has_output() {
            self.output_unit = create_audiounit(&out_dev_info).map_err(|e| {
                cubeb_log!("({:p}) AudioUnit creation for output failed.", self.stm_ptr);
                e
            })?;

            cubeb_log!(
                "({:p}) Opening output side: rate {}, channels {}, format {:?}, layout {:?}, prefs {:?}, latency in frames {}.",
                self.stm_ptr,
                self.output_stream_params.rate(),
                self.output_stream_params.channels(),
                self.output_stream_params.format(),
                self.output_stream_params.layout(),
                self.output_stream_params.prefs(),
                stream.latency_frames
            );

            self.output_desc =
                create_stream_description(&self.output_stream_params).map_err(|e| {
                    cubeb_log!(
                        "({:p}) Could not initialize the audio stream description.",
                        self.stm_ptr
                    );
                    e
                })?;

            // Get output device sample rate.
            let mut output_hw_desc = AudioStreamBasicDescription::default();
            let mut size = mem::size_of::<AudioStreamBasicDescription>();
            let r = audio_unit_get_property(
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
            cubeb_log!(
                "({:p}) Output hardware description: {:?}",
                self.stm_ptr,
                output_hw_desc
            );
            self.output_hw_rate = output_hw_desc.mSampleRate;
            let hw_channels = output_hw_desc.mChannelsPerFrame;

            // Set the input layout to match the output device layout.
            self.device_layout = audiounit_get_current_channel_layout(self.output_unit);
            audiounit_set_channel_layout(self.output_unit, io_side::OUTPUT, self.device_layout);
            cubeb_log!(
                "({:p}) Output hardware layout: {:?}",
                self.stm_ptr,
                self.device_layout
            );

            self.mixer = if hw_channels != self.output_stream_params.channels()
                || self.device_layout != self.output_stream_params.layout()
            {
                cubeb_log!("Incompatible channel layouts detected, setting up remixer");
                // We will be remixing the data before it reaches the output device.
                // We need to adjust the number of channels and other
                // AudioStreamDescription details.
                self.output_desc.mChannelsPerFrame = hw_channels;
                self.output_desc.mBytesPerFrame =
                    (self.output_desc.mBitsPerChannel / 8) * self.output_desc.mChannelsPerFrame;
                self.output_desc.mBytesPerPacket =
                    self.output_desc.mBytesPerFrame * self.output_desc.mFramesPerPacket;
                Some(Mixer::new(
                    self.output_stream_params.format(),
                    self.output_stream_params.channels(),
                    self.output_stream_params.layout(),
                    hw_channels,
                    self.device_layout,
                ))
            } else {
                None
            };

            let r = audio_unit_set_property(
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
            assert_ne!(stream.latency_frames, 0);
            if let Err(r) =
                set_buffer_size_sync(self.output_unit, io_side::OUTPUT, stream.latency_frames)
            {
                cubeb_log!("({:p}) Error in change output buffer size.", self.stm_ptr);
                return Err(r);
            }

            // Frames per buffer in the input callback.
            let r = audio_unit_set_property(
                self.output_unit,
                kAudioUnitProperty_MaximumFramesPerSlice,
                kAudioUnitScope_Global,
                AU_OUT_BUS,
                &stream.latency_frames,
                mem::size_of::<u32>(),
            );
            if r != NO_ERR {
                cubeb_log!(
                    "AudioUnitSetProperty/output/kAudioUnitProperty_MaximumFramesPerSlice rv={}",
                    r
                );
                return Err(Error::error());
            }

            let aurcbs_out = AURenderCallbackStruct {
                inputProc: Some(audiounit_output_callback),
                inputProcRefCon: self.stm_ptr as *mut c_void,
            };
            let r = audio_unit_set_property(
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

            stream.frames_written.store(0, Ordering::SeqCst);

            cubeb_log!(
                "({:p}) Output audiounit init with device {} successfully.",
                self.stm_ptr,
                out_dev_info.id
            );
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

        let resampler_input_params = if self.has_input() {
            let mut params = unsafe { (*(self.input_stream_params.as_ptr())) };
            params.rate = self.input_hw_rate as u32;
            Some(params)
        } else {
            None
        };
        let resampler_output_params = if self.has_output() {
            let params = unsafe { (*(self.output_stream_params.as_ptr())) };
            Some(params)
        } else {
            None
        };

        self.resampler = Resampler::new(
            self.stm_ptr as *mut ffi::cubeb_stream,
            resampler_input_params,
            resampler_output_params,
            target_sample_rate,
            stream.data_callback,
            stream.user_ptr,
        );

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

            stream.current_latency_frames.store(
                audiounit_get_device_presentation_latency(
                    self.output_device.id,
                    DeviceType::OUTPUT,
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
                stream.current_latency_frames.fetch_add(
                    (unit_s * self.output_desc.mSampleRate) as u32,
                    Ordering::SeqCst,
                );
            }
        }

        if let Err(r) = self.install_system_changed_callback() {
            cubeb_log!(
                "({:p}) Could not install the device change callback.",
                self.stm_ptr
            );
            return Err(r);
        }

        if let Err(r) = self.install_device_changed_callback() {
            cubeb_log!(
                "({:p}) Could not install all device change callback.",
                self.stm_ptr
            );
            return Err(r);
        }

        Ok(())
    }

    fn close(&mut self) {
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

        self.resampler.destroy();
        self.mixer = None;
        self.aggregate_device = AggregateDevice::default();

        if self.uninstall_system_changed_callback().is_err() {
            cubeb_log!(
                "({:p}) Could not uninstall the system changed callback",
                self.stm_ptr
            );
        }

        if self.uninstall_device_changed_callback().is_err() {
            cubeb_log!(
                "({:p}) Could not uninstall all device change listeners",
                self.stm_ptr
            );
        }
    }

    fn install_device_changed_callback(&mut self) -> Result<()> {
        assert!(!self.stm_ptr.is_null());
        let stm = unsafe { &(*self.stm_ptr) };

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
            let rv = stm.add_device_listener(self.output_source_listener.as_ref().unwrap());
            if rv != NO_ERR {
                self.output_source_listener = None;
                cubeb_log!("AudioObjectAddPropertyListener/output/kAudioDevicePropertyDataSource rv={}, device id={}", rv, self.output_device.id);
                return Err(Error::error());
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
            let rv = stm.add_device_listener(self.input_source_listener.as_ref().unwrap());
            if rv != NO_ERR {
                self.input_source_listener = None;
                cubeb_log!("AudioObjectAddPropertyListener/input/kAudioDevicePropertyDataSource rv={}, device id={}", rv, self.input_device.id);
                return Err(Error::error());
            }

            // Event to notify when the input is going away.
            self.input_alive_listener = Some(device_property_listener::new(
                self.input_device.id,
                &DEVICE_IS_ALIVE_PROPERTY_ADDRESS,
                audiounit_property_listener_callback,
            ));
            let rv = stm.add_device_listener(self.input_alive_listener.as_ref().unwrap());
            if rv != NO_ERR {
                self.input_alive_listener = None;
                cubeb_log!("AudioObjectAddPropertyListener/input/kAudioDevicePropertyDeviceIsAlive rv={}, device id ={}", rv, self.input_device.id);
                return Err(Error::error());
            }
        }

        Ok(())
    }

    fn install_system_changed_callback(&mut self) -> Result<()> {
        assert!(!self.stm_ptr.is_null());
        let stm = unsafe { &(*self.stm_ptr) };

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
            let r = stm.add_device_listener(self.default_output_listener.as_ref().unwrap());
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
            let r = stm.add_device_listener(self.default_input_listener.as_ref().unwrap());
            if r != NO_ERR {
                self.default_input_listener = None;
                cubeb_log!("AudioObjectAddPropertyListener/input/kAudioHardwarePropertyDefaultInputDevice rv={}", r);
                return Err(Error::error());
            }
        }

        Ok(())
    }

    fn uninstall_device_changed_callback(&mut self) -> Result<()> {
        if self.stm_ptr.is_null() {
            assert!(
                self.output_source_listener.is_none()
                    && self.input_source_listener.is_none()
                    && self.input_alive_listener.is_none()
            );
            return Ok(());
        }

        let stm = unsafe { &(*self.stm_ptr) };

        // Failing to uninstall listeners is not a fatal error.
        let mut r = Ok(());

        if self.output_source_listener.is_some() {
            let rv = stm.remove_device_listener(self.output_source_listener.as_ref().unwrap());
            if rv != NO_ERR {
                cubeb_log!("AudioObjectRemovePropertyListener/output/kAudioDevicePropertyDataSource rv={}, device id={}", rv, self.output_device.id);
                r = Err(Error::error());
            }
            self.output_source_listener = None;
        }

        if self.input_source_listener.is_some() {
            let rv = stm.remove_device_listener(self.input_source_listener.as_ref().unwrap());
            if rv != NO_ERR {
                cubeb_log!("AudioObjectRemovePropertyListener/input/kAudioDevicePropertyDataSource rv={}, device id={}", rv, self.input_device.id);
                r = Err(Error::error());
            }
            self.input_source_listener = None;
        }

        if self.input_alive_listener.is_some() {
            let rv = stm.remove_device_listener(self.input_alive_listener.as_ref().unwrap());
            if rv != NO_ERR {
                cubeb_log!("AudioObjectRemovePropertyListener/input/kAudioDevicePropertyDeviceIsAlive rv={}, device id={}", rv, self.input_device.id);
                r = Err(Error::error());
            }
            self.input_alive_listener = None;
        }

        r
    }

    fn uninstall_system_changed_callback(&mut self) -> Result<()> {
        if self.stm_ptr.is_null() {
            assert!(
                self.default_output_listener.is_none() && self.default_input_listener.is_none()
            );
            return Ok(());
        }

        let stm = unsafe { &(*self.stm_ptr) };

        if self.default_output_listener.is_some() {
            let r = stm.remove_device_listener(self.default_output_listener.as_ref().unwrap());
            if r != NO_ERR {
                return Err(Error::error());
            }
            self.default_output_listener = None;
        }

        if self.default_input_listener.is_some() {
            let r = stm.remove_device_listener(self.default_input_listener.as_ref().unwrap());
            if r != NO_ERR {
                return Err(Error::error());
            }
            self.default_input_listener = None;
        }

        Ok(())
    }
}

impl<'ctx> Drop for CoreStreamData<'ctx> {
    fn drop(&mut self) {
        self.stop_audiounits();
        self.close();
    }
}

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
    device_changed_callback: Mutex<ffi::cubeb_device_changed_callback>,
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
    total_output_latency_frames: AtomicU32,
    panning: atomic::Atomic<f32>,
    // This is true if a device change callback is currently running.
    switching_device: AtomicBool,
    core_stream_data: CoreStreamData<'ctx>,
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
            device_changed_callback: Mutex::new(None),
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
            total_output_latency_frames: AtomicU32::new(0),
            panning: atomic::Atomic::new(0.0_f32),
            switching_device: AtomicBool::new(false),
            core_stream_data: CoreStreamData::default(),
        }
    }

    fn add_device_listener(&self, listener: &device_property_listener) -> OSStatus {
        audio_object_add_property_listener(
            listener.device,
            listener.property,
            listener.listener,
            self as *const Self as *mut c_void,
        )
    }

    fn remove_device_listener(&self, listener: &device_property_listener) -> OSStatus {
        audio_object_remove_property_listener(
            listener.device,
            listener.property,
            listener.listener,
            self as *const Self as *mut c_void,
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

    fn reinit(&mut self) -> Result<()> {
        // Call stop_audiounits to avoid potential data race. If there is a running data callback,
        // which locks a mutex inside CoreAudio framework, then this call will block the current
        // thread until the callback is finished since this call asks to lock a mutex inside
        // CoreAudio framework that is used by the data callback.
        if !self.shutdown.load(Ordering::SeqCst) {
            self.core_stream_data.stop_audiounits();
        }

        assert!(
            !self.core_stream_data.input_unit.is_null()
                || !self.core_stream_data.output_unit.is_null()
        );
        let vol_rv = if self.core_stream_data.output_unit.is_null() {
            Err(Error::error())
        } else {
            get_volume(self.core_stream_data.output_unit)
        };

        let has_input = !self.core_stream_data.input_unit.is_null();
        let input_device = if has_input {
            self.core_stream_data.input_device.id
        } else {
            kAudioObjectUnknown
        };

        self.core_stream_data.close();

        // Reinit occurs in one of the following case:
        // - When the device is not alive any more
        // - When the default system device change.
        // - The bluetooth device changed from A2DP to/from HFP/HSP profile
        // We first attempt to re-use the same device id, should that fail we will
        // default to the (potentially new) default device.
        if has_input {
            self.core_stream_data.input_device = create_device_info(input_device, DeviceType::INPUT).map_err(|e| {
                cubeb_log!(
                    "({:p}) Create input device info failed. This can happen when last media device is unplugged",
                    self.core_stream_data.stm_ptr
                );
                self.core_stream_data.close();
                e
            })?;
        }

        // Always use the default output on reinit. This is not correct in every
        // case but it is sufficient for Firefox and prevent reinit from reporting
        // failures. It will change soon when reinit mechanism will be updated.
        self.core_stream_data.output_device = create_device_info(kAudioObjectUnknown, DeviceType::OUTPUT).map_err(|e| {
            cubeb_log!(
                "({:p}) Create output device info failed. This can happen when last media device is unplugged",
                self.core_stream_data.stm_ptr
            );
            self.core_stream_data.close();
            e
        })?;

        if self.core_stream_data.setup().is_err() {
            cubeb_log!(
                "({:p}) Stream reinit failed.",
                self.core_stream_data.stm_ptr
            );
            if has_input && input_device != kAudioObjectUnknown {
                // Attempt to re-use the same device-id failed, so attempt again with
                // default input device.
                self.core_stream_data.input_device = create_device_info(kAudioObjectUnknown, DeviceType::INPUT).map_err(|e| {
                    cubeb_log!(
                        "({:p}) Create input device info failed. This can happen when last media device is unplugged",
                        self.core_stream_data.stm_ptr
                    );
                    self.core_stream_data.close();
                    e
                })?;
                self.core_stream_data.setup().map_err(|e| {
                    cubeb_log!(
                        "({:p}) Second stream reinit failed.",
                        self.core_stream_data.stm_ptr
                    );
                    self.core_stream_data.close();
                    e
                })?;
            }
        }

        if vol_rv.is_ok() {
            set_volume(self.core_stream_data.output_unit, vol_rv.unwrap());
        }

        // If the stream was running, start it again.
        if !self.shutdown.load(Ordering::SeqCst) {
            self.core_stream_data.start_audiounits().map_err(|e| {
                cubeb_log!(
                    "({:p}) Start audiounit failed.",
                    self.core_stream_data.stm_ptr
                );
                self.core_stream_data.close();
                e
            })?;
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

    fn destroy_internal(&mut self) {
        self.core_stream_data.close();
        assert!(self.context.active_streams() >= 1);
        self.context.update_latency_by_removing_stream();
    }

    fn destroy(&mut self) {
        // Call stop_audiounits to avoid potential data race. If there is a running data callback,
        // which locks a mutex inside CoreAudio framework, then this call will block the current
        // thread until the callback is finished since this call asks to lock a mutex inside
        // CoreAudio framework that is used by the data callback.
        if !self.shutdown.load(Ordering::SeqCst) {
            self.core_stream_data.stop_audiounits();
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

        self.core_stream_data.start_audiounits()?;

        self.notify_state_changed(State::Started);

        cubeb_log!(
            "Cubeb stream ({:p}) started successfully.",
            self as *const AudioUnitStream
        );
        Ok(())
    }
    fn stop(&mut self) -> Result<()> {
        *self.shutdown.get_mut() = true;

        self.core_stream_data.stop_audiounits();

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
        Ok(self.total_output_latency_frames.load(Ordering::SeqCst))
    }
    fn set_volume(&mut self, volume: f32) -> Result<()> {
        set_volume(self.core_stream_data.output_unit, volume)
    }
    fn set_panning(&mut self, panning: f32) -> Result<()> {
        if self.core_stream_data.output_desc.mChannelsPerFrame > 2 {
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
        let mut callback = self.device_changed_callback.lock().unwrap();
        // Note: second register without unregister first causes 'nope' error.
        // Current implementation requires unregister before register a new cb.
        if device_changed_callback.is_some() && callback.is_some() {
            Err(Error::invalid_parameter())
        } else {
            *callback = device_changed_callback;
            Ok(())
        }
    }
}

unsafe impl<'ctx> Send for AudioUnitStream<'ctx> {}
unsafe impl<'ctx> Sync for AudioUnitStream<'ctx> {}
