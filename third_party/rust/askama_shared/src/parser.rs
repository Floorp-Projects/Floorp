use std::cell::Cell;
use std::str;

use nom::branch::alt;
use nom::bytes::complete::{escaped, is_not, tag, take_till, take_until};
use nom::character::complete::{anychar, char, digit1};
use nom::combinator::{complete, consumed, cut, eof, map, not, opt, peek, recognize, value};
use nom::error::{Error, ErrorKind};
use nom::multi::{fold_many0, many0, many1, separated_list0, separated_list1};
use nom::sequence::{delimited, pair, preceded, terminated, tuple};
use nom::{self, error_position, AsChar, IResult, InputTakeAtPosition};

use crate::{CompileError, Syntax};

#[derive(Debug, PartialEq)]
pub enum Node<'a> {
    Lit(&'a str, &'a str, &'a str),
    Comment(Ws),
    Expr(Ws, Expr<'a>),
    Call(Ws, Option<&'a str>, &'a str, Vec<Expr<'a>>),
    LetDecl(Ws, Target<'a>),
    Let(Ws, Target<'a>, Expr<'a>),
    Cond(Vec<Cond<'a>>, Ws),
    Match(Ws, Expr<'a>, Vec<When<'a>>, Ws),
    Loop(Loop<'a>),
    Extends(Expr<'a>),
    BlockDef(Ws, &'a str, Vec<Node<'a>>, Ws),
    Include(Ws, &'a str),
    Import(Ws, &'a str, &'a str),
    Macro(&'a str, Macro<'a>),
    Raw(Ws, &'a str, &'a str, &'a str, Ws),
    Break(Ws),
    Continue(Ws),
}

#[derive(Debug, PartialEq)]
pub struct Loop<'a> {
    pub ws1: Ws,
    pub var: Target<'a>,
    pub iter: Expr<'a>,
    pub cond: Option<Expr<'a>>,
    pub body: Vec<Node<'a>>,
    pub ws2: Ws,
    pub else_block: Vec<Node<'a>>,
    pub ws3: Ws,
}

#[derive(Debug, PartialEq)]
pub enum Expr<'a> {
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
    /// Returns `true` if enough assumptions can be made,
    /// to determine that `self` is copyable.
    pub fn is_copyable(&self) -> bool {
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
    pub fn is_attr_self(&self) -> bool {
        match self {
            Expr::Attr(obj, _) if matches!(obj.as_ref(), Expr::Var("self")) => true,
            Expr::Attr(obj, _) if matches!(obj.as_ref(), Expr::Attr(..)) => obj.is_attr_self(),
            _ => false,
        }
    }
}

pub type When<'a> = (Ws, Target<'a>, Vec<Node<'a>>);

#[derive(Debug, PartialEq)]
pub struct Macro<'a> {
    pub ws1: Ws,
    pub args: Vec<&'a str>,
    pub nodes: Vec<Node<'a>>,
    pub ws2: Ws,
}

#[derive(Debug, PartialEq)]
pub enum Target<'a> {
    Name(&'a str),
    Tuple(Vec<&'a str>, Vec<Target<'a>>),
    Struct(Vec<&'a str>, Vec<(&'a str, Target<'a>)>),
    NumLit(&'a str),
    StrLit(&'a str),
    CharLit(&'a str),
    BoolLit(&'a str),
    Path(Vec<&'a str>),
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Ws(pub bool, pub bool);

pub type Cond<'a> = (Ws, Option<CondTest<'a>>, Vec<Node<'a>>);

#[derive(Debug, PartialEq)]
pub struct CondTest<'a> {
    pub target: Option<Target<'a>>,
    pub expr: Expr<'a>,
}

fn is_ws(c: char) -> bool {
    matches!(c, ' ' | '\t' | '\r' | '\n')
}

fn not_ws(c: char) -> bool {
    !is_ws(c)
}

fn ws<'a, O>(
    inner: impl FnMut(&'a str) -> IResult<&'a str, O>,
) -> impl FnMut(&'a str) -> IResult<&'a str, O> {
    delimited(take_till(not_ws), inner, take_till(not_ws))
}

fn split_ws_parts(s: &str) -> Node<'_> {
    let trimmed_start = s.trim_start_matches(is_ws);
    let len_start = s.len() - trimmed_start.len();
    let trimmed = trimmed_start.trim_end_matches(is_ws);
    Node::Lit(&s[..len_start], trimmed, &trimmed_start[trimmed.len()..])
}

/// Skips input until `end` was found, but does not consume it.
/// Returns tuple that would be returned when parsing `end`.
fn skip_till<'a, O>(
    end: impl FnMut(&'a str) -> IResult<&'a str, O>,
) -> impl FnMut(&'a str) -> IResult<&'a str, (&'a str, O)> {
    enum Next<O> {
        IsEnd(O),
        NotEnd(char),
    }
    let mut next = alt((map(end, Next::IsEnd), map(anychar, Next::NotEnd)));
    move |start: &'a str| {
        let mut i = start;
        loop {
            let (j, is_end) = next(i)?;
            match is_end {
                Next::IsEnd(lookahead) => return Ok((i, (j, lookahead))),
                Next::NotEnd(_) => i = j,
            }
        }
    }
}

struct State<'a> {
    syntax: &'a Syntax<'a>,
    loop_depth: Cell<usize>,
}

fn take_content<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Node<'a>> {
    let p_start = alt((
        tag(s.syntax.block_start),
        tag(s.syntax.comment_start),
        tag(s.syntax.expr_start),
    ));

    let (i, _) = not(eof)(i)?;
    let (i, content) = opt(recognize(skip_till(p_start)))(i)?;
    let (i, content) = match content {
        Some("") => {
            // {block,comment,expr}_start follows immediately.
            return Err(nom::Err::Error(error_position!(i, ErrorKind::TakeUntil)));
        }
        Some(content) => (i, content),
        None => ("", i), // there is no {block,comment,expr}_start: take everything
    };
    Ok((i, split_ws_parts(content)))
}

fn identifier(input: &str) -> IResult<&str, &str> {
    recognize(pair(identifier_start, opt(identifier_tail)))(input)
}

fn identifier_start(s: &str) -> IResult<&str, &str> {
    s.split_at_position1_complete(
        |c| !(c.is_alpha() || c == '_' || c >= '\u{0080}'),
        nom::error::ErrorKind::Alpha,
    )
}

fn identifier_tail(s: &str) -> IResult<&str, &str> {
    s.split_at_position1_complete(
        |c| !(c.is_alphanum() || c == '_' || c >= '\u{0080}'),
        nom::error::ErrorKind::Alpha,
    )
}

fn bool_lit(i: &str) -> IResult<&str, &str> {
    alt((tag("false"), tag("true")))(i)
}

fn expr_bool_lit(i: &str) -> IResult<&str, Expr<'_>> {
    map(bool_lit, Expr::BoolLit)(i)
}

fn variant_bool_lit(i: &str) -> IResult<&str, Target<'_>> {
    map(bool_lit, Target::BoolLit)(i)
}

fn num_lit(i: &str) -> IResult<&str, &str> {
    recognize(pair(digit1, opt(pair(char('.'), digit1))))(i)
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

fn variant_num_lit(i: &str) -> IResult<&str, Target<'_>> {
    map(num_lit, Target::NumLit)(i)
}

fn str_lit(i: &str) -> IResult<&str, &str> {
    let (i, s) = delimited(
        char('"'),
        opt(escaped(is_not("\\\""), '\\', anychar)),
        char('"'),
    )(i)?;
    Ok((i, s.unwrap_or_default()))
}

fn expr_str_lit(i: &str) -> IResult<&str, Expr<'_>> {
    map(str_lit, Expr::StrLit)(i)
}

fn variant_str_lit(i: &str) -> IResult<&str, Target<'_>> {
    map(str_lit, Target::StrLit)(i)
}

fn char_lit(i: &str) -> IResult<&str, &str> {
    let (i, s) = delimited(
        char('\''),
        opt(escaped(is_not("\\\'"), '\\', anychar)),
        char('\''),
    )(i)?;
    Ok((i, s.unwrap_or_default()))
}

fn expr_char_lit(i: &str) -> IResult<&str, Expr<'_>> {
    map(char_lit, Expr::CharLit)(i)
}

fn variant_char_lit(i: &str) -> IResult<&str, Target<'_>> {
    map(char_lit, Target::CharLit)(i)
}

fn expr_var(i: &str) -> IResult<&str, Expr<'_>> {
    map(identifier, Expr::Var)(i)
}

fn path(i: &str) -> IResult<&str, Vec<&str>> {
    let root = opt(value("", ws(tag("::"))));
    let tail = separated_list1(ws(tag("::")), identifier);

    match tuple((root, identifier, ws(tag("::")), tail))(i) {
        Ok((i, (root, start, _, rest))) => {
            let mut path = Vec::new();
            path.extend(root);
            path.push(start);
            path.extend(rest);
            Ok((i, path))
        }
        Err(err) => {
            if let Ok((i, name)) = identifier(i) {
                // The returned identifier can be assumed to be path if:
                // - Contains both a lowercase and uppercase character, i.e. a type name like `None`
                // - Doesn't contain any lowercase characters, i.e. it's a constant
                // In short, if it contains any uppercase characters it's a path.
                if name.contains(char::is_uppercase) {
                    return Ok((i, vec![name]));
                }
            }

            // If `identifier()` fails then just return the original error
            Err(err)
        }
    }
}

fn expr_path(i: &str) -> IResult<&str, Expr<'_>> {
    let (i, path) = path(i)?;
    Ok((i, Expr::Path(path)))
}

fn named_target(i: &str) -> IResult<&str, (&str, Target<'_>)> {
    let (i, (src, target)) = pair(identifier, opt(preceded(ws(char(':')), target)))(i)?;
    Ok((i, (src, target.unwrap_or(Target::Name(src)))))
}

fn variant_lit(i: &str) -> IResult<&str, Target<'_>> {
    alt((
        variant_str_lit,
        variant_char_lit,
        variant_num_lit,
        variant_bool_lit,
    ))(i)
}

fn target(i: &str) -> IResult<&str, Target<'_>> {
    let mut opt_opening_paren = map(opt(ws(char('('))), |o| o.is_some());
    let mut opt_closing_paren = map(opt(ws(char(')'))), |o| o.is_some());
    let mut opt_opening_brace = map(opt(ws(char('{'))), |o| o.is_some());

    let (i, lit) = opt(variant_lit)(i)?;
    if let Some(lit) = lit {
        return Ok((i, lit));
    }

    // match tuples and unused parentheses
    let (i, target_is_tuple) = opt_opening_paren(i)?;
    if target_is_tuple {
        let (i, is_empty_tuple) = opt_closing_paren(i)?;
        if is_empty_tuple {
            return Ok((i, Target::Tuple(Vec::new(), Vec::new())));
        }

        let (i, first_target) = target(i)?;
        let (i, is_unused_paren) = opt_closing_paren(i)?;
        if is_unused_paren {
            return Ok((i, first_target));
        }

        let mut targets = vec![first_target];
        let (i, _) = cut(tuple((
            fold_many0(
                preceded(ws(char(',')), target),
                || (),
                |_, target| {
                    targets.push(target);
                },
            ),
            opt(ws(char(','))),
            ws(cut(char(')'))),
        )))(i)?;
        return Ok((i, Target::Tuple(Vec::new(), targets)));
    }

    // match structs
    let (i, path) = opt(path)(i)?;
    if let Some(path) = path {
        let i_before_matching_with = i;
        let (i, _) = opt(ws(tag("with")))(i)?;

        let (i, is_unnamed_struct) = opt_opening_paren(i)?;
        if is_unnamed_struct {
            let (i, targets) = alt((
                map(char(')'), |_| Vec::new()),
                terminated(
                    cut(separated_list1(ws(char(',')), target)),
                    pair(opt(ws(char(','))), ws(cut(char(')')))),
                ),
            ))(i)?;
            return Ok((i, Target::Tuple(path, targets)));
        }

        let (i, is_named_struct) = opt_opening_brace(i)?;
        if is_named_struct {
            let (i, targets) = alt((
                map(char('}'), |_| Vec::new()),
                terminated(
                    cut(separated_list1(ws(char(',')), named_target)),
                    pair(opt(ws(char(','))), ws(cut(char('}')))),
                ),
            ))(i)?;
            return Ok((i, Target::Struct(path, targets)));
        }

        return Ok((i_before_matching_with, Target::Path(path)));
    }

    // neither literal nor struct nor path
    map(identifier, Target::Name)(i)
}

