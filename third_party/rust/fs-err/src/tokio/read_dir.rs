use crate::errors::{Error, ErrorKind};
use std::ffi::OsString;
use std::fs::{FileType, Metadata};
use std::io;
use std::path::{Path, PathBuf};
use std::task::{ready, Context, Poll};
use tokio::fs;

/// Wrapper for [`tokio::fs::read_dir`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn read_dir(path: impl AsRef<Path>) -> io::Result<ReadDir> {
    let path = path.as_ref();
    let tokio = fs::read_dir(path)
        .await
        .map_err(|err| Error::build(err, ErrorKind::ReadDir, path))?;
    Ok(ReadDir {
        tokio,
        path: path.to_owned(),
    })
}

/// Reads the entries in a directory.
///
/// This is a wrapper around [`tokio::fs::ReadDir`].
#[derive(Debug)]
#[must_use = "streams do nothing unless polled"]
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub struct ReadDir {
    tokio: fs::ReadDir,
    path: PathBuf,
}

impl ReadDir {
    /// Wrapper around [`tokio::fs::ReadDir::next_entry`].
    pub async fn next_entry(&mut self) -> io::Result<Option<DirEntry>> {
        match self.tokio.next_entry().await {
            Ok(entry) => Ok(entry.map(|e| DirEntry { tokio: e })),
            Err(err) => Err(Error::build(err, ErrorKind::ReadDir, &self.path)),
        }
    }

    /// Wrapper around [`tokio::fs::ReadDir::poll_next_entry`].
    pub fn poll_next_entry(&mut self, cx: &mut Context<'_>) -> Poll<io::Result<Option<DirEntry>>> {
        Poll::Ready(match ready!(self.tokio.poll_next_entry(cx)) {
            Ok(entry) => Ok(entry.map(|e| DirEntry { tokio: e })),
            Err(err) => Err(Error::build(err, ErrorKind::ReadDir, &self.path)),
        })
    }
}

/// Entries returned by the [`ReadDir`] stream.
///
/// This is a wrapper around [`tokio::fs::DirEntry`].
#[derive(Debug)]
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub struct DirEntry {
    tokio: fs::DirEntry,
}

impl DirEntry {
    /// Wrapper around [`tokio::fs::DirEntry::path`].
    pub fn path(&self) -> PathBuf {
        self.tokio.path()
    }

    /// Wrapper around [`tokio::fs::DirEntry::file_name`].
    pub fn file_name(&self) -> OsString {
        self.tokio.file_name()
    }

    /// Wrapper around [`tokio::fs::DirEntry::metadata`].
    pub async fn metadata(&self) -> io::Result<Metadata> {
        self.tokio
            .metadata()
            .await
            .map_err(|err| Error::build(err, ErrorKind::Metadata, self.path()))
    }

    /// Wrapper around [`tokio::fs::DirEntry::file_type`].
    pub async fn file_type(&self) -> io::Result<FileType> {
        self.tokio
            .file_type()
            .await
            .map_err(|err| Error::build(err, ErrorKind::Metadata, self.path()))
    }
}

#[cfg(unix)]
impl DirEntry {
    /// Wrapper around [`tokio::fs::DirEntry::ino`].
    pub fn ino(&self) -> u64 {
        self.tokio.ino()
    }
}
