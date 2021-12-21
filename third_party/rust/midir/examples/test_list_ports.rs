extern crate midir;

use std::io::{stdin, stdout, Write};
use std::error::Error;

use midir::{MidiInput, MidiOutput, Ignore};

fn main() {
    match run() {
        Ok(_) => (),
        Err(err) => println!("Error: {}", err)
    }
}

fn run() -> Result<(), Box<dyn Error>> {
    let mut midi_in = MidiInput::new("midir test input")?;
    midi_in.ignore(Ignore::None);
    let midi_out = MidiOutput::new("midir test output")?;

    let mut input = String::new();

    loop {
        println!("Available input ports:");
        for (i, p) in midi_in.ports().iter().enumerate() {
            println!("{}: {}", i, midi_in.port_name(p)?);
        }
        
        println!("\nAvailable output ports:");
        for (i, p) in midi_out.ports().iter().enumerate() {
            println!("{}: {}", i, midi_out.port_name(p)?);
        }

        // run in endless loop if "--loop" parameter is specified
        match ::std::env::args().nth(1) {
            Some(ref arg) if arg == "--loop" => {}
            _ => break
        }
        print!("\nPress <enter> to retry ...");
        stdout().flush()?;
        input.clear();
        stdin().read_line(&mut input)?;
        println!("\n");
    }
    
    Ok(())
}
