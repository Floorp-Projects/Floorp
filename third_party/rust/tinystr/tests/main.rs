use std::fmt::Write;
use std::mem::size_of;
use std::ops::Deref;
use tinystr::{Error, TinyStr16, TinyStr4, TinyStr8};

#[cfg(any(feature = "std", feature = "alloc"))]
use tinystr::TinyStrAuto;

#[test]
fn tiny_sizes() {
    assert_eq!(4, size_of::<TinyStr4>());
    assert_eq!(8, size_of::<TinyStr8>());
    assert_eq!(16, size_of::<TinyStr16>());
    assert_eq!(24, size_of::<String>());
    // Note: TinyStrAuto is size 32 even when a smaller TinyStr type is used
    #[cfg(any(feature = "std", feature = "alloc"))]
    assert_eq!(32, size_of::<TinyStrAuto>());
}

#[test]
fn tiny4_basic() {
    let s: TinyStr4 = "abc".parse().unwrap();
    assert_eq!(s.deref(), "abc");
}

#[test]
fn tiny4_from_bytes() {
    let s = TinyStr4::from_bytes("abc".as_bytes()).unwrap();
    assert_eq!(s.deref(), "abc");

    assert_eq!(
        TinyStr4::from_bytes(&[0, 159, 146, 150]),
        Err(Error::NonAscii)
    );
    assert_eq!(TinyStr4::from_bytes(&[]), Err(Error::InvalidSize));
    assert_eq!(TinyStr4::from_bytes(&[0]), Err(Error::InvalidNull));
}

#[test]
fn tiny4_size() {
    assert_eq!("".parse::<TinyStr4>(), Err(Error::InvalidSize));
    assert!("1".parse::<TinyStr4>().is_ok());
    assert!("12".parse::<TinyStr4>().is_ok());
    assert!("123".parse::<TinyStr4>().is_ok());
    assert!("1234".parse::<TinyStr4>().is_ok());
    assert_eq!("12345".parse::<TinyStr4>(), Err(Error::InvalidSize));
    assert_eq!("123456789".parse::<TinyStr4>(), Err(Error::InvalidSize));
}

#[test]
fn tiny4_null() {
    assert_eq!("a\u{0}b".parse::<TinyStr4>(), Err(Error::InvalidNull));
}

#[test]
fn tiny4_new_unchecked() {
    let reference: TinyStr4 = "en".parse().unwrap();
    let uval: u32 = reference.into();
    let s = unsafe { TinyStr4::new_unchecked(uval) };
    assert_eq!(s, reference);
    assert_eq!(s, "en");
}

#[test]
fn tiny4_nonascii() {
    assert_eq!("\u{4000}".parse::<TinyStr4>(), Err(Error::NonAscii));
}

#[test]
fn tiny4_alpha() {
    let s: TinyStr4 = "@aZ[".parse().unwrap();
    assert!(!s.is_ascii_alphabetic());
    assert!(!s.is_ascii_alphanumeric());
    assert_eq!(s.to_ascii_uppercase().as_str(), "@AZ[");
    assert_eq!(s.to_ascii_lowercase().as_str(), "@az[");

    assert!("abYZ".parse::<TinyStr4>().unwrap().is_ascii_alphabetic());
    assert!("abYZ".parse::<TinyStr4>().unwrap().is_ascii_alphanumeric());
    assert!("a123".parse::<TinyStr4>().unwrap().is_ascii_alphanumeric());
    assert!(!"a123".parse::<TinyStr4>().unwrap().is_ascii_alphabetic());
}

#[test]
fn tiny4_numeric() {
    let s: TinyStr4 = "@aZ[".parse().unwrap();
    assert!(!s.is_ascii_numeric());

    assert!("0123".parse::<TinyStr4>().unwrap().is_ascii_numeric());
}

#[test]
fn tiny4_titlecase() {
    assert_eq!(
        "abcd"
            .parse::<TinyStr4>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "Abcd"
    );
    assert_eq!(
        "ABCD"
            .parse::<TinyStr4>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "Abcd"
    );
    assert_eq!(
        "aBCD"
            .parse::<TinyStr4>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "Abcd"
    );
    assert_eq!(
        "A123"
            .parse::<TinyStr4>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "A123"
    );
    assert_eq!(
        "123a"
            .parse::<TinyStr4>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "123a"
    );
}

