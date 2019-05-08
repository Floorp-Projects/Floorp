use std;
use std::env;
use std::error;
use std::ffi::OsStr;
use std::fmt;
use std::fs::{self, File};
use std::io::{self, Read, Seek, SeekFrom, Write};
use std::mem;
use std::ops::Deref;
use std::path::{Path, PathBuf};

use Builder;

mod imp;

/// Create a new temporary file.
///
/// The file will be created in the location returned by [`std::env::temp_dir()`].
///
/// # Security
///
/// This variant is secure/reliable in the presence of a pathological temporary file cleaner.
///
/// # Resource Leaking
///
/// The temporary file will be automatically removed by the OS when the last handle to it is closed.
/// This doesn't rely on Rust destructors being run, so will (almost) never fail to clean up the temporary file.
///
/// # Errors
///
/// If the file can not be created, `Err` is returned.
///
/// # Examples
///
/// ```
/// # extern crate tempfile;
/// use tempfile::tempfile;
/// use std::io::{self, Write};
///
/// # fn main() {
/// #     if let Err(_) = run() {
/// #         ::std::process::exit(1);
/// #     }
/// # }
/// # fn run() -> Result<(), io::Error> {
/// // Create a file inside of `std::env::temp_dir()`.
/// let mut file = tempfile()?;
///
/// writeln!(file, "Brian was here. Briefly.")?;
/// # Ok(())
/// # }
/// ```
///
/// [`std::env::temp_dir()`]: https://doc.rust-lang.org/std/env/fn.temp_dir.html
pub fn tempfile() -> io::Result<File> {
    tempfile_in(&env::temp_dir())
}

/// Create a new temporary file in the specified directory.
///
/// # Security
///
/// This variant is secure/reliable in the presence of a pathological temporary file cleaner.
/// If the temporary file isn't created in [`std::env::temp_dir()`] then temporary file cleaners aren't an issue.
///
/// # Resource Leaking
///
/// The temporary file will be automatically removed by the OS when the last handle to it is closed.
/// This doesn't rely on Rust destructors being run, so will (almost) never fail to clean up the temporary file.
///
/// # Errors
///
/// If the file can not be created, `Err` is returned.
///
/// # Examples
///
/// ```
/// # extern crate tempfile;
/// use tempfile::tempfile_in;
/// use std::io::{self, Write};
///
/// # fn main() {
/// #     if let Err(_) = run() {
/// #         ::std::process::exit(1);
/// #     }
/// # }
/// # fn run() -> Result<(), io::Error> {
/// // Create a file inside of the current working directory
/// let mut file = tempfile_in("./")?;
///
/// writeln!(file, "Brian was here. Briefly.")?;
/// # Ok(())
/// # }
/// ```
///
/// [`std::env::temp_dir()`]: https://doc.rust-lang.org/std/env/fn.temp_dir.html
pub fn tempfile_in<P: AsRef<Path>>(dir: P) -> io::Result<File> {
    imp::create(dir.as_ref())
}

/// Error returned when persisting a temporary file path fails.
#[derive(Debug)]
pub struct PathPersistError {
    /// The underlying IO error.
    pub error: io::Error,
    /// The temporary file path that couldn't be persisted.
    pub path: TempPath,
}

impl From<PathPersistError> for io::Error {
    #[inline]
    fn from(error: PathPersistError) -> io::Error {
        error.error
    }
}

impl From<PathPersistError> for TempPath {
    #[inline]
    fn from(error: PathPersistError) -> TempPath {
        error.path
    }
}

impl fmt::Display for PathPersistError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "failed to persist temporary file path: {}", self.error)
    }
}

impl error::Error for PathPersistError {
    fn description(&self) -> &str {
        "failed to persist temporary file path"
    }

    fn cause(&self) -> Option<&error::Error> {
        Some(&self.error)
    }
}

/// A path to a named temporary file without an open file handle.
///
/// This is useful when the temporary file needs to be used by a child process,
/// for example.
///
/// When dropped, the temporary file is deleted.
pub struct TempPath {
    path: PathBuf,
}

