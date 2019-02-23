/// Recognized associated type for the token location
pub const LOCATION: &'static str = "Location";

/// Recognized associated type for custom errors
pub const ERROR: &'static str = "Error";

/// The lifetime parameter injected when we do not have an external token enum
pub const INPUT_LIFETIME: &'static str = "'input";

/// The parameter injected when we do not have an external token enum
pub const INPUT_PARAMETER: &'static str = "input";

/// The annotation to request inlining.
pub const INLINE: &'static str = "inline";

/// The annotation to request conditional compilation.
pub const CFG: &'static str = "cfg";

/// Annotation to request LALR.
pub const LALR: &'static str = "LALR";

/// Annotation to request recursive-ascent-style code generation.
pub const TABLE_DRIVEN: &'static str = "table_driven";

/// Annotation to request recursive-ascent-style code generation.
pub const RECURSIVE_ASCENT: &'static str = "recursive_ascent";

/// Annotation to request test-all-style code generation.
pub const TEST_ALL: &'static str = "test_all";
