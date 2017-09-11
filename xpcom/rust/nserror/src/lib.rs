extern crate nsstring;

use nsstring::{nsCString, nsACString};

/// The type of errors in gecko. This type is currently a type alias, rather
/// than a newtype, in order to conform to the C ABI. In future versions of rust
/// which support RFC #1758 or similar we may be able to use
/// `#[repr(transparent)]` to get a better API for using nsresult.
///
/// The most unfortunate thing about this current implementation is that `u32`
/// and `nsresult` unify.
#[allow(non_camel_case_types)]
pub type nsresult = u32;

/// An extension trait which is intended to add methods to `nsresult` types.
/// Unfortunately, due to ABI issues, this trait is implemented on all u32
/// types. These methods are meaningless on non-nsresult values.
pub trait NsresultExt {
    fn failed(self) -> bool;
    fn succeeded(self) -> bool;
    fn to_result(self) -> Result<nsresult, nsresult>;

    /// Get a printable name for the nsresult error code. This function returns
    /// a nsCString<'static>, which implements `Display`.
    fn error_name(self) -> nsCString;
}

impl NsresultExt for nsresult {
    fn failed(self) -> bool {
        (self >> 31) != 0
    }

    fn succeeded(self) -> bool {
        !self.failed()
    }

    fn to_result(self) -> Result<nsresult, nsresult> {
        if self.failed() {
            Err(self)
        } else {
            Ok(self)
        }
    }

    fn error_name(self) -> nsCString {
        let mut cstr = nsCString::new();
        unsafe {
            Gecko_GetErrorName(self, &mut *cstr);
        }
        cstr
    }
}

extern "C" {
    fn Gecko_GetErrorName(rv: nsresult, cstr: *mut nsACString);
}

mod error_list {
    include!(concat!(env!("MOZ_TOPOBJDIR"), "/xpcom/base/error_list.rs"));
}

pub use error_list::*;