impl TempPath {
    /// Close and remove the temporary file.
    ///
    /// Use this if you want to detect errors in deleting the file.
    ///
    /// # Errors
    ///
    /// If the file cannot be deleted, `Err` is returned.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # extern crate tempfile;
    /// # use std::io;
    /// use tempfile::NamedTempFile;
    ///
    /// # fn main() {
    /// #     if let Err(_) = run() {
    /// #         ::std::process::exit(1);
    /// #     }
    /// # }
    /// # fn run() -> Result<(), io::Error> {
    /// let file = NamedTempFile::new()?;
    ///
    /// // Close the file, but keep the path to it around.
    /// let path = file.into_temp_path();
    ///
    /// // By closing the `TempPath` explicitly, we can check that it has
    /// // been deleted successfully. If we don't close it explicitly, the
    /// // file will still be deleted when `file` goes out of scope, but we
    /// // won't know whether deleting the file succeeded.
    /// path.close()?;
    /// # Ok(())
    /// # }
    /// ```
    pub fn close(mut self) -> io::Result<()> {
        let result = fs::remove_file(&self.path);
        mem::replace(&mut self.path, PathBuf::new());
        mem::forget(self);
        result
    }

    /// Persist the temporary file at the target path.
    ///
    /// If a file exists at the target path, persist will atomically replace it.
    /// If this method fails, it will return `self` in the resulting
    /// [`PathPersistError`].
    ///
    /// Note: Temporary files cannot be persisted across filesystems.
    ///
    /// # Security
    ///
    /// Only use this method if you're positive that a temporary file cleaner
    /// won't have deleted your file. Otherwise, you might end up persisting an
    /// attacker controlled file.
    ///
    /// # Errors
    ///
    /// If the file cannot be moved to the new location, `Err` is returned.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use std::io::{self, Write};
    /// # extern crate tempfile;
    /// use tempfile::NamedTempFile;
    ///
    /// # fn main() {
    /// #     if let Err(_) = run() {
    /// #         ::std::process::exit(1);
    /// #     }
    /// # }
    /// # fn run() -> Result<(), io::Error> {
    /// let mut file = NamedTempFile::new()?;
    /// writeln!(file, "Brian was here. Briefly.")?;
    ///
    /// let path = file.into_temp_path();
    /// path.persist("./saved_file.txt")?;
    /// # Ok(())
    /// # }
    /// ```
    ///
    /// [`PathPersistError`]: struct.PathPersistError.html
    pub fn persist<P: AsRef<Path>>(mut self, new_path: P) -> Result<(), PathPersistError> {
        match imp::persist(&self.path, new_path.as_ref(), true) {
            Ok(_) => {
                // Don't drop `self`. We don't want to try deleting the old
                // temporary file path. (It'll fail, but the failure is never
                // seen.)
                mem::replace(&mut self.path, PathBuf::new());
                mem::forget(self);
                Ok(())
            }
            Err(e) => Err(PathPersistError {
                error: e,
                path: self,
            }),
        }
    }

    /// Persist the temporary file at the target path iff no file exists there.
    ///
    /// If a file exists at the target path, fail. If this method fails, it will
    /// return `self` in the resulting [`PathPersistError`].
    ///
    /// Note: Temporary files cannot be persisted across filesystems. Also Note:
    /// This method is not atomic. It can leave the original link to the
    /// temporary file behind.
    ///
    /// # Security
    ///
    /// Only use this method if you're positive that a temporary file cleaner
    /// won't have deleted your file. Otherwise, you might end up persisting an
    /// attacker controlled file.
    ///
    /// # Errors
    ///
    /// If the file cannot be moved to the new location or a file already exists
    /// there, `Err` is returned.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use std::io::{self, Write};
    /// # extern crate tempfile;
    /// use tempfile::NamedTempFile;
    ///
    /// # fn main() {
    /// #     if let Err(_) = run() {
    /// #         ::std::process::exit(1);
    /// #     }
    /// # }
    /// # fn run() -> Result<(), io::Error> {
    /// let mut file = NamedTempFile::new()?;
    /// writeln!(file, "Brian was here. Briefly.")?;
    ///
    /// let path = file.into_temp_path();
    /// path.persist_noclobber("./saved_file.txt")?;
    /// # Ok(())
    /// # }
    /// ```
    ///
    /// [`PathPersistError`]: struct.PathPersistError.html
    pub fn persist_noclobber<P: AsRef<Path>>(
        mut self,
        new_path: P,
    ) -> Result<(), PathPersistError> {
        match imp::persist(&self.path, new_path.as_ref(), false) {
            Ok(_) => {
                // Don't drop `self`. We don't want to try deleting the old
                // temporary file path. (It'll fail, but the failure is never
                // seen.)
                mem::replace(&mut self.path, PathBuf::new());
                mem::forget(self);
                Ok(())
            }
            Err(e) => Err(PathPersistError {
                error: e,
                path: self,
            }),
        }
    }
}

impl fmt::Debug for TempPath {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.path.fmt(f)
    }
}

