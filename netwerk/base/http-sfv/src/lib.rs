/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use nserror::{nsresult, NS_ERROR_FAILURE, NS_ERROR_NULL_POINTER, NS_ERROR_UNEXPECTED, NS_OK};
use nsstring::{nsACString, nsCString};
use sfv::Parser;
use sfv::SerializeValue;
use sfv::{
    BareItem, Decimal, Dictionary, FromPrimitive, InnerList, Item, List, ListEntry, Parameters,
    ParseMore,
};
use std::cell::RefCell;
use std::ops::Deref;
use thin_vec::ThinVec;
use xpcom::interfaces::{
    nsISFVBareItem, nsISFVBool, nsISFVByteSeq, nsISFVDecimal, nsISFVDictionary, nsISFVInnerList,
    nsISFVInteger, nsISFVItem, nsISFVItemOrInnerList, nsISFVList, nsISFVParams, nsISFVService,
    nsISFVString, nsISFVToken,
};
use xpcom::{xpcom, xpcom_method, RefPtr, XpCom};

#[no_mangle]
pub unsafe extern "C" fn new_sfv_service(result: *mut *const nsISFVService) {
    let service: RefPtr<SFVService> = SFVService::new();
    RefPtr::new(service.coerce::<nsISFVService>()).forget(&mut *result);
}

#[xpcom(implement(nsISFVService), atomic)]
struct SFVService {}

impl SFVService {
    fn new() -> RefPtr<SFVService> {
        SFVService::allocate(InitSFVService {})
    }

    xpcom_method!(parse_dictionary => ParseDictionary(header: *const nsACString) -> *const nsISFVDictionary);
    fn parse_dictionary(&self, header: &nsACString) -> Result<RefPtr<nsISFVDictionary>, nsresult> {
        let parsed_dict = Parser::parse_dictionary(&header).map_err(|_| NS_ERROR_FAILURE)?;
        let sfv_dict = SFVDictionary::new();
        sfv_dict.value.replace(parsed_dict);
        Ok(RefPtr::new(sfv_dict.coerce::<nsISFVDictionary>()))
    }

    xpcom_method!(parse_list => ParseList(field_value: *const nsACString) -> *const nsISFVList);
    fn parse_list(&self, header: &nsACString) -> Result<RefPtr<nsISFVList>, nsresult> {
        let parsed_list = Parser::parse_list(&header).map_err(|_| NS_ERROR_FAILURE)?;

        let mut nsi_members = Vec::new();
        for item_or_inner_list in parsed_list.iter() {
            nsi_members.push(interface_from_list_entry(item_or_inner_list)?)
        }
        let sfv_list = SFVList::allocate(InitSFVList {
            members: RefCell::new(nsi_members),
        });
        Ok(RefPtr::new(sfv_list.coerce::<nsISFVList>()))
    }

    xpcom_method!(parse_item => ParseItem(header: *const nsACString) -> *const nsISFVItem);
    fn parse_item(&self, header: &nsACString) -> Result<RefPtr<nsISFVItem>, nsresult> {
        let parsed_item = Parser::parse_item(&header).map_err(|_| NS_ERROR_FAILURE)?;
        interface_from_item(&parsed_item)
    }

    xpcom_method!(new_integer => NewInteger(value: i64) -> *const nsISFVInteger);
    fn new_integer(&self, value: i64) -> Result<RefPtr<nsISFVInteger>, nsresult> {
        Ok(RefPtr::new(
            SFVInteger::new(value).coerce::<nsISFVInteger>(),
        ))
    }

    xpcom_method!(new_decimal => NewDecimal(value: f64) -> *const nsISFVDecimal);
    fn new_decimal(&self, value: f64) -> Result<RefPtr<nsISFVDecimal>, nsresult> {
        Ok(RefPtr::new(
            SFVDecimal::new(value).coerce::<nsISFVDecimal>(),
        ))
    }

    xpcom_method!(new_bool => NewBool(value: bool) -> *const nsISFVBool);
    fn new_bool(&self, value: bool) -> Result<RefPtr<nsISFVBool>, nsresult> {
        Ok(RefPtr::new(SFVBool::new(value).coerce::<nsISFVBool>()))
    }

