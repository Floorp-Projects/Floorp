#![cfg(feature = "serde")]
#![allow(dead_code)]

use enumset::*;
use serde_derive::*;

// Test resistance against shadowed types.
type Some = ();
type None = ();
type Result = ();

#[derive(Serialize, Deserialize, EnumSetType, Debug)]
#[enumset(serialize_as_list)]
#[serde(crate="serde2")]
pub enum ListEnum {
    A, B, C, D, E, F, G, H,
}

#[derive(EnumSetType, Debug)]
#[enumset(serialize_repr = "u128")]
pub enum ReprEnum {
    A, B, C, D, E, F, G, H,
}

#[derive(EnumSetType, Debug)]
#[enumset(serialize_repr = "u128", serialize_deny_unknown)]
pub enum DenyUnknownEnum {
    A, B, C, D, E, F, G, H,
}

macro_rules! serde_test_simple {
    ($e:ident, $ser_size:expr) => {
        #[test]
        fn serialize_deserialize_test_bincode() {
            let value = $e::A | $e::C | $e::D | $e::F | $e::E | $e::G;
            let serialized = bincode::serialize(&value).unwrap();
            let deserialized = bincode::deserialize::<EnumSet<$e>>(&serialized).unwrap();
            assert_eq!(value, deserialized);
            if $ser_size != !0 {
                assert_eq!(serialized.len(), $ser_size);
            }
        }

        #[test]
        fn serialize_deserialize_test_json() {
            let value = $e::A | $e::C | $e::D | $e::F | $e::E | $e::G;
            let serialized = serde_json::to_string(&value).unwrap();
            let deserialized = serde_json::from_str::<EnumSet<$e>>(&serialized).unwrap();
            assert_eq!(value, deserialized);
        }
    }
}
macro_rules! serde_test {
    ($e:ident, $ser_size:expr) => {
        serde_test_simple!($e, $ser_size);

        #[test]
        fn deserialize_all_test() {
            let serialized = bincode::serialize(&!0u128).unwrap();
            let deserialized = bincode::deserialize::<EnumSet<$e>>(&serialized).unwrap();
            assert_eq!(EnumSet::<$e>::all(), deserialized);
        }
    }
}
macro_rules! tests {
    ($m:ident, $($tt:tt)*) => { mod $m { use super::*; $($tt)*; } }
}

#[test]
fn test_deny_unknown() {
    let serialized = bincode::serialize(&!0u128).unwrap();
    let deserialized = bincode::deserialize::<EnumSet<DenyUnknownEnum>>(&serialized);
    assert!(deserialized.is_err());
}

#[test]
fn test_json_reprs() {
    assert_eq!(ListEnum::A | ListEnum::C | ListEnum::F,
               serde_json::from_str::<EnumSet<ListEnum>>(r#"["A","C","F"]"#).unwrap());
    assert_eq!(ReprEnum::A | ReprEnum::C | ReprEnum::D,
               serde_json::from_str::<EnumSet<ReprEnum>>("13").unwrap());
    assert_eq!(r#"["A","C","F"]"#,
               serde_json::to_string(&(ListEnum::A | ListEnum::C | ListEnum::F)).unwrap());
    assert_eq!("13",
               serde_json::to_string(&(ReprEnum::A | ReprEnum::C | ReprEnum::D)).unwrap());
}

tests!(list_enum, serde_test_simple!(ListEnum, !0));
tests!(repr_enum, serde_test!(ReprEnum, 16));
tests!(deny_unknown_enum, serde_test_simple!(DenyUnknownEnum, 16));
