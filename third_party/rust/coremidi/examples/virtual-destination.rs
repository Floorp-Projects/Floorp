extern crate coremidi;

fn main() {
    let client = coremidi::Client::new("Example Client").unwrap();

    let callback = |packet_list: &coremidi::PacketList| {
        println!("{}", packet_list);
    };

    let _destination = client
        .virtual_destination("Example Destination", callback)
        .unwrap();

    let mut input_line = String::new();
    println!("Created Virtual Destination \"Example Destination\"");
    println!("Press Enter to Finish");
    std::io::stdin()
        .read_line(&mut input_line)
        .expect("Failed to read line");
}
