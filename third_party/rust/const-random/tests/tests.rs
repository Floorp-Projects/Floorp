use const_random::const_random;

#[test]
fn u32() {
    const value1: u32 = const_random!(u32);
    const value2: u32 = const_random!(u32);
    assert_ne!(0, value1);
    assert_ne!(0, value2);
    assert_ne!(value1, value2);
}

#[test]
fn i64() {
    const value1: i64 = const_random!(i64);
    const value2: i64 = const_random!(i64);
    assert_ne!(0, value1);
    assert_ne!(0, value2);
    assert_ne!(value1, value2);
}


#[test]
fn u128() {
    const value1: u128 = const_random!(u128);
    const value2: u128 = const_random!(u128);
    assert_ne!(0, value1);
    assert_ne!(0, value2);
    assert_ne!(value1, value2);
}