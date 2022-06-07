//! A GLSL450/GLSL460 transpiler that takes a syntax tree and writes it as a plain raw GLSL
//! [`String`].
//!
//! # Foreword
//!
//! This module exports several functions that just transform a part of a syntax tree into its raw
//! GLSL [`String`] representation.
//!
//! > Important note: this module – and actually, any [`transpiler`] module – is not responsible in
//! > optimizing the syntax tree nor semantically check its validity. This is done in other stages
//! > of the compilation process.
//!
//! In order to achieve that purpose, you could:
//!
//! - For each elements in the AST, return a [`String`] or [`Cow<str>`].
//! - Insert the string representation via a formatter.
//!
//! The second solution is better because it lets the user handle the memory the way they want:
//! they might just use a dynamic buffer that implements [`Write`] or simply pass a `&mut`
//! [`String`]. It’s up to you.
//!
//! # How to use this module
//!
//! First, head over to the [`syntax`] module. That module defines the AST items defined by GLSL. This
//! very module provides you with functions like `show_*` taking the AST item and writing it to a
//! [`Write`] object. You’re likely to be interested in [`show_translation_unit`] to start with.
//!
//! [`Cow<str>`]: std::borrow::Cow
//! [`Write`]: std::fmt::Write
//! [`show_translation_unit`]: crate::transpiler::glsl::show_translation_unit
//! [`syntax`]: crate::syntax
//! [`transpiler`]: crate::transpiler

use std::fmt::Write;

use crate::syntax;

// Precedence information for transpiling parentheses properly
trait HasPrecedence {
  fn precedence(&self) -> u32;
}

impl HasPrecedence for syntax::Expr {
  fn precedence(&self) -> u32 {
    match self {
      // 0 isn't a valid precedence, but we use this to represent atomic expressions
      Self::Variable(_)
      | Self::IntConst(_)
      | Self::UIntConst(_)
      | Self::BoolConst(_)
      | Self::FloatConst(_)
      | Self::DoubleConst(_) => 0,
      // Precedence operator expression is precedence of operator
      Self::Unary(op, _) => op.precedence(),
      Self::Binary(op, _, _) => op.precedence(),
      Self::Ternary(_, _, _) => 15,
      Self::Assignment(_, op, _) => op.precedence(),
      Self::Bracket(_, _)
      | Self::FunCall(_, _)
      | Self::Dot(_, _)
      | Self::PostInc(_)
      | Self::PostDec(_) => 2,
      Self::Comma(_, _) => 17,
    }
  }
}

impl HasPrecedence for syntax::UnaryOp {
  fn precedence(&self) -> u32 {
    3
  }
}

impl HasPrecedence for syntax::BinaryOp {
  fn precedence(&self) -> u32 {
    match self {
      Self::Mult | Self::Div | Self::Mod => 4,
      Self::Add | Self::Sub => 5,
      Self::LShift | Self::RShift => 6,
      Self::LT | Self::GT | Self::LTE | Self::GTE => 7,
      Self::Equal | Self::NonEqual => 8,
      Self::BitAnd => 9,
      Self::BitXor => 10,
      Self::BitOr => 11,
      Self::And => 12,
      Self::Xor => 13,
      Self::Or => 14,
    }
  }
}

impl HasPrecedence for syntax::AssignmentOp {
  fn precedence(&self) -> u32 {
    16
  }
}

pub fn show_identifier<F>(f: &mut F, i: &syntax::Identifier)
where
  F: Write,
{
  let _ = f.write_str(&i.0);
}

pub fn show_type_name<F>(f: &mut F, t: &syntax::TypeName)
where
  F: Write,
{
  let _ = f.write_str(&t.0);
}

