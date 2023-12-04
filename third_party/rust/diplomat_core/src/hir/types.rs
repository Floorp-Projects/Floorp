//! Types that can be exposed in Diplomat APIs.

use super::{
    EnumPath, Everywhere, MaybeStatic, NonOptional, OpaquePath, Optional, OutputOnly,
    PrimitiveType, StructPath, TyPosition, TypeContext, TypeLifetime,
};
use crate::ast;
pub use ast::Mutability;

/// Type that can only be used as an output.
pub type OutType = Type<OutputOnly>;

/// Type that may be used as input or output.
#[derive(Debug)]
#[non_exhaustive]
pub enum Type<P: TyPosition = Everywhere> {
    Primitive(PrimitiveType),
    Opaque(OpaquePath<Optional, P::OpaqueOwnership>),
    Struct(P::StructPath),
    Enum(EnumPath),
    Slice(Slice),
}

/// Type that can appear in the `self` position.
#[derive(Debug, Clone)]
#[non_exhaustive]
pub enum SelfType {
    Opaque(OpaquePath<NonOptional, Borrow>),
    Struct(StructPath),
    Enum(EnumPath),
}

#[derive(Copy, Clone, Debug)]
#[non_exhaustive]
pub enum Slice {
    /// A string slice, e.g. `&str`.
    Str(MaybeStatic<TypeLifetime>),

    /// A primitive slice, e.g. `&mut [u8]`.
    Primitive(Borrow, PrimitiveType),
}

// For now, the lifetime in not optional. This is because when you have references
// as fields of structs, the lifetime must always be present, and we want to uphold
// this invariant at the type level within the HIR.
//
// The only time when a lifetime is optional in Rust code is in function signatures,
// where implicit lifetimes are allowed. Getting this to all fit together will
// involve getting the implicit lifetime thing to be understood by Diplomat, but
// should be doable.
#[derive(Copy, Clone, Debug)]
#[non_exhaustive]
pub struct Borrow {
    pub lifetime: MaybeStatic<TypeLifetime>,
    pub mutability: Mutability,
}

impl Type {
    /// Return the number of fields and leaves that will show up in the [`LifetimeTree`]
    /// returned by [`Param::lifetime_tree`] and [`ParamSelf::lifetime_tree`].
    ///
    /// This method is used to calculate how much space to allocate upfront.
    pub(super) fn field_leaf_lifetime_counts(&self, tcx: &TypeContext) -> (usize, usize) {
        match self {
            Type::Struct(ty) => ty.resolve(tcx).fields.iter().fold((1, 0), |acc, field| {
                let inner = field.ty.field_leaf_lifetime_counts(tcx);
                (acc.0 + inner.0, acc.1 + inner.1)
            }),
            Type::Opaque(_) | Type::Slice(_) => (1, 1),
            Type::Primitive(_) | Type::Enum(_) => (0, 0),
        }
    }
}

impl SelfType {
    /// Returns whether the self parameter is borrowed immutably.
    ///
    /// Curently this can only happen with opaque types.
    pub fn is_immutably_borrowed(&self) -> bool {
        match self {
            SelfType::Opaque(opaque_path) => opaque_path.owner.mutability == Mutability::Immutable,
            _ => false,
        }
    }
}

impl Slice {
    /// Returns the [`TypeLifetime`] contained in either the `Str` or `Primitive`
    /// variant.
    pub fn lifetime(&self) -> &MaybeStatic<TypeLifetime> {
        match self {
            Slice::Str(lifetime) => lifetime,
            Slice::Primitive(reference, _) => &reference.lifetime,
        }
    }
}

impl Borrow {
    pub(super) fn new(lifetime: MaybeStatic<TypeLifetime>, mutability: Mutability) -> Self {
        Self {
            lifetime,
            mutability,
        }
    }
}

impl From<SelfType> for Type {
    fn from(s: SelfType) -> Type {
        match s {
            SelfType::Opaque(o) => Type::Opaque(o.wrap_optional()),
            SelfType::Struct(s) => Type::Struct(s),
            SelfType::Enum(e) => Type::Enum(e),
        }
    }
}
