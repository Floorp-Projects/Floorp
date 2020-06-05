//! High-level bytecode emitter.
//!
//! Converts AST nodes to bytecode.

use crate::array_emitter::*;
use crate::block_emitter::BlockEmitter;
use crate::compilation_info::CompilationInfo;
use crate::emitter::{EmitError, EmitOptions, InstructionWriter};
use crate::emitter_scope::{EmitterScopeStack, NameLocation};
use crate::expression_emitter::*;
use crate::function_declaration_emitter::{FunctionDeclarationEmitter, LazyFunctionEmitter};
use crate::object_emitter::*;
use crate::reference_op_emitter::{
    AssignmentEmitter, CallEmitter, DeclarationEmitter, ElemReferenceEmitter, GetElemEmitter,
    GetNameEmitter, GetPropEmitter, GetSuperElemEmitter, GetSuperPropEmitter, NameReferenceEmitter,
    NewEmitter, PropReferenceEmitter,
};
use crate::script_emitter::ScriptEmitter;
use ast::source_atom_set::{CommonSourceAtomSetIndices, SourceAtomSetIndex};
use ast::types::*;
use std::collections::HashSet;
use stencil::opcode::Opcode;
use stencil::regexp::RegExpItem;
use stencil::result::EmitResult;
use stencil::script::ScriptStencil;

use crate::control_structures::{
    BreakEmitter, CForEmitter, ContinueEmitter, ControlStructureStack, DoWhileEmitter,
    ForwardJumpEmitter, JumpKind, LabelEmitter, WhileEmitter,
};

/// Emit a program, converting the AST directly to bytecode.
pub fn emit_program<'alloc>(
    ast: &Program,
    options: &EmitOptions,
    mut compilation_info: CompilationInfo<'alloc>,
) -> Result<EmitResult<'alloc>, EmitError> {
    let emitter = AstEmitter::new(options, &mut compilation_info);

    let script = match ast {
        Program::Script(script) => emitter.emit_script(script)?,
        _ => {
            return Err(EmitError::NotImplemented("TODO: modules"));
        }
    };

    Ok(EmitResult::new(
        compilation_info.atoms.into(),
        compilation_info.slices.into(),
        compilation_info.scope_data_map.into(),
        script,
        compilation_info.functions.into(),
    ))
}

pub struct AstEmitter<'alloc, 'opt> {
    pub emit: InstructionWriter,
    pub scope_stack: EmitterScopeStack,
    pub options: &'opt EmitOptions,
    pub compilation_info: &'opt mut CompilationInfo<'alloc>,
    pub control_stack: ControlStructureStack,

    /// Holds the offset of top-level function declarations, to check if the
    /// given function function declaration is top-level.
    top_level_function_offsets: HashSet<usize>,
}

impl<'alloc, 'opt> AstEmitter<'alloc, 'opt> {
    fn new(
        options: &'opt EmitOptions,
        compilation_info: &'opt mut CompilationInfo<'alloc>,
    ) -> Self {
        Self {
            emit: InstructionWriter::new(),
            scope_stack: EmitterScopeStack::new(),
            options,
            compilation_info,
            control_stack: ControlStructureStack::new(),
            top_level_function_offsets: HashSet::new(),
        }
    }

    pub fn lookup_name(&mut self, name: SourceAtomSetIndex) -> NameLocation {
        self.scope_stack.lookup_name(name)
    }

    fn emit_script(mut self, ast: &Script) -> Result<ScriptStencil, EmitError> {
        let scope_data_map = &self.compilation_info.scope_data_map;
        let function_map = &self.compilation_info.function_map;

        let scope_index = scope_data_map.get_global_index();
        let scope_data = scope_data_map.get_global_at(scope_index);

        let top_level_functions: Vec<&Function> = scope_data
            .functions
            .iter()
            .map(|key| *function_map.get(key).expect("function should exist"))
            .collect();

        for fun in &top_level_functions {
            self.note_top_level_function(fun);
        }

        ScriptEmitter {
            top_level_functions: top_level_functions.iter(),
            top_level_function: |emitter, fun| emitter.emit_top_level_function_declaration(fun),
            statements: ast.statements.iter(),
            statement: |emitter, statement| emitter.emit_statement(statement),
        }
        .emit(&mut self)?;

        Ok(self.emit.into())
    }

