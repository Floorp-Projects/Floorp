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
#[repr(i32)] // use i32 to be compatible with our JNA definition
pub enum MemoryUnit {
    /// 1 byte
    Byte,
    /// 2^10 bytes
    Kilobyte,
    /// 2^20 bytes
    Megabyte,
    /// 2^30 bytes
    Gigabyte,
}

impl MemoryUnit {
    /// Converts a value in the given unit to bytes.
    ///
    /// # Arguments
    ///
    /// * `value` - the value to convert.
    ///
    /// # Returns
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
/// to a [`MemoryUnit`]. This is used in the FFI code. Please
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
