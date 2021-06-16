use std::{cell::RefCell, collections::HashSet};

use crate::RingBuffer;

#[derive(Debug)]
struct Dropper<'a> {
    id: i32,
    set: &'a RefCell<HashSet<i32>>,
}

impl<'a> Dropper<'a> {
    fn new(set: &'a RefCell<HashSet<i32>>, id: i32) -> Self {
        if !set.borrow_mut().insert(id) {
            panic!("value {} already exists", id);
        }
        Self { set, id }
    }
}

impl<'a> Drop for Dropper<'a> {
    fn drop(&mut self) {
        if !self.set.borrow_mut().remove(&self.id) {
            panic!("value {} already removed", self.id);
        }
    }
}

#[test]
fn single() {
    let set = RefCell::new(HashSet::new());

    let cap = 3;
    let buf = RingBuffer::new(cap);

    assert_eq!(set.borrow().len(), 0);

    {
        let (mut prod, mut cons) = buf.split();

        prod.push(Dropper::new(&set, 1)).unwrap();
        assert_eq!(set.borrow().len(), 1);
        prod.push(Dropper::new(&set, 2)).unwrap();
        assert_eq!(set.borrow().len(), 2);
        prod.push(Dropper::new(&set, 3)).unwrap();
        assert_eq!(set.borrow().len(), 3);

        cons.pop().unwrap();
        assert_eq!(set.borrow().len(), 2);
        cons.pop().unwrap();
        assert_eq!(set.borrow().len(), 1);

        prod.push(Dropper::new(&set, 4)).unwrap();
        assert_eq!(set.borrow().len(), 2);
    }

    assert_eq!(set.borrow().len(), 0);
}

#[test]
fn multiple_each() {
    let set = RefCell::new(HashSet::new());

    let cap = 5;
    let buf = RingBuffer::new(cap);

    assert_eq!(set.borrow().len(), 0);

    {
        let (mut prod, mut cons) = buf.split();
        let mut id = 0;
        let mut cnt = 0;

        assert_eq!(
            prod.push_each(|| {
                if cnt < 4 {
                    id += 1;
                    cnt += 1;
                    Some(Dropper::new(&set, id))
                } else {
                    None
                }
            }),
            4
        );
        assert_eq!(cnt, 4);
        assert_eq!(cnt, set.borrow().len());

        assert_eq!(
            cons.pop_each(
                |_| {
                    cnt -= 1;
                    true
                },
                Some(2)
            ),
            2
        );
        assert_eq!(cnt, 2);
        assert_eq!(cnt, set.borrow().len());

        assert_eq!(
            prod.push_each(|| {
                id += 1;
                cnt += 1;
                Some(Dropper::new(&set, id))
            }),
            3
        );
        assert_eq!(cnt, 5);
        assert_eq!(cnt, set.borrow().len());

        assert_eq!(
            cons.pop_each(
                |_| {
                    cnt -= 1;
                    true
                },
                None
            ),
            5
        );
        assert_eq!(cnt, 0);
        assert_eq!(cnt, set.borrow().len());

        assert_eq!(
            prod.push_each(|| {
                id += 1;
                cnt += 1;
                Some(Dropper::new(&set, id))
            }),
            5
        );
        assert_eq!(cnt, 5);
        assert_eq!(cnt, set.borrow().len());
    }

    assert_eq!(set.borrow().len(), 0);
}

/// Test the `pop_each` with internal function that returns false
#[test]
fn pop_each_test1() {
    let cap = 10usize;
    let (mut producer, mut consumer) = RingBuffer::new(cap).split();

    for i in 0..cap {
        producer.push((i, vec![0u8; 1000])).unwrap();
    }

    for _ in 0..cap {
        let removed = consumer.pop_each(|_val| -> bool { false }, None);
        assert_eq!(removed, 1);
    }

    assert_eq!(consumer.len(), 0);
}

/// Test the `pop_each` with capped pop
#[test]
fn pop_each_test2() {
    let cap = 10usize;
    let (mut producer, mut consumer) = RingBuffer::new(cap).split();

    for i in 0..cap {
        producer.push((i, vec![0u8; 1000])).unwrap();
    }

    for _ in 0..cap {
        let removed = consumer.pop_each(|_val| -> bool { true }, Some(1));
        assert_eq!(removed, 1);
    }

    assert_eq!(consumer.len(), 0);
}

/// Test the `push_each` with internal function that adds only 1 element.
#[test]
fn push_each_test1() {
    let cap = 10usize;
    let (mut producer, mut consumer) = RingBuffer::new(cap).split();

    for i in 0..cap {
        let mut count = 0;
        // Add 1 element at a time
        let added = producer.push_each(|| -> Option<(usize, Vec<u8>)> {
            if count == 0 {
                count += 1;
                Some((i, vec![0u8; 1000]))
            } else {
                None
            }
        });
        assert_eq!(added, 1);
    }

    for _ in 0..cap {
        consumer.pop().unwrap();
    }

    assert_eq!(consumer.len(), 0);
}

/// Test the `push_each` with split internal buffer
#[test]
fn push_each_test2() {
    let cap = 10usize;
    let cap_half = 5usize;
    let (mut producer, mut consumer) = RingBuffer::new(cap).split();

    // Fill the entire buffer
    for i in 0..cap {
        producer.push((i, vec![0u8; 1000])).unwrap();
    }

    // Remove half elements
    for _ in 0..cap_half {
        consumer.pop().unwrap();
    }

    // Re add half elements one by one and check the return count.
    for i in 0..cap_half {
        let mut count = 0;
        // Add 1 element at a time
        let added = producer.push_each(|| -> Option<(usize, Vec<u8>)> {
            if count == 0 {
                count += 1;
                Some((i, vec![0u8; 1000]))
            } else {
                None
            }
        });
        assert_eq!(added, 1);
    }

    for _ in 0..cap {
        consumer.pop().unwrap();
    }

    assert_eq!(consumer.len(), 0);
}