fn arguments(i: &str) -> IResult<&str, Vec<Expr<'_>>> {
    delimited(
        ws(char('(')),
        separated_list0(char(','), ws(expr_any)),
        ws(char(')')),
    )(i)
}

fn macro_arguments(i: &str) -> IResult<&str, &str> {
    delimited(char('('), recognize(nested_parenthesis), char(')'))(i)
}

fn nested_parenthesis(i: &str) -> IResult<&str, ()> {
    let mut nested = 0;
    let mut last = 0;
    let mut in_str = false;
    let mut escaped = false;

    for (i, b) in i.chars().enumerate() {
        if !(b == '(' || b == ')') || !in_str {
            match b {
                '(' => nested += 1,
                ')' => {
                    if nested == 0 {
                        last = i;
                        break;
                    }
                    nested -= 1;
                }
                '"' => {
                    if in_str {
                        if !escaped {
                            in_str = false;
                        }
                    } else {
                        in_str = true;
                    }
                }
                '\\' => {
                    escaped = !escaped;
                }
                _ => (),
            }
        }

        if escaped && b != '\\' {
            escaped = false;
        }
    }

    if nested == 0 {
        Ok((&i[last..], ()))
    } else {
        Err(nom::Err::Error(error_position!(
            i,
            ErrorKind::SeparatedNonEmptyList
        )))
    }
}

