use super::{
    analyzer::{UniformityDisruptor, UniformityRequirements},
    ExpressionError, FunctionInfo, ModuleInfo, ShaderStages, TypeFlags, ValidationFlags,
};
use crate::arena::{Arena, Handle};
use bit_set::BitSet;

#[derive(Clone, Debug, thiserror::Error)]
#[cfg_attr(test, derive(PartialEq))]
pub enum CallError {
    #[error("Bad function")]
    InvalidFunction,
    #[error("The callee is declared after the caller")]
    ForwardDeclaredFunction,
    #[error("Argument {index} expression is invalid")]
    Argument {
        index: usize,
        #[source]
        error: ExpressionError,
    },
    #[error("Result expression {0:?} has already been introduced earlier")]
    ResultAlreadyInScope(Handle<crate::Expression>),
    #[error("Result value is invalid")]
    ResultValue(#[source] ExpressionError),
    #[error("Requires {required} arguments, but {seen} are provided")]
    ArgumentCount { required: usize, seen: usize },
    #[error("Argument {index} value {seen_expression:?} doesn't match the type {required:?}")]
    ArgumentType {
        index: usize,
        required: Handle<crate::Type>,
        seen_expression: Handle<crate::Expression>,
    },
    #[error("The emitted expression doesn't match the call")]
    ExpressionMismatch(Option<Handle<crate::Expression>>),
}

#[derive(Clone, Debug, thiserror::Error)]
#[cfg_attr(test, derive(PartialEq))]
pub enum AtomicError {
    #[error("Pointer {0:?} to atomic is invalid.")]
    InvalidPointer(Handle<crate::Expression>),
    #[error("Operand {0:?} has invalid type.")]
    InvalidOperand(Handle<crate::Expression>),
    #[error("Result expression {0:?} has already been introduced earlier")]
    ResultAlreadyInScope(Handle<crate::Expression>),
    #[error("Result type for {0:?} doesn't match the statement")]
    ResultTypeMismatch(Handle<crate::Expression>),
}

#[derive(Clone, Debug, thiserror::Error)]
#[cfg_attr(test, derive(PartialEq))]
pub enum LocalVariableError {
    #[error("Local variable has a type {0:?} that can't be stored in a local variable.")]
    InvalidType(Handle<crate::Type>),
    #[error("Initializer doesn't match the variable type")]
    InitializerType,
}

#[derive(Clone, Debug, thiserror::Error)]
#[cfg_attr(test, derive(PartialEq))]
pub enum FunctionError {
    #[error("Expression {handle:?} is invalid")]
    Expression {
        handle: Handle<crate::Expression>,
        #[source]
        error: ExpressionError,
    },
    #[error("Expression {0:?} can't be introduced - it's already in scope")]
    ExpressionAlreadyInScope(Handle<crate::Expression>),
    #[error("Local variable {handle:?} '{name}' is invalid")]
    LocalVariable {
        handle: Handle<crate::LocalVariable>,
        name: String,
        #[source]
        error: LocalVariableError,
    },
    #[error("Argument '{name}' at index {index} has a type that can't be passed into functions.")]
    InvalidArgumentType { index: usize, name: String },
    #[error("There are instructions after `return`/`break`/`continue`")]
    InstructionsAfterReturn,
    #[error("The `break` is used outside of a `loop` or `switch` context")]
    BreakOutsideOfLoopOrSwitch,
    #[error("The `continue` is used outside of a `loop` context")]
    ContinueOutsideOfLoop,
    #[error("The `return` is called within a `continuing` block")]
    InvalidReturnSpot,
    #[error("The `return` value {0:?} does not match the function return value")]
    InvalidReturnType(Option<Handle<crate::Expression>>),
    #[error("The `if` condition {0:?} is not a boolean scalar")]
    InvalidIfType(Handle<crate::Expression>),
    #[error("The `switch` value {0:?} is not an integer scalar")]
    InvalidSwitchType(Handle<crate::Expression>),
    #[error("Multiple `switch` cases for {0} are present")]
    ConflictingSwitchCase(i32),
    #[error("The pointer {0:?} doesn't relate to a valid destination for a store")]
    InvalidStorePointer(Handle<crate::Expression>),
    #[error("The value {0:?} can not be stored")]
    InvalidStoreValue(Handle<crate::Expression>),
    #[error("Store of {value:?} into {pointer:?} doesn't have matching types")]
    InvalidStoreTypes {
        pointer: Handle<crate::Expression>,
        value: Handle<crate::Expression>,
    },
    #[error("The expression {0:?} is currupted")]
    InvalidExpression(Handle<crate::Expression>),
    #[error("Image store parameters are invalid")]
    InvalidImageStore(#[source] ExpressionError),
    #[error("Call to {function:?} is invalid")]
    InvalidCall {
        function: Handle<crate::Function>,
        #[source]
        error: CallError,
    },
    #[error("Atomic operation is invalid")]
    InvalidAtomic(#[from] AtomicError),
    #[error(
        "Required uniformity of control flow for {0:?} in {1:?} is not fulfilled because of {2:?}"
    )]
    NonUniformControlFlow(
        UniformityRequirements,
        Handle<crate::Expression>,
        UniformityDisruptor,
    ),
}

bitflags::bitflags! {
    #[repr(transparent)]
    struct ControlFlowAbility: u8 {
        /// The control can return out of this block.
        const RETURN = 0x1;
        /// The control can break.
        const BREAK = 0x2;
        /// The control can continue.
        const CONTINUE = 0x4;
    }
}

struct BlockContext<'a> {
    abilities: ControlFlowAbility,
    info: &'a FunctionInfo,
    expressions: &'a Arena<crate::Expression>,
    types: &'a Arena<crate::Type>,
    global_vars: &'a Arena<crate::GlobalVariable>,
    functions: &'a Arena<crate::Function>,
    prev_infos: &'a [FunctionInfo],
    return_type: Option<Handle<crate::Type>>,
}

