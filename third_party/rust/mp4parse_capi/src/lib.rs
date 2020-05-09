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
//! let io = mp4parse_capi::Mp4parseIo {
//!     read: Some(buf_read),
//!     userdata: &mut file as *mut _ as *mut std::os::raw::c_void
//! };
//! let mut parser = std::ptr::null_mut();
//! unsafe {
//!     let rv = mp4parse_capi::mp4parse_new(&io, &mut parser);
//!     assert_eq!(rv, mp4parse_capi::Mp4parseStatus::Ok);
//!     assert!(!parser.is_null());
//!     mp4parse_capi::mp4parse_free(parser);
//! }
//! ```

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

extern crate byteorder;
extern crate mp4parse;
extern crate num_traits;

use byteorder::WriteBytesExt;
use num_traits::{PrimInt, Zero};
use std::io::Read;

// Symbols we need from our rust api.
use mp4parse::read_avif;
use mp4parse::read_mp4;
use mp4parse::serialize_opus_header;
use mp4parse::AudioCodecSpecific;
use mp4parse::AvifContext;
use mp4parse::CodecType;
use mp4parse::Error;
use mp4parse::MediaContext;
use mp4parse::MediaScaledTime;
use mp4parse::MediaTimeScale;
use mp4parse::SampleEntry;
use mp4parse::Track;
use mp4parse::TrackScaledTime;
use mp4parse::TrackTimeScale;
use mp4parse::TrackType;
use mp4parse::TryBox;
use mp4parse::TryHashMap;
use mp4parse::TryVec;
use mp4parse::VideoCodecSpecific;

// To ensure we don't use stdlib allocating types by accident
#[allow(dead_code)]
struct Vec;
#[allow(dead_code)]
struct Box;
#[allow(dead_code)]
struct HashMap;
#[allow(dead_code)]
struct String;

#[repr(C)]
#[derive(PartialEq, Debug)]
pub enum Mp4parseStatus {
    Ok = 0,
    BadArg = 1,
    Invalid = 2,
    Unsupported = 3,
    Eof = 4,
    Io = 5,
    Oom = 6,
}

#[repr(C)]
#[derive(PartialEq, Debug)]
pub enum Mp4parseTrackType {
    Video = 0,
    Audio = 1,
    Metadata = 2,
}

impl Default for Mp4parseTrackType {
    fn default() -> Self {
        Mp4parseTrackType::Video
    }
}

#[allow(non_camel_case_types)]
#[repr(C)]
#[derive(PartialEq, Debug)]
pub enum Mp4parseCodec {
    Unknown,
    Aac,
    Flac,
    Opus,
    Avc,
    Vp9,
    Av1,
    Mp3,
    Mp4v,
    Jpeg, // for QT JPEG atom in video track
    Ac3,
    Ec3,
    Alac,
}

impl Default for Mp4parseCodec {
    fn default() -> Self {
        Mp4parseCodec::Unknown
    }
}

#[repr(C)]
#[derive(PartialEq, Debug)]
pub enum Mp4ParseEncryptionSchemeType {
    None,
    Cenc,
    Cbc1,
    Cens,
    Cbcs,
    // Schemes also have a version component. At the time of writing, this does
    // not impact handling, so we do not expose it. Note that this may need to
    // be exposed in future, should the spec change.
}

impl Default for Mp4ParseEncryptionSchemeType {
    fn default() -> Self {
        Mp4ParseEncryptionSchemeType::None
    }
}

#[repr(C)]
#[derive(Default, Debug)]
pub struct Mp4parseTrackInfo {
    pub track_type: Mp4parseTrackType,
    pub track_id: u32,
    pub duration: u64,
    pub media_time: i64, // wants to be u64? understand how elst adjustment works
                         // TODO(kinetik): include crypto guff
}

#[repr(C)]
#[derive(Default, Debug, PartialEq)]
pub struct Mp4parseIndice {
    /// The byte offset in the file where the indexed sample begins.
    pub start_offset: u64,
    /// The byte offset in the file where the indexed sample ends. This is
    /// equivalent to `start_offset` + the length in bytes of the indexed
    /// sample. Typically this will be the `start_offset` of the next sample
    /// in the file.
    pub end_offset: u64,
    /// The time in microseconds when the indexed sample should be displayed.
    /// Analogous to the concept of presentation time stamp (pts).
    pub start_composition: i64,
    /// The time in microseconds when the indexed sample should stop being
    /// displayed. Typically this would be the `start_composition` time of the
    /// next sample if samples were ordered by composition time.
    pub end_composition: i64,
    /// The time in microseconds that the indexed sample should be decoded at.
    /// Analogous to the concept of decode time stamp (dts).
    pub start_decode: i64,
    /// Set if the indexed sample is a sync sample. The meaning of sync is
    /// somewhat codec specific, but essentially amounts to if the sample is a
    /// key frame.
    pub sync: bool,
}

#[repr(C)]
#[derive(Debug)]
pub struct Mp4parseByteData {
    pub length: u32,
    // cheddar can't handle generic type, so it needs to be multiple data types here.
    pub data: *const u8,
    pub indices: *const Mp4parseIndice,
}

impl Default for Mp4parseByteData {
    fn default() -> Self {
        Self {
            length: 0,
            data: std::ptr::null(),
            indices: std::ptr::null(),
        }
    }
}

impl Mp4parseByteData {
    fn set_data(&mut self, data: &[u8]) {
        self.length = data.len() as u32;
        self.data = data.as_ptr();
    }

    fn set_indices(&mut self, data: &[Mp4parseIndice]) {
        self.length = data.len() as u32;
        self.indices = data.as_ptr();
    }
}

#[repr(C)]
#[derive(Default)]
pub struct Mp4parsePsshInfo {
    pub data: Mp4parseByteData,
}

#[repr(C)]
#[derive(Default, Debug)]
pub struct Mp4parseSinfInfo {
    pub scheme_type: Mp4ParseEncryptionSchemeType,
    pub is_encrypted: u8,
    pub iv_size: u8,
    pub kid: Mp4parseByteData,
    // Members for pattern encryption schemes, may be 0 (u8) or empty
    // (Mp4parseByteData) if pattern encryption is not in use
    pub crypt_byte_block: u8,
    pub skip_byte_block: u8,
    pub constant_iv: Mp4parseByteData,
    // End pattern encryption scheme members
}

#[repr(C)]
#[derive(Default, Debug)]
pub struct Mp4parseTrackAudioSampleInfo {
    pub codec_type: Mp4parseCodec,
    pub channels: u16,
    pub bit_depth: u16,
    pub sample_rate: u32,
    pub profile: u16,
    pub extended_profile: u16,
    pub codec_specific_config: Mp4parseByteData,
    pub extra_data: Mp4parseByteData,
    pub protected_data: Mp4parseSinfInfo,
}

#[repr(C)]
#[derive(Debug)]
pub struct Mp4parseTrackAudioInfo {
    pub sample_info_count: u32,
    pub sample_info: *const Mp4parseTrackAudioSampleInfo,
}

impl Default for Mp4parseTrackAudioInfo {
    fn default() -> Self {
        Self {
            sample_info_count: 0,
            sample_info: std::ptr::null(),
        }
    }
}

#[repr(C)]
#[derive(Default, Debug)]
pub struct Mp4parseTrackVideoSampleInfo {
    pub codec_type: Mp4parseCodec,
    pub image_width: u16,
    pub image_height: u16,
    pub extra_data: Mp4parseByteData,
    pub protected_data: Mp4parseSinfInfo,
}

#[repr(C)]
#[derive(Debug)]
pub struct Mp4parseTrackVideoInfo {
    pub display_width: u32,
    pub display_height: u32,
    pub rotation: u16,
    pub sample_info_count: u32,
    pub sample_info: *const Mp4parseTrackVideoSampleInfo,
}

impl Default for Mp4parseTrackVideoInfo {
    fn default() -> Self {
        Self {
            display_width: 0,
            display_height: 0,
            rotation: 0,
            sample_info_count: 0,
            sample_info: std::ptr::null(),
        }
    }
}