fn parameters(i: &str) -> IResult<&str, Vec<&str>> {
    delimited(
        ws(char('(')),
        separated_list0(char(','), ws(identifier)),
        ws(char(')')),
    )(i)
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

fn expr_node<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Node<'a>> {
    let mut p = tuple((
        |i| tag_expr_start(i, s),
        cut(tuple((opt(char('-')), ws(expr_any), opt(char('-')), |i| {
            tag_expr_end(i, s)
        }))),
    ));
    let (i, (_, (pws, expr, nws, _))) = p(i)?;
    Ok((i, Node::Expr(Ws(pws.is_some(), nws.is_some()), expr)))
}

fn block_call(i: &str) -> IResult<&str, Node<'_>> {
    let mut p = tuple((
        opt(char('-')),
        ws(tag("call")),
        cut(tuple((
            opt(tuple((ws(identifier), ws(tag("::"))))),
            ws(identifier),
            ws(arguments),
            opt(char('-')),
        ))),
    ));
    let (i, (pws, _, (scope, name, args, nws))) = p(i)?;
    let scope = scope.map(|(scope, _)| scope);
    Ok((
        i,
        Node::Call(Ws(pws.is_some(), nws.is_some()), scope, name, args),
    ))
}

fn cond_if(i: &str) -> IResult<&str, CondTest<'_>> {
    let mut p = preceded(
        ws(tag("if")),
        cut(tuple((
            opt(delimited(
                ws(alt((tag("let"), tag("set")))),
                ws(target),
                ws(char('=')),
            )),
            ws(expr_any),
        ))),
    );
    let (i, (target, expr)) = p(i)?;
    Ok((i, CondTest { target, expr }))
}

fn cond_block<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Cond<'a>> {
    let mut p = tuple((
        |i| tag_block_start(i, s),
        opt(char('-')),
        ws(tag("else")),
        cut(tuple((
            opt(cond_if),
            opt(char('-')),
            |i| tag_block_end(i, s),
            cut(|i| parse_template(i, s)),
        ))),
    ));
    let (i, (_, pws, _, (cond, nws, _, block))) = p(i)?;
    Ok((i, (Ws(pws.is_some(), nws.is_some()), cond, block)))
}

fn block_if<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Node<'a>> {
    let mut p = tuple((
        opt(char('-')),
        cond_if,
        cut(tuple((
            opt(char('-')),
            |i| tag_block_end(i, s),
            cut(tuple((
                |i| parse_template(i, s),
                many0(|i| cond_block(i, s)),
                cut(tuple((
                    |i| tag_block_start(i, s),
                    opt(char('-')),
                    ws(tag("endif")),
                    opt(char('-')),
                ))),
            ))),
        ))),
    ));
    let (i, (pws1, cond, (nws1, _, (block, elifs, (_, pws2, _, nws2))))) = p(i)?;

    let mut res = vec![(Ws(pws1.is_some(), nws1.is_some()), Some(cond), block)];
    res.extend(elifs);
    Ok((i, Node::Cond(res, Ws(pws2.is_some(), nws2.is_some()))))
}

