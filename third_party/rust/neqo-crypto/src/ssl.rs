// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(
    dead_code,
    non_upper_case_globals,
    non_snake_case,
    clippy::cognitive_complexity,
    clippy::too_many_lines,
    clippy::upper_case_acronyms
)]
#![allow(unknown_lints, renamed_and_removed_lints, clippy::unknown_clippy_lints)] // Until we require rust 1.51.

use crate::constants::Epoch;
use crate::err::{secstatus_to_res, Res};

use std::os::raw::{c_uint, c_void};

include!(concat!(env!("OUT_DIR"), "/nss_ssl.rs"));
mod SSLOption {
    include!(concat!(env!("OUT_DIR"), "/nss_sslopt.rs"));
}

// I clearly don't understand how bindgen operates.
#[allow(clippy::empty_enum)]
pub enum PLArenaPool {}
#[allow(clippy::empty_enum)]
pub enum PRFileDesc {}

// Remap some constants.
pub const SECSuccess: SECStatus = _SECStatus_SECSuccess;
pub const SECFailure: SECStatus = _SECStatus_SECFailure;

#[derive(Debug, Copy, Clone)]
pub enum Opt {
    Locking,
    Tickets,
    OcspStapling,
    Alpn,
    ExtendedMasterSecret,
    SignedCertificateTimestamps,
    EarlyData,
    RecordSizeLimit,
    Tls13CompatMode,
    HelloDowngradeCheck,
    SuppressEndOfEarlyData,
}

impl Opt {
    // Cast is safe here because SSLOptions are within the i32 range
    #[allow(clippy::cast_possible_wrap)]
    pub(crate) fn as_int(self) -> PRInt32 {
        let i = match self {
            Self::Locking => SSLOption::SSL_NO_LOCKS,
            Self::Tickets => SSLOption::SSL_ENABLE_SESSION_TICKETS,
            Self::OcspStapling => SSLOption::SSL_ENABLE_OCSP_STAPLING,
            Self::Alpn => SSLOption::SSL_ENABLE_ALPN,
            Self::ExtendedMasterSecret => SSLOption::SSL_ENABLE_EXTENDED_MASTER_SECRET,
            Self::SignedCertificateTimestamps => SSLOption::SSL_ENABLE_SIGNED_CERT_TIMESTAMPS,
            Self::EarlyData => SSLOption::SSL_ENABLE_0RTT_DATA,
            Self::RecordSizeLimit => SSLOption::SSL_RECORD_SIZE_LIMIT,
            Self::Tls13CompatMode => SSLOption::SSL_ENABLE_TLS13_COMPAT_MODE,
            Self::HelloDowngradeCheck => SSLOption::SSL_ENABLE_HELLO_DOWNGRADE_CHECK,
            Self::SuppressEndOfEarlyData => SSLOption::SSL_SUPPRESS_END_OF_EARLY_DATA,
        };
        i as PRInt32
    }

    // Some options are backwards, like SSL_NO_LOCKS, so use this to manage that.
    fn map_enabled(self, enabled: bool) -> PRIntn {
        let v = match self {
            Self::Locking => !enabled,
            _ => enabled,
        };
        PRIntn::from(v)
    }

    pub(crate) fn set(self, fd: *mut PRFileDesc, value: bool) -> Res<()> {
        secstatus_to_res(unsafe { SSL_OptionSet(fd, self.as_int(), self.map_enabled(value)) })
    }
}

experimental_api!(SSL_GetCurrentEpoch(
    fd: *mut PRFileDesc,
    read_epoch: *mut u16,
    write_epoch: *mut u16,
));
experimental_api!(SSL_HelloRetryRequestCallback(
    fd: *mut PRFileDesc,
    cb: SSLHelloRetryRequestCallback,
    arg: *mut c_void,
));
experimental_api!(SSL_RecordLayerWriteCallback(
    fd: *mut PRFileDesc,
    cb: SSLRecordWriteCallback,
    arg: *mut c_void,
));
experimental_api!(SSL_RecordLayerData(
    fd: *mut PRFileDesc,
    epoch: Epoch,
    ct: SSLContentType::Type,
    data: *const u8,
    len: c_uint,
));
experimental_api!(SSL_SendSessionTicket(
    fd: *mut PRFileDesc,
    extra: *const u8,
    len: c_uint,
));
experimental_api!(SSL_SetMaxEarlyDataSize(fd: *mut PRFileDesc, size: u32));
experimental_api!(SSL_SetResumptionToken(
    fd: *mut PRFileDesc,
    token: *const u8,
    len: c_uint,
));
experimental_api!(SSL_SetResumptionTokenCallback(
    fd: *mut PRFileDesc,
    cb: SSLResumptionTokenCallback,
    arg: *mut c_void,
));

experimental_api!(SSL_GetResumptionTokenInfo(
    token: *const u8,
    token_len: c_uint,
    info: *mut SSLResumptionTokenInfo,
    len: c_uint,
));

experimental_api!(SSL_DestroyResumptionTokenInfo(
    info: *mut SSLResumptionTokenInfo,
));

#[cfg(test)]
mod tests {
    use super::{SSL_GetNumImplementedCiphers, SSL_NumImplementedCiphers};

    #[test]
    fn num_ciphers() {
        assert!(unsafe { SSL_NumImplementedCiphers } > 0);
        assert!(unsafe { SSL_GetNumImplementedCiphers() } > 0);
        assert_eq!(unsafe { SSL_NumImplementedCiphers }, unsafe {
            SSL_GetNumImplementedCiphers()
        });
    }
}
