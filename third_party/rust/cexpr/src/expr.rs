// (C) Copyright 2016 Jethro G. Beekman
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//! Evaluating C expressions from tokens.
//!
//! Numerical operators are supported. All numerical values are treated as
//! `i64` or `f64`. Type casting is not supported. `i64` are converted to
//! `f64` when used in conjunction with a `f64`. Right shifts are always
//! arithmetic shifts.
//!
//! The `sizeof` operator is not supported.
//!
//! String concatenation is supported, but width prefixes are ignored; all
//! strings are treated as narrow strings.
//!
//! Use the `IdentifierParser` to substitute identifiers found in expressions.

use std::collections::HashMap;
use std::ops::{AddAssign,BitAndAssign,BitOrAssign,BitXorAssign,DivAssign,MulAssign,RemAssign,ShlAssign,ShrAssign,SubAssign};
use std::num::Wrapping;

use literal::{self,CChar};
use token::{Token,Kind as TokenKind};
use nom_crate::*;

/// Expression parser/evaluator that supports identifiers.
#[derive(Debug)]
pub struct IdentifierParser<'ident> {
	identifiers: &'ident HashMap<Vec<u8>,EvalResult>,
}
#[derive(Copy,Clone)]
struct PRef<'a>(&'a IdentifierParser<'a>);

pub type CResult<'a,R:'a> = IResult<&'a [Token],R,::Error>;

/// The result of parsing a literal or evaluating an expression.
#[derive(Debug,Clone,PartialEq)]
pub enum EvalResult {
	Int(Wrapping<i64>),
	Float(f64),
	Char(CChar),
	Str(Vec<u8>),
	Invalid,
}

macro_rules! result_opt (
	(fn $n:ident: $e:ident -> $t:ty) => (
		#[allow(dead_code)]
		fn $n(self) -> Option<$t> {
			if let EvalResult::$e(v) = self {
				Some(v)
			} else {
				None
			}
		}
	);
);

impl EvalResult {
	result_opt!(fn as_int: Int -> Wrapping<i64>);
	result_opt!(fn as_float: Float -> f64);
	result_opt!(fn as_char: Char -> CChar);
	result_opt!(fn as_str: Str -> Vec<u8>);
	
	fn as_numeric(self) -> Option<EvalResult> {
		match self {
			EvalResult::Int(_) | EvalResult::Float(_) => Some(self),
			_ => None,
		}
	}
}

impl From<Vec<u8>> for EvalResult {
	fn from(s: Vec<u8>) -> EvalResult {
		EvalResult::Str(s)
	}
}

// ===========================================
// ============= Clang tokens ================
// ===========================================

macro_rules! exact_token (
	($i:expr, $k:ident, $c:expr) => ({
		if $i.is_empty() {
			let res: CResult<&[u8]> = IResult::Incomplete(Needed::Size($c.len()));
			res
		} else {
			if $i[0].kind==TokenKind::$k && &$i[0].raw[..]==$c {
				IResult::Done(&$i[1..], &$i[0].raw[..])
			} else {
				IResult::Error(Err::Position(ErrorKind::Custom(::Error::ExactToken(TokenKind::$k,$c)), $i))
			}
		}
	});
);

macro_rules! typed_token (
	($i:expr, $k:ident) => ({
		if $i.is_empty() {
			let res: CResult<&[u8]> = IResult::Incomplete(Needed::Size(1));
			res
		} else {
			if $i[0].kind==TokenKind::$k {
				IResult::Done(&$i[1..], &$i[0].raw[..])
			} else {
				IResult::Error(Err::Position(ErrorKind::Custom(::Error::TypedToken(TokenKind::$k)), $i))
			}
		}
	});
);

#[allow(unused_macros)]
macro_rules! any_token (
	($i:expr,) => ({
		if $i.is_empty() {
			let res: CResult<&Token> = IResult::Incomplete(Needed::Size(1));
			res
		} else {
			IResult::Done(&$i[1..], &$i[0])
		}
	});
);

macro_rules! p (
	($i:expr, $c:expr) => (exact_token!($i,Punctuation,$c.as_bytes()))
);