fn match_else_block<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, When<'a>> {
    let mut p = tuple((
        |i| tag_block_start(i, s),
        opt(char('-')),
        ws(tag("else")),
        cut(tuple((
            opt(char('-')),
            |i| tag_block_end(i, s),
            cut(|i| parse_template(i, s)),
        ))),
    ));
    let (i, (_, pws, _, (nws, _, block))) = p(i)?;
    Ok((
        i,
        (Ws(pws.is_some(), nws.is_some()), Target::Name("_"), block),
    ))
}

fn when_block<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, When<'a>> {
    let mut p = tuple((
        |i| tag_block_start(i, s),
        opt(char('-')),
        ws(tag("when")),
        cut(tuple((
            ws(target),
            opt(char('-')),
            |i| tag_block_end(i, s),
            cut(|i| parse_template(i, s)),
        ))),
    ));
    let (i, (_, pws, _, (target, nws, _, block))) = p(i)?;
    Ok((i, (Ws(pws.is_some(), nws.is_some()), target, block)))
}

fn block_match<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Node<'a>> {
    let mut p = tuple((
        opt(char('-')),
        ws(tag("match")),
        cut(tuple((
            ws(expr_any),
            opt(char('-')),
            |i| tag_block_end(i, s),
            cut(tuple((
                ws(many0(ws(value((), |i| block_comment(i, s))))),
                many1(|i| when_block(i, s)),
                cut(tuple((
                    opt(|i| match_else_block(i, s)),
                    cut(tuple((
                        ws(|i| tag_block_start(i, s)),
                        opt(char('-')),
                        ws(tag("endmatch")),
                        opt(char('-')),
                    ))),
                ))),
            ))),
        ))),
    ));
    let (i, (pws1, _, (expr, nws1, _, (_, arms, (else_arm, (_, pws2, _, nws2)))))) = p(i)?;

    let mut arms = arms;
    if let Some(arm) = else_arm {
        arms.push(arm);
    }

    Ok((
        i,
        Node::Match(
            Ws(pws1.is_some(), nws1.is_some()),
            expr,
            arms,
            Ws(pws2.is_some(), nws2.is_some()),
        ),
    ))
}

fn block_let(i: &str) -> IResult<&str, Node<'_>> {
    let mut p = tuple((
        opt(char('-')),
        ws(alt((tag("let"), tag("set")))),
        cut(tuple((
            ws(target),
            opt(tuple((ws(char('=')), ws(expr_any)))),
            opt(char('-')),
        ))),
    ));
    let (i, (pws, _, (var, val, nws))) = p(i)?;

    Ok((
        i,
        if let Some((_, val)) = val {
            Node::Let(Ws(pws.is_some(), nws.is_some()), var, val)
        } else {
            Node::LetDecl(Ws(pws.is_some(), nws.is_some()), var)
        },
    ))
}

fn parse_loop_content<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Vec<Node<'a>>> {
    s.loop_depth.set(s.loop_depth.get() + 1);
    let result = parse_template(i, s);
    s.loop_depth.set(s.loop_depth.get() - 1);
    result
}

fn block_for<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Node<'a>> {
    let if_cond = preceded(ws(tag("if")), cut(ws(expr_any)));
    let else_block = |i| {
        let mut p = preceded(
            ws(tag("else")),
            cut(tuple((
                opt(tag("-")),
                delimited(
                    |i| tag_block_end(i, s),
                    |i| parse_template(i, s),
                    |i| tag_block_start(i, s),
                ),
                opt(tag("-")),
            ))),
        );
        let (i, (pws, nodes, nws)) = p(i)?;
        Ok((i, (pws.is_some(), nodes, nws.is_some())))
    };
    let mut p = tuple((
        opt(char('-')),
        ws(tag("for")),
        cut(tuple((
            ws(target),
            ws(tag("in")),
            cut(tuple((
                ws(expr_any),
                opt(if_cond),
                opt(char('-')),
                |i| tag_block_end(i, s),
                cut(tuple((
                    |i| parse_loop_content(i, s),
                    cut(tuple((
                        |i| tag_block_start(i, s),
                        opt(char('-')),
                        opt(else_block),
                        ws(tag("endfor")),
                        opt(char('-')),
                    ))),
                ))),
            ))),
        ))),
    ));
    let (i, (pws1, _, (var, _, (iter, cond, nws1, _, (body, (_, pws2, else_block, _, nws2)))))) =
        p(i)?;
    let (nws3, else_block, pws3) = else_block.unwrap_or_default();
    Ok((
        i,
        Node::Loop(Loop {
            ws1: Ws(pws1.is_some(), nws1.is_some()),
            var,
            iter,
            cond,
            body,
            ws2: Ws(pws2.is_some(), nws3),
            else_block,
            ws3: Ws(pws3, nws2.is_some()),
        }),
    ))
}

fn block_extends(i: &str) -> IResult<&str, Node<'_>> {
    let (i, (_, name)) = tuple((ws(tag("extends")), ws(expr_str_lit)))(i)?;
    Ok((i, Node::Extends(name)))
}

fn block_block<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Node<'a>> {
    let mut start = tuple((
        opt(char('-')),
        ws(tag("block")),
        cut(tuple((ws(identifier), opt(char('-')), |i| {
            tag_block_end(i, s)
        }))),
    ));
    let (i, (pws1, _, (name, nws1, _))) = start(i)?;

    let mut end = cut(tuple((
        |i| parse_template(i, s),
        cut(tuple((
            |i| tag_block_start(i, s),
            opt(char('-')),
            ws(tag("endblock")),
            cut(tuple((opt(ws(tag(name))), opt(char('-'))))),
        ))),
    )));
    let (i, (contents, (_, pws2, _, (_, nws2)))) = end(i)?;

    Ok((
        i,
        Node::BlockDef(
            Ws(pws1.is_some(), nws1.is_some()),
            name,
            contents,
            Ws(pws2.is_some(), nws2.is_some()),
        ),
    ))
}

