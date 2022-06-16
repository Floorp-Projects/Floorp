extern crate proc_macro;

use proc_macro2::{Span, TokenStream};
use quote::quote;
use syn::*;

static ARBITRARY_LIFETIME_NAME: &str = "'arbitrary";

#[proc_macro_derive(Arbitrary)]
pub fn derive_arbitrary(tokens: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input = syn::parse_macro_input!(tokens as syn::DeriveInput);
    let (lifetime_without_bounds, lifetime_with_bounds) =
        build_arbitrary_lifetime(input.generics.clone());

    let recursive_count = syn::Ident::new(
        &format!("RECURSIVE_COUNT_{}", input.ident),
        Span::call_site(),
    );

    let arbitrary_method =
        gen_arbitrary_method(&input, lifetime_without_bounds.clone(), &recursive_count);
    let size_hint_method = gen_size_hint_method(&input);
    let name = input.ident;
    // Add a bound `T: Arbitrary` to every type parameter T.
    let generics = add_trait_bounds(input.generics, lifetime_without_bounds.clone());

    // Build ImplGeneric with a lifetime (https://github.com/dtolnay/syn/issues/90)
    let mut generics_with_lifetime = generics.clone();
    generics_with_lifetime
        .params
        .push(GenericParam::Lifetime(lifetime_with_bounds));
    let (impl_generics, _, _) = generics_with_lifetime.split_for_impl();

    // Build TypeGenerics and WhereClause without a lifetime
    let (_, ty_generics, where_clause) = generics.split_for_impl();

    (quote! {
        thread_local! {
            static #recursive_count: std::cell::Cell<u32> = std::cell::Cell::new(0);
        }

        impl #impl_generics arbitrary::Arbitrary<#lifetime_without_bounds> for #name #ty_generics #where_clause {
            #arbitrary_method
            #size_hint_method
        }
    })
    .into()
}

// Returns: (lifetime without bounds, lifetime with bounds)
// Example: ("'arbitrary", "'arbitrary: 'a + 'b")
fn build_arbitrary_lifetime(generics: Generics) -> (LifetimeDef, LifetimeDef) {
    let lifetime_without_bounds =
        LifetimeDef::new(Lifetime::new(ARBITRARY_LIFETIME_NAME, Span::call_site()));
    let mut lifetime_with_bounds = lifetime_without_bounds.clone();

    for param in generics.params.iter() {
        if let GenericParam::Lifetime(lifetime_def) = param {
            lifetime_with_bounds
                .bounds
                .push(lifetime_def.lifetime.clone());
        }
    }

    (lifetime_without_bounds, lifetime_with_bounds)
}