    xpcom_method!(new_string => NewString(value: *const nsACString) -> *const nsISFVString);
    fn new_string(&self, value: &nsACString) -> Result<RefPtr<nsISFVString>, nsresult> {
        Ok(RefPtr::new(SFVString::new(value).coerce::<nsISFVString>()))
    }

    xpcom_method!(new_token => NewToken(value: *const nsACString) -> *const nsISFVToken);
    fn new_token(&self, value: &nsACString) -> Result<RefPtr<nsISFVToken>, nsresult> {
        Ok(RefPtr::new(SFVToken::new(value).coerce::<nsISFVToken>()))
    }

    xpcom_method!(new_byte_sequence => NewByteSequence(value: *const nsACString) -> *const nsISFVByteSeq);
    fn new_byte_sequence(&self, value: &nsACString) -> Result<RefPtr<nsISFVByteSeq>, nsresult> {
        Ok(RefPtr::new(
            SFVByteSeq::new(value).coerce::<nsISFVByteSeq>(),
        ))
    }

    xpcom_method!(new_parameters => NewParameters() -> *const nsISFVParams);
    fn new_parameters(&self) -> Result<RefPtr<nsISFVParams>, nsresult> {
        Ok(RefPtr::new(SFVParams::new().coerce::<nsISFVParams>()))
    }

    xpcom_method!(new_item => NewItem(value: *const nsISFVBareItem, params:  *const nsISFVParams) -> *const nsISFVItem);
    fn new_item(
        &self,
        value: &nsISFVBareItem,
        params: &nsISFVParams,
    ) -> Result<RefPtr<nsISFVItem>, nsresult> {
        Ok(RefPtr::new(
            SFVItem::new(value, params).coerce::<nsISFVItem>(),
        ))
    }

    xpcom_method!(new_inner_list => NewInnerList(items: *const thin_vec::ThinVec<Option<RefPtr<nsISFVItem>>>, params:  *const nsISFVParams) -> *const nsISFVInnerList);
    fn new_inner_list(
        &self,
        items: &thin_vec::ThinVec<Option<RefPtr<nsISFVItem>>>,
        params: &nsISFVParams,
    ) -> Result<RefPtr<nsISFVInnerList>, nsresult> {
        let items = items
            .iter()
            .cloned()
            .map(|item| item.ok_or(NS_ERROR_NULL_POINTER))
            .collect::<Result<Vec<_>, nsresult>>()?;
        Ok(RefPtr::new(
            SFVInnerList::new(items, params).coerce::<nsISFVInnerList>(),
        ))
    }

    xpcom_method!(new_list => NewList(members: *const thin_vec::ThinVec<Option<RefPtr<nsISFVItemOrInnerList>>>) -> *const nsISFVList);
    fn new_list(
        &self,
        members: &thin_vec::ThinVec<Option<RefPtr<nsISFVItemOrInnerList>>>,
    ) -> Result<RefPtr<nsISFVList>, nsresult> {
        let members = members
            .iter()
            .cloned()
            .map(|item| item.ok_or(NS_ERROR_NULL_POINTER))
            .collect::<Result<Vec<_>, nsresult>>()?;
        Ok(RefPtr::new(SFVList::new(members).coerce::<nsISFVList>()))
    }

    xpcom_method!(new_dictionary => NewDictionary() -> *const nsISFVDictionary);
    fn new_dictionary(&self) -> Result<RefPtr<nsISFVDictionary>, nsresult> {
        Ok(RefPtr::new(
            SFVDictionary::new().coerce::<nsISFVDictionary>(),
        ))
    }
}

#[xpcom(implement(nsISFVInteger, nsISFVBareItem), nonatomic)]
struct SFVInteger {
    value: RefCell<i64>,
}

impl SFVInteger {
    fn new(value: i64) -> RefPtr<SFVInteger> {
        SFVInteger::allocate(InitSFVInteger {
            value: RefCell::new(value),
        })
    }

