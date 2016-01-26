//! Module for parsing ISO Base Media Format aka video/mp4 streams.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

mod byteorder; // 'extern crate' upstream.
use byteorder::ReadBytesExt;
use std::error::Error as ErrorTrait; // For Err(e) => e.description().
use std::io::{Read, BufRead, Take};
use std::cmp;
use std::fmt;

// Expose C api wrapper.
pub mod capi;
// FIXME: We can 'pub use capi::*' in rustc 1.5 and later.
pub use capi::{mp4parse_new, mp4parse_free, mp4parse_read};

/// Describes parser failures.
///
/// This enum wraps athe standard `io::Error` type, unified with
/// our own parser error states and those of crates we use.
#[derive(Debug)]
pub enum Error {
    /// Parse error caused by corrupt or malformed data.
    InvalidData,
    /// Parse error caused by limited parser support rather than invalid data.
    Unsupported,
    /// Reflect `byteorder::Error::UnexpectedEOF` for short data.
    UnexpectedEOF,
    /// Caught panic! or assert! meaning the parser couldn't recover.
    AssertCaught,
    /// Propagate underlying errors from `std::io`.
    Io(std::io::Error),
}

impl From<std::io::Error> for Error {
    fn from(err: std::io::Error) -> Error {
        Error::Io(err)
    }
}

impl From<byteorder::Error> for Error {
    fn from(err: byteorder::Error) -> Error {
        match err {
            byteorder::Error::UnexpectedEOF => Error::UnexpectedEOF,
            byteorder::Error::Io(e) => Error::Io(e),
        }
    }
}

/// Result shorthand using our Error enum.
pub type Result<T> = std::result::Result<T, Error>;

/// Four-byte 'character code' describing the type of a piece of data.
#[derive(Clone, Copy, Eq, PartialEq)]
pub struct FourCC([u8; 4]);

impl fmt::Debug for FourCC {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "'{}'", String::from_utf8_lossy(&self.0))
    }
}

/// Basic ISO box structure.
///
/// mp4 files are a sequence of possibly-nested 'box' structures.
/// Each box begins with a header describing the length of the
/// box's data and a four-byte 'character code' or `FourCC` which
/// identifies the type of the box. Together these are enough to
/// interpret the contents of that section of the file.
#[derive(Debug, Clone, Copy)]
pub struct BoxHeader {
    /// Four character box type.
    pub name: FourCC,
    /// Size of the box in bytes.
    pub size: u64,
    /// Offset to the start of the contained data (or header size).
    pub offset: u64,
}

/// File type box 'ftyp'.
#[derive(Debug)]
struct FileTypeBox {
    header: BoxHeader,
    major_brand: FourCC,
    minor_version: u32,
    compatible_brands: Vec<FourCC>,
}

/// Movie header box 'mvhd'.
#[derive(Debug)]
struct MovieHeaderBox {
    header: BoxHeader,
    timescale: u32,
    duration: u64,
}

/// Track header box 'tkhd'
#[derive(Debug, Clone)]
struct TrackHeaderBox {
    header: BoxHeader,
    track_id: u32,
    disabled: bool,
    duration: u64,
    width: u32,
    height: u32,
}

/// Edit list box 'elst'
#[derive(Debug)]
struct EditListBox {
    header: BoxHeader,
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
    header: BoxHeader,
    timescale: u32,
    duration: u64,
}

// Chunk offset box 'stco' or 'co64'
#[derive(Debug)]
struct ChunkOffsetBox {
    header: BoxHeader,
    offsets: Vec<u64>,
}

// Sync sample box 'stss'
#[derive(Debug)]
struct SyncSampleBox {
    header: BoxHeader,
    samples: Vec<u32>,
}

// Sample to chunk box 'stsc'
#[derive(Debug)]
struct SampleToChunkBox {
    header: BoxHeader,
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
    header: BoxHeader,
    sample_size: u32,
    sample_sizes: Vec<u32>,
}

// Time to sample box 'stts'
#[derive(Debug)]
struct TimeToSampleBox {
    header: BoxHeader,
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
    header: BoxHeader,
    handler_type: FourCC,
}

// Sample description box 'stsd'
#[derive(Debug)]
struct SampleDescriptionBox {
    header: BoxHeader,
    descriptions: Vec<SampleEntry>,
}

#[derive(Debug, Clone)]
enum SampleEntry {
    Audio(AudioSampleEntry),
    Video(VideoSampleEntry),
    Unknown,
}

#[derive(Debug, Clone)]
struct AudioSampleEntry {
    data_reference_index: u16,
    channelcount: u16,
    samplesize: u16,
    samplerate: u32,
    esds: ES_Descriptor,
}

#[derive(Debug, Clone)]
struct VideoSampleEntry {
    data_reference_index: u16,
    width: u16,
    height: u16,
    avcc: AVCDecoderConfigurationRecord,
}

#[derive(Debug, Clone)]
struct AVCDecoderConfigurationRecord {
    data: Vec<u8>,
}

#[allow(non_camel_case_types)]
#[derive(Debug, Clone)]
struct ES_Descriptor {
    data: Vec<u8>,
}

/// Internal data structures.
#[derive(Debug)]
pub struct MediaContext {
    timescale: Option<MediaTimeScale>,
    /// Tracks found in the file.
    tracks: Vec<Track>,
    /// Print boxes and other info as parsing proceeds. For debugging.
    trace: bool,
}

impl MediaContext {
    pub fn new() -> MediaContext {
        MediaContext {
            timescale: None,
            tracks: Vec::new(),
            trace: false,
        }
    }

