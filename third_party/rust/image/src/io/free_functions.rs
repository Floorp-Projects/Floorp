use std::ffi::OsString;
use std::fs::File;
use std::io::{BufRead, BufReader, BufWriter, Seek};
use std::path::Path;
use std::u32;

#[cfg(feature = "bmp")]
use crate::bmp;
#[cfg(feature = "gif")]
use crate::gif;
#[cfg(feature = "hdr")]
use crate::hdr;
#[cfg(feature = "ico")]
use crate::ico;
#[cfg(feature = "jpeg")]
use crate::jpeg;
#[cfg(feature = "png")]
use crate::png;
#[cfg(feature = "pnm")]
use crate::pnm;
#[cfg(feature = "tga")]
use crate::tga;
#[cfg(feature = "dds")]
use crate::dds;
#[cfg(feature = "tiff")]
use crate::tiff;
#[cfg(feature = "webp")]
use crate::webp;

use crate::color;
use crate::image;
use crate::dynimage::DynamicImage;
use crate::error::{ImageError, ImageFormatHint, ImageResult};
use crate::image::{ImageDecoder, ImageEncoder, ImageFormat};

/// Internal error type for guessing format from path.
pub(crate) enum PathError {
    /// The extension did not fit a supported format.
    UnknownExtension(OsString),
    /// Extension could not be converted to `str`.
    NoExtension,
}

pub(crate) fn open_impl(path: &Path) -> ImageResult<DynamicImage> {
    let fin = match File::open(path) {
        Ok(f) => f,
        Err(err) => return Err(ImageError::IoError(err)),
    };
    let fin = BufReader::new(fin);

    load(fin, ImageFormat::from_path(path)?)
}

/// Create a new image from a Reader
///
/// Try [`io::Reader`] for more advanced uses.
///
/// [`io::Reader`]: io/struct.Reader.html
pub fn load<R: BufRead + Seek>(r: R, format: ImageFormat) -> ImageResult<DynamicImage> {
    #[allow(deprecated, unreachable_patterns)]
    // Default is unreachable if all features are supported.
    match format {
        #[cfg(feature = "png")]
        image::ImageFormat::Png => DynamicImage::from_decoder(png::PngDecoder::new(r)?),
        #[cfg(feature = "gif")]
        image::ImageFormat::Gif => DynamicImage::from_decoder(gif::GifDecoder::new(r)?),
        #[cfg(feature = "jpeg")]
        image::ImageFormat::Jpeg => DynamicImage::from_decoder(jpeg::JpegDecoder::new(r)?),
        #[cfg(feature = "webp")]
        image::ImageFormat::WebP => DynamicImage::from_decoder(webp::WebPDecoder::new(r)?),
        #[cfg(feature = "tiff")]
        image::ImageFormat::Tiff => DynamicImage::from_decoder(tiff::TiffDecoder::new(r)?),
        #[cfg(feature = "tga")]
        image::ImageFormat::Tga => DynamicImage::from_decoder(tga::TgaDecoder::new(r)?),
        #[cfg(feature = "dds")]
        image::ImageFormat::Dds => DynamicImage::from_decoder(dds::DdsDecoder::new(r)?),
        #[cfg(feature = "bmp")]
        image::ImageFormat::Bmp => DynamicImage::from_decoder(bmp::BmpDecoder::new(r)?),
        #[cfg(feature = "ico")]
        image::ImageFormat::Ico => DynamicImage::from_decoder(ico::IcoDecoder::new(r)?),
        #[cfg(feature = "hdr")]
        image::ImageFormat::Hdr => DynamicImage::from_decoder(hdr::HDRAdapter::new(BufReader::new(r))?),
        #[cfg(feature = "pnm")]
        image::ImageFormat::Pnm => DynamicImage::from_decoder(pnm::PnmDecoder::new(BufReader::new(r))?),
        _ => Err(ImageError::Unsupported(ImageFormatHint::Exact(format).into())),
    }
}

pub(crate) fn image_dimensions_impl(path: &Path) -> ImageResult<(u32, u32)> {
    let format = image::ImageFormat::from_path(path)?;

    let fin = File::open(path)?;
    let fin = BufReader::new(fin);

    image_dimensions_with_format_impl(fin, format)
}

