use metal::*;

use winit::{
    event::{Event, WindowEvent},
    event_loop::{ControlFlow, EventLoop},
    platform::macos::WindowExtMacOS,
};

use cocoa::{appkit::NSView, base::id as cocoa_id};
use core_graphics_types::geometry::CGSize;

use objc::{rc::autoreleasepool, runtime::YES};

use std::mem;

// Declare the data structures needed to carry vertex layout to
// metal shading language(MSL) program. Use #[repr(C)], to make
// the data structure compatible with C++ type data structure
// for vertex defined in MSL program as MSL program is broadly
// based on C++
#[repr(C)]
#[derive(Debug)]
pub struct position(cty::c_float, cty::c_float);
#[repr(C)]
#[derive(Debug)]
pub struct color(cty::c_float, cty::c_float, cty::c_float);
#[repr(C)]
#[derive(Debug)]
pub struct AAPLVertex {
    p: position,
    c: color,
}

fn main() {
    // Create a window for viewing the content
    let event_loop = EventLoop::new();
    let events_loop = winit::event_loop::EventLoop::new();
    let size = winit::dpi::LogicalSize::new(800, 600);

    let window = winit::window::WindowBuilder::new()
        .with_inner_size(size)
        .with_title("Metal".to_string())
        .build(&events_loop)
        .unwrap();

    // Set up the GPU device found in the system
    let device = Device::system_default().expect("no device found");
    println!("Your device is: {}", device.name(),);

    let binary_archive_path = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("examples/circle/binary_archive.metallib");

    let binary_archive_url =
        URL::new_with_string(&format!("file://{}", binary_archive_path.display()));

    let binary_archive_descriptor = BinaryArchiveDescriptor::new();
    if binary_archive_path.exists() {
        binary_archive_descriptor.set_url(&binary_archive_url);
    }

    // Set up a binary archive to cache compiled shaders.
    let binary_archive = device
        .new_binary_archive_with_descriptor(&binary_archive_descriptor)
        .unwrap();

    let library_path = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("examples/circle/shaders.metallib");

    // Use the metallib file generated out of .metal shader file
    let library = device.new_library_with_file(library_path).unwrap();

    // The render pipeline generated from the vertex and fragment shaders in the .metal shader file.
    let pipeline_state = prepare_pipeline_state(&device, &library, &binary_archive);

    // Serialize the binary archive to disk.
    binary_archive
        .serialize_to_url(&binary_archive_url)
        .unwrap();

    // Set the command queue used to pass commands to the device.
    let command_queue = device.new_command_queue();

    // Currently, MetalLayer is the only interface that provide
    // layers to carry drawable texture from GPU rendaring through metal
    // library to viewable windows.
    let layer = MetalLayer::new();
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

    let vbuf = {
        let vertex_data = create_vertex_points_for_circle();
        let vertex_data = vertex_data.as_slice();

        device.new_buffer_with_data(
            vertex_data.as_ptr() as *const _,
            (vertex_data.len() * mem::size_of::<AAPLVertex>()) as u64,
            MTLResourceOptions::CPUCacheModeDefaultCache | MTLResourceOptions::StorageModeManaged,
        )
    };

    event_loop.run(move |event, _, control_flow| {
        autoreleasepool(|| {
            // ControlFlow::Wait pauses the event loop if no events are available to process.
            // This is ideal for non-game applications that only update in response to user
            // input, and uses significantly less power/CPU time than ControlFlow::Poll.
            *control_flow = ControlFlow::Wait;

            match event {
                Event::WindowEvent {
                    event: WindowEvent::CloseRequested,
                    ..
                } => {
                    println!("The close button was pressed; stopping");
                    *control_flow = ControlFlow::Exit
                }
                Event::MainEventsCleared => {
                    // Queue a RedrawRequested event.
                    window.request_redraw();
                }
                Event::RedrawRequested(_) => {
                    // It's preferrable to render in this event rather than in MainEventsCleared, since
                    // rendering in here allows the program to gracefully handle redraws requested
                    // by the OS.
                    let drawable = match layer.next_drawable() {
                        Some(drawable) => drawable,
                        None => return,
                    };

                    // Create a new command buffer for each render pass to the current drawable
                    let command_buffer = command_queue.new_command_buffer();

                    // Obtain a renderPassDescriptor generated from the view's drawable textures.
                    let render_pass_descriptor = RenderPassDescriptor::new();
                    prepare_render_pass_descriptor(&render_pass_descriptor, drawable.texture());

                    // Create a render command encoder.
                    let encoder =
                        command_buffer.new_render_command_encoder(&render_pass_descriptor);
                    encoder.set_render_pipeline_state(&pipeline_state);
                    // Pass in the parameter data.
                    encoder.set_vertex_buffer(0, Some(&vbuf), 0);
                    // Draw the triangles which will eventually form the circle.
                    encoder.draw_primitives(MTLPrimitiveType::TriangleStrip, 0, 1080);
                    encoder.end_encoding();

                    // Schedule a present once the framebuffer is complete using the current drawable.
                    command_buffer.present_drawable(&drawable);

                    // Finalize rendering here & push the command buffer to the GPU.
                    command_buffer.commit();
                }
                _ => (),
            }
        });
    });
}

