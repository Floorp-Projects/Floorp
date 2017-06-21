#[macro_use]
extern crate serde_derive;

extern crate bincode;
extern crate serde;
extern crate serde_bytes;
extern crate byteorder;

use std::fmt::Debug;
use std::collections::HashMap;
use std::borrow::Cow;

use bincode::{Infinite, Bounded};
use bincode::{serialized_size, ErrorKind, Result};
use bincode::internal::{serialize, deserialize, deserialize_from};

use bincode::serialize as serialize_little;
use bincode::deserialize as deserialize_little;
use bincode::deserialize_from as deserialize_from_little;

fn the_same<V>(element: V)
    where V: serde::Serialize+serde::de::DeserializeOwned+PartialEq+Debug+'static
{
    let size = serialized_size(&element);

    {
        let encoded = serialize_little(&element, Infinite).unwrap();
        let decoded = deserialize_little(&encoded[..]).unwrap();

        assert_eq!(element, decoded);
        assert_eq!(size, encoded.len() as u64);
    }

    {
        let encoded = serialize::<_, _, byteorder::BigEndian>(&element, Infinite).unwrap();
        let decoded = deserialize::<_, byteorder::BigEndian>(&encoded[..]).unwrap();
        let decoded_reader = deserialize_from::<_, _, _, byteorder::BigEndian>(&mut &encoded[..], Infinite).unwrap();

        assert_eq!(element, decoded);
        assert_eq!(element, decoded_reader);
        assert_eq!(size, encoded.len() as u64);
    }
}

#[test]
fn test_numbers() {
    // unsigned positive
    the_same(5u8);
    the_same(5u16);
    the_same(5u32);
    the_same(5u64);
    the_same(5usize);
    // signed positive
    the_same(5i8);
    the_same(5i16);
    the_same(5i32);
    the_same(5i64);
    the_same(5isize);
    // signed negative
    the_same(-5i8);
    the_same(-5i16);
    the_same(-5i32);
    the_same(-5i64);
    the_same(-5isize);
    // floating
    the_same(-100f32);
    the_same(0f32);
    the_same(5f32);
    the_same(-100f64);
    the_same(5f64);
}

#[test]
fn test_string() {
    the_same("".to_string());
    the_same("a".to_string());
}

#[test]
fn test_tuple() {
    the_same((1isize,));
    the_same((1isize,2isize,3isize));
    the_same((1isize,"foo".to_string(),()));
}

#[test]
fn test_basic_struct() {
    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    struct Easy {
        x: isize,
        s: String,
        y: usize
    }
    the_same(Easy{x: -4, s: "foo".to_string(), y: 10});
}

#[test]
fn test_nested_struct() {
    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    struct Easy {
        x: isize,
        s: String,
        y: usize
    }
    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    struct Nest {
        f: Easy,
        b: usize,
        s: Easy
    }

    the_same(Nest {
        f: Easy {x: -1, s: "foo".to_string(), y: 20},
        b: 100,
        s: Easy {x: -100, s: "bar".to_string(), y: 20}
    });
}

#[test]
fn test_struct_newtype() {
    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    struct NewtypeStr(usize);

    the_same(NewtypeStr(5));
}

#[test]
fn test_struct_tuple() {
    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    struct TubStr(usize, String, f32);

    the_same(TubStr(5, "hello".to_string(), 3.2));
}

#[test]
fn test_option() {
    the_same(Some(5usize));
    the_same(Some("foo bar".to_string()));
    the_same(None::<usize>);
}

#[test]
fn test_enum() {
    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    enum TestEnum {
        NoArg,
        OneArg(usize),
        Args(usize, usize),
        AnotherNoArg,
        StructLike{x: usize, y: f32}
    }
    the_same(TestEnum::NoArg);
    the_same(TestEnum::OneArg(4));
    //the_same(TestEnum::Args(4, 5));
    the_same(TestEnum::AnotherNoArg);
    the_same(TestEnum::StructLike{x: 4, y: 3.14159});
    the_same(vec![TestEnum::NoArg, TestEnum::OneArg(5), TestEnum::AnotherNoArg,
                  TestEnum::StructLike{x: 4, y:1.4}]);
}

#[test]
fn test_vec() {
    let v: Vec<u8> = vec![];
    the_same(v);
    the_same(vec![1u64]);
    the_same(vec![1u64,2,3,4,5,6]);
}

