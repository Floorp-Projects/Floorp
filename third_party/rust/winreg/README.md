winreg [![Crates.io](https://img.shields.io/crates/v/winreg.svg)](https://crates.io/crates/winreg)
======

Rust bindings to MS Windows Registry API. Work in progress.

Current features:
* Basic registry operations:
    * open/create/delete keys
    * read and write values
    * seamless conversion between `REG_*` types and rust primitives
        * `String` and `OsString` <= `REG_SZ`, `REG_EXPAND_SZ` or `REG_MULTI_SZ`
        * `String`, `&str` and `OsStr` => `REG_SZ`
        * `u32` <=> `REG_DWORD`
        * `u64` <=> `REG_QWORD`
* Iteration through key names and through values
* Transactions
* Transacted serialization of rust types into/from registry (only primitives and structures for now)

## Usage

### Basic usage

```toml
# Cargo.toml
[dependencies]
winreg = "0.3"
```

```rust
extern crate winreg;
use std::path::Path;
use std::io;
use winreg::RegKey;
use winreg::enums::*;

fn main() {
    println!("Reading some system info...");
    let hklm = RegKey::predef(HKEY_LOCAL_MACHINE);
    let cur_ver = hklm.open_subkey_with_flags("SOFTWARE\\Microsoft\\Windows\\CurrentVersion",
        KEY_READ).unwrap();
    let pf: String = cur_ver.get_value("ProgramFilesDir").unwrap();
    let dp: String = cur_ver.get_value("DevicePath").unwrap();
    println!("ProgramFiles = {}\nDevicePath = {}", pf, dp);
    let info = cur_ver.query_info().unwrap();
    println!("info = {:?}", info);

    println!("And now lets write something...");
    let hkcu = RegKey::predef(HKEY_CURRENT_USER);
    let path = Path::new("Software").join("WinregRsExample1");
    let key = hkcu.create_subkey(&path).unwrap();

    key.set_value("TestSZ", &"written by Rust").unwrap();
    let sz_val: String = key.get_value("TestSZ").unwrap();
    key.delete_value("TestSZ").unwrap();
    println!("TestSZ = {}", sz_val);

    key.set_value("TestDWORD", &1234567890u32).unwrap();
    let dword_val: u32 = key.get_value("TestDWORD").unwrap();
    println!("TestDWORD = {}", dword_val);

    key.set_value("TestQWORD", &1234567891011121314u64).unwrap();
    let qword_val: u64 = key.get_value("TestQWORD").unwrap();
    println!("TestQWORD = {}", qword_val);

    key.create_subkey("sub\\key").unwrap();
    hkcu.delete_subkey_all(&path).unwrap();

    println!("Trying to open nonexistent key...");
    let key2 = hkcu.open_subkey(&path)
    .unwrap_or_else(|e| match e.kind() {
        io::ErrorKind::NotFound => panic!("Key doesn't exist"),
        io::ErrorKind::PermissionDenied => panic!("Access denied"),
        _ => panic!("{:?}", e)
    });
}
```

### Iterators

```rust
extern crate winreg;
use winreg::RegKey;
use winreg::enums::*;

fn main() {
    println!("File extensions, registered in system:");
    for i in RegKey::predef(HKEY_CLASSES_ROOT)
        .enum_keys().map(|x| x.unwrap())
        .filter(|x| x.starts_with("."))
    {
        println!("{}", i);
    }

    let system = RegKey::predef(HKEY_LOCAL_MACHINE)
        .open_subkey_with_flags("HARDWARE\\DESCRIPTION\\System", KEY_READ)
        .unwrap();
    for (name, value) in system.enum_values().map(|x| x.unwrap()) {
        println!("{} = {:?}", name, value);
    }
}
```

### Transactions

```rust
extern crate winreg;
use std::io;
use winreg::RegKey;
use winreg::enums::*;
use winreg::transaction::Transaction;

fn main() {
    let t = Transaction::new().unwrap();
    let hkcu = RegKey::predef(HKEY_CURRENT_USER);
    let key = hkcu.create_subkey_transacted("Software\\RustTransaction", &t).unwrap();
    key.set_value("TestQWORD", &1234567891011121314u64).unwrap();
    key.set_value("TestDWORD", &1234567890u32).unwrap();

    println!("Commit transaction? [y/N]:");
    let mut input = String::new();
    io::stdin().read_line(&mut input).unwrap();
    input = input.trim_right().to_owned();
    if input == "y" || input == "Y" {
        t.commit().unwrap();
        println!("Transaction committed.");
    }
    else {
        // this is optional, if transaction wasn't committed,
        // it will be rolled back on disposal
        t.rollback().unwrap();

        println!("Transaction wasn't committed, it will be rolled back.");
    }
}
```

### Serialization

```rust
extern crate rustc_serialize;
extern crate winreg;
use winreg::enums::*;

#[derive(Debug,RustcEncodable,RustcDecodable,PartialEq)]
struct Rectangle{
    x: u32,
    y: u32,
    w: u32,
    h: u32,
}

#[derive(Debug,RustcEncodable,RustcDecodable,PartialEq)]
struct Test {
    t_bool: bool,
    t_u8: u8,
    t_u16: u16,
    t_u32: u32,
    t_u64: u64,
    t_usize: usize,
    t_struct: Rectangle,
    t_string: String,
    t_i8: i8,
    t_i16: i16,
    t_i32: i32,
    t_i64: i64,
    t_isize: isize,
    t_f64: f64,
    t_f32: f32,
}

fn main() {
    let hkcu = winreg::RegKey::predef(HKEY_CURRENT_USER);
    let key = hkcu.create_subkey("Software\\RustEncode").unwrap();
    let v1 = Test{
        t_bool: false,
        t_u8: 127,
        t_u16: 32768,
        t_u32: 123456789,
        t_u64: 123456789101112,
        t_usize: 123456789101112,
        t_struct: Rectangle{
            x: 55,
            y: 77,
            w: 500,
            h: 300,
        },
        t_string: "test 123!".to_owned(),
        t_i8: -123,
        t_i16: -2049,
        t_i32: 20100,
        t_i64: -12345678910,
        t_isize: -1234567890,
        t_f64: -0.01,
        t_f32: 3.14,
    };

    key.encode(&v1).unwrap();

    let v2: Test = key.decode().unwrap();
    println!("Decoded {:?}", v2);

    // This shows `false` because f32 and f64 encoding/decoding is NOT precise
    println!("Equal to encoded: {:?}", v1 == v2);
}
```

## Changelog

### 0.3.5

* Implement `FromRegValue` for `OsString` and `ToRegValue` for `OsStr` (#8)
* Minor fixes

### 0.3.4

* Add `copy_tree` method to `RegKey`
* Now checked with [rust-clippy](https://github.com/Manishearth/rust-clippy)
    * no more `unwrap`s
    * replaced `to_string` with `to_owned`
* Fix: reading strings longer than 2048 characters (#6)

### 0.3.3

* Fix: now able to read values longer than 2048 bytes (#3)

### 0.3.2

* Fix: `FromRegValue` trait now requires `Sized` (fixes build with rust 1.4)

### 0.3.1

* Fix: bump `winapi` version to fix build

### 0.3.0

* Add transactions support and make serialization transacted
* Breaking change: use `std::io::{Error,Result}` instead of own `RegError` and `RegResult`
