use lucet_runtime_internals::val::UntypedRetVal;

#[test]
fn untyped_ret_val_from_f32() {
    assert_eq!(1.2, f32::from(UntypedRetVal::from(1.2f32)));
}

#[test]
fn untyped_ret_val_from_f64() {
    assert_eq!(1.2, f64::from(UntypedRetVal::from(1.2f64)));
}

#[test]
fn untyped_ret_val_from_u8() {
    assert_eq!(5, u8::from(UntypedRetVal::from(5u8)));
}

#[test]
fn untyped_ret_val_from_u16() {
    assert_eq!(5, u16::from(UntypedRetVal::from(5u16)));
}

#[test]
fn untyped_ret_val_from_u32() {
    assert_eq!(5, u32::from(UntypedRetVal::from(5u32)));
}

#[test]
fn untyped_ret_val_from_u64() {
    assert_eq!(5, u64::from(UntypedRetVal::from(5u64)));
}

#[test]
fn untyped_ret_val_from_i8() {
    assert_eq!(5, i8::from(UntypedRetVal::from(5i8)));
}

#[test]
fn untyped_ret_val_from_i16() {
    assert_eq!(5, i16::from(UntypedRetVal::from(5i16)));
}

#[test]
fn untyped_ret_val_from_i32() {
    assert_eq!(5, i32::from(UntypedRetVal::from(5i32)));
}

#[test]
fn untyped_ret_val_from_i64() {
    assert_eq!(5, i64::from(UntypedRetVal::from(5i64)));
}

#[test]
fn untyped_ret_val_from_bool_true() {
    assert_eq!(true, bool::from(UntypedRetVal::from(true)));
}

#[test]
fn untyped_ret_val_from_bool_false() {
    assert_eq!(false, bool::from(UntypedRetVal::from(false)));
}