impl<'a> BlockContext<'a> {
    fn new(
        fun: &'a crate::Function,
        module: &'a crate::Module,
        info: &'a FunctionInfo,
        prev_infos: &'a [FunctionInfo],
    ) -> Self {
        Self {
            abilities: ControlFlowAbility::RETURN,
            info,
            expressions: &fun.expressions,
            types: &module.types,
            global_vars: &module.global_variables,
            functions: &module.functions,
            prev_infos,
            return_type: fun.result.as_ref().map(|fr| fr.ty),
        }
    }

    fn with_abilities(&self, abilities: ControlFlowAbility) -> Self {
        BlockContext { abilities, ..*self }
    }

    fn get_expression(
        &self,
        handle: Handle<crate::Expression>,
    ) -> Result<&'a crate::Expression, FunctionError> {
        self.expressions
            .try_get(handle)
            .ok_or(FunctionError::InvalidExpression(handle))
    }

    fn resolve_type_impl(
        &self,
        handle: Handle<crate::Expression>,
        valid_expressions: &BitSet,
    ) -> Result<&crate::TypeInner, ExpressionError> {
        if handle.index() >= self.expressions.len() {
            Err(ExpressionError::DoesntExist)
        } else if !valid_expressions.contains(handle.index()) {
            Err(ExpressionError::NotInScope)
        } else {
            Ok(self.info[handle].ty.inner_with(self.types))
        }
    }

    fn resolve_type(
        &self,
        handle: Handle<crate::Expression>,
        valid_expressions: &BitSet,
    ) -> Result<&crate::TypeInner, FunctionError> {
        self.resolve_type_impl(handle, valid_expressions)
            .map_err(|error| FunctionError::Expression { handle, error })
    }

    fn resolve_pointer_type(
        &self,
        handle: Handle<crate::Expression>,
    ) -> Result<&crate::TypeInner, FunctionError> {
        if handle.index() >= self.expressions.len() {
            Err(FunctionError::Expression {
                handle,
                error: ExpressionError::DoesntExist,
            })
        } else {
            Ok(self.info[handle].ty.inner_with(self.types))
        }
    }
}

