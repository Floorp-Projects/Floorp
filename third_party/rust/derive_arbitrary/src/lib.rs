extern crate proc_macro;

use proc_macro2::{Span, TokenStream};
use quote::quote;
use syn::*;

mod field_attributes;
use field_attributes::{determine_field_constructor, FieldConstructor};

static ARBITRARY_LIFETIME_NAME: &str = "'arbitrary";

#[proc_macro_derive(Arbitrary, attributes(arbitrary))]
pub fn derive_arbitrary(tokens: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input = syn::parse_macro_input!(tokens as syn::DeriveInput);
    expand_derive_arbitrary(input)
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

fn expand_derive_arbitrary(input: syn::DeriveInput) -> Result<TokenStream> {
    let (lifetime_without_bounds, lifetime_with_bounds) =
        build_arbitrary_lifetime(input.generics.clone());

    let recursive_count = syn::Ident::new(
        &format!("RECURSIVE_COUNT_{}", input.ident),
        Span::call_site(),
    );

    let arbitrary_method =
        gen_arbitrary_method(&input, lifetime_without_bounds.clone(), &recursive_count)?;
    let size_hint_method = gen_size_hint_method(&input)?;
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

    Ok(quote! {
        const _: () = {
            std::thread_local! {
                #[allow(non_upper_case_globals)]
                static #recursive_count: std::cell::Cell<u32> = std::cell::Cell::new(0);
            }

            impl #impl_generics arbitrary::Arbitrary<#lifetime_without_bounds> for #name #ty_generics #where_clause {
                #arbitrary_method
                #size_hint_method
            }
        };
    })
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
        let guard_against_recursion = u.is_empty();
        if guard_against_recursion {
            #recursive_count.with(|count| {
                if count.get() > 0 {
                    return Err(arbitrary::Error::NotEnoughData);
                }
                count.set(count.get() + 1);
                Ok(())
            })?;
        }

        let result = (|| { #expr })();

        if guard_against_recursion {
            #recursive_count.with(|count| {
                count.set(count.get() - 1);
            });
        }

        result
    }
}

