use num_traits::{Signed, ToPrimitive, Zero};

use rust_decimal::{Decimal, RoundingStrategy};

use std::{
    cmp::{Ordering, Ordering::*},
    str::FromStr,
};

// Parsing

#[test]
fn it_creates_a_new_negative_decimal() {
    let a = Decimal::new(-100, 2);
    assert_eq!(a.is_sign_negative(), true);
    assert_eq!(a.scale(), 2);
    assert_eq!("-1.00", a.to_string());
}

#[test]
fn it_creates_a_new_decimal_using_numeric_boundaries() {
    let a = Decimal::new(i64::max_value(), 2);
    assert_eq!(a.is_sign_negative(), false);
    assert_eq!(a.scale(), 2);
    assert_eq!("92233720368547758.07", a.to_string());

    let b = Decimal::new(i64::min_value(), 2);
    assert_eq!(b.is_sign_negative(), true);
    assert_eq!(b.scale(), 2);
    assert_eq!("-92233720368547758.08", b.to_string());
}

#[test]
fn it_parses_empty_string() {
    assert!(Decimal::from_str("").is_err());
    assert!(Decimal::from_str(" ").is_err());
}

#[test]
fn it_parses_positive_int_string() {
    let a = Decimal::from_str("233").unwrap();
    assert_eq!(a.is_sign_negative(), false);
    assert_eq!(a.scale(), 0);
    assert_eq!("233", a.to_string());
}

#[test]
fn it_parses_negative_int_string() {
    let a = Decimal::from_str("-233").unwrap();
    assert_eq!(a.is_sign_negative(), true);
    assert_eq!(a.scale(), 0);
    println!("to_string");
    assert_eq!("-233", a.to_string());
}

#[test]
fn it_parses_positive_float_string() {
    let a = Decimal::from_str("233.323223").unwrap();
    assert_eq!(a.is_sign_negative(), false);
    assert_eq!(a.scale(), 6);
    assert_eq!("233.323223", a.to_string());
}

#[test]
fn it_parses_negative_float_string() {
    let a = Decimal::from_str("-233.43343").unwrap();
    assert_eq!(a.is_sign_negative(), true);
    assert_eq!(a.scale(), 5);
    assert_eq!("-233.43343", a.to_string());
}

#[test]
fn it_parses_positive_tiny_float_string() {
    let a = Decimal::from_str(".000001").unwrap();
    assert_eq!(a.is_sign_negative(), false);
    assert_eq!(a.scale(), 6);
    assert_eq!("0.000001", a.to_string());
}

#[test]
fn it_parses_negative_tiny_float_string() {
    let a = Decimal::from_str("-0.000001").unwrap();
    assert_eq!(a.is_sign_negative(), true);
    assert_eq!(a.scale(), 6);
    assert_eq!("-0.000001", a.to_string());
}

#[test]
fn it_parses_big_integer_string() {
    let a = Decimal::from_str("79228162514264337593543950330").unwrap();
    assert_eq!("79228162514264337593543950330", a.to_string());
}

#[test]
fn it_parses_big_float_string() {
    let a = Decimal::from_str("79.228162514264337593543950330").unwrap();
    assert_eq!("79.228162514264337593543950330", a.to_string());
}

#[test]
fn it_can_serialize_deserialize() {
    let a = Decimal::from_str("12.3456789").unwrap();
    let bytes = a.serialize();
    let b = Decimal::deserialize(bytes);
    assert_eq!("12.3456789", b.to_string());
}

// Formatting

#[test]
fn it_formats() {
    let a = Decimal::from_str("233.323223").unwrap();
    assert_eq!(format!("{}", a), "233.323223");
    assert_eq!(format!("{:.9}", a), "233.323223000");
    assert_eq!(format!("{:.0}", a), "233");
    assert_eq!(format!("{:.2}", a), "233.32");
    assert_eq!(format!("{:010.2}", a), "0000233.32");
    assert_eq!(format!("{:0<10.2}", a), "233.320000");
}
#[test]
fn it_formats_neg() {
    let a = Decimal::from_str("-233.323223").unwrap();
    assert_eq!(format!("{}", a), "-233.323223");
    assert_eq!(format!("{:.9}", a), "-233.323223000");
    assert_eq!(format!("{:.0}", a), "-233");
    assert_eq!(format!("{:.2}", a), "-233.32");
    assert_eq!(format!("{:010.2}", a), "-000233.32");
    assert_eq!(format!("{:0<10.2}", a), "-233.32000");
}
#[test]
fn it_formats_small() {
    let a = Decimal::from_str("0.2223").unwrap();
    assert_eq!(format!("{}", a), "0.2223");
    assert_eq!(format!("{:.9}", a), "0.222300000");
    assert_eq!(format!("{:.0}", a), "0");
    assert_eq!(format!("{:.2}", a), "0.22");
    assert_eq!(format!("{:010.2}", a), "0000000.22");
    assert_eq!(format!("{:0<10.2}", a), "0.22000000");
}
#[test]
fn it_formats_small_leading_zeros() {
    let a = Decimal::from_str("0.0023554701772169").unwrap();
    assert_eq!(format!("{}", a), "0.0023554701772169");
    assert_eq!(format!("{:.9}", a), "0.002355470");
    assert_eq!(format!("{:.0}", a), "0");
    assert_eq!(format!("{:.2}", a), "0.00");
    assert_eq!(format!("{:010.2}", a), "0000000.00");
    assert_eq!(format!("{:0<10.2}", a), "0.00000000");
}
#[test]
fn it_formats_small_neg() {
    let a = Decimal::from_str("-0.2223").unwrap();
    assert_eq!(format!("{}", a), "-0.2223");
    assert_eq!(format!("{:.9}", a), "-0.222300000");
    assert_eq!(format!("{:.0}", a), "-0");
    assert_eq!(format!("{:.2}", a), "-0.22");
    assert_eq!(format!("{:010.2}", a), "-000000.22");
    assert_eq!(format!("{:0<10.2}", a), "-0.2200000");
}
#[test]
fn it_formats_zero() {
    let a = Decimal::from_str("0").unwrap();
    assert_eq!(format!("{}", a), "0");
    assert_eq!(format!("{:.9}", a), "0.000000000");
    assert_eq!(format!("{:.0}", a), "0");
    assert_eq!(format!("{:.2}", a), "0.00");
    assert_eq!(format!("{:010.2}", a), "0000000.00");
    assert_eq!(format!("{:0<10.2}", a), "0.00000000");
}
#[test]
fn it_formats_int() {
    let a = Decimal::from_str("5").unwrap();
    assert_eq!(format!("{}", a), "5");
    assert_eq!(format!("{:.9}", a), "5.000000000");
    assert_eq!(format!("{:.0}", a), "5");
    assert_eq!(format!("{:.2}", a), "5.00");
    assert_eq!(format!("{:010.2}", a), "0000005.00");
    assert_eq!(format!("{:0<10.2}", a), "5.00000000");
}

