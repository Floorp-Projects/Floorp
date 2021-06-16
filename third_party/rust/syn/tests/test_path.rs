#[macro_use]
mod macros;

use proc_macro2::{Delimiter, Group, Ident, Punct, Spacing, Span, TokenStream, TokenTree};
use quote::quote;
use std::iter::FromIterator;
use syn::{Expr, Type};

#[test]
fn parse_interpolated_leading_component() {
    // mimics the token stream corresponding to `$mod::rest`
    let tokens = TokenStream::from_iter(vec![
        TokenTree::Group(Group::new(Delimiter::None, quote! { first })),
        TokenTree::Punct(Punct::new(':', Spacing::Joint)),
        TokenTree::Punct(Punct::new(':', Spacing::Alone)),
        TokenTree::Ident(Ident::new("rest", Span::call_site())),
    ]);

    snapshot!(tokens.clone() as Expr, @r###"
    Expr::Path {
        path: Path {
            segments: [
                PathSegment {
                    ident: "first",
                    arguments: None,
                },
                PathSegment {
                    ident: "rest",
                    arguments: None,
                },
            ],
        },
    }
    "###);

    snapshot!(tokens as Type, @r###"
    Type::Path {
        path: Path {
            segments: [
                PathSegment {
                    ident: "first",
                    arguments: None,
                },
                PathSegment {
                    ident: "rest",
                    arguments: None,
                },
            ],
        },
    }
    "###);
}
