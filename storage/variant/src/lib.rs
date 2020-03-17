/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;
extern crate nserror;
extern crate nsstring;
extern crate xpcom;

mod bag;

use std::{
    borrow::Cow,
    convert::{TryFrom, TryInto},
};

use libc::c_double;
use nserror::{nsresult, NS_ERROR_CANNOT_CONVERT_DATA, NS_OK};
use nsstring::{nsACString, nsAString, nsCString, nsString};
use xpcom::{getter_addrefs, interfaces::nsIVariant, RefPtr};

pub use crate::bag::HashPropertyBag;

extern "C" {
    fn NS_GetDataType(variant: *const nsIVariant) -> u16;
    fn NS_NewStorageNullVariant(result: *mut *const nsIVariant);
    fn NS_NewStorageBooleanVariant(value: bool, result: *mut *const nsIVariant);
    fn NS_NewStorageIntegerVariant(value: i64, result: *mut *const nsIVariant);
    fn NS_NewStorageFloatVariant(value: c_double, result: *mut *const nsIVariant);
    fn NS_NewStorageTextVariant(value: *const nsAString, result: *mut *const nsIVariant);
    fn NS_NewStorageUTF8TextVariant(value: *const nsACString, result: *mut *const nsIVariant);
}

// These are the relevant parts of the nsXPTTypeTag enum in xptinfo.h,
// which nsIVariant.idl reflects into the nsIDataType struct class and uses
// to constrain the values of nsIVariant::dataType.
#[repr(u16)]
#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq)]
pub enum DataType {
    Int32 = 2,
    Int64 = 3,
    Double = 9,
    Bool = 10,
    Void = 13,
    CharStr = 15,
    WCharStr = 16,
    StringSizeIs = 20,
    WStringSizeIs = 21,
    Utf8String = 24,
    CString = 25,
    AString = 26,
    EmptyArray = 254,
    Empty = 255,
}

impl TryFrom<u16> for DataType {
    type Error = nsresult;

    /// Converts a raw type tag for an `nsIVariant` into a `DataType` variant.
    /// Returns `NS_ERROR_CANNOT_CONVERT_DATA` if the type isn't one that we
    /// support.
    fn try_from(raw: u16) -> Result<Self, Self::Error> {
        Ok(match raw {
            2 => DataType::Int32,
            3 => DataType::Int64,
            9 => DataType::Double,
            10 => DataType::Bool,
            13 => DataType::Void,
            15 => DataType::CharStr,
            16 => DataType::WCharStr,
            20 => DataType::StringSizeIs,
            21 => DataType::WStringSizeIs,
            24 => DataType::Utf8String,
            25 => DataType::CString,
            26 => DataType::AString,
            254 => DataType::EmptyArray,
            255 => DataType::Empty,
            _ => Err(NS_ERROR_CANNOT_CONVERT_DATA)?,
        })
    }
}

/// Extension methods implemented on `nsIVariant` types, to make them easier
/// to work with.
pub trait NsIVariantExt {
    /// Returns the raw type tag for this variant. Call
    /// `DataType::try_from()` on this tag to turn it into a `DataType`.
    fn get_data_type(&self) -> u16;

    /// Tries to clone this variant, failing with `NS_ERROR_CANNOT_CONVERT_DATA`
    /// if its type is unsupported.
    fn try_clone(&self) -> Result<RefPtr<nsIVariant>, nsresult>;
}

impl NsIVariantExt for nsIVariant {
    fn get_data_type(&self) -> u16 {
        unsafe { NS_GetDataType(self) }
    }

    fn try_clone(&self) -> Result<RefPtr<nsIVariant>, nsresult> {
        Ok(match self.get_data_type().try_into()? {
            DataType::Bool => bool::from_variant(self)?.into_variant(),
            DataType::Int32 => i32::from_variant(self)?.into_variant(),
            DataType::Int64 => i64::from_variant(self)?.into_variant(),
            DataType::Double => f64::from_variant(self)?.into_variant(),
            DataType::AString | DataType::WCharStr | DataType::WStringSizeIs => {
                nsString::from_variant(self)?.into_variant()
            }
            DataType::CString
            | DataType::CharStr
            | DataType::StringSizeIs
            | DataType::Utf8String => nsCString::from_variant(self)?.into_variant(),
            DataType::Void | DataType::EmptyArray | DataType::Empty => ().into_variant(),
        })
    }
}

pub trait VariantType {
    fn type_name() -> Cow<'static, str>;
    fn into_variant(self) -> RefPtr<nsIVariant>;
    fn from_variant(variant: &nsIVariant) -> Result<Self, nsresult>
    where
        Self: Sized;
}

/// Implements traits to convert between variants and their types.
macro_rules! variant {
    ($typ:ident, $constructor:ident, $getter:ident) => {
        impl VariantType for $typ {
            fn type_name() -> Cow<'static, str> {
                stringify!($typ).into()
            }
            fn into_variant(self) -> RefPtr<nsIVariant> {
                // getter_addrefs returns a Result<RefPtr<T>, nsresult>,
                // but we know that our $constructor is infallible, so we can
                // safely unwrap and return the RefPtr.
                getter_addrefs(|p| {
                    unsafe { $constructor(self.into(), p) };
                    NS_OK
                })
                .unwrap()
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
            fn type_name() -> Cow<'static, str> {
                stringify!($typ).into()
            }
            fn into_variant(self) -> RefPtr<nsIVariant> {
                // getter_addrefs returns a Result<RefPtr<T>, nsresult>,
                // but we know that our $constructor is infallible, so we can
                // safely unwrap and return the RefPtr.
                getter_addrefs(|p| {
                    unsafe { $constructor(&*self, p) };
                    NS_OK
                })
                .unwrap()
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
    fn type_name() -> Cow<'static, str> {
        "()".into()
    }
    fn into_variant(self) -> RefPtr<nsIVariant> {
        // getter_addrefs returns a Result<RefPtr<T>, nsresult>,
        // but we know that NS_NewStorageNullVariant is infallible, so we can
        // safely unwrap and return the RefPtr.
        getter_addrefs(|p| {
            unsafe { NS_NewStorageNullVariant(p) };
            NS_OK
        })
        .unwrap()
    }
    fn from_variant(_variant: &nsIVariant) -> Result<Self, nsresult> {
        Ok(())
    }
}

impl<T> VariantType for Option<T>
where
    T: VariantType,
{
    fn type_name() -> Cow<'static, str> {
        format!("Option<{}>", T::type_name()).into()
    }
    fn into_variant(self) -> RefPtr<nsIVariant> {
        match self {
            Some(v) => v.into_variant(),
            None => ().into_variant(),
        }
    }
    fn from_variant(variant: &nsIVariant) -> Result<Self, nsresult> {
        Ok(match variant.get_data_type().try_into() {
            Ok(DataType::Void) | Ok(DataType::EmptyArray) | Ok(DataType::Empty) => None,
            _ => Some(VariantType::from_variant(variant)?),
        })
    }
}

variant!(bool, NS_NewStorageBooleanVariant, GetAsBool);
variant!(i32, NS_NewStorageIntegerVariant, GetAsInt32);
variant!(i64, NS_NewStorageIntegerVariant, GetAsInt64);
variant!(f64, NS_NewStorageFloatVariant, GetAsDouble);
variant!(*nsString, NS_NewStorageTextVariant, GetAsAString);
variant!(*nsCString, NS_NewStorageUTF8TextVariant, GetAsAUTF8String);