macro_rules! one_of_punctuation (
	($i:expr, $c:expr) => ({
		if $i.is_empty() {
			let min = $c.iter().map(|opt|opt.len()).min().expect("at least one option");
			let res: CResult<&[u8]> = IResult::Incomplete(Needed::Size(min));
			res
		} else {
			if $i[0].kind==TokenKind::Punctuation && $c.iter().any(|opt|opt.as_bytes()==&$i[0].raw[..]) {
				IResult::Done(&$i[1..], &$i[0].raw[..])
			} else {
				const VAILD_VALUES: &'static [&'static str] = &$c;
				IResult::Error(Err::Position(ErrorKind::Custom(::Error::ExactTokens(TokenKind::Punctuation,VAILD_VALUES)), $i))
			}
		}
	});
);

// ==================================================
// ============= Numeric expressions ================
// ==================================================

impl<'a> AddAssign<&'a EvalResult> for EvalResult {
    fn add_assign(&mut self, rhs: &'a EvalResult) {
		use self::EvalResult::*;
		*self=match (&*self,rhs) {
			(&Int(a),  &Int(b))   => Int(a+b),
			(&Float(a),&Int(b))   => Float(a+(b.0 as f64)),
			(&Int(a),  &Float(b)) => Float(a.0 as f64+b),
			(&Float(a),&Float(b)) => Float(a+b),
			_ => Invalid
		};
	}
}
impl<'a> BitAndAssign<&'a EvalResult> for EvalResult {
    fn bitand_assign(&mut self, rhs: &'a EvalResult) {
		use self::EvalResult::*;
		*self=match (&*self,rhs) {
			(&Int(a),&Int(b)) => Int(a&b),
			_ => Invalid
		};	}
}
impl<'a> BitOrAssign<&'a EvalResult> for EvalResult {
    fn bitor_assign(&mut self, rhs: &'a EvalResult) {
		use self::EvalResult::*;
		*self=match (&*self,rhs) {
			(&Int(a),&Int(b)) => Int(a|b),
			_ => Invalid
		};
	}
}
impl<'a> BitXorAssign<&'a EvalResult> for EvalResult {
    fn bitxor_assign(&mut self, rhs: &'a EvalResult) {
		use self::EvalResult::*;
		*self=match (&*self,rhs) {
			(&Int(a),&Int(b)) => Int(a^b),
			_ => Invalid
		};
	}
}
impl<'a> DivAssign<&'a EvalResult> for EvalResult {
    fn div_assign(&mut self, rhs: &'a EvalResult) {
		use self::EvalResult::*;
		*self=match (&*self,rhs) {
			(&Int(a),  &Int(b))   => Int(a/b),
			(&Float(a),&Int(b))   => Float(a/(b.0 as f64)),
			(&Int(a),  &Float(b)) => Float(a.0 as f64/b),
			(&Float(a),&Float(b)) => Float(a/b),
			_ => Invalid
		};
	}
}
impl<'a> MulAssign<&'a EvalResult> for EvalResult {
    fn mul_assign(&mut self, rhs: &'a EvalResult) {
		use self::EvalResult::*;
		*self=match (&*self,rhs) {
			(&Int(a),  &Int(b))   => Int(a*b),
			(&Float(a),&Int(b))   => Float(a*(b.0 as f64)),
			(&Int(a),  &Float(b)) => Float(a.0 as f64*b),
			(&Float(a),&Float(b)) => Float(a*b),
			_ => Invalid
		};
	}
}
impl<'a> RemAssign<&'a EvalResult> for EvalResult {
    fn rem_assign(&mut self, rhs: &'a EvalResult) {
		use self::EvalResult::*;
		*self=match (&*self,rhs) {
			(&Int(a),  &Int(b))   => Int(a%b),
			(&Float(a),&Int(b))   => Float(a%(b.0 as f64)),
			(&Int(a),  &Float(b)) => Float(a.0 as f64%b),
			(&Float(a),&Float(b)) => Float(a%b),
			_ => Invalid
		};
	}
}
impl<'a> ShlAssign<&'a EvalResult> for EvalResult {
    fn shl_assign(&mut self, rhs: &'a EvalResult) {
		use self::EvalResult::*;
		*self=match (&*self,rhs) {
			(&Int(a),&Int(b)) => Int(a<<(b.0 as usize)),
			_ => Invalid
		};
	}
}
impl<'a> ShrAssign<&'a EvalResult> for EvalResult {
    fn shr_assign(&mut self, rhs: &'a EvalResult) {
		use self::EvalResult::*;
		*self=match (&*self,rhs) {
			(&Int(a),&Int(b)) => Int(a>>(b.0 as usize)),
			_ => Invalid
		};
	}
}
impl<'a> SubAssign<&'a EvalResult> for EvalResult {
    fn sub_assign(&mut self, rhs: &'a EvalResult) {
		use self::EvalResult::*;
		*self=match (&*self,rhs) {
			(&Int(a),  &Int(b))   => Int(a-b),
			(&Float(a),&Int(b))   => Float(a-(b.0 as f64)),
			(&Int(a),  &Float(b)) => Float(a.0 as f64-b),
			(&Float(a),&Float(b)) => Float(a-b),
			_ => Invalid
		};
	}
}