fn block_include(i: &str) -> IResult<&str, Node<'_>> {
    let mut p = tuple((
        opt(char('-')),
        ws(tag("include")),
        cut(pair(ws(str_lit), opt(char('-')))),
    ));
    let (i, (pws, _, (name, nws))) = p(i)?;
    Ok((i, Node::Include(Ws(pws.is_some(), nws.is_some()), name)))
}

fn block_import(i: &str) -> IResult<&str, Node<'_>> {
    let mut p = tuple((
        opt(char('-')),
        ws(tag("import")),
        cut(tuple((
            ws(str_lit),
            ws(tag("as")),
            cut(pair(ws(identifier), opt(char('-')))),
        ))),
    ));
    let (i, (pws, _, (name, _, (scope, nws)))) = p(i)?;
    Ok((
        i,
        Node::Import(Ws(pws.is_some(), nws.is_some()), name, scope),
    ))
}

fn block_macro<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Node<'a>> {
    let mut p = tuple((
        opt(char('-')),
        ws(tag("macro")),
        cut(tuple((
            ws(identifier),
            ws(parameters),
            opt(char('-')),
            |i| tag_block_end(i, s),
            cut(tuple((
                |i| parse_template(i, s),
                cut(tuple((
                    |i| tag_block_start(i, s),
                    opt(char('-')),
                    ws(tag("endmacro")),
                    opt(char('-')),
                ))),
            ))),
        ))),
    ));

    let (i, (pws1, _, (name, params, nws1, _, (contents, (_, pws2, _, nws2))))) = p(i)?;
    assert_ne!(name, "super", "invalid macro name 'super'");

    Ok((
        i,
        Node::Macro(
            name,
            Macro {
                ws1: Ws(pws1.is_some(), nws1.is_some()),
                args: params,
                nodes: contents,
                ws2: Ws(pws2.is_some(), nws2.is_some()),
            },
        ),
    ))
}

fn block_raw<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Node<'a>> {
    let endraw = tuple((
        |i| tag_block_start(i, s),
        opt(char('-')),
        ws(tag("endraw")),
        opt(char('-')),
        peek(|i| tag_block_end(i, s)),
    ));

    let mut p = tuple((
        opt(char('-')),
        ws(tag("raw")),
        cut(tuple((
            opt(char('-')),
            |i| tag_block_end(i, s),
            consumed(skip_till(endraw)),
        ))),
    ));

    let (_, (pws1, _, (nws1, _, (contents, (i, (_, pws2, _, nws2, _)))))) = p(i)?;
    let (lws, val, rws) = match split_ws_parts(contents) {
        Node::Lit(lws, val, rws) => (lws, val, rws),
        _ => unreachable!(),
    };
    let ws1 = Ws(pws1.is_some(), nws1.is_some());
    let ws2 = Ws(pws2.is_some(), nws2.is_some());
    Ok((i, Node::Raw(ws1, lws, val, rws, ws2)))
}

fn break_statement<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Node<'a>> {
    let mut p = tuple((opt(char('-')), ws(tag("break")), opt(char('-'))));
    let (j, (pws, _, nws)) = p(i)?;
    if s.loop_depth.get() == 0 {
        return Err(nom::Err::Failure(error_position!(i, ErrorKind::Tag)));
    }
    Ok((j, Node::Break(Ws(pws.is_some(), nws.is_some()))))
}

fn continue_statement<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Node<'a>> {
    let mut p = tuple((opt(char('-')), ws(tag("continue")), opt(char('-'))));
    let (j, (pws, _, nws)) = p(i)?;
    if s.loop_depth.get() == 0 {
        return Err(nom::Err::Failure(error_position!(i, ErrorKind::Tag)));
    }
    Ok((j, Node::Continue(Ws(pws.is_some(), nws.is_some()))))
}

fn block_node<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Node<'a>> {
    let mut p = tuple((
        |i| tag_block_start(i, s),
        alt((
            block_call,
            block_let,
            |i| block_if(i, s),
            |i| block_for(i, s),
            |i| block_match(i, s),
            block_extends,
            block_include,
            block_import,
            |i| block_block(i, s),
            |i| block_macro(i, s),
            |i| block_raw(i, s),
            |i| break_statement(i, s),
            |i| continue_statement(i, s),
        )),
        cut(|i| tag_block_end(i, s)),
    ));
    let (i, (_, contents, _)) = p(i)?;
    Ok((i, contents))
}

fn block_comment_body<'a>(mut i: &'a str, s: &State<'_>) -> IResult<&'a str, &'a str> {
    let mut level = 0;
    loop {
        let (end, tail) = take_until(s.syntax.comment_end)(i)?;
        match take_until::<_, _, Error<_>>(s.syntax.comment_start)(i) {
            Ok((start, _)) if start.as_ptr() < end.as_ptr() => {
                level += 1;
                i = &start[2..];
            }
            _ if level > 0 => {
                level -= 1;
                i = &end[2..];
            }
            _ => return Ok((end, tail)),
        }
    }
}

fn block_comment<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Node<'a>> {
    let mut p = tuple((
        |i| tag_comment_start(i, s),
        cut(tuple((
            opt(char('-')),
            |i| block_comment_body(i, s),
            |i| tag_comment_end(i, s),
        ))),
    ));
    let (i, (_, (pws, tail, _))) = p(i)?;
    Ok((i, Node::Comment(Ws(pws.is_some(), tail.ends_with('-')))))
}

