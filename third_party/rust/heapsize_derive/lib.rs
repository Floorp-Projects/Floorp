/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[cfg(not(test))] extern crate proc_macro;
#[macro_use] extern crate quote;
extern crate syn;
extern crate synstructure;

#[cfg(not(test))]
#[proc_macro_derive(HeapSizeOf, attributes(ignore_heap_size_of))]
pub fn expand_token_stream(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    expand_string(&input.to_string()).parse().unwrap()
}

fn expand_string(input: &str) -> String {
    let mut type_ = syn::parse_macro_input(input).unwrap();

    let style = synstructure::BindStyle::Ref.into();
    let match_body = synstructure::each_field(&mut type_, &style, |binding| {
        let ignore = binding.field.attrs.iter().any(|attr| match attr.value {
            syn::MetaItem::Word(ref ident) |
            syn::MetaItem::List(ref ident, _) if ident == "ignore_heap_size_of" => {
                panic!("#[ignore_heap_size_of] should have an explanation, \
                        e.g. #[ignore_heap_size_of = \"because reasons\"]");
            }
            syn::MetaItem::NameValue(ref ident, _) if ident == "ignore_heap_size_of" => {
                true
            }
            _ => false,
        });
        if ignore {
            None
        } else if let syn::Ty::Array(..) = binding.field.ty {
            Some(quote! {
                for item in #binding.iter() {
                    sum += ::heapsize::HeapSizeOf::heap_size_of_children(item);
                }
            })
        } else {
            Some(quote! {
                sum += ::heapsize::HeapSizeOf::heap_size_of_children(#binding);
            })
        }
    });

    let name = &type_.ident;
    let (impl_generics, ty_generics, where_clause) = type_.generics.split_for_impl();
    let mut where_clause = where_clause.clone();
    for param in &type_.generics.ty_params {
        where_clause.predicates.push(syn::WherePredicate::BoundPredicate(syn::WhereBoundPredicate {
            bound_lifetimes: Vec::new(),
            bounded_ty: syn::Ty::Path(None, param.ident.clone().into()),
            bounds: vec![syn::TyParamBound::Trait(
                syn::PolyTraitRef {
                    bound_lifetimes: Vec::new(),
                    trait_ref: syn::parse_path("::heapsize::HeapSizeOf").unwrap(),
                },
                syn::TraitBoundModifier::None
            )],
        }))
    }

    let tokens = quote! {
        impl #impl_generics ::heapsize::HeapSizeOf for #name #ty_generics #where_clause {
            #[inline]
            #[allow(unused_variables, unused_mut, unreachable_code)]
            fn heap_size_of_children(&self) -> usize {
                let mut sum = 0;
                match *self {
                    #match_body
                }
                sum
            }
        }
    };

    tokens.to_string()
}

#[test]
fn test_struct() {
    let mut source = "struct Foo<T> { bar: Bar, baz: T, #[ignore_heap_size_of = \"\"] z: Arc<T> }";
    let mut expanded = expand_string(source);
    let mut no_space = expanded.replace(" ", "");
    macro_rules! match_count {
        ($e: expr, $count: expr) => {
            assert_eq!(no_space.matches(&$e.replace(" ", "")).count(), $count,
                       "counting occurences of {:?} in {:?} (whitespace-insensitive)",
                       $e, expanded)
        }
    }
    match_count!("struct", 0);
    match_count!("ignore_heap_size_of", 0);
    match_count!("impl<T> ::heapsize::HeapSizeOf for Foo<T> where T: ::heapsize::HeapSizeOf {", 1);
    match_count!("sum += ::heapsize::HeapSizeOf::heap_size_of_children(", 2);

    source = "struct Bar([Baz; 3]);";
    expanded = expand_string(source);
    no_space = expanded.replace(" ", "");
    match_count!("for item in", 1);
}

#[should_panic(expected = "should have an explanation")]
#[test]
fn test_no_reason() {
    expand_string("struct A { #[ignore_heap_size_of] b: C }");
}
