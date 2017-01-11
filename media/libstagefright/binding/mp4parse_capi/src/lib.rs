//! C API for mp4parse module.
//!
//! Parses ISO Base Media Format aka video/mp4 streams.
//!
//! # Examples
//!
//! ```rust
//! extern crate mp4parse_capi;
//! use std::io::Read;
//!
//! extern fn buf_read(buf: *mut u8, size: usize, userdata: *mut std::os::raw::c_void) -> isize {
//!    let mut input: &mut std::fs::File = unsafe { &mut *(userdata as *mut _) };
//!    let mut buf = unsafe { std::slice::from_raw_parts_mut(buf, size) };
//!    match input.read(&mut buf) {
//!        Ok(n) => n as isize,
//!        Err(_) => -1,
//!    }
//! }
//!
//! let mut file = std::fs::File::open("../mp4parse/tests/minimal.mp4").unwrap();
//! let io = mp4parse_capi::mp4parse_io {
//!     read: buf_read,
//!     userdata: &mut file as *mut _ as *mut std::os::raw::c_void
//! };
//! unsafe {
//!     let parser = mp4parse_capi::mp4parse_new(&io);
//!     let rv = mp4parse_capi::mp4parse_read(parser);
//!     assert_eq!(rv, mp4parse_capi::mp4parse_error::MP4PARSE_OK);
//!     mp4parse_capi::mp4parse_free(parser);
//! }
//! ```

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

extern crate mp4parse;
extern crate byteorder;

use std::io::Read;
use std::collections::HashMap;
use byteorder::WriteBytesExt;

// Symbols we need from our rust api.
use mp4parse::MediaContext;
use mp4parse::TrackType;
use mp4parse::read_mp4;
use mp4parse::Error;
use mp4parse::SampleEntry;
use mp4parse::AudioCodecSpecific;
use mp4parse::VideoCodecSpecific;
use mp4parse::MediaTimeScale;
use mp4parse::MediaScaledTime;
use mp4parse::TrackTimeScale;
use mp4parse::TrackScaledTime;
use mp4parse::serialize_opus_header;
use mp4parse::CodecType;

// rusty-cheddar's C enum generation doesn't namespace enum members by
// prefixing them, so we're forced to do it in our member names until
// https://github.com/Sean1708/rusty-cheddar/pull/35 is fixed.  Importing
// the members into the module namespace avoids doubling up on the
// namespacing on the Rust side.
use mp4parse_error::*;
use mp4parse_track_type::*;

#[repr(C)]
#[derive(PartialEq, Debug)]
pub enum mp4parse_error {
    MP4PARSE_OK = 0,
    MP4PARSE_ERROR_BADARG = 1,
    MP4PARSE_ERROR_INVALID = 2,
    MP4PARSE_ERROR_UNSUPPORTED = 3,
    MP4PARSE_ERROR_EOF = 4,
    MP4PARSE_ERROR_IO = 5,
}

#[repr(C)]
#[derive(PartialEq, Debug)]
pub enum mp4parse_track_type {
    MP4PARSE_TRACK_TYPE_VIDEO = 0,
    MP4PARSE_TRACK_TYPE_AUDIO = 1,
}

impl Default for mp4parse_track_type {
    fn default() -> Self { mp4parse_track_type::MP4PARSE_TRACK_TYPE_VIDEO }
}

#[repr(C)]
#[derive(PartialEq, Debug)]
pub enum mp4parse_codec {
    MP4PARSE_CODEC_UNKNOWN,
    MP4PARSE_CODEC_AAC,
    MP4PARSE_CODEC_FLAC,
    MP4PARSE_CODEC_OPUS,
    MP4PARSE_CODEC_AVC,
    MP4PARSE_CODEC_VP9,
    MP4PARSE_CODEC_MP3,
}

impl Default for mp4parse_codec {
    fn default() -> Self { mp4parse_codec::MP4PARSE_CODEC_UNKNOWN }
}

#[repr(C)]
#[derive(Default)]
pub struct mp4parse_track_info {
    pub track_type: mp4parse_track_type,
    pub codec: mp4parse_codec,
    pub track_id: u32,
    pub duration: u64,
    pub media_time: i64, // wants to be u64? understand how elst adjustment works
    // TODO(kinetik): include crypto guff
}

#[repr(C)]
pub struct mp4parse_byte_data {
    pub length: u32,
    pub data: *const u8,
}

impl Default for mp4parse_byte_data {
    fn default() -> Self {
        mp4parse_byte_data {
            length: 0,
            data: std::ptr::null_mut(),
        }
    }
}