    pub fn trace(&mut self, on: bool) {
        self.trace = on;
    }
}

macro_rules! log {
    ( $ctx:expr, $( $args:expr),* ) => {
        if $ctx.trace {
            println!( $( $args, )* );
        }
    }
}

#[derive(Debug)]
enum TrackType {
    Audio,
    Video,
    Unknown,
}

/// The media's global (mvhd) timescale.
#[derive(Debug, Copy, Clone)]
struct MediaTimeScale(u64);

/// A time scaled by the media's global (mvhd) timescale.
#[derive(Debug, Copy, Clone)]
struct MediaScaledTime(u64);

/// The track's local (mdhd) timescale.
#[derive(Debug, Copy, Clone)]
struct TrackTimeScale(u64, usize);

/// A time scaled by the track's local (mdhd) timescale.
#[derive(Debug, Copy, Clone)]
struct TrackScaledTime(u64, usize);

#[derive(Debug)]
struct Track {
    id: usize,
    track_type: TrackType,
    empty_duration: Option<MediaScaledTime>,
    media_time: Option<TrackScaledTime>,
    timescale: Option<TrackTimeScale>,
    duration: Option<TrackScaledTime>,
    track_id: Option<u32>,
    mime_type: String,
    data: Option<SampleEntry>,
    tkhd: Option<TrackHeaderBox>, // TODO(kinetik): find a nicer way to export this.
}

impl Track {
    fn new(id: usize) -> Track {
        Track {
            id: id,
            track_type: TrackType::Unknown,
            empty_duration: None,
            media_time: None,
            timescale: None,
            duration: None,
            track_id: None,
            mime_type: String::new(),
            data: None,
            tkhd: None,
        }
    }
}

/// Read and parse a box header.
///
/// Call this first to determine the type of a particular mp4 box
/// and its length. Used internally for dispatching to specific
/// parsers for the internal content, or to get the length to
/// skip unknown or uninteresting boxes.
pub fn read_box_header<T: ReadBytesExt>(src: &mut T) -> Result<BoxHeader> {
    let size32 = try!(be_u32(src));
    let name = try!(be_fourcc(src));
    let size = match size32 {
        0 => return Err(Error::Unsupported),
        1 => {
            let size64 = try!(be_u64(src));
            if size64 < 16 {
                return Err(Error::InvalidData);
            }
            size64
        }
        2...7 => return Err(Error::InvalidData),
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
    let version = try!(src.read_u8());
    let flags_a = try!(src.read_u8());
    let flags_b = try!(src.read_u8());
    let flags_c = try!(src.read_u8());
    Ok((version,
        (flags_a as u32) << 16 | (flags_b as u32) << 8 | (flags_c as u32)))
}

/// Skip over the entire contents of a box.
fn skip_box_content<T: BufRead>(src: &mut T, header: &BoxHeader) -> Result<usize> {
    skip(src, (header.size - header.offset) as usize)
}

/// Skip over the remaining contents of a box.
fn skip_remaining_box_content<T: BufRead>(src: &mut T, header: &BoxHeader) -> Result<()> {
    match skip(src, (header.size - header.offset) as usize) {
        Ok(_) | Err(Error::UnexpectedEOF) => Ok(()),
        e => Err(e.err().unwrap()),
    }
}

/// Helper to construct a Take over the contents of a box.
fn limit<'a, T: BufRead>(f: &'a mut T, h: &BoxHeader) -> Take<&'a mut T> {
    f.take(h.size - h.offset)
}

fn driver<F, T: BufRead>(f: &mut T, context: &mut MediaContext, mut action: F) -> Result<()>
    where F: FnMut(&mut MediaContext, BoxHeader, &mut Take<&mut T>) -> Result<()>
{
    loop {
        let r = read_box_header(f).and_then(|h| {
            let mut content = limit(f, &h);
            let r = action(context, h, &mut content);
            if r.is_ok() {
                // TODO(kinetik): can check this for "non-fatal" errors (e.g. EOF) too.
                log!(context, "{} content bytes left", content.limit());
                assert!(content.limit() == 0);
                log!(context, "read_box context: {:?}", context);
            }
            r
        });
        match r {
            Ok(_) => {}
            Err(Error::UnexpectedEOF) => {
                // byteorder returns EOF at the end of the buffer.
                // This isn't an error for us, just an signal to
                // stop recursion.
                log!(context, "Caught Error::UnexpectedEOF");
                break;
            }
            Err(Error::InvalidData) => {
                log!(context, "Invalid data");
                return Err(Error::InvalidData);
            }
            Err(Error::Unsupported) => {
                log!(context, "Unsupported BMFF construct");
                return Err(Error::Unsupported);
            }
            Err(Error::AssertCaught) => {
                log!(context, "Unrecoverable error or assertion");
                return Err(Error::AssertCaught);
            }
            Err(Error::Io(e)) => {
                log!(context,
                     "I/O Error '{:?}' reading box: {:?}",
                     e.kind(),
                     e.description());
                return Err(Error::Io(e));
            }
        }
    }
    Ok(())
}

/// Read the contents of a box, including sub boxes.
///
/// Metadata is accumulated in the passed-through MediaContext struct,
/// which can be examined later.
pub fn read_mp4<T: BufRead>(f: &mut T, context: &mut MediaContext) -> Result<()> {
    driver(f, context, |context, h, mut content| {
        match &h.name.0 {
            b"ftyp" => {
                let ftyp = try!(read_ftyp(&mut content, &h));
                log!(context, "{:?}", ftyp);
            }
            b"moov" => try!(read_moov(&mut content, &h, context)),
            _ => {
                // Skip the contents of unknown chunks.
                try!(skip_box_content(&mut content, &h));
            }
        };
        Ok(())
    })
}

