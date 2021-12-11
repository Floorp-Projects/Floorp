//! AST visitors (i.e. on-the-fly mutation at different places in the AST).
//!
//! Visitors are mutable objects that can mutate parts of an AST while traversing it. You can see
//! them as flexible mutations taking place on *patterns* representing your AST – they get called
//! everytime an interesting node gets visited. Because of their mutable nature, you can accumulate
//! a state as you traverse the AST and implement exotic filtering.
//!
//! Visitors must implement the [`Visitor`] trait in order to be usable.
//!
//! In order to visit any part of an AST (from its very top root or from any part of it), you must
//! use the [`Host`] interface, that provides the [`Host::visit`] function.
//!
//! For instance, we can imagine visiting an AST to count how many variables are declared:
//!
//! ```
//! use glsl::syntax::{CompoundStatement, Expr, SingleDeclaration, Statement, TypeSpecifierNonArray};
//! use glsl::visitor::{Host, Visit, Visitor};
//! use std::iter::FromIterator;
//!
//! let decl0 = Statement::declare_var(
//!   TypeSpecifierNonArray::Float,
//!   "x",
//!   None,
//!   Some(Expr::from(3.14).into())
//! );
//!
//! let decl1 = Statement::declare_var(
//!   TypeSpecifierNonArray::Int,
//!   "y",
//!   None,
//!   None
//! );
//!
//! let decl2 = Statement::declare_var(
//!   TypeSpecifierNonArray::Vec4,
//!   "z",
//!   None,
//!   None
//! );
//!
//! let mut compound = CompoundStatement::from_iter(vec![decl0, decl1, decl2]);
//!
//! // our visitor that will count the number of variables it saw
//! struct Counter {
//!   var_nb: usize
//! }
//!
//! impl Visitor for Counter {
//!   // we are only interested in single declaration with a name
//!   fn visit_single_declaration(&mut self, declaration: &mut SingleDeclaration) -> Visit {
//!     if declaration.name.is_some() {
//!       self.var_nb += 1;
//!     }
//!
//!     // do not go deeper
//!     Visit::Parent
//!   }
//! }
//!
//! let mut counter = Counter { var_nb: 0 };
//! compound.visit(&mut counter);
//! assert_eq!(counter.var_nb, 3);
//! ```
//!
//! [`Host`]: visitor::Host
//! [`Host::visit`]: visitor::Host::visit
//! [`Visitor`]: visitor::Visitor

use crate::syntax;

/// Visit strategy after having visited an AST node.
///
/// Some AST nodes have *children* – in enum’s variants, in some fields as nested in [`Vec`], etc.
/// Those nodes can be visited depending on the strategy you chose.
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum Visit {
  /// The visitor will go deeper in the AST by visiting all the children, if any. If no children are
  /// present or if having children doesn’t make sense for a specific part of the AST, this
  /// strategy will be ignored.
  Children,
  /// The visitor won’t visit children nor siblings and will go up.
  Parent,
}

/// Visitor object, visiting AST nodes.
pub trait Visitor {
  fn visit_translation_unit(&mut self, _: &mut syntax::TranslationUnit) -> Visit {
    Visit::Children
  }

  fn visit_external_declaration(&mut self, _: &mut syntax::ExternalDeclaration) -> Visit {
    Visit::Children
  }

  fn visit_identifier(&mut self, _: &mut syntax::Identifier) -> Visit {
    Visit::Children
  }

  fn visit_arrayed_identifier(&mut self, _: &mut syntax::ArrayedIdentifier) -> Visit {
    Visit::Children
  }

  fn visit_type_name(&mut self, _: &mut syntax::TypeName) -> Visit {
    Visit::Children
  }

  fn visit_block(&mut self, _: &mut syntax::Block) -> Visit {
    Visit::Children
  }

  fn visit_for_init_statement(&mut self, _: &mut syntax::ForInitStatement) -> Visit {
    Visit::Children
  }

  fn visit_for_rest_statement(&mut self, _: &mut syntax::ForRestStatement) -> Visit {
    Visit::Children
  }

  fn visit_function_definition(&mut self, _: &mut syntax::FunctionDefinition) -> Visit {
    Visit::Children
  }

  fn visit_function_parameter_declarator(
    &mut self,
    _: &mut syntax::FunctionParameterDeclarator,
  ) -> Visit {
    Visit::Children
  }

