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
pub extern "C" fn Rust_MeasureInitializeTime() {
    // At this point FOG is already initialized.
    // We still need for it to finish, as it is running in a separate thread.

    let metric = &*fog::metrics::fog::initialization;
    while metric.test_get_value("metrics").is_none() {
        // We _know_ this value is recorded early, so let's just yield
        // and try again quickly.
        std::thread::yield_now();
    }

    let value = metric.test_get_value("metrics").unwrap();
    expect!(value > 0);
}