#[repr(C)]
#[derive(Default, Debug)]
pub struct Mp4parseFragmentInfo {
    pub fragment_duration: u64,
    // TODO:
    // info in trex box.
}

#[derive(Default)]
pub struct Mp4parseParser {
    context: MediaContext,
    opus_header: TryHashMap<u32, TryVec<u8>>,
    pssh_data: TryVec<u8>,
    sample_table: TryHashMap<u32, TryVec<Mp4parseIndice>>,
    // Store a mapping from track index (not id) to associated sample
    // descriptions. Because each track has a variable number of sample
    // descriptions, and because we need the data to live long enough to be
    // copied out by callers, we store these on the parser struct.
    audio_track_sample_descriptions: TryHashMap<u32, TryVec<Mp4parseTrackAudioSampleInfo>>,
    video_track_sample_descriptions: TryHashMap<u32, TryVec<Mp4parseTrackVideoSampleInfo>>,
}

/// A unified interface for the parsers which have different contexts, but
/// share the same pattern of construction. This allows unification of
/// argument validation from C and minimizes the surface of unsafe code.
trait ContextParser
where
    Self: Sized,
{
    type Context: Default;

    fn with_context(context: Self::Context) -> Self;

    fn read<T: Read>(io: &mut T, context: &mut Self::Context) -> mp4parse::Result<()>;
}

impl Mp4parseParser {
    fn context(&self) -> &MediaContext {
        &self.context
    }

    fn context_mut(&mut self) -> &mut MediaContext {
        &mut self.context
    }
}

impl ContextParser for Mp4parseParser {
    type Context = MediaContext;

    fn with_context(context: Self::Context) -> Self {
        Self {
            context,
            ..Default::default()
        }
    }

    fn read<T: Read>(io: &mut T, context: &mut Self::Context) -> mp4parse::Result<()> {
        read_mp4(io, context)
    }
}

#[derive(Default)]
pub struct Mp4parseAvifParser {
    context: AvifContext,
}

impl Mp4parseAvifParser {
    fn context(&self) -> &AvifContext {
        &self.context
    }
}

impl ContextParser for Mp4parseAvifParser {
    type Context = AvifContext;

    fn with_context(context: Self::Context) -> Self {
        Self { context }
    }

    fn read<T: Read>(io: &mut T, context: &mut Self::Context) -> mp4parse::Result<()> {
        read_avif(io, context)
    }
}

#[repr(C)]
#[derive(Clone)]
pub struct Mp4parseIo {
    pub read: Option<
        extern "C" fn(buffer: *mut u8, size: usize, userdata: *mut std::os::raw::c_void) -> isize,
    >,
    pub userdata: *mut std::os::raw::c_void,
}

impl Read for Mp4parseIo {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        if buf.len() > isize::max_value() as usize {
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "buf length overflow in Mp4parseIo Read impl",
            ));
        }
        let rv = self.read.unwrap()(buf.as_mut_ptr(), buf.len(), self.userdata);
        if rv >= 0 {
            Ok(rv as usize)
        } else {
            Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "I/O error in Mp4parseIo Read impl",
            ))
        }
    }
}

// C API wrapper functions.

/// Allocate an `Mp4parseParser*` to read from the supplied `Mp4parseIo` and
/// parse the content from the `Mp4parseIo` argument until EOF or error.
///
/// # Safety
///
/// This function is unsafe because it dereferences the `io` and `parser_out`
/// pointers given to it. The caller should ensure that the `Mp4ParseIo`
/// struct passed in is a valid pointer. The caller should also ensure the
/// members of io are valid: the `read` function should be sanely implemented,
/// and the `userdata` pointer should be valid. The `parser_out` should be a
/// valid pointer to a location containing a null pointer. Upon successful
/// return (`Mp4parseStatus::Ok`), that location will contain the address of
/// an `Mp4parseParser` allocated by this function.
///
/// To avoid leaking memory, any successful return of this function must be
/// paired with a call to `mp4parse_free`. In the event of error, no memory
/// will be allocated and `mp4parse_free` must *not* be called.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_new(
    io: *const Mp4parseIo,
    parser_out: *mut *mut Mp4parseParser,
) -> Mp4parseStatus {
    mp4parse_new_common(io, parser_out)
}

/// Allocate an `Mp4parseAvifParser*` to read from the supplied `Mp4parseIo`.
///
/// See mp4parse_new; this function is identical except that it allocates an
/// `Mp4parseAvifParser`, which (when successful) must be paired with a call
/// to mp4parse_avif_free.
///
/// # Safety
///
/// Same as mp4parse_new.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_avif_new(
    io: *const Mp4parseIo,
    parser_out: *mut *mut Mp4parseAvifParser,
) -> Mp4parseStatus {
    mp4parse_new_common(io, parser_out)
}

unsafe fn mp4parse_new_common<P: ContextParser>(
    io: *const Mp4parseIo,
    parser_out: *mut *mut P,
) -> Mp4parseStatus {
    // Validate arguments from C.
    if io.is_null()
        || (*io).userdata.is_null()
        || (*io).read.is_none()
        || parser_out.is_null()
        || !(*parser_out).is_null()
    {
        Mp4parseStatus::BadArg
    } else {
        match mp4parse_new_common_safe(&mut (*io).clone()) {
            Ok(parser) => {
                *parser_out = parser;
                Mp4parseStatus::Ok
            }
            Err(status) => status,
        }
    }
}

fn mp4parse_new_common_safe<T: Read, P: ContextParser>(
    io: &mut T,
) -> Result<*mut P, Mp4parseStatus> {
    let mut context = P::Context::default();

    P::read(io, &mut context)
        .map(|_| P::with_context(context))
        .and_then(TryBox::try_new)
        .map(TryBox::into_raw)
        .map_err(Mp4parseStatus::from)
}

impl From<mp4parse::Error> for Mp4parseStatus {
    fn from(error: mp4parse::Error) -> Self {
        match error {
            Error::NoMoov | Error::InvalidData(_) => Mp4parseStatus::Invalid,
            Error::Unsupported(_) => Mp4parseStatus::Unsupported,
            Error::UnexpectedEOF => Mp4parseStatus::Eof,
            Error::Io(_) => {
                // Getting std::io::ErrorKind::UnexpectedEof is normal
                // but our From trait implementation should have converted
                // those to our Error::UnexpectedEOF variant.
                Mp4parseStatus::Io
            }
            Error::OutOfMemory => Mp4parseStatus::Oom,
        }
    }
}

impl From<Result<(), Mp4parseStatus>> for Mp4parseStatus {
    fn from(result: Result<(), Mp4parseStatus>) -> Self {
        match result {
            Ok(()) => Mp4parseStatus::Ok,
            Err(Mp4parseStatus::Ok) => unreachable!(),
            Err(e) => e,
        }
    }
}

/// Free an `Mp4parseParser*` allocated by `mp4parse_new()`.
///
/// # Safety
///
/// This function is unsafe because it creates a box from a raw pointer.
/// Callers should ensure that the parser pointer points to a valid
/// `Mp4parseParser` created by `mp4parse_new`.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_free(parser: *mut Mp4parseParser) {
    assert!(!parser.is_null());
    let _ = TryBox::from_raw(parser);
}

/// Free an `Mp4parseAvifParser*` allocated by `mp4parse_avif_new()`.
///
/// # Safety
///
/// This function is unsafe because it creates a box from a raw pointer.
/// Callers should ensure that the parser pointer points to a valid
/// `Mp4parseAvifParser` created by `mp4parse_avif_new`.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_avif_free(parser: *mut Mp4parseAvifParser) {
    assert!(!parser.is_null());
    let _ = TryBox::from_raw(parser);
}

