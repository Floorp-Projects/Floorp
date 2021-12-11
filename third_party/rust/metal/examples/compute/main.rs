// Copyright 2017 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use metal::*;
use objc::rc::autoreleasepool;
use std::mem;

fn main() {
    autoreleasepool(|| {
        let device = Device::system_default().expect("no device found");
        let command_queue = device.new_command_queue();

        let data = [
            1u32, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
            24, 25, 26, 27, 28, 29, 30,
        ];

        let buffer = device.new_buffer_with_data(
            unsafe { mem::transmute(data.as_ptr()) },
            (data.len() * mem::size_of::<u32>()) as u64,
            MTLResourceOptions::CPUCacheModeDefaultCache,
        );

        let sum = {
            let data = [0u32];
            device.new_buffer_with_data(
                unsafe { mem::transmute(data.as_ptr()) },
                (data.len() * mem::size_of::<u32>()) as u64,
                MTLResourceOptions::CPUCacheModeDefaultCache,
            )
        };

        let command_buffer = command_queue.new_command_buffer();

        command_buffer.set_label("label");
        let block = block::ConcreteBlock::new(move |buffer: &metal::CommandBufferRef| {
            println!("{}", buffer.label());
        })
        .copy();

        command_buffer.add_completed_handler(&block);

        let encoder = command_buffer.new_compute_command_encoder();
        let library_path = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("examples/compute/shaders.metallib");

        let library = device.new_library_with_file(library_path).unwrap();
        let kernel = library.get_function("sum", None).unwrap();

        let pipeline_state_descriptor = ComputePipelineDescriptor::new();
        pipeline_state_descriptor.set_compute_function(Some(&kernel));

        let pipeline_state = device
            .new_compute_pipeline_state_with_function(
                pipeline_state_descriptor.compute_function().unwrap(),
            )
            .unwrap();

        encoder.set_compute_pipeline_state(&pipeline_state);
        encoder.set_buffer(0, Some(&buffer), 0);
        encoder.set_buffer(1, Some(&sum), 0);

        let width = 16;

        let thread_group_count = MTLSize {
            width,
            height: 1,
            depth: 1,
        };

        let thread_group_size = MTLSize {
            width: (data.len() as u64 + width) / width,
            height: 1,
            depth: 1,
        };

        encoder.dispatch_thread_groups(thread_group_count, thread_group_size);
        encoder.end_encoding();
        command_buffer.commit();
        command_buffer.wait_until_completed();

        let ptr = sum.contents() as *mut u32;
        unsafe {
            assert_eq!(465, *ptr);
        }
    });
}
