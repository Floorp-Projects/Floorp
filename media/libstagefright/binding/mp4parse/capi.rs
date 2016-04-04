//! C API for mp4parse module.
//!
//! Parses ISO Base Media Format aka video/mp4 streams.
//!
//! # Examples
//!
//! ```rust
//! extern crate mp4parse;
//!
//! // Minimal valid mp4 containing no tracks.
//! let data = b"\0\0\0\x0cftypmp42";
//!
//! let context = mp4parse::mp4parse_new();
//! unsafe {
//!     let rv = mp4parse::mp4parse_read(context, data.as_ptr(), data.len());
//!     assert_eq!(0, rv);
//!     mp4parse::mp4parse_free(context);
//! }
//! ```

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std;
use std::io::Cursor;

// Symbols we need from our rust api.
use MediaContext;
use TrackType;
use read_mp4;
use Error;
use media_time_to_ms;
use track_time_to_ms;
use SampleEntry;

// These constants *must* match those in include/mp4parse.h.

/// Map Error to int32 return codes.
const MP4PARSE_OK: i32 = 0;
const MP4PARSE_ERROR_BADARG: i32 = 1;
const MP4PARSE_ERROR_INVALID: i32 = 2;
const MP4PARSE_ERROR_UNSUPPORTED: i32 = 3;
const MP4PARSE_ERROR_EOF: i32 = 4;
const MP4PARSE_ASSERT: i32 = 5;
const MP4PARSE_ERROR_IO: i32 = 6;

/// Map TrackType to uint32 constants.
const TRACK_TYPE_H264: u32 = 0;
const TRACK_TYPE_AAC: u32 = 1;

/// Map Track mime_type to uint32 constants.
const TRACK_CODEC_UNKNOWN: u32 = 0;
const TRACK_CODEC_AAC: u32 = 1;
const TRACK_CODEC_OPUS: u32 = 2;
const TRACK_CODEC_H264: u32 = 3;
const TRACK_CODEC_VP9: u32 = 4;

// These structs *must* match those declared in include/mp4parse.h.

#[repr(C)]
pub struct TrackInfo {
    track_type: u32,
    track_id: u32,
    codec: u32,
    duration: u64,
    media_time: i64, // wants to be u64? understand how elst adjustment works
    // TODO(kinetik): include crypto guff
}

#[repr(C)]
pub struct TrackAudioInfo {
    channels: u16,
    bit_depth: u16,
    sample_rate: u32,
    // TODO(kinetik):
    // int32_t profile;
    // int32_t extended_profile; // check types
    // extra_data
    // codec_specific_config
}

#[repr(C)]
pub struct TrackVideoInfo {
    display_width: u32,
    display_height: u32,
    image_width: u16,
    image_height: u16,
    // TODO(kinetik):
    // extra_data
    // codec_specific_config
}

// C API wrapper functions.

/// Allocate an opaque rust-side parser context.
#[no_mangle]
pub extern "C" fn mp4parse_new() -> *mut MediaContext {
    let context = Box::new(MediaContext::new());
    Box::into_raw(context)
}

/// Free a rust-side parser context.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_free(context: *mut MediaContext) {
    assert!(!context.is_null());
    let _ = Box::from_raw(context);
}

/// Feed a buffer through `read_mp4()` with the given rust-side
/// parser context, returning success or an error code.
///
/// This is safe to call with NULL arguments but will crash
/// if given invalid pointers, as is usual for C.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_read(context: *mut MediaContext, buffer: *const u8, size: usize) -> i32 {
    // Validate arguments from C.
    if context.is_null() || buffer.is_null() || size < 8 {
        return MP4PARSE_ERROR_BADARG;
    }

    let mut context: &mut MediaContext = &mut *context;

    // Wrap the buffer we've been give in a slice.
    let b = std::slice::from_raw_parts(buffer, size);
    let mut c = Cursor::new(b);

    // Parse in a subthread to catch any panics.
    // We must use the thread::Builder API to avoid spawn itself
    // panicking if thread creation fails. See bug 1266309.
    let task = match std::thread::Builder::new()
        .name("mp4parse_read isolation".to_string())
        .spawn(move || read_mp4(&mut c, &mut context)) {
            Ok(task) => task,
            Err(_) => return MP4PARSE_ASSERT,
    };
    // The task's JoinHandle will return an error result if the
    // thread panicked, and will wrap the closure's return'd
    // result in an Ok(..) otherwise, meaning we could see
    // Ok(Err(Error::..)) here. So map thread failures back
    // to an mp4parse::Error before converting to a C return value.
    match task.join().unwrap_or(Err(Error::AssertCaught)) {
        Ok(_) => MP4PARSE_OK,
        Err(Error::InvalidData) => MP4PARSE_ERROR_INVALID,
        Err(Error::Unsupported) => MP4PARSE_ERROR_UNSUPPORTED,
        Err(Error::UnexpectedEOF) => MP4PARSE_ERROR_EOF,
        Err(Error::AssertCaught) => MP4PARSE_ASSERT,
        Err(Error::Io(_)) => MP4PARSE_ERROR_IO,
    }
}

/// Return the number of tracks parsed by previous `read_mp4()` calls.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_get_track_count(context: *const MediaContext) -> u32 {
    // Validate argument from C.
    assert!(!context.is_null());
    let context = &*context;

    // Make sure the track count fits in a u32.
    assert!(context.tracks.len() < u32::max_value() as usize);
    context.tracks.len() as u32
}

