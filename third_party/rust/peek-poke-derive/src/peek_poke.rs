use crate::{max_size_expr, peek_from_expr, poke_into_expr};
use proc_macro2::TokenStream;
use quote::{quote, ToTokens};
use syn::{parse_quote, Data::*, DeriveInput, GenericParam, Generics};

#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum Generate {
    Both,
    Poke,
    PeekCopy,
    PeekDefault,
}

impl Generate {
    fn peek(self) -> bool {
        match self {
            Generate::Both | Generate::PeekCopy | Generate::PeekDefault => true,
            _ => false,
        }
    }
    fn poke(self) -> bool {
        match self {
            Generate::Both | Generate::Poke => true,
            _ => false,
        }
    }
}

pub fn get_peek_poke_impl(input: DeriveInput) -> TokenStream {
    get_impl(input, Generate::Both)
}

pub fn get_poke_impl(input: DeriveInput) -> TokenStream {
    get_impl(input, Generate::Poke)
}

pub fn get_peek_copy_impl(input: DeriveInput) -> TokenStream {
    get_impl(input, Generate::PeekCopy)
}

pub fn get_peek_default_impl(input: DeriveInput) -> TokenStream {
    get_impl(input, Generate::PeekDefault)
}

/// Returns `PeekPoke` trait implementation
fn get_impl(input: DeriveInput, gen: Generate) -> TokenStream {
    let name = input.ident;
    let (add_copy_trait, add_default_trait) = match &input.data {
        Enum(..) => {
            assert!(
                gen != Generate::Both,
                "This macro cannot be used on enums! use `PeekCopy` or `PeekDefault`"
            );
            (gen == Generate::PeekCopy, gen == Generate::PeekDefault)
        }
        _ => (false, false),
    };

    let (max_size, poke_into, peek_from) = match &input.data {
        Struct(ref struct_data) => (
            max_size_expr::for_struct(&struct_data),
            poke_into_expr::for_struct(&name, &struct_data),
            peek_from_expr::for_struct(&struct_data),
        ),
        Enum(ref enum_data) => (
            max_size_expr::for_enum(&enum_data),
            poke_into_expr::for_enum(&name, &enum_data),
            peek_from_expr::for_enum(&name, &enum_data, gen),
        ),

        Union(_) => panic!("This macro cannot be used on unions!"),
    };

    let poke_generics = add_trait_bound(input.generics.clone(), quote! { peek_poke::Poke });
    let (impl_generics, ty_generics, where_clause) = poke_generics.split_for_impl();

    let poke_impl = if gen.poke() {
        quote! {
        #[automatically_derived]
        #[allow(unused_qualifications)]
        #[allow(unused)]
        unsafe impl #impl_generics peek_poke::Poke for #name #ty_generics #where_clause {
            #[inline(always)]
            fn max_size() -> usize {
                #max_size
            }

            #[inline(always)]
            unsafe fn poke_into(&self, bytes: *mut u8) -> *mut u8 {
                #poke_into
            }
        }
        }
    } else {
        quote! {}
    };

    let peek_generics = add_trait_bound(input.generics.clone(), quote! { peek_poke::Peek });
    let peek_generics = add_trait_bound_if(peek_generics, quote! { Copy }, add_copy_trait);
    let peek_generics = add_trait_bound_if(peek_generics, quote! { Default }, add_default_trait);
    let peek_generics = add_where_predicate_if(peek_generics, quote! { Self: Copy }, add_copy_trait);
    let peek_generics = add_where_predicate_if(peek_generics, quote! { Self: Default }, add_default_trait);
    let (impl_generics, ty_generics, where_clause) = peek_generics.split_for_impl();

    let peek_impl = if gen.peek() {
        quote! {
            #[automatically_derived]
            #[allow(unused_qualifications)]
            #[allow(unused)]
            impl #impl_generics peek_poke::Peek for #name #ty_generics #where_clause {
                #[inline(always)]
                unsafe fn peek_from(&mut self, bytes: *const u8) -> *const u8 {
                    #peek_from
                }
            }
        }
    } else {
        quote! {}
    };

    quote! {
        #poke_impl
        #peek_impl
    }
}

// Add a bound, eg `T: PeekPoke`, for every type parameter `T`.
fn add_trait_bound(mut generics: Generics, bound: impl ToTokens) -> Generics {
    for param in &mut generics.params {
        if let GenericParam::Type(ref mut type_param) = *param {
            type_param.bounds.push(parse_quote!(#bound));
        }
    }
    generics
}

fn add_trait_bound_if(generics: Generics, bound: impl ToTokens, add: bool) -> Generics {
    if add {
        add_trait_bound(generics, bound)
    } else {
        generics
    }
}

fn add_where_predicate_if(mut generics: Generics, predicate: impl ToTokens, add: bool) -> Generics {
    if add {
        generics
            .make_where_clause()
            .predicates
            .push(parse_quote!(#predicate));
    }
    generics
}
