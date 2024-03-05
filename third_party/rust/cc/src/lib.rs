//! A library for [Cargo build scripts](https://doc.rust-lang.org/cargo/reference/build-scripts.html)
//! to compile a set of C/C++/assembly/CUDA files into a static archive for Cargo
//! to link into the crate being built. This crate does not compile code itself;
//! it calls out to the default compiler for the platform. This crate will
//! automatically detect situations such as cross compilation and
//! [various environment variables](#external-configuration-via-environment-variables) and will build code appropriately.
//!
//! # Example
//!
//! First, you'll want to both add a build script for your crate (`build.rs`) and
//! also add this crate to your `Cargo.toml` via:
//!
//! ```toml
//! [build-dependencies]
//! cc = "1.0"
//! ```
//!
//! Next up, you'll want to write a build script like so:
//!
//! ```rust,no_run
//! // build.rs
//!
//! fn main() {
//!     cc::Build::new()
//!         .file("foo.c")
//!         .file("bar.c")
//!         .compile("foo");
//! }
//! ```
//!
//! And that's it! Running `cargo build` should take care of the rest and your Rust
//! application will now have the C files `foo.c` and `bar.c` compiled into a file
//! named `libfoo.a`. If the C files contain
//!
//! ```c
//! void foo_function(void) { ... }
//! ```
//!
//! and
//!
//! ```c
//! int32_t bar_function(int32_t x) { ... }
//! ```
//!
//! you can call them from Rust by declaring them in
//! your Rust code like so:
//!
//! ```rust,no_run
//! extern "C" {
//!     fn foo_function();
//!     fn bar_function(x: i32) -> i32;
//! }
//!
//! pub fn call() {
//!     unsafe {
//!         foo_function();
//!         bar_function(42);
//!     }
//! }
//!
//! fn main() {
//!     call();
//! }
//! ```
//!
//! See [the Rustonomicon](https://doc.rust-lang.org/nomicon/ffi.html) for more details.
//!
//! # External configuration via environment variables
//!
//! To control the programs and flags used for building, the builder can set a
//! number of different environment variables.
//!
//! * `CFLAGS` - a series of space separated flags passed to compilers. Note that
//! individual flags cannot currently contain spaces, so doing
//! something like: `-L=foo\ bar` is not possible.
//! * `CC` - the actual C compiler used. Note that this is used as an exact
//! executable name, so (for example) no extra flags can be passed inside
//! this variable, and the builder must ensure that there aren't any
//! trailing spaces. This compiler must understand the `-c` flag. For
//! certain `TARGET`s, it also is assumed to know about other flags (most
//! common is `-fPIC`).
//! * `AR` - the `ar` (archiver) executable to use to build the static library.
//! * `CRATE_CC_NO_DEFAULTS` - the default compiler flags may cause conflicts in
//! some cross compiling scenarios. Setting this variable
//! will disable the generation of default compiler
//! flags.
//! * `CC_ENABLE_DEBUG_OUTPUT` - if set, compiler command invocations and exit codes will
//! be logged to stdout. This is useful for debugging build script issues, but can be
//! overly verbose for normal use.
//! * `CXX...` - see [C++ Support](#c-support).
//!
//! Furthermore, projects using this crate may specify custom environment variables
//! to be inspected, for example via the `Build::try_flags_from_environment`
//! function. Consult the project’s own documentation or its use of the `cc` crate
//! for any additional variables it may use.
//!
//! Each of these variables can also be supplied with certain prefixes and suffixes,
//! in the following prioritized order:
//!
//!   1. `<var>_<target>` - for example, `CC_x86_64-unknown-linux-gnu`
//!   2. `<var>_<target_with_underscores>` - for example, `CC_x86_64_unknown_linux_gnu`
//!   3. `<build-kind>_<var>` - for example, `HOST_CC` or `TARGET_CFLAGS`
//!   4. `<var>` - a plain `CC`, `AR` as above.
//!
//! If none of these variables exist, cc-rs uses built-in defaults.
//!
//! In addition to the above optional environment variables, `cc-rs` has some
//! functions with hard requirements on some variables supplied by [cargo's
//! build-script driver][cargo] that it has the `TARGET`, `OUT_DIR`, `OPT_LEVEL`,
//! and `HOST` variables.
//!
//! [cargo]: https://doc.rust-lang.org/cargo/reference/build-scripts.html#inputs-to-the-build-script
//!
//! # Optional features
//!
//! ## Parallel
//!
//! Currently cc-rs supports parallel compilation (think `make -jN`) but this
//! feature is turned off by default. To enable cc-rs to compile C/C++ in parallel,
//! you can change your dependency to:
//!
//! ```toml
//! [build-dependencies]
//! cc = { version = "1.0", features = ["parallel"] }
//! ```
//!
//! By default cc-rs will limit parallelism to `$NUM_JOBS`, or if not present it
//! will limit it to the number of cpus on the machine. If you are using cargo,
//! use `-jN` option of `build`, `test` and `run` commands as `$NUM_JOBS`
//! is supplied by cargo.
//!
//! # Compile-time Requirements
//!
//! To work properly this crate needs access to a C compiler when the build script
//! is being run. This crate does not ship a C compiler with it. The compiler
//! required varies per platform, but there are three broad categories:
//!
//! * Unix platforms require `cc` to be the C compiler. This can be found by
//! installing cc/clang on Linux distributions and Xcode on macOS, for example.
//! * Windows platforms targeting MSVC (e.g. your target triple ends in `-msvc`)
//! require Visual Studio to be installed. `cc-rs` attempts to locate it, and
//! if it fails, `cl.exe` is expected to be available in `PATH`. This can be
//! set up by running the appropriate developer tools shell.
//! * Windows platforms targeting MinGW (e.g. your target triple ends in `-gnu`)
//! require `cc` to be available in `PATH`. We recommend the
//! [MinGW-w64](https://www.mingw-w64.org/) distribution, which is using the
//! [Win-builds](http://win-builds.org/) installation system.
//! You may also acquire it via
//! [MSYS2](https://www.msys2.org/), as explained [here][msys2-help].  Make sure
//! to install the appropriate architecture corresponding to your installation of
//! rustc. GCC from older [MinGW](http://www.mingw.org/) project is compatible
//! only with 32-bit rust compiler.
//!
//! [msys2-help]: https://github.com/rust-lang/rust#building-on-windows
//!
//! # C++ support
//!
//! `cc-rs` supports C++ libraries compilation by using the `cpp` method on
//! `Build`:
//!
//! ```rust,no_run
//! fn main() {
//!     cc::Build::new()
//!         .cpp(true) // Switch to C++ library compilation.
//!         .file("foo.cpp")
//!         .compile("foo");
//! }
//! ```
//!
//! For C++ libraries, the `CXX` and `CXXFLAGS` environment variables are used instead of `CC` and `CFLAGS`.
//!
//! The C++ standard library may be linked to the crate target. By default it's `libc++` for macOS, FreeBSD, and OpenBSD, `libc++_shared` for Android, nothing for MSVC, and `libstdc++` for anything else. It can be changed in one of two ways:
//!
//! 1. by using the `cpp_link_stdlib` method on `Build`:
//! ```rust,no_run
//! fn main() {
//!     cc::Build::new()
//!         .cpp(true)
//!         .file("foo.cpp")
//!         .cpp_link_stdlib("stdc++") // use libstdc++
//!         .compile("foo");
//! }
//! ```
//! 2. by setting the `CXXSTDLIB` environment variable.
//!
//! In particular, for Android you may want to [use `c++_static` if you have at most one shared library](https://developer.android.com/ndk/guides/cpp-support).
//!
//! Remember that C++ does name mangling so `extern "C"` might be required to enable Rust linker to find your functions.
//!
//! # CUDA C++ support
//!
//! `cc-rs` also supports compiling CUDA C++ libraries by using the `cuda` method
//! on `Build`:
//!
//! ```rust,no_run
//! fn main() {
//!     cc::Build::new()
//!         // Switch to CUDA C++ library compilation using NVCC.
//!         .cuda(true)
//!         .cudart("static")
//!         // Generate code for Maxwell (GTX 970, 980, 980 Ti, Titan X).
//!         .flag("-gencode").flag("arch=compute_52,code=sm_52")
//!         // Generate code for Maxwell (Jetson TX1).
//!         .flag("-gencode").flag("arch=compute_53,code=sm_53")
//!         // Generate code for Pascal (GTX 1070, 1080, 1080 Ti, Titan Xp).
//!         .flag("-gencode").flag("arch=compute_61,code=sm_61")
//!         // Generate code for Pascal (Tesla P100).
//!         .flag("-gencode").flag("arch=compute_60,code=sm_60")
//!         // Generate code for Pascal (Jetson TX2).
//!         .flag("-gencode").flag("arch=compute_62,code=sm_62")
//!         // Generate code in parallel
//!         .flag("-t0")
//!         .file("bar.cu")
//!         .compile("bar");
//! }
//! ```

#![doc(html_root_url = "https://docs.rs/cc/1.0")]
#![cfg_attr(test, deny(warnings))]
#![allow(deprecated)]
#![deny(missing_docs)]

use std::borrow::Cow;
use std::collections::HashMap;
use std::env;
use std::ffi::{OsStr, OsString};
use std::fmt::{self, Display, Formatter};
use std::fs;
use std::io::{self, Write};
use std::path::{Component, Path, PathBuf};
#[cfg(feature = "parallel")]
use std::process::Child;
use std::process::Command;
use std::sync::{Arc, Mutex};

#[cfg(feature = "parallel")]
mod parallel;
mod windows;
// Regardless of whether this should be in this crate's public API,
// it has been since 2015, so don't break it.
pub use windows::find_tools as windows_registry;

mod command_helpers;
use command_helpers::*;

mod tool;
pub use tool::Tool;
use tool::ToolFamily;

/// A builder for compilation of a native library.
///
/// A `Build` is the main type of the `cc` crate and is used to control all the
/// various configuration options and such of a compile. You'll find more
/// documentation on each method itself.
#[derive(Clone, Debug)]
pub struct Build {
    include_directories: Vec<Arc<Path>>,
    definitions: Vec<(Arc<str>, Option<Arc<str>>)>,
    objects: Vec<Arc<Path>>,
    flags: Vec<Arc<str>>,
    flags_supported: Vec<Arc<str>>,
    known_flag_support_status: Arc<Mutex<HashMap<String, bool>>>,
    ar_flags: Vec<Arc<str>>,
    asm_flags: Vec<Arc<str>>,
    no_default_flags: bool,
    files: Vec<Arc<Path>>,
    cpp: bool,
    cpp_link_stdlib: Option<Option<Arc<str>>>,
    cpp_set_stdlib: Option<Arc<str>>,
    cuda: bool,
    cudart: Option<Arc<str>>,
    std: Option<Arc<str>>,
    target: Option<Arc<str>>,
    host: Option<Arc<str>>,
    out_dir: Option<Arc<Path>>,
    opt_level: Option<Arc<str>>,
    debug: Option<bool>,
    force_frame_pointer: Option<bool>,
    env: Vec<(Arc<OsStr>, Arc<OsStr>)>,
    compiler: Option<Arc<Path>>,
    archiver: Option<Arc<Path>>,
    ranlib: Option<Arc<Path>>,
    cargo_output: CargoOutput,
    link_lib_modifiers: Vec<Arc<str>>,
    pic: Option<bool>,
    use_plt: Option<bool>,
    static_crt: Option<bool>,
    shared_flag: Option<bool>,
    static_flag: Option<bool>,
    warnings_into_errors: bool,
    warnings: Option<bool>,
    extra_warnings: Option<bool>,
    env_cache: Arc<Mutex<HashMap<String, Option<Arc<str>>>>>,
    apple_sdk_root_cache: Arc<Mutex<HashMap<String, OsString>>>,
    apple_versions_cache: Arc<Mutex<HashMap<String, String>>>,
    emit_rerun_if_env_changed: bool,
    cached_compiler_family: Arc<Mutex<HashMap<Box<Path>, ToolFamily>>>,
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
    /// One of the function arguments failed validation.
    InvalidArgument,
    #[cfg(feature = "parallel")]
    /// jobserver helpthread failure
    JobserverHelpThreadError,
}

/// Represents an internal error that occurred, with an explanation.
#[derive(Clone, Debug)]
pub struct Error {
    /// Describes the kind of error that occurred.
    kind: ErrorKind,
    /// More explanation of error that occurred.
    message: Cow<'static, str>,
}

impl Error {
    fn new(kind: ErrorKind, message: impl Into<Cow<'static, str>>) -> Error {
        Error {
            kind,
            message: message.into(),
        }
    }
}

impl From<io::Error> for Error {
    fn from(e: io::Error) -> Error {
        Error::new(ErrorKind::IOError, format!("{}", e))
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:?}: {}", self.kind, self.message)
    }
}

