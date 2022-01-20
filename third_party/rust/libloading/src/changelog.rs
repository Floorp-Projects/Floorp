//! The change log.

/// Release 0.7.0 (2021-01-31)
///
/// ## Breaking changes
///
/// ### Loading functions are now `unsafe`
///
/// A number of associated methods involved in loading a library were changed to
/// be `unsafe`. The affected functions are: [`Library::new`], [`os::unix::Library::new`],
/// [`os::unix::Library::open`], [`os::windows::Library::new`],
/// [`os::windows::Library::load_with_flags`]. This is the most prominent breaking change in this
/// release and affects majority of the users of `libloading`.
///
/// In order to see why it was necessary, consider the following snippet of C++ code:
///
/// ```c++
/// #include <vector>
/// #include <iostream>
///
/// static std::vector<unsigned int> UNSHUU = { 1, 2, 3 };
///
/// int main() {
///     std::cout << UNSHUU[0] << UNSHUU[1] << UNSHUU[2] << std::endl; // Prints 123
///     return 0;
/// }
/// ```
///
/// The `std::vector` type, much like in Rust's `Vec`, stores its contents in a buffer allocated on
/// the heap. In this example the vector object itself is stored and initialized as a static
/// variable – a compile time construct. The heap, on the other hand, is a runtime construct. And
/// yet the code works exactly as you'd expect – the vector contains numbers 1, 2 and 3 stored in
/// a buffer on heap. So, _what_ makes it work out, exactly?
///
/// Various executable and shared library formats define conventions and machinery to execute
/// arbitrary code when a program or a shared library is loaded. On systems using the PE format
/// (e.g. Windows) this is available via the optional `DllMain` initializer. Various systems
/// utilizing the ELF format take a sightly different approach of maintaining an array of function
/// pointers in the `.init_array` section. A very similar mechanism exists on systems that utilize
/// the Mach-O format.
///
/// For the C++ program above, the object stored in the `UNSHUU` global variable is constructed
/// by code run as part of such an initializer routine. This initializer is run before the entry
/// point (the `main` function) is executed, allowing for this magical behaviour to be possible.
/// Were the C++ code built as a shared library instead, the initialization routines would run as
/// the resulting shared library is loaded. In case of `libloading` – during the call to
/// `Library::new` and other methods affected by this change.
///
/// These initialization (and very closely related termination) routines can be utilized outside of
/// C++ too. Anybody can build a shared library in variety of different programming languages and
/// set up the initializers to execute arbitrary code. Potentially code that does all sorts of
/// wildly unsound stuff.
///
/// The routines are executed by components that are an integral part of the operating system.
/// Changing or controlling the operation of these components is infeasible. With that in
/// mind, the initializer and termination routines are something anybody loading a library must
/// carefully evaluate the libraries loaded for soundness.
///
/// In practice, a vast majority of the libraries can be considered a good citizen and their
/// initialization and termination routines, if they have any at all, can be trusted to be sound.
///
/// Also see: [issue #86].
///
/// ### Better & more consistent default behaviour on UNIX systems
///
/// On UNIX systems the [`Library::new`], [`os::unix::Library::new`] and
/// [`os::unix::Library::this`] methods have been changed to use
/// <code>[RTLD_LAZY] | [RTLD_LOCAL]</code> as the default set of loader options (previously:
/// [`RTLD_NOW`]). This has a couple benefits. Namely:
///
/// * Lazy binding is generally quicker to execute when only a subset of symbols from a library are
///   used and is typically the default when neither `RTLD_LAZY` nor `RTLD_NOW` are specified when
///   calling the underlying `dlopen` API;
/// * On most UNIX systems (macOS being a notable exception) `RTLD_LOCAL` is the default when
///   neither `RTLD_LOCAL` nor [`RTLD_GLOBAL`] are specified. The explicit setting of the
///   `RTLD_LOCAL` flag makes this behaviour consistent across platforms.
///
/// ### Dropped support for Windows XP/Vista
///
/// The (broken) support for Windows XP and Windows Vista environments was removed. This was
/// prompted primarily by a similar policy change in the [Rust
/// project](https://github.com/rust-lang/compiler-team/issues/378) but also as an acknowledgement
/// to the fact that `libloading` never worked in these environments anyway.
///
/// ### More accurate error variant names
///
/// Finally, the `Error::LoadLibraryW` renamed to [`Error::LoadLibraryExW`] to more accurately
/// represent the underlying API that's failing. No functional changes as part of this rename
/// intended.
///
/// [issue #86]: https://github.com/nagisa/rust_libloading/issues/86
/// [`Library::new`]: crate::Library::new
/// [`Error::LoadLibraryExW`]: crate::Error::LoadLibraryExW
/// [`os::unix::Library::this`]: crate::os::unix::Library::this
/// [`os::unix::Library::new`]: crate::os::unix::Library::new
/// [`os::unix::Library::open`]: crate::os::unix::Library::new
/// [`os::windows::Library::new`]: crate::os::windows::Library::new
/// [`os::windows::Library::load_with_flags`]: crate::os::windows::Library::load_with_flags
/// [`RTLD_NOW`]: crate::os::unix::RTLD_NOW
/// [RTLD_LAZY]: crate::os::unix::RTLD_LAZY
/// [RTLD_LOCAL]: crate::os::unix::RTLD_LOCAL
/// [`RTLD_GLOBAL`]: crate::os::unix::RTLD_GLOBAL
pub mod r0_7_0 {}

