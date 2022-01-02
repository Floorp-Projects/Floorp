//! parsers recognizing bytes streams, complete input version

use crate::error::ErrorKind;
use crate::error::ParseError;
use crate::internal::{Err, IResult};
use crate::lib::std::ops::RangeFrom;
use crate::lib::std::result::Result::*;
use crate::traits::{Compare, CompareResult, FindSubstring, FindToken, InputIter, InputLength, InputTake, InputTakeAtPosition, Slice, ToUsize};

/// Recognizes a pattern
///
/// The input data will be compared to the tag combinator's argument and will return the part of
/// the input that matches the argument
///
/// It will return `Err(Err::Error((_, ErrorKind::Tag)))` if the input doesn't match the pattern
/// # Example
/// ```rust
/// # #[macro_use] extern crate nom;
/// # use nom::{Err, error::ErrorKind, Needed, IResult};
/// use nom::bytes::complete::tag;
///
/// fn parser(s: &str) -> IResult<&str, &str> {
///   tag("Hello")(s)
/// }
///
/// assert_eq!(parser("Hello, World!"), Ok((", World!", "Hello")));
/// assert_eq!(parser("Something"), Err(Err::Error(("Something", ErrorKind::Tag))));
/// assert_eq!(parser(""), Err(Err::Error(("", ErrorKind::Tag))));
/// ```
pub fn tag<'a, T: 'a, Input: 'a, Error: ParseError<Input>>(tag: T) -> impl Fn(Input) -> IResult<Input, Input, Error>
where
  Input: InputTake + Compare<T>,
  T: InputLength + Clone,
{
  move |i: Input| {
    let tag_len = tag.input_len();
    let t = tag.clone();
    let res: IResult<_, _, Error> = match i.compare(t) {
      CompareResult::Ok => Ok(i.take_split(tag_len)),
      _ => {
        let e: ErrorKind = ErrorKind::Tag;
        Err(Err::Error(Error::from_error_kind(i, e)))
      }
    };
    res
  }
}

/// Recognizes a case insensitive pattern
///
/// The input data will be compared to the tag combinator's argument and will return the part of
/// the input that matches the argument with no regard to case
///
/// It will return `Err(Err::Error((_, ErrorKind::Tag)))` if the input doesn't match the pattern
/// # Example
/// ```rust
/// # #[macro_use] extern crate nom;
/// # use nom::{Err, error::ErrorKind, Needed, IResult};
/// use nom::bytes::complete::tag_no_case;
///
/// fn parser(s: &str) -> IResult<&str, &str> {
///   tag_no_case("hello")(s)
/// }
///
/// assert_eq!(parser("Hello, World!"), Ok((", World!", "Hello")));
/// assert_eq!(parser("hello, World!"), Ok((", World!", "hello")));
/// assert_eq!(parser("HeLlO, World!"), Ok((", World!", "HeLlO")));
/// assert_eq!(parser("Something"), Err(Err::Error(("Something", ErrorKind::Tag))));
/// assert_eq!(parser(""), Err(Err::Error(("", ErrorKind::Tag))));
/// ```
pub fn tag_no_case<T, Input, Error: ParseError<Input>>(tag: T) -> impl Fn(Input) -> IResult<Input, Input, Error>
where
  Input: InputTake + Compare<T>,
  T: InputLength + Clone,
{
  move |i: Input| {
    let tag_len = tag.input_len();
    let t = tag.clone();

    let res: IResult<_, _, Error> = match (i).compare_no_case(t) {
      CompareResult::Ok => Ok(i.take_split(tag_len)),
      _ => {
        let e: ErrorKind = ErrorKind::Tag;
        Err(Err::Error(Error::from_error_kind(i, e)))
      }
    };
    res
  }
}

