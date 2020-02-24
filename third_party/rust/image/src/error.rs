//! Contains detailed error representation.
//!
//! See the main [`ImageError`] which contains a variant for each specialized error type. The
//! subtypes used in each variant are opaque by design. They can be roughly inspected through their
//! respective `kind` methods which work similar to `std::io::Error::kind`.
//!
//! The error interface makes it possible to inspect the error of an underlying decoder or encoder,
//! through the `Error::source` method. Note that this is not part of the stable interface and you
//! may not rely on a particular error value for a particular operation. This means mainly that
//! `image` does not promise to remain on a particular version of its underlying decoders but if
//! you ensure to use the same version of the dependency (or at least of the error type) through
//! external means then you could inspect the error type in slightly more detail.
//!
//! [`ImageError`]: enum.ImageError.html

use std::{fmt, io};
use std::error::Error;

use crate::color::ExtendedColorType;
use crate::image::ImageFormat;
use crate::utils::NonExhaustiveMarker;

/// The generic error type for image operations.
///
/// This high level enum allows, by variant matching, a rough separation of concerns between
/// underlying IO, the caller, format specifications, and the `image` implementation.
#[derive(Debug)]
pub enum ImageError {
    /// An error was encountered while decoding.
    ///
    /// This means that the input data did not conform to the specification of some image format,
    /// or that no format could be determined, or that it did not match format specific
    /// requirements set by the caller.
    Decoding(DecodingError),

    /// An error was encountered while encoding.
    ///
    /// The input image can not be encoded with the chosen format, for example because the
    /// specification has no representation for its color space or because a necessary conversion
    /// is ambiguous. In some cases it might also happen that the dimensions can not be used with
    /// the format.
    Encoding(EncodingError),

    /// An error was encountered in input arguments.
    ///
    /// This is a catch-all case for strictly internal operations such as scaling, conversions,
    /// etc. that involve no external format specifications.
    Parameter(ParameterError),

    /// Completing the operation would have required more resources than allowed.
    ///
    /// Errors of this type are limits set by the user or environment, *not* inherent in a specific
    /// format or operation that was executed.
    Limits(LimitError),

    /// An operation can not be completed by the chosen abstraction.
    ///
    /// This means that it might be possible for the operation to succeed in general but
    /// * it requires a disabled feature,
    /// * the implementation does not yet exist, or
    /// * no abstraction for a lower level could be found.
    Unsupported(UnsupportedError),

    /// An error occurred while interacting with the environment.
    IoError(io::Error),
}

/// The implementation for an operation was not provided.
///
/// See the variant [`Unsupported`] for more documentation.
///
/// [`Unsupported`]: enum.ImageError.html#variant.Unsupported
#[derive(Debug)]
pub struct UnsupportedError {
    format: ImageFormatHint,
    kind: UnsupportedErrorKind,
}

/// Details what feature is not supported.
#[derive(Clone, Debug, Hash, PartialEq)]
pub enum UnsupportedErrorKind {
    /// The required color type can not be handled.
    Color(ExtendedColorType),
    /// An image format is not supported.
    Format(ImageFormatHint),
    /// Some feature specified by string.
    /// This is discouraged and is likely to get deprecated (but not removed).
    GenericFeature(String),
    #[doc(hidden)]
    __NonExhaustive(NonExhaustiveMarker),
}

/// An error was encountered while encoding an image.
///
/// This is used as an opaque representation for the [`ImageError::Encoding`] variant. See its
/// documentation for more information.
///
/// [`ImageError::Encoding`]: enum.ImageError.html#variant.Encoding
#[derive(Debug)]
pub struct EncodingError {
    format: ImageFormatHint,
    underlying: Option<Box<dyn Error + Send + Sync>>,
}


/// An error was encountered in inputs arguments.
///
/// This is used as an opaque representation for the [`ImageError::Parameter`] variant. See its
/// documentation for more information.
///
/// [`ImageError::Parameter`]: enum.ImageError.html#variant.Parameter
#[derive(Debug)]
pub struct ParameterError {
    kind: ParameterErrorKind,
    underlying: Option<Box<dyn Error + Send + Sync>>,
}

/// Details how a parameter is malformed.
#[derive(Clone, Debug, Hash, PartialEq)]
pub enum ParameterErrorKind {
    /// Repeated an operation for which error that could not be cloned was emitted already.
    FailedAlready,
    /// The dimensions passed are wrong.
    DimensionMismatch,
    /// A string describing the parameter.
    /// This is discouraged and is likely to get deprecated (but not removed).
    Generic(String),
    #[doc(hidden)]
    /// Do not use this, not part of stability guarantees.
    __NonExhaustive(NonExhaustiveMarker),
}

