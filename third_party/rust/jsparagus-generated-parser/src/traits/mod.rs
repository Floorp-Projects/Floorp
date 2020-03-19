/// This macro is pre-processed by the python grammar processor and generate
/// code out-side the current context.
#[macro_export]
macro_rules! grammar_extension {
    ( $($_:tt)* ) => {};
}
