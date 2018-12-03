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

extern crate libc;
#[cfg(test)]
extern crate tempdir;

use std::ascii::AsciiExt;
use std::path::{Path,PathBuf};
use std::{env, fs};
#[cfg(unix)]
use std::ffi::CString;
use std::ffi::OsStr;
#[cfg(unix)]
use std::os::unix::ffi::OsStrExt;

/// Like `Path::with_extension`, but don't replace an existing extension.
fn ensure_exe_extension<T: AsRef<Path>>(path: T) -> PathBuf {
    if env::consts::EXE_EXTENSION.is_empty() {
        // Nothing to do.
        path.as_ref().to_path_buf()
    } else {
        match path.as_ref().extension().and_then(|e| e.to_str()).map(|e| e.eq_ignore_ascii_case(env::consts::EXE_EXTENSION)) {
            // Already has the right extension.
            Some(true) => path.as_ref().to_path_buf(),
            _ => {
                // Append the extension.
                let mut s = path.as_ref().to_path_buf().into_os_string();
                s.push(".");
                s.push(env::consts::EXE_EXTENSION);
                PathBuf::from(s)
            }
        }
    }
}


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
pub fn which<T: AsRef<OsStr>>(binary_name: T)
             -> Result<PathBuf, &'static str> {
    env::current_dir()
        .or_else(|_| Err("Couldn't get current directory"))
        .and_then(|cwd| which_in(binary_name, env::var_os("PATH"), &cwd))
}

/// Find `binary_name` in the path list `paths`, using `cwd` to resolve relative paths.
pub fn which_in<T, U, V>(binary_name: T, paths: Option<U>, cwd: V)
             -> Result<PathBuf, &'static str>
                where T: AsRef<OsStr>,
                      U: AsRef<OsStr>,
                      V: AsRef<Path> {
    let binary_checker = CompositeChecker::new()
        .add_checker(Box::new(ExistedChecker::new()))
        .add_checker(Box::new(ExecutableChecker::new()));

    let finder = Finder::new();

    finder.find(binary_name, paths, cwd, &binary_checker)
}

struct Finder;

impl Finder {
    fn new() -> Finder {
        Finder
    }

    fn find<T, U, V>(&self, binary_name: T, paths: Option<U>, cwd: V,
                     binary_checker: &Checker)
                     -> Result<PathBuf, &'static str>
        where T: AsRef<OsStr>,
              U: AsRef<OsStr>,
              V: AsRef<Path> {

        let path = ensure_exe_extension(binary_name.as_ref());

        // Does it have a path separator?
        if path.components().count() > 1 {
            if path.is_absolute() {
                if binary_checker.is_valid(&path) {
                    // Already fine.
                    Ok(path)
                } else {
                    // Absolute path but it's not usable.
                    Err("Bad absolute path")
                }
            } else {
                // Try to make it absolute.
                let mut new_path = PathBuf::from(cwd.as_ref());
                new_path.push(path);
                let new_path = ensure_exe_extension(new_path);
                if binary_checker.is_valid(&new_path) {
                    Ok(new_path)
                } else {
                    // File doesn't exist or isn't executable.
                    Err("Bad relative path")
                }
            }
        } else {
            // No separator, look it up in `paths`.
            paths.and_then(
                |paths|
                env::split_paths(paths.as_ref())
                    .map(|p| ensure_exe_extension(p.join(binary_name.as_ref())))
                    .skip_while(|p| !(binary_checker.is_valid(&p)))
                    .next())
                .ok_or("Cannot find binary path")
        }
    }
}


trait Checker {
    fn is_valid(&self, path: &Path) -> bool;
}

struct ExecutableChecker;

impl ExecutableChecker {
    fn new() -> ExecutableChecker {
        ExecutableChecker
    }
}

impl Checker for ExecutableChecker {
    #[cfg(unix)]
    fn is_valid(&self, path: &Path) -> bool {
        CString::new(path.as_os_str().as_bytes())
            .and_then(|c| {
                Ok(unsafe { libc::access(c.as_ptr(), libc::X_OK) == 0 })
            })
            .unwrap_or(false)
    }

    #[cfg(not(unix))]
    fn is_valid(&self, _path: &Path) -> bool { true }
}

struct ExistedChecker;