// Negation
#[test]
fn it_negates_decimals() {
    fn neg(a: &str, b: &str) {
        let a = Decimal::from_str(a).unwrap();
        let result = -a;
        assert_eq!(b, result.to_string(), "- {}", a.to_string());
    }

    let tests = &[
        ("1", "-1"),
        ("2", "-2"),
        ("2454495034", "-2454495034"),
        (".1", "-0.1"),
        ("11.815126050420168067226890757", "-11.815126050420168067226890757"),
    ];

    for &(a, b) in tests {
        neg(a, b);
    }
}

// Addition

#[test]
fn it_adds_decimals() {
    fn add(a: &str, b: &str, c: &str) {
        let a = Decimal::from_str(a).unwrap();
        let b = Decimal::from_str(b).unwrap();
        let result = a + b;
        assert_eq!(c, result.to_string(), "{} + {}", a.to_string(), b.to_string());
        let result = b + a;
        assert_eq!(c, result.to_string(), "{} + {}", b.to_string(), a.to_string());
    }

    let tests = &[
        ("2", "3", "5"),
        ("2454495034", "3451204593", "5905699627"),
        ("24544.95034", ".3451204593", "24545.2954604593"),
        (".1", ".1", "0.2"),
        (".10", ".1", "0.20"),
        (".1", "-.1", "0.0"),
        ("0", "1.001", "1.001"),
        ("2", "-3", "-1"),
        ("-2", "3", "1"),
        ("-2", "-3", "-5"),
        ("3", "-2", "1"),
        ("-3", "2", "-1"),
        ("1.234", "2.4567", "3.6907"),
        (
            "11.815126050420168067226890757",
            "0.6386554621848739495798319328",
            "12.453781512605042016806722690",
        ),
        (
            "-11.815126050420168067226890757",
            "0.6386554621848739495798319328",
            "-11.176470588235294117647058824",
        ),
        (
            "11.815126050420168067226890757",
            "-0.6386554621848739495798319328",
            "11.176470588235294117647058824",
        ),
        (
            "-11.815126050420168067226890757",
            "-0.6386554621848739495798319328",
            "-12.453781512605042016806722690",
        ),
        (
            "11815126050420168067226890757",
            "0.4386554621848739495798319328",
            "11815126050420168067226890757",
        ),
        (
            "-11815126050420168067226890757",
            "0.4386554621848739495798319328",
            "-11815126050420168067226890757",
        ),
        (
            "11815126050420168067226890757",
            "-0.4386554621848739495798319328",
            "11815126050420168067226890757",
        ),
        (
            "-11815126050420168067226890757",
            "-0.4386554621848739495798319328",
            "-11815126050420168067226890757",
        ),
        (
            "0.0872727272727272727272727272",
            "843.65000000",
            "843.7372727272727272727272727",
        ),
        (
            "7314.6229858868828353570724702",
            "1000",
            // Overflow causes this to round
            "8314.622985886882835357072470",
        ),
        (
            "108053.27500000000000000000000",
            "0.00000000000000000000000",
            "108053.27500000000000000000000",
        ),
        (
            "108053.27500000000000000000000",
            // This zero value has too high precision and will be trimmed
            "0.000000000000000000000000",
            "108053.27500000000000000000000",
        ),
        (
            "108053.27500000000000000000000",
            // This value has too high precision and will be rounded
            "0.000000000000000000000001",
            "108053.27500000000000000000000",
        ),
        (
            "108053.27500000000000000000000",
            // This value has too high precision and will be rounded
            "0.000000000000000000000005",
            "108053.27500000000000000000001",
        ),
        (
            "8097370036018690744.2590371109596744091",
            "3807285637671831400.15346897797550749555",
            "11904655673690522144.412506089",
        ),
    ];
    for &(a, b, c) in tests {
        add(a, b, c);
    }
}

#[test]
fn it_can_addassign() {
    let mut a = Decimal::from_str("1.01").unwrap();
    let b = Decimal::from_str("0.99").unwrap();
    a += b;
    assert_eq!("2.00", a.to_string());

    a += &b;
    assert_eq!("2.99", a.to_string());

    let mut c = &mut a;
    c += b;
    assert_eq!("3.98", a.to_string());

    let mut c = &mut a;
    c += &b;
    assert_eq!("4.97", a.to_string());
}

// Subtraction

#[test]
fn it_subtracts_decimals() {
    fn sub(a: &str, b: &str, c: &str) {
        let a = Decimal::from_str(a).unwrap();
        let b = Decimal::from_str(b).unwrap();
        let result = a - b;
        assert_eq!(c, result.to_string(), "{} - {}", a.to_string(), b.to_string());
    }

    let tests = &[
        ("2", "3", "-1"),
        ("3451204593", "2323322332", "1127882261"),
        ("24544.95034", ".3451204593", "24544.6052195407"),
        (".1", ".1", "0.0"),
        (".1", "-.1", "0.2"),
        ("1.001", "0", "1.001"),
        ("2", "-3", "5"),
        ("-2", "3", "-5"),
        ("-2", "-3", "1"),
        ("3", "-2", "5"),
        ("-3", "2", "-5"),
        ("1.234", "2.4567", "-1.2227"),
        ("844.13000000", "843.65000000", "0.48000000"),
        ("79228162514264337593543950335", "79228162514264337593543950335", "0"), // 0xFFFF_FFFF_FFFF_FFFF_FFF_FFFF - 0xFFFF_FFFF_FFFF_FFFF_FFF_FFFF
        ("79228162514264337593543950335", "0", "79228162514264337593543950335"),
        ("79228162514264337593543950335", "79228162514264337593543950333", "2"),
        ("4951760157141521099596496896", "1", "4951760157141521099596496895"), // 0x1000_0000_0000_0000_0000_0000 - 1 = 0x0FFF_FFFF_FFFF_FFFF_FFF_FFFF
        ("79228162514264337593543950334", "79228162514264337593543950335", "-1"),
        ("1", "4951760157141521099596496895", "-4951760157141521099596496894"),
        ("18446744073709551615", "-18446744073709551615", "36893488147419103230"), // 0xFFFF_FFFF_FFFF_FFFF - -0xFFFF_FFFF_FFFF_FFFF
    ];
    for &(a, b, c) in tests {
        sub(a, b, c);
    }
}

#[test]
fn it_can_subassign() {
    let mut a = Decimal::from_str("1.01").unwrap();
    let b = Decimal::from_str("0.51").unwrap();
    a -= b;
    assert_eq!("0.50", a.to_string());

    a -= &b;
    assert_eq!("-0.01", a.to_string());

    let mut c = &mut a;
    c -= b;
    assert_eq!("-0.52", a.to_string());

    let mut c = &mut a;
    c -= &b;
    assert_eq!("-1.03", a.to_string());
}

