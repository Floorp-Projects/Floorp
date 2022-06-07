//! GLSL abstract syntax tree and grammar.
//!
//! This module exports all the grammar syntax that defines GLSL. You’ll be handling ASTs
//! representing your GLSL source.
//!
//! The most external form of a GLSL parsed AST is [`TranslationUnit`] (a shader). Some parts of the
//! tree are *boxed*. This is due to two facts:
//!
//! - Recursion is used, hence we need a way to give our types a static size.
//! - Because of some very deep variants, runtime size would explode if no indirection weren’t
//!   in place.
//!
//! The types are commented so feel free to inspect each of theme. As a starter, you should read
//! the documentation of [`Expr`], [`FunctionDefinition`], [`Statement`] and [`TranslationUnit`].
//!
//! [`Statement`]: crate::syntax::Statement
//! [`TranslationUnit`]: crate::syntax::TranslationUnit
//! [`Expr`]: crate::syntax::Expr
//! [`FunctionDefinition`]: crate::syntax::FunctionDefinition

use std::fmt;
use std::iter::{once, FromIterator};
use std::ops::{Deref, DerefMut};

/// A non-empty [`Vec`]. It has at least one element.
#[derive(Clone, Debug, PartialEq)]
pub struct NonEmpty<T>(pub Vec<T>);

impl<T> NonEmpty<T> {
  /// Construct a non-empty from an iterator.
  ///
  /// # Errors
  ///
  /// `None` if the iterator yields no value.
  pub fn from_non_empty_iter<I>(iter: I) -> Option<Self>
  where
    I: IntoIterator<Item = T>,
  {
    let vec: Vec<_> = iter.into_iter().collect();

    if vec.is_empty() {
      None
    } else {
      Some(NonEmpty(vec))
    }
  }

  /// Move a new item at the end of the non-empty.
  pub fn push(&mut self, item: T) {
    self.0.push(item);
  }

  /// Move out the last element of the non-empty.
  ///
  /// # Errors
  ///
  /// This function returns `None` if called on a non-empty that contains a single element.
  pub fn pop(&mut self) -> Option<T> {
    if self.0.len() == 1 {
      None
    } else {
      self.0.pop()
    }
  }
}

impl<T> IntoIterator for NonEmpty<T> {
  type IntoIter = <Vec<T> as IntoIterator>::IntoIter;
  type Item = T;

  fn into_iter(self) -> Self::IntoIter {
    self.0.into_iter()
  }
}

impl<'a, T> IntoIterator for &'a NonEmpty<T> {
  type IntoIter = <&'a Vec<T> as IntoIterator>::IntoIter;
  type Item = &'a T;

  fn into_iter(self) -> Self::IntoIter {
    self.0.iter()
  }
}

impl<'a, T> IntoIterator for &'a mut NonEmpty<T> {
  type IntoIter = <&'a mut Vec<T> as IntoIterator>::IntoIter;
  type Item = &'a mut T;

  fn into_iter(self) -> Self::IntoIter {
    self.0.iter_mut()
  }
}

impl<T> Extend<T> for NonEmpty<T> {
  fn extend<I>(&mut self, iter: I)
  where
    I: IntoIterator<Item = T>,
  {
    self.0.extend(iter);
  }
}

/// A path literal.
#[derive(Clone, Debug, PartialEq)]
pub enum Path {
  /// Specified with angle brackets.
  Absolute(String),
  /// Specified with double quotes.
  Relative(String),
}

/// Error that might occur when creating a new [`Identifier`].
#[derive(Debug)]
pub enum IdentifierError {
  StartsWithDigit,
  ContainsNonASCIIAlphaNum,
}

impl std::error::Error for IdentifierError {}

impl fmt::Display for IdentifierError {
  fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
    match *self {
      IdentifierError::StartsWithDigit => f.write_str("starts starts with a digit"),

      IdentifierError::ContainsNonASCIIAlphaNum => {
        f.write_str("contains at least one non-alphanumeric ASCII character")
      }
    }
  }
}

/// A generic identifier.
#[derive(Clone, Debug, PartialEq)]
pub struct Identifier(pub String);

impl Identifier {
  /// Create a new [`Identifier`].
  ///
  /// # Errors
  ///
  /// This function will fail if the identifier starts with a digit or contains non-alphanumeric
  /// ASCII characters.
  pub fn new<N>(name: N) -> Result<Self, IdentifierError>
  where
    N: Into<String>,
  {
    let name = name.into();

    if name
      .chars()
      .next()
      .map(|c| c.is_ascii_alphabetic() || c == '_')
      == Some(false)
    {
      // check the first letter is not a digit
      Err(IdentifierError::StartsWithDigit)
    } else if name.contains(|c: char| !(c.is_ascii_alphanumeric() || c == '_')) {
      // check we only have ASCII alphanumeric characters
      Err(IdentifierError::ContainsNonASCIIAlphaNum)
    } else {
      Ok(Identifier(name))
    }
  }

