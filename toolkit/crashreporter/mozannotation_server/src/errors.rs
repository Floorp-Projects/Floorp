/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use thiserror::Error;

#[derive(Debug, Error)]
pub enum RetrievalError {
    #[error("The process handle/PID was invalid")]
    InvalidProcessHandle,
    #[error("Could not find the address of the annotations vector")]
    AnnotationTableNotFound(#[from] FindAnnotationsAddressError),
    #[error("Corrupt or wrong annotation table")]
    InvalidAnnotationTable,
    #[error("The data read from the target process is invalid")]
    InvalidData,
    #[cfg(any(target_os = "linux", target_os = "android"))]
    #[error("Could not attach to the target process")]
    AttachError(#[from] PtraceError),
    #[error("Could not read from the target process address space")]
    ReadFromProcessError(#[from] ReadError),
    #[error("waitpid() failed when attaching to the process")]
    WaitPidError,
}

#[derive(Debug, Error)]
pub enum FindAnnotationsAddressError {
    #[error("Could not convert address {0}")]
    ConvertAddressError(#[from] std::num::TryFromIntError),
    #[error("goblin failed to parse a module")]
    GoblinError(#[from] goblin::error::Error),
    #[error("Address was out of bounds")]
    InvalidAddress,
    #[error("IO error for file {0}")]
    IOError(#[from] std::io::Error),
    #[error("Could not find the address of the annotations vector")]
    NotFound,
    #[error("Could not parse address {0}")]
    ParseAddressError(#[from] std::num::ParseIntError),
    #[error("Could not parse a line in /proc/<pid>/maps")]
    ProcMapsParseError,
    #[cfg(any(target_os = "linux", target_os = "android"))]
    #[error("Program header was not found")]
    ProgramHeaderNotFound,
    #[cfg(target_os = "windows")]
    #[error("Section was not found")]
    SectionNotFound,
    #[cfg(target_os = "windows")]
    #[error("Cannot enumerate the target process's modules")]
    EnumProcessModulesError,
    #[error("Could not read memory from the target process")]
    ReadError(#[from] ReadError),
    #[cfg(target_os = "macos")]
    #[error("Failure when requesting the task information")]
    TaskInfoError,
    #[cfg(target_os = "macos")]
    #[error("The task dyld information format is unknown or invalid")]
    ImageFormatError,
}

#[derive(Debug, Error)]
pub enum ReadError {
    #[cfg(any(target_os = "linux", target_os = "android"))]
    #[error("ptrace-specific error")]
    PtraceError(#[from] PtraceError),
    #[cfg(target_os = "windows")]
    #[error("ReadProcessMemory failed")]
    ReadProcessMemoryError,
    #[cfg(target_os = "macos")]
    #[error("mach call failed")]
    MachError,
}

#[cfg(any(target_os = "linux", target_os = "android"))]
#[derive(Debug, Error)]
pub enum PtraceError {
    #[error("Could not read from the target process address space")]
    ReadError(#[source] std::io::Error),
    #[error("Could not trace the process")]
    TraceError(#[source] std::io::Error),
}
