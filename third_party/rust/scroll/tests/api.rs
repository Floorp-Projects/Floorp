// this exists primarily to test various API usages of scroll; e.g., must compile

// guard against potential undefined behaviour when borrowing from
// packed structs. See https://github.com/rust-lang/rust/issues/46043
#![deny(unaligned_references)]

// #[macro_use] extern crate scroll_derive;

use scroll::ctx::SizeWith;
use scroll::{ctx, Cread, Pread, Result};
use std::ops::{Deref, DerefMut};

#[derive(Default)]
pub struct Section<'a> {
    pub sectname: [u8; 16],
    pub segname: [u8; 16],
    pub addr: u64,
    pub size: u64,
    pub offset: u32,
    pub align: u32,
    pub reloff: u32,
    pub nreloc: u32,
    pub flags: u32,
    pub data: &'a [u8],
}

impl<'a> Section<'a> {
    pub fn name(&self) -> Result<&str> {
        self.sectname.pread::<&str>(0)
    }
    pub fn segname(&self) -> Result<&str> {
        self.segname.pread::<&str>(0)
    }
}

impl<'a> ctx::SizeWith for Section<'a> {
    fn size_with(_ctx: &()) -> usize {
        4
    }
}

#[repr(C)]
// renable when scroll_derive Pread/Pwrite matches
//#[derive(Debug, Clone, Copy, Pread, Pwrite)]
#[derive(Debug, Clone, Copy)]
pub struct Section32 {
    pub sectname: [u8; 16],
    pub segname: [u8; 16],
    pub addr: u32,
    pub size: u32,
    pub offset: u32,
    pub align: u32,
    pub reloff: u32,
    pub nreloc: u32,
    pub flags: u32,
    pub reserved1: u32,
    pub reserved2: u32,
}

impl<'a> ctx::TryFromCtx<'a, ()> for Section<'a> {
    type Error = scroll::Error;
    fn try_from_ctx(
        _bytes: &'a [u8],
        _ctx: (),
    ) -> ::std::result::Result<(Self, usize), Self::Error> {
        let section = Section::default();
        Ok((section, ::std::mem::size_of::<Section>()))
    }
}

pub struct Segment<'a> {
    pub cmd: u32,
    pub cmdsize: u32,
    pub segname: [u8; 16],
    pub vmaddr: u64,
    pub vmsize: u64,
    pub fileoff: u64,
    pub filesize: u64,
    pub maxprot: u32,
    pub initprot: u32,
    pub nsects: u32,
    pub flags: u32,
    pub data: &'a [u8],
    offset: usize,
    raw_data: &'a [u8],
}

impl<'a> Segment<'a> {
    pub fn name(&self) -> Result<&str> {
        Ok(self.segname.pread::<&str>(0)?)
    }
    pub fn sections(&self) -> Result<Vec<Section<'a>>> {
        let nsects = self.nsects as usize;
        let mut sections = Vec::with_capacity(nsects);
        let offset = &mut (self.offset + Self::size_with(&()));
        let _size = Section::size_with(&());
        let raw_data: &'a [u8] = self.raw_data;
        for _ in 0..nsects {
            let section = raw_data.gread_with::<Section<'a>>(offset, ())?;
            sections.push(section);
            //offset += size;
        }
        Ok(sections)
    }
}

impl<'a> ctx::SizeWith for Segment<'a> {
    fn size_with(_ctx: &()) -> usize {
        4
    }
}

pub struct Segments<'a> {
    pub segments: Vec<Segment<'a>>,
}

impl<'a> Deref for Segments<'a> {
    type Target = Vec<Segment<'a>>;
    fn deref(&self) -> &Self::Target {
        &self.segments
    }
}

impl<'a> DerefMut for Segments<'a> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.segments
    }
}

impl<'a> Segments<'a> {
    pub fn new() -> Self {
        Segments {
            segments: Vec::new(),
        }
    }
    pub fn sections(&self) -> Result<Vec<Vec<Section<'a>>>> {
        let mut sections = Vec::new();
        for segment in &self.segments {
            sections.push(segment.sections()?);
        }
        Ok(sections)
    }
}

fn lifetime_passthrough_<'a>(segments: &Segments<'a>, section_name: &str) -> Option<&'a [u8]> {
    let segment_name = "__TEXT";
    for segment in &segments.segments {
        if let Ok(name) = segment.name() {
            println!("segment.name: {}", name);
            if name == segment_name {
                if let Ok(sections) = segment.sections() {
                    for section in sections {
                        let sname = section.name().unwrap();
                        println!("section.name: {}", sname);
                        if section_name == sname {
                            return Some(section.data);
                        }
                    }
                }
            }
        }
    }
    None
}

