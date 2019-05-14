extern crate ws;

use std::sync::mpsc::channel;
use std::thread;
use std::time::Duration;

fn main() {
    let (tx, rx) = channel();

    let socket = ws::Builder::new()
        .build(move |out: ws::Sender| {
            // When we get a connection, send a handle to the parent thread
            tx.send(out).unwrap();

            // Dummy message handler
            move |_| {
                println!("Message handler called.");
                Ok(())
            }
        })
        .unwrap();

    let handle = socket.broadcaster();

    let t = thread::spawn(move || {
        socket.listen("127.0.0.1:3012").unwrap();
    });

    // Wait for 5 seconds only for incoming connections;
    thread::sleep(Duration::from_millis(5000));

    if rx.try_recv().is_err() {
        // shutdown the server from the outside
        handle.shutdown().unwrap();
        println!("Shutting down server because no connections were established.");
    }

    // Let the server finish up (whether it's waiting for new connections or going down)
    t.join().unwrap();
}
