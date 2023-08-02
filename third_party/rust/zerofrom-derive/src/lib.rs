// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Custom derives for `ZeroFrom` from the `zerofrom` crate.

// https://github.com/unicode-org/icu4x/blob/main/docs/process/boilerplate.md#library-annotations
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        clippy::panic,
        clippy::exhaustive_structs,
        clippy::exhaustive_enums,
        missing_debug_implementations,
    )
)]

use proc_macro::TokenStream;
use proc_macro2::{Span, TokenStream as TokenStream2};
use quote::quote;
use syn::spanned::Spanned;
use syn::{parse_macro_input, parse_quote, DeriveInput, Ident, Lifetime, Type, WherePredicate};
use synstructure::Structure;

mod visitor;

/// Custom derive for `zerofrom::ZeroFrom`,
///
/// This implements `ZeroFrom<Ty> for Ty` for types
/// without a lifetime parameter, and `ZeroFrom<Ty<'data>> for Ty<'static>`
/// for types with a lifetime parameter.
///
/// Apply the `#[zerofrom(clone)]` attribute to a field if it doesn't implement
/// Copy or ZeroFrom; this data will be cloned when the struct is zero_from'ed.
#[proc_macro_derive(ZeroFrom, attributes(zerofrom))]
pub fn zf_derive(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    TokenStream::from(zf_derive_impl(&input))
}

fn has_clone_attr(attrs: &[syn::Attribute]) -> bool {
    attrs.iter().any(|a| {
        if let Ok(i) = a.parse_args::<Ident>() {
            if i == "clone" {
                return true;
            }
        }
        false
    })
}

fn zf_derive_impl(input: &DeriveInput) -> TokenStream2 {
    let tybounds = input
        .generics
        .type_params()
        .map(|ty| {
            // Strip out param defaults, we don't need them in the impl
            let mut ty = ty.clone();
            ty.eq_token = None;
            ty.default = None;
            ty
        })
        .collect::<Vec<_>>();
    let typarams = tybounds
        .iter()
        .map(|ty| ty.ident.clone())
        .collect::<Vec<_>>();
    let lts = input.generics.lifetimes().count();
    let name = &input.ident;
    let structure = Structure::new(input);

    if lts == 0 {
        let has_clone = structure
            .variants()
            .iter()
            .flat_map(|variant| variant.bindings().iter())
            .any(|binding| has_clone_attr(&binding.ast().attrs));
        let (clone, clone_trait) = if has_clone {
            (quote!(this.clone()), quote!(Clone))
        } else {
            (quote!(*this), quote!(Copy))
        };
        let bounds: Vec<WherePredicate> = typarams
            .iter()
            .map(|ty| parse_quote!(#ty: #clone_trait + 'static))
            .collect();
        quote! {
            impl<'zf, #(#tybounds),*> zerofrom::ZeroFrom<'zf, #name<#(#typarams),*>> for #name<#(#typarams),*> where #(#bounds),* {
                fn zero_from(this: &'zf Self) -> Self {
                    #clone
                }
            }
        }
    } else {
        if lts != 1 {
            return syn::Error::new(
                input.generics.span(),
                "derive(ZeroFrom) cannot have multiple lifetime parameters",
            )
            .to_compile_error();
        }

        let generics_env = typarams.iter().cloned().collect();

        let mut zf_bounds: Vec<WherePredicate> = vec![];
        let body = structure.each_variant(|vi| {
            vi.construct(|f, i| {
                let binding = format!("__binding_{i}");
                let field = Ident::new(&binding, Span::call_site());

                if has_clone_attr(&f.attrs) {
                    quote! {
                        #field.clone()
                    }
                } else {
                    let fty = replace_lifetime(&f.ty, custom_lt("'zf"));
                    let lifetime_ty = replace_lifetime(&f.ty, custom_lt("'zf_inner"));

                    let (has_ty, has_lt) = visitor::check_type_for_parameters(&f.ty, &generics_env);
                    if has_ty {
                        // For types without type parameters, the compiler can figure out that the field implements
                        // ZeroFrom on its own. However, if there are type parameters, there may be complex preconditions
                        // to `FieldTy: ZeroFrom` that need to be satisfied. We get them to be satisfied by requiring
                        // `FieldTy<'zf>: ZeroFrom<'zf, FieldTy<'zf_inner>>`
                        if has_lt {
                            zf_bounds
                                .push(parse_quote!(#fty: zerofrom::ZeroFrom<'zf, #lifetime_ty>));
                        } else {
                            zf_bounds.push(parse_quote!(#fty: zerofrom::ZeroFrom<'zf, #fty>));
                        }
                    }
                    if has_ty || has_lt {
                        // By doing this we essentially require ZF to be implemented
                        // on all fields
                        quote! {
                            <#fty as zerofrom::ZeroFrom<'zf, #lifetime_ty>>::zero_from(#field)
                        }
                    } else {
                        // No lifetimes, so we can just copy
                        quote! { *#field }
                    }
                }
            })
        });

        quote! {
            impl<'zf, 'zf_inner, #(#tybounds),*> zerofrom::ZeroFrom<'zf, #name<'zf_inner, #(#typarams),*>> for #name<'zf, #(#typarams),*>
                where
                #(#zf_bounds,)* {
                fn zero_from(this: &'zf #name<'zf_inner, #(#typarams),*>) -> Self {
                    match *this { #body }
                }
            }
        }
    }
}

fn custom_lt(s: &str) -> Lifetime {
    Lifetime::new(s, Span::call_site())
}

fn replace_lifetime(x: &Type, lt: Lifetime) -> Type {
    use syn::fold::Fold;
    struct ReplaceLifetime(Lifetime);

    impl Fold for ReplaceLifetime {
        fn fold_lifetime(&mut self, _: Lifetime) -> Lifetime {
            self.0.clone()
        }
    }
    ReplaceLifetime(lt).fold_type(x.clone())
}
