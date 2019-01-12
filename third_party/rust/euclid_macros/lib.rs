extern crate proc_macro;
extern crate proc_macro2;
#[macro_use]
extern crate quote;
#[macro_use]
extern crate syn;

use proc_macro::TokenStream;

mod euclid_matrix;

#[proc_macro_derive(EuclidMatrix)]
pub fn derive_euclid_matrix(input: TokenStream) -> TokenStream {
    let input = syn::parse(input).unwrap();
    euclid_matrix::derive(input).into()
}
