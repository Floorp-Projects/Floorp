/*!
fs-err is a drop-in replacement for [`std::fs`][std::fs] that provides more
helpful messages on errors. Extra information includes which operations was
attempted and any involved paths.

# Error Messages

Using [`std::fs`][std::fs], if this code fails:

```no_run
# use std::fs::File;
let file = File::open("does not exist.txt")?;
# Ok::<(), std::io::Error>(())
```

The error message that Rust gives you isn't very useful:

```txt
The system cannot find the file specified. (os error 2)
```

...but if we use fs-err instead, our error contains more actionable information:

```txt
failed to open file `does not exist.txt`
    caused by: The system cannot find the file specified. (os error 2)
```

# Usage

fs-err's API is the same as [`std::fs`][std::fs], so migrating code to use it is easy.

```no_run
// use std::fs;
use fs_err as fs;

let contents = fs::read_to_string("foo.txt")?;

println!("Read foo.txt: {}", contents);

# Ok::<(), std::io::Error>(())
```

fs-err uses [`std::io::Error`][std::io::Error] for all errors. This helps fs-err
compose well with traits from the standard library like
[`std::io::Read`][std::io::Read] and crates that use them like
[`serde_json`][serde_json]:

```no_run
use fs_err::File;

let file = File::open("my-config.json")?;

// If an I/O error occurs inside serde_json, the error will include a file path
// as well as what operation was being performed.
let decoded: Vec<String> = serde_json::from_reader(file)?;

println!("Program config: {:?}", decoded);

# Ok::<(), Box<dyn std::error::Error>>(())
```

[std::fs]: https://doc.rust-lang.org/stable/std/fs/
[std::io::Error]: https://doc.rust-lang.org/stable/std/io/struct.Error.html
[std::io::Read]: https://doc.rust-lang.org/stable/std/io/trait.Read.html
[serde_json]: https://crates.io/crates/serde_json
*/

#![doc(html_root_url = "https://docs.rs/fs-err/2.8.1")]
#![deny(missing_debug_implementations, missing_docs)]
#![cfg_attr(docsrs, feature(doc_cfg))]

mod dir;
mod errors;
mod file;
mod open_options;
pub mod os;
mod path;

use std::fs;
use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};

use errors::{Error, ErrorKind, SourceDestError, SourceDestErrorKind};

pub use dir::*;
pub use file::*;
pub use open_options::OpenOptions;
pub use path::PathExt;

/// Wrapper for [`fs::read`](https://doc.rust-lang.org/stable/std/fs/fn.read.html).
pub fn read<P: AsRef<Path>>(path: P) -> io::Result<Vec<u8>> {
    let path = path.as_ref();
    let mut file = file::open(path).map_err(|err_gen| err_gen(path.to_path_buf()))?;
    let mut bytes = Vec::with_capacity(initial_buffer_size(&file));
    file.read_to_end(&mut bytes)
        .map_err(|err| Error::build(err, ErrorKind::Read, path))?;
    Ok(bytes)
}

/// Wrapper for [`fs::read_to_string`](https://doc.rust-lang.org/stable/std/fs/fn.read_to_string.html).
pub fn read_to_string<P: AsRef<Path>>(path: P) -> io::Result<String> {
    let path = path.as_ref();
    let mut file = file::open(path).map_err(|err_gen| err_gen(path.to_path_buf()))?;
    let mut string = String::with_capacity(initial_buffer_size(&file));
    file.read_to_string(&mut string)
        .map_err(|err| Error::build(err, ErrorKind::Read, path))?;
    Ok(string)
}

/// Wrapper for [`fs::write`](https://doc.rust-lang.org/stable/std/fs/fn.write.html).
pub fn write<P: AsRef<Path>, C: AsRef<[u8]>>(path: P, contents: C) -> io::Result<()> {
    let path = path.as_ref();
    file::create(path)
        .map_err(|err_gen| err_gen(path.to_path_buf()))?
        .write_all(contents.as_ref())
        .map_err(|err| Error::build(err, ErrorKind::Write, path))
}

/// Wrapper for [`fs::copy`](https://doc.rust-lang.org/stable/std/fs/fn.copy.html).
pub fn copy<P, Q>(from: P, to: Q) -> io::Result<u64>
where
    P: AsRef<Path>,
    Q: AsRef<Path>,
{
    let from = from.as_ref();
    let to = to.as_ref();
    fs::copy(from, to)
        .map_err(|source| SourceDestError::build(source, SourceDestErrorKind::Copy, from, to))
}

/// Wrapper for [`fs::create_dir`](https://doc.rust-lang.org/stable/std/fs/fn.create_dir.html).
pub fn create_dir<P>(path: P) -> io::Result<()>
where
    P: AsRef<Path>,
{
    let path = path.as_ref();
    fs::create_dir(path).map_err(|source| Error::build(source, ErrorKind::CreateDir, path))
}

/// Wrapper for [`fs::create_dir_all`](https://doc.rust-lang.org/stable/std/fs/fn.create_dir_all.html).
pub fn create_dir_all<P>(path: P) -> io::Result<()>
where
    P: AsRef<Path>,
{
    let path = path.as_ref();
    fs::create_dir_all(path).map_err(|source| Error::build(source, ErrorKind::CreateDir, path))
}