    xpcom_method!(get_value => GetValue() -> i64);
    fn get_value(&self) -> Result<i64, nsresult> {
        Ok(*self.value.borrow())
    }

    xpcom_method!(set_value => SetValue(value: i64));
    fn set_value(&self, value: i64) -> Result<(), nsresult> {
        self.value.replace(value);
        Ok(())
    }

    xpcom_method!(get_type => GetType() -> i32);
    fn get_type(&self) -> Result<i32, nsresult> {
        Ok(nsISFVBareItem::INTEGER)
    }

    fn from_bare_item_interface(obj: &nsISFVBareItem) -> &Self {
        unsafe { ::std::mem::transmute(obj) }
    }
}

#[xpcom(implement(nsISFVBool, nsISFVBareItem), nonatomic)]
struct SFVBool {
    value: RefCell<bool>,
}

impl SFVBool {
    fn new(value: bool) -> RefPtr<SFVBool> {
        SFVBool::allocate(InitSFVBool {
            value: RefCell::new(value),
        })
    }

    xpcom_method!(get_value => GetValue() -> bool);
    fn get_value(&self) -> Result<bool, nsresult> {
        Ok(*self.value.borrow())
    }

    xpcom_method!(set_value => SetValue(value: bool));
    fn set_value(&self, value: bool) -> Result<(), nsresult> {
        self.value.replace(value);
        Ok(())
    }

    xpcom_method!(get_type => GetType() -> i32);
    fn get_type(&self) -> Result<i32, nsresult> {
        Ok(nsISFVBareItem::BOOL)
    }

    fn from_bare_item_interface(obj: &nsISFVBareItem) -> &Self {
        unsafe { ::std::mem::transmute(obj) }
    }
}

#[xpcom(implement(nsISFVString, nsISFVBareItem), nonatomic)]
struct SFVString {
    value: RefCell<nsCString>,
}

impl SFVString {
    fn new(value: &nsACString) -> RefPtr<SFVString> {
        SFVString::allocate(InitSFVString {
            value: RefCell::new(nsCString::from(value)),
        })
    }

    xpcom_method!(
        get_value => GetValue(
        ) -> nsACString
    );

    fn get_value(&self) -> Result<nsCString, nsresult> {
        Ok(self.value.borrow().clone())
    }

    xpcom_method!(
        set_value => SetValue(value: *const nsACString)
    );

    fn set_value(&self, value: &nsACString) -> Result<(), nsresult> {
        self.value.borrow_mut().assign(value);
        Ok(())
    }

    xpcom_method!(get_type => GetType() -> i32);
    fn get_type(&self) -> Result<i32, nsresult> {
        Ok(nsISFVBareItem::STRING)
    }

    fn from_bare_item_interface(obj: &nsISFVBareItem) -> &Self {
        unsafe { ::std::mem::transmute(obj) }
    }
}

#[xpcom(implement(nsISFVToken, nsISFVBareItem), nonatomic)]
struct SFVToken {
    value: RefCell<nsCString>,
}

impl SFVToken {
    fn new(value: &nsACString) -> RefPtr<SFVToken> {
        SFVToken::allocate(InitSFVToken {
            value: RefCell::new(nsCString::from(value)),
        })
    }

    xpcom_method!(
        get_value => GetValue(
        ) -> nsACString
    );

    fn get_value(&self) -> Result<nsCString, nsresult> {
        Ok(self.value.borrow().clone())
    }

    xpcom_method!(
        set_value => SetValue(value: *const nsACString)
    );

    fn set_value(&self, value: &nsACString) -> Result<(), nsresult> {
        self.value.borrow_mut().assign(value);
        Ok(())
    }

    xpcom_method!(get_type => GetType() -> i32);
    fn get_type(&self) -> Result<i32, nsresult> {
        Ok(nsISFVBareItem::TOKEN)
    }

    fn from_bare_item_interface(obj: &nsISFVBareItem) -> &Self {
        unsafe { ::std::mem::transmute(obj) }
    }
}

