use crate::consts::{SW_CONDITIONS_NOT_SATISFIED, SW_NO_ERROR, SW_WRONG_DATA, SW_WRONG_LENGTH};
use crate::ctap2::commands::CommandError;
use std::fmt;
use std::io;
use std::path;

#[allow(unused)]
#[derive(Debug, PartialEq, Eq)]
pub enum ApduErrorStatus {
    ConditionsNotSatisfied,
    WrongData,
    WrongLength,
    Unknown([u8; 2]),
}

impl ApduErrorStatus {
    pub fn from(status: [u8; 2]) -> Result<(), ApduErrorStatus> {
        match status {
            s if s == SW_NO_ERROR => Ok(()),
            s if s == SW_CONDITIONS_NOT_SATISFIED => Err(ApduErrorStatus::ConditionsNotSatisfied),
            s if s == SW_WRONG_DATA => Err(ApduErrorStatus::WrongData),
            s if s == SW_WRONG_LENGTH => Err(ApduErrorStatus::WrongLength),
            other => Err(ApduErrorStatus::Unknown(other)),
        }
    }
}

impl fmt::Display for ApduErrorStatus {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            ApduErrorStatus::ConditionsNotSatisfied => write!(f, "Apdu: condition not satisfied"),
            ApduErrorStatus::WrongData => write!(f, "Apdu: wrong data"),
            ApduErrorStatus::WrongLength => write!(f, "Apdu: wrong length"),
            ApduErrorStatus::Unknown(ref u) => write!(f, "Apdu: unknown error: {u:?}"),
        }
    }
}

#[allow(unused)]
#[derive(Debug)]
pub enum HIDError {
    /// Transport replied with a status not expected
    DeviceError,
    UnexpectedInitReplyLen,
    NonceMismatch,
    DeviceNotInitialized,
    DeviceNotSupported,
    UnsupportedCommand,
    UnexpectedVersion,
    IO(Option<path::PathBuf>, io::Error),
    UnexpectedCmd(u8),
    Command(CommandError),
    ApduStatus(ApduErrorStatus),
}

impl From<io::Error> for HIDError {
    fn from(e: io::Error) -> HIDError {
        HIDError::IO(None, e)
    }
}

impl From<CommandError> for HIDError {
    fn from(e: CommandError) -> HIDError {
        HIDError::Command(e)
    }
}

impl From<ApduErrorStatus> for HIDError {
    fn from(e: ApduErrorStatus) -> HIDError {
        HIDError::ApduStatus(e)
    }
}

impl fmt::Display for HIDError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            HIDError::UnexpectedInitReplyLen => {
                write!(f, "Error: Unexpected reply len when initilizaling")
            }
            HIDError::NonceMismatch => write!(f, "Error: Nonce mismatch"),
            HIDError::DeviceError => write!(f, "Error: device returned error"),
            HIDError::DeviceNotInitialized => write!(f, "Error: using not initiliazed device"),
            HIDError::DeviceNotSupported => {
                write!(f, "Error: requested operation is not available on device")
            }
            HIDError::UnexpectedVersion => write!(f, "Error: Unexpected protocol version"),
            HIDError::UnsupportedCommand => {
                write!(f, "Error: command is not supported on this device")
            }
            HIDError::IO(ref p, ref e) => write!(f, "Error: Ioerror({p:?}): {e}"),
            HIDError::Command(ref e) => write!(f, "Error: Error issuing command: {e}"),
            HIDError::UnexpectedCmd(s) => write!(f, "Error: Unexpected status: {s}"),
            HIDError::ApduStatus(ref status) => {
                write!(f, "Error: Unexpected apdu status: {status:?}")
            }
        }
    }
}
