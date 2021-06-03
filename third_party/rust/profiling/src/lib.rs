//
// To use this library, enable one of the feature flags. Each backend implementation provides the
// exact same interface. Only one may be active at a time.
//

/// Proc macro for creating a scope around the function, using the name of the function for the
/// scope's name
///
/// This must be done as a proc macro because tracing requires a const string
///
/// ```
/// #[profiling::function]
/// fn my_function() {
///
/// }
/// ```
#[cfg(feature = "procmacros")]
pub use profiling_procmacros::function;

#[cfg(feature = "profile-with-puffin")]
pub use puffin;
#[cfg(feature = "profile-with-puffin")]
mod puffin_impl;
#[cfg(feature = "profile-with-puffin")]
pub use puffin_impl::*;

#[cfg(feature = "profile-with-optick")]
pub use optick;
#[cfg(feature = "profile-with-optick")]
mod optick_impl;
#[cfg(feature = "profile-with-optick")]
pub use optick_impl::*;

#[cfg(feature = "profile-with-superluminal")]
pub use superluminal_perf;
#[cfg(feature = "profile-with-superluminal")]
mod superluminal_impl;
#[cfg(feature = "profile-with-superluminal")]
pub use superluminal_impl::*;

#[cfg(feature = "profile-with-tracing")]
pub use tracing;
#[cfg(feature = "profile-with-tracing")]
mod tracing_impl;
#[cfg(feature = "profile-with-tracing")]
pub use tracing_impl::*;

#[cfg(feature = "profile-with-tracy")]
pub use tracy_client;
#[cfg(feature = "profile-with-tracy")]
mod tracy_impl;
#[cfg(feature = "profile-with-tracy")]
pub use tracy_impl::*;

#[cfg(feature = "type-check")]
mod type_check_impl;
#[cfg(feature = "type-check")]
pub use type_check_impl::*;

#[cfg(not(any(
    feature = "profile-with-puffin",
    feature = "profile-with-optick",
    feature = "profile-with-superluminal",
    feature = "profile-with-tracing",
    feature = "profile-with-tracy",
    feature = "type-check"
)))]
mod empty_impl;

#[cfg(not(any(
    feature = "profile-with-puffin",
    feature = "profile-with-optick",
    feature = "profile-with-superluminal",
    feature = "profile-with-tracing",
    feature = "profile-with-tracy",
    feature = "type-check"
)))]
pub use empty_impl::*;
