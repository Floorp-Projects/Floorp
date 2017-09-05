//! Traits for determining whether we can derive traits for a thing or not.

use super::context::BindgenContext;

/// A trait that encapsulates the logic for whether or not we can derive `Debug`
/// for a given thing.
///
/// This should ideally be a no-op that just returns `true`, but instead needs
/// to be a recursive method that checks whether all the proper members can
/// derive debug or not, because of the limit rust has on 32 items as max in the
/// array.
pub trait CanDeriveDebug {
    /// Return `true` if `Debug` can be derived for this thing, `false`
    /// otherwise.
    fn can_derive_debug(&self, ctx: &BindgenContext) -> bool;
}

/// A trait that encapsulates the logic for whether or not we can derive `Debug`.
/// The difference between this trait and the CanDeriveDebug is that the type
/// implementing this trait cannot use recursion or lookup result from fix point
/// analysis. It's a helper trait for fix point analysis.
pub trait CanTriviallyDeriveDebug {
    /// Return `true` if `Debug` can be derived for this thing, `false`
    /// otherwise.
    fn can_trivially_derive_debug(&self) -> bool;
}

/// A trait that encapsulates the logic for whether or not we can derive `Copy`
/// for a given thing.
pub trait CanDeriveCopy<'a> {
    /// Return `true` if `Copy` can be derived for this thing, `false`
    /// otherwise.
    fn can_derive_copy(&'a self, ctx: &'a BindgenContext) -> bool;
}

/// A trait that encapsulates the logic for whether or not we can derive `Copy`.
/// The difference between this trait and the CanDeriveCopy is that the type
/// implementing this trait cannot use recursion or lookup result from fix point
/// analysis. It's a helper trait for fix point analysis.
pub trait CanTriviallyDeriveCopy {
    /// Return `true` if `Copy` can be derived for this thing, `false`
    /// otherwise.
    fn can_trivially_derive_copy(&self) -> bool;
}

/// A trait that encapsulates the logic for whether or not we can derive `Default`
/// for a given thing.
///
/// This should ideally be a no-op that just returns `true`, but instead needs
/// to be a recursive method that checks whether all the proper members can
/// derive default or not, because of the limit rust has on 32 items as max in the
/// array.
pub trait CanDeriveDefault {
    /// Return `true` if `Default` can be derived for this thing, `false`
    /// otherwise.
    fn can_derive_default(&self, ctx: &BindgenContext) -> bool;
}

/// A trait that encapsulates the logic for whether or not we can derive `Default`.
/// The difference between this trait and the CanDeriveDefault is that the type
/// implementing this trait cannot use recursion or lookup result from fix point
/// analysis. It's a helper trait for fix point analysis.
pub trait CanTriviallyDeriveDefault {
    /// Return `true` if `Default` can be derived for this thing, `false`
    /// otherwise.
    fn can_trivially_derive_default(&self) -> bool;
}

/// A trait that encapsulates the logic for whether or not we can derive `Hash`
/// for a given thing.
///
/// This should ideally be a no-op that just returns `true`, but instead needs
/// to be a recursive method that checks whether all the proper members can
/// derive default or not, because of the limit rust has on 32 items as max in the
/// array.
pub trait CanDeriveHash {
    /// Return `true` if `Default` can be derived for this thing, `false`
    /// otherwise.
    fn can_derive_hash(&self, ctx: &BindgenContext) -> bool;
}

/// A trait that encapsulates the logic for whether or not we can derive `PartialEq`
/// for a given thing.
///
/// This should ideally be a no-op that just returns `true`, but instead needs
/// to be a recursive method that checks whether all the proper members can
/// derive default or not, because of the limit rust has on 32 items as max in the
/// array.
pub trait CanDerivePartialEq {
    /// Return `true` if `Default` can be derived for this thing, `false`
    /// otherwise.
    fn can_derive_partialeq(&self, ctx: &BindgenContext) -> bool;
}

/// A trait that encapsulates the logic for whether or not we can derive `Eq`
/// for a given thing.
///
/// This should ideally be a no-op that just returns `true`, but instead needs
/// to be a recursive method that checks whether all the proper members can
/// derive eq or not, because of the limit rust has on 32 items as max in the
/// array.
pub trait CanDeriveEq {

    /// Return `true` if `Eq` can be derived for this thing, `false`
    /// otherwise.
    fn can_derive_eq(&self,
                     ctx: &BindgenContext)
                     -> bool;
}

/// A trait that encapsulates the logic for whether or not we can derive `Hash`.
/// The difference between this trait and the CanDeriveHash is that the type
/// implementing this trait cannot use recursion or lookup result from fix point
/// analysis. It's a helper trait for fix point analysis.
pub trait CanTriviallyDeriveHash {
    /// Return `true` if `Hash` can be derived for this thing, `false`
    /// otherwise.
    fn can_trivially_derive_hash(&self) -> bool;
}

/// A trait that encapsulates the logic for whether or not we can derive `PartialEq`.
/// The difference between this trait and the CanDerivePartialEq is that the type
/// implementing this trait cannot use recursion or lookup result from fix point
/// analysis. It's a helper trait for fix point analysis.
pub trait CanTriviallyDerivePartialEq {
    /// Return `true` if `PartialEq` can be derived for this thing, `false`
    /// otherwise.
    fn can_trivially_derive_partialeq(&self) -> bool;
}