  /// Get the string representation of the identifier.
  pub fn as_str(&self) -> &str {
    &self.0
  }
}

impl<'a> From<&'a str> for Identifier {
  fn from(s: &str) -> Self {
    Identifier(s.to_owned())
  }
}

impl From<String> for Identifier {
  fn from(s: String) -> Self {
    Identifier(s)
  }
}

impl fmt::Display for Identifier {
  fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
    self.0.fmt(f)
  }
}

/// Any type name.
#[derive(Clone, Debug, PartialEq)]
pub struct TypeName(pub String);

impl TypeName {
  /// Create a new [`TypeName`].
  ///
  /// # Errors
  ///
  /// This function will fail if the type name starts with a digit or contains non-alphanumeric
  /// ASCII characters.
  pub fn new<N>(name: N) -> Result<Self, IdentifierError>
  where
    N: Into<String>,
  {
    // build as identifier and unwrap into type name
    let Identifier(tn) = Identifier::new(name)?;
    Ok(TypeName(tn))
  }

  /// Get the string representation of the type name.
  pub fn as_str(&self) -> &str {
    &self.0
  }
}

impl<'a> From<&'a str> for TypeName {
  fn from(s: &str) -> Self {
    TypeName(s.to_owned())
  }
}

impl From<String> for TypeName {
  fn from(s: String) -> Self {
    TypeName(s)
  }
}

impl fmt::Display for TypeName {
  fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
    self.0.fmt(f)
  }
}

/// Type specifier (non-array).
#[derive(Clone, Debug, PartialEq)]
pub enum TypeSpecifierNonArray {
  // transparent types
  Void,
  Bool,
  Int,
  UInt,
  Float,
  Double,
  Vec2,
  Vec3,
  Vec4,
  DVec2,
  DVec3,
  DVec4,
  BVec2,
  BVec3,
  BVec4,
  IVec2,
  IVec3,
  IVec4,
  UVec2,
  UVec3,
  UVec4,
  Mat2,
  Mat3,
  Mat4,
  Mat23,
  Mat24,
  Mat32,
  Mat34,
  Mat42,
  Mat43,
  DMat2,
  DMat3,
  DMat4,
  DMat23,
  DMat24,
  DMat32,
  DMat34,
  DMat42,
  DMat43,
  // floating point opaque types
  Sampler1D,
  Image1D,
  Sampler2D,
  Image2D,
  Sampler3D,
  Image3D,
  SamplerCube,
  ImageCube,
  Sampler2DRect,
  Image2DRect,
  Sampler1DArray,
  Image1DArray,
  Sampler2DArray,
  Image2DArray,
  SamplerBuffer,
  ImageBuffer,
  Sampler2DMS,
  Image2DMS,
  Sampler2DMSArray,
  Image2DMSArray,
  SamplerCubeArray,
  ImageCubeArray,
  Sampler1DShadow,
  Sampler2DShadow,
  Sampler2DRectShadow,
  Sampler1DArrayShadow,
  Sampler2DArrayShadow,
  SamplerCubeShadow,
  SamplerCubeArrayShadow,
  // signed integer opaque types
  ISampler1D,
  IImage1D,
  ISampler2D,
  IImage2D,
  ISampler3D,
  IImage3D,
  ISamplerCube,
  IImageCube,
  ISampler2DRect,
  IImage2DRect,
  ISampler1DArray,
  IImage1DArray,
  ISampler2DArray,
  IImage2DArray,
  ISamplerBuffer,
  IImageBuffer,
  ISampler2DMS,
  IImage2DMS,
  ISampler2DMSArray,
  IImage2DMSArray,
  ISamplerCubeArray,
  IImageCubeArray,
  // unsigned integer opaque types
  AtomicUInt,
  USampler1D,
  UImage1D,
  USampler2D,
  UImage2D,
  USampler3D,
  UImage3D,
  USamplerCube,
  UImageCube,
  USampler2DRect,
  UImage2DRect,
  USampler1DArray,
  UImage1DArray,
  USampler2DArray,
  UImage2DArray,
  USamplerBuffer,
  UImageBuffer,
  USampler2DMS,
  UImage2DMS,
  USampler2DMSArray,
  UImage2DMSArray,
  USamplerCubeArray,
  UImageCubeArray,
  Struct(StructSpecifier),
  TypeName(TypeName),
}

/// Type specifier.
#[derive(Clone, Debug, PartialEq)]
pub struct TypeSpecifier {
  pub ty: TypeSpecifierNonArray,
  pub array_specifier: Option<ArraySpecifier>,
}

impl TypeSpecifier {
  pub fn new(ty: TypeSpecifierNonArray) -> Self {
    TypeSpecifier {
      ty,
      array_specifier: None,
    }
  }
}

