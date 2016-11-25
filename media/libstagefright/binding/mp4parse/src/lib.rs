//! Module for parsing ISO Base Media Format aka video/mp4 streams.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#![cfg_attr(feature = "fuzz", feature(plugin))]
#![cfg_attr(feature = "fuzz", plugin(afl_plugin))]
#[cfg(feature = "fuzz")]
extern crate afl;

extern crate byteorder;
use byteorder::{ReadBytesExt, WriteBytesExt};
use std::io::{Read, Take};
use std::io::Cursor;
use std::cmp;

mod boxes;
use boxes::BoxType;

// Unit tests.
#[cfg(test)]
mod tests;

// Arbitrary buffer size limit used for raw read_bufs on a box.
const BUF_SIZE_LIMIT: u64 = 1024 * 1024;

static DEBUG_MODE: std::sync::atomic::AtomicBool = std::sync::atomic::ATOMIC_BOOL_INIT;

pub fn set_debug_mode(mode: bool) {
    DEBUG_MODE.store(mode, std::sync::atomic::Ordering::SeqCst);
}

#[inline(always)]
fn get_debug_mode() -> bool {
    DEBUG_MODE.load(std::sync::atomic::Ordering::Relaxed)
}

macro_rules! log {
    ($($args:tt)*) => (
        if get_debug_mode() {
            println!( $( $args )* );
        }
    )
}

/// Describes parser failures.
///
/// This enum wraps the standard `io::Error` type, unified with
/// our own parser error states and those of crates we use.
#[derive(Debug)]
pub enum Error {
    /// Parse error caused by corrupt or malformed data.
    InvalidData(&'static str),
    /// Parse error caused by limited parser support rather than invalid data.
    Unsupported(&'static str),
    /// Reflect `std::io::ErrorKind::UnexpectedEof` for short data.
    UnexpectedEOF,
    /// Propagate underlying errors from `std::io`.
    Io(std::io::Error),
    /// read_mp4 terminated without detecting a moov box.
    NoMoov,
}

impl From<std::io::Error> for Error {
    fn from(err: std::io::Error) -> Error {
        match err.kind() {
            std::io::ErrorKind::UnexpectedEof => Error::UnexpectedEOF,
            _ => Error::Io(err),
        }
    }
}

impl From<std::string::FromUtf8Error> for Error {
    fn from(_: std::string::FromUtf8Error) -> Error {
        Error::InvalidData("invalid utf8")
    }
}

/// Result shorthand using our Error enum.
pub type Result<T> = std::result::Result<T, Error>;

/// Basic ISO box structure.
///
/// mp4 files are a sequence of possibly-nested 'box' structures.  Each box
/// begins with a header describing the length of the box's data and a
/// four-byte box type which identifies the type of the box. Together these
/// are enough to interpret the contents of that section of the file.
#[derive(Debug, Clone, Copy)]
struct BoxHeader {
    /// Box type.
    name: BoxType,
    /// Size of the box in bytes.
    size: u64,
    /// Offset to the start of the contained data (or header size).
    offset: u64,
}

/// File type box 'ftyp'.
#[derive(Debug)]
struct FileTypeBox {
    major_brand: u32,
    minor_version: u32,
    compatible_brands: Vec<u32>,
}

/// Movie header box 'mvhd'.
#[derive(Debug)]
struct MovieHeaderBox {
    pub timescale: u32,
    duration: u64,
}

/// Track header box 'tkhd'
#[derive(Debug, Clone)]
pub struct TrackHeaderBox {
    track_id: u32,
    pub disabled: bool,
    pub duration: u64,
    pub width: u32,
    pub height: u32,
}

/// Edit list box 'elst'
#[derive(Debug)]
struct EditListBox {
    edits: Vec<Edit>,
}

#[derive(Debug)]
struct Edit {
    segment_duration: u64,
    media_time: i64,
    media_rate_integer: i16,
    media_rate_fraction: i16,
}

/// Media header box 'mdhd'
#[derive(Debug)]
struct MediaHeaderBox {
    timescale: u32,
    duration: u64,
}

// Chunk offset box 'stco' or 'co64'
#[derive(Debug)]
struct ChunkOffsetBox {
    offsets: Vec<u64>,
}

// Sync sample box 'stss'
#[derive(Debug)]
struct SyncSampleBox {
    samples: Vec<u32>,
}

// Sample to chunk box 'stsc'
#[derive(Debug)]
struct SampleToChunkBox {
    samples: Vec<SampleToChunk>,
}

#[derive(Debug)]
struct SampleToChunk {
    first_chunk: u32,
    samples_per_chunk: u32,
    sample_description_index: u32,
}

// Sample size box 'stsz'
#[derive(Debug)]
struct SampleSizeBox {
    sample_size: u32,
    sample_sizes: Vec<u32>,
}

// Time to sample box 'stts'
#[derive(Debug)]
struct TimeToSampleBox {
    samples: Vec<Sample>,
}

#[derive(Debug)]
struct Sample {
    sample_count: u32,
    sample_delta: u32,
}

// Handler reference box 'hdlr'
#[derive(Debug)]
struct HandlerBox {
    handler_type: u32,
}

// Sample description box 'stsd'
#[derive(Debug)]
struct SampleDescriptionBox {
    descriptions: Vec<SampleEntry>,
}

#[derive(Debug, Clone)]
pub enum SampleEntry {
    Audio(AudioSampleEntry),
    Video(VideoSampleEntry),
    Unknown,
}

#[allow(non_camel_case_types)]
#[derive(Debug, Clone, Default)]
pub struct ES_Descriptor {
    pub audio_codec: CodecType,
    pub audio_sample_rate: Option<u32>,
    pub audio_channel_count: Option<u16>,
    pub codec_esds: Vec<u8>,
}

#[allow(non_camel_case_types)]
#[derive(Debug, Clone)]
pub enum AudioCodecSpecific {
    ES_Descriptor(ES_Descriptor),
    FLACSpecificBox(FLACSpecificBox),
    OpusSpecificBox(OpusSpecificBox),
}

#[derive(Debug, Clone)]
pub struct AudioSampleEntry {
    data_reference_index: u16,
    pub channelcount: u16,
    pub samplesize: u16,
    pub samplerate: u32,
    pub codec_specific: AudioCodecSpecific,
}

#[derive(Debug, Clone)]
pub enum VideoCodecSpecific {
    AVCConfig(Vec<u8>),
    VPxConfig(VPxConfigBox),
}

#[derive(Debug, Clone)]
pub struct VideoSampleEntry {
    data_reference_index: u16,
    pub width: u16,
    pub height: u16,
    pub codec_specific: VideoCodecSpecific,
}

/// Represent a Video Partition Codec Configuration 'vpcC' box (aka vp9).
#[derive(Debug, Clone)]
pub struct VPxConfigBox {
    profile: u8,
    level: u8,
    pub bit_depth: u8,
    pub color_space: u8, // Really an enum
    pub chroma_subsampling: u8,
    transfer_function: u8,
    video_full_range: bool,
    pub codec_init: Vec<u8>, // Empty for vp8/vp9.
}

#[derive(Debug, Clone)]
pub struct FLACMetadataBlock {
    pub block_type: u8,
    pub data: Vec<u8>,
}

/// Represet a FLACSpecificBox 'dfLa'
#[derive(Debug, Clone)]
pub struct FLACSpecificBox {
    version: u8,
    pub blocks: Vec<FLACMetadataBlock>,
}

#[derive(Debug, Clone)]
struct ChannelMappingTable {
    stream_count: u8,
    coupled_count: u8,
    channel_mapping: Vec<u8>,
}

/// Represent an OpusSpecificBox 'dOps'
#[derive(Debug, Clone)]
pub struct OpusSpecificBox {
    pub version: u8,
    output_channel_count: u8,
    pre_skip: u16,
    input_sample_rate: u32,
    output_gain: i16,
    channel_mapping_family: u8,
    channel_mapping_table: Option<ChannelMappingTable>,
}

#[derive(Debug)]
pub struct MovieExtendsBox {
    pub fragment_duration: Option<MediaScaledTime>,
}

pub type ByteData = Vec<u8>;

#[derive(Debug, Default)]
pub struct ProtectionSystemSpecificHeaderBox {
    pub system_id: ByteData,
    pub kid: Vec<ByteData>,
    pub data: ByteData,

    // The entire pssh box (include header) required by Gecko.
    pub box_content: ByteData,
}

/// Internal data structures.
#[derive(Debug, Default)]
pub struct MediaContext {
    pub timescale: Option<MediaTimeScale>,
    /// Tracks found in the file.
    pub tracks: Vec<Track>,
    pub mvex: Option<MovieExtendsBox>,
    pub psshs: Vec<ProtectionSystemSpecificHeaderBox>
}

impl MediaContext {
    pub fn new() -> MediaContext {
        Default::default()
    }
}

#[derive(Debug, PartialEq)]
pub enum TrackType {
    Audio,
    Video,
    Unknown,
}

impl Default for TrackType {
    fn default() -> Self { TrackType::Unknown }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum CodecType {
    Unknown,
    MP3,
    AAC,
    FLAC,
    Opus,
    H264,
    VP9,
    VP8,
    EncryptedVideo,
    EncryptedAudio,
}

impl Default for CodecType {
    fn default() -> Self { CodecType::Unknown }
}

/// The media's global (mvhd) timescale in units per second.
#[derive(Debug, Copy, Clone, PartialEq)]
pub struct MediaTimeScale(pub u64);

/// A time to be scaled by the media's global (mvhd) timescale.
#[derive(Debug, Copy, Clone, PartialEq)]
pub struct MediaScaledTime(pub u64);

/// The track's local (mdhd) timescale.
/// Members are timescale units per second and the track id.
#[derive(Debug, Copy, Clone, PartialEq)]
pub struct TrackTimeScale(pub u64, pub usize);

/// A time to be scaled by the track's local (mdhd) timescale.
/// Members are time in scale units and the track id.
#[derive(Debug, Copy, Clone, PartialEq)]
pub struct TrackScaledTime(pub u64, pub usize);

/// A fragmented file contains no sample data in stts, stsc, and stco.
#[derive(Debug, Default)]
pub struct EmptySampleTableBoxes {
    pub empty_stts : bool,
    pub empty_stsc : bool,
    pub empty_stco : bool,
}

/// Check boxes contain data.
impl EmptySampleTableBoxes {
    pub fn all_empty(&self) -> bool {
        self.empty_stts & self.empty_stsc & self.empty_stco
    }
}

#[derive(Debug, Default)]
pub struct Track {
    id: usize,
    pub track_type: TrackType,
    pub empty_duration: Option<MediaScaledTime>,
    pub media_time: Option<TrackScaledTime>,
    pub timescale: Option<TrackTimeScale>,
    pub duration: Option<TrackScaledTime>,
    pub track_id: Option<u32>,
    pub codec_type: CodecType,
    pub empty_sample_boxes: EmptySampleTableBoxes,
    pub data: Option<SampleEntry>,
    pub tkhd: Option<TrackHeaderBox>, // TODO(kinetik): find a nicer way to export this.
}

impl Track {
    fn new(id: usize) -> Track {
        Track { id: id, ..Default::default() }
    }
}

struct BMFFBox<'a, T: 'a + Read> {
    head: BoxHeader,
    content: Take<&'a mut T>,
}

struct BoxIter<'a, T: 'a + Read> {
    src: &'a mut T,
}

impl<'a, T: Read> BoxIter<'a, T> {
    fn new(src: &mut T) -> BoxIter<T> {
        BoxIter { src: src }
    }

    fn next_box(&mut self) -> Result<Option<BMFFBox<T>>> {
        let r = read_box_header(self.src);
        match r {
            Ok(h) => Ok(Some(BMFFBox {
                head: h,
                content: self.src.take(h.size - h.offset),
            })),
            Err(Error::UnexpectedEOF) => Ok(None),
            Err(e) => Err(e),
        }
    }
}

impl<'a, T: Read> Read for BMFFBox<'a, T> {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        self.content.read(buf)
    }
}

impl<'a, T: Read> BMFFBox<'a, T> {
    fn bytes_left(&self) -> usize {
        self.content.limit() as usize
    }

    fn get_header(&self) -> &BoxHeader {
        &self.head
    }

    fn box_iter<'b>(&'b mut self) -> BoxIter<BMFFBox<'a, T>> {
        BoxIter::new(self)
    }
}

