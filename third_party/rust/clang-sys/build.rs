// Copyright 2016 Kyle Mayes
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

//! Finds the required `libclang` libraries and links to them.
//!
//! # Environment Variables
//!
//! This build script can make use of several environment variables to help it find the required
//! static or dynamic libraries.
//!
//! * `LLVM_CONFIG_PATH` - provides a path to an `llvm-config` executable
//! * `LIBCLANG_PATH` - provides a path to a directory containing a `libclang` shared library
//! * `LIBCLANG_STATIC_PATH` - provides a path to a directory containing LLVM and Clang static libraries

#![allow(unused_attributes)]

extern crate glob;

use std::env;
use std::fs::{self, File};
use std::io::{Read, Seek, SeekFrom};
use std::path::{Path, PathBuf};
use std::process::{Command};

use glob::{MatchOptions};

/// Returns the version in the supplied file if one can be found.
fn find_version(file: &str) -> Option<&str> {
    if file.starts_with("libclang.so.") {
        Some(&file[12..])
    } else if file.starts_with("libclang-") {
        Some(&file[9..])
    } else {
        None
    }
}

/// Returns the components of the version appended to the supplied file.
fn parse_version(file: &Path) -> Vec<u32> {
    let file = file.file_name().and_then(|f| f.to_str()).unwrap_or("");
    let version = find_version(file).unwrap_or("");
    version.split('.').map(|s| s.parse::<u32>().unwrap_or(0)).collect()
}

/// Returns a path to one of the supplied files if such a file can be found in the supplied directory.
fn contains(directory: &Path, files: &[String]) -> Option<PathBuf> {
    // Join the directory to the files to obtain our glob patterns.
    let patterns = files.iter().filter_map(|f| directory.join(f).to_str().map(ToOwned::to_owned));

    // Prevent wildcards from matching path separators.
    let mut options = MatchOptions::new();
    options.require_literal_separator = true;

    // Collect any files that match the glob patterns.
    let mut matches = patterns.flat_map(|p| {
        if let Ok(paths) = glob::glob_with(&p, &options) {
            paths.filter_map(Result::ok).collect()
        } else {
            vec![]
        }
    }).collect::<Vec<_>>();

    // Sort the matches by their version, preferring shorter and higher versions.
    matches.sort_by_key(|m| parse_version(m));
    matches.pop()
}

/// Runs a console command, returning the output if the command was successfully executed.
fn run(command: &str, arguments: &[&str]) -> Option<String> {
    Command::new(command).args(arguments).output().map(|o| {
        String::from_utf8_lossy(&o.stdout).into_owned()
    }).ok()
}

/// Runs `llvm-config`, returning the output if the command was successfully executed.
fn run_llvm_config(arguments: &[&str]) -> Result<String, String> {
    match run(&env::var("LLVM_CONFIG_PATH").unwrap_or_else(|_| "llvm-config".into()), arguments) {
        Some(output) => Ok(output),
        None => {
            let message = format!(
                "couldn't execute `llvm-config {}`, set the LLVM_CONFIG_PATH environment variable \
                to a path to a valid `llvm-config` executable",
                arguments.join(" "),
            );
            Err(message)
        },
    }
}

/// Backup search directory globs for FreeBSD and Linux.
const SEARCH_LINUX: &[&str] = &[
    "/usr/lib*",
    "/usr/lib*/*",
    "/usr/lib*/*/*",
    "/usr/local/lib*",
    "/usr/local/lib*/*",
    "/usr/local/lib*/*/*",
    "/usr/local/llvm*/lib",
];

/// Backup search directory globs for OS X.
const SEARCH_OSX: &[&str] = &[
    "/usr/local/opt/llvm*/lib",
    "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib",
    "/Library/Developer/CommandLineTools/usr/lib",
    "/usr/local/opt/llvm*/lib/llvm*/lib",
];

/// Backup search directory globs for Windows.
const SEARCH_WINDOWS: &[&str] = &[
    "C:\\LLVM\\lib",
    "C:\\Program Files*\\LLVM\\lib",
    "C:\\MSYS*\\MinGW*\\lib",
];