#[xpcom(implement(nsISFVByteSeq, nsISFVBareItem), nonatomic)]
struct SFVByteSeq {
    value: RefCell<nsCString>,
}

impl SFVByteSeq {
    fn new(value: &nsACString) -> RefPtr<SFVByteSeq> {
        SFVByteSeq::allocate(InitSFVByteSeq {
            value: RefCell::new(nsCString::from(value)),
        })
    }

    xpcom_method!(
        get_value => GetValue(
        ) -> nsACString
    );

    fn get_value(&self) -> Result<nsCString, nsresult> {
        Ok(self.value.borrow().clone())
    }

    xpcom_method!(
        set_value => SetValue(value: *const nsACString)
    );

    fn set_value(&self, value: &nsACString) -> Result<(), nsresult> {
        self.value.borrow_mut().assign(value);
        Ok(())
    }

    xpcom_method!(get_type => GetType() -> i32);
    fn get_type(&self) -> Result<i32, nsresult> {
        Ok(nsISFVBareItem::BYTE_SEQUENCE)
    }

    fn from_bare_item_interface(obj: &nsISFVBareItem) -> &Self {
        unsafe { ::std::mem::transmute(obj) }
    }
}

#[xpcom(implement(nsISFVDecimal, nsISFVBareItem), nonatomic)]
struct SFVDecimal {
    value: RefCell<f64>,
}

impl SFVDecimal {
    fn new(value: f64) -> RefPtr<SFVDecimal> {
        SFVDecimal::allocate(InitSFVDecimal {
            value: RefCell::new(value),
        })
    }

    xpcom_method!(
        get_value => GetValue(
        ) -> f64
    );

    fn get_value(&self) -> Result<f64, nsresult> {
        Ok(*self.value.borrow())
    }

    xpcom_method!(
        set_value => SetValue(value: f64)
    );

    fn set_value(&self, value: f64) -> Result<(), nsresult> {
        self.value.replace(value);
        Ok(())
    }

    xpcom_method!(get_type => GetType() -> i32);
    fn get_type(&self) -> Result<i32, nsresult> {
        Ok(nsISFVBareItem::DECIMAL)
    }

    fn from_bare_item_interface(obj: &nsISFVBareItem) -> &Self {
        unsafe { ::std::mem::transmute(obj) }
    }
}

#[xpcom(implement(nsISFVParams), nonatomic)]
struct SFVParams {
    params: RefCell<Parameters>,
}

impl SFVParams {
    fn new() -> RefPtr<SFVParams> {
        SFVParams::allocate(InitSFVParams {
            params: RefCell::new(Parameters::new()),
        })
    }

    xpcom_method!(
        get => Get(key: *const nsACString) -> *const nsISFVBareItem
    );

    fn get(&self, key: &nsACString) -> Result<RefPtr<nsISFVBareItem>, nsresult> {
        let key = key.to_utf8();
        let params = self.params.borrow();
        let param_val = params.get(key.as_ref());

        match param_val {
            Some(val) => interface_from_bare_item(val),
            None => return Err(NS_ERROR_UNEXPECTED),
        }
    }

    xpcom_method!(
        set => Set(key: *const nsACString, item: *const nsISFVBareItem)
    );

    fn set(&self, key: &nsACString, item: &nsISFVBareItem) -> Result<(), nsresult> {
        let key = key.to_utf8().into_owned();
        let bare_item = bare_item_from_interface(item)?;
        self.params.borrow_mut().insert(key, bare_item);
        Ok(())
    }

    xpcom_method!(
        delete => Delete(key: *const nsACString)
    );
    fn delete(&self, key: &nsACString) -> Result<(), nsresult> {
        let key = key.to_utf8();
        let mut params = self.params.borrow_mut();

        if !params.contains_key(key.as_ref()) {
            return Err(NS_ERROR_UNEXPECTED);
        }

        // Keeps only entries that don't match key
        params.retain(|k, _| k != key.as_ref());
        Ok(())
    }

