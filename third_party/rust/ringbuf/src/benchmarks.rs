use super::*;

use test::Bencher;

#[bench]
fn push_pop(b: &mut Bencher) {
    let buf = RingBuffer::<u8>::new(0x100);
    let (mut prod, mut cons) = buf.split();
    b.iter(|| {
        while let Ok(()) = prod.push(0) {}
        while let Ok(0) = cons.pop() {}
    });
}