fn parse_mvhd<T: BufRead>(f: &mut T, h: &BoxHeader) -> Result<(MovieHeaderBox, Option<MediaTimeScale>)> {
    let mvhd = try!(read_mvhd(f, &h));
    if mvhd.timescale == 0 {
        return Err(Error::InvalidData);
    }
    let timescale = Some(MediaTimeScale(mvhd.timescale as u64));
    Ok((mvhd, timescale))
}

fn read_moov<T: BufRead>(f: &mut T, _: &BoxHeader, context: &mut MediaContext) -> Result<()> {
    driver(f, context, |context, h, mut content| {
        match &h.name.0 {
            b"mvhd" => {
                let (mvhd, timescale) = try!(parse_mvhd(content, &h));
                context.timescale = timescale;
                log!(context, "  {:?}", mvhd);
            }
            b"trak" => {
                let mut track = Track::new(context.tracks.len());
                try!(read_trak(&mut content, &h, context, &mut track));
                context.tracks.push(track);
            }
            _ => {
                // Skip the contents of unknown chunks.
                log!(context, "{:?} (skipped)", h);
                try!(skip_box_content(&mut content, &h));
            }
        };
        Ok(())
    })
}

fn read_trak<T: BufRead>(f: &mut T, _: &BoxHeader, context: &mut MediaContext, track: &mut Track) -> Result<()> {
    driver(f, context, |context, h, mut content| {
        match &h.name.0 {
            b"tkhd" => {
                let tkhd = try!(read_tkhd(&mut content, &h));
                track.track_id = Some(tkhd.track_id);
                track.tkhd = Some(tkhd.clone());
                log!(context, "  {:?}", tkhd);
            }
            b"edts" => try!(read_edts(&mut content, &h, context, track)),
            b"mdia" => try!(read_mdia(&mut content, &h, context, track)),
            _ => {
                // Skip the contents of unknown chunks.
                log!(context, "{:?} (skipped)", h);
                try!(skip_box_content(&mut content, &h));
            }
        };
        Ok(())
    })
}

fn read_edts<T: BufRead>(f: &mut T, _: &BoxHeader, context: &mut MediaContext, track: &mut Track) -> Result<()> {
    driver(f, context, |context, h, mut content| {
        match &h.name.0 {
            b"elst" => {
                let elst = try!(read_elst(&mut content, &h));
                let mut empty_duration = 0;
                let mut idx = 0;
                if elst.edits.len() > 2 {
                    return Err(Error::Unsupported);
                }
                if elst.edits[idx].media_time == -1 {
                    empty_duration = elst.edits[0].segment_duration;
                    idx += 1;
                }
                track.empty_duration = Some(MediaScaledTime(empty_duration));
                if elst.edits[idx].media_time < 0 {
                    return Err(Error::InvalidData);
                }
                track.media_time = Some(TrackScaledTime(elst.edits[idx].media_time as u64,
                                                        track.id));
                log!(context, "  {:?}", elst);
            }
            _ => {
                // Skip the contents of unknown chunks.
                log!(context, "{:?} (skipped)", h);
                try!(skip_box_content(&mut content, &h));
            }
        };
        Ok(())
    })
}

fn parse_mdhd<T: BufRead>(f: &mut T, h: &BoxHeader, track: &mut Track) -> Result<(MediaHeaderBox, Option<TrackScaledTime>, Option<TrackTimeScale>)> {
    let mdhd = try!(read_mdhd(f, h));
    let duration = match mdhd.duration {
        std::u64::MAX => None,
        duration => Some(TrackScaledTime(duration, track.id)),
    };
    if mdhd.timescale == 0 {
        return Err(Error::InvalidData);
    }
    let timescale = Some(TrackTimeScale(mdhd.timescale as u64, track.id));
    Ok((mdhd, duration, timescale))
}

fn read_mdia<T: BufRead>(f: &mut T, _: &BoxHeader, context: &mut MediaContext, track: &mut Track) -> Result<()> {
    driver(f, context, |context, h, mut content| {
        match &h.name.0 {
            b"mdhd" => {
                let (mdhd, duration, timescale) = try!(parse_mdhd(content, &h, track));
                track.duration = duration;
                track.timescale = timescale;
                log!(context, "  {:?}", mdhd);
            }
            b"hdlr" => {
                let hdlr = try!(read_hdlr(&mut content, &h));
                match &hdlr.handler_type.0 {
                    b"vide" => track.track_type = TrackType::Video,
                    b"soun" => track.track_type = TrackType::Audio,
                    _ => (),
                }
                log!(context, "  {:?}", hdlr);
            }
            b"minf" => try!(read_minf(&mut content, &h, context, track)),
            _ => {
                // Skip the contents of unknown chunks.
                log!(context, "{:?} (skipped)", h);
                try!(skip_box_content(&mut content, &h));
            }
        };
        Ok(())
    })
}

fn read_minf<T: BufRead>(f: &mut T, _: &BoxHeader, context: &mut MediaContext, track: &mut Track) -> Result<()> {
    driver(f, context, |context, h, mut content| {
        match &h.name.0 {
            b"stbl" => try!(read_stbl(&mut content, &h, context, track)),
            _ => {
                // Skip the contents of unknown chunks.
                log!(context, "{:?} (skipped)", h);
                try!(skip_box_content(&mut content, &h));
            }
        };
        Ok(())
    })
}