fn unary_op(input: (&[u8],EvalResult)) -> Option<EvalResult> {
	use self::EvalResult::*;
	assert_eq!(input.0.len(),1);
	match (input.0[0],input.1) {
		(b'+',i) => Some(i),
		(b'-',Int(i)) => Some(Int(Wrapping(i.0.wrapping_neg()))), // impl Neg for Wrapping not until rust 1.10...
		(b'-',Float(i)) => Some(Float(-i)),
		(b'-',_) => unreachable!("non-numeric unary op"),
		(b'~',Int(i)) => Some(Int(!i)),
		(b'~',Float(_)) => None,
		(b'~',_) => unreachable!("non-numeric unary op"),
		_ => unreachable!("invalid unary op"),
	}
}

macro_rules! numeric (
	($i:expr, $submac:ident!( $($args:tt)* )) => (map_opt!($i,$submac!($($args)*),EvalResult::as_numeric));
	($i:expr, $f:expr ) => (map_opt!($i,call!($f),EvalResult::as_numeric));
);

impl<'a> PRef<'a> {
	method!(unary<PRef<'a>,&[Token],EvalResult,::Error>, mut self,
		alt!(
			delimited!(p!("("),call_m!(self.numeric_expr),p!(")")) |
			numeric!(call_m!(self.literal)) |
			numeric!(call_m!(self.identifier)) |
			map_opt!(pair!(one_of_punctuation!(["+", "-", "~"]),call_m!(self.unary)),unary_op)
		)
	);

	method!(mul_div_rem<PRef<'a>,&[Token],EvalResult,::Error>, mut self,
		do_parse!(
			acc: call_m!(self.unary) >>
			res: fold_many0!(
				pair!(one_of_punctuation!(["*", "/", "%"]), call_m!(self.unary)),
				acc,
				|mut acc, (op, val): (&[u8], EvalResult)| {
					 match op[0] as char {
						'*' => acc *= &val,
						'/' => acc /= &val,
						'%' => acc %= &val,
						_   => unreachable!()
					};
					acc
				}
			) >> (res)
		)
	);

	method!(add_sub<PRef<'a>,&[Token],EvalResult,::Error>, mut self,
		do_parse!(
			acc: call_m!(self.mul_div_rem) >>
			res: fold_many0!(
				pair!(one_of_punctuation!(["+", "-"]), call_m!(self.mul_div_rem)),
				acc,
				|mut acc, (op, val): (&[u8], EvalResult)| {
					match op[0] as char {
						'+' => acc += &val,
						'-' => acc -= &val,
						_   => unreachable!()
					};
					acc
				}
			) >> (res)
		)
	);

	method!(shl_shr<PRef<'a>,&[Token],EvalResult,::Error>, mut self,
		numeric!(do_parse!(
			acc: call_m!(self.add_sub) >>
			res: fold_many0!(
				pair!(one_of_punctuation!(["<<", ">>"]), call_m!(self.add_sub)),
				acc,
				|mut acc, (op, val): (&[u8], EvalResult)| {
					match op {
						b"<<" => acc <<= &val,
						b">>" => acc >>= &val,
						_     => unreachable!()
					};
					acc
				}
			) >> (res)
		))
	);

	method!(and<PRef<'a>,&[Token],EvalResult,::Error>, mut self,
		numeric!(do_parse!(
			acc: call_m!(self.shl_shr) >>
			res: fold_many0!(
				preceded!(p!("&"), call_m!(self.shl_shr)),
				acc,
				|mut acc, val: EvalResult| {
					acc &= &val;
					acc
				}
			) >> (res)
		))
	);

