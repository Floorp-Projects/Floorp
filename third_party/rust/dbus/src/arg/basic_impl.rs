use ffi;
use super::*;
use super::check;
use {Signature, Path, OwnedFd};
use std::{ptr, any, mem};
use std::ffi::CStr;
use std::os::raw::{c_void, c_char, c_int};


fn arg_append_basic<T>(i: *mut ffi::DBusMessageIter, arg_type: ArgType, v: T) {
    let p = &v as *const _ as *const c_void;
    unsafe {
        check("dbus_message_iter_append_basic", ffi::dbus_message_iter_append_basic(i, arg_type as c_int, p));
    };
}

fn arg_get_basic<T>(i: *mut ffi::DBusMessageIter, arg_type: ArgType) -> Option<T> {
    unsafe {
        let mut c: T = mem::zeroed();
        if ffi::dbus_message_iter_get_arg_type(i) != arg_type as c_int { return None };
        ffi::dbus_message_iter_get_basic(i, &mut c as *mut _ as *mut c_void);
        Some(c)
    }
}

fn arg_append_f64(i: *mut ffi::DBusMessageIter, arg_type: ArgType, v: f64) {
    let p = &v as *const _ as *const c_void;
    unsafe {
        check("dbus_message_iter_append_basic", ffi::dbus_message_iter_append_basic(i, arg_type as c_int, p));
    };
}

fn arg_get_f64(i: *mut ffi::DBusMessageIter, arg_type: ArgType) -> Option<f64> {
    let mut c = 0f64;
    unsafe {
        if ffi::dbus_message_iter_get_arg_type(i) != arg_type as c_int { return None };
        ffi::dbus_message_iter_get_basic(i, &mut c as *mut _ as *mut c_void);
    }
    Some(c)
}

fn arg_append_str(i: *mut ffi::DBusMessageIter, arg_type: ArgType, v: &CStr) {
    let p = v.as_ptr();
    let q = &p as *const _ as *const c_void;
    unsafe {
        check("dbus_message_iter_append_basic", ffi::dbus_message_iter_append_basic(i, arg_type as c_int, q));
    };
}

unsafe fn arg_get_str<'a>(i: *mut ffi::DBusMessageIter, arg_type: ArgType) -> Option<&'a CStr> {
    if ffi::dbus_message_iter_get_arg_type(i) != arg_type as c_int { return None };
    let mut p = ptr::null_mut();
    ffi::dbus_message_iter_get_basic(i, &mut p as *mut _ as *mut c_void);
    Some(CStr::from_ptr(p as *const c_char))
}




// Implementation for basic types.

macro_rules! integer_impl {
    ($t: ident, $s: ident, $f: expr, $i: ident, $ii: expr, $u: ident, $uu: expr, $fff: ident, $ff: expr) => {

impl Arg for $t {
    const ARG_TYPE: ArgType = ArgType::$s;
    #[inline]
    fn signature() -> Signature<'static> { unsafe { Signature::from_slice_unchecked($f) } }
}

impl Append for $t {
    fn append(self, i: &mut IterAppend) { arg_append_basic(&mut i.0, ArgType::$s, self) }
}

impl<'a> Get<'a> for $t {
    fn get(i: &mut Iter) -> Option<Self> { arg_get_basic(&mut i.0, ArgType::$s) }
}

impl RefArg for $t {
    #[inline]
    fn arg_type(&self) -> ArgType { ArgType::$s }
    #[inline]
    fn signature(&self) -> Signature<'static> { unsafe { Signature::from_slice_unchecked($f) } }
    #[inline]
    fn append(&self, i: &mut IterAppend) { arg_append_basic(&mut i.0, ArgType::$s, *self) }
    #[inline]
    fn as_any(&self) -> &any::Any { self }
    #[inline]
    fn as_any_mut(&mut self) -> &mut any::Any { self }
    #[inline]
    fn as_i64(&self) -> Option<i64> { let $i = *self; $ii }
    #[inline]
    fn as_u64(&self) -> Option<u64> { let $u = *self; $uu }
    #[inline]
    fn as_f64(&self) -> Option<f64> { let $fff = *self; $ff }
    #[inline]
    fn box_clone(&self) -> Box<RefArg + 'static> { Box::new(self.clone()) }
}

impl DictKey for $t {}
unsafe impl FixedArray for $t {}

}} // End of macro_rules

