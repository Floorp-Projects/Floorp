// WARNING: This file is auto-generated.

use crate::types::*;

#[derive(Debug, PartialEq, Eq, Clone, Copy, Hash)]
pub enum NodeTypeId {
    Argument,
    Arguments,
    Identifier,
    IdentifierName,
    PrivateIdentifier,
    Label,
    VariableDeclarationKind,
    CompoundAssignmentOperator,
    BinaryOperator,
    UnaryOperator,
    UpdateOperator,
    Function,
    Program,
    IfStatement,
    Statement,
    Expression,
    MemberExpression,
    OptionalChain,
    PropertyName,
    CallExpression,
    ClassElementName,
    ObjectProperty,
    NamedObjectProperty,
    MethodDefinition,
    ImportDeclaration,
    ExportDeclaration,
    VariableReference,
    BindingPattern,
    Binding,
    SimpleAssignmentTarget,
    AssignmentTargetPattern,
    AssignmentTarget,
    Parameter,
    BindingWithDefault,
    BindingIdentifier,
    AssignmentTargetIdentifier,
    ExpressionOrSuper,
    MemberAssignmentTarget,
    ComputedMemberAssignmentTarget,
    StaticMemberAssignmentTarget,
    ArrayBinding,
    ObjectBinding,
    BindingProperty,
    BindingPropertyIdentifier,
    BindingPropertyProperty,
    AssignmentTargetWithDefault,
    AssignmentTargetMaybeDefault,
    ArrayAssignmentTarget,
    ObjectAssignmentTarget,
    AssignmentTargetProperty,
    AssignmentTargetPropertyIdentifier,
    AssignmentTargetPropertyProperty,
    ClassExpression,
    ClassDeclaration,
    ClassElement,
    ModuleItems,
    Module,
    Import,
    ImportNamespace,
    ImportSpecifier,
    ExportAllFrom,
    ExportFrom,
    ExportLocals,
    Export,
    ExportDefault,
    ExportFromSpecifier,
    ExportLocalSpecifier,
    Method,
    Getter,
    Setter,
    DataProperty,
    ShorthandProperty,
    ComputedPropertyName,
    StaticPropertyName,
    ArrayExpressionElement,
    ArrayExpression,
    ArrowExpressionBody,
    ComputedMemberExpression,
    IdentifierExpression,
    ObjectExpression,
    StaticMemberExpression,
    PrivateFieldExpression,
    TemplateExpressionElement,
    TemplateExpression,
    VariableDeclarationOrAssignmentTarget,
    VariableDeclarationOrExpression,
    Block,
    CatchClause,
    Directive,
    FormalParameters,
    FunctionBody,
    Script,
    SwitchCase,
    SwitchDefault,
    TemplateElement,
    VariableDeclaration,
    VariableDeclarator,
    CoverParenthesized,
}

pub trait NodeTypeIdAccessor {
    fn get_type_id(&self) -> NodeTypeId;
}

impl<'alloc> NodeTypeIdAccessor for Argument<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Argument
    }
}

impl<'alloc> NodeTypeIdAccessor for Arguments<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Arguments
    }
}

impl<'alloc> NodeTypeIdAccessor for Identifier<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Identifier
    }
}

impl<'alloc> NodeTypeIdAccessor for IdentifierName<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::IdentifierName
    }
}

impl<'alloc> NodeTypeIdAccessor for PrivateIdentifier<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::PrivateIdentifier
    }
}

impl<'alloc> NodeTypeIdAccessor for Label<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Label
    }
}

impl<'alloc> NodeTypeIdAccessor for VariableDeclarationKind {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::VariableDeclarationKind
    }
}

impl<'alloc> NodeTypeIdAccessor for CompoundAssignmentOperator {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::CompoundAssignmentOperator
    }
}

impl<'alloc> NodeTypeIdAccessor for BinaryOperator {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::BinaryOperator
    }
}

impl<'alloc> NodeTypeIdAccessor for UnaryOperator {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::UnaryOperator
    }
}

impl<'alloc> NodeTypeIdAccessor for UpdateOperator {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::UpdateOperator
    }
}

impl<'alloc> NodeTypeIdAccessor for Function<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Function
    }
}

impl<'alloc> NodeTypeIdAccessor for Program<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Program
    }
}

impl<'alloc> NodeTypeIdAccessor for IfStatement<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::IfStatement
    }
}

impl<'alloc> NodeTypeIdAccessor for Statement<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Statement
    }
}

impl<'alloc> NodeTypeIdAccessor for Expression<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Expression
    }
}

impl<'alloc> NodeTypeIdAccessor for MemberExpression<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::MemberExpression
    }
}

