use std::str;

use nom::branch::alt;
use nom::bytes::complete::{tag, take_till};
use nom::character::complete::char;
use nom::combinator::{cut, map, not, opt, peek, recognize};
use nom::multi::{fold_many0, many0, separated_list0, separated_list1};
use nom::sequence::{delimited, pair, preceded, terminated, tuple};
use nom::IResult;

use super::{
    bool_lit, char_lit, identifier, nested_parenthesis, not_ws, num_lit, path, str_lit, ws,
};

#[derive(Debug, PartialEq)]
pub(crate) enum Expr<'a> {
    BoolLit(&'a str),
    NumLit(&'a str),
    StrLit(&'a str),
    CharLit(&'a str),
    Var(&'a str),
    Path(Vec<&'a str>),
    Array(Vec<Expr<'a>>),
    Attr(Box<Expr<'a>>, &'a str),
    Index(Box<Expr<'a>>, Box<Expr<'a>>),
    Filter(&'a str, Vec<Expr<'a>>),
    Unary(&'a str, Box<Expr<'a>>),
    BinOp(&'a str, Box<Expr<'a>>, Box<Expr<'a>>),
    Range(&'a str, Option<Box<Expr<'a>>>, Option<Box<Expr<'a>>>),
    Group(Box<Expr<'a>>),
    Tuple(Vec<Expr<'a>>),
    Call(Box<Expr<'a>>, Vec<Expr<'a>>),
    RustMacro(&'a str, &'a str),
    Try(Box<Expr<'a>>),
}

impl Expr<'_> {
    pub(super) fn parse(i: &str) -> IResult<&str, Expr<'_>> {
        expr_any(i)
    }

    pub(super) fn parse_arguments(i: &str) -> IResult<&str, Vec<Expr<'_>>> {
        arguments(i)
    }

    /// Returns `true` if enough assumptions can be made,
    /// to determine that `self` is copyable.
    pub(crate) fn is_copyable(&self) -> bool {
        self.is_copyable_within_op(false)
    }

    fn is_copyable_within_op(&self, within_op: bool) -> bool {
        use Expr::*;
        match self {
            BoolLit(_) | NumLit(_) | StrLit(_) | CharLit(_) => true,
            Unary(.., expr) => expr.is_copyable_within_op(true),
            BinOp(_, lhs, rhs) => {
                lhs.is_copyable_within_op(true) && rhs.is_copyable_within_op(true)
            }
            Range(..) => true,
            // The result of a call likely doesn't need to be borrowed,
            // as in that case the call is more likely to return a
            // reference in the first place then.
            Call(..) | Path(..) => true,
            // If the `expr` is within a `Unary` or `BinOp` then
            // an assumption can be made that the operand is copy.
            // If not, then the value is moved and adding `.clone()`
            // will solve that issue. However, if the operand is
            // implicitly borrowed, then it's likely not even possible
            // to get the template to compile.
            _ => within_op && self.is_attr_self(),
        }
    }

    /// Returns `true` if this is an `Attr` where the `obj` is `"self"`.
    pub(crate) fn is_attr_self(&self) -> bool {
        match self {
            Expr::Attr(obj, _) if matches!(obj.as_ref(), Expr::Var("self")) => true,
            Expr::Attr(obj, _) if matches!(obj.as_ref(), Expr::Attr(..)) => obj.is_attr_self(),
            _ => false,
        }
    }

    /// Returns `true` if the outcome of this expression may be used multiple times in the same
    /// `write!()` call, without evaluating the expression again, i.e. the expression should be
    /// side-effect free.
    pub(crate) fn is_cacheable(&self) -> bool {
        match self {
            // Literals are the definition of pure:
            Expr::BoolLit(_) => true,
            Expr::NumLit(_) => true,
            Expr::StrLit(_) => true,
            Expr::CharLit(_) => true,
            // fmt::Display should have no effects:
            Expr::Var(_) => true,
            Expr::Path(_) => true,
            // Check recursively:
            Expr::Array(args) => args.iter().all(|arg| arg.is_cacheable()),
            Expr::Attr(lhs, _) => lhs.is_cacheable(),
            Expr::Index(lhs, rhs) => lhs.is_cacheable() && rhs.is_cacheable(),
            Expr::Filter(_, args) => args.iter().all(|arg| arg.is_cacheable()),
            Expr::Unary(_, arg) => arg.is_cacheable(),
            Expr::BinOp(_, lhs, rhs) => lhs.is_cacheable() && rhs.is_cacheable(),
            Expr::Range(_, lhs, rhs) => {
                lhs.as_ref().map_or(true, |v| v.is_cacheable())
                    && rhs.as_ref().map_or(true, |v| v.is_cacheable())
            }
            Expr::Group(arg) => arg.is_cacheable(),
            Expr::Tuple(args) => args.iter().all(|arg| arg.is_cacheable()),
            // We have too little information to tell if the expression is pure:
            Expr::Call(_, _) => false,
            Expr::RustMacro(_, _) => false,
            Expr::Try(_) => false,
        }
    }
}

fn expr_bool_lit(i: &str) -> IResult<&str, Expr<'_>> {
    map(bool_lit, Expr::BoolLit)(i)
}

fn expr_num_lit(i: &str) -> IResult<&str, Expr<'_>> {
    map(num_lit, Expr::NumLit)(i)
}

fn expr_array_lit(i: &str) -> IResult<&str, Expr<'_>> {
    delimited(
        ws(char('[')),
        map(separated_list1(ws(char(',')), expr_any), Expr::Array),
        ws(char(']')),
    )(i)
}

fn expr_str_lit(i: &str) -> IResult<&str, Expr<'_>> {
    map(str_lit, Expr::StrLit)(i)
}

fn expr_char_lit(i: &str) -> IResult<&str, Expr<'_>> {
    map(char_lit, Expr::CharLit)(i)
}

fn expr_var(i: &str) -> IResult<&str, Expr<'_>> {
    map(identifier, Expr::Var)(i)
}

fn expr_path(i: &str) -> IResult<&str, Expr<'_>> {
    let (i, path) = path(i)?;
    Ok((i, Expr::Path(path)))
}

fn expr_group(i: &str) -> IResult<&str, Expr<'_>> {
    let (i, expr) = preceded(ws(char('(')), opt(expr_any))(i)?;
    let expr = match expr {
        Some(expr) => expr,
        None => {
            let (i, _) = char(')')(i)?;
            return Ok((i, Expr::Tuple(vec![])));
        }
    };

    let (i, comma) = ws(opt(peek(char(','))))(i)?;
    if comma.is_none() {
        let (i, _) = char(')')(i)?;
        return Ok((i, Expr::Group(Box::new(expr))));
    }

    let mut exprs = vec![expr];
    let (i, _) = fold_many0(
        preceded(char(','), ws(expr_any)),
        || (),
        |_, expr| {
            exprs.push(expr);
        },
    )(i)?;
    let (i, _) = pair(ws(opt(char(','))), char(')'))(i)?;
    Ok((i, Expr::Tuple(exprs)))
}

