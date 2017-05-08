#![allow(dead_code, unused_imports)]
#![cfg_attr(feature = "i128", feature(i128_type))]

#[macro_use]
extern crate bitflags;

#[cfg(feature = "i128")]
bitflags! {
    /// baz
    flags Flags128: u128 {
        const A       = 0x0000_0000_0000_0000_0000_0000_0000_0001,
        const B       = 0x0000_0000_0000_1000_0000_0000_0000_0000,
        const C       = 0x8000_0000_0000_0000_0000_0000_0000_0000,
        const ABC     = A.bits | B.bits | C.bits,
    }
}

#[cfg(feature = "i128")]
#[test]
fn test_i128_bitflags() {
    assert_eq!(ABC, A | B | C);
    assert_eq!(A.bits, 0x0000_0000_0000_0000_0000_0000_0000_0001);
    assert_eq!(B.bits, 0x0000_0000_0000_1000_0000_0000_0000_0000);
    assert_eq!(C.bits, 0x8000_0000_0000_0000_0000_0000_0000_0000);
    assert_eq!(ABC.bits, 0x8000_0000_0000_1000_0000_0000_0000_0001);
    assert_eq!(format!("{:?}", A), "A");
    assert_eq!(format!("{:?}", B), "B");
    assert_eq!(format!("{:?}", C), "C");
    assert_eq!(format!("{:?}", ABC), "A | B | C | ABC");
}