impl mp4parse_byte_data {
    fn set_data(&mut self, data: &Vec<u8>) {
        self.length = data.len() as u32;
        self.data = data.as_ptr();
    }
}

#[repr(C)]
#[derive(Default)]
pub struct mp4parse_pssh_info {
    pub data: mp4parse_byte_data,
}

#[repr(C)]
#[derive(Default)]
pub struct mp4parser_sinf_info {
    pub is_encrypted: u32,
    pub iv_size: u8,
    pub kid: mp4parse_byte_data,
}

#[repr(C)]
#[derive(Default)]
pub struct mp4parse_track_audio_info {
    pub channels: u16,
    pub bit_depth: u16,
    pub sample_rate: u32,
    pub profile: u16,
    pub codec_specific_config: mp4parse_byte_data,
    pub protected_data: mp4parser_sinf_info,
}

#[repr(C)]
#[derive(Default)]
pub struct mp4parse_track_video_info {
    pub display_width: u32,
    pub display_height: u32,
    pub image_width: u16,
    pub image_height: u16,
    pub extra_data: mp4parse_byte_data,
    pub protected_data: mp4parser_sinf_info,
}

#[repr(C)]
#[derive(Default)]
pub struct mp4parse_fragment_info {
    pub fragment_duration: u64,
    // TODO:
    // info in trex box.
}

// Even though mp4parse_parser is opaque to C, rusty-cheddar won't let us
// use more than one member, so we introduce *another* wrapper.
struct Wrap {
    context: MediaContext,
    io: mp4parse_io,
    poisoned: bool,
    opus_header: HashMap<u32, Vec<u8>>,
    pssh_data: Vec<u8>,
}

#[repr(C)]
#[allow(non_camel_case_types)]
pub struct mp4parse_parser(Wrap);

impl mp4parse_parser {
    fn context(&self) -> &MediaContext {
        &self.0.context
    }

    fn context_mut(&mut self) -> &mut MediaContext {
        &mut self.0.context
    }

    fn io_mut(&mut self) -> &mut mp4parse_io {
        &mut self.0.io
    }

    fn poisoned(&self) -> bool {
        self.0.poisoned
    }

    fn set_poisoned(&mut self, poisoned: bool) {
        self.0.poisoned = poisoned;
    }

    fn opus_header_mut(&mut self) -> &mut HashMap<u32, Vec<u8>> {
        &mut self.0.opus_header
    }

    fn pssh_data_mut(&mut self) -> &mut Vec<u8> {
        &mut self.0.pssh_data
    }
}

#[repr(C)]
#[derive(Clone)]
pub struct mp4parse_io {
    pub read: extern fn(buffer: *mut u8, size: usize, userdata: *mut std::os::raw::c_void) -> isize,
    pub userdata: *mut std::os::raw::c_void,
}

impl Read for mp4parse_io {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        if buf.len() > isize::max_value() as usize {
            return Err(std::io::Error::new(std::io::ErrorKind::Other, "buf length overflow in mp4parse_io Read impl"));
        }
        let rv = (self.read)(buf.as_mut_ptr(), buf.len(), self.userdata);
        if rv >= 0 {
            Ok(rv as usize)
        } else {
            Err(std::io::Error::new(std::io::ErrorKind::Other, "I/O error in mp4parse_io Read impl"))
        }
    }
}

// C API wrapper functions.

/// Allocate an `mp4parse_parser*` to read from the supplied `mp4parse_io`.
#[no_mangle]
pub unsafe extern fn mp4parse_new(io: *const mp4parse_io) -> *mut mp4parse_parser {
    if io.is_null() || (*io).userdata.is_null() {
        return std::ptr::null_mut();
    }
    // is_null() isn't available on a fn type because it can't be null (in
    // Rust) by definition.  But since this value is coming from the C API,
    // it *could* be null.  Ideally, we'd wrap it in an Option to represent
    // reality, but this causes rusty-cheddar to emit the wrong type
    // (https://github.com/Sean1708/rusty-cheddar/issues/30).
    if ((*io).read as *mut std::os::raw::c_void).is_null() {
        return std::ptr::null_mut();
    }
    let parser = Box::new(mp4parse_parser(Wrap {
        context: MediaContext::new(),
        io: (*io).clone(),
        poisoned: false,
        opus_header: HashMap::new(),
        pssh_data: Vec::new(),
    }));

    Box::into_raw(parser)
}

