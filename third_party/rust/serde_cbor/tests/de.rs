#[macro_use]
extern crate serde_derive;

use serde_cbor;
use serde_cbor::de;

#[test]
fn test_str() {
    let s: &str =
        de::from_slice_with_scratch(&[0x66, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72], &mut []).unwrap();
    assert_eq!(s, "foobar");
}

#[test]
fn test_bytes() {
    let s: &[u8] =
        de::from_slice_with_scratch(&[0x46, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72], &mut []).unwrap();
    assert_eq!(s, b"foobar");
}

#[test]
fn test_int() {
    let num: i64 = de::from_slice_with_scratch(&[0x39, 0x07, 0xde], &mut []).unwrap();
    assert_eq!(num, -2015);
}

#[test]
fn test_float() {
    let float: f64 = de::from_slice_with_scratch(b"\xfa\x47\xc3\x50\x00", &mut []).unwrap();
    assert_eq!(float, 100000.0);
}

#[test]
fn test_indefinite_object() {
    #[derive(Debug, Deserialize, PartialEq)]
    struct Foo {
        a: u64,
        b: [u64; 2],
    }
    let expected = Foo { a: 1, b: [2, 3] };
    let actual: Foo =
        de::from_slice_with_scratch(b"\xbfaa\x01ab\x9f\x02\x03\xff\xff", &mut []).unwrap();
    assert_eq!(expected, actual);
}

#[cfg(feature = "std")]
mod std_tests {
    use std::collections::BTreeMap;

    use serde::de as serde_de;
    use serde_cbor::value::Value;
    use serde_cbor::{de, error, to_vec, Deserializer};

    #[test]
    fn test_string1() {
        let value: error::Result<Value> =
            de::from_slice(&[0x66, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72]);
        assert_eq!(value.unwrap(), Value::Text("foobar".to_owned()));
    }

    #[test]
    fn test_string2() {
        let value: error::Result<Value> = de::from_slice(&[
            0x71, 0x49, 0x20, 0x6d, 0x65, 0x74, 0x20, 0x61, 0x20, 0x74, 0x72, 0x61, 0x76, 0x65,
            0x6c, 0x6c, 0x65, 0x72,
        ]);
        assert_eq!(value.unwrap(), Value::Text("I met a traveller".to_owned()));
    }

    #[test]
    fn test_string3() {
        let slice = b"\x78\x2fI met a traveller from an antique land who said";
        let value: error::Result<Value> = de::from_slice(slice);
        assert_eq!(
            value.unwrap(),
            Value::Text("I met a traveller from an antique land who said".to_owned())
        );
    }

    #[test]
    fn test_byte_string() {
        let value: error::Result<Value> =
            de::from_slice(&[0x46, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72]);
        assert_eq!(value.unwrap(), Value::Bytes(b"foobar".to_vec()));
    }

    #[test]
    fn test_numbers1() {
        let value: error::Result<Value> = de::from_slice(&[0x00]);
        assert_eq!(value.unwrap(), Value::Integer(0));
    }

    #[test]
    fn test_numbers2() {
        let value: error::Result<Value> = de::from_slice(&[0x1a, 0x00, 0xbc, 0x61, 0x4e]);
        assert_eq!(value.unwrap(), Value::Integer(12345678));
    }

    #[test]
    fn test_numbers3() {
        let value: error::Result<Value> = de::from_slice(&[0x39, 0x07, 0xde]);
        assert_eq!(value.unwrap(), Value::Integer(-2015));
    }

    #[test]
    fn test_bool() {
        let value: error::Result<Value> = de::from_slice(b"\xf4");
        assert_eq!(value.unwrap(), Value::Bool(false));
    }

    #[test]
    fn test_trailing_bytes() {
        let value: error::Result<Value> = de::from_slice(b"\xf4trailing");
        assert!(value.is_err());
    }

    #[test]
    fn test_list1() {
        let value: error::Result<Value> = de::from_slice(b"\x83\x01\x02\x03");
        assert_eq!(
            value.unwrap(),
            Value::Array(vec![
                Value::Integer(1),
                Value::Integer(2),
                Value::Integer(3)
            ])
        );
    }

