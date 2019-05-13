extern crate env_logger;
/// Simple WebSocket server with error handling. It is not necessary to setup logging, but doing
/// so will allow you to see more details about the connection by using the RUST_LOG env variable.
extern crate ws;

use ws::listen;

fn main() {
    // Setup logging
    env_logger::init();

    // Listen on an address and call the closure for each connection
    if let Err(error) = listen("127.0.0.1:3012", |out| {
        // The handler needs to take ownership of out, so we use move
        move |msg| {
            // Handle messages received on this connection
            println!("Server got message '{}'. ", msg);

            // Use the out channel to send messages back
            out.send(msg)
        }
    }) {
        // Inform the user of failure
        println!("Failed to create WebSocket due to {:?}", error);
    }
}
