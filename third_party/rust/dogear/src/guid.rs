// Copyright 2018-2019 Mozilla

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use std::{
    cmp::Ordering,
    fmt,
    hash::{Hash, Hasher},
    ops, str,
};

use crate::error::{ErrorKind, Result};

/// A GUID for an item in a bookmark tree.
#[derive(Clone)]
pub struct Guid(Repr);

/// Indicates if the GUID is valid. Implemented for byte slices and GUIDs.
pub trait IsValidGuid {
    fn is_valid_guid(&self) -> bool;
}

/// The internal representation of a GUID. Valid GUIDs are 12 bytes, and contain
/// only Base64url characters; we can store them on the stack without a heap
/// allocation. However, both local and remote items might have invalid GUIDs,
/// in which case we fall back to a heap-allocated string.
#[derive(Clone)]
enum Repr {
    Valid([u8; 12]),
    Invalid(Box<str>),
}

/// The Places root GUID, used to root all items in a bookmark tree.
pub const ROOT_GUID: Guid = Guid(Repr::Valid(*b"root________"));

/// The bookmarks toolbar GUID.
pub const TOOLBAR_GUID: Guid = Guid(Repr::Valid(*b"toolbar_____"));

/// The bookmarks menu GUID.
pub const MENU_GUID: Guid = Guid(Repr::Valid(*b"menu________"));

/// The "Other Bookmarks" GUID, used to hold items without a parent.
pub const UNFILED_GUID: Guid = Guid(Repr::Valid(*b"unfiled_____"));

/// The mobile bookmarks GUID.
pub const MOBILE_GUID: Guid = Guid(Repr::Valid(*b"mobile______"));

const VALID_GUID_BYTES: [u8; 255] = [
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
];

impl Guid {
    /// Converts a UTF-8 byte slice to a GUID.
    pub fn from_utf8(b: &[u8]) -> Result<Guid> {
        let repr = if b.is_valid_guid() {
            let mut bytes = [0u8; 12];
            bytes.copy_from_slice(b);
            Repr::Valid(bytes)
        } else {
            match str::from_utf8(b) {
                Ok(s) => Repr::Invalid(s.into()),
                Err(err) => return Err(err.into()),
            }
        };
        Ok(Guid(repr))
    }

    /// Converts a UTF-16 byte slice to a GUID.
    pub fn from_utf16(b: &[u16]) -> Result<Guid> {
        let repr = if b.is_valid_guid() {
            let mut bytes = [0u8; 12];
            for (index, &byte) in b.iter().enumerate() {
                if byte > u16::from(u8::max_value()) {
                    return Err(ErrorKind::InvalidByte(byte).into());
                }
                bytes[index] = byte as u8;
            }
            Repr::Valid(bytes)
        } else {
            match String::from_utf16(b) {
                Ok(s) => Repr::Invalid(s.into()),
                Err(err) => return Err(err.into()),
            }
        };
        Ok(Guid(repr))
    }

    /// Returns the GUID as a byte slice.
    #[inline]
    pub fn as_bytes(&self) -> &[u8] {
        match self.0 {
            Repr::Valid(ref bytes) => bytes,
            Repr::Invalid(ref s) => s.as_bytes(),
        }
    }

    /// Returns the GUID as a string slice.
    #[inline]
    pub fn as_str(&self) -> &str {
        // We actually could use from_utf8_unchecked here, and depending on how
        // often we end up doing this, it's arguable that we should. We know
        // already this is valid utf8, since we know that we only ever create
        // these while respecting is_valid (and moreover, we assert that
        // `s.is_char_boundary(12)` in `Guid::from`).
        match self.0 {
            Repr::Valid(ref bytes) => str::from_utf8(bytes).unwrap(),
            Repr::Invalid(ref s) => s,
        }
    }

    /// Indicates if the GUID is one of the four Places user content roots.
    #[inline]
    pub fn is_user_content_root(&self) -> bool {
        self == TOOLBAR_GUID || self == MENU_GUID || self == UNFILED_GUID || self == MOBILE_GUID
    }
}