fn read_stbl<T: BufRead>(f: &mut T, _: &BoxHeader, context: &mut MediaContext, track: &mut Track) -> Result<()> {
    driver(f, context, |context, h, mut content| {
        match &h.name.0 {
            b"stsd" => {
                let stsd = try!(read_stsd(&mut content, &h, track));
                log!(context, "  {:?}", stsd);
            }
            b"stts" => {
                let stts = try!(read_stts(&mut content, &h));
                log!(context, "  {:?}", stts);
            }
            b"stsc" => {
                let stsc = try!(read_stsc(&mut content, &h));
                log!(context, "  {:?}", stsc);
            }
            b"stsz" => {
                let stsz = try!(read_stsz(&mut content, &h));
                log!(context, "  {:?}", stsz);
            }
            b"stco" => {
                let stco = try!(read_stco(&mut content, &h));
                log!(context, "  {:?}", stco);
            }
            b"co64" => {
                let co64 = try!(read_co64(&mut content, &h));
                log!(context, "  {:?}", co64);
            }
            b"stss" => {
                let stss = try!(read_stss(&mut content, &h));
                log!(context, "  {:?}", stss);
            }
            _ => {
                // Skip the contents of unknown chunks.
                log!(context, "{:?} (skipped)", h);
                try!(skip_box_content(&mut content, &h));
            }
        };
        Ok(())
    })
}

/// Parse an ftyp box.
fn read_ftyp<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> Result<FileTypeBox> {
    let major = try!(be_fourcc(src));
    let minor = try!(be_u32(src));
    let bytes_left = head.size - head.offset - 8;
    if bytes_left % 4 != 0 {
        return Err(Error::InvalidData);
    }
    // Is a brand_count of zero valid?
    let brand_count = bytes_left / 4;
    let mut brands = Vec::new();
    for _ in 0..brand_count {
        brands.push(try!(be_fourcc(src)));
    }
    Ok(FileTypeBox {
        header: *head,
        major_brand: major,
        minor_version: minor,
        compatible_brands: brands,
    })
}

/// Parse an mvhd box.
fn read_mvhd<T: ReadBytesExt + BufRead>(src: &mut T, head: &BoxHeader) -> Result<MovieHeaderBox> {
    let (version, _) = try!(read_fullbox_extra(src));
    match version {
        // 64 bit creation and modification times.
        1 => {
            try!(skip(src, 16));
        }
        // 32 bit creation and modification times.
        0 => {
            try!(skip(src, 8));
        }
        _ => return Err(Error::InvalidData),
    }
    let timescale = try!(be_u32(src));
    let duration = match version {
        1 => try!(be_u64(src)),
        0 => {
            let d = try!(be_u32(src));
            if d == std::u32::MAX {
                std::u64::MAX
            } else {
                d as u64
            }
        }
        _ => return Err(Error::InvalidData),
    };
    // Skip remaining fields.
    try!(skip(src, 80));
    Ok(MovieHeaderBox {
        header: *head,
        timescale: timescale,
        duration: duration,
    })
}

/// Parse a tkhd box.
fn read_tkhd<T: ReadBytesExt + BufRead>(src: &mut T, head: &BoxHeader) -> Result<TrackHeaderBox> {
    let (version, flags) = try!(read_fullbox_extra(src));
    let disabled = flags & 0x1u32 == 0 || flags & 0x2u32 == 0;
    match version {
        // 64 bit creation and modification times.
        1 => {
            try!(skip(src, 16));
        }
        // 32 bit creation and modification times.
        0 => {
            try!(skip(src, 8));
        }
        _ => return Err(Error::InvalidData),
    }
    let track_id = try!(be_u32(src));
    try!(skip(src, 4));
    let duration = match version {
        1 => try!(be_u64(src)),
        0 => try!(be_u32(src)) as u64,
        _ => return Err(Error::InvalidData),
    };
    // Skip uninteresting fields.
    try!(skip(src, 52));
    let width = try!(be_u32(src));
    let height = try!(be_u32(src));
    Ok(TrackHeaderBox {
        header: *head,
        track_id: track_id,
        disabled: disabled,
        duration: duration,
        width: width,
        height: height,
    })
}

/// Parse a elst box.
fn read_elst<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> Result<EditListBox> {
    let (version, _) = try!(read_fullbox_extra(src));
    let edit_count = try!(be_u32(src));
    let mut edits = Vec::new();
    for _ in 0..edit_count {
        let (segment_duration, media_time) = match version {
            1 => {
                // 64 bit segment duration and media times.
                (try!(be_u64(src)), try!(be_i64(src)))
            }
            0 => {
                // 32 bit segment duration and media times.
                (try!(be_u32(src)) as u64, try!(be_i32(src)) as i64)
            }
            _ => return Err(Error::InvalidData),
        };
        let media_rate_integer = try!(be_i16(src));
        let media_rate_fraction = try!(be_i16(src));
        edits.push(Edit {
            segment_duration: segment_duration,
            media_time: media_time,
            media_rate_integer: media_rate_integer,
            media_rate_fraction: media_rate_fraction,
        })
    }

    Ok(EditListBox {
        header: *head,
        edits: edits,
    })
}

