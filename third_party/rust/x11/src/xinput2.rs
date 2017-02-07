use xfixes::PointerBarrier;
use xlib::{Atom, Display, Time, Window};
use std::os::raw::{c_int, c_uint, c_long, c_double, c_ulong, c_uchar};

//
// macro translations
//
fn mask_byte(mask_flag: i32) -> usize {
    (mask_flag >> 3) as usize
}

pub fn XISetMask(mask: &mut [::std::os::raw::c_uchar], event: i32) {
    mask[mask_byte(event)] |= 1 << (event & 7);
}

pub fn XIClearMask(mask: &mut [::std::os::raw::c_uchar], event: i32) {
    mask[mask_byte(event)] &= 1 << (event & 7);
}

pub fn XIMaskIsSet(mask: &[::std::os::raw::c_uchar], event: i32) -> bool {
    (mask[mask_byte(event)] & (1 << (event & 7))) != 0
}

//
// functions
//
x11_link! { XInput2, xi, ["libXi.so.6", "libXi.so"], 34,
  pub fn XIAllowEvents (_4: *mut Display, _3: c_int, _2: c_int, _1: c_ulong) -> c_int,
  pub fn XIAllowTouchEvents (_5: *mut Display, _4: c_int, _3: c_uint, _2: c_ulong, _1: c_int) -> c_int,
  pub fn XIBarrierReleasePointer (_4: *mut Display, _3: c_int, _2: c_ulong, _1: c_uint) -> (),
  pub fn XIBarrierReleasePointers (_3: *mut Display, _2: *mut XIBarrierReleasePointerInfo, _1: c_int) -> (),
  pub fn XIChangeHierarchy (_3: *mut Display, _2: *mut XIAnyHierarchyChangeInfo, _1: c_int) -> c_int,
  pub fn XIChangeProperty (_8: *mut Display, _7: c_int, _6: c_ulong, _5: c_ulong, _4: c_int, _3: c_int, _2: *mut c_uchar, _1: c_int) -> (),
  pub fn XIDefineCursor (_4: *mut Display, _3: c_int, _2: c_ulong, _1: c_ulong) -> c_int,
  pub fn XIDeleteProperty (_3: *mut Display, _2: c_int, _1: c_ulong) -> (),
  pub fn XIFreeDeviceInfo (_1: *mut XIDeviceInfo) -> (),
  pub fn XIGetClientPointer (_3: *mut Display, _2: c_ulong, _1: *mut c_int) -> c_int,
  pub fn XIGetFocus (_3: *mut Display, _2: c_int, _1: *mut c_ulong) -> c_int,
  pub fn XIGetProperty (_12: *mut Display, _11: c_int, _10: c_ulong, _9: c_long, _8: c_long, _7: c_int, _6: c_ulong, _5: *mut c_ulong, _4: *mut c_int, _3: *mut c_ulong, _2: *mut c_ulong, _1: *mut *mut c_uchar) -> c_int,
  pub fn XIGetSelectedEvents (_3: *mut Display, _2: c_ulong, _1: *mut c_int) -> *mut XIEventMask,
  pub fn XIGrabButton (_11: *mut Display, _10: c_int, _9: c_int, _8: c_ulong, _7: c_ulong, _6: c_int, _5: c_int, _4: c_int, _3: *mut XIEventMask, _2: c_int, _1: *mut XIGrabModifiers) -> c_int,
  pub fn XIGrabDevice (_9: *mut Display, _8: c_int, _7: c_ulong, _6: c_ulong, _5: c_ulong, _4: c_int, _3: c_int, _2: c_int, _1: *mut XIEventMask) -> c_int,
  pub fn XIGrabEnter (_10: *mut Display, _9: c_int, _8: c_ulong, _7: c_ulong, _6: c_int, _5: c_int, _4: c_int, _3: *mut XIEventMask, _2: c_int, _1: *mut XIGrabModifiers) -> c_int,
  pub fn XIGrabFocusIn (_9: *mut Display, _8: c_int, _7: c_ulong, _6: c_int, _5: c_int, _4: c_int, _3: *mut XIEventMask, _2: c_int, _1: *mut XIGrabModifiers) -> c_int,
  pub fn XIGrabKeycode (_10: *mut Display, _9: c_int, _8: c_int, _7: c_ulong, _6: c_int, _5: c_int, _4: c_int, _3: *mut XIEventMask, _2: c_int, _1: *mut XIGrabModifiers) -> c_int,
  pub fn XIGrabTouchBegin (_7: *mut Display, _6: c_int, _5: c_ulong, _4: c_int, _3: *mut XIEventMask, _2: c_int, _1: *mut XIGrabModifiers) -> c_int,
  pub fn XIListProperties (_3: *mut Display, _2: c_int, _1: *mut c_int) -> *mut c_ulong,
  pub fn XIQueryDevice (_3: *mut Display, _2: c_int, _1: *mut c_int) -> *mut XIDeviceInfo,
  pub fn XIQueryPointer (_12: *mut Display, _11: c_int, _10: c_ulong, _9: *mut c_ulong, _8: *mut c_ulong, _7: *mut c_double, _6: *mut c_double, _5: *mut c_double, _4: *mut c_double, _3: *mut XIButtonState, _2: *mut XIModifierState, _1: *mut XIModifierState) -> c_int,
  pub fn XIQueryVersion (_3: *mut Display, _2: *mut c_int, _1: *mut c_int) -> c_int,
  pub fn XISelectEvents (_4: *mut Display, _3: c_ulong, _2: *mut XIEventMask, _1: c_int) -> c_int,
  pub fn XISetClientPointer (_3: *mut Display, _2: c_ulong, _1: c_int) -> c_int,
  pub fn XISetFocus (_4: *mut Display, _3: c_int, _2: c_ulong, _1: c_ulong) -> c_int,
  pub fn XIUndefineCursor (_3: *mut Display, _2: c_int, _1: c_ulong) -> c_int,
  pub fn XIUngrabButton (_6: *mut Display, _5: c_int, _4: c_int, _3: c_ulong, _2: c_int, _1: *mut XIGrabModifiers) -> c_int,
  pub fn XIUngrabDevice (_3: *mut Display, _2: c_int, _1: c_ulong) -> c_int,
  pub fn XIUngrabEnter (_5: *mut Display, _4: c_int, _3: c_ulong, _2: c_int, _1: *mut XIGrabModifiers) -> c_int,
  pub fn XIUngrabFocusIn (_5: *mut Display, _4: c_int, _3: c_ulong, _2: c_int, _1: *mut XIGrabModifiers) -> c_int,
  pub fn XIUngrabKeycode (_6: *mut Display, _5: c_int, _4: c_int, _3: c_ulong, _2: c_int, _1: *mut XIGrabModifiers) -> c_int,
  pub fn XIUngrabTouchBegin (_5: *mut Display, _4: c_int, _3: c_ulong, _2: c_int, _1: *mut XIGrabModifiers) -> c_int,
  pub fn XIWarpPointer (_10: *mut Display, _9: c_int, _8: c_ulong, _7: c_ulong, _6: c_double, _5: c_double, _4: c_uint, _3: c_uint, _2: c_double, _1: c_double) -> c_int,
variadic:
globals:
}

