extern crate nsstring;

use nsstring::{nsCString, nsACString};

/// The type of errors in gecko.  Uses a newtype to provide additional type
/// safety in Rust and #[repr(transparent)] to ensure the same representation
/// as the C++ equivalent.
#[repr(transparent)]
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, Debug)]
pub struct nsresult(pub u32);

/// An extension trait that adds methods to `nsresult` types.
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
        (self.0 >> 31) != 0
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