/// Parse till certain characters are met
///
/// The parser will return the longest slice till one of the characters of the combinator's argument are met.
///
/// It doesn't consume the matched character,
///
/// It will return a `Err::Error(("", ErrorKind::IsNot))` if the pattern wasn't met
/// # Example
/// ```rust
/// # #[macro_use] extern crate nom;
/// # use nom::{Err, error::ErrorKind, Needed, IResult};
/// use nom::bytes::complete::is_not;
///
/// fn not_space(s: &str) -> IResult<&str, &str> {
///   is_not(" \t\r\n")(s)
/// }
///
/// assert_eq!(not_space("Hello, World!"), Ok((" World!", "Hello,")));
/// assert_eq!(not_space("Sometimes\t"), Ok(("\t", "Sometimes")));
/// assert_eq!(not_space("Nospace"), Ok(("", "Nospace")));
/// assert_eq!(not_space(""), Err(Err::Error(("", ErrorKind::IsNot))));
/// ```
pub fn is_not<T, Input, Error: ParseError<Input>>(arr: T) -> impl Fn(Input) -> IResult<Input, Input, Error>
where
  Input: InputTakeAtPosition,
  T: InputLength + FindToken<<Input as InputTakeAtPosition>::Item>,
{
  move |i: Input| {
    let e: ErrorKind = ErrorKind::IsNot;
    i.split_at_position1_complete(|c| arr.find_token(c), e)
  }
}

/// Returns the longest slice of the matches the pattern
///
/// The parser will return the longest slice consisting of the characters in provided in the
/// combinator's argument
///
/// It will return a `Err(Err::Error((_, ErrorKind::IsA)))` if the pattern wasn't met
///
/// # Example
/// ```rust
/// # #[macro_use] extern crate nom;
/// # use nom::{Err, error::ErrorKind, Needed, IResult};
/// use nom::bytes::complete::is_a;
///
/// fn hex(s: &str) -> IResult<&str, &str> {
///   is_a("1234567890ABCDEF")(s)
/// }
///
/// assert_eq!(hex("123 and voila"), Ok((" and voila", "123")));
/// assert_eq!(hex("DEADBEEF and others"), Ok((" and others", "DEADBEEF")));
/// assert_eq!(hex("BADBABEsomething"), Ok(("something", "BADBABE")));
/// assert_eq!(hex("D15EA5E"), Ok(("", "D15EA5E")));
/// assert_eq!(hex(""), Err(Err::Error(("", ErrorKind::IsA))));
/// ```
pub fn is_a<T, Input, Error: ParseError<Input>>(arr: T) -> impl Fn(Input) -> IResult<Input, Input, Error>
where
  Input: InputTakeAtPosition,
  T: InputLength + FindToken<<Input as InputTakeAtPosition>::Item>,
{
  move |i: Input| {
    let e: ErrorKind = ErrorKind::IsA;
    i.split_at_position1_complete(|c| !arr.find_token(c), e)
  }
}

/// Returns the longest input slice (if any) that matches the predicate
///
/// The parser will return the longest slice that matches the given predicate *(a function that
/// takes the input and returns a bool)*
///
/// # Example
/// ```rust
/// # #[macro_use] extern crate nom;
/// # use nom::{Err, error::ErrorKind, Needed, IResult};
/// use nom::bytes::complete::take_while;
/// use nom::character::is_alphabetic;
///
/// fn alpha(s: &[u8]) -> IResult<&[u8], &[u8]> {
///   take_while(is_alphabetic)(s)
/// }
///
/// assert_eq!(alpha(b"latin123"), Ok((&b"123"[..], &b"latin"[..])));
/// assert_eq!(alpha(b"12345"), Ok((&b"12345"[..], &b""[..])));
/// assert_eq!(alpha(b"latin"), Ok((&b""[..], &b"latin"[..])));
/// assert_eq!(alpha(b""), Ok((&b""[..], &b""[..])));
/// ```
pub fn take_while<F, Input, Error: ParseError<Input>>(cond: F) -> impl Fn(Input) -> IResult<Input, Input, Error>
where
  Input: InputTakeAtPosition,
  F: Fn(<Input as InputTakeAtPosition>::Item) -> bool,
{
  move |i: Input| i.split_at_position_complete(|c| !cond(c))
}