pub(crate) fn image_dimensions_with_format_impl<R: BufRead + Seek>(fin: R, format: ImageFormat)
    -> ImageResult<(u32, u32)>
{
    #[allow(unreachable_patterns)]
    // Default is unreachable if all features are supported.
    Ok(match format {
        #[cfg(feature = "jpeg")]
        image::ImageFormat::Jpeg => jpeg::JpegDecoder::new(fin)?.dimensions(),
        #[cfg(feature = "png")]
        image::ImageFormat::Png => png::PngDecoder::new(fin)?.dimensions(),
        #[cfg(feature = "gif")]
        image::ImageFormat::Gif => gif::GifDecoder::new(fin)?.dimensions(),
        #[cfg(feature = "webp")]
        image::ImageFormat::WebP => webp::WebPDecoder::new(fin)?.dimensions(),
        #[cfg(feature = "tiff")]
        image::ImageFormat::Tiff => tiff::TiffDecoder::new(fin)?.dimensions(),
        #[cfg(feature = "tga")]
        image::ImageFormat::Tga => tga::TgaDecoder::new(fin)?.dimensions(),
        #[cfg(feature = "dds")]
        image::ImageFormat::Dds => dds::DdsDecoder::new(fin)?.dimensions(),
        #[cfg(feature = "bmp")]
        image::ImageFormat::Bmp => bmp::BmpDecoder::new(fin)?.dimensions(),
        #[cfg(feature = "ico")]
        image::ImageFormat::Ico => ico::IcoDecoder::new(fin)?.dimensions(),
        #[cfg(feature = "hdr")]
        image::ImageFormat::Hdr => hdr::HDRAdapter::new(fin)?.dimensions(),
        #[cfg(feature = "pnm")]
        image::ImageFormat::Pnm => {
            pnm::PnmDecoder::new(fin)?.dimensions()
        }
        format => return Err(ImageError::Unsupported(ImageFormatHint::Exact(format).into())),
    })
}

pub(crate) fn save_buffer_impl(
    path: &Path,
    buf: &[u8],
    width: u32,
    height: u32,
    color: color::ColorType,
) -> ImageResult<()> {
    let fout = &mut BufWriter::new(File::create(path)?);
    let ext = path.extension()
        .and_then(|s| s.to_str())
        .map_or("".to_string(), |s| s.to_ascii_lowercase());

    match &*ext {
        #[cfg(feature = "gif")]
        "gif" => gif::Encoder::new(fout).encode(buf, width, height, color),
        #[cfg(feature = "ico")]
        "ico" => ico::ICOEncoder::new(fout).write_image(buf, width, height, color),
        #[cfg(feature = "jpeg")]
        "jpg" | "jpeg" => jpeg::JPEGEncoder::new(fout).write_image(buf, width, height, color),
        #[cfg(feature = "png")]
        "png" => png::PNGEncoder::new(fout).write_image(buf, width, height, color),
        #[cfg(feature = "pnm")]
        "pbm" => pnm::PNMEncoder::new(fout)
            .with_subtype(pnm::PNMSubtype::Bitmap(pnm::SampleEncoding::Binary))
            .write_image(buf, width, height, color),
        #[cfg(feature = "pnm")]
        "pgm" => pnm::PNMEncoder::new(fout)
            .with_subtype(pnm::PNMSubtype::Graymap(pnm::SampleEncoding::Binary))
            .write_image(buf, width, height, color),
        #[cfg(feature = "pnm")]
        "ppm" => pnm::PNMEncoder::new(fout)
            .with_subtype(pnm::PNMSubtype::Pixmap(pnm::SampleEncoding::Binary))
            .write_image(buf, width, height, color),
        #[cfg(feature = "pnm")]
        "pam" => pnm::PNMEncoder::new(fout).write_image(buf, width, height, color),
        #[cfg(feature = "bmp")]
        "bmp" => bmp::BMPEncoder::new(fout).write_image(buf, width, height, color),
        #[cfg(feature = "tiff")]
        "tif" | "tiff" => tiff::TiffEncoder::new(fout)
            .write_image(buf, width, height, color),
        _ => Err(ImageError::Unsupported(ImageFormatHint::from(path).into())),
    }
}

