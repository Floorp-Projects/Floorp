/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::os::raw::{c_uchar, c_uint};

#[repr(u32)]
pub enum SECItemType {
    siBuffer = 0,
    siClearDataBuffer = 1,
    siCipherDataBuffer = 2,
    siDERCertBuffer = 3,
    siEncodedCertBuffer = 4,
    siDERNameBuffer = 5,
    siEncodedNameBuffer = 6,
    siAsciiNameString = 7,
    siAsciiString = 8,
    siDEROID = 9,
    siUnsignedInteger = 10,
    siUTCTime = 11,
    siGeneralizedTime = 12,
    siVisibleString = 13,
    siUTF8String = 14,
    siBMPString = 15,
}

pub type SECItem = SECItemStr;
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct SECItemStr {
    pub type_: u32, /* SECItemType */
    pub data: *mut c_uchar,
    pub len: c_uint,
}

#[repr(i32)]
#[derive(PartialEq)]
pub enum _SECStatus {
    SECWouldBlock = -2,
    SECFailure = -1,
    SECSuccess = 0,
}
pub use _SECStatus as SECStatus;
