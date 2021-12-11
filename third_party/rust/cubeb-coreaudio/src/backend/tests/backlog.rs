// Copyright © 2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.
use super::utils::test_get_default_raw_stream;
use super::*;

// Interface
// ============================================================================
// Remove these after test_ops_stream_register_device_changed_callback works.
#[test]
fn test_stream_register_device_changed_callback() {
    extern "C" fn callback(_: *mut c_void) {}

    test_get_default_raw_stream(|stream| {
        assert!(stream
            .register_device_changed_callback(Some(callback))
            .is_ok());
        assert!(stream.register_device_changed_callback(None).is_ok());
    });
}

#[test]
fn test_stream_register_device_changed_callback_twice() {
    extern "C" fn callback1(_: *mut c_void) {}
    extern "C" fn callback2(_: *mut c_void) {}

    test_get_default_raw_stream(|stream| {
        assert!(stream
            .register_device_changed_callback(Some(callback1))
            .is_ok());
        assert!(stream
            .register_device_changed_callback(Some(callback2))
            .is_err());
    });
}