    xpcom_method!(
        keys => Keys() -> thin_vec::ThinVec<nsCString>
    );
    fn keys(&self) -> Result<thin_vec::ThinVec<nsCString>, nsresult> {
        let keys = self.params.borrow();
        let keys = keys
            .keys()
            .map(nsCString::from)
            .collect::<ThinVec<nsCString>>();
        Ok(keys)
    }

    fn from_interface(obj: &nsISFVParams) -> &Self {
        unsafe { ::std::mem::transmute(obj) }
    }
}

#[xpcom(implement(nsISFVItem, nsISFVItemOrInnerList), nonatomic)]
struct SFVItem {
    value: RefPtr<nsISFVBareItem>,
    params: RefPtr<nsISFVParams>,
}

impl SFVItem {
    fn new(value: &nsISFVBareItem, params: &nsISFVParams) -> RefPtr<SFVItem> {
        SFVItem::allocate(InitSFVItem {
            value: RefPtr::new(value),
            params: RefPtr::new(params),
        })
    }

    xpcom_method!(
        get_value => GetValue(
        ) -> *const nsISFVBareItem
    );

    fn get_value(&self) -> Result<RefPtr<nsISFVBareItem>, nsresult> {
        Ok(self.value.clone())
    }

    xpcom_method!(
        get_params => GetParams(
        ) -> *const nsISFVParams
    );
    fn get_params(&self) -> Result<RefPtr<nsISFVParams>, nsresult> {
        Ok(self.params.clone())
    }

    xpcom_method!(
        serialize => Serialize() -> nsACString
    );
    fn serialize(&self) -> Result<nsCString, nsresult> {
        let bare_item = bare_item_from_interface(self.value.deref())?;
        let params = params_from_interface(self.params.deref())?;
        let serialized = Item::with_params(bare_item, params)
            .serialize_value()
            .map_err(|_| NS_ERROR_FAILURE)?;
        Ok(nsCString::from(serialized))
    }

    fn from_interface(obj: &nsISFVItem) -> &Self {
        unsafe { ::std::mem::transmute(obj) }
    }
}

#[xpcom(implement(nsISFVInnerList, nsISFVItemOrInnerList), nonatomic)]
struct SFVInnerList {
    items: RefCell<Vec<RefPtr<nsISFVItem>>>,
    params: RefPtr<nsISFVParams>,
}

impl SFVInnerList {
    fn new(items: Vec<RefPtr<nsISFVItem>>, params: &nsISFVParams) -> RefPtr<SFVInnerList> {
        SFVInnerList::allocate(InitSFVInnerList {
            items: RefCell::new(items),
            params: RefPtr::new(params),
        })
    }

    xpcom_method!(
        get_items => GetItems() -> thin_vec::ThinVec<Option<RefPtr<nsISFVItem>>>
    );

    fn get_items(&self) -> Result<thin_vec::ThinVec<Option<RefPtr<nsISFVItem>>>, nsresult> {
        let items = self.items.borrow().iter().cloned().map(Some).collect();
        Ok(items)
    }

    #[allow(non_snake_case)]
    unsafe fn SetItems(
        &self,
        value: *const thin_vec::ThinVec<Option<RefPtr<nsISFVItem>>>,
    ) -> nsresult {
        if value.is_null() {
            return NS_ERROR_NULL_POINTER;
        }
        match (*value)
            .iter()
            .map(|v| v.clone().ok_or(NS_ERROR_NULL_POINTER))
            .collect::<Result<Vec<_>, nsresult>>()
        {
            Ok(value) => *self.items.borrow_mut() = value,
            Err(rv) => return rv,
        }
        NS_OK
    }

    xpcom_method!(
        get_params => GetParams(
        ) -> *const nsISFVParams
    );
    fn get_params(&self) -> Result<RefPtr<nsISFVParams>, nsresult> {
        Ok(self.params.clone())
    }

    fn from_interface(obj: &nsISFVInnerList) -> &Self {
        unsafe { ::std::mem::transmute(obj) }
    }
}

#[xpcom(implement(nsISFVList, nsISFVSerialize), nonatomic)]
struct SFVList {
    members: RefCell<Vec<RefPtr<nsISFVItemOrInnerList>>>,
}

