/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use super::{
    action::Action,
    error::{BitsTaskError, ErrorStage, ErrorType},
};

use bits_client::Guid;
use nsstring::nsCString;
use std::ffi::OsString;
use std::{str, str::FromStr};

/// This function is fallible, and the consumers would prefer a BitsTaskError
/// in the failure case. To facilitate that, this function takes some params
/// that give it the data necessary to construct the BitsTaskError if it fails.
/// If it succeeds, those values will be unused.
#[allow(non_snake_case)]
pub fn nsCString_to_String(
    value: &nsCString,
    error_action: Action,
    error_stage: ErrorStage,
) -> Result<String, BitsTaskError> {
    match String::from_utf8(value[..].to_vec()) {
        Ok(s) => Ok(s),
        Err(_) => Err(BitsTaskError::new(
            ErrorType::NoUtf8Conversion,
            error_action,
            error_stage,
        )),
    }
}

/// This function is fallible, and the consumers would prefer a BitsTaskError
/// in the failure case. To facilitate that, this function takes some params
/// that give it the data necessary to construct the BitsTaskError if it fails.
/// If it succeeds, those values will be unused.
#[allow(non_snake_case)]
pub fn nsCString_to_OsString(
    value: &nsCString,
    error_action: Action,
    error_stage: ErrorStage,
) -> Result<OsString, BitsTaskError> {
    Ok(OsString::from(nsCString_to_String(
        value,
        error_action,
        error_stage,
    )?))
}

/// This function is fallible, and the consumers would prefer a BitsTaskError
/// in the failure case. To facilitate that, this function takes some params
/// that give it the data necessary to construct the BitsTaskError if it fails.
/// If it succeeds, those values will be unused.
#[allow(non_snake_case)]
pub fn Guid_from_nsCString(
    value: &nsCString,
    error_action: Action,
    error_stage: ErrorStage,
) -> Result<Guid, BitsTaskError> {
    let vector = &value[..].to_vec();
    let string = match str::from_utf8(vector) {
        Ok(s) => s,
        Err(_) => {
            return Err(BitsTaskError::new(
                ErrorType::NoUtf8Conversion,
                error_action,
                error_stage,
            ));
        }
    };
    Guid::from_str(string).map_err(|comedy_error| {
        BitsTaskError::from_comedy(
            ErrorType::InvalidGuid,
            error_action,
            error_stage,
            comedy_error,
        )
    })
}
