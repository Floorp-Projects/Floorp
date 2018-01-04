/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use {
    RefCounted,
    RefPtr,
    GetterAddrefs
};
use interfaces::nsISupports;
use nserror::NsresultExt;

#[repr(C)]
#[derive(Copy, Clone, Eq, PartialEq)]
/// A "unique identifier". This is modeled after OSF DCE UUIDs.
pub struct nsID(pub u32, pub u16, pub u16, pub [u8; 8]);

/// Interface IDs
pub type nsIID = nsID;
/// Class IDs
pub type nsCID = nsID;

/// A type which implements XpCom must follow the following rules:
///
/// * It must be a legal XPCOM interface.
/// * The result of a QueryInterface or similar call, passing IID, must return a
///   valid reference to an object of the given type.
/// * It must be valid to cast a &self reference to a &nsISupports reference.
pub unsafe trait XpCom : RefCounted {
    const IID: nsIID;

    /// Perform a QueryInterface call on this object, attempting to dynamically
    /// cast it to the requested interface type. Returns Some(RefPtr<T>) if the
    /// cast succeeded, and None otherwise.
    fn query_interface<T: XpCom>(&self) -> Option<RefPtr<T>> {
        let mut ga = GetterAddrefs::<T>::new();
        unsafe {
            if (*(self as *const Self as *const nsISupports)).QueryInterface(
                &T::IID,
                ga.void_ptr(),
            ).succeeded() {
                ga.refptr()
            } else {
                None
            }
        }
    }
}