    #[test]
    fn test_list2() {
        let value: error::Result<Value> = de::from_slice(b"\x82\x01\x82\x02\x81\x03");
        assert_eq!(
            value.unwrap(),
            Value::Array(vec![
                Value::Integer(1),
                Value::Array(vec![
                    Value::Integer(2),
                    Value::Array(vec![Value::Integer(3)])
                ])
            ])
        );
    }

    #[test]
    fn test_object() {
        let value: error::Result<Value> = de::from_slice(b"\xa5aaaAabaBacaCadaDaeaE");
        let mut object = BTreeMap::new();
        object.insert(Value::Text("a".to_owned()), Value::Text("A".to_owned()));
        object.insert(Value::Text("b".to_owned()), Value::Text("B".to_owned()));
        object.insert(Value::Text("c".to_owned()), Value::Text("C".to_owned()));
        object.insert(Value::Text("d".to_owned()), Value::Text("D".to_owned()));
        object.insert(Value::Text("e".to_owned()), Value::Text("E".to_owned()));
        assert_eq!(value.unwrap(), Value::Map(object));
    }

    #[test]
    fn test_indefinite_object() {
        let value: error::Result<Value> = de::from_slice(b"\xbfaa\x01ab\x9f\x02\x03\xff\xff");
        let mut object = BTreeMap::new();
        object.insert(Value::Text("a".to_owned()), Value::Integer(1));
        object.insert(
            Value::Text("b".to_owned()),
            Value::Array(vec![Value::Integer(2), Value::Integer(3)]),
        );
        assert_eq!(value.unwrap(), Value::Map(object));
    }

    #[test]
    fn test_indefinite_list() {
        let value: error::Result<Value> = de::from_slice(b"\x9f\x01\x02\x03\xff");
        assert_eq!(
            value.unwrap(),
            Value::Array(vec![
                Value::Integer(1),
                Value::Integer(2),
                Value::Integer(3)
            ])
        );
    }

    #[test]
    fn test_indefinite_string() {
        let value: error::Result<Value> =
            de::from_slice(b"\x7f\x65Mary \x64Had \x62a \x67Little \x60\x64Lamb\xff");
        assert_eq!(
            value.unwrap(),
            Value::Text("Mary Had a Little Lamb".to_owned())
        );
    }

    #[test]
    fn test_indefinite_byte_string() {
        let value: error::Result<Value> = de::from_slice(b"\x5f\x42\x01\x23\x42\x45\x67\xff");
        assert_eq!(value.unwrap(), Value::Bytes(b"\x01#Eg".to_vec()));
    }

    #[test]
    fn test_multiple_indefinite_strings() {
        let input = b"\x82\x7f\x65Mary \x64Had \x62a \x67Little \x60\x64Lamb\xff\x5f\x42\x01\x23\x42\x45\x67\xff";
        _test_multiple_indefinite_strings(de::from_slice(input));
        _test_multiple_indefinite_strings(de::from_mut_slice(input.to_vec().as_mut()));
        let mut buf = [0u8; 64];
        _test_multiple_indefinite_strings(de::from_slice_with_scratch(input, &mut buf));
    }
    fn _test_multiple_indefinite_strings(value: error::Result<Value>) {
        // This assures that buffer rewinding in infinite buffers works as intended.
        assert_eq!(
            value.unwrap(),
            Value::Array(vec![
                Value::Text("Mary Had a Little Lamb".to_owned()),
                Value::Bytes(b"\x01#Eg".to_vec())
            ])
        );
    }

    #[test]
    fn test_float() {
        let value: error::Result<Value> = de::from_slice(b"\xfa\x47\xc3\x50\x00");
        assert_eq!(value.unwrap(), Value::Float(100000.0));
    }

    #[test]
    fn test_self_describing() {
        let value: error::Result<Value> =
            de::from_slice(&[0xd9, 0xd9, 0xf7, 0x66, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72]);
        let expected = Value::Text("foobar".to_owned());
        let strip_tags = |x: Value| {
            if let Value::Tag(_, inner) = x {
                *inner
            } else {
                x
            }
        };
        assert_eq!(strip_tags(value.unwrap()), expected);
    }

    #[test]
    fn test_f16() {
        let mut x: Value = de::from_slice(&[0xf9, 0x41, 0x00]).unwrap();
        assert_eq!(x, Value::Float(2.5));
        x = de::from_slice(&[0xf9, 0x41, 0x90]).unwrap();
        assert_eq!(x, Value::Float(2.78125));
        x = de::from_slice(&[0xf9, 0x50, 0x90]).unwrap();
        assert_eq!(x, Value::Float(36.5));
        x = de::from_slice(&[0xf9, 0xd0, 0x90]).unwrap();
        assert_eq!(x, Value::Float(-36.5));
    }

