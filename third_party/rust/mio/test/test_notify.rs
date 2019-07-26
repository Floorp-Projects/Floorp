use {localhost, sleep_ms};
use mio::*;
use mio::deprecated::{EventLoop, EventLoopBuilder, Handler, Sender, NotifyError};
use mio::net::TcpListener;
use std::thread;

struct TestHandler {
    sender: Sender<String>,
    notify: usize
}

impl TestHandler {
    fn new(sender: Sender<String>) -> TestHandler {
        TestHandler {
            sender,
            notify: 0
        }
    }
}

impl Handler for TestHandler {
    type Timeout = usize;
    type Message = String;

    fn notify(&mut self, event_loop: &mut EventLoop<TestHandler>, msg: String) {
        match self.notify {
            0 => {
                assert!(msg == "First", "actual={}", msg);
                self.sender.send("Second".to_string()).unwrap();
            }
            1 => {
                assert!(msg == "Second", "actual={}", msg);
                event_loop.shutdown();
            }
            v => panic!("unexpected value for notify; val={}", v)
        }

        self.notify += 1;
    }
}

#[test]
pub fn test_notify() {
    debug!("Starting TEST_NOTIFY");
    let mut event_loop = EventLoop::new().unwrap();

    let addr = localhost();

    // Setup a server socket so that the event loop blocks
    let srv = TcpListener::bind(&addr).unwrap();

    event_loop.register(&srv, Token(0), Ready::readable() | Ready::writable(), PollOpt::edge()).unwrap();

    let sender = event_loop.channel();

    thread::spawn(move || {
        sleep_ms(1_000);
        sender.send("First".to_string()).unwrap();
    });

    let sender = event_loop.channel();
    let mut handler = TestHandler::new(sender);

    // Start the event loop
    event_loop.run(&mut handler).unwrap();

    assert!(handler.notify == 2, "actual={}", handler.notify);
}

#[test]
pub fn test_notify_capacity() {
    use std::sync::mpsc::*;
    use std::thread;

    struct Capacity(Receiver<i32>);

    impl Handler for Capacity {
        type Message = i32;
        type Timeout = ();

        fn notify(&mut self, event_loop: &mut EventLoop<Capacity>, msg: i32) {
            if msg == 1 {
                self.0.recv().unwrap();
            } else if msg == 3 {
                event_loop.shutdown();
            }
        }
    }

    let mut builder = EventLoopBuilder::new();
    builder.notify_capacity(1);

    let (tx, rx) = channel::<i32>();
    let mut event_loop = builder.build().unwrap();
    let notify = event_loop.channel();

    let handle = thread::spawn(move || {
        let mut handler = Capacity(rx);
        event_loop.run(&mut handler).unwrap();
    });

    assert!(notify.send(1).is_ok());

    loop {
        if notify.send(2).is_err() {
            break;
        }
    }

    tx.send(1).unwrap();

    loop {
        if notify.send(3).is_ok() {
            break;
        }
    }

    handle.join().unwrap();
}

#[test]
pub fn test_notify_drop() {
    use std::sync::mpsc::{self,Sender};
    use std::thread;

    struct MessageDrop(Sender<u8>);

    impl Drop for MessageDrop {
        fn drop(&mut self) {
            self.0.send(0).unwrap();
        }
    }

    struct DummyHandler;

    impl Handler for DummyHandler {
        type Timeout = ();
        type Message = MessageDrop;

        fn notify(&mut self, event_loop: &mut EventLoop<Self>, msg: MessageDrop) {
            msg.0.send(1).unwrap();
            drop(msg);
            // We stop after the first message
            event_loop.shutdown();
        }
    }

    let (tx_notif_1, rx_notif_1) = mpsc::channel();
    let (tx_notif_2, rx_notif_2) = mpsc::channel();
    let (tx_notif_3, _unused) = mpsc::channel();
    let (tx_exit_loop, rx_exit_loop) = mpsc::channel();
    let (tx_drop_loop, rx_drop_loop) = mpsc::channel();

    let mut event_loop = EventLoop::new().unwrap();
    let notify = event_loop.channel();

    let handle = thread::spawn(move || {
        let mut handler = DummyHandler;
        event_loop.run(&mut handler).unwrap();

        // Confirmation we exited the loop
        tx_exit_loop.send(()).unwrap();

        // Order to drop the loop
        rx_drop_loop.recv().unwrap();
        drop(event_loop);
    });
    notify.send(MessageDrop(tx_notif_1)).unwrap();
    assert_eq!(rx_notif_1.recv().unwrap(), 1); // Response from the loop
    assert_eq!(rx_notif_1.recv().unwrap(), 0); // Drop notification

    // We wait for the event loop to exit before sending the second notification
    rx_exit_loop.recv().unwrap();
    notify.send(MessageDrop(tx_notif_2)).unwrap();

    // We ensure the message is indeed stuck in the queue
    sleep_ms(100);
    assert!(rx_notif_2.try_recv().is_err());

    // Give the order to drop the event loop
    tx_drop_loop.send(()).unwrap();
    assert_eq!(rx_notif_2.recv().unwrap(), 0); // Drop notification

    // Check that sending a new notification will return an error
    // We should also get our message back
    match notify.send(MessageDrop(tx_notif_3)).unwrap_err() {
        NotifyError::Closed(Some(..)) => {}
        _ => panic!(),
    }

    handle.join().unwrap();
}