impl From<TypeSpecifierNonArray> for TypeSpecifier {
  fn from(ty: TypeSpecifierNonArray) -> Self {
    TypeSpecifier::new(ty)
  }
}

/// Struct specifier. Used to create new, user-defined types.
#[derive(Clone, Debug, PartialEq)]
pub struct StructSpecifier {
  pub name: Option<TypeName>,
  pub fields: NonEmpty<StructFieldSpecifier>,
}

/// Struct field specifier. Used to add fields to struct specifiers.
#[derive(Clone, Debug, PartialEq)]
pub struct StructFieldSpecifier {
  pub qualifier: Option<TypeQualifier>,
  pub ty: TypeSpecifier,
  pub identifiers: NonEmpty<ArrayedIdentifier>, // several identifiers of the same type
}

impl StructFieldSpecifier {
  /// Create a struct field.
  pub fn new<A, T>(identifier: A, ty: T) -> Self
  where
    A: Into<ArrayedIdentifier>,
    T: Into<TypeSpecifier>,
  {
    StructFieldSpecifier {
      qualifier: None,
      ty: ty.into(),
      identifiers: NonEmpty(vec![identifier.into()]),
    }
  }

  /// Create a list of struct fields that all have the same type.
  pub fn new_many<I>(identifiers: I, ty: TypeSpecifier) -> Self
  where
    I: IntoIterator<Item = ArrayedIdentifier>,
  {
    StructFieldSpecifier {
      qualifier: None,
      ty,
      identifiers: NonEmpty(identifiers.into_iter().collect()),
    }
  }
}

/// An identifier with an optional array specifier.
#[derive(Clone, Debug, PartialEq)]
pub struct ArrayedIdentifier {
  pub ident: Identifier,
  pub array_spec: Option<ArraySpecifier>,
}

impl ArrayedIdentifier {
  pub fn new<I, AS>(ident: I, array_spec: AS) -> Self
  where
    I: Into<Identifier>,
    AS: Into<Option<ArraySpecifier>>,
  {
    ArrayedIdentifier {
      ident: ident.into(),
      array_spec: array_spec.into(),
    }
  }
}

impl<'a> From<&'a str> for ArrayedIdentifier {
  fn from(ident: &str) -> Self {
    ArrayedIdentifier {
      ident: Identifier(ident.to_owned()),
      array_spec: None,
    }
  }
}

impl From<Identifier> for ArrayedIdentifier {
  fn from(ident: Identifier) -> Self {
    ArrayedIdentifier {
      ident,
      array_spec: None,
    }
  }
}

/// Type qualifier.
#[derive(Clone, Debug, PartialEq)]
pub struct TypeQualifier {
  pub qualifiers: NonEmpty<TypeQualifierSpec>,
}

/// Type qualifier spec.
#[derive(Clone, Debug, PartialEq)]
pub enum TypeQualifierSpec {
  Storage(StorageQualifier),
  Layout(LayoutQualifier),
  Precision(PrecisionQualifier),
  Interpolation(InterpolationQualifier),
  Invariant,
  Precise,
}

/// Storage qualifier.
#[derive(Clone, Debug, PartialEq)]
pub enum StorageQualifier {
  Const,
  InOut,
  In,
  Out,
  Centroid,
  Patch,
  Sample,
  Uniform,
  Attribute,
  Varying,
  Buffer,
  Shared,
  Coherent,
  Volatile,
  Restrict,
  ReadOnly,
  WriteOnly,
  Subroutine(Vec<TypeName>),
}

/// Layout qualifier.
#[derive(Clone, Debug, PartialEq)]
pub struct LayoutQualifier {
  pub ids: NonEmpty<LayoutQualifierSpec>,
}

/// Layout qualifier spec.
#[derive(Clone, Debug, PartialEq)]
pub enum LayoutQualifierSpec {
  Identifier(Identifier, Option<Box<Expr>>),
  Shared,
}

/// Precision qualifier.
#[derive(Clone, Debug, PartialEq)]
pub enum PrecisionQualifier {
  High,
  Medium,
  Low,
}

/// Interpolation qualifier.
#[derive(Clone, Debug, PartialEq)]
pub enum InterpolationQualifier {
  Smooth,
  Flat,
  NoPerspective,
}

/// Fully specified type.
#[derive(Clone, Debug, PartialEq)]
pub struct FullySpecifiedType {
  pub qualifier: Option<TypeQualifier>,
  pub ty: TypeSpecifier,
}

impl FullySpecifiedType {
  pub fn new(ty: TypeSpecifierNonArray) -> Self {
    FullySpecifiedType {
      qualifier: None,
      ty: TypeSpecifier {
        ty,
        array_specifier: None,
      },
    }
  }
}