fn parse_template<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, Vec<Node<'a>>> {
    many0(alt((
        complete(|i| take_content(i, s)),
        complete(|i| block_comment(i, s)),
        complete(|i| expr_node(i, s)),
        complete(|i| block_node(i, s)),
    )))(i)
}

fn tag_block_start<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, &'a str> {
    tag(s.syntax.block_start)(i)
}
fn tag_block_end<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, &'a str> {
    tag(s.syntax.block_end)(i)
}
fn tag_comment_start<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, &'a str> {
    tag(s.syntax.comment_start)(i)
}
fn tag_comment_end<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, &'a str> {
    tag(s.syntax.comment_end)(i)
}
fn tag_expr_start<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, &'a str> {
    tag(s.syntax.expr_start)(i)
}
fn tag_expr_end<'a>(i: &'a str, s: &State<'_>) -> IResult<&'a str, &'a str> {
    tag(s.syntax.expr_end)(i)
}

pub fn parse<'a>(src: &'a str, syntax: &'a Syntax<'a>) -> Result<Vec<Node<'a>>, CompileError> {
    let state = State {
        syntax,
        loop_depth: Cell::new(0),
    };
    match parse_template(src, &state) {
        Ok((left, res)) => {
            if !left.is_empty() {
                Err(format!("unable to parse template:\n\n{:?}", left).into())
            } else {
                Ok(res)
            }
        }

        Err(nom::Err::Error(err)) | Err(nom::Err::Failure(err)) => {
            let nom::error::Error { input, .. } = err;
            let offset = src.len() - input.len();
            let (source_before, source_after) = src.split_at(offset);

            let source_after = match source_after.char_indices().enumerate().take(41).last() {
                Some((40, (i, _))) => format!("{:?}...", &source_after[..i]),
                _ => format!("{:?}", source_after),
            };

            let (row, last_line) = source_before.lines().enumerate().last().unwrap();
            let column = last_line.chars().count();

            let msg = format!(
                "problems parsing template source at row {}, column {} near:\n{}",
                row + 1,
                column,
                source_after,
            );
            Err(msg.into())
        }

        Err(nom::Err::Incomplete(_)) => Err("parsing incomplete".into()),
    }
}

#[cfg(test)]
mod tests {
    use super::{Expr, Node, Ws};
    use crate::Syntax;

    fn check_ws_split(s: &str, res: &(&str, &str, &str)) {
        match super::split_ws_parts(s) {
            Node::Lit(lws, s, rws) => {
                assert_eq!(lws, res.0);
                assert_eq!(s, res.1);
                assert_eq!(rws, res.2);
            }
            _ => {
                panic!("fail");
            }
        }
    }

    #[test]
    fn test_ws_splitter() {
        check_ws_split("", &("", "", ""));
        check_ws_split("a", &("", "a", ""));
        check_ws_split("\ta", &("\t", "a", ""));
        check_ws_split("b\n", &("", "b", "\n"));
        check_ws_split(" \t\r\n", &(" \t\r\n", "", ""));
    }

    #[test]
    #[should_panic]
    fn test_invalid_block() {
        super::parse("{% extend \"blah\" %}", &Syntax::default()).unwrap();
    }

