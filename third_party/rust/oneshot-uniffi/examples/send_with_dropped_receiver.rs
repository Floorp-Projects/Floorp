use std::mem;

fn main() {
    let (sender, receiver) = oneshot::channel();
    mem::drop(receiver);
    let send_error = sender.send(5u128).unwrap_err();
    assert_eq!(send_error.into_inner(), 5);
}
