extern crate syn;
extern crate synstructure;
#[macro_use]
extern crate quote;
use synstructure::{match_substructs, BindStyle, BindOpts};

#[test]
fn alt_prefix() {
    let ast = syn::parse_macro_input("struct A { a: i32, b: i32 }").unwrap();

    let opts = BindOpts::with_prefix(BindStyle::Ref, "__foo".into());
    let tokens = match_substructs(&ast, &opts, |bindings| {
        assert_eq!(bindings.len(), 2);
        assert_eq!(bindings[0].ident.as_ref(), "__foo_0");
        assert_eq!(bindings[1].ident.as_ref(), "__foo_1");
        quote!("some_random_string")
    });
    let e = quote! {
        A{ a: ref __foo_0, b: ref __foo_1, } => { "some_random_string" }
    }.to_string();
    assert_eq!(tokens.to_string(), e);
}
