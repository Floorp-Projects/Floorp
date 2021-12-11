use bindgen::{
    builder, AliasVariation, Builder, CodegenConfig, EnumVariation,
    MacroTypeVariation, RustTarget, DEFAULT_ANON_FIELDS_PREFIX,
    RUST_TARGET_STRINGS,
};
use clap::{App, Arg};
use std::fs::File;
use std::io::{self, stderr, Error, ErrorKind, Write};
use std::path::PathBuf;
use std::str::FromStr;

/// Construct a new [`Builder`](./struct.Builder.html) from command line flags.
pub fn builder_from_flags<I>(
    args: I,
) -> Result<(Builder, Box<dyn io::Write>, bool), io::Error>
where
    I: Iterator<Item = String>,
{
    let rust_target_help = format!(
        "Version of the Rust compiler to target. Valid options are: {:?}. Defaults to {:?}.",
        RUST_TARGET_STRINGS,
        String::from(RustTarget::default())
    );

    let matches = App::new("bindgen")
        .version(option_env!("CARGO_PKG_VERSION").unwrap_or("unknown"))
        .about("Generates Rust bindings from C/C++ headers.")
        .usage("bindgen [FLAGS] [OPTIONS] <header> -- <clang-args>...")
        .args(&[
            Arg::with_name("header")
                .help("C or C++ header file")
                .required(true),
            Arg::with_name("default-enum-style")
                .long("default-enum-style")
                .help("The default style of code used to generate enums.")
                .value_name("variant")
                .default_value("consts")
                .possible_values(&[
                    "consts",
                    "moduleconsts",
                    "bitfield",
                    "newtype",
                    "rust",
                    "rust_non_exhaustive",
                ])
                .multiple(false),
            Arg::with_name("bitfield-enum")
                .long("bitfield-enum")
                .help(
                    "Mark any enum whose name matches <regex> as a set of \
                     bitfield flags.",
                )
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("newtype-enum")
                .long("newtype-enum")
                .help("Mark any enum whose name matches <regex> as a newtype.")
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("rustified-enum")
                .long("rustified-enum")
                .help("Mark any enum whose name matches <regex> as a Rust enum.")
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("constified-enum")
                .long("constified-enum")
                .help(
                    "Mark any enum whose name matches <regex> as a series of \
                     constants.",
                )
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("constified-enum-module")
                .long("constified-enum-module")
                .help(
                    "Mark any enum whose name matches <regex> as a module of \
                     constants.",
                )
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("default-macro-constant-type")
                .long("default-macro-constant-type")
                .help("The default signed/unsigned type for C macro constants.")
                .value_name("variant")
                .default_value("unsigned")
                .possible_values(&["signed", "unsigned"])
                .multiple(false),
            Arg::with_name("default-alias-style")
                .long("default-alias-style")
                .help("The default style of code used to generate typedefs.")
                .value_name("variant")
                .default_value("type_alias")
                .possible_values(&[
                    "type_alias",
                    "new_type",
                    "new_type_deref",
                ])
                .multiple(false),
            Arg::with_name("normal-alias")
                .long("normal-alias")
                .help(
                    "Mark any typedef alias whose name matches <regex> to use \
                     normal type aliasing.",
                )
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
             Arg::with_name("new-type-alias")
                .long("new-type-alias")
                .help(
                    "Mark any typedef alias whose name matches <regex> to have \
                     a new type generated for it.",
                )
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
             Arg::with_name("new-type-alias-deref")
                .long("new-type-alias-deref")
                .help(
                    "Mark any typedef alias whose name matches <regex> to have \
                     a new type with Deref and DerefMut to the inner type.",
                )
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("blacklist-type")
                .long("blacklist-type")
                .help("Mark <type> as hidden.")
                .value_name("type")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("blacklist-function")
                .long("blacklist-function")
                .help("Mark <function> as hidden.")
                .value_name("function")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("blacklist-item")
                .long("blacklist-item")
                .help("Mark <item> as hidden.")
                .value_name("item")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("no-layout-tests")
                .long("no-layout-tests")
                .help("Avoid generating layout tests for any type."),
            Arg::with_name("no-derive-copy")
                .long("no-derive-copy")
                .help("Avoid deriving Copy on any type."),
            Arg::with_name("no-derive-debug")
                .long("no-derive-debug")
                .help("Avoid deriving Debug on any type."),
            Arg::with_name("no-derive-default")
                .long("no-derive-default")
                .hidden(true)
                .help("Avoid deriving Default on any type."),
            Arg::with_name("impl-debug").long("impl-debug").help(
                "Create Debug implementation, if it can not be derived \
                 automatically.",
            ),
            Arg::with_name("impl-partialeq")
                .long("impl-partialeq")
                .help(
                    "Create PartialEq implementation, if it can not be derived \
                     automatically.",
                ),
            Arg::with_name("with-derive-default")
                .long("with-derive-default")
                .help("Derive Default on any type."),
            Arg::with_name("with-derive-hash")
                .long("with-derive-hash")
                .help("Derive hash on any type."),
            Arg::with_name("with-derive-partialeq")
                .long("with-derive-partialeq")
                .help("Derive partialeq on any type."),
            Arg::with_name("with-derive-partialord")
                .long("with-derive-partialord")
                .help("Derive partialord on any type."),
            Arg::with_name("with-derive-eq")
                .long("with-derive-eq")
                .help(
                    "Derive eq on any type. Enable this option also \
                     enables --with-derive-partialeq",
                ),
            Arg::with_name("with-derive-ord")
                .long("with-derive-ord")
                .help(
                    "Derive ord on any type. Enable this option also \
                     enables --with-derive-partialord",
                ),
            Arg::with_name("no-doc-comments")
                .long("no-doc-comments")
                .help(
                    "Avoid including doc comments in the output, see: \
                     https://github.com/rust-lang/rust-bindgen/issues/426",
                ),
            Arg::with_name("no-recursive-whitelist")
                .long("no-recursive-whitelist")
                .help(
                    "Disable whitelisting types recursively. This will cause \
                     bindgen to emit Rust code that won't compile! See the \
                     `bindgen::Builder::whitelist_recursively` method's \
                     documentation for details.",
                ),
            Arg::with_name("objc-extern-crate")
                .long("objc-extern-crate")
                .help("Use extern crate instead of use for objc."),
            Arg::with_name("generate-block")
                .long("generate-block")
                .help("Generate block signatures instead of void pointers."),
            Arg::with_name("block-extern-crate")
                .long("block-extern-crate")
                .help("Use extern crate instead of use for block."),
            Arg::with_name("distrust-clang-mangling")
                .long("distrust-clang-mangling")
                .help("Do not trust the libclang-provided mangling"),
            Arg::with_name("builtins").long("builtins").help(
                "Output bindings for builtin definitions, e.g. \
                 __builtin_va_list.",
            ),
            Arg::with_name("ctypes-prefix")
                .long("ctypes-prefix")
                .help(
                    "Use the given prefix before raw types instead of \
                     ::std::os::raw.",
                )
                .value_name("prefix")
                .takes_value(true),
            Arg::with_name("anon-fields-prefix")
                .long("anon-fields-prefix")
                .help("Use the given prefix for the anon fields.")
                .value_name("prefix")
                .default_value(DEFAULT_ANON_FIELDS_PREFIX)
                .takes_value(true),
            Arg::with_name("time-phases")
                .long("time-phases")
                .help("Time the different bindgen phases and print to stderr"),
            // All positional arguments after the end of options marker, `--`
            Arg::with_name("clang-args").last(true).multiple(true),
            Arg::with_name("emit-clang-ast")
                .long("emit-clang-ast")
                .help("Output the Clang AST for debugging purposes."),
            Arg::with_name("emit-ir")
                .long("emit-ir")
                .help("Output our internal IR for debugging purposes."),
            Arg::with_name("emit-ir-graphviz")
                .long("emit-ir-graphviz")
                .help("Dump graphviz dot file.")
                .value_name("path")
                .takes_value(true),
            Arg::with_name("enable-cxx-namespaces")
                .long("enable-cxx-namespaces")
                .help("Enable support for C++ namespaces."),
            Arg::with_name("disable-name-namespacing")
                .long("disable-name-namespacing")
                .help(
                    "Disable namespacing via mangling, causing bindgen to \
                     generate names like \"Baz\" instead of \"foo_bar_Baz\" \
                     for an input name \"foo::bar::Baz\".",
                ),
            Arg::with_name("disable-nested-struct-naming")
                .long("disable-nested-struct-naming")
                .help(
                    "Disable nested struct naming, causing bindgen to generate \
                     names like \"bar\" instead of \"foo_bar\" for a nested \
                     definition \"struct foo { struct bar { } b; };\"."
                ),
            Arg::with_name("disable-untagged-union")
                .long("disable-untagged-union")
                .help(
                    "Disable support for native Rust unions.",
                ),
            Arg::with_name("disable-header-comment")
                .long("disable-header-comment")
                .help("Suppress insertion of bindgen's version identifier into generated bindings.")
                .multiple(true),
            Arg::with_name("ignore-functions")
                .long("ignore-functions")
                .help(
                    "Do not generate bindings for functions or methods. This \
                     is useful when you only care about struct layouts.",
                ),
            Arg::with_name("generate")
                .long("generate")
                .help(
                    "Generate only given items, split by commas. \
                     Valid values are \"functions\",\"types\", \"vars\", \
                     \"methods\", \"constructors\" and \"destructors\".",
                )
                .takes_value(true),
            Arg::with_name("ignore-methods")
                .long("ignore-methods")
                .help("Do not generate bindings for methods."),
            Arg::with_name("no-convert-floats")
                .long("no-convert-floats")
                .help("Do not automatically convert floats to f32/f64."),
            Arg::with_name("no-prepend-enum-name")
                .long("no-prepend-enum-name")
                .help("Do not prepend the enum name to constant or newtype variants."),
            Arg::with_name("no-include-path-detection")
                .long("no-include-path-detection")
                .help("Do not try to detect default include paths"),
            Arg::with_name("unstable-rust")
                .long("unstable-rust")
                .help("Generate unstable Rust code (deprecated; use --rust-target instead).")
                .multiple(true), // FIXME: Pass legacy test suite
            Arg::with_name("opaque-type")
                .long("opaque-type")
                .help("Mark <type> as opaque.")
                .value_name("type")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("output")
                .short("o")
                .long("output")
                .help("Write Rust bindings to <output>.")
                .takes_value(true),
            Arg::with_name("raw-line")
                .long("raw-line")
                .help("Add a raw line of Rust code at the beginning of output.")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("rust-target")
                .long("rust-target")
                .help(&rust_target_help)
                .takes_value(true),
            Arg::with_name("use-core")
                .long("use-core")
                .help("Use types from Rust core instead of std."),
            Arg::with_name("conservative-inline-namespaces")
                .long("conservative-inline-namespaces")
                .help(
                    "Conservatively generate inline namespaces to avoid name \
                     conflicts.",
                ),
            Arg::with_name("use-msvc-mangling")
                .long("use-msvc-mangling")
                .help("MSVC C++ ABI mangling. DEPRECATED: Has no effect."),
            Arg::with_name("whitelist-function")
                .long("whitelist-function")
                .help(
                    "Whitelist all the free-standing functions matching \
                     <regex>. Other non-whitelisted functions will not be \
                     generated.",
                )
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("generate-inline-functions")
                .long("generate-inline-functions")
                .help("Generate inline functions."),
            Arg::with_name("whitelist-type")
                .long("whitelist-type")
                .help(
                    "Only generate types matching <regex>. Other non-whitelisted types will \
                     not be generated.",
                )
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("whitelist-var")
                .long("whitelist-var")
                .help(
                    "Whitelist all the free-standing variables matching \
                     <regex>. Other non-whitelisted variables will not be \
                     generated.",
                )
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("verbose")
                .long("verbose")
                .help("Print verbose error messages."),
            Arg::with_name("dump-preprocessed-input")
                .long("dump-preprocessed-input")
                .help(
                    "Preprocess and dump the input header files to disk. \
                     Useful when debugging bindgen, using C-Reduce, or when \
                     filing issues. The resulting file will be named \
                     something like `__bindgen.i` or `__bindgen.ii`.",
                ),
            Arg::with_name("no-record-matches")
                .long("no-record-matches")
                .help(
                    "Do not record matching items in the regex sets. \
                     This disables reporting of unused items.",
                ),
            Arg::with_name("size_t-is-usize")
                .long("size_t-is-usize")
                .help("Translate size_t to usize."),
            Arg::with_name("no-rustfmt-bindings")
                .long("no-rustfmt-bindings")
                .help("Do not format the generated bindings with rustfmt."),
            Arg::with_name("rustfmt-bindings")
                .long("rustfmt-bindings")
                .help(
                    "Format the generated bindings with rustfmt. DEPRECATED: \
                     --rustfmt-bindings is now enabled by default. Disable \
                     with --no-rustfmt-bindings.",
                ),
            Arg::with_name("rustfmt-configuration-file")
                .long("rustfmt-configuration-file")
                .help(
                    "The absolute path to the rustfmt configuration file. \
                     The configuration file will be used for formatting the bindings. \
                     This parameter is incompatible with --no-rustfmt-bindings.",
                )
                .value_name("path")
                .takes_value(true)
                .multiple(false)
                .number_of_values(1),
            Arg::with_name("no-partialeq")
                .long("no-partialeq")
                .help("Avoid deriving PartialEq for types matching <regex>.")
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("no-copy")
                .long("no-copy")
                .help("Avoid deriving Copy for types matching <regex>.")
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("no-debug")
                .long("no-debug")
                .help("Avoid deriving Debug for types matching <regex>.")
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("no-default")
                .long("no-default")
                .help("Avoid deriving/implement Default for types matching <regex>.")
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("no-hash")
                .long("no-hash")
                .help("Avoid deriving Hash for types matching <regex>.")
                .value_name("regex")
                .takes_value(true)
                .multiple(true)
                .number_of_values(1),
            Arg::with_name("enable-function-attribute-detection")
                .long("enable-function-attribute-detection")
                .help(
                    "Enables detecting unexposed attributes in functions (slow).
                       Used to generate #[must_use] annotations.",
                ),
            Arg::with_name("use-array-pointers-in-arguments")
                .long("use-array-pointers-in-arguments")
                .help("Use `*const [T; size]` instead of `*const T` for C arrays"),
            Arg::with_name("wasm-import-module-name")
                .long("wasm-import-module-name")
                .value_name("name")
                .takes_value(true)
                .help("The name to be used in a #[link(wasm_import_module = ...)] statement"),
            Arg::with_name("dynamic-loading")
                .long("dynamic-loading")
                .takes_value(true)
                .help("Use dynamic loading mode with the given library name."),
        ]) // .args()
        .get_matches_from(args);

    let mut builder = builder();

    if let Some(header) = matches.value_of("header") {
        builder = builder.header(header);
    } else {
        return Err(Error::new(ErrorKind::Other, "Header not found"));
    }

    if matches.is_present("unstable-rust") {
        builder = builder.rust_target(RustTarget::Nightly);
        writeln!(
            &mut stderr(),
            "warning: the `--unstable-rust` option is deprecated"
        )
        .expect("Unable to write error message");
    }

    if let Some(rust_target) = matches.value_of("rust-target") {
        builder = builder.rust_target(RustTarget::from_str(rust_target)?);
    }

    if let Some(variant) = matches.value_of("default-enum-style") {
        builder = builder.default_enum_style(EnumVariation::from_str(variant)?)
    }

    if let Some(bitfields) = matches.values_of("bitfield-enum") {
        for regex in bitfields {
            builder = builder.bitfield_enum(regex);
        }
    }

    if let Some(newtypes) = matches.values_of("newtype-enum") {
        for regex in newtypes {
            builder = builder.newtype_enum(regex);
        }
    }

    if let Some(rustifieds) = matches.values_of("rustified-enum") {
        for regex in rustifieds {
            builder = builder.rustified_enum(regex);
        }
    }

    if let Some(const_enums) = matches.values_of("constified-enum") {
        for regex in const_enums {
            builder = builder.constified_enum(regex);
        }
    }

    if let Some(constified_mods) = matches.values_of("constified-enum-module") {
        for regex in constified_mods {
            builder = builder.constified_enum_module(regex);
        }
    }

    if let Some(variant) = matches.value_of("default-macro-constant-type") {
        builder = builder
            .default_macro_constant_type(MacroTypeVariation::from_str(variant)?)
    }

    if let Some(variant) = matches.value_of("default-alias-style") {
        builder =
            builder.default_alias_style(AliasVariation::from_str(variant)?);
    }

    if let Some(type_alias) = matches.values_of("normal-alias") {
        for regex in type_alias {
            builder = builder.type_alias(regex);
        }
    }

    if let Some(new_type) = matches.values_of("new-type-alias") {
        for regex in new_type {
            builder = builder.new_type_alias(regex);
        }
    }

    if let Some(new_type_deref) = matches.values_of("new-type-alias-deref") {
        for regex in new_type_deref {
            builder = builder.new_type_alias_deref(regex);
        }
    }

    if let Some(hidden_types) = matches.values_of("blacklist-type") {
        for ty in hidden_types {
            builder = builder.blacklist_type(ty);
        }
    }

    if let Some(hidden_functions) = matches.values_of("blacklist-function") {
        for fun in hidden_functions {
            builder = builder.blacklist_function(fun);
        }
    }

    if let Some(hidden_identifiers) = matches.values_of("blacklist-item") {
        for id in hidden_identifiers {
            builder = builder.blacklist_item(id);
        }
    }

    if matches.is_present("builtins") {
        builder = builder.emit_builtins();
    }

    if matches.is_present("no-layout-tests") {
        builder = builder.layout_tests(false);
    }

    if matches.is_present("no-derive-copy") {
        builder = builder.derive_copy(false);
    }

    if matches.is_present("no-derive-debug") {
        builder = builder.derive_debug(false);
    }

    if matches.is_present("impl-debug") {
        builder = builder.impl_debug(true);
    }

    if matches.is_present("impl-partialeq") {
        builder = builder.impl_partialeq(true);
    }

    if matches.is_present("with-derive-default") {
        builder = builder.derive_default(true);
    }

    if matches.is_present("with-derive-hash") {
        builder = builder.derive_hash(true);
    }

    if matches.is_present("with-derive-partialeq") {
        builder = builder.derive_partialeq(true);
    }

    if matches.is_present("with-derive-partialord") {
        builder = builder.derive_partialord(true);
    }

    if matches.is_present("with-derive-eq") {
        builder = builder.derive_eq(true);
    }

    if matches.is_present("with-derive-ord") {
        builder = builder.derive_ord(true);
    }

    if matches.is_present("no-derive-default") {
        builder = builder.derive_default(false);
    }

    if matches.is_present("no-prepend-enum-name") {
        builder = builder.prepend_enum_name(false);
    }

    if matches.is_present("no-include-path-detection") {
        builder = builder.detect_include_paths(false);
    }

    if matches.is_present("time-phases") {
        builder = builder.time_phases(true);
    }

    if matches.is_present("use-array-pointers-in-arguments") {
        builder = builder.array_pointers_in_arguments(true);
    }

    if let Some(wasm_import_name) = matches.value_of("wasm-import-module-name")
    {
        builder = builder.wasm_import_module_name(wasm_import_name);
    }

    if let Some(prefix) = matches.value_of("ctypes-prefix") {
        builder = builder.ctypes_prefix(prefix);
    }

    if let Some(prefix) = matches.value_of("anon-fields-prefix") {
        builder = builder.anon_fields_prefix(prefix);
    }

    if let Some(what_to_generate) = matches.value_of("generate") {
        let mut config = CodegenConfig::empty();
        for what in what_to_generate.split(",") {
            match what {
                "functions" => config.insert(CodegenConfig::FUNCTIONS),
                "types" => config.insert(CodegenConfig::TYPES),
                "vars" => config.insert(CodegenConfig::VARS),
                "methods" => config.insert(CodegenConfig::METHODS),
                "constructors" => config.insert(CodegenConfig::CONSTRUCTORS),
                "destructors" => config.insert(CodegenConfig::DESTRUCTORS),
                otherwise => {
                    return Err(Error::new(
                        ErrorKind::Other,
                        format!("Unknown generate item: {}", otherwise),
                    ));
                }
            }
        }
        builder = builder.with_codegen_config(config);
    }

    if matches.is_present("emit-clang-ast") {
        builder = builder.emit_clang_ast();
    }

    if matches.is_present("emit-ir") {
        builder = builder.emit_ir();
    }

    if let Some(path) = matches.value_of("emit-ir-graphviz") {
        builder = builder.emit_ir_graphviz(path);
    }

    if matches.is_present("enable-cxx-namespaces") {
        builder = builder.enable_cxx_namespaces();
    }

    if matches.is_present("enable-function-attribute-detection") {
        builder = builder.enable_function_attribute_detection();
    }

    if matches.is_present("disable-name-namespacing") {
        builder = builder.disable_name_namespacing();
    }

    if matches.is_present("disable-nested-struct-naming") {
        builder = builder.disable_nested_struct_naming();
    }

    if matches.is_present("disable-untagged-union") {
        builder = builder.disable_untagged_union();
    }

    if matches.is_present("disable-header-comment") {
        builder = builder.disable_header_comment();
    }

    if matches.is_present("ignore-functions") {
        builder = builder.ignore_functions();
    }

    if matches.is_present("ignore-methods") {
        builder = builder.ignore_methods();
    }

    if matches.is_present("no-convert-floats") {
        builder = builder.no_convert_floats();
    }

    if matches.is_present("no-doc-comments") {
        builder = builder.generate_comments(false);
    }

    if matches.is_present("no-recursive-whitelist") {
        builder = builder.whitelist_recursively(false);
    }

    if matches.is_present("objc-extern-crate") {
        builder = builder.objc_extern_crate(true);
    }

    if matches.is_present("generate-block") {
        builder = builder.generate_block(true);
    }

    if matches.is_present("block-extern-crate") {
        builder = builder.block_extern_crate(true);
    }

    if let Some(opaque_types) = matches.values_of("opaque-type") {
        for ty in opaque_types {
            builder = builder.opaque_type(ty);
        }
    }

    if let Some(lines) = matches.values_of("raw-line") {
        for line in lines {
            builder = builder.raw_line(line);
        }
    }

    if matches.is_present("use-core") {
        builder = builder.use_core();
    }

    if matches.is_present("distrust-clang-mangling") {
        builder = builder.trust_clang_mangling(false);
    }

    if matches.is_present("conservative-inline-namespaces") {
        builder = builder.conservative_inline_namespaces();
    }

    if matches.is_present("generate-inline-functions") {
        builder = builder.generate_inline_functions(true);
    }

    if let Some(whitelist) = matches.values_of("whitelist-function") {
        for regex in whitelist {
            builder = builder.whitelist_function(regex);
        }
    }

    if let Some(whitelist) = matches.values_of("whitelist-type") {
        for regex in whitelist {
            builder = builder.whitelist_type(regex);
        }
    }

    if let Some(whitelist) = matches.values_of("whitelist-var") {
        for regex in whitelist {
            builder = builder.whitelist_var(regex);
        }
    }

    if let Some(args) = matches.values_of("clang-args") {
        for arg in args {
            builder = builder.clang_arg(arg);
        }
    }

    let output = if let Some(path) = matches.value_of("output") {
        let file = File::create(path)?;
        Box::new(io::BufWriter::new(file)) as Box<dyn io::Write>
    } else {
        Box::new(io::BufWriter::new(io::stdout())) as Box<dyn io::Write>
    };

    if matches.is_present("dump-preprocessed-input") {
        builder.dump_preprocessed_input()?;
    }

    if matches.is_present("no-record-matches") {
        builder = builder.record_matches(false);
    }

    if matches.is_present("size_t-is-usize") {
        builder = builder.size_t_is_usize(true);
    }

    let no_rustfmt_bindings = matches.is_present("no-rustfmt-bindings");
    if no_rustfmt_bindings {
        builder = builder.rustfmt_bindings(false);
    }

    if let Some(path_str) = matches.value_of("rustfmt-configuration-file") {
        let path = PathBuf::from(path_str);

        if no_rustfmt_bindings {
            return Err(Error::new(
                ErrorKind::Other,
                "Cannot supply both --rustfmt-configuration-file and --no-rustfmt-bindings",
            ));
        }

        if !path.is_absolute() {
            return Err(Error::new(
                ErrorKind::Other,
                "--rustfmt-configuration--file needs to be an absolute path!",
            ));
        }

        if path.to_str().is_none() {
            return Err(Error::new(
                ErrorKind::Other,
                "--rustfmt-configuration-file contains non-valid UTF8 characters.",
            ));
        }

        builder = builder.rustfmt_configuration_file(Some(path));
    }

    if let Some(no_partialeq) = matches.values_of("no-partialeq") {
        for regex in no_partialeq {
            builder = builder.no_partialeq(regex);
        }
    }

    if let Some(no_copy) = matches.values_of("no-copy") {
        for regex in no_copy {
            builder = builder.no_copy(regex);
        }
    }

    if let Some(no_debug) = matches.values_of("no-debug") {
        for regex in no_debug {
            builder = builder.no_debug(regex);
        }
    }

    if let Some(no_default) = matches.values_of("no-default") {
        for regex in no_default {
            builder = builder.no_default(regex);
        }
    }

    if let Some(no_hash) = matches.values_of("no-hash") {
        for regex in no_hash {
            builder = builder.no_hash(regex);
        }
    }

    if let Some(dynamic_library_name) = matches.value_of("dynamic-loading") {
        builder = builder.dynamic_library_name(dynamic_library_name);
    }

    let verbose = matches.is_present("verbose");

    Ok((builder, output, verbose))
}
