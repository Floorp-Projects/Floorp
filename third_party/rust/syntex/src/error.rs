use std::error;
use std::fmt;
use std::io;

use errors::DiagnosticBuilder;

#[derive(Debug)]
pub enum Error {
    Parse,
    Expand,
    Io(io::Error),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            Error::Parse | Error::Expand => {
                write!(f, "{}", error::Error::description(self))
            }
            Error::Io(ref err) => err.fmt(f),
        }
    }
}

impl error::Error for Error {
    fn description(&self) -> &str {
        match *self {
            Error::Parse => "failed to parse input",
            Error::Expand => "failed to expand input",
            Error::Io(ref err) => err.description(),
        }
    }
}

impl From<io::Error> for Error {
    fn from(err: io::Error) -> Self {
        Error::Io(err)
    }
}

impl<'a> From<DiagnosticBuilder<'a>> for Error {
    fn from(mut diagnostic: DiagnosticBuilder<'a>) -> Self {
        diagnostic.emit();
        Error::Parse
    }
}
