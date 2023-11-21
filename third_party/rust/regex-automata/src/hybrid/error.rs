use crate::{hybrid::id::LazyStateIDError, nfa};

/// An error that occurs when initial construction of a lazy DFA fails.
///
/// A build error can occur when insufficient cache capacity is configured or
/// if something about the NFA is unsupported. (For example, if one attempts
/// to build a lazy DFA without heuristic Unicode support but with an NFA that
/// contains a Unicode word boundary.)
///
/// This error does not provide many introspection capabilities. There are
/// generally only two things you can do with it:
///
/// * Obtain a human readable message via its `std::fmt::Display` impl.
/// * Access an underlying
/// [`nfa::thompson::BuildError`](crate::nfa::thompson::BuildError)
/// type from its `source` method via the `std::error::Error` trait. This error
/// only occurs when using convenience routines for building a lazy DFA
/// directly from a pattern string.
///
/// When the `std` feature is enabled, this implements the `std::error::Error`
/// trait.
#[derive(Clone, Debug)]
pub struct BuildError {
    kind: BuildErrorKind,
}

#[derive(Clone, Debug)]
enum BuildErrorKind {
    NFA(nfa::thompson::BuildError),
    InsufficientCacheCapacity { minimum: usize, given: usize },
    InsufficientStateIDCapacity { err: LazyStateIDError },
    Unsupported(&'static str),
}

impl BuildError {
    pub(crate) fn nfa(err: nfa::thompson::BuildError) -> BuildError {
        BuildError { kind: BuildErrorKind::NFA(err) }
    }

    pub(crate) fn insufficient_cache_capacity(
        minimum: usize,
        given: usize,
    ) -> BuildError {
        BuildError {
            kind: BuildErrorKind::InsufficientCacheCapacity { minimum, given },
        }
    }

    pub(crate) fn insufficient_state_id_capacity(
        err: LazyStateIDError,
    ) -> BuildError {
        BuildError {
            kind: BuildErrorKind::InsufficientStateIDCapacity { err },
        }
    }

    pub(crate) fn unsupported_dfa_word_boundary_unicode() -> BuildError {
        let msg = "cannot build lazy DFAs for regexes with Unicode word \
                   boundaries; switch to ASCII word boundaries, or \
                   heuristically enable Unicode word boundaries or use a \
                   different regex engine";
        BuildError { kind: BuildErrorKind::Unsupported(msg) }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for BuildError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self.kind {
            BuildErrorKind::NFA(ref err) => Some(err),
            _ => None,
        }
    }
}

impl core::fmt::Display for BuildError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self.kind {
            BuildErrorKind::NFA(_) => write!(f, "error building NFA"),
            BuildErrorKind::InsufficientCacheCapacity { minimum, given } => {
                write!(
                    f,
                    "given cache capacity ({}) is smaller than \
                     minimum required ({})",
                    given, minimum,
                )
            }
            BuildErrorKind::InsufficientStateIDCapacity { ref err } => {
                err.fmt(f)
            }
            BuildErrorKind::Unsupported(ref msg) => {
                write!(f, "unsupported regex feature for DFAs: {}", msg)
            }
        }
    }
}

/// An error that occurs when cache usage has become inefficient.
///
/// One of the weaknesses of a lazy DFA is that it may need to clear its
/// cache repeatedly if it's not big enough. If this happens too much, then it
/// can slow searching down significantly. A mitigation to this is to use
/// heuristics to detect whether the cache is being used efficiently or not.
/// If not, then a lazy DFA can return a `CacheError`.
///
/// The default configuration of a lazy DFA in this crate is
/// set such that a `CacheError` will never occur. Instead,
/// callers must opt into this behavior with settings like
/// [`dfa::Config::minimum_cache_clear_count`](crate::hybrid::dfa::Config::minimum_cache_clear_count)
/// and
/// [`dfa::Config::minimum_bytes_per_state`](crate::hybrid::dfa::Config::minimum_bytes_per_state).
///
/// When the `std` feature is enabled, this implements the `std::error::Error`
/// trait.
#[derive(Clone, Debug)]
pub struct CacheError(());

impl CacheError {
    pub(crate) fn too_many_cache_clears() -> CacheError {
        CacheError(())
    }

    pub(crate) fn bad_efficiency() -> CacheError {
        CacheError(())
    }
}

#[cfg(feature = "std")]
impl std::error::Error for CacheError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        None
    }
}

impl core::fmt::Display for CacheError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "lazy DFA cache has been cleared too many times")
    }
}
