//! Module for parsing ISO Base Media Format aka video/mp4 streams.
//! Internal unit tests.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::io::Cursor;
use super::read_mp4;
use super::MediaContext;
use super::Error;
extern crate test_assembler;
use self::test_assembler::*;

use boxes::BoxType;

enum BoxSize {
    Short(u32),
    Long(u64),
    UncheckedShort(u32),
    UncheckedLong(u64),
    Auto,
}

fn make_box<F>(size: BoxSize, name: &[u8; 4], func: F) -> Cursor<Vec<u8>>
    where F: Fn(Section) -> Section
{
    let mut section = Section::new();
    let box_size = Label::new();
    section = match size {
        BoxSize::Short(size) | BoxSize::UncheckedShort(size) => section.B32(size),
        BoxSize::Long(_) | BoxSize::UncheckedLong(_) => section.B32(1),
        BoxSize::Auto => section.B32(&box_size),
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
        BoxSize::Auto => {
            assert!(section.size() <= u32::max_value() as u64,
                    "Tried to use a long box with BoxSize::Auto");
            box_size.set_const(section.size());
        }
        // Skip checking BoxSize::Unchecked* cases.
        _ => (),
    }
    Cursor::new(section.get_contents().unwrap())
}

fn make_fullbox<F>(size: BoxSize, name: &[u8; 4], version: u8, func: F) -> Cursor<Vec<u8>>
    where F: Fn(Section) -> Section
{
    make_box(size, name, |s| {
        func(s.B8(version)
              .B8(0)
              .B8(0)
              .B8(0))
    })
}

#[test]
fn read_box_header_short() {
    let mut stream = make_box(BoxSize::Short(8), b"test", |s| s);
    let header = super::read_box_header(&mut stream).unwrap();
    assert_eq!(header.name, BoxType::UnknownBox(0x74657374)); // "test"
    assert_eq!(header.size, 8);
}

#[test]
fn read_box_header_long() {
    let mut stream = make_box(BoxSize::Long(16), b"test", |s| s);
    let header = super::read_box_header(&mut stream).unwrap();
    assert_eq!(header.name, BoxType::UnknownBox(0x74657374)); // "test"
    assert_eq!(header.size, 16);
}

#[test]
fn read_box_header_short_unknown_size() {
    let mut stream = make_box(BoxSize::Short(0), b"test", |s| s);
    match super::read_box_header(&mut stream) {
        Err(Error::Unsupported(s)) => assert_eq!(s, "unknown sized box"),
        _ => panic!("unexpected result reading box with unknown size"),
    };
}

#[test]
fn read_box_header_short_invalid_size() {
    let mut stream = make_box(BoxSize::UncheckedShort(2), b"test", |s| s);
    match super::read_box_header(&mut stream) {
        Err(Error::InvalidData(s)) => assert_eq!(s, "malformed size"),
        _ => panic!("unexpected result reading box with invalid size"),
    };
}

#[test]
fn read_box_header_long_invalid_size() {
    let mut stream = make_box(BoxSize::UncheckedLong(2), b"test", |s| s);
    match super::read_box_header(&mut stream) {
        Err(Error::InvalidData(s)) => assert_eq!(s, "malformed wide size"),
        _ => panic!("unexpected result reading box with invalid size"),
    };
}