  fn visit_function_prototype(&mut self, _: &mut syntax::FunctionPrototype) -> Visit {
    Visit::Children
  }

  fn visit_init_declarator_list(&mut self, _: &mut syntax::InitDeclaratorList) -> Visit {
    Visit::Children
  }

  fn visit_layout_qualifier(&mut self, _: &mut syntax::LayoutQualifier) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor(&mut self, _: &mut syntax::Preprocessor) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_define(&mut self, _: &mut syntax::PreprocessorDefine) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_elseif(&mut self, _: &mut syntax::PreprocessorElseIf) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_error(&mut self, _: &mut syntax::PreprocessorError) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_extension(&mut self, _: &mut syntax::PreprocessorExtension) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_extension_behavior(
    &mut self,
    _: &mut syntax::PreprocessorExtensionBehavior,
  ) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_extension_name(
    &mut self,
    _: &mut syntax::PreprocessorExtensionName,
  ) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_if(&mut self, _: &mut syntax::PreprocessorIf) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_ifdef(&mut self, _: &mut syntax::PreprocessorIfDef) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_ifndef(&mut self, _: &mut syntax::PreprocessorIfNDef) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_include(&mut self, _: &mut syntax::PreprocessorInclude) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_line(&mut self, _: &mut syntax::PreprocessorLine) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_pragma(&mut self, _: &mut syntax::PreprocessorPragma) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_undef(&mut self, _: &mut syntax::PreprocessorUndef) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_version(&mut self, _: &mut syntax::PreprocessorVersion) -> Visit {
    Visit::Children
  }

  fn visit_preprocessor_version_profile(
    &mut self,
    _: &mut syntax::PreprocessorVersionProfile,
  ) -> Visit {
    Visit::Children
  }

  fn visit_selection_statement(&mut self, _: &mut syntax::SelectionStatement) -> Visit {
    Visit::Children
  }

  fn visit_selection_rest_statement(&mut self, _: &mut syntax::SelectionRestStatement) -> Visit {
    Visit::Children
  }

  fn visit_single_declaration(&mut self, _: &mut syntax::SingleDeclaration) -> Visit {
    Visit::Children
  }

  fn visit_single_declaration_no_type(&mut self, _: &mut syntax::SingleDeclarationNoType) -> Visit {
    Visit::Children
  }

  fn visit_struct_field_specifier(&mut self, _: &mut syntax::StructFieldSpecifier) -> Visit {
    Visit::Children
  }

  fn visit_struct_specifier(&mut self, _: &mut syntax::StructSpecifier) -> Visit {
    Visit::Children
  }

  fn visit_switch_statement(&mut self, _: &mut syntax::SwitchStatement) -> Visit {
    Visit::Children
  }

  fn visit_type_qualifier(&mut self, _: &mut syntax::TypeQualifier) -> Visit {
    Visit::Children
  }

  fn visit_type_specifier(&mut self, _: &mut syntax::TypeSpecifier) -> Visit {
    Visit::Children
  }

  fn visit_full_specified_type(&mut self, _: &mut syntax::FullySpecifiedType) -> Visit {
    Visit::Children
  }

  fn visit_array_specifier(&mut self, _: &mut syntax::ArraySpecifier) -> Visit {
    Visit::Children
  }

  fn visit_assignment_op(&mut self, _: &mut syntax::AssignmentOp) -> Visit {
    Visit::Children
  }

  fn visit_binary_op(&mut self, _: &mut syntax::BinaryOp) -> Visit {
    Visit::Children
  }

  fn visit_case_label(&mut self, _: &mut syntax::CaseLabel) -> Visit {
    Visit::Children
  }

  fn visit_condition(&mut self, _: &mut syntax::Condition) -> Visit {
    Visit::Children
  }

  fn visit_declaration(&mut self, _: &mut syntax::Declaration) -> Visit {
    Visit::Children
  }

  fn visit_expr(&mut self, _: &mut syntax::Expr) -> Visit {
    Visit::Children
  }

  fn visit_fun_identifier(&mut self, _: &mut syntax::FunIdentifier) -> Visit {
    Visit::Children
  }

  fn visit_function_parameter_declaration(
    &mut self,
    _: &mut syntax::FunctionParameterDeclaration,
  ) -> Visit {
    Visit::Children
  }

  fn visit_initializer(&mut self, _: &mut syntax::Initializer) -> Visit {
    Visit::Children
  }

