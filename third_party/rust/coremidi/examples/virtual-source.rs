extern crate coremidi;

use std::thread;
use std::time::Duration;

fn main() {
    let client = coremidi::Client::new("Example Client").unwrap();
    let source = client.virtual_source("Example Source").unwrap();

    let note_on = create_note_on(0, 64, 127);
    let note_off = create_note_off(0, 64, 127);

    for i in 0..10 {
        println!("[{}] Sending note...", i);

        source.received(&note_on).unwrap();

        thread::sleep(Duration::from_millis(1000));

        source.received(&note_off).unwrap();
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
