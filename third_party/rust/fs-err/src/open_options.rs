use std::{fs, io, path::PathBuf};
#[derive(Clone, Debug)]
/// Wrapper around [`std::fs::OptionOptions`](https://doc.rust-lang.org/std/fs/struct.OpenOptions.html)
pub struct OpenOptions(fs::OpenOptions);

impl OpenOptions {
    /// Wrapper for [`std::fs::OpenOptions::new`](https://doc.rust-lang.org/std/fs/struct.OpenOptions.html#method.new)
    #[allow(clippy::new_without_default)]
    pub fn new() -> Self {
        OpenOptions(fs::OpenOptions::new())
    }

    /// Wrapper for [`std::fs::OpenOptions::read`](https://doc.rust-lang.org/std/fs/struct.OpenOptions.html#method.read)
    pub fn read(&mut self, read: bool) -> &mut Self {
        self.0.read(read);
        self
    }

    /// Wrapper for [`std::fs::OpenOptions::write`](https://doc.rust-lang.org/std/fs/struct.OpenOptions.html#method.write)
    pub fn write(&mut self, write: bool) -> &mut Self {
        self.0.write(write);
        self
    }

    /// Wrapper for [`std::fs::OpenOptions::append`](https://doc.rust-lang.org/std/fs/struct.OpenOptions.html#method.append)
    pub fn append(&mut self, append: bool) -> &mut Self {
        self.0.append(append);
        self
    }

    /// Wrapper for [`std::fs::OpenOptions::truncate`](https://doc.rust-lang.org/std/fs/struct.OpenOptions.html#method.truncate)
    pub fn truncate(&mut self, truncate: bool) -> &mut Self {
        self.0.truncate(truncate);
        self
    }

    /// Wrapper for [`std::fs::OpenOptions::create`](https://doc.rust-lang.org/std/fs/struct.OpenOptions.html#method.create)
    pub fn create(&mut self, create: bool) -> &mut Self {
        self.0.create(create);
        self
    }

    /// Wrapper for [`std::fs::OpenOptions::create_new`](https://doc.rust-lang.org/std/fs/struct.OpenOptions.html#method.create_new)
    pub fn create_new(&mut self, create_new: bool) -> &mut Self {
        self.0.create_new(create_new);
        self
    }

    /// Wrapper for [`std::fs::OpenOptions::open`](https://doc.rust-lang.org/std/fs/struct.OpenOptions.html#method.open)
    pub fn open<P>(&self, path: P) -> io::Result<crate::File>
    where
        P: Into<PathBuf>,
    {
        // We have to either duplicate the logic or call the deprecated method here.
        // We can't let the deprecated function call this method, because we can't construct
        // `&fs_err::OpenOptions` from `&fs::OpenOptions` without cloning
        // (although cloning would probably be cheap).
        #[allow(deprecated)]
        crate::File::from_options(path.into(), self.options())
    }
}

/// Methods added by fs-err that are not available on
/// [`std::fs::OpenOptions`](https://doc.rust-lang.org/stable/std/fs/struct.OpenOptions.html).
impl OpenOptions {
    /// Constructs `Self` from [`std::fs::OpenOptions`](https://doc.rust-lang.org/stable/std/fs/struct.OpenOptions.html)
    pub fn from_options(options: fs::OpenOptions) -> Self {
        Self(options)
    }

    /// Returns a reference to the underlying [`std::fs::OpenOptions`](https://doc.rust-lang.org/stable/std/fs/struct.OpenOptions.html).
    ///
    /// Note that calling `open()` on this reference will NOT give you the improved errors from fs-err.
    pub fn options(&self) -> &fs::OpenOptions {
        &self.0
    }

    /// Returns a mutable reference to the underlying [`std::fs::OpenOptions`](https://doc.rust-lang.org/stable/std/fs/struct.OpenOptions.html).
    ///
    /// This allows you to change settings that don't yet have wrappers in fs-err.
    /// Note that calling `open()` on this reference will NOT give you the improved errors from fs-err.
    pub fn options_mut(&mut self) -> &mut fs::OpenOptions {
        &mut self.0
    }
}

#[cfg(unix)]
mod unix {
    use crate::os::unix::fs::OpenOptionsExt;
    use std::os::unix::fs::OpenOptionsExt as _;
    impl OpenOptionsExt for crate::OpenOptions {
        fn mode(&mut self, mode: u32) -> &mut Self {
            self.options_mut().mode(mode);
            self
        }

        fn custom_flags(&mut self, flags: i32) -> &mut Self {
            self.options_mut().custom_flags(flags);
            self
        }
    }
}

#[cfg(windows)]
mod windows {
    use crate::os::windows::fs::OpenOptionsExt;
    use std::os::windows::fs::OpenOptionsExt as _;

    impl OpenOptionsExt for crate::OpenOptions {
        fn access_mode(&mut self, access: u32) -> &mut Self {
            self.options_mut().access_mode(access);
            self
        }

        fn share_mode(&mut self, val: u32) -> &mut Self {
            self.options_mut().share_mode(val);
            self
        }
        fn custom_flags(&mut self, flags: u32) -> &mut Self {
            self.options_mut().custom_flags(flags);
            self
        }

        fn attributes(&mut self, val: u32) -> &mut Self {
            self.options_mut().attributes(val);
            self
        }

        fn security_qos_flags(&mut self, flags: u32) -> &mut Self {
            self.options_mut().security_qos_flags(flags);
            self
        }
    }
}
