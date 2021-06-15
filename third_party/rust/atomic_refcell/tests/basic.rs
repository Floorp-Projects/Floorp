extern crate atomic_refcell;

use atomic_refcell::{AtomicRef, AtomicRefCell, AtomicRefMut};

#[derive(Debug)]
struct Foo {
    u: u32,
}

#[derive(Debug)]
struct Bar {
    f: Foo,
}

impl Default for Bar {
    fn default() -> Self {
        Bar { f: Foo { u: 42 } }
    }
}

// FIXME(bholley): Add tests to exercise this in concurrent scenarios.

#[test]
fn immutable() {
    let a = AtomicRefCell::new(Bar::default());
    let _first = a.borrow();
    let _second = a.borrow();
}

#[test]
fn try_immutable() {
    let a = AtomicRefCell::new(Bar::default());
    let _first = a.try_borrow().unwrap();
    let _second = a.try_borrow().unwrap();
}

#[test]
fn mutable() {
    let a = AtomicRefCell::new(Bar::default());
    let _ = a.borrow_mut();
}

#[test]
fn try_mutable() {
    let a = AtomicRefCell::new(Bar::default());
    let _ = a.try_borrow_mut().unwrap();
}

#[test]
fn get_mut() {
    let mut a = AtomicRefCell::new(Bar::default());
    let _ = a.get_mut();
}

#[test]
fn interleaved() {
    let a = AtomicRefCell::new(Bar::default());
    {
        let _ = a.borrow_mut();
    }
    {
        let _first = a.borrow();
        let _second = a.borrow();
    }
    {
        let _ = a.borrow_mut();
    }
}

#[test]
fn try_interleaved() {
    let a = AtomicRefCell::new(Bar::default());
    {
        let _ = a.try_borrow_mut().unwrap();
    }
    {
        let _first = a.try_borrow().unwrap();
        let _second = a.try_borrow().unwrap();
        let _ = a.try_borrow_mut().unwrap_err();
    }
    {
        let _first = a.try_borrow_mut().unwrap();
        let _ = a.try_borrow().unwrap_err();
    }
}

#[test]
#[should_panic(expected = "already immutably borrowed")]
fn immutable_then_mutable() {
    let a = AtomicRefCell::new(Bar::default());
    let _first = a.borrow();
    let _second = a.borrow_mut();
}

#[test]
fn immutable_then_try_mutable() {
    let a = AtomicRefCell::new(Bar::default());
    let _first = a.borrow();
    let _second = a.try_borrow_mut().unwrap_err();
}

#[test]
#[should_panic(expected = "already mutably borrowed")]
fn mutable_then_immutable() {
    let a = AtomicRefCell::new(Bar::default());
    let _first = a.borrow_mut();
    let _second = a.borrow();
}

#[test]
fn mutable_then_try_immutable() {
    let a = AtomicRefCell::new(Bar::default());
    let _first = a.borrow_mut();
    let _second = a.try_borrow().unwrap_err();
}

#[test]
#[should_panic(expected = "already mutably borrowed")]
fn double_mutable() {
    let a = AtomicRefCell::new(Bar::default());
    let _first = a.borrow_mut();
    let _second = a.borrow_mut();
}

#[test]
fn mutable_then_try_mutable() {
    let a = AtomicRefCell::new(Bar::default());
    let _first = a.borrow_mut();
    let _second = a.try_borrow_mut().unwrap_err();
}

#[test]
fn map() {
    let a = AtomicRefCell::new(Bar::default());
    let b = a.borrow();
    assert_eq!(b.f.u, 42);
    let c = AtomicRef::map(b, |x| &x.f);
    assert_eq!(c.u, 42);
    let d = AtomicRef::map(c, |x| &x.u);
    assert_eq!(*d, 42);
}

#[test]
fn map_mut() {
    let a = AtomicRefCell::new(Bar::default());
    let mut b = a.borrow_mut();
    assert_eq!(b.f.u, 42);
    b.f.u = 43;
    let mut c = AtomicRefMut::map(b, |x| &mut x.f);
    assert_eq!(c.u, 43);
    c.u = 44;
    let mut d = AtomicRefMut::map(c, |x| &mut x.u);
    assert_eq!(*d, 44);
    *d = 45;
    assert_eq!(*d, 45);
}
