//! A library for build scripts to compile custom C code
//!
//! This library is intended to be used as a `build-dependencies` entry in
//! `Cargo.toml`:
//!
//! ```toml
//! [build-dependencies]
//! cc = "1.0"
//! ```
//!
//! The purpose of this crate is to provide the utility functions necessary to
//! compile C code into a static archive which is then linked into a Rust crate.
//! Configuration is available through the `Build` struct.
//!
//! This crate will automatically detect situations such as cross compilation or
//! other environment variables set by Cargo and will build code appropriately.
//!
//! The crate is not limited to C code, it can accept any source code that can
//! be passed to a C or C++ compiler. As such, assembly files with extensions
//! `.s` (gcc/clang) and `.asm` (MSVC) can also be compiled.
//!
//! [`Build`]: struct.Build.html
//!
//! # Parallelism
//!
//! To parallelize computation, enable the `parallel` feature for the crate.
//!
//! ```toml
//! [build-dependencies]
//! cc = { version = "1.0", features = ["parallel"] }
//! ```
//! To specify the max number of concurrent compilation jobs, set the `NUM_JOBS`
//! environment variable to the desired amount.
//!
//! Cargo will also set this environment variable when executed with the `-jN` flag.
//!
//! If `NUM_JOBS` is not set, the `RAYON_NUM_THREADS` environment variable can
//! also specify the build paralellism.
//!
//! # Examples
//!
//! Use the `Build` struct to compile `src/foo.c`:
//!
//! ```no_run
//! extern crate cc;
//!
//! fn main() {
//!     cc::Build::new()
//!         .file("src/foo.c")
//!         .define("FOO", Some("bar"))
//!         .include("src")
//!         .compile("foo");
//! }
//! ```

#![doc(html_root_url = "https://docs.rs/cc/1.0")]
#![cfg_attr(test, deny(warnings))]
#![allow(deprecated)]
#![deny(missing_docs)]

#[cfg(feature = "parallel")]
extern crate rayon;

use std::env;
use std::ffi::{OsStr, OsString};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::io::{self, BufRead, BufReader, Read, Write};
use std::thread::{self, JoinHandle};
use std::collections::HashMap;
use std::sync::{Arc, Mutex};

// These modules are all glue to support reading the MSVC version from
// the registry and from COM interfaces
#[cfg(windows)]
mod registry;
#[cfg(windows)]
#[macro_use]
mod winapi;
#[cfg(windows)]
mod com;
#[cfg(windows)]
mod setup_config;

pub mod windows_registry;

/// A builder for compilation of a native static library.
///
/// A `Build` is the main type of the `cc` crate and is used to control all the
/// various configuration options and such of a compile. You'll find more
/// documentation on each method itself.
#[derive(Clone, Debug)]
pub struct Build {
    include_directories: Vec<PathBuf>,
    definitions: Vec<(String, Option<String>)>,
    objects: Vec<PathBuf>,
    flags: Vec<String>,
    flags_supported: Vec<String>,
    known_flag_support_status: Arc<Mutex<HashMap<String, bool>>>,
    files: Vec<PathBuf>,
    cpp: bool,
    cpp_link_stdlib: Option<Option<String>>,
    cpp_set_stdlib: Option<String>,
    cuda: bool,
    target: Option<String>,
    host: Option<String>,
    out_dir: Option<PathBuf>,
    opt_level: Option<String>,
    debug: Option<bool>,
    env: Vec<(OsString, OsString)>,
    compiler: Option<PathBuf>,
    archiver: Option<PathBuf>,
    cargo_metadata: bool,
    pic: Option<bool>,
    use_plt: Option<bool>,
    static_crt: Option<bool>,
    shared_flag: Option<bool>,
    static_flag: Option<bool>,
    warnings_into_errors: bool,
    warnings: Option<bool>,
    extra_warnings: Option<bool>,
    env_cache: Arc<Mutex<HashMap<String, Option<String>>>>,
}

/// Represents the types of errors that may occur while using cc-rs.
#[derive(Clone, Debug)]
enum ErrorKind {
    /// Error occurred while performing I/O.
    IOError,
    /// Invalid architecture supplied.
    ArchitectureInvalid,
    /// Environment variable not found, with the var in question as extra info.
    EnvVarNotFound,
    /// Error occurred while using external tools (ie: invocation of compiler).
    ToolExecError,
    /// Error occurred due to missing external tools.
    ToolNotFound,
}

/// Represents an internal error that occurred, with an explanation.
#[derive(Clone, Debug)]
pub struct Error {
    /// Describes the kind of error that occurred.
    kind: ErrorKind,
    /// More explanation of error that occurred.
    message: String,
}

impl Error {
    fn new(kind: ErrorKind, message: &str) -> Error {
        Error {
            kind: kind,
            message: message.to_owned(),
        }
    }
}

impl From<io::Error> for Error {
    fn from(e: io::Error) -> Error {
        Error::new(ErrorKind::IOError, &format!("{}", e))
    }
}

/// Configuration used to represent an invocation of a C compiler.
///
/// This can be used to figure out what compiler is in use, what the arguments
/// to it are, and what the environment variables look like for the compiler.
/// This can be used to further configure other build systems (e.g. forward
/// along CC and/or CFLAGS) or the `to_command` method can be used to run the
/// compiler itself.
#[derive(Clone, Debug)]
pub struct Tool {
    path: PathBuf,
    cc_wrapper_path: Option<PathBuf>,
    cc_wrapper_args: Vec<OsString>,
    args: Vec<OsString>,
    env: Vec<(OsString, OsString)>,
    family: ToolFamily,
    cuda: bool,
    removed_args: Vec<OsString>,
}

/// Represents the family of tools this tool belongs to.
///
/// Each family of tools differs in how and what arguments they accept.
///
/// Detection of a family is done on best-effort basis and may not accurately reflect the tool.
#[derive(Copy, Clone, Debug, PartialEq)]
enum ToolFamily {
    /// Tool is GNU Compiler Collection-like.
    Gnu,
    /// Tool is Clang-like. It differs from the GCC in a sense that it accepts superset of flags
    /// and its cross-compilation approach is different.
    Clang,
    /// Tool is the MSVC cl.exe.
    Msvc { clang_cl: bool },
}

impl ToolFamily {
    /// What the flag to request debug info for this family of tools look like
    fn add_debug_flags(&self, cmd: &mut Tool) {
        match *self {
            ToolFamily::Msvc { .. } => {
                cmd.push_cc_arg("/Z7".into());
            }
            ToolFamily::Gnu | ToolFamily::Clang => {
                cmd.push_cc_arg("-g".into());
                cmd.push_cc_arg("-fno-omit-frame-pointer".into());
            }
        }
    }

    /// What the flag to include directories into header search path looks like
    fn include_flag(&self) -> &'static str {
        match *self {
            ToolFamily::Msvc { .. } => "/I",
            ToolFamily::Gnu | ToolFamily::Clang => "-I",
        }
    }

    /// What the flag to request macro-expanded source output looks like
    fn expand_flag(&self) -> &'static str {
        match *self {
            ToolFamily::Msvc { .. } => "/E",
            ToolFamily::Gnu | ToolFamily::Clang => "-E",
        }
    }

    /// What the flags to enable all warnings
    fn warnings_flags(&self) -> &'static str {
        match *self {
            ToolFamily::Msvc { .. } => "/W4",
            ToolFamily::Gnu | ToolFamily::Clang => "-Wall",
        }
    }

    /// What the flags to enable extra warnings
    fn extra_warnings_flags(&self) -> Option<&'static str> {
        match *self {
            ToolFamily::Msvc { .. } => None,
            ToolFamily::Gnu | ToolFamily::Clang => Some("-Wextra"),
        }
    }

    /// What the flag to turn warning into errors
    fn warnings_to_errors_flag(&self) -> &'static str {
        match *self {
            ToolFamily::Msvc { .. } => "/WX",
            ToolFamily::Gnu | ToolFamily::Clang => "-Werror",
        }
    }

    /// NVCC-specific. Device code debug info flag. This is separate from the
    /// debug info flag passed to the C++ compiler.
    fn nvcc_debug_flag(&self) -> &'static str {
        match *self {
            ToolFamily::Msvc { .. } => unimplemented!(),
            ToolFamily::Gnu | ToolFamily::Clang => "-G",
        }
    }

    /// NVCC-specific. Redirect the following flag to the underlying C++
    /// compiler.
    fn nvcc_redirect_flag(&self) -> &'static str {
        match *self {
            ToolFamily::Msvc { .. } => unimplemented!(),
            ToolFamily::Gnu | ToolFamily::Clang => "-Xcompiler",
        }
    }

    fn verbose_stderr(&self) -> bool {
        *self == ToolFamily::Clang
    }
}

/// Represents an object.
///
/// This is a source file -> object file pair.
#[derive(Clone, Debug)]
struct Object {
    src: PathBuf,
    dst: PathBuf,
}

impl Object {
    /// Create a new source file -> object file pair.
    fn new(src: PathBuf, dst: PathBuf) -> Object {
        Object { src: src, dst: dst }
    }
}

impl Build {
    /// Construct a new instance of a blank set of configuration.
    ///
    /// This builder is finished with the [`compile`] function.
    ///
    /// [`compile`]: struct.Build.html#method.compile
    pub fn new() -> Build {
        Build {
            include_directories: Vec::new(),
            definitions: Vec::new(),
            objects: Vec::new(),
            flags: Vec::new(),
            flags_supported: Vec::new(),
            known_flag_support_status: Arc::new(Mutex::new(HashMap::new())),
            files: Vec::new(),
            shared_flag: None,
            static_flag: None,
            cpp: false,
            cpp_link_stdlib: None,
            cpp_set_stdlib: None,
            cuda: false,
            target: None,
            host: None,
            out_dir: None,
            opt_level: None,
            debug: None,
            env: Vec::new(),
            compiler: None,
            archiver: None,
            cargo_metadata: true,
            pic: None,
            use_plt: None,
            static_crt: None,
            warnings: None,
            extra_warnings: None,
            warnings_into_errors: false,
            env_cache: Arc::new(Mutex::new(HashMap::new())),
        }
    }