/// Free an `mp4parse_parser*` allocated by `mp4parse_new()`.
#[no_mangle]
pub unsafe extern fn mp4parse_free(parser: *mut mp4parse_parser) {
    assert!(!parser.is_null());
    let _ = Box::from_raw(parser);
}

/// Enable mp4_parser log.
#[no_mangle]
pub unsafe extern fn mp4parse_log(enable: bool) {
    mp4parse::set_debug_mode(enable);
}

/// Run the `mp4parse_parser*` allocated by `mp4parse_new()` until EOF or error.
#[no_mangle]
pub unsafe extern fn mp4parse_read(parser: *mut mp4parse_parser) -> mp4parse_error {
    // Validate arguments from C.
    if parser.is_null() || (*parser).poisoned() {
        return MP4PARSE_ERROR_BADARG;
    }

    let mut context = (*parser).context_mut();
    let mut io = (*parser).io_mut();

    let r = read_mp4(io, context);
    match r {
        Ok(_) => MP4PARSE_OK,
        Err(Error::NoMoov) | Err(Error::InvalidData(_)) => {
            // Block further calls. We've probable lost sync.
            (*parser).set_poisoned(true);
            MP4PARSE_ERROR_INVALID
        }
        Err(Error::Unsupported(_)) => MP4PARSE_ERROR_UNSUPPORTED,
        Err(Error::UnexpectedEOF) => MP4PARSE_ERROR_EOF,
        Err(Error::Io(_)) => {
            // Block further calls after a read failure.
            // Getting std::io::ErrorKind::UnexpectedEof is normal
            // but our From trait implementation should have converted
            // those to our Error::UnexpectedEOF variant.
            (*parser).set_poisoned(true);
            MP4PARSE_ERROR_IO
        }
    }
}

/// Return the number of tracks parsed by previous `mp4parse_read()` call.
#[no_mangle]
pub unsafe extern fn mp4parse_get_track_count(parser: *const mp4parse_parser, count: *mut u32) -> mp4parse_error {
    // Validate arguments from C.
    if parser.is_null() || count.is_null() || (*parser).poisoned() {
        return MP4PARSE_ERROR_BADARG;
    }
    let context = (*parser).context();

    // Make sure the track count fits in a u32.
    if context.tracks.len() > u32::max_value() as usize {
        return MP4PARSE_ERROR_INVALID;
    }
    *count = context.tracks.len() as u32;
    MP4PARSE_OK
}

/// Calculate numerator * scale / denominator, if possible.
///
/// Applying the associativity of integer arithmetic, we divide first
/// and add the remainder after multiplying each term separately
/// to preserve precision while leaving more headroom. That is,
/// (n * s) / d is split into floor(n / d) * s + (n % d) * s / d.
///
/// Return None on overflow or if the denominator is zero.
fn rational_scale(numerator: u64, denominator: u64, scale: u64) -> Option<u64> {
    if denominator == 0 {
        return None;
    }
    let integer = numerator / denominator;
    let remainder = numerator % denominator;
    match integer.checked_mul(scale) {
        Some(integer) => remainder.checked_mul(scale)
            .and_then(|remainder| (remainder/denominator).checked_add(integer)),
        None => None,
    }
}

fn media_time_to_us(time: MediaScaledTime, scale: MediaTimeScale) -> Option<u64> {
    let microseconds_per_second = 1000000;
    rational_scale(time.0, scale.0, microseconds_per_second)
}

fn track_time_to_us(time: TrackScaledTime, scale: TrackTimeScale) -> Option<u64> {
    assert_eq!(time.1, scale.1);
    let microseconds_per_second = 1000000;
    rational_scale(time.0, scale.0, microseconds_per_second)
}

