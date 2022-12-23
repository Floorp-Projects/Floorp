use crate::errors::{Error, ErrorKind};
use std::fs::{Metadata, Permissions};
use std::io;
use std::io::{IoSlice, SeekFrom};
use std::path::{Path, PathBuf};
use std::pin::Pin;
use std::task::{ready, Context, Poll};
use tokio::fs;
use tokio::fs::File as TokioFile;
use tokio::io::{AsyncRead, AsyncSeek, AsyncWrite, ReadBuf};

/// Wrapper around [`tokio::fs::File`] which adds more helpful
/// information to all errors.
#[derive(Debug)]
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub struct File {
    tokio: fs::File,
    path: PathBuf,
}

impl File {
    /// Wrapper for [`tokio::fs::File::open`].
    pub async fn open(path: impl Into<PathBuf>) -> io::Result<File> {
        let path = path.into();
        let f = TokioFile::open(&path)
            .await
            .map_err(|err| Error::build(err, ErrorKind::OpenFile, &path))?;
        Ok(File::from_parts(f, path))
    }

    /// Wrapper for [`tokio::fs::File::create`].
    pub async fn create(path: impl Into<PathBuf>) -> io::Result<File> {
        let path = path.into();
        match TokioFile::create(&path).await {
            Ok(f) => Ok(File::from_parts(f, path)),
            Err(err) => Err(Error::build(err, ErrorKind::CreateFile, &path)),
        }
    }

    /// Wrapper for [`tokio::fs::File::from_std`].
    pub fn from_std(std: crate::File) -> File {
        let (std, path) = std.into_parts();
        File::from_parts(TokioFile::from_std(std), path)
    }

    /// Wrapper for [`tokio::fs::File::sync_all`].
    pub async fn sync_all(&self) -> io::Result<()> {
        self.tokio
            .sync_all()
            .await
            .map_err(|err| self.error(err, ErrorKind::SyncFile))
    }

    /// Wrapper for [`tokio::fs::File::sync_data`].
    pub async fn sync_data(&self) -> io::Result<()> {
        self.tokio
            .sync_data()
            .await
            .map_err(|err| self.error(err, ErrorKind::SyncFile))
    }

    /// Wrapper for [`tokio::fs::File::set_len`].
    pub async fn set_len(&self, size: u64) -> io::Result<()> {
        self.tokio
            .set_len(size)
            .await
            .map_err(|err| self.error(err, ErrorKind::SetLen))
    }

    /// Wrapper for [`tokio::fs::File::metadata`].
    pub async fn metadata(&self) -> io::Result<Metadata> {
        self.tokio
            .metadata()
            .await
            .map_err(|err| self.error(err, ErrorKind::Metadata))
    }

    /// Wrapper for [`tokio::fs::File::try_clone`].
    pub async fn try_clone(&self) -> io::Result<File> {
        match self.tokio.try_clone().await {
            Ok(file) => Ok(File::from_parts(file, self.path.clone())),
            Err(err) => Err(self.error(err, ErrorKind::Clone)),
        }
    }

    /// Wrapper for [`tokio::fs::File::into_std`].
    pub async fn into_std(self) -> crate::File {
        crate::File::from_parts(self.tokio.into_std().await, self.path)
    }

    /// Wrapper for [`tokio::fs::File::try_into_std`].
    pub fn try_into_std(self) -> Result<crate::File, File> {
        match self.tokio.try_into_std() {
            Ok(f) => Ok(crate::File::from_parts(f, self.path)),
            Err(f) => Err(File::from_parts(f, self.path)),
        }
    }

    /// Wrapper for [`tokio::fs::File::set_permissions`].
    pub async fn set_permissions(&self, perm: Permissions) -> io::Result<()> {
        self.tokio
            .set_permissions(perm)
            .await
            .map_err(|err| self.error(err, ErrorKind::SetPermissions))
    }
}

