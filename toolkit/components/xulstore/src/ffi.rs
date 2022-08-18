/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate as XULStore;
use crate::{iter::XULStoreIterator, statics::update_profile_dir};
use libc::{c_char, c_void};
use nserror::{nsresult, NS_ERROR_NOT_IMPLEMENTED, NS_OK};
use nsstring::{nsAString, nsString};
use std::cell::RefCell;
use std::ptr;
use xpcom::{
    interfaces::{nsIJSEnumerator, nsIStringEnumerator, nsISupports, nsIXULStore},
    RefPtr,
};

#[no_mangle]
pub unsafe extern "C" fn xulstore_new_service(result: *mut *const nsIXULStore) {
    let xul_store_service = XULStoreService::new();
    RefPtr::new(xul_store_service.coerce::<nsIXULStore>()).forget(&mut *result);
}

#[xpcom(implement(nsIXULStore), atomic)]
pub struct XULStoreService {}

impl XULStoreService {
    fn new() -> RefPtr<XULStoreService> {
        XULStoreService::allocate(InitXULStoreService {})
    }

    #[allow(non_snake_case)]
    fn Persist(&self, _node: *const c_void, _attr: *const nsAString) -> nsresult {
        NS_ERROR_NOT_IMPLEMENTED
    }

    xpcom_method!(
        set_value => SetValue(
            doc: *const nsAString,
            id: *const nsAString,
            attr: *const nsAString,
            value: *const nsAString
        )
    );

    fn set_value(
        &self,
        doc: &nsAString,
        id: &nsAString,
        attr: &nsAString,
        value: &nsAString,
    ) -> Result<(), nsresult> {
        XULStore::set_value(doc, id, attr, value).map_err(|err| err.into())
    }

    xpcom_method!(
        has_value => HasValue(
            doc: *const nsAString,
            id: *const nsAString,
            attr: *const nsAString
        ) -> bool
    );

    fn has_value(
        &self,
        doc: &nsAString,
        id: &nsAString,
        attr: &nsAString,
    ) -> Result<bool, nsresult> {
        XULStore::has_value(doc, id, attr).map_err(|err| err.into())
    }

    xpcom_method!(
        get_value => GetValue(
            doc: *const nsAString,
            id: *const nsAString,
            attr: *const nsAString
        ) -> nsAString
    );

    fn get_value(
        &self,
        doc: &nsAString,
        id: &nsAString,
        attr: &nsAString,
    ) -> Result<nsString, nsresult> {
        match XULStore::get_value(doc, id, attr) {
            Ok(val) => Ok(nsString::from(&val)),
            Err(err) => Err(err.into()),
        }
    }

    xpcom_method!(
        remove_value => RemoveValue(
            doc: *const nsAString,
            id: *const nsAString,
            attr: *const nsAString
        )
    );

    fn remove_value(
        &self,
        doc: &nsAString,
        id: &nsAString,
        attr: &nsAString,
    ) -> Result<(), nsresult> {
        XULStore::remove_value(doc, id, attr).map_err(|err| err.into())
    }

    xpcom_method!(
        remove_document => RemoveDocument(doc: *const nsAString)
    );

    fn remove_document(&self, doc: &nsAString) -> Result<(), nsresult> {
        XULStore::remove_document(doc).map_err(|err| err.into())
    }

    xpcom_method!(
        get_ids_enumerator => GetIDsEnumerator(
            doc: *const nsAString
        ) -> * const nsIStringEnumerator
    );

    fn get_ids_enumerator(&self, doc: &nsAString) -> Result<RefPtr<nsIStringEnumerator>, nsresult> {
        match XULStore::get_ids(doc) {
            Ok(val) => {
                let enumerator = StringEnumerator::new(val);
                Ok(RefPtr::new(enumerator.coerce::<nsIStringEnumerator>()))
            }
            Err(err) => Err(err.into()),
        }
    }

    xpcom_method!(
        get_attribute_enumerator => GetAttributeEnumerator(
            doc: *const nsAString,
            id: *const nsAString
        ) -> * const nsIStringEnumerator
    );

    fn get_attribute_enumerator(
        &self,
        doc: &nsAString,
        id: &nsAString,
    ) -> Result<RefPtr<nsIStringEnumerator>, nsresult> {
        match XULStore::get_attrs(doc, id) {
            Ok(val) => {
                let enumerator = StringEnumerator::new(val);
                Ok(RefPtr::new(enumerator.coerce::<nsIStringEnumerator>()))
            }
            Err(err) => Err(err.into()),
        }
    }
}

