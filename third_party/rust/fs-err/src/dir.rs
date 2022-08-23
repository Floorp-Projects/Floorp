use std::ffi::OsString;
use std::fs;
use std::io;
use std::path::PathBuf;

use crate::errors::{Error, ErrorKind};

/// Wrapper for [`fs::read_dir`](https://doc.rust-lang.org/stable/std/fs/fn.read_dir.html).
pub fn read_dir<P: Into<PathBuf>>(path: P) -> io::Result<ReadDir> {
    let path = path.into();

    match fs::read_dir(&path) {
        Ok(inner) => Ok(ReadDir { inner, path }),
        Err(source) => Err(Error::build(source, ErrorKind::ReadDir, path)),
    }
}

/// Wrapper around [`std::fs::ReadDir`][std::fs::ReadDir] which adds more
/// helpful information to all errors.
///
/// This struct is created via [`fs_err::read_dir`][fs_err::read_dir].
///
/// [std::fs::ReadDir]: https://doc.rust-lang.org/stable/std/fs/struct.ReadDir.html
/// [fs_err::read_dir]: fn.read_dir.html
#[derive(Debug)]
pub struct ReadDir {
    inner: fs::ReadDir,
    path: PathBuf,
}

impl Iterator for ReadDir {
    type Item = io::Result<DirEntry>;

    fn next(&mut self) -> Option<Self::Item> {
        Some(
            self.inner
                .next()?
                .map_err(|source| Error::build(source, ErrorKind::ReadDir, &self.path))
                .map(|inner| DirEntry { inner }),
        )
    }
}

/// Wrapper around [`std::fs::DirEntry`][std::fs::DirEntry] which adds more
/// helpful information to all errors.
///
/// [std::fs::DirEntry]: https://doc.rust-lang.org/stable/std/fs/struct.DirEntry.html
#[derive(Debug)]
pub struct DirEntry {
    inner: fs::DirEntry,
}

impl DirEntry {
    /// Wrapper for [`DirEntry::path`](https://doc.rust-lang.org/stable/std/fs/struct.DirEntry.html#method.path).
    pub fn path(&self) -> PathBuf {
        self.inner.path()
    }

    /// Wrapper for [`DirEntry::metadata`](https://doc.rust-lang.org/stable/std/fs/struct.DirEntry.html#method.metadata).
    pub fn metadata(&self) -> io::Result<fs::Metadata> {
        self.inner
            .metadata()
            .map_err(|source| Error::build(source, ErrorKind::Metadata, self.path()))
    }

    /// Wrapper for [`DirEntry::file_type`](https://doc.rust-lang.org/stable/std/fs/struct.DirEntry.html#method.file_type).
    pub fn file_type(&self) -> io::Result<fs::FileType> {
        self.inner
            .file_type()
            .map_err(|source| Error::build(source, ErrorKind::Metadata, self.path()))
    }

    /// Wrapper for [`DirEntry::file_name`](https://doc.rust-lang.org/stable/std/fs/struct.DirEntry.html#method.file_name).
    pub fn file_name(&self) -> OsString {
        self.inner.file_name()
    }
}

#[cfg(unix)]
mod unix {
    use std::os::unix::fs::DirEntryExt;

    use super::*;

    impl DirEntryExt for DirEntry {
        fn ino(&self) -> u64 {
            self.inner.ino()
        }
    }
}
