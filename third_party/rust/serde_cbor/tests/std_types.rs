#[macro_use]
extern crate serde_derive;

#[cfg(feature = "std")]
mod std_tests {
    use std::u8;

    use serde_cbor::de::from_mut_slice;
    use serde_cbor::ser::{to_vec, to_vec_packed};
    use serde_cbor::{from_reader, from_slice};

    fn to_binary(s: &'static str) -> Vec<u8> {
        assert!(s.len() % 2 == 0);
        let mut b = Vec::with_capacity(s.len() / 2);
        for i in 0..s.len() / 2 {
            b.push(u8::from_str_radix(&s[i * 2..(i + 1) * 2], 16).unwrap());
        }
        b
    }

    macro_rules! testcase {
        ($name:ident, f64, $expr:expr, $s:expr) => {
            #[test]
            fn $name() {
                let expr: f64 = $expr;
                let mut serialized = to_binary($s);
                assert_eq!(to_vec(&expr).unwrap(), serialized);
                let parsed: f64 = from_slice(&serialized[..]).unwrap();
                if !expr.is_nan() {
                    assert_eq!(expr, parsed);
                } else {
                    assert!(parsed.is_nan())
                }

                let parsed: f64 = from_reader(&mut &serialized[..]).unwrap();
                if !expr.is_nan() {
                    assert_eq!(expr, parsed);
                } else {
                    assert!(parsed.is_nan())
                }

                let parsed: f64 = from_mut_slice(&mut serialized[..]).unwrap();
                if !expr.is_nan() {
                    assert_eq!(expr, parsed);
                } else {
                    assert!(parsed.is_nan())
                }
            }
        };
        ($name:ident, $ty:ty, $expr:expr, $s:expr) => {
            #[test]
            fn $name() {
                let expr: $ty = $expr;
                let mut serialized = to_binary($s);
                assert_eq!(
                    to_vec(&expr).expect("ser1 works"),
                    serialized,
                    "serialization differs"
                );
                let parsed: $ty = from_slice(&serialized[..]).expect("de1 works");
                assert_eq!(parsed, expr, "parsed result differs");
                let packed = &to_vec_packed(&expr).expect("serializing packed")[..];
                let parsed_from_packed: $ty = from_slice(packed).expect("parsing packed");
                assert_eq!(parsed_from_packed, expr, "packed roundtrip fail");

                let parsed: $ty = from_reader(&mut &serialized[..]).unwrap();
                assert_eq!(parsed, expr, "parsed result differs");
                let mut packed = to_vec_packed(&expr).expect("serializing packed");
                let parsed_from_packed: $ty =
                    from_reader(&mut &packed[..]).expect("parsing packed");
                assert_eq!(parsed_from_packed, expr, "packed roundtrip fail");

                let parsed: $ty = from_mut_slice(&mut serialized[..]).unwrap();
                assert_eq!(parsed, expr, "parsed result differs");
                let parsed_from_packed: $ty =
                    from_mut_slice(&mut packed[..]).expect("parsing packed");
                assert_eq!(parsed_from_packed, expr, "packed roundtrip fail");
            }
        };
    }

    testcase!(test_bool_false, bool, false, "f4");
    testcase!(test_bool_true, bool, true, "f5");
    testcase!(test_isize_neg_256, isize, -256, "38ff");
    testcase!(test_isize_neg_257, isize, -257, "390100");
    testcase!(test_isize_255, isize, 255, "18ff");
    testcase!(test_i8_5, i8, 5, "05");
    testcase!(test_i8_23, i8, 23, "17");
    testcase!(test_i8_24, i8, 24, "1818");
    testcase!(test_i8_neg_128, i8, -128, "387f");
    testcase!(test_u32_98745874, u32, 98745874, "1a05e2be12");
    testcase!(test_f32_1234_point_5, f32, 1234.5, "fa449a5000");
    testcase!(test_f64_12345_point_6, f64, 12345.6, "fb40c81ccccccccccd");
    testcase!(test_f64_nan, f64, ::std::f64::NAN, "f97e00");
    testcase!(test_f64_infinity, f64, ::std::f64::INFINITY, "f97c00");
    testcase!(test_f64_neg_infinity, f64, -::std::f64::INFINITY, "f9fc00");
    testcase!(test_char_null, char, '\x00', "6100");
    testcase!(test_char_broken_heart, char, '💔', "64f09f9294");
    testcase!(
        test_str_pangram_de,
        String,
        "aâø↓é".to_owned(),
        "6a61c3a2c3b8e28693c3a9"
    );
    testcase!(test_unit, (), (), "f6");

    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct UnitStruct;
    testcase!(test_unit_struct, UnitStruct, UnitStruct, "f6");

    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct NewtypeStruct(bool);
    testcase!(
        test_newtype_struct,
        NewtypeStruct,
        NewtypeStruct(true),
        "f5"
    );

    testcase!(test_option_none, Option<u8>, None, "f6");
    testcase!(test_option_some, Option<u8>, Some(42), "182a");

    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct Person {
        name: String,
        year_of_birth: u16,
        profession: Option<String>,
    }

    testcase!(test_person_struct,
    Person,
    Person {
        name: "Grace Hopper".to_string(),
        year_of_birth: 1906,
        profession: Some("computer scientist".to_string()),
    },
    "a3646e616d656c477261636520486f707065726d796561725f6f665f62697274681907726a70726f66657373696f6e72636f6d707574657220736369656e74697374");

    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    struct OptionalPerson {
        name: String,
        #[serde(skip_serializing_if = "Option::is_none")]
        year_of_birth: Option<u16>,
        profession: Option<String>,
    }

    testcase!(test_optional_person_struct,
    OptionalPerson,
    OptionalPerson {
        name: "Grace Hopper".to_string(),
        year_of_birth: None,
        profession: Some("computer scientist".to_string()),
    },
    "a2646e616d656c477261636520486f707065726a70726f66657373696f6e72636f6d707574657220736369656e74697374");

    #[derive(Debug, PartialEq, Deserialize, Serialize)]
    enum Color {
        Red,
        Blue,
        Yellow,
        Other(u64),
        Alpha(u64, u8),
    }

    testcase!(test_color_enum, Color, Color::Blue, "64426c7565");
    testcase!(
        test_color_enum_transparent,
        Color,
        Color::Other(42),
        "a1654f74686572182a"
    );
    testcase!(
        test_color_enum_with_alpha,
        Color,
        Color::Alpha(234567, 60),
        "a165416c706861821a00039447183c"
    );
    testcase!(test_i128_a, i128, -1i128, "20");
    testcase!(
        test_i128_b,
        i128,
        -18446744073709551616i128,
        "3BFFFFFFFFFFFFFFFF"
    );
    testcase!(test_u128, u128, 17, "11");
}
