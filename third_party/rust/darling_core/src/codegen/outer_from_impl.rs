use syn::{Generics, Path, TraitBound, TraitBoundModifier, TypeParamBound, GenericParam};
use quote::{Tokens, ToTokens};

use codegen::TraitImpl;

/// Wrapper for "outer From" traits, such as `FromDeriveInput`, `FromVariant`, and `FromField`.
pub trait OuterFromImpl<'a> {
    /// Gets the path of the trait being implemented.
    fn trait_path(&self) -> Path;

    fn base(&'a self) -> &'a TraitImpl<'a>;

    fn trait_bound(&self) -> Path {
        self.trait_path()
    }

    fn wrap<T: ToTokens>(&'a self, body: T, tokens: &mut Tokens) {
        let base = self.base();
        let trayt = self.trait_path();
        let ty_ident = base.ident;
        let generics = compute_impl_bounds(self.trait_bound(), base.generics.clone());
        let (impl_generics, ty_generics, where_clause) = generics.split_for_impl();

        tokens.append_all(quote!(
            impl #impl_generics #trayt for #ty_ident #ty_generics
                #where_clause
            {
                #body
            }
        ));
    }
}

fn compute_impl_bounds(bound: Path, mut generics: Generics) -> Generics {
    if generics.params.is_empty() {
        return generics;
    }

    let added_bound = TypeParamBound::Trait(TraitBound {
            paren_token: None,
            modifier: TraitBoundModifier::None,
            lifetimes: None,
            path: bound,
        });

    for mut param in generics.params.iter_mut() {
        if let &mut GenericParam::Type(ref mut typ) = param {
            typ.bounds.push(added_bound.clone());
        }
    }

    generics
}
