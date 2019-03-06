// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.
use shared::basetsd::SIZE_T;
use shared::guiddef::GUID;
use shared::ifdef::{NET_IFINDEX, NET_LUID, PNET_IFINDEX, PNET_LUID};
use shared::minwindef::DWORD;
use shared::ntdef::{CHAR, PSTR, PWSTR, WCHAR};
pub type NETIO_STATUS = DWORD;
pub type NETIOAPI_API = NETIO_STATUS;
extern "system" {
    pub fn ConvertInterfaceNameToLuidA(
        InterfaceName: *const CHAR,
        InterfaceLuid: *mut NET_LUID,
    ) -> NETIOAPI_API;
    pub fn ConvertInterfaceNameToLuidW(
        InterfaceName: *const WCHAR,
        InterfaceLuid: *mut NET_LUID,
    ) -> NETIOAPI_API;
    pub fn ConvertInterfaceLuidToNameA(
        InterfaceLuid: *const NET_LUID,
        InterfaceName: PSTR,
        Length: SIZE_T,
    ) -> NETIOAPI_API;
    pub fn ConvertInterfaceLuidToNameW(
        InterfaceLuid: *const NET_LUID,
        InterfaceName: PWSTR,
        Length: SIZE_T,
    ) -> NETIOAPI_API;
    pub fn ConvertInterfaceLuidToIndex(
        InterfaceLuid: *const NET_LUID,
        InterfaceIndex: PNET_IFINDEX,
    ) -> NETIOAPI_API;
    pub fn ConvertInterfaceIndexToLuid(
        InterfaceIndex: NET_IFINDEX,
        InterfaceLuid: PNET_LUID,
    ) -> NETIOAPI_API;
    pub fn ConvertInterfaceLuidToAlias(
        InterfaceLuid: *const NET_LUID,
        InterfaceAlias: PWSTR,
        Length: SIZE_T,
    ) -> NETIOAPI_API;
    pub fn ConvertInterfaceAliasToLuid(
        InterfaceAlias: *const WCHAR,
        InterfaceLuid: PNET_LUID,
    ) -> NETIOAPI_API;
    pub fn ConvertInterfaceLuidToGuid(
        InterfaceLuid: *const NET_LUID,
        InterfaceGuid: *mut GUID,
    ) -> NETIOAPI_API;
    pub fn ConvertInterfaceGuidToLuid(
        InterfaceGuid: *const GUID,
        InterfaceLuid: PNET_LUID,
    ) -> NETIOAPI_API;
}
