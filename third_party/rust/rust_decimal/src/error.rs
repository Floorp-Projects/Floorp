use std::{error, fmt};

/// Error type for the library.
#[derive(Clone, Debug)]
pub struct Error {
    message: String,
}

impl Error {
    /// Instantiate an error with the specified error message.
    ///
    /// This function is only available within the crate as there should never
    /// be a need to create this error outside of the library.
    pub(crate) fn new<S: Into<String>>(message: S) -> Error {
        Error {
            message: message.into(),
        }
    }
}

impl error::Error for Error {
    fn description(&self) -> &str {
        &self.message
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        f.pad(&self.message)
    }
}
