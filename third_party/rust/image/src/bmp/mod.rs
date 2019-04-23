//!  Decoding and Encoding of BMP Images
//!
//!  A decoder and encoder for BMP (Windows Bitmap) images
//!
//!  # Related Links
//!  * <https://msdn.microsoft.com/en-us/library/windows/desktop/dd183375%28v=vs.85%29.aspx>
//!  * <https://en.wikipedia.org/wiki/BMP_file_format>
//!

pub use self::decoder::BMPDecoder;
pub use self::encoder::BMPEncoder;

mod decoder;
mod encoder;