/// Read and parse a box header.
///
/// Call this first to determine the type of a particular mp4 box
/// and its length. Used internally for dispatching to specific
/// parsers for the internal content, or to get the length to
/// skip unknown or uninteresting boxes.
fn read_box_header<T: ReadBytesExt>(src: &mut T) -> Result<BoxHeader> {
    let size32 = be_u32(src)?;
    let name = BoxType::from(be_u32(src)?);
    let size = match size32 {
        // valid only for top-level box and indicates it's the last box in the file.  usually mdat.
        0 => return Err(Error::Unsupported("unknown sized box")),
        1 => {
            let size64 = be_u64(src)?;
            if size64 < 16 {
                return Err(Error::InvalidData("malformed wide size"));
            }
            size64
        }
        2...7 => return Err(Error::InvalidData("malformed size")),
        _ => size32 as u64,
    };
    let offset = match size32 {
        1 => 4 + 4 + 8,
        _ => 4 + 4,
    };
    assert!(offset <= size);
    Ok(BoxHeader {
        name: name,
        size: size,
        offset: offset,
    })
}

/// Parse the extra header fields for a full box.
fn read_fullbox_extra<T: ReadBytesExt>(src: &mut T) -> Result<(u8, u32)> {
    let version = src.read_u8()?;
    let flags_a = src.read_u8()?;
    let flags_b = src.read_u8()?;
    let flags_c = src.read_u8()?;
    Ok((version,
        (flags_a as u32) << 16 | (flags_b as u32) << 8 | (flags_c as u32)))
}