/// Methods added by fs-err that are not available on
/// [`tokio::fs::File`].
impl File {
    /// Creates a [`File`](struct.File.html) from a raw file and its path.
    pub fn from_parts<P>(file: TokioFile, path: P) -> Self
    where
        P: Into<PathBuf>,
    {
        File {
            tokio: file,
            path: path.into(),
        }
    }

    /// Extract the raw file and its path from this [`File`](struct.File.html).
    pub fn into_parts(self) -> (TokioFile, PathBuf) {
        (self.tokio, self.path)
    }

    /// Returns a reference to the underlying [`tokio::fs::File`].
    pub fn file(&self) -> &TokioFile {
        &self.tokio
    }

    /// Returns a mutable reference to the underlying [`tokio::fs::File`].
    pub fn file_mut(&mut self) -> &mut TokioFile {
        &mut self.tokio
    }

    /// Returns a reference to the path that this file was created with.
    pub fn path(&self) -> &Path {
        &self.path
    }

    /// Wrap the error in information specific to this `File` object.
    fn error(&self, source: io::Error, kind: ErrorKind) -> io::Error {
        Error::build(source, kind, &self.path)
    }
}

impl From<crate::File> for File {
    fn from(f: crate::File) -> Self {
        let (f, path) = f.into_parts();
        File::from_parts(f.into(), path)
    }
}

impl From<File> for TokioFile {
    fn from(f: File) -> Self {
        f.into_parts().0
    }
}

#[cfg(unix)]
impl std::os::unix::io::AsRawFd for File {
    fn as_raw_fd(&self) -> std::os::unix::io::RawFd {
        self.tokio.as_raw_fd()
    }
}

#[cfg(windows)]
impl std::os::windows::io::AsRawHandle for File {
    fn as_raw_handle(&self) -> std::os::windows::io::RawHandle {
        self.tokio.as_raw_handle()
    }
}

impl AsyncRead for File {
    fn poll_read(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        buf: &mut ReadBuf<'_>,
    ) -> Poll<io::Result<()>> {
        Poll::Ready(
            ready!(Pin::new(&mut self.tokio).poll_read(cx, buf))
                .map_err(|err| self.error(err, ErrorKind::Read)),
        )
    }
}

impl AsyncSeek for File {
    fn start_seek(mut self: Pin<&mut Self>, position: SeekFrom) -> io::Result<()> {
        Pin::new(&mut self.tokio)
            .start_seek(position)
            .map_err(|err| self.error(err, ErrorKind::Seek))
    }

    fn poll_complete(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<io::Result<u64>> {
        Poll::Ready(
            ready!(Pin::new(&mut self.tokio).poll_complete(cx))
                .map_err(|err| self.error(err, ErrorKind::Seek)),
        )
    }
}

impl AsyncWrite for File {
    fn poll_write(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        buf: &[u8],
    ) -> Poll<io::Result<usize>> {
        Poll::Ready(
            ready!(Pin::new(&mut self.tokio).poll_write(cx, buf))
                .map_err(|err| self.error(err, ErrorKind::Write)),
        )
    }

    fn poll_flush(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<io::Result<()>> {
        Poll::Ready(
            ready!(Pin::new(&mut self.tokio).poll_flush(cx))
                .map_err(|err| self.error(err, ErrorKind::Flush)),
        )
    }

    fn poll_shutdown(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<io::Result<()>> {
        Poll::Ready(
            ready!(Pin::new(&mut self.tokio).poll_shutdown(cx))
                .map_err(|err| self.error(err, ErrorKind::Flush)),
        )
    }

    fn poll_write_vectored(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        bufs: &[IoSlice<'_>],
    ) -> Poll<io::Result<usize>> {
        Poll::Ready(
            ready!(Pin::new(&mut self.tokio).poll_write_vectored(cx, bufs))
                .map_err(|err| self.error(err, ErrorKind::Write)),
        )
    }

    fn is_write_vectored(&self) -> bool {
        self.tokio.is_write_vectored()
    }
}
