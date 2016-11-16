#![feature(plugin, custom_derive, custom_attribute)]
#![plugin(serde_macros)]

extern crate bincode;
extern crate rustc_serialize;
extern crate serde;

use std::fmt::Debug;
use std::collections::HashMap;
use std::ops::Deref;

use rustc_serialize::{Encodable, Decodable};

use bincode::{RefBox, StrBox, SliceBox};

use bincode::SizeLimit::{self, Infinite, Bounded};
use bincode::rustc_serialize::{encode, decode, decode_from, DecodingError};
use bincode::serde::{serialize, deserialize, deserialize_from, DeserializeError, DeserializeResult};

fn proxy_encode<V>(element: &V, size_limit: SizeLimit) -> Vec<u8>
    where V: Encodable + Decodable + serde::Serialize + serde::Deserialize + PartialEq + Debug + 'static
{
    let v1 = bincode::rustc_serialize::encode(element, size_limit).unwrap();
    let v2 = bincode::serde::serialize(element, size_limit).unwrap();
    assert_eq!(v1, v2);

    v1
}

fn proxy_decode<V>(slice: &[u8]) -> V
    where V: Encodable + Decodable + serde::Serialize + serde::Deserialize + PartialEq + Debug + 'static
{
    let e1 = bincode::rustc_serialize::decode(slice).unwrap();
    let e2 = bincode::serde::deserialize(slice).unwrap();

    assert_eq!(e1, e2);

    e1
}

fn proxy_encoded_size<V>(element: &V) -> u64
    where V: Encodable + serde::Serialize + PartialEq + Debug + 'static
{
    let ser_size = bincode::rustc_serialize::encoded_size(element);
    let serde_size = bincode::serde::serialized_size(element);
    assert_eq!(ser_size, serde_size);
    ser_size
}

fn the_same<V>(element: V)
    where V: Encodable+Decodable+serde::Serialize+serde::Deserialize+PartialEq+Debug+'static
{
    // Make sure that the bahavior isize correct when wrapping with a RefBox.
    fn ref_box_correct<V>(v: &V) -> bool
        where V: Encodable + Decodable + PartialEq + Debug + 'static
    {
        let rf = RefBox::new(v);
        let encoded = bincode::rustc_serialize::encode(&rf, Infinite).unwrap();
        let decoded: RefBox<'static, V> = bincode::rustc_serialize::decode(&encoded[..]).unwrap();

        decoded.take().deref() == v
    }

    let size = proxy_encoded_size(&element);

    let encoded = proxy_encode(&element, Infinite);
    let decoded = proxy_decode(&encoded[..]);

    assert_eq!(element, decoded);
    assert_eq!(size, encoded.len() as u64);
    assert!(ref_box_correct(&element));
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
    #[derive(RustcEncodable, RustcDecodable, Serialize, Deserialize, PartialEq, Debug)]
    struct Easy {
        x: isize,
        s: String,
        y: usize
    }
    the_same(Easy{x: -4, s: "foo".to_string(), y: 10});
}

#[test]
fn test_nested_struct() {
    #[derive(RustcEncodable, RustcDecodable, Serialize, Deserialize, PartialEq, Debug)]
    struct Easy {
        x: isize,
        s: String,
        y: usize
    }
    #[derive(RustcEncodable, RustcDecodable, Serialize, Deserialize, PartialEq, Debug)]
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
    #[derive(RustcEncodable, RustcDecodable, Serialize, Deserialize, PartialEq, Debug)]
    struct NewtypeStr(usize);

    the_same(NewtypeStr(5));
}

