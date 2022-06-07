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
//! let compound = CompoundStatement::from_iter(vec![decl0, decl1, decl2]);
//!
//! // our visitor that will count the number of variables it saw
//! struct Counter {
//!   var_nb: usize
//! }
//!
//! impl Visitor for Counter {
//!   // we are only interested in single declaration with a name
//!   fn visit_single_declaration(&mut self, declaration: &SingleDeclaration) -> Visit {
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
//! [`Host`]: crate::visitor::Host
//! [`Host::visit`]: crate::visitor::Host::visit
//! [`Visitor`]: crate::visitor::Visitor

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

macro_rules! make_visitor_trait {
  ($t:ident, $($ref:tt)*) => {
    /// Visitor object, visiting AST nodes.
    ///
    /// This trait exists in two flavors, depending on whether you want to mutate the AST or not: [`Visitor`] doesn’t
    /// allow for mutation while [`VisitorMut`] does.
    pub trait $t {
      fn visit_translation_unit(&mut self, _: $($ref)* syntax::TranslationUnit) -> Visit {
        Visit::Children
      }

      fn visit_external_declaration(&mut self, _: $($ref)* syntax::ExternalDeclaration) -> Visit {
        Visit::Children
      }

      fn visit_identifier(&mut self, _: $($ref)* syntax::Identifier) -> Visit {
        Visit::Children
      }

      fn visit_arrayed_identifier(&mut self, _: $($ref)* syntax::ArrayedIdentifier) -> Visit {
        Visit::Children
      }

      fn visit_type_name(&mut self, _: $($ref)* syntax::TypeName) -> Visit {
        Visit::Children
      }

      fn visit_block(&mut self, _: $($ref)* syntax::Block) -> Visit {
        Visit::Children
      }

      fn visit_for_init_statement(&mut self, _: $($ref)* syntax::ForInitStatement) -> Visit {
        Visit::Children
      }

      fn visit_for_rest_statement(&mut self, _: $($ref)* syntax::ForRestStatement) -> Visit {
        Visit::Children
      }

      fn visit_function_definition(&mut self, _: $($ref)* syntax::FunctionDefinition) -> Visit {
        Visit::Children
      }

      fn visit_function_parameter_declarator(
        &mut self,
        _: $($ref)* syntax::FunctionParameterDeclarator,
      ) -> Visit {
        Visit::Children
      }

      fn visit_function_prototype(&mut self, _: $($ref)* syntax::FunctionPrototype) -> Visit {
        Visit::Children
      }

      fn visit_init_declarator_list(&mut self, _: $($ref)* syntax::InitDeclaratorList) -> Visit {
        Visit::Children
      }

      fn visit_layout_qualifier(&mut self, _: $($ref)* syntax::LayoutQualifier) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor(&mut self, _: $($ref)* syntax::Preprocessor) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_define(&mut self, _: $($ref)* syntax::PreprocessorDefine) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_elseif(&mut self, _: $($ref)* syntax::PreprocessorElseIf) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_error(&mut self, _: $($ref)* syntax::PreprocessorError) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_extension(&mut self, _: $($ref)* syntax::PreprocessorExtension) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_extension_behavior(
        &mut self,
        _: $($ref)* syntax::PreprocessorExtensionBehavior,
      ) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_extension_name(
        &mut self,
        _: $($ref)* syntax::PreprocessorExtensionName,
      ) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_if(&mut self, _: $($ref)* syntax::PreprocessorIf) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_ifdef(&mut self, _: $($ref)* syntax::PreprocessorIfDef) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_ifndef(&mut self, _: $($ref)* syntax::PreprocessorIfNDef) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_include(&mut self, _: $($ref)* syntax::PreprocessorInclude) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_line(&mut self, _: $($ref)* syntax::PreprocessorLine) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_pragma(&mut self, _: $($ref)* syntax::PreprocessorPragma) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_undef(&mut self, _: $($ref)* syntax::PreprocessorUndef) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_version(&mut self, _: $($ref)* syntax::PreprocessorVersion) -> Visit {
        Visit::Children
      }

      fn visit_preprocessor_version_profile(
        &mut self,
        _: $($ref)* syntax::PreprocessorVersionProfile,
      ) -> Visit {
        Visit::Children
      }

      fn visit_selection_statement(&mut self, _: $($ref)* syntax::SelectionStatement) -> Visit {
        Visit::Children
      }

      fn visit_selection_rest_statement(&mut self, _: $($ref)* syntax::SelectionRestStatement) -> Visit {
        Visit::Children
      }

      fn visit_single_declaration(&mut self, _: $($ref)* syntax::SingleDeclaration) -> Visit {
        Visit::Children
      }

      fn visit_single_declaration_no_type(&mut self, _: $($ref)* syntax::SingleDeclarationNoType) -> Visit {
        Visit::Children
      }

      fn visit_struct_field_specifier(&mut self, _: $($ref)* syntax::StructFieldSpecifier) -> Visit {
        Visit::Children
      }

      fn visit_struct_specifier(&mut self, _: $($ref)* syntax::StructSpecifier) -> Visit {
        Visit::Children
      }

      fn visit_switch_statement(&mut self, _: $($ref)* syntax::SwitchStatement) -> Visit {
        Visit::Children
      }

      fn visit_type_qualifier(&mut self, _: $($ref)* syntax::TypeQualifier) -> Visit {
        Visit::Children
      }

      fn visit_type_specifier(&mut self, _: $($ref)* syntax::TypeSpecifier) -> Visit {
        Visit::Children
      }

      fn visit_full_specified_type(&mut self, _: $($ref)* syntax::FullySpecifiedType) -> Visit {
        Visit::Children
      }

      fn visit_array_specifier(&mut self, _: $($ref)* syntax::ArraySpecifier) -> Visit {
        Visit::Children
      }

      fn visit_array_specifier_dimension(&mut self, _: $($ref)* syntax::ArraySpecifierDimension) -> Visit {
        Visit::Children
      }

      fn visit_assignment_op(&mut self, _: $($ref)* syntax::AssignmentOp) -> Visit {
        Visit::Children
      }

      fn visit_binary_op(&mut self, _: $($ref)* syntax::BinaryOp) -> Visit {
        Visit::Children
      }

      fn visit_case_label(&mut self, _: $($ref)* syntax::CaseLabel) -> Visit {
        Visit::Children
      }

      fn visit_condition(&mut self, _: $($ref)* syntax::Condition) -> Visit {
        Visit::Children
      }

      fn visit_declaration(&mut self, _: $($ref)* syntax::Declaration) -> Visit {
        Visit::Children
      }

      fn visit_expr(&mut self, _: $($ref)* syntax::Expr) -> Visit {
        Visit::Children
      }

      fn visit_fun_identifier(&mut self, _: $($ref)* syntax::FunIdentifier) -> Visit {
        Visit::Children
      }

      fn visit_function_parameter_declaration(
        &mut self,
        _: $($ref)* syntax::FunctionParameterDeclaration,
      ) -> Visit {
        Visit::Children
      }

      fn visit_initializer(&mut self, _: $($ref)* syntax::Initializer) -> Visit {
        Visit::Children
      }

      fn visit_interpolation_qualifier(&mut self, _: $($ref)* syntax::InterpolationQualifier) -> Visit {
        Visit::Children
      }

      fn visit_iteration_statement(&mut self, _: $($ref)* syntax::IterationStatement) -> Visit {
        Visit::Children
      }

      fn visit_jump_statement(&mut self, _: $($ref)* syntax::JumpStatement) -> Visit {
        Visit::Children
      }

      fn visit_layout_qualifier_spec(&mut self, _: $($ref)* syntax::LayoutQualifierSpec) -> Visit {
        Visit::Children
      }

      fn visit_precision_qualifier(&mut self, _: $($ref)* syntax::PrecisionQualifier) -> Visit {
        Visit::Children
      }

      fn visit_statement(&mut self, _: $($ref)* syntax::Statement) -> Visit {
        Visit::Children
      }

      fn visit_compound_statement(&mut self, _: $($ref)* syntax::CompoundStatement) -> Visit {
        Visit::Children
      }

      fn visit_simple_statement(&mut self, _: $($ref)* syntax::SimpleStatement) -> Visit {
        Visit::Children
      }

      fn visit_storage_qualifier(&mut self, _: $($ref)* syntax::StorageQualifier) -> Visit {
        Visit::Children
      }

      fn visit_type_qualifier_spec(&mut self, _: $($ref)* syntax::TypeQualifierSpec) -> Visit {
        Visit::Children
      }

      fn visit_type_specifier_non_array(&mut self, _: $($ref)* syntax::TypeSpecifierNonArray) -> Visit {
        Visit::Children
      }

      fn visit_unary_op(&mut self, _: $($ref)* syntax::UnaryOp) -> Visit {
        Visit::Children
      }

      fn visit_expr_statement(&mut self, _: $($ref)* syntax::ExprStatement) -> Visit {
        Visit::Children
      }
    }
  }
}