  fn visit_interpolation_qualifier(&mut self, _: &mut syntax::InterpolationQualifier) -> Visit {
    Visit::Children
  }

  fn visit_iteration_statement(&mut self, _: &mut syntax::IterationStatement) -> Visit {
    Visit::Children
  }

  fn visit_jump_statement(&mut self, _: &mut syntax::JumpStatement) -> Visit {
    Visit::Children
  }

  fn visit_layout_qualifier_spec(&mut self, _: &mut syntax::LayoutQualifierSpec) -> Visit {
    Visit::Children
  }

  fn visit_precision_qualifier(&mut self, _: &mut syntax::PrecisionQualifier) -> Visit {
    Visit::Children
  }

  fn visit_statement(&mut self, _: &mut syntax::Statement) -> Visit {
    Visit::Children
  }

  fn visit_compound_statement(&mut self, _: &mut syntax::CompoundStatement) -> Visit {
    Visit::Children
  }

  fn visit_simple_statement(&mut self, _: &mut syntax::SimpleStatement) -> Visit {
    Visit::Children
  }

  fn visit_storage_qualifier(&mut self, _: &mut syntax::StorageQualifier) -> Visit {
    Visit::Children
  }

  fn visit_type_qualifier_spec(&mut self, _: &mut syntax::TypeQualifierSpec) -> Visit {
    Visit::Children
  }

  fn visit_type_specifier_non_array(&mut self, _: &mut syntax::TypeSpecifierNonArray) -> Visit {
    Visit::Children
  }

  fn visit_unary_op(&mut self, _: &mut syntax::UnaryOp) -> Visit {
    Visit::Children
  }

  fn visit_expr_statement(&mut self, _: &mut syntax::ExprStatement) -> Visit {
    Visit::Children
  }
}

/// Part of the AST that can be visited.
///
/// You shouldn’t have to worry about this type nor how to implement it – it’s completely
/// implemented for you. However, it works in a pretty simple way: any implementor of [`Host`] can
/// be used with a [`Visitor`].
///
/// The idea is that visiting an AST node is a two-step process:
///
///   - First, you *can* get your visitor called once as soon as an interesting node gets visited.
///     For instance, if your visitor has an implementation for visiting expressions, everytime an
///     expression gets visited, your visitor will run.
///   - If your implementation of visiting an AST node returns [`Visit::Children`] and if the given
///     node has children, the visitor will go deeper, invoking other calls if you have defined any.
///     A typical pattern you might want to do is to implement your visitor to gets run on all
///     typenames. Since expressions contains variables, you will get your visitor called once again
///     there.
///   - Notice that since visitors are mutable, you can accumulate a state as you go deeper in the
///     AST to implement various checks and validations.
pub trait Host {
  /// Visit an AST node.
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor;
}

impl<T> Host for Option<T>
where
  T: Host,
{
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    if let Some(ref mut x) = *self {
      x.visit(visitor);
    }
  }
}

impl<T> Host for Box<T>
where
  T: Host,
{
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    (**self).visit(visitor);
  }
}

impl Host for syntax::TranslationUnit {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_translation_unit(self);

    if visit == Visit::Children {
      for ed in &mut (self.0).0 {
        ed.visit(visitor);
      }
    }
  }
}

impl Host for syntax::ExternalDeclaration {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_external_declaration(self);

    if visit == Visit::Children {
      match *self {
        syntax::ExternalDeclaration::Preprocessor(ref mut p) => p.visit(visitor),
        syntax::ExternalDeclaration::FunctionDefinition(ref mut fd) => fd.visit(visitor),
        syntax::ExternalDeclaration::Declaration(ref mut d) => d.visit(visitor),
      }
    }
  }
}

impl Host for syntax::Preprocessor {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_preprocessor(self);

    if visit == Visit::Children {
      match *self {
        syntax::Preprocessor::Define(ref mut pd) => pd.visit(visitor),
        syntax::Preprocessor::Else => (),
        syntax::Preprocessor::ElseIf(ref mut pei) => pei.visit(visitor),
        syntax::Preprocessor::EndIf => (),
        syntax::Preprocessor::Error(ref mut pe) => pe.visit(visitor),
        syntax::Preprocessor::If(ref mut pi) => pi.visit(visitor),
        syntax::Preprocessor::IfDef(ref mut pid) => pid.visit(visitor),
        syntax::Preprocessor::IfNDef(ref mut pind) => pind.visit(visitor),
        syntax::Preprocessor::Include(ref mut pi) => pi.visit(visitor),
        syntax::Preprocessor::Line(ref mut pl) => pl.visit(visitor),
        syntax::Preprocessor::Pragma(ref mut pp) => pp.visit(visitor),
        syntax::Preprocessor::Undef(ref mut pu) => pu.visit(visitor),
        syntax::Preprocessor::Version(ref mut pv) => pv.visit(visitor),
        syntax::Preprocessor::Extension(ref mut ext) => ext.visit(visitor),
      }
    }
  }
}

