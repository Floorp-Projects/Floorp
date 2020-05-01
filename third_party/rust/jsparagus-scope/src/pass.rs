//! Code to traverse the AST and drive the rest of scope analysis.
//!
//! This module is responsible for walking the AST. Other modules do the actual
//! analysis.
//!
//! Scope analysis is currently a second pass, after parsing, using the AST,
//! but the goal is to do this analysis as part of the parse phase, even when
//! no AST is built. So we try to keep AST use separate from the analysis code.

use crate::builder::ScopeDataMapBuilder;
use crate::data::ScopeDataMap;
use ast::arena;
use ast::associated_data::AssociatedData;
use ast::{types::*, visit::Pass};

/// The result of scope analysis.
pub struct ScopeDataMapAndFunctionMap<'alloc> {
    pub scope_data_map: ScopeDataMap,
    pub function_map: AssociatedData<&'alloc Function<'alloc>>,
}

/// The top-level struct responsible for extracting the necessary information
/// from the AST. The analysis itself is done mainly by the `ScopeDataMapBuilder`,
/// which has very limited interaction with the AST.
///
/// FIXME: This should be rewritten as a grammar extension.
#[derive(Debug)]
pub struct ScopePass<'alloc> {
    builder: ScopeDataMapBuilder,
    function_map: AssociatedData<&'alloc Function<'alloc>>,
}

impl<'alloc> ScopePass<'alloc> {
    pub fn new() -> Self {
        Self {
            builder: ScopeDataMapBuilder::new(),
            function_map: AssociatedData::new(),
        }
    }
}

impl<'alloc> From<ScopePass<'alloc>> for ScopeDataMapAndFunctionMap<'alloc> {
    fn from(pass: ScopePass<'alloc>) -> ScopeDataMapAndFunctionMap<'alloc> {
        ScopeDataMapAndFunctionMap {
            scope_data_map: pass.builder.into(),
            function_map: pass.function_map,
        }
    }
}

impl<'alloc> Pass<'alloc> for ScopePass<'alloc> {
    fn enter_script(&mut self, _ast: &'alloc Script<'alloc>) {
        self.builder.before_script();
    }

    fn leave_script(&mut self, _ast: &'alloc Script<'alloc>) {
        self.builder.after_script();
    }

    fn enter_enum_statement_variant_block_statement(&mut self, block: &'alloc Block<'alloc>) {
        self.builder.before_block_statement(block);
    }

    fn leave_enum_statement_variant_block_statement(&mut self, _block: &'alloc Block<'alloc>) {
        self.builder.after_block_statement();
    }

    fn enter_variable_declaration(&mut self, ast: &'alloc VariableDeclaration<'alloc>) {
        match ast.kind {
            VariableDeclarationKind::Var { .. } => {
                self.builder.before_var_declaration();
            }
            VariableDeclarationKind::Let { .. } => {
                self.builder.before_let_declaration();
            }
            VariableDeclarationKind::Const { .. } => {
                self.builder.before_const_declaration();
            }
        }
    }

    fn leave_variable_declaration(&mut self, ast: &'alloc VariableDeclaration<'alloc>) {
        match ast.kind {
            VariableDeclarationKind::Var { .. } => {
                self.builder.after_var_declaration();
            }
            VariableDeclarationKind::Let { .. } => {
                self.builder.after_let_declaration();
            }
            VariableDeclarationKind::Const { .. } => {
                self.builder.after_const_declaration();
            }
        }
    }

    fn visit_binding_identifier(&mut self, ast: &'alloc BindingIdentifier) {
        self.builder.on_binding_identifier(ast.name.value);
    }

    // Given we override `visit_binding_identifier` above,
    // visit_identifier is not called for Identifier inside BindingIdentifier,
    // and this is called only for references, either
    // IdentifierExpression or AssignmentTargetIdentifier.
    fn visit_identifier(&mut self, ast: &'alloc Identifier) {
        self.builder.on_non_binding_identifier(ast.value);
    }

    fn enter_enum_statement_variant_function_declaration(&mut self, ast: &'alloc Function<'alloc>) {
        let name = if let Some(ref name) = ast.name {
            name.name.value
        } else {
            panic!("FunctionDeclaration should have name");
        };
        self.builder.before_function_declaration(name, ast);
        self.function_map.insert(ast, ast);
    }

    fn enter_enum_expression_variant_function_expression(&mut self, ast: &'alloc Function<'alloc>) {
        self.builder.before_function_expression(ast);
    }

    fn leave_enum_expression_variant_function_expression(
        &mut self,
        _ast: &'alloc Function<'alloc>,
    ) {
        self.builder.after_function_expression();
    }

    fn visit_formal_parameters(&mut self, ast: &'alloc FormalParameters<'alloc>) {
        self.builder.before_function_parameters(ast);

        self.enter_formal_parameters(ast);
        for item in &ast.items {
            self.builder.before_parameter();
            self.visit_parameter(item);
        }
        if let Some(item) = &ast.rest {
            self.builder.before_rest_parameter();
            self.visit_binding(item);
        }
        self.leave_formal_parameters(ast);

        self.builder.after_function_parameters();
    }

    /// Getter doesn't have FormalParameters.
    /// Call builder methods just before body.
    fn visit_getter(&mut self, ast: &'alloc Getter<'alloc>) {
        self.enter_getter(ast);
        self.visit_property_name(&ast.property_name);

        self.builder.before_function_parameters(ast);
        self.builder.after_function_parameters();

        self.visit_function_body(&ast.body);
        self.leave_getter(ast);
    }

    /// Setter doesn't have FormalParameters, but single Parameter.
    /// Call builder methods around it.
    fn visit_setter(&mut self, ast: &'alloc Setter<'alloc>) {
        self.enter_setter(ast);
        self.visit_property_name(&ast.property_name);

        self.builder.before_function_parameters(ast);
        self.builder.before_parameter();
        self.visit_parameter(&ast.param);
        self.builder.after_function_parameters();

        self.visit_function_body(&ast.body);
        self.leave_setter(ast);
    }

    fn leave_binding_with_default(&mut self, _ast: &'alloc BindingWithDefault<'alloc>) {
        self.builder.after_initializer();
    }

    fn enter_enum_property_name_variant_computed_property_name(
        &mut self,
        _ast: &'alloc ComputedPropertyName<'alloc>,
    ) {
        self.builder.before_computed_property_name();
    }

    fn enter_binding_pattern(&mut self, _ast: &'alloc BindingPattern<'alloc>) {
        self.builder.before_binding_pattern();
    }

    fn enter_function_body(&mut self, ast: &'alloc FunctionBody<'alloc>) {
        self.builder.before_function_body(ast);
    }

    fn leave_function_body(&mut self, _ast: &'alloc FunctionBody<'alloc>) {
        self.builder.after_function_body();
    }

    /// Arrow function with expression body.
    /// Use the expression as the node for function body.
    fn enter_enum_arrow_expression_body_variant_expression(
        &mut self,
        ast: &'alloc arena::Box<'alloc, Expression<'alloc>>,
    ) {
        let expr: &Expression = ast;
        self.builder.before_function_body(expr);
    }

    fn leave_enum_arrow_expression_body_variant_expression(
        &mut self,
        _ast: &'alloc arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.builder.after_function_body();
    }
}
