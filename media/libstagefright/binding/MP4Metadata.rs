// Module for parsing ISO Base Media Format aka video/mp4 streams.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::fmt;

/// Expose C api wrapper.
pub mod capi;
// FIXME: We can 'pub use capi::*' in rustc 1.5 and later.
pub use capi::{mp4parse_new, mp4parse_free, mp4parse_read};

#[derive(Clone, Copy, Eq, PartialEq)]
pub struct FourCC(pub u32);

impl fmt::Debug for FourCC {
  fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
    write!(f, "'{}'", fourcc_to_string(*self))
  }
}

/// Basic ISO box structure.
#[derive(Debug)]
pub struct BoxHeader {
    /// Four character box type
    pub name: FourCC,
    /// Size of the box in bytes
    pub size: u64,
    /// Offset to the start of the contained data (or header size).
    pub offset: u64,
}

/// File type box 'ftyp'.
#[derive(Debug)]
pub struct FileTypeBox {
    name: FourCC,
    size: u64,
    major_brand: FourCC,
    minor_version: u32,
    compatible_brands: Vec<FourCC>,
}

/// Movie header box 'mvhd'.
#[derive(Debug)]
pub struct MovieHeaderBox {
    pub name: FourCC,
    pub size: u64,
    pub timescale: u32,
    pub duration: u64,
    // Ignore other fields.
}

/// Track header box 'tkhd'
#[derive(Debug)]
pub struct TrackHeaderBox {
    pub name: FourCC,
    pub size: u64,
    pub track_id: u32,
    pub enabled: bool,
    pub duration: u64,
    pub width: u32,
    pub height: u32,
}

/// Edit list box 'elst'
#[derive(Debug)]
pub struct EditListBox {
    name: FourCC,
    size: u64,
    edits: Vec<Edit>,
}

#[derive(Debug)]
pub struct Edit {
    segment_duration: u64,
    media_time: i64,
    media_rate_integer: i16,
    media_rate_fraction: i16,
}

/// Media header box 'mdhd'
#[derive(Debug)]
pub struct MediaHeaderBox {
    name: FourCC,
    size: u64,
    timescale: u32,
    duration: u64,
}

// Chunk offset box 'stco' or 'co64'
#[derive(Debug)]
pub struct ChunkOffsetBox {
    name: FourCC,
    size: u64,
    offsets: Vec<u64>,
}

// Sync sample box 'stss'
#[derive(Debug)]
pub struct SyncSampleBox {
    name: FourCC,
    size: u64,
    samples: Vec<u32>,
}

// Sample to chunk box 'stsc'
#[derive(Debug)]
pub struct SampleToChunkBox {
    name: FourCC,
    size: u64,
    samples: Vec<SampleToChunk>,
}

#[derive(Debug)]
pub struct SampleToChunk {
    first_chunk: u32,
    samples_per_chunk: u32,
    sample_description_index: u32,
}

// Sample size box 'stsz'
#[derive(Debug)]
pub struct SampleSizeBox {
    name: FourCC,
    size: u64,
    sample_size: u32,
    sample_sizes: Vec<u32>,
}

// Time to sample box 'stts'
#[derive(Debug)]
pub struct TimeToSampleBox {
    name: FourCC,
    size: u64,
    samples: Vec<Sample>,
}

#[derive(Debug)]
pub struct Sample {
    sample_count: u32,
    sample_delta: u32,
}

// Handler reference box 'hdlr'
#[derive(Debug)]
pub struct HandlerBox {
    name: FourCC,
    size: u64,
    handler_type: FourCC,
}

// Sample description box 'stsd'
#[derive(Debug)]
pub struct SampleDescriptionBox {
    name: FourCC,
    size: u64,
    descriptions: Vec<SampleEntry>,
}

#[allow(dead_code)]
#[derive(Debug)]
enum SampleEntry {
    Audio {
        data_reference_index: u16,
        channelcount: u16,
        samplesize: u16,
        samplerate: u32,
        esds: ES_Descriptor,
    },
    Video {
        data_reference_index: u16,
        width: u16,
        height: u16,
        avcc: AVCDecoderConfigurationRecord,
    },
}

