/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

extern crate firefox_on_glean;
use firefox_on_glean::metrics;

extern crate nsstring;
use nsstring::nsString;

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
    metrics::test_only::bad_code.add(12);
    expect!(metrics::test_only::bad_code.test_get_value(None) == Some(12));
}

#[no_mangle]
pub extern "C" fn Rust_TestJogfile() {
    // Ensure that the generated jogfile in t/c/g/tests/pytest
    // (which is installed nearby using TEST_HARNESS_FILES)
    // can be consumed by JOG's inner workings
    //
    // If it can't, that's perhaps a sign that the inner workings need to be updated.
    expect!(jog::jog_load_jogfile(&nsString::from("jogfile_output")));
}
