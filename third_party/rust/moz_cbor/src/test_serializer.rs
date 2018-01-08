use CborType;
use std::collections::BTreeMap;

#[test]
fn test_nint() {
    struct Testcase {
        value: i64,
        expected: Vec<u8>,
    }
    let testcases: Vec<Testcase> = vec![Testcase {
                                            value: -1,
                                            expected: vec![0x20],
                                        },
                                        Testcase {
                                            value: -10,
                                            expected: vec![0x29],
                                        },
                                        Testcase {
                                            value: -100,
                                            expected: vec![0x38, 0x63],
                                        },
                                        Testcase {
                                            value: -1000,
                                            expected: vec![0x39, 0x03, 0xe7],
                                        },
                                        Testcase {
                                            value: -1000000,
                                            expected: vec![0x3a, 0x00, 0x0f, 0x42, 0x3f],
                                        },
                                        Testcase {
                                            value: -4611686018427387903,
                                            expected: vec![0x3b, 0x3f, 0xff, 0xff, 0xff, 0xff,
                                                           0xff, 0xff, 0xfe],
                                        }];
    for testcase in testcases {
        let cbor = CborType::SignedInteger(testcase.value);
        assert_eq!(testcase.expected, cbor.serialize());
    }
}

#[test]
fn test_bstr() {
    struct Testcase {
        value: Vec<u8>,
        expected: Vec<u8>,
    }
    let testcases: Vec<Testcase> =
        vec![Testcase {
                 value: vec![],
                 expected: vec![0x40],
             },
             Testcase {
                 value: vec![0x01, 0x02, 0x03, 0x04],
                 expected: vec![0x44, 0x01, 0x02, 0x03, 0x04],
             },
             Testcase {
                 value: vec![0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf,
                             0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf,
                             0xaf, 0xaf, 0xaf],
                 expected: vec![0x58, 0x19, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf,
                                0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf, 0xaf,
                                0xaf, 0xaf, 0xaf, 0xaf, 0xaf],
             }];
    for testcase in testcases {
        let cbor = CborType::Bytes(testcase.value);
        assert_eq!(testcase.expected, cbor.serialize());
    }
}

#[test]
fn test_tstr() {
    struct Testcase {
        value: String,
        expected: Vec<u8>,
    }
    let testcases: Vec<Testcase> = vec![Testcase {
                                            value: String::new(),
                                            expected: vec![0x60],
                                        },
                                        Testcase {
                                            value: String::from("a"),
                                            expected: vec![0x61, 0x61],
                                        },
                                        Testcase {
                                            value: String::from("IETF"),
                                            expected: vec![0x64, 0x49, 0x45, 0x54, 0x46],
                                        },
                                        Testcase {
                                            value: String::from("\"\\"),
                                            expected: vec![0x62, 0x22, 0x5c],
                                        },
                                        Testcase {
                                            value: String::from("水"),
                                            expected: vec![0x63, 0xe6, 0xb0, 0xb4],
                                        }];
    for testcase in testcases {
        let cbor = CborType::String(testcase.value);
        assert_eq!(testcase.expected, cbor.serialize());
    }
}

#[test]
fn test_arr() {
    struct Testcase {
        value: Vec<CborType>,
        expected: Vec<u8>,
    }
    let nested_arr_1 = vec![CborType::Integer(2),
                            CborType::Integer(3)];
    let nested_arr_2 = vec![CborType::Integer(4),
                            CborType::Integer(5)];
    let testcases: Vec<Testcase> =
        vec![Testcase {
                 value: vec![],
                 expected: vec![0x80],
             },
             Testcase {
                 value: vec![CborType::Integer(1),
                             CborType::Integer(2),
                             CborType::Integer(3)],
                 expected: vec![0x83, 0x01, 0x02, 0x03],
             },
             Testcase {
                 value: vec![CborType::Integer(1),
                             CborType::Array(nested_arr_1),
                             CborType::Array(nested_arr_2)],
                 expected: vec![0x83, 0x01, 0x82, 0x02, 0x03, 0x82, 0x04, 0x05],
             },
             Testcase {
                 value: vec![CborType::Integer(1),
                             CborType::Integer(2),
                             CborType::Integer(3),
                             CborType::Integer(4),
                             CborType::Integer(5),
                             CborType::Integer(6),
                             CborType::Integer(7),
                             CborType::Integer(8),
                             CborType::Integer(9),
                             CborType::Integer(10),
                             CborType::Integer(11),
                             CborType::Integer(12),
                             CborType::Integer(13),
                             CborType::Integer(14),
                             CborType::Integer(15),
                             CborType::Integer(16),
                             CborType::Integer(17),
                             CborType::Integer(18),
                             CborType::Integer(19),
                             CborType::Integer(20),
                             CborType::Integer(21),
                             CborType::Integer(22),
                             CborType::Integer(23),
                             CborType::Integer(24),
                             CborType::Integer(25)],
                 expected: vec![0x98, 0x19, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
                                0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
                                0x15, 0x16, 0x17, 0x18, 0x18, 0x18, 0x19],
             }];
    for testcase in testcases {
        let cbor = CborType::Array(testcase.value);
        assert_eq!(testcase.expected, cbor.serialize());
    }
}