// Multiplication

#[test]
fn it_multiplies_decimals() {
    fn mul(a: &str, b: &str, c: &str) {
        let a = Decimal::from_str(a).unwrap();
        let b = Decimal::from_str(b).unwrap();
        let result = a * b;
        assert_eq!(c, result.to_string(), "{} * {}", a.to_string(), b.to_string());
        let result = b * a;
        assert_eq!(c, result.to_string(), "{} * {}", b.to_string(), a.to_string());
    }

    let tests = &[
        ("2", "3", "6"),
        ("2454495034", "3451204593", "8470964534836491162"),
        ("24544.95034", ".3451204593", "8470.964534836491162"),
        (".1", ".1", "0.01"),
        ("0", "1.001", "0"),
        ("2", "-3", "-6"),
        ("-2", "3", "-6"),
        ("-2", "-3", "6"),
        ("1", "2.01", "2.01"),
        ("1.0", "2.01", "2.010"), // Scale is always additive
        (
            "0.00000000000000001",
            "0.00000000000000001",
            "0.0000000000000000000000000000",
        ),
        ("0.0000000000000000000000000001", "0.0000000000000000000000000001", "0"),
        (
            "0.6386554621848739495798319328",
            "11.815126050420168067226890757",
            "7.5457947885036367488171739292",
        ),
        (
            "2123456789012345678901234567.8",
            "11.815126050420168067226890757",
            "25088909624801327937270048761",
        ),
        (
            "2123456789012345678901234567.8",
            "-11.815126050420168067226890757",
            "-25088909624801327937270048761",
        ),
        (
            "2.1234567890123456789012345678",
            "2.1234567890123456789012345678",
            "4.5090687348026215523554336227",
        ),
        (
            "0.48000000",
            "0.1818181818181818181818181818",
            "0.0872727272727272727272727272",
        ),
    ];
    for &(a, b, c) in tests {
        mul(a, b, c);
    }
}

#[test]
#[should_panic]
fn it_panics_when_multiply_with_overflow() {
    let a = Decimal::from_str("2000000000000000000001").unwrap();
    let b = Decimal::from_str("3000000000000000000001").unwrap();
    let _ = a * b;
}

#[test]
fn it_can_mulassign() {
    let mut a = Decimal::from_str("1.25").unwrap();
    let b = Decimal::from_str("0.01").unwrap();

    a *= b;
    assert_eq!("0.0125", a.to_string());

    a *= &b;
    assert_eq!("0.000125", a.to_string());

    let mut c = &mut a;
    c *= b;
    assert_eq!("0.00000125", a.to_string());

    let mut c = &mut a;
    c *= &b;
    assert_eq!("0.0000000125", a.to_string());
}

// Division

#[test]
fn it_divides_decimals() {
    fn div(a: &str, b: &str, c: &str) {
        let a = Decimal::from_str(a).unwrap();
        let b = Decimal::from_str(b).unwrap();
        let result = a / b;
        assert_eq!(c, result.to_string(), "{} / {}", a.to_string(), b.to_string());
    }

    let tests = &[
        ("6", "3", "2"),
        ("10", "2", "5"),
        ("2.2", "1.1", "2"),
        ("-2.2", "-1.1", "2"),
        ("12.88", "5.6", "2.3"),
        ("1023427554493", "43432632", "23563.562864276795382789603908"),
        ("10000", "3", "3333.3333333333333333333333333"),
        ("2", "3", "0.6666666666666666666666666667"),
        ("1", "3", "0.3333333333333333333333333333"),
        ("-2", "3", "-0.6666666666666666666666666667"),
        ("2", "-3", "-0.6666666666666666666666666667"),
        ("-2", "-3", "0.6666666666666666666666666667"),
        ("1234.567890123456789012345678", "1.234567890123456789012345678", "1000"),
    ];
    for &(a, b, c) in tests {
        div(a, b, c);
    }
}

#[test]
#[should_panic]
fn it_can_divide_by_zero() {
    let a = Decimal::from_str("2").unwrap();
    let _ = a / Decimal::zero();
}

#[test]
fn it_can_divassign() {
    let mut a = Decimal::from_str("1.25").unwrap();
    let b = Decimal::from_str("0.01").unwrap();

    a /= b;
    assert_eq!("125", a.to_string());

    a /= &b;
    assert_eq!("12500", a.to_string());

    let mut c = &mut a;
    c /= b;
    assert_eq!("1250000", a.to_string());

    let mut c = &mut a;
    c /= &b;
    assert_eq!("125000000", a.to_string());
}

// Modulus and Remainder are not the same thing!
// https://math.stackexchange.com/q/801962/82277

#[test]
fn it_rems_decimals() {
    fn rem(a: &str, b: &str, c: &str) {
        let a = Decimal::from_str(a).unwrap();
        let b = Decimal::from_str(b).unwrap();
        // a = qb + r
        let result = a % b;
        assert_eq!(c, result.to_string(), "{} % {}", a.to_string(), b.to_string());
    }

    let tests = &[
        ("2", "3", "2"),
        ("-2", "3", "-2"),
        ("2", "-3", "2"),
        ("-2", "-3", "-2"),
        ("6", "3", "0"),
        ("42.2", "11.9", "6.5"),
        ("2.1", "3", "2.1"),
        ("2", "3.1", "2"),
        ("2.0", "3.1", "2.0"),
        ("4", "3.1", "0.9"),
    ];
    for &(a, b, c) in tests {
        rem(a, b, c);
    }
}

#[test]
fn it_can_remassign() {
    let mut a = Decimal::from_str("5").unwrap();
    let b = Decimal::from_str("2").unwrap();

    a %= b;
    assert_eq!("1", a.to_string());

    a %= &b;
    assert_eq!("1", a.to_string());

    let mut c = &mut a;
    c %= b;
    assert_eq!("1", a.to_string());

    let mut c = &mut a;
    c %= &b;
    assert_eq!("1", a.to_string());
}

#[test]
fn it_eqs_decimals() {
    fn eq(a: &str, b: &str, c: bool) {
        let a = Decimal::from_str(a).unwrap();
        let b = Decimal::from_str(b).unwrap();
        assert_eq!(c, a.eq(&b), "{} == {}", a.to_string(), b.to_string());
        assert_eq!(c, b.eq(&a), "{} == {}", b.to_string(), a.to_string());
    }

    let tests = &[
        ("1", "1", true),
        ("1", "-1", false),
        ("1", "1.00", true),
        ("1.2345000000000", "1.2345", true),
        ("1.0000000000000000000000000000", "1.0000000000000000000000000000", true),
        (
            "1.0000000000000000000000000001",
            "1.0000000000000000000000000000",
            false,
        ),
    ];
    for &(a, b, c) in tests {
        eq(a, b, c);
    }
}

