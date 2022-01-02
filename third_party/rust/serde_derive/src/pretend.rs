use proc_macro2::{Span, TokenStream};
use syn::Ident;

use internals::ast::{Container, Data, Field, Style};

// Suppress dead_code warnings that would otherwise appear when using a remote
// derive. Other than this pretend code, a struct annotated with remote derive
// never has its fields referenced and an enum annotated with remote derive
// never has its variants constructed.
//
//     warning: field is never used: `i`
//      --> src/main.rs:4:20
//       |
//     4 | struct StructDef { i: i32 }
//       |                    ^^^^^^
//
//     warning: variant is never constructed: `V`
//      --> src/main.rs:8:16
//       |
//     8 | enum EnumDef { V }
//       |                ^
//
pub fn pretend_used(cont: &Container) -> TokenStream {
    let pretend_fields = pretend_fields_used(cont);
    let pretend_variants = pretend_variants_used(cont);

    quote! {
        #pretend_fields
        #pretend_variants
    }
}

// For structs with named fields, expands to:
//
//     match None::<T> {
//         Some(T { a: ref __v0, b: ref __v1 }) => {}
//         _ => {}
//     }
//
// For enums, expands to the following but only including struct variants:
//
//     match None::<T> {
//         Some(T::A { a: ref __v0 }) => {}
//         Some(T::B { b: ref __v0 }) => {}
//         _ => {}
//     }
//
// The `ref` is important in case the user has written a Drop impl on their
// type. Rust does not allow destructuring a struct or enum that has a Drop
// impl.
fn pretend_fields_used(cont: &Container) -> TokenStream {
    let type_ident = &cont.ident;
    let (_, ty_generics, _) = cont.generics.split_for_impl();

    let patterns = match &cont.data {
        Data::Enum(variants) => variants
            .iter()
            .filter_map(|variant| match variant.style {
                Style::Struct => {
                    let variant_ident = &variant.ident;
                    let pat = struct_pattern(&variant.fields);
                    Some(quote!(#type_ident::#variant_ident #pat))
                }
                _ => None,
            })
            .collect::<Vec<_>>(),
        Data::Struct(Style::Struct, fields) => {
            let pat = struct_pattern(fields);
            vec![quote!(#type_ident #pat)]
        }
        Data::Struct(_, _) => {
            return quote!();
        }
    };

    quote! {
        match _serde::__private::None::<#type_ident #ty_generics> {
            #(
                _serde::__private::Some(#patterns) => {}
            )*
            _ => {}
        }
    }
}

// Expands to one of these per enum variant:
//
//     match None {
//         Some((__v0, __v1,)) => {
//             let _ = E::V { a: __v0, b: __v1 };
//         }
//         _ => {}
//     }
//
fn pretend_variants_used(cont: &Container) -> TokenStream {
    let variants = match &cont.data {
        Data::Enum(variants) => variants,
        Data::Struct(_, _) => {
            return quote!();
        }
    };

    let type_ident = &cont.ident;
    let (_, ty_generics, _) = cont.generics.split_for_impl();
    let turbofish = ty_generics.as_turbofish();

    let cases = variants.iter().map(|variant| {
        let variant_ident = &variant.ident;
        let placeholders = &(0..variant.fields.len())
            .map(|i| Ident::new(&format!("__v{}", i), Span::call_site()))
            .collect::<Vec<_>>();

        let pat = match variant.style {
            Style::Struct => {
                let members = variant.fields.iter().map(|field| &field.member);
                quote!({ #(#members: #placeholders),* })
            }
            Style::Tuple | Style::Newtype => quote!(( #(#placeholders),* )),
            Style::Unit => quote!(),
        };

        quote! {
            match _serde::__private::None {
                _serde::__private::Some((#(#placeholders,)*)) => {
                    let _ = #type_ident::#variant_ident #turbofish #pat;
                }
                _ => {}
            }
        }
    });

    quote!(#(#cases)*)
}

fn struct_pattern(fields: &[Field]) -> TokenStream {
    let members = fields.iter().map(|field| &field.member);
    let placeholders =
        (0..fields.len()).map(|i| Ident::new(&format!("__v{}", i), Span::call_site()));
    quote!({ #(#members: ref #placeholders),* })
}
