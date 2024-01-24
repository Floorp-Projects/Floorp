// Copyright © 2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.
#![allow(unused_assignments)]
#![allow(unused_must_use)]

extern crate coreaudio_sys_utils;
extern crate libc;
extern crate ringbuf;

mod aggregate_device;
mod auto_release;
mod buffer_manager;
mod device_property;
mod mixer;
mod resampler;
mod utils;

use self::aggregate_device::*;
use self::auto_release::*;
use self::buffer_manager::*;
use self::coreaudio_sys_utils::aggregate_device::*;
use self::coreaudio_sys_utils::audio_device_extensions::*;
use self::coreaudio_sys_utils::audio_object::*;
use self::coreaudio_sys_utils::audio_unit::*;
use self::coreaudio_sys_utils::cf_mutable_dict::*;
use self::coreaudio_sys_utils::dispatch::*;
use self::coreaudio_sys_utils::string::*;
use self::coreaudio_sys_utils::sys::*;
use self::device_property::*;
use self::mixer::*;
use self::resampler::*;
use self::utils::*;
use atomic;
use backend::ringbuf::RingBuffer;
use cubeb_backend::{
    ffi, Context, ContextOps, DeviceCollectionRef, DeviceId, DeviceRef, DeviceType, Error,
    InputProcessingParams, Ops, Result, SampleFormat, State, Stream, StreamOps, StreamParams,
    StreamParamsRef, StreamPrefs,
};
use mach::mach_time::{mach_absolute_time, mach_timebase_info};
use std::cmp;
use std::ffi::{CStr, CString};
use std::fmt;
use std::mem;
use std::os::raw::{c_uint, c_void};
use std::ptr;
use std::slice;
use std::sync::atomic::{AtomicBool, AtomicU32, AtomicUsize, Ordering};
use std::sync::{Arc, Condvar, Mutex};
use std::time::Duration;
const NO_ERR: OSStatus = 0;

const AU_OUT_BUS: AudioUnitElement = 0;
const AU_IN_BUS: AudioUnitElement = 1;

const DISPATCH_QUEUE_LABEL: &str = "org.mozilla.cubeb";
const PRIVATE_AGGREGATE_DEVICE_NAME: &str = "CubebAggregateDevice";
const VOICEPROCESSING_AGGREGATE_DEVICE_NAME: &str = "VPAUAggregateAudioDevice";

// Testing empirically, some headsets report a minimal latency that is very low,
// but this does not work in practice. Lie and say the minimum is 128 frames.
const SAFE_MIN_LATENCY_FRAMES: u32 = 128;
const SAFE_MAX_LATENCY_FRAMES: u32 = 512;

