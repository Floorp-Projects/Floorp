/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This crate generates the update url used by Firefox to check for updates.

mod constants;
mod read_pref;
mod windows;

use cfg_if::cfg_if;
use ini::Ini;
use percent_encoding::{utf8_percent_encode, AsciiSet, NON_ALPHANUMERIC};
use read_pref::read_pref;
use std::{env::current_exe, path::Path};

pub fn generate() -> Result<String, String> {
    let binary_path =
        current_exe().map_err(|e| format!("Couldn't get executing binary's path: {}", e))?;
    let resource_path = binary_path
        .parent()
        .ok_or("Couldn't get executing binary's parent directory")?;

    let application_ini = read_application_ini(&resource_path)?;

    let windows_cpu_arch = windows::get_cpu_arch();

    // Get the data to be substituted into the URL
    let url = get_url_template(&application_ini)?;
    let (product, version, build_id) = get_application_info(&application_ini)?;
    let build_target = get_build_target(Some(&windows_cpu_arch))?;
    let channel = get_channel(&resource_path)?;
    let os_version = get_os_version(Some(&windows_cpu_arch))?;
    let (distribution, distribution_version) = get_distribution_info(&resource_path)?;
    let system_capabilities = get_system_capabilities()?;

    // Perform variable substitution
    let url = url.replace("%PRODUCT%", &product);
    let url = url.replace("%VERSION%", &version);
    let url = url.replace("%BUILD_ID%", &build_id);
    let url = url.replace("%BUILD_TARGET%", &build_target);
    let url = url.replace("%OS_VERSION%", &os_version);
    let url = url.replace("%LOCALE%", constants::LOCALE);
    let url = url.replace("%CHANNEL%", &channel);
    let url = url.replace("%PLATFORM_VERSION%", constants::GRE_MILESTONE);
    let url = url.replace("%SYSTEM_CAPABILITIES%", &system_capabilities);
    let url = url.replace("%DISTRIBUTION%", &distribution);
    let url = url.replace("%DISTRIBUTION_VERSION%", &distribution_version);

    let url = url.replace("+", "%2B");

    Ok(url)
}

fn read_application_ini(resource_path: &Path) -> Result<Ini, String> {
    let application_ini_path = resource_path.join("application.ini");
    let application_ini_path_str = application_ini_path
        .to_str()
        .ok_or("Path to application.ini is not valid Unicode")?;
    let application_ini = Ini::load_from_file(application_ini_path_str)
        .map_err(|e| format!("Unable to read application.ini: {}", e))?;
    Ok(application_ini)
}

fn get_url_template(application_ini: &Ini) -> Result<String, String> {
    let app_update_section = application_ini
        .section(Some("AppUpdate"))
        .ok_or("application.ini has no AppUpdate section")?;
    let template = app_update_section
        .get("URL")
        .ok_or("application.ini has no URL in its AppUpdate section")?;
    Ok(template.clone())
}

/// On success, returns a tuple of product, version, and build ID strings
fn get_application_info(application_ini: &Ini) -> Result<(String, String, String), String> {
    let app_section = application_ini
        .section(Some("App"))
        .ok_or("application.ini has no App section")?;
    let product = app_section
        .get("Name")
        .ok_or("application.ini has no Name in its App section")?;
    let version = app_section
        .get("Version")
        .ok_or("application.ini has no Version in its App section")?;
    let build_id = app_section
        .get("BuildID")
        .ok_or("application.ini has no BuildID in its App section")?;
    Ok((
        product.to_string(),
        version.to_string(),
        build_id.to_string(),
    ))
}

fn get_build_target(_maybe_windows_cpu_arch: Option<&str>) -> Result<String, String> {
    let mut target = String::new();
    target.push_str(constants::OS_TARGET);
    target.push('_');
    target.push_str(constants::TARGET_XPCOM_ABI);
    #[cfg(windows)]
    {
        if let Some(windows_cpu_arch) = _maybe_windows_cpu_arch {
            target.push('-');
            target.push_str(&windows_cpu_arch);
        } else {
            return Err("No CPU Architecture provided (required on Windows)".to_string());
        }
        if constants::MOZ_ASAN {
            target.push_str("-asan");
        }
    }
    Ok(target)
}

fn get_channel(resource_path: &Path) -> Result<String, String> {
    let channel_prefs_path = resource_path
        .join("defaults")
        .join("pref")
        .join("channel-prefs.js");
    let channel_prefs_path_str = channel_prefs_path
        .to_str()
        .ok_or("Path to channel-prefs.js is not valid Unicode")?;
    let channel = read_pref(channel_prefs_path_str, "app.update.channel")
        .map_err(|e| format!("Error parsing channel-prefs.js: {}", e))?;
    Ok(channel)
}

