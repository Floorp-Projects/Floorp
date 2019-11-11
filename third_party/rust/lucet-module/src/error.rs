use failure::Fail;

/// Module data (de)serialization errors.
#[derive(Debug, Fail)]
pub enum Error {
    #[fail(display = "Sparse data contained a page with length other than 4096")]
    IncorrectPageSize,
    #[fail(display = "Deserialization error: {}", _0)]
    DeserializationError(#[cause] bincode::Error),
    #[fail(display = "Serialization error: {}", _0)]
    SerializationError(#[cause] bincode::Error),
    #[cfg(feature = "signature_checking")]
    #[fail(display = "Module signature error: {}", _0)]
    ModuleSignatureError(#[cause] minisign::PError),
    #[fail(display = "I/O error: {}", _0)]
    IOError(#[cause] std::io::Error),
}
