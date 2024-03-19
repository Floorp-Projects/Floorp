/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use thiserror::Error;

#[derive(Debug, Error)]
pub enum ProcessReaderError {
    #[error("Could not convert address {0}")]
    ConvertAddressError(#[from] std::num::TryFromIntError),
    #[cfg(target_os = "windows")]
    #[error("Cannot enumerate the target process's modules")]
    EnumProcessModulesError,
    #[error("goblin failed to parse a module")]
    GoblinError(#[from] goblin::error::Error),
    #[error("Address was out of bounds")]
    InvalidAddress,
    #[error("Could not read from the target process address space")]
    ReadFromProcessError(#[from] ReadError),
    #[cfg(target_os = "windows")]
    #[error("Section was not found")]
    SectionNotFound,
}

#[derive(Debug, Error)]
pub enum ReadError {
    #[cfg(target_os = "windows")]
    #[error("ReadProcessMemory failed")]
    ReadProcessMemoryError,
}
