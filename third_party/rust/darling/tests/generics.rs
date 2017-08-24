#[macro_use]
extern crate darling;
extern crate syn;

use darling::FromDeriveInput;

#[derive(Debug, Clone, FromMetaItem)]
struct Wrapper<T>(pub T);

#[derive(Debug, FromDeriveInput)]
#[darling(attributes(hello))]
struct Foo<T> {
    lorem: Wrapper<T>,
}

#[test]
fn expansion() {
    let di = syn::parse_derive_input(r#"
        #[hello(lorem = "Hello")]
        pub struct Foo;
    "#)
        .unwrap();

    let _parsed = Foo::<String>::from_derive_input(&di).unwrap();
}