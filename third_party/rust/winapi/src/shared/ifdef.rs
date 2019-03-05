// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.
use shared::basetsd::{UINT16, ULONG64};
use shared::minwindef::ULONG;
STRUCT!{struct NET_LUID_LH {
    Value: ULONG64,
}}
BITFIELD!{NET_LUID_LH Value: ULONG64 [
    Reserved set_Reserved[0..24],
    NetLuidIndex set_NetLuidIndex[24..48],
    IfType set_IfType[48..64],
]}
pub type PNET_LUID_LH = *mut NET_LUID_LH;
pub type NET_LUID = NET_LUID_LH;
pub type PNET_LUID = *mut NET_LUID;
pub type NET_IFINDEX = ULONG;
pub type PNET_IFINDEX = *mut ULONG;
pub type NET_IFTYPE = UINT16;
pub type PNET_IFTYPE = *mut UINT16;
