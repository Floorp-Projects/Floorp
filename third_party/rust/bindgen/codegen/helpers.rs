//! Helpers for code generation that don't need macro expansion.

use crate::ir::context::BindgenContext;
use crate::ir::layout::Layout;

pub(crate) mod attributes {
    use proc_macro2::{Ident, Span, TokenStream};
    use std::{borrow::Cow, str::FromStr};

    pub(crate) fn repr(which: &str) -> TokenStream {
        let which = Ident::new(which, Span::call_site());
        quote! {
            #[repr( #which )]
        }
    }

    pub(crate) fn repr_list(which_ones: &[&str]) -> TokenStream {
        let which_ones = which_ones
            .iter()
            .cloned()
            .map(|one| TokenStream::from_str(one).expect("repr to be valid"));
        quote! {
            #[repr( #( #which_ones ),* )]
        }
    }

    pub(crate) fn derives(which_ones: &[&str]) -> TokenStream {
        let which_ones = which_ones
            .iter()
            .cloned()
            .map(|one| TokenStream::from_str(one).expect("derive to be valid"));
        quote! {
            #[derive( #( #which_ones ),* )]
        }
    }

    pub(crate) fn inline() -> TokenStream {
        quote! {
            #[inline]
        }
    }

    pub(crate) fn must_use() -> TokenStream {
        quote! {
            #[must_use]
        }
    }

    pub(crate) fn non_exhaustive() -> TokenStream {
        quote! {
            #[non_exhaustive]
        }
    }

    pub(crate) fn doc(comment: String) -> TokenStream {
        if comment.is_empty() {
            quote!()
        } else {
            quote!(#[doc = #comment])
        }
    }

    pub(crate) fn link_name<const MANGLE: bool>(name: &str) -> TokenStream {
        // LLVM mangles the name by default but it's already mangled.
        // Prefixing the name with \u{1} should tell LLVM to not mangle it.
        let name: Cow<'_, str> = if MANGLE {
            name.into()
        } else {
            format!("\u{1}{}", name).into()
        };

        quote! {
            #[link_name = #name]
        }
    }
}

