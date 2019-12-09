// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::convert::TryFrom;

use serde::{Deserialize, Serialize};

use crate::error::{Error, ErrorKind};

/// Different resolutions supported by the memory related metric types (e.g.
/// MemoryDistributionMetric).
#[derive(Copy, Clone, Debug, Deserialize, Serialize)]
#[serde(rename_all = "lowercase")]
pub enum MemoryUnit {
    ///
    Byte,
    ///
    Kilobyte,
    ///
    Megabyte,
    ///
    Gigabyte,
}

impl MemoryUnit {
    /// Convert a value in the given unit to bytes.
    ///
    /// ## Arguments
    ///
    /// * `value` - the value to convert.
    ///
    /// ## Return value
    ///
    /// The integer representation of the byte value.
    pub fn as_bytes(self, value: u64) -> u64 {
        use MemoryUnit::*;
        match self {
            Byte => value,
            Kilobyte => value << 10,
            Megabyte => value << 20,
            Gigabyte => value << 30,
        }
    }
}

/// Trait implementation for converting an integer value
/// to a MemoryUnit. This is used in the FFI code. Please
/// note that values should match the ordering of the platform
/// specific side of things (e.g. Kotlin implementation).
impl TryFrom<i32> for MemoryUnit {
    type Error = Error;

    fn try_from(value: i32) -> Result<MemoryUnit, Self::Error> {
        match value {
            0 => Ok(MemoryUnit::Byte),
            1 => Ok(MemoryUnit::Kilobyte),
            2 => Ok(MemoryUnit::Megabyte),
            3 => Ok(MemoryUnit::Gigabyte),
            e => Err(ErrorKind::MemoryUnit(e).into()),
        }
    }
}
