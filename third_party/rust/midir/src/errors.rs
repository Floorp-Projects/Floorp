use std::error::Error;
use std::fmt;

const INVALID_PORT_MSG: &str = "invalid port";
const PORT_OUT_OF_RANGE_MSG: &str = "provided port number was out of range";
const CANNOT_RETRIEVE_PORT_NAME_MSG: &str = "unknown error when trying to retrieve the port name";

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
/// An error that can occur during initialization (i.e., while
/// creating a `MidiInput` or `MidiOutput` object).
pub struct InitError;

impl Error for InitError {}

impl fmt::Display for InitError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        "MIDI support could not be initialized".fmt(f)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
/// An error that can occur when retrieving information about
/// available ports.
pub enum PortInfoError {
    PortNumberOutOfRange, // TODO: don't expose this
    InvalidPort,
    CannotRetrievePortName,
}

impl Error for PortInfoError {}

impl fmt::Display for PortInfoError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            PortInfoError::PortNumberOutOfRange => PORT_OUT_OF_RANGE_MSG.fmt(f),
            PortInfoError::InvalidPort => INVALID_PORT_MSG.fmt(f),
            PortInfoError::CannotRetrievePortName => CANNOT_RETRIEVE_PORT_NAME_MSG.fmt(f),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
/// The kind of error for a `ConnectError`.
pub enum ConnectErrorKind {
    InvalidPort,
    Other(&'static str)
}

impl ConnectErrorKind {}

impl fmt::Display for ConnectErrorKind {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            ConnectErrorKind::InvalidPort => INVALID_PORT_MSG.fmt(f),
            ConnectErrorKind::Other(msg) => msg.fmt(f)
        }
    }
}

/// An error that can occur when trying to connect to a port.
pub struct ConnectError<T> {
    kind: ConnectErrorKind,
    inner: T
}

impl<T> ConnectError<T> {
    pub fn new(kind: ConnectErrorKind, inner: T) -> ConnectError<T> {
        ConnectError { kind: kind, inner: inner }
    }
    
    /// Helper method to create ConnectErrorKind::Other.
    pub fn other(msg: &'static str, inner: T) -> ConnectError<T> {
        Self::new(ConnectErrorKind::Other(msg), inner)
    }
    
    pub fn kind(&self) -> ConnectErrorKind {
        self.kind
    }
    
    pub fn into_inner(self) -> T {
        self.inner
    }
}

impl<T> fmt::Debug for ConnectError<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        self.kind.fmt(f)
    }
}

impl<T> fmt::Display for ConnectError<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.kind.fmt(f)
    }
}

impl<T> Error for ConnectError<T> {}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
/// An error that can occur when sending MIDI messages.
pub enum SendError {
    InvalidData(&'static str),
    Other(&'static str)
}

impl Error for SendError {}

impl fmt::Display for SendError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            SendError::InvalidData(msg) | SendError::Other(msg) => msg.fmt(f)
        }
    }
}
