/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

extern crate firefox_on_glean;
use firefox_on_glean::{metrics, pings};

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

#[no_mangle]
pub extern "C" fn Rust_TestRideAlongPing() {
    // A similar test exists in `xpcshell/test_Glean.js`.
    // But here we can test that the `test_before_next_submit` callback
    // is correctly called.

    let test_submitted = Arc::new(AtomicBool::new(false));
    let ride_along_submitted = Arc::new(AtomicBool::new(false));

    {
        let test_submitted = Arc::clone(&test_submitted);
        pings::test_ping.test_before_next_submit(move |_reason| {
            test_submitted.store(true, Ordering::Release);
        });
    }

    {
        let ride_along_submitted = Arc::clone(&ride_along_submitted);
        pings::ride_along_ping.test_before_next_submit(move |_reason| {
            ride_along_submitted.store(true, Ordering::Release);
        });
    }

    // Submit only a single ping, the other will ride along.
    pings::test_ping.submit(None);

    expect!(test_submitted.load(Ordering::Acquire));
    expect!(ride_along_submitted.load(Ordering::Acquire));
}
