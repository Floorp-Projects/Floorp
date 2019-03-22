// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.

//! Wrap several flavors of Windows error into a `Result`.

use std::fmt;

use failure::Fail;

use winapi::shared::minwindef::DWORD;
use winapi::shared::winerror::{
    ERROR_SUCCESS, FACILITY_WIN32, HRESULT, HRESULT_FROM_WIN32, SUCCEEDED, S_OK,
};
use winapi::um::errhandlingapi::GetLastError;

/// An error code, optionally with information about the failing call.
#[derive(Clone, Debug, Eq, Fail, PartialEq)]
pub struct ErrorAndSource<T: ErrorCode> {
    code: T,
    function: Option<&'static str>,
    file_line: Option<FileLine>,
}

/// A wrapper for an error code.
pub trait ErrorCode:
    Copy + fmt::Debug + Eq + PartialEq + fmt::Display + Send + Sync + 'static
{
    type InnerT: Copy + Eq + PartialEq;

    fn get(&self) -> Self::InnerT;
}

impl<T> ErrorAndSource<T>
where
    T: ErrorCode,
{
    /// Get the underlying error code.
    pub fn code(&self) -> T::InnerT {
        self.code.get()
    }

    /// Add the name of the failing function to the error.
    pub fn function(self, function: &'static str) -> Self {
        Self {
            function: Some(function),
            ..self
        }
    }

    /// Get the name of the failing function, if known.
    pub fn get_function(&self) -> Option<&'static str> {
        self.function
    }

    /// Add the source file name and line number of the call to the error.
    pub fn file_line(self, file: &'static str, line: u32) -> Self {
        Self {
            file_line: Some(FileLine(file, line)),
            ..self
        }
    }

    /// Get the source file name and line number of the failing call.
    pub fn get_file_line(&self) -> &Option<FileLine> {
        &self.file_line
    }
}

impl<T> fmt::Display for ErrorAndSource<T>
where
    T: ErrorCode,
{
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        if let Some(function) = self.function {
            if let Some(ref file_line) = self.file_line {
                write!(f, "{} ", file_line)?;
            }

            write!(f, "{} ", function)?;

            write!(f, "error: ")?;
        }

        write!(f, "{}", self.code)?;

        Ok(())
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct FileLine(pub &'static str, pub u32);

impl fmt::Display for FileLine {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "{}:{}", self.0, self.1)
    }
}

/// A [Win32 error code](https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-erref/18d8fbe8-a967-4f1c-ae50-99ca8e491d2d),
/// usually from `GetLastError()`.
///
/// Includes optional function name, source file name, and line number. See
/// [`ErrorAndSource`](struct.ErrorAndSource.html) for additional methods.
pub type Win32Error = ErrorAndSource<Win32ErrorInner>;

impl Win32Error {
    /// Create from an error code.
    pub fn new(code: DWORD) -> Self {
        Win32Error {
            code: Win32ErrorInner(code),
            function: None,
            file_line: None,
        }
    }

    /// Create from `GetLastError()`
    pub fn get_last_error() -> Self {
        Win32Error::new(unsafe { GetLastError() })
    }
}

#[doc(hidden)]
#[repr(transparent)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Win32ErrorInner(DWORD);

impl ErrorCode for Win32ErrorInner {
    type InnerT = DWORD;

    fn get(&self) -> DWORD {
        self.0
    }
}

impl fmt::Display for Win32ErrorInner {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "{:#010x}", self.0)
    }
}

/// An [HRESULT error code](https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-erref/0642cb2f-2075-4469-918c-4441e69c548a).
/// These usually come from COM APIs.
///
/// Includes optional function name, source file name, and line number. See
/// [`ErrorAndSource`](struct.ErrorAndSource.html) for additional methods.
pub type HResult = ErrorAndSource<HResultInner>;

impl HResult {
    /// Create from an `HRESULT`.
    pub fn new(hr: HRESULT) -> Self {
        HResult {
            code: HResultInner(hr),
            function: None,
            file_line: None,
        }
    }

    /// Get the result code portion of the `HRESULT`
    pub fn extract_code(&self) -> HRESULT {
        // from winerror.h HRESULT_CODE macro
        self.code.0 & 0xFFFF
    }

    /// Get the facility portion of the `HRESULT`
    pub fn extract_facility(&self) -> HRESULT {
        // from winerror.h HRESULT_FACILITY macro
        (self.code.0 >> 16) & 0x1fff
    }

    /// If the `HResult` corresponds to a Win32 error, convert.
    ///
    /// Returns the original `HResult` as an error on failure.
    pub fn try_into_win32_err(self) -> Result<Win32Error, Self> {
        let code = if self.code() == S_OK {
            // Special case, facility is not set.
            ERROR_SUCCESS
        } else if self.extract_facility() == FACILITY_WIN32 {
            self.extract_code() as DWORD
        } else {
            return Err(self);
        };

        Ok(Win32Error {
            code: Win32ErrorInner(code),
            function: self.function,
            file_line: self.file_line,
        })
    }
}

