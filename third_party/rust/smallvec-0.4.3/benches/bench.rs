#![feature(test)]

extern crate smallvec;
extern crate test;

use smallvec::SmallVec;
use self::test::Bencher;

#[bench]
fn bench_push(b: &mut Bencher) {
    #[inline(never)]
    fn push_noinline(vec: &mut SmallVec<[u64; 16]>, x: u64) {
        vec.push(x)
    }

    b.iter(|| {
        let mut vec: SmallVec<[u64; 16]> = SmallVec::new();
        for x in 0..100 {
            push_noinline(&mut vec, x);
        }
        vec
    });
}

#[bench]
fn bench_insert(b: &mut Bencher) {
    #[inline(never)]
    fn insert_noinline(vec: &mut SmallVec<[u64; 16]>, x: u64) {
        vec.insert(0, x)
    }

    b.iter(|| {
        let mut vec: SmallVec<[u64; 16]> = SmallVec::new();
        for x in 0..100 {
            insert_noinline(&mut vec, x);
        }
        vec
    });
}

#[bench]
fn bench_insert_many(b: &mut Bencher) {
    #[inline(never)]
    fn insert_many_noinline<I: IntoIterator<Item=u64>>(
        vec: &mut SmallVec<[u64; 16]>, index: usize, iterable: I) {
        vec.insert_many(index, iterable)
    }

    b.iter(|| {
        let mut vec: SmallVec<[u64; 16]> = SmallVec::new();
        insert_many_noinline(&mut vec, 0, 0..100);
        insert_many_noinline(&mut vec, 0, 0..100);
        vec
    });
}

#[bench]
fn bench_extend(b: &mut Bencher) {
    b.iter(|| {
        let mut vec: SmallVec<[u64; 16]> = SmallVec::new();
        vec.extend(0..100);
        vec
    });
}

#[bench]
fn bench_from_slice(b: &mut Bencher) {
    let v: Vec<u64> = (0..100).collect();
    b.iter(|| {
        let vec: SmallVec<[u64; 16]> = SmallVec::from_slice(&v);
        vec
    });
}

#[bench]
fn bench_extend_from_slice(b: &mut Bencher) {
    let v: Vec<u64> = (0..100).collect();
    b.iter(|| {
        let mut vec: SmallVec<[u64; 16]> = SmallVec::new();
        vec.extend_from_slice(&v);
        vec
    });
}

#[bench]
fn bench_insert_from_slice(b: &mut Bencher) {
    let v: Vec<u64> = (0..100).collect();
    b.iter(|| {
        let mut vec: SmallVec<[u64; 16]> = SmallVec::new();
        vec.insert_from_slice(0, &v);
        vec.insert_from_slice(0, &v);
        vec
    });
}

#[bench]
fn bench_pushpop(b: &mut Bencher) {
    #[inline(never)]
    fn pushpop_noinline(vec: &mut SmallVec<[u64; 16]>, x: u64) {
        vec.push(x);
        vec.pop();
    }

    b.iter(|| {
        let mut vec: SmallVec<[u64; 16]> = SmallVec::new();
        for x in 0..100 {
            pushpop_noinline(&mut vec, x);
        }
        vec
    });
}
