//! Helpers for code generation that don't need macro expansion.

use aster;
use ir::layout::Layout;
use syntax::ast;
use syntax::ptr::P;


pub mod attributes {
    use aster;
    use syntax::ast;

    pub fn allow(which_ones: &[&str]) -> ast::Attribute {
        aster::AstBuilder::new().attr().list("allow").words(which_ones).build()
    }

    pub fn repr(which: &str) -> ast::Attribute {
        aster::AstBuilder::new().attr().list("repr").words(&[which]).build()
    }

    pub fn repr_list(which_ones: &[&str]) -> ast::Attribute {
        aster::AstBuilder::new().attr().list("repr").words(which_ones).build()
    }

    pub fn derives(which_ones: &[&str]) -> ast::Attribute {
        aster::AstBuilder::new().attr().list("derive").words(which_ones).build()
    }

    pub fn inline() -> ast::Attribute {
        aster::AstBuilder::new().attr().word("inline")
    }

    pub fn doc(comment: &str) -> ast::Attribute {
        aster::AstBuilder::new().attr().doc(comment)
    }

    pub fn link_name(name: &str) -> ast::Attribute {
        aster::AstBuilder::new().attr().name_value("link_name").str(name)
    }
}

/// Generates a proper type for a field or type with a given `Layout`, that is,
/// a type with the correct size and alignment restrictions.
pub struct BlobTyBuilder {
    layout: Layout,
}

impl BlobTyBuilder {
    pub fn new(layout: Layout) -> Self {
        BlobTyBuilder {
            layout: layout,
        }
    }

    pub fn build(self) -> P<ast::Ty> {
        let opaque = self.layout.opaque();

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

        let data_len = opaque.array_size().unwrap_or(self.layout.size);

        let inner_ty = aster::AstBuilder::new().ty().path().id(ty_name).build();
        if data_len == 1 {
            inner_ty
        } else {
            aster::ty::TyBuilder::new().array(data_len).build(inner_ty)
        }
    }
}

pub mod ast_ty {
    use aster;
    use ir::context::BindgenContext;
    use ir::function::FunctionSig;
    use ir::ty::FloatKind;
    use syntax::ast;
    use syntax::ptr::P;

    pub fn raw_type(ctx: &BindgenContext, name: &str) -> P<ast::Ty> {
        let ident = ctx.rust_ident_raw(&name);
        match ctx.options().ctypes_prefix {
            Some(ref prefix) => {
                let prefix = ctx.rust_ident_raw(prefix);
                quote_ty!(ctx.ext_cx(), $prefix::$ident)
            }
            None => quote_ty!(ctx.ext_cx(), ::std::os::raw::$ident),
        }
    }

    pub fn float_kind_rust_type(ctx: &BindgenContext,
                                fk: FloatKind)
                                -> P<ast::Ty> {
        // TODO: we probably should just take the type layout into
        // account?
        //
        // Also, maybe this one shouldn't be the default?
        //
        // FIXME: `c_longdouble` doesn't seem to be defined in some
        // systems, so we use `c_double` directly.
        match (fk, ctx.options().convert_floats) {
            (FloatKind::Float, true) => aster::ty::TyBuilder::new().f32(),
            (FloatKind::Double, true) |
            (FloatKind::LongDouble, true) => aster::ty::TyBuilder::new().f64(),
            (FloatKind::Float, false) => raw_type(ctx, "c_float"),
            (FloatKind::Double, false) |
            (FloatKind::LongDouble, false) => raw_type(ctx, "c_double"),
            (FloatKind::Float128, _) => {
                aster::ty::TyBuilder::new().array(16).u8()
            }
        }
    }

    pub fn int_expr(val: i64) -> P<ast::Expr> {
        use std::i64;
        let expr = aster::AstBuilder::new().expr();

        // This is not representable as an i64 if it's negative, so we
        // special-case it.
        //
        // Fix in aster incoming.
        if val == i64::MIN {
            expr.neg().uint(1u64 << 63)
        } else {
            expr.int(val)
        }
    }

    pub fn bool_expr(val: bool) -> P<ast::Expr> {
        aster::AstBuilder::new().expr().bool(val)
    }

    pub fn byte_array_expr(bytes: &[u8]) -> P<ast::Expr> {
        let mut vec = Vec::with_capacity(bytes.len() + 1);
        for byte in bytes {
            vec.push(int_expr(*byte as i64));
        }
        vec.push(int_expr(0));

        let kind = ast::ExprKind::Array(vec);

        aster::AstBuilder::new().expr().build_expr_kind(kind)
    }

    pub fn cstr_expr(mut string: String) -> P<ast::Expr> {
        string.push('\0');
        aster::AstBuilder::new()
            .expr()
            .build_lit(aster::AstBuilder::new().lit().byte_str(string))
    }

    pub fn float_expr(ctx: &BindgenContext,
                      f: f64)
                      -> Result<P<ast::Expr>, ()> {
        use aster::symbol::ToSymbol;

        if f.is_finite() {
            let mut string = f.to_string();

            // So it gets properly recognised as a floating point constant.
            if !string.contains('.') {
                string.push('.');
            }

            let kind = ast::LitKind::FloatUnsuffixed(string.as_str().to_symbol());
            return Ok(aster::AstBuilder::new().expr().lit().build_lit(kind))
        }

        let prefix = ctx.trait_prefix();
        if f.is_nan() {
            return Ok(quote_expr!(ctx.ext_cx(), ::$prefix::f64::NAN));
        }

        if f.is_infinite() {
            return Ok(if f.is_sign_positive() {
                quote_expr!(ctx.ext_cx(), ::$prefix::f64::INFINITY)
            } else {
                quote_expr!(ctx.ext_cx(), ::$prefix::f64::NEG_INFINITY)
            });
        }

        warn!("Unknown non-finite float number: {:?}", f);
        return Err(());
    }

    pub fn arguments_from_signature(signature: &FunctionSig,
                                    ctx: &BindgenContext)
                                    -> Vec<P<ast::Expr>> {
        // TODO: We need to keep in sync the argument names, so we should unify
        // this with the other loop that decides them.
        let mut unnamed_arguments = 0;
        signature.argument_types()
            .iter()
            .map(|&(ref name, _ty)| {
                let arg_name = match *name {
                    Some(ref name) => ctx.rust_mangle(name).into_owned(),
                    None => {
                        unnamed_arguments += 1;
                        format!("arg{}", unnamed_arguments)
                    }
                };
                aster::expr::ExprBuilder::new().id(arg_name)
            })
            .collect::<Vec<_>>()
    }
}
