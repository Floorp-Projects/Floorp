#![cfg_attr(feature = "unstable", feature(test))]

mod lexer;
mod parser;
mod simulator;

#[cfg(test)]
mod tests;

extern crate jsparagus_ast as ast;
extern crate jsparagus_generated_parser as generated_parser;

use crate::parser::Parser;
use ast::{
    arena,
    types::{Module, Script},
};
use bumpalo;
use generated_parser::{
    AstBuilder, StackValue, TerminalId, START_STATE_MODULE, START_STATE_SCRIPT, TABLES,
};
pub use generated_parser::{ParseError, Result};
use lexer::Lexer;

pub struct ParseOptions {}
impl ParseOptions {
    pub fn new() -> Self {
        Self {}
    }
}

pub fn parse_script<'alloc>(
    allocator: &'alloc bumpalo::Bump,
    source: &'alloc str,
    _options: &ParseOptions,
) -> Result<'alloc, arena::Box<'alloc, Script<'alloc>>> {
    Ok(parse(allocator, source, START_STATE_SCRIPT)?.to_ast()?)
}

pub fn parse_module<'alloc>(
    allocator: &'alloc bumpalo::Bump,
    source: &'alloc str,
    _options: &ParseOptions,
) -> Result<'alloc, arena::Box<'alloc, Module<'alloc>>> {
    Ok(parse(allocator, source, START_STATE_MODULE)?.to_ast()?)
}

fn parse<'alloc>(
    allocator: &'alloc bumpalo::Bump,
    source: &'alloc str,
    start_state: usize,
) -> Result<'alloc, StackValue<'alloc>> {
    let mut tokens = Lexer::new(allocator, source.chars());

    TABLES.check();

    let mut parser = Parser::new(AstBuilder::new(allocator), start_state);

    loop {
        let t = tokens.next(&parser)?;
        if t.terminal_id == TerminalId::End {
            break;
        }
        parser.write_token(&t)?;
    }
    parser.close(tokens.offset())
}

pub fn is_partial_script<'alloc>(
    allocator: &'alloc bumpalo::Bump,
    source: &'alloc str,
) -> Result<'alloc, bool> {
    let mut parser = Parser::new(AstBuilder::new(allocator), START_STATE_SCRIPT);
    let mut tokens = Lexer::new(allocator, source.chars());
    loop {
        let t = tokens.next(&parser)?;
        if t.terminal_id == TerminalId::End {
            break;
        }
        parser.write_token(&t)?;
    }
    Ok(!parser.can_close())
}
