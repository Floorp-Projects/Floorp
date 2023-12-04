//! Type definitions for structs, output structs, opaque structs, and enums.

use super::{Attrs, Everywhere, IdentBuf, Method, OutputOnly, TyPosition, Type};
use crate::ast::Docs;

#[non_exhaustive]
pub enum ReturnableStructDef<'tcx> {
    Struct(&'tcx StructDef),
    OutStruct(&'tcx OutStructDef),
}

#[derive(Copy, Clone, Debug)]
#[non_exhaustive]
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
#[non_exhaustive]
pub struct StructDef<P: TyPosition = Everywhere> {
    pub docs: Docs,
    pub name: IdentBuf,
    pub fields: Vec<StructField<P>>,
    pub methods: Vec<Method>,
    pub attrs: Attrs,
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
#[non_exhaustive]
pub struct OpaqueDef {
    pub docs: Docs,
    pub name: IdentBuf,
    pub methods: Vec<Method>,
    pub attrs: Attrs,
}

/// The enum type.
#[derive(Debug)]
#[non_exhaustive]
pub struct EnumDef {
    pub docs: Docs,
    pub name: IdentBuf,
    pub variants: Vec<EnumVariant>,
    pub methods: Vec<Method>,
    pub attrs: Attrs,
}

/// A field on a [`OutStruct`]s.
pub type OutStructField = StructField<OutputOnly>;

/// A field on a [`Struct`]s.
#[derive(Debug)]
#[non_exhaustive]
pub struct StructField<P: TyPosition = Everywhere> {
    pub docs: Docs,
    pub name: IdentBuf,
    pub ty: Type<P>,
}

/// A variant of an [`Enum`].
#[derive(Debug)]
#[non_exhaustive]
pub struct EnumVariant {
    pub docs: Docs,
    pub name: IdentBuf,
    pub discriminant: isize,
    pub attrs: Attrs,
}

impl<P: TyPosition> StructDef<P> {
    pub(super) fn new(
        docs: Docs,
        name: IdentBuf,
        fields: Vec<StructField<P>>,
        methods: Vec<Method>,
        attrs: Attrs,
    ) -> Self {
        Self {
            docs,
            name,
            fields,
            methods,
            attrs,
        }
    }
}

impl OpaqueDef {
    pub(super) fn new(docs: Docs, name: IdentBuf, methods: Vec<Method>, attrs: Attrs) -> Self {
        Self {
            docs,
            name,
            methods,
            attrs,
        }
    }
}

impl EnumDef {
    pub(super) fn new(
        docs: Docs,
        name: IdentBuf,
        variants: Vec<EnumVariant>,
        methods: Vec<Method>,
        attrs: Attrs,
    ) -> Self {
        Self {
            docs,
            name,
            variants,
            methods,
            attrs,
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

    pub fn attrs(&self) -> &'tcx Attrs {
        match *self {
            Self::Struct(ty) => &ty.attrs,
            Self::OutStruct(ty) => &ty.attrs,
            Self::Opaque(ty) => &ty.attrs,
            Self::Enum(ty) => &ty.attrs,
        }
    }
}
