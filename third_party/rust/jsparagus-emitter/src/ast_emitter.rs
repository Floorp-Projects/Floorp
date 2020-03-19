//! High-level bytecode emitter.
//!
//! Converts AST nodes to bytecode.

use crate::compilation_info::CompilationInfo;
use crate::emitter::{EmitError, EmitOptions, EmitResult, InstructionWriter};
use crate::emitter_scope::{EmitterScopeStack, NameLocation};
use crate::opcode::Opcode;
use crate::reference_op_emitter::{
    AssignmentEmitter, CallEmitter, DeclarationEmitter, ElemReferenceEmitter, GetElemEmitter,
    GetNameEmitter, GetPropEmitter, GetSuperElemEmitter, GetSuperPropEmitter, NameReferenceEmitter,
    NewEmitter, PropReferenceEmitter,
};
use crate::scope::ScopeDataMap;
use ast::source_atom_set::{CommonSourceAtomSetIndices, SourceAtomSet, SourceAtomSetIndex};
use ast::types::*;

use crate::forward_jump_emitter::{ForwardJumpEmitter, JumpKind};

/// Emit a program, converting the AST directly to bytecode.
pub fn emit_program<'alloc>(
    ast: &Program,
    options: &EmitOptions,
    atoms: SourceAtomSet<'alloc>,
    scope_data_map: ScopeDataMap,
) -> Result<EmitResult<'alloc>, EmitError> {
    let mut emitter = AstEmitter::new(options, atoms, scope_data_map);

    match ast {
        Program::Script(script) => emitter.emit_script(script)?,
        _ => {
            return Err(EmitError::NotImplemented("TODO: modules"));
        }
    }

    Ok(emitter.emit.into_emit_result(emitter.compilation_info))
}

pub struct AstEmitter<'alloc, 'opt> {
    pub emit: InstructionWriter,
    options: &'opt EmitOptions,
    compilation_info: CompilationInfo<'alloc>,
    scope_stack: EmitterScopeStack,
}

impl<'alloc, 'opt> AstEmitter<'alloc, 'opt> {
    fn new(
        options: &'opt EmitOptions,
        atoms: SourceAtomSet<'alloc>,
        scope_data_map: ScopeDataMap,
    ) -> Self {
        Self {
            emit: InstructionWriter::new(),
            options,
            compilation_info: CompilationInfo::new(atoms, scope_data_map),
            scope_stack: EmitterScopeStack::new(),
        }
    }

    pub fn lookup_name(&mut self, name: SourceAtomSetIndex) -> NameLocation {
        self.scope_stack.lookup_name(name)
    }

    fn emit_script(&mut self, ast: &Script) -> Result<(), EmitError> {
        self.scope_stack
            .enter_global(&mut self.emit, &self.compilation_info.scope_data_map);

        for statement in &ast.statements {
            self.emit_statement(statement)?;
        }
        self.emit.ret_rval();

        self.scope_stack.leave_global(&mut self.emit);

        Ok(())
    }