/// Return the number of tracks parsed by previous `mp4parse_read()` call.
///
/// # Safety
///
/// This function is unsafe because it dereferences both the parser and count
/// raw pointers passed into it. Callers should ensure the parser pointer
/// points to a valid `Mp4parseParser`, and that the count pointer points an
/// appropriate memory location to have a `u32` written to.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_get_track_count(
    parser: *const Mp4parseParser,
    count: *mut u32,
) -> Mp4parseStatus {
    // Validate arguments from C.
    if parser.is_null() || count.is_null() {
        return Mp4parseStatus::BadArg;
    }
    let context = (*parser).context();

    // Make sure the track count fits in a u32.
    if context.tracks.len() > u32::max_value() as usize {
        return Mp4parseStatus::Invalid;
    }
    *count = context.tracks.len() as u32;
    Mp4parseStatus::Ok
}

/// Calculate numerator * scale / denominator, if possible.
///
/// Applying the associativity of integer arithmetic, we divide first
/// and add the remainder after multiplying each term separately
/// to preserve precision while leaving more headroom. That is,
/// (n * s) / d is split into floor(n / d) * s + (n % d) * s / d.
///
/// Return None on overflow or if the denominator is zero.
fn rational_scale<T, S>(numerator: T, denominator: T, scale2: S) -> Option<T>
where
    T: PrimInt + Zero,
    S: PrimInt,
{
    if denominator.is_zero() {
        return None;
    }

    let integer = numerator / denominator;
    let remainder = numerator % denominator;
    num_traits::cast(scale2).and_then(|s| match integer.checked_mul(&s) {
        Some(integer) => remainder
            .checked_mul(&s)
            .and_then(|remainder| (remainder / denominator).checked_add(&integer)),
        None => None,
    })
}

fn media_time_to_us(time: MediaScaledTime, scale: MediaTimeScale) -> Option<u64> {
    let microseconds_per_second = 1_000_000;
    rational_scale::<u64, u64>(time.0, scale.0, microseconds_per_second)
}

fn track_time_to_us<T>(time: TrackScaledTime<T>, scale: TrackTimeScale<T>) -> Option<T>
where
    T: PrimInt + Zero,
{
    assert_eq!(time.1, scale.1);
    let microseconds_per_second = 1_000_000;
    rational_scale::<T, u64>(time.0, scale.0, microseconds_per_second)
}

/// Fill the supplied `Mp4parseTrackInfo` with metadata for `track`.
///
/// # Safety
///
/// This function is unsafe because it dereferences the the parser and info raw
/// pointers passed to it. Callers should ensure the parser pointer points to a
/// valid `Mp4parseParser` and that the info pointer points to a valid
/// `Mp4parseTrackInfo`.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_get_track_info(
    parser: *mut Mp4parseParser,
    track_index: u32,
    info: *mut Mp4parseTrackInfo,
) -> Mp4parseStatus {
    if parser.is_null() || info.is_null() {
        return Mp4parseStatus::BadArg;
    }

    // Initialize fields to default values to ensure all fields are always valid.
    *info = Default::default();

    let context = (*parser).context_mut();
    let track_index: usize = track_index as usize;
    let info: &mut Mp4parseTrackInfo = &mut *info;

    if track_index >= context.tracks.len() {
        return Mp4parseStatus::BadArg;
    }

    info.track_type = match context.tracks[track_index].track_type {
        TrackType::Video => Mp4parseTrackType::Video,
        TrackType::Audio => Mp4parseTrackType::Audio,
        TrackType::Metadata => Mp4parseTrackType::Metadata,
        TrackType::Unknown => return Mp4parseStatus::Unsupported,
    };

    let track = &context.tracks[track_index];

    if let (Some(track_timescale), Some(context_timescale)) = (track.timescale, context.timescale) {
        let media_time = match track.media_time.map_or(Some(0), |media_time| {
            track_time_to_us(media_time, track_timescale)
        }) {
            Some(time) => time as i64,
            None => return Mp4parseStatus::Invalid,
        };
        let empty_duration = match track.empty_duration.map_or(Some(0), |empty_duration| {
            media_time_to_us(empty_duration, context_timescale)
        }) {
            Some(time) => time as i64,
            None => return Mp4parseStatus::Invalid,
        };
        info.media_time = media_time - empty_duration;

        if let Some(track_duration) = track.duration {
            match track_time_to_us(track_duration, track_timescale) {
                Some(duration) => info.duration = duration,
                None => return Mp4parseStatus::Invalid,
            }
        } else {
            // Duration unknown; stagefright returns 0 for this.
            info.duration = 0
        }
    } else {
        return Mp4parseStatus::Invalid;
    }

    info.track_id = match track.track_id {
        Some(track_id) => track_id,
        None => return Mp4parseStatus::Invalid,
    };

    Mp4parseStatus::Ok
}

/// Fill the supplied `Mp4parseTrackAudioInfo` with metadata for `track`.
///
/// # Safety
///
/// This function is unsafe because it dereferences the the parser and info raw
/// pointers passed to it. Callers should ensure the parser pointer points to a
/// valid `Mp4parseParser` and that the info pointer points to a valid
/// `Mp4parseTrackAudioInfo`.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_get_track_audio_info(
    parser: *mut Mp4parseParser,
    track_index: u32,
    info: *mut Mp4parseTrackAudioInfo,
) -> Mp4parseStatus {
    if parser.is_null() || info.is_null() {
        return Mp4parseStatus::BadArg;
    }

    // Initialize fields to default values to ensure all fields are always valid.
    *info = Default::default();

    get_track_audio_info(&mut *parser, track_index, &mut *info).into()
}

