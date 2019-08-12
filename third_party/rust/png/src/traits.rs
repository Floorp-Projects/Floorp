use std::io;

// Will be replaced by stdlib solution
fn read_all<R: io::Read + ?Sized>(this: &mut R, buf: &mut [u8]) -> io::Result<()> {
    let mut total = 0;
    while total < buf.len() {
        match this.read(&mut buf[total..]) {
            Ok(0) => return Err(io::Error::new(io::ErrorKind::Other,
                                               "failed to read the whole buffer")),
            Ok(n) => total += n,
            Err(ref e) if e.kind() == io::ErrorKind::Interrupted => {}
            Err(e) => return Err(e),
        }
    }
    Ok(())
}

/// Read extension to read big endian data
pub trait ReadBytesExt<T>: io::Read {
    /// Read `T` from a bytes stream. Most significant byte first.
    fn read_be(&mut self) -> io::Result<T>;

}

/// Write extension to write big endian data
pub trait WriteBytesExt<T>: io::Write {
    /// Writes `T` to a bytes stream. Most significant byte first.
    fn write_be(&mut self, _: T) -> io::Result<()>;

}

impl<W: io::Read + ?Sized> ReadBytesExt<u8> for W {
	#[inline]
	fn read_be(&mut self) -> io::Result<u8> {
        let mut byte = [0];
		read_all(self, &mut byte)?;
        Ok(byte[0])
	}
}
impl<W: io::Read + ?Sized> ReadBytesExt<u16> for W {
	#[inline]
	fn read_be(&mut self) -> io::Result<u16> {
        let mut bytes = [0, 0];
		read_all(self, &mut bytes)?;
        Ok((bytes[0] as u16) << 8 | bytes[1] as u16)
	}
}

impl<W: io::Read + ?Sized> ReadBytesExt<u32> for W {
	#[inline]
	fn read_be(&mut self) -> io::Result<u32> {
        let mut bytes = [0, 0, 0, 0];
		read_all(self, &mut bytes)?;
        Ok(  (bytes[0] as u32) << 24 
           | (bytes[1] as u32) << 16
           | (bytes[2] as u32) << 8
           |  bytes[3] as u32
        )
	}
}

impl<W: io::Write + ?Sized> WriteBytesExt<u32> for W {
    #[inline]
    fn write_be(&mut self, n: u32) -> io::Result<()> {
        self.write_all(&[
            (n >> 24) as u8,
            (n >> 16) as u8,
            (n >>  8) as u8,
            n         as u8
        ])
    }
}
