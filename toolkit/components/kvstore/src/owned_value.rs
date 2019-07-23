/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use error::KeyValueError;
use nsstring::nsString;
use rkv::OwnedValue;
use storage_variant::{
    GetDataType, VariantType, DATA_TYPE_BOOL, DATA_TYPE_DOUBLE, DATA_TYPE_EMPTY, DATA_TYPE_INT32,
    DATA_TYPE_VOID, DATA_TYPE_WSTRING,
};
use xpcom::{interfaces::nsIVariant, RefPtr};

pub fn owned_to_variant(owned: OwnedValue) -> Result<RefPtr<nsIVariant>, KeyValueError> {
    match owned {
        OwnedValue::Bool(val) => Ok(val.into_variant()),
        OwnedValue::I64(val) => Ok(val.into_variant()),
        OwnedValue::F64(val) => Ok(val.into_variant()),
        OwnedValue::Str(ref val) => Ok(nsString::from(val).into_variant()),

        // kvstore doesn't (yet?) support these types of OwnedValue,
        // and we should never encounter them, but we need to exhaust
        // all possible variants of the OwnedValue enum.
        OwnedValue::Instant(_) => Err(KeyValueError::UnsupportedOwned),
        OwnedValue::Json(_) => Err(KeyValueError::UnsupportedOwned),
        OwnedValue::U64(_) => Err(KeyValueError::UnsupportedOwned),
        OwnedValue::Uuid(_) => Err(KeyValueError::UnsupportedOwned),
        OwnedValue::Blob(_) => Err(KeyValueError::UnsupportedOwned),
    }
}

pub fn variant_to_owned(variant: &nsIVariant) -> Result<Option<OwnedValue>, KeyValueError> {
    let data_type = variant.get_data_type();

    match data_type {
        DATA_TYPE_INT32 => {
            let mut val: i32 = 0;
            unsafe { variant.GetAsInt32(&mut val) }.to_result()?;
            Ok(Some(OwnedValue::I64(val.into())))
        }
        DATA_TYPE_DOUBLE => {
            let mut val: f64 = 0.0;
            unsafe { variant.GetAsDouble(&mut val) }.to_result()?;
            Ok(Some(OwnedValue::F64(val)))
        }
        DATA_TYPE_WSTRING => {
            let mut val: nsString = nsString::new();
            unsafe { variant.GetAsAString(&mut *val) }.to_result()?;
            let str = String::from_utf16(&val)?;
            Ok(Some(OwnedValue::Str(str)))
        }
        DATA_TYPE_BOOL => {
            let mut val: bool = false;
            unsafe { variant.GetAsBool(&mut val) }.to_result()?;
            Ok(Some(OwnedValue::Bool(val)))
        }
        DATA_TYPE_EMPTY | DATA_TYPE_VOID => Ok(None),
        unsupported_type => Err(KeyValueError::UnsupportedVariant(unsupported_type)),
    }
}