#[allow(dead_code)]
#[derive(Debug)]
pub struct AVCDecoderConfigurationRecord {
    data: Vec<u8>,
}

#[allow(non_camel_case_types)]
#[allow(dead_code)]
#[derive(Debug)]
pub struct ES_Descriptor {
    data: Vec<u8>,
}

/// Internal data structures.
#[derive(Debug)]
pub struct MediaContext {
    tracks: Vec<Track>,
}

impl MediaContext {
    pub fn new() -> MediaContext {
        MediaContext {
            tracks: Vec::new(),
        }
    }
}

#[derive(Debug)]
enum TrackType {
    Video,
    Audio
}

#[derive(Debug)]
pub struct Track {
    track_type: TrackType,
}

mod byteorder; // 'extern crate' upstream.
use byteorder::{BigEndian, ReadBytesExt};
use std::io::{Read, BufRead, Take};
use std::io::Cursor;
use std::cmp;

/// Parse a box out of a data buffer.
pub fn read_box_header<T: ReadBytesExt>(src: &mut T) -> byteorder::Result<BoxHeader> {
    let size32 = try!(be_u32(src));
    let name = FourCC(try!(be_u32(src)));
    let size = match size32 {
        0 => panic!("unknown box size not implemented"),
        1 => {
            let size64 = try!(be_u64(src));
            assert!(size64 >= 16);
            size64
        },
        2 ... 7 => panic!("invalid box size"),
        _ => size32 as u64,
    };
    let offset = match size32 {
        1 => 4 + 4 + 8,
        _ => 4 + 4,
    };
    assert!(offset <= size);
    Ok(BoxHeader{
      name: name,
      size: size,
      offset: offset,
    })
}

/// Parse the extra header fields for a full box.
fn read_fullbox_extra<T: ReadBytesExt>(src: &mut T) -> byteorder::Result<(u8, u32)> {
    let version = try!(src.read_u8());
    let flags_a = try!(src.read_u8());
    let flags_b = try!(src.read_u8());
    let flags_c = try!(src.read_u8());
    Ok((version, (flags_a as u32) << 16 |
                 (flags_b as u32) <<  8 |
                 (flags_c as u32)))
}

/// Skip over the entire contents of a box.
pub fn skip_box_content<T: BufRead> (src: &mut T, header: &BoxHeader) -> byteorder::Result<usize> {
    skip(src, (header.size - header.offset) as usize)
}

/// Skip over the remaining contents of a box.
pub fn skip_remaining_box_content<T: BufRead> (src: &mut T, header: &BoxHeader) -> byteorder::Result<()> {
    match skip(src, (header.size - header.offset) as usize) {
        Ok(_) | Err(byteorder::Error::UnexpectedEOF) => Ok(()),
        e @ _ => Err(e.err().unwrap())
    }
}

/// Helper to construct a Take over the contents of a box.
fn limit<'a, T: Read>(f: &'a mut T, h: &BoxHeader) -> Take<&'a mut T> {
    f.take(h.size - h.offset)
}

/// Helper to construct a Cursor over the contents of a box.
fn recurse<T: Read>(f: &mut T, h: &BoxHeader, context: &mut MediaContext) -> byteorder::Result<()> {
    use std::error::Error;
    println!("{:?} -- recursing", h);
    // FIXME: I couldn't figure out how to do this without copying.
    // We use Seek on the Read we return in skip_box_content, but
    // that trait isn't implemented for a Take like our limit()
    // returns. Slurping the buffer and wrapping it in a Cursor
    // functions as a work around.
    let buf: Vec<u8> = f
        .bytes()
        .map(|u| u.unwrap())
        .collect();
    let mut content = Cursor::new(buf);
    loop {
        match read_box(&mut content, context) {
            Ok(_) => {},
            Err(byteorder::Error::UnexpectedEOF) => {
                // byteorder returns EOF at the end of the buffer.
                // This isn't an error for us, just an signal to
                // stop recursion.
                println!("Caught byteorder::Error::UnexpectedEOF");
                break;
            },
            Err(byteorder::Error::Io(e)) => {
                println!("I/O Error '{:?}' reading box: {:?}",
                         e.kind(), e.description());
                return Err(byteorder::Error::Io(e));
            },
        }
    }
    assert!(content.position() == h.size - h.offset);
    println!("{:?} -- end", h);
    Ok(())
}

