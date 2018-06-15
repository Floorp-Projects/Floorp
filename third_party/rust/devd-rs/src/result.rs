use std::{io, result};

#[derive(Debug)]
pub enum Error {
    IoError(io::Error),
    Timeout,
    Parse,
}

impl Into<io::Error> for Error {
    fn into(self) -> io::Error {
        match self {
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