impl From<TypeSpecifierNonArray> for FullySpecifiedType {
  fn from(ty: TypeSpecifierNonArray) -> Self {
    FullySpecifiedType::new(ty)
  }
}

/// Dimensionality of an array.
#[derive(Clone, Debug, PartialEq)]
pub struct ArraySpecifier {
  /// List of all the dimensions – possibly unsized or explicitly-sized.
  pub dimensions: NonEmpty<ArraySpecifierDimension>,
}

/// One array specifier dimension.
#[derive(Clone, Debug, PartialEq)]
pub enum ArraySpecifierDimension {
  Unsized,
  ExplicitlySized(Box<Expr>),
}

/// A declaration.
#[derive(Clone, Debug, PartialEq)]
pub enum Declaration {
  FunctionPrototype(FunctionPrototype),
  InitDeclaratorList(InitDeclaratorList),
  Precision(PrecisionQualifier, TypeSpecifier),
  Block(Block),
  Global(TypeQualifier, Vec<Identifier>),
}

/// A general purpose block, containing fields and possibly a list of declared identifiers. Semantic
/// is given with the storage qualifier.
#[derive(Clone, Debug, PartialEq)]
pub struct Block {
  pub qualifier: TypeQualifier,
  pub name: Identifier,
  pub fields: Vec<StructFieldSpecifier>,
  pub identifier: Option<ArrayedIdentifier>,
}

/// Function identifier.
#[derive(Clone, Debug, PartialEq)]
pub enum FunIdentifier {
  Identifier(Identifier),
  Expr(Box<Expr>),
}

impl FunIdentifier {
  pub(crate) fn into_expr(self) -> Option<Expr> {
    match self {
      FunIdentifier::Identifier(..) => None,
      FunIdentifier::Expr(expr) => Some(*expr),
    }
  }
}

/// Function prototype.
#[derive(Clone, Debug, PartialEq)]
pub struct FunctionPrototype {
  pub ty: FullySpecifiedType,
  pub name: Identifier,
  pub parameters: Vec<FunctionParameterDeclaration>,
}

/// Function parameter declaration.
#[derive(Clone, Debug, PartialEq)]
pub enum FunctionParameterDeclaration {
  Named(Option<TypeQualifier>, FunctionParameterDeclarator),
  Unnamed(Option<TypeQualifier>, TypeSpecifier),
}

impl FunctionParameterDeclaration {
  /// Create a named function argument.
  pub fn new_named<I, T>(ident: I, ty: T) -> Self
  where
    I: Into<ArrayedIdentifier>,
    T: Into<TypeSpecifier>,
  {
    let declator = FunctionParameterDeclarator {
      ty: ty.into(),
      ident: ident.into(),
    };

    FunctionParameterDeclaration::Named(None, declator)
  }

  /// Create an unnamed function argument (mostly useful for interfaces / function prototypes).
  pub fn new_unnamed<T>(ty: T) -> Self
  where
    T: Into<TypeSpecifier>,
  {
    FunctionParameterDeclaration::Unnamed(None, ty.into())
  }
}

/// Function parameter declarator.
#[derive(Clone, Debug, PartialEq)]
pub struct FunctionParameterDeclarator {
  pub ty: TypeSpecifier,
  pub ident: ArrayedIdentifier,
}

/// Init declarator list.
#[derive(Clone, Debug, PartialEq)]
pub struct InitDeclaratorList {
  pub head: SingleDeclaration,
  pub tail: Vec<SingleDeclarationNoType>,
}

/// Single declaration.
#[derive(Clone, Debug, PartialEq)]
pub struct SingleDeclaration {
  pub ty: FullySpecifiedType,
  pub name: Option<Identifier>,
  pub array_specifier: Option<ArraySpecifier>,
  pub initializer: Option<Initializer>,
}

/// A single declaration with implicit, already-defined type.
#[derive(Clone, Debug, PartialEq)]
pub struct SingleDeclarationNoType {
  pub ident: ArrayedIdentifier,
  pub initializer: Option<Initializer>,
}

/// Initializer.
#[derive(Clone, Debug, PartialEq)]
pub enum Initializer {
  Simple(Box<Expr>),
  List(NonEmpty<Initializer>),
}

impl From<Expr> for Initializer {
  fn from(e: Expr) -> Self {
    Initializer::Simple(Box::new(e))
  }
}