/// Fill the supplied `mp4parse_track_info` with metadata for `track`.
#[no_mangle]
pub unsafe extern fn mp4parse_get_track_info(parser: *mut mp4parse_parser, track_index: u32, info: *mut mp4parse_track_info) -> mp4parse_error {
    if parser.is_null() || info.is_null() || (*parser).poisoned() {
        return MP4PARSE_ERROR_BADARG;
    }

    // Initialize fields to default values to ensure all fields are always valid.
    *info = Default::default();

    let context = (*parser).context_mut();
    let track_index: usize = track_index as usize;
    let info: &mut mp4parse_track_info = &mut *info;

    if track_index >= context.tracks.len() {
        return MP4PARSE_ERROR_BADARG;
    }

    info.track_type = match context.tracks[track_index].track_type {
        TrackType::Video => MP4PARSE_TRACK_TYPE_VIDEO,
        TrackType::Audio => MP4PARSE_TRACK_TYPE_AUDIO,
        TrackType::Unknown => return MP4PARSE_ERROR_UNSUPPORTED,
    };

    info.codec = match context.tracks[track_index].data {
        Some(SampleEntry::Audio(ref audio)) => match audio.codec_specific {
            AudioCodecSpecific::OpusSpecificBox(_) =>
                mp4parse_codec::MP4PARSE_CODEC_OPUS,
            AudioCodecSpecific::FLACSpecificBox(_) =>
                mp4parse_codec::MP4PARSE_CODEC_FLAC,
            AudioCodecSpecific::ES_Descriptor(ref esds) if esds.audio_codec == CodecType::AAC =>
                mp4parse_codec::MP4PARSE_CODEC_AAC,
            AudioCodecSpecific::ES_Descriptor(ref esds) if esds.audio_codec == CodecType::MP3 =>
                mp4parse_codec::MP4PARSE_CODEC_MP3,
            AudioCodecSpecific::ES_Descriptor(_) =>
                mp4parse_codec::MP4PARSE_CODEC_UNKNOWN,
        },
        Some(SampleEntry::Video(ref video)) => match video.codec_specific {
            VideoCodecSpecific::VPxConfig(_) =>
                mp4parse_codec::MP4PARSE_CODEC_VP9,
            VideoCodecSpecific::AVCConfig(_) =>
                mp4parse_codec::MP4PARSE_CODEC_AVC,
        },
        _ => mp4parse_codec::MP4PARSE_CODEC_UNKNOWN,
    };

    let track = &context.tracks[track_index];

    if let (Some(track_timescale),
            Some(context_timescale)) = (track.timescale,
                                        context.timescale) {
        let media_time =
            match track.media_time.map_or(Some(0), |media_time| {
                    track_time_to_us(media_time, track_timescale) }) {
                Some(time) => time as i64,
                None => return MP4PARSE_ERROR_INVALID,
            };
        let empty_duration =
            match track.empty_duration.map_or(Some(0), |empty_duration| {
                    media_time_to_us(empty_duration, context_timescale) }) {
                Some(time) => time as i64,
                None => return MP4PARSE_ERROR_INVALID,
            };
        info.media_time = media_time - empty_duration;

        if let Some(track_duration) = track.duration {
            match track_time_to_us(track_duration, track_timescale) {
                Some(duration) => info.duration = duration,
                None => return MP4PARSE_ERROR_INVALID,
            }
        } else {
            // Duration unknown; stagefright returns 0 for this.
            info.duration = 0
        }
    } else {
        return MP4PARSE_ERROR_INVALID
    }

    info.track_id = match track.track_id {
        Some(track_id) => track_id,
        None => return MP4PARSE_ERROR_INVALID,
    };

    MP4PARSE_OK
}

/// Fill the supplied `mp4parse_track_audio_info` with metadata for `track`.
#[no_mangle]
pub unsafe extern fn mp4parse_get_track_audio_info(parser: *mut mp4parse_parser, track_index: u32, info: *mut mp4parse_track_audio_info) -> mp4parse_error {
    if parser.is_null() || info.is_null() || (*parser).poisoned() {
        return MP4PARSE_ERROR_BADARG;
    }

    // Initialize fields to default values to ensure all fields are always valid.
    *info = Default::default();

    let context = (*parser).context_mut();

    if track_index as usize >= context.tracks.len() {
        return MP4PARSE_ERROR_BADARG;
    }

    let track = &context.tracks[track_index as usize];

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

    match audio.codec_specific {
        AudioCodecSpecific::ES_Descriptor(ref v) => {
            if v.codec_esds.len() > std::u32::MAX as usize {
                return MP4PARSE_ERROR_INVALID;
            }
            (*info).codec_specific_config.length = v.codec_esds.len() as u32;
            (*info).codec_specific_config.data = v.codec_esds.as_ptr();
            if let Some(rate) = v.audio_sample_rate {
                (*info).sample_rate = rate;
            }
            if let Some(channels) = v.audio_channel_count {
                (*info).channels = channels;
            }
            if let Some(profile) = v.audio_object_type {
                (*info).profile = profile;
            }
        }
        AudioCodecSpecific::FLACSpecificBox(ref flac) => {
            // Return the STREAMINFO metadata block in the codec_specific.
            let streaminfo = &flac.blocks[0];
            if streaminfo.block_type != 0 || streaminfo.data.len() != 34 {
                return MP4PARSE_ERROR_INVALID;
            }
            (*info).codec_specific_config.length = streaminfo.data.len() as u32;
            (*info).codec_specific_config.data = streaminfo.data.as_ptr();
        }
        AudioCodecSpecific::OpusSpecificBox(ref opus) => {
            let mut v = Vec::new();
            match serialize_opus_header(opus, &mut v) {
                Err(_) => {
                    return MP4PARSE_ERROR_INVALID;
                }
                Ok(_) => {
                    let header = (*parser).opus_header_mut();
                    header.insert(track_index, v);
                    match header.get(&track_index) {
                        None => {}
                        Some(v) => {
                            if v.len() > std::u32::MAX as usize {
                                return MP4PARSE_ERROR_INVALID;
                            }
                            (*info).codec_specific_config.length = v.len() as u32;
                            (*info).codec_specific_config.data = v.as_ptr();
                        }
                    }
                }
            }
        }
    }

    match audio.protection_info.iter().find(|sinf| sinf.tenc.is_some()) {
        Some(ref p) => {
            if let Some(ref tenc) = p.tenc {
                (*info).protected_data.is_encrypted = tenc.is_encrypted;
                (*info).protected_data.iv_size = tenc.iv_size;
                (*info).protected_data.kid.set_data(&(tenc.kid));
            }
        },
        _ => {},
    }

    MP4PARSE_OK
}

