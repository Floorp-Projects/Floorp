//! This file contains automated tests, but they require virtual ports and therefore can't work on Windows or Web MIDI ...
#![cfg(not(any(windows, target_arch = "wasm32")))]
extern crate midir;

use std::thread::sleep;
use std::time::Duration;

use midir::{MidiInput, MidiOutput, Ignore, MidiOutputPort};
use midir::os::unix::{VirtualInput, VirtualOutput};

#[test]
fn end_to_end() {
    let mut midi_in = MidiInput::new("My Test Input").unwrap();
    midi_in.ignore(Ignore::None);
    let midi_out = MidiOutput::new("My Test Output").unwrap();
    
    let previous_count = midi_out.port_count();
    
    println!("Creating virtual input port ...");
    let conn_in = midi_in.create_virtual("midir-test", |stamp, message, _| {
        println!("{}: {:?} (len = {})", stamp, message, message.len());
    }, ()).unwrap();
    
    assert_eq!(midi_out.port_count(), previous_count + 1);

    let new_port: MidiOutputPort = midi_out.ports().into_iter().rev().next().unwrap();

    println!("Connecting to port '{}' ...", midi_out.port_name(&new_port).unwrap());
    let mut conn_out = midi_out.connect(&new_port, "midir-test").unwrap();
    println!("Starting to send messages ...");
    conn_out.send(&[144, 60, 1]).unwrap();
    sleep(Duration::from_millis(200));
    conn_out.send(&[144, 60, 0]).unwrap();
    sleep(Duration::from_millis(50));
    conn_out.send(&[144, 60, 1]).unwrap();
    sleep(Duration::from_millis(200));
    conn_out.send(&[144, 60, 0]).unwrap();
    sleep(Duration::from_millis(50));
    println!("Closing output ...");
    let midi_out = conn_out.close();
    println!("Closing virtual input ...");
    let midi_in = conn_in.close().0;
    assert_eq!(midi_out.port_count(), previous_count);
    
    let previous_count = midi_in.port_count();
    
    // reuse midi_in and midi_out, but swap roles
    println!("\nCreating virtual output port ...");
    let mut conn_out = midi_out.create_virtual("midir-test").unwrap();
    assert_eq!(midi_in.port_count(), previous_count + 1);

    let new_port = midi_in.ports().into_iter().rev().next().unwrap();
    
    println!("Connecting to port '{}' ...", midi_in.port_name(&new_port).unwrap());
    let conn_in = midi_in.connect(&new_port, "midir-test", |stamp, message, _| {
        println!("{}: {:?} (len = {})", stamp, message, message.len());
    }, ()).unwrap();
    println!("Starting to send messages ...");
    conn_out.send(&[144, 60, 1]).unwrap();
    sleep(Duration::from_millis(200));
    conn_out.send(&[144, 60, 0]).unwrap();
    sleep(Duration::from_millis(50));
    conn_out.send(&[144, 60, 1]).unwrap();
    sleep(Duration::from_millis(200));
    conn_out.send(&[144, 60, 0]).unwrap();
    sleep(Duration::from_millis(50));
    println!("Closing input ...");
    let midi_in = conn_in.close().0;
    println!("Closing virtual output ...");
    conn_out.close();
    assert_eq!(midi_in.port_count(), previous_count);
}