    fn emit_top_level_function_declaration(&mut self, fun: &Function) -> Result<(), EmitError> {
        let stencil_index = *self
            .compilation_info
            .function_stencil_indices
            .get(fun)
            .expect("FunctionStencil should be created");
        let fun_index = LazyFunctionEmitter { stencil_index }.emit(self);

        FunctionDeclarationEmitter { fun: fun_index }.emit(self);

        Err(EmitError::NotImplemented(
            "TODO: closed over bindings for function",
        ))
        //Ok(())
    }

    fn emit_statement(&mut self, ast: &Statement) -> Result<(), EmitError> {
        match ast {
            Statement::ClassDeclaration(_) => {
                return Err(EmitError::NotImplemented("TODO: ClassDeclaration"));
            }
            Statement::BlockStatement { block, .. } => {
                BlockEmitter {
                    scope_index: self.compilation_info.scope_data_map.get_index(block),
                    statements: block.statements.iter(),
                    statement: |emitter, statement| emitter.emit_statement(statement),
                }
                .emit(self)?;
            }
            Statement::BreakStatement { label, .. } => {
                BreakEmitter {
                    label: label.as_ref().map(|x| x.value),
                }
                .emit(self);
            }
            Statement::ContinueStatement { label, .. } => {
                ContinueEmitter {
                    label: label.as_ref().map(|x| x.value),
                }
                .emit(self);
            }
            Statement::DebuggerStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: DebuggerStatement"));
            }
            Statement::DoWhileStatement { block, test, .. } => {
                DoWhileEmitter {
                    enclosing_emitter_scope_depth: self.scope_stack.current_depth(),
                    block: |emitter| emitter.emit_statement(block),
                    test: |emitter| emitter.emit_expression(test),
                }
                .emit(self)?;
            }
            Statement::EmptyStatement { .. } => (),
            Statement::ExpressionStatement(ast) => {
                ExpressionEmitter {
                    expr: |emitter| emitter.emit_expression(ast),
                }
                .emit(self)?;
            }
            Statement::ForInStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: ForInStatement"));
            }
            Statement::ForOfStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: ForOfStatement"));
            }
            Statement::ForStatement {
                init,
                test,
                update,
                block,
                ..
            } => {
                CForEmitter {
                    enclosing_emitter_scope_depth: self.scope_stack.current_depth(),
                    maybe_init: init,
                    maybe_test: test,
                    maybe_update: update,
                    init: |emitter, val| match val {
                        VariableDeclarationOrExpression::VariableDeclaration(ast) => {
                            emitter.emit_variable_declaration_statement(ast)
                        }
                        VariableDeclarationOrExpression::Expression(expr) => {
                            emitter.emit_expression(expr)?;
                            emitter.emit.pop();
                            Ok(())
                        }
                    },
                    test: |emitter, expr| emitter.emit_expression(expr),
                    update: |emitter, expr| {
                        emitter.emit_expression(expr)?;
                        emitter.emit.pop();
                        Ok(())
                    },
                    block: |emitter| emitter.emit_statement(block),
                }
                .emit(self)?;
            }
            Statement::IfStatement(if_statement) => {
                self.emit_if(if_statement)?;
            }
            Statement::LabelledStatement { label, body, .. } => {
                LabelEmitter {
                    enclosing_emitter_scope_depth: self.scope_stack.current_depth(),
                    name: label.value,
                    body: |emitter| emitter.emit_statement(body),
                }
                .emit(self)?;
            }
            Statement::ReturnStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: ReturnStatement"));
            }
            Statement::SwitchStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: SwitchStatement"));
            }
            Statement::SwitchStatementWithDefault { .. } => {
                return Err(EmitError::NotImplemented(
                    "TODO: SwitchStatementWithDefault",
                ));
            }
            Statement::ThrowStatement { expression, .. } => {
                self.emit_expression(expression)?;
                self.emit.throw_();
            }
            Statement::TryCatchStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: TryCatchStatement"));
            }
            Statement::TryFinallyStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: TryFinallyStatement"));
            }
            Statement::VariableDeclarationStatement(ast) => {
                self.emit_variable_declaration_statement(ast)?;
            }
            Statement::WhileStatement { test, block, .. } => {
                WhileEmitter {
                    enclosing_emitter_scope_depth: self.scope_stack.current_depth(),
                    test: |emitter| emitter.emit_expression(test),
                    block: |emitter| emitter.emit_statement(block),
                }
                .emit(self)?;
            }
            Statement::WithStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: WithStatement"));
            }
            Statement::FunctionDeclaration(fun) => {
                if !self.is_top_level_function(fun) {
                    return Err(EmitError::NotImplemented(
                        "TODO: non-top-level FunctionDeclaration",
                    ));
                }
            }
        };

        Ok(())
    }

    fn note_top_level_function(&mut self, fun: &Function) {
        self.top_level_function_offsets.insert(fun.loc.start);
    }

    fn is_top_level_function(&self, fun: &Function) -> bool {
        self.top_level_function_offsets.contains(&fun.loc.start)
    }

    fn emit_variable_declaration_statement(
        &mut self,
        ast: &VariableDeclaration,
    ) -> Result<(), EmitError> {
        match ast.kind {
            VariableDeclarationKind::Var { .. } => {}
            VariableDeclarationKind::Let { .. } | VariableDeclarationKind::Const { .. } => {}
        }

        for decl in &ast.declarators {
            if let Some(init) = &decl.init {
                self.emit_declaration_assignment(&decl.binding, &init)?;
            }
        }

        Ok(())
    }

    fn emit_declaration_assignment(
        &mut self,
        binding: &Binding,
        init: &Expression,
    ) -> Result<(), EmitError> {
        match binding {
            Binding::BindingIdentifier(binding) => {
                let name = binding.name.value;
                DeclarationEmitter {
                    lhs: |emitter| Ok(NameReferenceEmitter { name }.emit_for_declaration(emitter)),
                    rhs: |emitter| emitter.emit_expression(init),
                }
                .emit(self)?;
                self.emit.pop();
            }
            _ => {
                return Err(EmitError::NotImplemented("BindingPattern"));
            }
        }

        Ok(())
    }

    fn emit_this(&mut self) -> Result<(), EmitError> {
        Err(EmitError::NotImplemented("TODO: this"))
    }

    fn emit_if(&mut self, if_statement: &IfStatement) -> Result<(), EmitError> {
        self.emit_expression(&if_statement.test)?;

        let alternate_jump = ForwardJumpEmitter {
            jump: JumpKind::IfEq,
        }
        .emit(self);

        // Then branch
        self.emit.jump_target();
        self.emit_statement(&if_statement.consequent)?;

        if let Some(alternate) = &if_statement.alternate {
            let then_jump = ForwardJumpEmitter {
                jump: JumpKind::Goto,
            }
            .emit(self);
            // ^^ part of then branch

            // Else branch
            alternate_jump.patch_not_merge(self);
            self.emit_statement(alternate)?;

            // Merge point after else
            then_jump.patch_merge(self);
        } else {
            // Merge point without else
            alternate_jump.patch_merge(self);
        }

        Ok(())
    }

    fn emit_expression(&mut self, ast: &Expression) -> Result<(), EmitError> {
        match ast {
            Expression::ClassExpression(_) => {
                return Err(EmitError::NotImplemented("TODO: ClassExpression"));
            }

            Expression::LiteralBooleanExpression { value, .. } => {
                self.emit.emit_boolean(*value);
            }

            Expression::LiteralInfinityExpression { .. } => {
                self.emit.double_(std::f64::INFINITY);
            }

            Expression::LiteralNullExpression { .. } => {
                self.emit.null();
            }

            Expression::LiteralNumericExpression(num) => {
                self.emit.numeric(num.value);
            }

            Expression::LiteralRegExpExpression {
                pattern,
                global,
                ignore_case,
                multi_line,
                dot_all,
                sticky,
                unicode,
                ..
            } => {
                let item = RegExpItem {
                    pattern: *pattern,
                    global: *global,
                    ignore_case: *ignore_case,
                    multi_line: *multi_line,
                    dot_all: *dot_all,
                    sticky: *sticky,
                    unicode: *unicode,
                };
                let index = self.emit.get_regexp_gcthing_index(item);
                self.emit.reg_exp(index);
            }

            Expression::LiteralStringExpression { value, .. } => {
                let str_index = self.emit.get_atom_gcthing_index(*value);
                self.emit.string(str_index);
            }

            Expression::ArrayExpression(ast) => {
                self.emit_array_expression(ast)?;
            }

            Expression::ArrowExpression { .. } => {
                return Err(EmitError::NotImplemented("TODO: ArrowExpression"));
            }

            Expression::AssignmentExpression {
                binding,
                expression,
                ..
            } => {
                self.emit_assignment_expression(binding, expression)?;
            }

            Expression::BinaryExpression {
                operator,
                left,
                right,
                ..
            } => {
                self.emit_binary_expression(operator, left, right)?;
            }

            Expression::CallExpression(CallExpression {
                callee, arguments, ..
            }) => {
                self.emit_call_expression(callee, arguments)?;
            }

            Expression::CompoundAssignmentExpression { .. } => {
                return Err(EmitError::NotImplemented(
                    "TODO: CompoundAssignmentExpression",
                ));
            }

            Expression::ConditionalExpression {
                test,
                consequent,
                alternate,
                ..
            } => {
                self.emit_conditional_expression(test, consequent, alternate)?;
            }

            Expression::FunctionExpression(_) => {
                return Err(EmitError::NotImplemented("TODO: FunctionExpression"));
            }

            Expression::IdentifierExpression(ast) => {
                self.emit_identifier_expression(ast);
            }

            Expression::MemberExpression(ast) => {
                self.emit_member_expression(ast)?;
            }

            Expression::NewExpression {
                callee, arguments, ..
            } => {
                self.emit_new_expression(callee, arguments)?;
            }

            Expression::NewTargetExpression { .. } => {
                return Err(EmitError::NotImplemented("TODO: NewTargetExpression"));
            }

            Expression::ObjectExpression(ast) => {
                self.emit_object_expression(ast)?;
            }

            Expression::OptionalChain { .. } => {
                return Err(EmitError::NotImplemented("TODO: OptionalChain"));
            }

            Expression::OptionalExpression { .. } => {
                return Err(EmitError::NotImplemented("TODO: OptionalExpression"));
            }

            Expression::UnaryExpression {
                operator, operand, ..
            } => {
                let opcode = match operator {
                    UnaryOperator::Plus { .. } => Opcode::Pos,
                    UnaryOperator::Minus { .. } => Opcode::Neg,
                    UnaryOperator::LogicalNot { .. } => Opcode::Not,
                    UnaryOperator::BitwiseNot { .. } => Opcode::BitNot,
                    UnaryOperator::Void { .. } => Opcode::Void,
                    UnaryOperator::Typeof { .. } => {
                        return Err(EmitError::NotImplemented("TODO: Typeof"));
                    }
                    UnaryOperator::Delete { .. } => {
                        return Err(EmitError::NotImplemented("TODO: Delete"));
                    }
                };
                self.emit_expression(operand)?;
                self.emit.emit_unary_op(opcode);
            }

            Expression::TemplateExpression(_) => {
                return Err(EmitError::NotImplemented("TODO: TemplateExpression"));
            }

            Expression::ThisExpression { .. } => {
                self.emit_this()?;
            }

            Expression::UpdateExpression { .. } => {
                return Err(EmitError::NotImplemented("TODO: UpdateExpression"));
            }

            Expression::YieldExpression { .. } => {
                return Err(EmitError::NotImplemented("TODO: YieldExpression"));
            }

            Expression::YieldGeneratorExpression { .. } => {
                return Err(EmitError::NotImplemented("TODO: YieldGeneratorExpression"));
            }

            Expression::AwaitExpression { .. } => {
                return Err(EmitError::NotImplemented("TODO: AwaitExpression"));
            }

            Expression::ImportCallExpression { .. } => {
                return Err(EmitError::NotImplemented("TODO: ImportCallExpression"));
            }
        }

        Ok(())
    }

    fn emit_binary_expression(
        &mut self,
        operator: &BinaryOperator,
        left: &Expression,
        right: &Expression,
    ) -> Result<(), EmitError> {
        let opcode = match operator {
            BinaryOperator::Equals { .. } => Opcode::Eq,
            BinaryOperator::NotEquals { .. } => Opcode::Ne,
            BinaryOperator::StrictEquals { .. } => Opcode::StrictEq,
            BinaryOperator::StrictNotEquals { .. } => Opcode::StrictNe,
            BinaryOperator::LessThan { .. } => Opcode::Lt,
            BinaryOperator::LessThanOrEqual { .. } => Opcode::Le,
            BinaryOperator::GreaterThan { .. } => Opcode::Gt,
            BinaryOperator::GreaterThanOrEqual { .. } => Opcode::Ge,
            BinaryOperator::In { .. } => Opcode::In,
            BinaryOperator::Instanceof { .. } => Opcode::Instanceof,
            BinaryOperator::LeftShift { .. } => Opcode::Lsh,
            BinaryOperator::RightShift { .. } => Opcode::Rsh,
            BinaryOperator::RightShiftExt { .. } => Opcode::Ursh,
            BinaryOperator::Add { .. } => Opcode::Add,
            BinaryOperator::Sub { .. } => Opcode::Sub,
            BinaryOperator::Mul { .. } => Opcode::Mul,
            BinaryOperator::Div { .. } => Opcode::Div,
            BinaryOperator::Mod { .. } => Opcode::Mod,
            BinaryOperator::Pow { .. } => Opcode::Pow,
            BinaryOperator::BitwiseOr { .. } => Opcode::BitOr,
            BinaryOperator::BitwiseXor { .. } => Opcode::BitXor,
            BinaryOperator::BitwiseAnd { .. } => Opcode::BitAnd,

            BinaryOperator::Coalesce { .. } => {
                self.emit_short_circuit(JumpKind::Coalesce, left, right)?;
                return Ok(());
            }
            BinaryOperator::LogicalOr { .. } => {
                self.emit_short_circuit(JumpKind::LogicalOr, left, right)?;
                return Ok(());
            }
            BinaryOperator::LogicalAnd { .. } => {
                self.emit_short_circuit(JumpKind::LogicalAnd, left, right)?;
                return Ok(());
            }

            BinaryOperator::Comma { .. } => {
                self.emit_expression(left)?;
                self.emit.pop();
                self.emit_expression(right)?;
                return Ok(());
            }
        };

        self.emit_expression(left)?;
        self.emit_expression(right)?;
        self.emit.emit_binary_op(opcode);
        Ok(())
    }

    fn emit_short_circuit(
        &mut self,
        jump: JumpKind,
        left: &Expression,
        right: &Expression,
    ) -> Result<(), EmitError> {
        self.emit_expression(left)?;
        let jump = ForwardJumpEmitter { jump: jump }.emit(self);
        self.emit.pop();
        self.emit_expression(right)?;
        jump.patch_merge(self);
        return Ok(());
    }

    fn emit_object_expression(&mut self, object: &ObjectExpression) -> Result<(), EmitError> {
        ObjectEmitter {
            properties: object.properties.iter(),
            prop: |emitter, state, prop| emitter.emit_object_property(state, prop),
        }
        .emit(self)
    }

    fn emit_object_property(
        &mut self,
        state: &mut ObjectEmitterState,
        property: &ObjectProperty,
    ) -> Result<(), EmitError> {
        match property {
            ObjectProperty::NamedObjectProperty(NamedObjectProperty::DataProperty(
                DataProperty {
                    property_name,
                    expression: prop_value,
                    ..
                },
            )) => match property_name {
                PropertyName::StaticPropertyName(StaticPropertyName { value, .. }) => {
                    NamePropertyEmitter {
                        state,
                        key: *value,
                        value: |emitter| emitter.emit_expression(prop_value),
                    }
                    .emit(self)?;
                }
                PropertyName::StaticNumericPropertyName(NumericLiteral { value, .. }) => {
                    IndexPropertyEmitter {
                        state,
                        key: *value,
                        value: |emitter| emitter.emit_expression(prop_value),
                    }
                    .emit(self)?;
                }
                PropertyName::ComputedPropertyName(ComputedPropertyName {
                    expression: prop_key,
                    ..
                }) => {
                    ComputedPropertyEmitter {
                        state,
                        key: |emitter| emitter.emit_expression(prop_key),
                        value: |emitter| emitter.emit_expression(prop_value),
                    }
                    .emit(self)?;
                }
            },
            _ => return Err(EmitError::NotImplemented("TODO: non data property")),
        }
        Ok(())
    }

    fn emit_array_expression(&mut self, array: &ArrayExpression) -> Result<(), EmitError> {
        ArrayEmitter {
            elements: array.elements.iter(),
            elem_kind: |e| match e {
                ArrayExpressionElement::Expression(..) => ArrayElementKind::Normal,
                ArrayExpressionElement::Elision { .. } => ArrayElementKind::Elision,
                ArrayExpressionElement::SpreadElement(..) => ArrayElementKind::Spread,
            },
            elem: |emitter, state, elem| {
                match elem {
                    ArrayExpressionElement::Expression(expr) => {
                        ArrayElementEmitter {
                            state,
                            elem: |emitter| emitter.emit_expression(expr),
                        }
                        .emit(emitter)?;
                    }
                    ArrayExpressionElement::Elision { .. } => {
                        ArrayElisionEmitter { state }.emit(emitter);
                    }
                    ArrayExpressionElement::SpreadElement(expr) => {
                        ArraySpreadEmitter {
                            state,
                            elem: |emitter| emitter.emit_expression(expr),
                        }
                        .emit(emitter)?;
                    }
                }
                Ok(())
            },
        }
        .emit(self)
    }

    fn emit_conditional_expression(
        &mut self,
        test: &Expression,
        consequent: &Expression,
        alternate: &Expression,
    ) -> Result<(), EmitError> {
        self.emit_expression(test)?;

        let else_jump = ForwardJumpEmitter {
            jump: JumpKind::IfEq,
        }
        .emit(self);

        // Then branch
        self.emit.jump_target();
        self.emit_expression(consequent)?;

        let finally_jump = ForwardJumpEmitter {
            jump: JumpKind::Goto,
        }
        .emit(self);

        // Else branch
        else_jump.patch_not_merge(self);
        self.emit_expression(alternate)?;

        // Merge point
        finally_jump.patch_merge(self);

        Ok(())
    }

    fn emit_assignment_expression(
        &mut self,
        binding: &AssignmentTarget,
        expression: &Expression,
    ) -> Result<(), EmitError> {
        AssignmentEmitter {
            lhs: |emitter| match binding {
                AssignmentTarget::SimpleAssignmentTarget(
                    SimpleAssignmentTarget::AssignmentTargetIdentifier(
                        AssignmentTargetIdentifier { name, .. },
                    ),
                ) => Ok(NameReferenceEmitter { name: name.value }.emit_for_assignment(emitter)),
                _ => Err(EmitError::NotImplemented(
                    "non-identifier assignment target",
                )),
            },
            rhs: |emitter| emitter.emit_expression(expression),
        }
        .emit(self)
    }

    fn emit_identifier_expression(&mut self, ast: &IdentifierExpression) {
        let name = ast.name.value;
        GetNameEmitter { name }.emit(self);
    }

    fn emit_member_expression(&mut self, ast: &MemberExpression) -> Result<(), EmitError> {
        match ast {
            MemberExpression::ComputedMemberExpression(ComputedMemberExpression {
                object: ExpressionOrSuper::Expression(object),
                expression,
                ..
            }) => GetElemEmitter {
                obj: |emitter| emitter.emit_expression(object),
                key: |emitter| emitter.emit_expression(expression),
            }
            .emit(self),

            MemberExpression::ComputedMemberExpression(ComputedMemberExpression {
                object: ExpressionOrSuper::Super { .. },
                expression,
                ..
            }) => GetSuperElemEmitter {
                this: |emitter| emitter.emit_this(),
                key: |emitter| emitter.emit_expression(expression),
            }
            .emit(self),

            MemberExpression::StaticMemberExpression(StaticMemberExpression {
                object: ExpressionOrSuper::Expression(object),
                property,
                ..
            }) => GetPropEmitter {
                obj: |emitter| emitter.emit_expression(object),
                key: property.value,
            }
            .emit(self),

            MemberExpression::StaticMemberExpression(StaticMemberExpression {
                object: ExpressionOrSuper::Super { .. },
                property,
                ..
            }) => GetSuperPropEmitter {
                this: |emitter| emitter.emit_this(),
                key: property.value,
            }
            .emit(self),

            MemberExpression::PrivateFieldExpression(PrivateFieldExpression { .. }) => {
                Err(EmitError::NotImplemented("PrivateFieldExpression"))
            }
        }
    }

    fn emit_new_expression(
        &mut self,
        callee: &Expression,
        arguments: &Arguments,
    ) -> Result<(), EmitError> {
        for arg in &arguments.args {
            if let Argument::SpreadElement(_) = arg {
                return Err(EmitError::NotImplemented("TODO: SpreadNew"));
            }
        }

        NewEmitter {
            callee: |emitter| emitter.emit_expression(callee),
            arguments: |emitter| {
                emitter.emit_arguments(arguments)?;
                Ok(arguments.args.len())
            },
        }
        .emit(self)?;

        Ok(())
    }

    fn emit_call_expression(
        &mut self,
        callee: &ExpressionOrSuper,
        arguments: &Arguments,
    ) -> Result<(), EmitError> {
        // Don't do super handling in an emit_expresion_or_super because the bytecode heavily
        // depends on how you're using the super

        CallEmitter {
            callee: |emitter| match callee {
                ExpressionOrSuper::Expression(expr) => match &**expr {
                    Expression::IdentifierExpression(IdentifierExpression { name, .. }) => {
                        if name.value == CommonSourceAtomSetIndices::eval() {
                            return Err(EmitError::NotImplemented("TODO: direct eval"));
                        }
                        Ok(NameReferenceEmitter { name: name.value }.emit_for_call(emitter))
                    }

                    Expression::MemberExpression(MemberExpression::StaticMemberExpression(
                        StaticMemberExpression {
                            object: ExpressionOrSuper::Expression(object),
                            property,
                            ..
                        },
                    )) => PropReferenceEmitter {
                        obj: |emitter| emitter.emit_expression(object),
                        key: property.value,
                    }
                    .emit_for_call(emitter),

                    Expression::MemberExpression(MemberExpression::ComputedMemberExpression(
                        ComputedMemberExpression {
                            object: ExpressionOrSuper::Expression(object),
                            expression,
                            ..
                        },
                    )) => ElemReferenceEmitter {
                        obj: |emitter| emitter.emit_expression(object),
                        key: |emitter| emitter.emit_expression(expression),
                    }
                    .emit_for_call(emitter),

                    _ => {
                        return Err(EmitError::NotImplemented(
                            "TODO: Call (only global functions are supported)",
                        ));
                    }
                },
                _ => {
                    return Err(EmitError::NotImplemented("TODO: Super"));
                }
            },
            arguments: |emitter| {
                emitter.emit_arguments(arguments)?;
                Ok(arguments.args.len())
            },
        }
        .emit(self)?;

        Ok(())
    }

    fn emit_arguments(&mut self, ast: &Arguments) -> Result<(), EmitError> {
        for argument in &ast.args {
            self.emit_argument(argument)?;
        }

        Ok(())
    }

    fn emit_argument(&mut self, ast: &Argument) -> Result<(), EmitError> {
        match ast {
            Argument::Expression(ast) => self.emit_expression(ast)?,
            Argument::SpreadElement(_) => {
                return Err(EmitError::NotImplemented("TODO: SpreadElement"));
            }
        }

        Ok(())
    }
}