impl super::Validator {
    fn validate_call(
        &mut self,
        function: Handle<crate::Function>,
        arguments: &[Handle<crate::Expression>],
        result: Option<Handle<crate::Expression>>,
        context: &BlockContext,
    ) -> Result<ShaderStages, CallError> {
        let fun = context
            .functions
            .try_get(function)
            .ok_or(CallError::InvalidFunction)?;
        if fun.arguments.len() != arguments.len() {
            return Err(CallError::ArgumentCount {
                required: fun.arguments.len(),
                seen: arguments.len(),
            });
        }
        for (index, (arg, &expr)) in fun.arguments.iter().zip(arguments).enumerate() {
            let ty = context
                .resolve_type_impl(expr, &self.valid_expression_set)
                .map_err(|error| CallError::Argument { index, error })?;
            if ty != &context.types[arg.ty].inner {
                return Err(CallError::ArgumentType {
                    index,
                    required: arg.ty,
                    seen_expression: expr,
                });
            }
        }

        if let Some(expr) = result {
            if self.valid_expression_set.insert(expr.index()) {
                self.valid_expression_list.push(expr);
            } else {
                return Err(CallError::ResultAlreadyInScope(expr));
            }
            match context.expressions[expr] {
                crate::Expression::CallResult(callee)
                    if fun.result.is_some() && callee == function => {}
                _ => return Err(CallError::ExpressionMismatch(result)),
            }
        } else if fun.result.is_some() {
            return Err(CallError::ExpressionMismatch(result));
        }

        let callee_info = &context.prev_infos[function.index()];
        Ok(callee_info.available_stages)
    }

    fn validate_atomic(
        &mut self,
        pointer: Handle<crate::Expression>,
        fun: &crate::AtomicFunction,
        value: Handle<crate::Expression>,
        result: Handle<crate::Expression>,
        context: &BlockContext,
    ) -> Result<(), FunctionError> {
        let pointer_inner = context.resolve_type(pointer, &self.valid_expression_set)?;
        let (ptr_kind, ptr_width) = match *pointer_inner {
            crate::TypeInner::Pointer { base, .. } => match context.types[base].inner {
                crate::TypeInner::Atomic { kind, width } => (kind, width),
                ref other => {
                    log::error!("Atomic pointer to type {:?}", other);
                    return Err(AtomicError::InvalidPointer(pointer).into());
                }
            },
            ref other => {
                log::error!("Atomic on type {:?}", other);
                return Err(AtomicError::InvalidPointer(pointer).into());
            }
        };

        let value_inner = context.resolve_type(value, &self.valid_expression_set)?;
        match *value_inner {
            crate::TypeInner::Scalar { width, kind } if kind == ptr_kind && width == ptr_width => {}
            ref other => {
                log::error!("Atomic operand type {:?}", other);
                return Err(AtomicError::InvalidOperand(value).into());
            }
        }

        if let crate::AtomicFunction::Exchange { compare: Some(cmp) } = *fun {
            if context.resolve_type(cmp, &self.valid_expression_set)? != value_inner {
                log::error!("Atomic exchange comparison has a different type from the value");
                return Err(AtomicError::InvalidOperand(cmp).into());
            }
        }

        if self.valid_expression_set.insert(result.index()) {
            self.valid_expression_list.push(result);
        } else {
            return Err(AtomicError::ResultAlreadyInScope(result).into());
        }
        match context.expressions[result] {
            //TODO: support atomic result with comparison
            crate::Expression::AtomicResult {
                kind,
                width,
                comparison: false,
            } if kind == ptr_kind && width == ptr_width => {}
            _ => return Err(AtomicError::ResultTypeMismatch(result).into()),
        }
        Ok(())
    }