    /// Add a directory to the `-I` or include path for headers
    ///
    /// # Example
    ///
    /// ```no_run
    /// use std::path::Path;
    ///
    /// let library_path = Path::new("/path/to/library");
    ///
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .include(library_path)
    ///     .include("src")
    ///     .compile("foo");
    /// ```
    pub fn include<P: AsRef<Path>>(&mut self, dir: P) -> &mut Build {
        self.include_directories.push(dir.as_ref().to_path_buf());
        self
    }

    /// Specify a `-D` variable with an optional value.
    ///
    /// # Example
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .define("FOO", "BAR")
    ///     .define("BAZ", None)
    ///     .compile("foo");
    /// ```
    pub fn define<'a, V: Into<Option<&'a str>>>(&mut self, var: &str, val: V) -> &mut Build {
        self.definitions
            .push((var.to_string(), val.into().map(|s| s.to_string())));
        self
    }

    /// Add an arbitrary object file to link in
    pub fn object<P: AsRef<Path>>(&mut self, obj: P) -> &mut Build {
        self.objects.push(obj.as_ref().to_path_buf());
        self
    }

    /// Add an arbitrary flag to the invocation of the compiler
    ///
    /// # Example
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .flag("-ffunction-sections")
    ///     .compile("foo");
    /// ```
    pub fn flag(&mut self, flag: &str) -> &mut Build {
        self.flags.push(flag.to_string());
        self
    }

    fn ensure_check_file(&self) -> Result<PathBuf, Error> {
        let out_dir = self.get_out_dir()?;
        let src = if self.cuda {
            assert!(self.cpp);
            out_dir.join("flag_check.cu")
        } else if self.cpp {
            out_dir.join("flag_check.cpp")
        } else {
            out_dir.join("flag_check.c")
        };

        if !src.exists() {
            let mut f = fs::File::create(&src)?;
            write!(f, "int main(void) {{ return 0; }}")?;
        }

        Ok(src)
    }

    /// Run the compiler to test if it accepts the given flag.
    ///
    /// For a convenience method for setting flags conditionally,
    /// see `flag_if_supported()`.
    ///
    /// It may return error if it's unable to run the compilier with a test file
    /// (e.g. the compiler is missing or a write to the `out_dir` failed).
    ///
    /// Note: Once computed, the result of this call is stored in the
    /// `known_flag_support` field. If `is_flag_supported(flag)`
    /// is called again, the result will be read from the hash table.
    pub fn is_flag_supported(&self, flag: &str) -> Result<bool, Error> {
        let mut known_status = self.known_flag_support_status.lock().unwrap();
        if let Some(is_supported) = known_status.get(flag).cloned() {
            return Ok(is_supported);
        }

        let out_dir = self.get_out_dir()?;
        let src = self.ensure_check_file()?;
        let obj = out_dir.join("flag_check");
        let target = self.get_target()?;
        let host = self.get_host()?;
        let mut cfg = Build::new();
        cfg.flag(flag)
            .target(&target)
            .opt_level(0)
            .host(&host)
            .debug(false)
            .cpp(self.cpp)
            .cuda(self.cuda);
        let mut compiler = cfg.try_get_compiler()?;

        // Clang uses stderr for verbose output, which yields a false positive
        // result if the CFLAGS/CXXFLAGS include -v to aid in debugging.
        if compiler.family.verbose_stderr() {
            compiler.remove_arg("-v".into());
        }

        let mut cmd = compiler.to_command();
        let is_arm = target.contains("aarch64") || target.contains("arm");
        command_add_output_file(&mut cmd, &obj, target.contains("msvc"), false, is_arm);

        // We need to explicitly tell msvc not to link and create an exe
        // in the root directory of the crate
        if target.contains("msvc") {
            cmd.arg("/c");
        }

        cmd.arg(&src);

        let output = cmd.output()?;
        let is_supported = output.stderr.is_empty();

        known_status.insert(flag.to_owned(), is_supported);
        Ok(is_supported)
    }

    /// Add an arbitrary flag to the invocation of the compiler if it supports it
    ///
    /// # Example
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .flag_if_supported("-Wlogical-op") // only supported by GCC
    ///     .flag_if_supported("-Wunreachable-code") // only supported by clang
    ///     .compile("foo");
    /// ```
    pub fn flag_if_supported(&mut self, flag: &str) -> &mut Build {
        self.flags_supported.push(flag.to_string());
        self
    }

    /// Set the `-shared` flag.
    ///
    /// When enabled, the compiler will produce a shared object which can
    /// then be linked with other objects to form an executable.
    ///
    /// # Example
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .shared_flag(true)
    ///     .compile("libfoo.so");
    /// ```

    pub fn shared_flag(&mut self, shared_flag: bool) -> &mut Build {
        self.shared_flag = Some(shared_flag);
        self
    }

    /// Set the `-static` flag.
    ///
    /// When enabled on systems that support dynamic linking, this prevents
    /// linking with the shared libraries.
    ///
    /// # Example
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .shared_flag(true)
    ///     .static_flag(true)
    ///     .compile("foo");
    /// ```
    pub fn static_flag(&mut self, static_flag: bool) -> &mut Build {
        self.static_flag = Some(static_flag);
        self
    }

    /// Add a file which will be compiled
    pub fn file<P: AsRef<Path>>(&mut self, p: P) -> &mut Build {
        self.files.push(p.as_ref().to_path_buf());
        self
    }

    /// Add files which will be compiled
    pub fn files<P>(&mut self, p: P) -> &mut Build
    where
        P: IntoIterator,
        P::Item: AsRef<Path>,
    {
        for file in p.into_iter() {
            self.file(file);
        }
        self
    }

    /// Set C++ support.
    ///
    /// The other `cpp_*` options will only become active if this is set to
    /// `true`.
    pub fn cpp(&mut self, cpp: bool) -> &mut Build {
        self.cpp = cpp;
        self
    }

    /// Set CUDA C++ support.
    ///
    /// Enabling CUDA will pass the detected C/C++ toolchain as an argument to
    /// the CUDA compiler, NVCC. NVCC itself accepts some limited GNU-like args;
    /// any other arguments for the C/C++ toolchain will be redirected using
    /// "-Xcompiler" flags.
    ///
    /// If enabled, this also implicitly enables C++ support.
    pub fn cuda(&mut self, cuda: bool) -> &mut Build {
        self.cuda = cuda;
        if cuda {
            self.cpp = true;
        }
        self
    }

    /// Set warnings into errors flag.
    ///
    /// Disabled by default.
    ///
    /// Warning: turning warnings into errors only make sense
    /// if you are a developer of the crate using cc-rs.
    /// Some warnings only appear on some architecture or
    /// specific version of the compiler. Any user of this crate,
    /// or any other crate depending on it, could fail during
    /// compile time.
    ///
    /// # Example
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .warnings_into_errors(true)
    ///     .compile("libfoo.a");
    /// ```
    pub fn warnings_into_errors(&mut self, warnings_into_errors: bool) -> &mut Build {
        self.warnings_into_errors = warnings_into_errors;
        self
    }

    /// Set warnings flags.
    ///
    /// Adds some flags:
    /// - "/Wall" for MSVC.
    /// - "-Wall", "-Wextra" for GNU and Clang.
    ///
    /// Enabled by default.
    ///
    /// # Example
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .warnings(false)
    ///     .compile("libfoo.a");
    /// ```
    pub fn warnings(&mut self, warnings: bool) -> &mut Build {
        self.warnings = Some(warnings);
        self.extra_warnings = Some(warnings);
        self
    }

    /// Set extra warnings flags.
    ///
    /// Adds some flags:
    /// - nothing for MSVC.
    /// - "-Wextra" for GNU and Clang.
    ///
    /// Enabled by default.
    ///
    /// # Example
    ///
    /// ```no_run
    /// // Disables -Wextra, -Wall remains enabled:
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .extra_warnings(false)
    ///     .compile("libfoo.a");
    /// ```
    pub fn extra_warnings(&mut self, warnings: bool) -> &mut Build {
        self.extra_warnings = Some(warnings);
        self
    }

    /// Set the standard library to link against when compiling with C++
    /// support.
    ///
    /// The default value of this property depends on the current target: On
    /// OS X `Some("c++")` is used, when compiling for a Visual Studio based
    /// target `None` is used and for other targets `Some("stdc++")` is used.
    /// If the `CXXSTDLIB` environment variable is set, its value will
    /// override the default value.
    ///
    /// A value of `None` indicates that no automatic linking should happen,
    /// otherwise cargo will link against the specified library.
    ///
    /// The given library name must not contain the `lib` prefix.
    ///
    /// Common values:
    /// - `stdc++` for GNU
    /// - `c++` for Clang
    ///
    /// # Example
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .shared_flag(true)
    ///     .cpp_link_stdlib("stdc++")
    ///     .compile("libfoo.so");
    /// ```
    pub fn cpp_link_stdlib<'a, V: Into<Option<&'a str>>>(
        &mut self,
        cpp_link_stdlib: V,
    ) -> &mut Build {
        self.cpp_link_stdlib = Some(cpp_link_stdlib.into().map(|s| s.into()));
        self
    }

