use crate::{constants::MAX_PRECISION_U32, Decimal};
use alloc::string::String;
use core::fmt;

/// Error type for the library.
#[derive(Clone, Debug, PartialEq)]
pub enum Error {
    ErrorString(String),
    ExceedsMaximumPossibleValue,
    LessThanMinimumPossibleValue,
    Underflow,
    ScaleExceedsMaximumPrecision(u32),
    ConversionTo(String),
}

impl<S> From<S> for Error
where
    S: Into<String>,
{
    #[inline]
    fn from(from: S) -> Self {
        Self::ErrorString(from.into())
    }
}

#[cold]
pub(crate) fn tail_error(from: &'static str) -> Result<Decimal, Error> {
    Err(from.into())
}

#[cfg(feature = "std")]
impl std::error::Error for Error {}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            Self::ErrorString(ref err) => f.pad(err),
            Self::ExceedsMaximumPossibleValue => {
                write!(f, "Number exceeds maximum value that can be represented.")
            }
            Self::LessThanMinimumPossibleValue => {
                write!(f, "Number less than minimum value that can be represented.")
            }
            Self::Underflow => {
                write!(f, "Number has a high precision that can not be represented.")
            }
            Self::ScaleExceedsMaximumPrecision(ref scale) => {
                write!(
                    f,
                    "Scale exceeds the maximum precision allowed: {} > {}",
                    scale, MAX_PRECISION_U32
                )
            }
            Self::ConversionTo(ref type_name) => {
                write!(f, "Error while converting to {}", type_name)
            }
        }
    }
}