    fn validate_block_impl(
        &mut self,
        statements: &[crate::Statement],
        context: &BlockContext,
    ) -> Result<ShaderStages, FunctionError> {
        use crate::{Statement as S, TypeInner as Ti};
        let mut finished = false;
        let mut stages = ShaderStages::all();
        for statement in statements {
            if finished {
                return Err(FunctionError::InstructionsAfterReturn);
            }
            match *statement {
                S::Emit(ref range) => {
                    for handle in range.clone() {
                        if self.valid_expression_set.insert(handle.index()) {
                            self.valid_expression_list.push(handle);
                        } else {
                            return Err(FunctionError::ExpressionAlreadyInScope(handle));
                        }
                    }
                }
                S::Block(ref block) => {
                    stages &= self.validate_block(block, context)?;
                }
                S::If {
                    condition,
                    ref accept,
                    ref reject,
                } => {
                    match *context.resolve_type(condition, &self.valid_expression_set)? {
                        Ti::Scalar {
                            kind: crate::ScalarKind::Bool,
                            width: _,
                        } => {}
                        _ => return Err(FunctionError::InvalidIfType(condition)),
                    }
                    stages &= self.validate_block(accept, context)?;
                    stages &= self.validate_block(reject, context)?;
                }
                S::Switch {
                    selector,
                    ref cases,
                    ref default,
                } => {
                    match *context.resolve_type(selector, &self.valid_expression_set)? {
                        Ti::Scalar {
                            kind: crate::ScalarKind::Sint,
                            width: _,
                        } => {}
                        _ => return Err(FunctionError::InvalidSwitchType(selector)),
                    }
                    self.select_cases.clear();
                    for case in cases {
                        if !self.select_cases.insert(case.value) {
                            return Err(FunctionError::ConflictingSwitchCase(case.value));
                        }
                    }
                    let pass_through_abilities = context.abilities
                        & (ControlFlowAbility::RETURN | ControlFlowAbility::CONTINUE);
                    let sub_context =
                        context.with_abilities(pass_through_abilities | ControlFlowAbility::BREAK);
                    for case in cases {
                        stages &= self.validate_block(&case.body, &sub_context)?;
                    }
                    stages &= self.validate_block(default, &sub_context)?;
                }
                S::Loop {
                    ref body,
                    ref continuing,
                } => {
                    // special handling for block scoping is needed here,
                    // because the continuing{} block inherits the scope
                    let base_expression_count = self.valid_expression_list.len();
                    let pass_through_abilities = context.abilities & ControlFlowAbility::RETURN;
                    stages &= self.validate_block_impl(
                        body,
                        &context.with_abilities(
                            pass_through_abilities
                                | ControlFlowAbility::BREAK
                                | ControlFlowAbility::CONTINUE,
                        ),
                    )?;
                    stages &= self.validate_block_impl(
                        continuing,
                        &context.with_abilities(ControlFlowAbility::empty()),
                    )?;
                    for handle in self.valid_expression_list.drain(base_expression_count..) {
                        self.valid_expression_set.remove(handle.index());
                    }
                }
                S::Break => {
                    if !context.abilities.contains(ControlFlowAbility::BREAK) {
                        return Err(FunctionError::BreakOutsideOfLoopOrSwitch);
                    }
                    finished = true;
                }
                S::Continue => {
                    if !context.abilities.contains(ControlFlowAbility::CONTINUE) {
                        return Err(FunctionError::ContinueOutsideOfLoop);
                    }
                    finished = true;
                }
                S::Return { value } => {
                    if !context.abilities.contains(ControlFlowAbility::RETURN) {
                        return Err(FunctionError::InvalidReturnSpot);
                    }
                    let value_ty = value
                        .map(|expr| context.resolve_type(expr, &self.valid_expression_set))
                        .transpose()?;
                    let expected_ty = context.return_type.map(|ty| &context.types[ty].inner);
                    if value_ty != expected_ty {
                        log::error!(
                            "Returning {:?} where {:?} is expected",
                            value_ty,
                            expected_ty
                        );
                        return Err(FunctionError::InvalidReturnType(value));
                    }
                    finished = true;
                }
                S::Kill => {
                    finished = true;
                }
                S::Barrier(_) => {
                    stages &= ShaderStages::COMPUTE;
                }
                S::Store { pointer, value } => {
                    let mut current = pointer;
                    loop {
                        let _ = context.resolve_pointer_type(current)?;
                        match *context.get_expression(current)? {
                            crate::Expression::Access { base, .. }
                            | crate::Expression::AccessIndex { base, .. } => current = base,
                            crate::Expression::LocalVariable(_)
                            | crate::Expression::GlobalVariable(_)
                            | crate::Expression::FunctionArgument(_) => break,
                            _ => return Err(FunctionError::InvalidStorePointer(current)),
                        }
                    }

                    let value_ty = context.resolve_type(value, &self.valid_expression_set)?;
                    match *value_ty {
                        Ti::Image { .. } | Ti::Sampler { .. } => {
                            return Err(FunctionError::InvalidStoreValue(value));
                        }
                        _ => {}
                    }
                    let good = match *context.resolve_pointer_type(pointer)? {
                        Ti::Pointer { base, class: _ } => match context.types[base].inner {
                            Ti::Atomic { kind, width } => *value_ty == Ti::Scalar { kind, width },
                            ref other => value_ty == other,
                        },
                        Ti::ValuePointer {
                            size: Some(size),
                            kind,
                            width,
                            class: _,
                        } => *value_ty == Ti::Vector { size, kind, width },
                        Ti::ValuePointer {
                            size: None,
                            kind,
                            width,
                            class: _,
                        } => *value_ty == Ti::Scalar { kind, width },
                        _ => false,
                    };
                    if !good {
                        return Err(FunctionError::InvalidStoreTypes { pointer, value });
                    }
                }
                S::ImageStore {
                    image,
                    coordinate,
                    array_index,
                    value,
                } => {
                    //Note: this code uses a lot of `FunctionError::InvalidImageStore`,
                    // and could probably be refactored.
                    let var = match *context.get_expression(image)? {
                        crate::Expression::GlobalVariable(var_handle) => {
                            &context.global_vars[var_handle]
                        }
                        _ => {
                            return Err(FunctionError::InvalidImageStore(
                                ExpressionError::ExpectedGlobalVariable,
                            ))
                        }
                    };

                    let value_ty = match context.types[var.ty].inner {
                        Ti::Image {
                            class,
                            arrayed,
                            dim,
                        } => {
                            match context
                                .resolve_type(coordinate, &self.valid_expression_set)?
                                .image_storage_coordinates()
                            {
                                Some(coord_dim) if coord_dim == dim => {}
                                _ => {
                                    return Err(FunctionError::InvalidImageStore(
                                        ExpressionError::InvalidImageCoordinateType(
                                            dim, coordinate,
                                        ),
                                    ))
                                }
                            };
                            if arrayed != array_index.is_some() {
                                return Err(FunctionError::InvalidImageStore(
                                    ExpressionError::InvalidImageArrayIndex,
                                ));
                            }
                            if let Some(expr) = array_index {
                                match *context.resolve_type(expr, &self.valid_expression_set)? {
                                    Ti::Scalar {
                                        kind: crate::ScalarKind::Sint,
                                        width: _,
                                    } => {}
                                    _ => {
                                        return Err(FunctionError::InvalidImageStore(
                                            ExpressionError::InvalidImageArrayIndexType(expr),
                                        ))
                                    }
                                }
                            }
                            match class {
                                crate::ImageClass::Storage { format, .. } => {
                                    crate::TypeInner::Vector {
                                        kind: format.into(),
                                        size: crate::VectorSize::Quad,
                                        width: 4,
                                    }
                                }
                                _ => {
                                    return Err(FunctionError::InvalidImageStore(
                                        ExpressionError::InvalidImageClass(class),
                                    ))
                                }
                            }
                        }
                        _ => {
                            return Err(FunctionError::InvalidImageStore(
                                ExpressionError::ExpectedImageType(var.ty),
                            ))
                        }
                    };

                    if *context.resolve_type(value, &self.valid_expression_set)? != value_ty {
                        return Err(FunctionError::InvalidStoreValue(value));
                    }
                }
                S::Call {
                    function,
                    ref arguments,
                    result,
                } => match self.validate_call(function, arguments, result, context) {
                    Ok(callee_stages) => stages &= callee_stages,
                    Err(error) => return Err(FunctionError::InvalidCall { function, error }),
                },
                S::Atomic {
                    pointer,
                    ref fun,
                    value,
                    result,
                } => {
                    self.validate_atomic(pointer, fun, value, result, context)?;
                }
            }
        }
        Ok(stages)
    }

