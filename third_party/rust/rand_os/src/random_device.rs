// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Helper functions to read from a random device such as `/dev/urandom`.
//!
//! All instances use a single internal file handle, to prevent possible
//! exhaustion of file descriptors.
use rand_core::{Error, ErrorKind};
use std::fs::File;
use std::io;
use std::io::Read;
use std::sync::{Once, Mutex, ONCE_INIT};

// TODO: remove outer Option when `Mutex::new(None)` is a constant expression
static mut READ_RNG_FILE: Option<Mutex<Option<File>>> = None;
static READ_RNG_ONCE: Once = ONCE_INIT;

#[allow(unused)]
pub fn open<F>(path: &'static str, open_fn: F) -> Result<(), Error>
    where F: Fn(&'static str) -> Result<File, io::Error>
{
    READ_RNG_ONCE.call_once(|| {
        unsafe { READ_RNG_FILE = Some(Mutex::new(None)) }
    });

    // We try opening the file outside the `call_once` fn because we cannot
    // clone the error, thus we must retry on failure.

    let mutex = unsafe { READ_RNG_FILE.as_ref().unwrap() };
    let mut guard = mutex.lock().unwrap();
    if (*guard).is_none() {
        info!("OsRng: opening random device {}", path);
        let file = open_fn(path).map_err(map_err)?;
        *guard = Some(file);
    };
    Ok(())
}

pub fn read(dest: &mut [u8]) -> Result<(), Error> {
    // We expect this function only to be used after `random_device::open`
    // was succesful. Therefore we can assume that our memory was set with a
    // valid object.
    let mutex = unsafe { READ_RNG_FILE.as_ref().unwrap() };
    let mut guard = mutex.lock().unwrap();
    let file = (*guard).as_mut().unwrap();

    // Use `std::io::read_exact`, which retries on `ErrorKind::Interrupted`.
    file.read_exact(dest).map_err(|err| {
        Error::with_cause(ErrorKind::Unavailable,
                          "error reading random device", err)
    })

}

pub fn map_err(err: io::Error) -> Error {
    match err.kind() {
        io::ErrorKind::Interrupted =>
                Error::new(ErrorKind::Transient, "interrupted"),
        io::ErrorKind::WouldBlock =>
                Error::with_cause(ErrorKind::NotReady,
                "OS RNG not yet seeded", err),
        _ => Error::with_cause(ErrorKind::Unavailable,
                "error while opening random device", err)
    }
}