/// An error was encountered while decoding an image.
///
/// This is used as an opaque representation for the [`ImageError::Decoding`] variant. See its
/// documentation for more information.
///
/// [`ImageError::Decoding`]: enum.ImageError.html#variant.Decoding
#[derive(Debug)]
pub struct DecodingError {
    format: ImageFormatHint,
    message: Option<Box<str>>,
    underlying: Option<Box<dyn Error + Send + Sync>>,
}

/// Completing the operation would have required more resources than allowed.
///
/// This is used as an opaque representation for the [`ImageError::Limits`] variant. See its
/// documentation for more information.
///
/// [`ImageError::Limits`]: enum.ImageError.html#variant.Limits
#[derive(Debug)]
pub struct LimitError {
    kind: LimitErrorKind,
    // do we need an underlying error?
}

/// Indicates the limit that prevented an operation from completing.
///
/// Note that this enumeration is not exhaustive and may in the future be extended to provide more
/// detailed information or to incorporate other resources types.
#[derive(Clone, Debug, Hash, PartialEq, Eq)]
#[allow(missing_copy_implementations)] // Might be non-Copy in the future.
pub enum LimitErrorKind {
    /// The resulting image exceed dimension limits in either direction.
    DimensionError,
    /// The operation would have performed an allocation larger than allowed.
    InsufficientMemory,
    #[doc(hidden)]
    /// Do not use this, not part of stability guarantees.
    __NonExhaustive(NonExhaustiveMarker),
}

/// A best effort representation for image formats.
#[derive(Clone, Debug, Hash, PartialEq)]
pub enum ImageFormatHint {
    /// The format is known exactly.
    Exact(ImageFormat),

    /// The format can be identified by a name.
    Name(String),

    /// A common path extension for the format is known.
    PathExtension(std::path::PathBuf),

    /// The format is not known or could not be determined.
    Unknown,

    #[doc(hidden)]
    __NonExhaustive(NonExhaustiveMarker),
}

// Internal implementation block for ImageError.
#[allow(non_upper_case_globals)]
#[allow(non_snake_case)]
impl ImageError {
    pub(crate) const InsufficientMemory: Self =
        ImageError::Limits(LimitError {
            kind: LimitErrorKind::InsufficientMemory,
        });

    pub(crate) const DimensionError: Self =
        ImageError::Parameter(ParameterError {
            kind: ParameterErrorKind::DimensionMismatch,
            underlying: None,
        });

    pub(crate) const ImageEnd: Self =
        ImageError::Parameter(ParameterError {
            kind: ParameterErrorKind::FailedAlready,
            underlying: None,
        });

    pub(crate) fn UnsupportedError(message: String) -> Self {
        ImageError::Unsupported(UnsupportedError::legacy_from_string(message))
    }

    pub(crate) fn UnsupportedColor(color: ExtendedColorType) -> Self {
        ImageError::Unsupported(UnsupportedError::from_format_and_kind(
            ImageFormatHint::Unknown,
            UnsupportedErrorKind::Color(color),
        ))
    }

    pub(crate) fn FormatError(message: String) -> Self {
        ImageError::Decoding(DecodingError::legacy_from_string(message))
    }
}

impl UnsupportedError {
    /// Create an `UnsupportedError` for an image with details on the unsupported feature.
    ///
    /// If the operation was not connected to a particular image format then the hint may be
    /// `Unknown`.
    pub fn from_format_and_kind(format: ImageFormatHint, kind: UnsupportedErrorKind) -> Self {
        UnsupportedError {
            format,
            kind,
        }
    }

    /// A shorthand for a generic feature without an image format.
    pub(crate) fn legacy_from_string(message: String) -> Self {
        UnsupportedError {
            format: ImageFormatHint::Unknown,
            kind: UnsupportedErrorKind::GenericFeature(message),
        }
    }

    /// Returns the corresponding `UnsupportedErrorKind` of the error.
    pub fn kind(&self) -> UnsupportedErrorKind {
        self.kind.clone()
    }

    /// Returns the image format associated with this error.
    pub fn format_hint(&self) -> ImageFormatHint {
        self.format.clone()
    }
}

impl DecodingError {
    /// Create a `DecodingError` that stems from an arbitrary error of an underlying decoder.
    pub fn new(
        format: ImageFormatHint,
        err: impl Into<Box<dyn Error + Send + Sync>>,
    ) -> Self {
        DecodingError {
            format,
            message: None,
            underlying: Some(err.into()),
        }
    }

