#![recursion_limit = "1024"]

extern crate proc_macro;
use proc_macro2;
use quote::quote;

use proc_macro::TokenStream;

fn impl_field(ident: &proc_macro2::TokenStream, ty: &syn::Type) -> proc_macro2::TokenStream {
    match *ty {
        syn::Type::Array(ref array) => match array.len {
            syn::Expr::Lit(syn::ExprLit {
                lit: syn::Lit::Int(ref int),
                ..
            }) => {
                let size = int.base10_parse::<usize>().unwrap();
                quote! {
                    #ident: { let mut __tmp: #ty = [0u8.into(); #size]; src.gread_inout_with(offset, &mut __tmp, ctx)?; __tmp }
                }
            }
            _ => panic!("Pread derive with bad array constexpr"),
        },
        syn::Type::Group(ref group) => impl_field(ident, &group.elem),
        _ => {
            quote! {
                #ident: src.gread_with::<#ty>(offset, ctx)?
            }
        }
    }
}

fn impl_struct(
    name: &syn::Ident,
    fields: &syn::punctuated::Punctuated<syn::Field, syn::Token![,]>,
    generics: &syn::Generics,
) -> proc_macro2::TokenStream {
    let items: Vec<_> = fields
        .iter()
        .enumerate()
        .map(|(i, f)| {
            let ident = &f.ident.as_ref().map(|i| quote! {#i}).unwrap_or({
                let t = proc_macro2::Literal::usize_unsuffixed(i);
                quote! {#t}
            });
            let ty = &f.ty;
            impl_field(ident, ty)
        })
        .collect();

    let gl = &generics.lt_token;
    let gp = &generics.params;
    let gg = &generics.gt_token;
    let gn = gp.iter().map(|param: &syn::GenericParam| match param {
        syn::GenericParam::Type(ref t) => {
            let ident = &t.ident;
            quote! { #ident }
        }
        p => quote! { #p },
    });
    let gn = quote! { #gl #( #gn ),* #gg };
    let gw = if !gp.is_empty() {
        let gi = gp.iter().map(|param: &syn::GenericParam| match param {
            syn::GenericParam::Type(ref t) => {
                let ident = &t.ident;
                quote! {
                    #ident : ::scroll::ctx::TryFromCtx<'a, ::scroll::Endian> + ::std::convert::From<u8> + ::std::marker::Copy,
                    ::scroll::Error : ::std::convert::From<< #ident as ::scroll::ctx::TryFromCtx<'a, ::scroll::Endian>>::Error>,
                    < #ident as ::scroll::ctx::TryFromCtx<'a, ::scroll::Endian>>::Error : ::std::convert::From<scroll::Error>
                }
            },
            p => quote! { #p }
        });
        quote! { #( #gi ),* , }
    } else {
        quote! {}
    };

    quote! {
        impl<'a, #gp > ::scroll::ctx::TryFromCtx<'a, ::scroll::Endian> for #name #gn where #gw #name #gn : 'a {
            type Error = ::scroll::Error;
            #[inline]
            fn try_from_ctx(src: &'a [u8], ctx: ::scroll::Endian) -> ::scroll::export::result::Result<(Self, usize), Self::Error> {
                use ::scroll::Pread;
                let offset = &mut 0;
                let data  = Self { #(#items,)* };
                Ok((data, *offset))
            }
        }
    }
}

fn impl_try_from_ctx(ast: &syn::DeriveInput) -> proc_macro2::TokenStream {
    let name = &ast.ident;
    let generics = &ast.generics;
    match ast.data {
        syn::Data::Struct(ref data) => match data.fields {
            syn::Fields::Named(ref fields) => impl_struct(name, &fields.named, generics),
            syn::Fields::Unnamed(ref fields) => impl_struct(name, &fields.unnamed, generics),
            _ => {
                panic!("Pread can not be derived for unit structs")
            }
        },
        _ => panic!("Pread can only be derived for structs"),
    }
}

#[proc_macro_derive(Pread)]
pub fn derive_pread(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let gen = impl_try_from_ctx(&ast);
    gen.into()
}

fn impl_pwrite_field(ident: &proc_macro2::TokenStream, ty: &syn::Type) -> proc_macro2::TokenStream {
    match ty {
        syn::Type::Array(ref array) => match array.len {
            syn::Expr::Lit(syn::ExprLit {
                lit: syn::Lit::Int(ref int),
                ..
            }) => {
                let size = int.base10_parse::<usize>().unwrap();
                quote! {
                    for i in 0..#size {
                        dst.gwrite_with(&self.#ident[i], offset, ctx)?;
                    }
                }
            }
            _ => panic!("Pwrite derive with bad array constexpr"),
        },
        syn::Type::Group(group) => impl_pwrite_field(ident, &group.elem),
        _ => {
            quote! {
                dst.gwrite_with(&self.#ident, offset, ctx)?
            }
        }
    }
}

fn impl_try_into_ctx(
    name: &syn::Ident,
    fields: &syn::punctuated::Punctuated<syn::Field, syn::Token![,]>,
    generics: &syn::Generics,
) -> proc_macro2::TokenStream {
    let items: Vec<_> = fields
        .iter()
        .enumerate()
        .map(|(i, f)| {
            let ident = &f.ident.as_ref().map(|i| quote! {#i}).unwrap_or({
                let t = proc_macro2::Literal::usize_unsuffixed(i);
                quote! {#t}
            });
            let ty = &f.ty;
            impl_pwrite_field(ident, ty)
        })
        .collect();

    let gl = &generics.lt_token;
    let gp = &generics.params;
    let gg = &generics.gt_token;
    let gn = gp.iter().map(|param: &syn::GenericParam| match param {
        syn::GenericParam::Type(ref t) => {
            let ident = &t.ident;
            quote! { #ident }
        }
        p => quote! { #p },
    });
    let gn = quote! { #gl #( #gn ),* #gg };
    let gwref = if !gp.is_empty() {
        let gi = gp.iter().map(|param: &syn::GenericParam| match param {
            syn::GenericParam::Type(ref t) => {
                let ident = &t.ident;
                quote! {
                    &'a #ident : ::scroll::ctx::TryIntoCtx<::scroll::Endian>,
                    ::scroll::Error: ::std::convert::From<<&'a #ident as ::scroll::ctx::TryIntoCtx<::scroll::Endian>>::Error>,
                    <&'a #ident as ::scroll::ctx::TryIntoCtx<::scroll::Endian>>::Error: ::std::convert::From<scroll::Error>
                }
            },
            p => quote! { #p }
        });
        quote! { where #( #gi ),* }
    } else {
        quote! {}
    };
    let gw = if !gp.is_empty() {
        let gi = gp.iter().map(|param: &syn::GenericParam| match param {
            syn::GenericParam::Type(ref t) => {
                let ident = &t.ident;
                quote! {
                    #ident : ::scroll::ctx::TryIntoCtx<::scroll::Endian>,
                    ::scroll::Error: ::std::convert::From<<#ident as ::scroll::ctx::TryIntoCtx<::scroll::Endian>>::Error>,
                    <#ident as ::scroll::ctx::TryIntoCtx<::scroll::Endian>>::Error: ::std::convert::From<scroll::Error>
                }
            },
            p => quote! { #p }
        });
        quote! { where Self: ::std::marker::Copy, #( #gi ),* }
    } else {
        quote! {}
    };

    quote! {
        impl<'a, #gp > ::scroll::ctx::TryIntoCtx<::scroll::Endian> for &'a #name #gn #gwref {
            type Error = ::scroll::Error;
            #[inline]
            fn try_into_ctx(self, dst: &mut [u8], ctx: ::scroll::Endian) -> ::scroll::export::result::Result<usize, Self::Error> {
                use ::scroll::Pwrite;
                let offset = &mut 0;
                #(#items;)*;
                Ok(*offset)
            }
        }

        impl #gl #gp #gg ::scroll::ctx::TryIntoCtx<::scroll::Endian> for #name #gn #gw {
            type Error = ::scroll::Error;
            #[inline]
            fn try_into_ctx(self, dst: &mut [u8], ctx: ::scroll::Endian) -> ::scroll::export::result::Result<usize, Self::Error> {
                (&self).try_into_ctx(dst, ctx)
            }
        }
    }
}

fn impl_pwrite(ast: &syn::DeriveInput) -> proc_macro2::TokenStream {
    let name = &ast.ident;
    let generics = &ast.generics;
    match ast.data {
        syn::Data::Struct(ref data) => match data.fields {
            syn::Fields::Named(ref fields) => impl_try_into_ctx(name, &fields.named, generics),
            syn::Fields::Unnamed(ref fields) => impl_try_into_ctx(name, &fields.unnamed, generics),
            _ => {
                panic!("Pwrite can not be derived for unit structs")
            }
        },
        _ => panic!("Pwrite can only be derived for structs"),
    }
}

#[proc_macro_derive(Pwrite)]
pub fn derive_pwrite(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let gen = impl_pwrite(&ast);
    gen.into()
}

fn size_with(
    name: &syn::Ident,
    fields: &syn::punctuated::Punctuated<syn::Field, syn::Token![,]>,
    generics: &syn::Generics,
) -> proc_macro2::TokenStream {
    let items: Vec<_> = fields
        .iter()
        .map(|f| {
            let ty = &f.ty;
            match *ty {
                syn::Type::Array(ref array) => {
                    let elem = &array.elem;
                    match array.len {
                        syn::Expr::Lit(syn::ExprLit {
                            lit: syn::Lit::Int(ref int),
                            ..
                        }) => {
                            let size = int.base10_parse::<usize>().unwrap();
                            quote! {
                                (#size * <#elem>::size_with(ctx))
                            }
                        }
                        _ => panic!("Pread derive with bad array constexpr"),
                    }
                }
                _ => {
                    quote! {
                        <#ty>::size_with(ctx)
                    }
                }
            }
        })
        .collect();

    let gl = &generics.lt_token;
    let gp = &generics.params;
    let gg = &generics.gt_token;
    let gn = gp.iter().map(|param: &syn::GenericParam| match param {
        syn::GenericParam::Type(ref t) => {
            let ident = &t.ident;
            quote! { #ident }
        }
        p => quote! { #p },
    });
    let gn = quote! { #gl #( #gn ),* #gg };
    let gw = if !gp.is_empty() {
        let gi = gp.iter().map(|param: &syn::GenericParam| match param {
            syn::GenericParam::Type(ref t) => {
                let ident = &t.ident;
                quote! {
                    #ident : ::scroll::ctx::SizeWith<::scroll::Endian>
                }
            }
            p => quote! { #p },
        });
        quote! { where #( #gi ),* }
    } else {
        quote! {}
    };

    quote! {
        impl #gl #gp #gg ::scroll::ctx::SizeWith<::scroll::Endian> for #name #gn #gw {
            #[inline]
            fn size_with(ctx: &::scroll::Endian) -> usize {
                0 #(+ #items)*
            }
        }
    }
}

fn impl_size_with(ast: &syn::DeriveInput) -> proc_macro2::TokenStream {
    let name = &ast.ident;
    let generics = &ast.generics;
    match ast.data {
        syn::Data::Struct(ref data) => match data.fields {
            syn::Fields::Named(ref fields) => size_with(name, &fields.named, generics),
            syn::Fields::Unnamed(ref fields) => size_with(name, &fields.unnamed, generics),
            _ => {
                panic!("SizeWith can not be derived for unit structs")
            }
        },
        _ => panic!("SizeWith can only be derived for structs"),
    }
}

#[proc_macro_derive(SizeWith)]
pub fn derive_sizewith(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let gen = impl_size_with(&ast);
    gen.into()
}

fn impl_cread_struct(
    name: &syn::Ident,
    fields: &syn::punctuated::Punctuated<syn::Field, syn::Token![,]>,
    generics: &syn::Generics,
) -> proc_macro2::TokenStream {
    let items: Vec<_> = fields.iter().enumerate().map(|(i, f)| {
        let ident = &f.ident.as_ref().map(|i|quote!{#i}).unwrap_or({let t = proc_macro2::Literal::usize_unsuffixed(i); quote!{#t}});
        let ty = &f.ty;
        match *ty {
            syn::Type::Array(ref array) => {
                let arrty = &array.elem;
                match array.len {
                    syn::Expr::Lit(syn::ExprLit { lit: syn::Lit::Int(ref int), ..}) => {
                        let size = int.base10_parse::<usize>().unwrap();
                        let incr = quote! { ::scroll::export::mem::size_of::<#arrty>() };
                        quote! {
                            #ident: {
                                let mut __tmp: #ty = [0u8.into(); #size];
                                for i in 0..__tmp.len() {
                                    __tmp[i] = src.cread_with(*offset, ctx);
                                    *offset += #incr;
                                }
                                __tmp
                            }
                        }
                    },
                    _ => panic!("IOread derive with bad array constexpr")
                }
            },
            _ => {
                let size = quote! { ::scroll::export::mem::size_of::<#ty>() };
                quote! {
                    #ident: { let res = src.cread_with::<#ty>(*offset, ctx); *offset += #size; res }
                }
            }
        }
    }).collect();

    let gl = &generics.lt_token;
    let gp = &generics.params;
    let gg = &generics.gt_token;
    let gn = gp.iter().map(|param: &syn::GenericParam| match param {
        syn::GenericParam::Type(ref t) => {
            let ident = &t.ident;
            quote! { #ident }
        }
        p => quote! { #p },
    });
    let gn = quote! { #gl #( #gn ),* #gg };
    let gw = if !gp.is_empty() {
        let gi = gp.iter().map(|param: &syn::GenericParam| match param {
            syn::GenericParam::Type(ref t) => {
                let ident = &t.ident;
                quote! {
                    #ident : ::scroll::ctx::FromCtx<::scroll::Endian> + ::std::convert::From<u8> + ::std::marker::Copy
                }
            },
            p => quote! { #p }
        });
        quote! { where #( #gi ),* , }
    } else {
        quote! {}
    };

    quote! {
        impl #gl #gp #gg ::scroll::ctx::FromCtx<::scroll::Endian> for #name #gn #gw {
            #[inline]
            fn from_ctx(src: &[u8], ctx: ::scroll::Endian) -> Self {
                use ::scroll::Cread;
                let offset = &mut 0;
                let data = Self { #(#items,)* };
                data
            }
        }
    }
}

fn impl_from_ctx(ast: &syn::DeriveInput) -> proc_macro2::TokenStream {
    let name = &ast.ident;
    let generics = &ast.generics;
    match ast.data {
        syn::Data::Struct(ref data) => match data.fields {
            syn::Fields::Named(ref fields) => impl_cread_struct(name, &fields.named, generics),
            syn::Fields::Unnamed(ref fields) => impl_cread_struct(name, &fields.unnamed, generics),
            _ => {
                panic!("IOread can not be derived for unit structs")
            }
        },
        _ => panic!("IOread can only be derived for structs"),
    }
}

#[proc_macro_derive(IOread)]
pub fn derive_ioread(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let gen = impl_from_ctx(&ast);
    gen.into()
}

fn impl_into_ctx(
    name: &syn::Ident,
    fields: &syn::punctuated::Punctuated<syn::Field, syn::Token![,]>,
    generics: &syn::Generics,
) -> proc_macro2::TokenStream {
    let items: Vec<_> = fields
        .iter()
        .enumerate()
        .map(|(i, f)| {
            let ident = &f.ident.as_ref().map(|i| quote! {#i}).unwrap_or({
                let t = proc_macro2::Literal::usize_unsuffixed(i);
                quote! {#t}
            });
            let ty = &f.ty;
            let size = quote! { ::scroll::export::mem::size_of::<#ty>() };
            match *ty {
                syn::Type::Array(ref array) => {
                    let arrty = &array.elem;
                    quote! {
                        let size = ::scroll::export::mem::size_of::<#arrty>();
                        for i in 0..self.#ident.len() {
                            dst.cwrite_with(self.#ident[i], *offset, ctx);
                            *offset += size;
                        }
                    }
                }
                _ => {
                    quote! {
                        dst.cwrite_with(self.#ident, *offset, ctx);
                        *offset += #size;
                    }
                }
            }
        })
        .collect();

    let gl = &generics.lt_token;
    let gp = &generics.params;
    let gg = &generics.gt_token;
    let gn = gp.iter().map(|param: &syn::GenericParam| match param {
        syn::GenericParam::Type(ref t) => {
            let ident = &t.ident;
            quote! { #ident }
        }
        p => quote! { #p },
    });
    let gw = if !gp.is_empty() {
        let gi = gp.iter().map(|param: &syn::GenericParam| match param {
            syn::GenericParam::Type(ref t) => {
                let ident = &t.ident;
                quote! {
                    #ident : ::scroll::ctx::IntoCtx<::scroll::Endian> + ::std::marker::Copy
                }
            }
            p => quote! { #p },
        });
        quote! { where #( #gi ),* }
    } else {
        quote! {}
    };
    let gn = quote! { #gl #( #gn ),* #gg };

    quote! {
        impl<'a, #gp > ::scroll::ctx::IntoCtx<::scroll::Endian> for &'a #name #gn #gw {
            #[inline]
            fn into_ctx(self, dst: &mut [u8], ctx: ::scroll::Endian) {
                use ::scroll::Cwrite;
                let offset = &mut 0;
                #(#items;)*;
                ()
            }
        }

        impl #gl #gp #gg ::scroll::ctx::IntoCtx<::scroll::Endian> for #name #gn #gw {
            #[inline]
            fn into_ctx(self, dst: &mut [u8], ctx: ::scroll::Endian) {
                (&self).into_ctx(dst, ctx)
            }
        }
    }
}

fn impl_iowrite(ast: &syn::DeriveInput) -> proc_macro2::TokenStream {
    let name = &ast.ident;
    let generics = &ast.generics;
    match ast.data {
        syn::Data::Struct(ref data) => match data.fields {
            syn::Fields::Named(ref fields) => impl_into_ctx(name, &fields.named, generics),
            syn::Fields::Unnamed(ref fields) => impl_into_ctx(name, &fields.unnamed, generics),
            _ => {
                panic!("IOwrite can not be derived for unit structs")
            }
        },
        _ => panic!("IOwrite can only be derived for structs"),
    }
}

#[proc_macro_derive(IOwrite)]
pub fn derive_iowrite(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let gen = impl_iowrite(&ast);
    gen.into()
}
