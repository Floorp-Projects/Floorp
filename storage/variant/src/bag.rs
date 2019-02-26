/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use nserror::{nsresult, NS_OK};
use nsstring::nsString;
use xpcom::{getter_addrefs, interfaces::nsIWritablePropertyBag, RefPtr};

use crate::VariantType;

extern "C" {
    fn NS_NewHashPropertyBag(bag: *mut *const nsIWritablePropertyBag) -> libc::c_void;
}

/// A hash property bag backed by storage variant values.
pub struct HashPropertyBag(RefPtr<nsIWritablePropertyBag>);

impl Default for HashPropertyBag {
    fn default() -> HashPropertyBag {
        // This is safe to unwrap because `NS_NewHashPropertyBag` is infallible.
        let bag = getter_addrefs(|p| {
            unsafe { NS_NewHashPropertyBag(p) };
            NS_OK
        }).unwrap();
        HashPropertyBag(bag)
    }
}

impl HashPropertyBag {
    /// Creates an empty property bag.
    #[inline]
    pub fn new() -> HashPropertyBag {
        HashPropertyBag::default()
    }

    /// Returns the value for a property name. Fails with `NS_ERROR_FAILURE`
    /// if the property doesn't exist, or `NS_ERROR_CANNOT_CONVERT_DATA` if the
    /// property exists, but is not of the value type `V`.
    pub fn get<K, V>(&self, name: K) -> Result<V, nsresult>
    where
        K: AsRef<str>,
        V: VariantType,
    {
        getter_addrefs(|p| unsafe { self.0.GetProperty(&*nsString::from(name.as_ref()), p) })
            .and_then(|v| V::from_variant(v.coerce()))
    }

    /// Returns the value for a property name, or the default if not set or
    /// not of the value type `V`.
    #[inline]
    pub fn get_or_default<K, V>(&self, name: K) -> V
    where
        K: AsRef<str>,
        V: VariantType + Default,
    {
        self.get(name).unwrap_or_default()
    }

    /// Sets a property with the name to the value, overwriting any previous
    /// value.
    pub fn set<K, V>(&mut self, name: K, value: V)
    where
        K: AsRef<str>,
        V: VariantType,
    {
        let v = value.into_variant();
        unsafe {
            // This is safe to unwrap because
            // `nsHashPropertyBagBase::SetProperty` only returns an error if `v`
            // is a null pointer.
            self.0
                .SetProperty(&*nsString::from(name.as_ref()), v.coerce())
                .to_result()
                .unwrap()
        }
    }

    /// Deletes a property with the name. Returns `true` if the property
    /// was previously in the bag, `false` if not.
    pub fn delete(&mut self, name: impl AsRef<str>) -> bool {
        unsafe {
            self.0
                .DeleteProperty(&*nsString::from(name.as_ref()))
                .to_result()
                .is_ok()
        }
    }

    /// Returns a reference to the backing `nsIWritablePropertyBag`.
    #[inline]
    pub fn bag(&self) -> &nsIWritablePropertyBag {
        &self.0
    }
}
