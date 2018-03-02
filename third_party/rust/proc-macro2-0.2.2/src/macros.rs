macro_rules! forward_fmt {
    ($tr:ident for $ty:ident) => {
        impl ::std::fmt::$tr for $ty {
            fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
                ::std::fmt::$tr::fmt(&self.0, f)
            }
        }
    }
}