/// The most general form of an expression. As you can see if you read the variant list, in GLSL, an
/// assignment is an expression. This is a bit silly but think of an assignment as a statement first
/// then an expression which evaluates to what the statement “returns”.
///
/// An expression is either an assignment or a list (comma) of assignments.
#[derive(Clone, Debug, PartialEq)]
pub enum Expr {
  /// A variable expression, using an identifier.
  Variable(Identifier),
  /// Integral constant expression.
  IntConst(i32),
  /// Unsigned integral constant expression.
  UIntConst(u32),
  /// Boolean constant expression.
  BoolConst(bool),
  /// Single precision floating expression.
  FloatConst(f32),
  /// Double precision floating expression.
  DoubleConst(f64),
  /// A unary expression, gathering a single expression and a unary operator.
  Unary(UnaryOp, Box<Expr>),
  /// A binary expression, gathering two expressions and a binary operator.
  Binary(BinaryOp, Box<Expr>, Box<Expr>),
  /// A ternary conditional expression, gathering three expressions.
  Ternary(Box<Expr>, Box<Expr>, Box<Expr>),
  /// An assignment is also an expression. Gathers an expression that defines what to assign to, an
  /// assignment operator and the value to associate with.
  Assignment(Box<Expr>, AssignmentOp, Box<Expr>),
  /// Add an array specifier to an expression.
  Bracket(Box<Expr>, ArraySpecifier),
  /// A functional call. It has a function identifier and a list of expressions (arguments).
  FunCall(FunIdentifier, Vec<Expr>),
  /// An expression associated with a field selection (struct).
  Dot(Box<Expr>, Identifier),
  /// Post-incrementation of an expression.
  PostInc(Box<Expr>),
  /// Post-decrementation of an expression.
  PostDec(Box<Expr>),
  /// An expression that contains several, separated with comma.
  Comma(Box<Expr>, Box<Expr>),
}

impl From<i32> for Expr {
  fn from(x: i32) -> Expr {
    Expr::IntConst(x)
  }
}

impl From<u32> for Expr {
  fn from(x: u32) -> Expr {
    Expr::UIntConst(x)
  }
}

impl From<bool> for Expr {
  fn from(x: bool) -> Expr {
    Expr::BoolConst(x)
  }
}

impl From<f32> for Expr {
  fn from(x: f32) -> Expr {
    Expr::FloatConst(x)
  }
}

impl From<f64> for Expr {
  fn from(x: f64) -> Expr {
    Expr::DoubleConst(x)
  }
}

/// All unary operators that exist in GLSL.
#[derive(Clone, Debug, PartialEq)]
pub enum UnaryOp {
  Inc,
  Dec,
  Add,
  Minus,
  Not,
  Complement,
}

/// All binary operators that exist in GLSL.
#[derive(Clone, Debug, PartialEq)]
pub enum BinaryOp {
  Or,
  Xor,
  And,
  BitOr,
  BitXor,
  BitAnd,
  Equal,
  NonEqual,
  LT,
  GT,
  LTE,
  GTE,
  LShift,
  RShift,
  Add,
  Sub,
  Mult,
  Div,
  Mod,
}

/// All possible operators for assigning expressions.
#[derive(Clone, Debug, PartialEq)]
pub enum AssignmentOp {
  Equal,
  Mult,
  Div,
  Mod,
  Add,
  Sub,
  LShift,
  RShift,
  And,
  Xor,
  Or,
}

/// Starting rule.
#[derive(Clone, Debug, PartialEq)]
pub struct TranslationUnit(pub NonEmpty<ExternalDeclaration>);

/// A shader stage.
pub type ShaderStage = TranslationUnit;

impl TranslationUnit {
  /// Construct a translation unit from an iterator representing a _non-empty_ sequence of
  /// [`ExternalDeclaration`].
  ///
  /// # Errors
  ///
  /// `None` if the iterator yields no value.
  pub fn from_non_empty_iter<I>(iter: I) -> Option<Self>
  where
    I: IntoIterator<Item = ExternalDeclaration>,
  {
    NonEmpty::from_non_empty_iter(iter).map(TranslationUnit)
  }
}

impl Deref for TranslationUnit {
  type Target = NonEmpty<ExternalDeclaration>;

  fn deref(&self) -> &Self::Target {
    &self.0
  }
}

impl DerefMut for TranslationUnit {
  fn deref_mut(&mut self) -> &mut Self::Target {
    &mut self.0
  }
}

impl IntoIterator for TranslationUnit {
  type IntoIter = <NonEmpty<ExternalDeclaration> as IntoIterator>::IntoIter;
  type Item = ExternalDeclaration;

  fn into_iter(self) -> Self::IntoIter {
    self.0.into_iter()
  }
}

impl<'a> IntoIterator for &'a TranslationUnit {
  type IntoIter = <&'a NonEmpty<ExternalDeclaration> as IntoIterator>::IntoIter;
  type Item = &'a ExternalDeclaration;

  fn into_iter(self) -> Self::IntoIter {
    (&self.0).into_iter()
  }
}

impl<'a> IntoIterator for &'a mut TranslationUnit {
  type IntoIter = <&'a mut NonEmpty<ExternalDeclaration> as IntoIterator>::IntoIter;
  type Item = &'a mut ExternalDeclaration;