/// Parse a mdhd box.
fn read_mdhd<T: ReadBytesExt + BufRead>(src: &mut T, head: &BoxHeader) -> Result<MediaHeaderBox> {
    let (version, _) = try!(read_fullbox_extra(src));
    let (timescale, duration) = match version {
        1 => {
            // Skip 64-bit creation and modification times.
            try!(skip(src, 16));

            // 64 bit duration.
            (try!(be_u32(src)), try!(be_u64(src)))
        }
        0 => {
            // Skip 32-bit creation and modification times.
            try!(skip(src, 8));

            // 32 bit duration.
            let timescale = try!(be_u32(src));
            let duration = {
                // Since we convert the 32-bit duration to 64-bit by
                // upcasting, we need to preserve the special all-1s
                // ("unknown") case by hand.
                let d = try!(be_u32(src));
                if d == std::u32::MAX {
                    std::u64::MAX
                } else {
                    d as u64
                }
            };
            (timescale, duration)
        }
        _ => return Err(Error::InvalidData),
    };

    // Skip uninteresting fields.
    try!(skip(src, 4));

    Ok(MediaHeaderBox {
        header: *head,
        timescale: timescale,
        duration: duration,
    })
}

/// Parse a stco box.
fn read_stco<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> Result<ChunkOffsetBox> {
    let (_, _) = try!(read_fullbox_extra(src));
    let offset_count = try!(be_u32(src));
    let mut offsets = Vec::new();
    for _ in 0..offset_count {
        offsets.push(try!(be_u32(src)) as u64);
    }

    Ok(ChunkOffsetBox {
        header: *head,
        offsets: offsets,
    })
}

/// Parse a stco box.
fn read_co64<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> Result<ChunkOffsetBox> {
    let (_, _) = try!(read_fullbox_extra(src));
    let offset_count = try!(be_u32(src));
    let mut offsets = Vec::new();
    for _ in 0..offset_count {
        offsets.push(try!(be_u64(src)));
    }

    Ok(ChunkOffsetBox {
        header: *head,
        offsets: offsets,
    })
}

/// Parse a stss box.
fn read_stss<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> Result<SyncSampleBox> {
    let (_, _) = try!(read_fullbox_extra(src));
    let sample_count = try!(be_u32(src));
    let mut samples = Vec::new();
    for _ in 0..sample_count {
        samples.push(try!(be_u32(src)));
    }

    Ok(SyncSampleBox {
        header: *head,
        samples: samples,
    })
}

/// Parse a stsc box.
fn read_stsc<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> Result<SampleToChunkBox> {
    let (_, _) = try!(read_fullbox_extra(src));
    let sample_count = try!(be_u32(src));
    let mut samples = Vec::new();
    for _ in 0..sample_count {
        let first_chunk = try!(be_u32(src));
        let samples_per_chunk = try!(be_u32(src));
        let sample_description_index = try!(be_u32(src));
        samples.push(SampleToChunk {
            first_chunk: first_chunk,
            samples_per_chunk: samples_per_chunk,
            sample_description_index: sample_description_index,
        });
    }

    Ok(SampleToChunkBox {
        header: *head,
        samples: samples,
    })
}

/// Parse a stsz box.
fn read_stsz<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> Result<SampleSizeBox> {
    let (_, _) = try!(read_fullbox_extra(src));
    let sample_size = try!(be_u32(src));
    let sample_count = try!(be_u32(src));
    let mut sample_sizes = Vec::new();
    if sample_size == 0 {
        for _ in 0..sample_count {
            sample_sizes.push(try!(be_u32(src)));
        }
    }

    Ok(SampleSizeBox {
        header: *head,
        sample_size: sample_size,
        sample_sizes: sample_sizes,
    })
}

/// Parse a stts box.
fn read_stts<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> Result<TimeToSampleBox> {
    let (_, _) = try!(read_fullbox_extra(src));
    let sample_count = try!(be_u32(src));
    let mut samples = Vec::new();
    for _ in 0..sample_count {
        let sample_count = try!(be_u32(src));
        let sample_delta = try!(be_u32(src));
        samples.push(Sample {
            sample_count: sample_count,
            sample_delta: sample_delta,
        });
    }

    Ok(TimeToSampleBox {
        header: *head,
        samples: samples,
    })
}

/// Parse a hdlr box.
fn read_hdlr<T: ReadBytesExt + BufRead>(src: &mut T, head: &BoxHeader) -> Result<HandlerBox> {
    let (_, _) = try!(read_fullbox_extra(src));

    // Skip uninteresting fields.
    try!(skip(src, 4));

    let handler_type = try!(be_fourcc(src));

    // Skip uninteresting fields.
    try!(skip(src, 12));

    // TODO(kinetik): Find a copy of ISO/IEC 14496-1 to work out how strings are encoded.
    // As a hack, just consume the rest of the box.
    try!(skip_remaining_box_content(src, head));

    Ok(HandlerBox {
        header: *head,
        handler_type: handler_type,
    })
}

/// Parse an video description inside an stsd box.
fn read_video_desc<T: ReadBytesExt + BufRead>(src: &mut T, head: &BoxHeader, track: &mut Track) -> Result<SampleEntry> {
    let h = try!(read_box_header(src));
    // TODO(kinetik): encv here also?
    if &h.name.0 != b"avc1" && &h.name.0 != b"avc3" {
        return Err(Error::Unsupported);
    }

    // Skip uninteresting fields.
    try!(skip(src, 6));

    let data_reference_index = try!(be_u16(src));

    // Skip uninteresting fields.
    try!(skip(src, 16));

    let width = try!(be_u16(src));
    let height = try!(be_u16(src));

    // Skip uninteresting fields.
    try!(skip(src, 50));

    // TODO(kinetik): Parse avcC atom?  For now we just stash the data.
    let h = try!(read_box_header(src));
    if &h.name.0 != b"avcC" {
        return Err(Error::InvalidData);
    }
    let mut data: Vec<u8> = vec![0; (h.size - h.offset) as usize];
    let r = try!(src.read(&mut data));
    assert!(r == data.len());
    let avcc = AVCDecoderConfigurationRecord { data: data };

    try!(skip_remaining_box_content(src, head));

    track.mime_type = String::from("video/avc");

    Ok(SampleEntry::Video(VideoSampleEntry {
        data_reference_index: data_reference_index,
        width: width,
        height: height,
        avcc: avcc,
    }))
}

