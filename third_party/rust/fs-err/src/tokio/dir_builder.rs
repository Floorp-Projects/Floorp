use crate::errors::{Error, ErrorKind};
use std::io;
use std::path::Path;

/// A builder for creating directories in various manners.
///
/// This is a wrapper around [`tokio::fs::DirBuilder`].
#[derive(Debug, Default)]
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub struct DirBuilder {
    inner: tokio::fs::DirBuilder,
}

impl DirBuilder {
    /// Creates a new set of options with default mode/security settings for all
    /// platforms and also non-recursive.
    ///
    /// This is a wrapper version of [`tokio::fs::DirBuilder::new`]
    ///
    /// # Examples
    ///
    /// ```no_run
    /// use fs_err::tokio::DirBuilder;
    ///
    /// let builder = DirBuilder::new();
    /// ```
    pub fn new() -> Self {
        Default::default()
    }

    /// Wrapper around [`tokio::fs::DirBuilder::recursive`].
    pub fn recursive(&mut self, recursive: bool) -> &mut Self {
        self.inner.recursive(recursive);
        self
    }

    /// Wrapper around [`tokio::fs::DirBuilder::create`].
    pub async fn create(&self, path: impl AsRef<Path>) -> io::Result<()> {
        let path = path.as_ref();
        self.inner
            .create(path)
            .await
            .map_err(|err| Error::build(err, ErrorKind::CreateDir, path))
    }
}

#[cfg(unix)]
impl DirBuilder {
    /// Wrapper around [`tokio::fs::DirBuilder::mode`].
    pub fn mode(&mut self, mode: u32) -> &mut Self {
        self.inner.mode(mode);
        self
    }
}
