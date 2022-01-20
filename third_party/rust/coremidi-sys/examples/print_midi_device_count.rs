extern crate coremidi_sys as cm;

fn main()
{
    let src: u64 = unsafe { cm::MIDIGetNumberOfSources() };
    let dest: u64 = unsafe { cm::MIDIGetNumberOfDestinations() };

    println!("Number of MIDI sources: {}", src);
    println!("Number of MIDI destinations: {}", dest);
}
