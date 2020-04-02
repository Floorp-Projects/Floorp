use super::*;

use test::Bencher;

const RB_SIZE: usize = 0x400;

#[bench]
fn single_item(b: &mut Bencher) {
    let buf = RingBuffer::<u64>::new(RB_SIZE);
    let (mut prod, mut cons) = buf.split();
    prod.push_slice(&[1; RB_SIZE / 2]);
    b.iter(|| {
        prod.push(1).unwrap();
        cons.pop().unwrap();
    });
}

#[bench]
fn slice_x10(b: &mut Bencher) {
    let buf = RingBuffer::<u64>::new(RB_SIZE);
    let (mut prod, mut cons) = buf.split();
    prod.push_slice(&[1; RB_SIZE / 2]);
    let mut data = [1; 10];
    b.iter(|| {
        prod.push_slice(&data);
        cons.pop_slice(&mut data);
    });
}

#[bench]
fn slice_x100(b: &mut Bencher) {
    let buf = RingBuffer::<u64>::new(RB_SIZE);
    let (mut prod, mut cons) = buf.split();
    prod.push_slice(&[1; RB_SIZE / 2]);
    let mut data = [1; 100];
    b.iter(|| {
        prod.push_slice(&data);
        cons.pop_slice(&mut data);
    });
}
#[bench]
fn slice_x1000(b: &mut Bencher) {
    let buf = RingBuffer::<u64>::new(RB_SIZE);
    let (mut prod, mut cons) = buf.split();
    prod.push_slice(&[1; 16]);
    let mut data = [1; 1000];
    b.iter(|| {
        prod.push_slice(&data);
        cons.pop_slice(&mut data);
    });
}
