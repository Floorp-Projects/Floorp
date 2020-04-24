use crate::declaration_kind::DeclarationKind;
use crate::early_errors::*;
use crate::error::{ParseError, Result};
use crate::Token;
use ast::{
    arena,
    source_atom_set::{CommonSourceAtomSetIndices, SourceAtomSet, SourceAtomSetIndex},
    source_location_accessor::SourceLocationAccessor,
    source_slice_list::SourceSliceList,
    types::*,
    SourceLocation,
};
use bumpalo::{vec, Bump};
use std::cell::RefCell;
use std::rc::Rc;

// The kind of BindingIdentifier found while parsing.
//
// Given we don't yet have the context stack, at the point of finding
// a BindingIdentifier, we don't know what kind of binding it is.
// So it's marked as `Unknown`.
//
// Once the parser reaches the end of a declaration, bindings found in the
// range are marked as corresponding kind.
//
// This is a separate enum than `DeclarationKind` for the following reason:
//   * `DeclarationKind` is determined only when the parser reaches the end of
//     the entire context (also because we don't have context stack), not each
//     declaration
//   * As long as `BindingKind` is known for each binding, we can map it to
//     `DeclarationKind`
//   * `DeclarationKind::CatchParameter` and some others don't to be marked
//     this way, because `AstBuilder` knows where they are in the `bindings`
//     vector.
#[derive(Debug, PartialEq, Clone, Copy)]
enum BindingKind {
    // The initial state.
    // BindingIdentifier is found, and haven't yet marked as any kind.
    Unknown,

    // BindingIdentifier is inside VariableDeclaration.
    Var,

    // BindingIdentifier is the name of FunctionDeclaration.
    Function,

    // BindingIdentifier is the name of GeneratorDeclaration,
    // AsyncFunctionDeclaration, or AsyncGeneratorDeclaration.
    AsyncOrGenerator,

    // BindingIdentifier is inside LexicalDeclaration with let.
    Let,

    // BindingIdentifier is inside LexicalDeclaration with const.
    Const,

    // BindingIdentifier is the name of ClassDeclaration.
    Class,

    // BindingIdentifier is the name of LabelIdentifier.
    // Only used to track which labels have been seen for duplicate labels. See
    // 'on_label_identifier' for more information
    Label,
}

#[derive(Debug, PartialEq, Clone, Copy)]
struct BindingInfo {
    name: SourceAtomSetIndex,
    // The offset of the BindingIdentifier in the source.
    offset: usize,
    kind: BindingKind,
}

pub struct AstBuilder<'alloc> {
    pub allocator: &'alloc Bump,

    // The stack of information about BindingIdentifier.
    //
    // When the parser found BindingIdentifier, the information (`name` and
    // `offset`) are noted to this vector, and when the parser determined what
    // kind of binding it is, `kind` is updated.
    //
    // The bindings are sorted by offset.
    //
    // When the parser reaches the end of a scope, all bindings declared within
    // that scope (not including nested scopes) are fed to an
    // EarlyErrorsContext to detect Early Errors.
    //
    // When leaving a context that is not one of script/module/function,
    // lexical items (`kind != BindingKind::Var && kind != BindingKind::Label`)
    // in the corresponding range are removed, while non-lexical items
    // (`kind == BindingKind::Var || kind == BindingKind::Label`) are
    // left there, so that VariableDeclarations and labels are propagated to the
    // enclosing context.
    //
    // FIXME: Once the context stack gets implemented, this structure and
    //        related methods should be removed and each declaration should be
    //        fed directly to EarlyErrorsContext.
    bindings: Vec<BindingInfo>,

    // Track continues and breaks without the use of a context stack.
    //
    // FIXME: Once th context stack geets implmentd, this structure and
    //        related metehods should be removed, and each break/continue should be
    //        fed directly to EarlyErrorsContext.
    breaks_and_continues: Vec<ControlInfo>,

    atoms: Rc<RefCell<SourceAtomSet<'alloc>>>,

    slices: Rc<RefCell<SourceSliceList<'alloc>>>,
}

pub trait AstBuilderDelegate<'alloc> {
    fn ast_builder_refmut(&mut self) -> &mut AstBuilder<'alloc>;
}

impl<'alloc> AstBuilder<'alloc> {
    pub fn new(
        allocator: &'alloc Bump,
        atoms: Rc<RefCell<SourceAtomSet<'alloc>>>,
        slices: Rc<RefCell<SourceSliceList<'alloc>>>,
    ) -> Self {
        Self {
            allocator,
            bindings: Vec::new(),
            breaks_and_continues: Vec::new(),
            atoms,
            slices,
        }
    }

