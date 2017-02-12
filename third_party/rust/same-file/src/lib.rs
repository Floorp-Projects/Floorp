/*!
This crate provides a safe and simple **cross platform** way to determine
whether two file paths refer to the same file or directory.

Most uses of this crate should be limited to the top-level `is_same_file`
function, which takes two file paths and returns true if they refer to the
same file or directory:

```rust,no_run
# fn example() -> ::std::io::Result<()> {
use same_file::is_same_file;

assert!(try!(is_same_file("/bin/sh", "/usr/bin/sh")));
# Ok(()) } example().unwrap();
```

Additionally, this crate provides a `Handle` type that permits a more efficient
equality check depending on your access pattern. For example, if one wanted to
checked whether any path in a list of paths corresponded to the process' stdout
handle, then one could build a handle once for stdout. The equality check for
each file in the list then only requires one stat call instead of two. The code
might look like this:

```rust,no_run
# fn example() -> ::std::io::Result<()> {
use same_file::Handle;

let candidates = &[
    "examples/is_same_file.rs",
    "examples/is_stderr.rs",
    "examples/stderr",
];
let stdout_handle = try!(Handle::stdout());
for candidate in candidates {
    let handle = try!(Handle::from_path(candidate));
    if stdout_handle == handle {
        println!("{:?} is stdout!", candidate);
    } else {
        println!("{:?} is NOT stdout!", candidate);
    }
}
# Ok(()) } example().unwrap();
```

See `examples/is_stderr.rs` for a runnable example. Compare the output of
`cargo run is_stderr 2> examples/stderr` and `cargo run is_stderr`.
*/

#![deny(missing_docs)]

#[cfg(windows)]
extern crate kernel32;
#[cfg(windows)]
extern crate winapi;

use std::fs::File;
use std::io;
use std::path::Path;

#[cfg(any(target_os = "redox", unix))]
use unix as imp;
#[cfg(windows)]
use win as imp;

#[cfg(any(target_os = "redox", unix))]
mod unix;
#[cfg(windows)]
mod win;

/// A handle to a file that can be tested for equality with other handles.
///
/// If two files are the same, then any two handles of those files will compare
/// equal. If two files are not the same, then any two handles of those files
/// will compare not-equal.
///
/// A handle consumes an open file resource as long as it exists.
///
/// Note that it's possible for comparing two handles to produce a false
/// positive on some platforms. Namely, two handles can compare equal even if
/// the two handles *don't* point to the same file.
#[derive(Debug, Eq, PartialEq)]
pub struct Handle(imp::Handle);

impl Handle {
    /// Construct a handle from a path.
    ///
    /// Note that the underlying `File` is opened in read-only mode on all
    /// platforms.
    pub fn from_path<P: AsRef<Path>>(p: P) -> io::Result<Handle> {
        imp::Handle::from_path(p).map(Handle)
    }

    /// Construct a handle from a file.
    pub fn from_file(file: File) -> io::Result<Handle> {
        imp::Handle::from_file(file).map(Handle)
    }

    /// Construct a handle from stdin.
    pub fn stdin() -> io::Result<Handle> {
        imp::Handle::stdin().map(Handle)
    }

    /// Construct a handle from stdout.
    pub fn stdout() -> io::Result<Handle> {
        imp::Handle::stdout().map(Handle)
    }

    /// Construct a handle from stderr.
    pub fn stderr() -> io::Result<Handle> {
        imp::Handle::stderr().map(Handle)
    }

    /// Return a reference to the underlying file.
    pub fn as_file(&self) -> &File {
        self.0.as_file()
    }

    /// Return a mutable reference to the underlying file.
    pub fn as_file_mut(&mut self) -> &mut File {
        self.0.as_file_mut()
    }

    /// Return the underlying device number of this handle.
    #[cfg(any(target_os = "redox", unix))]
    pub fn dev(&self) -> u64 {
        self.0.dev()
    }

    /// Return the underlying inode number of this handle.
    #[cfg(any(target_os = "redox", unix))]
    pub fn ino(&self) -> u64 {
        self.0.ino()
    }
}

/// Returns true if the two file paths may correspond to the same file.
///
/// If there was a problem accessing either file path, then an error is
/// returned.
///
/// Note that it's possible for this to produce a false positive on some
/// platforms. Namely, this can return true even if the two file paths *don't*
/// resolve to the same file.
///
/// # Example
///
/// ```rust,no_run
/// use same_file::is_same_file;
///
/// assert!(is_same_file("./foo", "././foo").unwrap_or(false));
/// ```
pub fn is_same_file<P, Q>(
    path1: P,
    path2: Q,
) -> io::Result<bool> where P: AsRef<Path>, Q: AsRef<Path> {
    Ok(try!(Handle::from_path(path1)) == try!(Handle::from_path(path2)))
}