    #[test]
    fn test_parse_filter() {
        use Expr::*;
        let syntax = Syntax::default();
        assert_eq!(
            super::parse("{{ strvar|e }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Filter("e", vec![Var("strvar")]),
            )],
        );
        assert_eq!(
            super::parse("{{ 2|abs }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Filter("abs", vec![NumLit("2")]),
            )],
        );
        assert_eq!(
            super::parse("{{ -2|abs }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Filter("abs", vec![Unary("-", NumLit("2").into())]),
            )],
        );
        assert_eq!(
            super::parse("{{ (1 - 2)|abs }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Filter(
                    "abs",
                    vec![Group(
                        BinOp("-", NumLit("1").into(), NumLit("2").into()).into()
                    )]
                ),
            )],
        );
    }

    #[test]
    fn test_parse_numbers() {
        let syntax = Syntax::default();
        assert_eq!(
            super::parse("{{ 2 }}", &syntax).unwrap(),
            vec![Node::Expr(Ws(false, false), Expr::NumLit("2"),)],
        );
        assert_eq!(
            super::parse("{{ 2.5 }}", &syntax).unwrap(),
            vec![Node::Expr(Ws(false, false), Expr::NumLit("2.5"),)],
        );
    }

    #[test]
    fn test_parse_var() {
        let s = Syntax::default();

        assert_eq!(
            super::parse("{{ foo }}", &s).unwrap(),
            vec![Node::Expr(Ws(false, false), Expr::Var("foo"))],
        );
        assert_eq!(
            super::parse("{{ foo_bar }}", &s).unwrap(),
            vec![Node::Expr(Ws(false, false), Expr::Var("foo_bar"))],
        );

        assert_eq!(
            super::parse("{{ none }}", &s).unwrap(),
            vec![Node::Expr(Ws(false, false), Expr::Var("none"))],
        );
    }

    #[test]
    fn test_parse_const() {
        let s = Syntax::default();

        assert_eq!(
            super::parse("{{ FOO }}", &s).unwrap(),
            vec![Node::Expr(Ws(false, false), Expr::Path(vec!["FOO"]))],
        );
        assert_eq!(
            super::parse("{{ FOO_BAR }}", &s).unwrap(),
            vec![Node::Expr(Ws(false, false), Expr::Path(vec!["FOO_BAR"]))],
        );

        assert_eq!(
            super::parse("{{ NONE }}", &s).unwrap(),
            vec![Node::Expr(Ws(false, false), Expr::Path(vec!["NONE"]))],
        );
    }

    #[test]
    fn test_parse_path() {
        let s = Syntax::default();

        assert_eq!(
            super::parse("{{ None }}", &s).unwrap(),
            vec![Node::Expr(Ws(false, false), Expr::Path(vec!["None"]))],
        );
        assert_eq!(
            super::parse("{{ Some(123) }}", &s).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Expr::Call(
                    Box::new(Expr::Path(vec!["Some"])),
                    vec![Expr::NumLit("123")]
                ),
            )],
        );

        assert_eq!(
            super::parse("{{ Ok(123) }}", &s).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Expr::Call(Box::new(Expr::Path(vec!["Ok"])), vec![Expr::NumLit("123")]),
            )],
        );
        assert_eq!(
            super::parse("{{ Err(123) }}", &s).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Expr::Call(Box::new(Expr::Path(vec!["Err"])), vec![Expr::NumLit("123")]),
            )],
        );
    }

    #[test]
    fn test_parse_var_call() {
        assert_eq!(
            super::parse("{{ function(\"123\", 3) }}", &Syntax::default()).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Expr::Call(
                    Box::new(Expr::Var("function")),
                    vec![Expr::StrLit("123"), Expr::NumLit("3")]
                ),
            )],
        );
    }

    #[test]
    fn test_parse_path_call() {
        let s = Syntax::default();

        assert_eq!(
            super::parse("{{ Option::None }}", &s).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Expr::Path(vec!["Option", "None"])
            )],
        );
        assert_eq!(
            super::parse("{{ Option::Some(123) }}", &s).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Expr::Call(
                    Box::new(Expr::Path(vec!["Option", "Some"])),
                    vec![Expr::NumLit("123")],
                ),
            )],
        );

        assert_eq!(
            super::parse("{{ self::function(\"123\", 3) }}", &s).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Expr::Call(
                    Box::new(Expr::Path(vec!["self", "function"])),
                    vec![Expr::StrLit("123"), Expr::NumLit("3")],
                ),
            )],
        );
    }

    #[test]
    fn test_parse_root_path() {
        let syntax = Syntax::default();
        assert_eq!(
            super::parse("{{ std::string::String::new() }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Expr::Call(
                    Box::new(Expr::Path(vec!["std", "string", "String", "new"])),
                    vec![]
                ),
            )],
        );
        assert_eq!(
            super::parse("{{ ::std::string::String::new() }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Expr::Call(
                    Box::new(Expr::Path(vec!["", "std", "string", "String", "new"])),
                    vec![]
                ),
            )],
        );
    }

    #[test]
    fn change_delimiters_parse_filter() {
        let syntax = Syntax {
            expr_start: "{~",
            expr_end: "~}",
            ..Syntax::default()
        };

        super::parse("{~ strvar|e ~}", &syntax).unwrap();
    }

    #[test]
    fn test_precedence() {
        use Expr::*;
        let syntax = Syntax::default();
        assert_eq!(
            super::parse("{{ a + b == c }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                BinOp(
                    "==",
                    BinOp("+", Var("a").into(), Var("b").into()).into(),
                    Var("c").into(),
                )
            )],
        );
        assert_eq!(
            super::parse("{{ a + b * c - d / e }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                BinOp(
                    "-",
                    BinOp(
                        "+",
                        Var("a").into(),
                        BinOp("*", Var("b").into(), Var("c").into()).into(),
                    )
                    .into(),
                    BinOp("/", Var("d").into(), Var("e").into()).into(),
                )
            )],
        );
        assert_eq!(
            super::parse("{{ a * (b + c) / -d }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                BinOp(
                    "/",
                    BinOp(
                        "*",
                        Var("a").into(),
                        Group(BinOp("+", Var("b").into(), Var("c").into()).into()).into()
                    )
                    .into(),
                    Unary("-", Var("d").into()).into()
                )
            )],
        );
        assert_eq!(
            super::parse("{{ a || b && c || d && e }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                BinOp(
                    "||",
                    BinOp(
                        "||",
                        Var("a").into(),
                        BinOp("&&", Var("b").into(), Var("c").into()).into(),
                    )
                    .into(),
                    BinOp("&&", Var("d").into(), Var("e").into()).into(),
                )
            )],
        );
    }

    #[test]
    fn test_associativity() {
        use Expr::*;
        let syntax = Syntax::default();
        assert_eq!(
            super::parse("{{ a + b + c }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                BinOp(
                    "+",
                    BinOp("+", Var("a").into(), Var("b").into()).into(),
                    Var("c").into()
                )
            )],
        );
        assert_eq!(
            super::parse("{{ a * b * c }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                BinOp(
                    "*",
                    BinOp("*", Var("a").into(), Var("b").into()).into(),
                    Var("c").into()
                )
            )],
        );
        assert_eq!(
            super::parse("{{ a && b && c }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                BinOp(
                    "&&",
                    BinOp("&&", Var("a").into(), Var("b").into()).into(),
                    Var("c").into()
                )
            )],
        );
        assert_eq!(
            super::parse("{{ a + b - c + d }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                BinOp(
                    "+",
                    BinOp(
                        "-",
                        BinOp("+", Var("a").into(), Var("b").into()).into(),
                        Var("c").into()
                    )
                    .into(),
                    Var("d").into()
                )
            )],
        );
        assert_eq!(
            super::parse("{{ a == b != c > d > e == f }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                BinOp(
                    "==",
                    BinOp(
                        ">",
                        BinOp(
                            ">",
                            BinOp(
                                "!=",
                                BinOp("==", Var("a").into(), Var("b").into()).into(),
                                Var("c").into()
                            )
                            .into(),
                            Var("d").into()
                        )
                        .into(),
                        Var("e").into()
                    )
                    .into(),
                    Var("f").into()
                )
            )],
        );
    }

    #[test]
    fn test_odd_calls() {
        use Expr::*;
        let syntax = Syntax::default();
        assert_eq!(
            super::parse("{{ a[b](c) }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Call(
                    Box::new(Index(Box::new(Var("a")), Box::new(Var("b")))),
                    vec![Var("c")],
                ),
            )],
        );
        assert_eq!(
            super::parse("{{ (a + b)(c) }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Call(
                    Box::new(Group(Box::new(BinOp(
                        "+",
                        Box::new(Var("a")),
                        Box::new(Var("b"))
                    )))),
                    vec![Var("c")],
                ),
            )],
        );
        assert_eq!(
            super::parse("{{ a + b(c) }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                BinOp(
                    "+",
                    Box::new(Var("a")),
                    Box::new(Call(Box::new(Var("b")), vec![Var("c")])),
                ),
            )],
        );
        assert_eq!(
            super::parse("{{ (-a)(b) }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Call(
                    Box::new(Group(Box::new(Unary("-", Box::new(Var("a")))))),
                    vec![Var("b")],
                ),
            )],
        );
        assert_eq!(
            super::parse("{{ -a(b) }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Unary("-", Box::new(Call(Box::new(Var("a")), vec![Var("b")])),),
            )],
        );
    }

    #[test]
    fn test_parse_comments() {
        let s = &Syntax::default();

        assert_eq!(
            super::parse("{##}", s).unwrap(),
            vec![Node::Comment(Ws(false, false))],
        );
        assert_eq!(
            super::parse("{#- #}", s).unwrap(),
            vec![Node::Comment(Ws(true, false))],
        );
        assert_eq!(
            super::parse("{# -#}", s).unwrap(),
            vec![Node::Comment(Ws(false, true))],
        );
        assert_eq!(
            super::parse("{#--#}", s).unwrap(),
            vec![Node::Comment(Ws(true, true))],
        );

        assert_eq!(
            super::parse("{#- foo\n bar -#}", s).unwrap(),
            vec![Node::Comment(Ws(true, true))],
        );
        assert_eq!(
            super::parse("{#- foo\n {#- bar\n -#} baz -#}", s).unwrap(),
            vec![Node::Comment(Ws(true, true))],
        );
        assert_eq!(
            super::parse("{# foo {# bar #} {# {# baz #} qux #} #}", s).unwrap(),
            vec![Node::Comment(Ws(false, false))],
        );
    }

    #[test]
    fn test_parse_tuple() {
        use super::Expr::*;
        let syntax = Syntax::default();
        assert_eq!(
            super::parse("{{ () }}", &syntax).unwrap(),
            vec![Node::Expr(Ws(false, false), Tuple(vec![]),)],
        );
        assert_eq!(
            super::parse("{{ (1) }}", &syntax).unwrap(),
            vec![Node::Expr(Ws(false, false), Group(Box::new(NumLit("1"))),)],
        );
        assert_eq!(
            super::parse("{{ (1,) }}", &syntax).unwrap(),
            vec![Node::Expr(Ws(false, false), Tuple(vec![NumLit("1")]),)],
        );
        assert_eq!(
            super::parse("{{ (1, ) }}", &syntax).unwrap(),
            vec![Node::Expr(Ws(false, false), Tuple(vec![NumLit("1")]),)],
        );
        assert_eq!(
            super::parse("{{ (1 ,) }}", &syntax).unwrap(),
            vec![Node::Expr(Ws(false, false), Tuple(vec![NumLit("1")]),)],
        );
        assert_eq!(
            super::parse("{{ (1 , ) }}", &syntax).unwrap(),
            vec![Node::Expr(Ws(false, false), Tuple(vec![NumLit("1")]),)],
        );
        assert_eq!(
            super::parse("{{ (1, 2) }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Tuple(vec![NumLit("1"), NumLit("2")]),
            )],
        );
        assert_eq!(
            super::parse("{{ (1, 2,) }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Tuple(vec![NumLit("1"), NumLit("2")]),
            )],
        );
        assert_eq!(
            super::parse("{{ (1, 2, 3) }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Tuple(vec![NumLit("1"), NumLit("2"), NumLit("3")]),
            )],
        );
        assert_eq!(
            super::parse("{{ ()|abs }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Filter("abs", vec![Tuple(vec![])]),
            )],
        );
        assert_eq!(
            super::parse("{{ () | abs }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                BinOp("|", Box::new(Tuple(vec![])), Box::new(Var("abs"))),
            )],
        );
        assert_eq!(
            super::parse("{{ (1)|abs }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Filter("abs", vec![Group(Box::new(NumLit("1")))]),
            )],
        );
        assert_eq!(
            super::parse("{{ (1) | abs }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                BinOp(
                    "|",
                    Box::new(Group(Box::new(NumLit("1")))),
                    Box::new(Var("abs"))
                ),
            )],
        );
        assert_eq!(
            super::parse("{{ (1,)|abs }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Filter("abs", vec![Tuple(vec![NumLit("1")])]),
            )],
        );
        assert_eq!(
            super::parse("{{ (1,) | abs }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                BinOp(
                    "|",
                    Box::new(Tuple(vec![NumLit("1")])),
                    Box::new(Var("abs"))
                ),
            )],
        );
        assert_eq!(
            super::parse("{{ (1, 2)|abs }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                Filter("abs", vec![Tuple(vec![NumLit("1"), NumLit("2")])]),
            )],
        );
        assert_eq!(
            super::parse("{{ (1, 2) | abs }}", &syntax).unwrap(),
            vec![Node::Expr(
                Ws(false, false),
                BinOp(
                    "|",
                    Box::new(Tuple(vec![NumLit("1"), NumLit("2")])),
                    Box::new(Var("abs"))
                ),
            )],
        );
    }
}
