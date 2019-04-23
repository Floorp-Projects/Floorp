use std::io::{Write, Error, ErrorKind};
use std::io;
use InflateStream;

/// A DEFLATE decoder.
///
/// A struct implementing the `std::io::Write` trait that decompresses DEFLATE
/// encoded data into the given writer `w`.
///
/// # Example
///
/// ```
/// use inflate::InflateWriter;
/// use std::io::Write;
///
/// let encoded = [243, 72, 205, 201, 201, 215, 81, 40, 207, 47, 202, 73, 1, 0];
/// let mut decoder = InflateWriter::new(Vec::new());
/// decoder.write(&encoded).unwrap();
/// let decoded = decoder.finish().unwrap();
/// println!("{}", std::str::from_utf8(&decoded).unwrap()); // prints "Hello, world"
/// ```
pub struct InflateWriter<W: Write> {
    inflater: InflateStream,
    writer: W
}

impl<W: Write> InflateWriter<W> {
    pub fn new(w: W) -> InflateWriter<W> {
        InflateWriter { inflater: InflateStream::new(), writer: w }
    }

    pub fn from_zlib(w: W) -> InflateWriter<W> {
        InflateWriter { inflater: InflateStream::from_zlib(), writer: w }
    }

    pub fn finish(mut self) -> io::Result<W> {
        try!(self.flush());
        Ok(self.writer)
    }
}

fn update<'a>(inflater: &'a mut InflateStream, buf: &[u8]) -> io::Result<(usize, &'a [u8])>  {
    match inflater.update(buf) {
        Ok(res) => Ok(res),
        Err(m) => return Err(Error::new(ErrorKind::Other, m)),
    }
}
impl<W: Write> Write for InflateWriter<W> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        let mut n = 0;
        while n < buf.len() {
            let (num_bytes_read, result) = try!(update(&mut self.inflater, &buf[n..]));
            n += num_bytes_read;
            try!(self.writer.write(result));
        }
        Ok(n)
    }

    fn flush(&mut self) -> io::Result<()> {
        let (_, result) = try!(update(&mut self.inflater, &[]));
        try!(self.writer.write(result));
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::InflateWriter;
    use std::io::Write;

    #[test]
    fn inflate_writer() {
       let encoded = [243, 72, 205, 201, 201, 215, 81, 40, 207, 47, 202, 73, 1, 0];
       let mut decoder = InflateWriter::new(Vec::new());
       decoder.write(&encoded).unwrap();
       let decoded = decoder.finish().unwrap();
       assert!(String::from_utf8(decoded).unwrap() == "Hello, world");
    }

    #[test]
    fn inflate_writer_from_zlib() {
       let encoded = [120, 156, 243, 72, 205, 201, 201, 215, 81, 168, 202, 201, 76, 82, 4, 0, 27, 101, 4, 19];
       let mut decoder = InflateWriter::from_zlib(Vec::new());
       decoder.write(&encoded).unwrap();
       let decoded = decoder.finish().unwrap();
       assert!(String::from_utf8(decoded).unwrap() == "Hello, zlib!");
    }
}
