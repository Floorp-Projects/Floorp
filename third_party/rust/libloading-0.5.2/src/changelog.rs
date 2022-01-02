//! Project changelog

/// Release 0.5.2 (2019-07-07)
///
/// * Added API to convert OS-specific `Library` and `Symbol` conversion to underlying resources.
pub mod r0_5_2 {}

/// Release 0.5.1 (2019-06-01)
///
/// * Build on Haiku targets.
pub mod r0_5_1 {}

/// Release 0.5.0 (2018-01-11)
///
/// * Update to `winapi = ^0.3`;
///
/// ## Breaking changes
///
/// * libloading now requires a C compiler to build on UNIX;
///   * This is a temporary measure until the [`linkage`] attribute is stabilised;
///   * Necessary to resolve [#32].
///
/// [`linkage`]: https://github.com/rust-lang/rust/issues/29603
/// [#32]: https://github.com/nagisa/rust_libloading/issues/32
pub mod r0_5_0 {}

/// Release 0.4.3 (2017-12-07)
///
/// * Bump lazy-static dependency to `^1.0`;
/// * `cargo test --release` now works when testing libloading.
pub mod r0_4_3 {}


/// Release 0.4.2 (2017-09-24)
///
/// * Improved error and race-condition handling on Windows;
/// * Improved documentation about thread-safety of Library;
/// * Added `Symbol::<Option<T>::lift_option() -> Option<Symbol<T>>` convenience method.
pub mod r0_4_2 {}


/// Release 0.4.1 (2017-08-29)
///
/// * Solaris support
pub mod r0_4_1 {}

/// Release 0.4.0 (2017-05-01)
///
/// * Remove build-time dependency on target_build_utils (and by extension serde/phf);
/// * Require at least version 1.14.0 of rustc to build;
///   * Actually, it is cargo which has to be more recent here. The one shipped with rustc 1.14.0
///     is what’s being required from now on.
pub mod r0_4_0 {}

/// Release 0.3.4 (2017-03-25)
///
/// * Remove rogue println!
pub mod r0_3_4 {}

/// Release 0.3.3 (2017-03-25)
///
/// * Panics when `Library::get` is called for incompatibly sized type such as named function
///   types (which are zero-sized).
pub mod r0_3_3 {}

/// Release 0.3.2 (2017-02-10)
///
/// * Minimum version required is now rustc 1.12.0;
/// * Updated dependency versions (most notably target_build_utils to 0.3.0)
pub mod r0_3_2 {}

/// Release 0.3.1 (2016-10-01)
///
/// * `Symbol<T>` and `os::*::Symbol<T>` now implement `Send` where `T: Send`;
/// * `Symbol<T>` and `os::*::Symbol<T>` now implement `Sync` where `T: Sync`;
/// * `Library` and `os::*::Library` now implement `Sync` (they were `Send` in 0.3.0 already).
pub mod r0_3_1 {}

/// Release 0.3.0 (2016-07-27)
///
/// * Greatly improved documentation, especially around platform-specific behaviours;
/// * Improved test suite by building our own library to test against;
/// * All `Library`-ies now implement `Send`.
/// * Added `impl From<os::platform::Library> for Library` and `impl From<Library> for
/// os::platform::Library` allowing wrapping and extracting the platform-specific library handle;
/// * Added methods to wrap (`Symbol::from_raw`) and unwrap (`Symbol::into_raw`) the safe `Symbol`
/// wrapper into unsafe `os::platform::Symbol`.
///
/// The last two additions focus on not restricting potential usecases of this library, allowing
/// users of the library to circumvent safety checks if need be.
///
/// ## Breaking Changes
///
/// `Library::new` defaults to `RTLD_NOW` instead of `RTLD_LAZY` on UNIX for more consistent
/// cross-platform behaviour. If a library loaded with `Library::new` had any linking errors, but
/// unresolved references weren’t forced to be resolved, the library would’ve “just worked”,
/// whereas now the call to `Library::new` will return an error signifying presence of such error.
///
/// ## os::platform
/// * Added `os::unix::Library::open` which allows specifying arbitrary flags (e.g. `RTLD_LAZY`);
/// * Added `os::windows::Library::get_ordinal` which allows finding a function or variable by its
/// ordinal number;
pub mod r0_3_0 {}
