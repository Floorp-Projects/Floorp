extern crate proc_macro;

use proc_macro::TokenStream;
use syn::{parse_macro_input, DeriveInput};

mod max_size_expr;
mod peek_from_expr;
mod peek_poke;
mod poke_into_expr;

#[proc_macro_derive(PeekPoke)]
pub fn peek_poke_macro_derive(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    peek_poke::get_peek_poke_impl(input).into()
}

#[proc_macro_derive(Poke)]
pub fn poke_macro_derive(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    peek_poke::get_poke_impl(input).into()
}

#[proc_macro_derive(PeekCopy)]
pub fn peek_copy_macro_derive(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    peek_poke::get_peek_copy_impl(input).into()
}

#[proc_macro_derive(PeekDefault)]
pub fn peek_default_macro_derive(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    peek_poke::get_peek_default_impl(input).into()
}