#[test]
fn test_map() {
    let mut m = HashMap::new();
    m.insert(4u64, "foo".to_string());
    m.insert(0u64, "bar".to_string());
    the_same(m);
}

#[test]
fn test_bool() {
    the_same(true);
    the_same(false);
}

#[test]
fn test_unicode() {
    the_same("å".to_string());
    the_same("aåååååååa".to_string());
}

#[test]
fn test_fixed_size_array() {
    the_same([24u32; 32]);
    the_same([1u64, 2, 3, 4, 5, 6, 7, 8]);
    the_same([0u8; 19]);
}

#[test]
fn deserializing_errors() {
    fn isize_invalid_deserialize<T: Debug>(res: Result<T>) {
        match res.map_err(|e| *e) {
            Err(ErrorKind::InvalidEncoding{..}) => {},
            Err(ErrorKind::Custom(ref s)) if s.contains("invalid encoding") => {},
            Err(ErrorKind::Custom(ref s)) if s.contains("invalid value") => {},
            other => panic!("Expecting InvalidEncoding, got {:?}", other),
        }
    }

    isize_invalid_deserialize(deserialize_little::<bool>(&vec![0xA][..]));
    isize_invalid_deserialize(deserialize_little::<String>(&vec![1, 0, 0, 0, 0, 0, 0, 0, 0xFF][..]));
    // Out-of-bounds variant
    #[derive(Serialize, Deserialize, Debug)]
    enum Test {
        One,
        Two,
    };
    isize_invalid_deserialize(deserialize_little::<Test>(&vec![0, 0, 0, 5][..]));
    isize_invalid_deserialize(deserialize_little::<Option<u8>>(&vec![5, 0][..]));
}

#[test]
fn too_big_deserialize() {
    let serialized = vec![0,0,0,3];
    let deserialized: Result<u32> = deserialize_from_little::<_, _, _>(&mut &serialized[..], Bounded(3));
    assert!(deserialized.is_err());

    let serialized = vec![0,0,0,3];
    let deserialized: Result<u32> = deserialize_from_little::<_, _, _>(&mut &serialized[..], Bounded(4));
    assert!(deserialized.is_ok());
}

#[test]
fn char_serialization() {
    let chars = "Aa\0☺♪";
    for c in chars.chars() {
        let encoded = serialize_little(&c, Bounded(4)).expect("serializing char failed");
        let decoded: char = deserialize_little(&encoded).expect("deserializing failed");
        assert_eq!(decoded, c);
    }
}

#[test]
fn too_big_char_deserialize() {
    let serialized = vec![0x41];
    let deserialized: Result<char> = deserialize_from_little::<_, _, _>(&mut &serialized[..], Bounded(1));
    assert!(deserialized.is_ok());
    assert_eq!(deserialized.unwrap(), 'A');
}

#[test]
fn too_big_serialize() {
    assert!(serialize_little(&0u32, Bounded(3)).is_err());
    assert!(serialize_little(&0u32, Bounded(4)).is_ok());

    assert!(serialize_little(&"abcde", Bounded(8 + 4)).is_err());
    assert!(serialize_little(&"abcde", Bounded(8 + 5)).is_ok());
}

#[test]
fn test_proxy_encoded_size() {
    assert!(serialized_size(&0u8) == 1);
    assert!(serialized_size(&0u16) == 2);
    assert!(serialized_size(&0u32) == 4);
    assert!(serialized_size(&0u64) == 8);

    // length isize stored as u64
    assert!(serialized_size(&"") == 8);
    assert!(serialized_size(&"a") == 8 + 1);

    assert!(serialized_size(&vec![0u32, 1u32, 2u32]) == 8 + 3 * (4))

}

#[test]
fn test_serialized_size() {
    assert!(serialized_size(&0u8) == 1);
    assert!(serialized_size(&0u16) == 2);
    assert!(serialized_size(&0u32) == 4);
    assert!(serialized_size(&0u64) == 8);

    // length isize stored as u64
    assert!(serialized_size(&"") == 8);
    assert!(serialized_size(&"a") == 8 + 1);

    assert!(serialized_size(&vec![0u32, 1u32, 2u32]) == 8 + 3 * (4))
}

#[test]
fn encode_box() {
    the_same(Box::new(5));
}