#[xpcom(implement(nsIStringEnumerator), nonatomic)]
pub(crate) struct StringEnumerator {
    iter: RefCell<XULStoreIterator>,
}
impl StringEnumerator {
    pub(crate) fn new(iter: XULStoreIterator) -> RefPtr<StringEnumerator> {
        StringEnumerator::allocate(InitStringEnumerator {
            iter: RefCell::new(iter),
        })
    }

    xpcom_method!(string_iterator => StringIterator() -> *const nsIJSEnumerator);

    fn string_iterator(&self) -> Result<RefPtr<nsIJSEnumerator>, nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }

    xpcom_method!(has_more => HasMore() -> bool);

    fn has_more(&self) -> Result<bool, nsresult> {
        let iter = self.iter.borrow();
        Ok(iter.has_more())
    }

    xpcom_method!(get_next => GetNext() -> nsAString);

    fn get_next(&self) -> Result<nsString, nsresult> {
        let mut iter = self.iter.borrow_mut();
        match iter.get_next() {
            Ok(value) => Ok(nsString::from(&value)),
            Err(err) => Err(err.into()),
        }
    }
}

#[xpcom(implement(nsIObserver), nonatomic)]
pub(crate) struct ProfileChangeObserver {}
impl ProfileChangeObserver {
    #[allow(non_snake_case)]
    unsafe fn Observe(
        &self,
        _subject: *const nsISupports,
        _topic: *const c_char,
        _data: *const u16,
    ) -> nsresult {
        update_profile_dir();
        NS_OK
    }

    pub(crate) fn new() -> RefPtr<ProfileChangeObserver> {
        ProfileChangeObserver::allocate(InitProfileChangeObserver {})
    }
}

#[no_mangle]
pub unsafe extern "C" fn xulstore_set_value(
    doc: &nsAString,
    id: &nsAString,
    attr: &nsAString,
    value: &nsAString,
) -> nsresult {
    XULStore::set_value(doc, id, attr, value).into()
}

#[no_mangle]
pub unsafe extern "C" fn xulstore_has_value(
    doc: &nsAString,
    id: &nsAString,
    attr: &nsAString,
    has_value: *mut bool,
) -> nsresult {
    match XULStore::has_value(doc, id, attr) {
        Ok(val) => {
            *has_value = val;
            NS_OK
        }
        Err(err) => err.into(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn xulstore_get_value(
    doc: &nsAString,
    id: &nsAString,
    attr: &nsAString,
    value: *mut nsAString,
) -> nsresult {
    match XULStore::get_value(doc, id, attr) {
        Ok(val) => {
            (*value).assign(&nsString::from(&val));
            NS_OK
        }
        Err(err) => err.into(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn xulstore_remove_value(
    doc: &nsAString,
    id: &nsAString,
    attr: &nsAString,
) -> nsresult {
    XULStore::remove_value(doc, id, attr).into()
}

#[no_mangle]
pub unsafe extern "C" fn xulstore_get_ids(
    doc: &nsAString,
    result: *mut nsresult,
) -> *mut XULStoreIterator {
    match XULStore::get_ids(doc) {
        Ok(iter) => {
            *result = NS_OK;
            Box::into_raw(Box::new(iter))
        }
        Err(err) => {
            *result = err.into();
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn xulstore_get_attrs(
    doc: &nsAString,
    id: &nsAString,
    result: *mut nsresult,
) -> *mut XULStoreIterator {
    match XULStore::get_attrs(doc, id) {
        Ok(iter) => {
            *result = NS_OK;
            Box::into_raw(Box::new(iter))
        }
        Err(err) => {
            *result = err.into();
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn xulstore_iter_has_more(iter: &XULStoreIterator) -> bool {
    iter.has_more()
}

#[no_mangle]
pub unsafe extern "C" fn xulstore_iter_get_next(
    iter: &mut XULStoreIterator,
    value: *mut nsAString,
) -> nsresult {
    match iter.get_next() {
        Ok(val) => {
            (*value).assign(&nsString::from(&val));
            NS_OK
        }
        Err(err) => err.into(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn xulstore_iter_free(iter: *mut XULStoreIterator) {
    drop(Box::from_raw(iter));
}

#[no_mangle]
pub unsafe extern "C" fn xulstore_shutdown() -> nsresult {
    match XULStore::shutdown() {
        Ok(()) => NS_OK,
        Err(err) => err.into(),
    }
}
