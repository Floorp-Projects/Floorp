// Copyright 2018 Kyle Mayes
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

extern crate glob;

use std::env;
use std::path::{Path, PathBuf};
use std::process::Command;

use glob::MatchOptions;

/// `libclang` directory patterns for FreeBSD and Linux.
const DIRECTORIES_LINUX: &[&str] = &[
    "/usr/lib*",
    "/usr/lib*/*",
    "/usr/lib*/*/*",
    "/usr/local/lib*",
    "/usr/local/lib*/*",
    "/usr/local/lib*/*/*",
    "/usr/local/llvm*/lib*",
];

/// `libclang` directory patterns for macOS.
const DIRECTORIES_MACOS: &[&str] = &[
    "/usr/local/opt/llvm*/lib",
    "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib",
    "/Library/Developer/CommandLineTools/usr/lib",
    "/usr/local/opt/llvm*/lib/llvm*/lib",
];

/// `libclang` directory patterns for Windows.
const DIRECTORIES_WINDOWS: &[&str] = &[
    "C:\\LLVM\\lib",
    "C:\\Program Files*\\LLVM\\lib",
    "C:\\MSYS*\\MinGW*\\lib",
];

/// Executes the supplied console command, returning the `stdout` output if the
/// command was successfully executed.
fn run_command(command: &str, arguments: &[&str]) -> Option<String> {
    macro_rules! warn {
        ($error:expr) => {
            println!(
                "cargo:warning=couldn't execute `{} {}` ({})",
                command,
                arguments.join(" "),
                $error,
            );
        };
    }

    let output = match Command::new(command).args(arguments).output() {
        Ok(output) => output,
        Err(error) => {
            warn!(format!("error: {}", error));
            return None;
        }
    };

    if !output.status.success() {
        warn!(format!("exit code: {}", output.status));
        return None;
    }

    Some(String::from_utf8_lossy(&output.stdout).into_owned())
}

/// Executes `llvm-config`, returning the `stdout` output if the command was
/// successfully executed.
pub fn run_llvm_config(arguments: &[&str]) -> Option<String> {
    let path = env::var("LLVM_CONFIG_PATH").unwrap_or_else(|_| "llvm-config".into());

    let output = run_command(&path, arguments);
    if output.is_none() {
        println!("cargo:warning=set the LLVM_CONFIG_PATH environment variable to a valid `llvm-config` executable");
    }

    output
}

/// Returns the paths to and the filenames of the files matching the supplied
/// filename patterns in the supplied directory.
fn search_directory(directory: &Path, filenames: &[String]) -> Vec<(PathBuf, String)> {
    // Join the directory to the filename patterns to obtain the path patterns.
    let paths = filenames
        .iter()
        .filter_map(|f| directory.join(f).to_str().map(ToOwned::to_owned));

    // Prevent wildcards from matching path separators.
    let mut options = MatchOptions::new();
    options.require_literal_separator = true;

    paths
        .flat_map(|p| {
            if let Ok(paths) = glob::glob_with(&p, options) {
                paths.filter_map(Result::ok).collect()
            } else {
                vec![]
            }
        })
        .filter_map(|p| {
            let filename = p.file_name().and_then(|f| f.to_str())?;

            // The `libclang_shared` library has been renamed to `libclang-cpp`
            // in Clang 10. This can cause instances of this library (e.g.,
            // `libclang-cpp.so.10`) to be matched by patterns looking for
            // instances of `libclang`.
            if filename.contains("-cpp.") {
                return None;
            }

            Some((directory.to_owned(), filename.into()))
        })
        .collect::<Vec<_>>()
}

/// Returns the paths to and the filenames of the files matching the supplied
/// filename patterns in the supplied directory, checking any relevant sibling
/// directories.
fn search_directories(directory: &Path, filenames: &[String]) -> Vec<(PathBuf, String)> {
    let mut results = search_directory(directory, filenames);

    // On Windows, `libclang.dll` is usually found in the LLVM `bin` directory
    // while `libclang.lib` is usually found in the LLVM `lib` directory. To
    // keep things consistent with other platforms, only LLVM `lib` directories
    // are included in the backup search directory globs so we need to search
    // the LLVM `bin` directory here.
    if cfg!(target_os = "windows") && directory.ends_with("lib") {
        let sibling = directory.parent().unwrap().join("bin");
        results.extend(search_directory(&sibling, filenames).into_iter());
    }

    results
}

/// Returns the paths to and the filenames of the `libclang` static or dynamic
/// libraries matching the supplied filename patterns.
pub fn search_libclang_directories(files: &[String], variable: &str) -> Vec<(PathBuf, String)> {
    // Use the path provided by the relevant environment variable.
    if let Ok(path) = env::var(variable).map(|d| Path::new(&d).to_path_buf()) {
        // Check if the path is referring to a matching file already.
        if let Some(parent) = path.parent() {
            let filename = path.file_name().unwrap().to_str().unwrap();
            let libraries = search_directories(parent, files);
            if libraries.iter().any(|(_, f)| f == filename) {
                return vec![(parent.into(), filename.into())];
            }
        }

        return search_directories(&path, files);
    }

    let mut found = vec![];

    // Search the `bin` and `lib` directories in directory provided by
    // `llvm-config --prefix`.
    if let Some(output) = run_llvm_config(&["--prefix"]) {
        let directory = Path::new(output.lines().next().unwrap()).to_path_buf();
        found.extend(search_directories(&directory.join("bin"), files));
        found.extend(search_directories(&directory.join("lib"), files));
        found.extend(search_directories(&directory.join("lib64"), files));
    }

    // Search the toolchain directory in the directory provided by
    // `xcode-select --print-path`.
    if cfg!(target_os = "macos") {
        if let Some(output) = run_command("xcode-select", &["--print-path"]) {
            let directory = Path::new(output.lines().next().unwrap()).to_path_buf();
            let directory = directory.join("Toolchains/XcodeDefault.xctoolchain/usr/lib");
            found.extend(search_directories(&directory, files));
        }
    }

    // Search the directories provided by the `LD_LIBRARY_PATH` environment
    // variable.
    if let Ok(path) = env::var("LD_LIBRARY_PATH") {
        for directory in path.split(':').map(Path::new) {
            found.extend(search_directories(&directory, files));
        }
    }

    // Determine the `libclang` directory patterns.
    let directories = if cfg!(any(target_os = "freebsd", target_os = "linux")) {
        DIRECTORIES_LINUX
    } else if cfg!(target_os = "macos") {
        DIRECTORIES_MACOS
    } else if cfg!(target_os = "windows") {
        DIRECTORIES_WINDOWS
    } else {
        &[]
    };

    // Search the directories provided by the `libclang` directory patterns.
    let mut options = MatchOptions::new();
    options.case_sensitive = false;
    options.require_literal_separator = true;
    for directory in directories.iter().rev() {
        if let Ok(directories) = glob::glob_with(directory, options) {
            for directory in directories.filter_map(Result::ok).filter(|p| p.is_dir()) {
                found.extend(search_directories(&directory, files));
            }
        }
    }

    found
}