impl SFVList {
    fn new(members: Vec<RefPtr<nsISFVItemOrInnerList>>) -> RefPtr<SFVList> {
        SFVList::allocate(InitSFVList {
            members: RefCell::new(members),
        })
    }

    xpcom_method!(
        get_members => GetMembers() -> thin_vec::ThinVec<Option<RefPtr<nsISFVItemOrInnerList>>>
    );

    fn get_members(
        &self,
    ) -> Result<thin_vec::ThinVec<Option<RefPtr<nsISFVItemOrInnerList>>>, nsresult> {
        Ok(self.members.borrow().iter().cloned().map(Some).collect())
    }

    #[allow(non_snake_case)]
    unsafe fn SetMembers(
        &self,
        value: *const thin_vec::ThinVec<Option<RefPtr<nsISFVItemOrInnerList>>>,
    ) -> nsresult {
        if value.is_null() {
            return NS_ERROR_NULL_POINTER;
        }
        match (*value)
            .iter()
            .map(|v| v.clone().ok_or(NS_ERROR_NULL_POINTER))
            .collect::<Result<Vec<_>, nsresult>>()
        {
            Ok(value) => *self.members.borrow_mut() = value,
            Err(rv) => return rv,
        }
        NS_OK
    }

    xpcom_method!(
        parse_more => ParseMore(header: *const nsACString)
    );
    fn parse_more(&self, header: &nsACString) -> Result<(), nsresult> {
        // create List from SFVList to call parse_more on it
        let mut list = List::new();
        let members = self.members.borrow().clone();
        for interface_entry in members.iter() {
            let item_or_inner_list = list_entry_from_interface(interface_entry)?;
            list.push(item_or_inner_list);
        }

        let _ = list.parse_more(&header).map_err(|_| NS_ERROR_FAILURE)?;

        // replace SFVList's members with new_members
        let mut new_members = Vec::new();
        for item_or_inner_list in list.iter() {
            new_members.push(interface_from_list_entry(item_or_inner_list)?)
        }
        self.members.replace(new_members);
        Ok(())
    }

    xpcom_method!(
        serialize => Serialize() -> nsACString
    );
    fn serialize(&self) -> Result<nsCString, nsresult> {
        let mut list = List::new();
        let members = self.members.borrow().clone();
        for interface_entry in members.iter() {
            let item_or_inner_list = list_entry_from_interface(interface_entry)?;
            list.push(item_or_inner_list);
        }

        let serialized = list.serialize_value().map_err(|_| NS_ERROR_FAILURE)?;
        Ok(nsCString::from(serialized))
    }
}

#[xpcom(implement(nsISFVDictionary, nsISFVSerialize), nonatomic)]
struct SFVDictionary {
    value: RefCell<Dictionary>,
}

impl SFVDictionary {
    fn new() -> RefPtr<SFVDictionary> {
        SFVDictionary::allocate(InitSFVDictionary {
            value: RefCell::new(Dictionary::new()),
        })
    }

    xpcom_method!(
        get => Get(key: *const nsACString) -> *const nsISFVItemOrInnerList
    );

    fn get(&self, key: &nsACString) -> Result<RefPtr<nsISFVItemOrInnerList>, nsresult> {
        let key = key.to_utf8();
        let value = self.value.borrow();
        let member_value = value.get(key.as_ref());

        match member_value {
            Some(member) => interface_from_list_entry(member),
            None => return Err(NS_ERROR_UNEXPECTED),
        }
    }

    xpcom_method!(
        set => Set(key: *const nsACString, item: *const nsISFVItemOrInnerList)
    );

    fn set(&self, key: &nsACString, member_value: &nsISFVItemOrInnerList) -> Result<(), nsresult> {
        let key = key.to_utf8().into_owned();
        let value = list_entry_from_interface(member_value)?;
        self.value.borrow_mut().insert(key, value);
        Ok(())
    }

    xpcom_method!(
        delete => Delete(key: *const nsACString)
    );

