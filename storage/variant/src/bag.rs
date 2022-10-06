/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use nserror::{nsresult, NS_ERROR_CANNOT_CONVERT_DATA, NS_OK};
use nsstring::nsString;
use xpcom::{
    getter_addrefs,
    interfaces::{nsIProperty, nsIPropertyBag, nsIWritablePropertyBag},
    RefPtr, XpCom,
};

use crate::{NsIVariantExt, VariantType};

extern "C" {
    fn NS_NewHashPropertyBag(bag: *mut *const nsIWritablePropertyBag);
}

/// A hash property bag backed by storage variant values.
pub struct HashPropertyBag(RefPtr<nsIWritablePropertyBag>);

// This is safe as long as our `nsIWritablePropertyBag` is an instance of
// `mozilla::nsHashPropertyBag`, which is atomically reference counted, and
// all properties are backed by `Storage*Variant`s, all of which are
// thread-safe.
unsafe impl Send for HashPropertyBag {}
unsafe impl Sync for HashPropertyBag {}

impl Default for HashPropertyBag {
    fn default() -> HashPropertyBag {
        // This is safe to unwrap because `NS_NewHashPropertyBag` is infallible.
        let bag = getter_addrefs(|p| {
            unsafe { NS_NewHashPropertyBag(p) };
            NS_OK
        })
        .unwrap();
        HashPropertyBag(bag)
    }
}

impl HashPropertyBag {
    /// Creates an empty property bag.
    #[inline]
    pub fn new() -> Self {
        Self::default()
    }

    /// Creates a property bag from an instance of `nsIPropertyBag`, cloning its
    /// contents. The `source` bag can only contain primitive values for which
    /// the `VariantType` trait is implemented. Attempting to clone a bag with
    /// unsupported types, such as arrays, interface pointers, and `jsval`s,
    /// fails with `NS_ERROR_CANNOT_CONVERT_DATA`.
    ///
    /// `clone_from_bag` can be used to clone a thread-unsafe `nsIPropertyBag`,
    /// like one passed from JavaScript via XPConnect, into one that can be
    /// shared across threads.
    pub fn clone_from_bag(source: &nsIPropertyBag) -> Result<Self, nsresult> {
        let enumerator = getter_addrefs(|p| unsafe { source.GetEnumerator(p) })?;
        let b = HashPropertyBag::new();
        while {
            let mut has_more = false;
            unsafe { enumerator.HasMoreElements(&mut has_more) }.to_result()?;
            has_more
        } {
            let element = getter_addrefs(|p| unsafe { enumerator.GetNext(p) })?;
            let property = element
                .query_interface::<nsIProperty>()
                .ok_or(NS_ERROR_CANNOT_CONVERT_DATA)?;
            let mut name = nsString::new();
            unsafe { property.GetName(&mut *name) }.to_result()?;
            let value = getter_addrefs(|p| unsafe { property.GetValue(p) })?;
            unsafe { b.0.SetProperty(&*name, value.try_clone()?.coerce()) }.to_result()?;
        }
        Ok(b)
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
