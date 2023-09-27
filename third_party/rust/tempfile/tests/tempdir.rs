// Copyright 2013-2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![deny(rust_2018_idioms)]

use std::env;
use std::fs;
use std::path::Path;
use std::sync::mpsc::channel;
use std::thread;

use tempfile::{Builder, TempDir};

fn test_tempdir() {
    let path = {
        let p = Builder::new().prefix("foobar").tempdir_in(".").unwrap();
        let p = p.path();
        assert!(p.to_str().unwrap().contains("foobar"));
        p.to_path_buf()
    };
    assert!(!path.exists());
}

fn test_prefix() {
    let tmpfile = TempDir::with_prefix_in("prefix", ".").unwrap();
    let name = tmpfile.path().file_name().unwrap().to_str().unwrap();
    assert!(name.starts_with("prefix"));
}

fn test_customnamed() {
    let tmpfile = Builder::new()
        .prefix("prefix")
        .suffix("suffix")
        .rand_bytes(12)
        .tempdir()
        .unwrap();
    let name = tmpfile.path().file_name().unwrap().to_str().unwrap();
    assert!(name.starts_with("prefix"));
    assert!(name.ends_with("suffix"));
    assert_eq!(name.len(), 24);
}

fn test_rm_tempdir() {
    let (tx, rx) = channel();
    let f = move || {
        let tmp = TempDir::new().unwrap();
        tx.send(tmp.path().to_path_buf()).unwrap();
        panic!("panic to unwind past `tmp`");
    };
    let _ = thread::spawn(f).join();
    let path = rx.recv().unwrap();
    assert!(!path.exists());

    let tmp = TempDir::new().unwrap();
    let path = tmp.path().to_path_buf();
    let f = move || {
        let _tmp = tmp;
        panic!("panic to unwind past `tmp`");
    };
    let _ = thread::spawn(f).join();
    assert!(!path.exists());

    let path;
    {
        let f = move || TempDir::new().unwrap();

        let tmp = thread::spawn(f).join().unwrap();
        path = tmp.path().to_path_buf();
        assert!(path.exists());
    }
    assert!(!path.exists());

    let path;
    {
        let tmp = TempDir::new().unwrap();
        path = tmp.into_path();
    }
    assert!(path.exists());
    fs::remove_dir_all(&path).unwrap();
    assert!(!path.exists());
}

fn test_rm_tempdir_close() {
    let (tx, rx) = channel();
    let f = move || {
        let tmp = TempDir::new().unwrap();
        tx.send(tmp.path().to_path_buf()).unwrap();
        tmp.close().unwrap();
        panic!("panic when unwinding past `tmp`");
    };
    let _ = thread::spawn(f).join();
    let path = rx.recv().unwrap();
    assert!(!path.exists());

    let tmp = TempDir::new().unwrap();
    let path = tmp.path().to_path_buf();
    let f = move || {
        let tmp = tmp;
        tmp.close().unwrap();
        panic!("panic when unwinding past `tmp`");
    };
    let _ = thread::spawn(f).join();
    assert!(!path.exists());

    let path;
    {
        let f = move || TempDir::new().unwrap();

        let tmp = thread::spawn(f).join().unwrap();
        path = tmp.path().to_path_buf();
        assert!(path.exists());
        tmp.close().unwrap();
    }
    assert!(!path.exists());

    let path;
    {
        let tmp = TempDir::new().unwrap();
        path = tmp.into_path();
    }
    assert!(path.exists());
    fs::remove_dir_all(&path).unwrap();
    assert!(!path.exists());
}

fn dont_double_panic() {
    let r: Result<(), _> = thread::spawn(move || {
        let tmpdir = TempDir::new().unwrap();
        // Remove the temporary directory so that TempDir sees
        // an error on drop
        fs::remove_dir(tmpdir.path()).unwrap();
        // Panic. If TempDir panics *again* due to the rmdir
        // error then the process will abort.
        panic!();
    })
    .join();
    assert!(r.is_err());
}

fn in_tmpdir<F>(f: F)
where
    F: FnOnce(),
{
    let tmpdir = TempDir::new().unwrap();
    assert!(env::set_current_dir(tmpdir.path()).is_ok());

    f();
}

fn pass_as_asref_path() {
    let tempdir = TempDir::new().unwrap();
    takes_asref_path(&tempdir);

    fn takes_asref_path<T: AsRef<Path>>(path: T) {
        let path = path.as_ref();
        assert!(path.exists());
    }
}

#[test]
fn main() {
    in_tmpdir(test_tempdir);
    in_tmpdir(test_prefix);
    in_tmpdir(test_customnamed);
    in_tmpdir(test_rm_tempdir);
    in_tmpdir(test_rm_tempdir_close);
    in_tmpdir(dont_double_panic);
    in_tmpdir(pass_as_asref_path);
}