    fn validate_block(
        &mut self,
        statements: &[crate::Statement],
        context: &BlockContext,
    ) -> Result<ShaderStages, FunctionError> {
        let base_expression_count = self.valid_expression_list.len();
        let stages = self.validate_block_impl(statements, context)?;
        for handle in self.valid_expression_list.drain(base_expression_count..) {
            self.valid_expression_set.remove(handle.index());
        }
        Ok(stages)
    }

    fn validate_local_var(
        &self,
        var: &crate::LocalVariable,
        types: &Arena<crate::Type>,
        constants: &Arena<crate::Constant>,
    ) -> Result<(), LocalVariableError> {
        log::debug!("var {:?}", var);
        if !self.types[var.ty.index()]
            .flags
            .contains(TypeFlags::DATA | TypeFlags::SIZED)
        {
            return Err(LocalVariableError::InvalidType(var.ty));
        }
        if let Some(const_handle) = var.init {
            match constants[const_handle].inner {
                crate::ConstantInner::Scalar { width, ref value } => {
                    let ty_inner = crate::TypeInner::Scalar {
                        width,
                        kind: value.scalar_kind(),
                    };
                    if types[var.ty].inner != ty_inner {
                        return Err(LocalVariableError::InitializerType);
                    }
                }
                crate::ConstantInner::Composite { ty, components: _ } => {
                    if ty != var.ty {
                        return Err(LocalVariableError::InitializerType);
                    }
                }
            }
        }
        Ok(())
    }

