#![recursion_limit="1024"]

extern crate proc_macro;
extern crate proc_macro2;
#[macro_use]
extern crate quote;
extern crate syn;

use proc_macro::TokenStream;

fn impl_struct(name: &syn::Ident, fields: &syn::FieldsNamed) -> proc_macro2::TokenStream {
    let items: Vec<_> = fields.named.iter().map(|f| {
        let ident = &f.ident;
        let ty = &f.ty;
        match ty {
            &syn::Type::Array(ref array) => {
                match array.len {
                    syn::Expr::Lit(syn::ExprLit { lit: syn::Lit::Int(ref int), ..}) => {
                        let size = int.value();
                        quote! {
                            #ident: { let mut __tmp: #ty = [0; #size as usize]; src.gread_inout_with(offset, &mut __tmp, ctx)?; __tmp }
                        }
                    },
                    _ => panic!("Pread derive with bad array constexpr")
                }
            },
            _ => {
                quote! {
                    #ident: src.gread_with::<#ty>(offset, ctx)?
                }
            }
        }
    }).collect();
    
    quote! {
        impl<'a> ::scroll::ctx::TryFromCtx<'a, ::scroll::Endian> for #name where #name: 'a {
            type Error = ::scroll::Error;
            type Size = usize;
            #[inline]
            fn try_from_ctx(src: &'a [u8], ctx: ::scroll::Endian) -> ::scroll::export::result::Result<(Self, Self::Size), Self::Error> {
                use ::scroll::Pread;
                let offset = &mut 0;
                let data  = #name { #(#items,)* };
                Ok((data, *offset))
            }
        }
    }
}

fn impl_try_from_ctx(ast: &syn::DeriveInput) -> proc_macro2::TokenStream {
    let name = &ast.ident;
    match ast.data {
        syn::Data::Struct(ref data) => {
            match data.fields {
                syn::Fields::Named(ref fields) => {
                    impl_struct(name, fields)
                },
                _ => {
                    panic!("Pread can only be derived for a regular struct with public fields")
                }
            }
        },
        _ => panic!("Pread can only be derived for structs")
    }
}

#[proc_macro_derive(Pread)]
pub fn derive_pread(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let gen = impl_try_from_ctx(&ast);
    gen.into()
}

fn impl_try_into_ctx(name: &syn::Ident, fields: &syn::FieldsNamed) -> proc_macro2::TokenStream {
    let items: Vec<_> = fields.named.iter().map(|f| {
        let ident = &f.ident;
        let ty = &f.ty;
        match ty {
            &syn::Type::Array(_) => {
                quote! {
                    for i in 0..self.#ident.len() {
                        dst.gwrite_with(&self.#ident[i], offset, ctx)?;
                    }
                }
            },
            _ => {
                quote! {
                    dst.gwrite_with(&self.#ident, offset, ctx)?
                }
            }
        }
    }).collect();
    
    quote! {
        impl<'a> ::scroll::ctx::TryIntoCtx<::scroll::Endian> for &'a #name {
            type Error = ::scroll::Error;
            type Size = usize;
            #[inline]
            fn try_into_ctx(self, dst: &mut [u8], ctx: ::scroll::Endian) -> ::scroll::export::result::Result<Self::Size, Self::Error> {
                use ::scroll::Pwrite;
                let offset = &mut 0;
                #(#items;)*;
                Ok(*offset)
            }
        }

        impl ::scroll::ctx::TryIntoCtx<::scroll::Endian> for #name {
            type Error = ::scroll::Error;
            type Size = usize;
            #[inline]
            fn try_into_ctx(self, dst: &mut [u8], ctx: ::scroll::Endian) -> ::scroll::export::result::Result<Self::Size, Self::Error> {
                (&self).try_into_ctx(dst, ctx)
            }
        }
    }
}

fn impl_pwrite(ast: &syn::DeriveInput) -> proc_macro2::TokenStream {
    let name = &ast.ident;
    match ast.data {
        syn::Data::Struct(ref data) => {
            match data.fields {
                syn::Fields::Named(ref fields) => {
                    impl_try_into_ctx(name, fields)
                },
                _ => {
                    panic!("Pwrite can only be derived for a regular struct with public fields")
                }
            }
        },
        _ => panic!("Pwrite can only be derived for structs")
    }
}

#[proc_macro_derive(Pwrite)]
pub fn derive_pwrite(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let gen = impl_pwrite(&ast);
    gen.into()
}

fn size_with(name: &syn::Ident, fields: &syn::FieldsNamed) -> proc_macro2::TokenStream {
    let items: Vec<_> = fields.named.iter().map(|f| {
        let ty = &f.ty;
        match ty {
            &syn::Type::Array(ref array) => {
                let elem = &array.elem;
                match array.len {
                    syn::Expr::Lit(syn::ExprLit { lit: syn::Lit::Int(ref int), ..}) => {
                        let size = int.value() as usize;
                        quote! {
                            (#size * <#elem>::size_with(ctx))
                        }
                    },
                    _ => panic!("Pread derive with bad array constexpr")
                }
            },
            _ => {
                quote! {
                    <#ty>::size_with(ctx)
                }
            }
        }
    }).collect();
    quote! {
        impl ::scroll::ctx::SizeWith<::scroll::Endian> for #name {
            type Units = usize;
            #[inline]
            fn size_with(ctx: &::scroll::Endian) -> Self::Units {
                0 #(+ #items)*
            }
        }
    }
}

