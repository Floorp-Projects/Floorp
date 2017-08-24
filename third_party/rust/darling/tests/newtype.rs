//! A newtype struct should be able to derive `FromMetaItem` if its member implements it.

#[macro_use]
extern crate darling;

extern crate syn;

use darling::FromDeriveInput;

#[derive(Debug, FromMetaItem, PartialEq, Eq)]
struct Lorem(bool);

#[derive(Debug, FromDeriveInput)]
#[darling(attributes(newtype))]
struct DemoContainer {
    lorem: Lorem
}

#[test]
fn generated() {
    let di = syn::parse_derive_input(r#"
        #[derive(Baz)]
        #[newtype(lorem = false)]
        pub struct Foo;
    "#).unwrap();

    let c = DemoContainer::from_derive_input(&di).unwrap();

    assert_eq!(c.lorem, Lorem(false));
}