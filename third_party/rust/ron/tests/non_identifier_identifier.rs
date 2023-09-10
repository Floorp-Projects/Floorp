macro_rules! test_non_identifier {
    ($test_name:ident => $deserialize_method:ident($($deserialize_param:expr),*)) => {
        #[test]
        fn $test_name() {
            use serde::{Deserialize, Deserializer, de::Visitor, de::MapAccess};

            struct FieldVisitor;

            impl<'de> Visitor<'de> for FieldVisitor {
                type Value = FieldName;

                // GRCOV_EXCL_START
                fn expecting(&self, fmt: &mut std::fmt::Formatter) -> std::fmt::Result {
                    fmt.write_str("an error")
                }
                // GRCOV_EXCL_STOP
            }

            struct FieldName;

            impl<'de> Deserialize<'de> for FieldName {
                fn deserialize<D: Deserializer<'de>>(deserializer: D)
                    -> Result<Self, D::Error>
                {
                    deserializer.$deserialize_method($($deserialize_param,)* FieldVisitor)
                }
            }

            struct StructVisitor;

            impl<'de> Visitor<'de> for StructVisitor {
                type Value = Struct;

                // GRCOV_EXCL_START
                fn expecting(&self, fmt: &mut std::fmt::Formatter) -> std::fmt::Result {
                    fmt.write_str("a struct")
                }
                // GRCOV_EXCL_STOP

                fn visit_map<A: MapAccess<'de>>(self, mut map: A)
                    -> Result<Self::Value, A::Error>
                {
                    map.next_key::<FieldName>().map(|_| Struct)
                }
            }

            #[derive(Debug)]
            struct Struct;

            impl<'de> Deserialize<'de> for Struct {
                fn deserialize<D: Deserializer<'de>>(deserializer: D)
                    -> Result<Self, D::Error>
                {
                    deserializer.deserialize_struct("Struct", &[], StructVisitor)
                }
            }

            assert_eq!(
                ron::from_str::<Struct>("(true: 4)").unwrap_err().code,
                ron::Error::ExpectedIdentifier
            )
        }
    };
}

test_non_identifier! { test_bool => deserialize_bool() }
test_non_identifier! { test_i8 => deserialize_i8() }
test_non_identifier! { test_i16 => deserialize_i16() }
test_non_identifier! { test_i32 => deserialize_i32() }
test_non_identifier! { test_i64 => deserialize_i64() }
#[cfg(feature = "integer128")]
test_non_identifier! { test_i128 => deserialize_i128() }
test_non_identifier! { test_u8 => deserialize_u8() }
test_non_identifier! { test_u16 => deserialize_u16() }
test_non_identifier! { test_u32 => deserialize_u32() }
test_non_identifier! { test_u64 => deserialize_u64() }
#[cfg(feature = "integer128")]
test_non_identifier! { test_u128 => deserialize_u128() }
test_non_identifier! { test_f32 => deserialize_f32() }
test_non_identifier! { test_f64 => deserialize_f64() }
test_non_identifier! { test_char => deserialize_char() }
test_non_identifier! { test_string => deserialize_string() }
test_non_identifier! { test_bytes => deserialize_bytes() }
test_non_identifier! { test_byte_buf => deserialize_byte_buf() }
test_non_identifier! { test_option => deserialize_option() }
test_non_identifier! { test_unit => deserialize_unit() }
test_non_identifier! { test_unit_struct => deserialize_unit_struct("") }
test_non_identifier! { test_newtype_struct => deserialize_newtype_struct("") }
test_non_identifier! { test_seq => deserialize_seq() }
test_non_identifier! { test_tuple => deserialize_tuple(0) }
test_non_identifier! { test_tuple_struct => deserialize_tuple_struct("", 0) }
test_non_identifier! { test_map => deserialize_map() }
test_non_identifier! { test_struct => deserialize_struct("", &[]) }
test_non_identifier! { test_enum => deserialize_enum("", &[]) }