/// Returns the longest (atleast 1) input slice that matches the predicate
///
/// The parser will return the longest slice that matches the given predicate *(a function that
/// takes the input and returns a bool)*
///
/// It will return an `Err(Err::Error((_, ErrorKind::TakeWhile1)))` if the pattern wasn't met
///
/// # Example
/// ```rust
/// # #[macro_use] extern crate nom;
/// # use nom::{Err, error::ErrorKind, Needed, IResult};
/// use nom::bytes::complete::take_while1;
/// use nom::character::is_alphabetic;
///
/// fn alpha(s: &[u8]) -> IResult<&[u8], &[u8]> {
///   take_while1(is_alphabetic)(s)
/// }
///
/// assert_eq!(alpha(b"latin123"), Ok((&b"123"[..], &b"latin"[..])));
/// assert_eq!(alpha(b"latin"), Ok((&b""[..], &b"latin"[..])));
/// assert_eq!(alpha(b"12345"), Err(Err::Error((&b"12345"[..], ErrorKind::TakeWhile1))));
/// ```
pub fn take_while1<F, Input, Error: ParseError<Input>>(cond: F) -> impl Fn(Input) -> IResult<Input, Input, Error>
where
  Input: InputTakeAtPosition,
  F: Fn(<Input as InputTakeAtPosition>::Item) -> bool,
{
  move |i: Input| {
    let e: ErrorKind = ErrorKind::TakeWhile1;
    i.split_at_position1_complete(|c| !cond(c), e)
  }
}

/// Returns the longest (m <= len <= n) input slice  that matches the predicate
///
/// The parser will return the longest slice that matches the given predicate *(a function that
/// takes the input and returns a bool)*
///
/// It will return an `Err::Error((_, ErrorKind::TakeWhileMN))` if the pattern wasn't met or is out
/// of range (m <= len <= n)
///
/// # Example
/// ```rust
/// # #[macro_use] extern crate nom;
/// # use nom::{Err, error::ErrorKind, Needed, IResult};
/// use nom::bytes::complete::take_while_m_n;
/// use nom::character::is_alphabetic;
///
/// fn short_alpha(s: &[u8]) -> IResult<&[u8], &[u8]> {
///   take_while_m_n(3, 6, is_alphabetic)(s)
/// }
///
/// assert_eq!(short_alpha(b"latin123"), Ok((&b"123"[..], &b"latin"[..])));
/// assert_eq!(short_alpha(b"lengthy"), Ok((&b"y"[..], &b"length"[..])));
/// assert_eq!(short_alpha(b"latin"), Ok((&b""[..], &b"latin"[..])));
/// assert_eq!(short_alpha(b"ed"), Err(Err::Error((&b"ed"[..], ErrorKind::TakeWhileMN))));
/// assert_eq!(short_alpha(b"12345"), Err(Err::Error((&b"12345"[..], ErrorKind::TakeWhileMN))));
/// ```
pub fn take_while_m_n<F, Input, Error: ParseError<Input>>(m: usize, n: usize, cond: F) -> impl Fn(Input) -> IResult<Input, Input, Error>
where
  Input: InputTake + InputIter + InputLength + Slice<RangeFrom<usize>>,
  F: Fn(<Input as InputIter>::Item) -> bool,
{
  move |i: Input| {
    let input = i;

    match input.position(|c| !cond(c)) {
      Some(idx) => {
        if idx >= m {
          if idx <= n {
            let res: IResult<_, _, Error> = if let Some(index) = input.slice_index(idx) {
              Ok(input.take_split(index))
            } else {
              Err(Err::Error(Error::from_error_kind(input, ErrorKind::TakeWhileMN)))
            };
            res
          } else {
            let res: IResult<_, _, Error> = if let Some(index) = input.slice_index(n) {
              Ok(input.take_split(index))
            } else {
              Err(Err::Error(Error::from_error_kind(input, ErrorKind::TakeWhileMN)))
            };
            res
          }
        } else {
          let e = ErrorKind::TakeWhileMN;
          Err(Err::Error(Error::from_error_kind(input, e)))
        }
      }
      None => {
        let len = input.input_len();
        if len >= n {
          match input.slice_index(n) {
            Some(index) => Ok(input.take_split(index)),
            None => Err(Err::Error(Error::from_error_kind(input, ErrorKind::TakeWhileMN)))
          }
        } else {
          if len >= m && len <= n {
            let res: IResult<_, _, Error> = Ok((input.slice(len..), input));
            res
          } else {
            let e = ErrorKind::TakeWhileMN;
            Err(Err::Error(Error::from_error_kind(input, e)))
          }
        }
      }
    }
  }
}

