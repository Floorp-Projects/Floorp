use std::{io, result};

#[derive(Debug)]
pub enum Error {
    IoError(io::Error),
    Timeout,
    Parse,
}

impl From<Error> for io::Error {
    fn from(val: Error) -> Self {
        match val {
            Error::IoError(e) => e,
            Error::Timeout => io::Error::new(io::ErrorKind::Other, "devd poll timeout"),
            Error::Parse => io::Error::new(io::ErrorKind::Other, "devd parse error"),
        }
    }
}

impl From<io::Error> for Error {
    fn from(err: io::Error) -> Error {
        Error::IoError(err)
    }
}

pub type Result<T> = result::Result<T, Error>;