fn get_track_audio_info(
    parser: &mut Mp4parseParser,
    track_index: u32,
    info: &mut Mp4parseTrackAudioInfo,
) -> Result<(), Mp4parseStatus> {
    let Mp4parseParser {
        context,
        opus_header,
        ..
    } = parser;

    if track_index as usize >= context.tracks.len() {
        return Err(Mp4parseStatus::BadArg);
    }

    let track = &context.tracks[track_index as usize];

    if track.track_type != TrackType::Audio {
        return Err(Mp4parseStatus::Invalid);
    }

    // Handle track.stsd
    let stsd = match track.stsd {
        Some(ref stsd) => stsd,
        None => return Err(Mp4parseStatus::Invalid), // Stsd should be present
    };

    if stsd.descriptions.is_empty() {
        return Err(Mp4parseStatus::Invalid); // Should have at least 1 description
    }

    let mut audio_sample_infos = TryVec::with_capacity(stsd.descriptions.len())?;
    for description in stsd.descriptions.iter() {
        let mut sample_info = Mp4parseTrackAudioSampleInfo::default();
        let audio = match description {
            SampleEntry::Audio(a) => a,
            _ => return Err(Mp4parseStatus::Invalid),
        };

        // UNKNOWN for unsupported format.
        sample_info.codec_type = match audio.codec_specific {
            AudioCodecSpecific::OpusSpecificBox(_) => Mp4parseCodec::Opus,
            AudioCodecSpecific::FLACSpecificBox(_) => Mp4parseCodec::Flac,
            AudioCodecSpecific::ES_Descriptor(ref esds) if esds.audio_codec == CodecType::AAC => {
                Mp4parseCodec::Aac
            }
            AudioCodecSpecific::ES_Descriptor(ref esds) if esds.audio_codec == CodecType::MP3 => {
                Mp4parseCodec::Mp3
            }
            AudioCodecSpecific::ES_Descriptor(_) | AudioCodecSpecific::LPCM => {
                Mp4parseCodec::Unknown
            }
            AudioCodecSpecific::MP3 => Mp4parseCodec::Mp3,
            AudioCodecSpecific::ALACSpecificBox(_) => Mp4parseCodec::Alac,
        };
        sample_info.channels = audio.channelcount as u16;
        sample_info.bit_depth = audio.samplesize;
        sample_info.sample_rate = audio.samplerate as u32;
        // sample_info.profile is handled below on a per case basis

        match audio.codec_specific {
            AudioCodecSpecific::ES_Descriptor(ref esds) => {
                if esds.codec_esds.len() > std::u32::MAX as usize {
                    return Err(Mp4parseStatus::Invalid);
                }
                sample_info.extra_data.length = esds.codec_esds.len() as u32;
                sample_info.extra_data.data = esds.codec_esds.as_ptr();
                sample_info.codec_specific_config.length = esds.decoder_specific_data.len() as u32;
                sample_info.codec_specific_config.data = esds.decoder_specific_data.as_ptr();
                if let Some(rate) = esds.audio_sample_rate {
                    sample_info.sample_rate = rate;
                }
                if let Some(channels) = esds.audio_channel_count {
                    sample_info.channels = channels;
                }
                if let Some(profile) = esds.audio_object_type {
                    sample_info.profile = profile;
                }
                sample_info.extended_profile = match esds.extended_audio_object_type {
                    Some(extended_profile) => extended_profile,
                    _ => sample_info.profile,
                };
            }
            AudioCodecSpecific::FLACSpecificBox(ref flac) => {
                // Return the STREAMINFO metadata block in the codec_specific.
                let streaminfo = &flac.blocks[0];
                if streaminfo.block_type != 0 || streaminfo.data.len() != 34 {
                    return Err(Mp4parseStatus::Invalid);
                }
                sample_info.codec_specific_config.length = streaminfo.data.len() as u32;
                sample_info.codec_specific_config.data = streaminfo.data.as_ptr();
            }
            AudioCodecSpecific::OpusSpecificBox(ref opus) => {
                let mut v = TryVec::new();
                match serialize_opus_header(opus, &mut v) {
                    Err(_) => {
                        return Err(Mp4parseStatus::Invalid);
                    }
                    Ok(_) => {
                        opus_header.insert(track_index, v)?;
                        if let Some(v) = opus_header.get(&track_index) {
                            if v.len() > std::u32::MAX as usize {
                                return Err(Mp4parseStatus::Invalid);
                            }
                            sample_info.codec_specific_config.length = v.len() as u32;
                            sample_info.codec_specific_config.data = v.as_ptr();
                        }
                    }
                }
            }
            AudioCodecSpecific::ALACSpecificBox(ref alac) => {
                sample_info.codec_specific_config.length = alac.data.len() as u32;
                sample_info.codec_specific_config.data = alac.data.as_ptr();
            }
            AudioCodecSpecific::MP3 | AudioCodecSpecific::LPCM => (),
        }

        if let Some(p) = audio
            .protection_info
            .iter()
            .find(|sinf| sinf.tenc.is_some())
        {
            sample_info.protected_data.scheme_type = match p.scheme_type {
                Some(ref scheme_type_box) => {
                    match scheme_type_box.scheme_type.value.as_ref() {
                        b"cenc" => Mp4ParseEncryptionSchemeType::Cenc,
                        b"cbcs" => Mp4ParseEncryptionSchemeType::Cbcs,
                        // We don't support other schemes, and shouldn't reach
                        // this case. Try to gracefully handle by treating as
                        // no encryption case.
                        _ => Mp4ParseEncryptionSchemeType::None,
                    }
                }
                None => Mp4ParseEncryptionSchemeType::None,
            };
            if let Some(ref tenc) = p.tenc {
                sample_info.protected_data.is_encrypted = tenc.is_encrypted;
                sample_info.protected_data.iv_size = tenc.iv_size;
                sample_info.protected_data.kid.set_data(&(tenc.kid));
                sample_info.protected_data.crypt_byte_block = match tenc.crypt_byte_block_count {
                    Some(n) => n,
                    None => 0,
                };
                sample_info.protected_data.skip_byte_block = match tenc.skip_byte_block_count {
                    Some(n) => n,
                    None => 0,
                };
                if let Some(ref iv_vec) = tenc.constant_iv {
                    if iv_vec.len() > std::u32::MAX as usize {
                        return Err(Mp4parseStatus::Invalid);
                    }
                    sample_info.protected_data.constant_iv.set_data(iv_vec);
                };
            }
        }
        audio_sample_infos.push(sample_info)?;
    }

    parser
        .audio_track_sample_descriptions
        .insert(track_index, audio_sample_infos)?;
    match parser.audio_track_sample_descriptions.get(&track_index) {
        Some(sample_info) => {
            if sample_info.len() > std::u32::MAX as usize {
                // Should never happen due to upper limits on number of sample
                // descriptions a track can have, but lets be safe.
                return Err(Mp4parseStatus::Invalid);
            }
            info.sample_info_count = sample_info.len() as u32;
            info.sample_info = sample_info.as_ptr();
        }
        None => return Err(Mp4parseStatus::Invalid), // Shouldn't happen, we just inserted the info!
    }

    Ok(())
}

/// Fill the supplied `Mp4parseTrackVideoInfo` with metadata for `track`.
///
/// # Safety
///
/// This function is unsafe because it dereferences the the parser and info raw
/// pointers passed to it. Callers should ensure the parser pointer points to a
/// valid `Mp4parseParser` and that the info pointer points to a valid
/// `Mp4parseTrackVideoInfo`.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_get_track_video_info(
    parser: *mut Mp4parseParser,
    track_index: u32,
    info: *mut Mp4parseTrackVideoInfo,
) -> Mp4parseStatus {
    if parser.is_null() || info.is_null() {
        return Mp4parseStatus::BadArg;
    }

    // Initialize fields to default values to ensure all fields are always valid.
    *info = Default::default();

    mp4parse_get_track_video_info_safe(&mut *parser, track_index, &mut *info).into()
}

fn mp4parse_get_track_video_info_safe(
    parser: &mut Mp4parseParser,
    track_index: u32,
    info: &mut Mp4parseTrackVideoInfo,
) -> Result<(), Mp4parseStatus> {
    let context = parser.context();

    if track_index as usize >= context.tracks.len() {
        return Err(Mp4parseStatus::BadArg);
    }

    let track = &context.tracks[track_index as usize];

    if track.track_type != TrackType::Video {
        return Err(Mp4parseStatus::Invalid);
    }

    // Handle track.tkhd
    if let Some(ref tkhd) = track.tkhd {
        info.display_width = tkhd.width >> 16; // 16.16 fixed point
        info.display_height = tkhd.height >> 16; // 16.16 fixed point
        let matrix = (
            tkhd.matrix.a >> 16,
            tkhd.matrix.b >> 16,
            tkhd.matrix.c >> 16,
            tkhd.matrix.d >> 16,
        );
        info.rotation = match matrix {
            (0, 1, -1, 0) => 90,   // rotate 90 degrees
            (-1, 0, 0, -1) => 180, // rotate 180 degrees
            (0, -1, 1, 0) => 270,  // rotate 270 degrees
            _ => 0,
        };
    } else {
        return Err(Mp4parseStatus::Invalid);
    }

    // Handle track.stsd
    let stsd = match track.stsd {
        Some(ref stsd) => stsd,
        None => return Err(Mp4parseStatus::Invalid), // Stsd should be present
    };

    if stsd.descriptions.is_empty() {
        return Err(Mp4parseStatus::Invalid); // Should have at least 1 description
    }

    let mut video_sample_infos = TryVec::with_capacity(stsd.descriptions.len())?;
    for description in stsd.descriptions.iter() {
        let mut sample_info = Mp4parseTrackVideoSampleInfo::default();
        let video = match description {
            SampleEntry::Video(v) => v,
            _ => return Err(Mp4parseStatus::Invalid),
        };

        // UNKNOWN for unsupported format.
        sample_info.codec_type = match video.codec_specific {
            VideoCodecSpecific::VPxConfig(_) => Mp4parseCodec::Vp9,
            VideoCodecSpecific::AV1Config(_) => Mp4parseCodec::Av1,
            VideoCodecSpecific::AVCConfig(_) => Mp4parseCodec::Avc,
            VideoCodecSpecific::ESDSConfig(_) =>
            // MP4V (14496-2) video is unsupported.
            {
                Mp4parseCodec::Unknown
            }
        };
        sample_info.image_width = video.width;
        sample_info.image_height = video.height;

        match video.codec_specific {
            VideoCodecSpecific::AVCConfig(ref data) | VideoCodecSpecific::ESDSConfig(ref data) => {
                sample_info.extra_data.set_data(data);
            }
            _ => {}
        }

        if let Some(p) = video
            .protection_info
            .iter()
            .find(|sinf| sinf.tenc.is_some())
        {
            sample_info.protected_data.scheme_type = match p.scheme_type {
                Some(ref scheme_type_box) => {
                    match scheme_type_box.scheme_type.value.as_ref() {
                        b"cenc" => Mp4ParseEncryptionSchemeType::Cenc,
                        b"cbcs" => Mp4ParseEncryptionSchemeType::Cbcs,
                        // We don't support other schemes, and shouldn't reach
                        // this case. Try to gracefully handle by treating as
                        // no encryption case.
                        _ => Mp4ParseEncryptionSchemeType::None,
                    }
                }
                None => Mp4ParseEncryptionSchemeType::None,
            };
            if let Some(ref tenc) = p.tenc {
                sample_info.protected_data.is_encrypted = tenc.is_encrypted;
                sample_info.protected_data.iv_size = tenc.iv_size;
                sample_info.protected_data.kid.set_data(&(tenc.kid));
                sample_info.protected_data.crypt_byte_block = match tenc.crypt_byte_block_count {
                    Some(n) => n,
                    None => 0,
                };
                sample_info.protected_data.skip_byte_block = match tenc.skip_byte_block_count {
                    Some(n) => n,
                    None => 0,
                };
                if let Some(ref iv_vec) = tenc.constant_iv {
                    if iv_vec.len() > std::u32::MAX as usize {
                        return Err(Mp4parseStatus::Invalid);
                    }
                    sample_info.protected_data.constant_iv.set_data(iv_vec);
                };
            }
        }
        video_sample_infos.push(sample_info)?;
    }

    parser
        .video_track_sample_descriptions
        .insert(track_index, video_sample_infos)?;
    match parser.video_track_sample_descriptions.get(&track_index) {
        Some(sample_info) => {
            if sample_info.len() > std::u32::MAX as usize {
                // Should never happen due to upper limits on number of sample
                // descriptions a track can have, but lets be safe.
                return Err(Mp4parseStatus::Invalid);
            }
            info.sample_info_count = sample_info.len() as u32;
            info.sample_info = sample_info.as_ptr();
        }
        None => return Err(Mp4parseStatus::Invalid), // Shouldn't happen, we just inserted the info!
    }
    Ok(())
}

