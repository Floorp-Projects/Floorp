// In principle, this is what you need to run puffin:
// fn main() {
//     // Create the UI
//     let mut profiler_ui = puffin_imgui::ProfilerUi::default();
//
//     // Enable it
//     puffin::set_scopes_on(true);
//
//     loop {
//         profiling::scope!("Scope Name Here!");
//
//         // Draw the UI
//         profiler_ui.window(ui);
//
//         //
//         puffin::GlobalProfiler::lock().new_frame();
//     }
// }
//

mod imgui_support;
use imgui_support::ImguiManager;

mod renderer;
use renderer::Renderer;

fn update() {
    profiling::scope!("update");
    some_function();
    some_other_function(5);
}

fn draw(
    imgui_manager: &ImguiManager,
    profiler_ui: &mut puffin_imgui::ProfilerUi,
) {
    profiling::scope!("draw");
    //
    //Draw an inspect window for the example struct
    //
    imgui_manager.with_ui(|ui| {
        profiler_ui.window(ui);
    });
}

// Creates a window and runs the event loop.
fn main() {
    // Setup logging
    env_logger::Builder::from_default_env()
        .filter_level(log::LevelFilter::Debug)
        .init();

    // Enable puffin
    puffin::set_scopes_on(true);

    // Create the winit event loop
    let event_loop = winit::event_loop::EventLoop::<()>::with_user_event();

    // Set up the coordinate system to be fixed at 900x600, and use this as the default window size
    // This means the drawing code can be written as though the window is always 900x600. The
    // output will be automatically scaled so that it's always visible.
    let logical_size = winit::dpi::LogicalSize::new(900.0, 600.0);

    // Create a single window
    let window = winit::window::WindowBuilder::new()
        .with_title("Profiling Demo")
        .with_inner_size(logical_size)
        .build(&event_loop)
        .expect("Failed to create window");

    // Initialize imgui
    let imgui_manager = imgui_support::init_imgui_manager(&window);

    let renderer = Renderer::new(&window, imgui_manager.font_atlas_texture());

    // Check if there were errors setting up vulkan
    if let Err(e) = renderer {
        println!("Error during renderer construction: {:?}", e);
        return;
    }

    let mut renderer = renderer.unwrap();

    let mut profiler_ui = puffin_imgui::ProfilerUi::default();

    // Start the window event loop. Winit will not return once run is called. We will get notified
    // when important events happen.
    event_loop.run(move |event, _window_target, control_flow| {
        imgui_manager.handle_event(&window, &event);

        match event {
            //
            // Halt if the user requests to close the window
            //
            winit::event::Event::WindowEvent {
                event: winit::event::WindowEvent::CloseRequested,
                ..
            } => *control_flow = winit::event_loop::ControlFlow::Exit,

            //
            // Close if the escape key is hit
            //
            winit::event::Event::WindowEvent {
                event:
                    winit::event::WindowEvent::KeyboardInput {
                        input:
                            winit::event::KeyboardInput {
                                virtual_keycode: Some(winit::event::VirtualKeyCode::Escape),
                                ..
                            },
                        ..
                    },
                ..
            } => *control_flow = winit::event_loop::ControlFlow::Exit,

            //
            // Request a redraw any time we finish processing events
            //
            winit::event::Event::MainEventsCleared => {
                update();

                // Queue a RedrawRequested event.
                window.request_redraw();
            }

            //
            // Redraw
            //
            winit::event::Event::RedrawRequested(_window_id) => {
                imgui_manager.begin_frame(&window);
                draw(&imgui_manager, &mut profiler_ui);
                imgui_manager.render(&window);

                if let Err(e) = renderer.draw(&window, imgui_manager.draw_data()) {
                    println!("Error during draw: {:?}", e);
                    *control_flow = winit::event_loop::ControlFlow::Exit
                }

                profiling::finish_frame!();
            }

            //
            // Ignore all other events
            //
            _ => {}
        }
    });
}

fn burn_time(micros: u128) {
    let start_time = std::time::Instant::now();
    loop {
        if (std::time::Instant::now() - start_time).as_micros() > micros {
            break;
        }
    }
}

// This `profiling::function` attribute is equivalent to profiling::scope!(function_name)
#[profiling::function]
fn some_function() {
    burn_time(10000);
}

fn some_other_function(iterations: usize) {
    profiling::scope!("some_other_function");
    burn_time(400);

    {
        profiling::scope!("do iterations");
        for i in 0..iterations {
            profiling::scope!(
                "some_inner_function_that_sleeps",
                format!("other data {}", i).as_str()
            );
            some_inner_function(i);
            burn_time(400);
        }
    }
}

#[profiling::function]
fn some_inner_function(_iteration_index: usize) {
    burn_time(200);
}
