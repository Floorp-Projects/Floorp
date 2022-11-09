use std::f64;

use ron::{
    de::{ErrorCode, Position},
    Error,
};

#[test]
fn test_float_literal_parsing() {
    assert_eq!(ron::from_str("inf"), Ok(f64::INFINITY));
    assert_eq!(ron::from_str("+inf"), Ok(f64::INFINITY));
    assert_eq!(ron::from_str("-inf"), Ok(f64::NEG_INFINITY));

    assert!(ron::from_str::<f64>("NaN").unwrap().is_nan());
    assert!(ron::from_str::<f64>("+NaN").unwrap().is_nan());
    assert!(ron::from_str::<f64>("-NaN").unwrap().is_nan());

    assert_eq!(ron::from_str("1"), Ok(1.0_f64));
    assert_eq!(ron::from_str("+1"), Ok(1.0_f64));
    assert_eq!(ron::from_str("-1"), Ok(-1.0_f64));
    assert_eq!(ron::from_str("1e3"), Ok(1000.0_f64));
    assert_eq!(ron::from_str("1e+1"), Ok(10.0_f64));
    assert_eq!(ron::from_str("7E-1"), Ok(0.7_f64));

    assert_eq!(ron::from_str("1."), Ok(1.0_f64));
    assert_eq!(ron::from_str("+1.1"), Ok(1.1_f64));
    assert_eq!(ron::from_str("-1.42"), Ok(-1.42_f64));
    assert_eq!(ron::from_str("-1.5e3"), Ok(-1500.0_f64));
    assert_eq!(ron::from_str("1.e+1"), Ok(10.0_f64));
    assert_eq!(ron::from_str("7.4E-1"), Ok(0.74_f64));

    assert_eq!(ron::from_str(".1"), Ok(0.1_f64));
    assert_eq!(ron::from_str("+.1"), Ok(0.1_f64));
    assert_eq!(ron::from_str("-.42"), Ok(-0.42_f64));
    assert_eq!(ron::from_str("-.5e3"), Ok(-500.0_f64));
    assert_eq!(ron::from_str(".3e+1"), Ok(3.0_f64));
    assert_eq!(ron::from_str(".4E-1"), Ok(0.04_f64));

    assert_eq!(
        ron::from_str::<f64>("1_0.1_0"),
        Err(Error {
            code: ErrorCode::FloatUnderscore,
            position: Position { line: 1, col: 2 },
        })
    );
    assert_eq!(
        ron::from_str::<f64>("1_0.10"),
        Err(Error {
            code: ErrorCode::FloatUnderscore,
            position: Position { line: 1, col: 2 },
        })
    );
    assert_eq!(
        ron::from_str::<f64>("10.1_0"),
        Err(Error {
            code: ErrorCode::FloatUnderscore,
            position: Position { line: 1, col: 5 },
        })
    );

    assert_eq!(
        ron::from_str::<f64>("1.0e1.0"),
        Err(Error {
            code: ErrorCode::ExpectedFloat,
            position: Position { line: 1, col: 1 },
        })
    );
}
