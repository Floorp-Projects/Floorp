use crate::utils::{AttrParams, DeriveType, State};
use convert_case::{Case, Casing};
use proc_macro2::TokenStream;
use quote::{format_ident, quote};
use syn::{DeriveInput, Fields, Ident, Result};

pub fn expand(input: &DeriveInput, trait_name: &'static str) -> Result<TokenStream> {
    let state = State::with_attr_params(
        input,
        trait_name,
        quote!(),
        String::from("unwrap"),
        AttrParams {
            enum_: vec!["ignore"],
            variant: vec!["ignore"],
            struct_: vec!["ignore"],
            field: vec!["ignore"],
        },
    )?;
    assert!(
        state.derive_type == DeriveType::Enum,
        "Unwrap can only be derived for enums"
    );

    let enum_name = &input.ident;
    let (imp_generics, type_generics, where_clause) = input.generics.split_for_impl();

    let mut funcs = vec![];
    for variant_state in state.enabled_variant_data().variant_states {
        let variant = variant_state.variant.unwrap();
        let fn_name = Ident::new(
            &format_ident!("unwrap_{}", variant.ident)
                .to_string()
                .to_case(Case::Snake),
            variant.ident.span(),
        );
        let variant_ident = &variant.ident;

        let (data_pattern, ret_value, ret_type) = match variant.fields {
            Fields::Named(_) => panic!("cannot unwrap anonymous records"),
            Fields::Unnamed(ref fields) => {
                let data_pattern =
                    (0..fields.unnamed.len()).fold(vec![], |mut a, n| {
                        a.push(format_ident!("field_{}", n));
                        a
                    });
                let ret_type = &fields.unnamed;
                (
                    quote! { (#(#data_pattern),*) },
                    quote! { (#(#data_pattern),*) },
                    quote! { (#ret_type) },
                )
            }
            Fields::Unit => (quote! {}, quote! { () }, quote! { () }),
        };

        let other_arms = state.variant_states.iter().map(|variant| {
            variant.variant.unwrap()
        }).filter(|variant| {
            &variant.ident != variant_ident
        }).map(|variant| {
            let data_pattern = match variant.fields {
                Fields::Named(_) => quote! { {..} },
                Fields::Unnamed(_) => quote! { (..) },
                Fields::Unit => quote! {},
            };
            let variant_ident = &variant.ident;
            quote! { #enum_name :: #variant_ident #data_pattern =>
                      panic!(concat!("called `", stringify!(#enum_name), "::", stringify!(#fn_name),
                                     "()` on a `", stringify!(#variant_ident), "` value"))
            }
        });

        // The `track-caller` feature is set by our build script based
        // on rustc version detection, as `#[track_caller]` was
        // stabilized in a later version (1.46) of Rust than our MSRV (1.36).
        let track_caller = if cfg!(feature = "track-caller") {
            quote! { #[track_caller] }
        } else {
            quote! {}
        };
        let func = quote! {
            #track_caller
            pub fn #fn_name(self) -> #ret_type {
                match self {
                    #enum_name ::#variant_ident #data_pattern => #ret_value,
                    #(#other_arms),*
                }
            }
        };
        funcs.push(func);
    }

    let imp = quote! {
        impl #imp_generics #enum_name #type_generics #where_clause{
            #(#funcs)*
        }
    };

    Ok(imp)
}
