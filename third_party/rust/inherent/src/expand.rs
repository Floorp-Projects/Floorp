use crate::parse::TraitImpl;
use proc_macro2::{Span, TokenStream, TokenTree};
use quote::{quote, quote_spanned};
use syn::{FnArg, Ident, ImplItem, ImplItemMethod, Item, Pat, Path, Stmt, Visibility};

pub fn inherent(mut input: TraitImpl) -> TokenStream {
    let generics = &input.generics;
    let where_clause = &input.generics.where_clause;
    let trait_ = &input.trait_;
    let ty = &input.self_ty;

    let fwd_methods: Vec<_> = input
        .items
        .iter()
        .filter_map(|item| match item {
            ImplItem::Method(method) => Some(fwd_method(trait_, method)),
            _ => None,
        })
        .collect();

    input.items = input
        .items
        .into_iter()
        .filter_map(|item| match item {
            ImplItem::Method(mut method) => {
                if inherit_default_implementation(&method) {
                    None
                } else {
                    method.vis = Visibility::Inherited;
                    Some(ImplItem::Method(method))
                }
            }
            item => Some(item),
        })
        .collect();

    quote! {
        impl #generics #ty #where_clause {
            #(#fwd_methods)*
        }

        #input
    }
}

fn fwd_method(trait_: &Path, method: &ImplItemMethod) -> TokenStream {
    let attrs = &method.attrs;
    let vis = &method.vis;
    let constness = &method.sig.constness;
    let asyncness = &method.sig.asyncness;
    let unsafety = &method.sig.unsafety;
    let abi = &method.sig.abi;
    let fn_token = method.sig.fn_token;
    let ident = &method.sig.ident;
    let generics = &method.sig.generics;
    let output = &method.sig.output;
    let where_clause = &method.sig.generics.where_clause;

    let (arg_pat, arg_val): (Vec<_>, Vec<_>) = method
        .sig
        .inputs
        .pairs()
        .enumerate()
        .map(|(i, pair)| {
            let (input, comma_token) = pair.into_tuple();
            match input {
                FnArg::Receiver(receiver) => {
                    let self_token = receiver.self_token;
                    if receiver.reference.is_some() {
                        (quote!(#receiver #comma_token), quote!(#self_token))
                    } else {
                        (quote!(#self_token #comma_token), quote!(#self_token))
                    }
                }
                FnArg::Typed(arg) => {
                    let var = match arg.pat.as_ref() {
                        Pat::Ident(pat) => pat.ident.clone(),
                        _ => Ident::new(&format!("__arg{}", i), Span::call_site()),
                    };
                    let colon_token = arg.colon_token;
                    let ty = &arg.ty;
                    (quote!(#var #colon_token #ty #comma_token), quote!(#var))
                }
            }
        })
        .unzip();

    let types = generics.type_params().map(|param| &param.ident);
    let body = quote!(<Self as #trait_>::#ident::<#(#types,)*>(#(#arg_val,)*));
    let block = quote_spanned!(method.block.brace_token.span=> { #body });
    let args = quote_spanned!(method.sig.paren_token.span=> (#(#arg_pat)*));

    let has_doc = attrs.iter().any(|attr| attr.path.is_ident("doc"));
    let default_doc = if has_doc {
        None
    } else {
        let mut link = String::new();
        for segment in &trait_.segments {
            link += &segment.ident.to_string();
            link += "::";
        }
        let msg = format!("See [`{}{}`]", link, ident);
        Some(quote!(#[doc = #msg]))
    };

    quote! {
        #(#attrs)*
        #default_doc
        #vis #constness #asyncness #unsafety #abi #fn_token #ident #generics #args #output #where_clause #block
    }
}

fn inherit_default_implementation(method: &ImplItemMethod) -> bool {
    method.block.stmts.len() == 1
        && match &method.block.stmts[0] {
            Stmt::Item(Item::Verbatim(verbatim)) => {
                let mut iter = verbatim.clone().into_iter();
                match iter.next() {
                    Some(TokenTree::Punct(punct)) => {
                        punct.as_char() == ';' && iter.next().is_none()
                    }
                    _ => false,
                }
            }
            _ => false,
        }
}