/// Returns the longest input slice (if any) till a predicate is met
///
/// The parser will return the longest slice till the given predicate *(a function that
/// takes the input and returns a bool)*
///
/// # Example
/// ```rust
/// # #[macro_use] extern crate nom;
/// # use nom::{Err, error::ErrorKind, Needed, IResult};
/// use nom::bytes::complete::take_till;
///
/// fn till_colon(s: &str) -> IResult<&str, &str> {
///   take_till(|c| c == ':')(s)
/// }
///
/// assert_eq!(till_colon("latin:123"), Ok((":123", "latin")));
/// assert_eq!(till_colon(":empty matched"), Ok((":empty matched", ""))); //allowed
/// assert_eq!(till_colon("12345"), Ok(("", "12345")));
/// assert_eq!(till_colon(""), Ok(("", "")));
/// ```
pub fn take_till<F, Input, Error: ParseError<Input>>(cond: F) -> impl Fn(Input) -> IResult<Input, Input, Error>
where
  Input: InputTakeAtPosition,
  F: Fn(<Input as InputTakeAtPosition>::Item) -> bool,
{
  move |i: Input| i.split_at_position_complete(|c| cond(c))
}

/// Returns the longest (atleast 1) input slice till a predicate is met
///
/// The parser will return the longest slice till the given predicate *(a function that
/// takes the input and returns a bool)*
///
/// It will return `Err(Err::Error((_, ErrorKind::TakeTill1)))` if the input is empty or the
/// predicate matches the first input
///
/// # Example
/// ```rust
/// # #[macro_use] extern crate nom;
/// # use nom::{Err, error::ErrorKind, Needed, IResult};
/// use nom::bytes::complete::take_till1;
///
/// fn till_colon(s: &str) -> IResult<&str, &str> {
///   take_till1(|c| c == ':')(s)
/// }
///
/// assert_eq!(till_colon("latin:123"), Ok((":123", "latin")));
/// assert_eq!(till_colon(":empty matched"), Err(Err::Error((":empty matched", ErrorKind::TakeTill1))));
/// assert_eq!(till_colon("12345"), Ok(("", "12345")));
/// assert_eq!(till_colon(""), Err(Err::Error(("", ErrorKind::TakeTill1))));
/// ```
pub fn take_till1<F, Input, Error: ParseError<Input>>(cond: F) -> impl Fn(Input) -> IResult<Input, Input, Error>
where
  Input: InputTakeAtPosition,
  F: Fn(<Input as InputTakeAtPosition>::Item) -> bool,
{
  move |i: Input| {
    let e: ErrorKind = ErrorKind::TakeTill1;
    i.split_at_position1_complete(|c| cond(c), e)
  }
}