    #[test]
    fn test_crazy_list() {
        let slice = b"\x88\x1b\x00\x00\x00\x1c\xbe\x99\x1d\xc7\x3b\x00\x7a\xcf\x51\xdc\x51\x70\xdb\x3a\x1b\x3a\x06\xdd\xf5\xf6\xf7\xfb\x41\x76\x5e\xb1\xf8\x00\x00\x00\xf9\x7c\x00";
        let value: Vec<Value> = de::from_slice(slice).unwrap();
        assert_eq!(
            value,
            vec![
                Value::Integer(123456789959),
                Value::Integer(-34567897654325468),
                Value::Integer(-456787678),
                Value::Bool(true),
                Value::Null,
                Value::Null,
                Value::Float(23456543.5),
                Value::Float(::std::f64::INFINITY)
            ]
        );
    }

    #[test]
    fn test_nan() {
        let value: f64 = de::from_slice(b"\xf9\x7e\x00").unwrap();
        assert!(value.is_nan());
    }

    #[test]
    fn test_32f16() {
        let value: f32 = de::from_slice(b"\xf9\x50\x00").unwrap();
        assert_eq!(value, 32.0f32);
    }

    #[test]
    // The file was reported as not working by user kie0tauB
    // but it parses to a cbor value.
    fn test_kietaub_file() {
        let file = include_bytes!("kietaub.cbor");
        let value_result: error::Result<Value> = de::from_slice(file);
        value_result.unwrap();
    }

    #[test]
    fn test_option_roundtrip() {
        let obj1 = Some(10u32);

        let v = to_vec(&obj1).unwrap();
        let obj2: Result<Option<u32>, _> = serde_cbor::de::from_reader(&v[..]);
        println!("{:?}", obj2);

        assert_eq!(obj1, obj2.unwrap());
    }

    #[test]
    fn test_option_none_roundtrip() {
        let obj1 = None;

        let v = to_vec(&obj1).unwrap();
        println!("{:?}", v);
        let obj2: Result<Option<u32>, _> = serde_cbor::de::from_reader(&v[..]);

        assert_eq!(obj1, obj2.unwrap());
    }

    #[test]
    fn test_variable_length_map() {
        let slice = b"\xbf\x67\x6d\x65\x73\x73\x61\x67\x65\x64\x70\x6f\x6e\x67\xff";
        let value: Value = de::from_slice(slice).unwrap();
        let mut map = BTreeMap::new();
        map.insert(
            Value::Text("message".to_string()),
            Value::Text("pong".to_string()),
        );
        assert_eq!(value, Value::Map(map))
    }

    #[test]
    fn test_object_determinism_roundtrip() {
        let expected = b"\xa2aa\x01ab\x82\x02\x03";

        // 0.1% chance of not catching failure
        for _ in 0..10 {
            assert_eq!(
                &to_vec(&de::from_slice::<Value>(expected).unwrap()).unwrap(),
                expected
            );
        }
    }

    #[test]
    fn stream_deserializer() {
        let slice = b"\x01\x66foobar";
        let mut it = Deserializer::from_slice(slice).into_iter::<Value>();
        assert_eq!(Value::Integer(1), it.next().unwrap().unwrap());
        assert_eq!(
            Value::Text("foobar".to_string()),
            it.next().unwrap().unwrap()
        );
        assert!(it.next().is_none());
    }

    #[test]
    fn stream_deserializer_eof() {
        let slice = b"\x01\x66foob";
        let mut it = Deserializer::from_slice(slice).into_iter::<Value>();
        assert_eq!(Value::Integer(1), it.next().unwrap().unwrap());
        assert!(it.next().unwrap().unwrap_err().is_eof());
    }