/// Return a pointer to the primary item parsed by previous `mp4parse_avif_new()` call.
///
/// # Safety
///
/// This function is unsafe because it dereferences both the parser and
/// primary_item raw pointers passed into it. Callers should ensure the parser
/// pointer points to a valid `Mp4parseAvifParser`, and that the primary_item
/// pointer points to a valid `Mp4parseByteData`. If there was not a previous
/// successful call to `mp4parse_avif_read()`, no guarantees are made as to
/// the state of `primary_item`.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_avif_get_primary_item(
    parser: *mut Mp4parseAvifParser,
    primary_item: *mut Mp4parseByteData,
) -> Mp4parseStatus {
    if parser.is_null() {
        return Mp4parseStatus::BadArg;
    }

    // Initialize fields to default values to ensure all fields are always valid.
    *primary_item = Default::default();

    let context = (*parser).context();

    // TODO: check for a valid parsed context. See https://github.com/mozilla/mp4parse-rust/issues/195
    (*primary_item).set_data(&context.primary_item);

    Mp4parseStatus::Ok
}

/// Fill the supplied `Mp4parseByteData` with index information from `track`.
///
/// # Safety
///
/// This function is unsafe because it dereferences the the parser and indices
/// raw pointers passed to it. Callers should ensure the parser pointer points
/// to a valid `Mp4parseParser` and that the indices pointer points to a valid
/// `Mp4parseByteData`.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_get_indice_table(
    parser: *mut Mp4parseParser,
    track_id: u32,
    indices: *mut Mp4parseByteData,
) -> Mp4parseStatus {
    if parser.is_null() {
        return Mp4parseStatus::BadArg;
    }

    // Initialize fields to default values to ensure all fields are always valid.
    *indices = Default::default();

    get_indice_table(&mut *parser, track_id, &mut *indices).into()
}

fn get_indice_table(
    parser: &mut Mp4parseParser,
    track_id: u32,
    indices: &mut Mp4parseByteData,
) -> Result<(), Mp4parseStatus> {
    let Mp4parseParser {
        context,
        sample_table: index_table,
        ..
    } = parser;
    let tracks = &context.tracks;
    let track = match tracks.iter().find(|track| track.track_id == Some(track_id)) {
        Some(t) => t,
        _ => return Err(Mp4parseStatus::Invalid),
    };

    if let Some(v) = index_table.get(&track_id) {
        indices.set_indices(v);
        return Ok(());
    }

    let media_time = match (&track.media_time, &track.timescale) {
        (&Some(t), &Some(s)) => track_time_to_us(t, s).map(|v| v as i64),
        _ => None,
    };

    let empty_duration = match (&track.empty_duration, &context.timescale) {
        (&Some(e), &Some(s)) => media_time_to_us(e, s).map(|v| v as i64),
        _ => None,
    };

    // Find the track start offset time from 'elst'.
    // 'media_time' maps start time onward, 'empty_duration' adds time offset
    // before first frame is displayed.
    let offset_time = match (empty_duration, media_time) {
        (Some(e), Some(m)) => e - m,
        (Some(e), None) => e,
        (None, Some(m)) => m,
        _ => 0,
    };

    if let Some(v) = create_sample_table(track, offset_time) {
        indices.set_indices(&v);
        index_table.insert(track_id, v)?;
        return Ok(());
    }

    Err(Mp4parseStatus::Invalid)
}

// Convert a 'ctts' compact table to full table by iterator,
// (sample_with_the_same_offset_count, offset) => (offset), (offset), (offset) ...
//
// For example:
// (2, 10), (4, 9) into (10, 10, 9, 9, 9, 9) by calling next_offset_time().
struct TimeOffsetIterator<'a> {
    cur_sample_range: std::ops::Range<u32>,
    cur_offset: i64,
    ctts_iter: Option<std::slice::Iter<'a, mp4parse::TimeOffset>>,
    track_id: usize,
}

impl<'a> Iterator for TimeOffsetIterator<'a> {
    type Item = i64;

    fn next(&mut self) -> Option<i64> {
        let has_sample = self.cur_sample_range.next().or_else(|| {
            // At end of current TimeOffset, find the next TimeOffset.
            let iter = match self.ctts_iter {
                Some(ref mut v) => v,
                _ => return None,
            };
            let offset_version;
            self.cur_sample_range = match iter.next() {
                Some(v) => {
                    offset_version = v.time_offset;
                    0..v.sample_count
                }
                _ => {
                    offset_version = mp4parse::TimeOffsetVersion::Version0(0);
                    0..0
                }
            };

            self.cur_offset = match offset_version {
                mp4parse::TimeOffsetVersion::Version0(i) => i64::from(i),
                mp4parse::TimeOffsetVersion::Version1(i) => i64::from(i),
            };

            self.cur_sample_range.next()
        });

        has_sample.and(Some(self.cur_offset))
    }
}

impl<'a> TimeOffsetIterator<'a> {
    fn next_offset_time(&mut self) -> TrackScaledTime<i64> {
        match self.next() {
            Some(v) => TrackScaledTime::<i64>(v as i64, self.track_id),
            _ => TrackScaledTime::<i64>(0, self.track_id),
        }
    }
}

// Convert 'stts' compact table to full table by iterator,
// (sample_count_with_the_same_time, time) => (time, time, time) ... repeats
// sample_count_with_the_same_time.
//
// For example:
// (2, 3000), (1, 2999) to (3000, 3000, 2999).
struct TimeToSampleIterator<'a> {
    cur_sample_count: std::ops::Range<u32>,
    cur_sample_delta: u32,
    stts_iter: std::slice::Iter<'a, mp4parse::Sample>,
    track_id: usize,
}

impl<'a> Iterator for TimeToSampleIterator<'a> {
    type Item = u32;