/// Skip over the entire contents of a box.
fn skip_box_content<T: Read>(src: &mut BMFFBox<T>) -> Result<()> {
    // Skip the contents of unknown chunks.
    let to_skip = {
        let header = src.get_header();
        log!("{:?} (skipped)", header);
        (header.size - header.offset) as usize
    };
    assert_eq!(to_skip, src.bytes_left());
    skip(src, to_skip)
}

/// Skip over the remain data of a box.
fn skip_box_remain<T: Read>(src: &mut BMFFBox<T>) -> Result<()> {
    let remain = {
        let header = src.get_header();
        let len = src.bytes_left();
        log!("remain {} (skipped) in {:?}", len, header);
        len
    };
    skip(src, remain)
}

macro_rules! check_parser_state {
    ( $src:expr ) => {
        if $src.limit() > 0 {
            log!("bad parser state: {} content bytes left", $src.limit());
            return Err(Error::InvalidData("unread box content or bad parser sync"));
        }
    }
}

/// Read the contents of a box, including sub boxes.
///
/// Metadata is accumulated in the passed-through `MediaContext` struct,
/// which can be examined later.
pub fn read_mp4<T: Read>(f: &mut T, context: &mut MediaContext) -> Result<()> {
    let mut found_ftyp = false;
    let mut found_moov = false;
    // TODO(kinetik): Top-level parsing should handle zero-sized boxes
    // rather than throwing an error.
    let mut iter = BoxIter::new(f);
    while let Some(mut b) = iter.next_box()? {
        // box ordering: ftyp before any variable length box (inc. moov),
        // but may not be first box in file if file signatures etc. present
        // fragmented mp4 order: ftyp, moov, pairs of moof/mdat (1-multiple), mfra

        // "special": uuid, wide (= 8 bytes)
        // isom: moov, mdat, free, skip, udta, ftyp, moof, mfra
        // iso2: pdin, meta
        // iso3: meco
        // iso5: styp, sidx, ssix, prft
        // unknown, maybe: id32

        // qt: pnot

        // possibly allow anything where all printable and/or all lowercase printable
        // "four printable characters from the ISO 8859-1 character set"
        match b.head.name {
            BoxType::FileTypeBox => {
                let ftyp = read_ftyp(&mut b)?;
                found_ftyp = true;
                log!("{:?}", ftyp);
            }
            BoxType::MovieBox => {
                read_moov(&mut b, context)?;
                found_moov = true;
            }
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
        if found_moov {
            log!("found moov {}, could stop pure 'moov' parser now", if found_ftyp {
                "and ftyp"
            } else {
                "but no ftyp"
            });
        }
    }

    // XXX(kinetik): This isn't perfect, as a "moov" with no contents is
    // treated as okay but we haven't found anything useful.  Needs more
    // thought for clearer behaviour here.
    if found_moov {
        Ok(())
    } else {
        Err(Error::NoMoov)
    }
}

fn parse_mvhd<T: Read>(f: &mut BMFFBox<T>) -> Result<(MovieHeaderBox, Option<MediaTimeScale>)> {
    let mvhd = read_mvhd(f)?;
    if mvhd.timescale == 0 {
        return Err(Error::InvalidData("zero timescale in mdhd"));
    }
    let timescale = Some(MediaTimeScale(mvhd.timescale as u64));
    Ok((mvhd, timescale))
}

fn read_moov<T: Read>(f: &mut BMFFBox<T>, context: &mut MediaContext) -> Result<()> {
    let mut iter = f.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::MovieHeaderBox => {
                let (mvhd, timescale) = parse_mvhd(&mut b)?;
                context.timescale = timescale;
                log!("{:?}", mvhd);
            }
            BoxType::TrackBox => {
                let mut track = Track::new(context.tracks.len());
                read_trak(&mut b, &mut track)?;
                context.tracks.push(track);
            }
            BoxType::MovieExtendsBox => {
                let mvex = read_mvex(&mut b)?;
                log!("{:?}", mvex);
                context.mvex = Some(mvex);
            }
            BoxType::ProtectionSystemSpecificHeaderBox => {
                let pssh = read_pssh(&mut b)?;
                log!("{:?}", pssh);
                context.psshs.push(pssh);
            }
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(())
}

fn read_pssh<T: Read>(src: &mut BMFFBox<T>) -> Result<ProtectionSystemSpecificHeaderBox> {
    let mut box_content = Vec::with_capacity(src.head.size as usize);
    src.read_to_end(&mut box_content)?;

    let (system_id, kid, data) = {
        let pssh = &mut Cursor::new(box_content.as_slice());

        let (version, _) = read_fullbox_extra(pssh)?;

        let system_id = read_buf(pssh, 16)?;

        let mut kid: Vec<ByteData> = Vec::new();
        if version > 0 {
            let count = be_i32(pssh)?;
            for _ in 0..count {
                let item = read_buf(pssh, 16)?;
                kid.push(item);
            }
        }

        let data_size = be_i32(pssh)? as usize;
        let data = read_buf(pssh, data_size)?;

        (system_id, kid, data)
    };

    let mut pssh_box = Vec::new();
    write_be_u32(&mut pssh_box, src.head.size as u32)?;
    pssh_box.append(&mut b"pssh".to_vec());
    pssh_box.append(&mut box_content);

    Ok(ProtectionSystemSpecificHeaderBox {
        system_id: system_id,
        kid: kid,
        data: data,
        box_content: pssh_box,
    })
}

fn read_mvex<T: Read>(src: &mut BMFFBox<T>) -> Result<MovieExtendsBox> {
    let mut iter = src.box_iter();
    let mut fragment_duration = None;
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::MovieExtendsHeaderBox => {
                let duration = read_mehd(&mut b)?;
                fragment_duration = Some(duration);
            },
            _ => skip_box_content(&mut b)?,
        }
    }
    Ok(MovieExtendsBox {
        fragment_duration: fragment_duration,
    })
}

fn read_mehd<T: Read>(src: &mut BMFFBox<T>) -> Result<MediaScaledTime> {
    let (version, _) = read_fullbox_extra(src)?;
    let fragment_duration = match version {
        1 => be_u64(src)?,
        0 => be_u32(src)? as u64,
        _ => return Err(Error::InvalidData("unhandled mehd version")),
    };
    Ok(MediaScaledTime(fragment_duration))
}

fn read_trak<T: Read>(f: &mut BMFFBox<T>, track: &mut Track) -> Result<()> {
    let mut iter = f.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::TrackHeaderBox => {
                let tkhd = read_tkhd(&mut b)?;
                track.track_id = Some(tkhd.track_id);
                track.tkhd = Some(tkhd.clone());
                log!("{:?}", tkhd);
            }
            BoxType::EditBox => read_edts(&mut b, track)?,
            BoxType::MediaBox => read_mdia(&mut b, track)?,
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(())
}

