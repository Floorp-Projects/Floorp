// Copyright © 2016-2017 winapi-rs developers
// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.
use shared::basetsd::DWORD_PTR;
use shared::minwindef::{
    BOOL, BYTE, DWORD, HKEY, LPBYTE, LPCVOID, LPDWORD, PFILETIME, PHKEY, ULONG
};
use um::minwinbase::LPSECURITY_ATTRIBUTES;
use um::winnt::{ACCESS_MASK, HANDLE, LONG, LPCSTR, LPCWSTR, LPSTR, LPWSTR, PVOID};
pub type LSTATUS = LONG;
pub const RRF_RT_REG_NONE: DWORD = 0x00000001;
pub const RRF_RT_REG_SZ: DWORD = 0x00000002;
pub const RRF_RT_REG_EXPAND_SZ: DWORD = 0x00000004;
pub const RRF_RT_REG_BINARY: DWORD = 0x00000008;
pub const RRF_RT_REG_DWORD: DWORD = 0x00000010;
pub const RRF_RT_REG_MULTI_SZ: DWORD = 0x00000020;
pub const RRF_RT_REG_QWORD: DWORD = 0x00000040;
pub const RRF_RT_DWORD: DWORD = RRF_RT_REG_BINARY | RRF_RT_REG_DWORD;
pub const RRF_RT_QWORD: DWORD = RRF_RT_REG_BINARY | RRF_RT_REG_QWORD;
pub const RRF_RT_ANY: DWORD = 0x0000ffff;
pub const RRF_SUBKEY_WOW6464KEY: DWORD = 0x00010000;
pub const RRF_SUBKEY_WOW6432KEY: DWORD = 0x00020000;
pub const RRF_WOW64_MASK: DWORD = 0x00030000;
pub const RRF_NOEXPAND: DWORD = 0x10000000;
pub const RRF_ZEROONFAILURE: DWORD = 0x20000000;
pub const REG_PROCESS_APPKEY: DWORD = 0x00000001;
pub type REGSAM = ACCESS_MASK;
pub const HKEY_CLASSES_ROOT: HKEY = 0x80000000i32 as isize as HKEY;
pub const HKEY_CURRENT_USER: HKEY = 0x80000001i32 as isize as HKEY;
pub const HKEY_LOCAL_MACHINE: HKEY = 0x80000002i32 as isize as HKEY;
pub const HKEY_USERS: HKEY = 0x80000003i32 as isize as HKEY;
pub const HKEY_PERFORMANCE_DATA: HKEY = 0x80000004i32 as isize as HKEY;
pub const HKEY_PERFORMANCE_TEXT: HKEY = 0x80000050i32 as isize as HKEY;
pub const HKEY_PERFORMANCE_NLSTEXT: HKEY = 0x80000060i32 as isize as HKEY;
pub const HKEY_CURRENT_CONFIG: HKEY = 0x80000005i32 as isize as HKEY;
pub const HKEY_DYN_DATA: HKEY = 0x80000006i32 as isize as HKEY;
pub const HKEY_CURRENT_USER_LOCAL_SETTINGS: HKEY = 0x80000007i32 as isize as HKEY;
// PROVIDER_KEEPS_VALUE_LENGTH
// val_context
// PVALUEA
// PVALUEW
// QUERYHANDLER
// REG_PROVIDER
STRUCT!{struct VALENTA {
    ve_valuename: LPSTR,
    ve_valuelen: DWORD,
    ve_valueptr: DWORD_PTR,
    ve_type: DWORD,
}}
pub type PVALENTA = *mut VALENTA;
STRUCT!{struct VALENTW {
    ve_valuename: LPWSTR,
    ve_valuelen: DWORD,
    ve_valueptr: DWORD_PTR,
    ve_type: DWORD,
}}
pub type PVALENTW = *mut VALENTW;
// WIN31_CLASS
pub const REG_MUI_STRING_TRUNCATE: DWORD = 0x00000001;
pub const REG_SECURE_CONNECTION: DWORD = 1;
extern "system" {
    pub fn RegCloseKey(
        hKey: HKEY
    ) -> LSTATUS;
    pub fn RegOverridePredefKey(
        hKey: HKEY,
        hNewHKey: HKEY
    ) -> LSTATUS;
    pub fn RegOpenUserClassesRoot(
        hToken: HANDLE,
        dwOptions: DWORD,
        samDesired: REGSAM,
        phkResult: PHKEY,
    ) -> LSTATUS;
    pub fn RegOpenCurrentUser(
        samDesired: REGSAM,
        phkResult: PHKEY
    ) -> LSTATUS;
    pub fn RegDisablePredefinedCache() -> LSTATUS;
    pub fn RegDisablePredefinedCacheEx() -> LSTATUS;
    pub fn RegConnectRegistryA(
        lpMachineName: LPCSTR,
        hKey: HKEY,
        phkResult: PHKEY
    ) -> LSTATUS;
    pub fn RegConnectRegistryW(
        lpMachineName: LPCWSTR,
        hKey: HKEY,
        phkResult: PHKEY
    ) -> LSTATUS;
    pub fn RegConnectRegistryExA(
        lpMachineName: LPCSTR,
        hKey: HKEY,
        flags: ULONG,
        phkResult: PHKEY
    ) -> LSTATUS;
    pub fn RegConnectRegistryExW(
        lpMachineName: LPCWSTR,
        hKey: HKEY,
        flags: ULONG,
        phkResult: PHKEY
    ) -> LSTATUS;
    pub fn RegCreateKeyA(
        hKey: HKEY,
        lpSubKey: LPCSTR,
        phkResult: PHKEY
    ) -> LSTATUS;
    pub fn RegCreateKeyW(
        hKey: HKEY,
        lpSubKey: LPCWSTR,
        phkResult: PHKEY
    ) -> LSTATUS;
    pub fn RegCreateKeyExA(
        hKey: HKEY,
        lpSubKey: LPCSTR,
        Reserved: DWORD,
        lpClass: LPSTR,
        dwOptions: DWORD,
        samDesired: REGSAM,
        lpSecurityAttributes: LPSECURITY_ATTRIBUTES,
        phkResult: PHKEY,
        lpdwDisposition: LPDWORD,
    ) -> LSTATUS;
    pub fn RegCreateKeyExW(
        hKey: HKEY,
        lpSubKey: LPCWSTR,
        Reserved: DWORD,
        lpClass: LPWSTR,
        dwOptions: DWORD,
        samDesired: REGSAM,
        lpSecurityAttributes: LPSECURITY_ATTRIBUTES,
        phkResult: PHKEY,
        lpdwDisposition: LPDWORD,
    ) -> LSTATUS;
    pub fn RegCreateKeyTransactedA(
        hKey: HKEY,
        lpSubKey: LPCSTR,
        Reserved: DWORD,
        lpClass: LPSTR,
        dwOptions: DWORD,
        samDesired: REGSAM,
        lpSecurityAttributes: LPSECURITY_ATTRIBUTES,
        phkResult: PHKEY,
        lpdwDisposition: LPDWORD,
        hTransaction: HANDLE,
        pExtendedParemeter: PVOID,
    ) -> LSTATUS;
    pub fn RegCreateKeyTransactedW(
        hKey: HKEY,
        lpSubKey: LPCWSTR,
        Reserved: DWORD,
        lpClass: LPWSTR,
        dwOptions: DWORD,
        samDesired: REGSAM,
        lpSecurityAttributes: LPSECURITY_ATTRIBUTES,
        phkResult: PHKEY,
        lpdwDisposition: LPDWORD,
        hTransaction: HANDLE,
        pExtendedParemeter: PVOID,
    ) -> LSTATUS;
    pub fn RegDeleteKeyA(
        hKey: HKEY,
        lpSubKey: LPCSTR
    ) -> LSTATUS;
    pub fn RegDeleteKeyW(
        hKey: HKEY,
        lpSubKey: LPCWSTR
    ) -> LSTATUS;
    pub fn RegDeleteKeyExA(
        hKey: HKEY,
        lpSubKey: LPCSTR,
        samDesired: REGSAM,
        Reserved: DWORD,
    ) -> LSTATUS;
    pub fn RegDeleteKeyExW(
        hKey: HKEY,
        lpSubKey: LPCWSTR,
        samDesired: REGSAM,
        Reserved: DWORD,
    ) -> LSTATUS;
    pub fn RegDeleteKeyTransactedA(
        hKey: HKEY,
        lpSubKey: LPCSTR,
        samDesired: REGSAM,
        Reserved: DWORD,
        hTransaction: HANDLE,
        pExtendedParemeter: PVOID,
    ) -> LSTATUS;
    pub fn RegDeleteKeyTransactedW(
        hKey: HKEY,
        lpSubKey: LPCWSTR,
        samDesired: REGSAM,
        Reserved: DWORD,
        hTransaction: HANDLE,
        pExtendedParemeter: PVOID,
    ) -> LSTATUS;
    pub fn RegDisableReflectionKey(
        hBase: HKEY
    ) -> LONG;
    pub fn RegEnableReflectionKey(
        hBase: HKEY
    ) -> LONG;
    pub fn RegQueryReflectionKey(
        hBase: HKEY,
        bIsReflectionDisabled: *mut BOOL
    ) -> LONG;
    pub fn RegDeleteValueA(
        hKey: HKEY,
        lpValueName: LPCSTR
    ) -> LSTATUS;
    pub fn RegDeleteValueW(
        hKey: HKEY,
        lpValueName: LPCWSTR
    ) -> LSTATUS;
    // pub fn RegEnumKeyA();
    // pub fn RegEnumKeyW();
    pub fn RegEnumKeyExA(
        hKey: HKEY,
        dwIndex: DWORD,
        lpName: LPSTR,
        lpcName: LPDWORD,
        lpReserved: LPDWORD,
        lpClass: LPSTR,
        lpcClass: LPDWORD,
        lpftLastWriteTime: PFILETIME,
    ) -> LSTATUS;
    pub fn RegEnumKeyExW(
        hKey: HKEY,
        dwIndex: DWORD,
        lpName: LPWSTR,
        lpcName: LPDWORD,
        lpReserved: LPDWORD,
        lpClass: LPWSTR,
        lpcClass: LPDWORD,
        lpftLastWriteTime: PFILETIME,
    ) -> LSTATUS;
    pub fn RegEnumValueA(
        hKey: HKEY,
        dwIndex: DWORD,
        lpValueName: LPSTR,
        lpcchValueName: LPDWORD,
        lpReserved: LPDWORD,
        lpType: LPDWORD,
        lpData: LPBYTE,
        lpcbData: LPDWORD,
    ) -> LSTATUS;
    pub fn RegEnumValueW(
        hKey: HKEY,
        dwIndex: DWORD,
        lpValueName: LPWSTR,
        lpcchValueName: LPDWORD,
        lpReserved: LPDWORD,
        lpType: LPDWORD,
        lpData: LPBYTE,
        lpcbData: LPDWORD,
    ) -> LSTATUS;
    pub fn RegFlushKey(
        hKey: HKEY
    ) -> LSTATUS;
    // pub fn RegGetKeySecurity();
    // pub fn RegLoadKeyA();
    // pub fn RegLoadKeyW();
    pub fn RegNotifyChangeKeyValue(
        hKey: HKEY,
        bWatchSubtree: BOOL,
        dwNotifyFilter: DWORD,
        hEvent: HANDLE,
        fAsynchronous: BOOL,
    ) -> LSTATUS;
    // pub fn RegOpenKeyA();
    // pub fn RegOpenKeyW();
    pub fn RegOpenKeyExA(
        hKey: HKEY,
        lpSubKey: LPCSTR,
        ulOptions: DWORD,
        samDesired: REGSAM,
        phkResult: PHKEY,
    ) -> LSTATUS;
    pub fn RegOpenKeyExW(
        hKey: HKEY,
        lpSubKey: LPCWSTR,
        ulOptions: DWORD,
        samDesired: REGSAM,
        phkResult: PHKEY,
    ) -> LSTATUS;
    pub fn RegOpenKeyTransactedA(
        hKey: HKEY,
        lpSubKey: LPCSTR,
        ulOptions: DWORD,
        samDesired: REGSAM,
        phkResult: PHKEY,
        hTransaction: HANDLE,
        pExtendedParemeter: PVOID,
    ) -> LSTATUS;
    pub fn RegOpenKeyTransactedW(
        hKey: HKEY,
        lpSubKey: LPCWSTR,
        ulOptions: DWORD,
        samDesired: REGSAM,
        phkResult: PHKEY,
        hTransaction: HANDLE,
        pExtendedParemeter: PVOID,
    ) -> LSTATUS;
    pub fn RegQueryInfoKeyA(
        hKey: HKEY,
        lpClass: LPSTR,
        lpcClass: LPDWORD,
        lpReserved: LPDWORD,
        lpcSubKeys: LPDWORD,
        lpcMaxSubKeyLen: LPDWORD,
        lpcMaxClassLen: LPDWORD,
        lpcValues: LPDWORD,
        lpcMaxValueNameLen: LPDWORD,
        lpcMaxValueLen: LPDWORD,
        lpcbSecurityDescriptor: LPDWORD,
        lpftLastWriteTime: PFILETIME,
    ) -> LSTATUS;
    pub fn RegQueryInfoKeyW(
        hKey: HKEY,
        lpClass: LPWSTR,
        lpcClass: LPDWORD,
        lpReserved: LPDWORD,
        lpcSubKeys: LPDWORD,
        lpcMaxSubKeyLen: LPDWORD,
        lpcMaxClassLen: LPDWORD,
        lpcValues: LPDWORD,
        lpcMaxValueNameLen: LPDWORD,
        lpcMaxValueLen: LPDWORD,
        lpcbSecurityDescriptor: LPDWORD,
        lpftLastWriteTime: PFILETIME,
    ) -> LSTATUS;
    // pub fn RegQueryValueA();
    // pub fn RegQueryValueW();
    pub fn RegQueryMultipleValuesA(
        hKey: HKEY,
        val_list: PVALENTA,
        num_vals: DWORD,
        lpValueBuf: LPSTR,
        ldwTotsize: LPDWORD,
    ) -> LSTATUS;
    pub fn RegQueryMultipleValuesW(
        hKey: HKEY,
        val_list: PVALENTW,
        num_vals: DWORD,
        lpValueBuf: LPWSTR,
        ldwTotsize: LPDWORD,
    ) -> LSTATUS;
    pub fn RegQueryValueExA(
        hKey: HKEY,
        lpValueName: LPCSTR,
        lpReserved: LPDWORD,
        lpType: LPDWORD,
        lpData: LPBYTE,
        lpcbData: LPDWORD,
    ) -> LSTATUS;
    pub fn RegQueryValueExW(
        hKey: HKEY,
        lpValueName: LPCWSTR,
        lpReserved: LPDWORD,
        lpType: LPDWORD,
        lpData: LPBYTE,
        lpcbData: LPDWORD,
    ) -> LSTATUS;
    // pub fn RegReplaceKeyA();
    // pub fn RegReplaceKeyW();
    // pub fn RegRestoreKeyA();
    // pub fn RegRestoreKeyW();
    // pub fn RegRenameKey();
    // pub fn RegSaveKeyA();
    // pub fn RegSaveKeyW();
    // pub fn RegSetKeySecurity();
    // pub fn RegSetValueA();
    // pub fn RegSetValueW();
    pub fn RegSetValueExA(
        hKey: HKEY,
        lpValueName: LPCSTR,
        Reserved: DWORD,
        dwType: DWORD,
        lpData: *const BYTE,
        cbData: DWORD,
    ) -> LSTATUS;
    pub fn RegSetValueExW(
        hKey: HKEY,
        lpValueName: LPCWSTR,
        Reserved: DWORD,
        dwType: DWORD,
        lpData: *const BYTE,
        cbData: DWORD,
    ) -> LSTATUS;
    // pub fn RegUnLoadKeyA();
    // pub fn RegUnLoadKeyW();
    pub fn RegDeleteKeyValueA(
        hKey: HKEY,
        lpSubKey: LPCSTR,
        lpValueName: LPCSTR
    ) -> LSTATUS;
    pub fn RegDeleteKeyValueW(
        hKey: HKEY,
        lpSubKey: LPCWSTR,
        lpValueName: LPCWSTR
    ) -> LSTATUS;
    pub fn RegSetKeyValueA(
        hKey: HKEY,
        lpSubKey: LPCSTR,
        lpValueName: LPCSTR,
        dwType: DWORD,
        lpData: LPCVOID,
        cbData: DWORD,
    ) -> LSTATUS;
    pub fn RegSetKeyValueW(
        hKey: HKEY,
        lpSubKey: LPCWSTR,
        lpValueName: LPCWSTR,
        dwType: DWORD,
        lpData: LPCVOID,
        cbData: DWORD,
    ) -> LSTATUS;
    pub fn RegDeleteTreeA(
        hKey: HKEY,
        lpSubKey: LPCSTR
    ) -> LSTATUS;
    pub fn RegDeleteTreeW(
        hKey: HKEY,
        lpSubKey: LPCWSTR
    ) -> LSTATUS;
    pub fn RegCopyTreeA(
        hKeySrc: HKEY,
        lpSubKey: LPCSTR,
        hKeyDest: HKEY
    ) -> LSTATUS;
    pub fn RegGetValueA(
        hkey: HKEY,
        lpSubKey: LPCSTR,
        lpValue: LPCSTR,
        dwFlags: DWORD,
        pdwType: LPDWORD,
        pvData: PVOID,
        pcbData: LPDWORD,
    ) -> LSTATUS;
    pub fn RegGetValueW(
        hkey: HKEY,
        lpSubKey: LPCWSTR,
        lpValue: LPCWSTR,
        dwFlags: DWORD,
        pdwType: LPDWORD,
        pvData: PVOID,
        pcbData: LPDWORD,
    ) -> LSTATUS;
    pub fn RegCopyTreeW(
        hKeySrc: HKEY,
        lpSubKey: LPCWSTR,
        hKeyDest: HKEY
    ) -> LSTATUS;
    // pub fn RegLoadMUIStringA();
    pub fn RegLoadMUIStringW(
        hKey: HKEY,
        pszValue: LPCWSTR,
        pszOutBuf: LPWSTR,
        cbOutBuf: DWORD,
        pcbData: LPDWORD,
        Flags: DWORD,
        pszDirectory: LPCWSTR,
    ) -> LSTATUS;
    // pub fn RegLoadAppKeyA();
    // pub fn RegLoadAppKeyW();
    // pub fn InitiateSystemShutdownA();
    // pub fn InitiateSystemShutdownW();
    pub fn AbortSystemShutdownA(
        lpMachineName: LPSTR
    ) -> BOOL;
    pub fn AbortSystemShutdownW(
        lpMachineName: LPWSTR
    ) -> BOOL;
}
// REASON_*
// MAX_SHUTDOWN_TIMEOUT
extern "system" {
    // pub fn InitiateSystemShutdownExA();
    // pub fn InitiateSystemShutdownExW();
}
// SHUTDOWN_*
extern "system" {
    // pub fn InitiateShutdownA();
    // pub fn InitiateShutdownW();
    // pub fn CheckForHiberboot();
    // pub fn RegSaveKeyExA();
    // pub fn RegSaveKeyExW();
}
