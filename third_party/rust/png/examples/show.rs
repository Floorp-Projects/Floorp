extern crate glium;
extern crate glob;
extern crate png;

use std::env;
use std::io;
use std::fs::File;
use std::borrow::Cow;
use std::path;
use std::error::Error;

use glium::{Surface, Rect, BlitTarget};
use glium::texture::{RawImage2d, ClientFormat};
use glium::glutin::{self, Event, VirtualKeyCode, dpi};
use glium::backend::glutin::Display;

/// Load the image using `png`
fn load_image(path: &path::PathBuf) -> io::Result<RawImage2d<'static, u8>> {
    use png::ColorType::*;
    let decoder = png::Decoder::new(try!(File::open(path)));
    let (info, mut reader) = try!(decoder.read_info());
    let mut img_data = vec![0; info.buffer_size()];
    try!(reader.next_frame(&mut img_data));

    let (data, format) = match info.color_type {
        RGB => (img_data, ClientFormat::U8U8U8),
        RGBA => (img_data, ClientFormat::U8U8U8U8),
        Grayscale => (
            {
                let mut vec = Vec::with_capacity(img_data.len()*3);
                for g in img_data {
                    vec.extend([g, g, g].iter().cloned())
                }
                vec
            },
            ClientFormat::U8U8U8
        ),
        GrayscaleAlpha => (
            {
                let mut vec = Vec::with_capacity(img_data.len()*3);
                for ga in img_data.chunks(2) {
                    let g = ga[0]; let a = ga[1];
                    vec.extend([g, g, g, a].iter().cloned())
                }
                vec
            },
            ClientFormat::U8U8U8U8
        ),
        _ => unreachable!("uncovered color type")
    };

    Ok(RawImage2d {
        data: Cow::Owned(data),
        width: info.width,
        height: info.height,
        format: format
    })
}

fn main_loop(files: Vec<path::PathBuf>) -> io::Result<()> {
    use glium::glutin::{KeyboardInput, WindowEvent};

    let mut files = files.iter();
    let image = try!(load_image(files.next().unwrap()));

    let mut events_loop = glutin::EventsLoop::new();
    let window = glutin::WindowBuilder::new();
    let context = glutin::ContextBuilder::new()
        .with_vsync(true);

    let display = Display::new(window, context, &events_loop)
        .map_err(|err| io::Error::new(
            io::ErrorKind::Other,
            err.description()
        ))?;
    // building the display, ie. the main object
    resize_window(&display, &image);
    let mut opengl_texture = glium::Texture2d::new(&display, image).unwrap();

    let mut stop = false;
    let mut res = Ok(());
    'main: loop {
        let frame = display.draw();
        fill_v_flipped(&opengl_texture.as_surface(), &frame, glium::uniforms::MagnifySamplerFilter::Linear);
        frame.finish().unwrap();

        // polling and handling the events received by the window
        events_loop.poll_events(|event| {
            if stop {return;}
            match event {
                Event::WindowEvent {event: WindowEvent::CloseRequested, ..} => {
                    stop = true;
                    return;
                }
                Event::WindowEvent {
                    event: WindowEvent::KeyboardInput {
                        input: KeyboardInput {
                            state: glutin::ElementState::Pressed,
                            virtual_keycode: code,
                            ..
                        },
                        ..
                    },
                    ..
                } => match code {
                    Some(VirtualKeyCode::Escape) => {
                        stop = true;
                        return;
                    }
                    Some(VirtualKeyCode::Right) => {
                        match files.next() {
                            Some(path) => {
                                let image = match load_image(path) {
                                    Ok(image) => image,
                                    Err(err) => {
                                        stop = true;
                                        res = Err(err);
                                        return;
                                    }
                                };
                                resize_window(&display, &image);
                                opengl_texture = glium::Texture2d::new(&display, image).unwrap();
                            },
                            None => {
                                stop = true;
                                return;
                            }
                        }
                    },
                    _ => ()
                },
                _ => ()
            }
        });

        if stop {break 'main;}
    }
    res
}

fn fill_v_flipped<S1, S2>(src: &S1, target: &S2, filter: glium::uniforms::MagnifySamplerFilter)
where S1: Surface, S2: Surface {
    let src_dim = src.get_dimensions();
    let src_rect = Rect { left: 0, bottom: 0, width: src_dim.0 as u32, height: src_dim.1 as u32 };
    let target_dim = target.get_dimensions();
    let target_rect = BlitTarget { left: 0, bottom: target_dim.1, width: target_dim.0 as i32, height: -(target_dim.1 as i32) };
    src.blit_color(&src_rect, target, &target_rect, filter);
}

fn resize_window(display: &Display, image: &RawImage2d<'static, u8>) {
    let mut width = image.width;
    let mut height = image.height;
    if width < 50 && height < 50 {
        width *= 10;
        height *= 10;
    } else if width < 5 && height < 5 {
        width *= 10;
        height *= 10;
    }
    display.gl_window().set_inner_size(dpi::LogicalSize::new(f64::from(width), f64::from(height)));
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        println!("Usage: show files [...]");
    } else {
        let mut files = vec![];
        for file in args.iter().skip(1) {
            match if file.contains("*") {
                (|| -> io::Result<_> {
                    for entry in try!(glob::glob(&file).map_err(|err| {
                        io::Error::new(io::ErrorKind::Other, err.msg)
                    })) {
                        files.push(try!(entry.map_err(|_| {
                            io::Error::new(io::ErrorKind::Other, "glob error")
                        })))
                    }
                    Ok(())
                })()
            } else {
                files.push(path::PathBuf::from(file));
                Ok(())
            } {
                Ok(_) => (),
                Err(err) => {
                    println!("{}: {}", file, err);
                    break
                }
            }

        }
        // "tests/pngsuite/pngsuite.png"
        match main_loop(files) {
            Ok(_) => (),
            Err(err) => println!("Error: {}", err)
        }
    }
}