    /// Force the C++ compiler to use the specified standard library.
    ///
    /// Setting this option will automatically set `cpp_link_stdlib` to the same
    /// value.
    ///
    /// The default value of this option is always `None`.
    ///
    /// This option has no effect when compiling for a Visual Studio based
    /// target.
    ///
    /// This option sets the `-stdlib` flag, which is only supported by some
    /// compilers (clang, icc) but not by others (gcc). The library will not
    /// detect which compiler is used, as such it is the responsibility of the
    /// caller to ensure that this option is only used in conjuction with a
    /// compiler which supports the `-stdlib` flag.
    ///
    /// A value of `None` indicates that no specific C++ standard library should
    /// be used, otherwise `-stdlib` is added to the compile invocation.
    ///
    /// The given library name must not contain the `lib` prefix.
    ///
    /// Common values:
    /// - `stdc++` for GNU
    /// - `c++` for Clang
    ///
    /// # Example
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .cpp_set_stdlib("c++")
    ///     .compile("libfoo.a");
    /// ```
    pub fn cpp_set_stdlib<'a, V: Into<Option<&'a str>>>(
        &mut self,
        cpp_set_stdlib: V,
    ) -> &mut Build {
        let cpp_set_stdlib = cpp_set_stdlib.into();
        self.cpp_set_stdlib = cpp_set_stdlib.map(|s| s.into());
        self.cpp_link_stdlib(cpp_set_stdlib);
        self
    }

    /// Configures the target this configuration will be compiling for.
    ///
    /// This option is automatically scraped from the `TARGET` environment
    /// variable by build scripts, so it's not required to call this function.
    ///
    /// # Example
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .target("aarch64-linux-android")
    ///     .compile("foo");
    /// ```
    pub fn target(&mut self, target: &str) -> &mut Build {
        self.target = Some(target.to_string());
        self
    }

    /// Configures the host assumed by this configuration.
    ///
    /// This option is automatically scraped from the `HOST` environment
    /// variable by build scripts, so it's not required to call this function.
    ///
    /// # Example
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .host("arm-linux-gnueabihf")
    ///     .compile("foo");
    /// ```
    pub fn host(&mut self, host: &str) -> &mut Build {
        self.host = Some(host.to_string());
        self
    }

    /// Configures the optimization level of the generated object files.
    ///
    /// This option is automatically scraped from the `OPT_LEVEL` environment
    /// variable by build scripts, so it's not required to call this function.
    pub fn opt_level(&mut self, opt_level: u32) -> &mut Build {
        self.opt_level = Some(opt_level.to_string());
        self
    }

    /// Configures the optimization level of the generated object files.
    ///
    /// This option is automatically scraped from the `OPT_LEVEL` environment
    /// variable by build scripts, so it's not required to call this function.
    pub fn opt_level_str(&mut self, opt_level: &str) -> &mut Build {
        self.opt_level = Some(opt_level.to_string());
        self
    }

    /// Configures whether the compiler will emit debug information when
    /// generating object files.
    ///
    /// This option is automatically scraped from the `PROFILE` environment
    /// variable by build scripts (only enabled when the profile is "debug"), so
    /// it's not required to call this function.
    pub fn debug(&mut self, debug: bool) -> &mut Build {
        self.debug = Some(debug);
        self
    }

    /// Configures the output directory where all object files and static
    /// libraries will be located.
    ///
    /// This option is automatically scraped from the `OUT_DIR` environment
    /// variable by build scripts, so it's not required to call this function.
    pub fn out_dir<P: AsRef<Path>>(&mut self, out_dir: P) -> &mut Build {
        self.out_dir = Some(out_dir.as_ref().to_owned());
        self
    }

    /// Configures the compiler to be used to produce output.
    ///
    /// This option is automatically determined from the target platform or a
    /// number of environment variables, so it's not required to call this
    /// function.
    pub fn compiler<P: AsRef<Path>>(&mut self, compiler: P) -> &mut Build {
        self.compiler = Some(compiler.as_ref().to_owned());
        self
    }

    /// Configures the tool used to assemble archives.
    ///
    /// This option is automatically determined from the target platform or a
    /// number of environment variables, so it's not required to call this
    /// function.
    pub fn archiver<P: AsRef<Path>>(&mut self, archiver: P) -> &mut Build {
        self.archiver = Some(archiver.as_ref().to_owned());
        self
    }
    /// Define whether metadata should be emitted for cargo allowing it to
    /// automatically link the binary. Defaults to `true`.
    ///
    /// The emitted metadata is:
    ///
    ///  - `rustc-link-lib=static=`*compiled lib*
    ///  - `rustc-link-search=native=`*target folder*
    ///  - When target is MSVC, the ATL-MFC libs are added via `rustc-link-search=native=`
    ///  - When C++ is enabled, the C++ stdlib is added via `rustc-link-lib`
    ///
    pub fn cargo_metadata(&mut self, cargo_metadata: bool) -> &mut Build {
        self.cargo_metadata = cargo_metadata;
        self
    }

    /// Configures whether the compiler will emit position independent code.
    ///
    /// This option defaults to `false` for `windows-gnu` targets and
    /// to `true` for all other targets.
    pub fn pic(&mut self, pic: bool) -> &mut Build {
        self.pic = Some(pic);
        self
    }

    /// Configures whether the Procedure Linkage Table is used for indirect
    /// calls into shared libraries.
    ///
    /// The PLT is used to provide features like lazy binding, but introduces
    /// a small performance loss due to extra pointer indirection. Setting
    /// `use_plt` to `false` can provide a small performance increase.
    ///
    /// Note that skipping the PLT requires a recent version of GCC/Clang.
    ///
    /// This only applies to ELF targets. It has no effect on other platforms.
    pub fn use_plt(&mut self, use_plt: bool) -> &mut Build {
        self.use_plt = Some(use_plt);
        self
    }

    /// Configures whether the /MT flag or the /MD flag will be passed to msvc build tools.
    ///
    /// This option defaults to `false`, and affect only msvc targets.
    pub fn static_crt(&mut self, static_crt: bool) -> &mut Build {
        self.static_crt = Some(static_crt);
        self
    }

    #[doc(hidden)]
    pub fn __set_env<A, B>(&mut self, a: A, b: B) -> &mut Build
    where
        A: AsRef<OsStr>,
        B: AsRef<OsStr>,
    {
        self.env
            .push((a.as_ref().to_owned(), b.as_ref().to_owned()));
        self
    }

    /// Run the compiler, generating the file `output`
    ///
    /// This will return a result instead of panicing; see compile() for the complete description.
    pub fn try_compile(&self, output: &str) -> Result<(), Error> {
        let (lib_name, gnu_lib_name) = if output.starts_with("lib") && output.ends_with(".a") {
            (&output[3..output.len() - 2], output.to_owned())
        } else {
            let mut gnu = String::with_capacity(5 + output.len());
            gnu.push_str("lib");
            gnu.push_str(&output);
            gnu.push_str(".a");
            (output, gnu)
        };
        let dst = self.get_out_dir()?;

        let mut objects = Vec::new();
        for file in self.files.iter() {
            let obj = dst.join(file).with_extension("o");
            let obj = if !obj.starts_with(&dst) {
                dst.join(obj.file_name().ok_or_else(|| {
                    Error::new(ErrorKind::IOError, "Getting object file details failed.")
                })?)
            } else {
                obj
            };

            match obj.parent() {
                Some(s) => fs::create_dir_all(s)?,
                None => {
                    return Err(Error::new(
                        ErrorKind::IOError,
                        "Getting object file details failed.",
                    ))
                }
            };

            objects.push(Object::new(file.to_path_buf(), obj));
        }
        self.compile_objects(&objects)?;
        self.assemble(lib_name, &dst.join(gnu_lib_name), &objects)?;

        if self.get_target()?.contains("msvc") {
            let compiler = self.get_base_compiler()?;
            let atlmfc_lib = compiler
                .env()
                .iter()
                .find(|&&(ref var, _)| var.as_os_str() == OsStr::new("LIB"))
                .and_then(|&(_, ref lib_paths)| {
                    env::split_paths(lib_paths).find(|path| {
                        let sub = Path::new("atlmfc/lib");
                        path.ends_with(sub) || path.parent().map_or(false, |p| p.ends_with(sub))
                    })
                });

            if let Some(atlmfc_lib) = atlmfc_lib {
                self.print(&format!(
                    "cargo:rustc-link-search=native={}",
                    atlmfc_lib.display()
                ));
            }
        }

        self.print(&format!("cargo:rustc-link-lib=static={}", lib_name));
        self.print(&format!("cargo:rustc-link-search=native={}", dst.display()));

        // Add specific C++ libraries, if enabled.
        if self.cpp {
            if let Some(stdlib) = self.get_cpp_link_stdlib()? {
                self.print(&format!("cargo:rustc-link-lib={}", stdlib));
            }
        }

        Ok(())
    }

    /// Run the compiler, generating the file `output`
    ///
    /// The name `output` should be the name of the library.  For backwards compatibility,
    /// the `output` may start with `lib` and end with `.a`.  The Rust compilier will create
    /// the assembly with the lib prefix and .a extension.  MSVC will create a file without prefix,
    /// ending with `.lib`.
    ///
    /// # Panics
    ///
    /// Panics if `output` is not formatted correctly or if one of the underlying
    /// compiler commands fails. It can also panic if it fails reading file names
    /// or creating directories.
    pub fn compile(&self, output: &str) {
        if let Err(e) = self.try_compile(output) {
            fail(&e.message);
        }
    }

    #[cfg(feature = "parallel")]
    fn compile_objects(&self, objs: &[Object]) -> Result<(), Error> {
        use self::rayon::prelude::*;

        if let Some(amt) = self.getenv("NUM_JOBS") {
            if let Ok(amt) = amt.parse() {
                let _ = rayon::ThreadPoolBuilder::new()
                    .num_threads(amt)
                    .build_global();
            }
        }

        // Check for any errors and return the first one found.
        objs.par_iter()
            .with_max_len(1)
            .map(|obj| self.compile_object(obj))
            .collect()
    }