#[test]
fn test_struct_tuple() {
    #[derive(RustcEncodable, RustcDecodable, Serialize, Deserialize, PartialEq, Debug)]
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
    #[derive(RustcEncodable, RustcDecodable, Serialize, Deserialize, PartialEq, Debug)]
    enum TestEnum {
        NoArg,
        OneArg(usize),
        Args(usize, usize),
        AnotherNoArg,
        StructLike{x: usize, y: f32}
    }
    the_same(TestEnum::NoArg);
    the_same(TestEnum::OneArg(4));
    the_same(TestEnum::Args(4, 5));
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
fn decoding_errors() {
    fn isize_invalid_encoding<T>(res: bincode::rustc_serialize::DecodingResult<T>) {
        match res {
            Ok(_) => panic!("Expecting error"),
            Err(DecodingError::IoError(_)) => panic!("Expecting InvalidEncoding"),
            Err(DecodingError::SizeLimit) => panic!("Expecting InvalidEncoding"),
            Err(DecodingError::InvalidEncoding(_)) => {},
        }
    }

    isize_invalid_encoding(decode::<bool>(&vec![0xA][..]));
    isize_invalid_encoding(decode::<String>(&vec![0, 0, 0, 0, 0, 0, 0, 1, 0xFF][..]));
    // Out-of-bounds variant
    #[derive(RustcEncodable, RustcDecodable, Serialize)]
    enum Test {
        One,
        Two,
    };
    isize_invalid_encoding(decode::<Test>(&vec![0, 0, 0, 5][..]));
    isize_invalid_encoding(decode::<Option<u8>>(&vec![5, 0][..]));
}

#[test]
fn deserializing_errors() {
    fn isize_invalid_deserialize<T: Debug>(res: DeserializeResult<T>) {
        match res {
            Err(DeserializeError::InvalidEncoding(_)) => {},
            Err(DeserializeError::Serde(serde::de::value::Error::UnknownVariant(_))) => {},
            Err(DeserializeError::Serde(serde::de::value::Error::InvalidValue(_))) => {},
            _ => panic!("Expecting InvalidEncoding, got {:?}", res),
        }
    }

    isize_invalid_deserialize(deserialize::<bool>(&vec![0xA][..]));
    isize_invalid_deserialize(deserialize::<String>(&vec![0, 0, 0, 0, 0, 0, 0, 1, 0xFF][..]));
    // Out-of-bounds variant
    #[derive(RustcEncodable, RustcDecodable, Serialize, Deserialize, Debug)]
    enum Test {
        One,
        Two,
    };
    isize_invalid_deserialize(deserialize::<Test>(&vec![0, 0, 0, 5][..]));
    isize_invalid_deserialize(deserialize::<Option<u8>>(&vec![5, 0][..]));
}

#[test]
fn too_big_decode() {
    let encoded = vec![0,0,0,3];
    let decoded: Result<u32, _> = decode_from(&mut &encoded[..], Bounded(3));
    assert!(decoded.is_err());

    let encoded = vec![0,0,0,3];
    let decoded: Result<u32, _> = decode_from(&mut &encoded[..], Bounded(4));
    assert!(decoded.is_ok());
}

#[test]
fn too_big_deserialize() {
    let serialized = vec![0,0,0,3];
    let deserialized: Result<u32, _> = deserialize_from(&mut &serialized[..], Bounded(3));
    assert!(deserialized.is_err());

    let serialized = vec![0,0,0,3];
    let deserialized: Result<u32, _> = deserialize_from(&mut &serialized[..], Bounded(4));
    assert!(deserialized.is_ok());
}

#[test]
fn char_serialization() {
    let chars = "Aa\0☺♪";
    for c in chars.chars() {
        let encoded = serialize(&c, Bounded(4)).expect("serializing char failed");
        let decoded: char = deserialize(&encoded).expect("deserializing failed");
        assert_eq!(decoded, c);
    }
}

#[test]
fn too_big_char_decode() {
    let encoded = vec![0x41];
    let decoded: Result<char, _> = decode_from(&mut &encoded[..], Bounded(1));
    assert!(decoded.is_ok());
    assert_eq!(decoded.unwrap(), 'A');
}

#[test]
fn too_big_char_deserialize() {
    let serialized = vec![0x41];
    let deserialized: Result<char, _> = deserialize_from(&mut &serialized[..], Bounded(1));
    assert!(deserialized.is_ok());
    assert_eq!(deserialized.unwrap(), 'A');
}

#[test]
fn too_big_encode() {
    assert!(encode(&0u32, Bounded(3)).is_err());
    assert!(encode(&0u32, Bounded(4)).is_ok());

    assert!(encode(&"abcde", Bounded(8 + 4)).is_err());
    assert!(encode(&"abcde", Bounded(8 + 5)).is_ok());
}

#[test]
fn too_big_serialize() {
    assert!(serialize(&0u32, Bounded(3)).is_err());
    assert!(serialize(&0u32, Bounded(4)).is_ok());

    assert!(serialize(&"abcde", Bounded(8 + 4)).is_err());
    assert!(serialize(&"abcde", Bounded(8 + 5)).is_ok());
}

#[test]
fn test_proxy_encoded_size() {
    assert!(proxy_encoded_size(&0u8) == 1);
    assert!(proxy_encoded_size(&0u16) == 2);
    assert!(proxy_encoded_size(&0u32) == 4);
    assert!(proxy_encoded_size(&0u64) == 8);

    // length isize stored as u64
    assert!(proxy_encoded_size(&"") == 8);
    assert!(proxy_encoded_size(&"a") == 8 + 1);

    assert!(proxy_encoded_size(&vec![0u32, 1u32, 2u32]) == 8 + 3 * (4))

}

#[test]
fn test_serialized_size() {
    assert!(proxy_encoded_size(&0u8) == 1);
    assert!(proxy_encoded_size(&0u16) == 2);
    assert!(proxy_encoded_size(&0u32) == 4);
    assert!(proxy_encoded_size(&0u64) == 8);

    // length isize stored as u64
    assert!(proxy_encoded_size(&"") == 8);
    assert!(proxy_encoded_size(&"a") == 8 + 1);

    assert!(proxy_encoded_size(&vec![0u32, 1u32, 2u32]) == 8 + 3 * (4))
}

#[test]
fn encode_box() {
    the_same(Box::new(5));
}

#[test]
fn test_refbox_encode() {
    let large_object = vec![1u32,2,3,4,5,6];
    let mut large_map = HashMap::new();
    large_map.insert(1, 2);


    #[derive(RustcEncodable, RustcDecodable, Debug)]
    enum Message<'a> {
        M1(RefBox<'a, Vec<u32>>),
        M2(RefBox<'a, HashMap<u32, u32>>)
    }

    // Test 1
    {
        let encoded = encode(&Message::M1(RefBox::new(&large_object)), Infinite).unwrap();
        let decoded: Message<'static> = decode(&encoded[..]).unwrap();

        match decoded {
            Message::M1(b) => assert!(b.take().deref() == &large_object),
            _ => assert!(false)
        }
    }

    // Test 2
    {
        let encoded = encode(&Message::M2(RefBox::new(&large_map)), Infinite).unwrap();
        let decoded: Message<'static> = decode(&encoded[..]).unwrap();

        match decoded {
            Message::M2(b) => assert!(b.take().deref() == &large_map),
            _ => assert!(false)
        }
    }
}

#[test]
fn test_refbox_serialize() {
    let large_object = vec![1u32,2,3,4,5,6];
    let mut large_map = HashMap::new();
    large_map.insert(1, 2);


    #[derive(RustcEncodable, RustcDecodable, Serialize, Deserialize, Debug)]
    enum Message<'a> {
        M1(RefBox<'a, Vec<u32>>),
        M2(RefBox<'a, HashMap<u32, u32>>)
    }

    // Test 1
    {
        let serialized = serialize(&Message::M1(RefBox::new(&large_object)), Infinite).unwrap();
        let deserialized: Message<'static> = deserialize_from(&mut &serialized[..], Infinite).unwrap();

        match deserialized {
            Message::M1(b) => assert!(b.take().deref() == &large_object),
            _ => assert!(false)
        }
    }

    // Test 2
    {
        let serialized = serialize(&Message::M2(RefBox::new(&large_map)), Infinite).unwrap();
        let deserialized: Message<'static> = deserialize_from(&mut &serialized[..], Infinite).unwrap();

        match deserialized {
            Message::M2(b) => assert!(b.take().deref() == &large_map),
            _ => assert!(false)
        }
    }
}

#[test]
fn test_strbox_encode() {
    let strx: &'static str = "hello world";
    let encoded = encode(&StrBox::new(strx), Infinite).unwrap();
    let decoded: StrBox<'static> = decode(&encoded[..]).unwrap();
    let stringx: String = decoded.take();
    assert!(strx == &stringx[..]);
}

#[test]
fn test_strbox_serialize() {
    let strx: &'static str = "hello world";
    let serialized = serialize(&StrBox::new(strx), Infinite).unwrap();
    let deserialized: StrBox<'static> = deserialize_from(&mut &serialized[..], Infinite).unwrap();
    let stringx: String = deserialized.take();
    assert!(strx == &stringx[..]);
}

#[test]
fn test_slicebox_encode() {
    let slice = [1u32, 2, 3 ,4, 5];
    let encoded = encode(&SliceBox::new(&slice), Infinite).unwrap();
    let decoded: SliceBox<'static, u32> = decode(&encoded[..]).unwrap();
    {
        let sb: &[u32] = &decoded;
        assert!(slice == sb);
    }
    let vecx: Vec<u32> = decoded.take();
    assert!(slice == &vecx[..]);
}

#[test]
fn test_slicebox_serialize() {
    let slice = [1u32, 2, 3 ,4, 5];
    let serialized = serialize(&SliceBox::new(&slice), Infinite).unwrap();
    let deserialized: SliceBox<'static, u32> = deserialize_from(&mut &serialized[..], Infinite).unwrap();
    {
        let sb: &[u32] = &deserialized;
        assert!(slice == sb);
    }
    let vecx: Vec<u32> = deserialized.take();
    assert!(slice == &vecx[..]);
}

#[test]
fn test_multi_strings_encode() {
    assert!(encode(&("foo", "bar", "baz"), Infinite).is_ok());
}

#[test]
fn test_multi_strings_serialize() {
    assert!(serialize(&("foo", "bar", "baz"), Infinite).is_ok());
}

#[test]
fn test_oom_protection() {
    use std::io::Cursor;
    #[derive(RustcEncodable, RustcDecodable)]
    struct FakeVec {
        len: u64,
        byte: u8
    }
    let x = bincode::rustc_serialize::encode(&FakeVec { len: 0xffffffffffffffffu64, byte: 1 }, bincode::SizeLimit::Bounded(10)).unwrap();
    let y : Result<Vec<u8>, _> = bincode::rustc_serialize::decode_from(&mut Cursor::new(&x[..]), bincode::SizeLimit::Bounded(10));
    assert!(y.is_err());
}

#[test]
fn path_buf() {
    use std::path::{Path, PathBuf};
    let path = Path::new("foo").to_path_buf();
    let serde_encoded = bincode::serde::serialize(&path, Infinite).unwrap();
    let decoded: PathBuf = bincode::serde::deserialize(&serde_encoded).unwrap();
    assert!(path.to_str() == decoded.to_str());
}