pub fn show_type_specifier_non_array<F>(f: &mut F, t: &syntax::TypeSpecifierNonArray)
where
  F: Write,
{
  match *t {
    syntax::TypeSpecifierNonArray::Void => {
      let _ = f.write_str("void");
    }
    syntax::TypeSpecifierNonArray::Bool => {
      let _ = f.write_str("bool");
    }
    syntax::TypeSpecifierNonArray::Int => {
      let _ = f.write_str("int");
    }
    syntax::TypeSpecifierNonArray::UInt => {
      let _ = f.write_str("uint");
    }
    syntax::TypeSpecifierNonArray::Float => {
      let _ = f.write_str("float");
    }
    syntax::TypeSpecifierNonArray::Double => {
      let _ = f.write_str("double");
    }
    syntax::TypeSpecifierNonArray::Vec2 => {
      let _ = f.write_str("vec2");
    }
    syntax::TypeSpecifierNonArray::Vec3 => {
      let _ = f.write_str("vec3");
    }
    syntax::TypeSpecifierNonArray::Vec4 => {
      let _ = f.write_str("vec4");
    }
    syntax::TypeSpecifierNonArray::DVec2 => {
      let _ = f.write_str("dvec2");
    }
    syntax::TypeSpecifierNonArray::DVec3 => {
      let _ = f.write_str("dvec3");
    }
    syntax::TypeSpecifierNonArray::DVec4 => {
      let _ = f.write_str("dvec4");
    }
    syntax::TypeSpecifierNonArray::BVec2 => {
      let _ = f.write_str("bvec2");
    }
    syntax::TypeSpecifierNonArray::BVec3 => {
      let _ = f.write_str("bvec3");
    }
    syntax::TypeSpecifierNonArray::BVec4 => {
      let _ = f.write_str("bvec4");
    }
    syntax::TypeSpecifierNonArray::IVec2 => {
      let _ = f.write_str("ivec2");
    }
    syntax::TypeSpecifierNonArray::IVec3 => {
      let _ = f.write_str("ivec3");
    }
    syntax::TypeSpecifierNonArray::IVec4 => {
      let _ = f.write_str("ivec4");
    }
    syntax::TypeSpecifierNonArray::UVec2 => {
      let _ = f.write_str("uvec2");
    }
    syntax::TypeSpecifierNonArray::UVec3 => {
      let _ = f.write_str("uvec3");
    }
    syntax::TypeSpecifierNonArray::UVec4 => {
      let _ = f.write_str("uvec4");
    }
    syntax::TypeSpecifierNonArray::Mat2 => {
      let _ = f.write_str("mat2");
    }
    syntax::TypeSpecifierNonArray::Mat3 => {
      let _ = f.write_str("mat3");
    }
    syntax::TypeSpecifierNonArray::Mat4 => {
      let _ = f.write_str("mat4");
    }
    syntax::TypeSpecifierNonArray::Mat23 => {
      let _ = f.write_str("mat2x3");
    }
    syntax::TypeSpecifierNonArray::Mat24 => {
      let _ = f.write_str("mat2x4");
    }
    syntax::TypeSpecifierNonArray::Mat32 => {
      let _ = f.write_str("mat3x2");
    }
    syntax::TypeSpecifierNonArray::Mat34 => {
      let _ = f.write_str("mat3x4");
    }
    syntax::TypeSpecifierNonArray::Mat42 => {
      let _ = f.write_str("mat4x2");
    }
    syntax::TypeSpecifierNonArray::Mat43 => {
      let _ = f.write_str("mat4x3");
    }
    syntax::TypeSpecifierNonArray::DMat2 => {
      let _ = f.write_str("dmat2");
    }
    syntax::TypeSpecifierNonArray::DMat3 => {
      let _ = f.write_str("dmat3");
    }
    syntax::TypeSpecifierNonArray::DMat4 => {
      let _ = f.write_str("dmat4");
    }
    syntax::TypeSpecifierNonArray::DMat23 => {
      let _ = f.write_str("dmat2x3");
    }
    syntax::TypeSpecifierNonArray::DMat24 => {
      let _ = f.write_str("dmat2x4");
    }
    syntax::TypeSpecifierNonArray::DMat32 => {
      let _ = f.write_str("dmat3x2");
    }
    syntax::TypeSpecifierNonArray::DMat34 => {
      let _ = f.write_str("dmat3x4");
    }
    syntax::TypeSpecifierNonArray::DMat42 => {
      let _ = f.write_str("dmat4x2");
    }
    syntax::TypeSpecifierNonArray::DMat43 => {
      let _ = f.write_str("dmat4x3");
    }
    syntax::TypeSpecifierNonArray::Sampler1D => {
      let _ = f.write_str("sampler1D");
    }
    syntax::TypeSpecifierNonArray::Image1D => {
      let _ = f.write_str("image1D");
    }
    syntax::TypeSpecifierNonArray::Sampler2D => {
      let _ = f.write_str("sampler2D");
    }
    syntax::TypeSpecifierNonArray::Image2D => {
      let _ = f.write_str("image2D");
    }
    syntax::TypeSpecifierNonArray::Sampler3D => {
      let _ = f.write_str("sampler3D");
    }
    syntax::TypeSpecifierNonArray::Image3D => {
      let _ = f.write_str("image3D");
    }
    syntax::TypeSpecifierNonArray::SamplerCube => {
      let _ = f.write_str("samplerCube");
    }
    syntax::TypeSpecifierNonArray::ImageCube => {
      let _ = f.write_str("imageCube");
    }
    syntax::TypeSpecifierNonArray::Sampler2DRect => {
      let _ = f.write_str("sampler2DRect");
    }
    syntax::TypeSpecifierNonArray::Image2DRect => {
      let _ = f.write_str("image2DRect");
    }
    syntax::TypeSpecifierNonArray::Sampler1DArray => {
      let _ = f.write_str("sampler1DArray");
    }
    syntax::TypeSpecifierNonArray::Image1DArray => {
      let _ = f.write_str("image1DArray");
    }
    syntax::TypeSpecifierNonArray::Sampler2DArray => {
      let _ = f.write_str("sampler2DArray");
    }
    syntax::TypeSpecifierNonArray::Image2DArray => {
      let _ = f.write_str("image2DArray");
    }
    syntax::TypeSpecifierNonArray::SamplerBuffer => {
      let _ = f.write_str("samplerBuffer");
    }
    syntax::TypeSpecifierNonArray::ImageBuffer => {
      let _ = f.write_str("imageBuffer");
    }
    syntax::TypeSpecifierNonArray::Sampler2DMS => {
      let _ = f.write_str("sampler2DMS");
    }
    syntax::TypeSpecifierNonArray::Image2DMS => {
      let _ = f.write_str("image2DMS");
    }
    syntax::TypeSpecifierNonArray::Sampler2DMSArray => {
      let _ = f.write_str("sampler2DMSArray");
    }
    syntax::TypeSpecifierNonArray::Image2DMSArray => {
      let _ = f.write_str("image2DMSArray");
    }
    syntax::TypeSpecifierNonArray::SamplerCubeArray => {
      let _ = f.write_str("samplerCubeArray");
    }
    syntax::TypeSpecifierNonArray::ImageCubeArray => {
      let _ = f.write_str("imageCubeArray");
    }
    syntax::TypeSpecifierNonArray::Sampler1DShadow => {
      let _ = f.write_str("sampler1DShadow");
    }
    syntax::TypeSpecifierNonArray::Sampler2DShadow => {
      let _ = f.write_str("sampler2DShadow");
    }
    syntax::TypeSpecifierNonArray::Sampler2DRectShadow => {
      let _ = f.write_str("sampler2DRectShadow");
    }
    syntax::TypeSpecifierNonArray::Sampler1DArrayShadow => {
      let _ = f.write_str("sampler1DArrayShadow");
    }
    syntax::TypeSpecifierNonArray::Sampler2DArrayShadow => {
      let _ = f.write_str("sampler2DArrayShadow");
    }
    syntax::TypeSpecifierNonArray::SamplerCubeShadow => {
      let _ = f.write_str("samplerCubeShadow");
    }
    syntax::TypeSpecifierNonArray::SamplerCubeArrayShadow => {
      let _ = f.write_str("samplerCubeArrayShadow");
    }
    syntax::TypeSpecifierNonArray::ISampler1D => {
      let _ = f.write_str("isampler1D");
    }
    syntax::TypeSpecifierNonArray::IImage1D => {
      let _ = f.write_str("iimage1D");
    }
    syntax::TypeSpecifierNonArray::ISampler2D => {
      let _ = f.write_str("isampler2D");
    }
    syntax::TypeSpecifierNonArray::IImage2D => {
      let _ = f.write_str("iimage2D");
    }
    syntax::TypeSpecifierNonArray::ISampler3D => {
      let _ = f.write_str("isampler3D");
    }
    syntax::TypeSpecifierNonArray::IImage3D => {
      let _ = f.write_str("iimage3D");
    }
    syntax::TypeSpecifierNonArray::ISamplerCube => {
      let _ = f.write_str("isamplerCube");
    }
    syntax::TypeSpecifierNonArray::IImageCube => {
      let _ = f.write_str("iimageCube");
    }
    syntax::TypeSpecifierNonArray::ISampler2DRect => {
      let _ = f.write_str("isampler2DRect");
    }
    syntax::TypeSpecifierNonArray::IImage2DRect => {
      let _ = f.write_str("iimage2DRect");
    }
    syntax::TypeSpecifierNonArray::ISampler1DArray => {
      let _ = f.write_str("isampler1DArray");
    }
    syntax::TypeSpecifierNonArray::IImage1DArray => {
      let _ = f.write_str("iimage1DArray");
    }
    syntax::TypeSpecifierNonArray::ISampler2DArray => {
      let _ = f.write_str("isampler2DArray");
    }
    syntax::TypeSpecifierNonArray::IImage2DArray => {
      let _ = f.write_str("iimage2DArray");
    }
    syntax::TypeSpecifierNonArray::ISamplerBuffer => {
      let _ = f.write_str("isamplerBuffer");
    }
    syntax::TypeSpecifierNonArray::IImageBuffer => {
      let _ = f.write_str("iimageBuffer");
    }
    syntax::TypeSpecifierNonArray::ISampler2DMS => {
      let _ = f.write_str("isampler2MS");
    }
    syntax::TypeSpecifierNonArray::IImage2DMS => {
      let _ = f.write_str("iimage2DMS");
    }
    syntax::TypeSpecifierNonArray::ISampler2DMSArray => {
      let _ = f.write_str("isampler2DMSArray");
    }
    syntax::TypeSpecifierNonArray::IImage2DMSArray => {
      let _ = f.write_str("iimage2DMSArray");
    }
    syntax::TypeSpecifierNonArray::ISamplerCubeArray => {
      let _ = f.write_str("isamplerCubeArray");
    }
    syntax::TypeSpecifierNonArray::IImageCubeArray => {
      let _ = f.write_str("iimageCubeArray");
    }
    syntax::TypeSpecifierNonArray::AtomicUInt => {
      let _ = f.write_str("atomic_uint");
    }
    syntax::TypeSpecifierNonArray::USampler1D => {
      let _ = f.write_str("usampler1D");
    }
    syntax::TypeSpecifierNonArray::UImage1D => {
      let _ = f.write_str("uimage1D");
    }
    syntax::TypeSpecifierNonArray::USampler2D => {
      let _ = f.write_str("usampler2D");
    }
    syntax::TypeSpecifierNonArray::UImage2D => {
      let _ = f.write_str("uimage2D");
    }
    syntax::TypeSpecifierNonArray::USampler3D => {
      let _ = f.write_str("usampler3D");
    }
    syntax::TypeSpecifierNonArray::UImage3D => {
      let _ = f.write_str("uimage3D");
    }
    syntax::TypeSpecifierNonArray::USamplerCube => {
      let _ = f.write_str("usamplerCube");
    }
    syntax::TypeSpecifierNonArray::UImageCube => {
      let _ = f.write_str("uimageCube");
    }
    syntax::TypeSpecifierNonArray::USampler2DRect => {
      let _ = f.write_str("usampler2DRect");
    }
    syntax::TypeSpecifierNonArray::UImage2DRect => {
      let _ = f.write_str("uimage2DRect");
    }
    syntax::TypeSpecifierNonArray::USampler1DArray => {
      let _ = f.write_str("usampler1DArray");
    }
    syntax::TypeSpecifierNonArray::UImage1DArray => {
      let _ = f.write_str("uimage1DArray");
    }
    syntax::TypeSpecifierNonArray::USampler2DArray => {
      let _ = f.write_str("usampler2DArray");
    }
    syntax::TypeSpecifierNonArray::UImage2DArray => {
      let _ = f.write_str("uimage2DArray");
    }
    syntax::TypeSpecifierNonArray::USamplerBuffer => {
      let _ = f.write_str("usamplerBuffer");
    }
    syntax::TypeSpecifierNonArray::UImageBuffer => {
      let _ = f.write_str("uimageBuffer");
    }
    syntax::TypeSpecifierNonArray::USampler2DMS => {
      let _ = f.write_str("usampler2DMS");
    }
    syntax::TypeSpecifierNonArray::UImage2DMS => {
      let _ = f.write_str("uimage2DMS");
    }
    syntax::TypeSpecifierNonArray::USampler2DMSArray => {
      let _ = f.write_str("usamplerDMSArray");
    }
    syntax::TypeSpecifierNonArray::UImage2DMSArray => {
      let _ = f.write_str("uimage2DMSArray");
    }
    syntax::TypeSpecifierNonArray::USamplerCubeArray => {
      let _ = f.write_str("usamplerCubeArray");
    }
    syntax::TypeSpecifierNonArray::UImageCubeArray => {
      let _ = f.write_str("uimageCubeArray");
    }
    syntax::TypeSpecifierNonArray::Struct(ref s) => show_struct_non_declaration(f, s),
    syntax::TypeSpecifierNonArray::TypeName(ref tn) => show_type_name(f, tn),
  }
}