/// Fill the supplied `mp4parse_track_video_info` with metadata for `track`.
#[no_mangle]
pub unsafe extern fn mp4parse_get_track_video_info(parser: *mut mp4parse_parser, track_index: u32, info: *mut mp4parse_track_video_info) -> mp4parse_error {
    if parser.is_null() || info.is_null() || (*parser).poisoned() {
        return MP4PARSE_ERROR_BADARG;
    }

    // Initialize fields to default values to ensure all fields are always valid.
    *info = Default::default();

    let context = (*parser).context_mut();

    if track_index as usize >= context.tracks.len() {
        return MP4PARSE_ERROR_BADARG;
    }

    let track = &context.tracks[track_index as usize];

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
    (*info).image_height = video.height;

    match video.codec_specific {
        VideoCodecSpecific::AVCConfig(ref avc) => {
            (*info).extra_data.set_data(avc);
        },
        _ => {},
    }

    match video.protection_info.iter().find(|sinf| sinf.tenc.is_some()) {
        Some(ref p) => {
            if let Some(ref tenc) = p.tenc {
                (*info).protected_data.is_encrypted = tenc.is_encrypted;
                (*info).protected_data.iv_size = tenc.iv_size;
                (*info).protected_data.kid.set_data(&(tenc.kid));
            }
        },
        _ => {},
    }

    MP4PARSE_OK
}

/// Fill the supplied `mp4parse_fragment_info` with metadata from fragmented file.
#[no_mangle]
pub unsafe extern fn mp4parse_get_fragment_info(parser: *mut mp4parse_parser, info: *mut mp4parse_fragment_info) -> mp4parse_error {
    if parser.is_null() || info.is_null() || (*parser).poisoned() {
        return MP4PARSE_ERROR_BADARG;
    }

    // Initialize fields to default values to ensure all fields are always valid.
    *info = Default::default();

    let context = (*parser).context();
    let info: &mut mp4parse_fragment_info = &mut *info;

    info.fragment_duration = 0;

    let duration = match context.mvex {
        Some(ref mvex) => mvex.fragment_duration,
        None => return MP4PARSE_ERROR_INVALID,
    };

    if let (Some(time), Some(scale)) = (duration, context.timescale) {
        info.fragment_duration = match media_time_to_us(time, scale) {
            Some(time_us) => time_us as u64,
            None => return MP4PARSE_ERROR_INVALID,
        }
    }

    MP4PARSE_OK
}

/// A fragmented file needs mvex table and contains no data in stts, stsc, and stco boxes.
#[no_mangle]
pub unsafe extern fn mp4parse_is_fragmented(parser: *mut mp4parse_parser, track_id: u32, fragmented: *mut u8) -> mp4parse_error {
    if parser.is_null() || (*parser).poisoned() {
        return MP4PARSE_ERROR_BADARG;
    }

    let context = (*parser).context_mut();
    let tracks = &context.tracks;
    (*fragmented) = false as u8;

    if context.mvex.is_none() {
        return MP4PARSE_OK;
    }

    // check sample tables.
    let mut iter = tracks.iter();
    match iter.find(|track| track.track_id == Some(track_id)) {
        Some(track) if track.empty_sample_boxes.all_empty() => (*fragmented) = true as u8,
        Some(_) => {},
        None => return MP4PARSE_ERROR_BADARG,
    }

    MP4PARSE_OK
}

