use serde::de::{Deserialize, Deserializer, SeqAccess, Visitor};
use serde::ser::{Serialize, Serializer};
use std::collections::HashSet;
use std::fmt;

#[derive(Debug, Clone, Eq, PartialEq)]
struct Names {
    inner: HashSet<String>,
}

impl Serialize for Names {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut names: Vec<_> = self.inner.iter().collect();
        names.sort();
        names.serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for Names {
    fn deserialize<D>(deserializer: D) -> Result<Names, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_any(NamesVisitor)
    }
}

struct NamesVisitor;

impl<'de> Visitor<'de> for NamesVisitor {
    type Value = Names;

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("names or list of names")
    }

    fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        let mut out = HashSet::new();
        out.insert(v.to_string());
        Ok(Names { inner: out })
    }

    fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
    where
        A: SeqAccess<'de>,
    {
        let mut out: HashSet<String> = HashSet::new();

        // FIXME: Change `&str` to String to make the error go away
        while let Some(s) = seq.next_element::<&str>()? {
            out.insert(s.to_string());
        }
        Ok(Names { inner: out })
    }
}

#[test]
#[ignore]
/// This test is an almost exact replica of the "string or struct" example
/// in the [serde guide](https://serde.rs/string-or-struct.html)
///
/// FIXME: it currently breaks. If you explicitly select `String` instead
/// of `&str` in the FIXME above, it works.
fn test_visitor() {
    let single = r#"
---
"foo"
"#;
    let single_expected = Names {
        inner: {
            let mut i = HashSet::new();
            i.insert("foo".into());
            i
        },
    };
    let multi = r#"
---
- "foo"
- "bar"
"#;
    let multi_expected = Names {
        inner: {
            let mut i = HashSet::new();
            i.insert("foo".into());
            i.insert("bar".into());
            i
        },
    };

    let result: Names = serde_yaml::from_str(single).expect("didn't deserialize");
    assert_eq!(result, single_expected);

    let result: Names = serde_yaml::from_str(multi).expect("didn't deserialize");
    assert_eq!(result, multi_expected);
}