#[test]
fn it_cmps_decimals() {
    fn cmp(a: &str, b: &str, c: Ordering) {
        let a = Decimal::from_str(a).unwrap();
        let b = Decimal::from_str(b).unwrap();
        assert_eq!(c, a.cmp(&b), "{} {:?} {}", a.to_string(), c, b.to_string());
    }

    let tests = &[
        ("1", "1", Equal),
        ("1", "-1", Greater),
        ("1", "1.00", Equal),
        ("1.2345000000000", "1.2345", Equal),
        (
            "1.0000000000000000000000000001",
            "1.0000000000000000000000000000",
            Greater,
        ),
        ("1.0000000000000000000000000000", "1.0000000000000000000000000001", Less),
        ("-1", "100", Less),
        ("-100", "1", Less),
        ("0", "0.5", Less),
        ("0.5", "0", Greater),
        ("100", "0.0098", Greater),
        ("1000000000000000", "999000000000000.0001", Greater),
        ("2.0001", "2.0001", Equal),
        (
            "11.815126050420168067226890757",
            "0.6386554621848739495798319328",
            Greater,
        ),
        ("0.6386554621848739495798319328", "11.815126050420168067226890757", Less),
        ("-0.5", "-0.01", Less),
        ("-0.5", "-0.1", Less),
        ("-0.01", "-0.5", Greater),
        ("-0.1", "-0.5", Greater),
    ];
    for &(a, b, c) in tests {
        cmp(a, b, c);
    }
}

#[test]
fn it_floors_decimals() {
    let tests = &[
        ("1", "1"),
        ("1.00", "1"),
        ("1.2345", "1"),
        ("-1", "-1"),
        ("-1.00", "-1"),
        ("-1.2345", "-2"),
    ];
    for &(a, expected) in tests {
        let a = Decimal::from_str(a).unwrap();
        assert_eq!(expected, a.floor().to_string(), "Failed flooring {}", a);
    }
}

#[test]
fn it_ceils_decimals() {
    let tests = &[
        ("1", "1"),
        ("1.00", "1"),
        ("1.2345", "2"),
        ("-1", "-1"),
        ("-1.00", "-1"),
        ("-1.2345", "-1"),
    ];
    for &(a, expected) in tests {
        let a = Decimal::from_str(a).unwrap();
        assert_eq!(expected, a.ceil().to_string(), "Failed ceiling {}", a);
    }
}

#[test]
fn it_finds_max_of_two() {
    let tests = &[("1", "1", "1"), ("2", "1", "2"), ("1", "2", "2")];
    for &(a, b, expected) in tests {
        let a = Decimal::from_str(a).unwrap();
        let b = Decimal::from_str(b).unwrap();
        assert_eq!(expected, a.max(b).to_string());
    }
}

#[test]
fn it_finds_min_of_two() {
    let tests = &[("1", "1", "1"), ("2", "1", "1"), ("1", "2", "1")];
    for &(a, b, expected) in tests {
        let a = Decimal::from_str(a).unwrap();
        let b = Decimal::from_str(b).unwrap();
        assert_eq!(expected, a.min(b).to_string());
    }
}

#[test]
fn test_max_compares() {
    let x = "225.33543601344182".parse::<Decimal>().unwrap();
    let y = Decimal::max_value();
    assert!(x < y);
    assert!(y > x);
    assert_ne!(y, x);
}

#[test]
fn test_min_compares() {
    let x = "225.33543601344182".parse::<Decimal>().unwrap();
    let y = Decimal::min_value();
    assert!(x > y);
    assert!(y < x);
    assert_ne!(y, x);
}

#[test]
fn it_can_parse_from_i32() {
    use num_traits::FromPrimitive;

    let tests = &[
        (0i32, "0"),
        (1i32, "1"),
        (-1i32, "-1"),
        (i32::max_value(), "2147483647"),
        (i32::min_value(), "-2147483648"),
    ];
    for &(input, expected) in tests {
        let parsed = Decimal::from_i32(input).unwrap();
        assert_eq!(
            expected,
            parsed.to_string(),
            "expected {} does not match parsed {}",
            expected,
            parsed
        );
        assert_eq!(
            input.to_string(),
            parsed.to_string(),
            "i32 to_string {} does not match parsed {}",
            input,
            parsed
        );
    }
}

#[test]
fn it_can_parse_from_i64() {
    use num_traits::FromPrimitive;

    let tests = &[
        (0i64, "0"),
        (1i64, "1"),
        (-1i64, "-1"),
        (i64::max_value(), "9223372036854775807"),
        (i64::min_value(), "-9223372036854775808"),
    ];
    for &(input, expected) in tests {
        let parsed = Decimal::from_i64(input).unwrap();
        assert_eq!(
            expected,
            parsed.to_string(),
            "expected {} does not match parsed {}",
            expected,
            parsed
        );
        assert_eq!(
            input.to_string(),
            parsed.to_string(),
            "i64 to_string {} does not match parsed {}",
            input,
            parsed
        );
    }
}

#[test]
fn it_can_round_to_2dp() {
    let a = Decimal::from_str("6.12345").unwrap();
    let b = (Decimal::from_str("100").unwrap() * a).round() / Decimal::from_str("100").unwrap();
    assert_eq!("6.12", b.to_string());
}

#[test]
fn it_can_round_using_bankers_rounding() {
    let tests = &[
        ("6.12345", 2, "6.12"),
        ("6.126", 2, "6.13"),
        ("-6.126", 2, "-6.13"),
        ("6.5", 0, "6"),
        ("7.5", 0, "8"),
        ("1.2250", 2, "1.22"),
        ("1.2252", 2, "1.23"),
        ("1.2249", 2, "1.22"),
        ("6.1", 2, "6.1"),
        ("0.0000", 2, "0.00"),
        ("0.6666666666666666666666666666", 2, "0.67"),
        ("1.40", 0, "1"),
        ("2.60", 0, "3"),
        ("2.1234567890123456789012345678", 27, "2.123456789012345678901234568"),
    ];
    for &(input, dp, expected) in tests {
        let a = Decimal::from_str(input).unwrap();
        let b = a.round_dp_with_strategy(dp, RoundingStrategy::BankersRounding);
        assert_eq!(expected, b.to_string());
    }
}

#[test]
fn it_can_round_complex_numbers_using_bankers_rounding() {
    // Issue #71
    let rate = Decimal::new(19, 2); // 0.19
    let one = Decimal::new(1, 0); // 1
    let part = rate / (rate + one); // 0.19 / (0.19 + 1) = 0.1596638655462184873949579832
    let part = part.round_dp_with_strategy(2, RoundingStrategy::BankersRounding); // 0.16
    assert_eq!("0.16", part.to_string());
}

