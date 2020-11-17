//! which
//!
//! A Rust equivalent of Unix command `which(1)`.
//! # Example:
//!
//! To find which rustc executable binary is using:
//!
//! ``` norun
//! use which::which;
//!
//! let result = which::which("rustc").unwrap();
//! assert_eq!(result, PathBuf::from("/usr/bin/rustc"));
//!
//! ```

#[cfg(feature = "failure")]
extern crate failure;
extern crate libc;

#[cfg(feature = "failure")]
use failure::ResultExt;
mod checker;
mod error;
mod finder;
#[cfg(windows)]
mod helper;

use std::env;
use std::fmt;
use std::path;

use std::ffi::OsStr;

use checker::CompositeChecker;
use checker::ExecutableChecker;
use checker::ExistedChecker;
pub use error::*;
use finder::Finder;

/// Find a exectable binary's path by name.
///
/// If given an absolute path, returns it if the file exists and is executable.
///
/// If given a relative path, returns an absolute path to the file if
/// it exists and is executable.
///
/// If given a string without path separators, looks for a file named
/// `binary_name` at each directory in `$PATH` and if it finds an executable
/// file there, returns it.
///
/// # Example
///
/// ``` norun
/// use which::which;
/// use std::path::PathBuf;
///
/// let result = which::which("rustc").unwrap();
/// assert_eq!(result, PathBuf::from("/usr/bin/rustc"));
///
/// ```
pub fn which<T: AsRef<OsStr>>(binary_name: T) -> Result<path::PathBuf> {
    #[cfg(feature = "failure")]
    let cwd = env::current_dir().context(ErrorKind::CannotGetCurrentDir)?;
    #[cfg(not(feature = "failure"))]
    let cwd = env::current_dir().map_err(|_| ErrorKind::CannotGetCurrentDir)?;

    which_in(binary_name, env::var_os("PATH"), &cwd)
}

/// Find `binary_name` in the path list `paths`, using `cwd` to resolve relative paths.
pub fn which_in<T, U, V>(binary_name: T, paths: Option<U>, cwd: V) -> Result<path::PathBuf>
where
    T: AsRef<OsStr>,
    U: AsRef<OsStr>,
    V: AsRef<path::Path>,
{
    let binary_checker = CompositeChecker::new()
        .add_checker(Box::new(ExistedChecker::new()))
        .add_checker(Box::new(ExecutableChecker::new()));

    let finder = Finder::new();

    finder.find(binary_name, paths, cwd, &binary_checker)
}

/// An owned, immutable wrapper around a `PathBuf` containing the path of an executable.
///
/// The constructed `PathBuf` is the output of `which` or `which_in`, but `which::Path` has the
/// advantage of being a type distinct from `std::path::Path` and `std::path::PathBuf`.
///
/// It can be beneficial to use `which::Path` instead of `std::path::Path` when you want the type
/// system to enforce the need for a path that exists and points to a binary that is executable.
///
/// Since `which::Path` implements `Deref` for `std::path::Path`, all methods on `&std::path::Path`
/// are also available to `&which::Path` values.
#[derive(Clone, PartialEq)]
pub struct Path {
    inner: path::PathBuf,
}

impl Path {
    /// Returns the path of an executable binary by name.
    ///
    /// This calls `which` and maps the result into a `Path`.
    pub fn new<T: AsRef<OsStr>>(binary_name: T) -> Result<Path> {
        which(binary_name).map(|inner| Path { inner })
    }

    /// Returns the path of an executable binary by name in the path list `paths` and using the
    /// current working directory `cwd` to resolve relative paths.
    ///
    /// This calls `which_in` and maps the result into a `Path`.
    pub fn new_in<T, U, V>(binary_name: T, paths: Option<U>, cwd: V) -> Result<Path>
    where
        T: AsRef<OsStr>,
        U: AsRef<OsStr>,
        V: AsRef<path::Path>,
    {
        which_in(binary_name, paths, cwd).map(|inner| Path { inner })
    }

    /// Returns a reference to a `std::path::Path`.
    pub fn as_path(&self) -> &path::Path {
        self.inner.as_path()
    }

    /// Consumes the `which::Path`, yielding its underlying `std::path::PathBuf`.
    pub fn into_path_buf(self) -> path::PathBuf {
        self.inner
    }
}

