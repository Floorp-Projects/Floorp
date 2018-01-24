/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module contains definitions of interfaces which are used in idl files
//! as forward declarations, but are not actually defined in an idl file.
//!
//! NOTE: The IIDs in these files must be kept in sync with the IDL definitions
//! in the corresponding C++ files.

use nsID;

// XXX: This macro should have an option for a custom base interface instead of
// nsISupports, such that nsIDocument can have nsINode as a base, etc. For now,
// query_interface should be sufficient.
macro_rules! nonidl {
    ($name:ident, $iid:expr) => {
        /// This interface is referenced from idl files, but not defined in
        /// them. It exports no methods to rust code.
        #[repr(C)]
        pub struct $name {
            _vtable: *const $crate::interfaces::nsISupportsVTable,
        }

        unsafe impl $crate::XpCom for $name {
            const IID: $crate::nsIID = $iid;
        }

        unsafe impl $crate::RefCounted for $name {
            #[inline]
            unsafe fn addref(&self) {
                self.AddRef();
            }
            #[inline]
            unsafe fn release(&self) {
                self.Release();
            }
        }

        impl ::std::ops::Deref for $name {
            type Target = $crate::interfaces::nsISupports;
            #[inline]
            fn deref(&self) -> &$crate::interfaces::nsISupports {
                unsafe {
                    ::std::mem::transmute(self)
                }
            }
        }
    }
}

// Must be kept in sync with nsIDocument.h
nonidl!(nsIDocument,
        nsID(0xce1f7627, 0x7109, 0x4977,
             [0xba, 0x77, 0x49, 0x0f, 0xfd, 0xe0, 0x7a, 0xaa]));

// Must be kept in sync with nsINode.h
nonidl!(nsINode,
        nsID(0x70ba4547, 0x7699, 0x44fc,
             [0xb3, 0x20, 0x52, 0xdb, 0xe3, 0xd1, 0xf9, 0x0a]));

// Must be kept in sync with nsIContent.h
nonidl!(nsIContent,
        nsID(0x8e1bab9d, 0x8815, 0x4d2c,
             [0xa2, 0x4d, 0x7a, 0xba, 0x52, 0x39, 0xdc, 0x22]));

// Must be kept in sync with nsIConsoleReportCollector.h
nonidl!(nsIConsoleReportCollector,
        nsID(0xdd98a481, 0xd2c4, 0x4203,
             [0x8d, 0xfa, 0x85, 0xbf, 0xd7, 0xdc, 0xd7, 0x05]));

// Must be kept in sync with nsIGlobalObject.h
nonidl!(nsIGlobalObject,
        nsID(0x11afa8be, 0xd997, 0x4e07,
             [0xa6, 0xa3, 0x6f, 0x87, 0x2e, 0xc3, 0xee, 0x7f]));

// Must be kept in sync with nsIScriptElement.h
nonidl!(nsIScriptElement,
        nsID(0xe60fca9b, 0x1b96, 0x4e4e,
             [0xa9, 0xb4, 0xdc, 0x98, 0x4f, 0x88, 0x3f, 0x9c]));

// Must be kept in sync with nsPIDOMWindow.h
nonidl!(nsPIDOMWindowOuter,
        nsID(0x769693d4, 0xb009, 0x4fe2,
             [0xaf, 0x18, 0x7d, 0xc8, 0xdf, 0x74, 0x96, 0xdf]));

// Must be kept in sync with nsPIDOMWindow.h
nonidl!(nsPIDOMWindowInner,
        nsID(0x775dabc9, 0x8f43, 0x4277,
             [0x9a, 0xdb, 0xf1, 0x99, 0x0d, 0x77, 0xcf, 0xfb]));

// Must be kept in sync with nsIScriptContext.h
nonidl!(nsIScriptContext,
        nsID(0x54cbe9cf, 0x7282, 0x421a,
             [0x91, 0x6f, 0xd0, 0x70, 0x73, 0xde, 0xb8, 0xc0]));

// Must be kept in sync with nsIScriptGlobalObject.h
nonidl!(nsIScriptGlobalObject,
        nsID(0x876f83bd, 0x6314, 0x460a,
             [0xa0, 0x45, 0x1c, 0x8f, 0x46, 0x2f, 0xb8, 0xe1]));

// Must be kept in sync with nsIScrollObserver.h
nonidl!(nsIScrollObserver,
        nsID(0xaa5026eb, 0x2f88, 0x4026,
             [0xa4, 0x6b, 0xf4, 0x59, 0x6b, 0x4e, 0xdf, 0x00]));

// Must be kept in sync with nsIWidget.h
nonidl!(nsIWidget,
        nsID(0x06396bf6, 0x2dd8, 0x45e5,
             [0xac, 0x45, 0x75, 0x26, 0x53, 0xb1, 0xc9, 0x80]));