    #[cfg(not(feature = "parallel"))]
    fn compile_objects(&self, objs: &[Object]) -> Result<(), Error> {
        for obj in objs {
            self.compile_object(obj)?;
        }
        Ok(())
    }

    fn compile_object(&self, obj: &Object) -> Result<(), Error> {
        let is_asm = obj.src.extension().and_then(|s| s.to_str()) == Some("asm");
        let target = self.get_target()?;
        let msvc = target.contains("msvc");
        let (mut cmd, name) = if msvc && is_asm {
            self.msvc_macro_assembler()?
        } else {
            let compiler = self.try_get_compiler()?;
            let mut cmd = compiler.to_command();
            for &(ref a, ref b) in self.env.iter() {
                cmd.env(a, b);
            }
            (
                cmd,
                compiler
                    .path
                    .file_name()
                    .ok_or_else(|| Error::new(ErrorKind::IOError, "Failed to get compiler path."))?
                    .to_string_lossy()
                    .into_owned(),
            )
        };
        let is_arm = target.contains("aarch64") || target.contains("arm");
        command_add_output_file(&mut cmd, &obj.dst, msvc, is_asm, is_arm);
        // armasm and armasm64 don't requrie -c option
        if !msvc || !is_asm || !is_arm {
            cmd.arg(if msvc { "/c" } else { "-c" });
        }
        cmd.arg(&obj.src);

        run(&mut cmd, &name)?;
        Ok(())
    }

    /// This will return a result instead of panicing; see expand() for the complete description.
    pub fn try_expand(&self) -> Result<Vec<u8>, Error> {
        let compiler = self.try_get_compiler()?;
        let mut cmd = compiler.to_command();
        for &(ref a, ref b) in self.env.iter() {
            cmd.env(a, b);
        }
        cmd.arg(compiler.family.expand_flag());

        assert!(
            self.files.len() <= 1,
            "Expand may only be called for a single file"
        );

        for file in self.files.iter() {
            cmd.arg(file);
        }

        let name = compiler
            .path
            .file_name()
            .ok_or_else(|| Error::new(ErrorKind::IOError, "Failed to get compiler path."))?
            .to_string_lossy()
            .into_owned();

        Ok(run_output(&mut cmd, &name)?)
    }

    /// Run the compiler, returning the macro-expanded version of the input files.
    ///
    /// This is only relevant for C and C++ files.
    ///
    /// # Panics
    /// Panics if more than one file is present in the config, or if compiler
    /// path has an invalid file name.
    ///
    /// # Example
    /// ```no_run
    /// let out = cc::Build::new().file("src/foo.c").expand();
    /// ```
    pub fn expand(&self) -> Vec<u8> {
        match self.try_expand() {
            Err(e) => fail(&e.message),
            Ok(v) => v,
        }
    }

    /// Get the compiler that's in use for this configuration.
    ///
    /// This function will return a `Tool` which represents the culmination
    /// of this configuration at a snapshot in time. The returned compiler can
    /// be inspected (e.g. the path, arguments, environment) to forward along to
    /// other tools, or the `to_command` method can be used to invoke the
    /// compiler itself.
    ///
    /// This method will take into account all configuration such as debug
    /// information, optimization level, include directories, defines, etc.
    /// Additionally, the compiler binary in use follows the standard
    /// conventions for this path, e.g. looking at the explicitly set compiler,
    /// environment variables (a number of which are inspected here), and then
    /// falling back to the default configuration.
    ///
    /// # Panics
    ///
    /// Panics if an error occurred while determining the architecture.
    pub fn get_compiler(&self) -> Tool {
        match self.try_get_compiler() {
            Ok(tool) => tool,
            Err(e) => fail(&e.message),
        }
    }

    /// Get the compiler that's in use for this configuration.
    ///
    /// This will return a result instead of panicing; see get_compiler() for the complete description.
    pub fn try_get_compiler(&self) -> Result<Tool, Error> {
        let opt_level = self.get_opt_level()?;
        let target = self.get_target()?;

        let mut cmd = self.get_base_compiler()?;
        let envflags = self.envflags(if self.cpp { "CXXFLAGS" } else { "CFLAGS" });

        // Disable default flag generation via environment variable or when
        // certain cross compiling arguments are set
        let use_defaults = self.getenv("CRATE_CC_NO_DEFAULTS").is_none();

        if use_defaults {
            self.add_default_flags(&mut cmd, &target, &opt_level)?;
        } else {
            println!("Info: default compiler flags are disabled");
        }

        for arg in envflags {
            cmd.push_cc_arg(arg.into());
        }

        for directory in self.include_directories.iter() {
            cmd.args.push(cmd.family.include_flag().into());
            cmd.args.push(directory.into());
        }

        // If warnings and/or extra_warnings haven't been explicitly set,
        // then we set them only if the environment doesn't already have
        // CFLAGS/CXXFLAGS, since those variables presumably already contain
        // the desired set of warnings flags.

        if self.warnings.unwrap_or(if self.has_flags() { false } else { true }) {
            let wflags = cmd.family.warnings_flags().into();
            cmd.push_cc_arg(wflags);
        }

        if self.extra_warnings.unwrap_or(if self.has_flags() { false } else { true }) {
            if let Some(wflags) = cmd.family.extra_warnings_flags() {
                cmd.push_cc_arg(wflags.into());
            }
        }

        for flag in self.flags.iter() {
            cmd.args.push(flag.into());
        }

        for flag in self.flags_supported.iter() {
            if self.is_flag_supported(flag).unwrap_or(false) {
                cmd.push_cc_arg(flag.into());
            }
        }

        for &(ref key, ref value) in self.definitions.iter() {
            let lead = if let ToolFamily::Msvc { .. } = cmd.family {
                "/"
            } else {
                "-"
            };
            if let Some(ref value) = *value {
                cmd.args.push(format!("{}D{}={}", lead, key, value).into());
            } else {
                cmd.args.push(format!("{}D{}", lead, key).into());
            }
        }

        if self.warnings_into_errors {
            let warnings_to_errors_flag = cmd.family.warnings_to_errors_flag().into();
            cmd.push_cc_arg(warnings_to_errors_flag);
        }

        Ok(cmd)
    }