impl Host for syntax::PreprocessorDefine {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_preprocessor_define(self);

    if visit == Visit::Children {
      match *self {
        syntax::PreprocessorDefine::ObjectLike { ref mut ident, .. } => {
          ident.visit(visitor);
        }

        syntax::PreprocessorDefine::FunctionLike {
          ref mut ident,
          ref mut args,
          ..
        } => {
          ident.visit(visitor);

          for arg in args {
            arg.visit(visitor);
          }
        }
      }
    }
  }
}

impl Host for syntax::PreprocessorElseIf {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_preprocessor_elseif(self);
  }
}

impl Host for syntax::PreprocessorError {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_preprocessor_error(self);
  }
}

impl Host for syntax::PreprocessorIf {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_preprocessor_if(self);
  }
}

impl Host for syntax::PreprocessorIfDef {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_preprocessor_ifdef(self);

    if visit == Visit::Children {
      self.ident.visit(visitor);
    }
  }
}

impl Host for syntax::PreprocessorIfNDef {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_preprocessor_ifndef(self);

    if visit == Visit::Children {
      self.ident.visit(visitor);
    }
  }
}

impl Host for syntax::PreprocessorInclude {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_preprocessor_include(self);
  }
}

impl Host for syntax::PreprocessorLine {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_preprocessor_line(self);
  }
}

impl Host for syntax::PreprocessorPragma {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_preprocessor_pragma(self);
  }
}

impl Host for syntax::PreprocessorUndef {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_preprocessor_undef(self);

    if visit == Visit::Children {
      self.name.visit(visitor);
    }
  }
}

impl Host for syntax::PreprocessorVersion {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_preprocessor_version(self);

    if visit == Visit::Children {
      self.profile.visit(visitor);
    }
  }
}

impl Host for syntax::PreprocessorVersionProfile {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_preprocessor_version_profile(self);
  }
}

impl Host for syntax::PreprocessorExtension {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_preprocessor_extension(self);

    if visit == Visit::Children {
      self.name.visit(visitor);
      self.behavior.visit(visitor);
    }
  }
}

impl Host for syntax::PreprocessorExtensionBehavior {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_preprocessor_extension_behavior(self);
  }
}

impl Host for syntax::PreprocessorExtensionName {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_preprocessor_extension_name(self);
  }
}

impl Host for syntax::FunctionPrototype {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_function_prototype(self);

    if visit == Visit::Children {
      self.ty.visit(visitor);
      self.name.visit(visitor);

      for param in &mut self.parameters {
        param.visit(visitor);
      }
    }
  }
}

impl Host for syntax::FunctionParameterDeclaration {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_function_parameter_declaration(self);

    if visit == Visit::Children {
      match *self {
        syntax::FunctionParameterDeclaration::Named(ref mut tq, ref mut fpd) => {
          tq.visit(visitor);
          fpd.visit(visitor);
        }

        syntax::FunctionParameterDeclaration::Unnamed(ref mut tq, ref mut ty) => {
          tq.visit(visitor);
          ty.visit(visitor);
        }
      }
    }
  }
}

impl Host for syntax::FunctionParameterDeclarator {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_function_parameter_declarator(self);

    if visit == Visit::Children {
      self.ty.visit(visitor);
      self.ident.visit(visitor);
    }
  }
}

impl Host for syntax::FunctionDefinition {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_function_definition(self);

    if visit == Visit::Children {
      self.prototype.visit(visitor);
      self.statement.visit(visitor);
    }
  }
}

impl Host for syntax::Declaration {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_declaration(self);

