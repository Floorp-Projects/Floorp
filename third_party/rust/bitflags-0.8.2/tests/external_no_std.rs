#![allow(dead_code)]
#![no_std]

#[macro_use]
extern crate bitflags;

bitflags! {
    /// baz
    flags Flags: u32 {
        const A       = 0b00000001,
        #[doc = "bar"]
        const B       = 0b00000010,
        const C       = 0b00000100,
        #[doc = "foo"]
        const ABC     = A.bits | B.bits | C.bits,
    }
}

#[test]
fn smoke() {
    assert_eq!(ABC, A | B | C);
}
