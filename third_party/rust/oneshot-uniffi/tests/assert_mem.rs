use oneshot::{Receiver, Sender};
use std::mem;

/// Just sanity check that both channel endpoints stay the size of a single pointer.
#[test]
fn channel_endpoints_single_pointer() {
    const PTR_SIZE: usize = mem::size_of::<*const ()>();

    assert_eq!(mem::size_of::<Sender<()>>(), PTR_SIZE);
    assert_eq!(mem::size_of::<Receiver<()>>(), PTR_SIZE);

    assert_eq!(mem::size_of::<Sender<u8>>(), PTR_SIZE);
    assert_eq!(mem::size_of::<Receiver<u8>>(), PTR_SIZE);

    assert_eq!(mem::size_of::<Sender<[u8; 1024]>>(), PTR_SIZE);
    assert_eq!(mem::size_of::<Receiver<[u8; 1024]>>(), PTR_SIZE);

    assert_eq!(mem::size_of::<Option<Sender<[u8; 1024]>>>(), PTR_SIZE);
    assert_eq!(mem::size_of::<Option<Receiver<[u8; 1024]>>>(), PTR_SIZE);
}

/// Check that the `SendError` stays small. Useful to automatically detect if it is refactored
/// to become large. We do not want the stack requirement for calling `Sender::send` to grow.
#[test]
fn error_sizes() {
    const PTR_SIZE: usize = mem::size_of::<usize>();

    assert_eq!(mem::size_of::<oneshot::SendError<()>>(), PTR_SIZE);
    assert_eq!(mem::size_of::<oneshot::SendError<u8>>(), PTR_SIZE);
    assert_eq!(mem::size_of::<oneshot::SendError<[u8; 1024]>>(), PTR_SIZE);

    // The type returned from `Sender::send` is also just pointer sized
    assert_eq!(
        mem::size_of::<Result<(), oneshot::SendError<[u8; 1024]>>>(),
        PTR_SIZE
    );
}