fn impl_size_with(ast: &syn::DeriveInput) -> proc_macro2::TokenStream {
    let name = &ast.ident;
    match ast.data {
        syn::Data::Struct(ref data) => {
            match data.fields {
                syn::Fields::Named(ref fields) => {
                    size_with(name, fields)
                },
                _ => {
                    panic!("SizeWith can only be derived for a regular struct with public fields")
                }
            }
        },
        _ => panic!("SizeWith can only be derived for structs")
    }
}

#[proc_macro_derive(SizeWith)]
pub fn derive_sizewith(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let gen = impl_size_with(&ast);
    gen.into()
}

fn impl_cread_struct(name: &syn::Ident, fields: &syn::FieldsNamed) -> proc_macro2::TokenStream {
    let items: Vec<_> = fields.named.iter().map(|f| {
        let ident = &f.ident;
        let ty = &f.ty;
        match ty {
            &syn::Type::Array(ref array) => {
                let arrty = &array.elem;
                match array.len {
                    syn::Expr::Lit(syn::ExprLit { lit: syn::Lit::Int(ref int), ..}) => {
                        let size = int.value();
                        let incr = quote! { ::scroll::export::mem::size_of::<#arrty>() };
                        quote! {
                            #ident: {
                                let mut __tmp: #ty = [0; #size as usize];
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

    quote! {
        impl ::scroll::ctx::FromCtx<::scroll::Endian> for #name {
            #[inline]
            fn from_ctx(src: &[u8], ctx: ::scroll::Endian) -> Self {
                use ::scroll::Cread;
                let offset = &mut 0;
                let data = #name { #(#items,)* };
                data
            }
        }
    }
}

fn impl_from_ctx(ast: &syn::DeriveInput) -> proc_macro2::TokenStream {
    let name = &ast.ident;
    match ast.data {
        syn::Data::Struct(ref data) => {
            match data.fields {
                syn::Fields::Named(ref fields) => {
                    impl_cread_struct(name, fields)
                },
                _ => {
                    panic!("IOread can only be derived for a regular struct with public fields")
                }
            }
        },
        _ => panic!("IOread can only be derived for structs")
    }
}

#[proc_macro_derive(IOread)]
pub fn derive_ioread(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let gen = impl_from_ctx(&ast);
    gen.into()
}

fn impl_into_ctx(name: &syn::Ident, fields: &syn::FieldsNamed) -> proc_macro2::TokenStream {
    let items: Vec<_> = fields.named.iter().map(|f| {
        let ident = &f.ident;
        let ty = &f.ty;
        let size = quote! { ::scroll::export::mem::size_of::<#ty>() };
        match ty {
            &syn::Type::Array(ref array) => {
                let arrty = &array.elem;
                quote! {
                    let size = ::scroll::export::mem::size_of::<#arrty>();
                    for i in 0..self.#ident.len() {
                        dst.cwrite_with(self.#ident[i], *offset, ctx);
                        *offset += size;
                    }
                }
            },
            _ => {
                quote! {
                    dst.cwrite_with(self.#ident, *offset, ctx);
                    *offset += #size;
                }
            }
        }
    }).collect();

    quote! {
        impl<'a> ::scroll::ctx::IntoCtx<::scroll::Endian> for &'a #name {
            #[inline]
            fn into_ctx(self, dst: &mut [u8], ctx: ::scroll::Endian) {
                use ::scroll::Cwrite;
                let offset = &mut 0;
                #(#items;)*;
                ()
            }
        }

        impl ::scroll::ctx::IntoCtx<::scroll::Endian> for #name {
            #[inline]
            fn into_ctx(self, dst: &mut [u8], ctx: ::scroll::Endian) {
                (&self).into_ctx(dst, ctx)
            }
        }
    }
}

fn impl_iowrite(ast: &syn::DeriveInput) -> proc_macro2::TokenStream {
    let name = &ast.ident;
    match ast.data {
        syn::Data::Struct(ref data) => {
            match data.fields {
                syn::Fields::Named(ref fields) => {
                    impl_into_ctx(name, fields)
                },
                _ => {
                    panic!("IOwrite can only be derived for a regular struct with public fields")
                }
            }
        },
        _ => panic!("IOwrite can only be derived for structs")
    }
}

#[proc_macro_derive(IOwrite)]
pub fn derive_iowrite(input: TokenStream) -> TokenStream {
    let ast: syn::DeriveInput = syn::parse(input).unwrap();
    let gen = impl_iowrite(&ast);
    gen.into()
}