#[test]
fn test_cow_serialize() {
    let large_object = vec![1u32,2,3,4,5,6];
    let mut large_map = HashMap::new();
    large_map.insert(1, 2);


    #[derive(Serialize, Deserialize, Debug)]
    enum Message<'a> {
        M1(Cow<'a, Vec<u32>>),
        M2(Cow<'a, HashMap<u32, u32>>)
    }

    // Test 1
    {
        let serialized = serialize_little(&Message::M1(Cow::Borrowed(&large_object)), Infinite).unwrap();
        let deserialized: Message<'static> = deserialize_from_little(&mut &serialized[..], Infinite).unwrap();

        match deserialized {
            Message::M1(b) => assert!(&b.into_owned() == &large_object),
            _ => assert!(false)
        }
    }

    // Test 2
    {
        let serialized = serialize_little(&Message::M2(Cow::Borrowed(&large_map)), Infinite).unwrap();
        let deserialized: Message<'static> = deserialize_from_little(&mut &serialized[..], Infinite).unwrap();

        match deserialized {
            Message::M2(b) => assert!(&b.into_owned() == &large_map),
            _ => assert!(false)
        }
    }
}

#[test]
fn test_strbox_serialize() {
    let strx: &'static str = "hello world";
    let serialized = serialize_little(&Cow::Borrowed(strx), Infinite).unwrap();
    let deserialized: Cow<'static, String> = deserialize_from_little(&mut &serialized[..], Infinite).unwrap();
    let stringx: String = deserialized.into_owned();
    assert!(strx == &stringx[..]);
}

#[test]
fn test_slicebox_serialize() {
    let slice = [1u32, 2, 3 ,4, 5];
    let serialized = serialize_little(&Cow::Borrowed(&slice[..]), Infinite).unwrap();
    println!("{:?}", serialized);
    let deserialized: Cow<'static, Vec<u32>> = deserialize_from_little(&mut &serialized[..], Infinite).unwrap();
    {
        let sb: &[u32] = &deserialized;
        assert!(slice == sb);
    }
    let vecx: Vec<u32> = deserialized.into_owned();
    assert!(slice == &vecx[..]);
}

#[test]
fn test_multi_strings_serialize() {
    assert!(serialize_little(&("foo", "bar", "baz"), Infinite).is_ok());
}

#[test]
fn test_oom_protection() {
    use std::io::Cursor;
    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    struct FakeVec {
        len: u64,
        byte: u8
    }
    let x = serialize_little(&FakeVec { len: 0xffffffffffffffffu64, byte: 1 }, Bounded(10)).unwrap();
    let y: Result<Vec<u8>> = deserialize_from_little(&mut Cursor::new(&x[..]), Bounded(10));
    assert!(y.is_err());
}

#[test]
fn path_buf() {
    use std::path::{Path, PathBuf};
    let path = Path::new("foo").to_path_buf();
    let serde_encoded = serialize_little(&path, Infinite).unwrap();
    let decoded: PathBuf = deserialize_little(&serde_encoded).unwrap();
    assert!(path.to_str() == decoded.to_str());
}

#[test]
fn bytes() {
    use serde_bytes::Bytes;

    let data = b"abc\0123";
    let s = serialize_little(&data[..], Infinite).unwrap();
    let s2 = serialize_little(&Bytes::new(data), Infinite).unwrap();
    assert_eq!(s[..], s2[..]);
}

#[test]
fn serde_bytes() {
    use serde_bytes::ByteBuf;
    the_same(ByteBuf::from(vec![1,2,3,4,5]));
}


#[test]
fn endian_difference() {
    let x = 10u64;
    let little = serialize_little(&x, Infinite).unwrap();
    let big = serialize::<_, _, byteorder::BigEndian>(&x, Infinite).unwrap();
    assert_ne!(little, big);
}

#[test]
fn test_zero_copy_parse() {
    #[derive(Serialize, Deserialize, Eq, PartialEq, Debug)]
    struct Foo<'a> {
        borrowed_str: &'a str,
        borrowed_bytes: &'a [u8],
    }

    let f = Foo {
        borrowed_str: "hi",
        borrowed_bytes: &[0, 1, 2, 3],
    };
    {
        let encoded = serialize_little(&f, Infinite).unwrap();
        let out: Foo = deserialize_little(&encoded[..]).unwrap();
        assert_eq!(out, f);
    }
}
