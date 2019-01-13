use proc_macro2::TokenStream;
use syn::{self, DeriveInput};

type Fields = syn::punctuated::Punctuated<syn::Field, syn::token::Comma>;

fn derive_trait<F>(
    input: &DeriveInput,
    trait_name: TokenStream,
    generics: &syn::Generics,
    body: F
) -> TokenStream
where
    F: FnOnce() -> TokenStream,
{
    let struct_name = &input.ident;

    let (impl_generics, _, where_clause) = generics.split_for_impl();
    let (_, ty_generics, _) = input.generics.split_for_impl();

    let body = body();
    quote! {
        impl #impl_generics #trait_name for #struct_name #ty_generics #where_clause {
            #body
        }
    }
}

fn derive_simple_trait<F>(
    input: &DeriveInput,
    trait_name: TokenStream,
    t: &syn::TypeParam,
    body: F,
) -> TokenStream
where
    F: FnOnce() -> TokenStream,
{
    let mut generics = input.generics.clone();
    generics
        .make_where_clause()
        .predicates
        .push(parse_quote!(#t: #trait_name));
    derive_trait(input, trait_name, &generics, body)
}

fn each_field_except_unit<F>(
    fields: &Fields,
    unit: &syn::Field,
    mut field_expr: F,
) -> TokenStream
where
    F: FnMut(&syn::Ident) -> TokenStream,
{
    fields.iter().filter(|f| f.ident != unit.ident).fold(quote! {}, |body, field| {
        let name = field.ident.as_ref().unwrap();
        let expr = field_expr(name);
        quote! {
            #body
            #expr
        }
    })
}


fn derive_struct_body<F>(
    fields: &Fields,
    unit: &syn::Field,
    mut field_expr: F,
) -> TokenStream
where
    F: FnMut(&syn::Ident) -> TokenStream,
{
    let body = each_field_except_unit(fields, unit, |name| {
        let expr = field_expr(name);
        quote! {
            #name: #expr,
        }
    });

    let unit_name = unit.ident.as_ref().unwrap();
    quote! {
        Self {
            #body
            #unit_name: PhantomData,
        }
    }
}

fn clone_impl(input: &DeriveInput, fields: &Fields, unit: &syn::Field, t: &syn::TypeParam) -> TokenStream {
    derive_simple_trait(input, quote! { Clone }, t, || {
        let body = derive_struct_body(fields, unit, |name| {
            quote! { self.#name.clone() }
        });
        quote! {
            fn clone(&self) -> Self {
                #body
            }
        }
    })
}

fn copy_impl(input: &DeriveInput, t: &syn::TypeParam) -> TokenStream {
    derive_simple_trait(input, quote!{ Copy }, t, || quote! {})
}

fn eq_impl(input: &DeriveInput, t: &syn::TypeParam) -> TokenStream {
    derive_simple_trait(input, quote!{ ::core::cmp::Eq }, t, || quote! {})
}

fn partialeq_impl(input: &DeriveInput, fields: &Fields, unit: &syn::Field, t: &syn::TypeParam) -> TokenStream {
    derive_simple_trait(input, quote!{ ::core::cmp::PartialEq }, t, || {
        let body = each_field_except_unit(fields, unit, |name| {
            quote! { && self.#name == other.#name }
        });

        quote! {
            fn eq(&self, other: &Self) -> bool {
                true #body
            }
        }
    })
}

fn hash_impl(input: &DeriveInput, fields: &Fields, unit: &syn::Field, t: &syn::TypeParam) -> TokenStream {
    derive_simple_trait(input, quote!{ ::core::hash::Hash }, t, || {
        let body = each_field_except_unit(fields, unit, |name| {
            quote! { self.#name.hash(h); }
        });

        quote! {
            fn hash<H: ::core::hash::Hasher>(&self, h: &mut H) {
                #body
            }
        }
    })
}

fn serde_impl(
    input: &DeriveInput,
    fields: &Fields,
    unit: &syn::Field,
    t: &syn::TypeParam,
) -> TokenStream {
    let deserialize_impl = {
        let mut generics = input.generics.clone();
        generics.params.insert(0, parse_quote!('de));
        generics
            .make_where_clause()
            .predicates
            .push(parse_quote!(#t: ::serde::Deserialize<'de>));
        derive_trait(input, quote!{ ::serde::Deserialize<'de> }, &generics, || {
            let tuple = each_field_except_unit(fields, unit, |name| {
                quote! { #name, }
            });
            let body = derive_struct_body(fields, unit, |name| quote! { #name });
            quote! {
                fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
                where
                    D: ::serde::Deserializer<'de>,
                {
                    let (#tuple) = ::serde::Deserialize::deserialize(deserializer)?;
                    Ok(#body)
                }
            }
        })
    };

    let serialize_impl = derive_simple_trait(input, quote! { ::serde::Serialize }, t, || {
        let tuple = each_field_except_unit(fields, unit, |name| {
            quote! { &self.#name, }
        });
        quote! {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: ::serde::Serializer,
            {
                (#tuple).serialize(serializer)
            }
        }
    });

    quote! {
        #[cfg(feature = "serde")]
        #serialize_impl
        #[cfg(feature = "serde")]
        #deserialize_impl
    }
}

pub fn derive(input: DeriveInput) -> TokenStream {
    let s = match input.data {
        syn::Data::Struct(ref s) => s,
        _ => panic!("Need to derive this on a struct"),
    };

    let fields = match s.fields {
        syn::Fields::Named(ref named) => &named.named,
        _ => panic!("Need to use named fields"),
    };

    assert!(!fields.is_empty());

    let unit_field = fields.last().unwrap();
    assert_eq!(
        unit_field.value().ident.as_ref().unwrap().to_string(),
        "_unit",
        "You need to have a _unit field to derive this trait",
    );

    assert!(match unit_field.value().vis {
        syn::Visibility::Public(..) => true,
        _ => false,
    }, "Unit field should be public");

    assert!(input.attrs.iter().filter_map(|attr| attr.interpret_meta()).any(|attr| {
        match attr {
            syn::Meta::Word(..) |
            syn::Meta::NameValue(..) => false,
            syn::Meta::List(ref list) => {
                list.ident == "repr" && list.nested.iter().any(|meta| {
                    match *meta {
                        syn::NestedMeta::Meta(syn::Meta::Word(ref w)) => w == "C",
                        _ => false,
                    }
                })
            }
        }
    }), "struct should be #[repr(C)]");

    let type_param =
        input.generics.type_params().next().cloned().expect("Need a T");

    let clone = clone_impl(&input, fields, unit_field.value(), &type_param);
    let copy = copy_impl(&input, &type_param);
    let serde = serde_impl(&input, fields, unit_field.value(), &type_param);
    let eq = eq_impl(&input, &type_param);
    let partialeq = partialeq_impl(&input, fields, unit_field.value(), &type_param);
    let hash = hash_impl(&input, fields, unit_field.value(), &type_param);

    quote! {
        #clone
        #copy
        #serde
        #eq
        #partialeq
        #hash
    }
}
