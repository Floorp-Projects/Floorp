#[macro_use]
extern crate darling;
extern crate syn;

use darling::ast;
use darling::FromDeriveInput;

#[derive(Debug, FromDeriveInput)]
#[darling(attributes(from_variants), supports(enum_any))]
pub struct Container {
    data: ast::Data<Variant, ()>,
}

#[derive(Default, Debug, FromVariant)]
#[darling(default, attributes(from_variants), supports(newtype, unit))]
pub struct Variant {
    into: Option<bool>,
    skip: Option<bool>,
}

#[derive(Debug, FromDeriveInput)]
#[darling(attributes(from_struct), supports(struct_named))]
pub struct StructContainer {
    data: ast::Data<(), syn::Field>,
}

mod source {
    use syn::{self, DeriveInput};

    pub fn newtype_enum() -> DeriveInput {
        syn::parse_str(
            r#"
        enum Hello {
            World(bool),
            String(String),
        }
    "#,
        ).unwrap()
    }

    pub fn named_field_enum() -> DeriveInput {
        syn::parse_str(
            r#"
        enum Hello {
            Foo(u16),
            World {
                name: String
            },
        }
    "#,
        ).unwrap()
    }

    pub fn named_struct() -> DeriveInput {
        syn::parse_str(
            r#"
        struct Hello {
            world: bool,
        }
    "#,
        ).unwrap()
    }

    pub fn tuple_struct() -> DeriveInput {
        syn::parse_str("struct Hello(String, bool);").unwrap()
    }
}

#[test]
fn enum_newtype_or_unit() {
    // Should pass
    Container::from_derive_input(&source::newtype_enum()).unwrap();

    // Should error
    Container::from_derive_input(&source::named_field_enum()).unwrap_err();
    Container::from_derive_input(&source::named_struct()).unwrap_err();
}

#[test]
fn struct_named() {
    // Should pass
    StructContainer::from_derive_input(&source::named_struct()).unwrap();

    // Should fail
    StructContainer::from_derive_input(&source::tuple_struct()).unwrap_err();
    StructContainer::from_derive_input(&source::named_field_enum()).unwrap_err();
    StructContainer::from_derive_input(&source::newtype_enum()).unwrap_err();
}
