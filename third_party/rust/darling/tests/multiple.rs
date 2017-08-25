#[macro_use]
extern crate darling;
extern crate syn;

use darling::FromDeriveInput;

#[derive(FromDeriveInput)]
#[darling(attributes(hello))]
#[allow(dead_code)]
struct Lorem {
    ident: syn::Ident,
    ipsum: Ipsum,
}

#[derive(FromMetaItem)]
struct Ipsum {
    #[darling(multiple)]
    dolor: Vec<String>,
}

#[test]
fn expand_many() {
    let di = syn::parse_derive_input(r#"
        #[hello(ipsum(dolor = "Hello", dolor = "World"))]
        pub struct Baz;
    "#).unwrap();

    let lorem: Lorem = Lorem::from_derive_input(&di).unwrap();
    assert_eq!(lorem.ipsum.dolor, vec!["Hello".to_string(), "World".to_string()]);
}