//
// constants
// (auto-generated with cmacros)
//

pub const XInput_2_0: i32 = 7;
pub const XI_2_Major: i32 = 2;
pub const XI_2_Minor: i32 = 3;
pub const XIPropertyDeleted: i32 = 0;
pub const XIPropertyCreated: i32 = 1;
pub const XIPropertyModified: i32 = 2;
pub const XIPropModeReplace: i32 = 0;
pub const XIPropModePrepend: i32 = 1;
pub const XIPropModeAppend: i32 = 2;
pub const XINotifyNormal: i32 = 0;
pub const XINotifyGrab: i32 = 1;
pub const XINotifyUngrab: i32 = 2;
pub const XINotifyWhileGrabbed: i32 = 3;
pub const XINotifyPassiveGrab: i32 = 4;
pub const XINotifyPassiveUngrab: i32 = 5;
pub const XINotifyAncestor: i32 = 0;
pub const XINotifyVirtual: i32 = 1;
pub const XINotifyInferior: i32 = 2;
pub const XINotifyNonlinear: i32 = 3;
pub const XINotifyNonlinearVirtual: i32 = 4;
pub const XINotifyPointer: i32 = 5;
pub const XINotifyPointerRoot: i32 = 6;
pub const XINotifyDetailNone: i32 = 7;
pub const XIGrabModeSync: i32 = 0;
pub const XIGrabModeAsync: i32 = 1;
pub const XIGrabModeTouch: i32 = 2;
pub const XIGrabSuccess: i32 = 0;
pub const XIAlreadyGrabbed: i32 = 1;
pub const XIGrabInvalidTime: i32 = 2;
pub const XIGrabNotViewable: i32 = 3;
pub const XIGrabFrozen: i32 = 4;
pub const XIGrabtypeButton: i32 = 0;
pub const XIGrabtypeKeycode: i32 = 1;
pub const XIGrabtypeEnter: i32 = 2;
pub const XIGrabtypeFocusIn: i32 = 3;
pub const XIGrabtypeTouchBegin: i32 = 4;
pub const XIAnyButton: i32 = 0;
pub const XIAnyKeycode: i32 = 0;
pub const XIAsyncDevice: i32 = 0;
pub const XISyncDevice: i32 = 1;
pub const XIReplayDevice: i32 = 2;
pub const XIAsyncPairedDevice: i32 = 3;
pub const XIAsyncPair: i32 = 4;
pub const XISyncPair: i32 = 5;
pub const XIAcceptTouch: i32 = 6;
pub const XIRejectTouch: i32 = 7;
pub const XISlaveSwitch: i32 = 1;
pub const XIDeviceChange: i32 = 2;
pub const XIMasterAdded: i32 = (1 << 0);
pub const XIMasterRemoved: i32 = (1 << 1);
pub const XISlaveAdded: i32 = (1 << 2);
pub const XISlaveRemoved: i32 = (1 << 3);
pub const XISlaveAttached: i32 = (1 << 4);
pub const XISlaveDetached: i32 = (1 << 5);
pub const XIDeviceEnabled: i32 = (1 << 6);
pub const XIDeviceDisabled: i32 = (1 << 7);
pub const XIAddMaster: i32 = 1;
pub const XIRemoveMaster: i32 = 2;
pub const XIAttachSlave: i32 = 3;
pub const XIDetachSlave: i32 = 4;
pub const XIAttachToMaster: i32 = 1;
pub const XIFloating: i32 = 2;
pub const XIModeRelative: i32 = 0;
pub const XIModeAbsolute: i32 = 1;
pub const XIMasterPointer: i32 = 1;
pub const XIMasterKeyboard: i32 = 2;
pub const XISlavePointer: i32 = 3;
pub const XISlaveKeyboard: i32 = 4;
pub const XIFloatingSlave: i32 = 5;
pub const XIKeyClass: i32 = 0;
pub const XIButtonClass: i32 = 1;
pub const XIValuatorClass: i32 = 2;
pub const XIScrollClass: i32 = 3;
pub const XITouchClass: i32 = 8;
pub const XIScrollTypeVertical: i32 = 1;
pub const XIScrollTypeHorizontal: i32 = 2;
pub const XIScrollFlagNoEmulation: i32 = (1 << 0);
pub const XIScrollFlagPreferred: i32 = (1 << 1);
pub const XIKeyRepeat: i32 = (1 << 16);
pub const XIPointerEmulated: i32 = (1 << 16);
pub const XITouchPendingEnd: i32 = (1 << 16);
pub const XITouchEmulatingPointer: i32 = (1 << 17);
pub const XIBarrierPointerReleased: i32 = (1 << 0);
pub const XIBarrierDeviceIsGrabbed: i32 = (1 << 1);
pub const XIDirectTouch: i32 = 1;
pub const XIDependentTouch: i32 = 2;
pub const XIAllDevices: i32 = 0;
pub const XIAllMasterDevices: i32 = 1;
pub const XI_DeviceChanged: i32 = 1;
pub const XI_KeyPress: i32 = 2;
pub const XI_KeyRelease: i32 = 3;
pub const XI_ButtonPress: i32 = 4;
pub const XI_ButtonRelease: i32 = 5;
pub const XI_Motion: i32 = 6;
pub const XI_Enter: i32 = 7;
pub const XI_Leave: i32 = 8;
pub const XI_FocusIn: i32 = 9;
pub const XI_FocusOut: i32 = 10;
pub const XI_HierarchyChanged: i32 = 11;
pub const XI_PropertyEvent: i32 = 12;
pub const XI_RawKeyPress: i32 = 13;
pub const XI_RawKeyRelease: i32 = 14;
pub const XI_RawButtonPress: i32 = 15;
pub const XI_RawButtonRelease: i32 = 16;
pub const XI_RawMotion: i32 = 17;
pub const XI_TouchBegin: i32 = 18 /* XI 2.2 */;
pub const XI_TouchUpdate: i32 = 19;
pub const XI_TouchEnd: i32 = 20;
pub const XI_TouchOwnership: i32 = 21;
pub const XI_RawTouchBegin: i32 = 22;
pub const XI_RawTouchUpdate: i32 = 23;
pub const XI_RawTouchEnd: i32 = 24;
pub const XI_BarrierHit: i32 = 25 /* XI 2.3 */;
pub const XI_BarrierLeave: i32 = 26;
pub const XI_LASTEVENT: i32 = XI_BarrierLeave;
pub const XI_DeviceChangedMask: i32 = (1 << XI_DeviceChanged);
pub const XI_KeyPressMask: i32 = (1 << XI_KeyPress);
pub const XI_KeyReleaseMask: i32 = (1 << XI_KeyRelease);
pub const XI_ButtonPressMask: i32 = (1 << XI_ButtonPress);
pub const XI_ButtonReleaseMask: i32 = (1 << XI_ButtonRelease);
pub const XI_MotionMask: i32 = (1 << XI_Motion);
pub const XI_EnterMask: i32 = (1 << XI_Enter);
pub const XI_LeaveMask: i32 = (1 << XI_Leave);
pub const XI_FocusInMask: i32 = (1 << XI_FocusIn);
pub const XI_FocusOutMask: i32 = (1 << XI_FocusOut);
pub const XI_HierarchyChangedMask: i32 = (1 << XI_HierarchyChanged);
pub const XI_PropertyEventMask: i32 = (1 << XI_PropertyEvent);
pub const XI_RawKeyPressMask: i32 = (1 << XI_RawKeyPress);
pub const XI_RawKeyReleaseMask: i32 = (1 << XI_RawKeyRelease);
pub const XI_RawButtonPressMask: i32 = (1 << XI_RawButtonPress);
pub const XI_RawButtonReleaseMask: i32 = (1 << XI_RawButtonRelease);
pub const XI_RawMotionMask: i32 = (1 << XI_RawMotion);
pub const XI_TouchBeginMask: i32 = (1 << XI_TouchBegin);
pub const XI_TouchEndMask: i32 = (1 << XI_TouchEnd);
pub const XI_TouchOwnershipChangedMask: i32 = (1 << XI_TouchOwnership);
pub const XI_TouchUpdateMask: i32 = (1 << XI_TouchUpdate);
pub const XI_RawTouchBeginMask: i32 = (1 << XI_RawTouchBegin);
pub const XI_RawTouchEndMask: i32 = (1 << XI_RawTouchEnd);
pub const XI_RawTouchUpdateMask: i32 = (1 << XI_RawTouchUpdate);
pub const XI_BarrierHitMask: i32 = (1 << XI_BarrierHit);
pub const XI_BarrierLeaveMask: i32 = (1 << XI_BarrierLeave);

