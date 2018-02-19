/// Unmangle the wrapped functions if no_c_export is not defined.
/// For benchmarks, and other comparisons where we want to have both the miniz and miniz_oxide
/// functions available, functions shouldn not be marked `no_mangle` since that will cause
/// conflicts.
#[cfg(not(feature = "no_c_export"))]
#[macro_export]
macro_rules! unmangle {
    ($($it:item)*) => {$(
        #[no_mangle]
        #[allow(bad_style)]
        $it
    )*}
}

#[cfg(feature = "no_c_export")]
#[macro_export]
macro_rules! unmangle {
    ($($it:item)*) => {$(
        #[allow(bad_style)]
        $it
    )*}
}