impl ExistedChecker {
    fn new() -> ExistedChecker {
        ExistedChecker
    }
}

impl Checker for ExistedChecker {
    fn is_valid(&self, path: &Path) -> bool {
        fs::metadata(path).map(|metadata|{
            metadata.is_file()
        }).unwrap_or(false)
    }
}

struct CompositeChecker {
    checkers: Vec<Box<Checker>>
}

impl CompositeChecker {
    fn new() -> CompositeChecker {
        CompositeChecker {
            checkers: Vec::new()
        }
    }

    fn add_checker(mut self, checker: Box<Checker>) -> CompositeChecker {
        self.checkers.push(checker);
        self
    }
}

impl Checker for CompositeChecker {
    fn is_valid(&self, path: &Path) -> bool {
        self.checkers.iter()
            .all(|checker| checker.is_valid(path))
    }
}

#[test]
fn test_exe_extension() {
    let expected = PathBuf::from("foo").with_extension(env::consts::EXE_EXTENSION);
    assert_eq!(expected, ensure_exe_extension(PathBuf::from("foo")));
    let p = expected.clone();
    assert_eq!(expected, ensure_exe_extension(p));
}

#[test]
#[cfg(windows)]
fn test_exe_extension_existing_extension() {
    assert_eq!(PathBuf::from("foo.bar.exe"),
               ensure_exe_extension("foo.bar"));
}

#[test]
#[cfg(windows)]
fn test_exe_extension_existing_extension_uppercase() {
    assert_eq!(PathBuf::from("foo.EXE"),
               ensure_exe_extension("foo.EXE"));
}

#[cfg(test)]
mod test {
    use super::*;

    use std::env;
    use std::ffi::{OsStr,OsString};
    use std::fs;
    use std::io;
    use std::path::{Path,PathBuf};
    use tempdir::TempDir;

    struct TestFixture {
        /// Temp directory.
        pub tempdir: TempDir,
        /// $PATH
        pub paths: OsString,
        /// Binaries created in $PATH
        pub bins: Vec<PathBuf>,
    }