/// Returns the ELF class from the ELF header in the supplied file.
fn parse_elf_header(file: &PathBuf) -> Result<u8, String> {
    let mut file = try!(File::open(file).map_err(|e| e.to_string()));
    let mut elf = [0; 5];
    try!(file.read_exact(&mut elf).map_err(|e| e.to_string()));
    if elf[..4] == [127, 69, 76, 70] {
        Ok(elf[4])
    } else {
        Err("invalid ELF header".into())
    }
}

/// Returns the magic number from the PE header in the supplied file.
fn parse_pe_header(file: &PathBuf) -> Result<u16, String> {
    let mut file = try!(File::open(file).map_err(|e| e.to_string()));
    let mut pe = [0; 4];

    // Determine the header offset.
    try!(file.seek(SeekFrom::Start(0x3C)).map_err(|e| e.to_string()));
    try!(file.read_exact(&mut pe).map_err(|e| e.to_string()));
    let offset = i32::from(pe[0]) + (i32::from(pe[1]) << 8) + (i32::from(pe[2]) << 16) + (i32::from(pe[3]) << 24);

    // Determine the validity of the header.
    try!(file.seek(SeekFrom::Start(offset as u64)).map_err(|e| e.to_string()));
    try!(file.read_exact(&mut pe).map_err(|e| e.to_string()));
    if pe != [80, 69, 0, 0] {
        return Err("invalid PE header".into());
    }

    // Find the magic number.
    try!(file.seek(SeekFrom::Current(20)).map_err(|e| e.to_string()));
    try!(file.read_exact(&mut pe).map_err(|e| e.to_string()));
    Ok(u16::from(pe[0]) + (u16::from(pe[1]) << 8))
}

/// Indicates the type of library being searched for.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
enum Library {
    Dynamic,
    Static,
}

impl Library {
    /// Checks whether the supplied file is a valid library for the architecture.
    fn check(&self, file: &PathBuf) -> Result<(), String> {
        if cfg!(any(target_os="freebsd", target_os="linux")) {
            if *self == Library::Static {
                return Ok(());
            }
            let class = try!(parse_elf_header(file));
            if cfg!(target_pointer_width="32") && class != 1 {
                return Err("invalid ELF class (64-bit)".into());
            }
            if cfg!(target_pointer_width="64") && class != 2 {
                return Err("invalid ELF class (32-bit)".into());
            }
            Ok(())
        } else if cfg!(target_os="windows") {
            if *self == Library::Static {
                return Ok(());
            }
            let magic = try!(parse_pe_header(file));
            if cfg!(target_pointer_width="32") && magic != 267 {
                return Err("invalid DLL (64-bit)".into());
            }
            if cfg!(target_pointer_width="64") && magic != 523 {
                return Err("invalid DLL (32-bit)".into());
            }
            Ok(())
        } else {
            Ok(())
        }
    }
}