impl fmt::Debug for Path {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(&self.inner, f)
    }
}

impl std::ops::Deref for Path {
    type Target = path::Path;

    fn deref(&self) -> &path::Path {
        self.inner.deref()
    }
}

impl AsRef<path::Path> for Path {
    fn as_ref(&self) -> &path::Path {
        self.as_path()
    }
}

impl AsRef<OsStr> for Path {
    fn as_ref(&self) -> &OsStr {
        self.as_os_str()
    }
}

impl Eq for Path {}

impl PartialEq<path::PathBuf> for Path {
    fn eq(&self, other: &path::PathBuf) -> bool {
        self.inner == *other
    }
}

impl PartialEq<Path> for path::PathBuf {
    fn eq(&self, other: &Path) -> bool {
        *self == other.inner
    }
}

/// An owned, immutable wrapper around a `PathBuf` containing the _canonical_ path of an
/// executable.
///
/// The constructed `PathBuf` is the result of `which` or `which_in` followed by
/// `Path::canonicalize`, but `CanonicalPath` has the advantage of being a type distinct from
/// `std::path::Path` and `std::path::PathBuf`.
///
/// It can be beneficial to use `CanonicalPath` instead of `std::path::Path` when you want the type
/// system to enforce the need for a path that exists, points to a binary that is executable, is
/// absolute, has all components normalized, and has all symbolic links resolved
///
/// Since `CanonicalPath` implements `Deref` for `std::path::Path`, all methods on
/// `&std::path::Path` are also available to `&CanonicalPath` values.
#[derive(Clone, PartialEq)]
pub struct CanonicalPath {
    inner: path::PathBuf,
}

impl CanonicalPath {
    /// Returns the canonical path of an executable binary by name.
    ///
    /// This calls `which` and `Path::canonicalize` and maps the result into a `CanonicalPath`.
    pub fn new<T: AsRef<OsStr>>(binary_name: T) -> Result<CanonicalPath> {
        which(binary_name)
            .and_then(|p| {
                p.canonicalize()
                    .map_err(|_| ErrorKind::CannotCanonicalize.into())
            })
            .map(|inner| CanonicalPath { inner })
    }

    /// Returns the canonical path of an executable binary by name in the path list `paths` and
    /// using the current working directory `cwd` to resolve relative paths.
    ///
    /// This calls `which` and `Path::canonicalize` and maps the result into a `CanonicalPath`.
    pub fn new_in<T, U, V>(binary_name: T, paths: Option<U>, cwd: V) -> Result<CanonicalPath>
    where
        T: AsRef<OsStr>,
        U: AsRef<OsStr>,
        V: AsRef<path::Path>,
    {
        which_in(binary_name, paths, cwd)
            .and_then(|p| {
                p.canonicalize()
                    .map_err(|_| ErrorKind::CannotCanonicalize.into())
            })
            .map(|inner| CanonicalPath { inner })
    }

    /// Returns a reference to a `std::path::Path`.
    pub fn as_path(&self) -> &path::Path {
        self.inner.as_path()
    }

    /// Consumes the `which::CanonicalPath`, yielding its underlying `std::path::PathBuf`.
    pub fn into_path_buf(self) -> path::PathBuf {
        self.inner
    }
}

impl fmt::Debug for CanonicalPath {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Debug::fmt(&self.inner, f)
    }
}

impl std::ops::Deref for CanonicalPath {
    type Target = path::Path;

    fn deref(&self) -> &path::Path {
        self.inner.deref()
    }
}

impl AsRef<path::Path> for CanonicalPath {
    fn as_ref(&self) -> &path::Path {
        self.as_path()
    }
}

impl AsRef<OsStr> for CanonicalPath {
    fn as_ref(&self) -> &OsStr {
        self.as_os_str()
    }
}

impl Eq for CanonicalPath {}

impl PartialEq<path::PathBuf> for CanonicalPath {
    fn eq(&self, other: &path::PathBuf) -> bool {
        self.inner == *other
    }
}

impl PartialEq<CanonicalPath> for path::PathBuf {
    fn eq(&self, other: &CanonicalPath) -> bool {
        *self == other.inner
    }
}