pub fn show_type_specifier<F>(f: &mut F, t: &syntax::TypeSpecifier)
where
  F: Write,
{
  show_type_specifier_non_array(f, &t.ty);

  if let Some(ref arr_spec) = t.array_specifier {
    show_array_spec(f, arr_spec);
  }
}

pub fn show_fully_specified_type<F>(f: &mut F, t: &syntax::FullySpecifiedType)
where
  F: Write,
{
  if let Some(ref qual) = t.qualifier {
    show_type_qualifier(f, &qual);
    let _ = f.write_str(" ");
  }

  show_type_specifier(f, &t.ty);
}

pub fn show_struct_non_declaration<F>(f: &mut F, s: &syntax::StructSpecifier)
where
  F: Write,
{
  let _ = f.write_str("struct ");

  if let Some(ref name) = s.name {
    let _ = write!(f, "{} ", name);
  }

  let _ = f.write_str("{\n");

  for field in &s.fields.0 {
    show_struct_field(f, field);
  }

  let _ = f.write_str("}");
}

pub fn show_struct<F>(f: &mut F, s: &syntax::StructSpecifier)
where
  F: Write,
{
  show_struct_non_declaration(f, s);
  let _ = f.write_str(";\n");
}

pub fn show_struct_field<F>(f: &mut F, field: &syntax::StructFieldSpecifier)
where
  F: Write,
{
  if let Some(ref qual) = field.qualifier {
    show_type_qualifier(f, &qual);
    let _ = f.write_str(" ");
  }

  show_type_specifier(f, &field.ty);
  let _ = f.write_str(" ");

  // there’s at least one identifier
  let mut identifiers = field.identifiers.0.iter();
  let identifier = identifiers.next().unwrap();

  show_arrayed_identifier(f, identifier);

  // write the rest of the identifiers
  for identifier in identifiers {
    let _ = f.write_str(", ");
    show_arrayed_identifier(f, identifier);
  }

  let _ = f.write_str(";\n");
}

pub fn show_array_spec<F>(f: &mut F, a: &syntax::ArraySpecifier)
where
  F: Write,
{
  for dimension in &a.dimensions {
    match *dimension {
      syntax::ArraySpecifierDimension::Unsized => {
        let _ = f.write_str("[]");
      }
      syntax::ArraySpecifierDimension::ExplicitlySized(ref e) => {
        let _ = f.write_str("[");
        show_expr(f, &e);
        let _ = f.write_str("]");
      }
    }
  }
}

pub fn show_arrayed_identifier<F>(f: &mut F, a: &syntax::ArrayedIdentifier)
where
  F: Write,
{
  let _ = write!(f, "{}", a.ident);

  if let Some(ref arr_spec) = a.array_spec {
    show_array_spec(f, arr_spec);
  }
}

pub fn show_type_qualifier<F>(f: &mut F, q: &syntax::TypeQualifier)
where
  F: Write,
{
  let mut qualifiers = q.qualifiers.0.iter();
  let first = qualifiers.next().unwrap();

  show_type_qualifier_spec(f, first);

  for qual_spec in qualifiers {
    let _ = f.write_str(" ");
    show_type_qualifier_spec(f, qual_spec)
  }
}

pub fn show_type_qualifier_spec<F>(f: &mut F, q: &syntax::TypeQualifierSpec)
where
  F: Write,
{
  match *q {
    syntax::TypeQualifierSpec::Storage(ref s) => show_storage_qualifier(f, &s),
    syntax::TypeQualifierSpec::Layout(ref l) => show_layout_qualifier(f, &l),
    syntax::TypeQualifierSpec::Precision(ref p) => show_precision_qualifier(f, &p),
    syntax::TypeQualifierSpec::Interpolation(ref i) => show_interpolation_qualifier(f, &i),
    syntax::TypeQualifierSpec::Invariant => {
      let _ = f.write_str("invariant");
    }
    syntax::TypeQualifierSpec::Precise => {
      let _ = f.write_str("precise");
    }
  }
}

