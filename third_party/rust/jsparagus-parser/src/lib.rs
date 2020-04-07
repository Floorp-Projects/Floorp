#![cfg_attr(feature = "unstable", feature(test))]

mod lexer;
pub mod numeric_value;
mod parser;
mod simulator;

#[cfg(test)]
mod tests;

extern crate jsparagus_ast as ast;
extern crate jsparagus_generated_parser as generated_parser;
extern crate jsparagus_json_log as json_log;

use crate::parser::Parser;
use ast::{
    arena,
    source_atom_set::SourceAtomSet,
    source_slice_list::SourceSliceList,
    types::{Module, Script},
};
use bumpalo;
use generated_parser::{
    AstBuilder, StackValue, TerminalId, START_STATE_MODULE, START_STATE_SCRIPT, TABLES,
};
pub use generated_parser::{ParseError, Result};
use json_log::json_debug;
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
    slices: Rc<RefCell<SourceSliceList<'alloc>>>,
) -> Result<'alloc, arena::Box<'alloc, Script<'alloc>>> {
    json_debug!({
        "parse": "script",
    });
    Ok(parse(allocator, source, START_STATE_SCRIPT, atoms, slices)?.to_ast()?)
}

pub fn parse_module<'alloc>(
    allocator: &'alloc bumpalo::Bump,
    source: &'alloc str,
    _options: &ParseOptions,
    atoms: Rc<RefCell<SourceAtomSet<'alloc>>>,
    slices: Rc<RefCell<SourceSliceList<'alloc>>>,
) -> Result<'alloc, arena::Box<'alloc, Module<'alloc>>> {
    json_debug!({
        "parse": "module",
    });
    Ok(parse(allocator, source, START_STATE_MODULE, atoms, slices)?.to_ast()?)
}

fn parse<'alloc>(
    allocator: &'alloc bumpalo::Bump,
    source: &'alloc str,
    start_state: usize,
    atoms: Rc<RefCell<SourceAtomSet<'alloc>>>,
    slices: Rc<RefCell<SourceSliceList<'alloc>>>,
) -> Result<'alloc, StackValue<'alloc>> {
    let mut tokens = Lexer::new(allocator, source.chars(), atoms.clone(), slices.clone());

    TABLES.check();

    let mut parser = Parser::new(AstBuilder::new(allocator, atoms, slices), start_state);

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
    slices: Rc<RefCell<SourceSliceList<'alloc>>>,
) -> Result<'alloc, bool> {
    let mut parser = Parser::new(
        AstBuilder::new(allocator, atoms.clone(), slices.clone()),
        START_STATE_SCRIPT,
    );
    let mut tokens = Lexer::new(allocator, source.chars(), atoms, slices);
    loop {
        let t = tokens.next(&parser)?;
        if t.terminal_id == TerminalId::End {
            break;
        }
        parser.write_token(&t)?;
    }
    Ok(!parser.can_close())
}