#[test]
fn it_can_round_using_round_half_up() {
    let tests = &[
        ("0", 0, "0"),
        ("1.234", 3, "1.234"),
        ("1.12", 5, "1.12"),
        ("6.34567", 2, "6.35"),
        ("6.5", 0, "7"),
        ("12.49", 0, "12"),
        ("0.6666666666666666666666666666", 2, "0.67"),
        ("1.40", 0, "1"),
        ("2.60", 0, "3"),
        ("2.1234567890123456789012345678", 27, "2.123456789012345678901234568"),
    ];
    for &(input, dp, expected) in tests {
        let a = Decimal::from_str(input).unwrap();
        let b = a.round_dp_with_strategy(dp, RoundingStrategy::RoundHalfUp);
        assert_eq!(expected, b.to_string());
    }
}

#[test]
fn it_can_round_complex_numbers_using_round_half_up() {
    // Issue #71
    let rate = Decimal::new(19, 2); // 0.19
    let one = Decimal::new(1, 0); // 1
    let part = rate / (rate + one); // 0.19 / (0.19 + 1) = 0.1596638655462184873949579832
    let part = part.round_dp_with_strategy(2, RoundingStrategy::RoundHalfUp); // 0.16
    assert_eq!("0.16", part.to_string());
}

#[test]
fn it_can_round_using_round_half_down() {
    let tests = &[
        ("0", 0, "0"),
        ("1.234", 3, "1.234"),
        ("1.12", 5, "1.12"),
        ("6.34567", 2, "6.35"),
        ("6.51", 0, "7"),
        ("12.5", 0, "12"),
        ("0.6666666666666666666666666666", 2, "0.67"),
        ("1.40", 0, "1"),
        ("2.60", 0, "3"),
        ("2.1234567890123456789012345678", 27, "2.123456789012345678901234568"),
    ];
    for &(input, dp, expected) in tests {
        let a = Decimal::from_str(input).unwrap();
        let b = a.round_dp_with_strategy(dp, RoundingStrategy::RoundHalfDown);
        assert_eq!(expected, b.to_string());
    }
}

#[test]
fn it_can_round_complex_numbers_using_round_half_down() {
    // Issue #71
    let rate = Decimal::new(19, 2); // 0.19
    let one = Decimal::new(1, 0); // 1
    let part = rate / (rate + one); // 0.19 / (0.19 + 1) = 0.1596638655462184873949579832
    let part = part.round_dp_with_strategy(2, RoundingStrategy::RoundHalfDown); // 0.16
    assert_eq!("0.16", part.to_string());
}

#[test]
fn it_can_round_to_2dp_using_explicit_function() {
    let a = Decimal::from_str("6.12345").unwrap();
    let b = a.round_dp(2u32);
    assert_eq!("6.12", b.to_string());
}

#[test]
fn it_can_round_up_to_2dp_using_explicit_function() {
    let a = Decimal::from_str("6.126").unwrap();
    let b = a.round_dp(2u32);
    assert_eq!("6.13", b.to_string());
}

#[test]
fn it_can_round_down_to_2dp_using_explicit_function() {
    let a = Decimal::from_str("-6.126").unwrap();
    let b = a.round_dp(2u32);
    assert_eq!("-6.13", b.to_string());
}

#[test]
fn it_can_round_down_using_bankers_rounding() {
    let a = Decimal::from_str("6.5").unwrap();
    let b = a.round_dp(0u32);
    assert_eq!("6", b.to_string());
}

#[test]
fn it_can_round_up_using_bankers_rounding() {
    let a = Decimal::from_str("7.5").unwrap();
    let b = a.round_dp(0u32);
    assert_eq!("8", b.to_string());
}

#[test]
fn it_can_round_correctly_using_bankers_rounding_1() {
    let a = Decimal::from_str("1.2250").unwrap();
    let b = a.round_dp(2u32);
    assert_eq!("1.22", b.to_string());
}

#[test]
fn it_can_round_correctly_using_bankers_rounding_2() {
    let a = Decimal::from_str("1.2251").unwrap();
    let b = a.round_dp(2u32);
    assert_eq!("1.23", b.to_string());
}

#[test]
fn it_can_round_down_when_required() {
    let a = Decimal::from_str("1.2249").unwrap();
    let b = a.round_dp(2u32);
    assert_eq!("1.22", b.to_string());
}

#[test]
fn it_can_round_to_2dp_using_explicit_function_without_changing_value() {
    let a = Decimal::from_str("6.1").unwrap();
    let b = a.round_dp(2u32);
    assert_eq!("6.1", b.to_string());
}

#[test]
fn it_can_round_zero() {
    let a = Decimal::from_str("0.0000").unwrap();
    let b = a.round_dp(2u32);
    assert_eq!("0.00", b.to_string());
}

#[test]
fn it_can_round_large_decimals() {
    let a = Decimal::from_str("0.6666666666666666666666666666").unwrap();
    let b = a.round_dp(2u32);
    assert_eq!("0.67", b.to_string());
}

#[test]
fn it_can_round_simple_numbers_down() {
    let a = Decimal::from_str("1.40").unwrap();
    let b = a.round_dp(0u32);
    assert_eq!("1", b.to_string());
}

#[test]
fn it_can_round_simple_numbers_up() {
    let a = Decimal::from_str("2.60").unwrap();
    let b = a.round_dp(0u32);
    assert_eq!("3", b.to_string());
}

#[test]
fn it_can_round_simple_numbers_with_high_precision() {
    let a = Decimal::from_str("2.1234567890123456789012345678").unwrap();
    let b = a.round_dp(27u32);
    assert_eq!("2.123456789012345678901234568", b.to_string());
}

#[test]
fn it_can_round_complex_numbers() {
    // Issue #71
    let rate = Decimal::new(19, 2); // 0.19
    let one = Decimal::new(1, 0); // 1
    let part = rate / (rate + one); // 0.19 / (0.19 + 1) = 0.1596638655462184873949579832
    let part = part.round_dp(2); // 0.16
    assert_eq!("0.16", part.to_string());
}

#[test]
fn it_can_round_down() {
    let a = Decimal::new(470, 3).round_dp_with_strategy(1, RoundingStrategy::RoundDown);
    assert_eq!("0.4", a.to_string());
}

#[test]
fn it_only_rounds_down_when_needed() {
    let a = Decimal::new(400, 3).round_dp_with_strategy(1, RoundingStrategy::RoundDown);
    assert_eq!("0.4", a.to_string());
}

#[test]
fn it_can_round_up() {
    let a = Decimal::new(320, 3).round_dp_with_strategy(1, RoundingStrategy::RoundUp);
    assert_eq!("0.4", a.to_string());
}

#[test]
fn it_only_rounds_up_when_needed() {
    let a = Decimal::new(300, 3).round_dp_with_strategy(1, RoundingStrategy::RoundUp);
    assert_eq!("0.3", a.to_string());
}