impl Drop for TempPath {
    fn drop(&mut self) {
        let _ = fs::remove_file(&self.path);
    }
}

impl Deref for TempPath {
    type Target = Path;

    fn deref(&self) -> &Path {
        &self.path
    }
}

impl AsRef<Path> for TempPath {
    fn as_ref(&self) -> &Path {
        &self.path
    }
}

impl AsRef<OsStr> for TempPath {
    fn as_ref(&self) -> &OsStr {
        self.path.as_os_str()
    }
}

/// A named temporary file.
///
/// The default constructor, [`NamedTempFile::new()`], creates files in
/// the location returned by [`std::env::temp_dir()`], but `NamedTempFile`
/// can be configured to manage a temporary file in any location
/// by constructing with [`NamedTempFile::new_in()`].
///
/// # Security
///
/// This variant is *NOT* secure/reliable in the presence of a pathological temporary file cleaner.
///
/// # Resource Leaking
///
/// If the program exits before the `NamedTempFile` destructor is
/// run, such as via [`std::process::exit()`], by segfaulting, or by
/// receiving a signal like `SIGINT`, then the temporary file
/// will not be deleted.
///
/// Use the [`tempfile()`] function unless you absolutely need a named file.
///
/// [`tempfile()`]: fn.tempfile.html
/// [`NamedTempFile::new()`]: #method.new
/// [`NamedTempFile::new_in()`]: #method.new_in
/// [`std::env::temp_dir()`]: https://doc.rust-lang.org/std/env/fn.temp_dir.html
/// [`std::process::exit()`]: http://doc.rust-lang.org/std/process/fn.exit.html
pub struct NamedTempFile {
    path: TempPath,
    file: File,
}

impl fmt::Debug for NamedTempFile {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "NamedTempFile({:?})", self.path)
    }
}

impl AsRef<Path> for NamedTempFile {
    #[inline]
    fn as_ref(&self) -> &Path {
        self.path()
    }
}

/// Error returned when persisting a temporary file fails.
#[derive(Debug)]
pub struct PersistError {
    /// The underlying IO error.
    pub error: io::Error,
    /// The temporary file that couldn't be persisted.
    pub file: NamedTempFile,
}

impl From<PersistError> for io::Error {
    #[inline]
    fn from(error: PersistError) -> io::Error {
        error.error
    }
}

impl From<PersistError> for NamedTempFile {
    #[inline]
    fn from(error: PersistError) -> NamedTempFile {
        error.file
    }
}

impl fmt::Display for PersistError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "failed to persist temporary file: {}", self.error)
    }
}

impl error::Error for PersistError {
    fn description(&self) -> &str {
        "failed to persist temporary file"
    }
    fn cause(&self) -> Option<&error::Error> {
        Some(&self.error)
    }
}

impl NamedTempFile {
    /// Create a new named temporary file.
    ///
    /// See [`Builder`] for more configuration.
    ///
    /// # Security
    ///
    /// This will create a temporary file in the default temporary file
    /// directory (platform dependent). These directories are often patrolled by temporary file
    /// cleaners so only use this method if you're *positive* that the temporary file cleaner won't
    /// delete your file.
    ///
    /// Reasons to use this method:
    ///
    ///   1. The file has a short lifetime and your temporary file cleaner is
    ///      sane (doesn't delete recently accessed files).
    ///
    ///   2. You trust every user on your system (i.e. you are the only user).
    ///
    ///   3. You have disabled your system's temporary file cleaner or verified
    ///      that your system doesn't have a temporary file cleaner.
    ///
    /// Reasons not to use this method:
    ///
    ///   1. You'll fix it later. No you won't.
    ///
    ///   2. You don't care about the security of the temporary file. If none of
    ///      the "reasons to use this method" apply, referring to a temporary
    ///      file by name may allow an attacker to create/overwrite your
    ///      non-temporary files. There are exceptions but if you don't already
    ///      know them, don't use this method.
    ///
    /// # Errors
    ///
    /// If the file can not be created, `Err` is returned.
    ///
    /// # Examples
    ///
    /// Create a named temporary file and write some data to it:
    ///
    /// ```no_run
    /// # use std::io::{self, Write};
    /// # extern crate tempfile;
    /// use tempfile::NamedTempFile;
    ///
    /// # fn main() {
    /// #     if let Err(_) = run() {
    /// #         ::std::process::exit(1);
    /// #     }
    /// # }
    /// # fn run() -> Result<(), ::std::io::Error> {
    /// let mut file = NamedTempFile::new()?;
    ///
    /// writeln!(file, "Brian was here. Briefly.")?;
    /// # Ok(())
    /// # }
    /// ```
    ///
    /// [`Builder`]: struct.Builder.html
    pub fn new() -> io::Result<NamedTempFile> {
        Builder::new().tempfile()
    }

