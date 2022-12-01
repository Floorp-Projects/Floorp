#[macro_use]
extern crate serde_derive;

#[cfg(feature = "std")]
mod std_tests {
    use serde_cbor;

    use std::collections::BTreeMap;

    #[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
    struct TupleStruct(String, i32, u64);

    #[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
    struct UnitStruct;

    #[derive(Serialize, Deserialize, Clone, Debug, PartialEq)]
    struct Struct<'a> {
        tuple_struct: TupleStruct,
        tuple: (String, f32, f64),
        map: BTreeMap<String, String>,
        bytes: &'a [u8],
        array: Vec<String>,
        unit_array: Vec<UnitStruct>,
    }

    use serde_cbor::value::Value;
    use std::iter::FromIterator;

    #[test]
    fn serde() {
        let tuple_struct = TupleStruct(format!("test"), -60, 3000);

        let tuple = (format!("hello"), -50.0040957, -12.094635556478);

        let map = BTreeMap::from_iter(
            [
                (format!("key1"), format!("value1")),
                (format!("key2"), format!("value2")),
                (format!("key3"), format!("value3")),
                (format!("key4"), format!("value4")),
            ]
            .iter()
            .cloned(),
        );

        let bytes = b"test byte string";

        let array = vec![format!("one"), format!("two"), format!("three")];
        let unit_array = vec![UnitStruct, UnitStruct, UnitStruct];

        let data = Struct {
            tuple_struct,
            tuple,
            map,
            bytes,
            array,
            unit_array,
        };

        let value = serde_cbor::value::to_value(data.clone()).unwrap();
        println!("{:?}", value);

        let data_ser = serde_cbor::to_vec(&value).unwrap();
        let data_de_value: Value = serde_cbor::from_slice(&data_ser).unwrap();

        fn as_object(value: &Value) -> &BTreeMap<Value, Value> {
            if let Value::Map(ref v) = value {
                return v;
            }
            panic!()
        }

        for ((k1, v1), (k2, v2)) in as_object(&value)
            .iter()
            .zip(as_object(&data_de_value).iter())
        {
            assert_eq!(k1, k2);
            assert_eq!(v1, v2);
        }

        assert_eq!(value, data_de_value);
    }

    #[derive(Debug, Deserialize, Serialize)]
    struct SmallStruct {
        spam: u32,
        eggs: u32,
    }

    #[test]
    fn small_struct() {
        // Test whether the packed format works.
        // Field names should not be serialized,
        // instead field indizes are serialized.
        let value = SmallStruct { spam: 17, eggs: 42 };
        let data = serde_cbor::ser::to_vec_packed(&value).unwrap();
        let reference = b"\xa2\x00\x11\x01\x18\x2a";
        assert_eq!(data, reference);
    }
}
