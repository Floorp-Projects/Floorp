// WARNING: This file is auto-generated.

use crate::source_location::SourceLocation;
use crate::arena;
use crate::source_atom_set::SourceAtomSetIndex;
use crate::source_slice_list::SourceSliceIndex;

#[derive(Debug, PartialEq)]
pub enum Void {
}

#[derive(Debug, PartialEq)]
pub enum Argument<'alloc> {
    SpreadElement(arena::Box<'alloc, Expression<'alloc>>),
    Expression(arena::Box<'alloc, Expression<'alloc>>),
}

#[derive(Debug, PartialEq)]
pub struct Arguments<'alloc> {
    pub args: arena::Vec<'alloc, Argument<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct Identifier {
    pub value: SourceAtomSetIndex,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct IdentifierName {
    pub value: SourceAtomSetIndex,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct PrivateIdentifier {
    pub value: SourceAtomSetIndex,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct Label {
    pub value: SourceAtomSetIndex,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum VariableDeclarationKind {
    Var {
        loc: SourceLocation,
    },
    Let {
        loc: SourceLocation,
    },
    Const {
        loc: SourceLocation,
    },
}

#[derive(Debug, PartialEq)]
pub enum CompoundAssignmentOperator {
    Add {
        loc: SourceLocation,
    },
    Sub {
        loc: SourceLocation,
    },
    Mul {
        loc: SourceLocation,
    },
    Div {
        loc: SourceLocation,
    },
    Mod {
        loc: SourceLocation,
    },
    Pow {
        loc: SourceLocation,
    },
    LeftShift {
        loc: SourceLocation,
    },
    RightShift {
        loc: SourceLocation,
    },
    RightShiftExt {
        loc: SourceLocation,
    },
    Or {
        loc: SourceLocation,
    },
    Xor {
        loc: SourceLocation,
    },
    And {
        loc: SourceLocation,
    },
}

#[derive(Debug, PartialEq)]
pub enum BinaryOperator {
    Equals {
        loc: SourceLocation,
    },
    NotEquals {
        loc: SourceLocation,
    },
    StrictEquals {
        loc: SourceLocation,
    },
    StrictNotEquals {
        loc: SourceLocation,
    },
    LessThan {
        loc: SourceLocation,
    },
    LessThanOrEqual {
        loc: SourceLocation,
    },
    GreaterThan {
        loc: SourceLocation,
    },
    GreaterThanOrEqual {
        loc: SourceLocation,
    },
    In {
        loc: SourceLocation,
    },
    Instanceof {
        loc: SourceLocation,
    },
    LeftShift {
        loc: SourceLocation,
    },
    RightShift {
        loc: SourceLocation,
    },
    RightShiftExt {
        loc: SourceLocation,
    },
    Add {
        loc: SourceLocation,
    },
    Sub {
        loc: SourceLocation,
    },
    Mul {
        loc: SourceLocation,
    },
    Div {
        loc: SourceLocation,
    },
    Mod {
        loc: SourceLocation,
    },
    Pow {
        loc: SourceLocation,
    },
    Comma {
        loc: SourceLocation,
    },
    Coalesce {
        loc: SourceLocation,
    },
    LogicalOr {
        loc: SourceLocation,
    },
    LogicalAnd {
        loc: SourceLocation,
    },
    BitwiseOr {
        loc: SourceLocation,
    },
    BitwiseXor {
        loc: SourceLocation,
    },
    BitwiseAnd {
        loc: SourceLocation,
    },
}

#[derive(Debug, PartialEq)]
pub enum UnaryOperator {
    Plus {
        loc: SourceLocation,
    },
    Minus {
        loc: SourceLocation,
    },
    LogicalNot {
        loc: SourceLocation,
    },
    BitwiseNot {
        loc: SourceLocation,
    },
    Typeof {
        loc: SourceLocation,
    },
    Void {
        loc: SourceLocation,
    },
    Delete {
        loc: SourceLocation,
    },
}

#[derive(Debug, PartialEq)]
pub enum UpdateOperator {
    Increment {
        loc: SourceLocation,
    },
    Decrement {
        loc: SourceLocation,
    },
}

#[derive(Debug, PartialEq)]
pub struct Function<'alloc> {
    pub name: Option<BindingIdentifier>,
    pub is_async: bool,
    pub is_generator: bool,
    pub params: FormalParameters<'alloc>,
    pub body: FunctionBody<'alloc>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum Program<'alloc> {
    Module(Module<'alloc>),
    Script(Script<'alloc>),
}

#[derive(Debug, PartialEq)]
pub struct IfStatement<'alloc> {
    pub test: arena::Box<'alloc, Expression<'alloc>>,
    pub consequent: arena::Box<'alloc, Statement<'alloc>>,
    pub alternate: Option<arena::Box<'alloc, Statement<'alloc>>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum Statement<'alloc> {
    BlockStatement {
        block: Block<'alloc>,
        loc: SourceLocation,
    },
    BreakStatement {
        label: Option<Label>,
        loc: SourceLocation,
    },
    ContinueStatement {
        label: Option<Label>,
        loc: SourceLocation,
    },
    DebuggerStatement {
        loc: SourceLocation,
    },
    DoWhileStatement {
        block: arena::Box<'alloc, Statement<'alloc>>,
        test: arena::Box<'alloc, Expression<'alloc>>,
        loc: SourceLocation,
    },
    EmptyStatement {
        loc: SourceLocation,
    },
    ExpressionStatement(arena::Box<'alloc, Expression<'alloc>>),
    ForInStatement {
        left: VariableDeclarationOrAssignmentTarget<'alloc>,
        right: arena::Box<'alloc, Expression<'alloc>>,
        block: arena::Box<'alloc, Statement<'alloc>>,
        loc: SourceLocation,
    },
    ForOfStatement {
        left: VariableDeclarationOrAssignmentTarget<'alloc>,
        right: arena::Box<'alloc, Expression<'alloc>>,
        block: arena::Box<'alloc, Statement<'alloc>>,
        loc: SourceLocation,
    },
    ForStatement {
        init: Option<VariableDeclarationOrExpression<'alloc>>,
        test: Option<arena::Box<'alloc, Expression<'alloc>>>,
        update: Option<arena::Box<'alloc, Expression<'alloc>>>,
        block: arena::Box<'alloc, Statement<'alloc>>,
        loc: SourceLocation,
    },
    IfStatement(IfStatement<'alloc>),
    LabeledStatement {
        label: Label,
        body: arena::Box<'alloc, Statement<'alloc>>,
        loc: SourceLocation,
    },
    ReturnStatement {
        expression: Option<arena::Box<'alloc, Expression<'alloc>>>,
        loc: SourceLocation,
    },
    SwitchStatement {
        discriminant: arena::Box<'alloc, Expression<'alloc>>,
        cases: arena::Vec<'alloc, SwitchCase<'alloc>>,
        loc: SourceLocation,
    },
    SwitchStatementWithDefault {
        discriminant: arena::Box<'alloc, Expression<'alloc>>,
        pre_default_cases: arena::Vec<'alloc, SwitchCase<'alloc>>,
        default_case: SwitchDefault<'alloc>,
        post_default_cases: arena::Vec<'alloc, SwitchCase<'alloc>>,
        loc: SourceLocation,
    },
    ThrowStatement {
        expression: arena::Box<'alloc, Expression<'alloc>>,
        loc: SourceLocation,
    },
    TryCatchStatement {
        body: Block<'alloc>,
        catch_clause: CatchClause<'alloc>,
        loc: SourceLocation,
    },
    TryFinallyStatement {
        body: Block<'alloc>,
        catch_clause: Option<CatchClause<'alloc>>,
        finalizer: Block<'alloc>,
        loc: SourceLocation,
    },
    WhileStatement {
        test: arena::Box<'alloc, Expression<'alloc>>,
        block: arena::Box<'alloc, Statement<'alloc>>,
        loc: SourceLocation,
    },
    WithStatement {
        object: arena::Box<'alloc, Expression<'alloc>>,
        body: arena::Box<'alloc, Statement<'alloc>>,
        loc: SourceLocation,
    },
    VariableDeclarationStatement(VariableDeclaration<'alloc>),
    FunctionDeclaration(Function<'alloc>),
    ClassDeclaration(ClassDeclaration<'alloc>),
}

#[derive(Debug, PartialEq)]
pub enum Expression<'alloc> {
    MemberExpression(MemberExpression<'alloc>),
    ClassExpression(ClassExpression<'alloc>),
    LiteralBooleanExpression {
        value: bool,
        loc: SourceLocation,
    },
    LiteralInfinityExpression {
        loc: SourceLocation,
    },
    LiteralNullExpression {
        loc: SourceLocation,
    },
    LiteralNumericExpression(NumericLiteral),
    LiteralRegExpExpression {
        pattern: SourceSliceIndex,
        global: bool,
        ignore_case: bool,
        multi_line: bool,
        dot_all: bool,
        sticky: bool,
        unicode: bool,
        loc: SourceLocation,
    },
    LiteralStringExpression {
        value: SourceAtomSetIndex,
        loc: SourceLocation,
    },
    ArrayExpression(ArrayExpression<'alloc>),
    ArrowExpression {
        is_async: bool,
        params: FormalParameters<'alloc>,
        body: ArrowExpressionBody<'alloc>,
        loc: SourceLocation,
    },
    AssignmentExpression {
        binding: AssignmentTarget<'alloc>,
        expression: arena::Box<'alloc, Expression<'alloc>>,
        loc: SourceLocation,
    },
    BinaryExpression {
        operator: BinaryOperator,
        left: arena::Box<'alloc, Expression<'alloc>>,
        right: arena::Box<'alloc, Expression<'alloc>>,
        loc: SourceLocation,
    },
    CallExpression(CallExpression<'alloc>),
    CompoundAssignmentExpression {
        operator: CompoundAssignmentOperator,
        binding: SimpleAssignmentTarget<'alloc>,
        expression: arena::Box<'alloc, Expression<'alloc>>,
        loc: SourceLocation,
    },
    ConditionalExpression {
        test: arena::Box<'alloc, Expression<'alloc>>,
        consequent: arena::Box<'alloc, Expression<'alloc>>,
        alternate: arena::Box<'alloc, Expression<'alloc>>,
        loc: SourceLocation,
    },
    FunctionExpression(Function<'alloc>),
    IdentifierExpression(IdentifierExpression),
    NewExpression {
        callee: arena::Box<'alloc, Expression<'alloc>>,
        arguments: Arguments<'alloc>,
        loc: SourceLocation,
    },
    NewTargetExpression {
        loc: SourceLocation,
    },
    ObjectExpression(ObjectExpression<'alloc>),
    OptionalExpression {
        object: ExpressionOrSuper<'alloc>,
        tail: arena::Box<'alloc, Expression<'alloc>>,
        loc: SourceLocation,
    },
    OptionalChain(OptionalChain<'alloc>),
    UnaryExpression {
        operator: UnaryOperator,
        operand: arena::Box<'alloc, Expression<'alloc>>,
        loc: SourceLocation,
    },
    TemplateExpression(TemplateExpression<'alloc>),
    ThisExpression {
        loc: SourceLocation,
    },
    UpdateExpression {
        is_prefix: bool,
        operator: UpdateOperator,
        operand: SimpleAssignmentTarget<'alloc>,
        loc: SourceLocation,
    },
    YieldExpression {
        expression: Option<arena::Box<'alloc, Expression<'alloc>>>,
        loc: SourceLocation,
    },
    YieldGeneratorExpression {
        expression: arena::Box<'alloc, Expression<'alloc>>,
        loc: SourceLocation,
    },
    AwaitExpression {
        expression: arena::Box<'alloc, Expression<'alloc>>,
        loc: SourceLocation,
    },
    ImportCallExpression {
        argument: arena::Box<'alloc, Expression<'alloc>>,
        loc: SourceLocation,
    },
}

#[derive(Debug, PartialEq)]
pub enum MemberExpression<'alloc> {
    ComputedMemberExpression(ComputedMemberExpression<'alloc>),
    StaticMemberExpression(StaticMemberExpression<'alloc>),
    PrivateFieldExpression(PrivateFieldExpression<'alloc>),
}

#[derive(Debug, PartialEq)]
pub enum OptionalChain<'alloc> {
    ComputedMemberExpressionTail {
        expression: arena::Box<'alloc, Expression<'alloc>>,
        loc: SourceLocation,
    },
    StaticMemberExpressionTail {
        property: IdentifierName,
        loc: SourceLocation,
    },
    CallExpressionTail {
        arguments: Arguments<'alloc>,
        loc: SourceLocation,
    },
    ComputedMemberExpression(ComputedMemberExpression<'alloc>),
    StaticMemberExpression(StaticMemberExpression<'alloc>),
    CallExpression(CallExpression<'alloc>),
}

#[derive(Debug, PartialEq)]
pub enum PropertyName<'alloc> {
    ComputedPropertyName(ComputedPropertyName<'alloc>),
    StaticPropertyName(StaticPropertyName),
    StaticNumericPropertyName(NumericLiteral),
}

#[derive(Debug, PartialEq)]
pub struct CallExpression<'alloc> {
    pub callee: ExpressionOrSuper<'alloc>,
    pub arguments: Arguments<'alloc>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum ClassElementName<'alloc> {
    ComputedPropertyName(ComputedPropertyName<'alloc>),
    StaticPropertyName(StaticPropertyName),
    StaticNumericPropertyName(NumericLiteral),
    PrivateFieldName(PrivateIdentifier),
}

#[derive(Debug, PartialEq)]
pub enum ObjectProperty<'alloc> {
    NamedObjectProperty(NamedObjectProperty<'alloc>),
    ShorthandProperty(ShorthandProperty),
    SpreadProperty(arena::Box<'alloc, Expression<'alloc>>),
}

#[derive(Debug, PartialEq)]
pub enum NamedObjectProperty<'alloc> {
    MethodDefinition(MethodDefinition<'alloc>),
    DataProperty(DataProperty<'alloc>),
}

#[derive(Debug, PartialEq)]
pub enum MethodDefinition<'alloc> {
    Method(Method<'alloc>),
    Getter(Getter<'alloc>),
    Setter(Setter<'alloc>),
}

#[derive(Debug, PartialEq)]
pub enum ImportDeclaration<'alloc> {
    Import(Import<'alloc>),
    ImportNamespace(ImportNamespace),
}

#[derive(Debug, PartialEq)]
pub enum ExportDeclaration<'alloc> {
    ExportAllFrom(ExportAllFrom),
    ExportFrom(ExportFrom<'alloc>),
    ExportLocals(ExportLocals<'alloc>),
    Export(Export<'alloc>),
    ExportDefault(ExportDefault<'alloc>),
}

#[derive(Debug, PartialEq)]
pub enum VariableReference {
    BindingIdentifier(BindingIdentifier),
    AssignmentTargetIdentifier(AssignmentTargetIdentifier),
}

#[derive(Debug, PartialEq)]
pub enum BindingPattern<'alloc> {
    ObjectBinding(ObjectBinding<'alloc>),
    ArrayBinding(ArrayBinding<'alloc>),
}

#[derive(Debug, PartialEq)]
pub enum Binding<'alloc> {
    BindingPattern(BindingPattern<'alloc>),
    BindingIdentifier(BindingIdentifier),
}

#[derive(Debug, PartialEq)]
pub enum SimpleAssignmentTarget<'alloc> {
    AssignmentTargetIdentifier(AssignmentTargetIdentifier),
    MemberAssignmentTarget(MemberAssignmentTarget<'alloc>),
}

#[derive(Debug, PartialEq)]
pub enum AssignmentTargetPattern<'alloc> {
    ArrayAssignmentTarget(ArrayAssignmentTarget<'alloc>),
    ObjectAssignmentTarget(ObjectAssignmentTarget<'alloc>),
}

#[derive(Debug, PartialEq)]
pub enum AssignmentTarget<'alloc> {
    AssignmentTargetPattern(AssignmentTargetPattern<'alloc>),
    SimpleAssignmentTarget(SimpleAssignmentTarget<'alloc>),
}

#[derive(Debug, PartialEq)]
pub enum Parameter<'alloc> {
    Binding(Binding<'alloc>),
    BindingWithDefault(BindingWithDefault<'alloc>),
}

#[derive(Debug, PartialEq)]
pub struct BindingWithDefault<'alloc> {
    pub binding: Binding<'alloc>,
    pub init: arena::Box<'alloc, Expression<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct BindingIdentifier {
    pub name: Identifier,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct AssignmentTargetIdentifier {
    pub name: Identifier,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum ExpressionOrSuper<'alloc> {
    Expression(arena::Box<'alloc, Expression<'alloc>>),
    Super {
        loc: SourceLocation,
    },
}

#[derive(Debug, PartialEq)]
pub enum MemberAssignmentTarget<'alloc> {
    ComputedMemberAssignmentTarget(ComputedMemberAssignmentTarget<'alloc>),
    StaticMemberAssignmentTarget(StaticMemberAssignmentTarget<'alloc>),
}

#[derive(Debug, PartialEq)]
pub struct ComputedMemberAssignmentTarget<'alloc> {
    pub object: ExpressionOrSuper<'alloc>,
    pub expression: arena::Box<'alloc, Expression<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct StaticMemberAssignmentTarget<'alloc> {
    pub object: ExpressionOrSuper<'alloc>,
    pub property: IdentifierName,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct ArrayBinding<'alloc> {
    pub elements: arena::Vec<'alloc, Option<Parameter<'alloc>>>,
    pub rest: Option<arena::Box<'alloc, Binding<'alloc>>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct ObjectBinding<'alloc> {
    pub properties: arena::Vec<'alloc, BindingProperty<'alloc>>,
    pub rest: Option<arena::Box<'alloc, BindingIdentifier>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum BindingProperty<'alloc> {
    BindingPropertyIdentifier(BindingPropertyIdentifier<'alloc>),
    BindingPropertyProperty(BindingPropertyProperty<'alloc>),
}

#[derive(Debug, PartialEq)]
pub struct BindingPropertyIdentifier<'alloc> {
    pub binding: BindingIdentifier,
    pub init: Option<arena::Box<'alloc, Expression<'alloc>>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct BindingPropertyProperty<'alloc> {
    pub name: PropertyName<'alloc>,
    pub binding: Parameter<'alloc>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct AssignmentTargetWithDefault<'alloc> {
    pub binding: AssignmentTarget<'alloc>,
    pub init: arena::Box<'alloc, Expression<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum AssignmentTargetMaybeDefault<'alloc> {
    AssignmentTarget(AssignmentTarget<'alloc>),
    AssignmentTargetWithDefault(AssignmentTargetWithDefault<'alloc>),
}

#[derive(Debug, PartialEq)]
pub struct ArrayAssignmentTarget<'alloc> {
    pub elements: arena::Vec<'alloc, Option<AssignmentTargetMaybeDefault<'alloc>>>,
    pub rest: Option<arena::Box<'alloc, AssignmentTarget<'alloc>>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct ObjectAssignmentTarget<'alloc> {
    pub properties: arena::Vec<'alloc, AssignmentTargetProperty<'alloc>>,
    pub rest: Option<arena::Box<'alloc, AssignmentTarget<'alloc>>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum AssignmentTargetProperty<'alloc> {
    AssignmentTargetPropertyIdentifier(AssignmentTargetPropertyIdentifier<'alloc>),
    AssignmentTargetPropertyProperty(AssignmentTargetPropertyProperty<'alloc>),
}

#[derive(Debug, PartialEq)]
pub struct AssignmentTargetPropertyIdentifier<'alloc> {
    pub binding: AssignmentTargetIdentifier,
    pub init: Option<arena::Box<'alloc, Expression<'alloc>>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct AssignmentTargetPropertyProperty<'alloc> {
    pub name: PropertyName<'alloc>,
    pub binding: AssignmentTargetMaybeDefault<'alloc>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct ClassExpression<'alloc> {
    pub name: Option<BindingIdentifier>,
    pub super_: Option<arena::Box<'alloc, Expression<'alloc>>>,
    pub elements: arena::Vec<'alloc, arena::Box<'alloc, ClassElement<'alloc>>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct ClassDeclaration<'alloc> {
    pub name: BindingIdentifier,
    pub super_: Option<arena::Box<'alloc, Expression<'alloc>>>,
    pub elements: arena::Vec<'alloc, arena::Box<'alloc, ClassElement<'alloc>>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum ClassElement<'alloc> {
    MethodDefinition {
        is_static: bool,
        method: MethodDefinition<'alloc>,
        loc: SourceLocation,
    },
    FieldDefinition {
        name: ClassElementName<'alloc>,
        init: Option<arena::Box<'alloc, Expression<'alloc>>>,
        loc: SourceLocation,
    },
}

#[derive(Debug, PartialEq)]
pub enum ModuleItems<'alloc> {
    ImportDeclaration(ImportDeclaration<'alloc>),
    ExportDeclaration(ExportDeclaration<'alloc>),
    Statement(arena::Box<'alloc, Statement<'alloc>>),
}

#[derive(Debug, PartialEq)]
pub struct Module<'alloc> {
    pub directives: arena::Vec<'alloc, Directive>,
    pub items: arena::Vec<'alloc, ModuleItems<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct Import<'alloc> {
    pub module_specifier: SourceAtomSetIndex,
    pub default_binding: Option<BindingIdentifier>,
    pub named_imports: arena::Vec<'alloc, ImportSpecifier>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct ImportNamespace {
    pub module_specifier: SourceAtomSetIndex,
    pub default_binding: Option<BindingIdentifier>,
    pub namespace_binding: BindingIdentifier,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct ImportSpecifier {
    pub name: Option<IdentifierName>,
    pub binding: BindingIdentifier,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct ExportAllFrom {
    pub module_specifier: SourceAtomSetIndex,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct ExportFrom<'alloc> {
    pub named_exports: arena::Vec<'alloc, ExportFromSpecifier>,
    pub module_specifier: SourceAtomSetIndex,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct ExportLocals<'alloc> {
    pub named_exports: arena::Vec<'alloc, ExportLocalSpecifier>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum Export<'alloc> {
    FunctionDeclaration(Function<'alloc>),
    ClassDeclaration(ClassDeclaration<'alloc>),
    VariableDeclaration(VariableDeclaration<'alloc>),
}

#[derive(Debug, PartialEq)]
pub enum ExportDefault<'alloc> {
    FunctionDeclaration(Function<'alloc>),
    ClassDeclaration(ClassDeclaration<'alloc>),
    Expression(arena::Box<'alloc, Expression<'alloc>>),
}

#[derive(Debug, PartialEq)]
pub struct ExportFromSpecifier {
    pub name: IdentifierName,
    pub exported_name: Option<IdentifierName>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct ExportLocalSpecifier {
    pub name: IdentifierExpression,
    pub exported_name: Option<IdentifierName>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct Method<'alloc> {
    pub name: PropertyName<'alloc>,
    pub is_async: bool,
    pub is_generator: bool,
    pub params: FormalParameters<'alloc>,
    pub body: FunctionBody<'alloc>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct Getter<'alloc> {
    pub property_name: PropertyName<'alloc>,
    pub body: FunctionBody<'alloc>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct Setter<'alloc> {
    pub property_name: PropertyName<'alloc>,
    pub param: Parameter<'alloc>,
    pub body: FunctionBody<'alloc>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct DataProperty<'alloc> {
    pub property_name: PropertyName<'alloc>,
    pub expression: arena::Box<'alloc, Expression<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct ShorthandProperty {
    pub name: IdentifierExpression,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct ComputedPropertyName<'alloc> {
    pub expression: arena::Box<'alloc, Expression<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct StaticPropertyName {
    pub value: SourceAtomSetIndex,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct NumericLiteral {
    pub value: f64,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum ArrayExpressionElement<'alloc> {
    SpreadElement(arena::Box<'alloc, Expression<'alloc>>),
    Expression(arena::Box<'alloc, Expression<'alloc>>),
    Elision {
        loc: SourceLocation,
    },
}

#[derive(Debug, PartialEq)]
pub struct ArrayExpression<'alloc> {
    pub elements: arena::Vec<'alloc, ArrayExpressionElement<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum ArrowExpressionBody<'alloc> {
    FunctionBody(FunctionBody<'alloc>),
    Expression(arena::Box<'alloc, Expression<'alloc>>),
}

#[derive(Debug, PartialEq)]
pub struct ComputedMemberExpression<'alloc> {
    pub object: ExpressionOrSuper<'alloc>,
    pub expression: arena::Box<'alloc, Expression<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct IdentifierExpression {
    pub name: Identifier,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct ObjectExpression<'alloc> {
    pub properties: arena::Vec<'alloc, arena::Box<'alloc, ObjectProperty<'alloc>>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct StaticMemberExpression<'alloc> {
    pub object: ExpressionOrSuper<'alloc>,
    pub property: IdentifierName,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct PrivateFieldExpression<'alloc> {
    pub object: arena::Box<'alloc, Expression<'alloc>>,
    pub field: PrivateIdentifier,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum TemplateExpressionElement<'alloc> {
    Expression(arena::Box<'alloc, Expression<'alloc>>),
    TemplateElement(TemplateElement),
}

#[derive(Debug, PartialEq)]
pub struct TemplateExpression<'alloc> {
    pub tag: Option<arena::Box<'alloc, Expression<'alloc>>>,
    pub elements: arena::Vec<'alloc, TemplateExpressionElement<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum VariableDeclarationOrAssignmentTarget<'alloc> {
    VariableDeclaration(VariableDeclaration<'alloc>),
    AssignmentTarget(AssignmentTarget<'alloc>),
}

#[derive(Debug, PartialEq)]
pub enum VariableDeclarationOrExpression<'alloc> {
    VariableDeclaration(VariableDeclaration<'alloc>),
    Expression(arena::Box<'alloc, Expression<'alloc>>),
}

#[derive(Debug, PartialEq)]
pub struct Block<'alloc> {
    pub statements: arena::Vec<'alloc, Statement<'alloc>>,
    pub declarations: Option<arena::Vec<'alloc, SourceAtomSetIndex>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct CatchClause<'alloc> {
    pub binding: Option<arena::Box<'alloc, Binding<'alloc>>>,
    pub body: Block<'alloc>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct Directive {
    pub raw_value: SourceAtomSetIndex,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct FormalParameters<'alloc> {
    pub items: arena::Vec<'alloc, Parameter<'alloc>>,
    pub rest: Option<Binding<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct FunctionBody<'alloc> {
    pub directives: arena::Vec<'alloc, Directive>,
    pub statements: arena::Vec<'alloc, Statement<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct Script<'alloc> {
    pub directives: arena::Vec<'alloc, Directive>,
    pub statements: arena::Vec<'alloc, Statement<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct SwitchCase<'alloc> {
    pub test: arena::Box<'alloc, Expression<'alloc>>,
    pub consequent: arena::Vec<'alloc, Statement<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct SwitchDefault<'alloc> {
    pub consequent: arena::Vec<'alloc, Statement<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct TemplateElement {
    pub raw_value: SourceAtomSetIndex,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct VariableDeclaration<'alloc> {
    pub kind: VariableDeclarationKind,
    pub declarators: arena::Vec<'alloc, VariableDeclarator<'alloc>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub struct VariableDeclarator<'alloc> {
    pub binding: Binding<'alloc>,
    pub init: Option<arena::Box<'alloc, Expression<'alloc>>>,
    pub loc: SourceLocation,
}

#[derive(Debug, PartialEq)]
pub enum CoverParenthesized<'alloc> {
    Expression {
        expression: arena::Box<'alloc, Expression<'alloc>>,
        loc: SourceLocation,
    },
    Parameters(arena::Box<'alloc, FormalParameters<'alloc>>),
}