fn expr_single(i: &str) -> IResult<&str, Expr<'_>> {
    alt((
        expr_bool_lit,
        expr_num_lit,
        expr_str_lit,
        expr_char_lit,
        expr_path,
        expr_rust_macro,
        expr_array_lit,
        expr_var,
        expr_group,
    ))(i)
}

enum Suffix<'a> {
    Attr(&'a str),
    Index(Expr<'a>),
    Call(Vec<Expr<'a>>),
    Try,
}

fn expr_attr(i: &str) -> IResult<&str, Suffix<'_>> {
    map(
        preceded(
            ws(pair(char('.'), not(char('.')))),
            cut(alt((num_lit, identifier))),
        ),
        Suffix::Attr,
    )(i)
}

fn expr_index(i: &str) -> IResult<&str, Suffix<'_>> {
    map(
        preceded(ws(char('[')), cut(terminated(expr_any, ws(char(']'))))),
        Suffix::Index,
    )(i)
}

fn expr_call(i: &str) -> IResult<&str, Suffix<'_>> {
    map(arguments, Suffix::Call)(i)
}

fn expr_try(i: &str) -> IResult<&str, Suffix<'_>> {
    map(preceded(take_till(not_ws), char('?')), |_| Suffix::Try)(i)
}

fn filter(i: &str) -> IResult<&str, (&str, Option<Vec<Expr<'_>>>)> {
    let (i, (_, fname, args)) = tuple((char('|'), ws(identifier), opt(arguments)))(i)?;
    Ok((i, (fname, args)))
}