#[test]
fn tiny4_ord() {
    let mut v: Vec<TinyStr4> = vec!["zh".parse().unwrap(), "fr".parse().unwrap()];
    v.sort();

    assert_eq!(v.get(0).unwrap().as_str(), "fr");
    assert_eq!(v.get(1).unwrap().as_str(), "zh");
}

#[test]
fn tiny4_eq() {
    let s1: TinyStr4 = "en".parse().unwrap();
    let s2: TinyStr4 = "fr".parse().unwrap();
    let s3: TinyStr4 = "en".parse().unwrap();

    assert_eq!(s1, s3);
    assert_ne!(s1, s2);
}

#[test]
fn tiny4_display() {
    let s: TinyStr4 = "abcd".parse().unwrap();
    let mut result = String::new();
    write!(result, "{}", s).unwrap();
    assert_eq!(result, "abcd");
    assert_eq!(format!("{}", s), "abcd");
}

#[test]
fn tiny4_debug() {
    let s: TinyStr4 = "abcd".parse().unwrap();
    assert_eq!(format!("{:#?}", s), "\"abcd\"");
}

#[test]
fn tiny8_basic() {
    let s: TinyStr8 = "abcde".parse().unwrap();
    assert_eq!(s.deref(), "abcde");
}

#[test]
fn tiny8_from_bytes() {
    let s = TinyStr8::from_bytes("abcde".as_bytes()).unwrap();
    assert_eq!(s.deref(), "abcde");

    assert_eq!(
        TinyStr8::from_bytes(&[0, 159, 146, 150]),
        Err(Error::NonAscii)
    );
    assert_eq!(TinyStr8::from_bytes(&[]), Err(Error::InvalidSize));
    assert_eq!(TinyStr8::from_bytes(&[0]), Err(Error::InvalidNull));
}

#[test]
fn tiny8_size() {
    assert_eq!("".parse::<TinyStr8>(), Err(Error::InvalidSize));
    assert!("1".parse::<TinyStr8>().is_ok());
    assert!("12".parse::<TinyStr8>().is_ok());
    assert!("123".parse::<TinyStr8>().is_ok());
    assert!("1234".parse::<TinyStr8>().is_ok());
    assert!("12345".parse::<TinyStr8>().is_ok());
    assert!("123456".parse::<TinyStr8>().is_ok());
    assert!("1234567".parse::<TinyStr8>().is_ok());
    assert!("12345678".parse::<TinyStr8>().is_ok());
    assert_eq!("123456789".parse::<TinyStr8>(), Err(Error::InvalidSize));
}

#[test]
fn tiny8_null() {
    assert_eq!("a\u{0}b".parse::<TinyStr8>(), Err(Error::InvalidNull));
}

#[test]
fn tiny8_new_unchecked() {
    let reference: TinyStr8 = "Windows".parse().unwrap();
    let uval: u64 = reference.into();
    let s = unsafe { TinyStr8::new_unchecked(uval) };
    assert_eq!(s, reference);
    assert_eq!(s, "Windows");
}

#[test]
fn tiny8_nonascii() {
    assert_eq!("\u{4000}".parse::<TinyStr8>(), Err(Error::NonAscii));
}

#[test]
fn tiny8_alpha() {
    let s: TinyStr8 = "@abcXYZ[".parse().unwrap();
    assert!(!s.is_ascii_alphabetic());
    assert!(!s.is_ascii_alphanumeric());
    assert_eq!(s.to_ascii_uppercase().as_str(), "@ABCXYZ[");
    assert_eq!(s.to_ascii_lowercase().as_str(), "@abcxyz[");

    assert!("abcXYZ".parse::<TinyStr8>().unwrap().is_ascii_alphabetic());
    assert!("abcXYZ"
        .parse::<TinyStr8>()
        .unwrap()
        .is_ascii_alphanumeric());
    assert!(!"abc123".parse::<TinyStr8>().unwrap().is_ascii_alphabetic());
    assert!("abc123"
        .parse::<TinyStr8>()
        .unwrap()
        .is_ascii_alphanumeric());
}

#[test]
fn tiny8_numeric() {
    let s: TinyStr8 = "@abcXYZ[".parse().unwrap();
    assert!(!s.is_ascii_numeric());

    assert!("01234567".parse::<TinyStr8>().unwrap().is_ascii_numeric());
}