    /// Create a `DecodingError` for an image format.
    ///
    /// The error will not contain any further information but is very easy to create.
    pub fn from_format_hint(format: ImageFormatHint) -> Self {
        DecodingError {
            format,
            message: None,
            underlying: None,
        }
    }

    /// Returns the image format associated with this error.
    pub fn format_hint(&self) -> ImageFormatHint {
        self.format.clone()
    }

    /// A shorthand for a string error without an image format.
    pub(crate) fn legacy_from_string(message: String) -> Self {
        DecodingError {
            format: ImageFormatHint::Unknown,
            message: Some(message.into_boxed_str()),
            underlying: None,
        }
    }

    /// Not quite legacy but also highly discouraged.
    /// This is just since the string typing is prevalent in the `image` decoders...
    // TODO: maybe a Cow? A constructor from `&'static str` wouldn't be too bad.
    pub(crate) fn with_message(
        format: ImageFormatHint,
        message: String,
    ) -> Self {
        DecodingError {
            format,
            message: Some(message.into_boxed_str()),
            underlying: None,
        }
    }

    fn get_message_or_default(&self) -> &str {
        match &self.message {
            Some(st) => st,
            None => "",
        }
    }
}

impl EncodingError {
    /// Create an `EncodingError` that stems from an arbitrary error of an underlying encoder.
    pub fn new(
        format: ImageFormatHint,
        err: impl Into<Box<dyn Error + Send + Sync>>,
    ) -> Self {
        EncodingError {
            format,
            underlying: Some(err.into()),
        }
    }

    /// Create a `DecodingError` for an image format.
    ///
    /// The error will not contain any further information but is very easy to create.
    pub fn from_format_hint(format: ImageFormatHint) -> Self {
        EncodingError {
            format,
            underlying: None,
        }
    }

    /// Return the image format associated with this error.
    pub fn format_hint(&self) -> ImageFormatHint {
        self.format.clone()
    }
}

impl ParameterError {
    /// Construct a `ParameterError` directly from a corresponding kind.
    pub fn from_kind(kind: ParameterErrorKind) -> Self {
        ParameterError {
            kind,
            underlying: None,
        }
    }

    /// Returns the corresponding `ParameterErrorKind` of the error.
    pub fn kind(&self) -> ParameterErrorKind {
        self.kind.clone()
    }
}

impl LimitError {
    /// Construct a generic `LimitError` directly from a corresponding kind.
    pub fn from_kind(kind: LimitErrorKind) -> Self {
        LimitError {
            kind,
        }
    }

    /// Returns the corresponding `LimitErrorKind` of the error.
    pub fn kind(&self) -> LimitErrorKind {
        self.kind.clone()
    }
}

impl From<io::Error> for ImageError {
    fn from(err: io::Error) -> ImageError {
        ImageError::IoError(err)
    }
}

impl From<ImageFormat> for ImageFormatHint {
    fn from(format: ImageFormat) -> Self {
        ImageFormatHint::Exact(format)
    }
}

impl From<&'_ std::path::Path> for ImageFormatHint {
    fn from(path: &'_ std::path::Path) -> Self {
        match path.extension() {
            Some(ext) => ImageFormatHint::PathExtension(ext.into()),
            None => ImageFormatHint::Unknown,
        }
    }
}

impl From<ImageFormatHint> for UnsupportedError {
    fn from(hint: ImageFormatHint) -> Self {
        UnsupportedError {
            format: hint.clone(),
            kind: UnsupportedErrorKind::Format(hint),
        }
    }
}

/// Result of an image decoding/encoding process
pub type ImageResult<T> = Result<T, ImageError>;

impl fmt::Display for ImageError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        match self {
            ImageError::IoError(err) => err.fmt(fmt),
            ImageError::Decoding(err) => err.fmt(fmt),
            ImageError::Encoding(err) => err.fmt(fmt),
            ImageError::Parameter(err) => err.fmt(fmt),
            ImageError::Limits(err) => err.fmt(fmt),
            ImageError::Unsupported(err) => err.fmt(fmt),
        }
    }
}

impl Error for ImageError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            ImageError::IoError(err) => err.source(),
            ImageError::Decoding(err) => err.source(),
            ImageError::Encoding(err) => err.source(),
            ImageError::Parameter(err) => err.source(),
            ImageError::Limits(err) => err.source(),
            ImageError::Unsupported(err) => err.source(),
        }
    }
}