	method!(xor<PRef<'a>,&[Token],EvalResult,::Error>, mut self,
		numeric!(do_parse!(
			acc: call_m!(self.and) >>
			res: fold_many0!(
				preceded!(p!("^"), call_m!(self.and)),
				acc,
				|mut acc, val: EvalResult| {
					acc ^= &val;
					acc
				}
			) >> (res)
		))
	);

	method!(or<PRef<'a>,&[Token],EvalResult,::Error>, mut self,
		numeric!(do_parse!(
			acc: call_m!(self.xor) >>
			res: fold_many0!(
				preceded!(p!("|"), call_m!(self.xor)),
				acc,
				|mut acc, val: EvalResult| {
					acc |= &val;
					acc
				}
			) >> (res)
		))
	);

	#[inline(always)]
	fn numeric_expr(self, input: &[Token]) -> (Self,CResult<EvalResult>) {
		self.or(input)
	}
}

// =======================================================
// ============= Literals and identifiers ================
// =======================================================

impl<'a> PRef<'a> {
	fn identifier(self, input: &[Token]) -> (Self,CResult<EvalResult>) {
		(self,match input.split_first() {
			None =>
				IResult::Incomplete(Needed::Size(1)),
			Some((&Token{kind:TokenKind::Identifier,ref raw},rest)) => {
				if let Some(r) = self.identifiers.get(&raw[..]) {
					IResult::Done(rest, r.clone())
				} else {
					IResult::Error(Err::Position(ErrorKind::Custom(::Error::UnknownIdentifier), input))
				}
			},
			Some(_) =>
				IResult::Error(Err::Position(ErrorKind::Custom(::Error::TypedToken(TokenKind::Identifier)), input)),
		})
	}

	fn literal(self, input: &[Token]) -> (Self,CResult<EvalResult>) {
		(self,match input.split_first() {
			None =>
				IResult::Incomplete(Needed::Size(1)),
			Some((&Token{kind:TokenKind::Literal,ref raw},rest)) =>
				match literal::parse(raw) {
					IResult::Done(_,result) => IResult::Done(rest, result),
					_ => IResult::Error(Err::Position(ErrorKind::Custom(::Error::InvalidLiteral), input))
				},
			Some(_) =>
				IResult::Error(Err::Position(ErrorKind::Custom(::Error::TypedToken(TokenKind::Literal)), input)),
		})
	}

	method!(string<PRef<'a>,&[Token],Vec<u8>,::Error>, mut self,
		alt!(
			map_opt!(call_m!(self.literal),EvalResult::as_str) |
			map_opt!(call_m!(self.identifier),EvalResult::as_str)
		)
	);

	// "string1" "string2" etc...
	method!(concat_str<PRef<'a>,&[Token],EvalResult,::Error>, mut self,
		map!(
			pair!(call_m!(self.string),many0!(call_m!(self.string))),
			|(first,v)| Vec::into_iter(v).fold(first,|mut s,elem|{Vec::extend_from_slice(&mut s,Vec::<u8>::as_slice(&elem));s}).into()
		)
	);

	method!(expr<PRef<'a>,&[Token],EvalResult,::Error>, mut self,
		alt!(
			call_m!(self.numeric_expr) |
			delimited!(p!("("),call_m!(self.expr),p!(")")) |
			call_m!(self.concat_str) |
			call_m!(self.literal) |
			call_m!(self.identifier)
		)
	);

	method!(macro_definition<PRef<'a>,&[Token],(&[u8],EvalResult),::Error>, mut self,
		pair!(typed_token!(Identifier),call_m!(self.expr))
	);
}

impl<'a> ::std::ops::Deref for PRef<'a> {
	type Target=IdentifierParser<'a>;
	fn deref(&self) -> &IdentifierParser<'a> {
		self.0
	}
}

impl<'ident> IdentifierParser<'ident> {
	fn as_ref(&self) -> PRef {
		PRef(self)
	}
	
	/// Create a new `IdentifierParser` with a set of known identifiers. When
	/// a known identifier is encountered during parsing, it is substituted
	/// for the value specified.
	pub fn new(identifiers: &HashMap<Vec<u8>,EvalResult>) -> IdentifierParser {
		IdentifierParser{identifiers:identifiers}
	}

