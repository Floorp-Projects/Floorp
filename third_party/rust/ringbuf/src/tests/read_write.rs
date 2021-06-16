use std::io::{self};

use crate::RingBuffer;

#[test]
fn from() {
    let buf0 = RingBuffer::<u8>::new(4);
    let buf1 = RingBuffer::<u8>::new(4);
    let (mut prod0, mut cons0) = buf0.split();
    let (mut prod1, mut cons1) = buf1.split();

    let mut tmp = [0; 5];

    assert_eq!(prod0.push_slice(&[0, 1, 2]), 3);

    match prod1.read_from(&mut cons0, None) {
        Ok(n) => assert_eq!(n, 3),
        other => panic!("{:?}", other),
    }
    match prod1.read_from(&mut cons0, None) {
        Err(e) => {
            assert_eq!(e.kind(), io::ErrorKind::WouldBlock);
        }
        other => panic!("{:?}", other),
    }

    assert_eq!(cons1.pop_slice(&mut tmp), 3);
    assert_eq!(tmp[0..3], [0, 1, 2]);

    assert_eq!(prod0.push_slice(&[3, 4, 5]), 3);

    match prod1.read_from(&mut cons0, None) {
        Ok(n) => assert_eq!(n, 2),
        other => panic!("{:?}", other),
    }
    assert_eq!(cons1.pop_slice(&mut tmp), 2);
    assert_eq!(tmp[0..2], [3, 4]);

    match prod1.read_from(&mut cons0, None) {
        Ok(n) => assert_eq!(n, 1),
        other => panic!("{:?}", other),
    }
    assert_eq!(cons1.pop_slice(&mut tmp), 1);
    assert_eq!(tmp[0..1], [5]);

    assert_eq!(prod1.push_slice(&[6, 7, 8]), 3);
    assert_eq!(prod0.push_slice(&[9, 10]), 2);

    match prod1.read_from(&mut cons0, None) {
        Ok(n) => assert_eq!(n, 1),
        other => panic!("{:?}", other),
    }
    match prod1.read_from(&mut cons0, None) {
        Ok(n) => assert_eq!(n, 0),
        other => panic!("{:?}", other),
    }

    assert_eq!(cons1.pop_slice(&mut tmp), 4);
    assert_eq!(tmp[0..4], [6, 7, 8, 9]);
}

#[test]
fn into() {
    let buf0 = RingBuffer::<u8>::new(4);
    let buf1 = RingBuffer::<u8>::new(4);
    let (mut prod0, mut cons0) = buf0.split();
    let (mut prod1, mut cons1) = buf1.split();

    let mut tmp = [0; 5];

    assert_eq!(prod0.push_slice(&[0, 1, 2]), 3);

    match cons0.write_into(&mut prod1, None) {
        Ok(n) => assert_eq!(n, 3),
        other => panic!("{:?}", other),
    }
    match cons0.write_into(&mut prod1, None) {
        Ok(n) => assert_eq!(n, 0),
        other => panic!("{:?}", other),
    }

    assert_eq!(cons1.pop_slice(&mut tmp), 3);
    assert_eq!(tmp[0..3], [0, 1, 2]);

    assert_eq!(prod0.push_slice(&[3, 4, 5]), 3);

    match cons0.write_into(&mut prod1, None) {
        Ok(n) => assert_eq!(n, 2),
        other => panic!("{:?}", other),
    }
    assert_eq!(cons1.pop_slice(&mut tmp), 2);
    assert_eq!(tmp[0..2], [3, 4]);

    match cons0.write_into(&mut prod1, None) {
        Ok(n) => assert_eq!(n, 1),
        other => panic!("{:?}", other),
    }
    assert_eq!(cons1.pop_slice(&mut tmp), 1);
    assert_eq!(tmp[0..1], [5]);

    assert_eq!(prod1.push_slice(&[6, 7, 8]), 3);
    assert_eq!(prod0.push_slice(&[9, 10]), 2);

    match cons0.write_into(&mut prod1, None) {
        Ok(n) => assert_eq!(n, 1),
        other => panic!("{:?}", other),
    }
    match cons0.write_into(&mut prod1, None) {
        Err(e) => {
            assert_eq!(e.kind(), io::ErrorKind::WouldBlock);
        }
        other => panic!("{:?}", other),
    }

    assert_eq!(cons1.pop_slice(&mut tmp), 4);
    assert_eq!(tmp[0..4], [6, 7, 8, 9]);
}

#[test]
fn count() {
    let buf0 = RingBuffer::<u8>::new(4);
    let buf1 = RingBuffer::<u8>::new(4);
    let (mut prod0, mut cons0) = buf0.split();
    let (mut prod1, mut cons1) = buf1.split();

    let mut tmp = [0; 5];

    assert_eq!(prod0.push_slice(&[0, 1, 2, 3]), 4);

    match prod1.read_from(&mut cons0, Some(3)) {
        Ok(n) => assert_eq!(n, 3),
        other => panic!("{:?}", other),
    }
    match prod1.read_from(&mut cons0, Some(2)) {
        Ok(n) => assert_eq!(n, 1),
        other => panic!("{:?}", other),
    }

    assert_eq!(cons1.pop_slice(&mut tmp), 4);
    assert_eq!(tmp[0..4], [0, 1, 2, 3]);

    assert_eq!(prod0.push_slice(&[4, 5, 6, 7]), 4);

    match cons0.write_into(&mut prod1, Some(3)) {
        Ok(n) => assert_eq!(n, 1),
        other => panic!("{:?}", other),
    }
    match cons0.write_into(&mut prod1, Some(2)) {
        Ok(n) => assert_eq!(n, 2),
        other => panic!("{:?}", other),
    }
    match cons0.write_into(&mut prod1, Some(2)) {
        Ok(n) => assert_eq!(n, 1),
        other => panic!("{:?}", other),
    }

    assert_eq!(cons1.pop_slice(&mut tmp), 4);
    assert_eq!(tmp[0..4], [4, 5, 6, 7]);
}
