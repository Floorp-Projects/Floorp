use super::*;

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub enum ConstExpr {
    /// A function call
    ///
    /// The first field resolves to the function itself,
    /// and the second field is the list of arguments
    Call(Box<ConstExpr>, Vec<ConstExpr>),

    /// A binary operation (For example: `a + b`, `a * b`)
    Binary(BinOp, Box<ConstExpr>, Box<ConstExpr>),

    /// A unary operation (For example: `!x`, `*x`)
    Unary(UnOp, Box<ConstExpr>),

    /// A literal (For example: `1`, `"foo"`)
    Lit(Lit),

    /// A cast (`foo as f64`)
    Cast(Box<ConstExpr>, Box<Ty>),

    /// Variable reference, possibly containing `::` and/or type
    /// parameters, e.g. foo::bar::<baz>.
    Path(Path),

    /// An indexing operation (`foo[2]`)
    Index(Box<ConstExpr>, Box<ConstExpr>),

    /// No-op: used solely so we can pretty-print faithfully
    Paren(Box<ConstExpr>),

    /// If compiling with full support for expression syntax, any expression is
    /// allowed
    Other(Other),
}

#[cfg(not(feature = "full"))]
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct Other {
    _private: (),
}

#[cfg(feature = "full")]
pub type Other = Expr;

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use {BinOp, Ty};
    use lit::parsing::lit;
    use op::parsing::{binop, unop};
    use ty::parsing::{path, ty};

    named!(pub const_expr -> ConstExpr, do_parse!(
        mut e: alt!(
            expr_unary
            |
            expr_lit
            |
            expr_path
            |
            expr_paren
            // Cannot handle ConstExpr::Other here because for example
            // `[u32; n!()]` would end up successfully parsing `n` as
            // ConstExpr::Path and then fail to parse `!()`. Instead, callers
            // are required to handle Other. See ty::parsing::array_len and
            // data::parsing::discriminant.
        ) >>
        many0!(alt!(
            tap!(args: and_call => {
                e = ConstExpr::Call(Box::new(e), args);
            })
            |
            tap!(more: and_binary => {
                let (op, other) = more;
                e = ConstExpr::Binary(op, Box::new(e), Box::new(other));
            })
            |
            tap!(ty: and_cast => {
                e = ConstExpr::Cast(Box::new(e), Box::new(ty));
            })
            |
            tap!(i: and_index => {
                e = ConstExpr::Index(Box::new(e), Box::new(i));
            })
        )) >>
        (e)
    ));

    named!(and_call -> Vec<ConstExpr>, do_parse!(
        punct!("(") >>
        args: terminated_list!(punct!(","), const_expr) >>
        punct!(")") >>
        (args)
    ));

    named!(and_binary -> (BinOp, ConstExpr), tuple!(binop, const_expr));

    named!(expr_unary -> ConstExpr, do_parse!(
        operator: unop >>
        operand: const_expr >>
        (ConstExpr::Unary(operator, Box::new(operand)))
    ));

    named!(expr_lit -> ConstExpr, map!(lit, ConstExpr::Lit));

    named!(expr_path -> ConstExpr, map!(path, ConstExpr::Path));

    named!(and_index -> ConstExpr, delimited!(punct!("["), const_expr, punct!("]")));

    named!(expr_paren -> ConstExpr, do_parse!(
        punct!("(") >>
        e: const_expr >>
        punct!(")") >>
        (ConstExpr::Paren(Box::new(e)))
    ));

    named!(and_cast -> Ty, do_parse!(
        keyword!("as") >>
        ty: ty >>
        (ty)
    ));
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use quote::{Tokens, ToTokens};

    impl ToTokens for ConstExpr {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                ConstExpr::Call(ref func, ref args) => {
                    func.to_tokens(tokens);
                    tokens.append("(");
                    tokens.append_separated(args, ",");
                    tokens.append(")");
                }
                ConstExpr::Binary(op, ref left, ref right) => {
                    left.to_tokens(tokens);
                    op.to_tokens(tokens);
                    right.to_tokens(tokens);
                }
                ConstExpr::Unary(op, ref expr) => {
                    op.to_tokens(tokens);
                    expr.to_tokens(tokens);
                }
                ConstExpr::Lit(ref lit) => lit.to_tokens(tokens),
                ConstExpr::Cast(ref expr, ref ty) => {
                    expr.to_tokens(tokens);
                    tokens.append("as");
                    ty.to_tokens(tokens);
                }
                ConstExpr::Path(ref path) => path.to_tokens(tokens),
                ConstExpr::Index(ref expr, ref index) => {
                    expr.to_tokens(tokens);
                    tokens.append("[");
                    index.to_tokens(tokens);
                    tokens.append("]");
                }
                ConstExpr::Paren(ref expr) => {
                    tokens.append("(");
                    expr.to_tokens(tokens);
                    tokens.append(")");
                }
                ConstExpr::Other(ref other) => {
                    other.to_tokens(tokens);
                }
            }
        }
    }

    #[cfg(not(feature = "full"))]
    impl ToTokens for Other {
        fn to_tokens(&self, _tokens: &mut Tokens) {
            unreachable!()
        }
    }
}