    pub(super) fn validate_function(
        &mut self,
        fun: &crate::Function,
        module: &crate::Module,
        mod_info: &ModuleInfo,
    ) -> Result<FunctionInfo, FunctionError> {
        let mut info = mod_info.process_function(fun, module, self.flags)?;

        for (var_handle, var) in fun.local_variables.iter() {
            self.validate_local_var(var, &module.types, &module.constants)
                .map_err(|error| FunctionError::LocalVariable {
                    handle: var_handle,
                    name: var.name.clone().unwrap_or_default(),
                    error,
                })?;
        }

        for (index, argument) in fun.arguments.iter().enumerate() {
            if !self.types[argument.ty.index()]
                .flags
                .contains(TypeFlags::ARGUMENT)
            {
                return Err(FunctionError::InvalidArgumentType {
                    index,
                    name: argument.name.clone().unwrap_or_default(),
                });
            }
        }

        self.valid_expression_set.clear();
        self.valid_expression_list.clear();
        for (handle, expr) in fun.expressions.iter() {
            if expr.needs_pre_emit() {
                self.valid_expression_set.insert(handle.index());
            }
            if self.flags.contains(ValidationFlags::EXPRESSIONS) {
                match self.validate_expression(
                    handle,
                    expr,
                    fun,
                    module,
                    &info,
                    &mod_info.functions,
                ) {
                    Ok(stages) => info.available_stages &= stages,
                    Err(error) => return Err(FunctionError::Expression { handle, error }),
                }
            }
        }

        if self.flags.contains(ValidationFlags::BLOCKS) {
            let stages = self.validate_block(
                &fun.body,
                &BlockContext::new(fun, module, &info, &mod_info.functions),
            )?;
            info.available_stages &= stages;
        }
        Ok(info)
    }
}
