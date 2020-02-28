//! Generated parts of a JS parser.

mod ast_builder;
mod declaration_kind;
mod early_errors;
mod error;
mod parser_tables_generated;
mod stack_value_generated;
mod token;

extern crate jsparagus_ast as ast;

pub use ast_builder::AstBuilder;
pub use declaration_kind::DeclarationKind;
pub use error::{ParseError, Result};
pub use parser_tables_generated::{
    reduce, ErrorCode, NonterminalId, ParserTables, TerminalId, START_STATE_MODULE,
    START_STATE_SCRIPT, TABLES,
};
pub use stack_value_generated::StackValue;
pub use token::Token;
