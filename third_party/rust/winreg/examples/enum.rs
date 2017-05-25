// Copyright 2015, Igor Shaula
// Licensed under the MIT License <LICENSE or
// http://opensource.org/licenses/MIT>. This file
// may not be copied, modified, or distributed
// except according to those terms.
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
