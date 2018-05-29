//! Provides utilities for searching the system path.

use std::env;
use std::path::PathBuf;

#[cfg(unix)]
fn is_executable(path: &PathBuf) -> bool {
    use std::fs;
    use std::os::unix::fs::PermissionsExt;

    // Permissions are a set of four 4-bit bitflags, represented by a single octal
    // digit.  The lowest bit of each of the last three values represents the
    // executable permission for all, group and user, repsectively.  We assume the
    // file is executable if any of these are set.
    match fs::metadata(path).ok() {
        Some(meta) => meta.permissions().mode() & 0o111 != 0,
        None => false,
    }
}

#[cfg(not(unix))]
fn is_executable(_: &PathBuf) -> bool {
    true
}

/// Determines if the path is an executable binary.  That is, if it exists, is
/// a file, and is executable where applicable.
pub fn is_binary(path: &PathBuf) -> bool {
    path.exists() && path.is_file() && is_executable(&path)
}

/// Searches the system path (`PATH`) for an executable binary and returns the
/// first match, or `None` if not found.
pub fn find_binary(binary_name: &str) -> Option<PathBuf> {
    env::var_os("PATH").and_then(|path_env| {
        for mut path in env::split_paths(&path_env) {
            path.push(binary_name);
            if is_binary(&path) {
                return Some(path);
            }
        }
        None
    })
}