#[test]
fn lifetime_passthrough() {
    let segments = Segments::new();
    let _res = lifetime_passthrough_(&segments, "__text");
    assert!(true)
}

#[derive(Default)]
#[repr(packed)]
struct Foo {
    foo: i64,
    bar: u32,
}

impl scroll::ctx::FromCtx<scroll::Endian> for Foo {
    fn from_ctx(bytes: &[u8], ctx: scroll::Endian) -> Self {
        Foo {
            foo: bytes.cread_with::<i64>(0, ctx),
            bar: bytes.cread_with::<u32>(8, ctx),
        }
    }
}

impl scroll::ctx::SizeWith<scroll::Endian> for Foo {
    fn size_with(_: &scroll::Endian) -> usize {
        ::std::mem::size_of::<Foo>()
    }
}

#[test]
fn ioread_api() {
    use scroll::{IOread, LE};
    use std::io::Cursor;
    let bytes_ = [
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xef, 0xbe, 0x00, 0x00,
    ];
    let mut bytes = Cursor::new(bytes_);
    let foo = bytes.ioread_with::<i64>(LE).unwrap();
    let bar = bytes.ioread_with::<u32>(LE).unwrap();
    assert_eq!(foo, 1);
    assert_eq!(bar, 0xbeef);
    let error = bytes.ioread_with::<f64>(LE);
    assert!(error.is_err());
    let mut bytes = Cursor::new(bytes_);
    let foo_ = bytes.ioread_with::<Foo>(LE).unwrap();
    assert_eq!({ foo_.foo }, foo);
    assert_eq!({ foo_.bar }, bar);
}

#[repr(packed)]
struct Bar {
    foo: i32,
    bar: u32,
}

impl scroll::ctx::FromCtx<scroll::Endian> for Bar {
    fn from_ctx(bytes: &[u8], ctx: scroll::Endian) -> Self {
        Bar {
            foo: bytes.cread_with(0, ctx),
            bar: bytes.cread_with(4, ctx),
        }
    }
}

#[test]
fn cread_api() {
    use scroll::{Cread, LE};
    let bytes = [
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xef, 0xbe, 0x00, 0x00,
    ];
    let foo = bytes.cread_with::<u64>(0, LE);
    let bar = bytes.cread_with::<u32>(8, LE);
    assert_eq!(foo, 1);
    assert_eq!(bar, 0xbeef);
}

#[test]
fn cread_api_customtype() {
    use scroll::{Cread, LE};
    let bytes = [0xff, 0xff, 0xff, 0xff, 0xef, 0xbe, 0xad, 0xde];
    let bar = &bytes[..].cread_with::<Bar>(0, LE);
    assert_eq!({ bar.foo }, -1);
    assert_eq!({ bar.bar }, 0xdeadbeef);
}

#[test]
#[should_panic]
fn cread_api_badindex() {
    use scroll::Cread;
    let bytes = [
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xef, 0xbe, 0xad, 0xde,
    ];
    let _foo = bytes.cread::<i64>(1_000_000);
}

#[test]
fn cwrite_api() {
    use scroll::Cread;
    use scroll::Cwrite;
    let mut bytes = [0x0; 16];
    bytes.cwrite::<u64>(42, 0);
    bytes.cwrite::<u32>(0xdeadbeef, 8);
    assert_eq!(bytes.cread::<u64>(0), 42);
    assert_eq!(bytes.cread::<u32>(8), 0xdeadbeef);
}

impl scroll::ctx::IntoCtx<scroll::Endian> for Bar {
    fn into_ctx(self, bytes: &mut [u8], ctx: scroll::Endian) {
        use scroll::Cwrite;
        bytes.cwrite_with(self.foo, 0, ctx);
        bytes.cwrite_with(self.bar, 4, ctx);
    }
}

#[test]
fn cwrite_api_customtype() {
    use scroll::{Cread, Cwrite};
    let bar = Bar {
        foo: -1,
        bar: 0xdeadbeef,
    };
    let mut bytes = [0x0; 16];
    let _ = &bytes[..].cwrite::<Bar>(bar, 0);
    let bar = bytes.cread::<Bar>(0);
    assert_eq!({ bar.foo }, -1);
    assert_eq!({ bar.bar }, 0xdeadbeef);
}