/// Get 'pssh' system id and 'pssh' box content for eme playback.
///
/// The data format in 'info' passing to gecko is:
///   system_id
///   pssh box size (in native endian)
///   pssh box content (including header)
#[no_mangle]
pub unsafe extern fn mp4parse_get_pssh_info(parser: *mut mp4parse_parser, info: *mut mp4parse_pssh_info) -> mp4parse_error {
    if parser.is_null() || info.is_null() || (*parser).poisoned() {
        return MP4PARSE_ERROR_BADARG;
    }

    // Initialize fields to default values to ensure all fields are always valid.
    *info = Default::default();

    let context = (*parser).context_mut();
    let pssh_data = (*parser).pssh_data_mut();
    let info: &mut mp4parse_pssh_info = &mut *info;

    pssh_data.clear();
    for pssh in &context.psshs {
        let mut data_len = Vec::new();
        match data_len.write_u32::<byteorder::NativeEndian>(pssh.box_content.len() as u32) {
            Err(_) => {
                return MP4PARSE_ERROR_IO;
            },
            _ => (),
        }
        pssh_data.extend_from_slice(pssh.system_id.as_slice());
        pssh_data.extend_from_slice(data_len.as_slice());
        pssh_data.extend_from_slice(pssh.box_content.as_slice());
    }

    info.data.set_data(&pssh_data);

    MP4PARSE_OK
}

#[cfg(test)]
extern fn panic_read(_: *mut u8, _: usize, _: *mut std::os::raw::c_void) -> isize {
    panic!("panic_read shouldn't be called in these tests");
}

#[cfg(test)]
extern fn error_read(_: *mut u8, _: usize, _: *mut std::os::raw::c_void) -> isize {
    -1
}

#[cfg(test)]
extern fn valid_read(buf: *mut u8, size: usize, userdata: *mut std::os::raw::c_void) -> isize {
    let mut input: &mut std::fs::File = unsafe { &mut *(userdata as *mut _) };

    let mut buf = unsafe { std::slice::from_raw_parts_mut(buf, size) };
    match input.read(&mut buf) {
        Ok(n) => n as isize,
        Err(_) => -1,
    }
}

#[test]
fn new_parser() {
    let mut dummy_value: u32 = 42;
    let io = mp4parse_io {
        read: panic_read,
        userdata: &mut dummy_value as *mut _ as *mut std::os::raw::c_void,
    };
    unsafe {
        let parser = mp4parse_new(&io);
        assert!(!parser.is_null());
        mp4parse_free(parser);
    }
}

#[test]
#[should_panic(expected = "assertion failed")]
fn free_null_parser() {
    unsafe {
        mp4parse_free(std::ptr::null_mut());
    }
}

#[test]
fn get_track_count_null_parser() {
    unsafe {
        let mut count: u32 = 0;
        let rv = mp4parse_get_track_count(std::ptr::null(), std::ptr::null_mut());
        assert_eq!(rv, MP4PARSE_ERROR_BADARG);
        let rv = mp4parse_get_track_count(std::ptr::null(), &mut count);
        assert_eq!(rv, MP4PARSE_ERROR_BADARG);
    }
}

#[test]
fn arg_validation() {
    unsafe {
        // Passing a null mp4parse_io is an error.
        let parser = mp4parse_new(std::ptr::null());
        assert!(parser.is_null());

        let null_mut: *mut std::os::raw::c_void = std::ptr::null_mut();

        // Passing an mp4parse_io with null members is an error.
        let io = mp4parse_io { read: std::mem::transmute(null_mut),
                               userdata: null_mut };
        let parser = mp4parse_new(&io);
        assert!(parser.is_null());

        let io = mp4parse_io { read: panic_read,
                               userdata: null_mut };
        let parser = mp4parse_new(&io);
        assert!(parser.is_null());

        let mut dummy_value = 42;
        let io = mp4parse_io {
            read: std::mem::transmute(null_mut),
            userdata: &mut dummy_value as *mut _ as *mut std::os::raw::c_void,
        };
        let parser = mp4parse_new(&io);
        assert!(parser.is_null());

        // Passing a null mp4parse_parser is an error.
        assert_eq!(MP4PARSE_ERROR_BADARG, mp4parse_read(std::ptr::null_mut()));

        let mut dummy_info = mp4parse_track_info {
            track_type: MP4PARSE_TRACK_TYPE_VIDEO,
            codec: mp4parse_codec::MP4PARSE_CODEC_UNKNOWN,
            track_id: 0,
            duration: 0,
            media_time: 0,
        };
        assert_eq!(MP4PARSE_ERROR_BADARG, mp4parse_get_track_info(std::ptr::null_mut(), 0, &mut dummy_info));

        let mut dummy_video = mp4parse_track_video_info {
            display_width: 0,
            display_height: 0,
            image_width: 0,
            image_height: 0,
            extra_data: mp4parse_byte_data::default(),
            protected_data: Default::default(),
        };
        assert_eq!(MP4PARSE_ERROR_BADARG, mp4parse_get_track_video_info(std::ptr::null_mut(), 0, &mut dummy_video));

        let mut dummy_audio = Default::default();
        assert_eq!(MP4PARSE_ERROR_BADARG, mp4parse_get_track_audio_info(std::ptr::null_mut(), 0, &mut dummy_audio));
    }
}

