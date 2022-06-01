use proc_macro::{Delimiter, Group, Ident, Literal, Punct, Spacing, Span, TokenStream, TokenTree};
use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};
use std::iter::FromIterator;

pub fn wrap(output: TokenStream) -> TokenStream {
    let mut hasher = DefaultHasher::default();
    output.to_string().hash(&mut hasher);
    let mangled_name = format!("_paste_{}", hasher.finish());
    let ident = Ident::new(&mangled_name, Span::call_site());

    // #[derive(paste::EnumHack)]
    // enum #ident {
    //     Value = (stringify! {
    //         #output
    //     }, 0).1,
    // }
    TokenStream::from_iter(vec![
        TokenTree::Punct(Punct::new('#', Spacing::Alone)),
        TokenTree::Group(Group::new(
            Delimiter::Bracket,
            TokenStream::from_iter(vec![
                TokenTree::Ident(Ident::new("derive", Span::call_site())),
                TokenTree::Group(Group::new(
                    Delimiter::Parenthesis,
                    TokenStream::from_iter(vec![
                        TokenTree::Ident(Ident::new("paste", Span::call_site())),
                        TokenTree::Punct(Punct::new(':', Spacing::Joint)),
                        TokenTree::Punct(Punct::new(':', Spacing::Alone)),
                        TokenTree::Ident(Ident::new("EnumHack", Span::call_site())),
                    ]),
                )),
            ]),
        )),
        TokenTree::Ident(Ident::new("enum", Span::call_site())),
        TokenTree::Ident(ident),
        TokenTree::Group(Group::new(
            Delimiter::Brace,
            TokenStream::from_iter(vec![
                TokenTree::Ident(Ident::new("Value", Span::call_site())),
                TokenTree::Punct(Punct::new('=', Spacing::Alone)),
                TokenTree::Group(Group::new(
                    Delimiter::Parenthesis,
                    TokenStream::from_iter(vec![
                        TokenTree::Ident(Ident::new("stringify", Span::call_site())),
                        TokenTree::Punct(Punct::new('!', Spacing::Alone)),
                        TokenTree::Group(Group::new(Delimiter::Brace, output)),
                        TokenTree::Punct(Punct::new(',', Spacing::Alone)),
                        TokenTree::Literal(Literal::usize_unsuffixed(0)),
                    ]),
                )),
                TokenTree::Punct(Punct::new('.', Spacing::Alone)),
                TokenTree::Literal(Literal::usize_unsuffixed(1)),
                TokenTree::Punct(Punct::new(',', Spacing::Alone)),
            ]),
        )),
    ])
}

pub fn extract(input: TokenStream) -> TokenStream {
    let mut tokens = input.into_iter();
    let _ = tokens.next().expect("enum");
    let _ = tokens.next().expect("#ident");
    let mut braces = match tokens.next().expect("{...}") {
        TokenTree::Group(group) => group.stream().into_iter(),
        _ => unreachable!("{...}"),
    };
    let _ = braces.next().expect("Value");
    let _ = braces.next().expect("=");
    let mut parens = match braces.next().expect("(...)") {
        TokenTree::Group(group) => group.stream().into_iter(),
        _ => unreachable!("(...)"),
    };
    let _ = parens.next().expect("stringify");
    let _ = parens.next().expect("!");
    let token_stream = match parens.next().expect("{...}") {
        TokenTree::Group(group) => group.stream(),
        _ => unreachable!("{...}"),
    };
    let _ = parens.next().expect(",");
    let _ = parens.next().expect("0");
    let _ = braces.next().expect(".");
    let _ = braces.next().expect("1");
    let _ = braces.next().expect(",");
    token_stream
}
