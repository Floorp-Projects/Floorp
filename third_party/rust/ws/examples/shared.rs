extern crate env_logger;
extern crate url;
/// A single-threaded client + server example showing how flexible closure handlers can be for
/// trivial applications.
extern crate ws;

use ws::{Sender, WebSocket};

fn main() {
    // Setup logging
    env_logger::init();

    // A variable to distinguish the two halves
    let mut name = "Client";

    // Create a WebSocket with a closure as the factory
    let mut ws = WebSocket::new(|output: Sender| {
        // The first connection is named Client
        if name == "Client" {
            println!("{} sending 'Hello Websocket' ", name);
            output.send("Hello Websocket").unwrap();
        }

        // The closure handler needs to take ownership of output
        let handler = move |msg| {
            println!("{} got '{}' ", name, msg);

            // If we are the server,
            if name == "Server" {
                println!("{} sending 'How are you?' ", name);

                // send the message back
                output.send("How are you?")
            } else {
                // otherwise, we are the client and will shutdown the WebSocket
                output.shutdown()
            }
        };

        // The next connection this factory makes will be named Server
        name = "Server";

        // We must return the handler
        handler
    }).unwrap();

    // Url for the client
    let url = url::Url::parse("ws://127.0.0.1:3012").unwrap();

    // Queue a WebSocket connection to the url
    ws.connect(url).unwrap();

    // Start listening for incoming conections
    ws.listen("127.0.0.1:3012").unwrap();

    // The WebSocket has shutdown
    println!("All done.")
}
