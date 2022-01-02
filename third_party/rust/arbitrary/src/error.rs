use std::{error, fmt};

/// An enumeration of buffer creation errors
#[derive(Debug, Clone, Copy)]
#[non_exhaustive]
pub enum Error {
    /// No choices were provided to the Unstructured::choose call
    EmptyChoose,
    /// There was not enough underlying data to fulfill some request for raw
    /// bytes.
    NotEnoughData,
    /// The input bytes were not of the right format
    IncorrectFormat,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::EmptyChoose => write!(
                f,
                "`arbitrary::Unstructured::choose` must be given a non-empty set of choices"
            ),
            Error::NotEnoughData => write!(
                f,
                "There is not enough underlying raw data to construct an `Arbitrary` instance"
            ),
            Error::IncorrectFormat => write!(
                f,
                "The raw data is not of the correct format to construct this type"
            ),
        }
    }
}

impl error::Error for Error {}

/// A `Result` with the error type fixed as `arbitrary::Error`.
///
/// Either an `Ok(T)` or `Err(arbitrary::Error)`.
pub type Result<T> = std::result::Result<T, Error>;
