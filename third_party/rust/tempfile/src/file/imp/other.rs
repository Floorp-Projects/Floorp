use std::path::Path;
use std::io;
use std::fs::File;

fn not_supported<T>() -> io::Result<T> {
    Err(io::Error::new(io::ErrorKind::Other, "operation not supported on this platform"))
}

pub fn create_named(_path: &Path) -> io::Result<File> {
    not_supported()
}

pub fn create(_dir: &Path) -> io::Result<File> {
    not_supported()
}

pub fn reopen(_file: &File, _path: &Path) -> io::Result<File> {
    not_supported()
}

pub fn persist(_old_path: &Path, _new_path: &Path, _overwrite: bool) -> io::Result<()> {
    not_supported()
}
