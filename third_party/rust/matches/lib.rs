#[macro_export]
macro_rules! matches {
    ($expression:expr, $($pattern:tt)+) => {
        _matches_tt_as_expr_hack! {
            match $expression {
                $($pattern)+ => true,
                _ => false
            }
        }
    }
}

/// Work around "error: unexpected token: `an interpolated tt`", whatever that means.
#[macro_export]
macro_rules! _matches_tt_as_expr_hack {
    ($value:expr) => ($value)
}

#[macro_export]
macro_rules! assert_matches {
    ($expression:expr, $($pattern:tt)+) => {
        _matches_tt_as_expr_hack! {
            match $expression {
                $($pattern)+ => (),
                ref e => panic!("assertion failed: `{:?}` does not match `{}`", e, stringify!($($pattern)+)),
            }
        }
    }
}

#[macro_export]
macro_rules! debug_assert_matches {
    ($($arg:tt)*) => (if cfg!(debug_assertions) { assert_matches!($($arg)*); })
}

#[test]
fn matches_works() {
    let foo = Some("-12");
    assert!(matches!(foo, Some(bar) if
        matches!(bar.as_bytes()[0], b'+' | b'-') &&
        matches!(bar.as_bytes()[1], b'0'...b'9')
    ));
}

#[test]
fn assert_matches_works() {
    let foo = Some("-12");
    assert_matches!(foo, Some(bar) if
        matches!(bar.as_bytes()[0], b'+' | b'-') &&
        matches!(bar.as_bytes()[1], b'0'...b'9')
    );
}

#[test]
#[should_panic(expected = "assertion failed: `Some(\"-AB\")` does not match ")]
fn assert_matches_panics() {
    let foo = Some("-AB");
    assert_matches!(foo, Some(bar) if
        matches!(bar.as_bytes()[0], b'+' | b'-') &&
        matches!(bar.as_bytes()[1], b'0'...b'9')
    );
}
