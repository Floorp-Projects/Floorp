use crate::errors::{Error, ErrorKind};
use crate::tokio::File;
use std::io;
use std::path::Path;
use tokio::fs::OpenOptions as TokioOpenOptions;

/// Options and flags which can be used to configure how a file is opened.
///
/// This is a wrapper around [`tokio::fs::OpenOptions`].
#[derive(Clone, Debug, Default)]
#[cfg_attr(docsrs, doc(cfg(feature = "tokio")))]
pub struct OpenOptions {
    tokio: TokioOpenOptions,
}

impl OpenOptions {
    /// Creates a blank new set of options ready for configuration.
    ///
    /// All options are initially set to `false`.
    ///
    /// This is a wrapped version of [`tokio::fs::OpenOptions::new`]
    ///
    /// # Examples
    ///
    /// ```no_run
    /// use fs_err::tokio::OpenOptions;
    ///
    /// let mut options = OpenOptions::new();
    /// let future = options.read(true).open("foo.txt");
    /// ```
    pub fn new() -> OpenOptions {
        OpenOptions {
            tokio: TokioOpenOptions::new(),
        }
    }

    /// Wrapper for [`tokio::fs::OpenOptions::read`].
    pub fn read(&mut self, read: bool) -> &mut OpenOptions {
        self.tokio.read(read);
        self
    }

    /// Wrapper for [`tokio::fs::OpenOptions::write`].
    pub fn write(&mut self, write: bool) -> &mut OpenOptions {
        self.tokio.write(write);
        self
    }

    /// Wrapper for [`tokio::fs::OpenOptions::append`].
    pub fn append(&mut self, append: bool) -> &mut OpenOptions {
        self.tokio.append(append);
        self
    }

    /// Wrapper for [`tokio::fs::OpenOptions::truncate`].
    pub fn truncate(&mut self, truncate: bool) -> &mut OpenOptions {
        self.tokio.truncate(truncate);
        self
    }

    /// Wrapper for [`tokio::fs::OpenOptions::create`].
    pub fn create(&mut self, create: bool) -> &mut OpenOptions {
        self.tokio.create(create);
        self
    }

    /// Wrapper for [`tokio::fs::OpenOptions::create_new`].
    pub fn create_new(&mut self, create_new: bool) -> &mut OpenOptions {
        self.tokio.create_new(create_new);
        self
    }

    /// Wrapper for [`tokio::fs::OpenOptions::open`].
    pub async fn open(&self, path: impl AsRef<Path>) -> io::Result<File> {
        let path = path.as_ref();
        self.tokio
            .open(path)
            .await
            .map(|f| File::from_parts(f, path))
            .map_err(|err| Error::build(err, ErrorKind::OpenFile, path))
    }
}

#[cfg(unix)]
impl OpenOptions {
    /// Wrapper for [`tokio::fs::OpenOptions::mode`].
    pub fn mode(&mut self, mode: u32) -> &mut OpenOptions {
        self.tokio.mode(mode);
        self
    }

    /// Wrapper for [`tokio::fs::OpenOptions::custom_flags`].
    pub fn custom_flags(&mut self, flags: i32) -> &mut OpenOptions {
        self.tokio.custom_flags(flags);
        self
    }
}

impl From<std::fs::OpenOptions> for OpenOptions {
    fn from(std: std::fs::OpenOptions) -> Self {
        OpenOptions { tokio: std.into() }
    }
}

impl From<TokioOpenOptions> for OpenOptions {
    fn from(tokio: TokioOpenOptions) -> Self {
        OpenOptions { tokio }
    }
}
