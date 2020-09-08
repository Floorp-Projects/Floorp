use quote::quote;
use syn::{Item, Pat, Stmt};

#[test]
fn test_pat_ident() {
    match syn::parse2(quote!(self)).unwrap() {
        Pat::Ident(_) => (),
        value => panic!("expected PatIdent, got {:?}", value),
    }
}

#[test]
fn test_pat_path() {
    match syn::parse2(quote!(self::CONST)).unwrap() {
        Pat::Path(_) => (),
        value => panic!("expected PatPath, got {:?}", value),
    }
}

#[test]
fn test_leading_vert() {
    // https://github.com/rust-lang/rust/blob/1.43.0/src/test/ui/or-patterns/remove-leading-vert.rs

    syn::parse_str::<Item>("fn f() {}").unwrap();
    syn::parse_str::<Item>("fn fun1(| A: E) {}").unwrap_err();
    syn::parse_str::<Item>("fn fun2(|| A: E) {}").unwrap_err();

    syn::parse_str::<Stmt>("let | () = ();").unwrap();
    syn::parse_str::<Stmt>("let (| A): E;").unwrap_err();
    syn::parse_str::<Stmt>("let (|| A): (E);").unwrap_err();
    syn::parse_str::<Stmt>("let (| A,): (E,);").unwrap_err();
    syn::parse_str::<Stmt>("let [| A]: [E; 1];").unwrap_err();
    syn::parse_str::<Stmt>("let [|| A]: [E; 1];").unwrap_err();
    syn::parse_str::<Stmt>("let TS(| A): TS;").unwrap_err();
    syn::parse_str::<Stmt>("let TS(|| A): TS;").unwrap_err();
    syn::parse_str::<Stmt>("let NS { f: | A }: NS;").unwrap_err();
    syn::parse_str::<Stmt>("let NS { f: || A }: NS;").unwrap_err();
}