    #[test]
    fn stream_deserializer_eof_in_indefinite() {
        let slice = b"\x7f\x65Mary \x64Had \x62a \x60\x67Little \x60\x64Lamb\xff";
        let indices: &[usize] = &[
            2,  // announcement but no data
            10, // mid-buffer EOF
            12, // neither new element nor end marker
        ];
        for end_of_slice in indices {
            let mut it = Deserializer::from_slice(&slice[..*end_of_slice]).into_iter::<Value>();
            assert!(it.next().unwrap().unwrap_err().is_eof());

            let mut mutcopy = slice[..*end_of_slice].to_vec();
            let mut it = Deserializer::from_mut_slice(mutcopy.as_mut()).into_iter::<Value>();
            assert!(it.next().unwrap().unwrap_err().is_eof());

            let mut buf = [0u8; 64];
            let mut it = Deserializer::from_slice_with_scratch(&slice[..*end_of_slice], &mut buf)
                .into_iter::<Value>();
            assert!(it.next().unwrap().unwrap_err().is_eof());
        }
    }

    #[test]
    fn crash() {
        let file = include_bytes!("crash.cbor");
        let value_result: error::Result<Value> = de::from_slice(file);
        assert_eq!(
            value_result.unwrap_err().classify(),
            serde_cbor::error::Category::Syntax
        );
    }