impl IsValidGuid for Guid {
    #[inline]
    fn is_valid_guid(&self) -> bool {
        match self.0 {
            Repr::Valid(_) => true,
            Repr::Invalid(_) => false,
        }
    }
}

impl<T: Copy + Into<usize>> IsValidGuid for [T] {
    /// Equivalent to `PlacesUtils.isValidGuid`.
    #[inline]
    fn is_valid_guid(&self) -> bool {
        self.len() == 12
            && self
                .iter()
                .all(|&byte| VALID_GUID_BYTES.get(byte.into()).map_or(false, |&b| b == 1))
    }
}

impl From<String> for Guid {
    #[inline]
    fn from(s: String) -> Guid {
        Guid::from(s.as_str())
    }
}

impl<'a> From<&'a str> for Guid {
    #[inline]
    fn from(s: &'a str) -> Guid {
        let repr = if s.as_bytes().is_valid_guid() {
            assert!(s.is_char_boundary(12));
            let mut bytes = [0u8; 12];
            bytes.copy_from_slice(s.as_bytes());
            Repr::Valid(bytes)
        } else {
            Repr::Invalid(s.into())
        };
        Guid(repr)
    }
}

impl AsRef<str> for Guid {
    #[inline]
    fn as_ref(&self) -> &str {
        self.as_str()
    }
}

impl AsRef<[u8]> for Guid {
    #[inline]
    fn as_ref(&self) -> &[u8] {
        self.as_bytes()
    }
}

impl ops::Deref for Guid {
    type Target = str;

    #[inline]
    fn deref(&self) -> &str {
        self.as_str()
    }
}

impl Ord for Guid {
    fn cmp(&self, other: &Guid) -> Ordering {
        self.as_bytes().cmp(other.as_bytes())
    }
}

impl PartialOrd for Guid {
    fn partial_cmp(&self, other: &Guid) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

// Allow direct comparison with str
impl PartialEq<str> for Guid {
    #[inline]
    fn eq(&self, other: &str) -> bool {
        self.as_bytes() == other.as_bytes()
    }
}

impl<'a> PartialEq<&'a str> for Guid {
    #[inline]
    fn eq(&self, other: &&'a str) -> bool {
        self == *other
    }
}

impl PartialEq for Guid {
    #[inline]
    fn eq(&self, other: &Guid) -> bool {
        self.as_bytes() == other.as_bytes()
    }
}

impl<'a> PartialEq<Guid> for &'a Guid {
    #[inline]
    fn eq(&self, other: &Guid) -> bool {
        *self == other
    }
}

impl Eq for Guid {}

impl Hash for Guid {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.as_bytes().hash(state);
    }
}

// The default Debug impl is pretty unhelpful here.
impl fmt::Debug for Guid {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Guid({:?})", self.as_str())
    }
}

impl fmt::Display for Guid {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn is_valid() {
        let valid_guids = &[
            "bookmarkAAAA",
            "menu________",
            "__folderBB__",
            "queryAAAAAAA",
        ];
        for s in valid_guids {
            assert!(s.as_bytes().is_valid_guid(), "{:?} should validate", s);
            assert!(Guid::from(*s).is_valid_guid());
        }

        let invalid_guids = &["bookmarkAAA", "folder!", "b@dgu1d!"];
        for s in invalid_guids {
            assert!(!s.as_bytes().is_valid_guid(), "{:?} should not validate", s);
            assert!(!Guid::from(*s).is_valid_guid());
        }

        let invalid_guid_bytes: &[[u8; 12]] =
            &[[113, 117, 101, 114, 121, 65, 225, 193, 65, 65, 65, 65]];
        for bytes in invalid_guid_bytes {
            assert!(!bytes.is_valid_guid(), "{:?} should not validate", bytes);
            Guid::from_utf8(bytes).expect_err("Should not make GUID from invalid UTF-8");
        }
    }
}