fn read_edts<T: Read>(f: &mut BMFFBox<T>, track: &mut Track) -> Result<()> {
    let mut iter = f.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::EditListBox => {
                let elst = read_elst(&mut b)?;
                let mut empty_duration = 0;
                let mut idx = 0;
                if elst.edits.len() > 2 {
                    return Err(Error::Unsupported("more than two edits"));
                }
                if elst.edits[idx].media_time == -1 {
                    empty_duration = elst.edits[idx].segment_duration;
                    if elst.edits.len() < 2 {
                        return Err(Error::InvalidData("expected additional edit"));
                    }
                    idx += 1;
                }
                track.empty_duration = Some(MediaScaledTime(empty_duration));
                if elst.edits[idx].media_time < 0 {
                    return Err(Error::InvalidData("unexpected negative media time in edit"));
                }
                track.media_time = Some(TrackScaledTime(elst.edits[idx].media_time as u64,
                                                        track.id));
                log!("{:?}", elst);
            }
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(())
}

fn parse_mdhd<T: Read>(f: &mut BMFFBox<T>, track: &mut Track) -> Result<(MediaHeaderBox, Option<TrackScaledTime>, Option<TrackTimeScale>)> {
    let mdhd = read_mdhd(f)?;
    let duration = match mdhd.duration {
        std::u64::MAX => None,
        duration => Some(TrackScaledTime(duration, track.id)),
    };
    if mdhd.timescale == 0 {
        return Err(Error::InvalidData("zero timescale in mdhd"));
    }
    let timescale = Some(TrackTimeScale(mdhd.timescale as u64, track.id));
    Ok((mdhd, duration, timescale))
}

fn read_mdia<T: Read>(f: &mut BMFFBox<T>, track: &mut Track) -> Result<()> {
    let mut iter = f.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::MediaHeaderBox => {
                let (mdhd, duration, timescale) = parse_mdhd(&mut b, track)?;
                track.duration = duration;
                track.timescale = timescale;
                log!("{:?}", mdhd);
            }
            BoxType::HandlerBox => {
                let hdlr = read_hdlr(&mut b)?;
                match hdlr.handler_type {
                    0x76696465 /* 'vide' */ => track.track_type = TrackType::Video,
                    0x736f756e /* 'soun' */ => track.track_type = TrackType::Audio,
                    _ => (),
                }
                log!("{:?}", hdlr);
            }
            BoxType::MediaInformationBox => read_minf(&mut b, track)?,
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(())
}

fn read_minf<T: Read>(f: &mut BMFFBox<T>, track: &mut Track) -> Result<()> {
    let mut iter = f.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::SampleTableBox => read_stbl(&mut b, track)?,
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(())
}

fn read_stbl<T: Read>(f: &mut BMFFBox<T>, track: &mut Track) -> Result<()> {
    let mut iter = f.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::SampleDescriptionBox => {
                let stsd = read_stsd(&mut b, track)?;
                log!("{:?}", stsd);
            }
            BoxType::TimeToSampleBox => {
                let stts = read_stts(&mut b)?;
                track.empty_sample_boxes.empty_stts = stts.samples.is_empty();
                log!("{:?}", stts);
            }
            BoxType::SampleToChunkBox => {
                let stsc = read_stsc(&mut b)?;
                track.empty_sample_boxes.empty_stsc = stsc.samples.is_empty();
                log!("{:?}", stsc);
            }
            BoxType::SampleSizeBox => {
                let stsz = read_stsz(&mut b)?;
                log!("{:?}", stsz);
            }
            BoxType::ChunkOffsetBox => {
                let stco = read_stco(&mut b)?;
                track.empty_sample_boxes.empty_stco = stco.offsets.is_empty();
                log!("{:?}", stco);
            }
            BoxType::ChunkLargeOffsetBox => {
                let co64 = read_co64(&mut b)?;
                log!("{:?}", co64);
            }
            BoxType::SyncSampleBox => {
                let stss = read_stss(&mut b)?;
                log!("{:?}", stss);
            }
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(())
}

/// Parse an ftyp box.
fn read_ftyp<T: Read>(src: &mut BMFFBox<T>) -> Result<FileTypeBox> {
    let major = be_u32(src)?;
    let minor = be_u32(src)?;
    let bytes_left = src.bytes_left();
    if bytes_left % 4 != 0 {
        return Err(Error::InvalidData("invalid ftyp size"));
    }
    // Is a brand_count of zero valid?
    let brand_count = bytes_left / 4;
    let mut brands = Vec::new();
    for _ in 0..brand_count {
        brands.push(be_u32(src)?);
    }
    Ok(FileTypeBox {
        major_brand: major,
        minor_version: minor,
        compatible_brands: brands,
    })
}

/// Parse an mvhd box.
fn read_mvhd<T: Read>(src: &mut BMFFBox<T>) -> Result<MovieHeaderBox> {
    let (version, _) = read_fullbox_extra(src)?;
    match version {
        // 64 bit creation and modification times.
        1 => {
            skip(src, 16)?;
        }
        // 32 bit creation and modification times.
        0 => {
            skip(src, 8)?;
        }
        _ => return Err(Error::InvalidData("unhandled mvhd version")),
    }
    let timescale = be_u32(src)?;
    let duration = match version {
        1 => be_u64(src)?,
        0 => {
            let d = be_u32(src)?;
            if d == std::u32::MAX {
                std::u64::MAX
            } else {
                d as u64
            }
        }
        _ => return Err(Error::InvalidData("unhandled mvhd version")),
    };
    // Skip remaining fields.
    skip(src, 80)?;
    Ok(MovieHeaderBox {
        timescale: timescale,
        duration: duration,
    })
}

/// Parse a tkhd box.
fn read_tkhd<T: Read>(src: &mut BMFFBox<T>) -> Result<TrackHeaderBox> {
    let (version, flags) = read_fullbox_extra(src)?;
    let disabled = flags & 0x1u32 == 0 || flags & 0x2u32 == 0;
    match version {
        // 64 bit creation and modification times.
        1 => {
            skip(src, 16)?;
        }
        // 32 bit creation and modification times.
        0 => {
            skip(src, 8)?;
        }
        _ => return Err(Error::InvalidData("unhandled tkhd version")),
    }
    let track_id = be_u32(src)?;
    skip(src, 4)?;
    let duration = match version {
        1 => be_u64(src)?,
        0 => be_u32(src)? as u64,
        _ => return Err(Error::InvalidData("unhandled tkhd version")),
    };
    // Skip uninteresting fields.
    skip(src, 52)?;
    let width = be_u32(src)?;
    let height = be_u32(src)?;
    Ok(TrackHeaderBox {
        track_id: track_id,
        disabled: disabled,
        duration: duration,
        width: width,
        height: height,
    })
}