    fn from_slice_stream<'a, T>(slice: &'a [u8]) -> error::Result<(&'a [u8], T)>
    where
        T: serde_de::Deserialize<'a>,
    {
        let mut deserializer = Deserializer::from_slice(slice);
        let value = serde_de::Deserialize::deserialize(&mut deserializer)?;
        let rest = &slice[deserializer.byte_offset()..];

        Ok((rest, value))
    }

    #[test]
    fn test_slice_offset() {
        let v: Vec<u8> = vec![
            0x66, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72, 0x66, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72,
        ];
        let (rest, value): (&[u8], String) = from_slice_stream(&v[..]).unwrap();
        assert_eq!(value, "foobar");
        assert_eq!(rest, &[0x66, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72]);
        let (rest, value): (&[u8], String) = from_slice_stream(rest).unwrap();
        assert_eq!(value, "foobar");
        assert_eq!(rest, &[]);
    }

    #[derive(Debug, Copy, Clone)]
    struct Options {
        standard: bool,
        legacy: bool,
        packed: bool,
        named: bool,
    }

    impl Default for Options {
        fn default() -> Self {
            Options {
                standard: true,
                legacy: true,
                packed: true,
                named: true,
            }
        }
    }

    impl Options {
        fn no_standard(self) -> Self {
            Options {
                standard: false,
                ..self
            }
        }

        fn no_legacy(self) -> Self {
            Options {
                legacy: false,
                ..self
            }
        }

        fn no_packed(self) -> Self {
            Options {
                packed: false,
                ..self
            }
        }

        fn no_named(self) -> Self {
            Options {
                named: false,
                ..self
            }
        }
    }

    fn from_slice_stream_options<'a, T>(
        slice: &'a [u8],
        options: Options,
    ) -> error::Result<(&'a [u8], T)>
    where
        T: serde_de::Deserialize<'a>,
    {
        let deserializer = Deserializer::from_slice(slice);
        let deserializer = if !options.packed {
            deserializer.disable_packed_format()
        } else {
            deserializer
        };
        let deserializer = if !options.named {
            deserializer.disable_named_format()
        } else {
            deserializer
        };
        let deserializer = if !options.standard {
            deserializer.disable_standard_enums()
        } else {
            deserializer
        };
        let mut deserializer = if !options.legacy {
            deserializer.disable_legacy_enums()
        } else {
            deserializer
        };
        let value = serde_de::Deserialize::deserialize(&mut deserializer)?;
        let rest = &slice[deserializer.byte_offset()..];

        Ok((rest, value))
    }

    #[test]
    fn test_deserializer_enums() {
        #[derive(Debug, PartialEq, Deserialize)]
        enum Enum {
            Unit,
            NewType(i32),
            Tuple(String, bool),
            Struct { x: i32, y: i32 },
        }

        // This is the format used in serde >= 0.10
        //
        // Serialization of Enum::NewType(10)
        let v: Vec<u8> = vec![
            0xa1, // map 1pair
            0x67, 0x4e, 0x65, 0x77, 0x54, 0x79, 0x70, 0x65, // utf8 string: NewType
            0x1a, // u32
            0x00, 0x00, 0x00, 0x0a, // 10 (dec)
        ];
        let (_rest, value): (&[u8], Enum) = from_slice_stream(&v[..]).unwrap();
        assert_eq!(value, Enum::NewType(10));
        let (_rest, value): (&[u8], Enum) =
            from_slice_stream_options(&v[..], Options::default().no_legacy()).unwrap();
        assert_eq!(value, Enum::NewType(10));
        let value: error::Result<(&[u8], Enum)> =
            from_slice_stream_options(&v[..], Options::default().no_standard());
        assert_eq!(
            value.unwrap_err().classify(),
            serde_cbor::error::Category::Syntax
        );
        let value: error::Result<(&[u8], Enum)> =
            from_slice_stream_options(&v[..], Options::default().no_standard().no_legacy());
        assert_eq!(
            value.unwrap_err().classify(),
            serde_cbor::error::Category::Syntax
        );
        // Serialization of Enum::Unit
        let v: Vec<u8> = vec![
            0x64, 0x55, 0x6e, 0x69, 0x74, // utf8 string: Unit
        ];
        let (_rest, value): (&[u8], Enum) = from_slice_stream(&v[..]).unwrap();
        assert_eq!(value, Enum::Unit);
        let (_rest, value): (&[u8], Enum) =
            from_slice_stream_options(&v[..], Options::default().no_legacy()).unwrap();
        assert_eq!(value, Enum::Unit);
        let (_rest, value): (&[u8], Enum) =
            from_slice_stream_options(&v[..], Options::default().no_standard()).unwrap();
        assert_eq!(value, Enum::Unit);
        let value: error::Result<(&[u8], Enum)> =
            from_slice_stream_options(&v[..], Options::default().no_legacy().no_standard());
        assert_eq!(
            value.unwrap_err().classify(),
            serde_cbor::error::Category::Syntax
        );

        // This is the format used in serde <= 0.9
        let v: Vec<u8> = vec![
            0x82, // array 2 items
            0x67, 0x4e, 0x65, 0x77, 0x54, 0x79, 0x70, 0x65, // utf8 string: NewType
            0x1a, // u32
            0x00, 0x00, 0x00, 0x0a, // 10 (dec)
        ];
        let (_rest, value): (&[u8], Enum) = from_slice_stream(&v[..]).unwrap();
        assert_eq!(value, Enum::NewType(10));
        let value: error::Result<(&[u8], Enum)> =
            from_slice_stream_options(&v[..], Options::default().no_legacy());
        assert_eq!(
            value.unwrap_err().classify(),
            serde_cbor::error::Category::Syntax
        );
        let value: error::Result<(&[u8], Enum)> =
            from_slice_stream_options(&v[..], Options::default().no_standard());
        assert_eq!(value.unwrap().1, Enum::NewType(10));
        let value: error::Result<(&[u8], Enum)> =
            from_slice_stream_options(&v[..], Options::default().no_standard().no_legacy());
        assert_eq!(
            value.unwrap_err().classify(),
            serde_cbor::error::Category::Syntax
        );
    }

    #[test]
    fn test_packed_deserialization() {
        #[derive(Debug, PartialEq, Deserialize)]
        struct User {
            user_id: u32,
            password_hash: [u8; 4],
        }

        // unpacked
        let v: Vec<u8> = vec![
            0xa2, // map 2pair
            0x67, 0x75, 0x73, 0x65, 0x72, 0x5f, 0x69, 0x64, // utf8 string: user_id
            0x0a, // integer: 10
            // utf8 string: password_hash
            0x6d, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x5f, 0x68, 0x61, 0x73, 0x68,
            0x84, 0x01, 0x02, 0x03, 0x04, // 4 byte array [1, 2, 3, 4]
        ];

        let (_rest, value): (&[u8], User) = from_slice_stream(&v[..]).unwrap();
        assert_eq!(
            value,
            User {
                user_id: 10,
                password_hash: [1, 2, 3, 4],
            }
        );
        let (_rest, value): (&[u8], User) =
            from_slice_stream_options(&v[..], Options::default().no_packed()).unwrap();
        assert_eq!(
            value,
            User {
                user_id: 10,
                password_hash: [1, 2, 3, 4],
            }
        );
        let value: error::Result<(&[u8], User)> =
            from_slice_stream_options(&v[..], Options::default().no_named());
        assert_eq!(
            value.unwrap_err().classify(),
            serde_cbor::error::Category::Syntax
        );

        // unpacked - indefinite length
        let v: Vec<u8> = vec![
            0xbf, // map to be followed by a break
            0x67, 0x75, 0x73, 0x65, 0x72, 0x5f, 0x69, 0x64, // utf8 string: user_id
            0x0a, // integer: 10
            // utf8 string: password_hash
            0x6d, 0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x5f, 0x68, 0x61, 0x73, 0x68,
            0x84, 0x01, 0x02, 0x03, 0x04, // 4 byte array [1, 2, 3, 4]
            0xff, // break
        ];

        let (_rest, value): (&[u8], User) = from_slice_stream(&v[..]).unwrap();
        assert_eq!(
            value,
            User {
                user_id: 10,
                password_hash: [1, 2, 3, 4],
            }
        );
        let (_rest, value): (&[u8], User) =
            from_slice_stream_options(&v[..], Options::default().no_packed()).unwrap();
        assert_eq!(
            value,
            User {
                user_id: 10,
                password_hash: [1, 2, 3, 4],
            }
        );
        let value: error::Result<(&[u8], User)> =
            from_slice_stream_options(&v[..], Options::default().no_named());
        assert_eq!(
            value.unwrap_err().classify(),
            serde_cbor::error::Category::Syntax
        );

        // packed
        let v: Vec<u8> = vec![
            0xa2, // map 2pair
            0x00, // index 0
            0x0a, // integer: 10
            0x01, // index 1
            0x84, 0x01, 0x02, 0x03, 0x04, // 4 byte array [1, 2, 3, 4]
        ];

        let (_rest, value): (&[u8], User) = from_slice_stream(&v[..]).unwrap();
        assert_eq!(
            value,
            User {
                user_id: 10,
                password_hash: [1, 2, 3, 4],
            }
        );
        let (_rest, value): (&[u8], User) =
            from_slice_stream_options(&v[..], Options::default().no_named()).unwrap();
        assert_eq!(
            value,
            User {
                user_id: 10,
                password_hash: [1, 2, 3, 4],
            }
        );
        let value: error::Result<(&[u8], User)> =
            from_slice_stream_options(&v[..], Options::default().no_packed());
        assert_eq!(
            value.unwrap_err().classify(),
            serde_cbor::error::Category::Syntax
        );

        // packed - indefinite length
        let v: Vec<u8> = vec![
            0xbf, // map, to be followed by a break
            0x00, // index 0
            0x0a, // integer: 10
            0x01, // index 1
            0x84, 0x01, 0x02, 0x03, 0x04, // 4 byte array [1, 2, 3, 4]
            0xff, // break
        ];

        let (_rest, value): (&[u8], User) = from_slice_stream(&v[..]).unwrap();
        assert_eq!(
            value,
            User {
                user_id: 10,
                password_hash: [1, 2, 3, 4],
            }
        );
        let (_rest, value): (&[u8], User) =
            from_slice_stream_options(&v[..], Options::default().no_named()).unwrap();
        assert_eq!(
            value,
            User {
                user_id: 10,
                password_hash: [1, 2, 3, 4],
            }
        );
        let value: error::Result<(&[u8], User)> =
            from_slice_stream_options(&v[..], Options::default().no_packed());
        assert_eq!(
            value.unwrap_err().classify(),
            serde_cbor::error::Category::Syntax
        );
    }

    use serde_cbor::{de::from_slice, ser::to_vec_packed};
    use std::net::{IpAddr, Ipv4Addr};
    #[test]
    fn test_ipaddr_deserialization() {
        let ip = IpAddr::V4(Ipv4Addr::new(127, 0, 0, 1));
        let buf = to_vec_packed(&ip).unwrap();
        let deserialized_ip = from_slice::<IpAddr>(&buf).unwrap();
        assert_eq!(ip, deserialized_ip);

        let buf = to_vec(&ip).unwrap();
        let deserialized_ip = from_slice::<IpAddr>(&buf).unwrap();
        assert_eq!(ip, deserialized_ip);
    }

    #[test]
    fn attempt_stack_overflow() {
        // Create a tag 17, followed by 999 more tag 17:
        // 17(17(17(17(17(17(17(17(17(17(17(17(17(17(17(17(17(17(...
        // This causes deep recursion in the decoder and may
        // exhaust the stack and therfore result in a stack overflow.
        let input = vec![0xd1; 1000];
        let err = serde_cbor::from_slice::<serde_cbor::Value>(&input).expect_err("recursion limit");
        assert!(err.is_syntax());
    }
}
