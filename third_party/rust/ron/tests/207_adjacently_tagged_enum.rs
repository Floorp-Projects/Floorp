use ron::{de::from_str, ser::to_string};
use serde::{Deserialize, Serialize};

#[derive(Debug, PartialEq, Serialize, Deserialize)]
#[serde(tag = "type", content = "data")]
enum TestEnum {
    Name(String),
    Index(u32),
}

#[test]
fn test_adjacently_tagged() {
    let source = TestEnum::Index(1);

    let ron_string = to_string(&source).unwrap();

    assert_eq!(ron_string, "(type:Index,data:1)");

    let deserialized = from_str::<TestEnum>(&ron_string).unwrap();

    assert_eq!(deserialized, source);
}
