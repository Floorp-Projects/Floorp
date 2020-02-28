// WARNING: This file is auto-generated.

#![allow(unused_mut)]
#![allow(unused_parens)]
#![allow(unused_variables)]
#![allow(dead_code)]

use crate::arena;
use crate::types::*;

pub trait Pass<'alloc> {
    fn visit_argument(&mut self, ast: &mut Argument<'alloc>) {
        self.enter_argument(ast);
        match ast {
            Argument::SpreadElement(ast) => {
                self.visit_enum_argument_variant_spread_element(ast)
            }
            Argument::Expression(ast) => {
                self.visit_enum_argument_variant_expression(ast)
            }
        }
        self.leave_argument(ast);
    }

    fn enter_argument(&mut self, ast: &mut Argument<'alloc>) {
    }

    fn leave_argument(&mut self, ast: &mut Argument<'alloc>) {
    }

    fn visit_enum_argument_variant_spread_element(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_argument_variant_spread_element(ast);
        self.visit_expression(ast);
        self.leave_enum_argument_variant_spread_element(ast);
    }

    fn enter_enum_argument_variant_spread_element(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_argument_variant_spread_element(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_argument_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_argument_variant_expression(ast);
        self.visit_expression(ast);
        self.leave_enum_argument_variant_expression(ast);
    }

    fn enter_enum_argument_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_argument_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_arguments(&mut self, ast: &mut Arguments<'alloc>) {
        self.enter_arguments(ast);
        for item in &mut ast.args {
            self.visit_argument(item);
        }
        self.leave_arguments(ast);
    }

    fn enter_arguments(&mut self, ast: &mut Arguments<'alloc>) {
    }

    fn leave_arguments(&mut self, ast: &mut Arguments<'alloc>) {
    }

    fn visit_identifier(&mut self, ast: &mut Identifier<'alloc>) {
        self.enter_identifier(ast);
        self.leave_identifier(ast);
    }

    fn enter_identifier(&mut self, ast: &mut Identifier<'alloc>) {
    }

    fn leave_identifier(&mut self, ast: &mut Identifier<'alloc>) {
    }

    fn visit_identifier_name(&mut self, ast: &mut IdentifierName<'alloc>) {
        self.enter_identifier_name(ast);
        self.leave_identifier_name(ast);
    }

    fn enter_identifier_name(&mut self, ast: &mut IdentifierName<'alloc>) {
    }

    fn leave_identifier_name(&mut self, ast: &mut IdentifierName<'alloc>) {
    }

    fn visit_private_identifier(&mut self, ast: &mut PrivateIdentifier<'alloc>) {
        self.enter_private_identifier(ast);
        self.leave_private_identifier(ast);
    }

    fn enter_private_identifier(&mut self, ast: &mut PrivateIdentifier<'alloc>) {
    }

    fn leave_private_identifier(&mut self, ast: &mut PrivateIdentifier<'alloc>) {
    }

    fn visit_label(&mut self, ast: &mut Label<'alloc>) {
        self.enter_label(ast);
        self.leave_label(ast);
    }

    fn enter_label(&mut self, ast: &mut Label<'alloc>) {
    }

    fn leave_label(&mut self, ast: &mut Label<'alloc>) {
    }

    fn visit_variable_declaration_kind(&mut self, ast: &mut VariableDeclarationKind) {
        self.enter_variable_declaration_kind(ast);
        match ast {
            VariableDeclarationKind::Var { .. } => {
                self.visit_enum_variable_declaration_kind_variant_var()
            }
            VariableDeclarationKind::Let { .. } => {
                self.visit_enum_variable_declaration_kind_variant_let()
            }
            VariableDeclarationKind::Const { .. } => {
                self.visit_enum_variable_declaration_kind_variant_const()
            }
        }
        self.leave_variable_declaration_kind(ast);
    }

    fn enter_variable_declaration_kind(&mut self, ast: &mut VariableDeclarationKind) {
    }

    fn leave_variable_declaration_kind(&mut self, ast: &mut VariableDeclarationKind) {
    }

    fn visit_enum_variable_declaration_kind_variant_var(&mut self) {
    }

    fn visit_enum_variable_declaration_kind_variant_let(&mut self) {
    }

    fn visit_enum_variable_declaration_kind_variant_const(&mut self) {
    }

    fn visit_compound_assignment_operator(&mut self, ast: &mut CompoundAssignmentOperator) {
        self.enter_compound_assignment_operator(ast);
        match ast {
            CompoundAssignmentOperator::Add { .. } => {
                self.visit_enum_compound_assignment_operator_variant_add()
            }
            CompoundAssignmentOperator::Sub { .. } => {
                self.visit_enum_compound_assignment_operator_variant_sub()
            }
            CompoundAssignmentOperator::Mul { .. } => {
                self.visit_enum_compound_assignment_operator_variant_mul()
            }
            CompoundAssignmentOperator::Div { .. } => {
                self.visit_enum_compound_assignment_operator_variant_div()
            }
            CompoundAssignmentOperator::Mod { .. } => {
                self.visit_enum_compound_assignment_operator_variant_mod()
            }
            CompoundAssignmentOperator::Pow { .. } => {
                self.visit_enum_compound_assignment_operator_variant_pow()
            }
            CompoundAssignmentOperator::LeftShift { .. } => {
                self.visit_enum_compound_assignment_operator_variant_left_shift()
            }
            CompoundAssignmentOperator::RightShift { .. } => {
                self.visit_enum_compound_assignment_operator_variant_right_shift()
            }
            CompoundAssignmentOperator::RightShiftExt { .. } => {
                self.visit_enum_compound_assignment_operator_variant_right_shift_ext()
            }
            CompoundAssignmentOperator::Or { .. } => {
                self.visit_enum_compound_assignment_operator_variant_or()
            }
            CompoundAssignmentOperator::Xor { .. } => {
                self.visit_enum_compound_assignment_operator_variant_xor()
            }
            CompoundAssignmentOperator::And { .. } => {
                self.visit_enum_compound_assignment_operator_variant_and()
            }
        }
        self.leave_compound_assignment_operator(ast);
    }

    fn enter_compound_assignment_operator(&mut self, ast: &mut CompoundAssignmentOperator) {
    }

    fn leave_compound_assignment_operator(&mut self, ast: &mut CompoundAssignmentOperator) {
    }

    fn visit_enum_compound_assignment_operator_variant_add(&mut self) {
    }

    fn visit_enum_compound_assignment_operator_variant_sub(&mut self) {
    }

    fn visit_enum_compound_assignment_operator_variant_mul(&mut self) {
    }

    fn visit_enum_compound_assignment_operator_variant_div(&mut self) {
    }

    fn visit_enum_compound_assignment_operator_variant_mod(&mut self) {
    }

    fn visit_enum_compound_assignment_operator_variant_pow(&mut self) {
    }

    fn visit_enum_compound_assignment_operator_variant_left_shift(&mut self) {
    }

    fn visit_enum_compound_assignment_operator_variant_right_shift(&mut self) {
    }

    fn visit_enum_compound_assignment_operator_variant_right_shift_ext(&mut self) {
    }

    fn visit_enum_compound_assignment_operator_variant_or(&mut self) {
    }

    fn visit_enum_compound_assignment_operator_variant_xor(&mut self) {
    }

    fn visit_enum_compound_assignment_operator_variant_and(&mut self) {
    }

    fn visit_binary_operator(&mut self, ast: &mut BinaryOperator) {
        self.enter_binary_operator(ast);
        match ast {
            BinaryOperator::Equals { .. } => {
                self.visit_enum_binary_operator_variant_equals()
            }
            BinaryOperator::NotEquals { .. } => {
                self.visit_enum_binary_operator_variant_not_equals()
            }
            BinaryOperator::StrictEquals { .. } => {
                self.visit_enum_binary_operator_variant_strict_equals()
            }
            BinaryOperator::StrictNotEquals { .. } => {
                self.visit_enum_binary_operator_variant_strict_not_equals()
            }
            BinaryOperator::LessThan { .. } => {
                self.visit_enum_binary_operator_variant_less_than()
            }
            BinaryOperator::LessThanOrEqual { .. } => {
                self.visit_enum_binary_operator_variant_less_than_or_equal()
            }
            BinaryOperator::GreaterThan { .. } => {
                self.visit_enum_binary_operator_variant_greater_than()
            }
            BinaryOperator::GreaterThanOrEqual { .. } => {
                self.visit_enum_binary_operator_variant_greater_than_or_equal()
            }
            BinaryOperator::In { .. } => {
                self.visit_enum_binary_operator_variant_in()
            }
            BinaryOperator::Instanceof { .. } => {
                self.visit_enum_binary_operator_variant_instanceof()
            }
            BinaryOperator::LeftShift { .. } => {
                self.visit_enum_binary_operator_variant_left_shift()
            }
            BinaryOperator::RightShift { .. } => {
                self.visit_enum_binary_operator_variant_right_shift()
            }
            BinaryOperator::RightShiftExt { .. } => {
                self.visit_enum_binary_operator_variant_right_shift_ext()
            }
            BinaryOperator::Add { .. } => {
                self.visit_enum_binary_operator_variant_add()
            }
            BinaryOperator::Sub { .. } => {
                self.visit_enum_binary_operator_variant_sub()
            }
            BinaryOperator::Mul { .. } => {
                self.visit_enum_binary_operator_variant_mul()
            }
            BinaryOperator::Div { .. } => {
                self.visit_enum_binary_operator_variant_div()
            }
            BinaryOperator::Mod { .. } => {
                self.visit_enum_binary_operator_variant_mod()
            }
            BinaryOperator::Pow { .. } => {
                self.visit_enum_binary_operator_variant_pow()
            }
            BinaryOperator::Comma { .. } => {
                self.visit_enum_binary_operator_variant_comma()
            }
            BinaryOperator::Coalesce { .. } => {
                self.visit_enum_binary_operator_variant_coalesce()
            }
            BinaryOperator::LogicalOr { .. } => {
                self.visit_enum_binary_operator_variant_logical_or()
            }
            BinaryOperator::LogicalAnd { .. } => {
                self.visit_enum_binary_operator_variant_logical_and()
            }
            BinaryOperator::BitwiseOr { .. } => {
                self.visit_enum_binary_operator_variant_bitwise_or()
            }
            BinaryOperator::BitwiseXor { .. } => {
                self.visit_enum_binary_operator_variant_bitwise_xor()
            }
            BinaryOperator::BitwiseAnd { .. } => {
                self.visit_enum_binary_operator_variant_bitwise_and()
            }
        }
        self.leave_binary_operator(ast);
    }

    fn enter_binary_operator(&mut self, ast: &mut BinaryOperator) {
    }

    fn leave_binary_operator(&mut self, ast: &mut BinaryOperator) {
    }

    fn visit_enum_binary_operator_variant_equals(&mut self) {
    }

    fn visit_enum_binary_operator_variant_not_equals(&mut self) {
    }

    fn visit_enum_binary_operator_variant_strict_equals(&mut self) {
    }

    fn visit_enum_binary_operator_variant_strict_not_equals(&mut self) {
    }

    fn visit_enum_binary_operator_variant_less_than(&mut self) {
    }

    fn visit_enum_binary_operator_variant_less_than_or_equal(&mut self) {
    }

    fn visit_enum_binary_operator_variant_greater_than(&mut self) {
    }

    fn visit_enum_binary_operator_variant_greater_than_or_equal(&mut self) {
    }

    fn visit_enum_binary_operator_variant_in(&mut self) {
    }

    fn visit_enum_binary_operator_variant_instanceof(&mut self) {
    }

    fn visit_enum_binary_operator_variant_left_shift(&mut self) {
    }

    fn visit_enum_binary_operator_variant_right_shift(&mut self) {
    }

    fn visit_enum_binary_operator_variant_right_shift_ext(&mut self) {
    }

    fn visit_enum_binary_operator_variant_add(&mut self) {
    }

    fn visit_enum_binary_operator_variant_sub(&mut self) {
    }

    fn visit_enum_binary_operator_variant_mul(&mut self) {
    }

    fn visit_enum_binary_operator_variant_div(&mut self) {
    }

    fn visit_enum_binary_operator_variant_mod(&mut self) {
    }

    fn visit_enum_binary_operator_variant_pow(&mut self) {
    }

    fn visit_enum_binary_operator_variant_comma(&mut self) {
    }

    fn visit_enum_binary_operator_variant_coalesce(&mut self) {
    }

    fn visit_enum_binary_operator_variant_logical_or(&mut self) {
    }

    fn visit_enum_binary_operator_variant_logical_and(&mut self) {
    }

    fn visit_enum_binary_operator_variant_bitwise_or(&mut self) {
    }

    fn visit_enum_binary_operator_variant_bitwise_xor(&mut self) {
    }

    fn visit_enum_binary_operator_variant_bitwise_and(&mut self) {
    }

    fn visit_unary_operator(&mut self, ast: &mut UnaryOperator) {
        self.enter_unary_operator(ast);
        match ast {
            UnaryOperator::Plus { .. } => {
                self.visit_enum_unary_operator_variant_plus()
            }
            UnaryOperator::Minus { .. } => {
                self.visit_enum_unary_operator_variant_minus()
            }
            UnaryOperator::LogicalNot { .. } => {
                self.visit_enum_unary_operator_variant_logical_not()
            }
            UnaryOperator::BitwiseNot { .. } => {
                self.visit_enum_unary_operator_variant_bitwise_not()
            }
            UnaryOperator::Typeof { .. } => {
                self.visit_enum_unary_operator_variant_typeof()
            }
            UnaryOperator::Void { .. } => {
                self.visit_enum_unary_operator_variant_void()
            }
            UnaryOperator::Delete { .. } => {
                self.visit_enum_unary_operator_variant_delete()
            }
        }
        self.leave_unary_operator(ast);
    }

    fn enter_unary_operator(&mut self, ast: &mut UnaryOperator) {
    }

    fn leave_unary_operator(&mut self, ast: &mut UnaryOperator) {
    }

    fn visit_enum_unary_operator_variant_plus(&mut self) {
    }

    fn visit_enum_unary_operator_variant_minus(&mut self) {
    }

    fn visit_enum_unary_operator_variant_logical_not(&mut self) {
    }

    fn visit_enum_unary_operator_variant_bitwise_not(&mut self) {
    }

    fn visit_enum_unary_operator_variant_typeof(&mut self) {
    }

    fn visit_enum_unary_operator_variant_void(&mut self) {
    }

    fn visit_enum_unary_operator_variant_delete(&mut self) {
    }

    fn visit_update_operator(&mut self, ast: &mut UpdateOperator) {
        self.enter_update_operator(ast);
        match ast {
            UpdateOperator::Increment { .. } => {
                self.visit_enum_update_operator_variant_increment()
            }
            UpdateOperator::Decrement { .. } => {
                self.visit_enum_update_operator_variant_decrement()
            }
        }
        self.leave_update_operator(ast);
    }

    fn enter_update_operator(&mut self, ast: &mut UpdateOperator) {
    }

    fn leave_update_operator(&mut self, ast: &mut UpdateOperator) {
    }

    fn visit_enum_update_operator_variant_increment(&mut self) {
    }

    fn visit_enum_update_operator_variant_decrement(&mut self) {
    }

    fn visit_function(&mut self, ast: &mut Function<'alloc>) {
        self.enter_function(ast);
        if let Some(item) = &mut ast.name {
            self.visit_binding_identifier(item);
        }
        self.visit_formal_parameters(&mut ast.params);
        self.visit_function_body(&mut ast.body);
        self.leave_function(ast);
    }

    fn enter_function(&mut self, ast: &mut Function<'alloc>) {
    }

    fn leave_function(&mut self, ast: &mut Function<'alloc>) {
    }

    fn visit_program(&mut self, ast: &mut Program<'alloc>) {
        self.enter_program(ast);
        match ast {
            Program::Module(ast) => {
                self.visit_enum_program_variant_module(ast)
            }
            Program::Script(ast) => {
                self.visit_enum_program_variant_script(ast)
            }
        }
        self.leave_program(ast);
    }

    fn enter_program(&mut self, ast: &mut Program<'alloc>) {
    }

    fn leave_program(&mut self, ast: &mut Program<'alloc>) {
    }

    fn visit_enum_program_variant_module(
        &mut self,
        ast: &mut Module<'alloc>,
    ) {
        self.enter_enum_program_variant_module(ast);
        self.visit_module(ast);
        self.leave_enum_program_variant_module(ast);
    }

    fn enter_enum_program_variant_module(
        &mut self,
        ast: &mut Module<'alloc>,
    ) {
    }

    fn leave_enum_program_variant_module(
        &mut self,
        ast: &mut Module<'alloc>,
    ) {
    }

    fn visit_enum_program_variant_script(
        &mut self,
        ast: &mut Script<'alloc>,
    ) {
        self.enter_enum_program_variant_script(ast);
        self.visit_script(ast);
        self.leave_enum_program_variant_script(ast);
    }

    fn enter_enum_program_variant_script(
        &mut self,
        ast: &mut Script<'alloc>,
    ) {
    }

    fn leave_enum_program_variant_script(
        &mut self,
        ast: &mut Script<'alloc>,
    ) {
    }

    fn visit_if_statement(&mut self, ast: &mut IfStatement<'alloc>) {
        self.enter_if_statement(ast);
        self.visit_expression(&mut ast.test);
        self.visit_statement(&mut ast.consequent);
        if let Some(item) = &mut ast.alternate {
            self.visit_statement(item);
        }
        self.leave_if_statement(ast);
    }

    fn enter_if_statement(&mut self, ast: &mut IfStatement<'alloc>) {
    }

    fn leave_if_statement(&mut self, ast: &mut IfStatement<'alloc>) {
    }

    fn visit_statement(&mut self, ast: &mut Statement<'alloc>) {
        self.enter_statement(ast);
        match ast {
            Statement::BlockStatement { block, .. } => {
                self.visit_enum_statement_variant_block_statement(
                    block,
                )
            }
            Statement::BreakStatement { label, .. } => {
                self.visit_enum_statement_variant_break_statement(
                    label,
                )
            }
            Statement::ContinueStatement { label, .. } => {
                self.visit_enum_statement_variant_continue_statement(
                    label,
                )
            }
            Statement::DebuggerStatement { .. } => {
                self.visit_enum_statement_variant_debugger_statement()
            }
            Statement::DoWhileStatement { block, test, .. } => {
                self.visit_enum_statement_variant_do_while_statement(
                    block,
                    test,
                )
            }
            Statement::EmptyStatement { .. } => {
                self.visit_enum_statement_variant_empty_statement()
            }
            Statement::ExpressionStatement(ast) => {
                self.visit_enum_statement_variant_expression_statement(ast)
            }
            Statement::ForInStatement { left, right, block, .. } => {
                self.visit_enum_statement_variant_for_in_statement(
                    left,
                    right,
                    block,
                )
            }
            Statement::ForOfStatement { left, right, block, .. } => {
                self.visit_enum_statement_variant_for_of_statement(
                    left,
                    right,
                    block,
                )
            }
            Statement::ForStatement { init, test, update, block, .. } => {
                self.visit_enum_statement_variant_for_statement(
                    init,
                    test,
                    update,
                    block,
                )
            }
            Statement::IfStatement(ast) => {
                self.visit_enum_statement_variant_if_statement(ast)
            }
            Statement::LabeledStatement { label, body, .. } => {
                self.visit_enum_statement_variant_labeled_statement(
                    label,
                    body,
                )
            }
            Statement::ReturnStatement { expression, .. } => {
                self.visit_enum_statement_variant_return_statement(
                    expression,
                )
            }
            Statement::SwitchStatement { discriminant, cases, .. } => {
                self.visit_enum_statement_variant_switch_statement(
                    discriminant,
                    cases,
                )
            }
            Statement::SwitchStatementWithDefault { discriminant, pre_default_cases, default_case, post_default_cases, .. } => {
                self.visit_enum_statement_variant_switch_statement_with_default(
                    discriminant,
                    pre_default_cases,
                    default_case,
                    post_default_cases,
                )
            }
            Statement::ThrowStatement { expression, .. } => {
                self.visit_enum_statement_variant_throw_statement(
                    expression,
                )
            }
            Statement::TryCatchStatement { body, catch_clause, .. } => {
                self.visit_enum_statement_variant_try_catch_statement(
                    body,
                    catch_clause,
                )
            }
            Statement::TryFinallyStatement { body, catch_clause, finalizer, .. } => {
                self.visit_enum_statement_variant_try_finally_statement(
                    body,
                    catch_clause,
                    finalizer,
                )
            }
            Statement::WhileStatement { test, block, .. } => {
                self.visit_enum_statement_variant_while_statement(
                    test,
                    block,
                )
            }
            Statement::WithStatement { object, body, .. } => {
                self.visit_enum_statement_variant_with_statement(
                    object,
                    body,
                )
            }
            Statement::VariableDeclarationStatement(ast) => {
                self.visit_enum_statement_variant_variable_declaration_statement(ast)
            }
            Statement::FunctionDeclaration(ast) => {
                self.visit_enum_statement_variant_function_declaration(ast)
            }
            Statement::ClassDeclaration(ast) => {
                self.visit_enum_statement_variant_class_declaration(ast)
            }
        }
        self.leave_statement(ast);
    }

    fn enter_statement(&mut self, ast: &mut Statement<'alloc>) {
    }

    fn leave_statement(&mut self, ast: &mut Statement<'alloc>) {
    }

    fn visit_enum_statement_variant_block_statement(
        &mut self,
        block: &mut Block<'alloc>,
    ) {
        self.enter_enum_statement_variant_block_statement(
            block,
        );
        self.visit_block(block);
        self.leave_enum_statement_variant_block_statement(
            block,
        );
    }

    fn enter_enum_statement_variant_block_statement(
        &mut self,
        block: &mut Block<'alloc>,
    ) {
    }

    fn leave_enum_statement_variant_block_statement(
        &mut self,
        block: &mut Block<'alloc>,
    ) {
    }

    fn visit_enum_statement_variant_break_statement(
        &mut self,
        label: &mut Option<Label<'alloc>>,
    ) {
        self.enter_enum_statement_variant_break_statement(
            label,
        );
        if let Some(item) = label {
            self.visit_label(item);
        }
        self.leave_enum_statement_variant_break_statement(
            label,
        );
    }

    fn enter_enum_statement_variant_break_statement(
        &mut self,
        label: &mut Option<Label<'alloc>>,
    ) {
    }

    fn leave_enum_statement_variant_break_statement(
        &mut self,
        label: &mut Option<Label<'alloc>>,
    ) {
    }

    fn visit_enum_statement_variant_continue_statement(
        &mut self,
        label: &mut Option<Label<'alloc>>,
    ) {
        self.enter_enum_statement_variant_continue_statement(
            label,
        );
        if let Some(item) = label {
            self.visit_label(item);
        }
        self.leave_enum_statement_variant_continue_statement(
            label,
        );
    }

    fn enter_enum_statement_variant_continue_statement(
        &mut self,
        label: &mut Option<Label<'alloc>>,
    ) {
    }

    fn leave_enum_statement_variant_continue_statement(
        &mut self,
        label: &mut Option<Label<'alloc>>,
    ) {
    }

    fn visit_enum_statement_variant_debugger_statement(&mut self) {
    }

    fn visit_enum_statement_variant_do_while_statement(
        &mut self,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
        test: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_statement_variant_do_while_statement(
            block,
            test,
        );
        self.visit_statement(block);
        self.visit_expression(test);
        self.leave_enum_statement_variant_do_while_statement(
            block,
            test,
        );
    }

    fn enter_enum_statement_variant_do_while_statement(
        &mut self,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
        test: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_statement_variant_do_while_statement(
        &mut self,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
        test: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_statement_variant_empty_statement(&mut self) {
    }

    fn visit_enum_statement_variant_expression_statement(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_statement_variant_expression_statement(ast);
        self.visit_expression(ast);
        self.leave_enum_statement_variant_expression_statement(ast);
    }

    fn enter_enum_statement_variant_expression_statement(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_statement_variant_expression_statement(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_statement_variant_for_in_statement(
        &mut self,
        left: &mut VariableDeclarationOrAssignmentTarget<'alloc>,
        right: &mut arena::Box<'alloc, Expression<'alloc>>,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
        self.enter_enum_statement_variant_for_in_statement(
            left,
            right,
            block,
        );
        self.visit_variable_declaration_or_assignment_target(left);
        self.visit_expression(right);
        self.visit_statement(block);
        self.leave_enum_statement_variant_for_in_statement(
            left,
            right,
            block,
        );
    }

    fn enter_enum_statement_variant_for_in_statement(
        &mut self,
        left: &mut VariableDeclarationOrAssignmentTarget<'alloc>,
        right: &mut arena::Box<'alloc, Expression<'alloc>>,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
    }

    fn leave_enum_statement_variant_for_in_statement(
        &mut self,
        left: &mut VariableDeclarationOrAssignmentTarget<'alloc>,
        right: &mut arena::Box<'alloc, Expression<'alloc>>,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
    }

    fn visit_enum_statement_variant_for_of_statement(
        &mut self,
        left: &mut VariableDeclarationOrAssignmentTarget<'alloc>,
        right: &mut arena::Box<'alloc, Expression<'alloc>>,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
        self.enter_enum_statement_variant_for_of_statement(
            left,
            right,
            block,
        );
        self.visit_variable_declaration_or_assignment_target(left);
        self.visit_expression(right);
        self.visit_statement(block);
        self.leave_enum_statement_variant_for_of_statement(
            left,
            right,
            block,
        );
    }

    fn enter_enum_statement_variant_for_of_statement(
        &mut self,
        left: &mut VariableDeclarationOrAssignmentTarget<'alloc>,
        right: &mut arena::Box<'alloc, Expression<'alloc>>,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
    }

    fn leave_enum_statement_variant_for_of_statement(
        &mut self,
        left: &mut VariableDeclarationOrAssignmentTarget<'alloc>,
        right: &mut arena::Box<'alloc, Expression<'alloc>>,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
    }

    fn visit_enum_statement_variant_for_statement(
        &mut self,
        init: &mut Option<VariableDeclarationOrExpression<'alloc>>,
        test: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
        update: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
        self.enter_enum_statement_variant_for_statement(
            init,
            test,
            update,
            block,
        );
        if let Some(item) = init {
            self.visit_variable_declaration_or_expression(item);
        }
        if let Some(item) = test {
            self.visit_expression(item);
        }
        if let Some(item) = update {
            self.visit_expression(item);
        }
        self.visit_statement(block);
        self.leave_enum_statement_variant_for_statement(
            init,
            test,
            update,
            block,
        );
    }

    fn enter_enum_statement_variant_for_statement(
        &mut self,
        init: &mut Option<VariableDeclarationOrExpression<'alloc>>,
        test: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
        update: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
    }

    fn leave_enum_statement_variant_for_statement(
        &mut self,
        init: &mut Option<VariableDeclarationOrExpression<'alloc>>,
        test: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
        update: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
    }

    fn visit_enum_statement_variant_if_statement(
        &mut self,
        ast: &mut IfStatement<'alloc>,
    ) {
        self.enter_enum_statement_variant_if_statement(ast);
        self.visit_if_statement(ast);
        self.leave_enum_statement_variant_if_statement(ast);
    }

    fn enter_enum_statement_variant_if_statement(
        &mut self,
        ast: &mut IfStatement<'alloc>,
    ) {
    }

    fn leave_enum_statement_variant_if_statement(
        &mut self,
        ast: &mut IfStatement<'alloc>,
    ) {
    }

    fn visit_enum_statement_variant_labeled_statement(
        &mut self,
        label: &mut Label<'alloc>,
        body: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
        self.enter_enum_statement_variant_labeled_statement(
            label,
            body,
        );
        self.visit_label(label);
        self.visit_statement(body);
        self.leave_enum_statement_variant_labeled_statement(
            label,
            body,
        );
    }

    fn enter_enum_statement_variant_labeled_statement(
        &mut self,
        label: &mut Label<'alloc>,
        body: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
    }

    fn leave_enum_statement_variant_labeled_statement(
        &mut self,
        label: &mut Label<'alloc>,
        body: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
    }

    fn visit_enum_statement_variant_return_statement(
        &mut self,
        expression: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) {
        self.enter_enum_statement_variant_return_statement(
            expression,
        );
        if let Some(item) = expression {
            self.visit_expression(item);
        }
        self.leave_enum_statement_variant_return_statement(
            expression,
        );
    }

    fn enter_enum_statement_variant_return_statement(
        &mut self,
        expression: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) {
    }

    fn leave_enum_statement_variant_return_statement(
        &mut self,
        expression: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) {
    }

    fn visit_enum_statement_variant_switch_statement(
        &mut self,
        discriminant: &mut arena::Box<'alloc, Expression<'alloc>>,
        cases: &mut arena::Vec<'alloc, SwitchCase<'alloc>>,
    ) {
        self.enter_enum_statement_variant_switch_statement(
            discriminant,
            cases,
        );
        self.visit_expression(discriminant);
        for item in cases.iter_mut() {
            self.visit_switch_case(item);
        }
        self.leave_enum_statement_variant_switch_statement(
            discriminant,
            cases,
        );
    }

    fn enter_enum_statement_variant_switch_statement(
        &mut self,
        discriminant: &mut arena::Box<'alloc, Expression<'alloc>>,
        cases: &mut arena::Vec<'alloc, SwitchCase<'alloc>>,
    ) {
    }

    fn leave_enum_statement_variant_switch_statement(
        &mut self,
        discriminant: &mut arena::Box<'alloc, Expression<'alloc>>,
        cases: &mut arena::Vec<'alloc, SwitchCase<'alloc>>,
    ) {
    }

    fn visit_enum_statement_variant_switch_statement_with_default(
        &mut self,
        discriminant: &mut arena::Box<'alloc, Expression<'alloc>>,
        pre_default_cases: &mut arena::Vec<'alloc, SwitchCase<'alloc>>,
        default_case: &mut SwitchDefault<'alloc>,
        post_default_cases: &mut arena::Vec<'alloc, SwitchCase<'alloc>>,
    ) {
        self.enter_enum_statement_variant_switch_statement_with_default(
            discriminant,
            pre_default_cases,
            default_case,
            post_default_cases,
        );
        self.visit_expression(discriminant);
        for item in pre_default_cases.iter_mut() {
            self.visit_switch_case(item);
        }
        self.visit_switch_default(default_case);
        for item in post_default_cases.iter_mut() {
            self.visit_switch_case(item);
        }
        self.leave_enum_statement_variant_switch_statement_with_default(
            discriminant,
            pre_default_cases,
            default_case,
            post_default_cases,
        );
    }

    fn enter_enum_statement_variant_switch_statement_with_default(
        &mut self,
        discriminant: &mut arena::Box<'alloc, Expression<'alloc>>,
        pre_default_cases: &mut arena::Vec<'alloc, SwitchCase<'alloc>>,
        default_case: &mut SwitchDefault<'alloc>,
        post_default_cases: &mut arena::Vec<'alloc, SwitchCase<'alloc>>,
    ) {
    }

    fn leave_enum_statement_variant_switch_statement_with_default(
        &mut self,
        discriminant: &mut arena::Box<'alloc, Expression<'alloc>>,
        pre_default_cases: &mut arena::Vec<'alloc, SwitchCase<'alloc>>,
        default_case: &mut SwitchDefault<'alloc>,
        post_default_cases: &mut arena::Vec<'alloc, SwitchCase<'alloc>>,
    ) {
    }

    fn visit_enum_statement_variant_throw_statement(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_statement_variant_throw_statement(
            expression,
        );
        self.visit_expression(expression);
        self.leave_enum_statement_variant_throw_statement(
            expression,
        );
    }

    fn enter_enum_statement_variant_throw_statement(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_statement_variant_throw_statement(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_statement_variant_try_catch_statement(
        &mut self,
        body: &mut Block<'alloc>,
        catch_clause: &mut CatchClause<'alloc>,
    ) {
        self.enter_enum_statement_variant_try_catch_statement(
            body,
            catch_clause,
        );
        self.visit_block(body);
        self.visit_catch_clause(catch_clause);
        self.leave_enum_statement_variant_try_catch_statement(
            body,
            catch_clause,
        );
    }

    fn enter_enum_statement_variant_try_catch_statement(
        &mut self,
        body: &mut Block<'alloc>,
        catch_clause: &mut CatchClause<'alloc>,
    ) {
    }

    fn leave_enum_statement_variant_try_catch_statement(
        &mut self,
        body: &mut Block<'alloc>,
        catch_clause: &mut CatchClause<'alloc>,
    ) {
    }

    fn visit_enum_statement_variant_try_finally_statement(
        &mut self,
        body: &mut Block<'alloc>,
        catch_clause: &mut Option<CatchClause<'alloc>>,
        finalizer: &mut Block<'alloc>,
    ) {
        self.enter_enum_statement_variant_try_finally_statement(
            body,
            catch_clause,
            finalizer,
        );
        self.visit_block(body);
        if let Some(item) = catch_clause {
            self.visit_catch_clause(item);
        }
        self.visit_block(finalizer);
        self.leave_enum_statement_variant_try_finally_statement(
            body,
            catch_clause,
            finalizer,
        );
    }

    fn enter_enum_statement_variant_try_finally_statement(
        &mut self,
        body: &mut Block<'alloc>,
        catch_clause: &mut Option<CatchClause<'alloc>>,
        finalizer: &mut Block<'alloc>,
    ) {
    }

    fn leave_enum_statement_variant_try_finally_statement(
        &mut self,
        body: &mut Block<'alloc>,
        catch_clause: &mut Option<CatchClause<'alloc>>,
        finalizer: &mut Block<'alloc>,
    ) {
    }

    fn visit_enum_statement_variant_while_statement(
        &mut self,
        test: &mut arena::Box<'alloc, Expression<'alloc>>,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
        self.enter_enum_statement_variant_while_statement(
            test,
            block,
        );
        self.visit_expression(test);
        self.visit_statement(block);
        self.leave_enum_statement_variant_while_statement(
            test,
            block,
        );
    }

    fn enter_enum_statement_variant_while_statement(
        &mut self,
        test: &mut arena::Box<'alloc, Expression<'alloc>>,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
    }

    fn leave_enum_statement_variant_while_statement(
        &mut self,
        test: &mut arena::Box<'alloc, Expression<'alloc>>,
        block: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
    }

    fn visit_enum_statement_variant_with_statement(
        &mut self,
        object: &mut arena::Box<'alloc, Expression<'alloc>>,
        body: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
        self.enter_enum_statement_variant_with_statement(
            object,
            body,
        );
        self.visit_expression(object);
        self.visit_statement(body);
        self.leave_enum_statement_variant_with_statement(
            object,
            body,
        );
    }

    fn enter_enum_statement_variant_with_statement(
        &mut self,
        object: &mut arena::Box<'alloc, Expression<'alloc>>,
        body: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
    }

    fn leave_enum_statement_variant_with_statement(
        &mut self,
        object: &mut arena::Box<'alloc, Expression<'alloc>>,
        body: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
    }

    fn visit_enum_statement_variant_variable_declaration_statement(
        &mut self,
        ast: &mut VariableDeclaration<'alloc>,
    ) {
        self.enter_enum_statement_variant_variable_declaration_statement(ast);
        self.visit_variable_declaration(ast);
        self.leave_enum_statement_variant_variable_declaration_statement(ast);
    }

    fn enter_enum_statement_variant_variable_declaration_statement(
        &mut self,
        ast: &mut VariableDeclaration<'alloc>,
    ) {
    }

    fn leave_enum_statement_variant_variable_declaration_statement(
        &mut self,
        ast: &mut VariableDeclaration<'alloc>,
    ) {
    }

    fn visit_enum_statement_variant_function_declaration(
        &mut self,
        ast: &mut Function<'alloc>,
    ) {
        self.enter_enum_statement_variant_function_declaration(ast);
        self.visit_function(ast);
        self.leave_enum_statement_variant_function_declaration(ast);
    }

    fn enter_enum_statement_variant_function_declaration(
        &mut self,
        ast: &mut Function<'alloc>,
    ) {
    }

    fn leave_enum_statement_variant_function_declaration(
        &mut self,
        ast: &mut Function<'alloc>,
    ) {
    }

    fn visit_enum_statement_variant_class_declaration(
        &mut self,
        ast: &mut ClassDeclaration<'alloc>,
    ) {
        self.enter_enum_statement_variant_class_declaration(ast);
        self.visit_class_declaration(ast);
        self.leave_enum_statement_variant_class_declaration(ast);
    }

    fn enter_enum_statement_variant_class_declaration(
        &mut self,
        ast: &mut ClassDeclaration<'alloc>,
    ) {
    }

    fn leave_enum_statement_variant_class_declaration(
        &mut self,
        ast: &mut ClassDeclaration<'alloc>,
    ) {
    }

    fn visit_expression(&mut self, ast: &mut Expression<'alloc>) {
        self.enter_expression(ast);
        match ast {
            Expression::MemberExpression(ast) => {
                self.visit_enum_expression_variant_member_expression(ast)
            }
            Expression::ClassExpression(ast) => {
                self.visit_enum_expression_variant_class_expression(ast)
            }
            Expression::LiteralBooleanExpression { value, .. } => {
                self.visit_enum_expression_variant_literal_boolean_expression(
                    value,
                )
            }
            Expression::LiteralInfinityExpression { .. } => {
                self.visit_enum_expression_variant_literal_infinity_expression()
            }
            Expression::LiteralNullExpression { .. } => {
                self.visit_enum_expression_variant_literal_null_expression()
            }
            Expression::LiteralNumericExpression { value, .. } => {
                self.visit_enum_expression_variant_literal_numeric_expression(
                    value,
                )
            }
            Expression::LiteralRegExpExpression { pattern, global, ignore_case, multi_line, sticky, unicode, .. } => {
                self.visit_enum_expression_variant_literal_reg_exp_expression(
                    pattern,
                    global,
                    ignore_case,
                    multi_line,
                    sticky,
                    unicode,
                )
            }
            Expression::LiteralStringExpression { value, .. } => {
                self.visit_enum_expression_variant_literal_string_expression(
                    value,
                )
            }
            Expression::ArrayExpression(ast) => {
                self.visit_enum_expression_variant_array_expression(ast)
            }
            Expression::ArrowExpression { is_async, params, body, .. } => {
                self.visit_enum_expression_variant_arrow_expression(
                    is_async,
                    params,
                    body,
                )
            }
            Expression::AssignmentExpression { binding, expression, .. } => {
                self.visit_enum_expression_variant_assignment_expression(
                    binding,
                    expression,
                )
            }
            Expression::BinaryExpression { operator, left, right, .. } => {
                self.visit_enum_expression_variant_binary_expression(
                    operator,
                    left,
                    right,
                )
            }
            Expression::CallExpression(ast) => {
                self.visit_enum_expression_variant_call_expression(ast)
            }
            Expression::CompoundAssignmentExpression { operator, binding, expression, .. } => {
                self.visit_enum_expression_variant_compound_assignment_expression(
                    operator,
                    binding,
                    expression,
                )
            }
            Expression::ConditionalExpression { test, consequent, alternate, .. } => {
                self.visit_enum_expression_variant_conditional_expression(
                    test,
                    consequent,
                    alternate,
                )
            }
            Expression::FunctionExpression(ast) => {
                self.visit_enum_expression_variant_function_expression(ast)
            }
            Expression::IdentifierExpression(ast) => {
                self.visit_enum_expression_variant_identifier_expression(ast)
            }
            Expression::NewExpression { callee, arguments, .. } => {
                self.visit_enum_expression_variant_new_expression(
                    callee,
                    arguments,
                )
            }
            Expression::NewTargetExpression { .. } => {
                self.visit_enum_expression_variant_new_target_expression()
            }
            Expression::ObjectExpression(ast) => {
                self.visit_enum_expression_variant_object_expression(ast)
            }
            Expression::OptionalExpression { object, tail, .. } => {
                self.visit_enum_expression_variant_optional_expression(
                    object,
                    tail,
                )
            }
            Expression::OptionalChain(ast) => {
                self.visit_enum_expression_variant_optional_chain(ast)
            }
            Expression::UnaryExpression { operator, operand, .. } => {
                self.visit_enum_expression_variant_unary_expression(
                    operator,
                    operand,
                )
            }
            Expression::TemplateExpression(ast) => {
                self.visit_enum_expression_variant_template_expression(ast)
            }
            Expression::ThisExpression { .. } => {
                self.visit_enum_expression_variant_this_expression()
            }
            Expression::UpdateExpression { is_prefix, operator, operand, .. } => {
                self.visit_enum_expression_variant_update_expression(
                    is_prefix,
                    operator,
                    operand,
                )
            }
            Expression::YieldExpression { expression, .. } => {
                self.visit_enum_expression_variant_yield_expression(
                    expression,
                )
            }
            Expression::YieldGeneratorExpression { expression, .. } => {
                self.visit_enum_expression_variant_yield_generator_expression(
                    expression,
                )
            }
            Expression::AwaitExpression { expression, .. } => {
                self.visit_enum_expression_variant_await_expression(
                    expression,
                )
            }
            Expression::ImportCallExpression { argument, .. } => {
                self.visit_enum_expression_variant_import_call_expression(
                    argument,
                )
            }
        }
        self.leave_expression(ast);
    }

    fn enter_expression(&mut self, ast: &mut Expression<'alloc>) {
    }

    fn leave_expression(&mut self, ast: &mut Expression<'alloc>) {
    }

    fn visit_enum_expression_variant_member_expression(
        &mut self,
        ast: &mut MemberExpression<'alloc>,
    ) {
        self.enter_enum_expression_variant_member_expression(ast);
        self.visit_member_expression(ast);
        self.leave_enum_expression_variant_member_expression(ast);
    }

    fn enter_enum_expression_variant_member_expression(
        &mut self,
        ast: &mut MemberExpression<'alloc>,
    ) {
    }

    fn leave_enum_expression_variant_member_expression(
        &mut self,
        ast: &mut MemberExpression<'alloc>,
    ) {
    }

    fn visit_enum_expression_variant_class_expression(
        &mut self,
        ast: &mut ClassExpression<'alloc>,
    ) {
        self.enter_enum_expression_variant_class_expression(ast);
        self.visit_class_expression(ast);
        self.leave_enum_expression_variant_class_expression(ast);
    }

    fn enter_enum_expression_variant_class_expression(
        &mut self,
        ast: &mut ClassExpression<'alloc>,
    ) {
    }

    fn leave_enum_expression_variant_class_expression(
        &mut self,
        ast: &mut ClassExpression<'alloc>,
    ) {
    }

    fn visit_enum_expression_variant_literal_boolean_expression(
        &mut self,
        value: &mut bool,
    ) {
        self.enter_enum_expression_variant_literal_boolean_expression(
            value,
        );
        self.leave_enum_expression_variant_literal_boolean_expression(
            value,
        );
    }

    fn enter_enum_expression_variant_literal_boolean_expression(
        &mut self,
        value: &mut bool,
    ) {
    }

    fn leave_enum_expression_variant_literal_boolean_expression(
        &mut self,
        value: &mut bool,
    ) {
    }

    fn visit_enum_expression_variant_literal_infinity_expression(&mut self) {
    }

    fn visit_enum_expression_variant_literal_null_expression(&mut self) {
    }

    fn visit_enum_expression_variant_literal_numeric_expression(
        &mut self,
        value: &mut f64,
    ) {
        self.enter_enum_expression_variant_literal_numeric_expression(
            value,
        );
        self.leave_enum_expression_variant_literal_numeric_expression(
            value,
        );
    }

    fn enter_enum_expression_variant_literal_numeric_expression(
        &mut self,
        value: &mut f64,
    ) {
    }

    fn leave_enum_expression_variant_literal_numeric_expression(
        &mut self,
        value: &mut f64,
    ) {
    }

    fn visit_enum_expression_variant_literal_reg_exp_expression(
        &mut self,
        pattern: &mut &'alloc str,
        global: &mut bool,
        ignore_case: &mut bool,
        multi_line: &mut bool,
        sticky: &mut bool,
        unicode: &mut bool,
    ) {
        self.enter_enum_expression_variant_literal_reg_exp_expression(
            pattern,
            global,
            ignore_case,
            multi_line,
            sticky,
            unicode,
        );
        self.leave_enum_expression_variant_literal_reg_exp_expression(
            pattern,
            global,
            ignore_case,
            multi_line,
            sticky,
            unicode,
        );
    }

    fn enter_enum_expression_variant_literal_reg_exp_expression(
        &mut self,
        pattern: &mut &'alloc str,
        global: &mut bool,
        ignore_case: &mut bool,
        multi_line: &mut bool,
        sticky: &mut bool,
        unicode: &mut bool,
    ) {
    }

    fn leave_enum_expression_variant_literal_reg_exp_expression(
        &mut self,
        pattern: &mut &'alloc str,
        global: &mut bool,
        ignore_case: &mut bool,
        multi_line: &mut bool,
        sticky: &mut bool,
        unicode: &mut bool,
    ) {
    }

    fn visit_enum_expression_variant_literal_string_expression(
        &mut self,
        value: &mut &'alloc str,
    ) {
        self.enter_enum_expression_variant_literal_string_expression(
            value,
        );
        self.leave_enum_expression_variant_literal_string_expression(
            value,
        );
    }

    fn enter_enum_expression_variant_literal_string_expression(
        &mut self,
        value: &mut &'alloc str,
    ) {
    }

    fn leave_enum_expression_variant_literal_string_expression(
        &mut self,
        value: &mut &'alloc str,
    ) {
    }

    fn visit_enum_expression_variant_array_expression(
        &mut self,
        ast: &mut ArrayExpression<'alloc>,
    ) {
        self.enter_enum_expression_variant_array_expression(ast);
        self.visit_array_expression(ast);
        self.leave_enum_expression_variant_array_expression(ast);
    }

    fn enter_enum_expression_variant_array_expression(
        &mut self,
        ast: &mut ArrayExpression<'alloc>,
    ) {
    }

    fn leave_enum_expression_variant_array_expression(
        &mut self,
        ast: &mut ArrayExpression<'alloc>,
    ) {
    }

    fn visit_enum_expression_variant_arrow_expression(
        &mut self,
        is_async: &mut bool,
        params: &mut FormalParameters<'alloc>,
        body: &mut ArrowExpressionBody<'alloc>,
    ) {
        self.enter_enum_expression_variant_arrow_expression(
            is_async,
            params,
            body,
        );
        self.visit_formal_parameters(params);
        self.visit_arrow_expression_body(body);
        self.leave_enum_expression_variant_arrow_expression(
            is_async,
            params,
            body,
        );
    }

    fn enter_enum_expression_variant_arrow_expression(
        &mut self,
        is_async: &mut bool,
        params: &mut FormalParameters<'alloc>,
        body: &mut ArrowExpressionBody<'alloc>,
    ) {
    }

    fn leave_enum_expression_variant_arrow_expression(
        &mut self,
        is_async: &mut bool,
        params: &mut FormalParameters<'alloc>,
        body: &mut ArrowExpressionBody<'alloc>,
    ) {
    }

    fn visit_enum_expression_variant_assignment_expression(
        &mut self,
        binding: &mut AssignmentTarget<'alloc>,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_expression_variant_assignment_expression(
            binding,
            expression,
        );
        self.visit_assignment_target(binding);
        self.visit_expression(expression);
        self.leave_enum_expression_variant_assignment_expression(
            binding,
            expression,
        );
    }

    fn enter_enum_expression_variant_assignment_expression(
        &mut self,
        binding: &mut AssignmentTarget<'alloc>,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_expression_variant_assignment_expression(
        &mut self,
        binding: &mut AssignmentTarget<'alloc>,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_expression_variant_binary_expression(
        &mut self,
        operator: &mut BinaryOperator,
        left: &mut arena::Box<'alloc, Expression<'alloc>>,
        right: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_expression_variant_binary_expression(
            operator,
            left,
            right,
        );
        self.visit_binary_operator(operator);
        self.visit_expression(left);
        self.visit_expression(right);
        self.leave_enum_expression_variant_binary_expression(
            operator,
            left,
            right,
        );
    }

    fn enter_enum_expression_variant_binary_expression(
        &mut self,
        operator: &mut BinaryOperator,
        left: &mut arena::Box<'alloc, Expression<'alloc>>,
        right: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_expression_variant_binary_expression(
        &mut self,
        operator: &mut BinaryOperator,
        left: &mut arena::Box<'alloc, Expression<'alloc>>,
        right: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_expression_variant_call_expression(
        &mut self,
        ast: &mut CallExpression<'alloc>,
    ) {
        self.enter_enum_expression_variant_call_expression(ast);
        self.visit_call_expression(ast);
        self.leave_enum_expression_variant_call_expression(ast);
    }

    fn enter_enum_expression_variant_call_expression(
        &mut self,
        ast: &mut CallExpression<'alloc>,
    ) {
    }

    fn leave_enum_expression_variant_call_expression(
        &mut self,
        ast: &mut CallExpression<'alloc>,
    ) {
    }

    fn visit_enum_expression_variant_compound_assignment_expression(
        &mut self,
        operator: &mut CompoundAssignmentOperator,
        binding: &mut SimpleAssignmentTarget<'alloc>,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_expression_variant_compound_assignment_expression(
            operator,
            binding,
            expression,
        );
        self.visit_compound_assignment_operator(operator);
        self.visit_simple_assignment_target(binding);
        self.visit_expression(expression);
        self.leave_enum_expression_variant_compound_assignment_expression(
            operator,
            binding,
            expression,
        );
    }

    fn enter_enum_expression_variant_compound_assignment_expression(
        &mut self,
        operator: &mut CompoundAssignmentOperator,
        binding: &mut SimpleAssignmentTarget<'alloc>,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_expression_variant_compound_assignment_expression(
        &mut self,
        operator: &mut CompoundAssignmentOperator,
        binding: &mut SimpleAssignmentTarget<'alloc>,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_expression_variant_conditional_expression(
        &mut self,
        test: &mut arena::Box<'alloc, Expression<'alloc>>,
        consequent: &mut arena::Box<'alloc, Expression<'alloc>>,
        alternate: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_expression_variant_conditional_expression(
            test,
            consequent,
            alternate,
        );
        self.visit_expression(test);
        self.visit_expression(consequent);
        self.visit_expression(alternate);
        self.leave_enum_expression_variant_conditional_expression(
            test,
            consequent,
            alternate,
        );
    }

    fn enter_enum_expression_variant_conditional_expression(
        &mut self,
        test: &mut arena::Box<'alloc, Expression<'alloc>>,
        consequent: &mut arena::Box<'alloc, Expression<'alloc>>,
        alternate: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_expression_variant_conditional_expression(
        &mut self,
        test: &mut arena::Box<'alloc, Expression<'alloc>>,
        consequent: &mut arena::Box<'alloc, Expression<'alloc>>,
        alternate: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_expression_variant_function_expression(
        &mut self,
        ast: &mut Function<'alloc>,
    ) {
        self.enter_enum_expression_variant_function_expression(ast);
        self.visit_function(ast);
        self.leave_enum_expression_variant_function_expression(ast);
    }

    fn enter_enum_expression_variant_function_expression(
        &mut self,
        ast: &mut Function<'alloc>,
    ) {
    }

    fn leave_enum_expression_variant_function_expression(
        &mut self,
        ast: &mut Function<'alloc>,
    ) {
    }

    fn visit_enum_expression_variant_identifier_expression(
        &mut self,
        ast: &mut IdentifierExpression<'alloc>,
    ) {
        self.enter_enum_expression_variant_identifier_expression(ast);
        self.visit_identifier_expression(ast);
        self.leave_enum_expression_variant_identifier_expression(ast);
    }

    fn enter_enum_expression_variant_identifier_expression(
        &mut self,
        ast: &mut IdentifierExpression<'alloc>,
    ) {
    }

    fn leave_enum_expression_variant_identifier_expression(
        &mut self,
        ast: &mut IdentifierExpression<'alloc>,
    ) {
    }

    fn visit_enum_expression_variant_new_expression(
        &mut self,
        callee: &mut arena::Box<'alloc, Expression<'alloc>>,
        arguments: &mut Arguments<'alloc>,
    ) {
        self.enter_enum_expression_variant_new_expression(
            callee,
            arguments,
        );
        self.visit_expression(callee);
        self.visit_arguments(arguments);
        self.leave_enum_expression_variant_new_expression(
            callee,
            arguments,
        );
    }

    fn enter_enum_expression_variant_new_expression(
        &mut self,
        callee: &mut arena::Box<'alloc, Expression<'alloc>>,
        arguments: &mut Arguments<'alloc>,
    ) {
    }

    fn leave_enum_expression_variant_new_expression(
        &mut self,
        callee: &mut arena::Box<'alloc, Expression<'alloc>>,
        arguments: &mut Arguments<'alloc>,
    ) {
    }

    fn visit_enum_expression_variant_new_target_expression(&mut self) {
    }

    fn visit_enum_expression_variant_object_expression(
        &mut self,
        ast: &mut ObjectExpression<'alloc>,
    ) {
        self.enter_enum_expression_variant_object_expression(ast);
        self.visit_object_expression(ast);
        self.leave_enum_expression_variant_object_expression(ast);
    }

    fn enter_enum_expression_variant_object_expression(
        &mut self,
        ast: &mut ObjectExpression<'alloc>,
    ) {
    }

    fn leave_enum_expression_variant_object_expression(
        &mut self,
        ast: &mut ObjectExpression<'alloc>,
    ) {
    }

    fn visit_enum_expression_variant_optional_expression(
        &mut self,
        object: &mut ExpressionOrSuper<'alloc>,
        tail: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_expression_variant_optional_expression(
            object,
            tail,
        );
        self.visit_expression_or_super(object);
        self.visit_expression(tail);
        self.leave_enum_expression_variant_optional_expression(
            object,
            tail,
        );
    }

    fn enter_enum_expression_variant_optional_expression(
        &mut self,
        object: &mut ExpressionOrSuper<'alloc>,
        tail: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_expression_variant_optional_expression(
        &mut self,
        object: &mut ExpressionOrSuper<'alloc>,
        tail: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_expression_variant_optional_chain(
        &mut self,
        ast: &mut OptionalChain<'alloc>,
    ) {
        self.enter_enum_expression_variant_optional_chain(ast);
        self.visit_optional_chain(ast);
        self.leave_enum_expression_variant_optional_chain(ast);
    }

    fn enter_enum_expression_variant_optional_chain(
        &mut self,
        ast: &mut OptionalChain<'alloc>,
    ) {
    }

    fn leave_enum_expression_variant_optional_chain(
        &mut self,
        ast: &mut OptionalChain<'alloc>,
    ) {
    }

    fn visit_enum_expression_variant_unary_expression(
        &mut self,
        operator: &mut UnaryOperator,
        operand: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_expression_variant_unary_expression(
            operator,
            operand,
        );
        self.visit_unary_operator(operator);
        self.visit_expression(operand);
        self.leave_enum_expression_variant_unary_expression(
            operator,
            operand,
        );
    }

    fn enter_enum_expression_variant_unary_expression(
        &mut self,
        operator: &mut UnaryOperator,
        operand: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_expression_variant_unary_expression(
        &mut self,
        operator: &mut UnaryOperator,
        operand: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_expression_variant_template_expression(
        &mut self,
        ast: &mut TemplateExpression<'alloc>,
    ) {
        self.enter_enum_expression_variant_template_expression(ast);
        self.visit_template_expression(ast);
        self.leave_enum_expression_variant_template_expression(ast);
    }

    fn enter_enum_expression_variant_template_expression(
        &mut self,
        ast: &mut TemplateExpression<'alloc>,
    ) {
    }

    fn leave_enum_expression_variant_template_expression(
        &mut self,
        ast: &mut TemplateExpression<'alloc>,
    ) {
    }

    fn visit_enum_expression_variant_this_expression(&mut self) {
    }

    fn visit_enum_expression_variant_update_expression(
        &mut self,
        is_prefix: &mut bool,
        operator: &mut UpdateOperator,
        operand: &mut SimpleAssignmentTarget<'alloc>,
    ) {
        self.enter_enum_expression_variant_update_expression(
            is_prefix,
            operator,
            operand,
        );
        self.visit_update_operator(operator);
        self.visit_simple_assignment_target(operand);
        self.leave_enum_expression_variant_update_expression(
            is_prefix,
            operator,
            operand,
        );
    }

    fn enter_enum_expression_variant_update_expression(
        &mut self,
        is_prefix: &mut bool,
        operator: &mut UpdateOperator,
        operand: &mut SimpleAssignmentTarget<'alloc>,
    ) {
    }

    fn leave_enum_expression_variant_update_expression(
        &mut self,
        is_prefix: &mut bool,
        operator: &mut UpdateOperator,
        operand: &mut SimpleAssignmentTarget<'alloc>,
    ) {
    }

    fn visit_enum_expression_variant_yield_expression(
        &mut self,
        expression: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) {
        self.enter_enum_expression_variant_yield_expression(
            expression,
        );
        if let Some(item) = expression {
            self.visit_expression(item);
        }
        self.leave_enum_expression_variant_yield_expression(
            expression,
        );
    }

    fn enter_enum_expression_variant_yield_expression(
        &mut self,
        expression: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) {
    }

    fn leave_enum_expression_variant_yield_expression(
        &mut self,
        expression: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) {
    }

    fn visit_enum_expression_variant_yield_generator_expression(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_expression_variant_yield_generator_expression(
            expression,
        );
        self.visit_expression(expression);
        self.leave_enum_expression_variant_yield_generator_expression(
            expression,
        );
    }

    fn enter_enum_expression_variant_yield_generator_expression(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_expression_variant_yield_generator_expression(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_expression_variant_await_expression(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_expression_variant_await_expression(
            expression,
        );
        self.visit_expression(expression);
        self.leave_enum_expression_variant_await_expression(
            expression,
        );
    }

    fn enter_enum_expression_variant_await_expression(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_expression_variant_await_expression(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_expression_variant_import_call_expression(
        &mut self,
        argument: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_expression_variant_import_call_expression(
            argument,
        );
        self.visit_expression(argument);
        self.leave_enum_expression_variant_import_call_expression(
            argument,
        );
    }

    fn enter_enum_expression_variant_import_call_expression(
        &mut self,
        argument: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_expression_variant_import_call_expression(
        &mut self,
        argument: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_member_expression(&mut self, ast: &mut MemberExpression<'alloc>) {
        self.enter_member_expression(ast);
        match ast {
            MemberExpression::ComputedMemberExpression(ast) => {
                self.visit_enum_member_expression_variant_computed_member_expression(ast)
            }
            MemberExpression::StaticMemberExpression(ast) => {
                self.visit_enum_member_expression_variant_static_member_expression(ast)
            }
            MemberExpression::PrivateFieldExpression(ast) => {
                self.visit_enum_member_expression_variant_private_field_expression(ast)
            }
        }
        self.leave_member_expression(ast);
    }

    fn enter_member_expression(&mut self, ast: &mut MemberExpression<'alloc>) {
    }

    fn leave_member_expression(&mut self, ast: &mut MemberExpression<'alloc>) {
    }

    fn visit_enum_member_expression_variant_computed_member_expression(
        &mut self,
        ast: &mut ComputedMemberExpression<'alloc>,
    ) {
        self.enter_enum_member_expression_variant_computed_member_expression(ast);
        self.visit_computed_member_expression(ast);
        self.leave_enum_member_expression_variant_computed_member_expression(ast);
    }

    fn enter_enum_member_expression_variant_computed_member_expression(
        &mut self,
        ast: &mut ComputedMemberExpression<'alloc>,
    ) {
    }

    fn leave_enum_member_expression_variant_computed_member_expression(
        &mut self,
        ast: &mut ComputedMemberExpression<'alloc>,
    ) {
    }

    fn visit_enum_member_expression_variant_static_member_expression(
        &mut self,
        ast: &mut StaticMemberExpression<'alloc>,
    ) {
        self.enter_enum_member_expression_variant_static_member_expression(ast);
        self.visit_static_member_expression(ast);
        self.leave_enum_member_expression_variant_static_member_expression(ast);
    }

    fn enter_enum_member_expression_variant_static_member_expression(
        &mut self,
        ast: &mut StaticMemberExpression<'alloc>,
    ) {
    }

    fn leave_enum_member_expression_variant_static_member_expression(
        &mut self,
        ast: &mut StaticMemberExpression<'alloc>,
    ) {
    }

    fn visit_enum_member_expression_variant_private_field_expression(
        &mut self,
        ast: &mut PrivateFieldExpression<'alloc>,
    ) {
        self.enter_enum_member_expression_variant_private_field_expression(ast);
        self.visit_private_field_expression(ast);
        self.leave_enum_member_expression_variant_private_field_expression(ast);
    }

    fn enter_enum_member_expression_variant_private_field_expression(
        &mut self,
        ast: &mut PrivateFieldExpression<'alloc>,
    ) {
    }

    fn leave_enum_member_expression_variant_private_field_expression(
        &mut self,
        ast: &mut PrivateFieldExpression<'alloc>,
    ) {
    }

    fn visit_optional_chain(&mut self, ast: &mut OptionalChain<'alloc>) {
        self.enter_optional_chain(ast);
        match ast {
            OptionalChain::ComputedMemberExpressionTail { expression, .. } => {
                self.visit_enum_optional_chain_variant_computed_member_expression_tail(
                    expression,
                )
            }
            OptionalChain::StaticMemberExpressionTail { property, .. } => {
                self.visit_enum_optional_chain_variant_static_member_expression_tail(
                    property,
                )
            }
            OptionalChain::CallExpressionTail { arguments, .. } => {
                self.visit_enum_optional_chain_variant_call_expression_tail(
                    arguments,
                )
            }
            OptionalChain::ComputedMemberExpression(ast) => {
                self.visit_enum_optional_chain_variant_computed_member_expression(ast)
            }
            OptionalChain::StaticMemberExpression(ast) => {
                self.visit_enum_optional_chain_variant_static_member_expression(ast)
            }
            OptionalChain::CallExpression(ast) => {
                self.visit_enum_optional_chain_variant_call_expression(ast)
            }
        }
        self.leave_optional_chain(ast);
    }

    fn enter_optional_chain(&mut self, ast: &mut OptionalChain<'alloc>) {
    }

    fn leave_optional_chain(&mut self, ast: &mut OptionalChain<'alloc>) {
    }

    fn visit_enum_optional_chain_variant_computed_member_expression_tail(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_optional_chain_variant_computed_member_expression_tail(
            expression,
        );
        self.visit_expression(expression);
        self.leave_enum_optional_chain_variant_computed_member_expression_tail(
            expression,
        );
    }

    fn enter_enum_optional_chain_variant_computed_member_expression_tail(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_optional_chain_variant_computed_member_expression_tail(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_optional_chain_variant_static_member_expression_tail(
        &mut self,
        property: &mut IdentifierName<'alloc>,
    ) {
        self.enter_enum_optional_chain_variant_static_member_expression_tail(
            property,
        );
        self.visit_identifier_name(property);
        self.leave_enum_optional_chain_variant_static_member_expression_tail(
            property,
        );
    }

    fn enter_enum_optional_chain_variant_static_member_expression_tail(
        &mut self,
        property: &mut IdentifierName<'alloc>,
    ) {
    }

    fn leave_enum_optional_chain_variant_static_member_expression_tail(
        &mut self,
        property: &mut IdentifierName<'alloc>,
    ) {
    }

    fn visit_enum_optional_chain_variant_call_expression_tail(
        &mut self,
        arguments: &mut Arguments<'alloc>,
    ) {
        self.enter_enum_optional_chain_variant_call_expression_tail(
            arguments,
        );
        self.visit_arguments(arguments);
        self.leave_enum_optional_chain_variant_call_expression_tail(
            arguments,
        );
    }

    fn enter_enum_optional_chain_variant_call_expression_tail(
        &mut self,
        arguments: &mut Arguments<'alloc>,
    ) {
    }

    fn leave_enum_optional_chain_variant_call_expression_tail(
        &mut self,
        arguments: &mut Arguments<'alloc>,
    ) {
    }

    fn visit_enum_optional_chain_variant_computed_member_expression(
        &mut self,
        ast: &mut ComputedMemberExpression<'alloc>,
    ) {
        self.enter_enum_optional_chain_variant_computed_member_expression(ast);
        self.visit_computed_member_expression(ast);
        self.leave_enum_optional_chain_variant_computed_member_expression(ast);
    }

    fn enter_enum_optional_chain_variant_computed_member_expression(
        &mut self,
        ast: &mut ComputedMemberExpression<'alloc>,
    ) {
    }

    fn leave_enum_optional_chain_variant_computed_member_expression(
        &mut self,
        ast: &mut ComputedMemberExpression<'alloc>,
    ) {
    }

    fn visit_enum_optional_chain_variant_static_member_expression(
        &mut self,
        ast: &mut StaticMemberExpression<'alloc>,
    ) {
        self.enter_enum_optional_chain_variant_static_member_expression(ast);
        self.visit_static_member_expression(ast);
        self.leave_enum_optional_chain_variant_static_member_expression(ast);
    }

    fn enter_enum_optional_chain_variant_static_member_expression(
        &mut self,
        ast: &mut StaticMemberExpression<'alloc>,
    ) {
    }

    fn leave_enum_optional_chain_variant_static_member_expression(
        &mut self,
        ast: &mut StaticMemberExpression<'alloc>,
    ) {
    }

    fn visit_enum_optional_chain_variant_call_expression(
        &mut self,
        ast: &mut CallExpression<'alloc>,
    ) {
        self.enter_enum_optional_chain_variant_call_expression(ast);
        self.visit_call_expression(ast);
        self.leave_enum_optional_chain_variant_call_expression(ast);
    }

    fn enter_enum_optional_chain_variant_call_expression(
        &mut self,
        ast: &mut CallExpression<'alloc>,
    ) {
    }

    fn leave_enum_optional_chain_variant_call_expression(
        &mut self,
        ast: &mut CallExpression<'alloc>,
    ) {
    }

    fn visit_property_name(&mut self, ast: &mut PropertyName<'alloc>) {
        self.enter_property_name(ast);
        match ast {
            PropertyName::ComputedPropertyName(ast) => {
                self.visit_enum_property_name_variant_computed_property_name(ast)
            }
            PropertyName::StaticPropertyName(ast) => {
                self.visit_enum_property_name_variant_static_property_name(ast)
            }
        }
        self.leave_property_name(ast);
    }

    fn enter_property_name(&mut self, ast: &mut PropertyName<'alloc>) {
    }

    fn leave_property_name(&mut self, ast: &mut PropertyName<'alloc>) {
    }

    fn visit_enum_property_name_variant_computed_property_name(
        &mut self,
        ast: &mut ComputedPropertyName<'alloc>,
    ) {
        self.enter_enum_property_name_variant_computed_property_name(ast);
        self.visit_computed_property_name(ast);
        self.leave_enum_property_name_variant_computed_property_name(ast);
    }

    fn enter_enum_property_name_variant_computed_property_name(
        &mut self,
        ast: &mut ComputedPropertyName<'alloc>,
    ) {
    }

    fn leave_enum_property_name_variant_computed_property_name(
        &mut self,
        ast: &mut ComputedPropertyName<'alloc>,
    ) {
    }

    fn visit_enum_property_name_variant_static_property_name(
        &mut self,
        ast: &mut StaticPropertyName<'alloc>,
    ) {
        self.enter_enum_property_name_variant_static_property_name(ast);
        self.visit_static_property_name(ast);
        self.leave_enum_property_name_variant_static_property_name(ast);
    }

    fn enter_enum_property_name_variant_static_property_name(
        &mut self,
        ast: &mut StaticPropertyName<'alloc>,
    ) {
    }

    fn leave_enum_property_name_variant_static_property_name(
        &mut self,
        ast: &mut StaticPropertyName<'alloc>,
    ) {
    }

    fn visit_call_expression(&mut self, ast: &mut CallExpression<'alloc>) {
        self.enter_call_expression(ast);
        self.visit_expression_or_super(&mut ast.callee);
        self.visit_arguments(&mut ast.arguments);
        self.leave_call_expression(ast);
    }

    fn enter_call_expression(&mut self, ast: &mut CallExpression<'alloc>) {
    }

    fn leave_call_expression(&mut self, ast: &mut CallExpression<'alloc>) {
    }

    fn visit_class_element_name(&mut self, ast: &mut ClassElementName<'alloc>) {
        self.enter_class_element_name(ast);
        match ast {
            ClassElementName::ComputedPropertyName(ast) => {
                self.visit_enum_class_element_name_variant_computed_property_name(ast)
            }
            ClassElementName::StaticPropertyName(ast) => {
                self.visit_enum_class_element_name_variant_static_property_name(ast)
            }
            ClassElementName::PrivateFieldName(ast) => {
                self.visit_enum_class_element_name_variant_private_field_name(ast)
            }
        }
        self.leave_class_element_name(ast);
    }

    fn enter_class_element_name(&mut self, ast: &mut ClassElementName<'alloc>) {
    }

    fn leave_class_element_name(&mut self, ast: &mut ClassElementName<'alloc>) {
    }

    fn visit_enum_class_element_name_variant_computed_property_name(
        &mut self,
        ast: &mut ComputedPropertyName<'alloc>,
    ) {
        self.enter_enum_class_element_name_variant_computed_property_name(ast);
        self.visit_computed_property_name(ast);
        self.leave_enum_class_element_name_variant_computed_property_name(ast);
    }

    fn enter_enum_class_element_name_variant_computed_property_name(
        &mut self,
        ast: &mut ComputedPropertyName<'alloc>,
    ) {
    }

    fn leave_enum_class_element_name_variant_computed_property_name(
        &mut self,
        ast: &mut ComputedPropertyName<'alloc>,
    ) {
    }

    fn visit_enum_class_element_name_variant_static_property_name(
        &mut self,
        ast: &mut StaticPropertyName<'alloc>,
    ) {
        self.enter_enum_class_element_name_variant_static_property_name(ast);
        self.visit_static_property_name(ast);
        self.leave_enum_class_element_name_variant_static_property_name(ast);
    }

    fn enter_enum_class_element_name_variant_static_property_name(
        &mut self,
        ast: &mut StaticPropertyName<'alloc>,
    ) {
    }

    fn leave_enum_class_element_name_variant_static_property_name(
        &mut self,
        ast: &mut StaticPropertyName<'alloc>,
    ) {
    }

    fn visit_enum_class_element_name_variant_private_field_name(
        &mut self,
        ast: &mut PrivateIdentifier<'alloc>,
    ) {
        self.enter_enum_class_element_name_variant_private_field_name(ast);
        self.visit_private_identifier(ast);
        self.leave_enum_class_element_name_variant_private_field_name(ast);
    }

    fn enter_enum_class_element_name_variant_private_field_name(
        &mut self,
        ast: &mut PrivateIdentifier<'alloc>,
    ) {
    }

    fn leave_enum_class_element_name_variant_private_field_name(
        &mut self,
        ast: &mut PrivateIdentifier<'alloc>,
    ) {
    }

    fn visit_object_property(&mut self, ast: &mut ObjectProperty<'alloc>) {
        self.enter_object_property(ast);
        match ast {
            ObjectProperty::NamedObjectProperty(ast) => {
                self.visit_enum_object_property_variant_named_object_property(ast)
            }
            ObjectProperty::ShorthandProperty(ast) => {
                self.visit_enum_object_property_variant_shorthand_property(ast)
            }
            ObjectProperty::SpreadProperty(ast) => {
                self.visit_enum_object_property_variant_spread_property(ast)
            }
        }
        self.leave_object_property(ast);
    }

    fn enter_object_property(&mut self, ast: &mut ObjectProperty<'alloc>) {
    }

    fn leave_object_property(&mut self, ast: &mut ObjectProperty<'alloc>) {
    }

    fn visit_enum_object_property_variant_named_object_property(
        &mut self,
        ast: &mut NamedObjectProperty<'alloc>,
    ) {
        self.enter_enum_object_property_variant_named_object_property(ast);
        self.visit_named_object_property(ast);
        self.leave_enum_object_property_variant_named_object_property(ast);
    }

    fn enter_enum_object_property_variant_named_object_property(
        &mut self,
        ast: &mut NamedObjectProperty<'alloc>,
    ) {
    }

    fn leave_enum_object_property_variant_named_object_property(
        &mut self,
        ast: &mut NamedObjectProperty<'alloc>,
    ) {
    }

    fn visit_enum_object_property_variant_shorthand_property(
        &mut self,
        ast: &mut ShorthandProperty<'alloc>,
    ) {
        self.enter_enum_object_property_variant_shorthand_property(ast);
        self.visit_shorthand_property(ast);
        self.leave_enum_object_property_variant_shorthand_property(ast);
    }

    fn enter_enum_object_property_variant_shorthand_property(
        &mut self,
        ast: &mut ShorthandProperty<'alloc>,
    ) {
    }

    fn leave_enum_object_property_variant_shorthand_property(
        &mut self,
        ast: &mut ShorthandProperty<'alloc>,
    ) {
    }

    fn visit_enum_object_property_variant_spread_property(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_object_property_variant_spread_property(ast);
        self.visit_expression(ast);
        self.leave_enum_object_property_variant_spread_property(ast);
    }

    fn enter_enum_object_property_variant_spread_property(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_object_property_variant_spread_property(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_named_object_property(&mut self, ast: &mut NamedObjectProperty<'alloc>) {
        self.enter_named_object_property(ast);
        match ast {
            NamedObjectProperty::MethodDefinition(ast) => {
                self.visit_enum_named_object_property_variant_method_definition(ast)
            }
            NamedObjectProperty::DataProperty(ast) => {
                self.visit_enum_named_object_property_variant_data_property(ast)
            }
        }
        self.leave_named_object_property(ast);
    }

    fn enter_named_object_property(&mut self, ast: &mut NamedObjectProperty<'alloc>) {
    }

    fn leave_named_object_property(&mut self, ast: &mut NamedObjectProperty<'alloc>) {
    }

    fn visit_enum_named_object_property_variant_method_definition(
        &mut self,
        ast: &mut MethodDefinition<'alloc>,
    ) {
        self.enter_enum_named_object_property_variant_method_definition(ast);
        self.visit_method_definition(ast);
        self.leave_enum_named_object_property_variant_method_definition(ast);
    }

    fn enter_enum_named_object_property_variant_method_definition(
        &mut self,
        ast: &mut MethodDefinition<'alloc>,
    ) {
    }

    fn leave_enum_named_object_property_variant_method_definition(
        &mut self,
        ast: &mut MethodDefinition<'alloc>,
    ) {
    }

    fn visit_enum_named_object_property_variant_data_property(
        &mut self,
        ast: &mut DataProperty<'alloc>,
    ) {
        self.enter_enum_named_object_property_variant_data_property(ast);
        self.visit_data_property(ast);
        self.leave_enum_named_object_property_variant_data_property(ast);
    }

    fn enter_enum_named_object_property_variant_data_property(
        &mut self,
        ast: &mut DataProperty<'alloc>,
    ) {
    }

    fn leave_enum_named_object_property_variant_data_property(
        &mut self,
        ast: &mut DataProperty<'alloc>,
    ) {
    }

    fn visit_method_definition(&mut self, ast: &mut MethodDefinition<'alloc>) {
        self.enter_method_definition(ast);
        match ast {
            MethodDefinition::Method(ast) => {
                self.visit_enum_method_definition_variant_method(ast)
            }
            MethodDefinition::Getter(ast) => {
                self.visit_enum_method_definition_variant_getter(ast)
            }
            MethodDefinition::Setter(ast) => {
                self.visit_enum_method_definition_variant_setter(ast)
            }
        }
        self.leave_method_definition(ast);
    }

    fn enter_method_definition(&mut self, ast: &mut MethodDefinition<'alloc>) {
    }

    fn leave_method_definition(&mut self, ast: &mut MethodDefinition<'alloc>) {
    }

    fn visit_enum_method_definition_variant_method(
        &mut self,
        ast: &mut Method<'alloc>,
    ) {
        self.enter_enum_method_definition_variant_method(ast);
        self.visit_method(ast);
        self.leave_enum_method_definition_variant_method(ast);
    }

    fn enter_enum_method_definition_variant_method(
        &mut self,
        ast: &mut Method<'alloc>,
    ) {
    }

    fn leave_enum_method_definition_variant_method(
        &mut self,
        ast: &mut Method<'alloc>,
    ) {
    }

    fn visit_enum_method_definition_variant_getter(
        &mut self,
        ast: &mut Getter<'alloc>,
    ) {
        self.enter_enum_method_definition_variant_getter(ast);
        self.visit_getter(ast);
        self.leave_enum_method_definition_variant_getter(ast);
    }

    fn enter_enum_method_definition_variant_getter(
        &mut self,
        ast: &mut Getter<'alloc>,
    ) {
    }

    fn leave_enum_method_definition_variant_getter(
        &mut self,
        ast: &mut Getter<'alloc>,
    ) {
    }

    fn visit_enum_method_definition_variant_setter(
        &mut self,
        ast: &mut Setter<'alloc>,
    ) {
        self.enter_enum_method_definition_variant_setter(ast);
        self.visit_setter(ast);
        self.leave_enum_method_definition_variant_setter(ast);
    }

    fn enter_enum_method_definition_variant_setter(
        &mut self,
        ast: &mut Setter<'alloc>,
    ) {
    }

    fn leave_enum_method_definition_variant_setter(
        &mut self,
        ast: &mut Setter<'alloc>,
    ) {
    }

    fn visit_import_declaration(&mut self, ast: &mut ImportDeclaration<'alloc>) {
        self.enter_import_declaration(ast);
        match ast {
            ImportDeclaration::Import(ast) => {
                self.visit_enum_import_declaration_variant_import(ast)
            }
            ImportDeclaration::ImportNamespace(ast) => {
                self.visit_enum_import_declaration_variant_import_namespace(ast)
            }
        }
        self.leave_import_declaration(ast);
    }

    fn enter_import_declaration(&mut self, ast: &mut ImportDeclaration<'alloc>) {
    }

    fn leave_import_declaration(&mut self, ast: &mut ImportDeclaration<'alloc>) {
    }

    fn visit_enum_import_declaration_variant_import(
        &mut self,
        ast: &mut Import<'alloc>,
    ) {
        self.enter_enum_import_declaration_variant_import(ast);
        self.visit_import(ast);
        self.leave_enum_import_declaration_variant_import(ast);
    }

    fn enter_enum_import_declaration_variant_import(
        &mut self,
        ast: &mut Import<'alloc>,
    ) {
    }

    fn leave_enum_import_declaration_variant_import(
        &mut self,
        ast: &mut Import<'alloc>,
    ) {
    }

    fn visit_enum_import_declaration_variant_import_namespace(
        &mut self,
        ast: &mut ImportNamespace<'alloc>,
    ) {
        self.enter_enum_import_declaration_variant_import_namespace(ast);
        self.visit_import_namespace(ast);
        self.leave_enum_import_declaration_variant_import_namespace(ast);
    }

    fn enter_enum_import_declaration_variant_import_namespace(
        &mut self,
        ast: &mut ImportNamespace<'alloc>,
    ) {
    }

    fn leave_enum_import_declaration_variant_import_namespace(
        &mut self,
        ast: &mut ImportNamespace<'alloc>,
    ) {
    }

    fn visit_export_declaration(&mut self, ast: &mut ExportDeclaration<'alloc>) {
        self.enter_export_declaration(ast);
        match ast {
            ExportDeclaration::ExportAllFrom(ast) => {
                self.visit_enum_export_declaration_variant_export_all_from(ast)
            }
            ExportDeclaration::ExportFrom(ast) => {
                self.visit_enum_export_declaration_variant_export_from(ast)
            }
            ExportDeclaration::ExportLocals(ast) => {
                self.visit_enum_export_declaration_variant_export_locals(ast)
            }
            ExportDeclaration::Export(ast) => {
                self.visit_enum_export_declaration_variant_export(ast)
            }
            ExportDeclaration::ExportDefault(ast) => {
                self.visit_enum_export_declaration_variant_export_default(ast)
            }
        }
        self.leave_export_declaration(ast);
    }

    fn enter_export_declaration(&mut self, ast: &mut ExportDeclaration<'alloc>) {
    }

    fn leave_export_declaration(&mut self, ast: &mut ExportDeclaration<'alloc>) {
    }

    fn visit_enum_export_declaration_variant_export_all_from(
        &mut self,
        ast: &mut ExportAllFrom<'alloc>,
    ) {
        self.enter_enum_export_declaration_variant_export_all_from(ast);
        self.visit_export_all_from(ast);
        self.leave_enum_export_declaration_variant_export_all_from(ast);
    }

    fn enter_enum_export_declaration_variant_export_all_from(
        &mut self,
        ast: &mut ExportAllFrom<'alloc>,
    ) {
    }

    fn leave_enum_export_declaration_variant_export_all_from(
        &mut self,
        ast: &mut ExportAllFrom<'alloc>,
    ) {
    }

    fn visit_enum_export_declaration_variant_export_from(
        &mut self,
        ast: &mut ExportFrom<'alloc>,
    ) {
        self.enter_enum_export_declaration_variant_export_from(ast);
        self.visit_export_from(ast);
        self.leave_enum_export_declaration_variant_export_from(ast);
    }

    fn enter_enum_export_declaration_variant_export_from(
        &mut self,
        ast: &mut ExportFrom<'alloc>,
    ) {
    }

    fn leave_enum_export_declaration_variant_export_from(
        &mut self,
        ast: &mut ExportFrom<'alloc>,
    ) {
    }

    fn visit_enum_export_declaration_variant_export_locals(
        &mut self,
        ast: &mut ExportLocals<'alloc>,
    ) {
        self.enter_enum_export_declaration_variant_export_locals(ast);
        self.visit_export_locals(ast);
        self.leave_enum_export_declaration_variant_export_locals(ast);
    }

    fn enter_enum_export_declaration_variant_export_locals(
        &mut self,
        ast: &mut ExportLocals<'alloc>,
    ) {
    }

    fn leave_enum_export_declaration_variant_export_locals(
        &mut self,
        ast: &mut ExportLocals<'alloc>,
    ) {
    }

    fn visit_enum_export_declaration_variant_export(
        &mut self,
        ast: &mut Export<'alloc>,
    ) {
        self.enter_enum_export_declaration_variant_export(ast);
        self.visit_export(ast);
        self.leave_enum_export_declaration_variant_export(ast);
    }

    fn enter_enum_export_declaration_variant_export(
        &mut self,
        ast: &mut Export<'alloc>,
    ) {
    }

    fn leave_enum_export_declaration_variant_export(
        &mut self,
        ast: &mut Export<'alloc>,
    ) {
    }

    fn visit_enum_export_declaration_variant_export_default(
        &mut self,
        ast: &mut ExportDefault<'alloc>,
    ) {
        self.enter_enum_export_declaration_variant_export_default(ast);
        self.visit_export_default(ast);
        self.leave_enum_export_declaration_variant_export_default(ast);
    }

    fn enter_enum_export_declaration_variant_export_default(
        &mut self,
        ast: &mut ExportDefault<'alloc>,
    ) {
    }

    fn leave_enum_export_declaration_variant_export_default(
        &mut self,
        ast: &mut ExportDefault<'alloc>,
    ) {
    }

    fn visit_variable_reference(&mut self, ast: &mut VariableReference<'alloc>) {
        self.enter_variable_reference(ast);
        match ast {
            VariableReference::BindingIdentifier(ast) => {
                self.visit_enum_variable_reference_variant_binding_identifier(ast)
            }
            VariableReference::AssignmentTargetIdentifier(ast) => {
                self.visit_enum_variable_reference_variant_assignment_target_identifier(ast)
            }
        }
        self.leave_variable_reference(ast);
    }

    fn enter_variable_reference(&mut self, ast: &mut VariableReference<'alloc>) {
    }

    fn leave_variable_reference(&mut self, ast: &mut VariableReference<'alloc>) {
    }

    fn visit_enum_variable_reference_variant_binding_identifier(
        &mut self,
        ast: &mut BindingIdentifier<'alloc>,
    ) {
        self.enter_enum_variable_reference_variant_binding_identifier(ast);
        self.visit_binding_identifier(ast);
        self.leave_enum_variable_reference_variant_binding_identifier(ast);
    }

    fn enter_enum_variable_reference_variant_binding_identifier(
        &mut self,
        ast: &mut BindingIdentifier<'alloc>,
    ) {
    }

    fn leave_enum_variable_reference_variant_binding_identifier(
        &mut self,
        ast: &mut BindingIdentifier<'alloc>,
    ) {
    }

    fn visit_enum_variable_reference_variant_assignment_target_identifier(
        &mut self,
        ast: &mut AssignmentTargetIdentifier<'alloc>,
    ) {
        self.enter_enum_variable_reference_variant_assignment_target_identifier(ast);
        self.visit_assignment_target_identifier(ast);
        self.leave_enum_variable_reference_variant_assignment_target_identifier(ast);
    }

    fn enter_enum_variable_reference_variant_assignment_target_identifier(
        &mut self,
        ast: &mut AssignmentTargetIdentifier<'alloc>,
    ) {
    }

    fn leave_enum_variable_reference_variant_assignment_target_identifier(
        &mut self,
        ast: &mut AssignmentTargetIdentifier<'alloc>,
    ) {
    }

    fn visit_binding_pattern(&mut self, ast: &mut BindingPattern<'alloc>) {
        self.enter_binding_pattern(ast);
        match ast {
            BindingPattern::ObjectBinding(ast) => {
                self.visit_enum_binding_pattern_variant_object_binding(ast)
            }
            BindingPattern::ArrayBinding(ast) => {
                self.visit_enum_binding_pattern_variant_array_binding(ast)
            }
        }
        self.leave_binding_pattern(ast);
    }

    fn enter_binding_pattern(&mut self, ast: &mut BindingPattern<'alloc>) {
    }

    fn leave_binding_pattern(&mut self, ast: &mut BindingPattern<'alloc>) {
    }

    fn visit_enum_binding_pattern_variant_object_binding(
        &mut self,
        ast: &mut ObjectBinding<'alloc>,
    ) {
        self.enter_enum_binding_pattern_variant_object_binding(ast);
        self.visit_object_binding(ast);
        self.leave_enum_binding_pattern_variant_object_binding(ast);
    }

    fn enter_enum_binding_pattern_variant_object_binding(
        &mut self,
        ast: &mut ObjectBinding<'alloc>,
    ) {
    }

    fn leave_enum_binding_pattern_variant_object_binding(
        &mut self,
        ast: &mut ObjectBinding<'alloc>,
    ) {
    }

    fn visit_enum_binding_pattern_variant_array_binding(
        &mut self,
        ast: &mut ArrayBinding<'alloc>,
    ) {
        self.enter_enum_binding_pattern_variant_array_binding(ast);
        self.visit_array_binding(ast);
        self.leave_enum_binding_pattern_variant_array_binding(ast);
    }

    fn enter_enum_binding_pattern_variant_array_binding(
        &mut self,
        ast: &mut ArrayBinding<'alloc>,
    ) {
    }

    fn leave_enum_binding_pattern_variant_array_binding(
        &mut self,
        ast: &mut ArrayBinding<'alloc>,
    ) {
    }

    fn visit_binding(&mut self, ast: &mut Binding<'alloc>) {
        self.enter_binding(ast);
        match ast {
            Binding::BindingPattern(ast) => {
                self.visit_enum_binding_variant_binding_pattern(ast)
            }
            Binding::BindingIdentifier(ast) => {
                self.visit_enum_binding_variant_binding_identifier(ast)
            }
        }
        self.leave_binding(ast);
    }

    fn enter_binding(&mut self, ast: &mut Binding<'alloc>) {
    }

    fn leave_binding(&mut self, ast: &mut Binding<'alloc>) {
    }

    fn visit_enum_binding_variant_binding_pattern(
        &mut self,
        ast: &mut BindingPattern<'alloc>,
    ) {
        self.enter_enum_binding_variant_binding_pattern(ast);
        self.visit_binding_pattern(ast);
        self.leave_enum_binding_variant_binding_pattern(ast);
    }

    fn enter_enum_binding_variant_binding_pattern(
        &mut self,
        ast: &mut BindingPattern<'alloc>,
    ) {
    }

    fn leave_enum_binding_variant_binding_pattern(
        &mut self,
        ast: &mut BindingPattern<'alloc>,
    ) {
    }

    fn visit_enum_binding_variant_binding_identifier(
        &mut self,
        ast: &mut BindingIdentifier<'alloc>,
    ) {
        self.enter_enum_binding_variant_binding_identifier(ast);
        self.visit_binding_identifier(ast);
        self.leave_enum_binding_variant_binding_identifier(ast);
    }

    fn enter_enum_binding_variant_binding_identifier(
        &mut self,
        ast: &mut BindingIdentifier<'alloc>,
    ) {
    }

    fn leave_enum_binding_variant_binding_identifier(
        &mut self,
        ast: &mut BindingIdentifier<'alloc>,
    ) {
    }

    fn visit_simple_assignment_target(&mut self, ast: &mut SimpleAssignmentTarget<'alloc>) {
        self.enter_simple_assignment_target(ast);
        match ast {
            SimpleAssignmentTarget::AssignmentTargetIdentifier(ast) => {
                self.visit_enum_simple_assignment_target_variant_assignment_target_identifier(ast)
            }
            SimpleAssignmentTarget::MemberAssignmentTarget(ast) => {
                self.visit_enum_simple_assignment_target_variant_member_assignment_target(ast)
            }
        }
        self.leave_simple_assignment_target(ast);
    }

    fn enter_simple_assignment_target(&mut self, ast: &mut SimpleAssignmentTarget<'alloc>) {
    }

    fn leave_simple_assignment_target(&mut self, ast: &mut SimpleAssignmentTarget<'alloc>) {
    }

    fn visit_enum_simple_assignment_target_variant_assignment_target_identifier(
        &mut self,
        ast: &mut AssignmentTargetIdentifier<'alloc>,
    ) {
        self.enter_enum_simple_assignment_target_variant_assignment_target_identifier(ast);
        self.visit_assignment_target_identifier(ast);
        self.leave_enum_simple_assignment_target_variant_assignment_target_identifier(ast);
    }

    fn enter_enum_simple_assignment_target_variant_assignment_target_identifier(
        &mut self,
        ast: &mut AssignmentTargetIdentifier<'alloc>,
    ) {
    }

    fn leave_enum_simple_assignment_target_variant_assignment_target_identifier(
        &mut self,
        ast: &mut AssignmentTargetIdentifier<'alloc>,
    ) {
    }

    fn visit_enum_simple_assignment_target_variant_member_assignment_target(
        &mut self,
        ast: &mut MemberAssignmentTarget<'alloc>,
    ) {
        self.enter_enum_simple_assignment_target_variant_member_assignment_target(ast);
        self.visit_member_assignment_target(ast);
        self.leave_enum_simple_assignment_target_variant_member_assignment_target(ast);
    }

    fn enter_enum_simple_assignment_target_variant_member_assignment_target(
        &mut self,
        ast: &mut MemberAssignmentTarget<'alloc>,
    ) {
    }

    fn leave_enum_simple_assignment_target_variant_member_assignment_target(
        &mut self,
        ast: &mut MemberAssignmentTarget<'alloc>,
    ) {
    }

    fn visit_assignment_target_pattern(&mut self, ast: &mut AssignmentTargetPattern<'alloc>) {
        self.enter_assignment_target_pattern(ast);
        match ast {
            AssignmentTargetPattern::ArrayAssignmentTarget(ast) => {
                self.visit_enum_assignment_target_pattern_variant_array_assignment_target(ast)
            }
            AssignmentTargetPattern::ObjectAssignmentTarget(ast) => {
                self.visit_enum_assignment_target_pattern_variant_object_assignment_target(ast)
            }
        }
        self.leave_assignment_target_pattern(ast);
    }

    fn enter_assignment_target_pattern(&mut self, ast: &mut AssignmentTargetPattern<'alloc>) {
    }

    fn leave_assignment_target_pattern(&mut self, ast: &mut AssignmentTargetPattern<'alloc>) {
    }

    fn visit_enum_assignment_target_pattern_variant_array_assignment_target(
        &mut self,
        ast: &mut ArrayAssignmentTarget<'alloc>,
    ) {
        self.enter_enum_assignment_target_pattern_variant_array_assignment_target(ast);
        self.visit_array_assignment_target(ast);
        self.leave_enum_assignment_target_pattern_variant_array_assignment_target(ast);
    }

    fn enter_enum_assignment_target_pattern_variant_array_assignment_target(
        &mut self,
        ast: &mut ArrayAssignmentTarget<'alloc>,
    ) {
    }

    fn leave_enum_assignment_target_pattern_variant_array_assignment_target(
        &mut self,
        ast: &mut ArrayAssignmentTarget<'alloc>,
    ) {
    }

    fn visit_enum_assignment_target_pattern_variant_object_assignment_target(
        &mut self,
        ast: &mut ObjectAssignmentTarget<'alloc>,
    ) {
        self.enter_enum_assignment_target_pattern_variant_object_assignment_target(ast);
        self.visit_object_assignment_target(ast);
        self.leave_enum_assignment_target_pattern_variant_object_assignment_target(ast);
    }

    fn enter_enum_assignment_target_pattern_variant_object_assignment_target(
        &mut self,
        ast: &mut ObjectAssignmentTarget<'alloc>,
    ) {
    }

    fn leave_enum_assignment_target_pattern_variant_object_assignment_target(
        &mut self,
        ast: &mut ObjectAssignmentTarget<'alloc>,
    ) {
    }

    fn visit_assignment_target(&mut self, ast: &mut AssignmentTarget<'alloc>) {
        self.enter_assignment_target(ast);
        match ast {
            AssignmentTarget::AssignmentTargetPattern(ast) => {
                self.visit_enum_assignment_target_variant_assignment_target_pattern(ast)
            }
            AssignmentTarget::SimpleAssignmentTarget(ast) => {
                self.visit_enum_assignment_target_variant_simple_assignment_target(ast)
            }
        }
        self.leave_assignment_target(ast);
    }

    fn enter_assignment_target(&mut self, ast: &mut AssignmentTarget<'alloc>) {
    }

    fn leave_assignment_target(&mut self, ast: &mut AssignmentTarget<'alloc>) {
    }

    fn visit_enum_assignment_target_variant_assignment_target_pattern(
        &mut self,
        ast: &mut AssignmentTargetPattern<'alloc>,
    ) {
        self.enter_enum_assignment_target_variant_assignment_target_pattern(ast);
        self.visit_assignment_target_pattern(ast);
        self.leave_enum_assignment_target_variant_assignment_target_pattern(ast);
    }

    fn enter_enum_assignment_target_variant_assignment_target_pattern(
        &mut self,
        ast: &mut AssignmentTargetPattern<'alloc>,
    ) {
    }

    fn leave_enum_assignment_target_variant_assignment_target_pattern(
        &mut self,
        ast: &mut AssignmentTargetPattern<'alloc>,
    ) {
    }

    fn visit_enum_assignment_target_variant_simple_assignment_target(
        &mut self,
        ast: &mut SimpleAssignmentTarget<'alloc>,
    ) {
        self.enter_enum_assignment_target_variant_simple_assignment_target(ast);
        self.visit_simple_assignment_target(ast);
        self.leave_enum_assignment_target_variant_simple_assignment_target(ast);
    }

    fn enter_enum_assignment_target_variant_simple_assignment_target(
        &mut self,
        ast: &mut SimpleAssignmentTarget<'alloc>,
    ) {
    }

    fn leave_enum_assignment_target_variant_simple_assignment_target(
        &mut self,
        ast: &mut SimpleAssignmentTarget<'alloc>,
    ) {
    }

    fn visit_parameter(&mut self, ast: &mut Parameter<'alloc>) {
        self.enter_parameter(ast);
        match ast {
            Parameter::Binding(ast) => {
                self.visit_enum_parameter_variant_binding(ast)
            }
            Parameter::BindingWithDefault(ast) => {
                self.visit_enum_parameter_variant_binding_with_default(ast)
            }
        }
        self.leave_parameter(ast);
    }

    fn enter_parameter(&mut self, ast: &mut Parameter<'alloc>) {
    }

    fn leave_parameter(&mut self, ast: &mut Parameter<'alloc>) {
    }

    fn visit_enum_parameter_variant_binding(
        &mut self,
        ast: &mut Binding<'alloc>,
    ) {
        self.enter_enum_parameter_variant_binding(ast);
        self.visit_binding(ast);
        self.leave_enum_parameter_variant_binding(ast);
    }

    fn enter_enum_parameter_variant_binding(
        &mut self,
        ast: &mut Binding<'alloc>,
    ) {
    }

    fn leave_enum_parameter_variant_binding(
        &mut self,
        ast: &mut Binding<'alloc>,
    ) {
    }

    fn visit_enum_parameter_variant_binding_with_default(
        &mut self,
        ast: &mut BindingWithDefault<'alloc>,
    ) {
        self.enter_enum_parameter_variant_binding_with_default(ast);
        self.visit_binding_with_default(ast);
        self.leave_enum_parameter_variant_binding_with_default(ast);
    }

    fn enter_enum_parameter_variant_binding_with_default(
        &mut self,
        ast: &mut BindingWithDefault<'alloc>,
    ) {
    }

    fn leave_enum_parameter_variant_binding_with_default(
        &mut self,
        ast: &mut BindingWithDefault<'alloc>,
    ) {
    }

    fn visit_binding_with_default(&mut self, ast: &mut BindingWithDefault<'alloc>) {
        self.enter_binding_with_default(ast);
        self.visit_binding(&mut ast.binding);
        self.visit_expression(&mut ast.init);
        self.leave_binding_with_default(ast);
    }

    fn enter_binding_with_default(&mut self, ast: &mut BindingWithDefault<'alloc>) {
    }

    fn leave_binding_with_default(&mut self, ast: &mut BindingWithDefault<'alloc>) {
    }

    fn visit_binding_identifier(&mut self, ast: &mut BindingIdentifier<'alloc>) {
        self.enter_binding_identifier(ast);
        self.visit_identifier(&mut ast.name);
        self.leave_binding_identifier(ast);
    }

    fn enter_binding_identifier(&mut self, ast: &mut BindingIdentifier<'alloc>) {
    }

    fn leave_binding_identifier(&mut self, ast: &mut BindingIdentifier<'alloc>) {
    }

    fn visit_assignment_target_identifier(&mut self, ast: &mut AssignmentTargetIdentifier<'alloc>) {
        self.enter_assignment_target_identifier(ast);
        self.visit_identifier(&mut ast.name);
        self.leave_assignment_target_identifier(ast);
    }

    fn enter_assignment_target_identifier(&mut self, ast: &mut AssignmentTargetIdentifier<'alloc>) {
    }

    fn leave_assignment_target_identifier(&mut self, ast: &mut AssignmentTargetIdentifier<'alloc>) {
    }

    fn visit_expression_or_super(&mut self, ast: &mut ExpressionOrSuper<'alloc>) {
        self.enter_expression_or_super(ast);
        match ast {
            ExpressionOrSuper::Expression(ast) => {
                self.visit_enum_expression_or_super_variant_expression(ast)
            }
            ExpressionOrSuper::Super { .. } => {
                self.visit_enum_expression_or_super_variant_super()
            }
        }
        self.leave_expression_or_super(ast);
    }

    fn enter_expression_or_super(&mut self, ast: &mut ExpressionOrSuper<'alloc>) {
    }

    fn leave_expression_or_super(&mut self, ast: &mut ExpressionOrSuper<'alloc>) {
    }

    fn visit_enum_expression_or_super_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_expression_or_super_variant_expression(ast);
        self.visit_expression(ast);
        self.leave_enum_expression_or_super_variant_expression(ast);
    }

    fn enter_enum_expression_or_super_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_expression_or_super_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_expression_or_super_variant_super(&mut self) {
    }

    fn visit_member_assignment_target(&mut self, ast: &mut MemberAssignmentTarget<'alloc>) {
        self.enter_member_assignment_target(ast);
        match ast {
            MemberAssignmentTarget::ComputedMemberAssignmentTarget(ast) => {
                self.visit_enum_member_assignment_target_variant_computed_member_assignment_target(ast)
            }
            MemberAssignmentTarget::StaticMemberAssignmentTarget(ast) => {
                self.visit_enum_member_assignment_target_variant_static_member_assignment_target(ast)
            }
        }
        self.leave_member_assignment_target(ast);
    }

    fn enter_member_assignment_target(&mut self, ast: &mut MemberAssignmentTarget<'alloc>) {
    }

    fn leave_member_assignment_target(&mut self, ast: &mut MemberAssignmentTarget<'alloc>) {
    }

    fn visit_enum_member_assignment_target_variant_computed_member_assignment_target(
        &mut self,
        ast: &mut ComputedMemberAssignmentTarget<'alloc>,
    ) {
        self.enter_enum_member_assignment_target_variant_computed_member_assignment_target(ast);
        self.visit_computed_member_assignment_target(ast);
        self.leave_enum_member_assignment_target_variant_computed_member_assignment_target(ast);
    }

    fn enter_enum_member_assignment_target_variant_computed_member_assignment_target(
        &mut self,
        ast: &mut ComputedMemberAssignmentTarget<'alloc>,
    ) {
    }

    fn leave_enum_member_assignment_target_variant_computed_member_assignment_target(
        &mut self,
        ast: &mut ComputedMemberAssignmentTarget<'alloc>,
    ) {
    }

    fn visit_enum_member_assignment_target_variant_static_member_assignment_target(
        &mut self,
        ast: &mut StaticMemberAssignmentTarget<'alloc>,
    ) {
        self.enter_enum_member_assignment_target_variant_static_member_assignment_target(ast);
        self.visit_static_member_assignment_target(ast);
        self.leave_enum_member_assignment_target_variant_static_member_assignment_target(ast);
    }

    fn enter_enum_member_assignment_target_variant_static_member_assignment_target(
        &mut self,
        ast: &mut StaticMemberAssignmentTarget<'alloc>,
    ) {
    }

    fn leave_enum_member_assignment_target_variant_static_member_assignment_target(
        &mut self,
        ast: &mut StaticMemberAssignmentTarget<'alloc>,
    ) {
    }

    fn visit_computed_member_assignment_target(&mut self, ast: &mut ComputedMemberAssignmentTarget<'alloc>) {
        self.enter_computed_member_assignment_target(ast);
        self.visit_expression_or_super(&mut ast.object);
        self.visit_expression(&mut ast.expression);
        self.leave_computed_member_assignment_target(ast);
    }

    fn enter_computed_member_assignment_target(&mut self, ast: &mut ComputedMemberAssignmentTarget<'alloc>) {
    }

    fn leave_computed_member_assignment_target(&mut self, ast: &mut ComputedMemberAssignmentTarget<'alloc>) {
    }

    fn visit_static_member_assignment_target(&mut self, ast: &mut StaticMemberAssignmentTarget<'alloc>) {
        self.enter_static_member_assignment_target(ast);
        self.visit_expression_or_super(&mut ast.object);
        self.visit_identifier_name(&mut ast.property);
        self.leave_static_member_assignment_target(ast);
    }

    fn enter_static_member_assignment_target(&mut self, ast: &mut StaticMemberAssignmentTarget<'alloc>) {
    }

    fn leave_static_member_assignment_target(&mut self, ast: &mut StaticMemberAssignmentTarget<'alloc>) {
    }

    fn visit_array_binding(&mut self, ast: &mut ArrayBinding<'alloc>) {
        self.enter_array_binding(ast);
        for item in &mut ast.elements {
            if let Some(item) = item {
                self.visit_parameter(item);
            }
        }
        if let Some(item) = &mut ast.rest {
            self.visit_binding(item);
        }
        self.leave_array_binding(ast);
    }

    fn enter_array_binding(&mut self, ast: &mut ArrayBinding<'alloc>) {
    }

    fn leave_array_binding(&mut self, ast: &mut ArrayBinding<'alloc>) {
    }

    fn visit_object_binding(&mut self, ast: &mut ObjectBinding<'alloc>) {
        self.enter_object_binding(ast);
        for item in &mut ast.properties {
            self.visit_binding_property(item);
        }
        if let Some(item) = &mut ast.rest {
            self.visit_binding_identifier(item);
        }
        self.leave_object_binding(ast);
    }

    fn enter_object_binding(&mut self, ast: &mut ObjectBinding<'alloc>) {
    }

    fn leave_object_binding(&mut self, ast: &mut ObjectBinding<'alloc>) {
    }

    fn visit_binding_property(&mut self, ast: &mut BindingProperty<'alloc>) {
        self.enter_binding_property(ast);
        match ast {
            BindingProperty::BindingPropertyIdentifier(ast) => {
                self.visit_enum_binding_property_variant_binding_property_identifier(ast)
            }
            BindingProperty::BindingPropertyProperty(ast) => {
                self.visit_enum_binding_property_variant_binding_property_property(ast)
            }
        }
        self.leave_binding_property(ast);
    }

    fn enter_binding_property(&mut self, ast: &mut BindingProperty<'alloc>) {
    }

    fn leave_binding_property(&mut self, ast: &mut BindingProperty<'alloc>) {
    }

    fn visit_enum_binding_property_variant_binding_property_identifier(
        &mut self,
        ast: &mut BindingPropertyIdentifier<'alloc>,
    ) {
        self.enter_enum_binding_property_variant_binding_property_identifier(ast);
        self.visit_binding_property_identifier(ast);
        self.leave_enum_binding_property_variant_binding_property_identifier(ast);
    }

    fn enter_enum_binding_property_variant_binding_property_identifier(
        &mut self,
        ast: &mut BindingPropertyIdentifier<'alloc>,
    ) {
    }

    fn leave_enum_binding_property_variant_binding_property_identifier(
        &mut self,
        ast: &mut BindingPropertyIdentifier<'alloc>,
    ) {
    }

    fn visit_enum_binding_property_variant_binding_property_property(
        &mut self,
        ast: &mut BindingPropertyProperty<'alloc>,
    ) {
        self.enter_enum_binding_property_variant_binding_property_property(ast);
        self.visit_binding_property_property(ast);
        self.leave_enum_binding_property_variant_binding_property_property(ast);
    }

    fn enter_enum_binding_property_variant_binding_property_property(
        &mut self,
        ast: &mut BindingPropertyProperty<'alloc>,
    ) {
    }

    fn leave_enum_binding_property_variant_binding_property_property(
        &mut self,
        ast: &mut BindingPropertyProperty<'alloc>,
    ) {
    }

    fn visit_binding_property_identifier(&mut self, ast: &mut BindingPropertyIdentifier<'alloc>) {
        self.enter_binding_property_identifier(ast);
        self.visit_binding_identifier(&mut ast.binding);
        if let Some(item) = &mut ast.init {
            self.visit_expression(item);
        }
        self.leave_binding_property_identifier(ast);
    }

    fn enter_binding_property_identifier(&mut self, ast: &mut BindingPropertyIdentifier<'alloc>) {
    }

    fn leave_binding_property_identifier(&mut self, ast: &mut BindingPropertyIdentifier<'alloc>) {
    }

    fn visit_binding_property_property(&mut self, ast: &mut BindingPropertyProperty<'alloc>) {
        self.enter_binding_property_property(ast);
        self.visit_property_name(&mut ast.name);
        self.visit_parameter(&mut ast.binding);
        self.leave_binding_property_property(ast);
    }

    fn enter_binding_property_property(&mut self, ast: &mut BindingPropertyProperty<'alloc>) {
    }

    fn leave_binding_property_property(&mut self, ast: &mut BindingPropertyProperty<'alloc>) {
    }

    fn visit_assignment_target_with_default(&mut self, ast: &mut AssignmentTargetWithDefault<'alloc>) {
        self.enter_assignment_target_with_default(ast);
        self.visit_assignment_target(&mut ast.binding);
        self.visit_expression(&mut ast.init);
        self.leave_assignment_target_with_default(ast);
    }

    fn enter_assignment_target_with_default(&mut self, ast: &mut AssignmentTargetWithDefault<'alloc>) {
    }

    fn leave_assignment_target_with_default(&mut self, ast: &mut AssignmentTargetWithDefault<'alloc>) {
    }

    fn visit_assignment_target_maybe_default(&mut self, ast: &mut AssignmentTargetMaybeDefault<'alloc>) {
        self.enter_assignment_target_maybe_default(ast);
        match ast {
            AssignmentTargetMaybeDefault::AssignmentTarget(ast) => {
                self.visit_enum_assignment_target_maybe_default_variant_assignment_target(ast)
            }
            AssignmentTargetMaybeDefault::AssignmentTargetWithDefault(ast) => {
                self.visit_enum_assignment_target_maybe_default_variant_assignment_target_with_default(ast)
            }
        }
        self.leave_assignment_target_maybe_default(ast);
    }

    fn enter_assignment_target_maybe_default(&mut self, ast: &mut AssignmentTargetMaybeDefault<'alloc>) {
    }

    fn leave_assignment_target_maybe_default(&mut self, ast: &mut AssignmentTargetMaybeDefault<'alloc>) {
    }

    fn visit_enum_assignment_target_maybe_default_variant_assignment_target(
        &mut self,
        ast: &mut AssignmentTarget<'alloc>,
    ) {
        self.enter_enum_assignment_target_maybe_default_variant_assignment_target(ast);
        self.visit_assignment_target(ast);
        self.leave_enum_assignment_target_maybe_default_variant_assignment_target(ast);
    }

    fn enter_enum_assignment_target_maybe_default_variant_assignment_target(
        &mut self,
        ast: &mut AssignmentTarget<'alloc>,
    ) {
    }

    fn leave_enum_assignment_target_maybe_default_variant_assignment_target(
        &mut self,
        ast: &mut AssignmentTarget<'alloc>,
    ) {
    }

    fn visit_enum_assignment_target_maybe_default_variant_assignment_target_with_default(
        &mut self,
        ast: &mut AssignmentTargetWithDefault<'alloc>,
    ) {
        self.enter_enum_assignment_target_maybe_default_variant_assignment_target_with_default(ast);
        self.visit_assignment_target_with_default(ast);
        self.leave_enum_assignment_target_maybe_default_variant_assignment_target_with_default(ast);
    }

    fn enter_enum_assignment_target_maybe_default_variant_assignment_target_with_default(
        &mut self,
        ast: &mut AssignmentTargetWithDefault<'alloc>,
    ) {
    }

    fn leave_enum_assignment_target_maybe_default_variant_assignment_target_with_default(
        &mut self,
        ast: &mut AssignmentTargetWithDefault<'alloc>,
    ) {
    }

    fn visit_array_assignment_target(&mut self, ast: &mut ArrayAssignmentTarget<'alloc>) {
        self.enter_array_assignment_target(ast);
        for item in &mut ast.elements {
            if let Some(item) = item {
                self.visit_assignment_target_maybe_default(item);
            }
        }
        if let Some(item) = &mut ast.rest {
            self.visit_assignment_target(item);
        }
        self.leave_array_assignment_target(ast);
    }

    fn enter_array_assignment_target(&mut self, ast: &mut ArrayAssignmentTarget<'alloc>) {
    }

    fn leave_array_assignment_target(&mut self, ast: &mut ArrayAssignmentTarget<'alloc>) {
    }

    fn visit_object_assignment_target(&mut self, ast: &mut ObjectAssignmentTarget<'alloc>) {
        self.enter_object_assignment_target(ast);
        for item in &mut ast.properties {
            self.visit_assignment_target_property(item);
        }
        if let Some(item) = &mut ast.rest {
            self.visit_assignment_target(item);
        }
        self.leave_object_assignment_target(ast);
    }

    fn enter_object_assignment_target(&mut self, ast: &mut ObjectAssignmentTarget<'alloc>) {
    }

    fn leave_object_assignment_target(&mut self, ast: &mut ObjectAssignmentTarget<'alloc>) {
    }

    fn visit_assignment_target_property(&mut self, ast: &mut AssignmentTargetProperty<'alloc>) {
        self.enter_assignment_target_property(ast);
        match ast {
            AssignmentTargetProperty::AssignmentTargetPropertyIdentifier(ast) => {
                self.visit_enum_assignment_target_property_variant_assignment_target_property_identifier(ast)
            }
            AssignmentTargetProperty::AssignmentTargetPropertyProperty(ast) => {
                self.visit_enum_assignment_target_property_variant_assignment_target_property_property(ast)
            }
        }
        self.leave_assignment_target_property(ast);
    }

    fn enter_assignment_target_property(&mut self, ast: &mut AssignmentTargetProperty<'alloc>) {
    }

    fn leave_assignment_target_property(&mut self, ast: &mut AssignmentTargetProperty<'alloc>) {
    }

    fn visit_enum_assignment_target_property_variant_assignment_target_property_identifier(
        &mut self,
        ast: &mut AssignmentTargetPropertyIdentifier<'alloc>,
    ) {
        self.enter_enum_assignment_target_property_variant_assignment_target_property_identifier(ast);
        self.visit_assignment_target_property_identifier(ast);
        self.leave_enum_assignment_target_property_variant_assignment_target_property_identifier(ast);
    }

    fn enter_enum_assignment_target_property_variant_assignment_target_property_identifier(
        &mut self,
        ast: &mut AssignmentTargetPropertyIdentifier<'alloc>,
    ) {
    }

    fn leave_enum_assignment_target_property_variant_assignment_target_property_identifier(
        &mut self,
        ast: &mut AssignmentTargetPropertyIdentifier<'alloc>,
    ) {
    }

    fn visit_enum_assignment_target_property_variant_assignment_target_property_property(
        &mut self,
        ast: &mut AssignmentTargetPropertyProperty<'alloc>,
    ) {
        self.enter_enum_assignment_target_property_variant_assignment_target_property_property(ast);
        self.visit_assignment_target_property_property(ast);
        self.leave_enum_assignment_target_property_variant_assignment_target_property_property(ast);
    }

    fn enter_enum_assignment_target_property_variant_assignment_target_property_property(
        &mut self,
        ast: &mut AssignmentTargetPropertyProperty<'alloc>,
    ) {
    }

    fn leave_enum_assignment_target_property_variant_assignment_target_property_property(
        &mut self,
        ast: &mut AssignmentTargetPropertyProperty<'alloc>,
    ) {
    }

    fn visit_assignment_target_property_identifier(&mut self, ast: &mut AssignmentTargetPropertyIdentifier<'alloc>) {
        self.enter_assignment_target_property_identifier(ast);
        self.visit_assignment_target_identifier(&mut ast.binding);
        if let Some(item) = &mut ast.init {
            self.visit_expression(item);
        }
        self.leave_assignment_target_property_identifier(ast);
    }

    fn enter_assignment_target_property_identifier(&mut self, ast: &mut AssignmentTargetPropertyIdentifier<'alloc>) {
    }

    fn leave_assignment_target_property_identifier(&mut self, ast: &mut AssignmentTargetPropertyIdentifier<'alloc>) {
    }

    fn visit_assignment_target_property_property(&mut self, ast: &mut AssignmentTargetPropertyProperty<'alloc>) {
        self.enter_assignment_target_property_property(ast);
        self.visit_property_name(&mut ast.name);
        self.visit_assignment_target_maybe_default(&mut ast.binding);
        self.leave_assignment_target_property_property(ast);
    }

    fn enter_assignment_target_property_property(&mut self, ast: &mut AssignmentTargetPropertyProperty<'alloc>) {
    }

    fn leave_assignment_target_property_property(&mut self, ast: &mut AssignmentTargetPropertyProperty<'alloc>) {
    }

    fn visit_class_expression(&mut self, ast: &mut ClassExpression<'alloc>) {
        self.enter_class_expression(ast);
        if let Some(item) = &mut ast.name {
            self.visit_binding_identifier(item);
        }
        if let Some(item) = &mut ast.super_ {
            self.visit_expression(item);
        }
        for item in &mut ast.elements {
            self.visit_class_element(item);
        }
        self.leave_class_expression(ast);
    }

    fn enter_class_expression(&mut self, ast: &mut ClassExpression<'alloc>) {
    }

    fn leave_class_expression(&mut self, ast: &mut ClassExpression<'alloc>) {
    }

    fn visit_class_declaration(&mut self, ast: &mut ClassDeclaration<'alloc>) {
        self.enter_class_declaration(ast);
        self.visit_binding_identifier(&mut ast.name);
        if let Some(item) = &mut ast.super_ {
            self.visit_expression(item);
        }
        for item in &mut ast.elements {
            self.visit_class_element(item);
        }
        self.leave_class_declaration(ast);
    }

    fn enter_class_declaration(&mut self, ast: &mut ClassDeclaration<'alloc>) {
    }

    fn leave_class_declaration(&mut self, ast: &mut ClassDeclaration<'alloc>) {
    }

    fn visit_class_element(&mut self, ast: &mut ClassElement<'alloc>) {
        self.enter_class_element(ast);
        match ast {
            ClassElement::MethodDefinition { is_static, method, .. } => {
                self.visit_enum_class_element_variant_method_definition(
                    is_static,
                    method,
                )
            }
            ClassElement::FieldDefinition { name, init, .. } => {
                self.visit_enum_class_element_variant_field_definition(
                    name,
                    init,
                )
            }
        }
        self.leave_class_element(ast);
    }

    fn enter_class_element(&mut self, ast: &mut ClassElement<'alloc>) {
    }

    fn leave_class_element(&mut self, ast: &mut ClassElement<'alloc>) {
    }

    fn visit_enum_class_element_variant_method_definition(
        &mut self,
        is_static: &mut bool,
        method: &mut MethodDefinition<'alloc>,
    ) {
        self.enter_enum_class_element_variant_method_definition(
            is_static,
            method,
        );
        self.visit_method_definition(method);
        self.leave_enum_class_element_variant_method_definition(
            is_static,
            method,
        );
    }

    fn enter_enum_class_element_variant_method_definition(
        &mut self,
        is_static: &mut bool,
        method: &mut MethodDefinition<'alloc>,
    ) {
    }

    fn leave_enum_class_element_variant_method_definition(
        &mut self,
        is_static: &mut bool,
        method: &mut MethodDefinition<'alloc>,
    ) {
    }

    fn visit_enum_class_element_variant_field_definition(
        &mut self,
        name: &mut ClassElementName<'alloc>,
        init: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) {
        self.enter_enum_class_element_variant_field_definition(
            name,
            init,
        );
        self.visit_class_element_name(name);
        if let Some(item) = init {
            self.visit_expression(item);
        }
        self.leave_enum_class_element_variant_field_definition(
            name,
            init,
        );
    }

    fn enter_enum_class_element_variant_field_definition(
        &mut self,
        name: &mut ClassElementName<'alloc>,
        init: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) {
    }

    fn leave_enum_class_element_variant_field_definition(
        &mut self,
        name: &mut ClassElementName<'alloc>,
        init: &mut Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) {
    }

    fn visit_module_items(&mut self, ast: &mut ModuleItems<'alloc>) {
        self.enter_module_items(ast);
        match ast {
            ModuleItems::ImportDeclaration(ast) => {
                self.visit_enum_module_items_variant_import_declaration(ast)
            }
            ModuleItems::ExportDeclaration(ast) => {
                self.visit_enum_module_items_variant_export_declaration(ast)
            }
            ModuleItems::Statement(ast) => {
                self.visit_enum_module_items_variant_statement(ast)
            }
        }
        self.leave_module_items(ast);
    }

    fn enter_module_items(&mut self, ast: &mut ModuleItems<'alloc>) {
    }

    fn leave_module_items(&mut self, ast: &mut ModuleItems<'alloc>) {
    }

    fn visit_enum_module_items_variant_import_declaration(
        &mut self,
        ast: &mut ImportDeclaration<'alloc>,
    ) {
        self.enter_enum_module_items_variant_import_declaration(ast);
        self.visit_import_declaration(ast);
        self.leave_enum_module_items_variant_import_declaration(ast);
    }

    fn enter_enum_module_items_variant_import_declaration(
        &mut self,
        ast: &mut ImportDeclaration<'alloc>,
    ) {
    }

    fn leave_enum_module_items_variant_import_declaration(
        &mut self,
        ast: &mut ImportDeclaration<'alloc>,
    ) {
    }

    fn visit_enum_module_items_variant_export_declaration(
        &mut self,
        ast: &mut ExportDeclaration<'alloc>,
    ) {
        self.enter_enum_module_items_variant_export_declaration(ast);
        self.visit_export_declaration(ast);
        self.leave_enum_module_items_variant_export_declaration(ast);
    }

    fn enter_enum_module_items_variant_export_declaration(
        &mut self,
        ast: &mut ExportDeclaration<'alloc>,
    ) {
    }

    fn leave_enum_module_items_variant_export_declaration(
        &mut self,
        ast: &mut ExportDeclaration<'alloc>,
    ) {
    }

    fn visit_enum_module_items_variant_statement(
        &mut self,
        ast: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
        self.enter_enum_module_items_variant_statement(ast);
        self.visit_statement(ast);
        self.leave_enum_module_items_variant_statement(ast);
    }

    fn enter_enum_module_items_variant_statement(
        &mut self,
        ast: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
    }

    fn leave_enum_module_items_variant_statement(
        &mut self,
        ast: &mut arena::Box<'alloc, Statement<'alloc>>,
    ) {
    }

    fn visit_module(&mut self, ast: &mut Module<'alloc>) {
        self.enter_module(ast);
        for item in &mut ast.directives {
            self.visit_directive(item);
        }
        for item in &mut ast.items {
            self.visit_module_items(item);
        }
        self.leave_module(ast);
    }

    fn enter_module(&mut self, ast: &mut Module<'alloc>) {
    }

    fn leave_module(&mut self, ast: &mut Module<'alloc>) {
    }

    fn visit_import(&mut self, ast: &mut Import<'alloc>) {
        self.enter_import(ast);
        if let Some(item) = &mut ast.default_binding {
            self.visit_binding_identifier(item);
        }
        for item in &mut ast.named_imports {
            self.visit_import_specifier(item);
        }
        self.leave_import(ast);
    }

    fn enter_import(&mut self, ast: &mut Import<'alloc>) {
    }

    fn leave_import(&mut self, ast: &mut Import<'alloc>) {
    }

    fn visit_import_namespace(&mut self, ast: &mut ImportNamespace<'alloc>) {
        self.enter_import_namespace(ast);
        if let Some(item) = &mut ast.default_binding {
            self.visit_binding_identifier(item);
        }
        self.visit_binding_identifier(&mut ast.namespace_binding);
        self.leave_import_namespace(ast);
    }

    fn enter_import_namespace(&mut self, ast: &mut ImportNamespace<'alloc>) {
    }

    fn leave_import_namespace(&mut self, ast: &mut ImportNamespace<'alloc>) {
    }

    fn visit_import_specifier(&mut self, ast: &mut ImportSpecifier<'alloc>) {
        self.enter_import_specifier(ast);
        if let Some(item) = &mut ast.name {
            self.visit_identifier_name(item);
        }
        self.visit_binding_identifier(&mut ast.binding);
        self.leave_import_specifier(ast);
    }

    fn enter_import_specifier(&mut self, ast: &mut ImportSpecifier<'alloc>) {
    }

    fn leave_import_specifier(&mut self, ast: &mut ImportSpecifier<'alloc>) {
    }

    fn visit_export_all_from(&mut self, ast: &mut ExportAllFrom<'alloc>) {
        self.enter_export_all_from(ast);
        self.leave_export_all_from(ast);
    }

    fn enter_export_all_from(&mut self, ast: &mut ExportAllFrom<'alloc>) {
    }

    fn leave_export_all_from(&mut self, ast: &mut ExportAllFrom<'alloc>) {
    }

    fn visit_export_from(&mut self, ast: &mut ExportFrom<'alloc>) {
        self.enter_export_from(ast);
        for item in &mut ast.named_exports {
            self.visit_export_from_specifier(item);
        }
        self.leave_export_from(ast);
    }

    fn enter_export_from(&mut self, ast: &mut ExportFrom<'alloc>) {
    }

    fn leave_export_from(&mut self, ast: &mut ExportFrom<'alloc>) {
    }

    fn visit_export_locals(&mut self, ast: &mut ExportLocals<'alloc>) {
        self.enter_export_locals(ast);
        for item in &mut ast.named_exports {
            self.visit_export_local_specifier(item);
        }
        self.leave_export_locals(ast);
    }

    fn enter_export_locals(&mut self, ast: &mut ExportLocals<'alloc>) {
    }

    fn leave_export_locals(&mut self, ast: &mut ExportLocals<'alloc>) {
    }

    fn visit_export(&mut self, ast: &mut Export<'alloc>) {
        self.enter_export(ast);
        match ast {
            Export::FunctionDeclaration(ast) => {
                self.visit_enum_export_variant_function_declaration(ast)
            }
            Export::ClassDeclaration(ast) => {
                self.visit_enum_export_variant_class_declaration(ast)
            }
            Export::VariableDeclaration(ast) => {
                self.visit_enum_export_variant_variable_declaration(ast)
            }
        }
        self.leave_export(ast);
    }

    fn enter_export(&mut self, ast: &mut Export<'alloc>) {
    }

    fn leave_export(&mut self, ast: &mut Export<'alloc>) {
    }

    fn visit_enum_export_variant_function_declaration(
        &mut self,
        ast: &mut Function<'alloc>,
    ) {
        self.enter_enum_export_variant_function_declaration(ast);
        self.visit_function(ast);
        self.leave_enum_export_variant_function_declaration(ast);
    }

    fn enter_enum_export_variant_function_declaration(
        &mut self,
        ast: &mut Function<'alloc>,
    ) {
    }

    fn leave_enum_export_variant_function_declaration(
        &mut self,
        ast: &mut Function<'alloc>,
    ) {
    }

    fn visit_enum_export_variant_class_declaration(
        &mut self,
        ast: &mut ClassDeclaration<'alloc>,
    ) {
        self.enter_enum_export_variant_class_declaration(ast);
        self.visit_class_declaration(ast);
        self.leave_enum_export_variant_class_declaration(ast);
    }

    fn enter_enum_export_variant_class_declaration(
        &mut self,
        ast: &mut ClassDeclaration<'alloc>,
    ) {
    }

    fn leave_enum_export_variant_class_declaration(
        &mut self,
        ast: &mut ClassDeclaration<'alloc>,
    ) {
    }

    fn visit_enum_export_variant_variable_declaration(
        &mut self,
        ast: &mut VariableDeclaration<'alloc>,
    ) {
        self.enter_enum_export_variant_variable_declaration(ast);
        self.visit_variable_declaration(ast);
        self.leave_enum_export_variant_variable_declaration(ast);
    }

    fn enter_enum_export_variant_variable_declaration(
        &mut self,
        ast: &mut VariableDeclaration<'alloc>,
    ) {
    }

    fn leave_enum_export_variant_variable_declaration(
        &mut self,
        ast: &mut VariableDeclaration<'alloc>,
    ) {
    }

    fn visit_export_default(&mut self, ast: &mut ExportDefault<'alloc>) {
        self.enter_export_default(ast);
        match ast {
            ExportDefault::FunctionDeclaration(ast) => {
                self.visit_enum_export_default_variant_function_declaration(ast)
            }
            ExportDefault::ClassDeclaration(ast) => {
                self.visit_enum_export_default_variant_class_declaration(ast)
            }
            ExportDefault::Expression(ast) => {
                self.visit_enum_export_default_variant_expression(ast)
            }
        }
        self.leave_export_default(ast);
    }

    fn enter_export_default(&mut self, ast: &mut ExportDefault<'alloc>) {
    }

    fn leave_export_default(&mut self, ast: &mut ExportDefault<'alloc>) {
    }

    fn visit_enum_export_default_variant_function_declaration(
        &mut self,
        ast: &mut Function<'alloc>,
    ) {
        self.enter_enum_export_default_variant_function_declaration(ast);
        self.visit_function(ast);
        self.leave_enum_export_default_variant_function_declaration(ast);
    }

    fn enter_enum_export_default_variant_function_declaration(
        &mut self,
        ast: &mut Function<'alloc>,
    ) {
    }

    fn leave_enum_export_default_variant_function_declaration(
        &mut self,
        ast: &mut Function<'alloc>,
    ) {
    }

    fn visit_enum_export_default_variant_class_declaration(
        &mut self,
        ast: &mut ClassDeclaration<'alloc>,
    ) {
        self.enter_enum_export_default_variant_class_declaration(ast);
        self.visit_class_declaration(ast);
        self.leave_enum_export_default_variant_class_declaration(ast);
    }

    fn enter_enum_export_default_variant_class_declaration(
        &mut self,
        ast: &mut ClassDeclaration<'alloc>,
    ) {
    }

    fn leave_enum_export_default_variant_class_declaration(
        &mut self,
        ast: &mut ClassDeclaration<'alloc>,
    ) {
    }

    fn visit_enum_export_default_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_export_default_variant_expression(ast);
        self.visit_expression(ast);
        self.leave_enum_export_default_variant_expression(ast);
    }

    fn enter_enum_export_default_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_export_default_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_export_from_specifier(&mut self, ast: &mut ExportFromSpecifier<'alloc>) {
        self.enter_export_from_specifier(ast);
        self.visit_identifier_name(&mut ast.name);
        if let Some(item) = &mut ast.exported_name {
            self.visit_identifier_name(item);
        }
        self.leave_export_from_specifier(ast);
    }

    fn enter_export_from_specifier(&mut self, ast: &mut ExportFromSpecifier<'alloc>) {
    }

    fn leave_export_from_specifier(&mut self, ast: &mut ExportFromSpecifier<'alloc>) {
    }

    fn visit_export_local_specifier(&mut self, ast: &mut ExportLocalSpecifier<'alloc>) {
        self.enter_export_local_specifier(ast);
        self.visit_identifier_expression(&mut ast.name);
        if let Some(item) = &mut ast.exported_name {
            self.visit_identifier_name(item);
        }
        self.leave_export_local_specifier(ast);
    }

    fn enter_export_local_specifier(&mut self, ast: &mut ExportLocalSpecifier<'alloc>) {
    }

    fn leave_export_local_specifier(&mut self, ast: &mut ExportLocalSpecifier<'alloc>) {
    }

    fn visit_method(&mut self, ast: &mut Method<'alloc>) {
        self.enter_method(ast);
        self.visit_property_name(&mut ast.name);
        self.visit_formal_parameters(&mut ast.params);
        self.visit_function_body(&mut ast.body);
        self.leave_method(ast);
    }

    fn enter_method(&mut self, ast: &mut Method<'alloc>) {
    }

    fn leave_method(&mut self, ast: &mut Method<'alloc>) {
    }

    fn visit_getter(&mut self, ast: &mut Getter<'alloc>) {
        self.enter_getter(ast);
        self.visit_property_name(&mut ast.property_name);
        self.visit_function_body(&mut ast.body);
        self.leave_getter(ast);
    }

    fn enter_getter(&mut self, ast: &mut Getter<'alloc>) {
    }

    fn leave_getter(&mut self, ast: &mut Getter<'alloc>) {
    }

    fn visit_setter(&mut self, ast: &mut Setter<'alloc>) {
        self.enter_setter(ast);
        self.visit_property_name(&mut ast.property_name);
        self.visit_parameter(&mut ast.param);
        self.visit_function_body(&mut ast.body);
        self.leave_setter(ast);
    }

    fn enter_setter(&mut self, ast: &mut Setter<'alloc>) {
    }

    fn leave_setter(&mut self, ast: &mut Setter<'alloc>) {
    }

    fn visit_data_property(&mut self, ast: &mut DataProperty<'alloc>) {
        self.enter_data_property(ast);
        self.visit_property_name(&mut ast.property_name);
        self.visit_expression(&mut ast.expression);
        self.leave_data_property(ast);
    }

    fn enter_data_property(&mut self, ast: &mut DataProperty<'alloc>) {
    }

    fn leave_data_property(&mut self, ast: &mut DataProperty<'alloc>) {
    }

    fn visit_shorthand_property(&mut self, ast: &mut ShorthandProperty<'alloc>) {
        self.enter_shorthand_property(ast);
        self.visit_identifier_expression(&mut ast.name);
        self.leave_shorthand_property(ast);
    }

    fn enter_shorthand_property(&mut self, ast: &mut ShorthandProperty<'alloc>) {
    }

    fn leave_shorthand_property(&mut self, ast: &mut ShorthandProperty<'alloc>) {
    }

    fn visit_computed_property_name(&mut self, ast: &mut ComputedPropertyName<'alloc>) {
        self.enter_computed_property_name(ast);
        self.visit_expression(&mut ast.expression);
        self.leave_computed_property_name(ast);
    }

    fn enter_computed_property_name(&mut self, ast: &mut ComputedPropertyName<'alloc>) {
    }

    fn leave_computed_property_name(&mut self, ast: &mut ComputedPropertyName<'alloc>) {
    }

    fn visit_static_property_name(&mut self, ast: &mut StaticPropertyName<'alloc>) {
        self.enter_static_property_name(ast);
        self.leave_static_property_name(ast);
    }

    fn enter_static_property_name(&mut self, ast: &mut StaticPropertyName<'alloc>) {
    }

    fn leave_static_property_name(&mut self, ast: &mut StaticPropertyName<'alloc>) {
    }

    fn visit_array_expression_element(&mut self, ast: &mut ArrayExpressionElement<'alloc>) {
        self.enter_array_expression_element(ast);
        match ast {
            ArrayExpressionElement::SpreadElement(ast) => {
                self.visit_enum_array_expression_element_variant_spread_element(ast)
            }
            ArrayExpressionElement::Expression(ast) => {
                self.visit_enum_array_expression_element_variant_expression(ast)
            }
            ArrayExpressionElement::Elision { .. } => {
                self.visit_enum_array_expression_element_variant_elision()
            }
        }
        self.leave_array_expression_element(ast);
    }

    fn enter_array_expression_element(&mut self, ast: &mut ArrayExpressionElement<'alloc>) {
    }

    fn leave_array_expression_element(&mut self, ast: &mut ArrayExpressionElement<'alloc>) {
    }

    fn visit_enum_array_expression_element_variant_spread_element(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_array_expression_element_variant_spread_element(ast);
        self.visit_expression(ast);
        self.leave_enum_array_expression_element_variant_spread_element(ast);
    }

    fn enter_enum_array_expression_element_variant_spread_element(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_array_expression_element_variant_spread_element(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_array_expression_element_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_array_expression_element_variant_expression(ast);
        self.visit_expression(ast);
        self.leave_enum_array_expression_element_variant_expression(ast);
    }

    fn enter_enum_array_expression_element_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_array_expression_element_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_array_expression_element_variant_elision(&mut self) {
    }

    fn visit_array_expression(&mut self, ast: &mut ArrayExpression<'alloc>) {
        self.enter_array_expression(ast);
        for item in &mut ast.elements {
            self.visit_array_expression_element(item);
        }
        self.leave_array_expression(ast);
    }

    fn enter_array_expression(&mut self, ast: &mut ArrayExpression<'alloc>) {
    }

    fn leave_array_expression(&mut self, ast: &mut ArrayExpression<'alloc>) {
    }

    fn visit_arrow_expression_body(&mut self, ast: &mut ArrowExpressionBody<'alloc>) {
        self.enter_arrow_expression_body(ast);
        match ast {
            ArrowExpressionBody::FunctionBody(ast) => {
                self.visit_enum_arrow_expression_body_variant_function_body(ast)
            }
            ArrowExpressionBody::Expression(ast) => {
                self.visit_enum_arrow_expression_body_variant_expression(ast)
            }
        }
        self.leave_arrow_expression_body(ast);
    }

    fn enter_arrow_expression_body(&mut self, ast: &mut ArrowExpressionBody<'alloc>) {
    }

    fn leave_arrow_expression_body(&mut self, ast: &mut ArrowExpressionBody<'alloc>) {
    }

    fn visit_enum_arrow_expression_body_variant_function_body(
        &mut self,
        ast: &mut FunctionBody<'alloc>,
    ) {
        self.enter_enum_arrow_expression_body_variant_function_body(ast);
        self.visit_function_body(ast);
        self.leave_enum_arrow_expression_body_variant_function_body(ast);
    }

    fn enter_enum_arrow_expression_body_variant_function_body(
        &mut self,
        ast: &mut FunctionBody<'alloc>,
    ) {
    }

    fn leave_enum_arrow_expression_body_variant_function_body(
        &mut self,
        ast: &mut FunctionBody<'alloc>,
    ) {
    }

    fn visit_enum_arrow_expression_body_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_arrow_expression_body_variant_expression(ast);
        self.visit_expression(ast);
        self.leave_enum_arrow_expression_body_variant_expression(ast);
    }

    fn enter_enum_arrow_expression_body_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_arrow_expression_body_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_computed_member_expression(&mut self, ast: &mut ComputedMemberExpression<'alloc>) {
        self.enter_computed_member_expression(ast);
        self.visit_expression_or_super(&mut ast.object);
        self.visit_expression(&mut ast.expression);
        self.leave_computed_member_expression(ast);
    }

    fn enter_computed_member_expression(&mut self, ast: &mut ComputedMemberExpression<'alloc>) {
    }

    fn leave_computed_member_expression(&mut self, ast: &mut ComputedMemberExpression<'alloc>) {
    }

    fn visit_identifier_expression(&mut self, ast: &mut IdentifierExpression<'alloc>) {
        self.enter_identifier_expression(ast);
        self.visit_identifier(&mut ast.name);
        self.leave_identifier_expression(ast);
    }

    fn enter_identifier_expression(&mut self, ast: &mut IdentifierExpression<'alloc>) {
    }

    fn leave_identifier_expression(&mut self, ast: &mut IdentifierExpression<'alloc>) {
    }

    fn visit_object_expression(&mut self, ast: &mut ObjectExpression<'alloc>) {
        self.enter_object_expression(ast);
        for item in &mut ast.properties {
            self.visit_object_property(item);
        }
        self.leave_object_expression(ast);
    }

    fn enter_object_expression(&mut self, ast: &mut ObjectExpression<'alloc>) {
    }

    fn leave_object_expression(&mut self, ast: &mut ObjectExpression<'alloc>) {
    }

    fn visit_static_member_expression(&mut self, ast: &mut StaticMemberExpression<'alloc>) {
        self.enter_static_member_expression(ast);
        self.visit_expression_or_super(&mut ast.object);
        self.visit_identifier_name(&mut ast.property);
        self.leave_static_member_expression(ast);
    }

    fn enter_static_member_expression(&mut self, ast: &mut StaticMemberExpression<'alloc>) {
    }

    fn leave_static_member_expression(&mut self, ast: &mut StaticMemberExpression<'alloc>) {
    }

    fn visit_private_field_expression(&mut self, ast: &mut PrivateFieldExpression<'alloc>) {
        self.enter_private_field_expression(ast);
        self.visit_expression(&mut ast.object);
        self.visit_private_identifier(&mut ast.field);
        self.leave_private_field_expression(ast);
    }

    fn enter_private_field_expression(&mut self, ast: &mut PrivateFieldExpression<'alloc>) {
    }

    fn leave_private_field_expression(&mut self, ast: &mut PrivateFieldExpression<'alloc>) {
    }

    fn visit_template_expression_element(&mut self, ast: &mut TemplateExpressionElement<'alloc>) {
        self.enter_template_expression_element(ast);
        match ast {
            TemplateExpressionElement::Expression(ast) => {
                self.visit_enum_template_expression_element_variant_expression(ast)
            }
            TemplateExpressionElement::TemplateElement(ast) => {
                self.visit_enum_template_expression_element_variant_template_element(ast)
            }
        }
        self.leave_template_expression_element(ast);
    }

    fn enter_template_expression_element(&mut self, ast: &mut TemplateExpressionElement<'alloc>) {
    }

    fn leave_template_expression_element(&mut self, ast: &mut TemplateExpressionElement<'alloc>) {
    }

    fn visit_enum_template_expression_element_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_template_expression_element_variant_expression(ast);
        self.visit_expression(ast);
        self.leave_enum_template_expression_element_variant_expression(ast);
    }

    fn enter_enum_template_expression_element_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_template_expression_element_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_template_expression_element_variant_template_element(
        &mut self,
        ast: &mut TemplateElement<'alloc>,
    ) {
        self.enter_enum_template_expression_element_variant_template_element(ast);
        self.visit_template_element(ast);
        self.leave_enum_template_expression_element_variant_template_element(ast);
    }

    fn enter_enum_template_expression_element_variant_template_element(
        &mut self,
        ast: &mut TemplateElement<'alloc>,
    ) {
    }

    fn leave_enum_template_expression_element_variant_template_element(
        &mut self,
        ast: &mut TemplateElement<'alloc>,
    ) {
    }

    fn visit_template_expression(&mut self, ast: &mut TemplateExpression<'alloc>) {
        self.enter_template_expression(ast);
        if let Some(item) = &mut ast.tag {
            self.visit_expression(item);
        }
        for item in &mut ast.elements {
            self.visit_template_expression_element(item);
        }
        self.leave_template_expression(ast);
    }

    fn enter_template_expression(&mut self, ast: &mut TemplateExpression<'alloc>) {
    }

    fn leave_template_expression(&mut self, ast: &mut TemplateExpression<'alloc>) {
    }

    fn visit_variable_declaration_or_assignment_target(&mut self, ast: &mut VariableDeclarationOrAssignmentTarget<'alloc>) {
        self.enter_variable_declaration_or_assignment_target(ast);
        match ast {
            VariableDeclarationOrAssignmentTarget::VariableDeclaration(ast) => {
                self.visit_enum_variable_declaration_or_assignment_target_variant_variable_declaration(ast)
            }
            VariableDeclarationOrAssignmentTarget::AssignmentTarget(ast) => {
                self.visit_enum_variable_declaration_or_assignment_target_variant_assignment_target(ast)
            }
        }
        self.leave_variable_declaration_or_assignment_target(ast);
    }

    fn enter_variable_declaration_or_assignment_target(&mut self, ast: &mut VariableDeclarationOrAssignmentTarget<'alloc>) {
    }

    fn leave_variable_declaration_or_assignment_target(&mut self, ast: &mut VariableDeclarationOrAssignmentTarget<'alloc>) {
    }

    fn visit_enum_variable_declaration_or_assignment_target_variant_variable_declaration(
        &mut self,
        ast: &mut VariableDeclaration<'alloc>,
    ) {
        self.enter_enum_variable_declaration_or_assignment_target_variant_variable_declaration(ast);
        self.visit_variable_declaration(ast);
        self.leave_enum_variable_declaration_or_assignment_target_variant_variable_declaration(ast);
    }

    fn enter_enum_variable_declaration_or_assignment_target_variant_variable_declaration(
        &mut self,
        ast: &mut VariableDeclaration<'alloc>,
    ) {
    }

    fn leave_enum_variable_declaration_or_assignment_target_variant_variable_declaration(
        &mut self,
        ast: &mut VariableDeclaration<'alloc>,
    ) {
    }

    fn visit_enum_variable_declaration_or_assignment_target_variant_assignment_target(
        &mut self,
        ast: &mut AssignmentTarget<'alloc>,
    ) {
        self.enter_enum_variable_declaration_or_assignment_target_variant_assignment_target(ast);
        self.visit_assignment_target(ast);
        self.leave_enum_variable_declaration_or_assignment_target_variant_assignment_target(ast);
    }

    fn enter_enum_variable_declaration_or_assignment_target_variant_assignment_target(
        &mut self,
        ast: &mut AssignmentTarget<'alloc>,
    ) {
    }

    fn leave_enum_variable_declaration_or_assignment_target_variant_assignment_target(
        &mut self,
        ast: &mut AssignmentTarget<'alloc>,
    ) {
    }

    fn visit_variable_declaration_or_expression(&mut self, ast: &mut VariableDeclarationOrExpression<'alloc>) {
        self.enter_variable_declaration_or_expression(ast);
        match ast {
            VariableDeclarationOrExpression::VariableDeclaration(ast) => {
                self.visit_enum_variable_declaration_or_expression_variant_variable_declaration(ast)
            }
            VariableDeclarationOrExpression::Expression(ast) => {
                self.visit_enum_variable_declaration_or_expression_variant_expression(ast)
            }
        }
        self.leave_variable_declaration_or_expression(ast);
    }

    fn enter_variable_declaration_or_expression(&mut self, ast: &mut VariableDeclarationOrExpression<'alloc>) {
    }

    fn leave_variable_declaration_or_expression(&mut self, ast: &mut VariableDeclarationOrExpression<'alloc>) {
    }

    fn visit_enum_variable_declaration_or_expression_variant_variable_declaration(
        &mut self,
        ast: &mut VariableDeclaration<'alloc>,
    ) {
        self.enter_enum_variable_declaration_or_expression_variant_variable_declaration(ast);
        self.visit_variable_declaration(ast);
        self.leave_enum_variable_declaration_or_expression_variant_variable_declaration(ast);
    }

    fn enter_enum_variable_declaration_or_expression_variant_variable_declaration(
        &mut self,
        ast: &mut VariableDeclaration<'alloc>,
    ) {
    }

    fn leave_enum_variable_declaration_or_expression_variant_variable_declaration(
        &mut self,
        ast: &mut VariableDeclaration<'alloc>,
    ) {
    }

    fn visit_enum_variable_declaration_or_expression_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_variable_declaration_or_expression_variant_expression(ast);
        self.visit_expression(ast);
        self.leave_enum_variable_declaration_or_expression_variant_expression(ast);
    }

    fn enter_enum_variable_declaration_or_expression_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_variable_declaration_or_expression_variant_expression(
        &mut self,
        ast: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_block(&mut self, ast: &mut Block<'alloc>) {
        self.enter_block(ast);
        for item in &mut ast.statements {
            self.visit_statement(item);
        }
        if let Some(item) = &mut ast.declarations {
            for item in item {
            }
        }
        self.leave_block(ast);
    }

    fn enter_block(&mut self, ast: &mut Block<'alloc>) {
    }

    fn leave_block(&mut self, ast: &mut Block<'alloc>) {
    }

    fn visit_catch_clause(&mut self, ast: &mut CatchClause<'alloc>) {
        self.enter_catch_clause(ast);
        if let Some(item) = &mut ast.binding {
            self.visit_binding(item);
        }
        self.visit_block(&mut ast.body);
        self.leave_catch_clause(ast);
    }

    fn enter_catch_clause(&mut self, ast: &mut CatchClause<'alloc>) {
    }

    fn leave_catch_clause(&mut self, ast: &mut CatchClause<'alloc>) {
    }

    fn visit_directive(&mut self, ast: &mut Directive<'alloc>) {
        self.enter_directive(ast);
        self.leave_directive(ast);
    }

    fn enter_directive(&mut self, ast: &mut Directive<'alloc>) {
    }

    fn leave_directive(&mut self, ast: &mut Directive<'alloc>) {
    }

    fn visit_formal_parameters(&mut self, ast: &mut FormalParameters<'alloc>) {
        self.enter_formal_parameters(ast);
        for item in &mut ast.items {
            self.visit_parameter(item);
        }
        if let Some(item) = &mut ast.rest {
            self.visit_binding(item);
        }
        self.leave_formal_parameters(ast);
    }

    fn enter_formal_parameters(&mut self, ast: &mut FormalParameters<'alloc>) {
    }

    fn leave_formal_parameters(&mut self, ast: &mut FormalParameters<'alloc>) {
    }

    fn visit_function_body(&mut self, ast: &mut FunctionBody<'alloc>) {
        self.enter_function_body(ast);
        for item in &mut ast.directives {
            self.visit_directive(item);
        }
        for item in &mut ast.statements {
            self.visit_statement(item);
        }
        self.leave_function_body(ast);
    }

    fn enter_function_body(&mut self, ast: &mut FunctionBody<'alloc>) {
    }

    fn leave_function_body(&mut self, ast: &mut FunctionBody<'alloc>) {
    }

    fn visit_script(&mut self, ast: &mut Script<'alloc>) {
        self.enter_script(ast);
        for item in &mut ast.directives {
            self.visit_directive(item);
        }
        for item in &mut ast.statements {
            self.visit_statement(item);
        }
        self.leave_script(ast);
    }

    fn enter_script(&mut self, ast: &mut Script<'alloc>) {
    }

    fn leave_script(&mut self, ast: &mut Script<'alloc>) {
    }

    fn visit_switch_case(&mut self, ast: &mut SwitchCase<'alloc>) {
        self.enter_switch_case(ast);
        self.visit_expression(&mut ast.test);
        for item in &mut ast.consequent {
            self.visit_statement(item);
        }
        self.leave_switch_case(ast);
    }

    fn enter_switch_case(&mut self, ast: &mut SwitchCase<'alloc>) {
    }

    fn leave_switch_case(&mut self, ast: &mut SwitchCase<'alloc>) {
    }

    fn visit_switch_default(&mut self, ast: &mut SwitchDefault<'alloc>) {
        self.enter_switch_default(ast);
        for item in &mut ast.consequent {
            self.visit_statement(item);
        }
        self.leave_switch_default(ast);
    }

    fn enter_switch_default(&mut self, ast: &mut SwitchDefault<'alloc>) {
    }

    fn leave_switch_default(&mut self, ast: &mut SwitchDefault<'alloc>) {
    }

    fn visit_template_element(&mut self, ast: &mut TemplateElement<'alloc>) {
        self.enter_template_element(ast);
        self.leave_template_element(ast);
    }

    fn enter_template_element(&mut self, ast: &mut TemplateElement<'alloc>) {
    }

    fn leave_template_element(&mut self, ast: &mut TemplateElement<'alloc>) {
    }

    fn visit_variable_declaration(&mut self, ast: &mut VariableDeclaration<'alloc>) {
        self.enter_variable_declaration(ast);
        self.visit_variable_declaration_kind(&mut ast.kind);
        for item in &mut ast.declarators {
            self.visit_variable_declarator(item);
        }
        self.leave_variable_declaration(ast);
    }

    fn enter_variable_declaration(&mut self, ast: &mut VariableDeclaration<'alloc>) {
    }

    fn leave_variable_declaration(&mut self, ast: &mut VariableDeclaration<'alloc>) {
    }

    fn visit_variable_declarator(&mut self, ast: &mut VariableDeclarator<'alloc>) {
        self.enter_variable_declarator(ast);
        self.visit_binding(&mut ast.binding);
        if let Some(item) = &mut ast.init {
            self.visit_expression(item);
        }
        self.leave_variable_declarator(ast);
    }

    fn enter_variable_declarator(&mut self, ast: &mut VariableDeclarator<'alloc>) {
    }

    fn leave_variable_declarator(&mut self, ast: &mut VariableDeclarator<'alloc>) {
    }

    fn visit_cover_parenthesized(&mut self, ast: &mut CoverParenthesized<'alloc>) {
        self.enter_cover_parenthesized(ast);
        match ast {
            CoverParenthesized::Expression { expression, .. } => {
                self.visit_enum_cover_parenthesized_variant_expression(
                    expression,
                )
            }
            CoverParenthesized::Parameters(ast) => {
                self.visit_enum_cover_parenthesized_variant_parameters(ast)
            }
        }
        self.leave_cover_parenthesized(ast);
    }

    fn enter_cover_parenthesized(&mut self, ast: &mut CoverParenthesized<'alloc>) {
    }

    fn leave_cover_parenthesized(&mut self, ast: &mut CoverParenthesized<'alloc>) {
    }

    fn visit_enum_cover_parenthesized_variant_expression(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
        self.enter_enum_cover_parenthesized_variant_expression(
            expression,
        );
        self.visit_expression(expression);
        self.leave_enum_cover_parenthesized_variant_expression(
            expression,
        );
    }

    fn enter_enum_cover_parenthesized_variant_expression(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn leave_enum_cover_parenthesized_variant_expression(
        &mut self,
        expression: &mut arena::Box<'alloc, Expression<'alloc>>,
    ) {
    }

    fn visit_enum_cover_parenthesized_variant_parameters(
        &mut self,
        ast: &mut arena::Box<'alloc, FormalParameters<'alloc>>,
    ) {
        self.enter_enum_cover_parenthesized_variant_parameters(ast);
        self.visit_formal_parameters(ast);
        self.leave_enum_cover_parenthesized_variant_parameters(ast);
    }

    fn enter_enum_cover_parenthesized_variant_parameters(
        &mut self,
        ast: &mut arena::Box<'alloc, FormalParameters<'alloc>>,
    ) {
    }

    fn leave_enum_cover_parenthesized_variant_parameters(
        &mut self,
        ast: &mut arena::Box<'alloc, FormalParameters<'alloc>>,
    ) {
    }

}