#[test]
fn it_can_trunc() {
    let tests = &[("1.00000000000000000000", "1"), ("1.000000000000000000000001", "1")];

    for &(value, expected) in tests {
        let value = Decimal::from_str(value).unwrap();
        let expected = Decimal::from_str(expected).unwrap();
        let trunc = value.trunc();
        assert_eq!(expected.to_string(), trunc.to_string());
    }
}

#[test]
fn it_can_fract() {
    let tests = &[
        ("1.00000000000000000000", "0.00000000000000000000"),
        ("1.000000000000000000000001", "0.000000000000000000000001"),
    ];

    for &(value, expected) in tests {
        let value = Decimal::from_str(value).unwrap();
        let expected = Decimal::from_str(expected).unwrap();
        let fract = value.fract();
        assert_eq!(expected.to_string(), fract.to_string());
    }
}

#[test]
fn it_can_normalize() {
    let tests = &[
        ("1.00000000000000000000", "1"),
        ("1.10000000000000000000000", "1.1"),
        ("1.00010000000000000000000", "1.0001"),
        ("1", "1"),
        ("1.1", "1.1"),
        ("1.0001", "1.0001"),
        ("-0", "0"),
        ("-0.0", "0"),
        ("-0.010", "-0.01"),
        ("0.0", "0"),
    ];

    for &(value, expected) in tests {
        let value = Decimal::from_str(value).unwrap();
        let expected = Decimal::from_str(expected).unwrap();
        let normalized = value.normalize();
        assert_eq!(expected.to_string(), normalized.to_string());
    }
}

#[test]
fn it_can_return_the_max_value() {
    assert_eq!("79228162514264337593543950335", Decimal::max_value().to_string());
}

#[test]
fn it_can_return_the_min_value() {
    assert_eq!("-79228162514264337593543950335", Decimal::min_value().to_string());
}

#[test]
fn it_can_go_from_and_into() {
    let d = Decimal::from_str("5").unwrap();
    let di8 = 5u8.into();
    let di32 = 5i32.into();
    let disize = 5isize.into();
    let di64 = 5i64.into();
    let du8 = 5u8.into();
    let du32 = 5u32.into();
    let dusize = 5usize.into();
    let du64 = 5u64.into();

    assert_eq!(d, di8);
    assert_eq!(di8, di32);
    assert_eq!(di32, disize);
    assert_eq!(disize, di64);
    assert_eq!(di64, du8);
    assert_eq!(du8, du32);
    assert_eq!(du32, dusize);
    assert_eq!(dusize, du64);
}

#[test]
fn it_converts_to_f64() {
    assert_eq!(5f64, Decimal::from_str("5").unwrap().to_f64().unwrap());
    assert_eq!(-5f64, Decimal::from_str("-5").unwrap().to_f64().unwrap());
    assert_eq!(0.1f64, Decimal::from_str("0.1").unwrap().to_f64().unwrap());
    assert_eq!(0f64, Decimal::from_str("0.0").unwrap().to_f64().unwrap());
    assert_eq!(0f64, Decimal::from_str("-0.0").unwrap().to_f64().unwrap());
    assert_eq!(
        0.25e-11f64,
        Decimal::from_str("0.0000000000025").unwrap().to_f64().unwrap(),
    );
    assert_eq!(
        1e6f64,
        Decimal::from_str("1000000.0000000000025").unwrap().to_f64().unwrap()
    );
    assert_eq!(
        0.25e-25_f64,
        Decimal::from_str("0.000000000000000000000000025")
            .unwrap()
            .to_f64()
            .unwrap(),
    );
    assert_eq!(
        2.1234567890123456789012345678_f64,
        Decimal::from_str("2.1234567890123456789012345678")
            .unwrap()
            .to_f64()
            .unwrap(),
    );

    assert_eq!(
        None,
        // Cannot be represented in an f64
        Decimal::from_str("21234567890123456789012345678").unwrap().to_f64(),
    );
}

#[test]
fn it_converts_to_i64() {
    assert_eq!(5i64, Decimal::from_str("5").unwrap().to_i64().unwrap());
    assert_eq!(-5i64, Decimal::from_str("-5").unwrap().to_i64().unwrap());
    assert_eq!(5i64, Decimal::from_str("5.12345").unwrap().to_i64().unwrap());
    assert_eq!(-5i64, Decimal::from_str("-5.12345").unwrap().to_i64().unwrap());
    assert_eq!(
        0x7FFF_FFFF_FFFF_FFFF,
        Decimal::from_str("9223372036854775807").unwrap().to_i64().unwrap()
    );
    assert_eq!(None, Decimal::from_str("92233720368547758089").unwrap().to_i64());
}

#[test]
fn it_converts_to_u64() {
    assert_eq!(5u64, Decimal::from_str("5").unwrap().to_u64().unwrap());
    assert_eq!(None, Decimal::from_str("-5").unwrap().to_u64());
    assert_eq!(5u64, Decimal::from_str("5.12345").unwrap().to_u64().unwrap());
    assert_eq!(
        0xFFFF_FFFF_FFFF_FFFF,
        Decimal::from_str("18446744073709551615").unwrap().to_u64().unwrap()
    );
    assert_eq!(None, Decimal::from_str("18446744073709551616").unwrap().to_u64());
}

#[test]
fn it_converts_from_f32() {
    fn from_f32(f: f32) -> Option<Decimal> {
        num_traits::FromPrimitive::from_f32(f)
    }

    assert_eq!("1", from_f32(1f32).unwrap().to_string());
    assert_eq!("0", from_f32(0f32).unwrap().to_string());
    assert_eq!("0.12345", from_f32(0.12345f32).unwrap().to_string());
    assert_eq!(
        "0.12345678",
        from_f32(0.1234567800123456789012345678f32).unwrap().to_string()
    );
    assert_eq!(
        "0.12345679",
        from_f32(0.12345678901234567890123456789f32).unwrap().to_string()
    );
    assert_eq!("0", from_f32(0.00000000000000000000000000001f32).unwrap().to_string());

    assert!(from_f32(std::f32::NAN).is_none());
    assert!(from_f32(std::f32::INFINITY).is_none());

    // These both overflow
    assert!(from_f32(std::f32::MAX).is_none());
    assert!(from_f32(std::f32::MIN).is_none());
}

