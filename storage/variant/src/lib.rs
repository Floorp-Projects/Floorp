/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;
extern crate nserror;
extern crate nsstring;
extern crate xpcom;

mod bag;

use libc::{c_double, int64_t, uint16_t};
use nserror::{nsresult, NS_OK};
use nsstring::{nsACString, nsAString, nsCString, nsString};
use xpcom::{getter_addrefs, interfaces::nsIVariant, RefPtr};

pub use crate::bag::HashPropertyBag;

extern "C" {
    fn NS_GetDataType(variant: *const nsIVariant) -> uint16_t;
    fn NS_NewStorageNullVariant(result: *mut *const nsIVariant);
    fn NS_NewStorageBooleanVariant(value: bool, result: *mut *const nsIVariant);
    fn NS_NewStorageIntegerVariant(value: int64_t, result: *mut *const nsIVariant);
    fn NS_NewStorageFloatVariant(value: c_double, result: *mut *const nsIVariant);
    fn NS_NewStorageTextVariant(value: *const nsAString, result: *mut *const nsIVariant);
    fn NS_NewStorageUTF8TextVariant(value: *const nsACString, result: *mut *const nsIVariant);
}

// These are the relevant parts of the nsXPTTypeTag enum in xptinfo.h,
// which nsIVariant.idl reflects into the nsIDataType struct class and uses
// to constrain the values of nsIVariant::dataType.
#[repr(u16)]
pub enum DataType {
    INT32 = 2,
    DOUBLE = 9,
    BOOL = 10,
    VOID = 13,
    WSTRING = 21,
    EMPTY = 255,
}

// Per https://github.com/rust-lang/rust/issues/44266, casts aren't allowed
// in match arms, so it isn't possible to cast DataType variants to u16
// in order to match them against the value of nsIVariant::dataType.
// Instead we have to reflect each variant into a constant and then match
// against the values of the constants.
//
// (Alternatively, we could use the enum_primitive crate to convert primitive
// values of nsIVariant::dataType to their enum equivalents.  Or perhaps
// bindgen would convert the nsXPTTypeTag enum in xptinfo.h into something else
// we could use.  Since we currently only accept a small subset of values,
// and since that enum is unlikely to change frequently, this workaround
// seems sufficient.)
//
pub const DATA_TYPE_INT32: uint16_t = DataType::INT32 as u16;
pub const DATA_TYPE_DOUBLE: uint16_t = DataType::DOUBLE as u16;
pub const DATA_TYPE_BOOL: uint16_t = DataType::BOOL as u16;
pub const DATA_TYPE_VOID: uint16_t = DataType::VOID as u16;
pub const DATA_TYPE_WSTRING: uint16_t = DataType::WSTRING as u16;
pub const DATA_TYPE_EMPTY: uint16_t = DataType::EMPTY as u16;

pub trait GetDataType {
    fn get_data_type(&self) -> uint16_t;
}

impl GetDataType for nsIVariant {
    fn get_data_type(&self) -> uint16_t {
        unsafe { NS_GetDataType(self) }
    }
}

pub trait VariantType {
    fn into_variant(self) -> RefPtr<nsIVariant>;
    fn from_variant(variant: &nsIVariant) -> Result<Self, nsresult>
    where
        Self: Sized;
}

/// Implements traits to convert between variants and their types.
macro_rules! variant {
    ($typ:ident, $constructor:ident, $getter:ident) => {
        impl VariantType for $typ {
            fn into_variant(self) -> RefPtr<nsIVariant> {
                // getter_addrefs returns a Result<RefPtr<T>, nsresult>,
                // but we know that our $constructor is infallible, so we can
                // safely unwrap and return the RefPtr.
                getter_addrefs(|p| {
                    unsafe { $constructor(self.into(), p) };
                    NS_OK
                }).unwrap()
            }
            fn from_variant(variant: &nsIVariant) -> Result<$typ, nsresult> {
                let mut result = $typ::default();
                let rv = unsafe { variant.$getter(&mut result) };
                if rv.succeeded() {
                    Ok(result)
                } else {
                    Err(rv)
                }
            }
        }
    };
    (* $typ:ident, $constructor:ident, $getter:ident) => {
        impl VariantType for $typ {
            fn into_variant(self) -> RefPtr<nsIVariant> {
                // getter_addrefs returns a Result<RefPtr<T>, nsresult>,
                // but we know that our $constructor is infallible, so we can
                // safely unwrap and return the RefPtr.
                getter_addrefs(|p| {
                    unsafe { $constructor(&*self, p) };
                    NS_OK
                }).unwrap()
            }
            fn from_variant(variant: &nsIVariant) -> Result<$typ, nsresult> {
                let mut result = $typ::new();
                let rv = unsafe { variant.$getter(&mut *result) };
                if rv.succeeded() {
                    Ok(result)
                } else {
                    Err(rv)
                }
            }
        }
    };
}

// The unit type (()) is a reasonable equivalation of the null variant.
// The macro can't produce its implementations of VariantType, however,
// so we implement them concretely.
impl VariantType for () {
    fn into_variant(self) -> RefPtr<nsIVariant> {
        // getter_addrefs returns a Result<RefPtr<T>, nsresult>,
        // but we know that NS_NewStorageNullVariant is infallible, so we can
        // safely unwrap and return the RefPtr.
        getter_addrefs(|p| {
            unsafe { NS_NewStorageNullVariant(p) };
            NS_OK
        }).unwrap()
    }
    fn from_variant(_variant: &nsIVariant) -> Result<Self, nsresult> {
        Ok(())
    }
}

impl<T> VariantType for Option<T> where T: VariantType {
    fn into_variant(self) -> RefPtr<nsIVariant> {
        match self {
            Some(v) => v.into_variant(),
            None => ().into_variant(),
        }
    }
    fn from_variant(variant: &nsIVariant) -> Result<Self, nsresult> {
        match variant.get_data_type() {
            DATA_TYPE_EMPTY => Ok(None),
            _ => Ok(Some(VariantType::from_variant(variant)?)),
        }
    }
}

variant!(bool, NS_NewStorageBooleanVariant, GetAsBool);
variant!(i32, NS_NewStorageIntegerVariant, GetAsInt32);
variant!(i64, NS_NewStorageIntegerVariant, GetAsInt64);
variant!(f64, NS_NewStorageFloatVariant, GetAsDouble);
variant!(*nsString, NS_NewStorageTextVariant, GetAsAString);
variant!(*nsCString, NS_NewStorageUTF8TextVariant, GetAsACString);
