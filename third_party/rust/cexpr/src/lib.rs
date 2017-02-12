// (C) Copyright 2016 Jethro G. Beekman
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#[macro_use]
extern crate nom as nom_crate;

pub mod nom {
    //! nom's result types, re-exported.
    pub use nom_crate::{IResult,Needed,Err,ErrorKind};
}
pub mod literal;
pub mod expr;
pub mod token;

use nom::*;

#[derive(Debug)]
/// Parsing errors specific to C parsing
pub enum Error {
    /// Expected the specified token
	ExactToken(token::Kind,&'static [u8]),
    /// Expected a token of the specified kind
	TypedToken(token::Kind),
    /// An unknown identifier was encountered
	UnknownIdentifier,
    /// An invalid literal was encountered.
    ///
    /// When encountered, this generally means a bug exists in the data that
    /// was passed in or the parsing logic.
	InvalidLiteral,
    /// A full parse was requested, but data was left over after parsing finished.
    Partial,
}

macro_rules! identity (
    ($i:expr,$e:expr) => ($e);
);

/// If the input result indicates a succesful parse, but there is data left,
/// return an `Error::Partial` instead.
pub fn assert_full_parse<I,O,E>(result: IResult<&[I],O,E>) -> IResult<&[I],O,::Error> {
	match fix_error!((),::Error,identity!(result)) {
		IResult::Done(rem,output) => if rem.len()==0 {
			IResult::Done(rem, output)
		} else {
			IResult::Error(Err::Position(ErrorKind::Custom(::Error::Partial), rem))
		},
		r => r,
	}
}
