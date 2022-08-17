use serde::{Deserialize, Serialize};

use ron::{extensions::Extensions, ser::PrettyConfig, Options};

#[derive(Serialize, Deserialize)]
struct Newtype(f64);

#[derive(Serialize, Deserialize)]
struct Struct(Option<u32>, Newtype);

#[test]
fn default_options() {
    let ron = Options::default();

    let de: Struct = ron.from_str("(Some(42),(4.2))").unwrap();
    let ser = ron.to_string(&de).unwrap();

    assert_eq!(ser, "(Some(42),(4.2))")
}

#[test]
fn single_default_extension() {
    let ron = Options::default().with_default_extension(Extensions::IMPLICIT_SOME);

    let de: Struct = ron.from_str("(42,(4.2))").unwrap();
    let ser = ron.to_string(&de).unwrap();

    assert_eq!(ser, "(42,(4.2))");

    let de: Struct = ron.from_str("#![enable(implicit_some)](42,(4.2))").unwrap();
    let ser = ron.to_string(&de).unwrap();

    assert_eq!(ser, "(42,(4.2))");

    let de: Struct = ron
        .from_str("#![enable(implicit_some)]#![enable(unwrap_newtypes)](42,4.2)")
        .unwrap();
    let ser = ron.to_string(&de).unwrap();

    assert_eq!(ser, "(42,(4.2))");

    let de: Struct = ron
        .from_str("#![enable(implicit_some)]#![enable(unwrap_newtypes)](42,4.2)")
        .unwrap();
    let ser = ron
        .to_string_pretty(
            &de,
            PrettyConfig::default().extensions(Extensions::UNWRAP_NEWTYPES),
        )
        .unwrap();

    assert_eq!(ser, "#![enable(unwrap_newtypes)]\n(42, 4.2)");
}