pub fn show_storage_qualifier<F>(f: &mut F, q: &syntax::StorageQualifier)
where
  F: Write,
{
  match *q {
    syntax::StorageQualifier::Const => {
      let _ = f.write_str("const");
    }
    syntax::StorageQualifier::InOut => {
      let _ = f.write_str("inout");
    }
    syntax::StorageQualifier::In => {
      let _ = f.write_str("in");
    }
    syntax::StorageQualifier::Out => {
      let _ = f.write_str("out");
    }
    syntax::StorageQualifier::Centroid => {
      let _ = f.write_str("centroid");
    }
    syntax::StorageQualifier::Patch => {
      let _ = f.write_str("patch");
    }
    syntax::StorageQualifier::Sample => {
      let _ = f.write_str("sample");
    }
    syntax::StorageQualifier::Uniform => {
      let _ = f.write_str("uniform");
    }
    syntax::StorageQualifier::Attribute => {
      let _ = f.write_str("attribute");
    }
    syntax::StorageQualifier::Varying => {
      let _ = f.write_str("varying");
    }
    syntax::StorageQualifier::Buffer => {
      let _ = f.write_str("buffer");
    }
    syntax::StorageQualifier::Shared => {
      let _ = f.write_str("shared");
    }
    syntax::StorageQualifier::Coherent => {
      let _ = f.write_str("coherent");
    }
    syntax::StorageQualifier::Volatile => {
      let _ = f.write_str("volatile");
    }
    syntax::StorageQualifier::Restrict => {
      let _ = f.write_str("restrict");
    }
    syntax::StorageQualifier::ReadOnly => {
      let _ = f.write_str("readonly");
    }
    syntax::StorageQualifier::WriteOnly => {
      let _ = f.write_str("writeonly");
    }
    syntax::StorageQualifier::Subroutine(ref n) => show_subroutine(f, &n),
  }
}

pub fn show_subroutine<F>(f: &mut F, types: &Vec<syntax::TypeName>)
where
  F: Write,
{
  let _ = f.write_str("subroutine");

  if !types.is_empty() {
    let _ = f.write_str("(");

    let mut types_iter = types.iter();
    let first = types_iter.next().unwrap();

    show_type_name(f, first);

    for type_name in types_iter {
      let _ = f.write_str(", ");
      show_type_name(f, type_name);
    }

    let _ = f.write_str(")");
  }
}

pub fn show_layout_qualifier<F>(f: &mut F, l: &syntax::LayoutQualifier)
where
  F: Write,
{
  let mut qualifiers = l.ids.0.iter();
  let first = qualifiers.next().unwrap();

  let _ = f.write_str("layout (");
  show_layout_qualifier_spec(f, first);

  for qual_spec in qualifiers {
    let _ = f.write_str(", ");
    show_layout_qualifier_spec(f, qual_spec);
  }

  let _ = f.write_str(")");
}

pub fn show_layout_qualifier_spec<F>(f: &mut F, l: &syntax::LayoutQualifierSpec)
where
  F: Write,
{
  match *l {
    syntax::LayoutQualifierSpec::Identifier(ref i, Some(ref e)) => {
      let _ = write!(f, "{} = ", i);
      show_expr(f, &e);
    }
    syntax::LayoutQualifierSpec::Identifier(ref i, None) => show_identifier(f, &i),
    syntax::LayoutQualifierSpec::Shared => {
      let _ = f.write_str("shared");
    }
  }
}

pub fn show_precision_qualifier<F>(f: &mut F, p: &syntax::PrecisionQualifier)
where
  F: Write,
{
  match *p {
    syntax::PrecisionQualifier::High => {
      let _ = f.write_str("highp");
    }
    syntax::PrecisionQualifier::Medium => {
      let _ = f.write_str("mediump");
    }
    syntax::PrecisionQualifier::Low => {
      let _ = f.write_str("low");
    }
  }
}

pub fn show_interpolation_qualifier<F>(f: &mut F, i: &syntax::InterpolationQualifier)
where
  F: Write,
{
  match *i {
    syntax::InterpolationQualifier::Smooth => {
      let _ = f.write_str("smooth");
    }
    syntax::InterpolationQualifier::Flat => {
      let _ = f.write_str("flat");
    }
    syntax::InterpolationQualifier::NoPerspective => {
      let _ = f.write_str("noperspective");
    }
  }
}

pub fn show_float<F>(f: &mut F, x: f32)
where
  F: Write,
{
  if x.fract() == 0. {
    let _ = write!(f, "{}.", x);
  } else {
    let _ = write!(f, "{}", x);
  }
}

pub fn show_double<F>(f: &mut F, x: f64)
where
  F: Write,
{
  if x.fract() == 0. {
    let _ = write!(f, "{}.lf", x);
  } else {
    let _ = write!(f, "{}lf", x);
  }
}

pub fn show_expr<F>(f: &mut F, expr: &syntax::Expr)
where
  F: Write,
{
  match *expr {
    syntax::Expr::Variable(ref i) => show_identifier(f, &i),
    syntax::Expr::IntConst(ref x) => {
      let _ = write!(f, "{}", x);
    }
    syntax::Expr::UIntConst(ref x) => {
      let _ = write!(f, "{}u", x);
    }
    syntax::Expr::BoolConst(ref x) => {
      let _ = write!(f, "{}", x);
    }
    syntax::Expr::FloatConst(ref x) => show_float(f, *x),
    syntax::Expr::DoubleConst(ref x) => show_double(f, *x),
    syntax::Expr::Unary(ref op, ref e) => {
      // Note: all unary ops are right-to-left associative
      show_unary_op(f, &op);

      if e.precedence() > op.precedence() {
        let _ = f.write_str("(");
        show_expr(f, &e);
        let _ = f.write_str(")");
      } else if let syntax::Expr::Unary(eop, _) = &**e {
        // Prevent double-unary plus/minus turning into inc/dec
        if eop == op && (*eop == syntax::UnaryOp::Add || *eop == syntax::UnaryOp::Minus) {
          let _ = f.write_str("(");
          show_expr(f, &e);
          let _ = f.write_str(")");
        } else {
          show_expr(f, &e);
        }
      } else {
        show_expr(f, &e);
      }
    }
    syntax::Expr::Binary(ref op, ref l, ref r) => {
      // Note: all binary ops are left-to-right associative (<= for left part)

      if l.precedence() <= op.precedence() {
        show_expr(f, &l);
      } else {
        let _ = f.write_str("(");
        show_expr(f, &l);
        let _ = f.write_str(")");
      }

      show_binary_op(f, &op);

      if r.precedence() < op.precedence() {
        show_expr(f, &r);
      } else {
        let _ = f.write_str("(");
        show_expr(f, &r);
        let _ = f.write_str(")");
      }
    }
    syntax::Expr::Ternary(ref c, ref s, ref e) => {
      // Note: ternary is right-to-left associative (<= for right part)

      if c.precedence() < expr.precedence() {
        show_expr(f, &c);
      } else {
        let _ = f.write_str("(");
        show_expr(f, &c);
        let _ = f.write_str(")");
      }
      let _ = f.write_str(" ? ");
      show_expr(f, &s);
      let _ = f.write_str(" : ");
      if e.precedence() <= expr.precedence() {
        show_expr(f, &e);
      } else {
        let _ = f.write_str("(");
        show_expr(f, &e);
        let _ = f.write_str(")");
      }
    }
    syntax::Expr::Assignment(ref v, ref op, ref e) => {
      // Note: all assignment ops are right-to-left associative

      if v.precedence() < op.precedence() {
        show_expr(f, &v);
      } else {
        let _ = f.write_str("(");
        show_expr(f, &v);
        let _ = f.write_str(")");
      }

      let _ = f.write_str(" ");
      show_assignment_op(f, &op);
      let _ = f.write_str(" ");

      if e.precedence() <= op.precedence() {
        show_expr(f, &e);
      } else {
        let _ = f.write_str("(");
        show_expr(f, &e);
        let _ = f.write_str(")");
      }
    }
    syntax::Expr::Bracket(ref e, ref a) => {
      // Note: bracket is left-to-right associative

      if e.precedence() <= expr.precedence() {
        show_expr(f, &e);
      } else {
        let _ = f.write_str("(");
        show_expr(f, &e);
        let _ = f.write_str(")");
      }

      show_array_spec(f, &a);
    }
    syntax::Expr::FunCall(ref fun, ref args) => {
      show_function_identifier(f, &fun);
      let _ = f.write_str("(");

      if !args.is_empty() {
        let mut args_iter = args.iter();
        let first = args_iter.next().unwrap();
        show_expr(f, first);

        for e in args_iter {
          let _ = f.write_str(", ");
          show_expr(f, e);
        }
      }

      let _ = f.write_str(")");
    }
    syntax::Expr::Dot(ref e, ref i) => {
      // Note: dot is left-to-right associative

      if e.precedence() <= expr.precedence() {
        show_expr(f, &e);
      } else {
        let _ = f.write_str("(");
        show_expr(f, &e);
        let _ = f.write_str(")");
      }
      let _ = f.write_str(".");
      show_identifier(f, &i);
    }
    syntax::Expr::PostInc(ref e) => {
      // Note: post-increment is right-to-left associative

      if e.precedence() < expr.precedence() {
        show_expr(f, &e);
      } else {
        let _ = f.write_str("(");
        show_expr(f, &e);
        let _ = f.write_str(")");
      }

      let _ = f.write_str("++");
    }
    syntax::Expr::PostDec(ref e) => {
      // Note: post-decrement is right-to-left associative

      if e.precedence() < expr.precedence() {
        show_expr(f, &e);
      } else {
        let _ = f.write_str("(");
        show_expr(f, &e);
        let _ = f.write_str(")");
      }

      let _ = f.write_str("--");
    }
    syntax::Expr::Comma(ref a, ref b) => {
      // Note: comma is left-to-right associative

      if a.precedence() <= expr.precedence() {
        show_expr(f, &a);
      } else {
        let _ = f.write_str("(");
        show_expr(f, &a);
        let _ = f.write_str(")");
      }

      let _ = f.write_str(", ");

      if b.precedence() < expr.precedence() {
        show_expr(f, &b);
      } else {
        let _ = f.write_str("(");
        show_expr(f, &b);
        let _ = f.write_str(")");
      }
    }
  }
}