/// Parse an audio description inside an stsd box.
fn read_audio_desc<T: ReadBytesExt + BufRead>(src: &mut T, _: &BoxHeader, track: &mut Track) -> Result<SampleEntry> {
    let h = try!(read_box_header(src));
    // TODO(kinetik): enca here also?
    if &h.name.0 != b"mp4a" {
        return Err(Error::Unsupported);
    }

    // Skip uninteresting fields.
    try!(skip(src, 6));

    let data_reference_index = try!(be_u16(src));

    // Skip uninteresting fields.
    try!(skip(src, 8));

    let channelcount = try!(be_u16(src));
    let samplesize = try!(be_u16(src));

    // Skip uninteresting fields.
    try!(skip(src, 4));

    let samplerate = try!(be_u32(src));

    // TODO(kinetik): Parse esds atom?  For now we just stash the data.
    let h = try!(read_box_header(src));
    if &h.name.0 != b"esds" {
        return Err(Error::InvalidData);
    }
    let (_, _) = try!(read_fullbox_extra(src));
    let mut data: Vec<u8> = vec![0; (h.size - h.offset - 4) as usize];
    let r = try!(src.read(&mut data));
    assert!(r == data.len());
    let esds = ES_Descriptor { data: data };

    // TODO(kinetik): stagefright inspects ESDS to detect MP3 (audio/mpeg).
    track.mime_type = String::from("audio/mp4a-latm");

    Ok(SampleEntry::Audio(AudioSampleEntry {
        data_reference_index: data_reference_index,
        channelcount: channelcount,
        samplesize: samplesize,
        samplerate: samplerate,
        esds: esds,
    }))
}

/// Parse a stsd box.
fn read_stsd<T: ReadBytesExt + BufRead>(src: &mut T, head: &BoxHeader, track: &mut Track) -> Result<SampleDescriptionBox> {
    let (_, _) = try!(read_fullbox_extra(src));

    let description_count = try!(be_u32(src));
    let mut descriptions = Vec::new();

    // TODO(kinetik): check if/when more than one desc per track? do we need to support?
    for _ in 0..description_count {
        let description = match track.track_type {
            TrackType::Video => try!(read_video_desc(src, head, track)),
            TrackType::Audio => try!(read_audio_desc(src, head, track)),
            TrackType::Unknown => SampleEntry::Unknown,
        };
        if track.data.is_none() {
            track.data = Some(description.clone());
        } else {
            return Err(Error::InvalidData);
        }
        descriptions.push(description);
    }

    Ok(SampleDescriptionBox {
        header: *head,
        descriptions: descriptions,
    })
}

/// Skip a number of bytes that we don't care to parse.
fn skip<T: BufRead>(src: &mut T, bytes: usize) -> Result<usize> {
    let mut bytes_to_skip = bytes;
    while bytes_to_skip > 0 {
        let len = try!(src.fill_buf()).len();
        if len == 0 {
            return Err(Error::UnexpectedEOF);
        }
        let discard = cmp::min(len, bytes_to_skip);
        src.consume(discard);
        bytes_to_skip -= discard;
    }
    assert!(bytes_to_skip == 0);
    Ok(bytes)
}

fn media_time_to_ms(time: MediaScaledTime, scale: MediaTimeScale) -> u64 {
    assert!(scale.0 != 0);
    time.0 * 1000000 / scale.0
}

fn track_time_to_ms(time: TrackScaledTime, scale: TrackTimeScale) -> u64 {
    assert!(time.1 == scale.1);
    assert!(scale.0 != 0);
    time.0 * 1000000 / scale.0
}

fn be_i16<T: ReadBytesExt>(src: &mut T) -> byteorder::Result<i16> {
    src.read_i16::<byteorder::BigEndian>()
}

fn be_i32<T: ReadBytesExt>(src: &mut T) -> byteorder::Result<i32> {
    src.read_i32::<byteorder::BigEndian>()
}

fn be_i64<T: ReadBytesExt>(src: &mut T) -> byteorder::Result<i64> {
    src.read_i64::<byteorder::BigEndian>()
}

fn be_u16<T: ReadBytesExt>(src: &mut T) -> byteorder::Result<u16> {
    src.read_u16::<byteorder::BigEndian>()
}

fn be_u32<T: ReadBytesExt>(src: &mut T) -> byteorder::Result<u32> {
    src.read_u32::<byteorder::BigEndian>()
}

fn be_u64<T: ReadBytesExt>(src: &mut T) -> byteorder::Result<u64> {
    src.read_u64::<byteorder::BigEndian>()
}

fn be_fourcc<T: Read>(src: &mut T) -> Result<FourCC> {
    let mut fourcc = [0; 4];
    match src.read(&mut fourcc) {
        // Expect all 4 bytes read.
        Ok(4) => Ok(FourCC(fourcc)),
        // Short read means EOF.
        Ok(_) => Err(Error::UnexpectedEOF),
        // Propagate std::io errors.
        Err(e) => Err(Error::Io(e)),
    }
}

#[cfg(test)]
mod tests {
    use std::io::Cursor;
    use super::*;
    extern crate test_assembler;
    use self::test_assembler::*;

