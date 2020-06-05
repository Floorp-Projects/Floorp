//! Collect information about scopes and bindings, used by the emitter.
//!
//! Scope analysis happens in a separate pass after the AST is built:
//!
//! 1.  Parse the script, check for early errors, and build an AST.
//! 2.  Traverse the AST and do scope analysis (this crate).
//! 3.  Traverse the AST and emit bytecode.
//!
//! The output of this analysis is a `ScopeDataMapAndFunctionMap`
//! describing each scope, binding, and function in the AST.

mod builder;
pub mod data;
pub mod free_name_tracker;
mod pass;

extern crate jsparagus_ast as ast;
extern crate jsparagus_stencil as stencil;

use ast::visit::Pass;

pub use pass::ScopePassResult;

/// Visit all nodes in the AST, and create a scope data.
///
/// `ast` must already have been checked for early errors. This analysis does
/// not check for errors, even scope-related errors like redeclaration of a
/// `let` variable.
pub fn generate_scope_data<'alloc, 'a>(
    ast: &'alloc ast::types::Program<'alloc>,
) -> ScopePassResult {
    let mut scope_pass = pass::ScopePass::new();
    scope_pass.visit_program(ast);
    scope_pass.into()
}
