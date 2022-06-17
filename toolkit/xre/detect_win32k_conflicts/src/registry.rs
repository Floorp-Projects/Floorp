/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module contains helpers for exploring the Windows Registry

use super::error::DetectConflictError;

use std::ffi::{OsStr, OsString};
use std::os::windows::ffi::{OsStrExt, OsStringExt};

use winapi::{
    shared::{
        minwindef::HKEY,
        winerror::{ERROR_FILE_NOT_FOUND, ERROR_NO_MORE_ITEMS, ERROR_SUCCESS},
    },
    um::{
        winnt::{KEY_READ, REG_BINARY, REG_DWORD, REG_SZ},
        winreg::{
            RegCloseKey, RegEnumKeyExW, RegGetValueW, RegOpenKeyExW, RegQueryInfoKeyW,
            HKEY_LOCAL_MACHINE, RRF_RT_REG_BINARY, RRF_RT_REG_DWORD, RRF_RT_REG_SZ,
        },
    },
};

/// An open key in the Windows Registry
///
/// (Note that Windows ref-counts keys internally, so we don't need subkeys to "hang on" to
/// their parents)
#[derive(Debug)]
pub struct RegKey {
    /// The Win32 handle of the open key
    handle: HKEY,
}

impl RegKey {
    /// Open the global HKEY_LOCAL_MACHINE handle
    pub fn root_local_machine() -> RegKey {
        RegKey {
            handle: HKEY_LOCAL_MACHINE,
        }
    }
    /// Try to open a subkey relative to this key with read-only access rights
    ///
    /// Returns `None` if the subkey doesn't exist
    pub fn try_open_subkey(
        &self,
        subkey: impl AsRef<OsStr>,
    ) -> Result<Option<RegKey>, DetectConflictError> {
        let win32_subkey = to_win32_string(subkey.as_ref());

        let mut subkey_handle = std::ptr::null_mut();
        let rv = unsafe {
            RegOpenKeyExW(
                self.handle,
                win32_subkey.as_ptr(),
                0, // options
                KEY_READ,
                &mut subkey_handle,
            )
        }
        .try_into()
        .unwrap();

        match rv {
            ERROR_SUCCESS => {
                assert!(!subkey_handle.is_null());

                Ok(Some(RegKey {
                    handle: subkey_handle,
                }))
            }
            ERROR_FILE_NOT_FOUND => Ok(None),
            _ => Err(DetectConflictError::RegOpenKeyFailed(rv)),
        }
    }
    /// Try to get a value with the given name
    ///
    /// Returns `None` if there is no value with that name
    ///
    /// Note that you can pass the empty string to get the default value
    pub fn try_get_value(
        &self,
        value_name: impl AsRef<OsStr>,
    ) -> Result<Option<RegValue>, DetectConflictError> {
        let win32_value_name = to_win32_string(value_name.as_ref());

        // First we query the value's length and type so we know how to create the data buffer
        let mut value_type = 0;
        let mut value_len = 0;

        let rv = unsafe {
            RegGetValueW(
                self.handle,
                std::ptr::null(), // subkey
                win32_value_name.as_ptr(),
                RRF_RT_REG_BINARY | RRF_RT_REG_DWORD | RRF_RT_REG_SZ,
                &mut value_type,
                std::ptr::null_mut(), // no data ptr, just query length & type
                &mut value_len,
            )
        }
        .try_into()
        .unwrap();

        if rv == ERROR_FILE_NOT_FOUND {
            return Ok(None);
        }

        if rv != ERROR_SUCCESS || value_len == 0 {
            return Err(DetectConflictError::RegGetValueLenFailed(rv));
        }

        if value_type == REG_SZ {
            // buffer length is in wide characters
            let buffer_len = value_len / 2;

            assert_eq!(buffer_len * 2, value_len);

            let mut buffer = vec![0u16; buffer_len.try_into().unwrap()];

            let rv = unsafe {
                RegGetValueW(
                    self.handle,
                    std::ptr::null(), // subkey
                    win32_value_name.as_ptr(),
                    RRF_RT_REG_SZ,
                    std::ptr::null_mut(), // no need to query type again
                    buffer.as_mut_ptr().cast(),
                    &mut value_len,
                )
            }
            .try_into()
            .unwrap();

            if rv != ERROR_SUCCESS {
                return Err(DetectConflictError::RegGetValueFailed(rv));
            }

            Ok(Some(RegValue::String(from_win32_string(&buffer))))
        } else if value_type == REG_BINARY {
            let mut buffer = vec![0u8; value_len.try_into().unwrap()];

            let rv = unsafe {
                RegGetValueW(
                    self.handle,
                    std::ptr::null(), // subkey
                    win32_value_name.as_ptr(),
                    RRF_RT_REG_BINARY,
                    std::ptr::null_mut(), // no need to query type again
                    buffer.as_mut_ptr().cast(),
                    &mut value_len,
                )
            }
            .try_into()
            .unwrap();

            if rv != ERROR_SUCCESS {
                return Err(DetectConflictError::RegGetValueFailed(rv));
            }

            Ok(Some(RegValue::Binary(buffer)))
        } else if value_type == REG_DWORD {
            assert_eq!(value_len, 4);

            let mut buffer = 0u32;

            let rv = unsafe {
                RegGetValueW(
                    self.handle,
                    std::ptr::null(), // subkey
                    win32_value_name.as_ptr(),
                    RRF_RT_REG_DWORD,
                    std::ptr::null_mut(), // no need to query type again
                    (&mut buffer as *mut u32).cast(),
                    &mut value_len,
                )
            }
            .try_into()
            .unwrap();

            if rv != ERROR_SUCCESS {
                return Err(DetectConflictError::RegGetValueFailed(rv));
            }

            Ok(Some(RegValue::Dword(buffer)))
        } else {
            Err(DetectConflictError::UnsupportedValueType(value_type))
        }
    }
    /// Get an iterator that returns the names of all the subkeys of this key
    pub fn subkey_names(&self) -> SubkeyNames<'_> {
        SubkeyNames::new(self)
    }
}

