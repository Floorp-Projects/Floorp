extern crate tempdir;
extern crate which;

use std::env;
use std::ffi::{OsStr, OsString};
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
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
fn mk_bin(dir: &Path, path: &str, extension: &str) -> io::Result<PathBuf> {
    use std::os::unix::fs::OpenOptionsExt;
    let bin = dir.join(path).with_extension(extension);
    fs::OpenOptions::new()
        .write(true)
        .create(true)
        .mode(0o666 | (libc::S_IXUSR as u32))
        .open(&bin)
        .and_then(|_f| bin.canonicalize())
}

fn touch(dir: &Path, path: &str, extension: &str) -> io::Result<PathBuf> {
    let b = dir.join(path).with_extension(extension);
    fs::File::create(&b).and_then(|_f| b.canonicalize())
}

#[cfg(windows)]
fn mk_bin(dir: &Path, path: &str, extension: &str) -> io::Result<PathBuf> {
    touch(dir, path, extension)
}

impl TestFixture {
    // tmp/a/bin
    // tmp/a/bin.exe
    // tmp/a/bin.cmd
    // tmp/b/bin
    // tmp/b/bin.exe
    // tmp/b/bin.cmd
    // tmp/c/bin
    // tmp/c/bin.exe
    // tmp/c/bin.cmd
    pub fn new() -> TestFixture {
        let tempdir = TempDir::new("which_tests").unwrap();
        let mut builder = fs::DirBuilder::new();
        builder.recursive(true);
        let mut paths = vec![];
        let mut bins = vec![];
        for d in SUBDIRS.iter() {
            let p = tempdir.path().join(d);
            builder.create(&p).unwrap();
            bins.push(mk_bin(&p, &BIN_NAME, "").unwrap());
            bins.push(mk_bin(&p, &BIN_NAME, "exe").unwrap());
            bins.push(mk_bin(&p, &BIN_NAME, "cmd").unwrap());
            paths.push(p);
        }
        TestFixture {
            tempdir: tempdir,
            paths: env::join_paths(paths).unwrap(),
            bins: bins,
        }
    }

    #[allow(dead_code)]
    pub fn touch(&self, path: &str, extension: &str) -> io::Result<PathBuf> {
        touch(self.tempdir.path(), &path, &extension)
    }

    pub fn mk_bin(&self, path: &str, extension: &str) -> io::Result<PathBuf> {
        mk_bin(self.tempdir.path(), &path, &extension)
    }
}

fn _which<T: AsRef<OsStr>>(f: &TestFixture, path: T) -> which::Result<which::CanonicalPath> {
    which::CanonicalPath::new_in(path, Some(f.paths.clone()), f.tempdir.path())
}

#[test]
#[cfg(unix)]
fn it_works() {
    use std::process::Command;
    let result = which::Path::new("rustc");
    assert!(result.is_ok());

    let which_result = Command::new("which").arg("rustc").output();

    assert_eq!(
        String::from(result.unwrap().to_str().unwrap()),
        String::from_utf8(which_result.unwrap().stdout)
            .unwrap()
            .trim()
    );
}

#[test]
#[cfg(unix)]
fn test_which() {
    let f = TestFixture::new();
    assert_eq!(_which(&f, &BIN_NAME).unwrap(), f.bins[0])
}

#[test]
#[cfg(windows)]
fn test_which() {
    let f = TestFixture::new();
    assert_eq!(_which(&f, &BIN_NAME).unwrap(), f.bins[1])
}

#[test]
#[cfg(unix)]
fn test_which_extension() {
    let f = TestFixture::new();
    let b = Path::new(&BIN_NAME).with_extension("");
    assert_eq!(_which(&f, &b).unwrap(), f.bins[0])
}

#[test]
#[cfg(windows)]
fn test_which_extension() {
    let f = TestFixture::new();
    let b = Path::new(&BIN_NAME).with_extension("cmd");
    assert_eq!(_which(&f, &b).unwrap(), f.bins[2])
}

#[test]
fn test_which_not_found() {
    let f = TestFixture::new();
    assert!(_which(&f, "a").is_err());
}

#[test]
fn test_which_second() {
    let f = TestFixture::new();
    let b = f.mk_bin("b/another", env::consts::EXE_EXTENSION).unwrap();
    assert_eq!(_which(&f, "another").unwrap(), b);
}

#[test]
#[cfg(unix)]
fn test_which_absolute() {
    let f = TestFixture::new();
    assert_eq!(
        _which(&f, &f.bins[3]).unwrap(),
        f.bins[3].canonicalize().unwrap()
    );
}

