use std::fs::File;
use std::io::{self, BufRead, BufReader, Cursor, Read, Seek, SeekFrom};
use std::path::Path;

use crate::dynimage::DynamicImage;
use crate::image::ImageFormat;
use crate::{ImageError, ImageResult};

use super::free_functions;

/// A multi-format image reader.
///
/// Wraps an input reader to facilitate automatic detection of an image's format, appropriate
/// decoding method, and dispatches into the set of supported [`ImageDecoder`] implementations.
///
/// ## Usage
///
/// Opening a file, deducing the format based on the file path automatically, and trying to decode
/// the image contained can be performed by constructing the reader and immediately consuming it.
///
/// ```no_run
/// # use image::ImageError;
/// # use image::io::Reader;
/// # fn main() -> Result<(), ImageError> {
/// let image = Reader::open("path/to/image.png")?
///     .decode()?;
/// # Ok(()) }
/// ```
///
/// It is also possible to make a guess based on the content. This is especially handy if the
/// source is some blob in memory and you have constructed the reader in another way. Here is an
/// example with a `pnm` black-and-white subformat that encodes its pixel matrix with ascii values.
///
#[cfg_attr(feature = "pnm", doc = "```")]
#[cfg_attr(not(feature = "pnm"), doc = "```no_run")]
/// # use image::ImageError;
/// # use image::io::Reader;
/// # fn main() -> Result<(), ImageError> {
/// use std::io::Cursor;
/// use image::ImageFormat;
///
/// let raw_data = b"P1 2 2\n\
///     0 1\n\
///     1 0\n";
///
/// let mut reader = Reader::new(Cursor::new(raw_data))
///     .with_guessed_format()
///     .expect("Cursor io never fails");
/// assert_eq!(reader.format(), Some(ImageFormat::Pnm));
///
/// let image = reader.decode()?;
/// # Ok(()) }
/// ```
///
/// As a final fallback or if only a specific format must be used, the reader always allows manual
/// specification of the supposed image format with [`set_format`].
///
/// [`set_format`]: #method.set_format
/// [`ImageDecoder`]: ../trait.ImageDecoder.html
pub struct Reader<R: Read> {
    /// The reader.
    inner: R,
    /// The format, if one has been set or deduced.
    format: Option<ImageFormat>,
}

impl<R: Read> Reader<R> {
    /// Create a new image reader without a preset format.
    ///
    /// It is possible to guess the format based on the content of the read object with
    /// [`guess_format`], or to set the format directly with [`set_format`].
    ///
    /// [`guess_format`]: #method.guess_format
    /// [`set_format`]: method.set_format
    pub fn new(reader: R) -> Self {
        Reader {
            inner: reader,
            format: None,
        }
    }

    /// Construct a reader with specified format.
    pub fn with_format(reader: R, format: ImageFormat) -> Self {
        Reader {
            inner: reader,
            format: Some(format),
        }
    }

    /// Get the currently determined format.
    pub fn format(&self) -> Option<ImageFormat> {
        self.format
    }

    /// Supply the format as which to interpret the read image.
    pub fn set_format(&mut self, format: ImageFormat) {
        self.format = Some(format);
    }

    /// Remove the current information on the image format.
    ///
    /// Note that many operations require format information to be present and will return e.g. an
    /// `ImageError::UnsupportedError` when the image format has not been set.
    pub fn clear_format(&mut self) {
        self.format = None;
    }

    /// Unwrap the reader.
    pub fn into_inner(self) -> R {
        self.inner
    }
}

impl Reader<BufReader<File>> {
    /// Open a file to read, format will be guessed from path.
    ///
    /// This will not attempt any io operation on the opened file.
    ///
    /// If you want to inspect the content for a better guess on the format, which does not depend
    /// on file extensions, follow this call with a call to [`guess_format`].
    ///
    /// [`guess_format`]: #method.guess_format
    pub fn open<P>(path: P) -> io::Result<Self> where P: AsRef<Path> {
        Self::open_impl(path.as_ref())
    }

    fn open_impl(path: &Path) -> io::Result<Self> {
        let file = File::open(path)?;
        Ok(Reader {
            inner: BufReader::new(file),
            format: ImageFormat::from_path(path).ok(),
        })
    }
}

impl<R: BufRead + Seek> Reader<R> {
    /// Make a format guess based on the content, replacing it on success.
    ///
    /// Returns `Ok` with the guess if no io error occurs. Additionally, replaces the current
    /// format if the guess was successful. If the guess was not unable to determine a format then
    /// the current format of the reader is unchanged.
    ///
    /// Returns an error if the underlying reader fails. The format is unchanged. The error is a
    /// `std::io::Error` and not `ImageError` since the only error case is an error when the
    /// underlying reader seeks.
    ///
    /// When an error occurs, the reader may not have been properly reset and it is potentially
    /// hazardous to continue with more io.
    ///
    /// ## Usage
    ///
    /// This supplements the path based type deduction from [`open`] with content based deduction.
    /// This is more common in Linux and UNIX operating systems and also helpful if the path can
    /// not be directly controlled.
    ///
    /// ```no_run
    /// # use image::ImageError;
    /// # use image::io::Reader;
    /// # fn main() -> Result<(), ImageError> {
    /// let image = Reader::open("image.unknown")?
    ///     .with_guessed_format()?
    ///     .decode()?;
    /// # Ok(()) }
    /// ```
    pub fn with_guessed_format(mut self) -> io::Result<Self> {
        let format = self.guess_format()?;
        // Replace format if found, keep current state if not.
        self.format = format.or(self.format);
        Ok(self)
    }

    fn guess_format(&mut self) -> io::Result<Option<ImageFormat>> {
        let mut start = [0; 16];

        // Save current offset, read start, restore offset.
        let cur = self.inner.seek(SeekFrom::Current(0))?;
        let len = io::copy(
            // Accept shorter files but read at most 16 bytes.
            &mut self.inner.by_ref().take(16),
            &mut Cursor::new(&mut start[..]))?;
        self.inner.seek(SeekFrom::Start(cur))?;

        Ok(free_functions::guess_format_impl(&start[..len as usize]))
    }

    /// Read the image dimensions.
    ///
    /// Uses the current format to construct the correct reader for the format.
    ///
    /// If no format was determined, returns an `ImageError::UnsupportedError`.
    pub fn into_dimensions(mut self) -> ImageResult<(u32, u32)> {
        let format = self.require_format()?;
        free_functions::image_dimensions_with_format_impl(self.inner, format)
    }

    /// Read the image (replaces `load`).
    ///
    /// Uses the current format to construct the correct reader for the format.
    ///
    /// If no format was determined, returns an `ImageError::UnsupportedError`.
    pub fn decode(mut self) -> ImageResult<DynamicImage> {
        let format = self.require_format()?;
        free_functions::load(self.inner, format)
    }

    fn require_format(&mut self) -> ImageResult<ImageFormat> {
        self.format.ok_or_else(||
            ImageError::UnsupportedError("Unable to determine image format".into()))
    }
}
