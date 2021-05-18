/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! The automatically generated code from `xpcom_macros` depends on some types
//! which are defined in other libraries which `xpcom` depends on, but which may
//! not be `extern crate`-ed into the crate the macros are expanded into. This
//! module re-exports those types from `xpcom` so that they can be used from the
//! macro.
// re-export libc so it can be used by the procedural macro.
pub extern crate libc;

pub use nsstring::{nsACString, nsAString, nsCString, nsString};

pub use nserror::{nsresult, NS_ERROR_NO_INTERFACE, NS_OK};

pub use std::ops::Deref;

/// Helper method used by the xpcom codegen, it is not public API or meant for
/// calling outside of that context.
///
/// Takes a reference to the `this` pointer received from XPIDL, and offsets and
/// casts it to a reference to the concrete rust `struct` type, `U`.
///
/// `vtable_index` is the index, and therefore the offset in pointers, of the
/// vtable for `T` in `U`.
///
/// A reference to `this` is taken, instead of taking `*const T` by value, to use
/// as a lifetime bound, such that the returned `&U` reference has a bounded
/// lifetime when used to call the implementation method.
#[inline]
pub unsafe fn transmute_from_vtable_ptr<'a, T, U>(
    this: &'a *const T,
    vtable_index: usize,
) -> &'a U {
    &*((*this as *const *const ()).sub(vtable_index) as *const U)
}

/// On some ABIs, extra information is included before the vtable's function
/// pointers which are used to implement RTTI. We build Gecko with RTTI
/// disabled, however these fields may still be present to support
/// `dynamic_cast<void*>` on our rust VTables in case they are accessed.
///
/// Itanium ABI Layout: https://refspecs.linuxbase.org/cxxabi-1.83.html#vtable
#[repr(C)]
pub struct VTableExtra<T> {
    #[cfg(not(windows))]
    pub offset: isize,
    #[cfg(not(windows))]
    pub typeinfo: *const libc::c_void,
    pub vtable: T,
}
