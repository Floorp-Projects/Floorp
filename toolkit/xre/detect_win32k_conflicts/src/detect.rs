/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module contains the actual functions that explore the registry for the status
//! of the conflicting mitigations.

use super::error::DetectConflictError;
use super::registry::{RegKey, RegValue};

use std::ffi::OsStr;

/// Subkey that the Windows Exploit Protection status is located in
const EXPLOIT_PROTECTION_SUBKEY: &str =
    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options";

/// Name of the value within that subkey that contains the exploit protection status
const MITIGATION_VALUE_NAME: &str = "MitigationOptions";

/// The bit number within the value with the StackPivot status
const STACK_PIVOT_BIT: usize = 80;

/// The bit number within the value with the CallerCheck status
const CALLER_CHECK_BIT: usize = 84;

/// The bit number within the value with the SimExec status
const SIM_EXEC_BIT: usize = 88;

/// Printable, FFI-safe structure containing the status of the conflicting mitigations
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ConflictingMitigationStatus {
    /// The status of the CallerCheck feature. 0 = disabled, 1 = enabled
    caller_check: bool,
    /// The status of the SimExec feature. 0 = disabled, 1 = enabled
    sim_exec: bool,
    /// The status of the StackPivot feature. 0 = disabled, 1 = enabled
    stack_pivot: bool,
}

impl std::fmt::Display for ConflictingMitigationStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        use std::fmt::Write;
        write_status(f, "CallerCheck", self.caller_check)?;
        f.write_char(' ')?;
        write_status(f, "SimExec", self.sim_exec)?;
        f.write_char(' ')?;
        write_status(f, "StackPivot", self.stack_pivot)
    }
}

/// Search the registry for win32k-conflicting mitigations for the given process
///
/// `process_name` is the name of the process that will be checked for conflicting mitigations
/// It uses the same case-insensitive algorithm that the Windows Registry uses itself to detect
/// whether a process needs mitigations or not
///
/// (This will usually be `firefox.exe`)
///
/// # Caveat
///
/// Windows allows an arbitrary number of "filters", which are subkeys of the main key
/// that also have a `MitigationOptions` value. They are used to allow filtering by path.
///
/// For simplicity, we "flatten" the path filters when we figure this out, and so the
/// returned value will be the logical "OR" of all mitigations enabled with any filter
///
/// So if there are two entries for "C:\Firefox\firefox.exe" with mitigations `A` and `B` enabled
/// and another entry for "C:\Program Files\Mozilla Firefox\firefox.exe" with mitigation `C`
/// enabled, we will return that `A`, `B`, and `C` are all enabled
pub fn get_conflicting_mitigations(
    process_name: impl AsRef<OsStr>,
) -> Result<ConflictingMitigationStatus, DetectConflictError> {
    let process_name = process_name.as_ref();

    let key = RegKey::root_local_machine()
        .try_open_subkey(EXPLOIT_PROTECTION_SUBKEY)?
        .ok_or(DetectConflictError::ExploitProtectionKeyMissing)?;

    let process_key = match key.try_open_subkey(process_name)? {
        Some(key) => key,
        None => {
            log::info!(
                "process name {:?} not found in exploit protection",
                process_name
            );
            return Ok(ConflictingMitigationStatus::default());
        }
    };

    // First get mitigation status for the root key
    let mut status = get_conflicting_mitigations_for_key(&process_key)?;

    // Then get mitigation status for each subkey and logical-or them all together
    for subkey_name in process_key.subkey_names() {
        let subkey_name = subkey_name?;

        let subkey = process_key
            .try_open_subkey(subkey_name)?
            .expect("a subkey somehow doesn't exist after enumerating it");

        let subkey_status = get_conflicting_mitigations_for_key(&subkey)?;
        status.caller_check |= subkey_status.caller_check;
        status.sim_exec |= subkey_status.sim_exec;
        status.stack_pivot |= subkey_status.stack_pivot;
    }

    log::info!(
        "process name {:?} has mitigation status {:?}",
        process_name,
        status
    );

    Ok(status)
}

/// Check a key to see if it has a "MitigationOptions" value with any conflicting mitigations
/// enabled
fn get_conflicting_mitigations_for_key(
    key: &RegKey,
) -> Result<ConflictingMitigationStatus, DetectConflictError> {
    let value = match key.try_get_value(MITIGATION_VALUE_NAME)? {
        Some(value) => value,
        None => return Ok(ConflictingMitigationStatus::default()),
    };

    let bits = match value {
        RegValue::Binary(bits) => bits,
        _ => return Ok(ConflictingMitigationStatus::default()),
    };

    Ok(ConflictingMitigationStatus {
        caller_check: get_bit(&bits, CALLER_CHECK_BIT)?,
        sim_exec: get_bit(&bits, SIM_EXEC_BIT)?,
        stack_pivot: get_bit(&bits, STACK_PIVOT_BIT)?,
    })
}

/// Retrieve a bit from a vector-of-bitflags, which is what will be returned by the registry
/// entry for exploit protection. The bits are numbered first by increasing value within a byte,
/// and then by increasing index within the slice.
///
/// Ex: bit 0 = (index 0, mask 0x01), bit 7 = (index 0, mask 0x80), bit 8 = (index 1, mask 0x01)
fn get_bit(bytes: &[u8], bit_idx: usize) -> Result<bool, DetectConflictError> {
    let byte = bytes
        .get(bit_idx / 8)
        .ok_or(DetectConflictError::RegValueTooShort)?;

    Ok(if byte & (1 << (bit_idx % 8)) != 0 {
        true
    } else {
        false
    })
}

/// Write the status of a feature to the given formatter. `bit == 0` means feature is disabled,
/// `bit == 1` means enabled
fn write_status(
    f: &mut std::fmt::Formatter<'_>,
    name: &str,
    bit: bool,
) -> Result<(), std::fmt::Error> {
    let status_str = if bit { "enabled" } else { "disabled" };
    write!(f, "{} is {}.", name, status_str)
}
