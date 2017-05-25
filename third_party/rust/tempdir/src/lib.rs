// Copyright 2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![doc(html_logo_url = "https://www.rust-lang.org/logos/rust-logo-128x128-blk-v2.png",
       html_favicon_url = "https://www.rust-lang.org/favicon.ico",
       html_root_url = "https://doc.rust-lang.org/tempdir/")]
#![cfg_attr(test, deny(warnings))]

//! Temporary directories of files.
//!
//! The [`TempDir`] type creates a directory on the file system that
//! is deleted once it goes out of scope. At construction, the
//! `TempDir` creates a new directory with a randomly generated name
//! and a prefix of your choosing.
//!
//! [`TempDir`]: struct.TempDir.html
//! [`std::env::temp_dir()`]: https://doc.rust-lang.org/std/env/fn.temp_dir.html
//!
//! # Examples
//!
//! ```
//! extern crate tempdir;
//!
//! use std::fs::File;
//! use std::io::Write;
//! use tempdir::TempDir;
//!
//! fn main() {
//!     // Create a directory inside of `std::env::temp_dir()`, named with
//!     // the prefix "example".
//!     let tmp_dir = TempDir::new("example").expect("create temp dir");
//!     let file_path = tmp_dir.path().join("my-temporary-note.txt");
//!     let mut tmp_file = File::create(file_path).expect("create temp file");
//!     writeln!(tmp_file, "Brian was here. Briefly.").expect("write temp file");
//!
//!     // By closing the `TempDir` explicitly, we can check that it has
//!     // been deleted successfully. If we don't close it explicitly,
//!     // the directory will still be deleted when `tmp_dir` goes out
//!     // of scope, but we won't know whether deleting the directory
//!     // succeeded.
//!     drop(tmp_file);
//!     tmp_dir.close().expect("delete temp dir");
//! }
//! ```

extern crate rand;

use std::env;
use std::io::{self, Error, ErrorKind};
use std::fmt;
use std::fs;
use std::path::{self, PathBuf, Path};
use rand::{thread_rng, Rng};

/// A directory in the filesystem that is automatically deleted when
/// it goes out of scope.
///
/// The [`TempDir`] type creates a directory on the file system that
/// is deleted once it goes out of scope. At construction, the
/// `TempDir` creates a new directory with a randomly generated name,
/// and with a prefix of your choosing.
///
/// The default constructor, [`TempDir::new`], creates directories in
/// the location returned by [`std::env::temp_dir()`], but `TempDir`
/// can be configured to manage a temporary directory in any location
/// by constructing with [`TempDir::new_in`].
///
/// After creating a `TempDir`, work with the file system by doing
/// standard [`std::fs`] file system operations on its [`Path`],
/// which can be retrieved with [`TempDir::path`]. Once the `TempDir`
/// value is dropped, the directory at the path will be deleted, along
/// with any files and directories it contains. It is your responsibility
/// to ensure that no further file system operations are attempted
/// inside the temporary directory once it has been deleted.
///
/// Various platform-specific conditions may cause `TempDir` to fail
/// to delete the underlying directory. It's important to ensure that
/// handles (like [`File`] and [`ReadDir`]) to files inside the
/// directory are dropped before the `TempDir` goes out of scope. The
/// `TempDir` destructor will silently ignore any errors in deleting
/// the directory; to instead handle errors call [`TempDir::close`].
///
/// Note that if the program exits before the `TempDir` destructor is
/// run, such as via [`std::process::exit`], by segfaulting, or by
/// receiving a signal like `SIGINT`, then the temporary directory
/// will not be deleted.
/// 
/// [`File`]: http://doc.rust-lang.org/std/fs/struct.File.html
/// [`Path`]: http://doc.rust-lang.org/std/path/struct.Path.html
/// [`ReadDir`]: http://doc.rust-lang.org/std/fs/struct.ReadDir.html
/// [`TempDir::close`]: struct.TempDir.html#method.close
/// [`TempDir::new`]: struct.TempDir.html#method.new
/// [`TempDir::new_in`]: struct.TempDir.html#method.new_in
/// [`TempDir::path`]: struct.TempDir.html#method.path
/// [`TempDir`]: struct.TempDir.html
/// [`std::env::temp_dir()`]: https://doc.rust-lang.org/std/env/fn.temp_dir.html
/// [`std::fs`]: http://doc.rust-lang.org/std/fs/index.html
/// [`std::process::exit`]: http://doc.rust-lang.org/std/process/fn.exit.html
pub struct TempDir {
    path: Option<PathBuf>,
}

