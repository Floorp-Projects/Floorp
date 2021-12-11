//! This module uses doc-tests on modules for `compile_fail`

// We need "syn/full" to parse macros.
// Use `--nocapture` to check the quality of the error message.
#[cfg(not(feature = "full-syntax"))]
/// ```compile_fail
/// macro_rules! get_an_isize {
///     () => (0_isize)
/// }
///
/// #[derive(num_derive::FromPrimitive)]
/// pub enum CLikeEnum {
///     VarA = get_an_isize!(), // error without "syn/full"
///     VarB = 2,
/// }
/// ```
mod issue16 {}

#[cfg(feature = "full-syntax")]
/// ```
/// macro_rules! get_an_isize {
///     () => (0_isize)
/// }
///
/// #[derive(num_derive::FromPrimitive)]
/// pub enum CLikeEnum {
///     VarA = get_an_isize!(), // ok with "syn/full"
///     VarB = 2,
/// }
/// ```
mod issue16 {}