#[test]
fn it_converts_from_f64() {
    fn from_f64(f: f64) -> Option<Decimal> {
        num_traits::FromPrimitive::from_f64(f)
    }

    assert_eq!("1", from_f64(1f64).unwrap().to_string());
    assert_eq!("0", from_f64(0f64).unwrap().to_string());
    assert_eq!("0.12345", from_f64(0.12345f64).unwrap().to_string());
    assert_eq!(
        "0.1234567890123456",
        from_f64(0.1234567890123456089012345678f64).unwrap().to_string()
    );
    assert_eq!(
        "0.1234567890123457",
        from_f64(0.12345678901234567890123456789f64).unwrap().to_string()
    );
    assert_eq!("0", from_f64(0.00000000000000000000000000001f64).unwrap().to_string());
    assert_eq!("0.6927", from_f64(0.6927f64).unwrap().to_string());
    assert_eq!("0.00006927", from_f64(0.00006927f64).unwrap().to_string());
    assert_eq!("0.000000006927", from_f64(0.000000006927f64).unwrap().to_string());

    assert!(from_f64(std::f64::NAN).is_none());
    assert!(from_f64(std::f64::INFINITY).is_none());

    // These both overflow
    assert!(from_f64(std::f64::MAX).is_none());
    assert!(from_f64(std::f64::MIN).is_none());
}

#[test]
fn it_handles_simple_underflow() {
    // Issue #71
    let rate = Decimal::new(19, 2); // 0.19
    let one = Decimal::new(1, 0); // 1
    let part = rate / (rate + one); // 0.19 / (0.19 + 1) = 0.1596638655462184873949579832
    let result = one * part;
    assert_eq!("0.1596638655462184873949579832", result.to_string());

    // 169 * 0.1596638655462184873949579832 = 26.983193277310924
    let result = part * Decimal::new(169, 0);
    assert_eq!("26.983193277310924369747899161", result.to_string());
    let result = Decimal::new(169, 0) * part;
    assert_eq!("26.983193277310924369747899161", result.to_string());
}

#[test]
fn it_can_parse_highly_significant_numbers() {
    let tests = &[
        ("11.111111111111111111111111111", "11.111111111111111111111111111"),
        ("11.11111111111111111111111111111", "11.111111111111111111111111111"),
        ("11.1111111111111111111111111115", "11.111111111111111111111111112"),
        ("115.111111111111111111111111111", "115.11111111111111111111111111"),
        ("1115.11111111111111111111111111", "1115.1111111111111111111111111"),
        ("11.1111111111111111111111111195", "11.111111111111111111111111120"),
        ("99.9999999999999999999999999995", "100.00000000000000000000000000"),
        ("-11.1111111111111111111111111195", "-11.111111111111111111111111120"),
        ("-99.9999999999999999999999999995", "-100.00000000000000000000000000"),
        ("3.1415926535897932384626433832", "3.1415926535897932384626433832"),
        (
            "8808257419827262908.5944405087133154018",
            "8808257419827262908.594440509",
        ),
        (
            "8097370036018690744.2590371109596744091",
            "8097370036018690744.259037111",
        ),
        (
            "8097370036018690744.2590371149596744091",
            "8097370036018690744.259037115",
        ),
        (
            "8097370036018690744.2590371159596744091",
            "8097370036018690744.259037116",
        ),
    ];
    for &(value, expected) in tests {
        assert_eq!(expected, Decimal::from_str(value).unwrap().to_string());
    }
}

#[test]
fn it_can_parse_alternative_formats() {
    let tests = &[
        ("1_000", "1000"),
        ("1_000_000", "1000000"),
        ("10_000_000", "10000000"),
        ("100_000", "100000"),
        // At the moment, we'll accept this
        ("1_____________0", "10"),
    ];
    for &(value, expected) in tests {
        assert_eq!(expected, Decimal::from_str(value).unwrap().to_string());
    }
}

#[test]
fn it_can_parse_fractional_numbers_with_underscore_separators() {
    let a = Decimal::from_str("0.1_23_456").unwrap();
    assert_eq!(a.is_sign_negative(), false);
    assert_eq!(a.scale(), 6);
    assert_eq!("0.123456", a.to_string());
}

#[test]
fn it_can_parse_numbers_with_underscore_separators_before_decimal_point() {
    let a = Decimal::from_str("1_234.56").unwrap();
    assert_eq!(a.is_sign_negative(), false);
    assert_eq!(a.scale(), 2);
    assert_eq!("1234.56", a.to_string());
}

#[test]
fn it_can_parse_numbers_and_round_correctly_with_underscore_separators_before_decimal_point() {
    let tests = &[
        (
            "8_097_370_036_018_690_744.2590371159596744091",
            "8097370036018690744.259037116",
        ),
        (
            "8097370036018690744.259_037_115_959_674_409_1",
            "8097370036018690744.259037116",
        ),
        (
            "8_097_370_036_018_690_744.259_037_115_959_674_409_1",
            "8097370036018690744.259037116",
        ),
    ];
    for &(value, expected) in tests {
        assert_eq!(expected, Decimal::from_str(value).unwrap().to_string());
    }
}

#[test]
fn it_can_reject_invalid_formats() {
    let tests = &["_1", "1.0.0", "10_00.0_00.0"];
    for &value in tests {
        assert!(
            Decimal::from_str(value).is_err(),
            "This succeeded unexpectedly: {}",
            value
        );
    }
}

#[test]
fn it_can_reject_large_numbers_with_panic() {
    let tests = &[
        // The maximum number supported is 79,228,162,514,264,337,593,543,950,335
        "79228162514264337593543950336",
        "79228162514264337593543950337",
        "79228162514264337593543950338",
        "79228162514264337593543950339",
        "79228162514264337593543950340",
    ];
    for &value in tests {
        assert!(
            Decimal::from_str(value).is_err(),
            "This succeeded unexpectedly: {}",
            value
        );
    }
}

#[test]
fn it_can_parse_individual_parts() {
    let pi = Decimal::from_parts(1102470952, 185874565, 1703060790, false, 28);
    assert_eq!(pi.to_string(), "3.1415926535897932384626433832");
}

#[test]
fn it_can_parse_scientific_notation() {
    let tests = &[
        ("9.7e-7", "0.00000097"),
        ("9e-7", "0.0000009"),
        ("1.2e10", "12000000000"),
        ("1.2e+10", "12000000000"),
        ("12e10", "120000000000"),
        ("9.7E-7", "0.00000097"),
    ];

    for &(value, expected) in tests {
        assert_eq!(expected, Decimal::from_scientific(value).unwrap().to_string());
    }
}

#[test]
fn it_can_parse_different_radix() {
    use num_traits::Num;

    let tests = &[
        // Input, Radix, Success, to_string()
        ("123", 10, true, "123"),
        ("123", 8, true, "83"),
        ("123", 16, true, "291"),
        ("abc", 10, false, ""),
        ("abc", 16, true, "2748"),
        ("78", 10, true, "78"),
        ("78", 8, false, ""),
        ("101", 2, true, "5"),
        // Parse base 2
        ("1111_1111_1111_1111_1111_1111_1111_1111", 2, true, "4294967295"),
        // Max supported value
        (
            "1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_\
          1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111_1111",
            2,
            true,
            &Decimal::max_value().to_string(),
        ),
        // We limit to 28 dp
        (
            "843.6500000000000000000000000000",
            10,
            true,
            "843.6500000000000000000000000",
        ),
    ];

    for &(input, radix, success, expected) in tests {
        let result = Decimal::from_str_radix(input, radix);
        assert_eq!(
            success,
            result.is_ok(),
            "Failed to parse: {} radix {}: {:?}",
            input,
            radix,
            result.err()
        );
        if result.is_ok() {
            assert_eq!(
                expected,
                result.unwrap().to_string(),
                "Original input: {} radix {}",
                input,
                radix
            );
        }
    }
}