    /// Create a new named temporary file in the specified directory.
    ///
    /// See [`NamedTempFile::new()`] for details.
    ///
    /// [`NamedTempFile::new()`]: #method.new_in
    pub fn new_in<P: AsRef<Path>>(dir: P) -> io::Result<NamedTempFile> {
        Builder::new().tempfile_in(dir)
    }

    /// Get the temporary file's path.
    ///
    /// # Security
    ///
    /// Only use this method if you're positive that a
    /// temporary file cleaner won't have deleted your file. Otherwise, the path
    /// returned by this method may refer to an attacker controlled file.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use std::io::{self, Write};
    /// # extern crate tempfile;
    /// use tempfile::NamedTempFile;
    ///
    /// # fn main() {
    /// #     if let Err(_) = run() {
    /// #         ::std::process::exit(1);
    /// #     }
    /// # }
    /// # fn run() -> Result<(), ::std::io::Error> {
    /// let file = NamedTempFile::new()?;
    ///
    /// println!("{:?}", file.path());
    /// # Ok(())
    /// # }
    /// ```
    #[inline]
    pub fn path(&self) -> &Path {
        &self.path
    }

    /// Close and remove the temporary file.
    ///
    /// Use this if you want to detect errors in deleting the file.
    ///
    /// # Errors
    ///
    /// If the file cannot be deleted, `Err` is returned.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # extern crate tempfile;
    /// # use std::io;
    /// use tempfile::NamedTempFile;
    ///
    /// # fn main() {
    /// #     if let Err(_) = run() {
    /// #         ::std::process::exit(1);
    /// #     }
    /// # }
    /// # fn run() -> Result<(), io::Error> {
    /// let file = NamedTempFile::new()?;
    ///
    /// // By closing the `NamedTempFile` explicitly, we can check that it has
    /// // been deleted successfully. If we don't close it explicitly,
    /// // the file will still be deleted when `file` goes out
    /// // of scope, but we won't know whether deleting the file
    /// // succeeded.
    /// file.close()?;
    /// # Ok(())
    /// # }
    /// ```
    pub fn close(self) -> io::Result<()> {
        let NamedTempFile { path, .. } = self;
        path.close()
    }

    /// Persist the temporary file at the target path.
    ///
    /// If a file exists at the target path, persist will atomically replace it.
    /// If this method fails, it will return `self` in the resulting
    /// [`PersistError`].
    ///
    /// Note: Temporary files cannot be persisted across filesystems.
    ///
    /// # Security
    ///
    /// Only use this method if you're positive that a
    /// temporary file cleaner won't have deleted your file. Otherwise, you
    /// might end up persisting an attacker controlled file.
    ///
    /// # Errors
    ///
    /// If the file cannot be moved to the new location, `Err` is returned.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use std::io::{self, Write};
    /// # extern crate tempfile;
    /// use tempfile::NamedTempFile;
    ///
    /// # fn main() {
    /// #     if let Err(_) = run() {
    /// #         ::std::process::exit(1);
    /// #     }
    /// # }
    /// # fn run() -> Result<(), io::Error> {
    /// let file = NamedTempFile::new()?;
    ///
    /// let mut persisted_file = file.persist("./saved_file.txt")?;
    /// writeln!(persisted_file, "Brian was here. Briefly.")?;
    /// # Ok(())
    /// # }
    /// ```
    ///
    /// [`PersistError`]: struct.PersistError.html
    pub fn persist<P: AsRef<Path>>(self, new_path: P) -> Result<File, PersistError> {
        let NamedTempFile { path, file } = self;
        match path.persist(new_path) {
            Ok(_) => Ok(file),
            Err(err) => {
                let PathPersistError { error, path } = err;
                Err(PersistError {
                    file: NamedTempFile { path, file },
                    error,
                })
            }
        }
    }