    fn add_default_flags(&self, cmd: &mut Tool, target: &str, opt_level: &str) -> Result<(), Error> {
        // Non-target flags
        // If the flag is not conditioned on target variable, it belongs here :)
        match cmd.family {
            ToolFamily::Msvc { .. } => {
                assert!(!self.cuda,
                    "CUDA C++ compilation not supported for MSVC, yet... but you are welcome to implement it :)");

                cmd.args.push("/nologo".into());

                let crt_flag = match self.static_crt {
                    Some(true) => "/MT",
                    Some(false) => "/MD",
                    None => {
                        let features =
                            self.getenv("CARGO_CFG_TARGET_FEATURE").unwrap_or(String::new());
                        if features.contains("crt-static") {
                            "/MT"
                        } else {
                            "/MD"
                        }
                    }
                };
                cmd.args.push(crt_flag.into());

                match &opt_level[..] {
                    // Msvc uses /O1 to enable all optimizations that minimize code size.
                    "z" | "s" | "1" => cmd.push_opt_unless_duplicate("/O1".into()),
                    // -O3 is a valid value for gcc and clang compilers, but not msvc. Cap to /O2.
                    "2" | "3" => cmd.push_opt_unless_duplicate("/O2".into()),
                    _ => {}
                }
            }
            ToolFamily::Gnu | ToolFamily::Clang => {
                // arm-linux-androideabi-gcc 4.8 shipped with Android NDK does
                // not support '-Oz'
                if opt_level == "z" && cmd.family != ToolFamily::Clang {
                    cmd.push_opt_unless_duplicate("-Os".into());
                } else {
                    cmd.push_opt_unless_duplicate(format!("-O{}", opt_level).into());
                }

                if !target.contains("-ios") {
                    cmd.push_cc_arg("-ffunction-sections".into());
                    cmd.push_cc_arg("-fdata-sections".into());
                }
                if self.pic.unwrap_or(!target.contains("windows-gnu")) {
                    cmd.push_cc_arg("-fPIC".into());
                    // PLT only applies if code is compiled with PIC support,
                    // and only for ELF targets.
                    if target.contains("linux") && !self.use_plt.unwrap_or(true) {
                        cmd.push_cc_arg("-fno-plt".into());
                    }
                }
            }
        }

        if self.get_debug() {
            if self.cuda {
                let nvcc_debug_flag = cmd.family.nvcc_debug_flag().into();
                cmd.args.push(nvcc_debug_flag);
            }
            let family = cmd.family;
            family.add_debug_flags(cmd);
        }

        // Target flags
        match cmd.family {
            ToolFamily::Clang => {
                cmd.args.push(format!("--target={}", target).into());
            }
            ToolFamily::Msvc { clang_cl } => {
                if clang_cl {
                    if target.contains("x86_64") {
                        cmd.args.push("-m64".into());
                    } else if target.contains("86") {
                        cmd.args.push("-m32".into());
                        cmd.args.push("/arch:IA32".into());
                    } else {
                        cmd.args.push(format!("--target={}", target).into());
                    }
                } else {
                    if target.contains("i586") {
                        cmd.args.push("/ARCH:IA32".into());
                    }
                }

                // There is a check in corecrt.h that will generate a
                // compilation error if
                // _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE is
                // not defined to 1. The check was added in Windows
                // 8 days because only store apps were allowed on ARM.
                // This changed with the release of Windows 10 IoT Core.
                // The check will be going away in future versions of
                // the SDK, but for all released versions of the
                // Windows SDK it is required.
                if target.contains("arm") || target.contains("thumb") {
                    cmd.args.push("/D_ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE=1".into());
                }
            }
            ToolFamily::Gnu => {
                if target.contains("i686") || target.contains("i586") {
                    cmd.args.push("-m32".into());
                } else if target == "x86_64-unknown-linux-gnux32" {
                    cmd.args.push("-mx32".into());
                } else if target.contains("x86_64") || target.contains("powerpc64") {
                    cmd.args.push("-m64".into());
                }

                if self.static_flag.is_none() {
                    let features = self.getenv("CARGO_CFG_TARGET_FEATURE").unwrap_or(String::new());
                    if features.contains("crt-static") {
                        cmd.args.push("-static".into());
                    }
                }

                // armv7 targets get to use armv7 instructions
                if (target.starts_with("armv7") || target.starts_with("thumbv7")) && target.contains("-linux-") {
                    cmd.args.push("-march=armv7-a".into());
                }

                // (x86 Android doesn't say "eabi")
                if target.contains("-androideabi") && target.contains("v7") {
                    // -march=armv7-a handled above
                    cmd.args.push("-mthumb".into());
                    if !target.contains("neon") {
                        // On android we can guarantee some extra float instructions
                        // (specified in the android spec online)
                        // NEON guarantees even more; see below.
                        cmd.args.push("-mfpu=vfpv3-d16".into());
                    }
                    cmd.args.push("-mfloat-abi=softfp".into());
                }

                if target.contains("neon") {
                    cmd.args.push("-mfpu=neon-vfpv4".into());
                }

                if target.starts_with("armv4t-unknown-linux-") {
                    cmd.args.push("-march=armv4t".into());
                    cmd.args.push("-marm".into());
                    cmd.args.push("-mfloat-abi=soft".into());
                }

                if target.starts_with("armv5te-unknown-linux-") {
                    cmd.args.push("-march=armv5te".into());
                    cmd.args.push("-marm".into());
                    cmd.args.push("-mfloat-abi=soft".into());
                }

                // For us arm == armv6 by default
                if target.starts_with("arm-unknown-linux-") {
                    cmd.args.push("-march=armv6".into());
                    cmd.args.push("-marm".into());
                    if target.ends_with("hf") {
                        cmd.args.push("-mfpu=vfp".into());
                    } else {
                        cmd.args.push("-mfloat-abi=soft".into());
                    }
                }

                // We can guarantee some settings for FRC
                if target.starts_with("arm-frc-") {
                    cmd.args.push("-march=armv7-a".into());
                    cmd.args.push("-mcpu=cortex-a9".into());
                    cmd.args.push("-mfpu=vfpv3".into());
                    cmd.args.push("-mfloat-abi=softfp".into());
                    cmd.args.push("-marm".into());
                }

                // Turn codegen down on i586 to avoid some instructions.
                if target.starts_with("i586-unknown-linux-") {
                    cmd.args.push("-march=pentium".into());
                }

                // Set codegen level for i686 correctly
                if target.starts_with("i686-unknown-linux-") {
                    cmd.args.push("-march=i686".into());
                }

                // Looks like `musl-gcc` makes is hard for `-m32` to make its way
                // all the way to the linker, so we need to actually instruct the
                // linker that we're generating 32-bit executables as well. This'll
                // typically only be used for build scripts which transitively use
                // these flags that try to compile executables.
                if target == "i686-unknown-linux-musl" || target == "i586-unknown-linux-musl" {
                    cmd.args.push("-Wl,-melf_i386".into());
                }

                if target.starts_with("thumb") {
                    cmd.args.push("-mthumb".into());

                    if target.ends_with("eabihf") {
                        cmd.args.push("-mfloat-abi=hard".into())
                    }
                }
                if target.starts_with("thumbv6m") {
                    cmd.args.push("-march=armv6s-m".into());
                }
                if target.starts_with("thumbv7em") {
                    cmd.args.push("-march=armv7e-m".into());

                    if target.ends_with("eabihf") {
                        cmd.args.push("-mfpu=fpv4-sp-d16".into())
                    }
                }
                if target.starts_with("thumbv7m") {
                    cmd.args.push("-march=armv7-m".into());
                }
                if target.starts_with("thumbv8m.base") {
                    cmd.args.push("-march=armv8-m.base".into());
                }
                if target.starts_with("thumbv8m.main") {
                    cmd.args.push("-march=armv8-m.main".into());

                    if target.ends_with("eabihf") {
                        cmd.args.push("-mfpu=fpv5-sp-d16".into())
                    }
                }
                if target.starts_with("armebv7r") | target.starts_with("armv7r") {
                    if target.starts_with("armeb") {
                        cmd.args.push("-mbig-endian".into());
                    } else {
                        cmd.args.push("-mlittle-endian".into());
                    }

                    // ARM mode
                    cmd.args.push("-marm".into());

                    // R Profile
                    cmd.args.push("-march=armv7-r".into());

                    if target.ends_with("eabihf") {
                        // Calling convention
                        cmd.args.push("-mfloat-abi=hard".into());

                        // lowest common denominator FPU
                        // (see Cortex-R4 technical reference manual)
                        cmd.args.push("-mfpu=vfpv3-d16".into())
                    } else {
                        // Calling convention
                        cmd.args.push("-mfloat-abi=soft".into());
                    }
                }
            }
        }

        if target.contains("-ios") {
            // FIXME: potential bug. iOS is always compiled with Clang, but Gcc compiler may be
            // detected instead.
            self.ios_flags(cmd)?;
        }

        if self.static_flag.unwrap_or(false) {
            cmd.args.push("-static".into());
        }
        if self.shared_flag.unwrap_or(false) {
            cmd.args.push("-shared".into());
        }

        if self.cpp {
            match (self.cpp_set_stdlib.as_ref(), cmd.family) {
                (None, _) => {}
                (Some(stdlib), ToolFamily::Gnu) | (Some(stdlib), ToolFamily::Clang) => {
                    cmd.push_cc_arg(format!("-stdlib=lib{}", stdlib).into());
                }
                _ => {
                    println!(
                        "cargo:warning=cpp_set_stdlib is specified, but the {:?} compiler \
                         does not support this option, ignored",
                        cmd.family
                    );
                }
            }
        }

        Ok(())
    }

    fn has_flags(&self) -> bool {
        let flags_env_var_name = if self.cpp { "CXXFLAGS" } else { "CFLAGS" };
        let flags_env_var_value = self.get_var(flags_env_var_name);
        if let Ok(_) = flags_env_var_value { true } else { false }
    }

    fn msvc_macro_assembler(&self) -> Result<(Command, String), Error> {
        let target = self.get_target()?;
        let tool = if target.contains("x86_64") {
            "ml64.exe"
        } else if target.contains("arm") {
            "armasm.exe"
        } else if target.contains("aarch64") {
            "armasm64.exe"
        } else {
            "ml.exe"
        };
        let mut cmd = windows_registry::find(&target, tool).unwrap_or_else(|| self.cmd(tool));
        for directory in self.include_directories.iter() {
            cmd.arg("/I").arg(directory);
        }
        for &(ref key, ref value) in self.definitions.iter() {
            if let Some(ref value) = *value {
                cmd.arg(&format!("/D{}={}", key, value));
            } else {
                cmd.arg(&format!("/D{}", key));
            }
        }

        if target.contains("i686") || target.contains("i586") {
            cmd.arg("/safeseh");
        }
        for flag in self.flags.iter() {
            cmd.arg(flag);
        }

        Ok((cmd, tool.to_string()))
    }

    fn assemble(&self, lib_name: &str, dst: &Path, objs: &[Object]) -> Result<(), Error> {
        // Delete the destination if it exists as the `ar` tool at least on Unix
        // appends to it, which we don't want.
        let _ = fs::remove_file(&dst);

        let objects: Vec<_> = objs.iter().map(|obj| obj.dst.clone()).collect();
        let target = self.get_target()?;
        if target.contains("msvc") {
            let (mut cmd, program) = self.get_ar()?;
            let mut out = OsString::from("/OUT:");
            out.push(dst);
            cmd.arg(out).arg("/nologo");

            // Similar to https://github.com/rust-lang/rust/pull/47507
            // and https://github.com/rust-lang/rust/pull/48548
            let estimated_command_line_len = objects
                .iter()
                .chain(&self.objects)
                .map(|a| a.as_os_str().len())
                .sum::<usize>();
            if estimated_command_line_len > 1024 * 6 {
                let mut args = String::from("\u{FEFF}"); // BOM
                for arg in objects.iter().chain(&self.objects) {
                    args.push('"');
                    for c in arg.to_str().unwrap().chars() {
                        if c == '"' {
                            args.push('\\')
                        }
                        args.push(c)
                    }
                    args.push('"');
                    args.push('\n');
                }

                let mut utf16le = Vec::new();
                for code_unit in args.encode_utf16() {
                    utf16le.push(code_unit as u8);
                    utf16le.push((code_unit >> 8) as u8);
                }

                let mut args_file = OsString::from(dst);
                args_file.push(".args");
                fs::File::create(&args_file)
                    .unwrap()
                    .write_all(&utf16le)
                    .unwrap();

                let mut args_file_arg = OsString::from("@");
                args_file_arg.push(args_file);
                cmd.arg(args_file_arg);
            } else {
                cmd.args(&objects).args(&self.objects);
            }
            run(&mut cmd, &program)?;

            // The Rust compiler will look for libfoo.a and foo.lib, but the
            // MSVC linker will also be passed foo.lib, so be sure that both
            // exist for now.
            let lib_dst = dst.with_file_name(format!("{}.lib", lib_name));
            let _ = fs::remove_file(&lib_dst);
            match fs::hard_link(&dst, &lib_dst).or_else(|_| {
                // if hard-link fails, just copy (ignoring the number of bytes written)
                fs::copy(&dst, &lib_dst).map(|_| ())
            }) {
                Ok(_) => (),
                Err(_) => {
                    return Err(Error::new(
                        ErrorKind::IOError,
                        "Could not copy or create a hard-link to the generated lib file.",
                    ))
                }
            };
        } else {
            let (mut ar, cmd) = self.get_ar()?;
            run(
                ar.arg("crs").arg(dst).args(&objects).args(&self.objects),
                &cmd,
            )?;
        }

        Ok(())
    }