/// Generates a proper type for a field or type with a given `Layout`, that is,
/// a type with the correct size and alignment restrictions.
pub(crate) fn blob(ctx: &BindgenContext, layout: Layout) -> syn::Type {
    let opaque = layout.opaque();

    // FIXME(emilio, #412): We fall back to byte alignment, but there are
    // some things that legitimately are more than 8-byte aligned.
    //
    // Eventually we should be able to `unwrap` here, but...
    let ty = match opaque.known_rust_type_for_array(ctx) {
        Some(ty) => ty,
        None => {
            warn!("Found unknown alignment on code generation!");
            syn::parse_quote! { u8 }
        }
    };

    let data_len = opaque.array_size(ctx).unwrap_or(layout.size);

    if data_len == 1 {
        ty
    } else {
        syn::parse_quote! { [ #ty ; #data_len ] }
    }
}

/// Integer type of the same size as the given `Layout`.
pub(crate) fn integer_type(
    ctx: &BindgenContext,
    layout: Layout,
) -> Option<syn::Type> {
    Layout::known_type_for_size(ctx, layout.size)
}

/// Generates a bitfield allocation unit type for a type with the given `Layout`.
pub(crate) fn bitfield_unit(ctx: &BindgenContext, layout: Layout) -> syn::Type {
    let size = layout.size;
    let ty = syn::parse_quote! { __BindgenBitfieldUnit<[u8; #size]> };

    if ctx.options().enable_cxx_namespaces {
        return syn::parse_quote! { root::#ty };
    }

    ty
}

pub(crate) mod ast_ty {
    use crate::ir::context::BindgenContext;
    use crate::ir::function::FunctionSig;
    use crate::ir::layout::Layout;
    use crate::ir::ty::{FloatKind, IntKind};
    use proc_macro2::{self, TokenStream};
    use std::str::FromStr;

    pub(crate) fn c_void(ctx: &BindgenContext) -> syn::Type {
        // ctypes_prefix takes precedence
        match ctx.options().ctypes_prefix {
            Some(ref prefix) => {
                let prefix = TokenStream::from_str(prefix.as_str()).unwrap();
                syn::parse_quote! { #prefix::c_void }
            }
            None => {
                if ctx.options().use_core &&
                    ctx.options().rust_features.core_ffi_c_void
                {
                    syn::parse_quote! { ::core::ffi::c_void }
                } else {
                    syn::parse_quote! { ::std::os::raw::c_void }
                }
            }
        }
    }

    pub(crate) fn raw_type(ctx: &BindgenContext, name: &str) -> syn::Type {
        let ident = ctx.rust_ident_raw(name);
        match ctx.options().ctypes_prefix {
            Some(ref prefix) => {
                let prefix = TokenStream::from_str(prefix.as_str()).unwrap();
                syn::parse_quote! { #prefix::#ident }
            }
            None => {
                if ctx.options().use_core &&
                    ctx.options().rust_features().core_ffi_c
                {
                    syn::parse_quote! { ::core::ffi::#ident }
                } else {
                    syn::parse_quote! { ::std::os::raw::#ident }
                }
            }
        }
    }

    pub(crate) fn int_kind_rust_type(
        ctx: &BindgenContext,
        ik: IntKind,
        layout: Option<Layout>,
    ) -> syn::Type {
        match ik {
            IntKind::Bool => syn::parse_quote! { bool },
            IntKind::Char { .. } => raw_type(ctx, "c_char"),
            IntKind::SChar => raw_type(ctx, "c_schar"),
            IntKind::UChar => raw_type(ctx, "c_uchar"),
            IntKind::Short => raw_type(ctx, "c_short"),
            IntKind::UShort => raw_type(ctx, "c_ushort"),
            IntKind::Int => raw_type(ctx, "c_int"),
            IntKind::UInt => raw_type(ctx, "c_uint"),
            IntKind::Long => raw_type(ctx, "c_long"),
            IntKind::ULong => raw_type(ctx, "c_ulong"),
            IntKind::LongLong => raw_type(ctx, "c_longlong"),
            IntKind::ULongLong => raw_type(ctx, "c_ulonglong"),
            IntKind::WChar => {
                let layout =
                    layout.expect("Couldn't compute wchar_t's layout?");
                Layout::known_type_for_size(ctx, layout.size)
                    .expect("Non-representable wchar_t?")
            }

            IntKind::I8 => syn::parse_quote! { i8 },
            IntKind::U8 => syn::parse_quote! { u8 },
            IntKind::I16 => syn::parse_quote! { i16 },
            IntKind::U16 => syn::parse_quote! { u16 },
            IntKind::I32 => syn::parse_quote! { i32 },
            IntKind::U32 => syn::parse_quote! { u32 },
            IntKind::I64 => syn::parse_quote! { i64 },
            IntKind::U64 => syn::parse_quote! { u64 },
            IntKind::Custom { name, .. } => {
                syn::parse_str(name).expect("Invalid integer type.")
            }
            IntKind::U128 => {
                if ctx.options().rust_features.i128_and_u128 {
                    syn::parse_quote! { u128 }
                } else {
                    // Best effort thing, but wrong alignment
                    // unfortunately.
                    syn::parse_quote! { [u64; 2] }
                }
            }
            IntKind::I128 => {
                if ctx.options().rust_features.i128_and_u128 {
                    syn::parse_quote! { i128 }
                } else {
                    syn::parse_quote! { [u64; 2] }
                }
            }
        }
    }

    pub(crate) fn float_kind_rust_type(
        ctx: &BindgenContext,
        fk: FloatKind,
        layout: Option<Layout>,
    ) -> syn::Type {
        // TODO: we probably should take the type layout into account more
        // often?
        //
        // Also, maybe this one shouldn't be the default?
        match (fk, ctx.options().convert_floats) {
            (FloatKind::Float, true) => syn::parse_quote! { f32 },
            (FloatKind::Double, true) => syn::parse_quote! { f64 },
            (FloatKind::Float, false) => raw_type(ctx, "c_float"),
            (FloatKind::Double, false) => raw_type(ctx, "c_double"),
            (FloatKind::LongDouble, _) => {
                match layout {
                    Some(layout) => {
                        match layout.size {
                            4 => syn::parse_quote! { f32 },
                            8 => syn::parse_quote! { f64 },
                            // TODO(emilio): If rust ever gains f128 we should
                            // use it here and below.
                            _ => super::integer_type(ctx, layout)
                                .unwrap_or(syn::parse_quote! { f64 }),
                        }
                    }
                    None => {
                        debug_assert!(
                            false,
                            "How didn't we know the layout for a primitive type?"
                        );
                        syn::parse_quote! { f64 }
                    }
                }
            }
            (FloatKind::Float128, _) => {
                if ctx.options().rust_features.i128_and_u128 {
                    syn::parse_quote! { u128 }
                } else {
                    syn::parse_quote! { [u64; 2] }
                }
            }
        }
    }

    pub(crate) fn int_expr(val: i64) -> TokenStream {
        // Don't use quote! { #val } because that adds the type suffix.
        let val = proc_macro2::Literal::i64_unsuffixed(val);
        quote!(#val)
    }

    pub(crate) fn uint_expr(val: u64) -> TokenStream {
        // Don't use quote! { #val } because that adds the type suffix.
        let val = proc_macro2::Literal::u64_unsuffixed(val);
        quote!(#val)
    }

    pub(crate) fn cstr_expr(mut string: String) -> TokenStream {
        string.push('\0');
        let b = proc_macro2::Literal::byte_string(string.as_bytes());
        quote! {
            #b
        }
    }

    pub(crate) fn float_expr(
        ctx: &BindgenContext,
        f: f64,
    ) -> Result<TokenStream, ()> {
        if f.is_finite() {
            let val = proc_macro2::Literal::f64_unsuffixed(f);

            return Ok(quote!(#val));
        }

        let prefix = ctx.trait_prefix();

        if f.is_nan() {
            return Ok(quote! {
                ::#prefix::f64::NAN
            });
        }

        if f.is_infinite() {
            return Ok(if f.is_sign_positive() {
                quote! {
                    ::#prefix::f64::INFINITY
                }
            } else {
                quote! {
                    ::#prefix::f64::NEG_INFINITY
                }
            });
        }

        warn!("Unknown non-finite float number: {:?}", f);
        Err(())
    }

    pub(crate) fn arguments_from_signature(
        signature: &FunctionSig,
        ctx: &BindgenContext,
    ) -> Vec<TokenStream> {
        let mut unnamed_arguments = 0;
        signature
            .argument_types()
            .iter()
            .map(|&(ref name, _ty)| match *name {
                Some(ref name) => {
                    let name = ctx.rust_ident(name);
                    quote! { #name }
                }
                None => {
                    unnamed_arguments += 1;
                    let name =
                        ctx.rust_ident(format!("arg{}", unnamed_arguments));
                    quote! { #name }
                }
            })
            .collect()
    }
}
