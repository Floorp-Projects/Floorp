use std::any::Any;

use serde::{
    de::{MapAccess, Visitor},
    Deserializer,
};

#[test]
fn manually_deserialize_dyn() {
    let ron = r#"SerializeDyn(
        type: "engine_utils::types::registry::tests::Player",
    )"#;

    let mut de = ron::Deserializer::from_bytes(ron.as_bytes()).unwrap();

    let result = de
        .deserialize_struct("SerializeDyn", &["type"], SerializeDynVisitor)
        .unwrap();

    assert_eq!(
        *result.downcast::<Option<(String, String)>>().unwrap(),
        Some((
            String::from("type"),
            String::from("engine_utils::types::registry::tests::Player")
        ))
    );
}

struct SerializeDynVisitor;

impl<'de> Visitor<'de> for SerializeDynVisitor {
    type Value = Box<dyn Any>;

    // GRCOV_EXCL_START
    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(formatter, "a serialize dyn struct")
    }
    // GRCOV_EXCL_STOP

    fn visit_map<A: MapAccess<'de>>(self, mut map: A) -> Result<Self::Value, A::Error> {
        let entry = map.next_entry::<&str, String>()?;

        Ok(Box::new(entry.map(|(k, v)| (String::from(k), v))))
    }
}