fn gen_arbitrary_method(
    input: &DeriveInput,
    lifetime: LifetimeDef,
    recursive_count: &syn::Ident,
) -> Result<TokenStream> {
    fn arbitrary_structlike(
        fields: &Fields,
        ident: &syn::Ident,
        lifetime: LifetimeDef,
        recursive_count: &syn::Ident,
    ) -> Result<TokenStream> {
        let arbitrary = construct(fields, |_idx, field| gen_constructor_for_field(field))?;
        let body = with_recursive_count_guard(recursive_count, quote! { Ok(#ident #arbitrary) });

        let arbitrary_take_rest = construct_take_rest(fields)?;
        let take_rest_body =
            with_recursive_count_guard(recursive_count, quote! { Ok(#ident #arbitrary_take_rest) });

        Ok(quote! {
            fn arbitrary(u: &mut arbitrary::Unstructured<#lifetime>) -> arbitrary::Result<Self> {
                #body
            }

            fn arbitrary_take_rest(mut u: arbitrary::Unstructured<#lifetime>) -> arbitrary::Result<Self> {
                #take_rest_body
            }
        })
    }

    let ident = &input.ident;
    let output = match &input.data {
        Data::Struct(data) => arbitrary_structlike(&data.fields, ident, lifetime, recursive_count)?,
        Data::Union(data) => arbitrary_structlike(
            &Fields::Named(data.fields.clone()),
            ident,
            lifetime,
            recursive_count,
        )?,
        Data::Enum(data) => {
            let variants: Vec<TokenStream> = data
                .variants
                .iter()
                .enumerate()
                .map(|(i, variant)| {
                    let idx = i as u64;
                    let variant_name = &variant.ident;
                    construct(&variant.fields, |_, field| gen_constructor_for_field(field))
                        .map(|ctor| quote! { #idx => #ident::#variant_name #ctor })
                })
                .collect::<Result<_>>()?;

            let variants_take_rest: Vec<TokenStream> = data
                .variants
                .iter()
                .enumerate()
                .map(|(i, variant)| {
                    let idx = i as u64;
                    let variant_name = &variant.ident;
                    construct_take_rest(&variant.fields)
                        .map(|ctor| quote! { #idx => #ident::#variant_name #ctor })
                })
                .collect::<Result<_>>()?;

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
    };
    Ok(output)
}

fn construct(
    fields: &Fields,
    ctor: impl Fn(usize, &Field) -> Result<TokenStream>,
) -> Result<TokenStream> {
    let output = match fields {
        Fields::Named(names) => {
            let names: Vec<TokenStream> = names
                .named
                .iter()
                .enumerate()
                .map(|(i, f)| {
                    let name = f.ident.as_ref().unwrap();
                    ctor(i, f).map(|ctor| quote! { #name: #ctor })
                })
                .collect::<Result<_>>()?;
            quote! { { #(#names,)* } }
        }
        Fields::Unnamed(names) => {
            let names: Vec<TokenStream> = names
                .unnamed
                .iter()
                .enumerate()
                .map(|(i, f)| ctor(i, f).map(|ctor| quote! { #ctor }))
                .collect::<Result<_>>()?;
            quote! { ( #(#names),* ) }
        }
        Fields::Unit => quote!(),
    };
    Ok(output)
}

fn construct_take_rest(fields: &Fields) -> Result<TokenStream> {
    construct(fields, |idx, field| {
        determine_field_constructor(field).map(|field_constructor| match field_constructor {
            FieldConstructor::Default => quote!(Default::default()),
            FieldConstructor::Arbitrary => {
                if idx + 1 == fields.len() {
                    quote! { arbitrary::Arbitrary::arbitrary_take_rest(u)? }
                } else {
                    quote! { arbitrary::Arbitrary::arbitrary(&mut u)? }
                }
            }
            FieldConstructor::With(function_or_closure) => quote!((#function_or_closure)(&mut u)?),
            FieldConstructor::Value(value) => quote!(#value),
        })
    })
}

fn gen_size_hint_method(input: &DeriveInput) -> Result<TokenStream> {
    let size_hint_fields = |fields: &Fields| {
        fields
            .iter()
            .map(|f| {
                let ty = &f.ty;
                determine_field_constructor(f).map(|field_constructor| {
                    match field_constructor {
                        FieldConstructor::Default | FieldConstructor::Value(_) => {
                            quote!((0, Some(0)))
                        }
                        FieldConstructor::Arbitrary => {
                            quote! { <#ty as arbitrary::Arbitrary>::size_hint(depth) }
                        }

                        // Note that in this case it's hard to determine what size_hint must be, so size_of::<T>() is
                        // just an educated guess, although it's gonna be inaccurate for dynamically
                        // allocated types (Vec, HashMap, etc.).
                        FieldConstructor::With(_) => {
                            quote! { (::core::mem::size_of::<#ty>(), None) }
                        }
                    }
                })
            })
            .collect::<Result<Vec<TokenStream>>>()
            .map(|hints| {
                quote! {
                    arbitrary::size_hint::and_all(&[
                        #( #hints ),*
                    ])
                }
            })
    };
    let size_hint_structlike = |fields: &Fields| {
        size_hint_fields(fields).map(|hint| {
            quote! {
                #[inline]
                fn size_hint(depth: usize) -> (usize, Option<usize>) {
                    arbitrary::size_hint::recursion_guard(depth, |depth| #hint)
                }
            }
        })
    };
    match &input.data {
        Data::Struct(data) => size_hint_structlike(&data.fields),
        Data::Union(data) => size_hint_structlike(&Fields::Named(data.fields.clone())),
        Data::Enum(data) => data
            .variants
            .iter()
            .map(|v| size_hint_fields(&v.fields))
            .collect::<Result<Vec<TokenStream>>>()
            .map(|variants| {
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
            }),
    }
}

fn gen_constructor_for_field(field: &Field) -> Result<TokenStream> {
    let ctor = match determine_field_constructor(field)? {
        FieldConstructor::Default => quote!(Default::default()),
        FieldConstructor::Arbitrary => quote!(arbitrary::Arbitrary::arbitrary(u)?),
        FieldConstructor::With(function_or_closure) => quote!((#function_or_closure)(u)?),
        FieldConstructor::Value(value) => quote!(#value),
    };
    Ok(ctor)
}
