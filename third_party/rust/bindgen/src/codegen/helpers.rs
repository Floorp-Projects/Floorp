//! Helpers for code generation that don't need macro expansion.

use ir::context::BindgenContext;
use ir::layout::Layout;
use quote;
use std::mem;

pub mod attributes {
    use quote;

    pub fn repr(which: &str) -> quote::Tokens {
        let which = quote::Ident::new(which);
        quote! {
            #[repr( #which )]
        }
    }

    pub fn repr_list(which_ones: &[&str]) -> quote::Tokens {
        let which_ones = which_ones.iter().cloned().map(quote::Ident::new);
        quote! {
            #[repr( #( #which_ones ),* )]
        }
    }

    pub fn derives(which_ones: &[&str]) -> quote::Tokens {
        let which_ones = which_ones.iter().cloned().map(quote::Ident::new);
        quote! {
            #[derive( #( #which_ones ),* )]
        }
    }

    pub fn inline() -> quote::Tokens {
        quote! {
            #[inline]
        }
    }

    pub fn doc(comment: String) -> quote::Tokens {
        // Doc comments are already preprocessed into nice `///` formats by the
        // time they get here. Just make sure that we have newlines around it so
        // that nothing else gets wrapped into the comment.
        let mut tokens = quote! {};
        tokens.append("\n");
        tokens.append(comment);
        tokens.append("\n");
        tokens
    }

    pub fn link_name(name: &str) -> quote::Tokens {
        // LLVM mangles the name by default but it's already mangled.
        // Prefixing the name with \u{1} should tell LLVM to not mangle it.
        let name = format!("\u{1}{}", name);
        quote! {
            #[link_name = #name]
        }
    }
}

/// Generates a proper type for a field or type with a given `Layout`, that is,
/// a type with the correct size and alignment restrictions.
pub fn blob(layout: Layout) -> quote::Tokens {
    let opaque = layout.opaque();

    // FIXME(emilio, #412): We fall back to byte alignment, but there are
    // some things that legitimately are more than 8-byte aligned.
    //
    // Eventually we should be able to `unwrap` here, but...
    let ty_name = match opaque.known_rust_type_for_array() {
        Some(ty) => ty,
        None => {
            warn!("Found unknown alignment on code generation!");
            "u8"
        }
    };

    let ty_name = quote::Ident::new(ty_name);

    let data_len = opaque.array_size().unwrap_or(layout.size);

    if data_len == 1 {
        quote! {
            #ty_name
        }
    } else {
        quote! {
            [ #ty_name ; #data_len ]
        }
    }
}

/// Integer type of the same size as the given `Layout`.
pub fn integer_type(layout: Layout) -> Option<quote::Tokens> {
    // This guard can be weakened when Rust implements u128.
    if layout.size > mem::size_of::<u64>() {
        None
    } else {
        Some(blob(layout))
    }
}

/// Generates a bitfield allocation unit type for a type with the given `Layout`.
pub fn bitfield_unit(ctx: &BindgenContext, layout: Layout) -> quote::Tokens {
    let mut tokens = quote! {};

    if ctx.options().enable_cxx_namespaces {
        tokens.append(quote! { root:: });
    }

    let align = match layout.align {
        n if n >= 8 => quote! { u64 },
        4 => quote! { u32 },
        2 => quote! { u16 },
        _ => quote! { u8  },
    };

    let size = layout.size;
    tokens.append(quote! {
        __BindgenBitfieldUnit<[u8; #size], #align>
    });

    tokens
}

pub mod ast_ty {
    use ir::context::BindgenContext;
    use ir::function::FunctionSig;
    use ir::ty::FloatKind;
    use quote;

    pub fn raw_type(ctx: &BindgenContext, name: &str) -> quote::Tokens {
        let ident = ctx.rust_ident_raw(name);
        match ctx.options().ctypes_prefix {
            Some(ref prefix) => {
                let prefix = ctx.rust_ident_raw(prefix.as_str());
                quote! {
                    #prefix::#ident
                }
            }
            None => quote! {
                ::std::os::raw::#ident
            },
        }
    }

    pub fn float_kind_rust_type(
        ctx: &BindgenContext,
        fk: FloatKind,
    ) -> quote::Tokens {
        // TODO: we probably should just take the type layout into
        // account?
        //
        // Also, maybe this one shouldn't be the default?
        //
        // FIXME: `c_longdouble` doesn't seem to be defined in some
        // systems, so we use `c_double` directly.
        match (fk, ctx.options().convert_floats) {
            (FloatKind::Float, true) => quote! { f32 },
            (FloatKind::Double, true) |
            (FloatKind::LongDouble, true) => quote! { f64 },
            (FloatKind::Float, false) => raw_type(ctx, "c_float"),
            (FloatKind::Double, false) |
            (FloatKind::LongDouble, false) => raw_type(ctx, "c_double"),
            (FloatKind::Float128, _) => quote! { [u8; 16] },
        }
    }

    pub fn int_expr(val: i64) -> quote::Tokens {
        // Don't use quote! { #val } because that adds the type suffix.
        let mut tokens = quote! {};
        tokens.append(val.to_string());
        tokens
    }

    pub fn uint_expr(val: u64) -> quote::Tokens {
        // Don't use quote! { #val } because that adds the type suffix.
        let mut tokens = quote! {};
        tokens.append(val.to_string());
        tokens
    }

    pub fn byte_array_expr(bytes: &[u8]) -> quote::Tokens {
        let mut bytes: Vec<_> = bytes.iter().cloned().collect();
        bytes.push(0);
        quote! {
            #bytes
        }
    }

    pub fn cstr_expr(mut string: String) -> quote::Tokens {
        string.push('\0');
        let b = quote::ByteStr(&string);
        quote! {
            #b
        }
    }

    pub fn float_expr(
        ctx: &BindgenContext,
        f: f64,
    ) -> Result<quote::Tokens, ()> {
        if f.is_finite() {
            let mut string = f.to_string();

            // So it gets properly recognised as a floating point constant.
            if !string.contains('.') {
                string.push('.');
            }

            let mut tokens = quote! {};
            tokens.append(string);
            return Ok(tokens);
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
        return Err(());
    }

    pub fn arguments_from_signature(
        signature: &FunctionSig,
        ctx: &BindgenContext,
    ) -> Vec<quote::Tokens> {
        let mut unnamed_arguments = 0;
        signature
            .argument_types()
            .iter()
            .map(|&(ref name, _ty)| {
                match *name {
                    Some(ref name) => {
                        let name = ctx.rust_ident(name);
                        quote! { #name }
                    }
                    None => {
                        unnamed_arguments += 1;
                        let name = ctx.rust_ident(format!("arg{}", unnamed_arguments));
                        quote! { #name }
                    }
                }
            })
            .collect()
    }
}
