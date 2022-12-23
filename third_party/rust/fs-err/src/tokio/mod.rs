//! Tokio-specific wrappers that use `fs_err` error messages.

use crate::errors::{Error, ErrorKind, SourceDestError, SourceDestErrorKind};
use std::fs::{Metadata, Permissions};
use std::path::{Path, PathBuf};
use tokio::io;
mod dir_builder;
mod file;
mod open_options;
mod read_dir;

pub use self::open_options::OpenOptions;
pub use self::read_dir::{read_dir, DirEntry, ReadDir};
pub use dir_builder::DirBuilder;
pub use file::File;

/// Wrapper for [`tokio::fs::canonicalize`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn canonicalize(path: impl AsRef<Path>) -> io::Result<PathBuf> {
    let path = path.as_ref();
    tokio::fs::canonicalize(path)
        .await
        .map_err(|err| Error::build(err, ErrorKind::Canonicalize, path))
}

/// Wrapper for [`tokio::fs::copy`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn copy(from: impl AsRef<Path>, to: impl AsRef<Path>) -> Result<u64, io::Error> {
    let (from, to) = (from.as_ref(), to.as_ref());
    tokio::fs::copy(from, to)
        .await
        .map_err(|err| SourceDestError::build(err, SourceDestErrorKind::Copy, from, to))
}

/// Wrapper for [`tokio::fs::create_dir`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn create_dir(path: impl AsRef<Path>) -> io::Result<()> {
    let path = path.as_ref();
    tokio::fs::create_dir(path)
        .await
        .map_err(|err| Error::build(err, ErrorKind::CreateDir, path))
}

/// Wrapper for [`tokio::fs::create_dir_all`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn create_dir_all(path: impl AsRef<Path>) -> io::Result<()> {
    let path = path.as_ref();
    tokio::fs::create_dir_all(path)
        .await
        .map_err(|err| Error::build(err, ErrorKind::CreateDir, path))
}

/// Wrapper for [`tokio::fs::hard_link`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn hard_link(src: impl AsRef<Path>, dst: impl AsRef<Path>) -> io::Result<()> {
    let (src, dst) = (src.as_ref(), dst.as_ref());
    tokio::fs::hard_link(src, dst)
        .await
        .map_err(|err| SourceDestError::build(err, SourceDestErrorKind::HardLink, src, dst))
}

/// Wrapper for [`tokio::fs::metadata`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn metadata(path: impl AsRef<Path>) -> io::Result<Metadata> {
    let path = path.as_ref();
    tokio::fs::metadata(path)
        .await
        .map_err(|err| Error::build(err, ErrorKind::Metadata, path))
}

/// Wrapper for [`tokio::fs::read`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn read(path: impl AsRef<Path>) -> io::Result<Vec<u8>> {
    let path = path.as_ref();
    tokio::fs::read(path)
        .await
        .map_err(|err| Error::build(err, ErrorKind::Read, path))
}

/// Wrapper for [`tokio::fs::read_link`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn read_link(path: impl AsRef<Path>) -> io::Result<PathBuf> {
    let path = path.as_ref();
    tokio::fs::read_link(path)
        .await
        .map_err(|err| Error::build(err, ErrorKind::ReadLink, path))
}

/// Wrapper for [`tokio::fs::read_to_string`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn read_to_string(path: impl AsRef<Path>) -> io::Result<String> {
    let path = path.as_ref();
    tokio::fs::read_to_string(path)
        .await
        .map_err(|err| Error::build(err, ErrorKind::Read, path))
}

/// Wrapper for [`tokio::fs::remove_dir`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn remove_dir(path: impl AsRef<Path>) -> io::Result<()> {
    let path = path.as_ref();
    tokio::fs::remove_dir(path)
        .await
        .map_err(|err| Error::build(err, ErrorKind::RemoveDir, path))
}

/// Wrapper for [`tokio::fs::remove_dir_all`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn remove_dir_all(path: impl AsRef<Path>) -> io::Result<()> {
    let path = path.as_ref();
    tokio::fs::remove_dir_all(path)
        .await
        .map_err(|err| Error::build(err, ErrorKind::RemoveDir, path))
}

/// Wrapper for [`tokio::fs::remove_file`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn remove_file(path: impl AsRef<Path>) -> io::Result<()> {
    let path = path.as_ref();
    tokio::fs::remove_file(path)
        .await
        .map_err(|err| Error::build(err, ErrorKind::RemoveFile, path))
}

/// Wrapper for [`tokio::fs::rename`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn rename(from: impl AsRef<Path>, to: impl AsRef<Path>) -> io::Result<()> {
    let (from, to) = (from.as_ref(), to.as_ref());
    tokio::fs::rename(from, to)
        .await
        .map_err(|err| SourceDestError::build(err, SourceDestErrorKind::Rename, from, to))
}

/// Wrapper for [`tokio::fs::set_permissions`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn set_permissions(path: impl AsRef<Path>, perm: Permissions) -> io::Result<()> {
    let path = path.as_ref();
    tokio::fs::set_permissions(path, perm)
        .await
        .map_err(|err| Error::build(err, ErrorKind::SetPermissions, path))
}

/// Wrapper for [`tokio::fs::symlink_metadata`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn symlink_metadata(path: impl AsRef<Path>) -> io::Result<Metadata> {
    let path = path.as_ref();
    tokio::fs::symlink_metadata(path)
        .await
        .map_err(|err| Error::build(err, ErrorKind::SymlinkMetadata, path))
}

/// Wrapper for [`tokio::fs::symlink`].
#[cfg(unix)]
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn symlink(src: impl AsRef<Path>, dst: impl AsRef<Path>) -> io::Result<()> {
    let (src, dst) = (src.as_ref(), dst.as_ref());
    tokio::fs::symlink(src, dst)
        .await
        .map_err(|err| SourceDestError::build(err, SourceDestErrorKind::Symlink, src, dst))
}

/// Wrapper for [`tokio::fs::symlink_dir`].
#[cfg(windows)]
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn symlink(src: impl AsRef<Path>, dst: impl AsRef<Path>) -> io::Result<()> {
    let (src, dst) = (src.as_ref(), dst.as_ref());
    tokio::fs::symlink_dir(src, dst)
        .await
        .map_err(|err| SourceDestError::build(err, SourceDestErrorKind::SymlinkDir, src, dst))
}

/// Wrapper for [`tokio::fs::symlink_file`].
#[cfg(windows)]
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn symlink_file(src: impl AsRef<Path>, dst: impl AsRef<Path>) -> io::Result<()> {
    let (src, dst) = (src.as_ref(), dst.as_ref());
    tokio::fs::symlink_file(src, dst)
        .await
        .map_err(|err| SourceDestError::build(err, SourceDestErrorKind::SymlinkFile, src, dst))
}

/// Wrapper for [`tokio::fs::write`].
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub async fn write(path: impl AsRef<Path>, contents: impl AsRef<[u8]>) -> io::Result<()> {
    let (path, contents) = (path.as_ref(), contents.as_ref());
    tokio::fs::write(path, contents)
        .await
        .map_err(|err| Error::build(err, ErrorKind::Write, path))
}
