// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.

//! Windows [`FILETIME`](https://docs.microsoft.com/en-us/windows/desktop/api/minwinbase/ns-minwinbase-filetime)
//! and [`SYSTEMTIME`](https://docs.microsoft.com/en-us/windows/desktop/api/minwinbase/ns-minwinbase-systemtime)
//! string and binary serialization
//!
//! A transparent wrapper is provided for each type, with
//! `Display` for [`SystemTimeUTC`](struct.SystemTimeUTC.html) and
//! `Ord` and `Eq` for [`FileTime`](struct.FileTime.html).
//!
//! # serde #
//!
//! Use the `filetime_serde` feature to derive `Serialize` and `Deserialize`, you can then
//! derive them for structs containing `FILETIME` and `SYSTEMTIME` like so:
//!
//! ```
//! # fn main() {}
//! #
//! # #[cfg(feature = "filetime_serde")]
//! # extern crate serde_derive;
//! # extern crate winapi;
//! #
//! # #[cfg(feature = "filetime_serde")]
//! # mod test {
//! use filetime_win::{FileTimeSerde, SystemTimeSerde};
//! use serde_derive::{Deserialize, Serialize};
//! use winapi::shared::minwindef::FILETIME;
//! use winapi::um::minwinbase::SYSTEMTIME;
//!
//! #[derive(Serialize, Deserialize)]
//! struct SerdeTest {
//!     #[serde(with = "FileTimeSerde")]
//!     ft: FILETIME,
//!     #[serde(with = "SystemTimeSerde")]
//!     st: SYSTEMTIME,
//! }
//! # }
//! ```
extern crate comedy;
#[cfg(feature = "filetime_serde")]
extern crate serde;
#[cfg(feature = "filetime_serde")]
extern crate serde_derive;
extern crate winapi;

use std::cmp::Ordering;
use std::fmt::{Debug, Display, Formatter, Result};
use std::mem;
use std::result;

use comedy::check_true;

use winapi::shared::minwindef::FILETIME;
#[cfg(feature = "filetime_serde")]
use winapi::shared::minwindef::{DWORD, WORD};
use winapi::shared::ntdef::ULARGE_INTEGER;
use winapi::um::minwinbase::SYSTEMTIME;
use winapi::um::sysinfoapi::GetSystemTime;
use winapi::um::timezoneapi::{FileTimeToSystemTime, SystemTimeToFileTime};

#[cfg(feature = "filetime_serde")]
use serde_derive::{Deserialize, Serialize};

#[cfg(feature = "filetime_serde")]
#[allow(non_snake_case)]
#[derive(Serialize, Deserialize)]
#[serde(remote = "FILETIME")]
pub struct FileTimeSerde {
    dwLowDateTime: DWORD,
    dwHighDateTime: DWORD,
}

/// Wraps `FILETIME`
#[derive(Copy, Clone)]
#[cfg_attr(feature = "filetime_serde", derive(Serialize, Deserialize))]
#[repr(transparent)]
pub struct FileTime(
    #[cfg_attr(feature = "filetime_serde", serde(with = "FileTimeSerde"))] pub FILETIME,
);

#[cfg(feature = "filetime_serde")]
#[allow(non_snake_case)]
#[derive(Serialize, Deserialize)]
#[serde(remote = "SYSTEMTIME")]
pub struct SystemTimeSerde {
    wYear: WORD,
    wMonth: WORD,
    wDayOfWeek: WORD,
    wDay: WORD,
    wHour: WORD,
    wMinute: WORD,
    wSecond: WORD,
    wMilliseconds: WORD,
}

/// Wraps `SYSTEMTIME`
///
/// The `SYSTEMTIME` struct can be UTC or local time, but `SystemTimeUTC` should only be used for
/// UTC.
///
#[derive(Copy, Clone)]
#[cfg_attr(feature = "filetime_serde", derive(Serialize, Deserialize))]
#[repr(transparent)]
pub struct SystemTimeUTC(
    #[cfg_attr(feature = "filetime_serde", serde(with = "SystemTimeSerde"))] pub SYSTEMTIME,
);

impl FileTime {
    /// Convert to raw integer
    ///
    /// `FILETIME` is 100-nanosecond intervals since January 1, 1601 (UTC), but if the high
    /// bit is 1 there may be a different interpretation.
    pub fn to_u64(self) -> u64 {
        unsafe {
            let mut v: ULARGE_INTEGER = mem::zeroed();
            v.s_mut().LowPart = self.0.dwLowDateTime;
            v.s_mut().HighPart = self.0.dwHighDateTime;
            *v.QuadPart()
        }
    }