#[test]
fn read_ftyp() {
    let mut stream = make_box(BoxSize::Short(24), b"ftyp", |s| {
        s.append_bytes(b"mp42")
         .B32(0) // minor version
         .append_bytes(b"isom")
         .append_bytes(b"mp42")
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::FileTypeBox);
    assert_eq!(stream.head.size, 24);
    let parsed = super::read_ftyp(&mut stream).unwrap();
    assert_eq!(parsed.major_brand, 0x6d703432); // mp42
    assert_eq!(parsed.minor_version, 0);
    assert_eq!(parsed.compatible_brands.len(), 2);
    assert_eq!(parsed.compatible_brands[0], 0x69736f6d); // isom
    assert_eq!(parsed.compatible_brands[1], 0x6d703432); // mp42
}

#[test]
fn read_truncated_ftyp() {
    // We declare a 24 byte box, but only write 20 bytes.
    let mut stream = make_box(BoxSize::UncheckedShort(24), b"ftyp", |s| {
        s.append_bytes(b"mp42")
            .B32(0) // minor version
            .append_bytes(b"isom")
    });
    let mut context = MediaContext::new();
    match read_mp4(&mut stream, &mut context) {
        Err(Error::UnexpectedEOF) => (),
        Ok(_) => panic!("expected an error result"),
        _ => panic!("expected a different error result"),
    }
}

#[test]
fn read_ftyp_case() {
    // Brands in BMFF are represented as a u32, so it would seem clear that
    // 0x6d703432 ("mp42") is not equal to 0x4d503432 ("MP42"), but some
    // demuxers treat these as case-insensitive strings, e.g. street.mp4's
    // major brand is "MP42".  I haven't seen case-insensitive
    // compatible_brands (which we also test here), but it doesn't seem
    // unlikely given the major_brand behaviour.
    let mut stream = make_box(BoxSize::Auto, b"ftyp", |s| {
        s.append_bytes(b"MP42")
         .B32(0) // minor version
         .append_bytes(b"ISOM")
         .append_bytes(b"MP42")
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::FileTypeBox);
    assert_eq!(stream.head.size, 24);
    let parsed = super::read_ftyp(&mut stream).unwrap();
    assert_eq!(parsed.major_brand, 0x4d503432);
    assert_eq!(parsed.minor_version, 0);
    assert_eq!(parsed.compatible_brands.len(), 2);
    assert_eq!(parsed.compatible_brands[0], 0x49534f4d); // ISOM
    assert_eq!(parsed.compatible_brands[1], 0x4d503432); // MP42
}

#[test]
fn read_elst_v0() {
    let mut stream = make_fullbox(BoxSize::Short(28), b"elst", 0, |s| {
        s.B32(1) // list count
          // first entry
         .B32(1234) // duration
         .B32(5678) // time
         .B16(12) // rate integer
         .B16(34) // rate fraction
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::EditListBox);
    assert_eq!(stream.head.size, 28);
    let parsed = super::read_elst(&mut stream).unwrap();
    assert_eq!(parsed.edits.len(), 1);
    assert_eq!(parsed.edits[0].segment_duration, 1234);
    assert_eq!(parsed.edits[0].media_time, 5678);
    assert_eq!(parsed.edits[0].media_rate_integer, 12);
    assert_eq!(parsed.edits[0].media_rate_fraction, 34);
}

#[test]
fn read_elst_v1() {
    let mut stream = make_fullbox(BoxSize::Short(56), b"elst", 1, |s| {
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
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::EditListBox);
    assert_eq!(stream.head.size, 56);
    let parsed = super::read_elst(&mut stream).unwrap();
    assert_eq!(parsed.edits.len(), 2);
    assert_eq!(parsed.edits[1].segment_duration, 1234);
    assert_eq!(parsed.edits[1].media_time, 5678);
    assert_eq!(parsed.edits[1].media_rate_integer, 12);
    assert_eq!(parsed.edits[1].media_rate_fraction, 34);
}

#[test]
fn read_mdhd_v0() {
    let mut stream = make_fullbox(BoxSize::Short(32), b"mdhd", 0, |s| {
        s.B32(0)
         .B32(0)
         .B32(1234) // timescale
         .B32(5678) // duration
         .B32(0)
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::MediaHeaderBox);
    assert_eq!(stream.head.size, 32);
    let parsed = super::read_mdhd(&mut stream).unwrap();
    assert_eq!(parsed.timescale, 1234);
    assert_eq!(parsed.duration, 5678);
}

#[test]
fn read_mdhd_v1() {
    let mut stream = make_fullbox(BoxSize::Short(44), b"mdhd", 1, |s| {
        s.B64(0)
         .B64(0)
         .B32(1234) // timescale
         .B64(5678) // duration
         .B32(0)
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::MediaHeaderBox);
    assert_eq!(stream.head.size, 44);
    let parsed = super::read_mdhd(&mut stream).unwrap();
    assert_eq!(parsed.timescale, 1234);
    assert_eq!(parsed.duration, 5678);
}

#[test]
fn read_mdhd_unknown_duration() {
    let mut stream = make_fullbox(BoxSize::Short(32), b"mdhd", 0, |s| {
        s.B32(0)
         .B32(0)
         .B32(1234) // timescale
         .B32(::std::u32::MAX) // duration
         .B32(0)
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::MediaHeaderBox);
    assert_eq!(stream.head.size, 32);
    let parsed = super::read_mdhd(&mut stream).unwrap();
    assert_eq!(parsed.timescale, 1234);
    assert_eq!(parsed.duration, ::std::u64::MAX);
}

#[test]
fn read_mdhd_invalid_timescale() {
    let mut stream = make_fullbox(BoxSize::Short(44), b"mdhd", 1, |s| {
        s.B64(0)
         .B64(0)
         .B32(0) // timescale
         .B64(5678) // duration
         .B32(0)
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::MediaHeaderBox);
    assert_eq!(stream.head.size, 44);
    let r = super::parse_mdhd(&mut stream, &mut super::Track::new(0));
    assert_eq!(r.is_err(), true);
}

#[test]
fn read_mvhd_v0() {
    let mut stream = make_fullbox(BoxSize::Short(108), b"mvhd", 0, |s| {
        s.B32(0)
         .B32(0)
         .B32(1234)
         .B32(5678)
         .append_repeated(0, 80)
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::MovieHeaderBox);
    assert_eq!(stream.head.size, 108);
    let parsed = super::read_mvhd(&mut stream).unwrap();
    assert_eq!(parsed.timescale, 1234);
    assert_eq!(parsed.duration, 5678);
}

#[test]
fn read_mvhd_v1() {
    let mut stream = make_fullbox(BoxSize::Short(120), b"mvhd", 1, |s| {
        s.B64(0)
         .B64(0)
         .B32(1234)
         .B64(5678)
         .append_repeated(0, 80)
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::MovieHeaderBox);
    assert_eq!(stream.head.size, 120);
    let parsed = super::read_mvhd(&mut stream).unwrap();
    assert_eq!(parsed.timescale, 1234);
    assert_eq!(parsed.duration, 5678);
}

#[test]
fn read_mvhd_invalid_timescale() {
    let mut stream = make_fullbox(BoxSize::Short(120), b"mvhd", 1, |s| {
        s.B64(0)
         .B64(0)
         .B32(0)
         .B64(5678)
         .append_repeated(0, 80)
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::MovieHeaderBox);
    assert_eq!(stream.head.size, 120);
    let r = super::parse_mvhd(&mut stream);
    assert_eq!(r.is_err(), true);
}

#[test]
fn read_mvhd_unknown_duration() {
    let mut stream = make_fullbox(BoxSize::Short(108), b"mvhd", 0, |s| {
        s.B32(0)
         .B32(0)
         .B32(1234)
         .B32(::std::u32::MAX)
         .append_repeated(0, 80)
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::MovieHeaderBox);
    assert_eq!(stream.head.size, 108);
    let parsed = super::read_mvhd(&mut stream).unwrap();
    assert_eq!(parsed.timescale, 1234);
    assert_eq!(parsed.duration, ::std::u64::MAX);
}

#[test]
fn read_vpcc() {
    let data_length = 12u16;
    let mut stream = make_fullbox(BoxSize::Auto, b"vpcC", 0, |s| {
        s.B8(2)
         .B8(0)
         .B8(0x82)
         .B8(0)
         .B16(data_length)
         .append_repeated(42, data_length as usize)
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::VPCodecConfigurationBox);
    let r = super::read_vpcc(&mut stream);
    assert!(r.is_ok());
}

#[test]
fn read_hdlr() {
    let mut stream = make_fullbox(BoxSize::Short(45), b"hdlr", 0, |s| {
        s.B32(0)
         .append_bytes(b"vide")
         .B32(0)
         .B32(0)
         .B32(0)
         .append_bytes(b"VideoHandler")
         .B8(0) // null-terminate string
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::HandlerBox);
    assert_eq!(stream.head.size, 45);
    let parsed = super::read_hdlr(&mut stream).unwrap();
    assert_eq!(parsed.handler_type, 0x76696465); // vide
}

#[test]
fn read_hdlr_short_name() {
    let mut stream = make_fullbox(BoxSize::Short(33), b"hdlr", 0, |s| {
        s.B32(0)
         .append_bytes(b"vide")
         .B32(0)
         .B32(0)
         .B32(0)
         .B8(0) // null-terminate string
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::HandlerBox);
    assert_eq!(stream.head.size, 33);
    let parsed = super::read_hdlr(&mut stream).unwrap();
    assert_eq!(parsed.handler_type, 0x76696465); // vide
}

#[test]
fn read_hdlr_zero_length_name() {
    let mut stream = make_fullbox(BoxSize::Short(32), b"hdlr", 0, |s| {
        s.B32(0)
         .append_bytes(b"vide")
         .B32(0)
         .B32(0)
         .B32(0)
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::HandlerBox);
    assert_eq!(stream.head.size, 32);
    let parsed = super::read_hdlr(&mut stream).unwrap();
    assert_eq!(parsed.handler_type, 0x76696465); // vide
}

fn flac_streaminfo() -> Vec<u8> {
    vec![
        0x10, 0x00, 0x10, 0x00, 0x00, 0x0a, 0x11, 0x00,
        0x38, 0x32, 0x0a, 0xc4, 0x42, 0xf0, 0x00, 0xc9,
        0xdf, 0xae, 0xb5, 0x66, 0xfc, 0x02, 0x15, 0xa3,
        0xb1, 0x54, 0x61, 0x47, 0x0f, 0xfb, 0x05, 0x00,
        0x33, 0xad,
    ]
}

#[test]
fn read_flac() {
    let mut stream = make_box(BoxSize::Auto, b"fLaC", |s| {
        s.append_repeated(0, 6) // reserved
         .B16(1) // data reference index
         .B32(0) // reserved
         .B32(0) // reserved
         .B16(2) // channel count
         .B16(16) // bits per sample
         .B16(0) // pre_defined
         .B16(0) // reserved
         .B32(44100 << 16) // Sample rate
         .append_bytes(&make_dfla(FlacBlockType::StreamInfo, true,
                                  &flac_streaminfo(), FlacBlockLength::Correct)
         .into_inner())
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    let mut track = super::Track::new(0);
    let r = super::read_audio_sample_entry(&mut stream, &mut track);
    r.unwrap();
}

#[derive(Clone, Copy)]
enum FlacBlockType {
    StreamInfo = 0,
    _Padding = 1,
    _Application = 2,
    _Seektable = 3,
    _Comment = 4,
    _Cuesheet = 5,
    _Picture = 6,
    _Reserved,
    _Invalid = 127,
}

enum FlacBlockLength {
    Correct,
    Incorrect(usize),
}

fn make_dfla(block_type: FlacBlockType, last: bool, data: &Vec<u8>,
             data_length: FlacBlockLength) -> Cursor<Vec<u8>> {
    assert!(data.len() < 1<<24);
    make_fullbox(BoxSize::Auto, b"dfLa", 0, |s| {
        let flag = match last {
            true => 1,
            false => 0,
        };
        let size = match data_length {
            FlacBlockLength::Correct => (data.len() as u32) & 0xffffff,
            FlacBlockLength::Incorrect(size) => {
                assert!(size < 1<<24);
                (size as u32) & 0xffffff
            }
        };
        let block_type = (block_type as u32) & 0x7f;
        s.B32(flag << 31 | block_type << 24 | size)
         .append_bytes(data)
    })
}

#[test]
fn read_dfla() {
    let mut stream = make_dfla(FlacBlockType::StreamInfo, true,
                               &flac_streaminfo(), FlacBlockLength::Correct);
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::FLACSpecificBox);
    let dfla = super::read_dfla(&mut stream).unwrap();
    assert_eq!(dfla.version, 0);
}

#[test]
fn long_flac_metadata() {
    let streaminfo = flac_streaminfo();
    let mut stream = make_dfla(FlacBlockType::StreamInfo, true,
                               &streaminfo,
                               FlacBlockLength::Incorrect(streaminfo.len() + 4));
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::FLACSpecificBox);
    let r = super::read_dfla(&mut stream);
    assert!(r.is_err());
}

#[test]
fn read_opus() {
    let mut stream = make_box(BoxSize::Auto, b"Opus", |s| {
        s.append_repeated(0, 6)
         .B16(1) // data reference index
         .B32(0)
         .B32(0)
         .B16(2) // channel count
         .B16(16) // bits per sample
         .B16(0)
         .B16(0)
         .B32(48000 << 16) // Sample rate is always 48 kHz for Opus.
         .append_bytes(&make_dops().into_inner())
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    let mut track = super::Track::new(0);
    let r = super::read_audio_sample_entry(&mut stream, &mut track);
    assert!(r.is_ok());
}

fn make_dops() -> Cursor<Vec<u8>> {
    make_box(BoxSize::Auto, b"dOps", |s| {
        s.B8(0) // version
         .B8(2) // channel count
         .B16(348) // pre-skip
         .B32(44100) // original sample rate
         .B16(0) // gain
         .B8(0) // channel mapping
    })
}

#[test]
fn read_dops() {
    let mut stream = make_dops();
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    assert_eq!(stream.head.name, BoxType::OpusSpecificBox);
    let r = super::read_dops(&mut stream);
    assert!(r.is_ok());
}

#[test]
fn serialize_opus_header() {
    let opus = super::OpusSpecificBox {
        version: 0,
        output_channel_count: 1,
        pre_skip: 342,
        input_sample_rate: 24000,
        output_gain: 0,
        channel_mapping_family: 0,
        channel_mapping_table: None,
    };
    let mut v = Vec::<u8>::new();
    super::serialize_opus_header(&opus, &mut v).unwrap();
    assert_eq!(v.len(), 19);
    assert_eq!(v, vec![
            0x4f, 0x70, 0x75, 0x73, 0x48,0x65, 0x61, 0x64,
            0x01, 0x01, 0x56, 0x01,
            0xc0, 0x5d, 0x00, 0x00,
            0x00, 0x00, 0x00,
    ]);
    let opus = super::OpusSpecificBox {
        version: 0,
        output_channel_count: 6,
        pre_skip: 152,
        input_sample_rate: 48000,
        output_gain: 0,
        channel_mapping_family: 1,
        channel_mapping_table: Some(super::ChannelMappingTable {
            stream_count: 4,
            coupled_count: 2,
            channel_mapping: vec![0, 4, 1, 2, 3, 5],
        }),
    };
    let mut v = Vec::<u8>::new();
    super::serialize_opus_header(&opus, &mut v).unwrap();
    assert_eq!(v.len(), 27);
    assert_eq!(v, vec![
            0x4f, 0x70, 0x75, 0x73, 0x48,0x65, 0x61, 0x64,
            0x01, 0x06, 0x98, 0x00,
            0x80, 0xbb, 0x00, 0x00,
            0x00, 0x00, 0x01, 0x04, 0x02,
            0x00, 0x04, 0x01, 0x02, 0x03, 0x05,
    ]);
}

#[test]
fn avcc_limit() {
    let mut stream = make_box(BoxSize::Auto, b"avc1", |s| {
        s.append_repeated(0, 6)
         .B16(1)
         .append_repeated(0, 16)
         .B16(320)
         .B16(240)
         .append_repeated(0, 14)
         .append_repeated(0, 32)
         .append_repeated(0, 4)
         .B32(0xffffffff)
         .append_bytes(b"avcC")
         .append_repeated(0, 100)
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    let mut track = super::Track::new(0);
    match super::read_video_sample_entry(&mut stream, &mut track) {
        Err(Error::InvalidData(s)) => assert_eq!(s, "avcC box exceeds BUF_SIZE_LIMIT"),
        Ok(_) => panic!("expected an error result"),
        _ => panic!("expected a different error result"),
    }
}

#[test]
fn esds_limit() {
    let mut stream = make_box(BoxSize::Auto, b"mp4a", |s| {
        s.append_repeated(0, 6)
         .B16(1)
         .B32(0)
         .B32(0)
         .B16(2)
         .B16(16)
         .B16(0)
         .B16(0)
         .B32(48000 << 16)
         .B32(0xffffffff)
         .append_bytes(b"esds")
         .append_repeated(0, 100)
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    let mut track = super::Track::new(0);
    match super::read_audio_sample_entry(&mut stream, &mut track) {
        Err(Error::InvalidData(s)) => assert_eq!(s, "esds box exceeds BUF_SIZE_LIMIT"),
        Ok(_) => panic!("expected an error result"),
        _ => panic!("expected a different error result"),
    }
}

#[test]
fn esds_limit_2() {
    let mut stream = make_box(BoxSize::Auto, b"mp4a", |s| {
        s.append_repeated(0, 6)
         .B16(1)
         .B32(0)
         .B32(0)
         .B16(2)
         .B16(16)
         .B16(0)
         .B16(0)
         .B32(48000 << 16)
         .B32(8)
         .append_bytes(b"esds")
         .append_repeated(0, 4)
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    let mut track = super::Track::new(0);
    match super::read_audio_sample_entry(&mut stream, &mut track) {
        Err(Error::UnexpectedEOF) => (),
        Ok(_) => panic!("expected an error result"),
        _ => panic!("expected a different error result"),
    }
}

#[test]
fn read_elst_zero_entries() {
    let mut stream = make_fullbox(BoxSize::Auto, b"elst", 0, |s| {
        s.B32(0)
         .B16(12)
         .B16(34)
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    match super::read_elst(&mut stream) {
        Err(Error::InvalidData(s)) => assert_eq!(s, "invalid edit count"),
        Ok(_) => panic!("expected an error result"),
        _ => panic!("expected a different error result"),
    }
}

fn make_elst() -> Cursor<Vec<u8>> {
    make_fullbox(BoxSize::Auto, b"elst", 1, |s| {
        s.B32(1)
        // first entry
         .B64(1234) // duration
         .B64(0xffffffffffffffff) // time
         .B16(12) // rate integer
         .B16(34) // rate fraction
    })
}

#[test]
fn read_edts_bogus() {
    // First edit list entry has a media_time of -1, so we expect a second
    // edit list entry to be present to provide a valid media_time.
    let mut stream = make_box(BoxSize::Auto, b"edts", |s| {
        s.append_bytes(&make_elst().into_inner())
    });
    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    let mut track = super::Track::new(0);
    match super::read_edts(&mut stream, &mut track) {
        Err(Error::InvalidData(s)) => assert_eq!(s, "expected additional edit"),
        Ok(_) => panic!("expected an error result"),
        _ => panic!("expected a different error result"),
    }
}

#[test]
fn invalid_pascal_string() {
    // String claims to be 32 bytes long (we provide 33 bytes to account for
    // the 1 byte length prefix).
    let pstr = "\x20xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    let mut stream = Cursor::new(pstr);
    // Reader wants to limit the total read length to 32 bytes, so any
    // returned string must be no longer than 31 bytes.
    let s = super::read_fixed_length_pascal_string(&mut stream, 32).unwrap();
    assert_eq!(s.len(), 31);
}

#[test]
fn skip_padding_in_boxes() {
    // Padding data could be added in the end of these boxes. Parser needs to skip
    // them instead of returning error.
    let box_names = vec![b"stts", b"stsc", b"stsz", b"stco", b"co64", b"stss"];

    for name in box_names {
        let mut stream = make_fullbox(BoxSize::Auto, name, 1, |s| {
            s.append_repeated(0, 100) // add padding data
        });
        let mut iter = super::BoxIter::new(&mut stream);
        let mut stream = iter.next_box().unwrap().unwrap();
        match name {
            b"stts" => {
                super::read_stts(&mut stream).expect("fail to skip padding: stts");
            },
            b"stsc" => {
                super::read_stsc(&mut stream).expect("fail to skip padding: stsc");
            },
            b"stsz" => {
                super::read_stsz(&mut stream).expect("fail to skip padding: stsz");
            },
            b"stco" => {
                super::read_stco(&mut stream).expect("fail to skip padding: stco");
            },
            b"co64" => {
                super::read_co64(&mut stream).expect("fail to skip padding: co64");
            },
            b"stss" => {
                super::read_stss(&mut stream).expect("fail to skip padding: stss");
            },
            _ => (),
        }
    }
}

#[test]
fn skip_padding_in_stsd() {
    // Padding data could be added in the end of stsd boxes. Parser needs to skip
    // them instead of returning error.
    let avc = make_box(BoxSize::Auto, b"avc1", |s| {
        s.append_repeated(0, 6)
         .B16(1)
         .append_repeated(0, 16)
         .B16(320)
         .B16(240)
         .append_repeated(0, 14)
         .append_repeated(0, 32)
         .append_repeated(0, 4)
         .B32(0xffffffff)
         .append_bytes(b"avcC")
         .append_repeated(0, 100)
    }).into_inner();
    let mut stream = make_fullbox(BoxSize::Auto, b"stsd", 0, |s| {
        s.B32(1)
         .append_bytes(avc.as_slice())
         .append_repeated(0, 100) // add padding data
    });

    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    super::read_stsd(&mut stream, &mut super::Track::new(0))
          .expect("fail to skip padding: stsd");
}

#[test]
fn read_qt_wave_atom() {
    let esds = make_fullbox(BoxSize::Auto, b"esds", 0, |s| {
        s.B8(0x03)  // elementary stream descriptor tag
         .B8(0x12)  // esds length
         .append_repeated(0, 2)
         .B8(0x00)  // flags
         .B8(0x04)  // decoder config descriptor tag
         .B8(0x0d)  // dcds length
         .B8(0x6b)  // mp3
         .append_repeated(0, 12)
    }).into_inner();
    let wave = make_box(BoxSize::Auto, b"wave", |s| {
        s.append_bytes(esds.as_slice())
    }).into_inner();
    let mut stream = make_box(BoxSize::Auto, b"mp4a", |s| {
        s.append_repeated(0, 6)
         .B16(1)    // data_reference_count
         .B16(1)    // verion: qt -> 1
         .append_repeated(0, 6)
         .B16(2)
         .B16(16)
         .append_repeated(0, 4)
         .B32(48000 << 16)
         .append_repeated(0, 16)
         .append_bytes(wave.as_slice())
    });

    let mut iter = super::BoxIter::new(&mut stream);
    let mut stream = iter.next_box().unwrap().unwrap();
    let mut track = super::Track::new(0);
    super::read_audio_sample_entry(&mut stream, &mut track)
          .expect("fail to read qt wave atom");
    assert_eq!(track.codec_type, super::CodecType::MP3);
}