fn expr_filtered(i: &str) -> IResult<&str, Expr<'_>> {
    let (i, (obj, filters)) = tuple((expr_prefix, many0(filter)))(i)?;

    let mut res = obj;
    for (fname, args) in filters {
        res = Expr::Filter(fname, {
            let mut args = match args {
                Some(inner) => inner,
                None => Vec::new(),
            };
            args.insert(0, res);
            args
        });
    }

    Ok((i, res))
}

fn expr_prefix(i: &str) -> IResult<&str, Expr<'_>> {
    let (i, (ops, mut expr)) = pair(many0(ws(alt((tag("!"), tag("-"))))), expr_suffix)(i)?;
    for op in ops.iter().rev() {
        expr = Expr::Unary(op, Box::new(expr));
    }
    Ok((i, expr))
}

fn expr_suffix(i: &str) -> IResult<&str, Expr<'_>> {
    let (mut i, mut expr) = expr_single(i)?;
    loop {
        let (j, suffix) = opt(alt((expr_attr, expr_index, expr_call, expr_try)))(i)?;
        i = j;
        match suffix {
            Some(Suffix::Attr(attr)) => expr = Expr::Attr(expr.into(), attr),
            Some(Suffix::Index(index)) => expr = Expr::Index(expr.into(), index.into()),
            Some(Suffix::Call(args)) => expr = Expr::Call(expr.into(), args),
            Some(Suffix::Try) => expr = Expr::Try(expr.into()),
            None => break,
        }
    }
    Ok((i, expr))
}

fn macro_arguments(i: &str) -> IResult<&str, &str> {
    delimited(char('('), recognize(nested_parenthesis), char(')'))(i)
}

fn expr_rust_macro(i: &str) -> IResult<&str, Expr<'_>> {
    let (i, (mname, _, args)) = tuple((identifier, char('!'), macro_arguments))(i)?;
    Ok((i, Expr::RustMacro(mname, args)))
}

macro_rules! expr_prec_layer {
    ( $name:ident, $inner:ident, $op:expr ) => {
        fn $name(i: &str) -> IResult<&str, Expr<'_>> {
            let (i, left) = $inner(i)?;
            let (i, right) = many0(pair(
                ws(tag($op)),
                $inner,
            ))(i)?;
            Ok((
                i,
                right.into_iter().fold(left, |left, (op, right)| {
                    Expr::BinOp(op, Box::new(left), Box::new(right))
                }),
            ))
        }
    };
    ( $name:ident, $inner:ident, $( $op:expr ),+ ) => {
        fn $name(i: &str) -> IResult<&str, Expr<'_>> {
            let (i, left) = $inner(i)?;
            let (i, right) = many0(pair(
                ws(alt(($( tag($op) ),+,))),
                $inner,
            ))(i)?;
            Ok((
                i,
                right.into_iter().fold(left, |left, (op, right)| {
                    Expr::BinOp(op, Box::new(left), Box::new(right))
                }),
            ))
        }
    }
}

expr_prec_layer!(expr_muldivmod, expr_filtered, "*", "/", "%");
expr_prec_layer!(expr_addsub, expr_muldivmod, "+", "-");
expr_prec_layer!(expr_shifts, expr_addsub, ">>", "<<");
expr_prec_layer!(expr_band, expr_shifts, "&");
expr_prec_layer!(expr_bxor, expr_band, "^");
expr_prec_layer!(expr_bor, expr_bxor, "|");
expr_prec_layer!(expr_compare, expr_bor, "==", "!=", ">=", ">", "<=", "<");
expr_prec_layer!(expr_and, expr_compare, "&&");
expr_prec_layer!(expr_or, expr_and, "||");

fn expr_any(i: &str) -> IResult<&str, Expr<'_>> {
    let range_right = |i| pair(ws(alt((tag("..="), tag("..")))), opt(expr_or))(i);
    alt((
        map(range_right, |(op, right)| {
            Expr::Range(op, None, right.map(Box::new))
        }),
        map(
            pair(expr_or, opt(range_right)),
            |(left, right)| match right {
                Some((op, right)) => Expr::Range(op, Some(Box::new(left)), right.map(Box::new)),
                None => left,
            },
        ),
    ))(i)
}

fn arguments(i: &str) -> IResult<&str, Vec<Expr<'_>>> {
    delimited(
        ws(char('(')),
        separated_list0(char(','), ws(expr_any)),
        ws(char(')')),
    )(i)
}
