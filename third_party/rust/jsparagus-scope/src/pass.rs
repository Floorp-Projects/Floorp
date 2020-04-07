use crate::context::ScopeContext;
use ast::{types::*, visit::Pass};
use std::marker::PhantomData;

/// Visit all nodes in the AST, and invoke ScopeContext methods.
/// FIXME: This should be rewritten with grammar extension.
#[derive(Debug)]
pub struct ScopePass<'alloc> {
    pub context: ScopeContext,
    phantom: PhantomData<&'alloc ()>,
}

impl<'alloc> ScopePass<'alloc> {
    pub fn new() -> Self {
        Self {
            context: ScopeContext::new(),
            phantom: PhantomData,
        }
    }
}

impl<'alloc> Pass<'alloc> for ScopePass<'alloc> {
    fn enter_script(&mut self, _ast: &'alloc Script<'alloc>) {
        self.context.before_script();
    }

    fn leave_script(&mut self, _ast: &'alloc Script<'alloc>) {
        self.context.after_script();
    }

    fn enter_enum_statement_variant_block_statement(&mut self, block: &'alloc Block<'alloc>) {
        self.context.before_block_statement(block);
    }

    fn leave_enum_statement_variant_block_statement(&mut self, _block: &'alloc Block<'alloc>) {
        self.context.after_block_statement();
    }

    fn enter_variable_declaration(&mut self, ast: &'alloc VariableDeclaration<'alloc>) {
        match ast.kind {
            VariableDeclarationKind::Var { .. } => {
                self.context.before_var_declaration();
            }
            VariableDeclarationKind::Let { .. } => {
                self.context.before_let_declaration();
            }
            VariableDeclarationKind::Const { .. } => {
                self.context.before_const_declaration();
            }
        }
    }

    fn leave_variable_declaration(&mut self, ast: &'alloc VariableDeclaration<'alloc>) {
        match ast.kind {
            VariableDeclarationKind::Var { .. } => {
                self.context.after_var_declaration();
            }
            VariableDeclarationKind::Let { .. } => {
                self.context.after_let_declaration();
            }
            VariableDeclarationKind::Const { .. } => {
                self.context.after_const_declaration();
            }
        }
    }

    fn visit_binding_identifier(&mut self, ast: &'alloc BindingIdentifier) {
        // NOTE: The following should be handled at the parent node,
        // without visiting BindingIdentifier field:
        //   * Function.name
        //   * ClassExpression.name
        //   * ClassDeclaration.name
        //   * Import.default_binding
        //   * ImportNamespace.default_binding
        //   * ImportNamespace.namespace_binding
        //   * ImportSpecifier.binding
        self.context.on_binding_identifier(ast.name.value);
    }

    // Given we override `visit_binding_identifier` above,
    // visit_identifier is not called for Identifier inside BindingIdentifier,
    // and this is called only for references, either
    // IdentifierExpression or AssignmentTargetIdentifier.
    fn visit_identifier(&mut self, ast: &'alloc Identifier) {
        self.context.on_non_binding_identifier(ast.value);
    }

    fn enter_enum_statement_variant_function_declaration(&mut self, ast: &'alloc Function<'alloc>) {
        let name = if let Some(ref name) = ast.name {
            name.name.value
        } else {
            panic!("FunctionDeclaration should have name");
        };
        self.context.before_function_declaration(name);
    }
}