#[test]
fn tiny8_titlecase() {
    assert_eq!(
        "abcdabcd"
            .parse::<TinyStr8>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "Abcdabcd"
    );
    assert_eq!(
        "ABCDABCD"
            .parse::<TinyStr8>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "Abcdabcd"
    );
    assert_eq!(
        "aBCDaBCD"
            .parse::<TinyStr8>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "Abcdabcd"
    );
    assert_eq!(
        "A123a123"
            .parse::<TinyStr8>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "A123a123"
    );
    assert_eq!(
        "123a123A"
            .parse::<TinyStr8>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "123a123a"
    );
}

#[test]
fn tiny8_ord() {
    let mut v: Vec<TinyStr8> = vec!["nedis".parse().unwrap(), "macos".parse().unwrap()];
    v.sort();

    assert_eq!(v.get(0).unwrap().as_str(), "macos");
    assert_eq!(v.get(1).unwrap().as_str(), "nedis");
}

#[test]
fn tiny8_eq() {
    let s1: TinyStr8 = "windows".parse().unwrap();
    let s2: TinyStr8 = "mac".parse().unwrap();
    let s3: TinyStr8 = "windows".parse().unwrap();

    assert_eq!(s1, s3);
    assert_ne!(s1, s2);
}

#[test]
fn tiny8_display() {
    let s: TinyStr8 = "abcdef".parse().unwrap();
    let mut result = String::new();
    write!(result, "{}", s).unwrap();
    assert_eq!(result, "abcdef");
    assert_eq!(format!("{}", s), "abcdef");
}

#[test]
fn tiny8_debug() {
    let s: TinyStr8 = "abcdef".parse().unwrap();
    assert_eq!(format!("{:#?}", s), "\"abcdef\"");
}

#[test]
fn tiny16_from_bytes() {
    let s = TinyStr16::from_bytes("abcdefghijk".as_bytes()).unwrap();
    assert_eq!(s.deref(), "abcdefghijk");

    assert_eq!(
        TinyStr16::from_bytes(&[0, 159, 146, 150]),
        Err(Error::NonAscii)
    );
    assert_eq!(TinyStr16::from_bytes(&[]), Err(Error::InvalidSize));
    assert_eq!(TinyStr16::from_bytes(&[0]), Err(Error::InvalidNull));
}

#[test]
fn tiny16_size() {
    assert_eq!("".parse::<TinyStr16>(), Err(Error::InvalidSize));
    assert!("1".parse::<TinyStr16>().is_ok());
    assert!("12".parse::<TinyStr16>().is_ok());
    assert!("123".parse::<TinyStr16>().is_ok());
    assert!("1234".parse::<TinyStr16>().is_ok());
    assert!("12345".parse::<TinyStr16>().is_ok());
    assert!("123456".parse::<TinyStr16>().is_ok());
    assert!("1234567".parse::<TinyStr16>().is_ok());
    assert!("12345678".parse::<TinyStr16>().is_ok());
    assert!("123456781".parse::<TinyStr16>().is_ok());
    assert!("1234567812".parse::<TinyStr16>().is_ok());
    assert!("12345678123".parse::<TinyStr16>().is_ok());
    assert!("123456781234".parse::<TinyStr16>().is_ok());
    assert!("1234567812345".parse::<TinyStr16>().is_ok());
    assert!("12345678123456".parse::<TinyStr16>().is_ok());
    assert!("123456781234567".parse::<TinyStr16>().is_ok());
    assert!("1234567812345678".parse::<TinyStr16>().is_ok());
    assert_eq!(
        "12345678123456789".parse::<TinyStr16>(),
        Err(Error::InvalidSize)
    );
}

#[test]
fn tiny16_null() {
    assert_eq!("a\u{0}b".parse::<TinyStr16>(), Err(Error::InvalidNull));
}

#[test]
fn tiny16_new_unchecked() {
    let reference: TinyStr16 = "WindowsCE/ME/NT".parse().unwrap();
    let uval: u128 = reference.into();
    let s = unsafe { TinyStr16::new_unchecked(uval) };
    assert_eq!(s, reference);
    assert_eq!(s, "WindowsCE/ME/NT");
}

#[test]
fn tiny16_nonascii() {
    assert_eq!("\u{4000}".parse::<TinyStr16>(), Err(Error::NonAscii));
}

