extern crate ringbuf;

use ringbuf::RingBuffer;

fn main() {
    let rb = RingBuffer::<i32>::new(2);
    let (mut prod, mut cons) = rb.split();

    prod.push(0).unwrap();
    prod.push(1).unwrap();
    assert_eq!(prod.push(2), Err(2));

    assert_eq!(cons.pop().unwrap(), 0);

    prod.push(2).unwrap();

    assert_eq!(cons.pop().unwrap(), 1);
    assert_eq!(cons.pop().unwrap(), 2);
    assert_eq!(cons.pop(), None);
}