/// Parse a elst box.
fn read_elst<T: Read>(src: &mut BMFFBox<T>) -> Result<EditListBox> {
    let (version, _) = read_fullbox_extra(src)?;
    let edit_count = be_u32(src)?;
    if edit_count == 0 {
        return Err(Error::InvalidData("invalid edit count"));
    }
    let mut edits = Vec::new();
    for _ in 0..edit_count {
        let (segment_duration, media_time) = match version {
            1 => {
                // 64 bit segment duration and media times.
                (be_u64(src)?, be_i64(src)?)
            }
            0 => {
                // 32 bit segment duration and media times.
                (be_u32(src)? as u64, be_i32(src)? as i64)
            }
            _ => return Err(Error::InvalidData("unhandled elst version")),
        };
        let media_rate_integer = be_i16(src)?;
        let media_rate_fraction = be_i16(src)?;
        edits.push(Edit {
            segment_duration: segment_duration,
            media_time: media_time,
            media_rate_integer: media_rate_integer,
            media_rate_fraction: media_rate_fraction,
        })
    }

    Ok(EditListBox {
        edits: edits,
    })
}

/// Parse a mdhd box.
fn read_mdhd<T: Read>(src: &mut BMFFBox<T>) -> Result<MediaHeaderBox> {
    let (version, _) = read_fullbox_extra(src)?;
    let (timescale, duration) = match version {
        1 => {
            // Skip 64-bit creation and modification times.
            skip(src, 16)?;

            // 64 bit duration.
            (be_u32(src)?, be_u64(src)?)
        }
        0 => {
            // Skip 32-bit creation and modification times.
            skip(src, 8)?;

            // 32 bit duration.
            let timescale = be_u32(src)?;
            let duration = {
                // Since we convert the 32-bit duration to 64-bit by
                // upcasting, we need to preserve the special all-1s
                // ("unknown") case by hand.
                let d = be_u32(src)?;
                if d == std::u32::MAX {
                    std::u64::MAX
                } else {
                    d as u64
                }
            };
            (timescale, duration)
        }
        _ => return Err(Error::InvalidData("unhandled mdhd version")),
    };

    // Skip uninteresting fields.
    skip(src, 4)?;

    Ok(MediaHeaderBox {
        timescale: timescale,
        duration: duration,
    })
}

