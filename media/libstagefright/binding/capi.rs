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

const TRACK_TYPE_H264: u32 = 0;
const TRACK_TYPE_AAC:  u32 = 1;

// These structs *must* match those declared in include/mp4parse.h.
#[repr(C)]
pub struct TrackInfo {
    track_type: u32,
    track_id: u32,
    duration: u64,
    media_time: i64, // wants to be u64? understand how elst adjustment works
}

#[repr(C)]
pub struct TrackAudioInfo {
    channels: u16,
    bit_depth: u16,
    sample_rate: u32,
//    profile: i32,
//    extended_profile: i32, // check types
}

#[repr(C)]
pub struct TrackVideoInfo {
    display_width: u32,
    display_height: u32,
    image_width: u16,
    image_height: u16,
}

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
/// parser context, returning the number of detected tracks.
///
/// This is safe to call with NULL arguments but will crash
/// if given invalid pointers, as is usual for C.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_read(context: *mut MediaContext, buffer: *const u8, size: usize) -> i32 {
    // Validate arguments from C.
    if context.is_null() || buffer.is_null() || size < 8 {
        return -1;
    }

    let mut context: &mut MediaContext = &mut *context;

    // Wrap the buffer we've been give in a slice.
    let b = std::slice::from_raw_parts(buffer, size);
    let mut c = Cursor::new(b);

    // Parse in a subthread to catch any panics.
    let task = std::thread::spawn(move || {
        match read_mp4(&mut c, &mut context) {
            Ok(_) => {},
            Err(Error::UnexpectedEOF) => {},
            Err(e) => { panic!(e); },
        }
        // Make sure the track count fits in an i32 so we can use
        // negative values for failure.
        assert!(context.tracks.len() < i32::max_value() as usize);
        context.tracks.len() as i32
    });
    task.join().unwrap_or(-1)
}

#[no_mangle]
pub unsafe extern "C" fn mp4parse_get_track_info(context: *mut MediaContext, track: i32, info: *mut TrackInfo) -> i32 {
    if context.is_null() || track < 0 || info.is_null() {
        return -1;
    }

    let context: &mut MediaContext = &mut *context;

    if track as usize >= context.tracks.len() {
        return -1;
    }

    let track = &context.tracks[track as usize];

    (*info).track_type = match track.track_type {
        TrackType::Video => TRACK_TYPE_H264,
        TrackType::Audio => TRACK_TYPE_AAC,
        TrackType::Unknown => return -1,
    };

    // Maybe context & track should just have a single simple is_valid() instead?
    if context.timescale.is_none() ||
        track.timescale.is_none() ||
        track.duration.is_none() ||
        track.track_id.is_none() {
            return -1;
        }

    let empty_duration = if track.empty_duration.is_some() {
        media_time_to_ms(track.empty_duration.unwrap(), context.timescale.unwrap())
    } else {
        0
    };
    (*info).media_time = if track.media_time.is_some() {
        track_time_to_ms(track.media_time.unwrap(), track.timescale.unwrap()) as i64 - empty_duration as i64
    } else {
        0
    };
    (*info).duration = track_time_to_ms(track.duration.unwrap(), track.timescale.unwrap());
    (*info).track_id = track.track_id.unwrap();

    0
}

#[no_mangle]
pub unsafe extern "C" fn mp4parse_get_track_audio_info(context: *mut MediaContext, track: i32, info: *mut TrackAudioInfo) -> i32 {
    if context.is_null() || track < 0 || info.is_null() {
        return -1;
    }

    let context: &mut MediaContext = &mut *context;

    if track as usize >= context.tracks.len() {
        return -1;
    }

    let track = &context.tracks[track as usize];

    match track.track_type {
        TrackType::Audio => {},
        _ => return -1,
    };

    let audio = match track.data {
        Some(ref data) => data,
        None => return -1,
    };

    let audio = match audio {
        &SampleEntry::Audio(ref x) => x,
        _ => return -1,
    };

    (*info).channels = audio.channelcount;
    (*info).bit_depth = audio.samplesize;
    (*info).sample_rate = audio.samplerate >> 16; // 16.16 fixed point

    0
}

#[no_mangle]
pub unsafe extern "C" fn mp4parse_get_track_video_info(context: *mut MediaContext, track: i32, info: *mut TrackVideoInfo) -> i32 {
    if context.is_null() || track < 0 || info.is_null() {
        return -1;
    }

    let context: &mut MediaContext = &mut *context;

    if track as usize >= context.tracks.len() {
        return -1;
    }

    let track = &context.tracks[track as usize];

    match track.track_type {
        TrackType::Video => {},
        _ => return -1,
    };

    let video = match track.data {
        Some(ref data) => data,
        None => return -1,
    };

    let video = match video {
        &SampleEntry::Video(ref x) => x,
        _ => return -1,
    };

    if let Some(ref tkhd) = track.tkhd {
        (*info).display_width = tkhd.width >> 16; // 16.16 fixed point
        (*info).display_height = tkhd.height >> 16; // 16.16 fixed point
    } else {
        return -1
    }
    (*info).image_width = video.width;
    (*info).image_width = video.height;

    0
}

#[test]
fn new_context() {
    let context = mp4parse_new();
    assert!(!context.is_null());
    unsafe { mp4parse_free(context); }
}

#[test]
#[should_panic(expected = "assertion failed")]
fn free_null_context() {
    unsafe { mp4parse_free(std::ptr::null_mut()); }
}

#[test]
fn arg_validation() {
    let null_buffer = std::ptr::null();
    let null_context = std::ptr::null_mut();

    let context = mp4parse_new();
    assert!(!context.is_null());

    let buffer = vec![0u8; 8];

    unsafe {
        assert_eq!(-1, mp4parse_read(null_context, null_buffer, 0));
        assert_eq!(-1, mp4parse_read(context, null_buffer, 0));
    }

    for size in 0..buffer.len() {
        println!("testing buffer length {}", size);
        unsafe {
            assert_eq!(-1, mp4parse_read(context, buffer.as_ptr(), size));
        }
    }

    unsafe { mp4parse_free(context); }
}
