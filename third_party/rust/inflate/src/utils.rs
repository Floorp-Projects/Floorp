/// Convenience functions for inflating DEFLATE compressed data.
///
/// # Example
/// ```
/// use inflate::inflate_bytes;
/// use std::str::from_utf8;
///
/// let encoded = [243, 72, 205, 201, 201, 215, 81, 40, 207, 47, 202, 73, 1, 0];
/// let decoded = inflate_bytes(&encoded).unwrap();
/// println!("{}", from_utf8(&decoded).unwrap()); // prints "Hello, world"
/// ```

use InflateStream;

fn inflate(inflater: &mut InflateStream, data: &[u8]) -> Result<Vec<u8>, String> {
    let mut decoded = Vec::<u8>::new();

    let mut n = 0;
    loop {
        let (num_bytes_read, bytes) = try!(inflater.update(&data[n..]));
        if bytes.len() == 0 {
            break;
        }
        n += num_bytes_read;
        decoded.extend(bytes.iter().map(|v| *v));
    }

    Ok(decoded)
}

/// Decompress the given slice of DEFLATE compressed data.
///
/// Returns a `Vec` with the decompressed data or an error message.
pub fn inflate_bytes(data: &[u8]) -> Result<Vec<u8>, String> {
    inflate(&mut InflateStream::new(), data)
}

/// Decompress the given slice of DEFLATE compressed (with zlib headers and trailers) data.
///
/// Returns a `Vec` with the decompressed data or an error message.
pub fn inflate_bytes_zlib(data: &[u8]) -> Result<Vec<u8>, String> {
    inflate(&mut InflateStream::from_zlib(), data)
}

/// Decompress the given slice of DEFLATE compressed (with zlib headers and trailers) data,
/// without calculating and validating the checksum.
///
/// Returns a `Vec` with the decompressed data or an error message.
pub fn inflate_bytes_zlib_no_checksum(data: &[u8]) -> Result<Vec<u8>, String> {
    inflate(&mut InflateStream::from_zlib_no_checksum(), data)
}

#[cfg(test)]
mod test {
    #[test]
    fn inflate_bytes_with_zlib() {
        use super::inflate_bytes_zlib;
        use std::str::from_utf8;

        let encoded = [120, 156, 243, 72, 205, 201, 201, 215, 81, 168, 202, 201,
                       76, 82, 4, 0, 27, 101, 4, 19];
        let decoded = inflate_bytes_zlib(&encoded).unwrap();
        assert!(from_utf8(&decoded).unwrap() == "Hello, zlib!");
    }

    #[test]
    fn inflate_bytes_with_zlib_checksum_fail() {
        use super::inflate_bytes_zlib;

        // The last 4 bytes are the checksum, we set them to 0 here to check that decoding fails
        // if the checksum is wrong.
        let encoded = [120, 156, 243, 72, 205, 201, 201, 215, 81, 168, 202, 201,
                       76, 82, 4, 0, 0, 0, 0, 0];
        inflate_bytes_zlib(&encoded).unwrap_err();
    }

    #[test]
    fn inflate_bytes_with_zlib_trailing() {
        use super::inflate_bytes_zlib;
        use std::str::from_utf8;

        // The additional 4 bytes should be ignored.
        let encoded = [120, 156, 243, 72, 205, 201, 201, 215, 81, 168, 202, 201,
                       76, 82, 4, 0, 27, 101, 4, 19, 0, 0, 0, 0];
        let decoded = inflate_bytes_zlib(&encoded).unwrap();
        assert!(from_utf8(&decoded).unwrap() == "Hello, zlib!");
    }

}