/// Searches for a library, returning the directory it can be found in if the search was successful.
fn find(library: Library, files: &[String], env: &str) -> Result<PathBuf, String> {
    let mut skipped = vec![];

    /// Attempts to return the supplied file.
    macro_rules! try_file {
        ($file:expr) => ({
            match library.check(&$file) {
                Ok(_) => return Ok($file),
                Err(message) => skipped.push(format!("({}: {})", $file.display(), message)),
            }
        });
    }

    /// Searches the supplied directory and, on Windows, any relevant sibling directories.
    macro_rules! search_directory {
        ($directory:ident) => {
            if let Some(file) = contains(&$directory, files) {
                try_file!(file);
            }

            // On Windows, `libclang.dll` is usually found in the LLVM `bin` directory while
            // `libclang.lib` is usually found in the LLVM `lib` directory. To keep things
            // consistent with other platforms, only LLVM `lib` directories are included in the
            // backup search directory globs so we need to search the LLVM `bin` directory here.
            if cfg!(target_os="windows") && $directory.ends_with("lib") {
                let sibling = $directory.parent().unwrap().join("bin");
                if let Some(file) = contains(&sibling, files) {
                    try_file!(file);
                }
            }
        }
    }

    // Search the directory provided by the relevant environment variable if it is set.
    if let Ok(directory) = env::var(env).map(|d| Path::new(&d).to_path_buf()) {
        search_directory!(directory);
    }

    // Search the `bin` and `lib` subdirectories in the directory returned by
    // `llvm-config --prefix` if `llvm-config` is available.
    if let Ok(output) = run_llvm_config(&["--prefix"]) {
        let directory = Path::new(output.lines().next().unwrap()).to_path_buf();
        let bin = directory.join("bin");
        if let Some(file) = contains(&bin, files) {
            try_file!(file);
        }
        let lib = directory.join("lib");
        if let Some(file) = contains(&lib, files) {
            try_file!(file);
        }
    }

    // Search the `LD_LIBRARY_PATH` directories.
    if let Ok(path) = env::var("LD_LIBRARY_PATH") {
        for directory in path.split(':').map(Path::new) {
            search_directory!(directory);
        }
    }

    // Search the backup directories.
    let search = if cfg!(any(target_os="freebsd", target_os="linux")) {
        SEARCH_LINUX
    } else if cfg!(target_os="macos") {
        SEARCH_OSX
    } else if cfg!(target_os="windows") {
        SEARCH_WINDOWS
    } else {
        &[]
    };
    for pattern in search {
        let mut options = MatchOptions::new();
        options.case_sensitive = false;
        options.require_literal_separator = true;
        if let Ok(paths) = glob::glob_with(pattern, &options) {
            for path in paths.filter_map(Result::ok).filter(|p| p.is_dir()) {
                search_directory!(path);
            }
        }
    }

    let message = format!(
        "couldn't find any of [{}], set the {} environment variable to a path where one of these \
         files can be found (skipped: [{}])",
        files.iter().map(|f| format!("'{}'", f)).collect::<Vec<_>>().join(", "),
        env,
        skipped.join(", "),
    );
    Err(message)
}

/// Searches for a `libclang` shared library, returning the path to such a shared library if the
/// search was successful.
pub fn find_shared_library() -> Result<PathBuf, String> {
    let mut files = vec![format!("{}clang{}", env::consts::DLL_PREFIX, env::consts::DLL_SUFFIX)];
    if cfg!(any(target_os="freebsd", target_os="linux", target_os="openbsd")) {
        // Some BSDs and Linux distributions don't create a `libclang.so` symlink, so we need to
        // look for any versioned files (e.g., `libclang.so.3.9` or `libclang-3.9.so`).
        files.push("libclang.so.*".into());
        files.push("libclang-*.so".into());
    }
    if cfg!(target_os="windows") {
        // The official LLVM build uses `libclang.dll` on Windows instead of `clang.dll`. However,
        // unofficial builds such as MinGW use `clang.dll`.
        files.push("libclang.dll".into());
    }
    find(Library::Dynamic, &files, "LIBCLANG_PATH")
}

/// Returns the name of an LLVM or Clang library from a path to such a library.
fn get_library_name(path: &Path) -> Option<String> {
    path.file_stem().map(|p| {
        let string = p.to_string_lossy();
        if string.starts_with("lib") {
            string[3..].to_owned()
        } else {
            string.to_string()
        }
    })
}

/// Returns the LLVM libraries required to link to `libclang` statically.
fn get_llvm_libraries() -> Vec<String> {
    run_llvm_config(&["--libs"]).unwrap().split_whitespace().filter_map(|p| {
        // Depending on the version of `llvm-config` in use, listed libraries may be in one of two
        // forms, a full path to the library or simply prefixed with `-l`.
        if p.starts_with("-l") {
            Some(p[2..].into())
        } else {
            get_library_name(Path::new(p))
        }
    }).collect()
}

/// Clang libraries required to link to `libclang` 3.5 and later statically.
const CLANG_LIBRARIES: &[&str] = &[
    "clang",
    "clangAST",
    "clangAnalysis",
    "clangBasic",
    "clangDriver",
    "clangEdit",
    "clangFrontend",
    "clangIndex",
    "clangLex",
    "clangParse",
    "clangRewrite",
    "clangSema",
    "clangSerialization",
];

