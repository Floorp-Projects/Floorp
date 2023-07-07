extern crate objc;

use cocoa::{appkit::NSView, base::id as cocoa_id};
use core_graphics_types::geometry::CGSize;
use metal::*;
use objc::{rc::autoreleasepool, runtime::YES};
use std::mem;
use winit::{
    event::{Event, WindowEvent},
    event_loop::ControlFlow,
    platform::macos::WindowExtMacOS,
};

pub mod camera;
pub mod geometry;
pub mod renderer;
pub mod scene;

fn find_raytracing_supporting_device() -> Device {
    for device in Device::all() {
        if !device.supports_raytracing() {
            continue;
        }
        if device.is_low_power() {
            continue;
        }
        return device;
    }

    panic!("No device in this machine supports raytracing!")
}

fn main() {
    let events_loop = winit::event_loop::EventLoop::new();
    let size = winit::dpi::LogicalSize::new(800, 600);

    let window = winit::window::WindowBuilder::new()
        .with_inner_size(size)
        .with_title("Metal Raytracing Example".to_string())
        .build(&events_loop)
        .unwrap();

    let device = find_raytracing_supporting_device();

    let layer = MetalLayer::new();
    layer.set_device(&device);
    layer.set_pixel_format(MTLPixelFormat::RGBA16Float);
    layer.set_presents_with_transaction(false);

    unsafe {
        let view = window.ns_view() as cocoa_id;
        view.setWantsLayer(YES);
        view.setLayer(mem::transmute(layer.as_ref()));
    }

    let draw_size = window.inner_size();
    let cg_size = CGSize::new(draw_size.width as f64, draw_size.height as f64);
    layer.set_drawable_size(cg_size);

    let mut renderer = renderer::Renderer::new(device);
    renderer.window_resized(cg_size);

    events_loop.run(move |event, _, control_flow| {
        autoreleasepool(|| {
            *control_flow = ControlFlow::Poll;

            match event {
                Event::WindowEvent { event, .. } => match event {
                    WindowEvent::CloseRequested => *control_flow = ControlFlow::Exit,
                    WindowEvent::Resized(size) => {
                        let size = CGSize::new(size.width as f64, size.height as f64);
                        layer.set_drawable_size(size);
                        renderer.window_resized(size);
                    }
                    _ => (),
                },
                Event::MainEventsCleared => {
                    window.request_redraw();
                }
                Event::RedrawRequested(_) => {
                    renderer.draw(&layer);
                }
                _ => {}
            }
        });
    });
}
