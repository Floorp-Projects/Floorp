#![cfg(feature = "unstable_testing")]

#![feature(i128_type)]

#[macro_use]
extern crate bitflags;

bitflags! {
    /// baz
    struct Flags128: u128 {
        const A       = 0x0000_0000_0000_0000_0000_0000_0000_0001;
        const B       = 0x0000_0000_0000_1000_0000_0000_0000_0000;
        const C       = 0x8000_0000_0000_0000_0000_0000_0000_0000;
        const ABC     = Self::A.bits | Self::B.bits | Self::C.bits;
    }
}

#[test]
fn test_i128_bitflags() {
    assert_eq!(Flags128::ABC, Flags128::A | Flags128::B | Flags128::C);
    assert_eq!(Flags128::A.bits, 0x0000_0000_0000_0000_0000_0000_0000_0001);
    assert_eq!(Flags128::B.bits, 0x0000_0000_0000_1000_0000_0000_0000_0000);
    assert_eq!(Flags128::C.bits, 0x8000_0000_0000_0000_0000_0000_0000_0000);
    assert_eq!(Flags128::ABC.bits, 0x8000_0000_0000_1000_0000_0000_0000_0001);
    assert_eq!(format!("{:?}", Flags128::A), "A");
    assert_eq!(format!("{:?}", Flags128::B), "B");
    assert_eq!(format!("{:?}", Flags128::C), "C");
    assert_eq!(format!("{:?}", Flags128::ABC), "A | B | C | ABC");
}
