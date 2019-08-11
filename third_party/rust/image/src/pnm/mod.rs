//! Decoding of netpbm image formats (pbm, pgm, ppm and pam).
//!
//! The formats pbm, pgm and ppm are fully supported. The pam decoder recognizes the tuple types
//! `BLACKANDWHITE`, `GRAYSCALE` and `RGB` and explicitely recognizes but rejects their `_ALPHA`
//! variants for now as alpha color types are unsupported.
use self::autobreak::AutoBreak;
pub use self::decoder::PNMDecoder;
pub use self::encoder::PNMEncoder;
use self::header::HeaderRecord;
pub use self::header::{ArbitraryHeader, ArbitraryTuplType, BitmapHeader, GraymapHeader,
                       PixmapHeader};
pub use self::header::{PNMHeader, PNMSubtype, SampleEncoding};

mod autobreak;
mod decoder;
mod encoder;
mod header;

#[cfg(test)]
mod tests {
    use super::*;
    use byteorder::{ByteOrder, NativeEndian};
    use color::ColorType;
    use image::ImageDecoder;

    fn execute_roundtrip_default(buffer: &[u8], width: u32, height: u32, color: ColorType) {
        let mut encoded_buffer = Vec::new();

        {
            let mut encoder = PNMEncoder::new(&mut encoded_buffer);
            encoder
                .encode(buffer, width, height, color)
                .expect("Failed to encode the image buffer");
        }

        let (header, loaded_color, loaded_image) = {
            let mut decoder = PNMDecoder::new(&encoded_buffer[..]).unwrap();
            let colortype = decoder.colortype();
            let image = decoder.read_image().expect("Failed to decode the image");
            let (_, header) = PNMDecoder::new(&encoded_buffer[..]).unwrap().into_inner();
            (header, colortype, image)
        };

        assert_eq!(header.width(), width);
        assert_eq!(header.height(), height);
        assert_eq!(loaded_color, color);
        assert_eq!(loaded_image.as_slice(), buffer);
    }

    fn execute_roundtrip_with_subtype(
        buffer: &[u8],
        width: u32,
        height: u32,
        color: ColorType,
        subtype: PNMSubtype,
    ) {
        let mut encoded_buffer = Vec::new();

        {
            let mut encoder = PNMEncoder::new(&mut encoded_buffer).with_subtype(subtype);
            encoder
                .encode(buffer, width, height, color)
                .expect("Failed to encode the image buffer");
        }

        let (header, loaded_color, loaded_image) = {
            let mut decoder = PNMDecoder::new(&encoded_buffer[..]).unwrap();
            let colortype = decoder.colortype();
            let image = decoder.read_image().expect("Failed to decode the image");
            let (_, header) = PNMDecoder::new(&encoded_buffer[..]).unwrap().into_inner();
            (header, colortype, image)
        };

        assert_eq!(header.width(), width);
        assert_eq!(header.height(), height);
        assert_eq!(header.subtype(), subtype);
        assert_eq!(loaded_color, color);
        assert_eq!(loaded_image.as_slice(), buffer);
    }

    fn execute_roundtrip_u16(buffer: &[u16], width: u32, height: u32, color: ColorType) {
        let mut encoded_buffer = Vec::new();

        {
            let mut encoder = PNMEncoder::new(&mut encoded_buffer);
            encoder
                .encode(buffer, width, height, color)
                .expect("Failed to encode the image buffer");
        }

        let (header, loaded_color, loaded_image) = {
            let mut decoder = PNMDecoder::new(&encoded_buffer[..]).unwrap();
            let colortype = decoder.colortype();
            let image = decoder.read_image().expect("Failed to decode the image");
            let (_, header) = PNMDecoder::new(&encoded_buffer[..]).unwrap().into_inner();
            (header, colortype, image)
        };

        let mut buffer_u8 = vec![0; buffer.len() * 2];
        NativeEndian::write_u16_into(buffer, &mut buffer_u8[..]);

        assert_eq!(header.width(), width);
        assert_eq!(header.height(), height);
        assert_eq!(loaded_color, color);
        assert_eq!(loaded_image, buffer_u8);
    }

    #[test]
    fn roundtrip_rgb() {
        #[cfg_attr(rustfmt, rustfmt_skip)]
        let buf: [u8; 27] = [
              0,   0,   0,
              0,   0, 255,
              0, 255,   0,
              0, 255, 255,
            255,   0,   0,
            255,   0, 255,
            255, 255,   0,
            255, 255, 255,
            255, 255, 255,
        ];
        execute_roundtrip_default(&buf, 3, 3, ColorType::RGB(8));
        execute_roundtrip_with_subtype(&buf, 3, 3, ColorType::RGB(8), PNMSubtype::ArbitraryMap);
        execute_roundtrip_with_subtype(
            &buf,
            3,
            3,
            ColorType::RGB(8),
            PNMSubtype::Pixmap(SampleEncoding::Binary),
        );
        execute_roundtrip_with_subtype(
            &buf,
            3,
            3,
            ColorType::RGB(8),
            PNMSubtype::Pixmap(SampleEncoding::Ascii),
        );
    }

    #[test]
    fn roundtrip_u16() {
        let buf: [u16; 6] = [0, 1, 0xFFFF, 0x1234, 0x3412, 0xBEAF];

        execute_roundtrip_u16(&buf, 6, 1, ColorType::Gray(16));
    }
}