impl<'alloc> NodeTypeIdAccessor for OptionalChain<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::OptionalChain
    }
}

impl<'alloc> NodeTypeIdAccessor for PropertyName<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::PropertyName
    }
}

impl<'alloc> NodeTypeIdAccessor for CallExpression<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::CallExpression
    }
}

impl<'alloc> NodeTypeIdAccessor for ClassElementName<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ClassElementName
    }
}

impl<'alloc> NodeTypeIdAccessor for ObjectProperty<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ObjectProperty
    }
}

impl<'alloc> NodeTypeIdAccessor for NamedObjectProperty<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::NamedObjectProperty
    }
}

impl<'alloc> NodeTypeIdAccessor for MethodDefinition<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::MethodDefinition
    }
}

impl<'alloc> NodeTypeIdAccessor for ImportDeclaration<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ImportDeclaration
    }
}

impl<'alloc> NodeTypeIdAccessor for ExportDeclaration<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ExportDeclaration
    }
}

impl<'alloc> NodeTypeIdAccessor for VariableReference<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::VariableReference
    }
}

impl<'alloc> NodeTypeIdAccessor for BindingPattern<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::BindingPattern
    }
}

impl<'alloc> NodeTypeIdAccessor for Binding<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Binding
    }
}

impl<'alloc> NodeTypeIdAccessor for SimpleAssignmentTarget<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::SimpleAssignmentTarget
    }
}

impl<'alloc> NodeTypeIdAccessor for AssignmentTargetPattern<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::AssignmentTargetPattern
    }
}

impl<'alloc> NodeTypeIdAccessor for AssignmentTarget<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::AssignmentTarget
    }
}

impl<'alloc> NodeTypeIdAccessor for Parameter<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Parameter
    }
}

impl<'alloc> NodeTypeIdAccessor for BindingWithDefault<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::BindingWithDefault
    }
}

impl<'alloc> NodeTypeIdAccessor for BindingIdentifier<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::BindingIdentifier
    }
}

impl<'alloc> NodeTypeIdAccessor for AssignmentTargetIdentifier<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::AssignmentTargetIdentifier
    }
}

impl<'alloc> NodeTypeIdAccessor for ExpressionOrSuper<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ExpressionOrSuper
    }
}

impl<'alloc> NodeTypeIdAccessor for MemberAssignmentTarget<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::MemberAssignmentTarget
    }
}

impl<'alloc> NodeTypeIdAccessor for ComputedMemberAssignmentTarget<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ComputedMemberAssignmentTarget
    }
}

impl<'alloc> NodeTypeIdAccessor for StaticMemberAssignmentTarget<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::StaticMemberAssignmentTarget
    }
}

impl<'alloc> NodeTypeIdAccessor for ArrayBinding<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ArrayBinding
    }
}

impl<'alloc> NodeTypeIdAccessor for ObjectBinding<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ObjectBinding
    }
}

impl<'alloc> NodeTypeIdAccessor for BindingProperty<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::BindingProperty
    }
}

impl<'alloc> NodeTypeIdAccessor for BindingPropertyIdentifier<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::BindingPropertyIdentifier
    }
}

impl<'alloc> NodeTypeIdAccessor for BindingPropertyProperty<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::BindingPropertyProperty
    }
}

impl<'alloc> NodeTypeIdAccessor for AssignmentTargetWithDefault<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::AssignmentTargetWithDefault
    }
}

impl<'alloc> NodeTypeIdAccessor for AssignmentTargetMaybeDefault<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::AssignmentTargetMaybeDefault
    }
}

impl<'alloc> NodeTypeIdAccessor for ArrayAssignmentTarget<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ArrayAssignmentTarget
    }
}

impl<'alloc> NodeTypeIdAccessor for ObjectAssignmentTarget<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ObjectAssignmentTarget
    }
}

impl<'alloc> NodeTypeIdAccessor for AssignmentTargetProperty<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::AssignmentTargetProperty
    }
}

impl<'alloc> NodeTypeIdAccessor for AssignmentTargetPropertyIdentifier<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::AssignmentTargetPropertyIdentifier
    }
}

impl<'alloc> NodeTypeIdAccessor for AssignmentTargetPropertyProperty<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::AssignmentTargetPropertyProperty
    }
}

impl<'alloc> NodeTypeIdAccessor for ClassExpression<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ClassExpression
    }
}

impl<'alloc> NodeTypeIdAccessor for ClassDeclaration<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ClassDeclaration
    }
}

impl<'alloc> NodeTypeIdAccessor for ClassElement<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ClassElement
    }
}

impl<'alloc> NodeTypeIdAccessor for ModuleItems<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ModuleItems
    }
}