/// Returns the Clang libraries required to link to `libclang` statically.
fn get_clang_libraries<P: AsRef<Path>>(directory: P) -> Vec<String> {
    let pattern = directory.as_ref().join("libclang*.a").to_string_lossy().to_string();
    if let Ok(libraries) = glob::glob(&pattern) {
        libraries.filter_map(|l| l.ok().and_then(|l| get_library_name(&l))).collect()
    } else {
        CLANG_LIBRARIES.iter().map(|l| l.to_string()).collect()
    }
}

/// Find and link to `libclang` statically.
#[cfg_attr(feature="runtime", allow(dead_code))]
fn link_static() {
    let name = if cfg!(target_os="windows") { "libclang.lib" } else { "libclang.a" };
    let file = find(Library::Static, &[name.into()], "LIBCLANG_STATIC_PATH").unwrap();
    let directory = file.parent().unwrap();

    // Specify required Clang static libraries.
    println!("cargo:rustc-link-search=native={}", directory.display());
    for library in get_clang_libraries(directory) {
        println!("cargo:rustc-link-lib=static={}", library);
    }

    // Determine the shared mode used by LLVM.
    let mode = run_llvm_config(&["--shared-mode"]).map(|m| m.trim().to_owned());
    let prefix = if mode.ok().map_or(false, |m| m == "static") { "static=" } else { "" };

    // Specify required LLVM static libraries.
    println!("cargo:rustc-link-search=native={}", run_llvm_config(&["--libdir"]).unwrap().trim_right());
    for library in get_llvm_libraries() {
        println!("cargo:rustc-link-lib={}{}", prefix, library);
    }

    // Specify required system libraries.
    // MSVC doesn't need this, as it tracks dependencies inside `.lib` files.
    if cfg!(target_os="freebsd") {
        println!("cargo:rustc-flags=-l ffi -l ncursesw -l c++ -l z");
    } else if cfg!(target_os="linux") {
        println!("cargo:rustc-flags=-l ffi -l ncursesw -l stdc++ -l z");
    } else if cfg!(target_os="macos") {
        println!("cargo:rustc-flags=-l ffi -l ncurses -l c++ -l z");
    }
}

/// Find and link to `libclang` dynamically.
#[cfg_attr(feature="runtime", allow(dead_code))]
fn link_dynamic() {
    let file = find_shared_library().unwrap();
    let directory = file.parent().unwrap();
    println!("cargo:rustc-link-search={}", directory.display());

    if cfg!(all(target_os="windows", target_env="msvc")) {
        // Find the `libclang` stub static library required for the MSVC toolchain.
        let libdir = if !directory.ends_with("bin") {
            directory.to_owned()
        } else {
            directory.parent().unwrap().join("lib")
        };
        if libdir.join("libclang.lib").exists() {
            println!("cargo:rustc-link-search={}", libdir.display());
        } else if libdir.join("libclang.dll.a").exists() {
            // MSYS and MinGW use `libclang.dll.a` instead of `libclang.lib`. It is linkable with
            // the MSVC linker, but Rust doesn't recognize the `.a` suffix, so we need to copy it
            // with a different name.
            //
            // FIXME: Maybe we can just hardlink or symlink it?
            let out = env::var("OUT_DIR").unwrap();
            fs::copy(libdir.join("libclang.dll.a"), Path::new(&out).join("libclang.lib")).unwrap();
            println!("cargo:rustc-link-search=native={}", out);
        } else {
            panic!(
                "using '{}', so 'libclang.lib' or 'libclang.dll.a' must be available in {}",
                file.display(),
                libdir.display(),
            );
        }
        println!("cargo:rustc-link-lib=dylib=libclang");
    } else {
        println!("cargo:rustc-link-lib=dylib=clang");
    }
}

#[cfg_attr(feature="runtime", allow(dead_code))]
fn main() {
    if cfg!(feature="runtime") {
        if cfg!(feature="static") {
            panic!("`runtime` and `static` features can't be combined");
        }
    } else if cfg!(feature="static") {
        link_static();
    } else {
        link_dynamic();
    }

    if let Ok(output) = run_llvm_config(&["--includedir"]) {
        let directory = Path::new(output.trim_right());
        println!("cargo:include={}", directory.display());
    }
}
