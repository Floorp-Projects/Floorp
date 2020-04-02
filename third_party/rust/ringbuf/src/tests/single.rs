use std::{sync::atomic::Ordering, thread};

use crate::RingBuffer;

fn head_tail<T>(rb: &RingBuffer<T>) -> (usize, usize) {
    (
        rb.head.load(Ordering::Acquire),
        rb.tail.load(Ordering::Acquire),
    )
}

#[test]
fn capacity() {
    let cap = 13;
    let buf = RingBuffer::<i32>::new(cap);
    assert_eq!(buf.capacity(), cap);
}
#[test]
fn split_capacity() {
    let cap = 13;
    let buf = RingBuffer::<i32>::new(cap);
    let (prod, cons) = buf.split();

    assert_eq!(prod.capacity(), cap);
    assert_eq!(cons.capacity(), cap);
}

#[test]
fn split_threads() {
    let buf = RingBuffer::<i32>::new(10);
    let (prod, cons) = buf.split();

    let pjh = thread::spawn(move || {
        let _ = prod;
    });

    let cjh = thread::spawn(move || {
        let _ = cons;
    });

    pjh.join().unwrap();
    cjh.join().unwrap();
}

#[test]
fn push() {
    let cap = 2;
    let buf = RingBuffer::<i32>::new(cap);
    let (mut prod, _) = buf.split();

    assert_eq!(head_tail(&prod.rb), (0, 0));

    assert_eq!(prod.push(123), Ok(()));
    assert_eq!(head_tail(&prod.rb), (0, 1));

    assert_eq!(prod.push(234), Ok(()));
    assert_eq!(head_tail(&prod.rb), (0, 2));

    assert_eq!(prod.push(345), Err(345));
    assert_eq!(head_tail(&prod.rb), (0, 2));
}

#[test]
fn pop_empty() {
    let cap = 2;
    let buf = RingBuffer::<i32>::new(cap);
    let (_, mut cons) = buf.split();

    assert_eq!(head_tail(&cons.rb), (0, 0));

    assert_eq!(cons.pop(), None);
    assert_eq!(head_tail(&cons.rb), (0, 0));
}

#[test]
fn push_pop_one() {
    let cap = 2;
    let buf = RingBuffer::<i32>::new(cap);
    let (mut prod, mut cons) = buf.split();

    let vcap = cap + 1;
    let values = [12, 34, 56, 78, 90];
    assert_eq!(head_tail(&cons.rb), (0, 0));

    for (i, v) in values.iter().enumerate() {
        assert_eq!(prod.push(*v), Ok(()));
        assert_eq!(head_tail(&cons.rb), (i % vcap, (i + 1) % vcap));

        assert_eq!(cons.pop().unwrap(), *v);
        assert_eq!(head_tail(&cons.rb), ((i + 1) % vcap, (i + 1) % vcap));

        assert_eq!(cons.pop(), None);
        assert_eq!(head_tail(&cons.rb), ((i + 1) % vcap, (i + 1) % vcap));
    }
}

#[test]
fn push_pop_all() {
    let cap = 2;
    let buf = RingBuffer::<i32>::new(cap);
    let (mut prod, mut cons) = buf.split();

    let vcap = cap + 1;
    let values = [(12, 34, 13), (56, 78, 57), (90, 10, 91)];
    assert_eq!(head_tail(&cons.rb), (0, 0));

    for (i, v) in values.iter().enumerate() {
        assert_eq!(prod.push(v.0), Ok(()));
        assert_eq!(head_tail(&cons.rb), (cap * i % vcap, (cap * i + 1) % vcap));

        assert_eq!(prod.push(v.1), Ok(()));
        assert_eq!(head_tail(&cons.rb), (cap * i % vcap, (cap * i + 2) % vcap));

        assert_eq!(prod.push(v.2).unwrap_err(), v.2);
        assert_eq!(head_tail(&cons.rb), (cap * i % vcap, (cap * i + 2) % vcap));

        assert_eq!(cons.pop().unwrap(), v.0);
        assert_eq!(
            head_tail(&cons.rb),
            ((cap * i + 1) % vcap, (cap * i + 2) % vcap)
        );

        assert_eq!(cons.pop().unwrap(), v.1);
        assert_eq!(
            head_tail(&cons.rb),
            ((cap * i + 2) % vcap, (cap * i + 2) % vcap)
        );

        assert_eq!(cons.pop(), None);
        assert_eq!(
            head_tail(&cons.rb),
            ((cap * i + 2) % vcap, (cap * i + 2) % vcap)
        );
    }
}

#[test]
fn empty_full() {
    let buf = RingBuffer::<i32>::new(1);
    let (mut prod, cons) = buf.split();

    assert!(prod.is_empty());
    assert!(cons.is_empty());
    assert!(!prod.is_full());
    assert!(!cons.is_full());

    assert_eq!(prod.push(123), Ok(()));

    assert!(!prod.is_empty());
    assert!(!cons.is_empty());
    assert!(prod.is_full());
    assert!(cons.is_full());
}

#[test]
fn len_remaining() {
    let buf = RingBuffer::<i32>::new(2);
    let (mut prod, mut cons) = buf.split();

    assert_eq!(prod.len(), 0);
    assert_eq!(cons.len(), 0);
    assert_eq!(prod.remaining(), 2);
    assert_eq!(cons.remaining(), 2);

    assert_eq!(prod.push(123), Ok(()));

    assert_eq!(prod.len(), 1);
    assert_eq!(cons.len(), 1);
    assert_eq!(prod.remaining(), 1);
    assert_eq!(cons.remaining(), 1);

    assert_eq!(prod.push(456), Ok(()));

    assert_eq!(prod.len(), 2);
    assert_eq!(cons.len(), 2);
    assert_eq!(prod.remaining(), 0);
    assert_eq!(cons.remaining(), 0);

    assert_eq!(cons.pop(), Some(123));

    assert_eq!(prod.len(), 1);
    assert_eq!(cons.len(), 1);
    assert_eq!(prod.remaining(), 1);
    assert_eq!(cons.remaining(), 1);

    assert_eq!(cons.pop(), Some(456));

    assert_eq!(prod.len(), 0);
    assert_eq!(cons.len(), 0);
    assert_eq!(prod.remaining(), 2);
    assert_eq!(cons.remaining(), 2);

    // now head is at 2, so tail will be at 0. This caught an overflow error
    // when tail+1 < head because of the substraction of usize.
    assert_eq!(prod.push(789), Ok(()));

    assert_eq!(prod.len(), 1);
    assert_eq!(cons.len(), 1);
    assert_eq!(prod.remaining(), 1);
    assert_eq!(cons.remaining(), 1);
}
