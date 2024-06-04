# oneshot

Oneshot spsc (single producer, single consumer) channel. Meaning each channel instance
can only transport a single message. This has a few nice outcomes. One thing is that
the implementation can be very efficient, utilizing the knowledge that there will
only be one message. But more importantly, it allows the API to be expressed in such
a way that certain edge cases that you don't want to care about when only sending a
single message on a channel does not exist. For example: The sender can't be copied
or cloned, and the send method takes ownership and consumes the sender.
So you are guaranteed, at the type level, that there can only be one message sent.

The sender's send method is non-blocking, and potentially lock- and wait-free.
See documentation on [Sender::send] for situations where it might not be fully wait-free.
The receiver supports both lock- and wait-free `try_recv` as well as indefinite and time
limited thread blocking receive operations. The receiver also implements `Future` and
supports asynchronously awaiting the message.


## Examples

This example sets up a background worker that processes requests coming in on a standard
mpsc channel and replies on a oneshot channel provided with each request. The worker can
be interacted with both from sync and async contexts since the oneshot receiver
can receive both blocking and async.

```rust
use std::sync::mpsc;
use std::thread;
use std::time::Duration;

type Request = String;

// Starts a background thread performing some computation on requests sent to it.
// Delivers the response back over a oneshot channel.
fn spawn_processing_thread() -> mpsc::Sender<(Request, oneshot::Sender<usize>)> {
    let (request_sender, request_receiver) = mpsc::channel::<(Request, oneshot::Sender<usize>)>();
    thread::spawn(move || {
        for (request_data, response_sender) in request_receiver.iter() {
            let compute_operation = || request_data.len();
            let _ = response_sender.send(compute_operation()); // <- Send on the oneshot channel
        }
    });
    request_sender
}

let processor = spawn_processing_thread();

// If compiled with `std` the library can receive messages with timeout on regular threads
#[cfg(feature = "std")] {
    let (response_sender, response_receiver) = oneshot::channel();
    let request = Request::from("data from sync thread");

    processor.send((request, response_sender)).expect("Processor down");
    match response_receiver.recv_timeout(Duration::from_secs(1)) { // <- Receive on the oneshot channel
        Ok(result) => println!("Processor returned {}", result),
        Err(oneshot::RecvTimeoutError::Timeout) => eprintln!("Processor was too slow"),
        Err(oneshot::RecvTimeoutError::Disconnected) => panic!("Processor exited"),
    }
}

// If compiled with the `async` feature, the `Receiver` can be awaited in an async context
#[cfg(feature = "async")] {
    tokio::runtime::Runtime::new()
        .unwrap()
        .block_on(async move {
            let (response_sender, response_receiver) = oneshot::channel();
            let request = Request::from("data from sync thread");

            processor.send((request, response_sender)).expect("Processor down");
            match response_receiver.await { // <- Receive on the oneshot channel asynchronously
                Ok(result) => println!("Processor returned {}", result),
                Err(_e) => panic!("Processor exited"),
            }
        });
}
```

## Sync vs async

The main motivation for writing this library was that there were no (known to me) channel
implementations allowing you to seamlessly send messages between a normal thread and an async
task, or the other way around. If message passing is the way you are communicating, of course
that should work smoothly between the sync and async parts of the program!

This library achieves that by having a fast and cheap send operation that can
be used in both sync threads and async tasks. The receiver has both thread blocking
receive methods for synchronous usage, and implements `Future` for asynchronous usage.

The receiving endpoint of this channel implements Rust's `Future` trait and can be waited on
in an asynchronous task. This implementation is completely executor/runtime agnostic. It should
be possible to use this library with any executor.


License: MIT OR Apache-2.0
