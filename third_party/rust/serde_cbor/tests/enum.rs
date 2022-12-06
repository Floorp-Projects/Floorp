use serde::Serialize;
use serde_cbor;
use serde_cbor::ser::{Serializer, SliceWrite};

#[macro_use]
extern crate serde_derive;

#[test]
fn test_simple_data_enum_roundtrip() {
    #[derive(Debug, Serialize, Deserialize, PartialEq)]
    enum DataEnum {
        A(u32),
        B(f32),
    }

    let a = DataEnum::A(42);

    let mut slice = [0u8; 64];
    let writer = SliceWrite::new(&mut slice);
    let mut serializer = Serializer::new(writer);
    a.serialize(&mut serializer).unwrap();
    let writer = serializer.into_inner();
    let end = writer.bytes_written();
    let slice = writer.into_inner();
    let deserialized: DataEnum =
        serde_cbor::de::from_slice_with_scratch(&slice[..end], &mut []).unwrap();
    assert_eq!(a, deserialized);
}

#[cfg(feature = "std")]
mod std_tests {
    use std::collections::BTreeMap;

    use serde_cbor::ser::{IoWrite, Serializer};
    use serde_cbor::value::Value;
    use serde_cbor::{from_slice, to_vec};

    pub fn to_vec_legacy<T>(value: &T) -> serde_cbor::Result<Vec<u8>>
    where
        T: serde::ser::Serialize,
    {
        let mut vec = Vec::new();
        value.serialize(&mut Serializer::new(&mut IoWrite::new(&mut vec)).legacy_enums())?;
        Ok(vec)
    }

    #[derive(Debug, Serialize, Deserialize, PartialEq, Eq)]
    enum Enum {
        A,
        B,
    }

    #[derive(Debug, Serialize, Deserialize, PartialEq, Eq)]
    struct EnumStruct {
        e: Enum,
    }

    #[test]
    fn test_enum() {
        let enum_struct = EnumStruct { e: Enum::B };
        let raw = &to_vec(&enum_struct).unwrap();
        println!("raw enum {:?}", raw);
        let re: EnumStruct = from_slice(raw).unwrap();
        assert_eq!(enum_struct, re);
    }

    #[repr(u16)]
    #[derive(Debug, Serialize, Deserialize, PartialEq, Eq)]
    enum ReprEnum {
        A,
        B,
    }

    #[derive(Debug, Serialize, Deserialize, PartialEq, Eq)]
    struct ReprEnumStruct {
        e: ReprEnum,
    }

    #[test]
    fn test_repr_enum() {
        let repr_enum_struct = ReprEnumStruct { e: ReprEnum::B };
        let re: ReprEnumStruct = from_slice(&to_vec(&repr_enum_struct).unwrap()).unwrap();
        assert_eq!(repr_enum_struct, re);
    }

    #[derive(Debug, Serialize, Deserialize, PartialEq, Eq)]
    enum DataEnum {
        A(u32),
        B(bool, u8),
        C { x: u8, y: String },
    }

    #[test]
    fn test_data_enum() {
        let data_enum_a = DataEnum::A(4);
        let re_a: DataEnum = from_slice(&to_vec(&data_enum_a).unwrap()).unwrap();
        assert_eq!(data_enum_a, re_a);
        let data_enum_b = DataEnum::B(true, 42);
        let re_b: DataEnum = from_slice(&to_vec(&data_enum_b).unwrap()).unwrap();
        assert_eq!(data_enum_b, re_b);
        let data_enum_c = DataEnum::C {
            x: 3,
            y: "foo".to_owned(),
        };
        println!("{:?}", &to_vec(&data_enum_c).unwrap());
        let re_c: DataEnum = from_slice(&to_vec(&data_enum_c).unwrap()).unwrap();
        assert_eq!(data_enum_c, re_c);
    }

    #[test]
    fn test_serialize() {
        assert_eq!(to_vec_legacy(&Enum::A).unwrap(), &[97, 65]);
        assert_eq!(to_vec_legacy(&Enum::B).unwrap(), &[97, 66]);
        assert_eq!(
            to_vec_legacy(&DataEnum::A(42)).unwrap(),
            &[130, 97, 65, 24, 42]
        );
        assert_eq!(
            to_vec_legacy(&DataEnum::B(true, 9)).unwrap(),
            &[131, 97, 66, 245, 9]
        );
    }

    #[test]
    fn test_newtype_struct() {
        #[derive(Debug, Deserialize, Serialize, PartialEq, Eq)]
        pub struct Newtype(u8);
        assert_eq!(to_vec(&142u8).unwrap(), to_vec(&Newtype(142u8)).unwrap());
        assert_eq!(from_slice::<Newtype>(&[24, 142]).unwrap(), Newtype(142));
    }