    fn delete(&self, key: &nsACString) -> Result<(), nsresult> {
        let key = key.to_utf8();
        let mut params = self.value.borrow_mut();

        if !params.contains_key(key.as_ref()) {
            return Err(NS_ERROR_UNEXPECTED);
        }

        // Keeps only entries that don't match key
        params.retain(|k, _| k != key.as_ref());
        Ok(())
    }

    xpcom_method!(
        keys => Keys() -> thin_vec::ThinVec<nsCString>
    );
    fn keys(&self) -> Result<thin_vec::ThinVec<nsCString>, nsresult> {
        let members = self.value.borrow();
        let keys = members
            .keys()
            .map(nsCString::from)
            .collect::<ThinVec<nsCString>>();
        Ok(keys)
    }

    xpcom_method!(
        parse_more => ParseMore(header: *const nsACString)
    );
    fn parse_more(&self, header: &nsACString) -> Result<(), nsresult> {
        let _ = self
            .value
            .borrow_mut()
            .parse_more(&header)
            .map_err(|_| NS_ERROR_FAILURE)?;
        Ok(())
    }

    xpcom_method!(
        serialize => Serialize() -> nsACString
    );
    fn serialize(&self) -> Result<nsCString, nsresult> {
        let serialized = self
            .value
            .borrow()
            .serialize_value()
            .map_err(|_| NS_ERROR_FAILURE)?;
        Ok(nsCString::from(serialized))
    }
}

fn bare_item_from_interface(obj: &nsISFVBareItem) -> Result<BareItem, nsresult> {
    let obj = obj
        .query_interface::<nsISFVBareItem>()
        .ok_or(NS_ERROR_UNEXPECTED)?;
    let mut obj_type: i32 = -1;
    unsafe {
        obj.deref().GetType(&mut obj_type);
    }

    match obj_type {
        nsISFVBareItem::BOOL => {
            let item_value = SFVBool::from_bare_item_interface(obj.deref()).get_value()?;
            Ok(BareItem::Boolean(item_value))
        }
        nsISFVBareItem::STRING => {
            let string_itm = SFVString::from_bare_item_interface(obj.deref()).get_value()?;
            let item_value = (*string_itm.to_utf8()).to_string();
            Ok(BareItem::String(item_value))
        }
        nsISFVBareItem::TOKEN => {
            let token_itm = SFVToken::from_bare_item_interface(obj.deref()).get_value()?;
            let item_value = (*token_itm.to_utf8()).to_string();
            Ok(BareItem::Token(item_value))
        }
        nsISFVBareItem::INTEGER => {
            let item_value = SFVInteger::from_bare_item_interface(obj.deref()).get_value()?;
            Ok(BareItem::Integer(item_value))
        }
        nsISFVBareItem::DECIMAL => {
            let item_value = SFVDecimal::from_bare_item_interface(obj.deref()).get_value()?;
            let decimal: Decimal = Decimal::from_f64(item_value).ok_or(NS_ERROR_UNEXPECTED)?;
            Ok(BareItem::Decimal(decimal))
        }
        nsISFVBareItem::BYTE_SEQUENCE => {
            let token_itm = SFVByteSeq::from_bare_item_interface(obj.deref()).get_value()?;
            let item_value: String = (*token_itm.to_utf8()).to_string();
            Ok(BareItem::ByteSeq(item_value.into_bytes()))
        }
        _ => return Err(NS_ERROR_UNEXPECTED),
    }
}

fn params_from_interface(obj: &nsISFVParams) -> Result<Parameters, nsresult> {
    let params = SFVParams::from_interface(obj).params.borrow();
    Ok(params.clone())
}

fn item_from_interface(obj: &nsISFVItem) -> Result<Item, nsresult> {
    let sfv_item = SFVItem::from_interface(obj);
    let bare_item = bare_item_from_interface(sfv_item.value.deref())?;
    let parameters = params_from_interface(sfv_item.params.deref())?;
    Ok(Item::with_params(bare_item, parameters))
}

