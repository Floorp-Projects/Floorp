use rand;
use rand::RngCore;
use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::{io, iter};

use error::IoResultExt;

fn tmpname(prefix: &str, suffix: &str, rand_len: usize) -> OsString {
    let mut buf = String::with_capacity(prefix.len() + suffix.len() + rand_len);
    buf.push_str(prefix);
    buf.extend(iter::repeat('X').take(rand_len));
    buf.push_str(suffix);

    // Randomize.
    unsafe {
        // We guarantee utf8.
        let bytes = &mut buf.as_mut_vec()[prefix.len()..prefix.len() + rand_len];
        rand::thread_rng().fill_bytes(bytes);
        for byte in bytes.iter_mut() {
            *byte = match *byte % 62 {
                v @ 0...9 => (v + b'0'),
                v @ 10...35 => (v - 10 + b'a'),
                v @ 36...61 => (v - 36 + b'A'),
                _ => unreachable!(),
            }
        }
    }
    OsString::from(buf)
}

pub fn create_helper<F, R>(
    base: &Path,
    prefix: &str,
    suffix: &str,
    random_len: usize,
    f: F,
) -> io::Result<R>
where
    F: Fn(PathBuf) -> io::Result<R>,
{
    let num_retries = if random_len != 0 { ::NUM_RETRIES } else { 1 };

    for _ in 0..num_retries {
        let path = base.join(tmpname(prefix, suffix, random_len));
        return match f(path) {
            Err(ref e) if e.kind() == io::ErrorKind::AlreadyExists => continue,
            res => res,
        };
    }

    Err(io::Error::new(
        io::ErrorKind::AlreadyExists, 
        "too many temporary files exist"))
        .with_err_path(|| base)
}