/// Release 0.6.7 (2021-01-14)
///
/// * Added a [`os::windows::Library::open_already_loaded`] to obtain a handle to a library that
/// must already be loaded. There is no portable equivalent for all UNIX targets. Users who do not
/// care about portability across UNIX platforms may use [`os::unix::Library::open`] with
/// `libc::RTLD_NOLOAD`;
///
/// [`os::windows::Library::open_already_loaded`]: crate::os::windows::Library::open_already_loaded
/// [`os::unix::Library::open`]: crate::os::unix::Library::open
pub mod r0_6_7 {}

/// Release 0.6.6 (2020-12-03)
///
/// * Fix a double-release of resources when [`Library::close`] or [`os::windows::Library::close`]
///   is used on Windows.
///
/// [`Library::close`]: crate::Library::close
/// [`os::windows::Library::close`]: crate::os::windows::Library::close
pub mod r0_6_6 {}

/// Release 0.6.5 (2020-10-23)
///
/// * Upgrade cfg-if 0.1 to 1.0
pub mod r0_6_5 {}

/// Release 0.6.4 (2020-10-10)
///
/// * Remove use of `build.rs` making it easier to build `libloading` without cargo. It also
///   almost halves the build time of this crate.
pub mod r0_6_4 {}

/// Release 0.6.3 (2020-08-22)
///
/// * Improve documentation, allowing to view all of the os-specific functionality from
/// documentation generated for any target;
/// * Add [`os::windows::Library::this`];
/// * Added constants to use with OS-specific `Library::open`;
/// * Add [`library_filename`].
///
/// [`os::windows::Library::this`]: crate::os::windows::Library::this
/// [`library_filename`]: crate::library_filename
pub mod r0_6_3 {}

/// Release 0.6.2 (2020-05-06)
///
/// * Fixed building of this library on Illumos.
pub mod r0_6_2 {}

/// Release 0.6.1 (2020-04-15)
///
/// * Introduced a new method [`os::windows::Library::load_with_flags`];
/// * Added support for the Illumos triple.
///
/// [`os::windows::Library::load_with_flags`]: crate::os::windows::Library::load_with_flags
pub mod r0_6_1 {}

/// Release 0.6.0 (2020-04-05)
///
/// * Introduced a new method [`os::unix::Library::get_singlethreaded`];
/// * Added (untested) support for building when targetting Redox and Fuchsia;
/// * The APIs exposed by this library no longer panic and instead return an `Err` when it used
///   to panic.
///
/// ## Breaking changes
///
/// * Minimum required (stable) version of Rust to build this library is now 1.40.0;
/// * This crate now implements a custom [`Error`] type and all APIs now return this type rather
///   than returning the `std::io::Error`;
/// * `libloading::Result` has been removed;
/// * Removed the dependency on the C compiler to build this library on UNIX-like platforms.
///   `libloading` used to utilize a snippet written in C to work-around the unlikely possibility
///   of the target having a thread-unsafe implementation of the `dlerror` function. The effect of
///   the work-around was very opportunistic: it would not work if the function was called by
///   forgoing `libloading`.
///
///   Starting with 0.6.0, [`Library::get`] on platforms where `dlerror` is not MT-safe (such as
///   FreeBSD, DragonflyBSD or NetBSD) will unconditionally return an error when the underlying
///   `dlsym` returns a null pointer. For the use-cases where loading null pointers is necessary
///   consider using [`os::unix::Library::get_singlethreaded`] instead.
///
/// [`Library::get`]: crate::Library::get
/// [`os::unix::Library::get_singlethreaded`]: crate::os::unix::Library::get_singlethreaded
/// [`Error`]: crate::Error
pub mod r0_6_0 {}


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
