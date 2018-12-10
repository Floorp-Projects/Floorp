//! A newtype struct should be able to derive `FromMeta` if its member implements it.

#[macro_use]
extern crate darling;

extern crate syn;

use darling::FromDeriveInput;

#[derive(Debug, FromMeta, PartialEq, Eq)]
struct Lorem(bool);

#[derive(Debug, FromDeriveInput)]
#[darling(attributes(newtype))]
struct DemoContainer {
    lorem: Lorem,
}

#[test]
fn generated() {
    let di = syn::parse_str(
        r#"
        #[derive(Baz)]
        #[newtype(lorem = false)]
        pub struct Foo;
    "#,
    ).unwrap();

    let c = DemoContainer::from_derive_input(&di).unwrap();

    assert_eq!(c.lorem, Lorem(false));
}