    enum BoxSize {
        Short(u32),
        Long(u64),
        UncheckedShort(u32),
        UncheckedLong(u64),
    }

    fn make_box_raw<F>(size: BoxSize, name: &[u8; 4], func: F) -> Cursor<Vec<u8>>
        where F: Fn(Section) -> Section
    {
        let mut section = Section::new();
        section = match size {
            BoxSize::Short(size) | BoxSize::UncheckedShort(size) => section.B32(size),
            BoxSize::Long(_) | BoxSize::UncheckedLong(_) => section.B32(1),
        };
        section = section.append_bytes(name);
        section = match size {
            // The spec allows the 32-bit size to be 0 to indicate unknown
            // length streams.  It's not clear if this is valid when using a
            // 64-bit size, so prohibit it for now.
            BoxSize::Long(size) => {
                assert!(size > 0);
                section.B64(size)
            }
            BoxSize::UncheckedLong(size) => section.B64(size),
            _ => section,
        };
        section = func(section);
        match size {
            BoxSize::Short(size) => {
                if size > 0 {
                    assert_eq!(size as u64, section.size())
                }
            }
            BoxSize::Long(size) => assert_eq!(size, section.size()),
            _ => (),
        }
        Cursor::new(section.get_contents().unwrap())
    }

    fn make_box<F>(size: u32, name: &[u8; 4], func: F) -> Cursor<Vec<u8>>
        where F: Fn(Section) -> Section
    {
        make_box_raw(BoxSize::Short(size), name, func)
    }

    fn make_fullbox<F>(size: u32, name: &[u8; 4], version: u8, func: F) -> Cursor<Vec<u8>>
        where F: Fn(Section) -> Section
    {
        make_box_raw(BoxSize::Short(size), name, |s| {
            func(s.B8(version)
                  .B8(0)
                  .B8(0)
                  .B8(0))
        })
    }

    #[test]
    fn read_box_header_short() {
        let mut stream = make_box_raw(BoxSize::Short(8), b"test", |s| s);
        let parsed = read_box_header(&mut stream).unwrap();
        assert_eq!(parsed.name, FourCC(*b"test"));
        assert_eq!(parsed.size, 8);
    }

    #[test]
    fn read_box_header_long() {
        let mut stream = make_box_raw(BoxSize::Long(16), b"test", |s| s);
        let parsed = read_box_header(&mut stream).unwrap();
        assert_eq!(parsed.name, FourCC(*b"test"));
        assert_eq!(parsed.size, 16);
    }

    #[test]
    fn read_box_header_short_unknown_size() {
        let mut stream = make_box_raw(BoxSize::Short(0), b"test", |s| s);
        match read_box_header(&mut stream) {
            Err(Error::Unsupported) => (),
            _ => panic!("unexpected result reading box with unknown size"),
        };
    }

    #[test]
    fn read_box_header_short_invalid_size() {
        let mut stream = make_box_raw(BoxSize::UncheckedShort(2), b"test", |s| s);
        match read_box_header(&mut stream) {
            Err(Error::InvalidData) => (),
            _ => panic!("unexpected result reading box with invalid size"),
        };
    }

    #[test]
    fn read_box_header_long_invalid_size() {
        let mut stream = make_box_raw(BoxSize::UncheckedLong(2), b"test", |s| s);
        match read_box_header(&mut stream) {
            Err(Error::InvalidData) => (),
            _ => panic!("unexpected result reading box with invalid size"),
        };
    }

    #[test]
    fn read_ftyp() {
        let mut stream = make_box(24, b"ftyp", |s| {
            s.append_bytes(b"mp42")
             .B32(0) // minor version
             .append_bytes(b"isom")
             .append_bytes(b"mp42")
        });
        let header = read_box_header(&mut stream).unwrap();
        let parsed = super::read_ftyp(&mut stream, &header).unwrap();
        assert_eq!(parsed.header.name, FourCC(*b"ftyp"));
        assert_eq!(parsed.header.size, 24);
        assert_eq!(parsed.major_brand, FourCC(*b"mp42"));
        assert_eq!(parsed.minor_version, 0);
        assert_eq!(parsed.compatible_brands.len(), 2);
        assert_eq!(parsed.compatible_brands[0], FourCC(*b"isom"));
        assert_eq!(parsed.compatible_brands[1], FourCC(*b"mp42"));
    }

    #[test]
    fn read_elst_v0() {
        let mut stream = make_fullbox(28, b"elst", 0, |s| {
            s.B32(1) // list count
              // first entry
             .B32(1234) // duration
             .B32(5678) // time
             .B16(12) // rate integer
             .B16(34) // rate fraction
        });
        let header = read_box_header(&mut stream).unwrap();
        let parsed = super::read_elst(&mut stream, &header).unwrap();
        assert_eq!(parsed.header.name, FourCC(*b"elst"));
        assert_eq!(parsed.header.size, 28);
        assert_eq!(parsed.edits.len(), 1);
        assert_eq!(parsed.edits[0].segment_duration, 1234);
        assert_eq!(parsed.edits[0].media_time, 5678);
        assert_eq!(parsed.edits[0].media_rate_integer, 12);
        assert_eq!(parsed.edits[0].media_rate_fraction, 34);
    }

