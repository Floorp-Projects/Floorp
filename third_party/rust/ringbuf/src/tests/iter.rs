use crate::RingBuffer;

#[test]
fn iter() {
    let buf = RingBuffer::<i32>::new(2);
    let (mut prod, mut cons) = buf.split();

    prod.push(10).unwrap();
    prod.push(20).unwrap();

    let sum: i32 = cons.iter().sum();

    let first = cons.pop().expect("First element not available");
    let second = cons.pop().expect("Second element not available");

    assert_eq!(sum, first + second);
}

#[test]
fn iter_mut() {
    let buf = RingBuffer::<i32>::new(2);
    let (mut prod, mut cons) = buf.split();

    prod.push(10).unwrap();
    prod.push(20).unwrap();

    for v in cons.iter_mut() {
        *v *= 2;
    }

    let sum: i32 = cons.iter().sum();

    let first = cons.pop().expect("First element not available");
    let second = cons.pop().expect("Second element not available");

    assert_eq!(sum, first + second);
}

#[test]
fn into_iter() {
    let buf = RingBuffer::<i32>::new(2);
    let (mut prod, cons) = buf.split();

    prod.push(10).unwrap();
    prod.push(20).unwrap();

    for (i, v) in cons.into_iter().enumerate() {
        assert_eq!(10 * (i + 1) as i32, v);
    }
    assert!(prod.is_empty());
}