impl fmt::Display for UnsupportedError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        match &self.kind {
            UnsupportedErrorKind::Format(ImageFormatHint::Unknown) => write!(
                fmt,
                "The image format could not be determined",
            ),
            UnsupportedErrorKind::Format(format @ ImageFormatHint::PathExtension(_)) => write!(
                fmt,
                "The file extension {} was not recognized as an image format",
                format,
            ),
            UnsupportedErrorKind::Format(format) => write!(
                fmt,
                "The image format {} is not supported",
                format,
            ),
            UnsupportedErrorKind::Color(color) => write!(
                fmt,
                "The decoder for {} does not support the color type `{:?}`",
                self.format,
                color,
            ),
            UnsupportedErrorKind::GenericFeature(message) => {
                match &self.format {
                    ImageFormatHint::Unknown => write!(
                        fmt,
                        "The decoder does not support the format feature {}",
                        message,
                    ),
                    other => write!(
                        fmt,
                        "The decoder for {} does not support the format features {}",
                        other,
                        message,
                    ),
                }
            },
            UnsupportedErrorKind::__NonExhaustive(marker) => match marker._private {},
        }
    }
}

impl Error for UnsupportedError { }

impl fmt::Display for ParameterError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        match &self.kind {
            ParameterErrorKind::DimensionMismatch => write!(
                fmt,
                "The Image's dimensions are either too \
                 small or too large"
            ),
            ParameterErrorKind::FailedAlready => write!(
                fmt,
                "The end the image stream has been reached due to a previous error"
            ),
            ParameterErrorKind::Generic(message) => write!(
                fmt,
                "The parameter is malformed: {}",
                message,
            ),
            ParameterErrorKind::__NonExhaustive(marker) => match marker._private {},
        }?;

        if let Some(underlying) = &self.underlying {
            write!(fmt, "\n{}", underlying)?;
        }

        Ok(())
    }
}

impl Error for ParameterError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match &self.underlying {
            None => None,
            Some(source) => Some(&**source),
        }
    }
}

impl fmt::Display for EncodingError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        match &self.underlying {
            Some(underlying) => write!(
                fmt,
                "Format error encoding {}:\n{}",
                self.format,
                underlying,
            ),
            None => write!(
                fmt,
                "Format error encoding {}",
                self.format,
            ),
        }
    }
}

impl Error for EncodingError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match &self.underlying {
            None => None,
            Some(source) => Some(&**source),
        }
    }
}

impl fmt::Display for DecodingError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        match &self.underlying {
            None => match self.format {
                ImageFormatHint::Unknown => write!(
                    fmt,
                    "Format error: {}",
                    self.get_message_or_default(),
                ),
                _ => write!(
                    fmt,
                    "Format error decoding {}: {}",
                    self.format,
                    self.get_message_or_default(),
                ),
            },
            Some(underlying) => write!(
                fmt,
                "Format error decoding {}: {}\n{}",
                self.format,
                self.get_message_or_default(),
                underlying,
            ),
        }
    }
}

impl Error for DecodingError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match &self.underlying {
            None => None,
            Some(source) => Some(&**source),
        }
    }
}

impl fmt::Display for LimitError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        match self.kind {
            LimitErrorKind::InsufficientMemory => write!(fmt, "Insufficient memory"),
            LimitErrorKind::DimensionError => write!(fmt, "Image is too large"),
            LimitErrorKind::__NonExhaustive(marker) => match marker._private {},
        }
    }
}

impl Error for LimitError { }

impl fmt::Display for ImageFormatHint {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        match self {
            ImageFormatHint::Exact(format) => write!(fmt, "{:?}", format),
            ImageFormatHint::Name(name) => write!(fmt, "`{}`", name),
            ImageFormatHint::PathExtension(ext) => write!(fmt, "`.{:?}`", ext),
            ImageFormatHint::Unknown => write!(fmt, "`Unknown`"),
            ImageFormatHint::__NonExhaustive(marker) => match marker._private {},
        }
    }
}

#[cfg(test)]
mod tests {
    use std::mem;
    use super::*;

    #[allow(dead_code)]
    // This will fail to compile if the size of this type is large.
    const ASSERT_SMALLISH: usize = [0][(mem::size_of::<ImageError>() >= 200) as usize];

    #[test]
    fn test_send_sync_stability() {
        fn assert_send_sync<T: Send + Sync>() { }

        assert_send_sync::<ImageError>();
    }
}
