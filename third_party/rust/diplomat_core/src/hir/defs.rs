//! Type definitions for structs, output structs, opaque structs, and enums.

use super::{Everywhere, IdentBuf, Method, OutputOnly, TyPosition, Type};
use crate::ast::Docs;

pub enum ReturnableStructDef<'tcx> {
    Struct(&'tcx StructDef),
    OutStruct(&'tcx OutStructDef),
}

#[derive(Copy, Clone, Debug)]
pub enum TypeDef<'tcx> {
    Struct(&'tcx StructDef),
    OutStruct(&'tcx OutStructDef),
    Opaque(&'tcx OpaqueDef),
    Enum(&'tcx EnumDef),
}

/// Structs that can only be returned from methods.
pub type OutStructDef = StructDef<OutputOnly>;

/// Structs that can be either inputs or outputs in methods.
#[derive(Debug)]
pub struct StructDef<P: TyPosition = Everywhere> {
    pub docs: Docs,
    pub name: IdentBuf,
    pub fields: Vec<StructField<P>>,
    pub methods: Vec<Method>,
}

/// A struct whose contents are opaque across the FFI boundary, and can only
/// cross when behind a pointer.
///
/// All opaques can be inputs or outputs when behind a reference, but owned
/// opaques can only be returned since there isn't a general way for most languages
/// to give up ownership.
///
/// A struct marked with `#[diplomat::opaque]`.
#[derive(Debug)]
pub struct OpaqueDef {
    pub docs: Docs,
    pub name: IdentBuf,
    pub methods: Vec<Method>,
}

/// The enum type.
#[derive(Debug)]
pub struct EnumDef {
    pub docs: Docs,
    pub name: IdentBuf,
    pub variants: Vec<EnumVariant>,
    pub methods: Vec<Method>,
}

/// A field on a [`OutStruct`]s.
pub type OutStructField = StructField<OutputOnly>;

/// A field on a [`Struct`]s.
#[derive(Debug)]
pub struct StructField<P: TyPosition = Everywhere> {
    pub docs: Docs,
    pub name: IdentBuf,
    pub ty: Type<P>,
}

/// A variant of an [`Enum`].
#[derive(Debug)]
pub struct EnumVariant {
    pub docs: Docs,
    pub name: IdentBuf,
    pub discriminant: isize,
}

impl<P: TyPosition> StructDef<P> {
    pub(super) fn new(
        docs: Docs,
        name: IdentBuf,
        fields: Vec<StructField<P>>,
        methods: Vec<Method>,
    ) -> Self {
        Self {
            docs,
            name,
            fields,
            methods,
        }
    }
}

impl OpaqueDef {
    pub(super) fn new(docs: Docs, name: IdentBuf, methods: Vec<Method>) -> Self {
        Self {
            docs,
            name,
            methods,
        }
    }
}

impl EnumDef {
    pub(super) fn new(
        docs: Docs,
        name: IdentBuf,
        variants: Vec<EnumVariant>,
        methods: Vec<Method>,
    ) -> Self {
        Self {
            docs,
            name,
            variants,
            methods,
        }
    }
}

impl<'a> From<&'a StructDef> for TypeDef<'a> {
    fn from(x: &'a StructDef) -> Self {
        TypeDef::Struct(x)
    }
}

impl<'a> From<&'a OutStructDef> for TypeDef<'a> {
    fn from(x: &'a OutStructDef) -> Self {
        TypeDef::OutStruct(x)
    }
}

impl<'a> From<&'a OpaqueDef> for TypeDef<'a> {
    fn from(x: &'a OpaqueDef) -> Self {
        TypeDef::Opaque(x)
    }
}

impl<'a> From<&'a EnumDef> for TypeDef<'a> {
    fn from(x: &'a EnumDef) -> Self {
        TypeDef::Enum(x)
    }
}

impl<'tcx> TypeDef<'tcx> {
    pub fn name(&self) -> &'tcx IdentBuf {
        match *self {
            Self::Struct(ty) => &ty.name,
            Self::OutStruct(ty) => &ty.name,
            Self::Opaque(ty) => &ty.name,
            Self::Enum(ty) => &ty.name,
        }
    }

    pub fn docs(&self) -> &'tcx Docs {
        match *self {
            Self::Struct(ty) => &ty.docs,
            Self::OutStruct(ty) => &ty.docs,
            Self::Opaque(ty) => &ty.docs,
            Self::Enum(ty) => &ty.docs,
        }
    }
    pub fn methods(&self) -> &'tcx [Method] {
        match *self {
            Self::Struct(ty) => &ty.methods,
            Self::OutStruct(ty) => &ty.methods,
            Self::Opaque(ty) => &ty.methods,
            Self::Enum(ty) => &ty.methods,
        }
    }
}
