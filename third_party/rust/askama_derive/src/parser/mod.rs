use std::cell::Cell;
use std::str;

use nom::branch::alt;
use nom::bytes::complete::{escaped, is_not, tag, take_till};
use nom::character::complete::char;
use nom::character::complete::{anychar, digit1};
use nom::combinator::{eof, map, not, opt, recognize, value};
use nom::error::ErrorKind;
use nom::multi::separated_list1;
use nom::sequence::{delimited, pair, tuple};
use nom::{error_position, AsChar, IResult, InputTakeAtPosition};

pub(crate) use self::expr::Expr;
pub(crate) use self::node::{Cond, CondTest, Loop, Macro, Node, Target, When, Whitespace, Ws};
use crate::config::Syntax;
use crate::CompileError;

mod expr;
mod node;
#[cfg(test)]
mod tests;

struct State<'a> {
    syntax: &'a Syntax<'a>,
    loop_depth: Cell<usize>,
}

impl<'a> State<'a> {
    fn new(syntax: &'a Syntax<'a>) -> State<'a> {
        State {
            syntax,
            loop_depth: Cell::new(0),
        }
    }

    fn enter_loop(&self) {
        self.loop_depth.set(self.loop_depth.get() + 1);
    }

    fn leave_loop(&self) {
        self.loop_depth.set(self.loop_depth.get() - 1);
    }

    fn is_in_loop(&self) -> bool {
        self.loop_depth.get() > 0
    }
}

impl From<char> for Whitespace {
    fn from(c: char) -> Self {
        match c {
            '+' => Self::Preserve,
            '-' => Self::Suppress,
            '~' => Self::Minimize,
            _ => panic!("unsupported `Whitespace` conversion"),
        }
    }
}

pub(crate) fn parse<'a>(
    src: &'a str,
    syntax: &'a Syntax<'_>,
) -> Result<Vec<Node<'a>>, CompileError> {
    match Node::parse(src, &State::new(syntax)) {
        Ok((left, res)) => {
            if !left.is_empty() {
                Err(format!("unable to parse template:\n\n{left:?}").into())
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
                _ => format!("{source_after:?}"),
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

fn keyword<'a>(k: &'a str) -> impl FnMut(&'a str) -> IResult<&'a str, &'a str> {
    move |i: &'a str| -> IResult<&'a str, &'a str> {
        let (j, v) = identifier(i)?;
        if k == v {
            Ok((j, v))
        } else {
            Err(nom::Err::Error(error_position!(i, ErrorKind::Tag)))
        }
    }
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
    alt((keyword("false"), keyword("true")))(i)
}

fn num_lit(i: &str) -> IResult<&str, &str> {
    recognize(pair(digit1, opt(pair(char('.'), digit1))))(i)
}

fn str_lit(i: &str) -> IResult<&str, &str> {
    let (i, s) = delimited(
        char('"'),
        opt(escaped(is_not("\\\""), '\\', anychar)),
        char('"'),
    )(i)?;
    Ok((i, s.unwrap_or_default()))
}

fn char_lit(i: &str) -> IResult<&str, &str> {
    let (i, s) = delimited(
        char('\''),
        opt(escaped(is_not("\\\'"), '\\', anychar)),
        char('\''),
    )(i)?;
    Ok((i, s.unwrap_or_default()))
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
