use super::{
    Borrow, MaybeOwn, Mutability, OutStructId, ReturnableStructPath, StructId, StructPath, TypeId,
};
use core::fmt::Debug;

/// Abstraction over where a type can appear in a function signature.
///
/// # "Output only" and "everywhere" types
///
/// While Rust is able to give up ownership of values, languages that Diplomat
/// supports (C++, Javascript, etc.) generally cannot. For example, we can
/// construct a `Box<MyOpaque>` in a Rust function and _return_ it to the other
/// language as a pointer. However, we cannot _accept_ `Box<MyOpaque>` as an input
/// because there's nothing stopping other languages from using that value again.
/// Therefore, we classify boxed opaques as "output only" types, since they can
/// only be returned from Rust but not taken as inputs.
///
/// Furthermore, Diplomat also supports "bag o' stuff" structs where all fields get
/// translated at the boundary. If one contains an "output only" type as a field,
/// then the whole struct must also be "output only". In particular, this means
/// that if a boxed opaque is nested in a bunch of "bag o' stuff" structs, than
/// all of those structs must also be "output only".
///
/// Currently, there are only two classes of structs: those that are "output only",
/// and those that are not. These are represented by the types [`OutputOnly`]
/// and [`Everywhere`] marker types respectively, which are the _only_ two types
/// that implement [`TyPosition`].
///
/// # How does abstraction help?
///
/// The HIR was designed around the idea of making invalid states unrepresentable.
/// Since "output only" types can contain values that "everywhere" types cannot,
/// it doesn't make sense for them to be represented in the same type, even if
/// they're mostly the same. One of these differences is that opaques (which are
/// always behind a pointer) can only be represented as a borrow in "everywhere"
/// types, but can additionally be represented as owned in "output only" types.
/// If we were to use the same type for both, then backends working with "everywhere"
/// types would constantly have unreachable statements for owned opaque cases.
///
/// That being said, "output only" and "everywhere" types are still mostly the
/// same, so this trait allows us to describe the differences. For example, the
/// HIR uses a singular [`Type`](super::Type) type for representing both
/// "output only" types and "everywhere" types, since it takes advantage of this
/// traits associated types to "fill in" the different parts:
/// ```ignore
/// pub enum Type<P: TyPosition = Everywhere> {
///     Primitive(PrimitiveType),
///     Opaque(OpaquePath<Optional, P::OpaqueOwnership>),
///     Struct(P::StructPath),
///     Enum(EnumPath),
///     Slice(Slice),
/// }
/// ```
///
/// When `P` takes on [`Everywhere`], this signature becomes:
/// ```ignore
/// pub enum Type {
///     Primitive(PrimitiveType),
///     Opaque(OpaquePath<Optional, Borrow>),
///     Struct(StructPath),
///     Enum(EnumPath),
///     Slice(Slice),
/// }
/// ```
///
/// This allows us to represent any kind of type that can appear "everywhere"
/// i.e. in inputs or outputs. Notice how the second generic in the `Opaque`
/// variant becomes [`Borrow`]. This describes the semantics of the pointer that
/// the opaque lives behind, and shows that for "everywhere" types, opaques
/// can _only_ be represented as living behind a borrow.
///
/// Contrast this to when `P` takes on [`OutputOnly`]:
/// ```ignore
/// pub enum Type {
///     Primitive(PrimitiveType),
///     Opaque(OpaquePath<Optional, MaybeOwn>),
///     Struct(OutStructPath),
///     Enum(EnumPath),
///     Slice(Slice),
/// }
/// ```
/// Here, the second generic of the `Opaque` variant becomes [`MaybeOwn`], meaning
/// that "output only" types can contain opaques that are either borrowed _or_ owned.
///
/// Therefore, this trait allows be extremely precise about making invalid states
/// unrepresentable, while also reducing duplicated code.
///
pub trait TyPosition: Debug + Copy {
    const IS_OUT_ONLY: bool;

    /// Type representing how we can point to opaques, which must always be behind a pointer.
    ///
    /// The types represented by [`OutputOnly`] are capable of either owning or
    /// borrowing opaques, and so the associated type for that impl is [`MaybeOwn`].
    ///
    /// On the other hand, types represented by [`Everywhere`] can only contain
    /// borrowes, so the associated type for that impl is [`Borrow`].
    type OpaqueOwnership: Debug + OpaqueOwner;

    type StructId: Debug;

    type StructPath: Debug;

    fn id_for_path(p: &Self::StructPath) -> TypeId;
}

/// One of two types implementing [`TyPosition`], representing types that can be
/// used as both input and output to functions.
///
/// The complement of this type is [`OutputOnly`].
#[derive(Debug, Copy, Clone)]
#[non_exhaustive]
pub struct Everywhere;

/// One of two types implementing [`TyPosition`], representing types that can
/// only be used as return types in functions.
///
/// The complement of this type is [`Everywhere`].
#[derive(Debug, Copy, Clone)]
#[non_exhaustive]
pub struct OutputOnly;

impl TyPosition for Everywhere {
    const IS_OUT_ONLY: bool = false;
    type OpaqueOwnership = Borrow;
    type StructId = StructId;
    type StructPath = StructPath;

    fn id_for_path(p: &Self::StructPath) -> TypeId {
        p.tcx_id.into()
    }
}

impl TyPosition for OutputOnly {
    const IS_OUT_ONLY: bool = true;
    type OpaqueOwnership = MaybeOwn;
    type StructId = OutStructId;
    type StructPath = ReturnableStructPath;
    fn id_for_path(p: &Self::StructPath) -> TypeId {
        match p {
            ReturnableStructPath::Struct(p) => p.tcx_id.into(),
            ReturnableStructPath::OutStruct(p) => p.tcx_id.into(),
        }
    }
}

/// Abstraction over how a type can hold a pointer to an opaque.
///
/// This trait is designed as a helper abstraction for the `OpaqueOwnership`
/// associated type in the [`TyPosition`] trait. As such, only has two implementing
/// types: [`MaybeOwn`] and [`Borrow`] for the [`OutputOnly`] and [`Everywhere`]
/// implementations of [`TyPosition`] respectively.
pub trait OpaqueOwner {
    /// Return the mutability of this owner
    fn mutability(&self) -> Option<Mutability>;

    fn is_owned(&self) -> bool;
}

impl OpaqueOwner for MaybeOwn {
    fn mutability(&self) -> Option<Mutability> {
        match self {
            MaybeOwn::Own => None,
            MaybeOwn::Borrow(b) => b.mutability(),
        }
    }

    fn is_owned(&self) -> bool {
        match self {
            MaybeOwn::Own => true,
            MaybeOwn::Borrow(_) => false,
        }
    }
}

impl OpaqueOwner for Borrow {
    fn mutability(&self) -> Option<Mutability> {
        Some(self.mutability)
    }

    fn is_owned(&self) -> bool {
        false
    }
}
