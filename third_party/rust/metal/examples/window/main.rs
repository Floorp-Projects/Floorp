// Copyright 2016 metal-rs developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

extern crate objc;

use cocoa::{
    appkit::{NSView},
    base::id as cocoa_id,
    foundation::{NSRange},
};

use core_graphics::geometry::CGSize;
use objc::runtime::YES;
use metal::*;
use winit::platform::macos::WindowExtMacOS;
use std::mem;

use winit::{
    event::{
        Event, WindowEvent,
    },
    event_loop::ControlFlow
};

fn prepare_pipeline_state<'a>(device: &DeviceRef, library: &LibraryRef) -> RenderPipelineState {
    let vert = library.get_function("triangle_vertex", None).unwrap();
    let frag = library.get_function("triangle_fragment", None).unwrap();

    let pipeline_state_descriptor = RenderPipelineDescriptor::new();
    pipeline_state_descriptor.set_vertex_function(Some(&vert));
    pipeline_state_descriptor.set_fragment_function(Some(&frag));
    pipeline_state_descriptor
        .color_attachments()
        .object_at(0)
        .unwrap()
        .set_pixel_format(MTLPixelFormat::BGRA8Unorm);

    device
        .new_render_pipeline_state(&pipeline_state_descriptor)
        .unwrap()
}

fn prepare_render_pass_descriptor(descriptor: &RenderPassDescriptorRef, texture: &TextureRef) {
    //descriptor.color_attachments().set_object_at(0, MTLRenderPassColorAttachmentDescriptor::alloc());
    //let color_attachment: MTLRenderPassColorAttachmentDescriptor = unsafe { msg_send![descriptor.color_attachments().0, _descriptorAtIndex:0] };//descriptor.color_attachments().object_at(0);
    let color_attachment = descriptor.color_attachments().object_at(0).unwrap();

    color_attachment.set_texture(Some(texture));
    color_attachment.set_load_action(MTLLoadAction::Clear);
    color_attachment.set_clear_color(MTLClearColor::new(0.5, 0.2, 0.2, 1.0));
    color_attachment.set_store_action(MTLStoreAction::Store);
}

fn main() {
    let events_loop = winit::event_loop::EventLoop::new();
    let size = winit::dpi::LogicalSize::new(800, 600);

    let window = winit::window::WindowBuilder::new()
        .with_inner_size(size)
        .with_title("Metal".to_string())
        .build(&events_loop)
        .unwrap();

    let device = Device::system_default().expect("no device found");

    let layer = CoreAnimationLayer::new();
    layer.set_device(&device);
    layer.set_pixel_format(MTLPixelFormat::BGRA8Unorm);
    layer.set_presents_with_transaction(false);

    unsafe {
        let view = window.ns_view() as cocoa_id;
        view.setWantsLayer(YES);
        view.setLayer(mem::transmute(layer.as_ref()));
    }

    let draw_size = window.inner_size();
    layer.set_drawable_size(CGSize::new(draw_size.width as f64, draw_size.height as f64));

    let library = device
        .new_library_with_file("examples/window/shaders.metallib")
        .unwrap();
    let pipeline_state = prepare_pipeline_state(&device, &library);
    let command_queue = device.new_command_queue();
    //let nc: () = msg_send![command_queue.0, setExecutionEnabled:true];

    let vbuf = {
        let vertex_data = [
            0.0f32, 0.5, 1.0, 0.0, 0.0, -0.5, -0.5, 0.0, 1.0, 0.0, 0.5, 0.5, 0.0, 0.0, 1.0,
        ];

        device.new_buffer_with_data(
            vertex_data.as_ptr() as *const _,
            (vertex_data.len() * mem::size_of::<f32>()) as u64,
            MTLResourceOptions::CPUCacheModeDefaultCache | MTLResourceOptions::StorageModeManaged,
        )
    };

    let mut r = 0.0f32;

    events_loop.run(move |event, _, control_flow| {
        *control_flow = ControlFlow::Poll;

        match event {
            Event::WindowEvent { event, .. } => match event {
                WindowEvent::CloseRequested => *control_flow = ControlFlow::Exit,
                WindowEvent::Resized(size) => {
                    layer.set_drawable_size(CGSize::new(size.width as f64, size.height as f64));
                }
                _ => ()
            }
            Event::MainEventsCleared => {
                window.request_redraw();
            }
            Event::RedrawRequested(_) => {
                let drawable = match layer.next_drawable() {
                    Some(drawable) => drawable,
                    None => return,
                };

                let render_pass_descriptor = RenderPassDescriptor::new();
                let _a = prepare_render_pass_descriptor(&render_pass_descriptor, drawable.texture());

                let command_buffer = command_queue.new_command_buffer();
                let encoder = command_buffer.new_render_command_encoder(&render_pass_descriptor);
                encoder.set_render_pipeline_state(&pipeline_state);
                encoder.set_vertex_buffer(0, Some(&vbuf), 0);
                encoder.draw_primitives(MTLPrimitiveType::Triangle, 0, 3);
                encoder.end_encoding();

                render_pass_descriptor
                    .color_attachments()
                    .object_at(0)
                    .unwrap()
                    .set_load_action(MTLLoadAction::DontCare);

                let encoder = command_buffer.new_render_command_encoder(&render_pass_descriptor);
                let p = vbuf.contents();
                let vertex_data = [
                    0.0f32,
                    0.5,
                    1.0,
                    0.0 - r,
                    0.0,
                    -0.5,
                    -0.5,
                    0.0,
                    1.0 - r,
                    0.0,
                    0.5,
                    0.5,
                    0.0,
                    0.0,
                    1.0 + r,
                ];

                unsafe {
                    std::ptr::copy(
                        vertex_data.as_ptr(),
                        p as *mut f32,
                        (vertex_data.len() * mem::size_of::<f32>()) as usize,
                    );
                }
                vbuf.did_modify_range(NSRange::new(
                    0 as u64,
                    (vertex_data.len() * mem::size_of::<f32>()) as u64,
                ));

                encoder.set_render_pipeline_state(&pipeline_state);
                encoder.set_vertex_buffer(0, Some(&vbuf), 0);
                encoder.draw_primitives(MTLPrimitiveType::Triangle, 0, 3);
                encoder.end_encoding();

                command_buffer.present_drawable(&drawable);
                command_buffer.commit();

                r += 0.01f32;
            },
            _ => {}
        }
    });
}