    fn emit_statement(&mut self, ast: &Statement) -> Result<(), EmitError> {
        match ast {
            Statement::ClassDeclaration(_) => {
                return Err(EmitError::NotImplemented("TODO: ClassDeclaration"));
            }
            Statement::BlockStatement { block, .. } => {
                let scope_data_index = self.compilation_info.scope_data_map.get_index(block);

                self.scope_stack.enter_lexical(
                    &mut self.emit,
                    &mut self.compilation_info.scope_data_map,
                    scope_data_index,
                );

                for statement in &block.statements {
                    self.emit_statement(statement)?;
                }

                self.scope_stack.leave_lexical(&mut self.emit);
            }
            Statement::BreakStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: BreakStatement"));
            }
            Statement::ContinueStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: ContinueStatement"));
            }
            Statement::DebuggerStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: DebuggerStatement"));
            }
            Statement::DoWhileStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: DoWhileStatement"));
            }
            Statement::EmptyStatement { .. } => (),
            Statement::ExpressionStatement(ast) => {
                self.emit_expression(ast)?;
                if self.options.no_script_rval {
                    self.emit.pop();
                } else {
                    self.emit.set_rval();
                }
            }
            Statement::ForInStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: ForInStatement"));
            }
            Statement::ForOfStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: ForOfStatement"));
            }
            Statement::ForStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: ForStatement"));
            }
            Statement::IfStatement(if_statement) => {
                self.emit_if(if_statement)?;
            }
            Statement::LabeledStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: LabeledStatement"));
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
            Statement::WhileStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: WhileStatement"));
            }
            Statement::WithStatement { .. } => {
                return Err(EmitError::NotImplemented("TODO: WithStatement"));
            }
            Statement::FunctionDeclaration(_) => {
                return Err(EmitError::NotImplemented("TODO: FunctionDeclaration"));
            }
        };

        Ok(())
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
                self.emit_numeric_expression(num.value);
            }

            Expression::LiteralRegExpExpression { .. } => {
                return Err(EmitError::NotImplemented("TODO: LiteralRegExpExpression"));
            }

            Expression::LiteralStringExpression { value, .. } => {
                let str_index = self.emit.get_atom_index(*value);
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

    fn emit_numeric_expression(&mut self, value: f64) {
        if value.is_finite() && value.fract() == 0.0 {
            if i8::min_value() as f64 <= value && value <= i8::max_value() as f64 {
                self.emit.int8(value as i8);
                return;
            }
            if i32::min_value() as f64 <= value && value <= i32::max_value() as f64 {
                self.emit.int32(value as i32);
                return;
            }
        }
        self.emit.double_(value);
    }

    fn emit_object_expression(&mut self, object: &ObjectExpression) -> Result<(), EmitError> {
        self.emit.new_init();

        for property in object.properties.iter() {
            self.emit_object_property(property)?;
        }

        Ok(())
    }

    fn emit_object_property(&mut self, property: &ObjectProperty) -> Result<(), EmitError> {
        match property {
            ObjectProperty::NamedObjectProperty(NamedObjectProperty::DataProperty(
                DataProperty {
                    property_name,
                    expression,
                    ..
                },
            )) => match property_name {
                PropertyName::StaticPropertyName(StaticPropertyName { value, .. }) => {
                    match self.to_property_index(*value) {
                        Some(value) => {
                            self.emit.double_(value as f64);
                            self.emit_expression(expression)?;
                            self.emit.init_elem();
                        }
                        None => {
                            self.emit_expression(expression)?;
                            let name_index = self.emit.get_atom_index(*value);
                            self.emit.init_prop(name_index);
                        }
                    }
                }
                PropertyName::StaticNumericPropertyName(NumericLiteral { value, .. }) => {
                    self.emit.double_(*value);
                    self.emit_expression(expression)?;
                    self.emit.init_elem();
                }
                PropertyName::ComputedPropertyName(ComputedPropertyName { .. }) => {
                    return Err(EmitError::NotImplemented("TODO: computed property"))
                }
            },
            _ => return Err(EmitError::NotImplemented("TODO: non data property")),
        }

        Ok(())
    }

    fn to_property_index(&self, index: SourceAtomSetIndex) -> Option<u32> {
        let s = self.compilation_info.atoms.get(index);
        s.parse::<u32>().ok()
    }

    fn emit_array_expression(&mut self, array: &ArrayExpression) -> Result<(), EmitError> {
        // Initialize the array to its minimum possible length.
        let min_length = array
            .elements
            .iter()
            .map(|e| match e {
                ArrayExpressionElement::Expression(_) => 1,
                ArrayExpressionElement::Elision { .. } => 1,
                ArrayExpressionElement::SpreadElement { .. } => 0,
            })
            .sum::<u32>();

        self.emit.new_array(min_length);

        for (index, element) in array.elements.iter().enumerate() {
            match element {
                ArrayExpressionElement::Expression(expr) => {
                    self.emit_expression(&expr)?;
                    self.emit.init_elem_array(index as u32);
                }
                ArrayExpressionElement::Elision { .. } => {
                    self.emit.hole();
                    self.emit.init_elem_array(index as u32);
                }
                ArrayExpressionElement::SpreadElement { .. } => {
                    return Err(EmitError::NotImplemented("TODO: SpreadElement"));
                }
            }
        }

        Ok(())
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