impl std::error::Error for Error {}

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
        Object { src, dst }
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
            ar_flags: Vec::new(),
            asm_flags: Vec::new(),
            no_default_flags: false,
            files: Vec::new(),
            shared_flag: None,
            static_flag: None,
            cpp: false,
            cpp_link_stdlib: None,
            cpp_set_stdlib: None,
            cuda: false,
            cudart: None,
            std: None,
            target: None,
            host: None,
            out_dir: None,
            opt_level: None,
            debug: None,
            force_frame_pointer: None,
            env: Vec::new(),
            compiler: None,
            archiver: None,
            ranlib: None,
            cargo_output: CargoOutput::new(),
            link_lib_modifiers: Vec::new(),
            pic: None,
            use_plt: None,
            static_crt: None,
            warnings: None,
            extra_warnings: None,
            warnings_into_errors: false,
            env_cache: Arc::new(Mutex::new(HashMap::new())),
            apple_sdk_root_cache: Arc::new(Mutex::new(HashMap::new())),
            apple_versions_cache: Arc::new(Mutex::new(HashMap::new())),
            emit_rerun_if_env_changed: true,
            cached_compiler_family: Arc::default(),
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
        self.include_directories.push(dir.as_ref().into());
        self
    }

    /// Add multiple directories to the `-I` include path.
    ///
    /// # Example
    ///
    /// ```no_run
    /// # use std::path::Path;
    /// # let condition = true;
    /// #
    /// let mut extra_dir = None;
    /// if condition {
    ///     extra_dir = Some(Path::new("/path/to"));
    /// }
    ///
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .includes(extra_dir)
    ///     .compile("foo");
    /// ```
    pub fn includes<P>(&mut self, dirs: P) -> &mut Build
    where
        P: IntoIterator,
        P::Item: AsRef<Path>,
    {
        for dir in dirs {
            self.include(dir);
        }
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
            .push((var.into(), val.into().map(Into::into)));
        self
    }

    /// Add an arbitrary object file to link in
    pub fn object<P: AsRef<Path>>(&mut self, obj: P) -> &mut Build {
        self.objects.push(obj.as_ref().into());
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
        self.flags.push(flag.into());
        self
    }

    /// Removes a compiler flag that was added by [`Build::flag`].
    ///
    /// Will not remove flags added by other means (default flags,
    /// flags from env, and so on).
    ///
    /// # Example
    /// ```
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .flag("unwanted_flag")
    ///     .remove_flag("unwanted_flag");
    /// ```

    pub fn remove_flag(&mut self, flag: &str) -> &mut Build {
        self.flags.retain(|other_flag| &**other_flag != flag);
        self
    }

    /// Add a flag to the invocation of the ar
    ///
    /// # Example
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .file("src/bar.c")
    ///     .ar_flag("/NODEFAULTLIB:libc.dll")
    ///     .compile("foo");
    /// ```
    pub fn ar_flag(&mut self, flag: &str) -> &mut Build {
        self.ar_flags.push(flag.into());
        self
    }

    /// Add a flag that will only be used with assembly files.
    ///
    /// The flag will be applied to input files with either a `.s` or
    /// `.asm` extension (case insensitive).
    ///
    /// # Example
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .asm_flag("-Wa,-defsym,abc=1")
    ///     .file("src/foo.S")  // The asm flag will be applied here
    ///     .file("src/bar.c")  // The asm flag will not be applied here
    ///     .compile("foo");
    /// ```
    pub fn asm_flag(&mut self, flag: &str) -> &mut Build {
        self.asm_flags.push(flag.into());
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
    /// It may return error if it's unable to run the compiler with a test file
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
            .cargo_metadata(self.cargo_output.metadata)
            .target(&target)
            .opt_level(0)
            .host(&host)
            .debug(false)
            .cpp(self.cpp)
            .cuda(self.cuda);
        if let Some(ref c) = self.compiler {
            cfg.compiler(c.clone());
        }
        let mut compiler = cfg.try_get_compiler()?;

        // Clang uses stderr for verbose output, which yields a false positive
        // result if the CFLAGS/CXXFLAGS include -v to aid in debugging.
        if compiler.family.verbose_stderr() {
            compiler.remove_arg("-v".into());
        }
        if compiler.family == ToolFamily::Clang {
            // Avoid reporting that the arg is unsupported just because the
            // compiler complains that it wasn't used.
            compiler.push_cc_arg("-Wno-unused-command-line-argument".into());
        }

        let mut cmd = compiler.to_command();
        let is_arm = target.contains("aarch64") || target.contains("arm");
        let clang = compiler.family == ToolFamily::Clang;
        let gnu = compiler.family == ToolFamily::Gnu;
        command_add_output_file(
            &mut cmd,
            &obj,
            self.cuda,
            target.contains("msvc"),
            clang,
            gnu,
            false,
            is_arm,
        );

        // Checking for compiler flags does not require linking
        cmd.arg("-c");

        cmd.arg(&src);

        let output = cmd.output()?;
        let is_supported = output.status.success() && output.stderr.is_empty();

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
        self.flags_supported.push(flag.into());
        self
    }

    /// Add flags from the specified environment variable.
    ///
    /// Normally the `cc` crate will consult with the standard set of environment
    /// variables (such as `CFLAGS` and `CXXFLAGS`) to construct the compiler invocation. Use of
    /// this method provides additional levers for the end user to use when configuring the build
    /// process.
    ///
    /// Just like the standard variables, this method will search for an environment variable with
    /// appropriate target prefixes, when appropriate.
    ///
    /// # Examples
    ///
    /// This method is particularly beneficial in introducing the ability to specify crate-specific
    /// flags.
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .file("src/foo.c")
    ///     .try_flags_from_environment(concat!(env!("CARGO_PKG_NAME"), "_CFLAGS"))
    ///     .expect("the environment variable must be specified and UTF-8")
    ///     .compile("foo");
    /// ```
    ///
    pub fn try_flags_from_environment(&mut self, environ_key: &str) -> Result<&mut Build, Error> {
        let flags = self.envflags(environ_key)?;
        self.flags.extend(flags.into_iter().map(Into::into));
        Ok(self)
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

    /// Disables the generation of default compiler flags. The default compiler
    /// flags may cause conflicts in some cross compiling scenarios.
    ///
    /// Setting the `CRATE_CC_NO_DEFAULTS` environment variable has the same
    /// effect as setting this to `true`. The presence of the environment
    /// variable and the value of `no_default_flags` will be OR'd together.
    pub fn no_default_flags(&mut self, no_default_flags: bool) -> &mut Build {
        self.no_default_flags = no_default_flags;
        self
    }

    /// Add a file which will be compiled
    pub fn file<P: AsRef<Path>>(&mut self, p: P) -> &mut Build {
        self.files.push(p.as_ref().into());
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

    /// Get the files which will be compiled
    pub fn get_files(&self) -> impl Iterator<Item = &Path> {
        self.files.iter().map(AsRef::as_ref)
    }

    /// Set C++ support.
    ///
    /// The other `cpp_*` options will only become active if this is set to
    /// `true`.
    ///
    /// The name of the C++ standard library to link is decided by:
    /// 1. If [`cpp_link_stdlib`](Build::cpp_link_stdlib) is set, use its value.
    /// 2. Else if the `CXXSTDLIB` environment variable is set, use its value.
    /// 3. Else the default is `libc++` for OS X and BSDs, `libc++_shared` for Android,
    /// `None` for MSVC and `libstdc++` for anything else.
    pub fn cpp(&mut self, cpp: bool) -> &mut Build {
        self.cpp = cpp;
        self
    }

    /// Set CUDA C++ support.
    ///
    /// Enabling CUDA will invoke the CUDA compiler, NVCC. While NVCC accepts
    /// the most common compiler flags, e.g. `-std=c++17`, some project-specific
    /// flags might have to be prefixed with "-Xcompiler" flag, for example as
    /// `.flag("-Xcompiler").flag("-fpermissive")`. See the documentation for
    /// `nvcc`, the CUDA compiler driver, at <https://docs.nvidia.com/cuda/cuda-compiler-driver-nvcc/>
    /// for more information.
    ///
    /// If enabled, this also implicitly enables C++ support.
    pub fn cuda(&mut self, cuda: bool) -> &mut Build {
        self.cuda = cuda;
        if cuda {
            self.cpp = true;
            self.cudart = Some("static".into());
        }
        self
    }

    /// Link CUDA run-time.
    ///
    /// This option mimics the `--cudart` NVCC command-line option. Just like
    /// the original it accepts `{none|shared|static}`, with default being
    /// `static`. The method has to be invoked after `.cuda(true)`, or not
    /// at all, if the default is right for the project.
    pub fn cudart(&mut self, cudart: &str) -> &mut Build {
        if self.cuda {
            self.cudart = Some(cudart.into());
        }
        self
    }

    /// Specify the C or C++ language standard version.
    ///
    /// These values are common to modern versions of GCC, Clang and MSVC:
    /// - `c11` for ISO/IEC 9899:2011
    /// - `c17` for ISO/IEC 9899:2018
    /// - `c++14` for ISO/IEC 14882:2014
    /// - `c++17` for ISO/IEC 14882:2017
    /// - `c++20` for ISO/IEC 14882:2020
    ///
    /// Other values have less broad support, e.g. MSVC does not support `c++11`
    /// (`c++14` is the minimum), `c89` (omit the flag instead) or `c99`.
    ///
    /// For compiling C++ code, you should also set `.cpp(true)`.
    ///
    /// The default is that no standard flag is passed to the compiler, so the
    /// language version will be the compiler's default.
    ///
    /// # Example
    ///
    /// ```no_run
    /// cc::Build::new()
    ///     .file("src/modern.cpp")
    ///     .cpp(true)
    ///     .std("c++17")
    ///     .compile("modern");
    /// ```
    pub fn std(&mut self, std: &str) -> &mut Build {
        self.std = Some(std.into());
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
    /// - "-Wall" for MSVC.
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
    /// If the `CXXSTDLIB` environment variable is set, its value will
    /// override the default value, but not the value explicitly set by calling
    /// this function.
    ///
    /// A value of `None` indicates that no automatic linking should happen,
    /// otherwise cargo will link against the specified library.
    ///
    /// The given library name must not contain the `lib` prefix.
    ///
    /// Common values:
    /// - `stdc++` for GNU
    /// - `c++` for Clang
    /// - `c++_shared` or `c++_static` for Android
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
    /// caller to ensure that this option is only used in conjunction with a
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
        self.target = Some(target.into());
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
        self.host = Some(host.into());
        self
    }

    /// Configures the optimization level of the generated object files.
    ///
    /// This option is automatically scraped from the `OPT_LEVEL` environment
    /// variable by build scripts, so it's not required to call this function.
    pub fn opt_level(&mut self, opt_level: u32) -> &mut Build {
        self.opt_level = Some(opt_level.to_string().into());
        self
    }

    /// Configures the optimization level of the generated object files.
    ///
    /// This option is automatically scraped from the `OPT_LEVEL` environment
    /// variable by build scripts, so it's not required to call this function.
    pub fn opt_level_str(&mut self, opt_level: &str) -> &mut Build {
        self.opt_level = Some(opt_level.into());
        self
    }

    /// Configures whether the compiler will emit debug information when
    /// generating object files.
    ///
    /// This option is automatically scraped from the `DEBUG` environment
    /// variable by build scripts, so it's not required to call this function.
    pub fn debug(&mut self, debug: bool) -> &mut Build {
        self.debug = Some(debug);
        self
    }

    /// Configures whether the compiler will emit instructions to store
    /// frame pointers during codegen.
    ///
    /// This option is automatically enabled when debug information is emitted.
    /// Otherwise the target platform compiler's default will be used.
    /// You can use this option to force a specific setting.
    pub fn force_frame_pointer(&mut self, force: bool) -> &mut Build {
        self.force_frame_pointer = Some(force);
        self
    }

    /// Configures the output directory where all object files and static
    /// libraries will be located.
    ///
    /// This option is automatically scraped from the `OUT_DIR` environment
    /// variable by build scripts, so it's not required to call this function.
    pub fn out_dir<P: AsRef<Path>>(&mut self, out_dir: P) -> &mut Build {
        self.out_dir = Some(out_dir.as_ref().into());
        self
    }

    /// Configures the compiler to be used to produce output.
    ///
    /// This option is automatically determined from the target platform or a
    /// number of environment variables, so it's not required to call this
    /// function.
    pub fn compiler<P: AsRef<Path>>(&mut self, compiler: P) -> &mut Build {
        self.compiler = Some(compiler.as_ref().into());
        self
    }

    /// Configures the tool used to assemble archives.
    ///
    /// This option is automatically determined from the target platform or a
    /// number of environment variables, so it's not required to call this
    /// function.
    pub fn archiver<P: AsRef<Path>>(&mut self, archiver: P) -> &mut Build {
        self.archiver = Some(archiver.as_ref().into());
        self
    }

    /// Configures the tool used to index archives.
    ///
    /// This option is automatically determined from the target platform or a
    /// number of environment variables, so it's not required to call this
    /// function.
    pub fn ranlib<P: AsRef<Path>>(&mut self, ranlib: P) -> &mut Build {
        self.ranlib = Some(ranlib.as_ref().into());
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
    ///  - If `emit_rerun_if_env_changed` is not `false`, `rerun-if-env-changed=`*env*
    ///
    pub fn cargo_metadata(&mut self, cargo_metadata: bool) -> &mut Build {
        self.cargo_output.metadata = cargo_metadata;
        self
    }

    /// Define whether compile warnings should be emitted for cargo. Defaults to
    /// `true`.
    ///
    /// If disabled, compiler messages will not be printed.
    /// Issues unrelated to the compilation will always produce cargo warnings regardless of this setting.
    pub fn cargo_warnings(&mut self, cargo_warnings: bool) -> &mut Build {
        self.cargo_output.warnings = cargo_warnings;
        self
    }

    /// Define whether debug information should be emitted for cargo. Defaults to whether
    /// or not the environment variable `CC_ENABLE_DEBUG_OUTPUT` is set.
    ///
    /// If enabled, the compiler will emit debug information when generating object files,
    /// such as the command invoked and the exit status.
    pub fn cargo_debug(&mut self, cargo_debug: bool) -> &mut Build {
        self.cargo_output.debug = cargo_debug;
        self
    }

    /// Adds a native library modifier that will be added to the
    /// `rustc-link-lib=static:MODIFIERS=LIBRARY_NAME` metadata line
    /// emitted for cargo if `cargo_metadata` is enabled.
    /// See <https://doc.rust-lang.org/rustc/command-line-arguments.html#-l-link-the-generated-crate-to-a-native-library>
    /// for the list of modifiers accepted by rustc.
    pub fn link_lib_modifier(&mut self, link_lib_modifier: &str) -> &mut Build {
        self.link_lib_modifiers.push(link_lib_modifier.into());
        self
    }

    /// Configures whether the compiler will emit position independent code.
    ///
    /// This option defaults to `false` for `windows-gnu` and bare metal targets and
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

    /// Define whether metadata should be emitted for cargo to detect environment
    /// changes that should trigger a rebuild.
    ///
    /// This has no effect if the `cargo_metadata` option is `false`.
    ///
    /// This option defaults to `true`.
    pub fn emit_rerun_if_env_changed(&mut self, emit_rerun_if_env_changed: bool) -> &mut Build {
        self.emit_rerun_if_env_changed = emit_rerun_if_env_changed;
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
        self.env.push((a.as_ref().into(), b.as_ref().into()));
        self
    }

    /// Run the compiler, generating the file `output`
    ///
    /// This will return a result instead of panicking; see compile() for the complete description.
    pub fn try_compile(&self, output: &str) -> Result<(), Error> {
        let mut output_components = Path::new(output).components();
        match (output_components.next(), output_components.next()) {
            (Some(Component::Normal(_)), None) => {}
            _ => {
                return Err(Error::new(
                    ErrorKind::InvalidArgument,
                    "argument of `compile` must be a single normal path component",
                ));
            }
        }

        let (lib_name, gnu_lib_name) = if output.starts_with("lib") && output.ends_with(".a") {
            (&output[3..output.len() - 2], output.to_owned())
        } else {
            let mut gnu = String::with_capacity(5 + output.len());
            gnu.push_str("lib");
            gnu.push_str(output);
            gnu.push_str(".a");
            (output, gnu)
        };
        let dst = self.get_out_dir()?;

        let objects = objects_from_files(&self.files, &dst)?;

        self.compile_objects(&objects)?;
        self.assemble(lib_name, &dst.join(gnu_lib_name), &objects)?;

        if self.get_target()?.contains("msvc") {
            let compiler = self.get_base_compiler()?;
            let atlmfc_lib = compiler
                .env()
                .iter()
                .find(|&(var, _)| var.as_os_str() == OsStr::new("LIB"))
                .and_then(|(_, lib_paths)| {
                    env::split_paths(lib_paths).find(|path| {
                        let sub = Path::new("atlmfc/lib");
                        path.ends_with(sub) || path.parent().map_or(false, |p| p.ends_with(sub))
                    })
                });

            if let Some(atlmfc_lib) = atlmfc_lib {
                self.cargo_output.print_metadata(&format_args!(
                    "cargo:rustc-link-search=native={}",
                    atlmfc_lib.display()
                ));
            }
        }

        if self.link_lib_modifiers.is_empty() {
            self.cargo_output
                .print_metadata(&format_args!("cargo:rustc-link-lib=static={}", lib_name));
        } else {
            let m = self.link_lib_modifiers.join(",");
            self.cargo_output.print_metadata(&format_args!(
                "cargo:rustc-link-lib=static:{}={}",
                m, lib_name
            ));
        }
        self.cargo_output.print_metadata(&format_args!(
            "cargo:rustc-link-search=native={}",
            dst.display()
        ));

        // Add specific C++ libraries, if enabled.
        if self.cpp {
            if let Some(stdlib) = self.get_cpp_link_stdlib()? {
                self.cargo_output
                    .print_metadata(&format_args!("cargo:rustc-link-lib={}", stdlib));
            }
        }

        let cudart = match &self.cudart {
            Some(opt) => &*opt, // {none|shared|static}
            None => "none",
        };
        if cudart != "none" {
            if let Some(nvcc) = which(&self.get_compiler().path, None) {
                // Try to figure out the -L search path. If it fails,
                // it's on user to specify one by passing it through
                // RUSTFLAGS environment variable.
                let mut libtst = false;
                let mut libdir = nvcc;
                libdir.pop(); // remove 'nvcc'
                libdir.push("..");
                let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap();
                if cfg!(target_os = "linux") {
                    libdir.push("targets");
                    libdir.push(target_arch.to_owned() + "-linux");
                    libdir.push("lib");
                    libtst = true;
                } else if cfg!(target_env = "msvc") {
                    libdir.push("lib");
                    match target_arch.as_str() {
                        "x86_64" => {
                            libdir.push("x64");
                            libtst = true;
                        }
                        "x86" => {
                            libdir.push("Win32");
                            libtst = true;
                        }
                        _ => libtst = false,
                    }
                }
                if libtst && libdir.is_dir() {
                    self.cargo_output.print_metadata(&format_args!(
                        "cargo:rustc-link-search=native={}",
                        libdir.to_str().unwrap()
                    ));
                }

                // And now the -l flag.
                let lib = match cudart {
                    "shared" => "cudart",
                    "static" => "cudart_static",
                    bad => panic!("unsupported cudart option: {}", bad),
                };
                self.cargo_output
                    .print_metadata(&format_args!("cargo:rustc-link-lib={}", lib));
            }
        }

        Ok(())
    }

    /// Run the compiler, generating the file `output`
    ///
    /// # Library name
    ///
    /// The `output` string argument determines the file name for the compiled
    /// library. The Rust compiler will create an assembly named "lib"+output+".a".
    /// MSVC will create a file named output+".lib".
    ///
    /// The choice of `output` is close to arbitrary, but:
    ///
    /// - must be nonempty,
    /// - must not contain a path separator (`/`),
    /// - must be unique across all `compile` invocations made by the same build
    ///   script.
    ///
    /// If your build script compiles a single source file, the base name of
    /// that source file would usually be reasonable:
    ///
    /// ```no_run
    /// cc::Build::new().file("blobstore.c").compile("blobstore");
    /// ```
    ///
    /// Compiling multiple source files, some people use their crate's name, or
    /// their crate's name + "-cc".
    ///
    /// Otherwise, please use your imagination.
    ///
    /// For backwards compatibility, if `output` starts with "lib" *and* ends
    /// with ".a", a second "lib" prefix and ".a" suffix do not get added on,
    /// but this usage is deprecated; please omit `lib` and `.a` in the argument
    /// that you pass.
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

    /// Run the compiler, generating intermediate files, but without linking
    /// them into an archive file.
    ///
    /// This will return a list of compiled object files, in the same order
    /// as they were passed in as `file`/`files` methods.
    pub fn compile_intermediates(&self) -> Vec<PathBuf> {
        match self.try_compile_intermediates() {
            Ok(v) => v,
            Err(e) => fail(&e.message),
        }
    }

    /// Run the compiler, generating intermediate files, but without linking
    /// them into an archive file.
    ///
    /// This will return a result instead of panicking; see `compile_intermediates()` for the complete description.
    pub fn try_compile_intermediates(&self) -> Result<Vec<PathBuf>, Error> {
        let dst = self.get_out_dir()?;
        let objects = objects_from_files(&self.files, &dst)?;

        self.compile_objects(&objects)?;

        Ok(objects.into_iter().map(|v| v.dst).collect())
    }

    #[cfg(feature = "parallel")]
    fn compile_objects(&self, objs: &[Object]) -> Result<(), Error> {
        use std::cell::Cell;

        use parallel::async_executor::{block_on, YieldOnce};

        if objs.len() <= 1 {
            for obj in objs {
                let (mut cmd, name) = self.create_compile_object_cmd(obj)?;
                run(&mut cmd, &name, &self.cargo_output)?;
            }

            return Ok(());
        }

        // Limit our parallelism globally with a jobserver.
        let tokens = parallel::job_token::ActiveJobTokenServer::new()?;

        // When compiling objects in parallel we do a few dirty tricks to speed
        // things up:
        //
        // * First is that we use the `jobserver` crate to limit the parallelism
        //   of this build script. The `jobserver` crate will use a jobserver
        //   configured by Cargo for build scripts to ensure that parallelism is
        //   coordinated across C compilations and Rust compilations. Before we
        //   compile anything we make sure to wait until we acquire a token.
        //
        //   Note that this jobserver is cached globally so we only used one per
        //   process and only worry about creating it once.
        //
        // * Next we use spawn the process to actually compile objects in
        //   parallel after we've acquired a token to perform some work
        //
        // With all that in mind we compile all objects in a loop here, after we
        // acquire the appropriate tokens, Once all objects have been compiled
        // we wait on all the processes and propagate the results of compilation.

        let pendings = Cell::new(Vec::<(
            Command,
            String,
            KillOnDrop,
            parallel::job_token::JobToken,
        )>::new());
        let is_disconnected = Cell::new(false);
        let has_made_progress = Cell::new(false);

        let wait_future = async {
            let mut error = None;
            // Buffer the stdout
            let mut stdout = io::BufWriter::with_capacity(128, io::stdout());

            loop {
                // If the other end of the pipe is already disconnected, then we're not gonna get any new jobs,
                // so it doesn't make sense to reuse the tokens; in fact,
                // releasing them as soon as possible (once we know that the other end is disconnected) is beneficial.
                // Imagine that the last file built takes an hour to finish; in this scenario,
                // by not releasing the tokens before that last file is done we would effectively block other processes from
                // starting sooner - even though we only need one token for that last file, not N others that were acquired.

                let mut pendings_is_empty = false;

                cell_update(&pendings, |mut pendings| {
                    // Try waiting on them.
                    parallel::retain_unordered_mut(
                        &mut pendings,
                        |(cmd, program, child, _token)| {
                            match try_wait_on_child(
                                cmd,
                                program,
                                &mut child.0,
                                &mut stdout,
                                &mut child.1,
                            ) {
                                Ok(Some(())) => {
                                    // Task done, remove the entry
                                    has_made_progress.set(true);
                                    false
                                }
                                Ok(None) => true, // Task still not finished, keep the entry
                                Err(err) => {
                                    // Task fail, remove the entry.
                                    // Since we can only return one error, log the error to make
                                    // sure users always see all the compilation failures.
                                    has_made_progress.set(true);

                                    if self.cargo_output.warnings {
                                        let _ = writeln!(stdout, "cargo:warning={}", err);
                                    }
                                    error = Some(err);

                                    false
                                }
                            }
                        },
                    );
                    pendings_is_empty = pendings.is_empty();
                    pendings
                });

                if pendings_is_empty && is_disconnected.get() {
                    break if let Some(err) = error {
                        Err(err)
                    } else {
                        Ok(())
                    };
                }

                YieldOnce::default().await;
            }
        };
        let spawn_future = async {
            for obj in objs {
                let (mut cmd, program) = self.create_compile_object_cmd(obj)?;
                let token = tokens.acquire().await?;
                let mut child = spawn(&mut cmd, &program, &self.cargo_output)?;
                let mut stderr_forwarder = StderrForwarder::new(&mut child);
                stderr_forwarder.set_non_blocking()?;

                cell_update(&pendings, |mut pendings| {
                    pendings.push((cmd, program, KillOnDrop(child, stderr_forwarder), token));
                    pendings
                });

                has_made_progress.set(true);
            }
            is_disconnected.set(true);

            Ok::<_, Error>(())
        };

        return block_on(wait_future, spawn_future, &has_made_progress);

        struct KillOnDrop(Child, StderrForwarder);

        impl Drop for KillOnDrop {
            fn drop(&mut self) {
                let child = &mut self.0;

                child.kill().ok();
            }
        }

        fn cell_update<T, F>(cell: &Cell<T>, f: F)
        where
            T: Default,
            F: FnOnce(T) -> T,
        {
            let old = cell.take();
            let new = f(old);
            cell.set(new);
        }
    }

    #[cfg(not(feature = "parallel"))]
    fn compile_objects(&self, objs: &[Object]) -> Result<(), Error> {
        for obj in objs {
            let (mut cmd, name) = self.create_compile_object_cmd(obj)?;
            run(&mut cmd, &name, &self.cargo_output)?;
        }

        Ok(())
    }

    fn create_compile_object_cmd(&self, obj: &Object) -> Result<(Command, String), Error> {
        let asm_ext = AsmFileExt::from_path(&obj.src);
        let is_asm = asm_ext.is_some();
        let target = self.get_target()?;
        let msvc = target.contains("msvc");
        let compiler = self.try_get_compiler()?;
        let clang = compiler.family == ToolFamily::Clang;
        let gnu = compiler.family == ToolFamily::Gnu;

        let is_assembler_msvc = msvc && asm_ext == Some(AsmFileExt::DotAsm);
        let (mut cmd, name) = if is_assembler_msvc {
            self.msvc_macro_assembler()?
        } else {
            let mut cmd = compiler.to_command();
            for (a, b) in self.env.iter() {
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
        command_add_output_file(
            &mut cmd, &obj.dst, self.cuda, msvc, clang, gnu, is_asm, is_arm,
        );
        // armasm and armasm64 don't requrie -c option
        if !is_assembler_msvc || !is_arm {
            cmd.arg("-c");
        }
        if self.cuda && self.cuda_file_count() > 1 {
            cmd.arg("--device-c");
        }
        if is_asm {
            cmd.args(self.asm_flags.iter().map(std::ops::Deref::deref));
        }
        if compiler.family == (ToolFamily::Msvc { clang_cl: true }) && !is_assembler_msvc {
            // #513: For `clang-cl`, separate flags/options from the input file.
            // When cross-compiling macOS -> Windows, this avoids interpreting
            // common `/Users/...` paths as the `/U` flag and triggering
            // `-Wslash-u-filename` warning.
            cmd.arg("--");
        }
        cmd.arg(&obj.src);
        if cfg!(target_os = "macos") {
            self.fix_env_for_apple_os(&mut cmd)?;
        }

        Ok((cmd, name))
    }

    /// This will return a result instead of panicking; see expand() for the complete description.
    pub fn try_expand(&self) -> Result<Vec<u8>, Error> {
        let compiler = self.try_get_compiler()?;
        let mut cmd = compiler.to_command();
        for (a, b) in self.env.iter() {
            cmd.env(a, b);
        }
        cmd.arg("-E");

        assert!(
            self.files.len() <= 1,
            "Expand may only be called for a single file"
        );

        let is_asm = self
            .files
            .iter()
            .map(std::ops::Deref::deref)
            .find_map(AsmFileExt::from_path)
            .is_some();

        if compiler.family == (ToolFamily::Msvc { clang_cl: true }) && !is_asm {
            // #513: For `clang-cl`, separate flags/options from the input file.
            // When cross-compiling macOS -> Windows, this avoids interpreting
            // common `/Users/...` paths as the `/U` flag and triggering
            // `-Wslash-u-filename` warning.
            cmd.arg("--");
        }

        cmd.args(self.files.iter().map(std::ops::Deref::deref));

        let name = compiler
            .path
            .file_name()
            .ok_or_else(|| Error::new(ErrorKind::IOError, "Failed to get compiler path."))?
            .to_string_lossy()
            .into_owned();

        Ok(run_output(&mut cmd, &name, &self.cargo_output)?)
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
    /// This will return a result instead of panicking; see
    /// [`get_compiler()`](Self::get_compiler) for the complete description.
    pub fn try_get_compiler(&self) -> Result<Tool, Error> {
        let opt_level = self.get_opt_level()?;
        let target = self.get_target()?;

        let mut cmd = self.get_base_compiler()?;

        // Disable default flag generation via `no_default_flags` or environment variable
        let no_defaults = self.no_default_flags || self.getenv("CRATE_CC_NO_DEFAULTS").is_some();

        if !no_defaults {
            self.add_default_flags(&mut cmd, &target, &opt_level)?;
        } else {
            println!("Info: default compiler flags are disabled");
        }

        if let Some(ref std) = self.std {
            let separator = match cmd.family {
                ToolFamily::Msvc { .. } => ':',
                ToolFamily::Gnu | ToolFamily::Clang => '=',
            };
            cmd.push_cc_arg(format!("-std{}{}", separator, std).into());
        }

        if let Ok(flags) = self.envflags(if self.cpp { "CXXFLAGS" } else { "CFLAGS" }) {
            for arg in flags {
                cmd.push_cc_arg(arg.into());
            }
        }

        for directory in self.include_directories.iter() {
            cmd.args.push("-I".into());
            cmd.args.push(directory.as_os_str().into());
        }

        // If warnings and/or extra_warnings haven't been explicitly set,
        // then we set them only if the environment doesn't already have
        // CFLAGS/CXXFLAGS, since those variables presumably already contain
        // the desired set of warnings flags.

        if self.warnings.unwrap_or(!self.has_flags()) {
            let wflags = cmd.family.warnings_flags().into();
            cmd.push_cc_arg(wflags);
        }

        if self.extra_warnings.unwrap_or(!self.has_flags()) {
            if let Some(wflags) = cmd.family.extra_warnings_flags() {
                cmd.push_cc_arg(wflags.into());
            }
        }

        for flag in self.flags.iter() {
            cmd.args.push((**flag).into());
        }

        for flag in self.flags_supported.iter() {
            if self.is_flag_supported(flag).unwrap_or(false) {
                cmd.push_cc_arg((**flag).into());
            }
        }

        for (key, value) in self.definitions.iter() {
            if let Some(ref value) = *value {
                cmd.args.push(format!("-D{}={}", key, value).into());
            } else {
                cmd.args.push(format!("-D{}", key).into());
            }
        }

        if self.warnings_into_errors {
            let warnings_to_errors_flag = cmd.family.warnings_to_errors_flag().into();
            cmd.push_cc_arg(warnings_to_errors_flag);
        }

        Ok(cmd)
    }

    fn add_default_flags(
        &self,
        cmd: &mut Tool,
        target: &str,
        opt_level: &str,
    ) -> Result<(), Error> {
        // Non-target flags
        // If the flag is not conditioned on target variable, it belongs here :)
        match cmd.family {
            ToolFamily::Msvc { .. } => {
                cmd.push_cc_arg("-nologo".into());

                let crt_flag = match self.static_crt {
                    Some(true) => "-MT",
                    Some(false) => "-MD",
                    None => {
                        let features = self.getenv("CARGO_CFG_TARGET_FEATURE");
                        let features = features.as_deref().unwrap_or_default();
                        if features.contains("crt-static") {
                            "-MT"
                        } else {
                            "-MD"
                        }
                    }
                };
                cmd.push_cc_arg(crt_flag.into());

                match &opt_level[..] {
                    // Msvc uses /O1 to enable all optimizations that minimize code size.
                    "z" | "s" | "1" => cmd.push_opt_unless_duplicate("-O1".into()),
                    // -O3 is a valid value for gcc and clang compilers, but not msvc. Cap to /O2.
                    "2" | "3" => cmd.push_opt_unless_duplicate("-O2".into()),
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

                if cmd.family == ToolFamily::Clang && target.contains("windows") {
                    // Disambiguate mingw and msvc on Windows. Problem is that
                    // depending on the origin clang can default to a mismatchig
                    // run-time.
                    cmd.push_cc_arg(format!("--target={}", target).into());
                }

                if cmd.family == ToolFamily::Clang && target.contains("android") {
                    // For compatibility with code that doesn't use pre-defined `__ANDROID__` macro.
                    // If compiler used via ndk-build or cmake (officially supported build methods)
                    // this macros is defined.
                    // See https://android.googlesource.com/platform/ndk/+/refs/heads/ndk-release-r21/build/cmake/android.toolchain.cmake#456
                    // https://android.googlesource.com/platform/ndk/+/refs/heads/ndk-release-r21/build/core/build-binary.mk#141
                    cmd.push_opt_unless_duplicate("-DANDROID".into());
                }

                if !target.contains("apple-ios")
                    && !target.contains("apple-watchos")
                    && !target.contains("apple-tvos")
                {
                    cmd.push_cc_arg("-ffunction-sections".into());
                    cmd.push_cc_arg("-fdata-sections".into());
                }
                // Disable generation of PIC on bare-metal for now: rust-lld doesn't support this yet
                if self.pic.unwrap_or(
                    !target.contains("windows")
                        && !target.contains("-none-")
                        && !target.contains("uefi"),
                ) {
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
                // NVCC debug flag
                cmd.args.push("-G".into());
            }
            let family = cmd.family;
            family.add_debug_flags(cmd, self.get_dwarf_version());
        }

        if self.get_force_frame_pointer() {
            let family = cmd.family;
            family.add_force_frame_pointer(cmd);
        }

        if !cmd.is_like_msvc() {
            if target.contains("i686") || target.contains("i586") {
                cmd.args.push("-m32".into());
            } else if target == "x86_64-unknown-linux-gnux32" {
                cmd.args.push("-mx32".into());
            } else if target.contains("x86_64") || target.contains("powerpc64") {
                cmd.args.push("-m64".into());
            }
        }

        // Target flags
        if target.contains("-apple-") {
            self.apple_flags(cmd, target)?;
        } else {
            self.target_flags(cmd, target);
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
                    self.cargo_output.print_warning(&format_args!("cpp_set_stdlib is specified, but the {:?} compiler does not support this option, ignored", cmd.family));
                }
            }
        }

        Ok(())
    }

    fn target_flags(&self, cmd: &mut Tool, target: &str) {
        match cmd.family {
            ToolFamily::Clang => {
                if !(target.contains("android") && cmd.has_internal_target_arg) {
                    if target.starts_with("riscv64gc-") {
                        cmd.args.push(
                            format!("--target={}", target.replace("riscv64gc", "riscv64")).into(),
                        );
                    } else if target.starts_with("riscv32gc-") {
                        cmd.args.push(
                            format!("--target={}", target.replace("riscv32gc", "riscv32")).into(),
                        );
                    } else if target.contains("uefi") {
                        if target.contains("x86_64") {
                            cmd.args.push("--target=x86_64-unknown-windows-gnu".into());
                        } else if target.contains("i686") {
                            cmd.args.push("--target=i686-unknown-windows-gnu".into())
                        } else if target.contains("aarch64") {
                            cmd.args.push("--target=aarch64-unknown-windows-gnu".into())
                        }
                    } else if target.ends_with("-freebsd") {
                        // FreeBSD only supports C++11 and above when compiling against libc++
                        // (available from FreeBSD 10 onwards). Under FreeBSD, clang uses libc++ by
                        // default on FreeBSD 10 and newer unless `--target` is manually passed to
                        // the compiler, in which case its default behavior differs:
                        // * If --target=xxx-unknown-freebsdX(.Y) is specified and X is greater than
                        //   or equal to 10, clang++ uses libc++
                        // * If --target=xxx-unknown-freebsd is specified (without a version),
                        //   clang++ cannot assume libc++ is available and reverts to a default of
                        //   libstdc++ (this behavior was changed in llvm 14).
                        //
                        // This breaks C++11 (or greater) builds if targeting FreeBSD with the
                        // generic xxx-unknown-freebsd triple on clang 13 or below *without*
                        // explicitly specifying that libc++ should be used.
                        // When cross-compiling, we can't infer from the rust/cargo target triple
                        // which major version of FreeBSD we are targeting, so we need to make sure
                        // that libc++ is used (unless the user has explicitly specified otherwise).
                        // There's no compelling reason to use a different approach when compiling
                        // natively.
                        if self.cpp && self.cpp_set_stdlib.is_none() {
                            cmd.push_cc_arg("-stdlib=libc++".into());
                        }

                        cmd.push_cc_arg(format!("--target={}", target).into());
                    } else {
                        cmd.push_cc_arg(format!("--target={}", target).into());
                    }
                }
            }
            ToolFamily::Msvc { clang_cl } => {
                // This is an undocumented flag from MSVC but helps with making
                // builds more reproducible by avoiding putting timestamps into
                // files.
                cmd.push_cc_arg("-Brepro".into());

                if clang_cl {
                    if target.contains("x86_64") {
                        cmd.push_cc_arg("-m64".into());
                    } else if target.contains("86") {
                        cmd.push_cc_arg("-m32".into());
                        cmd.push_cc_arg("-arch:IA32".into());
                    } else {
                        cmd.push_cc_arg(format!("--target={}", target).into());
                    }
                } else {
                    if target.contains("i586") {
                        cmd.push_cc_arg("-arch:IA32".into());
                    } else if target.contains("arm64ec") {
                        cmd.push_cc_arg("-arm64EC".into());
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
                    cmd.args
                        .push("-D_ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE=1".into());
                }
            }
            ToolFamily::Gnu => {
                if target.contains("-kmc-solid_") {
                    cmd.args.push("-finput-charset=utf-8".into());
                }

                if self.static_flag.is_none() {
                    let features = self.getenv("CARGO_CFG_TARGET_FEATURE");
                    let features = features.as_deref().unwrap_or_default();
                    if features.contains("crt-static") {
                        cmd.args.push("-static".into());
                    }
                }

                // armv7 targets get to use armv7 instructions
                if (target.starts_with("armv7") || target.starts_with("thumbv7"))
                    && (target.contains("-linux-") || target.contains("-kmc-solid_"))
                {
                    cmd.args.push("-march=armv7-a".into());

                    if target.ends_with("eabihf") {
                        // lowest common denominator FPU
                        cmd.args.push("-mfpu=vfpv3-d16".into());
                    }
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

                // Looks like `musl-gcc` makes it hard for `-m32` to make its way
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
                if target.starts_with("armv7a") {
                    cmd.args.push("-march=armv7-a".into());

                    if target.ends_with("eabihf") {
                        // lowest common denominator FPU
                        cmd.args.push("-mfpu=vfpv3-d16".into());
                    }
                }
                if target.starts_with("riscv32") || target.starts_with("riscv64") {
                    // get the 32i/32imac/32imc/64gc/64imac/... part
                    let mut parts = target.split('-');
                    if let Some(arch) = parts.next() {
                        let arch = &arch[5..];
                        if arch.starts_with("64") {
                            if target.contains("linux")
                                | target.contains("freebsd")
                                | target.contains("netbsd")
                                | target.contains("linux")
                            {
                                cmd.args.push(("-march=rv64gc").into());
                                cmd.args.push("-mabi=lp64d".into());
                            } else {
                                cmd.args.push(("-march=rv".to_owned() + arch).into());
                                cmd.args.push("-mabi=lp64".into());
                            }
                        } else if arch.starts_with("32") {
                            if target.contains("linux") {
                                cmd.args.push(("-march=rv32gc").into());
                                cmd.args.push("-mabi=ilp32d".into());
                            } else {
                                cmd.args.push(("-march=rv".to_owned() + arch).into());
                                cmd.args.push("-mabi=ilp32".into());
                            }
                        } else {
                            cmd.args.push("-mcmodel=medany".into());
                        }
                    }
                }
            }
        }
    }

    fn has_flags(&self) -> bool {
        let flags_env_var_name = if self.cpp { "CXXFLAGS" } else { "CFLAGS" };
        let flags_env_var_value = self.getenv_with_target_prefixes(flags_env_var_name);
        if let Ok(_) = flags_env_var_value {
            true
        } else {
            false
        }
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
        cmd.arg("-nologo"); // undocumented, yet working with armasm[64]
        for directory in self.include_directories.iter() {
            cmd.arg("-I").arg(&**directory);
        }
        if target.contains("aarch64") || target.contains("arm") {
            if self.get_debug() {
                cmd.arg("-g");
            }

            for (key, value) in self.definitions.iter() {
                cmd.arg("-PreDefine");
                if let Some(ref value) = *value {
                    if let Ok(i) = value.parse::<i32>() {
                        cmd.arg(&format!("{} SETA {}", key, i));
                    } else if value.starts_with('"') && value.ends_with('"') {
                        cmd.arg(&format!("{} SETS {}", key, value));
                    } else {
                        cmd.arg(&format!("{} SETS \"{}\"", key, value));
                    }
                } else {
                    cmd.arg(&format!("{} SETL {}", key, "{TRUE}"));
                }
            }
        } else {
            if self.get_debug() {
                cmd.arg("-Zi");
            }

            for (key, value) in self.definitions.iter() {
                if let Some(ref value) = *value {
                    cmd.arg(&format!("-D{}={}", key, value));
                } else {
                    cmd.arg(&format!("-D{}", key));
                }
            }
        }

        if target.contains("i686") || target.contains("i586") {
            cmd.arg("-safeseh");
        }

        Ok((cmd, tool.to_string()))
    }

    fn assemble(&self, lib_name: &str, dst: &Path, objs: &[Object]) -> Result<(), Error> {
        // Delete the destination if it exists as we want to
        // create on the first iteration instead of appending.
        let _ = fs::remove_file(dst);

        // Add objects to the archive in limited-length batches. This helps keep
        // the length of the command line within a reasonable length to avoid
        // blowing system limits on limiting platforms like Windows.
        let objs: Vec<_> = objs
            .iter()
            .map(|o| o.dst.as_path())
            .chain(self.objects.iter().map(std::ops::Deref::deref))
            .collect();
        for chunk in objs.chunks(100) {
            self.assemble_progressive(dst, chunk)?;
        }

        if self.cuda && self.cuda_file_count() > 0 {
            // Link the device-side code and add it to the target library,
            // so that non-CUDA linker can link the final binary.

            let out_dir = self.get_out_dir()?;
            let dlink = out_dir.join(lib_name.to_owned() + "_dlink.o");
            let mut nvcc = self.get_compiler().to_command();
            nvcc.arg("--device-link").arg("-o").arg(&dlink).arg(dst);
            run(&mut nvcc, "nvcc", &self.cargo_output)?;
            self.assemble_progressive(dst, &[dlink.as_path()])?;
        }

        let target = self.get_target()?;
        if target.contains("msvc") {
            // The Rust compiler will look for libfoo.a and foo.lib, but the
            // MSVC linker will also be passed foo.lib, so be sure that both
            // exist for now.

            let lib_dst = dst.with_file_name(format!("{}.lib", lib_name));
            let _ = fs::remove_file(&lib_dst);
            match fs::hard_link(dst, &lib_dst).or_else(|_| {
                // if hard-link fails, just copy (ignoring the number of bytes written)
                fs::copy(dst, &lib_dst).map(|_| ())
            }) {
                Ok(_) => (),
                Err(_) => {
                    return Err(Error::new(
                        ErrorKind::IOError,
                        "Could not copy or create a hard-link to the generated lib file.",
                    ));
                }
            };
        } else {
            // Non-msvc targets (those using `ar`) need a separate step to add
            // the symbol table to archives since our construction command of
            // `cq` doesn't add it for us.
            let (mut ar, cmd, _any_flags) = self.get_ar()?;

            // NOTE: We add `s` even if flags were passed using $ARFLAGS/ar_flag, because `s`
            // here represents a _mode_, not an arbitrary flag. Further discussion of this choice
            // can be seen in https://github.com/rust-lang/cc-rs/pull/763.
            run(ar.arg("s").arg(dst), &cmd, &self.cargo_output)?;
        }

        Ok(())
    }

    fn assemble_progressive(&self, dst: &Path, objs: &[&Path]) -> Result<(), Error> {
        let target = self.get_target()?;

        if target.contains("msvc") {
            let (mut cmd, program, any_flags) = self.get_ar()?;
            // NOTE: -out: here is an I/O flag, and so must be included even if $ARFLAGS/ar_flag is
            // in use. -nologo on the other hand is just a regular flag, and one that we'll skip if
            // the caller has explicitly dictated the flags they want. See
            // https://github.com/rust-lang/cc-rs/pull/763 for further discussion.
            let mut out = OsString::from("-out:");
            out.push(dst);
            cmd.arg(out);
            if !any_flags {
                cmd.arg("-nologo");
            }
            // If the library file already exists, add the library name
            // as an argument to let lib.exe know we are appending the objs.
            if dst.exists() {
                cmd.arg(dst);
            }
            cmd.args(objs);
            run(&mut cmd, &program, &self.cargo_output)?;
        } else {
            let (mut ar, cmd, _any_flags) = self.get_ar()?;

            // Set an environment variable to tell the OSX archiver to ensure
            // that all dates listed in the archive are zero, improving
            // determinism of builds. AFAIK there's not really official
            // documentation of this but there's a lot of references to it if
            // you search google.
            //
            // You can reproduce this locally on a mac with:
            //
            //      $ touch foo.c
            //      $ cc -c foo.c -o foo.o
            //
            //      # Notice that these two checksums are different
            //      $ ar crus libfoo1.a foo.o && sleep 2 && ar crus libfoo2.a foo.o
            //      $ md5sum libfoo*.a
            //
            //      # Notice that these two checksums are the same
            //      $ export ZERO_AR_DATE=1
            //      $ ar crus libfoo1.a foo.o && sleep 2 && touch foo.o && ar crus libfoo2.a foo.o
            //      $ md5sum libfoo*.a
            //
            // In any case if this doesn't end up getting read, it shouldn't
            // cause that many issues!
            ar.env("ZERO_AR_DATE", "1");

            // NOTE: We add cq here regardless of whether $ARFLAGS/ar_flag have been used because
            // it dictates the _mode_ ar runs in, which the setter of $ARFLAGS/ar_flag can't
            // dictate. See https://github.com/rust-lang/cc-rs/pull/763 for further discussion.
            run(ar.arg("cq").arg(dst).args(objs), &cmd, &self.cargo_output)?;
        }

        Ok(())
    }

    fn apple_flags(&self, cmd: &mut Tool, target: &str) -> Result<(), Error> {
        let os = if target.contains("-darwin") {
            AppleOs::MacOs
        } else if target.contains("-watchos") {
            AppleOs::WatchOs
        } else if target.contains("-tvos") {
            AppleOs::TvOs
        } else {
            AppleOs::Ios
        };
        let is_mac = match os {
            AppleOs::MacOs => true,
            _ => false,
        };

        let arch_str = target.split('-').nth(0).ok_or_else(|| {
            Error::new(
                ErrorKind::ArchitectureInvalid,
                format!("Unknown architecture for {:?} target.", os),
            )
        })?;

        let is_catalyst = match target.split('-').nth(3) {
            Some(v) => v == "macabi",
            None => false,
        };

        let is_arm_sim = match target.split('-').nth(3) {
            Some(v) => v == "sim",
            None => false,
        };

        let arch = if is_mac {
            match arch_str {
                "i686" => AppleArchSpec::Device("-m32"),
                "x86_64" | "x86_64h" | "aarch64" => AppleArchSpec::Device("-m64"),
                _ => {
                    return Err(Error::new(
                        ErrorKind::ArchitectureInvalid,
                        "Unknown architecture for macOS target.",
                    ));
                }
            }
        } else if is_catalyst {
            match arch_str {
                "arm64e" => AppleArchSpec::Catalyst("arm64e"),
                "arm64" | "aarch64" => AppleArchSpec::Catalyst("arm64"),
                "x86_64" | "x86_64h" => AppleArchSpec::Catalyst("-m64"),
                _ => {
                    return Err(Error::new(
                        ErrorKind::ArchitectureInvalid,
                        "Unknown architecture for iOS target.",
                    ));
                }
            }
        } else if is_arm_sim {
            match arch_str {
                "arm64" | "aarch64" => AppleArchSpec::Simulator("arm64"),
                "x86_64" | "x86_64h" => AppleArchSpec::Simulator("-m64"),
                _ => {
                    return Err(Error::new(
                        ErrorKind::ArchitectureInvalid,
                        "Unknown architecture for simulator target.",
                    ));
                }
            }
        } else {
            match arch_str {
                "arm" | "armv7" | "thumbv7" => AppleArchSpec::Device("armv7"),
                "armv7k" => AppleArchSpec::Device("armv7k"),
                "armv7s" | "thumbv7s" => AppleArchSpec::Device("armv7s"),
                "arm64e" => AppleArchSpec::Device("arm64e"),
                "arm64" | "aarch64" => AppleArchSpec::Device("arm64"),
                "arm64_32" => AppleArchSpec::Device("arm64_32"),
                "i386" | "i686" => AppleArchSpec::Simulator("-m32"),
                "x86_64" | "x86_64h" => AppleArchSpec::Simulator("-m64"),
                _ => {
                    return Err(Error::new(
                        ErrorKind::ArchitectureInvalid,
                        format!("Unknown architecture for {:?} target.", os),
                    ));
                }
            }
        };

        let sdk_details = apple_os_sdk_parts(os, &arch);
        let min_version = self.apple_deployment_version(os, Some(arch_str), &sdk_details.sdk);

        match arch {
            AppleArchSpec::Device(_) if is_mac => {
                cmd.args
                    .push(format!("-mmacosx-version-min={}", min_version).into());
            }
            AppleArchSpec::Device(arch) => {
                cmd.args.push("-arch".into());
                cmd.args.push(arch.into());
                cmd.args.push(
                    format!("-m{}os-version-min={}", sdk_details.sdk_prefix, min_version).into(),
                );
            }
            AppleArchSpec::Simulator(arch) => {
                if arch.starts_with('-') {
                    // -m32 or -m64
                    cmd.args.push(arch.into());
                } else {
                    cmd.args.push("-arch".into());
                    cmd.args.push(arch.into());
                }
                cmd.args.push(
                    format!(
                        "-m{}simulator-version-min={}",
                        sdk_details.sim_prefix, min_version
                    )
                    .into(),
                );
            }
            AppleArchSpec::Catalyst(_) => {}
        };

        // AppleClang sometimes requires sysroot even for darwin
        if cmd.is_xctoolchain_clang() || !target.ends_with("-darwin") {
            self.cargo_output.print_metadata(&format_args!(
                "Detecting {:?} SDK path for {}",
                os, sdk_details.sdk
            ));
            let sdk_path = self.apple_sdk_root(&sdk_details.sdk)?;

            cmd.args.push("-isysroot".into());
            cmd.args.push(sdk_path);
        }

        match cmd.family {
            ToolFamily::Gnu => {
                if target.contains("darwin") {
                    if let Some(arch) = map_darwin_target_from_rust_to_compiler_architecture(target)
                    {
                        cmd.args.push("-arch".into());
                        cmd.args.push(arch.into());
                    }
                }
            }
            ToolFamily::Clang => {
                if target.contains("darwin") {
                    if let Some(arch) = map_darwin_target_from_rust_to_compiler_architecture(target)
                    {
                        cmd.args
                            .push(format!("--target={}-apple-darwin", arch).into());
                    }
                } else if target.contains("macabi") {
                    if let Some(arch) = map_darwin_target_from_rust_to_compiler_architecture(target)
                    {
                        cmd.args
                            .push(format!("--target={}-apple-ios-macabi", arch).into());
                    }
                } else if target.contains("ios-sim") {
                    if let Some(arch) = map_darwin_target_from_rust_to_compiler_architecture(target)
                    {
                        cmd.args.push(
                            format!("--target={}-apple-ios{}-simulator", arch, min_version).into(),
                        );
                    }
                } else if target.contains("watchos-sim") {
                    if let Some(arch) = map_darwin_target_from_rust_to_compiler_architecture(target)
                    {
                        cmd.args.push(
                            format!("--target={}-apple-watchos{}-simulator", arch, min_version)
                                .into(),
                        );
                    }
                } else if target.contains("tvos-sim") || target.contains("x86_64-apple-tvos") {
                    if let Some(arch) = map_darwin_target_from_rust_to_compiler_architecture(target)
                    {
                        cmd.args.push(
                            format!("--target={}-apple-tvos{}-simulator", arch, min_version).into(),
                        );
                    }
                } else if target.contains("aarch64-apple-tvos") {
                    if let Some(arch) = map_darwin_target_from_rust_to_compiler_architecture(target)
                    {
                        cmd.args
                            .push(format!("--target={}-apple-tvos{}", arch, min_version).into());
                    }
                }
            }
            _ => unreachable!("unexpected compiler for apple architectures"),
        }

        if let AppleArchSpec::Catalyst(_) = arch {
            // Mac Catalyst uses the macOS SDK, but to compile against and
            // link to iOS-specific frameworks, we should have the support
            // library stubs in the include and library search path.
            let sdk_path = self.apple_sdk_root(&sdk_details.sdk)?;
            let ios_support = PathBuf::from(sdk_path).join("/System/iOSSupport");

            cmd.args.extend([
                // Header search path
                OsString::from("-isystem"),
                ios_support.join("/usr/include").into(),
                // Framework header search path
                OsString::from("-iframework"),
                ios_support.join("/System/Library/Frameworks").into(),
                // Library search path
                {
                    let mut s = OsString::from("-L");
                    s.push(&ios_support.join("/usr/lib"));
                    s
                },
                // Framework linker search path
                {
                    // Technically, we _could_ avoid emitting `-F`, as
                    // `-iframework` implies it, but let's keep it in for
                    // clarity.
                    let mut s = OsString::from("-F");
                    s.push(&ios_support.join("/System/Library/Frameworks"));
                    s
                },
            ]);
        }

        Ok(())
    }

    fn cmd<P: AsRef<OsStr>>(&self, prog: P) -> Command {
        let mut cmd = Command::new(prog);
        for (a, b) in self.env.iter() {
            cmd.env(a, b);
        }
        cmd
    }

    fn get_base_compiler(&self) -> Result<Tool, Error> {
        if let Some(c) = &self.compiler {
            return Ok(Tool::new(
                (**c).to_owned(),
                &self.cached_compiler_family,
                &self.cargo_output,
            ));
        }
        let host = self.get_host()?;
        let target = self.get_target()?;
        let target = &*target;
        let (env, msvc, gnu, traditional, clang) = if self.cpp {
            ("CXX", "cl.exe", "g++", "c++", "clang++")
        } else {
            ("CC", "cl.exe", "gcc", "cc", "clang")
        };

        // On historical Solaris systems, "cc" may have been Sun Studio, which
        // is not flag-compatible with "gcc".  This history casts a long shadow,
        // and many modern illumos distributions today ship GCC as "gcc" without
        // also making it available as "cc".
        let default = if host.contains("solaris") || host.contains("illumos") {
            gnu
        } else {
            traditional
        };

        let cl_exe = windows_registry::find_tool(target, "cl.exe");

        let tool_opt: Option<Tool> = self
            .env_tool(env)
            .map(|(tool, wrapper, args)| {
                // find the driver mode, if any
                const DRIVER_MODE: &str = "--driver-mode=";
                let driver_mode = args
                    .iter()
                    .find(|a| a.starts_with(DRIVER_MODE))
                    .map(|a| &a[DRIVER_MODE.len()..]);
                // Chop off leading/trailing whitespace to work around
                // semi-buggy build scripts which are shared in
                // makefiles/configure scripts (where spaces are far more
                // lenient)
                let mut t = Tool::with_clang_driver(
                    tool,
                    driver_mode,
                    &self.cached_compiler_family,
                    &self.cargo_output,
                );
                if let Some(cc_wrapper) = wrapper {
                    t.cc_wrapper_path = Some(PathBuf::from(cc_wrapper));
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
                        let mut t = Tool::new(
                            PathBuf::from("cmd"),
                            &self.cached_compiler_family,
                            &self.cargo_output,
                        );
                        t.args.push("/c".into());
                        t.args.push(format!("{}.bat", tool).into());
                        Some(t)
                    } else {
                        Some(Tool::new(
                            PathBuf::from(tool),
                            &self.cached_compiler_family,
                            &self.cargo_output,
                        ))
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
                        let cc = if target.contains("llvm") { clang } else { gnu };
                        format!("{}.exe", cc)
                    }
                } else if target.contains("apple-ios")
                    | target.contains("apple-watchos")
                    | target.contains("apple-tvos")
                {
                    clang.to_string()
                } else if target.contains("android") {
                    autodetect_android_compiler(target, &host, gnu, clang)
                } else if target.contains("cloudabi") {
                    format!("{}-{}", target, traditional)
                } else if target == "wasm32-wasi"
                    || target == "wasm32-unknown-wasi"
                    || target == "wasm32-unknown-unknown"
                {
                    "clang".to_string()
                } else if target.contains("vxworks") {
                    if self.cpp {
                        "wr-c++".to_string()
                    } else {
                        "wr-cc".to_string()
                    }
                } else if target.starts_with("armv7a-kmc-solid_") {
                    format!("arm-kmc-eabi-{}", gnu)
                } else if target.starts_with("aarch64-kmc-solid_") {
                    format!("aarch64-kmc-elf-{}", gnu)
                } else if &*self.get_host()? != target {
                    let prefix = self.prefix_for_target(target);
                    match prefix {
                        Some(prefix) => {
                            let cc = if target.contains("llvm") { clang } else { gnu };
                            format!("{}-{}", prefix, cc)
                        }
                        None => default.to_string(),
                    }
                } else {
                    default.to_string()
                };

                let mut t = Tool::new(
                    PathBuf::from(compiler),
                    &self.cached_compiler_family,
                    &self.cargo_output,
                );
                if let Some(cc_wrapper) = Self::rustc_wrapper_fallback() {
                    t.cc_wrapper_path = Some(PathBuf::from(cc_wrapper));
                }
                t
            }
        };

        let mut tool = if self.cuda {
            assert!(
                tool.args.is_empty(),
                "CUDA compilation currently assumes empty pre-existing args"
            );
            let nvcc = match self.getenv_with_target_prefixes("NVCC") {
                Err(_) => PathBuf::from("nvcc"),
                Ok(nvcc) => PathBuf::from(&*nvcc),
            };
            let mut nvcc_tool = Tool::with_features(
                nvcc,
                None,
                self.cuda,
                &self.cached_compiler_family,
                &self.cargo_output,
            );
            nvcc_tool
                .args
                .push(format!("-ccbin={}", tool.path.display()).into());
            nvcc_tool.family = tool.family;
            nvcc_tool
        } else {
            tool
        };

        // New "standalone" C/C++ cross-compiler executables from recent Android NDK
        // are just shell scripts that call main clang binary (from Android NDK) with
        // proper `--target` argument.
        //
        // For example, armv7a-linux-androideabi16-clang passes
        // `--target=armv7a-linux-androideabi16` to clang.
        //
        // As the shell script calls the main clang binary, the command line limit length
        // on Windows is restricted to around 8k characters instead of around 32k characters.
        // To remove this limit, we call the main clang binary directly and construct the
        // `--target=` ourselves.
        if host.contains("windows") && android_clang_compiler_uses_target_arg_internally(&tool.path)
        {
            if let Some(path) = tool.path.file_name() {
                let file_name = path.to_str().unwrap().to_owned();
                let (target, clang) = file_name.split_at(file_name.rfind('-').unwrap());

                tool.has_internal_target_arg = true;
                tool.path.set_file_name(clang.trim_start_matches('-'));
                tool.path.set_extension("exe");
                tool.args.push(format!("--target={}", target).into());

                // Additionally, shell scripts for target i686-linux-android versions 16 to 24
                // pass the `mstackrealign` option so we do that here as well.
                if target.contains("i686-linux-android") {
                    let (_, version) = target.split_at(target.rfind('d').unwrap() + 1);
                    if let Ok(version) = version.parse::<u32>() {
                        if version > 15 && version < 25 {
                            tool.args.push("-mstackrealign".into());
                        }
                    }
                }
            };
        }

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
            if tool.family == (ToolFamily::Msvc { clang_cl: true })
                && tool.env.len() == 0
                && target.contains("msvc")
            {
                for (k, v) in cl_exe.env.iter() {
                    tool.env.push((k.to_owned(), v.to_owned()));
                }
            }
        }

        if target.contains("msvc") && tool.family == ToolFamily::Gnu {
            self.cargo_output
                .print_warning(&"GNU compiler is not supported for this target");
        }

        Ok(tool)
    }

    /// Returns a fallback `cc_compiler_wrapper` by introspecting `RUSTC_WRAPPER`
    fn rustc_wrapper_fallback() -> Option<String> {
        // No explicit CC wrapper was detected, but check if RUSTC_WRAPPER
        // is defined and is a build accelerator that is compatible with
        // C/C++ compilers (e.g. sccache)
        const VALID_WRAPPERS: &[&'static str] = &["sccache", "cachepot"];

        let rustc_wrapper = std::env::var_os("RUSTC_WRAPPER")?;
        let wrapper_path = Path::new(&rustc_wrapper);
        let wrapper_stem = wrapper_path.file_stem()?;

        if VALID_WRAPPERS.contains(&wrapper_stem.to_str()?) {
            Some(rustc_wrapper.to_str()?.to_owned())
        } else {
            None
        }
    }

    /// Returns compiler path, optional modifier name from whitelist, and arguments vec
    fn env_tool(&self, name: &str) -> Option<(PathBuf, Option<String>, Vec<String>)> {
        let tool = match self.getenv_with_target_prefixes(name) {
            Ok(tool) => tool,
            Err(_) => return None,
        };

        // If this is an exact path on the filesystem we don't want to do any
        // interpretation at all, just pass it on through. This'll hopefully get
        // us to support spaces-in-paths.
        if Path::new(&*tool).exists() {
            return Some((
                PathBuf::from(&*tool),
                Self::rustc_wrapper_fallback(),
                Vec::new(),
            ));
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
        let known_wrappers = ["ccache", "distcc", "sccache", "icecc", "cachepot"];

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
                    compiler.into(),
                    Some(maybe_wrapper.to_string()),
                    parts.map(|s| s.to_string()).collect(),
                ));
            }
        }

        Some((
            maybe_wrapper.into(),
            Self::rustc_wrapper_fallback(),
            parts.map(|s| s.to_string()).collect(),
        ))
    }

    /// Returns the C++ standard library:
    /// 1. If [`cpp_link_stdlib`](cc::Build::cpp_link_stdlib) is set, uses its value.
    /// 2. Else if the `CXXSTDLIB` environment variable is set, uses its value.
    /// 3. Else the default is `libc++` for OS X and BSDs, `libc++_shared` for Android,
    /// `None` for MSVC and `libstdc++` for anything else.
    fn get_cpp_link_stdlib(&self) -> Result<Option<String>, Error> {
        match &self.cpp_link_stdlib {
            Some(s) => Ok(s.as_ref().map(|s| (*s).to_string())),
            None => {
                if let Ok(stdlib) = self.getenv_with_target_prefixes("CXXSTDLIB") {
                    if stdlib.is_empty() {
                        Ok(None)
                    } else {
                        Ok(Some(stdlib.to_string()))
                    }
                } else {
                    let target = self.get_target()?;
                    if target.contains("msvc") {
                        Ok(None)
                    } else if target.contains("apple")
                        | target.contains("freebsd")
                        | target.contains("openbsd")
                        | target.contains("aix")
                        | target.contains("linux-ohos")
                    {
                        Ok(Some("c++".to_string()))
                    } else if target.contains("android") {
                        Ok(Some("c++_shared".to_string()))
                    } else {
                        Ok(Some("stdc++".to_string()))
                    }
                }
            }
        }
    }

    fn get_ar(&self) -> Result<(Command, String, bool), Error> {
        self.try_get_archiver_and_flags()
    }

    /// Get the archiver (ar) that's in use for this configuration.
    ///
    /// You can use [`Command::get_program`] to get just the path to the command.
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
    pub fn get_archiver(&self) -> Command {
        match self.try_get_archiver() {
            Ok(tool) => tool,
            Err(e) => fail(&e.message),
        }
    }

    /// Get the archiver that's in use for this configuration.
    ///
    /// This will return a result instead of panicking;
    /// see [`Self::get_archiver`] for the complete description.
    pub fn try_get_archiver(&self) -> Result<Command, Error> {
        Ok(self.try_get_archiver_and_flags()?.0)
    }

    fn try_get_archiver_and_flags(&self) -> Result<(Command, String, bool), Error> {
        let (mut cmd, name) = self.get_base_archiver()?;
        let mut any_flags = false;
        if let Ok(flags) = self.envflags("ARFLAGS") {
            any_flags = any_flags | !flags.is_empty();
            cmd.args(flags);
        }
        for flag in &self.ar_flags {
            any_flags = true;
            cmd.arg(&**flag);
        }
        Ok((cmd, name, any_flags))
    }

    fn get_base_archiver(&self) -> Result<(Command, String), Error> {
        if let Some(ref a) = self.archiver {
            return Ok((self.cmd(&**a), a.to_string_lossy().into_owned()));
        }

        self.get_base_archiver_variant("AR", "ar")
    }

    /// Get the ranlib that's in use for this configuration.
    ///
    /// You can use [`Command::get_program`] to get just the path to the command.
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
    pub fn get_ranlib(&self) -> Command {
        match self.try_get_ranlib() {
            Ok(tool) => tool,
            Err(e) => fail(&e.message),
        }
    }

    /// Get the ranlib that's in use for this configuration.
    ///
    /// This will return a result instead of panicking;
    /// see [`Self::get_ranlib`] for the complete description.
    pub fn try_get_ranlib(&self) -> Result<Command, Error> {
        let mut cmd = self.get_base_ranlib()?;
        if let Ok(flags) = self.envflags("RANLIBFLAGS") {
            cmd.args(flags);
        }
        Ok(cmd)
    }

    fn get_base_ranlib(&self) -> Result<Command, Error> {
        if let Some(ref r) = self.ranlib {
            return Ok(self.cmd(&**r));
        }

        Ok(self.get_base_archiver_variant("RANLIB", "ranlib")?.0)
    }

    fn get_base_archiver_variant(&self, env: &str, tool: &str) -> Result<(Command, String), Error> {
        let target = self.get_target()?;
        let mut name = String::new();
        let tool_opt: Option<Command> = self
            .env_tool(env)
            .map(|(tool, _wrapper, args)| {
                let mut cmd = self.cmd(tool);
                cmd.args(args);
                cmd
            })
            .or_else(|| {
                if target.contains("emscripten") {
                    // Windows use bat files so we have to be a bit more specific
                    if cfg!(windows) {
                        let mut cmd = self.cmd("cmd");
                        name = format!("em{}.bat", tool);
                        cmd.arg("/c").arg(&name);
                        Some(cmd)
                    } else {
                        name = format!("em{}", tool);
                        Some(self.cmd(&name))
                    }
                } else if target.starts_with("wasm32") {
                    // Formally speaking one should be able to use this approach,
                    // parsing -print-search-dirs output, to cover all clang targets,
                    // including Android SDKs and other cross-compilation scenarios...
                    // And even extend it to gcc targets by searching for "ar" instead
                    // of "llvm-ar"...
                    let compiler = self.get_base_compiler().ok()?;
                    if compiler.family == ToolFamily::Clang {
                        name = format!("llvm-{}", tool);
                        search_programs(&mut self.cmd(&compiler.path), &name, &self.cargo_output)
                            .map(|name| self.cmd(name))
                    } else {
                        None
                    }
                } else {
                    None
                }
            });

        let default = tool.to_string();
        let tool = match tool_opt {
            Some(t) => t,
            None => {
                if target.contains("android") {
                    name = format!("llvm-{}", tool);
                    match Command::new(&name).arg("--version").status() {
                        Ok(status) if status.success() => (),
                        _ => name = format!("{}-{}", target.replace("armv7", "arm"), tool),
                    }
                    self.cmd(&name)
                } else if target.contains("msvc") {
                    // NOTE: There isn't really a ranlib on msvc, so arguably we should return
                    // `None` somehow here. But in general, callers will already have to be aware
                    // of not running ranlib on Windows anyway, so it feels okay to return lib.exe
                    // here.

                    let compiler = self.get_base_compiler()?;
                    let mut lib = String::new();
                    if compiler.family == (ToolFamily::Msvc { clang_cl: true }) {
                        // See if there is 'llvm-lib' next to 'clang-cl'
                        // Another possibility could be to see if there is 'clang'
                        // next to 'clang-cl' and use 'search_programs()' to locate
                        // 'llvm-lib'. This is because 'clang-cl' doesn't support
                        // the -print-search-dirs option.
                        if let Some(mut cmd) = which(&compiler.path, None) {
                            cmd.pop();
                            cmd.push("llvm-lib.exe");
                            if let Some(llvm_lib) = which(&cmd, None) {
                                lib = llvm_lib.to_str().unwrap().to_owned();
                            }
                        }
                    }

                    if lib.is_empty() {
                        name = String::from("lib.exe");
                        let mut cmd = match windows_registry::find(&target, "lib.exe") {
                            Some(t) => t,
                            None => self.cmd("lib.exe"),
                        };
                        if target.contains("arm64ec") {
                            cmd.arg("/machine:arm64ec");
                        }
                        cmd
                    } else {
                        name = lib;
                        self.cmd(&name)
                    }
                } else if target.contains("illumos") {
                    // The default 'ar' on illumos uses a non-standard flags,
                    // but the OS comes bundled with a GNU-compatible variant.
                    //
                    // Use the GNU-variant to match other Unix systems.
                    name = format!("g{}", tool);
                    self.cmd(&name)
                } else if self.get_host()? != target {
                    match self.prefix_for_target(&target) {
                        Some(p) => {
                            // GCC uses $target-gcc-ar, whereas binutils uses $target-ar -- try both.
                            // Prefer -ar if it exists, as builds of `-gcc-ar` have been observed to be
                            // outright broken (such as when targeting freebsd with `--disable-lto`
                            // toolchain where the archiver attempts to load the LTO plugin anyway but
                            // fails to find one).
                            //
                            // The same applies to ranlib.
                            let mut chosen = default;
                            for &infix in &["", "-gcc"] {
                                let target_p = format!("{}{}-{}", p, infix, tool);
                                if Command::new(&target_p).output().is_ok() {
                                    chosen = target_p;
                                    break;
                                }
                            }
                            name = chosen;
                            self.cmd(&name)
                        }
                        None => {
                            name = default;
                            self.cmd(&name)
                        }
                    }
                } else {
                    name = default;
                    self.cmd(&name)
                }
            }
        };

        Ok((tool, name))
    }

    fn prefix_for_target(&self, target: &str) -> Option<String> {
        // Put aside RUSTC_LINKER's prefix to be used as second choice, after CROSS_COMPILE
        let linker_prefix = self
            .getenv("RUSTC_LINKER")
            .and_then(|var| var.strip_suffix("-gcc").map(str::to_string));
        // CROSS_COMPILE is of the form: "arm-linux-gnueabi-"
        let cc_env = self.getenv("CROSS_COMPILE");
        let cross_compile = cc_env.as_ref().map(|s| s.trim_end_matches('-').to_owned());
        cross_compile.or(linker_prefix).or(match &target[..] {
            // Note: there is no `aarch64-pc-windows-gnu` target, only `-gnullvm`
            "aarch64-pc-windows-gnullvm" => Some("aarch64-w64-mingw32"),
            "aarch64-uwp-windows-gnu" => Some("aarch64-w64-mingw32"),
            "aarch64-unknown-linux-gnu" => Some("aarch64-linux-gnu"),
            "aarch64-unknown-linux-musl" => Some("aarch64-linux-musl"),
            "aarch64-unknown-netbsd" => Some("aarch64--netbsd"),
            "arm-unknown-linux-gnueabi" => Some("arm-linux-gnueabi"),
            "armv4t-unknown-linux-gnueabi" => Some("arm-linux-gnueabi"),
            "armv5te-unknown-linux-gnueabi" => Some("arm-linux-gnueabi"),
            "armv5te-unknown-linux-musleabi" => Some("arm-linux-gnueabi"),
            "arm-frc-linux-gnueabi" => Some("arm-frc-linux-gnueabi"),
            "arm-unknown-linux-gnueabihf" => Some("arm-linux-gnueabihf"),
            "arm-unknown-linux-musleabi" => Some("arm-linux-musleabi"),
            "arm-unknown-linux-musleabihf" => Some("arm-linux-musleabihf"),
            "arm-unknown-netbsd-eabi" => Some("arm--netbsdelf-eabi"),
            "armv6-unknown-netbsd-eabihf" => Some("armv6--netbsdelf-eabihf"),
            "armv7-unknown-linux-gnueabi" => Some("arm-linux-gnueabi"),
            "armv7-unknown-linux-gnueabihf" => Some("arm-linux-gnueabihf"),
            "armv7-unknown-linux-musleabihf" => Some("arm-linux-musleabihf"),
            "armv7neon-unknown-linux-gnueabihf" => Some("arm-linux-gnueabihf"),
            "armv7neon-unknown-linux-musleabihf" => Some("arm-linux-musleabihf"),
            "thumbv7-unknown-linux-gnueabihf" => Some("arm-linux-gnueabihf"),
            "thumbv7-unknown-linux-musleabihf" => Some("arm-linux-musleabihf"),
            "thumbv7neon-unknown-linux-gnueabihf" => Some("arm-linux-gnueabihf"),
            "thumbv7neon-unknown-linux-musleabihf" => Some("arm-linux-musleabihf"),
            "armv7-unknown-netbsd-eabihf" => Some("armv7--netbsdelf-eabihf"),
            "hexagon-unknown-linux-musl" => Some("hexagon-linux-musl"),
            "i586-unknown-linux-musl" => Some("musl"),
            "i686-pc-windows-gnu" => Some("i686-w64-mingw32"),
            "i686-uwp-windows-gnu" => Some("i686-w64-mingw32"),
            "i686-unknown-linux-gnu" => self.find_working_gnu_prefix(&[
                "i686-linux-gnu",
                "x86_64-linux-gnu", // transparently support gcc-multilib
            ]), // explicit None if not found, so caller knows to fall back
            "i686-unknown-linux-musl" => Some("musl"),
            "i686-unknown-netbsd" => Some("i486--netbsdelf"),
            "loongarch64-unknown-linux-gnu" => Some("loongarch64-linux-gnu"),
            "mips-unknown-linux-gnu" => Some("mips-linux-gnu"),
            "mips-unknown-linux-musl" => Some("mips-linux-musl"),
            "mipsel-unknown-linux-gnu" => Some("mipsel-linux-gnu"),
            "mipsel-unknown-linux-musl" => Some("mipsel-linux-musl"),
            "mips64-unknown-linux-gnuabi64" => Some("mips64-linux-gnuabi64"),
            "mips64el-unknown-linux-gnuabi64" => Some("mips64el-linux-gnuabi64"),
            "mipsisa32r6-unknown-linux-gnu" => Some("mipsisa32r6-linux-gnu"),
            "mipsisa32r6el-unknown-linux-gnu" => Some("mipsisa32r6el-linux-gnu"),
            "mipsisa64r6-unknown-linux-gnuabi64" => Some("mipsisa64r6-linux-gnuabi64"),
            "mipsisa64r6el-unknown-linux-gnuabi64" => Some("mipsisa64r6el-linux-gnuabi64"),
            "powerpc-unknown-linux-gnu" => Some("powerpc-linux-gnu"),
            "powerpc-unknown-linux-gnuspe" => Some("powerpc-linux-gnuspe"),
            "powerpc-unknown-netbsd" => Some("powerpc--netbsd"),
            "powerpc64-unknown-linux-gnu" => Some("powerpc-linux-gnu"),
            "powerpc64le-unknown-linux-gnu" => Some("powerpc64le-linux-gnu"),
            "riscv32i-unknown-none-elf" => self.find_working_gnu_prefix(&[
                "riscv32-unknown-elf",
                "riscv64-unknown-elf",
                "riscv-none-embed",
            ]),
            "riscv32imac-esp-espidf" => Some("riscv32-esp-elf"),
            "riscv32imac-unknown-none-elf" => self.find_working_gnu_prefix(&[
                "riscv32-unknown-elf",
                "riscv64-unknown-elf",
                "riscv-none-embed",
            ]),
            "riscv32imac-unknown-xous-elf" => self.find_working_gnu_prefix(&[
                "riscv32-unknown-elf",
                "riscv64-unknown-elf",
                "riscv-none-embed",
            ]),
            "riscv32imc-esp-espidf" => Some("riscv32-esp-elf"),
            "riscv32imc-unknown-none-elf" => self.find_working_gnu_prefix(&[
                "riscv32-unknown-elf",
                "riscv64-unknown-elf",
                "riscv-none-embed",
            ]),
            "riscv64gc-unknown-none-elf" => self.find_working_gnu_prefix(&[
                "riscv64-unknown-elf",
                "riscv32-unknown-elf",
                "riscv-none-embed",
            ]),
            "riscv64imac-unknown-none-elf" => self.find_working_gnu_prefix(&[
                "riscv64-unknown-elf",
                "riscv32-unknown-elf",
                "riscv-none-embed",
            ]),
            "riscv64gc-unknown-linux-gnu" => Some("riscv64-linux-gnu"),
            "riscv32gc-unknown-linux-gnu" => Some("riscv32-linux-gnu"),
            "riscv64gc-unknown-linux-musl" => Some("riscv64-linux-musl"),
            "riscv32gc-unknown-linux-musl" => Some("riscv32-linux-musl"),
            "riscv64gc-unknown-netbsd" => Some("riscv64--netbsd"),
            "s390x-unknown-linux-gnu" => Some("s390x-linux-gnu"),
            "sparc-unknown-linux-gnu" => Some("sparc-linux-gnu"),
            "sparc64-unknown-linux-gnu" => Some("sparc64-linux-gnu"),
            "sparc64-unknown-netbsd" => Some("sparc64--netbsd"),
            "sparcv9-sun-solaris" => Some("sparcv9-sun-solaris"),
            "armv7a-none-eabi" => Some("arm-none-eabi"),
            "armv7a-none-eabihf" => Some("arm-none-eabi"),
            "armebv7r-none-eabi" => Some("arm-none-eabi"),
            "armebv7r-none-eabihf" => Some("arm-none-eabi"),
            "armv7r-none-eabi" => Some("arm-none-eabi"),
            "armv7r-none-eabihf" => Some("arm-none-eabi"),
            "armv8r-none-eabihf" => Some("arm-none-eabi"),
            "thumbv6m-none-eabi" => Some("arm-none-eabi"),
            "thumbv7em-none-eabi" => Some("arm-none-eabi"),
            "thumbv7em-none-eabihf" => Some("arm-none-eabi"),
            "thumbv7m-none-eabi" => Some("arm-none-eabi"),
            "thumbv8m.base-none-eabi" => Some("arm-none-eabi"),
            "thumbv8m.main-none-eabi" => Some("arm-none-eabi"),
            "thumbv8m.main-none-eabihf" => Some("arm-none-eabi"),
            "x86_64-pc-windows-gnu" => Some("x86_64-w64-mingw32"),
            "x86_64-pc-windows-gnullvm" => Some("x86_64-w64-mingw32"),
            "x86_64-uwp-windows-gnu" => Some("x86_64-w64-mingw32"),
            "x86_64-rumprun-netbsd" => Some("x86_64-rumprun-netbsd"),
            "x86_64-unknown-linux-gnu" => self.find_working_gnu_prefix(&[
                "x86_64-linux-gnu", // rustfmt wrap
            ]), // explicit None if not found, so caller knows to fall back
            "x86_64-unknown-linux-musl" => Some("musl"),
            "x86_64-unknown-netbsd" => Some("x86_64--netbsd"),
            _ => None,
        }
        .map(|x| x.to_owned()))
    }

    /// Some platforms have multiple, compatible, canonical prefixes. Look through
    /// each possible prefix for a compiler that exists and return it. The prefixes
    /// should be ordered from most-likely to least-likely.
    fn find_working_gnu_prefix(&self, prefixes: &[&'static str]) -> Option<&'static str> {
        let suffix = if self.cpp { "-g++" } else { "-gcc" };
        let extension = std::env::consts::EXE_SUFFIX;

        // Loop through PATH entries searching for each toolchain. This ensures that we
        // are more likely to discover the toolchain early on, because chances are good
        // that the desired toolchain is in one of the higher-priority paths.
        env::var_os("PATH")
            .as_ref()
            .and_then(|path_entries| {
                env::split_paths(path_entries).find_map(|path_entry| {
                    for prefix in prefixes {
                        let target_compiler = format!("{}{}{}", prefix, suffix, extension);
                        if path_entry.join(&target_compiler).exists() {
                            return Some(prefix);
                        }
                    }
                    None
                })
            })
            .map(|prefix| *prefix)
            .or_else(||
            // If no toolchain was found, provide the first toolchain that was passed in.
            // This toolchain has been shown not to exist, however it will appear in the
            // error that is shown to the user which should make it easier to search for
            // where it should be obtained.
            prefixes.first().map(|prefix| *prefix))
    }

    fn get_target(&self) -> Result<Arc<str>, Error> {
        match &self.target {
            Some(t) => Ok(t.clone()),
            None => self.getenv_unwrap("TARGET"),
        }
    }

    fn get_host(&self) -> Result<Arc<str>, Error> {
        match &self.host {
            Some(h) => Ok(h.clone()),
            None => self.getenv_unwrap("HOST"),
        }
    }

    fn get_opt_level(&self) -> Result<Arc<str>, Error> {
        match &self.opt_level {
            Some(ol) => Ok(ol.clone()),
            None => self.getenv_unwrap("OPT_LEVEL"),
        }
    }

    fn get_debug(&self) -> bool {
        self.debug.unwrap_or_else(|| match self.getenv("DEBUG") {
            Some(s) => &*s != "false",
            None => false,
        })
    }

    fn get_dwarf_version(&self) -> Option<u32> {
        // Tentatively matches the DWARF version defaults as of rustc 1.62.
        let target = self.get_target().ok()?;
        if target.contains("android")
            || target.contains("apple")
            || target.contains("dragonfly")
            || target.contains("freebsd")
            || target.contains("netbsd")
            || target.contains("openbsd")
            || target.contains("windows-gnu")
        {
            Some(2)
        } else if target.contains("linux") {
            Some(4)
        } else {
            None
        }
    }

    fn get_force_frame_pointer(&self) -> bool {
        self.force_frame_pointer.unwrap_or_else(|| self.get_debug())
    }

    fn get_out_dir(&self) -> Result<Cow<'_, Path>, Error> {
        match &self.out_dir {
            Some(p) => Ok(Cow::Borrowed(&**p)),
            None => env::var_os("OUT_DIR")
                .map(PathBuf::from)
                .map(Cow::Owned)
                .ok_or_else(|| {
                    Error::new(
                        ErrorKind::EnvVarNotFound,
                        "Environment variable OUT_DIR not defined.",
                    )
                }),
        }
    }

    fn getenv(&self, v: &str) -> Option<Arc<str>> {
        // Returns true for environment variables cargo sets for build scripts:
        // https://doc.rust-lang.org/cargo/reference/environment-variables.html#environment-variables-cargo-sets-for-build-scripts
        //
        // This handles more of the vars than we actually use (it tries to check
        // complete-ish set), just to avoid needing maintenance if/when new
        // calls to `getenv`/`getenv_unwrap` are added.
        fn provided_by_cargo(envvar: &str) -> bool {
            match envvar {
                v if v.starts_with("CARGO") || v.starts_with("RUSTC") => true,
                "HOST" | "TARGET" | "RUSTDOC" | "OUT_DIR" | "OPT_LEVEL" | "DEBUG" | "PROFILE"
                | "NUM_JOBS" | "RUSTFLAGS" => true,
                _ => false,
            }
        }
        let mut cache = self.env_cache.lock().unwrap();
        if let Some(val) = cache.get(v) {
            return val.clone();
        }
        if self.emit_rerun_if_env_changed && !provided_by_cargo(v) {
            self.cargo_output
                .print_metadata(&format_args!("cargo:rerun-if-env-changed={}", v));
        }
        let r = env::var(v).ok().map(Arc::from);
        self.cargo_output
            .print_metadata(&format_args!("{} = {:?}", v, r));
        cache.insert(v.to_string(), r.clone());
        r
    }

    fn getenv_unwrap(&self, v: &str) -> Result<Arc<str>, Error> {
        match self.getenv(v) {
            Some(s) => Ok(s),
            None => Err(Error::new(
                ErrorKind::EnvVarNotFound,
                format!("Environment variable {} not defined.", v),
            )),
        }
    }

    fn getenv_with_target_prefixes(&self, var_base: &str) -> Result<Arc<str>, Error> {
        let target = self.get_target()?;
        let host = self.get_host()?;
        let kind = if host == target { "HOST" } else { "TARGET" };
        let target_u = target.replace('-', "_");
        let res = self
            .getenv(&format!("{}_{}", var_base, target))
            .or_else(|| self.getenv(&format!("{}_{}", var_base, target_u)))
            .or_else(|| self.getenv(&format!("{}_{}", kind, var_base)))
            .or_else(|| self.getenv(var_base));

        match res {
            Some(res) => Ok(res),
            None => Err(Error::new(
                ErrorKind::EnvVarNotFound,
                format!("Could not find environment variable {}.", var_base),
            )),
        }
    }

    fn envflags(&self, name: &str) -> Result<Vec<String>, Error> {
        Ok(self
            .getenv_with_target_prefixes(name)?
            .split_ascii_whitespace()
            .map(|slice| slice.to_string())
            .collect())
    }

    fn fix_env_for_apple_os(&self, cmd: &mut Command) -> Result<(), Error> {
        let target = self.get_target()?;
        let host = self.get_host()?;
        if host.contains("apple-darwin") && target.contains("apple-darwin") {
            // Additionally, `IPHONEOS_DEPLOYMENT_TARGET` must not be set when using the Xcode linker at
            // "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ld",
            // although this is apparently ignored when using the linker at "/usr/bin/ld".
            cmd.env_remove("IPHONEOS_DEPLOYMENT_TARGET");
        }
        Ok(())
    }

    fn apple_sdk_root(&self, sdk: &str) -> Result<OsString, Error> {
        if let Some(sdkroot) = env::var_os("SDKROOT") {
            return Ok(sdkroot);
        }

        let mut cache = self
            .apple_sdk_root_cache
            .lock()
            .expect("apple_sdk_root_cache lock failed");
        if let Some(ret) = cache.get(sdk) {
            return Ok(ret.clone());
        }

        let sdk_path = run_output(
            self.cmd("xcrun")
                .arg("--show-sdk-path")
                .arg("--sdk")
                .arg(sdk),
            "xcrun",
            &self.cargo_output,
        )?;

        let sdk_path = match String::from_utf8(sdk_path) {
            Ok(p) => p,
            Err(_) => {
                return Err(Error::new(
                    ErrorKind::IOError,
                    "Unable to determine Apple SDK path.",
                ));
            }
        };
        let ret: OsString = sdk_path.trim().into();
        cache.insert(sdk.into(), ret.clone());
        Ok(ret)
    }

    fn apple_deployment_version(&self, os: AppleOs, arch_str: Option<&str>, sdk: &str) -> String {
        let default_deployment_from_sdk = || {
            let mut cache = self
                .apple_versions_cache
                .lock()
                .expect("apple_versions_cache lock failed");

            if let Some(ret) = cache.get(sdk) {
                return Some(ret.clone());
            }

            let version = run_output(
                self.cmd("xcrun")
                    .arg("--show-sdk-platform-version")
                    .arg("--sdk")
                    .arg(sdk),
                "xcrun",
                &self.cargo_output,
            )
            .ok()?;

            let version = std::str::from_utf8(&version).ok()?.trim().to_owned();

            cache.insert(sdk.into(), version.clone());
            Some(version)
        };

        let deployment_from_env = |name: &str| {
            // note this isn't hit in production codepaths, its mostly just for tests which don't
            // set the real env
            if let Some((_, v)) = self.env.iter().find(|(k, _)| &**k == OsStr::new(name)) {
                Some(v.to_str().unwrap().to_string())
            } else {
                env::var(name).ok()
            }
        };

        // Determines if the acquired deployment target is too low to support modern C++ on some Apple platform.
        //
        // A long time ago they used libstdc++, but since macOS 10.9 and iOS 7 libc++ has been the library the SDKs provide to link against.
        // If a `cc`` config wants to use C++, we round up to these versions as the baseline.
        let maybe_cpp_version_baseline = |deployment_target_ver: String| -> Option<String> {
            if !self.cpp {
                return Some(deployment_target_ver);
            }

            let mut deployment_target = deployment_target_ver
                .split('.')
                .map(|v| v.parse::<u32>().expect("integer version"));

            match os {
                AppleOs::MacOs => {
                    let major = deployment_target.next().unwrap_or(0);
                    let minor = deployment_target.next().unwrap_or(0);

                    // If below 10.9, we ignore it and let the SDK's target definitions handle it.
                    if major == 10 && minor < 9 {
                        self.cargo_output.print_warning(&format_args!(
                            "macOS deployment target ({}) too low, it will be increased",
                            deployment_target_ver
                        ));
                        return None;
                    }
                }
                AppleOs::Ios => {
                    let major = deployment_target.next().unwrap_or(0);

                    // If below 10.7, we ignore it and let the SDK's target definitions handle it.
                    if major < 7 {
                        self.cargo_output.print_warning(&format_args!(
                            "iOS deployment target ({}) too low, it will be increased",
                            deployment_target_ver
                        ));
                        return None;
                    }
                }
                // watchOS, tvOS, and others are all new enough that libc++ is their baseline.
                _ => {}
            }

            // If the deployment target met or exceeded the C++ baseline
            Some(deployment_target_ver)
        };

        // The hardcoded minimums here are subject to change in a future compiler release,
        // and only exist as last resort fallbacks. Don't consider them stable.
        // `cc` doesn't use rustc's `--print deployment-target`` because the compiler's defaults
        // don't align well with Apple's SDKs and other third-party libraries that require ~generally~ higher
        // deployment targets. rustc isn't interested in those by default though so its fine to be different here.
        //
        // If no explicit target is passed, `cc` defaults to the current Xcode SDK's `DefaultDeploymentTarget` for better
        // compatibility. This is also the crate's historical behavior and what has become a relied-on value.
        //
        // The ordering of env -> XCode SDK -> old rustc defaults is intentional for performance when using
        // an explicit target.
        match os {
            AppleOs::MacOs => deployment_from_env("MACOSX_DEPLOYMENT_TARGET")
                .and_then(maybe_cpp_version_baseline)
                .or_else(default_deployment_from_sdk)
                .unwrap_or_else(|| {
                    if arch_str == Some("aarch64") {
                        "11.0".into()
                    } else {
                        let default = "10.7";
                        maybe_cpp_version_baseline(default.into()).unwrap_or_else(|| default.into())
                    }
                }),

            AppleOs::Ios => deployment_from_env("IPHONEOS_DEPLOYMENT_TARGET")
                .and_then(maybe_cpp_version_baseline)
                .or_else(default_deployment_from_sdk)
                .unwrap_or_else(|| "7.0".into()),

            AppleOs::WatchOs => deployment_from_env("WATCHOS_DEPLOYMENT_TARGET")
                .or_else(default_deployment_from_sdk)
                .unwrap_or_else(|| "5.0".into()),

            AppleOs::TvOs => deployment_from_env("TVOS_DEPLOYMENT_TARGET")
                .or_else(default_deployment_from_sdk)
                .unwrap_or_else(|| "9.0".into()),
        }
    }

    fn cuda_file_count(&self) -> usize {
        self.files
            .iter()
            .filter(|file| file.extension() == Some(OsStr::new("cu")))
            .count()
    }
}

impl Default for Build {
    fn default() -> Build {
        Build::new()
    }
}

fn fail(s: &str) -> ! {
    eprintln!("\n\nerror occurred: {}\n\n", s);
    std::process::exit(1);
}

#[derive(Clone, Copy, PartialEq)]
enum AppleOs {
    MacOs,
    Ios,
    WatchOs,
    TvOs,
}
impl std::fmt::Debug for AppleOs {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        match self {
            AppleOs::MacOs => f.write_str("macOS"),
            AppleOs::Ios => f.write_str("iOS"),
            AppleOs::WatchOs => f.write_str("WatchOS"),
            AppleOs::TvOs => f.write_str("AppleTVOS"),
        }
    }
}

struct AppleSdkTargetParts {
    sdk_prefix: &'static str,
    sim_prefix: &'static str,
    sdk: Cow<'static, str>,
}

fn apple_os_sdk_parts(os: AppleOs, arch: &AppleArchSpec) -> AppleSdkTargetParts {
    let (sdk_prefix, sim_prefix) = match os {
        AppleOs::MacOs => ("macosx", ""),
        AppleOs::Ios => ("iphone", "ios-"),
        AppleOs::WatchOs => ("watch", "watch"),
        AppleOs::TvOs => ("appletv", "appletv"),
    };
    let sdk = match arch {
        AppleArchSpec::Device(_) if os == AppleOs::MacOs => Cow::Borrowed("macosx"),
        AppleArchSpec::Device(_) => format!("{}os", sdk_prefix).into(),
        AppleArchSpec::Simulator(_) => format!("{}simulator", sdk_prefix).into(),
        AppleArchSpec::Catalyst(_) => Cow::Borrowed("macosx"),
    };

    AppleSdkTargetParts {
        sdk_prefix,
        sim_prefix,
        sdk,
    }
}

#[allow(dead_code)]
enum AppleArchSpec {
    Device(&'static str),
    Simulator(&'static str),
    #[allow(dead_code)]
    Catalyst(&'static str),
}

// Use by default minimum available API level
// See note about naming here
// https://android.googlesource.com/platform/ndk/+/refs/heads/ndk-release-r21/docs/BuildSystemMaintainers.md#Clang
static NEW_STANDALONE_ANDROID_COMPILERS: [&str; 4] = [
    "aarch64-linux-android21-clang",
    "armv7a-linux-androideabi16-clang",
    "i686-linux-android16-clang",
    "x86_64-linux-android21-clang",
];

// New "standalone" C/C++ cross-compiler executables from recent Android NDK
// are just shell scripts that call main clang binary (from Android NDK) with
// proper `--target` argument.
//
// For example, armv7a-linux-androideabi16-clang passes
// `--target=armv7a-linux-androideabi16` to clang.
// So to construct proper command line check if
// `--target` argument would be passed or not to clang
fn android_clang_compiler_uses_target_arg_internally(clang_path: &Path) -> bool {
    if let Some(filename) = clang_path.file_name() {
        if let Some(filename_str) = filename.to_str() {
            if let Some(idx) = filename_str.rfind('-') {
                return filename_str.split_at(idx).0.contains("android");
            }
        }
    }
    false
}

#[test]
fn test_android_clang_compiler_uses_target_arg_internally() {
    for version in 16..21 {
        assert!(android_clang_compiler_uses_target_arg_internally(
            &PathBuf::from(format!("armv7a-linux-androideabi{}-clang", version))
        ));
        assert!(android_clang_compiler_uses_target_arg_internally(
            &PathBuf::from(format!("armv7a-linux-androideabi{}-clang++", version))
        ));
    }
    assert!(!android_clang_compiler_uses_target_arg_internally(
        &PathBuf::from("clang-i686-linux-android")
    ));
    assert!(!android_clang_compiler_uses_target_arg_internally(
        &PathBuf::from("clang")
    ));
    assert!(!android_clang_compiler_uses_target_arg_internally(
        &PathBuf::from("clang++")
    ));
}

fn autodetect_android_compiler(target: &str, host: &str, gnu: &str, clang: &str) -> String {
    let new_clang_key = match target {
        "aarch64-linux-android" => Some("aarch64"),
        "armv7-linux-androideabi" => Some("armv7a"),
        "i686-linux-android" => Some("i686"),
        "x86_64-linux-android" => Some("x86_64"),
        _ => None,
    };

    let new_clang = new_clang_key
        .map(|key| {
            NEW_STANDALONE_ANDROID_COMPILERS
                .iter()
                .find(|x| x.starts_with(key))
        })
        .unwrap_or(None);

    if let Some(new_clang) = new_clang {
        if Command::new(new_clang).output().is_ok() {
            return (*new_clang).into();
        }
    }

    let target = target
        .replace("armv7neon", "arm")
        .replace("armv7", "arm")
        .replace("thumbv7neon", "arm")
        .replace("thumbv7", "arm");
    let gnu_compiler = format!("{}-{}", target, gnu);
    let clang_compiler = format!("{}-{}", target, clang);

    // On Windows, the Android clang compiler is provided as a `.cmd` file instead
    // of a `.exe` file. `std::process::Command` won't run `.cmd` files unless the
    // `.cmd` is explicitly appended to the command name, so we do that here.
    let clang_compiler_cmd = format!("{}-{}.cmd", target, clang);

    // Check if gnu compiler is present
    // if not, use clang
    if Command::new(&gnu_compiler).output().is_ok() {
        gnu_compiler
    } else if host.contains("windows") && Command::new(&clang_compiler_cmd).output().is_ok() {
        clang_compiler_cmd
    } else {
        clang_compiler
    }
}

// Rust and clang/cc don't agree on how to name the target.
fn map_darwin_target_from_rust_to_compiler_architecture(target: &str) -> Option<&'static str> {
    if target.contains("x86_64h") {
        Some("x86_64h")
    } else if target.contains("x86_64") {
        Some("x86_64")
    } else if target.contains("arm64e") {
        Some("arm64e")
    } else if target.contains("aarch64") {
        Some("arm64")
    } else if target.contains("i686") {
        Some("i386")
    } else if target.contains("powerpc") {
        Some("ppc")
    } else if target.contains("powerpc64") {
        Some("ppc64")
    } else {
        None
    }
}

fn which(tool: &Path, path_entries: Option<OsString>) -> Option<PathBuf> {
    fn check_exe(exe: &mut PathBuf) -> bool {
        let exe_ext = std::env::consts::EXE_EXTENSION;
        exe.exists() || (!exe_ext.is_empty() && exe.set_extension(exe_ext) && exe.exists())
    }

    // If |tool| is not just one "word," assume it's an actual path...
    if tool.components().count() > 1 {
        let mut exe = PathBuf::from(tool);
        return if check_exe(&mut exe) { Some(exe) } else { None };
    }

    // Loop through PATH entries searching for the |tool|.
    let path_entries = path_entries.or(env::var_os("PATH"))?;
    env::split_paths(&path_entries).find_map(|path_entry| {
        let mut exe = path_entry.join(tool);
        if check_exe(&mut exe) {
            Some(exe)
        } else {
            None
        }
    })
}

// search for |prog| on 'programs' path in '|cc| -print-search-dirs' output
fn search_programs(cc: &mut Command, prog: &str, cargo_output: &CargoOutput) -> Option<PathBuf> {
    let search_dirs = run_output(
        cc.arg("-print-search-dirs"),
        "cc",
        // this doesn't concern the compilation so we always want to show warnings.
        cargo_output,
    )
    .ok()?;
    // clang driver appears to be forcing UTF-8 output even on Windows,
    // hence from_utf8 is assumed to be usable in all cases.
    let search_dirs = std::str::from_utf8(&search_dirs).ok()?;
    for dirs in search_dirs.split(|c| c == '\r' || c == '\n') {
        if let Some(path) = dirs.strip_prefix("programs: =") {
            return which(Path::new(prog), Some(OsString::from(path)));
        }
    }
    None
}

#[derive(Clone, Copy, PartialEq)]
enum AsmFileExt {
    /// `.asm` files. On MSVC targets, we assume these should be passed to MASM
    /// (`ml{,64}.exe`).
    DotAsm,
    /// `.s` or `.S` files, which do not have the special handling on MSVC targets.
    DotS,
}

impl AsmFileExt {
    fn from_path(file: &Path) -> Option<Self> {
        if let Some(ext) = file.extension() {
            if let Some(ext) = ext.to_str() {
                let ext = ext.to_lowercase();
                match &*ext {
                    "asm" => return Some(AsmFileExt::DotAsm),
                    "s" => return Some(AsmFileExt::DotS),
                    _ => return None,
                }
            }
        }
        None
    }
}