/// Parse a stco box.
fn read_stco<T: Read>(src: &mut BMFFBox<T>) -> Result<ChunkOffsetBox> {
    let (_, _) = read_fullbox_extra(src)?;
    let offset_count = be_u32(src)?;
    let mut offsets = Vec::new();
    for _ in 0..offset_count {
        offsets.push(be_u32(src)? as u64);
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(ChunkOffsetBox {
        offsets: offsets,
    })
}

/// Parse a co64 box.
fn read_co64<T: Read>(src: &mut BMFFBox<T>) -> Result<ChunkOffsetBox> {
    let (_, _) = read_fullbox_extra(src)?;
    let offset_count = be_u32(src)?;
    let mut offsets = Vec::new();
    for _ in 0..offset_count {
        offsets.push(be_u64(src)?);
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(ChunkOffsetBox {
        offsets: offsets,
    })
}

/// Parse a stss box.
fn read_stss<T: Read>(src: &mut BMFFBox<T>) -> Result<SyncSampleBox> {
    let (_, _) = read_fullbox_extra(src)?;
    let sample_count = be_u32(src)?;
    let mut samples = Vec::new();
    for _ in 0..sample_count {
        samples.push(be_u32(src)?);
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(SyncSampleBox {
        samples: samples,
    })
}

/// Parse a stsc box.
fn read_stsc<T: Read>(src: &mut BMFFBox<T>) -> Result<SampleToChunkBox> {
    let (_, _) = read_fullbox_extra(src)?;
    let sample_count = be_u32(src)?;
    let mut samples = Vec::new();
    for _ in 0..sample_count {
        let first_chunk = be_u32(src)?;
        let samples_per_chunk = be_u32(src)?;
        let sample_description_index = be_u32(src)?;
        samples.push(SampleToChunk {
            first_chunk: first_chunk,
            samples_per_chunk: samples_per_chunk,
            sample_description_index: sample_description_index,
        });
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(SampleToChunkBox {
        samples: samples,
    })
}

/// Parse a stsz box.
fn read_stsz<T: Read>(src: &mut BMFFBox<T>) -> Result<SampleSizeBox> {
    let (_, _) = read_fullbox_extra(src)?;
    let sample_size = be_u32(src)?;
    let sample_count = be_u32(src)?;
    let mut sample_sizes = Vec::new();
    if sample_size == 0 {
        for _ in 0..sample_count {
            sample_sizes.push(be_u32(src)?);
        }
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(SampleSizeBox {
        sample_size: sample_size,
        sample_sizes: sample_sizes,
    })
}

/// Parse a stts box.
fn read_stts<T: Read>(src: &mut BMFFBox<T>) -> Result<TimeToSampleBox> {
    let (_, _) = read_fullbox_extra(src)?;
    let sample_count = be_u32(src)?;
    let mut samples = Vec::new();
    for _ in 0..sample_count {
        let sample_count = be_u32(src)?;
        let sample_delta = be_u32(src)?;
        samples.push(Sample {
            sample_count: sample_count,
            sample_delta: sample_delta,
        });
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(TimeToSampleBox {
        samples: samples,
    })
}

/// Parse a VPx Config Box.
fn read_vpcc<T: Read>(src: &mut BMFFBox<T>) -> Result<VPxConfigBox> {
    let (version, _) = read_fullbox_extra(src)?;
    if version != 0 {
        return Err(Error::Unsupported("unknown vpcC version"));
    }

    let profile = src.read_u8()?;
    let level = src.read_u8()?;
    let (bit_depth, color_space) = {
        let byte = src.read_u8()?;
        ((byte >> 4) & 0x0f, byte & 0x0f)
    };
    let (chroma_subsampling, transfer_function, video_full_range) = {
        let byte = src.read_u8()?;
        ((byte >> 4) & 0x0f, (byte >> 1) & 0x07, (byte & 1) == 1)
    };

    let codec_init_size = be_u16(src)?;
    let codec_init = read_buf(src, codec_init_size as usize)?;

    // TODO(rillian): validate field value ranges.
    Ok(VPxConfigBox {
        profile: profile,
        level: level,
        bit_depth: bit_depth,
        color_space: color_space,
        chroma_subsampling: chroma_subsampling,
        transfer_function: transfer_function,
        video_full_range: video_full_range,
        codec_init: codec_init,
    })
}

fn read_flac_metadata<T: Read>(src: &mut BMFFBox<T>) -> Result<FLACMetadataBlock> {
    let temp = src.read_u8()?;
    let block_type = temp & 0x7f;
    let length = be_u24(src)?;
    if length as usize > src.bytes_left() {
        return Err(Error::InvalidData(
                "FLACMetadataBlock larger than parent box"));
    }
    let data = read_buf(src, length as usize)?;
    Ok(FLACMetadataBlock {
        block_type: block_type,
        data: data,
    })
}

fn find_descriptor(data: &[u8], esds: &mut ES_Descriptor) -> Result<()> {
    // Tags for elementary stream description
    const ESDESCR_TAG: u8          = 0x03;
    const DECODER_CONFIG_TAG: u8   = 0x04;
    const DECODER_SPECIFIC_TAG: u8 = 0x05;

    let mut remains = data;

    while !remains.is_empty() {
        let des = &mut Cursor::new(remains);
        let tag = des.read_u8()?;

        let extend_or_len = des.read_u8()?;
        // extension tag start from 0x80.
        let end = if extend_or_len >= 0x80 {
            // Extension found, skip remaining extension.
            skip(des, 2)?;
            des.read_u8()? as u64 + des.position()
        } else {
            extend_or_len as u64 + des.position()
        };

        if end as usize > remains.len() {
            return Err(Error::InvalidData("Invalid descriptor."));
        }

        let descriptor = &remains[des.position() as usize .. end as usize];

        match tag {
            ESDESCR_TAG => {
                read_es_descriptor(descriptor, esds)?;
            },
            DECODER_CONFIG_TAG => {
                read_dc_descriptor(descriptor, esds)?;
            },
            DECODER_SPECIFIC_TAG => {
                read_ds_descriptor(descriptor, esds)?;
            },
            _ => {
                log!("Unsupported descriptor, tag {}", tag);
            },
        }

        remains = &remains[end as usize .. remains.len()];
    }

    Ok(())
}

fn read_ds_descriptor(data: &[u8], esds: &mut ES_Descriptor) -> Result<()> {
    let frequency_table =
        vec![(0x1, 96000), (0x1, 88200), (0x2, 64000), (0x3, 48000),
             (0x4, 44100), (0x5, 32000), (0x6, 24000), (0x7, 22050),
             (0x8, 16000), (0x9, 12000), (0xa, 11025), (0xb, 8000),
             (0xc, 7350)];

    let des = &mut Cursor::new(data);

    let audio_specific_config = be_u16(des)?;

    let sample_index = (audio_specific_config & 0x07FF) >> 7;

    let channel_counts = (audio_specific_config & 0x007F) >> 3;

    let sample_frequency =
        frequency_table.iter().find(|item| item.0 == sample_index).map(|x| x.1);

    esds.audio_sample_rate = sample_frequency;
    esds.audio_channel_count = Some(channel_counts);

    Ok(())
}

fn read_dc_descriptor(data: &[u8], esds: &mut ES_Descriptor) -> Result<()> {
    let des = &mut Cursor::new(data);
    let object_profile = des.read_u8()?;

    // Skip uninteresting fields.
    skip(des, 12)?;

    if data.len() > des.position() as usize {
        find_descriptor(&data[des.position() as usize .. data.len()], esds)?;
    }

    esds.audio_codec = match object_profile {
        0x40 | 0x41 => CodecType::AAC,
        0x6B => CodecType::MP3,
        _ => CodecType::Unknown,
    };

    Ok(())
}

fn read_es_descriptor(data: &[u8], esds: &mut ES_Descriptor) -> Result<()> {
    let des = &mut Cursor::new(data);

    skip(des, 2)?;

    let esds_flags = des.read_u8()?;

    // Stream dependency flag, first bit from left most.
    if esds_flags & 0x80 > 0 {
        // Skip uninteresting fields.
        skip(des, 2)?;
    }

    // Url flag, second bit from left most.
    if esds_flags & 0x40 > 0 {
        // Skip uninteresting fields.
        let skip_es_len: usize = des.read_u8()? as usize + 2;
        skip(des, skip_es_len)?;
    }

    if data.len() > des.position() as usize {
        find_descriptor(&data[des.position() as usize .. data.len()], esds)?;
    }

    Ok(())
}

fn read_esds<T: Read>(src: &mut BMFFBox<T>) -> Result<ES_Descriptor> {
    let (_, _) = read_fullbox_extra(src)?;

    let esds_size = src.head.size - src.head.offset - 4;
    if esds_size > BUF_SIZE_LIMIT {
        return Err(Error::InvalidData("esds box exceeds BUF_SIZE_LIMIT"));
    }
    let esds_array = read_buf(src, esds_size as usize)?;

    let mut es_data = ES_Descriptor::default();
    find_descriptor(&esds_array, &mut es_data)?;

    es_data.codec_esds = esds_array;

    Ok(es_data)
}

/// Parse `FLACSpecificBox`.
fn read_dfla<T: Read>(src: &mut BMFFBox<T>) -> Result<FLACSpecificBox> {
    let (version, flags) = read_fullbox_extra(src)?;
    if version != 0 {
        return Err(Error::Unsupported("unknown dfLa (FLAC) version"));
    }
    if flags != 0 {
        return Err(Error::InvalidData("no-zero dfLa (FLAC) flags"));
    }
    let mut blocks = Vec::new();
    while src.bytes_left() > 0 {
        let block = read_flac_metadata(src)?;
        blocks.push(block);
    }
    // The box must have at least one meta block, and the first block
    // must be the METADATA_BLOCK_STREAMINFO
    if blocks.is_empty() {
        return Err(Error::InvalidData("FLACSpecificBox missing metadata"));
    } else if blocks[0].block_type != 0 {
        println!("flac metadata block:\n  {:?}", blocks[0]);
        return Err(Error::InvalidData(
                "FLACSpecificBox must have STREAMINFO metadata first"));
    } else if blocks[0].data.len() != 34 {
        return Err(Error::InvalidData(
                "FLACSpecificBox STREAMINFO block is the wrong size"));
    }
    Ok(FLACSpecificBox {
        version: version,
        blocks: blocks,
    })
}

/// Parse `OpusSpecificBox`.
fn read_dops<T: Read>(src: &mut BMFFBox<T>) -> Result<OpusSpecificBox> {
    let version = src.read_u8()?;
    if version != 0 {
        return Err(Error::Unsupported("unknown dOps (Opus) version"));
    }

    let output_channel_count = src.read_u8()?;
    let pre_skip = be_u16(src)?;
    let input_sample_rate = be_u32(src)?;
    let output_gain = be_i16(src)?;
    let channel_mapping_family = src.read_u8()?;

    let channel_mapping_table = if channel_mapping_family == 0 {
        None
    } else {
        let stream_count = src.read_u8()?;
        let coupled_count = src.read_u8()?;
        let channel_mapping = read_buf(src, output_channel_count as usize)?;

        Some(ChannelMappingTable {
            stream_count: stream_count,
            coupled_count: coupled_count,
            channel_mapping: channel_mapping,
        })
    };

    // TODO(kinetik): validate field value ranges.
    Ok(OpusSpecificBox {
        version: version,
        output_channel_count: output_channel_count,
        pre_skip: pre_skip,
        input_sample_rate: input_sample_rate,
        output_gain: output_gain,
        channel_mapping_family: channel_mapping_family,
        channel_mapping_table: channel_mapping_table,
    })
}

/// Re-serialize the Opus codec-specific config data as an `OpusHead` packet.
///
/// Some decoders expect the initialization data in the format used by the
/// Ogg and WebM encapsulations. To support this we prepend the `OpusHead`
/// tag and byte-swap the data from big- to little-endian relative to the
/// dOps box.
pub fn serialize_opus_header<W: byteorder::WriteBytesExt + std::io::Write>(opus: &OpusSpecificBox, dst: &mut W) -> Result<()> {
    match dst.write(b"OpusHead") {
        Err(e) => return Err(Error::from(e)),
        Ok(bytes) => {
            if bytes != 8 {
                return Err(Error::InvalidData("Couldn't write OpusHead tag."));
            }
        }
    }
    // In mp4 encapsulation, the version field is 0, but in ogg
    // it is 1. While decoders generally accept zero as well, write
    // out the version of the header we're supporting rather than
    // whatever we parsed out of mp4.
    dst.write_u8(1)?;
    dst.write_u8(opus.output_channel_count)?;
    dst.write_u16::<byteorder::LittleEndian>(opus.pre_skip)?;
    dst.write_u32::<byteorder::LittleEndian>(opus.input_sample_rate)?;
    dst.write_i16::<byteorder::LittleEndian>(opus.output_gain)?;
    dst.write_u8(opus.channel_mapping_family)?;
    match opus.channel_mapping_table {
        None => {}
        Some(ref table) => {
            dst.write_u8(table.stream_count)?;
            dst.write_u8(table.coupled_count)?;
            match dst.write(&table.channel_mapping) {
                Err(e) => return Err(Error::from(e)),
                Ok(bytes) => {
                    if bytes != table.channel_mapping.len() {
                        return Err(Error::InvalidData("Couldn't write channel mapping table data."));
                    }
                }
            }
        }
    };
    Ok(())
}

/// Parse a hdlr box.
fn read_hdlr<T: Read>(src: &mut BMFFBox<T>) -> Result<HandlerBox> {
    let (_, _) = read_fullbox_extra(src)?;

    // Skip uninteresting fields.
    skip(src, 4)?;

    let handler_type = be_u32(src)?;

    // Skip uninteresting fields.
    skip(src, 12)?;

    let bytes_left = src.bytes_left();
    let _name = read_null_terminated_string(src, bytes_left)?;

    Ok(HandlerBox {
        handler_type: handler_type,
    })
}

/// Parse an video description inside an stsd box.
fn read_video_sample_entry<T: Read>(src: &mut BMFFBox<T>, track: &mut Track) -> Result<SampleEntry> {
    let name = src.get_header().name;
    track.codec_type = match name {
        BoxType::AVCSampleEntry | BoxType::AVC3SampleEntry => CodecType::H264,
        BoxType::VP8SampleEntry => CodecType::VP8,
        BoxType::VP9SampleEntry => CodecType::VP9,
        BoxType::ProtectedVisualSampleEntry => CodecType::EncryptedVideo,
        _ => CodecType::Unknown,
    };

    // Skip uninteresting fields.
    skip(src, 6)?;

    let data_reference_index = be_u16(src)?;

    // Skip uninteresting fields.
    skip(src, 16)?;

    let width = be_u16(src)?;
    let height = be_u16(src)?;

    // Skip uninteresting fields.
    skip(src, 14)?;

    let _compressorname = read_fixed_length_pascal_string(src, 32)?;

    // Skip uninteresting fields.
    skip(src, 4)?;

    // Skip clap/pasp/etc. for now.
    let mut codec_specific = None;
    let mut iter = src.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::AVCConfigurationBox => {
                if (name != BoxType::AVCSampleEntry &&
                    name != BoxType::AVC3SampleEntry &&
                    name != BoxType::ProtectedVisualSampleEntry) ||
                    codec_specific.is_some() {
                        return Err(Error::InvalidData("malformed video sample entry"));
                    }
                let avcc_size = b.head.size - b.head.offset;
                if avcc_size > BUF_SIZE_LIMIT {
                    return Err(Error::InvalidData("avcC box exceeds BUF_SIZE_LIMIT"));
                }
                let avcc = read_buf(&mut b.content, avcc_size as usize)?;
                // TODO(kinetik): Parse avcC box?  For now we just stash the data.
                codec_specific = Some(VideoCodecSpecific::AVCConfig(avcc));
            }
            BoxType::VPCodecConfigurationBox => { // vpcC
                if (name != BoxType::VP8SampleEntry &&
                    name != BoxType::VP9SampleEntry) ||
                    codec_specific.is_some() {
                        return Err(Error::InvalidData("malformed video sample entry"));
                    }
                let vpcc = read_vpcc(&mut b)?;
                codec_specific = Some(VideoCodecSpecific::VPxConfig(vpcc));
            }
            _ => skip_box_content(&mut b)?,
        }
        check_parser_state!(b.content);
    }

    codec_specific
        .map(|codec_specific| SampleEntry::Video(VideoSampleEntry {
            data_reference_index: data_reference_index,
            width: width,
            height: height,
            codec_specific: codec_specific,
        }))
        .ok_or_else(|| Error::InvalidData("malformed video sample entry"))
}

fn read_qt_wave_atom<T: Read>(src: &mut BMFFBox<T>) -> Result<ES_Descriptor> {
    let mut codec_specific = None;
    let mut iter = src.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::ESDBox => {
                let esds = read_esds(&mut b)?;
                codec_specific = Some(esds);
            },
            _ => skip_box_content(&mut b)?,
        }
    }

    codec_specific.ok_or_else(|| Error::InvalidData("malformed audio sample entry"))
}

/// Parse an audio description inside an stsd box.
fn read_audio_sample_entry<T: Read>(src: &mut BMFFBox<T>, track: &mut Track) -> Result<SampleEntry> {
    let name = src.get_header().name;
    track.codec_type = match name {
        // TODO(kinetik): stagefright inspects ESDS to detect MP3 (audio/mpeg).
        BoxType::MP4AudioSampleEntry => CodecType::AAC,
        BoxType::FLACSampleEntry => CodecType::FLAC,
        BoxType::OpusSampleEntry => CodecType::Opus,
        BoxType::ProtectedAudioSampleEntry => CodecType::EncryptedAudio,
        _ => CodecType::Unknown,
    };

    // Skip uninteresting fields.
    skip(src, 6)?;

    let data_reference_index = be_u16(src)?;

    // XXX(kinetik): This is "reserved" in BMFF, but some old QT MOV variant
    // uses it, need to work out if we have to support it.  Without checking
    // here and reading extra fields after samplerate (or bailing with an
    // error), the parser loses sync completely.
    let version = be_u16(src)?;

    // Skip uninteresting fields.
    skip(src, 6)?;

    let channelcount = be_u16(src)?;
    let samplesize = be_u16(src)?;

    // Skip uninteresting fields.
    skip(src, 4)?;

    let samplerate = be_u32(src)?;

    match version {
        0 => (),
        1 => {
            // Quicktime sound sample description version 1.
            // Skip uninteresting fields.
            skip(src, 16)?;
        },
        _ => return Err(Error::Unsupported("unsupported non-isom audio sample entry")),
    }

    // Skip chan/etc. for now.
    let mut codec_specific = None;
    let mut iter = src.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::ESDBox => {
                if (name != BoxType::MP4AudioSampleEntry &&
                    name != BoxType::ProtectedAudioSampleEntry) ||
                    codec_specific.is_some() {
                        return Err(Error::InvalidData("malformed audio sample entry"));
                }

                let esds = read_esds(&mut b)?;
                track.codec_type = esds.audio_codec;
                codec_specific = Some(AudioCodecSpecific::ES_Descriptor(esds));
            }
            BoxType::FLACSpecificBox => {
                if name != BoxType::FLACSampleEntry ||
                    codec_specific.is_some() {
                    return Err(Error::InvalidData("malformed audio sample entry"));
                }
                let dfla = read_dfla(&mut b)?;
                track.codec_type = CodecType::FLAC;
                codec_specific = Some(AudioCodecSpecific::FLACSpecificBox(dfla));
            }
            BoxType::OpusSpecificBox => {
                if name != BoxType::OpusSampleEntry ||
                    codec_specific.is_some() {
                    return Err(Error::InvalidData("malformed audio sample entry"));
                }
                let dops = read_dops(&mut b)?;
                track.codec_type = CodecType::Opus;
                codec_specific = Some(AudioCodecSpecific::OpusSpecificBox(dops));
            }
            BoxType::QTWaveAtom => {
                let qt_esds = read_qt_wave_atom(&mut b)?;
                track.codec_type = qt_esds.audio_codec;
                codec_specific = Some(AudioCodecSpecific::ES_Descriptor(qt_esds));
            }
            _ => skip_box_content(&mut b)?,
        }
        check_parser_state!(b.content);
    }

    codec_specific
        .map(|codec_specific| SampleEntry::Audio(AudioSampleEntry {
            data_reference_index: data_reference_index,
            channelcount: channelcount,
            samplesize: samplesize,
            samplerate: samplerate,
            codec_specific: codec_specific,
        }))
        .ok_or_else(|| Error::InvalidData("malformed audio sample entry"))
}