pub fn show_path<F>(f: &mut F, path: &syntax::Path)
where
  F: Write,
{
  match path {
    syntax::Path::Absolute(s) => {
      let _ = write!(f, "<{}>", s);
    }
    syntax::Path::Relative(s) => {
      let _ = write!(f, "\"{}\"", s);
    }
  }
}

pub fn show_unary_op<F>(f: &mut F, op: &syntax::UnaryOp)
where
  F: Write,
{
  match *op {
    syntax::UnaryOp::Inc => {
      let _ = f.write_str("++");
    }
    syntax::UnaryOp::Dec => {
      let _ = f.write_str("--");
    }
    syntax::UnaryOp::Add => {
      let _ = f.write_str("+");
    }
    syntax::UnaryOp::Minus => {
      let _ = f.write_str("-");
    }
    syntax::UnaryOp::Not => {
      let _ = f.write_str("!");
    }
    syntax::UnaryOp::Complement => {
      let _ = f.write_str("~");
    }
  }
}

pub fn show_binary_op<F>(f: &mut F, op: &syntax::BinaryOp)
where
  F: Write,
{
  match *op {
    syntax::BinaryOp::Or => {
      let _ = f.write_str("||");
    }
    syntax::BinaryOp::Xor => {
      let _ = f.write_str("^^");
    }
    syntax::BinaryOp::And => {
      let _ = f.write_str("&&");
    }
    syntax::BinaryOp::BitOr => {
      let _ = f.write_str("|");
    }
    syntax::BinaryOp::BitXor => {
      let _ = f.write_str("^");
    }
    syntax::BinaryOp::BitAnd => {
      let _ = f.write_str("&");
    }
    syntax::BinaryOp::Equal => {
      let _ = f.write_str("==");
    }
    syntax::BinaryOp::NonEqual => {
      let _ = f.write_str("!=");
    }
    syntax::BinaryOp::LT => {
      let _ = f.write_str("<");
    }
    syntax::BinaryOp::GT => {
      let _ = f.write_str(">");
    }
    syntax::BinaryOp::LTE => {
      let _ = f.write_str("<=");
    }
    syntax::BinaryOp::GTE => {
      let _ = f.write_str(">=");
    }
    syntax::BinaryOp::LShift => {
      let _ = f.write_str("<<");
    }
    syntax::BinaryOp::RShift => {
      let _ = f.write_str(">>");
    }
    syntax::BinaryOp::Add => {
      let _ = f.write_str("+");
    }
    syntax::BinaryOp::Sub => {
      let _ = f.write_str("-");
    }
    syntax::BinaryOp::Mult => {
      let _ = f.write_str("*");
    }
    syntax::BinaryOp::Div => {
      let _ = f.write_str("/");
    }
    syntax::BinaryOp::Mod => {
      let _ = f.write_str("%");
    }
  }
}

pub fn show_assignment_op<F>(f: &mut F, op: &syntax::AssignmentOp)
where
  F: Write,
{
  match *op {
    syntax::AssignmentOp::Equal => {
      let _ = f.write_str("=");
    }
    syntax::AssignmentOp::Mult => {
      let _ = f.write_str("*=");
    }
    syntax::AssignmentOp::Div => {
      let _ = f.write_str("/=");
    }
    syntax::AssignmentOp::Mod => {
      let _ = f.write_str("%=");
    }
    syntax::AssignmentOp::Add => {
      let _ = f.write_str("+=");
    }
    syntax::AssignmentOp::Sub => {
      let _ = f.write_str("-=");
    }
    syntax::AssignmentOp::LShift => {
      let _ = f.write_str("<<=");
    }
    syntax::AssignmentOp::RShift => {
      let _ = f.write_str(">>=");
    }
    syntax::AssignmentOp::And => {
      let _ = f.write_str("&=");
    }
    syntax::AssignmentOp::Xor => {
      let _ = f.write_str("^=");
    }
    syntax::AssignmentOp::Or => {
      let _ = f.write_str("|=");
    }
  }
}

pub fn show_function_identifier<F>(f: &mut F, i: &syntax::FunIdentifier)
where
  F: Write,
{
  match *i {
    syntax::FunIdentifier::Identifier(ref n) => show_identifier(f, &n),
    syntax::FunIdentifier::Expr(ref e) => show_expr(f, &*e),
  }
}

pub fn show_declaration<F>(f: &mut F, d: &syntax::Declaration)
where
  F: Write,
{
  match *d {
    syntax::Declaration::FunctionPrototype(ref proto) => {
      show_function_prototype(f, &proto);
      let _ = f.write_str(";\n");
    }
    syntax::Declaration::InitDeclaratorList(ref list) => {
      show_init_declarator_list(f, &list);
      let _ = f.write_str(";\n");
    }
    syntax::Declaration::Precision(ref qual, ref ty) => {
      show_precision_qualifier(f, &qual);
      show_type_specifier(f, &ty);
      let _ = f.write_str(";\n");
    }
    syntax::Declaration::Block(ref block) => {
      show_block(f, &block);
      let _ = f.write_str(";\n");
    }
    syntax::Declaration::Global(ref qual, ref identifiers) => {
      show_type_qualifier(f, &qual);

      if !identifiers.is_empty() {
        let mut iter = identifiers.iter();
        let first = iter.next().unwrap();
        show_identifier(f, first);

        for identifier in iter {
          let _ = write!(f, ", {}", identifier);
        }
      }

      let _ = f.write_str(";\n");
    }
  }
}