#[doc(hidden)]
#[repr(transparent)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct HResultInner(HRESULT);

impl ErrorCode for HResultInner {
    type InnerT = HRESULT;

    fn get(&self) -> HRESULT {
        self.0
    }
}

impl fmt::Display for HResultInner {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "HRESULT {:#010x}", self.0)
    }
}

/// Extra functions to work with a `Result<T, ErrorAndSource>`.
pub trait ResultExt<T, E> {
    type Code;

    /// Add the name of the failing function to the error.
    fn function(self, function: &'static str) -> Self;

    /// Add the source file name and line number of the call to the error.
    fn file_line(self, file: &'static str, line: u32) -> Self;

    /// Replace `Err(code)` with `Ok(replacement)`.
    fn allow_err(self, code: Self::Code, replacement: T) -> Self;

    /// Replace `Err(code)` with `Ok(replacement())`.
    fn allow_err_with<F>(self, code: Self::Code, replacement: F) -> Self
    where
        F: FnOnce() -> T;
}

impl<T, EC> ResultExt<T, ErrorAndSource<EC>> for Result<T, ErrorAndSource<EC>>
where
    EC: ErrorCode,
{
    type Code = EC::InnerT;

    fn function(self, function: &'static str) -> Self {
        self.map_err(|e| e.function(function))
    }

    fn file_line(self, file: &'static str, line: u32) -> Self {
        self.map_err(|e| e.file_line(file, line))
    }

    fn allow_err(self, code: Self::Code, replacement: T) -> Self {
        self.or_else(|e| {
            if e.code() == code {
                Ok(replacement)
            } else {
                Err(e)
            }
        })
    }

    fn allow_err_with<F>(self, code: Self::Code, replacement: F) -> Self
    where
        F: FnOnce() -> T,
    {
        self.or_else(|e| {
            if e.code() == code {
                Ok(replacement())
            } else {
                Err(e)
            }
        })
    }
}

impl From<Win32Error> for HResult {
    fn from(win32_error: Win32Error) -> Self {
        HResult {
            code: HResultInner(HRESULT_FROM_WIN32(win32_error.code())),
            function: win32_error.function,
            file_line: win32_error.file_line,
        }
    }
}

/// Convert an `HRESULT` into a `Result`.
pub fn succeeded_or_err(hr: HRESULT) -> Result<HRESULT, HResult> {
    if !SUCCEEDED(hr) {
        Err(HResult::new(hr))
    } else {
        Ok(hr)
    }
}

/// Call a function that returns an `HRESULT`, convert to a `Result`.
///
/// The error will be augmented with the name of the function and the file and line number of
/// the macro usage.
///
/// # Example
/// ```no_run
/// # extern crate winapi;
/// # use std::ptr;
/// # use winapi::um::combaseapi::CoUninitialize;
/// # use winapi::um::objbase::CoInitialize;
/// # use comedy::{check_succeeded, HResult};
/// #
/// fn coinit() -> Result<(), HResult> {
///     unsafe {
///         check_succeeded!(CoInitialize(ptr::null_mut()))?;
///
///         CoUninitialize();
///     }
///     Ok(())
/// }
/// ```
#[macro_export]
macro_rules! check_succeeded {
    ($f:ident ( $($arg:expr),* )) => {
        {
            use $crate::error::ResultExt;
            $crate::error::succeeded_or_err($f($($arg),*))
                .function(stringify!($f))
                .file_line(file!(), line!())
        }
    };

    // support for trailing comma in argument list
    ($f:ident ( $($arg:expr),+ , )) => {
        $crate::check_succeeded!($f($($arg),+))
    };
}

/// Convert an integer return value into a `Result`, using `GetLastError()` if zero.
pub fn true_or_last_err<T>(rv: T) -> Result<T, Win32Error>
where
    T: Eq,
    T: From<bool>,
{
    if rv == T::from(false) {
        Err(Win32Error::get_last_error())
    } else {
        Ok(rv)
    }
}

/// Call a function that returns a integer, convert to a `Result`, using `GetLastError()` if zero.
///
/// The error will be augmented with the name of the function and the file and line number of
/// the macro usage.
///
/// # Example
/// ```no_run
/// # extern crate winapi;
/// # use winapi::shared::minwindef::BOOL;
/// # use winapi::um::fileapi::FlushFileBuffers;
/// # use winapi::um::winnt::HANDLE;
/// # use comedy::{check_true, Win32Error};
/// #
/// fn flush(file: HANDLE) -> Result<(), Win32Error> {
///     unsafe {
///         check_true!(FlushFileBuffers(file))?;
///     }
///     Ok(())
/// }
/// ```
#[macro_export]
macro_rules! check_true {
    ($f:ident ( $($arg:expr),* )) => {
        {
            use $crate::error::ResultExt;
            $crate::error::true_or_last_err($f($($arg),*))
                .function(stringify!($f))
                .file_line(file!(), line!())
        }
    };

    // support for trailing comma in argument list
    ($f:ident ( $($arg:expr),+ , )) => {
        $crate::check_true!($f($($arg),+))
    };
}
