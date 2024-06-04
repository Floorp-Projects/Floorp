//! Various error types returned by methods in the time crate.

mod component_range;
mod conversion_range;
mod different_variant;
#[cfg(feature = "formatting")]
mod format;
#[cfg(feature = "local-offset")]
mod indeterminate_offset;
#[cfg(all(any(feature = "formatting", feature = "parsing"), feature = "alloc"))]
mod invalid_format_description;
mod invalid_variant;
#[cfg(feature = "parsing")]
mod parse;
#[cfg(feature = "parsing")]
mod parse_from_description;
#[cfg(feature = "parsing")]
mod try_from_parsed;

use core::fmt;

pub use component_range::ComponentRange;
pub use conversion_range::ConversionRange;
pub use different_variant::DifferentVariant;
#[cfg(feature = "formatting")]
pub use format::Format;
#[cfg(feature = "local-offset")]
pub use indeterminate_offset::IndeterminateOffset;
#[cfg(all(any(feature = "formatting", feature = "parsing"), feature = "alloc"))]
pub use invalid_format_description::InvalidFormatDescription;
pub use invalid_variant::InvalidVariant;
#[cfg(feature = "parsing")]
pub use parse::Parse;
#[cfg(feature = "parsing")]
pub use parse_from_description::ParseFromDescription;
#[cfg(feature = "parsing")]
pub use try_from_parsed::TryFromParsed;

#[cfg(feature = "parsing")]
use crate::internal_macros::bug;

/// A unified error type for anything returned by a method in the time crate.
///
/// This can be used when you either don't know or don't care about the exact error returned.
/// `Result<_, time::Error>` (or its alias `time::Result<_>`) will work in these situations.
#[allow(missing_copy_implementations, variant_size_differences)]
#[non_exhaustive]
#[derive(Debug)]
pub enum Error {
    #[allow(missing_docs)]
    ConversionRange(ConversionRange),
    #[allow(missing_docs)]
    ComponentRange(ComponentRange),
    #[cfg(feature = "local-offset")]
    #[allow(missing_docs)]
    IndeterminateOffset(IndeterminateOffset),
    #[cfg(feature = "formatting")]
    #[allow(missing_docs)]
    Format(Format),
    #[cfg(feature = "parsing")]
    #[allow(missing_docs)]
    ParseFromDescription(ParseFromDescription),
    #[cfg(feature = "parsing")]
    #[allow(missing_docs)]
    #[non_exhaustive]
    #[deprecated(
        since = "0.3.28",
        note = "no longer output. moved to the `ParseFromDescription` variant"
    )]
    UnexpectedTrailingCharacters,
    #[cfg(feature = "parsing")]
    #[allow(missing_docs)]
    TryFromParsed(TryFromParsed),
    #[cfg(all(any(feature = "formatting", feature = "parsing"), feature = "alloc"))]
    #[allow(missing_docs)]
    InvalidFormatDescription(InvalidFormatDescription),
    #[allow(missing_docs)]
    DifferentVariant(DifferentVariant),
    #[allow(missing_docs)]
    InvalidVariant(InvalidVariant),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::ConversionRange(e) => e.fmt(f),
            Self::ComponentRange(e) => e.fmt(f),
            #[cfg(feature = "local-offset")]
            Self::IndeterminateOffset(e) => e.fmt(f),
            #[cfg(feature = "formatting")]
            Self::Format(e) => e.fmt(f),
            #[cfg(feature = "parsing")]
            Self::ParseFromDescription(e) => e.fmt(f),
            #[cfg(feature = "parsing")]
            #[allow(deprecated)]
            Self::UnexpectedTrailingCharacters => bug!("variant should not be used"),
            #[cfg(feature = "parsing")]
            Self::TryFromParsed(e) => e.fmt(f),
            #[cfg(all(any(feature = "formatting", feature = "parsing"), feature = "alloc"))]
            Self::InvalidFormatDescription(e) => e.fmt(f),
            Self::DifferentVariant(e) => e.fmt(f),
            Self::InvalidVariant(e) => e.fmt(f),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for Error {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::ConversionRange(err) => Some(err),
            Self::ComponentRange(err) => Some(err),
            #[cfg(feature = "local-offset")]
            Self::IndeterminateOffset(err) => Some(err),
            #[cfg(feature = "formatting")]
            Self::Format(err) => Some(err),
            #[cfg(feature = "parsing")]
            Self::ParseFromDescription(err) => Some(err),
            #[cfg(feature = "parsing")]
            #[allow(deprecated)]
            Self::UnexpectedTrailingCharacters => bug!("variant should not be used"),
            #[cfg(feature = "parsing")]
            Self::TryFromParsed(err) => Some(err),
            #[cfg(all(any(feature = "formatting", feature = "parsing"), feature = "alloc"))]
            Self::InvalidFormatDescription(err) => Some(err),
            Self::DifferentVariant(err) => Some(err),
            Self::InvalidVariant(err) => Some(err),
        }
    }
}