#[test]
fn arg_validation_with_parser() {
    unsafe {
        let mut dummy_value = 42;
        let io = mp4parse_io {
            read: error_read,
            userdata: &mut dummy_value as *mut _ as *mut std::os::raw::c_void,
        };
        let parser = mp4parse_new(&io);
        assert!(!parser.is_null());

        // Our mp4parse_io read should simply fail with an error.
        assert_eq!(MP4PARSE_ERROR_IO, mp4parse_read(parser));

        // The parser is now poisoned and unusable.
        assert_eq!(MP4PARSE_ERROR_BADARG,  mp4parse_read(parser));

        // Null info pointers are an error.
        assert_eq!(MP4PARSE_ERROR_BADARG, mp4parse_get_track_info(parser, 0, std::ptr::null_mut()));
        assert_eq!(MP4PARSE_ERROR_BADARG, mp4parse_get_track_video_info(parser, 0, std::ptr::null_mut()));
        assert_eq!(MP4PARSE_ERROR_BADARG, mp4parse_get_track_audio_info(parser, 0, std::ptr::null_mut()));

        let mut dummy_info = mp4parse_track_info {
            track_type: MP4PARSE_TRACK_TYPE_VIDEO,
            codec: mp4parse_codec::MP4PARSE_CODEC_UNKNOWN,
            track_id: 0,
            duration: 0,
            media_time: 0,
        };
        assert_eq!(MP4PARSE_ERROR_BADARG, mp4parse_get_track_info(parser, 0, &mut dummy_info));

        let mut dummy_video = mp4parse_track_video_info {
            display_width: 0,
            display_height: 0,
            image_width: 0,
            image_height: 0,
            extra_data: mp4parse_byte_data::default(),
            protected_data: Default::default(),
        };
        assert_eq!(MP4PARSE_ERROR_BADARG, mp4parse_get_track_video_info(parser, 0, &mut dummy_video));

        let mut dummy_audio = Default::default();
        assert_eq!(MP4PARSE_ERROR_BADARG, mp4parse_get_track_audio_info(parser, 0, &mut dummy_audio));

        mp4parse_free(parser);
    }
}

#[test]
fn get_track_count_poisoned_parser() {
    unsafe {
        let mut dummy_value = 42;
        let io = mp4parse_io {
            read: error_read,
            userdata: &mut dummy_value as *mut _ as *mut std::os::raw::c_void,
        };
        let parser = mp4parse_new(&io);
        assert!(!parser.is_null());

        // Our mp4parse_io read should simply fail with an error.
        assert_eq!(MP4PARSE_ERROR_IO, mp4parse_read(parser));

        let mut count: u32 = 0;
        let rv = mp4parse_get_track_count(parser, &mut count);
        assert_eq!(rv, MP4PARSE_ERROR_BADARG);
    }
}

