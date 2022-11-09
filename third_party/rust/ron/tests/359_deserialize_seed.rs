#[test]
fn test_deserialize_seed() {
    // Test adapted from David Tolnay's serde-yaml:
    // https://github.com/dtolnay/serde-yaml/blob/8a806e316302fd2e6541dccee6d166dd51b689d6/tests/test_de.rs#L357-L392

    struct Seed(i64);

    impl<'de> serde::de::DeserializeSeed<'de> for Seed {
        type Value = i64;

        fn deserialize<D>(self, deserializer: D) -> Result<i64, D::Error>
        where
            D: serde::de::Deserializer<'de>,
        {
            struct Visitor(i64);

            impl<'de> serde::de::Visitor<'de> for Visitor {
                type Value = i64;

                fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
                    write!(formatter, "an integer")
                }

                fn visit_i64<E: serde::de::Error>(self, v: i64) -> Result<i64, E> {
                    Ok(v * self.0)
                }

                fn visit_u64<E: serde::de::Error>(self, v: u64) -> Result<i64, E> {
                    Ok(v as i64 * self.0)
                }
            }

            deserializer.deserialize_any(Visitor(self.0))
        }
    }

    let cases = [("3", 5, 15), ("6", 7, 42), ("-5", 9, -45)];

    for &(ron, seed, expected) in &cases {
        let deserialized = ron::Options::default()
            .from_str_seed(ron, Seed(seed))
            .unwrap();

        assert_eq!(expected, deserialized);
    }

    assert_eq!(
        ron::Options::default().from_str_seed("'a'", Seed(42)),
        Err(ron::error::Error {
            code: ron::ErrorCode::Message(String::from(
                "invalid type: string \"a\", expected an integer"
            )),
            position: ron::error::Position { line: 0, col: 0 },
        })
    );
}
