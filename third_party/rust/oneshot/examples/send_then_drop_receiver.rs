use std::mem;

fn main() {
    let (sender, receiver) = oneshot::channel();
    assert!(sender.send(19i128).is_ok());
    mem::drop(receiver);
}
