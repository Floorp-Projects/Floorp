use std::{
    collections::HashMap,
    ffi::OsString,
    path::{Path, PathBuf},
    process::Command,
    sync::Mutex,
};

use crate::command_helpers::{run_output, CargoOutput};

/// Configuration used to represent an invocation of a C compiler.
///
/// This can be used to figure out what compiler is in use, what the arguments
/// to it are, and what the environment variables look like for the compiler.
/// This can be used to further configure other build systems (e.g. forward
/// along CC and/or CFLAGS) or the `to_command` method can be used to run the
/// compiler itself.
#[derive(Clone, Debug)]
#[allow(missing_docs)]
pub struct Tool {
    pub(crate) path: PathBuf,
    pub(crate) cc_wrapper_path: Option<PathBuf>,
    pub(crate) cc_wrapper_args: Vec<OsString>,
    pub(crate) args: Vec<OsString>,
    pub(crate) env: Vec<(OsString, OsString)>,
    pub(crate) family: ToolFamily,
    pub(crate) cuda: bool,
    pub(crate) removed_args: Vec<OsString>,
    pub(crate) has_internal_target_arg: bool,
}

impl Tool {
    pub(crate) fn new(
        path: PathBuf,
        cached_compiler_family: &Mutex<HashMap<Box<Path>, ToolFamily>>,
        cargo_output: &CargoOutput,
    ) -> Self {
        Self::with_features(path, None, false, cached_compiler_family, cargo_output)
    }

    pub(crate) fn with_clang_driver(
        path: PathBuf,
        clang_driver: Option<&str>,
        cached_compiler_family: &Mutex<HashMap<Box<Path>, ToolFamily>>,
        cargo_output: &CargoOutput,
    ) -> Self {
        Self::with_features(
            path,
            clang_driver,
            false,
            cached_compiler_family,
            cargo_output,
        )
    }

    /// Explicitly set the `ToolFamily`, skipping name-based detection.
    pub(crate) fn with_family(path: PathBuf, family: ToolFamily) -> Self {
        Self {
            path,
            cc_wrapper_path: None,
            cc_wrapper_args: Vec::new(),
            args: Vec::new(),
            env: Vec::new(),
            family,
            cuda: false,
            removed_args: Vec::new(),
            has_internal_target_arg: false,
        }
    }

    pub(crate) fn with_features(
        path: PathBuf,
        clang_driver: Option<&str>,
        cuda: bool,
        cached_compiler_family: &Mutex<HashMap<Box<Path>, ToolFamily>>,
        cargo_output: &CargoOutput,
    ) -> Self {
        fn detect_family_inner(path: &Path, cargo_output: &CargoOutput) -> ToolFamily {
            let mut cmd = Command::new(path);
            cmd.arg("--version");

            let stdout = match run_output(
                &mut cmd,
                &path.to_string_lossy(),
                // tool detection issues should always be shown as warnings
                cargo_output,
            )
            .ok()
            .and_then(|o| String::from_utf8(o).ok())
            {
                Some(s) => s,
                None => {
                    // --version failed. fallback to gnu
                    cargo_output.print_warning(&format_args!("Failed to run: {:?}", cmd));
                    return ToolFamily::Gnu;
                }
            };
            if stdout.contains("clang") {
                ToolFamily::Clang
            } else if stdout.contains("GCC") {
                ToolFamily::Gnu
            } else {
                // --version doesn't include clang for GCC
                cargo_output.print_warning(&format_args!(
                    "Compiler version doesn't include clang or GCC: {:?}",
                    cmd
                ));
                ToolFamily::Gnu
            }
        }
        let detect_family = |path: &Path| -> ToolFamily {
            if let Some(family) = cached_compiler_family.lock().unwrap().get(path) {
                return *family;
            }

            let family = detect_family_inner(path, cargo_output);
            cached_compiler_family
                .lock()
                .unwrap()
                .insert(path.into(), family);
            family
        };

        // Try to detect family of the tool from its name, falling back to Gnu.
        let family = if let Some(fname) = path.file_name().and_then(|p| p.to_str()) {
            if fname.contains("clang-cl") {
                ToolFamily::Msvc { clang_cl: true }
            } else if fname.ends_with("cl") || fname == "cl.exe" {
                ToolFamily::Msvc { clang_cl: false }
            } else if fname.contains("clang") {
                match clang_driver {
                    Some("cl") => ToolFamily::Msvc { clang_cl: true },
                    _ => ToolFamily::Clang,
                }
            } else {
                detect_family(&path)
            }
        } else {
            detect_family(&path)
        };

        Tool {
            path,
            cc_wrapper_path: None,
            cc_wrapper_args: Vec::new(),
            args: Vec::new(),
            env: Vec::new(),
            family,
            cuda,
            removed_args: Vec::new(),
            has_internal_target_arg: false,
        }
    }