  fn into_iter(self) -> Self::IntoIter {
    (&mut self.0).into_iter()
  }
}

/// External declaration.
#[derive(Clone, Debug, PartialEq)]
pub enum ExternalDeclaration {
  Preprocessor(Preprocessor),
  FunctionDefinition(FunctionDefinition),
  Declaration(Declaration),
}

impl ExternalDeclaration {
  /// Create a new function.
  pub fn new_fn<T, N, A, S>(ret_ty: T, name: N, args: A, body: S) -> Self
  where
    T: Into<FullySpecifiedType>,
    N: Into<Identifier>,
    A: IntoIterator<Item = FunctionParameterDeclaration>,
    S: IntoIterator<Item = Statement>,
  {
    ExternalDeclaration::FunctionDefinition(FunctionDefinition {
      prototype: FunctionPrototype {
        ty: ret_ty.into(),
        name: name.into(),
        parameters: args.into_iter().collect(),
      },
      statement: CompoundStatement {
        statement_list: body.into_iter().collect(),
      },
    })
  }

  /// Create a new structure.
  ///
  /// # Errors
  ///
  ///   - [`None`] if no fields are provided. GLSL forbids having empty structs.
  pub fn new_struct<N, F>(name: N, fields: F) -> Option<Self>
  where
    N: Into<TypeName>,
    F: IntoIterator<Item = StructFieldSpecifier>,
  {
    let fields: Vec<_> = fields.into_iter().collect();

    if fields.is_empty() {
      None
    } else {
      Some(ExternalDeclaration::Declaration(
        Declaration::InitDeclaratorList(InitDeclaratorList {
          head: SingleDeclaration {
            ty: FullySpecifiedType {
              qualifier: None,
              ty: TypeSpecifier {
                ty: TypeSpecifierNonArray::Struct(StructSpecifier {
                  name: Some(name.into()),
                  fields: NonEmpty(fields.into_iter().collect()),
                }),
                array_specifier: None,
              },
            },
            name: None,
            array_specifier: None,
            initializer: None,
          },
          tail: vec![],
        }),
      ))
    }
  }
}

/// Function definition.
#[derive(Clone, Debug, PartialEq)]
pub struct FunctionDefinition {
  pub prototype: FunctionPrototype,
  pub statement: CompoundStatement,
}

/// Compound statement (with no new scope).
#[derive(Clone, Debug, PartialEq)]
pub struct CompoundStatement {
  pub statement_list: Vec<Statement>,
}

impl FromIterator<Statement> for CompoundStatement {
  fn from_iter<T>(iter: T) -> Self
  where
    T: IntoIterator<Item = Statement>,
  {
    CompoundStatement {
      statement_list: iter.into_iter().collect(),
    }
  }
}

/// Statement.
#[derive(Clone, Debug, PartialEq)]
pub enum Statement {
  Compound(Box<CompoundStatement>),
  Simple(Box<SimpleStatement>),
}

impl Statement {
  /// Create a case-label sequence of nested statements.
  pub fn new_case<C, S>(case: C, statements: S) -> Self
  where
    C: Into<CaseLabel>,
    S: IntoIterator<Item = Statement>,
  {
    let case_stmt = Statement::Simple(Box::new(SimpleStatement::CaseLabel(case.into())));

    Statement::Compound(Box::new(CompoundStatement {
      statement_list: once(case_stmt).chain(statements.into_iter()).collect(),
    }))
  }

  /// Declare a new variable.
  ///
  /// `ty` is the type of the variable, `name` the name of the binding to create,
  /// `array_specifier` an optional argument to make your binding an array and
  /// `initializer`
  pub fn declare_var<T, N, A, I>(ty: T, name: N, array_specifier: A, initializer: I) -> Self
  where
    T: Into<FullySpecifiedType>,
    N: Into<Identifier>,
    A: Into<Option<ArraySpecifier>>,
    I: Into<Option<Initializer>>,
  {
    Statement::Simple(Box::new(SimpleStatement::Declaration(
      Declaration::InitDeclaratorList(InitDeclaratorList {
        head: SingleDeclaration {
          ty: ty.into(),
          name: Some(name.into()),
          array_specifier: array_specifier.into(),
          initializer: initializer.into(),
        },
        tail: Vec::new(),
      }),
    )))
  }
}

/// Simple statement.
#[derive(Clone, Debug, PartialEq)]
pub enum SimpleStatement {
  Declaration(Declaration),
  Expression(ExprStatement),
  Selection(SelectionStatement),
  Switch(SwitchStatement),
  CaseLabel(CaseLabel),
  Iteration(IterationStatement),
  Jump(JumpStatement),
}

impl SimpleStatement {
  /// Create a new expression statement.
  pub fn new_expr<E>(expr: E) -> Self
  where
    E: Into<Expr>,
  {
    SimpleStatement::Expression(Some(expr.into()))
  }