    if visit == Visit::Children {
      match *self {
        syntax::Declaration::FunctionPrototype(ref mut fp) => fp.visit(visitor),

        syntax::Declaration::InitDeclaratorList(ref mut idl) => idl.visit(visitor),

        syntax::Declaration::Precision(ref mut pq, ref mut ty) => {
          pq.visit(visitor);
          ty.visit(visitor);
        }

        syntax::Declaration::Block(ref mut block) => block.visit(visitor),

        syntax::Declaration::Global(ref mut tq, ref mut idents) => {
          tq.visit(visitor);

          for ident in idents {
            ident.visit(visitor);
          }
        }
      }
    }
  }
}

impl Host for syntax::Block {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_block(self);

    if visit == Visit::Children {
      self.qualifier.visit(visitor);
      self.name.visit(visitor);

      for field in &mut self.fields {
        field.visit(visitor);
      }

      self.identifier.visit(visitor);
    }
  }
}

impl Host for syntax::InitDeclaratorList {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_init_declarator_list(self);

    if visit == Visit::Children {
      self.head.visit(visitor);

      for d in &mut self.tail {
        d.visit(visitor);
      }
    }
  }
}

impl Host for syntax::SingleDeclaration {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_single_declaration(self);

    if visit == Visit::Children {
      self.ty.visit(visitor);
      self.name.visit(visitor);
      self.array_specifier.visit(visitor);
      self.initializer.visit(visitor);
    }
  }
}

impl Host for syntax::SingleDeclarationNoType {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_single_declaration_no_type(self);

    if visit == Visit::Children {
      self.ident.visit(visitor);
      self.initializer.visit(visitor);
    }
  }
}

impl Host for syntax::FullySpecifiedType {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_full_specified_type(self);

    if visit == Visit::Children {
      self.qualifier.visit(visitor);
      self.ty.visit(visitor);
    }
  }
}

impl Host for syntax::TypeSpecifier {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_type_specifier(self);

    if visit == Visit::Children {
      self.ty.visit(visitor);
      self.array_specifier.visit(visitor);
    }
  }
}

impl Host for syntax::TypeSpecifierNonArray {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_type_specifier_non_array(self);

    if visit == Visit::Children {
      match *self {
        syntax::TypeSpecifierNonArray::Struct(ref mut ss) => ss.visit(visitor),
        syntax::TypeSpecifierNonArray::TypeName(ref mut tn) => tn.visit(visitor),
        _ => (),
      }
    }
  }
}

impl Host for syntax::TypeQualifier {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_type_qualifier(self);

    if visit == Visit::Children {
      for tqs in &mut self.qualifiers.0 {
        tqs.visit(visitor);
      }
    }
  }
}

impl Host for syntax::TypeQualifierSpec {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_type_qualifier_spec(self);

    if visit == Visit::Children {
      match *self {
        syntax::TypeQualifierSpec::Storage(ref mut sq) => sq.visit(visitor),
        syntax::TypeQualifierSpec::Layout(ref mut lq) => lq.visit(visitor),
        syntax::TypeQualifierSpec::Precision(ref mut pq) => pq.visit(visitor),
        syntax::TypeQualifierSpec::Interpolation(ref mut iq) => iq.visit(visitor),
        _ => (),
      }
    }
  }
}

impl Host for syntax::StorageQualifier {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_storage_qualifier(self);

    if visit == Visit::Children {
      if let syntax::StorageQualifier::Subroutine(ref mut names) = *self {
        for name in names {
          name.visit(visitor);
        }
      }
    }
  }
}

impl Host for syntax::LayoutQualifier {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_layout_qualifier(self);

    if visit == Visit::Children {
      for lqs in &mut self.ids.0 {
        lqs.visit(visitor);
      }
    }
  }
}

impl Host for syntax::LayoutQualifierSpec {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_layout_qualifier_spec(self);

    if visit == Visit::Children {
      if let syntax::LayoutQualifierSpec::Identifier(ref mut ident, ref mut expr) = *self {
        ident.visit(visitor);

        if let Some(ref mut e) = *expr {
          e.visit(visitor);
        }
      }
    }
  }
}

impl Host for syntax::PrecisionQualifier {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_precision_qualifier(self);
  }
}

impl Host for syntax::InterpolationQualifier {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_interpolation_qualifier(self);
  }
}

impl Host for syntax::TypeName {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_type_name(self);
  }
}

impl Host for syntax::Identifier {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_identifier(self);
  }
}

impl Host for syntax::ArrayedIdentifier {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_arrayed_identifier(self);

    if visit == Visit::Children {
      self.ident.visit(visitor);
      self.array_spec.visit(visitor);
    }
  }
}

impl Host for syntax::Expr {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_expr(self);