// How many times should we (re)try finding an unused random name? It should be
// enough that an attacker will run out of luck before we run out of patience.
const NUM_RETRIES: u32 = 1 << 31;
// How many characters should we include in a random file name? It needs to
// be enough to dissuade an attacker from trying to preemptively create names
// of that length, but not so huge that we unnecessarily drain the random number
// generator of entropy.
const NUM_RAND_CHARS: usize = 12;

impl TempDir {
    /// Attempts to make a temporary directory inside of `env::temp_dir()` whose
    /// name will have the prefix, `prefix`. The directory and
    /// everything inside it will be automatically deleted once the
    /// returned `TempDir` is destroyed.
    ///
    /// # Errors
    ///
    /// If the directory can not be created, `Err` is returned.
    ///
    /// # Examples
    ///
    /// ```
    /// use std::fs::File;
    /// use std::io::Write;
    /// use tempdir::TempDir;
    ///
    /// // Create a directory inside of `std::env::temp_dir()`, named with
    /// // the prefix, "example".
    /// let tmp_dir = TempDir::new("example").expect("create temp dir");
    /// let file_path = tmp_dir.path().join("my-temporary-note.txt");
    /// let mut tmp_file = File::create(file_path).expect("create temp file");
    /// writeln!(tmp_file, "Brian was here. Briefly.").expect("write temp file");
    ///
    /// // `tmp_dir` goes out of scope, the directory as well as
    /// // `tmp_file` will be deleted here.
    /// ```
    pub fn new(prefix: &str) -> io::Result<TempDir> {
        TempDir::new_in(&env::temp_dir(), prefix)
    }

    /// Attempts to make a temporary directory inside of `tmpdir`
    /// whose name will have the prefix `prefix`. The directory and
    /// everything inside it will be automatically deleted once the
    /// returned `TempDir` is destroyed.
    ///
    /// # Errors
    ///
    /// If the directory can not be created, `Err` is returned.
    ///
    /// # Examples
    ///
    /// ```
    /// use std::fs::{self, File};
    /// use std::io::Write;
    /// use tempdir::TempDir;
    ///
    /// // Create a directory inside of the current directory, named with
    /// // the prefix, "example".
    /// let tmp_dir = TempDir::new_in(".", "example").expect("create temp dir");
    /// let file_path = tmp_dir.path().join("my-temporary-note.txt");
    /// let mut tmp_file = File::create(file_path).expect("create temp file");
    /// writeln!(tmp_file, "Brian was here. Briefly.").expect("write temp file");
    /// ```
    pub fn new_in<P: AsRef<Path>>(tmpdir: P, prefix: &str) -> io::Result<TempDir> {
        let storage;
        let mut tmpdir = tmpdir.as_ref();
        if !tmpdir.is_absolute() {
            let cur_dir = try!(env::current_dir());
            storage = cur_dir.join(tmpdir);
            tmpdir = &storage;
            // return TempDir::new_in(&cur_dir.join(tmpdir), prefix);
        }

        let mut rng = thread_rng();
        for _ in 0..NUM_RETRIES {
            let suffix: String = rng.gen_ascii_chars().take(NUM_RAND_CHARS).collect();
            let leaf = if prefix.len() > 0 {
                format!("{}.{}", prefix, suffix)
            } else {
                // If we're given an empty string for a prefix, then creating a
                // directory starting with "." would lead to it being
                // semi-invisible on some systems.
                suffix
            };
            let path = tmpdir.join(&leaf);
            match fs::create_dir(&path) {
                Ok(_) => return Ok(TempDir { path: Some(path) }),
                Err(ref e) if e.kind() == ErrorKind::AlreadyExists => {}
                Err(e) => return Err(e),
            }
        }

        Err(Error::new(ErrorKind::AlreadyExists,
                       "too many temporary directories already exist"))
    }