#[test]
fn tiny16_alpha() {
    let s: TinyStr16 = "@abcdefgTUVWXYZ[".parse().unwrap();
    assert!(!s.is_ascii_alphabetic());
    assert!(!s.is_ascii_alphanumeric());
    assert_eq!(s.to_ascii_uppercase().as_str(), "@ABCDEFGTUVWXYZ[");
    assert_eq!(s.to_ascii_lowercase().as_str(), "@abcdefgtuvwxyz[");

    assert!("abcdefgTUVWXYZ"
        .parse::<TinyStr16>()
        .unwrap()
        .is_ascii_alphabetic());
    assert!("abcdefgTUVWXYZ"
        .parse::<TinyStr16>()
        .unwrap()
        .is_ascii_alphanumeric());
    assert!(!"abcdefg0123456"
        .parse::<TinyStr16>()
        .unwrap()
        .is_ascii_alphabetic());
    assert!("abcdefgTUVWXYZ"
        .parse::<TinyStr16>()
        .unwrap()
        .is_ascii_alphanumeric());
}

#[test]
fn tiny16_numeric() {
    let s: TinyStr16 = "@abcdefgTUVWXYZ[".parse().unwrap();
    assert!(!s.is_ascii_numeric());

    assert!("0123456789"
        .parse::<TinyStr16>()
        .unwrap()
        .is_ascii_numeric());
}

#[test]
fn tiny16_titlecase() {
    assert_eq!(
        "abcdabcdabcdabcd"
            .parse::<TinyStr16>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "Abcdabcdabcdabcd"
    );
    assert_eq!(
        "ABCDABCDABCDABCD"
            .parse::<TinyStr16>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "Abcdabcdabcdabcd"
    );
    assert_eq!(
        "aBCDaBCDaBCDaBCD"
            .parse::<TinyStr16>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "Abcdabcdabcdabcd"
    );
    assert_eq!(
        "A123a123A123a123"
            .parse::<TinyStr16>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "A123a123a123a123"
    );
    assert_eq!(
        "123a123A123a123A"
            .parse::<TinyStr16>()
            .unwrap()
            .to_ascii_titlecase()
            .as_str(),
        "123a123a123a123a"
    );
}

#[test]
fn tiny16_ord() {
    let mut v: Vec<TinyStr16> = vec!["nedis_xxxx".parse().unwrap(), "macos_xxxx".parse().unwrap()];
    v.sort();

    assert_eq!(v.get(0).unwrap().as_str(), "macos_xxxx");
    assert_eq!(v.get(1).unwrap().as_str(), "nedis_xxxx");
}

#[test]
fn tiny16_eq() {
    let s1: TinyStr16 = "windows98SE".parse().unwrap();
    let s2: TinyStr16 = "mac".parse().unwrap();
    let s3: TinyStr16 = "windows98SE".parse().unwrap();

    assert_eq!(s1, s3);
    assert_ne!(s1, s2);
}

#[test]
fn tiny16_display() {
    let s: TinyStr16 = "abcdefghijkl".parse().unwrap();
    let mut result = String::new();
    write!(result, "{}", s).unwrap();
    assert_eq!(result, "abcdefghijkl");
    assert_eq!(format!("{}", s), "abcdefghijkl");
}

#[test]
fn tiny16_debug() {
    let s: TinyStr16 = "abcdefghijkl".parse().unwrap();
    assert_eq!(format!("{:#?}", s), "\"abcdefghijkl\"");
}

#[cfg(any(feature = "std", feature = "alloc"))]
#[test]
fn tinyauto_basic() {
    let s1: TinyStrAuto = "abc".parse().unwrap();
    assert_eq!(s1, "abc");

    let s2: TinyStrAuto = "veryveryveryveryverylong".parse().unwrap();
    assert_eq!(s2, "veryveryveryveryverylong");
}

#[cfg(any(feature = "std", feature = "alloc"))]
#[test]
fn tinyauto_nonascii() {
    assert_eq!("\u{4000}".parse::<TinyStrAuto>(), Err(Error::NonAscii));
    assert_eq!(
        "veryveryveryveryverylong\u{4000}".parse::<TinyStrAuto>(),
        Err(Error::NonAscii)
    );
}

#[cfg(feature = "macros")]
const TS: TinyStr8 = tinystr::macros::tinystr8!("test");

#[cfg(feature = "macros")]
#[test]
fn tinystr_macros() {
    use tinystr::macros::*;

    let x: TinyStr8 = "test".parse().unwrap();
    assert_eq!(TS, x);

    let x: TinyStr4 = "foo".parse().unwrap();
    assert_eq!(tinystr4!("foo"), x);

    let x: TinyStr8 = "barbaz".parse().unwrap();
    assert_eq!(tinystr8!("barbaz"), x);

    let x: TinyStr16 = "metamorphosis".parse().unwrap();
    assert_eq!(tinystr16!("metamorphosis"), x);
}
