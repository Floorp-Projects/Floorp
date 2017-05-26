#![warn(missing_docs)]

//! This crate converts a `Tm` struct to an `MsDosDateTime` and vice-versa
//!
//! MsDosDateTime is based on a FAT datetime and is a compact representation of a date.
//! It is currently mostly used in zip files.

extern crate time;
#[cfg(windows)] extern crate kernel32;
#[cfg(windows)] extern crate winapi;

use std::io;
use time::Tm;

/// Struct representing the date and time part of an MsDos datetime
#[derive(Copy, Clone, Debug)]
pub struct MsDosDateTime {
    /// Part representing the date
    pub datepart: u16,
    /// Part representing the time
    pub timepart: u16,
}

impl MsDosDateTime {
    /// Constructor of an MsDos datetime, from the raw representation
    pub fn new(time: u16, date: u16) -> MsDosDateTime {
        MsDosDateTime {
            datepart: date,
            timepart: time,
        }
    }
}

/// Trait to convert a time representation to and from a MsDosDateTime
pub trait TmMsDosExt : Sized {
    /// Convert a value to MsDosDateTime
    fn to_msdos(&self) -> Result<MsDosDateTime, io::Error>;
    /// Construct a value from an MsDosDateTime
    fn from_msdos(MsDosDateTime) -> Result<Self, io::Error>;
}

impl TmMsDosExt for Tm {
    fn to_msdos(&self) -> Result<MsDosDateTime, io::Error> {
        sys::tm_to_msdos(self)
    }

    fn from_msdos(ms: MsDosDateTime) -> Result<Self, io::Error> {
        sys::msdos_to_tm(ms)
    }
}

#[cfg(not(windows))]
mod sys {
    use super::MsDosDateTime;
    use time::{self, Tm};
    use std::io;

    pub fn msdos_to_tm(ms: MsDosDateTime) -> Result<Tm, io::Error> {
        let seconds = (ms.timepart & 0b0000000000011111) << 1;
        let minutes = (ms.timepart & 0b0000011111100000) >> 5;
        let hours =   (ms.timepart & 0b1111100000000000) >> 11;
        let days =    (ms.datepart & 0b0000000000011111) >> 0;
        let months =  (ms.datepart & 0b0000000111100000) >> 5;
        let years =   (ms.datepart & 0b1111111000000000) >> 9;

        // Month range: Dos [1..12], Tm [0..11]
        // Year zero: Dos 1980, Tm 1900

        let tm = Tm {
            tm_sec: seconds as i32,
            tm_min: minutes as i32,
            tm_hour: hours as i32,
            tm_mday: days as i32,
            tm_mon: months as i32 - 1,
            tm_year: years as i32 + 80,
            ..time::empty_tm()
        };

        // Re-parse the possibly incorrect timestamp to get a correct one.
        // This ensures every value will be in range
        // TODO: Check if this will only panic on Windows, or also other platforms
        Ok(time::at_utc(tm.to_timespec()))
    }

    pub fn tm_to_msdos(tm: &Tm) -> Result<MsDosDateTime, io::Error> {
        let timepart = ((tm.tm_sec >> 1) | (tm.tm_min << 5) | (tm.tm_hour << 11)) as u16;
        let datepart = (tm.tm_mday | ((tm.tm_mon + 1) << 5) | ((tm.tm_year - 80) << 9)) as u16;
        Ok(MsDosDateTime { datepart: datepart, timepart: timepart })
    }
}

#[cfg(windows)]
mod sys {
    use super::MsDosDateTime;
    use time::{self, Tm};
    use winapi::*;
    use kernel32::*;
    use std::io;