    /// Accesses the [`Path`] to the temporary directory.
    ///
    /// [`Path`]: http://doc.rust-lang.org/std/path/struct.Path.html
    ///
    /// # Examples
    ///
    /// ```
    /// use tempdir::TempDir;
    ///
    /// let tmp_path;
    ///
    /// {
    ///    let tmp_dir = TempDir::new("example").expect("create temp dir");
    ///    tmp_path = tmp_dir.path().to_owned();
    ///
    ///    // Check that the temp directory actually exists.
    ///    assert!(tmp_path.exists());
    ///
    ///    // End of `tmp_dir` scope, directory will be deleted
    /// }
    ///
    /// // Temp directory should be deleted by now
    /// assert_eq!(tmp_path.exists(), false);
    /// ```
    pub fn path(&self) -> &path::Path {
        self.path.as_ref().unwrap()
    }

    /// Unwraps the [`Path`] contained in the `TempDir` and
    /// returns it. This destroys the `TempDir` without deleting the
    /// directory represented by the returned `Path`.
    ///
    /// [`Path`]: http://doc.rust-lang.org/std/path/struct.Path.html
    ///
    /// # Examples
    ///
    /// ```
    /// use std::fs;
    /// use tempdir::TempDir;
    ///
    /// let tmp_dir = TempDir::new("example").expect("create temp dir");
    ///
    /// // Convert `tmp_dir` into a `Path`, destroying the `TempDir`
    /// // without deleting the directory.
    /// let tmp_path = tmp_dir.into_path();
    ///
    /// // Delete the temporary directory ourselves.
    /// fs::remove_dir_all(tmp_path).expect("remove temp dir");
    /// ```
    pub fn into_path(mut self) -> PathBuf {
        self.path.take().unwrap()
    }

    /// Closes and removes the temporary directory, returing a `Result`.
    ///
    /// Although `TempDir` removes the directory on drop, in the destructor
    /// any errors are ignored. To detect errors cleaning up the temporary
    /// directory, call `close` instead.
    ///
    /// # Errors
    ///
    /// This function may return a variety of [`std::io::Error`]s that result from deleting
    /// the files and directories contained with the temporary directory,
    /// as well as from deleting the temporary directory itself. These errors
    /// may be platform specific.
    ///
    /// [`std::io::Error`]: http://doc.rust-lang.org/std/io/struct.Error.html
    ///
    /// # Examples
    ///
    /// ```
    /// use std::fs::File;
    /// use std::io::Write;
    /// use tempdir::TempDir;
    ///
    /// // Create a directory inside of `std::env::temp_dir()`, named with
    /// // the prefix, "example".
    /// let tmp_dir = TempDir::new("example").expect("create temp dir");
    /// let file_path = tmp_dir.path().join("my-temporary-note.txt");
    /// let mut tmp_file = File::create(file_path).expect("create temp file");
    /// writeln!(tmp_file, "Brian was here. Briefly.").expect("write temp file");
    ///
    /// // By closing the `TempDir` explicitly we can check that it has
    /// // been deleted successfully. If we don't close it explicitly,
    /// // the directory will still be deleted when `tmp_dir` goes out
    /// // of scope, but we won't know whether deleting the directory
    /// // succeeded.
    /// drop(tmp_file);
    /// tmp_dir.close().expect("delete temp dir");
    /// ```
    pub fn close(mut self) -> io::Result<()> {
        self.cleanup_dir()
    }

    fn cleanup_dir(&mut self) -> io::Result<()> {
        match self.path {
            Some(ref p) => fs::remove_dir_all(p),
            None => Ok(()),
        }
    }
}

impl AsRef<Path> for TempDir {
    fn as_ref(&self) -> &Path {
        self.path()
    }
}

impl fmt::Debug for TempDir {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("TempDir")
            .field("path", &self.path())
            .finish()
    }
}

impl Drop for TempDir {
    fn drop(&mut self) {
        let _ = self.cleanup_dir();
    }
}

// the tests for this module need to change the path using change_dir,
// and this doesn't play nicely with other tests so these unit tests are located
// in src/test/run-pass/tempfile.rs
