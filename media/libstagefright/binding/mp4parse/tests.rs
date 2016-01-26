//! Module for parsing ISO Base Media Format aka video/mp4 streams.
//! Internal unit tests.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::io::Cursor;
use super::*;
extern crate test_assembler;
use self::test_assembler::*;

enum BoxSize {
    Short(u32),
    Long(u64),
    UncheckedShort(u32),
    UncheckedLong(u64),
    Auto,
}

fn make_box_raw<F>(size: BoxSize, name: &[u8; 4], func: F) -> Cursor<Vec<u8>>
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
        },
        // Skip checking BoxSize::Unchecked* cases.
        _ => (),
    }
    Cursor::new(section.get_contents().unwrap())
}

fn make_box<F>(size: u32, name: &[u8; 4], func: F) -> Cursor<Vec<u8>>
    where F: Fn(Section) -> Section
{
    make_box_raw(BoxSize::Short(size), name, func)
}

fn make_fullbox<F>(size: BoxSize, name: &[u8; 4], version: u8, func: F) -> Cursor<Vec<u8>>
    where F: Fn(Section) -> Section
{
    make_box_raw(size, name, |s| {
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
#[should_panic(expected = "expected an error result")]
fn read_truncated_ftyp() {
    // We declare a 24 byte box, but only write 20 bytes.
    let mut stream = make_box_raw(BoxSize::UncheckedShort(24), b"ftyp", |s| {
        s.append_bytes(b"mp42")
            .B32(0) // minor version
            .append_bytes(b"isom")
    });
    let mut context = MediaContext::new();
    match read_mp4(&mut stream, &mut context) {
        Err(Error::UnexpectedEOF) => (),
        Ok(_) => assert!(false, "expected an error result"),
        _ => assert!(false, "expected a different error result"),
    }
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
    let mut stream = make_fullbox(BoxSize::Short(32), b"mdhd", 0, |s| {
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
    let mut stream = make_fullbox(BoxSize::Short(44), b"mdhd", 1, |s| {
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
    let mut stream = make_fullbox(BoxSize::Short(32), b"mdhd", 0, |s| {
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
    let mut stream = make_fullbox(BoxSize::Short(44), b"mdhd", 1, |s| {
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
    let mut stream = make_fullbox(BoxSize::Short(108), b"mvhd", 0, |s| {
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
    let mut stream = make_fullbox(BoxSize::Short(120), b"mvhd", 1, |s| {
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
    let mut stream = make_fullbox(BoxSize::Short(120), b"mvhd", 1, |s| {
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
    let mut stream = make_fullbox(BoxSize::Short(108), b"mvhd", 0, |s| {
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
    let header = read_box_header(&mut stream).unwrap();
    assert_eq!(header.name.as_bytes(), b"vpcC");
    let r = super::read_vpcc(&mut stream);
    assert!(r.is_ok());
}

#[test]
fn read_hdlr() {
    let mut stream = make_fullbox(BoxSize::Short(45), b"mvhd", 0, |s| {
        s.B32(0)
         .append_bytes(b"vide")
         .B32(0)
         .B32(0)
         .B32(0)
         .append_bytes(b"VideoHandler")
         .B8(0) // null-terminate string
    });
    let header = read_box_header(&mut stream).unwrap();
    let parsed = super::read_hdlr(&mut stream, &header).unwrap();
    assert_eq!(parsed.header.name, FourCC(*b"mvhd"));
    assert_eq!(parsed.header.size, 45);
    assert_eq!(parsed.handler_type, FourCC(*b"vide"));
}

#[test]
fn read_hdlr_short_name() {
    let mut stream = make_fullbox(BoxSize::Short(33), b"mvhd", 0, |s| {
        s.B32(0)
         .append_bytes(b"vide")
         .B32(0)
         .B32(0)
         .B32(0)
         .B8(0) // null-terminate string
    });
    let header = read_box_header(&mut stream).unwrap();
    let parsed = super::read_hdlr(&mut stream, &header).unwrap();
    assert_eq!(parsed.header.name, FourCC(*b"mvhd"));
    assert_eq!(parsed.header.size, 33);
    assert_eq!(parsed.handler_type, FourCC(*b"vide"));
}

#[test]
fn read_hdlr_zero_length_name() {
    let mut stream = make_fullbox(BoxSize::Short(32), b"mvhd", 0, |s| {
        s.B32(0)
         .append_bytes(b"vide")
         .B32(0)
         .B32(0)
         .B32(0)
    });
    let header = read_box_header(&mut stream).unwrap();
    let parsed = super::read_hdlr(&mut stream, &header).unwrap();
    assert_eq!(parsed.header.name, FourCC(*b"mvhd"));
    assert_eq!(parsed.header.size, 32);
    assert_eq!(parsed.handler_type, FourCC(*b"vide"));
}