/// Read the contents of a box, including sub boxes.
/// Metadata is accumulated in the passed-through MediaContext struct.
pub fn read_box<T: BufRead>(f: &mut T, context: &mut MediaContext) -> byteorder::Result<()> {
    read_box_header(f).and_then(|h| {
        let mut content = limit(f, &h);
        match &fourcc_to_string(h.name)[..] {
            "ftyp" => {
                let ftyp = try!(read_ftyp(&mut content, &h));
                println!("{:?}", ftyp);
            },
            "moov" => try!(recurse(&mut content, &h, context)),
            "mvhd" => {
                let mvhd = try!(read_mvhd(&mut content, &h));
                println!("  {:?}", mvhd);
            },
            "trak" => try!(recurse(&mut content, &h, context)),
            "tkhd" => {
                let tkhd = try!(read_tkhd(&mut content, &h));
                println!("  {:?}", tkhd);
            },
            "edts" => try!(recurse(&mut content, &h, context)),
            "elst" => {
                let elst = try!(read_elst(&mut content, &h));
                println!("  {:?}", elst);
            },
            "mdia" => try!(recurse(&mut content, &h, context)),
            "mdhd" => {
                let mdhd = try!(read_mdhd(&mut content, &h));
                println!("  {:?}", mdhd);
            },
            "minf" => try!(recurse(&mut content, &h, context)),
            "stbl" => try!(recurse(&mut content, &h, context)),
            "stco" => {
                let stco = try!(read_stco(&mut content, &h));
                println!("  {:?}", stco);
            },
            "co64" => {
                let co64 = try!(read_co64(&mut content, &h));
                println!("  {:?}", co64);
            },
            "stss" => {
                let stss = try!(read_stss(&mut content, &h));
                println!("  {:?}", stss);
            },
            "stsc" => {
                let stsc = try!(read_stsc(&mut content, &h));
                println!("  {:?}", stsc);
            },
            "stsz" => {
                let stsz = try!(read_stsz(&mut content, &h));
                println!("  {:?}", stsz);
            },
            "stts" => {
                let stts = try!(read_stts(&mut content, &h));
                println!("  {:?}", stts);
            },
            "hdlr" => {
                let hdlr = try!(read_hdlr(&mut content, &h));
                let track_type = match &fourcc_to_string(hdlr.handler_type)[..] {
                    "vide" => Some(TrackType::Video),
                    "soun" => Some(TrackType::Audio),
                    _ => None
                };
                // Save track types with recognized types.
                match track_type {
                    Some(track_type) =>
                         context.tracks.push(Track { track_type: track_type }),
                    None => println!("unknown track type!"),
                };
                println!("  {:?}", hdlr);
            },
            "stsd" => {
                let track = &context.tracks[context.tracks.len() - 1];
                let stsd = try!(read_stsd(&mut content, &h, &track));
                println!("  {:?}", stsd);
            },
            _ => {
                // Skip the contents of unknown chunks.
                println!("{:?} (skipped)", h);
                try!(skip_box_content(&mut content, &h));
            },
        };
        assert!(content.limit() == 0);
        println!("read_box context: {:?}", context);
        Ok(()) // and_then needs a Result to return.
    })
}