impl<'alloc> NodeTypeIdAccessor for Module<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Module
    }
}

impl<'alloc> NodeTypeIdAccessor for Import<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Import
    }
}

impl<'alloc> NodeTypeIdAccessor for ImportNamespace<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ImportNamespace
    }
}

impl<'alloc> NodeTypeIdAccessor for ImportSpecifier<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ImportSpecifier
    }
}

impl<'alloc> NodeTypeIdAccessor for ExportAllFrom<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ExportAllFrom
    }
}

impl<'alloc> NodeTypeIdAccessor for ExportFrom<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ExportFrom
    }
}

impl<'alloc> NodeTypeIdAccessor for ExportLocals<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ExportLocals
    }
}

impl<'alloc> NodeTypeIdAccessor for Export<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Export
    }
}

impl<'alloc> NodeTypeIdAccessor for ExportDefault<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ExportDefault
    }
}

impl<'alloc> NodeTypeIdAccessor for ExportFromSpecifier<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ExportFromSpecifier
    }
}

impl<'alloc> NodeTypeIdAccessor for ExportLocalSpecifier<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ExportLocalSpecifier
    }
}

impl<'alloc> NodeTypeIdAccessor for Method<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Method
    }
}

impl<'alloc> NodeTypeIdAccessor for Getter<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Getter
    }
}

impl<'alloc> NodeTypeIdAccessor for Setter<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Setter
    }
}

impl<'alloc> NodeTypeIdAccessor for DataProperty<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::DataProperty
    }
}

impl<'alloc> NodeTypeIdAccessor for ShorthandProperty<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ShorthandProperty
    }
}

impl<'alloc> NodeTypeIdAccessor for ComputedPropertyName<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ComputedPropertyName
    }
}

impl<'alloc> NodeTypeIdAccessor for StaticPropertyName<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::StaticPropertyName
    }
}

impl<'alloc> NodeTypeIdAccessor for ArrayExpressionElement<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ArrayExpressionElement
    }
}

impl<'alloc> NodeTypeIdAccessor for ArrayExpression<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ArrayExpression
    }
}

impl<'alloc> NodeTypeIdAccessor for ArrowExpressionBody<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ArrowExpressionBody
    }
}

impl<'alloc> NodeTypeIdAccessor for ComputedMemberExpression<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ComputedMemberExpression
    }
}

impl<'alloc> NodeTypeIdAccessor for IdentifierExpression<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::IdentifierExpression
    }
}

impl<'alloc> NodeTypeIdAccessor for ObjectExpression<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::ObjectExpression
    }
}

impl<'alloc> NodeTypeIdAccessor for StaticMemberExpression<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::StaticMemberExpression
    }
}

impl<'alloc> NodeTypeIdAccessor for PrivateFieldExpression<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::PrivateFieldExpression
    }
}

impl<'alloc> NodeTypeIdAccessor for TemplateExpressionElement<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::TemplateExpressionElement
    }
}

impl<'alloc> NodeTypeIdAccessor for TemplateExpression<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::TemplateExpression
    }
}

impl<'alloc> NodeTypeIdAccessor for VariableDeclarationOrAssignmentTarget<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::VariableDeclarationOrAssignmentTarget
    }
}

impl<'alloc> NodeTypeIdAccessor for VariableDeclarationOrExpression<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::VariableDeclarationOrExpression
    }
}

impl<'alloc> NodeTypeIdAccessor for Block<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Block
    }
}

impl<'alloc> NodeTypeIdAccessor for CatchClause<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::CatchClause
    }
}

impl<'alloc> NodeTypeIdAccessor for Directive<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Directive
    }
}

impl<'alloc> NodeTypeIdAccessor for FormalParameters<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::FormalParameters
    }
}

impl<'alloc> NodeTypeIdAccessor for FunctionBody<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::FunctionBody
    }
}

impl<'alloc> NodeTypeIdAccessor for Script<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::Script
    }
}

impl<'alloc> NodeTypeIdAccessor for SwitchCase<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::SwitchCase
    }
}

impl<'alloc> NodeTypeIdAccessor for SwitchDefault<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::SwitchDefault
    }
}

impl<'alloc> NodeTypeIdAccessor for TemplateElement<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::TemplateElement
    }
}

impl<'alloc> NodeTypeIdAccessor for VariableDeclaration<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::VariableDeclaration
    }
}

impl<'alloc> NodeTypeIdAccessor for VariableDeclarator<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::VariableDeclarator
    }
}

impl<'alloc> NodeTypeIdAccessor for CoverParenthesized<'alloc> {
    fn get_type_id(&self) -> NodeTypeId {
        NodeTypeId::CoverParenthesized
    }
}