pub(crate) fn save_buffer_with_format_impl(
    path: &Path,
    buf: &[u8],
    width: u32,
    height: u32,
    color: color::ColorType,
    format: ImageFormat,
) -> ImageResult<()> {
    let fout = &mut BufWriter::new(File::create(path)?);

    match format {
        #[cfg(feature = "gif")]
        image::ImageFormat::Gif => gif::Encoder::new(fout).encode(buf, width, height, color),
        #[cfg(feature = "ico")]
        image::ImageFormat::Ico => ico::ICOEncoder::new(fout).write_image(buf, width, height, color),
        #[cfg(feature = "jpeg")]
        image::ImageFormat::Jpeg => jpeg::JPEGEncoder::new(fout).write_image(buf, width, height, color),
        #[cfg(feature = "png")]
        image::ImageFormat::Png => png::PNGEncoder::new(fout).write_image(buf, width, height, color),
        #[cfg(feature = "bmp")]
        image::ImageFormat::Bmp => bmp::BMPEncoder::new(fout).write_image(buf, width, height, color),
        #[cfg(feature = "tiff")]
        image::ImageFormat::Tiff => tiff::TiffEncoder::new(fout)
            .write_image(buf, width, height, color),
        format => return Err(ImageError::Unsupported(ImageFormatHint::Exact(format).into())),
    }
}

/// Guess format from a path.
///
/// Returns `PathError::NoExtension` if the path has no extension or returns a
/// `PathError::UnknownExtension` containing the extension if  it can not be convert to a `str`.
pub(crate) fn guess_format_from_path_impl(path: &Path) -> Result<ImageFormat, PathError> {
    let exact_ext = path.extension();

    let ext = exact_ext
        .and_then(|s| s.to_str())
        .map(str::to_ascii_lowercase);

    let ext = ext.as_ref()
        .map(String::as_str);

    Ok(match ext {
        Some("jpg") | Some("jpeg") => image::ImageFormat::Jpeg,
        Some("png") => image::ImageFormat::Png,
        Some("gif") => image::ImageFormat::Gif,
        Some("webp") => image::ImageFormat::WebP,
        Some("tif") | Some("tiff") => image::ImageFormat::Tiff,
        Some("tga") => image::ImageFormat::Tga,
        Some("dds") => image::ImageFormat::Dds,
        Some("bmp") => image::ImageFormat::Bmp,
        Some("ico") => image::ImageFormat::Ico,
        Some("hdr") => image::ImageFormat::Hdr,
        Some("pbm") | Some("pam") | Some("ppm") | Some("pgm") => image::ImageFormat::Pnm,
        // The original extension is used, instead of _format
        _ => return match exact_ext {
            None => Err(PathError::NoExtension),
            Some(os) => Err(PathError::UnknownExtension(os.to_owned())),
        },
    })
}

static MAGIC_BYTES: [(&'static [u8], ImageFormat); 18] = [
    (b"\x89PNG\r\n\x1a\n", ImageFormat::Png),
    (&[0xff, 0xd8, 0xff], ImageFormat::Jpeg),
    (b"GIF89a", ImageFormat::Gif),
    (b"GIF87a", ImageFormat::Gif),
    (b"RIFF", ImageFormat::WebP), // TODO: better magic byte detection, see https://github.com/image-rs/image/issues/660
    (b"MM\x00*", ImageFormat::Tiff),
    (b"II*\x00", ImageFormat::Tiff),
    (b"DDS ", ImageFormat::Dds),
    (b"BM", ImageFormat::Bmp),
    (&[0, 0, 1, 0], ImageFormat::Ico),
    (b"#?RADIANCE", ImageFormat::Hdr),
    (b"P1", ImageFormat::Pnm),
    (b"P2", ImageFormat::Pnm),
    (b"P3", ImageFormat::Pnm),
    (b"P4", ImageFormat::Pnm),
    (b"P5", ImageFormat::Pnm),
    (b"P6", ImageFormat::Pnm),
    (b"P7", ImageFormat::Pnm),
];

/// Guess image format from memory block
///
/// Makes an educated guess about the image format based on the Magic Bytes at the beginning.
/// TGA is not supported by this function.
/// This is not to be trusted on the validity of the whole memory block
pub fn guess_format(buffer: &[u8]) -> ImageResult<ImageFormat> {
    match guess_format_impl(buffer) {
        Some(format) => Ok(format),
        None => Err(ImageError::Unsupported(ImageFormatHint::Unknown.into())),
    }
}

pub(crate) fn guess_format_impl(buffer: &[u8]) -> Option<ImageFormat> {
    for &(signature, format) in &MAGIC_BYTES {
        if buffer.starts_with(signature) {
            return Some(format);
        }
    }

    None
}

impl From<PathError> for ImageError {
    fn from(path: PathError) -> Self {
        let format_hint = match path {
            PathError::NoExtension => ImageFormatHint::Unknown,
            PathError::UnknownExtension(ext) => ImageFormatHint::PathExtension(ext.into()),
        };
        ImageError::Unsupported(format_hint.into())
    }
}