/// Parse an ftyp box.
pub fn read_ftyp<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> byteorder::Result<FileTypeBox> {
    let major = FourCC(try!(be_u32(src)));
    let minor = try!(be_u32(src));
    let brand_count = (head.size - 8 - 8) / 4;
    let mut brands = Vec::new();
    for _ in 0..brand_count {
        brands.push(FourCC(try!(be_u32(src))));
    }
    Ok(FileTypeBox{
        name: head.name,
        size: head.size,
        major_brand: major,
        minor_version: minor,
        compatible_brands: brands,
    })
}

/// Parse an mvhd box.
pub fn read_mvhd<T: ReadBytesExt + BufRead>(src: &mut T, head: &BoxHeader) -> byteorder::Result<MovieHeaderBox> {
    let (version, _) = try!(read_fullbox_extra(src));
    match version {
        // 64 bit creation and modification times.
        1 => { try!(skip(src, 16)); },
        // 32 bit creation and modification times.
        0 => { try!(skip(src, 8)); },
        _ => panic!("invalid mhdr version"),
    }
    let timescale = try!(be_u32(src));
    let duration = match version {
        1 => try!(be_u64(src)),
        0 => try!(be_u32(src)) as u64,
        _ => panic!("invalid mhdr version"),
    };
    // Skip remaining fields.
    try!(skip(src, 80));
    Ok(MovieHeaderBox {
        name: head.name,
        size: head.size,
        timescale: timescale,
        duration: duration,
    })
}

/// Parse a tkhd box.
pub fn read_tkhd<T: ReadBytesExt + BufRead>(src: &mut T, head: &BoxHeader) -> byteorder::Result<TrackHeaderBox> {
    let (version, flags) = try!(read_fullbox_extra(src));
    let disabled = flags & 0x1u32 == 0 || flags & 0x2u32 == 0;
    match version {
        // 64 bit creation and modification times.
        1 => { try!(skip(src, 16)); },
        // 32 bit creation and modification times.
        0 => { try!(skip(src, 8)); },
        _ => panic!("invalid tkhd version"),
    }
    let track_id = try!(be_u32(src));
    try!(skip(src, 4));
    let duration = match version {
        1 => try!(be_u64(src)),
        0 => try!(be_u32(src)) as u64,
        _ => panic!("invalid tkhd version"),
    };
    // Skip uninteresting fields.
    try!(skip(src, 52));
    let width = try!(be_u32(src));
    let height = try!(be_u32(src));
    Ok(TrackHeaderBox {
        name: head.name,
        size: head.size,
        track_id: track_id,
        enabled: !disabled,
        duration: duration,
        width: width,
        height: height,
    })
}

/// Parse a elst box.
pub fn read_elst<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> byteorder::Result<EditListBox> {
    let (version, _) = try!(read_fullbox_extra(src));
    let edit_count = try!(be_u32(src));
    let mut edits = Vec::new();
    for _ in 0..edit_count {
        let (segment_duration, media_time) = match version {
            1 => {
                // 64 bit segment duration and media times.
                (try!(be_u64(src)),
                 try!(be_i64(src)))
            },
            0 => {
                // 32 bit segment duration and media times.
                (try!(be_u32(src)) as u64,
                 try!(be_i32(src)) as i64)
            },
            _ => panic!("invalid elst version"),
        };
        let media_rate_integer = try!(be_i16(src));
        let media_rate_fraction = try!(be_i16(src));
        edits.push(Edit{
            segment_duration: segment_duration,
            media_time: media_time,
            media_rate_integer: media_rate_integer,
            media_rate_fraction: media_rate_fraction,
        })
    }

    Ok(EditListBox{
        name: head.name,
        size: head.size,
        edits: edits
    })
}

/// Parse a mdhd box.
pub fn read_mdhd<T: ReadBytesExt + BufRead>(src: &mut T, head: &BoxHeader) -> byteorder::Result<MediaHeaderBox> {
    let (version, _) = try!(read_fullbox_extra(src));
    let (timescale, duration) = match version {
        1 => {
            // Skip 64-bit creation and modification times.
            try!(skip(src, 16));

            // 64 bit duration.
            (try!(be_u32(src)),
             try!(be_u64(src)))
        },
        0 => {
            // Skip 32-bit creation and modification times.
            try!(skip(src, 8));

            // 32 bit duration.
            (try!(be_u32(src)),
             try!(be_u32(src)) as u64)
        },
        _ => panic!("invalid mdhd version"),
    };

    // Skip uninteresting fields.
    try!(skip(src, 4));

    Ok(MediaHeaderBox{
        name: head.name,
        size: head.size,
        timescale: timescale,
        duration: duration,
    })
}

