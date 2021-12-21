extern crate coremidi;

use std::env;
use std::thread;
use std::time::Duration;

fn main() {
    let destination_index = get_destination_index();
    println!("Destination index: {}", destination_index);

    let destination = coremidi::Destination::from_index(destination_index).unwrap();
    println!(
        "Destination display name: {}",
        destination.display_name().unwrap()
    );

    let client = coremidi::Client::new("Example Client").unwrap();
    let output_port = client.output_port("Example Port").unwrap();

    let note_on = create_note_on(0, 64, 127);
    let note_off = create_note_off(0, 64, 127);

    for i in 0..10 {
        println!("[{}] Sending note ...", i);

        output_port.send(&destination, &note_on).unwrap();

        thread::sleep(Duration::from_millis(1000));

        output_port.send(&destination, &note_off).unwrap();
    }
}

fn get_destination_index() -> usize {
    let mut args_iter = env::args();
    let tool_name = args_iter
        .next()
        .and_then(|path| {
            path.split(std::path::MAIN_SEPARATOR)
                .last()
                .map(|v| v.to_string())
        })
        .unwrap_or_else(|| "send".to_string());

    match args_iter.next() {
        Some(arg) => match arg.parse::<usize>() {
            Ok(index) => {
                if index >= coremidi::Destinations::count() {
                    println!("Destination index out of range: {}", index);
                    std::process::exit(-1);
                }
                index
            }
            Err(_) => {
                println!("Wrong destination index: {}", arg);
                std::process::exit(-1);
            }
        },
        None => {
            println!("Usage: {} <destination-index>", tool_name);
            println!();
            println!("Available Destinations:");
            print_destinations();
            std::process::exit(-1);
        }
    }
}

fn print_destinations() {
    for (i, destination) in coremidi::Destinations.into_iter().enumerate() {
        if let Some(display_name) = destination.display_name() {
            println!("[{}] {}", i, display_name)
        }
    }
}

fn create_note_on(channel: u8, note: u8, velocity: u8) -> coremidi::PacketBuffer {
    let data = &[0x90 | (channel & 0x0f), note & 0x7f, velocity & 0x7f];
    coremidi::PacketBuffer::new(0, data)
}

fn create_note_off(channel: u8, note: u8, velocity: u8) -> coremidi::PacketBuffer {
    let data = &[0x80 | (channel & 0x0f), note & 0x7f, velocity & 0x7f];
    coremidi::PacketBuffer::new(0, data)
}