  /// Create a new selection statement (if / else).
  pub fn new_if_else<If, True, False>(ife: If, truee: True, falsee: False) -> Self
  where
    If: Into<Expr>,
    True: Into<Statement>,
    False: Into<Statement>,
  {
    SimpleStatement::Selection(SelectionStatement {
      cond: Box::new(ife.into()),
      rest: SelectionRestStatement::Else(Box::new(truee.into()), Box::new(falsee.into())),
    })
  }

  /// Create a new switch statement.
  ///
  /// A switch statement is always composed of a [`SimpleStatement::Switch`] block, that contains it
  /// all, and has as body a compound list of case statements.
  pub fn new_switch<H, B>(head: H, body: B) -> Self
  where
    H: Into<Expr>,
    B: IntoIterator<Item = Statement>,
  {
    SimpleStatement::Switch(SwitchStatement {
      head: Box::new(head.into()),
      body: body.into_iter().collect(),
    })
  }

  /// Create a new while statement.
  pub fn new_while<C, S>(cond: C, body: S) -> Self
  where
    C: Into<Condition>,
    S: Into<Statement>,
  {
    SimpleStatement::Iteration(IterationStatement::While(
      cond.into(),
      Box::new(body.into()),
    ))
  }

  /// Create a new do-while statement.
  pub fn new_do_while<C, S>(body: S, cond: C) -> Self
  where
    S: Into<Statement>,
    C: Into<Expr>,
  {
    SimpleStatement::Iteration(IterationStatement::DoWhile(
      Box::new(body.into()),
      Box::new(cond.into()),
    ))
  }
}

/// Expression statement.
pub type ExprStatement = Option<Expr>;

/// Selection statement.
#[derive(Clone, Debug, PartialEq)]
pub struct SelectionStatement {
  pub cond: Box<Expr>,
  pub rest: SelectionRestStatement,
}

/// Condition.
#[derive(Clone, Debug, PartialEq)]
pub enum Condition {
  Expr(Box<Expr>),
  Assignment(FullySpecifiedType, Identifier, Initializer),
}

impl From<Expr> for Condition {
  fn from(expr: Expr) -> Self {
    Condition::Expr(Box::new(expr))
  }
}

/// Selection rest statement.
#[derive(Clone, Debug, PartialEq)]
pub enum SelectionRestStatement {
  /// Body of the if.
  Statement(Box<Statement>),
  /// The first argument is the body of the if, the rest is the next statement.
  Else(Box<Statement>, Box<Statement>),
}

/// Switch statement.
#[derive(Clone, Debug, PartialEq)]
pub struct SwitchStatement {
  pub head: Box<Expr>,
  pub body: Vec<Statement>,
}

/// Case label statement.
#[derive(Clone, Debug, PartialEq)]
pub enum CaseLabel {
  Case(Box<Expr>),
  Def,
}

/// Iteration statement.
#[derive(Clone, Debug, PartialEq)]
pub enum IterationStatement {
  While(Condition, Box<Statement>),
  DoWhile(Box<Statement>, Box<Expr>),
  For(ForInitStatement, ForRestStatement, Box<Statement>),
}

/// For init statement.
#[derive(Clone, Debug, PartialEq)]
pub enum ForInitStatement {
  Expression(Option<Expr>),
  Declaration(Box<Declaration>),
}

/// For init statement.
#[derive(Clone, Debug, PartialEq)]
pub struct ForRestStatement {
  pub condition: Option<Condition>,
  pub post_expr: Option<Box<Expr>>,
}

/// Jump statement.
#[derive(Clone, Debug, PartialEq)]
pub enum JumpStatement {
  Continue,
  Break,
  Return(Option<Box<Expr>>),
  Discard,
}

/// Some basic preprocessor directives.
///
/// As it’s important to carry them around the AST because they cannot be substituted in a normal
/// preprocessor (they’re used by GPU’s compilers), those preprocessor directives are available for
/// inspection.
#[derive(Clone, Debug, PartialEq)]
pub enum Preprocessor {
  Define(PreprocessorDefine),
  Else,
  ElseIf(PreprocessorElseIf),
  EndIf,
  Error(PreprocessorError),
  If(PreprocessorIf),
  IfDef(PreprocessorIfDef),
  IfNDef(PreprocessorIfNDef),
  Include(PreprocessorInclude),
  Line(PreprocessorLine),
  Pragma(PreprocessorPragma),
  Undef(PreprocessorUndef),
  Version(PreprocessorVersion),
  Extension(PreprocessorExtension),
}

/// A #define preprocessor directive.
///
/// Allows any expression but only Integer and Float literals make sense
#[derive(Clone, Debug, PartialEq)]
pub enum PreprocessorDefine {
  ObjectLike {
    ident: Identifier,
    value: String,
  },

