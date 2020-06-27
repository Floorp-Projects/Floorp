/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use std::convert::TryInto;
use std::ffi::OsStr;
use std::mem;
use std::os::windows::ffi::OsStrExt;
use std::ptr::NonNull;
use std::slice;

use winapi::shared::{winerror, wtypes};
use winapi::um::{oaidl, oleauto};

use comedy::HResult;

/// Conveniently create and destroy a `BSTR`.
///
/// This is called `BString` by analogy to `String` since it owns the data.
///
/// The internal `BSTR` is always non-null, even for an empty string, for simple safety reasons.

#[derive(Debug)]
pub struct BString(NonNull<u16>);

impl BString {
    pub fn from_slice(v: impl AsRef<[u16]>) -> Result<BString, HResult> {
        let v = v.as_ref();
        let real_len = v.len();
        let len = real_len
            .try_into()
            .map_err(|_| HResult::new(winerror::E_OUTOFMEMORY))?;
        let bs = unsafe { oleauto::SysAllocStringLen(v.as_ptr(), len) };

        Ok(Self(NonNull::new(bs).ok_or_else(|| {
            HResult::new(winerror::E_OUTOFMEMORY).function("SysAllocStringLen")
        })?))
    }

    pub fn from_os_str(s: impl AsRef<OsStr>) -> Result<BString, HResult> {
        BString::from_slice(s.as_ref().encode_wide().collect::<Vec<_>>().as_slice())
    }

    /// Take ownership of a `BSTR`.
    ///
    /// This will be freed when the `BString` is dropped, so the pointer shouldn't be used
    /// after calling this function.
    ///
    /// Returns `None` if the pointer is null; though this means an empty string in most
    /// contexts where `BSTR` is used, `BString` is always non-null.
    pub unsafe fn from_raw(p: *mut u16) -> Option<Self> {
        Some(Self(NonNull::new(p)?))
    }

    /// Get a pointer to the `BSTR`.
    ///
    /// The caller must ensure that the `BString` outlives the pointer this function returns,
    /// or else it will end up pointing to garbage.
    ///
    /// This pointer shouldn't be written to, but most APIs require a mutable pointer.
    pub fn as_raw_ptr(&self) -> *mut u16 {
        self.0.as_ptr()
    }

    /// Build a raw `VARIANT`, essentially a typed pointer.
    ///
    /// The caller must ensure that the `BString` outlives the `VARIANT` this function returns,
    /// or else it will end up pointing to garbage.
    ///
    /// This is meant for passing by value to Windows APIs.
    pub fn as_raw_variant(&self) -> oaidl::VARIANT {
        unsafe {
            let mut v: oaidl::VARIANT = mem::zeroed();
            {
                let tv = v.n1.n2_mut();
                *tv.n3.bstrVal_mut() = self.as_raw_ptr();
                tv.vt = wtypes::VT_BSTR as wtypes::VARTYPE;
            }

            v
        }
    }
}

impl Drop for BString {
    fn drop(&mut self) {
        unsafe { oleauto::SysFreeString(self.0.as_ptr()) }
    }
}

impl AsRef<[u16]> for BString {
    fn as_ref(&self) -> &[u16] {
        unsafe {
            let len = oleauto::SysStringLen(self.0.as_ptr());

            slice::from_raw_parts(self.0.as_ptr(), len as usize)
        }
    }
}

/// Try to convert, decorate `Err` with call site info
#[macro_export]
macro_rules! try_to_bstring {
    ($ex:expr) => {
        $crate::ole_utils::BString::from_os_str($ex).map_err(|e| e.file_line(file!(), line!()))
    };
}

pub fn empty_variant() -> oaidl::VARIANT {
    unsafe {
        let mut v: oaidl::VARIANT = mem::zeroed();
        {
            let tv = v.n1.n2_mut();
            tv.vt = wtypes::VT_EMPTY as wtypes::VARTYPE;
        }

        v
    }
}

pub trait OptionBstringExt {
    fn as_raw_variant(&self) -> oaidl::VARIANT;
}

/// Shorthand for unwrapping, returns `BString::as_raw_variant()` or `empty_variant()`
impl OptionBstringExt for Option<&BString> {
    fn as_raw_variant(&self) -> oaidl::VARIANT {
        self.map(|bs| bs.as_raw_variant())
            .unwrap_or_else(empty_variant)
    }
}

// Note: A `VARIANT_BOOL` is not a `VARIANT`, rather it would go into a `VARIANT` of type
// `VT_BOOL`. Some APIs use it directly.
pub trait IntoVariantBool {
    fn into_variant_bool(self) -> wtypes::VARIANT_BOOL;
}

impl IntoVariantBool for bool {
    fn into_variant_bool(self) -> wtypes::VARIANT_BOOL {
        if self {
            wtypes::VARIANT_TRUE
        } else {
            wtypes::VARIANT_FALSE
        }
    }
}
