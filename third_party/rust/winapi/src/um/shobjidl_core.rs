// Copyright © 2015-2017 winapi-rs developers
// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.

use ctypes::{c_int, c_void};
use shared::guiddef::{REFGUID, REFIID};
use shared::minwindef::ULONG;
use shared::windef::HWND;
use um::objidl::IBindCtx;
use um::unknwnbase::{IUnknown, IUnknownVtbl};
use um::winnt::{HRESULT, LPWSTR};

//4498
pub type SFGAOF = ULONG;
//9466
ENUM!{enum SIGDN {
    SIGDN_NORMALDISPLAY = 0,
    SIGDN_PARENTRELATIVEPARSING = 0x80018001,
    SIGDN_DESKTOPABSOLUTEPARSING = 0x80028000,
    SIGDN_PARENTRELATIVEEDITING = 0x80031001,
    SIGDN_DESKTOPABSOLUTEEDITING = 0x8004c000,
    SIGDN_FILESYSPATH = 0x80058000,
    SIGDN_URL = 0x80068000,
    SIGDN_PARENTRELATIVEFORADDRESSBAR = 0x8007c001,
    SIGDN_PARENTRELATIVE = 0x80080001,
    SIGDN_PARENTRELATIVEFORUI = 0x80094001,
}}
ENUM!{enum SICHINTF {
    SICHINT_DISPLAY = 0,
    SICHINT_ALLFIELDS = 0x80000000,
    SICHINT_CANONICAL = 0x10000000,
    SICHINT_TEST_FILESYSPATH_IF_NOT_EQUAL = 0x20000000,
}}
RIDL!(
#[uuid(0x43826d1e, 0xe718, 0x42ee, 0xbc, 0x55, 0xa1, 0xe2, 0x61, 0xc3, 0x7b, 0xfe)]
interface IShellItem(IShellItemVtbl): IUnknown(IUnknownVtbl) {
    fn BindToHandler(
        pbc: *mut IBindCtx,
        bhid: REFGUID,
        riid: REFIID,
        ppv: *mut *mut c_void,
    ) -> HRESULT,
    fn GetParent(
        ppsi: *mut *mut IShellItem,
    ) -> HRESULT,
    fn GetDisplayName(
        sigdnName: SIGDN,
        ppszName: *mut LPWSTR,
    ) -> HRESULT,
    fn GetAttributes(
        sfgaoMask: SFGAOF,
        psfgaoAttribs: *mut SFGAOF,
    ) -> HRESULT,
    fn Compare(
        psi: *mut IShellItem,
        hint: SICHINTF,
        piOrder: *mut c_int,
    ) -> HRESULT,
});
//20869
RIDL!(
#[uuid(0xb4db1657, 0x70d7, 0x485e, 0x8e, 0x3e, 0x6f, 0xcb, 0x5a, 0x5c, 0x18, 0x02)]
interface IModalWindow(IModalWindowVtbl): IUnknown(IUnknownVtbl) {
    fn Show(
        hwndOwner: HWND,
    ) -> HRESULT,
});
//22307
//27457
pub type IShellItemFilter = IUnknown; // TODO