    fn next(&mut self) -> Option<u32> {
        let has_sample = self.cur_sample_count.next().or_else(|| {
            self.cur_sample_count = match self.stts_iter.next() {
                Some(v) => {
                    self.cur_sample_delta = v.sample_delta;
                    0..v.sample_count
                }
                _ => 0..0,
            };

            self.cur_sample_count.next()
        });

        has_sample.and(Some(self.cur_sample_delta))
    }
}

impl<'a> TimeToSampleIterator<'a> {
    fn next_delta(&mut self) -> TrackScaledTime<i64> {
        match self.next() {
            Some(v) => TrackScaledTime::<i64>(i64::from(v), self.track_id),
            _ => TrackScaledTime::<i64>(0, self.track_id),
        }
    }
}

// Convert 'stco' compact table to full table by iterator.
// (start_chunk_num, sample_number) => (start_chunk_num, sample_number),
//                                     (start_chunk_num + 1, sample_number),
//                                     (start_chunk_num + 2, sample_number),
//                                     ...
//                                     (next start_chunk_num, next sample_number),
//                                     ...
//
// For example:
// (1, 5), (5, 10), (9, 2) => (1, 5), (2, 5), (3, 5), (4, 5), (5, 10), (6, 10),
// (7, 10), (8, 10), (9, 2)
struct SampleToChunkIterator<'a> {
    chunks: std::ops::Range<u32>,
    sample_count: u32,
    stsc_peek_iter: std::iter::Peekable<std::slice::Iter<'a, mp4parse::SampleToChunk>>,
    remain_chunk_count: u32, // total chunk number from 'stco'.
}

impl<'a> Iterator for SampleToChunkIterator<'a> {
    type Item = (u32, u32);

    fn next(&mut self) -> Option<(u32, u32)> {
        let has_chunk = self.chunks.next().or_else(|| {
            self.chunks = self.locate();
            self.remain_chunk_count
                .checked_sub(self.chunks.len() as u32)
                .and_then(|res| {
                    self.remain_chunk_count = res;
                    self.chunks.next()
                })
        });

        has_chunk.map(|id| (id, self.sample_count))
    }
}

impl<'a> SampleToChunkIterator<'a> {
    fn locate(&mut self) -> std::ops::Range<u32> {
        loop {
            return match (self.stsc_peek_iter.next(), self.stsc_peek_iter.peek()) {
                (Some(next), Some(peek)) if next.first_chunk == peek.first_chunk => {
                    // Invalid entry, skip it and will continue searching at
                    // next loop iteration.
                    continue;
                }
                (Some(next), Some(peek)) if next.first_chunk > 0 && peek.first_chunk > 0 => {
                    self.sample_count = next.samples_per_chunk;
                    (next.first_chunk - 1)..(peek.first_chunk - 1)
                }
                (Some(next), None) if next.first_chunk > 0 => {
                    self.sample_count = next.samples_per_chunk;
                    // Total chunk number in 'stsc' could be different to 'stco',
                    // there could be more chunks at the last 'stsc' record.
                    match next.first_chunk.checked_add(self.remain_chunk_count) {
                        Some(r) => (next.first_chunk - 1)..r - 1,
                        _ => 0..0,
                    }
                }
                _ => 0..0,
            };
        }
    }
}

fn create_sample_table(track: &Track, track_offset_time: i64) -> Option<TryVec<Mp4parseIndice>> {
    let timescale = match track.timescale {
        Some(ref t) => TrackTimeScale::<i64>(t.0 as i64, t.1),
        _ => return None,
    };

    let (stsc, stco, stsz, stts) = match (&track.stsc, &track.stco, &track.stsz, &track.stts) {
        (&Some(ref a), &Some(ref b), &Some(ref c), &Some(ref d)) => (a, b, c, d),
        _ => return None,
    };

    // According to spec, no sync table means every sample is sync sample.
    let has_sync_table = match track.stss {
        Some(_) => true,
        _ => false,
    };

    let mut sample_table = TryVec::new();
    let mut sample_size_iter = stsz.sample_sizes.iter();

    // Get 'stsc' iterator for (chunk_id, chunk_sample_count) and calculate the sample
    // offset address.
    let stsc_iter = SampleToChunkIterator {
        chunks: (0..0),
        sample_count: 0,
        stsc_peek_iter: stsc.samples.as_slice().iter().peekable(),
        remain_chunk_count: stco.offsets.len() as u32,
    };

    for i in stsc_iter {
        let chunk_id = i.0 as usize;
        let sample_counts = i.1;
        let mut cur_position = match stco.offsets.get(chunk_id) {
            Some(&i) => i,
            _ => return None,
        };
        for _ in 0..sample_counts {
            let start_offset = cur_position;
            let end_offset = match (stsz.sample_size, sample_size_iter.next()) {
                (_, Some(t)) => start_offset + u64::from(*t),
                (t, _) if t > 0 => start_offset + u64::from(t),
                _ => 0,
            };
            if end_offset == 0 {
                return None;
            }
            cur_position = end_offset;

            sample_table
                .push(Mp4parseIndice {
                    start_offset,
                    end_offset,
                    start_composition: 0,
                    end_composition: 0,
                    start_decode: 0,
                    sync: !has_sync_table,
                })
                .ok()?;
        }
    }

    // Mark the sync sample in sample_table according to 'stss'.
    if let Some(ref v) = track.stss {
        for iter in &v.samples {
            match iter
                .checked_sub(1)
                .and_then(|idx| sample_table.get_mut(idx as usize))
            {
                Some(elem) => elem.sync = true,
                _ => return None,
            }
        }
    }

    let ctts_iter = match track.ctts {
        Some(ref v) => Some(v.samples.as_slice().iter()),
        _ => None,
    };

    let mut ctts_offset_iter = TimeOffsetIterator {
        cur_sample_range: (0..0),
        cur_offset: 0,
        ctts_iter,
        track_id: track.id,
    };

    let mut stts_iter = TimeToSampleIterator {
        cur_sample_count: (0..0),
        cur_sample_delta: 0,
        stts_iter: stts.samples.as_slice().iter(),
        track_id: track.id,
    };

    // sum_delta is the sum of stts_iter delta.
    // According to sepc:
    //      decode time => DT(n) = DT(n-1) + STTS(n)
    //      composition time => CT(n) = DT(n) + CTTS(n)
    // Note:
    //      composition time needs to add the track offset time from 'elst' table.
    let mut sum_delta = TrackScaledTime::<i64>(0, track.id);
    for sample in sample_table.as_mut_slice() {
        let decode_time = sum_delta;
        sum_delta = sum_delta + stts_iter.next_delta();

        // ctts_offset is the current sample offset time.
        let ctts_offset = ctts_offset_iter.next_offset_time();

        let start_composition = track_time_to_us(decode_time + ctts_offset, timescale);

        let end_composition = track_time_to_us(sum_delta + ctts_offset, timescale);

        let start_decode = track_time_to_us(decode_time, timescale);

        match (start_composition, end_composition, start_decode) {
            (Some(s_c), Some(e_c), Some(s_d)) => {
                sample.start_composition = s_c + track_offset_time;
                sample.end_composition = e_c + track_offset_time;
                sample.start_decode = s_d;
            }
            _ => return None,
        }
    }

    // Correct composition end time due to 'ctts' causes composition time re-ordering.
    //
    // Composition end time is not in specification. However, gecko needs it, so we need to
    // calculate to correct the composition end time.
    if !sample_table.is_empty() {
        // Create an index table refers to sample_table and sorted by start_composisiton time.
        let mut sort_table = TryVec::new();
        for i in 0..sample_table.len() {
            sort_table.push(i).ok()?;
        }

        sort_table.sort_by_key(|i| match sample_table.get(*i) {
            Some(v) => v.start_composition,
            _ => 0,
        });

        for indices in sort_table.windows(2) {
            if let [current_index, peek_index] = *indices {
                let next_start_composition_time = sample_table[peek_index].start_composition;
                let sample = &mut sample_table[current_index];
                sample.end_composition = next_start_composition_time;
            }
        }
    }

    Some(sample_table)
}