pub fn show_function_prototype<F>(f: &mut F, fp: &syntax::FunctionPrototype)
where
  F: Write,
{
  show_fully_specified_type(f, &fp.ty);
  let _ = f.write_str(" ");
  show_identifier(f, &fp.name);

  let _ = f.write_str("(");

  if !fp.parameters.is_empty() {
    let mut iter = fp.parameters.iter();
    let first = iter.next().unwrap();
    show_function_parameter_declaration(f, first);

    for param in iter {
      let _ = f.write_str(", ");
      show_function_parameter_declaration(f, param);
    }
  }

  let _ = f.write_str(")");
}
pub fn show_function_parameter_declaration<F>(f: &mut F, p: &syntax::FunctionParameterDeclaration)
where
  F: Write,
{
  match *p {
    syntax::FunctionParameterDeclaration::Named(ref qual, ref fpd) => {
      if let Some(ref q) = *qual {
        show_type_qualifier(f, q);
        let _ = f.write_str(" ");
      }

      show_function_parameter_declarator(f, fpd);
    }
    syntax::FunctionParameterDeclaration::Unnamed(ref qual, ref ty) => {
      if let Some(ref q) = *qual {
        show_type_qualifier(f, q);
        let _ = f.write_str(" ");
      }

      show_type_specifier(f, ty);
    }
  }
}

pub fn show_function_parameter_declarator<F>(f: &mut F, p: &syntax::FunctionParameterDeclarator)
where
  F: Write,
{
  show_type_specifier(f, &p.ty);
  let _ = f.write_str(" ");
  show_arrayed_identifier(f, &p.ident);
}

pub fn show_init_declarator_list<F>(f: &mut F, i: &syntax::InitDeclaratorList)
where
  F: Write,
{
  show_single_declaration(f, &i.head);

  for decl in &i.tail {
    let _ = f.write_str(", ");
    show_single_declaration_no_type(f, decl);
  }
}

pub fn show_single_declaration<F>(f: &mut F, d: &syntax::SingleDeclaration)
where
  F: Write,
{
  show_fully_specified_type(f, &d.ty);

  if let Some(ref name) = d.name {
    let _ = f.write_str(" ");
    show_identifier(f, name);
  }

  if let Some(ref arr_spec) = d.array_specifier {
    show_array_spec(f, arr_spec);
  }

  if let Some(ref initializer) = d.initializer {
    let _ = f.write_str(" = ");
    show_initializer(f, initializer);
  }
}

pub fn show_single_declaration_no_type<F>(f: &mut F, d: &syntax::SingleDeclarationNoType)
where
  F: Write,
{
  show_arrayed_identifier(f, &d.ident);

  if let Some(ref initializer) = d.initializer {
    let _ = f.write_str(" = ");
    show_initializer(f, initializer);
  }
}

pub fn show_initializer<F>(f: &mut F, i: &syntax::Initializer)
where
  F: Write,
{
  match *i {
    syntax::Initializer::Simple(ref e) => show_expr(f, e),
    syntax::Initializer::List(ref list) => {
      let mut iter = list.0.iter();
      let first = iter.next().unwrap();

      let _ = f.write_str("{ ");
      show_initializer(f, first);

      for ini in iter {
        let _ = f.write_str(", ");
        show_initializer(f, ini);
      }

      let _ = f.write_str(" }");
    }
  }
}

pub fn show_block<F>(f: &mut F, b: &syntax::Block)
where
  F: Write,
{
  show_type_qualifier(f, &b.qualifier);
  let _ = f.write_str(" ");
  show_identifier(f, &b.name);
  let _ = f.write_str(" {");

  for field in &b.fields {
    show_struct_field(f, field);
    let _ = f.write_str("\n");
  }
  let _ = f.write_str("}");

  if let Some(ref ident) = b.identifier {
    show_arrayed_identifier(f, ident);
  }
}

pub fn show_function_definition<F>(f: &mut F, fd: &syntax::FunctionDefinition)
where
  F: Write,
{
  show_function_prototype(f, &fd.prototype);
  let _ = f.write_str(" ");
  show_compound_statement(f, &fd.statement);
}

pub fn show_compound_statement<F>(f: &mut F, cst: &syntax::CompoundStatement)
where
  F: Write,
{
  let _ = f.write_str("{\n");

  for st in &cst.statement_list {
    show_statement(f, st);
  }

  let _ = f.write_str("}\n");
}

pub fn show_statement<F>(f: &mut F, st: &syntax::Statement)
where
  F: Write,
{
  match *st {
    syntax::Statement::Compound(ref cst) => show_compound_statement(f, cst),
    syntax::Statement::Simple(ref sst) => show_simple_statement(f, sst),
  }
}

pub fn show_simple_statement<F>(f: &mut F, sst: &syntax::SimpleStatement)
where
  F: Write,
{
  match *sst {
    syntax::SimpleStatement::Declaration(ref d) => show_declaration(f, d),
    syntax::SimpleStatement::Expression(ref e) => show_expression_statement(f, e),
    syntax::SimpleStatement::Selection(ref s) => show_selection_statement(f, s),
    syntax::SimpleStatement::Switch(ref s) => show_switch_statement(f, s),
    syntax::SimpleStatement::CaseLabel(ref cl) => show_case_label(f, cl),
    syntax::SimpleStatement::Iteration(ref i) => show_iteration_statement(f, i),
    syntax::SimpleStatement::Jump(ref j) => show_jump_statement(f, j),
  }
}

pub fn show_expression_statement<F>(f: &mut F, est: &syntax::ExprStatement)
where
  F: Write,
{
  if let Some(ref e) = *est {
    show_expr(f, e);
  }

  let _ = f.write_str(";\n");
}

pub fn show_selection_statement<F>(f: &mut F, sst: &syntax::SelectionStatement)
where
  F: Write,
{
  let _ = f.write_str("if (");
  show_expr(f, &sst.cond);
  let _ = f.write_str(") {\n");
  show_selection_rest_statement(f, &sst.rest);
}

pub fn show_selection_rest_statement<F>(f: &mut F, sst: &syntax::SelectionRestStatement)
where
  F: Write,
{
  match *sst {
    syntax::SelectionRestStatement::Statement(ref if_st) => {
      show_statement(f, if_st);
      let _ = f.write_str("}\n");
    }
    syntax::SelectionRestStatement::Else(ref if_st, ref else_st) => {
      show_statement(f, if_st);
      let _ = f.write_str("} else ");
      show_statement(f, else_st);
    }
  }
}

pub fn show_switch_statement<F>(f: &mut F, sst: &syntax::SwitchStatement)
where
  F: Write,
{
  let _ = f.write_str("switch (");
  show_expr(f, &sst.head);
  let _ = f.write_str(") {\n");

  for st in &sst.body {
    show_statement(f, st);
  }

  let _ = f.write_str("}\n");
}