// Add a bound `T: Arbitrary` to every type parameter T.
fn add_trait_bounds(mut generics: Generics, lifetime: LifetimeDef) -> Generics {
    for param in generics.params.iter_mut() {
        if let GenericParam::Type(type_param) = param {
            type_param
                .bounds
                .push(parse_quote!(arbitrary::Arbitrary<#lifetime>));
        }
    }
    generics
}

fn with_recursive_count_guard(
    recursive_count: &syn::Ident,
    expr: impl quote::ToTokens,
) -> impl quote::ToTokens {
    quote! {
        #recursive_count.with(|count| {
            if count.get() > 0 && u.is_empty() {
                return Err(arbitrary::Error::NotEnoughData);
            }

            count.set(count.get() + 1);
            let result = { #expr };
            count.set(count.get() - 1);

            result
        })
    }
}

fn gen_arbitrary_method(
    input: &DeriveInput,
    lifetime: LifetimeDef,
    recursive_count: &syn::Ident,
) -> TokenStream {
    let ident = &input.ident;

    let arbitrary_structlike = |fields| {
        let arbitrary = construct(fields, |_, _| quote!(arbitrary::Arbitrary::arbitrary(u)?));
        let body = with_recursive_count_guard(recursive_count, quote! { Ok(#ident #arbitrary) });

        let arbitrary_take_rest = construct_take_rest(fields);
        let take_rest_body =
            with_recursive_count_guard(recursive_count, quote! { Ok(#ident #arbitrary_take_rest) });

        quote! {
            fn arbitrary(u: &mut arbitrary::Unstructured<#lifetime>) -> arbitrary::Result<Self> {
                #body
            }

            fn arbitrary_take_rest(mut u: arbitrary::Unstructured<#lifetime>) -> arbitrary::Result<Self> {
                #take_rest_body
            }
        }
    };

    match &input.data {
        Data::Struct(data) => arbitrary_structlike(&data.fields),
        Data::Union(data) => arbitrary_structlike(&Fields::Named(data.fields.clone())),
        Data::Enum(data) => {
            let variants = data.variants.iter().enumerate().map(|(i, variant)| {
                let idx = i as u64;
                let ctor = construct(&variant.fields, |_, _| {
                    quote!(arbitrary::Arbitrary::arbitrary(u)?)
                });
                let variant_name = &variant.ident;
                quote! { #idx => #ident::#variant_name #ctor }
            });
            let variants_take_rest = data.variants.iter().enumerate().map(|(i, variant)| {
                let idx = i as u64;
                let ctor = construct_take_rest(&variant.fields);
                let variant_name = &variant.ident;
                quote! { #idx => #ident::#variant_name #ctor }
            });
            let count = data.variants.len() as u64;

            let arbitrary = with_recursive_count_guard(
                recursive_count,
                quote! {
                    // Use a multiply + shift to generate a ranged random number
                    // with slight bias. For details, see:
                    // https://lemire.me/blog/2016/06/30/fast-random-shuffling
                    Ok(match (u64::from(<u32 as arbitrary::Arbitrary>::arbitrary(u)?) * #count) >> 32 {
                        #(#variants,)*
                        _ => unreachable!()
                    })
                },
            );

            let arbitrary_take_rest = with_recursive_count_guard(
                recursive_count,
                quote! {
                    // Use a multiply + shift to generate a ranged random number
                    // with slight bias. For details, see:
                    // https://lemire.me/blog/2016/06/30/fast-random-shuffling
                    Ok(match (u64::from(<u32 as arbitrary::Arbitrary>::arbitrary(&mut u)?) * #count) >> 32 {
                        #(#variants_take_rest,)*
                        _ => unreachable!()
                    })
                },
            );

            quote! {
                fn arbitrary(u: &mut arbitrary::Unstructured<#lifetime>) -> arbitrary::Result<Self> {
                    #arbitrary
                }

                fn arbitrary_take_rest(mut u: arbitrary::Unstructured<#lifetime>) -> arbitrary::Result<Self> {
                    #arbitrary_take_rest
                }
            }
        }
    }
}

fn construct(fields: &Fields, ctor: impl Fn(usize, &Field) -> TokenStream) -> TokenStream {
    match fields {
        Fields::Named(names) => {
            let names = names.named.iter().enumerate().map(|(i, f)| {
                let name = f.ident.as_ref().unwrap();
                let ctor = ctor(i, f);
                quote! { #name: #ctor }
            });
            quote! { { #(#names,)* } }
        }
        Fields::Unnamed(names) => {
            let names = names.unnamed.iter().enumerate().map(|(i, f)| {
                let ctor = ctor(i, f);
                quote! { #ctor }
            });
            quote! { ( #(#names),* ) }
        }
        Fields::Unit => quote!(),
    }
}

fn construct_take_rest(fields: &Fields) -> TokenStream {
    construct(fields, |idx, _| {
        if idx + 1 == fields.len() {
            quote! { arbitrary::Arbitrary::arbitrary_take_rest(u)? }
        } else {
            quote! { arbitrary::Arbitrary::arbitrary(&mut u)? }
        }
    })
}

fn gen_size_hint_method(input: &DeriveInput) -> TokenStream {
    let size_hint_fields = |fields: &Fields| {
        let tys = fields.iter().map(|f| &f.ty);
        quote! {
            arbitrary::size_hint::and_all(&[
                #( <#tys as arbitrary::Arbitrary>::size_hint(depth) ),*
            ])
        }
    };
    let size_hint_structlike = |fields: &Fields| {
        let hint = size_hint_fields(fields);
        quote! {
            #[inline]
            fn size_hint(depth: usize) -> (usize, Option<usize>) {
                arbitrary::size_hint::recursion_guard(depth, |depth| #hint)
            }
        }
    };
    match &input.data {
        Data::Struct(data) => size_hint_structlike(&data.fields),
        Data::Union(data) => size_hint_structlike(&Fields::Named(data.fields.clone())),
        Data::Enum(data) => {
            let variants = data.variants.iter().map(|v| size_hint_fields(&v.fields));
            quote! {
                #[inline]
                fn size_hint(depth: usize) -> (usize, Option<usize>) {
                    arbitrary::size_hint::and(
                        <u32 as arbitrary::Arbitrary>::size_hint(depth),
                        arbitrary::size_hint::recursion_guard(depth, |depth| {
                            arbitrary::size_hint::or_all(&[ #( #variants ),* ])
                        }),
                    )
                }
            }
        }
    }
}