/// Fill the supplied `Mp4parseFragmentInfo` with metadata from fragmented file.
///
/// # Safety
///
/// This function is unsafe because it dereferences the the parser and
/// info raw pointers passed to it. Callers should ensure the parser
/// pointer points to a valid `Mp4parseParser` and that the info pointer points
/// to a valid `Mp4parseFragmentInfo`.

#[no_mangle]
pub unsafe extern "C" fn mp4parse_get_fragment_info(
    parser: *mut Mp4parseParser,
    info: *mut Mp4parseFragmentInfo,
) -> Mp4parseStatus {
    if parser.is_null() || info.is_null() {
        return Mp4parseStatus::BadArg;
    }

    // Initialize fields to default values to ensure all fields are always valid.
    *info = Default::default();

    let context = (*parser).context();
    let info: &mut Mp4parseFragmentInfo = &mut *info;

    info.fragment_duration = 0;

    let duration = match context.mvex {
        Some(ref mvex) => mvex.fragment_duration,
        None => return Mp4parseStatus::Invalid,
    };

    if let (Some(time), Some(scale)) = (duration, context.timescale) {
        info.fragment_duration = match media_time_to_us(time, scale) {
            Some(time_us) => time_us as u64,
            None => return Mp4parseStatus::Invalid,
        }
    }

    Mp4parseStatus::Ok
}

/// Determine if an mp4 file is fragmented. A fragmented file needs mvex table
/// and contains no data in stts, stsc, and stco boxes.
///
/// # Safety
///
/// This function is unsafe because it dereferences the the parser and
/// fragmented raw pointers passed to it. Callers should ensure the parser
/// pointer points to a valid `Mp4parseParser` and that the fragmented pointer
/// points to an appropriate memory location to have a `u8` written to.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_is_fragmented(
    parser: *mut Mp4parseParser,
    track_id: u32,
    fragmented: *mut u8,
) -> Mp4parseStatus {
    if parser.is_null() {
        return Mp4parseStatus::BadArg;
    }

    let context = (*parser).context_mut();
    let tracks = &context.tracks;
    (*fragmented) = false as u8;

    if context.mvex.is_none() {
        return Mp4parseStatus::Ok;
    }

    // check sample tables.
    let mut iter = tracks.iter();
    iter.find(|track| track.track_id == Some(track_id))
        .map_or(Mp4parseStatus::BadArg, |track| {
            match (&track.stsc, &track.stco, &track.stts) {
                (&Some(ref stsc), &Some(ref stco), &Some(ref stts))
                    if stsc.samples.is_empty()
                        && stco.offsets.is_empty()
                        && stts.samples.is_empty() =>
                {
                    (*fragmented) = true as u8
                }
                _ => {}
            };
            Mp4parseStatus::Ok
        })
}

/// Get 'pssh' system id and 'pssh' box content for eme playback.
///
/// The data format of the `info` struct passed to gecko is:
///
/// - system id (16 byte uuid)
/// - pssh box size (32-bit native endian)
/// - pssh box content (including header)
///
/// # Safety
///
/// This function is unsafe because it dereferences the the parser and
/// info raw pointers passed to it. Callers should ensure the parser
/// pointer points to a valid `Mp4parseParser` and that the fragmented pointer
/// points to a valid `Mp4parsePsshInfo`.
#[no_mangle]
pub unsafe extern "C" fn mp4parse_get_pssh_info(
    parser: *mut Mp4parseParser,
    info: *mut Mp4parsePsshInfo,
) -> Mp4parseStatus {
    if parser.is_null() || info.is_null() {
        return Mp4parseStatus::BadArg;
    }

    // Initialize fields to default values to ensure all fields are always valid.
    *info = Default::default();

    get_pssh_info(&mut *parser, &mut *info).into()
}

fn get_pssh_info(
    parser: &mut Mp4parseParser,
    info: &mut Mp4parsePsshInfo,
) -> Result<(), Mp4parseStatus> {
    let Mp4parseParser {
        context, pssh_data, ..
    } = parser;

    pssh_data.clear();
    for pssh in &context.psshs {
        let content_len = pssh.box_content.len();
        if content_len > std::u32::MAX as usize {
            return Err(Mp4parseStatus::Invalid);
        }
        let mut data_len = TryVec::new();
        if data_len
            .write_u32::<byteorder::NativeEndian>(content_len as u32)
            .is_err()
        {
            return Err(Mp4parseStatus::Io);
        }
        pssh_data.extend_from_slice(pssh.system_id.as_slice())?;
        pssh_data.extend_from_slice(data_len.as_slice())?;
        pssh_data.extend_from_slice(pssh.box_content.as_slice())?;
    }

    info.data.set_data(pssh_data);

    Ok(())
}

#[cfg(test)]
extern "C" fn error_read(_: *mut u8, _: usize, _: *mut std::os::raw::c_void) -> isize {
    -1
}

#[cfg(test)]
extern "C" fn valid_read(buf: *mut u8, size: usize, userdata: *mut std::os::raw::c_void) -> isize {
    let input: &mut std::fs::File = unsafe { &mut *(userdata as *mut _) };

    let mut buf = unsafe { std::slice::from_raw_parts_mut(buf, size) };
    match input.read(&mut buf) {
        Ok(n) => n as isize,
        Err(_) => -1,
    }
}

#[test]
fn get_track_count_null_parser() {
    unsafe {
        let mut count: u32 = 0;
        let rv = mp4parse_get_track_count(std::ptr::null(), std::ptr::null_mut());
        assert_eq!(rv, Mp4parseStatus::BadArg);
        let rv = mp4parse_get_track_count(std::ptr::null(), &mut count);
        assert_eq!(rv, Mp4parseStatus::BadArg);
    }
}

#[test]
fn arg_validation() {
    unsafe {
        let rv = mp4parse_new(std::ptr::null(), std::ptr::null_mut());
        assert_eq!(rv, Mp4parseStatus::BadArg);

        // Passing a null Mp4parseIo is an error.
        let mut parser = std::ptr::null_mut();
        let rv = mp4parse_new(std::ptr::null(), &mut parser);
        assert_eq!(rv, Mp4parseStatus::BadArg);
        assert!(parser.is_null());

        let null_mut: *mut std::os::raw::c_void = std::ptr::null_mut();

        // Passing an Mp4parseIo with null members is an error.
        let io = Mp4parseIo {
            read: None,
            userdata: null_mut,
        };
        let mut parser = std::ptr::null_mut();
        let rv = mp4parse_new(&io, &mut parser);
        assert_eq!(rv, Mp4parseStatus::BadArg);
        assert!(parser.is_null());

        let mut dummy_value = 42;
        let io = Mp4parseIo {
            read: None,
            userdata: &mut dummy_value as *mut _ as *mut std::os::raw::c_void,
        };
        let mut parser = std::ptr::null_mut();
        let rv = mp4parse_new(&io, &mut parser);
        assert_eq!(rv, Mp4parseStatus::BadArg);
        assert!(parser.is_null());

        let mut dummy_info = Mp4parseTrackInfo {
            track_type: Mp4parseTrackType::Video,
            track_id: 0,
            duration: 0,
            media_time: 0,
        };
        assert_eq!(
            Mp4parseStatus::BadArg,
            mp4parse_get_track_info(std::ptr::null_mut(), 0, &mut dummy_info)
        );

        let mut dummy_video = Mp4parseTrackVideoInfo {
            display_width: 0,
            display_height: 0,
            rotation: 0,
            sample_info_count: 0,
            sample_info: std::ptr::null(),
        };
        assert_eq!(
            Mp4parseStatus::BadArg,
            mp4parse_get_track_video_info(std::ptr::null_mut(), 0, &mut dummy_video)
        );

        let mut dummy_audio = Default::default();
        assert_eq!(
            Mp4parseStatus::BadArg,
            mp4parse_get_track_audio_info(std::ptr::null_mut(), 0, &mut dummy_audio)
        );
    }
}

#[test]
fn parser_input_must_be_null() {
    let mut dummy_value = 42;
    let io = Mp4parseIo {
        read: Some(error_read),
        userdata: &mut dummy_value as *mut _ as *mut std::os::raw::c_void,
    };
    let mut parser = 0xDEAD_BEEF as *mut _;
    let rv = unsafe { mp4parse_new(&io, &mut parser) };
    assert_eq!(rv, Mp4parseStatus::BadArg);
}