    /// Add an argument to be stripped from the final command arguments.
    pub(crate) fn remove_arg(&mut self, flag: OsString) {
        self.removed_args.push(flag);
    }

    /// Push an "exotic" flag to the end of the compiler's arguments list.
    ///
    /// Nvidia compiler accepts only the most common compiler flags like `-D`,
    /// `-I`, `-c`, etc. Options meant specifically for the underlying
    /// host C++ compiler have to be prefixed with `-Xcompiler`.
    /// [Another possible future application for this function is passing
    /// clang-specific flags to clang-cl, which otherwise accepts only
    /// MSVC-specific options.]
    pub(crate) fn push_cc_arg(&mut self, flag: OsString) {
        if self.cuda {
            self.args.push("-Xcompiler".into());
        }
        self.args.push(flag);
    }

    /// Checks if an argument or flag has already been specified or conflicts.
    ///
    /// Currently only checks optimization flags.
    pub(crate) fn is_duplicate_opt_arg(&self, flag: &OsString) -> bool {
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
            return self
                .args()
                .iter()
                .any(|a| a.to_str().unwrap_or("").chars().nth(1) == Some('O'));
        }

        // TODO Check for existing -m..., -m...=..., /arch:... flags
        false
    }

    /// Don't push optimization arg if it conflicts with existing args.
    pub(crate) fn push_opt_unless_duplicate(&mut self, flag: OsString) {
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
                let mut cmd = Command::new(cc_wrapper_path);
                cmd.arg(&self.path);
                cmd
            }
            None => Command::new(&self.path),
        };
        cmd.args(&self.cc_wrapper_args);

        let value = self
            .args
            .iter()
            .filter(|a| !self.removed_args.contains(a))
            .collect::<Vec<_>>();
        cmd.args(&value);

        for (k, v) in self.env.iter() {
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

    /// Whether the tool is AppleClang under .xctoolchain
    #[cfg(target_vendor = "apple")]
    pub(crate) fn is_xctoolchain_clang(&self) -> bool {
        let path = self.path.to_string_lossy();
        path.contains(".xctoolchain/")
    }
    #[cfg(not(target_vendor = "apple"))]
    pub(crate) fn is_xctoolchain_clang(&self) -> bool {
        false
    }

    /// Whether the tool is MSVC-like.
    pub fn is_like_msvc(&self) -> bool {
        match self.family {
            ToolFamily::Msvc { .. } => true,
            _ => false,
        }
    }
}

/// Represents the family of tools this tool belongs to.
///
/// Each family of tools differs in how and what arguments they accept.
///
/// Detection of a family is done on best-effort basis and may not accurately reflect the tool.
#[derive(Copy, Clone, Debug, PartialEq)]
pub enum ToolFamily {
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
    pub(crate) fn add_debug_flags(&self, cmd: &mut Tool, dwarf_version: Option<u32>) {
        match *self {
            ToolFamily::Msvc { .. } => {
                cmd.push_cc_arg("-Z7".into());
            }
            ToolFamily::Gnu | ToolFamily::Clang => {
                cmd.push_cc_arg(
                    dwarf_version
                        .map_or_else(|| "-g".into(), |v| format!("-gdwarf-{}", v))
                        .into(),
                );
            }
        }
    }

    /// What the flag to force frame pointers.
    pub(crate) fn add_force_frame_pointer(&self, cmd: &mut Tool) {
        match *self {
            ToolFamily::Gnu | ToolFamily::Clang => {
                cmd.push_cc_arg("-fno-omit-frame-pointer".into());
            }
            _ => (),
        }
    }

    /// What the flags to enable all warnings
    pub(crate) fn warnings_flags(&self) -> &'static str {
        match *self {
            ToolFamily::Msvc { .. } => "-W4",
            ToolFamily::Gnu | ToolFamily::Clang => "-Wall",
        }
    }

    /// What the flags to enable extra warnings
    pub(crate) fn extra_warnings_flags(&self) -> Option<&'static str> {
        match *self {
            ToolFamily::Msvc { .. } => None,
            ToolFamily::Gnu | ToolFamily::Clang => Some("-Wextra"),
        }
    }

    /// What the flag to turn warning into errors
    pub(crate) fn warnings_to_errors_flag(&self) -> &'static str {
        match *self {
            ToolFamily::Msvc { .. } => "-WX",
            ToolFamily::Gnu | ToolFamily::Clang => "-Werror",
        }
    }

    pub(crate) fn verbose_stderr(&self) -> bool {
        *self == ToolFamily::Clang
    }
}