/// Returns an input slice containing the first N input elements (Input[..N])
///
/// It will return `Err(Err::Error((_, ErrorKind::Eof)))` if the input is shorter than the argument
///
/// # Example
/// ```rust
/// # #[macro_use] extern crate nom;
/// # use nom::{Err, error::ErrorKind, Needed, IResult};
/// use nom::bytes::complete::take;
///
/// fn take6(s: &str) -> IResult<&str, &str> {
///   take(6usize)(s)
/// }
///
/// assert_eq!(take6("1234567"), Ok(("7", "123456")));
/// assert_eq!(take6("things"), Ok(("", "things")));
/// assert_eq!(take6("short"), Err(Err::Error(("short", ErrorKind::Eof))));
/// assert_eq!(take6(""), Err(Err::Error(("", ErrorKind::Eof))));
/// ```
pub fn take<C, Input, Error: ParseError<Input>>(count: C) -> impl Fn(Input) -> IResult<Input, Input, Error>
where
  Input: InputIter + InputTake,
  C: ToUsize,
{
  let c = count.to_usize();
  move |i: Input| match i.slice_index(c) {
    None => Err(Err::Error(Error::from_error_kind(i, ErrorKind::Eof))),
    Some(index) => Ok(i.take_split(index)),
  }
}

/// Returns the longest input slice till it matches the pattern.
///
/// It doesn't consume the pattern. It will return `Err(Err::Error((_, ErrorKind::TakeUntil)))`
/// if the pattern wasn't met
///
/// # Example
/// ```rust
/// # #[macro_use] extern crate nom;
/// # use nom::{Err, error::ErrorKind, Needed, IResult};
/// use nom::bytes::complete::take_until;
///
/// fn until_eof(s: &str) -> IResult<&str, &str> {
///   take_until("eof")(s)
/// }
///
/// assert_eq!(until_eof("hello, worldeof"), Ok(("eof", "hello, world")));
/// assert_eq!(until_eof("hello, world"), Err(Err::Error(("hello, world", ErrorKind::TakeUntil))));
/// assert_eq!(until_eof(""), Err(Err::Error(("", ErrorKind::TakeUntil))));
/// ```
pub fn take_until<T, Input, Error: ParseError<Input>>(tag: T) -> impl Fn(Input) -> IResult<Input, Input, Error>
where
  Input: InputTake + FindSubstring<T>,
  T: InputLength + Clone,
{
  move |i: Input| {
    let t = tag.clone();
    let res: IResult<_, _, Error> = match i.find_substring(t) {
      None => Err(Err::Error(Error::from_error_kind(i, ErrorKind::TakeUntil))),
      Some(index) => Ok(i.take_split(index)),
    };
    res
  }
}