#[test]
fn arg_validation_with_parser() {
    unsafe {
        let mut dummy_value = 42;
        let io = Mp4parseIo {
            read: Some(error_read),
            userdata: &mut dummy_value as *mut _ as *mut std::os::raw::c_void,
        };
        let mut parser = std::ptr::null_mut();
        let rv = mp4parse_new(&io, &mut parser);
        assert_eq!(rv, Mp4parseStatus::Io);
        assert!(parser.is_null());

        // Null info pointers are an error.
        assert_eq!(
            Mp4parseStatus::BadArg,
            mp4parse_get_track_info(parser, 0, std::ptr::null_mut())
        );
        assert_eq!(
            Mp4parseStatus::BadArg,
            mp4parse_get_track_video_info(parser, 0, std::ptr::null_mut())
        );
        assert_eq!(
            Mp4parseStatus::BadArg,
            mp4parse_get_track_audio_info(parser, 0, std::ptr::null_mut())
        );

        let mut dummy_info = Mp4parseTrackInfo {
            track_type: Mp4parseTrackType::Video,
            track_id: 0,
            duration: 0,
            media_time: 0,
        };
        assert_eq!(
            Mp4parseStatus::BadArg,
            mp4parse_get_track_info(parser, 0, &mut dummy_info)
        );

        let mut dummy_video = Mp4parseTrackVideoInfo {
            display_width: 0,
            display_height: 0,
            rotation: 0,
            sample_info_count: 0,
            sample_info: std::ptr::null(),
        };
        assert_eq!(
            Mp4parseStatus::BadArg,
            mp4parse_get_track_video_info(parser, 0, &mut dummy_video)
        );

        let mut dummy_audio = Default::default();
        assert_eq!(
            Mp4parseStatus::BadArg,
            mp4parse_get_track_audio_info(parser, 0, &mut dummy_audio)
        );
    }
}

#[cfg(test)]
fn parse_minimal_mp4() -> *mut Mp4parseParser {
    let mut file = std::fs::File::open("../mp4parse/tests/minimal.mp4").unwrap();
    let io = Mp4parseIo {
        read: Some(valid_read),
        userdata: &mut file as *mut _ as *mut std::os::raw::c_void,
    };
    let mut parser = std::ptr::null_mut();
    let rv = unsafe { mp4parse_new(&io, &mut parser) };
    assert_eq!(Mp4parseStatus::Ok, rv);
    parser
}

#[test]
fn minimal_mp4_parse_ok() {
    let parser = parse_minimal_mp4();

    assert!(!parser.is_null());

    unsafe {
        mp4parse_free(parser);
    }
}

#[test]
fn minimal_mp4_get_track_cout() {
    let parser = parse_minimal_mp4();

    let mut count: u32 = 0;
    assert_eq!(Mp4parseStatus::Ok, unsafe {
        mp4parse_get_track_count(parser, &mut count)
    });
    assert_eq!(2, count);

    unsafe {
        mp4parse_free(parser);
    }
}

#[test]
fn minimal_mp4_get_track_info() {
    let parser = parse_minimal_mp4();

    let mut info = Mp4parseTrackInfo {
        track_type: Mp4parseTrackType::Video,
        track_id: 0,
        duration: 0,
        media_time: 0,
    };
    assert_eq!(Mp4parseStatus::Ok, unsafe {
        mp4parse_get_track_info(parser, 0, &mut info)
    });
    assert_eq!(info.track_type, Mp4parseTrackType::Video);
    assert_eq!(info.track_id, 1);
    assert_eq!(info.duration, 40000);
    assert_eq!(info.media_time, 0);

    assert_eq!(Mp4parseStatus::Ok, unsafe {
        mp4parse_get_track_info(parser, 1, &mut info)
    });
    assert_eq!(info.track_type, Mp4parseTrackType::Audio);
    assert_eq!(info.track_id, 2);
    assert_eq!(info.duration, 61333);
    assert_eq!(info.media_time, 21333);

    unsafe {
        mp4parse_free(parser);
    }
}

#[test]
fn minimal_mp4_get_track_video_info() {
    let parser = parse_minimal_mp4();

    let mut video = Mp4parseTrackVideoInfo::default();
    assert_eq!(Mp4parseStatus::Ok, unsafe {
        mp4parse_get_track_video_info(parser, 0, &mut video)
    });
    assert_eq!(video.display_width, 320);
    assert_eq!(video.display_height, 240);
    assert_eq!(video.sample_info_count, 1);

    unsafe {
        assert_eq!((*video.sample_info).image_width, 320);
        assert_eq!((*video.sample_info).image_height, 240);
    }

    unsafe {
        mp4parse_free(parser);
    }
}

#[test]
fn minimal_mp4_get_track_audio_info() {
    let parser = parse_minimal_mp4();

    let mut audio = Mp4parseTrackAudioInfo::default();
    assert_eq!(Mp4parseStatus::Ok, unsafe {
        mp4parse_get_track_audio_info(parser, 1, &mut audio)
    });
    assert_eq!(audio.sample_info_count, 1);

    unsafe {
        assert_eq!((*audio.sample_info).channels, 1);
        assert_eq!((*audio.sample_info).bit_depth, 16);
        assert_eq!((*audio.sample_info).sample_rate, 48000);
    }

    unsafe {
        mp4parse_free(parser);
    }
}

#[test]
fn minimal_mp4_get_track_info_invalid_track_number() {
    let parser = parse_minimal_mp4();

    let mut info = Mp4parseTrackInfo {
        track_type: Mp4parseTrackType::Video,
        track_id: 0,
        duration: 0,
        media_time: 0,
    };
    assert_eq!(Mp4parseStatus::BadArg, unsafe {
        mp4parse_get_track_info(parser, 3, &mut info)
    });
    assert_eq!(info.track_type, Mp4parseTrackType::Video);
    assert_eq!(info.track_id, 0);
    assert_eq!(info.duration, 0);
    assert_eq!(info.media_time, 0);

    let mut video = Mp4parseTrackVideoInfo::default();
    assert_eq!(Mp4parseStatus::BadArg, unsafe {
        mp4parse_get_track_video_info(parser, 3, &mut video)
    });
    assert_eq!(video.display_width, 0);
    assert_eq!(video.display_height, 0);
    assert_eq!(video.sample_info_count, 0);

    let mut audio = Default::default();
    assert_eq!(Mp4parseStatus::BadArg, unsafe {
        mp4parse_get_track_audio_info(parser, 3, &mut audio)
    });
    assert_eq!(audio.sample_info_count, 0);

    unsafe {
        mp4parse_free(parser);
    }
}

#[test]
fn rational_scale_overflow() {
    assert_eq!(rational_scale::<u64, u64>(17, 3, 1000), Some(5666));
    let large = 0x4000_0000_0000_0000;
    assert_eq!(rational_scale::<u64, u64>(large, 2, 2), Some(large));
    assert_eq!(rational_scale::<u64, u64>(large, 4, 4), Some(large));
    assert_eq!(rational_scale::<u64, u64>(large, 2, 8), None);
    assert_eq!(rational_scale::<u64, u64>(large, 8, 4), Some(large / 2));
    assert_eq!(rational_scale::<u64, u64>(large + 1, 4, 4), Some(large + 1));
    assert_eq!(rational_scale::<u64, u64>(large, 40, 1000), None);
}

#[test]
fn media_time_overflow() {
    let scale = MediaTimeScale(90000);
    let duration = MediaScaledTime(9_007_199_254_710_000);
    assert_eq!(
        media_time_to_us(duration, scale),
        Some(100_079_991_719_000_000)
    );
}

#[test]
fn track_time_overflow() {
    let scale = TrackTimeScale(44100u64, 0);
    let duration = TrackScaledTime(4_413_527_634_807_900u64, 0);
    assert_eq!(
        track_time_to_us(duration, scale),
        Some(100_079_991_719_000_000)
    );
}