#[test]
fn it_can_calculate_signum() {
    let tests = &[("123", 1), ("-123", -1), ("0", 0)];

    for &(input, expected) in tests {
        let input = Decimal::from_str(input).unwrap();
        assert_eq!(expected, input.signum().to_i32().unwrap(), "Input: {}", input);
    }
}

#[test]
fn it_can_calculate_abs_sub() {
    let tests = &[
        ("123", "124", 0),
        ("123", "123", 0),
        ("123", "122", 123),
        ("-123", "-124", 123),
        ("-123", "-123", 0),
        ("-123", "-122", 0),
    ];

    for &(input1, input2, expected) in tests {
        let input1 = Decimal::from_str(input1).unwrap();
        let input2 = Decimal::from_str(input2).unwrap();
        assert_eq!(
            expected,
            input1.abs_sub(&input2).to_i32().unwrap(),
            "Input: {} {}",
            input1,
            input2
        );
    }
}

#[test]
#[should_panic]
fn it_panics_when_scale_too_large() {
    let _ = Decimal::new(1, 29);
}

#[test]
fn test_zero_eq_negative_zero() {
    let zero: Decimal = 0.into();

    assert!(zero == zero);
    assert!(-zero == zero);
    assert!(zero == -zero);
}

#[cfg(feature = "postgres")]
#[test]
fn to_from_sql() {
    use bytes::BytesMut;
    use postgres::types::{FromSql, Kind, ToSql, Type};

    let tests = &[
        "3950.123456",
        "3950",
        "0.1",
        "0.01",
        "0.001",
        "0.0001",
        "0.00001",
        "0.000001",
        "1",
        "-100",
        "-123.456",
        "119996.25",
        "1000000",
        "9999999.99999",
        "12340.56789",
        "79228162514264337593543950335", // 0xFFFF_FFFF_FFFF_FFFF_FFFF_FFFF (96 bit)
        "4951760157141521099596496895",  // 0x0FFF_FFFF_FFFF_FFFF_FFFF_FFFF (95 bit)
        "4951760157141521099596496896",  // 0x1000_0000_0000_0000_0000_0000
        "18446744073709551615",
        "-18446744073709551615",
    ];

    let t = Type::new("".into(), 0, Kind::Simple, "".into());

    for test in tests {
        let input = Decimal::from_str(test).unwrap();
        let mut bytes = BytesMut::new();
        input.to_sql(&t, &mut bytes).unwrap();
        let output = Decimal::from_sql(&t, &bytes).unwrap();

        assert_eq!(input, output);
    }
}

fn hash_it(d: Decimal) -> u64 {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::Hash;
    use std::hash::Hasher;

    let mut h = DefaultHasher::new();
    d.hash(&mut h);
    h.finish()
}

#[test]
fn it_computes_equal_hashes_for_equal_values() {
    // From the Rust Hash docs:
    //
    // "When implementing both Hash and Eq, it is important that the following property holds:
    //
    //     k1 == k2 -> hash(k1) == hash(k2)"

    let k1 = Decimal::from_str("1").unwrap();
    let k2 = Decimal::from_str("1.0").unwrap();
    let k3 = Decimal::from_str("1.00").unwrap();
    let k4 = Decimal::from_str("1.01").unwrap();

    assert_eq!(k1, k2);
    assert_eq!(k1, k3);
    assert_ne!(k1, k4);

    let h1 = hash_it(k1);
    let h2 = hash_it(k2);
    let h3 = hash_it(k3);
    let h4 = hash_it(k4);

    assert_eq!(h1, h2);
    assert_eq!(h1, h3);
    assert_ne!(h1, h4);

    // Test the application of Hash calculation to a HashMap.

    use std::collections::HashMap;

    let mut map = HashMap::new();

    map.insert(k1, k1.to_string());
    // map[k2] should overwrite map[k1] because k1 == k2.
    map.insert(k2, k2.to_string());

    assert_eq!("1.0", map.get(&k3).expect("could not get k3"));
    assert_eq!(1, map.len());

    // map[k3] should overwrite map[k2] because k3 == k2.
    map.insert(k3, k3.to_string());
    // map[k4] should not overwrite map[k3] because k4 != k3.
    map.insert(k4, k4.to_string());

    assert_eq!(2, map.len());
    assert_eq!("1.00", map.get(&k1).expect("could not get k1"));
}

#[test]
fn it_computes_equal_hashes_for_positive_and_negative_zero() {
    // Verify 0 and -0 have the same hash
    let k1 = Decimal::from_str("0").unwrap();
    let k2 = Decimal::from_str("-0").unwrap();
    assert_eq!("-0", k2.to_string());
    assert_eq!(k1, k2);
    let h1 = hash_it(k1);
    let h2 = hash_it(k2);
    assert_eq!(h1, h2);

    // Verify 0 and -0.0 have the same hash
    let k1 = Decimal::from_str("0").unwrap();
    let k2 = Decimal::from_str("-0.0").unwrap();
    assert_eq!("-0.0", k2.to_string());
    assert_eq!(k1, k2);
    let h1 = hash_it(k1);
    let h2 = hash_it(k2);
    assert_eq!(h1, h2);
}

#[test]
#[should_panic]
fn it_handles_i128_min() {
    Decimal::from_i128_with_scale(std::i128::MIN, 0);
}

#[test]
fn it_can_rescale() {
    let tests = &[
        ("0", 6, "0.000000"),
        ("0.000000", 2, "0.00"),
        ("0.12345600000", 6, "0.123456"),
        ("0.123456", 12, "0.123456000000"),
        ("0.123456", 0, "0"),
        ("0.000001", 4, "0.0000"),
        ("1233456", 4, "1233456.0000"),
        ("1.2", 30, "1.2000000000000000000000000000"),
        ("79228162514264337593543950335", 0, "79228162514264337593543950335"),
        ("4951760157141521099596496895", 1, "4951760157141521099596496895.0"),
        ("4951760157141521099596496896", 1, "4951760157141521099596496896.0"),
        ("18446744073709551615", 6, "18446744073709551615.000000"),
        ("-18446744073709551615", 6, "-18446744073709551615.000000"),
    ];

    for &(value_raw, new_scale, expected_value) in tests {
        let new_value = Decimal::from_str(expected_value).unwrap();
        let mut value = Decimal::from_str(value_raw).unwrap();
        value.rescale(new_scale);
        assert_eq!(new_value.to_string(), value.to_string());
    }
}