#[test]
#[cfg(windows)]
fn test_which_absolute() {
    let f = TestFixture::new();
    assert_eq!(
        _which(&f, &f.bins[4]).unwrap(),
        f.bins[4].canonicalize().unwrap()
    );
}

#[test]
#[cfg(windows)]
fn test_which_absolute_path_case() {
    // Test that an absolute path with an uppercase extension
    // is accepted.
    let f = TestFixture::new();
    let p = &f.bins[4];
    assert_eq!(_which(&f, &p).unwrap(), f.bins[4].canonicalize().unwrap());
}

#[test]
#[cfg(unix)]
fn test_which_absolute_extension() {
    let f = TestFixture::new();
    // Don't append EXE_EXTENSION here.
    let b = f.bins[3].parent().unwrap().join(&BIN_NAME);
    assert_eq!(_which(&f, &b).unwrap(), f.bins[3].canonicalize().unwrap());
}

#[test]
#[cfg(windows)]
fn test_which_absolute_extension() {
    let f = TestFixture::new();
    // Don't append EXE_EXTENSION here.
    let b = f.bins[4].parent().unwrap().join(&BIN_NAME);
    assert_eq!(_which(&f, &b).unwrap(), f.bins[4].canonicalize().unwrap());
}

#[test]
#[cfg(unix)]
fn test_which_relative() {
    let f = TestFixture::new();
    assert_eq!(
        _which(&f, "b/bin").unwrap(),
        f.bins[3].canonicalize().unwrap()
    );
}

#[test]
#[cfg(windows)]
fn test_which_relative() {
    let f = TestFixture::new();
    assert_eq!(
        _which(&f, "b/bin").unwrap(),
        f.bins[4].canonicalize().unwrap()
    );
}

#[test]
#[cfg(unix)]
fn test_which_relative_extension() {
    // test_which_relative tests a relative path without an extension,
    // so test a relative path with an extension here.
    let f = TestFixture::new();
    let b = Path::new("b/bin").with_extension(env::consts::EXE_EXTENSION);
    assert_eq!(_which(&f, &b).unwrap(), f.bins[3].canonicalize().unwrap());
}

#[test]
#[cfg(windows)]
fn test_which_relative_extension() {
    // test_which_relative tests a relative path without an extension,
    // so test a relative path with an extension here.
    let f = TestFixture::new();
    let b = Path::new("b/bin").with_extension("cmd");
    assert_eq!(_which(&f, &b).unwrap(), f.bins[5].canonicalize().unwrap());
}

#[test]
#[cfg(windows)]
fn test_which_relative_extension_case() {
    // Test that a relative path with an uppercase extension
    // is accepted.
    let f = TestFixture::new();
    let b = Path::new("b/bin").with_extension("EXE");
    assert_eq!(_which(&f, &b).unwrap(), f.bins[4].canonicalize().unwrap());
}

#[test]
#[cfg(unix)]
fn test_which_relative_leading_dot() {
    let f = TestFixture::new();
    assert_eq!(
        _which(&f, "./b/bin").unwrap(),
        f.bins[3].canonicalize().unwrap()
    );
}

#[test]
#[cfg(windows)]
fn test_which_relative_leading_dot() {
    let f = TestFixture::new();
    assert_eq!(
        _which(&f, "./b/bin").unwrap(),
        f.bins[4].canonicalize().unwrap()
    );
}

#[test]
#[cfg(unix)]
fn test_which_non_executable() {
    // Shouldn't return non-executable files.
    let f = TestFixture::new();
    f.touch("b/another", "").unwrap();
    assert!(_which(&f, "another").is_err());
}

#[test]
#[cfg(unix)]
fn test_which_absolute_non_executable() {
    // Shouldn't return non-executable files, even if given an absolute path.
    let f = TestFixture::new();
    let b = f.touch("b/another", "").unwrap();
    assert!(_which(&f, &b).is_err());
}

#[test]
#[cfg(unix)]
fn test_which_relative_non_executable() {
    // Shouldn't return non-executable files.
    let f = TestFixture::new();
    f.touch("b/another", "").unwrap();
    assert!(_which(&f, "b/another").is_err());
}

#[test]
#[cfg(feature = "failure")]
fn test_failure() {
    let f = TestFixture::new();

    let run = || -> std::result::Result<PathBuf, failure::Error> {
        // Test the conversion to failure
        let p = _which(&f, "./b/bin")?;
        Ok(p.into_path_buf())
    };

    let _ = run();
}
