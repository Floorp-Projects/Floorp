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
    /// Implementations can define this type to get access to any extra
    /// information required to determine whether they can derive `Debug`. If
    /// extra information is unneeded, then this should simply be the unit type.
    type Extra;

    /// Return `true` if `Debug` can be derived for this thing, `false`
    /// otherwise.
    fn can_derive_debug(&self,
                        ctx: &BindgenContext,
                        extra: Self::Extra)
                        -> bool;
}

/// A trait that encapsulates the logic for whether or not we can derive `Copy`
/// for a given thing.
pub trait CanDeriveCopy<'a> {
    /// Implementations can define this type to get access to any extra
    /// information required to determine whether they can derive `Copy`. If
    /// extra information is unneeded, then this should simply be the unit type.
    type Extra;

    /// Return `true` if `Copy` can be derived for this thing, `false`
    /// otherwise.
    fn can_derive_copy(&'a self,
                       ctx: &'a BindgenContext,
                       extra: Self::Extra)
                       -> bool;

    /// For some reason, deriving copies of an array of a type that is not known
    /// to be `Copy` is a compile error. e.g.:
    ///
    /// ```rust
    /// #[derive(Copy, Clone)]
    /// struct A<T> {
    ///     member: T,
    /// }
    /// ```
    ///
    /// is fine, while:
    ///
    /// ```rust,ignore
    /// #[derive(Copy, Clone)]
    /// struct A<T> {
    ///     member: [T; 1],
    /// }
    /// ```
    ///
    /// is an error.
    ///
    /// That's the whole point of the existence of `can_derive_copy_in_array`.
    fn can_derive_copy_in_array(&'a self,
                                ctx: &'a BindgenContext,
                                extra: Self::Extra)
                                -> bool;
}

/// A trait that encapsulates the logic for whether or not we can derive `Default`
/// for a given thing.
///
/// This should ideally be a no-op that just returns `true`, but instead needs
/// to be a recursive method that checks whether all the proper members can
/// derive default or not, because of the limit rust has on 32 items as max in the
/// array.
pub trait CanDeriveDefault {
    /// Implementations can define this type to get access to any extra
    /// information required to determine whether they can derive `Default`. If
    /// extra information is unneeded, then this should simply be the unit type.
    type Extra;

    /// Return `true` if `Default` can be derived for this thing, `false`
    /// otherwise.
    fn can_derive_default(&self,
                          ctx: &BindgenContext,
                          extra: Self::Extra)
                          -> bool;
}