/// Parse a stco box.
pub fn read_stco<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> byteorder::Result<ChunkOffsetBox> {
    let (_, _) = try!(read_fullbox_extra(src));
    let offset_count = try!(be_u32(src));
    let mut offsets = Vec::new();
    for _ in 0..offset_count {
        offsets.push(try!(be_u32(src)) as u64);
    }

    Ok(ChunkOffsetBox{
        name: head.name,
        size: head.size,
        offsets: offsets,
    })
}

/// Parse a stco box.
pub fn read_co64<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> byteorder::Result<ChunkOffsetBox> {
    let (_, _) = try!(read_fullbox_extra(src));
    let offset_count = try!(be_u32(src));
    let mut offsets = Vec::new();
    for _ in 0..offset_count {
        offsets.push(try!(be_u64(src)));
    }

    Ok(ChunkOffsetBox{
        name: head.name,
        size: head.size,
        offsets: offsets,
    })
}

/// Parse a stss box.
pub fn read_stss<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> byteorder::Result<SyncSampleBox> {
    let (_, _) = try!(read_fullbox_extra(src));
    let sample_count = try!(be_u32(src));
    let mut samples = Vec::new();
    for _ in 0..sample_count {
        samples.push(try!(be_u32(src)));
    }

    Ok(SyncSampleBox{
        name: head.name,
        size: head.size,
        samples: samples,
    })
}

/// Parse a stsc box.
pub fn read_stsc<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> byteorder::Result<SampleToChunkBox> {
    let (_, _) = try!(read_fullbox_extra(src));
    let sample_count = try!(be_u32(src));
    let mut samples = Vec::new();
    for _ in 0..sample_count {
        let first_chunk = try!(be_u32(src));
        let samples_per_chunk = try!(be_u32(src));
        let sample_description_index = try!(be_u32(src));
        samples.push(SampleToChunk{
            first_chunk: first_chunk,
            samples_per_chunk: samples_per_chunk,
            sample_description_index: sample_description_index
        });
    }

    Ok(SampleToChunkBox{
        name: head.name,
        size: head.size,
        samples: samples,
    })
}

/// Parse a stsz box.
pub fn read_stsz<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> byteorder::Result<SampleSizeBox> {
    let (_, _) = try!(read_fullbox_extra(src));
    let sample_size = try!(be_u32(src));
    let sample_count = try!(be_u32(src));
    let mut sample_sizes = Vec::new();
    if sample_size == 0 {
        for _ in 0..sample_count {
            sample_sizes.push(try!(be_u32(src)));
        }
    }

    Ok(SampleSizeBox{
        name: head.name,
        size: head.size,
        sample_size: sample_size,
        sample_sizes: sample_sizes,
    })
}

/// Parse a stts box.
pub fn read_stts<T: ReadBytesExt>(src: &mut T, head: &BoxHeader) -> byteorder::Result<TimeToSampleBox> {
    let (_, _) = try!(read_fullbox_extra(src));
    let sample_count = try!(be_u32(src));
    let mut samples = Vec::new();
    for _ in 0..sample_count {
        let sample_count = try!(be_u32(src));
        let sample_delta = try!(be_u32(src));
        samples.push(Sample{
            sample_count: sample_count,
            sample_delta: sample_delta,
        });
    }

    Ok(TimeToSampleBox{
        name: head.name,
        size: head.size,
        samples: samples,
    })
}

