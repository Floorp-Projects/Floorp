use ffi;
use std::{error, fmt};
use std::ffi::NulError;
use std::os::raw::c_int;

pub type Result<T> = ::std::result::Result<T, Error>;

/// An enumeration of possible errors that can happen when working with cubeb.
#[derive(PartialEq, Eq, Clone, Debug, Copy)]
pub enum ErrorCode {
    /// GenericError
    Error,
    /// Requested format is invalid
    InvalidFormat,
    /// Requested parameter is invalid
    InvalidParameter,
    /// Requested operation is not supported
    NotSupported,
    /// Requested device is unavailable
    DeviceUnavailable,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Error {
    code: ErrorCode,
}

impl Error {
    pub fn error() -> Self {
        Error {
            code: ErrorCode::Error,
        }
    }
    pub fn invalid_format() -> Self {
        Error {
            code: ErrorCode::InvalidFormat,
        }
    }
    pub fn invalid_parameter() -> Self {
        Error {
            code: ErrorCode::InvalidParameter,
        }
    }
    pub fn not_supported() -> Self {
        Error {
            code: ErrorCode::NotSupported,
        }
    }
    pub fn device_unavailable() -> Self {
        Error {
            code: ErrorCode::DeviceUnavailable,
        }
    }

    pub unsafe fn from_raw(code: c_int) -> Error {
        let code = match code {
            ffi::CUBEB_ERROR_INVALID_FORMAT => ErrorCode::InvalidFormat,
            ffi::CUBEB_ERROR_INVALID_PARAMETER => ErrorCode::InvalidParameter,
            ffi::CUBEB_ERROR_NOT_SUPPORTED => ErrorCode::NotSupported,
            ffi::CUBEB_ERROR_DEVICE_UNAVAILABLE => ErrorCode::DeviceUnavailable,
            // Everything else is just the generic error
            _ => ErrorCode::Error,
        };

        Error { code: code }
    }

    pub fn code(&self) -> ErrorCode {
        self.code
    }

    pub fn raw_code(&self) -> c_int {
        match self.code {
            ErrorCode::Error => ffi::CUBEB_ERROR,
            ErrorCode::InvalidFormat => ffi::CUBEB_ERROR_INVALID_FORMAT,
            ErrorCode::InvalidParameter => ffi::CUBEB_ERROR_INVALID_PARAMETER,
            ErrorCode::NotSupported => ffi::CUBEB_ERROR_NOT_SUPPORTED,
            ErrorCode::DeviceUnavailable => ffi::CUBEB_ERROR_DEVICE_UNAVAILABLE,
        }
    }
}

impl Default for Error {
    fn default() -> Self {
        Error::error()
    }
}

impl error::Error for Error {
    fn description(&self) -> &str {
        match self.code {
            ErrorCode::Error => "Error",
            ErrorCode::InvalidFormat => "Invalid format",
            ErrorCode::InvalidParameter => "Invalid parameter",
            ErrorCode::NotSupported => "Not supported",
            ErrorCode::DeviceUnavailable => "Device unavailable",
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self)
    }
}

impl From<ErrorCode> for Error {
    fn from(code: ErrorCode) -> Error {
        Error { code: code }
    }
}

impl From<NulError> for Error {
    fn from(_: NulError) -> Error {
        unsafe { Error::from_raw(ffi::CUBEB_ERROR) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ffi;

    #[test]
    fn test_from_raw() {
        macro_rules! test {
            ( $($raw:ident => $err:ident),* ) => {{
                $(
                    let e = unsafe { Error::from_raw(ffi::$raw) };
                    assert_eq!(e.raw_code(), ffi::$raw);
                    assert_eq!(e.code(), ErrorCode::$err);
                )*
            }};
        }
        test!(CUBEB_ERROR => Error,
              CUBEB_ERROR_INVALID_FORMAT => InvalidFormat,
              CUBEB_ERROR_INVALID_PARAMETER => InvalidParameter,
              CUBEB_ERROR_NOT_SUPPORTED => NotSupported,
              CUBEB_ERROR_DEVICE_UNAVAILABLE => DeviceUnavailable
        );
    }

    #[test]
    fn test_from_error_code() {
        macro_rules! test {
            ( $($raw:ident => $err:ident),* ) => {{
                $(
                    let e = Error::from(ErrorCode::$err);
                    assert_eq!(e.raw_code(), ffi::$raw);
                    assert_eq!(e.code(), ErrorCode::$err);
                )*
            }};
        }
        test!(CUBEB_ERROR => Error,
              CUBEB_ERROR_INVALID_FORMAT => InvalidFormat,
              CUBEB_ERROR_INVALID_PARAMETER => InvalidParameter,
              CUBEB_ERROR_NOT_SUPPORTED => NotSupported,
              CUBEB_ERROR_DEVICE_UNAVAILABLE => DeviceUnavailable
        );
    }
}
