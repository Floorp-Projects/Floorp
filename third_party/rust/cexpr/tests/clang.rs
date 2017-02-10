// (C) Copyright 2016 Jethro G. Beekman
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
extern crate cexpr;
extern crate clang_sys;

use std::{ptr,mem,ffi,slice,char};
use std::str::{self,FromStr};
use std::collections::HashMap;

use clang_sys::*;
use cexpr::token::Token;
use cexpr::expr::{IdentifierParser,EvalResult};
use cexpr::literal::CChar;

// main testing routine
fn test_definition(ident: Vec<u8>, tokens: &[Token], idents: &mut HashMap<Vec<u8>,EvalResult>) -> bool {
	use cexpr::expr::EvalResult::*;

	let display_name=String::from_utf8_lossy(&ident).into_owned();

	let test={
		// Split name such as Str_test_string into (Str,test_string)
		let pos=ident.iter().position(|c|*c==b'_').expect(&format!("Invalid definition in testcase: {}",display_name));
		let expected=&ident[..pos];
		let value=&ident[(pos+1)..];

		if expected==b"Str" {
			Some(Str(value.to_owned()))
		} else if expected==b"Int" {
			str::from_utf8(value).ok().map(|s|s.replace("n","-")).and_then(|v|i64::from_str(&v).ok()).map(::std::num::Wrapping).map(Int)
		} else if expected==b"Float" {
			str::from_utf8(value).ok().map(|s|s.replace("n","-").replace("p",".")).and_then(|v|f64::from_str(&v).ok()).map(Float)
		} else if expected==b"CharRaw" {
			str::from_utf8(value).ok().and_then(|v|u64::from_str(v).ok()).map(CChar::Raw).map(Char)
		} else if expected==b"CharChar" {
			str::from_utf8(value).ok().and_then(|v|u32::from_str(v).ok()).and_then(char::from_u32).map(CChar::Char).map(Char)
		} else {
			Some(Invalid)
		}.expect(&format!("Invalid definition in testcase: {}",display_name))
	};

	match IdentifierParser::new(idents).macro_definition(&tokens) {
		cexpr::nom::IResult::Done(_,(_,val)) => {
			if val==test {
				if let Some(_)=idents.insert(ident,val) {
					panic!("Duplicate definition for testcase: {}",display_name);
				}
				true
			} else {
				println!("Failed test for {}, expected {:?}, got {:?}",display_name,test,val);
				false
			}
		},
		e @ _ => {
			if test==Invalid {
				true
			} else {
				println!("Failed test for {}, expected {:?}, got {:?}",display_name,test,e);
				false
			}
		}
	}
}

// support code for the clang lexer
unsafe fn clang_str_to_vec(s: CXString) -> Vec<u8> {
	let vec=ffi::CStr::from_ptr(clang_getCString(s)).to_bytes().to_owned();
	clang_disposeString(s);
	vec
}

#[allow(non_upper_case_globals)]
unsafe fn token_clang_to_cexpr(tu: CXTranslationUnit, orig: &CXToken) -> Token {
	Token {
		kind:match clang_getTokenKind(*orig) {
			CXToken_Comment => cexpr::token::Kind::Comment,
			CXToken_Identifier => cexpr::token::Kind::Identifier,
			CXToken_Keyword => cexpr::token::Kind::Keyword,
			CXToken_Literal => cexpr::token::Kind::Literal,
			CXToken_Punctuation => cexpr::token::Kind::Punctuation,
			_ => panic!("invalid token kind: {:?}", *orig),
		},
		raw:clang_str_to_vec(clang_getTokenSpelling(tu,*orig)).into_boxed_slice()
	}
}

extern "C" fn visit_children_thunk<F>(cur: CXCursor, parent: CXCursor, closure: CXClientData) -> CXChildVisitResult
    where F: FnMut(CXCursor,CXCursor) -> CXChildVisitResult
{
    unsafe{(&mut *(closure as *mut F))(cur,parent)}
}

unsafe fn visit_children<F>(cursor: CXCursor, mut f: F)
	where F: FnMut(CXCursor,CXCursor) -> CXChildVisitResult
{
	clang_visitChildren(cursor, visit_children_thunk::<F> as _, &mut f as *mut F as CXClientData);
}

unsafe fn location_in_scope(r: CXSourceRange) -> bool {
	let start=clang_getRangeStart(r);
	let mut file=CXFile(ptr::null_mut());
	clang_getSpellingLocation(start,&mut file,ptr::null_mut(),ptr::null_mut(),ptr::null_mut());
	clang_Location_isFromMainFile(start)!=0
		&& clang_Location_isInSystemHeader(start)==0
		&& file.0!=ptr::null_mut()
}

fn test_file(file: &str) -> bool {
	let mut idents=HashMap::new();
	let mut all_succeeded=true;
	unsafe {
		let tu={
			let index=clang_createIndex(true as _, false as _);
			let cfile=ffi::CString::new(file).unwrap();
			let mut tu=mem::uninitialized();
			assert!(clang_parseTranslationUnit2(
				index,
				cfile.as_ptr(),
				[b"-std=c11\0".as_ptr() as *const ::std::os::raw::c_char].as_ptr(),1,
				ptr::null_mut(),0,
				CXTranslationUnit_DetailedPreprocessingRecord,
				&mut tu
			)==CXError_Success,"Failure reading test case {}",file);
			tu
		};
		visit_children(clang_getTranslationUnitCursor(tu),|cur,_parent| {
			if cur.kind==CXCursor_MacroDefinition {
				let mut range=clang_getCursorExtent(cur);
				if !location_in_scope(range) { return CXChildVisit_Continue }
				range.end_int_data-=1; // clang bug for macros only
				let mut token_ptr=ptr::null_mut();
				let mut num=0;
				clang_tokenize(tu,range,&mut token_ptr,&mut num);
				if token_ptr!=ptr::null_mut() {
					let tokens=slice::from_raw_parts(token_ptr,num as usize);
					let tokens: Vec<_>=tokens.iter().filter_map(|t|
						if clang_getTokenKind(*t)!=CXToken_Comment {
							Some(token_clang_to_cexpr(tu,t))
						} else {
							None
						}
					).collect();
					clang_disposeTokens(tu,token_ptr,num);
					all_succeeded&=test_definition(clang_str_to_vec(clang_getCursorSpelling(cur)),&tokens,&mut idents);
				}
			}
			CXChildVisit_Continue
		});
		clang_disposeTranslationUnit(tu);
	};
	all_succeeded
}

macro_rules! test_file {
	($f:ident) => {
		#[test] fn $f() {
			assert!(test_file(concat!("tests/input/",stringify!($f),".h")),"test_file")
		}
	}
}

test_file!(floats);
test_file!(chars);
test_file!(strings);
test_file!(int_signed);
test_file!(int_unsigned);
test_file!(fail);