    #[derive(Deserialize, PartialEq, Debug)]
    enum Foo {
        #[serde(rename = "require")]
        Require,
    }

    #[test]
    fn test_variable_length_array() {
        let slice = b"\x9F\x67\x72\x65\x71\x75\x69\x72\x65\xFF";
        let value: Vec<Foo> = from_slice(slice).unwrap();
        assert_eq!(value, [Foo::Require]);
    }

    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    enum Bar {
        Empty,
        Number(i32),
        Flag(String, bool),
        Point { x: i32, y: i32 },
    }

    #[test]
    fn test_enum_as_map() {
        // unit variants serialize like bare strings
        let empty_s = to_vec_legacy(&Bar::Empty).unwrap();
        let empty_str_s = to_vec_legacy(&"Empty").unwrap();
        assert_eq!(empty_s, empty_str_s);

        // tuple-variants serialize like ["<variant>", values..]
        let number_s = to_vec_legacy(&Bar::Number(42)).unwrap();
        let number_vec = vec![Value::Text("Number".to_string()), Value::Integer(42)];
        let number_vec_s = to_vec_legacy(&number_vec).unwrap();
        assert_eq!(number_s, number_vec_s);

        let flag_s = to_vec_legacy(&Bar::Flag("foo".to_string(), true)).unwrap();
        let flag_vec = vec![
            Value::Text("Flag".to_string()),
            Value::Text("foo".to_string()),
            Value::Bool(true),
        ];
        let flag_vec_s = to_vec_legacy(&flag_vec).unwrap();
        assert_eq!(flag_s, flag_vec_s);

        // struct-variants serialize like ["<variant>", {struct..}]
        let point_s = to_vec_legacy(&Bar::Point { x: 5, y: -5 }).unwrap();
        let mut struct_map = BTreeMap::new();
        struct_map.insert(Value::Text("x".to_string()), Value::Integer(5));
        struct_map.insert(Value::Text("y".to_string()), Value::Integer(-5));
        let point_vec = vec![
            Value::Text("Point".to_string()),
            Value::Map(struct_map.clone()),
        ];
        let point_vec_s = to_vec_legacy(&point_vec).unwrap();
        assert_eq!(point_s, point_vec_s);

        // enum_as_map matches serde_json's default serialization for enums.

        // unit variants still serialize like bare strings
        let empty_s = to_vec(&Bar::Empty).unwrap();
        assert_eq!(empty_s, empty_str_s);

        // 1-element tuple variants serialize like {"<variant>": value}
        let number_s = to_vec(&Bar::Number(42)).unwrap();
        let mut number_map = BTreeMap::new();
        number_map.insert("Number", 42);
        let number_map_s = to_vec(&number_map).unwrap();
        assert_eq!(number_s, number_map_s);

        // multi-element tuple variants serialize like {"<variant>": [values..]}
        let flag_s = to_vec(&Bar::Flag("foo".to_string(), true)).unwrap();
        let mut flag_map = BTreeMap::new();
        flag_map.insert(
            "Flag",
            vec![Value::Text("foo".to_string()), Value::Bool(true)],
        );
        let flag_map_s = to_vec(&flag_map).unwrap();
        assert_eq!(flag_s, flag_map_s);

        // struct-variants serialize like {"<variant>", {struct..}}
        let point_s = to_vec(&Bar::Point { x: 5, y: -5 }).unwrap();
        let mut point_map = BTreeMap::new();
        point_map.insert("Point", Value::Map(struct_map));
        let point_map_s = to_vec(&point_map).unwrap();
        assert_eq!(point_s, point_map_s);

        // deserialization of all encodings should just work
        let empty_str_ds = from_slice(&empty_str_s).unwrap();
        assert_eq!(Bar::Empty, empty_str_ds);

        let number_vec_ds = from_slice(&number_vec_s).unwrap();
        assert_eq!(Bar::Number(42), number_vec_ds);
        let number_map_ds = from_slice(&number_map_s).unwrap();
        assert_eq!(Bar::Number(42), number_map_ds);

        let flag_vec_ds = from_slice(&flag_vec_s).unwrap();
        assert_eq!(Bar::Flag("foo".to_string(), true), flag_vec_ds);
        let flag_map_ds = from_slice(&flag_map_s).unwrap();
        assert_eq!(Bar::Flag("foo".to_string(), true), flag_map_ds);

        let point_vec_ds = from_slice(&point_vec_s).unwrap();
        assert_eq!(Bar::Point { x: 5, y: -5 }, point_vec_ds);
        let point_map_ds = from_slice(&point_map_s).unwrap();
        assert_eq!(Bar::Point { x: 5, y: -5 }, point_map_ds);
    }
}