integer_impl!(u8, Byte, b"y\0", i, Some(i as i64),    u, Some(u as u64), f, Some(f as f64));
integer_impl!(i16, Int16, b"n\0", i, Some(i as i64),  _u, None,          f, Some(f as f64));
integer_impl!(u16, UInt16, b"q\0", i, Some(i as i64), u, Some(u as u64), f, Some(f as f64));
integer_impl!(i32, Int32, b"i\0", i, Some(i as i64),  _u, None,          f, Some(f as f64));
integer_impl!(u32, UInt32, b"u\0", i, Some(i as i64), u, Some(u as u64), f, Some(f as f64));
integer_impl!(i64, Int64, b"x\0", i, Some(i),         _u, None,          _f, None);
integer_impl!(u64, UInt64, b"t\0", _i, None,          u, Some(u as u64), _f, None);


macro_rules! refarg_impl {
    ($t: ty, $i: ident, $ii: expr, $ss: expr, $uu: expr, $ff: expr) => {

impl RefArg for $t {
    #[inline]
    fn arg_type(&self) -> ArgType { <$t as Arg>::ARG_TYPE }
    #[inline]
    fn signature(&self) -> Signature<'static> { <$t as Arg>::signature() }
    #[inline]
    fn append(&self, i: &mut IterAppend) { <$t as Append>::append(self.clone(), i) }
    #[inline]
    fn as_any(&self) -> &any::Any { self }
    #[inline]
    fn as_any_mut(&mut self) -> &mut any::Any { self }
    #[inline]
    fn as_i64(&self) -> Option<i64> { let $i = self; $ii }
    #[inline]
    fn as_u64(&self) -> Option<u64> { let $i = self; $uu }
    #[inline]
    fn as_f64(&self) -> Option<f64> { let $i = self; $ff }
    #[inline]
    fn as_str(&self) -> Option<&str> { let $i = self; $ss }
    #[inline]
    fn box_clone(&self) -> Box<RefArg + 'static> { Box::new(self.clone()) }
}

    }
}


impl Arg for bool {
    const ARG_TYPE: ArgType = ArgType::Boolean;
    fn signature() -> Signature<'static> { unsafe { Signature::from_slice_unchecked(b"b\0") } }
}
impl Append for bool {
    fn append(self, i: &mut IterAppend) { arg_append_basic(&mut i.0, ArgType::Boolean, if self {1} else {0}) }
}
impl DictKey for bool {}
impl<'a> Get<'a> for bool {
    fn get(i: &mut Iter) -> Option<Self> { arg_get_basic::<u32>(&mut i.0, ArgType::Boolean).map(|q| q != 0) }
}

refarg_impl!(bool, _i, Some(if *_i { 1 } else { 0 }), None, Some(if *_i { 1 as u64 } else { 0 as u64 }), Some(if *_i { 1 as f64 } else { 0 as f64 }));

impl Arg for f64 {
    const ARG_TYPE: ArgType = ArgType::Double;
    fn signature() -> Signature<'static> { unsafe { Signature::from_slice_unchecked(b"d\0") } }
}
impl Append for f64 {
    fn append(self, i: &mut IterAppend) { arg_append_f64(&mut i.0, ArgType::Double, self) }
}
impl DictKey for f64 {}
impl<'a> Get<'a> for f64 {
    fn get(i: &mut Iter) -> Option<Self> { arg_get_f64(&mut i.0, ArgType::Double) }
}
unsafe impl FixedArray for f64 {}

refarg_impl!(f64, _i, None, None, None, Some(*_i));

/// Represents a D-Bus string.
impl<'a> Arg for &'a str {
    const ARG_TYPE: ArgType = ArgType::String;
    fn signature() -> Signature<'static> { unsafe { Signature::from_slice_unchecked(b"s\0") } }
}

impl<'a> Append for &'a str {
    fn append(self, i: &mut IterAppend) {
        use std::borrow::Cow;
        let b: &[u8] = self.as_bytes();
        let v: Cow<[u8]> = if b.len() > 0 && b[b.len()-1] == 0 { Cow::Borrowed(b) }
        else {
            let mut bb: Vec<u8> = b.into();
            bb.push(0);
            Cow::Owned(bb)
        };
        let z = unsafe { CStr::from_ptr(v.as_ptr() as *const c_char) };
        arg_append_str(&mut i.0, ArgType::String, &z)
    }
}
impl<'a> DictKey for &'a str {}
impl<'a> Get<'a> for &'a str {
    fn get(i: &mut Iter<'a>) -> Option<&'a str> { unsafe { arg_get_str(&mut i.0, ArgType::String) }
        .and_then(|s| s.to_str().ok()) }
}