    if visit == Visit::Children {
      match *self {
        syntax::Expr::Variable(ref mut ident) => ident.visit(visitor),

        syntax::Expr::Unary(ref mut op, ref mut e) => {
          op.visit(visitor);
          e.visit(visitor);
        }

        syntax::Expr::Binary(ref mut op, ref mut a, ref mut b) => {
          op.visit(visitor);
          a.visit(visitor);
          b.visit(visitor);
        }

        syntax::Expr::Ternary(ref mut a, ref mut b, ref mut c) => {
          a.visit(visitor);
          b.visit(visitor);
          c.visit(visitor);
        }

        syntax::Expr::Assignment(ref mut lhs, ref mut op, ref mut rhs) => {
          lhs.visit(visitor);
          op.visit(visitor);
          rhs.visit(visitor);
        }

        syntax::Expr::Bracket(ref mut e, ref mut arr_spec) => {
          e.visit(visitor);
          arr_spec.visit(visitor);
        }

        syntax::Expr::FunCall(ref mut fi, ref mut params) => {
          fi.visit(visitor);

          for param in params {
            param.visit(visitor);
          }
        }

        syntax::Expr::Dot(ref mut e, ref mut i) => {
          e.visit(visitor);
          i.visit(visitor);
        }

        syntax::Expr::PostInc(ref mut e) => e.visit(visitor),

        syntax::Expr::PostDec(ref mut e) => e.visit(visitor),

        syntax::Expr::Comma(ref mut a, ref mut b) => {
          a.visit(visitor);
          b.visit(visitor);
        }

        _ => (),
      }
    }
  }
}

impl Host for syntax::UnaryOp {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_unary_op(self);
  }
}

impl Host for syntax::BinaryOp {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_binary_op(self);
  }
}

impl Host for syntax::AssignmentOp {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let _ = visitor.visit_assignment_op(self);
  }
}

impl Host for syntax::ArraySpecifier {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_array_specifier(self);

    if visit == Visit::Children {
      if let syntax::ArraySpecifier::ExplicitlySized(ref mut e) = *self {
        e.visit(visitor);
      }
    }
  }
}

impl Host for syntax::FunIdentifier {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_fun_identifier(self);

    if visit == Visit::Children {
      match *self {
        syntax::FunIdentifier::Identifier(ref mut i) => i.visit(visitor),
        syntax::FunIdentifier::Expr(ref mut e) => e.visit(visitor),
      }
    }
  }
}

impl Host for syntax::StructSpecifier {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_struct_specifier(self);

    if visit == Visit::Children {
      self.name.visit(visitor);

      for field in &mut self.fields.0 {
        field.visit(visitor);
      }
    }
  }
}

impl Host for syntax::StructFieldSpecifier {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_struct_field_specifier(self);

    if visit == Visit::Children {
      self.qualifier.visit(visitor);
      self.ty.visit(visitor);

      for identifier in &mut self.identifiers.0 {
        identifier.visit(visitor);
      }
    }
  }
}

impl Host for syntax::Statement {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_statement(self);

    if visit == Visit::Children {
      match *self {
        syntax::Statement::Compound(ref mut cs) => cs.visit(visitor),
        syntax::Statement::Simple(ref mut ss) => ss.visit(visitor),
      }
    }
  }
}

impl Host for syntax::SimpleStatement {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_simple_statement(self);

    if visit == Visit::Children {
      match *self {
        syntax::SimpleStatement::Declaration(ref mut d) => d.visit(visitor),
        syntax::SimpleStatement::Expression(ref mut e) => e.visit(visitor),
        syntax::SimpleStatement::Selection(ref mut s) => s.visit(visitor),
        syntax::SimpleStatement::Switch(ref mut s) => s.visit(visitor),
        syntax::SimpleStatement::CaseLabel(ref mut cl) => cl.visit(visitor),
        syntax::SimpleStatement::Iteration(ref mut i) => i.visit(visitor),
        syntax::SimpleStatement::Jump(ref mut j) => j.visit(visitor),
      }
    }
  }
}

impl Host for syntax::CompoundStatement {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_compound_statement(self);

    if visit == Visit::Children {
      for stmt in &mut self.statement_list {
        stmt.visit(visitor);
      }
    }
  }
}

impl Host for syntax::SelectionStatement {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_selection_statement(self);

