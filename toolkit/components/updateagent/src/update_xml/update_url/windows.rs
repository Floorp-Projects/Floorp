/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#![cfg(windows)]

//! This file contains the Windows-specific code for generating the update URL
//! Currently, this crate only supports being built on Windows, but this code is separated anyways
//! because we may later want to extend this to other OS's.

use std::{mem, ptr};
use winapi::{
    shared::{minwindef::BOOL, winerror::ERROR_SUCCESS},
    um::{
        errhandlingapi::GetLastError,
        sysinfoapi::{
            GetNativeSystemInfo, GetVersionExW, GlobalMemoryStatusEx, LPSYSTEM_INFO,
            MEMORYSTATUSEX, SYSTEM_INFO,
        },
        winnt::{
            LPOSVERSIONINFOEXW, LPOSVERSIONINFOW, OSVERSIONINFOEXW, PROCESSOR_ARCHITECTURE_AMD64,
            PROCESSOR_ARCHITECTURE_ARM64, PROCESSOR_ARCHITECTURE_IA64,
            PROCESSOR_ARCHITECTURE_INTEL, PROCESSOR_ARCHITECTURE_UNKNOWN, VER_PLATFORM_WIN32_NT,
            VER_PLATFORM_WIN32_WINDOWS,
        },
        winreg::{RegGetValueW, HKEY_LOCAL_MACHINE, RRF_RT_REG_DWORD, RRF_SUBKEY_WOW6464KEY},
    },
};
use wio::wide::ToWide;

/// Returns a string indicating the CPU architecture of the current platform.
/// It will be one of:
///    "x86"
///    "x64"
///    "IA64"
///    "aarch64"
///    "unknown"
pub fn get_cpu_arch() -> &'static str {
    let arch = unsafe {
        let mut info: SYSTEM_INFO = mem::zeroed();
        info.u.s_mut().wProcessorArchitecture = PROCESSOR_ARCHITECTURE_UNKNOWN;
        GetNativeSystemInfo(&mut info as LPSYSTEM_INFO);
        info.u.s().wProcessorArchitecture
    };
    match arch {
        PROCESSOR_ARCHITECTURE_ARM64 => "aarch64",
        PROCESSOR_ARCHITECTURE_AMD64 => "x64",
        PROCESSOR_ARCHITECTURE_IA64 => "IA64",
        PROCESSOR_ARCHITECTURE_INTEL => "x86",
        _ => "unknown",
    }
}

/// This function will return a tuple of the OS name and version strings.
pub fn get_sys_info() -> Result<(String, String), String> {
    let (platform_id, major_ver, minor_ver, sp_major_ver, sp_minor_ver, build_number) = unsafe {
        let mut info: OSVERSIONINFOEXW = mem::zeroed();
        info.dwOSVersionInfoSize = mem::size_of::<OSVERSIONINFOEXW>() as u32;
        let success: BOOL = GetVersionExW(&mut info as LPOSVERSIONINFOEXW as LPOSVERSIONINFOW);
        if success == 0 {
            return Err(format!("GetVersionExW failed: {:#X}", GetLastError()));
        }
        (
            info.dwPlatformId,
            info.dwMajorVersion,
            info.dwMinorVersion,
            info.wServicePackMajor,
            info.wServicePackMinor,
            info.dwBuildNumber,
        )
    };
    let os_name = match platform_id {
        VER_PLATFORM_WIN32_NT => "Windows_NT".to_string(),
        _ => "Windows_Unknown".to_string(),
    };
    let mut os_version =
        if platform_id == VER_PLATFORM_WIN32_NT || platform_id == VER_PLATFORM_WIN32_WINDOWS {
            format!(
                "{}.{}.{}.{}.{}",
                major_ver, minor_ver, sp_major_ver, sp_minor_ver, build_number
            )
        } else {
            "0.0.unknown".to_string()
        };

    if major_ver >= 10 {
        os_version.push('.');
        os_version.push_str(&get_ubr());
    }

    Ok((os_name, os_version))
}

/// Gets the Update Build Revision of the current Windows platform. This will not be available on
/// systems prior to Windows 10. If the value cannot be determined, the string "unknown" will be
/// returned.
pub fn get_ubr() -> String {
    let key = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    let value = "UBR";
    let wide_key: Vec<u16> = key.to_wide_null();
    let wide_value: Vec<u16> = value.to_wide_null();

    let ubr = unsafe {
        let mut ubr: u32 = 0;
        let mut ubr_size: u32 = mem::size_of::<u32>() as u32;

        let status = RegGetValueW(
            HKEY_LOCAL_MACHINE,
            wide_key.as_ptr(),
            wide_value.as_ptr(),
            RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY,
            ptr::null_mut(),
            &mut ubr as *mut u32 as *mut _,
            &mut ubr_size,
        );

        if status != ERROR_SUCCESS as i32 {
            return "unknown".to_string();
        }

        ubr
    };

    ubr.to_string()
}

/// This function gets a string describing the relevant secondary libraries. On Windows, there are
/// never any relevant secondary libraries, so this function will always return None.
pub fn get_secondary_libraries() -> Result<Option<String>, String> {
    Ok(None)
}

/// On success, returns a string indicating how many megabytes of memory are available on the
/// system. On failure, returns the string "unknown".
pub fn get_memory_mb() -> String {
    let memory_bytes: u64 = unsafe {
        let mut mem_info: MEMORYSTATUSEX = mem::zeroed();
        mem_info.dwLength = mem::size_of::<MEMORYSTATUSEX>() as u32;
        let success: BOOL = GlobalMemoryStatusEx(&mut mem_info);
        if success == 0 {
            return "unknown".to_string();
        }
        mem_info.ullTotalPhys
    };

    format!("{:.0}", memory_bytes as f64 / 1024_f64 / 1024_f64)
}
