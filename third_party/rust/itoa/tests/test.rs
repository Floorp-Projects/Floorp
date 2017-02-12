#![allow(non_snake_case)]

extern crate itoa;

macro_rules! test {
    ($($name:ident($value:expr, $expected:expr),)*) => {
        $(
            #[test]
            fn $name() {
                let mut buf = [b'\0'; 20];
                let len = itoa::write(&mut buf[..], $value).unwrap();
                assert_eq!(&buf[0..len], $expected.as_bytes());
            }
        )*
    }
}

test!(
    test_0u64(0u64, "0"),
    test_HALFu64(<u32>::max_value() as u64, "4294967295"),
    test_MAXu64(<u64>::max_value(), "18446744073709551615"),

    test_0i16(0i16, "0"),
    test_MINi16(<i16>::min_value(), "-32768"),
);
