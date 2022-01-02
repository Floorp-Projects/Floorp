
#[macro_export]
macro_rules! approx_eq {
    ($typ:ty, $lhs:expr, $rhs:expr) => {
        {
            let m: <$typ as $crate::ApproxEq>::Margin = Default::default();
            <$typ as $crate::ApproxEq>::approx_eq($lhs, $rhs, m)
        }
    };
    ($typ:ty, $lhs:expr, $rhs:expr $(, $set:ident = $val:expr)*) => {
        {
            let m = <$typ as $crate::ApproxEq>::Margin::zero()$(.$set($val))*;
            <$typ as $crate::ApproxEq>::approx_eq($lhs, $rhs, m)
        }
    };
    ($typ:ty, $lhs:expr, $rhs:expr, $marg:expr) => {
        {
            <$typ as $crate::ApproxEq>::approx_eq($lhs, $rhs, $marg)
        }
    };
}

// Until saturating_abs() comes out of nightly, we have to code it ourselves.
macro_rules! saturating_abs_i32 {
    ($val:expr) => {
        if $val.is_negative() {
            match $val.checked_neg() {
                Some(v) => v,
                None => std::i32::MAX
            }
        } else {
            $val
        }
    };
}
macro_rules! saturating_abs_i64 {
    ($val:expr) => {
        if $val.is_negative() {
            match $val.checked_neg() {
                Some(v) => v,
                None => std::i64::MAX
            }
        } else {
            $val
        }
    };
}

#[test]
fn test_macro() {
    let a: f32 = 0.15 + 0.15 + 0.15;
    let b: f32 = 0.1 + 0.1 + 0.25;
    assert!( approx_eq!(f32, a, b) ); // uses the default
    assert!( approx_eq!(f32, a, b, ulps = 2) );
    assert!( approx_eq!(f32, a, b, epsilon = 0.00000003) );
    assert!( approx_eq!(f32, a, b, epsilon = 0.00000003, ulps = 2) );
    assert!( approx_eq!(f32, a, b, (0.0, 2)) );
}

#[test]
fn test_macro_2() {
    assert!( approx_eq!(f64, 1000000_f64, 1000000.0000000003_f64) );
    assert!( approx_eq!(f64, 1000000_f64, 1000000.0000000003_f64, ulps=3) );
    assert!( approx_eq!(f64, 1000000_f64, 1000000.0000000003_f64, epsilon=0.0000000004) );
    assert!( approx_eq!(f64, 1000000_f64, 1000000.0000000003_f64, (0.0000000004, 0)) );
    assert!( approx_eq!(f64, 1000000_f64, 1000000.0000000003_f64, (0.0, 3)) );
}

#[test]
fn test_macro_3() {
    use crate::F32Margin;

    let a: f32 = 0.15 + 0.15 + 0.15;
    let b: f32 = 0.1 + 0.1 + 0.25;
    assert!( approx_eq!(f32, a, b, F32Margin { epsilon: 0.0, ulps: 2 }) );
    assert!( approx_eq!(f32, a, b, F32Margin::default()) );
}