    fn ios_flags(&self, cmd: &mut Tool) -> Result<(), Error> {
        enum ArchSpec {
            Device(&'static str),
            Simulator(&'static str),
        }

        let target = self.get_target()?;
        let arch = target.split('-').nth(0).ok_or_else(|| {
            Error::new(
                ErrorKind::ArchitectureInvalid,
                "Unknown architecture for iOS target.",
            )
        })?;
        let arch = match arch {
            "arm" | "armv7" | "thumbv7" => ArchSpec::Device("armv7"),
            "armv7s" | "thumbv7s" => ArchSpec::Device("armv7s"),
            "arm64" | "aarch64" => ArchSpec::Device("arm64"),
            "i386" | "i686" => ArchSpec::Simulator("-m32"),
            "x86_64" => ArchSpec::Simulator("-m64"),
            _ => {
                return Err(Error::new(
                    ErrorKind::ArchitectureInvalid,
                    "Unknown architecture for iOS target.",
                ))
            }
        };

        let sdk = match arch {
            ArchSpec::Device(arch) => {
                cmd.args.push("-arch".into());
                cmd.args.push(arch.into());
                cmd.args.push("-miphoneos-version-min=7.0".into());
                "iphoneos"
            }
            ArchSpec::Simulator(arch) => {
                cmd.args.push(arch.into());
                cmd.args.push("-mios-simulator-version-min=7.0".into());
                "iphonesimulator"
            }
        };

        self.print(&format!("Detecting iOS SDK path for {}", sdk));
        let sdk_path = self.cmd("xcrun")
            .arg("--show-sdk-path")
            .arg("--sdk")
            .arg(sdk)
            .stderr(Stdio::inherit())
            .output()?
            .stdout;

        let sdk_path = match String::from_utf8(sdk_path) {
            Ok(p) => p,
            Err(_) => {
                return Err(Error::new(
                    ErrorKind::IOError,
                    "Unable to determine iOS SDK path.",
                ))
            }
        };

        cmd.args.push("-isysroot".into());
        cmd.args.push(sdk_path.trim().into());
        cmd.args.push("-fembed-bitcode".into());
        /*
         * TODO we probably ultimatedly want the -fembed-bitcode-marker flag
         * but can't have it now because of an issue in LLVM:
         * https://github.com/alexcrichton/cc-rs/issues/301
         * https://github.com/rust-lang/rust/pull/48896#comment-372192660
         */
        /*
        if self.get_opt_level()? == "0" {
            cmd.args.push("-fembed-bitcode-marker".into());
        }
        */

        Ok(())
    }

    fn cmd<P: AsRef<OsStr>>(&self, prog: P) -> Command {
        let mut cmd = Command::new(prog);
        for &(ref a, ref b) in self.env.iter() {
            cmd.env(a, b);
        }
        cmd
    }

    fn get_base_compiler(&self) -> Result<Tool, Error> {
        if let Some(ref c) = self.compiler {
            return Ok(Tool::new(c.clone()));
        }
        let host = self.get_host()?;
        let target = self.get_target()?;
        let (env, msvc, gnu, traditional, clang) = if self.cpp {
            ("CXX", "cl.exe", "g++", "c++", "clang++")
        } else {
            ("CC", "cl.exe", "gcc", "cc", "clang")
        };

        // On Solaris, c++/cc unlikely to exist or be correct.
        let default = if host.contains("solaris") {
            gnu
        } else {
            traditional
        };

        let cl_exe = windows_registry::find_tool(&target, "cl.exe");

        let tool_opt: Option<Tool> = self.env_tool(env)
            .map(|(tool, cc, args)| {
                // chop off leading/trailing whitespace to work around
                // semi-buggy build scripts which are shared in
                // makefiles/configure scripts (where spaces are far more
                // lenient)
                let mut t = Tool::new(PathBuf::from(tool.trim()));
                if let Some(cc) = cc {
                    t.cc_wrapper_path = Some(PathBuf::from(cc));
                }
                for arg in args {
                    t.cc_wrapper_args.push(arg.into());
                }
                t
            })
            .or_else(|| {
                if target.contains("emscripten") {
                    let tool = if self.cpp { "em++" } else { "emcc" };
                    // Windows uses bat file so we have to be a bit more specific
                    if cfg!(windows) {
                        let mut t = Tool::new(PathBuf::from("cmd"));
                        t.args.push("/c".into());
                        t.args.push(format!("{}.bat", tool).into());
                        Some(t)
                    } else {
                        Some(Tool::new(PathBuf::from(tool)))
                    }
                } else {
                    None
                }
            })
            .or_else(|| cl_exe.clone());

        let tool = match tool_opt {
            Some(t) => t,
            None => {
                let compiler = if host.contains("windows") && target.contains("windows") {
                    if target.contains("msvc") {
                        msvc.to_string()
                    } else {
                        format!("{}.exe", gnu)
                    }
                } else if target.contains("android") {
                    let target = target
                        .replace("armv7neon", "arm")
                        .replace("armv7", "arm")
                        .replace("thumbv7neon", "arm")
                        .replace("thumbv7", "arm");
                    let gnu_compiler = format!("{}-{}", target, gnu);
                    let clang_compiler = format!("{}-{}", target, clang);
                    // Check if gnu compiler is present
                    // if not, use clang
                    if Command::new(&gnu_compiler).spawn().is_ok() {
                        gnu_compiler
                    } else {
                        clang_compiler
                    }
                } else if target.contains("cloudabi") {
                    format!("{}-{}", target, traditional)
                } else if target == "wasm32-unknown-wasi" || target == "wasm32-unknown-unknown" {
                    "clang".to_string()
                } else if self.get_host()? != target {
                    // CROSS_COMPILE is of the form: "arm-linux-gnueabi-"
                    let cc_env = self.getenv("CROSS_COMPILE");
                    let cross_compile = cc_env.as_ref().map(|s| s.trim_right_matches('-'));
                    let prefix = cross_compile.or(match &target[..] {
                        "aarch64-unknown-linux-gnu" => Some("aarch64-linux-gnu"),
                        "aarch64-unknown-linux-musl" => Some("aarch64-linux-musl"),
                        "aarch64-unknown-netbsd" => Some("aarch64--netbsd"),
                        "arm-unknown-linux-gnueabi" => Some("arm-linux-gnueabi"),
                        "armv4t-unknown-linux-gnueabi" => Some("arm-linux-gnueabi"),
                        "armv5te-unknown-linux-gnueabi" => Some("arm-linux-gnueabi"),
                        "arm-frc-linux-gnueabi" => Some("arm-frc-linux-gnueabi"),
                        "arm-unknown-linux-gnueabihf" => Some("arm-linux-gnueabihf"),
                        "arm-unknown-linux-musleabi" => Some("arm-linux-musleabi"),
                        "arm-unknown-linux-musleabihf" => Some("arm-linux-musleabihf"),
                        "arm-unknown-netbsd-eabi" => Some("arm--netbsdelf-eabi"),
                        "armv6-unknown-netbsd-eabihf" => Some("armv6--netbsdelf-eabihf"),
                        "armv7-unknown-linux-gnueabihf" => Some("arm-linux-gnueabihf"),
                        "armv7-unknown-linux-musleabihf" => Some("arm-linux-musleabihf"),
                        "armv7neon-unknown-linux-gnueabihf" => Some("arm-linux-gnueabihf"),
                        "armv7neon-unknown-linux-musleabihf" => Some("arm-linux-musleabihf"),
                        "thumbv7-unknown-linux-gnueabihf" => Some("arm-linux-gnueabihf"),
                        "thumbv7-unknown-linux-musleabihf" => Some("arm-linux-musleabihf"),
                        "thumbv7neon-unknown-linux-gnueabihf" => Some("arm-linux-gnueabihf"),
                        "thumbv7neon-unknown-linux-musleabihf" => Some("arm-linux-musleabihf"),
                        "armv7-unknown-netbsd-eabihf" => Some("armv7--netbsdelf-eabihf"),
                        "i586-unknown-linux-musl" => Some("musl"),
                        "i686-pc-windows-gnu" => Some("i686-w64-mingw32"),
                        "i686-unknown-linux-musl" => Some("musl"),
                        "i686-unknown-netbsd" => Some("i486--netbsdelf"),
                        "mips-unknown-linux-gnu" => Some("mips-linux-gnu"),
                        "mipsel-unknown-linux-gnu" => Some("mipsel-linux-gnu"),
                        "mips64-unknown-linux-gnuabi64" => Some("mips64-linux-gnuabi64"),
                        "mips64el-unknown-linux-gnuabi64" => Some("mips64el-linux-gnuabi64"),
                        "powerpc-unknown-linux-gnu" => Some("powerpc-linux-gnu"),
                        "powerpc-unknown-linux-gnuspe" => Some("powerpc-linux-gnuspe"),
                        "powerpc-unknown-netbsd" => Some("powerpc--netbsd"),
                        "powerpc64-unknown-linux-gnu" => Some("powerpc-linux-gnu"),
                        "powerpc64le-unknown-linux-gnu" => Some("powerpc64le-linux-gnu"),
                        "s390x-unknown-linux-gnu" => Some("s390x-linux-gnu"),
                        "sparc-unknown-linux-gnu" => Some("sparc-linux-gnu"),
                        "sparc64-unknown-linux-gnu" => Some("sparc64-linux-gnu"),
                        "sparc64-unknown-netbsd" => Some("sparc64--netbsd"),
                        "sparcv9-sun-solaris" => Some("sparcv9-sun-solaris"),
                        "armebv7r-none-eabi" => Some("arm-none-eabi"),
                        "armebv7r-none-eabihf" => Some("arm-none-eabi"),
                        "armv7r-none-eabi" => Some("arm-none-eabi"),
                        "armv7r-none-eabihf" => Some("arm-none-eabi"),
                        "thumbv6m-none-eabi" => Some("arm-none-eabi"),
                        "thumbv7em-none-eabi" => Some("arm-none-eabi"),
                        "thumbv7em-none-eabihf" => Some("arm-none-eabi"),
                        "thumbv7m-none-eabi" => Some("arm-none-eabi"),
                        "thumbv8m.base-none-eabi" => Some("arm-none-eabi"),
                        "thumbv8m.main-none-eabi" => Some("arm-none-eabi"),
                        "thumbv8m.main-none-eabihf" => Some("arm-none-eabi"),
                        "x86_64-pc-windows-gnu" => Some("x86_64-w64-mingw32"),
                        "x86_64-rumprun-netbsd" => Some("x86_64-rumprun-netbsd"),
                        "x86_64-unknown-linux-musl" => Some("musl"),
                        "x86_64-unknown-netbsd" => Some("x86_64--netbsd"),
                        _ => None,
                    });
                    match prefix {
                        Some(prefix) => format!("{}-{}", prefix, gnu),
                        None => default.to_string(),
                    }
                } else {
                    default.to_string()
                };
                Tool::new(PathBuf::from(compiler))
            }
        };

        let mut tool = if self.cuda {
            assert!(
                tool.args.is_empty(),
                "CUDA compilation currently assumes empty pre-existing args"
            );
            let nvcc = match self.get_var("NVCC") {
                Err(_) => "nvcc".into(),
                Ok(nvcc) => nvcc,
            };
            let mut nvcc_tool = Tool::with_features(PathBuf::from(nvcc), self.cuda);
            nvcc_tool
                .args
                .push(format!("-ccbin={}", tool.path.display()).into());
            nvcc_tool
        } else {
            tool
        };

        // If we found `cl.exe` in our environment, the tool we're returning is
        // an MSVC-like tool, *and* no env vars were set then set env vars for
        // the tool that we're returning.
        //
        // Env vars are needed for things like `link.exe` being put into PATH as
        // well as header include paths sometimes. These paths are automatically
        // included by default but if the `CC` or `CXX` env vars are set these
        // won't be used. This'll ensure that when the env vars are used to
        // configure for invocations like `clang-cl` we still get a "works out
        // of the box" experience.
        if let Some(cl_exe) = cl_exe {
            if tool.family == (ToolFamily::Msvc { clang_cl: true }) &&
                tool.env.len() == 0 &&
                target.contains("msvc")
            {
                for &(ref k, ref v) in cl_exe.env.iter() {
                    tool.env.push((k.to_owned(), v.to_owned()));
                }
            }
        }

        Ok(tool)
    }

    fn get_var(&self, var_base: &str) -> Result<String, Error> {
        let target = self.get_target()?;
        let host = self.get_host()?;
        let kind = if host == target { "HOST" } else { "TARGET" };
        let target_u = target.replace("-", "_");
        let res = self.getenv(&format!("{}_{}", var_base, target))
            .or_else(|| self.getenv(&format!("{}_{}", var_base, target_u)))
            .or_else(|| self.getenv(&format!("{}_{}", kind, var_base)))
            .or_else(|| self.getenv(var_base));

        match res {
            Some(res) => Ok(res),
            None => Err(Error::new(
                ErrorKind::EnvVarNotFound,
                &format!("Could not find environment variable {}.", var_base),
            )),
        }
    }

    fn envflags(&self, name: &str) -> Vec<String> {
        self.get_var(name)
            .unwrap_or(String::new())
            .split(|c: char| c.is_whitespace())
            .filter(|s| !s.is_empty())
            .map(|s| s.to_string())
            .collect()
    }

    /// Returns compiler path, optional modifier name from whitelist, and arguments vec
    fn env_tool(&self, name: &str) -> Option<(String, Option<String>, Vec<String>)> {
        let tool = match self.get_var(name) {
            Ok(tool) => tool,
            Err(_) => return None,
        };

        // If this is an exact path on the filesystem we don't want to do any
        // interpretation at all, just pass it on through. This'll hopefully get
        // us to support spaces-in-paths.
        if Path::new(&tool).exists() {
            return Some((tool, None, Vec::new()));
        }

        // Ok now we want to handle a couple of scenarios. We'll assume from
        // here on out that spaces are splitting separate arguments. Two major
        // features we want to support are:
        //
        //      CC='sccache cc'
        //
        // aka using `sccache` or any other wrapper/caching-like-thing for
        // compilations. We want to know what the actual compiler is still,
        // though, because our `Tool` API support introspection of it to see
        // what compiler is in use.
        //
        // additionally we want to support
        //
        //      CC='cc -flag'
        //
        // where the CC env var is used to also pass default flags to the C
        // compiler.
        //
        // It's true that everything here is a bit of a pain, but apparently if
        // you're not literally make or bash then you get a lot of bug reports.
        let known_wrappers = ["ccache", "distcc", "sccache", "icecc"];

        let mut parts = tool.split_whitespace();
        let maybe_wrapper = match parts.next() {
            Some(s) => s,
            None => return None,
        };

        let file_stem = Path::new(maybe_wrapper)
            .file_stem()
            .unwrap()
            .to_str()
            .unwrap();
        if known_wrappers.contains(&file_stem) {
            if let Some(compiler) = parts.next() {
                return Some((
                    compiler.to_string(),
                    Some(maybe_wrapper.to_string()),
                    parts.map(|s| s.to_string()).collect(),
                ));
            }
        }

        Some((
            maybe_wrapper.to_string(),
            None,
            parts.map(|s| s.to_string()).collect(),
        ))
    }

    /// Returns the default C++ standard library for the current target: `libc++`
    /// for OS X and `libstdc++` for anything else.
    fn get_cpp_link_stdlib(&self) -> Result<Option<String>, Error> {
        match self.cpp_link_stdlib.clone() {
            Some(s) => Ok(s),
            None => {
                if let Ok(stdlib) = self.get_var("CXXSTDLIB") {
                    if stdlib.is_empty() {
                        Ok(None)
                    } else {
                        Ok(Some(stdlib))
                    }
                } else {
                    let target = self.get_target()?;
                    if target.contains("msvc") {
                        Ok(None)
                    } else if target.contains("apple") {
                        Ok(Some("c++".to_string()))
                    } else if target.contains("freebsd") {
                        Ok(Some("c++".to_string()))
                    } else if target.contains("openbsd") {
                        Ok(Some("c++".to_string()))
                    } else {
                        Ok(Some("stdc++".to_string()))
                    }
                }
            }
        }
    }

    fn get_ar(&self) -> Result<(Command, String), Error> {
        if let Some(ref p) = self.archiver {
            let name = p.file_name().and_then(|s| s.to_str()).unwrap_or("ar");
            return Ok((self.cmd(p), name.to_string()));
        }
        if let Ok(p) = self.get_var("AR") {
            return Ok((self.cmd(&p), p));
        }
        let target = self.get_target()?;
        let program = if target.contains("android") {
            format!("{}-ar", target.replace("armv7", "arm"))
        } else if target.contains("emscripten") {
            // Windows use bat files so we have to be a bit more specific
            if cfg!(windows) {
                let mut cmd = self.cmd("cmd");
                cmd.arg("/c").arg("emar.bat");
                return Ok((cmd, "emar.bat".to_string()));
            }

            "emar".to_string()
        } else if target.contains("msvc") {
            match windows_registry::find(&target, "lib.exe") {
                Some(t) => return Ok((t, "lib.exe".to_string())),
                None => "lib.exe".to_string(),
            }
        } else {
            "ar".to_string()
        };
        Ok((self.cmd(&program), program))
    }

    fn get_target(&self) -> Result<String, Error> {
        match self.target.clone() {
            Some(t) => Ok(t),
            None => Ok(self.getenv_unwrap("TARGET")?),
        }
    }

    fn get_host(&self) -> Result<String, Error> {
        match self.host.clone() {
            Some(h) => Ok(h),
            None => Ok(self.getenv_unwrap("HOST")?),
        }
    }

    fn get_opt_level(&self) -> Result<String, Error> {
        match self.opt_level.as_ref().cloned() {
            Some(ol) => Ok(ol),
            None => Ok(self.getenv_unwrap("OPT_LEVEL")?),
        }
    }

    fn get_debug(&self) -> bool {
        self.debug.unwrap_or_else(|| match self.getenv("DEBUG") {
            Some(s) => s != "false",
            None => false,
        })
    }

    fn get_out_dir(&self) -> Result<PathBuf, Error> {
        match self.out_dir.clone() {
            Some(p) => Ok(p),
            None => Ok(env::var_os("OUT_DIR").map(PathBuf::from).ok_or_else(|| {
                Error::new(
                    ErrorKind::EnvVarNotFound,
                    "Environment variable OUT_DIR not defined.",
                )
            })?),
        }
    }

    fn getenv(&self, v: &str) -> Option<String> {
        let mut cache = self.env_cache.lock().unwrap();
        if let Some(val) = cache.get(v) {
            return val.clone()
        }
        let r = env::var(v).ok();
        self.print(&format!("{} = {:?}", v, r));
        cache.insert(v.to_string(), r.clone());
        r
    }

    fn getenv_unwrap(&self, v: &str) -> Result<String, Error> {
        match self.getenv(v) {
            Some(s) => Ok(s),
            None => Err(Error::new(
                ErrorKind::EnvVarNotFound,
                &format!("Environment variable {} not defined.", v.to_string()),
            )),
        }
    }

    fn print(&self, s: &str) {
        if self.cargo_metadata {
            println!("{}", s);
        }
    }
}

impl Default for Build {
    fn default() -> Build {
        Build::new()
    }
}

impl Tool {
    fn new(path: PathBuf) -> Tool {
        Tool::with_features(path, false)
    }

    fn with_features(path: PathBuf, cuda: bool) -> Tool {
        // Try to detect family of the tool from its name, falling back to Gnu.
        let family = if let Some(fname) = path.file_name().and_then(|p| p.to_str()) {
            if fname.contains("clang-cl") {
                ToolFamily::Msvc { clang_cl: true }
            } else if fname.contains("cl") &&
                !fname.contains("cloudabi") &&
                !fname.contains("uclibc") &&
                !fname.contains("clang") {
                ToolFamily::Msvc { clang_cl: false }
            } else if fname.contains("clang") {
                ToolFamily::Clang
            } else {
                ToolFamily::Gnu
            }
        } else {
            ToolFamily::Gnu
        };
        Tool {
            path: path,
            cc_wrapper_path: None,
            cc_wrapper_args: Vec::new(),
            args: Vec::new(),
            env: Vec::new(),
            family: family,
            cuda: cuda,
            removed_args: Vec::new(),
        }
    }

    /// Add an argument to be stripped from the final command arguments.
    fn remove_arg(&mut self, flag: OsString) {
        self.removed_args.push(flag);
    }

    /// Add a flag, and optionally prepend the NVCC wrapper flag "-Xcompiler".
    ///
    /// Currently this is only used for compiling CUDA sources, since NVCC only
    /// accepts a limited set of GNU-like flags, and the rest must be prefixed
    /// with a "-Xcompiler" flag to get passed to the underlying C++ compiler.
    fn push_cc_arg(&mut self, flag: OsString) {
        if self.cuda {
            self.args.push(self.family.nvcc_redirect_flag().into());
        }
        self.args.push(flag);
    }

    fn is_duplicate_opt_arg(&self, flag: &OsString) -> bool {
        let flag = flag.to_str().unwrap();
        let mut chars = flag.chars();

        // Only duplicate check compiler flags
        if self.is_like_msvc() {
            if chars.next() != Some('/') {
                return false;
            }
        } else if self.is_like_gnu() || self.is_like_clang() {
            if chars.next() != Some('-') {
                return false;
            }
        }

        // Check for existing optimization flags (-O, /O)
        if chars.next() == Some('O') {
            return self.args().iter().any(|ref a|
                a.to_str().unwrap_or("").chars().nth(1) == Some('O')
            );
        }

        // TODO Check for existing -m..., -m...=..., /arch:... flags
        return false;
    }

    /// Don't push optimization arg if it conflicts with existing args
    fn push_opt_unless_duplicate(&mut self, flag: OsString) {
        if self.is_duplicate_opt_arg(&flag) {
            println!("Info: Ignoring duplicate arg {:?}", &flag);
        } else {
            self.push_cc_arg(flag);
        }
    }

    /// Converts this compiler into a `Command` that's ready to be run.
    ///
    /// This is useful for when the compiler needs to be executed and the
    /// command returned will already have the initial arguments and environment
    /// variables configured.
    pub fn to_command(&self) -> Command {
        let mut cmd = match self.cc_wrapper_path {
            Some(ref cc_wrapper_path) => {
                let mut cmd = Command::new(&cc_wrapper_path);
                cmd.arg(&self.path);
                cmd
            }
            None => Command::new(&self.path),
        };
        cmd.args(&self.cc_wrapper_args);

        let value = self.args.iter().filter(|a| !self.removed_args.contains(a)).collect::<Vec<_>>();
        cmd.args(&value);

        for &(ref k, ref v) in self.env.iter() {
            cmd.env(k, v);
        }
        cmd
    }

    /// Returns the path for this compiler.
    ///
    /// Note that this may not be a path to a file on the filesystem, e.g. "cc",
    /// but rather something which will be resolved when a process is spawned.
    pub fn path(&self) -> &Path {
        &self.path
    }

    /// Returns the default set of arguments to the compiler needed to produce
    /// executables for the target this compiler generates.
    pub fn args(&self) -> &[OsString] {
        &self.args
    }

    /// Returns the set of environment variables needed for this compiler to
    /// operate.
    ///
    /// This is typically only used for MSVC compilers currently.
    pub fn env(&self) -> &[(OsString, OsString)] {
        &self.env
    }

    /// Returns the compiler command in format of CC environment variable.
    /// Or empty string if CC env was not present
    ///
    /// This is typically used by configure script
    pub fn cc_env(&self) -> OsString {
        match self.cc_wrapper_path {
            Some(ref cc_wrapper_path) => {
                let mut cc_env = cc_wrapper_path.as_os_str().to_owned();
                cc_env.push(" ");
                cc_env.push(self.path.to_path_buf().into_os_string());
                for arg in self.cc_wrapper_args.iter() {
                    cc_env.push(" ");
                    cc_env.push(arg);
                }
                cc_env
            }
            None => OsString::from(""),
        }
    }

    /// Returns the compiler flags in format of CFLAGS environment variable.
    /// Important here - this will not be CFLAGS from env, its internal gcc's flags to use as CFLAGS
    /// This is typically used by configure script
    pub fn cflags_env(&self) -> OsString {
        let mut flags = OsString::new();
        for (i, arg) in self.args.iter().enumerate() {
            if i > 0 {
                flags.push(" ");
            }
            flags.push(arg);
        }
        flags
    }

    /// Whether the tool is GNU Compiler Collection-like.
    pub fn is_like_gnu(&self) -> bool {
        self.family == ToolFamily::Gnu
    }

    /// Whether the tool is Clang-like.
    pub fn is_like_clang(&self) -> bool {
        self.family == ToolFamily::Clang
    }

    /// Whether the tool is MSVC-like.
    pub fn is_like_msvc(&self) -> bool {
        match self.family {
            ToolFamily::Msvc { .. } => true,
            _ => false,
        }
    }
}

fn run(cmd: &mut Command, program: &str) -> Result<(), Error> {
    let (mut child, print) = spawn(cmd, program)?;
    let status = match child.wait() {
        Ok(s) => s,
        Err(_) => {
            return Err(Error::new(
                ErrorKind::ToolExecError,
                &format!(
                    "Failed to wait on spawned child process, command {:?} with args {:?}.",
                    cmd, program
                ),
            ))
        }
    };
    print.join().unwrap();
    println!("{}", status);

    if status.success() {
        Ok(())
    } else {
        Err(Error::new(
            ErrorKind::ToolExecError,
            &format!(
                "Command {:?} with args {:?} did not execute successfully (status code {}).",
                cmd, program, status
            ),
        ))
    }
}

fn run_output(cmd: &mut Command, program: &str) -> Result<Vec<u8>, Error> {
    cmd.stdout(Stdio::piped());
    let (mut child, print) = spawn(cmd, program)?;
    let mut stdout = vec![];
    child
        .stdout
        .take()
        .unwrap()
        .read_to_end(&mut stdout)
        .unwrap();
    let status = match child.wait() {
        Ok(s) => s,
        Err(_) => {
            return Err(Error::new(
                ErrorKind::ToolExecError,
                &format!(
                    "Failed to wait on spawned child process, command {:?} with args {:?}.",
                    cmd, program
                ),
            ))
        }
    };
    print.join().unwrap();
    println!("{}", status);

    if status.success() {
        Ok(stdout)
    } else {
        Err(Error::new(
            ErrorKind::ToolExecError,
            &format!(
                "Command {:?} with args {:?} did not execute successfully (status code {}).",
                cmd, program, status
            ),
        ))
    }
}

fn spawn(cmd: &mut Command, program: &str) -> Result<(Child, JoinHandle<()>), Error> {
    println!("running: {:?}", cmd);

    // Capture the standard error coming from these programs, and write it out
    // with cargo:warning= prefixes. Note that this is a bit wonky to avoid
    // requiring the output to be UTF-8, we instead just ship bytes from one
    // location to another.
    match cmd.stderr(Stdio::piped()).spawn() {
        Ok(mut child) => {
            let stderr = BufReader::new(child.stderr.take().unwrap());
            let print = thread::spawn(move || {
                for line in stderr.split(b'\n').filter_map(|l| l.ok()) {
                    print!("cargo:warning=");
                    std::io::stdout().write_all(&line).unwrap();
                    println!("");
                }
            });
            Ok((child, print))
        }
        Err(ref e) if e.kind() == io::ErrorKind::NotFound => {
            let extra = if cfg!(windows) {
                " (see https://github.com/alexcrichton/cc-rs#compile-time-requirements \
                 for help)"
            } else {
                ""
            };
            Err(Error::new(
                ErrorKind::ToolNotFound,
                &format!("Failed to find tool. Is `{}` installed?{}", program, extra),
            ))
        }
        Err(_) => Err(Error::new(
            ErrorKind::ToolExecError,
            &format!("Command {:?} with args {:?} failed to start.", cmd, program),
        )),
    }
}

fn fail(s: &str) -> ! {
    panic!("\n\nInternal error occurred: {}\n\n", s)
}

fn command_add_output_file(cmd: &mut Command, dst: &Path, msvc: bool, is_asm: bool, is_arm: bool) {
    if msvc && is_asm && is_arm {
        cmd.arg("-o").arg(&dst);
    } else if msvc && is_asm {
        cmd.arg("/Fo").arg(dst);
    } else if msvc {
        let mut s = OsString::from("/Fo");
        s.push(&dst);
        cmd.arg(s);
    } else {
        cmd.arg("-o").arg(&dst);
    }
}