    /// Persist the temporary file at the target path iff no file exists there.
    ///
    /// If a file exists at the target path, fail. If this method fails, it will
    /// return `self` in the resulting PersistError.
    ///
    /// Note: Temporary files cannot be persisted across filesystems. Also Note:
    /// This method is not atomic. It can leave the original link to the
    /// temporary file behind.
    ///
    /// # Security
    ///
    /// Only use this method if you're positive that a
    /// temporary file cleaner won't have deleted your file. Otherwise, you
    /// might end up persisting an attacker controlled file.
    ///
    /// # Errors
    ///
    /// If the file cannot be moved to the new location or a file already exists there,
    /// `Err` is returned.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use std::io::{self, Write};
    /// # extern crate tempfile;
    /// use tempfile::NamedTempFile;
    ///
    /// # fn main() {
    /// #     if let Err(_) = run() {
    /// #         ::std::process::exit(1);
    /// #     }
    /// # }
    /// # fn run() -> Result<(), io::Error> {
    /// let file = NamedTempFile::new()?;
    ///
    /// let mut persisted_file = file.persist_noclobber("./saved_file.txt")?;
    /// writeln!(persisted_file, "Brian was here. Briefly.")?;
    /// # Ok(())
    /// # }
    /// ```
    pub fn persist_noclobber<P: AsRef<Path>>(self, new_path: P) -> Result<File, PersistError> {
        let NamedTempFile { path, file } = self;
        match path.persist_noclobber(new_path) {
            Ok(_) => Ok(file),
            Err(err) => {
                let PathPersistError { error, path } = err;
                Err(PersistError {
                    file: NamedTempFile { path, file },
                    error,
                })
            }
        }
    }

    /// Reopen the temporary file.
    ///
    /// This function is useful when you need multiple independent handles to
    /// the same file. It's perfectly fine to drop the original `NamedTempFile`
    /// while holding on to `File`s returned by this function; the `File`s will
    /// remain usable. However, they may not be nameable.
    ///
    /// # Errors
    ///
    /// If the file cannot be reopened, `Err` is returned.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use std::io;
    /// # extern crate tempfile;
    /// use tempfile::NamedTempFile;
    ///
    /// # fn main() {
    /// #     if let Err(_) = run() {
    /// #         ::std::process::exit(1);
    /// #     }
    /// # }
    /// # fn run() -> Result<(), io::Error> {
    /// let file = NamedTempFile::new()?;
    ///
    /// let another_handle = file.reopen()?;
    /// # Ok(())
    /// # }
    /// ```
    pub fn reopen(&self) -> io::Result<File> {
        imp::reopen(self.as_file(), NamedTempFile::path(self))
    }

    /// Get a reference to the underlying file.
    pub fn as_file(&self) -> &File {
        &self.file
    }

    /// Get a mutable reference to the underlying file.
    pub fn as_file_mut(&mut self) -> &mut File {
        &mut self.file
    }

    /// Convert the temporary file into a `std::fs::File`.
    ///
    /// The inner file will be deleted.
    pub fn into_file(self) -> File {
        self.file
    }

    /// Closes the file, leaving only the temporary file path.
    ///
    /// This is useful when another process must be able to open the temporary
    /// file.
    pub fn into_temp_path(self) -> TempPath {
        self.path
    }
}

impl Read for NamedTempFile {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.as_file_mut().read(buf)
    }
}

impl<'a> Read for &'a NamedTempFile {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.as_file().read(buf)
    }
}

impl Write for NamedTempFile {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.as_file_mut().write(buf)
    }
    #[inline]
    fn flush(&mut self) -> io::Result<()> {
        self.as_file_mut().flush()
    }
}

impl<'a> Write for &'a NamedTempFile {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.as_file().write(buf)
    }
    #[inline]
    fn flush(&mut self) -> io::Result<()> {
        self.as_file().flush()
    }
}

impl Seek for NamedTempFile {
    fn seek(&mut self, pos: SeekFrom) -> io::Result<u64> {
        self.as_file_mut().seek(pos)
    }
}

impl<'a> Seek for &'a NamedTempFile {
    fn seek(&mut self, pos: SeekFrom) -> io::Result<u64> {
        self.as_file().seek(pos)
    }
}

#[cfg(unix)]
impl std::os::unix::io::AsRawFd for NamedTempFile {
    #[inline]
    fn as_raw_fd(&self) -> std::os::unix::io::RawFd {
        self.as_file().as_raw_fd()
    }
}

#[cfg(windows)]
impl std::os::windows::io::AsRawHandle for NamedTempFile {
    #[inline]
    fn as_raw_handle(&self) -> std::os::windows::io::RawHandle {
        self.as_file().as_raw_handle()
    }
}

// pub(crate)
pub fn create_named(path: PathBuf) -> io::Result<NamedTempFile> {
    imp::create_named(&path).map(|file| NamedTempFile {
        path: TempPath { path },
        file,
    })
}
