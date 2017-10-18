#[macro_use]
extern crate bitflags;

bitflags! {
    /// baz
    struct Flags: u32 {
        const A       = 0b00000001;
        #[doc = "bar"]
        const B       = 0b00000010;
        const C       = 0b00000100;
        #[doc = "foo"]
        const ABC     = Flags::A.bits | Flags::B.bits | Flags::C.bits;
    }
}

#[test]
fn smoke() {
    assert_eq!(Flags::ABC, Flags::A | Flags::B | Flags::C);
}