    if visit == Visit::Children {
      self.cond.visit(visitor);
      self.rest.visit(visitor);
    }
  }
}

impl Host for syntax::SelectionRestStatement {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_selection_rest_statement(self);

    if visit == Visit::Children {
      match *self {
        syntax::SelectionRestStatement::Statement(ref mut s) => s.visit(visitor),

        syntax::SelectionRestStatement::Else(ref mut a, ref mut b) => {
          a.visit(visitor);
          b.visit(visitor);
        }
      }
    }
  }
}

impl Host for syntax::SwitchStatement {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_switch_statement(self);

    if visit == Visit::Children {
      self.head.visit(visitor);

      for s in &mut self.body {
        s.visit(visitor);
      }
    }
  }
}

impl Host for syntax::CaseLabel {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_case_label(self);

    if visit == Visit::Children {
      if let syntax::CaseLabel::Case(ref mut e) = *self {
        e.visit(visitor);
      }
    }
  }
}

impl Host for syntax::IterationStatement {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_iteration_statement(self);

    if visit == Visit::Children {
      match *self {
        syntax::IterationStatement::While(ref mut c, ref mut s) => {
          c.visit(visitor);
          s.visit(visitor);
        }

        syntax::IterationStatement::DoWhile(ref mut s, ref mut e) => {
          s.visit(visitor);
          e.visit(visitor);
        }

        syntax::IterationStatement::For(ref mut fis, ref mut frs, ref mut s) => {
          fis.visit(visitor);
          frs.visit(visitor);
          s.visit(visitor);
        }
      }
    }
  }
}

impl Host for syntax::ForInitStatement {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_for_init_statement(self);

    if visit == Visit::Children {
      match *self {
        syntax::ForInitStatement::Expression(ref mut e) => e.visit(visitor),
        syntax::ForInitStatement::Declaration(ref mut d) => d.visit(visitor),
      }
    }
  }
}

impl Host for syntax::ForRestStatement {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_for_rest_statement(self);

    if visit == Visit::Children {
      self.condition.visit(visitor);
      self.post_expr.visit(visitor);
    }
  }
}

impl Host for syntax::JumpStatement {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_jump_statement(self);

    if visit == Visit::Children {
      if let syntax::JumpStatement::Return(ref mut r) = *self {
        r.visit(visitor);
      }
    }
  }
}

impl Host for syntax::Condition {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_condition(self);

    if visit == Visit::Children {
      match *self {
        syntax::Condition::Expr(ref mut e) => e.visit(visitor),

        syntax::Condition::Assignment(ref mut fst, ref mut ident, ref mut init) => {
          fst.visit(visitor);
          ident.visit(visitor);
          init.visit(visitor);
        }
      }
    }
  }
}

impl Host for syntax::Initializer {
  fn visit<V>(&mut self, visitor: &mut V)
  where
    V: Visitor,
  {
    let visit = visitor.visit_initializer(self);

    if visit == Visit::Children {
      match *self {
        syntax::Initializer::Simple(ref mut e) => e.visit(visitor),

        syntax::Initializer::List(ref mut i) => {
          for i in &mut i.0 {
            i.visit(visitor);
          }
        }
      }
    }
  }
}

#[cfg(test)]
mod tests {
  use std::iter::FromIterator;

  use super::*;
  use syntax;

  #[test]
  fn count_variables() {
    let decl0 = syntax::Statement::declare_var(
      syntax::TypeSpecifierNonArray::Float,
      "x",
      None,
      Some(syntax::Expr::from(3.14).into()),
    );

    let decl1 = syntax::Statement::declare_var(syntax::TypeSpecifierNonArray::Int, "y", None, None);

    let decl2 =
      syntax::Statement::declare_var(syntax::TypeSpecifierNonArray::Vec4, "z", None, None);

    let mut compound = syntax::CompoundStatement::from_iter(vec![decl0, decl1, decl2]);

    // our visitor that will count the number of variables it saw
    struct Counter {
      var_nb: usize,
    }

    impl Visitor for Counter {
      // we are only interested in single declaration with a name
      fn visit_single_declaration(&mut self, declaration: &mut syntax::SingleDeclaration) -> Visit {
        if declaration.name.is_some() {
          self.var_nb += 1;
        }

        // do not go deeper
        Visit::Parent
      }
    }

    let mut counter = Counter { var_nb: 0 };
    compound.visit(&mut counter);
    assert_eq!(counter.var_nb, 3);
  }
}