/// Matches a byte string with escaped characters.
///
/// * The first argument matches the normal characters (it must not accept the control character),
/// * the second argument is the control character (like `\` in most languages),
/// * the third argument matches the escaped characters
///
/// # Example
/// ```
/// # #[macro_use] extern crate nom;
/// # use nom::{Err, error::ErrorKind, Needed, IResult};
/// # use nom::character::complete::digit1;
/// use nom::bytes::complete::escaped;
/// use nom::character::complete::one_of;
///
/// fn esc(s: &str) -> IResult<&str, &str> {
///   escaped(digit1, '\\', one_of(r#""n\"#))(s)
/// }
///
/// assert_eq!(esc("123;"), Ok((";", "123")));
/// assert_eq!(esc(r#"12\"34;"#), Ok((";", r#"12\"34"#)));
/// ```
///
pub fn escaped<Input, Error, F, G, O1, O2>(normal: F, control_char: char, escapable: G) -> impl Fn(Input) -> IResult<Input, Input, Error>
where
  Input: Clone + crate::traits::Offset + InputLength + InputTake + InputTakeAtPosition + Slice<RangeFrom<usize>> + InputIter,
  <Input as InputIter>::Item: crate::traits::AsChar,
  F: Fn(Input) -> IResult<Input, O1, Error>,
  G: Fn(Input) -> IResult<Input, O2, Error>,
  Error: ParseError<Input>,
{
  use crate::traits::AsChar;

  move |input: Input| {
    let mut i = input.clone();

    while i.input_len() > 0 {
      match normal(i.clone()) {
        Ok((i2, _)) => {
          if i2.input_len() == 0 {
            return Ok((input.slice(input.input_len()..), input));
          } else {
            i = i2;
          }
        }
        Err(Err::Error(_)) => {
          // unwrap() should be safe here since index < $i.input_len()
          if i.iter_elements().next().unwrap().as_char() == control_char {
            let next = control_char.len_utf8();
            if next >= i.input_len() {
              return Err(Err::Error(Error::from_error_kind(input, ErrorKind::Escaped)));
            } else {
              match escapable(i.slice(next..)) {
                Ok((i2, _)) => {
                  if i2.input_len() == 0 {
                    return Ok((input.slice(input.input_len()..), input));
                  } else {
                    i = i2;
                  }
                }
                Err(e) => return Err(e),
              }
            }
          } else {
            let index = input.offset(&i);
            if index == 0 {
              return Err(Err::Error(Error::from_error_kind(input, ErrorKind::Escaped)));
            }
            return Ok(input.take_split(index));
          }
        }
        Err(e) => {
          return Err(e);
        }
      }
    }

    Ok((input.slice(input.input_len()..), input))
  }
}

#[doc(hidden)]
pub fn escapedc<Input, Error, F, G, O1, O2>(i: Input, normal: F, control_char: char, escapable: G) -> IResult<Input, Input, Error>
where
  Input: Clone + crate::traits::Offset + InputLength + InputTake + InputTakeAtPosition + Slice<RangeFrom<usize>> + InputIter,
  <Input as InputIter>::Item: crate::traits::AsChar,
  F: Fn(Input) -> IResult<Input, O1, Error>,
  G: Fn(Input) -> IResult<Input, O2, Error>,
  Error: ParseError<Input>,
{
  escaped(normal, control_char, escapable)(i)
}

