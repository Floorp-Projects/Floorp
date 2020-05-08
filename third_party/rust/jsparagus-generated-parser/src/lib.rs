//! Generated parts of a JS parser.

mod ast_builder;
mod context_stack;
mod declaration_kind;
mod early_error_checker;
mod early_errors;
mod error;
mod parser_tables_generated;
mod stack_value_generated;
mod token;
pub mod traits;

extern crate jsparagus_ast as ast;
extern crate static_assertions;

pub use ast_builder::{AstBuilder, AstBuilderDelegate};
pub use declaration_kind::DeclarationKind;
pub use error::{ParseError, Result};
pub use parser_tables_generated::{
    full_actions, noop_actions, ErrorCode, NonterminalId, ParseTable, ParserTrait, Term, TermValue,
    TerminalId, START_STATE_MODULE, START_STATE_SCRIPT, TABLES,
};
pub use stack_value_generated::StackValue;
pub use token::{Token, TokenValue};