impl<'a> Arg for String {
    const ARG_TYPE: ArgType = ArgType::String;
    fn signature() -> Signature<'static> { unsafe { Signature::from_slice_unchecked(b"s\0") } }
}
impl<'a> Append for String {
    fn append(mut self, i: &mut IterAppend) {
        self.push_str("\0");
        let s: &str = &self;
        s.append(i)
    }
}
impl<'a> DictKey for String {}
impl<'a> Get<'a> for String {
    fn get(i: &mut Iter<'a>) -> Option<String> { <&str>::get(i).map(|s| String::from(s)) }
}

refarg_impl!(String, _i, None, Some(&_i), None, None);

/// Represents a D-Bus string.
impl<'a> Arg for &'a CStr {
    const ARG_TYPE: ArgType = ArgType::String;
    fn signature() -> Signature<'static> { unsafe { Signature::from_slice_unchecked(b"s\0") } }
}

/*
/// Note: Will give D-Bus errors in case the CStr is not valid UTF-8.
impl<'a> Append for &'a CStr {
    fn append(self, i: &mut IterAppend) {
        arg_append_str(&mut i.0, Self::arg_type(), &self)
    }
}
*/

impl<'a> DictKey for &'a CStr {}
impl<'a> Get<'a> for &'a CStr {
    fn get(i: &mut Iter<'a>) -> Option<&'a CStr> { unsafe { arg_get_str(&mut i.0, Self::ARG_TYPE) }}
}

impl Arg for OwnedFd {
    const ARG_TYPE: ArgType = ArgType::UnixFd;
    fn signature() -> Signature<'static> { unsafe { Signature::from_slice_unchecked(b"h\0") } }
}
impl Append for OwnedFd {
    fn append(self, i: &mut IterAppend) {
        use std::os::unix::io::AsRawFd;
        arg_append_basic(&mut i.0, ArgType::UnixFd, self.as_raw_fd())
    }
}
impl DictKey for OwnedFd {}
impl<'a> Get<'a> for OwnedFd {
    fn get(i: &mut Iter) -> Option<Self> {
        arg_get_basic(&mut i.0, ArgType::UnixFd).map(|q| OwnedFd::new(q)) 
    }
}

refarg_impl!(OwnedFd, _i, { use std::os::unix::io::AsRawFd; Some(_i.as_raw_fd() as i64) }, None, None, None);

macro_rules! string_impl {
    ($t: ident, $s: ident, $f: expr) => {

impl<'a> Arg for $t<'a> {
    const ARG_TYPE: ArgType = ArgType::$s;
    fn signature() -> Signature<'static> { unsafe { Signature::from_slice_unchecked($f) } }
}

impl RefArg for $t<'static> {
    fn arg_type(&self) -> ArgType { ArgType::$s }
    fn signature(&self) -> Signature<'static> { unsafe { Signature::from_slice_unchecked($f) } }
    fn append(&self, i: &mut IterAppend) { arg_append_str(&mut i.0, ArgType::$s, self.as_cstr()) }
    #[inline]
    fn as_any(&self) -> &any::Any { self }
    #[inline]
    fn as_any_mut(&mut self) -> &mut any::Any { self }
    #[inline]
    fn as_str(&self) -> Option<&str> { Some(self) }
    #[inline]
    fn box_clone(&self) -> Box<RefArg + 'static> { Box::new(self.clone().into_static()) }
}

impl<'a> DictKey for $t<'a> {}

impl<'a> Append for $t<'a> {
    fn append(self, i: &mut IterAppend) {
        arg_append_str(&mut i.0, ArgType::$s, self.as_cstr())
    }
}

/*

Unfortunately, this does not work because it conflicts with getting a $t<'static>.

impl<'a> Get<'a> for $t<'a> {
    fn get(i: &mut Iter<'a>) -> Option<$t<'a>> { unsafe { arg_get_str(&mut i.0, ArgType::$s) }
        .map(|s| unsafe { $t::from_slice_unchecked(s.to_bytes_with_nul()) } ) }
}
*/

impl<'a> Get<'a> for $t<'static> {
    fn get(i: &mut Iter<'a>) -> Option<$t<'static>> { unsafe {
        arg_get_str(&mut i.0, ArgType::$s).map(|s| $t::from_slice_unchecked(s.to_bytes_with_nul()).into_static())
    }}
}


    }
}

string_impl!(Path, ObjectPath, b"o\0");
string_impl!(Signature, Signature, b"g\0");