#[test]
fn test_map() {
    let empty_map: BTreeMap<CborType, CborType> = BTreeMap::new();
    assert_eq!(vec![0xa0], CborType::Map(empty_map).serialize());

    let mut positive_map: BTreeMap<CborType, CborType> = BTreeMap::new();
    positive_map.insert(CborType::Integer(20), CborType::Integer(10));
    positive_map.insert(CborType::Integer(10), CborType::Integer(20));
    positive_map.insert(CborType::Integer(15), CborType::Integer(15));
    assert_eq!(
        vec![0xa3, 0x0a, 0x14, 0x0f, 0x0f, 0x14, 0x0a],
        CborType::Map(positive_map).serialize()
    );

    let mut negative_map: BTreeMap<CborType, CborType> = BTreeMap::new();
    negative_map.insert(CborType::SignedInteger(-4), CborType::Integer(10));
    negative_map.insert(CborType::SignedInteger(-1), CborType::Integer(20));
    negative_map.insert(CborType::SignedInteger(-5), CborType::Integer(15));
    negative_map.insert(CborType::SignedInteger(-6), CborType::Integer(10));
    assert_eq!(
        vec![0xa4, 0x20, 0x14, 0x23, 0x0a, 0x24, 0x0f, 0x25, 0x0a],
        CborType::Map(negative_map).serialize()
    );

    let mut mixed_map: BTreeMap<CborType, CborType> = BTreeMap::new();
    mixed_map.insert(CborType::Integer(0), CborType::Integer(10));
    mixed_map.insert(CborType::SignedInteger(-10), CborType::Integer(20));
    mixed_map.insert(CborType::Integer(15), CborType::Integer(15));
    assert_eq!(
        vec![0xa3, 0x00, 0x0a, 0x0f, 0x0f, 0x29, 0x14],
        CborType::Map(mixed_map).serialize()
    );

    let mut very_mixed_map: BTreeMap<CborType, CborType> = BTreeMap::new();
    very_mixed_map.insert(CborType::Integer(0), CborType::Integer(10));
    very_mixed_map.insert(
        CborType::SignedInteger(-10000),
        CborType::String("low".to_string()),
    );
    very_mixed_map.insert(CborType::SignedInteger(-10), CborType::Integer(20));
    very_mixed_map.insert(
        CborType::Integer(10001),
        CborType::String("high".to_string()),
    );
    very_mixed_map.insert(
        CborType::Integer(10000),
        CborType::String("high".to_string()),
    );
    very_mixed_map.insert(CborType::Integer(15), CborType::Integer(15));
    let expected = vec![0xa6, 0x00, 0x0a, 0x0f, 0x0f, 0x29, 0x14, 0x19, 0x27, 0x10, 0x64, 0x68,
                        0x69, 0x67, 0x68, 0x19, 0x27, 0x11, 0x64, 0x68, 0x69, 0x67, 0x68, 0x39,
                        0x27, 0x0F, 0x63, 0x6C, 0x6F, 0x77];
    assert_eq!(expected, CborType::Map(very_mixed_map).serialize());
}

#[test]
#[ignore]
// XXX: The string isn't put into the map at the moment, so we can't actually
//      test this.
fn test_invalid_map() {
    let mut invalid_map: BTreeMap<CborType, CborType> = BTreeMap::new();
    invalid_map.insert(CborType::SignedInteger(-10), CborType::Integer(20));
    invalid_map.insert(CborType::String("0".to_string()), CborType::Integer(10));
    invalid_map.insert(CborType::Integer(15), CborType::Integer(15));
    let expected: Vec<u8> = vec![];
    assert_eq!(expected, CborType::Map(invalid_map).serialize());
}

#[test]
fn test_integer() {
    struct Testcase {
        value: u64,
        expected: Vec<u8>,
    }
    let testcases: Vec<Testcase> =
        vec![Testcase {
                 value: 0,
                 expected: vec![0],
             },
             Testcase {
                 value: 1,
                 expected: vec![1],
             },
             Testcase {
                 value: 10,
                 expected: vec![0x0a],
             },
             Testcase {
                 value: 23,
                 expected: vec![0x17],
             },
             Testcase {
                 value: 24,
                 expected: vec![0x18, 0x18],
             },
             Testcase {
                 value: 25,
                 expected: vec![0x18, 0x19],
             },
             Testcase {
                 value: 100,
                 expected: vec![0x18, 0x64],
             },
             Testcase {
                 value: 1000,
                 expected: vec![0x19, 0x03, 0xe8],
             },
             Testcase {
                 value: 1000000,
                 expected: vec![0x1a, 0x00, 0x0f, 0x42, 0x40],
             },
             Testcase {
                 value: 1000000000000,
                 expected: vec![0x1b, 0x00, 0x00, 0x00, 0xe8, 0xd4, 0xa5, 0x10, 0x00],
             },
             Testcase {
                 value: 18446744073709551615,
                 expected: vec![0x1b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff],
             }];
    for testcase in testcases {
        let cbor = CborType::Integer(testcase.value);
        assert_eq!(testcase.expected, cbor.serialize());
    }
}

#[test]
fn test_tagged_item() {
    let cbor = CborType::Tag(0x12, Box::new(CborType::Integer(2).clone()));
    assert_eq!(vec![0xD2, 0x02], cbor.serialize());

    let cbor = CborType::Tag(0x62, Box::new(CborType::Array(vec![]).clone()));
    assert_eq!(vec![0xD8, 0x62, 0x80], cbor.serialize());
}

#[test]
fn test_null() {
    let cbor = CborType::Null;
    assert_eq!(vec![0xf6], cbor.serialize());
}

#[test]
fn test_null_in_array() {
    let cbor = CborType::Array(vec![CborType::Null,
         CborType::Null]);
    assert_eq!(vec![0x82, 0xf6, 0xf6], cbor.serialize());
}
