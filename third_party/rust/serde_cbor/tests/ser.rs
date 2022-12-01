use serde::Serialize;
use serde_cbor::ser::{Serializer, SliceWrite};

#[test]
fn test_str() {
    serialize_and_compare("foobar", b"ffoobar");
}

#[test]
fn test_list() {
    serialize_and_compare(&[1, 2, 3], b"\x83\x01\x02\x03");
}

#[test]
fn test_float() {
    serialize_and_compare(12.3f64, b"\xfb@(\x99\x99\x99\x99\x99\x9a");
}

#[test]
fn test_integer() {
    // u8
    serialize_and_compare(24, b"\x18\x18");
    // i8
    serialize_and_compare(-5, b"\x24");
    // i16
    serialize_and_compare(-300, b"\x39\x01\x2b");
    // i32
    serialize_and_compare(-23567997, b"\x3a\x01\x67\x9e\x7c");
    // u64
    serialize_and_compare(::core::u64::MAX, b"\x1b\xff\xff\xff\xff\xff\xff\xff\xff");
}

fn serialize_and_compare<T: Serialize>(value: T, expected: &[u8]) {
    let mut slice = [0u8; 64];
    let writer = SliceWrite::new(&mut slice);
    let mut serializer = Serializer::new(writer);
    value.serialize(&mut serializer).unwrap();
    let writer = serializer.into_inner();
    let end = writer.bytes_written();
    let slice = writer.into_inner();
    assert_eq!(&slice[..end], expected);
}

#[cfg(feature = "std")]
mod std_tests {
    use serde::Serializer;
    use serde_cbor::ser;
    use serde_cbor::{from_slice, to_vec};
    use std::collections::BTreeMap;

    #[test]
    fn test_string() {
        let value = "foobar".to_owned();
        assert_eq!(&to_vec(&value).unwrap()[..], b"ffoobar");
    }

    #[test]
    fn test_list() {
        let value = vec![1, 2, 3];
        assert_eq!(&to_vec(&value).unwrap()[..], b"\x83\x01\x02\x03");
    }

    #[test]
    fn test_object() {
        let mut object = BTreeMap::new();
        object.insert("a".to_owned(), "A".to_owned());
        object.insert("b".to_owned(), "B".to_owned());
        object.insert("c".to_owned(), "C".to_owned());
        object.insert("d".to_owned(), "D".to_owned());
        object.insert("e".to_owned(), "E".to_owned());
        let vec = to_vec(&object).unwrap();
        let test_object = from_slice(&vec[..]).unwrap();
        assert_eq!(object, test_object);
    }

    #[test]
    fn test_object_list_keys() {
        let mut object = BTreeMap::new();
        object.insert(vec![0i64], ());
        object.insert(vec![100i64], ());
        object.insert(vec![-1i64], ());
        object.insert(vec![-2i64], ());
        object.insert(vec![0i64, 0i64], ());
        object.insert(vec![0i64, -1i64], ());
        let vec = to_vec(&serde_cbor::value::to_value(object.clone()).unwrap()).unwrap();
        assert_eq!(
            vec![
                166, 129, 0, 246, 129, 24, 100, 246, 129, 32, 246, 129, 33, 246, 130, 0, 0, 246,
                130, 0, 32, 246
            ],
            vec
        );
        let test_object = from_slice(&vec[..]).unwrap();
        assert_eq!(object, test_object);
    }

    #[test]
    fn test_object_object_keys() {
        use std::iter::FromIterator;
        let mut object = BTreeMap::new();
        let keys = vec![
            vec!["a"],
            vec!["b"],
            vec!["c"],
            vec!["d"],
            vec!["aa"],
            vec!["a", "aa"],
        ]
        .into_iter()
        .map(|v| BTreeMap::from_iter(v.into_iter().map(|s| (s.to_owned(), ()))));

        for key in keys {
            object.insert(key, ());
        }
        let vec = to_vec(&serde_cbor::value::to_value(object.clone()).unwrap()).unwrap();
        assert_eq!(
            vec![
                166, 161, 97, 97, 246, 246, 161, 97, 98, 246, 246, 161, 97, 99, 246, 246, 161, 97,
                100, 246, 246, 161, 98, 97, 97, 246, 246, 162, 97, 97, 246, 98, 97, 97, 246, 246
            ],
            vec
        );
        let test_object = from_slice(&vec[..]).unwrap();
        assert_eq!(object, test_object);
    }

    #[test]
    fn test_float() {
        let vec = to_vec(&12.3f64).unwrap();
        assert_eq!(vec, b"\xfb@(\x99\x99\x99\x99\x99\x9a");
    }

