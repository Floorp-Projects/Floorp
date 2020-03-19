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
    source_atom_set::SourceAtomSet,
    types::{Module, Script},
};
use bumpalo;
use generated_parser::{
    AstBuilder, StackValue, TerminalId, START_STATE_MODULE, START_STATE_SCRIPT, TABLES,
};
pub use generated_parser::{ParseError, Result};
use lexer::Lexer;
use std::cell::RefCell;
use std::rc::Rc;

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
    atoms: Rc<RefCell<SourceAtomSet<'alloc>>>,
) -> Result<'alloc, arena::Box<'alloc, Script<'alloc>>> {
    Ok(parse(allocator, source, START_STATE_SCRIPT, atoms)?.to_ast()?)
}

pub fn parse_module<'alloc>(
    allocator: &'alloc bumpalo::Bump,
    source: &'alloc str,
    _options: &ParseOptions,
    atoms: Rc<RefCell<SourceAtomSet<'alloc>>>,
) -> Result<'alloc, arena::Box<'alloc, Module<'alloc>>> {
    Ok(parse(allocator, source, START_STATE_MODULE, atoms)?.to_ast()?)
}

fn parse<'alloc>(
    allocator: &'alloc bumpalo::Bump,
    source: &'alloc str,
    start_state: usize,
    atoms: Rc<RefCell<SourceAtomSet<'alloc>>>,
) -> Result<'alloc, StackValue<'alloc>> {
    let mut tokens = Lexer::new(allocator, source.chars(), atoms.clone());

    TABLES.check();

    let mut parser = Parser::new(AstBuilder::new(allocator, atoms), start_state);

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
    atoms: Rc<RefCell<SourceAtomSet<'alloc>>>,
) -> Result<'alloc, bool> {
    let mut parser = Parser::new(
        AstBuilder::new(allocator, atoms.clone()),
        START_STATE_SCRIPT,
    );
    let mut tokens = Lexer::new(allocator, source.chars(), atoms);
    loop {
        let t = tokens.next(&parser)?;
        if t.terminal_id == TerminalId::End {
            break;
        }
        parser.write_token(&t)?;
    }
    Ok(!parser.can_close())
}
