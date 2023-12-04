use proc_macro2::Span;
use quote::{quote, ToTokens};
use syn::*;

use diplomat_core::ast;

mod enum_convert;
mod transparent_convert;

fn cfgs_to_stream(attrs: &[Attribute]) -> proc_macro2::TokenStream {
    attrs
        .iter()
        .fold(quote!(), |prev, attr| quote!(#prev #attr))
}

fn gen_params_at_boundary(param: &ast::Param, expanded_params: &mut Vec<FnArg>) {
    match &param.ty {
        ast::TypeName::StrReference(_) | ast::TypeName::PrimitiveSlice(..) => {
            let data_type = if let ast::TypeName::PrimitiveSlice(.., prim) = &param.ty {
                ast::TypeName::Primitive(*prim).to_syn().to_token_stream()
            } else {
                quote! { u8 }
            };
            expanded_params.push(FnArg::Typed(PatType {
                attrs: vec![],
                pat: Box::new(Pat::Ident(PatIdent {
                    attrs: vec![],
                    by_ref: None,
                    mutability: None,
                    ident: Ident::new(&format!("{}_diplomat_data", param.name), Span::call_site()),
                    subpat: None,
                })),
                colon_token: syn::token::Colon(Span::call_site()),
                ty: Box::new(
                    parse2({
                        if let ast::TypeName::PrimitiveSlice(_, ast::Mutability::Mutable, _) =
                            &param.ty
                        {
                            quote! { *mut #data_type }
                        } else {
                            quote! { *const #data_type }
                        }
                    })
                    .unwrap(),
                ),
            }));

            expanded_params.push(FnArg::Typed(PatType {
                attrs: vec![],
                pat: Box::new(Pat::Ident(PatIdent {
                    attrs: vec![],
                    by_ref: None,
                    mutability: None,
                    ident: Ident::new(&format!("{}_diplomat_len", param.name), Span::call_site()),
                    subpat: None,
                })),
                colon_token: syn::token::Colon(Span::call_site()),
                ty: Box::new(
                    parse2(quote! {
                        usize
                    })
                    .unwrap(),
                ),
            }));
        }
        o => {
            expanded_params.push(FnArg::Typed(PatType {
                attrs: vec![],
                pat: Box::new(Pat::Ident(PatIdent {
                    attrs: vec![],
                    by_ref: None,
                    mutability: None,
                    ident: Ident::new(param.name.as_str(), Span::call_site()),
                    subpat: None,
                })),
                colon_token: syn::token::Colon(Span::call_site()),
                ty: Box::new(o.to_syn()),
            }));
        }
    }
}

fn gen_params_invocation(param: &ast::Param, expanded_params: &mut Vec<Expr>) {
    match &param.ty {
        ast::TypeName::StrReference(_) | ast::TypeName::PrimitiveSlice(..) => {
            let data_ident =
                Ident::new(&format!("{}_diplomat_data", param.name), Span::call_site());
            let len_ident = Ident::new(&format!("{}_diplomat_len", param.name), Span::call_site());

            let tokens = if let ast::TypeName::PrimitiveSlice(_, mutability, _) = &param.ty {
                match mutability {
                    ast::Mutability::Mutable => quote! {
                        unsafe { core::slice::from_raw_parts_mut(#data_ident, #len_ident) }
                    },
                    ast::Mutability::Immutable => quote! {
                        unsafe { core::slice::from_raw_parts(#data_ident, #len_ident) }
                    },
                }
            } else {
                // TODO(#57): don't just unwrap? or should we assume that the other side gives us a good value?
                quote! {
                    unsafe {
                        core::str::from_utf8(core::slice::from_raw_parts(#data_ident, #len_ident)).unwrap()
                    }
                }
            };
            expanded_params.push(parse2(tokens).unwrap());
        }
        ast::TypeName::Result(_, _, _) => {
            let param = &param.name;
            expanded_params.push(parse2(quote!(#param.into())).unwrap());
        }
        _ => {
            expanded_params.push(Expr::Path(ExprPath {
                attrs: vec![],
                qself: None,
                path: Ident::new(param.name.as_str(), Span::call_site()).into(),
            }));
        }
    }
}

fn gen_custom_type_method(strct: &ast::CustomType, m: &ast::Method) -> Item {
    let self_ident = Ident::new(strct.name().as_str(), Span::call_site());
    let method_ident = Ident::new(m.name.as_str(), Span::call_site());
    let extern_ident = Ident::new(m.full_path_name.as_str(), Span::call_site());

    let mut all_params = vec![];
    m.params.iter().for_each(|p| {
        gen_params_at_boundary(p, &mut all_params);
    });

    let mut all_params_invocation = vec![];
    m.params.iter().for_each(|p| {
        gen_params_invocation(p, &mut all_params_invocation);
    });

    let this_ident = Pat::Ident(PatIdent {
        attrs: vec![],
        by_ref: None,
        mutability: None,
        ident: Ident::new("this", Span::call_site()),
        subpat: None,
    });

    if let Some(self_param) = &m.self_param {
        all_params.insert(
            0,
            FnArg::Typed(PatType {
                attrs: vec![],
                pat: Box::new(this_ident.clone()),
                colon_token: syn::token::Colon(Span::call_site()),
                ty: Box::new(self_param.to_typename().to_syn()),
            }),
        );
    }

    let lifetimes = {
        let lifetime_env = &m.lifetime_env;
        if lifetime_env.is_empty() {
            quote! {}
        } else {
            quote! { <#lifetime_env> }
        }
    };

    let method_invocation = if m.self_param.is_some() {
        quote! { #this_ident.#method_ident }
    } else {
        quote! { #self_ident::#method_ident }
    };

    let (return_tokens, maybe_into) = if let Some(return_type) = &m.return_type {
        if let ast::TypeName::Result(ok, err, true) = return_type {
            let ok = ok.to_syn();
            let err = err.to_syn();
            (
                quote! { -> diplomat_runtime::DiplomatResult<#ok, #err> },
                quote! { .into() },
            )
        } else {
            let return_type_syn = return_type.to_syn();
            (quote! { -> #return_type_syn }, quote! {})
        }
    } else {
        (quote! {}, quote! {})
    };

    let writeable_flushes = m
        .params
        .iter()
        .filter(|p| p.is_writeable())
        .map(|p| {
            let p = &p.name;
            quote! { #p.flush(); }
        })
        .collect::<Vec<_>>();

    let cfg = cfgs_to_stream(&m.attrs.cfg);

    if writeable_flushes.is_empty() {
        Item::Fn(syn::parse_quote! {
            #[no_mangle]
            #cfg
            extern "C" fn #extern_ident#lifetimes(#(#all_params),*) #return_tokens {
                #method_invocation(#(#all_params_invocation),*) #maybe_into
            }
        })
    } else {
        Item::Fn(syn::parse_quote! {
            #[no_mangle]
            #cfg
            extern "C" fn #extern_ident#lifetimes(#(#all_params),*) #return_tokens {
                let ret = #method_invocation(#(#all_params_invocation),*);
                #(#writeable_flushes)*
                ret #maybe_into
            }
        })
    }
}

struct AttributeInfo {
    repr: bool,
    opaque: bool,
}

impl AttributeInfo {
    fn extract(attrs: &mut Vec<Attribute>) -> Self {
        let mut repr = false;
        let mut opaque = false;
        attrs.retain(|attr| {
            let ident = &attr.path().segments.iter().next().unwrap().ident;
            if ident == "repr" {
                repr = true;
                // don't actually extract repr attrs, just detect them
                return true;
            } else if ident == "diplomat" {
                if attr.path().segments.len() == 2 {
                    let seg = &attr.path().segments.iter().nth(1).unwrap().ident;
                    if seg == "opaque" {
                        opaque = true;
                        return false;
                    } else if seg == "rust_link" || seg == "out" || seg == "attr" {
                        // diplomat-tool reads these, not diplomat::bridge.
                        // throw them away so rustc doesn't complain about unknown attributes
                        return false;
                    } else if seg == "enum_convert" || seg == "transparent_convert" {
                        // diplomat::bridge doesn't read this, but it's handled separately
                        // as an attribute
                        return true;
                    } else {
                        panic!("Only #[diplomat::opaque] and #[diplomat::rust_link] are supported")
                    }
                } else {
                    panic!("#[diplomat::foo] attrs have a single-segment path name")
                }
            }
            true
        });

        Self { repr, opaque }
    }
}

fn gen_bridge(input: ItemMod) -> ItemMod {
    let module = ast::Module::from_syn(&input, true);
    let (brace, mut new_contents) = input.content.unwrap();

    new_contents.iter_mut().for_each(|c| match c {
        Item::Struct(s) => {
            let info = AttributeInfo::extract(&mut s.attrs);
            if info.opaque || !info.repr {
                let repr = if info.opaque {
                    // Normal opaque types don't need repr(transparent) because the inner type is
                    // never referenced. #[diplomat::transparent_convert] handles adding repr(transparent)
                    // on its own
                    quote!()
                } else {
                    quote!(#[repr(C)])
                };
                *s = syn::parse_quote! {
                    #repr
                    #s
                }
            }
        }

        Item::Enum(e) => {
            let info = AttributeInfo::extract(&mut e.attrs);
            if info.opaque {
                panic!("#[diplomat::opaque] not allowed on enums")
            }
            for v in &mut e.variants {
                let info = AttributeInfo::extract(&mut v.attrs);
                if info.opaque {
                    panic!("#[diplomat::opaque] not allowed on enum variants");
                }
            }
            *e = syn::parse_quote! {
                #[repr(C)]
                #e
            };
        }

        Item::Impl(i) => {
            for item in &mut i.items {
                if let syn::ImplItem::Fn(ref mut m) = *item {
                    let info = AttributeInfo::extract(&mut m.attrs);
                    if info.opaque {
                        panic!("#[diplomat::opaque] not allowed on methods")
                    }
                }
            }
        }
        _ => (),
    });

    for custom_type in module.declared_types.values() {
        custom_type.methods().iter().for_each(|m| {
            new_contents.push(gen_custom_type_method(custom_type, m));
        });

        let destroy_ident = Ident::new(
            format!("{}_destroy", custom_type.name()).as_str(),
            Span::call_site(),
        );

        let type_ident = custom_type.name().to_syn();

        let (lifetime_defs, lifetimes) = if let Some(lifetime_env) = custom_type.lifetimes() {
            (
                quote! { <#lifetime_env> },
                lifetime_env.lifetimes_to_tokens(),
            )
        } else {
            (quote! {}, quote! {})
        };

        let cfg = cfgs_to_stream(&custom_type.attrs().cfg);

        // for now, body is empty since all we need to do is drop the box
        // TODO(#13): change to take a `*mut` and handle DST boxes appropriately
        new_contents.push(Item::Fn(syn::parse_quote! {
            #[no_mangle]
            #cfg
            extern "C" fn #destroy_ident#lifetime_defs(this: Box<#type_ident#lifetimes>) {}
        }));
    }

    ItemMod {
        attrs: input.attrs,
        vis: input.vis,
        mod_token: input.mod_token,
        ident: input.ident,
        content: Some((brace, new_contents)),
        semi: input.semi,
        unsafety: None,
    }
}

/// Mark a module to be exposed through Diplomat-generated FFI.
#[proc_macro_attribute]
pub fn bridge(
    _attr: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let expanded = gen_bridge(parse_macro_input!(input));
    proc_macro::TokenStream::from(expanded.to_token_stream())
}

/// Generate From and Into implementations for a Diplomat enum
///
/// This is invoked as `#[diplomat::enum_convert(OtherEnumName)]`
/// on a Diplomat enum. It will assume the other enum has exactly the same variants
/// and generate From and Into implementations using those. In case that enum is `#[non_exhaustive]`,
/// you may use `#[diplomat::enum_convert(OtherEnumName, needs_wildcard)]` to generate a panicky wildcard
/// branch. It is up to the library author to ensure the enums are kept in sync. You may use the `#[non_exhaustive_omitted_patterns]`
/// lint to enforce this.
#[proc_macro_attribute]
pub fn enum_convert(
    attr: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    // proc macros handle compile errors by using special error tokens.
    // In case of an error, we don't want the original code to go away too
    // (otherwise that will cause more errors) so we hold on to it and we tack it in
    // with no modifications below
    let input_cached: proc_macro2::TokenStream = input.clone().into();
    let expanded =
        enum_convert::gen_enum_convert(parse_macro_input!(attr), parse_macro_input!(input));

    let full = quote! {
        #expanded
        #input_cached
    };
    proc_macro::TokenStream::from(full.to_token_stream())
}

/// Generate conversions from inner types for opaque Diplomat types with a single field
///
/// This is invoked as `#[diplomat::transparent_convert]`
/// on an opaque Diplomat type. It will add `#[repr(transparent)]` and implement `pub(crate) fn transparent_convert()`
/// which allows constructing an `&Self` from a reference to the inner field.
#[proc_macro_attribute]
pub fn transparent_convert(
    _attr: proc_macro::TokenStream,
    input: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    // proc macros handle compile errors by using special error tokens.
    // In case of an error, we don't want the original code to go away too
    // (otherwise that will cause more errors) so we hold on to it and we tack it in
    // with no modifications below
    let input_cached: proc_macro2::TokenStream = input.clone().into();
    let expanded = transparent_convert::gen_transparent_convert(parse_macro_input!(input));

    let full = quote! {
        #expanded
        #input_cached
    };
    proc_macro::TokenStream::from(full.to_token_stream())
}

#[cfg(test)]
mod tests {
    use std::fs::File;
    use std::io::{Read, Write};
    use std::process::Command;

    use quote::ToTokens;
    use syn::parse_quote;
    use tempfile::tempdir;

    use super::gen_bridge;

    fn rustfmt_code(code: &str) -> String {
        let dir = tempdir().unwrap();
        let file_path = dir.path().join("temp.rs");
        let mut file = File::create(file_path.clone()).unwrap();

        writeln!(file, "{code}").unwrap();
        drop(file);

        Command::new("rustfmt")
            .arg(file_path.to_str().unwrap())
            .spawn()
            .unwrap()
            .wait()
            .unwrap();

        let mut file = File::open(file_path).unwrap();
        let mut data = String::new();
        file.read_to_string(&mut data).unwrap();
        drop(file);
        dir.close().unwrap();
        data
    }

    #[test]
    fn method_taking_str() {
        insta::assert_display_snapshot!(rustfmt_code(
            &gen_bridge(parse_quote! {
                mod ffi {
                    struct Foo {}

                    impl Foo {
                        pub fn from_str(s: &str) {
                            unimplemented!()
                        }
                    }
                }
            })
            .to_token_stream()
            .to_string()
        ));
    }

    #[test]
    fn method_taking_slice() {
        insta::assert_display_snapshot!(rustfmt_code(
            &gen_bridge(parse_quote! {
                mod ffi {
                    struct Foo {}

                    impl Foo {
                        pub fn from_slice(s: &[f64]) {
                            unimplemented!()
                        }
                    }
                }
            })
            .to_token_stream()
            .to_string()
        ));
    }

    #[test]
    fn method_taking_mutable_slice() {
        insta::assert_display_snapshot!(rustfmt_code(
            &gen_bridge(parse_quote! {
                mod ffi {
                    struct Foo {}

                    impl Foo {
                        pub fn fill_slice(s: &mut [f64]) {
                            unimplemented!()
                        }
                    }
                }
            })
            .to_token_stream()
            .to_string()
        ));
    }

    #[test]
    fn mod_with_enum() {
        insta::assert_display_snapshot!(rustfmt_code(
            &gen_bridge(parse_quote! {
                mod ffi {
                    enum Abc {
                        A,
                        B = 123,
                    }

                    impl Abc {
                        pub fn do_something(&self) {
                            unimplemented!()
                        }
                    }
                }
            })
            .to_token_stream()
            .to_string()
        ));
    }

    #[test]
    fn mod_with_writeable_result() {
        insta::assert_display_snapshot!(rustfmt_code(
            &gen_bridge(parse_quote! {
                mod ffi {
                    struct Foo {}

                    impl Foo {
                        pub fn to_string(&self, to: &mut DiplomatWriteable) -> Result<(), ()> {
                            unimplemented!()
                        }
                    }
                }
            })
            .to_token_stream()
            .to_string()
        ));
    }

    #[test]
    fn mod_with_rust_result() {
        insta::assert_display_snapshot!(rustfmt_code(
            &gen_bridge(parse_quote! {
                mod ffi {
                    struct Foo {}

                    impl Foo {
                        pub fn bar(&self) -> Result<(), ()> {
                            unimplemented!()
                        }
                    }
                }
            })
            .to_token_stream()
            .to_string()
        ));
    }

    #[test]
    fn multilevel_borrows() {
        insta::assert_display_snapshot!(rustfmt_code(
            &gen_bridge(parse_quote! {
                mod ffi {
                    #[diplomat::opaque]
                    struct Foo<'a>(&'a str);

                    #[diplomat::opaque]
                    struct Bar<'b, 'a: 'b>(&'b Foo<'a>);

                    struct Baz<'x, 'y> {
                        foo: &'y Foo<'x>,
                    }

                    impl<'a> Foo<'a> {
                        pub fn new(x: &'a str) -> Box<Foo<'a>> {
                            unimplemented!()
                        }

                        pub fn get_bar<'b>(&'b self) -> Box<Bar<'b, 'a>> {
                            unimplemented!()
                        }

                        pub fn get_baz<'b>(&'b self) -> Baz<'b, 'a> {
                            Bax { foo: self }
                        }
                    }
                }
            })
            .to_token_stream()
            .to_string()
        ));
    }

    #[test]
    fn self_params() {
        insta::assert_display_snapshot!(rustfmt_code(
            &gen_bridge(parse_quote! {
                mod ffi {
                    #[diplomat::opaque]
                    struct RefList<'a> {
                        data: &'a i32,
                        next: Option<Box<Self>>,
                    }

                    impl<'b> RefList<'b> {
                        pub fn extend(&mut self, other: &Self) -> Self {
                            unimplemented!()
                        }
                    }
                }
            })
            .to_token_stream()
            .to_string()
        ));
    }

    #[test]
    fn cfged_method() {
        insta::assert_display_snapshot!(rustfmt_code(
            &gen_bridge(parse_quote! {
                mod ffi {
                    struct Foo {}

                    impl Foo {
                        #[cfg(feature = "foo")]
                        pub fn bar(s: u8) {
                            unimplemented!()
                        }
                    }
                }
            })
            .to_token_stream()
            .to_string()
        ));

        insta::assert_display_snapshot!(rustfmt_code(
            &gen_bridge(parse_quote! {
                mod ffi {
                    struct Foo {}

                    #[cfg(feature = "bar")]
                    impl Foo {
                        #[cfg(feature = "foo")]
                        pub fn bar(s: u8) {
                            unimplemented!()
                        }
                    }
                }
            })
            .to_token_stream()
            .to_string()
        ));
    }

    #[test]
    fn cfgd_struct() {
        insta::assert_display_snapshot!(rustfmt_code(
            &gen_bridge(parse_quote! {
                mod ffi {
                    #[diplomat::opaque]
                    #[cfg(feature = "foo")]
                    struct Foo {}
                    #[cfg(feature = "foo")]
                    impl Foo {
                        pub fn bar(s: u8) {
                            unimplemented!()
                        }
                    }
                }
            })
            .to_token_stream()
            .to_string()
        ));
    }
}