pub fn show_case_label<F>(f: &mut F, cl: &syntax::CaseLabel)
where
  F: Write,
{
  match *cl {
    syntax::CaseLabel::Case(ref e) => {
      let _ = f.write_str("case ");
      show_expr(f, e);
      let _ = f.write_str(":\n");
    }
    syntax::CaseLabel::Def => {
      let _ = f.write_str("default:\n");
    }
  }
}

pub fn show_iteration_statement<F>(f: &mut F, ist: &syntax::IterationStatement)
where
  F: Write,
{
  match *ist {
    syntax::IterationStatement::While(ref cond, ref body) => {
      let _ = f.write_str("while (");
      show_condition(f, cond);
      let _ = f.write_str(") ");
      show_statement(f, body);
    }
    syntax::IterationStatement::DoWhile(ref body, ref cond) => {
      let _ = f.write_str("do ");
      show_statement(f, body);
      let _ = f.write_str(" while (");
      show_expr(f, cond);
      let _ = f.write_str(")\n");
    }
    syntax::IterationStatement::For(ref init, ref rest, ref body) => {
      let _ = f.write_str("for (");
      show_for_init_statement(f, init);
      show_for_rest_statement(f, rest);
      let _ = f.write_str(") ");
      show_statement(f, body);
    }
  }
}

pub fn show_condition<F>(f: &mut F, c: &syntax::Condition)
where
  F: Write,
{
  match *c {
    syntax::Condition::Expr(ref e) => show_expr(f, e),
    syntax::Condition::Assignment(ref ty, ref name, ref initializer) => {
      show_fully_specified_type(f, ty);
      let _ = f.write_str(" ");
      show_identifier(f, name);
      let _ = f.write_str(" = ");
      show_initializer(f, initializer);
    }
  }
}

pub fn show_for_init_statement<F>(f: &mut F, i: &syntax::ForInitStatement)
where
  F: Write,
{
  match *i {
    syntax::ForInitStatement::Expression(ref expr) => {
      if let Some(ref e) = *expr {
        show_expr(f, e);
      }
    }
    syntax::ForInitStatement::Declaration(ref d) => show_declaration(f, d),
  }
}

pub fn show_for_rest_statement<F>(f: &mut F, r: &syntax::ForRestStatement)
where
  F: Write,
{
  if let Some(ref cond) = r.condition {
    show_condition(f, cond);
  }

  let _ = f.write_str("; ");

  if let Some(ref e) = r.post_expr {
    show_expr(f, e);
  }
}

pub fn show_jump_statement<F>(f: &mut F, j: &syntax::JumpStatement)
where
  F: Write,
{
  match *j {
    syntax::JumpStatement::Continue => {
      let _ = f.write_str("continue;\n");
    }
    syntax::JumpStatement::Break => {
      let _ = f.write_str("break;\n");
    }
    syntax::JumpStatement::Discard => {
      let _ = f.write_str("discard;\n");
    }
    syntax::JumpStatement::Return(ref e) => {
      let _ = f.write_str("return ");
      if let Some(e) = e {
        show_expr(f, e);
      }
      let _ = f.write_str(";\n");
    }
  }
}

pub fn show_preprocessor<F>(f: &mut F, pp: &syntax::Preprocessor)
where
  F: Write,
{
  match *pp {
    syntax::Preprocessor::Define(ref pd) => show_preprocessor_define(f, pd),
    syntax::Preprocessor::Else => show_preprocessor_else(f),
    syntax::Preprocessor::ElseIf(ref pei) => show_preprocessor_elseif(f, pei),
    syntax::Preprocessor::EndIf => show_preprocessor_endif(f),
    syntax::Preprocessor::Error(ref pe) => show_preprocessor_error(f, pe),
    syntax::Preprocessor::If(ref pi) => show_preprocessor_if(f, pi),
    syntax::Preprocessor::IfDef(ref pid) => show_preprocessor_ifdef(f, pid),
    syntax::Preprocessor::IfNDef(ref pind) => show_preprocessor_ifndef(f, pind),
    syntax::Preprocessor::Include(ref pi) => show_preprocessor_include(f, pi),
    syntax::Preprocessor::Line(ref pl) => show_preprocessor_line(f, pl),
    syntax::Preprocessor::Pragma(ref pp) => show_preprocessor_pragma(f, pp),
    syntax::Preprocessor::Undef(ref pu) => show_preprocessor_undef(f, pu),
    syntax::Preprocessor::Version(ref pv) => show_preprocessor_version(f, pv),
    syntax::Preprocessor::Extension(ref pe) => show_preprocessor_extension(f, pe),
  }
}

pub fn show_preprocessor_define<F>(f: &mut F, pd: &syntax::PreprocessorDefine)
where
  F: Write,
{
  match *pd {
    syntax::PreprocessorDefine::ObjectLike {
      ref ident,
      ref value,
    } => {
      let _ = write!(f, "#define {} {}\n", ident, value);
    }

    syntax::PreprocessorDefine::FunctionLike {
      ref ident,
      ref args,
      ref value,
    } => {
      let _ = write!(f, "#define {}(", ident);

      if !args.is_empty() {
        let _ = write!(f, "{}", &args[0]);

        for arg in &args[1..args.len()] {
          let _ = write!(f, ", {}", arg);
        }
      }

      let _ = write!(f, ") {}\n", value);
    }
  }
}

pub fn show_preprocessor_else<F>(f: &mut F)
where
  F: Write,
{
  let _ = f.write_str("#else\n");
}

pub fn show_preprocessor_elseif<F>(f: &mut F, pei: &syntax::PreprocessorElseIf)
where
  F: Write,
{
  let _ = write!(f, "#elseif {}\n", pei.condition);
}

pub fn show_preprocessor_error<F>(f: &mut F, pe: &syntax::PreprocessorError)
where
  F: Write,
{
  let _ = writeln!(f, "#error {}", pe.message);
}

pub fn show_preprocessor_endif<F>(f: &mut F)
where
  F: Write,
{
  let _ = f.write_str("#endif\n");
}

pub fn show_preprocessor_if<F>(f: &mut F, pi: &syntax::PreprocessorIf)
where
  F: Write,
{
  let _ = write!(f, "#if {}\n", pi.condition);
}

pub fn show_preprocessor_ifdef<F>(f: &mut F, pid: &syntax::PreprocessorIfDef)
where
  F: Write,
{
  let _ = f.write_str("#ifdef ");
  show_identifier(f, &pid.ident);
  let _ = f.write_str("\n");
}

pub fn show_preprocessor_ifndef<F>(f: &mut F, pind: &syntax::PreprocessorIfNDef)
where
  F: Write,
{
  let _ = f.write_str("#ifndef ");
  show_identifier(f, &pind.ident);
  let _ = f.write_str("\n");
}

pub fn show_preprocessor_include<F>(f: &mut F, pi: &syntax::PreprocessorInclude)
where
  F: Write,
{
  let _ = f.write_str("#include ");
  show_path(f, &pi.path);
  let _ = f.write_str("\n");
}

pub fn show_preprocessor_line<F>(f: &mut F, pl: &syntax::PreprocessorLine)
where
  F: Write,
{
  let _ = write!(f, "#line {}", pl.line);
  if let Some(source_string_number) = pl.source_string_number {
    let _ = write!(f, " {}", source_string_number);
  }
  let _ = f.write_str("\n");
}

pub fn show_preprocessor_pragma<F>(f: &mut F, pp: &syntax::PreprocessorPragma)
where
  F: Write,
{
  let _ = writeln!(f, "#pragma {}", pp.command);
}