impl Drop for RegKey {
    fn drop(&mut self) {
        assert!(!self.handle.is_null());

        if self.handle == HKEY_LOCAL_MACHINE {
            return;
        }

        let rv: u32 = unsafe { RegCloseKey(self.handle) }.try_into().unwrap();

        if rv != ERROR_SUCCESS {
            log::warn!("failed to close opened registry key");
        }
    }
}

/// An iterator through the subkeys of a key
///
/// Returns items of type `Result<OsString, DetectConflictError>`. If an error occurs, calling `next()` again
/// will likely just continue to result in errors, so it's best to just abandon ship at that
/// point
#[derive(Debug)]
pub struct SubkeyNames<'a> {
    /// The key that is being iterated
    key: &'a RegKey,
    /// The current index of the subkey -- Incremented each successful call to `next()`
    index: u32,
    /// A buffer that is large enough to hold the longest subkey name
    buffer: Option<Vec<u16>>,
}

impl<'a> SubkeyNames<'a> {
    /// Create a new subkey name iterator
    fn new(key: &'a RegKey) -> SubkeyNames<'a> {
        SubkeyNames {
            key,
            index: 0,
            buffer: None,
        }
    }
    /// Initialize `self.buffer` with a vector that's long enough to hold the longest subkey name
    /// (if it isn't already)
    fn create_buffer_if_needed(&mut self) -> Result<(), DetectConflictError> {
        if self.buffer.is_some() {
            return Ok(());
        }

        let mut max_key_length = 0;

        let rv = unsafe {
            RegQueryInfoKeyW(
                self.key.handle,
                std::ptr::null_mut(), // class ptr
                std::ptr::null_mut(), // class len ptr
                std::ptr::null_mut(), // reserved
                std::ptr::null_mut(), // num of subkeys ptr
                &mut max_key_length,
                std::ptr::null_mut(), // max class length ptr
                std::ptr::null_mut(), // number of values ptr
                std::ptr::null_mut(), // longest value name ptr
                std::ptr::null_mut(), // longest value data ptr
                std::ptr::null_mut(), // DACL ptr
                std::ptr::null_mut(), // last write time ptr
            )
        }
        .try_into()
        .unwrap();

        if rv != ERROR_SUCCESS {
            return Err(DetectConflictError::RegQueryInfoKeyFailed(rv));
        }

        // +1 for the null terminator
        let max_key_length = usize::try_from(max_key_length).unwrap() + 1;

        self.buffer = Some(vec![0u16; max_key_length]);

        Ok(())
    }
}

impl<'a> Iterator for SubkeyNames<'a> {
    type Item = Result<OsString, DetectConflictError>;

    fn next(&mut self) -> Option<Self::Item> {
        if let Err(e) = self.create_buffer_if_needed() {
            return Some(Err(e));
        }

        let buffer = self.buffer.as_mut().unwrap();
        let mut name_len = u32::try_from(buffer.len()).unwrap();

        let rv = unsafe {
            RegEnumKeyExW(
                self.key.handle,
                self.index,
                buffer.as_mut_ptr(),
                &mut name_len,
                std::ptr::null_mut(), // reserved
                std::ptr::null_mut(), // class ptr
                std::ptr::null_mut(), // class len ptr
                std::ptr::null_mut(), // last write time ptr
            )
        }
        .try_into()
        .unwrap();

        if rv == ERROR_NO_MORE_ITEMS {
            return None;
        }

        if rv != ERROR_SUCCESS {
            return Some(Err(DetectConflictError::RegEnumKeyFailed(rv)));
        }

        self.index += 1;

        // +1 for the null terminator
        let name_len = usize::try_from(name_len).unwrap() + 1;

        Some(Ok(from_win32_string(&buffer[0..name_len])))
    }
}

/// A single value in the registry
#[derive(Clone, Debug, PartialEq)]
pub enum RegValue {
    /// A REG_BINARY value
    Binary(Vec<u8>),
    /// A REG_DWORD value
    Dword(u32),
    /// A REG_SZ value
    String(OsString),
}

/// Convert an OsStr to a null-terminated wide character string
///
/// The input string must not contain any interior null values, and the resulting wide
/// character array will have a null terminator appended
fn to_win32_string(s: &OsStr) -> Vec<u16> {
    // +1 for the null terminator
    let mut result = Vec::with_capacity(s.len() + 1);

    for wc in s.encode_wide() {
        assert_ne!(wc, 0);
        result.push(wc);
    }

    result.push(0);
    result
}

/// Convert a null-terminated wide character string into an OsString
///
/// The passed-in slice must have exactly one wide null character as the last element
fn from_win32_string(s: &[u16]) -> OsString {
    for (idx, wc) in s.iter().enumerate() {
        if *wc == 0 {
            assert_eq!(idx, s.len() - 1);
            return OsString::from_wide(&s[0..idx]);
        }
    }
    panic!("missing null terminator at end of win32 string");
}