macro_rules! make_host_trait {
  ($host_ty:ident, $visitor_ty:ident, $mthd_name:ident, $($ref:tt)*) => {
    /// Part of the AST that can be visited.
    ///
    /// You shouldn’t have to worry about this type nor how to implement it – it’s completely
    /// implemented for you. However, it works in a pretty simple way: any implementor of the host trait can
    /// be used with a visitor.
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
    ///
    /// Note that this trait exists in two versions: an immutable one, [`Host`], which doesn’t allow you to mutate the
    /// AST (but takes a `&`), and a mutable one, [`HostMut`], which allows for AST mutation.
    pub trait $host_ty {
      /// Visit an AST node.
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty;
    }

    impl<T> $host_ty for Option<T>
      where
          T: $host_ty,
      {
        fn $mthd_name<V>($($ref)* self, visitor: &mut V)
        where
            V: $visitor_ty,
        {
          if let Some(x) = self {
            x.$mthd_name(visitor);
          }
        }
      }

    impl<T> $host_ty for Box<T>
      where
          T: $host_ty,
      {
        fn $mthd_name<V>($($ref)* self, visitor: &mut V)
        where
            V: $visitor_ty,
        {
          (**self).$mthd_name(visitor);
        }
      }

    impl $host_ty for syntax::TranslationUnit {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_translation_unit(self);

        if visit == Visit::Children {
          for ed in $($ref)* (self.0).0 {
            ed.$mthd_name(visitor);
          }
        }
      }
    }

    impl $host_ty for syntax::ExternalDeclaration {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_external_declaration(self);

        if visit == Visit::Children {
          match self {
            syntax::ExternalDeclaration::Preprocessor(p) => p.$mthd_name(visitor),
            syntax::ExternalDeclaration::FunctionDefinition(fd) => fd.$mthd_name(visitor),
            syntax::ExternalDeclaration::Declaration(d) => d.$mthd_name(visitor),
          }
        }
      }
    }

    impl $host_ty for syntax::Preprocessor {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_preprocessor(self);

        if visit == Visit::Children {
          match self {
            syntax::Preprocessor::Define(pd) => pd.$mthd_name(visitor),
            syntax::Preprocessor::Else => (),
            syntax::Preprocessor::ElseIf(pei) => pei.$mthd_name(visitor),
            syntax::Preprocessor::EndIf => (),
            syntax::Preprocessor::Error(pe) => pe.$mthd_name(visitor),
            syntax::Preprocessor::If(pi) => pi.$mthd_name(visitor),
            syntax::Preprocessor::IfDef(pid) => pid.$mthd_name(visitor),
            syntax::Preprocessor::IfNDef(pind) => pind.$mthd_name(visitor),
            syntax::Preprocessor::Include(pi) => pi.$mthd_name(visitor),
            syntax::Preprocessor::Line(pl) => pl.$mthd_name(visitor),
            syntax::Preprocessor::Pragma(pp) => pp.$mthd_name(visitor),
            syntax::Preprocessor::Undef(pu) => pu.$mthd_name(visitor),
            syntax::Preprocessor::Version(pv) => pv.$mthd_name(visitor),
            syntax::Preprocessor::Extension(ext) => ext.$mthd_name(visitor),
          }
        }
      }
    }

    impl $host_ty for syntax::PreprocessorDefine {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_preprocessor_define(self);

        if visit == Visit::Children {
          match self {
            syntax::PreprocessorDefine::ObjectLike { ident, .. } => {
              ident.$mthd_name(visitor);
            }

            syntax::PreprocessorDefine::FunctionLike {
              ident,
              args,
              ..
            } => {
              ident.$mthd_name(visitor);

              for arg in args {
                arg.$mthd_name(visitor);
              }
            }
          }
        }
      }
    }

    impl $host_ty for syntax::PreprocessorElseIf {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_preprocessor_elseif(self);
      }
    }

    impl $host_ty for syntax::PreprocessorError {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_preprocessor_error(self);
      }
    }

    impl $host_ty for syntax::PreprocessorIf {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_preprocessor_if(self);
      }
    }

    impl $host_ty for syntax::PreprocessorIfDef {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_preprocessor_ifdef(self);

        if visit == Visit::Children {
          self.ident.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::PreprocessorIfNDef {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_preprocessor_ifndef(self);

        if visit == Visit::Children {
          self.ident.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::PreprocessorInclude {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_preprocessor_include(self);
      }
    }

    impl $host_ty for syntax::PreprocessorLine {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_preprocessor_line(self);
      }
    }

    impl $host_ty for syntax::PreprocessorPragma {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_preprocessor_pragma(self);
      }
    }

    impl $host_ty for syntax::PreprocessorUndef {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_preprocessor_undef(self);

        if visit == Visit::Children {
          self.name.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::PreprocessorVersion {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_preprocessor_version(self);

        if visit == Visit::Children {
          self.profile.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::PreprocessorVersionProfile {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_preprocessor_version_profile(self);
      }
    }

    impl $host_ty for syntax::PreprocessorExtension {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_preprocessor_extension(self);

        if visit == Visit::Children {
          self.name.$mthd_name(visitor);
          self.behavior.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::PreprocessorExtensionBehavior {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_preprocessor_extension_behavior(self);
      }
    }

    impl $host_ty for syntax::PreprocessorExtensionName {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_preprocessor_extension_name(self);
      }
    }

    impl $host_ty for syntax::FunctionPrototype {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_function_prototype(self);

        if visit == Visit::Children {
          self.ty.$mthd_name(visitor);
          self.name.$mthd_name(visitor);

          for param in $($ref)* self.parameters {
            param.$mthd_name(visitor);
          }
        }
      }
    }

    impl $host_ty for syntax::FunctionParameterDeclaration {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_function_parameter_declaration(self);

        if visit == Visit::Children {
          match self {
            syntax::FunctionParameterDeclaration::Named(tq, fpd) => {
              tq.$mthd_name(visitor);
              fpd.$mthd_name(visitor);
            }

            syntax::FunctionParameterDeclaration::Unnamed(tq, ty) => {
              tq.$mthd_name(visitor);
              ty.$mthd_name(visitor);
            }
          }
        }
      }
    }

    impl $host_ty for syntax::FunctionParameterDeclarator {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_function_parameter_declarator(self);

        if visit == Visit::Children {
          self.ty.$mthd_name(visitor);
          self.ident.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::FunctionDefinition {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_function_definition(self);

        if visit == Visit::Children {
          self.prototype.$mthd_name(visitor);
          self.statement.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::Declaration {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_declaration(self);

        if visit == Visit::Children {
          match self {
            syntax::Declaration::FunctionPrototype(fp) => fp.$mthd_name(visitor),

            syntax::Declaration::InitDeclaratorList(idl) => idl.$mthd_name(visitor),

            syntax::Declaration::Precision(pq, ty) => {
              pq.$mthd_name(visitor);
              ty.$mthd_name(visitor);
            }

            syntax::Declaration::Block(block) => block.$mthd_name(visitor),

            syntax::Declaration::Global(tq, idents) => {
              tq.$mthd_name(visitor);

              for ident in idents {
                ident.$mthd_name(visitor);
              }
            }
          }
        }
      }
    }

    impl $host_ty for syntax::Block {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_block(self);

        if visit == Visit::Children {
          self.qualifier.$mthd_name(visitor);
          self.name.$mthd_name(visitor);

          for field in $($ref)* self.fields {
            field.$mthd_name(visitor);
          }

          self.identifier.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::InitDeclaratorList {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_init_declarator_list(self);

        if visit == Visit::Children {
          self.head.$mthd_name(visitor);

          for d in $($ref)* self.tail {
            d.$mthd_name(visitor);
          }
        }
      }
    }

    impl $host_ty for syntax::SingleDeclaration {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_single_declaration(self);

        if visit == Visit::Children {
          self.ty.$mthd_name(visitor);
          self.name.$mthd_name(visitor);
          self.array_specifier.$mthd_name(visitor);
          self.initializer.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::SingleDeclarationNoType {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_single_declaration_no_type(self);

        if visit == Visit::Children {
          self.ident.$mthd_name(visitor);
          self.initializer.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::FullySpecifiedType {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_full_specified_type(self);

        if visit == Visit::Children {
          self.qualifier.$mthd_name(visitor);
          self.ty.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::TypeSpecifier {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_type_specifier(self);

        if visit == Visit::Children {
          self.ty.$mthd_name(visitor);
          self.array_specifier.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::TypeSpecifierNonArray {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_type_specifier_non_array(self);

        if visit == Visit::Children {
          match self {
            syntax::TypeSpecifierNonArray::Struct(ss) => ss.$mthd_name(visitor),
            syntax::TypeSpecifierNonArray::TypeName(tn) => tn.$mthd_name(visitor),
            _ => (),
          }
        }
      }
    }

    impl $host_ty for syntax::TypeQualifier {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_type_qualifier(self);

        if visit == Visit::Children {
          for tqs in $($ref)* self.qualifiers.0 {
            tqs.$mthd_name(visitor);
          }
        }
      }
    }

    impl $host_ty for syntax::TypeQualifierSpec {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_type_qualifier_spec(self);

        if visit == Visit::Children {
          match self {
            syntax::TypeQualifierSpec::Storage(sq) => sq.$mthd_name(visitor),
            syntax::TypeQualifierSpec::Layout(lq) => lq.$mthd_name(visitor),
            syntax::TypeQualifierSpec::Precision(pq) => pq.$mthd_name(visitor),
            syntax::TypeQualifierSpec::Interpolation(iq) => iq.$mthd_name(visitor),
            _ => (),
          }
        }
      }
    }

    impl $host_ty for syntax::StorageQualifier {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_storage_qualifier(self);

        if visit == Visit::Children {
          if let syntax::StorageQualifier::Subroutine(names) = self {
            for name in names {
              name.$mthd_name(visitor);
            }
          }
        }
      }
    }

    impl $host_ty for syntax::LayoutQualifier {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_layout_qualifier(self);

        if visit == Visit::Children {
          for lqs in $($ref)* self.ids.0 {
            lqs.$mthd_name(visitor);
          }
        }
      }
    }

    impl $host_ty for syntax::LayoutQualifierSpec {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_layout_qualifier_spec(self);

        if visit == Visit::Children {
          if let syntax::LayoutQualifierSpec::Identifier(ident, expr) = self {
            ident.$mthd_name(visitor);

            if let Some(e) = expr {
              e.$mthd_name(visitor);
            }
          }
        }
      }
    }

    impl $host_ty for syntax::PrecisionQualifier {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_precision_qualifier(self);
      }
    }

    impl $host_ty for syntax::InterpolationQualifier {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_interpolation_qualifier(self);
      }
    }

    impl $host_ty for syntax::TypeName {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_type_name(self);
      }
    }

    impl $host_ty for syntax::Identifier {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_identifier(self);
      }
    }

    impl $host_ty for syntax::ArrayedIdentifier {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_arrayed_identifier(self);

        if visit == Visit::Children {
          self.ident.$mthd_name(visitor);
          self.array_spec.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::Expr {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_expr(self);

        if visit == Visit::Children {
          match self {
            syntax::Expr::Variable(ident) => ident.$mthd_name(visitor),

            syntax::Expr::Unary(op, e) => {
              op.$mthd_name(visitor);
              e.$mthd_name(visitor);
            }

            syntax::Expr::Binary(op, a, b) => {
              op.$mthd_name(visitor);
              a.$mthd_name(visitor);
              b.$mthd_name(visitor);
            }

            syntax::Expr::Ternary(a, b, c) => {
              a.$mthd_name(visitor);
              b.$mthd_name(visitor);
              c.$mthd_name(visitor);
            }

            syntax::Expr::Assignment(lhs, op, rhs) => {
              lhs.$mthd_name(visitor);
              op.$mthd_name(visitor);
              rhs.$mthd_name(visitor);
            }

            syntax::Expr::Bracket(e, arr_spec) => {
              e.$mthd_name(visitor);
              arr_spec.$mthd_name(visitor);
            }

            syntax::Expr::FunCall(fi, params) => {
              fi.$mthd_name(visitor);

              for param in params {
                param.$mthd_name(visitor);
              }
            }

            syntax::Expr::Dot(e, i) => {
              e.$mthd_name(visitor);
              i.$mthd_name(visitor);
            }

            syntax::Expr::PostInc(e) => e.$mthd_name(visitor),

            syntax::Expr::PostDec(e) => e.$mthd_name(visitor),

            syntax::Expr::Comma(a, b) => {
              a.$mthd_name(visitor);
              b.$mthd_name(visitor);
            }

            _ => (),
          }
        }
      }
    }

    impl $host_ty for syntax::UnaryOp {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_unary_op(self);
      }
    }

    impl $host_ty for syntax::BinaryOp {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_binary_op(self);
      }
    }

    impl $host_ty for syntax::AssignmentOp {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let _ = visitor.visit_assignment_op(self);
      }
    }

    impl $host_ty for syntax::ArraySpecifier {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_array_specifier(self);

        if visit == Visit::Children {
          for dimension in $($ref)* self.dimensions {
            dimension.$mthd_name(visitor);
          }
        }
      }
    }

    impl $host_ty for syntax::ArraySpecifierDimension {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_array_specifier_dimension(self);

        if visit == Visit::Children {
          if let syntax::ArraySpecifierDimension::ExplicitlySized(e) = self {
            e.$mthd_name(visitor);
          }
        }
      }
    }

    impl $host_ty for syntax::FunIdentifier {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_fun_identifier(self);

        if visit == Visit::Children {
          match self {
            syntax::FunIdentifier::Identifier(i) => i.$mthd_name(visitor),
            syntax::FunIdentifier::Expr(e) => e.$mthd_name(visitor),
          }
        }
      }
    }

    impl $host_ty for syntax::StructSpecifier {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_struct_specifier(self);

        if visit == Visit::Children {
          self.name.$mthd_name(visitor);

          for field in $($ref)* self.fields.0 {
            field.$mthd_name(visitor);
          }
        }
      }
    }

    impl $host_ty for syntax::StructFieldSpecifier {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_struct_field_specifier(self);

        if visit == Visit::Children {
          self.qualifier.$mthd_name(visitor);
          self.ty.$mthd_name(visitor);

          for identifier in $($ref)* self.identifiers.0 {
            identifier.$mthd_name(visitor);
          }
        }
      }
    }

    impl $host_ty for syntax::Statement {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_statement(self);

        if visit == Visit::Children {
          match self {
            syntax::Statement::Compound(cs) => cs.$mthd_name(visitor),
            syntax::Statement::Simple(ss) => ss.$mthd_name(visitor),
          }
        }
      }
    }

    impl $host_ty for syntax::SimpleStatement {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_simple_statement(self);

        if visit == Visit::Children {
          match self {
            syntax::SimpleStatement::Declaration(d) => d.$mthd_name(visitor),
            syntax::SimpleStatement::Expression(e) => e.$mthd_name(visitor),
            syntax::SimpleStatement::Selection(s) => s.$mthd_name(visitor),
            syntax::SimpleStatement::Switch(s) => s.$mthd_name(visitor),
            syntax::SimpleStatement::CaseLabel(cl) => cl.$mthd_name(visitor),
            syntax::SimpleStatement::Iteration(i) => i.$mthd_name(visitor),
            syntax::SimpleStatement::Jump(j) => j.$mthd_name(visitor),
          }
        }
      }
    }

    impl $host_ty for syntax::CompoundStatement {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_compound_statement(self);

        if visit == Visit::Children {
          for stmt in $($ref)* self.statement_list {
            stmt.$mthd_name(visitor);
          }
        }
      }
    }

    impl $host_ty for syntax::SelectionStatement {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_selection_statement(self);

        if visit == Visit::Children {
          self.cond.$mthd_name(visitor);
          self.rest.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::SelectionRestStatement {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_selection_rest_statement(self);

        if visit == Visit::Children {
          match self {
            syntax::SelectionRestStatement::Statement(s) => s.$mthd_name(visitor),

            syntax::SelectionRestStatement::Else(a, b) => {
              a.$mthd_name(visitor);
              b.$mthd_name(visitor);
            }
          }
        }
      }
    }

    impl $host_ty for syntax::SwitchStatement {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_switch_statement(self);

        if visit == Visit::Children {
          self.head.$mthd_name(visitor);

          for s in $($ref)* self.body {
            s.$mthd_name(visitor);
          }
        }
      }
    }

    impl $host_ty for syntax::CaseLabel {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_case_label(self);

        if visit == Visit::Children {
          if let syntax::CaseLabel::Case(e) = self {
            e.$mthd_name(visitor);
          }
        }
      }
    }

    impl $host_ty for syntax::IterationStatement {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_iteration_statement(self);

        if visit == Visit::Children {
          match self {
            syntax::IterationStatement::While(c, s) => {
              c.$mthd_name(visitor);
              s.$mthd_name(visitor);
            }

            syntax::IterationStatement::DoWhile(s, e) => {
              s.$mthd_name(visitor);
              e.$mthd_name(visitor);
            }

            syntax::IterationStatement::For(fis, frs, s) => {
              fis.$mthd_name(visitor);
              frs.$mthd_name(visitor);
              s.$mthd_name(visitor);
            }
          }
        }
      }
    }

    impl $host_ty for syntax::ForInitStatement {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_for_init_statement(self);

        if visit == Visit::Children {
          match self {
            syntax::ForInitStatement::Expression(e) => e.$mthd_name(visitor),
            syntax::ForInitStatement::Declaration(d) => d.$mthd_name(visitor),
          }
        }
      }
    }

    impl $host_ty for syntax::ForRestStatement {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_for_rest_statement(self);

        if visit == Visit::Children {
          self.condition.$mthd_name(visitor);
          self.post_expr.$mthd_name(visitor);
        }
      }
    }

    impl $host_ty for syntax::JumpStatement {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_jump_statement(self);

        if visit == Visit::Children {
          if let syntax::JumpStatement::Return(r) = self {
            r.$mthd_name(visitor);
          }
        }
      }
    }

    impl $host_ty for syntax::Condition {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_condition(self);

        if visit == Visit::Children {
          match self {
            syntax::Condition::Expr(e) => e.$mthd_name(visitor),

            syntax::Condition::Assignment(fst, ident, init) => {
              fst.$mthd_name(visitor);
              ident.$mthd_name(visitor);
              init.$mthd_name(visitor);
            }
          }
        }
      }
    }

    impl $host_ty for syntax::Initializer {
      fn $mthd_name<V>($($ref)* self, visitor: &mut V)
      where
          V: $visitor_ty,
      {
        let visit = visitor.visit_initializer(self);

        if visit == Visit::Children {
          match self {
            syntax::Initializer::Simple(e) => e.$mthd_name(visitor),

            syntax::Initializer::List(i) => {
              for i in $($ref)* i.0 {
                i.$mthd_name(visitor);
              }
            }
          }
        }
      }
    }
  }
}

// immutable
make_visitor_trait!(Visitor, &);
make_host_trait!(Host, Visitor, visit, &);

// mutable
make_visitor_trait!(VisitorMut, &mut);
make_host_trait!(HostMut, VisitorMut, visit_mut, &mut);

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

    let compound = syntax::CompoundStatement::from_iter(vec![decl0, decl1, decl2]);

    // our visitor that will count the number of variables it saw
    struct Counter {
      var_nb: usize,
    }

    impl Visitor for Counter {
      // we are only interested in single declaration with a name
      fn visit_single_declaration(&mut self, declaration: &syntax::SingleDeclaration) -> Visit {
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