    #[test]
    fn read_elst_v1() {
        let mut stream = make_fullbox(56, b"elst", 1, |s| {
            s.B32(2) // list count
             // first entry
             .B64(1234) // duration
             .B64(5678) // time
             .B16(12) // rate integer
             .B16(34) // rate fraction
             // second entry
             .B64(1234) // duration
             .B64(5678) // time
             .B16(12) // rate integer
             .B16(34) // rate fraction
        });
        let header = read_box_header(&mut stream).unwrap();
        let parsed = super::read_elst(&mut stream, &header).unwrap();
        assert_eq!(parsed.header.name, FourCC(*b"elst"));
        assert_eq!(parsed.header.size, 56);
        assert_eq!(parsed.edits.len(), 2);
        assert_eq!(parsed.edits[1].segment_duration, 1234);
        assert_eq!(parsed.edits[1].media_time, 5678);
        assert_eq!(parsed.edits[1].media_rate_integer, 12);
        assert_eq!(parsed.edits[1].media_rate_fraction, 34);
    }

    #[test]
    fn read_mdhd_v0() {
        let mut stream = make_fullbox(32, b"mdhd", 0, |s| {
            s.B32(0)
             .B32(0)
             .B32(1234) // timescale
             .B32(5678) // duration
             .B32(0)
        });
        let header = read_box_header(&mut stream).unwrap();
        let parsed = super::read_mdhd(&mut stream, &header).unwrap();
        assert_eq!(parsed.header.name, FourCC(*b"mdhd"));
        assert_eq!(parsed.header.size, 32);
        assert_eq!(parsed.timescale, 1234);
        assert_eq!(parsed.duration, 5678);
    }

    #[test]
    fn read_mdhd_v1() {
        let mut stream = make_fullbox(44, b"mdhd", 1, |s| {
            s.B64(0)
             .B64(0)
             .B32(1234) // timescale
             .B64(5678) // duration
             .B32(0)
        });
        let header = read_box_header(&mut stream).unwrap();
        let parsed = super::read_mdhd(&mut stream, &header).unwrap();
        assert_eq!(parsed.header.name, FourCC(*b"mdhd"));
        assert_eq!(parsed.header.size, 44);
        assert_eq!(parsed.timescale, 1234);
        assert_eq!(parsed.duration, 5678);
    }

    #[test]
    fn read_mdhd_unknown_duration() {
        let mut stream = make_fullbox(32, b"mdhd", 0, |s| {
            s.B32(0)
             .B32(0)
             .B32(1234) // timescale
             .B32(::std::u32::MAX) // duration
             .B32(0)
        });
        let header = read_box_header(&mut stream).unwrap();
        let parsed = super::read_mdhd(&mut stream, &header).unwrap();
        assert_eq!(parsed.header.name, FourCC(*b"mdhd"));
        assert_eq!(parsed.header.size, 32);
        assert_eq!(parsed.timescale, 1234);
        assert_eq!(parsed.duration, ::std::u64::MAX);
    }

    #[test]
    fn read_mdhd_invalid_timescale() {
        let mut stream = make_fullbox(44, b"mdhd", 1, |s| {
            s.B64(0)
             .B64(0)
             .B32(0) // timescale
             .B64(5678) // duration
             .B32(0)
        });
        let header = read_box_header(&mut stream).unwrap();
        let r = super::parse_mdhd(&mut stream, &header, &mut super::Track::new(0));
        assert_eq!(r.is_err(), true);
    }

    #[test]
    fn read_mvhd_v0() {
        let mut stream = make_fullbox(108, b"mvhd", 0, |s| {
            s.B32(0)
             .B32(0)
             .B32(1234)
             .B32(5678)
             .append_repeated(0, 80)
        });
        let header = read_box_header(&mut stream).unwrap();
        let parsed = super::read_mvhd(&mut stream, &header).unwrap();
        assert_eq!(parsed.header.name, FourCC(*b"mvhd"));
        assert_eq!(parsed.header.size, 108);
        assert_eq!(parsed.timescale, 1234);
        assert_eq!(parsed.duration, 5678);
    }

    #[test]
    fn read_mvhd_v1() {
        let mut stream = make_fullbox(120, b"mvhd", 1, |s| {
            s.B64(0)
             .B64(0)
             .B32(1234)
             .B64(5678)
             .append_repeated(0, 80)
        });
        let header = read_box_header(&mut stream).unwrap();
        let parsed = super::read_mvhd(&mut stream, &header).unwrap();
        assert_eq!(parsed.header.name, FourCC(*b"mvhd"));
        assert_eq!(parsed.header.size, 120);
        assert_eq!(parsed.timescale, 1234);
        assert_eq!(parsed.duration, 5678);
    }

    #[test]
    fn read_mvhd_invalid_timescale() {
        let mut stream = make_fullbox(120, b"mvhd", 1, |s| {
            s.B64(0)
             .B64(0)
             .B32(0)
             .B64(5678)
             .append_repeated(0, 80)
        });
        let header = read_box_header(&mut stream).unwrap();
        let r = super::parse_mvhd(&mut stream, &header);
        assert_eq!(r.is_err(), true);
    }

    #[test]
    fn read_mvhd_unknown_duration() {
        let mut stream = make_fullbox(108, b"mvhd", 0, |s| {
            s.B32(0)
             .B32(0)
             .B32(1234)
             .B32(::std::u32::MAX)
             .append_repeated(0, 80)
        });
        let header = read_box_header(&mut stream).unwrap();
        let parsed = super::read_mvhd(&mut stream, &header).unwrap();
        assert_eq!(parsed.header.name, FourCC(*b"mvhd"));
        assert_eq!(parsed.header.size, 108);
        assert_eq!(parsed.timescale, 1234);
        assert_eq!(parsed.duration, ::std::u64::MAX);
    }
}