    const SUBDIRS: &'static [&'static str] = &["a", "b", "c"];
    const BIN_NAME: &'static str = "bin";

    #[cfg(unix)]
    fn mk_bin(dir: &Path, path: &str) -> io::Result<PathBuf> {
        use libc;
        use std::os::unix::fs::OpenOptionsExt;
        let bin = dir.join(path).with_extension(env::consts::EXE_EXTENSION);
        fs::OpenOptions::new()
            .write(true)
            .create(true)
            .mode(0o666 | (libc::S_IXUSR as u32))
            .open(&bin)
            .and_then(|_f| bin.canonicalize())
    }

    fn touch(dir: &Path, path: &str) -> io::Result<PathBuf> {
        let b = dir.join(path).with_extension(env::consts::EXE_EXTENSION);
        fs::File::create(&b)
            .and_then(|_f| b.canonicalize())
    }

    #[cfg(not(unix))]
    fn mk_bin(dir: &Path, path: &str) -> io::Result<PathBuf> {
        touch(dir, path)
    }

    impl TestFixture {
        pub fn new() -> TestFixture {
            let tempdir = TempDir::new("which_tests").unwrap();
            let mut builder = fs::DirBuilder::new();
            builder.recursive(true);
            let mut paths = vec!();
            let mut bins = vec!();
            for d in SUBDIRS.iter() {
                let p = tempdir.path().join(d);
                builder.create(&p).unwrap();
                bins.push(mk_bin(&p, &BIN_NAME).unwrap());
                paths.push(p);
            }
            TestFixture {
                tempdir: tempdir,
                paths: env::join_paths(paths).unwrap(),
                bins: bins,
            }
        }

        #[allow(dead_code)]
        pub fn touch(&self, path: &str) -> io::Result<PathBuf> {
            touch(self.tempdir.path(), &path)
        }

        pub fn mk_bin(&self, path: &str) -> io::Result<PathBuf> {
            mk_bin(self.tempdir.path(), &path)
        }
    }

    fn _which<T: AsRef<OsStr>>(f: &TestFixture, path: T) -> Result<PathBuf, &'static str> {
        which_in(path, Some(f.paths.clone()), f.tempdir.path())
    }

    #[test]
    #[cfg(unix)]
    fn it_works() {
        use std::process::Command;
        let result = which("rustc");
        assert!(result.is_ok());

        let which_result = Command::new("which")
            .arg("rustc")
            .output();

        assert_eq!(String::from(result.unwrap().to_str().unwrap()),
                   String::from_utf8(which_result.unwrap().stdout).unwrap().trim());
    }

    #[test]
    fn test_which() {
        let f = TestFixture::new();
        assert_eq!(_which(&f, &BIN_NAME).unwrap().canonicalize().unwrap(),
                   f.bins[0])
    }

    #[test]
    fn test_which_extension() {
        let f = TestFixture::new();
        let b = Path::new(&BIN_NAME).with_extension(env::consts::EXE_EXTENSION);
        assert_eq!(_which(&f, &b).unwrap().canonicalize().unwrap(),
                   f.bins[0])
    }

    #[test]
    fn test_which_not_found() {
        let f = TestFixture::new();
        assert!(_which(&f, "a").is_err());
    }

    #[test]
    fn test_which_second() {
        let f = TestFixture::new();
        let b = f.mk_bin("b/another").unwrap();
        assert_eq!(_which(&f, "another").unwrap().canonicalize().unwrap(), b);
    }

    #[test]
    fn test_which_absolute() {
        let f = TestFixture::new();
        assert_eq!(_which(&f, &f.bins[1]).unwrap().canonicalize().unwrap(),
                   f.bins[1].canonicalize().unwrap());
    }

    #[test]
    #[cfg(windows)]
    fn test_which_absolute_path_case() {
        // Test that an absolute path with an uppercase extension
        // is accepted.
        let f = TestFixture::new();
        let p = f.bins[1].with_extension("EXE");
        assert_eq!(_which(&f, &p).unwrap().canonicalize().unwrap(),
                   f.bins[1].canonicalize().unwrap());
    }

    #[test]
    fn test_which_absolute_extension() {
        let f = TestFixture::new();
        // Don't append EXE_EXTENSION here.
        let b = f.bins[1].parent().unwrap().join(&BIN_NAME);
        assert_eq!(_which(&f, &b).unwrap().canonicalize().unwrap(),
                   f.bins[1].canonicalize().unwrap());
    }

    #[test]
    fn test_which_relative() {
        let f = TestFixture::new();
        assert_eq!(_which(&f, "b/bin").unwrap().canonicalize().unwrap(),
                   f.bins[1].canonicalize().unwrap());
    }

    #[test]
    fn test_which_relative_extension() {
        // test_which_relative tests a relative path without an extension,
        // so test a relative path with an extension here.
        let f = TestFixture::new();
        let b = Path::new("b/bin").with_extension(env::consts::EXE_EXTENSION);
        assert_eq!(_which(&f, &b).unwrap().canonicalize().unwrap(),
                   f.bins[1].canonicalize().unwrap());
    }

    #[test]
    #[cfg(windows)]
    fn test_which_relative_extension_case() {
        // Test that a relative path with an uppercase extension
        // is accepted.
        let f = TestFixture::new();
        let b = Path::new("b/bin").with_extension("EXE");
        assert_eq!(_which(&f, &b).unwrap().canonicalize().unwrap(),
                   f.bins[1].canonicalize().unwrap());
    }

    #[test]
    fn test_which_relative_leading_dot() {
        let f = TestFixture::new();
        assert_eq!(_which(&f, "./b/bin").unwrap().canonicalize().unwrap(),
                   f.bins[1].canonicalize().unwrap());
    }

    #[test]
    #[cfg(unix)]
    fn test_which_non_executable() {
        // Shouldn't return non-executable files.
        let f = TestFixture::new();
        f.touch("b/another").unwrap();
        assert!(_which(&f, "another").is_err());
    }

    #[test]
    #[cfg(unix)]
    fn test_which_absolute_non_executable() {
        // Shouldn't return non-executable files, even if given an absolute path.
        let f = TestFixture::new();
        let b = f.touch("b/another").unwrap();
        assert!(_which(&f, &b).is_err());
    }

    #[test]
    #[cfg(unix)]
    fn test_which_relative_non_executable() {
        // Shouldn't return non-executable files.
        let f = TestFixture::new();
        f.touch("b/another").unwrap();
        assert!(_which(&f, "b/another").is_err());
    }
}
