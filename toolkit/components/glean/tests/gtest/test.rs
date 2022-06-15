/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

fn nonfatal_fail(msg: String) {
    extern "C" {
        fn GTest_FOG_ExpectFailure(message: *const ::std::os::raw::c_char);
    }
    unsafe {
        let msg = ::std::ffi::CString::new(msg).unwrap();
        GTest_FOG_ExpectFailure(msg.as_ptr());
    }
}

/// This macro checks if the expression evaluates to true,
/// and causes a non-fatal GTest test failure if it doesn't.
macro_rules! expect {
    ($x:expr) => {
        match (&$x) {
            true => {}
            false => nonfatal_fail(format!(
                "check failed: (`{}`) at {}:{}",
                stringify!($x),
                file!(),
                line!()
            )),
        }
    };
}

#[no_mangle]
pub extern "C" fn Rust_TestRustInGTest() {
    // Just a smoke test, we show here how tests might work that both
    // a) Are in Rust, and
    // b) Require Gecko
    // This demonstration doesn't actually require Gecko. But we pretend it
    // does so we remember how to do this rust-in-gtest pattern.
    fog::metrics::test_only::bad_code.add(12);
    expect!(fog::metrics::test_only::bad_code.test_get_value(None) == Some(12));
}
