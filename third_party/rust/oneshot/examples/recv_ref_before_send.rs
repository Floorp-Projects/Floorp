#[cfg(feature = "std")]
fn main() {
    use std::thread;
    use std::time::Duration;

    let (sender, receiver) = oneshot::channel();
    let t = thread::spawn(move || {
        thread::sleep(Duration::from_millis(2));
        sender.send(9u128).unwrap();
    });
    assert_eq!(receiver.recv_ref(), Ok(9));
    t.join().unwrap();
}

#[cfg(not(feature = "std"))]
fn main() {
    panic!("This example is only for when the \"sync\" feature is used");
}
