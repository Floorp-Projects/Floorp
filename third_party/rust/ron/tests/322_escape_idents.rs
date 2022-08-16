use serde::{Deserialize, Serialize};

#[derive(Debug, Deserialize, PartialEq, Serialize)]
#[serde(rename_all = "kebab-case")]
enum MyEnumWithDashes {
    ThisIsMyUnitVariant,
    ThisIsMyTupleVariant(bool, i32),
}

#[derive(Debug, Deserialize, PartialEq, Serialize)]
#[serde(rename_all = "kebab-case")]
struct MyStructWithDashes {
    my_enum: MyEnumWithDashes,
    #[serde(rename = "2nd")]
    my_enum2: MyEnumWithDashes,
    will_be_renamed: u32,
}

#[test]
fn roundtrip_ident_with_dash() {
    let value = MyStructWithDashes {
        my_enum: MyEnumWithDashes::ThisIsMyUnitVariant,
        my_enum2: MyEnumWithDashes::ThisIsMyTupleVariant(false, -3),
        will_be_renamed: 32,
    };

    let serial = ron::ser::to_string(&value).unwrap();

    println!("Serialized: {}", serial);

    let deserial = ron::de::from_str(&serial);

    assert_eq!(Ok(value), deserial);
}
