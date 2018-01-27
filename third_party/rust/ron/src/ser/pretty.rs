//! Provides default pretty serialization with `to_string`.

use super::{Result, to_string_pretty};

use serde::ser::Serialize;
use std::default::Default;

/// Serializes `value` in the recommended RON layout with
/// default pretty configuration.
pub fn to_string<T>(value: &T) -> Result<String>
    where T: Serialize
{
    to_string_pretty(value, Default::default())
}
