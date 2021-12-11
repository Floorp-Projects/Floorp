// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.

//! Windows GUID/CLSID/IID string and binary serialization
//!
//! [`Guid`](struct.Guid.html) transparently wraps
//! [`GUID`](https://msdn.microsoft.com/en-us/library/windows/desktop/aa373931(v=vs.85).aspx).
//!
//! Implements `Display` and `FromStr` string conversion, also `Hash` and `Eq`.
//!
//! Curly braces (`{}`) are optional for `FromStr`.
//!
//! # serde #
//!
//! Use the `guid_serde` feature to derive `Serialize` and `Deserialize`, you can then
//! derive them for structs containing `GUID` like so:
//!
//! ```
//! # fn main() {}
//! #
//! # #[cfg(feature = "guid_serde")]
//! # extern crate serde_derive;
//! # extern crate winapi;
//! #
//! # #[cfg(feature = "guid_serde")]
//! # mod test {
//! use guid_win::GUIDSerde;
//! use serde_derive::{Deserialize, Serialize};
//! use winapi::shared::guiddef::GUID;
//!
//! #[derive(Serialize, Deserialize)]
//! struct SerdeTest {
//!     #[serde(with = "GUIDSerde")]
//!     guid: GUID,
//! }
//! # }
//! ```

extern crate comedy;
#[cfg(feature = "guid_serde")]
extern crate serde;
#[cfg(feature = "guid_serde")]
extern crate serde_derive;
extern crate winapi;

use std::ffi::{OsStr, OsString};
use std::fmt::{Debug, Display, Error, Formatter, Result};
use std::hash::{Hash, Hasher};
use std::mem;
use std::os::windows::ffi::{OsStrExt, OsStringExt};
use std::result;
use std::str::FromStr;

use comedy::check_succeeded;

use winapi::ctypes;
use winapi::shared::guiddef::GUID;
use winapi::um::combaseapi::{CLSIDFromString, StringFromGUID2};

#[cfg(feature = "guid_serde")]
use serde_derive::{Deserialize, Serialize};

const GUID_STRING_CHARACTERS: usize = 38;

#[cfg(feature = "guid_serde")]
#[allow(non_snake_case)]
#[derive(Serialize, Deserialize)]
#[serde(remote = "GUID")]
pub struct GUIDSerde {
    Data1: ctypes::c_ulong,
    Data2: ctypes::c_ushort,
    Data3: ctypes::c_ushort,
    Data4: [ctypes::c_uchar; 8],
}

/// Wraps `GUID`
#[derive(Clone)]
#[cfg_attr(feature = "guid_serde", derive(Serialize, Deserialize))]
#[repr(transparent)]
pub struct Guid(#[cfg_attr(feature = "guid_serde", serde(with = "GUIDSerde"))] pub GUID);

impl PartialEq for Guid {
    fn eq(&self, other: &Guid) -> bool {
        self.0.Data1 == other.0.Data1
            && self.0.Data2 == other.0.Data2
            && self.0.Data3 == other.0.Data3
            && self.0.Data4 == other.0.Data4
    }
}

impl Eq for Guid {}

impl Hash for Guid {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.0.Data1.hash(state);
        self.0.Data2.hash(state);
        self.0.Data3.hash(state);
        self.0.Data4.hash(state);
    }
}

impl Debug for Guid {
    fn fmt(&self, f: &mut Formatter) -> Result {
        write!(f, "{:?}", unsafe {
            &mem::transmute::<Guid, [u8; mem::size_of::<Guid>()]>(self.clone())
        })
    }
}

/// Output a string via `StringFromGUID2()`
impl Display for Guid {
    fn fmt(&self, f: &mut Formatter) -> Result {
        let mut s: [u16; GUID_STRING_CHARACTERS + 1] = unsafe { mem::uninitialized() };

        let len = unsafe {
            StringFromGUID2(
                &(*self).0 as *const _ as *mut _,
                s.as_mut_ptr(),
                s.len() as ctypes::c_int,
            )
        };

        if len <= 0 {
            return Err(Error);
        }
        // len is number of characters, including the null terminator

        let s = &s[..len as usize - 1];
        // TODO: no reason to expect this to fail, maybe just unwrap()
        if let Ok(s) = OsString::from_wide(&s).into_string() {
            f.write_str(&s)
        } else {
            Err(Error)
        }
    }
}

/// Read from a string via `CLSIDFromString()`
///
/// Braces (`{}`) are added if missing.
impl FromStr for Guid {
    type Err = comedy::error::HResult;

    fn from_str(s: &str) -> result::Result<Self, Self::Err> {
        let mut guid = unsafe { mem::uninitialized() };

        let braced;
        let s = if s.starts_with('{') {
            s
        } else {
            braced = format!("{{{}}}", s);
            braced.as_str()
        };
        let s: Vec<_> = OsStr::new(s).encode_wide().chain(Some(0)).collect();

        unsafe { check_succeeded!(CLSIDFromString(s.as_ptr(), &mut guid)) }?;

        Ok(Guid(guid))
    }
}

#[cfg(test)]
mod test {
    use super::Guid;
    use std::str::FromStr;

    #[test]
    fn without_braces() {
        let uuid = "F1BD1079-9F01-4BDC-8036-F09B70095066";
        let guid = Guid::from_str(uuid).unwrap();
        assert_eq!(format!("{}", guid), format!("{{{}}}", uuid));
    }

    #[test]
    fn with_braces() {
        let uuid = "{F1BD1079-9F01-4BDC-8036-F09B70095066}";
        let guid = Guid::from_str(uuid).unwrap();
        assert_eq!(format!("{}", guid), uuid);
    }

    #[test]
    fn format_error() {
        let uuid = "foo";
        Guid::from_str(uuid).unwrap_err();
    }
}
