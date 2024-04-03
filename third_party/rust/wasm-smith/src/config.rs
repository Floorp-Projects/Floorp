//! Configuring the shape of generated Wasm modules.

use crate::InstructionKinds;
use arbitrary::{Arbitrary, Result, Unstructured};

macro_rules! define_config {
    (
        $(#[$attr:meta])*
        pub struct Config {
            $(
                $(#[$field_attr:meta])*
                pub $field:ident : $field_ty:ty = $default:expr,
            )*
        }
    ) => {
        $(#[$attr])*
        pub struct Config {
            /// The imports that may be used when generating the module.
            ///
            /// Defaults to `None` which means that any arbitrary import can be
            /// generated.
            ///
            /// To only allow specific imports, set this field to a WebAssembly
            /// module which describes the imports allowed.
            ///
            /// Note that [`Self::min_imports`] is ignored when
            /// `available_imports` are enabled.
            ///
            /// The provided value must be a valid binary encoding of a
            /// WebAssembly module. `wasm-smith` will panic if the module cannot
            /// be parsed.
            ///
            /// # Example
            ///
            /// An implementation of this method could use the `wat` crate to
            /// provide a human-readable and maintainable description:
            ///
            /// ```rust
            /// Some(wat::parse_str(r#"
            ///     (module
            ///         (import "env" "ping" (func (param i32)))
            ///         (import "env" "pong" (func (result i32)))
            ///         (import "env" "memory" (memory 1))
            ///         (import "env" "table" (table 1))
            ///         (import "env" "tag" (tag (param i32)))
            ///     )
            /// "#))
            /// # ;
            /// ```
            pub available_imports: Option<Vec<u8>>,

            /// If provided, the generated module will have exports with exactly
            /// the same names and types as those in the provided WebAssembly
            /// module. The implementation (e.g. function bodies, global
            /// initializers) of each export in the generated module will be
            /// random and unrelated to the implementation in the provided
            /// module. Only globals and functions are supported.
            ///
            ///
            /// Defaults to `None` which means arbitrary exports will be
            /// generated.
            ///
            /// To specify which exports the generated modules should have, set
            /// this field to a WebAssembly module which describes the desired
            /// exports. To generate modules with varying exports that meet some
            /// constraints, consider randomly generating the value for this
            /// field.
            ///
            /// The provided value must be a valid binary encoding of a
            /// WebAssembly module. `wasm-smith` will panic if the module cannot
            /// be parsed.
            ///
            /// # Module Limits
            ///
            /// All types, functions, globals, and exports that are needed to
            /// provide the required exports will be generated, even if it
            /// causes the resulting module to exceed the limits defined in
            /// [`Self::max_type_size`], [`Self::max_types`],
            /// [`Self::max_funcs`], [`Self::max_globals`], or
            /// [`Self::max_exports`].
            ///
            /// # Example
            ///
            /// As for [`Self::available_imports`], the `wat` crate can be used
            /// to provide an human-readable description of the desired exports:
            ///
            /// ```rust
            /// Some(wat::parse_str(r#"
            ///     (module
            ///         (func (export "foo") (param i32) (result i64) unreachable)
            ///         (global (export "bar") f32 f32.const 0)
            ///     )
            /// "#));
            /// ```
            pub exports: Option<Vec<u8>>,

            $(
                $(#[$field_attr])*
                pub $field: $field_ty,
            )*
        }

        impl Default for Config {
            fn default() -> Config {
                Config {
                    available_imports: None,
                    exports: None,

                    $(
                        $field: $default,
                    )*
                }
            }
        }

        #[cfg(feature = "_internal_cli")]
        #[doc(hidden)]
        #[derive(Clone, Debug, Default, clap::Parser, serde_derive::Deserialize)]
        #[serde(rename_all = "kebab-case")]
        pub struct InternalOptionalConfig {
            /// The imports that may be used when generating the module.
            ///
            /// When unspecified, any arbitrary import can be generated.
            ///
            /// To only allow specific imports, provide a file path of a
            /// WebAssembly module which describes the imports allowed.
            ///
            /// Note that [`Self::min_imports`] is ignored when
            /// `available_imports` are enabled.
            ///
            /// The provided value must be a valid binary encoding of a
            /// WebAssembly module. `wasm-smith` will panic if the module cannot
            /// be parsed.
            #[cfg_attr(feature = "clap", clap(long))]
            available_imports: Option<std::path::PathBuf>,

            /// If provided, the generated module will have exports with exactly
            /// the same names and types as those in the provided WebAssembly
            /// module. The implementation (e.g. function bodies, global
            /// initializers) of each export in the generated module will be
            /// random and unrelated to the implementation in the provided
            /// module. Only globals and functions are supported.
            ///
            /// Defaults to `None` which means arbitrary exports will be
            /// generated.
            ///
            /// To specify which exports the generated modules should have, set
            /// this field to a WebAssembly module which describes the desired
            /// exports. To generate modules with varying exports that meet some
            /// constraints, consider randomly generating the value for this
            /// field.
            ///
            /// The provided value must be a valid binary encoding of a
            /// WebAssembly module. `wasm-smith` will panic if the module cannot
            /// be parsed.
            ///
            /// # Module Limits
            ///
            /// All types, functions, globals, and exports that are needed to
            /// provide the required exports will be generated, even if it
            /// causes the resulting module to exceed the limits defined in
            /// [`Self::max_type_size`], [`Self::max_types`],
            /// [`Self::max_funcs`], [`Self::max_globals`], or
            /// [`Self::max_exports`].
            ///
            #[cfg_attr(feature = "clap", clap(long))]
            exports: Option<std::path::PathBuf>,

            $(
                $(#[$field_attr])*
                #[cfg_attr(feature = "clap", clap(long))]
                pub $field: Option<$field_ty>,
            )*
        }

        #[cfg(feature = "_internal_cli")]
        impl InternalOptionalConfig {
            pub fn or(self, other: Self) -> Self {
                Self {
                    available_imports: self.available_imports.or(other.available_imports),
                    exports: self.exports.or(other.exports),

                    $(
                        $field: self.$field.or(other.$field),
                    )*
                }
            }
        }

        #[cfg(feature = "_internal_cli")]
        impl TryFrom<InternalOptionalConfig> for Config {
            type Error = anyhow::Error;
            fn try_from(config: InternalOptionalConfig) -> anyhow::Result<Config> {
                let default = Config::default();
                Ok(Config {
                    available_imports: if let Some(file) = config
                        .available_imports
                        .as_ref() {
                            Some(wat::parse_file(file)?)
                        } else {
                            None
                        },
                    exports: if let Some(file) = config
                        .exports
                        .as_ref() {
                            Some(wat::parse_file(file)?)
                        } else {
                            None
                        },

                    $(
                        $field: config.$field.unwrap_or(default.$field),
                    )*
                })
            }
        }
    }
}

define_config! {
    /// Configuration for a generated module.
    ///
    /// Don't care to configure your generated modules? Just use
    /// [`Module::arbitrary`][crate::Module], which internally uses the default
    /// configuration.
    ///
    /// Want control over the shape of the module that gets generated? Create a
    /// `Config` and then pass it to [`Module::new`][crate::Module::new].
    ///
    /// # Swarm Testing
    ///
    /// You can use the `Arbitrary for Config` implementation for [swarm
    /// testing]. This will dynamically -- but still deterministically -- choose
    /// configuration options for you.
    ///
    /// [swarm testing]: https://www.cs.utah.edu/~regehr/papers/swarm12.pdf
    ///
    /// Note that we pick only *maximums*, not minimums, here because it is more
    /// complex to describe the domain of valid configs when minima are involved
    /// (`min <= max` for each variable) and minima are mostly used to ensure
    /// certain elements are present, but do not widen the range of generated
    /// Wasm modules.
    #[derive(Clone, Debug)]
    pub struct Config {
        /// Determines whether a `start` export may be included. Defaults to `true`.
        pub allow_start_export: bool = true,

        /// The kinds of instructions allowed in the generated wasm
        /// programs. Defaults to all.
        ///
        /// The categories of instructions match the categories used by the
        /// [WebAssembly
        /// specification](https://webassembly.github.io/spec/core/syntax/instructions.html);
        /// e.g., numeric, vector, control, memory, etc.
        ///
        /// Note that modifying this setting is separate from the proposal
        /// flags; that is, if `simd_enabled() == true` but
        /// `allowed_instruction()` does not include vector instructions, the
        /// generated programs will not include these instructions but could
        /// contain vector types.
        pub allowed_instructions: InstructionKinds = InstructionKinds::all(),

        /// Determines whether the bulk memory proposal is enabled for
        /// generating instructions.
        ///
        /// Defaults to `false`.
        pub bulk_memory_enabled: bool = false,

        /// Returns whether NaN values are canonicalized after all f32/f64
        /// operation. Defaults to false.
        ///
        /// This can be useful when a generated wasm module is executed in
        /// multiple runtimes which may produce different NaN values. This
        /// ensures that the generated module will always use the same NaN
        /// representation for all instructions which have visible side effects,
        /// for example writing floats to memory or float-to-int bitcast
        /// instructions.
        pub canonicalize_nans: bool = false,

        /// Returns whether we should avoid generating code that will possibly
        /// trap.
        ///
        /// For some trapping instructions, this will emit extra instructions to
        /// ensure they don't trap, while some instructions will simply be
        /// excluded.  In cases where we would run into a trap, we instead
        /// choose some arbitrary non-trapping behavior. For example, if we
        /// detect that a Load instruction would attempt to access out-of-bounds
        /// memory, we instead pretend the load succeeded and push 0 onto the
        /// stack.
        ///
        /// One type of trap that we can't currently avoid is
        /// StackOverflow. Even when `disallow_traps` is set to true, wasm-smith
        /// will eventually generate a program that infinitely recurses, causing
        /// the call stack to be exhausted.
        ///
        /// Defaults to `false`.
        pub disallow_traps: bool = false,

        /// Determines whether the exception-handling proposal is enabled for
        /// generating instructions.
        ///
        /// Defaults to `false`.
        pub exceptions_enabled: bool = false,

        /// Export all WebAssembly objects in the module. Defaults to false.
        ///
        /// This overrides [`Config::min_exports`] and [`Config::max_exports`].
        pub export_everything: bool = false,

        /// Determines whether the GC proposal is enabled when generating a Wasm
        /// module.
        ///
        /// Defaults to `false`.
        pub gc_enabled: bool = false,

        /// Returns whether we should generate custom sections or not. Defaults
        /// to false.
        pub generate_custom_sections: bool = false,

        /// Returns the maximal size of the `alias` section. Defaults to 1000.
        pub max_aliases: usize = 1000,

        /// The maximum number of components to use. Defaults to 10.
        ///
        /// This includes imported components.
        ///
        /// Note that this is only relevant for components.
        pub max_components: usize = 10,

        /// The maximum number of data segments to generate. Defaults to 100.
        pub max_data_segments: usize = 100,

        /// The maximum number of element segments to generate. Defaults to 100.
        pub max_element_segments: usize = 100,

        /// The maximum number of elements within a segment to
        /// generate. Defaults to 100.
        pub max_elements: usize = 100,

        /// The maximum number of exports to generate. Defaults to 100.
        pub max_exports: usize = 100,

        /// The maximum number of functions to generate. Defaults to 100.  This
        /// includes imported functions.
        pub max_funcs: usize = 100,

        /// The maximum number of globals to generate. Defaults to 100.  This
        /// includes imported globals.
        pub max_globals: usize = 100,

        /// The maximum number of imports to generate. Defaults to 100.
        pub max_imports: usize = 100,

        /// The maximum number of instances to use. Defaults to 10.
        ///
        /// This includes imported instances.
        ///
        /// Note that this is only relevant for components.
        pub max_instances: usize = 10,

        /// The maximum number of instructions to generate in a function
        /// body. Defaults to 100.
        ///
        /// Note that some additional `end`s, `else`s, and `unreachable`s may be
        /// appended to the function body to finish block scopes.
        pub max_instructions: usize = 100,

        /// The maximum number of memories to use. Defaults to 1.
        ///
        /// This includes imported memories.
        ///
        /// Note that more than one memory is in the realm of the multi-memory
        /// wasm proposal.
        pub max_memories: usize = 1,

        /// The maximum, in 64k Wasm pages, of any 32-bit memory's initial or
        /// maximum size.
        ///
        /// Defaults to 2^16.
        pub max_memory32_pages: u64 = 1 << 16,

        /// The maximum, in 64k Wasm pages, of any 64-bit memory's initial or
        /// maximum size.
        ///
        /// Defaults to 2^48.
        pub max_memory64_pages: u64 = 1 << 48,

        /// The maximum number of modules to use. Defaults to 10.
        ///
        /// This includes imported modules.
        ///
        /// Note that this is only relevant for components.
        pub max_modules: usize = 10,

        /// Returns the maximal nesting depth of modules with the component
        /// model proposal. Defaults to 10.
        pub max_nesting_depth: usize = 10,

        /// The maximum, elements, of any table's initial or maximum
        /// size. Defaults to 1 million.
        pub max_table_elements: u32 = 1_000_000,

        /// The maximum number of tables to use. Defaults to 1.
        ///
        /// This includes imported tables.
        ///
        /// Note that more than one table is in the realm of the reference types
        /// proposal.
        pub max_tables: usize = 1,

        /// The maximum number of tags to generate. Defaults to 100.
        pub max_tags: usize = 100,

        /// Returns the maximal effective size of any type generated by
        /// wasm-smith.
        ///
        /// Note that this number is roughly in units of "how many types would
        /// be needed to represent the recursive type". A function with 8
        /// parameters and 2 results would take 11 types (one for the type, 10
        /// for params/results). A module type with 2 imports and 3 exports
        /// would take 6 (module + imports + exports) plus the size of each
        /// import/export type. This is a somewhat rough measurement that is not
        /// intended to be very precise.
        ///
        /// Defaults to 1000.
        pub max_type_size: u32 = 1000,

        /// The maximum number of types to generate. Defaults to 100.
        pub max_types: usize = 100,

        /// The maximum number of values to use. Defaults to 10.
        ///
        /// This includes imported values.
        ///
        /// Note that this is irrelevant unless value model support is enabled.
        pub max_values: usize = 10,

        /// Returns whether 64-bit memories are allowed. Defaults to false.
        ///
        /// Note that this is the gate for the memory64 proposal to WebAssembly.
        pub memory64_enabled: bool = false,

        /// Whether every Wasm memory must have a maximum size
        /// specified. Defaults to `false`.
        pub memory_max_size_required: bool = false,

        /// Control the probability of generating memory offsets that are in
        /// bounds vs. potentially out of bounds.
        ///
        /// See the `MemoryOffsetChoices` struct for details.
        pub memory_offset_choices: MemoryOffsetChoices = MemoryOffsetChoices::default(),

        /// The minimum number of data segments to generate. Defaults to 0.
        pub min_data_segments: usize = 0,

        /// The minimum number of element segments to generate. Defaults to 0.
        pub min_element_segments: usize = 0,

        /// The minimum number of elements within a segment to
        /// generate. Defaults to 0.
        pub min_elements: usize = 0,

        /// The minimum number of exports to generate. Defaults to 0.
        pub min_exports: usize = 0,

        /// The minimum number of functions to generate. Defaults to 0.
        ///
        /// This includes imported functions.
        pub min_funcs: usize = 0,

        /// The minimum number of globals to generate. Defaults to 0.
        ///
        /// This includes imported globals.
        pub min_globals: usize = 0,

        /// The minimum number of imports to generate. Defaults to 0.
        ///
        /// Note that if the sum of the maximum function[^1], table, global and
        /// memory counts is less than the minimum number of imports, then it
        /// will not be possible to satisfy all constraints (because imports
        /// count against the limits for those element kinds). In that case, we
        /// strictly follow the max-constraints, and can fail to satisfy this
        /// minimum number.
        ///
        /// [^1]: the maximum number of functions is also limited by the number
        /// of function types arbitrarily chosen; strictly speaking, then, the
        /// maximum number of imports that can be created due to max-constraints
        /// is `sum(min(num_func_types, max_funcs), max_tables, max_globals,
        /// max_memories)`.
        pub min_imports: usize = 0,

        /// The minimum number of memories to use. Defaults to 0.
        ///
        /// This includes imported memories.
        pub min_memories: u32 = 0,

        /// The minimum number of tables to use. Defaults to 0.
        ///
        /// This includes imported tables.
        pub min_tables: u32 = 0,

        /// The minimum number of tags to generate. Defaults to 0.
        pub min_tags: usize = 0,

        /// The minimum number of types to generate. Defaults to 0.
        pub min_types: usize = 0,

        /// The minimum size, in bytes, of all leb-encoded integers. Defaults to
        /// 1.
        ///
        /// This is useful for ensuring that all leb-encoded integers are
        /// decoded as such rather than as simply one byte. This will forcibly
        /// extend leb integers with an over-long encoding in some locations if
        /// the size would otherwise be smaller than number returned here.
        pub min_uleb_size: u8 = 1,

        /// Determines whether the multi-value results are enabled.
        ///
        /// Defaults to `true`.
        pub multi_value_enabled: bool = true,

        /// Determines whether the reference types proposal is enabled for
        /// generating instructions.
        ///
        /// Defaults to `false`.
        pub reference_types_enabled: bool = false,

        /// Determines whether the Relaxed SIMD proposal is enabled for
        /// generating instructions.
        ///
        /// Defaults to `false`.
        pub relaxed_simd_enabled: bool = false,

        /// Determines whether the nontrapping-float-to-int-conversions propsal
        /// is enabled.
        ///
        /// Defaults to `true`.
        pub saturating_float_to_int_enabled: bool = true,

        /// Determines whether the sign-extension-ops propsal is enabled.
        ///
        /// Defaults to `true`.
        pub sign_extension_ops_enabled: bool = true,

        /// Determines whether the SIMD proposal is enabled for generating
        /// instructions.
        ///
        /// Defaults to `false`.
        pub simd_enabled: bool = false,

        /// Determines whether the tail calls proposal is enabled for generating
        /// instructions.
        ///
        /// Defaults to `false`.
        pub tail_call_enabled: bool = false,

        /// Whether every Wasm table must have a maximum size
        /// specified. Defaults to `false`.
        pub table_max_size_required: bool = false,

        /// Determines whether the threads proposal is enabled.
        ///
        /// The [threads proposal] involves shared linear memory, new atomic
        /// instructions, and new `wait` and `notify` instructions.
        ///
        /// [threads proposal]: https://github.com/WebAssembly/threads/blob/master/proposals/threads/Overview.md
        ///
        /// Defaults to `false`.
        pub threads_enabled: bool = false,

        /// Indicates whether wasm-smith is allowed to generate invalid function
        /// bodies.
        ///
        /// When enabled this option will enable taking raw bytes from the input
        /// byte stream and using them as a wasm function body. This means that
        /// the output module is not guaranteed to be valid but can help tickle
        /// various parts of validation/compilation in some circumstances as
        /// well.
        ///
        /// Defaults to `false`.
        pub allow_invalid_funcs: bool = false,
    }
}

/// This is a tuple `(a, b, c)` where
///
/// * `a / (a+b+c)` is the probability of generating a memory offset within
///   `0..memory.min_size`, i.e. an offset that is definitely in bounds of a
///   non-empty memory. (Note that if a memory is zero-sized, however, no offset
///   will ever be in bounds.)
///
/// * `b / (a+b+c)` is the probability of generating a memory offset within
///   `memory.min_size..memory.max_size`, i.e. an offset that is possibly in
///   bounds if the memory has been grown.
///
/// * `c / (a+b+c)` is the probability of generating a memory offset within the
///   range `memory.max_size..`, i.e. an offset that is definitely out of
///   bounds.
///
/// At least one of `a`, `b`, and `c` must be non-zero.
///
/// If you want to always generate memory offsets that are definitely in bounds
/// of a non-zero-sized memory, for example, you could return `(1, 0, 0)`.
///
/// The default is `(90, 9, 1)`.
#[derive(Clone, Debug)]
#[cfg_attr(feature = "serde_derive", derive(serde_derive::Deserialize))]
pub struct MemoryOffsetChoices(pub u32, pub u32, pub u32);

impl Default for MemoryOffsetChoices {
    fn default() -> Self {
        MemoryOffsetChoices(90, 9, 1)
    }
}

#[cfg(feature = "_internal_cli")]
impl std::str::FromStr for MemoryOffsetChoices {
    type Err = String;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        use std::str::FromStr;
        let mut parts = s.split(",");
        let a = parts
            .next()
            .ok_or_else(|| "need 3 comma separated values".to_string())?;
        let a = <u32 as FromStr>::from_str(a).map_err(|e| e.to_string())?;
        let b = parts
            .next()
            .ok_or_else(|| "need 3 comma separated values".to_string())?;
        let b = <u32 as FromStr>::from_str(b).map_err(|e| e.to_string())?;
        let c = parts
            .next()
            .ok_or_else(|| "need 3 comma separated values".to_string())?;
        let c = <u32 as FromStr>::from_str(c).map_err(|e| e.to_string())?;
        if parts.next().is_some() {
            return Err("found more than 3 comma separated values".to_string());
        }
        Ok(MemoryOffsetChoices(a, b, c))
    }
}

impl<'a> Arbitrary<'a> for Config {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self> {
        const MAX_MAXIMUM: usize = 1000;

        let reference_types_enabled: bool = u.arbitrary()?;
        let max_tables = if reference_types_enabled { 100 } else { 1 };

        Ok(Config {
            max_types: u.int_in_range(0..=MAX_MAXIMUM)?,
            max_imports: u.int_in_range(0..=MAX_MAXIMUM)?,
            max_tags: u.int_in_range(0..=MAX_MAXIMUM)?,
            max_funcs: u.int_in_range(0..=MAX_MAXIMUM)?,
            max_globals: u.int_in_range(0..=MAX_MAXIMUM)?,
            max_exports: u.int_in_range(0..=MAX_MAXIMUM)?,
            max_element_segments: u.int_in_range(0..=MAX_MAXIMUM)?,
            max_elements: u.int_in_range(0..=MAX_MAXIMUM)?,
            max_data_segments: u.int_in_range(0..=MAX_MAXIMUM)?,
            max_instructions: u.int_in_range(0..=MAX_MAXIMUM)?,
            max_memories: u.int_in_range(0..=100)?,
            max_tables,
            max_memory32_pages: u.int_in_range(0..=1 << 16)?,
            max_memory64_pages: u.int_in_range(0..=1 << 48)?,
            min_uleb_size: u.int_in_range(0..=5)?,
            bulk_memory_enabled: u.arbitrary()?,
            reference_types_enabled,
            simd_enabled: u.arbitrary()?,
            multi_value_enabled: u.arbitrary()?,
            max_aliases: u.int_in_range(0..=MAX_MAXIMUM)?,
            max_nesting_depth: u.int_in_range(0..=10)?,
            saturating_float_to_int_enabled: u.arbitrary()?,
            sign_extension_ops_enabled: u.arbitrary()?,
            allowed_instructions: {
                use flagset::Flags;
                let mut allowed = Vec::new();
                for kind in crate::core::InstructionKind::LIST {
                    if u.arbitrary()? {
                        allowed.push(*kind);
                    }
                }
                InstructionKinds::new(&allowed)
            },
            table_max_size_required: u.arbitrary()?,
            max_table_elements: u.int_in_range(0..=1_000_000)?,

            // These fields, unlike the ones above, are less useful to set.
            // They either make weird inputs or are for features not widely
            // implemented yet so they're turned off by default.
            min_types: 0,
            min_imports: 0,
            min_tags: 0,
            min_funcs: 0,
            min_globals: 0,
            min_exports: 0,
            min_element_segments: 0,
            min_elements: 0,
            min_data_segments: 0,
            min_memories: 0,
            min_tables: 0,
            memory_max_size_required: false,
            max_instances: 0,
            max_modules: 0,
            max_components: 0,
            max_values: 0,
            memory_offset_choices: MemoryOffsetChoices::default(),
            allow_start_export: true,
            relaxed_simd_enabled: false,
            exceptions_enabled: false,
            memory64_enabled: false,
            max_type_size: 1000,
            canonicalize_nans: false,
            available_imports: None,
            exports: None,
            threads_enabled: false,
            export_everything: false,
            disallow_traps: false,
            tail_call_enabled: false,
            gc_enabled: false,
            generate_custom_sections: false,
            allow_invalid_funcs: false,
        })
    }
}
