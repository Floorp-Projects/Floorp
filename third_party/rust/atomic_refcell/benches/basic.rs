#![feature(test)]

extern crate atomic_refcell;
extern crate test;

use atomic_refcell::AtomicRefCell;
use test::Bencher;

#[derive(Default)]
struct Bar(u32);

#[bench]
fn immutable_borrow(b: &mut Bencher) {
    let a = AtomicRefCell::new(Bar::default());
    b.iter(|| a.borrow());
}

#[bench]
fn immutable_second_borrow(b: &mut Bencher) {
    let a = AtomicRefCell::new(Bar::default());
    let _first = a.borrow();
    b.iter(|| a.borrow());
}

#[bench]
fn immutable_third_borrow(b: &mut Bencher) {
    let a = AtomicRefCell::new(Bar::default());
    let _first = a.borrow();
    let _second = a.borrow();
    b.iter(|| a.borrow());
}

#[bench]
fn mutable_borrow(b: &mut Bencher) {
    let a = AtomicRefCell::new(Bar::default());
    b.iter(|| a.borrow_mut());
}
