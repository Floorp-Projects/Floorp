/// Windows-specific extensions to wrappers in `fs_err` for `std::fs` types.
pub mod fs {
    use crate::{SourceDestError, SourceDestErrorKind};
    use std::io;
    use std::path::Path;
    /// Wrapper for [std::os::windows::fs::symlink_dir](https://doc.rust-lang.org/std/os/windows/fs/fn.symlink_dir.html)
    pub fn symlink_dir<P: AsRef<Path>, Q: AsRef<Path>>(src: P, dst: Q) -> io::Result<()> {
        let src = src.as_ref();
        let dst = dst.as_ref();
        std::os::windows::fs::symlink_dir(src, dst)
            .map_err(|err| SourceDestError::build(err, SourceDestErrorKind::SymlinkDir, src, dst))
    }

    /// Wrapper for [std::os::windows::fs::symlink_file](https://doc.rust-lang.org/std/os/windows/fs/fn.symlink_file.html)
    pub fn symlink_file<P: AsRef<Path>, Q: AsRef<Path>>(src: P, dst: Q) -> io::Result<()> {
        let src = src.as_ref();
        let dst = dst.as_ref();
        std::os::windows::fs::symlink_file(src, dst)
            .map_err(|err| SourceDestError::build(err, SourceDestErrorKind::SymlinkFile, src, dst))
    }

    /// Wrapper for [`std::os::windows::fs::FileExt`](https://doc.rust-lang.org/std/os/windows/fs/trait.FileExt.html).
    ///
    /// The std traits might be extended in the future (See issue [#49961](https://github.com/rust-lang/rust/issues/49961#issuecomment-382751777)).
    /// This trait is sealed and can not be implemented by other crates.
    pub trait FileExt: crate::Sealed {
        /// Wrapper for [`FileExt::seek_read`](https://doc.rust-lang.org/std/os/windows/fs/trait.FileExt.html#tymethod.seek_read)
        fn seek_read(&self, buf: &mut [u8], offset: u64) -> io::Result<usize>;
        /// Wrapper for [`FileExt::seek_wriite`](https://doc.rust-lang.org/std/os/windows/fs/trait.FileExt.html#tymethod.seek_write)
        fn seek_write(&self, buf: &[u8], offset: u64) -> io::Result<usize>;
    }

    /// Wrapper for [`std::os::windows::fs::OpenOptionsExt`](https://doc.rust-lang.org/std/os/windows/fs/trait.OpenOptionsExt.html)
    ///
    /// The std traits might be extended in the future (See issue [#49961](https://github.com/rust-lang/rust/issues/49961#issuecomment-382751777)).
    /// This trait is sealed and can not be implemented by other crates.
    pub trait OpenOptionsExt: crate::Sealed {
        /// Wrapper for [`OpenOptionsExt::access_mode`](https://doc.rust-lang.org/std/os/windows/fs/trait.OpenOptionsExt.html#tymethod.access_mode)
        fn access_mode(&mut self, access: u32) -> &mut Self;
        /// Wrapper for [`OpenOptionsExt::share_mode`](https://doc.rust-lang.org/std/os/windows/fs/trait.OpenOptionsExt.html#tymethod.share_mode)
        fn share_mode(&mut self, val: u32) -> &mut Self;
        /// Wrapper for [`OpenOptionsExt::custom_flags`](https://doc.rust-lang.org/std/os/windows/fs/trait.OpenOptionsExt.html#tymethod.custom_flags)
        fn custom_flags(&mut self, flags: u32) -> &mut Self;
        /// Wrapper for [`OpenOptionsExt::attributes`](https://doc.rust-lang.org/std/os/windows/fs/trait.OpenOptionsExt.html#tymethod.attributes)
        fn attributes(&mut self, val: u32) -> &mut Self;
        /// Wrapper for [`OpenOptionsExt::security_qos_flags`](https://doc.rust-lang.org/std/os/windows/fs/trait.OpenOptionsExt.html#tymethod.security_qos_flags)
        fn security_qos_flags(&mut self, flags: u32) -> &mut Self;
    }
}
