// Copyright © 2016-2017 winapi-rs developers
// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.
//! this ALWAYS GENERATED file contains the definitions for the interfaces
use ctypes::c_void;
use shared::basetsd::UINT64;
use shared::guiddef::{CLSID, REFIID};
use shared::minwindef::{BOOL, DWORD, FILETIME, ULONG};
use shared::wtypesbase::{LPOLESTR, OLECHAR};
use um::objidlbase::{IEnumString, IStream};
use um::unknwnbase::{IUnknown, IUnknownVtbl};
use um::winnt::{HRESULT, ULARGE_INTEGER};
//8402
STRUCT!{struct BIND_OPTS {
    cbStruct: DWORD,
    grfFlags: DWORD,
    grfMode: DWORD,
    dwTickCountDeadline: DWORD,
}}
pub type LPBIND_OPTS = *mut BIND_OPTS;
//8479
RIDL!(
#[uuid(0x0000000e, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46)]
interface IBindCtx(IBindCtxVtbl): IUnknown(IUnknownVtbl) {
    fn RegisterObjectBound(
        punk: *mut IUnknown,
    ) -> HRESULT,
    fn RevokeObjectBound(
        punk: *mut IUnknown,
    ) -> HRESULT,
    fn ReleaseBoundObjects() -> HRESULT,
    fn SetBindOptions(
        pbindopts: *mut BIND_OPTS,
    ) -> HRESULT,
    fn GetBindOptions(
        pbindopts: *mut BIND_OPTS,
    ) -> HRESULT,
    fn GetRunningObjectTable(
        pprot: *mut *mut IRunningObjectTable,
    ) -> HRESULT,
    fn RegisterObjectParam(
        pszKey: LPOLESTR,
        punk: *mut IUnknown,
    ) -> HRESULT,
    fn GetObjectParam(
        pszKey: LPOLESTR,
        ppunk: *mut *mut IUnknown,
    ) -> HRESULT,
    fn EnumObjectParam(
        ppenum: *mut *mut IEnumString,
    ) -> HRESULT,
    fn RevokeObjectParam(
        pszKey: LPOLESTR,
    ) -> HRESULT,
}
);
//8681
RIDL!(
#[uuid(0x00000102, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46)]
interface IEnumMoniker(IEnumMonikerVtbl): IUnknown(IUnknownVtbl) {
    fn Next(
        celt: ULONG,
        rgelt: *mut *mut IMoniker,
        pceltFetched: *mut ULONG,
    ) -> HRESULT,
    fn Skip(
        celt: ULONG,
    ) -> HRESULT,
    fn Reset() -> HRESULT,
    fn Clone(
        ppenum: *mut *mut IEnumMoniker,
    ) -> HRESULT,
}
);
//8958
RIDL!(
#[uuid(0x00000010, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46)]
interface IRunningObjectTable(IRunningObjectTableVtbl): IUnknown(IUnknownVtbl) {
    fn Register(
        grfFlags: DWORD,
        punkObject: *mut IUnknown,
        pmkObjectName: *mut IMoniker,
        pdwRegister: *mut DWORD,
    ) -> HRESULT,
    fn Revoke(
        dwRegister: DWORD,
    ) -> HRESULT,
    fn IsRunning(
        pmkObjectName: *mut IMoniker,
    ) -> HRESULT,
    fn GetObject(
        pmkObjectName: *mut IMoniker,
        ppunkObject: *mut *mut IUnknown,
    ) -> HRESULT,
    fn NoteChangeTime(
        dwRegister: DWORD,
        pfiletime: *mut FILETIME,
    ) -> HRESULT,
    fn GetTimeOfLastChange(
        pmkObjectName: *mut IMoniker,
        pfiletime: *mut FILETIME,
    ) -> HRESULT,
    fn EnumRunning(
        ppenumMoniker: *mut *mut IEnumMoniker,
    ) -> HRESULT,
}
);
//9125
RIDL!(
#[uuid(0x0000010c, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46)]
interface IPersist(IPersistVtbl): IUnknown(IUnknownVtbl) {
    fn GetClassID(
        pClassID: *mut CLSID,
    ) -> HRESULT,
}
);
//9207
RIDL!(
#[uuid(0x00000109, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46)]
interface IPersistStream(IPersistStreamVtbl): IPersist(IPersistVtbl) {
    fn IsDirty() -> HRESULT,
    fn Load(
        pStm: *mut IStream,
    ) -> HRESULT,
    fn Save(
        pStm: *mut IStream,
        fClearDirty: BOOL,
    ) -> HRESULT,
    fn GetSizeMax(
        pcbSize: *mut ULARGE_INTEGER,
    ) -> HRESULT,
}
);
//9350
RIDL!(
#[uuid(0x0000000f, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46)]
interface IMoniker(IMonikerVtbl): IPersistStream(IPersistStreamVtbl) {
    fn BindToObject(
        pbc: *mut IBindCtx,
        pmkToLeft: *mut IMoniker,
        riidResult: REFIID,
        ppvResult: *mut *mut c_void,
    ) -> HRESULT,
    fn BindToStorage(
        pbc: *mut IBindCtx,
        pmkToLeft: *mut IMoniker,
        riid: REFIID,
        ppvObj: *mut *mut c_void,
    ) -> HRESULT,
    fn Reduce(
        pbc: *mut IBindCtx,
        dwReduceHowFar: DWORD,
        ppmkToLeft: *mut *mut IMoniker,
        ppmkReduced: *mut *mut IMoniker,
    ) -> HRESULT,
    fn ComposeWith(
        pmkRight: *mut IMoniker,
        fOnlyIfNotGeneric: BOOL,
        ppmkComposite: *mut *mut IMoniker,
    ) -> HRESULT,
    fn Enum(
        fForward: BOOL,
        ppenumMoniker: *mut *mut IEnumMoniker,
    ) -> HRESULT,
    fn IsEqual(
        pmkOtherMoniker: *mut IMoniker,
    ) -> HRESULT,
    fn Hash(
        pdwHash: *mut DWORD,
    ) -> HRESULT,
    fn IsRunning(
        pbc: *mut IBindCtx,
        pmkToLeft: *mut IMoniker,
        pmkNewlyRunning: *mut IMoniker,
    ) -> HRESULT,
    fn GetTimeOfLastChange(
        pbc: *mut IBindCtx,
        pmkToLeft: *mut IMoniker,
        pFileTime: *mut FILETIME,
    ) -> HRESULT,
    fn Inverse(
        ppmk: *mut *mut IMoniker,
    ) -> HRESULT,
    fn CommonPrefixWith(
        pmkOther: *mut IMoniker,
        ppmkPrefix: *mut *mut IMoniker,
    ) -> HRESULT,
    fn RelativePathTo(
        pmkOther: *mut IMoniker,
        ppmkRelPath: *mut *mut IMoniker,
    ) -> HRESULT,
    fn GetDisplayName(
        pbc: *mut IBindCtx,
        pmkToLeft: *mut IMoniker,
        ppszDisplayName: *mut LPOLESTR,
    ) -> HRESULT,
    fn ParseDisplayName(
        pbc: *mut IBindCtx,
        pmkToLeft: *mut IMoniker,
        pszDisplayName: LPOLESTR,
        pchEaten: *mut ULONG,
        ppmkOut: *mut *mut IMoniker,
    ) -> HRESULT,
    fn IsSystemMoniker(
        pdwMksys: *mut DWORD,
    ) -> HRESULT,
}
);
ENUM!{enum EOLE_AUTHENTICATION_CAPABILITIES {
    EOAC_NONE = 0,
    EOAC_MUTUAL_AUTH = 0x1,
    EOAC_STATIC_CLOAKING = 0x20,
    EOAC_DYNAMIC_CLOAKING = 0x40,
    EOAC_ANY_AUTHORITY = 0x80,
    EOAC_MAKE_FULLSIC = 0x100,
    EOAC_DEFAULT = 0x800,
    EOAC_SECURE_REFS = 0x2,
    EOAC_ACCESS_CONTROL = 0x4,
    EOAC_APPID = 0x8,
    EOAC_DYNAMIC = 0x10,
    EOAC_REQUIRE_FULLSIC = 0x200,
    EOAC_AUTO_IMPERSONATE = 0x400,
    EOAC_NO_CUSTOM_MARSHAL = 0x2000,
    EOAC_DISABLE_AAA = 0x1000,
}}
STRUCT!{struct SOLE_AUTHENTICATION_SERVICE {
    dwAuthnSvc: DWORD,
    dwAuthzSvc: DWORD,
    pPrincipalName: *mut OLECHAR,
    hr: HRESULT,
}}
RIDL!(
#[uuid(0xa2f05a09, 0x27a2, 0x42b5, 0xbc, 0x0e, 0xac, 0x16, 0x3e, 0xf4, 0x9d, 0x9b)]
interface IApartmentShutdown(IApartmentShutdownVtbl): IUnknown(IUnknownVtbl) {
    fn OnUninitialize(
        ui64ApartmentIdentifier: UINT64,
    ) -> (),
}
);
RIDL!(
#[uuid(0x00000003, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46)]
interface IMarshal(IMarshalVtbl): IUnknown(IUnknownVtbl) {
    fn GetUnmarshalClass(
        riid: REFIID,
        pv: *mut c_void,
        dwDestContext: DWORD,
        pvDestContext: *mut c_void,
        mshlflags: DWORD,
        pCid: *mut CLSID,
    ) -> HRESULT,
    fn GetMarshalSizeMax(
        riid: REFIID,
        pv: *mut c_void,
        dwDestContext: DWORD,
        pvDestContext: *mut c_void,
        mshlflags: DWORD,
        pSize: *mut DWORD,
    ) -> HRESULT,
    fn MarshalInterface(
        pStm: *mut IStream,
        riid: REFIID,
        pv: *mut c_void,
        dwDestContext: DWORD,
        pvDestContext: *mut c_void,
        mshlflags: DWORD,
    ) -> HRESULT,
    fn UnmarshalInterface(
        pStm: *mut IStream,
        riid: REFIID,
        ppv: *mut *mut c_void,
    ) -> HRESULT,
    fn ReleaseMarshalData(
        pStm: *mut IStream,
    ) -> HRESULT,
    fn DisconnectObject(
        dwReserved: DWORD,
    ) -> HRESULT,
}
);