pub fn show_preprocessor_undef<F>(f: &mut F, pud: &syntax::PreprocessorUndef)
where
  F: Write,
{
  let _ = f.write_str("#undef ");
  show_identifier(f, &pud.name);
  let _ = f.write_str("\n");
}

pub fn show_preprocessor_version<F>(f: &mut F, pv: &syntax::PreprocessorVersion)
where
  F: Write,
{
  let _ = write!(f, "#version {}", pv.version);

  if let Some(ref profile) = pv.profile {
    match *profile {
      syntax::PreprocessorVersionProfile::Core => {
        let _ = f.write_str(" core");
      }
      syntax::PreprocessorVersionProfile::Compatibility => {
        let _ = f.write_str(" compatibility");
      }
      syntax::PreprocessorVersionProfile::ES => {
        let _ = f.write_str(" es");
      }
    }
  }

  let _ = f.write_str("\n");
}

pub fn show_preprocessor_extension<F>(f: &mut F, pe: &syntax::PreprocessorExtension)
where
  F: Write,
{
  let _ = f.write_str("#extension ");

  match pe.name {
    syntax::PreprocessorExtensionName::All => {
      let _ = f.write_str("all");
    }
    syntax::PreprocessorExtensionName::Specific(ref n) => {
      let _ = f.write_str(n);
    }
  }

  if let Some(ref behavior) = pe.behavior {
    match *behavior {
      syntax::PreprocessorExtensionBehavior::Require => {
        let _ = f.write_str(" : require");
      }
      syntax::PreprocessorExtensionBehavior::Enable => {
        let _ = f.write_str(" : enable");
      }
      syntax::PreprocessorExtensionBehavior::Warn => {
        let _ = f.write_str(" : warn");
      }
      syntax::PreprocessorExtensionBehavior::Disable => {
        let _ = f.write_str(" : disable");
      }
    }
  }

  let _ = f.write_str("\n");
}

pub fn show_external_declaration<F>(f: &mut F, ed: &syntax::ExternalDeclaration)
where
  F: Write,
{
  match *ed {
    syntax::ExternalDeclaration::Preprocessor(ref pp) => show_preprocessor(f, pp),
    syntax::ExternalDeclaration::FunctionDefinition(ref fd) => show_function_definition(f, fd),
    syntax::ExternalDeclaration::Declaration(ref d) => show_declaration(f, d),
  }
}

pub fn show_translation_unit<F>(f: &mut F, tu: &syntax::TranslationUnit)
where
  F: Write,
{
  for ed in &(tu.0).0 {
    show_external_declaration(f, ed);
  }
}

#[cfg(test)]
mod tests {
  use super::*;
  use crate::parsers::expr;

  fn to_string(e: &syntax::Expr) -> String {
    let mut s = String::new();
    show_expr(&mut s, e);
    s
  }

  #[test]
  fn unary_parentheses() {
    assert_eq!(to_string(&expr("-a").unwrap().1), "-a");
    assert_eq!(to_string(&expr("-(a + b)").unwrap().1), "-(a+b)");
    assert_eq!(to_string(&expr("-a.x").unwrap().1), "-a.x");

    assert_eq!(to_string(&expr("-(-a)").unwrap().1), "-(-a)");
    assert_eq!(to_string(&expr("+(+a)").unwrap().1), "+(+a)");
    assert_eq!(to_string(&expr("~~a").unwrap().1), "~~a");
    assert_eq!(to_string(&expr("--a").unwrap().1), "--a");
    assert_eq!(to_string(&expr("++a").unwrap().1), "++a");
    assert_eq!(to_string(&expr("+-a").unwrap().1), "+-a");
  }

  #[test]
  fn binary_parentheses() {
    assert_eq!(to_string(&expr("a + b").unwrap().1), "a+b");
    assert_eq!(to_string(&expr("a * b + c").unwrap().1), "a*b+c");
    assert_eq!(to_string(&expr("(a + b) * c").unwrap().1), "(a+b)*c");
    assert_eq!(to_string(&expr("a + (b * c)").unwrap().1), "a+b*c");
    assert_eq!(to_string(&expr("a * (b + c)").unwrap().1), "a*(b+c)");
    assert_eq!(to_string(&expr("(a * b) * c").unwrap().1), "a*b*c");
    assert_eq!(to_string(&expr("a * (b * c)").unwrap().1), "a*(b*c)");
    assert_eq!(to_string(&expr("a&&b&&c").unwrap().1), "a&&b&&c");
    assert_eq!(
      to_string(&expr("n - p > 0. && u.y < n && u.y > p").unwrap().1),
      "n-p>0.&&u.y<n&&u.y>p"
    );
  }

  #[test]
  fn ternary_parentheses() {
    assert_eq!(
      to_string(&expr("a ? b : c ? d : e").unwrap().1),
      "a ? b : c ? d : e"
    );
    assert_eq!(
      to_string(&expr("(a ? b : c) ? d : e").unwrap().1),
      "(a ? b : c) ? d : e"
    );
  }

  #[test]
  fn assignment_parentheses() {
    assert_eq!(to_string(&expr("a = b = c").unwrap().1), "a = b = c");
    assert_eq!(to_string(&expr("(a = b) = c").unwrap().1), "(a = b) = c");
  }

  #[test]
  fn dot_parentheses() {
    assert_eq!(to_string(&expr("a.x").unwrap().1), "a.x");
    assert_eq!(to_string(&expr("(a + b).x").unwrap().1), "(a+b).x");
  }

  #[test]
  fn test_parentheses() {
    use crate::parsers::function_definition;

    const SRC: &'static str = r#"vec2 main() {
float n = 0.;
float p = 0.;
float u = vec2(0., 0.);
if (n-p>0.&&u.y<n&&u.y>p) {
}
return u;
}
"#;

    // Ideally we would use SRC as the expected, but there's a bug in block braces generation
    const DST: &'static str = r#"vec2 main() {
float n = 0.;
float p = 0.;
float u = vec2(0., 0.);
if (n-p>0.&&u.y<n&&u.y>p) {
{
}
}
return u;
}
"#;

    let mut s = String::new();
    show_function_definition(&mut s, &function_definition(SRC).unwrap().1);

    assert_eq!(s, DST);
  }

  #[test]
  fn roundtrip_glsl_complex_expr() {
    let zero = syntax::Expr::DoubleConst(0.);
    let ray = syntax::Expr::Variable("ray".into());
    let raydir = syntax::Expr::Dot(Box::new(ray), "dir".into());
    let vec4 = syntax::Expr::FunCall(
      syntax::FunIdentifier::Identifier("vec4".into()),
      vec![raydir, zero],
    );
    let view = syntax::Expr::Variable("view".into());
    let iview = syntax::Expr::FunCall(
      syntax::FunIdentifier::Identifier("inverse".into()),
      vec![view],
    );
    let mul = syntax::Expr::Binary(syntax::BinaryOp::Mult, Box::new(iview), Box::new(vec4));
    let xyz = syntax::Expr::Dot(Box::new(mul), "xyz".into());
    let input = syntax::Expr::FunCall(
      syntax::FunIdentifier::Identifier("normalize".into()),
      vec![xyz],
    );

    let mut output = String::new();
    show_expr(&mut output, &input);
    let _ = output.write_str(";");

    let back = expr(&output);

    assert_eq!(back, Ok((";", input)), "intermediate source '{}'", output);
  }
}