/// Matches a byte string with escaped characters.
///
/// * The first argument matches the normal characters (it must not match the control character),
/// * the second argument is the control character (like `\` in most languages),
/// * the third argument matches the escaped characters and transforms them.
///
/// As an example, the chain `abc\tdef` could be `abc    def` (it also consumes the control character)
///
/// ```
/// # #[macro_use] extern crate nom;
/// # use nom::{Err, error::ErrorKind, Needed, IResult};
/// # use std::str::from_utf8;
/// use nom::bytes::complete::escaped_transform;
/// use nom::character::complete::alpha1;
///
/// fn parser(input: &str) -> IResult<&str, String> {
///   escaped_transform(
///     alpha1,
///     '\\',
///     |i:&str| alt!(i,
///         tag!("\\")       => { |_| "\\" }
///       | tag!("\"")       => { |_| "\"" }
///       | tag!("n")        => { |_| "\n" }
///     )
///   )(input)
/// }
///
/// assert_eq!(parser("ab\\\"cd"), Ok(("", String::from("ab\"cd"))));
/// ```
#[cfg(feature = "alloc")]
pub fn escaped_transform<Input, Error, F, G, O1, O2, ExtendItem, Output>(
  normal: F,
  control_char: char,
  transform: G,
) -> impl Fn(Input) -> IResult<Input, Output, Error>
where
  Input: Clone + crate::traits::Offset + InputLength + InputTake + InputTakeAtPosition + Slice<RangeFrom<usize>> + InputIter,
  Input: crate::traits::ExtendInto<Item = ExtendItem, Extender = Output>,
  O1: crate::traits::ExtendInto<Item = ExtendItem, Extender = Output>,
  O2: crate::traits::ExtendInto<Item = ExtendItem, Extender = Output>,
  Output: core::iter::Extend<<Input as crate::traits::ExtendInto>::Item>,
  Output: core::iter::Extend<<O1 as crate::traits::ExtendInto>::Item>,
  Output: core::iter::Extend<<O2 as crate::traits::ExtendInto>::Item>,
  <Input as InputIter>::Item: crate::traits::AsChar,
  F: Fn(Input) -> IResult<Input, O1, Error>,
  G: Fn(Input) -> IResult<Input, O2, Error>,
  Error: ParseError<Input>,
{
  use crate::traits::AsChar;

  move |input: Input| {
    let mut index = 0;
    let mut res = input.new_builder();

    let i = input.clone();

    while index < i.input_len() {
      let remainder = i.slice(index..);
      match normal(remainder.clone()) {
        Ok((i2, o)) => {
          o.extend_into(&mut res);
          if i2.input_len() == 0 {
            return Ok((i.slice(i.input_len()..), res));
          } else {
            index = input.offset(&i2);
          }
        }
        Err(Err::Error(_)) => {
          // unwrap() should be safe here since index < $i.input_len()
          if remainder.iter_elements().next().unwrap().as_char() == control_char {
            let next = index + control_char.len_utf8();
            let input_len = input.input_len();

            if next >= input_len {
              return Err(Err::Error(Error::from_error_kind(remainder, ErrorKind::EscapedTransform)));
            } else {
              match transform(i.slice(next..)) {
                Ok((i2, o)) => {
                  o.extend_into(&mut res);
                  if i2.input_len() == 0 {
                    return Ok((i.slice(i.input_len()..), res));
                  } else {
                    index = input.offset(&i2);
                  }
                }
                Err(e) => return Err(e),
              }
            }
          } else {
            if index == 0 {
              return Err(Err::Error(Error::from_error_kind(remainder, ErrorKind::EscapedTransform)));
            }
            return Ok((remainder, res));
          }
        }
        Err(e) => return Err(e),
      }
    }
    Ok((input.slice(index..), res))
  }
}

#[doc(hidden)]
#[cfg(feature = "alloc")]
pub fn escaped_transformc<Input, Error, F, G, O1, O2, ExtendItem, Output>(
  i: Input,
  normal: F,
  control_char: char,
  transform: G,
) -> IResult<Input, Output, Error>
where
  Input: Clone + crate::traits::Offset + InputLength + InputTake + InputTakeAtPosition + Slice<RangeFrom<usize>> + InputIter,
  Input: crate::traits::ExtendInto<Item = ExtendItem, Extender = Output>,
  O1: crate::traits::ExtendInto<Item = ExtendItem, Extender = Output>,
  O2: crate::traits::ExtendInto<Item = ExtendItem, Extender = Output>,
  Output: core::iter::Extend<<Input as crate::traits::ExtendInto>::Item>,
  Output: core::iter::Extend<<O1 as crate::traits::ExtendInto>::Item>,
  Output: core::iter::Extend<<O2 as crate::traits::ExtendInto>::Item>,
  <Input as InputIter>::Item: crate::traits::AsChar,
  F: Fn(Input) -> IResult<Input, O1, Error>,
  G: Fn(Input) -> IResult<Input, O2, Error>,
  Error: ParseError<Input>,
{
  escaped_transform(normal, control_char, transform)(i)

}

#[cfg(test)]
mod tests {
  use super::*;

  #[test]
  fn complete_take_while_m_n_utf8_all_matching() {
    let result: IResult<&str, &str> = super::take_while_m_n(1, 4, |c: char| c.is_alphabetic())("øn");
    assert_eq!(result, Ok(("", "øn")));
  }

  #[test]
  fn complete_take_while_m_n_utf8_all_matching_substring() {
    let result: IResult<&str, &str> = super::take_while_m_n(1, 1, |c: char| c.is_alphabetic())("øn");
    assert_eq!(result, Ok(("n", "ø")));
  }
}
