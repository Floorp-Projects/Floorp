//! This module contains types that represent cookie properties that are not yet
//! standardized. That is, _draft_ features.

use std::fmt;

/// The `SameSite` cookie attribute.
///
/// A cookie with a `SameSite` attribute is imposed restrictions on when it is
/// sent to the origin server in a cross-site request. If the `SameSite`
/// attribute is "Strict", then the cookie is never sent in cross-site requests.
/// If the `SameSite` attribute is "Lax", the cookie is only sent in cross-site
/// requests with "safe" HTTP methods, i.e, `GET`, `HEAD`, `OPTIONS`, `TRACE`.
///
/// **Note:** This cookie attribute is an HTTP draft! Its meaning and definition
/// are subject to change.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum SameSite {
    /// The "Strict" `SameSite` attribute.
    Strict,
    /// The "Lax" `SameSite` attribute.
    Lax
}

impl SameSite {
    /// Returns `true` if `self` is `SameSite::Strict` and `false` otherwise.
    ///
    /// # Example
    ///
    /// ```rust
    /// use cookie::SameSite;
    ///
    /// let strict = SameSite::Strict;
    /// assert!(strict.is_strict());
    /// assert!(!strict.is_lax());
    /// ```
    #[inline]
    pub fn is_strict(&self) -> bool {
        match *self {
            SameSite::Strict => true,
            SameSite::Lax => false
        }
    }

    /// Returns `true` if `self` is `SameSite::Lax` and `false` otherwise.
    ///
    /// # Example
    ///
    /// ```rust
    /// use cookie::SameSite;
    ///
    /// let lax = SameSite::Lax;
    /// assert!(lax.is_lax());
    /// assert!(!lax.is_strict());
    /// ```
    #[inline]
    pub fn is_lax(&self) -> bool {
        match *self {
            SameSite::Strict => false,
            SameSite::Lax => true
        }
    }
}

impl fmt::Display for SameSite {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            SameSite::Strict => write!(f, "Strict"),
            SameSite::Lax => write!(f, "Lax")
        }
    }
}