#[no_mangle]
pub unsafe extern "C" fn mp4parse_get_track_info(context: *mut MediaContext, track: u32, info: *mut TrackInfo) -> i32 {
    if context.is_null() || info.is_null() {
        return MP4PARSE_ERROR_BADARG;
    }

    let context: &mut MediaContext = &mut *context;
    let track_index: usize = track as usize;
    let info: &mut TrackInfo = &mut *info;

    if track_index >= context.tracks.len() {
        return MP4PARSE_ERROR_BADARG;
    }

    info.track_type = match context.tracks[track_index].track_type {
        TrackType::Video => TRACK_TYPE_H264,
        TrackType::Audio => TRACK_TYPE_AAC,
        TrackType::Unknown => return MP4PARSE_ERROR_UNSUPPORTED,
    };

    info.codec = match &*context.tracks[track_index].mime_type {
        "audio/opus" => TRACK_CODEC_OPUS,
        "video/vp9" => TRACK_CODEC_VP9,
        "video/h264" => TRACK_CODEC_H264,
        "audio/aac" => TRACK_CODEC_AAC,
        _ => TRACK_CODEC_UNKNOWN,
    };

    // Maybe context & track should just have a single simple is_valid() instead?
    if context.timescale.is_none() ||
       context.tracks[track_index].timescale.is_none() ||
       context.tracks[track_index].duration.is_none() ||
       context.tracks[track_index].track_id.is_none() {
        return MP4PARSE_ERROR_INVALID;
    }

    let track = &context.tracks[track_index];
    let empty_duration = if track.empty_duration.is_some() {
        media_time_to_ms(track.empty_duration.unwrap(), context.timescale.unwrap())
    } else {
        0
    };
    info.media_time = if track.media_time.is_some() {
        track_time_to_ms(track.media_time.unwrap(), track.timescale.unwrap()) as i64 - empty_duration as i64
    } else {
        0
    };
    info.duration = track_time_to_ms(track.duration.unwrap(), track.timescale.unwrap());
    info.track_id = track.track_id.unwrap();

    MP4PARSE_OK
}

#[no_mangle]
pub unsafe extern "C" fn mp4parse_get_track_audio_info(context: *mut MediaContext, track: u32, info: *mut TrackAudioInfo) -> i32 {
    if context.is_null() || info.is_null() {
        return MP4PARSE_ERROR_BADARG;
    }

    let context: &mut MediaContext = &mut *context;

    if track as usize >= context.tracks.len() {
        return MP4PARSE_ERROR_BADARG;
    }

    let track = &context.tracks[track as usize];

    match track.track_type {
        TrackType::Audio => {}
        _ => return MP4PARSE_ERROR_INVALID,
    };

    let audio = match track.data {
        Some(ref data) => data,
        None => return MP4PARSE_ERROR_INVALID,
    };

    let audio = match *audio {
        SampleEntry::Audio(ref x) => x,
        _ => return MP4PARSE_ERROR_INVALID,
    };

    (*info).channels = audio.channelcount;
    (*info).bit_depth = audio.samplesize;
    (*info).sample_rate = audio.samplerate >> 16; // 16.16 fixed point

    MP4PARSE_OK
}

#[no_mangle]
pub unsafe extern "C" fn mp4parse_get_track_video_info(context: *mut MediaContext, track: u32, info: *mut TrackVideoInfo) -> i32 {
    if context.is_null() || info.is_null() {
        return MP4PARSE_ERROR_BADARG;
    }

    let context: &mut MediaContext = &mut *context;

    if track as usize >= context.tracks.len() {
        return MP4PARSE_ERROR_BADARG;
    }

    let track = &context.tracks[track as usize];

    match track.track_type {
        TrackType::Video => {}
        _ => return MP4PARSE_ERROR_INVALID,
    };

    let video = match track.data {
        Some(ref data) => data,
        None => return MP4PARSE_ERROR_INVALID,
    };

    let video = match *video {
        SampleEntry::Video(ref x) => x,
        _ => return MP4PARSE_ERROR_INVALID,
    };

    if let Some(ref tkhd) = track.tkhd {
        (*info).display_width = tkhd.width >> 16; // 16.16 fixed point
        (*info).display_height = tkhd.height >> 16; // 16.16 fixed point
    } else {
        return MP4PARSE_ERROR_INVALID;
    }
    (*info).image_width = video.width;
    (*info).image_width = video.height;

    MP4PARSE_OK
}

#[test]
fn new_context() {
    let context = mp4parse_new();
    assert!(!context.is_null());
    unsafe {
        mp4parse_free(context);
    }
}

#[test]
#[should_panic(expected = "assertion failed")]
fn free_null_context() {
    unsafe {
        mp4parse_free(std::ptr::null_mut());
    }
}

#[test]
fn arg_validation() {
    let null_buffer = std::ptr::null();
    let null_context = std::ptr::null_mut();

    let context = mp4parse_new();
    assert!(!context.is_null());

    let buffer = vec![0u8; 8];

    unsafe {
        assert_eq!(MP4PARSE_ERROR_BADARG,
                   mp4parse_read(null_context, null_buffer, 0));
        assert_eq!(MP4PARSE_ERROR_BADARG,
                   mp4parse_read(context, null_buffer, 0));
    }

    for size in 0..buffer.len() {
        println!("testing buffer length {}", size);
        unsafe {
            assert_eq!(MP4PARSE_ERROR_BADARG,
                       mp4parse_read(context, buffer.as_ptr(), size));
        }
    }

    unsafe {
        mp4parse_free(context);
    }
}
