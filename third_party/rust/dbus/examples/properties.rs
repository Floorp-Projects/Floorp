extern crate dbus;

use dbus::{Connection, BusType, stdintf, arg};
use std::collections::HashMap;

fn print_refarg(value: &arg::RefArg) {
    // We don't know what type the value is. We'll try a few and fall back to
    // debug printing if the value is more complex than that.
    if let Some(s) = value.as_str() { println!("{}", s); }
    else if let Some(i) = value.as_i64() { println!("{}", i); }
    else { println!("{:?}", value); }
}

fn main() {
    // Connect to server and create a ConnPath. A ConnPath implements several interfaces,
    // in this case we'll use OrgFreedesktopDBusProperties, which allows us to call "get".
    let c = Connection::get_private(BusType::Session).unwrap();
    let p = c.with_path("org.mpris.MediaPlayer2.rhythmbox", "/org/mpris/MediaPlayer2", 5000);
    use stdintf::org_freedesktop_dbus::Properties;

    // The Metadata property is a Dict<String, Variant>. 

    // Option 1: we can get the dict straight into a hashmap, like this:

    let metadata: HashMap<String, arg::Variant<Box<arg::RefArg>>> = p.get("org.mpris.MediaPlayer2.Player", "Metadata").unwrap();

    println!("Option 1:");

    // We now iterate over the hashmap.
    for (key, value) in metadata.iter() {
        print!("  {}: ", key);
        print_refarg(&value);
    }


    // Option 2: we can get the entire dict as a RefArg and get the values out by iterating over it.

    let metadata: Box<arg::RefArg> = p.get("org.mpris.MediaPlayer2.Player", "Metadata").unwrap();

    // When using "as_iter()" for a dict, we'll get one key, it's value, next key, it's value, etc.
    let mut iter = metadata.as_iter().unwrap();

    println!("Option 2:");
    while let Some(key) = iter.next() {
        // Printing the key is easy, since we know it's a String.
        print!("  {}: ", key.as_str().unwrap());
        let value = iter.next().unwrap();
        print_refarg(&value);
    }
}
