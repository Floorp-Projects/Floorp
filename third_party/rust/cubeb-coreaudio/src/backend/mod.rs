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
use cubeb_backend::{
    ffi, Context, ContextOps, DeviceCollectionRef, DeviceId, DeviceRef, DeviceType, Error, Ops,
    Result, SampleFormat, State, Stream, StreamOps, StreamParams, StreamParamsRef, StreamPrefs,
};
use mach::mach_time::{mach_absolute_time, mach_timebase_info};
use std::cmp;
use std::ffi::{CStr, CString};
use std::mem;
use std::os::raw::c_void;
use std::ptr;
use std::slice;
use std::sync::atomic::{AtomicBool, AtomicU32, AtomicU64, AtomicUsize, Ordering};
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

impl Into<mixer::Channel> for CAChannelLabel {
    fn into(self) -> mixer::Channel {
        use self::coreaudio_sys_utils::sys;
        match self.0 {
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

fn clamp_latency(latency_frames: u32) -> u32 {
    cmp::max(
        cmp::min(latency_frames, SAFE_MAX_LATENCY_FRAMES),
        SAFE_MIN_LATENCY_FRAMES,
    )
}

fn create_device_info(id: AudioDeviceID, devtype: DeviceType) -> Result<device_info> {
    assert_ne!(id, kAudioObjectSystemObject);

    let mut info = device_info {
        id,
        flags: match devtype {
            DeviceType::INPUT => device_flags::DEV_INPUT,
            DeviceType::OUTPUT => device_flags::DEV_OUTPUT,
            _ => panic!("Only accept input or output type"),
        },
    };

    let default_device_id = audiounit_get_default_device_id(devtype);
    if default_device_id == kAudioObjectUnknown {
        cubeb_log!("Could not find default audio device for {:?}", devtype);
        return Err(Error::error());
    }

    if id == kAudioObjectUnknown {
        info.id = default_device_id;
        cubeb_log!("Creating a default device info.");
        info.flags |= device_flags::DEV_SELECTED_DEFAULT;
    }

    if info.id == default_device_id {
        cubeb_log!("Requesting default system device.");
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

    let input_latency_frames = compute_input_latency(&stm, unsafe { (*tstamp).mHostTime });
    stm.total_input_latency_frames
        .store(input_latency_frames, Ordering::SeqCst);

    if stm.shutdown.load(Ordering::SeqCst) {
        cubeb_log!("({:p}) input shutdown", stm as *const AudioUnitStream);
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
            stm.core_stream_data.input_desc.mBytesPerFrame * input_frames;
        input_buffer_list.mBuffers[0].mData = ptr::null_mut();
        input_buffer_list.mBuffers[0].mNumberChannels =
            stm.core_stream_data.input_desc.mChannelsPerFrame;
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
            cubeb_logv!(
                "({:p}) input: reinit pending, output will pull silence instead",
                stm.core_stream_data.stm_ptr
            );
            ErrorHandle::Reinit
        } else {
            assert_eq!(status, NO_ERR);
            // Copy input data in linear buffer.
            let elements =
                (input_frames * stm.core_stream_data.input_desc.mChannelsPerFrame) as usize;
            input_buffer_manager.push_data(input_buffer_list.mBuffers[0].mData, elements);
            ErrorHandle::Return(status)
        };

        // Advance input frame counter.
        stm.frames_read
            .fetch_add(input_frames as usize, atomic::Ordering::SeqCst);

        cubeb_logv!(
            "({:p}) input: buffers {}, size {}, channels {}, rendered frames {}, total frames {}.",
            stm.core_stream_data.stm_ptr,
            input_buffer_list.mNumberBuffers,
            input_buffer_list.mBuffers[0].mDataByteSize,
            input_buffer_list.mBuffers[0].mNumberChannels,
            input_frames,
            input_buffer_manager.available_samples()
                / stm.core_stream_data.input_desc.mChannelsPerFrame as usize
        );

        // Full Duplex. We'll call data_callback in the AudioUnit output callback.
        if !stm.core_stream_data.output_unit.is_null() {
            return handle;
        }

        // Input only. Call the user callback through resampler.
        // Resampler will deliver input buffer in the correct rate.
        let mut total_input_frames = (input_buffer_manager.available_samples()
            / stm.core_stream_data.input_desc.mChannelsPerFrame as usize)
            as i64;
        assert!(input_frames as i64 <= total_input_frames);
        let input_buffer = input_buffer_manager.get_linear_data(total_input_frames as usize);
        let outframes = stm.core_stream_data.resampler.fill(
            input_buffer,
            &mut total_input_frames,
            ptr::null_mut(),
            0,
        );
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
    // cancel this callback.
    if stm.draining.load(Ordering::SeqCst) {
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

fn compute_output_latency(stm: &AudioUnitStream, host_time: u64) -> u32 {
    let now = host_time_to_ns(unsafe { mach_absolute_time() });
    let audio_output_time = host_time_to_ns(host_time);
    let output_latency_ns = if audio_output_time < now {
        0
    } else {
        audio_output_time - now
    };

    const NS2S: u64 = 1_000_000_000;
    // The total output latency is the timestamp difference + the stream latency +
    // the hardware latency.
    let out_hw_rate = stm.core_stream_data.output_hw_rate as u64;
    (output_latency_ns * out_hw_rate / NS2S
        + stm.current_output_latency_frames.load(Ordering::SeqCst) as u64) as u32
}

fn compute_input_latency(stm: &AudioUnitStream, host_time: u64) -> u32 {
    let now = host_time_to_ns(unsafe { mach_absolute_time() });
    let audio_input_time = host_time_to_ns(host_time);
    let input_latency_ns = if audio_input_time > now {
        0
    } else {
        now - audio_input_time
    };

    const NS2S: u64 = 1_000_000_000;
    // The total input latency is the timestamp difference + the stream latency +
    // the hardware latency.
    let input_hw_rate = stm.core_stream_data.input_hw_rate as u64;
    (input_latency_ns * input_hw_rate / NS2S
        + stm.current_input_latency_frames.load(Ordering::SeqCst) as u64) as u32
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
        // Cancel the output callback only. For duplex stream,
        // the input callback will be cancelled in its own callback.
        let r = stop_audiounit(stm.core_stream_data.output_unit);
        assert!(r.is_ok());
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

        let prev_frames_written = stm.frames_written.load(Ordering::SeqCst);

        stm.frames_written
            .fetch_add(output_frames as usize, Ordering::SeqCst);

        // Also get the input buffer if the stream is duplex
        let (input_buffer, mut input_frames) = if !stm.core_stream_data.input_unit.is_null() {
            let input_buffer_manager = stm.core_stream_data.input_buffer_manager.as_mut().unwrap();
            assert_ne!(stm.core_stream_data.input_desc.mChannelsPerFrame, 0);
            let input_channels = stm.core_stream_data.input_desc.mChannelsPerFrame as usize;
            // If the output callback came first and this is a duplex stream, we need to
            // fill in some additional silence in the resampler.
            // Otherwise, if we had more than expected callbacks in a row, or we're
            // currently switching, we add some silence as well to compensate for the
            // fact that we're lacking some input data.
            let input_frames_needed = minimum_resampling_input_frames(
                stm.core_stream_data.input_hw_rate,
                f64::from(stm.core_stream_data.output_stream_params.rate()),
                output_frames as usize,
            );
            let buffered_input_frames = input_buffer_manager.available_samples() / input_channels;
            // Else if the input has buffered a lot already because the output started late, we
            // need to trim the input buffer
            if prev_frames_written == 0 && buffered_input_frames > input_frames_needed as usize {
                input_buffer_manager.trim(input_frames_needed * input_channels);
                let popped_samples =
                    ((buffered_input_frames - input_frames_needed) * input_channels) as usize;
                stm.frames_read.fetch_sub(popped_samples, Ordering::SeqCst);

                cubeb_log!("Dropping {} frames in input buffer.", popped_samples);
            }

            if input_frames_needed > buffered_input_frames
                && (stm.switching_device.load(Ordering::SeqCst)
                    || stm.frames_read.load(Ordering::SeqCst) == 0)
            {
                // The silent frames will be inserted in `get_linear_data` below.
                let silent_frames_to_push = input_frames_needed - buffered_input_frames;
                stm.frames_read
                    .fetch_add(input_frames_needed, Ordering::SeqCst);
                cubeb_log!(
                    "({:p}) Missing Frames: {} will append {} frames of input silence.",
                    stm.core_stream_data.stm_ptr,
                    if stm.frames_read.load(Ordering::SeqCst) == 0 {
                        "input hasn't started,"
                    } else {
                        assert!(stm.switching_device.load(Ordering::SeqCst));
                        "device switching,"
                    },
                    silent_frames_to_push
                );
            }

            let input_samples_needed = buffered_input_frames * input_channels;
            (
                input_buffer_manager.get_linear_data(input_samples_needed),
                buffered_input_frames as i64,
            )
        } else {
            (ptr::null_mut::<c_void>(), 0)
        };

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
            stm.shutdown.store(true, Ordering::SeqCst);
            stm.core_stream_data.stop_audiounits();
            audiounit_make_silent(&mut buffers[0]);
            return (NO_ERR, Some(State::Error));
        }

        stm.draining
            .store(outframes < i64::from(output_frames), Ordering::SeqCst);
        stm.frames_played
            .store(stm.frames_queued, atomic::Ordering::SeqCst);
        stm.frames_queued += outframes as u64;

        // Post process output samples.
        if stm.draining.load(Ordering::SeqCst) {
            // Clear missing frames (silence)
            let frames_to_bytes = |frames: usize| -> usize {
                let sample_size =
                    cubeb_sample_size(stm.core_stream_data.output_stream_params.format());
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

#[allow(clippy::cognitive_complexity)]
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
    stm.switching_device.store(true, Ordering::SeqCst);

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
                    stm.switching_device.store(false, Ordering::SeqCst);
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
                stm.switching_device.store(false, Ordering::SeqCst);
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

fn audiounit_get_default_device_id(devtype: DeviceType) -> AudioObjectID {
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
    if audio_object_get_property_data(kAudioObjectSystemObject, &address, &mut size, &mut devid)
        != NO_ERR
    {
        return kAudioObjectUnknown;
    }

    devid
}

fn audiounit_convert_channel_layout(layout: &AudioChannelLayout) -> Vec<mixer::Channel> {
    if layout.mChannelLayoutTag != kAudioChannelLayoutTag_UseChannelDescriptions {
        // kAudioChannelLayoutTag_UseChannelBitmap
        // kAudioChannelLayoutTag_Mono
        // kAudioChannelLayoutTag_Stereo
        // ....
        cubeb_log!("Only handle UseChannelDescriptions for now.\n");
        return Vec::new();
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

    channels
}

fn audiounit_get_preferred_channel_layout(output_unit: AudioUnit) -> Vec<mixer::Channel> {
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
        return Vec::new();
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
        return Vec::new();
    }

    audiounit_convert_channel_layout(layout.as_ref())
}

// This is for output AudioUnit only. Calling this by input-only AudioUnit is prone
// to crash intermittently.
fn audiounit_get_current_channel_layout(output_unit: AudioUnit) -> Vec<mixer::Channel> {
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
        // This property isn't known before macOS 10.12, attempt another method.
        return audiounit_get_preferred_channel_layout(output_unit);
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
        return Vec::new();
    }

    audiounit_convert_channel_layout(layout.as_ref())
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
        enable_audiounit_scope(unit, DeviceType::INPUT, true).map_err(|e| {
            cubeb_log!("Fail to enable audiounit input scope. Error: {}", e);
            Error::error()
        })?;
        enable_audiounit_scope(unit, DeviceType::OUTPUT, false).map_err(|e| {
            cubeb_log!("Fail to disable audiounit output scope. Error: {}", e);
            Error::error()
        })?;
    }

    if device.flags.contains(device_flags::DEV_OUTPUT) {
        // Output only.
        enable_audiounit_scope(unit, DeviceType::OUTPUT, true).map_err(|e| {
            cubeb_log!("Fail to enable audiounit output scope. Error: {}", e);
            Error::error()
        })?;
        enable_audiounit_scope(unit, DeviceType::INPUT, false).map_err(|e| {
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
    devtype: DeviceType,
    enable_io: bool,
) -> std::result::Result<(), OSStatus> {
    assert!(!unit.is_null());

    let enable: u32 = if enable_io { 1 } else { 0 };
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
        assert_ne!(frames, 0);
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
            "Fail to set buffer size for AudioUnit {:?} for {:?}. Error: {}",
            unit,
            devtype,
            e
        );
        Error::error()
    })?;

    let &(ref lock, ref cvar) = &*pair;
    let changed = lock.lock().unwrap();
    if !*changed {
        let (chg, timeout_res) = cvar.wait_timeout(changed, waiting_time).unwrap();
        if timeout_res.timed_out() {
            cubeb_log!(
                "Time out for waiting the buffer frame size setting of AudioUnit {:?} for {:?}",
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

fn audiounit_get_default_datasource_string(devtype: DeviceType) -> Result<CString> {
    let id = audiounit_get_default_device_id(devtype);
    if id == kAudioObjectUnknown {
        return Err(Error::error());
    }
    let data = get_device_source(id, devtype).unwrap_or(0);
    Ok(convert_uint32_into_string(data))
}

fn is_device_a_type_of(devid: AudioObjectID, devtype: DeviceType) -> bool {
    assert_ne!(devid, kAudioObjectUnknown);
    get_channel_count(devid, devtype).unwrap_or(0) > 0
}

fn get_channel_count(devid: AudioObjectID, devtype: DeviceType) -> Result<u32> {
    assert_ne!(devid, kAudioObjectUnknown);

    let buffers = get_device_stream_configuration(devid, devtype).map_err(|e| {
        cubeb_log!("Cannot get the stream configuration. Error: {}", e);
        Error::error()
    })?;

    let mut count = 0;
    for buffer in buffers {
        count += buffer.mNumberChannels;
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
                "No any stream on device {} in {:?} scope!",
                devid,
                devtype
            );
            Ok(0) // default stream latency
        } else {
            get_stream_latency(streams[0], devtype)
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

fn get_device_group_id(
    id: AudioDeviceID,
    devtype: DeviceType,
) -> std::result::Result<CString, OSStatus> {
    const BLTN: u32 = 0x626C_746E; // "bltn" (builtin)

    match get_device_transport_type(id, devtype) {
        Ok(BLTN) => {
            cubeb_log!(
                "The transport type is {:?}",
                convert_uint32_into_string(BLTN)
            );
            match get_custom_group_id(id, devtype) {
                Some(id) => return Ok(id),
                None => {
                    cubeb_log!("Get model uid instead.");
                }
            };
        }
        Ok(trans_type) => {
            cubeb_log!(
                "The transport type is {:?}. Get model uid instead.",
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
                "Use hardcode group id: {} when source is: {:?}.",
                GROUP_ID,
                convert_uint32_into_string(s.unwrap())
            );
            return Some(CString::new(GROUP_ID).unwrap());
        }
        s @ Ok(EMIC) | s @ Ok(HDPN) => {
            const GROUP_ID: &str = "builtin-external-mic|hdpn";
            cubeb_log!(
                "Use hardcode group id: {} when source is: {:?}.",
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

    match get_device_group_id(devid, devtype) {
        Ok(group_id) => {
            dev_info.group_id = group_id.into_raw();
        }
        Err(e) => {
            cubeb_log!(
                "Cannot get the model uid for device {} in {:?} scope. Error: {}",
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
        let label = match get_device_label(device, DeviceType::OUTPUT | DeviceType::INPUT) {
            Ok(label) => label.into_string(),
            Err(e) => format!("Unknown(error: {})", e),
        };
        let info = format!("{} ({})", device, label);

        if let Ok(channels) = get_channel_count(device, devtype) {
            cubeb_log!("device {} has {} {:?}-channels", info, channels, devtype);
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
    let mutexed_context = Arc::new(Mutex::new(context));
    let also_mutexed_context = Arc::clone(&mutexed_context);

    // This can be called from inside an AudioUnit function, dispatch to another queue.
    queue.run_async(move || {
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
        self.changed_callback == None && self.callback_user_ptr.is_null() && self.devices.is_empty()
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
        let device = audiounit_get_default_device_id(DeviceType::OUTPUT);
        if device == kAudioObjectUnknown {
            return Err(Error::error());
        }

        let format = get_device_stream_format(device, DeviceType::OUTPUT).map_err(|e| {
            cubeb_log!(
                "Cannot get the stream format of the default output device. Error: {}",
                e
            );
            Error::error()
        })?;
        Ok(format.mChannelsPerFrame)
    }
    #[cfg(target_os = "ios")]
    fn min_latency(&mut self, _params: StreamParams) -> Result<u32> {
        Err(not_supported());
    }
    #[cfg(not(target_os = "ios"))]
    fn min_latency(&mut self, _params: StreamParams) -> Result<u32> {
        let device = audiounit_get_default_device_id(DeviceType::OUTPUT);
        if device == kAudioObjectUnknown {
            cubeb_log!("Could not get default output device id.");
            return Err(Error::error());
        }

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
        let device = audiounit_get_default_device_id(DeviceType::OUTPUT);
        if device == kAudioObjectUnknown {
            return Err(Error::error());
        }
        let rate = get_device_sample_rate(device, DeviceType::OUTPUT).map_err(|e| {
            cubeb_log!(
                "Cannot get the sample rate of the default output device. Error: {}",
                e
            );
            Error::error()
        })?;
        Ok(rate as u32)
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
            let stm_params = StreamParams::from(unsafe { *params.as_ptr() });
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

        // Make sure all the pending (device-collection-changed-callback) tasks
        // in queue are done, and cancel all the tasks appended after `drop` is executed.
        let queue = self.serial_queue.clone();
        queue.run_final(|| {
            // Unregister the callback if necessary.
            self.remove_devices_changed_listener(DeviceType::INPUT);
            self.remove_devices_changed_listener(DeviceType::OUTPUT);
        });
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
    device_layout: Vec<mixer::Channel>,
    input_buffer_manager: Option<BufferManager>,
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
            device_layout: Vec::new(),
            input_buffer_manager: None,
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
            device_layout: Vec::new(),
            input_buffer_manager: None,
            default_input_listener: None,
            default_output_listener: None,
            input_alive_listener: None,
            input_source_listener: None,
            output_source_listener: None,
        }
    }

    fn start_audiounits(&self) -> Result<()> {
        // Only allowed to be called after the stream is initialized
        // and before the stream is destroyed.
        debug_assert!(!self.input_unit.is_null() || !self.output_unit.is_null());

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
            let r = stop_audiounit(self.input_unit);
            assert!(r.is_ok());
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

    fn should_use_aggregate_device(&self) -> bool {
        // Only using aggregate device when the input is a mic-only device and the output is a
        // speaker-only device. Otherwise, the mic on the output device may become the main
        // microphone of the aggregate device for this duplex stream.
        self.has_input()
            && self.has_output()
            && self.input_device.id != kAudioObjectUnknown
            && self.input_device.flags.contains(device_flags::DEV_INPUT)
            && self.output_device.id != kAudioObjectUnknown
            && self.output_device.flags.contains(device_flags::DEV_OUTPUT)
            && self.input_device.id != self.output_device.id
            && !is_device_a_type_of(self.input_device.id, DeviceType::OUTPUT)
            && !is_device_a_type_of(self.output_device.id, DeviceType::INPUT)
    }

    #[allow(clippy::cognitive_complexity)] // TODO: Refactoring.
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

        if self.should_use_aggregate_device() {
            match AggregateDevice::new(in_dev_info.id, out_dev_info.id) {
                Ok(device) => {
                    in_dev_info.id = device.get_device_id();
                    out_dev_info.id = device.get_device_id();
                    in_dev_info.flags = device_flags::DEV_INPUT;
                    out_dev_info.flags = device_flags::DEV_OUTPUT;
                    self.aggregate_device = device;
                    cubeb_log!(
                        "({:p}) Use aggregate device {} for input and output.",
                        self.stm_ptr,
                        self.aggregate_device.get_device_id()
                    );
                }
                Err(status) => {
                    cubeb_log!(
                        "({:p}) Create aggregate devices failed. Error: {}.\
                         Use assigned devices directly instead.",
                        self.stm_ptr,
                        status
                    );
                }
            }
        }

        assert!(!self.stm_ptr.is_null());
        let stream = unsafe { &(*self.stm_ptr) };

        // Configure I/O stream
        if self.has_input() {
            cubeb_log!(
                "({:p}) Initialize input by device info: {:?}",
                self.stm_ptr,
                in_dev_info
            );

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
                set_buffer_size_sync(self.input_unit, DeviceType::INPUT, stream.latency_frames)
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

            self.input_buffer_manager = Some(BufferManager::new(self.input_stream_params.format()));

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
            cubeb_log!(
                "({:p}) Initialize output by device info: {:?}",
                self.stm_ptr,
                out_dev_info
            );

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
            if hw_channels == 0 {
                cubeb_log!(
                    "({:p}) Output hardware has no output channel! Bail out.",
                    self.stm_ptr
                );
                return Err(Error::device_unavailable());
            }

            self.device_layout = audiounit_get_current_channel_layout(self.output_unit);

            self.mixer = if hw_channels != self.output_stream_params.channels()
                || self.device_layout
                    != mixer::get_channel_order(self.output_stream_params.layout())
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
                    self.output_stream_params.channels() as usize,
                    self.output_stream_params.layout(),
                    hw_channels as usize,
                    self.device_layout.clone(),
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
            let mut params = unsafe { *(self.input_stream_params.as_ptr()) };
            params.rate = self.input_hw_rate as u32;
            Some(params)
        } else {
            None
        };
        let resampler_output_params = if self.has_output() {
            let params = unsafe { *(self.output_stream_params.as_ptr()) };
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

            stream.current_input_latency_frames.store(
                get_fixed_latency(self.input_device.id, DeviceType::INPUT),
                Ordering::SeqCst,
            );
        }

        if !self.output_unit.is_null() {
            let r = audio_unit_initialize(self.output_unit);
            if r != NO_ERR {
                cubeb_log!("AudioUnitInitialize/output rv={}", r);
                return Err(Error::error());
            }

            stream.current_output_latency_frames.store(
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
                stream.current_output_latency_frames.fetch_add(
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
                get_property_address(Property::DeviceSource, DeviceType::OUTPUT),
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
                get_property_address(Property::DeviceSource, DeviceType::INPUT),
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

        if !self.input_unit.is_null() {
            // This event will notify us when the default input device changes.
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
    frames_played: AtomicU64,
    frames_queued: u64,
    // How many frames got read from the input since the stream started (includes
    // padded silence)
    frames_read: AtomicUsize,
    // How many frames got written to the output device since the stream started
    frames_written: AtomicUsize,
    shutdown: AtomicBool,
    draining: AtomicBool,
    reinit_pending: AtomicBool,
    destroy_pending: AtomicBool,
    // Latency requested by the user.
    latency_frames: u32,
    current_output_latency_frames: AtomicU32,
    current_input_latency_frames: AtomicU32,
    total_output_latency_frames: AtomicU32,
    total_input_latency_frames: AtomicU32,
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
            queue: Queue::new(DISPATCH_QUEUE_LABEL),
            data_callback,
            state_callback,
            device_changed_callback: Mutex::new(None),
            frames_played: AtomicU64::new(0),
            frames_queued: 0,
            frames_read: AtomicUsize::new(0),
            frames_written: AtomicUsize::new(0),
            shutdown: AtomicBool::new(true),
            draining: AtomicBool::new(false),
            reinit_pending: AtomicBool::new(false),
            destroy_pending: AtomicBool::new(false),
            latency_frames,
            current_output_latency_frames: AtomicU32::new(0),
            current_input_latency_frames: AtomicU32::new(0),
            total_output_latency_frames: AtomicU32::new(0),
            total_input_latency_frames: AtomicU32::new(0),
            switching_device: AtomicBool::new(false),
            core_stream_data: CoreStreamData::default(),
        }
    }

    fn add_device_listener(&self, listener: &device_property_listener) -> OSStatus {
        audio_object_add_property_listener(
            listener.device,
            &listener.property,
            listener.listener,
            self as *const Self as *mut c_void,
        )
    }

    fn remove_device_listener(&self, listener: &device_property_listener) -> OSStatus {
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
        // Call stop_audiounits to avoid potential data race. If there is a running data callback,
        // which locks a mutex inside CoreAudio framework, then this call will block the current
        // thread until the callback is finished since this call asks to lock a mutex inside
        // CoreAudio framework that is used by the data callback.
        if !self.shutdown.load(Ordering::SeqCst) {
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
                    e
                })?;
                self.core_stream_data.setup().map_err(|e| {
                    cubeb_log!(
                        "({:p}) Second stream reinit failed.",
                        self.core_stream_data.stm_ptr
                    );
                    e
                })?;
            }
        }

        if let Ok(volume) = vol_rv {
            set_volume(self.core_stream_data.output_unit, volume);
        }

        // If the stream was running, start it again.
        if !self.shutdown.load(Ordering::SeqCst) {
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
        let mutexed_stm = Arc::new(Mutex::new(self));
        let also_mutexed_stm = Arc::clone(&mutexed_stm);
        // Use a new thread, through the queue, to avoid deadlock when calling
        // Get/SetProperties method from inside notify callback
        queue.run_async(move || {
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
                stm_guard.core_stream_data.close();
                stm_guard.notify_state_changed(State::Error);
                cubeb_log!(
                    "({:p}) Could not reopen the stream after switching.",
                    stm_ptr
                );
            }
            stm_guard.switching_device.store(false, Ordering::SeqCst);
            stm_guard.reinit_pending.store(false, Ordering::SeqCst);
        });
    }

    fn destroy_internal(&mut self) {
        self.core_stream_data.close();
        assert!(self.context.active_streams() >= 1);
        self.context.update_latency_by_removing_stream();
    }

    fn destroy(&mut self) {
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

        let queue = self.queue.clone();

        let stream_ptr = self as *const AudioUnitStream;
        // Execute close in serial queue to avoid collision
        // with reinit when un/plug devices
        queue.run_final(move || {
            // Call stop_audiounits to avoid potential data race. If there is a running data callback,
            // which locks a mutex inside CoreAudio framework, then this call will block the current
            // thread until the callback is finished since this call asks to lock a mutex inside
            // CoreAudio framework that is used by the data callback.
            if !self.shutdown.load(Ordering::SeqCst) {
                self.core_stream_data.stop_audiounits();
                self.shutdown.store(true, Ordering::SeqCst);
            }

            self.destroy_internal();
        });

        cubeb_log!("Cubeb stream ({:p}) destroyed successful.", stream_ptr);
    }
}

impl<'ctx> Drop for AudioUnitStream<'ctx> {
    fn drop(&mut self) {
        self.destroy();
    }
}

impl<'ctx> StreamOps for AudioUnitStream<'ctx> {
    fn start(&mut self) -> Result<()> {
        self.shutdown.store(false, Ordering::SeqCst);
        self.draining.store(false, Ordering::SeqCst);

        // Execute start in serial queue to avoid racing with destroy or reinit.
        let mut result = Err(Error::error());
        let started = &mut result;
        let stream = &self;
        self.queue.run_sync(move || {
            *started = stream.core_stream_data.start_audiounits();
        });

        if result.is_err() {
            return result;
        }

        self.notify_state_changed(State::Started);

        cubeb_log!(
            "Cubeb stream ({:p}) started successfully.",
            self as *const AudioUnitStream
        );
        Ok(())
    }
    fn stop(&mut self) -> Result<()> {
        self.shutdown.store(true, Ordering::SeqCst);

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
    fn reset_default_device(&mut self) -> Result<()> {
        Err(Error::not_supported())
    }
    fn position(&mut self) -> Result<u64> {
        let current_output_latency_frames =
            u64::from(self.current_output_latency_frames.load(Ordering::SeqCst));
        let frames_played = self.frames_played.load(Ordering::SeqCst);
        let position = if current_output_latency_frames > frames_played {
            0
        } else {
            frames_played - current_output_latency_frames
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
    #[cfg(target_os = "ios")]
    fn input_latency(&mut self) -> Result<u32> {
        Err(not_supported())
    }
    #[cfg(not(target_os = "ios"))]
    fn input_latency(&mut self) -> Result<u32> {
        let user_rate = self.core_stream_data.input_stream_params.rate();
        let hw_rate = self.core_stream_data.input_hw_rate as u32;
        let frames = self.total_input_latency_frames.load(Ordering::SeqCst);
        if hw_rate == user_rate {
            Ok(frames)
        } else {
            Ok(frames * (user_rate / hw_rate))
        }
    }
    fn set_volume(&mut self, volume: f32) -> Result<()> {
        set_volume(self.core_stream_data.output_unit, volume)
    }
    #[cfg(target_os = "ios")]
    fn current_device(&mut self) -> Result<&DeviceRef> {
        Err(not_supported())
    }
    #[cfg(not(target_os = "ios"))]
    fn current_device(&mut self) -> Result<&DeviceRef> {
        let input_name = audiounit_get_default_datasource_string(DeviceType::INPUT);
        let output_name = audiounit_get_default_datasource_string(DeviceType::OUTPUT);
        if input_name.is_err() && output_name.is_err() {
            return Err(Error::error());
        }

        let mut device: Box<ffi::cubeb_device> = Box::new(ffi::cubeb_device::default());

        let input_name = input_name.unwrap_or_default();
        device.input_name = input_name.into_raw();

        let output_name = output_name.unwrap_or_default();
        device.output_name = output_name.into_raw();

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

#[cfg(test)]
mod tests;
