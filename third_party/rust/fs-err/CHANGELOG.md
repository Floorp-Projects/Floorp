# fs-err Changelog

## 2.8.1

* Fixed docs.rs build

## 2.8.0

* Implement I/O safety traits (`AsFd`/`AsHandle`, `Into<OwnedFd>`/`Into<OwnedHandle>`) for file. This feature requires Rust 1.63 or later and is gated behind the `io_safety` feature flag. ([#39](https://github.com/andrewhickman/fs-err/pull/39))

## 2.7.0

* Implement `From<fs_err::File> for std::fs::File` ([#38](https://github.com/andrewhickman/fs-err/pull/38))

## 2.6.0

* Added [`File::into_parts`](https://docs.rs/fs-err/2.6.0/fs_err/struct.File.html#method.into_parts) and [`File::file_mut`](https://docs.rs/fs-err/2.6.0/fs_err/struct.File.html#method.file_mut) to provide more access to the underlying `std::fs::File`.
* Fixed some typos in documention ([#33](https://github.com/andrewhickman/fs-err/pull/33))

## 2.5.0
* Added `symlink` for unix platforms
* Added `symlink_file` and `symlink_dir` for windows
* Implemented os-specific extension traits for `File`
  - `std::os::unix::io::{AsRawFd, IntoRawFd}`
  - `std::os::windows::io::{AsRawHandle, IntoRawHandle}`
  - Added trait wrappers for `std::os::{unix, windows}::fs::FileExt` and implemented them for `fs_err::File`
* Implemented os-specific extension traits for `OpenOptions`
  - Added trait wrappers for `std::os::{unix, windows}::fs::OpenOptionsExt` and implemented them for `fs_err::OpenOptions`
* Improved compile times by converting arguments early and forwarding only a small number of types internally. There will be a slight performance hit only in the error case.
* Reduced trait bounds on generics from `AsRef<Path> + Into<PathBuf>` to either `AsRef<Path>` or `Into<PathBuf>`, making the functions more general.

## 2.4.0
* Added `canonicalize`, `hard link`, `read_link`, `rename`, `symlink_metadata` and `soft_link`. ([#25](https://github.com/andrewhickman/fs-err/pull/25))
* Added aliases to `std::path::Path` via extension trait ([#26](https://github.com/andrewhickman/fs-err/pull/26))
* Added `OpenOptions` ([#27](https://github.com/andrewhickman/fs-err/pull/27))
* Added `set_permissions` ([#28](https://github.com/andrewhickman/fs-err/pull/28))

## 2.3.0
* Added `create_dir` and `create_dir_all`. ([#19](https://github.com/andrewhickman/fs-err/pull/19))
* Added `remove_file`, `remove_dir`, and `remove_dir_all`. ([#16](https://github.com/andrewhickman/fs-err/pull/16))

## 2.2.0
* Added `metadata`. ([#15](https://github.com/andrewhickman/fs-err/pull/15))

## 2.1.0
* Updated crate-level documentation. ([#8](https://github.com/andrewhickman/fs-err/pull/8))
* Added `read_dir`, `ReadDir`, and `DirEntry`. ([#9](https://github.com/andrewhickman/fs-err/pull/9))

## 2.0.1 (2020-02-22)
* Added `copy`. ([#7](https://github.com/andrewhickman/fs-err/pull/7))

## 2.0.0 (2020-02-19)
* Removed custom error type in favor of `std::io::Error`. ([#2](https://github.com/andrewhickman/fs-err/pull/2))

## 1.0.1 (2020-02-15)
* Fixed bad documentation link in `Cargo.toml`.

## 1.0.0 (2020-02-15)
* No changes from 0.1.2.

## 0.1.2 (2020-02-10)
* Added `Error::cause` implementation for `fs_err::Error`.

## 0.1.1 (2020-02-05)
* Added wrappers for `std::fs::*` functions.

## 0.1.0 (2020-02-02)
* Initial release, containing a wrapper around `std::fs::File`.
