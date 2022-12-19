#[cfg(feature = "read-bhttp")]
use crate::err::Error;
use crate::err::Res;
use std::convert::TryFrom;
use std::io;

#[cfg(feature = "write-bhttp")]
#[allow(clippy::cast_possible_truncation)]
fn write_uint(n: u8, v: impl Into<u64>, w: &mut impl io::Write) -> Res<()> {
    let v = v.into();
    assert!(n > 0 && usize::from(n) < std::mem::size_of::<u64>());
    for i in 0..n {
        w.write_all(&[((v >> (8 * (n - i - 1))) & 0xff) as u8])?;
    }
    Ok(())
}

#[cfg(feature = "write-bhttp")]
pub fn write_varint(v: impl Into<u64>, w: &mut impl io::Write) -> Res<()> {
    let v = v.into();
    match () {
        _ if v < (1 << 6) => write_uint(1, v, w),
        _ if v < (1 << 14) => write_uint(2, v | (1 << 14), w),
        _ if v < (1 << 30) => write_uint(4, v | (2 << 30), w),
        _ if v < (1 << 62) => write_uint(8, v | (3 << 62), w),
        _ => panic!("Varint value too large"),
    }
}

#[cfg(feature = "write-bhttp")]
pub fn write_len(len: usize, w: &mut impl io::Write) -> Res<()> {
    write_varint(u64::try_from(len).unwrap(), w)
}

#[cfg(feature = "write-bhttp")]
pub fn write_vec(v: &[u8], w: &mut impl io::Write) -> Res<()> {
    write_len(v.len(), w)?;
    w.write_all(v)?;
    Ok(())
}

#[cfg(feature = "read-bhttp")]
fn read_uint(n: usize, r: &mut impl io::BufRead) -> Res<Option<u64>> {
    let mut buf = [0; 7];
    let count = r.read(&mut buf[..n])?;
    if count == 0 {
        return Ok(None);
    } else if count < n {
        return Err(Error::Truncated);
    }
    let mut v = 0;
    for i in &buf[..n] {
        v = (v << 8) | u64::from(*i);
    }
    Ok(Some(v))
}

#[cfg(feature = "read-bhttp")]
pub fn read_varint(r: &mut impl io::BufRead) -> Res<Option<u64>> {
    if let Some(b1) = read_uint(1, r)? {
        Ok(Some(match b1 >> 6 {
            0 => b1 & 0x3f,
            1 => ((b1 & 0x3f) << 8) | read_uint(1, r)?.ok_or(Error::Truncated)?,
            2 => ((b1 & 0x3f) << 24) | read_uint(3, r)?.ok_or(Error::Truncated)?,
            3 => ((b1 & 0x3f) << 56) | read_uint(7, r)?.ok_or(Error::Truncated)?,
            _ => unreachable!(),
        }))
    } else {
        Ok(None)
    }
}

#[cfg(feature = "read-bhttp")]
pub fn read_vec(r: &mut impl io::BufRead) -> Res<Option<Vec<u8>>> {
    if let Some(len) = read_varint(r)? {
        let mut v = vec![0; usize::try_from(len).unwrap()];
        r.read_exact(&mut v)?;
        Ok(Some(v))
    } else {
        Ok(None)
    }
}
