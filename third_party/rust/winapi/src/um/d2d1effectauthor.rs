// Copyright © 2017 winapi-rs developers
// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.
// TODO:It is a minimal implementation.
use shared::basetsd::UINT32;
use shared::minwindef::BYTE;
use shared::ntdef::{HRESULT, PCWSTR};
use um::unknwnbase::IUnknown;
FN!{stdcall PD2D1_PROPERTY_SET_FUNCTION(
    effect: *const IUnknown,
    data: *const BYTE,
    dataSize: UINT32,
) -> HRESULT}
FN!{stdcall PD2D1_PROPERTY_GET_FUNCTION(
    effect: *const IUnknown,
    data: *mut BYTE,
    dataSize: UINT32,
    actualSize: *mut UINT32,
) -> HRESULT}
STRUCT!{struct D2D1_PROPERTY_BINDING {
    propertyName: PCWSTR,
    setFunction: PD2D1_PROPERTY_SET_FUNCTION,
    getFunction: PD2D1_PROPERTY_GET_FUNCTION,
}}