  FunctionLike {
    ident: Identifier,
    args: Vec<Identifier>,
    value: String,
  },
}

/// An #else preprocessor directive.
#[derive(Clone, Debug, PartialEq)]
pub struct PreprocessorElseIf {
  pub condition: String,
}

/// An #error preprocessor directive.
#[derive(Clone, Debug, PartialEq)]
pub struct PreprocessorError {
  pub message: String,
}

/// An #if preprocessor directive.
#[derive(Clone, Debug, PartialEq)]
pub struct PreprocessorIf {
  pub condition: String,
}

/// An #ifdef preprocessor directive.
#[derive(Clone, Debug, PartialEq)]
pub struct PreprocessorIfDef {
  pub ident: Identifier,
}

/// A #ifndef preprocessor directive.
#[derive(Clone, Debug, PartialEq)]
pub struct PreprocessorIfNDef {
  pub ident: Identifier,
}

/// An #include name annotation.
#[derive(Clone, Debug, PartialEq)]
pub struct PreprocessorInclude {
  pub path: Path,
}

/// A #line preprocessor directive.
#[derive(Clone, Debug, PartialEq)]
pub struct PreprocessorLine {
  pub line: u32,
  pub source_string_number: Option<u32>,
}

/// A #pragma preprocessor directive.
/// Holds compiler-specific command.
#[derive(Clone, Debug, PartialEq)]
pub struct PreprocessorPragma {
  pub command: String,
}

/// A #undef preprocessor directive.
#[derive(Clone, Debug, PartialEq)]
pub struct PreprocessorUndef {
  pub name: Identifier,
}

/// A #version preprocessor directive.
#[derive(Clone, Debug, PartialEq)]
pub struct PreprocessorVersion {
  pub version: u16,
  pub profile: Option<PreprocessorVersionProfile>,
}

/// A #version profile annotation.
#[derive(Clone, Debug, PartialEq)]
pub enum PreprocessorVersionProfile {
  Core,
  Compatibility,
  ES,
}

/// An #extension preprocessor directive.
#[derive(Clone, Debug, PartialEq)]
pub struct PreprocessorExtension {
  pub name: PreprocessorExtensionName,
  pub behavior: Option<PreprocessorExtensionBehavior>,
}

/// An #extension name annotation.
#[derive(Clone, Debug, PartialEq)]
pub enum PreprocessorExtensionName {
  /// All extensions you could ever imagine in your whole lifetime (how crazy is that!).
  All,
  /// A specific extension.
  Specific(String),
}

/// An #extension behavior annotation.
#[derive(Clone, Debug, PartialEq)]
pub enum PreprocessorExtensionBehavior {
  Require,
  Enable,
  Warn,
  Disable,
}

#[cfg(test)]
mod tests {
  use super::*;

  #[test]
  fn create_new_identifier() {
    assert!(Identifier::new("foo_bar").is_ok());
    assert!(Identifier::new("3foo_bar").is_err());
    assert!(Identifier::new("FooBar").is_ok());
    assert!(Identifier::new("_FooBar").is_ok());
    assert!(Identifier::new("foo3").is_ok());
    assert!(Identifier::new("foo3_").is_ok());
    assert!(Identifier::new("fδo3_").is_err());
  }

  #[test]
  fn create_new_type_name() {
    assert!(TypeName::new("foo_bar").is_ok());
    assert!(TypeName::new("FooBar").is_ok());
    assert!(TypeName::new("foo3").is_ok());
    assert!(TypeName::new("foo3_").is_ok());

    assert!(TypeName::new("_FooBar").is_ok());
    assert!(TypeName::new("3foo_bar").is_err());
    assert!(TypeName::new("fδo3_").is_err());
  }

  // bool predicate(float x) {
  // }
  #[test]
  fn declare_new_fn() {
    let _ = ExternalDeclaration::new_fn(
      TypeSpecifierNonArray::Bool,
      "predicate",
      vec![FunctionParameterDeclaration::new_named(
        "x",
        TypeSpecifierNonArray::Float,
      )],
      vec![],
    );
  }

  // struct Point2D {
  //   float x;
  //   float y;
  // };
  #[test]
  fn declare_struct() {
    let point = ExternalDeclaration::new_struct(
      "Point2D",
      vec![
        StructFieldSpecifier::new("x", TypeSpecifierNonArray::Double),
        StructFieldSpecifier::new("y", TypeSpecifierNonArray::Double),
      ],
    );

    assert!(point.is_some());
  }

  // struct Point2D {};
  #[test]
  fn declare_bad_struct() {
    let point = ExternalDeclaration::new_struct("Point2D", vec![]);
    assert!(point.is_none());
  }

  #[test]
  fn initializer_from_expr() {
    let _: Initializer = Expr::from(false).into();
  }
}
