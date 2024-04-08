#![cfg(not(loom))]

use oneshot::{channel, Receiver, Sender};

#[test]
fn test_raw_sender() {
    let (sender, receiver) = channel::<u32>();
    let raw = sender.into_raw();
    let recreated = unsafe { Sender::<u32>::from_raw(raw) };
    recreated
        .send(100)
        .unwrap_or_else(|e| panic!("error sending after into_raw/from_raw roundtrip: {e}"));
    assert_eq!(receiver.try_recv(), Ok(100))
}

#[test]
fn test_raw_receiver() {
    let (sender, receiver) = channel::<u32>();
    let raw = receiver.into_raw();
    sender.send(100).unwrap();
    let recreated = unsafe { Receiver::<u32>::from_raw(raw) };
    assert_eq!(
        recreated
            .try_recv()
            .unwrap_or_else(|e| panic!("error receiving after into_raw/from_raw roundtrip: {e}")),
        100
    )
}

#[test]
fn test_raw_sender_and_receiver() {
    let (sender, receiver) = channel::<u32>();
    let raw_receiver = receiver.into_raw();
    let raw_sender = sender.into_raw();

    let recreated_sender = unsafe { Sender::<u32>::from_raw(raw_sender) };
    recreated_sender.send(100).unwrap();

    let recreated_receiver = unsafe { Receiver::<u32>::from_raw(raw_receiver) };
    assert_eq!(
        recreated_receiver
            .try_recv()
            .unwrap_or_else(|e| panic!("error receiving after into_raw/from_raw roundtrip: {e}")),
        100
    )
}