//
// structs
// (auto-generated with rust-bindgen)
//

#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed1 {
    pub _type: ::std::os::raw::c_int,
    pub name: *mut ::std::os::raw::c_char,
    pub send_core: ::std::os::raw::c_int,
    pub enable: ::std::os::raw::c_int,
}
impl ::std::clone::Clone for Struct_Unnamed1 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed1 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIAddMasterInfo = Struct_Unnamed1;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed2 {
    pub _type: ::std::os::raw::c_int,
    pub deviceid: ::std::os::raw::c_int,
    pub return_mode: ::std::os::raw::c_int,
    pub return_pointer: ::std::os::raw::c_int,
    pub return_keyboard: ::std::os::raw::c_int,
}
impl ::std::clone::Clone for Struct_Unnamed2 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed2 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIRemoveMasterInfo = Struct_Unnamed2;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed3 {
    pub _type: ::std::os::raw::c_int,
    pub deviceid: ::std::os::raw::c_int,
    pub new_master: ::std::os::raw::c_int,
}
impl ::std::clone::Clone for Struct_Unnamed3 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed3 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIAttachSlaveInfo = Struct_Unnamed3;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed4 {
    pub _type: ::std::os::raw::c_int,
    pub deviceid: ::std::os::raw::c_int,
}
impl ::std::clone::Clone for Struct_Unnamed4 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed4 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIDetachSlaveInfo = Struct_Unnamed4;
#[repr(C)]
#[derive(Copy)]
pub struct Union_Unnamed5 {
    pub _bindgen_data_: [u64; 3usize],
}
impl Union_Unnamed5 {
    pub unsafe fn _type(&mut self) -> *mut ::std::os::raw::c_int {
        let raw: *mut u8 = ::std::mem::transmute(&self._bindgen_data_);
        ::std::mem::transmute(raw.offset(0))
    }
    pub unsafe fn add(&mut self) -> *mut XIAddMasterInfo {
        let raw: *mut u8 = ::std::mem::transmute(&self._bindgen_data_);
        ::std::mem::transmute(raw.offset(0))
    }
    pub unsafe fn remove(&mut self) -> *mut XIRemoveMasterInfo {
        let raw: *mut u8 = ::std::mem::transmute(&self._bindgen_data_);
        ::std::mem::transmute(raw.offset(0))
    }
    pub unsafe fn attach(&mut self) -> *mut XIAttachSlaveInfo {
        let raw: *mut u8 = ::std::mem::transmute(&self._bindgen_data_);
        ::std::mem::transmute(raw.offset(0))
    }
    pub unsafe fn detach(&mut self) -> *mut XIDetachSlaveInfo {
        let raw: *mut u8 = ::std::mem::transmute(&self._bindgen_data_);
        ::std::mem::transmute(raw.offset(0))
    }
}
impl ::std::clone::Clone for Union_Unnamed5 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Union_Unnamed5 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIAnyHierarchyChangeInfo = Union_Unnamed5;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed6 {
    pub base: ::std::os::raw::c_int,
    pub latched: ::std::os::raw::c_int,
    pub locked: ::std::os::raw::c_int,
    pub effective: ::std::os::raw::c_int,
}
impl ::std::clone::Clone for Struct_Unnamed6 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed6 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIModifierState = Struct_Unnamed6;
pub type XIGroupState = XIModifierState;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed7 {
    pub mask_len: ::std::os::raw::c_int,
    pub mask: *mut ::std::os::raw::c_uchar,
}
impl ::std::clone::Clone for Struct_Unnamed7 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed7 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIButtonState = Struct_Unnamed7;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed8 {
    pub mask_len: ::std::os::raw::c_int,
    pub mask: *mut ::std::os::raw::c_uchar,
    pub values: *mut ::std::os::raw::c_double,
}
impl ::std::clone::Clone for Struct_Unnamed8 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed8 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIValuatorState = Struct_Unnamed8;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed9 {
    pub deviceid: ::std::os::raw::c_int,
    pub mask_len: ::std::os::raw::c_int,
    pub mask: *mut ::std::os::raw::c_uchar,
}
impl ::std::clone::Clone for Struct_Unnamed9 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed9 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIEventMask = Struct_Unnamed9;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed10 {
    pub _type: ::std::os::raw::c_int,
    pub sourceid: ::std::os::raw::c_int,
}
impl ::std::clone::Clone for Struct_Unnamed10 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed10 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIAnyClassInfo = Struct_Unnamed10;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed11 {
    pub _type: ::std::os::raw::c_int,
    pub sourceid: ::std::os::raw::c_int,
    pub num_buttons: ::std::os::raw::c_int,
    pub labels: *mut Atom,
    pub state: XIButtonState,
}
impl ::std::clone::Clone for Struct_Unnamed11 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed11 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIButtonClassInfo = Struct_Unnamed11;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed12 {
    pub _type: ::std::os::raw::c_int,
    pub sourceid: ::std::os::raw::c_int,
    pub num_keycodes: ::std::os::raw::c_int,
    pub keycodes: *mut ::std::os::raw::c_int,
}
impl ::std::clone::Clone for Struct_Unnamed12 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed12 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIKeyClassInfo = Struct_Unnamed12;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed13 {
    pub _type: ::std::os::raw::c_int,
    pub sourceid: ::std::os::raw::c_int,
    pub number: ::std::os::raw::c_int,
    pub label: Atom,
    pub min: ::std::os::raw::c_double,
    pub max: ::std::os::raw::c_double,
    pub value: ::std::os::raw::c_double,
    pub resolution: ::std::os::raw::c_int,
    pub mode: ::std::os::raw::c_int,
}
impl ::std::clone::Clone for Struct_Unnamed13 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed13 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIValuatorClassInfo = Struct_Unnamed13;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed14 {
    pub _type: ::std::os::raw::c_int,
    pub sourceid: ::std::os::raw::c_int,
    pub number: ::std::os::raw::c_int,
    pub scroll_type: ::std::os::raw::c_int,
    pub increment: ::std::os::raw::c_double,
    pub flags: ::std::os::raw::c_int,
}
impl ::std::clone::Clone for Struct_Unnamed14 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed14 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIScrollClassInfo = Struct_Unnamed14;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed15 {
    pub _type: ::std::os::raw::c_int,
    pub sourceid: ::std::os::raw::c_int,
    pub mode: ::std::os::raw::c_int,
    pub num_touches: ::std::os::raw::c_int,
}
impl ::std::clone::Clone for Struct_Unnamed15 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed15 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XITouchClassInfo = Struct_Unnamed15;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed16 {
    pub deviceid: ::std::os::raw::c_int,
    pub name: *mut ::std::os::raw::c_char,
    pub _use: ::std::os::raw::c_int,
    pub attachment: ::std::os::raw::c_int,
    pub enabled: ::std::os::raw::c_int,
    pub num_classes: ::std::os::raw::c_int,
    pub classes: *mut *mut XIAnyClassInfo,
}
impl ::std::clone::Clone for Struct_Unnamed16 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed16 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIDeviceInfo = Struct_Unnamed16;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed17 {
    pub modifiers: ::std::os::raw::c_int,
    pub status: ::std::os::raw::c_int,
}
impl ::std::clone::Clone for Struct_Unnamed17 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed17 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIGrabModifiers = Struct_Unnamed17;
pub type BarrierEventID = ::std::os::raw::c_uint;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed18 {
    pub deviceid: ::std::os::raw::c_int,
    pub barrier: PointerBarrier,
    pub eventid: BarrierEventID,
}
impl ::std::clone::Clone for Struct_Unnamed18 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed18 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIBarrierReleasePointerInfo = Struct_Unnamed18;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed19 {
    pub _type: ::std::os::raw::c_int,
    pub serial: ::std::os::raw::c_ulong,
    pub send_event: ::std::os::raw::c_int,
    pub display: *mut Display,
    pub extension: ::std::os::raw::c_int,
    pub evtype: ::std::os::raw::c_int,
    pub time: Time,
}
impl ::std::clone::Clone for Struct_Unnamed19 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed19 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIEvent = Struct_Unnamed19;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed20 {
    pub deviceid: ::std::os::raw::c_int,
    pub attachment: ::std::os::raw::c_int,
    pub _use: ::std::os::raw::c_int,
    pub enabled: ::std::os::raw::c_int,
    pub flags: ::std::os::raw::c_int,
}
impl ::std::clone::Clone for Struct_Unnamed20 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed20 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIHierarchyInfo = Struct_Unnamed20;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed21 {
    pub _type: ::std::os::raw::c_int,
    pub serial: ::std::os::raw::c_ulong,
    pub send_event: ::std::os::raw::c_int,
    pub display: *mut Display,
    pub extension: ::std::os::raw::c_int,
    pub evtype: ::std::os::raw::c_int,
    pub time: Time,
    pub flags: ::std::os::raw::c_int,
    pub num_info: ::std::os::raw::c_int,
    pub info: *mut XIHierarchyInfo,
}
impl ::std::clone::Clone for Struct_Unnamed21 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed21 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIHierarchyEvent = Struct_Unnamed21;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed22 {
    pub _type: ::std::os::raw::c_int,
    pub serial: ::std::os::raw::c_ulong,
    pub send_event: ::std::os::raw::c_int,
    pub display: *mut Display,
    pub extension: ::std::os::raw::c_int,
    pub evtype: ::std::os::raw::c_int,
    pub time: Time,
    pub deviceid: ::std::os::raw::c_int,
    pub sourceid: ::std::os::raw::c_int,
    pub reason: ::std::os::raw::c_int,
    pub num_classes: ::std::os::raw::c_int,
    pub classes: *mut *mut XIAnyClassInfo,
}
impl ::std::clone::Clone for Struct_Unnamed22 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed22 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIDeviceChangedEvent = Struct_Unnamed22;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed23 {
    pub _type: ::std::os::raw::c_int,
    pub serial: ::std::os::raw::c_ulong,
    pub send_event: ::std::os::raw::c_int,
    pub display: *mut Display,
    pub extension: ::std::os::raw::c_int,
    pub evtype: ::std::os::raw::c_int,
    pub time: Time,
    pub deviceid: ::std::os::raw::c_int,
    pub sourceid: ::std::os::raw::c_int,
    pub detail: ::std::os::raw::c_int,
    pub root: Window,
    pub event: Window,
    pub child: Window,
    pub root_x: ::std::os::raw::c_double,
    pub root_y: ::std::os::raw::c_double,
    pub event_x: ::std::os::raw::c_double,
    pub event_y: ::std::os::raw::c_double,
    pub flags: ::std::os::raw::c_int,
    pub buttons: XIButtonState,
    pub valuators: XIValuatorState,
    pub mods: XIModifierState,
    pub group: XIGroupState,
}
impl ::std::clone::Clone for Struct_Unnamed23 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed23 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIDeviceEvent = Struct_Unnamed23;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed24 {
    pub _type: ::std::os::raw::c_int,
    pub serial: ::std::os::raw::c_ulong,
    pub send_event: ::std::os::raw::c_int,
    pub display: *mut Display,
    pub extension: ::std::os::raw::c_int,
    pub evtype: ::std::os::raw::c_int,
    pub time: Time,
    pub deviceid: ::std::os::raw::c_int,
    pub sourceid: ::std::os::raw::c_int,
    pub detail: ::std::os::raw::c_int,
    pub flags: ::std::os::raw::c_int,
    pub valuators: XIValuatorState,
    pub raw_values: *mut ::std::os::raw::c_double,
}
impl ::std::clone::Clone for Struct_Unnamed24 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed24 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIRawEvent = Struct_Unnamed24;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed25 {
    pub _type: ::std::os::raw::c_int,
    pub serial: ::std::os::raw::c_ulong,
    pub send_event: ::std::os::raw::c_int,
    pub display: *mut Display,
    pub extension: ::std::os::raw::c_int,
    pub evtype: ::std::os::raw::c_int,
    pub time: Time,
    pub deviceid: ::std::os::raw::c_int,
    pub sourceid: ::std::os::raw::c_int,
    pub detail: ::std::os::raw::c_int,
    pub root: Window,
    pub event: Window,
    pub child: Window,
    pub root_x: ::std::os::raw::c_double,
    pub root_y: ::std::os::raw::c_double,
    pub event_x: ::std::os::raw::c_double,
    pub event_y: ::std::os::raw::c_double,
    pub mode: ::std::os::raw::c_int,
    pub focus: ::std::os::raw::c_int,
    pub same_screen: ::std::os::raw::c_int,
    pub buttons: XIButtonState,
    pub mods: XIModifierState,
    pub group: XIGroupState,
}
impl ::std::clone::Clone for Struct_Unnamed25 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed25 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIEnterEvent = Struct_Unnamed25;
pub type XILeaveEvent = XIEnterEvent;
pub type XIFocusInEvent = XIEnterEvent;
pub type XIFocusOutEvent = XIEnterEvent;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed26 {
    pub _type: ::std::os::raw::c_int,
    pub serial: ::std::os::raw::c_ulong,
    pub send_event: ::std::os::raw::c_int,
    pub display: *mut Display,
    pub extension: ::std::os::raw::c_int,
    pub evtype: ::std::os::raw::c_int,
    pub time: Time,
    pub deviceid: ::std::os::raw::c_int,
    pub property: Atom,
    pub what: ::std::os::raw::c_int,
}
impl ::std::clone::Clone for Struct_Unnamed26 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed26 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIPropertyEvent = Struct_Unnamed26;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed27 {
    pub _type: ::std::os::raw::c_int,
    pub serial: ::std::os::raw::c_ulong,
    pub send_event: ::std::os::raw::c_int,
    pub display: *mut Display,
    pub extension: ::std::os::raw::c_int,
    pub evtype: ::std::os::raw::c_int,
    pub time: Time,
    pub deviceid: ::std::os::raw::c_int,
    pub sourceid: ::std::os::raw::c_int,
    pub touchid: ::std::os::raw::c_uint,
    pub root: Window,
    pub event: Window,
    pub child: Window,
    pub flags: ::std::os::raw::c_int,
}
impl ::std::clone::Clone for Struct_Unnamed27 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed27 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XITouchOwnershipEvent = Struct_Unnamed27;
#[repr(C)]
#[derive(Copy)]
pub struct Struct_Unnamed28 {
    pub _type: ::std::os::raw::c_int,
    pub serial: ::std::os::raw::c_ulong,
    pub send_event: ::std::os::raw::c_int,
    pub display: *mut Display,
    pub extension: ::std::os::raw::c_int,
    pub evtype: ::std::os::raw::c_int,
    pub time: Time,
    pub deviceid: ::std::os::raw::c_int,
    pub sourceid: ::std::os::raw::c_int,
    pub event: Window,
    pub root: Window,
    pub root_x: ::std::os::raw::c_double,
    pub root_y: ::std::os::raw::c_double,
    pub dx: ::std::os::raw::c_double,
    pub dy: ::std::os::raw::c_double,
    pub dtime: ::std::os::raw::c_int,
    pub flags: ::std::os::raw::c_int,
    pub barrier: PointerBarrier,
    pub eventid: BarrierEventID,
}
impl ::std::clone::Clone for Struct_Unnamed28 {
    fn clone(&self) -> Self { *self }
}
impl ::std::default::Default for Struct_Unnamed28 {
    fn default() -> Self { unsafe { ::std::mem::zeroed() } }
}
pub type XIBarrierEvent = Struct_Unnamed28;