	/// Parse and evalute an expression of a list of tokens.
	///
	/// Returns an error if the input is not a valid expression or if the token
	/// stream contains comments, keywords or unknown identifiers.
	pub fn expr<'a>(&self,input: &'a [Token]) -> CResult<'a,EvalResult> {
		self.as_ref().expr(input).1
	}

	/// Parse and evaluate a macro definition from of a list of tokens.
	///
	/// Returns the identifier for the macro and its replacement evaluated as an
	/// expression. The input should not include `#define`.
	///
	/// Returns an error if the replacement is not a valid expression, if called
	/// on most function-like macros, or if the token stream contains comments,
	/// keywords or unknown identifiers.
	///
	/// N.B. This is intended to fail on function-like macros, but if it the
	/// macro takes a single argument, the argument name is defined as an
	/// identifier, and the macro otherwise parses as an expression, it will
	/// return a result even on function-like macros.
	///
	/// ```ignore
	/// // will evaluate into IDENTIFIER
	/// #define DELETE(IDENTIFIER)
	/// // will evaluate into IDENTIFIER-3
	/// #define NEGATIVE_THREE(IDENTIFIER)  -3
	/// ```
	pub fn macro_definition<'a>(&self,input: &'a [Token]) -> CResult<'a,(&'a [u8],EvalResult)> {
		::assert_full_parse(self.as_ref().macro_definition(input).1)
	}
}

/// Parse and evalute an expression of a list of tokens.
///
/// Returns an error if the input is not a valid expression or if the token
/// stream contains comments, keywords or identifiers.
pub fn expr<'a>(input: &'a [Token]) -> CResult<'a,EvalResult> {
	IdentifierParser::new(&HashMap::new()).expr(input)
}

/// Parse and evaluate a macro definition from of a list of tokens.
///
/// Returns the identifier for the macro and its replacement evaluated as an
/// expression. The input should not include `#define`.
///
/// Returns an error if the replacement is not a valid expression, if called
/// on a function-like macro, or if the token stream contains comments,
/// keywords or identifiers.
pub fn macro_definition<'a>(input: &'a [Token]) -> CResult<'a,(&'a [u8],EvalResult)> {
	IdentifierParser::new(&HashMap::new()).macro_definition(input)
}

named_attr!(
/// Parse a functional macro declaration from a list of tokens.
///
/// Returns the identifier for the macro and the argument list (in order). The
/// input should not include `#define`. The actual definition is not parsed and
/// may be obtained from the unparsed data returned.
///
/// Returns an error if the input is not a functional macro or if the token
/// stream contains comments.
///
/// # Example
/// ```
/// use cexpr::expr::{IdentifierParser, EvalResult, fn_macro_declaration};
/// use cexpr::assert_full_parse;
/// use cexpr::token::Kind::*;
/// use cexpr::token::Token;
///
/// // #define SUFFIX(arg) arg "suffix"
/// let tokens = vec![
///     (Identifier,  &b"SUFFIX"[..]).into(),
///     (Punctuation, &b"("[..]).into(),
///     (Identifier,  &b"arg"[..]).into(),
///     (Punctuation, &b")"[..]).into(),
///     (Identifier,  &b"arg"[..]).into(),
///     (Literal,     &br#""suffix""#[..]).into(),
/// ];
///
/// // Try to parse the functional part
/// let (expr, (ident, args)) = fn_macro_declaration(&tokens).unwrap();
/// assert_eq!(ident, b"SUFFIX");
///
/// // Create dummy arguments
/// let idents = args.into_iter().map(|arg|
///     (arg.to_owned(), EvalResult::Str(b"test".to_vec()))
/// ).collect();
///
/// // Evaluate the macro
/// let (_, evaluated) = assert_full_parse(IdentifierParser::new(&idents).expr(expr)).unwrap();
/// assert_eq!(evaluated, EvalResult::Str(b"testsuffix".to_vec()));
/// ```
,pub fn_macro_declaration<&[Token],(&[u8],Vec<&[u8]>),::Error>,
	pair!(
		typed_token!(Identifier),
		delimited!(
			p!("("),
			separated_list!(p!(","), typed_token!(Identifier)),
			p!(")")
		)
	)
);