    #[test]
    fn test_f32() {
        let vec = to_vec(&4000.5f32).unwrap();
        assert_eq!(vec, b"\xfa\x45\x7a\x08\x00");
    }

    #[test]
    fn test_infinity() {
        let vec = to_vec(&::std::f64::INFINITY).unwrap();
        assert_eq!(vec, b"\xf9|\x00");
    }

    #[test]
    fn test_neg_infinity() {
        let vec = to_vec(&::std::f64::NEG_INFINITY).unwrap();
        assert_eq!(vec, b"\xf9\xfc\x00");
    }

    #[test]
    fn test_nan() {
        let vec = to_vec(&::std::f32::NAN).unwrap();
        assert_eq!(vec, b"\xf9\x7e\x00");
    }

    #[test]
    fn test_integer() {
        // u8
        let vec = to_vec(&24).unwrap();
        assert_eq!(vec, b"\x18\x18");
        // i8
        let vec = to_vec(&-5).unwrap();
        assert_eq!(vec, b"\x24");
        // i16
        let vec = to_vec(&-300).unwrap();
        assert_eq!(vec, b"\x39\x01\x2b");
        // i32
        let vec = to_vec(&-23567997).unwrap();
        assert_eq!(vec, b"\x3a\x01\x67\x9e\x7c");
        // u64
        let vec = to_vec(&::std::u64::MAX).unwrap();
        assert_eq!(vec, b"\x1b\xff\xff\xff\xff\xff\xff\xff\xff");
    }

    #[test]
    fn test_self_describing() {
        let mut vec = Vec::new();
        {
            let mut serializer = ser::Serializer::new(&mut vec);
            serializer.self_describe().unwrap();
            serializer.serialize_u64(9).unwrap();
        }
        assert_eq!(vec, b"\xd9\xd9\xf7\x09");
    }

    #[test]
    fn test_ip_addr() {
        use std::net::Ipv4Addr;

        let addr = Ipv4Addr::new(8, 8, 8, 8);
        let vec = to_vec(&addr).unwrap();
        println!("{:?}", vec);
        assert_eq!(vec.len(), 5);
        let test_addr: Ipv4Addr = from_slice(&vec).unwrap();
        assert_eq!(addr, test_addr);
    }

    /// Test all of CBOR's fixed-length byte string types
    #[test]
    fn test_byte_string() {
        // Very short byte strings have 1-byte headers
        let short = vec![0, 1, 2, 255];
        let mut short_s = Vec::new();
        serde_cbor::Serializer::new(&mut short_s)
            .serialize_bytes(&short)
            .unwrap();
        assert_eq!(&short_s[..], [0x44, 0, 1, 2, 255]);

        // byte strings > 23 bytes have 2-byte headers
        let medium = vec![
            0u8, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 255,
        ];
        let mut medium_s = Vec::new();
        serde_cbor::Serializer::new(&mut medium_s)
            .serialize_bytes(&medium)
            .unwrap();
        assert_eq!(
            &medium_s[..],
            [
                0x58, 24, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                21, 22, 255
            ]
        );

        // byte strings > 256 bytes have 3-byte headers
        let long_vec = (0..256).map(|i| (i & 0xFF) as u8).collect::<Vec<_>>();
        let mut long_s = Vec::new();
        serde_cbor::Serializer::new(&mut long_s)
            .serialize_bytes(&long_vec)
            .unwrap();
        assert_eq!(&long_s[0..3], [0x59, 1, 0]);
        assert_eq!(&long_s[3..], &long_vec[..]);

        // byte strings > 2^16 bytes have 5-byte headers
        let very_long_vec = (0..65536).map(|i| (i & 0xFF) as u8).collect::<Vec<_>>();
        let mut very_long_s = Vec::new();
        serde_cbor::Serializer::new(&mut very_long_s)
            .serialize_bytes(&very_long_vec)
            .unwrap();
        assert_eq!(&very_long_s[0..5], [0x5a, 0, 1, 0, 0]);
        assert_eq!(&very_long_s[5..], &very_long_vec[..]);

        // byte strings > 2^32 bytes have 9-byte headers, but they take too much RAM
        // to test in Travis.
    }

    #[test]
    fn test_half() {
        let vec = to_vec(&42.5f32).unwrap();
        assert_eq!(vec, b"\xF9\x51\x50");
        assert_eq!(from_slice::<f32>(&vec[..]).unwrap(), 42.5f32);
    }
}
