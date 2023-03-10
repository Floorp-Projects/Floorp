#[cfg(feature = "read-http")]
use crate::{Error, ReadSeek, Res};
#[cfg(feature = "read-http")]
use std::borrow::BorrowMut;

pub const HTAB: u8 = 0x09;
#[cfg(feature = "read-http")]
pub const NL: u8 = 0x0a;
#[cfg(feature = "read-http")]
pub const CR: u8 = 0x0d;
pub const SP: u8 = 0x20;
pub const COMMA: u8 = 0x2c;
#[cfg(feature = "read-http")]
pub const SLASH: u8 = 0x2f;
#[cfg(feature = "read-http")]
pub const COLON: u8 = 0x3a;
#[cfg(feature = "read-http")]
pub const SEMICOLON: u8 = 0x3b;

pub fn is_ows(x: u8) -> bool {
    x == SP || x == HTAB
}

pub fn trim_ows(v: &[u8]) -> &[u8] {
    for s in 0..v.len() {
        if !is_ows(v[s]) {
            for e in (s..v.len()).rev() {
                if !is_ows(v[e]) {
                    return &v[s..=e];
                }
            }
        }
    }
    &v[..0]
}

#[cfg(feature = "read-http")]
pub fn downcase(n: &mut [u8]) {
    for i in n {
        if *i >= 0x41 && *i <= 0x5a {
            *i += 0x20;
        }
    }
}

pub fn index_of(v: u8, line: &[u8]) -> Option<usize> {
    for (i, x) in line.iter().enumerate() {
        if *x == v {
            return Some(i);
        }
    }
    None
}

#[cfg(feature = "read-http")]
pub fn split_at(v: u8, mut line: Vec<u8>) -> Option<(Vec<u8>, Vec<u8>)> {
    index_of(v, &line).map(|i| {
        let tail = line.split_off(i + 1);
        let _ = line.pop();
        (line, tail)
    })
}

#[cfg(feature = "read-http")]
pub fn read_line<T, R>(r: &mut T) -> Res<Vec<u8>>
where
    T: BorrowMut<R> + ?Sized,
    R: ReadSeek + ?Sized,
{
    let mut buf = Vec::new();
    r.borrow_mut().read_until(NL, &mut buf)?;
    let tail = buf.pop();
    if tail != Some(NL) {
        return Err(Error::Truncated);
    }
    if buf.pop().ok_or(Error::Missing(CR))? == CR {
        Ok(buf)
    } else {
        Err(Error::Missing(CR))
    }
}
