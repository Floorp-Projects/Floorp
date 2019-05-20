extern crate env_logger;
/// An example of using channels to transfer data between three parts of some system.
///
/// A WebSocket server echoes data back to a client and tees that data to a logging system.
/// A WebSocket client sends some data do the server.
/// A worker thread stores data as a log and sends that data back to the main program when the
/// WebSocket server has finished receiving data.
///
/// This example demonstrates how to use threads, channels, and WebSocket handlers to create a
/// complex system from simple, composable parts.
extern crate ws;

use std::sync::mpsc::Sender as ThreadOut;
use std::sync::mpsc::channel;
use std::thread;
use std::thread::sleep;
use std::time::Duration;

use ws::{connect, listen, CloseCode, Handler, Handshake, Message, Result, Sender};

fn main() {
    // Setup logging
    env_logger::init();

    // Data to be sent across WebSockets and channels
    let data = vec![1, 2, 3, 4, 5];
    let (final_in, final_out) = channel();
    let (log_in, log_out) = channel();

    // WebSocket connection handler for the server connection
    struct Server {
        ws: Sender,
        log: ThreadOut<String>,
    }

    impl Handler for Server {
        fn on_message(&mut self, msg: Message) -> Result<()> {
            println!("Server got message '{}'. ", msg);

            // log it
            self.log.send(msg.to_string()).unwrap();

            // echo it back
            self.ws.send(msg)
        }

        fn on_close(&mut self, _: CloseCode, _: &str) {
            self.ws.shutdown().unwrap()
        }
    }

    // Server thread
    let server = thread::Builder::new()
        .name("server".to_owned())
        .spawn(move || {
            listen("127.0.0.1:3012", |out| {
                Server {
                    ws: out,
                    // we need to clone the channel because
                    // in theory, there could be many active connections
                    log: log_in.clone(),
                }
            }).unwrap()
        })
        .unwrap();

    // Give the server a little time to get going
    sleep(Duration::from_millis(10));

    // WebSocket connection handler for the client connection
    struct Client {
        out: Sender,
        ind: usize,
        data: Vec<u32>,
    }

    impl Client {
        // Core business logic for client, keeping it DRY
        fn increment(&mut self) -> Result<()> {
            if let Some(num) = self.data.get(self.ind) {
                // Advance the index
                self.ind += 1;

                // Send the number to the server
                self.out.send(num.to_string())
            } else {
                // All of the data has been sent, let's close
                self.out.close(CloseCode::Normal)
            }
        }
    }

    impl Handler for Client {
        fn on_open(&mut self, _: Handshake) -> Result<()> {
            self.increment()
        }

        fn on_message(&mut self, msg: Message) -> Result<()> {
            println!("Client got message '{}'. ", msg);
            self.increment()
        }
    }

    // We need to clone the data into the client, making two versions we will compare for
    // consistency later
    let client_data = data.clone();

    // Client thread
    let client = thread::Builder::new()
        .name("client".to_owned())
        .spawn(move || {
            connect("ws://127.0.0.1:3012", |out| {
                Client {
                    out,
                    ind: 0,
                    // we need to clone again because
                    // in theory, there could be many client connections sending off the data
                    data: client_data.clone(),
                }
            }).unwrap()
        })
        .unwrap();

    // Logger thread
    let logger = thread::Builder::new()
        .name("logger".to_owned())
        .spawn(move || {
            // Make a new vector to store the numbers
            let mut log: Vec<u32> = Vec::new();

            // Receive data and push it to the log, this only works if we have one WebSocket
            // connection, otherwise the log would have data from all connections. But for our example,
            // we know we only have one :)
            while let Ok(string) = log_out.recv() {
                println!("Logger is storing {}", string);
                log.push(string.parse().unwrap());
            }

            println!("Logger sending final log result.");
            final_in.send(log).unwrap();
        })
        .unwrap();

    // Wait for the worker threads to finish what they are doing
    let _ = server.join();
    let _ = client.join();
    let _ = logger.join();

    // Get the result from the logger and check that it is correct
    let final_data = final_out.recv().unwrap();
    println!("In: {:?}", data);
    println!("Out: {:?}", final_data);
    assert_eq!(final_data, data);

    println!("All done.")
}
