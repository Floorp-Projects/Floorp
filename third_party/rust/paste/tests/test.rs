mod test_basic {
    struct Struct;

    paste::item! {
        impl Struct {
            fn [<a b c>]() {}
        }
    }

    #[test]
    fn test() {
        Struct::abc();
    }
}

mod test_in_impl {
    struct Struct;

    impl Struct {
        paste::item! {
            fn [<a b c>]() {}
        }
    }

    #[test]
    fn test() {
        Struct::abc();
    }
}

#[test]
fn test_shared_hygiene() {
    paste::expr! {
        let [<a a>] = 1;
        assert_eq!([<a a>], 1);
    }
}

#[test]
fn test_repeat() {
    const ROCKET_A: &'static str = "/a";
    const ROCKET_B: &'static str = "/b";

    macro_rules! routes {
        ($($route:ident),*) => {{
            paste::expr! {
                vec![$( [<ROCKET_ $route>] ),*]
            }
        }}
    }

    let routes = routes!(A, B);
    assert_eq!(routes, vec!["/a", "/b"]);
}

#[test]
fn test_integer() {
    const CONST0: &'static str = "const0";

    let pasted = paste::expr!([<CONST 0>]);
    assert_eq!(pasted, CONST0);
}

#[test]
fn test_underscore() {
    paste::expr! {
        const A_B: usize = 0;
        assert_eq!([<A _ B>], 0);
    }
}

#[test]
fn test_lifetime() {
    paste::expr! {
        #[allow(dead_code)]
        struct S<[<'d e>]> {
            q: &[<'d e>] str,
        }
    }
}

#[test]
fn test_keyword() {
    paste::expr! {
        struct [<F move>];

        let _ = Fmove;
    }
}

#[test]
fn test_literal_str() {
    paste::expr! {
        struct [<Foo "Bar-Baz">];

        let _ = FooBar_Baz;
    }
}

#[test]
fn test_env_literal() {
    paste::expr! {
        struct [<Lib env bar>];

        let _ = Libenvbar;
    }
}

#[test]
fn test_env_present() {
    paste::expr! {
        struct [<Lib env!("CARGO_PKG_NAME")>];

        let _ = Libpaste;
    }
}

#[test]
fn test_raw_identifier() {
    paste::expr! {
        struct [<F r#move>];

        let _ = Fmove;
    }
}

#[test]
fn test_false_start() {
    trait Trait {
        fn f() -> usize;
    }

    struct S;

    impl Trait for S {
        fn f() -> usize {
            0
        }
    }

    paste::expr! {
        let x = [<S as Trait>::f()];
        assert_eq!(x[0], 0);
    }
}

#[test]
fn test_local_variable() {
    let yy = 0;

    paste::expr! {
        assert_eq!([<y y>], 0);
    }
}

mod test_none_delimited_single_ident {
    macro_rules! m {
        ($id:ident) => {
            paste::item! {
                fn f() -> &'static str {
                    stringify!($id)
                }
            }
        };
    }

    m!(i32x4);

    #[test]
    fn test() {
        assert_eq!(f(), "i32x4");
    }
}

mod test_none_delimited_single_lifetime {
    macro_rules! m {
        ($life:lifetime) => {
            paste::item! {
                pub struct S;
                impl<$life> S {
                    fn f() {}
                }
            }
        };
    }

    m!('a);

    #[test]
    fn test() {
        S::f();
    }
}

mod test_to_lower {
    macro_rules! m {
        ($id:ident) => {
            paste::item! {
                fn [<my_ $id:lower _here>](_arg: u8) -> &'static str {
                    stringify!([<$id:lower>])
                }
            }
        };
    }

    m!(Test);

    #[test]
    fn test_to_lower() {
        assert_eq!(my_test_here(0), "test");
    }
}

#[test]
fn test_env_to_lower() {
    paste::expr! {
        struct [<Lib env!("CARGO_PKG_NAME"):lower>];

        let _ = Libpaste;
    }
}

mod test_to_upper {
    macro_rules! m {
        ($id:ident) => {
            paste::item! {
                const [<MY_ $id:upper _HERE>]: &str = stringify!([<$id:upper>]);
            }
        };
    }

    m!(Test);

    #[test]
    fn test_to_upper() {
        assert_eq!(MY_TEST_HERE, "TEST");
    }
}

#[test]
fn test_env_to_upper() {
    paste::expr! {
        const [<LIB env!("CARGO_PKG_NAME"):upper>]: &str = "libpaste";

        let _ = LIBPASTE;
    }
}

mod test_to_snake {
    macro_rules! m {
        ($id:ident) => {
            paste::item! {
                const DEFAULT_SNAKE: &str = stringify!([<$id:snake>]);
                const LOWER_SNAKE: &str = stringify!([<$id:snake:lower>]);
                const UPPER_SNAKE: &str = stringify!([<$id:snake:upper>]);
            }
        };
    }

    m!(ThisIsButATest);

    #[test]
    fn test_to_snake() {
        assert_eq!(DEFAULT_SNAKE, "this_is_but_a_test");
        assert_eq!(LOWER_SNAKE, "this_is_but_a_test");
        assert_eq!(UPPER_SNAKE, "THIS_IS_BUT_A_TEST");
    }
}

#[test]
fn test_env_to_snake() {
    paste::expr! {
        const [<LIB env!("CARGO_PKG_NAME"):snake:upper>]: &str = "libpaste";

        let _ = LIBPASTE;
    }
}

mod test_to_camel {
    macro_rules! m {
        ($id:ident) => {
            paste::item! {
                const DEFAULT_CAMEL: &str = stringify!([<$id:camel>]);
                const LOWER_CAMEL: &str = stringify!([<$id:camel:lower>]);
                const UPPER_CAMEL: &str = stringify!([<$id:camel:upper>]);
            }
        };
    }

    m!(this_is_but_a_test);

    #[test]
    fn test_to_camel() {
        assert_eq!(DEFAULT_CAMEL, "ThisIsButATest");
        assert_eq!(LOWER_CAMEL, "thisisbutatest");
        assert_eq!(UPPER_CAMEL, "THISISBUTATEST");
    }
}

#[test]
fn test_env_to_camel() {
    paste::expr! {
        const [<LIB env!("CARGO_PKG_NAME"):camel>]: &str = "libpaste";

        let _ = LIBPaste;
    }
}