fn get_os_version(_maybe_windows_cpu_arch: Option<&str>) -> Result<String, String> {
    let (os_name, os_version) = windows::get_sys_info()?;

    let mut version_string = String::new();
    version_string.push_str(&os_name);
    version_string.push(' ');
    version_string.push_str(&os_version);

    #[cfg(windows)]
    {
        if let Some(windows_cpu_arch) = _maybe_windows_cpu_arch {
            version_string.push(' ');
            version_string.push('(');
            version_string.push_str(&windows_cpu_arch);
            version_string.push(')');
        } else {
            return Err("No CPU Architecture provided (required on Windows)".to_string());
        }
    }

    if let Some(secondary_libraries) = windows::get_secondary_libraries()? {
        version_string.push(' ');
        version_string.push('(');
        version_string.push_str(&secondary_libraries);
        version_string.push(')');
    }

    Ok(encode_uri_component(&version_string))
}

/// This mimics Javascript's encodeURIComponent.
/// MDN specifies that encodeURIComponent escapes all characters except:
///   A-Z a-z 0-9 - _ . ! ~ * ' ( )
fn encode_uri_component(value: &str) -> String {
    const URI_COMPONENT_ASCII_ESCAPE_SET: &AsciiSet = &NON_ALPHANUMERIC
        .remove(b'-')
        .remove(b'_')
        .remove(b'.')
        .remove(b'!')
        .remove(b'~')
        .remove(b'*')
        .remove(b'\'')
        .remove(b'(')
        .remove(b')');
    utf8_percent_encode(&value, URI_COMPONENT_ASCII_ESCAPE_SET).to_string()
}

fn get_system_capabilities() -> Result<String, String> {
    let instruction_set = get_instruction_set();
    let memory_mb = windows::get_memory_mb();

    Ok(format!("ISET:{},MEM:{}", instruction_set, memory_mb))
}

// is_x86_feature_detected!("sse4a") doesn't quite match Firefox's implementation; this should
// match more closely. Firefox does not check the brand of the CPU before making the cpuid checks,
// whereas is_x86_feature_detected!("sse4a") only makes the cpuid checks if the CPU is the right
// brand.
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
fn has_sse4a() -> bool {
    const QUERY_MAX_EXTENDED_LEVEL: u32 = 0x8000_0000;
    const QUERY_EXTENDED_FEATURE_INFO: u32 = 0x8000_0001;
    const ECX_SSE4A: u32 = 1 << 6;

    let max_extended_level = raw_cpuid::cpuid!(QUERY_MAX_EXTENDED_LEVEL).eax;
    if max_extended_level < QUERY_EXTENDED_FEATURE_INFO {
        return false;
    }

    (raw_cpuid::cpuid!(QUERY_EXTENDED_FEATURE_INFO).ecx & ECX_SSE4A) != 0
}

/// On success, returns a string describing the CPU instruction set. On failure, returns the string
/// "unknown".
fn get_instruction_set() -> &'static str {
    cfg_if! {
        if #[cfg(any(target_arch = "x86", target_arch = "x86_64"))] {
            if is_x86_feature_detected!("sse4.2") {
                return "SSE4_2";
            } else if is_x86_feature_detected!("sse4.1") {
                return "SSE4_1";
            } else if has_sse4a() {
                return "SSE4A";
            } else if is_x86_feature_detected!("ssse3") {
                return "SSSE3";
            } else if is_x86_feature_detected!("sse3") {
                return "SSE3";
            } else if is_x86_feature_detected!("sse2") {
                return "SSE2";
            } else if is_x86_feature_detected!("sse") {
                return "SSE";
            } else if is_x86_feature_detected!("mmx") {
                return "MMX";
            }
            "unknown"
        } else if #[cfg(target_arch = "aarch64")] {
            "NEON"
        } else {
            compile_error!("CPU feature detection not implemented for this arch")
        }
    }
}

/// On success, returns a tuple of the distribution id and version strings
fn get_distribution_info(resource_path: &Path) -> Result<(String, String), String> {
    let distribution_ini_path = resource_path.join("distribution").join("distribution.ini");
    let distribution_ini_path_str = distribution_ini_path
        .to_str()
        .ok_or("Path to distribution.ini is not valid Unicode")?;
    // It is ok for the distribution.ini file not to exist.
    let maybe_distribution_ini = Ini::load_from_file(distribution_ini_path_str).ok();
    match maybe_distribution_ini {
        Some(distribution_ini) => {
            let global_section = distribution_ini
                .section(Some("Global"))
                .ok_or("distribution.ini has no Global section")?;
            let id = global_section
                .get("id")
                .ok_or("distribution.ini has no id in its Global section")?;
            let id = if id.is_empty() {
                "default".to_string()
            } else {
                id.to_string()
            };
            let version = global_section
                .get("version")
                .ok_or("distribution.ini has no version in its Global section")?;
            let version = if version.is_empty() {
                "default".to_string()
            } else {
                version.to_string()
            };
            Ok((id, version))
        }
        None => Ok(("default".to_string(), "default".to_string())),
    }
}
