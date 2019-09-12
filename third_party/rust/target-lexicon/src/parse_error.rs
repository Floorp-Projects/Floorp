use alloc::string::String;

/// An error returned from parsing a triple.
#[derive(Fail, Clone, Debug, PartialEq, Eq)]
#[allow(missing_docs)]
pub enum ParseError {
    #[fail(display = "Unrecognized architecture: {}", _0)]
    UnrecognizedArchitecture(String),
    #[fail(display = "Unrecognized vendor: {}", _0)]
    UnrecognizedVendor(String),
    #[fail(display = "Unrecognized operating system: {}", _0)]
    UnrecognizedOperatingSystem(String),
    #[fail(display = "Unrecognized environment: {}", _0)]
    UnrecognizedEnvironment(String),
    #[fail(display = "Unrecognized binary format: {}", _0)]
    UnrecognizedBinaryFormat(String),
    #[fail(display = "Unrecognized field: {}", _0)]
    UnrecognizedField(String),
}
