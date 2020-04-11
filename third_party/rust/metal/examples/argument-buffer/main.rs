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
    let pool = unsafe { NSAutoreleasePool::new(cocoa::base::nil) };

    let device = Device::system_default().expect("no device found");

    let desc1 = ArgumentDescriptor::new();
    desc1.set_data_type(MTLDataType::Texture);
    let desc2 = ArgumentDescriptor::new();
    desc2.set_data_type(MTLDataType::Sampler);
    desc2.set_index(1);

    let encoder = device.new_argument_encoder(&Array::from_slice(&[desc1, desc2]));
    println!("{:?}", encoder);

    let buffer = device.new_buffer(encoder.encoded_length(), MTLResourceOptions::empty());
    encoder.set_argument_buffer(&buffer, 0);

    let sampler = {
        let descriptor = SamplerDescriptor::new();
        descriptor.set_support_argument_buffers(true);
        device.new_sampler(&descriptor)
    };
    encoder.set_sampler_state(1, &sampler);
    println!("{:?}", sampler);

    unsafe {
        let () = msg_send![pool, release];
    }
}