    pub fn msdos_to_tm(ms: MsDosDateTime) -> Result<Tm, io::Error> {
        let datepart: WORD = ms.datepart;
        let timepart: WORD = ms.timepart;
        let mut filetime: FILETIME = unsafe { ::std::mem::zeroed() };
        let mut systemtime: SYSTEMTIME = unsafe { ::std::mem::zeroed() };

        if unsafe { DosDateTimeToFileTime(datepart, timepart, &mut filetime) } == 0 {
            return Err(io::Error::last_os_error());
        }

        if unsafe { FileTimeToSystemTime(&filetime, &mut systemtime) } == 0 {
            return Err(io::Error::last_os_error());
        }

        Ok(Tm {
            tm_sec: systemtime.wSecond as i32,
            tm_min: systemtime.wMinute as i32,
            tm_hour: systemtime.wHour as i32,
            tm_mday: systemtime.wDay as i32,
            tm_wday: systemtime.wDayOfWeek as i32,
            tm_mon: (systemtime.wMonth - 1) as i32,
            tm_year: (systemtime.wYear - 1900) as i32,
            ..time::empty_tm()
        })
    }

    pub fn tm_to_msdos(tm: &Tm) -> Result<MsDosDateTime, io::Error> {
        let systemtime = SYSTEMTIME {
            wYear: (tm.tm_year + 1900) as WORD,
            wMonth: (tm.tm_mon + 1) as WORD,
            wDayOfWeek: tm.tm_wday as WORD,
            wDay: tm.tm_mday as WORD,
            wHour: tm.tm_hour as WORD,
            wMinute: tm.tm_min as WORD,
            wSecond: tm.tm_sec as WORD,
            wMilliseconds: 0,
        };
        let mut filetime = unsafe { ::std::mem::zeroed() };
        let mut datepart : WORD = 0;
        let mut timepart : WORD = 0;

        if unsafe { SystemTimeToFileTime(&systemtime, &mut filetime) } == 0 {
            return Err(io::Error::last_os_error());
        }

        if unsafe { FileTimeToDosDateTime(&filetime, &mut datepart, &mut timepart) } == 0 {
            return Err(io::Error::last_os_error());
        }

        Ok(MsDosDateTime { datepart: datepart, timepart: timepart })
    }
}


#[cfg(test)]
mod test {
    use super::*;
    use time::{self, Tm};

    fn check_date(input: Tm, day: i32, month: i32, year: i32) {
        assert_eq!(input.tm_mday, day);
        assert_eq!(input.tm_mon + 1, month);
        assert_eq!(input.tm_year + 1900, year);
    }

    fn check_time(input: Tm, hour: i32, minute: i32, second: i32) {
        assert_eq!(input.tm_hour, hour);
        assert_eq!(input.tm_min, minute);
        assert_eq!(input.tm_sec, second);
    }

    #[test]
    fn dos_zero() {
        // The 0 date is not a correct msdos date
        // We assert here that it does not panic. What it will return is undefined.
        let _ = Tm::from_msdos(MsDosDateTime::new(0, 0));
    }

    #[test]
    fn dos_smallest() {
        // This is the actual smallest date possible
        let tm = Tm::from_msdos(MsDosDateTime::new(0, 0b100001)).unwrap();
        check_date(tm, 1, 1, 1980);
        check_time(tm, 0, 0, 0);
    }

    #[test]
    fn dos_today() {
        let tm = Tm::from_msdos(MsDosDateTime::new(0b01001_100000_10101, 0b0100011_0110_11110)).unwrap();
        check_date(tm, 30, 6, 2015);
        check_time(tm, 9, 32, 42);
    }

    #[test]
    fn zero_dos() {
        let tm = Tm {
            tm_year: 80,
            tm_mon: 0,
            tm_mday: 1,
            tm_hour: 0,
            tm_min: 0,
            tm_sec: 0,
            ..time::empty_tm()
        };
        let ms = tm.to_msdos().unwrap();
        assert_eq!(ms.datepart, 0b100001);
        assert_eq!(ms.timepart, 0);
    }

    #[test]
    fn today_dos() {
        let tm = Tm {
            tm_year: 115,
            tm_mon: 5,
            tm_mday: 30,
            tm_hour: 9,
            tm_min: 32,
            tm_sec: 42,
            ..time::empty_tm()
        };
        let ms = tm.to_msdos().unwrap();
        assert_eq!(ms.datepart, 0b0100011_0110_11110);
        assert_eq!(ms.timepart, 0b01001_100000_10101);
    }
}
