/// Unix-specific extensions to wrappers in `fs_err` for `std::fs` types.
pub mod fs {
    use std::io;
    use std::path::Path;

    use crate::SourceDestError;
    use crate::SourceDestErrorKind;

    /// Wrapper for [`std::os::unix::fs::symlink`](https://doc.rust-lang.org/std/os/unix/fs/fn.symlink.html)
    pub fn symlink<P: AsRef<Path>, Q: AsRef<Path>>(src: P, dst: Q) -> io::Result<()> {
        let src = src.as_ref();
        let dst = dst.as_ref();
        std::os::unix::fs::symlink(src, dst)
            .map_err(|err| SourceDestError::build(err, SourceDestErrorKind::Symlink, src, dst))
    }

    /// Wrapper for [`std::os::unix::fs::FileExt`](https://doc.rust-lang.org/std/os/unix/fs/trait.FileExt.html).
    ///
    /// The std traits might be extended in the future (See issue [#49961](https://github.com/rust-lang/rust/issues/49961#issuecomment-382751777)).
    /// This trait is sealed and can not be implemented by other crates.
    pub trait FileExt: crate::Sealed {
        /// Wrapper for [`FileExt::read_at`](https://doc.rust-lang.org/std/os/unix/fs/trait.FileExt.html#tymethod.read_at)
        fn read_at(&self, buf: &mut [u8], offset: u64) -> io::Result<usize>;
        /// Wrapper for [`FileExt::write_at`](https://doc.rust-lang.org/std/os/unix/fs/trait.FileExt.html#tymethod.write_at)
        fn write_at(&self, buf: &[u8], offset: u64) -> io::Result<usize>;
    }

    /// Wrapper for [`std::os::unix::fs::OpenOptionsExt`](https://doc.rust-lang.org/std/os/unix/fs/trait.OpenOptionsExt.html)
    ///
    /// The std traits might be extended in the future (See issue [#49961](https://github.com/rust-lang/rust/issues/49961#issuecomment-382751777)).
    /// This trait is sealed and can not be implemented by other crates.
    pub trait OpenOptionsExt: crate::Sealed {
        /// Wrapper for [`OpenOptionsExt::mode`](https://doc.rust-lang.org/std/os/unix/fs/trait.OpenOptionsExt.html#tymethod.mode)
        fn mode(&mut self, mode: u32) -> &mut Self;
        /// Wrapper for [`OpenOptionsExt::custom_flags`](https://doc.rust-lang.org/std/os/unix/fs/trait.OpenOptionsExt.html#tymethod.custom_flags)
        fn custom_flags(&mut self, flags: i32) -> &mut Self;
    }
}