/// Parse a stsd box.
fn read_stsd<T: Read>(src: &mut BMFFBox<T>, track: &mut Track) -> Result<SampleDescriptionBox> {
    let (_, _) = read_fullbox_extra(src)?;

    let description_count = be_u32(src)?;
    let mut descriptions = Vec::new();

    {
        // TODO(kinetik): check if/when more than one desc per track? do we need to support?
        let mut iter = src.box_iter();
        while let Some(mut b) = iter.next_box()? {
            let description = match track.track_type {
                TrackType::Video => read_video_sample_entry(&mut b, track),
                TrackType::Audio => read_audio_sample_entry(&mut b, track),
                TrackType::Unknown => Err(Error::Unsupported("unknown track type")),
            };
            let description = match description {
                Ok(desc) => desc,
                Err(Error::Unsupported(_)) => {
                    // read_{audio,video}_desc may have returned Unsupported
                    // after partially reading the box content, so we can't
                    // simply use skip_box_content here.
                    let to_skip = b.bytes_left();
                    skip(&mut b, to_skip)?;
                    SampleEntry::Unknown
                }
                Err(e) => return Err(e),
            };
            if track.data.is_none() {
                track.data = Some(description.clone());
            } else {
                log!("** don't know how to handle multiple descriptions **");
            }
            descriptions.push(description);
            check_parser_state!(b.content);
            if descriptions.len() == description_count as usize {
                break;
            }
        }
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(SampleDescriptionBox {
        descriptions: descriptions,
    })
}

/// Skip a number of bytes that we don't care to parse.
fn skip<T: Read>(src: &mut T, mut bytes: usize) -> Result<()> {
    const BUF_SIZE: usize = 64 * 1024;
    let mut buf = vec![0; BUF_SIZE];
    while bytes > 0 {
        let buf_size = cmp::min(bytes, BUF_SIZE);
        let len = src.take(buf_size as u64).read(&mut buf)?;
        if len == 0 {
            return Err(Error::UnexpectedEOF);
        }
        bytes -= len;
    }
    Ok(())
}

/// Read size bytes into a Vector or return error.
fn read_buf<T: ReadBytesExt>(src: &mut T, size: usize) -> Result<Vec<u8>> {
    let mut buf = vec![0; size];
    let r = src.read(&mut buf)?;
    if r != size {
        return Err(Error::InvalidData("failed buffer read"));
    }
    Ok(buf)
}

// TODO(kinetik): Find a copy of ISO/IEC 14496-1 to confirm various string encodings.
// XXX(kinetik): definition of "null-terminated" string is fuzzy, we have:
// - zero or more byte strings, with a single null terminating the string.
// - zero byte strings with no null terminator (i.e. zero space in the box for the string)
// - length-prefixed strings with no null terminator (e.g. bear_rotate_0.mp4)
fn read_null_terminated_string<T: ReadBytesExt>(src: &mut T, mut size: usize) -> Result<String> {
    let mut buf = Vec::new();
    while size > 0 {
        let c = src.read_u8()?;
        if c == 0 {
            break;
        }
        buf.push(c);
        size -= 1;
    }
    String::from_utf8(buf).map_err(From::from)
}

#[allow(dead_code)]
fn read_pascal_string<T: ReadBytesExt>(src: &mut T) -> Result<String> {
    let len = src.read_u8()?;
    let buf = read_buf(src, len as usize)?;
    String::from_utf8(buf).map_err(From::from)
}

// Weird string encoding with a length prefix and a fixed sized buffer which
// contains padding if the string doesn't fill the buffer.
fn read_fixed_length_pascal_string<T: Read>(src: &mut T, size: usize) -> Result<String> {
    assert!(size > 0);
    let len = cmp::min(src.read_u8()? as usize, size - 1);
    let buf = read_buf(src, len)?;
    skip(src, size - 1 - buf.len())?;
    String::from_utf8(buf).map_err(From::from)
}

fn be_i16<T: ReadBytesExt>(src: &mut T) -> Result<i16> {
    src.read_i16::<byteorder::BigEndian>().map_err(From::from)
}

fn be_i32<T: ReadBytesExt>(src: &mut T) -> Result<i32> {
    src.read_i32::<byteorder::BigEndian>().map_err(From::from)
}

fn be_i64<T: ReadBytesExt>(src: &mut T) -> Result<i64> {
    src.read_i64::<byteorder::BigEndian>().map_err(From::from)
}

fn be_u16<T: ReadBytesExt>(src: &mut T) -> Result<u16> {
    src.read_u16::<byteorder::BigEndian>().map_err(From::from)
}

fn be_u24<T: ReadBytesExt>(src: &mut T) -> Result<u32> {
    src.read_uint::<byteorder::BigEndian>(3)
        .map(|v| v as u32)
        .map_err(From::from)
}

fn be_u32<T: ReadBytesExt>(src: &mut T) -> Result<u32> {
    src.read_u32::<byteorder::BigEndian>().map_err(From::from)
}

fn be_u64<T: ReadBytesExt>(src: &mut T) -> Result<u64> {
    src.read_u64::<byteorder::BigEndian>().map_err(From::from)
}

fn write_be_u32<T: WriteBytesExt>(des: &mut T, num: u32) -> Result<()> {
    des.write_u32::<byteorder::BigEndian>(num).map_err(From::from)
}
