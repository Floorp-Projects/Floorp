#[macro_use]
extern crate darling;
extern crate syn;

use darling::ast;
use darling::FromDeriveInput;

#[derive(Debug,FromDeriveInput)]
#[darling(attributes(from_variants), supports(enum_any))]
pub struct Container {
    body: ast::Body<Variant, ()>,
}

#[derive(Default, Debug, FromVariant)]
#[darling(default, attributes(from_variants), supports(newtype, unit))]
pub struct Variant {
    into: Option<bool>,
    skip: Option<bool>,
}

#[test]
fn expansion() {
    let di = syn::parse_derive_input(r#"
        enum Hello {
            World(bool),
            String(String),
        }
    "#).unwrap();

    Container::from_derive_input(&di).unwrap();
}

#[test]
fn unsupported_shape() {
    let di = syn::parse_derive_input(r#"
        enum Hello {
            Foo(u16),
            World {
                name: String
            },
        }
    "#).unwrap();

    Container::from_derive_input(&di).unwrap_err();
}