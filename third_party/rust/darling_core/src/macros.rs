macro_rules! path {
    ($($path:tt)+) => {
        parse_quote!($($path)+)
        //stringify!($($path)+).parse().unwrap()
    };
}
