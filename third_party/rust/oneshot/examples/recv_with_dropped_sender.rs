#[cfg(feature = "std")]
fn main() {
    let (sender, receiver) = oneshot::channel::<u128>();
    std::mem::drop(sender);
    receiver.recv().unwrap_err();
}

#[cfg(not(feature = "std"))]
fn main() {
    panic!("This example is only for when the \"sync\" feature is used");
}
