#[cfg(feature = "std")]
fn main() {
    use std::thread;
    use std::time::Duration;

    let (sender, receiver) = oneshot::channel::<u128>();
    let t = thread::spawn(move || {
        thread::sleep(Duration::from_millis(2));
        std::mem::drop(sender);
    });
    assert!(receiver.recv_timeout(Duration::from_millis(100)).is_err());
    t.join().unwrap();
}

#[cfg(not(feature = "std"))]
fn main() {
    panic!("This example is only for when the \"sync\" feature is used");
}