bitflags! {
    #[allow(non_camel_case_types)]
    #[derive(Clone, Debug, PartialEq, Copy)]
    struct device_flags: u32 {
        const DEV_UNKNOWN           = 0b0000_0000; // Unknown
        const DEV_INPUT             = 0b0000_0001; // Record device like mic
        const DEV_OUTPUT            = 0b0000_0010; // Playback device like speakers
        const DEV_SELECTED_DEFAULT  = 0b0000_0100; // User selected to use the system default device
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
    property: AudioObjectPropertyAddress,
    listener: audio_object_property_listener_proc,
}

impl device_property_listener {
    fn new(
        device: AudioDeviceID,
        property: AudioObjectPropertyAddress,
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

impl From<CAChannelLabel> for mixer::Channel {
    fn from(label: CAChannelLabel) -> mixer::Channel {
        use self::coreaudio_sys_utils::sys;
        match label.0 {
            sys::kAudioChannelLabel_Left => mixer::Channel::FrontLeft,
            sys::kAudioChannelLabel_Right => mixer::Channel::FrontRight,
            sys::kAudioChannelLabel_Center | sys::kAudioChannelLabel_Mono => {
                mixer::Channel::FrontCenter
            }
            sys::kAudioChannelLabel_LFEScreen => mixer::Channel::LowFrequency,
            sys::kAudioChannelLabel_LeftSurround => mixer::Channel::BackLeft,
            sys::kAudioChannelLabel_RightSurround => mixer::Channel::BackRight,
            sys::kAudioChannelLabel_LeftCenter => mixer::Channel::FrontLeftOfCenter,
            sys::kAudioChannelLabel_RightCenter => mixer::Channel::FrontRightOfCenter,
            sys::kAudioChannelLabel_CenterSurround => mixer::Channel::BackCenter,
            sys::kAudioChannelLabel_LeftSurroundDirect => mixer::Channel::SideLeft,
            sys::kAudioChannelLabel_RightSurroundDirect => mixer::Channel::SideRight,
            sys::kAudioChannelLabel_TopCenterSurround => mixer::Channel::TopCenter,
            sys::kAudioChannelLabel_VerticalHeightLeft => mixer::Channel::TopFrontLeft,
            sys::kAudioChannelLabel_VerticalHeightCenter => mixer::Channel::TopFrontCenter,
            sys::kAudioChannelLabel_VerticalHeightRight => mixer::Channel::TopFrontRight,
            sys::kAudioChannelLabel_TopBackLeft => mixer::Channel::TopBackLeft,
            sys::kAudioChannelLabel_TopBackCenter => mixer::Channel::TopBackCenter,
            sys::kAudioChannelLabel_TopBackRight => mixer::Channel::TopBackRight,
            _ => mixer::Channel::Silence,
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

fn create_device_info(devid: AudioDeviceID, devtype: DeviceType) -> Option<device_info> {
    assert_ne!(devid, kAudioObjectSystemObject);

    let mut flags = match devtype {
        DeviceType::INPUT => device_flags::DEV_INPUT,
        DeviceType::OUTPUT => device_flags::DEV_OUTPUT,
        _ => panic!("Only accept input or output type"),
    };

    if devid == kAudioObjectUnknown {
        cubeb_log!("Using the system default device");
        flags |= device_flags::DEV_SELECTED_DEFAULT;
        get_default_device(devtype).map(|id| device_info { id, flags })
    } else {
        Some(device_info { id: devid, flags })
    }
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

fn minimum_resampling_input_frames(
    input_rate: f64,
    output_rate: f64,
    output_frames: usize,
) -> usize {
    assert!(!approx_eq!(f64, input_rate, 0_f64));
    assert!(!approx_eq!(f64, output_rate, 0_f64));
    if approx_eq!(f64, input_rate, output_rate) {
        return output_frames;
    }
    (input_rate * output_frames as f64 / output_rate).ceil() as usize
}

fn audiounit_make_silent(io_data: &AudioBuffer) {
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
    }

    assert!(input_frames > 0);
    assert_eq!(bus, AU_IN_BUS);

    assert!(!user_ptr.is_null());
    let stm = unsafe { &mut *(user_ptr as *mut AudioUnitStream) };
    let using_voice_processing_unit = stm.core_stream_data.using_voice_processing_unit();

    if unsafe { *flags | kAudioTimeStampHostTimeValid } != 0 {
        let now = unsafe { mach_absolute_time() };
        let input_latency_frames = compute_input_latency(stm, unsafe { (*tstamp).mHostTime }, now);
        stm.total_input_latency_frames
            .store(input_latency_frames, Ordering::SeqCst);
    }

    if stm.stopped.load(Ordering::SeqCst) {
        cubeb_log!("({:p}) input stopped", stm as *const AudioUnitStream);
        return NO_ERR;
    }

    let handler = |stm: &mut AudioUnitStream,
                   flags: *mut AudioUnitRenderActionFlags,
                   tstamp: *const AudioTimeStamp,
                   bus: u32,
                   input_frames: u32|
     -> ErrorHandle {
        let input_buffer_manager = stm.core_stream_data.input_buffer_manager.as_mut().unwrap();
        assert_eq!(
            stm.core_stream_data.stm_ptr,
            user_ptr as *const AudioUnitStream
        );

        // `flags` and `tstamp` must be non-null so they can be casted into the references.
        assert!(!flags.is_null());
        let flags = unsafe { &mut (*flags) };
        assert!(!tstamp.is_null());
        let tstamp = unsafe { &(*tstamp) };

        // Create the AudioBufferList to store input.
        let mut input_buffer_list = AudioBufferList::default();
        input_buffer_list.mBuffers[0].mDataByteSize =
            stm.core_stream_data.input_dev_desc.mBytesPerFrame * input_frames;
        input_buffer_list.mBuffers[0].mData = ptr::null_mut();
        input_buffer_list.mBuffers[0].mNumberChannels =
            stm.core_stream_data.input_dev_desc.mChannelsPerFrame;
        input_buffer_list.mNumberBuffers = 1;

        debug_assert!(!stm.core_stream_data.input_unit.is_null());
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
            return ErrorHandle::Return(status);
        }
        let handle = if status == kAudioUnitErr_CannotDoInCurrentContext {
            assert!(!stm.core_stream_data.output_unit.is_null());
            // kAudioUnitErr_CannotDoInCurrentContext is returned when using a BT
            // headset and the profile is changed from A2DP to HFP/HSP. The previous
            // output device is no longer valid and must be reset.
            // For now state that no error occurred and feed silence, stream will be
            // resumed once reinit has completed.
            ErrorHandle::Reinit
        } else {
            assert_eq!(status, NO_ERR);
            input_buffer_manager
                .push_data(input_buffer_list.mBuffers[0].mData, input_frames as usize);
            ErrorHandle::Return(status)
        };

        // Full Duplex. We'll call data_callback in the AudioUnit output callback. Record this
        // callback for logging.
        if !stm.core_stream_data.output_unit.is_null() {
            let input_callback_data = InputCallbackData {
                bytes: input_buffer_list.mBuffers[0].mDataByteSize,
                rendered_frames: input_frames,
                total_available: input_buffer_manager.available_frames(),
                channels: input_buffer_list.mBuffers[0].mNumberChannels,
                num_buf: input_buffer_list.mNumberBuffers,
            };
            stm.core_stream_data
                .input_logging
                .as_mut()
                .unwrap()
                .push(input_callback_data);
            return handle;
        }

        cubeb_alogv!(
            "({:p}) input: buffers {}, size {}, channels {}, rendered frames {}, total frames {}.",
            stm.core_stream_data.stm_ptr,
            input_buffer_list.mNumberBuffers,
            input_buffer_list.mBuffers[0].mDataByteSize,
            input_buffer_list.mBuffers[0].mNumberChannels,
            input_frames,
            input_buffer_manager.available_frames()
        );

        // Input only. Call the user callback through resampler.
        // Resampler will deliver input buffer in the correct rate.
        assert!(input_frames as usize <= input_buffer_manager.available_frames());
        stm.frames_read.fetch_add(
            input_buffer_manager.available_frames(),
            atomic::Ordering::SeqCst,
        );
        let mut total_input_frames = input_buffer_manager.available_frames() as i64;
        let input_buffer =
            input_buffer_manager.get_linear_data(input_buffer_manager.available_frames());
        let outframes = stm.core_stream_data.resampler.fill(
            input_buffer,
            &mut total_input_frames,
            ptr::null_mut(),
            0,
        );
        if outframes < 0 {
            stm.stopped.store(true, Ordering::SeqCst);
            stm.notify_state_changed(State::Error);
            let queue = stm.queue.clone();
            // Use a new thread, through the queue, to avoid deadlock when calling
            // AudioOutputUnitStop method from inside render callback
            queue.run_async(move || {
                stm.core_stream_data.stop_audiounits();
            });
            return handle;
        }
        if outframes < total_input_frames {
            stm.draining.store(true, Ordering::SeqCst);
        }

        handle
    };

    // If the stream is drained, do nothing.
    let handle = if !stm.draining.load(Ordering::SeqCst) {
        handler(stm, flags, tstamp, bus, input_frames)
    } else {
        ErrorHandle::Return(NO_ERR)
    };

    // If the input (input-only stream) or the output is drained (duplex stream),
    // cancel this callback. Note that for voice processing cases (a single unit),
    // the output callback handles stopping the unit and notifying of state.
    if !using_voice_processing_unit && stm.draining.load(Ordering::SeqCst) {
        let r = stop_audiounit(stm.core_stream_data.input_unit);
        assert!(r.is_ok());
        // Only fire state-changed callback for input-only stream.
        // The state-changed callback for the duplex stream is fired in the output callback.
        if stm.core_stream_data.output_unit.is_null() {
            stm.notify_state_changed(State::Drained);
        }
    }

    match handle {
        ErrorHandle::Reinit => {
            stm.reinit_async();
            NO_ERR
        }
        ErrorHandle::Return(s) => s,
    }
}

fn host_time_to_ns(host_time: u64) -> u64 {
    let mut rv: f64 = host_time as f64;
    rv *= HOST_TIME_TO_NS_RATIO.0 as f64;
    rv /= HOST_TIME_TO_NS_RATIO.1 as f64;
    rv as u64
}

fn compute_output_latency(stm: &AudioUnitStream, audio_output_time: u64, now: u64) -> u32 {
    const NS2S: u64 = 1_000_000_000;
    let output_hw_rate = stm.core_stream_data.output_dev_desc.mSampleRate as u64;
    let fixed_latency_ns =
        (stm.output_device_latency_frames.load(Ordering::SeqCst) as u64 * NS2S) / output_hw_rate;
    // The total output latency is the timestamp difference + the stream latency + the hardware
    // latency.
    let total_output_latency_ns =
        fixed_latency_ns + host_time_to_ns(audio_output_time.saturating_sub(now));

    (total_output_latency_ns * output_hw_rate / NS2S) as u32
}

fn compute_input_latency(stm: &AudioUnitStream, audio_input_time: u64, now: u64) -> u32 {
    const NS2S: u64 = 1_000_000_000;
    let input_hw_rate = stm.core_stream_data.input_dev_desc.mSampleRate as u64;
    let fixed_latency_ns =
        (stm.input_device_latency_frames.load(Ordering::SeqCst) as u64 * NS2S) / input_hw_rate;
    // The total input latency is the timestamp difference + the stream latency +
    // the hardware latency.
    let total_input_latency_ns =
        host_time_to_ns(now.saturating_sub(audio_input_time)) + fixed_latency_ns;

    (total_input_latency_ns * input_hw_rate / NS2S) as u32
}

extern "C" fn audiounit_output_callback(
    user_ptr: *mut c_void,
    flags: *mut AudioUnitRenderActionFlags,
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
    let buffers = unsafe {
        let ptr = out_buffer_list_ref.mBuffers.as_mut_ptr();
        let len = out_buffer_list_ref.mNumberBuffers as usize;
        slice::from_raw_parts_mut(ptr, len)
    };

    if stm.stopped.load(Ordering::SeqCst) {
        cubeb_alog!("({:p}) output stopped.", stm as *const AudioUnitStream);
        audiounit_make_silent(&buffers[0]);
        return NO_ERR;
    }

    if stm.draining.load(Ordering::SeqCst) {
        // Cancel the output callback only. For duplex stream,
        // the input callback will be cancelled in its own callback.
        let r = stop_audiounit(stm.core_stream_data.output_unit);
        assert!(r.is_ok());
        stm.notify_state_changed(State::Drained);
        audiounit_make_silent(&buffers[0]);
        return NO_ERR;
    }

    let now = unsafe { mach_absolute_time() };

    if unsafe { *flags | kAudioTimeStampHostTimeValid } != 0 {
        let output_latency_frames =
            compute_output_latency(stm, unsafe { (*tstamp).mHostTime }, now);
        stm.total_output_latency_frames
            .store(output_latency_frames, Ordering::SeqCst);
    }
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

    let prev_frames_written = stm.frames_written.load(Ordering::SeqCst);

    stm.frames_written
        .fetch_add(output_frames as usize, Ordering::SeqCst);

    // Also get the input buffer if the stream is duplex
    let (input_buffer, mut input_frames) = if !stm.core_stream_data.input_unit.is_null() {
        let input_logging = &mut stm.core_stream_data.input_logging.as_mut().unwrap();
        if input_logging.is_empty() {
            cubeb_alogv!("no audio input data in output callback");
        } else {
            while let Some(input_callback_data) = input_logging.pop() {
                cubeb_alogv!(
                    "input: buffers {}, size {}, channels {}, rendered frames {}, total frames {}.",
                    input_callback_data.num_buf,
                    input_callback_data.bytes,
                    input_callback_data.channels,
                    input_callback_data.rendered_frames,
                    input_callback_data.total_available
                );
            }
        }
        let input_buffer_manager = stm.core_stream_data.input_buffer_manager.as_mut().unwrap();
        assert_ne!(stm.core_stream_data.input_dev_desc.mChannelsPerFrame, 0);
        // If the output callback came first and this is a duplex stream, we need to
        // fill in some additional silence in the resampler.
        // Otherwise, if we had more than expected callbacks in a row, or we're
        // currently switching, we add some silence as well to compensate for the
        // fact that we're lacking some input data.
        let input_frames_needed = minimum_resampling_input_frames(
            stm.core_stream_data.input_dev_desc.mSampleRate,
            f64::from(stm.core_stream_data.output_stream_params.rate()),
            output_frames as usize,
        );
        let buffered_input_frames = input_buffer_manager.available_frames();
        // Else if the input has buffered a lot already because the output started late, we
        // need to trim the input buffer
        if prev_frames_written == 0 && buffered_input_frames > input_frames_needed {
            input_buffer_manager.trim(input_frames_needed);
            let popped_frames = buffered_input_frames - input_frames_needed;
            cubeb_alog!("Dropping {} frames in input buffer.", popped_frames);
        }

        let input_frames = if input_frames_needed > buffered_input_frames
            && (stm.switching_device.load(Ordering::SeqCst)
                || stm.reinit_pending.load(Ordering::SeqCst)
                || stm.frames_read.load(Ordering::SeqCst) == 0)
        {
            // The silent frames will be inserted in `get_linear_data` below.
            let silent_frames_to_push = input_frames_needed - buffered_input_frames;
            cubeb_alog!(
                "({:p}) Missing Frames: {} will append {} frames of input silence.",
                stm.core_stream_data.stm_ptr,
                if stm.frames_read.load(Ordering::SeqCst) == 0 {
                    "input hasn't started,"
                } else if stm.switching_device.load(Ordering::SeqCst) {
                    "device switching,"
                } else {
                    "reinit pending,"
                },
                silent_frames_to_push
            );
            input_frames_needed
        } else {
            buffered_input_frames
        };

        stm.frames_read.fetch_add(input_frames, Ordering::SeqCst);
        (
            input_buffer_manager.get_linear_data(input_frames),
            input_frames as i64,
        )
    } else {
        (ptr::null_mut::<c_void>(), 0)
    };

    cubeb_alogv!(
        "({:p}) output: buffers {}, size {}, channels {}, frames {}.",
        stm as *const AudioUnitStream,
        buffers.len(),
        buffers[0].mDataByteSize,
        buffers[0].mNumberChannels,
        output_frames
    );

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

    if outframes < 0 || outframes > i64::from(output_frames) {
        stm.stopped.store(true, Ordering::SeqCst);
        stm.notify_state_changed(State::Error);
        let queue = stm.queue.clone();
        // Use a new thread, through the queue, to avoid deadlock when calling
        // AudioOutputUnitStop method from inside render callback
        queue.run_async(move || {
            stm.core_stream_data.stop_audiounits();
        });
        audiounit_make_silent(&buffers[0]);
        return NO_ERR;
    }

    stm.draining
        .store(outframes < i64::from(output_frames), Ordering::SeqCst);
    stm.output_callback_timing_data_write
        .write(OutputCallbackTimingData {
            frames_queued: stm.frames_queued,
            timestamp: now,
            buffer_size: outframes as u64,
        });

    stm.frames_queued += outframes as u64;

    // Post process output samples.
    if stm.draining.load(Ordering::SeqCst) {
        // Clear missing frames (silence)
        let frames_to_bytes = |frames: usize| -> usize {
            let sample_size = cubeb_sample_size(stm.core_stream_data.output_stream_params.format());
            let channel_count = stm.core_stream_data.output_stream_params.channels() as usize;
            frames * sample_size * channel_count
        };
        let out_bytes = unsafe {
            slice::from_raw_parts_mut(
                output_buffer as *mut u8,
                frames_to_bytes(output_frames as usize),
            )
        };
        let start = frames_to_bytes(outframes as usize);
        for byte in out_bytes.iter_mut().skip(start) {
            *byte = 0;
        }
    }

    // Mixing
    if stm.core_stream_data.mixer.is_some() {
        assert!(
            buffers[0].mDataByteSize
                >= stm.core_stream_data.output_dev_desc.mBytesPerFrame * output_frames
        );
        stm.core_stream_data.mixer.as_mut().unwrap().mix(
            output_frames as usize,
            buffers[0].mData,
            buffers[0].mDataByteSize as usize,
        );
    }
    NO_ERR
}

#[allow(clippy::cognitive_complexity)]
extern "C" fn audiounit_property_listener_callback(
    id: AudioObjectID,
    address_count: u32,
    addresses: *const AudioObjectPropertyAddress,
    user: *mut c_void,
) -> OSStatus {
    assert_ne!(address_count, 0);

    let stm = unsafe { &mut *(user as *mut AudioUnitStream) };
    let addrs = unsafe { slice::from_raw_parts(addresses, address_count as usize) };
    if stm.switching_device.load(Ordering::SeqCst) {
        cubeb_log!(
            "Switching is already taking place. Skipping event for device {}",
            id
        );
        return NO_ERR;
    }
    stm.switching_device.store(true, Ordering::SeqCst);

    let mut explicit_device_dead = false;

    cubeb_log!(
        "({:p}) Handling {} device changed events for device {}",
        stm as *const AudioUnitStream,
        address_count,
        id
    );
    for (i, addr) in addrs.iter().enumerate() {
        let p = PropertySelector::from(addr.mSelector);
        cubeb_log!("Event #{}: {}", i, p);
        assert_ne!(p, PropertySelector::Unknown);
        if p == PropertySelector::DeviceIsAlive {
            explicit_device_dead = true;
        }
    }

    // Handle the events
    if explicit_device_dead {
        cubeb_log!("The user-selected input or output device is dead, entering error state");
        stm.stopped.store(true, Ordering::SeqCst);

        // Use a different thread, through the queue, to avoid deadlock when calling
        // Get/SetProperties method from inside notify callback
        stm.queue.clone().run_async(move || {
            stm.core_stream_data.stop_audiounits();
            stm.close_on_error();
        });
        return NO_ERR;
    }
    {
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

fn get_default_device(devtype: DeviceType) -> Option<AudioObjectID> {
    match get_default_device_id(devtype) {
        Err(e) => {
            cubeb_log!("Cannot get default {:?} device. Error: {}", devtype, e);
            None
        }
        Ok(id) if id == kAudioObjectUnknown => {
            cubeb_log!("Get an invalid default {:?} device: {}", devtype, id);
            None
        }
        Ok(id) => Some(id),
    }
}

fn get_default_device_id(devtype: DeviceType) -> std::result::Result<AudioObjectID, OSStatus> {
    let address = get_property_address(
        match devtype {
            DeviceType::INPUT => Property::HardwareDefaultInputDevice,
            DeviceType::OUTPUT => Property::HardwareDefaultOutputDevice,
            _ => panic!("Unsupport type"),
        },
        DeviceType::INPUT | DeviceType::OUTPUT,
    );

    let mut devid: AudioDeviceID = kAudioObjectUnknown;
    let mut size = mem::size_of::<AudioDeviceID>();
    let status =
        audio_object_get_property_data(kAudioObjectSystemObject, &address, &mut size, &mut devid);
    if status == NO_ERR {
        Ok(devid)
    } else {
        Err(status)
    }
}

fn audiounit_convert_channel_layout(layout: &AudioChannelLayout) -> Result<Vec<mixer::Channel>> {
    if layout.mChannelLayoutTag != kAudioChannelLayoutTag_UseChannelDescriptions {
        // kAudioChannelLayoutTag_UseChannelBitmap
        // kAudioChannelLayoutTag_Mono
        // kAudioChannelLayoutTag_Stereo
        // ....
        cubeb_log!("Only handling UseChannelDescriptions for now.\n");
        return Err(Error::error());
    }

    let channel_descriptions = unsafe {
        slice::from_raw_parts(
            layout.mChannelDescriptions.as_ptr(),
            layout.mNumberChannelDescriptions as usize,
        )
    };

    let mut channels = Vec::with_capacity(layout.mNumberChannelDescriptions as usize);
    for description in channel_descriptions {
        let label = CAChannelLabel(description.mChannelLabel);
        channels.push(label.into());
    }

    Ok(channels)
}

fn audiounit_get_preferred_channel_layout(output_unit: AudioUnit) -> Result<Vec<mixer::Channel>> {
    let mut rv = NO_ERR;
    let mut size: usize = 0;
    rv = audio_unit_get_property_info(
        output_unit,
        kAudioDevicePropertyPreferredChannelLayout,
        kAudioUnitScope_Output,
        AU_OUT_BUS,
        &mut size,
        None,
    );
    if rv != NO_ERR {
        cubeb_log!(
            "AudioUnitGetPropertyInfo/kAudioDevicePropertyPreferredChannelLayout rv={}",
            rv
        );
        return Err(Error::error());
    }
    debug_assert!(size > 0);

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
        return Err(Error::error());
    }

    audiounit_convert_channel_layout(layout.as_ref())
}

// This is for output AudioUnit only. Calling this by input-only AudioUnit is prone
// to crash intermittently.
fn audiounit_get_current_channel_layout(output_unit: AudioUnit) -> Result<Vec<mixer::Channel>> {
    let mut rv = NO_ERR;
    let mut size: usize = 0;
    rv = audio_unit_get_property_info(
        output_unit,
        kAudioUnitProperty_AudioChannelLayout,
        kAudioUnitScope_Output,
        AU_OUT_BUS,
        &mut size,
        None,
    );
    if rv != NO_ERR {
        cubeb_log!(
            "AudioUnitGetPropertyInfo/kAudioUnitProperty_AudioChannelLayout rv={}",
            rv
        );
        return Err(Error::error());
    }
    debug_assert!(size > 0);

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
        return Err(Error::error());
    }

    audiounit_convert_channel_layout(layout.as_ref())
}

fn get_channel_layout(output_unit: AudioUnit) -> Result<Vec<mixer::Channel>> {
    audiounit_get_current_channel_layout(output_unit)
        .or_else(|_| {
            // The kAudioUnitProperty_AudioChannelLayout property isn't known before
            // macOS 10.12, attempt another method.
            cubeb_log!(
                "Cannot get current channel layout for audiounit @ {:p}. Trying preferred channel layout.",
                output_unit
            );
            audiounit_get_preferred_channel_layout(output_unit)
        })
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

    let unit = create_blank_audiounit()?;
    let mut bus = AU_OUT_BUS;

    if device.flags.contains(device_flags::DEV_INPUT) {
        // Input only.
        if let Err(e) = enable_audiounit_scope(unit, DeviceType::INPUT, true) {
            cubeb_log!("Failed to enable audiounit input scope. Error: {}", e);
            dispose_audio_unit(unit);
            return Err(Error::error());
        }
        if let Err(e) = enable_audiounit_scope(unit, DeviceType::OUTPUT, false) {
            cubeb_log!("Failed to disable audiounit output scope. Error: {}", e);
            dispose_audio_unit(unit);
            return Err(Error::error());
        }
        bus = AU_IN_BUS;
    }

    if device.flags.contains(device_flags::DEV_OUTPUT) {
        // Output only.
        if let Err(e) = enable_audiounit_scope(unit, DeviceType::OUTPUT, true) {
            cubeb_log!("Failed to enable audiounit output scope. Error: {}", e);
            dispose_audio_unit(unit);
            return Err(Error::error());
        }
        if let Err(e) = enable_audiounit_scope(unit, DeviceType::INPUT, false) {
            cubeb_log!("Failed to disable audiounit input scope. Error: {}", e);
            dispose_audio_unit(unit);
            return Err(Error::error());
        }
        bus = AU_OUT_BUS;
    }

    if let Err(e) = set_device_to_audiounit(unit, device.id, bus) {
        cubeb_log!(
            "Failed to set device {} to the created audiounit. Error: {}",
            device.id,
            e
        );
        dispose_audio_unit(unit);
        return Err(Error::error());
    }

    Ok(unit)
}

fn create_voiceprocessing_audiounit(
    in_device: &device_info,
    out_device: &device_info,
) -> Result<AudioUnit> {
    assert!(in_device.flags.contains(device_flags::DEV_INPUT));
    assert!(!in_device.flags.contains(device_flags::DEV_OUTPUT));
    assert!(!out_device.flags.contains(device_flags::DEV_INPUT));
    assert!(out_device.flags.contains(device_flags::DEV_OUTPUT));

    let unit = create_typed_audiounit(kAudioUnitSubType_VoiceProcessingIO)?;

    if let Err(e) = set_device_to_audiounit(unit, in_device.id, AU_IN_BUS) {
        cubeb_log!(
            "Failed to set in device {} to the created audiounit. Error: {}",
            in_device.id,
            e
        );
        dispose_audio_unit(unit);
        return Err(Error::error());
    }

    if let Err(e) = set_device_to_audiounit(unit, out_device.id, AU_OUT_BUS) {
        cubeb_log!(
            "Failed to set out device {} to the created audiounit. Error: {}",
            out_device.id,
            e
        );
        dispose_audio_unit(unit);
        return Err(Error::error());
    }

    let bypass = u32::from(true);
    let r = audio_unit_set_property(
        unit,
        kAudioUnitProperty_BypassEffect,
        kAudioUnitScope_Global,
        AU_IN_BUS,
        &bypass,
        mem::size_of::<u32>(),
    );
    if r != NO_ERR {
        cubeb_log!("Failed to enable bypass of voiceprocessing. Error: {}", r);
        dispose_audio_unit(unit);
        return Err(Error::error());
    }

    Ok(unit)
}

fn enable_audiounit_scope(
    unit: AudioUnit,
    devtype: DeviceType,
    enable_io: bool,
) -> std::result::Result<(), OSStatus> {
    assert!(!unit.is_null());

    let enable = u32::from(enable_io);
    let (scope, element) = match devtype {
        DeviceType::INPUT => (kAudioUnitScope_Input, AU_IN_BUS),
        DeviceType::OUTPUT => (kAudioUnitScope_Output, AU_OUT_BUS),
        _ => panic!(
            "Enable AudioUnit {:?} with unsupported type: {:?}",
            unit, devtype
        ),
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
    bus: AudioUnitElement,
) -> std::result::Result<(), OSStatus> {
    assert!(!unit.is_null());

    let status = audio_unit_set_property(
        unit,
        kAudioOutputUnitProperty_CurrentDevice,
        kAudioUnitScope_Global,
        bus,
        &device_id,
        mem::size_of::<AudioDeviceID>(),
    );
    if status == NO_ERR {
        Ok(())
    } else {
        Err(status)
    }
}

fn create_typed_audiounit(sub_type: c_uint) -> Result<AudioUnit> {
    let desc = AudioComponentDescription {
        componentType: kAudioUnitType_Output,
        componentSubType: sub_type,
        componentManufacturer: kAudioUnitManufacturer_Apple,
        componentFlags: 0,
        componentFlagsMask: 0,
    };

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

fn create_blank_audiounit() -> Result<AudioUnit> {
    #[cfg(not(target_os = "ios"))]
    return create_typed_audiounit(kAudioUnitSubType_HALOutput);
    #[cfg(target_os = "ios")]
    return create_typed_audiounit(kAudioUnitSubType_RemoteIO);
}

fn get_buffer_size(unit: AudioUnit, devtype: DeviceType) -> std::result::Result<u32, OSStatus> {
    assert!(!unit.is_null());
    let (scope, element) = match devtype {
        DeviceType::INPUT => (kAudioUnitScope_Output, AU_IN_BUS),
        DeviceType::OUTPUT => (kAudioUnitScope_Input, AU_OUT_BUS),
        _ => panic!(
            "Get buffer size of AudioUnit {:?} with unsupported type: {:?}",
            unit, devtype
        ),
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
        Ok(frames)
    } else {
        Err(status)
    }
}

fn set_buffer_size(
    unit: AudioUnit,
    devtype: DeviceType,
    frames: u32,
) -> std::result::Result<(), OSStatus> {
    assert!(!unit.is_null());
    let (scope, element) = match devtype {
        DeviceType::INPUT => (kAudioUnitScope_Output, AU_IN_BUS),
        DeviceType::OUTPUT => (kAudioUnitScope_Input, AU_OUT_BUS),
        _ => panic!(
            "Set buffer size of AudioUnit {:?} with unsupported type: {:?}",
            unit, devtype
        ),
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

#[allow(clippy::mutex_atomic)] // The mutex needs to be fed into Condvar::wait_timeout.
fn set_buffer_size_sync(unit: AudioUnit, devtype: DeviceType, frames: u32) -> Result<()> {
    let current_frames = get_buffer_size(unit, devtype).map_err(|e| {
        cubeb_log!(
            "Cannot get buffer size of AudioUnit {:?} for {:?}. Error: {}",
            unit,
            devtype,
            e
        );
        Error::error()
    })?;
    if frames == current_frames {
        cubeb_log!(
            "The buffer frame size of AudioUnit {:?} for {:?} is already {}",
            unit,
            devtype,
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

    set_buffer_size(unit, devtype, frames).map_err(|e| {
        cubeb_log!(
            "Failed to set buffer size for AudioUnit {:?} for {:?}. Error: {}",
            unit,
            devtype,
            e
        );
        Error::error()
    })?;

    let (lock, cvar) = &*pair;
    let changed = lock.lock().unwrap();
    if !*changed {
        let (chg, timeout_res) = cvar.wait_timeout(changed, waiting_time).unwrap();
        if timeout_res.timed_out() {
            cubeb_log!(
                "Timed out for waiting the buffer frame size setting of AudioUnit {:?} for {:?}",
                unit,
                devtype
            );
        }
        if !*chg {
            return Err(Error::error());
        }
    }

    let new_frames = get_buffer_size(unit, devtype).map_err(|e| {
        cubeb_log!(
            "Cannot get new buffer size of AudioUnit {:?} for {:?}. Error: {}",
            unit,
            devtype,
            e
        );
        Error::error()
    })?;
    cubeb_log!(
        "The new buffer frames size of AudioUnit {:?} for {:?} is {}",
        unit,
        devtype,
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
        let (lock, cvar) = &**pair;
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

fn get_channel_count(
    devid: AudioObjectID,
    devtype: DeviceType,
) -> std::result::Result<u32, OSStatus> {
    assert_ne!(devid, kAudioObjectUnknown);

    let mut streams = get_device_streams(devid, devtype)?;

    if devtype == DeviceType::INPUT {
        // With VPIO, output devices will/may get a Tap that appears as input channels on the
        // output device id. One could check for whether the output device has a tap enabled,
        // but it is impossible to distinguish an output-only device from an input+output
        // device. There have also been corner cases observed, where the device does NOT have
        // a Tap enabled, but it still has the extra input channels from the Tap.
        // We can check the terminal type of the input stream instead, the VPIO type is
        // INPUT_UNDEFINED or an output type, we explicitly ignore those and keep all other cases.
        streams.retain(|stream| {
            let terminal_type = get_stream_terminal_type(*stream);
            if terminal_type.is_err() {
                return true;
            }

            #[allow(non_upper_case_globals)]
            match terminal_type.unwrap() {
                kAudioStreamTerminalTypeMicrophone
                | kAudioStreamTerminalTypeHeadsetMicrophone
                | kAudioStreamTerminalTypeReceiverMicrophone => true,
                t if [
                    kAudioStreamTerminalTypeSpeaker,
                    kAudioStreamTerminalTypeHeadphones,
                    kAudioStreamTerminalTypeLFESpeaker,
                    kAudioStreamTerminalTypeReceiverSpeaker,
                ]
                .contains(&t) =>
                {
                    cubeb_log!(
                        "Output TerminalType {:#06X} for input stream. Ignoring its channels.",
                        t
                    );
                    false
                }
                INPUT_UNDEFINED => {
                    cubeb_log!(
                        "INPUT_UNDEFINED TerminalType for input stream. Ignoring its channels."
                    );
                    false
                }
                // Note INPUT_UNDEFINED is 0x200 and INPUT_MICROPHONE is 0x201
                t if (INPUT_MICROPHONE..OUTPUT_UNDEFINED).contains(&t) => true,
                t if (OUTPUT_UNDEFINED..BIDIRECTIONAL_UNDEFINED).contains(&t) => false,
                t if (BIDIRECTIONAL_UNDEFINED..TELEPHONY_UNDEFINED).contains(&t) => true,
                t if (TELEPHONY_UNDEFINED..EXTERNAL_UNDEFINED).contains(&t) => true,
                t => {
                    cubeb_log!("Unknown TerminalType {:#06X} for input stream.", t);
                    true
                }
            }
        });
    }

    let mut count = 0;
    for stream in streams {
        if let Ok(format) = get_stream_virtual_format(stream) {
            count += format.mChannelsPerFrame;
        }
    }
    Ok(count)
}

fn get_range_of_sample_rates(
    devid: AudioObjectID,
    devtype: DeviceType,
) -> std::result::Result<(f64, f64), String> {
    let result = get_ranges_of_device_sample_rate(devid, devtype);
    if let Err(e) = result {
        return Err(format!("status {}", e));
    }
    let rates = result.unwrap();
    if rates.is_empty() {
        return Err(String::from("No data"));
    }
    let (mut min, mut max) = (std::f64::MAX, std::f64::MIN);
    for rate in rates {
        if rate.mMaximum > max {
            max = rate.mMaximum;
        }
        if rate.mMinimum < min {
            min = rate.mMinimum;
        }
    }
    Ok((min, max))
}

fn get_fixed_latency(devid: AudioObjectID, devtype: DeviceType) -> u32 {
    let device_latency = match get_device_latency(devid, devtype) {
        Ok(latency) => latency,
        Err(e) => {
            cubeb_log!(
                "Cannot get the device latency for device {} in {:?} scope. Error: {}",
                devid,
                devtype,
                e
            );
            0 // default device latency
        }
    };

    let stream_latency = get_device_streams(devid, devtype).and_then(|streams| {
        if streams.is_empty() {
            cubeb_log!(
                "No stream on device {} in {:?} scope!",
                devid,
                devtype
            );
            Ok(0) // default stream latency
        } else {
            get_stream_latency(streams[0])
        }
    }).map_err(|e| {
        cubeb_log!(
            "Cannot get the stream, or the latency of the first stream on device {} in {:?} scope. Error: {}",
            devid,
            devtype,
            e
        );
        e
    }).unwrap_or(0); // default stream latency

    device_latency + stream_latency
}

#[allow(non_upper_case_globals)]
fn get_device_group_id(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<CString, OSStatus> {
    match get_device_transport_type(id, devtype) {
        Ok(kAudioDeviceTransportTypeBuiltIn) => {
            cubeb_log!(
                "The transport type is {:?}",
                convert_uint32_into_string(kAudioDeviceTransportTypeBuiltIn)
            );
            match get_custom_group_id(id, devtype) {
                Some(id) => return Ok(id),
                None => {
                    cubeb_log!("Getting model UID instead.");
                }
            };
        }
        Ok(trans_type) => {
            cubeb_log!(
                "The transport type is {:?}. Getting model UID instead.",
                convert_uint32_into_string(trans_type)
            );
        }
        Err(e) => {
            cubeb_log!(
                "Error: {} when getting transport type. Get model uid instead.",
                e
            );
        }
    }

    // Some devices (e.g. AirPods) might only set the model-uid in the global scope.
    // The query might fail if the scope is input-only or output-only.
    get_device_model_uid(id, devtype)
        .or_else(|_| get_device_model_uid(id, DeviceType::INPUT | DeviceType::OUTPUT))
        .map(|uid| uid.into_cstring())
}

fn get_custom_group_id(id: AudioDeviceID, devtype: DeviceType) -> Option<CString> {
    const IMIC: u32 = 0x696D_6963; // "imic" (internal microphone)
    const ISPK: u32 = 0x6973_706B; // "ispk" (internal speaker)
    const EMIC: u32 = 0x656D_6963; // "emic" (external microphone)
    const HDPN: u32 = 0x6864_706E; // "hdpn" (headphone)

    match get_device_source(id, devtype) {
        s @ Ok(IMIC) | s @ Ok(ISPK) => {
            const GROUP_ID: &str = "builtin-internal-mic|spk";
            cubeb_log!(
                "Using hardcode group id: {} when source is: {:?}.",
                GROUP_ID,
                convert_uint32_into_string(s.unwrap())
            );
            return Some(CString::new(GROUP_ID).unwrap());
        }
        s @ Ok(EMIC) | s @ Ok(HDPN) => {
            const GROUP_ID: &str = "builtin-external-mic|hdpn";
            cubeb_log!(
                "Using hardcode group id: {} when source is: {:?}.",
                GROUP_ID,
                convert_uint32_into_string(s.unwrap())
            );
            return Some(CString::new(GROUP_ID).unwrap());
        }
        Ok(s) => {
            cubeb_log!(
                "No custom group id when source is: {:?}.",
                convert_uint32_into_string(s)
            );
        }
        Err(e) => {
            cubeb_log!("Error: {} when getting device source. ", e);
        }
    }
    None
}

fn get_device_label(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<StringRef, OSStatus> {
    get_device_source_name(id, devtype).or_else(|_| get_device_name(id, devtype))
}

fn get_device_global_uid(id: AudioDeviceID) -> std::result::Result<StringRef, OSStatus> {
    get_device_uid(id, DeviceType::INPUT | DeviceType::OUTPUT)
}

#[allow(clippy::cognitive_complexity)]
fn create_cubeb_device_info(
    devid: AudioObjectID,
    devtype: DeviceType,
) -> Result<ffi::cubeb_device_info> {
    if devtype != DeviceType::INPUT && devtype != DeviceType::OUTPUT {
        return Err(Error::error());
    }
    let channels = get_channel_count(devid, devtype).map_err(|e| {
        cubeb_log!("Cannot get the channel count. Error: {}", e);
        Error::error()
    })?;
    if channels == 0 {
        // Invalid type for this device.
        return Err(Error::error());
    }

    let mut dev_info = ffi::cubeb_device_info {
        max_channels: channels,
        ..Default::default()
    };

    assert!(
        mem::size_of::<ffi::cubeb_devid>() >= mem::size_of_val(&devid),
        "cubeb_devid can't represent devid"
    );
    dev_info.devid = devid as ffi::cubeb_devid;

    match get_device_uid(devid, devtype) {
        Ok(uid) => {
            let c_string = uid.into_cstring();
            dev_info.device_id = c_string.into_raw();
        }
        Err(e) => {
            cubeb_log!(
                "Cannot get the UID for device {} in {:?} scope. Error: {}",
                devid,
                devtype,
                e
            );
        }
    }

    match get_device_group_id(devid, devtype) {
        Ok(group_id) => {
            dev_info.group_id = group_id.into_raw();
        }
        Err(e) => {
            cubeb_log!(
                "Cannot get the model UID for device {} in {:?} scope. Error: {}",
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
    dev_info.preferred = match get_default_device(devtype) {
        Some(id) if id == devid => ffi::CUBEB_DEVICE_PREF_ALL,
        _ => ffi::CUBEB_DEVICE_PREF_NONE,
    };

    dev_info.format = ffi::CUBEB_DEVICE_FMT_ALL;
    dev_info.default_format = ffi::CUBEB_DEVICE_FMT_F32NE;

    match get_device_sample_rate(devid, devtype) {
        Ok(rate) => {
            dev_info.default_rate = rate as u32;
        }
        Err(e) => {
            cubeb_log!(
                "Cannot get the sample rate for device {} in {:?} scope. Error: {}",
                devid,
                devtype,
                e
            );
        }
    }

    match get_range_of_sample_rates(devid, devtype) {
        Ok((min, max)) => {
            dev_info.min_rate = min as u32;
            dev_info.max_rate = max as u32;
        }
        Err(e) => {
            cubeb_log!(
                "Cannot get the range of sample rate for device {} in {:?} scope. Error: {}",
                devid,
                devtype,
                e
            );
        }
    }

    let latency = get_fixed_latency(devid, devtype);

    let (latency_low, latency_high) = match get_device_buffer_frame_size_range(devid, devtype) {
        Ok(range) => (
            latency + range.mMinimum as u32,
            latency + range.mMaximum as u32,
        ),
        Err(e) => {
            cubeb_log!("Cannot get the buffer frame size for device {} in {:?} scope. Using default value instead. Error: {}", devid, devtype, e);
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

fn destroy_cubeb_device_info(device: &mut ffi::cubeb_device_info) {
    // This should be mapped to the memory allocation in `create_cubeb_device_info`.
    // The `device_id`, `group_id`, `vendor_name` can be null pointer if the queries
    // failed, while `friendly_name` will be assigned to a default empty "" string.
    // Set the pointers to null in case it points to some released memory.
    unsafe {
        if !device.device_id.is_null() {
            let _ = CString::from_raw(device.device_id as *mut _);
            device.device_id = ptr::null();
        }

        if !device.group_id.is_null() {
            let _ = CString::from_raw(device.group_id as *mut _);
            device.group_id = ptr::null();
        }

        assert!(!device.friendly_name.is_null());
        let _ = CString::from_raw(device.friendly_name as *mut _);
        device.friendly_name = ptr::null();

        if !device.vendor_name.is_null() {
            let _ = CString::from_raw(device.vendor_name as *mut _);
            device.vendor_name = ptr::null();
        }
    }
}

fn audiounit_get_devices() -> Vec<AudioObjectID> {
    let mut size: usize = 0;
    let address = get_property_address(
        Property::HardwareDevices,
        DeviceType::INPUT | DeviceType::OUTPUT,
    );
    let mut ret =
        audio_object_get_property_data_size(kAudioObjectSystemObject, &address, &mut size);
    if ret != NO_ERR {
        return Vec::new();
    }
    // Total number of input and output devices.
    let mut devices: Vec<AudioObjectID> = allocate_array_by_size(size);
    ret = audio_object_get_property_data(
        kAudioObjectSystemObject,
        &address,
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
        // TODO: (bug 1628411) Figure out when `device` is `kAudioObjectUnknown`.
        if device == kAudioObjectUnknown {
            false
        } else if let Ok(uid) = get_device_global_uid(device) {
            let uid = uid.into_string();
            !uid.contains(PRIVATE_AGGREGATE_DEVICE_NAME)
                && !uid.contains(VOICEPROCESSING_AGGREGATE_DEVICE_NAME)
        } else {
            // Fail to get device uid.
            true
        }
    });

    // Expected sorted but did not find anything in the docs.
    devices.sort_unstable();
    if devtype.contains(DeviceType::INPUT | DeviceType::OUTPUT) {
        return devices;
    }

    let mut devices_in_scope = Vec::new();
    for device in devices {
        let label = match get_device_label(device, DeviceType::OUTPUT | DeviceType::INPUT) {
            Ok(label) => label.into_string(),
            Err(e) => format!("Unknown(error: {})", e),
        };
        let info = format!("{} ({})", device, label);

        if let Ok(channels) = get_channel_count(device, devtype) {
            cubeb_log!("Device {} has {} {:?}-channels", info, channels, devtype);
            if channels > 0 {
                devices_in_scope.push(device);
            }
        } else {
            cubeb_log!("Cannot get the channel count for device {}. Ignored.", info);
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

    let queue = context.serial_queue.clone();

    // This can be called from inside an AudioUnit function, dispatch to another queue.
    queue.run_async(move || {
        let ctx_ptr = context as *const AudioUnitContext;

        let mut devices = context.devices.lock().unwrap();

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
        self.changed_callback.is_none()
            && self.callback_user_ptr.is_null()
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

#[derive(Debug, Default)]
struct SharedDevices {
    input: DevicesData,
    output: DevicesData,
}

#[derive(Debug, Default)]
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
            self.latency = Some(latency.clamp(SAFE_MIN_LATENCY_FRAMES, SAFE_MAX_LATENCY_FRAMES));
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

pub const OPS: Ops = capi_new!(AudioUnitContext, AudioUnitStream);

// The fisrt member of the Cubeb context must be a pointer to a Ops struct. The Ops struct is an
// interface to link to all the Cubeb APIs, and the Cubeb interface use this assumption to operate
// the Cubeb APIs on different implementation.
// #[repr(C)] is used to prevent any padding from being added in the beginning of the AudioUnitContext.
#[repr(C)]
#[derive(Debug)]
pub struct AudioUnitContext {
    _ops: *const Ops,
    serial_queue: Queue,
    latency_controller: Mutex<LatencyController>,
    devices: Mutex<SharedDevices>,
}

impl AudioUnitContext {
    fn new() -> Self {
        Self {
            _ops: &OPS as *const _,
            serial_queue: Queue::new(DISPATCH_QUEUE_LABEL),
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
            let address = get_property_address(
                Property::HardwareDevices,
                DeviceType::INPUT | DeviceType::OUTPUT,
            );
            let ret = audio_object_add_property_listener(
                kAudioObjectSystemObject,
                &address,
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

        let address = get_property_address(
            Property::HardwareDevices,
            DeviceType::INPUT | DeviceType::OUTPUT,
        );
        // Note: unregister a non registered cb is not a problem, not checking.
        let r = audio_object_remove_property_listener(
            kAudioObjectSystemObject,
            &address,
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
        let device = match get_default_device(DeviceType::OUTPUT) {
            None => {
                cubeb_log!("Could not get default output device");
                return Err(Error::error());
            }
            Some(id) => id,
        };
        get_channel_count(device, DeviceType::OUTPUT).map_err(|e| {
            cubeb_log!("Cannot get the channel count. Error: {}", e);
            Error::error()
        })
    }
    #[cfg(target_os = "ios")]
    fn min_latency(&mut self, _params: StreamParams) -> Result<u32> {
        Err(not_supported());
    }
    #[cfg(not(target_os = "ios"))]
    fn min_latency(&mut self, _params: StreamParams) -> Result<u32> {
        let device = match get_default_device(DeviceType::OUTPUT) {
            None => {
                cubeb_log!("Could not get default output device");
                return Err(Error::error());
            }
            Some(id) => id,
        };

        let range =
            get_device_buffer_frame_size_range(device, DeviceType::OUTPUT).map_err(|e| {
                cubeb_log!("Could not get acceptable latency range. Error: {}", e);
                Error::error()
            })?;

        Ok(cmp::max(range.mMinimum as u32, SAFE_MIN_LATENCY_FRAMES))
    }
    #[cfg(target_os = "ios")]
    fn preferred_sample_rate(&mut self) -> Result<u32> {
        Err(not_supported());
    }
    #[cfg(not(target_os = "ios"))]
    fn preferred_sample_rate(&mut self) -> Result<u32> {
        let device = match get_default_device(DeviceType::OUTPUT) {
            None => {
                cubeb_log!("Could not get default output device");
                return Err(Error::error());
            }
            Some(id) => id,
        };
        let rate = get_device_sample_rate(device, DeviceType::OUTPUT).map_err(|e| {
            cubeb_log!(
                "Cannot get the sample rate of the default output device. Error: {}",
                e
            );
            Error::error()
        })?;
        Ok(rate as u32)
    }
    fn supported_input_processing_params(&mut self) -> Result<InputProcessingParams> {
        Ok(InputProcessingParams::NONE)
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
                    device_infos.push(info);
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
            destroy_cubeb_device_info(device);
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
        if !input_device.is_null() && input_stream_params.is_none() {
            cubeb_log!("Cannot init an input device without input stream params");
            return Err(Error::invalid_parameter());
        }

        if !output_device.is_null() && output_stream_params.is_none() {
            cubeb_log!("Cannot init an output device without output stream params");
            return Err(Error::invalid_parameter());
        }

        if input_stream_params.is_none() && output_stream_params.is_none() {
            cubeb_log!("Cannot init a stream without any stream params");
            return Err(Error::invalid_parameter());
        }

        if data_callback.is_none() {
            cubeb_log!("Cannot init a stream without a data callback");
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
            let in_device =
                match create_device_info(input_device as AudioDeviceID, DeviceType::INPUT) {
                    None => {
                        cubeb_log!("Fail to create device info for input");
                        return Err(Error::error());
                    }
                    Some(d) => d,
                };
            let stm_params = StreamParams::from(unsafe { *params.as_ptr() });
            Some((stm_params, in_device))
        } else {
            None
        };

        let out_stm_settings = if let Some(params) = output_stream_params {
            let out_device =
                match create_device_info(output_device as AudioDeviceID, DeviceType::OUTPUT) {
                    None => {
                        cubeb_log!("Fail to create device info for output");
                        return Err(Error::error());
                    }
                    Some(d) => d,
                };
            let stm_params = StreamParams::from(unsafe { *params.as_ptr() });
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

        // Rename the task queue to be an unique label.
        let queue_label = format!("{}.{:p}", DISPATCH_QUEUE_LABEL, boxed_stream.as_ref());
        boxed_stream.queue = Queue::new(queue_label.as_str());

        boxed_stream.core_stream_data =
            CoreStreamData::new(boxed_stream.as_ref(), in_stm_settings, out_stm_settings);

        let mut result = Ok(());
        boxed_stream.queue.clone().run_sync(|| {
            result = boxed_stream.core_stream_data.setup();
        });
        if let Err(r) = result {
            cubeb_log!(
                "({:p}) Could not setup the audiounit stream.",
                boxed_stream.as_ref()
            );
            return Err(r);
        }

        let cubeb_stream = unsafe { Stream::from_ptr(Box::into_raw(boxed_stream) as *mut _) };
        cubeb_log!(
            "({:p}) Cubeb stream init successful.",
            cubeb_stream.as_ref()
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
        let devices = self.devices.lock().unwrap();
        assert!(
            devices.input.changed_callback.is_none() && devices.output.changed_callback.is_none()
        );

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
        // Make sure all the pending (device-collection-changed-callback) tasks
        // in queue are done, and cancel all the tasks appended after `drop` is executed.
        let queue = self.serial_queue.clone();
        queue.run_final(|| {});
    }
}

#[allow(clippy::non_send_fields_in_send_ty)]
unsafe impl Send for AudioUnitContext {}
unsafe impl Sync for AudioUnitContext {}

// Holds the information for an audio input callback call, for debugging purposes.
struct InputCallbackData {
    bytes: u32,
    rendered_frames: u32,
    total_available: usize,
    channels: u32,
    num_buf: u32,
}
struct InputCallbackLogger {
    prod: ringbuf::Producer<InputCallbackData>,
    cons: ringbuf::Consumer<InputCallbackData>,
}

impl InputCallbackLogger {
    fn new() -> Self {
        let ring = RingBuffer::<InputCallbackData>::new(16);
        let (prod, cons) = ring.split();
        Self { prod, cons }
    }

    fn push(&mut self, data: InputCallbackData) {
        self.prod.push(data);
    }

    fn pop(&mut self) -> Option<InputCallbackData> {
        self.cons.pop()
    }

    fn is_empty(&self) -> bool {
        self.cons.is_empty()
    }
}

impl fmt::Debug for InputCallbackLogger {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "InputCallbackLogger  {{ prod: {}, cons: {} }}",
            self.prod.len(),
            self.cons.len()
        )
    }
}

#[derive(Debug)]
struct CoreStreamData<'ctx> {
    stm_ptr: *const AudioUnitStream<'ctx>,
    aggregate_device: Option<AggregateDevice>,
    mixer: Option<Mixer>,
    resampler: Resampler,
    // Stream creation parameters.
    input_stream_params: StreamParams,
    output_stream_params: StreamParams,
    // Device settings for AudioUnits.
    input_dev_desc: AudioStreamBasicDescription,
    output_dev_desc: AudioStreamBasicDescription,
    // I/O AudioUnits.
    input_unit: AudioUnit,
    output_unit: AudioUnit,
    // Info of the I/O devices.
    input_device: device_info,
    output_device: device_info,
    input_buffer_manager: Option<BufferManager>,
    // Listeners indicating what system events are monitored.
    default_input_listener: Option<device_property_listener>,
    default_output_listener: Option<device_property_listener>,
    input_alive_listener: Option<device_property_listener>,
    input_source_listener: Option<device_property_listener>,
    output_alive_listener: Option<device_property_listener>,
    output_source_listener: Option<device_property_listener>,
    input_logging: Option<InputCallbackLogger>,
}

impl<'ctx> Default for CoreStreamData<'ctx> {
    fn default() -> Self {
        Self {
            stm_ptr: ptr::null(),
            aggregate_device: None,
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
            input_dev_desc: AudioStreamBasicDescription::default(),
            output_dev_desc: AudioStreamBasicDescription::default(),
            input_unit: ptr::null_mut(),
            output_unit: ptr::null_mut(),
            input_device: device_info::default(),
            output_device: device_info::default(),
            input_buffer_manager: None,
            default_input_listener: None,
            default_output_listener: None,
            input_alive_listener: None,
            input_source_listener: None,
            output_alive_listener: None,
            output_source_listener: None,
            input_logging: None,
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
            aggregate_device: None,
            mixer: None,
            resampler: Resampler::default(),
            input_stream_params: in_stm_params,
            output_stream_params: out_stm_params,
            input_dev_desc: AudioStreamBasicDescription::default(),
            output_dev_desc: AudioStreamBasicDescription::default(),
            input_unit: ptr::null_mut(),
            output_unit: ptr::null_mut(),
            input_device: in_dev,
            output_device: out_dev,
            input_buffer_manager: None,
            default_input_listener: None,
            default_output_listener: None,
            input_alive_listener: None,
            input_source_listener: None,
            output_alive_listener: None,
            output_source_listener: None,
            input_logging: None,
        }
    }

    fn debug_assert_is_on_stream_queue(&self) {
        if self.stm_ptr.is_null() {
            return;
        }
        let stm = unsafe { &*self.stm_ptr };
        stm.queue.debug_assert_is_current();
    }

    fn start_audiounits(&self) -> Result<()> {
        self.debug_assert_is_on_stream_queue();
        // Only allowed to be called after the stream is initialized
        // and before the stream is destroyed.
        debug_assert!(!self.input_unit.is_null() || !self.output_unit.is_null());

        if !self.input_unit.is_null() {
            start_audiounit(self.input_unit)?;
        }
        if self.using_voice_processing_unit() {
            // Handle the VoiceProcessIO case where there is a single unit.
            return Ok(());
        }
        if !self.output_unit.is_null() {
            start_audiounit(self.output_unit)?;
        }
        Ok(())
    }

    fn stop_audiounits(&self) {
        self.debug_assert_is_on_stream_queue();
        if !self.input_unit.is_null() {
            let r = stop_audiounit(self.input_unit);
            assert!(r.is_ok());
        }
        if self.using_voice_processing_unit() {
            // Handle the VoiceProcessIO case where there is a single unit.
            return;
        }
        if !self.output_unit.is_null() {
            let r = stop_audiounit(self.output_unit);
            assert!(r.is_ok());
        }
    }

    fn has_input(&self) -> bool {
        self.input_stream_params.rate() > 0
    }

    fn has_output(&self) -> bool {
        self.output_stream_params.rate() > 0
    }

    fn using_voice_processing_unit(&self) -> bool {
        !self.input_unit.is_null() && self.input_unit == self.output_unit
    }

    fn same_clock_domain(&self) -> bool {
        self.debug_assert_is_on_stream_queue();
        // If not setting up a duplex stream, there is only one device,
        // no reclocking necessary.
        if !(self.has_input() && self.has_output()) {
            return true;
        }
        let input_domain = match get_clock_domain(self.input_device.id, DeviceType::INPUT) {
            Ok(clock_domain) => clock_domain,
            Err(_) => {
                cubeb_log!("Coudn't determine clock domains for input.");
                return false;
            }
        };

        let output_domain = match get_clock_domain(self.output_device.id, DeviceType::OUTPUT) {
            Ok(clock_domain) => clock_domain,
            Err(_) => {
                cubeb_log!("Coudn't determine clock domains for input.");
                return false;
            }
        };
        input_domain == output_domain
    }

    fn create_audiounits(&mut self) -> Result<(device_info, device_info)> {
        self.debug_assert_is_on_stream_queue();
        let should_use_voice_processing_unit = self.has_input()
            && self.has_output()
            && self
                .input_stream_params
                .prefs()
                .contains(StreamPrefs::VOICE);

        let should_use_aggregate_device = {
            // It's impossible to create an aggregate device from an aggregate device, and it's
            // unnecessary to create an aggregate device when opening the same device input/output. In
            // all other cases, use an aggregate device.
            let mut either_already_aggregate = false;
            if self.has_input() {
                let input_is_aggregate =
                    get_device_transport_type(self.input_device.id, DeviceType::INPUT).unwrap_or(0)
                        == kAudioDeviceTransportTypeAggregate;
                if input_is_aggregate {
                    either_already_aggregate = true;
                }
                cubeb_log!(
                    "Input device ID: {} (aggregate: {:?})",
                    self.input_device.id,
                    input_is_aggregate
                );
            }
            if self.has_output() {
                let output_is_aggregate =
                    get_device_transport_type(self.output_device.id, DeviceType::OUTPUT)
                        .unwrap_or(0)
                        == kAudioDeviceTransportTypeAggregate;
                if output_is_aggregate {
                    either_already_aggregate = true;
                }
                cubeb_log!(
                    "Output device ID: {} (aggregate: {:?})",
                    self.input_device.id,
                    output_is_aggregate
                );
            }
            // Only use an aggregate device when the device are different.
            self.has_input()
                && self.has_output()
                && self.input_device.id != self.output_device.id
                && !either_already_aggregate
        };

        // Create an AudioUnit:
        // - If we're eligible to use voice processing, try creating a VoiceProcessingIO AudioUnit.
        // - If we should use an aggregate device, try creating one and input and output AudioUnits next.
        // - As last resort, create regular AudioUnits. This is also the normal non-duplex path.

        if should_use_voice_processing_unit {
            if let Ok(au) =
                create_voiceprocessing_audiounit(&self.input_device, &self.output_device)
            {
                cubeb_log!("({:p}) Using VoiceProcessingIO AudioUnit", self.stm_ptr);
                self.input_unit = au;
                self.output_unit = au;
                return Ok((self.input_device.clone(), self.output_device.clone()));
            }
            cubeb_log!(
                "({:p}) Failed to create VoiceProcessingIO AudioUnit. Trying a regular one.",
                self.stm_ptr
            );
        }

        if should_use_aggregate_device {
            if let Ok(device) = AggregateDevice::new(self.input_device.id, self.output_device.id) {
                let in_dev_info = {
                    device_info {
                        id: device.get_device_id(),
                        ..self.input_device
                    }
                };
                let out_dev_info = {
                    device_info {
                        id: device.get_device_id(),
                        ..self.output_device
                    }
                };

                match (
                    create_audiounit(&in_dev_info),
                    create_audiounit(&out_dev_info),
                ) {
                    (Ok(in_au), Ok(out_au)) => {
                        cubeb_log!(
                            "({:p}) Using an aggregate device {} for input and output.",
                            self.stm_ptr,
                            device.get_device_id()
                        );
                        self.aggregate_device = Some(device);
                        self.input_unit = in_au;
                        self.output_unit = out_au;
                        return Ok((in_dev_info, out_dev_info));
                    }
                    (Err(e), Ok(au)) => {
                        cubeb_log!(
                            "({:p}) Failed to create input AudioUnit for aggregate device. Error: {}.",
                            self.stm_ptr,
                            e
                        );
                        dispose_audio_unit(au);
                    }
                    (Ok(au), Err(e)) => {
                        cubeb_log!(
                            "({:p}) Failed to create output AudioUnit for aggregate device. Error: {}.",
                            self.stm_ptr,
                            e
                        );
                        dispose_audio_unit(au);
                    }
                    (Err(e), _) => {
                        cubeb_log!(
                            "({:p}) Failed to create AudioUnits for aggregate device. Error: {}.",
                            self.stm_ptr,
                            e
                        );
                    }
                }
            }
            cubeb_log!(
                "({:p}) Failed to set up aggregate device. Using regular AudioUnits.",
                self.stm_ptr
            );
        }

        if self.has_input() {
            match create_audiounit(&self.input_device) {
                Ok(in_au) => self.input_unit = in_au,
                Err(e) => {
                    cubeb_log!(
                        "({:p}) Failed to create regular AudioUnit for input. Error: {}",
                        self.stm_ptr,
                        e
                    );
                    return Err(e);
                }
            }
        }

        if self.has_output() {
            match create_audiounit(&self.output_device) {
                Ok(out_au) => self.output_unit = out_au,
                Err(e) => {
                    cubeb_log!(
                        "({:p}) Failed to create regular AudioUnit for output. Error: {}",
                        self.stm_ptr,
                        e
                    );
                    if !self.input_unit.is_null() {
                        dispose_audio_unit(self.input_unit);
                        self.input_unit = ptr::null_mut();
                    }
                    return Err(e);
                }
            }
        }

        Ok((self.input_device.clone(), self.output_device.clone()))
    }

    #[allow(clippy::cognitive_complexity)] // TODO: Refactoring.
    fn setup(&mut self) -> Result<()> {
        self.debug_assert_is_on_stream_queue();
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

        let same_clock_domain = self.same_clock_domain();
        let (in_dev_info, out_dev_info) = self.create_audiounits()?;
        let using_voice_processing_unit = self.using_voice_processing_unit();

        assert!(!self.stm_ptr.is_null());
        let stream = unsafe { &(*self.stm_ptr) };

        // Configure I/O stream
        if self.has_input() {
            assert!(!self.input_unit.is_null());

            cubeb_log!(
                "({:p}) Initializing input by device info: {:?}",
                self.stm_ptr,
                in_dev_info
            );

            let device_channel_count =
                get_channel_count(self.input_device.id, DeviceType::INPUT).unwrap_or(0);
            if device_channel_count < self.input_stream_params.channels() {
                cubeb_log!(
                    "({:p}) Invalid input channel count; device={}, params={}",
                    self.stm_ptr,
                    device_channel_count,
                    self.input_stream_params.channels()
                );
                return Err(Error::invalid_parameter());
            }

            cubeb_log!(
                "({:p}) Opening input side: rate {}, channels {}, format {:?}, layout {:?}, prefs {:?}, latency in frames {}, voice processing {}.",
                self.stm_ptr,
                self.input_stream_params.rate(),
                self.input_stream_params.channels(),
                self.input_stream_params.format(),
                self.input_stream_params.layout(),
                self.input_stream_params.prefs(),
                stream.latency_frames,
                using_voice_processing_unit
            );

            // Get input device hardware information.
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
            // In some cases with VPIO the stream format's mChannelsPerFrame is higher than
            // expected. Use get_channel_count as source of truth.
            input_hw_desc.mChannelsPerFrame = device_channel_count;
            // Notice: when we are using aggregate device, the input_hw_desc.mChannelsPerFrame is
            // the total of all the input channel count of the devices added in the aggregate device.
            // Due to our aggregate device settings, the data captured by the output device's input
            // channels will be put in the beginning of the raw data given by the input callback.

            // Always request all the input channels of the device, and only pass the correct
            // channels to the audio callback.
            let params = unsafe {
                let mut p = *self.input_stream_params.as_ptr();
                p.channels = if using_voice_processing_unit {
                    // With VPIO, stereo input devices configured for stereo have been observed to
                    // spit out only a single mono channel.
                    1
                } else {
                    input_hw_desc.mChannelsPerFrame
                };
                // Input AudioUnit must be configured with device's sample rate.
                // we will resample inside input callback.
                p.rate = input_hw_desc.mSampleRate as _;
                StreamParams::from(p)
            };

            self.input_dev_desc = create_stream_description(&params).map_err(|e| {
                cubeb_log!(
                    "({:p}) Setting format description for input failed.",
                    self.stm_ptr
                );
                e
            })?;

            assert_eq!(self.input_dev_desc.mSampleRate, input_hw_desc.mSampleRate);

            // Use latency to set buffer size
            assert_ne!(stream.latency_frames, 0);
            if let Err(r) =
                set_buffer_size_sync(self.input_unit, DeviceType::INPUT, stream.latency_frames)
            {
                cubeb_log!("({:p}) Error in change input buffer size.", self.stm_ptr);
                return Err(r);
            }

            let r = audio_unit_set_property(
                self.input_unit,
                kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Output,
                AU_IN_BUS,
                &self.input_dev_desc,
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

            // When we use the aggregate device, the self.input_dev_desc.mChannelsPerFrame is the
            // total input channel count of all the device added in the aggregate device. However,
            // we only need the audio data captured by the requested input device, so we need to
            // ignore some data captured by the audio input of the requested output device (e.g.,
            // the requested output device is a USB headset with built-in mic), in the beginning of
            // the raw data taken from input callback.
            self.input_buffer_manager = Some(BufferManager::new(
                self.input_stream_params.format(),
                SAFE_MAX_LATENCY_FRAMES as usize,
                self.input_dev_desc.mChannelsPerFrame as usize,
                self.input_dev_desc
                    .mChannelsPerFrame
                    .saturating_sub(device_channel_count) as usize,
                self.input_stream_params.channels() as usize,
            ));

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
            assert!(!self.output_unit.is_null());

            cubeb_log!(
                "({:p}) Initialize output by device info: {:?}",
                self.stm_ptr,
                out_dev_info
            );

            let device_channel_count =
                get_channel_count(self.output_device.id, DeviceType::OUTPUT).unwrap_or(0);

            cubeb_log!(
                "({:p}) Opening output side: rate {}, channels {}, format {:?}, layout {:?}, prefs {:?}, latency in frames {}, voice processing {}.",
                self.stm_ptr,
                self.output_stream_params.rate(),
                self.output_stream_params.channels(),
                self.output_stream_params.format(),
                self.output_stream_params.layout(),
                self.output_stream_params.prefs(),
                stream.latency_frames,
                using_voice_processing_unit
            );

            // Get output device hardware information.
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

            // In some cases with VPIO the stream format's mChannelsPerFrame is higher than
            // expected. Use get_channel_count as source of truth.
            output_hw_desc.mChannelsPerFrame = device_channel_count;

            // This has been observed in the wild.
            if output_hw_desc.mChannelsPerFrame == 0 {
                cubeb_log!(
                    "({:p}) Output hardware description channel count is zero",
                    self.stm_ptr
                );
                return Err(Error::error());
            }

            // Notice: when we are using aggregate device, the output_hw_desc.mChannelsPerFrame is
            // the total of all the output channel count of the devices added in the aggregate device.
            // Due to our aggregate device settings, the data recorded by the input device's output
            // channels will be appended at the end of the raw data given by the output callback.
            let params = unsafe {
                let mut p = *self.output_stream_params.as_ptr();
                p.channels = output_hw_desc.mChannelsPerFrame;
                if using_voice_processing_unit {
                    // VPIO will always use the sample rate of the input hw for both input and output,
                    // as reported to us. (We can override it but we cannot improve quality this way).
                    p.rate = self.input_dev_desc.mSampleRate as _;
                }
                StreamParams::from(p)
            };

            self.output_dev_desc = create_stream_description(&params).map_err(|e| {
                cubeb_log!(
                    "({:p}) Could not initialize the audio stream description.",
                    self.stm_ptr
                );
                e
            })?;

            let device_layout = self
                .get_output_channel_layout()
                .map_err(|e| {
                    cubeb_log!(
                        "({:p}) Could not get any channel layout. Defaulting to no channels.",
                        self.stm_ptr
                    );
                    e
                })
                .unwrap_or_default();

            cubeb_log!(
                "({:p} Using output device channel layout {:?}",
                self.stm_ptr,
                device_layout
            );

            // The mixer will be set up when
            // 1. using aggregate device whose input device has output channels
            // 2. output device has more channels than we need
            // 3. output device has different layout than the one we have
            self.mixer = if self.output_dev_desc.mChannelsPerFrame
                != self.output_stream_params.channels()
                || device_layout != mixer::get_channel_order(self.output_stream_params.layout())
            {
                cubeb_log!("Incompatible channel layouts detected, setting up remixer");
                // We will be remixing the data before it reaches the output device.
                Some(Mixer::new(
                    self.output_stream_params.format(),
                    self.output_stream_params.channels() as usize,
                    self.output_stream_params.layout(),
                    self.output_dev_desc.mChannelsPerFrame as usize,
                    device_layout,
                ))
            } else {
                None
            };

            let r = audio_unit_set_property(
                self.output_unit,
                kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Input,
                AU_OUT_BUS,
                &self.output_dev_desc,
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
                set_buffer_size_sync(self.output_unit, DeviceType::OUTPUT, stream.latency_frames)
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
            let mut p = unsafe { *(self.input_stream_params.as_ptr()) };
            p.rate = self.input_dev_desc.mSampleRate as u32;
            Some(p)
        } else {
            None
        };
        let resampler_output_params = if self.has_output() {
            let mut p = unsafe { *(self.output_stream_params.as_ptr()) };
            p.rate = self.output_dev_desc.mSampleRate as u32;
            Some(p)
        } else {
            None
        };

        // Only reclock if there is an input and we couldn't use an aggregate device, and the
        // devices are not part of the same clock domain.
        let reclock_policy = if self.aggregate_device.is_none()
            && !using_voice_processing_unit
            && !same_clock_domain
        {
            cubeb_log!(
                "Reclocking duplex steam using_aggregate_device={} same_clock_domain={}",
                self.aggregate_device.is_some(),
                same_clock_domain
            );
            ffi::CUBEB_RESAMPLER_RECLOCK_INPUT
        } else {
            ffi::CUBEB_RESAMPLER_RECLOCK_NONE
        };

        self.resampler = Resampler::new(
            self.stm_ptr as *mut ffi::cubeb_stream,
            resampler_input_params,
            resampler_output_params,
            target_sample_rate,
            stream.data_callback,
            stream.user_ptr,
            ffi::CUBEB_RESAMPLER_QUALITY_DESKTOP,
            reclock_policy,
        );

        // In duplex, the input thread might be different from the output thread, and we're logging
        // everything from the output thread: relay the audio input callback information using a
        // ring buffer to diagnose issues.
        if self.has_input() && self.has_output() {
            self.input_logging = Some(InputCallbackLogger::new());
        }

        if !self.input_unit.is_null() {
            let r = audio_unit_initialize(self.input_unit);
            if r != NO_ERR {
                cubeb_log!("AudioUnitInitialize/input rv={}", r);
                return Err(Error::error());
            }

            stream.input_device_latency_frames.store(
                get_fixed_latency(self.input_device.id, DeviceType::INPUT),
                Ordering::SeqCst,
            );
        }

        if !self.output_unit.is_null() {
            if self.input_unit != self.output_unit {
                let r = audio_unit_initialize(self.output_unit);
                if r != NO_ERR {
                    cubeb_log!("AudioUnitInitialize/output rv={}", r);
                    return Err(Error::error());
                }
            }

            stream.output_device_latency_frames.store(
                get_fixed_latency(self.output_device.id, DeviceType::OUTPUT),
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
                stream.output_device_latency_frames.fetch_add(
                    (unit_s * self.output_dev_desc.mSampleRate) as u32,
                    Ordering::SeqCst,
                );
            }
        }

        if using_voice_processing_unit {
            // The VPIO AudioUnit automatically ducks other audio streams on the VPIO
            // output device. Its ramp duration is 0.5s when ducking, so unduck similarly
            // now.
            // NOTE: On MacOS 14 the ducking happens on creation of the VPIO AudioUnit.
            //       On MacOS 10.15 it happens on both creation and initialization, which
            //       is why we defer the unducking until now.
            let r = audio_device_duck(self.output_device.id, 1.0, ptr::null_mut(), 0.5);
            if r != NO_ERR {
                cubeb_log!(
                    "({:p}) Failed to undo ducking of voiceprocessing on output device {}. Proceeding... Error: {}",
                    self.stm_ptr,
                    self.output_device.id,
                    r
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

        // We have either default_input_listener or input_alive_listener.
        // We cannot have both of them at the same time.
        assert!(
            !self.has_input()
                || ((self.default_input_listener.is_some() != self.input_alive_listener.is_some())
                    && (self.default_input_listener.is_some()
                        || self.input_alive_listener.is_some()))
        );

        // We have either default_output_listener or output_alive_listener.
        // We cannot have both of them at the same time.
        assert!(
            !self.has_output()
                || ((self.default_output_listener.is_some()
                    != self.output_alive_listener.is_some())
                    && (self.default_output_listener.is_some()
                        || self.output_alive_listener.is_some()))
        );

        Ok(())
    }

    fn close(&mut self) {
        self.debug_assert_is_on_stream_queue();
        if !self.input_unit.is_null() {
            audio_unit_uninitialize(self.input_unit);
            if self.using_voice_processing_unit() {
                // Handle the VoiceProcessIO case where there is a single unit.
                self.output_unit = ptr::null_mut();
            }

            // Cannot unset self.input_unit yet, since the output callback might be live
            // and reading it.
        }

        if !self.output_unit.is_null() {
            audio_unit_uninitialize(self.output_unit);
            dispose_audio_unit(self.output_unit);
            self.output_unit = ptr::null_mut();
        }

        if !self.input_unit.is_null() {
            dispose_audio_unit(self.input_unit);
            self.input_unit = ptr::null_mut();
        }

        self.resampler.destroy();
        self.mixer = None;
        self.aggregate_device = None;

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
        self.debug_assert_is_on_stream_queue();
        assert!(!self.stm_ptr.is_null());
        let stm = unsafe { &(*self.stm_ptr) };

        if !self.output_unit.is_null() {
            assert_ne!(self.output_device.id, kAudioObjectUnknown);
            assert_ne!(self.output_device.id, kAudioObjectSystemObject);
            assert!(
                self.output_source_listener.is_none(),
                "register output_source_listener without unregistering the one in use"
            );
            assert!(
                self.output_alive_listener.is_none(),
                "register output_alive_listener without unregistering the one in use"
            );

            // Get the notification when the data source on the same device changes,
            // e.g., when the user plugs in a TRRS headset into the headphone jack.
            self.output_source_listener = Some(device_property_listener::new(
                self.output_device.id,
                get_property_address(Property::DeviceSource, DeviceType::OUTPUT),
                audiounit_property_listener_callback,
            ));
            let rv = stm.add_device_listener(self.output_source_listener.as_ref().unwrap());
            if rv != NO_ERR {
                self.output_source_listener = None;
                cubeb_log!("AudioObjectAddPropertyListener/output/kAudioDevicePropertyDataSource rv={}, device id={}", rv, self.output_device.id);
                return Err(Error::error());
            }

            // Get the notification when the output device is going away
            // if the output doesn't follow the system default.
            if !self
                .output_device
                .flags
                .contains(device_flags::DEV_SELECTED_DEFAULT)
            {
                self.output_alive_listener = Some(device_property_listener::new(
                    self.output_device.id,
                    get_property_address(
                        Property::DeviceIsAlive,
                        DeviceType::INPUT | DeviceType::OUTPUT,
                    ),
                    audiounit_property_listener_callback,
                ));
                let rv = stm.add_device_listener(self.output_alive_listener.as_ref().unwrap());
                if rv != NO_ERR {
                    self.output_alive_listener = None;
                    cubeb_log!("AudioObjectAddPropertyListener/output/kAudioDevicePropertyDeviceIsAlive rv={}, device id ={}", rv, self.output_device.id);
                    return Err(Error::error());
                }
            }
        }

        if !self.input_unit.is_null() {
            assert_ne!(self.input_device.id, kAudioObjectUnknown);
            assert_ne!(self.input_device.id, kAudioObjectSystemObject);
            assert!(
                self.input_source_listener.is_none(),
                "register input_source_listener without unregistering the one in use"
            );
            assert!(
                self.input_alive_listener.is_none(),
                "register input_alive_listener without unregistering the one in use"
            );

            // Get the notification when the data source on the same device changes,
            // e.g., when the user plugs in a TRRS mic into the headphone jack.
            self.input_source_listener = Some(device_property_listener::new(
                self.input_device.id,
                get_property_address(Property::DeviceSource, DeviceType::INPUT),
                audiounit_property_listener_callback,
            ));
            let rv = stm.add_device_listener(self.input_source_listener.as_ref().unwrap());
            if rv != NO_ERR {
                self.input_source_listener = None;
                cubeb_log!("AudioObjectAddPropertyListener/input/kAudioDevicePropertyDataSource rv={}, device id={}", rv, self.input_device.id);
                return Err(Error::error());
            }

            // Get the notification when the input device is going away
            // if the input doesn't follow the system default.
            if !self
                .input_device
                .flags
                .contains(device_flags::DEV_SELECTED_DEFAULT)
            {
                self.input_alive_listener = Some(device_property_listener::new(
                    self.input_device.id,
                    get_property_address(
                        Property::DeviceIsAlive,
                        DeviceType::INPUT | DeviceType::OUTPUT,
                    ),
                    audiounit_property_listener_callback,
                ));
                let rv = stm.add_device_listener(self.input_alive_listener.as_ref().unwrap());
                if rv != NO_ERR {
                    self.input_alive_listener = None;
                    cubeb_log!("AudioObjectAddPropertyListener/input/kAudioDevicePropertyDeviceIsAlive rv={}, device id ={}", rv, self.input_device.id);
                    return Err(Error::error());
                }
            }
        }

        Ok(())
    }

    fn install_system_changed_callback(&mut self) -> Result<()> {
        self.debug_assert_is_on_stream_queue();
        assert!(!self.stm_ptr.is_null());
        let stm = unsafe { &(*self.stm_ptr) };

        if !self.output_unit.is_null()
            && self
                .output_device
                .flags
                .contains(device_flags::DEV_SELECTED_DEFAULT)
        {
            assert!(
                self.default_output_listener.is_none(),
                "register default_output_listener without unregistering the one in use"
            );

            // Get the notification when the default output audio changes, e.g.,
            // when the user plugs in a USB headset and the system chooses it automatically as the default,
            // or when another device is chosen in the dropdown list.
            self.default_output_listener = Some(device_property_listener::new(
                kAudioObjectSystemObject,
                get_property_address(
                    Property::HardwareDefaultOutputDevice,
                    DeviceType::INPUT | DeviceType::OUTPUT,
                ),
                audiounit_property_listener_callback,
            ));
            let r = stm.add_device_listener(self.default_output_listener.as_ref().unwrap());
            if r != NO_ERR {
                self.default_output_listener = None;
                cubeb_log!("AudioObjectAddPropertyListener/output/kAudioHardwarePropertyDefaultOutputDevice rv={}", r);
                return Err(Error::error());
            }
        }

        if !self.input_unit.is_null()
            && self
                .input_device
                .flags
                .contains(device_flags::DEV_SELECTED_DEFAULT)
        {
            assert!(
                self.default_input_listener.is_none(),
                "register default_input_listener without unregistering the one in use"
            );

            // Get the notification when the default intput audio changes, e.g.,
            // when the user plugs in a USB mic and the system chooses it automatically as the default,
            // or when another device is chosen in the system preference.
            self.default_input_listener = Some(device_property_listener::new(
                kAudioObjectSystemObject,
                get_property_address(
                    Property::HardwareDefaultInputDevice,
                    DeviceType::INPUT | DeviceType::OUTPUT,
                ),
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
        self.debug_assert_is_on_stream_queue();
        if self.stm_ptr.is_null() {
            assert!(
                self.output_source_listener.is_none()
                    && self.output_alive_listener.is_none()
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

        if self.output_alive_listener.is_some() {
            let rv = stm.remove_device_listener(self.output_alive_listener.as_ref().unwrap());
            if rv != NO_ERR {
                cubeb_log!("AudioObjectRemovePropertyListener/output/kAudioDevicePropertyDeviceIsAlive rv={}, device id={}", rv, self.output_device.id);
                r = Err(Error::error());
            }
            self.output_alive_listener = None;
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
        self.debug_assert_is_on_stream_queue();
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

    fn get_output_channel_layout(&self) -> Result<Vec<mixer::Channel>> {
        self.debug_assert_is_on_stream_queue();
        assert!(!self.output_unit.is_null());
        if !self.using_voice_processing_unit() {
            return get_channel_layout(self.output_unit);
        }

        // The VoiceProcessingIO unit (as tried on MacOS 14) is known to not support
        // kAudioUnitProperty_AudioChannelLayout queries, and to lie about
        // kAudioDevicePropertyPreferredChannelLayout. If we're using
        // VoiceProcessingIO, try standing up a regular AudioUnit and query that.
        cubeb_log!(
            "({:p}) get_output_channel_layout with a VoiceProcessingIO output unit. Trying a dedicated unit.",
            self.stm_ptr
        );
        let mut dedicated_unit = create_audiounit(&self.output_device)?;
        let res = get_channel_layout(dedicated_unit);
        dispose_audio_unit(dedicated_unit);
        dedicated_unit = ptr::null_mut();
        res
    }
}

impl<'ctx> Drop for CoreStreamData<'ctx> {
    fn drop(&mut self) {
        self.debug_assert_is_on_stream_queue();
        self.stop_audiounits();
        self.close();
    }
}

#[derive(Debug, Clone)]
struct OutputCallbackTimingData {
    frames_queued: u64,
    timestamp: u64,
    buffer_size: u64,
}

// The fisrt two members of the Cubeb stream must be a pointer to its Cubeb context and a void user
// defined pointer. The Cubeb interface use this assumption to operate the Cubeb APIs.
// #[repr(C)] is used to prevent any padding from being added in the beginning of the AudioUnitStream.
#[repr(C)]
#[derive(Debug)]
// Allow exposing this private struct in public interfaces when running tests.
#[cfg_attr(test, allow(private_in_public))]
struct AudioUnitStream<'ctx> {
    context: &'ctx mut AudioUnitContext,
    user_ptr: *mut c_void,
    // Task queue for the stream.
    queue: Queue,

    data_callback: ffi::cubeb_data_callback,
    state_callback: ffi::cubeb_state_callback,
    device_changed_callback: Mutex<ffi::cubeb_device_changed_callback>,
    // Frame counters
    frames_queued: u64,
    // How many frames got read from the input since the stream started (includes
    // padded silence)
    frames_read: AtomicUsize,
    // How many frames got written to the output device since the stream started
    frames_written: AtomicUsize,
    stopped: AtomicBool,
    draining: AtomicBool,
    reinit_pending: AtomicBool,
    destroy_pending: AtomicBool,
    // Latency requested by the user.
    latency_frames: u32,
    // Fixed latency, characteristic of the device.
    output_device_latency_frames: AtomicU32,
    input_device_latency_frames: AtomicU32,
    // Total latency: the latency of the device + the OS latency
    total_output_latency_frames: AtomicU32,
    total_input_latency_frames: AtomicU32,
    output_callback_timing_data_read: triple_buffer::Output<OutputCallbackTimingData>,
    output_callback_timing_data_write: triple_buffer::Input<OutputCallbackTimingData>,
    prev_position: u64,
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
        let output_callback_timing_data =
            triple_buffer::TripleBuffer::new(OutputCallbackTimingData {
                frames_queued: 0,
                timestamp: 0,
                buffer_size: 0,
            });
        let (output_callback_timing_data_write, output_callback_timing_data_read) =
            output_callback_timing_data.split();
        AudioUnitStream {
            context,
            user_ptr,
            queue: Queue::new(DISPATCH_QUEUE_LABEL),
            data_callback,
            state_callback,
            device_changed_callback: Mutex::new(None),
            frames_queued: 0,
            frames_read: AtomicUsize::new(0),
            frames_written: AtomicUsize::new(0),
            stopped: AtomicBool::new(true),
            draining: AtomicBool::new(false),
            reinit_pending: AtomicBool::new(false),
            destroy_pending: AtomicBool::new(false),
            latency_frames,
            output_device_latency_frames: AtomicU32::new(0),
            input_device_latency_frames: AtomicU32::new(0),
            total_output_latency_frames: AtomicU32::new(0),
            total_input_latency_frames: AtomicU32::new(0),
            output_callback_timing_data_write,
            output_callback_timing_data_read,
            prev_position: 0,
            switching_device: AtomicBool::new(false),
            core_stream_data: CoreStreamData::default(),
        }
    }

    fn add_device_listener(&self, listener: &device_property_listener) -> OSStatus {
        self.queue.debug_assert_is_current();
        audio_object_add_property_listener(
            listener.device,
            &listener.property,
            listener.listener,
            self as *const Self as *mut c_void,
        )
    }

    fn remove_device_listener(&self, listener: &device_property_listener) -> OSStatus {
        self.queue.debug_assert_is_current();
        audio_object_remove_property_listener(
            listener.device,
            &listener.property,
            listener.listener,
            self as *const Self as *mut c_void,
        )
    }

    fn notify_state_changed(&self, state: State) {
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
        self.queue.debug_assert_is_current();
        // Call stop_audiounits to avoid potential data race. If there is a running data callback,
        // which locks a mutex inside CoreAudio framework, then this call will block the current
        // thread until the callback is finished since this call asks to lock a mutex inside
        // CoreAudio framework that is used by the data callback.
        if !self.stopped.load(Ordering::SeqCst) {
            self.core_stream_data.stop_audiounits();
        }

        debug_assert!(
            !self.core_stream_data.input_unit.is_null()
                || !self.core_stream_data.output_unit.is_null()
        );
        let vol_rv = if self.core_stream_data.output_unit.is_null() {
            Err(Error::error())
        } else {
            get_volume(self.core_stream_data.output_unit)
        };

        self.core_stream_data.close();

        // Use the new default device if this stream was set to follow the output device.
        if self.core_stream_data.has_output()
            && self
                .core_stream_data
                .output_device
                .flags
                .contains(device_flags::DEV_SELECTED_DEFAULT)
        {
            self.core_stream_data.output_device =
                match create_device_info(kAudioObjectUnknown, DeviceType::OUTPUT) {
                    None => {
                        cubeb_log!("Fail to create device info for output");
                        return Err(Error::error());
                    }
                    Some(d) => d,
                };
        }

        // Likewise, for the input side
        if self.core_stream_data.has_input()
            && self
                .core_stream_data
                .input_device
                .flags
                .contains(device_flags::DEV_SELECTED_DEFAULT)
        {
            self.core_stream_data.input_device =
                match create_device_info(kAudioObjectUnknown, DeviceType::INPUT) {
                    None => {
                        cubeb_log!("Fail to create device info for input");
                        return Err(Error::error());
                    }
                    Some(d) => d,
                }
        }

        self.core_stream_data.setup().map_err(|e| {
            cubeb_log!("({:p}) Setup failed.", self.core_stream_data.stm_ptr);
            e
        })?;

        if let Ok(volume) = vol_rv {
            set_volume(self.core_stream_data.output_unit, volume);
        }

        // If the stream was running, start it again.
        if !self.stopped.load(Ordering::SeqCst) {
            self.core_stream_data.start_audiounits().map_err(|e| {
                cubeb_log!(
                    "({:p}) Start audiounit failed.",
                    self.core_stream_data.stm_ptr
                );
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

        let queue = self.queue.clone();
        // Use a new thread, through the queue, to avoid deadlock when calling
        // Get/SetProperties method from inside notify callback
        queue.run_async(move || {
            let stm_ptr = self as *const AudioUnitStream;
            if self.destroy_pending.load(Ordering::SeqCst) {
                cubeb_log!(
                    "({:p}) stream pending destroy, cancelling reinit task",
                    stm_ptr
                );
                return;
            }

            if self.reinit().is_err() {
                self.core_stream_data.close();
                self.notify_state_changed(State::Error);
                cubeb_log!(
                    "({:p}) Could not reopen the stream after switching.",
                    stm_ptr
                );
            }
            self.switching_device.store(false, Ordering::SeqCst);
            self.reinit_pending.store(false, Ordering::SeqCst);
        });
    }

    fn close_on_error(&mut self) {
        self.queue.debug_assert_is_current();
        let stm_ptr = self as *const AudioUnitStream;

        self.core_stream_data.close();
        self.notify_state_changed(State::Error);
        cubeb_log!("({:p}) Close the stream due to an error.", stm_ptr);

        self.switching_device.store(false, Ordering::SeqCst);
    }

    fn destroy_internal(&mut self) {
        self.queue.debug_assert_is_current();
        self.core_stream_data.close();
        assert!(self.context.active_streams() >= 1);
        self.context.update_latency_by_removing_stream();
    }

    fn destroy(&mut self) {
        self.queue.debug_assert_is_current();
        if self
            .core_stream_data
            .uninstall_system_changed_callback()
            .is_err()
        {
            cubeb_log!(
                "({:p}) Could not uninstall the system changed callback",
                self as *const AudioUnitStream
            );
        }

        if self
            .core_stream_data
            .uninstall_device_changed_callback()
            .is_err()
        {
            cubeb_log!(
                "({:p}) Could not uninstall all device change listeners",
                self as *const AudioUnitStream
            );
        }

        // Execute the stream destroy work.
        self.destroy_pending.store(true, Ordering::SeqCst);

        // Call stop_audiounits to avoid potential data race. If there is a running data callback,
        // which locks a mutex inside CoreAudio framework, then this call will block the current
        // thread until the callback is finished since this call asks to lock a mutex inside
        // CoreAudio framework that is used by the data callback.
        if !self.stopped.load(Ordering::SeqCst) {
            self.core_stream_data.stop_audiounits();
            self.stopped.store(true, Ordering::SeqCst);
        }

        self.destroy_internal();

        cubeb_log!(
            "Cubeb stream ({:p}) destroyed successful.",
            self as *const AudioUnitStream
        );
    }
}

impl<'ctx> Drop for AudioUnitStream<'ctx> {
    fn drop(&mut self) {
        // Execute destroy in serial queue to avoid collision with reinit when un/plug devices
        self.queue.clone().run_final(move || {
            self.destroy();
            self.core_stream_data = CoreStreamData::default();
        });
    }
}

impl<'ctx> StreamOps for AudioUnitStream<'ctx> {
    fn start(&mut self) -> Result<()> {
        self.stopped.store(false, Ordering::SeqCst);
        self.draining.store(false, Ordering::SeqCst);

        // Execute start in serial queue to avoid racing with destroy or reinit.
        let mut result = Err(Error::error());
        let started = &mut result;
        let stream = &self;
        self.queue.run_sync(move || {
            *started = stream.core_stream_data.start_audiounits();
        });

        result?;

        self.notify_state_changed(State::Started);

        cubeb_log!(
            "Cubeb stream ({:p}) started successfully.",
            self as *const AudioUnitStream
        );
        Ok(())
    }
    fn stop(&mut self) -> Result<()> {
        self.stopped.store(true, Ordering::SeqCst);

        // Execute stop in serial queue to avoid racing with destroy or reinit.
        let stream = &self;
        self.queue.run_sync(move || {
            stream.core_stream_data.stop_audiounits();
        });

        self.notify_state_changed(State::Stopped);

        cubeb_log!(
            "Cubeb stream ({:p}) stopped successfully.",
            self as *const AudioUnitStream
        );
        Ok(())
    }
    fn position(&mut self) -> Result<u64> {
        let OutputCallbackTimingData {
            frames_queued,
            timestamp,
            buffer_size,
        } = self.output_callback_timing_data_read.read().clone();
        let total_output_latency_frames =
            u64::from(self.total_output_latency_frames.load(Ordering::SeqCst));
        // If output latency is available, take it into account. Otherwise, use the number of
        // frames played.
        let position = if total_output_latency_frames != 0 {
            if total_output_latency_frames > frames_queued {
                0
            } else {
                // Interpolate here to match other cubeb backends. Only return an interpolated time
                // if we've played enough frames. If the stream is paused, clamp the interpolated
                // number of frames to the buffer size.
                const NS2S: u64 = 1_000_000_000;
                let now = unsafe { mach_absolute_time() };
                let diff = now - timestamp;
                let interpolated_frames = cmp::min(
                    host_time_to_ns(diff)
                        * self.core_stream_data.output_stream_params.rate() as u64
                        / NS2S,
                    buffer_size,
                );
                (frames_queued - total_output_latency_frames) + interpolated_frames
            }
        } else {
            frames_queued
        };

        // Ensure mononicity of the clock even when changing output device.
        if position > self.prev_position {
            self.prev_position = position;
        }
        Ok(self.prev_position)
    }
    #[cfg(target_os = "ios")]
    fn latency(&mut self) -> Result<u32> {
        Err(not_supported())
    }
    #[cfg(not(target_os = "ios"))]
    fn latency(&mut self) -> Result<u32> {
        Ok(self.total_output_latency_frames.load(Ordering::SeqCst))
    }
    #[cfg(target_os = "ios")]
    fn input_latency(&mut self) -> Result<u32> {
        Err(not_supported())
    }
    #[cfg(not(target_os = "ios"))]
    fn input_latency(&mut self) -> Result<u32> {
        let user_rate = self.core_stream_data.input_stream_params.rate();
        let hw_rate = self.core_stream_data.input_dev_desc.mSampleRate as u32;
        let frames = self.total_input_latency_frames.load(Ordering::SeqCst);
        if frames != 0 {
            if hw_rate == user_rate {
                Ok(frames)
            } else {
                Ok((frames * user_rate) / hw_rate)
            }
        } else {
            Err(Error::error())
        }
    }
    fn set_volume(&mut self, volume: f32) -> Result<()> {
        // Execute set_volume in serial queue to avoid racing with destroy or reinit.
        let mut result = Err(Error::error());
        let set = &mut result;
        let stream = &self;
        self.queue.run_sync(move || {
            *set = set_volume(stream.core_stream_data.output_unit, volume);
        });

        result?;

        cubeb_log!(
            "Cubeb stream ({:p}) set volume to {}.",
            self as *const AudioUnitStream,
            volume
        );
        Ok(())
    }
    fn set_name(&mut self, _: &CStr) -> Result<()> {
        Err(Error::not_supported())
    }
    fn current_device(&mut self) -> Result<&DeviceRef> {
        Err(Error::not_supported())
    }
    fn set_input_mute(&mut self, _mute: bool) -> Result<()> {
        Err(Error::not_supported())
    }
    fn set_input_processing_params(&mut self, _params: InputProcessingParams) -> Result<()> {
        Err(Error::not_supported())
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

#[allow(clippy::non_send_fields_in_send_ty)]
unsafe impl<'ctx> Send for AudioUnitStream<'ctx> {}
unsafe impl<'ctx> Sync for AudioUnitStream<'ctx> {}

#[cfg(test)]
mod tests;