    /// Convert to `SystemTimeUTC` via `FileTimeToSystemTime()`
    pub fn to_system_time_utc(self) -> result::Result<SystemTimeUTC, comedy::Win32Error> {
        unsafe {
            let mut system_time = mem::zeroed();

            check_true!(FileTimeToSystemTime(&self.0, &mut system_time))?;

            Ok(SystemTimeUTC(system_time))
        }
    }
}

impl PartialEq for FileTime {
    fn eq(&self, other: &FileTime) -> bool {
        self.0.dwLowDateTime == other.0.dwLowDateTime
            && self.0.dwHighDateTime == other.0.dwHighDateTime
    }
}

impl Eq for FileTime {}

impl PartialOrd for FileTime {
    fn partial_cmp(&self, other: &FileTime) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for FileTime {
    fn cmp(&self, other: &FileTime) -> Ordering {
        self.to_u64().cmp(&other.to_u64())
    }
}

impl Debug for FileTime {
    fn fmt(&self, f: &mut Formatter) -> Result {
        write!(
            f,
            "FileTime {{ dwLowDateTime: {:?}, dwHighDateTime: {:?} }}",
            self.0.dwLowDateTime, self.0.dwHighDateTime
        )
    }
}

impl SystemTimeUTC {
    /// Get current system time in UTC via `GetSystemTime()`
    ///
    /// "Because the system time can be adjusted either forward or backward, do not compare
    /// system time readings to determine elapsed time."
    pub fn now() -> SystemTimeUTC {
        unsafe {
            let mut system_time = mem::zeroed();
            GetSystemTime(&mut system_time);
            SystemTimeUTC(system_time)
        }
    }

    /// Convert to `FileTime` via `SystemTimeToFileTime()`
    pub fn to_file_time(&self) -> result::Result<FileTime, comedy::Win32Error> {
        unsafe {
            let mut file_time = mem::zeroed();

            check_true!(SystemTimeToFileTime(&self.0, &mut file_time))?;

            Ok(FileTime(file_time))
        }
    }
}

impl Debug for SystemTimeUTC {
    fn fmt(&self, f: &mut Formatter) -> Result {
        write!(
            f,
            concat!(
                "SystemTimeUTC {{ ",
                "wYear: {:?}, wMonth: {:?}, wDayOfWeek: {:?}, wDay: {:?}, ",
                "wHour: {:?}, wMinute: {:?}, wSecond: {:?}, wMilliseconds: {:?} }}"
            ),
            self.0.wYear,
            self.0.wMonth,
            self.0.wDayOfWeek,
            self.0.wDay,
            self.0.wHour,
            self.0.wMinute,
            self.0.wSecond,
            self.0.wMilliseconds
        )
    }
}

/// Format as ISO 8601 date and time: `YYYY-MM-DDThh:mm:ss.fffZ`
impl Display for SystemTimeUTC {
    fn fmt(&self, f: &mut Formatter) -> Result {
        write!(
            f,
            "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z",
            self.0.wYear,
            self.0.wMonth,
            self.0.wDay,
            self.0.wHour,
            self.0.wMinute,
            self.0.wSecond,
            self.0.wMilliseconds
        )
    }
}

#[cfg(test)]
mod tests {
    use super::{FileTime, SystemTimeUTC};
    use winapi::shared::minwindef::FILETIME;
    use winapi::um::minwinbase::SYSTEMTIME;

    #[test]
    fn roundtrip() {
        let ft = SystemTimeUTC::now().to_file_time().unwrap();

        assert_eq!(ft, ft);
        assert_eq!(ft, ft.to_system_time_utc().unwrap().to_file_time().unwrap());
    }

    #[test]
    fn next_year() {
        let st_now = SystemTimeUTC::now();
        let st_next_year = SystemTimeUTC(SYSTEMTIME {
            wYear: st_now.0.wYear + 1,
            ..st_now.0
        });

        let ft_now = st_now.to_file_time().unwrap();
        let ft_next_year = st_next_year.to_file_time().unwrap();
        assert!(ft_next_year > ft_now);
    }

    #[test]
    fn non_time_filetime() {
        let ft = FileTime(FILETIME {
            dwLowDateTime: 0xFFFF_FFFFu32,
            dwHighDateTime: 0xFFFF_FFFFu32,
        });

        ft.to_system_time_utc().expect_err("should have failed");
    }
}