fn inner_list_from_interface(obj: &nsISFVInnerList) -> Result<InnerList, nsresult> {
    let sfv_inner_list = SFVInnerList::from_interface(obj);

    let mut inner_list_items: Vec<Item> = vec![];
    for item in sfv_inner_list.items.borrow().iter() {
        let item = item_from_interface(item)?;
        inner_list_items.push(item);
    }
    let inner_list_params = params_from_interface(sfv_inner_list.params.deref())?;
    Ok(InnerList::with_params(inner_list_items, inner_list_params))
}

fn list_entry_from_interface(obj: &nsISFVItemOrInnerList) -> Result<ListEntry, nsresult> {
    if let Some(nsi_item) = obj.query_interface::<nsISFVItem>() {
        let item = item_from_interface(nsi_item.deref())?;
        Ok(ListEntry::Item(item))
    } else if let Some(nsi_inner_list) = obj.query_interface::<nsISFVInnerList>() {
        let inner_list = inner_list_from_interface(nsi_inner_list.deref())?;
        Ok(ListEntry::InnerList(inner_list))
    } else {
        return Err(NS_ERROR_UNEXPECTED);
    }
}

fn interface_from_bare_item(bare_item: &BareItem) -> Result<RefPtr<nsISFVBareItem>, nsresult> {
    let bare_item = match bare_item {
        BareItem::Boolean(val) => RefPtr::new(SFVBool::new(*val).coerce::<nsISFVBareItem>()),
        BareItem::String(val) => {
            RefPtr::new(SFVString::new(&nsCString::from(val)).coerce::<nsISFVBareItem>())
        }
        BareItem::Token(val) => {
            RefPtr::new(SFVToken::new(&nsCString::from(val)).coerce::<nsISFVBareItem>())
        }
        BareItem::ByteSeq(val) => RefPtr::new(
            SFVByteSeq::new(&nsCString::from(String::from_utf8(val.to_vec()).unwrap()))
                .coerce::<nsISFVBareItem>(),
        ),
        BareItem::Decimal(val) => {
            let val = val
                .to_string()
                .parse::<f64>()
                .map_err(|_| NS_ERROR_UNEXPECTED)?;
            RefPtr::new(SFVDecimal::new(val).coerce::<nsISFVBareItem>())
        }
        BareItem::Integer(val) => RefPtr::new(SFVInteger::new(*val).coerce::<nsISFVBareItem>()),
    };

    Ok(bare_item)
}

fn interface_from_item(item: &Item) -> Result<RefPtr<nsISFVItem>, nsresult> {
    let nsi_bare_item = interface_from_bare_item(&item.bare_item)?;
    let nsi_params = interface_from_params(&item.params)?;
    Ok(RefPtr::new(
        SFVItem::new(&nsi_bare_item, &nsi_params).coerce::<nsISFVItem>(),
    ))
}

fn interface_from_params(params: &Parameters) -> Result<RefPtr<nsISFVParams>, nsresult> {
    let sfv_params = SFVParams::new();
    for (key, value) in params.iter() {
        sfv_params
            .params
            .borrow_mut()
            .insert(key.clone(), value.clone());
    }
    Ok(RefPtr::new(sfv_params.coerce::<nsISFVParams>()))
}

fn interface_from_list_entry(
    member: &ListEntry,
) -> Result<RefPtr<nsISFVItemOrInnerList>, nsresult> {
    match member {
        ListEntry::Item(item) => {
            let nsi_bare_item = interface_from_bare_item(&item.bare_item)?;
            let nsi_params = interface_from_params(&item.params)?;
            Ok(RefPtr::new(
                SFVItem::new(&nsi_bare_item, &nsi_params).coerce::<nsISFVItemOrInnerList>(),
            ))
        }
        ListEntry::InnerList(inner_list) => {
            let mut nsi_inner_list = Vec::new();
            for item in inner_list.items.iter() {
                let nsi_item = interface_from_item(item)?;
                nsi_inner_list.push(nsi_item);
            }

            let nsi_params = interface_from_params(&inner_list.params)?;
            Ok(RefPtr::new(
                SFVInnerList::new(nsi_inner_list, &nsi_params).coerce::<nsISFVItemOrInnerList>(),
            ))
        }
    }
}