#[test]
fn arg_validation_with_data() {
    unsafe {
        let mut file = std::fs::File::open("../mp4parse/tests/minimal.mp4").unwrap();
        let io = mp4parse_io { read: valid_read,
                               userdata: &mut file as *mut _ as *mut std::os::raw::c_void };
        let parser = mp4parse_new(&io);
        assert!(!parser.is_null());

        assert_eq!(MP4PARSE_OK, mp4parse_read(parser));

        let mut count: u32 = 0;
        assert_eq!(MP4PARSE_OK, mp4parse_get_track_count(parser, &mut count));
        assert_eq!(2, count);

        let mut info = mp4parse_track_info {
            track_type: MP4PARSE_TRACK_TYPE_VIDEO,
            codec: mp4parse_codec::MP4PARSE_CODEC_UNKNOWN,
            track_id: 0,
            duration: 0,
            media_time: 0,
        };
        assert_eq!(MP4PARSE_OK, mp4parse_get_track_info(parser, 0, &mut info));
        assert_eq!(info.track_type, MP4PARSE_TRACK_TYPE_VIDEO);
        assert_eq!(info.codec, mp4parse_codec::MP4PARSE_CODEC_AVC);
        assert_eq!(info.track_id, 1);
        assert_eq!(info.duration, 40000);
        assert_eq!(info.media_time, 0);

        assert_eq!(MP4PARSE_OK, mp4parse_get_track_info(parser, 1, &mut info));
        assert_eq!(info.track_type, MP4PARSE_TRACK_TYPE_AUDIO);
        assert_eq!(info.codec, mp4parse_codec::MP4PARSE_CODEC_AAC);
        assert_eq!(info.track_id, 2);
        assert_eq!(info.duration, 61333);
        assert_eq!(info.media_time, 21333);

        let mut video = mp4parse_track_video_info {
            display_width: 0,
            display_height: 0,
            image_width: 0,
            image_height: 0,
            extra_data: mp4parse_byte_data::default(),
            protected_data: Default::default(),
        };
        assert_eq!(MP4PARSE_OK, mp4parse_get_track_video_info(parser, 0, &mut video));
        assert_eq!(video.display_width, 320);
        assert_eq!(video.display_height, 240);
        assert_eq!(video.image_width, 320);
        assert_eq!(video.image_height, 240);

        let mut audio = Default::default();
        assert_eq!(MP4PARSE_OK, mp4parse_get_track_audio_info(parser, 1, &mut audio));
        assert_eq!(audio.channels, 1);
        assert_eq!(audio.bit_depth, 16);
        assert_eq!(audio.sample_rate, 48000);

        // Test with an invalid track number.
        let mut info = mp4parse_track_info {
            track_type: MP4PARSE_TRACK_TYPE_VIDEO,
            codec: mp4parse_codec::MP4PARSE_CODEC_UNKNOWN,
            track_id: 0,
            duration: 0,
            media_time: 0,
        };
        assert_eq!(MP4PARSE_ERROR_BADARG, mp4parse_get_track_info(parser, 3, &mut info));
        assert_eq!(info.track_type, MP4PARSE_TRACK_TYPE_VIDEO);
        assert_eq!(info.codec, mp4parse_codec::MP4PARSE_CODEC_UNKNOWN);
        assert_eq!(info.track_id, 0);
        assert_eq!(info.duration, 0);
        assert_eq!(info.media_time, 0);

        let mut video = mp4parse_track_video_info { display_width: 0,
                                                    display_height: 0,
                                                    image_width: 0,
                                                    image_height: 0,
                                                    extra_data: mp4parse_byte_data::default(),
                                                    protected_data: Default::default(),
        };
        assert_eq!(MP4PARSE_ERROR_BADARG, mp4parse_get_track_video_info(parser, 3, &mut video));
        assert_eq!(video.display_width, 0);
        assert_eq!(video.display_height, 0);
        assert_eq!(video.image_width, 0);
        assert_eq!(video.image_height, 0);

        let mut audio = Default::default();
        assert_eq!(MP4PARSE_ERROR_BADARG, mp4parse_get_track_audio_info(parser, 3, &mut audio));
        assert_eq!(audio.channels, 0);
        assert_eq!(audio.bit_depth, 0);
        assert_eq!(audio.sample_rate, 0);

        mp4parse_free(parser);
    }
}

#[test]
fn rational_scale_overflow() {
    assert_eq!(rational_scale(17, 3, 1000), Some(5666));
    let large = 0x4000_0000_0000_0000;
    assert_eq!(rational_scale(large, 2, 2), Some(large));
    assert_eq!(rational_scale(large, 4, 4), Some(large));
    assert_eq!(rational_scale(large, 2, 8), None);
    assert_eq!(rational_scale(large, 8, 4), Some(large/2));
    assert_eq!(rational_scale(large + 1, 4, 4), Some(large+1));
    assert_eq!(rational_scale(large, 40, 1000), None);
}

#[test]
fn media_time_overflow() {
  let scale = MediaTimeScale(90000);
  let duration = MediaScaledTime(9007199254710000);
  assert_eq!(media_time_to_us(duration, scale), Some(100079991719000000));
}

#[test]
fn track_time_overflow() {
  let scale = TrackTimeScale(44100, 0);
  let duration = TrackScaledTime(4413527634807900, 0);
  assert_eq!(track_time_to_us(duration, scale), Some(100079991719000000));
}