#[cfg(test)]
mod tests {
    extern crate rand;

    use std::env;
    use std::fs::{self, File};
    use std::io;
    use std::path::{Path, PathBuf};

    use self::rand::Rng;

    use super::is_same_file;

    struct TempDir(PathBuf);

    impl TempDir {
        fn path<'a>(&'a self) -> &'a Path {
            &self.0
        }
    }

    impl Drop for TempDir {
        fn drop(&mut self) {
            fs::remove_dir_all(&self.0).unwrap();
        }
    }

    fn tmpdir() -> TempDir {
        let p = env::temp_dir();
        let mut r = self::rand::thread_rng();
        let ret = p.join(&format!("rust-{}", r.next_u32()));
        fs::create_dir(&ret).unwrap();
        TempDir(ret)
    }

    #[cfg(unix)]
    pub fn soft_link_dir<P: AsRef<Path>, Q: AsRef<Path>>(
        src: P,
        dst: Q,
    ) -> io::Result<()> {
        use std::os::unix::fs::symlink;
        symlink(src, dst)
    }

    #[cfg(unix)]
    pub fn soft_link_file<P: AsRef<Path>, Q: AsRef<Path>>(
        src: P,
        dst: Q,
    ) -> io::Result<()> {
        soft_link_dir(src, dst)
    }

    #[cfg(windows)]
    pub fn soft_link_dir<P: AsRef<Path>, Q: AsRef<Path>>(
        src: P,
        dst: Q,
    ) -> io::Result<()> {
        use std::os::windows::fs::symlink_dir;
        symlink_dir(src, dst)
    }

    #[cfg(windows)]
    pub fn soft_link_file<P: AsRef<Path>, Q: AsRef<Path>>(
        src: P,
        dst: Q,
    ) -> io::Result<()> {
        use std::os::windows::fs::symlink_file;
        symlink_file(src, dst)
    }

    // These tests are rather uninteresting. The really interesting tests
    // would stress the edge cases. On Unix, this might be comparing two files
    // on different mount points with the same inode number. On Windows, this
    // might be comparing two files whose file indices are the same on file
    // systems where such things aren't guaranteed to be unique.
    //
    // Alas, I don't know how to create those environmental conditions. ---AG

    #[test]
    fn same_file_trivial() {
        let tdir = tmpdir();
        let dir = tdir.path();

        File::create(dir.join("a")).unwrap();
        assert!(is_same_file(dir.join("a"), dir.join("a")).unwrap());
    }

    #[test]
    fn same_dir_trivial() {
        let tdir = tmpdir();
        let dir = tdir.path();

        fs::create_dir(dir.join("a")).unwrap();
        assert!(is_same_file(dir.join("a"), dir.join("a")).unwrap());
    }

    #[test]
    fn not_same_file_trivial() {
        let tdir = tmpdir();
        let dir = tdir.path();

        File::create(dir.join("a")).unwrap();
        File::create(dir.join("b")).unwrap();
        assert!(!is_same_file(dir.join("a"), dir.join("b")).unwrap());
    }

    #[test]
    fn not_same_dir_trivial() {
        let tdir = tmpdir();
        let dir = tdir.path();

        fs::create_dir(dir.join("a")).unwrap();
        fs::create_dir(dir.join("b")).unwrap();
        assert!(!is_same_file(dir.join("a"), dir.join("b")).unwrap());
    }

    #[test]
    fn same_file_hard() {
        let tdir = tmpdir();
        let dir = tdir.path();

        File::create(dir.join("a")).unwrap();
        fs::hard_link(dir.join("a"), dir.join("alink")).unwrap();
        assert!(is_same_file(dir.join("a"), dir.join("alink")).unwrap());
    }

    #[test]
    fn same_file_soft() {
        let tdir = tmpdir();
        let dir = tdir.path();

        File::create(dir.join("a")).unwrap();
        soft_link_file(dir.join("a"), dir.join("alink")).unwrap();
        assert!(is_same_file(dir.join("a"), dir.join("alink")).unwrap());
    }

    #[test]
    fn same_dir_soft() {
        let tdir = tmpdir();
        let dir = tdir.path();

        fs::create_dir(dir.join("a")).unwrap();
        soft_link_dir(dir.join("a"), dir.join("alink")).unwrap();
        assert!(is_same_file(dir.join("a"), dir.join("alink")).unwrap());
    }
}
