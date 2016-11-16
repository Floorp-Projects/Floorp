use super::*;

#[derive(Debug, Clone, Eq, PartialEq)]
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
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use {BinOp, Ty};
    use lit::parsing::lit;
    use op::parsing::{binop, unop};
    use ty::parsing::{path, ty};

    named!(pub const_expr -> ConstExpr, do_parse!(
        mut e: alt!(
            expr_lit
            |
            expr_unary
            |
            path => { ConstExpr::Path }
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
            }
        }
    }
}