// If we want to draw a circle, we need to draw it out of the three primitive
// types available with metal framework. Triangle is used in this case to form
// the circle. If we consider a circle to be total of 360 degree at center, we
// can form small triangle with one point at origin and two points at the
// perimeter of the circle for each degree. Eventually, if we can take enough
// triangle virtices for total of 360 degree, the triangles together will
// form a circle. This function captures the triangle vertices for each degree
// and push the co-ordinates of the vertices to a rust vector
fn create_vertex_points_for_circle() -> Vec<AAPLVertex> {
    let mut v: Vec<AAPLVertex> = Vec::new();
    let origin_x: f32 = 0.0;
    let origin_y: f32 = 0.0;

    // Size of the circle
    let circle_size = 0.8f32;

    for i in 0..720 {
        let y = i as f32;
        // Get the X co-ordinate of each point on the perimeter of circle
        let position_x: f32 = y.to_radians().cos() * 100.0;
        let position_x: f32 = position_x.trunc() / 100.0;
        // Set the size of the circle
        let position_x: f32 = position_x * circle_size;
        // Get the Y co-ordinate of each point on the perimeter of circle
        let position_y: f32 = y.to_radians().sin() * 100.0;
        let position_y: f32 = position_y.trunc() / 100.0;
        // Set the size of the circle
        let position_y: f32 = position_y * circle_size;

        v.push(AAPLVertex {
            p: position(position_x, position_y),
            c: color(0.7, 0.3, 0.5),
        });

        if (i + 1) % 2 == 0 {
            // For each two points on perimeter, push one point of origin
            v.push(AAPLVertex {
                p: position(origin_x, origin_y),
                c: color(0.2, 0.7, 0.4),
            });
        }
    }

    v
}

fn prepare_render_pass_descriptor(descriptor: &RenderPassDescriptorRef, texture: &TextureRef) {
    let color_attachment = descriptor.color_attachments().object_at(0).unwrap();

    color_attachment.set_texture(Some(texture));
    color_attachment.set_load_action(MTLLoadAction::Clear);
    // Setting a background color
    color_attachment.set_clear_color(MTLClearColor::new(0.5, 0.5, 0.8, 1.0));
    color_attachment.set_store_action(MTLStoreAction::Store);
}

fn prepare_pipeline_state(
    device: &Device,
    library: &Library,
    binary_archive: &BinaryArchive,
) -> RenderPipelineState {
    let vert = library.get_function("vs", None).unwrap();
    let frag = library.get_function("ps", None).unwrap();

    let pipeline_state_descriptor = RenderPipelineDescriptor::new();
    pipeline_state_descriptor.set_vertex_function(Some(&vert));
    pipeline_state_descriptor.set_fragment_function(Some(&frag));
    pipeline_state_descriptor
        .color_attachments()
        .object_at(0)
        .unwrap()
        .set_pixel_format(MTLPixelFormat::BGRA8Unorm);
    // Set the binary archives to search for a cached pipeline in.
    pipeline_state_descriptor.set_binary_archives(&[binary_archive]);

    // Add the pipeline descriptor to the binary archive cache.
    binary_archive
        .add_render_pipeline_functions_with_descriptor(&pipeline_state_descriptor)
        .unwrap();

    device
        .new_render_pipeline_state(&pipeline_state_descriptor)
        .unwrap()
}
