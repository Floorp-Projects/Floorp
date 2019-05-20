extern crate env_logger;
/// A thread-based client + server example. It also demonstrates using a struct as a WebSocket
/// handler to implement more handler methods than a closure handler allows.
extern crate ws;

use std::thread;
use std::thread::sleep;
use std::time::Duration;

use ws::{connect, listen, CloseCode, Handler, Message, Result, Sender};

fn main() {
    // Setup logging
    env_logger::init();

    // Server WebSocket handler
    struct Server {
        out: Sender,
    }

    impl Handler for Server {
        fn on_message(&mut self, msg: Message) -> Result<()> {
            println!("Server got message '{}'. ", msg);
            self.out.send(msg)
        }

        fn on_close(&mut self, code: CloseCode, reason: &str) {
            println!("WebSocket closing for ({:?}) {}", code, reason);
            println!("Shutting down server after first connection closes.");
            self.out.shutdown().unwrap();
        }
    }

    // Server thread
    let server = thread::spawn(move || listen("127.0.0.1:3012", |out| Server { out }).unwrap());

    // Give the server a little time to get going
    sleep(Duration::from_millis(10));

    // Client thread
    let client = thread::spawn(move || {
        connect("ws://127.0.0.1:3012", |out| {
            out.send("Hello WebSocket").unwrap();

            move |msg| {
                println!("Client got message '{}'. ", msg);
                out.close(CloseCode::Normal)
            }
        }).unwrap()
    });

    let _ = server.join();
    let _ = client.join();

    println!("All done.")
}