/// Wrapper for [`fs::remove_dir`](https://doc.rust-lang.org/stable/std/fs/fn.remove_dir.html).
pub fn remove_dir<P>(path: P) -> io::Result<()>
where
    P: AsRef<Path>,
{
    let path = path.as_ref();
    fs::remove_dir(path).map_err(|source| Error::build(source, ErrorKind::RemoveDir, path))
}

/// Wrapper for [`fs::remove_dir_all`](https://doc.rust-lang.org/stable/std/fs/fn.remove_dir_all.html).
pub fn remove_dir_all<P>(path: P) -> io::Result<()>
where
    P: AsRef<Path>,
{
    let path = path.as_ref();
    fs::remove_dir_all(path).map_err(|source| Error::build(source, ErrorKind::RemoveDir, path))
}

/// Wrapper for [`fs::remove_file`](https://doc.rust-lang.org/stable/std/fs/fn.remove_file.html).
pub fn remove_file<P>(path: P) -> io::Result<()>
where
    P: AsRef<Path>,
{
    let path = path.as_ref();
    fs::remove_file(path).map_err(|source| Error::build(source, ErrorKind::RemoveFile, path))
}

/// Wrapper for [`fs::metadata`](https://doc.rust-lang.org/stable/std/fs/fn.metadata.html).
pub fn metadata<P: AsRef<Path>>(path: P) -> io::Result<fs::Metadata> {
    let path = path.as_ref();
    fs::metadata(path).map_err(|source| Error::build(source, ErrorKind::Metadata, path))
}

/// Wrapper for [`fs::canonicalize`](https://doc.rust-lang.org/stable/std/fs/fn.canonicalize.html).
pub fn canonicalize<P: AsRef<Path>>(path: P) -> io::Result<PathBuf> {
    let path = path.as_ref();
    fs::canonicalize(path).map_err(|source| Error::build(source, ErrorKind::Canonicalize, path))
}

/// Wrapper for [`fs::hard_link`](https://doc.rust-lang.org/stable/std/fs/fn.hard_link.html).
pub fn hard_link<P: AsRef<Path>, Q: AsRef<Path>>(src: P, dst: Q) -> io::Result<()> {
    let src = src.as_ref();
    let dst = dst.as_ref();
    fs::hard_link(src, dst)
        .map_err(|source| SourceDestError::build(source, SourceDestErrorKind::HardLink, src, dst))
}

/// Wrapper for [`fs::read_link`](https://doc.rust-lang.org/stable/std/fs/fn.read_link.html).
pub fn read_link<P: AsRef<Path>>(path: P) -> io::Result<PathBuf> {
    let path = path.as_ref();
    fs::read_link(path).map_err(|source| Error::build(source, ErrorKind::ReadLink, path))
}

/// Wrapper for [`fs::rename`](https://doc.rust-lang.org/stable/std/fs/fn.rename.html).
pub fn rename<P: AsRef<Path>, Q: AsRef<Path>>(from: P, to: Q) -> io::Result<()> {
    let from = from.as_ref();
    let to = to.as_ref();
    fs::rename(from, to)
        .map_err(|source| SourceDestError::build(source, SourceDestErrorKind::Rename, from, to))
}

/// Wrapper for [`fs::soft_link`](https://doc.rust-lang.org/stable/std/fs/fn.soft_link.html).
#[deprecated = "replaced with std::os::unix::fs::symlink and \
std::os::windows::fs::{symlink_file, symlink_dir}"]
pub fn soft_link<P: AsRef<Path>, Q: AsRef<Path>>(src: P, dst: Q) -> io::Result<()> {
    let src = src.as_ref();
    let dst = dst.as_ref();
    #[allow(deprecated)]
    fs::soft_link(src, dst)
        .map_err(|source| SourceDestError::build(source, SourceDestErrorKind::SoftLink, src, dst))
}

/// Wrapper for [`fs::symlink_metadata`](https://doc.rust-lang.org/stable/std/fs/fn.symlink_metadata.html).
pub fn symlink_metadata<P: AsRef<Path>>(path: P) -> io::Result<fs::Metadata> {
    let path = path.as_ref();
    fs::symlink_metadata(path)
        .map_err(|source| Error::build(source, ErrorKind::SymlinkMetadata, path))
}

/// Wrapper for [`fs::set_permissions`](https://doc.rust-lang.org/stable/std/fs/fn.set_permissions.html).
pub fn set_permissions<P: AsRef<Path>>(path: P, perm: fs::Permissions) -> io::Result<()> {
    let path = path.as_ref();
    fs::set_permissions(path, perm)
        .map_err(|source| Error::build(source, ErrorKind::SetPermissions, path))
}

fn initial_buffer_size(file: &std::fs::File) -> usize {
    file.metadata().map(|m| m.len() as usize + 1).unwrap_or(0)
}

pub(crate) use private::Sealed;
mod private {
    pub trait Sealed {}

    impl Sealed for crate::File {}
    impl Sealed for std::path::Path {}
    impl Sealed for crate::OpenOptions {}
}
