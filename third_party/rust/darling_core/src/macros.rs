macro_rules! path {
    ($($path:tt)+) => {
        ::syn::parse_path(stringify!($($path)+)).unwrap()
    };
}