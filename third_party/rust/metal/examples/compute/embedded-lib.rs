// Copyright 2017 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

#[macro_use]
extern crate objc;

use metal::*;

use cocoa::foundation::NSAutoreleasePool;

fn main() {
    let library_data = include_bytes!("default.metallib");

    let pool = unsafe { NSAutoreleasePool::new(cocoa::base::nil) };
    let device = Device::system_default().expect("no device found");

    let library = device.new_library_with_data(&library_data[..]).unwrap();
    let kernel = library.get_function("sum", None).unwrap();

    println!("Function name: {}", kernel.name());
    println!("Function type: {:?}", kernel.function_type());
    println!("OK");

    unsafe {
        let () = msg_send![pool, release];
    }
}