/// Parse a hdlr box.
pub fn read_hdlr<T: ReadBytesExt + BufRead>(src: &mut T, head: &BoxHeader) -> byteorder::Result<HandlerBox> {
    let (_, _) = try!(read_fullbox_extra(src));

    // Skip uninteresting fields.
    try!(skip(src, 4));

    let handler_type = FourCC(try!(be_u32(src)));

    // Skip uninteresting fields.
    try!(skip(src, 12));

    // TODO(kinetik): Find a copy of ISO/IEC 14496-1 to work out how strings are encoded.
    // As a hack, just consume the rest of the box.
    try!(skip_remaining_box_content(src, head));

    Ok(HandlerBox{
        name: head.name,
        size: head.size,
        handler_type: handler_type,
    })
}

/// Parse a stsd box.
pub fn read_stsd<T: ReadBytesExt + BufRead>(src: &mut T, head: &BoxHeader, track: &Track) -> byteorder::Result<SampleDescriptionBox> {
    let (_, _) = try!(read_fullbox_extra(src));

    let description_count = try!(be_u32(src));
    let mut descriptions = Vec::new();

    for _ in 0..description_count {
        let description = match track.track_type {
            TrackType::Video => {
                let h = try!(read_box_header(src));
                // TODO(kinetik): avc3 and encv here also?
                if fourcc_to_string(h.name) != "avc1" {
                    panic!("unsupported SampleEntry::Video subtype");
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
                if fourcc_to_string(h.name) != "avcC" {
                    panic!("expected avcC atom inside avc1");
                }
                let mut data: Vec<u8> = vec![0; (h.size - h.offset) as usize];
                let r = try!(src.read(&mut data));
                assert!(r == data.len());
                let avcc = AVCDecoderConfigurationRecord { data: data };

                try!(skip_remaining_box_content(src, head));

                SampleEntry::Video {
                    data_reference_index: data_reference_index,
                    width: width,
                    height: height,
                    avcc: avcc,
                }
            },
            TrackType::Audio => {
                let h = try!(read_box_header(src));
                // TODO(kinetik): enca here also?
                if fourcc_to_string(h.name) != "mp4a" {
                    panic!("unsupported SampleEntry::Audio subtype");
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
                if fourcc_to_string(h.name) != "esds" {
                    panic!("expected esds atom inside mp4a");
                }
                let (_, _) = try!(read_fullbox_extra(src));
                let mut data: Vec<u8> = vec![0; (h.size - h.offset - 4) as usize];
                let r = try!(src.read(&mut data));
                assert!(r == data.len());
                let esds = ES_Descriptor { data: data };

                SampleEntry::Audio {
                    data_reference_index: data_reference_index,
                    channelcount: channelcount,
                    samplesize: samplesize,
                    samplerate: samplerate,
                    esds: esds,
                }
            },
        };
        descriptions.push(description);
    }

    Ok(SampleDescriptionBox{
        name: head.name,
        size: head.size,
        descriptions: descriptions,
    })
}

/// Convert the iso box type or other 4-character value to a string.
fn fourcc_to_string(name: FourCC) -> String {
    let u32_to_vec = |u| {
        vec!((u >> 24 & 0xffu32) as u8,
             (u >> 16 & 0xffu32) as u8,
             (u >>  8 & 0xffu32) as u8,
             (u & 0xffu32) as u8)
    };
    let name_bytes = u32_to_vec(name.0);
    String::from_utf8_lossy(&name_bytes).into_owned()
}

/// Skip a number of bytes that we don't care to parse.
fn skip<T: BufRead>(src: &mut T, bytes: usize) -> byteorder::Result<usize> {
    let mut bytes_to_skip = bytes;
    while bytes_to_skip > 0 {
        let len = {
            let buf = src.fill_buf().unwrap();
            buf.len()
        };
        if len == 0 {
            return Err(byteorder::Error::UnexpectedEOF)
        }
        let discard = cmp::min(len, bytes_to_skip);
        src.consume(discard);
        bytes_to_skip -= discard;
    }
    assert!(bytes_to_skip == 0);
    Ok(bytes)
}

fn be_i16<T: ReadBytesExt>(src: &mut T) -> byteorder::Result<i16> {
    src.read_i16::<BigEndian>()
}

fn be_i32<T: ReadBytesExt>(src: &mut T) -> byteorder::Result<i32> {
    src.read_i32::<BigEndian>()
}

fn be_i64<T: ReadBytesExt>(src: &mut T) -> byteorder::Result<i64> {
    src.read_i64::<BigEndian>()
}

fn be_u16<T: ReadBytesExt>(src: &mut T) -> byteorder::Result<u16> {
    src.read_u16::<BigEndian>()
}

fn be_u32<T: ReadBytesExt>(src: &mut T) -> byteorder::Result<u32> {
    src.read_u32::<BigEndian>()
}

fn be_u64<T: ReadBytesExt>(src: &mut T) -> byteorder::Result<u64> {
    src.read_u64::<BigEndian>()
}

#[test]
fn test_read_box_header() {
    use std::io::Cursor;
    use std::io::Write;
    let mut test: Vec<u8> = vec![0, 0, 0, 8]; // minimal box length
    write!(&mut test, "test").unwrap(); // box type
    let mut stream = Cursor::new(test);
    let parsed = read_box_header(&mut stream).unwrap();
    assert_eq!(parsed.name, FourCC(1952805748));
    assert_eq!(parsed.size, 8);
    println!("box {:?}", parsed);
}

#[test]
fn test_read_box_header_long() {
    use std::io::Cursor;
    let mut test: Vec<u8> = vec![0, 0, 0, 1]; // long box extension code
    test.extend("long".to_string().into_bytes()); // box type
    test.extend(vec![0, 0, 0, 0, 0, 0, 16, 0]); // 64 bit size
    // Skip generating box content.
    let mut stream = Cursor::new(test);
    let parsed = read_box_header(&mut stream).unwrap();
    assert_eq!(parsed.name, FourCC(1819242087));
    assert_eq!(parsed.size, 4096);
    println!("box {:?}", parsed);
}

#[test]
fn test_read_ftyp() {
    use std::io::Cursor;
    use std::io::Write;
    let mut test: Vec<u8> = vec![0, 0, 0, 24]; // size
    write!(&mut test, "ftyp").unwrap(); // type
    write!(&mut test, "mp42").unwrap(); // major brand
    test.extend(vec![0, 0, 0, 0]);      // minor version
    write!(&mut test, "isom").unwrap(); // compatible brands...
    write!(&mut test, "mp42").unwrap();
    assert_eq!(test.len(), 24);

    let mut stream = Cursor::new(test);
    let header = read_box_header(&mut stream).unwrap();
    let parsed = read_ftyp(&mut stream, &header).unwrap();
    assert_eq!(parsed.name, FourCC(1718909296));
    assert_eq!(parsed.size, 24);
    assert_eq!(parsed.major_brand, FourCC(1836069938));
    assert_eq!(parsed.minor_version, 0);
    assert_eq!(parsed.compatible_brands.len(), 2);
    assert_eq!(parsed.compatible_brands[0], FourCC(1769172845));
    assert_eq!(fourcc_to_string(parsed.compatible_brands[1]), "mp42");
    println!("box {:?}", parsed);
}

#[test]
fn test_read_elst_v0() {
    use std::io::Cursor;
    use std::io::Write;
    let mut test: Vec<u8> = vec![0, 0, 0, 28]; // size
    write!(&mut test, "elst").unwrap(); // type
    test.extend(vec![0, 0, 0, 0]); // fullbox
    test.extend(vec![0, 0, 0, 1]); // count
    test.extend(vec![1, 2, 3, 4,
                     5, 6, 7, 8,
                     9, 10,
                     11, 12]);
    assert_eq!(test.len(), 28);

    let mut stream = Cursor::new(test);
    let header = read_box_header(&mut stream).unwrap();
    let parsed = read_elst(&mut stream, &header).unwrap();
    assert_eq!(parsed.name, FourCC(1701606260));
    assert_eq!(parsed.size, 28);
    assert_eq!(parsed.edits.len(), 1);
    assert_eq!(parsed.edits[0].segment_duration, 16909060);
    assert_eq!(parsed.edits[0].media_time, 84281096);
    assert_eq!(parsed.edits[0].media_rate_integer, 2314);
    assert_eq!(parsed.edits[0].media_rate_fraction, 2828);
    println!("box {:?}", parsed);
}

#[test]
fn test_read_elst_v1() {
    use std::io::Cursor;
    use std::io::Write;
    let mut test: Vec<u8> = vec![0, 0, 0, 56]; // size
    write!(&mut test, "elst").unwrap(); // type
    test.extend(vec![1, 0, 0, 0]); // fullbox
    test.extend(vec![0, 0, 0, 2]); // count
    test.extend(vec![1, 2, 3, 4, 1, 2, 3, 4,
                     5, 6, 7, 8, 5, 6, 7, 8,
                     9, 10,
                     11, 12]);
    test.extend(vec![1, 2, 3, 4, 1, 2, 3, 4,
                     5, 6, 7, 8, 5, 6, 7, 8,
                     9, 10,
                     11, 12]);
    assert_eq!(test.len(), 56);

    let mut stream = Cursor::new(test);
    let header = read_box_header(&mut stream).unwrap();
    let parsed = read_elst(&mut stream, &header).unwrap();
    assert_eq!(parsed.name, FourCC(1701606260));
    assert_eq!(parsed.size, 56);
    assert_eq!(parsed.edits.len(), 2);
    assert_eq!(parsed.edits[1].segment_duration, 72623859723010820);
    assert_eq!(parsed.edits[1].media_time, 361984551075317512);
    assert_eq!(parsed.edits[1].media_rate_integer, 2314);
    assert_eq!(parsed.edits[1].media_rate_fraction, 2828);
    println!("box {:?}", parsed);
}

#[test]
fn test_read_mdhd_v0() {
    use std::io::Cursor;
    use std::io::Write;
    let mut test: Vec<u8> = vec![0, 0, 0, 32]; // size
    write!(&mut test, "mdhd").unwrap(); // type
    test.extend(vec![0, 0, 0, 0]); // fullbox
    test.extend(vec![0, 0, 0, 0,
                     0, 0, 0, 0,
                     1, 2, 3, 4,
                     5, 6, 7, 8,
                     0, 0, 0, 0]);
    assert_eq!(test.len(), 32);

    let mut stream = Cursor::new(test);
    let header = read_box_header(&mut stream).unwrap();
    let parsed = read_mdhd(&mut stream, &header).unwrap();
    assert_eq!(parsed.name, FourCC(1835296868));
    assert_eq!(parsed.size, 32);
    assert_eq!(parsed.timescale, 16909060);
    assert_eq!(parsed.duration, 84281096);
    println!("box {:?}", parsed);
}

#[test]
fn test_read_mdhd_v1() {
    use std::io::Cursor;
    use std::io::Write;
    let mut test: Vec<u8> = vec![0, 0, 0, 44]; // size
    write!(&mut test, "mdhd").unwrap(); // type
    test.extend(vec![1, 0, 0, 0]); // fullbox
    test.extend(vec![0, 0, 0, 0, 0, 0, 0, 0,
                     0, 0, 0, 0, 0, 0, 0, 0,
                     1, 2, 3, 4,
                     5, 6, 7, 8, 5, 6, 7, 8,
                     0, 0, 0, 0]);
    assert_eq!(test.len(), 44);

    let mut stream = Cursor::new(test);
    let header = read_box_header(&mut stream).unwrap();
    let parsed = read_mdhd(&mut stream, &header).unwrap();
    assert_eq!(parsed.name, FourCC(1835296868));
    assert_eq!(parsed.size, 44);
    assert_eq!(parsed.timescale, 16909060);
    assert_eq!(parsed.duration, 361984551075317512);
    println!("box {:?}", parsed);
}
