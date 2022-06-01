#![warn(nonstandard_style, rust_2018_idioms, unused)]
// Note: This does not guarantee compatibility with forbidding these lints in the future.
// If rustc adds a new lint, we may not be able to keep this.
#![forbid(future_incompatible, rust_2018_compatibility)]
#![allow(unknown_lints)] // for old compilers
#![warn(
    box_pointers,
    deprecated_in_future,
    elided_lifetimes_in_paths,
    explicit_outlives_requirements,
    macro_use_extern_crate,
    meta_variable_misuse,
    missing_copy_implementations,
    missing_debug_implementations,
    missing_docs,
    non_ascii_idents,
    single_use_lifetimes,
    trivial_casts,
    trivial_numeric_casts,
    unreachable_pub,
    unused_extern_crates,
    unused_import_braces,
    unused_lifetimes,
    unused_qualifications,
    unused_results,
    variant_size_differences
)]
// absolute_paths_not_starting_with_crate, anonymous_parameters, keyword_idents, pointer_structural_match: forbidden as a part of future_incompatible
// unsafe_block_in_unsafe_fn: unstable: https://github.com/rust-lang/rust/issues/71668
// unsafe_code: checked in forbid_unsafe module
// unstable_features: deprecated: https://doc.rust-lang.org/beta/rustc/lints/listing/allowed-by-default.html#unstable-features
// unused_crate_dependencies: unrelated
#![warn(clippy::all, clippy::pedantic, clippy::nursery)]

// Check interoperability with rustc and clippy lints.

pub mod basic {
    include!("include/basic.rs");
}

pub mod forbid_unsafe {
    #![forbid(unsafe_code)]

    include!("include/basic-safe-part.rs");
}

pub mod clippy {
    use pin_project::pin_project;

    #[rustversion::attr(before(1.37), allow(single_use_lifetimes))] // https://github.com/rust-lang/rust/issues/53738
    #[pin_project(project_replace)]
    #[derive(Debug)]
    pub struct MutMutStruct<'a, T, U> {
        #[pin]
        pub pinned: &'a mut T,
        pub unpinned: &'a mut U,
    }

    #[rustversion::attr(before(1.37), allow(single_use_lifetimes))] // https://github.com/rust-lang/rust/issues/53738
    #[pin_project(project_replace)]
    #[derive(Debug)]
    pub struct MutMutTupleStruct<'a, T, U>(#[pin] &'a mut T, &'a mut U);

    #[rustversion::attr(before(1.37), allow(single_use_lifetimes))] // https://github.com/rust-lang/rust/issues/53738
    #[pin_project(project_replace)]
    #[derive(Debug)]
    pub enum MutMutEnum<'a, T, U> {
        Struct {
            #[pin]
            pinned: &'a mut T,
            unpinned: &'a mut U,
        },
        Tuple(#[pin] &'a mut T, &'a mut U),
        Unit,
    }

    #[pin_project(project_replace)]
    #[derive(Debug)]
    pub struct TypeRepetitionInBoundsStruct<T, U>
    where
        Self: Sized,
    {
        #[pin]
        pub pinned: T,
        pub unpinned: U,
    }

    #[pin_project(project_replace)]
    #[derive(Debug)]
    pub struct TypeRepetitionInBoundsTupleStruct<T, U>(#[pin] T, U)
    where
        Self: Sized;

    #[pin_project(project_replace)]
    #[derive(Debug)]
    pub enum TypeRepetitionInBoundsEnum<T, U>
    where
        Self: Sized,
    {
        Struct {
            #[pin]
            pinned: T,
            unpinned: U,
        },
        Tuple(#[pin] T, U),
        Unit,
    }

    #[pin_project(project_replace)]
    #[derive(Debug)]
    pub struct UsedUnderscoreBindingStruct<T, U> {
        #[pin]
        pub _pinned: T,
        pub _unpinned: U,
    }

    #[pin_project(project_replace)]
    #[derive(Debug)]
    pub enum UsedUnderscoreBindingEnum<T, U> {
        Struct {
            #[pin]
            _pinned: T,
            _unpinned: U,
        },
    }
}
