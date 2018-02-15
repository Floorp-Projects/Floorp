// Copyright 2015, Igor Shaula
// Licensed under the MIT License <LICENSE or
// http://opensource.org/licenses/MIT>. This file
// may not be copied, modified, or distributed
// except according to those terms.
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
        println!("Transaction commited.");
    }
    else {
        // this is optional, if transaction wasn't commited,
        // it will be rolled back on disposal
        t.rollback().unwrap();

        println!("Transaction wasn't commited, it will be rolled back.");
    }
}