    pub fn alloc<T>(&self, value: T) -> arena::Box<'alloc, T> {
        arena::alloc(self.allocator, value)
    }

    pub fn alloc_with<F, T>(&self, gen: F) -> arena::Box<'alloc, T>
    where
        F: FnOnce() -> T,
    {
        arena::alloc_with(self.allocator, gen)
    }

    pub fn alloc_str(&self, s: &str) -> &'alloc str {
        arena::alloc_str(self.allocator, s)
    }

    fn new_vec<T>(&self) -> arena::Vec<'alloc, T> {
        arena::Vec::new_in(self.allocator)
    }

    fn new_vec_single<T>(&self, value: T) -> arena::Vec<'alloc, T> {
        vec![in self.allocator; value]
    }

    fn collect_vec_from_results<T, C>(&self, results: C) -> Result<'alloc, arena::Vec<'alloc, T>>
    where
        C: IntoIterator<Item = Result<'alloc, T>>,
    {
        let mut out = self.new_vec();
        for result in results {
            out.push(result?);
        }
        Ok(out)
    }

    fn push<T>(&self, list: &mut arena::Vec<'alloc, T>, value: T) {
        list.push(value);
    }

    fn append<T>(&self, list: &mut arena::Vec<'alloc, T>, elements: &mut arena::Vec<'alloc, T>) {
        list.append(elements);
    }

    // IdentifierReference : Identifier
    pub fn identifier_reference(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Identifier>> {
        self.on_identifier_reference(&token)?;
        Ok(self.alloc_with(|| self.identifier(token)))
    }

    // BindingIdentifier : Identifier
    pub fn binding_identifier(
        &mut self,
        token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, BindingIdentifier>> {
        self.on_binding_identifier(&token)?;
        let loc = token.loc;
        Ok(self.alloc_with(|| BindingIdentifier {
            name: self.identifier(token),
            loc,
        }))
    }

    // BindingIdentifier : `yield`
    pub fn binding_identifier_yield(
        &mut self,
        token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, BindingIdentifier>> {
        self.on_binding_identifier(&token)?;
        let loc = token.loc;
        Ok(self.alloc_with(|| BindingIdentifier {
            name: Identifier {
                value: CommonSourceAtomSetIndices::yield_(),
                loc,
            },
            loc,
        }))
    }

    // BindingIdentifier : `await`
    pub fn binding_identifier_await(
        &mut self,
        token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, BindingIdentifier>> {
        self.on_binding_identifier(&token)?;
        let loc = token.loc;
        Ok(self.alloc_with(|| BindingIdentifier {
            name: Identifier {
                value: CommonSourceAtomSetIndices::await_(),
                loc,
            },
            loc,
        }))
    }

    // LabelIdentifier : Identifier
    pub fn label_identifier(
        &mut self,
        token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Label>> {
        self.on_label_identifier(&token)?;
        let loc = token.loc;
        Ok(self.alloc_with(|| Label {
            value: token.value.as_atom(),
            loc,
        }))
    }

    // PrimaryExpression : `this`
    pub fn this_expr(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let loc = token.loc;
        self.alloc_with(|| Expression::ThisExpression { loc })
    }

    // PrimaryExpression : IdentifierReference
    pub fn identifier_expr(
        &self,
        name: arena::Box<'alloc, Identifier>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let loc = name.loc;
        self.alloc_with(|| {
            Expression::IdentifierExpression(IdentifierExpression {
                name: name.unbox(),
                loc,
            })
        })
    }

    // PrimaryExpression : RegularExpressionLiteral
    pub fn regexp_literal(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let source = self.slices.borrow().get(token.value.as_slice());

        debug_assert!(source.chars().nth(0).unwrap() == '/');

        let end = source.rfind('/').unwrap();

        let pattern = self.slices.borrow_mut().push(&source[1..end]);
        let flags = &source[end + 1..];

        let mut global: bool = false;
        let mut ignore_case: bool = false;
        let mut multi_line: bool = false;
        let mut dot_all: bool = false;
        let mut unicode: bool = false;
        let mut sticky: bool = false;
        for c in flags.chars() {
            if c == 'g' {
                global = true;
            }
            if c == 'i' {
                ignore_case = true;
            }
            if c == 'm' {
                multi_line = true;
            }
            if c == 's' {
                dot_all = true;
            }
            if c == 'u' {
                unicode = true;
            }
            if c == 'y' {
                sticky = true;
            }
        }

        let loc = token.loc;
        self.alloc_with(|| Expression::LiteralRegExpExpression {
            pattern,
            global,
            ignore_case,
            multi_line,
            dot_all,
            sticky,
            unicode,
            loc,
        })
    }

    // PrimaryExpression : TemplateLiteral
    pub fn untagged_template_expr(
        &self,
        template_literal: arena::Box<'alloc, TemplateExpression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        self.alloc_with(|| Expression::TemplateExpression(template_literal.unbox()))
    }

    // PrimaryExpression : CoverParenthesizedExpressionAndArrowParameterList
    pub fn uncover_parenthesized_expression(
        &self,
        parenthesized: arena::Box<'alloc, CoverParenthesized<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Expression<'alloc>>> {
        match parenthesized.unbox() {
            CoverParenthesized::Expression { expression, .. } => {
                // TODO - does this need to rewalk the expression to look for
                // invalid ObjectPattern or ArrayPattern syntax?
                Ok(expression)
            }
            CoverParenthesized::Parameters(_parameters) => Err(ParseError::NotImplemented(
                "parenthesized expression with `...` should be a syntax error",
            )),
        }
    }

    // CoverParenthesizedExpressionAndArrowParameterList : `(` Expression `)`
    pub fn cover_parenthesized_expression(
        &self,
        open_token: arena::Box<'alloc, Token>,
        expression: arena::Box<'alloc, Expression<'alloc>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, CoverParenthesized<'alloc>> {
        self.alloc_with(|| CoverParenthesized::Expression {
            expression,
            loc: SourceLocation::from_parts(open_token.loc, close_token.loc),
        })
    }

    // CoverParenthesizedExpressionAndArrowParameterList : `(` `)`
    pub fn empty_parameter_list(&self) -> arena::Vec<'alloc, Parameter<'alloc>> {
        self.new_vec()
    }

    /// Used when parsing `([a, b=2]=arr) =>` to reinterpret as parameter bindings
    /// the snippets `a` and `b=2`, which were previously parsed as assignment targets.
    fn assignment_target_maybe_default_to_binding(
        &self,
        target: AssignmentTargetMaybeDefault<'alloc>,
    ) -> Result<'alloc, Parameter<'alloc>> {
        match target {
            AssignmentTargetMaybeDefault::AssignmentTarget(target) => Ok(Parameter::Binding(
                self.assignment_target_to_binding(target)?,
            )),

            AssignmentTargetMaybeDefault::AssignmentTargetWithDefault(
                AssignmentTargetWithDefault { binding, init, loc },
            ) => Ok(Parameter::BindingWithDefault(BindingWithDefault {
                binding: self.assignment_target_to_binding(binding)?,
                init,
                loc,
            })),
        }
    }

    fn assignment_target_property_to_binding_property(
        &self,
        target: AssignmentTargetProperty<'alloc>,
    ) -> Result<'alloc, BindingProperty<'alloc>> {
        Ok(match target {
            AssignmentTargetProperty::AssignmentTargetPropertyIdentifier(
                AssignmentTargetPropertyIdentifier {
                    binding: AssignmentTargetIdentifier { name, loc },
                    init,
                    loc: loc2,
                },
            ) => BindingProperty::BindingPropertyIdentifier(BindingPropertyIdentifier {
                binding: BindingIdentifier { name, loc },
                init,
                loc: loc2,
            }),

            AssignmentTargetProperty::AssignmentTargetPropertyProperty(
                AssignmentTargetPropertyProperty { name, binding, loc },
            ) => BindingProperty::BindingPropertyProperty(BindingPropertyProperty {
                name,
                binding: self.assignment_target_maybe_default_to_binding(binding)?,
                loc,
            }),
        })
    }

    /// Refine an AssignmentRestProperty into a BindingRestProperty.
    fn assignment_rest_property_to_binding_identifier(
        &self,
        target: AssignmentTarget<'alloc>,
    ) -> Result<'alloc, arena::Box<'alloc, BindingIdentifier>> {
        match target {
            // ({...x} = dv) => {}
            AssignmentTarget::SimpleAssignmentTarget(
                SimpleAssignmentTarget::AssignmentTargetIdentifier(AssignmentTargetIdentifier {
                    name,
                    loc,
                }),
            ) => Ok(self.alloc_with(|| BindingIdentifier { name, loc })),

            // ({...x.y} = dv) => {}
            _ => Err(ParseError::ObjectBindingPatternWithInvalidRest),
        }
    }

    /// Refine the left-hand side of `=` to a parameter binding. The spec says:
    ///
    /// > When the production *ArrowParameters* :
    /// > *CoverParenthesizedExpressionAndArrowParameterList* is recognized,
    /// > the following grammar is used to refine the interpretation of
    /// > *CoverParenthesizedExpressionAndArrowParameterList*:
    /// >
    /// > *ArrowFormalParameters*\[Yield, Await\] :
    /// > `(` *UniqueFormalParameters*\[?Yield, ?Await\] `)`
    ///
    /// Of course, rather than actually reparsing the arrow function parameters,
    /// we work by refining the AST we already built.
    ///
    /// When parsing `(a = 1, [b, c] = obj) => {}`, the assignment targets `a`
    /// and `[b, c]` are passed to this method.
    fn assignment_target_to_binding(
        &self,
        target: AssignmentTarget<'alloc>,
    ) -> Result<'alloc, Binding<'alloc>> {
        match target {
            // (a = dv) => {}
            AssignmentTarget::SimpleAssignmentTarget(
                SimpleAssignmentTarget::AssignmentTargetIdentifier(AssignmentTargetIdentifier {
                    name,
                    loc,
                }),
            ) => Ok(Binding::BindingIdentifier(BindingIdentifier { name, loc })),

            // This case is always an early SyntaxError.
            // (a.x = dv) => {}
            // (a[i] = dv) => {}
            AssignmentTarget::SimpleAssignmentTarget(
                SimpleAssignmentTarget::MemberAssignmentTarget(_),
            ) => Err(ParseError::InvalidParameter),

            // ([a, b] = dv) => {}
            AssignmentTarget::AssignmentTargetPattern(
                AssignmentTargetPattern::ArrayAssignmentTarget(ArrayAssignmentTarget {
                    elements,
                    rest,
                    loc,
                }),
            ) => {
                let elements: arena::Vec<'alloc, Option<AssignmentTargetMaybeDefault<'alloc>>> =
                    elements;
                let elements: arena::Vec<'alloc, Option<Parameter<'alloc>>> = self
                    .collect_vec_from_results(elements.into_iter().map(|maybe_target| {
                        maybe_target
                            .map(|target| self.assignment_target_maybe_default_to_binding(target))
                            .transpose()
                    }))?;
                let rest: Option<Result<'alloc, arena::Box<'alloc, Binding<'alloc>>>> = rest.map(
                    |rest_target| -> Result<'alloc, arena::Box<'alloc, Binding<'alloc>>> {
                        Ok(self.alloc(self.assignment_target_to_binding(rest_target.unbox())?))
                    },
                );
                let rest: Option<arena::Box<'alloc, Binding<'alloc>>> = rest.transpose()?;
                Ok(Binding::BindingPattern(BindingPattern::ArrayBinding(
                    ArrayBinding {
                        elements,
                        rest,
                        loc,
                    },
                )))
            }

            // ({a, b: c} = dv) => {}
            AssignmentTarget::AssignmentTargetPattern(
                AssignmentTargetPattern::ObjectAssignmentTarget(ObjectAssignmentTarget {
                    properties,
                    rest,
                    loc,
                }),
            ) => {
                let properties =
                    self.collect_vec_from_results(properties.into_iter().map(|target| {
                        self.assignment_target_property_to_binding_property(target)
                    }))?;

                let rest = if let Some(rest_target) = rest {
                    Some(self.assignment_rest_property_to_binding_identifier(rest_target.unbox())?)
                } else {
                    None
                };
                Ok(Binding::BindingPattern(BindingPattern::ObjectBinding(
                    ObjectBinding {
                        properties,
                        rest,
                        loc,
                    },
                )))
            }
        }
    }

    fn object_property_to_binding_property(
        &self,
        op: ObjectProperty<'alloc>,
    ) -> Result<'alloc, BindingProperty<'alloc>> {
        match op {
            ObjectProperty::NamedObjectProperty(NamedObjectProperty::DataProperty(
                DataProperty {
                    property_name,
                    expression,
                    loc,
                },
            )) => Ok(BindingProperty::BindingPropertyProperty(
                BindingPropertyProperty {
                    name: property_name,
                    binding: self.expression_to_parameter(expression.unbox())?,
                    loc,
                },
            )),

            ObjectProperty::NamedObjectProperty(NamedObjectProperty::MethodDefinition(_)) => {
                Err(ParseError::ObjectPatternWithMethod)
            }

            ObjectProperty::ShorthandProperty(ShorthandProperty {
                name: IdentifierExpression { name, loc },
                ..
            }) => {
                // TODO - CoverInitializedName can't be represented in an
                // ObjectProperty, but we need it here.
                Ok(BindingProperty::BindingPropertyIdentifier(
                    BindingPropertyIdentifier {
                        binding: BindingIdentifier { name, loc },
                        init: None,
                        loc,
                    },
                ))
            }

            ObjectProperty::SpreadProperty(_expression) => {
                Err(ParseError::ObjectPatternWithNonFinalRest)
            }
        }
    }

    /// Refine an instance of "*PropertyDefinition* : `...`
    /// *AssignmentExpression*" into a *BindingRestProperty*.
    fn spread_expression_to_rest_binding(
        &self,
        expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, BindingIdentifier>> {
        Ok(match expression.unbox() {
            Expression::IdentifierExpression(IdentifierExpression { name, loc }) => {
                self.alloc_with(|| BindingIdentifier { name, loc })
            }
            _ => {
                return Err(ParseError::ObjectBindingPatternWithInvalidRest);
            }
        })
    }

    fn pop_trailing_spread_property(
        &self,
        properties: &mut arena::Vec<'alloc, arena::Box<'alloc, ObjectProperty<'alloc>>>,
    ) -> Option<arena::Box<'alloc, Expression<'alloc>>> {
        // Check whether we want to pop a PropertyDefinition
        match properties.last().map(|boxed| &**boxed) {
            Some(ObjectProperty::SpreadProperty(_)) => {}
            _ => return None,
        }

        // We do.
        match properties.pop().unwrap().unbox() {
            ObjectProperty::SpreadProperty(expression) => Some(expression),
            _ => panic!("bug"), // can't happen: we just checked this above
        }
    }

    /// Refine an *ObjectLiteral* into an *ObjectBindingPattern*.
    fn object_expression_to_object_binding(
        &self,
        object: ObjectExpression<'alloc>,
    ) -> Result<'alloc, ObjectBinding<'alloc>> {
        let mut properties = object.properties;
        let loc = object.loc;
        let rest = self.pop_trailing_spread_property(&mut properties);
        Ok(ObjectBinding {
            properties: self.collect_vec_from_results(
                properties
                    .into_iter()
                    .map(|prop| self.object_property_to_binding_property(prop.unbox())),
            )?,
            rest: rest
                .map(|expression| self.spread_expression_to_rest_binding(expression))
                .transpose()?,
            loc,
        })
    }

    fn array_elements_to_parameters(
        &self,
        elements: arena::Vec<'alloc, ArrayExpressionElement<'alloc>>,
    ) -> Result<'alloc, arena::Vec<'alloc, Option<Parameter<'alloc>>>> {
        self.collect_vec_from_results(elements.into_iter().map(|element| match element {
                ArrayExpressionElement::Expression(expr) =>
                    Ok(Some(self.expression_to_parameter(expr.unbox())?)),
                ArrayExpressionElement::SpreadElement(_expr) =>
                    // ([...a, b]) => {}
                    Err(ParseError::ArrayPatternWithNonFinalRest),
                ArrayExpressionElement::Elision { .. } => Ok(None),
            }))
    }

    fn pop_trailing_spread_element(
        &self,
        elements: &mut arena::Vec<'alloc, ArrayExpressionElement<'alloc>>,
    ) -> Option<arena::Box<'alloc, Expression<'alloc>>> {
        // Check whether we want to pop an element.
        match elements.last() {
            Some(ArrayExpressionElement::SpreadElement(_)) => {}
            _ => return None,
        }

        // We do.
        match elements.pop() {
            Some(ArrayExpressionElement::SpreadElement(expression)) => Some(expression),
            _ => panic!("bug"), // can't happen: we just checked this above
        }
    }

    fn expression_to_binding_no_default(
        &self,
        expression: Expression<'alloc>,
    ) -> Result<'alloc, Binding<'alloc>> {
        match expression {
            Expression::IdentifierExpression(IdentifierExpression { name, loc }) => {
                Ok(Binding::BindingIdentifier(BindingIdentifier { name, loc }))
            }

            Expression::ArrayExpression(ArrayExpression { mut elements, loc }) => {
                let rest = self.pop_trailing_spread_element(&mut elements);
                let elements = self.array_elements_to_parameters(elements)?;
                let rest = rest
                    .map(|expr| match self.expression_to_parameter(expr.unbox())? {
                        Parameter::Binding(b) => Ok(self.alloc_with(|| b)),
                        Parameter::BindingWithDefault(_) => {
                            Err(ParseError::ArrayBindingPatternWithInvalidRest)
                        }
                    })
                    .transpose()?;
                Ok(Binding::BindingPattern(BindingPattern::ArrayBinding(
                    ArrayBinding {
                        elements,
                        rest,
                        loc,
                    },
                )))
            }

            Expression::ObjectExpression(object) => Ok(Binding::BindingPattern(
                BindingPattern::ObjectBinding(self.object_expression_to_object_binding(object)?),
            )),

            _ => Err(ParseError::InvalidParameter),
        }
    }

    fn expression_to_parameter(
        &self,
        expression: Expression<'alloc>,
    ) -> Result<'alloc, Parameter<'alloc>> {
        match expression {
            Expression::AssignmentExpression {
                binding,
                expression,
                loc,
            } => Ok(Parameter::BindingWithDefault(BindingWithDefault {
                binding: self.assignment_target_to_binding(binding)?,
                init: expression,
                loc,
            })),

            other => Ok(Parameter::Binding(
                self.expression_to_binding_no_default(other)?,
            )),
        }
    }

    // CoverParenthesizedExpressionAndArrowParameterList : `(` Expression `,` `)`
    // CoverParenthesizedExpressionAndArrowParameterList : `(` Expression `,` `...` BindingIdentifier `)`
    // CoverParenthesizedExpressionAndArrowParameterList : `(` Expression `,` `...` BindingPattern `)`
    pub fn expression_to_parameter_list(
        &self,
        expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, arena::Vec<'alloc, Parameter<'alloc>>> {
        // When the production
        // *ArrowParameters* `:` *CoverParenthesizedExpressionAndArrowParameterList*
        // is recognized the following grammar is used to refine the
        // interpretation of
        // *CoverParenthesizedExpressionAndArrowParameterList*:
        //
        //     ArrowFormalParameters[Yield, Await]:
        //         `(` UniqueFormalParameters[?Yield, ?Await] `)`
        match expression.unbox() {
            Expression::BinaryExpression {
                operator: BinaryOperator::Comma { .. },
                left,
                right,
                ..
            } => {
                let mut parameters = self.expression_to_parameter_list(left)?;
                self.push(
                    &mut parameters,
                    self.expression_to_parameter(right.unbox())?,
                );
                Ok(parameters)
            }
            other => Ok(self.new_vec_single(self.expression_to_parameter(other)?)),
        }
    }

    /// Used to convert `async(x, y, ...z)` from a *CallExpression* to async
    /// arrow function parameters.
    fn arguments_to_parameter_list(
        &self,
        arguments: Arguments<'alloc>,
    ) -> Result<'alloc, arena::Box<'alloc, FormalParameters<'alloc>>> {
        let loc = arguments.loc;
        let mut items = self.new_vec();
        let mut rest: Option<Binding<'alloc>> = None;
        for arg in arguments.args {
            if rest.is_some() {
                return Err(ParseError::ArrowParametersWithNonFinalRest);
            }
            match arg {
                Argument::Expression(expr) => {
                    self.push(&mut items, self.expression_to_parameter(expr.unbox())?);
                }
                Argument::SpreadElement(spread_expr) => {
                    rest = Some(self.expression_to_binding_no_default(spread_expr.unbox())?);
                }
            }
        }
        Ok(self.alloc_with(|| FormalParameters { items, rest, loc }))
    }

    // CoverParenthesizedExpressionAndArrowParameterList : `(` `)`
    // CoverParenthesizedExpressionAndArrowParameterList : `(` `...` BindingIdentifier `)`
    // CoverParenthesizedExpressionAndArrowParameterList : `(` `...` BindingPattern `)`
    // CoverParenthesizedExpressionAndArrowParameterList : `(` Expression `,` `...` BindingIdentifier `)`
    // CoverParenthesizedExpressionAndArrowParameterList : `(` Expression `,` `...` BindingPattern `)`
    pub fn cover_arrow_parameter_list(
        &self,
        open_token: arena::Box<'alloc, Token>,
        parameters: arena::Vec<'alloc, Parameter<'alloc>>,
        rest: Option<arena::Box<'alloc, Binding<'alloc>>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, CoverParenthesized<'alloc>> {
        self.alloc_with(|| {
            CoverParenthesized::Parameters(self.alloc_with(|| FormalParameters {
                items: parameters,
                rest: rest.map(|boxed| boxed.unbox()),
                loc: SourceLocation::from_parts(open_token.loc, close_token.loc),
            }))
        })
    }

    // Literal : NullLiteral
    pub fn null_literal(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let loc = token.loc;
        self.alloc_with(|| Expression::LiteralNullExpression { loc })
    }

    // Literal : BooleanLiteral
    pub fn boolean_literal(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let loc = token.loc;
        let s = token.value.as_atom();
        assert!(
            s == CommonSourceAtomSetIndices::true_() || s == CommonSourceAtomSetIndices::false_()
        );

        self.alloc_with(|| Expression::LiteralBooleanExpression {
            value: s == CommonSourceAtomSetIndices::true_(),
            loc,
        })
    }

    fn numeric_literal_value(token: arena::Box<'alloc, Token>) -> f64 {
        token.unbox().value.as_number()
    }

    // Literal : NumericLiteral
    pub fn numeric_literal(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Expression<'alloc>>> {
        let loc = token.loc;
        Ok(self.alloc_with(|| {
            Expression::LiteralNumericExpression(NumericLiteral {
                value: Self::numeric_literal_value(token),
                loc,
            })
        }))
    }

    // Literal : NumericLiteral
    //
    // where NumericLiteral is either:
    //   * DecimalBigIntegerLiteral
    //   * NonDecimalIntegerLiteralBigIntLiteralSuffix
    pub fn bigint_literal(
        &self,
        _token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Expression<'alloc>>> {
        Err(ParseError::NotImplemented("BigInt"))
    }

    // Literal : StringLiteral
    pub fn string_literal(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Expression<'alloc>>> {
        let loc = token.loc;
        // Hack: Prevent emission for scripts with "use strict"
        // directive.
        let value = token.value.as_atom();
        if value == CommonSourceAtomSetIndices::use_strict() {
            return Err(ParseError::NotImplemented("use strict directive"));
        }

        Ok(self.alloc_with(|| Expression::LiteralStringExpression { value, loc }))
    }

    // ArrayLiteral : `[` Elision? `]`
    pub fn array_literal_empty(
        &self,
        open_token: arena::Box<'alloc, Token>,
        elision: Option<arena::Box<'alloc, ArrayExpression<'alloc>>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        self.alloc_with(|| {
            Expression::ArrayExpression(match elision {
                None => ArrayExpression {
                    elements: self.new_vec(),
                    loc: SourceLocation::from_parts(open_token.loc, close_token.loc),
                },
                Some(mut array) => {
                    array.loc.set_range(open_token.loc, close_token.loc);
                    array.unbox()
                }
            })
        })
    }

    // ArrayLiteral : `[` ElementList `]`
    pub fn array_literal(
        &self,
        open_token: arena::Box<'alloc, Token>,
        mut array: arena::Box<'alloc, ArrayExpression<'alloc>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        array.loc.set_range(open_token.loc, close_token.loc);
        self.alloc_with(|| Expression::ArrayExpression(array.unbox()))
    }

    // ArrayLiteral : `[` ElementList `,` Elision? `]`
    pub fn array_literal_with_trailing_elision(
        &self,
        open_token: arena::Box<'alloc, Token>,
        mut array: arena::Box<'alloc, ArrayExpression<'alloc>>,
        elision: Option<arena::Box<'alloc, ArrayExpression<'alloc>>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        if let Some(mut more) = elision {
            self.append(&mut array.elements, &mut more.elements);
        }
        array.loc.set_range(open_token.loc, close_token.loc);
        self.alloc_with(|| Expression::ArrayExpression(array.unbox()))
    }

    // ElementList : Elision? AssignmentExpression
    pub fn element_list_first(
        &self,
        elision: Option<arena::Box<'alloc, ArrayExpression<'alloc>>>,
        element: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, ArrayExpression<'alloc>> {
        let mut array = elision.unwrap_or_else(|| {
            self.alloc_with(|| ArrayExpression {
                elements: self.new_vec(),
                // This will be overwritten once the enclosing array gets
                // parsed.
                loc: SourceLocation::default(),
            })
        });
        self.push(
            &mut array.elements,
            ArrayExpressionElement::Expression(element),
        );
        array
    }

    // ElementList : Elision? SpreadElement
    pub fn element_list_first_spread(
        &self,
        elision: Option<arena::Box<'alloc, ArrayExpression<'alloc>>>,
        spread_element: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, ArrayExpression<'alloc>> {
        let mut array = elision.unwrap_or_else(|| {
            self.alloc_with(|| ArrayExpression {
                elements: self.new_vec(),
                // This will be overwritten once the enclosing array gets
                // parsed.
                loc: SourceLocation::default(),
            })
        });
        self.push(
            &mut array.elements,
            ArrayExpressionElement::SpreadElement(spread_element),
        );
        array
    }

    // ElementList : ElementList `,` Elision? AssignmentExpression
    pub fn element_list_append(
        &self,
        mut array: arena::Box<'alloc, ArrayExpression<'alloc>>,
        elision: Option<arena::Box<'alloc, ArrayExpression<'alloc>>>,
        element: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, ArrayExpression<'alloc>> {
        if let Some(mut elision) = elision {
            self.append(&mut array.elements, &mut elision.elements);
        }
        self.push(
            &mut array.elements,
            ArrayExpressionElement::Expression(element),
        );
        array
    }

    // ElementList : ElementList `,` Elision? SpreadElement
    pub fn element_list_append_spread(
        &self,
        mut array: arena::Box<'alloc, ArrayExpression<'alloc>>,
        elision: Option<arena::Box<'alloc, ArrayExpression<'alloc>>>,
        spread_element: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, ArrayExpression<'alloc>> {
        if let Some(mut elision) = elision {
            self.append(&mut array.elements, &mut elision.elements);
        }
        self.push(
            &mut array.elements,
            ArrayExpressionElement::SpreadElement(spread_element),
        );
        array
    }

    // Elision : `,`
    pub fn elision_single(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, ArrayExpression<'alloc>> {
        let loc = token.loc;
        self.alloc_with(|| ArrayExpression {
            elements: self.new_vec_single(ArrayExpressionElement::Elision { loc }),
            // This will be overwritten once the enclosing array gets parsed.
            loc: SourceLocation::default(),
        })
    }

    // Elision : Elision `,`
    pub fn elision_append(
        &self,
        mut array: arena::Box<'alloc, ArrayExpression<'alloc>>,
        token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, ArrayExpression<'alloc>> {
        let loc = token.loc;
        self.push(&mut array.elements, ArrayExpressionElement::Elision { loc });
        array
    }

    // SpreadElement : `...` AssignmentExpression
    pub fn spread_element(
        &self,
        expr: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        expr
    }

    // ObjectLiteral : `{` `}`
    pub fn object_literal_empty(
        &self,
        open_token: arena::Box<'alloc, Token>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        self.alloc_with(|| {
            Expression::ObjectExpression(ObjectExpression {
                properties: self.new_vec(),
                loc: SourceLocation::from_parts(open_token.loc, close_token.loc),
            })
        })
    }

    // ObjectLiteral : `{` PropertyDefinitionList `}`
    // ObjectLiteral : `{` PropertyDefinitionList `,` `}`
    pub fn object_literal(
        &self,
        open_token: arena::Box<'alloc, Token>,
        mut object: arena::Box<'alloc, ObjectExpression<'alloc>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        object.loc.set_range(open_token.loc, close_token.loc);
        self.alloc_with(|| Expression::ObjectExpression(object.unbox()))
    }

    // PropertyDefinitionList : PropertyDefinition
    pub fn property_definition_list_single(
        &self,
        property: arena::Box<'alloc, ObjectProperty<'alloc>>,
    ) -> arena::Box<'alloc, ObjectExpression<'alloc>> {
        self.alloc_with(|| ObjectExpression {
            properties: self.new_vec_single(property),
            // This will be overwritten once the enclosing object gets parsed.
            loc: SourceLocation::default(),
        })
    }

    // PropertyDefinitionList : PropertyDefinitionList `,` PropertyDefinition
    pub fn property_definition_list_append(
        &self,
        mut object: arena::Box<'alloc, ObjectExpression<'alloc>>,
        property: arena::Box<'alloc, ObjectProperty<'alloc>>,
    ) -> arena::Box<'alloc, ObjectExpression<'alloc>> {
        self.push(&mut object.properties, property);
        object
    }

    // PropertyDefinition : IdentifierReference
    pub fn shorthand_property(
        &self,
        name: arena::Box<'alloc, Identifier>,
    ) -> arena::Box<'alloc, ObjectProperty<'alloc>> {
        let loc = name.loc;
        self.alloc_with(|| {
            ObjectProperty::ShorthandProperty(ShorthandProperty {
                name: IdentifierExpression {
                    name: name.unbox(),
                    loc,
                },
                loc,
            })
        })
    }

    // PropertyDefinition : PropertyName `:` AssignmentExpression
    pub fn property_definition(
        &self,
        name: arena::Box<'alloc, PropertyName<'alloc>>,
        expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, ObjectProperty<'alloc>> {
        let name_loc = name.get_loc();
        let expression_loc = expression.get_loc();
        self.alloc_with(|| {
            ObjectProperty::NamedObjectProperty(NamedObjectProperty::DataProperty(DataProperty {
                property_name: name.unbox(),
                expression,
                loc: SourceLocation::from_parts(name_loc, expression_loc),
            }))
        })
    }

    // PropertyDefinition : MethodDefinition
    pub fn property_definition_method(
        &self,
        method: arena::Box<'alloc, MethodDefinition<'alloc>>,
    ) -> arena::Box<'alloc, ObjectProperty<'alloc>> {
        self.alloc_with(|| {
            ObjectProperty::NamedObjectProperty(NamedObjectProperty::MethodDefinition(
                method.unbox(),
            ))
        })
    }

    // PropertyDefinition : `...` AssignmentExpression
    pub fn property_definition_spread(
        &self,
        spread: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, ObjectProperty<'alloc>> {
        self.alloc_with(|| ObjectProperty::SpreadProperty(spread))
    }

    // LiteralPropertyName : IdentifierName
    pub fn property_name_identifier(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, PropertyName<'alloc>>> {
        let value = token.value.as_atom();
        if value == CommonSourceAtomSetIndices::__proto__() {
            return Err(ParseError::NotImplemented("__proto__ as property name"));
        }

        let loc = token.loc;
        Ok(self.alloc_with(|| PropertyName::StaticPropertyName(StaticPropertyName { value, loc })))
    }

    // LiteralPropertyName : StringLiteral
    pub fn property_name_string(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, PropertyName<'alloc>>> {
        let value = token.value.as_atom();
        if value == CommonSourceAtomSetIndices::__proto__() {
            return Err(ParseError::NotImplemented("__proto__ as property name"));
        }

        let loc = token.loc;
        Ok(self.alloc_with(|| PropertyName::StaticPropertyName(StaticPropertyName { value, loc })))
    }

    // LiteralPropertyName : NumericLiteral
    pub fn property_name_numeric(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, PropertyName<'alloc>>> {
        let loc = token.loc;
        let value = Self::numeric_literal_value(token);
        Ok(self
            .alloc_with(|| PropertyName::StaticNumericPropertyName(NumericLiteral { value, loc })))
    }

    // LiteralPropertyName : NumericLiteral
    //
    // where NumericLiteral is either:
    //   * DecimalBigIntegerLiteral
    //   * NonDecimalIntegerLiteralBigIntLiteralSuffix
    pub fn property_name_bigint(
        &self,
        _token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, PropertyName<'alloc>>> {
        Err(ParseError::NotImplemented("BigInt"))
    }

    // ComputedPropertyName : `[` AssignmentExpression `]`
    pub fn computed_property_name(
        &self,
        open_token: arena::Box<'alloc, Token>,
        expression: arena::Box<'alloc, Expression<'alloc>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, PropertyName<'alloc>> {
        self.alloc_with(|| {
            PropertyName::ComputedPropertyName(ComputedPropertyName {
                expression,
                loc: SourceLocation::from_parts(open_token.loc, close_token.loc),
            })
        })
    }

    // CoverInitializedName : IdentifierReference Initializer
    pub fn cover_initialized_name(
        &self,
        _name: arena::Box<'alloc, Identifier>,
        _initializer: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, ObjectProperty<'alloc>>> {
        // Awkward. This needs to be stored somehow until we reach an enclosing
        // context where it can be reinterpreted as a default value in an
        // object destructuring assignment pattern.
        Err(ParseError::NotImplemented(
            "default initializers in object patterns",
        ))
    }

    // TemplateLiteral : NoSubstitutionTemplate
    pub fn template_literal(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, TemplateExpression<'alloc>> {
        let loc = token.loc;
        self.alloc_with(|| TemplateExpression {
            tag: None,
            elements: self.new_vec_single(TemplateExpressionElement::TemplateElement(
                TemplateElement {
                    raw_value: token.value.as_atom(),
                    loc,
                },
            )),
            loc,
        })
    }

    // SubstitutionTemplate : TemplateHead Expression TemplateSpans
    pub fn substitution_template(
        &self,
        _head: arena::Box<'alloc, Token>,
        _expression: arena::Box<'alloc, Expression<'alloc>>,
        _spans: arena::Box<'alloc, Void>,
    ) -> Result<'alloc, arena::Box<'alloc, TemplateExpression<'alloc>>> {
        Err(ParseError::NotImplemented("template strings"))
    }

    // TemplateSpans : TemplateTail
    // TemplateSpans : TemplateMiddleList TemplateTail
    pub fn template_spans(
        &self,
        _middle_list: Option<arena::Box<'alloc, Void>>,
        _tail: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("template strings"))
    }

    // TemplateMiddleList : TemplateMiddle Expression
    pub fn template_middle_list_single(
        &self,
        _middle: arena::Box<'alloc, Token>,
        _expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("template strings"))
    }

    // TemplateMiddleList : TemplateMiddleList TemplateMiddle Expression
    pub fn template_middle_list_append(
        &self,
        _middle_list: arena::Box<'alloc, Void>,
        _middle: arena::Box<'alloc, Token>,
        _expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("template strings"))
    }

    // MemberExpression : MemberExpression `[` Expression `]`
    // CallExpression : CallExpression `[` Expression `]`
    pub fn computed_member_expr(
        &self,
        object: arena::Box<'alloc, Expression<'alloc>>,
        expression: arena::Box<'alloc, Expression<'alloc>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let object_loc = object.get_loc();
        self.alloc_with(|| {
            Expression::MemberExpression(MemberExpression::ComputedMemberExpression(
                ComputedMemberExpression {
                    object: ExpressionOrSuper::Expression(object),
                    expression,
                    loc: SourceLocation::from_parts(object_loc, close_token.loc),
                },
            ))
        })
    }

    // OptionalExpression : MemberExpression OptionalChain
    // OptionalExpression : CallExpression OptionalChain
    // OptionalExpression : OptionalExpression OptionalChain
    pub fn optional_expr(
        &self,
        object: arena::Box<'alloc, Expression<'alloc>>,
        tail: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let object_loc = object.get_loc();
        let expression_loc = tail.get_loc();
        self.alloc_with(|| Expression::OptionalExpression {
            object: ExpressionOrSuper::Expression(object),
            tail,
            loc: SourceLocation::from_parts(object_loc, expression_loc),
        })
    }

    // OptionalChain : `?.` `[` Expression `]`
    pub fn optional_computed_member_expr_tail(
        &self,
        start_token: arena::Box<'alloc, Token>,
        expression: arena::Box<'alloc, Expression<'alloc>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        self.alloc_with(|| {
            Expression::OptionalChain(OptionalChain::ComputedMemberExpressionTail {
                expression,
                loc: SourceLocation::from_parts(start_token.loc, close_token.loc),
            })
        })
    }

    // OptionalChain : `?.` Expression
    pub fn optional_static_member_expr_tail(
        &self,
        start_token: arena::Box<'alloc, Token>,
        identifier_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let identifier_token_loc = identifier_token.loc;
        self.alloc_with(|| {
            Expression::OptionalChain(OptionalChain::StaticMemberExpressionTail {
                property: self.identifier_name(identifier_token),
                loc: SourceLocation::from_parts(start_token.loc, identifier_token_loc),
            })
        })
    }

    // OptionalChain : `?.` Arguments
    pub fn optional_call_expr_tail(
        &self,
        start_token: arena::Box<'alloc, Token>,
        arguments: arena::Box<'alloc, Arguments<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let arguments_loc = arguments.loc;
        self.alloc_with(|| {
            Expression::OptionalChain(OptionalChain::CallExpressionTail {
                arguments: arguments.unbox(),
                loc: SourceLocation::from_parts(start_token.loc, arguments_loc),
            })
        })
    }

    // OptionalChain : `?.` TemplateLiteral
    pub fn error_optional_chain_with_template(
        &self,
    ) -> Result<'alloc, arena::Box<'alloc, Expression<'alloc>>> {
        Err(ParseError::IllegalCharacter('`'))
    }

    // OptionalChain : OptionalChain `[` Expression `]`
    pub fn optional_computed_member_expr(
        &self,
        object: arena::Box<'alloc, Expression<'alloc>>,
        expression: arena::Box<'alloc, Expression<'alloc>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let object_loc = object.get_loc();
        self.alloc_with(|| {
            Expression::OptionalChain(OptionalChain::ComputedMemberExpression(
                ComputedMemberExpression {
                    object: ExpressionOrSuper::Expression(object),
                    expression,
                    loc: SourceLocation::from_parts(object_loc, close_token.loc),
                },
            ))
        })
    }

    // OptionalChain : OptionalChain `.` Expression
    pub fn optional_static_member_expr(
        &self,
        object: arena::Box<'alloc, Expression<'alloc>>,
        identifier_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let object_loc = object.get_loc();
        let identifier_token_loc = identifier_token.loc;
        self.alloc_with(|| {
            Expression::OptionalChain(OptionalChain::StaticMemberExpression(
                StaticMemberExpression {
                    object: ExpressionOrSuper::Expression(object),
                    property: self.identifier_name(identifier_token),
                    loc: SourceLocation::from_parts(object_loc, identifier_token_loc),
                },
            ))
        })
    }

    // OptionalChain : OptionalChain Arguments
    pub fn optional_call_expr(
        &self,
        callee: arena::Box<'alloc, Expression<'alloc>>,
        arguments: arena::Box<'alloc, Arguments<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let callee_loc = callee.get_loc();
        let arguments_loc = arguments.loc;
        self.alloc_with(|| {
            Expression::OptionalChain(OptionalChain::CallExpression(CallExpression {
                callee: ExpressionOrSuper::Expression(callee),
                arguments: arguments.unbox(),
                loc: SourceLocation::from_parts(callee_loc, arguments_loc),
            }))
        })
    }

    fn identifier(&self, token: arena::Box<'alloc, Token>) -> Identifier {
        Identifier {
            value: token.value.as_atom(),
            loc: token.loc,
        }
    }

    fn identifier_name(&self, token: arena::Box<'alloc, Token>) -> IdentifierName {
        IdentifierName {
            value: token.value.as_atom(),
            loc: token.loc,
        }
    }

    fn private_identifier(&self, token: arena::Box<'alloc, Token>) -> PrivateIdentifier {
        PrivateIdentifier {
            value: token.value.as_atom(),
            loc: token.loc,
        }
    }

    // MemberExpression : MemberExpression `.` IdentifierName
    // CallExpression : CallExpression `.` IdentifierName
    pub fn static_member_expr(
        &self,
        object: arena::Box<'alloc, Expression<'alloc>>,
        identifier_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let object_loc = object.get_loc();
        let identifier_token_loc = identifier_token.loc;
        self.alloc_with(|| {
            Expression::MemberExpression(MemberExpression::StaticMemberExpression(
                StaticMemberExpression {
                    object: ExpressionOrSuper::Expression(object),
                    property: self.identifier_name(identifier_token),
                    loc: SourceLocation::from_parts(object_loc, identifier_token_loc),
                },
            ))
        })
    }

    // MemberExpression : MemberExpression TemplateLiteral
    // CallExpression : CallExpression TemplateLiteral
    pub fn tagged_template_expr(
        &self,
        tag: arena::Box<'alloc, Expression<'alloc>>,
        mut template_literal: arena::Box<'alloc, TemplateExpression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        template_literal.tag = Some(tag);
        self.alloc_with(|| Expression::TemplateExpression(template_literal.unbox()))
    }

    // MemberExpression : `new` MemberExpression Arguments
    pub fn new_expr_with_arguments(
        &self,
        new_token: arena::Box<'alloc, Token>,
        callee: arena::Box<'alloc, Expression<'alloc>>,
        arguments: arena::Box<'alloc, Arguments<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let arguments_loc = arguments.loc;
        self.alloc_with(|| Expression::NewExpression {
            callee,
            arguments: arguments.unbox(),
            loc: SourceLocation::from_parts(new_token.loc, arguments_loc),
        })
    }

    // MemberExpression : MemberExpression `.` PrivateIdentifier
    pub fn private_field_expr(
        &self,
        object: arena::Box<'alloc, Expression<'alloc>>,
        private_identifier: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let object_loc = object.get_loc();
        let field_loc = private_identifier.loc;
        self.alloc_with(|| {
            Expression::MemberExpression(MemberExpression::PrivateFieldExpression(
                PrivateFieldExpression {
                    object,
                    field: self.private_identifier(private_identifier),
                    loc: SourceLocation::from_parts(object_loc, field_loc),
                },
            ))
        })
    }

    // SuperProperty : `super` `[` Expression `]`
    pub fn super_property_computed(
        &self,
        super_token: arena::Box<'alloc, Token>,
        expression: arena::Box<'alloc, Expression<'alloc>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let super_loc = super_token.loc;
        self.alloc_with(|| {
            Expression::MemberExpression(MemberExpression::ComputedMemberExpression(
                ComputedMemberExpression {
                    object: ExpressionOrSuper::Super { loc: super_loc },
                    expression: expression,
                    loc: SourceLocation::from_parts(super_loc, close_token.loc),
                },
            ))
        })
    }

    // SuperProperty : `super` `.` IdentifierName
    pub fn super_property_static(
        &self,
        super_token: arena::Box<'alloc, Token>,
        identifier_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let super_loc = super_token.loc;
        let identifier_loc = identifier_token.loc;
        self.alloc_with(|| {
            Expression::MemberExpression(MemberExpression::StaticMemberExpression(
                StaticMemberExpression {
                    object: ExpressionOrSuper::Super { loc: super_loc },
                    property: self.identifier_name(identifier_token),
                    loc: SourceLocation::from_parts(super_loc, identifier_loc),
                },
            ))
        })
    }

    // NewTarget : `new` `.` `target`
    pub fn new_target_expr(
        &self,
        new_token: arena::Box<'alloc, Token>,
        target_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        return self.alloc_with(|| Expression::NewTargetExpression {
            loc: SourceLocation::from_parts(new_token.loc, target_token.loc),
        });
    }

    // NewExpression : `new` NewExpression
    pub fn new_expr_without_arguments(
        &self,
        new_token: arena::Box<'alloc, Token>,
        callee: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let callee_loc = callee.get_loc();
        self.alloc_with(|| Expression::NewExpression {
            callee,
            arguments: Arguments {
                args: self.new_vec(),
                loc: SourceLocation::new(callee_loc.end, callee_loc.end),
            },
            loc: SourceLocation::from_parts(new_token.loc, callee_loc),
        })
    }

    // CallExpression : CallExpression Arguments
    // CoverCallExpressionAndAsyncArrowHead : MemberExpression Arguments
    // CallMemberExpression : MemberExpression Arguments
    pub fn call_expr(
        &self,
        callee: arena::Box<'alloc, Expression<'alloc>>,
        arguments: arena::Box<'alloc, Arguments<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let callee_loc = callee.get_loc();
        let arguments_loc = arguments.loc;
        self.alloc_with(|| {
            Expression::CallExpression(CallExpression {
                callee: ExpressionOrSuper::Expression(callee),
                arguments: arguments.unbox(),
                loc: SourceLocation::from_parts(callee_loc, arguments_loc),
            })
        })
    }

    // SuperCall : `super` Arguments
    pub fn super_call(
        &self,
        super_token: arena::Box<'alloc, Token>,
        arguments: arena::Box<'alloc, Arguments<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let super_loc = super_token.loc;
        let arguments_loc = arguments.loc;
        self.alloc_with(|| {
            Expression::CallExpression(CallExpression {
                callee: ExpressionOrSuper::Super { loc: super_loc },
                arguments: arguments.unbox(),
                loc: SourceLocation::from_parts(super_loc, arguments_loc),
            })
        })
    }

    // ImportCall : `import` `(` AssignmentExpression `)`
    pub fn import_call(
        &self,
        import_token: arena::Box<'alloc, Token>,
        argument: arena::Box<'alloc, Expression<'alloc>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        self.alloc_with(|| Expression::ImportCallExpression {
            argument,
            loc: SourceLocation::from_parts(import_token.loc, close_token.loc),
        })
    }

    // Arguments : `(` `)`
    pub fn arguments_empty(
        &self,
        open_token: arena::Box<'alloc, Token>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Arguments<'alloc>> {
        self.alloc_with(|| Arguments {
            args: self.new_vec(),
            loc: SourceLocation::from_parts(open_token.loc, close_token.loc),
        })
    }

    pub fn arguments_single(
        &self,
        expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Arguments<'alloc>> {
        self.alloc_with(|| Arguments {
            args: self.new_vec_single(Argument::Expression(expression)),
            // This will be overwritten once the enclosing arguments gets
            // parsed.
            loc: SourceLocation::default(),
        })
    }

    pub fn arguments_spread_single(
        &self,
        expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Arguments<'alloc>> {
        self.alloc_with(|| Arguments {
            args: self.new_vec_single(Argument::SpreadElement(expression)),
            // This will be overwritten once the enclosing arguments gets
            // parsed.
            loc: SourceLocation::default(),
        })
    }

    pub fn arguments(
        &self,
        open_token: arena::Box<'alloc, Token>,
        mut arguments: arena::Box<'alloc, Arguments<'alloc>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Arguments<'alloc>> {
        arguments.loc.set_range(open_token.loc, close_token.loc);
        arguments
    }

    // ArgumentList : AssignmentExpression
    // ArgumentList : ArgumentList `,` AssignmentExpression
    pub fn arguments_append(
        &self,
        mut arguments: arena::Box<'alloc, Arguments<'alloc>>,
        expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Arguments<'alloc>> {
        self.push(&mut arguments.args, Argument::Expression(expression));
        arguments
    }

    // ArgumentList : `...` AssignmentExpression
    // ArgumentList : ArgumentList `,` `...` AssignmentExpression
    pub fn arguments_append_spread(
        &self,
        mut arguments: arena::Box<'alloc, Arguments<'alloc>>,
        expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Arguments<'alloc>> {
        self.push(&mut arguments.args, Argument::SpreadElement(expression));
        arguments
    }

    // UpdateExpression : LeftHandSideExpression `++`
    pub fn post_increment_expr(
        &self,
        operand: arena::Box<'alloc, Expression<'alloc>>,
        operator_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Expression<'alloc>>> {
        let operand = self.expression_to_simple_assignment_target(operand)?;
        let operand_loc = operand.get_loc();
        Ok(self.alloc_with(|| Expression::UpdateExpression {
            is_prefix: false,
            operator: UpdateOperator::Increment {
                loc: operator_token.loc,
            },
            operand,
            loc: SourceLocation::from_parts(operand_loc, operator_token.loc),
        }))
    }

    // UpdateExpression : LeftHandSideExpression `--`
    pub fn post_decrement_expr(
        &self,
        operand: arena::Box<'alloc, Expression<'alloc>>,
        operator_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Expression<'alloc>>> {
        let operand = self.expression_to_simple_assignment_target(operand)?;
        let operand_loc = operand.get_loc();
        Ok(self.alloc_with(|| Expression::UpdateExpression {
            is_prefix: false,
            operator: UpdateOperator::Decrement {
                loc: operator_token.loc,
            },
            operand,
            loc: SourceLocation::from_parts(operand_loc, operator_token.loc),
        }))
    }

    // UpdateExpression : `++` UnaryExpression
    pub fn pre_increment_expr(
        &self,
        operator_token: arena::Box<'alloc, Token>,
        operand: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Expression<'alloc>>> {
        let operand = self.expression_to_simple_assignment_target(operand)?;
        let operand_loc = operand.get_loc();
        Ok(self.alloc_with(|| Expression::UpdateExpression {
            is_prefix: true,
            operator: UpdateOperator::Increment {
                loc: operator_token.loc,
            },
            operand,
            loc: SourceLocation::from_parts(operator_token.loc, operand_loc),
        }))
    }

    // UpdateExpression : `--` UnaryExpression
    pub fn pre_decrement_expr(
        &self,
        operator_token: arena::Box<'alloc, Token>,
        operand: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Expression<'alloc>>> {
        let operand = self.expression_to_simple_assignment_target(operand)?;
        let operand_loc = operand.get_loc();
        Ok(self.alloc_with(|| Expression::UpdateExpression {
            is_prefix: true,
            operator: UpdateOperator::Decrement {
                loc: operator_token.loc,
            },
            operand,
            loc: SourceLocation::from_parts(operator_token.loc, operand_loc),
        }))
    }

    // UnaryExpression : `delete` UnaryExpression
    pub fn delete_expr(
        &self,
        operator_token: arena::Box<'alloc, Token>,
        operand: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let operand_loc = operand.get_loc();
        self.alloc_with(|| Expression::UnaryExpression {
            operator: UnaryOperator::Delete {
                loc: operator_token.loc,
            },
            operand,
            loc: SourceLocation::from_parts(operator_token.loc, operand_loc),
        })
    }

    // UnaryExpression : `void` UnaryExpression
    pub fn void_expr(
        &self,
        operator_token: arena::Box<'alloc, Token>,
        operand: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let operand_loc = operand.get_loc();
        self.alloc_with(|| Expression::UnaryExpression {
            operator: UnaryOperator::Void {
                loc: operator_token.loc,
            },
            operand,
            loc: SourceLocation::from_parts(operator_token.loc, operand_loc),
        })
    }

    // UnaryExpression : `typeof` UnaryExpression
    pub fn typeof_expr(
        &self,
        operator_token: arena::Box<'alloc, Token>,
        operand: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let operand_loc = operand.get_loc();
        self.alloc_with(|| Expression::UnaryExpression {
            operator: UnaryOperator::Typeof {
                loc: operator_token.loc,
            },
            operand,
            loc: SourceLocation::from_parts(operator_token.loc, operand_loc),
        })
    }

    // UnaryExpression : `+` UnaryExpression
    pub fn unary_plus_expr(
        &self,
        operator_token: arena::Box<'alloc, Token>,
        operand: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let operand_loc = operand.get_loc();
        self.alloc_with(|| Expression::UnaryExpression {
            operator: UnaryOperator::Plus {
                loc: operator_token.loc,
            },
            operand,
            loc: SourceLocation::from_parts(operator_token.loc, operand_loc),
        })
    }

    // UnaryExpression : `-` UnaryExpression
    pub fn unary_minus_expr(
        &self,
        operator_token: arena::Box<'alloc, Token>,
        operand: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let operand_loc = operand.get_loc();
        self.alloc_with(|| Expression::UnaryExpression {
            operator: UnaryOperator::Minus {
                loc: operator_token.loc,
            },
            operand,
            loc: SourceLocation::from_parts(operator_token.loc, operand_loc),
        })
    }

    // UnaryExpression : `~` UnaryExpression
    pub fn bitwise_not_expr(
        &self,
        operator_token: arena::Box<'alloc, Token>,
        operand: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let operand_loc = operand.get_loc();
        self.alloc_with(|| Expression::UnaryExpression {
            operator: UnaryOperator::BitwiseNot {
                loc: operator_token.loc,
            },
            operand,
            loc: SourceLocation::from_parts(operator_token.loc, operand_loc),
        })
    }

    // UnaryExpression : `!` UnaryExpression
    pub fn logical_not_expr(
        &self,
        operator_token: arena::Box<'alloc, Token>,
        operand: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let operand_loc = operand.get_loc();
        self.alloc_with(|| Expression::UnaryExpression {
            operator: UnaryOperator::LogicalNot {
                loc: operator_token.loc,
            },
            operand,
            loc: SourceLocation::from_parts(operator_token.loc, operand_loc),
        })
    }

    pub fn equals_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::Equals { loc: token.loc }
    }
    pub fn not_equals_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::NotEquals { loc: token.loc }
    }
    pub fn strict_equals_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::StrictEquals { loc: token.loc }
    }
    pub fn strict_not_equals_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::StrictNotEquals { loc: token.loc }
    }
    pub fn less_than_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::LessThan { loc: token.loc }
    }
    pub fn less_than_or_equal_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::LessThanOrEqual { loc: token.loc }
    }
    pub fn greater_than_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::GreaterThan { loc: token.loc }
    }
    pub fn greater_than_or_equal_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::GreaterThanOrEqual { loc: token.loc }
    }
    pub fn in_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::In { loc: token.loc }
    }
    pub fn instanceof_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::Instanceof { loc: token.loc }
    }
    pub fn left_shift_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::LeftShift { loc: token.loc }
    }
    pub fn right_shift_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::RightShift { loc: token.loc }
    }
    pub fn right_shift_ext_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::RightShiftExt { loc: token.loc }
    }
    pub fn add_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::Add { loc: token.loc }
    }
    pub fn sub_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::Sub { loc: token.loc }
    }
    pub fn mul_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::Mul { loc: token.loc }
    }
    pub fn div_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::Div { loc: token.loc }
    }
    pub fn mod_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::Mod { loc: token.loc }
    }
    pub fn pow_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::Pow { loc: token.loc }
    }
    pub fn comma_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::Comma { loc: token.loc }
    }
    pub fn coalesce_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::Coalesce { loc: token.loc }
    }
    pub fn logical_or_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::LogicalOr { loc: token.loc }
    }
    pub fn logical_and_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::LogicalAnd { loc: token.loc }
    }
    pub fn bitwise_or_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::BitwiseOr { loc: token.loc }
    }
    pub fn bitwise_xor_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::BitwiseXor { loc: token.loc }
    }
    pub fn bitwise_and_op(&self, token: arena::Box<'alloc, Token>) -> BinaryOperator {
        BinaryOperator::BitwiseAnd { loc: token.loc }
    }

    // Due to limitations of the current parser generator,
    // MultiplicativeOperators and CompoundAssignmentOperators currently get
    // boxed.
    pub fn box_op(&self, op: BinaryOperator) -> arena::Box<'alloc, BinaryOperator> {
        self.alloc_with(|| op)
    }

    // MultiplicativeExpression : MultiplicativeExpression MultiplicativeOperator ExponentiationExpression
    pub fn multiplicative_expr(
        &self,
        left: arena::Box<'alloc, Expression<'alloc>>,
        operator: arena::Box<'alloc, BinaryOperator>,
        right: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        self.binary_expr(operator.unbox(), left, right)
    }

    // ExponentiationExpression : UpdateExpression `**` ExponentiationExpression
    // AdditiveExpression : AdditiveExpression `+` MultiplicativeExpression
    // AdditiveExpression : AdditiveExpression `-` MultiplicativeExpression
    // ShiftExpression : ShiftExpression `<<` AdditiveExpression
    // ShiftExpression : ShiftExpression `>>` AdditiveExpression
    // ShiftExpression : ShiftExpression `>>>` AdditiveExpression
    // RelationalExpression : RelationalExpression `<` ShiftExpression
    // RelationalExpression : RelationalExpression `>` ShiftExpression
    // RelationalExpression : RelationalExpression `<=` ShiftExpression
    // RelationalExpression : RelationalExpression `>=` ShiftExpression
    // RelationalExpression : RelationalExpression `instanceof` ShiftExpression
    // RelationalExpression : [+In] RelationalExpression `in` ShiftExpression
    // EqualityExpression : EqualityExpression `==` RelationalExpression
    // EqualityExpression : EqualityExpression `!=` RelationalExpression
    // EqualityExpression : EqualityExpression `===` RelationalExpression
    // EqualityExpression : EqualityExpression `!==` RelationalExpression
    // BitwiseANDExpression : BitwiseANDExpression `&` EqualityExpression
    // BitwiseXORExpression : BitwiseXORExpression `^` BitwiseANDExpression
    // BitwiseORExpression : BitwiseORExpression `|` BitwiseXORExpression
    // LogicalANDExpression : LogicalANDExpression `&&` BitwiseORExpression
    // LogicalORExpression : LogicalORExpression `||` LogicalANDExpression
    pub fn binary_expr(
        &self,
        operator: BinaryOperator,
        left: arena::Box<'alloc, Expression<'alloc>>,
        right: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let left_loc = left.get_loc();
        let right_loc = right.get_loc();
        self.alloc_with(|| Expression::BinaryExpression {
            operator,
            left,
            right,
            loc: SourceLocation::from_parts(left_loc, right_loc),
        })
    }

    // ConditionalExpression : LogicalORExpression `?` AssignmentExpression `:` AssignmentExpression
    pub fn conditional_expr(
        &self,
        test: arena::Box<'alloc, Expression<'alloc>>,
        consequent: arena::Box<'alloc, Expression<'alloc>>,
        alternate: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let test_loc = test.get_loc();
        let alternate_loc = alternate.get_loc();
        self.alloc_with(|| Expression::ConditionalExpression {
            test,
            consequent,
            alternate,
            loc: SourceLocation::from_parts(test_loc, alternate_loc),
        })
    }

    /// Refine an *ArrayLiteral* into an *ArrayAssignmentPattern*.
    fn array_expression_to_array_assignment_target(
        &self,
        mut elements: arena::Vec<'alloc, ArrayExpressionElement<'alloc>>,
        loc: SourceLocation,
    ) -> Result<'alloc, ArrayAssignmentTarget<'alloc>> {
        let spread = self.pop_trailing_spread_element(&mut elements);
        let elements =
            self.collect_vec_from_results(elements.into_iter().map(|element| match element {
                ArrayExpressionElement::SpreadElement(_) => Err(ParseError::NotImplemented(
                    "rest destructuring in array pattern",
                )),
                ArrayExpressionElement::Expression(expression) => Ok(Some(
                    self.expression_to_assignment_target_maybe_default(expression)?,
                )),
                ArrayExpressionElement::Elision { .. } => Ok(None),
            }))?;
        let rest: Option<Result<'alloc, arena::Box<'alloc, AssignmentTarget<'alloc>>>> =
            spread.map(|expr| Ok(self.alloc(self.expression_to_assignment_target(expr)?)));
        let rest = rest.transpose()?;
        Ok(ArrayAssignmentTarget {
            elements,
            rest,
            loc,
        })
    }

    fn object_property_to_assignment_target_property(
        &self,
        property: arena::Box<'alloc, ObjectProperty<'alloc>>,
    ) -> Result<'alloc, AssignmentTargetProperty<'alloc>> {
        Ok(match property.unbox() {
            ObjectProperty::NamedObjectProperty(NamedObjectProperty::MethodDefinition(_)) => {
                return Err(ParseError::ObjectPatternWithMethod)
            }

            ObjectProperty::NamedObjectProperty(NamedObjectProperty::DataProperty(
                DataProperty {
                    property_name,
                    expression,
                    loc,
                },
            )) => AssignmentTargetProperty::AssignmentTargetPropertyProperty(
                AssignmentTargetPropertyProperty {
                    name: property_name,
                    binding: self.expression_to_assignment_target_maybe_default(expression)?,
                    loc,
                },
            ),

            ObjectProperty::ShorthandProperty(ShorthandProperty {
                name: IdentifierExpression { name, loc },
                ..
            }) => {
                // TODO - support CoverInitializedName
                AssignmentTargetProperty::AssignmentTargetPropertyIdentifier(
                    AssignmentTargetPropertyIdentifier {
                        binding: AssignmentTargetIdentifier { name, loc },
                        init: None,
                        loc,
                    },
                )
            }

            ObjectProperty::SpreadProperty(_expression) => {
                return Err(ParseError::ObjectPatternWithNonFinalRest)
            }
        })
    }

    // Refine an *ObjectLiteral* into an *ObjectAssignmentPattern*.
    fn object_expression_to_object_assignment_target(
        &self,
        mut properties: arena::Vec<'alloc, arena::Box<'alloc, ObjectProperty<'alloc>>>,
        loc: SourceLocation,
    ) -> Result<'alloc, ObjectAssignmentTarget<'alloc>> {
        let spread = self.pop_trailing_spread_property(&mut properties);
        let properties = self.collect_vec_from_results(
            properties
                .into_iter()
                .map(|p| self.object_property_to_assignment_target_property(p)),
        )?;
        let rest: Option<Result<'alloc, arena::Box<'alloc, AssignmentTarget<'alloc>>>> =
            spread.map(|expr| Ok(self.alloc(self.expression_to_assignment_target(expr)?)));
        let rest = rest.transpose()?;
        Ok(ObjectAssignmentTarget {
            properties,
            rest,
            loc,
        })
    }

    fn expression_to_assignment_target_maybe_default(
        &self,
        expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, AssignmentTargetMaybeDefault<'alloc>> {
        Ok(match expression.unbox() {
            Expression::AssignmentExpression {
                binding,
                expression,
                loc,
            } => AssignmentTargetMaybeDefault::AssignmentTargetWithDefault(
                AssignmentTargetWithDefault {
                    binding,
                    init: expression,
                    loc,
                },
            ),

            other => AssignmentTargetMaybeDefault::AssignmentTarget(
                self.expression_to_assignment_target(self.alloc_with(|| other))?,
            ),
        })
    }

    fn expression_to_assignment_target(
        &self,
        expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, AssignmentTarget<'alloc>> {
        Ok(match expression.unbox() {
            Expression::ArrayExpression(ArrayExpression { elements, loc }) => {
                AssignmentTarget::AssignmentTargetPattern(
                    AssignmentTargetPattern::ArrayAssignmentTarget(
                        self.array_expression_to_array_assignment_target(elements, loc)?,
                    ),
                )
            }

            Expression::ObjectExpression(ObjectExpression { properties, loc }) => {
                AssignmentTarget::AssignmentTargetPattern(
                    AssignmentTargetPattern::ObjectAssignmentTarget(
                        self.object_expression_to_object_assignment_target(properties, loc)?,
                    ),
                )
            }

            other => AssignmentTarget::SimpleAssignmentTarget(
                self.expression_to_simple_assignment_target(self.alloc_with(|| other))?,
            ),
        })
    }

    fn expression_to_simple_assignment_target(
        &self,
        expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, SimpleAssignmentTarget<'alloc>> {
        Ok(match expression.unbox() {
            // Static Semantics: AssignmentTargetType
            // https://tc39.es/ecma262/#sec-identifiers-static-semantics-assignmenttargettype
            Expression::IdentifierExpression(IdentifierExpression { name, loc }) => {
                // IdentifierReference : Identifier
                //
                // 1. If this IdentifierReference is contained in strict mode
                //    code and StringValue of Identifier is "eval" or
                //    "arguments", return invalid.
                if name.value == CommonSourceAtomSetIndices::arguments()
                    || name.value == CommonSourceAtomSetIndices::eval()
                {
                    if self.is_strict()? {
                        return Err(ParseError::InvalidAssignmentTarget);
                    }
                }

                // 2. Return simple.
                //
                // IdentifierReference : yield
                //
                // 1. Return simple.
                //
                // IdentifierReference : await
                //
                // 1. Return simple.
                SimpleAssignmentTarget::AssignmentTargetIdentifier(AssignmentTargetIdentifier {
                    name,
                    loc,
                })
            }

            // Static Semantics: AssignmentTargetType
            // https://tc39.es/ecma262/#sec-static-semantics-static-semantics-assignmenttargettype
            //
            // MemberExpression :
            //   MemberExpression [ Expression ]
            //   MemberExpression . IdentifierName
            //   SuperProperty
            //
            // 1. Return simple.
            Expression::MemberExpression(MemberExpression::StaticMemberExpression(
                StaticMemberExpression {
                    object,
                    property,
                    loc,
                },
            )) => SimpleAssignmentTarget::MemberAssignmentTarget(
                MemberAssignmentTarget::StaticMemberAssignmentTarget(
                    StaticMemberAssignmentTarget {
                        object,
                        property,
                        loc,
                    },
                ),
            ),
            Expression::MemberExpression(MemberExpression::ComputedMemberExpression(
                ComputedMemberExpression {
                    object,
                    expression,
                    loc,
                },
            )) => SimpleAssignmentTarget::MemberAssignmentTarget(
                MemberAssignmentTarget::ComputedMemberAssignmentTarget(
                    ComputedMemberAssignmentTarget {
                        object,
                        expression,
                        loc,
                    },
                ),
            ),

            // Static Semantics: AssignmentTargetType
            // https://tc39.es/ecma262/#sec-static-semantics-static-semantics-assignmenttargettype
            //
            // CallExpression :
            //   CallExpression [ Expression ]
            //   CallExpression . IdentifierName
            //
            // 1. Return simple.
            Expression::CallExpression(CallExpression { .. }) => {
                return Err(ParseError::NotImplemented(
                    "Assignment to CallExpression is allowed for non-strict mode.",
                ));
            }

            _ => {
                return Err(ParseError::InvalidAssignmentTarget);
            }
        })
    }

    // AssignmentExpression : LeftHandSideExpression `=` AssignmentExpression
    pub fn assignment_expr(
        &self,
        left_hand_side: arena::Box<'alloc, Expression<'alloc>>,
        value: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Expression<'alloc>>> {
        let target = self.expression_to_assignment_target(left_hand_side)?;
        let target_loc = target.get_loc();
        let value_loc = value.get_loc();
        Ok(self.alloc_with(|| Expression::AssignmentExpression {
            binding: target,
            expression: value,
            loc: SourceLocation::from_parts(target_loc, value_loc),
        }))
    }

    pub fn add_assign_op(&self, token: arena::Box<'alloc, Token>) -> CompoundAssignmentOperator {
        CompoundAssignmentOperator::Add { loc: token.loc }
    }
    pub fn sub_assign_op(&self, token: arena::Box<'alloc, Token>) -> CompoundAssignmentOperator {
        CompoundAssignmentOperator::Sub { loc: token.loc }
    }
    pub fn mul_assign_op(&self, token: arena::Box<'alloc, Token>) -> CompoundAssignmentOperator {
        CompoundAssignmentOperator::Mul { loc: token.loc }
    }
    pub fn div_assign_op(&self, token: arena::Box<'alloc, Token>) -> CompoundAssignmentOperator {
        CompoundAssignmentOperator::Div { loc: token.loc }
    }
    pub fn mod_assign_op(&self, token: arena::Box<'alloc, Token>) -> CompoundAssignmentOperator {
        CompoundAssignmentOperator::Mod { loc: token.loc }
    }
    pub fn pow_assign_op(&self, token: arena::Box<'alloc, Token>) -> CompoundAssignmentOperator {
        CompoundAssignmentOperator::Pow { loc: token.loc }
    }
    pub fn left_shift_assign_op(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> CompoundAssignmentOperator {
        CompoundAssignmentOperator::LeftShift { loc: token.loc }
    }
    pub fn right_shift_assign_op(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> CompoundAssignmentOperator {
        CompoundAssignmentOperator::RightShift { loc: token.loc }
    }
    pub fn right_shift_ext_assign_op(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> CompoundAssignmentOperator {
        CompoundAssignmentOperator::RightShiftExt { loc: token.loc }
    }
    pub fn bitwise_or_assign_op(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> CompoundAssignmentOperator {
        CompoundAssignmentOperator::Or { loc: token.loc }
    }
    pub fn bitwise_xor_assign_op(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> CompoundAssignmentOperator {
        CompoundAssignmentOperator::Xor { loc: token.loc }
    }
    pub fn bitwise_and_assign_op(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> CompoundAssignmentOperator {
        CompoundAssignmentOperator::And { loc: token.loc }
    }

    pub fn box_assign_op(
        &self,
        op: CompoundAssignmentOperator,
    ) -> arena::Box<'alloc, CompoundAssignmentOperator> {
        self.alloc_with(|| op)
    }

    // AssignmentExpression : LeftHandSideExpression AssignmentOperator AssignmentExpression
    pub fn compound_assignment_expr(
        &self,
        left_hand_side: arena::Box<'alloc, Expression<'alloc>>,
        operator: arena::Box<'alloc, CompoundAssignmentOperator>,
        value: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Expression<'alloc>>> {
        let target = self.expression_to_simple_assignment_target(left_hand_side)?;
        let target_loc = target.get_loc();
        let value_loc = value.get_loc();
        Ok(
            self.alloc_with(|| Expression::CompoundAssignmentExpression {
                operator: operator.unbox(),
                binding: target,
                expression: value,
                loc: SourceLocation::from_parts(target_loc, value_loc),
            }),
        )
    }

    // BlockStatement : Block
    pub fn block_statement(
        &self,
        block: arena::Box<'alloc, Block<'alloc>>,
    ) -> arena::Box<'alloc, Statement<'alloc>> {
        let loc = block.loc;
        self.alloc_with(|| Statement::BlockStatement {
            block: block.unbox(),
            loc,
        })
    }

    // Block : `{` StatementList? `}`
    pub fn block(
        &mut self,
        open_token: arena::Box<'alloc, Token>,
        statements: Option<arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Block<'alloc>>> {
        self.check_block_bindings(open_token.loc.start)?;

        Ok(self.alloc_with(|| Block {
            statements: match statements {
                Some(statements) => statements.unbox(),
                None => self.new_vec(),
            },
            declarations: None,
            loc: SourceLocation::from_parts(open_token.loc, close_token.loc),
        }))
    }

    // Block : `{` StatementList? `}`
    // for Catch
    pub fn catch_block(
        &mut self,
        open_token: arena::Box<'alloc, Token>,
        statements: Option<arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Block<'alloc>> {
        // Early Error handling is done in Catch.

        self.alloc_with(|| Block {
            statements: match statements {
                Some(statements) => statements.unbox(),
                None => self.new_vec(),
            },
            declarations: None,
            loc: SourceLocation::from_parts(open_token.loc, close_token.loc),
        })
    }

    // StatementList : StatementListItem
    pub fn statement_list_single(
        &self,
        statement: arena::Box<'alloc, Statement<'alloc>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>> {
        self.alloc_with(|| self.new_vec_single(statement.unbox()))
    }

    // StatementList : StatementList StatementListItem
    pub fn statement_list_append(
        &self,
        mut list: arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>>,
        statement: arena::Box<'alloc, Statement<'alloc>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>> {
        self.push(&mut list, statement.unbox());
        list
    }

    // LexicalDeclaration : LetOrConst BindingList `;`
    pub fn lexical_declaration(
        &mut self,
        kind: arena::Box<'alloc, VariableDeclarationKind>,
        declarators: arena::Box<'alloc, arena::Vec<'alloc, VariableDeclarator<'alloc>>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        let binding_kind = match &*kind {
            VariableDeclarationKind::Let { .. } => BindingKind::Let,
            VariableDeclarationKind::Const { .. } => BindingKind::Const,
            _ => panic!("unexpected VariableDeclarationKind"),
        };

        self.mark_binding_kind(kind.get_loc().start, None, binding_kind);

        // 13.3.1.1 Static Semantics: Early Errors
        if let VariableDeclarationKind::Const { .. } = *kind {
            for v in declarators.iter() {
                if v.init == None {
                    return Err(ParseError::NotImplemented(
                        "Missing initializer in a lexical binding.",
                    ));
                }
            }
        }

        let kind_loc = kind.get_loc();
        let declarator_loc = declarators
            .last()
            .expect("There should be at least one declarator")
            .get_loc();
        Ok(self.alloc_with(|| {
            Statement::VariableDeclarationStatement(VariableDeclaration {
                kind: kind.unbox(),
                declarators: declarators.unbox(),
                loc: SourceLocation::from_parts(kind_loc, declarator_loc),
            })
        }))
    }

    // ForLexicalDeclaration : LetOrConst BindingList `;`
    pub fn for_lexical_declaration(
        &mut self,
        kind: arena::Box<'alloc, VariableDeclarationKind>,
        declarators: arena::Box<'alloc, arena::Vec<'alloc, VariableDeclarator<'alloc>>>,
    ) -> Result<'alloc, arena::Box<'alloc, VariableDeclarationOrExpression<'alloc>>> {
        let binding_kind = match &*kind {
            VariableDeclarationKind::Let { .. } => BindingKind::Let,
            VariableDeclarationKind::Const { .. } => BindingKind::Const,
            _ => panic!("unexpected VariableDeclarationKind"),
        };
        self.mark_binding_kind(kind.get_loc().start, None, binding_kind);

        // 13.3.1.1 Static Semantics: Early Errors
        if let VariableDeclarationKind::Const { .. } = *kind {
            for v in declarators.iter() {
                if v.init == None {
                    return Err(ParseError::NotImplemented(
                        "Missing initializer in a lexical binding.",
                    ));
                }
            }
        }

        let kind_loc = kind.get_loc();
        let declarator_loc = declarators
            .last()
            .expect("There should be at least one declarator")
            .get_loc();
        Ok(self.alloc_with(|| {
            VariableDeclarationOrExpression::VariableDeclaration(VariableDeclaration {
                kind: kind.unbox(),
                declarators: declarators.unbox(),
                loc: SourceLocation::from_parts(kind_loc, declarator_loc),
            })
        }))
    }

    // LetOrConst : `let`
    pub fn let_kind(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, VariableDeclarationKind> {
        self.alloc_with(|| VariableDeclarationKind::Let { loc: token.loc })
    }

    // LetOrConst : `const`
    pub fn const_kind(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, VariableDeclarationKind> {
        self.alloc_with(|| VariableDeclarationKind::Const { loc: token.loc })
    }

    // VariableStatement : `var` VariableDeclarationList `;`
    pub fn variable_statement(
        &mut self,
        var_token: arena::Box<'alloc, Token>,
        declarators: arena::Box<'alloc, arena::Vec<'alloc, VariableDeclarator<'alloc>>>,
    ) -> arena::Box<'alloc, Statement<'alloc>> {
        let var_loc = var_token.loc;
        let declarator_loc = declarators
            .last()
            .expect("There should be at least one declarator")
            .get_loc();

        self.mark_binding_kind(var_loc.start, None, BindingKind::Var);

        self.alloc_with(|| {
            Statement::VariableDeclarationStatement(VariableDeclaration {
                kind: VariableDeclarationKind::Var { loc: var_loc },
                declarators: declarators.unbox(),
                loc: SourceLocation::from_parts(var_loc, declarator_loc),
            })
        })
    }

    // VariableDeclarationList : VariableDeclaration
    // BindingList : LexicalBinding
    pub fn variable_declaration_list_single(
        &self,
        decl: arena::Box<'alloc, VariableDeclarator<'alloc>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, VariableDeclarator<'alloc>>> {
        self.alloc_with(|| self.new_vec_single(decl.unbox()))
    }

    // VariableDeclarationList : VariableDeclarationList `,` VariableDeclaration
    // BindingList : BindingList `,` LexicalBinding
    pub fn variable_declaration_list_append(
        &self,
        mut list: arena::Box<'alloc, arena::Vec<'alloc, VariableDeclarator<'alloc>>>,
        decl: arena::Box<'alloc, VariableDeclarator<'alloc>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, VariableDeclarator<'alloc>>> {
        self.push(&mut list, decl.unbox());
        list
    }

    // VariableDeclaration : BindingIdentifier Initializer?
    // VariableDeclaration : BindingPattern Initializer
    pub fn variable_declaration(
        &self,
        binding: arena::Box<'alloc, Binding<'alloc>>,
        init: Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) -> arena::Box<'alloc, VariableDeclarator<'alloc>> {
        let binding_loc = binding.get_loc();
        let loc = match init {
            Some(ref init) => SourceLocation::from_parts(binding_loc, init.get_loc()),
            None => binding_loc,
        };
        self.alloc_with(|| VariableDeclarator {
            binding: binding.unbox(),
            init,
            loc,
        })
    }

    // ObjectBindingPattern : `{` `}`
    // ObjectBindingPattern : `{` BindingRestProperty `}`
    // ObjectBindingPattern : `{` BindingPropertyList `}`
    // ObjectBindingPattern : `{` BindingPropertyList `,` BindingRestProperty? `}`
    pub fn object_binding_pattern(
        &self,
        open_token: arena::Box<'alloc, Token>,
        properties: arena::Box<'alloc, arena::Vec<'alloc, BindingProperty<'alloc>>>,
        rest: Option<arena::Box<'alloc, BindingIdentifier>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Binding<'alloc>> {
        self.alloc_with(|| {
            Binding::BindingPattern(BindingPattern::ObjectBinding(ObjectBinding {
                properties: properties.unbox(),
                rest,
                loc: SourceLocation::from_parts(open_token.loc, close_token.loc),
            }))
        })
    }

    pub fn binding_element_list_empty(
        &self,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, Option<Parameter<'alloc>>>> {
        self.alloc_with(|| self.new_vec())
    }

    // ArrayBindingPattern : `[` Elision? BindingRestElement? `]`
    // ArrayBindingPattern : `[` BindingElementList `]`
    // ArrayBindingPattern : `[` BindingElementList `,` Elision? BindingRestElement? `]`
    pub fn array_binding_pattern(
        &self,
        open_token: arena::Box<'alloc, Token>,
        mut elements: arena::Box<'alloc, arena::Vec<'alloc, Option<Parameter<'alloc>>>>,
        elision: Option<arena::Box<'alloc, ArrayExpression<'alloc>>>,
        rest: Option<arena::Box<'alloc, Binding<'alloc>>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Binding<'alloc>> {
        if let Some(elision) = elision {
            for _ in 0..elision.elements.len() {
                self.push(&mut elements, None);
            }
        }

        self.alloc_with(|| {
            Binding::BindingPattern(BindingPattern::ArrayBinding(ArrayBinding {
                elements: elements.unbox(),
                rest,
                loc: SourceLocation::from_parts(open_token.loc, close_token.loc),
            }))
        })
    }

    pub fn binding_property_list_empty(
        &self,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, BindingProperty<'alloc>>> {
        self.alloc_with(|| self.new_vec())
    }

    // BindingPropertyList : BindingProperty
    pub fn binding_property_list_single(
        &self,
        property: arena::Box<'alloc, BindingProperty<'alloc>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, BindingProperty<'alloc>>> {
        self.alloc_with(|| self.new_vec_single(property.unbox()))
    }

    // BindingPropertyList : BindingPropertyList `,` BindingProperty
    pub fn binding_property_list_append(
        &self,
        mut list: arena::Box<'alloc, arena::Vec<'alloc, BindingProperty<'alloc>>>,
        property: arena::Box<'alloc, BindingProperty<'alloc>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, BindingProperty<'alloc>>> {
        self.push(&mut list, property.unbox());
        list
    }

    // BindingElementList : BindingElementList `,` BindingElisionElement
    pub fn binding_element_list_append(
        &self,
        mut list: arena::Box<'alloc, arena::Vec<'alloc, Option<Parameter<'alloc>>>>,
        mut element: arena::Box<'alloc, arena::Vec<'alloc, Option<Parameter<'alloc>>>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, Option<Parameter<'alloc>>>> {
        self.append(&mut list, &mut element);
        list
    }

    // BindingElisionElement : Elision? BindingElement
    pub fn binding_elision_element(
        &self,
        elision: Option<arena::Box<'alloc, ArrayExpression<'alloc>>>,
        element: arena::Box<'alloc, Parameter<'alloc>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, Option<Parameter<'alloc>>>> {
        let elision_count = elision.map(|v| v.elements.len()).unwrap_or(0);
        let mut result = self.alloc_with(|| self.new_vec());
        for _ in 0..elision_count {
            self.push(&mut result, None);
        }
        self.push(&mut result, Some(element.unbox()));
        result
    }

    // BindingProperty : SingleNameBinding
    pub fn binding_property_shorthand(
        &self,
        binding: arena::Box<'alloc, Parameter<'alloc>>,
    ) -> arena::Box<'alloc, BindingProperty<'alloc>> {
        // Previous parsing interpreted this as a Parameter. We need to take
        // all the pieces out of that box and put them in a new box.
        let (binding, init) = match binding.unbox() {
            Parameter::Binding(binding) => (binding, None),
            Parameter::BindingWithDefault(BindingWithDefault { binding, init, .. }) => {
                (binding, Some(init.unbox()))
            }
        };

        let binding = match binding {
            Binding::BindingIdentifier(bi) => bi,
            _ => {
                // The grammar ensures that the parser always passes a valid
                // argument to this method.
                panic!("invalid argument: binding_property_shorthand requires a Binding::BindingIdentifier");
            }
        };

        let loc = binding.loc;

        self.alloc_with(|| {
            BindingProperty::BindingPropertyIdentifier(BindingPropertyIdentifier {
                binding,
                init: init.map(|x| self.alloc_with(|| x)),
                loc,
            })
        })
    }

    // BindingProperty : PropertyName `:` BindingElement
    pub fn binding_property(
        &self,
        name: arena::Box<'alloc, PropertyName<'alloc>>,
        binding: arena::Box<'alloc, Parameter<'alloc>>,
    ) -> arena::Box<'alloc, BindingProperty<'alloc>> {
        let name_loc = name.get_loc();
        let binding_loc = binding.get_loc();
        self.alloc_with(|| {
            BindingProperty::BindingPropertyProperty(BindingPropertyProperty {
                name: name.unbox(),
                binding: binding.unbox(),
                loc: SourceLocation::from_parts(name_loc, binding_loc),
            })
        })
    }

    // BindingElement : BindingPattern Initializer?
    pub fn binding_element_pattern(
        &self,
        binding: arena::Box<'alloc, Binding<'alloc>>,
        init: Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) -> arena::Box<'alloc, Parameter<'alloc>> {
        self.alloc_with(|| match init {
            None => Parameter::Binding(binding.unbox()),
            Some(init) => {
                let binding_loc = binding.get_loc();
                let init_loc = init.get_loc();
                Parameter::BindingWithDefault(BindingWithDefault {
                    binding: binding.unbox(),
                    init,
                    loc: SourceLocation::from_parts(binding_loc, init_loc),
                })
            }
        })
    }

    // SingleNameBinding : BindingIdentifier Initializer?
    pub fn single_name_binding(
        &self,
        name: arena::Box<'alloc, BindingIdentifier>,
        init: Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) -> arena::Box<'alloc, Parameter<'alloc>> {
        let binding = Binding::BindingIdentifier(name.unbox());
        self.alloc_with(|| match init {
            None => Parameter::Binding(binding),
            Some(init) => {
                let binding_loc = binding.get_loc();
                let init_loc = init.get_loc();
                Parameter::BindingWithDefault(BindingWithDefault {
                    binding,
                    init,
                    loc: SourceLocation::from_parts(binding_loc, init_loc),
                })
            }
        })
    }

    // BindingRestElement : `...` BindingIdentifier
    pub fn binding_rest_element(
        &self,
        name: arena::Box<'alloc, BindingIdentifier>,
    ) -> arena::Box<'alloc, Binding<'alloc>> {
        self.alloc_with(|| Binding::BindingIdentifier(name.unbox()))
    }

    // EmptyStatement : `;`
    pub fn empty_statement(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Statement<'alloc>> {
        self.alloc_with(|| Statement::EmptyStatement { loc: token.loc })
    }

    // ExpressionStatement : [lookahead not in {'{', 'function', 'async', 'class', 'let'}] Expression `;`
    pub fn expression_statement(
        &self,
        expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Statement<'alloc>> {
        self.alloc_with(|| Statement::ExpressionStatement(expression))
    }

    // IfStatement : `if` `(` Expression `)` Statement `else` Statement
    // IfStatement : `if` `(` Expression `)` Statement
    pub fn if_statement(
        &self,
        if_token: arena::Box<'alloc, Token>,
        test: arena::Box<'alloc, Expression<'alloc>>,
        consequent: arena::Box<'alloc, Statement<'alloc>>,
        alternate: Option<arena::Box<'alloc, Statement<'alloc>>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        self.check_single_statement(&consequent)?;
        if let Some(ref stmt) = alternate {
            self.check_single_statement(&stmt)?;
        }

        let if_loc = if_token.loc;
        let loc = match alternate {
            Some(ref alternate) => SourceLocation::from_parts(if_loc, alternate.get_loc()),
            None => SourceLocation::from_parts(if_loc, consequent.get_loc()),
        };
        Ok(self.alloc_with(|| {
            Statement::IfStatement(IfStatement {
                test,
                consequent,
                alternate,
                loc,
            })
        }))
    }

    // Create BlockStatement from FunctionDeclaration, for the following:
    //
    // IfStatement : `if` `(` Expression `)` FunctionDeclaration `else` Statement
    // IfStatement : `if` `(` Expression `)` Statement `else` FunctionDeclaration
    // IfStatement : `if` `(` Expression `)` FunctionDeclaration `else` FunctionDeclaration
    // IfStatement : `if` `(` Expression `)` FunctionDeclaration
    pub fn make_block_stmt_from_function_decl(
        &mut self,
        fun: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        let fun_loc = fun.get_loc();

        // Annex B. FunctionDeclarations in IfStatement Statement Clauses
        // https://tc39.es/ecma262/#sec-functiondeclarations-in-ifstatement-statement-clauses
        //
        // This production only applies when parsing non-strict code.
        if self.is_strict()? {
            return Err(ParseError::FunctionDeclInSingleStatement);
        }

        // Code matching this production is processed as if each matching
        // occurrence of FunctionDeclaration[?Yield, ?Await, ~Default] was the
        // sole StatementListItem of a BlockStatement occupying that position
        // in the source code. The semantics of such a synthetic BlockStatement
        // includes the web legacy compatibility semantics specified in B.3.3.
        self.check_block_bindings(fun_loc.start)?;

        Ok(self.alloc_with(|| Statement::BlockStatement {
            block: Block {
                statements: self.new_vec_single(fun.unbox()),
                declarations: None,
                loc: fun_loc,
            },
            loc: fun_loc,
        }))
    }

    fn is_strict(&self) -> Result<'alloc, bool> {
        Err(ParseError::NotImplemented(
            "strict-mode-only early error is not yet supported",
        ))
    }

    // IterationStatement : `do` Statement `while` `(` Expression `)` `;`
    pub fn do_while_statement(
        &mut self,
        do_token: arena::Box<'alloc, Token>,
        stmt: arena::Box<'alloc, Statement<'alloc>>,
        test: arena::Box<'alloc, Expression<'alloc>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        self.check_single_statement(&stmt)?;

        self.pop_unlabelled_breaks_and_continues_from(do_token.loc.start);
        Ok(self.alloc_with(|| Statement::DoWhileStatement {
            block: stmt,
            test,
            loc: SourceLocation::from_parts(do_token.loc, close_token.loc),
        }))
    }

    // IterationStatement : `while` `(` Expression `)` Statement
    pub fn while_statement(
        &mut self,
        while_token: arena::Box<'alloc, Token>,
        test: arena::Box<'alloc, Expression<'alloc>>,
        stmt: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        self.check_single_statement(&stmt)?;

        let stmt_loc = stmt.get_loc();
        self.pop_unlabelled_breaks_and_continues_from(stmt_loc.start);
        Ok(self.alloc_with(|| Statement::WhileStatement {
            test,
            block: stmt,
            loc: SourceLocation::from_parts(while_token.loc, stmt_loc),
        }))
    }

    // IterationStatement : `for` `(` [lookahead != 'let'] Expression? `;` Expression? `;` Expression? `)` Statement
    // IterationStatement : `for` `(` `var` VariableDeclarationList `;` Expression? `;` Expression? `)` Statement
    pub fn for_statement(
        &mut self,
        for_token: arena::Box<'alloc, Token>,
        init: Option<VariableDeclarationOrExpression<'alloc>>,
        test: Option<arena::Box<'alloc, Expression<'alloc>>>,
        update: Option<arena::Box<'alloc, Expression<'alloc>>>,
        stmt: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        self.check_single_statement(&stmt)?;
        self.for_statement_common(for_token, init, test, update, stmt)
    }

    // IterationStatement : `for` `(` ForLexicalDeclaration Expression? `;` Expression? `)` Statement
    pub fn for_statement_lexical(
        &mut self,
        for_token: arena::Box<'alloc, Token>,
        init: VariableDeclarationOrExpression<'alloc>,
        test: Option<arena::Box<'alloc, Expression<'alloc>>>,
        update: Option<arena::Box<'alloc, Expression<'alloc>>>,
        stmt: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        self.check_single_statement(&stmt)?;
        self.check_lexical_for_bindings(&init.get_loc())?;
        self.for_statement_common(for_token, Some(init), test, update, stmt)
    }

    pub fn for_statement_common(
        &mut self,
        for_token: arena::Box<'alloc, Token>,
        init: Option<VariableDeclarationOrExpression<'alloc>>,
        test: Option<arena::Box<'alloc, Expression<'alloc>>>,
        update: Option<arena::Box<'alloc, Expression<'alloc>>>,
        stmt: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        let stmt_loc = stmt.get_loc();
        self.pop_unlabelled_breaks_and_continues_from(stmt_loc.start);
        Ok(self.alloc_with(|| Statement::ForStatement {
            init,
            test,
            update,
            block: stmt,
            loc: SourceLocation::from_parts(for_token.loc, stmt_loc),
        }))
    }

    pub fn for_expression(
        &self,
        expr: Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) -> Option<VariableDeclarationOrExpression<'alloc>> {
        expr.map(|expr| VariableDeclarationOrExpression::Expression(expr))
    }

    pub fn for_var_declaration(
        &mut self,
        var_token: arena::Box<'alloc, Token>,
        declarators: arena::Box<'alloc, arena::Vec<'alloc, VariableDeclarator<'alloc>>>,
    ) -> VariableDeclarationOrExpression<'alloc> {
        let var_loc = var_token.loc;
        let declarator_loc = declarators
            .last()
            .expect("There should be at least one declarator")
            .get_loc();

        self.mark_binding_kind(var_loc.start, Some(declarator_loc.end), BindingKind::Var);

        VariableDeclarationOrExpression::VariableDeclaration(VariableDeclaration {
            kind: VariableDeclarationKind::Var { loc: var_loc },
            declarators: declarators.unbox(),
            loc: SourceLocation::from_parts(var_loc, declarator_loc),
        })
    }

    pub fn unbox_for_lexical_declaration(
        &self,
        declaration: arena::Box<'alloc, VariableDeclarationOrExpression<'alloc>>,
    ) -> VariableDeclarationOrExpression<'alloc> {
        declaration.unbox()
    }

    // IterationStatement : `for` `(` [lookahead != 'let'] LeftHandSideExpression `in` Expression `)` Statement
    // IterationStatement : `for` `(` `var` ForBinding `in` Expression `)` Statement
    //
    // Annex B: Initializers in ForIn Statement Heads
    // https://tc39.es/ecma262/#sec-initializers-in-forin-statement-heads
    //
    // IterationStatement :  `for` `(` `var` BindingIdentifier Initializer `in` Expression `)` Statement
    pub fn for_in_statement(
        &mut self,
        for_token: arena::Box<'alloc, Token>,
        left: VariableDeclarationOrAssignmentTarget<'alloc>,
        right: arena::Box<'alloc, Expression<'alloc>>,
        stmt: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        self.check_single_statement(&stmt)?;
        self.for_in_statement_common(for_token, left, right, stmt)
    }

    // IterationStatement : `for` `(` ForDeclaration `in` Expression `)` Statement
    pub fn for_in_statement_lexical(
        &mut self,
        for_token: arena::Box<'alloc, Token>,
        left: VariableDeclarationOrAssignmentTarget<'alloc>,
        right: arena::Box<'alloc, Expression<'alloc>>,
        stmt: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        self.check_single_statement(&stmt)?;
        self.check_lexical_for_bindings(&left.get_loc())?;
        self.for_in_statement_common(for_token, left, right, stmt)
    }

    pub fn for_in_statement_common(
        &mut self,
        for_token: arena::Box<'alloc, Token>,
        left: VariableDeclarationOrAssignmentTarget<'alloc>,
        right: arena::Box<'alloc, Expression<'alloc>>,
        stmt: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        let stmt_loc = stmt.get_loc();
        self.pop_unlabelled_breaks_and_continues_from(stmt_loc.start);
        Ok(self.alloc_with(|| Statement::ForInStatement {
            left,
            right,
            block: stmt,
            loc: SourceLocation::from_parts(for_token.loc, stmt_loc),
        }))
    }

    pub fn for_in_or_of_var_declaration(
        &mut self,
        var_token: arena::Box<'alloc, Token>,
        binding: arena::Box<'alloc, Binding<'alloc>>,
        init: Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) -> VariableDeclarationOrAssignmentTarget<'alloc> {
        let var_loc = var_token.loc;
        let binding_loc = binding.get_loc();
        let decl_loc = match init {
            Some(ref init) => SourceLocation::from_parts(binding_loc, init.get_loc()),
            None => binding_loc,
        };

        self.mark_binding_kind(binding_loc.start, Some(binding_loc.end), BindingKind::Var);

        VariableDeclarationOrAssignmentTarget::VariableDeclaration(VariableDeclaration {
            kind: VariableDeclarationKind::Var { loc: var_loc },
            declarators: self.new_vec_single(VariableDeclarator {
                binding: binding.unbox(),
                init,
                loc: decl_loc,
            }),
            loc: SourceLocation::from_parts(var_loc, decl_loc),
        })
    }

    pub fn for_assignment_target(
        &self,
        expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, VariableDeclarationOrAssignmentTarget<'alloc>> {
        Ok(VariableDeclarationOrAssignmentTarget::AssignmentTarget(
            self.expression_to_assignment_target(expression)?,
        ))
    }

    pub fn unbox_for_declaration(
        &self,
        declaration: arena::Box<'alloc, VariableDeclarationOrAssignmentTarget<'alloc>>,
    ) -> VariableDeclarationOrAssignmentTarget<'alloc> {
        declaration.unbox()
    }

    // IterationStatement : `for` `(` [lookahead != 'let'] LeftHandSideExpression `of` AssignmentExpression `)` Statement
    // IterationStatement : `for` `(` `var` ForBinding `of` AssignmentExpression `)` Statement
    pub fn for_of_statement(
        &mut self,
        for_token: arena::Box<'alloc, Token>,
        left: VariableDeclarationOrAssignmentTarget<'alloc>,
        right: arena::Box<'alloc, Expression<'alloc>>,
        stmt: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        self.check_single_statement(&stmt)?;
        self.for_of_statement_common(for_token, left, right, stmt)
    }

    // IterationStatement : `for` `(` ForDeclaration `of` AssignmentExpression `)` Statement
    pub fn for_of_statement_lexical(
        &mut self,
        for_token: arena::Box<'alloc, Token>,
        left: VariableDeclarationOrAssignmentTarget<'alloc>,
        right: arena::Box<'alloc, Expression<'alloc>>,
        stmt: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        self.check_single_statement(&stmt)?;
        self.check_lexical_for_bindings(&left.get_loc())?;
        self.for_of_statement_common(for_token, left, right, stmt)
    }

    pub fn for_of_statement_common(
        &mut self,
        for_token: arena::Box<'alloc, Token>,
        left: VariableDeclarationOrAssignmentTarget<'alloc>,
        right: arena::Box<'alloc, Expression<'alloc>>,
        stmt: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        let stmt_loc = stmt.get_loc();
        self.pop_unlabelled_breaks_and_continues_from(stmt_loc.start);
        Ok(self.alloc_with(|| Statement::ForOfStatement {
            left,
            right,
            block: stmt,
            loc: SourceLocation::from_parts(for_token.loc, stmt_loc),
        }))
    }

    // IterationStatement : `for` `await` `(` [lookahead != 'let'] LeftHandSideExpression `of` AssignmentExpression `)` Statement
    // IterationStatement : `for` `await` `(` `var` ForBinding `of` AssignmentExpression `)` Statement
    pub fn for_await_of_statement(
        &self,
        for_token: arena::Box<'alloc, Token>,
        left: VariableDeclarationOrAssignmentTarget,
        right: arena::Box<'alloc, Expression<'alloc>>,
        stmt: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        self.check_single_statement(&stmt)?;
        self.for_await_of_statement_common(for_token, left, right, stmt)
    }

    // IterationStatement : `for` `await` `(` ForDeclaration `of` AssignmentExpression `)` Statement
    pub fn for_await_of_statement_lexical(
        &mut self,
        for_token: arena::Box<'alloc, Token>,
        left: VariableDeclarationOrAssignmentTarget,
        right: arena::Box<'alloc, Expression<'alloc>>,
        stmt: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        self.check_single_statement(&stmt)?;
        self.check_lexical_for_bindings(&left.get_loc())?;
        self.for_await_of_statement_common(for_token, left, right, stmt)
    }

    pub fn for_await_of_statement_common(
        &self,
        _for_token: arena::Box<'alloc, Token>,
        _left: VariableDeclarationOrAssignmentTarget,
        _right: arena::Box<'alloc, Expression<'alloc>>,
        _stmt: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        Err(ParseError::NotImplemented(
            "for await statement (missing from AST)",
        ))
    }

    // ForDeclaration : LetOrConst ForBinding => ForDeclaration($0, $1)
    pub fn for_declaration(
        &mut self,
        kind: arena::Box<'alloc, VariableDeclarationKind>,
        binding: arena::Box<'alloc, Binding<'alloc>>,
    ) -> arena::Box<'alloc, VariableDeclarationOrAssignmentTarget<'alloc>> {
        let binding_kind = match &*kind {
            VariableDeclarationKind::Let { .. } => BindingKind::Let,
            VariableDeclarationKind::Const { .. } => BindingKind::Const,
            _ => panic!("unexpected VariableDeclarationKind"),
        };

        self.mark_binding_kind(kind.get_loc().start, None, binding_kind);

        let kind_loc = kind.get_loc();
        let binding_loc = binding.get_loc();
        self.alloc_with(|| {
            VariableDeclarationOrAssignmentTarget::VariableDeclaration(VariableDeclaration {
                kind: kind.unbox(),
                declarators: self.new_vec_single(VariableDeclarator {
                    binding: binding.unbox(),
                    init: None,
                    loc: binding_loc,
                }),
                loc: SourceLocation::from_parts(kind_loc, binding_loc),
            })
        })
    }

    // CatchParameter : BindingIdentifier
    // ForBinding : BindingIdentifier
    // LexicalBinding : BindingIdentifier Initializer?
    // VariableDeclaration : BindingIdentifier Initializer?
    pub fn binding_identifier_to_binding(
        &self,
        identifier: arena::Box<'alloc, BindingIdentifier>,
    ) -> arena::Box<'alloc, Binding<'alloc>> {
        self.alloc_with(|| Binding::BindingIdentifier(identifier.unbox()))
    }

    // ContinueStatement : `continue` `;`
    // ContinueStatement : `continue` LabelIdentifier `;`
    pub fn continue_statement(
        &mut self,
        continue_token: arena::Box<'alloc, Token>,
        label: Option<arena::Box<'alloc, Label>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        let info = match label {
            Some(ref label) => {
                // Label is used both for LabelledStatement and for labelled
                // ContinueStatements. A label will be noted in bindings whenever we hit
                // a label, as is the case for ContinueStatements. These bindings are
                // not necessary, and are at the end of the bindings stack. To keep things
                // clean, we will pop the last element (the label we just added) off the stack.
                let index = self.find_first_binding(continue_token.loc.start);
                self.pop_bindings_from(index);

                ControlInfo::new_continue(continue_token.loc.start, Some(label.value))
            }
            None => ControlInfo::new_continue(continue_token.loc.start, None),
        };

        self.breaks_and_continues.push(info);

        let continue_loc = continue_token.loc;
        let loc = match label {
            Some(ref label) => SourceLocation::from_parts(continue_loc, label.loc),
            None => continue_loc,
        };
        Ok(self.alloc_with(|| Statement::ContinueStatement {
            label: label.map(|boxed| boxed.unbox()),
            loc,
        }))
    }

    // BreakStatement : `break` `;`
    // BreakStatement : `break` LabelIdentifier `;`
    pub fn break_statement(
        &mut self,
        break_token: arena::Box<'alloc, Token>,
        label: Option<arena::Box<'alloc, Label>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        let info = match label {
            Some(ref label) => {
                // Label is used both for LabelledStatement and for labelled
                // BreakStatements. A label will be noted in bindings whenever we hit
                // a label, as is the case for BreakStatements. These bindings are
                // not necessary, and are at the end of the bindings stack. To keep things
                // clean, we will pop the last element (the label we just added) off the stack.
                let index = self.find_first_binding(label.loc.start);
                self.pop_bindings_from(index);

                ControlInfo::new_break(break_token.loc.start, Some(label.value))
            }
            None => ControlInfo::new_break(break_token.loc.start, None),
        };

        self.breaks_and_continues.push(info);
        let break_loc = break_token.loc;
        let loc = match label {
            Some(ref label) => SourceLocation::from_parts(break_loc, label.loc),
            None => break_loc,
        };
        Ok(self.alloc_with(|| Statement::BreakStatement {
            label: label.map(|boxed| boxed.unbox()),
            loc,
        }))
    }

    // ReturnStatement : `return` `;`
    // ReturnStatement : `return` Expression `;`
    pub fn return_statement(
        &self,
        return_token: arena::Box<'alloc, Token>,
        expression: Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) -> arena::Box<'alloc, Statement<'alloc>> {
        let return_loc = return_token.loc;
        let loc = match expression {
            Some(ref expression) => SourceLocation::from_parts(return_loc, expression.get_loc()),
            None => return_loc,
        };
        self.alloc_with(|| Statement::ReturnStatement { expression, loc })
    }

    // WithStatement : `with` `(` Expression `)` Statement
    pub fn with_statement(
        &self,
        with_token: arena::Box<'alloc, Token>,
        object: arena::Box<'alloc, Expression<'alloc>>,
        body: arena::Box<'alloc, Statement<'alloc>>,
    ) -> arena::Box<'alloc, Statement<'alloc>> {
        let body_loc = body.get_loc();
        self.alloc_with(|| Statement::WithStatement {
            object,
            body,
            loc: SourceLocation::from_parts(with_token.loc, body_loc),
        })
    }

    // SwitchStatement : `switch` `(` Expression `)` CaseBlock
    pub fn switch_statement(
        &self,
        switch_token: arena::Box<'alloc, Token>,
        discriminant_expr: arena::Box<'alloc, Expression<'alloc>>,
        mut cases: arena::Box<'alloc, Statement<'alloc>>,
    ) -> arena::Box<'alloc, Statement<'alloc>> {
        match &mut *cases {
            Statement::SwitchStatement {
                discriminant, loc, ..
            } => {
                *discriminant = discriminant_expr;
                (*loc).start = switch_token.loc.start;
            }
            Statement::SwitchStatementWithDefault {
                discriminant, loc, ..
            } => {
                *discriminant = discriminant_expr;
                (*loc).start = switch_token.loc.start;
            }
            _ => {
                // The grammar ensures that the parser always passes a valid
                // argument to this method.
                panic!("invalid argument: argument 2 must be a SwitchStatement");
            }
        }
        cases
    }

    // CaseBlock : `{` CaseClauses? `}`
    pub fn case_block(
        &mut self,
        open_token: arena::Box<'alloc, Token>,
        cases: Option<arena::Box<'alloc, arena::Vec<'alloc, SwitchCase<'alloc>>>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        self.check_case_block_binding(open_token.loc.start)?;

        Ok(self.alloc_with(|| Statement::SwitchStatement {
            // This will be overwritten once the enclosing switch statement
            // gets parsed.
            discriminant: self.alloc_with(|| Expression::LiteralNullExpression {
                loc: SourceLocation::default(),
            }),
            cases: match cases {
                None => self.new_vec(),
                Some(boxed) => boxed.unbox(),
            },
            // `start` of this will be overwritten once the enclosing switch
            // statement gets parsed.
            loc: close_token.loc,
        }))
    }

    // CaseBlock : `{` CaseClauses DefaultClause CaseClauses `}`
    pub fn case_block_with_default(
        &mut self,
        open_token: arena::Box<'alloc, Token>,
        pre_default_cases: Option<arena::Box<'alloc, arena::Vec<'alloc, SwitchCase<'alloc>>>>,
        default_case: arena::Box<'alloc, SwitchDefault<'alloc>>,
        post_default_cases: Option<arena::Box<'alloc, arena::Vec<'alloc, SwitchCase<'alloc>>>>,
        close_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        self.check_case_block_binding(open_token.loc.start)?;

        Ok(self.alloc_with(|| Statement::SwitchStatementWithDefault {
            // This will be overwritten once the enclosing switch statement
            // gets parsed.
            discriminant: self.alloc_with(|| Expression::LiteralNullExpression {
                loc: SourceLocation::default(),
            }),
            pre_default_cases: match pre_default_cases {
                None => self.new_vec(),
                Some(boxed) => boxed.unbox(),
            },
            default_case: default_case.unbox(),
            post_default_cases: match post_default_cases {
                None => self.new_vec(),
                Some(boxed) => boxed.unbox(),
            },
            // `start` of this will be overwritten once the enclosing switch
            // statement gets parsed.
            loc: close_token.loc,
        }))
    }

    // CaseClauses : CaseClause
    pub fn case_clauses_single(
        &self,
        case: arena::Box<'alloc, SwitchCase<'alloc>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, SwitchCase<'alloc>>> {
        self.alloc_with(|| self.new_vec_single(case.unbox()))
    }

    // CaseClauses : CaseClauses CaseClause
    pub fn case_clauses_append(
        &self,
        mut cases: arena::Box<'alloc, arena::Vec<'alloc, SwitchCase<'alloc>>>,
        case: arena::Box<'alloc, SwitchCase<'alloc>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, SwitchCase<'alloc>>> {
        self.push(&mut cases, case.unbox());
        cases
    }

    // CaseClause : `case` Expression `:` StatementList
    pub fn case_clause(
        &self,
        case_token: arena::Box<'alloc, Token>,
        expression: arena::Box<'alloc, Expression<'alloc>>,
        colon_token: arena::Box<'alloc, Token>,
        statements: Option<arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>>>,
    ) -> arena::Box<'alloc, SwitchCase<'alloc>> {
        let case_loc = case_token.loc;
        if let Some(statements) = statements {
            let statement_loc = statements
                .last()
                .expect("There should be at least one statement")
                .get_loc();

            self.alloc_with(|| SwitchCase {
                test: expression,
                consequent: statements.unbox(),
                loc: SourceLocation::from_parts(case_loc, statement_loc),
            })
        } else {
            self.alloc_with(|| SwitchCase {
                test: expression,
                consequent: self.new_vec(),
                loc: SourceLocation::from_parts(case_loc, colon_token.loc),
            })
        }
    }

    // DefaultClause : `default` `:` StatementList
    pub fn default_clause(
        &self,
        default_token: arena::Box<'alloc, Token>,
        colon_token: arena::Box<'alloc, Token>,
        statements: Option<arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>>>,
    ) -> arena::Box<'alloc, SwitchDefault<'alloc>> {
        let default_loc = default_token.loc;
        if let Some(statements) = statements {
            let statement_loc = statements
                .last()
                .expect("There should be at least one statement")
                .get_loc();

            self.alloc_with(|| SwitchDefault {
                consequent: statements.unbox(),
                loc: SourceLocation::from_parts(default_loc, statement_loc),
            })
        } else {
            self.alloc_with(|| SwitchDefault {
                consequent: self.new_vec(),
                loc: SourceLocation::from_parts(default_loc, colon_token.loc),
            })
        }
    }

    // LabelledStatement : LabelIdentifier `:` LabelledItem
    pub fn labelled_statement(
        &mut self,
        label: arena::Box<'alloc, Label>,
        body: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Statement<'alloc>>> {
        let label_loc = label.loc;
        let body_loc = body.get_loc();
        self.check_labelled_statement(&label, &body)?;
        Ok(self.alloc_with(|| Statement::LabelledStatement {
            label: label.unbox(),
            body,
            loc: SourceLocation::from_parts(label_loc, body_loc),
        }))
    }

    // ThrowStatement : `throw` Expression `;`
    pub fn throw_statement(
        &self,
        throw_token: arena::Box<'alloc, Token>,
        expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Statement<'alloc>> {
        let expression_loc = expression.get_loc();
        self.alloc_with(|| Statement::ThrowStatement {
            expression,
            loc: SourceLocation::from_parts(throw_token.loc, expression_loc),
        })
    }

    // TryStatement : `try` Block Catch
    // TryStatement : `try` Block Finally
    // TryStatement : `try` Block Catch Finally
    pub fn try_statement(
        &self,
        try_token: arena::Box<'alloc, Token>,
        body: arena::Box<'alloc, Block<'alloc>>,
        catch_clause: Option<arena::Box<'alloc, CatchClause<'alloc>>>,
        finally_block: Option<arena::Box<'alloc, Block<'alloc>>>,
    ) -> arena::Box<'alloc, Statement<'alloc>> {
        let try_loc = try_token.loc;
        match (catch_clause, finally_block) {
            (Some(catch_clause), None) => {
                let catch_clause_loc = catch_clause.loc;
                self.alloc_with(|| Statement::TryCatchStatement {
                    body: body.unbox(),
                    catch_clause: catch_clause.unbox(),
                    loc: SourceLocation::from_parts(try_loc, catch_clause_loc),
                })
            }
            (catch_clause, Some(finally_block)) => {
                let finally_block_loc = finally_block.loc;
                self.alloc_with(|| Statement::TryFinallyStatement {
                    body: body.unbox(),
                    catch_clause: catch_clause.map(|boxed| boxed.unbox()),
                    finalizer: finally_block.unbox(),
                    loc: SourceLocation::from_parts(try_loc, finally_block_loc),
                })
            }
            _ => {
                // The grammar won't accept a bare try-block, so the parser always
                // a catch clause, a finally block, or both.
                panic!("invalid argument: try_statement requires a catch or finally block");
            }
        }
    }

    // Catch : `catch` `(` CatchParameter `)` Block
    pub fn catch(
        &mut self,
        catch_token: arena::Box<'alloc, Token>,
        binding: arena::Box<'alloc, Binding<'alloc>>,
        body: arena::Box<'alloc, Block<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, CatchClause<'alloc>>> {
        let catch_loc = catch_token.loc;
        let body_loc = body.loc;

        let is_simple = match &*binding {
            Binding::BindingIdentifier(_) => true,
            _ => false,
        };

        self.check_catch_bindings(is_simple, &binding.get_loc())?;

        Ok(self.alloc_with(|| CatchClause {
            binding: Some(binding),
            body: body.unbox(),
            loc: SourceLocation::from_parts(catch_loc, body_loc),
        }))
    }

    // Catch : `catch` `(` CatchParameter `)` Block
    pub fn catch_no_param(
        &mut self,
        catch_token: arena::Box<'alloc, Token>,
        body: arena::Box<'alloc, Block<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, CatchClause<'alloc>>> {
        let catch_loc = catch_token.loc;
        let body_loc = body.loc;

        self.check_catch_no_param_bindings(catch_loc.start)?;

        Ok(self.alloc_with(|| CatchClause {
            binding: None,
            body: body.unbox(),
            loc: SourceLocation::from_parts(catch_loc, body_loc),
        }))
    }

    // DebuggerStatement : `debugger` `;`
    pub fn debugger_statement(
        &self,
        token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, Statement<'alloc>> {
        self.alloc_with(|| Statement::DebuggerStatement { loc: token.loc })
    }

    pub fn function_decl(&mut self, f: Function<'alloc>) -> arena::Box<'alloc, Statement<'alloc>> {
        self.mark_binding_kind(f.loc.start, None, BindingKind::Function);

        self.alloc_with(|| Statement::FunctionDeclaration(f))
    }

    pub fn async_or_generator_decl(
        &mut self,
        f: Function<'alloc>,
    ) -> arena::Box<'alloc, Statement<'alloc>> {
        self.mark_binding_kind(f.loc.start, None, BindingKind::AsyncOrGenerator);

        self.alloc_with(|| Statement::FunctionDeclaration(f))
    }

    pub fn function_expr(&mut self, f: Function<'alloc>) -> arena::Box<'alloc, Expression<'alloc>> {
        let index = self.find_first_binding(f.loc.start);
        self.pop_bindings_from(index);

        self.alloc_with(|| Expression::FunctionExpression(f))
    }

    // FunctionDeclaration : `function` BindingIdentifier `(` FormalParameters `)` `{` FunctionBody `}`
    // FunctionDeclaration : [+Default] `function` `(` FormalParameters `)` `{` FunctionBody `}`
    // FunctionExpression : `function` BindingIdentifier? `(` FormalParameters `)` `{` FunctionBody `}`
    pub fn function(
        &mut self,
        function_token: arena::Box<'alloc, Token>,
        name: Option<arena::Box<'alloc, BindingIdentifier>>,
        param_open_token: arena::Box<'alloc, Token>,
        mut params: arena::Box<'alloc, FormalParameters<'alloc>>,
        param_close_token: arena::Box<'alloc, Token>,
        body_open_token: arena::Box<'alloc, Token>,
        mut body: arena::Box<'alloc, FunctionBody<'alloc>>,
        body_close_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, Function<'alloc>> {
        let param_open_loc = param_open_token.loc;
        let param_close_loc = param_close_token.loc;
        let body_close_loc = body_close_token.loc;

        let is_simple = Self::is_params_simple(&params);
        self.check_function_bindings(is_simple, param_open_loc.start, param_close_loc.end)?;

        params.loc.set_range(param_open_loc, param_close_loc);
        body.loc.set_range(body_open_token.loc, body_close_loc);

        Ok(Function {
            name: name.map(|b| b.unbox()),
            is_async: false,
            is_generator: false,
            params: params.unbox(),
            body: body.unbox(),
            loc: SourceLocation::from_parts(function_token.loc, body_close_loc),
        })
    }

    // AsyncFunctionDeclaration : `async` `function` BindingIdentifier `(` FormalParameters `)` `{` AsyncFunctionBody `}`
    // AsyncFunctionDeclaration : [+Default] `async` `function` `(` FormalParameters `)` `{` AsyncFunctionBody `}`
    // AsyncFunctionExpression : `async` `function` `(` FormalParameters `)` `{` AsyncFunctionBody `}`
    pub fn async_function(
        &mut self,
        async_token: arena::Box<'alloc, Token>,
        name: Option<arena::Box<'alloc, BindingIdentifier>>,
        param_open_token: arena::Box<'alloc, Token>,
        mut params: arena::Box<'alloc, FormalParameters<'alloc>>,
        param_close_token: arena::Box<'alloc, Token>,
        body_open_token: arena::Box<'alloc, Token>,
        mut body: arena::Box<'alloc, FunctionBody<'alloc>>,
        body_close_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, Function<'alloc>> {
        let param_open_loc = param_open_token.loc;
        let param_close_loc = param_close_token.loc;
        let body_close_loc = body_close_token.loc;

        let is_simple = Self::is_params_simple(&params);
        self.check_function_bindings(is_simple, param_open_loc.start, param_close_loc.end)?;

        params.loc.set_range(param_open_loc, param_close_loc);
        body.loc.set_range(body_open_token.loc, body_close_loc);

        Ok(Function {
            name: name.map(|b| b.unbox()),
            is_async: true,
            is_generator: false,
            params: params.unbox(),
            body: body.unbox(),
            loc: SourceLocation::from_parts(async_token.loc, body_close_loc),
        })
    }

    // GeneratorDeclaration : `function` `*` BindingIdentifier `(` FormalParameters `)` `{` GeneratorBody `}`
    // GeneratorDeclaration : [+Default] `function` `*` `(` FormalParameters `)` `{` GeneratorBody `}`
    // GeneratorExpression : `function` `*` BindingIdentifier? `(` FormalParameters `)` `{` GeneratorBody `}`
    pub fn generator(
        &mut self,
        function_token: arena::Box<'alloc, Token>,
        name: Option<arena::Box<'alloc, BindingIdentifier>>,
        param_open_token: arena::Box<'alloc, Token>,
        mut params: arena::Box<'alloc, FormalParameters<'alloc>>,
        param_close_token: arena::Box<'alloc, Token>,
        body_open_token: arena::Box<'alloc, Token>,
        mut body: arena::Box<'alloc, FunctionBody<'alloc>>,
        body_close_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, Function<'alloc>> {
        let param_open_loc = param_open_token.loc;
        let param_close_loc = param_close_token.loc;
        let body_close_loc = body_close_token.loc;

        let is_simple = Self::is_params_simple(&params);
        self.check_function_bindings(is_simple, param_open_loc.start, param_close_loc.end)?;

        params.loc.set_range(param_open_loc, param_close_loc);
        body.loc.set_range(body_open_token.loc, body_close_loc);

        Ok(Function {
            name: name.map(|b| b.unbox()),
            is_async: false,
            is_generator: true,
            params: params.unbox(),
            body: body.unbox(),
            loc: SourceLocation::from_parts(function_token.loc, body_close_loc),
        })
    }

    // AsyncGeneratorDeclaration : `async` `function` `*` BindingIdentifier `(` FormalParameters `)` `{` AsyncGeneratorBody `}`
    // AsyncGeneratorDeclaration : [+Default] `async` `function` `*` `(` FormalParameters `)` `{` AsyncGeneratorBody `}`
    // AsyncGeneratorExpression : `async` `function` `*` BindingIdentifier? `(` FormalParameters `)` `{` AsyncGeneratorBody `}`
    pub fn async_generator(
        &mut self,
        async_token: arena::Box<'alloc, Token>,
        name: Option<arena::Box<'alloc, BindingIdentifier>>,
        param_open_token: arena::Box<'alloc, Token>,
        mut params: arena::Box<'alloc, FormalParameters<'alloc>>,
        param_close_token: arena::Box<'alloc, Token>,
        body_open_token: arena::Box<'alloc, Token>,
        mut body: arena::Box<'alloc, FunctionBody<'alloc>>,
        body_close_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, Function<'alloc>> {
        let param_open_loc = param_open_token.loc;
        let param_close_loc = param_close_token.loc;
        let body_close_loc = body_close_token.loc;

        let is_simple = Self::is_params_simple(&params);
        self.check_function_bindings(is_simple, param_open_loc.start, param_close_loc.end)?;

        params.loc.set_range(param_open_loc, param_close_loc);
        body.loc.set_range(body_open_token.loc, body_close_loc);

        Ok(Function {
            name: name.map(|b| b.unbox()),
            is_async: true,
            is_generator: true,
            params: params.unbox(),
            body: body.unbox(),
            loc: SourceLocation::from_parts(async_token.loc, body_close_loc),
        })
    }

    // UniqueFormalParameters : FormalParameters
    pub fn unique_formal_parameters(
        &self,
        parameters: arena::Box<'alloc, FormalParameters<'alloc>>,
    ) -> arena::Box<'alloc, FormalParameters<'alloc>> {
        parameters
    }

    // FormalParameters : [empty]
    pub fn empty_formal_parameters(&self) -> arena::Box<'alloc, FormalParameters<'alloc>> {
        self.alloc_with(|| FormalParameters {
            items: self.new_vec(),
            rest: None,
            // This will be overwritten once the enclosing function gets parsed.
            loc: SourceLocation::default(),
        })
    }

    // FormalParameters : FunctionRestParameter
    // FormalParameters : FormalParameterList `,` FunctionRestParameter
    pub fn with_rest_parameter(
        &self,
        mut params: arena::Box<'alloc, FormalParameters<'alloc>>,
        rest: arena::Box<'alloc, Binding<'alloc>>,
    ) -> arena::Box<'alloc, FormalParameters<'alloc>> {
        params.rest = Some(rest.unbox());
        params
    }

    // FormalParameterList : FormalParameter
    pub fn formal_parameter_list_single(
        &self,
        parameter: arena::Box<'alloc, Parameter<'alloc>>,
    ) -> arena::Box<'alloc, FormalParameters<'alloc>> {
        self.alloc_with(|| FormalParameters {
            items: self.new_vec_single(parameter.unbox()),
            rest: None,
            // This will be overwritten once the enclosing function gets parsed.
            loc: SourceLocation::default(),
        })
    }

    // FormalParameterList : FormalParameterList "," FormalParameter
    pub fn formal_parameter_list_append(
        &self,
        mut params: arena::Box<'alloc, FormalParameters<'alloc>>,
        next_param: arena::Box<'alloc, Parameter<'alloc>>,
    ) -> arena::Box<'alloc, FormalParameters<'alloc>> {
        self.push(&mut params.items, next_param.unbox());
        params
    }

    // FunctionBody : FunctionStatementList
    pub fn function_body(
        &self,
        statements: arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>>,
    ) -> arena::Box<'alloc, FunctionBody<'alloc>> {
        // TODO: Directives
        self.alloc_with(|| FunctionBody {
            directives: self.new_vec(),
            statements: statements.unbox(),
            // This will be overwritten once the enclosing function gets parsed.
            loc: SourceLocation::default(),
        })
    }

    // FunctionStatementList : StatementList?
    pub fn function_statement_list(
        &self,
        statements: Option<arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>> {
        match statements {
            Some(statements) => statements,
            None => self.alloc_with(|| self.new_vec()),
        }
    }

    // ArrowFunction : ArrowParameters `=>` ConciseBody
    pub fn arrow_function(
        &mut self,
        params: arena::Box<'alloc, FormalParameters<'alloc>>,
        body: arena::Box<'alloc, ArrowExpressionBody<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Expression<'alloc>>> {
        self.check_unique_function_bindings(params.loc.start, params.loc.end)?;

        let params_loc = params.loc;
        let body_loc = body.get_loc();

        Ok(self.alloc_with(|| Expression::ArrowExpression {
            is_async: false,
            params: params.unbox(),
            body: body.unbox(),
            loc: SourceLocation::from_parts(params_loc, body_loc),
        }))
    }

    // ArrowParameters : BindingIdentifier
    pub fn arrow_parameters_bare(
        &mut self,
        identifier: arena::Box<'alloc, BindingIdentifier>,
    ) -> arena::Box<'alloc, FormalParameters<'alloc>> {
        let loc = identifier.loc;
        self.alloc_with(|| FormalParameters {
            items: self.new_vec_single(Parameter::Binding(Binding::BindingIdentifier(
                identifier.unbox(),
            ))),
            rest: None,
            loc,
        })
    }

    // ArrowParameters : CoverParenthesizedExpressionAndArrowParameterList
    pub fn uncover_arrow_parameters(
        &self,
        covered: arena::Box<'alloc, CoverParenthesized<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, FormalParameters<'alloc>>> {
        Ok(match covered.unbox() {
            CoverParenthesized::Expression { expression, loc } => self.alloc(FormalParameters {
                items: self.expression_to_parameter_list(expression)?,
                rest: None,
                loc,
            }),
            CoverParenthesized::Parameters(parameters) => parameters,
        })
    }

    // ConciseBody : [lookahead != `{`] AssignmentExpression
    pub fn concise_body_expression(
        &self,
        expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, ArrowExpressionBody<'alloc>> {
        self.alloc_with(|| ArrowExpressionBody::Expression(expression))
    }

    // ConciseBody : `{` FunctionBody `}`
    pub fn concise_body_block(
        &self,
        body_open_token: arena::Box<'alloc, Token>,
        mut body: arena::Box<'alloc, FunctionBody<'alloc>>,
        body_close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, ArrowExpressionBody<'alloc>> {
        body.loc
            .set_range(body_open_token.loc, body_close_token.loc);
        self.alloc_with(|| ArrowExpressionBody::FunctionBody(body.unbox()))
    }

    // MethodDefinition : PropertyName `(` UniqueFormalParameters `)` `{` FunctionBody `}`
    pub fn method_definition(
        &mut self,
        name: arena::Box<'alloc, PropertyName<'alloc>>,
        param_open_token: arena::Box<'alloc, Token>,
        mut params: arena::Box<'alloc, FormalParameters<'alloc>>,
        param_close_token: arena::Box<'alloc, Token>,
        body_open_token: arena::Box<'alloc, Token>,
        mut body: arena::Box<'alloc, FunctionBody<'alloc>>,
        body_close_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, MethodDefinition<'alloc>>> {
        let name_loc = name.get_loc();
        let param_open_loc = param_open_token.loc;
        let param_close_loc = param_close_token.loc;
        let body_close_loc = body_close_token.loc;

        self.check_unique_function_bindings(param_open_loc.start, param_close_loc.end)?;

        params.loc.set_range(param_open_loc, param_close_loc);
        body.loc.set_range(body_open_token.loc, body_close_loc);

        Ok(self.alloc_with(|| {
            MethodDefinition::Method(Method {
                name: name.unbox(),
                is_async: false,
                is_generator: false,
                params: params.unbox(),
                body: body.unbox(),
                loc: SourceLocation::from_parts(name_loc, body_close_loc),
            })
        }))
    }

    // MethodDefinition : `get` PropertyName `(` `)` `{` FunctionBody `}`
    pub fn getter(
        &self,
        get_token: arena::Box<'alloc, Token>,
        name: arena::Box<'alloc, PropertyName<'alloc>>,
        body_open_token: arena::Box<'alloc, Token>,
        mut body: arena::Box<'alloc, FunctionBody<'alloc>>,
        body_close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, MethodDefinition<'alloc>> {
        let body_close_loc = body_close_token.loc;
        body.loc.set_range(body_open_token.loc, body_close_loc);
        self.alloc_with(|| {
            MethodDefinition::Getter(Getter {
                property_name: name.unbox(),
                body: body.unbox(),
                loc: SourceLocation::from_parts(get_token.loc, body_close_loc),
            })
        })
    }

    // MethodDefinition : `set` PropertyName `(` PropertySetParameterList `)` `{` FunctionBody `}`
    pub fn setter(
        &mut self,
        set_token: arena::Box<'alloc, Token>,
        name: arena::Box<'alloc, PropertyName<'alloc>>,
        param_open_token: arena::Box<'alloc, Token>,
        mut parameter: arena::Box<'alloc, Parameter<'alloc>>,
        param_close_token: arena::Box<'alloc, Token>,
        body_open_token: arena::Box<'alloc, Token>,
        mut body: arena::Box<'alloc, FunctionBody<'alloc>>,
        body_close_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, MethodDefinition<'alloc>>> {
        let param_open_loc = param_open_token.loc;
        let param_close_loc = param_close_token.loc;
        let body_close_loc = body_close_token.loc;

        // A setter only has one parameter, but it can be a destructuring
        // pattern, so it is still possible to flunk this check.
        self.check_unique_function_bindings(param_open_loc.start, param_close_loc.end)?;

        parameter.set_loc(param_open_loc, param_close_loc);
        body.loc.set_range(body_open_token.loc, body_close_loc);
        Ok(self.alloc_with(|| {
            MethodDefinition::Setter(Setter {
                property_name: name.unbox(),
                param: parameter.unbox(),
                body: body.unbox(),
                loc: SourceLocation::from_parts(set_token.loc, body_close_loc),
            })
        }))
    }

    // GeneratorMethod : `*` PropertyName `(` UniqueFormalParameters `)` `{` GeneratorBody `}`
    pub fn generator_method(
        &mut self,
        generator_token: arena::Box<'alloc, Token>,
        name: arena::Box<'alloc, PropertyName<'alloc>>,
        param_open_token: arena::Box<'alloc, Token>,
        mut params: arena::Box<'alloc, FormalParameters<'alloc>>,
        param_close_token: arena::Box<'alloc, Token>,
        body_open_token: arena::Box<'alloc, Token>,
        mut body: arena::Box<'alloc, FunctionBody<'alloc>>,
        body_close_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, MethodDefinition<'alloc>>> {
        let param_open_loc = param_open_token.loc;
        let param_close_loc = param_close_token.loc;
        let body_close_loc = body_close_token.loc;

        self.check_unique_function_bindings(param_open_loc.start, param_close_loc.end)?;

        params.loc.set_range(param_open_loc, param_close_loc);
        body.loc.set_range(body_open_token.loc, body_close_loc);

        Ok(self.alloc_with(|| {
            MethodDefinition::Method(Method {
                name: name.unbox(),
                is_async: false,
                is_generator: true,
                params: params.unbox(),
                body: body.unbox(),
                loc: SourceLocation::from_parts(generator_token.loc, body_close_loc),
            })
        }))
    }

    // YieldExpression : `yield`
    // YieldExpression : `yield` AssignmentExpression
    pub fn yield_expr(
        &self,
        yield_token: arena::Box<'alloc, Token>,
        operand: Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let yield_loc = yield_token.loc;
        let loc = match operand {
            Some(ref operand) => SourceLocation::from_parts(yield_loc, operand.get_loc()),
            None => yield_loc,
        };

        self.alloc_with(|| Expression::YieldExpression {
            expression: operand,
            loc,
        })
    }

    // YieldExpression : `yield` `*` AssignmentExpression
    pub fn yield_star_expr(
        &self,
        yield_token: arena::Box<'alloc, Token>,
        operand: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let yield_loc = yield_token.loc;
        let operand_loc = operand.get_loc();
        self.alloc_with(|| Expression::YieldGeneratorExpression {
            expression: operand,
            loc: SourceLocation::from_parts(yield_loc, operand_loc),
        })
    }

    // AsyncGeneratorMethod ::= "async" "*" PropertyName "(" UniqueFormalParameters ")" "{" AsyncGeneratorBody "}"
    pub fn async_generator_method(
        &mut self,
        async_token: arena::Box<'alloc, Token>,
        name: arena::Box<'alloc, PropertyName<'alloc>>,
        param_open_token: arena::Box<'alloc, Token>,
        mut params: arena::Box<'alloc, FormalParameters<'alloc>>,
        param_close_token: arena::Box<'alloc, Token>,
        body_open_token: arena::Box<'alloc, Token>,
        mut body: arena::Box<'alloc, FunctionBody<'alloc>>,
        body_close_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, MethodDefinition<'alloc>>> {
        let param_open_loc = param_open_token.loc;
        let param_close_loc = param_close_token.loc;
        let body_close_loc = body_close_token.loc;

        self.check_unique_function_bindings(param_open_loc.start, param_close_loc.end)?;

        params.loc.set_range(param_open_loc, param_close_loc);
        body.loc.set_range(body_open_token.loc, body_close_loc);

        Ok(self.alloc_with(|| {
            MethodDefinition::Method(Method {
                name: name.unbox(),
                is_async: true,
                is_generator: true,
                params: params.unbox(),
                body: body.unbox(),
                loc: SourceLocation::from_parts(async_token.loc, body_close_loc),
            })
        }))
    }

    // ClassDeclaration : `class` BindingIdentifier ClassTail
    // ClassDeclaration : `class` ClassTail
    pub fn class_declaration(
        &mut self,
        class_token: arena::Box<'alloc, Token>,
        name: Option<arena::Box<'alloc, BindingIdentifier>>,
        tail: arena::Box<'alloc, ClassExpression<'alloc>>,
    ) -> arena::Box<'alloc, Statement<'alloc>> {
        let class_loc = class_token.loc;

        self.mark_binding_kind(class_loc.start, None, BindingKind::Class);

        let tail = tail.unbox();
        let tail_loc = tail.loc;
        self.alloc_with(|| {
            Statement::ClassDeclaration(ClassDeclaration {
                name: match name {
                    None => {
                        let loc = SourceLocation::new(class_loc.end, class_loc.end);
                        BindingIdentifier {
                            name: Identifier {
                                value: CommonSourceAtomSetIndices::default(),
                                loc,
                            },
                            loc,
                        }
                    }
                    Some(bi) => bi.unbox(),
                },
                super_: tail.super_,
                elements: tail.elements,
                loc: SourceLocation::from_parts(class_loc, tail_loc),
            })
        })
    }

    // ClassExpression : `class` BindingIdentifier? ClassTail
    pub fn class_expression(
        &mut self,
        class_token: arena::Box<'alloc, Token>,
        name: Option<arena::Box<'alloc, BindingIdentifier>>,
        mut tail: arena::Box<'alloc, ClassExpression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let index = self.find_first_binding(class_token.loc.start);
        self.pop_bindings_from(index);

        tail.name = name.map(|boxed| boxed.unbox());
        tail.loc.start = class_token.loc.start;
        self.alloc_with(|| Expression::ClassExpression(tail.unbox()))
    }

    // ClassTail : ClassHeritage? `{` ClassBody? `}`
    pub fn class_tail(
        &self,
        heritage: Option<arena::Box<'alloc, Expression<'alloc>>>,
        body: Option<
            arena::Box<'alloc, arena::Vec<'alloc, arena::Box<'alloc, ClassElement<'alloc>>>>,
        >,
        body_close_token: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, ClassExpression<'alloc>> {
        self.alloc_with(|| ClassExpression {
            name: None,
            super_: heritage,
            elements: match body {
                None => self.new_vec(),
                Some(boxed) => boxed.unbox(),
            },
            // `start` of this will be overwritten once the enclosing class
            // gets parsed.
            loc: body_close_token.loc,
        })
    }

    // ClassElementList : ClassElementList ClassElement
    pub fn class_element_list_append(
        &self,
        mut list: arena::Box<'alloc, arena::Vec<'alloc, arena::Box<'alloc, ClassElement<'alloc>>>>,
        mut element: arena::Box<
            'alloc,
            arena::Vec<'alloc, arena::Box<'alloc, ClassElement<'alloc>>>,
        >,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, arena::Box<'alloc, ClassElement<'alloc>>>> {
        self.append(&mut list, &mut element);
        list
    }

    // FieldDefinition : ClassElementName Initializer?
    pub fn class_field_definition(
        &self,
        name: arena::Box<'alloc, ClassElementName<'alloc>>,
        init: Option<arena::Box<'alloc, Expression<'alloc>>>,
    ) -> arena::Box<'alloc, ClassElement<'alloc>> {
        let name_loc = name.get_loc();
        let loc = match &init {
            None => name_loc,
            Some(expr) => SourceLocation::from_parts(name_loc, expr.get_loc()),
        };
        self.alloc_with(|| ClassElement::FieldDefinition {
            name: name.unbox(),
            init,
            loc,
        })
    }

    // ClassElementName : PropertyName
    pub fn property_name_to_class_element_name(
        &self,
        name: arena::Box<'alloc, PropertyName<'alloc>>,
    ) -> arena::Box<'alloc, ClassElementName<'alloc>> {
        self.alloc_with(|| match name.unbox() {
            PropertyName::ComputedPropertyName(cpn) => ClassElementName::ComputedPropertyName(cpn),
            PropertyName::StaticPropertyName(spn) => ClassElementName::StaticPropertyName(spn),
            PropertyName::StaticNumericPropertyName(snpn) => {
                ClassElementName::StaticNumericPropertyName(snpn)
            }
        })
    }

    // ClassElementName : PrivateIdentifier
    pub fn class_element_name_private(
        &self,
        private_identifier: arena::Box<'alloc, Token>,
    ) -> arena::Box<'alloc, ClassElementName<'alloc>> {
        self.alloc_with(|| {
            ClassElementName::PrivateFieldName(self.private_identifier(private_identifier))
        })
    }

    // ClassElement : MethodDefinition
    pub fn class_element(
        &self,
        method: arena::Box<'alloc, MethodDefinition<'alloc>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, arena::Box<'alloc, ClassElement<'alloc>>>> {
        let loc = method.get_loc();
        self.class_element_to_vec(self.alloc_with(|| ClassElement::MethodDefinition {
            is_static: false,
            method: method.unbox(),
            loc,
        }))
    }

    // ClassElement : FieldDefinition `;`
    pub fn class_element_to_vec(
        &self,
        element: arena::Box<'alloc, ClassElement<'alloc>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, arena::Box<'alloc, ClassElement<'alloc>>>> {
        self.alloc_with(|| self.new_vec_single(element))
    }

    // ClassElement : `static` MethodDefinition
    pub fn class_element_static(
        &self,
        static_token: arena::Box<'alloc, Token>,
        method: arena::Box<'alloc, MethodDefinition<'alloc>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, arena::Box<'alloc, ClassElement<'alloc>>>> {
        let method_loc = method.get_loc();
        self.alloc_with(|| {
            self.new_vec_single(self.alloc_with(|| ClassElement::MethodDefinition {
                is_static: true,
                method: method.unbox(),
                loc: SourceLocation::from_parts(static_token.loc, method_loc),
            }))
        })
    }

    // ClassElement : `static` MethodDefinition
    pub fn class_element_static_field(
        &self,
        _static_token: arena::Box<'alloc, Token>,
        _field: arena::Box<'alloc, ClassElement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("class static field"))
    }

    // ClassElement : `;`
    pub fn class_element_empty(
        &self,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, arena::Box<'alloc, ClassElement<'alloc>>>> {
        self.alloc_with(|| self.new_vec())
    }

    // AsyncMethod : `async` PropertyName `(` UniqueFormalParameters `)` `{` AsyncFunctionBody `}`
    pub fn async_method(
        &mut self,
        async_token: arena::Box<'alloc, Token>,
        name: arena::Box<'alloc, PropertyName<'alloc>>,
        param_open_token: arena::Box<'alloc, Token>,
        mut params: arena::Box<'alloc, FormalParameters<'alloc>>,
        param_close_token: arena::Box<'alloc, Token>,
        body_open_token: arena::Box<'alloc, Token>,
        mut body: arena::Box<'alloc, FunctionBody<'alloc>>,
        body_close_token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, MethodDefinition<'alloc>>> {
        let param_open_loc = param_open_token.loc;
        let param_close_loc = param_close_token.loc;
        let body_close_loc = body_close_token.loc;

        self.check_unique_function_bindings(param_open_loc.start, param_close_loc.end)?;

        params.loc.set_range(param_open_loc, param_close_loc);
        body.loc.set_range(body_open_token.loc, body_close_loc);

        Ok(self.alloc_with(|| {
            MethodDefinition::Method(Method {
                name: name.unbox(),
                is_async: true,
                is_generator: false,
                params: params.unbox(),
                body: body.unbox(),
                loc: SourceLocation::from_parts(async_token.loc, body_close_loc),
            })
        }))
    }

    // AwaitExpression : `await` UnaryExpression
    pub fn await_expr(
        &self,
        await_token: arena::Box<'alloc, Token>,
        operand: arena::Box<'alloc, Expression<'alloc>>,
    ) -> arena::Box<'alloc, Expression<'alloc>> {
        let operand_loc = operand.get_loc();
        self.alloc_with(|| Expression::AwaitExpression {
            expression: operand,
            loc: SourceLocation::from_parts(await_token.loc, operand_loc),
        })
    }

    // AsyncArrowFunction : `async` AsyncArrowBindingIdentifier `=>` AsyncConciseBody
    // AsyncArrowFunction : CoverCallExpressionAndAsyncArrowHead `=>` AsyncConciseBody
    pub fn async_arrow_function_bare(
        &mut self,
        async_token: arena::Box<'alloc, Token>,
        identifier: arena::Box<'alloc, BindingIdentifier>,
        body: arena::Box<'alloc, ArrowExpressionBody<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Expression<'alloc>>> {
        let params = self.arrow_parameters_bare(identifier);

        self.check_unique_function_bindings(params.loc.start, params.loc.end)?;

        let body_loc = body.get_loc();
        Ok(self.alloc_with(|| Expression::ArrowExpression {
            is_async: true,
            params: params.unbox(),
            body: body.unbox(),
            loc: SourceLocation::from_parts(async_token.loc, body_loc),
        }))
    }

    pub fn async_arrow_function(
        &mut self,
        params: arena::Box<'alloc, Expression<'alloc>>,
        body: arena::Box<'alloc, ArrowExpressionBody<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Expression<'alloc>>> {
        let (params, call_loc) = self.async_arrow_parameters(params)?;

        self.check_unique_function_bindings(params.loc.start, params.loc.end)?;

        let body_loc = body.get_loc();
        Ok(self.alloc_with(|| Expression::ArrowExpression {
            is_async: true,
            params: params.unbox(),
            body: body.unbox(),
            loc: SourceLocation::from_parts(call_loc, body_loc),
        }))
    }

    // AsyncArrowFunction : CoverCallExpressionAndAsyncArrowHead `=>` AsyncConciseBody
    //
    // This is used to convert the Expression that is produced by parsing a CoverCallExpressionAndAsyncArrowHead
    fn async_arrow_parameters(
        &self,
        call_expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, (arena::Box<'alloc, FormalParameters<'alloc>>, SourceLocation)> {
        match call_expression.unbox() {
            Expression::CallExpression(CallExpression {
                callee: ce,
                arguments,
                loc,
            }) => {
                // Check that `callee` is `async`.
                match ce {
                    ExpressionOrSuper::Expression(callee) => match callee.unbox() {
                        Expression::IdentifierExpression(IdentifierExpression { name, .. }) => {
                            if name.value != CommonSourceAtomSetIndices::async_() {
                                // `foo(a, b) => {}`
                                return Err(ParseError::ArrowHeadInvalid);
                            }
                        }
                        _ => {
                            // `obj.async() => {}`
                            return Err(ParseError::ArrowHeadInvalid);
                        }
                    },

                    ExpressionOrSuper::Super { .. } => {
                        // Can't happen: `super()` doesn't match
                        // CoverCallExpressionAndAsyncArrowHead.
                        return Err(ParseError::ArrowHeadInvalid);
                    }
                }

                Ok((self.arguments_to_parameter_list(arguments)?, loc))
            }
            _ => {
                // The grammar ensures that the parser always passes
                // a valid CallExpression to this function.
                panic!("invalid argument");
            }
        }
    }

    // Script : ScriptBody?
    pub fn script(
        &mut self,
        script: Option<arena::Box<'alloc, Script<'alloc>>>,
    ) -> Result<'alloc, arena::Box<'alloc, Script<'alloc>>> {
        self.check_script_bindings()?;

        Ok(match script {
            Some(script) => script,
            None => self.alloc_with(|| Script {
                directives: self.new_vec(),
                statements: self.new_vec(),
                loc: SourceLocation::default(),
            }),
        })
    }

    // ScriptBody : StatementList
    pub fn script_body(
        &self,
        statements: arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>>,
    ) -> arena::Box<'alloc, Script<'alloc>> {
        let loc = if statements.is_empty() {
            SourceLocation::default()
        } else {
            SourceLocation::from_parts(
                statements.first().unwrap().get_loc(),
                statements.last().unwrap().get_loc(),
            )
        };

        // TODO: directives
        self.alloc_with(|| Script {
            directives: self.new_vec(),
            statements: statements.unbox(),
            loc,
        })
    }

    // Module : ModuleBody?
    pub fn module(
        &mut self,
        body: Option<arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>>>,
    ) -> Result<'alloc, arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>>> {
        self.check_module_bindings()?;

        Ok(body.unwrap_or_else(|| self.alloc_with(|| self.new_vec())))
    }

    // ModuleItemList : ModuleItem
    pub fn module_item_list_single(
        &self,
        item: arena::Box<'alloc, Statement<'alloc>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>> {
        self.alloc_with(|| self.new_vec_single(item.unbox()))
    }

    // ModuleItemList : ModuleItemList ModuleItem
    pub fn module_item_list_append(
        &self,
        mut list: arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>>,
        item: arena::Box<'alloc, Statement<'alloc>>,
    ) -> arena::Box<'alloc, arena::Vec<'alloc, Statement<'alloc>>> {
        self.push(&mut list, item.unbox());
        list
    }

    // ImportDeclaration : `import` ImportClause FromClause `;`
    // ImportDeclaration : `import` ModuleSpecifier `;`
    pub fn import_declaration(
        &self,
        _import_clause: Option<arena::Box<'alloc, Void>>,
        _module_specifier: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("import"))
    }

    // ImportClause : ImportedDefaultBinding
    // ImportClause : NameSpaceImport
    // ImportClause : NamedImports
    // ImportClause : ImportedDefaultBinding `,` NameSpaceImport
    // ImportClause : ImportedDefaultBinding `,` NamedImports
    pub fn import_clause(
        &self,
        _default_binding: Option<arena::Box<'alloc, BindingIdentifier>>,
        _name_space_import: Option<arena::Box<'alloc, Void>>,
        _named_imports: Option<arena::Box<'alloc, Void>>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("import"))
    }

    // NameSpaceImport : `*` `as` ImportedBinding
    pub fn name_space_import(
        &self,
        _name: arena::Box<'alloc, BindingIdentifier>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("import"))
    }

    // NamedImports : `{` `}`
    pub fn imports_list_empty(&self) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("import"))
    }

    // ImportsList : ImportSpecifier
    // ImportsList : ImportsList `,` ImportSpecifier
    pub fn imports_list_append(
        &self,
        _list: arena::Box<'alloc, Void>,
        _item: arena::Box<'alloc, Void>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("import"))
    }

    // ImportSpecifier : ImportedBinding
    pub fn import_specifier(
        &self,
        _name: arena::Box<'alloc, BindingIdentifier>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("import"))
    }

    // ImportSpecifier : IdentifierName `as` ImportedBinding
    pub fn import_specifier_renaming(
        &self,
        _original_name: arena::Box<'alloc, Token>,
        _local_name: arena::Box<'alloc, BindingIdentifier>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("import"))
    }

    // ModuleSpecifier : StringLiteral
    pub fn module_specifier(
        &self,
        _token: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Token>> {
        Err(ParseError::NotImplemented("import"))
    }

    // ExportDeclaration : `export` `*` FromClause `;`
    pub fn export_all_from(
        &self,
        _module_specifier: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("export"))
    }

    // ExportDeclaration : `export` ExportClause FromClause `;`
    pub fn export_set_from(
        &self,
        _export_clause: arena::Box<'alloc, Void>,
        _module_specifier: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("export"))
    }

    // ExportDeclaration : `export` ExportClause `;`
    pub fn export_set(
        &self,
        _export_clause: arena::Box<'alloc, Void>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("export"))
    }

    // ExportDeclaration : `export` VariableStatement
    pub fn export_vars(
        &self,
        _statement: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("export"))
    }

    // ExportDeclaration : `export` Declaration
    pub fn export_declaration(
        &self,
        _declaration: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("export"))
    }

    // ExportDeclaration : `export` `default` HoistableDeclaration
    pub fn export_default_hoistable(
        &self,
        _declaration: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("export"))
    }

    // ExportDeclaration : `export` `default` ClassDeclaration
    pub fn export_default_class(
        &self,
        _class_declaration: arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("export"))
    }

    // ExportDeclaration : `export` `default` [lookahead <! {`function`, `async`, `class`}] AssignmentExpression `;`
    pub fn export_default_value(
        &self,
        _expression: arena::Box<'alloc, Expression<'alloc>>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("export"))
    }

    // ExportClause : `{` `}`
    pub fn exports_list_empty(&self) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("export"))
    }

    // ExportsList : ExportSpecifier
    // ExportsList : ExportsList `,` ExportSpecifier
    pub fn exports_list_append(
        &self,
        _list: arena::Box<'alloc, Void>,
        _export_specifier: arena::Box<'alloc, Void>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("export"))
    }

    // ExportSpecifier : IdentifierName
    pub fn export_specifier(
        &self,
        _identifier: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("export"))
    }

    // ExportSpecifier : IdentifierName `as` IdentifierName
    pub fn export_specifier_renaming(
        &self,
        _local_name: arena::Box<'alloc, Token>,
        _exported_name: arena::Box<'alloc, Token>,
    ) -> Result<'alloc, arena::Box<'alloc, Void>> {
        Err(ParseError::NotImplemented("export"))
    }

    // Check Early Error for BindingIdentifier and note binding info to the
    // stack.
    fn on_binding_identifier(&mut self, token: &arena::Box<'alloc, Token>) -> Result<'alloc, ()> {
        let context = IdentifierEarlyErrorsContext::new();
        context.check_binding_identifier(token, &self.atoms.borrow())?;

        let name = token.value.as_atom();
        let offset = token.loc.start;

        if let Some(info) = self.bindings.last() {
            debug_assert!(info.offset < offset);
        }

        self.bindings.push(BindingInfo {
            name,
            offset,
            kind: BindingKind::Unknown,
        });

        Ok(())
    }

    // Check Early Error for IdentifierReference.
    fn on_identifier_reference(&self, token: &arena::Box<'alloc, Token>) -> Result<'alloc, ()> {
        let context = IdentifierEarlyErrorsContext::new();
        context.check_identifier_reference(token, &self.atoms.borrow())
    }

    // Check Early Error for LabelIdentifier and note binding info to the
    // stack
    fn on_label_identifier(&mut self, token: &arena::Box<'alloc, Token>) -> Result<'alloc, ()> {
        let context = IdentifierEarlyErrorsContext::new();

        let name = token.value.as_atom();
        let offset = token.loc.start;

        if let Some(info) = self.bindings.last() {
            debug_assert!(info.offset < offset);
        }

        // Labels are not usually considered bindings, but we are using bindings
        // in order to track duplicate label information. See `check_labelled_statement` for
        // more information about how this is used.
        //
        // If the label is attached to a continue or break statement, its binding info
        // is popped from the stack. See `continue_statement` and `break_statement` for more
        // information.
        self.bindings.push(BindingInfo {
            name,
            offset,
            kind: BindingKind::Label,
        });

        context.check_label_identifier(token, &self.atoms.borrow())
    }

    // Update the binding kind of all names declared in a specific range of the
    // source (and not in any nested scope). This is used e.g. when the parser
    // reaches the end of a VariableStatement to mark all the variables as Var
    // bindings.
    //
    // It's necessary because the current parser only calls AstBuilder methods
    // at the end of each production, not at the beginning.
    //
    // Bindings inside `StatementList` must be marked using this method before
    // we reach the end of its scope.
    fn mark_binding_kind(&mut self, from: usize, to: Option<usize>, kind: BindingKind) {
        for info in self.bindings.iter_mut().rev() {
            if info.offset < from {
                break;
            }

            if to.is_none() || info.offset < to.unwrap() {
                info.kind = kind;
            }
        }
    }

    // Returns the index of the first binding at/after `offset` source position.
    fn find_first_binding(&mut self, offset: usize) -> usize {
        let mut i = self.bindings.len();
        for info in self.bindings.iter_mut().rev() {
            if info.offset < offset {
                break;
            }
            i -= 1;
        }
        i
    }

    // Remove all bindings after `index`-th item.
    //
    // This should be called when leaving function/script/module.
    fn pop_bindings_from(&mut self, index: usize) {
        self.bindings.truncate(index)
    }

    // Remove lexical bindings after `index`-th item,
    // while keeping var bindings.
    //
    // This should be called when leaving block.
    fn pop_lexical_bindings_from(&mut self, index: usize) {
        let len = self.bindings.len();
        let mut i = index;

        while i < len && self.bindings[i].kind == BindingKind::Var {
            i += 1;
        }

        let mut j = i;
        while j < len {
            if self.bindings[j].kind == BindingKind::Var
                || self.bindings[j].kind == BindingKind::Label
            {
                self.bindings[i] = self.bindings[j];
                i += 1;
            }
            j += 1;
        }

        self.bindings.truncate(i)
    }

    // Returns the index of the first break or continue at/after `offset` source position.
    fn find_first_break_or_continue(&mut self, offset: usize) -> BreakOrContinueIndex {
        let mut i = self.breaks_and_continues.len();
        for info in self.breaks_and_continues.iter_mut().rev() {
            if info.offset < offset {
                break;
            }
            i -= 1;
        }
        BreakOrContinueIndex::new(i)
    }

    fn pop_labelled_breaks_and_continues_from_index(
        &mut self,
        index: BreakOrContinueIndex,
        name: SourceAtomSetIndex,
    ) {
        let len = self.breaks_and_continues.len();
        let mut i = index.index;

        let mut j = i;
        while j < len {
            let label = self.breaks_and_continues[j].label;
            if label.is_none() || label.unwrap() != name {
                self.breaks_and_continues[i] = self.breaks_and_continues[j];
                i += 1;
            }
            j += 1;
        }

        self.breaks_and_continues.truncate(i)
    }

    fn pop_unlabelled_breaks_and_continues_from(&mut self, offset: usize) {
        let len = self.breaks_and_continues.len();
        let index = self.find_first_break_or_continue(offset);
        let mut i = index.index;

        while i < len && self.breaks_and_continues[i].label.is_some() {
            i += 1;
        }

        let mut j = i;
        while j < len {
            if self.breaks_and_continues[j].label.is_some() {
                self.breaks_and_continues[i] = self.breaks_and_continues[j];
                i += 1;
            }
            j += 1;
        }

        self.breaks_and_continues.truncate(i)
    }

    fn pop_unlabelled_breaks_from(&mut self, offset: usize) {
        let len = self.breaks_and_continues.len();
        let index = self.find_first_break_or_continue(offset);
        let mut i = index.index;

        while i < len && self.breaks_and_continues[i].label.is_some() {
            i += 1;
        }

        let mut j = i;
        while j < len {
            if self.breaks_and_continues[j].label.is_some()
                || self.breaks_and_continues[j].kind == ControlKind::Continue
            {
                self.breaks_and_continues[i] = self.breaks_and_continues[j];
                i += 1;
            }
            j += 1;
        }

        self.breaks_and_continues.truncate(i)
    }

    // Declare bindings to Block-like context, where function declarations
    // are lexical.
    fn declare_block<T>(&self, context: &mut T, index: usize) -> Result<'alloc, ()>
    where
        T: LexicalEarlyErrorsContext + VarEarlyErrorsContext,
    {
        for info in self.bindings.iter().skip(index) {
            match info.kind {
                BindingKind::Var => {
                    context.declare_var(
                        info.name,
                        DeclarationKind::Var,
                        info.offset,
                        &self.atoms.borrow(),
                    )?;
                }
                BindingKind::Function => {
                    context.declare_lex(
                        info.name,
                        DeclarationKind::LexicalFunction,
                        info.offset,
                        &self.atoms.borrow(),
                    )?;
                }
                BindingKind::AsyncOrGenerator => {
                    context.declare_lex(
                        info.name,
                        DeclarationKind::LexicalAsyncOrGenerator,
                        info.offset,
                        &self.atoms.borrow(),
                    )?;
                }
                BindingKind::Let => {
                    context.declare_lex(
                        info.name,
                        DeclarationKind::Let,
                        info.offset,
                        &self.atoms.borrow(),
                    )?;
                }
                BindingKind::Const => {
                    context.declare_lex(
                        info.name,
                        DeclarationKind::Const,
                        info.offset,
                        &self.atoms.borrow(),
                    )?;
                }
                BindingKind::Class => {
                    context.declare_lex(
                        info.name,
                        DeclarationKind::Class,
                        info.offset,
                        &self.atoms.borrow(),
                    )?;
                }
                // Do nothing for Labels, as they have to be nested for
                // a syntax error to occur.
                //
                // We do not have the nesting information at the
                // script/block/function level so we cannot reuse the mechanism
                // used for checking duplicate bindings in those error contexts.
                // We only know that the label occurred somewhere in the block,
                // and that it might occur more than once, not how it occurs.
                // This is handled in check_labelled_statement.
                BindingKind::Label => {}
                _ => {
                    panic!("Unexpected binding found {:?}", info);
                }
            }
        }

        Ok(())
    }

    // Check bindings in Block.
    fn check_block_bindings(&mut self, start_of_block_offset: usize) -> Result<'alloc, ()> {
        let mut context = BlockEarlyErrorsContext::new();
        let index = self.find_first_binding(start_of_block_offset);
        self.declare_block(&mut context, index)?;
        self.pop_lexical_bindings_from(index);

        Ok(())
    }

    // Declare bindings to the head of lexical for-statement.
    fn declare_lexical_for_head(
        &self,
        context: &mut LexicalForHeadEarlyErrorsContext,
        from: usize,
        to: usize,
    ) -> Result<'alloc, ()> {
        for info in self.bindings.iter().skip(from).take(to - from) {
            match info.kind {
                BindingKind::Let => {
                    context.declare_lex(
                        info.name,
                        DeclarationKind::Let,
                        info.offset,
                        &self.atoms.borrow(),
                    )?;
                }
                BindingKind::Const => {
                    context.declare_lex(
                        info.name,
                        DeclarationKind::Const,
                        info.offset,
                        &self.atoms.borrow(),
                    )?;
                }
                _ => {
                    panic!("Unexpected binding found {:?}", info);
                }
            }
        }

        Ok(())
    }

    // Declare bindings to the body of lexical for-statement.
    fn declare_lexical_for_body(
        &self,
        context: &mut LexicalForBodyEarlyErrorsContext,
        index: usize,
    ) -> Result<'alloc, ()> {
        for info in self.bindings.iter().skip(index) {
            match info.kind {
                BindingKind::Var => {
                    context.declare_var(
                        info.name,
                        DeclarationKind::Var,
                        info.offset,
                        &self.atoms.borrow(),
                    )?;
                }
                // Do nothing for Labels, as they have to be nested for
                // a syntax error to occur
                //
                // We do not have the nesting information at the
                // script/block/function level so we cannot reuse the mechanism
                // used for checking duplicate bindings in those error contexts.
                // We only know that the label occurred somewhere in the block,
                // and that it might occur more than once, not how it occurs.
                // This is handled in check_labelled_statement.
                BindingKind::Label => {}
                _ => {
                    panic!("Unexpected binding found {:?}", info);
                }
            }
        }

        Ok(())
    }

    // Check bindings in lexical for-statement.
    fn check_lexical_for_bindings(&mut self, bindings_loc: &SourceLocation) -> Result<'alloc, ()> {
        let mut head_context = LexicalForHeadEarlyErrorsContext::new();

        let head_index = self.find_first_binding(bindings_loc.start);
        let body_index = self.find_first_binding(bindings_loc.end);
        self.declare_lexical_for_head(&mut head_context, head_index, body_index)?;

        let mut body_context = LexicalForBodyEarlyErrorsContext::new(head_context);
        self.declare_lexical_for_body(&mut body_context, body_index)?;
        self.pop_lexical_bindings_from(head_index);

        Ok(())
    }

    // Check bindings in CaseBlock of switch-statement.
    fn check_case_block_binding(&mut self, start_of_block_offset: usize) -> Result<'alloc, ()> {
        let mut context = CaseBlockEarlyErrorsContext::new();

        let index = self.find_first_binding(start_of_block_offset);
        // Check bindings in CaseBlock of switch-statement.
        self.declare_block(&mut context, index)?;

        self.pop_unlabelled_breaks_from(index);

        self.pop_lexical_bindings_from(index);

        Ok(())
    }

    // Declare bindings to the parameter of function or catch.
    fn declare_param<T>(&self, context: &mut T, from: usize, to: usize) -> Result<'alloc, ()>
    where
        T: ParameterEarlyErrorsContext,
    {
        for info in self.bindings.iter().skip(from).take(to - from) {
            context.declare(info.name, info.offset, &self.atoms.borrow())?;
        }

        Ok(())
    }

    // Check bindings in Catch and Block.
    fn check_catch_bindings(
        &mut self,
        is_simple: bool,
        bindings_loc: &SourceLocation,
    ) -> Result<'alloc, ()> {
        let mut param_context = if is_simple {
            CatchParameterEarlyErrorsContext::new_with_binding_identifier()
        } else {
            CatchParameterEarlyErrorsContext::new_with_binding_pattern()
        };

        let param_index = self.find_first_binding(bindings_loc.start);
        let body_index = self.find_first_binding(bindings_loc.end);
        self.declare_param(&mut param_context, param_index, body_index)?;

        let mut block_context = CatchBlockEarlyErrorsContext::new(param_context);
        self.declare_block(&mut block_context, body_index)?;
        self.pop_lexical_bindings_from(param_index);

        Ok(())
    }

    // Check bindings in Catch with no parameter and Block.
    fn check_catch_no_param_bindings(&mut self, catch_offset: usize) -> Result<'alloc, ()> {
        let body_index = self.find_first_binding(catch_offset);

        let param_context = CatchParameterEarlyErrorsContext::new_with_binding_identifier();
        let mut block_context = CatchBlockEarlyErrorsContext::new(param_context);
        self.declare_block(&mut block_context, body_index)?;
        self.pop_lexical_bindings_from(body_index);

        Ok(())
    }

    fn check_unhandled_break_or_continue<T>(
        &mut self,
        context: T,
        offset: usize,
    ) -> Result<'alloc, ()>
    where
        T: ControlEarlyErrorsContext,
    {
        let index = self.find_first_break_or_continue(offset);
        if let Some(info) = self.find_break_or_continue_at(index) {
            context.on_unhandled_break_or_continue(info)?;
        }

        Ok(())
    }

    fn check_unhandled_continue(
        &mut self,
        context: LabelledStatementEarlyErrorsContext,
        index: &BreakOrContinueIndex,
    ) -> Result<'alloc, ()> {
        for info in self.breaks_and_continues.iter().skip(index.index) {
            context.check_labelled_continue_to_non_loop(info)?;
        }

        Ok(())
    }

    fn find_break_or_continue_at(&self, index: BreakOrContinueIndex) -> Option<&ControlInfo> {
        self.breaks_and_continues.get(index.index)
    }

    // Declare bindings to script-or-function-like context, where function
    // declarations are body-level.
    fn declare_script_or_function<T>(&self, context: &mut T, index: usize) -> Result<'alloc, ()>
    where
        T: LexicalEarlyErrorsContext + VarEarlyErrorsContext,
    {
        for info in self.bindings.iter().skip(index) {
            match info.kind {
                BindingKind::Var => {
                    context.declare_var(
                        info.name,
                        DeclarationKind::Var,
                        info.offset,
                        &self.atoms.borrow(),
                    )?;
                }
                BindingKind::Function | BindingKind::AsyncOrGenerator => {
                    context.declare_var(
                        info.name,
                        DeclarationKind::BodyLevelFunction,
                        info.offset,
                        &self.atoms.borrow(),
                    )?;
                }
                BindingKind::Let => {
                    context.declare_lex(
                        info.name,
                        DeclarationKind::Let,
                        info.offset,
                        &self.atoms.borrow(),
                    )?;
                }
                BindingKind::Const => {
                    context.declare_lex(
                        info.name,
                        DeclarationKind::Const,
                        info.offset,
                        &self.atoms.borrow(),
                    )?;
                }
                BindingKind::Class => {
                    context.declare_lex(
                        info.name,
                        DeclarationKind::Class,
                        info.offset,
                        &self.atoms.borrow(),
                    )?;
                }
                // Do nothing for Labels, as they have to be nested for
                // a syntax error to occur
                //
                // We do not have the nesting information at the
                // script/block/function level so we cannot reuse the mechanism
                // used for checking duplicate bindings in those error contexts.
                // We only know that the label occurred somewhere in the block,
                // and that it might occur more than once, not how it occurs.
                // This is handled in check_labelled_statement.
                BindingKind::Label => {}
                _ => {
                    panic!("Unexpected binding found {:?}", info);
                }
            }
        }

        Ok(())
    }

    // Check bindings in function with FormalParameters.
    fn check_function_bindings(
        &mut self,
        is_simple: bool,
        start_of_param_offset: usize,
        end_of_param_offset: usize,
    ) -> Result<'alloc, ()> {
        let mut param_context = if is_simple {
            FormalParametersEarlyErrorsContext::new_simple()
        } else {
            FormalParametersEarlyErrorsContext::new_non_simple()
        };

        let param_index = self.find_first_binding(start_of_param_offset);
        let body_index = self.find_first_binding(end_of_param_offset);
        self.declare_param(&mut param_context, param_index, body_index)?;

        let mut body_context = FunctionBodyEarlyErrorsContext::new(param_context);
        self.declare_script_or_function(&mut body_context, body_index)?;

        self.check_unhandled_break_or_continue(body_context, end_of_param_offset)?;

        self.pop_bindings_from(param_index);

        Ok(())
    }

    // Check bindings in function with UniqueFormalParameters.
    fn check_unique_function_bindings(
        &mut self,
        start_of_param_offset: usize,
        end_of_param_offset: usize,
    ) -> Result<'alloc, ()> {
        let mut param_context = UniqueFormalParametersEarlyErrorsContext::new();

        let param_index = self.find_first_binding(start_of_param_offset);
        let body_index = self.find_first_binding(end_of_param_offset);
        self.declare_param(&mut param_context, param_index, body_index)?;

        let mut body_context = UniqueFunctionBodyEarlyErrorsContext::new(param_context);
        self.declare_script_or_function(&mut body_context, body_index)?;
        self.pop_bindings_from(param_index);

        self.check_unhandled_break_or_continue(body_context, end_of_param_offset)?;

        Ok(())
    }

    // Check bindings in Script.
    fn check_script_bindings(&mut self) -> Result<'alloc, ()> {
        let mut context = ScriptEarlyErrorsContext::new();
        self.declare_script_or_function(&mut context, 0)?;
        self.pop_bindings_from(0);

        self.check_unhandled_break_or_continue(context, 0)?;

        Ok(())
    }

    // Check bindings in Module.
    fn check_module_bindings(&mut self) -> Result<'alloc, ()> {
        let mut context = ModuleEarlyErrorsContext::new();
        self.declare_script_or_function(&mut context, 0)?;
        self.pop_bindings_from(0);

        self.check_unhandled_break_or_continue(context, 0)?;

        Ok(())
    }

    // Returns IsSimpleParameterList of `params`.
    //
    // NOTE: For Syntax-only parsing (NYI), the stack value for FormalParameters
    //       should contain this information.
    fn is_params_simple(params: &FormalParameters<'alloc>) -> bool {
        for param in params.items.iter() {
            match param {
                Parameter::Binding(Binding::BindingIdentifier(_)) => {}
                _ => {
                    return false;
                }
            }
        }

        if params.rest.is_some() {
            return false;
        }

        true
    }

    // Static Semantics: Early Errors
    // https://tc39.es/ecma262/#sec-if-statement-static-semantics-early-errors
    // https://tc39.es/ecma262/#sec-semantics-static-semantics-early-errors
    // https://tc39.es/ecma262/#sec-with-statement-static-semantics-early-errors
    fn check_single_statement(
        &self,
        stmt: &arena::Box<'alloc, Statement<'alloc>>,
    ) -> Result<'alloc, ()> {
        // * It is a Syntax Error if IsLabelledFunction(Statement) is true.
        if self.is_labelled_function(stmt) {
            return Err(ParseError::LabelledFunctionDeclInSingleStatement);
        }
        Ok(())
    }

    // https://tc39.es/ecma262/#sec-islabelledfunction
    // Static Semantics: IsLabelledFunction ( stmt )
    //
    // Returns IsLabelledFunction of `stmt`.
    //
    // NOTE: For Syntax-only parsing (NYI), the stack value for Statement
    //       should contain this information.
    fn is_labelled_function(&self, mut stmt: &Statement<'alloc>) -> bool {
        // Step 1. If stmt is not a LabelledStatement , return false.
        while let Statement::LabelledStatement { ref body, .. } = stmt {
            // Step 2. Let item be the LabelledItem of stmt.
            let item: &Statement<'alloc> = body;

            // Step 3. If item is LabelledItem : FunctionDeclaration,
            // return true.
            if let Statement::FunctionDeclaration(_) = item {
                return true;
            }

            // Step 4. Let subStmt be the Statement of item.
            // Step 5. Return IsLabelledFunction(subStmt).
            stmt = item;
        }

        false
    }

    fn check_labelled_statement(
        &mut self,
        label: &arena::Box<'alloc, Label>,
        body: &Statement<'alloc>,
    ) -> Result<'alloc, ()> {
        let start_label_offset = label.loc.start;
        let end_label_offset = label.loc.end;
        let name = label.value;
        let is_loop = match body {
            Statement::ForStatement { .. }
            | Statement::ForOfStatement { .. }
            | Statement::ForInStatement { .. }
            | Statement::WhileStatement { .. }
            | Statement::DoWhileStatement { .. } => true,
            _ => false,
        };

        let context = LabelledStatementEarlyErrorsContext::new(name, is_loop);

        let binding_index = self.find_first_binding(end_label_offset);
        for info in self.bindings.iter().skip(binding_index) {
            context.check_duplicate_label(info.name)?;
        }

        let label_index = self.find_first_break_or_continue(start_label_offset);
        self.check_unhandled_continue(context, &label_index)?;

        self.pop_labelled_breaks_and_continues_from_index(label_index, name);
        Ok(())
    }
}
